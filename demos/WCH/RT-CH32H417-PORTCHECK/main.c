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
 * @brief   CH32H417 V5F port check: HAL + tick + threads + console.
 *
 * @details Observation points for the combined on-target validation:
 *          - counter1/counter2 : per-thread loop counters (both must
 *            advance, no starvation)
 *          - systime_snapshot  : system time, written every main loop
 *          - main_loops        : main thread loop count
 *          - LED on PC0        : visible blink (PAL), 500ms period
 *          - SD1 console       : 1s status line (SERIAL)
 *
 * @addtogroup RT
 * @{
 */

#include "ch.h"
#include "hal.h"
#include "board.h"
#include "chprintf.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define CONSOLE_SPEED                       115200

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const SerialConfig serial_config = {
  .speed = CONSOLE_SPEED
};

static volatile uint32_t counter1;
static volatile uint32_t counter2;

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t main_loops;
volatile uint32_t systime_snapshot;

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waThread1, 256);
static THD_WORKING_AREA(waThread2, 256);
static THD_WORKING_AREA(waConsole, 512);

static THD_FUNCTION(thread_fn, arg) {
  volatile uint32_t *cp = (volatile uint32_t *)arg;

  while (true) {
    (*cp)++;
    chThdSleepMilliseconds(1);
  }
}

static THD_FUNCTION(console_thd, arg) {
  BaseSequentialStream *chp = (BaseSequentialStream *)&SD1;

  (void)arg;

  while (true) {
    chprintf(chp,
             "systime=%u loops=%u c1=%u c2=%u\r\n",
             (unsigned)chVTGetSystemTime(),
             (unsigned)main_loops,
             (unsigned)counter1,
             (unsigned)counter2);
    chThdSleepMilliseconds(1000);
  }
}

/*===========================================================================*/
/* Module main code.                                                         */
/*===========================================================================*/

int main(void) {

  halInit();
  chSysInit();

  sdStart(&SD1, &serial_config);
  chprintf((BaseSequentialStream *)&SD1,
           "CH32H417 V5F ChibiOS boot OK\r\n");

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO,
                    thread_fn, (void *)&counter1);
  chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO,
                    thread_fn, (void *)&counter2);
  chThdCreateStatic(waConsole, sizeof(waConsole), NORMALPRIO + 1,
                    console_thd, NULL);

  while (true) {
    LED_TOGGLE();
    systime_snapshot = chVTGetSystemTime();
    main_loops++;
    chThdSleepMilliseconds(500);
  }
}

/** @} */
