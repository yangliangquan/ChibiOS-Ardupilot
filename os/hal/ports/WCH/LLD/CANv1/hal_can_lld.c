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
 * @file    CANv1/hal_can_lld.c
 * @brief   CH32H417 CAN subsystem low level driver source.
 *
 * @details Interrupt driven engine on four vectors per controller
 *          (TX/RX0/RX1/SCE), bxCAN semantics with renamed registers.
 *
 * @addtogroup CAN
 * @{
 */

#include "hal.h"

#if (HAL_USE_CAN == TRUE) || defined(__DOXYGEN__)

#include <string.h>
#include "ch32h417_can.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* TSTATR (TX status) bits, F1-compatible layout. */
#define CH32_CAN_RQCP(i)                    (1UL << ((i) * 4))
#define CH32_CAN_TME(i)                     (1UL << (1 + ((i) * 4)))
#define CH32_CAN_ALST(i)                    (1UL << (2 + ((i) * 4)))
#define CH32_CAN_TEST(i)                    (1UL << (3 + ((i) * 4)))

/* RFIFO0/1 bits. */
#define CH32_CAN_FMP_MASK                   0x0000000FUL
#define CH32_CAN_FOVR                       (1UL << 6)

/* ERRSR bits (error status). */
#define CH32_CAN_ERRSR_LEC_MASK             0x00000007UL
#define CH32_CAN_ERRSR_EWGF                 (1UL << 4)
#define CH32_CAN_ERRSR_EPVF                 (1UL << 5)
#define CH32_CAN_ERRSR_BOFF                 (1UL << 6)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_CAN_USE_CAN1 == TRUE) && !defined(__DOXYGEN__)
CANDriver CAND1 = { .can = CAN1 };
#endif
#if (CH32H417_CAN_USE_CAN2 == TRUE) && !defined(__DOXYGEN__)
CANDriver CAND2 = { .can = CAN2 };
#endif
#if (CH32H417_CAN_USE_CAN3 == TRUE) && !defined(__DOXYGEN__)
CANDriver CAND3 = { .can = CAN3 };
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Enables the bus clock and the four IRQ vectors of a unit.
 */
static void unit_enable(CAN_TypeDef *c) {

  if (c == CAN1) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_CAN1, ENABLE);
    NVIC_EnableIRQ(CAN1_TX_IRQn);
    NVIC_EnableIRQ(CAN1_RX0_IRQn);
    NVIC_EnableIRQ(CAN1_RX1_IRQn);
    NVIC_EnableIRQ(CAN1_SCE_IRQn);
  }
#if CH32H417_CAN_USE_CAN2
  else if (c == CAN2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_CAN2, ENABLE);
    NVIC_EnableIRQ(CAN2_TX_IRQn);
    NVIC_EnableIRQ(CAN2_RX0_IRQn);
    NVIC_EnableIRQ(CAN2_RX1_IRQn);
    NVIC_EnableIRQ(CAN2_SCE_IRQn);
  }
#endif
#if CH32H417_CAN_USE_CAN3
  else if (c == CAN3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_CAN3, ENABLE);
    NVIC_EnableIRQ(CAN3_TX_IRQn);
    NVIC_EnableIRQ(CAN3_RX0_IRQn);
    NVIC_EnableIRQ(CAN3_RX1_IRQn);
    NVIC_EnableIRQ(CAN3_SCE_IRQn);
  }
#endif
}

static void unit_disable(CAN_TypeDef *c) {

  if (c == CAN1) {
    NVIC_DisableIRQ(CAN1_TX_IRQn);
    NVIC_DisableIRQ(CAN1_RX0_IRQn);
    NVIC_DisableIRQ(CAN1_RX1_IRQn);
    NVIC_DisableIRQ(CAN1_SCE_IRQn);
  }
#if CH32H417_CAN_USE_CAN2
  else if (c == CAN2) {
    NVIC_DisableIRQ(CAN2_TX_IRQn);
    NVIC_DisableIRQ(CAN2_RX0_IRQn);
    NVIC_DisableIRQ(CAN2_RX1_IRQn);
    NVIC_DisableIRQ(CAN2_SCE_IRQn);
  }
#endif
#if CH32H417_CAN_USE_CAN3
  else if (c == CAN3) {
    NVIC_DisableIRQ(CAN3_TX_IRQn);
    NVIC_DisableIRQ(CAN3_RX0_IRQn);
    NVIC_DisableIRQ(CAN3_RX1_IRQn);
    NVIC_DisableIRQ(CAN3_SCE_IRQn);
  }
#endif
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level CAN driver initialization.
 *
 * @notapi
 */
void can_lld_init(void) {
}

/**
 * @brief   CAN driver configuration and start.
 *
 * @param[in] canp      pointer to a @p CANDriver object
 *
 * @notapi
 */
void can_lld_start(CANDriver *canp) {
  CAN_InitTypeDef init;
  uint8_t status;

  unit_enable(canp->can);

  CAN_StructInit(&init);
  init.CAN_Prescaler = canp->config->prescaler;
  init.CAN_Mode = canp->config->mode;
  init.CAN_SJW = canp->config->sjw;
  init.CAN_BS1 = canp->config->bs1;
  init.CAN_BS2 = canp->config->bs2;
  init.CAN_TTCM = DISABLE;
  init.CAN_ABOM = canp->config->abom ? ENABLE : DISABLE;
  init.CAN_AWUM = canp->config->awum ? ENABLE : DISABLE;
  init.CAN_NART = canp->config->nart ? ENABLE : DISABLE;
  init.CAN_RFLM = DISABLE;
  init.CAN_TXFP = DISABLE;

  status = CAN_Init(canp->can, &init);
  osalDbgAssert(status == CANINITOK, "CAN initialization failed");

  /* Interrupts: TX completion, both FIFOs pending and overrun, errors. */
  CAN_ITConfig(canp->can,
               CAN_IT_TME | CAN_IT_FMP0 | CAN_IT_FMP1 |
               CAN_IT_FOV0 | CAN_IT_FOV1 |
               CAN_IT_ERR | CAN_IT_EWG | CAN_IT_EPV |
               CAN_IT_BOF | CAN_IT_LEC,
               ENABLE);
}

/**
 * @brief   CAN driver stop.
 *
 * @param[in] canp      pointer to a @p CANDriver object
 *
 * @notapi
 */
void can_lld_stop(CANDriver *canp) {

  CAN_ITConfig(canp->can, 0xFFFFFFFFUL, DISABLE);
  unit_disable(canp->can);
}

/**
 * @brief   Returns whether the given transmit mailbox (or any of them)
 *          is empty.
 *
 * @notapi
 */
bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox) {
  uint32_t sr = canp->can->TSTATR;

  if (mailbox == CAN_ANY_MAILBOX) {
    return (bool)((sr & (CH32_CAN_TME(0) | CH32_CAN_TME(1) |
                         CH32_CAN_TME(2))) != 0U);
  }

  return (bool)((sr & CH32_CAN_TME(mailbox - 1U)) != 0U);
}

/**
 * @brief   Transmission.
 *
 * @notapi
 */
void can_lld_transmit(CANDriver *canp,
                      canmbx_t mailbox,
                      const CANTxFrame *ctfp) {
  CanTxMsg msg;

  (void)mailbox;  /* The hardware selects the first free mailbox. */

  msg.StdId = ctfp->SID;
  msg.ExtId = ctfp->EID;
  msg.IDE = ctfp->IDE;
  msg.RTR = ctfp->RTR;
  msg.DLC = ctfp->DLC;
  memcpy(msg.Data, ctfp->data8, 8U);

  (void)CAN_Transmit(canp->can, &msg);
}

/**
 * @brief   Returns whether the given receive mailbox (or any of them)
 *          is non-empty.
 *
 * @notapi
 */
bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox) {

  if ((mailbox == CAN_ANY_MAILBOX) || (mailbox == 1U)) {
    if ((canp->can->RFIFO0 & CH32_CAN_FMP_MASK) != 0U) {
      return true;
    }
  }
  if ((mailbox == CAN_ANY_MAILBOX) || (mailbox == 2U)) {
    if ((canp->can->RFIFO1 & CH32_CAN_FMP_MASK) != 0U) {
      return true;
    }
  }

  return false;
}

/**
 * @brief   Reception (the vendor call also releases the FIFO slot).
 *
 * @notapi
 */
void can_lld_receive(CANDriver *canp,
                     canmbx_t mailbox,
                     CANRxFrame *crfp) {
  CanRxMsg msg;
  uint8_t fifo;

  if ((mailbox == 1U) ||
      ((mailbox == CAN_ANY_MAILBOX) &&
       ((canp->can->RFIFO0 & CH32_CAN_FMP_MASK) != 0U))) {
    fifo = 0U;
  }
  else {
    fifo = 1U;
  }

  CAN_Receive(canp->can, fifo, &msg);

  crfp->FMI = 0U;
  crfp->TIME = 0U;
  crfp->DLC = msg.DLC;
  crfp->RTR = msg.RTR;
  crfp->IDE = msg.IDE;
  if (msg.IDE == CAN_Id_Extended) {
    crfp->EID = msg.ExtId;
  }
  else {
    crfp->SID = msg.StdId;
  }
  memcpy(crfp->data8, msg.Data, 8U);
}

/**
 * @brief   Abort on the given transmit mailbox (or all of them).
 *
 * @notapi
 */
void can_lld_abort(CANDriver *canp, canmbx_t mailbox) {

  if (mailbox == CAN_ANY_MAILBOX) {
    CAN_CancelTransmit(canp->can, 0U);
    CAN_CancelTransmit(canp->can, 1U);
    CAN_CancelTransmit(canp->can, 2U);
  }
  else {
    CAN_CancelTransmit(canp->can, (uint8_t)(mailbox - 1U));
  }
}

/**
 * @brief   Programs filter banks through the shared filter controller
 *          on CAN1 (vendor API covers banks 0..27).
 *
 * @note    Call with the CAN1 driver; bank ownership between the three
 *          controllers is set with the vendor CAN_SlaveStartBank().
 *
 * @notapi
 */
void canWCHSetFilters(CANDriver *canp, uint32_t num, const CANFilter *cfp) {
  uint32_t i;

  osalDbgAssert(canp == &CAND1, "filters live on the CAN1 block");

  for (i = 0U; i < num; i++, cfp++) {
    CAN_FilterInitTypeDef f;

    f.CAN_FilterIdHigh = (uint16_t)(cfp->register1 >> 16);
    f.CAN_FilterIdLow = (uint16_t)(cfp->register1 & 0xFFFFU);
    f.CAN_FilterMaskIdHigh = (uint16_t)(cfp->register2 >> 16);
    f.CAN_FilterMaskIdLow = (uint16_t)(cfp->register2 & 0xFFFFU);
    f.CAN_FilterFIFOAssignment = cfp->assignment;
    f.CAN_FilterNumber = (uint8_t)cfp->filter;
    f.CAN_FilterMode = cfp->mode ? CAN_FilterMode_IdList :
                                   CAN_FilterMode_IdMask;
    f.CAN_FilterScale = cfp->scale ? CAN_FilterScale_32bit :
                                     CAN_FilterScale_16bit;
    f.CAN_FilterActivation = ENABLE;

    CAN_FilterInit(&f);
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

static void serve_tx(CANDriver *canp) {
  uint32_t sr = canp->can->TSTATR;
  uint32_t txflags = 0U;

  if ((sr & (CH32_CAN_RQCP(0) | CH32_CAN_RQCP(1) | CH32_CAN_RQCP(2))) == 0U) {
    return;
  }

  if ((sr & CH32_CAN_RQCP(0)) != 0U) {
    if ((sr & CH32_CAN_TME(0)) != 0U) {
      txflags |= 1UL << 0;
    }
  }
  if ((sr & CH32_CAN_RQCP(1)) != 0U) {
    if ((sr & CH32_CAN_TME(1)) != 0U) {
      txflags |= 1UL << 1;
    }
  }
  if ((sr & CH32_CAN_RQCP(2)) != 0U) {
    if ((sr & CH32_CAN_TME(2)) != 0U) {
      txflags |= 1UL << 2;
    }
  }

  /* RQCPx bits are cleared by writing them back. */
  canp->can->TSTATR = sr & (CH32_CAN_RQCP(0) | CH32_CAN_RQCP(1) |
                            CH32_CAN_RQCP(2));

  if (txflags != 0U) {
    _can_tx_empty_isr(canp, txflags);
  }
}

static void serve_fifo0(CANDriver *canp) {
  uint32_t sr = canp->can->RFIFO0;

  if ((sr & CH32_CAN_FMP_MASK) != 0U) {
    _can_rx_full_isr(canp, CAN_MAILBOX_TO_MASK(1U));
  }
  if ((sr & CH32_CAN_FOVR) != 0U) {
    canp->can->RFIFO0 = CH32_CAN_FOVR;
    _can_error_isr(canp, CAN_OVERFLOW_ERROR);
  }
}

static void serve_fifo1(CANDriver *canp) {
  uint32_t sr = canp->can->RFIFO1;

  if ((sr & CH32_CAN_FMP_MASK) != 0U) {
    _can_rx_full_isr(canp, CAN_MAILBOX_TO_MASK(2U));
  }
  if ((sr & CH32_CAN_FOVR) != 0U) {
    canp->can->RFIFO1 = CH32_CAN_FOVR;
    _can_error_isr(canp, CAN_OVERFLOW_ERROR);
  }
}

static void serve_sce(CANDriver *canp) {
  uint32_t errsr = canp->can->ERRSR;
  uint32_t lec = errsr & CH32_CAN_ERRSR_LEC_MASK;
  uint32_t flags = 0U;

  if ((errsr & CH32_CAN_ERRSR_EWGF) != 0U) {
    flags |= CAN_LIMIT_WARNING;
  }
  if ((errsr & CH32_CAN_ERRSR_EPVF) != 0U) {
    flags |= CAN_LIMIT_ERROR;
  }
  if ((errsr & CH32_CAN_ERRSR_BOFF) != 0U) {
    flags |= CAN_BUS_OFF_ERROR;
  }
  if ((lec >= 1U) && (lec <= 6U)) {
    flags |= CAN_FRAMING_ERROR;
  }

  if (flags != 0U) {
    _can_error_isr(canp, flags);
  }

  CAN_ClearITPendingBit(canp->can,
                        CAN_IT_ERR | CAN_IT_LEC | CAN_IT_EWG |
                        CAN_IT_EPV | CAN_IT_BOF);
}

/*===========================================================================*/
/* Driver interrupt handlers (per unit).                                     */
/*===========================================================================*/

#if (CH32H417_CAN_USE_CAN1 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(CAN1_TX_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_tx(&CAND1);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN1_RX0_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo0(&CAND1);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN1_RX1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo1(&CAND1);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN1_SCE_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_sce(&CAND1);
  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_CAN_USE_CAN2 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(CAN2_TX_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_tx(&CAND2);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN2_RX0_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo0(&CAND2);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN2_RX1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo1(&CAND2);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN2_SCE_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_sce(&CAND2);
  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_CAN_USE_CAN3 == TRUE) || defined(__DOXYGEN__)
OSAL_IRQ_HANDLER(CAN3_TX_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_tx(&CAND3);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN3_RX0_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo0(&CAND3);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN3_RX1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_fifo1(&CAND3);
  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(CAN3_SCE_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_sce(&CAND3);
  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_CAN == TRUE */

/** @} */
