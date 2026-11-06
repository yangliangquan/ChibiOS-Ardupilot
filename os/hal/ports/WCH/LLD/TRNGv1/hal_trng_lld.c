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
 * @file    TRNGv1/hal_trng_lld.c
 * @brief   CH32H417 TRNG subsystem low level driver source.
 *
 * @details Polled generator: waits for DRDY per 32-bit word, bounded
 *          by a spin limit so a clock/seed problem returns an error
 *          instead of hanging. Seed errors are cleared once and
 *          generation retried.
 *
 * @addtogroup TRNG
 * @{
 */

#include "hal.h"

#if (HAL_USE_TRNG == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_rng.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* Bounded wait per word (RNG clock is HCLK-class; generous limit). */
#define TRNG_SPIN_LIMIT                      2000000U

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_TRNG_USE_RNG == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   TRNG driver identifier.
 */
TRNGDriver TRNGD1;
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level TRNG driver initialization.
 *
 * @notapi
 */
void trng_lld_init(void) {

  trngObjectInit(&TRNGD1);
  RCC_HBPeriphClockCmd(RCC_HBPeriph_RNG, ENABLE);
}

/**
 * @brief   TRNG driver start.
 *
 * @notapi
 */
void trng_lld_start(TRNGDriver *trngp) {

  (void)trngp;

  RNG_Cmd(ENABLE);
}

/**
 * @brief   TRNG driver stop.
 *
 * @notapi
 */
void trng_lld_stop(TRNGDriver *trngp) {

  (void)trngp;

  RNG_Cmd(DISABLE);
}

/**
 * @brief   Fills the buffer with random bytes.
 *
 * @return              The generation status.
 * @retval false        success.
 * @retval true         clock or seed error / timeout.
 *
 * @notapi
 */
bool trng_lld_generate(TRNGDriver *trngp, size_t size, uint8_t *out) {
  size_t i;
  uint32_t word = 0U;

  (void)trngp;

  for (i = 0U; i < size; i++) {

    if ((i & 3U) == 0U) {
      uint32_t spin = TRNG_SPIN_LIMIT;

      /* A pending seed error is cleared and retried; a clock error
         is fatal for this call. */
      if ((RNG->SR & RNG_FLAG_SECS) != 0U) {
        RNG_ClearFlag(RNG_FLAG_SECS);
      }
      if ((RNG->SR & RNG_FLAG_CECS) != 0U) {
        return true;
      }

      while ((RNG->SR & RNG_FLAG_DRDY) == 0U) {
        if (--spin == 0U) {
          return true;
        }
      }

      word = RNG_GetRandomNumber();
    }

    *out++ = (uint8_t)(word & 0xFFU);
    word >>= 8;
  }

  return false;
}

#endif /* HAL_USE_TRNG == TRUE */

/** @} */
