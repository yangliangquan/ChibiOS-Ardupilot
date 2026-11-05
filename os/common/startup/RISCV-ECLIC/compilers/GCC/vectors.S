/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.
    Vector table derived from WCH EVT startup_ch32h417_v5f.S,
    Copyright (c) 2025 Nanjing Qinheng Microelectronics Co., Ltd.

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
 * @file    vectors.S
 * @brief   Interrupt vectors and per-vector stubs for CH32H417 (QingKe V5F).
 *
 * @details The hardware dispatches interrupts and exceptions by fetching
 *          the absolute address at vector_base + id*4 (mtvec MODE0|MODE1).
 *          Each slot points to a stub that pushes the address of the
 *          handler on the stack and jumps to the common entry point, the
 *          whole save/restore sequence is shared by all the vectors so the
 *          code size does not grow with the number of interrupt sources.
 *
 * @addtogroup RISCV_ECLIC_STARTUP
 * @{
 */

#define _FROM_ASM_
#include "riscv_encoding.h"
#include "irq_stubs.h"

/*===========================================================================*/
/* Code section.                                                             */
/*===========================================================================*/

#if !defined(__DOXYGEN__)

.section .vectors, "ax", @progbits
/* mtvec[9:2] are hardwired to 0 on QingKe V5F: the table must be
   aligned on a 1024-byte boundary and fit id*4 within that block. */
.balign 1024
.globl _vector_base
.globl vector_base
_vector_base:
vector_base:
    .option norvc;
    .word   _start
    .word   _vec_default
    .word   _vec_NMI_Handler
    .word   _vec_HardFault_Handler
    .word   _vec_default
    .word   _vec_Ecall_M_Mode_Handler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_Ecall_U_Mode_Handler
    .word   _vec_Break_Point_Handler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_SysTick0_Handler
    .word   _vec_SysTick1_Handler
    .word   _vec_SW_Handler
    .word   _vec_default
    .word   _vec_IPC_CH0_Handler
    .word   _vec_IPC_CH1_Handler
    .word   _vec_IPC_CH2_Handler
    .word   _vec_IPC_CH3_Handler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    .word   _vec_HSEM_Handler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_default
    /* External Interrupts */
    .word   _vec_WWDG_IRQHandler
    .word   _vec_EXTI15_8_IRQHandler
    .word   _vec_FLASH_IRQHandler
    .word   _vec_RCC_IRQHandler
    .word   _vec_EXTI7_0_IRQHandler
    .word   _vec_SPI1_IRQHandler
    .word   _vec_DMA1_Channel2_IRQHandler
    .word   _vec_DMA1_Channel3_IRQHandler
    .word   _vec_DMA1_Channel4_IRQHandler
    .word   _vec_DMA1_Channel5_IRQHandler
    .word   _vec_DMA1_Channel6_IRQHandler
    .word   _vec_DMA1_Channel7_IRQHandler
    .word   _vec_DMA1_Channel8_IRQHandler
    .word   _vec_USART2_IRQHandler
    .word   _vec_I2C1_EV_IRQHandler
    .word   _vec_I2C1_ER_IRQHandler
    .word   _vec_USART1_IRQHandler
    .word   _vec_SPI2_IRQHandler
    .word   _vec_SPI3_IRQHandler
    .word   _vec_SPI4_IRQHandler
    .word   _vec_I2C2_EV_IRQHandler
    .word   _vec_I2C2_ER_IRQHandler
    .word   _vec_USBPD_IRQHandler
    .word   _vec_USBPDWakeUp_IRQHandler
    .word   _vec_USBHS_IRQHandler
    .word   _vec_DMA1_Channel1_IRQHandler
    .word   _vec_CAN1_SCE_IRQHandler
    .word   _vec_CAN1_TX_IRQHandler
    .word   _vec_CAN1_RX0_IRQHandler
    .word   _vec_CAN1_RX1_IRQHandler
    .word   _vec_USBSS_IRQHandler
    .word   _vec_USBSS_LINK_IRQHandler
    .word   _vec_USBHSWakeup_IRQHandler
    .word   _vec_USBSSWakeup_IRQHandler
    .word   _vec_RTCAlarm_IRQHandler
    .word   _vec_USBFS_IRQHandler
    .word   _vec_USBFSWakeup_IRQHandler
    .word   _vec_ADC1_2_IRQHandler
    .word   _vec_TIM1_BRK_IRQHandler
    .word   _vec_TIM1_UP_IRQHandler
    .word   _vec_TIM1_TRG_COM_IRQHandler
    .word   _vec_TIM1_CC_IRQHandler
    .word   _vec_TIM2_IRQHandler
    .word   _vec_TIM3_IRQHandler
    .word   _vec_TIM4_IRQHandler
    .word   _vec_TIM5_IRQHandler
    .word   _vec_I2C3_EV_IRQHandler
    .word   _vec_I2C3_ER_IRQHandler
    .word   _vec_I2C4_EV_IRQHandler
    .word   _vec_I2C4_ER_IRQHandler
    .word   _vec_QSPI1_IRQHandler
    .word   _vec_SERDES_IRQHandler
    .word   _vec_USART3_IRQHandler
    .word   _vec_USART4_IRQHandler
    .word   _vec_TIM8_BRK_IRQHandler
    .word   _vec_TIM8_UP_IRQHandler
    .word   _vec_TIM8_TRG_COM_IRQHandler
    .word   _vec_TIM8_CC_IRQHandler
    .word   _vec_TIM9_IRQHandler
    .word   _vec_TIM10_IRQHandler
    .word   _vec_TIM11_IRQHandler
    .word   _vec_TIM12_IRQHandler
    .word   _vec_FMC_IRQHandler
    .word   _vec_SDMMC_IRQHandler
    .word   _vec_LPTIM1_IRQHandler
    .word   _vec_LPTIM2_IRQHandler
    .word   _vec_USART5_IRQHandler
    .word   _vec_USART6_IRQHandler
    .word   _vec_TIM6_IRQHandler
    .word   _vec_TIM7_IRQHandler
    .word   _vec_DMA2_Channel1_IRQHandler
    .word   _vec_DMA2_Channel2_IRQHandler
    .word   _vec_DMA2_Channel3_IRQHandler
    .word   _vec_DMA2_Channel4_IRQHandler
    .word   _vec_DMA2_Channel5_IRQHandler
    .word   _vec_DMA2_Channel6_IRQHandler
    .word   _vec_DMA2_Channel7_IRQHandler
    .word   _vec_DMA2_Channel8_IRQHandler
    .word   _vec_ETH_IRQHandler
    .word   _vec_ETH_WKUP_IRQHandler
    .word   _vec_CAN2_SCE_IRQHandler
    .word   _vec_CAN2_TX_IRQHandler
    .word   _vec_CAN2_RX0_IRQHandler
    .word   _vec_CAN2_RX1_IRQHandler
    .word   _vec_USART7_IRQHandler
    .word   _vec_USART8_IRQHandler
    .word   _vec_I3C_EV_IRQHandler
    .word   _vec_I3C_ER_IRQHandler
    .word   _vec_DVP_IRQHandler
    .word   _vec_ECDC_IRQHandler
    .word   _vec_PIOC_IRQHandler
    .word   _vec_SAI_IRQHandler
    .word   _vec_LTDC_IRQHandler
    .word   _vec_GPHA_IRQHandler
    .word   _vec_default
    .word   _vec_DFSDM0_IRQHandler
    .word   _vec_DFSDM1_IRQHandler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_SWPMI_IRQHandler
    .word   _vec_default
    .word   _vec_default
    .word   _vec_QSPI2_IRQHandler
    .word   _vec_SWPMI_WKUP_IRQHandler
    .word   _vec_CAN3_SCE_IRQHandler
    .word   _vec_CAN3_TX_IRQHandler
    .word   _vec_CAN3_RX0_IRQHandler
    .word   _vec_CAN3_RX1_IRQHandler
    .word   _vec_LPTIM2_WKUP_IRQHandler
    .word   _vec_LPTIM1_WKUP_IRQHandler
    .word   _vec_I3C_WKUP_IRQHandler
    .word   _vec_RTC_IRQHandler
    .word   _vec_HSADC_IRQHandler
    .word   _vec_UHSIF_IRQHandler
    .word   _vec_RNG_IRQHandler
    .word   _vec_SDIO_IRQHandler
    .word   _vec_USART_WKUP_IRQHandler
    .option rvc;

/*===========================================================================*/
/* Per-vector stubs.                                                         */
/*===========================================================================*/

.section .text, "ax", @progbits

/* Reserved vector slots: safe halt. */
_vec_default:
    csrc    CSR_MSTATUS, MSTATUS_MIE
1:
    j       1b

/* Core exceptions and system interrupts.
   Exceptions never allow nesting, the external and the core timer
   interrupts do. */
.macro VECN name
.align  2
.globl _vec_\name
.type _vec_\name,@function
_vec_\name:
    PORT_IRQ_STUB_PROLOGUE \name
    j       _port_irq_entry_nonest
.endm

.macro VEC name
.align  2
.globl _vec_\name
.type _vec_\name,@function
_vec_\name:
    PORT_IRQ_STUB_PROLOGUE \name
    j       _port_irq_entry
.endm

VECN    NMI_Handler
VECN    HardFault_Handler
VECN    Ecall_M_Mode_Handler
VECN    Ecall_U_Mode_Handler
VECN    Break_Point_Handler
VEC     SysTick0_Handler
VEC     SysTick1_Handler
VEC     SW_Handler
VEC     IPC_CH0_Handler
VEC     IPC_CH1_Handler
VEC     IPC_CH2_Handler
VEC     IPC_CH3_Handler
VEC     HSEM_Handler

/* External interrupts. */
VEC     WWDG_IRQHandler
VEC     EXTI15_8_IRQHandler
VEC     FLASH_IRQHandler
VEC     RCC_IRQHandler
VEC     EXTI7_0_IRQHandler
VEC     SPI1_IRQHandler
VEC     DMA1_Channel2_IRQHandler
VEC     DMA1_Channel3_IRQHandler
VEC     DMA1_Channel4_IRQHandler
VEC     DMA1_Channel5_IRQHandler
VEC     DMA1_Channel6_IRQHandler
VEC     DMA1_Channel7_IRQHandler
VEC     DMA1_Channel8_IRQHandler
VEC     USART2_IRQHandler
VEC     I2C1_EV_IRQHandler
VEC     I2C1_ER_IRQHandler
VEC     USART1_IRQHandler
VEC     SPI2_IRQHandler
VEC     SPI3_IRQHandler
VEC     SPI4_IRQHandler
VEC     I2C2_EV_IRQHandler
VEC     I2C2_ER_IRQHandler
VEC     USBPD_IRQHandler
VEC     USBPDWakeUp_IRQHandler
VEC     USBHS_IRQHandler
VEC     DMA1_Channel1_IRQHandler
VEC     CAN1_SCE_IRQHandler
VEC     CAN1_TX_IRQHandler
VEC     CAN1_RX0_IRQHandler
VEC     CAN1_RX1_IRQHandler
VEC     USBSS_IRQHandler
VEC     USBSS_LINK_IRQHandler
VEC     USBHSWakeup_IRQHandler
VEC     USBSSWakeup_IRQHandler
VEC     RTCAlarm_IRQHandler
VEC     USBFS_IRQHandler
VEC     USBFSWakeup_IRQHandler
VEC     ADC1_2_IRQHandler
VEC     TIM1_BRK_IRQHandler
VEC     TIM1_UP_IRQHandler
VEC     TIM1_TRG_COM_IRQHandler
VEC     TIM1_CC_IRQHandler
VEC     TIM2_IRQHandler
VEC     TIM3_IRQHandler
VEC     TIM4_IRQHandler
VEC     TIM5_IRQHandler
VEC     I2C3_EV_IRQHandler
VEC     I2C3_ER_IRQHandler
VEC     I2C4_EV_IRQHandler
VEC     I2C4_ER_IRQHandler
VEC     QSPI1_IRQHandler
VEC     SERDES_IRQHandler
VEC     USART3_IRQHandler
VEC     USART4_IRQHandler
VEC     TIM8_BRK_IRQHandler
VEC     TIM8_UP_IRQHandler
VEC     TIM8_TRG_COM_IRQHandler
VEC     TIM8_CC_IRQHandler
VEC     TIM9_IRQHandler
VEC     TIM10_IRQHandler
VEC     TIM11_IRQHandler
VEC     TIM12_IRQHandler
VEC     FMC_IRQHandler
VEC     SDMMC_IRQHandler
VEC     LPTIM1_IRQHandler
VEC     LPTIM2_IRQHandler
VEC     USART5_IRQHandler
VEC     USART6_IRQHandler
VEC     TIM6_IRQHandler
VEC     TIM7_IRQHandler
VEC     DMA2_Channel1_IRQHandler
VEC     DMA2_Channel2_IRQHandler
VEC     DMA2_Channel3_IRQHandler
VEC     DMA2_Channel4_IRQHandler
VEC     DMA2_Channel5_IRQHandler
VEC     DMA2_Channel6_IRQHandler
VEC     DMA2_Channel7_IRQHandler
VEC     DMA2_Channel8_IRQHandler
VEC     ETH_IRQHandler
VEC     ETH_WKUP_IRQHandler
VEC     CAN2_SCE_IRQHandler
VEC     CAN2_TX_IRQHandler
VEC     CAN2_RX0_IRQHandler
VEC     CAN2_RX1_IRQHandler
VEC     USART7_IRQHandler
VEC     USART8_IRQHandler
VEC     I3C_EV_IRQHandler
VEC     I3C_ER_IRQHandler
VEC     DVP_IRQHandler
VEC     ECDC_IRQHandler
VEC     PIOC_IRQHandler
VEC     SAI_IRQHandler
VEC     LTDC_IRQHandler
VEC     GPHA_IRQHandler
VEC     DFSDM0_IRQHandler
VEC     DFSDM1_IRQHandler
VEC     SWPMI_IRQHandler
VEC     QSPI2_IRQHandler
VEC     SWPMI_WKUP_IRQHandler
VEC     CAN3_SCE_IRQHandler
VEC     CAN3_TX_IRQHandler
VEC     CAN3_RX0_IRQHandler
VEC     CAN3_RX1_IRQHandler
VEC     LPTIM2_WKUP_IRQHandler
VEC     LPTIM1_WKUP_IRQHandler
VEC     I3C_WKUP_IRQHandler
VEC     RTC_IRQHandler
VEC     HSADC_IRQHandler
VEC     UHSIF_IRQHandler
VEC     RNG_IRQHandler
VEC     SDIO_IRQHandler
VEC     USART_WKUP_IRQHandler

/*===========================================================================*/
/* Weak default handlers (halt).                                             */
/*===========================================================================*/

.section .text.vector_handler, "ax", @progbits
    .weak   NMI_Handler
    .weak   HardFault_Handler
    .weak   Ecall_M_Mode_Handler
    .weak   Ecall_U_Mode_Handler
    .weak   Break_Point_Handler
    .weak   SysTick0_Handler
    .weak   SysTick1_Handler
    .weak   SW_Handler
    .weak   IPC_CH0_Handler
    .weak   IPC_CH1_Handler
    .weak   IPC_CH2_Handler
    .weak   IPC_CH3_Handler
    .weak   HSEM_Handler
    .weak   WWDG_IRQHandler
    .weak   EXTI15_8_IRQHandler
    .weak   FLASH_IRQHandler
    .weak   RCC_IRQHandler
    .weak   EXTI7_0_IRQHandler
    .weak   SPI1_IRQHandler
    .weak   DMA1_Channel2_IRQHandler
    .weak   DMA1_Channel3_IRQHandler
    .weak   DMA1_Channel4_IRQHandler
    .weak   DMA1_Channel5_IRQHandler
    .weak   DMA1_Channel6_IRQHandler
    .weak   DMA1_Channel7_IRQHandler
    .weak   DMA1_Channel8_IRQHandler
    .weak   USART2_IRQHandler
    .weak   I2C1_EV_IRQHandler
    .weak   I2C1_ER_IRQHandler
    .weak   USART1_IRQHandler
    .weak   SPI2_IRQHandler
    .weak   SPI3_IRQHandler
    .weak   SPI4_IRQHandler
    .weak   I2C2_EV_IRQHandler
    .weak   I2C2_ER_IRQHandler
    .weak   USBPD_IRQHandler
    .weak   USBPDWakeUp_IRQHandler
    .weak   USBHS_IRQHandler
    .weak   DMA1_Channel1_IRQHandler
    .weak   CAN1_SCE_IRQHandler
    .weak   CAN1_TX_IRQHandler
    .weak   CAN1_RX0_IRQHandler
    .weak   CAN1_RX1_IRQHandler
    .weak   USBSS_IRQHandler
    .weak   USBSS_LINK_IRQHandler
    .weak   USBHSWakeup_IRQHandler
    .weak   USBSSWakeup_IRQHandler
    .weak   RTCAlarm_IRQHandler
    .weak   USBFS_IRQHandler
    .weak   USBFSWakeup_IRQHandler
    .weak   ADC1_2_IRQHandler
    .weak   TIM1_BRK_IRQHandler
    .weak   TIM1_UP_IRQHandler
    .weak   TIM1_TRG_COM_IRQHandler
    .weak   TIM1_CC_IRQHandler
    .weak   TIM2_IRQHandler
    .weak   TIM3_IRQHandler
    .weak   TIM4_IRQHandler
    .weak   TIM5_IRQHandler
    .weak   I2C3_EV_IRQHandler
    .weak   I2C3_ER_IRQHandler
    .weak   I2C4_EV_IRQHandler
    .weak   I2C4_ER_IRQHandler
    .weak   QSPI1_IRQHandler
    .weak   SERDES_IRQHandler
    .weak   USART3_IRQHandler
    .weak   USART4_IRQHandler
    .weak   TIM8_BRK_IRQHandler
    .weak   TIM8_UP_IRQHandler
    .weak   TIM8_TRG_COM_IRQHandler
    .weak   TIM8_CC_IRQHandler
    .weak   TIM9_IRQHandler
    .weak   TIM10_IRQHandler
    .weak   TIM11_IRQHandler
    .weak   TIM12_IRQHandler
    .weak   FMC_IRQHandler
    .weak   SDMMC_IRQHandler
    .weak   LPTIM1_IRQHandler
    .weak   LPTIM2_IRQHandler
    .weak   USART5_IRQHandler
    .weak   USART6_IRQHandler
    .weak   TIM6_IRQHandler
    .weak   TIM7_IRQHandler
    .weak   DMA2_Channel1_IRQHandler
    .weak   DMA2_Channel2_IRQHandler
    .weak   DMA2_Channel3_IRQHandler
    .weak   DMA2_Channel4_IRQHandler
    .weak   DMA2_Channel5_IRQHandler
    .weak   DMA2_Channel6_IRQHandler
    .weak   DMA2_Channel7_IRQHandler
    .weak   DMA2_Channel8_IRQHandler
    .weak   ETH_IRQHandler
    .weak   ETH_WKUP_IRQHandler
    .weak   CAN2_SCE_IRQHandler
    .weak   CAN2_TX_IRQHandler
    .weak   CAN2_RX0_IRQHandler
    .weak   CAN2_RX1_IRQHandler
    .weak   USART7_IRQHandler
    .weak   USART8_IRQHandler
    .weak   I3C_EV_IRQHandler
    .weak   I3C_ER_IRQHandler
    .weak   DVP_IRQHandler
    .weak   ECDC_IRQHandler
    .weak   PIOC_IRQHandler
    .weak   SAI_IRQHandler
    .weak   LTDC_IRQHandler
    .weak   GPHA_IRQHandler
    .weak   DFSDM0_IRQHandler
    .weak   DFSDM1_IRQHandler
    .weak   SWPMI_IRQHandler
    .weak   QSPI2_IRQHandler
    .weak   SWPMI_WKUP_IRQHandler
    .weak   CAN3_SCE_IRQHandler
    .weak   CAN3_TX_IRQHandler
    .weak   CAN3_RX0_IRQHandler
    .weak   CAN3_RX1_IRQHandler
    .weak   LPTIM2_WKUP_IRQHandler
    .weak   LPTIM1_WKUP_IRQHandler
    .weak   I3C_WKUP_IRQHandler
    .weak   RTC_IRQHandler
    .weak   HSADC_IRQHandler
    .weak   UHSIF_IRQHandler
    .weak   RNG_IRQHandler
    .weak   SDIO_IRQHandler
    .weak   USART_WKUP_IRQHandler

NMI_Handler:
HardFault_Handler:
Ecall_M_Mode_Handler:
Ecall_U_Mode_Handler:
Break_Point_Handler:
SysTick0_Handler:
SysTick1_Handler:
SW_Handler:
IPC_CH0_Handler:
IPC_CH1_Handler:
IPC_CH2_Handler:
IPC_CH3_Handler:
HSEM_Handler:
WWDG_IRQHandler:
EXTI15_8_IRQHandler:
FLASH_IRQHandler:
RCC_IRQHandler:
EXTI7_0_IRQHandler:
SPI1_IRQHandler:
DMA1_Channel2_IRQHandler:
DMA1_Channel3_IRQHandler:
DMA1_Channel4_IRQHandler:
DMA1_Channel5_IRQHandler:
DMA1_Channel6_IRQHandler:
DMA1_Channel7_IRQHandler:
DMA1_Channel8_IRQHandler:
USART2_IRQHandler:
I2C1_EV_IRQHandler:
I2C1_ER_IRQHandler:
USART1_IRQHandler:
SPI2_IRQHandler:
SPI3_IRQHandler:
SPI4_IRQHandler:
I2C2_EV_IRQHandler:
I2C2_ER_IRQHandler:
USBPD_IRQHandler:
USBPDWakeUp_IRQHandler:
USBHS_IRQHandler:
DMA1_Channel1_IRQHandler:
CAN1_SCE_IRQHandler:
CAN1_TX_IRQHandler:
CAN1_RX0_IRQHandler:
CAN1_RX1_IRQHandler:
USBSS_IRQHandler:
USBSS_LINK_IRQHandler:
USBHSWakeup_IRQHandler:
USBSSWakeup_IRQHandler:
RTCAlarm_IRQHandler:
USBFS_IRQHandler:
USBFSWakeup_IRQHandler:
ADC1_2_IRQHandler:
TIM1_BRK_IRQHandler:
TIM1_UP_IRQHandler:
TIM1_TRG_COM_IRQHandler:
TIM1_CC_IRQHandler:
TIM2_IRQHandler:
TIM3_IRQHandler:
TIM4_IRQHandler:
TIM5_IRQHandler:
I2C3_EV_IRQHandler:
I2C3_ER_IRQHandler:
I2C4_EV_IRQHandler:
I2C4_ER_IRQHandler:
QSPI1_IRQHandler:
SERDES_IRQHandler:
USART3_IRQHandler:
USART4_IRQHandler:
TIM8_BRK_IRQHandler:
TIM8_UP_IRQHandler:
TIM8_TRG_COM_IRQHandler:
TIM8_CC_IRQHandler:
TIM9_IRQHandler:
TIM10_IRQHandler:
TIM11_IRQHandler:
TIM12_IRQHandler:
FMC_IRQHandler:
SDMMC_IRQHandler:
LPTIM1_IRQHandler:
LPTIM2_IRQHandler:
USART5_IRQHandler:
USART6_IRQHandler:
TIM6_IRQHandler:
TIM7_IRQHandler:
DMA2_Channel1_IRQHandler:
DMA2_Channel2_IRQHandler:
DMA2_Channel3_IRQHandler:
DMA2_Channel4_IRQHandler:
DMA2_Channel5_IRQHandler:
DMA2_Channel6_IRQHandler:
DMA2_Channel7_IRQHandler:
DMA2_Channel8_IRQHandler:
ETH_IRQHandler:
ETH_WKUP_IRQHandler:
CAN2_SCE_IRQHandler:
CAN2_TX_IRQHandler:
CAN2_RX0_IRQHandler:
CAN2_RX1_IRQHandler:
USART7_IRQHandler:
USART8_IRQHandler:
I3C_EV_IRQHandler:
I3C_ER_IRQHandler:
DVP_IRQHandler:
ECDC_IRQHandler:
PIOC_IRQHandler:
SAI_IRQHandler:
LTDC_IRQHandler:
GPHA_IRQHandler:
DFSDM0_IRQHandler:
DFSDM1_IRQHandler:
SWPMI_IRQHandler:
QSPI2_IRQHandler:
SWPMI_WKUP_IRQHandler:
CAN3_SCE_IRQHandler:
CAN3_TX_IRQHandler:
CAN3_RX0_IRQHandler:
CAN3_RX1_IRQHandler:
LPTIM2_WKUP_IRQHandler:
LPTIM1_WKUP_IRQHandler:
I3C_WKUP_IRQHandler:
RTC_IRQHandler:
HSADC_IRQHandler:
UHSIF_IRQHandler:
RNG_IRQHandler:
SDIO_IRQHandler:
USART_WKUP_IRQHandler:
1:
    j       1b

#endif /* !defined(__DOXYGEN__) */

/** @} */
