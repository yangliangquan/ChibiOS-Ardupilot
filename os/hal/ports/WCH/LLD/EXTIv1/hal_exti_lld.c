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
 * @file    EXTIv1/hal_exti_lld.c
 * @brief   CH32H417 EXTI subsystem low level driver source.
 *
 * @addtogroup EXTI
 * @{
 */

#include "hal.h"
#include "hal_exti_lld.h"

#if (CH32H417_EXTI_USE_EXTI == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_gpio.h"
#include "ch32h417_exti.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* Bit mask of the GPIO lines. */
#define EXTI_GPIO_MASK                      0x0000FFFFU

/*===========================================================================*/
/* Driver local variables and types.                                         */
/*===========================================================================*/

/**
 * @brief   Active configuration, NULL when the driver is stopped.
 */
static const EXTIConfig *exti_config;

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Maps a GPIO port to the AFIO source identifier.
 *
 * @param[in] port      GPIO port
 * @return              The AFIO port source value.
 */
static uint8_t exti_port_source(GPIO_TypeDef *port) {

  if (port == GPIOA) {
    return GPIO_PortSourceGPIOA;
  }
  if (port == GPIOB) {
    return GPIO_PortSourceGPIOB;
  }
  if (port == GPIOC) {
    return GPIO_PortSourceGPIOC;
  }
  if (port == GPIOD) {
    return GPIO_PortSourceGPIOD;
  }
  if (port == GPIOE) {
    return GPIO_PortSourceGPIOE;
  }
  osalDbgAssert(port == GPIOF, "invalid EXTI port");
  return GPIO_PortSourceGPIOF;
}

/**
 * @brief   Applies the configuration of one channel.
 *
 * @param[in] line      EXTI line
 * @param[in] enable    enable or disable the line
 */
static void exti_apply(uint32_t line, bool enable) {
  const EXTIChannelConfig *cfg = &exti_config->channels[line];
  EXTI_InitTypeDef init;

  osalDbgAssert(cfg->pad == line, "pad and line mismatch");

  init.EXTI_Line = 1U << line;
  init.EXTI_Mode = EXTI_Mode_Interrupt;
  switch (cfg->trigger) {
  case EXTI_TRIGGER_RISING:
    init.EXTI_Trigger = EXTI_Trigger_Rising;
    break;
  case EXTI_TRIGGER_FALLING:
    init.EXTI_Trigger = EXTI_Trigger_Falling;
    break;
  default:
    init.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    break;
  }
  init.EXTI_LineCmd = enable ? ENABLE : DISABLE;

  if (enable) {
    GPIO_EXTILineConfig(exti_port_source(cfg->port), (uint8_t)cfg->pad);
  }
  EXTI_Init(&init);

  if (enable) {
    EXTI_ClearITPendingBit(init.EXTI_Line);
    NVIC_EnableIRQ((line < 8U) ? EXTI7_0_IRQn : EXTI15_8_IRQn);
  }
}

/**
 * @brief   Dispatches the pending lines to the configured callbacks.
 *
 * @param[in] mask      group mask, the pending bits are cleared
 */
static void exti_dispatch(uint32_t mask) {
  uint32_t pending = EXTI->INTFR & mask;
  uint32_t line;

  /* Write one to clear the handled pending bits. */
  EXTI->INTFR = pending;

  if (exti_config == NULL) {
    return;
  }

  for (line = 0U; line < EXTI_MAX_CHANNELS; line++) {
    if ((pending & (1U << line)) != 0U) {
      if (exti_config->channels[line].callback != NULL) {
        exti_config->channels[line].callback(line);
      }
    }
  }
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   EXTI low level initialization.
 *
 * @notapi
 */
void extiInit(void) {

  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO, ENABLE);
  EXTI->INTFR = EXTI_GPIO_MASK;
}

/**
 * @brief   Configures and activates the EXTI channels.
 *
 * @param[in] config    driver configuration
 *
 * @notapi
 */
void extiStart(const EXTIConfig *config) {
  uint32_t line;

  osalDbgCheck(config != NULL);

  extiInit();
  exti_config = config;

  for (line = 0U; line < EXTI_MAX_CHANNELS; line++) {
    if (config->channels[line].trigger != EXTI_TRIGGER_DISABLED) {
      exti_apply(line, true);
    }
  }
}

/**
 * @brief   Deactivates the EXTI channels.
 *
 * @notapi
 */
void extiStop(void) {
  uint32_t line;

  if (exti_config != NULL) {
    for (line = 0U; line < EXTI_MAX_CHANNELS; line++) {
      if (exti_config->channels[line].trigger != EXTI_TRIGGER_DISABLED) {
        exti_apply(line, false);
      }
    }
    exti_config = NULL;
  }

  NVIC_DisableIRQ(EXTI7_0_IRQn);
  NVIC_DisableIRQ(EXTI15_8_IRQn);
  EXTI->INTFR = EXTI_GPIO_MASK;
}

/**
 * @brief   Enables one EXTI channel.
 *
 * @param[in] line      EXTI line
 *
 * @notapi
 */
void extiChannelEnable(uint32_t line) {

  osalDbgCheck(line < EXTI_MAX_CHANNELS);
  osalDbgCheck(exti_config != NULL);

  exti_apply(line, true);
}

/**
 * @brief   Disables one EXTI channel.
 *
 * @param[in] line      EXTI line
 *
 * @notapi
 */
void extiChannelDisable(uint32_t line) {

  osalDbgCheck(line < EXTI_MAX_CHANNELS);
  osalDbgCheck(exti_config != NULL);

  exti_apply(line, false);
}

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

OSAL_IRQ_HANDLER(EXTI7_0_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  exti_dispatch(0x00FFU);

  OSAL_IRQ_EPILOGUE();
}

OSAL_IRQ_HANDLER(EXTI15_8_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  exti_dispatch(0xFF00U);

  OSAL_IRQ_EPILOGUE();
}

#endif /* CH32H417_EXTI_USE_EXTI == TRUE */

/** @} */
