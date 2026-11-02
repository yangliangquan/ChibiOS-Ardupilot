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
 * @file    I2Cv1/hal_i2c_lld.c
 * @brief   CH32H417 I2C subsystem low level driver source.
 *
 * @details Blocking polled master implementation on the WCH vendor
 *          library with deadline based timeouts. The transfer loop
 *          runs with the kernel tick enabled (thread context, kernel
 *          unlocked by the generic driver).
 *
 * @addtogroup I2C
 * @{
 */

#include "hal.h"

#if (HAL_USE_I2C == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_i2c.h"

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

#if (CH32H417_I2C_USE_I2C1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Checks the bus error flags, records and clears them.
 *
 * @return              True if a bus error was found.
 */
static bool check_errors(I2CDriver *i2cp) {
  I2C_TypeDef *i2c = i2cp->i2c;
  bool err = false;

  if (I2C_GetFlagStatus(i2c, I2C_FLAG_AF) == SET) {
    i2cp->errors |= I2C_ACK_FAILURE;
    I2C_ClearFlag(i2c, I2C_FLAG_AF);
    err = true;
  }
  if (I2C_GetFlagStatus(i2c, I2C_FLAG_ARLO) == SET) {
    i2cp->errors |= I2C_ARBITRATION_LOST;
    I2C_ClearFlag(i2c, I2C_FLAG_ARLO);
    err = true;
  }
  if (I2C_GetFlagStatus(i2c, I2C_FLAG_BERR) == SET) {
    i2cp->errors |= I2C_BUS_ERROR;
    I2C_ClearFlag(i2c, I2C_FLAG_BERR);
    err = true;
  }

  return err;
}

/**
 * @brief   Waits for an I2C event with deadline and error checking.
 *
 * @return              MSG_OK, MSG_TIMEOUT or MSG_RESET.
 */
static msg_t wait_event(I2CDriver *i2cp,
                        uint32_t event,
                        systime_t start,
                        systime_t deadline) {
  I2C_TypeDef *i2c = i2cp->i2c;

  for (;;) {
    if (I2C_CheckEvent(i2c, event) == READY) {
      return MSG_OK;
    }
    if (check_errors(i2cp)) {
      return MSG_RESET;
    }
    if (!chTimeIsInRangeX(chVTGetSystemTimeX(), start, deadline)) {
      i2cp->errors |= I2C_TIMEOUT;
      return MSG_TIMEOUT;
    }
  }
}

/**
 * @brief   Aborts a transfer on error or timeout.
 */
static void abort_transfer(I2CDriver *i2cp) {

  I2C_GenerateSTOP(i2cp->i2c, ENABLE);
  I2C_AcknowledgeConfig(i2cp->i2c, ENABLE);
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level I2C driver initialization.
 *
 * @notapi
 */
void i2c_lld_init(void) {
}

/**
 * @brief   Enables the bus clock of an I2C unit.
 */
static void unit_enable(I2C_TypeDef *u) {

  if (u == I2C1) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_I2C1, ENABLE);
  }
#if CH32H417_I2C_USE_I2C2
  else if (u == I2C2) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_I2C2, ENABLE);
  }
#endif
#if CH32H417_I2C_USE_I2C3
  else if (u == I2C3) {
    RCC_HB1PeriphClockCmd(RCC_HB1Periph_I2C3, ENABLE);
  }
#endif
#if CH32H417_I2C_USE_I2C4
  else if (u == I2C4) {
    RCC_HB2PeriphClockCmd(RCC_HB2Periph_I2C4, ENABLE);
  }
#endif
}

/**
 * @brief   I2C driver configuration and start.
 *
 * @param[in] i2cp      pointer to an @p I2CDriver object
 *
 * @notapi
 */
void i2c_lld_start(I2CDriver *i2cp) {
  I2C_InitTypeDef init;

  unit_enable(i2cp->i2c);

  I2C_StructInit(&init);
  init.I2C_Mode = I2C_Mode_I2C;
  init.I2C_ClockSpeed = CH32H417_I2C1_CLOCK_SPEED;
  I2C_Init(i2cp->i2c, &init);
  I2C_Cmd(i2cp->i2c, ENABLE);
}

/**
 * @brief   I2C driver stop.
 *
 * @param[in] i2cp      pointer to an @p I2CDriver object
 *
 * @notapi
 */
void i2c_lld_stop(I2CDriver *i2cp) {

  I2C_Cmd(i2cp->i2c, DISABLE);
}

/**
 * @brief   Releases the bus without disabling the peripheral.
 *
 * @notapi
 */
void i2c_lld_soft_stop(I2CDriver *i2cp) {

  abort_transfer(i2cp);
}

/**
 * @brief   Master transmission with optional reception phase.
 *
 * @return              Operation status.
 *
 * @notapi
 */
msg_t i2c_lld_master_transmit_timeout(I2CDriver *i2cp,
                                      i2caddr_t addr,
                                      const uint8_t *txbuf,
                                      size_t txbytes,
                                      uint8_t *rxbuf,
                                      size_t rxbytes,
                                      sysinterval_t timeout) {
  I2C_TypeDef *i2c = i2cp->i2c;
  systime_t start = chVTGetSystemTimeX();
  systime_t deadline = chTimeAddX(start, timeout);
  msg_t msg;
  size_t i;

  I2C_AcknowledgeConfig(i2c, ENABLE);

  /* Write phase. */
  msg = wait_event(i2cp, I2C_EVENT_MASTER_MODE_SELECT, start, deadline);
  if (msg != MSG_OK) {
    abort_transfer(i2cp);
    return msg;
  }

  I2C_Send7bitAddress(i2c, (uint8_t)(addr << 1),
                      I2C_Direction_Transmitter);
  msg = wait_event(i2cp, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED,
                   start, deadline);
  if (msg != MSG_OK) {
    abort_transfer(i2cp);
    return msg;
  }

  for (i = 0; i < txbytes; i++) {
    msg = wait_event(i2cp, I2C_EVENT_MASTER_BYTE_TRANSMITTING,
                     start, deadline);
    if (msg != MSG_OK) {
      abort_transfer(i2cp);
      return msg;
    }
    I2C_SendData(i2c, txbuf[i]);
  }

  msg = wait_event(i2cp, I2C_EVENT_MASTER_BYTE_TRANSMITTED, start, deadline);
  if (msg != MSG_OK) {
    abort_transfer(i2cp);
    return msg;
  }

  if (rxbytes == 0U) {
    I2C_GenerateSTOP(i2c, ENABLE);
    return MSG_OK;
  }

  /* Repeated start, read phase. */
  I2C_GenerateSTART(i2c, ENABLE);
  msg = wait_event(i2cp, I2C_EVENT_MASTER_MODE_SELECT, start, deadline);
  if (msg != MSG_OK) {
    abort_transfer(i2cp);
    return msg;
  }

  I2C_Send7bitAddress(i2c, (uint8_t)(addr << 1),
                      I2C_Direction_Receiver);
  msg = wait_event(i2cp, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED,
                   start, deadline);
  if (msg != MSG_OK) {
    abort_transfer(i2cp);
    return msg;
  }

  for (i = 0; i < rxbytes; i++) {
    if (i == (rxbytes - 1U)) {
      I2C_AcknowledgeConfig(i2c, DISABLE);
      I2C_GenerateSTOP(i2c, ENABLE);
    }
    msg = wait_event(i2cp, I2C_EVENT_MASTER_BYTE_RECEIVED, start, deadline);
    if (msg != MSG_OK) {
      abort_transfer(i2cp);
      return msg;
    }
    rxbuf[i] = I2C_ReceiveData(i2c);
  }

  I2C_AcknowledgeConfig(i2c, ENABLE);

  return MSG_OK;
}

/**
 * @brief   Master reception.
 *
 * @return              Operation status.
 *
 * @notapi
 */
msg_t i2c_lld_master_receive_timeout(I2CDriver *i2cp,
                                     i2caddr_t addr,
                                     uint8_t *rxbuf,
                                     size_t rxbytes,
                                     sysinterval_t timeout) {

  return i2c_lld_master_transmit_timeout(i2cp, addr, NULL, 0U,
                                         rxbuf, rxbytes, timeout);
}

#if (CH32H417_I2C_USE_I2C1 == TRUE) && !defined(__DOXYGEN__)
I2CDriver I2CD1 = {
  .i2c = I2C1
};
#endif
#if (CH32H417_I2C_USE_I2C2 == TRUE) && !defined(__DOXYGEN__)
I2CDriver I2CD2 = {
  .i2c = I2C2
};
#endif
#if (CH32H417_I2C_USE_I2C3 == TRUE) && !defined(__DOXYGEN__)
I2CDriver I2CD3 = {
  .i2c = I2C3
};
#endif
#if (CH32H417_I2C_USE_I2C4 == TRUE) && !defined(__DOXYGEN__)
I2CDriver I2CD4 = {
  .i2c = I2C4
};
#endif

#endif /* HAL_USE_I2C == TRUE */

/** @} */
