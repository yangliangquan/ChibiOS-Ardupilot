/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

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
 * @file    EXTIv1/hal_exti_lld.h
 * @brief   CH32H417 EXTI subsystem low level driver header.
 *
 * @details Port local external interrupt driver for the sixteen GPIO
 *          lines (EXTI0..EXTI15). Each channel selects a source port
 *          and pad through AFIO, an edge mode and a callback invoked
 *          from the shared interrupt handlers.
 *
 * @addtogroup EXTI
 * @{
 */

#ifndef HAL_EXTI_LLD_H
#define HAL_EXTI_LLD_H

#include "ch32h417.h"

#if !defined(CH32H417_EXTI_USE_EXTI) || defined(__DOXYGEN__)
#define CH32H417_EXTI_USE_EXTI              FALSE
#endif

#if (CH32H417_EXTI_USE_EXTI == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Number of GPIO EXTI channels.
 */
#define EXTI_MAX_CHANNELS                   16U

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   EXTI callback type.
 *
 * @param[in] line      the EXTI line that generated the interrupt
 */
typedef void (*exticallback_t)(uint32_t line);

/**
 * @brief   Channel trigger mode.
 */
typedef enum {
  EXTI_TRIGGER_DISABLED = 0U,
  EXTI_TRIGGER_RISING = 1U,
  EXTI_TRIGGER_FALLING = 2U,
  EXTI_TRIGGER_BOTH = 3U
} EXTI_TriggerMode;

/**
 * @brief   EXTI channel configuration.
 */
typedef struct {
  /**
   * @brief   Source GPIO port.
   */
  GPIO_TypeDef              *port;
  /**
   * @brief   Source pad, it must be equal to the channel number.
   */
  uint32_t                  pad;
  /**
   * @brief   Trigger mode.
   */
  EXTI_TriggerMode          trigger;
  /**
   * @brief   Callback invoked from the interrupt context, may be NULL.
   */
  exticallback_t            callback;
} EXTIChannelConfig;

/**
 * @brief   EXTI driver configuration.
 */
typedef struct {
  /**
   * @brief   Channel configurations, index is the EXTI line.
   */
  EXTIChannelConfig         channels[EXTI_MAX_CHANNELS];
} EXTIConfig;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#ifdef __cplusplus
extern "C" {
#endif
  void extiInit(void);
  void extiStart(const EXTIConfig *config);
  void extiStop(void);
  void extiChannelEnable(uint32_t line);
  void extiChannelDisable(uint32_t line);
#ifdef __cplusplus
}
#endif

#endif /* CH32H417_EXTI_USE_EXTI == TRUE */

#endif /* HAL_EXTI_LLD_H */

/** @} */
