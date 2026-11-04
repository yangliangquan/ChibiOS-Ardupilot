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
 * @file    QSPIv1/hal_wspi_lld.h
 * @brief   CH32H417 WSPI subsystem low level driver header.
 *
 * @details WSPI on the CH32H417 QSPI blocks (QSPI1/QSPI2), indirect
 *          command/send/receive plus memory-mapped read mode.
 *
 *          The data phase runs on the general purpose DMA, routed
 *          through the DMAMUX request inputs 71/72 (RM table 10-2) and
 *          paced by the unit CR.DMAEN (RM 37.4.1) at the FIFO threshold
 *          level: one dynamically allocated channel per QSPI unit owns
 *          the DR register. The command phase has no data and never
 *          touches the channel. The frame itself is completed from the
 *          QSPI interrupt (TCF), which then waits out and stops the
 *          channel. Contrary to SDMMC (Ch.33) this controller has no
 *          internal DMA.
 *
 *          The transfer option masks (WSPI_CFG_*) are defined here in
 *          the WCH CCR register positions (RM 37.4.6):
 *          IMODE[9:8], ADMODE[11:10], ADSIZE[13:12], ABMODE[15:14],
 *          ABSIZE[17:16], DMODE[25:24], SIOO[28]; dummy cycles are
 *          carried in wspi_command_t::dummy (DCYC[22:18]).
 *          DTR/DQS are not implemented by this IP (SDR only).
 *
 * @addtogroup WSPI
 * @{
 */

#ifndef HAL_WSPI_LLD_H
#define HAL_WSPI_LLD_H

#if (HAL_USE_WSPI == TRUE) || defined(__DOXYGEN__)

#include "hal_lld.h"
#include "ch32h417_qspi.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Memory mapped mode supported.
 */
#define WSPI_SUPPORTS_MEMMAP                TRUE

/**
 * @brief   The LLD provides its own WSPI_CFG_* masks (CCR layout).
 */
#define WSPI_DEFAULT_CFG_MASKS              FALSE

/**
 * @name    Transfer option masks (CH32H417 QSPI CCR positions)
 * @{
 */
#define WSPI_CFG_CMD_MODE_MASK              (3LU << 8LU)
#define WSPI_CFG_CMD_MODE_NONE              (0LU << 8LU)
#define WSPI_CFG_CMD_MODE_ONE_LINE          (1LU << 8LU)
#define WSPI_CFG_CMD_MODE_TWO_LINES         (2LU << 8LU)
#define WSPI_CFG_CMD_MODE_FOUR_LINES        (3LU << 8LU)

#define WSPI_CFG_ADDR_MODE_MASK             (3LU << 10LU)
#define WSPI_CFG_ADDR_MODE_NONE             (0LU << 10LU)
#define WSPI_CFG_ADDR_MODE_ONE_LINE         (1LU << 10LU)
#define WSPI_CFG_ADDR_MODE_TWO_LINES        (2LU << 10LU)
#define WSPI_CFG_ADDR_MODE_FOUR_LINES       (3LU << 10LU)

#define WSPI_CFG_ADDR_SIZE_MASK             (3LU << 12LU)
#define WSPI_CFG_ADDR_SIZE_8                (0LU << 12LU)
#define WSPI_CFG_ADDR_SIZE_16               (1LU << 12LU)
#define WSPI_CFG_ADDR_SIZE_24               (2LU << 12LU)
#define WSPI_CFG_ADDR_SIZE_32               (3LU << 12LU)

#define WSPI_CFG_ALT_MODE_MASK              (3LU << 14LU)
#define WSPI_CFG_ALT_MODE_NONE              (0LU << 14LU)
#define WSPI_CFG_ALT_MODE_ONE_LINE          (1LU << 14LU)
#define WSPI_CFG_ALT_MODE_TWO_LINES         (2LU << 14LU)
#define WSPI_CFG_ALT_MODE_FOUR_LINES        (3LU << 14LU)

#define WSPI_CFG_ALT_SIZE_MASK              (3LU << 16LU)
#define WSPI_CFG_ALT_SIZE_8                 (0LU << 16LU)
#define WSPI_CFG_ALT_SIZE_16                (1LU << 16LU)
#define WSPI_CFG_ALT_SIZE_24                (2LU << 16LU)
#define WSPI_CFG_ALT_SIZE_32                (3LU << 16LU)

#define WSPI_CFG_DATA_MODE_MASK             (3LU << 24LU)
#define WSPI_CFG_DATA_MODE_NONE             (0LU << 24LU)
#define WSPI_CFG_DATA_MODE_ONE_LINE         (1LU << 24LU)
#define WSPI_CFG_DATA_MODE_TWO_LINES        (2LU << 24LU)
#define WSPI_CFG_DATA_MODE_FOUR_LINES       (3LU << 24LU)

#define WSPI_CFG_SIOO                       (1LU << 28LU)
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   QSPIx driver enable switches.
 */
#if !defined(CH32H417_WSPI_USE_QSPI1) || defined(__DOXYGEN__)
#define CH32H417_WSPI_USE_QSPI1             FALSE
#endif
#if !defined(CH32H417_WSPI_USE_QSPI2) || defined(__DOXYGEN__)
#define CH32H417_WSPI_USE_QSPI2             FALSE
#endif

/**
 * @brief   QSPI1 DMA channel priority (0..15), also used as the DMA
 *          channel interrupt priority.
 */
#if !defined(CH32H417_WSPI_QSPI1_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_WSPI_QSPI1_DMA_PRIORITY    8
#endif
/**
 * @brief   QSPI2 DMA channel priority (0..15), also used as the DMA
 *          channel interrupt priority.
 */
#if !defined(CH32H417_WSPI_QSPI2_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_WSPI_QSPI2_DMA_PRIORITY    8
#endif

/**
 * @brief   QSPI kernel clock in Hz (both units sit on HB1 = HCLK).
 */
#if !defined(CH32H417_QSPI_CLOCK) || defined(__DOXYGEN__)
#define CH32H417_QSPI_CLOCK                 ch32h417_hclk
#endif

/**
 * @brief   Memory mapped (XIP) windows of the two units, confirmed
 *          against the board: QSPI1 @ 0x90000000, QSPI2 @ 0x70000000.
 */
#if !defined(CH32H417_QSPI1_MEMMAP_BASE) || defined(__DOXYGEN__)
#define CH32H417_QSPI1_MEMMAP_BASE          0x90000000UL
#endif
#if !defined(CH32H417_QSPI2_MEMMAP_BASE) || defined(__DOXYGEN__)
#define CH32H417_QSPI2_MEMMAP_BASE          0x70000000UL
#endif

/**
 * @brief   FIFO threshold (SR.FTF trigger level, bytes).
 */
#if !defined(CH32H417_QSPI_FTHRES) || defined(__DOXYGEN__)
#define CH32H417_QSPI_FTHRES                16U
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_WSPI_USE_QSPI1 && !CH32H417_WSPI_USE_QSPI2
#error "WSPI driver activated but no QSPI unit assigned"
#endif

#if CH32H417_WSPI_USE_QSPI1 &&                                              \
    !CH32_DMA_IS_VALID_PRIORITY(CH32H417_WSPI_QSPI1_DMA_PRIORITY)
#error "Invalid DMA priority assigned to QSPI1"
#endif

#if CH32H417_WSPI_USE_QSPI2 &&                                              \
    !CH32_DMA_IS_VALID_PRIORITY(CH32H417_WSPI_QSPI2_DMA_PRIORITY)
#error "Invalid DMA priority assigned to QSPI2"
#endif

#if !defined(CH32_DMA_REQUIRED)
#define CH32_DMA_REQUIRED
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Low level driver configuration fields.
 */
#define wspi_lld_config_fields                                             \
  /* Requested SCK frequency in Hz.*/                                      \
  uint32_t                  clock_hz;                                       \
  /* Flash size as (address bits - 1), DCR.FSIZE, 0..31.*/                 \
  uint8_t                   fsize;

/**
 * @brief   Low level driver fields.
 */
#define wspi_lld_driver_fields                                             \
  /* Pointer to the QSPI registers block.*/                                \
  QSPI_TypeDef              *qspi;                                          \
  /* Memory mapped window base of this unit.*/                             \
  uint32_t                  mapbase;                                        \
  /* Dynamically allocated channel owning the DR register.*/               \
  const ch32_dma_stream_t   *dma;                                            \
  /* DMA mode bits, the transfer direction is added per transfer.*/         \
  uint32_t                  dmamode;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_WSPI_USE_QSPI1 == TRUE) && !defined(__DOXYGEN__)
extern WSPIDriver WSPID1;
#endif
#if (CH32H417_WSPI_USE_QSPI2 == TRUE) && !defined(__DOXYGEN__)
extern WSPIDriver WSPID2;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void wspi_lld_init(void);
  void wspi_lld_start(WSPIDriver *wspip);
  void wspi_lld_stop(WSPIDriver *wspip);
  void wspi_lld_command(WSPIDriver *wspip, const wspi_command_t *cmdp);
  void wspi_lld_send(WSPIDriver *wspip, const wspi_command_t *cmdp,
                     size_t n, const uint8_t *txbuf);
  void wspi_lld_receive(WSPIDriver *wspip, const wspi_command_t *cmdp,
                        size_t n, uint8_t *rxbuf);
#if WSPI_SUPPORTS_MEMMAP == TRUE
  void wspi_lld_map_flash(WSPIDriver *wspip,
                          const wspi_command_t *cmdp,
                          uint8_t **addrp);
  void wspi_lld_unmap_flash(WSPIDriver *wspip);
#endif
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_WSPI == TRUE */

#endif /* HAL_WSPI_LLD_H */

/** @} */
