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
 * @file    test_trng.c
 * @brief   CH32H417 V5F port check: TRNG test thread.
 *
 * @details Draws random words from the hardware generator and checks
 *          that they are not a constant or all zero, a generator that
 *          returns a fixed pattern is broken.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define TRNG_TEST_RETRY_MS 2000U
#define TRNG_TEST_SIZE 32U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t trng_generations;
volatile uint32_t trng_failures;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const TRNGConfig trng_config = {0};

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waTrngTest, 512);

static THD_FUNCTION(trng_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint8_t buf[TRNG_TEST_SIZE];
  unsigned i;
  unsigned nonzero;
  unsigned changes;

  (void)arg;

  if (trngStart(&TRNGD1, &trng_config) != MSG_OK) {
    chprintf(chp, "TRNG: driver start failed\r\n");
    return;
  }

  while (true) {
    if (trngGenerate(&TRNGD1, TRNG_TEST_SIZE, buf)) {
      trng_failures++;
      chprintf(chp, "TRNG: generation FAILED\r\n");
    }
    else {
      trng_generations++;

      nonzero = 0U;
      changes = 0U;
      for (i = 0U; i < TRNG_TEST_SIZE; i++) {
        if (buf[i] != 0U) {
          nonzero++;
        }
        if ((i > 0U) && (buf[i] != buf[i - 1U])) {
          changes++;
        }
      }

      chprintf(chp, "TRNG: %02x %02x %02x %02x %02x %02x %02x %02x "
                    "(nonzero=%u/%u changes=%u)\r\n",
               (unsigned)buf[0], (unsigned)buf[1], (unsigned)buf[2],
               (unsigned)buf[3], (unsigned)buf[4], (unsigned)buf[5],
               (unsigned)buf[6], (unsigned)buf[7],
               (unsigned)nonzero, (unsigned)TRNG_TEST_SIZE,
               (unsigned)changes);

      if ((nonzero == 0U) || (changes == 0U)) {
        trng_failures++;
        chprintf(chp, "TRNG: degenerate output\r\n");
      }
    }

    chThdSleepMilliseconds(TRNG_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the TRNG test thread.
 */
void trng_test_start(void) {

  chThdCreateStatic(waTrngTest, sizeof(waTrngTest), NORMALPRIO, trng_test_thd,
                    NULL);
}
