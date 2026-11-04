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
_port_thread_start:
#if CH_DBG_SYSTEM_STATE_CHECK
    jal     ra, __dbg_check_unlock
#endif
#if CH_DBG_STATISTICS
    jal     ra, _stats_stop_measure_crit_thd
#endif
#if PORT_RISCV_USE_FPU
    /* New threads start with the default FP control/status settings. */
    csrw    CSR_FCSR, zero
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
 *
 * The frame layout must match struct port_intctx in chcore.h. The frame is
 * 16-byte aligned in both the integer-only and the FP variants.
 * -------------------------------------------------------------------------- */
.option push
.option norelax
.align 4
.globl  _port_switch
.type _port_switch,@function
_port_switch:
    /* OLD THREAD CONTEXT SAVE BEGIN */
    addi sp, sp, -PORT_RISCV_INTCTX_SIZE

    STORE   ra,  (0*REGBYTES)(sp)
    STORE   s0,  (1*REGBYTES)(sp)
    STORE   s1,  (2*REGBYTES)(sp)
    STORE   s2,  (3*REGBYTES)(sp)
    STORE   s3,  (4*REGBYTES)(sp)
    STORE   s4,  (5*REGBYTES)(sp)
    STORE   s5,  (6*REGBYTES)(sp)
    STORE   s6,  (7*REGBYTES)(sp)
    STORE   s7,  (8*REGBYTES)(sp)
    STORE   s8,  (9*REGBYTES)(sp)
    STORE   s9,  (10*REGBYTES)(sp)
    STORE   s10, (11*REGBYTES)(sp)
    STORE   s11, (12*REGBYTES)(sp)
#if PORT_RISCV_USE_FPU
    FSTORE  fs0,  (16*FREGBYTES)(sp)
    FSTORE  fs1,  (17*FREGBYTES)(sp)
    FSTORE  fs2,  (18*FREGBYTES)(sp)
    FSTORE  fs3,  (19*FREGBYTES)(sp)
    FSTORE  fs4,  (20*FREGBYTES)(sp)
    FSTORE  fs5,  (21*FREGBYTES)(sp)
    FSTORE  fs6,  (22*FREGBYTES)(sp)
    FSTORE  fs7,  (23*FREGBYTES)(sp)
    FSTORE  fs8,  (24*FREGBYTES)(sp)
    FSTORE  fs9,  (25*FREGBYTES)(sp)
    FSTORE  fs10, (26*FREGBYTES)(sp)
    FSTORE  fs11, (27*FREGBYTES)(sp)
    csrr    t0, CSR_FCSR
    STORE   t0, (28*REGBYTES)(sp)
#endif

    /* Order all memory operations of the outgoing thread before the switch on
       the out-of-order V5F. */
    fence   iorw, iorw

    /* Store stack pointer in otp->ctx. */
    STORE   sp, CONTEXT_OFFSET(a1)
    /* OLD THREAD CONTEXT SAVE END */

    /* NEW THREAD CONTEXT RESTORE BEGIN */
    LOAD    sp, CONTEXT_OFFSET(a0)

    LOAD    ra,  (0*REGBYTES)(sp)
    LOAD    s0,  (1*REGBYTES)(sp)
    LOAD    s1,  (2*REGBYTES)(sp)
    LOAD    s2,  (3*REGBYTES)(sp)
    LOAD    s3,  (4*REGBYTES)(sp)
    LOAD    s4,  (5*REGBYTES)(sp)
    LOAD    s5,  (6*REGBYTES)(sp)
    LOAD    s6,  (7*REGBYTES)(sp)
    LOAD    s7,  (8*REGBYTES)(sp)
    LOAD    s8,  (9*REGBYTES)(sp)
    LOAD    s9,  (10*REGBYTES)(sp)
    LOAD    s10, (11*REGBYTES)(sp)
    LOAD    s11, (12*REGBYTES)(sp)
#if PORT_RISCV_USE_FPU
    LOAD    t0, (28*REGBYTES)(sp)
    csrw    CSR_FCSR, t0
    FLOAD   fs0,  (16*FREGBYTES)(sp)
    FLOAD   fs1,  (17*FREGBYTES)(sp)
    FLOAD   fs2,  (18*FREGBYTES)(sp)
    FLOAD   fs3,  (19*FREGBYTES)(sp)
    FLOAD   fs4,  (20*FREGBYTES)(sp)
    FLOAD   fs5,  (21*FREGBYTES)(sp)
    FLOAD   fs6,  (22*FREGBYTES)(sp)
    FLOAD   fs7,  (23*FREGBYTES)(sp)
    FLOAD   fs8,  (24*FREGBYTES)(sp)
    FLOAD   fs9,  (25*FREGBYTES)(sp)
    FLOAD   fs10, (26*FREGBYTES)(sp)
    FLOAD   fs11, (27*FREGBYTES)(sp)
#endif

    addi    sp, sp, PORT_RISCV_INTCTX_SIZE
    /* NEW THREAD CONTEXT RESTORE END */

    ret
.option pop

/* --------------------------------------------------------------------------
 * ISR exit path.
 *
 * All vector stubs tail-jump here after the C handler returned. Interrupts
 * are disabled before the restore and stay disabled until mret re-enables
 * them from the saved mstatus.MPIE. The saved mstatus itself has MIE=0, so
 * a nested trap can never observe a half-restored frame.
 * -------------------------------------------------------------------------- */
.section .text
.option push
.option norelax
.align 4
.globl _port_exit_from_isr
_port_exit_from_isr:
    csrc    CSR_MSTATUS, MSTATUS_MIE
    fence.i
    PORT_RESTORE_CONTEXT
    mret
.option pop

#endif /* !defined(__DOXYGEN__) */

/** @} */
