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

/* Disable interrupts globally. */
.macro DISABLE_MIE
    csrc CSR_MSTATUS, MSTATUS_MIE
.endm

/* Enable interrupts globally. */
.macro ENABLE_MIE
    csrs CSR_MSTATUS, MSTATUS_MIE
.endm

/* --------------------------------------------------------------------------
 * Trap entry point (_start_trap), exceptions are routed here.
 * -------------------------------------------------------------------------- */
.section .trap, "ax"
.option push
.option norelax
.align 4
.globl _start_trap
_start_trap:
    /* Save the caller saving registers (context). */
    PORT_SAVE_CONTEXT

    /* Set the function arguments: mcause, frame, mtval. */
    csrr a0, CSR_MCAUSE
    mv a1, sp
    csrr a2, CSR_MTVAL

    /* Call the function. */
    call handle_trap

    /* Restore the caller saving registers (context). */
    PORT_RESTORE_CONTEXT

    /* Return to regular code. */
    mret
.option pop

/* --------------------------------------------------------------------------
 * Start a thread by invoking its work function.
 *
 * Threads execution starts here, the code leaves the system critical zone
 * and then jumps into the thread function passed in register S0. The
 * register S1 contains the thread parameter. The function chThdExit() is
 * called on thread function return.
 * -------------------------------------------------------------------------- */
.globl  _port_thread_start
_port_thread_start:
#if CH_DBG_SYSTEM_STATE_CHECK
    jal     ra, __dbg_check_unlock
#endif
#if CH_DBG_STATISTICS
    jal     ra, _stats_stop_measure_crit_thd
#endif
    ENABLE_MIE
    mv      a0, s1
    jalr    ra, s0
    li      a0, 0              # MSG_OK
    jal     ra, chThdExit

_zombies:
       j       _zombies

/* --------------------------------------------------------------------------
 * Performs a context switch between two threads.
 * a0 = ntp, a1 = otp
 * -------------------------------------------------------------------------- */
.option push
.option norelax
.align 4
.globl  _port_switch
.type _port_switch,@function
_port_switch:
    # OLD THREAD CONTEXT SAVE BEGIN
    # Allocate space for port_intctx structure on the threading stack.
    # The stackpointer is 16 byte aligned to be compliant with risc-v abi.
#if !defined(__riscv_32e)
    addi sp, sp, -16*REGBYTES
#else
    addi sp, sp, -4*REGBYTES
#endif

    # Store callee save registers
    STORE      ra,  0*REGBYTES(sp)
    STORE      s0,  1*REGBYTES(sp)
    STORE      s1,  2*REGBYTES(sp)
#if !defined(__riscv_32e)
    STORE      s2,  3*REGBYTES(sp)
    STORE      s3,  4*REGBYTES(sp)
    STORE      s4,  5*REGBYTES(sp)
    STORE      s5,  6*REGBYTES(sp)
    STORE      s6,  7*REGBYTES(sp)
    STORE      s7,  8*REGBYTES(sp)
    STORE      s8,  9*REGBYTES(sp)
    STORE      s9,  10*REGBYTES(sp)
    STORE      s10, 11*REGBYTES(sp)
    STORE      s11, 12*REGBYTES(sp)
#endif

    # Store stackpointer in otp->ctx
    STORE      sp, CONTEXT_OFFSET(a1)
    # OLD THREAD CONTEXT SAVE END

    # NEW THREAD CONTEXT RESTORE BEGIN
    # Load stackpointer from ntp->ctx
    LOAD       sp, CONTEXT_OFFSET(a0)

    LOAD      ra,  0*REGBYTES(sp)
    LOAD      s0,  1*REGBYTES(sp)
    LOAD      s1,  2*REGBYTES(sp)
#if !defined(__riscv_32e)
    LOAD      s2,  3*REGBYTES(sp)
    LOAD      s3,  4*REGBYTES(sp)
    LOAD      s4,  5*REGBYTES(sp)
    LOAD      s5,  6*REGBYTES(sp)
    LOAD      s6,  7*REGBYTES(sp)
    LOAD      s7,  8*REGBYTES(sp)
    LOAD      s8,  9*REGBYTES(sp)
    LOAD      s9,  10*REGBYTES(sp)
    LOAD      s10, 11*REGBYTES(sp)
    LOAD      s11, 12*REGBYTES(sp)
#endif

    # De-allocate space on the threading stack
#if !defined(__riscv_32e)
    addi sp, sp, 16*REGBYTES
#else
    addi sp, sp, 4*REGBYTES
#endif
    # NEW THREAD CONTEXT RESTORE END

    # Jump to return address loaded into ra
    ret
.option pop

/* --------------------------------------------------------------------------
 * Post-IRQ switch code.
 *
 * Reached via mret from an interrupt stub when preemption is required.
 * mstatus.MIE is 0 at this point (MPIE cleared by the stub).
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl _port_switch_from_isr
_port_switch_from_isr:
#if CH_DBG_STATISTICS
    jal     ra, _stats_start_measure_crit_thd
#endif
#if CH_DBG_SYSTEM_STATE_CHECK
    jal     ra, __dbg_check_lock
#endif
    # Calls _port_switch at the end of the function
    jal     ra, chSchDoPreemption
#if CH_DBG_SYSTEM_STATE_CHECK
    jal     ra, __dbg_check_unlock
#endif
#if CH_DBG_STATISTICS
    jal     ra, _stats_stop_measure_crit_thd
#endif

.globl _port_exit_from_isr
_port_exit_from_isr:
    # Restore caller registers and csr registers from the thread stack
    PORT_RESTORE_CONTEXT

    # Leave interrupt handling and return to address stored in mepc.
    mret

.option pop

.globl handle_trap
.weak handle_trap
handle_trap:
    j       handle_trap  # jump to handle_trap

#endif /* !defined(__DOXYGEN__) */

/** @} */
