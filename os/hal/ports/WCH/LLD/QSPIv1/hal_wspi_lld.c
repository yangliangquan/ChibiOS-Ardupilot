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
 * @details Frames are staged in CCR/AR/DLR and started with CR.START, the
 *          data phase runs through the DMA on DR and the frame completes
 *          from the unit interrupt.
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
#define CH32_QSPI_SIOXEN                    (1UL << 13)
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
#define CH32_QSPI_IDLEF                     QSPI_SR_IDLEF
#define CH32_QSPI_FLEVEL                    QSPI_SR_FLEVEL

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

/* Bit 1 of each phase mode field (IMODE bit9, ADMODE bit11, ABMODE bit15,
   DMODE bit25): set for both the two-line and the four-line encodings. */
#define CH32_QSPI_CCR_MULTILINE             ((1UL << 9) | (1UL << 11) | \
                                             (1UL << 15) | (1UL << 25))

/* FTF is left out on purpose: it is a level flag paced by the DMA and the
   handler does not clear it, its interrupt would storm the CPU. */
#define CH32_QSPI_IE_MASK   (CH32_QSPI_TCIE | CH32_QSPI_TEIE)

/* Bounds of the waits below, they only exist to contain a stalled unit
   instead of freezing the system. */
#define CH32_QSPI_DMA_DRAIN_GUARD           1000000U
#define CH32_QSPI_ABORT_GUARD               1000000U
#define CH32_QSPI_IDLE_GUARD                1000000U

/**
 * @brief   DMA service routine for the channels owned by this driver.
 * @note    Completion comes from the QSPI interrupt, this only exists
 *          because the stream allocation requires a callback.
 *
 * @param[in] p         driver instance passed as callback argument
 * @param[in] flags     DMA interrupt flags
 */
static void wspi_lld_serve_dma_interrupt(void *p, uint32_t flags) {

  (void)p;
  (void)flags;
}

/**
 * @brief   Waits for the bus to return idle.
 * @details IDLEF states that no frame is on the bus anymore (RM 37.4.3),
 *          the vendor library waits for it before arming every frame.
 *
 * @notapi
 */
static void wait_idle(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;
  uint32_t guard = CH32_QSPI_IDLE_GUARD;

  /* IDLEF is not meaningful while the unit is disabled. */
  if ((q->CR & CH32_QSPI_EN) == 0U) {
    return;
  }

  while (((q->SR & CH32_QSPI_IDLEF) == 0U) && (guard > 0U)) {
    guard--;
  }
}

/**
 * @brief   Tears down a possibly half-written frame.
 * @details ABORT clears the FIFO and drops nCS (RM 37.4.1). BUSY and IDLEF
 *          are waited before the flags are dropped, otherwise the TCF
 *          raised by the abort itself would be read as the completion of
 *          the frame armed right after.
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

  guard = CH32_QSPI_ABORT_GUARD;
  while (((q->SR & CH32_QSPI_BUSY) != 0U) && (guard > 0U)) {
    guard--;
  }

  wait_idle(wspip);
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;
}

/**
 * @brief   Prepares the unit for a new frame.
 * @details A frame that completed normally leaves the bus idle and the FIFO
 *          empty, so the teardown only runs when the previous frame is still
 *          on the bus or left bytes behind; the flags are dropped either
 *          way. Back to back indirect frames skip the whole ABORT sequence.
 *
 * @notapi
 */
static void begin_frame(WSPIDriver *wspip) {
  QSPI_TypeDef *q = wspip->qspi;

  if ((q->SR & (CH32_QSPI_BUSY | CH32_QSPI_FLEVEL)) != 0U) {
    abort_frame(wspip);
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

  /* Multi-line phases are gated by CR.SIOXEN, the vendor library wraps
     every two- or four-line frame with QSPI_EnableQuad(); one-line frames
     run with it cleared. */
  if ((ccr & CH32_QSPI_CCR_MULTILINE) != 0U) {
    q->CR |= CH32_QSPI_SIOXEN;
  }
  else {
    q->CR &= (uint32_t)~CH32_QSPI_SIOXEN;
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

  /* The request line is dropped first, the frame is over either way. */
  q->CR &= (uint32_t)~(CH32_QSPI_IE_MASK | CH32_QSPI_DMAEN);

  /* A data frame owns the channel: it is counted out so the last FIFO
     bytes reach the buffer, then stopped. A command only frame never
     touched it. */
  if ((wspip->state == WSPI_SEND) || (wspip->state == WSPI_RECEIVE)) {
    uint32_t guard = CH32_QSPI_DMA_DRAIN_GUARD;

    while ((wspip->dma->channel->CNTR > 0U) && (guard > 0U)) {
      guard--;
    }
    ch32DmaStreamClearInterrupt(wspip->dma);
    ch32DmaStreamDisable(wspip->dma);
  }

  q->FCR = CH32_QSPI_CTCF | CH32_QSPI_CTEF | CH32_QSPI_CTOF;

  /* Completion wins over the error flags: the abort preceding every frame
     is acknowledged by a TCF too, so TEF can accompany a frame that ran
     correctly. Leftover FIFO bytes and the BUSY bit are settled by the
     abort that opens the next frame, in thread context. */
  if ((sr & CH32_QSPI_TCF) != 0U) {
    _wspi_isr_code(wspip);
  }
  else if ((sr & (CH32_QSPI_TEF | CH32_QSPI_TOF)) != 0U) {
    _wspi_error_code(wspip);
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

  /* f_sck = CH32H417_QSPI_CLOCK / (presc + 1), the prescaler divides
     FHCLK (RM 26.2.4). */
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
    /* The peripheral address is fixed for the lifetime of the channel. */
    ch32DmaStreamSetPeripheral(wspip->dma, &wspip->qspi->DR);

    q->CR = 0U;
  }

  /* CR without EN, DCR, then EN. DMAEN is armed by the data frames only. */
  q->CR  = CH32_QSPI_PRESCALER(presc) |
           CH32_QSPI_FTHRES(CH32H417_QSPI_FTHRES);
  q->DCR = ((uint32_t)wspip->config->fsize << 16);  /* CSHT=1, CKMODE=0 */
  q->FCR = CH32_QSPI_CTEF | CH32_QSPI_CTCF | CH32_QSPI_CTOF;
  q->CR |= CH32_QSPI_EN;

  /* A frame left over from the previous activation is dropped once the unit
     is clocked again: ABORT only self-clears while EN is set, doing it on a
     disabled unit leaves the bit pending and the next frame inherits it. */
  abort_frame(wspip);
}

/**
 * @brief   Deactivates the WSPI peripheral.
 *
 * @notapi
 */
void wspi_lld_stop(WSPIDriver *wspip) {

  if (wspip->state == WSPI_READY) {
    /* Interrupts are disabled first: the abort below raises a completion
       flag and the handler would resume a thread that is not waiting, which
       is a kernel state violation rather than a bus event. */
    wspip->qspi->CR &= (uint32_t)~(CH32_QSPI_IE_MASK | CH32_QSPI_DMAEN);

    abort_frame(wspip);

    wspip->qspi->CR = 0U;

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

  /* No data phase is stated by DMODE=00, and DLR must not contradict it:
     DLR=0xFFFFFFFF is "undefined length", the unit would clock the whole
     flash (RM 37.4.5). DLR=0 keeps the frame at the instruction phase plus
     one byte slot. */
  begin_frame(wspip);
  q->CR &= (uint32_t)~CH32_QSPI_DMAEN;
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

  begin_frame(wspip);

  ch32DmaStreamSetMemory0(wspip->dma, txbuf);
  ch32DmaStreamSetTransactionSize(wspip->dma, (uint32_t)n);
  ch32DmaStreamSetMode(wspip->dma, wspip->dmamode |
                                   CH32_DMA_CFGR_DIR_M2P);

  q->CR |= CH32_QSPI_IE_MASK | CH32_QSPI_DMAEN;
  q->DLR = (uint32_t)(n - 1U);

  /* Staging order as in the vendor library: CCR, AR, DLR, then the request
     line and the channel, START last. The unit pauses the shift register
     while the FIFO is empty (RM 37.3.3), so the channel may preload it. */
  load_frame(wspip, cmdp, CH32_QSPI_CCR_FMODE_WRITE);
  ch32DmaStreamEnable(wspip->dma);
  q->CR |= CH32_QSPI_START;
}

/**
 * @brief   Sends a command then receives data over the WSPI bus.
 *
 * @notapi
 */
void wspi_lld_receive(WSPIDriver *wspip, const wspi_command_t *cmdp,
                      size_t n, uint8_t *rxbuf) {
  QSPI_TypeDef *q = wspip->qspi;

  begin_frame(wspip);

  ch32DmaStreamSetMemory0(wspip->dma, rxbuf);
  ch32DmaStreamSetTransactionSize(wspip->dma, (uint32_t)n);
  ch32DmaStreamSetMode(wspip->dma, wspip->dmamode |
                                   CH32_DMA_CFGR_DIR_P2M);

  q->CR |= CH32_QSPI_IE_MASK | CH32_QSPI_DMAEN;
  q->DLR = (uint32_t)(n - 1U);

  /* The channel is live before the frame starts, each threshold crossing
     raises a request and the DMA drains the FIFO until DLR is exhausted. */
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

  wait_idle(wspip);

  /* No DMA request while in memory mapped mode.*/
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

  /* Drops nCS and clears the prefetch FIFO, the flags it raises are
     swallowed by the helper. */
  abort_frame(wspip);

  /* The indirect data frames arm the request line themselves. */
  q->CR &= (uint32_t)~CH32_QSPI_DMAEN;

  q->FCR = CH32_QSPI_CTCF | CH32_QSPI_CTEF | CH32_QSPI_CTOF;
}
#endif /* WSPI_SUPPORTS_MEMMAP == TRUE */

#endif /* HAL_USE_WSPI == TRUE */

/** @} */
