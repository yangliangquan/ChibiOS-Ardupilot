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
 * @file    TIMv1/hal_pwm_lld.c
 * @brief   CH32H417 PWM subsystem low level driver source.
 *
 * @details Edge-aligned PWM mode 1 on the TIM1 advanced timer,
 *          channels 1..4, main output enabled via BDTR.MOE.
 *
 * @addtogroup PWM
 * @{
 */

#include "hal.h"

#if (HAL_USE_PWM == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define CH32_TIM_CEN                        (1U << 0)
#define CH32_TIM_ARPE                       (1U << 7)
#define CH32_TIM_UIE                        (1U << 0)
#define CH32_TIM_UIF                        (1U << 0)
#define CH32_TIM_MOE                        (1U << 15)

#define CH32_TIM_CCxE_MASK(ch)              (1U << ((ch) * 4))
#define CH32_TIM_CCxP_MASK(ch)              (1U << (((ch) * 4) + 1))
#define CH32_TIM_CCxIF_MASK(ch)             (1U << (1 + ((ch) * 4)))
#define CH32_TIM_CCxIE_BIT(ch)              (1 + (ch))

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_PWM_USE_TIM1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   PWMD1 driver identifier (TIM1 unit).
 */
PWMDriver PWMD1 = {
  .state = PWM_STOP,
  .config = NULL,
  .period = 0U,
  .enabled = 0U,
  .channels = PWM_CHANNELS,
  .tim = TIM1,
  .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM2 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD2 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM2, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM3 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD3 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM3, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM4 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD4 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM4, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM5 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD5 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM5, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM8 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD8 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM8, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM9 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD9 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM9, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM10 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD10 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM10, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM11 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD11 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM11, .clock = CH32H417_TIMCLK1
};
#endif
#if (CH32H417_PWM_USE_TIM12 == TRUE) && !defined(__DOXYGEN__)
PWMDriver PWMD12 = {
  .state = PWM_STOP, .config = NULL, .period = 0U, .enabled = 0U,
  .channels = PWM_CHANNELS, .tim = TIM12, .clock = CH32H417_TIMCLK1
};
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables bus clock and interrupt vectors of a PWM timer.
 */
static void unit_enable(TIM_TypeDef *t) {

#if CH32H417_PWM_USE_TIM1
  if (t == TIM1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM1, ENABLE);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM2
  if (t == TIM2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM2, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM3
  if (t == TIM3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM3, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM4
  if (t == TIM4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM4, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM5
  if (t == TIM5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM5, ENABLE);
    NVIC_EnableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM8
  if (t == TIM8) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM8, ENABLE);
    NVIC_EnableIRQ(TIM8_UP_IRQn);
    NVIC_EnableIRQ(TIM8_CC_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM9
  if (t == TIM9) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM9, ENABLE);
    NVIC_EnableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM10
  if (t == TIM10) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM10, ENABLE);
    NVIC_EnableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM11
  if (t == TIM11) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM11, ENABLE);
    NVIC_EnableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM12
  if (t == TIM12) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE);
    NVIC_EnableIRQ(TIM12_IRQn);
  }
#endif
}

/**
 * @brief   Disables the interrupt vectors of a PWM timer.
 */
static void unit_disable(TIM_TypeDef *t) {

#if CH32H417_PWM_USE_TIM1
  if (t == TIM1) {
    NVIC_DisableIRQ(TIM1_UP_IRQn);
    NVIC_DisableIRQ(TIM1_CC_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM2
  if (t == TIM2) {
    NVIC_DisableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM3
  if (t == TIM3) {
    NVIC_DisableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM4
  if (t == TIM4) {
    NVIC_DisableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM5
  if (t == TIM5) {
    NVIC_DisableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM8
  if (t == TIM8) {
    NVIC_DisableIRQ(TIM8_UP_IRQn);
    NVIC_DisableIRQ(TIM8_CC_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM9
  if (t == TIM9) {
    NVIC_DisableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM10
  if (t == TIM10) {
    NVIC_DisableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM11
  if (t == TIM11) {
    NVIC_DisableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_PWM_USE_TIM12
  if (t == TIM12) {
    NVIC_DisableIRQ(TIM12_IRQn);
  }
#endif
}

static void set_channel_mode(TIM_TypeDef *timp,
                             pwmchannel_t channel,
                             pwmmode_t mode) {
  uint16_t ccmr;
  uint32_t shift = (uint32_t)(channel & 1U) * 8U;
  volatile uint16_t *ccmrp = (channel < 2U) ? &timp->CHCTLR1 : &timp->CHCTLR2;

  if (mode == PWM_OUTPUT_DISABLED) {
    timp->CCER &= (uint16_t)~(CH32_TIM_CCxE_MASK(channel) |
                              CH32_TIM_CCxP_MASK(channel));
    return;
  }

  /* PWM mode 1 with output preload enabled. */
  ccmr = *ccmrp;
  ccmr &= (uint16_t)~(0x0007U << (shift + 4U));
  ccmr |= (uint16_t)(0x0006U << (shift + 4U));
  ccmr |= (uint16_t)(1U << (shift + 3U));
  *ccmrp = ccmr;

  /* Polarity. */
  if (mode == PWM_OUTPUT_ACTIVE_LOW) {
    timp->CCER |= (uint16_t)CH32_TIM_CCxP_MASK(channel);
  }
  else {
    timp->CCER &= (uint16_t)~CH32_TIM_CCxP_MASK(channel);
  }

  timp->CCER |= (uint16_t)CH32_TIM_CCxE_MASK(channel);
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level PWM driver initialization.
 *
 * @notapi
 */
void pwm_lld_init(void) {
}

/**
 * @brief   PWM driver configuration and start.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 *
 * @notapi
 */
void pwm_lld_start(PWMDriver *pwmp) {
  TIM_TypeDef *timp = pwmp->tim;
  uint32_t prescaler;

  unit_enable(timp);

  osalDbgAssert((pwmp->clock % pwmp->config->frequency) == 0U,
                "timer clock rounding error");
  prescaler = (pwmp->clock / pwmp->config->frequency) - 1U;
  osalDbgAssert(prescaler <= 0xFFFFU, "prescaler overflow");
  osalDbgAssert((pwmp->config->period >= 1U) &&
                (pwmp->config->period <= 0x10000U),
                "invalid period");

  /* Upcounting, auto-reload preload enabled. */
  timp->CTLR1 = CH32_TIM_ARPE;
  timp->CTLR2 = 0;
  timp->SMCFGR = 0;
  timp->PSC = (uint16_t)prescaler;
  timp->ATRLR = (uint16_t)(pwmp->config->period - 1U);
  timp->CNT = 0;
  timp->CHCTLR1 = 0;
  timp->CHCTLR2 = 0;
  timp->CCER = 0;
  timp->DMAINTENR = 0;
  timp->INTFR = 0;

  /* Advanced timer main output enable. */
  timp->BDTR |= (uint16_t)CH32_TIM_MOE;

  /* Counter start, the outputs are gated by the CCxE bits of CCER until a
     channel is enabled. Without it the timer never counts, no compare
     event is ever generated and the periodic notification cannot fire. */
  timp->CTLR1 |= (uint16_t)CH32_TIM_CEN;
}

/**
 * @brief   PWM driver stop.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 *
 * @notapi
 */
void pwm_lld_stop(PWMDriver *pwmp) {
  TIM_TypeDef *timp = pwmp->tim;

  timp->CTLR1 &= (uint16_t)~CH32_TIM_CEN;
  timp->CCER = 0;
  timp->DMAINTENR = 0;
  timp->BDTR &= (uint16_t)~CH32_TIM_MOE;
  unit_disable(timp);
}

/**
 * @brief   Enables a PWM channel.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 * @param[in] channel   PWM channel identifier
 * @param[in] width     PWM pulse width in ticks
 *
 * @notapi
 */
void pwm_lld_enable_channel(PWMDriver *pwmp,
                            pwmchannel_t channel,
                            pwmcnt_t width) {
  TIM_TypeDef *timp = pwmp->tim;
  pwmmode_t mode = pwmp->config->channels[channel].mode;

  osalDbgAssert(channel < PWM_CHANNELS, "invalid channel");

  set_channel_mode(timp, channel, mode);

  switch (channel) {
  case 0:
    timp->CH1CVR = (uint16_t)width;
    break;
  case 1:
    timp->CH2CVR = (uint16_t)width;
    break;
  case 2:
    timp->CH3CVR = (uint16_t)width;
    break;
  case 3:
    timp->CH4CVR = (uint16_t)width;
    break;
  default:
    break;
  }
}

/**
 * @brief   Disables a PWM channel.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 * @param[in] channel   PWM channel identifier
 *
 * @notapi
 */
void pwm_lld_disable_channel(PWMDriver *pwmp, pwmchannel_t channel) {

  osalDbgAssert(channel < PWM_CHANNELS, "invalid channel");

  set_channel_mode(pwmp->tim, channel, PWM_OUTPUT_DISABLED);
}

/**
 * @brief   Enables the periodic notification.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 *
 * @notapi
 */
void pwm_lld_enable_periodic_notification(PWMDriver *pwmp) {

  pwmp->tim->DMAINTENR |= CH32_TIM_UIE;
}

/**
 * @brief   Disables the periodic notification.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 *
 * @notapi
 */
void pwm_lld_disable_periodic_notification(PWMDriver *pwmp) {

  pwmp->tim->DMAINTENR &= (uint16_t)~CH32_TIM_UIE;
}

/**
 * @brief   Enables a channel notification.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 * @param[in] channel   PWM channel identifier
 *
 * @notapi
 */
void pwm_lld_enable_channel_notification(PWMDriver *pwmp,
                                         pwmchannel_t channel) {

  osalDbgAssert(channel < PWM_CHANNELS, "invalid channel");

  pwmp->tim->DMAINTENR |= (uint16_t)(1U << CH32_TIM_CCxIE_BIT(channel));
}

/**
 * @brief   Disables a channel notification.
 *
 * @param[in] pwmp      pointer to a @p PWMDriver object
 * @param[in] channel   PWM channel identifier
 *
 * @notapi
 */
void pwm_lld_disable_channel_notification(PWMDriver *pwmp,
                                          pwmchannel_t channel) {

  osalDbgAssert(channel < PWM_CHANNELS, "invalid channel");

  pwmp->tim->DMAINTENR &=
      (uint16_t)~(1U << CH32_TIM_CCxIE_BIT(channel));
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

static void serve_periodic(PWMDriver *pwmp) {

  pwmp->tim->INTFR = (uint16_t)~CH32_TIM_UIF;
  if (pwmp->config->callback != NULL) {
    pwmp->config->callback(pwmp);
  }
}

static void serve_channels(PWMDriver *pwmp) {
  uint32_t sr = (uint32_t)pwmp->tim->INTFR;
  pwmchannel_t ch;

  for (ch = 0; ch < PWM_CHANNELS; ch++) {
    if (((pwmp->enabled & (1UL << ch)) != 0U) &&
        ((sr & CH32_TIM_CCxIF_MASK(ch)) != 0U)) {
      pwmp->tim->INTFR &= (uint16_t)~CH32_TIM_CCxIF_MASK(ch);
      if (pwmp->config->channels[ch].callback != NULL) {
        pwmp->config->channels[ch].callback(pwmp);
      }
    }
  }
}

#if (CH32H417_PWM_USE_TIM1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   TIM1 update interrupt handler (periodic notification).
 *
 * @isr
 */
OSAL_IRQ_HANDLER(TIM1_UP_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD1);
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   TIM1 capture/compare interrupt handler (channel notifications).
 *
 * @isr
 */
OSAL_IRQ_HANDLER(TIM1_CC_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_channels(&PWMD1);
  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_PWM_USE_TIM8 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   TIM8 update interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(TIM8_UP_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD8);
  OSAL_IRQ_EPILOGUE();
}

/**
 * @brief   TIM8 capture/compare interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(TIM8_CC_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_channels(&PWMD8);
  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_PWM_USE_TIM2 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD2);
  serve_channels(&PWMD2);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM3 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD3);
  serve_channels(&PWMD3);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM4 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD4);
  serve_channels(&PWMD4);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM5 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD5);
  serve_channels(&PWMD5);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM9 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM9_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD9);
  serve_channels(&PWMD9);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM10 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM10_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD10);
  serve_channels(&PWMD10);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM11 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM11_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD11);
  serve_channels(&PWMD11);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_PWM_USE_TIM12 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM12_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_periodic(&PWMD12);
  serve_channels(&PWMD12);
  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_PWM == TRUE */

/** @} */
