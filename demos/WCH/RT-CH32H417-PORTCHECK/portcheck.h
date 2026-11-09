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
extern volatile uint32_t adc_conversions;
extern volatile uint32_t adc_failures;
extern volatile uint32_t can_tx_frames;
extern volatile uint32_t can_rx_frames;
extern volatile uint32_t can_failures;
extern volatile uint32_t usb_events;
extern volatile uint32_t usb_tx_frames;
extern volatile uint32_t usb_rx_frames;
extern volatile uint32_t usb_ctl_flags;
extern volatile uint32_t trng_generations;
extern volatile uint32_t trng_failures;
extern volatile uint32_t dac_samples;
extern volatile uint32_t dac_failures;
extern volatile uint32_t dac_sine_cycles;
extern volatile uint16_t dac_last_ch1;
extern volatile uint16_t dac_last_ch2;
extern volatile uint32_t dac_last_mv;
extern volatile uint32_t pwm_periods;
extern volatile uint32_t pwm_steps;
extern volatile uint32_t pwm_failures;
extern volatile uint32_t pwm_duty;
extern volatile uint32_t pwm_frequency;
extern volatile uint32_t icu_captures;
extern volatile uint32_t icu_failures;
extern volatile uint32_t icu_rounds;
extern volatile uint16_t icu_period;
extern volatile uint16_t icu_width;
extern volatile uint32_t rtc_seconds;
extern volatile uint32_t rtc_alarms;
extern volatile uint32_t rtc_overflows;
extern volatile uint32_t rtc_checks;
extern volatile uint32_t rtc_failures;
extern volatile uint32_t rtc_last_epoch;
extern volatile uint32_t rtc_rate_mhz;
extern volatile uint32_t spi_transfers;
extern volatile uint32_t spi_failures;
extern volatile uint32_t spi_rounds;
extern volatile uint32_t spi_last_error;
extern volatile uint32_t cry_blocks;
extern volatile uint32_t cry_failures;
extern volatile uint32_t cry_rounds;
extern volatile uint32_t cry_last_error;
extern volatile uint32_t exti_edges;
extern volatile uint32_t exti_rounds;
extern volatile uint32_t exti_failures;

/*===========================================================================*/
/* Module entry points.                                                       */
/*===========================================================================*/

void counters_test_start(void);
void fp_test_start(void);
void console_test_start(void);
void sdc_test_start(void);
void wspi_test_start(void);
void adc_test_start(void);
void can_test_start(void);
void trng_test_start(void);
void usbfs_test_start(void);
void dac_test_start(void);
void pwm_test_start(void);
void icu_test_start(void);
void rtc_test_start(void);
void spi_test_start(void);
void ecdc_test_start(void);
void exti_test_start(void);
void dma_test_run(void);

#endif /* PORTCHECK_H */
