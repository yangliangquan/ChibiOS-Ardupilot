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
 * @file    CANv1/hal_can_lld.h
 * @brief   CH32H417 CAN subsystem low level driver header.
 *
 * @details Three bxCAN-style controllers (CAN1..CAN3), F1-compatible
 *          model with renamed registers (CTLR/STATR/TSTATR/RFIFO0/
 *          RFIFO1/INTENR/ERRSR). A shared filter block of 42 banks
 *          lives on CAN1 (vendor CAN_FilterInit, CAN_SlaveStartBank).
 *          Sleep/wakeup APIs are not implemented in this version.
 *
 * @addtogroup CAN
 * @{
 */

#ifndef HAL_CAN_LLD_H
#define HAL_CAN_LLD_H

#if (HAL_USE_CAN == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_can.h"

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @brief   This implementation does not support sleep/wakeup.
 */
#define CAN_SUPPORTS_SLEEP          FALSE

/**
 * @brief   Three transmit mailboxes per controller.
 */
#define CAN_TX_MAILBOXES           3

/**
 * @brief   Two receive mailboxes (FIFO0/FIFO1) per controller.
 */
#define CAN_RX_MAILBOXES           2

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   CANDx driver enable switches.
 */
#if !defined(CH32H417_CAN_USE_CAN1) || defined(__DOXYGEN__)
#define CH32H417_CAN_USE_CAN1              FALSE
#endif
#if !defined(CH32H417_CAN_USE_CAN2) || defined(__DOXYGEN__)
#define CH32H417_CAN_USE_CAN2              FALSE
#endif
#if !defined(CH32H417_CAN_USE_CAN3) || defined(__DOXYGEN__)
#define CH32H417_CAN_USE_CAN3              FALSE
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_CAN_USE_CAN1 && !CH32H417_CAN_USE_CAN2 && !CH32H417_CAN_USE_CAN3
#error "CAN driver activated but no CAN peripheral assigned"
#endif

#if CAN_USE_SLEEP_MODE && !CAN_SUPPORTS_SLEEP
#error "CAN sleep mode not supported by this driver"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of a structure representing a CAN driver.
 */
typedef struct hal_can_driver CANDriver;

/**
 * @brief   Type of a CAN identifier.
 */
typedef uint32_t canid_t;

/**
 * @brief   Type of a transmission mailbox index (1..CAN_TX_MAILBOXES).
 */
typedef uint32_t canmbx_t;

/**
 * @brief   Frame transmission message type.
 */
typedef struct {
  struct {
    uint8_t                 DLC:4;          /**< @brief Data length.        */
    uint8_t                 RTR:1;          /**< @brief Frame type.         */
    uint8_t                 IDE:1;          /**< @brief Identifier type.    */
  };
  union {
    struct {
      uint32_t              SID:11;         /**< @brief Standard identifier.*/
    };
    struct {
      uint32_t              EID:29;         /**< @brief Extended identifier.*/
    };
  };
  union {
    uint8_t                 data8[8];       /**< @brief Frame data.         */
    uint16_t                data16[4];      /**< @brief Frame data.         */
    uint32_t                data32[2];      /**< @brief Frame data.         */
    uint64_t                data64[1];      /**< @brief Frame data.         */
  };
} CANTxFrame;

/**
 * @brief   Frame receive message type.
 */
typedef struct {
  struct {
    uint8_t                 FMI;            /**< @brief Filter id.          */
    uint16_t                TIME;           /**< @brief Time stamp.         */
  };
  struct {
    uint8_t                 DLC:4;          /**< @brief Data length.        */
    uint8_t                 RTR:1;          /**< @brief Frame type.         */
    uint8_t                 IDE:1;          /**< @brief Identifier type.    */
  };
  union {
    struct {
      uint32_t              SID:11;         /**< @brief Standard identifier.*/
    };
    struct {
      uint32_t              EID:29;         /**< @brief Extended identifier.*/
    };
  };
  union {
    uint8_t                 data8[8];       /**< @brief Frame data.         */
    uint16_t                data16[4];      /**< @brief Frame data.         */
    uint32_t                data32[2];      /**< @brief Frame data.         */
    uint64_t                data64[1];      /**< @brief Frame data.         */
  };
} CANRxFrame;

/**
 * @brief   CAN filter description (bxCAN filter bank format).
 */
typedef struct {
  uint32_t                  filter:16;      /**< @brief Filter bank number. */
  uint32_t                  mode:1;         /**< @brief 0=mask, 1=list.     */
  uint32_t                  scale:1;        /**< @brief 0=16bit, 1=32bit.   */
  uint32_t                  assignment:1;   /**< @brief 0=FIFO0, 1=FIFO1.   */
  uint32_t                  register1;      /**< @brief FxR1 pattern.       */
  uint32_t                  register2;      /**< @brief FxR2 pattern.       */
} CANFilter;

/**
 * @brief   Type of a CAN configuration structure.
 * @note    Bit timing is supplied in F1 style; the application computes
 *          the prescaler from the peripheral clock and the desired
 *          baud rate (CAN clock on this platform = HCLK).
 */
typedef struct hal_can_config {
  uint16_t                  prescaler;      /**< @brief Time quantum length.*/
  uint8_t                   mode;           /**< @brief CAN_Mode_xxx.       */
  uint8_t                   sjw;            /**< @brief Resync jump width.  */
  uint8_t                   bs1;            /**< @brief Bit segment 1.      */
  uint8_t                   bs2;            /**< @brief Bit segment 2.      */
  bool                      abom;           /**< @brief Automatic bus-off.  */
  bool                      awum;           /**< @brief Automatic wakeup.   */
  bool                      nart;           /**< @brief No auto retransmit. */
} CANConfig;

/**
 * @brief   Structure representing a CAN driver.
 */
struct hal_can_driver {
  /**
   * @brief Driver state.
   */
  canstate_t                state;
  /**
   * @brief Current configuration data.
   */
  const CANConfig           *config;
  /**
   * @brief Transmission threads queue.
   */
  threads_queue_t           txqueue;
  /**
   * @brief Receive threads queue.
   */
  threads_queue_t           rxqueue;
#if (CAN_ENFORCE_USE_CALLBACKS == FALSE) || defined(__DOXYGEN__)
  /**
   * @brief One or more frames become available.
   */
  event_source_t            rxfull_event;
  /**
   * @brief One or more transmission mailbox become available.
   */
  event_source_t            txempty_event;
  /**
   * @brief A CAN bus error happened.
   */
  event_source_t            error_event;
#else
  can_callback_t            rxfull_cb;
  can_callback_t            txempty_cb;
  can_callback_t            error_cb;
#endif
  /* End of the mandatory fields.*/
  /**
   * @brief Pointer to the CAN registers block.
   */
  CAN_TypeDef               *can;
};

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_CAN_USE_CAN1 == TRUE) && !defined(__DOXYGEN__)
extern CANDriver CAND1;
#endif
#if (CH32H417_CAN_USE_CAN2 == TRUE) && !defined(__DOXYGEN__)
extern CANDriver CAND2;
#endif
#if (CH32H417_CAN_USE_CAN3 == TRUE) && !defined(__DOXYGEN__)
extern CANDriver CAND3;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void can_lld_init(void);
  void can_lld_start(CANDriver *canp);
  void can_lld_stop(CANDriver *canp);
  bool can_lld_is_tx_empty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_transmit(CANDriver *canp,
                        canmbx_t mailbox,
                        const CANTxFrame *ctfp);
  bool can_lld_is_rx_nonempty(CANDriver *canp, canmbx_t mailbox);
  void can_lld_receive(CANDriver *canp,
                       canmbx_t mailbox,
                       CANRxFrame *crfp);
  void can_lld_abort(CANDriver *canp, canmbx_t mailbox);
  void canWCHSetFilters(CANDriver *canp, uint32_t num, const CANFilter *cfp);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_CAN == TRUE */

#endif /* HAL_CAN_LLD_H */

/** @} */
