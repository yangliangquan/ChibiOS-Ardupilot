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
 * @file    USARTv1/hal_serial_lld.c
 * @brief   CH32H417 serial subsystem low level driver source.
 *
 * @details Interrupt driven engine (RXNE/TXE/TC), F1-compatible
 *          register model, 8N1 by default, up to eight units.
 *
 * @addtogroup SERIAL
 * @{
 */

#include "hal.h"

#if (HAL_USE_SERIAL == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_usart.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define USART_CTLR_RXNEIE                  (1U << 5)
#define USART_CTLR_TCIE                    (1U << 6)
#define USART_CTLR_TXEIE                   (1U << 7)

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables the bus clock and the interrupt vector of a unit.
 */
static void unit_enable(USART_TypeDef *u) {

  if (u == USART1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);
    NVIC_EnableIRQ(USART1_IRQn);
  }
#if CH32H417_SERIAL_USE_USART2
  else if (u == USART2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART2, ENABLE);
    NVIC_EnableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART3
  else if (u == USART3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART3, ENABLE);
    NVIC_EnableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART4
  else if (u == USART4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART4, ENABLE);
    NVIC_EnableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART5
  else if (u == USART5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART5, ENABLE);
    NVIC_EnableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART6
  else if (u == USART6) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART6, ENABLE);
    NVIC_EnableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART7
  else if (u == USART7) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART7, ENABLE);
    NVIC_EnableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART8
  else if (u == USART8) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART8, ENABLE);
    NVIC_EnableIRQ(USART8_IRQn);
  }
#endif
}

/**
 * @brief   Disables the interrupt vector of a unit.
 */
static void unit_disable(USART_TypeDef *u) {

  if (u == USART1) {
    NVIC_DisableIRQ(USART1_IRQn);
  }
#if CH32H417_SERIAL_USE_USART2
  else if (u == USART2) {
    NVIC_DisableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART3
  else if (u == USART3) {
    NVIC_DisableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART4
  else if (u == USART4) {
    NVIC_DisableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART5
  else if (u == USART5) {
    NVIC_DisableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART6
  else if (u == USART6) {
    NVIC_DisableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART7
  else if (u == USART7) {
    NVIC_DisableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_SERIAL_USE_USART8
  else if (u == USART8) {
    NVIC_DisableIRQ(USART8_IRQn);
  }
#endif
}

static void serve_interrupt(SerialDriver *sdp) {
  USART_TypeDef *u = sdp->usart;
  uint32_t sr = u->STATR;
  uint32_t cr1 = u->CTLR1;
  uint32_t errs = 0U;

  /* Reception, F1-style sequence: SR read then DATAR read. */
  if ((sr & USART_STATR_RXNE) != 0U) {
    uint16_t data = (uint16_t)u->DATAR;

    osalSysLockFromISR();
    if (iqPutI(&sdp->iqueue, (msg_t)data) < MSG_OK) {
      errs |= SD_OVERRUN_ERROR;
    }
    osalSysUnlockFromISR();
  }

  /* Transmission, buffer empty. */
  if (((cr1 & USART_CTLR_TXEIE) != 0U) && ((sr & USART_STATR_TXE) != 0U)) {
    msg_t b;

    osalSysLockFromISR();
    b = oqGetI(&sdp->oqueue);
    if (b < MSG_OK) {
      chnAddFlagsI(sdp, CHN_OUTPUT_EMPTY);
      cr1 &= ~(USART_CTLR_TXEIE | USART_CTLR_TCIE);
    }
    else {
      u->DATAR = (uint16_t)b;
    }
    osalSysUnlockFromISR();
  }

  /* Physical transmission end. */
  if (((cr1 & USART_CTLR_TCIE) != 0U) && ((sr & USART_STATR_TC) != 0U)) {
    osalSysLockFromISR();
    if (oqIsEmptyI(&sdp->oqueue)) {
      chnAddFlagsI(sdp, CHN_TRANSMISSION_END);
      cr1 &= ~USART_CTLR_TCIE;
    }
    osalSysUnlockFromISR();
  }

  /* Line errors. */
  if ((sr & USART_STATR_ORE) != 0U) {
    errs |= SD_OVERRUN_ERROR;
  }
  if ((sr & USART_STATR_FE) != 0U) {
    errs |= SD_FRAMING_ERROR;
  }
  if ((sr & USART_STATR_NE) != 0U) {
    errs |= SD_NOISE_ERROR;
  }
  if ((sr & USART_STATR_PE) != 0U) {
    errs |= SD_PARITY_ERROR;
  }

  u->CTLR1 = cr1;

  _serial_irq_code(sdp);

  if (errs != 0U) {
    osalSysLockFromISR();
    chnAddFlagsI(sdp, errs);
    osalSysUnlockFromISR();
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if CH32H417_SERIAL_USE_USART1 || defined(__DOXYGEN__)
static void notify1(io_queue_t *qp) {

  (void)qp;
  USART1->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD1);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART2 || defined(__DOXYGEN__)
static void notify2(io_queue_t *qp) {

  (void)qp;
  USART2->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD2);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART3 || defined(__DOXYGEN__)
static void notify3(io_queue_t *qp) {

  (void)qp;
  USART3->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD3);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART4 || defined(__DOXYGEN__)
static void notify4(io_queue_t *qp) {

  (void)qp;
  USART4->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD4);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART5 || defined(__DOXYGEN__)
static void notify5(io_queue_t *qp) {

  (void)qp;
  USART5->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART5 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD5);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART6 || defined(__DOXYGEN__)
static void notify6(io_queue_t *qp) {

  (void)qp;
  USART6->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART6 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD6);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART7 || defined(__DOXYGEN__)
static void notify7(io_queue_t *qp) {

  (void)qp;
  USART7->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART7 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD7);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SERIAL_USE_USART8 || defined(__DOXYGEN__)
static void notify8(io_queue_t *qp) {

  (void)qp;
  USART8->CTLR1 |= USART_CTLR_TXEIE | USART_CTLR_TCIE;
}

/**
 * @brief   USART8 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&SD8);

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level serial driver initialization.
 *
 * @notapi
 */
void sd_lld_init(void) {

#if CH32H417_SERIAL_USE_USART1 || defined(__DOXYGEN__)
  sdObjectInit(&SD1, NULL, notify1);
  SD1.usart = USART1;
#endif
#if CH32H417_SERIAL_USE_USART2 || defined(__DOXYGEN__)
  sdObjectInit(&SD2, NULL, notify2);
  SD2.usart = USART2;
#endif
#if CH32H417_SERIAL_USE_USART3 || defined(__DOXYGEN__)
  sdObjectInit(&SD3, NULL, notify3);
  SD3.usart = USART3;
#endif
#if CH32H417_SERIAL_USE_USART4 || defined(__DOXYGEN__)
  sdObjectInit(&SD4, NULL, notify4);
  SD4.usart = USART4;
#endif
#if CH32H417_SERIAL_USE_USART5 || defined(__DOXYGEN__)
  sdObjectInit(&SD5, NULL, notify5);
  SD5.usart = USART5;
#endif
#if CH32H417_SERIAL_USE_USART6 || defined(__DOXYGEN__)
  sdObjectInit(&SD6, NULL, notify6);
  SD6.usart = USART6;
#endif
#if CH32H417_SERIAL_USE_USART7 || defined(__DOXYGEN__)
  sdObjectInit(&SD7, NULL, notify7);
  SD7.usart = USART7;
#endif
#if CH32H417_SERIAL_USE_USART8 || defined(__DOXYGEN__)
  sdObjectInit(&SD8, NULL, notify8);
  SD8.usart = USART8;
#endif
}

/**
 * @brief   Low level serial driver configuration and start.
 *
 * @param[in] sdp       pointer to a @p SerialDriver object
 * @param[in] config    the architecture dependent chip configuration
 *
 * @notapi
 */
void sd_lld_start(SerialDriver *sdp, const SerialConfig *config) {
  USART_TypeDef *u = sdp->usart;
  USART_InitTypeDef init;

  unit_enable(u);

  init.USART_BaudRate = config->speed;
  init.USART_WordLength = USART_WordLength_8b;
  init.USART_StopBits = USART_StopBits_1;
  init.USART_Parity = USART_Parity_No;
  init.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
  init.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;

  USART_Init(u, &init);
  USART_Cmd(u, ENABLE);

  /* Receiver interrupt enabled. */
  sdp->config = config;
  u->CTLR2 |= config->cr2;
  u->CTLR3 |= config->cr3;
  u->CTLR1 |= config->cr1;
  u->CTLR1 |= USART_CTLR1_RXNEIE;
}

/**
 * @brief   Low level serial driver stop.
 *
 * @param[in] sdp       pointer to a @p SerialDriver object
 *
 * @notapi
 */
void sd_lld_stop(SerialDriver *sdp) {
  USART_TypeDef *u = sdp->usart;

  u->CTLR1 = 0;
  USART_Cmd(u, DISABLE);
  unit_disable(u);
}

#if CH32H417_SERIAL_USE_USART1 || defined(__DOXYGEN__)
SerialDriver SD1;
#endif
#if CH32H417_SERIAL_USE_USART2 || defined(__DOXYGEN__)
SerialDriver SD2;
#endif
#if CH32H417_SERIAL_USE_USART3 || defined(__DOXYGEN__)
SerialDriver SD3;
#endif
#if CH32H417_SERIAL_USE_USART4 || defined(__DOXYGEN__)
SerialDriver SD4;
#endif
#if CH32H417_SERIAL_USE_USART5 || defined(__DOXYGEN__)
SerialDriver SD5;
#endif
#if CH32H417_SERIAL_USE_USART6 || defined(__DOXYGEN__)
SerialDriver SD6;
#endif
#if CH32H417_SERIAL_USE_USART7 || defined(__DOXYGEN__)
SerialDriver SD7;
#endif
#if CH32H417_SERIAL_USE_USART8 || defined(__DOXYGEN__)
SerialDriver SD8;
#endif

#endif /* HAL_USE_SERIAL == TRUE */

/** @} */
