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
 * @file    USARTv1/hal_sio_lld.c
 * @brief   CH32H417 SIO subsystem low level driver source.
 *
 * @details Interrupt driven engine (RXNE/IDLE/TXE/TC/errors), F1-compatible
 *          register model, up to eight units.
 *
 *          Peculiarity of this peripheral: there is no write-one-to-clear
 *          status register. @p PE/@p NE/@p FE/@p ORE/@p IDLE are all reset
 *          by a @p STATR read followed by a @p DATAR read (RM 19.11.1). Such
 *          a read also consumes the received byte, so any byte found while
 *          clearing the flags is saved into @p rxbuf and handed to the
 *          application by the read functions instead of being lost. For the
 *          same reason the @p DATAR read happens at most once per interrupt.
 *
 * @addtogroup SIO
 * @{
 */

#include "hal.h"

#if (HAL_USE_SIO == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

#define SIO_CTLR1_IRQ_MASK                  (USART_CTLR1_IDLEIE |           \
                                             USART_CTLR1_RXNEIE |           \
                                             USART_CTLR1_TCIE   |           \
                                             USART_CTLR1_TXEIE  |           \
                                             USART_CTLR1_PEIE)

#define SIO_CTLR3_IRQ_MASK                  (USART_CTLR3_EIE |              \
                                             USART_CTLR3_DMAR)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_SIO_USE_USART1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART1 SIO driver identifier.
 */
SIODriver SIOD1;
#endif
#if (CH32H417_SIO_USE_USART2 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART2 SIO driver identifier.
 */
SIODriver SIOD2;
#endif
#if (CH32H417_SIO_USE_USART3 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART3 SIO driver identifier.
 */
SIODriver SIOD3;
#endif
#if (CH32H417_SIO_USE_USART4 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART4 SIO driver identifier.
 */
SIODriver SIOD4;
#endif
#if (CH32H417_SIO_USE_USART5 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART5 SIO driver identifier.
 */
SIODriver SIOD5;
#endif
#if (CH32H417_SIO_USE_USART6 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART6 SIO driver identifier.
 */
SIODriver SIOD6;
#endif
#if (CH32H417_SIO_USE_USART7 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART7 SIO driver identifier.
 */
SIODriver SIOD7;
#endif
#if (CH32H417_SIO_USE_USART8 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   USART8 SIO driver identifier.
 */
SIODriver SIOD8;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   Driver default configuration.
 * @note    In this implementation it is: 38400-8-N-1.
 */
static const SIOConfig default_config = {
  .baud         = SIO_DEFAULT_BITRATE,
  .cr1          = 0U,
  .cr2          = 0U,
  .cr3          = 0U
};

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables the bus clock of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 */
static void unit_enable(USART_TypeDef *u) {

  if (u == USART1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_USART1, ENABLE);
  }
#if CH32H417_SIO_USE_USART2 == TRUE
  else if (u == USART2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART2, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  else if (u == USART3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART3, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  else if (u == USART4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART4, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  else if (u == USART5) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART5, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  else if (u == USART6) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART6, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  else if (u == USART7) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART7, ENABLE);
  }
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  else if (u == USART8) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_USART8, ENABLE);
  }
#endif
}

/**
 * @brief   Resets a unit through the RCC peripheral reset.
 *
 * @param[in] u         pointer to the USART registers block
 */
static void unit_reset(USART_TypeDef *u) {

  if (u == USART1) {
    RCC_HB2PeriphResetCmd(RCC_HB2Periph_USART1, ENABLE);
    RCC_HB2PeriphResetCmd(RCC_HB2Periph_USART1, DISABLE);
  }
#if CH32H417_SIO_USE_USART2 == TRUE
  else if (u == USART2) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART2, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART2, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  else if (u == USART3) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART3, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART3, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  else if (u == USART4) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART4, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART4, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  else if (u == USART5) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART5, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART5, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  else if (u == USART6) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART6, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART6, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  else if (u == USART7) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART7, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART7, DISABLE);
  }
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  else if (u == USART8) {
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART8, ENABLE);
    RCC_HB1PeriphResetCmd(RCC_HB1Periph_USART8, DISABLE);
  }
#endif
}

/**
 * @brief   Interrupt vector enabling with priority.
 *
 * @param[in] u         pointer to the USART registers block
 * @param[in] prio      PFIC priority field
 */
static void unit_irq_enable(USART_TypeDef *u, uint32_t prio) {

  if (u == USART1) {
    NVIC_SetPriority(USART1_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART1_IRQn);
  }
#if CH32H417_SIO_USE_USART2 == TRUE
  else if (u == USART2) {
    NVIC_SetPriority(USART2_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  else if (u == USART3) {
    NVIC_SetPriority(USART3_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  else if (u == USART4) {
    NVIC_SetPriority(USART4_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  else if (u == USART5) {
    NVIC_SetPriority(USART5_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  else if (u == USART6) {
    NVIC_SetPriority(USART6_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  else if (u == USART7) {
    NVIC_SetPriority(USART7_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  else if (u == USART8) {
    NVIC_SetPriority(USART8_IRQn, (uint8_t)CH32_IRQ_PRIORITY(prio));
    NVIC_EnableIRQ(USART8_IRQn);
  }
#endif
}

/**
 * @brief   Interrupt vector disabling.
 *
 * @param[in] u         pointer to the USART registers block
 */
static void unit_irq_disable(USART_TypeDef *u) {

  if (u == USART1) {
    NVIC_DisableIRQ(USART1_IRQn);
  }
#if CH32H417_SIO_USE_USART2 == TRUE
  else if (u == USART2) {
    NVIC_DisableIRQ(USART2_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  else if (u == USART3) {
    NVIC_DisableIRQ(USART3_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  else if (u == USART4) {
    NVIC_DisableIRQ(USART4_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  else if (u == USART5) {
    NVIC_DisableIRQ(USART5_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  else if (u == USART6) {
    NVIC_DisableIRQ(USART6_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  else if (u == USART7) {
    NVIC_DisableIRQ(USART7_IRQn);
  }
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  else if (u == USART8) {
    NVIC_DisableIRQ(USART8_IRQn);
  }
#endif
}

/**
 * @brief   Returns the IRQ priority of a unit.
 *
 * @param[in] u         pointer to the USART registers block
 * @return              The configured priority.
 */
static uint32_t unit_irq_priority(USART_TypeDef *u) {

  if (u == USART1) {
    return CH32H417_SIO_USART1_IRQ_PRIORITY;
  }
#if CH32H417_SIO_USE_USART2 == TRUE
  else if (u == USART2) {
    return CH32H417_SIO_USART2_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  else if (u == USART3) {
    return CH32H417_SIO_USART3_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  else if (u == USART4) {
    return CH32H417_SIO_USART4_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  else if (u == USART5) {
    return CH32H417_SIO_USART5_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  else if (u == USART6) {
    return CH32H417_SIO_USART6_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  else if (u == USART7) {
    return CH32H417_SIO_USART7_IRQ_PRIORITY;
  }
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  else if (u == USART8) {
    return CH32H417_SIO_USART8_IRQ_PRIORITY;
  }
#endif
  else {
    osalDbgAssert(false, "invalid SIO instance");
    return 0U;
  }
}

/**
 * @brief   Completes the status reset sequence.
 * @details The flags are reset by a @p STATR read followed by a @p DATAR read.
 *          The @p sr parameter must be the @p STATR value read just before,
 *          it tells whether the @p DATAR read returns a real frame: a byte
 *          consumed while clearing the flags is saved, not dropped.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @param[in] sr        previously read @p STATR value
 */
static void reset_status(SIODriver *siop, uint16_t sr) {
  uint16_t data = siop->usart->DATAR;

  if ((sr & USART_STATR_RXNE) != 0U) {
    if (siop->rxpending == false) {
      siop->rxbuf     = (uint8_t)(data & 0xFFU);
      siop->rxpending = true;
    }
    /* If a byte is already held then the peripheral itself has overrun the
       one byte register, the older of the two is not recoverable.*/
  }
}

/**
 * @brief   Re-enables the RX related interrupt sources.
 * @details The sources are disabled when serviced, they are enabled again
 *          when the application has consumed the event.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 */
static void enable_rx_irqs(SIODriver *siop) {
  uint16_t cr1 = siop->usart->CTLR1;

  if ((siop->enabled & SIO_EV_RXNOTEMPY) != 0U) {
    cr1 |= USART_CTLR1_RXNEIE;
  }
  if ((siop->enabled & SIO_EV_RXIDLE) != 0U) {
    cr1 |= USART_CTLR1_IDLEIE;
  }
  siop->usart->CTLR1 = cr1;
}

/**
 * @brief   Re-enables the error interrupt sources.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 */
static void enable_error_irqs(SIODriver *siop) {
  uint16_t cr1 = siop->usart->CTLR1;
  uint16_t cr3 = siop->usart->CTLR3;

  if ((siop->enabled & SIO_EV_PARITY_ERR) != 0U) {
    cr1 |= USART_CTLR1_PEIE;
  }
  if ((siop->enabled & (SIO_EV_FRAMING_ERR | SIO_EV_NOISE_ERR |
                        SIO_EV_OVERRUN_ERR)) != 0U) {
    /* RM 19.11.6: EIE only produces interrupts while DMAR is set. No DMA
       request is routed in this driver, the corresponding requests are
       simply never serviced.*/
    cr3 |= USART_CTLR3_EIE | USART_CTLR3_DMAR;
  }
  siop->usart->CTLR1 = cr1;
  siop->usart->CTLR3 = cr3;
}

/**
 * @brief   Translates the STATR error flags into SIO events.
 *
 * @param[in] sr        @p STATR register value
 * @return              The SIO error events mask.
 */
static sioevents_t translate_errors(uint16_t sr) {
  sioevents_t errors = (sioevents_t)0;

  if ((sr & USART_STATR_PE) != 0U) {
    errors |= SIO_EV_PARITY_ERR;
  }
  if ((sr & USART_STATR_FE) != 0U) {
    errors |= SIO_EV_FRAMING_ERR;
  }
  if ((sr & USART_STATR_NE) != 0U) {
    errors |= SIO_EV_NOISE_ERR;
  }
  if ((sr & USART_STATR_ORE) != 0U) {
    errors |= SIO_EV_OVERRUN_ERR;
  }

  return errors;
}

/**
 * @brief   Configures the unit.
 * @details This function must be invoked with interrupts disabled.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 */
static void usart_init(SIODriver *siop) {
  USART_TypeDef *u = siop->usart;
  uint32_t div;

  /* Baud rate, BRR = HCLK / baud with 16x oversampling (RM 19.3), rounded to
     the nearest. The live value is used so that a runtime clock tree change
     is honoured.*/
  div = (ch32h417_hclk + (siop->config->baud / 2U)) / siop->config->baud;
  osalDbgAssert((div >= 16U) && (div <= 0xFFFFU), "invalid baud rate");
  u->BRR = (uint16_t)div;

  /* No write-one-to-clear register, pending flags are reset by the
     STATR/DATAR read sequence.*/
  (void)u->STATR;
  (void)u->DATAR;

  /* Configuration fields are raw CTLR values, transmitter and receiver are
     always enabled, the DMA request bits belong to the driver.*/
  u->CTLR2 = siop->config->cr2;
  u->CTLR3 = (uint16_t)(siop->config->cr3 & (uint16_t)~SIO_CTLR3_IRQ_MASK);
  u->CTLR1 = (uint16_t)(siop->config->cr1 | USART_CTLR1_UE |
                        USART_CTLR1_TE | USART_CTLR1_RE);

  /* The configuration writes must be visible to the peripheral before the
     interrupt sources are programmed (V5F core, out of order).*/
  CH32H417_DSB();
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if CH32H417_SIO_USE_USART1 || defined(__DOXYGEN__)
/**
 * @brief   USART1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD1);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART2 || defined(__DOXYGEN__)
/**
 * @brief   USART2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD2);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART3 || defined(__DOXYGEN__)
/**
 * @brief   USART3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD3);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART4 || defined(__DOXYGEN__)
/**
 * @brief   USART4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD4);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART5 || defined(__DOXYGEN__)
/**
 * @brief   USART5 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART5_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD5);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART6 || defined(__DOXYGEN__)
/**
 * @brief   USART6 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART6_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD6);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART7 || defined(__DOXYGEN__)
/**
 * @brief   USART7 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART7_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD7);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if CH32H417_SIO_USE_USART8 || defined(__DOXYGEN__)
/**
 * @brief   USART8 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(USART8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  sio_lld_serve_interrupt(&SIOD8);

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level SIO driver initialization.
 *
 * @notapi
 */
void sio_lld_init(void) {

#if CH32H417_SIO_USE_USART1 == TRUE
  sioObjectInit(&SIOD1);
  SIOD1.usart     = USART1;
  SIOD1.rxbuf     = 0U;
  SIOD1.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART2 == TRUE
  sioObjectInit(&SIOD2);
  SIOD2.usart     = USART2;
  SIOD2.rxbuf     = 0U;
  SIOD2.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART3 == TRUE
  sioObjectInit(&SIOD3);
  SIOD3.usart     = USART3;
  SIOD3.rxbuf     = 0U;
  SIOD3.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART4 == TRUE
  sioObjectInit(&SIOD4);
  SIOD4.usart     = USART4;
  SIOD4.rxbuf     = 0U;
  SIOD4.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART5 == TRUE
  sioObjectInit(&SIOD5);
  SIOD5.usart     = USART5;
  SIOD5.rxbuf     = 0U;
  SIOD5.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART6 == TRUE
  sioObjectInit(&SIOD6);
  SIOD6.usart     = USART6;
  SIOD6.rxbuf     = 0U;
  SIOD6.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART7 == TRUE
  sioObjectInit(&SIOD7);
  SIOD7.usart     = USART7;
  SIOD7.rxbuf     = 0U;
  SIOD7.rxpending = false;
#endif
#if CH32H417_SIO_USE_USART8 == TRUE
  sioObjectInit(&SIOD8);
  SIOD8.usart     = USART8;
  SIOD8.rxbuf     = 0U;
  SIOD8.rxpending = false;
#endif
}

/**
 * @brief   Configures and activates the SIO peripheral.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The operation status.
 *
 * @notapi
 */
msg_t sio_lld_start(SIODriver *siop) {

  /* Using the default configuration if the application passed a
     NULL pointer.*/
  if (siop->config == NULL) {
    siop->config = &default_config;
  }

  osalDbgAssert(siop->config->baud > 0U, "zero baud rate");

  if (siop->state == SIO_STOP) {
    /* Enables the peripheral clock, resets it and enables the vector.*/
    unit_enable(siop->usart);
    unit_reset(siop->usart);
    unit_irq_enable(siop->usart, unit_irq_priority(siop->usart));

    /* A byte left over from a previous run is not part of this stream.*/
    siop->rxpending = false;
  }

  usart_init(siop);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Deactivates the SIO peripheral.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 *
 * @notapi
 */
void sio_lld_stop(SIODriver *siop) {

  if (siop->state == SIO_READY) {
    USART_TypeDef *u = siop->usart;

    unit_irq_disable(u);

    /* Transmitter and receiver disabled, the interrupt sources go away with
       the control registers.*/
    u->CTLR1 = 0U;
    u->CTLR3 = 0U;

    unit_reset(u);

    siop->rxpending = false;
  }
}

/**
 * @brief   Enable flags change notification.
 * @note    @p NE/@p FE/@p ORE share the single @p EIE source which, on this
 *          device, requires @p DMAR to be set (RM 19.11.6). @p PE has its own
 *          @p PEIE bit.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 *
 * @notapi
 */
void sio_lld_update_enable_flags(SIODriver *siop) {
  USART_TypeDef *u = siop->usart;
  uint16_t cr1;
  uint16_t cr3;

  cr1 = (uint16_t)(u->CTLR1 & (uint16_t)~SIO_CTLR1_IRQ_MASK);
  cr3 = (uint16_t)(u->CTLR3 & (uint16_t)~SIO_CTLR3_IRQ_MASK);

  if ((siop->enabled & SIO_EV_RXNOTEMPY) != 0U) {
    cr1 |= USART_CTLR1_RXNEIE;
  }
  if ((siop->enabled & SIO_EV_TXNOTFULL) != 0U) {
    cr1 |= USART_CTLR1_TXEIE;
  }
  if ((siop->enabled & SIO_EV_TXDONE) != 0U) {
    cr1 |= USART_CTLR1_TCIE;
  }
  if ((siop->enabled & SIO_EV_RXIDLE) != 0U) {
    cr1 |= USART_CTLR1_IDLEIE;
  }
  if ((siop->enabled & SIO_EV_PARITY_ERR) != 0U) {
    cr1 |= USART_CTLR1_PEIE;
  }
  if ((siop->enabled & (SIO_EV_FRAMING_ERR | SIO_EV_NOISE_ERR |
                        SIO_EV_OVERRUN_ERR)) != 0U) {
    cr3 |= USART_CTLR3_EIE | USART_CTLR3_DMAR;
  }

  u->CTLR1 = cr1;
  u->CTLR3 = cr3;
}

/**
 * @brief   Get and clears SIO error event flags.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The pending error event flags.
 *
 * @notapi
 */
sioevents_t sio_lld_get_and_clear_errors(SIODriver *siop) {
  uint16_t sr = siop->usart->STATR;
  sioevents_t errors = (sioevents_t)0;

  if ((sr & SIO_LLD_ISR_RX_ERRORS) != 0U) {
    reset_status(siop, sr);
    errors = translate_errors(sr);
  }

  /* Status flags cleared, the related interrupt sources can be enabled
     again.*/
  enable_error_irqs(siop);

  return errors;
}

/**
 * @brief   Get and clears SIO event flags.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The pending event flags.
 *
 * @notapi
 */
sioevents_t sio_lld_get_and_clear_events(SIODriver *siop) {
  uint16_t sr = siop->usart->STATR;
  sioevents_t events = (sioevents_t)0;

  /* A single DATAR read services all the flags that need it.*/
  if ((sr & (SIO_LLD_ISR_RX_ERRORS | USART_STATR_IDLE)) != 0U) {
    reset_status(siop, sr);
    events = translate_errors(sr);

    if ((sr & USART_STATR_IDLE) != 0U) {
      events |= SIO_EV_RXIDLE;
    }
  }

  if (sio_lld_is_rx_empty(siop) == false) {
    events |= SIO_EV_RXNOTEMPY;
  }
  if (sio_lld_is_tx_full(siop) == false) {
    events |= SIO_EV_TXNOTFULL;
  }
  if (sio_lld_is_tx_ongoing(siop) == false) {
    events |= SIO_EV_TXDONE;
  }

  enable_rx_irqs(siop);
  enable_error_irqs(siop);

  return events;
}

/**
 * @brief   Returns the pending SIO event flags.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The pending event flags.
 *
 * @notapi
 */
sioevents_t sio_lld_get_events(SIODriver *siop) {
  uint16_t sr = siop->usart->STATR;
  sioevents_t events = (sioevents_t)0;

  events = translate_errors(sr);

  if ((sr & USART_STATR_IDLE) != 0U) {
    events |= SIO_EV_RXIDLE;
  }
  if (sio_lld_is_rx_empty(siop) == false) {
    events |= SIO_EV_RXNOTEMPY;
  }
  if (sio_lld_is_tx_full(siop) == false) {
    events |= SIO_EV_TXNOTFULL;
  }
  if (sio_lld_is_tx_ongoing(siop) == false) {
    events |= SIO_EV_TXDONE;
  }

  return events;
}

/**
 * @brief   Reads data from the RX register.
 * @details The function is not blocking, it reads frames until there is
 *          nothing available without waiting.
 *
 * @param[in] siop      pointer to an @p SIODriver structure
 * @param[out] buffer   pointer to the buffer for read frames
 * @param[in] n         maximum number of frames to be read
 * @return              The number of frames copied from the buffer.
 *
 * @notapi
 */
size_t sio_lld_read(SIODriver *siop, uint8_t *buffer, size_t n) {
  size_t rd = 0U;

  /* A byte captured while clearing the status flags is handed out first.*/
  if (siop->rxpending && (rd < n)) {
    *buffer++ = siop->rxbuf;
    siop->rxpending = false;
    rd++;
  }

  while (rd < n) {

    /* If the RX register has been emptied then the interrupts are enabled
       again.*/
    if (sio_lld_is_rx_empty(siop)) {
      enable_rx_irqs(siop);
      break;
    }

    *buffer++ = (uint8_t)(siop->usart->DATAR & 0xFFU);
    rd++;
  }

  return rd;
}

/**
 * @brief   Writes data into the TX register.
 * @details The function is not blocking, it writes frames until the register
 *          is busy without waiting.
 *
 * @param[in] siop      pointer to an @p SIODriver structure
 * @param[in] buffer    pointer to the buffer for written frames
 * @param[in] n         maximum number of frames to be written
 * @return              The number of frames copied from the buffer.
 *
 * @notapi
 */
size_t sio_lld_write(SIODriver *siop, const uint8_t *buffer, size_t n) {
  size_t wr = 0U;

  while (wr < n) {

    /* If the TX register is still busy then the interrupt is enabled again.*/
    if (sio_lld_is_tx_full(siop)) {
      if ((siop->enabled & SIO_EV_TXNOTFULL) != 0U) {
        siop->usart->CTLR1 |= USART_CTLR1_TXEIE;
      }
      break;
    }

    siop->usart->DATAR = (uint16_t)*buffer++;
    wr++;
  }

  /* The transmission complete interrupt is armed again on write, a stale TC
     flag of the previous frame is cleared so that it cannot fire at once.*/
  if ((siop->enabled & SIO_EV_TXDONE) != 0U) {
    siop->usart->STATR = (uint16_t)~USART_STATR_TC;
    siop->usart->CTLR1 |= USART_CTLR1_TCIE;
  }

  return wr;
}

/**
 * @brief   Returns one frame from the RX register.
 * @note    If the register is empty then the returned value is unpredictable.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The frame from RX register.
 *
 * @notapi
 */
msg_t sio_lld_get(SIODriver *siop) {
  msg_t msg;

  if (siop->rxpending) {
    msg = (msg_t)siop->rxbuf;
    siop->rxpending = false;
  }
  else {
    msg = (msg_t)(siop->usart->DATAR & 0xFFU);
  }

  /* If the RX register has been emptied then the interrupts are enabled
     again.*/
  if (sio_lld_is_rx_empty(siop)) {
    enable_rx_irqs(siop);
  }

  return msg;
}

/**
 * @brief   Pushes one frame into the TX register.
 * @note    If the register is busy then the behavior is unpredictable.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @param[in] data      frame to be written
 *
 * @notapi
 */
void sio_lld_put(SIODriver *siop, uint_fast16_t data) {

  siop->usart->DATAR = (uint16_t)data;

  /* If the TX register is still busy then the interrupt is enabled again.*/
  if ((sio_lld_is_tx_full(siop) == true) &&
      ((siop->enabled & SIO_EV_TXNOTFULL) != 0U)) {
    siop->usart->CTLR1 |= USART_CTLR1_TXEIE;
  }

  /* The transmission complete interrupt is armed again on write, a stale TC
     flag of the previous frame is cleared so that it cannot fire at once.*/
  if ((siop->enabled & SIO_EV_TXDONE) != 0U) {
    siop->usart->STATR = (uint16_t)~USART_STATR_TC;
    siop->usart->CTLR1 |= USART_CTLR1_TCIE;
  }
}

/**
 * @brief   Control operation on a serial port.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @param[in] operation control operation code
 * @param[in,out] arg   operation argument
 *
 * @return              The control operation status.
 * @retval MSG_OK       in case of success.
 *
 * @notapi
 */
msg_t sio_lld_control(SIODriver *siop, unsigned int operation, void *arg) {

  (void)siop;
  (void)operation;
  (void)arg;

  return MSG_OK;
}

/**
 * @brief   Serves an USART interrupt.
 * @note    The interrupt sources are disabled when serviced and enabled again
 *          only once the corresponding condition has been consumed, this
 *          replaces the missing write-one-to-clear status register. The
 *          @p DATAR register is read at most once per interrupt.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 *
 * @notapi
 */
void sio_lld_serve_interrupt(SIODriver *siop) {
  USART_TypeDef *u = siop->usart;
  uint16_t sr = u->STATR;
  uint16_t cr1 = u->CTLR1;
  uint16_t cr3 = u->CTLR3;

  osalDbgAssert(siop->state == SIO_READY, "invalid state");

  /* A single DATAR read services errors and the idle flag, a byte found in
     the process is preserved.*/
  if ((sr & (SIO_LLD_ISR_RX_ERRORS | USART_STATR_IDLE)) != 0U) {
    reset_status(siop, sr);
  }

  /* Error events handled as a group.*/
  if (((sr & SIO_LLD_ISR_RX_ERRORS) != 0U) &&
      (((cr1 & USART_CTLR1_PEIE) != 0U) ||
       ((cr3 & USART_CTLR3_EIE) != 0U))) {

    /* Sources disabled, re-enabled by sio_lld_get_and_clear_errors().*/
    cr1 &= (uint16_t)~USART_CTLR1_PEIE;
    cr3 &= (uint16_t)~(USART_CTLR3_EIE | USART_CTLR3_DMAR);

    __sio_wakeup_errors(siop);
  }

  /* Idle line event.*/
  if (((sr & USART_STATR_IDLE) != 0U) && ((cr1 & USART_CTLR1_IDLEIE) != 0U)) {

    /* Called once then the source is disabled.*/
    cr1 &= (uint16_t)~USART_CTLR1_IDLEIE;

    if (sio_lld_is_rx_empty(siop) == false) {
      __sio_wakeup_rx(siop);
    }
    __sio_wakeup_rxidle(siop);
  }

  /* A frame is available, it stays in DATAR and is read by the application.*/
  if (((sr & USART_STATR_RXNE) != 0U) && ((cr1 & USART_CTLR1_RXNEIE) != 0U)) {

    /* Called once then the source is disabled.*/
    cr1 &= (uint16_t)~USART_CTLR1_RXNEIE;

    __sio_wakeup_rx(siop);
  }

  /* TX register empty.*/
  if (((sr & USART_STATR_TXE) != 0U) && ((cr1 & USART_CTLR1_TXEIE) != 0U)) {

    /* Called once then the source is disabled.*/
    cr1 &= (uint16_t)~USART_CTLR1_TXEIE;

    __sio_wakeup_tx(siop);
  }

  /* Transmission complete, the flag is cleared by writing a 0 into it while
     the other bits are written as 1, which leaves them untouched (RM
     19.11.1: RXNE survives a 1 write, it is not dropped).*/
  if (((sr & USART_STATR_TC) != 0U) && ((cr1 & USART_CTLR1_TCIE) != 0U)) {

    u->STATR = (uint16_t)~USART_STATR_TC;
    cr1 &= (uint16_t)~USART_CTLR1_TCIE;

    __sio_wakeup_txend(siop);
  }

  /* Updating the control registers, some sources could have been
     disabled.*/
  u->CTLR1 = cr1;
  u->CTLR3 = cr3;

  /* The callback is invoked.*/
  __sio_callback(siop);
}

#endif /* HAL_USE_SIO == TRUE */

/** @} */