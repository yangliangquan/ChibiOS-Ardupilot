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
 * @brief   Inline-assembly RAM areas initialization switch.
 * @details When TRUE the three RAM areas are copied/zeroed by inline
 *          assembly loops right here instead of by crt1's
 *          __init_ram_areas().  The inline loops take every address from
 *          the instruction stream (la of the __ramN_*__ linker symbols),
 *          so the boot path never reads a relocation table from the
 *          cacheable flash window: crt1's const table lives in .rodata
 *          and could be served stale by the instruction/opcache after a
 *          soft reset that follows a flash reprogramming.  Requires the
 *          __ram0..2_init_text__/init/clear/noinit linker symbols (both
 *          common_ch32.ld and common_ch32_boot.ld define them) and
 *          4-aligned area boundaries (both scripts ALIGN(4) every area).
 *          Define FALSE to fall back to crt1.c's __init_ram_areas().
 */
#if !defined(CRT0_ASM_RAM_AREAS) || defined(__DOXYGEN__)
#define CRT0_ASM_RAM_AREAS                  TRUE
#endif

/**
 * @brief   .text relocation (text runs from ITCM).
 * @details Always compiled.  The copy loop is guarded at run time by
 *          __text_lma != __text_base__: images that link .text with a
 *          run address equal to the load address (XIP from CodeFlash
 *          or QSPI, common_ch32.ld) skip it, images that link .text
 *          into ITCM (common_ch32_boot.ld) copy it.  No build-time
 *          switch is needed.
 * @note    The hook calls below use "call" instead of "jal": with
 *          .text in ITCM the targets are ~536MB away, outside the
 *          +-1MB range of JAL ("call" is auipc+jalr, +-2GB).
 */

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

/**
 * @brief   Instruction cache master switch.
 * @details Programs cache_strtg_ctlr (CSR 0xBC2) before the DATA/BSS
 *          segments are initialized: TRUE turns the cache on
 *          (IC_DISABLE=0), FALSE leaves it off (the reset state).
 * @note    Only the V5F core has an instruction cache, the V3F core does
 *          not implement the CSR and has its own startup file.
 * @note    This file does not see mcuconf.h/halconf.h, override the
 *          switches of this section from the command line instead:
 *          UADEFS (rule ADEFS) for the assembler, UDEFS as well when C
 *          code needs to read the same value.
 */
#if !defined(PORT_RISCV_ICACHE_ENABLE) || defined(__DOXYGEN__)
#define PORT_RISCV_ICACHE_ENABLE            TRUE
#endif

/**
 * @brief   Allows instruction caching of 0x00000000-0x1FFFFFFF.
 * @note    The code flash window of this port (0x00010000 and up, see
 *          CH32H417_FLASH.ld) lives in this space, this is the switch
 *          that matters when code runs from XIP flash.
 */
#if !defined(PORT_RISCV_ICACHE_CODE_AREA) || defined(__DOXYGEN__)
#define PORT_RISCV_ICACHE_CODE_AREA         TRUE
#endif

/**
 * @brief   Allows instruction caching of 0x20000000-0x3FFFFFFF.
 * @note    ITCM, DTCM and the shared SRAM live in this space. They are
 *          zero-wait memories, keeping them out of the cache leaves the
 *          whole 32KB array to the flash window.
 */
#if !defined(PORT_RISCV_ICACHE_SRAM_AREA) || defined(__DOXYGEN__)
#define PORT_RISCV_ICACHE_SRAM_AREA         FALSE
#endif

/**
 * @brief   Allows instruction caching of 0x60000000-0x7FFFFFFF.
 * @note    External memory window (QSPI XIP, FMC), opt-in: enable it
 *          only when code is actually executed from there.
 */
#if !defined(PORT_RISCV_ICACHE_MEM0_AREA) || defined(__DOXYGEN__)
#define PORT_RISCV_ICACHE_MEM0_AREA         FALSE
#endif

/**
 * @brief   Allows instruction caching of 0x80000000-0x9FFFFFFF.
 * @note    Second external memory window, opt-in like MEM0.
 */
#if !defined(PORT_RISCV_ICACHE_MEM1_AREA) || defined(__DOXYGEN__)
#define PORT_RISCV_ICACHE_MEM1_AREA         FALSE
#endif

/*===========================================================================*/
/* Code section.                                                             */
/*===========================================================================*/

#if !defined(__DOXYGEN__)

#include "riscv_encoding.h"

/*
 * cache_strtg_ctlr value composed from the switches above, the bit
 * definitions come from riscv_encoding.h. Bit 0 is the reserved
 * read-only bit that reads back as one, it is kept in the value for
 * clarity, writes to it are ignored.
 * The bit selection uses the preprocessor rather than a ternary
 * because the result is consumed by the assembler.
 */
#if PORT_RISCV_ICACHE_ENABLE
#define ICACHE_CTLR_DIS_BIT     0
#else
#define ICACHE_CTLR_DIS_BIT     CACHE_STRTG_CTLR_IC_DISABLE
#endif

#if PORT_RISCV_ICACHE_CODE_AREA
#define ICACHE_CTLR_CODE_BIT    CACHE_STRTG_CTLR_IC_CODE
#else
#define ICACHE_CTLR_CODE_BIT    0
#endif

#if PORT_RISCV_ICACHE_SRAM_AREA
#define ICACHE_CTLR_SRAM_BIT    CACHE_STRTG_CTLR_IC_SRAM
#else
#define ICACHE_CTLR_SRAM_BIT    0
#endif

#if PORT_RISCV_ICACHE_MEM0_AREA
#define ICACHE_CTLR_MEM0_BIT    CACHE_STRTG_CTLR_IC_MEM0
#else
#define ICACHE_CTLR_MEM0_BIT    0
#endif

#if PORT_RISCV_ICACHE_MEM1_AREA
#define ICACHE_CTLR_MEM1_BIT    CACHE_STRTG_CTLR_IC_MEM1
#else
#define ICACHE_CTLR_MEM1_BIT    0
#endif

#define ICACHE_CTLR_VALUE       \
  (1 | ICACHE_CTLR_DIS_BIT | ICACHE_CTLR_CODE_BIT | ICACHE_CTLR_SRAM_BIT | \
   ICACHE_CTLR_MEM0_BIT | ICACHE_CTLR_MEM1_BIT)

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

    /* mscratch is the interrupt-stack switch sentinel: zero while running in
       thread mode, the interrupted thread's frame pointer while servicing an
       interrupt (see PORT_IRQ_ENTRY in irq_stubs.h). It must start clear and
       it must stay clear until the first trap is taken. */
    csrw    CSR_MSCRATCH, zero

    /*
     * QingKe V5 core configuration (values from the WCH EVT startup,
     * see the QingKe V5 processor manual for field definitions).
     */
    /* corecfgr: prefetch/benchmark options + IE remap enable (0xBC0). */
    li      t0, 0x1237B3E0
    csrw    CSR_CORECFGR, t0

    /*
     * cache_strtg_ctlr (0xBC2): instruction cache master switch plus the
     * per address space strategies, composed at the top of this file.
     * The array must be invalidated with the cache DISABLED: an
     * invalidation written while the array was still enabled did not
     * stick on this core (stale lines of a previous boot survived a
     * soft reset and fed the RAM-area relocation a stale table), so the
     * disable -> invalidate -> enable order below is mandatory.  A
     * fence.i orders the change into the instruction stream before any
     * further fetch.
     */
#if PORT_RISCV_ICACHE_ENABLE
    li      t0, (1 | CACHE_STRTG_CTLR_IC_DISABLE)
    csrw    CSR_CACHE_STRTG_CTLR, t0
    li      t0, (OPCACHE_CTLR_IDX_MODE_ADDR | OPCACHE_CTLR_OP_ICACHE_INVAL)
    csrw    CSR_OPCACHE_CTLR, t0
    li      t0, ICACHE_CTLR_VALUE
    csrw    CSR_CACHE_STRTG_CTLR, t0
    fence.i
#endif

    /*
     * inestcr (0xBC1): maximum nesting level. Must match
     * PORT_RISCV_NESTING_LEVELS, which chcore.h uses to size the per-thread
     * interrupt stack reserve.
     */
    li      t0, (PORT_RISCV_NESTING_LEVELS - 1)
    csrw    CSR_INESTCR, t0

    /*
     * intsyscr (0x804): HWSTKEN=0 (software stacking, the RTOS needs
     * per-thread stacks), INESTEN=1, PMTCFG=PORT_RISCV_PREEMPT_BITS.
     */
    li      t0, ((PORT_RISCV_PREEMPT_BITS << 2) | (1 << 1))
    csrw    CSR_INTSYSCR, t0

    /*
     * mstatus: MPP=machine (every mret stays in M-mode), MPIE=0, MIE=0.
     * FS=Initial (01) when the F extension is built in, so the FPU is
     * usable and the port can save/restore the FP register file. FS is
     * left OFF otherwise.
     */
#if PORT_RISCV_USE_FPU
    li      t0, 0x3800
#else
    li      t0, 0x1800
#endif
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

    /* Copy the .text image from its flash load address to the run
       address (ITCM) and synchronize the instruction stream. This
       must precede every C call: crt0 lives in .crt0/.vectors
       (flash) while all C functions run from the copied image.
       Half-word steps keep it safe for RVC-aligned sections.
       XIP images (VMA == LMA) take the beq exit. */
    la      a0, __text_lma
    la      a1, __text_base__
    la      a2, __text_end__
    beq     a0, a1, endtextloop
    bgeu    a1, a2, endtextloop
textloop:
    lhu     t0, 0(a0)
    sh      t0, 0(a1)
    addi    a0, a0, 2
    addi    a1, a1, 2
    bltu    a1, a2, textloop
endtextloop:
    fence.i

#if CRT0_INIT_CORE == TRUE
    /* Core initialization.*/
    call    __core_init
#endif

    /* Early initialization..*/
    call    __early_init

#if CRT0_INIT_STACKS == TRUE
    li      a0, CRT0_STACKS_FILL_PATTERN
    /* Process stack initialization. Note, it assumes that the
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

    /* Independent interrupt stack initialization. Filling it with the same
       pattern makes the worst-case ISR stack watermark measurable at run
       time; the linker script guarantees a size multiple of 16. */
    la      a1, __irq_stack_base__
    la      a2, __irq_stack_end__
isloop:
    bge     a1, a2, endisloop
    sw      a0, 0(a1)
    addi    a1, a1, 4
    j       isloop
endisloop:
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
    /* RAM areas initialization. */
#if CRT0_ASM_RAM_AREAS == TRUE
    /* Inline relocation: copy __ramN_init_text__.. into
       __ramN_init__..__ramN_clear__, zero __ramN_clear__..__ramN_noinit__.
       Word steps: every area boundary is ALIGN(4)-ed by the linker
       scripts, so all ranges are multiples of four bytes. */
    .macro  CRT0_RAM_AREA_INIT name
    la      t0, __\name\()_init_text__
    la      t1, __\name\()_init__
    la      t2, __\name\()_clear__
    la      t3, __\name\()_noinit__
1:  bgeu    t1, t2, 2f
    lw      t4, 0(t0)
    sw      t4, 0(t1)
    addi    t0, t0, 4
    addi    t1, t1, 4
    j       1b
2:  bgeu    t1, t3, 3f
    sw      zero, 0(t1)
    addi    t1, t1, 4
    j       2b
3:
    .endm
    CRT0_RAM_AREA_INIT ram0
    CRT0_RAM_AREA_INIT ram1
    CRT0_RAM_AREA_INIT ram2
    /* The ITCM/DTCM copies now hold executable code: sync the
       instruction stream before anything is called from them. */
    fence.i
#else
    call    __init_ram_areas
#endif
#endif

    /* All RAM images (data/bss/ram areas) are in place: order the
       out-of-order V5F core's buffered stores before any C code runs
       from them (RM ch4.1 recommends explicit fences). */
    fence   iorw, iorw

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
