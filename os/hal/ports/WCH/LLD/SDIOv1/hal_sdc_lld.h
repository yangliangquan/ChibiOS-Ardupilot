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
 * @file    SDIOv1/hal_sdc_lld.h
 * @brief   CH32H417 SDC subsystem low level driver header.
 *
 * @details SD card driver on the classic SDIO block (F1-compatible
 *          register set with WCH extras FIFOCNT/DCTRL2/HWFC).
 *          Data path is interrupt-driven FIFO PIO (no DMA), paced by
 *          the hardware flow control (HWFC_EN).
 *
 *          Clock limitation at HCLK=400MHz: SDIO_CK = HCLK/(CLKDIV+2)
 *          with an 8-bit CLKDIV, so the slowest card clock is
 *          ~1.56MHz - the SD spec identification clock of 400kHz is
 *          NOT reachable from a 400MHz HCLK (RM note: init clock
 *          should be <400kHz). Flagged for hardware validation; the
 *          SDMMC backend (Ch.33) provides a proper low-speed mode.
 *
 * @addtogroup SDC
 * @{
 */

#ifndef HAL_SDC_LLD_H
#define HAL_SDC_LLD_H

#if (HAL_USE_SDC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_sdio.h"
#include "ch32h417_sdmmc.h"

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   SDIO backend enable switch.
 */
#if !defined(CH32H417_SDC_USE_SDIO) || defined(__DOXYGEN__)
#define CH32H417_SDC_USE_SDIO               FALSE
#endif

/**
 * @brief   SDMMC (Ch.33) backend enable switch.
 */
#if !defined(CH32H417_SDC_USE_SDMMC) || defined(__DOXYGEN__)
#define CH32H417_SDC_USE_SDMMC              FALSE
#endif

/**
 * @brief   Card kernel clock in Hz (SDIO is fed by the core HCLK).
 */
#if !defined(CH32H417_SDIO_CLOCK) || defined(__DOXYGEN__)
#define CH32H417_SDIO_CLOCK                 CH32H417_HCLK
#endif

/**
 * @brief   SDMMC controller input clock in Hz.
 * @note    The SDMMC block is clocked by SYSPLL_SEL (=PLL_CLK, the
 *          main PLL output) out of reset; set to the actual PLL
 *          frequency (equals HCLK with the default clock tree).
 */
#if !defined(CH32H417_SDMMC_CLOCK) || defined(__DOXYGEN__)
#define CH32H417_SDMMC_CLOCK                CH32H417_HCLK
#endif

/**
 * @brief   SDMMC timeout counter register value (R8_EMMC_TIMEOUT).
 */
#if !defined(CH32H417_SDC_SDMMC_TIMEOUT) || defined(__DOXYGEN__)
#define CH32H417_SDC_SDMMC_TIMEOUT          0x0FU
#endif

/**
 * @brief   Hardware flow control (RXFIFOF/TXFIFOE clock pacing).
 * @note    Recommended for the PIO data path, see RM SDIO CLKCR.HWFC_EN.
 */
#if !defined(CH32H417_SDC_HWFC) || defined(__DOXYGEN__)
#define CH32H417_SDC_HWFC                   TRUE
#endif

/**
 * @brief   Write timeout in milliseconds.
 */
#if !defined(CH32H417_SDC_WRITE_TIMEOUT) || defined(__DOXYGEN__)
#define CH32H417_SDC_WRITE_TIMEOUT          10000
#endif

/**
 * @brief   Read timeout in milliseconds.
 */
#if !defined(CH32H417_SDC_READ_TIMEOUT) || defined(__DOXYGEN__)
#define CH32H417_SDC_READ_TIMEOUT           10000
#endif

/**
 * @brief   Card clock activation delay in milliseconds.
 */
#if !defined(CH32H417_SDC_CLOCK_DELAY) || defined(__DOXYGEN__)
#define CH32H417_SDC_CLOCK_DELAY            10
#endif

/**
 * @brief   Card clock power saving enable.
 */
#if !defined(CH32H417_SDC_PWRSAV) || defined(__DOXYGEN__)
#define CH32H417_SDC_PWRSAV                 TRUE
#endif

/**
 * @brief   Support for unaligned transfers (uses the driver buffer).
 */
#if !defined(CH32H417_SDC_UNALIGNED_SUPPORT) || defined(__DOXYGEN__)
#define CH32H417_SDC_UNALIGNED_SUPPORT      TRUE
#endif

/**
 * @brief   Card insertion test.
 * @note    Override in the board header when a CD pin is wired;
 *          default assumes a permanently seated card.
 */
#if !defined(CH32H417_SDC_IS_CARD_INSERTED) || defined(__DOXYGEN__)
#define CH32H417_SDC_IS_CARD_INSERTED(sdcp) true
#endif

/**
 * @brief   Write protect test.
 */
#if !defined(CH32H417_SDC_IS_WRITE_PROTECTED) || defined(__DOXYGEN__)
#define CH32H417_SDC_IS_WRITE_PROTECTED(sdcp) false
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_SDC_USE_SDIO && !CH32H417_SDC_USE_SDMMC
#error "SDC driver activated but no card controller assigned"
#endif

#if CH32H417_SDC_USE_SDIO && CH32H417_SDC_USE_SDMMC
#error "only one SDC backend can be active"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of card flags.
 */
typedef uint32_t sdcmode_t;

/**
 * @brief   SDC Driver condition flags type.
 */
typedef uint32_t sdcflags_t;

/**
 * @brief   Type of a structure representing an SDC driver.
 */
typedef struct SDCDriver SDCDriver;

/**
 * @brief   Driver configuration structure.
 * @note    Bus width plus an optional clock slowdown (added to the
 *          prescaler register value).
 */
typedef struct {
  /**
   * @brief   Bus width.
   */
  sdcbusmode_t  bus_width;

  /* End of the mandatory fields.*/
  /**
   * @brief   Bus slowdown.
   */
  uint32_t      slowdown;
} SDCConfig;

/**
 * @brief   @p SDCDriver specific methods.
 */
#define _sdc_driver_methods                                                 \
  _mmcsd_block_device_methods

/**
 * @extends MMCSDBlockDeviceVMT
 *
 * @brief   @p SDCDriver virtual methods table.
 */
struct SDCDriverVMT {
  _sdc_driver_methods
};

/**
 * @brief   Structure representing an SDC driver.
 */
struct SDCDriver {
  /**
   * @brief Virtual Methods Table.
   */
  const struct SDCDriverVMT *vmt;
  _mmcsd_block_device_data
  /**
   * @brief Current configuration data.
   */
  const SDCConfig           *config;
  /**
   * @brief Various flags regarding the mounted card.
   */
  sdcmode_t                 cardmode;
  /**
   * @brief Errors flags.
   */
  sdcflags_t                errors;
  /**
   * @brief Card RCA.
   */
  uint32_t                  rca;
  /* End of the mandatory fields.*/
  /**
   * @brief Thread waiting for I/O completion IRQ.
   */
  thread_reference_t        thread;
  /**
   * @brief Pointer to the SDIO registers block.
   */
  SDIO_TypeDef              *sdio;
  /**
   * @brief Pointer to the SDMMC registers block (Ch.33 backend).
   */
  SDMMC_TypeDef             *sdmmc;
  /**
   * @brief PIO transfer cursor (bytes still to move via the FIFO).
   */
  uint32_t                  remain;
  /**
   * @brief PIO read destination.
   */
  uint8_t                   *rxptr;
  /**
   * @brief PIO write source.
   */
  const uint8_t             *txptr;
  /**
   * @brief Buffer for unaligned/special operations.
   * @note    16-byte aligned, the SDMMC internal DMA requires it.
   */
  uint8_t                   buf[MMCSD_BLOCK_SIZE] __attribute__((aligned(16)));
};

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_SDC_USE_SDIO == TRUE) ||                                   \
    (CH32H417_SDC_USE_SDMMC == TRUE) || !defined(__DOXYGEN__)
extern SDCDriver SDCD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void sdc_lld_init(void);
  void sdc_lld_start(SDCDriver *sdcp);
  void sdc_lld_stop(SDCDriver *sdcp);
  void sdc_lld_start_clk(SDCDriver *sdcp);
  void sdc_lld_set_data_clk(SDCDriver *sdcp, sdcbusclk_t clk);
  void sdc_lld_stop_clk(SDCDriver *sdcp);
  void sdc_lld_set_bus_mode(SDCDriver *sdcp, sdcbusmode_t mode);
  void sdc_lld_send_cmd_none(SDCDriver *sdcp, uint8_t cmd, uint32_t arg);
  bool sdc_lld_send_cmd_short(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                              uint32_t *resp);
  bool sdc_lld_send_cmd_short_crc(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                                  uint32_t *resp);
  bool sdc_lld_send_cmd_long_crc(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                                 uint32_t *resp);
  bool sdc_lld_read_special(SDCDriver *sdcp, uint8_t *buf, size_t bytes,
                            uint8_t cmd, uint32_t argument);
  bool sdc_lld_read(SDCDriver *sdcp, uint32_t startblk,
                    uint8_t *buf, uint32_t blocks);
  bool sdc_lld_write(SDCDriver *sdcp, uint32_t startblk,
                     const uint8_t *buf, uint32_t blocks);
  bool sdc_lld_sync(SDCDriver *sdcp);
  bool sdc_lld_is_card_inserted(SDCDriver *sdcp);
  bool sdc_lld_is_write_protected(SDCDriver *sdcp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_SDC */

#endif /* HAL_SDC_LLD_H */

/** @} */
