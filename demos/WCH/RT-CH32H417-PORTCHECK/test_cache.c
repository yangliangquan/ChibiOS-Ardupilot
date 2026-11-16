/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/**
 * @file    test_cache.c
 * @brief   CH32H417 V5F port check: instruction cache test thread.
 *
 * @details Reports the cache strategy left behind by crt0.S (it must
 *          match the PORT_RISCV_ICACHE_* switches of the startup file,
 *          the decoded fields are printed) and then exercises the master
 *          switch and the per address space bits at run time, checking
 *          that the core keeps executing across every write.
 *          The code of this port runs from ITCM (zero wait), so what is
 *          validated here is the register path, not a speed-up: a
 *          measurement of the cache benefit needs flash-resident code.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#define CACHE_TEST_RETRY_MS 2000U

/*
 * Bits of cache_strtg_ctlr that are both writable and meaningful: the
 * master switch and the four address space enables.
 */
#define CACHE_TEST_WRITABLE_MASK \
  ((uint32_t)0x0F000000U | (uint32_t)CACHE_STRTG_CTLR_IC_DISABLE)

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t cache_rounds;
volatile uint32_t cache_failures;
volatile uint32_t cache_strtg;

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waCacheTest, 512);

static THD_FUNCTION(cache_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint32_t boot_value;
  uint32_t strtg;
  bool failed;

  (void)arg;

  boot_value = ch32h417_icache_get();
  chprintf(chp, "ICACHE: boot ctlr=0x%08x enabled=%u code=%u sram=%u "
                "mem0=%u mem1=%u\r\n",
           (unsigned)boot_value,
           (unsigned)((boot_value & (uint32_t)CACHE_STRTG_CTLR_IC_DISABLE) ?
                      0U : 1U),
           (unsigned)((boot_value & (uint32_t)CACHE_STRTG_CTLR_IC_CODE) ?
                      1U : 0U),
           (unsigned)((boot_value & (uint32_t)CACHE_STRTG_CTLR_IC_SRAM) ?
                      1U : 0U),
           (unsigned)((boot_value & (uint32_t)CACHE_STRTG_CTLR_IC_MEM0) ?
                      1U : 0U),
           (unsigned)((boot_value & (uint32_t)CACHE_STRTG_CTLR_IC_MEM1) ?
                      1U : 0U));

  while (true) {
    failed = false;

    /* 1) Master switch: disabling must set IC_DISABLE, the core keeps
          running across the write. */
    ch32h417_icache_set(boot_value |
                        (uint32_t)CACHE_STRTG_CTLR_IC_DISABLE);
    strtg = ch32h417_icache_get();
    if ((strtg & (uint32_t)CACHE_STRTG_CTLR_IC_DISABLE) == 0U) {
      failed = true;
      chprintf(chp, "ICACHE: cannot disable (ctlr=0x%08x)\r\n",
               (unsigned)strtg);
    }
    cache_strtg = strtg;

    /* 2) Address space enables: a strategy with no space allowed must
          read back with the four region bits clear. */
    ch32h417_icache_set(0U);
    strtg = ch32h417_icache_get();
    if ((strtg & (uint32_t)0x0F000000U) != 0U) {
      failed = true;
      chprintf(chp, "ICACHE: region bits stuck (ctlr=0x%08x)\r\n",
               (unsigned)strtg);
    }

    /* 3) Back to the boot strategy, byte for byte. */
    ch32h417_icache_set(boot_value);
    strtg = ch32h417_icache_get();
    if (strtg != boot_value) {
      failed = true;
      chprintf(chp, "ICACHE: restore failed (ctlr=0x%08x boot=0x%08x)\r\n",
               (unsigned)strtg, (unsigned)boot_value);
    }
    cache_strtg = strtg;

    cache_rounds++;
    if (failed) {
      cache_failures++;
    }

    chprintf(chp, "ICACHE: rounds=%u failures=%u ctlr=0x%08x\r\n",
             (unsigned)cache_rounds, (unsigned)cache_failures,
             (unsigned)cache_strtg);

    chThdSleepMilliseconds(CACHE_TEST_RETRY_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the instruction cache test thread.
 */
void cache_test_start(void) {

  chThdCreateStatic(waCacheTest, sizeof(waCacheTest), NORMALPRIO,
                    cache_test_thd, NULL);
}
