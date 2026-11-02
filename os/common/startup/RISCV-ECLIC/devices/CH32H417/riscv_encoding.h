/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.
    Derived from QingKe V5 processor manual, Copyright (c) WCH.

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
 * @file    riscv_encoding.h
 * @brief   QingKe V5F (CH32H417) CSR and stack-frame definitions.
 *
 * @addtogroup RISCV_ECLIC_STARTUP
 * @{
 */

#ifndef _RISCV_ENCODING_H_
#define _RISCV_ENCODING_H_

/*===========================================================================*/
/* Standard RISC-V CSRs.                                                     */
/*===========================================================================*/

#define CSR_MSTATUS                     0x300
#define CSR_MSTATUSH                    0x310
#define CSR_MIE_NEST                    0xBC8
#define CSR_MSCRATCH                    0x340
#define CSR_MEPC                        0x341
#define CSR_MCAUSE                      0x342
#define CSR_MTVAL                       0x343
#define CSR_MTVEC                       0x305
#define CSR_MCOUNTINHIBIT               0x320
#define CSR_MCYCLE                      0xB00
#define CSR_MINSTRET                    0xB02
#define CSR_CYCLE                       0xC00
#define CSR_INSTRET                     0xC02

#define MSTATUS_SIE                     (1UL << 1)
#define MSTATUS_MIE                     (1UL << 3)
#define MSTATUS_SPIE                    (1UL << 5)
#define MSTATUS_MPIE                    (1UL << 7)
#define MSTATUS_SPP                     (1UL << 8)
#define MSTATUS_MPP                     (3UL << 11)
#define MSTATUS_FS                      (3UL << 13)

/*===========================================================================*/
/* QingKe V5 custom CSRs.                                                    */
/*===========================================================================*/

/* Global interrupt enable, mirror of mstatus.MIE/MPIE when corecfgr.IE_REMAP_EN. */
#define CSR_GINTENR                     0x800
#define GINTENR_MIE                     (1UL << 3)
#define GINTENR_MPIE                    (1UL << 7)

/* Interrupt system control. */
#define CSR_INTSYSCR                    0x804
#define INTSYSCR_HWSTKEN                (1UL << 0)
#define INTSYSCR_INESTEN                (1UL << 1)
#define INTSYSCR_PMTCFG_OFF             2
#define INTSYSCR_GIHWSTKNEN             (1UL << 5)
#define INTSYSCR_LOCK                   (1UL << 31)

/* Core configuration: prefetch, branch prediction, IE remap. */
#define CSR_CORECFGR                    0xBC0
#define CORECFGR_IE_REMAP_EN            (1UL << 5)
#define CORECFGR_CSTA_FAULT_IE          (1UL << 7)

/* Interrupt nesting control/status. */
#define CSR_INESTCR                     0xBC1
#define INESTCR_NEST_STA_OFF            8
#define INESTCR_NEST_LVL_MASK           0x7UL
#define INESTCR_NEST_OV                 (1UL << 30)

/* Hardware interrupt stack address (DTCM block, 512-bit aligned). */
#define CSR_HW_POPDM_ADDR               0xBC4

/* Vector table base: mtvec MODE0|MODE1, entries are absolute addresses. */
#define MTVEC_MODE_ABSOLUTE             3UL

/* PFIC memory mapped registers (used via PFIC/core_riscv.h definitions). */
#define PFIC_GISR_NESTSTA_MASK          0xFFUL

/*===========================================================================*/
/* Assembler support.                                                        */
/*===========================================================================*/

#define REGBYTES                        4

#if defined(_FROM_ASM_)

.macro LOAD reg, offset
    lw      \reg, \offset
.endm

.macro STORE reg, offset
    sw      \reg, \offset
.endm

#endif /* !defined(_FROM_ASM_) */

#endif /* _RISCV_ENCODING_H_ */

/** @} */
