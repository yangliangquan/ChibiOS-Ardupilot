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
 * @file    I2Cv1/hal_i2c_lld.h
 * @brief   CH32H417 I2C subsystem low level driver header.
 *
 * @details Blocking polled master implementation built on the WCH
 *          vendor I2C library, deadline based timeouts.
 *
 * @addtogroup I2C
 * @{
 */

#ifndef HAL_I2C_LLD_H
#define HAL_I2C_LLD_H

#if (HAL_USE_I2C == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   I2Cx driver enable switches.
 */
#if !defined(CH32H417_I2C_USE_I2C1) || defined(__DOXYGEN__)
#define CH32H417_I2C_USE_I2C1               FALSE
#endif
#if !defined(CH32H417_I2C_USE_I2C2) || defined(__DOXYGEN__)
#define CH32H417_I2C_USE_I2C2               FALSE
#endif
#if !defined(CH32H417_I2C_USE_I2C3) || defined(__DOXYGEN__)
#define CH32H417_I2C_USE_I2C3               FALSE
#endif
#if !defined(CH32H417_I2C_USE_I2C4) || defined(__DOXYGEN__)
#define CH32H417_I2C_USE_I2C4               FALSE
#endif

/**
 * @brief   I2C1 bus speed in Hz.
 */
#if !defined(CH32H417_I2C1_CLOCK_SPEED) || defined(__DOXYGEN__)
#define CH32H417_I2C1_CLOCK_SPEED           100000U
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_I2C_USE_I2C1 && !CH32H417_I2C_USE_I2C2 &&                    \
    !CH32H417_I2C_USE_I2C3 && !CH32H417_I2C_USE_I2C4
#error "I2C driver activated but no I2C peripheral assigned"
#endif

#if (CH32H417_I2C1_CLOCK_SPEED > 400000U)
#error "CH32H417_I2C1_CLOCK_SPEED above fast mode"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type representing an I2C address.
 */
typedef uint16_t i2caddr_t;

/**
 * @brief   Type of I2C Driver condition flags.
 */
typedef uint32_t i2cflags_t;

/**
 * @brief   I2C driver configuration structure.
 */
struct hal_i2c_config {
  /* End of the mandatory fields.*/
  uint32_t                  dummy;
};

/**
 * @brief   Type of a structure representing an I2C configuration.
 */
typedef struct hal_i2c_config I2CConfig;

/**
 * @brief   Type of a structure representing an I2C driver.
 */
typedef struct hal_i2c_driver I2CDriver;

/**
 * @brief   Low level fields of the I2C driver structure.
 */
#define I2C_DRIVER_EXT_FIELDS                                              \
  /* Pointer to the I2C registers block.*/                                 \
  I2C_TypeDef               *i2c;

/**
 * @brief   Structure representing an I2C driver.
 */
struct hal_i2c_driver {
  /**
   * @brief   Driver state.
   */
  i2cstate_t                state;
  /**
   * @brief   Current configuration data.
   */
  const I2CConfig           *config;
  /**
   * @brief   Error flags.
   */
  i2cflags_t                errors;
#if (I2C_USE_MUTUAL_EXCLUSION == TRUE) || defined(__DOXYGEN__)
  mutex_t                   mutex;
#endif
  I2C_DRIVER_EXT_FIELDS
  /* End of the mandatory fields.*/
};

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Get errors from I2C driver.
 */
#define i2c_lld_get_errors(i2cp) ((i2cp)->errors)

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_I2C_USE_I2C1 == TRUE) && !defined(__DOXYGEN__)
extern I2CDriver I2CD1;
#endif
#if (CH32H417_I2C_USE_I2C2 == TRUE) && !defined(__DOXYGEN__)
extern I2CDriver I2CD2;
#endif
#if (CH32H417_I2C_USE_I2C3 == TRUE) && !defined(__DOXYGEN__)
extern I2CDriver I2CD3;
#endif
#if (CH32H417_I2C_USE_I2C4 == TRUE) && !defined(__DOXYGEN__)
extern I2CDriver I2CD4;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void i2c_lld_init(void);
  void i2c_lld_start(I2CDriver *i2cp);
  void i2c_lld_stop(I2CDriver *i2cp);
  void i2c_lld_soft_stop(I2CDriver *i2cp);
  msg_t i2c_lld_master_transmit_timeout(I2CDriver *i2cp, i2caddr_t addr,
                                        const uint8_t *txbuf, size_t txbytes,
                                        uint8_t *rxbuf, size_t rxbytes,
                                        sysinterval_t timeout);
  msg_t i2c_lld_master_receive_timeout(I2CDriver *i2cp, i2caddr_t addr,
                                       uint8_t *rxbuf, size_t rxbytes,
                                       sysinterval_t timeout);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_I2C == TRUE */

#endif /* HAL_I2C_LLD_H */

/** @} */
