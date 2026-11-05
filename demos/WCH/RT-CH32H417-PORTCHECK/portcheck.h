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
 * @file    portcheck.h
 * @brief   CH32H417 V5F port check: shared declarations.
 *
 * @details The port check is split in one module per validation, this
 *          header holds what they share: the console stream, the
 *          observation points read by the debugger and the entry points
 *          called by the boot code.
 */

#ifndef PORTCHECK_H
#define PORTCHECK_H

#include "ch.h"
#include "hal.h"

/* Console shared by all the test modules. */
#define PORTCheck_CONSOLE       ((BaseSequentialStream *)&SD3)

/*===========================================================================*/
/* Observation points.                                                        */
/*===========================================================================*/

extern volatile uint32_t counter1;
extern volatile uint32_t counter2;
extern volatile float fcounter1;
extern volatile float fcounter2;
extern volatile uint32_t systime_snapshot;
extern volatile uint32_t main_loops;
extern volatile uint32_t dma_selftest_result;

/*===========================================================================*/
/* Module entry points.                                                       */
/*===========================================================================*/

void counters_test_start(void);
void fp_test_start(void);
void console_test_start(void);
void sdc_test_start(void);
void wspi_test_start(void);
void dma_test_run(void);

#endif /* PORTCHECK_H */
