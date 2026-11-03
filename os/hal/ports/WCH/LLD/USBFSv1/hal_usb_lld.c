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

/* INT_ST token field [5:4]. */
#define UIS_TOKEN_OUT       0U
#define UIS_TOKEN_IN        2U
#define UIS_TOKEN_SETUP     3U

/* UEPn_TX_CTRL / UEPn_RX_CTRL fields. */
#define UEP_RES_MASK        0x03U
#define UEP_RES_ACK         0x00U
#define UEP_RES_NAK         0x02U
#define UEP_RES_STALL       0x03U
#define UEP_TOG             (1U << 2)
#define UEP_AUTO_TOG        (1U << 3)

/* UEPn_MOD register bits. */
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
 * @brief   Buffer for the EP0 setup packets.
 */
static uint8_t ep0setup_buffer[8];

/**
 * @brief   EP0 buffer (shared TX/RX, 4-byte aligned, +2 slack).
 */
static uint8_t epbuf0[68] __attribute__((aligned(4)));

/**
 * @brief   Per-group buffers (single 64+64 layout plus slack).
 */
static uint8_t epbuf1[136] __attribute__((aligned(4)));
static uint8_t epbuf2[136] __attribute__((aligned(4)));
static uint8_t epbuf3[136] __attribute__((aligned(4)));
static uint8_t epbuf4[136] __attribute__((aligned(4)));
static uint8_t epbuf5[136] __attribute__((aligned(4)));
static uint8_t epbuf6[136] __attribute__((aligned(4)));
static uint8_t epbuf7[136] __attribute__((aligned(4)));

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
  .setup_buf        = ep0setup_buffer
};

/* Per-group buffer base addresses (index = hardware group 0..7). */
static uint8_t *const group_buf[8] = {
  epbuf0, epbuf1, epbuf2, epbuf3, epbuf4, epbuf5, epbuf6, epbuf7
};

/* Which directions are enabled in each group (layout bookkeeping). */
static bool group_rx[8];
static bool group_tx[8];

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Maps an endpoint number (0..15) to its hardware group (0..7).
 */
static unsigned ep_group(usbep_t ep) {

  if (ep < 8U) {
    return (unsigned)ep;
  }

  switch (ep) {
  case 8:
  case 12:
    return 4U;
  case 9:
    return 1U;
  case 10:
    return 2U;
  case 11:
    return 3U;
  case 13:
    return 5U;
  case 14:
    return 6U;
  default:
    return 7U;
  }
}

/**
 * @brief   Pointer to the TX length register of a group.
 */
static inline volatile uint8_t *uep_tx_len(unsigned g) {

  return &USBFSD->UEP0_TX_LEN + (g * 4);
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
 */
static void usb_serve_transfer(USBDriver *usbp) {
  uint32_t ist = USBFSD->INT_ST;
  unsigned ep = ist & 0x0FU;
  unsigned token = (ist >> 4) & 3U;
  const USBEndpointConfig *epcp = usbp->epc[ep];
  unsigned g = ep_group(ep);

  if (epcp == NULL) {
    return;
  }

  if (token == UIS_TOKEN_SETUP) {
    /* EP0 next stages expect DATA1 on both directions. */
    *uep_tx_ctrl(0) = (uint8_t)(UEP_RES_NAK | UEP_TOG);
    *uep_rx_ctrl(0) = (uint8_t)(UEP_RES_ACK | UEP_TOG);

    _usb_isr_invoke_setup_cb(usbp, ep);
    return;
  }

  if (token == UIS_TOKEN_IN) {
    USBInEndpointState *isp = epcp->in_state;
    size_t n;

    if (isp == NULL) {
      return;
    }

    isp->txcnt += isp->txlast;
    n = isp->txsize - isp->txcnt;
    if (n > 0U) {
      /* More data to push, load the next packet. */
      if (n > (size_t)epcp->in_maxsize) {
        n = (size_t)epcp->in_maxsize;
      }
      isp->txbuf += isp->txlast;
      isp->txlast = n;
      memcpy(group_tx_ptr(g), isp->txbuf, n);
      *uep_tx_len(g) = (uint8_t)n;
      /* Arm the response (keep TOG/AUTO, RES=00). */
      *uep_tx_ctrl(g) = (uint8_t)(*uep_tx_ctrl(g) &
                                  ~(UEP_RES_MASK | UEP_AUTO_TOG | UEP_TOG)) |
                       (uint8_t)(*uep_tx_ctrl(g) & (UEP_AUTO_TOG | UEP_TOG));
      if (g == 0U) {
        /* EP0 manual toggling: DATA0 next if data continues, DATA1 for
           a status ZLP. */
        *uep_tx_ctrl(g) = (uint8_t)((*uep_tx_ctrl(g) & UEP_TOG) ^ UEP_TOG);
        *uep_tx_ctrl(g) = (uint8_t)((*uep_tx_ctrl(g) & ~UEP_RES_MASK) |
                                    UEP_RES_ACK);
      }
      else {
        *uep_tx_ctrl(g) = (uint8_t)(*uep_tx_ctrl(g) & ~UEP_RES_MASK);
      }
    }
    else {
      /* Transfer complete: park the endpoint until the next start. */
      *uep_tx_ctrl(g) = (uint8_t)((*uep_tx_ctrl(g) & (UEP_AUTO_TOG | UEP_TOG)) |
                                  UEP_RES_NAK);
      _usb_isr_invoke_in_cb(usbp, ep);
    }
    return;
  }

  /* OUT data packet. */
  {
    USBOutEndpointState *osp = epcp->out_state;
    size_t n = (size_t)(USBFSD->RX_LEN & 0x3FFU);

    if (osp == NULL) {
      return;
    }

    memcpy(osp->rxbuf, group_rx_ptr(g), n);
    osp->rxbuf += n;
    osp->rxcnt  += n;
    osp->rxsize -= n;
    osp->rxpkts -= 1U;

    if ((n < (size_t)epcp->out_maxsize) || (osp->rxpkts == 0U)) {
      /* Transfer complete. */
      *uep_rx_ctrl(g) = (uint8_t)((*uep_rx_ctrl(g) & (UEP_AUTO_TOG | UEP_TOG)) |
                                  UEP_RES_NAK);
      _usb_isr_invoke_out_cb(usbp, ep);
    }
    else {
      /* More packets expected, re-arm with the next DATA toggle. */
      if (g == 0U) {
        *uep_rx_ctrl(g) = (uint8_t)((*uep_rx_ctrl(g) ^ UEP_TOG) &
                                    ~UEP_RES_MASK);
      }
      else {
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

  OSAL_IRQ_PROLOGUE();

  osalSysLockFromISR();

  /* Bus reset (device mode flag 0). */
  if ((fg & UIF_BUS_RST) != 0U) {
    _usb_reset(&USBD1);
  }

  /* Suspend or wakeup (flag 2, direction taken from MIS_ST). */
  if ((fg & UIF_SUSPEND) != 0U) {
    if ((USBFSD->MIS_ST & (1U << 2)) != 0U) {
      _usb_suspend(&USBD1);
    }
    else {
      _usb_wakeup(&USBD1);
    }
  }

  /* Transfer completion (flag 1): endpoint dispatching. */
  if ((fg & UIF_TRANSFER) != 0U) {
    usb_serve_transfer(&USBD1);
  }

  /* Clear the write-one-to-clear flags we processed (and stale FIFO
     overruns), the rest is left for the polling context.*/
  USBFSD->INT_FG = (uint8_t)(fg & (UIF_BUS_RST | UIF_TRANSFER |
                                   UIF_SUSPEND | UIF_FIFO_OV));

  osalSysUnlockFromISR();

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

  if (usbp->state == USB_STOP) {
    /* 48MHz clock chain: USBHS PLL (HSE 25MHz reference at defaults),
       source select and divider.*/
#if CH32H417_USB_SETUP_USBHSPLL
    RCC_USBHS_PLLCmd(ENABLE);
    CH32H417_DSB();
    {
      unsigned spin = 1000000U;
      while ((RCC_GetFlagStatus(RCC_FLAG_USBHSPLLRDY) == RESET) &&
             (spin > 0U)) {
        spin--;
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

    /* Device physical port: full/low speed, internal pull-down
       disabled (device uses the SYS_CTRL pull-up), port enabled,
       device function off until usbConnectBus(). */
    USBFSD->UDEV_CTRL = (uint8_t)(UD_PORT_EN | UD_PD_DIS |
#if CH32H417_USB_LOW_SPEED
                                  UD_LOW_SPEED |
#endif
                                  0U);
    USBFSD->BASE_CTRL &= (uint8_t)~(UC_HOST_MODE | UC_SYS_CTRL_MSK);

    NVIC_EnableIRQ(USBFS_IRQn);

    /* Reset procedure enforced on driver start.*/
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
 *
 * @notapi
 */
void usb_lld_reset(USBDriver *usbp) {
  uint8_t bs = USBFSD->BASE_CTRL;

  /* Pulse the SIE reset and clear FIFO/flags (manual clear required,
     RM 26.2.1.1). */
  USBFSD->BASE_CTRL = (uint8_t)(bs | UC_RST_SIE | UC_CLR_ALL);
  USBFSD->BASE_CTRL = (uint8_t)(bs & ~(UC_RST_SIE | UC_CLR_ALL));

  /* Clear all pending flags, device address zero. */
  USBFSD->INT_FG  = 0xFFU;
  USBFSD->DEV_ADDR = 0U;
  /* The clear must be committed before interrupt sources are enabled
     below, otherwise a stale flag could fire immediately. */
  CH32H417_DSB();

  /* Bus reset, transfer and suspend sources. */
  USBFSD->INT_EN = UIF_BUS_RST | UIF_TRANSFER | UIF_SUSPEND;

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
  const USBEndpointConfig *epcp = usbp->epc[ep];
  unsigned g = ep_group(ep);
  bool rx = epcp->out_state != NULL;
  bool tx = epcp->in_state != NULL;

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

  for (i = 1U; i <= 7U; i++) {
    ep_mod_set(i, false, false);
    group_rx[i] = false;
    group_tx[i] = false;
    *uep_tx_ctrl(i) = 0U;
    *uep_rx_ctrl(i) = 0U;
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
 *
 * @notapi
 */
void usb_lld_read_setup(USBDriver *usbp, usbep_t ep, uint8_t *buf) {

  (void)usbp;
  (void)ep;
  memcpy(buf, ep0setup_buffer, 8U);
}

/**
 * @brief   Starts a receive operation on an OUT endpoint.
 *
 * @notapi
 */
void usb_lld_start_out(USBDriver *usbp, usbep_t ep) {
  USBOutEndpointState *osp = usbp->epc[ep]->out_state;
  unsigned g = ep_group(ep);
  volatile uint8_t *ctrl = uep_rx_ctrl(g);

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
  USBInEndpointState *isp = usbp->epc[ep]->in_state;
  unsigned g = ep_group(ep);
  volatile uint8_t *ctrl = uep_tx_ctrl(g);

  /* Transfer initialization.*/
  n = isp->txsize;
  if (n > (size_t)usbp->epc[ep]->in_maxsize) {
    n = (size_t)usbp->epc[ep]->in_maxsize;
  }

  isp->txlast = n;
  memcpy(group_tx_ptr(g), isp->txbuf, n);
  *uep_tx_len(g) = (uint8_t)n;

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
