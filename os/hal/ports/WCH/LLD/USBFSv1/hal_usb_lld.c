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
 *          Each non-zero endpoint has a single UEPn_DMA address and its
 *          DMA path is one-way, so a group serves either IN or OUT, never
 *          both.  Both directions therefore use the buffer base and no
 *          +64 TX half exists.  The application must allocate distinct
 *          endpoint numbers for the CDC bulk IN, bulk OUT and interrupt
 *          IN pipes.
 *
 *          There is no device SOF interrupt.  USBConfig::sof_cb is driven
 *          from a 1ms virtual timer (CH32H417_USB_SOF_EMULATION) so the
 *          SerialUSB output queue still gets its periodic flush.
 *
 * @note    A setup packet is delivered by the controller at the beginning
 *          of the EP0 DMA buffer, the very same place where the data
 *          stage of the control transfer is assembled: the setup packet
 *          has to be copied out before the data stage overwrites it.
 *
 * @note    INT_FG is cleared at the END of the handler with the flags
 *          present on entry, exactly as the vendor USBFS_RCC_Init()/
 *          USBFS_IRQHandler examples do. RB_UC_INT_BUSY makes the SIE
 *          answer NAK to every IN/OUT token while an INT_FG flag is
 *          pending; that is the intended protection for the endpoint
 *          DMA buffers while the handler prepares the next data stage.
 *          A flag raised during the handler is deliberately left set so
 *          the interrupt re-enters and services it.
 *
 * @note    On the dual core part an IRQ id > 31 is claimed by the first
 *          core that enables it (PFIC.IALLOCR[] latches the owner), so
 *          this driver must not write IALLOCR up front: NVIC_EnableIRQ()
 *          from the V5F image is what claims USBFS_IRQn for V5F.
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
#define UIS_IS_NAK          (1U << 7)
#define UIS_TOKEN_MASK      (3U << 4)
#define UIS_TOKEN_OUT       0U
#define UIS_TOKEN_SOF       1U
#define UIS_TOKEN_IN        2U
#define UIS_TOKEN_SETUP     3U
#define UIS_ENDP_MASK       0x0FU
#define UIS_TOG_OK          (1U << 6)

/* INT_FG bits that are real interrupt flags (write one to clear). Bits
   5..7 are read-only live status, they must never be written back.
   Bit 3 is the host SOF flag, unused in device mode. The ISR clears
   (entry snapshot & UIF_CLEARABLE), so a flag raised while the handler
   runs stays pending and gets serviced on re-entry (vendor behaviour). */
#define UIF_HST_SOF         (1U << 3)
#define UIF_CLEARABLE       (UIF_BUS_RST | UIF_TRANSFER |                  \
                             UIF_SUSPEND | UIF_HST_SOF | UIF_FIFO_OV)

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
 * @note    R32_UEPn_DMA keeps a 24 bit offset (mask 0xFFFFFF) that the
 *          controller adds to 0x20000000: the vendor
 *          USBFSD_UEP_BUF(n) macro is exactly
 *          ((uint8_t *)UEPn_DMA(n) + 0x20000000). Writing the absolute
 *          address of a buffer in the 0x20000000..0x20FFFFFF window
 *          therefore works, the 0x20 prefix is discarded by the mask.
 *          The official EVT link script places its endpoint buffers in
 *          plain .bss in DTCM (0x200C0200+), so DTCM is reachable too;
 *          .ram2 (flashram, 0x20120000+) is used here to keep the DMA
 *          traffic away from the core-own DTCM data. A buffer outside
 *          that 16MB window would be truncated and cause a bus error.
 * @note    The EP0 buffer holds the setup packet at its beginning and
 *          the data stage of the control transfer right after, it must
 *          be large enough for the largest packet plus the two bytes
 *          the controller appends (RM 26.2.2.6).
 */
static uint8_t epbuf0[72] __attribute__((section(".ram2"), aligned(4)));

/**
 * @brief   Per-group buffers (64 bytes plus slack).
 * @note    One-way DMA means a group never needs a separate +64 TX half.
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

/* Which directions are enabled in each group. Layout bookkeeping, also
   handy in the debugger; the buffer address no longer depends on them. */
static bool group_rx[USBFS_GROUPS];
static bool group_tx[USBFS_GROUPS];

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

#if (CH32H417_USB_SOF_EMULATION == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Virtual timer standing in for the missing device SOF interrupt.
 */
static virtual_timer_t sof_timer;

/**
 * @brief   Periodic SOF substitute.
 * @details chVTDoTickI() invokes callbacks with the kernel unlocked, so
 *          re-arming (an I-class operation) needs its own critical
 *          section. The callback is re-armed before the hook so a slow
 *          hook cannot stop the tick. USBConfig::sof_cb is left in the
 *          same context as a real SOF ISR: it takes the lock itself.
 */
static void sof_timer_cb(virtual_timer_t *vtp, void *p) {
  USBDriver *usbp = (USBDriver *)p;

  chSysLockFromISR();
  chVTSetI(vtp, TIME_MS2I(CH32H417_USB_SOF_PERIOD_MS),
           sof_timer_cb, p);
  chSysUnlockFromISR();

  if ((usbp->config != NULL) && (usbp->config->sof_cb != NULL)) {
    usbp->config->sof_cb(usbp);
  }
}
#endif

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
 * @brief   Transmit area of a group.
 * @note    The DMA path is one-way, a non-zero group is never enabled in
 *          both directions, so there is no +64 TX half: TX and RX both
 *          start at the buffer base. EP0 shares that buffer with the
 *          setup packet, which is copied out before the data stage.
 */
static inline uint8_t *group_tx_ptr(unsigned g) {

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

  if (token == UIS_TOKEN_SOF) {
    return;
  }

  if (token == UIS_TOKEN_SETUP) {
    *uep_tx_ctrl(0) = (uint8_t)(UEP_RES_NAK | UEP_TOG);
    *uep_rx_ctrl(0) = (uint8_t)(UEP_RES_NAK | UEP_TOG);

    _usb_isr_invoke_setup_cb(usbp, (usbep_t)0U);
    return;
  }

  ep = ep_group((usbep_t)addr);

  epcp = usbp->epc[ep];
  if (epcp == NULL) {
    return;
  }

  g = ep;

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

  USBFSD->INT_FG = (uint8_t)(fg & UIF_CLEARABLE);

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

    RCC->HBPCENR  |= RCC_USBOTGEN;
    RCC->HB2PCENR |= RCC_HB2Periph_GPIOA;
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

    USBFSD->BASE_CTRL = (uint8_t)(UC_DMA_EN |
#if CH32H417_USB_INT_BUSY
                                   UC_INT_BUSY |
#endif
                                   0U);

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

    usb_lld_reset(usbp);

    NVIC_EnableIRQ(USBFS_IRQn);

#if (CH32H417_USB_SOF_EMULATION == TRUE)
    chVTObjectInit(&sof_timer);
    chVTSetI(&sof_timer, TIME_MS2I(CH32H417_USB_SOF_PERIOD_MS),
             sof_timer_cb, (void *)usbp);
#endif
  }
}

/**
 * @brief   Deactivates the USB peripheral.
 *
 * @notapi
 */
void usb_lld_stop(USBDriver *usbp) {

  if (usbp->state != USB_STOP) {
#if (CH32H417_USB_SOF_EMULATION == TRUE)
    chVTResetI(&sof_timer);
#endif
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

  USBFSD->DEV_ADDR = (uint8_t)((USBFSD->DEV_ADDR & 0x80U) |
                               (usbp->address & 0x7FU));
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

  osalDbgAssert((g == 0U) || !(rx && tx),
                "endpoint configured in both directions");

  if (g == 0U) {
    group_rx[0] = true;
    group_tx[0] = true;
    *uep_dma(0) = (uint32_t)epbuf0;
    *uep_tx_ctrl(0) = UEP_RES_NAK;
    *uep_rx_ctrl(0) = UEP_RES_ACK;
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
