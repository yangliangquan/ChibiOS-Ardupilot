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
 * @file    GPIOv1/hal_pal_lld.h
 * @brief   CH32H417 PAL subsystem low level driver header.
 *
 * @details GPIO model: STM32F1-style 4-bit configuration
 *          (CFGLR/CFGHR) plus per-pin alternate function selection in
 *          AFIO->GPIOx_AFLR/AFHR.
 *
 * @addtogroup PAL
 * @{
 */

#ifndef HAL_PAL_LLD_H
#define HAL_PAL_LLD_H

#if (HAL_USE_PAL == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Unsupported modes and specific modes                                      */
/*===========================================================================*/

/* Specifies palInit() without parameter.*/
#define PAL_NEW_INIT

/*===========================================================================*/
/* I/O Ports Types and constants.                                            */
/*===========================================================================*/

/**
 * @name    Port related definitions
 * @{
 */
/**
 * @brief   Width, in bits, of an I/O port.
 */
#define PAL_IOPORTS_WIDTH           16U

/**
 * @brief   Whole port mask.
 */
#define PAL_WHOLE_PORT              ((ioportmask_t)0xFFFFU)
/** @} */

/**
 * @name    Line handling macros
 * @{
 */
#define PAL_LINE(port, pad)                                                 \
  ((ioline_t)((uint32_t)(port)) | ((uint32_t)(pad)))

#define PAL_PORT(line)                                                      \
  ((GPIO_TypeDef *)(((uint32_t)(line)) & 0xFFFFFFF0U))

#define PAL_PAD(line)                                                       \
  ((uint32_t)((uint32_t)(line) & 0x0000000FU))

#define PAL_NOLINE                      0U
/** @} */

/**
 * @brief   Generic I/O ports static initializer.
 */
typedef struct {

} PALConfig;

/**
 * @brief   Digital I/O port sized unsigned type.
 */
typedef uint32_t ioportmask_t;

/**
 * @brief   Digital I/O modes.
 */
typedef uint32_t iomode_t;

/**
 * @brief   Type of an I/O line.
 */
typedef uint32_t ioline_t;

/**
 * @brief   Port Identifier.
 */
typedef uint32_t ioportid_t;

/**
 * @brief   Type of a pad identifier.
 */
typedef uint32_t iopadid_t;

/*===========================================================================*/
/* I/O Ports Identifiers.                                                    */
/*===========================================================================*/

#define IOPORT1         ((ioportid_t)GPIOA)
#define IOPORT2         ((ioportid_t)GPIOB)
#define IOPORT3         ((ioportid_t)GPIOC)
#define IOPORT4         ((ioportid_t)GPIOD)
#define IOPORT5         ((ioportid_t)GPIOE)
#define IOPORT6         ((ioportid_t)GPIOF)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    PAL configuration options
 * @{
 */
/**
 * @brief   Alternate function mode with push-pull output driver.
 * @note    The AF number is encoded in bits [7:4].
 */
#define PAL_MODE_ALTERNATE(n)                                               \
  (0x100U | ((iomode_t)(n) << 4))

/**
 * @brief   Alternate function mode with open drain output driver.
 * @note    The AF number is encoded in bits [7:4].
 */
#define PAL_MODE_ALTERNATE_OPENDRAIN(n)                                     \
  (0x200U | ((iomode_t)(n) << 4))

/**
 * @brief   Alternate function input mode.
 * @note    Used for receiver pins, the AF number is encoded in
 *          bits [7:4].
 */
#define PAL_MODE_ALTERNATE_INPUT(n)                                         \
  (0x300U | ((iomode_t)(n) << 4))
/** @} */

/*===========================================================================*/
/* Implementation, some of the following macros could be implemented as      */
/* functions, if so please put them in pal_lld.c.                            */
/*===========================================================================*/

#define pal_lld_init() _pal_lld_init()

#define pal_lld_setgroupmode(port, mask, offset, mode)                      \
  _pal_lld_setgroupmode(port, mask << offset, mode)

#ifdef __cplusplus
extern "C" {
#endif
  void _pal_lld_init(void);
  void _pal_lld_setgroupmode(ioportid_t port,
                             ioportmask_t mask,
                             iomode_t mode);
  uint32_t pal_lld_readport(ioportid_t port);
  uint32_t pal_lld_readlatch(ioportid_t port);
  void pal_lld_writeport(ioportid_t port, ioportmask_t bits);
  void pal_lld_setport(ioportid_t port, ioportmask_t bits);
  void pal_lld_clearport(ioportid_t port, ioportmask_t bits);
  void pal_lld_toggleport(ioportid_t port, ioportmask_t bits);
  uint32_t pal_lld_readgroup(ioportid_t port,
                             ioportmask_t mask,
                             unsigned offset);
  void pal_lld_writegroup(ioportid_t port,
                          ioportmask_t mask,
                          unsigned offset,
                          ioportmask_t bits);
  ioportmask_t pal_lld_readpad(ioportid_t port, iopadid_t pad);
  void pal_lld_writepad(ioportid_t port, iopadid_t pad, ioportmask_t bit);
  void pal_lld_setpad(ioportid_t port, iopadid_t pad);
  void pal_lld_clearpad(ioportid_t port, iopadid_t pad);
  void pal_lld_togglepad(ioportid_t port, iopadid_t pad);
  void pal_lld_setpadmode(ioportid_t port, iopadid_t pad, iomode_t mode);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_PAL == TRUE */

#endif /* HAL_PAL_LLD_H */

/** @} */
