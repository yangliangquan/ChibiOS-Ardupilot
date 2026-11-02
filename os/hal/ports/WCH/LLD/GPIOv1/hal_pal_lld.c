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
 * @file    GPIOv1/hal_pal_lld.c
 * @brief   CH32H417 PAL subsystem low level driver source.
 *
 * @addtogroup PAL
 * @{
 */

#include "hal.h"

#if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_gpio.h"

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static void gpio_clock_enable(void) {

  RCC_HB2PeriphClockCmd(RCC_HB2Periph_AFIO |
                        RCC_HB2Periph_GPIOA |
                        RCC_HB2Periph_GPIOB |
                        RCC_HB2Periph_GPIOC |
                        RCC_HB2Periph_GPIOD |
                        RCC_HB2Periph_GPIOE |
                        RCC_HB2Periph_GPIOF,
                        ENABLE);
}

static void set_pad_mode(ioportid_t port, iopadid_t pad, iomode_t mode) {
  GPIO_TypeDef *gp = (GPIO_TypeDef *)port;
  GPIO_InitTypeDef init;
  uint32_t af = (mode >> 4) & 0x0FU;

  init.GPIO_Pin = (uint16_t)(1U << pad);
  init.GPIO_Speed = GPIO_Speed_Very_High;

  if ((mode & 0x300U) == 0x300U) {
    init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_PinAFConfig(gp, (uint8_t)pad, (uint8_t)af);
  }
  else if ((mode & 0x300U) == 0x200U) {
    init.GPIO_Mode = GPIO_Mode_AF_OD;
    GPIO_PinAFConfig(gp, (uint8_t)pad, (uint8_t)af);
  }
  else if ((mode & 0x300U) == 0x100U) {
    init.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_PinAFConfig(gp, (uint8_t)pad, (uint8_t)af);
  }
  else {
    switch (mode) {
    case PAL_MODE_INPUT:
      init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
      break;
    case PAL_MODE_INPUT_PULLUP:
      init.GPIO_Mode = GPIO_Mode_IPU;
      break;
    case PAL_MODE_INPUT_PULLDOWN:
      init.GPIO_Mode = GPIO_Mode_IPD;
      break;
    case PAL_MODE_INPUT_ANALOG:
      init.GPIO_Mode = GPIO_Mode_AIN;
      break;
    case PAL_MODE_OUTPUT_PUSHPULL:
      init.GPIO_Mode = GPIO_Mode_Out_PP;
      break;
    case PAL_MODE_OUTPUT_OPENDRAIN:
      init.GPIO_Mode = GPIO_Mode_Out_OD;
      break;
    case PAL_MODE_UNCONNECTED:
      init.GPIO_Mode = GPIO_Mode_IPU;
      break;
    case PAL_MODE_RESET:
    default:
      init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
      break;
    }
  }

  GPIO_Init(gp, &init);
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level PAL subsystem initialization.
 *
 * @notapi
 */
void _pal_lld_init(void) {

  gpio_clock_enable();
}

/**
 * @brief   Reads the physical I/O port states.
 *
 * @param[in] port      port identifier
 * @return              The port bits.
 *
 * @notapi
 */
uint32_t pal_lld_readport(ioportid_t port) {

  return ((GPIO_TypeDef *)port)->INDR;
}

/**
 * @brief   Reads the output latch.
 *
 * @param[in] port      port identifier
 * @return              The latched logical states.
 *
 * @notapi
 */
uint32_t pal_lld_readlatch(ioportid_t port) {

  return ((GPIO_TypeDef *)port)->OUTDR;
}

/**
 * @brief   Writes a bits mask on a I/O port.
 *
 * @param[in] port      port identifier
 * @param[in] bits      bits to be written on the specified port
 *
 * @notapi
 */
void pal_lld_writeport(ioportid_t port, ioportmask_t bits) {

  ((GPIO_TypeDef *)port)->OUTDR = bits;
}

/**
 * @brief   Sets a bits mask on a I/O port.
 *
 * @param[in] port      port identifier
 * @param[in] bits      bits to be ORed on the specified port
 *
 * @notapi
 */
void pal_lld_setport(ioportid_t port, ioportmask_t bits) {

  ((GPIO_TypeDef *)port)->BSHR = bits;
}

/**
 * @brief   Clears a bits mask on a I/O port.
 *
 * @param[in] port      port identifier
 * @param[in] bits      bits to be cleared on the specified port
 *
 * @notapi
 */
void pal_lld_clearport(ioportid_t port, ioportmask_t bits) {

  ((GPIO_TypeDef *)port)->BCR = bits;
}

/**
 * @brief   Toggles a bits mask on a I/O port.
 *
 * @param[in] port      port identifier
 * @param[in] bits      bits to be XORed on the specified port
 *
 * @notapi
 */
void pal_lld_toggleport(ioportid_t port, ioportmask_t bits) {
  GPIO_TypeDef *gp = (GPIO_TypeDef *)port;

  gp->OUTDR = gp->OUTDR ^ bits;
}

/**
 * @brief   Reads a group of bits.
 *
 * @param[in] port      port identifier
 * @param[in] mask      group mask
 * @param[in] offset    group bit offset within the port
 * @return              The group logical states.
 *
 * @notapi
 */
uint32_t pal_lld_readgroup(ioportid_t port,
                           ioportmask_t mask,
                           unsigned offset) {

  return (((GPIO_TypeDef *)port)->INDR >> offset) & mask;
}

/**
 * @brief   Writes a group of bits.
 *
 * @param[in] port      port identifier
 * @param[in] mask      group mask
 * @param[in] offset    group bit offset within the port
 * @param[in] bits      bits to be written.
 *
 * @notapi
 */
void pal_lld_writegroup(ioportid_t port,
                        ioportmask_t mask,
                        unsigned offset,
                        ioportmask_t bits) {
  GPIO_TypeDef *gp = (GPIO_TypeDef *)port;
  uint32_t v = gp->OUTDR;

  gp->OUTDR = (v & ~(mask << offset)) | ((bits & mask) << offset);
}

/**
 * @brief   Reads a logical state from an I/O pad.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 * @return              The logical state.
 *
 * @notapi
 */
ioportmask_t pal_lld_readpad(ioportid_t port, iopadid_t pad) {

  return (((GPIO_TypeDef *)port)->INDR >> pad) & 1U;
}

/**
 * @brief   Writes a logical state on an output pad.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 * @param[in] bit       logical value
 *
 * @notapi
 */
void pal_lld_writepad(ioportid_t port, iopadid_t pad, ioportmask_t bit) {
  GPIO_TypeDef *gp = (GPIO_TypeDef *)port;

  if (bit != 0U) {
    gp->BSHR = 1UL << pad;
  }
  else {
    gp->BCR = 1UL << pad;
  }
}

/**
 * @brief   Sets a pad logical state to @p PAL_HIGH.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 *
 * @notapi
 */
void pal_lld_setpad(ioportid_t port, iopadid_t pad) {

  ((GPIO_TypeDef *)port)->BSHR = 1UL << pad;
}

/**
 * @brief   Clears a pad logical state to @p PAL_LOW.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 *
 * @notapi
 */
void pal_lld_clearpad(ioportid_t port, iopadid_t pad) {

  ((GPIO_TypeDef *)port)->BCR = 1UL << pad;
}

/**
 * @brief   Toggles a pad logical state.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 *
 * @notapi
 */
void pal_lld_togglepad(ioportid_t port, iopadid_t pad) {
  GPIO_TypeDef *gp = (GPIO_TypeDef *)port;

  gp->OUTDR = gp->OUTDR ^ (1UL << pad);
}

/**
 * @brief   Pad mode setup.
 *
 * @param[in] port      port identifier
 * @param[in] pad       pad number within the port
 * @param[in] mode      pad mode
 *
 * @notapi
 */
void pal_lld_setpadmode(ioportid_t port, iopadid_t pad, iomode_t mode) {

  set_pad_mode(port, pad, mode);
}

/**
 * @brief   Pads group mode setup.
 *
 * @param[in] port      port identifier
 * @param[in] mask      group mask (already shifted)
 * @param[in] mode      group mode
 *
 * @notapi
 */
void _pal_lld_setgroupmode(ioportid_t port,
                           ioportmask_t mask,
                           iomode_t mode) {
  unsigned i;

  for (i = 0U; i < PAL_IOPORTS_WIDTH; i++) {
    if ((mask & (1UL << i)) != 0U) {
      set_pad_mode(port, i, mode);
    }
  }
}

#endif /* HAL_USE_PAL == TRUE */

/** @} */
