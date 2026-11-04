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
 * @file    main.c
 * @brief   CH32H417 V5F port check: HAL + tick + threads + console.
 *
 * @details Observation points for the combined on-target validation:
 *          - counter1/counter2 : per-thread loop counters (both must
 *            advance, no starvation)
 *          - fcounter1/2       : independent FP accumulators, they diverge
 *            from each other only if the FP context is switched
 *          - systime_snapshot  : system time, written every main loop
 *          - main_loops        : main thread loop count
 *          - dma_selftest_result : boot DMA smoke test, 1 = pass,
 *            2 = channel allocation failure, 3 = data mismatch,
 *            4 = timeout
 *          - LED on PD11       : visible blink (PAL), 500ms period
 *          - SD3 console       : 1s status line (SERIAL)
 *          - sdc_test_thd      : SDIO card read/write test every 5s
 *          - wspi_test_thd     : QSPI2 flash read-only validation
 *
 * @addtogroup RT
 * @{
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "hal.h"

#include <string.h>

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define CONSOLE_SPEED 115200

#define SDC_TEST_BLOCK_SIZE 512U
#define SDC_TEST_RETRY_MS 5000U

#define WSPI_TEST_RETRY_MS 5000U

/* QSPI2 kernel clock, both units sit on HB1 = HCLK (RM 37.4.1). */
#define WSPI_TEST_CLOCK_HZ 20000000U

/* Flash geometry, DCR.FSIZE = address bits - 1, 24 bits = 16MB. */
#define WSPI_TEST_FSIZE 23U

/* Read-only test window in the memory mapped image. */
#define WSPI_TEST_READ_SIZE 64U

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const SerialConfig serial_config = {.speed = CONSOLE_SPEED};

static const SDCConfig sdc_config = {SDC_MODE_4BIT, 1U};

static const WSPIConfig wspi_config = {.end_cb = NULL,
                                       .error_cb = NULL,
                                       .clock_hz = WSPI_TEST_CLOCK_HZ,
                                       .fsize = WSPI_TEST_FSIZE};

/* SDIO moves one word per DMA request, buffers must be word aligned. */
static uint8_t sdc_wbuf[SDC_TEST_BLOCK_SIZE] __attribute__((aligned(4)));
static uint8_t sdc_rbuf[SDC_TEST_BLOCK_SIZE] __attribute__((aligned(4)));

/* QSPI data phase runs on the DMA, buffers are word aligned as well. The
   memory mapped comparison reads the window directly, no second buffer
   is needed. */
static uint8_t wspi_rbuf[WSPI_TEST_READ_SIZE] __attribute__((aligned(4)));

static volatile uint32_t counter1;
static volatile uint32_t counter2;
static volatile float fcounter1;
static volatile float fcounter2;

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t main_loops;
volatile uint32_t systime_snapshot;
volatile uint32_t dma_selftest_result;

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waThread1, 256);
static THD_WORKING_AREA(waThread2, 256);
static THD_WORKING_AREA(waFp1, 256);
static THD_WORKING_AREA(waFp2, 256);
static THD_WORKING_AREA(waConsole, 512);
static THD_WORKING_AREA(waSdcTest, 1024);
static THD_WORKING_AREA(waWspiTest, 1024);

static THD_FUNCTION(thread_fn, arg) {
  volatile uint32_t *cp = (volatile uint32_t *)arg;

  while (true) {
    (*cp)++;
    chThdSleepMilliseconds(1);
  }
}

/*
 * Two independent FP accumulators. If the FP context is not switched
 * correctly each thread will observe the other one's values.
 */
static THD_FUNCTION(fp_thread_fn, arg) {
  volatile float *fp = (volatile float *)arg;

  while (true) {
    *fp = (*fp) * 1.0001f + 1.0f;
    chThdSleepMilliseconds(2);
  }
}

static THD_FUNCTION(fp2_thread_fn, arg) {
  volatile float *fp = (volatile float *)arg;

  while (true) {
    *fp = (*fp) * 0.9999f + 2.0f;
    chThdSleepMilliseconds(3);
  }
}

static THD_FUNCTION(console_thd, arg) {
  BaseSequentialStream *chp = (BaseSequentialStream *)&SD3;

  (void)arg;

  while (true) {
    chprintf(chp, "systime=%u loops=%u c1=%u c2=%u f1=%u f2=%u\r\n",
             (unsigned)chVTGetSystemTime(), (unsigned)main_loops,
             (unsigned)counter1, (unsigned)counter2, (unsigned)fcounter1,
             (unsigned)fcounter2);
    chThdSleepMilliseconds(1000);
  }
}

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

/*
 * Card test: connect, read block 0, then write/read/verify the last
 * block of the card. The last block is used as scratch area so that the
 * partition table in block 0 is never modified.
 */
static THD_FUNCTION(sdc_test_thd, arg) {
  BaseSequentialStream *chp = (BaseSequentialStream *)&SD3;
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

/*
 * QSPI2 pin mapping of the flash, single device on the primary bus
 * (RM table 2-2-25). The second-flash signals SCSXN/SIOX0..3
 * (PC1/PC2/PC3/PB13/PB14) are not wired to anything here.
 *   SCK  = PF6  (AF4)   SIO0 = PF2  (AF5)
 *   SCSN = PF1  (AF5)   SIO1 = PF9  (AF4)
 *   SIO2 = PF10 (AF4)   SIO3 = PE15 (AF7)
 */
static void wspi_pins_init(void) {

  palSetPadMode(IOPORT6, 6U, PAL_MODE_ALTERNATE(4));  /* QSPI2_SCK.  */
  palSetPadMode(IOPORT6, 1U, PAL_MODE_ALTERNATE(5));  /* QSPI2_SCSN. */
  palSetPadMode(IOPORT6, 2U, PAL_MODE_ALTERNATE(5));  /* QSPI2_SIO0. */
  palSetPadMode(IOPORT6, 9U, PAL_MODE_ALTERNATE(4));  /* QSPI2_SIO1. */
  palSetPadMode(IOPORT6, 10U, PAL_MODE_ALTERNATE(4)); /* QSPI2_SIO2. */
  palSetPadMode(IOPORT5, 15U, PAL_MODE_ALTERNATE(7)); /* QSPI2_SIO3. */
}

/*
 * Read-only QSPI2 validation. Every cycle starts the driver, reads the
 * JEDEC identification, then the flash status register, then a block of
 * data through the indirect path, then the same block through the memory
 * mapped window. Nothing on the flash is programmed or erased.
 */
static THD_FUNCTION(wspi_test_thd, arg) {
  BaseSequentialStream *chp = (BaseSequentialStream *)&SD3;
  static const wspi_command_t cmd_rdid = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE |
           WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_ONE_LINE,
    .cmd = 0x9FU,                          /* JEDEC ID.                */
    .addr = 0U,
    .alt = 0U,
    .dummy = 0U
  };
  static const wspi_command_t cmd_rdsr = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE |
           WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_ONE_LINE,
    .cmd = 0x05U,                          /* Read status register.    */
    .addr = 0U,
    .alt = 0U,
    .dummy = 0U
  };
  /*
   * Same frame for the indirect and the memory mapped read so the two
   * results are directly comparable. 0x6B is "fast read quad output":
   * instruction on one line, 24-bit address on one line, 8 dummy cycles,
   * data on four lines. It has no mode byte phase, so it needs no
   * alternate-byte register setup and behaves the same in both modes.
   */
  static const wspi_command_t cmd_quad_read = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
           WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_ALT_MODE_NONE |
           WSPI_CFG_DATA_MODE_FOUR_LINES,
    .cmd = 0x6BU,
    .addr = 0U,
    .alt = 0U,
    .dummy = 8U
  };
  uint8_t id[3];
  uint8_t sr;
  uint8_t *mapaddr;
  uint32_t attempt = 0U;
  bool id_ok;
  bool rd_ok;
  unsigned i;

  (void)arg;

  wspi_pins_init();

  while (true) {
    attempt++;

    /* The LLD start has no failure path, the portable layer only checks
       the resulting state, so the return value is informational here. */
    (void)wspiStart(&WSPID2, &wspi_config);

    chprintf(chp, "WSPI: attempt %u\r\n", (unsigned)attempt);

    /* JEDEC identification, three bytes on a single data line. This also
       proves the command phase and a short DMA data phase work. */
    id_ok = !wspiReceive(&WSPID2, &cmd_rdid, sizeof(id), id);
    if (id_ok) {
      chprintf(chp, "WSPI: id = %02x %02x %02x\r\n", (unsigned)id[0],
               (unsigned)id[1], (unsigned)id[2]);
    }
    else {
      chprintf(chp, "WSPI: read id FAILED\r\n");
    }

    /* Status register, a single byte frame. */
    if (!wspiReceive(&WSPID2, &cmd_rdsr, 1U, &sr)) {
      chprintf(chp, "WSPI: status = %02x\r\n", (unsigned)sr);
    }
    else {
      chprintf(chp, "WSPI: read status FAILED\r\n");
    }

    /* Indirect quad read, the data phase runs on the driver DMA channel. */
    rd_ok = !wspiReceive(&WSPID2, &cmd_quad_read, WSPI_TEST_READ_SIZE,
                         wspi_rbuf);

    /*
     * Only touch the memory mapped window once the identification looks
     * like a real device. Mapping it with nothing on the bus, or with the
     * pins on the wrong alternate function, would leave the window
     * driving a useless read stream.
     */
    if (!id_ok) {
      chprintf(chp, "WSPI: no identification, skipping the mapped read\r\n");
    }
    else if ((id[0] == 0x00U) || (id[0] == 0xFFU)) {
      chprintf(chp, "WSPI: id not valid, skipping the mapped read\r\n");
    }
    else if (!rd_ok) {
      chprintf(chp, "WSPI: indirect read FAILED\r\n");
    }
    else {
      wspiMapFlash(&WSPID2, &cmd_quad_read, &mapaddr);

      if (mapaddr == NULL) {
        chprintf(chp, "WSPI: memmap FAILED\r\n");
      }
      else if (mapaddr[0] == 0xFFU) {
        chprintf(chp, "WSPI: window reads 0xFF, flash absent or erased\r\n");
      }
      else {
        for (i = 0U; i < WSPI_TEST_READ_SIZE; i++) {
          if (mapaddr[i] != wspi_rbuf[i]) {
            break;
          }
        }

        if (i == WSPI_TEST_READ_SIZE) {
          chprintf(chp, "WSPI: PASS, %u bytes match, id=%02x %02x %02x, "
                        "window=0x%08x\r\n", (unsigned)WSPI_TEST_READ_SIZE,
                   (unsigned)id[0], (unsigned)id[1], (unsigned)id[2],
                   (unsigned)mapaddr);
        }
        else {
          chprintf(chp, "WSPI: memmap MISMATCH at %u (%02x vs %02x)\r\n",
                   (unsigned)i, (unsigned)mapaddr[i], (unsigned)wspi_rbuf[i]);
        }
      }

      wspiUnmapFlash(&WSPID2);
    }

    wspiStop(&WSPID2);

    chThdSleepMilliseconds(WSPI_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module main code.                                                         */
/*===========================================================================*/

/**
 * @brief   Boot smoke test of the DMA helper driver.
 * @details A memory to memory copy needs no peripheral request and no pin
 *          configuration, so it validates the whole new DMA path (clock
 *          gating, dynamic channel allocation, mode programming,
 *          completion) on its own. The completion is polled with a bound so
 *          that a DMA problem cannot stall the boot.
 * @note    The channel is taken from the free pool and given back right
 *          away, the drivers started afterwards get a different one.
 */
static void dma_selftest(void) {
  static uint8_t src[64];
  static uint8_t dst[64];
  const ch32_dma_stream_t *dmastp;
  uint32_t guard = 10000000U;
  unsigned i;

  dma_selftest_result = 4U;

  for (i = 0U; i < sizeof(src); i++) {
    src[i] = (uint8_t)(i ^ 0x5AU);
    dst[i] = 0U;
  }

  dmastp = ch32DmaStreamAlloc(CH32_DMA_STREAM_ID_ANY, 8, NULL, NULL);
  if (dmastp == NULL) {
    dma_selftest_result = 2U;
    return;
  }

  ch32DmaStartMemCopy(dmastp,
                      CH32_DMA_CFGR_PSIZE_BYTE | CH32_DMA_CFGR_MSIZE_BYTE, src,
                      dst, sizeof(src));

  while ((dmastp->channel->CNTR > 0U) && (guard > 0U)) {
    guard--;
  }

  ch32DmaStreamDisable(dmastp);
  ch32DmaStreamFree(dmastp);

  if (guard == 0U) {
    dma_selftest_result = 4U;
    return;
  }

  for (i = 0U; i < sizeof(src); i++) {
    if (dst[i] != src[i]) {
      dma_selftest_result = 3U;
      return;
    }
  }

  dma_selftest_result = 1U;
}

int main(void) {

  halInit();
  chSysInit();

  sdStart(&SD3, &serial_config);
  chprintf((BaseSequentialStream *)&SD3, "CH32H417 V5F ChibiOS boot OK\r\n");

  // dma_selftest();
  // chprintf((BaseSequentialStream *)&SD1,
  //          "dma selftest=%u (1=pass)\r\n",
  //          (unsigned)dma_selftest_result);

  chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, thread_fn,
                    (void *)&counter1);
  chThdCreateStatic(waThread2, sizeof(waThread2), NORMALPRIO, thread_fn,
                    (void *)&counter2);
  chThdCreateStatic(waFp1, sizeof(waFp1), NORMALPRIO, fp_thread_fn,
                    (void *)&fcounter1);
  chThdCreateStatic(waFp2, sizeof(waFp2), NORMALPRIO, fp2_thread_fn,
                    (void *)&fcounter2);
  // chThdCreateStatic(waSdcTest, sizeof(waSdcTest), NORMALPRIO, sdc_test_thd,
  //                   NULL);
  chThdCreateStatic(waWspiTest, sizeof(waWspiTest), NORMALPRIO, wspi_test_thd,
                    NULL);
  chThdCreateStatic(waConsole, sizeof(waConsole), NORMALPRIO + 1, console_thd,
                    NULL);
                    

  while (true) {
    LED_TOGGLE();
    // systime_snapshot = chVTGetSystemTime();
    // main_loops++;
    chThdSleepMilliseconds(500);
  }
}

/** @} */
