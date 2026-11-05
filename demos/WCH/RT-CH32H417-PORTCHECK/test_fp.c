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
 * @file    test_fp.c
 * @brief   CH32H417 V5F port check: floating point context threads.
 *
 * @details Two independent FP accumulators. If the FP context is not
 *          switched correctly each thread observes the other one's
 *          values.
 */

#include "board.h"
#include "ch.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile float fcounter1;
volatile float fcounter2;

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waFp1, 256);
static THD_WORKING_AREA(waFp2, 256);

static THD_FUNCTION(fp_thread_fn, arg) {
  volatile float *fp = (volatile float *)arg;

  while (true) {
    *fp = (*fp) * 1.0001f + 1.0f;
    chThdSleepMilliseconds(2);
  }
}

static THD_FUNCTION(fp2_thread_fn, arg) {
  volatile float *fp = (volatile float *)arg;

  while (true) {
    *fp = (*fp) * 0.9999f + 2.0f;
    chThdSleepMilliseconds(3);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the floating point context threads.
 */
void fp_test_start(void) {

  chThdCreateStatic(waFp1, sizeof(waFp1), NORMALPRIO, fp_thread_fn,
                    (void *)&fcounter1);
  chThdCreateStatic(waFp2, sizeof(waFp2), NORMALPRIO, fp2_thread_fn,
                    (void *)&fcounter2);
}
