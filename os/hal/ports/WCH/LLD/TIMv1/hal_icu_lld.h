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
 * @file    TIMv1/hal_icu_lld.h
 * @brief   CH32H417 ICU subsystem low level driver header.
 *
 * @details Input capture on channel 1 of any capture-capable timer
 *          (TIM1..TIM5, TIM8..TIM12; TIM6/TIM7 are basic and excluded),
 *          instance ICUDn maps to TIMn. Timer clock = HCLK.
 *
 * @addtogroup ICU
 * @{
 */

#ifndef HAL_ICU_LLD_H
#define HAL_ICU_LLD_H

#if (HAL_USE_ICU == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_tim.h"

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   ICUDn driver enable switches (ICUDn = TIMn).
 */
#if !defined(CH32H417_ICU_USE_TIM1) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM1                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM2) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM2                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM3) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM3                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM4) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM4                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM5) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM5                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM8) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM8                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM9) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM9                   FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM10) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM10                  FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM11) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM11                  FALSE
#endif
#if !defined(CH32H417_ICU_USE_TIM12) || defined(__DOXYGEN__)
#define CH32H417_ICU_USE_TIM12                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_ICU_USE_TIM1 && !CH32H417_ICU_USE_TIM2 && !CH32H417_ICU_USE_TIM3 &&        \
    !CH32H417_ICU_USE_TIM4 && !CH32H417_ICU_USE_TIM5 && !CH32H417_ICU_USE_TIM8 &&        \
    !CH32H417_ICU_USE_TIM9 && !CH32H417_ICU_USE_TIM10 && !CH32H417_ICU_USE_TIM11 &&      \
    !CH32H417_ICU_USE_TIM12
#error "ICU driver activated but no TIM peripheral assigned"
#endif

/* Allocation conflict guards (shared with GPT/PWM/ST).*/
#if CH32H417_ICU_USE_TIM1
#if defined(CH32_TIM1_IS_USED)
#error "ICUD1 requires TIM1 but the timer is already used"
#else
#define CH32_TIM1_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM2
#if defined(CH32_TIM2_IS_USED)
#error "ICUD2 requires TIM2 but the timer is already used"
#else
#define CH32_TIM2_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM3
#if defined(CH32_TIM3_IS_USED)
#error "ICUD3 requires TIM3 but the timer is already used"
#else
#define CH32_TIM3_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM4
#if defined(CH32_TIM4_IS_USED)
#error "ICUD4 requires TIM4 but the timer is already used"
#else
#define CH32_TIM4_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM5
#if defined(CH32_TIM5_IS_USED)
#error "ICUD5 requires TIM5 but the timer is already used"
#else
#define CH32_TIM5_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM8
#if defined(CH32_TIM8_IS_USED)
#error "ICUD8 requires TIM8 but the timer is already used"
#else
#define CH32_TIM8_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM9
#if defined(CH32_TIM9_IS_USED)
#error "ICUD9 requires TIM9 but the timer is already used"
#else
#define CH32_TIM9_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM10
#if defined(CH32_TIM10_IS_USED)
#error "ICUD10 requires TIM10 but the timer is already used"
#else
#define CH32_TIM10_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM11
#if defined(CH32_TIM11_IS_USED)
#error "ICUD11 requires TIM11 but the timer is already used"
#else
#define CH32_TIM11_IS_USED
#endif
#endif
#if CH32H417_ICU_USE_TIM12
#if defined(CH32_TIM12_IS_USED)
#error "ICUD12 requires TIM12 but the timer is already used"
#else
#define CH32_TIM12_IS_USED
#endif
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @name    ICU mode flags
 * @{
 */
#define ICU_USE_ICU                          1U
#define ICU_USE_FALLING_EDGE                 2U
#define ICU_USE_BOTH_EDGES                   4U
#define ICU_USE_PULSE_WIDTH                  8U
/** @} */

/**
 * @brief   ICU mode type.
 */
typedef uint32_t icumode_t;

/**
 * @brief   ICU counter type (16-bit timers on this platform).
 */
typedef uint16_t icucnt_t;

/**
 * @brief   Driver configuration structure.
 */
typedef struct {
  /**
   * @brief Timer clock in Hz.
   */
  uint32_t                  frequency;
  /**
   * @brief Capture mode, combination of @p ICU_USE_* flags.
   */
  icumode_t                 mode;
  /**
   * @brief Input capture callback pointer.
   * @note  May be @p NULL.
   */
  icucallback_t             callback;
} ICUConfig;

/**
 * @brief   Structure representing an ICU driver.
 */
struct ICUDriver {
  /**
   * @brief Driver state.
   */
  icustate_t                state;
  /**
   * @brief Current configuration data.
   */
  const ICUConfig           *config;
  /* End of the mandatory fields.*/
  /**
   * @brief Timer base clock.
   */
  uint32_t                  clock;
  /**
   * @brief Pointer to the TIMx registers block.
   */
  TIM_TypeDef               *tim;
  /**
   * @brief Next edge polarity to capture (1 = rising, 2 = falling).
   */
  uint8_t                   next_edge;
  /**
   * @brief Capture bookkeeping (edge state machine).
   */
  icucnt_t                  last;
  icucnt_t                  period;
  icucnt_t                  width;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

#define icu_lld_get_counter(icup) ((icucnt_t)(icup)->tim->CNT)

#define icu_lld_get_channel(icup) ((icucnt_t)(icup)->tim->CH1CVR)

#define icu_lld_get_period(icup) (icu_lld_get_channel(icup))

#define icu_lld_get_width(icup)  (icu_lld_get_channel(icup))

#define icu_lld_enable_notifications(icup)                                   \
  ((icup)->tim->DMAINTENR |= TIM_CC1IE)

#define icu_lld_disable_notifications(icup)                                  \
  ((icup)->tim->DMAINTENR &= (uint16_t)~TIM_CC1IE)

#define icu_lld_are_notifications_enabled(icup)                              \
  ((bool)(((icup)->tim->DMAINTENR & TIM_CC1IE) != 0U))

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if CH32H417_ICU_USE_TIM1 && !defined(__DOXYGEN__)
extern ICUDriver ICUD1;
#endif
#if CH32H417_ICU_USE_TIM2 && !defined(__DOXYGEN__)
extern ICUDriver ICUD2;
#endif
#if CH32H417_ICU_USE_TIM3 && !defined(__DOXYGEN__)
extern ICUDriver ICUD3;
#endif
#if CH32H417_ICU_USE_TIM4 && !defined(__DOXYGEN__)
extern ICUDriver ICUD4;
#endif
#if CH32H417_ICU_USE_TIM5 && !defined(__DOXYGEN__)
extern ICUDriver ICUD5;
#endif
#if CH32H417_ICU_USE_TIM8 && !defined(__DOXYGEN__)
extern ICUDriver ICUD8;
#endif
#if CH32H417_ICU_USE_TIM9 && !defined(__DOXYGEN__)
extern ICUDriver ICUD9;
#endif
#if CH32H417_ICU_USE_TIM10 && !defined(__DOXYGEN__)
extern ICUDriver ICUD10;
#endif
#if CH32H417_ICU_USE_TIM11 && !defined(__DOXYGEN__)
extern ICUDriver ICUD11;
#endif
#if CH32H417_ICU_USE_TIM12 && !defined(__DOXYGEN__)
extern ICUDriver ICUD12;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void icu_lld_init(void);
  void icu_lld_start(ICUDriver *icup);
  void icu_lld_stop(ICUDriver *icup);
  void icu_lld_start_capture(ICUDriver *icup);
  void icu_lld_stop_capture(ICUDriver *icup);
  bool icu_lld_wait_capture(ICUDriver *icup);
  void icu_lld_serve_interrupt(ICUDriver *icup);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_ICU == TRUE */

#endif /* HAL_ICU_LLD_H */

/** @} */
