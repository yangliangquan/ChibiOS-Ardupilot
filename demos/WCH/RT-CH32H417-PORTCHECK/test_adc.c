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
 * @file    test_adc.c
 * @brief   CH32H417 V5F port check: ADC test thread.
 *
 * @details Converts the internal reference and the internal temperature
 *          sensor of ADC1, the two channels that need no wiring, and
 *          derives the die temperature from them. One external channel is
 *          converted as well, it shows that the analog input path works
 *          but its value depends on the wiring of the board.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

#include "ch32h417_adc.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define ADC_TEST_RETRY_MS 1000U

/* Buffer depth, it must be one or an even number of sequences. */
#define ADC_TEST_DEPTH 4U

/* Channels of the conversion group. */
#define ADC_TEST_NUM_CHANNELS 3U
#define ADC_TEST_EXT_CHANNEL  ADC_Channel_1
#define ADC_TEST_EXT_PIN      1U

/* Nominal voltage of the internal reference, the VREFINT channel reads
   it back and the reference voltage is derived from that reading. */
#define ADC_TEST_VREFINT_MV 1200U

/* Full scale of the 12 bit converter. */
#define ADC_TEST_FULL_SCALE 4095U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t adc_conversions;
volatile uint32_t adc_failures;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const ADCConfig adc_config = {.dummy = 0};

static const uint8_t adc_channels[ADC_TEST_NUM_CHANNELS] = {
  ADC_Channel_Vrefint,
  ADC_Channel_TempSensor,
  ADC_TEST_EXT_CHANNEL
};

static const ADCConversionGroup adc_group = {
  .circular = FALSE,
  .num_channels = ADC_TEST_NUM_CHANNELS,
  .end_cb = NULL,
  .error_cb = NULL,
  .channels = adc_channels,
  .sample_time = ADC_SampleTime_CyclesMode5
};

/* The samples are a matrix, one row per converted sequence. */
static adcsample_t adc_samples[ADC_TEST_NUM_CHANNELS * ADC_TEST_DEPTH];

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waAdcTest, 1024);

static THD_FUNCTION(adc_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  adcsample_t vrefint;
  adcsample_t tempsensor;
  adcsample_t external;
  adcsample_t ts_min;
  adcsample_t ts_max;
  uint32_t ts_sum;
  uint32_t vref_mv;
  uint32_t ts_mv;
  s32 temp_deci;
  msg_t msg;
  unsigned i;

  (void)arg;

  /* The external channel needs its pin as analog input. */
  palSetPadMode(GPIOA, ADC_TEST_EXT_PIN, PAL_MODE_INPUT_ANALOG);

  if (adcStart(&ADCD1, &adc_config) != MSG_OK) {
    chprintf(chp, "ADC: driver start failed\r\n");
    return;
  }

  /* Internal reference and temperature sensor, they live in ADC1. */
  ADC_TempSensorVrefintCmd(ENABLE);

  /* The internal reference needs a settling time after it is enabled. */
  chThdSleepMilliseconds(10U);

  while (true) {
    msg = adcConvert(&ADCD1, &adc_group, adc_samples, ADC_TEST_DEPTH);
    if (msg != MSG_OK) {
      adc_failures++;
      chprintf(chp, "ADC: conversion failed (msg=%d)\r\n", (int)msg);
      chThdSleepMilliseconds(ADC_TEST_RETRY_MS);
      continue;
    }

    adc_conversions++;

    /* First row of the matrix, the other rows are the repetitions. */
    vrefint = adc_samples[0];
    tempsensor = adc_samples[1];
    external = adc_samples[2];

    ts_min = tempsensor;
    ts_max = tempsensor;
    ts_sum = tempsensor;
    for (i = 1U; i < ADC_TEST_DEPTH; i++) {
      adcsample_t sample = adc_samples[i * ADC_TEST_NUM_CHANNELS + 1U];

      ts_sum += sample;
      if (sample < ts_min) {
        ts_min = sample;
      }
      if (sample > ts_max) {
        ts_max = sample;
      }
    }

    if ((vrefint == 0U) || (vrefint >= ADC_TEST_FULL_SCALE)) {
      adc_failures++;
      chprintf(chp, "ADC: internal reference invalid (%u), raw: %u %u %u "
                    "(vrefint ts ext)\r\n",
               (unsigned)vrefint, (unsigned)vrefint, (unsigned)tempsensor,
               (unsigned)external);
      chThdSleepMilliseconds(ADC_TEST_RETRY_MS);
      continue;
    }

    /* Reference voltage from the internal reference, then the sensor
       voltage from the die temperature channel. */
    vref_mv = (ADC_TEST_VREFINT_MV * ADC_TEST_FULL_SCALE) / vrefint;
    ts_mv = ((ts_sum / ADC_TEST_DEPTH) * vref_mv) / ADC_TEST_FULL_SCALE;
    temp_deci = TempSensor_Volt_To_Temper((s32)ts_mv);

    chprintf(chp, "ADC: vrefint=%u (vref=%umV) ts=%u..%u (%umV) ext=%u "
                  "temp=%d.%dC\r\n",
             (unsigned)vrefint, (unsigned)vref_mv,
             (unsigned)ts_min, (unsigned)ts_max, (unsigned)ts_mv,
             (unsigned)external, (int)(temp_deci / 10),
             (int)(temp_deci % 10));

    chThdSleepMilliseconds(ADC_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the ADC test thread.
 */
void adc_test_start(void) {

  chThdCreateStatic(waAdcTest, sizeof(waAdcTest), NORMALPRIO, adc_test_thd,
                    NULL);
}
