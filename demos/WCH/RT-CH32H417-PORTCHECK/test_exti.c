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
    along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file    test_exti.c
 * @brief   CH32H417 V5F port check: EXTI test thread.
 *
 * @details EXTI line 0 is taken from PC0. The pad is driven as a push
 *          pull output by the test itself and toggled once per
 *          millisecond, the line is armed on both edges so every
 *          toggle must generate one interrupt.
 *
 *          Observation points: exti_edges, exti_rounds and
 *          exti_failures.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* EXTI source port and pad. */
#define EXTI_TEST_PORT          GPIOC
#define EXTI_TEST_PAD           0U

/* Toggles used to verify that a disabled pad event stops callbacks. */
#define EXTI_TEST_DISABLE_PULSES 4U

/* Toggles in one round, one interrupt is expected for each one. */
#define EXTI_TEST_PULSES        100U

/* Settle time before the edge count is checked. */
#define EXTI_TEST_SETTLE_MS     5U

/* Time between two rounds. */
#define EXTI_TEST_ROUND_MS      2000U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t exti_edges;
volatile uint32_t exti_rounds;
volatile uint32_t exti_failures;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static void exti_test_callback(void *arg) {

  (void)arg;
  exti_edges++;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waExtiTest, 1024);

static THD_FUNCTION(exti_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint32_t i;
  uint32_t before;
  uint32_t delta;

  (void)arg;

  palSetPadMode(EXTI_TEST_PORT, EXTI_TEST_PAD, PAL_MODE_OUTPUT_PUSHPULL);
  palClearPad(EXTI_TEST_PORT, EXTI_TEST_PAD);
  palSetPadCallback(EXTI_TEST_PORT, EXTI_TEST_PAD, exti_test_callback, NULL);
  palEnablePadEvent(EXTI_TEST_PORT, EXTI_TEST_PAD,
                    PAL_EVENT_MODE_BOTH_EDGES);

  chprintf(chp, "EXTI: PAL callback, line %u on PC%u, both edges\r\n",
           (unsigned)EXTI_TEST_PAD, (unsigned)EXTI_TEST_PAD);

  /* Verifies that disabling the pad event stops the callbacks.*/
  palDisablePadEvent(EXTI_TEST_PORT, EXTI_TEST_PAD);
  before = exti_edges;
  for (i = 0U; i < EXTI_TEST_DISABLE_PULSES; i++) {
    palTogglePad(EXTI_TEST_PORT, EXTI_TEST_PAD);
    chThdSleepMilliseconds(1);
  }
  if (exti_edges == before) {
    chprintf(chp, "EXTI: disable PASS\r\n");
  }
  else {
    exti_failures++;
    chprintf(chp, "EXTI: disable FAIL (%u edges)\r\n",
             (unsigned)(exti_edges - before));
  }
  palSetPadCallback(EXTI_TEST_PORT, EXTI_TEST_PAD, exti_test_callback, NULL);
  palEnablePadEvent(EXTI_TEST_PORT, EXTI_TEST_PAD,
                    PAL_EVENT_MODE_BOTH_EDGES);

  while (true) {
    before = exti_edges;

    for (i = 0U; i < EXTI_TEST_PULSES; i++) {
      palTogglePad(EXTI_TEST_PORT, EXTI_TEST_PAD);
      chThdSleepMilliseconds(1);
    }
    chThdSleepMilliseconds(EXTI_TEST_SETTLE_MS);

    delta = exti_edges - before;
    exti_rounds++;

    if (delta == EXTI_TEST_PULSES) {
      chprintf(chp, "EXTI: %u/%u edges PASS (total %u failures %u)\r\n",
               (unsigned)delta, (unsigned)EXTI_TEST_PULSES,
               (unsigned)exti_edges, (unsigned)exti_failures);
    }
    else {
      exti_failures++;
      chprintf(chp, "EXTI: %u/%u edges FAIL (total %u failures %u)\r\n",
               (unsigned)delta, (unsigned)EXTI_TEST_PULSES,
               (unsigned)exti_edges, (unsigned)exti_failures);
    }

    chThdSleepMilliseconds(EXTI_TEST_ROUND_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the EXTI test thread.
 */
void exti_test_start(void) {

  chThdCreateStatic(waExtiTest, sizeof(waExtiTest), NORMALPRIO, exti_test_thd,
                    NULL);
}
