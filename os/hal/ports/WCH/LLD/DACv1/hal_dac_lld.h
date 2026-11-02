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
 * @file    DACv1/hal_dac_lld.h
 * @brief   CH32H417 DAC subsystem low level driver header.
 *
 * @details One DAC unit with two channels (DAC1_OUT1/OUT2),
 *          F1-compatible register model. Single-value output via
 *          dacPutChannelX() is supported; buffered conversions
 *          require DMA and are not available yet.
 *
 * @addtogroup DAC
 * @{
 */

#ifndef HAL_DAC_LLD_H
#define HAL_DAC_LLD_H

#if (HAL_USE_DAC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_dac.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Enhanced API: the LLD functions return error codes.
 */
#define DAC_LLD_ENHANCED_API

/**
 * @brief   Number of DAC channels of the unit.
 */
#define DAC_MAX_CHANNELS                    2

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   DAC unit enable switch.
 */
#if !defined(CH32H417_DAC_USE_DAC1) || defined(__DOXYGEN__)
#define CH32H417_DAC_USE_DAC1                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_DAC_USE_DAC1
#error "DAC driver activated but no DAC unit assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   DAC sample type (12-bit right aligned on this platform).
 */
typedef uint16_t dacsample_t;

/**
 * @brief   DAC channel type (0..1).
 */
typedef uint8_t dacchannel_t;

/**
 * @brief   DAC error type.
 */
typedef uint32_t dacerror_t;

/**
 * @brief   Low level configuration fields (none).
 */
#define dac_lld_config_fields

/**
 * @brief   Low level conversion group fields (none).
 */
#define dac_lld_conversion_group_fields

/**
 * @brief   Low level driver fields.
 */
#define dac_lld_driver_fields                                             \
  /* Pointer to the DAC registers block.*/                                \
  DAC_TypeDef               *dac;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_DAC_USE_DAC1 == TRUE) && !defined(__DOXYGEN__)
extern DACDriver DACD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void dac_lld_init(void);
  msg_t dac_lld_start(DACDriver *dacp);
  void dac_lld_stop(DACDriver *dacp);
  msg_t dac_lld_put_channel(DACDriver *dacp,
                            dacchannel_t channel,
                            dacsample_t sample);
  msg_t dac_lld_start_conversion(DACDriver *dacp);
  void dac_lld_stop_conversion(DACDriver *dacp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_DAC == TRUE */

#endif /* HAL_DAC_LLD_H */

/** @} */
