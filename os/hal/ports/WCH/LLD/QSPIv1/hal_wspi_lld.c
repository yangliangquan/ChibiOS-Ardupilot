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
 * @file    QSPIv1/hal_wspi_lld.c
 * @brief   CH32H417 WSPI subsystem low level driver source.
 *
 * @details Indirect mode operations are started by the CCR/AR/DR write
 *          sequence (RM 37.3.8) and completed from the unit interrupt:
 *          the FIFO (SR.FLEVEL, 32 bytes) is serviced on the FTF
 *          threshold interrupt, TCF finishes the frame, TEF reports
 *          transfer errors. No DMA is used.
 *
 * @addtogroup WSPI
 * @{
 */

#include "hal.h"

#if (HAL_USE_WSPI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* CR bits (ch32h417.h). */
#define CH32_QSPI_EN                        QSPI_CR_EN
#define CH32_QSPI_ABORT                     QSPI_CR_ABORT
#define CH32_QSPI_FTHRES(n)                 ((uint32_t)(n) << 8)
#define CH32_QSPI_TCIE                      QSPI_CR_TCIE
#define CH32_QSPI_FTIE                      QSPI_CR_FTIE
#define CH32_QSPI_TEIE                      QSPI_CR_TEIE
#define CH32_QSPI_PRESCALER(n)              ((uint32_t)(n) << 24)

/* SR bits. */
#define CH32_QSPI_TEF                       QSPI_SR_TEF
#define CH32_QSPI_TCF                       QSPI_SR_TCF
#define CH32_QSPI_FTF                       QSPI_SR_FTF
#define CH32_QSPI_TOF                       QSPI_SR_TOF
#define CH32_QSPI_BUSY                      QSPI_SR_BUSY
#define CH32_QSPI_FLEVEL_MASK               QSPI_SR_FLEVEL

/* FCR bits. */
#define CH32_QSPI_CTEF                      QSPI_FCR_CTEF
#define CH32_QSPI_CTCF                      QSPI_FCR_CTCF
#define CH32_QSPI_CTOF                      QSPI_FCR_CTOF

/* CCR positions (RM 37.4.6). */
#define CH32_QSPI_CCR_INSTRUCTION(n)        ((uint32_t)(n) & 0xFFUL)
#define CH32_QSPI_CCR_DUMMY(n)              ((uint32_t)(n) << 18)
#define CH32_QSPI_CCR_FMODE_WRITE           (0UL << 26)
#define CH32_QSPI_CCR_FMODE_READ            (1UL << 26)
#define CH32_QSPI_CCR_FMODE_MMAP            (3UL << 26)

/* FIFO depth in bytes. */
#define CH32_QSPI_FIFO_SIZE                 32U

#define CH32_QSPI_IE_MASK   (CH32_QSPI_TCIE | CH32_QSPI_FTIE |           \
                             CH32_QSPI_TEIE)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_WSPI_USE_QSPI1 == TRUE) && !defined(__DOXYGEN__)
/** @brief QSPI1 driver identifier.*/
WSPIDriver WSPID1 = { .qspi = QSPI1, .mapbase = CH32H417_QSPI1_MEMMAP_BASE };
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE) && !defined(__DOXYGEN__)
/** @brief QSPI2 driver identifier.*/
WSPIDriver WSPID2 = { .qspi = QSPI2, .mapbase = CH32H417_QSPI2_MEMMAP_BASE };
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Current FIFO level in bytes.
 */
static uint32_t fifo_level(QSPI_TypeDef *q) {

  return (q->SR & CH32_QSPI_FLEVEL_MASK) >> 8;
}

/**
 * @brief   Prepares the CCR/AR registers of a frame.
 * @note    Ordering per RM 37.3.8: CCR first, AR last (the AR write
 *          kicks the frame when an address phase exists; without an
 *          address the CCR write kicks command-only frames and the
 *          first DR write kicks indirect writes).
 */
static void load_frame(WSPIDriver *wspip, const wspi_command_t *cmdp,
                       uint32_t fmode) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t ccr = CH32_QSPI_CCR_INSTRUCTION(cmdp->cmd) |
                 (cmdp->cfg & ~WSPI_CFG_SIOO) |
                 CH32_QSPI_CCR_DUMMY(cmdp->dummy) |
                 fmode;

  if ((cmdp->cfg & WSPI_CFG_SIOO) != 0U) {
    ccr |= (1UL << 28);
  }

  q->CCR = ccr;
  if ((cmdp->cfg & WSPI_CFG_ADDR_MODE_MASK) != WSPI_CFG_ADDR_MODE_NONE) {
    q->AR = cmdp->addr;
  }
}

/**
 * @brief   FIFO service for a running indirect transfer.
 */
static void fifo_service(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t level = fifo_level(q);

  if (wspip->state == WSPI_SEND) {
    while ((level < CH32_QSPI_FIFO_SIZE) && (wspip->txrem > 0U)) {
      q->DR = *wspip->txbuf++;
      wspip->txrem--;
      level++;
    }
  }
  else if (wspip->state == WSPI_RECEIVE) {
    while ((level > 0U) && (wspip->rxrem > 0U)) {
      *wspip->rxbuf++ = (uint8_t)q->DR;
      wspip->rxrem--;
      level--;
    }
  }
}

/**
 * @brief   Shared service routine for one QSPI unit.
 *
 * @notapi
 */
static void serve_interrupt(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t sr = q->SR;

  if ((sr & (CH32_QSPI_TEF | CH32_QSPI_TOF)) != 0U) {
    /* Transfer error or timeout: clear the frame state and signal
       failure through the portable macro.*/
    q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTOF | CH32_QSPI_CTCF;
    q->CR &= (uint32_t)~CH32_QSPI_IE_MASK;
    _wspi_error_code(wspip);
    return;
  }

  if ((sr & CH32_QSPI_FTF) != 0U) {
    fifo_service(wspip);
  }

  if ((sr & CH32_QSPI_TCF) != 0U) {
    if (wspip->state == WSPI_RECEIVE) {
      /* Final drain: the complete frame is in the FIFO now. */
      while ((fifo_level(q) > 0U) && (wspip->rxrem > 0U)) {
        *wspip->rxbuf++ = (uint8_t)q->DR;
        wspip->rxrem--;
      }
    }
    q->FCR = CH32_QSPI_CTCF;
    q->CR &= (uint32_t)~CH32_QSPI_IE_MASK;
    _wspi_isr_code(wspip);
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_WSPI_USE_QSPI1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   QSPI1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(QSPI1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&WSPID1);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_WSPI_USE_QSPI2 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   QSPI2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(QSPI2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&WSPID2);

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level WSPI driver initialization.
 *
 * @notapi
 */
void wspi_lld_init(void) {

#if (CH32H417_WSPI_USE_QSPI1 == TRUE)
  wspiObjectInit(&WSPID1);
  NVIC_EnableIRQ(QSPI1_IRQn);
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE)
  wspiObjectInit(&WSPID2);
  NVIC_EnableIRQ(QSPI2_IRQn);
#endif
}

/**
 * @brief   Configures and activates the WSPI peripheral.
 *
 * @notapi
 */
void wspi_lld_start(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t presc;

  /* f_sck = CH32H417_QSPI_CLOCK / (2 * (presc + 1)) (F7 style). */
  presc = (CH32H417_QSPI_CLOCK +
           (2U * wspip->config->clock_hz) - 1U) /
          (2U * wspip->config->clock_hz);
  if (presc > 0U) {
    presc -= 1U;
  }
  if (presc > 255U) {
    presc = 255U;
  }

  if (wspip->state == WSPI_STOP) {
#if (CH32H417_WSPI_USE_QSPI1 == TRUE)
    if (wspip == &WSPID1) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI1, ENABLE);
    }
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE)
    if (wspip == &WSPID2) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI2, ENABLE);
    }
#endif
    /* Disable a possibly running unit before reconfiguration. */
    q->CR = 0U;
  }

  /* WSPI setup and enable: CR without EN, DCR, then EN (vendor model).*/
  q->CR  = CH32_QSPI_PRESCALER(presc) |
           CH32_QSPI_FTHRES(CH32H417_QSPI_FTHRES);
  q->DCR = ((uint32_t)wspip->config->fsize << 16);  /* CSHT=1, CKMODE=0 */
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;
  q->CR |= CH32_QSPI_EN;
}

/**
 * @brief   Deactivates the WSPI peripheral.
 *
 * @notapi
 */
void wspi_lld_stop(WSPIDriver *wspip) {

  if (wspip->state == WSPI_READY) {
    wspip->qspi->CR = 0U;

#if (CH32H417_WSPI_USE_QSPI1 == TRUE)
    if (wspip == &WSPID1) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI1, DISABLE);
    }
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE)
    if (wspip == &WSPID2) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI2, DISABLE);
    }
#endif
  }
}

/**
 * @brief   Sends a command without data phase.
 *
 * @notapi
 */
void wspi_lld_command(WSPIDriver *wspip, const wspi_command_t *cmdp) {
  QSPI_TypeDef *q = wspip->qspi;

  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = 0U;
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_WRITE);
}

/**
 * @brief   Sends a command with data over the WSPI bus.
 *
 * @notapi
 */
void wspi_lld_send(WSPIDriver *wspip, const wspi_command_t *cmdp,
                   size_t n, const uint8_t *txbuf) {
  QSPI_TypeDef *q = wspip->qspi;

  wspip->txbuf = txbuf;
  wspip->txrem = n;

  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = (uint32_t)(n - 1U);
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_WRITE);

  /* The first DR write kicks the frame when no address phase exists,
     with an address the AR write kicked it already; either way data is
     now pushed into the FIFO, further bytes follow on FTF. */
  fifo_service(wspip);
}

/**
 * @brief   Sends a command then receives data over the WSPI bus.
 *
 * @notapi
 */
void wspi_lld_receive(WSPIDriver *wspip, const wspi_command_t *cmdp,
                      size_t n, uint8_t *rxbuf) {
  QSPI_TypeDef *q = wspip->qspi;

  wspip->rxbuf = rxbuf;
  wspip->rxrem = n;

  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = (uint32_t)(n - 1U);
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_READ);
}

#if (WSPI_SUPPORTS_MEMMAP == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Maps in memory space a WSPI flash device.
 *
 * @notapi
 */
void wspi_lld_map_flash(WSPIDriver *wspip,
                        const wspi_command_t *cmdp,
                        uint8_t **addrp) {

  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_MMAP);

  if (addrp != NULL) {
    *addrp = (uint8_t *)wspip->mapbase;
  }
}

/**
 * @brief   Unmaps from memory space a WSPI flash device.
 *
 * @notapi
 */
void wspi_lld_unmap_flash(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;

  q->CR |= CH32_QSPI_ABORT;
  while ((q->CR & CH32_QSPI_ABORT) != 0U) {
  }

  /* Leave the unit in a defined indirect state. */
  q->FCR = CH32_QSPI_CTCF | CH32_QSPI_CTEF | CH32_QSPI_CTOF;
}
#endif /* WSPI_SUPPORTS_MEMMAP == TRUE */

#endif /* HAL_USE_WSPI == TRUE */

/** @} */
