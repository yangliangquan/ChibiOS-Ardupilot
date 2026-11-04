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
 * @file    USARTv1/hal_sio_lld.h
 * @brief   CH32H417 SIO subsystem low level driver header.
 *
 * @details Interrupt driven engine over the F1-compatible USART register
 *          model, eight units (USART1..USART8). The peripheral has a single
 *          byte receive register and no write-one-to-clear status register,
 *          the error and idle flags are cleared by a STATR read followed by
 *          a DATAR read (RM 19.11.1), see the notes on the receive path.
 *
 * @addtogroup SIO
 * @{
 */

#ifndef HAL_SIO_LLD_H
#define HAL_SIO_LLD_H

#if (HAL_USE_SIO == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   Mask of RX-related errors in the STATR register.
 * @note    PE has its own enable bit (PEIE), NE/FE/ORE are gated by EIE
 *          in the multiple buffer (DMA) mode, RM 19.11.6.
 */
#define SIO_LLD_ISR_RX_ERRORS           (USART_STATR_PE |                    \
                                         USART_STATR_FE |                    \
                                         USART_STATR_NE |                    \
                                         USART_STATR_ORE)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    CH32H417 configuration options
 * @{
 */
/**
 * @brief   SIO driver enable switches.
 * @details If set to @p TRUE the support for the unit is included.
 */
#if !defined(CH32H417_SIO_USE_USART1) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART1             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART2) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART2             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART3) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART3             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART4) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART4             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART5) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART5             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART6) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART6             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART7) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART7             FALSE
#endif
#if !defined(CH32H417_SIO_USE_USART8) || defined(__DOXYGEN__)
#define CH32H417_SIO_USE_USART8             FALSE
#endif

/**
 * @brief   Per-unit interrupt priority levels.
 * @note    The value is the PFIC @p IPRIOR[7:4] field, with PMTCFG=2 the
 *          two high bits are the preemption level (0 = highest).
 * @{
 */
#if !defined(CH32H417_SIO_USART1_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART1_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART2_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART2_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART3_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART3_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART4_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART4_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART5_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART5_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART6_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART6_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART7_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART7_IRQ_PRIORITY    12
#endif
#if !defined(CH32H417_SIO_USART8_IRQ_PRIORITY) || defined(__DOXYGEN__)
#define CH32H417_SIO_USART8_IRQ_PRIORITY    12
#endif
/** @} */
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_SIO_USE_USART1 && !CH32H417_SIO_USE_USART2 &&                  \
    !CH32H417_SIO_USE_USART3 && !CH32H417_SIO_USE_USART4 &&                  \
    !CH32H417_SIO_USE_USART5 && !CH32H417_SIO_USE_USART6 &&                  \
    !CH32H417_SIO_USE_USART7 && !CH32H417_SIO_USE_USART8
#error "SIO driver activated but no USART unit assigned"
#endif

/* Checks on the allocation of the units, the SERIAL driver is included
   earlier by hal.h so its switches are already known here.*/
#if CH32H417_SIO_USE_USART1
#if defined(CH32H417_SERIAL_USE_USART1) && CH32H417_SERIAL_USE_USART1
#error "SIOD1 requires USART1 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART1_IS_USED)
#error "SIOD1 requires USART1 but it is already used"
#else
#define CH32H417_USART1_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART2
#if defined(CH32H417_SERIAL_USE_USART2) && CH32H417_SERIAL_USE_USART2
#error "SIOD2 requires USART2 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART2_IS_USED)
#error "SIOD2 requires USART2 but it is already used"
#else
#define CH32H417_USART2_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART3
#if defined(CH32H417_SERIAL_USE_USART3) && CH32H417_SERIAL_USE_USART3
#error "SIOD3 requires USART3 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART3_IS_USED)
#error "SIOD3 requires USART3 but it is already used"
#else
#define CH32H417_USART3_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART4
#if defined(CH32H417_SERIAL_USE_USART4) && CH32H417_SERIAL_USE_USART4
#error "SIOD4 requires USART4 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART4_IS_USED)
#error "SIOD4 requires USART4 but it is already used"
#else
#define CH32H417_USART4_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART5
#if defined(CH32H417_SERIAL_USE_USART5) && CH32H417_SERIAL_USE_USART5
#error "SIOD5 requires USART5 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART5_IS_USED)
#error "SIOD5 requires USART5 but it is already used"
#else
#define CH32H417_USART5_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART6
#if defined(CH32H417_SERIAL_USE_USART6) && CH32H417_SERIAL_USE_USART6
#error "SIOD6 requires USART6 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART6_IS_USED)
#error "SIOD6 requires USART6 but it is already used"
#else
#define CH32H417_USART6_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART7
#if defined(CH32H417_SERIAL_USE_USART7) && CH32H417_SERIAL_USE_USART7
#error "SIOD7 requires USART7 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART7_IS_USED)
#error "SIOD7 requires USART7 but it is already used"
#else
#define CH32H417_USART7_IS_USED
#endif
#endif

#if CH32H417_SIO_USE_USART8
#if defined(CH32H417_SERIAL_USE_USART8) && CH32H417_SERIAL_USE_USART8
#error "SIOD8 requires USART8 but it is already used by the SERIAL driver"
#endif
#if defined(CH32H417_USART8_IS_USED)
#error "SIOD8 requires USART8 but it is already used"
#else
#define CH32H417_USART8_IS_USED
#endif
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the SIO driver structure.
 */
#define sio_lld_driver_fields                                               \
  /* Pointer to the USARTx registers block.*/                               \
  USART_TypeDef             *usart;                                         \
  /* Byte consumed in order to clear the status flags.*/                    \
  uint8_t                   rxbuf;                                          \
  /* A byte is held in @p rxbuf.*/                                          \
  bool                      rxpending;

/**
 * @brief   Low level fields of the SIO configuration structure.
 */
#define sio_lld_config_fields                                               \
  /* Desired baud rate.*/                                                   \
  uint32_t                  baud;                                           \
  /* USART CTLR1 register initialization data.*/                            \
  uint16_t                  cr1;                                            \
  /* USART CTLR2 register initialization data.*/                            \
  uint16_t                  cr2;                                            \
  /* USART CTLR3 register initialization data.*/                            \
  uint16_t                  cr3;

/**
 * @brief   Determines the state of the RX data.
 * @note    A byte captured while clearing the status flags also counts.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The RX data state.
 * @retval false        if a frame is available
 * @retval true         if no frame is available
 *
 * @notapi
 */
#define sio_lld_is_rx_empty(siop)                                           \
  (bool)(((siop)->rxpending == false) &&                                    \
         (((siop)->usart->STATR & USART_STATR_RXNE) == 0U))

/**
 * @brief   Determines the activity state of the receiver.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The RX activity state.
 * @retval false        if RX is in active state.
 * @retval true         if RX is in idle state.
 *
 * @notapi
 */
#define sio_lld_is_rx_idle(siop)                                            \
  (bool)(((siop)->usart->STATR & USART_STATR_IDLE) != 0U)

/**
 * @brief   Determines if RX has pending error events to be read and cleared.
 * @note    Only errors are handled, data and idle events are not considered.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The RX error events.
 * @retval false        if RX has no pending events
 * @retval true         if RX has pending events
 *
 * @notapi
 */
#define sio_lld_has_rx_errors(siop)                                         \
  (bool)(((siop)->usart->STATR & SIO_LLD_ISR_RX_ERRORS) != 0U)

/**
 * @brief   Determines the state of the TX data register.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The TX data register state.
 * @retval false        if TX is ready to accept a frame
 * @retval true         if TX is still busy
 *
 * @notapi
 */
#define sio_lld_is_tx_full(siop)                                            \
  (bool)(((siop)->usart->STATR & USART_STATR_TXE) == 0U)

/**
 * @brief   Determines the transmission state.
 *
 * @param[in] siop      pointer to the @p SIODriver object
 * @return              The transmission state.
 * @retval false        if transmission is idle
 * @retval true         if transmission is ongoing
 *
 * @notapi
 */
#define sio_lld_is_tx_ongoing(siop)                                         \
  (bool)(((siop)->usart->STATR & USART_STATR_TC) == 0U)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_SIO_USE_USART1 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD1;
#endif
#if (CH32H417_SIO_USE_USART2 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD2;
#endif
#if (CH32H417_SIO_USE_USART3 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD3;
#endif
#if (CH32H417_SIO_USE_USART4 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD4;
#endif
#if (CH32H417_SIO_USE_USART5 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD5;
#endif
#if (CH32H417_SIO_USE_USART6 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD6;
#endif
#if (CH32H417_SIO_USE_USART7 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD7;
#endif
#if (CH32H417_SIO_USE_USART8 == TRUE) && !defined(__DOXYGEN__)
extern SIODriver SIOD8;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void sio_lld_init(void);
  msg_t sio_lld_start(SIODriver *siop);
  void sio_lld_stop(SIODriver *siop);
  void sio_lld_update_enable_flags(SIODriver *siop);
  sioevents_t sio_lld_get_and_clear_errors(SIODriver *siop);
  sioevents_t sio_lld_get_and_clear_events(SIODriver *siop);
  sioevents_t sio_lld_get_events(SIODriver *siop);
  size_t sio_lld_read(SIODriver *siop, uint8_t *buffer, size_t n);
  size_t sio_lld_write(SIODriver *siop, const uint8_t *buffer, size_t n);
  msg_t sio_lld_get(SIODriver *siop);
  void sio_lld_put(SIODriver *siop, uint_fast16_t data);
  msg_t sio_lld_control(SIODriver *siop, unsigned int operation, void *arg);
  void sio_lld_serve_interrupt(SIODriver *siop);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_SIO == TRUE */

#endif /* HAL_SIO_LLD_H */

/** @} */