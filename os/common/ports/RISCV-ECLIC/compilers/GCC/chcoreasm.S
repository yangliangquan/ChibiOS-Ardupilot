/*
    ChibiOS - Copyright (C) 2021 Stefan Kerkmann.
    ChibiOS - Copyright (C) 2020 Patrick Seidel.
    Adapted for CH32H417 (QingKe V5F), Copyright (C) 2026 ChibiOS contributors.

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
 * @file    compilers/GCC/chcoreasm.S
 * @brief   CH32H417 (QingKe V5F) ECLIC architecture port low level code.
 *
 * @addtogroup RISCV_ECLIC_GCC_CORE
 * @{
 */

#if !defined(FALSE) || defined(__DOXYGEN__)
#define FALSE   0
#endif

#if !defined(TRUE) || defined(__DOXYGEN__)
#define TRUE    1
#endif

#define _FROM_ASM_
#include "chlicense.h"
#include "chconf.h"
#include "chcore.h"
#include "riscv_encoding.h"
#include "irq_stubs.h"

#if !defined(__DOXYGEN__)

/* --------------------------------------------------------------------------
 * RTOS-specific context offset.
 * -------------------------------------------------------------------------- */
#if defined(_CHIBIOS_RT_CONF_)
#define CONTEXT_OFFSET  12
#elif defined(_CHIBIOS_NIL_CONF_)
#define CONTEXT_OFFSET  0
#else
#error "invalid chconf.h"
#endif

/* --------------------------------------------------------------------------
 * Start a thread by invoking its work function.
 *
 * Threads execution starts here, the code leaves the system critical zone
 * and then jumps into the thread function passed in register S0. The
 * register S1 contains the thread parameter. The function chThdExit() is
 * called on thread function return.
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl  _port_thread_start
.type _port_thread_start,@function
_port_thread_start:
#if CH_DBG_SYSTEM_STATE_CHECK
    call    __dbg_check_unlock
#endif
#if CH_DBG_STATISTICS
    call    __stats_stop_measure_crit_thd
#endif
#if PORT_RISCV_USE_FPU
    /* New threads start with the default FP control/status settings. */
    csrw    CSR_FCSR, zero
#endif
    csrs    CSR_MSTATUS, MSTATUS_MIE
    mv      a0, s1
    jalr    ra, s0
    li      a0, 0              # MSG_OK
    call    chThdExit

_zombies:
    j       _zombies
.option pop

/* --------------------------------------------------------------------------
 * Common interrupt entry.
 *
 * The vector stubs load the address of the C handler in gp and jump here,
 * the whole entry/exit sequence is shared by all the vectors so that the
 * code size does not grow with the number of interrupt sources.
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl  _port_irq_entry
.type _port_irq_entry,@function
_port_irq_entry:
    PORT_IRQ_ENTRY 1
    j       _port_exit_from_isr

/* Entry point for the exceptions, those never allow nesting. */
.align 4
.globl  _port_irq_entry_nonest
.type _port_irq_entry_nonest,@function
_port_irq_entry_nonest:
    PORT_IRQ_ENTRY 0
    j       _port_exit_from_isr
.option pop

/* --------------------------------------------------------------------------
 * Performs a context switch between two threads.
 * a0 = ntp, a1 = otp
 *
 * The frame layout must match struct port_intctx in chcore.h. The frame is
 * 16-byte aligned in both the integer-only and the FP variants.
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl  _port_switch
.type _port_switch,@function
_port_switch:
    /* OLD THREAD CONTEXT SAVE BEGIN */
    addi    sp, sp, -PORT_RISCV_INTCTX_SIZE

    STORE   ra,  PORT_RISCV_INTCTX_RA_OFF
    STORE   s0,  PORT_RISCV_INTCTX_S0_OFF
    STORE   s1,  PORT_RISCV_INTCTX_S1_OFF
    STORE   s2,  PORT_RISCV_INTCTX_S2_OFF
    STORE   s3,  PORT_RISCV_INTCTX_S3_OFF
    STORE   s4,  PORT_RISCV_INTCTX_S4_OFF
    STORE   s5,  PORT_RISCV_INTCTX_S5_OFF
    STORE   s6,  PORT_RISCV_INTCTX_S6_OFF
    STORE   s7,  PORT_RISCV_INTCTX_S7_OFF
    STORE   s8,  PORT_RISCV_INTCTX_S8_OFF
    STORE   s9,  PORT_RISCV_INTCTX_S9_OFF
    STORE   s10, PORT_RISCV_INTCTX_S10_OFF
    STORE   s11, PORT_RISCV_INTCTX_S11_OFF
#if PORT_RISCV_USE_FPU
    FSTORE  fs0,  PORT_RISCV_INTCTX_FS0_OFF
    FSTORE  fs1,  (PORT_RISCV_INTCTX_FS0_OFF + 1 * FREGBYTES)
    FSTORE  fs2,  (PORT_RISCV_INTCTX_FS0_OFF + 2 * FREGBYTES)
    FSTORE  fs3,  (PORT_RISCV_INTCTX_FS0_OFF + 3 * FREGBYTES)
    FSTORE  fs4,  (PORT_RISCV_INTCTX_FS0_OFF + 4 * FREGBYTES)
    FSTORE  fs5,  (PORT_RISCV_INTCTX_FS0_OFF + 5 * FREGBYTES)
    FSTORE  fs6,  (PORT_RISCV_INTCTX_FS0_OFF + 6 * FREGBYTES)
    FSTORE  fs7,  (PORT_RISCV_INTCTX_FS0_OFF + 7 * FREGBYTES)
    FSTORE  fs8,  (PORT_RISCV_INTCTX_FS0_OFF + 8 * FREGBYTES)
    FSTORE  fs9,  (PORT_RISCV_INTCTX_FS0_OFF + 9 * FREGBYTES)
    FSTORE  fs10, (PORT_RISCV_INTCTX_FS0_OFF + 10 * FREGBYTES)
    FSTORE  fs11, (PORT_RISCV_INTCTX_FS0_OFF + 11 * FREGBYTES)
    csrr    t0, CSR_FCSR
    STORE   t0, PORT_RISCV_INTCTX_FCSR_OFF
#endif

    /* Order all memory operations of the outgoing thread before the switch on
       the out-of-order V5F. */
    fence   iorw, iorw

    /* Store stack pointer in otp->ctx. */
    STORE_REG   sp, CONTEXT_OFFSET, a1
    /* OLD THREAD CONTEXT SAVE END */

    /* NEW THREAD CONTEXT RESTORE BEGIN */
    LOAD_REG    sp, CONTEXT_OFFSET, a0

    LOAD    ra,  PORT_RISCV_INTCTX_RA_OFF
    LOAD    s0,  PORT_RISCV_INTCTX_S0_OFF
    LOAD    s1,  PORT_RISCV_INTCTX_S1_OFF
    LOAD    s2,  PORT_RISCV_INTCTX_S2_OFF
    LOAD    s3,  PORT_RISCV_INTCTX_S3_OFF
    LOAD    s4,  PORT_RISCV_INTCTX_S4_OFF
    LOAD    s5,  PORT_RISCV_INTCTX_S5_OFF
    LOAD    s6,  PORT_RISCV_INTCTX_S6_OFF
    LOAD    s7,  PORT_RISCV_INTCTX_S7_OFF
    LOAD    s8,  PORT_RISCV_INTCTX_S8_OFF
    LOAD    s9,  PORT_RISCV_INTCTX_S9_OFF
    LOAD    s10, PORT_RISCV_INTCTX_S10_OFF
    LOAD    s11, PORT_RISCV_INTCTX_S11_OFF
#if PORT_RISCV_USE_FPU
    LOAD    t0, PORT_RISCV_INTCTX_FCSR_OFF
    csrw    CSR_FCSR, t0
    FLOAD   fs0,  PORT_RISCV_INTCTX_FS0_OFF
    FLOAD   fs1,  (PORT_RISCV_INTCTX_FS0_OFF + 1 * FREGBYTES)
    FLOAD   fs2,  (PORT_RISCV_INTCTX_FS0_OFF + 2 * FREGBYTES)
    FLOAD   fs3,  (PORT_RISCV_INTCTX_FS0_OFF + 3 * FREGBYTES)
    FLOAD   fs4,  (PORT_RISCV_INTCTX_FS0_OFF + 4 * FREGBYTES)
    FLOAD   fs5,  (PORT_RISCV_INTCTX_FS0_OFF + 5 * FREGBYTES)
    FLOAD   fs6,  (PORT_RISCV_INTCTX_FS0_OFF + 6 * FREGBYTES)
    FLOAD   fs7,  (PORT_RISCV_INTCTX_FS0_OFF + 7 * FREGBYTES)
    FLOAD   fs8,  (PORT_RISCV_INTCTX_FS0_OFF + 8 * FREGBYTES)
    FLOAD   fs9,  (PORT_RISCV_INTCTX_FS0_OFF + 9 * FREGBYTES)
    FLOAD   fs10, (PORT_RISCV_INTCTX_FS0_OFF + 10 * FREGBYTES)
    FLOAD   fs11, (PORT_RISCV_INTCTX_FS0_OFF + 11 * FREGBYTES)
#endif

    addi    sp, sp, PORT_RISCV_INTCTX_SIZE
    /* NEW THREAD CONTEXT RESTORE END */

    ret
.option pop

/* --------------------------------------------------------------------------
 * ISR exit path.
 *
 * All the vector stubs tail-jump here after the C handler returned. With
 * PORT_RISCV_USE_IRQ_STACK enabled the handler ran on .istack, so the
 * outermost level has its frame on the interrupted thread stack (latched
 * in mscratch) while sp points at __irq_stack_end__. Nested levels have
 * both their frame and sp in .istack.
 *
 * Nested levels simply restore their frame and mret back to the interrupted
 * point; only the outermost level switches sp back to the thread frame and
 * evaluates a pending preemption. This is the check the PFIC GISR nesting
 * state is used for (see _port_irq_stack_switch_back() in chcore.c).
 *
 * If the handler requested a preemption, the exception return of the
 * outermost level is redirected to the switch adapter: the frame stays on
 * the interrupted thread stack and the adapter performs the context switch,
 * then restores the frame and returns to the interrupted point. This is the
 * RISC-V equivalent of the ARMv7-M "return to base" trick, it keeps the
 * reschedule atomic with respect to the interrupt exit. Note that the
 * second mret of this path is harmless: stacking is done in software, so
 * no hardware prologue/epilogue is triggered by an exception return.
 *
 * Interrupts are disabled before the restore and stay disabled until mret
 * re-enables them from the saved mstatus.MPIE. The saved mstatus itself
 * has MIE=0, so a nested trap can never observe a half-restored frame.
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl _port_exit_from_isr
.type _port_exit_from_isr,@function
_port_exit_from_isr:
    csrc    CSR_MSTATUS, MSTATUS_MIE
    fence   iorw, iorw

#if PORT_RISCV_USE_IRQ_STACK
    /* Only the outermost interrupt level reaches the common preemption
       path. Nested levels keep sp in .istack and return straight away.
       The helper also clears the mscratch sentinel on the way out. */
    mv      a0, sp
    la      a1, __irq_stack_end__
    call    _port_irq_stack_switch_back
    beqz    a0, _port_restore_from_isr
    mv      sp, a0
#endif

    /* Asking the port whether the interrupted thread must be preempted, the
       check needs the scheduler state and the interrupt nesting level. */
    call    _port_irq_preempt_pending
    beqz    a0, _port_restore_from_isr

    /* Redirecting the exception return to the switch adapter, the frame is
       kept on the interrupted thread stack. Interrupts stay disabled during
       the context switch, mret would otherwise re-enable them from MPIE. */
    LOAD    t0, PORT_RISCV_EXTCTX_MSTATUS_OFF
    andi    t0, t0, ~MSTATUS_MPIE
    csrw    CSR_MSTATUS, t0
    la      t0, _port_switch_from_isr
    csrw    CSR_MEPC, t0
    mret

/* --------------------------------------------------------------------------
 * Restores the interrupted context from the frame at sp and returns to the
 * interrupted point.
 *
 * The argument area pushed by the vector stub is discarded as well.
 * -------------------------------------------------------------------------- */
.align 4
.globl _port_restore_from_isr
.type _port_restore_from_isr,@function
_port_restore_from_isr:
    PORT_RESTORE_CONTEXT
    addi    sp, sp, PORT_RISCV_IRQ_ARG_SIZE
    mret

/* --------------------------------------------------------------------------
 * Switch adapter.
 *
 * Reached through the exception return redirection done above, the frame of
 * the interrupted thread is at the top of the stack. The context switch is
 * performed here, the interrupted thread is scheduled back in later and the
 * execution resumes after the call, where the frame is finally restored.
 *
 * Note, the interrupt frame is still on the stack while the adapter runs,
 * but the frames pushed by a nested interrupt are allocated below it, so
 * the interrupted context cannot be damaged.
 * -------------------------------------------------------------------------- */
.align 4
.globl _port_switch_from_isr
.type _port_switch_from_isr,@function
_port_switch_from_isr:
#if CH_DBG_STATISTICS
    call    __stats_start_measure_crit_thd
#endif
#if CH_DBG_SYSTEM_STATE_CHECK
    call    __dbg_check_lock
#endif
    call    chSchDoPreemption
#if CH_DBG_SYSTEM_STATE_CHECK
    call    __dbg_check_unlock
#endif
#if CH_DBG_STATISTICS
    call    __stats_stop_measure_crit_thd
#endif
    j       _port_restore_from_isr
.option pop

#endif /* !defined(__DOXYGEN__) */

/** @} */
