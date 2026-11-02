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
 * @file    hal_lld.h
 * @brief   CH32H417 HAL subsystem low level driver header.
 *
 * @addtogroup HAL
 * @{
 */

#ifndef HAL_LLD_H
#define HAL_LLD_H

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Platform identification macros
 * @{
 */
#define PLATFORM_NAME           "WCH CH32H417 (QingKe V5F)"

/**
 * @brief   Requires the SPI driver v2 model (msg_t based transfers).
 */
#define HAL_LLD_SELECT_SPI_V2           TRUE
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    PLATFORM configuration options
 * @{
 */
/**
 * @brief   V5F core clock in Hz.
 * @note    Target of @p ch32h417_clock_init(): SYSCLK = 400MHz (HSE
 *          25MHz x PLL16), V5F core on SYSCLK, FPRE domain (HCLK/V3F
 *          side) at SYSCLK/4. The V3F boot stub normally establishes
 *          the same tree before the V5F core is woken;
 *          @p ch32h417_clock_init() is idempotent and only rebuilds a
 *          tree that does not match the target.
 */
#if !defined(CH32H417_HCLK) || defined(__DOXYGEN__)
#define CH32H417_HCLK                      400000000U
#endif

/**
 * @brief   Disables the clock tree setup in @p ch32h417_clock_init().
 */
#if !defined(CH32H417_NO_INIT) || defined(__DOXYGEN__)
#define CH32H417_NO_INIT                   FALSE
#endif
/** @} */

/**
 * @brief   Timer peripheral clock in Hz.
 * @note    All timers (TIM1..TIM12) are clocked by the HCLK on this
 *          platform (both bus domains run at HCLK).
 */
#if !defined(CH32H417_TIMCLK1) || defined(__DOXYGEN__)
#define CH32H417_TIMCLK1                    CH32H417_HCLK
#endif

#include "ch32h417.h"

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*
 * Configuration-related checks.
 */
#if !defined(CH32H417_MCUCONF)
#error "Using a wrong mcuconf.h file, CH32H417_MCUCONF not defined"
#endif

#if CH32H417_HCLK % 1000U != 0U
#error "CH32H417_HCLK not a multiple of 1kHz"
#endif

/* Note: ch32h417_clock_init() targets HSE x PLLMUL16; HSE_VALUE is a
   cast macro so the relation cannot be checked at preprocessing time,
   hal_lld_init() asserts SystemClock == CH32H417_HCLK at runtime. */

/**
 * @brief   Clock point query, no dynamic clock management on this
 *          platform (the tree only changes via ch32h417_clock_init(),
 *          which runs before the HAL is up).
 */
#define hal_lld_get_clock_point(clkpt)      0U

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void hal_lld_init(void);
  void ch32h417_clock_init(void);
#ifdef __cplusplus
}
#endif

#endif /* HAL_LLD_H */

/** @} */
