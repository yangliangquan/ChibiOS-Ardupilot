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
 * @file    hal_efl_lld.c
 * @brief   CH32H417 Embedded Flash subsystem low level driver source.
 *
 * @details Uses the WCH vendor flash library. The erase granularity
 *          (4K/8K pages) follows FLASH_CFGR0.DBMODE and is detected
 *          at initialization time. Operations are implemented
 *          synchronously.
 *
 * @addtogroup HAL_EFL
 * @{
 */

#include "hal.h"

#if (HAL_USE_EFL == TRUE) || defined(__DOXYGEN__)

#include <string.h>
#include "ch32h417_flash.h"

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_EFL_USE_EFL1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   EFL1 driver identifier.
 */
EFlashDriver EFLD1;
#endif

/*===========================================================================*/
/* Driver local variables.                                                   */
/*===========================================================================*/

static flash_descriptor_t efl_descriptor;

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static flash_error_t flash_status_to_error(FLASH_Status status) {

  switch (status) {
  case FLASH_COMPLETE:
    return FLASH_NO_ERROR;
  case FLASH_BUSY:
    return FLASH_BUSY_ERASING;
  default:
    return FLASH_ERROR_PROGRAM;
  }
}

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level embedded flash driver initialization.
 *
 * @notapi
 */
void efl_lld_init(void) {
  bool dual_mode =
      ((*(volatile uint32_t *)FLASH_CFGR0_BASE) & (1UL << 28)) != 0UL;
  uint32_t sector_size = dual_mode ? 8192UL : 4096UL;

  efl_descriptor.attributes = FLASH_ATTR_ERASED_IS_ONE |
                              FLASH_ATTR_MEMORY_MAPPED |
                              FLASH_ATTR_REWRITABLE;
  /* Standard programming mode unit is 2 bytes. */
  efl_descriptor.page_size = 2UL;
  efl_descriptor.sectors_count = CH32H417_EFL_SIZE / sector_size;
  efl_descriptor.sectors = NULL;
  efl_descriptor.sectors_size = sector_size;
  efl_descriptor.address = (uint8_t *)CH32H417_EFL_BASE_ADDR;
  efl_descriptor.size = CH32H417_EFL_SIZE;
}

/**
 * @brief   Enables the flash device.
 *
 * @notapi
 */
void efl_lld_start(EFlashDriver *eflp) {

  (void)eflp;
  FLASH_Unlock();
}

/**
 * @brief   Disables the flash device.
 *
 * @notapi
 */
void efl_lld_stop(EFlashDriver *eflp) {

  (void)eflp;
  FLASH_Lock();
}

/**
 * @brief   Returns a flash descriptor.
 *
 * @notapi
 */
const flash_descriptor_t *efl_lld_get_descriptor(void *instance) {

  (void)instance;
  return &efl_descriptor;
}

/**
 * @brief   Reads data from the flash device.
 *
 * @notapi
 */
flash_error_t efl_lld_read(void *instance,
                           flash_offset_t offset,
                           size_t n,
                           uint8_t *rp) {

  (void)instance;

  if ((offset + n) > efl_descriptor.size) {
    return FLASH_ERROR_READ;
  }

  const uint8_t *src = (const uint8_t *)(CH32H417_EFL_BASE_ADDR + offset);
  memcpy(rp, src, n);

  return FLASH_NO_ERROR;
}

/**
 * @brief   Programs data into the flash device.
 *
 * @notapi
 */
flash_error_t efl_lld_program(void *instance,
                              flash_offset_t offset,
                              size_t n,
                              const uint8_t *pp) {
  flash_error_t err = FLASH_NO_ERROR;
  size_t i;

  (void)instance;

  if (((offset + n) > efl_descriptor.size) || ((n & 1U) != 0U) ||
      ((offset & 1U) != 0U)) {
    return FLASH_ERROR_PROGRAM;
  }

  for (i = 0; i < n; i += 2U) {
    uint32_t addr = CH32H417_EFL_BASE_ADDR + offset + i;
    uint16_t hw = (uint16_t)pp[i] | ((uint16_t)pp[i + 1U] << 8);

    err = flash_status_to_error(FLASH_ProgramHalfWord(addr, hw));
    if (err != FLASH_NO_ERROR) {
      break;
    }
  }

  return err;
}

/**
 * @brief   Starts a mass erase of the managed partition.
 *
 * @notapi
 */
flash_error_t efl_lld_start_erase_all(void *instance) {
  flash_sector_t s;

  (void)instance;

  for (s = 0; s < efl_descriptor.sectors_count; s++) {
    flash_error_t err = efl_lld_start_erase_sector(instance, s);

    if (err != FLASH_NO_ERROR) {
      return err;
    }
  }

  return FLASH_NO_ERROR;
}

/**
 * @brief   Starts an erase of a single sector.
 *
 * @notapi
 */
flash_error_t efl_lld_start_erase_sector(void *instance,
                                         flash_sector_t sector) {
  uint32_t addr;

  (void)instance;

  if (sector >= efl_descriptor.sectors_count) {
    return FLASH_ERROR_ERASE;
  }

  addr = CH32H417_EFL_BASE_ADDR + (sector * efl_descriptor.sectors_size);
  if (FLASH_ErasePage(addr) != FLASH_COMPLETE) {
    return FLASH_ERROR_ERASE;
  }

  return FLASH_NO_ERROR;
}

/**
 * @brief   Queries the erase status.
 * @note    The implementation is synchronous, the erase is completed
 *          before the start function returns.
 *
 * @notapi
 */
flash_error_t efl_lld_query_erase(void *instance, uint32_t *msec) {

  (void)instance;
  *msec = 0U;

  return FLASH_NO_ERROR;
}

/**
 * @brief   Verifies the erase of a single sector.
 *
 * @notapi
 */
flash_error_t efl_lld_verify_erase(void *instance, flash_sector_t sector) {
  flash_offset_t offset;
  uint32_t addr;

  (void)instance;

  if (sector >= efl_descriptor.sectors_count) {
    return FLASH_ERROR_VERIFY;
  }

  addr = CH32H417_EFL_BASE_ADDR + (sector * efl_descriptor.sectors_size);
  for (offset = 0; offset < efl_descriptor.sectors_size; offset++) {
    if (*(const volatile uint8_t *)(addr + offset) != 0xFFU) {
      return FLASH_ERROR_VERIFY;
    }
  }

  return FLASH_NO_ERROR;
}

#endif /* HAL_USE_EFL == TRUE */

/** @} */
