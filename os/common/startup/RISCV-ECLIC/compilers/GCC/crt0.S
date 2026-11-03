/*
    ChibiOS - Copyright (C) 2020 Patrick Seidel
    ChibiOS - Copyright (C) 2021 Stefan Kerkmann
    Adapted for CH32H417 (QingKe V5F), Copyright (C) 2026 ChibiOS contributors.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

/**
 * @file    crt0.S
 * @brief   CH32H417 (QingKe V5F) startup file for ChibiOS.
 *
 * @addtogroup RISCV_ECLIC_STARTUP
 * @{
 */

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

#if !defined(FALSE) || defined(__DOXYGEN__)
#define FALSE                               0
#endif

#if !defined(TRUE) || defined(__DOXYGEN__)
#define TRUE                                1
#endif

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Core initialization switch.
 */
#if !defined(CRT0_INIT_CORE) || defined(__DOXYGEN__)
#define CRT0_INIT_CORE                      TRUE
#endif

/**
 * @brief   Stack segments initialization switch.
 */
#if !defined(CRT0_STACKS_FILL_PATTERN) || defined(__DOXYGEN__)
#define CRT0_STACKS_FILL_PATTERN            0x55555555
#endif

/**
 * @brief   Stack segments initialization switch.
 */
#if !defined(CRT0_INIT_STACKS) || defined(__DOXYGEN__)
#define CRT0_INIT_STACKS                    TRUE
#endif

/**
 * @brief   DATA segment initialization switch.
 */
#if !defined(CRT0_INIT_DATA) || defined(__DOXYGEN__)
#define CRT0_INIT_DATA                      TRUE
#endif

/**
 * @brief   BSS segment initialization switch.
 */
#if !defined(CRT0_INIT_BSS) || defined(__DOXYGEN__)
#define CRT0_INIT_BSS                       TRUE
#endif

/**
 * @brief   RAM areas initialization switch.
 */
#if !defined(CRT0_INIT_RAM_AREAS) || defined(__DOXYGEN__)
#define CRT0_INIT_RAM_AREAS                 TRUE
#endif

/**
 * @brief   .text relocation switch (text runs from ITCM).
 * @note    The hook calls below use "call" instead of "jal": with
 *          .text in ITCM the targets are ~536MB away, outside the
 *          +-1MB range of JAL ("call" is auipc+jalr, +-2GB).
 */
#if !defined(CRT0_INIT_TEXT) || defined(__DOXYGEN__)
#define CRT0_INIT_TEXT                      TRUE
#endif

/**
 * @brief   Constructors invocation switch.
 */
#if !defined(CRT0_CALL_CONSTRUCTORS) || defined(__DOXYGEN__)
#define CRT0_CALL_CONSTRUCTORS              TRUE
#endif

/**
 * @brief   Destructors invocation switch.
 */
#if !defined(CRT0_CALL_DESTRUCTORS) || defined(__DOXYGEN__)
#define CRT0_CALL_DESTRUCTORS               TRUE
#endif

/*===========================================================================*/
/* Code section.                                                             */
/*===========================================================================*/

#if !defined(__DOXYGEN__)

#include "riscv_encoding.h"

.section .crt0, "ax"
.global _start
.type _start, @function
_start:
    /* CH32H417: fall straight through into _crt0_entry. The image is
       linked in its true execution window (V5F XIP at 0x00010000, the
       V3F stub wakes this core there, and the WCH EVT startup jumps
       directly for the same reason). The upstream Nuclei "logical
       address" dance removed here would compute link+0x08000000 (the
       flash programming window of RM ch46) whenever _start sits below
       0x08000000 - always our case - and desynchronize every
       PC-relative reference (la/gp/sp) after the jump. Interrupts are
       disabled by the first instruction of _crt0_entry below. */

/*
 * CRT0 entry point.
 */
.global _crt0_entry
.type _crt0_entry, @function
_crt0_entry:
    /* Disable interrupts globaly */
    csrc CSR_MSTATUS, MSTATUS_MIE

    /* Initialize global pointer gp for linker relaxation.*/
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    /* Initialize stack pointer sp */
    la sp, __process_stack_end__

    /*
     * QingKe V5 core configuration (values from the WCH EVT startup,
     * see the QingKe V5 processor manual for field definitions).
     */
    /* corecfgr: prefetch/benchmark options + IE remap enable (0xBC0). */
    li      t0, 0x1237B3E0
    csrw    CSR_CORECFGR, t0

    /* inestcr: allow up to eight nesting levels (0xBC1). */
    li      t0, 0x07
    csrw    CSR_INESTCR, t0

    /*
     * intsyscr (0x804): HWSTKEN=0 (software stacking, see port design),
     * INESTEN=1, PMTCFG=11 (three preemption priority bits).
     */
    li      t0, 0x0E
    csrw    CSR_INTSYSCR, t0

    /*
     * mstatus: FS=dirty, MPP=machine (every mret stays in M-mode),
     * MPIE=0, MIE=0.
     */
    li      t0, 0x7800
    csrw    CSR_MSTATUS, t0

    /*
     * mtvec: vector table with absolute address entries
     * (MODE0|MODE1), both exceptions and interrupts dispatch
     * through vector_base.
     */
    la      t0, _vector_base
    ori     t0, t0, 3
    csrw    CSR_MTVEC, t0

    /* Enable mcycle and minstret counters */
    csrci   CSR_MCOUNTINHIBIT, 0x5

#if CRT0_INIT_TEXT == TRUE
    /* Copy the .text image from its flash load address to the run
       address (ITCM) and synchronize the instruction stream. This
       must precede every C call: crt0 lives in .crt0/.vectors
       (flash) while all C functions run from the copied image.
       Half-word steps keep it safe for RVC-aligned sections. */
    la      a0, __text_lma
    la      a1, __text_base__
    la      a2, __text_end__
    bgeu    a1, a2, endtextloop
textloop:
    lhu     t0, 0(a0)
    sh      t0, 0(a1)
    addi    a0, a0, 2
    addi    a1, a1, 2
    bltu    a1, a2, textloop
endtextloop:
    fence.i
#endif

#if CRT0_INIT_CORE == TRUE
    /* Core initialization.*/
    call    __core_init
#endif

    /* Early initialization..*/
    call    __early_init

#if CRT0_INIT_STACKS == TRUE
    li      a0, CRT0_STACKS_FILL_PATTERN
    /* Main Stack initialization. Note, it assumes that the
        stack size is a multiple of 4 so the linker file must
        ensure this.*/
    la      a1, __process_stack_base__
    la      a2, __process_stack_end__
psloop:
    bge     a1, a2, endpsloop
    sw      a0, 0(a1)
    addi    a1, a1, 4
    j       psloop
endpsloop:
#endif

#if CRT0_INIT_DATA == TRUE
    /* Data initialization. Note, it assumes that the DATA size
      is a multiple of 4 so the linker file must ensure this.*/
    la      a0, __textdata_base__
    la      a1, __data_base__
    la      a2, __data_end__
dloop:
    bge     a1, a2, enddloop
    lw      a3, 0(a0)
    sw      a3, 0(a1)
    addi    a0, a0, 4
    addi    a1, a1, 4
    j       dloop
enddloop:
#endif

#if CRT0_INIT_BSS == TRUE
    /* BSS initialization. Note, it assumes that the DATA size
      is a multiple of 4 so the linker file must ensure this.*/
    la      a1, __bss_base__
    la      a2, __bss_end__
bloop:
    bge     a1, a2, endbloop
    sw      zero, 0(a1)
    addi    a1, a1, 4
    j       bloop
endbloop:
#endif

#if CRT0_INIT_RAM_AREAS == TRUE
    /* RAM areas initialization.*/
    call    __init_ram_areas
#endif

    /* Late initialization..*/
    call    __late_init

#if CRT0_CALL_CONSTRUCTORS == TRUE
    /* Constructors invocation.*/
    la      s1, __init_array_base__
    la      s2, __init_array_end__
initloop:
    bge     s1, s2, endinitloop
    lw      a0, 0(s1)
    jalr    ra, a0, 0
    addi    s1, s1, 4
    j       initloop
endinitloop:
#endif

    /* Main program invocation, r0 contains the returned value.*/
    call    main

#if CRT0_CALL_DESTRUCTORS == TRUE
    /* Destructors invocation.*/
    la      s1, __fini_array_base__
    la      s2, __fini_array_end__
finiloop:
    bge     s1, s2, endfiniloop
    lw      a0, 0(s1)
    jalr    ra, a0, 0
    addi    s1, s1, 4
    j       finiloop
endfiniloop:
#endif

    /* Branching to the defined exit handler.*/
    call    __default_exit

#endif

/** @} */
