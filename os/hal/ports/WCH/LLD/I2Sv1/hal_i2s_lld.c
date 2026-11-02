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
 * @file    I2Sv1/hal_i2s_lld.c
 * @brief   CH32H417 I2S subsystem low level driver source.
 *
 * @details Interrupt driven (TXE/RXNE) half-word data path on SPI2,
 *          SPI3; the I2SE bit starts the bit clock at start, the
 *          peripheral IRQ vector is shared with the SPI driver (the
 *          unit assignment is mutually exclusive, see the header).
 *
 * @addtogroup I2S
 * @{
 */

#include "hal.h"

#if (HAL_USE_I2S == TRUE) || defined(__DOXYGEN__)

/* The SPI2/SPI3 interrupt vector is shared with the SPI driver: with
   hal.h fully processed both LLD switches are visible here. */
#if CH32H417_I2S_USE_SPI2 && defined(CH32H417_SPI_USE_SPI2) &&              \
    CH32H417_SPI_USE_SPI2
#error "SPI2 is used by both the SPI and the I2S driver (shared vector)"
#endif
#if CH32H417_I2S_USE_SPI3 && defined(CH32H417_SPI_USE_SPI3) &&              \
    CH32H417_SPI_USE_SPI3
#error "SPI3 is used by both the SPI and the I2S driver (shared vector)"
#endif

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* STATR flags (F1 layout, RM ch23). */
#define CH32_SPI_SR_RXNE                    (1U << 0)
#define CH32_SPI_SR_TXE                     (1U << 1)
#define CH32_SPI_SR_UDR                     (1U << 3)
#define CH32_SPI_SR_OVR                     (1U << 6)

/* CTLR2 interrupt enables. */
#define CH32_SPI_CR2_RXNEIE                 (1U << 6)
#define CH32_SPI_CR2_TXEIE                  (1U << 7)

/* I2SCFGR bits. */
#define CH32_I2SCFGR_I2SCFG_SHIFT           8U
#define CH32_I2SCFGR_I2SCFG_MASK            (3U << 8)
#define CH32_I2SCFGR_I2SE                   (1U << 10)
#define CH32_I2SCFGR_I2SMOD                 (1U << 11)
#define CH32_I2SCFGR_DATLEN_MASK            (3U << 2)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_I2S_USE_SPI2 == TRUE) && !defined(__DOXYGEN__)
/** @brief I2SD2 driver identifier (SPI2 unit).*/
I2SDriver I2SD2 = {
  .spi = SPI2,
  .unitmode = CH32H417_I2S_SPI2_MODE
};
#endif
#if (CH32H417_I2S_USE_SPI3 == TRUE) && !defined(__DOXYGEN__)
/** @brief I2SD3 driver identifier (SPI3 unit).*/
I2SDriver I2SD3 = {
  .spi = SPI3,
  .unitmode = CH32H417_I2S_SPI3_MODE
};
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Unit mode/I2SCFG field for a driver.
 */
static uint16_t unit_i2scfg(I2SDriver *i2sp) {
  uint16_t ic;

  /* F1 I2SCFG encoding: 00 slave TX, 01 slave RX,
     10 master TX, 11 master RX. */
  ic = (uint16_t)(CH32_I2S_IS_MASTER(i2sp->unitmode) ? (2U << 8) : 0U);
  if (CH32_I2S_RX_ENABLED(i2sp->unitmode)) {
    ic |= (uint16_t)(1U << 8);
  }

  return ic;
}

/**
 * @brief   Exchange progress: half and full buffer events.
 */
static inline void exchange_progress(I2SDriver *i2sp) {

  i2sp->processed++;

  if ((i2sp->half > 0U) && (i2sp->processed == i2sp->half)) {
    /* Portable callback macro defined in the high level driver. */
    _i2s_isr_half_code(i2sp);
  }
  if (i2sp->processed >= i2sp->config->size) {
    i2sp->spi->CTLR2 = 0U;
    /* Portable completion macro defined in the high level driver. */
    _i2s_isr_full_code(i2sp);
  }
}

/**
 * @brief   Common IRQ service for one I2S unit.
 */
static void serve_interrupt(I2SDriver *i2sp) {
  SPI_TypeDef *spi = i2sp->spi;
  uint32_t sr = spi->STATR;
  uint32_t cr2 = spi->CTLR2;

  if (((sr & CH32_SPI_SR_RXNE) != 0U) &&
      ((cr2 & CH32_SPI_CR2_RXNEIE) != 0U)) {
    uint16_t sample = (uint16_t)spi->DATAR;

    if (i2sp->rxrem > 0U) {
      *i2sp->rxbuf++ = sample;
      i2sp->rxrem--;
      exchange_progress(i2sp);
    }
  }

  if (((sr & CH32_SPI_SR_TXE) != 0U) &&
      ((cr2 & CH32_SPI_CR2_TXEIE) != 0U)) {
    if (i2sp->txrem > 0U) {
      spi->DATAR = *i2sp->txbuf++;
      i2sp->txrem--;
      exchange_progress(i2sp);
    }
  }
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_I2S_USE_SPI2 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI2 interrupt handler (I2S mode).
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI2_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&I2SD2);

  OSAL_IRQ_EPILOGUE();
}
#endif

#if (CH32H417_I2S_USE_SPI3 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SPI3 interrupt handler (I2S mode).
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SPI3_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  serve_interrupt(&I2SD3);

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level I2S driver initialization.
 *
 * @notapi
 */
void i2s_lld_init(void) {

#if (CH32H417_I2S_USE_SPI2 == TRUE)
  i2sObjectInit(&I2SD2);
  NVIC_EnableIRQ(SPI2_IRQn);
#endif
#if (CH32H417_I2S_USE_SPI3 == TRUE)
  i2sObjectInit(&I2SD3);
  NVIC_EnableIRQ(SPI3_IRQn);
#endif
}

/**
 * @brief   Configures and activates the I2S peripheral.
 *
 * @notapi
 */
void i2s_lld_start(I2SDriver *i2sp) {
  SPI_TypeDef *spi = i2sp->spi;

  /* The unit must provide exactly one direction. */
  osalDbgAssert((CH32_I2S_RX_ENABLED(i2sp->unitmode) ||
                 CH32_I2S_TX_ENABLED(i2sp->unitmode)),
                "unit without direction");

#if (CH32H417_I2S_USE_SPI2 == TRUE)
  if (i2sp == &I2SD2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, ENABLE);
  }
#endif
#if (CH32H417_I2S_USE_SPI3 == TRUE)
  if (i2sp == &I2SD3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI3, ENABLE);
  }
#endif

  /* I2S mode setup: SPI disabled, driver-owned I2SCFGR bits added. */
  spi->CTLR1 = 0U;
  spi->CTLR2 = 0U;
  i2sp->cfg = (uint16_t)(i2sp->config->i2scfgr &
                         ~(CH32_I2SCFGR_I2SCFG_MASK |
                           CH32_I2SCFGR_I2SE |
                           CH32_I2SCFGR_I2SMOD));
  spi->I2SPR = (uint16_t)i2sp->config->i2spr;
  spi->I2SCFGR = i2sp->cfg | CH32_I2SCFGR_I2SMOD | unit_i2scfg(i2sp);

  /* Clear stale flags (SR then DR read). */
  (void)spi->STATR;
  (void)spi->DATAR;

  /* Start the bit clock (I2SE). Data exchange interrupts are armed
     separately by i2s_lld_start_exchange(). */
  spi->I2SCFGR |= CH32_I2SCFGR_I2SE;
}

/**
 * @brief   Deactivates the I2S peripheral.
 *
 * @note    The RM describes elaborate drain sequences before I2SE can
 *          be cleared; this implementation stops immediately which may
 *          truncate the tail of a frame (see porting notes).
 *
 * @notapi
 */
void i2s_lld_stop(I2SDriver *i2sp) {
  SPI_TypeDef *spi = i2sp->spi;

  spi->CTLR2 = 0U;
  spi->I2SCFGR &= (uint16_t)~CH32_I2SCFGR_I2SE;

#if (CH32H417_I2S_USE_SPI2 == TRUE)
  if (i2sp == &I2SD2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI2, DISABLE);
  }
#endif
#if (CH32H417_I2S_USE_SPI3 == TRUE)
  if (i2sp == &I2SD3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_SPI3, DISABLE);
  }
#endif
}

/**
 * @brief   Starts a buffer exchange.
 *
 * @notapi
 */
void i2s_lld_start_exchange(I2SDriver *i2sp) {
  SPI_TypeDef *spi = i2sp->spi;
  const I2SConfig *cfg = i2sp->config;

  /* Only 16-bit data frames are supported (one DATAR access per
     sample), 24/32-bit formats need double accesses. */
  osalDbgAssert((cfg->i2scfgr & CH32_I2SCFGR_DATLEN_MASK) == 0,
                "only 16-bit data supported");

  i2sp->processed = 0U;
  i2sp->half = cfg->size / 2U;
  i2sp->txbuf = (const uint16_t *)cfg->tx_buffer;
  i2sp->rxbuf = (uint16_t *)cfg->rx_buffer;
  i2sp->txrem = (cfg->tx_buffer != NULL) ? cfg->size : 0U;
  i2sp->rxrem = (cfg->rx_buffer != NULL) ? cfg->size : 0U;

  /* Clear stale flags (SR then DR read) before enabling. */
  (void)spi->STATR;
  (void)spi->DATAR;

  if (CH32_I2S_TX_ENABLED(i2sp->unitmode)) {
    osalDbgAssert(cfg->tx_buffer != NULL, "TX buffer required");
    spi->CTLR2 = CH32_SPI_CR2_TXEIE;
  }
  else {
    osalDbgAssert(cfg->rx_buffer != NULL, "RX buffer required");
    spi->CTLR2 = CH32_SPI_CR2_RXNEIE;
  }
}

/**
 * @brief   Stops an ongoing buffer exchange.
 *
 * @notapi
 */
void i2s_lld_stop_exchange(I2SDriver *i2sp) {

  i2sp->spi->CTLR2 = 0U;
}

#endif /* HAL_USE_I2S == TRUE */

/** @} */
