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

/*
 * HAL configuration for the CH32H417 V5F kernel check demo.
 */

#ifndef CH32H417_MCUCONF
#define CH32H417_MCUCONF
#endif

/*
 * USART1 on PA9/PA10, 115200 8N1 console. All eight units enabled.
 */
#define CH32H417_SERIAL_USE_USART1             TRUE
#define CH32H417_SERIAL_USE_USART2             TRUE
#define CH32H417_SERIAL_USE_USART3             TRUE
#define CH32H417_SERIAL_USE_USART4             TRUE
#define CH32H417_SERIAL_USE_USART5             TRUE
#define CH32H417_SERIAL_USE_USART6             TRUE
#define CH32H417_SERIAL_USE_USART7             TRUE
#define CH32H417_SERIAL_USE_USART8             TRUE

/*
 * The SIO and UART drivers share the USART units with the SERIAL driver,
 * a unit can only be assigned to one of the three at a time (the LLD
 * headers fail the build otherwise). Every unit is taken by the SERIAL
 * console above, so both drivers are disabled here; to exercise them set
 * the matching CH32H417_SERIAL_USE_USARTx back to FALSE, enable the unit
 * below and enable HAL_USE_SIO / HAL_USE_UART in halconf.h.
 */
#define CH32H417_SIO_USE_USART1                FALSE
#define CH32H417_SIO_USE_USART2                FALSE
#define CH32H417_SIO_USE_USART3                FALSE
#define CH32H417_SIO_USE_USART4                FALSE
#define CH32H417_SIO_USE_USART5                FALSE
#define CH32H417_SIO_USE_USART6                FALSE
#define CH32H417_SIO_USE_USART7                FALSE
#define CH32H417_SIO_USE_USART8                FALSE

#define CH32H417_UART_USE_USART1               FALSE
#define CH32H417_UART_USE_USART2               FALSE
#define CH32H417_UART_USE_USART3               FALSE
#define CH32H417_UART_USE_USART4               FALSE
#define CH32H417_UART_USE_USART5               FALSE
#define CH32H417_UART_USE_USART6               FALSE
#define CH32H417_UART_USE_USART7               FALSE
#define CH32H417_UART_USE_USART8               FALSE

/*
 * Unit assignments.
 */
#define CH32H417_WDG_USE_IWDG                  TRUE
#define CH32H417_EFL_USE_EFL1                  TRUE
#define CH32H417_GPT_USE_TIM7                  TRUE
#define CH32H417_GPT_USE_TIM8                  TRUE
#define CH32H417_ICU_USE_TIM3                  TRUE
#define CH32H417_PWM_USE_TIM1                  TRUE
#define CH32H417_PWM_USE_TIM4                  TRUE
#define CH32H417_SPI_USE_SPI1                  TRUE
#define CH32H417_SPI_USE_SPI2                  FALSE
#define CH32H417_SPI_USE_SPI3                  FALSE
#define CH32H417_SPI_USE_SPI4                  TRUE
/*
 * No DMA channel has to be assigned: the DMAMUX routing is fully flexible,
 * every driver takes a free channel at start and routes its own request onto
 * it. Only the interrupt priorities have to be configured.
 *
 * SPI2/SPI3 host I2S instead of the SPI driver (the IRQ vector of a
 * unit is shared between the two drivers).
 */
#define CH32H417_I2S_USE_SPI2                  TRUE
#define CH32H417_I2S_USE_SPI3                  TRUE
#define CH32H417_I2C_USE_I2C1                  TRUE
#define CH32H417_I2C_USE_I2C2                  TRUE
#define CH32H417_I2C_USE_I2C3                  TRUE
#define CH32H417_I2C_USE_I2C4                  TRUE
#define CH32H417_ADC_USE_ADC1                  TRUE
#define CH32H417_ADC_USE_ADC2                  TRUE
#define CH32H417_RTC_USE_RTC1                  TRUE
#define CH32H417_CAN_USE_CAN1                  TRUE
#define CH32H417_CAN_USE_CAN2                  TRUE
#define CH32H417_CAN_USE_CAN3                  TRUE
#define CH32H417_DAC_USE_DAC1                  TRUE
#define CH32H417_TRNG_USE_RNG                  TRUE
#define CH32H417_WDG_USE_WWDG                  TRUE
/*
 * SDC backend: the slot is wired to the SDIO controller, the pin set
 * (PB10/PB11, PE8/PE9, PB3/PB4) is not reachable from the SDMMC
 * controller. SDMMC is kept for reference.
 */
#define CH32H417_SDC_USE_SDIO                  TRUE
#define CH32H417_SDC_USE_SDMMC                 FALSE
/*
 * QSPI XIP (memory mapped) windows, confirmed:
 *   QSPI1: 0x90000000, QSPI2: 0x70000000
 * (defaults in LLD/QSPIv1/hal_wspi_lld.h match these values).
 */
#define CH32H417_WSPI_USE_QSPI1                TRUE
#define CH32H417_WSPI_USE_QSPI2                TRUE
#define CH32H417_CRY_USE_ECDC                  TRUE
#define CH32H417_USB_USE_USBFS                 TRUE

#if !defined(HAL_XIP_ENABLED)
#define HAL_XIP_ENABLED                        FALSE
#endif