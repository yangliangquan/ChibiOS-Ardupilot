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
 * @file    RISCV/chcore.h
 * @brief   Port related template macros and structures.
 *
 * @addtogroup port_core
 * @{
 */

#ifndef CHCORE_H
#define CHCORE_H

/*===========================================================================*/
/* Module constants.                                                         */
/*===========================================================================*/

/**
 * @name    Port Capabilities and Constants
 * @{
 */
/**
 * @brief   This port supports a realtime counter.
 */
#define PORT_SUPPORTS_RT TRUE

/**
 * @brief   Natural alignment constant.
 * @note    It is the minimum alignment for pointer-size variables.
 */
#define PORT_NATURAL_ALIGN sizeof(void *)

/**
 * @brief   Stack alignment constant.
 * @note    It is the alignment required for the stack pointer.
 */
#define PORT_STACK_ALIGN sizeof(stkalign_t)

/**
 * @brief   Working Areas alignment constant.
 * @note    It is the alignment to be enforced for thread working areas.
 */
#define PORT_WORKING_AREA_ALIGN sizeof(stkalign_t)
/** @} */

/**
 * @name    Architecture and Compiler
 * @{
 */
/**
 * @brief   Macro defining a RISCV architecture.
 */
#define PORT_ARCHITECTURE_RISCV

/**
 * @brief   Macro defining the specific RISCV architecture.
 */
#define PORT_ARCHITECTURE_RISCV_ECLIC

/**
 * @brief   Name of the implemented architecture.
 */
#define PORT_ARCHITECTURE_NAME          "RISC-V"

/**
 * @brief   Name of the architecture variant.
 */
#define PORT_CORE_VARIANT_NAME          "QingKe V5F"

/**
 * @brief   Compiler name and version.
 */
#if defined(__GNUC__) || defined(__DOXYGEN__)
#define PORT_COMPILER_NAME "GCC " __VERSION__

#else
#error "unsupported compiler"
#endif

/**
 * @brief   Port-specific information string.
 */
#define PORT_INFO "RV32IMAFC+B - QingKe V5F PFIC/ECLIC"
/** @} */

/* Inclusion of the RISC-V implementation specific parameters.*/
#if !defined(_FROM_ASM_)

#include "ch32h417params.h"
#include "riscv_encoding.h"

#endif

/*===========================================================================*/
/* Module pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @brief   Stack size for the system idle thread.
 * @details This size depends on the idle thread implementation, usually
 *          the idle thread should take no more space than those reserved
 *          by @p PORT_INT_REQUIRED_STACK.
 */
#if !defined(PORT_IDLE_THREAD_STACK_SIZE) || defined(__DOXYGEN__)
#define PORT_IDLE_THREAD_STACK_SIZE 64
#endif

/**
 * @brief   Per-thread stack overhead for interrupts servicing.
 * @details This constant is used in the calculation of the correct working
 *          area size. It accounts for the worst-case interrupt nesting depth
 *          (each level consumes one @p port_extctx frame) plus a margin for
 *          the C handler stacks.
 */
#if !defined(PORT_INT_REQUIRED_STACK) || defined(__DOXYGEN__)
#define PORT_INT_REQUIRED_STACK                                             \
  (PORT_RISCV_NESTING_LEVELS * (PORT_RISCV_EXTCTX_SIZE +                     \
                                PORT_RISCV_IRQ_ARG_SIZE +                     \
                                PORT_RISCV_ISR_C_STACK_SIZE))
#endif

/**
 * @brief   Stack space reserved for the C code of an interrupt handler.
 * @note    The handler runs on the stack of the interrupted thread, the
 *          value is a safety margin on top of the interrupt frame itself.
 */
#if !defined(PORT_RISCV_ISR_C_STACK_SIZE) || defined(__DOXYGEN__)
#define PORT_RISCV_ISR_C_STACK_SIZE       64U
#endif

/**
 * @brief   Enables an alternative timer implementation.
 * @details Usually the port uses a timer interface defined in the file
 *          @p chcore_timer.h, if this option is enabled then the file
 *          @p chcore_timer_alt.h is included instead.
 */
#if !defined(PORT_USE_ALT_TIMER) || defined(__DOXYGEN__)
#define PORT_USE_ALT_TIMER FALSE
#endif

/**
 * @brief   Enables a "wait for interrupt" instruction in the idle loop.
 */
#if !defined(PORT_RISCV_ENABLE_WFI_IDLE) || defined(__DOXYGEN__)
#define PORT_RISCV_ENABLE_WFI_IDLE FALSE
#endif

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if defined(__riscv_32e)
#error "RV32E register file is not supported by this port"
#endif

/*===========================================================================*/
/* Module data structures and types.                                         */
/*===========================================================================*/

/* The following code is not processed when the file is included from an
   asm module.*/
#if !defined(_FROM_ASM_)

/**
 * @brief   Type of stack and memory alignment enforcement.
 * @note    In this architecture the stack alignment is enforced to 128 bits.
 */
typedef struct {
  uint8_t padding[16];
} stkalign_t ALIGNED_VAR(16);

/**
 * @brief   Interrupt saved context.
 * @details This structure represents the stack frame saved during a
 *          preemption-capable interrupt handler.
 * @note    GP is not saved because it is assumed to be immutable
 *          during the system life cycle.
 */
struct port_extctx {
  uint32_t ra;
  uint32_t tp;
  uint32_t t0;
  uint32_t t1;
  uint32_t t2;
  uint32_t a0;
  uint32_t a1;
  uint32_t a2;
  uint32_t a3;
  uint32_t a4;
  uint32_t a5;
  uint32_t mepc;
  uint32_t mcause;
  uint32_t mstatus;
  uint32_t a6;
  uint32_t a7;
  uint32_t t3;
  uint32_t t4;
  uint32_t t5;
  uint32_t t6;
#if PORT_RISCV_USE_FPU
  uint32_t ft0;
  uint32_t ft1;
  uint32_t ft2;
  uint32_t ft3;
  uint32_t ft4;
  uint32_t ft5;
  uint32_t ft6;
  uint32_t ft7;
  uint32_t ft8;
  uint32_t ft9;
  uint32_t ft10;
  uint32_t ft11;
  uint32_t fa0;
  uint32_t fa1;
  uint32_t fa2;
  uint32_t fa3;
  uint32_t fa4;
  uint32_t fa5;
  uint32_t fa6;
  uint32_t fa7;
  uint32_t fcsr;
  uint32_t fp_padding[3];
#endif
};

/**
 * @brief   System saved context.
 * @details This structure represents the inner stack frame during a context
 *          switching.
 * @note    GP and TP are not saved because those are assumed to be immutable
 *          during the system life cycle.
 */
struct port_intctx {
  uint32_t ra;
  uint32_t s0;
  uint32_t s1;
  uint32_t s2;
  uint32_t s3;
  uint32_t s4;
  uint32_t s5;
  uint32_t s6;
  uint32_t s7;
  uint32_t s8;
  uint32_t s9;
  uint32_t s10;
  uint32_t s11;
  uint32_t padding[3];
#if PORT_RISCV_USE_FPU
  uint32_t fs0;
  uint32_t fs1;
  uint32_t fs2;
  uint32_t fs3;
  uint32_t fs4;
  uint32_t fs5;
  uint32_t fs6;
  uint32_t fs7;
  uint32_t fs8;
  uint32_t fs9;
  uint32_t fs10;
  uint32_t fs11;
  uint32_t fcsr;
  uint32_t fp_padding[3];
#endif
};

#ifdef __cplusplus
static_assert(sizeof(struct port_extctx) == PORT_RISCV_EXTCTX_SIZE,
              "port_extctx size mismatch");
static_assert(sizeof(struct port_intctx) == PORT_RISCV_INTCTX_SIZE,
              "port_intctx size mismatch");
static_assert((sizeof(struct port_extctx) % 16U) == 0U,
              "port_extctx alignment mismatch");
static_assert((sizeof(struct port_intctx) % 16U) == 0U,
              "port_intctx alignment mismatch");
static_assert(offsetof(struct port_extctx, ra) == PORT_RISCV_EXTCTX_RA_OFF,
              "port_extctx.ra offset mismatch");
static_assert(offsetof(struct port_extctx, tp) == PORT_RISCV_EXTCTX_TP_OFF,
              "port_extctx.tp offset mismatch");
static_assert(offsetof(struct port_extctx, t0) == PORT_RISCV_EXTCTX_T0_OFF,
              "port_extctx.t0 offset mismatch");
static_assert(offsetof(struct port_extctx, t1) == PORT_RISCV_EXTCTX_T1_OFF,
              "port_extctx.t1 offset mismatch");
static_assert(offsetof(struct port_extctx, t2) == PORT_RISCV_EXTCTX_T2_OFF,
              "port_extctx.t2 offset mismatch");
static_assert(offsetof(struct port_extctx, a0) == PORT_RISCV_EXTCTX_A0_OFF,
              "port_extctx.a0 offset mismatch");
static_assert(offsetof(struct port_extctx, a6) == PORT_RISCV_EXTCTX_A6_OFF,
              "port_extctx.a6 offset mismatch");
static_assert(offsetof(struct port_extctx, a7) == PORT_RISCV_EXTCTX_A7_OFF,
              "port_extctx.a7 offset mismatch");
static_assert(offsetof(struct port_extctx, mepc) == PORT_RISCV_EXTCTX_MEPC_OFF,
              "port_extctx.mepc offset mismatch");
static_assert(offsetof(struct port_extctx, mcause) == PORT_RISCV_EXTCTX_MCAUSE_OFF,
              "port_extctx.mcause offset mismatch");
static_assert(offsetof(struct port_extctx, mstatus) == PORT_RISCV_EXTCTX_MSTATUS_OFF,
              "port_extctx.mstatus offset mismatch");
static_assert(offsetof(struct port_extctx, t3) == PORT_RISCV_EXTCTX_T3_OFF,
              "port_extctx.t3 offset mismatch");
static_assert(offsetof(struct port_extctx, t6) == PORT_RISCV_EXTCTX_T6_OFF,
              "port_extctx.t6 offset mismatch");
static_assert(offsetof(struct port_intctx, ra) == PORT_RISCV_INTCTX_RA_OFF,
              "port_intctx.ra offset mismatch");
static_assert(offsetof(struct port_intctx, s0) == PORT_RISCV_INTCTX_S0_OFF,
              "port_intctx.s0 offset mismatch");
static_assert(offsetof(struct port_intctx, s11) == PORT_RISCV_INTCTX_S11_OFF,
              "port_intctx.s11 offset mismatch");
#else
_Static_assert(sizeof(struct port_extctx) == PORT_RISCV_EXTCTX_SIZE,
               "port_extctx size mismatch");
_Static_assert(sizeof(struct port_intctx) == PORT_RISCV_INTCTX_SIZE,
               "port_intctx size mismatch");
_Static_assert((sizeof(struct port_extctx) % 16U) == 0U,
               "port_extctx alignment mismatch");
_Static_assert((sizeof(struct port_intctx) % 16U) == 0U,
               "port_intctx alignment mismatch");
_Static_assert(offsetof(struct port_extctx, ra) == PORT_RISCV_EXTCTX_RA_OFF,
               "port_extctx.ra offset mismatch");
_Static_assert(offsetof(struct port_extctx, tp) == PORT_RISCV_EXTCTX_TP_OFF,
               "port_extctx.tp offset mismatch");
_Static_assert(offsetof(struct port_extctx, t0) == PORT_RISCV_EXTCTX_T0_OFF,
               "port_extctx.t0 offset mismatch");
_Static_assert(offsetof(struct port_extctx, t1) == PORT_RISCV_EXTCTX_T1_OFF,
               "port_extctx.t1 offset mismatch");
_Static_assert(offsetof(struct port_extctx, t2) == PORT_RISCV_EXTCTX_T2_OFF,
               "port_extctx.t2 offset mismatch");
_Static_assert(offsetof(struct port_extctx, a0) == PORT_RISCV_EXTCTX_A0_OFF,
               "port_extctx.a0 offset mismatch");
_Static_assert(offsetof(struct port_extctx, a6) == PORT_RISCV_EXTCTX_A6_OFF,
               "port_extctx.a6 offset mismatch");
_Static_assert(offsetof(struct port_extctx, a7) == PORT_RISCV_EXTCTX_A7_OFF,
               "port_extctx.a7 offset mismatch");
_Static_assert(offsetof(struct port_extctx, mepc) == PORT_RISCV_EXTCTX_MEPC_OFF,
               "port_extctx.mepc offset mismatch");
_Static_assert(offsetof(struct port_extctx, mcause) == PORT_RISCV_EXTCTX_MCAUSE_OFF,
               "port_extctx.mcause offset mismatch");
_Static_assert(offsetof(struct port_extctx, mstatus) == PORT_RISCV_EXTCTX_MSTATUS_OFF,
               "port_extctx.mstatus offset mismatch");
_Static_assert(offsetof(struct port_extctx, t3) == PORT_RISCV_EXTCTX_T3_OFF,
               "port_extctx.t3 offset mismatch");
_Static_assert(offsetof(struct port_extctx, t6) == PORT_RISCV_EXTCTX_T6_OFF,
               "port_extctx.t6 offset mismatch");
_Static_assert(offsetof(struct port_intctx, ra) == PORT_RISCV_INTCTX_RA_OFF,
               "port_intctx.ra offset mismatch");
_Static_assert(offsetof(struct port_intctx, s0) == PORT_RISCV_INTCTX_S0_OFF,
               "port_intctx.s0 offset mismatch");
_Static_assert(offsetof(struct port_intctx, s11) == PORT_RISCV_INTCTX_S11_OFF,
               "port_intctx.s11 offset mismatch");
#endif

/**
 * @brief   Platform dependent part of the @p thread_t structure.
 * @details This structure usually contains just the saved stack pointer
 *          defined as a pointer to a @p port_intctx structure.
 */
struct port_context {
  struct port_intctx *sp;
};

/**
 * @brief   Initializes the floating point part of a thread context.
 * @details A thread starts with a well defined floating point register file,
 *          this matters for the threads whose working area is not zero
 *          initialized (dynamically allocated working areas).
 *
 * @param[in] ctxp      pointer to the thread context to be initialized
 */
static inline void _port_init_fp_context(struct port_intctx *ctxp) {

#if PORT_RISCV_USE_FPU
  ctxp->fs0 = 0U;
  ctxp->fs1 = 0U;
  ctxp->fs2 = 0U;
  ctxp->fs3 = 0U;
  ctxp->fs4 = 0U;
  ctxp->fs5 = 0U;
  ctxp->fs6 = 0U;
  ctxp->fs7 = 0U;
  ctxp->fs8 = 0U;
  ctxp->fs9 = 0U;
  ctxp->fs10 = 0U;
  ctxp->fs11 = 0U;
  ctxp->fcsr = 0U;
#endif
}

#endif /* !defined(_FROM_ASM_) */

/*===========================================================================*/
/* Module macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Platform dependent part of the @p chThdCreateI() API.
 * @details This code usually setup the context switching frame represented
 *          by an @p port_intctx structure.
 */
#define PORT_SETUP_CONTEXT(tp, wbase, wtop, pf, arg)                           \
  {                                                                            \
    (tp)->ctx.sp = (struct port_intctx *)((uint8_t *)(wtop) -                  \
                                          sizeof(struct port_intctx));         \
    (tp)->ctx.sp->ra = (uint32_t)_port_thread_start;                           \
    (tp)->ctx.sp->s0 = (uint32_t)(pf);                                         \
    (tp)->ctx.sp->s1 = (uint32_t)(arg);                                        \
    _port_init_fp_context((tp)->ctx.sp);                                       \
  }

/**
 * @brief   Computes the thread working area global size.
 * @note    There is no need to perform alignments in this macro.
 */
#define PORT_WA_SIZE(n)                                                        \
  (sizeof(struct port_intctx) + sizeof(struct port_extctx) + ((size_t)(n)) +   \
   ((size_t)(PORT_INT_REQUIRED_STACK)))

/**
 * @brief   Static working area allocation.
 * @details This macro is used to allocate a static thread working area
 *          aligned as both position and size.
 *
 * @param[in] s         the name to be assigned to the stack array
 * @param[in] n         the stack size to be assigned to the thread
 */
#define PORT_WORKING_AREA(s, n)                                                \
  stkalign_t s[THD_WORKING_AREA_SIZE(n) / sizeof(stkalign_t)]

/**
 * @brief   Priority level verification macro.
 * @note    The PFIC encodes four priority bits, the drivers use the
 *          0..15 range.
 */
#define PORT_IRQ_IS_VALID_PRIORITY(n)                                         \
  ((n) <= PORT_RISCV_LOWEST_IRQ_PRIORITY)

/**
 * @brief   Priority level verification macro.
 */
#define PORT_IRQ_IS_VALID_KERNEL_PRIORITY(n) PORT_IRQ_IS_VALID_PRIORITY(n)

/**
 * @brief   IRQ prologue code.
 * @details This macro must be inserted at the start of all IRQ handlers
 *          enabled to invoke system APIs.
 * @note    The interrupt context is saved by the common entry point before
 *          the handler is invoked, nothing has to be done here.
 */
#define PORT_IRQ_PROLOGUE()

/**
 * @brief   IRQ epilogue code.
 * @details This macro must be inserted at the end of all IRQ handlers
 *          enabled to invoke system APIs.
 * @note    The macro only records that a reschedule is required, the actual
 *          context switch is performed by the interrupt exit path, at the
 *          outermost interrupt level, so that the switch is atomic with
 *          respect to the interrupt exit. This is the RISC-V equivalent of
 *          the ARMv7-M PendSV mechanism.
 */
#define PORT_IRQ_EPILOGUE() do {                                               \
                            if (chSchIsPreemptionRequired()) {                 \
                              _port_preempt_request();                        \
                            }                                                  \
                          } while (0)

/**
 * @brief   IRQ handler function declaration.
 * @note    @p id can be a function name or a vector number depending on the
 *          port implementation.
 */
#ifdef __cplusplus
#define PORT_IRQ_HANDLER(id) extern "C" void id(void)
#else
#define PORT_IRQ_HANDLER(id) void id(void)
#endif

/**
 * @brief   Fast IRQ handler function declaration.
 * @note    @p id can be a function name or a vector number depending on the
 *          port implementation.
 * @note    Fast handlers are still invoked through the software-stacking
 *          vector stubs, so they must obey the standard C ABI and must not
 *          call OS APIs.
 */
#ifdef __cplusplus
#define PORT_FAST_IRQ_HANDLER(id) extern "C" void id(void)
#else
#define PORT_FAST_IRQ_HANDLER(id) void id(void)
#endif

/**
 * @brief   Performs a context switch between two threads.
 * @details This is the most critical code in any port, this function
 *          is responsible for the context switch between 2 threads.
 * @note    The implementation of this code affects <b>directly</b> the context
 *          switch performance so optimize here as much as you can.
 *
 * @param[in] ntp       the thread to be switched in
 * @param[in] otp       the thread to be switched out
 */
#if !CH_DBG_ENABLE_STACK_CHECK || defined(__DOXYGEN__)
#define port_switch(ntp, otp) _port_switch(ntp, otp)
#else
#define port_switch(ntp, otp)                                                  \
  {                                                                            \
    register struct port_intctx *sp asm("%sp");                                \
    if ((stkalign_t *)(sp - 1) < otp->wabase) {                                \
      chSysHalt("stackoverflow");                                              \
    }                                                                          \
    _port_switch(ntp, otp);                                                    \
  }
#endif

/**
 * @brief   Returns a word representing a critical section status.
 *
 * @return              The critical section status.
 */
#define port_get_lock_status() port_get_irq_status()

/**
 * @brief   Determines if in a critical section.
 *
 * @param[in] sts       status word returned by @p port_get_lock_status()
 * @return              The current status.
 * @retval false        if running outside a critical section.
 * @retval true         if running within a critical section.
 */
#define port_is_locked(sts) !port_irq_enabled(sts)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

/* The following code is not processed when the file is included from an
   asm module.*/
#if !defined(_FROM_ASM_)

#ifdef __cplusplus
extern "C" {
#endif
void _port_switch(thread_t *ntp, thread_t *otp);
void _port_exit_from_isr(void);
void _port_thread_start(void);
bool _port_irq_preempt_pending(void);
#ifdef __cplusplus
}
#endif

/**
 * @brief   Reschedule request flag.
 * @details Set by @p PORT_IRQ_EPILOGUE() when a higher priority thread is
 *          ready, consumed by the interrupt exit path which performs the
 *          context switch.
 */
extern volatile bool port_preempt_pending;

#endif /* !defined(_FROM_ASM_) */

/*===========================================================================*/
/* Module inline functions.                                                  */
/*===========================================================================*/

/* The following code is not processed when the file is included from an
   asm module.*/
#if !defined(_FROM_ASM_)

/**
 * @brief   Port-related initialization code.
 */
static inline void port_init(os_instance_t *oip) {

  (void)oip;
}

/**
 * @brief   Returns a word encoding the current interrupts status.
 *
 * @return              The interrupts status.
 */
static inline syssts_t port_get_irq_status(void) {
  syssts_t sts;
  __asm__ volatile("csrr %0, mstatus" : "=r"(sts));
  return sts;
}

/**
 * @brief   Checks the interrupt status.
 *
 * @param[in] sts       the interrupt status word
 *
 * @return              The interrupt status.
 * @retval false        the word specified a disabled interrupts status.
 * @retval true         the word specified an enabled interrupts status.
 */
static inline bool port_irq_enabled(syssts_t sts) { return sts & MSTATUS_MIE; }

/**
 * @brief   Returns the current interrupt nesting level.
 * @details The PFIC global interrupt state register mirrors the nesting
 *          state of the core interrupt controller (CSR 0xBC1, NEST_STA),
 *          the value is (1 << level) - 1 so the level is the number of
 *          trailing one bits. Zero means that no interrupt is being
 *          serviced.
 *
 * @return              The nesting level, zero when not in an ISR.
 */
static inline uint32_t port_isr_nesting_level(void) {
  uint32_t neststa = NVIC->GISR & PFIC_GISR_NESTSTA_MASK;
  uint32_t level = 0U;

  while (neststa != 0U) {
    neststa >>= 1;
    level++;
  }

  return level;
}

/**
 * @brief   Determines the current execution context.
 *
 * @return              The execution context.
 * @retval false        not running in ISR mode.
 * @retval true         running in ISR mode.
 */
static inline bool port_is_isr_context(void) {
  /* PFIC GISR.NESTSTA (bits 7:0): nesting depth mask, 0 when not in an ISR. */
  return (NVIC->GISR & PFIC_GISR_NESTSTA_MASK) != 0U;
}

/**
 * @brief   Checks the interrupt nesting overflow flag.
 * @details The flag is set by the hardware when an interrupt is taken while
 *          the maximum nesting level has already been reached, the condition
 *          is not recoverable because the CPU stack cannot be unwound.
 *
 * @return              The overflow state.
 * @retval true         the nesting levels have been overflowed.
 * @retval false        no overflow.
 */
static inline bool port_isr_nesting_overflow(void) {
  uint32_t val;

  __asm__ volatile("csrr %0, 0xBC1" : "=r"(val));

  return (val & (1UL << 30)) != 0U;
}

/**
 * @brief   Kernel-lock action.
 * @details Usually this function just disables interrupts but may perform more
 *          actions.
 * @note    No memory barrier is required, the RISC-V trap is ordered with
 *          respect to the memory accesses performed before it, so an
 *          interrupt handler cannot observe a partially completed critical
 *          section.
 */
static inline void port_lock(void) {
  __asm__ volatile("csrc %0, %1"
                   : : "i"(CSR_MSTATUS), "i"(MSTATUS_MIE) : "memory");
}

/**
 * @brief   Kernel-unlock action.
 * @details Usually this function just enables interrupts but may perform more
 *          actions.
 */
static inline void port_unlock(void) {
  __asm__ volatile("csrs %0, %1" : : "i"(CSR_MSTATUS), "i"(MSTATUS_MIE) : "memory");
}

/**
 * @brief   Kernel-lock action from an interrupt handler.
 * @details This function is invoked before invoking I-class APIs from
 *          interrupt handlers. The implementation is architecture dependent,
 *          in its simplest form it is void.
 */
static inline void port_lock_from_isr(void) { port_lock(); }

/**
 * @brief   Kernel-unlock action from an interrupt handler.
 * @details This function is invoked after invoking I-class APIs from interrupt
 *          handlers. The implementation is architecture dependent, in its
 *          simplest form it is void.
 */
static inline void port_unlock_from_isr(void) { port_unlock(); }

/**
 * @brief   Records that a reschedule is required.
 * @note    The flag is consumed by the interrupt exit path, which performs
 *          the actual context switch.
 */
static inline void _port_preempt_request(void) {

  port_preempt_pending = true;
}

/**
 * @brief   Disables all the interrupt sources.
 * @note    Of course non-maskable interrupt sources are not included.
 */
static inline void port_disable(void) {
  __asm__ volatile("csrc %0, %1"
                   : : "i"(CSR_MSTATUS), "i"(MSTATUS_MIE) : "memory");
}

/**
 * @brief   Disables the interrupt sources below kernel-level priority.
 * @note    Interrupt sources above kernel level remains enabled.
 */
static inline void port_suspend(void) {
  __asm__ volatile("csrc %0, %1"
                   : : "i"(CSR_MSTATUS), "i"(MSTATUS_MIE) : "memory");
}

/**
 * @brief   Enables all the interrupt sources.
 */
static inline void port_enable(void) {
  __asm__ volatile("csrs %0, %1" : : "i"(CSR_MSTATUS), "i"(MSTATUS_MIE) : "memory");
}

/**
 * @details The function is meant to return when an interrupt becomes pending.
 *          The simplest implementation is an empty function or macro but this
 *          would not take advantage of architecture-specific power saving
 *          modes.
 */
static inline void port_wait_for_interrupt(void) {
#if PORT_RISCV_ENABLE_WFI_IDLE
  __WFI();
#endif
}

/**
 * @brief   Returns the current value of the realtime counter.
 * @note    QingKe V5F implements only the 32-bit ucycle counter
 *          (no *h registers), the value is zero-extended to 64 bits.
 *
 * @return  The realtime counter value.
 */
static inline rtcnt_t port_rt_get_counter_value(void) {
  uint32_t cyc;
  __asm__ volatile("csrr %0, cycle" : "=r"(cyc));
  return (rtcnt_t)cyc;
}

#endif /* !defined(_FROM_ASM_) */

/*===========================================================================*/
/* Module late inclusions.                                                   */
/*===========================================================================*/

/* The following code is not processed when the file is included from an
   asm module.*/
#if !defined(_FROM_ASM_)

#if CH_CFG_ST_TIMEDELTA > 0
#include "chcore_timer.h"
#endif /* CH_CFG_ST_TIMEDELTA > 0 */

#endif /* !defined(_FROM_ASM_) */

#endif /* CHCORE_H */

/** @} */
