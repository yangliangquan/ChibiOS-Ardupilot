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
 * @details Frame layout must match struct port_extctx in chcore.h:
 *          0 ra, 1 tp, 2 t0, 3 t1, 4 t2, 5..10 a0..a5,
 *          11 mepc, 12 mcause, 13 mstatus, 14..19 a6,a7,t3..t6.
 *
 *          The interrupt entry is a jump (no hardware return address):
 *          the stub saves the caller-saved context on the interrupted
 *          stack, invokes the weak handler symbol with the standard
 *          ChibiOS bool-returning protocol, then either returns or
 *          performs an ISR context switch via _port_switch_from_isr.
 *
 * @addtogroup RISCV_ECLIC_GCC_CORE
 * @{
 */

#ifndef _IRQ_STUBS_H_
#define _IRQ_STUBS_H_

/* Frame size: 20 words. */
#define EXTCTX_SIZE     (20*REGBYTES)

/*
 * Saves the interrupted context (caller-saved regs + CSRs) at sp.
 * Only t0 used as temporary, stored before use.
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
.endm

/* Restores the context saved by PORT_SAVE_CONTEXT from sp. */
.macro PORT_RESTORE_CONTEXT
    LOAD    t0, (11*REGBYTES)(sp)
    csrw    CSR_MEPC, t0
    LOAD    t0, (12*REGBYTES)(sp)
    csrw    CSR_MCAUSE, t0
    LOAD    t0, (13*REGBYTES)(sp)
    csrw    CSR_MSTATUS, t0
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
 * Common interrupt stub: saves the context, calls the handler symbol
 * passed as argument and applies the ChibiOS ISR exit protocol.
 * a0 = bool returned by the handler (preemption required).
 */
.macro PORT_IRQ_STUB handler
    PORT_SAVE_CONTEXT
    csrc    CSR_MSTATUS, MSTATUS_MIE
    call    \handler
    csrc    CSR_MSTATUS, MSTATUS_MIE
    beq     a0, zero, 990f
    /* Preemption required: chain into the switch routine. mret must
       land with MIE=0 (clear MPIE so mret restores MIE=0). MPIE is
       bit 7, outside the CSRxxI 5-bit immediate range: use a register. */
    li      t0, MSTATUS_MPIE
    csrc    CSR_MSTATUS, t0
    la      a0, _port_switch_from_isr
    csrw    CSR_MEPC, a0
    mret
990:
    j       _port_exit_from_isr
.endm

#endif /* _IRQ_STUBS_H_ */

/** @} */
