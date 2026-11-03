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
 * @file    board.h
 * @brief   Board specific definitions and declarations.
 *
 * @defgroup BOARD_CUSTOM_CH32H417
 * @{
 */

#ifndef _BOARD_H_
#define _BOARD_H_

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

#define BOARD_CUSTOM_CH32H417
#define BOARD_NAME                  "Custom CH32H417 dual-core board"

/* Board oscillators-related settings. */
#if !defined(CH32H417_HSECLK)
#define CH32H417_HSECLK             25000000U
#endif

/*
 * Ethernet PHY management settings (MAC driver).
 * The address is provisional: it skips the PHY scan and avoids a boot
 * halt on a missing/unknown PHY. Set it to the board wiring (0..31)
 * during the hardware verification round; alternatively delete it and
 * define BOARD_PHY_ID to enable the address scan.
 */
#if !defined(BOARD_PHY_ADDRESS)
#define BOARD_PHY_ADDRESS           1
#endif

/*
 * Board pins assignments.
 */

/**
 * @brief   Board LED, PD11, active low.
 */
#define GPIOD_LED11                 11U

/**
 * @brief   USART1 TX, PA9, alternate function 7.
 */
#define GPIOD_USART3_TX             8U
#define USART3_TX_AF                7U

/**
 * @brief   USART1 RX, PA10, alternate function 7.
 */
#define GPIOD_USART3_RX             9U
#define USART3_RX_AF                7U

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Turns on the board LED (active low).
 */
#define LED_ON()                                                        \
  palClearPad(GPIOD, GPIOD_LED11)

/**
 * @brief   Turns off the board LED (active low).
 */
#define LED_OFF()                                                       \
  palSetPad(GPIOD, GPIOD_LED11)

/**
 * @brief   Toggles the board LED.
 */
#define LED_TOGGLE()                                                    \
  palTogglePad(GPIOD, GPIOD_LED11)

#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif

#endif /* _BOARD_H_ */

/** @} */
