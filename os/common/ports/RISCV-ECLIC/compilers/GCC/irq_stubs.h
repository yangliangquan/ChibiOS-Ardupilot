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
 * @file    irq_stubs.h
 * @brief   Shared assembler macros for the CH32H417 interrupt entry/exit
 *          protocol (software stacking, QingKe V5F).
 *
 * @details Frame layout must match struct port_extctx in chcore.h.
 *
 *          Interrupt entry:
 *          - the vector stub pushes an argument area on the stack holding
 *            the address of the C handler and the value of the register
 *            used to compute it, then jumps to the common entry point. The
 *            area is needed because the interrupted context is saved by the
 *            entry point, so no register can carry the handler address
 *            across the save without losing a register of the interrupted
 *            context. The area is a multiple of 16 bytes so that the
 *            interrupt frame stays aligned.
 *          - the entry point clears mstatus.MIE and executes a memory
 *            barrier before reading mepc/mcause/mstatus, so the captured
 *            state is the one of the interrupted context.
 *          - the full interrupted context is then saved on the current
 *            thread stack (software stacking, per-thread stacks);
 *          - external IRQs re-enable MIE to allow priority nesting, the
 *            non-nesting variant (exceptions) keeps it cleared.
 *
 *          Interrupt exit is centralised in _port_exit_from_isr
 *          (chcoreasm.S): the frame is restored and mret returns to the
 *          interrupted point. If the handler requested a preemption, and
 *          this is the outermost interrupt level, the exception return is
 *          redirected to the switch adapter which performs the context
 *          switch and then restores the frame.
 *
 * @addtogroup RISCV_ECLIC_GCC_CORE
 * @{
 */

#ifndef _IRQ_STUBS_H_
#define _IRQ_STUBS_H_

/* Frame size in bytes, shared with chcore.h. */
#define EXTCTX_SIZE     PORT_RISCV_EXTCTX_SIZE

/*
 * Saves the interrupted context (caller-saved GPRs + CSRs, plus the
 * caller-saved FP registers when the F extension is enabled) below the
 * argument area pushed by the vector stub.
 *
 * On entry the argument area is at the top of the stack, it holds the
 * address of the handler and the value of t0, the only register the stub
 * could use to compute it. Only t0 is used as a temporary, and it is saved
 * before first use.
 */
.macro PORT_SAVE_CONTEXT
    LOAD    t0, 0
    addi    sp, sp, -EXTCTX_SIZE
    STORE   t0, PORT_RISCV_EXTCTX_T0_OFF
    STORE   ra, PORT_RISCV_EXTCTX_RA_OFF
    STORE   tp, PORT_RISCV_EXTCTX_TP_OFF
    STORE   t1, PORT_RISCV_EXTCTX_T1_OFF
    STORE   t2, PORT_RISCV_EXTCTX_T2_OFF
    STORE   a0, PORT_RISCV_EXTCTX_A0_OFF
    STORE   a1, PORT_RISCV_EXTCTX_A1_OFF
    STORE   a2, PORT_RISCV_EXTCTX_A2_OFF
    STORE   a3, PORT_RISCV_EXTCTX_A3_OFF
    STORE   a4, PORT_RISCV_EXTCTX_A4_OFF
    STORE   a5, PORT_RISCV_EXTCTX_A5_OFF
    STORE   a6, PORT_RISCV_EXTCTX_A6_OFF
    STORE   a7, PORT_RISCV_EXTCTX_A7_OFF
    STORE   t3, PORT_RISCV_EXTCTX_T3_OFF
    STORE   t4, PORT_RISCV_EXTCTX_T4_OFF
    STORE   t5, PORT_RISCV_EXTCTX_T5_OFF
    STORE   t6, PORT_RISCV_EXTCTX_T6_OFF
    csrr    t0, CSR_MEPC
    STORE   t0, PORT_RISCV_EXTCTX_MEPC_OFF
    csrr    t0, CSR_MCAUSE
    STORE   t0, PORT_RISCV_EXTCTX_MCAUSE_OFF
    csrr    t0, CSR_MSTATUS
    STORE   t0, PORT_RISCV_EXTCTX_MSTATUS_OFF
#if PORT_RISCV_USE_FPU
    FSTORE  ft0,  PORT_RISCV_EXTCTX_FT0_OFF
    FSTORE  ft1,  (PORT_RISCV_EXTCTX_FT0_OFF + 1 * FREGBYTES)
    FSTORE  ft2,  (PORT_RISCV_EXTCTX_FT0_OFF + 2 * FREGBYTES)
    FSTORE  ft3,  (PORT_RISCV_EXTCTX_FT0_OFF + 3 * FREGBYTES)
    FSTORE  ft4,  (PORT_RISCV_EXTCTX_FT0_OFF + 4 * FREGBYTES)
    FSTORE  ft5,  (PORT_RISCV_EXTCTX_FT0_OFF + 5 * FREGBYTES)
    FSTORE  ft6,  (PORT_RISCV_EXTCTX_FT0_OFF + 6 * FREGBYTES)
    FSTORE  ft7,  (PORT_RISCV_EXTCTX_FT0_OFF + 7 * FREGBYTES)
    FSTORE  ft8,  PORT_RISCV_EXTCTX_FT8_OFF
    FSTORE  ft9,  (PORT_RISCV_EXTCTX_FT8_OFF + 1 * FREGBYTES)
    FSTORE  ft10, (PORT_RISCV_EXTCTX_FT8_OFF + 2 * FREGBYTES)
    FSTORE  ft11, (PORT_RISCV_EXTCTX_FT8_OFF + 3 * FREGBYTES)
    FSTORE  fa0,  PORT_RISCV_EXTCTX_FA0_OFF
    FSTORE  fa1,  (PORT_RISCV_EXTCTX_FA0_OFF + 1 * FREGBYTES)
    FSTORE  fa2,  (PORT_RISCV_EXTCTX_FA0_OFF + 2 * FREGBYTES)
    FSTORE  fa3,  (PORT_RISCV_EXTCTX_FA0_OFF + 3 * FREGBYTES)
    FSTORE  fa4,  (PORT_RISCV_EXTCTX_FA0_OFF + 4 * FREGBYTES)
    FSTORE  fa5,  (PORT_RISCV_EXTCTX_FA0_OFF + 5 * FREGBYTES)
    FSTORE  fa6,  (PORT_RISCV_EXTCTX_FA0_OFF + 6 * FREGBYTES)
    FSTORE  fa7,  (PORT_RISCV_EXTCTX_FA0_OFF + 7 * FREGBYTES)
    csrr    t0, CSR_FCSR
    STORE   t0, PORT_RISCV_EXTCTX_FCSR_OFF
#endif
.endm

/*
 * Restores the context saved by PORT_SAVE_CONTEXT from sp and pops the
 * frame.
 *
 * mstatus is written first so that FS is valid before the FP loads; the saved
 * value has MIE=0 (it was captured after the entry clear), so interrupts stay
 * disabled until mret restores MIE from MPIE.
 */
.macro PORT_RESTORE_CONTEXT
    LOAD    t0, PORT_RISCV_EXTCTX_MEPC_OFF
    csrw    CSR_MEPC, t0
    LOAD    t0, PORT_RISCV_EXTCTX_MCAUSE_OFF
    csrw    CSR_MCAUSE, t0
    LOAD    t0, PORT_RISCV_EXTCTX_MSTATUS_OFF
    csrw    CSR_MSTATUS, t0
#if PORT_RISCV_USE_FPU
    LOAD    t0, PORT_RISCV_EXTCTX_FCSR_OFF
    csrw    CSR_FCSR, t0
    FLOAD   ft0,  PORT_RISCV_EXTCTX_FT0_OFF
    FLOAD   ft1,  (PORT_RISCV_EXTCTX_FT0_OFF + 1 * FREGBYTES)
    FLOAD   ft2,  (PORT_RISCV_EXTCTX_FT0_OFF + 2 * FREGBYTES)
    FLOAD   ft3,  (PORT_RISCV_EXTCTX_FT0_OFF + 3 * FREGBYTES)
    FLOAD   ft4,  (PORT_RISCV_EXTCTX_FT0_OFF + 4 * FREGBYTES)
    FLOAD   ft5,  (PORT_RISCV_EXTCTX_FT0_OFF + 5 * FREGBYTES)
    FLOAD   ft6,  (PORT_RISCV_EXTCTX_FT0_OFF + 6 * FREGBYTES)
    FLOAD   ft7,  (PORT_RISCV_EXTCTX_FT0_OFF + 7 * FREGBYTES)
    FLOAD   ft8,  PORT_RISCV_EXTCTX_FT8_OFF
    FLOAD   ft9,  (PORT_RISCV_EXTCTX_FT8_OFF + 1 * FREGBYTES)
    FLOAD   ft10, (PORT_RISCV_EXTCTX_FT8_OFF + 2 * FREGBYTES)
    FLOAD   ft11, (PORT_RISCV_EXTCTX_FT8_OFF + 3 * FREGBYTES)
    FLOAD   fa0,  PORT_RISCV_EXTCTX_FA0_OFF
    FLOAD   fa1,  (PORT_RISCV_EXTCTX_FA0_OFF + 1 * FREGBYTES)
    FLOAD   fa2,  (PORT_RISCV_EXTCTX_FA0_OFF + 2 * FREGBYTES)
    FLOAD   fa3,  (PORT_RISCV_EXTCTX_FA0_OFF + 3 * FREGBYTES)
    FLOAD   fa4,  (PORT_RISCV_EXTCTX_FA0_OFF + 4 * FREGBYTES)
    FLOAD   fa5,  (PORT_RISCV_EXTCTX_FA0_OFF + 5 * FREGBYTES)
    FLOAD   fa6,  (PORT_RISCV_EXTCTX_FA0_OFF + 6 * FREGBYTES)
    FLOAD   fa7,  (PORT_RISCV_EXTCTX_FA0_OFF + 7 * FREGBYTES)
#endif
    LOAD    ra, PORT_RISCV_EXTCTX_RA_OFF
    LOAD    tp, PORT_RISCV_EXTCTX_TP_OFF
    LOAD    t0, PORT_RISCV_EXTCTX_T0_OFF
    LOAD    t1, PORT_RISCV_EXTCTX_T1_OFF
    LOAD    t2, PORT_RISCV_EXTCTX_T2_OFF
    LOAD    a0, PORT_RISCV_EXTCTX_A0_OFF
    LOAD    a1, PORT_RISCV_EXTCTX_A1_OFF
    LOAD    a2, PORT_RISCV_EXTCTX_A2_OFF
    LOAD    a3, PORT_RISCV_EXTCTX_A3_OFF
    LOAD    a4, PORT_RISCV_EXTCTX_A4_OFF
    LOAD    a5, PORT_RISCV_EXTCTX_A5_OFF
    LOAD    a6, PORT_RISCV_EXTCTX_A6_OFF
    LOAD    a7, PORT_RISCV_EXTCTX_A7_OFF
    LOAD    t3, PORT_RISCV_EXTCTX_T3_OFF
    LOAD    t4, PORT_RISCV_EXTCTX_T4_OFF
    LOAD    t5, PORT_RISCV_EXTCTX_T5_OFF
    LOAD    t6, PORT_RISCV_EXTCTX_T6_OFF
    addi    sp, sp, EXTCTX_SIZE
.endm

/*
 * Common interrupt entry.
 *
 * The vector stub leaves the argument area on the stack, the entry point
 * saves the interrupted context and then invokes the handler.
 *
 * @param[in] nest    TRUE to re-enable the interrupts after the context has
 *                    been saved (preemptable interrupt handlers), FALSE to
 *                    keep them disabled for the whole handler (exceptions).
 */
.macro PORT_IRQ_ENTRY nest
    csrc    CSR_MSTATUS, MSTATUS_MIE
    fence   iorw, iorw
    PORT_SAVE_CONTEXT
    /* The interrupted context is complete, the scratch register can be used
       to fetch the address of the handler from the argument area. */
    LOAD    t0, (EXTCTX_SIZE + PORT_RISCV_IRQ_ARG_HANDLER_OFF)
.if \nest
    csrs    CSR_MSTATUS, MSTATUS_MIE
.endif
    jalr    ra, t0, 0
.endm

/*
 * Vector stub prologue.
 *
 * The address of the handler is computed in t0, the previous value of t0 is
 * saved in the argument area together with the address itself.
 */
.macro PORT_IRQ_STUB_PROLOGUE handler
    addi    sp, sp, -PORT_RISCV_IRQ_ARG_SIZE
    STORE   t0, 0
    .option push
    .option norelax
    la      t0, \handler
    .option pop
    STORE   t0, PORT_RISCV_IRQ_ARG_HANDLER_OFF
.endm

#endif /* _IRQ_STUBS_H_ */

/** @} */
