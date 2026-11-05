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

/** @} */
