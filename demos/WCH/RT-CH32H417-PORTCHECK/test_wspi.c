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
 * @details Every cycle proves the read path with an indirect quad read
 *          compared against the memory mapped window, then the write path
 *          with a full write and read back verification of the scratch
 *          sector. Nothing outside the scratch sector is touched.
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

/* DCR.FSIZE = address bits - 1. The fitted device is a W25Q64JV (JEDEC
   0x17), 64 Mbit = 8 MB, so FSIZE = 22. A larger value lets the unit
   accept addresses that do not exist on the die and the erase/program
   silently lands nowhere. */
#define WSPI_TEST_FSIZE 22U

/* Scratch geometry: the last 4KB sector is reprogrammed on every attempt.
   8MB = 0x800000 and the address plus length must stay below the size or
   the unit raises a transfer error (RM 37.3.10), so the sector below the
   top is used. */
#define WSPI_TEST_PAGE_SIZE 256U
#define WSPI_TEST_SCRATCH_SECTOR 0x7FE000UL

/* The whole sector is written and verified, which also covers a run of
   consecutive page programs. */
#define WSPI_TEST_SECTOR_SIZE 4096U
#define WSPI_TEST_SECTOR_PAGES (WSPI_TEST_SECTOR_SIZE / WSPI_TEST_PAGE_SIZE)

/* Bound of the WIP polling loop, in milliseconds. */
#define WSPI_TEST_WIP_TIMEOUT_MS 2000U

/* The 4KB sector erase is busy for tens of milliseconds (W25Q64JV tSE1
   typ 30ms), polling before that only adds bus traffic. */
#define WSPI_TEST_ERASE_MIN_MS 10U

/* Status register bits used by the flow. */
#define WSPI_SR_WIP 0x01U
#define WSPI_SR_WEL 0x02U

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

static const WSPIConfig wspi_config = {.end_cb = NULL,
                                       .error_cb = NULL,
                                       .clock_hz = WSPI_TEST_CLOCK_HZ,
                                       .fsize = WSPI_TEST_FSIZE};

/* Buffers for the DMA data phase, word aligned: one holds the page being
   programmed, one the page being read back, one the whole sector read back
   at once for the verification. */
static uint8_t wspi_wbuf[WSPI_TEST_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t wspi_rbuf[WSPI_TEST_PAGE_SIZE] __attribute__((aligned(4)));
static uint8_t wspi_vbuf[WSPI_TEST_SECTOR_SIZE] __attribute__((aligned(4)));

/*===========================================================================*/
/* Flash command descriptors.                                                */
/*===========================================================================*/

/* JEDEC identification, three bytes on one data line. */
static const wspi_command_t cmd_rdid = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE |
         WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_ONE_LINE,
  .cmd = 0x9FU,
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

/* Status register, a single byte frame. */
static const wspi_command_t cmd_rdsr = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_NONE |
         WSPI_CFG_ALT_MODE_NONE | WSPI_CFG_DATA_MODE_ONE_LINE,
  .cmd = 0x05U,
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

/* Write enable, command only: no address, no data. It must precede every
   erase and program, the device consumes WEL on each of them. */
static const wspi_command_t cmd_wren = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE,
  .cmd = 0x06U,
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

/* Address carrying descriptors, the flow fills the address in a local
   copy. */
static const wspi_command_t cmd_serase = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
         WSPI_CFG_ADDR_SIZE_24,
  .cmd = 0x20U,                            /* 4KB sector erase.        */
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

static const wspi_command_t cmd_ppage = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
         WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_DATA_MODE_ONE_LINE,
  .cmd = 0x02U,                            /* Page program.            */
  .addr = 0U,
  .alt = 0U,
  .dummy = 0U
};

/* 0x6B, fast read quad output: instruction and address on one line, 8 dummy
   cycles, data on four lines. No mode byte, so no ABR setup is needed and
   the frame works the same in indirect and mapped mode. */
static const wspi_command_t cmd_quad_read = {
  .cfg = WSPI_CFG_CMD_MODE_ONE_LINE | WSPI_CFG_ADDR_MODE_ONE_LINE |
         WSPI_CFG_ADDR_SIZE_24 | WSPI_CFG_ALT_MODE_NONE |
         WSPI_CFG_DATA_MODE_FOUR_LINES,
  .cmd = 0x6BU,
  .addr = 0U,
  .alt = 0U,
  .dummy = 8U
};

/*===========================================================================*/
/* Write and read back verification flow.                                    */
/*===========================================================================*/

/**
 * @brief   Outcome of one write and read back verification run.
 */
typedef enum {
  WSPI_FLOW_OK = 0,         /**< Sector programmed and verified.        */
  WSPI_FLOW_WREN,           /**< Device did not accept the write enable. */
  WSPI_FLOW_ERASE,          /**< Sector erase frame failed.             */
  WSPI_FLOW_PROGRAM,        /**< Page program frame failed.             */
  WSPI_FLOW_TIMEOUT,        /**< WIP stayed set beyond the bound.       */
  WSPI_FLOW_READ,           /**< Read back frame failed.                */
  WSPI_FLOW_MISMATCH        /**< Content differs from what was written. */
} wspi_flow_result_t;

/**
 * @brief   Point where a run stopped.
 */
typedef struct {
  uint32_t   offset;        /**< Sector relative offset of the failure. */
  uint8_t    expected;      /**< Pattern byte, mismatch only.           */
  uint8_t    actual;        /**< Byte read back, mismatch only.         */
} wspi_flow_status_t;

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*
 * Polarity: wspiCommand/wspiSend/wspiReceive return true on FAILURE
 * (hal_wspi.c ends with "return (bool)(msg != MSG_OK)"), the helpers below
 * return true on success.
 */

/* QSPI2 pin mapping of the flash, single device on the primary bus
   (RM table 2-2-25). The second-flash signals are not wired here.
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
 * Polls the write-in-progress bit with a bounded wait. Returns false when
 * the bit never clears or the status reads keep failing. The caller states
 * how long the operation is guaranteed to stay busy, the poll itself runs
 * at one tick (100us at the configured CH_CFG_ST_FREQUENCY) instead of
 * milliseconds: a page program is typically done in 400us.
 */
static bool wspi_wait_wip(WSPIDriver *wspip, const wspi_command_t *cmdp,
                          time_msecs_t busy_ms) {
  systime_t start = chVTGetSystemTime();
  uint8_t sr;

  if (busy_ms > (time_msecs_t)0) {
    chThdSleepMilliseconds(busy_ms);
  }

  while (true) {
    if (!wspiReceive(wspip, cmdp, 1U, &sr) && ((sr & WSPI_SR_WIP) == 0U)) {
      return true;
    }
    if (chTimeI2MS(chVTTimeElapsedSinceX(start)) >
        (time_msecs_t)WSPI_TEST_WIP_TIMEOUT_MS) {
      return false;
    }
    chThdSleep(1U);
  }
}

/**
 * @brief   Arms the device for the next erase or program operation.
 * @details The status is read back because the frame completing on the bus
 *          does not prove that the device latched WEL.
 *
 * @param[in] wspip     pointer to the driver instance
 * @return              true when WEL is set.
 */
static bool wspi_write_enable(WSPIDriver *wspip) {
  uint8_t sr;

  if (wspiCommand(wspip, &cmd_wren)) {
    return false;
  }
  if (wspiReceive(wspip, &cmd_rdsr, 1U, &sr)) {
    return false;
  }

  return (sr & WSPI_SR_WEL) != 0U;
}

/**
 * @brief   Deterministic page content.
 * @details A pure function of the absolute address, recomputed at
 *          verification time instead of kept in a buffer: a page written
 *          at a wrong address is then compared against what the correct
 *          address holds, not against itself.
 *
 * @param[in] addr      absolute flash address of the page base
 * @param[in] i         byte index inside the page
 * @return              The byte the page must hold at that index.
 */
static uint8_t wspi_pattern(uint32_t addr, unsigned i) {
  uint32_t v = addr + (uint32_t)i;

  v ^= v >> 7;
  v *= 0x9E3779B1UL;
  v ^= v >> 13;

  return (uint8_t)v;
}

/**
 * @brief   Human readable name of a flow outcome.
 *
 * @param[in] res       result code
 * @return              Static string, never NULL.
 */
static const char *wspi_flow_name(wspi_flow_result_t res) {

  switch (res) {
  case WSPI_FLOW_WREN:
    return "write enable";
  case WSPI_FLOW_ERASE:
    return "sector erase";
  case WSPI_FLOW_PROGRAM:
    return "page program";
  case WSPI_FLOW_TIMEOUT:
    return "busy timeout";
  case WSPI_FLOW_READ:
    return "read back";
  case WSPI_FLOW_MISMATCH:
    return "verify";
  default:
    return "unknown";
  }
}

/**
 * @brief   Writes the scratch sector and reads it back.
 * @details Erases the sector, programs every page with the address
 *          dependent pattern and compares the whole sector read back. Each
 *          step maps to its own result code so a failure names the command
 *          that broke.
 *
 * @param[in] wspip     pointer to the driver instance
 * @param[out] stp      where the run stopped, always initialized
 * @return              The outcome of the run.
 */
static wspi_flow_result_t wspi_write_verify(WSPIDriver *wspip,
                                            BaseSequentialStream *chp,
                                            wspi_flow_status_t *stp) {
  uint32_t base = WSPI_TEST_SCRATCH_SECTOR;
  unsigned p;
  unsigned i;

  stp->offset = 0U;
  stp->expected = 0U;
  stp->actual = 0U;

  /* Erase, a cell can only be programmed onto an erased page. */
  chprintf(chp, "WSPI: erase\r\n");
  if (!wspi_write_enable(wspip)) {
    return WSPI_FLOW_WREN;
  }
  {
    wspi_command_t cmd = cmd_serase;

    cmd.addr = base;
    if (wspiCommand(wspip, &cmd)) {
      return WSPI_FLOW_ERASE;
    }
  }
  if (!wspi_wait_wip(wspip, &cmd_rdsr, WSPI_TEST_ERASE_MIN_MS)) {
    return WSPI_FLOW_TIMEOUT;
  }

  /* Program page by page, with the write enable reissued per page. */
  chprintf(chp, "WSPI: program");
  for (p = 0U; p < WSPI_TEST_SECTOR_PAGES; p++) {
    uint32_t paddr = base + (p * WSPI_TEST_PAGE_SIZE);
    wspi_command_t cmd = cmd_ppage;

    chprintf(chp, " %u", (unsigned)p);
    for (i = 0U; i < WSPI_TEST_PAGE_SIZE; i++) {
      wspi_wbuf[i] = wspi_pattern(paddr, i);
    }

    cmd.addr = paddr;
    stp->offset = paddr - base;

    if (!wspi_write_enable(wspip)) {
      return WSPI_FLOW_WREN;
    }
    if (wspiSend(wspip, &cmd, WSPI_TEST_PAGE_SIZE, wspi_wbuf)) {
      return WSPI_FLOW_PROGRAM;
    }
    if (!wspi_wait_wip(wspip, &cmd_rdsr, 0U)) {
      return WSPI_FLOW_TIMEOUT;
    }
  }

  /* Read the whole sector back in one quad frame and compare it byte by
     byte, the flash auto increments the address inside the frame. */
  chprintf(chp, "\r\nWSPI: verify\r\n");
  {
    wspi_command_t cmd = cmd_quad_read;

    cmd.addr = base;
    if (wspiReceive(wspip, &cmd, WSPI_TEST_SECTOR_SIZE, wspi_vbuf)) {
      return WSPI_FLOW_READ;
    }
  }
  for (p = 0U; p < WSPI_TEST_SECTOR_PAGES; p++) {
    uint32_t paddr = base + (p * WSPI_TEST_PAGE_SIZE);

    for (i = 0U; i < WSPI_TEST_PAGE_SIZE; i++) {
      uint8_t expect = wspi_pattern(paddr, i);
      uint32_t off = (p * WSPI_TEST_PAGE_SIZE) + i;

      if (wspi_vbuf[off] != expect) {
        stp->offset = off;
        stp->expected = expect;
        stp->actual = wspi_vbuf[off];
        return WSPI_FLOW_MISMATCH;
      }
    }
  }

  return WSPI_FLOW_OK;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

/* The flow adds a call level and formatted reports on top of the read
   checks, 1024 bytes is too tight. */
static THD_WORKING_AREA(waWspiTest, 1536);

static THD_FUNCTION(wspi_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
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

    /* The LLD start has no failure path, this is informational. */
    (void)wspiStart(&WSPID2, &wspi_config);

    chprintf(chp, "WSPI: attempt %u\r\n", (unsigned)attempt);

    /* JEDEC identification: proves the command phase and a short DMA data
       phase. */
    id_ok = !wspiReceive(&WSPID2, &cmd_rdid, sizeof(id), id);
    if (id_ok) {
      chprintf(chp, "WSPI: id = %02x %02x %02x\r\n", (unsigned)id[0],
               (unsigned)id[1], (unsigned)id[2]);
    }
    else {
      chprintf(chp, "WSPI: read id FAILED\r\n");
    }

    if (!wspiReceive(&WSPID2, &cmd_rdsr, 1U, &sr)) {
      chprintf(chp, "WSPI: status = %02x\r\n", (unsigned)sr);
    }
    else {
      chprintf(chp, "WSPI: read status FAILED\r\n");
    }

    rd_ok = !wspiReceive(&WSPID2, &cmd_quad_read, WSPI_TEST_PAGE_SIZE,
                         wspi_rbuf);

    /* Do not touch the flash before the identification looks like a real
       device. A blank or erased part legitimately reads 0xFF, so the
       window is compared against the indirect result instead of being
       judged by its content. */
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
      wspiMapFlash(&WSPID2, &cmd_quad_read, &mapaddr);

      if (mapaddr == NULL) {
        chprintf(chp, "WSPI: memmap FAILED\r\n");
      }
      else {
        chprintf(chp, "WSPI: indirect [%02x %02x %02x %02x], "
                      "window [%02x %02x %02x %02x]\r\n",
                 (unsigned)wspi_rbuf[0], (unsigned)wspi_rbuf[1],
                 (unsigned)wspi_rbuf[2], (unsigned)wspi_rbuf[3],
                 (unsigned)mapaddr[0], (unsigned)mapaddr[1],
                 (unsigned)mapaddr[2], (unsigned)mapaddr[3]);

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

      /* Write path: erase, program every page, read back and compare. */
      {
        wspi_flow_status_t st;
        wspi_flow_result_t res;

        chprintf(chp, "WSPI: flow start\r\n");
        res = wspi_write_verify(&WSPID2, chp, &st);

        if (res == WSPI_FLOW_OK) {
          chprintf(chp, "WSPI: write/verify PASS, sector 0x%06x, %u bytes "
                        "in %u pages\r\n",
                   (unsigned)WSPI_TEST_SCRATCH_SECTOR,
                   (unsigned)WSPI_TEST_SECTOR_SIZE,
                   (unsigned)WSPI_TEST_SECTOR_PAGES);
        }
        else if (res == WSPI_FLOW_MISMATCH) {
          chprintf(chp, "WSPI: verify MISMATCH at +0x%03x, wrote %02x, "
                        "read %02x\r\n",
                   (unsigned)st.offset, (unsigned)st.expected,
                   (unsigned)st.actual);
        }
        else {
          chprintf(chp, "WSPI: %s FAILED at +0x%03x\r\n",
                   wspi_flow_name(res), (unsigned)st.offset);
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
