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
 * @file    I2Sv1/hal_i2s_lld.h
 * @brief   CH32H417 I2S subsystem low level driver header.
 *
 * @details I2S on SPI2/SPI3 (the two units with I2S pins, F1 model:
 *          I2SCFGR/I2SPR/DATAR registers, RM ch23). The data path is
 *          interrupt driven (TXE/RXNE), 16-bit samples per interrupt
 *          (24/32-bit data frames need two DATAR accesses and are not
 *          supported by this implementation).
 *
 *          The SPI2/SPI3 interrupt vector is shared with the SPI
 *          driver: a unit cannot be assigned to both at the same
 *          time (compile error below).
 *
 *          The application supplies I2SPR (clock divider for the
 *          I2SxCLK input); the RM does not document the I2SxCLK
 *          source explicitly - see CH32H417_I2S_CLOCK note.
 *
 * @addtogroup I2S
 * @{
 */

#ifndef HAL_I2S_LLD_H
#define HAL_I2S_LLD_H

#if (HAL_USE_I2S == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Static I2S modes
 * @{
 */
#define CH32_I2S_MODE_SLAVE                0
#define CH32_I2S_MODE_MASTER               1
#define CH32_I2S_MODE_RX                   2
#define CH32_I2S_MODE_TX                   4
#define CH32_I2S_MODE_RXTX                 (CH32_I2S_MODE_RX |              \
                                            CH32_I2S_MODE_TX)
/** @} */

/**
 * @name    Mode checks
 * @{
 */
#define CH32_I2S_IS_MASTER(mode)           ((mode) & CH32_I2S_MODE_MASTER)
#define CH32_I2S_RX_ENABLED(mode)          ((mode) & CH32_I2S_MODE_RX)
#define CH32_I2S_TX_ENABLED(mode)          ((mode) & CH32_I2S_MODE_TX)
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   I2S unit enable switches (the units are SPI2/SPI3).
 */
#if !defined(CH32H417_I2S_USE_SPI2) || defined(__DOXYGEN__)
#define CH32H417_I2S_USE_SPI2               FALSE
#endif
#if !defined(CH32H417_I2S_USE_SPI3) || defined(__DOXYGEN__)
#define CH32H417_I2S_USE_SPI3               FALSE
#endif

/**
 * @brief   I2S2 mode, one of CH32_I2S_MODE_* (RX and TX are mutually
 *          exclusive in this implementation).
 */
#if !defined(CH32H417_I2S_SPI2_MODE) || defined(__DOXYGEN__)
#define CH32H417_I2S_SPI2_MODE              (CH32_I2S_MODE_MASTER |         \
                                             CH32_I2S_MODE_TX)
#endif

/**
 * @brief   I2S3 mode.
 */
#if !defined(CH32H417_I2S_SPI3_MODE) || defined(__DOXYGEN__)
#define CH32H417_I2S_SPI3_MODE              (CH32_I2S_MODE_MASTER |         \
                                             CH32_I2S_MODE_RX)
#endif

/**
 * @brief   I2S kernel clock in Hz, informational for the application
 *          building the I2SPR divider value.
 * @note    The RM does not state the I2SxCLK source explicitly (F1
 *          family uses PLL48); verify on hardware.
 */
#if !defined(CH32H417_I2S_CLOCK) || defined(__DOXYGEN__)
#define CH32H417_I2S_CLOCK                  CH32H417_HCLK
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_I2S_USE_SPI2 && !CH32H417_I2S_USE_SPI3
#error "I2S driver activated but no SPI peripheral assigned"
#endif

/* Note: the SPI2/SPI3 interrupt vector is shared with the SPI driver;
   the mutual exclusion check lives in hal_i2s_lld.c after hal.h so that
   both LLD headers are visible (hal.h includes I2S before SPI). */

#if CH32_I2S_RX_ENABLED(CH32H417_I2S_SPI2_MODE) &&                      \
    CH32_I2S_TX_ENABLED(CH32H417_I2S_SPI2_MODE)
#error "I2S2 RX and TX mode not supported in this driver implementation"
#endif
#if CH32_I2S_RX_ENABLED(CH32H417_I2S_SPI3_MODE) &&                      \
    CH32_I2S_TX_ENABLED(CH32H417_I2S_SPI3_MODE)
#error "I2S3 RX and TX mode not supported in this driver implementation"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Low level fields of the I2S driver structure.
 */
#define i2s_lld_driver_fields                                               \
  /* Pointer to the SPIx registers block.*/                                 \
  SPI_TypeDef               *spi;                                           \
  /* Calculated part of the I2SCFGR register.*/                             \
  uint16_t                  cfg;                                            \
  /* Static unit mode (CH32_I2S_MODE_*).*/                                  \
  uint8_t                   unitmode;                                       \
  /* Exchange cursors (16-bit samples).*/                                   \
  const uint16_t            *txbuf;                                         \
  uint16_t                  *rxbuf;                                         \
  size_t                    txrem;                                          \
  size_t                    rxrem;                                          \
  size_t                    processed;                                      \
  size_t                    half;

/**
 * @brief   Low level fields of the I2S configuration structure.
 * @note    I2SCFGR carries the application bits (I2SSTD/DATLEN/CHLEN...);
 *          I2SMOD/I2SE/I2SCFG are handled by the driver. DATLEN must be
 *          16-bit (other formats need two DATAR accesses, not supported).
 */
#define i2s_lld_config_fields                                               \
  int16_t                   i2scfgr;                                        \
  /* I2SPR register value (MCKOE|ODD|I2SDIV).*/                            \
  int16_t                   i2spr;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_I2S_USE_SPI2 == TRUE) && !defined(__DOXYGEN__)
extern I2SDriver I2SD2;
#endif
#if (CH32H417_I2S_USE_SPI3 == TRUE) && !defined(__DOXYGEN__)
extern I2SDriver I2SD3;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void i2s_lld_init(void);
  void i2s_lld_start(I2SDriver *i2sp);
  void i2s_lld_stop(I2SDriver *i2sp);
  void i2s_lld_start_exchange(I2SDriver *i2sp);
  void i2s_lld_stop_exchange(I2SDriver *i2sp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_I2S */

#endif /* HAL_I2S_LLD_H */

/** @} */
