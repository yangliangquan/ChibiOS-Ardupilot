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
 * @file    hal_lld.c
 * @brief   CH32H417 HAL subsystem low level driver source.
 *
 * @addtogroup HAL
 * @{
 */

#include "hal.h"

#include "ch32h417_rcc.h"

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

uint32_t ch32h417_hclk = CH32H417_HCLK;
uint32_t ch32h417_sysclk = CH32H417_SYSCLK;
uint32_t ch32h417_core1clk = CH32H417_CORE1CLK;
uint32_t ch32h417_pllclk = CH32H417_PLLCLK;
uint32_t ch32h417_timclk1 = CH32H417_TIMCLK1;
uint32_t ch32h417_adcclk = 0U;

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Clock tree initialization (counterpart of the STM32
 *          @p stm32_clock_init()), invoked from @p __early_init()
 *          before the DATA/BSS segments are initialized.
 * @details Builds the tree described by the "Clock tree configuration"
 *          macros of @p hal_lld.h: the reference oscillator (HSE or
 *          HSI) feeds the main PLL, the PLL output is the system clock
 *          and the HB prescaler (HPRE) and the HCLK prescaler (FPRE)
 *          derive the core and the HB bus clocks from it. With the
 *          default configuration the result is HSE 25MHz -> PLL
 *          (x16, /1) -> SYSCLK 400MHz, HPRE=DIV1 (V5F core on SYSCLK),
 *          FPRE=DIV4 (HCLK/V3F domain = 100MHz).
 *          When the V3F boot stub has already set up a matching tree
 *          (the normal dual-core case) the existing configuration is
 *          kept untouched; otherwise the vendor sequence, taken from
 *          the WCH system file, rebuilds it, so the V5F image is also
 *          self-sufficient when started without the V3F stub having
 *          configured anything.
 * @note    Register and rodata operations only: this runs before
 *          .data/.bss initialization, no RAM globals may be touched.
 * @note    HSE start-up failure behavior matches the vendor code: the
 *          system remains on the reset clock (HSI), without fallback;
 *          the assertion in @p hal_lld_init() flags it on debug builds.
 *
 * @special
 */
void ch32h417_clock_init(void) {

#if !CH32H417_NO_INIT
  /* Idempotent fast path: verify the running tree against the target
     configuration field by field (no RAM access). */
  if (((RCC->CFGR0 & RCC_SWS) == (uint32_t)0x08) &&          /* SYSCLK = PLL */
      ((RCC->PLLCFGR & RCC_PLLSRC) == (uint32_t)CH32H417_PLL_SOURCE) &&
      ((RCC->PLLCFGR & RCC_PLLMUL) == (uint32_t)CH32H417_PLL_MUL_ENC) &&
      ((RCC->PLLCFGR & RCC_PLL_SRC_DIV) == (uint32_t)CH32H417_PLL_SRC_DIV_ENC) &&
      ((RCC->PLLCFGR & RCC_SYSPLL_SEL) == (uint32_t)0) &&
      ((RCC->CFGR0 & RCC_HPRE) == (uint32_t)CH32H417_HPRE_ENC) &&
      ((RCC->CFGR0 & RCC_FPRE) == (uint32_t)CH32H417_FPRE_ENC)) {
    /* The tree is already in place, the flash latency is the only
       remaining item that could be out of date. */
    if ((FLASH->ACTLR & (uint32_t)FLASH_ACTLR_SCK_CFG) !=
        (uint32_t)CH32H417_FLASH_LATENCY) {
      FLASH->ACTLR = (FLASH->ACTLR & ~((uint32_t)FLASH_ACTLR_SCK_CFG)) |
                     (uint32_t)CH32H417_FLASH_LATENCY;
      CH32H417_DSB();
    }
    return;
  }

  /* Vendor SystemInit() preamble: enforce the reset situation (HSI
     running as SYSCLK, PLLs released, registers back to defaults).
     The masks below are the vendor's literal values. */
  RCC->CTLR |= (uint32_t)0x00000001;
  RCC->CFGR0 &= (uint32_t)0x305C0000;
  CH32H417_DSB();
  while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x00) {
  }
  RCC->CFGR0 &= (uint32_t)0xFFBFFFFF;
  RCC->PLLCFGR &= (uint32_t)0x7FFFFFFF;
  RCC->CTLR &= (uint32_t)0x6AA6FFFF;
  RCC->CTLR &= (uint32_t)0xFFFBFFFF;
  RCC->PLLCFGR &= (uint32_t)0x0FFFC000;
  RCC->PLLCFGR |= (uint32_t)0x00000004;
  RCC->INTR = 0x00FF0000;
  RCC->CFGR2 &= 0x0C600000;
  RCC->PLLCFGR2 &= 0xFFF0E080;
  RCC->PLLCFGR2 |= 0x00080020;

#if CH32H417_USE_HSE
  /* HSE activation with the vendor bounded wait. */
  {
    uint32_t start_up_counter = 0U;
    uint32_t hse_status;

    RCC->CTLR |= (uint32_t)RCC_HSEON;
    CH32H417_DSB();
    do {
      hse_status = RCC->CTLR & RCC_HSERDY;
      start_up_counter++;
    } while ((hse_status == 0U) && (start_up_counter != HSE_STARTUP_TIMEOUT));

    if ((RCC->CTLR & RCC_HSERDY) == RESET) {
      /* HSE start-up failure: stay on the reset clock (HSI), the same
         behavior as the vendor code, the assertion in hal_lld_init()
         flags it on debug builds. */
      return;
    }
  }
#endif /* CH32H417_USE_HSE */

  /* PLL configuration: reference clock, input divider and
     multiplication factor, all taken from the configuration macros. */
  RCC->PLLCFGR &= (uint32_t)~RCC_PLLMUL;
  RCC->PLLCFGR |= (uint32_t)CH32H417_PLL_MUL_ENC;
  RCC->PLLCFGR &= (uint32_t)~RCC_PLL_SRC_DIV;
  RCC->PLLCFGR |= (uint32_t)CH32H417_PLL_SRC_DIV_ENC;
  RCC->PLLCFGR &= (uint32_t)~RCC_PLLSRC;
  RCC->PLLCFGR |= (uint32_t)CH32H417_PLL_SOURCE;
  CH32H417_DSB();
  while ((RCC->PLLCFGR & (uint32_t)RCC_PLLSRC) !=
         (uint32_t)CH32H417_PLL_SOURCE) {
  }

  RCC->CTLR |= RCC_PLLON;
  CH32H417_DSB();
  while ((RCC->CTLR & RCC_PLLRDY) != (uint32_t)RCC_PLLRDY) {
  }

  /* Release the system PLL selector until the tree is ready. */
  RCC->PLLCFGR &= (uint32_t)~RCC_SYSPLL_GATE;
  RCC->PLLCFGR &= (uint32_t)~RCC_SYSPLL_SEL;
  CH32H417_DSB();
  while ((RCC->PLLCFGR & (uint32_t)RCC_SYSPLL_SEL) != (uint32_t)0) {
  }

  /* Core and bus prescalers. */
  RCC->CFGR0 &= (uint32_t)~RCC_HPRE;
  RCC->CFGR0 |= (uint32_t)CH32H417_HPRE_ENC;
  RCC->CFGR0 &= (uint32_t)~RCC_FPRE;
  RCC->CFGR0 |= (uint32_t)CH32H417_FPRE_ENC;

  /* Flash access, the latency is derived from the resulting HCLK. */
  FLASH->ACTLR = (FLASH->ACTLR & ~((uint32_t)FLASH_ACTLR_SCK_CFG)) |
                 (uint32_t)CH32H417_FLASH_LATENCY;
  /* The latency must be committed before the speed switch below. */
  CH32H417_DSB();

  /* Switch SYSCLK over to the PLL. */
  RCC->PLLCFGR |= (uint32_t)RCC_SYSPLL_GATE;
  RCC->CFGR0 &= (uint32_t)~RCC_SW;
  RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
  CH32H417_DSB();
  while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08) {
  }
#endif /* !CH32H417_NO_INIT */
}

/**
 * @brief   Low level HAL driver initialization.
 * @details Pulses the peripheral resets (GPIO ports and AFIO excluded:
 *          the pads are already configured by @p __early_init(), PWR
 *          and the backup domain are left alone, same exclusions as
 *          the STM32 ports) and then decodes the actual clock tree
 *          into the vendor frequency tracking variables used by the
 *          peripheral library.
 *
 * @notapi
 */
void hal_lld_init(void) {
  uint32_t hb2_keep = RCC_AFIORST | RCC_IOPARST | RCC_IOPBRST |
                      RCC_IOPCRST | RCC_IOPDRST | RCC_IOPERST |
                      RCC_IOPFRST;
  uint32_t hb1_keep = RCC_PWRRST | RCC_BKPRST;

  /* Assert and release the resets of everything but the preserved
     set. HBRSTR (DMA/FMC/SDMMC/USB/ETH side) is not touched: the
     header has no bit definitions for it. */
  RCC->HB2PRSTR |= ~hb2_keep;
  CH32H417_DSB();
  RCC->HB2PRSTR &= hb2_keep;
  CH32H417_DSB();
  RCC->HB1PRSTR |= ~hb1_keep;
  CH32H417_DSB();
  RCC->HB1PRSTR &= hb1_keep;
  CH32H417_DSB();
  (void)RCC->HB2PRSTR;
  (void)RCC->HB1PRSTR;

  /* Frequency tracking: decode the register state (SystemClock=SYSCLK,
     SystemCoreClock=V5F core, HCLKClock=FPRE/V3F domain). */
  SystemAndCoreClockUpdate();

  /* Update live clock variables from the hardware state so peripheral
     drivers can derive their rates from the actual tree (which may have
     been configured by a bootloader or differ due to HSE start-up). */
  ch32h417_clock_update_live();

  /* DMA subsystem initialization, after the reset pulses above so that the
     controllers are in a known state.*/
#if defined(CH32_DMA_REQUIRED)
  dmaInit();
#endif

  osalDbgAssert(SystemClock == CH32H417_SYSCLK,
                "unexpected system clock (HSE start-up failed?)");
  osalDbgAssert(SystemCoreClock == CH32H417_CORE1CLK,
                "unexpected core clock");
  osalDbgAssert(HCLKClock == CH32H417_HCLK,
                "unexpected HB bus clock");
}

/**
 * @brief   Get clocks from the vendor RCC helper.
 */
void ch32h417_clock_get_rcc_clocks(RCC_ClocksTypeDef *clocks) {

  if (clocks == NULL) {
    return;
  }

  RCC_GetClocksFreq(clocks);
}

/**
 * @brief   Update live clock variables from hardware.
 */
void ch32h417_clock_update_live(void) {
  RCC_ClocksTypeDef clocks;

  RCC_GetClocksFreq(&clocks);
  ch32h417_sysclk = clocks.SYSCLK_Frequency;
  ch32h417_hclk = clocks.HCLK_Frequency;
  ch32h417_core1clk = clocks.Core_Frequency;
  ch32h417_pllclk = ch32h417_sysclk;
  ch32h417_timclk1 = ch32h417_hclk;
  ch32h417_adcclk = clocks.ADCCLK_Frequency;
}

/**
 * @brief   Returns the frequency of a clock point in Hz.
 */
uint32_t hal_lld_get_clock_point(uint32_t clkpt) {

  (void)clkpt;

  return ch32h417_hclk;
}

/** @} */
