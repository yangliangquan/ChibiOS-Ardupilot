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
 *          - the very first instruction clears MIE and executes fence.i.
 *            This closes the hardware-nesting window before mepc/mcause/
 *            mstatus are read. An already-pending higher-priority IRQ cannot
 *            be the cause of a nest here: the PFIC arbiter would have
 *            selected it at the boundary where the current IRQ was taken.
 *          - the full interrupted context is then saved on the current
 *            thread stack (software stacking, per-thread stacks);
 *          - external IRQs re-enable MIE to allow priority nesting, the
 *            non-nesting variant (exceptions and SW) keeps it cleared.
 *
 *          Interrupt exit is centralised in _port_exit_from_isr (chcoreasm.S):
 *          MIE is cleared again, fence.i is executed, the frame is restored
 *          with mstatus.MIE=0 and finally mret re-enables interrupts from
 *          mstatus.MPIE. MIE is never restored early.
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
 * caller-saved FP registers when the F extension is enabled) at sp.
 * Only t0 is used as a temporary, and it is saved before first use.
 */
.macro PORT_SAVE_CONTEXT
    addi    sp, sp, -EXTCTX_SIZE
    STORE   ra, (0*REGBYTES)(sp)
    STORE   tp, (1*REGBYTES)(sp)
    STORE   t0, (2*REGBYTES)(sp)
    STORE   t1, (3*REGBYTES)(sp)
    STORE   t2, (4*REGBYTES)(sp)
    STORE   a0, (5*REGBYTES)(sp)
    STORE   a1, (6*REGBYTES)(sp)
    STORE   a2, (7*REGBYTES)(sp)
    STORE   a3, (8*REGBYTES)(sp)
    STORE   a4, (9*REGBYTES)(sp)
    STORE   a5, (10*REGBYTES)(sp)
    STORE   a6, (14*REGBYTES)(sp)
    STORE   a7, (15*REGBYTES)(sp)
    STORE   t3, (16*REGBYTES)(sp)
    STORE   t4, (17*REGBYTES)(sp)
    STORE   t5, (18*REGBYTES)(sp)
    STORE   t6, (19*REGBYTES)(sp)
    csrr    t0, CSR_MEPC
    STORE   t0, (11*REGBYTES)(sp)
    csrr    t0, CSR_MCAUSE
    STORE   t0, (12*REGBYTES)(sp)
    csrr    t0, CSR_MSTATUS
    STORE   t0, (13*REGBYTES)(sp)
#if PORT_RISCV_USE_FPU
    FSTORE  ft0,  (20*FREGBYTES)(sp)
    FSTORE  ft1,  (21*FREGBYTES)(sp)
    FSTORE  ft2,  (22*FREGBYTES)(sp)
    FSTORE  ft3,  (23*FREGBYTES)(sp)
    FSTORE  ft4,  (24*FREGBYTES)(sp)
    FSTORE  ft5,  (25*FREGBYTES)(sp)
    FSTORE  ft6,  (26*FREGBYTES)(sp)
    FSTORE  ft7,  (27*FREGBYTES)(sp)
    FSTORE  ft8,  (28*FREGBYTES)(sp)
    FSTORE  ft9,  (29*FREGBYTES)(sp)
    FSTORE  ft10, (30*FREGBYTES)(sp)
    FSTORE  ft11, (31*FREGBYTES)(sp)
    FSTORE  fa0,  (32*FREGBYTES)(sp)
    FSTORE  fa1,  (33*FREGBYTES)(sp)
    FSTORE  fa2,  (34*FREGBYTES)(sp)
    FSTORE  fa3,  (35*FREGBYTES)(sp)
    FSTORE  fa4,  (36*FREGBYTES)(sp)
    FSTORE  fa5,  (37*FREGBYTES)(sp)
    FSTORE  fa6,  (38*FREGBYTES)(sp)
    FSTORE  fa7,  (39*FREGBYTES)(sp)
    csrr    t0, CSR_FCSR
    STORE   t0, (40*REGBYTES)(sp)
#endif
.endm

/*
 * Restores the context saved by PORT_SAVE_CONTEXT from sp.
 *
 * mstatus is written first so that FS is valid before the FP loads; the saved
 * value has MIE=0 (it was captured after the entry clear), so interrupts stay
 * disabled until mret restores MIE from MPIE.
 */
.macro PORT_RESTORE_CONTEXT
    LOAD    t0, (11*REGBYTES)(sp)
    csrw    CSR_MEPC, t0
    LOAD    t0, (12*REGBYTES)(sp)
    csrw    CSR_MCAUSE, t0
    LOAD    t0, (13*REGBYTES)(sp)
    csrw    CSR_MSTATUS, t0
#if PORT_RISCV_USE_FPU
    LOAD    t0, (40*REGBYTES)(sp)
    csrw    CSR_FCSR, t0
    FLOAD   ft0,  (20*FREGBYTES)(sp)
    FLOAD   ft1,  (21*FREGBYTES)(sp)
    FLOAD   ft2,  (22*FREGBYTES)(sp)
    FLOAD   ft3,  (23*FREGBYTES)(sp)
    FLOAD   ft4,  (24*FREGBYTES)(sp)
    FLOAD   ft5,  (25*FREGBYTES)(sp)
    FLOAD   ft6,  (26*FREGBYTES)(sp)
    FLOAD   ft7,  (27*FREGBYTES)(sp)
    FLOAD   ft8,  (28*FREGBYTES)(sp)
    FLOAD   ft9,  (29*FREGBYTES)(sp)
    FLOAD   ft10, (30*FREGBYTES)(sp)
    FLOAD   ft11, (31*FREGBYTES)(sp)
    FLOAD   fa0,  (32*FREGBYTES)(sp)
    FLOAD   fa1,  (33*FREGBYTES)(sp)
    FLOAD   fa2,  (34*FREGBYTES)(sp)
    FLOAD   fa3,  (35*FREGBYTES)(sp)
    FLOAD   fa4,  (36*FREGBYTES)(sp)
    FLOAD   fa5,  (37*FREGBYTES)(sp)
    FLOAD   fa6,  (38*FREGBYTES)(sp)
    FLOAD   fa7,  (39*FREGBYTES)(sp)
#endif
    LOAD    ra, (0*REGBYTES)(sp)
    LOAD    tp, (1*REGBYTES)(sp)
    LOAD    t0, (2*REGBYTES)(sp)
    LOAD    t1, (3*REGBYTES)(sp)
    LOAD    t2, (4*REGBYTES)(sp)
    LOAD    a0, (5*REGBYTES)(sp)
    LOAD    a1, (6*REGBYTES)(sp)
    LOAD    a2, (7*REGBYTES)(sp)
    LOAD    a3, (8*REGBYTES)(sp)
    LOAD    a4, (9*REGBYTES)(sp)
    LOAD    a5, (10*REGBYTES)(sp)
    LOAD    a6, (14*REGBYTES)(sp)
    LOAD    a7, (15*REGBYTES)(sp)
    LOAD    t3, (16*REGBYTES)(sp)
    LOAD    t4, (17*REGBYTES)(sp)
    LOAD    t5, (18*REGBYTES)(sp)
    LOAD    t6, (19*REGBYTES)(sp)
    addi    sp, sp, EXTCTX_SIZE
.endm

/*
 * Common external interrupt stub: saves the context, re-enables nesting and
 * calls the handler. The handler is a normal C function returning void; the
 * exit path is shared with the non-nesting stub.
 */
.macro PORT_IRQ_STUB handler
    csrc    CSR_MSTATUS, MSTATUS_MIE
    fence.i
    PORT_SAVE_CONTEXT
    csrs    CSR_MSTATUS, MSTATUS_MIE
    call    \handler
    j       _port_exit_from_isr
.endm

/*
 * Non-nesting stub for exceptions and the software interrupt: MIE stays
 * cleared for the whole handler.
 */
.macro PORT_IRQ_STUB_NONEST handler
    csrc    CSR_MSTATUS, MSTATUS_MIE
    fence.i
    PORT_SAVE_CONTEXT
    call    \handler
    j       _port_exit_from_isr
.endm

#endif /* _IRQ_STUBS_H_ */

/** @} */
