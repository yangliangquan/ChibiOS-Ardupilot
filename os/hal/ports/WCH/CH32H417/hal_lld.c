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

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Clock tree initialization (counterpart of the STM32
 *          @p stm32_clock_init()), invoked from @p __early_init()
 *          before the DATA/BSS segments are initialized.
 * @details Establishes the target tree: HSE 25MHz -> PLL (x16, /1) ->
 *          SYSCLK 400MHz, HPRE=DIV1 (V5F core on SYSCLK), FPRE=DIV4
 *          (HCLK/V3F domain = 100MHz). When the V3F boot stub has
 *          already set up a matching tree (the normal dual-core case)
 *          the existing configuration is kept untouched; otherwise the
 *          vendor sequence, taken from the WCH system file, rebuilds
 *          it, so the V5F image is also self-sufficient when started
 *          without the V3F stub having configured anything.
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
      ((RCC->PLLCFGR & RCC_PLLSRC) == RCC_PLLSRC_HSE) &&
      ((RCC->PLLCFGR & RCC_PLLMUL) == RCC_PLLMUL16) &&
      ((RCC->PLLCFGR & RCC_PLL_SRC_DIV) == RCC_PLL_SRC_DIV1) &&
      ((RCC->PLLCFGR & RCC_SYSPLL_SEL) == (uint32_t)0) &&
      ((RCC->CFGR0 & RCC_HPRE) == RCC_HPRE_DIV1) &&
      ((RCC->CFGR0 & RCC_FPRE) == RCC_FPRE_DIV4)) {
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

    if ((RCC->CTLR & RCC_HSERDY) != RESET) {

      /* PLL configuration: HSE 25MHz x16, no input divider. */
      RCC->PLLCFGR &= (uint32_t)~RCC_PLLMUL;
      RCC->PLLCFGR |= (uint32_t)RCC_PLLMUL16;
      RCC->PLLCFGR &= (uint32_t)~RCC_PLL_SRC_DIV;
      RCC->PLLCFGR |= (uint32_t)RCC_PLL_SRC_DIV1;
      RCC->PLLCFGR &= (uint32_t)~RCC_PLLSRC;
      RCC->PLLCFGR |= (uint32_t)RCC_PLLSRC_HSE;
      CH32H417_DSB();
      while ((RCC->PLLCFGR & (uint32_t)RCC_PLLSRC) !=
             (uint32_t)RCC_PLLSRC_HSE) {
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
      RCC->CFGR0 |= (uint32_t)RCC_HPRE_DIV1;
      RCC->CFGR0 &= (uint32_t)~RCC_FPRE;
      RCC->CFGR0 |= (uint32_t)RCC_FPRE_DIV4;

      /* Flash access, latency for HCLK/2 (vendor value at 400MHz). */
      FLASH->ACTLR = (FLASH->ACTLR & ~((uint32_t)0x3)) |
                     FLASH_ACTLR_LATENCY_HCLK_DIV2;
      /* The latency must be committed before the speed switch below. */
      CH32H417_DSB();

      /* Switch SYSCLK over to the PLL. */
      RCC->PLLCFGR |= (uint32_t)RCC_SYSPLL_GATE;
      RCC->CFGR0 &= (uint32_t)~RCC_SW;
      RCC->CFGR0 |= (uint32_t)RCC_SW_PLL;
      CH32H417_DSB();
      while ((RCC->CFGR0 & (uint32_t)RCC_SWS) != (uint32_t)0x08) {
      }
    }
    /* HSE start-up failure: stay on the reset clock (HSI), the same
       behavior as the vendor code, the assertion in hal_lld_init()
       flags it on debug builds. */
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

  osalDbgAssert(SystemClock == CH32H417_HCLK,
                "unexpected clock tree (HSE start-up failed?)");
}

/** @} */
