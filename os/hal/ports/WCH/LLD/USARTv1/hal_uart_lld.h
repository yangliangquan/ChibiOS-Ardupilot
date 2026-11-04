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
 * @file    USARTv1/hal_uart_lld.h
 * @brief   CH32H417 UART subsystem low level driver header.
 *
 * @details Eight USART units (USART1..USART8), F1-compatible register model.
 *          Both directions are DMA driven, the channels are taken from the
 *          free pool at start time and routed through the DMAMUX.
 *
 * @addtogroup UART
 * @{
 */

#ifndef HAL_UART_LLD_H
#define HAL_UART_LLD_H

#if (HAL_USE_UART == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Enhanced API flag.
 * @details The start function can report a DMA allocation failure.
 */
#define UART_LLD_ENHANCED_API

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    CH32H417 configuration options
 * @{
 */
/**
 * @brief   UART driver enable switches.
 * @details If set to @p TRUE the support for the unit is included.
 */
#if !defined(CH32H417_UART_USE_USART1) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART1            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART2) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART2            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART3) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART3            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART4) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART4            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART5) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART5            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART6) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART6            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART7) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART7            FALSE
#endif
#if !defined(CH32H417_UART_USE_USART8) || defined(__DOXYGEN__)
#define CH32H417_UART_USE_USART8            FALSE
#endif

/**
 * @brief   Per-unit interrupt and DMA priority levels.
 * @note    The value is the PFIC @p IPRIOR[7:4] field, with PMTCFG=2 the
 *          two high bits are the preemption level (0 = highest).
 * @{
 */
#if !defined(CH32H417_UART_USART1_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART1_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART2_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART2_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART3_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART3_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART4_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART4_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART5_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART5_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART6_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART6_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART7_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART7_IRQ_PRIORITY   12
#endif
#if !defined(CH32H417_UART_USART8_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART8_IRQ_PRIORITY   12
#endif

#if !defined(CH32H417_UART_USART1_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART1_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART2_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART2_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART3_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART3_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART4_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART4_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART5_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART5_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART6_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART6_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART7_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART7_DMA_PRIORITY   8
#endif
#if !defined(CH32H417_UART_USART8_DMA_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_UART_USART8_DMA_PRIORITY   8
#endif
/** @} */
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_UART_USE_USART1 && !CH32H417_UART_USE_USART2 &&                \
    !CH32H417_UART_USE_USART3 && !CH32H417_UART_USE_USART4 &&                \
    !CH32H417_UART_USE_USART5 && !CH32H417_UART_USE_USART6 &&                \
    !CH32H417_UART_USE_USART7 && !CH32H417_UART_USE_USART8
#error "UART driver activated but no USART unit assigned"
#endif

/* The driver always transfers through DMA.*/
#if !defined(CH32_DMA_REQUIRED)
#define CH32_DMA_REQUIRED
#endif

/* Checks on the allocation of the units, the SERIAL and SIO drivers are
   included earlier by hal.h so their switches are already known here.*/
#if CH32H417_UART_USE_USART1
#if defined(CH32H417_SERIAL_USE_USART1) && CH32H417_SERIAL_USE_USART1
#error "UARTD1 requires USART1 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART1_IS_USED)
#error "UARTD1 requires USART1 but it is already used"
#else
#define CH32H417_USART1_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART1_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART1"
#endif
#endif

#if CH32H417_UART_USE_USART2
#if defined(CH32H417_SERIAL_USE_USART2) && CH32H417_SERIAL_USE_USART2
#error "UARTD2 requires USART2 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART2_IS_USED)
#error "UARTD2 requires USART2 but it is already used"
#else
#define CH32H417_USART2_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART2_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART2"
#endif
#endif

#if CH32H417_UART_USE_USART3
#if defined(CH32H417_SERIAL_USE_USART3) && CH32H417_SERIAL_USE_USART3
#error "UARTD3 requires USART3 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART3_IS_USED)
#error "UARTD3 requires USART3 but it is already used"
#else
#define CH32H417_USART3_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART3_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART3"
#endif
#endif

#if CH32H417_UART_USE_USART4
#if defined(CH32H417_SERIAL_USE_USART4) && CH32H417_SERIAL_USE_USART4
#error "UARTD4 requires USART4 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART4_IS_USED)
#error "UARTD4 requires USART4 but it is already used"
#else
#define CH32H417_USART4_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART4_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART4"
#endif
#endif

#if CH32H417_UART_USE_USART5
#if defined(CH32H417_SERIAL_USE_USART5) && CH32H417_SERIAL_USE_USART5
#error "UARTD5 requires USART5 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART5_IS_USED)
#error "UARTD5 requires USART5 but it is already used"
#else
#define CH32H417_USART5_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART5_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART5"
#endif
#endif

#if CH32H417_UART_USE_USART6
#if defined(CH32H417_SERIAL_USE_USART6) && CH32H417_SERIAL_USE_USART6
#error "UARTD6 requires USART6 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART6_IS_USED)
#error "UARTD6 requires USART6 but it is already used"
#else
#define CH32H417_USART6_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART6_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART6"
#endif
#endif

#if CH32H417_UART_USE_USART7
#if defined(CH32H417_SERIAL_USE_USART7) && CH32H417_SERIAL_USE_USART7
#error "UARTD7 requires USART7 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART7_IS_USED)
#error "UARTD7 requires USART7 but it is already used"
#else
#define CH32H417_USART7_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART7_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART7"
#endif
#endif

#if CH32H417_UART_USE_USART8
#if defined(CH32H417_SERIAL_USE_USART8) && CH32H417_SERIAL_USE_USART8
#error "UARTD8 requires USART8 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART8_IS_USED)
#error "UARTD8 requires USART8 but it is already used"
#else
#define CH32H417_USART8_IS_USED
#endif
#if !CH32_DMA_IS_VALID_PRIORITY(CH32H417_UART_USART8_DMA_PRIORITY)
#error "Invalid DMA priority assigned to USART8"
#endif
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   UART driver condition flags type.
 */
typedef uint32_t uartflags_t;

/**
 * @brief   Type of an UART driver.
 */
typedef struct hal_uart_driver UARTDriver;

/**
 * @brief   Generic UART notification callback type.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 */
typedef void (*uartcb_t)(UARTDriver *uartp);

/**
 * @brief   Character received UART notification callback type.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] c         received character
 */
typedef void (*uartccb_t)(UARTDriver *uartp, uint16_t c);

/**
 * @brief   Receive error UART notification callback type.
 *
 * @param[in] uartp     pointer to the @p UARTDriver object
 * @param[in] e         receive error mask
 */
typedef void (*uartecb_t)(UARTDriver *uartp, uartflags_t e);

/**
 * @brief   Type of an UART configuration structure.
 */
typedef struct hal_uart_config {
  /**
   * @brief End of transmission buffer callback.
   */
  uartcb_t                  txend1_cb;
  /**
   * @brief Physical end of transmission callback.
   */
  uartcb_t                  txend2_cb;
  /**
   * @brief Receive buffer filled callback.
   */
  uartcb_t                  rxend_cb;
  /**
   * @brief Character received while out of the @p UART_RECEIVE state.
   */
  uartccb_t                 rxchar_cb;
  /**
   * @brief Receive error callback.
   */
  uartecb_t                 rxerr_cb;
  /* End of the mandatory fields.*/
  /**
   * @brief Bit rate.
   */
  uint32_t                  speed;
  /**
   * @brief Initialization value for the CTLR1 register.
   */
  uint16_t                  cr1;
  /**
   * @brief Initialization value for the CTLR2 register.
   */
  uint16_t                  cr2;
  /**
   * @brief Initialization value for the CTLR3 register.
   */
  uint16_t                  cr3;
} UARTConfig;

/**
 * @brief   Structure representing an UART driver.
 */
struct hal_uart_driver {
  /**
   * @brief Driver state.
   */
  uartstate_t               state;
  /**
   * @brief Transmitter state.
   */
  uarttxstate_t             txstate;
  /**
   * @brief Receiver state.
   */
  uartrxstate_t             rxstate;
  /**
   * @brief Current configuration data.
   */
  const UARTConfig          *config;
#if (UART_USE_WAIT == TRUE) || defined(__DOXYGEN__)
  /**
   * @brief   Synchronization flag for transmit operations.
   */
  bool                      early;
  /**
   * @brief   Waiting thread on RX.
   */
  thread_reference_t        threadrx;
  /**
   * @brief   Waiting thread on TX.
   */
  thread_reference_t        threadtx;
#endif /* UART_USE_WAIT */
#if (UART_USE_MUTUAL_EXCLUSION == TRUE) || defined(__DOXYGEN__)
  /**
   * @brief   Mutex protecting the peripheral.
   */
  mutex_t                   mutex;
#endif /* UART_USE_MUTUAL_EXCLUSION */
#if defined(UART_DRIVER_EXT_FIELDS)
  UART_DRIVER_EXT_FIELDS
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief Pointer to the USART registers block.
   */
  USART_TypeDef             *usart;
  /**
   * @brief Receive DMA mode bit mask.
   */
  uint32_t                  dmarxmode;
  /**
   * @brief Send DMA mode bit mask.
   */
  uint32_t                  dmatxmode;
  /**
   * @brief Receive DMA channel.
   */
  const ch32_dma_stream_t   *dmarx;
  /**
   * @brief Transmit DMA channel.
   */
  const ch32_dma_stream_t   *dmatx;
  /**
   * @brief Default receive buffer while into @p UART_RX_IDLE state.
   */
  volatile uint8_t          rxbuf;
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_UART_USE_USART1 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD1;
#endif
#if (CH32H417_UART_USE_USART2 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD2;
#endif
#if (CH32H417_UART_USE_USART3 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD3;
#endif
#if (CH32H417_UART_USE_USART4 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD4;
#endif
#if (CH32H417_UART_USE_USART5 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD5;
#endif
#if (CH32H417_UART_USE_USART6 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD6;
#endif
#if (CH32H417_UART_USE_USART7 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD7;
#endif
#if (CH32H417_UART_USE_USART8 == TRUE) && !defined(__DOXYGEN__)
extern UARTDriver UARTD8;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void uart_lld_init(void);
  msg_t uart_lld_start(UARTDriver *uartp);
  void uart_lld_stop(UARTDriver *uartp);
  void uart_lld_start_send(UARTDriver *uartp, size_t n, const void *txbuf);
  size_t uart_lld_stop_send(UARTDriver *uartp);
  void uart_lld_start_receive(UARTDriver *uartp, size_t n, void *rxbuf);
  size_t uart_lld_stop_receive(UARTDriver *uartp);
  void uart_lld_serve_interrupt(UARTDriver *uartp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_UART == TRUE */

#endif /* HAL_UART_LLD_H */

/** @} */