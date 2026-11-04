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
 * @file    hal_lld.h
 * @brief   CH32H417 HAL subsystem low level driver header.
 *
 * @details The clock tree of the CH32H417 is described by the macros of
 *          the "Clock tree configuration" section below, all of them can
 *          be overridden from the board file or from "mcuconf.h". The
 *          frequencies of the clock domains are then computed from
 *          those macros and exported to the peripheral drivers:
 *
 *          - CH32H417_SYSCLK:    system clock, the output of the main
 *                                PLL.
 *          - CH32H417_CORE1CLK:  clock of the RISC-V V5F core.
 *          - CH32H417_HCLK:      clock of the HB bus, this is the clock
 *                                of all the peripherals placed on the
 *                                HB1, HB2 and HB domains.
 *          - CH32H417_TIMCLK1:   clock of the timers, same as HCLK.
 *          - CH32H417_PLLCLK:    output of the main PLL, it clocks the
 *                                SDMMC, DFSDM, SAI and ECDC blocks.
 *
 * @addtogroup HAL
 * @{
 */

#ifndef HAL_LLD_H
#define HAL_LLD_H

#include <stdint.h>

#include "ch32h417.h"
#include "ch32h417_rcc.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Platform identification macros
 * @{
 */
#define PLATFORM_NAME           "WCH CH32H417 (QingKe V5F)"

/**
 * @brief   Requires the SPI driver v2 model (msg_t based transfers).
 */
#define HAL_LLD_SELECT_SPI_V2           TRUE
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Clock tree configuration
 * @{
 */
/**
 * @brief   HSE oscillator frequency in Hz.
 * @note    Board related setting, see the board file.
 */
#if !defined(CH32H417_HSECLK) || defined(__DOXYGEN__)
#define CH32H417_HSECLK                     25000000U
#endif

/**
 * @brief   HSI oscillator frequency in Hz.
 */
#if !defined(CH32H417_HSICLK) || defined(__DOXYGEN__)
#define CH32H417_HSICLK                     25000000U
#endif

/**
 * @brief   Main PLL reference clock source.
 * @note    TRUE selects the HSE external oscillator, FALSE selects the
 *          HSI internal oscillator.
 */
#if !defined(CH32H417_USE_HSE) || defined(__DOXYGEN__)
#define CH32H417_USE_HSE                    TRUE
#endif

/**
 * @brief   Main PLL reference clock divider, 1...64.
 */
#if !defined(CH32H417_PLL_SRC_DIV) || defined(__DOXYGEN__)
#define CH32H417_PLL_SRC_DIV               1U
#endif

/**
 * @brief   Main PLL multiplication factor.
 * @note    The RM offers the factors x4, x6, x7, x8 up to x40 and x59,
 *          the half steps x8.5 ... x12.5 are selected with
 *          @p CH32H417_PLL_MUL_HALF.
 */
#if !defined(CH32H417_PLL_MUL) || defined(__DOXYGEN__)
#define CH32H417_PLL_MUL                   16U
#endif

/**
 * @brief   Main PLL half step.
 * @note    When TRUE the xN.5 factor is selected, it is only available
 *          for N = 8 ... 12.
 */
#if !defined(CH32H417_PLL_MUL_HALF) || defined(__DOXYGEN__)
#define CH32H417_PLL_MUL_HALF              FALSE
#endif

/**
 * @brief   HB prescaler, SYSCLK to Core1 clock divider.
 * @note    The supported values are 1, 2, 4, 8, 16, 64, 128, 256 and
 *          512.
 */
#if !defined(CH32H417_HPRE_DIV) || defined(__DOXYGEN__)
#define CH32H417_HPRE_DIV                  1U
#endif

/**
 * @brief   FPRE prescaler, Core1 clock to HCLK divider.
 * @note    The supported values are 1, 2 and 4.
 */
#if !defined(CH32H417_FPRE_DIV) || defined(__DOXYGEN__)
#define CH32H417_FPRE_DIV                  4U
#endif

/**
 * @brief   Disables the clock tree setup in @p ch32h417_clock_init().
 * @note    When TRUE the clock tree is left as the reset code (or as
 *          the V3F boot stub) set it up, the frequency macros keep
 *          describing the intended configuration.
 */
#if !defined(CH32H417_NO_INIT) || defined(__DOXYGEN__)
#define CH32H417_NO_INIT                   FALSE
#endif
/** @} */

#include "ch32h417.h"
#include "hal_dma_lld.h"

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

/*
 * Configuration-related checks.
 */
#if !defined(CH32H417_MCUCONF)
#error "Using a wrong mcuconf.h file, CH32H417_MCUCONF not defined"
#endif

#if (CH32H417_PLL_SRC_DIV < 1U) || (CH32H417_PLL_SRC_DIV > 64U)
#error "CH32H417_PLL_SRC_DIV must be in the range 1...64"
#endif

/* Main PLL reference clock and its register encoding. */
#if CH32H417_USE_HSE
#define CH32H417_PLL_SOURCE                RCC_PLLSRC_HSE
#define CH32H417_PLL_REFFREQ               (CH32H417_HSECLK / CH32H417_PLL_SRC_DIV)
#else
#define CH32H417_PLL_SOURCE                RCC_PLLSRC_HSI
#define CH32H417_PLL_REFFREQ               (CH32H417_HSICLK / CH32H417_PLL_SRC_DIV)
#endif

/* Main PLL multiplication factor expressed in half units, the RM offers
   the half steps x8.5 ... x12.5 on top of the integer factors. */
#define CH32H417_PLL_MUL_HALF_UNITS        ((CH32H417_PLL_MUL * 2U) +               \
                                            (CH32H417_PLL_MUL_HALF ? 1U : 0U))

/* PLLMUL[4:0] encoding of RCC_PLLCFGR. */
#if   CH32H417_PLL_MUL_HALF_UNITS == 8U
#define CH32H417_PLL_MUL_ENC               0x00U   /* x4 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 12U
#define CH32H417_PLL_MUL_ENC               0x01U   /* x6 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 14U
#define CH32H417_PLL_MUL_ENC               0x02U   /* x7 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 16U
#define CH32H417_PLL_MUL_ENC               0x03U   /* x8 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 17U
#define CH32H417_PLL_MUL_ENC               0x04U   /* x8.5 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 18U
#define CH32H417_PLL_MUL_ENC               0x05U   /* x9 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 19U
#define CH32H417_PLL_MUL_ENC               0x06U   /* x9.5 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 20U
#define CH32H417_PLL_MUL_ENC               0x07U   /* x10 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 21U
#define CH32H417_PLL_MUL_ENC               0x08U   /* x10.5 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 22U
#define CH32H417_PLL_MUL_ENC               0x09U   /* x11 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 23U
#define CH32H417_PLL_MUL_ENC               0x0AU   /* x11.5 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 24U
#define CH32H417_PLL_MUL_ENC               0x0BU   /* x12 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 25U
#define CH32H417_PLL_MUL_ENC               0x0CU   /* x12.5 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 26U
#define CH32H417_PLL_MUL_ENC               0x0DU   /* x13 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 28U
#define CH32H417_PLL_MUL_ENC               0x0EU   /* x14 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 30U
#define CH32H417_PLL_MUL_ENC               0x0FU   /* x15 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 32U
#define CH32H417_PLL_MUL_ENC               0x10U   /* x16 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 34U
#define CH32H417_PLL_MUL_ENC               0x11U   /* x17 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 36U
#define CH32H417_PLL_MUL_ENC               0x12U   /* x18 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 38U
#define CH32H417_PLL_MUL_ENC               0x13U   /* x19 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 40U
#define CH32H417_PLL_MUL_ENC               0x14U   /* x20 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 44U
#define CH32H417_PLL_MUL_ENC               0x15U   /* x22 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 48U
#define CH32H417_PLL_MUL_ENC               0x16U   /* x24 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 52U
#define CH32H417_PLL_MUL_ENC               0x17U   /* x26 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 56U
#define CH32H417_PLL_MUL_ENC               0x18U   /* x28 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 60U
#define CH32H417_PLL_MUL_ENC               0x19U   /* x30 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 64U
#define CH32H417_PLL_MUL_ENC               0x1AU   /* x32 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 68U
#define CH32H417_PLL_MUL_ENC               0x1BU   /* x34 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 72U
#define CH32H417_PLL_MUL_ENC               0x1CU   /* x36 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 76U
#define CH32H417_PLL_MUL_ENC               0x1DU   /* x38 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 80U
#define CH32H417_PLL_MUL_ENC               0x1EU   /* x40 */
#elif CH32H417_PLL_MUL_HALF_UNITS == 118U
#define CH32H417_PLL_MUL_ENC               0x1FU   /* x59 */
#else
#error "the requested PLL multiplication factor is not available"
#endif

/* HB prescaler encoding, the HPRE[3:0] field of RCC_CFGR0. */
#if   CH32H417_HPRE_DIV == 1
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV1
#elif CH32H417_HPRE_DIV == 2
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV2
#elif CH32H417_HPRE_DIV == 4
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV4
#elif CH32H417_HPRE_DIV == 8
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV8
#elif CH32H417_HPRE_DIV == 16
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV16
#elif CH32H417_HPRE_DIV == 64
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV64
#elif CH32H417_HPRE_DIV == 128
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV128
#elif CH32H417_HPRE_DIV == 256
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV256
#elif CH32H417_HPRE_DIV == 512
#define CH32H417_HPRE_ENC                  RCC_HPRE_DIV512
#else
#error "CH32H417_HPRE_DIV must be 1, 2, 4, 8, 16, 64, 128, 256 or 512"
#endif

/* HCLK prescaler encoding, the FPRE[1:0] field of RCC_CFGR0. */
#if   CH32H417_FPRE_DIV == 1
#define CH32H417_FPRE_ENC                  RCC_FPRE_DIV1
#elif CH32H417_FPRE_DIV == 2
#define CH32H417_FPRE_ENC                  RCC_FPRE_DIV2
#elif CH32H417_FPRE_DIV == 4
#define CH32H417_FPRE_ENC                  RCC_FPRE_DIV4
#else
#error "CH32H417_FPRE_DIV must be 1, 2 or 4"
#endif

/* PLL input divider encoding, the PLL_SRC_DIV[5:0] field of
   RCC_PLLCFGR, the divider value is encoded as (divider - 1). */
#define CH32H417_PLL_SRC_DIV_ENC            (((CH32H417_PLL_SRC_DIV - 1U) & 0x3FU) << 8)

/* System clock frequency in Hz, the output of the main PLL. */
#define CH32H417_SYSCLK                     ((CH32H417_PLL_REFFREQ * CH32H417_PLL_MUL_HALF_UNITS) / 2U)

/* Clock of the RISC-V V5F core, SYSCLK divided by the HB prescaler. */
#define CH32H417_CORE1CLK                   (CH32H417_SYSCLK / CH32H417_HPRE_DIV)

/* HCLK frequency in Hz, the clock of the HB bus and of all the
   peripherals placed on the HB1, HB2 and HB domains. */
#define CH32H417_HCLK                       (CH32H417_CORE1CLK / CH32H417_FPRE_DIV)

/* Frequency of the main PLL output in Hz, it clocks the SDMMC, DFSDM,
   SAI and ECDC blocks (RM 3.3.5). */
#define CH32H417_PLLCLK                     CH32H417_SYSCLK

/* Frequency of the timers in Hz, all the timers (TIM1..TIM12) are
   clocked by HCLK on this platform. */
#define CH32H417_TIMCLK1                    CH32H417_HCLK

/* The system clock cannot exceed the 400MHz maximum of the V5F core,
   the check is evaluated in 64 bits by the preprocessor. */
#if (CH32H417_SYSCLK > 400000000U)
#error "the configured system clock exceeds the 400MHz limit"
#endif

/* Flash access latency, the SCK_CFG field of FLASH_ACTLR selects the
   flash clock among HCLK, HCLK/2 and HCLK/4 (RM 46.4.1). */
#if   (CH32H417_HCLK <= 60000000U)
#define CH32H417_FLASH_LATENCY             FLASH_ACTLR_LATENCY_HCLK_DIV1
#elif (CH32H417_HCLK <= 120000000U)
#define CH32H417_FLASH_LATENCY             FLASH_ACTLR_LATENCY_HCLK_DIV2
#elif (CH32H417_HCLK <= 150000000U)
#define CH32H417_FLASH_LATENCY             FLASH_ACTLR_LATENCY_HCLK_DIV4
#else
#error "the configured HCLK frequency is too high for the flash interface"
#endif

/**
 * @name    Live clock access
 * @{
 */
extern uint32_t ch32h417_hclk;
extern uint32_t ch32h417_sysclk;
extern uint32_t ch32h417_core1clk;
extern uint32_t ch32h417_pllclk;
extern uint32_t ch32h417_timclk1;
extern uint32_t ch32h417_adcclk;

#ifdef __cplusplus
extern "C" {
#endif
void ch32h417_clock_get_rcc_clocks(RCC_ClocksTypeDef *clocks);
void ch32h417_clock_update_live(void);
#ifdef __cplusplus
}
#endif

/**
 * @brief   Returns the frequency of a clock point in Hz.
 * @note    For CH32H417 the frequency is obtained from the live hardware
 *          state via RCC_GetClocksFreq().
 *
 * @param[in] clkpt     clock point to be returned
 * @return              The clock point frequency in Hz or zero if the
 *                      frequency is unknown.
 *
 * @notapi
 */
uint32_t hal_lld_get_clock_point(uint32_t clkpt);
/** @} */

/**
 * @brief   Full memory barrier (fence iorw,iorw).
 * @details The CH32H417 V5F core is dual-issue and out-of-order (RM
 *          ch4.1): device write buffering and clock domain crossings
 *          (RCC gate -> peripheral, PWR -> backup domain, oscillator
 *          -> RTC) do not keep program order on their own. Use it
 *          after register writes that later reads or writes in other
 *          blocks must observe: flag polls and read-modify-write
 *          sequences, clock source switches, unlock-then-configure
 *          sequences. crt0 uses the separate fence.i for the
 *          instruction stream after code copies.
 */
#define CH32H417_DSB() __asm volatile ("fence iorw, iorw" : : : "memory")

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void hal_lld_init(void);
  void ch32h417_clock_init(void);
#ifdef __cplusplus
}
#endif

#endif /* HAL_LLD_H */

/** @} */
