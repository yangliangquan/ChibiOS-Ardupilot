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
 * @file    hal_efl_lld.h
 * @brief   CH32H417 Embedded Flash subsystem low level driver header.
 *
 * @addtogroup HAL_EFL
 * @{
 */

#ifndef HAL_EFL_LLD_H
#define HAL_EFL_LLD_H

#if (HAL_USE_EFL == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    PLATFORM configuration options
 * @{
 */
/**
 * @brief   EFL1 driver enable switch.
 */
#if !defined(CH32H417_EFL_USE_EFL1) || defined(__DOXYGEN__)
#define CH32H417_EFL_USE_EFL1               FALSE
#endif

/**
 * @brief   Start address of the managed flash partition.
 * @note    Storage partition carved from the top of the 960KB
 *          CodeFlash: XIP alias 0x000D0000..0x000F0000, programming
 *          alias below. Matches the flash1 reservation in
 *          ld/CH32H417_FLASH.ld; resize both together when the
 *          ArduPilot storage needs are sized.
 */
#if !defined(CH32H417_EFL_BASE_ADDR) || defined(__DOXYGEN__)
#define CH32H417_EFL_BASE_ADDR             0x080D0000U
#endif

/**
 * @brief   Size of the managed flash partition in bytes.
 */
#if !defined(CH32H417_EFL_SIZE) || defined(__DOXYGEN__)
#define CH32H417_EFL_SIZE                  (128U * 1024U)
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_EFL_USE_EFL1
#error "EFL driver activated but no flash unit assigned"
#endif

/*===========================================================================*/
/* Driver macros.                                                            */
/*===========================================================================*/

/**
 * @brief   Low level fields of the embedded flash driver structure.
 */
#define efl_lld_driver_fields                                               \
  /* Dummy field, it is not needed.*/                                       \
  uint32_t                  dummy;

/**
 * @brief   Low level fields of the embedded flash configuration structure.
 */
#define efl_lld_config_fields                                               \
  /* Dummy configuration, it is not needed.*/                               \
  uint32_t                  dummy;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_EFL_USE_EFL1 == TRUE) && !defined(__DOXYGEN__)
extern EFlashDriver EFLD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void efl_lld_init(void);
  void efl_lld_start(EFlashDriver *eflp);
  void efl_lld_stop(EFlashDriver *eflp);
  const flash_descriptor_t *efl_lld_get_descriptor(void *instance);
  flash_error_t efl_lld_read(void *instance, flash_offset_t offset,
                             size_t n, uint8_t *rp);
  flash_error_t efl_lld_program(void *instance, flash_offset_t offset,
                                size_t n, const uint8_t *pp);
  flash_error_t efl_lld_start_erase_all(void *instance);
  flash_error_t efl_lld_start_erase_sector(void *instance,
                                           flash_sector_t sector);
  flash_error_t efl_lld_query_erase(void *instance, uint32_t *msec);
  flash_error_t efl_lld_verify_erase(void *instance, flash_sector_t sector);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_EFL == TRUE */

#endif /* HAL_EFL_LLD_H */

/** @} */
