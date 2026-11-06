/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    USBFSv1/hal_usb_lld.c
 * @brief   CH32H417 USB device low level driver source.
 *
 * @details Transfer engine on the INT_FG/INT_ST protocol (RM 26.2.1):
 *          TRANSFER events carry the token type and endpoint number,
 *          data lives in per-group buffers pointed at by UEPn_DMA.
 *          Endpoint zero manages DATA toggles manually (AUTO_TOG is
 *          reserved on EP0), other groups use AUTO_TOG.
 *
 * @note    A setup packet is delivered by the controller at the beginning
 *          of the EP0 DMA buffer, the very same place where the data
 *          stage of the control transfer is assembled: the setup packet
 *          has to be copied out before the data stage overwrites it.
 *
 * @note    The interrupt flags are cleared at the end of the handler.
 *          RB_UC_INT_BUSY keeps the controller answering NAK while
 *          RB_UIF_TRANSFER is still set, this way the endpoint buffers
 *          cannot be overwritten while they are being processed.
 *
 * @addtogroup USB
 * @{
 */

#include <string.h>

#include "hal.h"

#if (HAL_USE_USB == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* R8_USB_CTRL bits (RM 26.2.1.1). */
#define UC_HOST_MODE        (1U << 7)
#define UC_LOW_SPEED        (1U << 6)
#define UC_SYS_CTRL_MSK     (3U << 4)
#define UC_INT_BUSY         (1U << 3)
#define UC_RST_SIE          (1U << 2)
#define UC_CLR_ALL          (1U << 1)
#define UC_DMA_EN           (1U << 0)

/* R8_UDEV_CTRL bits (RM 26.2.2.1). */
#define UD_PD_DIS           (1U << 7)
#define UD_LOW_SPEED        (1U << 2)
#define UD_PORT_EN          (1U << 0)

/* R8_USB_INT_EN / INT_FG device bits. */
#define UIF_BUS_RST         (1U << 0)
#define UIF_TRANSFER        (1U << 1)
#define UIF_SUSPEND         (1U << 2)
#define UIF_FIFO_OV         (1U << 4)

/* R8_USB_MIS_ST fields (RM 26.2.1.4). */
#define UMS_BUS_RST         (1U << 3)
#define UMS_SUSPEND         (1U << 2)

/* R8_USB_INT_ST fields (RM 26.2.1.6). */
#define UIS_TOKEN_MASK      (3U << 4)
#define UIS_TOKEN_OUT       0U
#define UIS_TOKEN_IN        2U
#define UIS_TOKEN_SETUP     3U
#define UIS_ENDP_MASK       0x0FU
#define UIS_TOG_OK          (1U << 6)

/* UEPn_TX_CTRL / UEPn_RX_CTRL fields (RM 26.2.2.9/26.2.2.10). */
#define UEP_RES_MASK        0x03U
#define UEP_RES_ACK         0x00U
#define UEP_RES_NAK         0x02U
#define UEP_RES_STALL       0x03U
#define UEP_TOG             (1U << 2)
#define UEP_AUTO_TOG        (1U << 3)

/* UEPn_MOD register bits (RM 26.2.2.4/26.2.2.5). */
#define UEP4_1_RX_EN        (1U << 7)
#define UEP4_1_TX_EN        (1U << 6)
#define UEP4_1_EP4_RX_EN    (1U << 3)
#define UEP4_1_EP4_TX_EN    (1U << 2)
#define UEP2_3_EP3_RX_EN    (1U << 7)
#define UEP2_3_EP3_TX_EN    (1U << 6)
#define UEP2_3_EP2_RX_EN    (1U << 3)
#define UEP2_3_EP2_TX_EN    (1U << 2)
#define UEP5_6_EP6_RX_EN    (1U << 7)
#define UEP5_6_EP6_TX_EN    (1U << 6)
#define UEP5_6_EP5_RX_EN    (1U << 3)
#define UEP5_6_EP5_TX_EN    (1U << 2)
#define UEP7_EP7_RX_EN      (1U << 3)
#define UEP7_EP7_TX_EN      (1U << 2)

/* Number of endpoint groups, the group zero is the control endpoint. */
#define USBFS_GROUPS        8U

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

/** @brief USBFS device driver identifier.*/
#if (CH32H417_USB_USE_USBFS == TRUE) && !defined(__DOXYGEN__)
USBDriver USBD1;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   EP0 state.
 * @note    Union because IN and OUT endpoints are never used at the
 *          same time for EP0.
 */
static union {
  USBInEndpointState in;
  USBOutEndpointState out;
} ep0_state;

/**
 * @brief   USB data buffers.
 * @note    The USBFS DMA engine can only reach the shared SRAM,
 *          not the core-own DTCM (RM ch26 RB_DMA_EN: "可访问 SRAM 中
 *          数据") and its address registers only retain the 2MB RAM
 *          window offset bits, so buffers must live in .ram2
 *          (SRAM, 0x20120000+) or the DMA writes go to an unmapped
 *          address and raise a bus error NMI at the CPU.
 * @note    The EP0 buffer holds the setup packet at its beginning and
 *          the data stage of the control transfer right after, it must
 *          be large enough for the largest packet plus the two bytes
 *          the controller appends (RM 26.2.2.6).
 */
static uint8_t epbuf0[72] __attribute__((section(".ram2"), aligned(4)));

/**
 * @brief   Per-group buffers (single 64+64 layout plus slack).
 */
static uint8_t epbuf1[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf2[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf3[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf4[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf5[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf6[136] __attribute__((section(".ram2"), aligned(4)));
static uint8_t epbuf7[136] __attribute__((section(".ram2"), aligned(4)));

/**
 * @brief   EP0 initialization structure.
 */
static const USBEndpointConfig ep0config = {
  .ep_mode          = USB_EP_MODE_TYPE_CTRL,
  .setup_cb         = _usb_ep0setup,
  .in_cb            = _usb_ep0in,
  .out_cb           = _usb_ep0out,
  .in_maxsize       = 0x40U,
  .out_maxsize      = 0x40U,
  .in_state         = &ep0_state.in,
  .out_state        = &ep0_state.out,
  .ep_buffers       = 1U,
  .setup_buf        = NULL
};

/* Per-group buffer base addresses (index = hardware group 0..7). */
static uint8_t *const group_buf[USBFS_GROUPS] = {
  epbuf0, epbuf1, epbuf2, epbuf3, epbuf4, epbuf5, epbuf6, epbuf7
};

/* Which directions are enabled in each group (layout bookkeeping). */
static bool group_rx[USBFS_GROUPS];
static bool group_tx[USBFS_GROUPS];

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Endpoint address to endpoint number map.
 * @note    The transfer interrupt reports the endpoint address, the field
 *          is four bits wide (RM 26.2.1.6, MASK_UIS_ENDP), and the
 *          addresses 8..15 are aliases of the addresses 0..7 that share
 *          their buffer (RM 26.2.2.2: "端点 1 配置选项映射端点 9，端点 4
 *          配置选项映射端点 8 和 12", RM 26.2.2.3: "端点 2 配置选项映射
 *          端点 10，端点 3 配置选项映射端点 11", RM 26.2.2.4: "端点 5 配置
 *          选项映射端点 13，端点 6 配置选项映射端点 14", RM 26.2.2.5: "端点
 *          7 配置选项映射端点 15"). The table folds an address onto the
 *          endpoint it is mapped to.
 */
static const uint8_t ep_addr_map[16] = {
  0, 1, 2, 3, 4, 5, 6, 7, 4, 1, 2, 3, 4, 5, 6, 7
};

/**
 * @brief   Returns the endpoint number of an endpoint address.
 * @note    The folded value is at the same time the hardware group of
 *          the endpoint (the groups are numbered 0..7) and the ChibiOS
 *          endpoint number, the addresses 0..7 are mapped onto
 *          themselves.
 */
static unsigned ep_group(usbep_t ep) {

  return (unsigned)ep_addr_map[ep & 0x0FU];
}

/**
 * @brief   Pointer to the TX length register of a group.
 */
static inline volatile uint8_t *uep_tx_len(unsigned g) {

  return &USBFSD->UEP0_TX_LEN + (g * 4);
}

/**
 * @brief   Sets the transmit length of a group.
 * @note    The length register is eight bits wide for every group but
 *          the third one, which is sixteen bits wide (RM 26.2.2.7 and
 *          RM 26.2.2.8).
 */
static void ep_tx_len_set(unsigned g, size_t n) {

  if (g == 3U) {
    USBFSD->UEP3_TX_LEN = (uint16_t)n;
  }
  else {
    *uep_tx_len(g) = (uint8_t)n;
  }
}

/**
 * @brief   Pointer to the TX control register of a group.
 */
static inline volatile uint8_t *uep_tx_ctrl(unsigned g) {

  return &USBFSD->UEP0_TX_CTRL + (g * 4);
}

/**
 * @brief   Pointer to the RX control register of a group.
 */
static inline volatile uint8_t *uep_rx_ctrl(unsigned g) {

  return &USBFSD->UEP0_RX_CTRL + (g * 4);
}

/**
 * @brief   Pointer to the buffer start address register of a group.
 */
static inline volatile uint32_t *uep_dma(unsigned g) {

  return &USBFSD->UEP0_DMA + g;
}

/**
 * @brief   Receive area of a group (always at the buffer base).
 */
static inline uint8_t *group_rx_ptr(unsigned g) {

  return group_buf[g];
}

/**
 * @brief   Transmit area of a group: at +64 when the group has both
 *          directions enabled, at the base otherwise (RM table 26-4),
 *          EP0 shares the single buffer.
 */
static inline uint8_t *group_tx_ptr(unsigned g) {

  if ((g == 0U) || (group_rx[g] && group_tx[g])) {
    return group_buf[g] + ((g == 0U) ? 0U : 64U);
  }
  return group_buf[g];
}

/**
 * @brief   Enables/disables the RX/TX bits of a group MOD register.
 */
static void ep_mod_set(unsigned g, bool rx, bool tx) {
  uint8_t v = 0U;

  switch (g) {
  case 1:
    v = (rx ? UEP4_1_RX_EN : 0U) | (tx ? UEP4_1_TX_EN : 0U);
    USBFSD->UEP4_1_MOD = (uint8_t)((USBFSD->UEP4_1_MOD &
                                     ~(UEP4_1_RX_EN | UEP4_1_TX_EN)) | v);
    break;
  case 2:
    v = (rx ? UEP2_3_EP2_RX_EN : 0U) | (tx ? UEP2_3_EP2_TX_EN : 0U);
    USBFSD->UEP2_3_MOD = (uint8_t)((USBFSD->UEP2_3_MOD &
                                     ~(UEP2_3_EP2_RX_EN | UEP2_3_EP2_TX_EN)) | v);
    break;
  case 3:
    v = (rx ? UEP2_3_EP3_RX_EN : 0U) | (tx ? UEP2_3_EP3_TX_EN : 0U);
    USBFSD->UEP2_3_MOD = (uint8_t)((USBFSD->UEP2_3_MOD &
                                     ~(UEP2_3_EP3_RX_EN | UEP2_3_EP3_TX_EN)) | v);
    break;
  case 4:
    v = (rx ? UEP4_1_EP4_RX_EN : 0U) | (tx ? UEP4_1_EP4_TX_EN : 0U);
    USBFSD->UEP4_1_MOD = (uint8_t)((USBFSD->UEP4_1_MOD &
                                     ~(UEP4_1_EP4_RX_EN | UEP4_1_EP4_TX_EN)) | v);
    break;
  case 5:
    v = (rx ? UEP5_6_EP5_RX_EN : 0U) | (tx ? UEP5_6_EP5_TX_EN : 0U);
    USBFSD->UEP5_6_MOD = (uint8_t)((USBFSD->UEP5_6_MOD &
                                     ~(UEP5_6_EP5_RX_EN | UEP5_6_EP5_TX_EN)) | v);
    break;
  case 6:
    v = (rx ? UEP5_6_EP6_RX_EN : 0U) | (tx ? UEP5_6_EP6_TX_EN : 0U);
    USBFSD->UEP5_6_MOD = (uint8_t)((USBFSD->UEP5_6_MOD &
                                     ~(UEP5_6_EP6_RX_EN | UEP5_6_EP6_TX_EN)) | v);
    break;
  case 7:
    v = (rx ? UEP7_EP7_RX_EN : 0U) | (tx ? UEP7_EP7_TX_EN : 0U);
    USBFSD->UEP7_MOD = (uint8_t)((USBFSD->UEP7_MOD &
                                   ~(UEP7_EP7_RX_EN | UEP7_EP7_TX_EN)) | v);
    break;
  default:
    break;
  }
}

/**
 * @brief   Returns whether the IN/OUT side of an endpoint is enabled.
 */
static bool ep_dir_enabled(usbep_t ep, bool tx) {
  unsigned g = ep_group(ep);
  uint8_t mod;

  if (g == 0U) {
    return true;  /* EP0 is always bidirectional. */
  }

  switch (g) {
  case 1:
    mod = USBFSD->UEP4_1_MOD;
    return tx ? ((mod & UEP4_1_TX_EN) != 0U) : ((mod & UEP4_1_RX_EN) != 0U);
  case 2:
    mod = USBFSD->UEP2_3_MOD;
    return tx ? ((mod & UEP2_3_EP2_TX_EN) != 0U) :
                ((mod & UEP2_3_EP2_RX_EN) != 0U);
  case 3:
    mod = USBFSD->UEP2_3_MOD;
    return tx ? ((mod & UEP2_3_EP3_TX_EN) != 0U) :
                ((mod & UEP2_3_EP3_RX_EN) != 0U);
  case 4:
    mod = USBFSD->UEP4_1_MOD;
    return tx ? ((mod & UEP4_1_EP4_TX_EN) != 0U) :
                ((mod & UEP4_1_EP4_RX_EN) != 0U);
  case 5:
    mod = USBFSD->UEP5_6_MOD;
    return tx ? ((mod & UEP5_6_EP5_TX_EN) != 0U) :
                ((mod & UEP5_6_EP5_RX_EN) != 0U);
  case 6:
    mod = USBFSD->UEP5_6_MOD;
    return tx ? ((mod & UEP5_6_EP6_TX_EN) != 0U) :
                ((mod & UEP5_6_EP6_RX_EN) != 0U);
  default:
    mod = USBFSD->UEP7_MOD;
    return tx ? ((mod & UEP7_EP7_TX_EN) != 0U) :
                ((mod & UEP7_EP7_RX_EN) != 0U);
  }
}

/**
 * @brief   TRANSFER event processing.
 * @note    The status and the length are the ones of the event that
 *          woke the handler, the controller is paused while the
 *          transfer flag is still set (RB_UC_INT_BUSY) so they cannot
 *          change under the feet of this function.
 */
static void usb_serve_transfer(USBDriver *usbp, uint32_t ist, uint32_t rxlen) {
  unsigned addr = ist & UIS_ENDP_MASK;
  unsigned token = (ist >> 4) & 3U;
  const USBEndpointConfig *epcp;
  unsigned ep;
  unsigned g;

  /* The interrupt reports the endpoint address and not the group, the
     alias addresses 8..15 are folded onto the endpoint they share their
     buffer with, so that the configuration lookup and the callbacks use
     the endpoint number of the application (RM 26.2.1.6 and RM 26.2.2.2
     to RM 26.2.2.5). */
  ep = ep_group((usbep_t)addr);

  epcp = usbp->epc[ep];
  if (epcp == NULL) {
    return;
  }

  g = ep;

  if (token == UIS_TOKEN_SETUP) {
    /* The controller delivered the setup packet at the beginning of the
       EP0 buffer, it is copied out by the setup callback before the
       data stage overwrites that place. The data toggles restart from
       DATA1 on both directions after a setup packet. */
    *uep_tx_ctrl(0) = (uint8_t)(UEP_RES_NAK | UEP_TOG);
    *uep_rx_ctrl(0) = (uint8_t)(UEP_RES_NAK | UEP_TOG);

    _usb_isr_invoke_setup_cb(usbp, (usbep_t)ep);
    return;
  }

  if (token == UIS_TOKEN_IN) {
    USBInEndpointState *isp = epcp->in_state;
    size_t n;

    if (isp == NULL) {
      return;
    }

    isp->txcnt += isp->txlast;

    if (g == 0U) {
      /* EP0 manual toggling: every packet that leaves the endpoint
         flips the data toggle, the next one is DATA0 and the one after
         is DATA1 again. */
      *uep_tx_ctrl(g) = (uint8_t)(*uep_tx_ctrl(g) ^ UEP_TOG);
    }

    n = isp->txsize - isp->txcnt;
    if (n > 0U) {
      /* More data to push, load the next packet. */
      if (n > (size_t)epcp->in_maxsize) {
        n = (size_t)epcp->in_maxsize;
      }
      isp->txbuf += isp->txlast;
      isp->txlast = n;
      memcpy(group_tx_ptr(g), isp->txbuf, n);
      ep_tx_len_set(g, n);
      /* Arm the response (RES=00, data ready). */
      *uep_tx_ctrl(g) = (uint8_t)((*uep_tx_ctrl(g) & ~UEP_RES_MASK) |
                                   UEP_RES_ACK);
    }
    else {
      /* Transfer complete: park the endpoint until the next start. */
      *uep_tx_ctrl(g) = (uint8_t)((*uep_tx_ctrl(g) & (UEP_AUTO_TOG | UEP_TOG)) |
                                  UEP_RES_NAK);
      _usb_isr_invoke_in_cb(usbp, (usbep_t)ep);
    }
    return;
  }

  /* OUT data packet. */
  {
    USBOutEndpointState *osp = epcp->out_state;
    size_t n;

    if (osp == NULL) {
      return;
    }

    /* A packet whose data toggle does not match the expected one is
       not accepted (RM 26.2.1.6, RB_UIS_TOG_OK). */
    if ((ist & UIS_TOG_OK) == 0U) {
      return;
    }

    /* The length comes from the hardware, it must not be trusted
       blindly: a packet cannot be longer than the endpoint maximum
       packet size and it cannot exceed the space left in the reception
       buffer. Copying more would overflow the destination buffer. */
    n = (size_t)(rxlen & 0x3FFU);
    if (n > (size_t)epcp->out_maxsize) {
      n = (size_t)epcp->out_maxsize;
    }
    if (n > osp->rxsize) {
      n = osp->rxsize;
    }

    if (n > 0U) {
      memcpy(osp->rxbuf, group_rx_ptr(g), n);
      osp->rxbuf += n;
      osp->rxcnt  += n;
      osp->rxsize -= n;
    }
    osp->rxpkts -= 1U;

    if ((n < (size_t)epcp->out_maxsize) || (osp->rxpkts == 0U)) {
      /* Transfer complete. */
      *uep_rx_ctrl(g) = (uint8_t)((*uep_rx_ctrl(g) & (UEP_AUTO_TOG | UEP_TOG)) |
                                  UEP_RES_NAK);
      _usb_isr_invoke_out_cb(usbp, (usbep_t)ep);
    }
    else {
      /* More packets expected, re-arm with the next DATA toggle. */
      if (g == 0U) {
        *uep_rx_ctrl(g) = (uint8_t)((*uep_rx_ctrl(g) ^ UEP_TOG) &
                                    ~UEP_RES_MASK);
      }
      else {
        /* The toggle is handled by the controller (AUTO_TOG). */
        *uep_rx_ctrl(g) = (uint8_t)(*uep_rx_ctrl(g) & ~UEP_RES_MASK);
      }
    }
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_USB_USE_USBFS == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USBFS global interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USBFS_IRQHandler) {
  uint32_t fg = USBFSD->INT_FG;
  uint32_t ist = USBFSD->INT_ST;
  uint32_t rxlen = USBFSD->RX_LEN;

  OSAL_IRQ_PROLOGUE();

  /* No kernel lock here: the HAL event routines (_usb_reset, _usb_suspend,
     _usb_wakeup) and the event callbacks lock the kernel incrementally by
     themselves, the same contract as the STM32 reference driver. */

  /* Bus reset: the device side state is rebuilt from scratch and the
     transfers in flight are dropped with it. */
  if ((fg & UIF_BUS_RST) != 0U) {
    _usb_reset(&USBD1);
  }
  else if ((fg & UIF_TRANSFER) != 0U) {
    /* Transfer completion (flag 1): endpoint dispatching. */
    usb_serve_transfer(&USBD1, ist, rxlen);
  }

  /* Suspend or wakeup (flag 2, direction taken from MIS_ST). A bus event
     raised while the bus is in reset carries no information, the reset
     event rebuilds the device state anyway (RM 26.2.1.4, RB_UMS_BUS_RST),
     reporting it would disturb an enumeration that has not reached the
     configuration stage yet. */
  if ((fg & UIF_SUSPEND) != 0U) {
    if ((USBFSD->MIS_ST & UMS_BUS_RST) == 0U) {
      if ((USBFSD->MIS_ST & UMS_SUSPEND) != 0U) {
        _usb_suspend(&USBD1);
      }
      else {
        _usb_wakeup(&USBD1);
      }
    }
  }

  /* The flags are cleared at the end of the handler: while
     RB_UIF_TRANSFER is still set the controller answers NAK
     (RB_UC_INT_BUSY), so the endpoint buffers cannot be overwritten
     while they are being processed. */
  USBFSD->INT_FG = (uint8_t)(fg & (UIF_BUS_RST | UIF_TRANSFER |
                                   UIF_SUSPEND | UIF_FIFO_OV));

  OSAL_IRQ_EPILOGUE();
}

#endif /* (CH32H417_USB_USE_USBFS == TRUE) */

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level USB driver initialization.
 *
 * @notapi
 */
void usb_lld_init(void) {

#if (CH32H417_USB_USE_USBFS == TRUE)
  usbObjectInit(&USBD1);
#endif
}

/**
 * @brief   Configures and activates the USB peripheral.
 *
 * @notapi
 */
void usb_lld_start(USBDriver *usbp) {
  volatile uint8_t dummy;
  unsigned i;

  if (usbp->state == USB_STOP) {
    /* 48MHz clock chain, same sequence as the vendor USBFS_RCC_Init():
       the USBHS PLL (480MHz) is programmed (source, reference frequency
       and input divider) and only then enabled, the USBFS 48MHz entry
       takes it divided by ten. Enabling the PLL without programming it
       leaves the SIE on a wrong clock: the protocol engine then samples
       the bus badly, the received length register returns garbage and
       the DMA writes to wrong addresses. */
#if CH32H417_USB_SETUP_USBHSPLL
    if ((RCC->PLLCFGR & RCC_SYSPLL_SEL) != RCC_SYSPLL_USBHS) {
      RCC_USBHS_PLLCmd(DISABLE);
      RCC_USBHSPLLCLKConfig((RCC->CTLR & RCC_HSERDY) != 0U ?
                            RCC_USBHSPLLSource_HSE : RCC_USBHSPLLSource_HSI);
      RCC_USBHSPLLReferConfig(CH32H417_USBHS_PLL_REFER);
      RCC_USBHSPLLClockSourceDivConfig(RCC_USBHSPLL_IN_Div1);
      RCC_USBHS_PLLCmd(ENABLE);
      while ((RCC->CTLR & RCC_USBHS_PLLRDY) == 0U) {
      }
    }
#endif
    RCC_USBFSCLKConfig(CH32H417_USBFS_CLK_SRC);
    RCC_USBFS48ClockSourceDivConfig(CH32H417_USBFS_CLK_DIV);
    CH32H417_DSB();

    /* Peripheral clock (HBPCENR.OTGFSEN/RCC_USBOTGEN). */
    RCC->HBPCENR |= RCC_USBOTGEN;
    /* Order the gate commit before the first USBFSD register access. */
    CH32H417_DSB();

    /* Protocol engine reset, the FIFO and the interrupt flags are
       cleared together with it (RM 26.2.1.1). */
    USBFSD->BASE_CTRL = (uint8_t)(UC_RST_SIE | UC_CLR_ALL);
    CH32H417_DSB();
    for (i = 0; i < 1000U; i++) {
      dummy = USBFSD->BASE_CTRL;
    }
    (void)dummy;
    USBFSD->BASE_CTRL = 0U;

    /* Device mode: the packet buffers are DMA driven and the busy bit
       makes the SIE answer NAK while a transfer event is pending, the
       vendor device examples always set both. Without DMA enabled the
       endpoints never complete a transfer. */
    USBFSD->BASE_CTRL = (uint8_t)(UC_DMA_EN | UC_INT_BUSY);

    /* Device physical port: full/low speed, internal pull-down
       disabled (device uses the SYS_CTRL pull-up), port enabled,
       device function off until usbConnectBus(). */
    USBFSD->UDEV_CTRL = (uint8_t)(UD_PORT_EN | UD_PD_DIS |
#if CH32H417_USB_LOW_SPEED
                                   UD_LOW_SPEED |
#endif
                                   0U);

    /* Bus reset, transfer and suspend sources. */
    USBFSD->INT_EN = UIF_BUS_RST | UIF_TRANSFER | UIF_SUSPEND;

    NVIC_EnableIRQ(USBFS_IRQn);

    /* Device state initialization. */
    usb_lld_reset(usbp);
  }
}

/**
 * @brief   Deactivates the USB peripheral.
 *
 * @notapi
 */
void usb_lld_stop(USBDriver *usbp) {

  if (usbp->state != USB_STOP) {
    NVIC_DisableIRQ(USBFS_IRQn);

    RCC->HBPCENR &= ~RCC_USBOTGEN;
#if CH32H417_USB_SETUP_USBHSPLL
    RCC_USBHS_PLLCmd(DISABLE);
#endif
  }
}

/**
 * @brief   USB low level reset routine.
 * @details Rebuilds the device side state after a bus reset. The
 *          protocol engine handles the bus reset on its own, only the
 *          device address and the endpoints have to be restored, the
 *          engine is not reset again here.
 *
 * @notapi
 */
void usb_lld_reset(USBDriver *usbp) {
  unsigned i;

  /* Device address zero. */
  USBFSD->DEV_ADDR = 0U;

  /* Drop the pending events, the transfers in flight are gone. */
  USBFSD->INT_FG = 0xFFU;
  /* The clear must be committed before the endpoints are re-armed. */
  CH32H417_DSB();

  /* Park all the endpoints but the zero, they are enabled again by the
     configuration callback. */
  for (i = 1U; i < USBFS_GROUPS; i++) {
    ep_mod_set(i, false, false);
    group_rx[i] = false;
    group_tx[i] = false;
    *uep_tx_ctrl(i) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
    *uep_rx_ctrl(i) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
  }

  /* EP0 initialization.*/
  usbp->epc[0] = &ep0config;
  usb_lld_init_endpoint(usbp, 0U);
}

/**
 * @brief   Sets the USB address.
 *
 * @notapi
 */
void usb_lld_set_address(USBDriver *usbp) {

  USBFSD->DEV_ADDR = usbp->address & 0x7FU;
}

/**
 * @brief   Enables an endpoint.
 *
 * @notapi
 */
void usb_lld_init_endpoint(USBDriver *usbp, usbep_t ep) {
  const USBEndpointConfig *epcp;
  unsigned g;
  bool rx;
  bool tx;

  if ((ep > (unsigned)USB_MAX_ENDPOINTS) || (usbp->epc[ep] == NULL)) {
    return;
  }
  epcp = usbp->epc[ep];
  g = ep_group(ep);
  rx = epcp->out_state != NULL;
  tx = epcp->in_state != NULL;

  /* EP0: single shared 64 byte buffer, no MOD register. */
  if (g == 0U) {
    group_rx[0] = true;
    group_tx[0] = true;
    *uep_dma(0) = (uint32_t)epbuf0;
    *uep_tx_ctrl(0) = UEP_RES_NAK;
    *uep_rx_ctrl(0) = UEP_RES_NAK;
    return;
  }

  group_rx[g] = rx;
  group_tx[g] = tx;

  ep_mod_set(g, rx, tx);
  *uep_dma(g) = (uint32_t)group_buf[g];
  *uep_tx_ctrl(g) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
  *uep_rx_ctrl(g) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
}

/**
 * @brief   Disables all the active endpoints except the endpoint zero.
 *
 * @notapi
 */
void usb_lld_disable_endpoints(USBDriver *usbp) {
  unsigned i;

  (void)usbp;

  for (i = 1U; i < USBFS_GROUPS; i++) {
    ep_mod_set(i, false, false);
    group_rx[i] = false;
    group_tx[i] = false;
    *uep_tx_ctrl(i) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
    *uep_rx_ctrl(i) = (uint8_t)(UEP_RES_NAK | UEP_AUTO_TOG);
  }
}

/**
 * @brief   Returns the status of an OUT endpoint.
 *
 * @notapi
 */
usbepstatus_t usb_lld_get_status_out(USBDriver *usbp, usbep_t ep) {

  (void)usbp;

  if (!ep_dir_enabled(ep, false)) {
    return EP_STATUS_DISABLED;
  }
  if (((*uep_rx_ctrl(ep_group(ep))) & UEP_RES_MASK) == UEP_RES_STALL) {
    return EP_STATUS_STALLED;
  }
  return EP_STATUS_ACTIVE;
}

/**
 * @brief   Returns the status of an IN endpoint.
 *
 * @notapi
 */
usbepstatus_t usb_lld_get_status_in(USBDriver *usbp, usbep_t ep) {

  (void)usbp;

  if (!ep_dir_enabled(ep, true)) {
    return EP_STATUS_DISABLED;
  }
  if (((*uep_tx_ctrl(ep_group(ep))) & UEP_RES_MASK) == UEP_RES_STALL) {
    return EP_STATUS_STALLED;
  }
  return EP_STATUS_ACTIVE;
}

/**
 * @brief   Reads a setup packet from the EP0 buffer.
 * @details The controller delivers the setup packet at the beginning of
 *          the EP0 DMA buffer.
 *
 * @notapi
 */
void usb_lld_read_setup(USBDriver *usbp, usbep_t ep, uint8_t *buf) {

  (void)usbp;
  (void)ep;
  memcpy(buf, group_buf[0], 8U);
}

/**
 * @brief   Starts a receive operation on an OUT endpoint.
 *
 * @notapi
 */
void usb_lld_start_out(USBDriver *usbp, usbep_t ep) {
  USBOutEndpointState *osp;
  unsigned g;
  volatile uint8_t *ctrl;

  /* The generic layer looks the endpoint up without any protection (the
     check it does is compiled out in a release build), an endpoint that
     was never registered would be a wild pointer dereference. */
  if ((ep > (unsigned)USB_MAX_ENDPOINTS) || (usbp->epc[ep] == NULL)) {
    return;
  }
  osp = usbp->epc[ep]->out_state;
  if (osp == NULL) {
    return;
  }

  g = ep_group(ep);
  ctrl = uep_rx_ctrl(g);

  /* Transfer initialization.*/
  if (osp->rxsize == 0U) {       /* Special case for zero sized packets.*/
    osp->rxpkts = 1U;
  }
  else {
    osp->rxpkts = (uint16_t)((osp->rxsize + usbp->epc[ep]->out_maxsize - 1) /
                             usbp->epc[ep]->out_maxsize);
  }

  /* RES=ACK (00), keep TOG/AUTO: ready to receive. */
  *ctrl = (uint8_t)(*ctrl & (UEP_AUTO_TOG | UEP_TOG));
}

/**
 * @brief   Starts a transmit operation on an IN endpoint.
 *
 * @notapi
 */
void usb_lld_start_in(USBDriver *usbp, usbep_t ep) {
  size_t n;
  USBInEndpointState *isp;
  unsigned g;
  volatile uint8_t *ctrl;

  /* The generic layer looks the endpoint up without any protection (the
     check it does is compiled out in a release build), an endpoint that
     was never registered would be a wild pointer dereference. */
  if ((ep > (unsigned)USB_MAX_ENDPOINTS) || (usbp->epc[ep] == NULL)) {
    return;
  }
  isp = usbp->epc[ep]->in_state;
  if (isp == NULL) {
    return;
  }

  g = ep_group(ep);
  ctrl = uep_tx_ctrl(g);

  /* Transfer initialization.*/
  n = isp->txsize;
  if (n > (size_t)usbp->epc[ep]->in_maxsize) {
    n = (size_t)usbp->epc[ep]->in_maxsize;
  }

  isp->txlast = n;
  if (n > 0U) {
    memcpy(group_tx_ptr(g), isp->txbuf, n);
  }
  ep_tx_len_set(g, n);

  /* RES=00 (data ready, expect ACK), keep TOG/AUTO. */
  *ctrl = (uint8_t)(*ctrl & (UEP_AUTO_TOG | UEP_TOG));
}

/**
 * @brief   Brings an OUT endpoint in the stalled state.
 *
 * @notapi
 */
void usb_lld_stall_out(USBDriver *usbp, usbep_t ep) {
  volatile uint8_t *ctrl = uep_rx_ctrl(ep_group(ep));

  (void)usbp;
  *ctrl = (uint8_t)((*ctrl & ~UEP_RES_MASK) | UEP_RES_STALL);
}

/**
 * @brief   Brings an IN endpoint in the stalled state.
 *
 * @notapi
 */
void usb_lld_stall_in(USBDriver *usbp, usbep_t ep) {
  volatile uint8_t *ctrl = uep_tx_ctrl(ep_group(ep));

  (void)usbp;
  *ctrl = (uint8_t)((*ctrl & ~UEP_RES_MASK) | UEP_RES_STALL);
}

/**
 * @brief   Brings an OUT endpoint in the active (NAK) state.
 *
 * @notapi
 */
void usb_lld_clear_out(USBDriver *usbp, usbep_t ep) {
  volatile uint8_t *ctrl = uep_rx_ctrl(ep_group(ep));

  (void)usbp;
  *ctrl = (uint8_t)((*ctrl & ~UEP_RES_MASK) | UEP_RES_NAK);
}

/**
 * @brief   Brings an IN endpoint in the active (NAK) state.
 *
 * @notapi
 */
void usb_lld_clear_in(USBDriver *usbp, usbep_t ep) {
  volatile uint8_t *ctrl = uep_tx_ctrl(ep_group(ep));

  (void)usbp;
  *ctrl = (uint8_t)((*ctrl & ~UEP_RES_MASK) | UEP_RES_NAK);
}

#endif /* HAL_USE_USB == TRUE */

/** @} */
