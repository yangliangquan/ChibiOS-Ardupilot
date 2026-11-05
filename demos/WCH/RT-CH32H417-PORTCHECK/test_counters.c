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
 * @file    test_counters.c
 * @brief   CH32H417 V5F port check: loop counter threads.
 *
 * @details Two threads increment their own counter every millisecond,
 *          both must advance, a starved thread shows up immediately.
 */

#include "board.h"
#include "ch.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t counter1;
volatile uint32_t counter2;

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waThread1, 256);
static THD_WORKING_AREA(waThread2, 256);

static THD_FUNCTION(counter_thread_fn, arg) {
  volatile uint32_t *cp = (volatile uint32_t *)arg;

  while (true) {
    (*cp)++;
    chThdSleepMilliseconds(1);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the counter threads.
 */
void counters_test_start(void) {

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO,
                    counter_thread_fn, (void *)&counter1);
  chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO,
                    counter_thread_fn, (void *)&counter2);
}
