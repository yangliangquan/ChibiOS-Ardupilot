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
 * @details Interrupt driven engine (TXE/RXNE), F1-compatible register
 *          model, master mode, 8/16 bit frames, up to four units.
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
#define CH32_SPI_ERRIE                     (1U << 5)
#define CH32_SPI_RXNEIE                    (1U << 6)
#define CH32_SPI_TXEIE                     (1U << 7)

/* STATR bits. */
#define CH32_SPI_RXNE                      (1U << 0)
#define CH32_SPI_TXE                       (1U << 1)
#define CH32_SPI_MODF                      (1U << 5)
#define CH32_SPI_OVR                       (1U << 6)

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
    NVIC_EnableIRQ(SPI1_IRQn);
  }
#if CH32H417_SPI_USE_SPI2
  else if (u == SPI2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);
    NVIC_EnableIRQ(SPI2_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI3
  else if (u == SPI3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI3, ENABLE);
    NVIC_EnableIRQ(SPI3_IRQn);
  }
#endif
#if CH32H417_SPI_USE_SPI4
  else if (u == SPI4) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI4, ENABLE);
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

static void store_rx(SPIDriver *spip, uint16_t data) {

  if (spip->rxbuf != NULL) {
    if (spip->config->databits > 8U) {
      ((uint16_t *)spip->rxbuf)[spip->rxidx] = data;
    }
    else {
      ((uint8_t *)spip->rxbuf)[spip->rxidx] = (uint8_t)data;
    }
  }
  spip->rxidx++;
}

static uint16_t fetch_tx(SPIDriver *spip) {
  uint16_t data = 0xFFFFU;

  if (spip->txbuf != NULL) {
    if (spip->config->databits > 8U) {
      data = ((const uint16_t *)spip->txbuf)[spip->txidx];
    }
    else {
      data = ((const uint8_t *)spip->txbuf)[spip->txidx];
    }
  }
  spip->txidx++;

  return data;
}

/**
 * @brief   Common IRQ service for one SPI unit.
 */
static void serve_interrupt(SPIDriver *spip) {
  SPI_TypeDef *u = spip->spi;
  uint16_t sr = u->STATR;

  if ((sr & (CH32_SPI_OVR | CH32_SPI_MODF)) != 0U) {
    /* Drain and report. */
    (void)u->DATAR;
    u->CTLR2 = 0;
    __spi_isr_error_code(spip, MSG_RESET);
  }
  else {
    if (((sr & CH32_SPI_RXNE) != 0U) && (spip->rxidx < spip->n)) {
      store_rx(spip, u->DATAR);
    }
    if (((sr & CH32_SPI_TXE) != 0U) && (spip->txidx < spip->n)) {
      u->DATAR = fetch_tx(spip);
    }
    if ((spip->rxidx >= spip->n) && (spip->txidx >= spip->n)) {
      u->CTLR2 = 0;
      __spi_isr_complete_code(spip);
    }
  }
}

/**
 * @brief   Starts a transfer engine (ignore/send/receive/exchange).
 */
static msg_t start_transfer(SPIDriver *spip,
                            size_t n,
                            const void *txbuf,
                            void *rxbuf) {

  spip->txbuf = txbuf;
  spip->rxbuf = rxbuf;
  spip->txidx = 0U;
  spip->rxidx = 0U;
  spip->n = n;

  /* Prime the engine: RXNE and TXE interrupts, the first byte is
     pushed by the ISR as soon as TXE is flagged. */
  spip->spi->STATR;
  spip->spi->DATAR;
  spip->spi->CTLR2 = CH32_SPI_ERRIE | CH32_SPI_RXNEIE | CH32_SPI_TXEIE;

  return HAL_RET_SUCCESS;
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level SPI driver initialization.
 *
 * @notapi
 */
msg_t spi_lld_init(void) {

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

  unit_enable(spip->spi);

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
  spip->spi->CTLR2 = 0;
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

  spip->spi->CTLR2 = 0;
  spip->spi->CTLR1 &= (uint16_t)~CH32_SPI_SPE;
  unit_disable(spip->spi);
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

  spip->spi->CTLR2 = 0;

  if (sizep != NULL) {
    *sizep = spip->n - spip->rxidx;
  }

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
