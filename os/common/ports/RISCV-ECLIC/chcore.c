/*
    ChibiOS - Copyright (C) 2021 Stefan Kerkmann.

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
 * @file    chcore.c
 * @brief   Port related template code.
 *
 * @addtogroup port_core
 * @details Non portable code templates.
 * @{
 */

#include <stddef.h>

#include "ch.h"

/* Bottom of the independent interrupt stack, defined by rules_stacks.ld. */
extern uint32_t __irq_stack_base__[];

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

#if defined(_CHIBIOS_RT_CONF_)
#define PORT_CONTEXT_OFFSET     12U
#elif defined(_CHIBIOS_NIL_CONF_)
#define PORT_CONTEXT_OFFSET     0U
#else
#error "invalid chconf.h"
#endif

_Static_assert(offsetof(thread_t, ctx) == PORT_CONTEXT_OFFSET,
               "thread_t.ctx offset mismatch");

/*===========================================================================*/
/* Module exported variables.                                                */
/*===========================================================================*/

/**
 * @brief   Reschedule request flag.
 * @details Set by @p PORT_IRQ_EPILOGUE() when a higher priority thread is
 *          ready, consumed by the interrupt exit path which performs the
 *          context switch.
 */
volatile bool port_preempt_pending;

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Evaluates if the interrupted thread must be preempted.
 * @details Called from the interrupt exit path, the function returns TRUE
 *          only if a reschedule has been requested and the core is at the
 *          outermost interrupt level.
 * @note    Deeper interrupt levels do not perform the switch, the request
 *          is left pending for the outermost level, this is the equivalent
 *          of the ARMv7-M "return to base" condition.
 *
 * @return              The preemption state.
 * @retval true         a context switch must be performed.
 * @retval false        the interrupted thread must be resumed.
 *
 * @special
 */
bool _port_irq_preempt_pending(void) {

#if CH_DBG_ENABLE_ASSERTS
  /* An overflow of the interrupt nesting levels is a fatal condition: the
     hardware cannot exit from the exception that detected it (QingKe V5
     processor manual, CSR 0xBC1 NEST_OV). */
  chDbgAssert(port_isr_nesting_overflow() == false, "interrupt nesting overflow");
#endif

  if (!port_preempt_pending) {
    return false;
  }

  if (port_isr_nesting_level() > 1U) {
    /* A nested interrupt level, the switch is performed by the outermost
       level, the request is left pending. */
    return false;
  }

  port_preempt_pending = false;

  return true;
}

/**
 * @brief   Brings the stack pointer back to the interrupted thread frame on
 *          interrupt exit.
 * @details Only the outermost interrupt level is allowed to leave the
 *          independent interrupt stack. Nested levels return zero and the
 *          caller restores their frame and executes mret immediately.
 *
 * @param[in] sp            stack pointer on return from the C handler
 * @param[in] irq_stack_top top of the independent interrupt stack
 * @return                  the frame pointer of the interrupted thread, or
 *                          zero when the current level is a nested one.
 *
 * @special
 */
uint32_t _port_irq_stack_switch_back(uint32_t sp, uint32_t irq_stack_top) {
  uint32_t frame_sp;

#if CH_DBG_ENABLE_ASSERTS
  /* An overflow of the interrupt nesting levels is a fatal condition: the
     hardware cannot exit from the exception that detected it (QingKe V5
     processor manual, CSR 0xBC1 NEST_OV). */
  chDbgAssert(port_isr_nesting_overflow() == false, "interrupt nesting overflow");
  chDbgAssert(sp >= (uint32_t)__irq_stack_base__, "ISR stack overflow");
#endif

  /* PFIC GISR.NESTSTA is the authoritative nesting state for ECLIC
     interrupts: a value above one means this is not the outermost level.
     The sp comparison additionally covers synchronous exceptions taken
     while an ISR is running, those are not accounted for by the PFIC and
     must not switch the stack back either. */
  if ((port_isr_nesting_level() > 1U) || (sp != irq_stack_top)) {
    return 0U;
  }

  /* Outermost level: mscratch holds the frame pointer latched by
     PORT_IRQ_ENTRY. Reading it and writing back the thread-mode zero
     sentinel is a single operation, so no trap can observe an intermediate
     state (MIE is clear at this point). */
  __asm__ volatile ("csrrw %0, %1, zero"
                    : "=r" (frame_sp) : "i" (CSR_MSCRATCH));

  return frame_sp;
}

/** @} */
