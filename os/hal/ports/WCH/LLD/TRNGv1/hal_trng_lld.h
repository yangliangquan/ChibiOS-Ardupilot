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
 * @file    TRNGv1/hal_trng_lld.h
 * @brief   CH32H417 TRNG subsystem low level driver header.
 *
 * @details True random number generator based on the RNG peripheral
 *          (RNG registers CR/SR/DR, polled operation).
 *
 * @addtogroup TRNG
 * @{
 */

#ifndef HAL_TRNG_LLD_H
#define HAL_TRNG_LLD_H

#if (HAL_USE_TRNG == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_rng.h"

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   RNG unit enable switch.
 */
#if !defined(CH32H417_TRNG_USE_RNG) || defined(__DOXYGEN__)
#define CH32H417_TRNG_USE_RNG                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_TRNG_USE_RNG
#error "TRNG driver activated but no RNG unit assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Low level configuration fields.
 */
#define trng_lld_config_fields                                               \
  /* Dummy configuration, it is not needed.*/                               \
  uint32_t                  dummy;

/**
 * @brief   Low level driver fields (none, singleton unit).
 */
#define trng_lld_driver_fields

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_TRNG_USE_RNG == TRUE) && !defined(__DOXYGEN__)
extern TRNGDriver TRNGD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void trng_lld_init(void);
  void trng_lld_start(TRNGDriver *trngp);
  void trng_lld_stop(TRNGDriver *trngp);
  bool trng_lld_generate(TRNGDriver *trngp, size_t size, uint8_t *out);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_TRNG == TRUE */

#endif /* HAL_TRNG_LLD_H */

/** @} */
