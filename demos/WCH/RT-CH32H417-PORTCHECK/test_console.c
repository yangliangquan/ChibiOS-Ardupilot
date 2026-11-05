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
 * @file    test_console.c
 * @brief   CH32H417 V5F port check: status line thread.
 *
 * @details Prints the state of the observation points once per second,
 *          it is the visible side of the threads and thread switching
 *          checks.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waConsole, 512);

static THD_FUNCTION(console_thd, arg) {

  (void)arg;

  while (true) {
    chprintf(PORTCheck_CONSOLE,
             "systime=%u loops=%u c1=%u c2=%u f1=%u f2=%u\r\n",
             (unsigned)chVTGetSystemTime(), (unsigned)main_loops,
             (unsigned)counter1, (unsigned)counter2, (unsigned)fcounter1,
             (unsigned)fcounter2);
    chThdSleepMilliseconds(1000);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the status line thread.
 */
void console_test_start(void) {

  chThdCreateStatic(waConsole, sizeof(waConsole), NORMALPRIO + 1, console_thd,
                    NULL);
}
