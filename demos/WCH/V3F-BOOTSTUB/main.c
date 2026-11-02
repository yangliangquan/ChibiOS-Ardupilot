/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.
    Derived from WCH EVT main (V3F), Copyright (c) 2025 Nanjing Qinheng
    Microelectronics Co., Ltd.

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
 * @file    main.c
 * @brief   CH32H417 V3F minimal boot stub.
 *
 * @details The V3F is the boot master: it comes out of reset first,
 *          configures the clock tree (WCH EVT default: SYSCLK 400MHz,
 *          V5F core = 400MHz), wakes the V5F core at its flash start
 *          address 0x00010000 and goes to sleep. The V5F then runs
 *          the ChibiOS image.
 *
 * @addtogroup V3F-BOOTSTUB
 * @{
 */

#include "ch32h417.h"

/**
 * @brief   Clock tree setup (from the WCH EVT V3F system file).
 */
extern void SystemInit(void);

/*===========================================================================*/
/* Module main code.                                                         */
/*===========================================================================*/

int main(void) {

  /* Configure the clock tree first: the V5F core clock is derived
     from the same PLL, so it must be stable before the V5F starts. */
  SystemInit();

  /* Start the V5F core at its flash image address. */
  NVIC_WakeUp_V5F(0x00010000UL);

  /* V3F idles forever. */
  for (;;) {
    __WFI();
  }
}

/** @} */
