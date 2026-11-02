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
 * @file    TIMv1/hal_pwm_lld.h
 * @brief   CH32H417 PWM subsystem low level driver header.
 *
 * @addtogroup PWM
 * @{
 */

#ifndef HAL_PWM_LLD_H
#define HAL_PWM_LLD_H

#if (HAL_USE_PWM == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Number of PWM channels per PWM driver.
 */
#define PWM_CHANNELS                            4

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   PWMDn driver enable switches (PWMDn = TIMn, capture-capable
 *          timers only; TIM6/TIM7 are basic and excluded).
 */
#if !defined(CH32H417_PWM_USE_TIM1) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM1                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM2) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM2                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM3) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM3                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM4) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM4                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM5) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM5                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM8) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM8                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM9) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM9                  FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM10) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM10                 FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM11) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM11                 FALSE
#endif
#if !defined(CH32H417_PWM_USE_TIM12) || defined(__DOXYGEN__)
#define CH32H417_PWM_USE_TIM12                 FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_PWM_USE_TIM1 && !CH32H417_PWM_USE_TIM2 &&                    \
    !CH32H417_PWM_USE_TIM3 && !CH32H417_PWM_USE_TIM4 &&                    \
    !CH32H417_PWM_USE_TIM5 && !CH32H417_PWM_USE_TIM8 &&                    \
    !CH32H417_PWM_USE_TIM9 && !CH32H417_PWM_USE_TIM10 &&                   \
    !CH32H417_PWM_USE_TIM11 && !CH32H417_PWM_USE_TIM12
#error "PWM driver activated but no timer unit assigned"
#endif

/* Allocation conflict guards (shared with GPT/ICU/ST).*/
#if CH32H417_PWM_USE_TIM1
#if defined(CH32_TIM1_IS_USED)
#error "PWMD1 requires TIM1 but the timer is already used"
#else
#define CH32_TIM1_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM2
#if defined(CH32_TIM2_IS_USED)
#error "PWMD2 requires TIM2 but the timer is already used"
#else
#define CH32_TIM2_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM3
#if defined(CH32_TIM3_IS_USED)
#error "PWMD3 requires TIM3 but the timer is already used"
#else
#define CH32_TIM3_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM4
#if defined(CH32_TIM4_IS_USED)
#error "PWMD4 requires TIM4 but the timer is already used"
#else
#define CH32_TIM4_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM5
#if defined(CH32_TIM5_IS_USED)
#error "PWMD5 requires TIM5 but the timer is already used"
#else
#define CH32_TIM5_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM8
#if defined(CH32_TIM8_IS_USED)
#error "PWMD8 requires TIM8 but the timer is already used"
#else
#define CH32_TIM8_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM9
#if defined(CH32_TIM9_IS_USED)
#error "PWMD9 requires TIM9 but the timer is already used"
#else
#define CH32_TIM9_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM10
#if defined(CH32_TIM10_IS_USED)
#error "PWMD10 requires TIM10 but the timer is already used"
#else
#define CH32_TIM10_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM11
#if defined(CH32_TIM11_IS_USED)
#error "PWMD11 requires TIM11 but the timer is already used"
#else
#define CH32_TIM11_IS_USED
#endif
#endif
#if CH32H417_PWM_USE_TIM12
#if defined(CH32_TIM12_IS_USED)
#error "PWMD12 requires TIM12 but the timer is already used"
#else
#define CH32_TIM12_IS_USED
#endif
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of a PWM mode.
 */
typedef uint32_t pwmmode_t;

/**
 * @brief   Type of a PWM channel.
 */
typedef uint8_t pwmchannel_t;

/**
 * @brief   Type of a channels mask.
 */
typedef uint32_t pwmchnmsk_t;

/**
 * @brief   Type of a PWM counter.
 */
typedef uint32_t pwmcnt_t;

/**
 * @brief   Type of a PWM driver channel configuration structure.
 */
typedef struct {
  /**
   * @brief Channel active logic level.
   */
  pwmmode_t                 mode;
  /**
   * @brief Channel callback pointer.
   */
  pwmcallback_t             callback;
  /* End of the mandatory fields.*/
} PWMChannelConfig;

/**
 * @brief   Type of a PWM driver configuration structure.
 */
typedef struct {
  /**
   * @brief   Timer clock in Hz.
   */
  uint32_t                  frequency;
  /**
   * @brief   PWM period in ticks.
   */
  pwmcnt_t                  period;
  /**
   * @brief Periodic callback pointer.
   */
  pwmcallback_t             callback;
  /**
   * @brief Channels configurations.
   */
  PWMChannelConfig          channels[PWM_CHANNELS];
  /* End of the mandatory fields.*/
} PWMConfig;

/**
 * @brief   Low level fields of the PWM driver structure.
 */
#define PWM_DRIVER_EXT_FIELDS                                              \
  /* Pointer to the timer registers block.*/                                \
  TIM_TypeDef               *tim;                                           \
  /* Timer clock in Hz.*/                                                   \
  uint32_t                  clock;

/**
 * @brief   Structure representing a PWM driver.
 */
struct PWMDriver {
  /**
   * @brief Driver state.
   */
  pwmstate_t                state;
  /**
   * @brief Current driver configuration data.
   */
  const PWMConfig           *config;
  /**
   * @brief   Current PWM period in ticks.
   */
  pwmcnt_t                  period;
  /**
   * @brief   Mask of the enabled channels.
   */
  pwmchnmsk_t               enabled;
  /**
   * @brief   Number of channels in this instance.
   */
  pwmchannel_t              channels;
  PWM_DRIVER_EXT_FIELDS
  /* End of the mandatory fields.*/
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Changes the period of the PWM peripheral.
 */
#define pwm_lld_change_period(pwmp, period) do {                            \
  (pwmp)->period = (pwmcnt_t)(period);                                      \
  (pwmp)->tim->ATRLR = (uint16_t)((period) - 1U);                           \
} while (0)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_PWM_USE_TIM1 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD1;
#endif
#if (CH32H417_PWM_USE_TIM2 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD2;
#endif
#if (CH32H417_PWM_USE_TIM3 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD3;
#endif
#if (CH32H417_PWM_USE_TIM4 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD4;
#endif
#if (CH32H417_PWM_USE_TIM5 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD5;
#endif
#if (CH32H417_PWM_USE_TIM8 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD8;
#endif
#if (CH32H417_PWM_USE_TIM9 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD9;
#endif
#if (CH32H417_PWM_USE_TIM10 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD10;
#endif
#if (CH32H417_PWM_USE_TIM11 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD11;
#endif
#if (CH32H417_PWM_USE_TIM12 == TRUE) && !defined(__DOXYGEN__)
extern PWMDriver PWMD12;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void pwm_lld_init(void);
  void pwm_lld_start(PWMDriver *pwmp);
  void pwm_lld_stop(PWMDriver *pwmp);
  void pwm_lld_enable_channel(PWMDriver *pwmp,
                              pwmchannel_t channel,
                              pwmcnt_t width);
  void pwm_lld_disable_channel(PWMDriver *pwmp, pwmchannel_t channel);
  void pwm_lld_enable_periodic_notification(PWMDriver *pwmp);
  void pwm_lld_disable_periodic_notification(PWMDriver *pwmp);
  void pwm_lld_enable_channel_notification(PWMDriver *pwmp,
                                           pwmchannel_t channel);
  void pwm_lld_disable_channel_notification(PWMDriver *pwmp,
                                            pwmchannel_t channel);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_PWM == TRUE */

#endif /* HAL_PWM_LLD_H */

/** @} */
