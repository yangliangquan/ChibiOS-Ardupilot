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

#define CSR_FCSR                        0x003
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
#define MSTATUS_FS_MASK                 (3UL << 13)
#define MSTATUS_FS_INITIAL              (1UL << 13)
#define MSTATUS_FS_CLEAN                (2UL << 13)
#define MSTATUS_FS_DIRTY                (3UL << 13)

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

/**
 * @brief   Encodes an IRQ priority for the PFIC IPRIOR register.
 * @details IPRIOR only uses bits [7:4], with PMTCFG=2 the two high bits of
 *          that nibble are the preemption level (0 = highest) and the low
 *          two bits are the sub priority. 0 is therefore the most urgent
 *          value and 15 the least.
 * @param[in] p         priority in the 0..15 range
 */
#define CH32_IRQ_PRIORITY(p)              (((p) & 15) << 4)

/*===========================================================================*/
/* Port configuration shared by C and assembler.                             */
/*===========================================================================*/

/**
 * @brief   Single-precision FPU support.
 * @note    GCC defines __riscv_flen to 32 when the F extension is enabled in
 *          -march. The port then saves/restores the FP register file.
 */
#if defined(__riscv_flen) && (__riscv_flen > 0)
#define PORT_RISCV_USE_FPU              1
#else
#define PORT_RISCV_USE_FPU              0
#endif

#if PORT_RISCV_USE_FPU
#if __riscv_flen != 32
#error "unsupported __riscv_flen, only single-precision F is supported"
#endif
#endif

/**
 * @brief   Number of interrupt preemption bits.
 * @note    Must match both CSR 0x804 PMTCFG and CSR 0xBC1 NEST_LVL written by
 *          crt0.S, and the per-thread interrupt stack reservation computed in
 *          chcore.h. Two bits give four preemption/nesting levels.
 */
#if !defined(PORT_RISCV_PREEMPT_BITS)
#define PORT_RISCV_PREEMPT_BITS         2
#endif

#define PORT_RISCV_NESTING_LEVELS       (1 << PORT_RISCV_PREEMPT_BITS)

/**
 * @brief   Interrupt context frame size.
 * @note    Base frame is 20 words; the FP variant adds 21 words plus 3 words
 *          of padding so the frame stays 16-byte aligned.
 * @note    These values are also consumed by the assembler, do not add C
 *          integer suffixes here.
 */
#define PORT_RISCV_EXTCTX_WORDS         20
#if PORT_RISCV_USE_FPU
#define PORT_RISCV_EXTCTX_FP_WORDS      24
#else
#define PORT_RISCV_EXTCTX_FP_WORDS      0
#endif
#define PORT_RISCV_EXTCTX_SIZE          \
  ((PORT_RISCV_EXTCTX_WORDS + PORT_RISCV_EXTCTX_FP_WORDS) * 4)

/**
 * @brief   Thread context frame size.
 * @note    Base frame is 16 words; the FP variant adds 12 callee-saved FP
 *          registers, fcsr and 3 words of padding (32 words total).
 */
#define PORT_RISCV_INTCTX_WORDS         16
#if PORT_RISCV_USE_FPU
#define PORT_RISCV_INTCTX_FP_WORDS      16
#else
#define PORT_RISCV_INTCTX_FP_WORDS      0
#endif
#define PORT_RISCV_INTCTX_SIZE          \
  ((PORT_RISCV_INTCTX_WORDS + PORT_RISCV_INTCTX_FP_WORDS) * 4)

/* Interrupt frame FP offsets (bytes): ft0-ft7, ft8-ft11, fa0-fa7, fcsr. */
#define PORT_RISCV_EXTCTX_FT0_OFF       (20 * 4)
#define PORT_RISCV_EXTCTX_FT8_OFF       (28 * 4)
#define PORT_RISCV_EXTCTX_FA0_OFF       (32 * 4)
#define PORT_RISCV_EXTCTX_FCSR_OFF      (40 * 4)

/* Thread frame FP offsets (bytes): fs0-fs11, fcsr. */
#define PORT_RISCV_INTCTX_FS0_OFF       (16 * 4)
#define PORT_RISCV_INTCTX_FCSR_OFF      (28 * 4)

/*===========================================================================*/
/* Assembler support.                                                        */
/*===========================================================================*/

#define REGBYTES                        4
#define FREGBYTES                       4

#if defined(_FROM_ASM_)

.macro LOAD reg, offset
    lw      \reg, \offset
.endm

.macro STORE reg, offset
    sw      \reg, \offset
.endm

.macro FLOAD reg, offset
    flw     \reg, \offset
.endm

.macro FSTORE reg, offset
    fsw     \reg, \offset
.endm

#endif /* !defined(_FROM_ASM_) */

#endif /* _RISCV_ENCODING_H_ */

/** @} */
