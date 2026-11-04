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
 * @file    hal_st_lld.c
 * @brief   CH32H417 ST Driver low level driver source.
 *
 * @details Periodic system tick on the QingKe V5F core SysTick1 timer
 *          (IRQ13) or free running mode on a general purpose timer
 *          (see hal_st_lld.h).
 *
 * @addtogroup ST
 * @{
 */

#include "hal.h"

#if (OSAL_ST_MODE != OSAL_ST_MODE_NONE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#if OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC

#define STK_SR                              SysTick0->ISR
#define STK_SR_STK_ISR1                     (1UL << 1)

#define STK_CTLR_EN                         (1UL << 0)
#define STK_CTLR_STIE                       (1UL << 1)
#define STK_CTLR_HCLK                       (1UL << 2)
#define STK_CTLR_AUTO_RELOAD                (1UL << 3)
#define STK_CTLR_MODE_DOWN                  (1UL << 4)

#endif /* OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC */

/*===========================================================================*/
/* Driver local definitions for the free running timer mode.                 */
/*===========================================================================*/

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING

/* TIMx register bits (F1-compatible layout). */
#define CH32_TIM_CEN                        (1U << 0)
#define CH32_TIM_UIE                        (1U << 0)
#define CH32_TIM_UIF                        (1U << 0)
#define CH32_TIM_CC1IF                      (1U << 1)
#define CH32_TIM_CC2IF                      (1U << 5)
#define CH32_TIM_CC3IF                      (1U << 9)
#define CH32_TIM_CC4IF                      (1U << 13)
#define CH32_TIM_UG                         (1U << 0)
#define CH32_TIM_IRQ_MASK                   0x1FU

#endif /* OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING */

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   System timer interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(ST_HANDLER) {

  OSAL_IRQ_PROLOGUE();

  st_lld_serve_interrupt();

  OSAL_IRQ_EPILOGUE();
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level ST driver initialization.
 *
 * @notapi
 */
void st_lld_init(void) {

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING
  /* Free running counter mode on a general purpose timer.*/
  ST_ENABLE_CLOCK();

  ST_TIM->CTLR1 = 0;
  ST_TIM->CTLR2 = 0;
  ST_TIM->SMCFGR = 0;
  ST_TIM->PSC = (uint16_t)((ch32h417_timclk1 / OSAL_ST_FREQUENCY) - 1U);
  ST_TIM->ATRLR = 0xFFFFU;
  ST_TIM->CHCTLR1 = 0;
  ST_TIM->CH1CVR = 0;
  ST_TIM->CH2CVR = 0;
  ST_TIM->CH3CVR = 0;
  ST_TIM->CH4CVR = 0;
  ST_TIM->DMAINTENR = 0;
  ST_TIM->INTFR = 0;
  ST_TIM->SWEVGR = CH32_TIM_UG;
  ST_TIM->CTLR1 = CH32_TIM_CEN;

  NVIC_EnableIRQ(ST_NUMBER);
#endif /* OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING */

#if OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC
  uint32_t period = ch32h417_hclk / OSAL_ST_FREQUENCY;

  /* Periodic tick on the core SysTick1 timer (IRQ13). */
  SysTick1->CTLR = 0;
  STK_SR &= ~STK_SR_STK_ISR1;
  SysTick1->CMP = period;
  SysTick1->CNT = period;
  SysTick1->CTLR = STK_CTLR_MODE_DOWN | STK_CTLR_AUTO_RELOAD |
                   STK_CTLR_HCLK | STK_CTLR_STIE | STK_CTLR_EN;

  /* IRQ enabled, core private IRQ13. */
  NVIC_EnableIRQ(SysTick1_IRQn);
#endif /* OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC */
}

/**
 * @brief   IRQ handling code.
 *
 * @notapi
 */
void st_lld_serve_interrupt(void) {

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING
  uint32_t sr = ST_TIM->INTFR;
  uint32_t enabled = ST_TIM->DMAINTENR & CH32_TIM_IRQ_MASK;

  sr &= enabled;
  ST_TIM->INTFR = ~sr;

  if ((sr & CH32_TIM_CC1IF) != 0U)
#endif
  {
#if OSAL_ST_MODE == OSAL_ST_MODE_PERIODIC
    STK_SR &= ~STK_SR_STK_ISR1;
#endif
    osalSysLockFromISR();
    osalOsTimerHandlerI();
    osalSysUnlockFromISR();
  }

#if OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING
#if ST_LLD_NUM_ALARMS > 1
  if ((sr & CH32_TIM_CC2IF) != 0U) {
    if (st_callbacks[1] != NULL) {
      st_callbacks[1](1U);
    }
  }
#endif
#if ST_LLD_NUM_ALARMS > 2
  if ((sr & CH32_TIM_CC3IF) != 0U) {
    if (st_callbacks[2] != NULL) {
      st_callbacks[2](2U);
    }
  }
#endif
#if ST_LLD_NUM_ALARMS > 3
  if ((sr & CH32_TIM_CC4IF) != 0U) {
    if (st_callbacks[3] != NULL) {
      st_callbacks[3](3U);
    }
  }
#endif
#endif /* OSAL_ST_MODE == OSAL_ST_MODE_FREERUNNING */
}

#endif /* OSAL_ST_MODE != OSAL_ST_MODE_NONE */

/** @} */
