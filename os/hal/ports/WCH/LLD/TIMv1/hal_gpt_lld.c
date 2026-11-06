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
 * @file    TIMv1/hal_gpt_lld.c
 * @brief   CH32H417 GPT subsystem low level driver source.
 *
 * @details Any TIM1..TIM12 as periodic one-shot timer, clock source
 *          HCLK, F1-compatible register model.
 *
 * @addtogroup GPT
 * @{
 */

#include "hal.h"

#if (HAL_USE_GPT == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_tim.h"

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_GPT_USE_TIM1 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD1 = { .tim = TIM1 };
#endif
#if (CH32H417_GPT_USE_TIM2 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD2 = { .tim = TIM2 };
#endif
#if (CH32H417_GPT_USE_TIM3 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD3 = { .tim = TIM3 };
#endif
#if (CH32H417_GPT_USE_TIM4 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD4 = { .tim = TIM4 };
#endif
#if (CH32H417_GPT_USE_TIM5 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD5 = { .tim = TIM5 };
#endif
#if (CH32H417_GPT_USE_TIM6 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD6 = { .tim = TIM6 };
#endif
#if (CH32H417_GPT_USE_TIM7 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD7 = { .tim = TIM7 };
#endif
#if (CH32H417_GPT_USE_TIM8 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD8 = { .tim = TIM8 };
#endif
#if (CH32H417_GPT_USE_TIM9 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD9 = { .tim = TIM9 };
#endif
#if (CH32H417_GPT_USE_TIM10 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD10 = { .tim = TIM10 };
#endif
#if (CH32H417_GPT_USE_TIM11 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD11 = { .tim = TIM11 };
#endif
#if (CH32H417_GPT_USE_TIM12 == TRUE) && !defined(__DOXYGEN__)
GPTDriver GPTD12 = { .tim = TIM12 };
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables bus clock and update IRQ vector of a timer.
 */
static void unit_enable(TIM_TypeDef *t) {

#if CH32H417_GPT_USE_TIM1
  if (t == TIM1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM1, ENABLE);
    NVIC_EnableIRQ(TIM1_UP_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM2
  if (t == TIM2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM2, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM3
  if (t == TIM3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM3, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM4
  if (t == TIM4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM4, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM5
  if (t == TIM5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM5, ENABLE);
    NVIC_EnableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM6
  if (t == TIM6) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM6, ENABLE);
    NVIC_EnableIRQ(TIM6_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM7
  if (t == TIM7) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM7, ENABLE);
    NVIC_EnableIRQ(TIM7_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM8
  if (t == TIM8) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM8, ENABLE);
    NVIC_EnableIRQ(TIM8_UP_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM9
  if (t == TIM9) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM9, ENABLE);
    NVIC_EnableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM10
  if (t == TIM10) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM10, ENABLE);
    NVIC_EnableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM11
  if (t == TIM11) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM11, ENABLE);
    NVIC_EnableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM12
  if (t == TIM12) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE);
    NVIC_EnableIRQ(TIM12_IRQn);
  }
#endif
}

/**
 * @brief   Disables the update IRQ vector of a timer.
 */
static void unit_disable(TIM_TypeDef *t) {

#if CH32H417_GPT_USE_TIM1
  if (t == TIM1) {
    NVIC_DisableIRQ(TIM1_UP_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM2
  if (t == TIM2) {
    NVIC_DisableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM3
  if (t == TIM3) {
    NVIC_DisableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM4
  if (t == TIM4) {
    NVIC_DisableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM5
  if (t == TIM5) {
    NVIC_DisableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM6
  if (t == TIM6) {
    NVIC_DisableIRQ(TIM6_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM7
  if (t == TIM7) {
    NVIC_DisableIRQ(TIM7_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM8
  if (t == TIM8) {
    NVIC_DisableIRQ(TIM8_UP_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM9
  if (t == TIM9) {
    NVIC_DisableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM10
  if (t == TIM10) {
    NVIC_DisableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM11
  if (t == TIM11) {
    NVIC_DisableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_GPT_USE_TIM12
  if (t == TIM12) {
    NVIC_DisableIRQ(TIM12_IRQn);
  }
#endif
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level GPT driver initialization.
 *
 * @notapi
 */
void gpt_lld_init(void) {

#if CH32H417_GPT_USE_TIM1 == TRUE
  gptObjectInit(&GPTD1);
#endif
#if CH32H417_GPT_USE_TIM2 == TRUE
  gptObjectInit(&GPTD2);
#endif
#if CH32H417_GPT_USE_TIM3 == TRUE
  gptObjectInit(&GPTD3);
#endif
#if CH32H417_GPT_USE_TIM4 == TRUE
  gptObjectInit(&GPTD4);
#endif
#if CH32H417_GPT_USE_TIM5 == TRUE
  gptObjectInit(&GPTD5);
#endif
#if CH32H417_GPT_USE_TIM6 == TRUE
  gptObjectInit(&GPTD6);
#endif
#if CH32H417_GPT_USE_TIM7 == TRUE
  gptObjectInit(&GPTD7);
#endif
#if CH32H417_GPT_USE_TIM8 == TRUE
  gptObjectInit(&GPTD8);
#endif
#if CH32H417_GPT_USE_TIM9 == TRUE
  gptObjectInit(&GPTD9);
#endif
#if CH32H417_GPT_USE_TIM10 == TRUE
  gptObjectInit(&GPTD10);
#endif
#if CH32H417_GPT_USE_TIM11 == TRUE
  gptObjectInit(&GPTD11);
#endif
#if CH32H417_GPT_USE_TIM12 == TRUE
  gptObjectInit(&GPTD12);
#endif
}

/**
 * @brief   GPT driver configuration and start.
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 *
 * @notapi
 */
void gpt_lld_start(GPTDriver *gptp) {
  TIM_TypeDef *t = gptp->tim;
  uint32_t pclk = CH32H417_TIMCLK1;
  uint32_t freq = gptp->config->frequency;

  osalDbgAssert(freq > 0U, "zero frequency");
  osalDbgAssert((pclk % freq) == 0U, "clock rounding error");
  osalDbgAssert((pclk / freq) <= 0x10000U, "prescaler overflow");

  unit_enable(t);
  gptp->clock = pclk;

  t->PSC = (uint16_t)(pclk / freq - 1U);
  t->CTLR1 = 0;
  t->DMAINTENR = 0;
  t->SWEVGR = TIM_UG;
}

/**
 * @brief   GPT driver stop.
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 *
 * @notapi
 */
void gpt_lld_stop(GPTDriver *gptp) {

  gptp->tim->DMAINTENR = 0;
  gptp->tim->CTLR1 &= (uint16_t)~TIM_CEN;
  unit_disable(gptp->tim);
}

/**
 * @brief   Starts the timer in one-shot mode.
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 * @param[in] period    the time count
 *
 * @notapi
 */
void gpt_lld_start_timer(GPTDriver *gptp, gptcnt_t period) {
  TIM_TypeDef *t = gptp->tim;

  /* gptcnt_t is 16-bit, matching the 16-bit timers, so the type
     itself enforces the upper bound; only zero has to be rejected. */
  osalDbgAssert(period > 0U, "invalid period");

  t->ATRLR = (uint16_t)(period - 1U);
  t->CNT = 0;
  t->DMAINTENR |= TIM_UIE;
  t->CTLR1 |= TIM_CEN;
}

/**
 * @brief   Stops the timer.
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 *
 * @notapi
 */
void gpt_lld_stop_timer(GPTDriver *gptp) {
  TIM_TypeDef *t = gptp->tim;

  t->CTLR1 &= (uint16_t)~TIM_CEN;
  t->DMAINTENR &= (uint16_t)~TIM_UIE;
}

/**
 * @brief   Polled delay.
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 * @param[in] interval  the number of ticks to wait
 *
 * @notapi
 */
void gpt_lld_polled_delay(GPTDriver *gptp, gptcnt_t interval) {
  TIM_TypeDef *t = gptp->tim;
  uint16_t start = t->CNT;

  while ((uint16_t)(t->CNT - start) < interval) {
  }
}

/**
 * @brief   Serves a timer interrupt event (shared by the per-unit
 *          vector wrappers).
 *
 * @param[in] gptp      pointer to a @p GPTDriver object
 *
 * @notapi
 */
void gpt_lld_serve_interrupt(GPTDriver *gptp) {
  TIM_TypeDef *t = gptp->tim;

  if ((t->INTFR & TIM_UIF) != 0U) {
    t->INTFR &= (uint16_t)~TIM_UIF;

    if (gptp->config != NULL) {
      if (gptp->config->callback != NULL) {
        gptp->config->callback(gptp);
      }
      else {
        t->DMAINTENR &= (uint16_t)~TIM_UIE;
        t->CTLR1 &= (uint16_t)~TIM_CEN;
      }
    }
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_GPT_USE_TIM1 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM1_UP_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD1);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM2 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD2);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM3 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD3);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM4 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD4);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM5 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD5);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM6 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD6);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM7 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD7);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM8 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM8_UP_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD8);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM9 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM9_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD9);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM10 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM10_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD10);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM11 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM11_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD11);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_GPT_USE_TIM12 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM12_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  gpt_lld_serve_interrupt(&GPTD12);
  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_GPT == TRUE */

/** @} */
