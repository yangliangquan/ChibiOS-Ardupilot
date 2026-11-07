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
 * @file    test_icu.c
 * @brief   CH32H417 V5F port check: input capture test thread.
 *
 * @details Captures the edges of the signal applied to the channel 1 of
 *          TIM3 and measures its period and its duty cycle. The edge
 *          alternation state machine of the driver needs both edges, the
 *          rising edge closes the period and the falling edge gives the
 *          width of the pulse.
 *
 *          The capture input is PA6, the signal is taken from a jumper
 *          connected to PA8, the frequency of the source must be low
 *          enough for the two edges to be separated by more than the
 *          time the driver spends in the interrupt.
 *
 *          The measurement is only meaningful when a signal is present,
 *          the module reports the values of the last capture and counts
 *          the rounds in which nothing was captured.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Capture input, the channel 1 of TIM3. */
#define ICU_TEST_PAD          6U

/* Frequency of the capture counter, it must divide the timer clock
   exactly. */
#define ICU_TEST_COUNTER_HZ   1000000U

/* Period of the measurement round. */
#define ICU_TEST_ROUND_MS     1000U

/* Minimum period accepted, a shorter one is a glitch of the input. */
#define ICU_TEST_MIN_PERIOD   10U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t icu_captures;
volatile uint32_t icu_failures;
volatile uint32_t icu_rounds;
volatile uint16_t icu_period;
volatile uint16_t icu_width;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Capture callback.
 * @details The driver calls it from the interrupt context, the values
 *          are only copied, the measurement is done by the thread.
 */
static void icu_capture_cb(ICUDriver *icup) {

  (void)icup;
  icu_captures++;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waIcuTest, 1024);

static THD_FUNCTION(icu_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  const ICUConfig icu_config = {
    .frequency = ICU_TEST_COUNTER_HZ,
    .mode = ICU_USE_ICU | ICU_USE_BOTH_EDGES | ICU_USE_PULSE_WIDTH,
    .callback = icu_capture_cb
  };
  uint32_t captures;

  (void)arg;

  /* The capture input is an alternate function input. */
  palSetPadMode(GPIOA, ICU_TEST_PAD, PAL_MODE_ALTERNATE_INPUT(2));

  if (icuStart(&ICUD3, &icu_config) != MSG_OK) {
    chprintf(chp, "ICU: driver start failed\r\n");
    return;
  }

  icuStartCapture(&ICUD3);

  chprintf(chp, "ICU: TIM3 channel 1 on PA%d, counter %u Hz\r\n",
           (unsigned)ICU_TEST_PAD, (unsigned)ICU_TEST_COUNTER_HZ);

  while (true) {
    captures = icu_captures;
    chThdSleepMilliseconds(ICU_TEST_ROUND_MS);

    icu_rounds++;

    if (icu_captures == captures) {
      /* Nothing was captured, the input is idle or the jumper is not
         connected. */
      icu_failures++;
      chprintf(chp, "ICU: no capture on PA%d\r\n", (unsigned)ICU_TEST_PAD);
      continue;
    }

    icu_period = (uint16_t)icuGetPeriodX(&ICUD3);
    icu_width = (uint16_t)icuGetWidthX(&ICUD3);

    if (icu_period < ICU_TEST_MIN_PERIOD) {
      icu_failures++;
      chprintf(chp, "ICU: period too short (%u)\r\n", (unsigned)icu_period);
      continue;
    }

    /* The width cannot be longer than the period, a longer one means
       that the two edges were not paired. */
    if (icu_width > icu_period) {
      icu_failures++;
      chprintf(chp, "ICU: width %u longer than period %u\r\n",
               (unsigned)icu_width, (unsigned)icu_period);
      continue;
    }

    chprintf(chp, "ICU: period %u ticks (%u us), width %u ticks (%u us), "
                  "%u captures\r\n",
             (unsigned)icu_period,
             (unsigned)((icu_period * 1000000U) / ICU_TEST_COUNTER_HZ),
             (unsigned)icu_width,
             (unsigned)((icu_width * 1000000U) / ICU_TEST_COUNTER_HZ),
             (unsigned)(icu_captures - captures));
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the ICU test thread.
 */
void icu_test_start(void) {

  chThdCreateStatic(waIcuTest, sizeof(waIcuTest), NORMALPRIO, icu_test_thd,
                    NULL);
}
