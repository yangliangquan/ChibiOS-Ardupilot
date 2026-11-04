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
 * @file    USARTv1/hal_uart_lld.c
 * @brief   CH32H417 UART subsystem low level driver source.
 *
 * @details DMA driven engine over the F1-compatible USART register model,
 *          eight units (USART1..USART8). Both directions use channels taken
 *          from the free pool and routed through the DMAMUX, the DMA mode
 *          and the peripheral request are the only channel settings that
 *          matter.
 *
 *          Receiver idle state: a circular DMA channel of one byte refills
 *          @p rxbuf and raises a transfer complete interrupt per received
 *          frame, that is what drives @p rxchar_cb. The CPU never touches
 *          @p DATAR while the DMA is running.
 *
 *          The peripheral has no write-one-to-clear status register, the
 *          error flags are reset by a @p STATR read followed by a @p DATAR
 *          read (RM 19.11.1). In an error condition the byte eventually
 *          consumed by that read is lost, the data was already compromised
 *          by the very error being handled.
 *
 * @addtogroup UART
 * @{
 */

#include "hal.h"

#if (HAL_USE_UART == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define UART_CTLR1_ENFORCED                  (USART_CTLR1_UE |               \
                                              USART_CTLR1_TE |               \
                                              USART_CTLR1_RE)

#define UART_CTLR3_ENFORCED                  (USART_CTLR3_DMAR |             \
                                              USART_CTLR3_DMAT |             \
                                              USART_CTLR3_EIE)

#define UART_STATR_ERRORS                    (USART_STATR_PE |               \
                                              USART_STATR_FE |               \
                                              USART_STATR_NE |               \
                                              USART_STATR_ORE)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_UART_USE_USART1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART1 UART driver identifier.
 */
UARTDriver UARTD1;
#endif
#if (CH32H417_UART_USE_USART2 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART2 UART driver identifier.
 */
UARTDriver UARTD2;
#endif
#if (CH32H417_UART_USE_USART3 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART3 UART driver identifier.
 */
UARTDriver UARTD3;
#endif
#if (CH32H417_UART_USE_USART4 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART4 UART driver identifier.
 */
UARTDriver UARTD4;
#endif
#if (CH32H417_UART_USE_USART5 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART5 UART driver identifier.
 */
UARTDriver UARTD5;
#endif
#if (CH32H417_UART_USE_USART6 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART6 UART driver identifier.
 */
UARTDriver UARTD6;
#endif
#if (CH32H417_UART_USE_USART7 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART7 UART driver identifier.
 */
UARTDriver UARTD7;
#endif
#if (CH32H417_UART_USE_USART8 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART8 UART driver identifier.
 */
UARTDriver UARTD8;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables the bus clock and the interrupt vector of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 * @param[in] prio      PFIC priority field
 */
static void unit_enable(USART_TypeDef *u, uint32_t prio) {

  if (u == USART1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);
    NVIC_SetPriority(USART1_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_EnableIRQ(USART1_IRQn);
  }
#if CH32H417_UART_USE_USART2 == TRUE
  else if (u == USART2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART2, ENABLE);
    NVIC_SetPriority(USART2_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART2_IRQn);
    NVIC_EnableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART3 == TRUE
  else if (u == USART3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART3, ENABLE);
    NVIC_SetPriority(USART3_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART3_IRQn);
    NVIC_EnableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART4 == TRUE
  else if (u == USART4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART4, ENABLE);
    NVIC_SetPriority(USART4_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART4_IRQn);
    NVIC_EnableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART5 == TRUE
  else if (u == USART5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART5, ENABLE);
    NVIC_SetPriority(USART5_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART5_IRQn);
    NVIC_EnableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART6 == TRUE
  else if (u == USART6) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART6, ENABLE);
    NVIC_SetPriority(USART6_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART6_IRQn);
    NVIC_EnableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART7 == TRUE
  else if (u == USART7) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART7, ENABLE);
    NVIC_SetPriority(USART7_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART7_IRQn);
    NVIC_EnableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART8 == TRUE
  else if (u == USART8) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART8, ENABLE);
    NVIC_SetPriority(USART8_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_ClearPendingIRQ(USART8_IRQn);
    NVIC_EnableIRQ(USART8_IRQn);
  }
#endif
}

/**
 * @brief   Disables the interrupt vector of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 */
static void unit_disable(USART_TypeDef *u) {

  if (u == USART1) {
    NVIC_DisableIRQ(USART1_IRQn);
  }
#if CH32H417_UART_USE_USART2 == TRUE
  else if (u == USART2) {
    NVIC_DisableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART3 == TRUE
  else if (u == USART3) {
    NVIC_DisableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART4 == TRUE
  else if (u == USART4) {
    NVIC_DisableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART5 == TRUE
  else if (u == USART5) {
    NVIC_DisableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART6 == TRUE
  else if (u == USART6) {
    NVIC_DisableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART7 == TRUE
  else if (u == USART7) {
    NVIC_DisableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_UART_USE_USART8 == TRUE
  else if (u == USART8) {
    NVIC_DisableIRQ(USART8_IRQn);
  }
#endif
}

/**
 * @brief   Returns the IRQ and DMA priority of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 * @param[out] irqprio  pointer to the interrupt priority
 * @param[out] dmaprio  pointer to the DMA priority
 */
static void unit_priorities(USART_TypeDef *u, uint32_t *irqprio,
                            uint32_t *dmaprio) {

  if (u == USART1) {
    *irqprio = CH32H417_UART_USART1_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART1_DMA_PRIORITY;
  }
#if CH32H417_UART_USE_USART2 == TRUE
  else if (u == USART2) {
    *irqprio = CH32H417_UART_USART2_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART2_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART3 == TRUE
  else if (u == USART3) {
    *irqprio = CH32H417_UART_USART3_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART3_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART4 == TRUE
  else if (u == USART4) {
    *irqprio = CH32H417_UART_USART4_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART4_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART5 == TRUE
  else if (u == USART5) {
    *irqprio = CH32H417_UART_USART5_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART5_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART6 == TRUE
  else if (u == USART6) {
    *irqprio = CH32H417_UART_USART6_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART6_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART7 == TRUE
  else if (u == USART7) {
    *irqprio = CH32H417_UART_USART7_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART7_DMA_PRIORITY;
  }
#endif
#if CH32H417_UART_USE_USART8 == TRUE
  else if (u == USART8) {
    *irqprio = CH32H417_UART_USART8_IRQ_PRIORITY;
    *dmaprio = CH32H417_UART_USART8_DMA_PRIORITY;
  }
#endif
  else {
    osalDbgAssert(false, "invalid UART instance");
    *irqprio = 12U;
    *dmaprio = 8U;
  }
}

/**
 * @brief   Routes the DMAMUX request of the channels of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 * @param[in] rxdmastp  receive channel
 * @param[in] txdmastp  transmit channel
 */
static void unit_set_request(USART_TypeDef *u,
                             const ch32_dma_stream_t *rxdmastp,
                             const ch32_dma_stream_t *txdmastp) {

  if (u == USART1) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART1_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART1_TX);
  }
#if CH32H417_UART_USE_USART2 == TRUE
  else if (u == USART2) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART2_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART2_TX);
  }
#endif
#if CH32H417_UART_USE_USART3 == TRUE
  else if (u == USART3) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART3_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART3_TX);
  }
#endif
#if CH32H417_UART_USE_USART4 == TRUE
  else if (u == USART4) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART4_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART4_TX);
  }
#endif
#if CH32H417_UART_USE_USART5 == TRUE
  else if (u == USART5) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART5_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART5_TX);
  }
#endif
#if CH32H417_UART_USE_USART6 == TRUE
  else if (u == USART6) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART6_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART6_TX);
  }
#endif
#if CH32H417_UART_USE_USART7 == TRUE
  else if (u == USART7) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART7_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART7_TX);
  }
#endif
#if CH32H417_UART_USE_USART8 == TRUE
  else if (u == USART8) {
    ch32DmaStreamSetRequest(rxdmastp, CH32_DMAMUX_USART8_RX);
    ch32DmaStreamSetRequest(txdmastp, CH32_DMAMUX_USART8_TX);
  }
#endif
}

/**
 * @brief   Translates the STATR error flags into UART condition flags.
 *
 * @param[in] sr        @p STATR register value
 * @return              The condition flags mask.
 */
static uartflags_t translate_errors(uint16_t sr) {
  uartflags_t sts = 0U;

  if ((sr & USART_STATR_ORE) != 0U) {
    sts |= UART_OVERRUN_ERROR;
  }
  if ((sr & USART_STATR_PE) != 0U) {
    sts |= UART_PARITY_ERROR;
  }
  if ((sr & USART_STATR_FE) != 0U) {
    sts |= UART_FRAMING_ERROR;
  }
  if ((sr & USART_STATR_NE) != 0U) {
    sts |= UART_NOISE_ERROR;
  }

  return sts;
}

/**
 * @brief   Clears the receive error flags.
 * @details The flags are reset by a @p STATR read followed by a @p DATAR read
 *          (RM 19.11.1), there is no write-one-to-clear register.
 *
 * @param[in] u         pointer to the USART registers block
 */
static void clear_rx_errors(USART_TypeDef *u) {

  (void)u->STATR;
  (void)u->DATAR;
}

/**
 * @brief   Puts the receiver in the idle state.
 * @details A one byte circular transfer refills @p rxbuf, each completion
 *          raises the transfer complete interrupt.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 */
static void uart_enter_rx_idle_loop(UARTDriver *uartp) {
  uint32_t mode;

  mode = CH32_DMA_CFGR_CIRC;
  if (uartp->config->rxchar_cb != NULL) {
    mode |= CH32_DMA_CFGR_TCIE;
  }

  ch32DmaStreamSetMemory0(uartp->dmarx, &uartp->rxbuf);
  ch32DmaStreamSetTransactionSize(uartp->dmarx, 1U);
  ch32DmaStreamSetMode(uartp->dmarx, uartp->dmarxmode | mode);
  ch32DmaStreamEnable(uartp->dmarx);
}

/**
 * @brief   Stops the unit, the DMA channels stay allocated.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 */
static void usart_stop(UARTDriver *uartp) {

  ch32DmaStreamDisable(uartp->dmarx);
  ch32DmaStreamDisable(uartp->dmatx);

  uartp->usart->CTLR1 = 0U;
  uartp->usart->CTLR2 = 0U;
  uartp->usart->CTLR3 = 0U;
}

/**
 * @brief   Configures the unit.
 * @details This function must be invoked with interrupts disabled.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 */
static void usart_start(UARTDriver *uartp) {
  USART_TypeDef *u = uartp->usart;
  uint32_t div;

  /* Defensive programming, starting from a clean state.*/
  usart_stop(uartp);

  /* Baud rate, BRR = HCLK / baud with 16x oversampling (RM 19.3), rounded to
     the nearest. The live value is used so that a runtime clock tree change
     is honoured.*/
  div = (ch32h417_hclk + (uartp->config->speed / 2U)) / uartp->config->speed;
  osalDbgAssert((div >= 16U) && (div <= 0xFFFFU), "invalid baud rate");
  u->BRR = (uint16_t)div;

  /* Pending flags reset, no write-one-to-clear register.*/
  clear_rx_errors(u);

  /* Some bits are enforced because required for correct driver operation,
     DMA on both directions and the error interrupt.*/
  u->CTLR2 = uartp->config->cr2;
  u->CTLR3 = (uint16_t)(uartp->config->cr3 | UART_CTLR3_ENFORCED);
  u->CTLR1 = (uint16_t)(uartp->config->cr1 | UART_CTLR1_ENFORCED);

  /* The configuration writes must be visible to the peripheral before the
     transfer is started (V5F core, out of order).*/
  CH32H417_DSB();

  /* Starting the receiver idle loop.*/
  uart_enter_rx_idle_loop(uartp);
}

/**
 * @brief   DMA end of transfer service routine, receive channel.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void uart_lld_serve_rx_end_irq(UARTDriver *uartp, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    ch32DmaStreamDisable(uartp->dmarx);
    return;
  }

  if (uartp->rxstate == UART_RX_IDLE) {

    /* Receiver in idle state, a callback is generated, if enabled, for each
       received character and then the driver stays in the same state.*/
    _uart_rx_idle_code(uartp);
  }
  else {

    /* Receiver in active state, the buffer is complete.*/
    ch32DmaStreamDisable(uartp->dmarx);
    _uart_rx_complete_isr_code(uartp);
  }
}

/**
 * @brief   DMA end of transfer service routine, transmit channel.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void uart_lld_serve_tx_end_irq(UARTDriver *uartp, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    ch32DmaStreamDisable(uartp->dmatx);
    return;
  }

  ch32DmaStreamDisable(uartp->dmatx);

  /* A callback is generated, if enabled, after a completed transfer.*/
  _uart_tx1_isr_code(uartp);
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if CH32H417_UART_USE_USART1 || defined(__DOXYGEN__)
/**
 * @brief   USART1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD1);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART2 || defined(__DOXYGEN__)
/**
 * @brief   USART2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD2);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART3 || defined(__DOXYGEN__)
/**
 * @brief   USART3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD3);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART4 || defined(__DOXYGEN__)
/**
 * @brief   USART4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD4);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART5 || defined(__DOXYGEN__)
/**
 * @brief   USART5 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD5);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART6 || defined(__DOXYGEN__)
/**
 * @brief   USART6 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD6);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART7 || defined(__DOXYGEN__)
/**
 * @brief   USART7 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD7);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_UART_USE_USART8 || defined(__DOXYGEN__)
/**
 * @brief   USART8 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  uart_lld_serve_interrupt(&UARTD8);

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level UART driver initialization.
 *
 * @notapi
 */
void uart_lld_init(void) {

#if (CH32H417_UART_USE_USART1 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD1);
  UARTD1.usart    = USART1;
  UARTD1.dmarx    = NULL;
  UARTD1.dmatx    = NULL;
  UARTD1.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART1_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD1.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART1_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART2 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD2);
  UARTD2.usart    = USART2;
  UARTD2.dmarx    = NULL;
  UARTD2.dmatx    = NULL;
  UARTD2.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART2_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD2.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART2_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART3 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD3);
  UARTD3.usart    = USART3;
  UARTD3.dmarx    = NULL;
  UARTD3.dmatx    = NULL;
  UARTD3.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART3_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD3.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART3_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART4 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD4);
  UARTD4.usart    = USART4;
  UARTD4.dmarx    = NULL;
  UARTD4.dmatx    = NULL;
  UARTD4.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART4_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD4.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART4_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART5 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD5);
  UARTD5.usart    = USART5;
  UARTD5.dmarx    = NULL;
  UARTD5.dmatx    = NULL;
  UARTD5.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART5_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD5.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART5_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART6 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD6);
  UARTD6.usart    = USART6;
  UARTD6.dmarx    = NULL;
  UARTD6.dmatx    = NULL;
  UARTD6.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART6_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD6.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART6_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART7 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD7);
  UARTD7.usart    = USART7;
  UARTD7.dmarx    = NULL;
  UARTD7.dmatx    = NULL;
  UARTD7.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART7_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD7.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART7_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif

#if (CH32H417_UART_USE_USART8 == TRUE) || defined(__DOXYGEN__)
  uartObjectInit(&UARTD8);
  UARTD8.usart    = USART8;
  UARTD8.dmarx    = NULL;
  UARTD8.dmatx    = NULL;
  UARTD8.dmarxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART8_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  UARTD8.dmatxmode = CH32_DMA_CFGR_PL(CH32H417_UART_USART8_DMA_PRIORITY) |
                     CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE |
                     CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
#endif
}

/**
 * @brief   UART driver configuration and start.
 *
 * @param[in] uartp     pointer to an @p UARTDriver object
 * @return              The operation status.
 *
 * @notapi
 */
msg_t uart_lld_start(UARTDriver *uartp) {
  uint32_t irqprio;
  uint32_t dmaprio;

  osalDbgAssert(uartp->config->speed > 0U, "zero baud rate");

  unit_priorities(uartp->usart, &irqprio, &dmaprio);

  if (uartp->state == UART_STOP) {

    /* First activation, channels taken from the free pool and routed onto
       the peripheral requests.*/
    uartp->dmarx = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY, dmaprio,
                                       (ch32_dmaisr_t)uart_lld_serve_rx_end_irq,
                                       (void *)uartp);
    if (uartp->dmarx == NULL) {
      return HAL_RET_NO_RESOURCE;
    }

    uartp->dmatx = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY, dmaprio,
                                       (ch32_dmaisr_t)uart_lld_serve_tx_end_irq,
                                       (void *)uartp);
    if (uartp->dmatx == NULL) {
      ch32DmaStreamFreeI(uartp->dmarx);
      uartp->dmarx = NULL;
      return HAL_RET_NO_RESOURCE;
    }

    unit_set_request(uartp->usart, uartp->dmarx, uartp->dmatx);

    /* Data register association, the same register serves both directions
       on this device.*/
    ch32DmaStreamSetPeripheral(uartp->dmarx, &uartp->usart->DATAR);
    ch32DmaStreamSetPeripheral(uartp->dmatx, &uartp->usart->DATAR);

    unit_enable(uartp->usart, irqprio);
  }

  usart_start(uartp);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   UART driver stop.
 *
 * @param[in] uartp     pointer to an @p UARTDriver object
 *
 * @notapi
 */
void uart_lld_stop(UARTDriver *uartp) {

  if (uartp->state == UART_READY) {
    usart_stop(uartp);
    unit_disable(uartp->usart);

    /* Channels given back in the same order they were taken.*/
    if (uartp->dmatx != NULL) {
      ch32DmaStreamFreeI(uartp->dmatx);
      uartp->dmatx = NULL;
    }
    if (uartp->dmarx != NULL) {
      ch32DmaStreamFreeI(uartp->dmarx);
      uartp->dmarx = NULL;
    }
  }
}

/**
 * @brief   Starts a transmission on the UART peripheral.
 * @note    The buffers are organized as @p uint8_t arrays, only 8 bit frames
 *          are supported by the DMA sizing.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] n         number of data frames to send
 * @param[in] txbuf     the pointer to the transmit buffer
 *
 * @notapi
 */
void uart_lld_start_send(UARTDriver *uartp, size_t n, const void *txbuf) {

  /* TX DMA channel preparation.*/
  ch32DmaStreamSetMemory0(uartp->dmatx, txbuf);
  ch32DmaStreamSetTransactionSize(uartp->dmatx, n);
  ch32DmaStreamSetMode(uartp->dmatx, uartp->dmatxmode |
                                     CH32_DMA_CFGR_MINC |
                                     CH32_DMA_CFGR_TCIE);

  /* The transmission complete interrupt is enabled only when a callback or a
     waiting thread is interested in it, the stale TC flag is cleared by the
     read-then-write sequence of the RM (RM 19.11.1).*/
#if UART_USE_WAIT == TRUE
  if ((uartp->config->txend2_cb != NULL) || (uartp->early == false)) {
#else
  if (uartp->config->txend2_cb != NULL) {
#endif
    (void)uartp->usart->STATR;
    uartp->usart->STATR = (uint16_t)(uartp->usart->STATR &
                                     (uint16_t)~USART_STATR_TC);
    uartp->usart->CTLR1 |= USART_CTLR1_TCIE;
  }

  /* Starting transfer.*/
  ch32DmaStreamEnable(uartp->dmatx);
}

/**
 * @brief   Stops any ongoing transmission.
 * @note    Stopping a transmission also suppresses the transmission callbacks.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 *
 * @return              The number of data frames not transmitted by the
 *                      stopped transmit operation.
 *
 * @notapi
 */
size_t uart_lld_stop_send(UARTDriver *uartp) {
  size_t n;

  ch32DmaStreamDisable(uartp->dmatx);
  n = ch32DmaStreamGetTransactionSize(uartp->dmatx);

  return n;
}

/**
 * @brief   Starts a receive operation on the UART peripheral.
 * @note    The buffers are organized as @p uint8_t arrays, only 8 bit frames
 *          are supported by the DMA sizing.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] n         number of data frames to receive
 * @param[out] rxbuf    the pointer to the receive buffer
 *
 * @notapi
 */
void uart_lld_start_receive(UARTDriver *uartp, size_t n, void *rxbuf) {

  /* Stopping previous activity (idle state).*/
  ch32DmaStreamDisable(uartp->dmarx);

  /* RX DMA channel preparation.*/
  ch32DmaStreamSetMemory0(uartp->dmarx, rxbuf);
  ch32DmaStreamSetTransactionSize(uartp->dmarx, n);
  ch32DmaStreamSetMode(uartp->dmarx, uartp->dmarxmode |
                                     CH32_DMA_CFGR_MINC |
                                     CH32_DMA_CFGR_TCIE);

  /* Starting transfer.*/
  ch32DmaStreamEnable(uartp->dmarx);
}

/**
 * @brief   Stops any ongoing receive operation.
 * @note    Stopping a receive operation also suppresses the receive callbacks.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 *
 * @return              The number of data frames not received by the
 *                      stopped receive operation.
 *
 * @notapi
 */
size_t uart_lld_stop_receive(UARTDriver *uartp) {
  size_t n;

  ch32DmaStreamDisable(uartp->dmarx);
  n = ch32DmaStreamGetTransactionSize(uartp->dmarx);
  uart_enter_rx_idle_loop(uartp);

  return n;
}

/**
 * @brief   USART common service routine.
 * @note    The idle line interrupt is not used by this driver: its flag can
 *          only be reset by a @p DATAR read, which would steal a byte from
 *          the DMA engine, so @p timeout_cb is never invoked.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 *
 * @notapi
 */
void uart_lld_serve_interrupt(UARTDriver *uartp) {
  USART_TypeDef *u = uartp->usart;
  uint16_t sr = u->STATR;
  uint16_t cr1 = u->CTLR1;

  /* Error handling: the flags are reset by reading the data register, that
     byte is lost but the error itself compromised it already.*/
  if ((sr & UART_STATR_ERRORS) != 0U) {
    clear_rx_errors(u);

    /* Parity errors are reported only when enabled, the other ones are
       gated by EIE which is enforced by the driver.*/
    if (((sr & USART_STATR_PE) == 0U) || ((cr1 & USART_CTLR1_PEIE) != 0U)) {
      _uart_rx_error_isr_code(uartp, translate_errors(sr));
    }
  }

  /* Transmission complete, only when enabled.*/
  if (((sr & USART_STATR_TC) != 0U) && ((cr1 & USART_CTLR1_TCIE) != 0U)) {

    /* TC is cleared by writing 0 to it, the other read-only flags are written
       back with their current value so that no received byte is dropped.*/
    u->STATR = (uint16_t)(sr & (uint16_t)~USART_STATR_TC);
    u->CTLR1 = (uint16_t)(cr1 & (uint16_t)~USART_CTLR1_TCIE);

    _uart_tx2_isr_code(uartp);
  }
}

#endif /* HAL_USE_UART == TRUE */

/** @} */