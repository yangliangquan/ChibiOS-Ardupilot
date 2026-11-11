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
 * @details Set by @p PORT_IRQ_EPILOGUE(), consumed by the interrupt exit path
 *          which performs the context switch.
 */
volatile bool port_preempt_pending;

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Evaluates if the interrupted thread must be preempted.
 * @details Only the outermost interrupt level performs the switch, deeper
 *          levels leave the request pending.
 *
 * @return              The preemption state.
 * @retval true         a context switch must be performed.
 * @retval false        the interrupted thread must be resumed.
 *
 * @special
 */
bool _port_irq_preempt_pending(void) {

#if CH_DBG_ENABLE_ASSERTS
  /* Fatal condition: the hardware cannot exit from the exception that
     detected it (QingKe V5 manual, CSR 0xBC1 NEST_OV). */
  chDbgAssert(port_isr_nesting_overflow() == false, "interrupt nesting overflow");
#endif

  if (!port_preempt_pending) {
    return false;
  }

  if (port_isr_nesting_level() > 1U) {
    return false;
  }

  port_preempt_pending = false;

  return true;
}

/**
 * @brief   Brings the stack pointer back to the interrupted thread frame on
 *          interrupt exit.
 * @details Only the level owning the frame latched in mscratch may leave the
 *          independent interrupt stack, every other level returns zero.
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
  chDbgAssert(port_isr_nesting_overflow() == false, "interrupt nesting overflow");
  chDbgAssert(sp >= (uint32_t)__irq_stack_base__, "ISR stack overflow");
  chDbgAssert(sp <= irq_stack_top, "sp above the ISR stack");
#endif

  /* The frame owner is identified by sp alone: PORT_IRQ_ENTRY points sp at
     the top of the interrupt stack only for the level that latched a frame
     in mscratch.
     GISR.NESTSTA must not be used here, the core dispatches an interrupt
     before the first instruction of its vector stub, so a level that owns no
     frame already counts in NESTSTA. Treating it as nested restored a frame
     from the stack top, which is the first word of .data. */
  if (sp != irq_stack_top) {
    return 0U;
  }

  /* Outermost level: mscratch holds the frame pointer of PORT_IRQ_ENTRY, the
     swap also restores the thread-mode zero sentinel. */
  __asm__ volatile ("csrrw %0, %1, zero"
                    : "=r" (frame_sp) : "i" (CSR_MSCRATCH));

  if (frame_sp == 0U) {
    chSysHalt("ISR stack desync");
  }

  return frame_sp;
}

/** @} */
