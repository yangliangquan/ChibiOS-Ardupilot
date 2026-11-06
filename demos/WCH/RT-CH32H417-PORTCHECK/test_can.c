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
 * @file    test_can.c
 * @brief   CH32H417 V5F port check: CAN1 loopback test thread.
 *
 * @details CAN1 runs in internal loopback mode, the controller receives its
 *          own transmission, so the whole driver path can be validated
 *          without a transceiver and without a second node on the bus. The
 *          receive filter stays active in this mode (RM 28.3.2), so the
 *          filter block is exercised as well.
 *
 *          Every attempt programs one filter bank per FIFO: bank 0 accepts
 *          only standard data frames and routes them to FIFO0, bank 1 does
 *          the same for extended data frames and routes them to FIFO1. One
 *          frame per identifier type is transmitted, read back from the
 *          matching mailbox and compared field by field. This covers the
 *          mailboxes, the filter block, both FIFOs and the interrupt driven
 *          driver queues.
 *
 * @note    The hardware filter block is not available on the version-A
 *          silicon (see the vendor CAN TestMode example), the two frames
 *          then land in FIFO0 and the FIFO1 receive reports a timeout.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

#include "ch32h417_can.h"
#include "ch32h417_dbgmcu.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define CAN_TEST_RETRY_MS 5000U

/* Portability layer retry bounds, in milliseconds. */
#define CAN_TEST_TX_TIMEOUT_MS 100U
#define CAN_TEST_RX_TIMEOUT_MS 100U

/* Bit rate and its segmentation: 1 + 15 + 4 = 20 time quanta, an 80%
   sample point, the prescaler is derived from the live HCLK at start. */
#define CAN_TEST_BAUD 500000U
#define CAN_TEST_TQ   20U

/* Identifiers used by the two frames, one per FIFO. */
#define CAN_TEST_STD_ID 0x123U
#define CAN_TEST_EXT_ID 0x1234567U

/* Payload length of both frames. */
#define CAN_TEST_DLC 8U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t can_tx_frames;
volatile uint32_t can_rx_frames;
volatile uint32_t can_failures;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/* Filled by the start function, the driver keeps the pointer. */
static CANConfig can_config;

/*
 * One 32 bit filter bank per FIFO, mask mode. The identifier bits, the IDE
 * bit and the RTR bit are all compared, so a standard data frame only
 * matches bank 0 and an extended data frame only matches bank 1 (RM 28.5.6,
 * table 28-1 bit mapping: STID at bit 21, EXID at bit 3, IDE at bit 2).
 */
static const CANFilter can_filters[2] = {
  {
    .filter     = 0U,
    .mode       = 0U,                       /* Mask mode.               */
    .scale      = 1U,                       /* 32 bits.                 */
    .assignment = 0U,                       /* FIFO0.                   */
    .register1  = (CAN_TEST_STD_ID << 21),
    .register2  = (0x7FFUL << 21) | (1UL << 2) | (1UL << 1)
  },
  {
    .filter     = 1U,
    .mode       = 0U,                       /* Mask mode.               */
    .scale      = 1U,                       /* 32 bits.                 */
    .assignment = 1U,                       /* FIFO1.                   */
    .register1  = (CAN_TEST_EXT_ID << 3) | (1UL << 2),
    .register2  = (0x1FFFFFFFUL << 3) | (1UL << 2) | (1UL << 1)
  }
};

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*
 * CAN1 pin mapping, first alternate function set of the controller:
 *   TX = PA12 (AF9), RX = PA11 (AF9).
 * The internal loopback does not use the RX input and needs no transceiver,
 * the mapping is here only to keep the pins in the state a real bus would
 * need. The RX pin is an input, the TX pin is a push-pull output.
 */
static void can_pins_init(void) {

  palSetPadMode(IOPORT1, 12U, PAL_MODE_ALTERNATE(9));       /* CAN1_TX. */
  palSetPadMode(IOPORT1, 11U, PAL_MODE_ALTERNATE_INPUT(9)); /* CAN1_RX. */
}

/*
 * Prints a payload as hex bytes, no line terminator is added.
 */
static void can_print_data(BaseSequentialStream *chp,
                           const uint8_t *datap, uint8_t dlc) {
  unsigned i;

  for (i = 0U; i < dlc; i++) {
    chprintf(chp, " %02x", (unsigned)datap[i]);
  }
}

/*
 * Transmits one frame, receives it back from the given mailbox and compares
 * identifier type, identifier, length and payload. Returns false when any
 * step fails or the content does not match, the failure is printed.
 */
static bool can_exchange(BaseSequentialStream *chp, const char *name,
                         bool extended, canid_t id, canmbx_t mailbox) {
  CANTxFrame txf;
  CANRxFrame rxf;
  uint8_t data[CAN_TEST_DLC];
  msg_t msg;
  unsigned i;

  for (i = 0U; i < CAN_TEST_DLC; i++) {
    data[i] = (uint8_t)((extended ? 0x20U : 0x10U) + i);
  }

  txf.IDE = extended ? 1U : 0U;
  txf.RTR = 0U;
  txf.DLC = CAN_TEST_DLC;
  if (extended) {
    txf.EID = id;
  }
  else {
    txf.SID = id;
  }
  for (i = 0U; i < CAN_TEST_DLC; i++) {
    txf.data8[i] = data[i];
  }

  msg = canTransmitTimeout(&CAND1, CAN_ANY_MAILBOX, &txf,
                           TIME_MS2I(CAN_TEST_TX_TIMEOUT_MS));
  if (msg != MSG_OK) {
    chprintf(chp, "CAN: %s transmit FAILED (msg=%d)\r\n", name, (int)msg);
    /* Bring-up dump: the unit state at the timeout tells whether the
       frame was never handed to the bus (mailbox still pending), the
       unit is still in init mode or the transmission failed. */
    chprintf(chp,
             "CAN: ctlr=%08x statr=%08x tstatr=%08x errsr=%08x btimr=%08x "
             "txmi=%08x chip=%08x\r\n",
             (unsigned)CAN1->CTLR, (unsigned)CAN1->STATR,
             (unsigned)CAN1->TSTATR, (unsigned)CAN1->ERRSR,
             (unsigned)CAN1->BTIMR,
             (unsigned)CAN1->sTxMailBox[0].TXMIR,
             (unsigned)DBGMCU_GetCHIPID());
    return false;
  }
  can_tx_frames++;

  msg = canReceiveTimeout(&CAND1, mailbox, &rxf,
                          TIME_MS2I(CAN_TEST_RX_TIMEOUT_MS));
  if (msg != MSG_OK) {
    chprintf(chp, "CAN: %s receive FAILED (msg=%d, mailbox=%u)\r\n", name,
             (int)msg, (unsigned)mailbox);
    return false;
  }
  can_rx_frames++;

  if (((bool)rxf.IDE) != extended) {
    chprintf(chp, "CAN: %s wrong identifier type (IDE=%u)\r\n", name,
             (unsigned)rxf.IDE);
    return false;
  }
  if ((extended ? rxf.EID : rxf.SID) != id) {
    chprintf(chp, "CAN: %s wrong identifier (%08x vs %08x)\r\n", name,
             (unsigned)(extended ? rxf.EID : rxf.SID), (unsigned)id);
    return false;
  }
  if ((rxf.DLC != CAN_TEST_DLC) || (rxf.RTR != 0U)) {
    chprintf(chp, "CAN: %s wrong header (DLC=%u RTR=%u)\r\n", name,
             (unsigned)rxf.DLC, (unsigned)rxf.RTR);
    return false;
  }
  for (i = 0U; i < CAN_TEST_DLC; i++) {
    if (rxf.data8[i] != data[i]) {
      chprintf(chp, "CAN: %s payload MISMATCH at %u (%02x vs %02x)\r\n", name,
               (unsigned)i, (unsigned)rxf.data8[i], (unsigned)data[i]);
      return false;
    }
  }

  chprintf(chp, "CAN: %s PASS id=%08x dlc=%u:", name, (unsigned)id,
           (unsigned)rxf.DLC);
  can_print_data(chp, rxf.data8, CAN_TEST_DLC);
  chprintf(chp, "\r\n");

  return true;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waCanTest, 1024);

static THD_FUNCTION(can_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint32_t attempt = 0U;
  uint32_t baud;
  bool ok;

  (void)arg;

  can_pins_init();

  while (true) {
    attempt++;

    baud = ch32h417_hclk / ((uint32_t)can_config.prescaler * CAN_TEST_TQ);
    chprintf(chp, "CAN: attempt %u, %u baud, internal loopback\r\n",
             (unsigned)attempt, (unsigned)baud);

    /* The LLD asserts on a failed initialization, the return value is only
       informational here. */
    if (canStart(&CAND1, &can_config) != MSG_OK) {
      chprintf(chp, "CAN: driver start failed\r\n");
    }

    /* The filter banks are cleared by the driver initialization, they must
       be programmed after the start. */
    canWCHSetFilters(&CAND1, 2U, can_filters);

    ok = can_exchange(chp, "std", false, CAN_TEST_STD_ID, 1U);
    ok = can_exchange(chp, "ext", true, CAN_TEST_EXT_ID, 2U) && ok;

    if (!ok) {
      can_failures++;
      chprintf(chp, "CAN: FAIL, errors=0x%08x\r\n",
               (unsigned)CAND1.can->ERRSR);
    }

    canStop(&CAND1);

    chThdSleepMilliseconds(CAN_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the CAN1 loopback test thread.
 * @note    The CAN kernel clock is the HB1 clock on this platform, so the
 *          prescaler is derived from the live HCLK.
 */
void can_test_start(void) {
  uint32_t div;

  div = ch32h417_hclk / (CAN_TEST_BAUD * CAN_TEST_TQ);
  if (div < 1U) {
    div = 1U;
  }
  if (div > 1024U) {
    div = 1024U;
  }

  can_config.prescaler = (uint16_t)div;
  can_config.mode      = CAN_Mode_LoopBack;
  can_config.sjw       = CAN_SJW_1tq;
  can_config.bs1       = CAN_BS1_15tq;
  can_config.bs2       = CAN_BS2_4tq;
  can_config.abom      = true;
  can_config.awum      = false;
  can_config.nart      = true;

  chThdCreateStatic(waCanTest, sizeof(waCanTest), NORMALPRIO, can_test_thd,
                    NULL);
}