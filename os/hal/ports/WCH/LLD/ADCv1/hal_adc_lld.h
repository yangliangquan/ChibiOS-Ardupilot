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

#include "hal_lld.h"

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
 * @note    ADCCLK = HCLK / PPRE2 / ADCPRE. The value is the PPRE2[2:0]
 *          field of RCC_CFGR0: 0x00 = /1 (RCC_PPRE2_DIV0), 0x04 = /2,
 *          0x05 = /4, 0x06 = /8, 0x07 = /16.
 */
#if !defined(CH32H417_ADC_PPRE2_DIV) || defined(__DOXYGEN__)
#define CH32H417_ADC_PPRE2_DIV                 0x00U
#endif

/**
 * @brief   ADC clock tree post-divider.
 * @note    The value is the ADCPRE[1:0] field of RCC_CFGR0: 0x00 = /2
 *          (RCC_HCLK_ADCPRE_DIV2), 0x01 = /4, 0x02 = /6, 0x03 = /8.
 */
#if !defined(CH32H417_ADC_PRESCALER) || defined(__DOXYGEN__)
#define CH32H417_ADC_PRESCALER                 0x03U
#endif

/* Divider values of the two ADC prescalers. */
#if   CH32H417_ADC_PPRE2_DIV == 0x00U
#define CH32H417_ADC_PPRE2_VALUE              1U
#elif CH32H417_ADC_PPRE2_DIV == 0x04U
#define CH32H417_ADC_PPRE2_VALUE              2U
#elif CH32H417_ADC_PPRE2_DIV == 0x05U
#define CH32H417_ADC_PPRE2_VALUE              4U
#elif CH32H417_ADC_PPRE2_DIV == 0x06U
#define CH32H417_ADC_PPRE2_VALUE              8U
#elif CH32H417_ADC_PPRE2_DIV == 0x07U
#define CH32H417_ADC_PPRE2_VALUE              16U
#else
#error "invalid CH32H417_ADC_PPRE2_DIV value"
#endif

#if   CH32H417_ADC_PRESCALER == 0x00U
#define CH32H417_ADC_PRESCALER_VALUE          2U
#elif CH32H417_ADC_PRESCALER == 0x01U
#define CH32H417_ADC_PRESCALER_VALUE          4U
#elif CH32H417_ADC_PRESCALER == 0x02U
#define CH32H417_ADC_PRESCALER_VALUE          6U
#elif CH32H417_ADC_PRESCALER == 0x03U
#define CH32H417_ADC_PRESCALER_VALUE          8U
#else
#error "invalid CH32H417_ADC_PRESCALER value"
#endif

/**
 * @brief   ADC clock frequency in Hz.
 * @note    The ADC clock cannot exceed 80MHz (RM 3.3.5). Derived from
 *          the live HCLK and the configured prescalers.
 */
#if !defined(CH32H417_ADCCLK) || defined(__DOXYGEN__)
#define CH32H417_ADCCLK                     (ch32h417_hclk /                    \
                                             CH32H417_ADC_PPRE2_VALUE /          \
                                             CH32H417_ADC_PRESCALER_VALUE)
#endif

/**
 * @brief   Per-unit IRQ and DMA channel IRQ priorities.
 * @note    DMA channels are allocated dynamically at driver start, the
 *          DMAMUX routing allows any channel to be connected to any request.
 * @note    The two units share the ADC1_2 IRQ vector, a single priority is
 *          therefore used for both.
 * @{
 */
#if !defined(CH32H417_ADC_ADC1_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_ADC_ADC1_DMA_PRIORITY        8
#endif
#if !defined(CH32H417_ADC_ADC2_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_ADC_ADC2_DMA_PRIORITY        8
#endif
#if !defined(CH32H417_ADC_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_ADC_IRQ_PRIORITY             8
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_ADC_USE_ADC1 && !CH32H417_ADC_USE_ADC2
#error "ADC driver activated but no ADC unit assigned"
#endif

/* The driver always moves the samples through DMA.*/
#if !defined(CH32_DMA_REQUIRED)
#define CH32_DMA_REQUIRED
#endif

#if CH32H417_ADC_USE_ADC1 &&                                                 \
    !CH32_DMA_IS_VALID_PRIORITY(CH32H417_ADC_ADC1_DMA_PRIORITY)
#error "Invalid DMA priority assigned to ADC1"
#endif
#if CH32H417_ADC_USE_ADC2 &&                                                 \
    !CH32_DMA_IS_VALID_PRIORITY(CH32H417_ADC_ADC2_DMA_PRIORITY)
#error "Invalid DMA priority assigned to ADC2"
#endif
#if (CH32H417_ADC_USE_ADC1 || CH32H417_ADC_USE_ADC2) &&                     \
    !CH32_DMA_IS_VALID_PRIORITY(CH32H417_ADC_IRQ_PRIORITY)
#error "Invalid IRQ priority assigned to the ADC vector"
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
  /* Associated DMA channel.*/                                              \
  const ch32_dma_stream_t   *dmastp;

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
