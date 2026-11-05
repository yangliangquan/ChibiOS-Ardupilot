/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    test_sdc.c
 * @brief   CH32H417 V5F port check: SDIO card test thread.
 *
 * @details Connect, read block 0, then write/read/verify the last block
 *          of the card. The last block is used as scratch area so that
 *          the partition table in block 0 is never modified.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

#include <string.h>

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define SDC_TEST_BLOCK_SIZE 512U
#define SDC_TEST_RETRY_MS 5000U

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const SDCConfig sdc_config = {SDC_MODE_4BIT, 1U};

/* SDIO moves one word per DMA request, buffers must be word aligned. */
static uint8_t sdc_wbuf[SDC_TEST_BLOCK_SIZE] __attribute__((aligned(4)));
static uint8_t sdc_rbuf[SDC_TEST_BLOCK_SIZE] __attribute__((aligned(4)));

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*
 * SDIO pin mapping of the card slot:
 *   CK = PB11, CMD = PB10, D0 = PE8, D1 = PE9, D2 = PB3, D3 = PB4.
 * PB10/PB11/PE8/PE9 use AF8, PB3/PB4 use AF9.
 */
static void sdc_pins_init(void) {

  palSetPadMode(IOPORT2, 11U, PAL_MODE_ALTERNATE(8)); /* SDIO_CK.  */
  palSetPadMode(IOPORT2, 10U, PAL_MODE_ALTERNATE(8)); /* SDIO_CMD. */
  palSetPadMode(IOPORT5, 8U, PAL_MODE_ALTERNATE(8));  /* SDIO_D0.  */
  palSetPadMode(IOPORT5, 9U, PAL_MODE_ALTERNATE(8));  /* SDIO_D1.  */
  palSetPadMode(IOPORT2, 3U, PAL_MODE_ALTERNATE(9));  /* SDIO_D2.  */
  palSetPadMode(IOPORT2, 4U, PAL_MODE_ALTERNATE(9));  /* SDIO_D3.  */
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waSdcTest, 1024);

static THD_FUNCTION(sdc_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  BlockDeviceInfo bdi;
  uint32_t attempt = 0U;
  uint32_t blk;
  unsigned i;
  bool ok;

  (void)arg;

  sdc_pins_init();

  /* The LLD start has no failure path, the result is only for clarity. */
  if (sdcStart(&SDCD1, &sdc_config) != HAL_SUCCESS) {
    chprintf(chp, "SDC: driver start failed\r\n");
  }

  while (true) {
    attempt++;

    chprintf(chp, "SDC: attempt %u, connecting...\r\n", (unsigned)attempt);
    if (!sdcConnect(&SDCD1)) {
      chprintf(chp, "SDC: connect FAILED (errors=0x%08x)\r\n",
               (unsigned)SDCD1.errors);
      chThdSleepMilliseconds(SDC_TEST_RETRY_MS);
      continue;
    }

    if (!sdcGetInfo(&SDCD1, &bdi)) {
      chprintf(chp, "SDC: get info FAILED\r\n");
      (void)sdcDisconnect(&SDCD1);
      chThdSleepMilliseconds(SDC_TEST_RETRY_MS);
      continue;
    }

    chprintf(chp, "SDC: connected, %u blocks x %u bytes (%u MB)\r\n",
             (unsigned)bdi.blk_num, (unsigned)bdi.blk_size,
             (unsigned)(bdi.blk_num / 2048U));

    /* Block 0 read, this also validates the single block read path. */
    if (!sdcRead(&SDCD1, 0U, sdc_rbuf, 1U)) {
      chprintf(chp, "SDC: read block 0 FAILED (errors=0x%08x)\r\n",
               (unsigned)sdcGetAndClearErrors(&SDCD1));
      (void)sdcDisconnect(&SDCD1);
      chThdSleepMilliseconds(SDC_TEST_RETRY_MS);
      continue;
    }

    /* Write/read/verify on the last block, content is overwritten. */
    blk = bdi.blk_num - 1U;
    for (i = 0U; i < SDC_TEST_BLOCK_SIZE; i++) {
      sdc_wbuf[i] = (uint8_t)((i * 7U) ^ (blk & 0xFFU) ^ attempt);
    }

    ok = sdcWrite(&SDCD1, blk, sdc_wbuf, 1U);
    if (ok) {
      ok = sdcRead(&SDCD1, blk, sdc_rbuf, 1U);
    }
    if (ok) {
      ok = (memcmp(sdc_wbuf, sdc_rbuf, SDC_TEST_BLOCK_SIZE) == 0);
    }

    if (ok) {
      chprintf(chp, "SDC: PASS, block %u written and verified\r\n",
               (unsigned)blk);
    } else {
      chprintf(chp, "SDC: FAIL (errors=0x%08x)\r\n",
               (unsigned)sdcGetAndClearErrors(&SDCD1));
    }

    (void)sdcDisconnect(&SDCD1);
    chThdSleepMilliseconds(SDC_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the SDIO card test thread.
 */
void sdc_test_start(void) {

  chThdCreateStatic(waSdcTest, sizeof(waSdcTest), NORMALPRIO, sdc_test_thd,
                    NULL);
}
