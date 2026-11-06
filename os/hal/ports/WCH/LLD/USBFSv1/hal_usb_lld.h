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
 * @file    USBFSv1/hal_usb_lld.h
 * @brief   CH32H417 USB device low level driver header.
 *
 * @details Device-side driver on the WCH proprietary USBFS controller
 *          (RM ch26): eight endpoint groups (UEP0..7) with separate
 *          TX/RX control registers, per-group DMA buffer pointers and
 *          a global transfer interrupt (INT_FG/INT_ST). Host mode,
 *          USBHS/USBSS and isochronous double buffering are not
 *          implemented. The SOF callback cannot fire (no device SOF
 *          flag) and usbGetFrameNumberX() returns zero.
 *
 * @note    The endpoint data buffers are placed in the .ram2 section
 *          (shared SRAM): the USBFS DMA engine only reaches the SRAM
 *          bus (RM ch26 RB_DMA_EN) and truncates the DMA address to
 *          the 2MB RAM window, so DTCM/ITCM placement generates bus
 *          error NMIs on the first transfer.
 *
 * @note    A setup packet is delivered by the controller at the beginning
 *          of the EP0 DMA buffer, the very same place where the data
 *          stage of the control transfer is assembled: the setup packet
 *          is copied out of that buffer before the data stage starts.
 *
 * @addtogroup USB
 * @{
 */

#ifndef HAL_USB_LLD_H
#define HAL_USB_LLD_H

#if (HAL_USE_USB == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   USBFS device unit enable switch.
 */
#if !defined(CH32H417_USB_USE_USBFS) || defined(__DOXYGEN__)
#define CH32H417_USB_USE_USBFS             FALSE
#endif

/**
 * @brief   48MHz clock source select (RCC_CFGR2.USBFSSRC).
 */
#if !defined(CH32H417_USBFS_CLK_SRC) || defined(__DOXYGEN__)
#define CH32H417_USBFS_CLK_SRC             RCC_USBFSCLKSource_USBHSPLL
#endif

/**
 * @brief   48MHz clock divider (RCC_USBFS48ClockSourceDivConfig).
 * @note    The USBHS PLL runs at 480MHz, the USBFS entry needs 48MHz
 *          so the divider stays at Div10 (vendor USBFS_RCC_Init()).
 */
#if !defined(CH32H417_USBFS_CLK_DIV) || defined(__DOXYGEN__)
#define CH32H417_USBFS_CLK_DIV             RCC_USBFS_Div10
#endif

/**
 * @brief   USBHS PLL reference frequency (RCC_USBHSPLLReferConfig).
 * @note    The port assumes a 25MHz HSE, the USBHS PLL is programmed
 *          for a 25MHz reference.
 */
#if !defined(CH32H417_USBHS_PLL_REFER) || defined(__DOXYGEN__)
#define CH32H417_USBHS_PLL_REFER           RCC_USBHSPLLRefer_25M
#endif

/**
 * @brief   Enable and wait for the USBHS PLL during start.
 */
#if !defined(CH32H417_USB_SETUP_USBHSPLL) || defined(__DOXYGEN__)
#define CH32H417_USB_SETUP_USBHSPLL        TRUE
#endif

/**
 * @brief   Bus speed of the device port (false = 12Mbps full speed).
 */
#if !defined(CH32H417_USB_LOW_SPEED) || defined(__DOXYGEN__)
#define CH32H417_USB_LOW_SPEED             FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_USB_USE_USBFS
#error "USB driver activated but the USBFS unit is not assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Maximum endpoint address.
 * @note    The controller exposes eight endpoint groups (RM 26.2.2.2 to
 *          RM 26.2.2.5), the endpoint addresses 8..15 are aliases of the
 *          addresses 0..7 and share their buffer, so only the addresses
 *          0..7 are usable as ChibiOS endpoint numbers. This value is
 *          the number of endpoints excluding the endpoint zero, exactly
 *          like the STM32 reference drivers.
 */
#define USB_MAX_ENDPOINTS                   7

/**
 * @brief   Status stage handling method.
 * @note    The status stage is generated in software by the generic
 *          driver (the hardware performs no implicit status stage).
 */
#define USB_EP0_STATUS_STAGE                USB_EP0_STATUS_STAGE_SW

/**
 * @brief   Address change timing.
 * @details The device changes its address only after the status
 *          packet of the SET_ADDRESS request has completed.
 */
#define USB_SET_ADDRESS_MODE                USB_LATE_SET_ADDRESS

/**
 * @brief   Method for set address acknowledge.
 */
#define USB_SET_ADDRESS_ACK_HANDLING        USB_SET_ADDRESS_ACK_SW

/**
 * @brief   Type of an IN endpoint state structure.
 */
typedef struct {
  /**
   * @brief   Requested transmit transfer size.
   */
  size_t                        txsize;
  /**
   * @brief   Transmitted bytes so far.
   */
  size_t                        txcnt;
  /**
   * @brief   Pointer to the transmission linear buffer.
   */
  const uint8_t                 *txbuf;
#if (USB_USE_WAIT == TRUE) || defined(__DOXYGEN__)
  /**
   * @brief   Waiting thread.
   */
  thread_reference_t            thread;
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief   Size of the last transmitted packet.
   */
  size_t                        txlast;
} USBInEndpointState;

/**
 * @brief   Type of an OUT endpoint state structure.
 */
typedef struct {
  /**
   * @brief   Requested receive transfer size.
   */
  size_t                        rxsize;
  /**
   * @brief   Received bytes so far.
   */
  size_t                        rxcnt;
  /**
   * @brief   Pointer to the receive linear buffer.
   */
  uint8_t                       *rxbuf;
#if (USB_USE_WAIT == TRUE) || defined(__DOXYGEN__)
  /**
   * @brief   Waiting thread.
   */
  thread_reference_t            thread;
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief   Number of packets to receive.
   */
  uint16_t                      rxpkts;
} USBOutEndpointState;

/**
 * @brief   Type of an endpoint configuration structure.
 */
typedef struct {
  /**
   * @brief   Endpoint mode bitmask.
   */
  uint32_t                      ep_mode;
  /**
   * @brief   Setup packet notification callback.
   * @note    Only valid for @p USB_EP_MODE_TYPE_CTRL endpoints, it
   *          should be set to @p NULL for other endpoint types.
   */
  usbepcallback_t                 setup_cb;
  /**
   * @brief   IN endpoint notification callback.
   * @details This field must be set to @p NULL if callback is not required.
   */
  usbepcallback_t                 in_cb;
  /**
   * @brief   OUT endpoint notification callback.
   * @details This field must be set to @p NULL if callback is not required.
   */
  usbepcallback_t                 out_cb;
  /**
   * @brief   IN endpoint maximum packet size.
   * @details This field must be set to zero if the IN endpoint is not used.
   */
  uint16_t                        in_maxsize;
  /**
   * @brief   OUT endpoint maximum packet size.
   * @details This field must be set to zero if the OUT endpoint is not used.
   */
  uint16_t                        out_maxsize;
  /**
   * @brief   @p USBEndpointState associated to the IN endpoint.
   * @details This field must be set to @p NULL if the IN endpoint is not
   *          used.
   */
  USBInEndpointState              *in_state;
  /**
   * @brief   @p USBEndpointState associated to the OUT endpoint.
   * @details This field must be set to @p NULL if the OUT endpoint is not
   *          used.
   */
  USBOutEndpointState             *out_state;
  /* End of the mandatory fields.*/
  /**
   * @brief   Reserved field, not currently used.
   * @note    Initialize this field to 1 in order to be forward compatible.
   */
  uint16_t                        ep_buffers;
  /**
   * @brief   Pointer to a buffer for setup packets.
   * @details Setup packets require a dedicated 8-bytes buffer, set this
   *          field to @p NULL for non-control endpoints.
   */
  uint8_t                         *setup_buf;
} USBEndpointConfig;

/**
 * @brief   Type of an USB driver configuration structure.
 */
typedef struct {
  /**
   * @brief   USB events callback.
   * @details This callback is invoked when an USB driver event is registered.
   */
  usbeventcb_t                    event_cb;
  /**
   * @brief   Device GET_DESCRIPTOR request callback.
   * @note    This callback is mandatory and cannot be set to @p NULL.
   */
  usbgetdescriptor_t              get_descriptor_cb;
  /**
   * @brief   Requests hook callback.
   * @details This hook allows to be notified of standard requests or to
   *          handle non standard requests.
   */
  usbreqhandler_t                 requests_hook_cb;
  /**
   * @brief   Start Of Frame callback.
   * @details Never invoked by this driver (no device-side SOF flag).
   */
  usbcallback_t                   sof_cb;
  /* End of the mandatory fields.*/
} USBConfig;

/**
 * @brief   Structure representing an USB driver.
 */
struct USBDriver {
  /**
   * @brief   Driver state.
   */
  usbstate_t                      state;
  /**
   * @brief   Current configuration data.
   */
  const USBConfig                 *config;
  /**
   * @brief   Bit map of the transmitting IN endpoints.
   */
  uint16_t                        transmitting;
  /**
   * @brief   Bit map of the receiving OUT endpoints.
   */
  uint16_t                        receiving;
  /**
   * @brief   Active endpoints configurations.
   */
  const USBEndpointConfig         *epc[USB_MAX_ENDPOINTS + 1];
  /**
   * @brief   Fields available to user, it can be used to associate an
   *          application-defined handler to an IN endpoint.
   * @note    The base index is one, the endpoint zero does not have a
   *          reserved element in this array.
   */
  void                            *in_params[USB_MAX_ENDPOINTS];
  /**
   * @brief   Fields available to user, it can be used to associate an
   *          application-defined handler to an OUT endpoint.
   * @note    The base index is one, the endpoint zero does not have a
   *          reserved element in this array.
   */
  void                            *out_params[USB_MAX_ENDPOINTS];
  /**
   * @brief   Endpoint 0 state.
   */
  usbep0state_t                   ep0state;
  /**
   * @brief   Next position in the buffer to be transferred through endpoint 0.
   */
  uint8_t                         *ep0next;
  /**
   * @brief   Number of bytes yet to be transferred through endpoint 0.
   */
  size_t                          ep0n;
  /**
   * @brief   Endpoint 0 end transaction callback.
   */
  usbcallback_t                   ep0endcb;
  /**
   * @brief   Setup packet buffer.
   */
  uint8_t                         setup[8];
  /**
   * @brief   Current USB device status.
   */
  uint16_t                        status;
  /**
   * @brief   Assigned USB address.
   */
  uint8_t                         address;
  /**
   * @brief   Current USB device configuration.
   */
  uint8_t                         configuration;
  /**
   * @brief   State of the driver when a suspend happened.
   */
  usbstate_t                      saved_state;
#if defined(USB_DRIVER_EXT_FIELDS)
  USB_DRIVER_EXT_FIELDS
#endif
  /* End of the mandatory fields.*/
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Returns the current frame number.
 * @note    The device register set has no frame counter, the SOF
 *          callback is consequently never invoked by this driver.
 */
#define usb_lld_get_frame_number(usbp) ((uint16_t)0U)

/**
 * @brief   Returns the exact size of a receive transaction.
 */
#define usb_lld_get_transaction_size(usbp, ep)                              \
  ((usbp)->epc[ep]->out_state->rxcnt)

/**
 * @brief   Connects the USB device (system control = device enabled
 *          with the internal 1.5K pull-up, RM table 26-2).
 */
#define usb_lld_connect_bus(usbp)                                           \
  (USBFSD->BASE_CTRL = (uint8_t)((USBFSD->BASE_CTRL & ~0x30U) | 0x20U))

/**
 * @brief   Disconnects the USB device (device function disabled).
 */
#define usb_lld_disconnect_bus(usbp)                                        \
  (USBFSD->BASE_CTRL = (uint8_t)(USBFSD->BASE_CTRL & ~0x30U))

/**
 * @brief   Start of host wake-up procedure (device-only driver).
 */
#define usb_lld_wakeup_host(usbp) do { (void)(usbp); } while (false)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_USB_USE_USBFS == TRUE) && !defined(__DOXYGEN__)
extern USBDriver USBD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void usb_lld_init(void);
  void usb_lld_start(USBDriver *usbp);
  void usb_lld_stop(USBDriver *usbp);
  void usb_lld_reset(USBDriver *usbp);
  void usb_lld_set_address(USBDriver *usbp);
  void usb_lld_init_endpoint(USBDriver *usbp, usbep_t ep);
  void usb_lld_disable_endpoints(USBDriver *usbp);
  usbepstatus_t usb_lld_get_status_in(USBDriver *usbp, usbep_t ep);
  usbepstatus_t usb_lld_get_status_out(USBDriver *usbp, usbep_t ep);
  void usb_lld_read_setup(USBDriver *usbp, usbep_t ep, uint8_t *buf);
  void usb_lld_start_out(USBDriver *usbp, usbep_t ep);
  void usb_lld_start_in(USBDriver *usbp, usbep_t ep);
  void usb_lld_stall_out(USBDriver *usbp, usbep_t ep);
  void usb_lld_stall_in(USBDriver *usbp, usbep_t ep);
  void usb_lld_clear_out(USBDriver *usbp, usbep_t ep);
  void usb_lld_clear_in(USBDriver *usbp, usbep_t ep);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_USB */

#endif /* HAL_USB_LLD_H */

/** @} */
