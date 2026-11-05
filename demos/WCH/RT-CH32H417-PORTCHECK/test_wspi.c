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
 * @brief   CH32H417 V5F port check: QSPI2 flash test thread.
 *
 * @details Every cycle starts the driver, reads the JEDEC identification
 *          and the status register, then proves the read path with an
 *          indirect quad read compared against the memory mapped window.
 *          Once the device is recognized the write path is validated too:
 *          the last 4KB scratch sector is erased, one pseudo random page
 *          is programmed into it, and the page is read back through the
 *          indirect path and verified. A failed cycle is reattempted with
 *          fresh content on the next round. Nothing else on the flash is
 *          touched.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define WSPI_TEST_RETRY_MS 5000U

/* QSPI2 kernel clock, both units sit on HB1 = HCLK (RM 37.4.1). */
#define WSPI_TEST_CLOCK_HZ 20000000U

/* Flash geometry, DCR.FSIZE = address bits - 1, 24 bits = 16MB. */
#define WSPI_TEST_FSIZE 23U

/* Scratch geometry: the last 4KB sector is erased and reprogrammed on
   every attempt, nothing else on the flash is touched. 16MB = 0x1000000,
   last sector at 0x1000000 - 0x1000. */
#define WSPI_TEST_PAGE_SIZE 256U
#define WSPI_TEST_SCRATCH_SECTOR 0xFFF000UL

/* Bound of the WIP (write in progress) polling loop, in milliseconds. */
#define WSPI_TEST_WIP_TIMEOUT_MS 2000U

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const WSPIConfig wspi_config = {.end_cb = NULL,
                                       .error_cb = NULL,
                                       .clock_hz = WSPI_TEST_CLOCK_HZ,
                                       .fsize = WSPI_TEST_FSIZE};

/* QSPI data phase runs on the DMA, buffers are word aligned as well. One
   buffer carries the generated page content, the other one the read back
   data phase. The memory mapped comparison reads the window directly. */
static uint8_t wspi_wbuf[WSPI_TEST_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t wspi_rbuf[WSPI_TEST_PAGE_SIZE] __attribute__((aligned(4)));

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

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
 * Polls the write-in-progress bit of the flash after an erase or a
 * program operation, with a bounded wait. Returns false when the busy
 * bit never clears within WSPI_TEST_WIP_TIMEOUT_MS or the status reads
 * keep failing.
 */
static bool wspi_wait_wip(WSPIDriver *wspip, const wspi_command_t *cmdp) {
  systime_t start = chVTGetSystemTime();
  uint8_t sr;

  while (true) {
    if (!wspiReceive(wspip, cmdp, 1U, &sr) && ((sr & 1U) == 0U)) {
      return true;
    }
    if (chTimeI2MS(chVTTimeElapsedSinceX(start)) >
        (time_msecs_t)WSPI_TEST_WIP_TIMEOUT_MS) {
      return false;
    }
    chThdSleepMilliseconds(2U);
  }
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waWspiTest, 1024);

static THD_FUNCTION(wspi_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
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
   * Write enable, command only, no address and no data. It must precede
   * both the erase and the program operations and has to be sent again
   * between them because the segment clears WEL once it consumes it.
   */
  static const wspi_command_t cmd_wren = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE,
    .cmd = 0x06U,                          /* Write enable.            */
    .addr = 0U,
    .alt = 0U,
    .dummy = 0U
  };
  /*
   * The scratch operations all target the sector base address: a 4KB
   * erase and a single 256 byte page programmed right at its start, so
   * the erase and the page can share the same descriptor address.
   */
  static const wspi_command_t cmd_serase = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
           WSPI_CFG_ADDR_SIZE_24,
    .cmd = 0x20U,                          /* 4KB sector erase.        */
    .addr = WSPI_TEST_SCRATCH_SECTOR,
    .alt = 0U,
    .dummy = 0U
  };
  static const wspi_command_t cmd_ppage = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
           WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_DATA_MODE_ONE_LINE,
    .cmd = 0x02U,                          /* Page program.            */
    .addr = WSPI_TEST_SCRATCH_SECTOR,
    .alt = 0U,
    .dummy = 0U
  };
  /*
   * The final verification uses the plain standard read (0x03), all
   * phases on one line and no dummy cycles, so it exercises a data
   * pattern different from the quad path it is compared against.
   */
  static const wspi_command_t cmd_pread = {
    .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
           WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_DATA_MODE_ONE_LINE,
    .cmd = 0x03U,                          /* Standard read.           */
    .addr = WSPI_TEST_SCRATCH_SECTOR,
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
  uint32_t seed;
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
    rd_ok = !wspiReceive(&WSPID2, &cmd_quad_read, WSPI_TEST_PAGE_SIZE,
                         wspi_rbuf);

    /*
     * Only touch the flash once the identification looks like a real
     * device. Mapping it with nothing on the bus, or with the pins on
     * the wrong alternate function, would leave the window driving a
     * useless read stream.
     */
    if (!id_ok) {
      chprintf(chp, "WSPI: no identification, skipping the flash tests\r\n");
    }
    else if ((id[0] == 0x00U) || (id[0] == 0xFFU)) {
      chprintf(chp, "WSPI: id not valid, skipping the flash tests\r\n");
    }
    else if (!rd_ok) {
      chprintf(chp, "WSPI: indirect quad read FAILED, skipping the flash "
                    "tests\r\n");
    }
    else {
      /* Memory mapped read validation against the indirect window. */
      wspiMapFlash(&WSPID2, &cmd_quad_read, &mapaddr);

      if (mapaddr == NULL) {
        chprintf(chp, "WSPI: memmap FAILED\r\n");
      }
      else if (mapaddr[0] == 0xFFU) {
        chprintf(chp, "WSPI: window reads 0xFF, flash absent or erased\r\n");
      }
      else {
        for (i = 0U; i < WSPI_TEST_PAGE_SIZE; i++) {
          if (mapaddr[i] != wspi_rbuf[i]) {
            break;
          }
        }

        if (i == WSPI_TEST_PAGE_SIZE) {
          chprintf(chp, "WSPI: memmap PASS, %u bytes match, id=%02x %02x "
                        "%02x\r\n", (unsigned)WSPI_TEST_PAGE_SIZE,
                   (unsigned)id[0], (unsigned)id[1], (unsigned)id[2]);
        }
        else {
          chprintf(chp, "WSPI: memmap MISMATCH at %u (%02x vs %02x)\r\n",
                   (unsigned)i, (unsigned)mapaddr[i], (unsigned)wspi_rbuf[i]);
        }
      }

      wspiUnmapFlash(&WSPID2);

      /*
       * A fresh pseudo random page for every attempt, a plain LCG keeps
       * the expected content well defined without needing real entropy.
       */
      seed = chVTGetSystemTime() ^ (attempt * 2654435761U);
      for (i = 0U; i < WSPI_TEST_PAGE_SIZE; i++) {
        seed = (seed * 1103515245U) + 12345U;
        wspi_wbuf[i] = (uint8_t)(seed >> 16U);
      }

      /*
       * Write path validation on the scratch sector: write enable, 4KB
       * sector erase, wait out the busy bit, write enable again, page
       * program, wait again, then read the page back through the plain
       * standard read and verify it byte by byte. The erase and program
       * commands run through the command path, the program data phase
       * through the DMA channel.
       */
      if (!wspiCommand(&WSPID2, &cmd_wren)) {
        chprintf(chp, "WSPI: write enable FAILED\r\n");
      }
      else if (!wspiCommand(&WSPID2, &cmd_serase)) {
        chprintf(chp, "WSPI: sector erase command FAILED\r\n");
      }
      else if (!wspi_wait_wip(&WSPID2, &cmd_rdsr)) {
        chprintf(chp, "WSPI: erase still busy, timeout\r\n");
      }
      else if (!wspiCommand(&WSPID2, &cmd_wren)) {
        chprintf(chp, "WSPI: write enable FAILED\r\n");
      }
      else if (!wspiSend(&WSPID2, &cmd_ppage, WSPI_TEST_PAGE_SIZE,
                         wspi_wbuf)) {
        chprintf(chp, "WSPI: page program FAILED\r\n");
      }
      else if (!wspi_wait_wip(&WSPID2, &cmd_rdsr)) {
        chprintf(chp, "WSPI: program still busy, timeout\r\n");
      }
      else if (wspiReceive(&WSPID2, &cmd_pread, WSPI_TEST_PAGE_SIZE,
                           wspi_rbuf)) {
        chprintf(chp, "WSPI: scratch read back FAILED\r\n");
      }
      else {
        for (i = 0U; i < WSPI_TEST_PAGE_SIZE; i++) {
          if (wspi_rbuf[i] != wspi_wbuf[i]) {
            break;
          }
        }

        if (i == WSPI_TEST_PAGE_SIZE) {
          chprintf(chp, "WSPI: scratch PASS, sector 0x%06x verified\r\n",
                   (unsigned)WSPI_TEST_SCRATCH_SECTOR);
        }
        else {
          chprintf(chp, "WSPI: scratch MISMATCH at %u (%02x vs %02x)\r\n",
                   (unsigned)i, (unsigned)wspi_rbuf[i], (unsigned)wspi_wbuf[i]);
        }
      }
    }

    wspiStop(&WSPID2);

    chThdSleepMilliseconds(WSPI_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the QSPI2 flash test thread.
 */
void wspi_test_start(void) {

  chThdCreateStatic(waWspiTest, sizeof(waWspiTest), NORMALPRIO, wspi_test_thd,
                    NULL);
}
