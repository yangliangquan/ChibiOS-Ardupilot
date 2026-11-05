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
 * @file    main.c
 * @brief   CH32H417 V5F port check: boot and module startup.
 *
 * @details The validations live in one module each, this file only
 *          starts them:
 *          - test_dma.c      : boot DMA smoke test, result in
 *                              dma_selftest_result (1 = pass,
 *                              2 = channel allocation failure,
 *                              3 = data mismatch, 4 = timeout)
 *          - test_counters.c : counter1/counter2, per-thread loop
 *                              counters (both must advance, no starvation)
 *          - test_fp.c       : fcounter1/2, independent FP accumulators,
 *                              they diverge from each other only if the FP
 *                              context is switched
 *          - test_console.c  : SD3 console, 1s status line (SERIAL)
 *          - test_sdc.c      : SDIO card read/write test every 5s
 *          - test_wspi.c     : QSPI2 flash erase/program/read validation
 *                              every 5s on the last 4KB scratch sector
 *
 *          Observation points of the debugger: counter1, counter2,
 *          fcounter1, fcounter2, systime_snapshot, main_loops and
 *          dma_selftest_result. The LED on PD11 blinks with a 500ms
 *          period (PAL).
 *
 * @addtogroup RT
 * @{
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "hal.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define CONSOLE_SPEED 115200

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const SerialConfig serial_config = {.speed = CONSOLE_SPEED};

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t systime_snapshot;
volatile uint32_t main_loops;

/*===========================================================================*/
/* Module main code.                                                         */
/*===========================================================================*/

int main(void) {

  halInit();
  chSysInit();

  sdStart(&SD3, &serial_config);
  chprintf(PORTCheck_CONSOLE, "CH32H417 V5F ChibiOS boot OK\r\n");

  /* The modules below are the ones enabled for this board, the others
     are kept out of the boot on purpose. */
  // dma_test_run();
  // chprintf(PORTCheck_CONSOLE, "dma selftest=%u (1=pass)\r\n",
  //          (unsigned)dma_selftest_result);

  counters_test_start();
  fp_test_start();
  // sdc_test_start();
  wspi_test_start();
  // console_test_start();

  while (true) {
    LED_TOGGLE();
    // systime_snapshot = chVTGetSystemTime();
    // main_loops++;
    chThdSleepMilliseconds(500);
  }
}

/** @} */
