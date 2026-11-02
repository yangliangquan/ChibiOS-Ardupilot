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
 * @file    SPIv2/hal_spi_v2_lld.h
 * @brief   CH32H417 SPI subsystem low level driver header (driver v2).
 *
 * @details Four SPI units (SPI1..SPI4), F1-compatible register model.
 *
 * @addtogroup SPI
 * @{
 */

#ifndef HAL_SPI_V2_LLD_H
#define HAL_SPI_V2_LLD_H

#if (HAL_USE_SPI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Circular mode support flag.
 * @note    Not supported without DMA.
 */
#define SPI_SUPPORTS_CIRCULAR           FALSE

/**
 * @brief   Slave mode support flag.
 * @note    Master only implementation.
 */
#define SPI_SUPPORTS_SLAVE_MODE         FALSE

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   SPIx driver enable switches.
 */
#if !defined(CH32H417_SPI_USE_SPI1) || defined(__DOXYGEN__)
#define CH32H417_SPI_USE_SPI1                  FALSE
#endif
#if !defined(CH32H417_SPI_USE_SPI2) || defined(__DOXYGEN__)
#define CH32H417_SPI_USE_SPI2                  FALSE
#endif
#if !defined(CH32H417_SPI_USE_SPI3) || defined(__DOXYGEN__)
#define CH32H417_SPI_USE_SPI3                  FALSE
#endif
#if !defined(CH32H417_SPI_USE_SPI4) || defined(__DOXYGEN__)
#define CH32H417_SPI_USE_SPI4                  FALSE
#endif

/**
 * @brief   SPI units peripheral clock in Hz.
 * @note    All SPI units sit on HB1/HB2 buses, equal to the core HCLK
 *          with the default clock tree configuration.
 */
#if !defined(CH32H417_SPI_CLOCK) || defined(__DOXYGEN__)
#define CH32H417_SPI_CLOCK                    CH32H417_HCLK
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_SPI_USE_SPI1 && !CH32H417_SPI_USE_SPI2 &&                      \
    !CH32H417_SPI_USE_SPI3 && !CH32H417_SPI_USE_SPI4
#error "SPI driver activated but no SPI peripheral assigned"
#endif

#if SPI_SELECT_MODE == SPI_SELECT_MODE_LLD
#error "SPI_SELECT_MODE_LLD not supported by this driver"
#endif

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the SPI driver structure.
 */
#define spi_lld_driver_fields                                               \
  /* Pointer to the SPI registers block.*/                                  \
  SPI_TypeDef               *spi;                                           \
  /* Transfer buffers.*/                                                    \
  const void                *txbuf;                                         \
  void                      *rxbuf;                                         \
  /* Transfer indices.*/                                                    \
  size_t                    txidx;                                          \
  size_t                    rxidx;                                          \
  /* Transfer length.*/                                                     \
  size_t                    n;

/**
 * @brief   Low level fields of the SPI configuration structure.
 */
#define spi_lld_config_fields                                               \
  /* Requested SCK frequency in Hz.*/                                       \
  uint32_t                  clock_freq;                                      \
  /* Frame mode, 0..3 (CPOL/CPHA combination).*/                            \
  uint8_t                   mode;                                           \
  /* Frame size, only 8 or 16 bits (F1-style DFF).*/                        \
  uint8_t                   databits;                                       \
  /* Least significant bit first.*/                                         \
  bool                      lsb_first;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_SPI_USE_SPI1 == TRUE) && !defined(__DOXYGEN__)
extern SPIDriver SPID1;
#endif
#if (CH32H417_SPI_USE_SPI2 == TRUE) && !defined(__DOXYGEN__)
extern SPIDriver SPID2;
#endif
#if (CH32H417_SPI_USE_SPI3 == TRUE) && !defined(__DOXYGEN__)
extern SPIDriver SPID3;
#endif
#if (CH32H417_SPI_USE_SPI4 == TRUE) && !defined(__DOXYGEN__)
extern SPIDriver SPID4;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  msg_t spi_lld_init(void);
  msg_t spi_lld_start(SPIDriver *spip);
  void spi_lld_stop(SPIDriver *spip);
  msg_t spi_lld_ignore(SPIDriver *spip, size_t n);
  msg_t spi_lld_exchange(SPIDriver *spip, size_t n,
                         const void *txbuf, void *rxbuf);
  msg_t spi_lld_send(SPIDriver *spip, size_t n, const void *txbuf);
  msg_t spi_lld_receive(SPIDriver *spip, size_t n, void *rxbuf);
  msg_t spi_lld_stop_transfer(SPIDriver *spip, size_t *sizep);
  uint16_t spi_lld_polled_exchange(SPIDriver *spip, uint16_t frame);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_SPI == TRUE */

#endif /* HAL_SPI_V2_LLD_H */

/** @} */
