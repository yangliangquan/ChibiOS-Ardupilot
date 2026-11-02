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
 * @file    IWDGv1/hal_wdg_lld.c
 * @brief   CH32H417 WDG Driver low level driver source.
 *
 * @addtogroup WDG
 * @{
 */

#include "hal.h"

#if (HAL_USE_WDG == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_iwdg.h"
#if (CH32H417_WDG_USE_WWDG == TRUE)
#include "ch32h417_wwdg.h"
#endif

#define CH32_WWDG_WDGA                       (1UL << 7)
#define CH32_WWDG_MAX_COUNTER                 0x7FU

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_WDG_USE_IWDG == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   IWDG driver identifier.
 */
WDGDriver WDGD1 = {
  .state = WDG_STOP,
  .config = NULL,
  .wdg = IWDG
};
#endif
#if (CH32H417_WDG_USE_WWDG == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   WWDG driver identifier (second unit).
 */
WDGDriver WDGD2 = {
  .state = WDG_STOP,
  .config = NULL,
  .wdg = NULL
};
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level WDG driver initialization.
 *
 * @notapi
 */
void wdg_lld_init(void) {
}

/**
 * @brief   Starts the watchdog.
 *
 * @param[in] wdgp      pointer to a @p WDGDriver object
 *
 * @notapi
 */
void wdg_lld_start(WDGDriver *wdgp) {
  const WDGConfig *config = wdgp->config;

#if (CH32H417_WDG_USE_WWDG == TRUE)
  if (wdgp == &WDGD2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_WWDG, ENABLE);
    WWDG->CFGR = config->pr;
    WWDG->CTLR = CH32_WWDG_WDGA | config->rlr;
    return;
  }
#endif

  IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
  IWDG_SetPrescaler((uint8_t)config->pr);
  IWDG_SetReload((uint16_t)config->rlr);
  IWDG_ReloadCounter();
  IWDG_Enable();
}

/**
 * @brief   Stops the watchdog.
 * @note    The IWDG hardware cannot be stopped once started, the
 *          function is a no-op, the driver state is handled by the
 *          generic driver.
 *
 * @param[in] wdgp      pointer to a @p WDGDriver object
 *
 * @notapi
 */
void wdg_lld_stop(WDGDriver *wdgp) {

#if (CH32H417_WDG_USE_WWDG == TRUE)
  if (wdgp == &WDGD2) {
    /* The window watchdog can be disarmed. */
    WWDG->CTLR &= (uint32_t)~CH32_WWDG_WDGA;
    return;
  }
#endif

  /* The IWDG hardware cannot be stopped once started, the function is
     a no-op, the driver state is handled by the generic driver. */
  (void)wdgp;
}

/**
 * @brief   Resets the watchdog counter.
 *
 * @param[in] wdgp      pointer to a @p WDGDriver object
 *
 * @notapi
 */
void wdg_lld_reset(WDGDriver *wdgp) {

#if (CH32H417_WDG_USE_WWDG == TRUE)
  if (wdgp == &WDGD2) {
    WWDG->CTLR = CH32_WWDG_WDGA | wdgp->config->rlr;
    return;
  }
#endif

  IWDG_ReloadCounter();
}

#endif /* HAL_USE_WDG == TRUE */

/** @} */
