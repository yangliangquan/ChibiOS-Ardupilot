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
 * @file    ADCv1/hal_adc_lld.c
 * @brief   CH32H417 ADC subsystem low level driver source.
 *
 * @details One interrupt per sample (EOC), the conversion engine
 *          re-arms the software trigger after each completed sequence
 *          in non-circular mode and relies on the hardware continuous
 *          mode for circular groups.
 *
 * @addtogroup ADC
 * @{
 */

#include "hal.h"

#if (HAL_USE_ADC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_adc.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define ADC_BOUNDED_LOOPS           0x10000U

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_ADC_USE_ADC1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   ADC1 driver identifier.
 */
ADCDriver ADCD1 = {
  .adc = ADC1,
  .idx = 0U
};
#endif
#if (CH32H417_ADC_USE_ADC2 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   ADC2 driver identifier.
 */
ADCDriver ADCD2 = {
  .adc = ADC2,
  .idx = 0U
};
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level ADC driver initialization.
 *
 * @notapi
 */
void adc_lld_init(void) {
}

/**
 * @brief   ADC driver configuration and start.
 *
 * @param[in] adcp      pointer to an @p ADCDriver object
 *
 * @notapi
 */
void adc_lld_start(ADCDriver *adcp) {
  ADC_InitTypeDef init;
  uint32_t guard;

  RCC_HB2PeriphClockCmd((adcp->adc == ADC1) ? RCC_HB2Periph_ADC1 :
                                              RCC_HB2Periph_ADC2,
                        ENABLE);
  RCC_ADCHCLKCLKAsSourceConfig(CH32H417_ADC_PPRE2_DIV,
                               CH32H417_ADC_PRESCALER);

  /* Default configuration, calibration with a bounded wait so that a
     clock tree problem cannot lock up halInit(). */
  ADC_DeInit(adcp->adc);
  ADC_StructInit(&init);
  ADC_Init(adcp->adc, &init);
  ADC_Cmd(adcp->adc, ENABLE);

  ADC_ResetCalibration(adcp->adc);
  guard = ADC_BOUNDED_LOOPS;
  while ((ADC_GetResetCalibrationStatus(adcp->adc) != RESET) &&
         (guard > 0U)) {
    guard--;
  }
  ADC_StartCalibration(adcp->adc);
  guard = ADC_BOUNDED_LOOPS;
  while ((ADC_GetCalibrationStatus(adcp->adc) != RESET) &&
         (guard > 0U)) {
    guard--;
  }

  adcp->idx = 0U;

  NVIC_EnableIRQ(ADC1_2_IRQn);
}

/**
 * @brief   ADC driver stop.
 *
 * @param[in] adcp      pointer to an @p ADCDriver object
 *
 * @notapi
 */
void adc_lld_stop(ADCDriver *adcp) {

  NVIC_DisableIRQ(ADC1_2_IRQn);
  ADC_ITConfig(adcp->adc, ADC_IT_EOC, DISABLE);
  ADC_Cmd(adcp->adc, DISABLE);
}

/**
 * @brief   Starts an ADC conversion group.
 *
 * @param[in] adcp      pointer to an @p ADCDriver object
 *
 * @notapi
 */
void adc_lld_start_conversion(ADCDriver *adcp) {
  const ADCConversionGroup *grpp = adcp->grpp;
  ADC_InitTypeDef init;
  unsigned i;

  /* Sequence setup: sample times first, then the sequencer registers
     (ADC_Init only touches the sequence length bits). */
  for (i = 0; i < (unsigned)grpp->num_channels; i++) {
    ADC_RegularChannelConfig(adcp->adc, grpp->channels[i],
                             (uint8_t)(i + 1U), grpp->sample_time);
  }

  ADC_StructInit(&init);
  init.ADC_Mode = ADC_Mode_Independent;
  init.ADC_ScanConvMode =
      (grpp->num_channels > 1U) ? ENABLE : DISABLE;
  init.ADC_ContinuousConvMode = grpp->circular ? ENABLE : DISABLE;
  init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
  init.ADC_DataAlign = ADC_DataAlign_Right;
  init.ADC_NbrOfChannel = (uint8_t)grpp->num_channels;
  ADC_Init(adcp->adc, &init);

  ADC_Cmd(adcp->adc, ENABLE);

  adcp->idx = 0U;
  ADC_ITConfig(adcp->adc, ADC_IT_EOC, ENABLE);
  ADC_SoftwareStartConvCmd(adcp->adc, ENABLE);
}

/**
 * @brief   Stops an ongoing ADC conversion group.
 *
 * @param[in] adcp      pointer to an @p ADCDriver object
 *
 * @notapi
 */
void adc_lld_stop_conversion(ADCDriver *adcp) {

  ADC_ITConfig(adcp->adc, ADC_IT_EOC, DISABLE);
  ADC_ClearFlag(adcp->adc, ADC_FLAG_EOC | ADC_FLAG_JEOC |
                           ADC_FLAG_STRT | ADC_FLAG_JSTRT);
}

/**
 * @brief   Common IRQ service for one ADC unit.
 */
static void serve(ADCDriver *adcp) {
  const ADCConversionGroup *grpp = adcp->grpp;

  if (grpp == NULL) {
    if (ADC_GetFlagStatus(adcp->adc, ADC_FLAG_EOC) != RESET) {
      ADC_ClearFlag(adcp->adc, ADC_FLAG_EOC);
    }
    return;
  }

  if (ADC_GetFlagStatus(adcp->adc, ADC_FLAG_EOC) != RESET) {
    size_t total = (size_t)grpp->num_channels * adcp->depth;

    ADC_ClearFlag(adcp->adc, ADC_FLAG_EOC);
    adcp->samples[adcp->idx] = ADC_GetConversionValue(adcp->adc);
    adcp->idx++;

    if (grpp->circular) {
      if ((adcp->depth > 1U) && (adcp->idx == (total / 2U))) {
        _adc_isr_half_code(adcp);
      }
      if (adcp->idx >= total) {
        adcp->idx = 0U;
        _adc_isr_full_code(adcp);
      }
    }
    else {
      if (adcp->idx >= total) {
        adcp->idx = 0U;
        _adc_isr_full_code(adcp);
      }
      else if ((adcp->idx % (size_t)grpp->num_channels) == 0U) {
        /* One sequence completed, re-arm for the next one. */
        ADC_SoftwareStartConvCmd(adcp->adc, ENABLE);
      }
    }
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_ADC_USE_ADC1 || CH32H417_ADC_USE_ADC2) || defined(__DOXYGEN__)
/**
 * @brief   ADC1/ADC2 interrupt handler (shared vector).
 *
 * @isr
 */
OSAL_IRQ_HANDLER(ADC1_2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

#if CH32H417_ADC_USE_ADC1
  serve(&ADCD1);
#endif
#if CH32H417_ADC_USE_ADC2
  serve(&ADCD2);
#endif

  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_ADC == TRUE */

/** @} */
