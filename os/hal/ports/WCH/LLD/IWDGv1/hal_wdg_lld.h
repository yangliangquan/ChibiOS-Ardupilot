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
 * @file    IWDGv1/hal_wdg_lld.h
 * @brief   CH32H417 WDG Driver low level driver header.
 *
 * @addtogroup WDG
 * @{
 */

#ifndef HAL_WDG_LLD_H
#define HAL_WDG_LLD_H

#if (HAL_USE_WDG == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    RLR register definitions
 * @{
 */
#define CH32_IWDG_RL_MASK                   (0x00000FFF << 0)
#define CH32_IWDG_RL(n)                     ((n) << 0)
/** @} */

/**
 * @name    PSCR register definitions
 * @{
 */
#define CH32_IWDG_PR_MASK                   (7 << 0)
#define CH32_IWDG_PR_4                      0U
#define CH32_IWDG_PR_8                      1U
#define CH32_IWDG_PR_16                     2U
#define CH32_IWDG_PR_32                     3U
#define CH32_IWDG_PR_64                     4U
#define CH32_IWDG_PR_128                    5U
#define CH32_IWDG_PR_256                    6U
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   IWDG driver enable switch.
 * @note    The default is @p FALSE.
 */
#if !defined(CH32H417_WDG_USE_IWDG) || defined(__DOXYGEN__)
#define CH32H417_WDG_USE_IWDG                  FALSE
#endif
/**
 * @brief   WWDG driver enable switch (second unit, WDGD2).
 * @note    The default is @p FALSE.
 */
#if !defined(CH32H417_WDG_USE_WWDG) || defined(__DOXYGEN__)
#define CH32H417_WDG_USE_WWDG                  FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_WDG_USE_IWDG && !CH32H417_WDG_USE_WWDG
#error "WDG driver activated but no xWDG peripheral assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of a structure representing an WDG driver.
 */
typedef struct WDGDriver WDGDriver;

/**
 * @brief   Driver configuration structure.
 * @note    Register-value style, interpreted per unit:
 *          - WDGD1 (IWDG): @p pr = prescaler (one of CH32_IWDG_PR_x),
 *            @p rlr = reload value (0...0xFFF).
 *          - WDGD2 (WWDG): @p pr = the whole CFGR value (window W[6:0] |
 *            prescaler WDGTB[8:7] | EWI[9]), @p rlr = the counter value
 *            loaded into CTLR (0x40...0x7F), WDGA is added by the driver.
 */
typedef struct {
  /**
   * @brief   Configuration of the IWDG_PSCR register.
   * @details One of @p CH32_IWDG_PR_x, the LSI clock is divided by
   *          4/8/.../256.
   */
  uint32_t    pr;
  /**
   * @brief   Configuration of the IWDG_RLDR register.
   * @details Reload value, 0...0x0FFF.
   */
  uint32_t    rlr;
} WDGConfig;

/**
 * @brief   Structure representing an WDG driver.
 */
struct WDGDriver {
  /**
   * @brief   Driver state.
   */
  wdgstate_t                state;
  /**
   * @brief   Current configuration data.
   */
  const WDGConfig           *config;
  /* End of the mandatory fields.*/
  /**
   * @brief   Pointer to the IWDG registers block.
   */
  IWDG_TypeDef              *wdg;
};

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_WDG_USE_IWDG == TRUE) && !defined(__DOXYGEN__)
extern WDGDriver WDGD1;
#endif
#if (CH32H417_WDG_USE_WWDG == TRUE) && !defined(__DOXYGEN__)
extern WDGDriver WDGD2;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void wdg_lld_init(void);
  void wdg_lld_start(WDGDriver *wdgp);
  void wdg_lld_stop(WDGDriver *wdgp);
  void wdg_lld_reset(WDGDriver *wdgp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_WDG == TRUE */

#endif /* HAL_WDG_LLD_H */

/** @} */
