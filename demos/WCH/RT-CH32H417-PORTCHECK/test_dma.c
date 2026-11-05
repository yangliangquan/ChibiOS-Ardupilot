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
 * @file    test_dma.c
 * @brief   CH32H417 V5F port check: DMA boot smoke test.
 *
 * @details A memory to memory copy needs no peripheral request and no pin
 *          configuration, so it validates the whole DMA path (clock
 *          gating, dynamic channel allocation, mode programming,
 *          completion) on its own. The completion is polled with a bound so
 *          that a DMA problem cannot stall the boot.
 */

#include "board.h"
#include "ch.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t dma_selftest_result;

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Runs the DMA smoke test.
 * @note    The channel is taken from the free pool and given back right
 *          away, the drivers started afterwards get a different one.
 */
void dma_test_run(void) {
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
