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
 * @file    USARTv1/hal_serial_lld.h
 * @brief   CH32H417 serial subsystem low level driver header.
 *
 * @details Eight USART units (USART1..USART8), F1-compatible
 *          register model.
 *
 * @addtogroup SERIAL
 * @{
 */

#ifndef HAL_SERIAL_LLD_H
#define HAL_SERIAL_LLD_H

#if (HAL_USE_SERIAL == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    CH32H417 configuration options
 * @{
 */
/**
 * @brief   USARTx driver enable switches.
 * @details If set to @p TRUE the support for the unit is included.
 */
#if !defined(CH32H417_SERIAL_USE_USART1) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART1          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART2) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART2          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART3) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART3          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART4) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART4          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART5) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART5          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART6) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART6          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART7) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART7          FALSE
#endif
#if !defined(CH32H417_SERIAL_USE_USART8) || defined(__DOXYGEN__)
#define CH32H417_SERIAL_USE_USART8          FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_SERIAL_USE_USART1 && !CH32H417_SERIAL_USE_USART2 &&            \
    !CH32H417_SERIAL_USE_USART3 && !CH32H417_SERIAL_USE_USART4 &&            \
    !CH32H417_SERIAL_USE_USART5 && !CH32H417_SERIAL_USE_USART6 &&            \
    !CH32H417_SERIAL_USE_USART7 && !CH32H417_SERIAL_USE_USART8
#error "SERIAL driver activated but no USART unit assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

typedef void (*uartirq_t)(void* sd);

/**
 * @brief   CH32H417 Serial Driver configuration structure.
 */
typedef struct hal_serial_config {
  uint32_t    speed;
  uint16_t    cr1;
  uint16_t    cr2;
  uint16_t    cr3;
  uartirq_t   irq_cb;
  void*       ctx;
} SerialConfig;

#define _serial_irq_code(sd) {                          \
  if ((sd)->config->irq_cb != NULL)                     \
    (sd)->config->irq_cb((sd)->config->ctx);            \
}

/**
 * @brief   @p SerialDriver specific data.
 */
#define _serial_driver_data                                                 \
  _base_asynchronous_channel_data                                           \
  /* Driver state.*/                                                        \
  sdstate_t                 state;                                          \
  /* Input queue.*/                                                         \
  input_queue_t             iqueue;                                         \
  /* Output queue.*/                                                        \
  output_queue_t            oqueue;                                         \
  /* Input circular buffer.*/                                               \
  uint8_t                   ib[SERIAL_BUFFERS_SIZE];                        \
  /* Output circular buffer.*/                                              \
  uint8_t                   ob[SERIAL_BUFFERS_SIZE];                        \
  /* End of the mandatory fields.*/                                         \
  /* Associated USART unit.*/                                               \
  USART_TypeDef             *usart;                                         \
  const SerialConfig *config;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_SERIAL_USE_USART1 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD1;
#endif
#if (CH32H417_SERIAL_USE_USART2 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD2;
#endif
#if (CH32H417_SERIAL_USE_USART3 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD3;
#endif
#if (CH32H417_SERIAL_USE_USART4 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD4;
#endif
#if (CH32H417_SERIAL_USE_USART5 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD5;
#endif
#if (CH32H417_SERIAL_USE_USART6 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD6;
#endif
#if (CH32H417_SERIAL_USE_USART7 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD7;
#endif
#if (CH32H417_SERIAL_USE_USART8 == TRUE) && !defined(__DOXYGEN__)
extern SerialDriver SD8;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void sd_lld_init(void);
  void sd_lld_start(SerialDriver *sdp, const SerialConfig *config);
  void sd_lld_stop(SerialDriver *sdp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_SERIAL == TRUE */

#endif /* HAL_SERIAL_LLD_H */

/** @} */
