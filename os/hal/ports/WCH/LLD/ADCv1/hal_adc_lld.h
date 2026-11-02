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
 * @file    ADCv1/hal_adc_lld.h
 * @brief   CH32H417 ADC subsystem low level driver header.
 *
 * @details Interrupt driven conversion engine (EOC per sample) on the
 *          F1-compatible vendor library, no DMA required.
 *
 * @addtogroup ADC
 * @{
 */

#ifndef HAL_ADC_LLD_H
#define HAL_ADC_LLD_H

#if (HAL_USE_ADC == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Possible ADC errors mask bits.
 * @{
 */
#define ADC_ERR_DMAFAILURE      1U  /**< DMA operations failure.            */
#define ADC_ERR_OVERFLOW        2U  /**< ADC overflow condition.            */
#define ADC_ERR_AWD             4U  /**< Watchdog triggered.                */
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   ADCx driver enable switches.
 */
#if !defined(CH32H417_ADC_USE_ADC1) || defined(__DOXYGEN__)
#define CH32H417_ADC_USE_ADC1                  FALSE
#endif
#if !defined(CH32H417_ADC_USE_ADC2) || defined(__DOXYGEN__)
#define CH32H417_ADC_USE_ADC2                  FALSE
#endif

/**
 * @brief   ADC clock tree pre-divider (HCLK bus divider).
 * @note    ADCCLK = HCLK / PPRE2 / ADCPRE, pending on-board validation.
 */
#if !defined(CH32H417_ADC_PPRE2_DIV) || defined(__DOXYGEN__)
#define CH32H417_ADC_PPRE2_DIV                 RCC_PPRE2_DIV0
#endif

/**
 * @brief   ADC clock tree post-divider.
 */
#if !defined(CH32H417_ADC_PRESCALER) || defined(__DOXYGEN__)
#define CH32H417_ADC_PRESCALER                 RCC_HCLK_ADCPRE_DIV8
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_ADC_USE_ADC1 && !CH32H417_ADC_USE_ADC2
#error "ADC driver activated but no ADC unit assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   ADC sample data type.
 */
typedef uint16_t adcsample_t;

/**
 * @brief   Channels number in a conversion group.
 */
typedef uint16_t adc_channels_num_t;

/**
 * @brief   Type of an ADC error mask.
 */
typedef uint32_t adcerror_t;

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the ADC driver structure.
 */
#define adc_lld_driver_fields                                               \
  /* Pointer to the ADC registers block.*/                                  \
  ADC_TypeDef               *adc;                                           \
  /* Current sample index in the destination buffer.*/                      \
  size_t                    idx;

/**
 * @brief   Low level fields of the ADC configuration structure.
 */
#define adc_lld_config_fields                                               \
  /* Dummy configuration, it is not needed.*/                               \
  uint32_t                  dummy;

/**
 * @brief   Low level fields of the conversion group configuration.
 */
#define adc_lld_configuration_group_fields                                  \
  /* Channel list, H417 ADC_Channel_x codes, num_channels entries.*/        \
  const uint8_t             *channels;                                      \
  /* Sample time mode (ADC_SampleTime_CyclesMode0..7).*/                    \
  uint8_t                   sample_time;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_ADC_USE_ADC1 == TRUE) && !defined(__DOXYGEN__)
extern ADCDriver ADCD1;
#endif
#if (CH32H417_ADC_USE_ADC2 == TRUE) && !defined(__DOXYGEN__)
extern ADCDriver ADCD2;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void adc_lld_init(void);
  void adc_lld_start(ADCDriver *adcp);
  void adc_lld_stop(ADCDriver *adcp);
  void adc_lld_start_conversion(ADCDriver *adcp);
  void adc_lld_stop_conversion(ADCDriver *adcp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_ADC == TRUE */

#endif /* HAL_ADC_LLD_H */

/** @} */
