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
 * @file    TIMv1/hal_gpt_lld.h
 * @brief   CH32H417 GPT subsystem low level driver header.
 *
 * @details Any of the twelve timers (TIM1..TIM12) can serve as a GPT,
 *          instance GPTDn maps to TIMn. Timer clock = HCLK.
 *
 * @addtogroup GPT
 * @{
 */

#ifndef HAL_GPT_LLD_H
#define HAL_GPT_LLD_H

#if (HAL_USE_GPT == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_tim.h"

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   GPTDn driver enable switches (GPTDn = TIMn).
 */
#if !defined(CH32H417_GPT_USE_TIM1) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM1                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM2) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM2                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM3) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM3                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM4) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM4                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM5) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM5                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM6) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM6                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM7) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM7                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM8) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM8                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM9) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM9                   FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM10) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM10                  FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM11) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM11                  FALSE
#endif
#if !defined(CH32H417_GPT_USE_TIM12) || defined(__DOXYGEN__)
#define CH32H417_GPT_USE_TIM12                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_GPT_USE_TIM1 && !CH32H417_GPT_USE_TIM2 && !CH32H417_GPT_USE_TIM3 &&        \
    !CH32H417_GPT_USE_TIM4 && !CH32H417_GPT_USE_TIM5 && !CH32H417_GPT_USE_TIM6 &&        \
    !CH32H417_GPT_USE_TIM7 && !CH32H417_GPT_USE_TIM8 && !CH32H417_GPT_USE_TIM9 &&        \
    !CH32H417_GPT_USE_TIM10 && !CH32H417_GPT_USE_TIM11 && !CH32H417_GPT_USE_TIM12
#error "GPT driver activated but no TIM peripheral assigned"
#endif

/* Allocation conflict guards (shared with ICU/PWM/ST).*/
#if CH32H417_GPT_USE_TIM1
#if defined(CH32_TIM1_IS_USED)
#error "GPTD1 requires TIM1 but the timer is already used"
#else
#define CH32_TIM1_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM2
#if defined(CH32_TIM2_IS_USED)
#error "GPTD2 requires TIM2 but the timer is already used"
#else
#define CH32_TIM2_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM3
#if defined(CH32_TIM3_IS_USED)
#error "GPTD3 requires TIM3 but the timer is already used"
#else
#define CH32_TIM3_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM4
#if defined(CH32_TIM4_IS_USED)
#error "GPTD4 requires TIM4 but the timer is already used"
#else
#define CH32_TIM4_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM5
#if defined(CH32_TIM5_IS_USED)
#error "GPTD5 requires TIM5 but the timer is already used"
#else
#define CH32_TIM5_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM6
#if defined(CH32_TIM6_IS_USED)
#error "GPTD6 requires TIM6 but the timer is already used"
#else
#define CH32_TIM6_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM7
#if defined(CH32_TIM7_IS_USED)
#error "GPTD7 requires TIM7 but the timer is already used"
#else
#define CH32_TIM7_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM8
#if defined(CH32_TIM8_IS_USED)
#error "GPTD8 requires TIM8 but the timer is already used"
#else
#define CH32_TIM8_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM9
#if defined(CH32_TIM9_IS_USED)
#error "GPTD9 requires TIM9 but the timer is already used"
#else
#define CH32_TIM9_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM10
#if defined(CH32_TIM10_IS_USED)
#error "GPTD10 requires TIM10 but the timer is already used"
#else
#define CH32_TIM10_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM11
#if defined(CH32_TIM11_IS_USED)
#error "GPTD11 requires TIM11 but the timer is already used"
#else
#define CH32_TIM11_IS_USED
#endif
#endif
#if CH32H417_GPT_USE_TIM12
#if defined(CH32_TIM12_IS_USED)
#error "GPTD12 requires TIM12 but the timer is already used"
#else
#define CH32_TIM12_IS_USED
#endif
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   GPT frequency type.
 */
typedef uint32_t gptfreq_t;

/**
 * @brief   GPT counter type (16-bit timers on this platform).
 */
typedef uint16_t gptcnt_t;

/**
 * @brief   Driver configuration structure.
 */
typedef struct {
  /**
   * @brief Timer clock in Hz.
   */
  gptfreq_t                 frequency;
  /**
   * @brief Timer callback pointer.
   * @note  May be @p NULL, in which case one-shot mode cannot be used.
   */
  gptcallback_t             callback;
} GPTConfig;

/**
 * @brief   Structure representing a GPT driver.
 */
struct GPTDriver {
  /**
   * @brief Driver state.
   */
  gptstate_t                state;
  /**
   * @brief Current configuration data.
   */
  const GPTConfig           *config;
  /* End of the mandatory fields.*/
  /**
   * @brief Timer base clock.
   */
  uint32_t                  clock;
  /**
   * @brief Pointer to the TIMx registers block.
   */
  TIM_TypeDef               *tim;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

#define gpt_lld_get_counter(gptp) ((gptcnt_t)(gptp)->tim->CNT)

#define gpt_lld_get_interval(gptp) ((gptcnt_t)((gptp)->tim->ATRLR + 1U))

#define gpt_lld_change_interval(gptp, interval)                             \
  ((gptp)->tim->ATRLR = (uint16_t)((interval) - 1U))

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if CH32H417_GPT_USE_TIM1 && !defined(__DOXYGEN__)
extern GPTDriver GPTD1;
#endif
#if CH32H417_GPT_USE_TIM2 && !defined(__DOXYGEN__)
extern GPTDriver GPTD2;
#endif
#if CH32H417_GPT_USE_TIM3 && !defined(__DOXYGEN__)
extern GPTDriver GPTD3;
#endif
#if CH32H417_GPT_USE_TIM4 && !defined(__DOXYGEN__)
extern GPTDriver GPTD4;
#endif
#if CH32H417_GPT_USE_TIM5 && !defined(__DOXYGEN__)
extern GPTDriver GPTD5;
#endif
#if CH32H417_GPT_USE_TIM6 && !defined(__DOXYGEN__)
extern GPTDriver GPTD6;
#endif
#if CH32H417_GPT_USE_TIM7 && !defined(__DOXYGEN__)
extern GPTDriver GPTD7;
#endif
#if CH32H417_GPT_USE_TIM8 && !defined(__DOXYGEN__)
extern GPTDriver GPTD8;
#endif
#if CH32H417_GPT_USE_TIM9 && !defined(__DOXYGEN__)
extern GPTDriver GPTD9;
#endif
#if CH32H417_GPT_USE_TIM10 && !defined(__DOXYGEN__)
extern GPTDriver GPTD10;
#endif
#if CH32H417_GPT_USE_TIM11 && !defined(__DOXYGEN__)
extern GPTDriver GPTD11;
#endif
#if CH32H417_GPT_USE_TIM12 && !defined(__DOXYGEN__)
extern GPTDriver GPTD12;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void gpt_lld_init(void);
  void gpt_lld_start(GPTDriver *gptp);
  void gpt_lld_stop(GPTDriver *gptp);
  void gpt_lld_start_timer(GPTDriver *gptp, gptcnt_t period);
  void gpt_lld_stop_timer(GPTDriver *gptp);
  void gpt_lld_polled_delay(GPTDriver *gptp, gptcnt_t interval);
  void gpt_lld_serve_interrupt(GPTDriver *gptp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_GPT == TRUE */

#endif /* HAL_GPT_LLD_H */

/** @} */
