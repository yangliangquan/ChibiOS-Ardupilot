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
 * @file    test_wspi.c
 * @brief   CH32H417 V5F port check: QSPI2 read speed, DMA vs XIP.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "chthreads.h"
#include "hal_wspi_lld.h"
#include "portcheck.h"

#define WSPI_TEST_RETRY_MS      20000U
#define WSPI_TEST_FSIZE         22U
#define WSPI_TEST_JEDEC_DENSITY 0x17U
#define WSPI_READ_ADDR          0U
#define WSPI_READ_SIZE          32768U
#define WSPI_READ_WORDS         (WSPI_READ_SIZE / 4U)
#define WSPI_READ_HALVES        (WSPI_READ_SIZE / 2U)
#define WSPI_READ_TIMEOUT       chTimeMS2I(10000U)

static const uint32_t wspi_clocks[] = {
  1000000U, 2000000U, 5000000U, 10000000U,
  20000000U, 25000000, 
  50000000U, 100000000U
};

#define WSPI_CLOCK_CASES (sizeof(wspi_clocks) / sizeof(wspi_clocks[0]))

static WSPIConfig wspi_config = {.end_cb = NULL,
                                 .error_cb = NULL,
                                 .clock_hz = 20000000U,
                                 .fsize = WSPI_TEST_FSIZE};

static uint8_t wspi_rbuf[WSPI_READ_SIZE] __attribute__((aligned(4)));

static volatile uint32_t wspi_dma_us;
static volatile uint32_t wspi_xip_word_us;
static volatile uint32_t wspi_xip_half_us;
static volatile uint32_t wspi_xip_byte_us;

static const wspi_command_t cmd_rdid = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE |
         WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_ONE_LINE,
  .cmd = 0x9FU,
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

static const wspi_command_t cmd_quad_read = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_FOUR_LINES |
         WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_ALT_MODE_NONE |
         WSPI_CFG_DATA_MODE_FOUR_LINES,
  .cmd = 0xEBU,
  .addr = WSPI_READ_ADDR,
  .alt = 0U,
  .dummy = 4U
};

static void wspi_pins_init(void) {

  palSetPadMode(IOPORT6, 6U, PAL_MODE_ALTERNATE(4));
  palSetPadMode(IOPORT6, 1U, PAL_MODE_ALTERNATE(5));
  palSetPadMode(IOPORT6, 2U, PAL_MODE_ALTERNATE(5));
  palSetPadMode(IOPORT6, 9U, PAL_MODE_ALTERNATE(4));
  palSetPadMode(IOPORT6, 10U, PAL_MODE_ALTERNATE(4));
  palSetPadMode(IOPORT5, 15U, PAL_MODE_ALTERNATE(7));
}

static uint32_t wspi_us_since(systime_t t0) {

  return (uint32_t)chTimeI2US(chVTTimeElapsedSinceX(t0));
}

static msg_t wspi_receive_bounded(WSPIDriver *wspip,
                                  const wspi_command_t *cmdp,
                                  size_t n, uint8_t *rxbuf,
                                  sysinterval_t timeout) {
  msg_t msg;

  osalSysLock();
  wspiStartReceiveI(wspip, cmdp, n, rxbuf);
  msg = osalThreadSuspendTimeoutS(&wspip->thread, timeout);
  if (msg == MSG_TIMEOUT) {
    wspip->state = WSPI_READY;
  }
  osalSysUnlock();

  return msg;
}

static uint32_t wspi_bench_dma(WSPIDriver *wspip, msg_t *msgp) {
  systime_t t0;

  t0 = chVTGetSystemTimeX();
  *msgp = wspi_receive_bounded(wspip, &cmd_quad_read, WSPI_READ_SIZE,
                               wspi_rbuf, WSPI_READ_TIMEOUT);
  if (*msgp != MSG_OK) {
    return 0U;
  }

  return wspi_us_since(t0);
}

static uint32_t wspi_bench_xip_words(const uint8_t *mapaddr) {
  volatile const uint32_t *wp = (volatile const uint32_t *)mapaddr;
  systime_t t0;
  uint32_t i;

  t0 = chVTGetSystemTimeX();
  for (i = 0U; i < WSPI_READ_WORDS; i++) {
    (void)wp[i];
  }

  return wspi_us_since(t0);
}

static uint32_t wspi_bench_xip_halves(const uint8_t *mapaddr) {
  volatile const uint16_t *hp = (volatile const uint16_t *)mapaddr;
  systime_t t0;
  uint32_t i;

  t0 = chVTGetSystemTimeX();
  for (i = 0U; i < WSPI_READ_HALVES; i++) {
    (void)hp[i];
  }

  return wspi_us_since(t0);
}

static uint32_t wspi_bench_xip_bytes(const uint8_t *mapaddr) {
  volatile const uint8_t *bp = mapaddr;
  systime_t t0;
  uint32_t i;

  t0 = chVTGetSystemTimeX();
  for (i = 0U; i < WSPI_READ_SIZE; i++) {
    (void)bp[i];
  }

  return wspi_us_since(t0);
}

static uint32_t wspi_rate_x10(uint32_t us) {

  return (us > 0U) ?
         (uint32_t)(((uint64_t)WSPI_READ_SIZE * 10000ULL) /
                    (uint64_t)us) : 0U;
}

static void wspi_report(BaseSequentialStream *chp, const char *tag,
                        uint32_t us, bool ok) {
  uint32_t r;

  if (!ok) {
    chprintf(chp, "%s TIMEOUT, ", tag);
  }
  else {
    r = wspi_rate_x10(us);
    chprintf(chp, "%s %u.%u, ", tag, (unsigned)(r / 10U),
             (unsigned)(r % 10U));
  }
}

static uint32_t wspi_sck_hz(uint32_t clock_hz) {
  uint32_t presc = (CH32H417_QSPI_CLOCK + clock_hz - 1U) / clock_hz;

  if (presc > 0U) {
    presc--;
  }
  if (presc > 255U) {
    presc = 255U;
  }

  return CH32H417_QSPI_CLOCK / (presc + 1U);
}

static THD_WORKING_AREA(waWspiTest, 1024);

static THD_FUNCTION(wspi_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint8_t id[3];
  uint32_t attempt = 0U;
  uint32_t i;

  (void)arg;
  wspi_pins_init();

  while (true) {
    attempt++;
    wspi_config.clock_hz = wspi_clocks[0];
    (void)wspiStart(&WSPID2, &wspi_config);

    (void)wspiReceive(&WSPID2, &cmd_rdid, sizeof(id), id);
    if ((id[0] != 0x00U) && (id[0] != 0xFFU)) {
      chprintf(chp, "WSPI: attempt %u, id %02x %02x %02x, %u points\r\n",
               (unsigned)attempt, (unsigned)id[0], (unsigned)id[1],
               (unsigned)id[2], (unsigned)WSPI_CLOCK_CASES);
      if (id[2] != WSPI_TEST_JEDEC_DENSITY) {
        chprintf(chp, "WSPI: warning, density %02x, expected %02x (8MB)\r\n",
                 (unsigned)id[2], (unsigned)WSPI_TEST_JEDEC_DENSITY);
      }

      for (i = 0U; i < WSPI_CLOCK_CASES; i++) {
        uint32_t dma_us;
        uint32_t word_us;
        uint32_t half_us;
        uint32_t byte_us;
        msg_t msg;
        uint8_t *mapaddr;

        if (i > 0U) {
          wspiStop(&WSPID2);
          wspi_config.clock_hz = wspi_clocks[i];
          (void)wspiStart(&WSPID2, &wspi_config);
        }

        dma_us = wspi_bench_dma(&WSPID2, &msg);
        wspi_dma_us = dma_us;
        if (msg != MSG_OK) {
          wspiStop(&WSPID2);
          (void)wspiStart(&WSPID2, &wspi_config);
        }

        chprintf(chp, "WSPI: sck %8u, ",
                 (unsigned)wspi_sck_hz(wspi_config.clock_hz));
        wspi_report(chp, "dma", dma_us, msg == MSG_OK);

        wspiMapFlash(&WSPID2, &cmd_quad_read, &mapaddr);
        if (mapaddr == NULL) {
          chprintf(chp, "xip32 FAILED, xip16 FAILED, xip8 FAILED\r\n");
          wspiUnmapFlash(&WSPID2);
          continue;
        }

        word_us = wspi_bench_xip_words(mapaddr);
        half_us = wspi_bench_xip_halves(mapaddr);
        byte_us = wspi_bench_xip_bytes(mapaddr);
        wspi_xip_word_us = word_us;
        wspi_xip_half_us = half_us;
        wspi_xip_byte_us = byte_us;
        wspiUnmapFlash(&WSPID2);

        wspi_report(chp, "xip32", word_us, true);
        wspi_report(chp, "xip16", half_us, true);
        wspi_report(chp, "xip8", byte_us, true);
        chprintf(chp, "\r\n");
      }
      chThdSleepMilliseconds(2000);
    }
    else {
      chprintf(chp, "WSPI: attempt %u, no flash, skipping\r\n",
               (unsigned)attempt);
    }

    wspiStop(&WSPID2);
    chThdSleepMilliseconds(WSPI_TEST_RETRY_MS);
  }
}

void wspi_test_start(void) {

  chThdCreateStatic(waWspiTest, sizeof(waWspiTest), NORMALPRIO, wspi_test_thd,
                    NULL);
}
