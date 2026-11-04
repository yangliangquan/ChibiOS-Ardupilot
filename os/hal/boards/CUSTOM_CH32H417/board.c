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
 * @file    board.c
 * @brief   Board specific code for the custom CH32H417 board.
 *
 * @addtogroup BOARD_CUSTOM_CH32H417
 * @{
 */

#include "hal.h"
#include "board.h"

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Board-specific early initialization.
 * @note    Called from the startup code (@p crt1.c) before the HAL
 *          and the kernel are initialized, GPIO clocks are enabled
 *          here because the pad configuration runs this early.
 */
void __early_init(void) {

  /* Clock tree first (STM32-style __early_init hook): idempotent,
     normally a no-op because the V3F boot stub configured the tree
     before waking this core. Runs before .data/.bss initialization. */
  ch32h417_clock_init();

  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                        RCC_HB2Periph_GPIOA |
                        RCC_HB2Periph_GPIOB |
                        RCC_HB2Periph_GPIOC |
                        RCC_HB2Periph_GPIOD |
                        RCC_HB2Periph_GPIOE |
                        RCC_HB2Periph_GPIOF,
                        ENABLE);

  /* LED, active low. */
  palSetPadMode(GPIOD, GPIOD_LED11, PAL_MODE_OUTPUT_PUSHPULL);
  LED_OFF();

  /* USART3 pins. */
  palSetPadMode(GPIOD, GPIOD_USART3_TX, PAL_MODE_ALTERNATE(USART3_TX_AF));
  palSetPadMode(GPIOD, GPIOD_USART3_RX, PAL_MODE_ALTERNATE_INPUT(USART3_RX_AF));
}

/**
 * @brief   Board-specific initialization.
 * @note    Called from @p halInit() after the HAL drivers.
 */
void boardInit(void) {
}

/** @} */
