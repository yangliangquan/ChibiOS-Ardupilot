/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    DACv1/hal_dac_lld.c
 * @brief   CH32H417 DAC subsystem low level driver source.
 *
 * @details Both channels are initialized at start with the software
 *          trigger; dacPutChannelX() writes the holding register and
 *          fires the trigger. Buffer conversions (dacStartConversion)
 *          are not supported without DMA.
 *
 * @addtogroup DAC
 * @{
 */

#include "hal.h"

#if (HAL_USE_DAC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_dac.h"

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_DAC_USE_DAC1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   DACD1 driver identifier.
 */
DACDriver DACD1 = {
  .dac = DAC
};
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level DAC driver initialization.
 *
 * @notapi
 */
void dac_lld_init(void) {
}

/**
 * @brief   DAC driver configuration and start.
 *
 * @notapi
 */
msg_t dac_lld_start(DACDriver *dacp) {
  DAC_InitTypeDef init;

  RCC_HB1PeriphClockCmd(RCC_HB1Periph_DAC, ENABLE);

  DAC_StructInit(&init);
  init.DAC_Trigger = DAC_Trigger_Software;
  init.DAC_WaveGeneration = DAC_WaveGeneration_None;
  init.DAC_OutputBuffer = DAC_OutputBuffer_Enable;

  DAC_Init(DAC_Channel_1, &init);
  DAC_Init(DAC_Channel_2, &init);
  DAC_Cmd(DAC_Channel_1, ENABLE);
  DAC_Cmd(DAC_Channel_2, ENABLE);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   DAC driver stop.
 *
 * @notapi
 */
void dac_lld_stop(DACDriver *dacp) {

  DAC_Cmd(DAC_Channel_1, DISABLE);
  DAC_Cmd(DAC_Channel_2, DISABLE);
}

/**
 * @brief   Direct channel output of one sample.
 *
 * @notapi
 */
msg_t dac_lld_put_channel(DACDriver *dacp,
                          dacchannel_t channel,
                          dacsample_t sample) {

  osalDbgAssert(channel < 2U, "invalid channel");

  if (channel == 0U) {
    DAC_SetChannel1Data(DAC_Align_12b_R, sample);
    DAC_SoftwareTriggerCmd(DAC_Channel_1, ENABLE);
  }
  else {
    DAC_SetChannel2Data(DAC_Align_12b_R, sample);
    DAC_SoftwareTriggerCmd(DAC_Channel_2, ENABLE);
  }

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Starts a buffered conversion (requires DMA).
 *
 * @notapi
 */
msg_t dac_lld_start_conversion(DACDriver *dacp) {

  /* Buffered conversions need DMA, not available yet. */
  return HAL_RET_NO_RESOURCE;
}

/**
 * @brief   Stops a buffered conversion.
 *
 * @notapi
 */
void dac_lld_stop_conversion(DACDriver *dacp) {
}

#endif /* HAL_USE_DAC == TRUE */

/** @} */
