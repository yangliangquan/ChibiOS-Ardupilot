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
 * @file    TIMv1/hal_icu_lld.c
 * @brief   CH32H417 ICU subsystem low level driver source.
 *
 * @details Input capture on channel 1 of any capture-capable timer,
 *          timer clock HCLK, F1-compatible register model. An edge
 *          alternation state machine provides period/duty measurement.
 *
 * @addtogroup ICU
 * @{
 */

#include "hal.h"

#if (HAL_USE_ICU == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_tim.h"

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_ICU_USE_TIM1 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD1 = { .tim = TIM1 };
#endif
#if (CH32H417_ICU_USE_TIM2 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD2 = { .tim = TIM2 };
#endif
#if (CH32H417_ICU_USE_TIM3 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD3 = { .tim = TIM3 };
#endif
#if (CH32H417_ICU_USE_TIM4 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD4 = { .tim = TIM4 };
#endif
#if (CH32H417_ICU_USE_TIM5 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD5 = { .tim = TIM5 };
#endif
#if (CH32H417_ICU_USE_TIM8 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD8 = { .tim = TIM8 };
#endif
#if (CH32H417_ICU_USE_TIM9 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD9 = { .tim = TIM9 };
#endif
#if (CH32H417_ICU_USE_TIM10 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD10 = { .tim = TIM10 };
#endif
#if (CH32H417_ICU_USE_TIM11 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD11 = { .tim = TIM11 };
#endif
#if (CH32H417_ICU_USE_TIM12 == TRUE) && !defined(__DOXYGEN__)
ICUDriver ICUD12 = { .tim = TIM12 };
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static void unit_enable(TIM_TypeDef *t) {

#if CH32H417_ICU_USE_TIM1
  if (t == TIM1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM1, ENABLE);
    NVIC_EnableIRQ(TIM1_CC_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM2
  if (t == TIM2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM2, ENABLE);
    NVIC_EnableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM3
  if (t == TIM3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM3, ENABLE);
    NVIC_EnableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM4
  if (t == TIM4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM4, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM5
  if (t == TIM5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_TIM5, ENABLE);
    NVIC_EnableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM8
  if (t == TIM8) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM8, ENABLE);
    NVIC_EnableIRQ(TIM8_CC_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM9
  if (t == TIM9) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM9, ENABLE);
    NVIC_EnableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM10
  if (t == TIM10) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM10, ENABLE);
    NVIC_EnableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM11
  if (t == TIM11) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM11, ENABLE);
    NVIC_EnableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM12
  if (t == TIM12) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_TIM12, ENABLE);
    NVIC_EnableIRQ(TIM12_IRQn);
  }
#endif
}

static void unit_disable(TIM_TypeDef *t) {

#if CH32H417_ICU_USE_TIM1
  if (t == TIM1) {
    NVIC_DisableIRQ(TIM1_CC_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM2
  if (t == TIM2) {
    NVIC_DisableIRQ(TIM2_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM3
  if (t == TIM3) {
    NVIC_DisableIRQ(TIM3_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM4
  if (t == TIM4) {
    NVIC_DisableIRQ(TIM4_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM5
  if (t == TIM5) {
    NVIC_DisableIRQ(TIM5_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM8
  if (t == TIM8) {
    NVIC_DisableIRQ(TIM8_CC_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM9
  if (t == TIM9) {
    NVIC_DisableIRQ(TIM9_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM10
  if (t == TIM10) {
    NVIC_DisableIRQ(TIM10_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM11
  if (t == TIM11) {
    NVIC_DisableIRQ(TIM11_IRQn);
  }
#endif
#if CH32H417_ICU_USE_TIM12
  if (t == TIM12) {
    NVIC_DisableIRQ(TIM12_IRQn);
  }
#endif
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level ICU driver initialization.
 *
 * @notapi
 */
void icu_lld_init(void) {
}

/**
 * @brief   ICU driver configuration and start.
 *
 * @param[in] icup      pointer to an @p ICUDriver object
 *
 * @notapi
 */
void icu_lld_start(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;
  uint32_t pclk = CH32H417_TIMCLK1;
  uint32_t freq = icup->config->frequency;
  uint16_t ccmr;

  osalDbgAssert(freq > 0U, "zero frequency");
  osalDbgAssert((pclk % freq) == 0U, "clock rounding error");
  osalDbgAssert((pclk / freq) <= 0x10000U, "prescaler overflow");

  unit_enable(t);
  icup->clock = pclk;

  t->PSC = (uint16_t)(pclk / freq - 1U);
  t->ATRLR = 0xFFFFU;
  t->CCER = 0;

  /* Channel 1: input capture on TI1, prescaler 0, no filter, all
     channels captured from TI1. */
  ccmr = (0U << 2) | (0U << 4);
  t->CHCTLR1 = ccmr;

  /* Start in alternating-edge state: next capture is a rising edge. */
  icup->next_edge = 1U;

  t->CTLR1 = 0;
  t->DMAINTENR = 0;
}

/**
 * @brief   ICU driver stop.
 *
 * @param[in] icup      pointer to an @p ICUDriver object
 *
 * @notapi
 */
void icu_lld_stop(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;

  t->DMAINTENR = 0;
  t->CCER = 0;
  t->CTLR1 &= (uint16_t)~TIM_CEN;
  unit_disable(t);
}

/**
 * @brief   Starts the capture.
 *
 * @param[in] icup      pointer to an @p ICUDriver object
 *
 * @notapi
 */
void icu_lld_start_capture(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;
  const ICUConfig *cfg = icup->config;
  uint16_t ccmr;

  osalDbgAssert(cfg->callback != NULL, "no callback");
  osalDbgAssert((cfg->mode & ICU_USE_ICU) != 0U, "capture not enabled");

  ccmr = t->CHCTLR1 & 0xFFFCU;

  /* Enable capture on channel 1, edge according to the requested mode. */
  if ((cfg->mode & ICU_USE_BOTH_EDGES) == ICU_USE_BOTH_EDGES) {
    icup->next_edge = 1U;
    ccmr |= 0U;
  }
  else if ((cfg->mode & ICU_USE_FALLING_EDGE) != 0U) {
    icup->next_edge = 2U;
    ccmr |= 0U;
  }
  else {
    icup->next_edge = 1U;
    ccmr |= 0U;
  }
  t->CHCTLR1 = ccmr;

  /* Clear pending and enable. */
  t->INTFR = (uint16_t)~TIM_CC1IF;
  t->DMAINTENR |= TIM_CC1IE;

  if (icup->next_edge == 1U) {
    t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E;
  }
  else {
    t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E | TIM_CC1P;
  }

  t->CTLR1 |= TIM_CEN;
}

/**
 * @brief   Stops the capture.
 *
 * @param[in] icup      pointer to an @p ICUDriver object
 *
 * @notapi
 */
void icu_lld_stop_capture(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;

  t->DMAINTENR &= (uint16_t)~TIM_CC1IE;
  t->CCER &= (uint16_t)~(TIM_CC1E | TIM_CC1P);
}

/**
 * @brief   Polled wait for a completed capture.
 *
 * @return              The capture status.
 * @retval false        if the capture is successful.
 * @retval true         if a timer overflow occurred.
 *
 * @notapi
 */
bool icu_lld_wait_capture(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;

  for (;;) {
    uint16_t sr = t->INTFR;

    if ((sr & TIM_CC1IF) != 0U) {
      (void)t->CH1CVR;
      t->INTFR = (uint16_t)~TIM_CC1IF;
      return false;
    }
    if ((sr & TIM_UIF) != 0U) {
      t->INTFR = (uint16_t)~TIM_UIF;
      return true;
    }
  }
}

/**
 * @brief   Serves a capture interrupt event (shared by the per-unit
 *          vector wrappers).
 *
 * @param[in] icup      pointer to an @p ICUDriver object
 *
 * @notapi
 */
void icu_lld_serve_interrupt(ICUDriver *icup) {
  TIM_TypeDef *t = icup->tim;
  const ICUConfig *cfg = icup->config;

  if ((t->INTFR & TIM_CC1IF) == 0U) {
    return;
  }

  if ((cfg->mode & ICU_USE_BOTH_EDGES) == ICU_USE_BOTH_EDGES) {
    uint16_t value = t->CH1CVR;

    if (icup->next_edge == 1U) {
      /* Rising edge captured: period complete, next is falling. */
      icup->last = value;
      icup->next_edge = 2U;
      t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E | TIM_CC1P;
      icup->period = value;
      if (cfg->callback != NULL) {
        cfg->callback(icup);
      }
    }
    else {
      /* Falling edge captured: duty width within the period. */
      icup->next_edge = 1U;
      t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E;
      icup->width = (uint16_t)(value - icup->last);
    }
    t->INTFR = (uint16_t)~TIM_CC1IF;
  }
  else if ((cfg->mode & ICU_USE_PULSE_WIDTH) != 0U) {
    uint16_t value = t->CH1CVR;

    if (icup->next_edge == 1U) {
      icup->last = value;
      icup->next_edge = 2U;
      t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E | TIM_CC1P;
    }
    else {
      icup->next_edge = 1U;
      t->CCER = (t->CCER & 0xFFFCU) | TIM_CC1E;
      icup->width = (uint16_t)(value - icup->last);
    }
    t->INTFR = (uint16_t)~TIM_CC1IF;
    if (cfg->callback != NULL) {
      cfg->callback(icup);
    }
  }
  else {
    /* Single edge capture. */
    icup->period = t->CH1CVR;
    t->INTFR = (uint16_t)~TIM_CC1IF;
    if (cfg->callback != NULL) {
      cfg->callback(icup);
    }
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_ICU_USE_TIM1 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM1_CC_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD1);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM2 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD2);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM3 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD3);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM4 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD4);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM5 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD5);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM8 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM8_CC_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD8);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM9 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM9_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD9);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM10 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM10_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD10);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM11 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM11_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD11);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_ICU_USE_TIM12 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(TIM12_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  icu_lld_serve_interrupt(&ICUD12);
  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_ICU == TRUE */

/** @} */
