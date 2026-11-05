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
 * @brief   Number of interrupt priority levels exposed by the PFIC.
 * @note    With PMTCFG=2 the IPRIOR register encodes four bits, two of them
 *          are the preemption level and the remaining two the sub priority,
 *          so the interrupt priorities usable by the drivers are 0..15.
 */
#define PORT_RISCV_IRQ_PRIORITY_LEVELS  16

/**
 * @brief   Lowest (least urgent) interrupt priority.
 */
#define PORT_RISCV_LOWEST_IRQ_PRIORITY  15

/**
 * @brief   Highest (most urgent) interrupt priority.
 */
#define PORT_RISCV_HIGHEST_IRQ_PRIORITY 0

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
/* Interrupt frame layout (struct port_extctx).                              */
/*===========================================================================*/

/**
 * @brief   Size, in bytes, of the argument area pushed by the vector stubs.
 * @details The stubs cannot pass the address of the handler in a register
 *          because the interrupted context has not been saved yet, so the
 *          address is pushed on the stack together with the value of the
 *          register used to compute it. The area is a multiple of 16 bytes
 *          so that the interrupt frame stays aligned.
 */
#define PORT_RISCV_IRQ_ARG_SIZE         16

/**
 * @brief   Offset of the handler address inside the argument area.
 */
#define PORT_RISCV_IRQ_ARG_HANDLER_OFF  4

/**
 * @brief   Offsets, in bytes, of the fields of the interrupt frame.
 * @note    The layout is shared between the C structure and the assembler
 *          macros, it must be kept in sync with @p port_extctx.
 */
#define PORT_RISCV_EXTCTX_RA_OFF        0
#define PORT_RISCV_EXTCTX_TP_OFF        4
#define PORT_RISCV_EXTCTX_T0_OFF        8
#define PORT_RISCV_EXTCTX_T1_OFF        12
#define PORT_RISCV_EXTCTX_T2_OFF        16
#define PORT_RISCV_EXTCTX_A0_OFF        20
#define PORT_RISCV_EXTCTX_A1_OFF        24
#define PORT_RISCV_EXTCTX_A2_OFF        28
#define PORT_RISCV_EXTCTX_A3_OFF        32
#define PORT_RISCV_EXTCTX_A4_OFF        36
#define PORT_RISCV_EXTCTX_A5_OFF        40
#define PORT_RISCV_EXTCTX_MEPC_OFF      44
#define PORT_RISCV_EXTCTX_MCAUSE_OFF    48
#define PORT_RISCV_EXTCTX_MSTATUS_OFF   52
#define PORT_RISCV_EXTCTX_A6_OFF        56
#define PORT_RISCV_EXTCTX_A7_OFF        60
#define PORT_RISCV_EXTCTX_T3_OFF        64
#define PORT_RISCV_EXTCTX_T4_OFF        68
#define PORT_RISCV_EXTCTX_T5_OFF        72
#define PORT_RISCV_EXTCTX_T6_OFF        76

/*===========================================================================*/
/* Thread frame layout (struct port_intctx).                                 */
/*===========================================================================*/

/**
 * @brief   Offsets, in bytes, of the fields of the thread frame.
 * @note    The layout is shared between the C structure and the assembler
 *          macros, it must be kept in sync with @p port_intctx.
 */
#define PORT_RISCV_INTCTX_RA_OFF        0
#define PORT_RISCV_INTCTX_S0_OFF        4
#define PORT_RISCV_INTCTX_S1_OFF        8
#define PORT_RISCV_INTCTX_S2_OFF        12
#define PORT_RISCV_INTCTX_S3_OFF        16
#define PORT_RISCV_INTCTX_S4_OFF        20
#define PORT_RISCV_INTCTX_S5_OFF        24
#define PORT_RISCV_INTCTX_S6_OFF        28
#define PORT_RISCV_INTCTX_S7_OFF        32
#define PORT_RISCV_INTCTX_S8_OFF        36
#define PORT_RISCV_INTCTX_S9_OFF        40
#define PORT_RISCV_INTCTX_S10_OFF       44
#define PORT_RISCV_INTCTX_S11_OFF       48

/*===========================================================================*/
/* Assembler support.                                                        */
/*===========================================================================*/

#define REGBYTES                        4
#define FREGBYTES                       4

#if defined(_FROM_ASM_)

/*
 * Load/store helpers, the base register is always the stack pointer.
 * The offset is an absolute expression, the frame offsets defined above
 * are used to address the fields of the saved contexts.
 */
.macro LOAD reg, offset
    lw      \reg, \offset(sp)
.endm

.macro STORE reg, offset
    sw      \reg, \offset(sp)
.endm

.macro FLOAD reg, offset
    flw     \reg, \offset(sp)
.endm

.macro FSTORE reg, offset
    fsw     \reg, \offset(sp)
.endm

/* Load/store with an explicit base register. */
.macro LOAD_REG reg, offset, base
    lw      \reg, \offset(\base)
.endm

.macro STORE_REG reg, offset, base
    sw      \reg, \offset(\base)
.endm

#endif /* !defined(_FROM_ASM_) */

#endif /* _RISCV_ENCODING_H_ */

/** @} */
