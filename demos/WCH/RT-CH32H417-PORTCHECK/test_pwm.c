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
 * @file    test_pwm.c
 * @brief   CH32H417 V5F port check: PWM test thread.
 *
 * @details Drives the PWM outputs of the two timers that the board
 *          enables:
 *          - TIM1 channel 2 on PE11 (AF1) and channel 3 on PE13 (AF1),
 *          - TIM4 channel 1 on PD12 (AF2) and channel 2 on PD13 (AF2).
 *          The duty cycle of the four channels is swept step by step
 *          and the counters of the outputs are kept in the observation
 *          points, so that the sweep can be followed from the
 *          debugger.
 *
 *          The periodic notification of the timer is enabled as well,
 *          it counts the periods and gives the frequency that the
 *          hardware actually produced, the nominal one is only a
 *          configuration value.
 *
 *          The pins and the alternate functions are the ones of the
 *          CH32H417 pin table, they are not used by any other module of
 *          the port check (PE9 is TIM1_CH1, not CH3, and it belongs to
 *          the SDIO pins of the SDC test).
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Frequency of the timer counter, it must divide the timer clock
   exactly, the driver refuses a rounded value. */
#define PWM_TEST_COUNTER_HZ    10000000U

/* Number of ticks of one PWM period, the output frequency is the
   counter frequency divided by this value. */
#define PWM_TEST_PERIOD        100U

/* Time between two duty cycle steps. */
#define PWM_TEST_STEP_MS       1000U

/* Duty cycle steps, the duty is in percent of the period. */
#define PWM_TEST_DUTY_STEP     10U

/* Channels of the two timers used by the module. */
#define PWM_TEST_TIM1_CH2      1U
#define PWM_TEST_TIM1_CH3      2U
#define PWM_TEST_TIM4_CH1      0U
#define PWM_TEST_TIM4_CH2      1U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t pwm_periods;
volatile uint32_t pwm_steps;
volatile uint32_t pwm_failures;
volatile uint32_t pwm_duty;
volatile uint32_t pwm_frequency;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Periodic notification of the PWM timer.
 * @details Counts the periods of the hardware, the frequency is
 *          derived from it by the test thread.
 */
static void pwm_periodic_cb(PWMDriver *pwmp) {

  (void)pwmp;
  pwm_periods++;
}

/* The channels are configured for the active high output, the driver
   selects the mode of the compare register from it. */
static const PWMConfig pwm_config = {
  .frequency = PWM_TEST_COUNTER_HZ,
  .period = PWM_TEST_PERIOD,
  .callback = pwm_periodic_cb,
  .channels = {
    {.mode = PWM_OUTPUT_ACTIVE_HIGH},
    {.mode = PWM_OUTPUT_ACTIVE_HIGH},
    {.mode = PWM_OUTPUT_ACTIVE_HIGH},
    {.mode = PWM_OUTPUT_ACTIVE_HIGH}
  }
};

/**
 * @brief   Configures the pins of the two timers.
 */
static void pwm_pins_setup(void) {

  /* TIM1 channel 2 is PE11, channel 3 is PE13, both on the alternate
     function 1 (PE9 is TIM1_CH1 and it is part of the SDIO pin set). */
  palSetPadMode(GPIOE, 11U, PAL_MODE_ALTERNATE(1));
  palSetPadMode(GPIOE, 13U, PAL_MODE_ALTERNATE(1));
  /* TIM4 channel 1 is PD12, channel 2 is PD13, both on the alternate
     function 2. */
  palSetPadMode(GPIOD, 12U, PAL_MODE_ALTERNATE(2));
  palSetPadMode(GPIOD, 13U, PAL_MODE_ALTERNATE(2));
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waPwmTest, 1024);

static THD_FUNCTION(pwm_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  pwmcnt_t width;
  uint32_t duty;
  uint32_t periods;
  systime_t start;

  (void)arg;

  pwm_pins_setup();

  if (pwmStart(&PWMD1, &pwm_config) != MSG_OK) {
    chprintf(chp, "PWM: TIM1 start failed\r\n");
    return;
  }
  if (pwmStart(&PWMD4, &pwm_config) != MSG_OK) {
    chprintf(chp, "PWM: TIM4 start failed\r\n");
    return;
  }

  /* The periodic notification gives the frequency that the hardware
     produces, the configuration only states the requested one. */
  pwmEnablePeriodicNotification(&PWMD1);

  chprintf(chp, "PWM: TIM1 ch2=PE11 ch3=PE13, TIM4 ch1=PD12 ch2=PD13, "
                "counter %u Hz, period %u ticks\r\n",
           (unsigned)PWM_TEST_COUNTER_HZ, (unsigned)PWM_TEST_PERIOD);

  /* All the channels start with a null pulse width, the output stays
     low until the duty cycle is programmed. */
  pwmEnableChannel(&PWMD1, PWM_TEST_TIM1_CH2, 0U);
  pwmEnableChannel(&PWMD1, PWM_TEST_TIM1_CH3, 0U);
  pwmEnableChannel(&PWMD4, PWM_TEST_TIM4_CH1, 0U);
  pwmEnableChannel(&PWMD4, PWM_TEST_TIM4_CH2, 0U);

  while (true) {
    for (duty = 0U; duty <= 100U; duty += PWM_TEST_DUTY_STEP) {
      /* The pulse width is the duty cycle of the period. */
      width = (pwmcnt_t)((PWM_TEST_PERIOD * duty) / 100U);

      pwmEnableChannel(&PWMD1, PWM_TEST_TIM1_CH2, width);
      pwmEnableChannel(&PWMD1, PWM_TEST_TIM1_CH3,
                       (pwmcnt_t)(PWM_TEST_PERIOD - width));
      pwmEnableChannel(&PWMD4, PWM_TEST_TIM4_CH1, width);
      pwmEnableChannel(&PWMD4, PWM_TEST_TIM4_CH2,
                       (pwmcnt_t)(PWM_TEST_PERIOD - width));

      pwm_duty = duty;
      pwm_steps++;

      /* The period counter is sampled over the step, the frequency
         follows from the number of periods and the elapsed time. */
      periods = pwm_periods;
      start = chVTGetSystemTimeX();
      chThdSleepMilliseconds(PWM_TEST_STEP_MS);
      {
        sysinterval_t elapsed = chVTTimeElapsedSinceX(start);
        uint32_t counted = pwm_periods - periods;

        if (elapsed > 0U) {
          pwm_frequency = (uint32_t)(((uint64_t)counted * 1000U) / elapsed);
        }
      }

      chprintf(chp, "PWM: duty %u%% width %u, measured %u Hz\r\n",
               (unsigned)duty, (unsigned)width, (unsigned)pwm_frequency);
    }
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the PWM test thread.
 */
void pwm_test_start(void) {

  chThdCreateStatic(waPwmTest, sizeof(waPwmTest), NORMALPRIO, pwm_test_thd,
                    NULL);
}
