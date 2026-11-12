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
 * @file    hal_st_lld.h
 * @brief   CH32H417 ST Driver low level driver header.
 *
 * @details Two clock sources, following the STM32 SYSTICKv1 model:
 *          - OSAL_ST_MODE_PERIODIC: core SysTick1 (IRQ13)
 *          - OSAL_ST_MODE_FREERUNNING: a general purpose timer
 *            (CH32H417_ST_USE_TIMER) providing a free running counter
 *            with four compare alarms. TIM2..TIM5 are 16-bit units,
 *            TIM9..TIM12 are 32-bit units (DS0, timer overview), the
 *            32-bit sources remove the narrow wrap window of the 16-bit
 *            ones under tickless operation.
 *          The timers are clocked by the core HCLK.
 *
 * @addtogroup ST
 * @{
 */

#ifndef HAL_ST_LLD_H
#define HAL_ST_LLD_H

#include "hal_lld.h"

#if (OSAL_ST_MODE != OSAL_ST_MODE_NONE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING
#define ST_LLD_NUM_ALARMS                  4
#else
#define ST_LLD_NUM_ALARMS                  1
#endif

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   Timer unit used as free running ST clock source
 *          (TIM2..TIM5, 16-bit, or TIM9..TIM12, 32-bit, ignored in
 *          periodic mode).
 */
#if !defined(CH32H417_ST_USE_TIMER) || defined(__DOXYGEN__)
#define CH32H417_ST_USE_TIMER               5
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING

#if (CH32H417_ST_USE_TIMER >= 2) && (CH32H417_ST_USE_TIMER <= 5)
#define ST_LLD_WIDTH                        16
#elif (CH32H417_ST_USE_TIMER >= 9) && (CH32H417_ST_USE_TIMER <= 12)
#define ST_LLD_WIDTH                        32
#else
#error "CH32H417_ST_USE_TIMER must be TIM2..TIM5 or TIM9..TIM12"
#endif

#if OSAL_ST_RESOLUTION != ST_LLD_WIDTH
#error "CH_CFG_ST_RESOLUTION must match the selected ST timer width"
#endif

#if !defined(CH32H417_TIMCLK1)
#define CH32H417_TIMCLK1 ch32h417_timclk1
#endif

#if (CH32H417_TIMCLK1 % OSAL_ST_FREQUENCY) != 0
#error "the selected ST frequency is not obtainable because integer rounding"
#endif

#if ((CH32H417_TIMCLK1 / OSAL_ST_FREQUENCY) - 1U) > 0xFFFFU
#error "the selected ST frequency is not obtainable because timer prescaler limits"
#endif

/* Register selection: TIM9..TIM12 use the 32-bit aliases of the shared
   TIM_TypeDef layout, all channels of those units are 32-bit wide. */
#if ST_LLD_WIDTH == 32
#define ST_REG_CNT                          CNT_32
#define ST_REG_ATRLR                        ATRLR_32
#define ST_REG_CH1CVR                       CH1CVR_32
#define ST_REG_CH2CVR                       CH2CVR_32
#define ST_REG_CH3CVR                       CH3CVR_32
#define ST_REG_CH4CVR                       CH4CVR_32
#define ST_LLD_COUNT_MAX                    0xFFFFFFFFU
#else
#define ST_REG_CNT                          CNT
#define ST_REG_ATRLR                        ATRLR
#define ST_REG_CH1CVR                       CH1CVR
#define ST_REG_CH2CVR                       CH2CVR
#define ST_REG_CH3CVR                       CH3CVR
#define ST_REG_CH4CVR                       CH4CVR
#define ST_LLD_COUNT_MAX                    0xFFFFU
#endif

/* Timer unit allocation conflict guards (shared with GPT/ICU/PWM).*/
#if CH32H417_ST_USE_TIMER == 2
#if defined(CH32_TIM2_IS_USED)
#error "ST requires TIM2 but the timer is already used"
#else
#define CH32_TIM2_IS_USED
#endif
#define ST_HANDLER                       TIM2_IRQHandler
#define ST_NUMBER                        TIM2_IRQn
#define ST_TIM                           TIM2
#define ST_ENABLE_CLOCK()                RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM2, ENABLE)
#elif CH32H417_ST_USE_TIMER == 3
#if defined(CH32_TIM3_IS_USED)
#error "ST requires TIM3 but the timer is already used"
#else
#define CH32_TIM3_IS_USED
#endif
#define ST_HANDLER                       TIM3_IRQHandler
#define ST_NUMBER                        TIM3_IRQn
#define ST_TIM                           TIM3
#define ST_ENABLE_CLOCK()                RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM3, ENABLE)
#elif CH32H417_ST_USE_TIMER == 4
#if defined(CH32_TIM4_IS_USED)
#error "ST requires TIM4 but the timer is already used"
#else
#define CH32_TIM4_IS_USED
#endif
#define ST_HANDLER                       TIM4_IRQHandler
#define ST_NUMBER                        TIM4_IRQn
#define ST_TIM                           TIM4
#define ST_ENABLE_CLOCK()                RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM4, ENABLE)
#elif CH32H417_ST_USE_TIMER == 5
#if defined(CH32_TIM5_IS_USED)
#error "ST requires TIM5 but the timer is already used"
#else
#define CH32_TIM5_IS_USED
#endif
#define ST_HANDLER                       TIM5_IRQHandler
#define ST_NUMBER                        TIM5_IRQn
#define ST_TIM                           TIM5
#define ST_ENABLE_CLOCK()                RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM5, ENABLE)
#elif CH32H417_ST_USE_TIMER == 9
#if defined(CH32_TIM9_IS_USED)
#error "ST requires TIM9 but the timer is already used"
#else
#define CH32_TIM9_IS_USED
#endif
#define ST_HANDLER                       TIM9_IRQHandler
#define ST_NUMBER                        TIM9_IRQn
#define ST_TIM                           TIM9
#define ST_ENABLE_CLOCK()                RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM9, ENABLE)
#elif CH32H417_ST_USE_TIMER == 10
#if defined(CH32_TIM10_IS_USED)
#error "ST requires TIM10 but the timer is already used"
#else
#define CH32_TIM10_IS_USED
#endif
#define ST_HANDLER                       TIM10_IRQHandler
#define ST_NUMBER                        TIM10_IRQn
#define ST_TIM                           TIM10
#define ST_ENABLE_CLOCK()                RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM10, ENABLE)
#elif CH32H417_ST_USE_TIMER == 11
#if defined(CH32_TIM11_IS_USED)
#error "ST requires TIM11 but the timer is already used"
#else
#define CH32_TIM11_IS_USED
#endif
#define ST_HANDLER                       TIM11_IRQHandler
#define ST_NUMBER                        TIM11_IRQn
#define ST_TIM                           TIM11
#define ST_ENABLE_CLOCK()                RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM11, ENABLE)
#elif CH32H417_ST_USE_TIMER == 12
#if defined(CH32_TIM12_IS_USED)
#error "ST requires TIM12 but the timer is already used"
#else
#define CH32_TIM12_IS_USED
#endif
#define ST_HANDLER                       TIM12_IRQHandler
#define ST_NUMBER                        TIM12_IRQn
#define ST_TIM                           TIM12
#define ST_ENABLE_CLOCK()                RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE)
#endif

#endif /* OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING */

#if OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC

#define ST_HANDLER                       SysTick1_Handler

#if !defined(CH32H417_HCLK)
#define CH32H417_HCLK                   ch32h417_hclk
#endif

#if (CH32H417_HCLK % OSAL_ST_FREQUENCY) != 0
#error "the selected ST frequency is not obtainable because integer rounding"
#endif

#if (CH32H417_HCLK / OSAL_ST_FREQUENCY) > 0x100000000ULL
#error "the selected ST frequency is not obtainable because counter limits"
#endif

#endif /* OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC */

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

#if (OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING) || defined(__DOXYGEN__)

/**
 * @brief   Returns the time counter value.
 *
 * @notapi
 */
static inline systime_t st_lld_get_counter(void) {

  return (systime_t)ST_TIM->ST_REG_CNT;
}

/**
 * @brief   Determines if the alarm is active.
 *
 * @notapi
 */
static inline bool st_lld_is_alarm_active(void) {

  return (bool)((ST_TIM->DMAINTENR & (1U << 1)) != 0U);
}

/**
 * @brief   Starts the alarm.
 *
 * @notapi
 */
static inline void st_lld_start_alarm(systime_t abstime) {

  osalDbgAssert(st_lld_is_alarm_active() == false, "already active");

  ST_TIM->ST_REG_CH1CVR = abstime;
  ST_TIM->INTFR = 0;
  ST_TIM->DMAINTENR |= 1U << 1;
}

/**
 * @brief   Stops the alarm interrupt.
 *
 * @notapi
 */
static inline void st_lld_stop_alarm(void) {

  ST_TIM->DMAINTENR &= ~(1U << 1);
}

/**
 * @brief   Sets the alarm time.
 *
 * @notapi
 */
static inline void st_lld_set_alarm(systime_t abstime) {

  ST_TIM->ST_REG_CH1CVR = abstime;
}

/**
 * @brief   Returns the current alarm time.
 *
 * @notapi
 */
static inline systime_t st_lld_get_alarm(void) {

  return (systime_t)ST_TIM->ST_REG_CH1CVR;
}

#if (ST_LLD_NUM_ALARMS > 1) || defined(__DOXYGEN__)
/**
 * @brief   Determines if an alarm is active.
 *
 * @notapi
 */
static inline bool st_lld_is_alarm_active_n(unsigned alarm) {

  return (bool)((ST_TIM->DMAINTENR & (2U << alarm)) != 0U);
}

/**
 * @brief   Starts an additional alarm.
 *
 * @notapi
 */
static inline void st_lld_start_alarm_n(unsigned alarm, systime_t abstime) {

  osalDbgAssert(alarm < (unsigned)ST_LLD_NUM_ALARMS, "invalid alarm");
  osalDbgAssert(st_lld_is_alarm_active_n(alarm) == false, "already active");

  if (alarm == 1U) {
    ST_TIM->ST_REG_CH2CVR = abstime;
  }
  else if (alarm == 2U) {
    ST_TIM->ST_REG_CH3CVR = abstime;
  }
  else {
    ST_TIM->ST_REG_CH4CVR = abstime;
  }
  ST_TIM->INTFR = 0;
  ST_TIM->DMAINTENR |= 2U << alarm;
}

/**
 * @brief   Stops an additional alarm interrupt.
 *
 * @notapi
 */
static inline void st_lld_stop_alarm_n(unsigned alarm) {

  ST_TIM->DMAINTENR &= ~(2U << alarm);
}

/**
 * @brief   Sets an additional alarm time.
 *
 * @notapi
 */
static inline void st_lld_set_alarm_n(unsigned alarm, systime_t abstime) {

  if (alarm == 1U) {
    ST_TIM->ST_REG_CH2CVR = abstime;
  }
  else if (alarm == 2U) {
    ST_TIM->ST_REG_CH3CVR = abstime;
  }
  else {
    ST_TIM->ST_REG_CH4CVR = abstime;
  }
}

/**
 * @brief   Returns an additional alarm current time.
 *
 * @notapi
 */
static inline systime_t st_lld_get_alarm_n(unsigned alarm) {

  if (alarm == 1U) {
    return (systime_t)ST_TIM->ST_REG_CH2CVR;
  }
  if (alarm == 2U) {
    return (systime_t)ST_TIM->ST_REG_CH3CVR;
  }
  return (systime_t)ST_TIM->ST_REG_CH4CVR;
}
#endif /* ST_LLD_NUM_ALARMS > 1 */

#endif /* OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING */

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void st_lld_init(void);
  void st_lld_serve_interrupt(void);
#ifdef __cplusplus
}
#endif

#endif /* OSAL_ST_MODE != OSAL_ST_MODE_NONE */

#endif /* HAL_ST_LLD_H */

/** @} */
