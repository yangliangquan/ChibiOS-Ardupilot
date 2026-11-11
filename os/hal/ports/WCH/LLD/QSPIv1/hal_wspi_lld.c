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
 * @details Frames are staged in the CCR/AR/DLR registers and launched
 *          with CR.START, the data phase is served by the DMA through
 *          the DR register and the frame is completed from the unit
 *          interrupt on TCF.
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
#define CH32_QSPI_DMAEN                     QSPI_CR_DMAEN
#define CH32_QSPI_START                     QSPI_CR_START
#define CH32_QSPI_FTHRES(n)                 ((uint32_t)(n) << 8)
#define CH32_QSPI_TCIE                      QSPI_CR_TCIE
#define CH32_QSPI_FTIE                      QSPI_CR_FTIE
#define CH32_QSPI_TEIE                      QSPI_CR_TEIE
#define CH32_QSPI_PRESCALER(n)              ((uint32_t)(n) << 24)

/* SR bits. */
#define CH32_QSPI_TEF                       QSPI_SR_TEF
#define CH32_QSPI_TCF                       QSPI_SR_TCF
#define CH32_QSPI_TOF                       QSPI_SR_TOF
#define CH32_QSPI_BUSY                      QSPI_SR_BUSY

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

/* Only the frame complete and error sources are enabled. FTF is a level
   flag paced by the DMA, enabling its interrupt storms the CPU for the
   whole data phase because the handler does not clear it. */
#define CH32_QSPI_IE_MASK   (CH32_QSPI_TCIE | CH32_QSPI_TEIE)

/* Iteration bound of the interrupt level drain wait. It only exists to
   contain a stalled DMA channel instead of freezing the system from
   the handler. */
#define CH32_QSPI_DMA_DRAIN_GUARD           1000000U

/* Iteration bound of the abort wait, the bit self-clears when the
   transaction is torn down. */
#define CH32_QSPI_ABORT_GUARD               1000000U

/**
 * @brief   DMA service routine for the channels owned by this driver.
 * @note    Completion is signalled by the QSPI interrupt, only transfer
 *          errors would be observed here.
 *
 * @param[in] p         driver instance passed as callback argument
 * @param[in] flags     DMA interrupt flags
 */
static void wspi_lld_serve_dma_interrupt(void *p, uint32_t flags) {

  (void)p;
  (void)flags;
}

/**
 * @brief   Tears down a possibly half-written frame.
 * @details ABORT stops an ongoing or pending transaction and clears
 *          the FIFO, the bit self-clears when it is done (RM 37.4.1).
 *          The frame flags are dropped afterwards, the abort can raise
 *          some of them. The wait is bounded, the unit must be clocked.
 *
 * @notapi
 */
static void abort_frame(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t guard = CH32_QSPI_ABORT_GUARD;

  q->CR |= CH32_QSPI_ABORT;
  while (((q->CR & CH32_QSPI_ABORT) != 0U) && (guard > 0U)) {
    guard--;
  }
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;
}

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
 * @brief   Prepares the CCR/AR registers of a frame.
 * @note    CCR first, AR last (RM 37.3.8).
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

  q->ABR = cmdp->alt;
  q->CCR = ccr;
  if ((cmdp->cfg & WSPI_CFG_ADDR_MODE_MASK) != WSPI_CFG_ADDR_MODE_NONE) {
    q->AR = cmdp->addr;
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
    /* Transfer error or timeout: stop the channel and tear down the
       frame, a frame left pending would poison the following ones.*/
    q->CR &= (uint32_t)~CH32_QSPI_IE_MASK;
    ch32DmaStreamDisable(wspip->dma);
    abort_frame(wspip);
    _wspi_error_code(wspip);
    return;
  }

  if ((sr & CH32_QSPI_TCF) != 0U) {
    /* Command only frames have no data phase and never touch the DMA
       channel, the teardown applies to data frames only.*/
    if (((q->CCR & WSPI_CFG_DATA_MODE_MASK) != 0U) &&
        ((wspip->state == WSPI_SEND) || (wspip->state == WSPI_RECEIVE))) {
      bool rx = (wspip->state == WSPI_RECEIVE);
      uint32_t guard = CH32_QSPI_DMA_DRAIN_GUARD;

      /* Same sequence as the STM32 QUADSPI driver: the channel is
         counted out before being stopped, then its interrupts are
         cleared and the channel released. The wait is bounded so a
         channel that never drains reports an error instead of freezing
         the system in the handler.*/
      while ((wspip->dma->channel->CNTR > 0U) && (guard > 0U)) {
        guard--;
      }
      if (guard == 0U) {
        q->CR &= (uint32_t)~CH32_QSPI_IE_MASK;
        ch32DmaStreamDisable(wspip->dma);
        abort_frame(wspip);
        _wspi_error_code(wspip);
        return;
      }
      ch32DmaStreamClearInterrupt(wspip->dma);
      ch32DmaStreamDisable(wspip->dma);

      /* BUSY only clears when the FIFO is empty (RM 37.3.11): a read
         may leave surplus bytes that are dropped reading DR, a write
         drains through the shift register.*/
      while (((q->SR & CH32_QSPI_BUSY) != 0U) && (guard > 0U)) {
        if (rx) {
          (void)*((volatile uint8_t *)&q->DR);
        }
        guard--;
      }
      if (guard == 0U) {
        q->CR &= (uint32_t)~CH32_QSPI_IE_MASK;
        abort_frame(wspip);
        _wspi_error_code(wspip);
        return;
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
  WSPID1.dma = NULL;
  WSPID1.dmamode = CH32_DMA_CFGR_PL(CH32H417_WSPI_QSPI1_DMA_PRIORITY) |
                   CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE |
                   CH32_DMA_CFGR_MINC;
  NVIC_SetPriority(QSPI1_IRQn,
                   (uint8_t)CH32_IRQ_PRIORITY(CH32H417_WSPI_QSPI1_IRQ_PRIORITY));
  NVIC_EnableIRQ(QSPI1_IRQn);
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE)
  wspiObjectInit(&WSPID2);
  WSPID2.dma = NULL;
  WSPID2.dmamode = CH32_DMA_CFGR_PL(CH32H417_WSPI_QSPI2_DMA_PRIORITY) |
                   CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE |
                   CH32_DMA_CFGR_MINC;
  NVIC_SetPriority(QSPI2_IRQn,
                   (uint8_t)CH32_IRQ_PRIORITY(CH32H417_WSPI_QSPI2_IRQ_PRIORITY));
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

  /* f_sck = CH32H417_QSPI_CLOCK / (presc + 1), the QSPI prescaler
     divides FHCLK (RM 26.2.4). */
  presc = (CH32H417_QSPI_CLOCK +
           wspip->config->clock_hz - 1U) /
          wspip->config->clock_hz;
  if (presc > 0U) {
    presc -= 1U;
  }
  if (presc > 255U) {
    presc = 255U;
  }

  if (wspip->state == WSPI_STOP) {
    /* First activation: DMA channel allocation and routing. */
#if (CH32H417_WSPI_USE_QSPI1 == TRUE)
    if (wspip == &WSPID1) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI1, ENABLE);
      wspip->dma = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                       CH32H417_WSPI_QSPI1_DMA_PRIORITY,
                                       (ch32_dmaisr_t)wspi_lld_serve_dma_interrupt,
                                       (void *)wspip);
      osalDbgAssert(wspip->dma != NULL, "unable to allocate stream");
      ch32DmaStreamSetRequest(wspip->dma, CH32_DMAMUX_QSPI1);
    }
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE)
    if (wspip == &WSPID2) {
      RCC_HB1PeriphClockCmd(RCC_HB1Periph_QSPI2, ENABLE);
      wspip->dma = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                       CH32H417_WSPI_QSPI2_DMA_PRIORITY,
                                       (ch32_dmaisr_t)wspi_lld_serve_dma_interrupt,
                                       (void *)wspip);
      osalDbgAssert(wspip->dma != NULL, "unable to allocate stream");
      ch32DmaStreamSetRequest(wspip->dma, CH32_DMAMUX_QSPI2);
    }
#endif
    /* The DR address is fixed for the lifetime of the channel, the
       direction and the buffer change from transfer to transfer.*/
    ch32DmaStreamSetPeripheral(wspip->dma, &wspip->qspi->DR);

    /* Release a pending frame before disabling the unit, CR=0 on a
       busy transaction would leave the flash mid-command. */
    abort_frame(wspip);
    q->CR = 0U;
  }

  /* CR without EN, DCR, then EN. DMAEN gates the FIFO threshold
     requests for the whole data phase.*/
  q->CR  = CH32_QSPI_PRESCALER(presc) |
           CH32_QSPI_FTHRES(CH32H417_QSPI_FTHRES) |
           CH32_QSPI_DMAEN;
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
    /* Release a pending frame while the unit is still clocked, gating
       the clock on a busy transaction would leave the flash selected
       mid-command.*/
    wspip->qspi->CR |= CH32_QSPI_ABORT;
    while ((wspip->qspi->CR & CH32_QSPI_ABORT) != 0U) {
    }

    wspip->qspi->CR = 0U;

    /* Releasing the channel.*/
    if (wspip->dma != NULL) {
      ch32DmaStreamFreeI(wspip->dma);
      wspip->dma = NULL;
    }

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

  /* As in the STM32 QUADSPI driver a command only frame uses DLR=0:
     DLR=0xFFFFFFFF means "until the end of the flash" and keeps the
     unit busy (RM 37.4.5).*/
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;
  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = 0U;
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_READ);
  q->CR |= CH32_QSPI_START;
}

/**
 * @brief   Sends a command with data over the WSPI bus.
 *
 * @notapi
 */
void wspi_lld_send(WSPIDriver *wspip, const wspi_command_t *cmdp,
                   size_t n, const uint8_t *txbuf) {
  QSPI_TypeDef *q = wspip->qspi;

  /* Direction change flush: ABORT empties the FIFO and resets its
     internal pointers (RM 37.4.1), which the unit expects whenever the
     data direction changes. Skipping it makes the unit reject the
     first write frame with a transfer error, although the data goes
     out correctly. The unit is idle here and the interrupts are still
     masked, so the abort cannot reach the handler.*/
  abort_frame(wspip);

  /* Flags are cleared before the channel is armed so a leftover FTF
     from the previous frame cannot predate the new one.*/
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;

  ch32DmaStreamSetMemory0(wspip->dma, txbuf);
  ch32DmaStreamSetTransactionSize(wspip->dma, (uint32_t)n);
  ch32DmaStreamSetMode(wspip->dma, wspip->dmamode |
                                   CH32_DMA_CFGR_DIR_M2P);

  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = (uint32_t)(n - 1U);

  /* Staging order as in the vendor flash library: CCR, AR, DLR, START
     and only then the data. The frame must be fully described before
     a byte reaches the data register, a DR write is one of the events
     that trigger the communication start (RM 37.3.8). The channel goes
     live after the start, the unit holds the shift register while the
     FIFO is empty (RM 37.3.3), so no byte is lost.*/
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_WRITE);
  q->CR |= CH32_QSPI_START;
  ch32DmaStreamEnable(wspip->dma);
}

/**
 * @brief   Sends a command then receives data over the WSPI bus.
 *
 * @notapi
 */
void wspi_lld_receive(WSPIDriver *wspip, const wspi_command_t *cmdp,
                      size_t n, uint8_t *rxbuf) {
  QSPI_TypeDef *q = wspip->qspi;

  /* Direction change flush, same reason as in the send path.*/
  abort_frame(wspip);

  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;

  ch32DmaStreamSetMemory0(wspip->dma, rxbuf);
  ch32DmaStreamSetTransactionSize(wspip->dma, (uint32_t)n);
  ch32DmaStreamSetMode(wspip->dma, wspip->dmamode |
                                   CH32_DMA_CFGR_DIR_P2M);

  q->CR |= CH32_QSPI_IE_MASK;
  q->DLR = (uint32_t)(n - 1U);

  /* The channel is live before the frame starts, the first FIFO byte
     must not cross a disabled channel. Each arrival at FTHRES raises a
     request and the DMA drains the FIFO until DLR is exhausted.*/
  ch32DmaStreamEnable(wspip->dma);
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_READ);
  q->CR |= CH32_QSPI_START;
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

  /* Disabling the DMA request while in memory mapped mode.*/
  wspip->qspi->CR &= ~CH32_QSPI_DMAEN;

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

  /* Re-enabling the DMA request, back to indirect mode.*/
  q->CR |= CH32_QSPI_DMAEN;

  /* Leave the unit in a defined indirect state. */
  q->FCR = CH32_QSPI_CTCF | CH32_QSPI_CTEF | CH32_QSPI_CTOF;
}
#endif /* WSPI_SUPPORTS_MEMMAP == TRUE */

#endif /* HAL_USE_WSPI == TRUE */

/** @} */
