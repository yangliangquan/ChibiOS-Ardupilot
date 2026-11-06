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
 * @details DMA driven conversion engine on the F1-compatible vendor library.
 *          One DMA channel per unit moves the conversion results into the
 *          samples buffer, the half and full transfer interrupts drive the
 *          streaming callbacks.
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
  .adc = ADC1
};
#endif
#if (CH32H417_ADC_USE_ADC2 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   ADC2 driver identifier.
 */
ADCDriver ADCD2 = {
  .adc = ADC2
};
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   DMA end of transfer service routine.
 *
 * @param[in] adcp      pointer to the @p ADCDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void adc_lld_serve_interrupt(ADCDriver *adcp, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    ch32DmaStreamDisable(adcp->dmastp);
    ADC_DMACmd(adcp->adc, DISABLE);

    /* DMA failure reported to the upper layer.*/
    _adc_isr_error_code(adcp, ADC_ERR_DMAFAILURE);
    return;
  }

  /* It is possible that the conversion group has already been reset by the
     error handler, in this case this interrupt is spurious.*/
  if (adcp->grpp == NULL) {
    return;
  }

  if ((flags & CH32_DMA_ISR_TCIF) != 0U) {
    if (!adcp->grpp->circular) {
      /* Normal mode, the buffer is complete, the engine is stopped here.*/
      ch32DmaStreamDisable(adcp->dmastp);
      ADC_DMACmd(adcp->adc, DISABLE);
      ADC_Cmd(adcp->adc, DISABLE);
    }
    _adc_isr_full_code(adcp);
  }
  else if ((flags & CH32_DMA_ISR_HTIF) != 0U) {
    _adc_isr_half_code(adcp);
  }
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level ADC driver initialization.
 *
 * @notapi
 */
void adc_lld_init(void) {

#if CH32H417_ADC_USE_ADC1 == TRUE
  adcObjectInit(&ADCD1);
#endif
#if CH32H417_ADC_USE_ADC2 == TRUE
  adcObjectInit(&ADCD2);
#endif
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

  /* Dynamic channel allocation, then routing of the ADC request onto the
     channel that was handed out.*/
  if (adcp->adc == ADC1) {
    adcp->dmastp = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                       CH32H417_ADC_ADC1_DMA_PRIORITY,
                                       (ch32_dmaisr_t)adc_lld_serve_interrupt,
                                       (void *)adcp);
    osalDbgAssert(adcp->dmastp != NULL, "ADC1 DMA channel not available");
    if (adcp->dmastp == NULL) {
      return;
    }
    ch32DmaStreamSetRequest(adcp->dmastp, CH32_DMAMUX_ADC1);
  }
  else {
    adcp->dmastp = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                       CH32H417_ADC_ADC2_DMA_PRIORITY,
                                       (ch32_dmaisr_t)adc_lld_serve_interrupt,
                                       (void *)adcp);
    osalDbgAssert(adcp->dmastp != NULL, "ADC2 DMA channel not available");
    if (adcp->dmastp == NULL) {
      return;
    }
    ch32DmaStreamSetRequest(adcp->dmastp, CH32_DMAMUX_ADC2);
  }

  ch32DmaStreamSetPeripheral(adcp->dmastp, &adcp->adc->RDATAR);

  /* Shared ADC1/ADC2 vector, kept for the watchdog source.*/
  NVIC_SetPriority(ADC1_2_IRQn,
                   (uint8_t)CH32_IRQ_PRIORITY(CH32H417_ADC_IRQ_PRIORITY));
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
  ADC_DMACmd(adcp->adc, DISABLE);
  ADC_Cmd(adcp->adc, DISABLE);

  if (adcp->dmastp != NULL) {
    ch32DmaStreamFreeI(adcp->dmastp);
    adcp->dmastp = NULL;
  }
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
  uint32_t mode;
  uint32_t total;
  unsigned i;

  /* No channel could be allocated at start, the conversion cannot run.*/
  osalDbgAssert(adcp->dmastp != NULL, "ADC DMA channel not allocated");
  if (adcp->dmastp == NULL) {
    return;
  }

  /* Sequence setup: sample times first, then the sequencer registers
     (ADC_Init only touches the sequence length bits). */
  for (i = 0; i < (unsigned)grpp->num_channels; i++) {
    ADC_RegularChannelConfig(adcp->adc, grpp->channels[i],
                             (uint8_t)(i + 1U), grpp->sample_time);
  }

  /* Continuous conversion is required when the ADC has to produce more than
     one sequence on its own: circular streaming, or a buffer deeper than the
     channel count. Otherwise a single software trigger is enough and the DMA
     completes as soon as the sequence is finished. */
  ADC_StructInit(&init);
  init.ADC_Mode = ADC_Mode_Independent;
  init.ADC_ScanConvMode = (grpp->num_channels > 1U) ? ENABLE : DISABLE;
  init.ADC_ContinuousConvMode = ((grpp->circular) || (adcp->depth > 1U)) ?
                                ENABLE : DISABLE;
  init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
  init.ADC_DataAlign = ADC_DataAlign_Right;
  init.ADC_NbrOfChannel = (uint8_t)grpp->num_channels;
  ADC_Init(adcp->adc, &init);

  /* DMA setup, one halfword per sample.*/
  total = (uint32_t)grpp->num_channels * (uint32_t)adcp->depth;
  mode = CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_MINC |
         CH32_DMA_CFGR_PSIZE_HWORD | CH32_DMA_CFGR_MSIZE_HWORD |
         CH32_DMA_CFGR_TCIE | CH32_DMA_CFGR_TEIE;
  if (grpp->circular) {
    mode |= CH32_DMA_CFGR_CIRC;
    if (adcp->depth > 1U) {
      mode |= CH32_DMA_CFGR_HTIE;
    }
  }

  ch32DmaStreamDisable(adcp->dmastp);
  ch32DmaStreamSetMemory0(adcp->dmastp, adcp->samples);
  ch32DmaStreamSetTransactionSize(adcp->dmastp, total);
  ch32DmaStreamSetMode(adcp->dmastp, mode);
  ch32DmaStreamClearInterrupt(adcp->dmastp);

  ADC_Cmd(adcp->adc, ENABLE);
  ADC_DMACmd(adcp->adc, ENABLE);

  ch32DmaStreamEnable(adcp->dmastp);
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

  if (adcp->dmastp != NULL) {
    ch32DmaStreamDisable(adcp->dmastp);
  }
  ADC_DMACmd(adcp->adc, DISABLE);
  ADC_ClearFlag(adcp->adc, ADC_FLAG_EOC | ADC_FLAG_JEOC |
                           ADC_FLAG_STRT | ADC_FLAG_JSTRT);
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_ADC_USE_ADC1 || CH32H417_ADC_USE_ADC2) || defined(__DOXYGEN__)
/**
 * @brief   ADC1/ADC2 analog watchdog interrupt handler (shared vector).
 * @note    The end of conversion interrupt is not used, the samples are moved
 *          by DMA. The vector is kept for the watchdog source.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(ADC1_2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

#if CH32H417_ADC_USE_ADC1
  if (ADC_GetFlagStatus(ADCD1.adc, ADC_FLAG_AWD) != RESET) {
    ADC_ClearFlag(ADCD1.adc, ADC_FLAG_AWD);
    _adc_isr_error_code(&ADCD1, ADC_ERR_AWD);
  }
#endif
#if CH32H417_ADC_USE_ADC2
  if (ADC_GetFlagStatus(ADCD2.adc, ADC_FLAG_AWD) != RESET) {
    ADC_ClearFlag(ADCD2.adc, ADC_FLAG_AWD);
    _adc_isr_error_code(&ADCD2, ADC_ERR_AWD);
  }
#endif

  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_ADC == TRUE */

/** @} */
