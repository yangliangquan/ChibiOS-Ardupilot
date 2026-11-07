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
 * @file    test_dac.c
 * @brief   CH32H417 V5F port check: DAC test thread.
 *
 * @details Validates the DAC unit in two steps:
 *          - direct output: a staircase of values is written to channel 1
 *            (PA4) and channel 2 (PA5) with the software trigger, the
 *            holding register and the output register are read back to
 *            check that the converter accepted the data.
 *          - buffered output: both channels are driven by the DMA from a
 *            sine wave buffer, the conversions are paced by the TIM6 TRGO
 *            selected in the conversion group. The hardware trigger is
 *            mandatory: the DAC issues a DMA request on each trigger
 *            event only, without it the DMA transfers nothing.
 *
 *          The DAC pins must be in analog mode before the unit is
 *          enabled, otherwise the digital input stage keeps the pin
 *          from settling to the analog voltage (RM ch18).
 *
 *          The expected output voltage is VDD33A * (value / 4096), the
 *          value of the last written sample is left in the observation
 *          points for the debugger.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

#include "ch32h417_dac.h"
#include "ch32h417_rcc.h"
#include "ch32h417_tim.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Period of the staircase, one value per step. */
#define DAC_TEST_STEP_MS        500U

/* Period of the sine wave buffer, the DMA replays it continuously. */
#define DAC_TEST_SINE_PERIOD_MS 1000U

/* Number of samples of the sine wave buffer, it must be even. */
#define DAC_TEST_SINE_DEPTH     64U

/* Rate of the conversion trigger, one sample per trigger event. With 64
   samples per period the sine frequency is this value divided by 64. */
#define DAC_TEST_SINE_RATE_HZ   64000U

/* Channels of the DAC unit: 0 = OUT1 (PA4), 1 = OUT2 (PA5). */
#define DAC_TEST_CHANNEL_1      0U
#define DAC_TEST_CHANNEL_2      1U

/* Full scale of the 12 bit converter. */
#define DAC_TEST_FULL_SCALE     4095U

/* Nominal analog supply of the DAC unit, used to print the expected
   output voltage of a sample. */
#define DAC_TEST_VDD33A_MV      3300U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t dac_samples;
volatile uint32_t dac_failures;
volatile uint32_t dac_sine_cycles;
volatile dacsample_t dac_last_ch1;
volatile dacsample_t dac_last_ch2;
volatile uint32_t dac_last_mv;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/* The DAC unit of this port has no configuration fields. */
static const DACConfig dac_config;

/* Channels of the conversion group: channel 1 and channel 2. */
static const dacchannel_t dac_channels[2] = {
  DAC_TEST_CHANNEL_1,
  DAC_TEST_CHANNEL_2
};

static const DACConversionGroup dac_group = {
  .num_channels = 2U,
  .end_cb = NULL,
  .error_cb = NULL,
  .channels = dac_channels,
  .trigger = DAC_Trigger_T6_TRGO
};

/* The samples are a matrix, one row per converted sequence, the two
   channels of a row are consecutive. */
static dacsample_t dac_samples_buffer[2U * DAC_TEST_SINE_DEPTH];

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Sine of an angle given in degrees, scaled by 1000.
 * @details The table covers one quadrant, the other ones are obtained by
 *          mirroring it, the result is an integer so that no math
 *          library is needed.
 *
 * @param[in] angle     angle in degrees, 0..359
 *
 * @return              the sine, scaled by 1000
 */
static int32_t sine_approx(uint32_t angle) {
  /* Sine of 0, 10, 20 ... 90 degrees, scaled by 1000. */
  static const int32_t quadrant[10] = {
    0, 174, 342, 500, 643, 766, 866, 940, 985, 1000
  };
  uint32_t index;
  uint32_t rest;

  angle %= 360U;

  if (angle < 90U) {
    index = angle / 10U;
    rest = angle % 10U;
    return quadrant[index] + ((quadrant[index + 1U] - quadrant[index]) * (int32_t)rest) / 10;
  }
  if (angle < 180U) {
    angle = 180U - angle;
    index = angle / 10U;
    rest = angle % 10U;
    return quadrant[index] + ((quadrant[index + 1U] - quadrant[index]) * (int32_t)rest) / 10;
  }
  if (angle < 270U) {
    angle -= 180U;
    index = angle / 10U;
    rest = angle % 10U;
    return -(quadrant[index] + ((quadrant[index + 1U] - quadrant[index]) * (int32_t)rest) / 10);
  }
  angle = 360U - angle;
  index = angle / 10U;
  rest = angle % 10U;
  return -(quadrant[index] + ((quadrant[index + 1U] - quadrant[index]) * (int32_t)rest) / 10);
}

/**
 * @brief   Builds one period of a sine wave on both channels.
 * @details The two channels are in quadrature, the second one is shifted
 *          by a quarter of a period, so that a scope shows the phase
 *          relation between the two outputs.
 */
static void dac_build_sine(void) {
  int32_t amplitude = (int32_t)(DAC_TEST_FULL_SCALE / 4U);
  uint32_t i;
  uint32_t phase;

  for (i = 0U; i < DAC_TEST_SINE_DEPTH; i++) {
    /* The sine is scaled by 1000, the amplitude is a quarter of the full
       scale and the offset is the mid scale. The amplitude factor is
       signed: with the unsigned constant the negative half periods would
       wrap around and the samples would be truncated to garbage. */
    phase = (i * 360U) / DAC_TEST_SINE_DEPTH;
    dac_samples_buffer[2U * i] =
        (dacsample_t)((int32_t)(DAC_TEST_FULL_SCALE / 2U) +
                      (amplitude * sine_approx(phase)) / 1000);
    dac_samples_buffer[2U * i + 1U] =
        (dacsample_t)((int32_t)(DAC_TEST_FULL_SCALE / 2U) +
                      (amplitude * sine_approx((phase + 90U) % 360U)) / 1000);
  }
}

/**
 * @brief   Starts the TIM6 update event used as the DAC trigger.
 * @details The DAC issues a DMA request on each trigger event only, so a
 *          hardware trigger is required by the buffered output. TIM6 runs
 *          free, its TRGO is the update event, the trigger is only used
 *          by the groups selecting DAC_Trigger_T6_TRGO.
 */
static void dac_trigger_timer_start(void) {
  TIM_TimeBaseInitTypeDef tim;

  RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM6, ENABLE);

  TIM_TimeBaseStructInit(&tim);
  tim.TIM_Prescaler = 0U;
  tim.TIM_CounterMode = TIM_CounterMode_Up;
  tim.TIM_Period = (uint16_t)((CH32H417_TIMCLK1 / DAC_TEST_SINE_RATE_HZ) - 1U);
  tim.TIM_ClockDivision = TIM_CKD_DIV1;
  tim.TIM_RepetitionCounter = 0U;
  TIM_TimeBaseInit(TIM6, &tim);

  TIM_SelectOutputTrigger(TIM6, TIM_TRGOSource_Update);
  TIM_Cmd(TIM6, ENABLE);
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waDacTest, 1024);

static THD_FUNCTION(dac_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  dacsample_t value;
  dacsample_t step;
  msg_t msg;

  (void)arg;

  /* The DAC pins are analog outputs, they must be in analog mode before
     the unit is enabled (RM ch18.2.2.1). */
  palSetPadMode(GPIOA, 4U, PAL_MODE_INPUT_ANALOG);
  palSetPadMode(GPIOA, 5U, PAL_MODE_INPUT_ANALOG);

  if (dacStart(&DACD1, &dac_config) != MSG_OK) {
    chprintf(chp, "DAC: driver start failed\r\n");
    return;
  }

  /* The trigger pacing the buffered output, see dac_group. */
  dac_trigger_timer_start();

  /* Direct output, a staircase on both channels. */
  step = 0U;
  while (true) {
    for (value = 0U; value <= DAC_TEST_FULL_SCALE;
         value = (dacsample_t)(value + 256U)) {
      msg = dacPutChannelX(&DACD1, DAC_TEST_CHANNEL_1, value);
      if (msg != MSG_OK) {
        dac_failures++;
        chprintf(chp, "DAC: write to channel 1 failed (msg=%d)\r\n", (int)msg);
      }
      msg = dacPutChannelX(&DACD1, DAC_TEST_CHANNEL_2,
                           (dacsample_t)(DAC_TEST_FULL_SCALE - value));
      if (msg != MSG_OK) {
        dac_failures++;
        chprintf(chp, "DAC: write to channel 2 failed (msg=%d)\r\n", (int)msg);
      }

      dac_last_ch1 = value;
      dac_last_ch2 = (dacsample_t)(DAC_TEST_FULL_SCALE - value);
      dac_samples += 2U;

      chThdSleepMilliseconds(DAC_TEST_STEP_MS);
    }

    /* The holding registers hold the last written values, the output
       registers hold the values that are actually converted. */
    if ((DAC->R12BDHR1 & 0x0FFFU) != dac_last_ch1) {
      dac_failures++;
      chprintf(chp, "DAC: channel 1 holding register mismatch (%u vs %u)\r\n",
               (unsigned)(DAC->R12BDHR1 & 0x0FFFU), (unsigned)dac_last_ch1);
    }
    if ((DAC->R12BDHR2 & 0x0FFFU) != dac_last_ch2) {
      dac_failures++;
      chprintf(chp, "DAC: channel 2 holding register mismatch (%u vs %u)\r\n",
               (unsigned)(DAC->R12BDHR2 & 0x0FFFU), (unsigned)dac_last_ch2);
    }
    if (DAC->DOR1 > DAC_TEST_FULL_SCALE) {
      dac_failures++;
      chprintf(chp, "DAC: channel 1 output register out of range (%u)\r\n",
               (unsigned)DAC->DOR1);
    }
    if (DAC->DOR2 > DAC_TEST_FULL_SCALE) {
      dac_failures++;
      chprintf(chp, "DAC: channel 2 output register out of range (%u)\r\n",
               (unsigned)DAC->DOR2);
    }

    dac_last_mv = (DAC_TEST_VDD33A_MV * dac_last_ch1) / DAC_TEST_FULL_SCALE;
    chprintf(chp, "DAC: step %u, ch1=%u (expected %umV) ch2=%u\r\n",
             (unsigned)step, (unsigned)dac_last_ch1, (unsigned)dac_last_mv,
             (unsigned)dac_last_ch2);
    step++;

    /* Buffered output, the DMA replays the sine wave. */
    dac_build_sine();

    msg = dacStartConversion(&DACD1, &dac_group, dac_samples_buffer,
                              DAC_TEST_SINE_DEPTH);
    if (msg != MSG_OK) {
      dac_failures++;
      chprintf(chp, "DAC: conversion start failed (msg=%d)\r\n", (int)msg);
      continue;
    }

    dac_sine_cycles++;
    chprintf(chp, "DAC: sine wave on both channels, %u samples, %u Hz trigger\r\n",
             (unsigned)(2U * DAC_TEST_SINE_DEPTH),
             (unsigned)DAC_TEST_SINE_RATE_HZ);

    chThdSleepMilliseconds(DAC_TEST_SINE_PERIOD_MS);

    dacStopConversion(&DACD1);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the DAC test thread.
 */
void dac_test_start(void) {

  chThdCreateStatic(waDacTest, sizeof(waDacTest), NORMALPRIO, dac_test_thd,
                    NULL);
}
