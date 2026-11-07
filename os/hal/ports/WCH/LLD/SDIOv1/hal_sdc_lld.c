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
 * @file    SDIOv1/hal_sdc_lld.c
 * @brief   CH32H417 SDC subsystem low level driver source.
 *
 * @details Two backends, selected in mcuconf.h:
 *          - CH32H417_SDC_USE_SDIO: classic SDIO block (F1 compatible).
 *            Its data phase runs on the general purpose DMA through
 *            DMAMUX request 111, gated by DCTRL.DMAEN; the ISR only
 *            reports errors and DATAEND, the FIFO itself is never
 *            touched by software.
 *          - CH32H417_SDC_USE_SDMMC: Ch.33 SD/eMMC controller using its
 *            OWN internal DMA (DMA_BEG1/BLOCK_CFG engine) with
 *            CONTROL.DMAEN. This controller has no DMAMUX request line
 *            (RM table 10-2), so no general purpose channel is ever
 *            allocated for it.
 *
 * @addtogroup SDC
 * @{
 */

#include <string.h>

#include "hal.h"
#include "hal_lld.h"

#if (HAL_USE_SDC == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_sdio.h"
#include "ch32h417_sdmmc.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* SDIO CLKCR bits (RM 32.6.2). */
#define CH32_SDIO_CLKCR_CLKDIV_Msk          0x000000FFUL
#define CH32_SDIO_CLKCR_CLKEN               (1UL << 8)
#define CH32_SDIO_CLKCR_PWRSAV              (1UL << 9)
#define CH32_SDIO_CLKCR_BYPASS              (1UL << 10)
#define CH32_SDIO_CLKCR_WIDBUS_0            (1UL << 11)
#define CH32_SDIO_CLKCR_WIDBUS_1            (1UL << 12)
#define CH32_SDIO_CLKCR_NEGEDGE             (1UL << 13)
#define CH32_SDIO_CLKCR_HWFC_EN             (1UL << 14)

/* SDIO POWER bits (RM 32.6.1). */
#define CH32_SDIO_POWER_PWRCTRL             0x3UL

/* SDIO CMD bits (RM 32.6.4). */
#define CH32_SDIO_CMD_WAITRESP_SHORT        (1UL << 6)
#define CH32_SDIO_CMD_WAITRESP_LONG         (3UL << 6)
#define CH32_SDIO_CMD_CPSMEN                (1UL << 10)

/* SDIO DCTRL bits (F1 layout). */
#define CH32_SDIO_DCTRL_DTEN                (1UL << 0)
#define CH32_SDIO_DCTRL_DTDIR               (1UL << 1)
#define CH32_SDIO_DCTRL_DTMODE              (1UL << 2)
#define CH32_SDIO_DCTRL_DMAEN               (1UL << 3)
#define CH32_SDIO_DCTRL_DBLOCKSIZE(n)       ((uint32_t)(n) << 4)

/* All write-one-to-clear SDIO status flags. */
#define CH32_SDIO_ICR_ALL_FLAGS             0x00C007FFUL

/* Errors part of the SDIO STA register. */
#define CH32_SDIO_STA_ERROR_MASK                                                 \
  (SDIO_FLAG_CCRCFAIL | SDIO_FLAG_DCRCFAIL |                                    \
   SDIO_FLAG_CTIMEOUT | SDIO_FLAG_DTIMEOUT |                                    \
   SDIO_FLAG_TXUNDERR | SDIO_FLAG_RXOVERR |                                     \
   SDIO_FLAG_STBITERR)

/* End-of-transfer sources that release the waiting thread (SDIO). */
#define CH32_SDIO_STA_DONE_MASK               SDIO_FLAG_DATAEND

/* SDMMC CMD_SET fields (RM 33.3.2). */
#define CH32_SDMMC_CMDIDX(n)                  ((uint16_t)((n) & 0x3FU))
#define CH32_SDMMC_RPTY_136                   ((uint16_t)0x0100)
#define CH32_SDMMC_RPTY_48                    ((uint16_t)0x0200)
#define CH32_SDMMC_RPTY_R1B                   ((uint16_t)0x0300)
#define CH32_SDMMC_CHECKCRC                   ((uint16_t)0x0400)

/* SDMMC CONTROL.DMA_EN: enables the internal data engine (vendor Init
   writes it as bit 3). */
#define CH32_SDMMC_DMA_EN                     (1U << 3)

/* SDMMC command-phase error flags. */
#define CH32_SDMMC_CMD_ERRS                                                 \
  (SDMMC_FLAG_RE_TMOUT | SDMMC_FLAG_RECRC_WR | SDMMC_FLAG_REIDX_ER)

/* SDMMC data-phase completion/error flags. */
#define CH32_SDMMC_DATA_DONE                                                \
  (SDMMC_FLAG_TRANDONE | SDMMC_FLAG_DATTMO | SDMMC_FLAG_TRANERR |           \
   SDMMC_FLAG_FIFO_OV)

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_SDC_USE_SDIO == TRUE) ||                                    \
    (CH32H417_SDC_USE_SDMMC == TRUE) || !defined(__DOXYGEN__)
/** @brief SDCD1 driver identifier.*/
SDCDriver SDCD1;
#endif

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   SDC default configuration.
 */
static const SDCConfig sdc_default_cfg = {
  SDC_MODE_4BIT,
  0U
};

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

#if (CH32H417_SDC_USE_SDIO == TRUE) || defined(__DOXYGEN__)

/**
 * @brief   Calculates a CLKCR divider for the specified card frequency.
 * @note    SDIO_CK = CH32H417_SDIO_CLOCK / (CLKDIV + 2), 8-bit CLKDIV,
 *          clamped at 255 (slowest achievable, ~390kHz at HCLK=100MHz).
 */
static uint32_t sdc_lld_clkdiv(SDCDriver *sdcp, uint32_t f) {
  uint32_t div;

#if defined(CH32H417_SDC_MAX_CLOCK)
  if (f > CH32H417_SDC_MAX_CLOCK) {
    f = CH32H417_SDC_MAX_CLOCK;
  }
#endif

  div = sdcp->config->slowdown + ((CH32H417_SDIO_CLOCK + f - 1U) / f) - 2U;
  if (div < 2U) {
    div = 2U;
  }
  if (div > 255U) {
    div = 255U;
  }

  return div;
}

/**
 * @brief   Common CLKCR assembly: divider + clock options.
 */
static uint32_t sdc_lld_clkcr(SDCDriver *sdcp, uint32_t f) {
  uint32_t clkcr = sdc_lld_clkdiv(sdcp, f);

#if CH32H417_SDC_HWFC
  clkcr |= CH32_SDIO_CLKCR_HWFC_EN;
#endif

  return clkcr;
}

/**
 * @brief   Calculates the value to be put in DTIMER for timeout.
 */
static uint32_t sdc_lld_get_timeout(SDCDriver *sdcp, uint32_t ms) {
  uint32_t div, ck;

  div = (sdcp->sdio->CLKCR & CH32_SDIO_CLKCR_CLKDIV_Msk) + 2U;
  ck = CH32H417_SDIO_CLOCK / div;

  return (ck / 1000U) * ms;
}

/**
 * @brief   Arms the DMA channel for one data phase.
 * @note    Word on both sides: every SDIO transfer length is a word
 *          multiple and the FIFO moves one 32-bit word per request.
 */
static void sdc_lld_arm_dma(SDCDriver *sdcp, const void *buf,
                            uint32_t bytes, bool tocard) {

  ch32DmaStreamSetMemory0(sdcp->dma, buf);
  ch32DmaStreamSetTransactionSize(sdcp->dma, bytes / 4U);
  ch32DmaStreamSetMode(sdcp->dma, sdcp->dmamode |
                                   (tocard ? CH32_DMA_CFGR_DIR_M2P :
                                             CH32_DMA_CFGR_DIR_P2M));
  ch32DmaStreamEnable(sdcp->dma);
}

/**
 * @brief   Prepares to handle a byte-count read (special registers).
 *
 * @return              The operation status.
 * @retval HAL_SUCCESS  operation succeeded.
 * @retval HAL_FAILED   operation failed.
 */
static bool sdc_lld_prepare_read_bytes(SDCDriver *sdcp,
                                       uint8_t *buf, uint32_t bytes) {
  osalDbgCheck(bytes < 0x1000000U);
  osalDbgCheck((bytes & 3U) == 0U);

  sdcp->sdio->DTIMER = sdc_lld_get_timeout(sdcp, CH32H417_SDC_READ_TIMEOUT);

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  sdcp->sdio->ICR   = CH32_SDIO_ICR_ALL_FLAGS;
  /* The FIFO half flags stay masked out: the DMA drains the FIFO and
     only errors and DATAEND need to reach the waiting thread.*/
  sdcp->sdio->MASK  = SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT |
                      SDIO_IT_STBITERR | SDIO_IT_RXOVERR |
                      SDIO_IT_DATAEND;
  sdcp->sdio->DLEN  = bytes;

  /* The channel is armed before DTEN so no request is lost.*/
  sdc_lld_arm_dma(sdcp, buf, bytes, false);

  sdcp->sdio->DCTRL = CH32_SDIO_DCTRL_DTDIR |
                      CH32_SDIO_DCTRL_DTMODE |
                      CH32_SDIO_DCTRL_DMAEN |
                      CH32_SDIO_DCTRL_DTEN;

  return HAL_SUCCESS;
}

/**
 * @brief   Wait end of data transaction and performs finalizes.
 */
static bool sdc_lld_wait_transaction_end(SDCDriver *sdcp, uint32_t n,
                                         uint32_t *resp) {

  osalSysLock();

  if (sdcp->sdio->MASK != 0U) {
    osalThreadSuspendS(&sdcp->thread);
  }

  osalSysUnlock();

  if ((sdcp->sdio->STA & SDIO_FLAG_DATAEND) == 0U) {
    ch32DmaStreamDisable(sdcp->dma);
    return HAL_FAILED;
  }

  /* DATAEND can be raised while the tail of the transfer is still
     crossing the FIFO, so the counter has to run out first.*/
  while (sdcp->dma->channel->CNTR > 0U) {
  }
  ch32DmaStreamDisable(sdcp->dma);

  sdcp->sdio->DCTRL = 0U;
  sdcp->sdio->ICR   = CH32_SDIO_ICR_ALL_FLAGS;

  if (n > 1U)
    return sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_STOP_TRANSMISSION, 0,
                                      resp);

  return HAL_SUCCESS;
}

/**
 * @brief   Gets SDC errors from an SDIO STA value.
 */
static void sdc_lld_collect_errors(SDCDriver *sdcp, uint32_t sta) {
  uint32_t errors = SDC_NO_ERROR;

  if (sta & SDIO_FLAG_CCRCFAIL)
    errors |= SDC_CMD_CRC_ERROR;
  if (sta & SDIO_FLAG_DCRCFAIL)
    errors |= SDC_DATA_CRC_ERROR;
  if (sta & SDIO_FLAG_CTIMEOUT)
    errors |= SDC_COMMAND_TIMEOUT;
  if (sta & SDIO_FLAG_DTIMEOUT)
    errors |= SDC_DATA_TIMEOUT;
  if (sta & SDIO_FLAG_TXUNDERR)
    errors |= SDC_TX_UNDERRUN;
  if (sta & SDIO_FLAG_RXOVERR)
    errors |= SDC_RX_OVERRUN;
  if (sta & SDIO_FLAG_STBITERR)
    errors |= SDC_STARTBIT_ERROR;

  sdcp->errors |= errors;
}

/**
 * @brief   Performs clean transaction stopping in case of errors.
 */
static void sdc_lld_error_cleanup(SDCDriver *sdcp,
                                  uint32_t n,
                                  uint32_t *resp) {
  uint32_t sta;

  ch32DmaStreamDisable(sdcp->dma);

  sta                = sdcp->sdio->STA;
  sdcp->sdio->ICR   = sta;
  sdcp->sdio->DCTRL = 0U;
  sdc_lld_collect_errors(sdcp, sta);

  if (n > 1U) {
    sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_STOP_TRANSMISSION, 0, resp);
  }
}

#endif /* CH32H417_SDC_USE_SDIO == TRUE */

/**
 * @brief   Prepares card to handle read transaction (backend neutral).
 */
static bool sdc_lld_prepare_read(SDCDriver *sdcp, uint32_t startblk,
                                 uint32_t n, uint32_t *resp) {

  if (!(sdcp->cardmode & SDC_MODE_HIGH_CAPACITY))
    startblk *= MMCSD_BLOCK_SIZE;

  if (n > 1U) {
    if (sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_READ_MULTIPLE_BLOCK,
                                   startblk, resp) || MMCSD_R1_ERROR(resp[0])) {
      return HAL_FAILED;
    }
  }
  else {
    if (sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_READ_SINGLE_BLOCK,
                                   startblk, resp) || MMCSD_R1_ERROR(resp[0])) {
      return HAL_FAILED;
    }
  }

  return HAL_SUCCESS;
}

/**
 * @brief   Prepares card to handle write transaction (backend neutral).
 */
static bool sdc_lld_prepare_write(SDCDriver *sdcp, uint32_t startblk,
                                  uint32_t n, uint32_t *resp) {

  if (!(sdcp->cardmode & SDC_MODE_HIGH_CAPACITY))
    startblk *= MMCSD_BLOCK_SIZE;

  if (n > 1U) {
    if (sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_WRITE_MULTIPLE_BLOCK,
                                   startblk, resp) || MMCSD_R1_ERROR(resp[0])) {
      return HAL_FAILED;
    }
  }
  else {
    if (sdc_lld_send_cmd_short_crc(sdcp, MMCSD_CMD_WRITE_BLOCK,
                                   startblk, resp) || MMCSD_R1_ERROR(resp[0])) {
      return HAL_FAILED;
    }
  }

  return HAL_SUCCESS;
}

#if (CH32H417_SDC_USE_SDMMC == TRUE) || defined(__DOXYGEN__)

/**
 * @brief   Calculates a CLK_DIV value for the specified card frequency.
 * @note    High mode:  SDCLK = CH32H417_SDMMC_CLOCK / div
 *          Low mode:   SDCLK = CH32H417_SDMMC_CLOCK / div / 64
 *          div is 5-bit (1..31). Low mode reaches the SD spec 400kHz
 *          identification clock from a 400MHz PLL_CLK (div=16).
 */
static uint32_t sdmmc_clkdiv(uint32_t f, bool lowmode) {
  uint32_t div;

  if (lowmode) {
    div = (CH32H417_SDMMC_CLOCK + (64U * f) - 1U) / (64U * f);
  }
  else {
    div = (CH32H417_SDMMC_CLOCK + f - 1U) / f;
  }

  if (div < 1U) {
    div = 1U;
  }
  if (div > 31U) {
    div = 31U;
  }

  return div;
}

/**
 * @brief   Programs the SDMMC card clock (mode, divider, output enable).
 */
static void sdmmc_set_clk(uint32_t f, bool lowmode) {
  uint16_t clk = (uint16_t)((lowmode ? 0U : SDMMC_ClockSpeed_High) |
                            sdmmc_clkdiv(f, lowmode) |
                            SDMMC_CLKOE);

  SDMMC->CLK_DIV = clk;
}

/**
 * @brief   Sends a command and waits for completion.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 * @param[in] cmd       card command index
 * @param[in] arg       command argument
 * @param[in] rpty      response type (0, CH32_SDMMC_RPTY_136/_48/_R1B)
 * @param[in] checkcrc  enable the hardware CRC7 check
 * @param[in] failmask  status bits that turn the command into a failure
 * @param[out] resp0    pointer to the first response word or NULL
 * @return              HAL_SUCCESS / HAL_FAILED.
 */
static bool sdmmc_cmd(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                      uint16_t rpty, bool checkcrc, uint16_t failmask,
                      uint32_t *resp0) {
  uint16_t fg;

  (void)sdcp;

  SDMMC->ARGUMENT = arg;
  SDMMC->CMD_SET  = (uint16_t)(CH32_SDMMC_CMDIDX(cmd) | rpty |
                               (checkcrc ? CH32_SDMMC_CHECKCRC : 0U));

  while (((fg = SDMMC->INT_FG) &
          (SDMMC_FLAG_CMDDONE | failmask)) == 0U) {
  }

  SDMMC->INT_FG = (uint16_t)(fg & (SDMMC_FLAG_CMDDONE | CH32_SDMMC_CMD_ERRS));

  if ((fg & failmask) != 0U) {
    uint32_t errors = SDC_NO_ERROR;

    if ((fg & SDMMC_FLAG_RE_TMOUT) != 0U) {
      errors |= SDC_COMMAND_TIMEOUT;
    }
    if ((fg & SDMMC_FLAG_RECRC_WR) != 0U) {
      errors |= SDC_CMD_CRC_ERROR;
    }
    sdcp->errors |= errors;

    return HAL_FAILED;
  }

  if (resp0 != NULL) {
    *resp0 = SDMMC->RESPONSE0;
  }

  return HAL_SUCCESS;
}

/**
 * @brief   Arms a read transfer: internal DMA + block config, then the
 *          read command is issued by the caller-provided cmd helper.
 */
static void sdmmc_arm_read(SDCDriver *sdcp, uint8_t *buf,
                           uint32_t blocksize, uint32_t blocks) {

  SDMMC->INT_EN  = 0U;
  SDMMC->INT_FG  = (uint16_t)CH32_SDMMC_DATA_DONE;
  SDMMC->DMA_BEG1 = (uint32_t)buf;
  SDMMC->TRAN_MODE = SDMMC_TranDir_Receive;
  SDMMC->INT_EN  = SDMMC_IT_TRANDONE | SDMMC_IT_DATTMO |
                   SDMMC_IT_TRANERR | SDMMC_IT_FIFO_OV;
  /* Writing a non-zero block number arms the engine. */
  SDMMC->BLOCK_CFG = (blocksize << 16) | blocks;

  (void)sdcp;
}

/**
 * @brief   Arms a write transfer (command is issued before this).
 */
static void sdmmc_arm_write(SDCDriver *sdcp, const uint8_t *buf,
                            uint32_t blocksize, uint32_t blocks) {

  SDMMC->INT_EN  = 0U;
  SDMMC->INT_FG  = (uint16_t)CH32_SDMMC_DATA_DONE;
  SDMMC->DMA_BEG1 = (uint32_t)buf;
  SDMMC->TRAN_MODE = SDMMC_TranDir_Send;
  SDMMC->INT_EN  = SDMMC_IT_TRANDONE | SDMMC_IT_DATTMO |
                   SDMMC_IT_TRANERR | SDMMC_IT_FIFO_OV;
  SDMMC->BLOCK_CFG = (blocksize << 16) | blocks;

  (void)sdcp;
}

/**
 * @brief   Waits for the end of the armed data transfer.
 */
static bool sdmmc_wait_data(SDCDriver *sdcp, uint32_t n) {
  uint16_t fg;
  bool failed = false;

  osalSysLock();

  if (SDMMC->INT_EN != 0U) {
    osalThreadSuspendS(&sdcp->thread);
  }

  osalSysUnlock();

  fg         = SDMMC->INT_FG;
  SDMMC->INT_EN = 0U;
  SDMMC->INT_FG = (uint16_t)(fg & ((uint16_t)CH32_SDMMC_DATA_DONE |
                                   SDMMC_FLAG_BKGAP));

  if ((fg & SDMMC_FLAG_TRANDONE) == 0U) {
    uint32_t errors = SDC_NO_ERROR;

    if ((fg & SDMMC_FLAG_DATTMO) != 0U) {
      errors |= SDC_DATA_TIMEOUT;
    }
    if ((fg & SDMMC_FLAG_TRANERR) != 0U) {
      errors |= SDC_DATA_CRC_ERROR;
    }
    if ((fg & SDMMC_FLAG_FIFO_OV) != 0U) {
      errors |= SDC_RX_OVERRUN;
    }
    if (errors == SDC_NO_ERROR) {
      errors |= SDC_DATA_TIMEOUT;
    }
    sdcp->errors |= errors;
    failed = true;
  }

  if (n > 1U) {
    /* Stop transmission waits for the busy line (R1b). */
    if (sdmmc_cmd(sdcp, MMCSD_CMD_STOP_TRANSMISSION, 0U,
                  CH32_SDMMC_RPTY_R1B, true,
                  (uint16_t)(SDMMC_FLAG_RE_TMOUT | SDMMC_FLAG_RECRC_WR),
                  NULL) != HAL_SUCCESS) {
      failed = true;
    }
  }

  return failed ? HAL_FAILED : HAL_SUCCESS;
}

/**
 * @brief   Error cleanup for an armed SDMMC data transfer.
 */
static void sdmmc_error_cleanup(SDCDriver *sdcp, uint32_t n) {
  uint16_t fg = SDMMC->INT_FG;
  uint32_t errors = SDC_NO_ERROR;

  SDMMC->INT_EN  = 0U;
  SDMMC->INT_FG  = (uint16_t)(fg | (uint16_t)CH32_SDMMC_DATA_DONE);

  if ((fg & SDMMC_FLAG_DATTMO) != 0U) {
    errors |= SDC_DATA_TIMEOUT;
  }
  if ((fg & SDMMC_FLAG_TRANERR) != 0U) {
    errors |= SDC_DATA_CRC_ERROR;
  }
  if ((fg & SDMMC_FLAG_FIFO_OV) != 0U) {
    errors |= SDC_RX_OVERRUN;
  }
  sdcp->errors |= errors;

  if (n > 1U) {
    (void)sdmmc_cmd(sdcp, MMCSD_CMD_STOP_TRANSMISSION, 0U,
                    CH32_SDMMC_RPTY_R1B, true,
                    (uint16_t)(SDMMC_FLAG_RE_TMOUT | SDMMC_FLAG_RECRC_WR),
                    NULL);
  }
}

#endif /* CH32H417_SDC_USE_SDMMC == TRUE */

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_SDC_USE_SDIO == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDIO IRQ handler.
 * @details Reports transfer completion and errors and releases the
 *          waiting thread; the data itself is moved by the DMA so the
 *          FIFO is never serviced from here.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SDIO_IRQHandler) {
  uint32_t sta = SDIO->STA;

  OSAL_IRQ_PROLOGUE();

  osalSysLockFromISR();

  if ((sta & (CH32_SDIO_STA_ERROR_MASK | CH32_SDIO_STA_DONE_MASK)) != 0U) {
    SDIO->MASK = 0U;
    osalThreadResumeI(&SDCD1.thread, MSG_OK);
  }

  osalSysUnlockFromISR();

  OSAL_IRQ_EPILOGUE();
}
#endif /* CH32H417_SDC_USE_SDIO == TRUE */

#if (CH32H417_SDC_USE_SDMMC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDMMC IRQ handler.
 * @details Releases the waiting thread on transfer completion or error;
 *          the status flags are left for the polling context.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(SDMMC_IRQHandler) {
  uint16_t fg = SDMMC->INT_FG;

  OSAL_IRQ_PROLOGUE();

  if ((fg & (uint16_t)CH32_SDMMC_DATA_DONE) != 0U) {
    osalSysLockFromISR();
    SDMMC->INT_EN = 0U;
    osalThreadResumeI(&SDCD1.thread, MSG_OK);
    osalSysUnlockFromISR();
  }

  OSAL_IRQ_EPILOGUE();
}
#endif /* CH32H417_SDC_USE_SDMMC == TRUE */

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level SDC driver initialization.
 *
 * @notapi
 */
void sdc_lld_init(void) {

  sdcObjectInit(&SDCD1);
  SDCD1.thread  = NULL;
  SDCD1.dma     = NULL;
  SDCD1.dmamode = 0U;
#if (CH32H417_SDC_USE_SDIO == TRUE)
  SDCD1.sdio    = SDIO;
  SDCD1.sdio->MASK = 0U;
  NVIC_EnableIRQ(SDIO_IRQn);
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  SDCD1.sdio    = NULL;
  SDCD1.sdmmc   = SDMMC;
  NVIC_EnableIRQ(SDMMC_IRQn);
#endif
}

/**
 * @brief   Configures and activates the SDC peripheral.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 *
 * @notapi
 */
void sdc_lld_start(SDCDriver *sdcp) {

  if (sdcp->config == NULL) {
    sdcp->config = &sdc_default_cfg;
  }

#if (CH32H417_SDC_USE_SDIO == TRUE)
  if (sdcp->state == BLK_STOP) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_SDIO, ENABLE);

    /* First activation: dynamically allocated channel routed to the
       SDIO FIFO, the direction is selected per transfer.*/
    sdcp->dma = ch32DmaStreamAllocI(CH32_DMA_STREAM_ID_ANY,
                                    CH32H417_SDC_SDIO_DMA_PRIORITY,
                                    NULL, NULL);
    osalDbgAssert(sdcp->dma != NULL, "unable to allocate stream");
    ch32DmaStreamSetRequest(sdcp->dma, CH32_DMAMUX_SDIO);
    ch32DmaStreamSetPeripheral(sdcp->dma, &sdcp->sdio->FIFO);
    sdcp->dmamode = CH32_DMA_CFGR_PL(CH32H417_SDC_SDIO_DMA_PRIORITY) |
                    CH32_DMA_CFGR_PSIZE_WORD | CH32_DMA_CFGR_MSIZE_WORD |
                    CH32_DMA_CFGR_MINC;
  }

  sdcp->sdio->POWER  = 0U;
  sdcp->sdio->CLKCR  = 0U;
  sdcp->sdio->DCTRL  = 0U;
  sdcp->sdio->DTIMER = 0U;
  sdcp->sdio->MASK   = 0U;
  sdcp->sdio->ICR    = CH32_SDIO_ICR_ALL_FLAGS;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  if (sdcp->state == BLK_STOP) {
    RCC_HBPeriphClockCmd(RCC_HBPeriph_SDMMC, ENABLE);
  }

  SDMMC_InternalLogicReset();
  SDMMC->CONTROL = CH32_SDMMC_DMA_EN;  /* internal DMA engine on, 1 bit */
  SDMMC->TIMEOUT = CH32H417_SDC_SDMMC_TIMEOUT;
  SDMMC->CLK_DIV = 0U;
  SDMMC->INT_EN  = 0U;
  SDMMC->INT_FG  = 0xFFFFU;
  NVIC_EnableIRQ(SDMMC_IRQn);
#endif
}

/**
 * @brief   Deactivates the SDC peripheral.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 *
 * @notapi
 */
void sdc_lld_stop(SDCDriver *sdcp) {

  if (sdcp->state != BLK_STOP) {
#if (CH32H417_SDC_USE_SDIO == TRUE)
    sdcp->sdio->POWER  = 0U;
    sdcp->sdio->CLKCR  = 0U;
    sdcp->sdio->DCTRL  = 0U;
    sdcp->sdio->DTIMER = 0U;
    sdcp->sdio->MASK   = 0U;

    /* Releasing the channel.*/
    if (sdcp->dma != NULL) {
      ch32DmaStreamFreeI(sdcp->dma);
      sdcp->dma = NULL;
    }

    RCC_HB2PeriphClockCmd(RCC_HB2Periph_SDIO, DISABLE);
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
    SDMMC->INT_EN = 0U;
    SDMMC->CLK_DIV &= (uint16_t)~SDMMC_CLKOE;

    RCC_HBPeriphClockCmd(RCC_HBPeriph_SDMMC, DISABLE);
#endif
  }
}

/**
 * @brief   Starts the card clock and sets it to identification mode.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 *
 * @notapi
 */
void sdc_lld_start_clk(SDCDriver *sdcp) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t clkcr = sdc_lld_clkcr(sdcp, 400000U);

  sdcp->sdio->CLKCR  = (clkcr & ~CH32_SDIO_CLKCR_WIDBUS_0 &
                                 ~CH32_SDIO_CLKCR_WIDBUS_1);
  sdcp->sdio->POWER |= CH32_SDIO_POWER_PWRCTRL;
  CH32H417_DSB();
    sdcp->sdio->CLKCR |= CH32_SDIO_CLKCR_CLKEN;

#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  (void)sdcp;
  /* Low speed mode: SDCLK = PLL_CLK / div / 64 (~390kHz at 400MHz). */
  SDMMC->CONTROL = CH32_SDMMC_DMA_EN | SDMMC_BusWidth_1;
  sdmmc_set_clk(400000U, true);
#endif

  osalThreadSleep(OSAL_MS2I(CH32H417_SDC_CLOCK_DELAY));
}

/**
 * @brief   Sets the card clock to data mode (25/50 MHz or less).
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 * @param[in] clk       the clock mode
 *
 * @notapi
 */
void sdc_lld_set_data_clk(SDCDriver *sdcp, sdcbusclk_t clk) {
  uint32_t f = (SDC_CLK_50MHz == clk) ? 50000000U : 25000000U;

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t keep = sdcp->sdio->CLKCR &
                  (CH32_SDIO_CLKCR_WIDBUS_0 | CH32_SDIO_CLKCR_WIDBUS_1 |
                   CH32_SDIO_CLKCR_HWFC_EN | CH32_SDIO_CLKCR_CLKEN |
                   CH32_SDIO_CLKCR_NEGEDGE);
  uint32_t clkcr = sdc_lld_clkcr(sdcp, f);

#if CH32H417_SDC_PWRSAV
  clkcr |= CH32_SDIO_CLKCR_PWRSAV;
#endif

  sdcp->sdio->CLKCR = keep | clkcr;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  (void)sdcp;
  /* High speed mode: SDCLK = PLL_CLK / div (25..200MHz range). */
  sdmmc_set_clk(f, false);
#endif
}

/**
 * @brief   Stops the card clock.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 *
 * @notapi
 */
void sdc_lld_stop_clk(SDCDriver *sdcp) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  sdcp->sdio->CLKCR = 0U;
  sdcp->sdio->POWER = 0U;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  (void)sdcp;
  SDMMC->CLK_DIV &= (uint16_t)~SDMMC_CLKOE;
#endif
}

/**
 * @brief   Switches the bus to 1, 4 or 8 bits mode.
 *
 * @param[in] sdcp      pointer to the @p SDCDriver object
 * @param[in] mode      bus mode
 *
 * @notapi
 */
void sdc_lld_set_bus_mode(SDCDriver *sdcp, sdcbusmode_t mode) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t clk = sdcp->sdio->CLKCR &
                 ~(CH32_SDIO_CLKCR_WIDBUS_0 | CH32_SDIO_CLKCR_WIDBUS_1);

  switch (mode) {
  case SDC_MODE_1BIT:
    sdcp->sdio->CLKCR = clk;
    break;
  case SDC_MODE_4BIT:
    sdcp->sdio->CLKCR = clk | CH32_SDIO_CLKCR_WIDBUS_0;
    break;
  case SDC_MODE_8BIT:
    sdcp->sdio->CLKCR = clk | CH32_SDIO_CLKCR_WIDBUS_1;
    break;
  }
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  uint16_t width;

  (void)sdcp;

  switch (mode) {
  case SDC_MODE_1BIT:
    width = SDMMC_BusWidth_1;
    break;
  case SDC_MODE_4BIT:
    width = SDMMC_BusWidth_4;
    break;
  default:
    width = SDMMC_BusWidth_8;
    break;
  }

  SDMMC->CONTROL = (uint16_t)((SDMMC->CONTROL & ~0x3U) |
                              CH32_SDMMC_DMA_EN | width);
#endif
}

/**
 * @brief   Sends a command with no response expected.
 *
 * @notapi
 */
void sdc_lld_send_cmd_none(SDCDriver *sdcp, uint8_t cmd, uint32_t arg) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  sdcp->sdio->ARG = arg;
  sdcp->sdio->CMD = (uint32_t)cmd | CH32_SDIO_CMD_CPSMEN;
  while ((sdcp->sdio->STA & SDIO_FLAG_CMDSENT) == 0U) {
  }
  sdcp->sdio->ICR = SDIO_FLAG_CMDSENT;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  /* No response expected: never a command failure by itself. */
  (void)sdmmc_cmd(sdcp, cmd, arg, 0U, false, 0U, NULL);
#endif
}

/**
 * @brief   Sends a command with a short response expected.
 * @note    The CRC is not verified.
 *
 * @notapi
 */
bool sdc_lld_send_cmd_short(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                            uint32_t *resp) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t sta;

  sdcp->sdio->ARG = arg;
  sdcp->sdio->CMD = (uint32_t)cmd | CH32_SDIO_CMD_WAITRESP_SHORT |
                    CH32_SDIO_CMD_CPSMEN;
  while (((sta = sdcp->sdio->STA) & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                                     SDIO_FLAG_CCRCFAIL)) == 0U)
    ;
  sdcp->sdio->ICR = sta & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                           SDIO_FLAG_CCRCFAIL);
  if ((sta & SDIO_FLAG_CTIMEOUT) != 0U) {
    sdc_lld_collect_errors(sdcp, sta);
    return HAL_FAILED;
  }
  *resp = sdcp->sdio->RESP1;
  return HAL_SUCCESS;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  return sdmmc_cmd(sdcp, cmd, arg, CH32_SDMMC_RPTY_48, false,
                   SDMMC_FLAG_RE_TMOUT, resp);
#endif
}

/**
 * @brief   Sends a command with a short response expected and CRC.
 *
 * @notapi
 */
bool sdc_lld_send_cmd_short_crc(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                                uint32_t *resp) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t sta;

  sdcp->sdio->ARG = arg;
  sdcp->sdio->CMD = (uint32_t)cmd | CH32_SDIO_CMD_WAITRESP_SHORT |
                    CH32_SDIO_CMD_CPSMEN;
  while (((sta = sdcp->sdio->STA) & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                                     SDIO_FLAG_CCRCFAIL)) == 0U)
    ;
  sdcp->sdio->ICR = sta & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                           SDIO_FLAG_CCRCFAIL);
  if ((sta & (SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)) != 0U) {
    sdc_lld_collect_errors(sdcp, sta);
    return HAL_FAILED;
  }
  *resp = sdcp->sdio->RESP1;
  return HAL_SUCCESS;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  return sdmmc_cmd(sdcp, cmd, arg, CH32_SDMMC_RPTY_48, true,
                   (uint16_t)(SDMMC_FLAG_RE_TMOUT | SDMMC_FLAG_RECRC_WR),
                   resp);
#endif
}

/**
 * @brief   Sends a command with a long response expected and CRC.
 *
 * @notapi
 */
bool sdc_lld_send_cmd_long_crc(SDCDriver *sdcp, uint8_t cmd, uint32_t arg,
                               uint32_t *resp) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
  uint32_t sta;

  sdcp->sdio->ARG = arg;
  sdcp->sdio->CMD = (uint32_t)cmd | CH32_SDIO_CMD_WAITRESP_LONG |
                    CH32_SDIO_CMD_CPSMEN;
  while (((sta = sdcp->sdio->STA) & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                                     SDIO_FLAG_CCRCFAIL)) == 0U)
    ;
  sdcp->sdio->ICR = sta & (SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT |
                           SDIO_FLAG_CCRCFAIL);
  if ((sta & CH32_SDIO_STA_ERROR_MASK) != 0U) {
    sdc_lld_collect_errors(sdcp, sta);
    return HAL_FAILED;
  }
  /* Save bytes in reverse order because MSB in response comes first.*/
  *resp++ = sdcp->sdio->RESP4;
  *resp++ = sdcp->sdio->RESP3;
  *resp++ = sdcp->sdio->RESP2;
  *resp   = sdcp->sdio->RESP1;
  return HAL_SUCCESS;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  if (sdmmc_cmd(sdcp, cmd, arg, CH32_SDMMC_RPTY_136, true,
                (uint16_t)(SDMMC_FLAG_RE_TMOUT | SDMMC_FLAG_RECRC_WR),
                NULL) != HAL_SUCCESS) {
    return HAL_FAILED;
  }
  /* RESPONSE3 holds bits [127:96], the response is stored MSB first.*/
  *resp++ = SDMMC->RESPONSE3;
  *resp++ = SDMMC->RESPONSE2;
  *resp++ = SDMMC->RESPONSE1;
  *resp   = SDMMC->RESPONSE0;
  return HAL_SUCCESS;
#endif
}

#if (CH32H417_SDC_USE_SDIO == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDIO variant of the block read engine (aligned buffer).
 */
static bool sdc_lld_read_aligned(SDCDriver *sdcp, uint32_t startblk,
                                 uint8_t *buf, uint32_t blocks) {
  uint32_t resp[1];
  uint32_t bytes = blocks * MMCSD_BLOCK_SIZE;

  osalDbgCheck(blocks < 0x1000000 / MMCSD_BLOCK_SIZE);

  sdcp->sdio->DTIMER = sdc_lld_get_timeout(sdcp, CH32H417_SDC_READ_TIMEOUT);

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  sdcp->sdio->ICR   = CH32_SDIO_ICR_ALL_FLAGS;
  sdcp->sdio->MASK  = SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT |
                      SDIO_IT_STBITERR | SDIO_IT_RXOVERR |
                      SDIO_IT_DATAEND;
  sdcp->sdio->DLEN  = bytes;

  if (sdc_lld_prepare_read(sdcp, startblk, blocks, resp) == true)
    goto error;

  sdc_lld_arm_dma(sdcp, buf, bytes, false);

  sdcp->sdio->DCTRL = CH32_SDIO_DCTRL_DTDIR |
                      CH32_SDIO_DCTRL_DBLOCKSIZE(9) |
                      CH32_SDIO_DCTRL_DMAEN |
                      CH32_SDIO_DCTRL_DTEN;

  if (sdc_lld_wait_transaction_end(sdcp, blocks, resp) == true)
    goto error;

  return HAL_SUCCESS;

error:
  sdc_lld_error_cleanup(sdcp, blocks, resp);
  return HAL_FAILED;
}
#endif /* CH32H417_SDC_USE_SDIO == TRUE */

#if (CH32H417_SDC_USE_SDMMC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDMMC variant of the block read engine (aligned buffer).
 */
static bool sdc_lld_read_aligned(SDCDriver *sdcp, uint32_t startblk,
                                 uint8_t *buf, uint32_t blocks) {
  uint32_t resp[1];

  osalDbgCheck(blocks < 0x1000000 / MMCSD_BLOCK_SIZE);

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  /* Configuration first, then the command (RM 33.2.3). */
  sdmmc_arm_read(sdcp, buf, MMCSD_BLOCK_SIZE, blocks);

  if (sdc_lld_prepare_read(sdcp, startblk, blocks, resp) == true) {
    sdmmc_error_cleanup(sdcp, blocks);
    return HAL_FAILED;
  }

  if (sdmmc_wait_data(sdcp, blocks) != HAL_SUCCESS) {
    sdmmc_error_cleanup(sdcp, blocks);
    return HAL_FAILED;
  }

  return HAL_SUCCESS;
}
#endif /* CH32H417_SDC_USE_SDMMC == TRUE */

#if (CH32H417_SDC_USE_SDIO == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDIO variant of the block write engine (aligned buffer).
 */
static bool sdc_lld_write_aligned(SDCDriver *sdcp, uint32_t startblk,
                                  const uint8_t *buf, uint32_t blocks) {
  uint32_t resp[1];
  uint32_t bytes = blocks * MMCSD_BLOCK_SIZE;

  osalDbgCheck(blocks < 0x1000000 / MMCSD_BLOCK_SIZE);

  sdcp->sdio->DTIMER = sdc_lld_get_timeout(sdcp, CH32H417_SDC_WRITE_TIMEOUT);

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  sdcp->sdio->ICR   = CH32_SDIO_ICR_ALL_FLAGS;
  sdcp->sdio->MASK  = SDIO_IT_DCRCFAIL | SDIO_IT_DTIMEOUT |
                      SDIO_IT_STBITERR | SDIO_IT_TXUNDERR |
                      SDIO_IT_DATAEND;
  sdcp->sdio->DLEN  = bytes;

  if (sdc_lld_prepare_write(sdcp, startblk, blocks, resp) == true)
    goto error;

  /* Arm the channel only now: RM SDIO requires the command to be
     answered before the data path can start pushing the FIFO.*/
  sdc_lld_arm_dma(sdcp, buf, bytes, true);

  sdcp->sdio->DCTRL = CH32_SDIO_DCTRL_DBLOCKSIZE(9) |
                      CH32_SDIO_DCTRL_DMAEN |
                      CH32_SDIO_DCTRL_DTEN;

  if (sdc_lld_wait_transaction_end(sdcp, blocks, resp) == true)
    goto error;

  return HAL_SUCCESS;

error:
  sdc_lld_error_cleanup(sdcp, blocks, resp);
  return HAL_FAILED;
}
#endif /* CH32H417_SDC_USE_SDIO == TRUE */

#if (CH32H417_SDC_USE_SDMMC == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   SDMMC variant of the block write engine (aligned buffer).
 */
static bool sdc_lld_write_aligned(SDCDriver *sdcp, uint32_t startblk,
                                  const uint8_t *buf, uint32_t blocks) {
  uint32_t resp[1];

  osalDbgCheck(blocks < 0x1000000 / MMCSD_BLOCK_SIZE);

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  /* The command goes first, the DMA address write drives the line
     (RM 33.2.4 note 1). */
  if (sdc_lld_prepare_write(sdcp, startblk, blocks, resp) == true) {
    return HAL_FAILED;
  }

  sdmmc_arm_write(sdcp, buf, MMCSD_BLOCK_SIZE, blocks);

  if (sdmmc_wait_data(sdcp, blocks) != HAL_SUCCESS) {
    sdmmc_error_cleanup(sdcp, blocks);
    return HAL_FAILED;
  }

  return HAL_SUCCESS;
}
#endif /* CH32H417_SDC_USE_SDMMC == TRUE */

/**
 * @brief   Reads special registers using data bus.
 * @details Needed only during card detection procedure.
 *
 * @notapi
 */
bool sdc_lld_read_special(SDCDriver *sdcp, uint8_t *buf, size_t bytes,
                          uint8_t cmd, uint32_t arg) {
  uint32_t resp[1];

#if (CH32H417_SDC_USE_SDIO == TRUE)
  if (sdc_lld_prepare_read_bytes(sdcp, buf, (uint32_t)bytes))
    goto error;

  if (sdc_lld_send_cmd_short_crc(sdcp, cmd, arg, resp)
                                 || MMCSD_R1_ERROR(resp[0]))
    goto error;

  if (sdc_lld_wait_transaction_end(sdcp, 1, resp))
    goto error;

  return HAL_SUCCESS;

error:
  sdc_lld_error_cleanup(sdcp, 1, resp);
  return HAL_FAILED;
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  uint8_t *workbuf = buf;
  bool bounced = false;

  /* The internal DMA requires a 16-byte aligned buffer. */
  if (((uintptr_t)buf & 0xFU) != 0U) {
    osalDbgCheck(bytes <= MMCSD_BLOCK_SIZE);
    workbuf = sdcp->buf;
    bounced = true;
  }

  if (_sdc_wait_for_transfer_state(sdcp))
    return HAL_FAILED;

  sdmmc_arm_read(sdcp, workbuf, (uint32_t)bytes, 1U);

  if (sdc_lld_send_cmd_short_crc(sdcp, cmd, arg, resp)
                                 || MMCSD_R1_ERROR(resp[0])) {
    sdmmc_error_cleanup(sdcp, 1);
    return HAL_FAILED;
  }

  if (sdmmc_wait_data(sdcp, 1) != HAL_SUCCESS) {
    sdmmc_error_cleanup(sdcp, 1);
    return HAL_FAILED;
  }

  if (bounced) {
    memcpy(buf, workbuf, bytes);
  }

  return HAL_SUCCESS;
#endif
}

/**
 * @brief   Reads one or more blocks.
 *
 * @notapi
 */
bool sdc_lld_read(SDCDriver *sdcp, uint32_t startblk,
                  uint8_t *buf, uint32_t blocks) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
#if CH32H417_SDC_UNALIGNED_SUPPORT
  if (((unsigned)buf & 3U) != 0U) {
    uint32_t i;
    for (i = 0U; i < blocks; i++) {
      if (sdc_lld_read_aligned(sdcp, startblk, sdcp->buf, 1)) {
        return HAL_FAILED;
      }
      memcpy(buf, sdcp->buf, MMCSD_BLOCK_SIZE);
      buf += MMCSD_BLOCK_SIZE;
      startblk++;
    }
    return HAL_SUCCESS;
  }
#else
  osalDbgAssert((((unsigned)buf & 3U) == 0U), "unaligned buffer");
#endif
  return sdc_lld_read_aligned(sdcp, startblk, buf, blocks);
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  /* The internal DMA requires 16-byte alignment. */
  if (((uintptr_t)buf & 0xFU) != 0U) {
    uint32_t i;
    for (i = 0U; i < blocks; i++) {
      if (sdc_lld_read_aligned(sdcp, startblk, sdcp->buf, 1)) {
        return HAL_FAILED;
      }
      memcpy(buf, sdcp->buf, MMCSD_BLOCK_SIZE);
      buf += MMCSD_BLOCK_SIZE;
      startblk++;
    }
    return HAL_SUCCESS;
  }
  return sdc_lld_read_aligned(sdcp, startblk, buf, blocks);
#endif
}

/**
 * @brief   Writes one or more blocks.
 *
 * @notapi
 */
bool sdc_lld_write(SDCDriver *sdcp, uint32_t startblk,
                   const uint8_t *buf, uint32_t blocks) {

#if (CH32H417_SDC_USE_SDIO == TRUE)
#if CH32H417_SDC_UNALIGNED_SUPPORT
  if (((unsigned)buf & 3U) != 0U) {
    uint32_t i;
    for (i = 0U; i < blocks; i++) {
      memcpy(sdcp->buf, buf, MMCSD_BLOCK_SIZE);
      buf += MMCSD_BLOCK_SIZE;
      if (sdc_lld_write_aligned(sdcp, startblk, sdcp->buf, 1))
        return HAL_FAILED;
      startblk++;
    }
    return HAL_SUCCESS;
  }
#else
  osalDbgAssert((((unsigned)buf & 3U) == 0U), "unaligned buffer");
#endif
  return sdc_lld_write_aligned(sdcp, startblk, buf, blocks);
#endif
#if (CH32H417_SDC_USE_SDMMC == TRUE)
  if (((uintptr_t)buf & 0xFU) != 0U) {
    uint32_t i;
    for (i = 0U; i < blocks; i++) {
      memcpy(sdcp->buf, buf, MMCSD_BLOCK_SIZE);
      buf += MMCSD_BLOCK_SIZE;
      if (sdc_lld_write_aligned(sdcp, startblk, sdcp->buf, 1))
        return HAL_FAILED;
      startblk++;
    }
    return HAL_SUCCESS;
  }
  return sdc_lld_write_aligned(sdcp, startblk, buf, blocks);
#endif
}

/**
 * @brief   Waits for card idle condition.
 *
 * @api
 */
bool sdc_lld_sync(SDCDriver *sdcp) {

  (void)sdcp;
  return HAL_SUCCESS;
}

/**
 * @brief   Card insertion status.
 *
 * @notapi
 */
bool sdc_lld_is_card_inserted(SDCDriver *sdcp) {

  (void)sdcp;
  return (bool)CH32H417_SDC_IS_CARD_INSERTED(sdcp);
}

/**
 * @brief   Write protect status.
 *
 * @notapi
 */
bool sdc_lld_is_write_protected(SDCDriver *sdcp) {

  (void)sdcp;
  return (bool)CH32H417_SDC_IS_WRITE_PROTECTED(sdcp);
}

#endif /* HAL_USE_SDC == TRUE */

/** @} */
