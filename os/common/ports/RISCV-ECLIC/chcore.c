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
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Software interrupt handler used for deferred preemption.
 *
 * @details The IRQ epilogue only pends this interrupt; the switch itself is
 *          performed here, at the lowest interrupt priority, once every IRQ
 *          handler has returned. This is the RISC-V equivalent of the
 *          ARMv7-M PendSV mechanism.
 *
 * @special
 */
void SW_Handler(void) {

  NVIC_ClearPendingIRQ(Software_IRQn);

#if CH_DBG_SYSTEM_STATE_CHECK
  __dbg_check_lock();
#endif

  /* The pending bit is coalesced, re-check the scheduler state. */
  if (chSchIsPreemptionRequired()) {
    chSchDoPreemption();
  }

#if CH_DBG_SYSTEM_STATE_CHECK
  __dbg_check_unlock();
#endif
}

/** @} */
