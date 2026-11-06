/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    test_usbfs.c
 * @brief   CH32H417 V5F port check: USBFS device test thread.
 *
 * @details The USBFS controller is configured as a CDC virtual COM port
 *          (the raw USB driver, no SerialUSB), the endpoint layout is the
 *          one of the vendor CDC example: EP1 interrupt IN for the CDC
 *          notification interface, EP2 bulk OUT and EP3 bulk IN for the
 *          data interface.
 *
 *          The data path needs a host: connect the device connector to a
 *          PC and open the COM port that enumerates, the test then sends
 *          a counter line on the bulk IN endpoint every two seconds and
 *          echoes every packet received on the bulk OUT endpoint, so both
 *          directions of the data interface can be walked interactively
 *          from a terminal.
 *
 *          Without a host the console line every two seconds shows the
 *          driver state and the counters: 0 STOP, 2 READY, 3 ADDRESS,
 *          4 ACTIVE, 5 SUSPENDED.
 */

#include <stdint.h>

/* The CDC header has to be the first ChibiOS header: the kernel pulls
   the vendor register set in, and that one re-defines the CDC request
   codes unless they are already defined. */
#include "hal_usb_cdc.h"

#include "ch.h"
#include "board.h"
#include "chprintf.h"
#include "portcheck.h"

#include <string.h>

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define USB_TEST_PERIOD_MS  2000U

#define USB_TEST_EP_NOTIFY  1U      /* CDC notification, interrupt IN.    */
#define USB_TEST_EP_RX      2U      /* CDC data, bulk OUT.                */
#define USB_TEST_EP_TX      3U      /* CDC data, bulk IN.                 */

#define USB_TEST_BUF_SIZE   64U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t usb_events;
volatile uint32_t usb_tx_frames;
volatile uint32_t usb_rx_frames;
volatile uint32_t usb_tx_dropped;
volatile uint32_t usb_ctl_flags;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static USBInEndpointState ep_notify_in;
static USBInEndpointState ep_tx_in;
static USBOutEndpointState ep_rx_out;

/* Packet buffers, the controller DMA reads and writes them directly. */
static uint8_t usb_rx_buf[USB_TEST_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t usb_tx_buf[USB_TEST_BUF_SIZE] __attribute__((aligned(4)));
static uint8_t usb_echo_buf[USB_TEST_BUF_SIZE] __attribute__((aligned(4)));

/* Emulated line coding, 115200 8N1. The DTE rate is a 32 bit little
   endian byte array per the CDC specification. */
static cdc_linecoding_t usb_line_coding = {
  {0x00, 0xC2, 0x01, 0x00},     /* dwDTERate, 115200 Baud.              */
  0x00,               /* bCharFormat, one stop bit.                       */
  0x00,               /* bParityType, none.                               */
  8                   /* bDataBits.                                       */
};

/* Endpoint callbacks, the endpoint configurations reference them. */
static void usb_data_sent(USBDriver *usbp, usbep_t ep);
static void usb_data_received(USBDriver *usbp, usbep_t ep);

/*
 * Device descriptor.
 */
static const uint8_t usb_device_descriptor_data[18] = {
  USB_DESC_DEVICE(0x0110,             /* bcdUSB (1.1).                    */
                  0x02,               /* bDeviceClass (CDC).              */
                  0x00,               /* bDeviceSubClass.                 */
                  0x00,               /* bDeviceProtocol.                 */
                  0x40,               /* bMaxPacketSize0.                 */
                  0x1A86,             /* idVendor (WCH).                  */
                  0xFE0C,             /* idProduct (vendor CDC example).  */
                  0x0100,             /* bcdDevice.                       */
                  1,                  /* iManufacturer.                   */
                  2,                  /* iProduct.                        */
                  0,                  /* iSerialNumber.                   */
                  1)                  /* bNumConfigurations.              */
};

static const USBDescriptor usb_device_descriptor = {
  sizeof usb_device_descriptor_data,
  usb_device_descriptor_data
};

/*
 * Configuration descriptor tree, standard CDC (no IAD), same layout as
 * the vendor SimulateCDC example.
 */
static const uint8_t usb_configuration_descriptor_data[67] = {
  /* Configuration descriptor. */
  USB_DESC_CONFIGURATION(67,          /* wTotalLength.                    */
                         2,           /* bNumInterfaces.                  */
                         0x01,        /* bConfigurationValue.             */
                         0,           /* iConfiguration.                  */
                         0x80,        /* bmAttributes (bus powered).      */
                         0x32),       /* bMaxPower (100mA).               */
  /* Control interface descriptor (CDC/ACM). */
  USB_DESC_INTERFACE(0x00, 0x00, 1,
                     0x02,            /* bInterfaceClass (CDC).           */
                     0x02,            /* bInterfaceSubClass (ACM).        */
                     0x01,            /* bInterfaceProtocol (AT).         */
                     0),
  /* CS_INTERFACE: header, bcdCDC 1.10. */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x00),
  USB_DESC_BCD(0x0110),
  /* CS_INTERFACE: call management, data class interface 1. */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x01),
  USB_DESC_BYTE(0x00), USB_DESC_BYTE(0x01),
  /* CS_INTERFACE: ACM, interface states supported. */
  USB_DESC_BYTE(4), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x02),
  USB_DESC_BYTE(0x02),
  /* CS_INTERFACE: union, control interface 0 + data interface 1. */
  USB_DESC_BYTE(5), USB_DESC_BYTE(0x24), USB_DESC_BYTE(0x06),
  USB_DESC_BYTE(0x00), USB_DESC_BYTE(0x01),
  /* Notification endpoint descriptor. */
  USB_DESC_ENDPOINT(USB_TEST_EP_NOTIFY | 0x80,
                    0x03,             /* bmAttributes (interrupt).        */
                    0x0008,           /* wMaxPacketSize.                  */
                    0xFF),            /* bInterval.                       */
  /* Data interface descriptor. */
  USB_DESC_INTERFACE(0x01, 0x00, 2,
                     0x0A,            /* bInterfaceClass (CDC data).      */
                     0x00,            /* bInterfaceSubClass.              */
                     0x00,            /* bInterfaceProtocol.              */
                     0),
  /* Data OUT endpoint descriptor. */
  USB_DESC_ENDPOINT(USB_TEST_EP_RX,
                    0x02,             /* bmAttributes (bulk).             */
                    USB_TEST_BUF_SIZE,
                    0),
  /* Data IN endpoint descriptor. */
  USB_DESC_ENDPOINT(USB_TEST_EP_TX | 0x80,
                    0x02,             /* bmAttributes (bulk).             */
                    USB_TEST_BUF_SIZE,
                    0)
};

static const USBDescriptor usb_configuration_descriptor = {
  sizeof usb_configuration_descriptor_data,
  usb_configuration_descriptor_data
};

/* U.S. English language identifier. */
static const uint8_t usb_string0[] = {
  USB_DESC_BYTE(4),
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  USB_DESC_WORD(0x0409)
};

/* Manufacturer string. */
static const uint8_t usb_string1[] = {
  USB_DESC_BYTE(8),
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  'W', 0, 'C', 0, 'H', 0
};

/* Product string. */
static const uint8_t usb_string2[] = {
  USB_DESC_BYTE(26),
  USB_DESC_BYTE(USB_DESCRIPTOR_STRING),
  'C', 0, 'H', 0, '3', 0, '2', 0, 'H', 0, '4', 0, '1', 0, '7', 0,
  ' ', 0, 'C', 0, 'D', 0, 'C', 0
};

static const USBDescriptor usb_strings[] = {
  {sizeof usb_string0, usb_string0},
  {sizeof usb_string1, usb_string1},
  {sizeof usb_string2, usb_string2}
};

/*
 * Endpoint configurations.
 */
static const USBEndpointConfig ep_notify_config = {
  USB_EP_MODE_TYPE_INTR,
  NULL,               /* setup_cb, only for control endpoints.            */
  NULL,               /* in_cb, the notifications stay silent.            */
  NULL,               /* out_cb, IN only.                                 */
  0x0008, 0x0000,     /* IN 8 bytes max, OUT unused.                      */
  &ep_notify_in, NULL,
  1, NULL
};

static const USBEndpointConfig ep_rx_config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  NULL,               /* in_cb, OUT only.                                 */
  usb_data_received,
  0x0000, USB_TEST_BUF_SIZE,
  NULL, &ep_rx_out,
  1, NULL
};

static const USBEndpointConfig ep_tx_config = {
  USB_EP_MODE_TYPE_BULK,
  NULL,
  usb_data_sent,
  NULL,               /* out_cb, IN only.                                 */
  USB_TEST_BUF_SIZE, 0x0000,
  &ep_tx_in, NULL,
  1, NULL
};

/*===========================================================================*/
/* Module local callbacks.                                                   */
/*===========================================================================*/

/* Handles the USB driver global events. Event callbacks are invoked by
   the driver ISR without the kernel lock held, they have to take it
   themselves for the I-class calls (the config path locks the endpoints
   in). */
static void usb_event(USBDriver *usbp, usbevent_t event) {
  usb_events++;

  switch (event) {
  case USB_EVENT_ADDRESS:
  case USB_EVENT_RESET:
  case USB_EVENT_UNCONFIGURED:
  case USB_EVENT_SUSPEND:
  case USB_EVENT_STALLED:
    return;
  case USB_EVENT_CONFIGURED:
    chSysLockFromISR();
    /* The configuration callback reinitializes all the endpoints, the
       reset event invalidated them in the driver. */
    usbInitEndpointI(usbp, USB_TEST_EP_NOTIFY, &ep_notify_config);
    usbInitEndpointI(usbp, USB_TEST_EP_RX, &ep_rx_config);
    usbInitEndpointI(usbp, USB_TEST_EP_TX, &ep_tx_config);
    /* The host may start sending on the OUT endpoint right away. */
    usbStartReceiveI(usbp, USB_TEST_EP_RX, usb_rx_buf, sizeof usb_rx_buf);
    chSysUnlockFromISR();
    return;
  case USB_EVENT_WAKEUP:
    /* The suspend path dropped the armed receive request, rearm it so
       the OUT endpoint accepts the next packet again. The endpoint is
       registered only when the host has selected a configuration: a
       suspend raised in the middle of the enumeration restores the
       previous state, that is not USB_ACTIVE, and the endpoint
       configuration is not there yet. Re-arming an unregistered
       endpoint makes the generic layer dereference a null endpoint
       configuration. */
    chSysLockFromISR();
    if ((usbGetDriverStateI(usbp) == USB_ACTIVE) &&
        (usbp->epc[USB_TEST_EP_RX] != NULL)) {
      usbStartReceiveI(usbp, USB_TEST_EP_RX, usb_rx_buf, sizeof usb_rx_buf);
    }
    chSysUnlockFromISR();
    return;
  }
}

/*
 * Handles the GET_DESCRIPTOR callback. All required descriptors must be
 * handled here.
 */
static const USBDescriptor *usb_get_descriptor(USBDriver *usbp,
                                               uint8_t dtype,
                                               uint8_t dindex,
                                               uint16_t lang) {
  (void)usbp;
  (void)lang;

  switch (dtype) {
  case USB_DESCRIPTOR_DEVICE:
    return &usb_device_descriptor;
  case USB_DESCRIPTOR_CONFIGURATION:
    return &usb_configuration_descriptor;
  case USB_DESCRIPTOR_STRING:
    if (dindex < 3U) {
      return &usb_strings[dindex];
    }
    break;
  }
  return NULL;
}

/*
 * Handles the CDC class requests the same way the SerialUSB driver
 * does, plus keeps the control line state at the observation point.
 */
static bool usb_requests_hook(USBDriver *usbp) {
  if ((usbp->setup[0] & USB_RTYPE_TYPE_MASK) == USB_RTYPE_TYPE_CLASS) {
    switch (usbp->setup[1]) {
    case CDC_GET_LINE_CODING:
      usbSetupTransfer(usbp, (uint8_t *)&usb_line_coding,
                       sizeof usb_line_coding, NULL);
      return true;
    case CDC_SET_LINE_CODING:
      usbSetupTransfer(usbp, (uint8_t *)&usb_line_coding,
                       sizeof usb_line_coding, NULL);
      return true;
    case CDC_SET_CONTROL_LINE_STATE:
      /* The wValue bits land at the observation point. */
      usb_ctl_flags = (uint32_t)usbp->setup[2] |
                      ((uint32_t)usbp->setup[3] << 8);
      usbSetupTransfer(usbp, NULL, 0, NULL);
      return true;
    default:
      return false;
    }
  }
  return false;
}

/* IN endpoint data sent, the counters serve both the alive reports and
   the echoes. */
static void usb_data_sent(USBDriver *usbp, usbep_t ep) {
  (void)usbp;
  (void)ep;
  usb_tx_frames++;
}

/*
 * OUT endpoint packet received, echoes it back on the data IN endpoint
 * if that one is idle, then rearms the receive.
 */
static void usb_data_received(USBDriver *usbp, usbep_t ep) {
  size_t n;

  /* The endpoint callbacks are invoked unlocked, the state updates and
     rearming happen inside the kernel lock. */
  osalSysLockFromISR();

  n = usbGetReceiveTransactionSizeX(usbp, ep);

  usb_rx_frames++;

  if (n > 0U) {
    memcpy(usb_echo_buf, usb_rx_buf, n);
    if (!usbGetTransmitStatusI(usbp, USB_TEST_EP_TX)) {
      usbStartTransmitI(usbp, USB_TEST_EP_TX, usb_echo_buf, n);
    }
    else {
      /* The data IN endpoint was still busy with the previous echo or
         with the alive report, this packet cannot be echoed back. */
      usb_tx_dropped++;
    }
  }

  /* Ready for the next packet. */
  usbStartReceiveI(usbp, ep, usb_rx_buf, sizeof usb_rx_buf);

  osalSysUnlockFromISR();
}

/* USB driver configuration. */
static const USBConfig usb_config = {
  usb_event,
  usb_get_descriptor,
  usb_requests_hook,
  NULL                /* sof_cb, no device SOF flag on this controller. */
};

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waUsbTest, 1024);

static THD_FUNCTION(usbfs_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  usbstate_t st;
  uint32_t seq = 0U;
  size_t n;

  (void)arg;

  /* Disconnect first, a reset without replugging the cable starts the
     enumeration from a clean pullup state. */
  usbDisconnectBus(&USBD1);
  chThdSleepMilliseconds(1500U);

  if (usbStart(&USBD1, &usb_config) != HAL_RET_SUCCESS) {
    chprintf(chp, "USB: driver start FAILED\r\n");
    return;
  }
  usbConnectBus(&USBD1);
  chprintf(chp, "USB: CDC VCP started, connect the device port to a host "
                "and open the COM port\r\n");

  while (true) {
    seq++;

    /* Alive report on the bulk IN endpoint, only when the host has
       selected a configuration and the endpoint is idle. The report
       buffer is copied first, a pending arm does not keep the pointer. */
    chsnprintf((char *)usb_tx_buf, USB_TEST_BUF_SIZE,
               "usbfs alive %u\r\n", (unsigned)seq);
    chSysLock();
    st = usbGetDriverStateI(&USBD1);
    if ((st == USB_ACTIVE) &&
        !usbGetTransmitStatusI(&USBD1, USB_TEST_EP_TX)) {
      n = strlen((char *)usb_tx_buf);
      usbStartTransmitI(&USBD1, USB_TEST_EP_TX, usb_tx_buf, n);
    }
    chSysUnlock();

    chprintf(chp,
             "USB: state=%u events=%u tx=%u rx=%u drop=%u ctl=%04x\r\n",
             (unsigned)st, (unsigned)usb_events,
             (unsigned)usb_tx_frames, (unsigned)usb_rx_frames,
             (unsigned)usb_tx_dropped, (unsigned)usb_ctl_flags);

    chThdSleepMilliseconds(USB_TEST_PERIOD_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the USBFS test thread.
 */
void usbfs_test_start(void) {

  chThdCreateStatic(waUsbTest, sizeof(waUsbTest), NORMALPRIO,
                    usbfs_test_thd, NULL);
}
