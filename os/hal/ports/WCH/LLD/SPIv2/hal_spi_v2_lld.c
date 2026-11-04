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
 * @file    SPIv2/hal_spi_v2_lld.c
 * @brief   CH32H417 SPI subsystem low level driver source (driver v2).
 *
 * @details DMA driven engine, F1-compatible register model, master mode,
 *          8/16 bit frames, up to four units. Every transfer moves through
 *          the RX and TX DMA channels associated to the unit.
 *
 * @addtogroup SPI
 * @{
 */

#include "hal.h"

#if (HAL_USE_SPI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* CTLR1 bits. */
#define CH32_SPI_CPHA                      (1U << 0)
#define CH32_SPI_CPOL                      (1U << 1)
#define CH32_SPI_MSTR                      (1U << 2)
#define CH32_SPI_BR_OFF                    3
#define CH32_SPI_SPE                       (1U << 6)
#define CH32_SPI_LSBFIRST                  (1U << 7)
#define CH32_SPI_SSI                       (1U << 8)
#define CH32_SPI_SSM                       (1U << 9)
#define CH32_SPI_DFF                       (1U << 11)

/* CTLR2 bits. */
#define CH32_SPI_RXDMAEN                   (1U << 0)
#define CH32_SPI_TXDMAEN                   (1U << 1)
#define CH32_SPI_ERRIE                     (1U << 5)

/* STATR bits. */
#define CH32_SPI_RXNE                      (1U << 0)
#define CH32_SPI_TXE                       (1U << 1)
#define CH32_SPI_MODF                      (1U << 5)
#define CH32_SPI_OVR                       (1U << 6)
#define CH32_SPI_BSY                       (1U << 7)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_SPI_USE_SPI1 == TRUE) && !defined(__DOXYGEN__)
SPIDriver SPID1 = { .spi = SPI1 };
#endif
#if (CH32H417_SPI_USE_SPI2 == TRUE) && !defined(__DOXYGEN__)
SPIDriver SPID2 = { .spi = SPI2 };
#endif
#if (CH32H417_SPI_USE_SPI3 == TRUE) && !defined(__DOXYGEN__)
SPIDriver SPID3 = { .spi = SPI3 };
#endif
#if (CH32H417_SPI_USE_SPI4 == TRUE) && !defined(__DOXYGEN__)
SPIDriver SPID4 = { .spi = SPI4 };
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static void unit_enable(SPI_TypeDef *u) {

  if (u == SPI1) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_SPI1, ENABLE);
    NVIC_SetPriority(SPI1_IRQn,
                     (uint8_t)CH32_IRQ_PRIORITY(CH32H417_SPI_SPI1_IRQ_PRIORITY));
    NVIC_EnableIRQ(SPI1_IRQn);
  }
#if CH32H417_SPI_USE_SPI2
  else if (u == SPI2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);
    NVIC_SetPriority(SPI2_IRQn,
                     (uint8_t)CH32_IRQ_PRIORITY(CH32H417_SPI_SPI2_IRQ_PRIORITY));
    NVIC_EnableIRQ(SPI2_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI3
  else if (u == SPI3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI3, ENABLE);
    NVIC_SetPriority(SPI3_IRQn,
                     (uint8_t)CH32_IRQ_PRIORITY(CH32H417_SPI_SPI3_IRQ_PRIORITY));
    NVIC_EnableIRQ(SPI3_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI4
  else if (u == SPI4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI4, ENABLE);
    NVIC_SetPriority(SPI4_IRQn,
                     (uint8_t)CH32_IRQ_PRIORITY(CH32H417_SPI_SPI4_IRQ_PRIORITY));
    NVIC_EnableIRQ(SPI4_IRQn);
  }
#endif
}

static void unit_disable(SPI_TypeDef *u) {

  if (u == SPI1) {
    NVIC_DisableIRQ(SPI1_IRQn);
  }
#if CH32H417_SPI_USE_SPI2
  else if (u == SPI2) {
    NVIC_DisableIRQ(SPI2_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI3
  else if (u == SPI3) {
    NVIC_DisableIRQ(SPI3_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI4
  else if (u == SPI4) {
    NVIC_DisableIRQ(SPI4_IRQn);
  }
#endif
}

/**
 * @brief   Shared end-of-rx service routine.
 *
 * @param[in] spip      pointer to the @p SPIDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void spi_lld_serve_rx_interrupt(SPIDriver *spip, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    /* Stopping DMAs.*/
    ch32DmaStreamDisable(spip->dmatx);
    ch32DmaStreamDisable(spip->dmarx);

    /* Reporting the failure.*/
    __spi_isr_error_code(spip, HAL_RET_HW_FAILURE);
  }
  else if (spip->config->circular) {
    if ((flags & CH32_DMA_ISR_HTIF) != 0U) {
      /* Half buffer interrupt.*/
      __spi_isr_half_code(spip);
    }
    if ((flags & CH32_DMA_ISR_TCIF) != 0U) {
      /* End buffer interrupt.*/
      __spi_isr_full_code(spip);
    }
  }
  else {
    /* Stopping DMAs.*/
    ch32DmaStreamDisable(spip->dmatx);
    ch32DmaStreamDisable(spip->dmarx);

    /* Operation finished interrupt.*/
    __spi_isr_complete_code(spip);
  }
}

/**
 * @brief   Shared end-of-tx service routine.
 *
 * @param[in] spip      pointer to the @p SPIDriver object
 * @param[in] flags     pre-shifted content of the INTFR register
 */
static void spi_lld_serve_tx_interrupt(SPIDriver *spip, uint32_t flags) {

  /* DMA errors handling.*/
  if ((flags & CH32_DMA_ISR_TEIF) != 0U) {
    /* Stopping DMAs.*/
    ch32DmaStreamDisable(spip->dmatx);
    ch32DmaStreamDisable(spip->dmarx);

    /* Reporting the failure.*/
    __spi_isr_error_code(spip, HAL_RET_HW_FAILURE);
  }
}

/**
 * @brief   DMA channel allocation for one SPI unit.
 *
 * @param[in] spip      pointer to the @p SPIDriver object
 * @param[in] rxstream  channel to be allocated for RX
 * @param[in] txstream  channel to be allocated for TX
 * @param[in] priority  channels IRQ priority
 * @return              The operation status.
 */
static msg_t spi_lld_get_dma(SPIDriver *spip, uint32_t rxstream,
                             uint32_t txstream, uint32_t priority) {

  spip->dmarx = ch32DmaStreamAllocI(rxstream, priority,
                                    (ch32_dmaisr_t)spi_lld_serve_rx_interrupt,
                                    (void *)spip);
  if (spip->dmarx == NULL) {
    return HAL_RET_NO_RESOURCE;
  }

  spip->dmatx = ch32DmaStreamAllocI(txstream, priority,
                                    (ch32_dmaisr_t)spi_lld_serve_tx_interrupt,
                                    (void *)spip);
  if (spip->dmatx == NULL) {
    ch32DmaStreamFreeI(spip->dmarx);
    spip->dmarx = NULL;
    return HAL_RET_NO_RESOURCE;
  }

  return HAL_RET_SUCCESS;
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level SPI driver initialization.
 *
 * @return              The operation status.
 *
 * @notapi
 */
msg_t spi_lld_init(void) {

#if (CH32H417_SPI_USE_SPI1 == TRUE) || defined(__DOXYGEN__)
  spiObjectInit(&SPID1);
  SPID1.spi       = SPI1;
  SPID1.dmarx     = NULL;
  SPID1.dmatx     = NULL;
  SPID1.rxdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI1_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TCIE |
                    CH32_DMA_CFGR_TEIE;
  SPID1.txdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI1_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE;
#endif

#if (CH32H417_SPI_USE_SPI2 == TRUE) || defined(__DOXYGEN__)
  spiObjectInit(&SPID2);
  SPID2.spi       = SPI2;
  SPID2.dmarx     = NULL;
  SPID2.dmatx     = NULL;
  SPID2.rxdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI2_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TCIE |
                    CH32_DMA_CFGR_TEIE;
  SPID2.txdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI2_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE;
#endif

#if (CH32H417_SPI_USE_SPI3 == TRUE) || defined(__DOXYGEN__)
  spiObjectInit(&SPID3);
  SPID3.spi       = SPI3;
  SPID3.dmarx     = NULL;
  SPID3.dmatx     = NULL;
  SPID3.rxdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI3_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TCIE |
                    CH32_DMA_CFGR_TEIE;
  SPID3.txdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI3_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE;
#endif

#if (CH32H417_SPI_USE_SPI4 == TRUE) || defined(__DOXYGEN__)
  spiObjectInit(&SPID4);
  SPID4.spi       = SPI4;
  SPID4.dmarx     = NULL;
  SPID4.dmatx     = NULL;
  SPID4.rxdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI4_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_P2M | CH32_DMA_CFGR_TCIE |
                    CH32_DMA_CFGR_TEIE;
  SPID4.txdmamode = CH32_DMA_CFGR_PL(CH32H417_SPI_SPI4_DMA_PRIORITY) |
                    CH32_DMA_CFGR_DIR_M2P | CH32_DMA_CFGR_TEIE;
#endif

  return HAL_RET_SUCCESS;
}

/**
 * @brief   SPI driver configuration and start.
 *
 * @notapi
 */
msg_t spi_lld_start(SPIDriver *spip) {
  const SPIConfig *config = spip->config;
  uint32_t presc = 0U;
  uint32_t div = 2U;
  uint16_t ctr1;
  msg_t msg;

  osalDbgAssert((config->databits == 8U) || (config->databits == 16U),
                "only 8/16 bit frames supported");
  osalDbgAssert(config->clock_freq > 0U, "zero frequency");
  osalDbgAssert(config->mode <= 3U, "invalid mode");

  /* Smallest prescaler giving a bus clock not above the request. */
  while ((div < 256U) && ((CH32H417_SPI_CLOCK / div) > config->clock_freq)) {
    div <<= 1;
    presc++;
  }
  osalDbgAssert((CH32H417_SPI_CLOCK / div) <= config->clock_freq,
                "frequency not obtainable");

  if (spip->state == SPI_STOP) {
    /* First activation, DMA channels allocation and routing.*/
    if (spip->spi == SPI1) {
      msg = spi_lld_get_dma(spip, CH32_DMA_STREAM_ID_ANY,
                            CH32_DMA_STREAM_ID_ANY,
                            CH32H417_SPI_SPI1_DMA_PRIORITY);
      if (msg != HAL_RET_SUCCESS) {
        return msg;
      }
      ch32DmaStreamSetRequest(spip->dmarx, CH32_DMAMUX_SPI1_RX);
      ch32DmaStreamSetRequest(spip->dmatx, CH32_DMAMUX_SPI1_TX);
    }
#if CH32H417_SPI_USE_SPI2
    else if (spip->spi == SPI2) {
      msg = spi_lld_get_dma(spip, CH32_DMA_STREAM_ID_ANY,
                            CH32_DMA_STREAM_ID_ANY,
                            CH32H417_SPI_SPI2_DMA_PRIORITY);
      if (msg != HAL_RET_SUCCESS) {
        return msg;
      }
      ch32DmaStreamSetRequest(spip->dmarx, CH32_DMAMUX_SPI2_RX);
      ch32DmaStreamSetRequest(spip->dmatx, CH32_DMAMUX_SPI2_TX);
    }
#endif
#if CH32H417_SPI_USE_SPI3
    else if (spip->spi == SPI3) {
      msg = spi_lld_get_dma(spip, CH32_DMA_STREAM_ID_ANY,
                            CH32_DMA_STREAM_ID_ANY,
                            CH32H417_SPI_SPI3_DMA_PRIORITY);
      if (msg != HAL_RET_SUCCESS) {
        return msg;
      }
      ch32DmaStreamSetRequest(spip->dmarx, CH32_DMAMUX_SPI3_RX);
      ch32DmaStreamSetRequest(spip->dmatx, CH32_DMAMUX_SPI3_TX);
    }
#endif
#if CH32H417_SPI_USE_SPI4
    else if (spip->spi == SPI4) {
      msg = spi_lld_get_dma(spip, CH32_DMA_STREAM_ID_ANY,
                            CH32_DMA_STREAM_ID_ANY,
                            CH32H417_SPI_SPI4_DMA_PRIORITY);
      if (msg != HAL_RET_SUCCESS) {
        return msg;
      }
      ch32DmaStreamSetRequest(spip->dmarx, CH32_DMAMUX_SPI4_RX);
      ch32DmaStreamSetRequest(spip->dmatx, CH32_DMAMUX_SPI4_TX);
    }
#endif
    else {
      osalDbgAssert(false, "invalid SPI instance");
      return HAL_RET_IS_INVALID;
    }

    /* Data register association.*/
    ch32DmaStreamSetPeripheral(spip->dmarx, &spip->spi->DATAR);
    ch32DmaStreamSetPeripheral(spip->dmatx, &spip->spi->DATAR);

    unit_enable(spip->spi);
  }
  else {
    /* Re-configuration on an already active unit, stop cleanly first.*/
    ch32DmaStreamDisable(spip->dmatx);
    while ((spip->spi->STATR & CH32_SPI_BSY) != 0U) {
    }
    ch32DmaStreamDisable(spip->dmarx);
    spip->spi->CTLR1 &= (uint16_t)~CH32_SPI_SPE;
  }

  /* Frame size dependent DMA configuration.*/
  if (config->databits == 16U) {
    spip->rxdmamode = (spip->rxdmamode & ~CH32_DMA_CFGR_SIZE_MASK) |
                      CH32_DMA_CFGR_PSIZE_HWORD | CH32_DMA_CFGR_MSIZE_HWORD;
    spip->txdmamode = (spip->txdmamode & ~CH32_DMA_CFGR_SIZE_MASK) |
                      CH32_DMA_CFGR_PSIZE_HWORD | CH32_DMA_CFGR_MSIZE_HWORD;
  }
  else {
    spip->rxdmamode = (spip->rxdmamode & ~CH32_DMA_CFGR_SIZE_MASK) |
                      CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
    spip->txdmamode = (spip->txdmamode & ~CH32_DMA_CFGR_SIZE_MASK) |
                      CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE;
  }

  /* Circular transfers need the circular bit and the half interrupt on the
     receive channel.*/
  if (config->circular) {
    spip->rxdmamode |= CH32_DMA_CFGR_CIRC | CH32_DMA_CFGR_HTIE;
    spip->txdmamode |= CH32_DMA_CFGR_CIRC | CH32_DMA_CFGR_HTIE;
  }
  else {
    spip->rxdmamode &= ~(CH32_DMA_CFGR_CIRC | CH32_DMA_CFGR_HTIE);
    spip->txdmamode &= ~(CH32_DMA_CFGR_CIRC | CH32_DMA_CFGR_HTIE);
  }

  ctr1 = CH32_SPI_MSTR | CH32_SPI_SSM | CH32_SPI_SSI |
         (uint16_t)(presc << CH32_SPI_BR_OFF);
  if ((config->mode & 1U) != 0U) {
    ctr1 |= CH32_SPI_CPHA;
  }
  if ((config->mode & 2U) != 0U) {
    ctr1 |= CH32_SPI_CPOL;
  }
  if (config->databits == 16U) {
    ctr1 |= CH32_SPI_DFF;
  }
  if (config->lsb_first) {
    ctr1 |= CH32_SPI_LSBFIRST;
  }

  spip->spi->CTLR1 = ctr1;
  spip->spi->CTLR2 = CH32_SPI_RXDMAEN | CH32_SPI_TXDMAEN | CH32_SPI_ERRIE;
  spip->spi->STATR;
  spip->spi->DATAR;
  spip->spi->CTLR1 |= CH32_SPI_SPE;

  return HAL_RET_SUCCESS;
}

/**
 * @brief   SPI driver stop.
 *
 * @notapi
 */
void spi_lld_stop(SPIDriver *spip) {

  /* Stopping TX DMA then waiting for the current frame completion.*/
  ch32DmaStreamDisable(spip->dmatx);
  while ((spip->spi->STATR & CH32_SPI_BSY) != 0U) {
  }

  /* Stopping RX DMA and the peripheral.*/
  ch32DmaStreamDisable(spip->dmarx);
  spip->spi->CTLR2 = 0U;
  spip->spi->CTLR1 &= (uint16_t)~CH32_SPI_SPE;

  ch32DmaStreamFreeI(spip->dmatx);
  ch32DmaStreamFreeI(spip->dmarx);
  spip->dmarx = NULL;
  spip->dmatx = NULL;

  unit_disable(spip->spi);
}

/**
 * @brief   Programs and starts both DMA channels for a transfer.
 */
static msg_t start_transfer(SPIDriver *spip,
                            size_t n,
                            const void *txbuf,
                            void *rxbuf) {

  osalDbgAssert(n <= CH32_DMA_MAX_TRANSFER, "unsupported DMA transfer size");

  /* Receive side, a dummy sink is used when the data is to be discarded.*/
  if (rxbuf != NULL) {
    ch32DmaStreamSetMemory0(spip->dmarx, rxbuf);
    ch32DmaStreamSetMode(spip->dmarx, spip->rxdmamode | CH32_DMA_CFGR_MINC);
  }
  else {
    ch32DmaStreamSetMemory0(spip->dmarx, &spip->rxsink);
    ch32DmaStreamSetMode(spip->dmarx, spip->rxdmamode);
  }
  ch32DmaStreamSetTransactionSize(spip->dmarx, n);

  /* Transmit side, a filler word is used when no data is supplied.*/
  if (txbuf != NULL) {
    ch32DmaStreamSetMemory0(spip->dmatx, txbuf);
    ch32DmaStreamSetMode(spip->dmatx, spip->txdmamode | CH32_DMA_CFGR_MINC);
  }
  else {
    spip->txsource = (spip->config->databits > 8U) ? 0x0000FFFFU : 0x000000FFU;
    ch32DmaStreamSetMemory0(spip->dmatx, &spip->txsource);
    ch32DmaStreamSetMode(spip->dmatx, spip->txdmamode);
  }
  ch32DmaStreamSetTransactionSize(spip->dmatx, n);

  /* Clearing stale status then starting the channels.*/
  spip->spi->STATR;
  spip->spi->DATAR;
  ch32DmaStreamClearInterrupt(spip->dmarx);
  ch32DmaStreamClearInterrupt(spip->dmatx);

  ch32DmaStreamEnable(spip->dmarx);
  ch32DmaStreamEnable(spip->dmatx);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Ignores n frames on the bus.
 *
 * @notapi
 */
msg_t spi_lld_ignore(SPIDriver *spip, size_t n) {

  return start_transfer(spip, n, NULL, NULL);
}

/**
 * @brief   Exchanges n frames on the bus.
 *
 * @notapi
 */
msg_t spi_lld_exchange(SPIDriver *spip, size_t n,
                       const void *txbuf, void *rxbuf) {

  return start_transfer(spip, n, txbuf, rxbuf);
}

/**
 * @brief   Sends n frames on the bus.
 *
 * @notapi
 */
msg_t spi_lld_send(SPIDriver *spip, size_t n, const void *txbuf) {

  return start_transfer(spip, n, txbuf, NULL);
}

/**
 * @brief   Receives n frames from the bus.
 *
 * @notapi
 */
msg_t spi_lld_receive(SPIDriver *spip, size_t n, void *rxbuf) {

  return start_transfer(spip, n, NULL, rxbuf);
}

/**
 * @brief   Stops the ongoing transfer.
 *
 * @notapi
 */
msg_t spi_lld_stop_transfer(SPIDriver *spip, size_t *sizep) {

  /* Stopping TX DMA.*/
  ch32DmaStreamDisable(spip->dmatx);

  /* Waiting for current frame completion then computing the residual size.*/
  while ((spip->spi->STATR & CH32_SPI_BSY) != 0U) {
  }

  if (sizep != NULL) {
    *sizep = ch32DmaStreamGetTransactionSize(spip->dmarx);
  }

  /* Stopping RX DMA.*/
  ch32DmaStreamDisable(spip->dmarx);

  return HAL_RET_SUCCESS;
}

/**
 * @brief   Exchanges one frame using a polled wait.
 *
 * @notapi
 */
uint16_t spi_lld_polled_exchange(SPIDriver *spip, uint16_t frame) {
  SPI_TypeDef *u = spip->spi;

  while ((u->STATR & CH32_SPI_TXE) == 0U) {
  }
  u->DATAR = frame;
  while ((u->STATR & CH32_SPI_RXNE) == 0U) {
  }

  return u->DATAR;
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

/**
 * @brief   Common error interrupt service for one SPI unit.
 */
static void serve_interrupt(SPIDriver *spip) {
  SPI_TypeDef *u = spip->spi;
  uint16_t sr = u->STATR;

  if ((sr & (CH32_SPI_OVR | CH32_SPI_MODF)) != 0U) {
    /* Drain, stop the DMA channels and report.*/
    (void)u->DATAR;
    u->CTLR2 = 0U;
    ch32DmaStreamDisable(spip->dmatx);
    ch32DmaStreamDisable(spip->dmarx);
    __spi_isr_error_code(spip, HAL_RET_HW_FAILURE);
  }
}

#if (CH32H417_SPI_USE_SPI1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI1 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI1_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_interrupt(&SPID1);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_SPI_USE_SPI2 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI2 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_interrupt(&SPID2);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_SPI_USE_SPI3 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI3 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_interrupt(&SPID3);
  OSAL_IRQ_EPILOGUE();
}
#endif
#if (CH32H417_SPI_USE_SPI4 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI4 interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI4_IRQHandler) {

  OSAL_IRQ_PROLOGUE();
  serve_interrupt(&SPID4);
  OSAL_IRQ_EPILOGUE();
}
#endif

#endif /* HAL_USE_SPI == TRUE */

/** @} */
