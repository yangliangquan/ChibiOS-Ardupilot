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
 * @file    ECDCv1/hal_crypto_lld.c
 * @brief   CH32H417 CRYPTO subsystem low level driver source.
 *
 * @details One transient AES key (id 0), single-block SFR operations
 *          through the vendor library (its ECDC_Init performs the key
 *          expansion pulse plus the required settle delay), completion
 *          polled on ECDC_FLAG_Single_END with a bounded spin.
 *          ECB loops over blocks, CTR = software counter + ECB + XOR.
 *
 * @addtogroup CRYPTO
 * @{
 */

#include <string.h>

#include "hal.h"

#if (HAL_USE_CRY == TRUE) || defined(__DOXYGEN__)

#include "ch32h417_ecdc.h"

/*===========================================================================*/
/* Driver local definitions.                                                 */
/*===========================================================================*/

/* Bounded completion poll for one SFR block operation. */
#define ECDC_SPIN_LIMIT                     5000000U

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_CRY_USE_ECDC == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief   CRYD1 driver identifier.
 */
CRYDriver CRYD1;
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

static inline uint32_t read_be32(const uint8_t *p) {

  return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline void write_be32(uint8_t *p, uint32_t v) {

  p[0] = (uint8_t)(v >> 24);
  p[1] = (uint8_t)(v >> 16);
  p[2] = (uint8_t)(v >> 8);
  p[3] = (uint8_t)v;
}

#if (CRY_LLD_SUPPORTS_AES == TRUE) ||                                       \
    (CRY_LLD_SUPPORTS_AES_ECB == TRUE) ||                                   \
    (CRY_LLD_SUPPORTS_AES_CTR == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Executes one 128-bit block on the ECDC SFR engine.
 *
 * @return              CRY_NOERROR or CRY_ERR_OP_FAILURE on timeout.
 */
static cryerror_t ecdc_block(CRYDriver *cryp, bool decrypt,
                             const uint8_t *in, uint8_t *out) {
  ECDC_InitTypeDef init;
  ECDC_KEY_TypeDef key;
  uint32_t words[4];
  ECDC_KeyLen_TypeDef klen;
  uint32_t spin;

  /* Key bytes (big-endian) into the named register words. */
  key.KEY_127T96 = read_be32(&cryp->k[0]);
  key.KEY_95T64 = read_be32(&cryp->k[4]);
  key.KEY_63T32 = read_be32(&cryp->k[8]);
  key.KEY_31T0 = read_be32(&cryp->k[12]);
  key.KEY_159T128 = read_be32(&cryp->k[16]);
  key.KEY_191T160 = read_be32(&cryp->k[20]);
  key.KEY_223T192 = read_be32(&cryp->k[24]);
  key.KEY_255T224 = read_be32(&cryp->k[28]);

  switch (cryp->kbytes) {
  case 20U:
    klen = ECDCKeyLen_192b;
    break;
  case 24U:
    klen = ECDCKeyLen_256b;
    break;
  default:
    klen = ECDCKeyLen_128b;
    break;
  }

  init.ExcuteMode = decrypt ? ECDC_SingleTime_Decrypt :
                              ECDC_SingleTime_Encrypt;
  init.Algorithm = ECDCAlgorithm_AES;
  init.BlockCipherMode = ECDCBlockCipherMode_ECB;
  init.KeyLen = klen;
  init.ExcuteEndian = ECDCExcuteEndian_Big;   /* mandatory for SFR mode */
  init.Key = &key;
  init.IV = NULL;

  /* Configures CTRL/key, runs the key expansion pulse and arms the
     execute mode (vendor sequence with its settle delay). */
  ECDC_Init(&init);

  /* Block words: pointer[3] holds bits [127:96] (see the vendor
     ECDC_SingleWR_RawData mapping). */
  words[3] = read_be32(in + 0);
  words[2] = read_be32(in + 4);
  words[1] = read_be32(in + 8);
  words[0] = read_be32(in + 12);
  ECDC_SingleWR_RawData(words);

  spin = ECDC_SPIN_LIMIT;
  while (ECDC_GetFlagStatus(ECDC_FLAG_Single_END) == RESET) {
    if (--spin == 0U) {
      return CRY_ERR_OP_FAILURE;
    }
  }
  ECDC_ClearFlag(ECDC_FLAG_Single_END);

  write_be32(out + 0, ECDC->SGRT_127T96);
  write_be32(out + 4, ECDC->SGRT_95T64);
  write_be32(out + 8, ECDC->SGRT_63T32);
  write_be32(out + 12, ECDC->SGRT_31T0);

  return CRY_NOERROR;
}
#endif /* AES related capabilities */

#if (CRY_LLD_SUPPORTS_AES_CTR == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   CTR mode built on the ECB engine (software counter).
 */
static cryerror_t ctr_common(CRYDriver *cryp, size_t size,
                             const uint8_t *in, uint8_t *out,
                             const uint8_t *iv) {
  uint8_t ctr[16];
  uint8_t ks[16];
  size_t i;
  unsigned j;
  cryerror_t r;

  if ((size % 16U) != 0U) {
    return CRY_ERR_OP_FAILURE;
  }

  memcpy(ctr, iv, sizeof ctr);

  for (i = 0U; i < size; i += 16U) {
    /* The keystream block is always AES_ECB(counter). */
    r = ecdc_block(cryp, false, ctr, ks);
    if (r != CRY_NOERROR) {
      return r;
    }

    for (j = 0U; j < 16U; j++) {
      out[i + j] = in[i + j] ^ ks[j];
    }

    /* Increment the counter as a big-endian 128-bit number. */
    for (j = 16U; j-- > 0U;) {
      if (++ctr[j] != 0U) {
        break;
      }
    }
  }

  return CRY_NOERROR;
}
#endif /* CRY_LLD_SUPPORTS_AES_CTR */

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Low level CRYPTO driver initialization.
 *
 * @notapi
 */
void cry_lld_init(void) {
}

/**
 * @brief   CRYPTO driver start.
 *
 * @notapi
 */
void cry_lld_start(CRYDriver *cryp) {

  (void)cryp;

  RCC_HB2PeriphClockCmd(RCC_HB2Periph_ECDC, ENABLE);
}

/**
 * @brief   CRYPTO driver stop.
 *
 * @notapi
 */
void cry_lld_stop(CRYDriver *cryp) {

  cryp->key_valid = false;
  RCC_HB2PeriphClockCmd(RCC_HB2Periph_ECDC, DISABLE);
}

#if (CRY_LLD_SUPPORTS_AES == TRUE) ||                                       \
    (CRY_LLD_SUPPORTS_AES_ECB == TRUE) ||                                   \
    (CRY_LLD_SUPPORTS_AES_CTR == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Loads the transient AES key.
 *
 * @param[in] cryp      pointer to a @p CRYDriver object
 * @param[in] size      key size in bytes (16, 20 or 24)
 * @param[in] keyp      pointer to the key material
 * @return              The operation status.
 *
 * @notapi
 */
cryerror_t cry_lld_aes_loadkey(CRYDriver *cryp,
                               size_t size,
                               const uint8_t *keyp) {

  if ((size != 16U) && (size != 20U) && (size != 24U)) {
    return CRY_ERR_INV_KEY_SIZE;
  }

  memcpy(cryp->k, keyp, size);
  cryp->kbytes = (uint8_t)size;
  cryp->key_valid = true;

  return CRY_NOERROR;
}
#endif

#if (CRY_LLD_SUPPORTS_AES == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   One block AES encryption.
 *
 * @notapi
 */
cryerror_t cry_lld_encrypt_AES(CRYDriver *cryp,
                               crykey_t key_id,
                               const uint8_t *in,
                               uint8_t *out) {

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }

  return ecdc_block(cryp, false, in, out);
}

/**
 * @brief   One block AES decryption.
 *
 * @notapi
 */
cryerror_t cry_lld_decrypt_AES(CRYDriver *cryp,
                               crykey_t key_id,
                               const uint8_t *in,
                               uint8_t *out) {

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }

  return ecdc_block(cryp, true, in, out);
}
#endif

#if (CRY_LLD_SUPPORTS_AES_ECB == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   AES-ECB encryption.
 * @note    size is in bytes and must be a multiple of 16.
 *
 * @notapi
 */
cryerror_t cry_lld_encrypt_AES_ECB(CRYDriver *cryp,
                                   crykey_t key_id,
                                   size_t size,
                                   const uint8_t *in,
                                   uint8_t *out) {
  size_t i;
  cryerror_t r;

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }
  if ((size % 16U) != 0U) {
    return CRY_ERR_OP_FAILURE;
  }

  for (i = 0U; i < size; i += 16U) {
    r = ecdc_block(cryp, false, in + i, out + i);
    if (r != CRY_NOERROR) {
      return r;
    }
  }

  return CRY_NOERROR;
}

/**
 * @brief   AES-ECB decryption.
 * @note    size is in bytes and must be a multiple of 16.
 *
 * @notapi
 */
cryerror_t cry_lld_decrypt_AES_ECB(CRYDriver *cryp,
                                   crykey_t key_id,
                                   size_t size,
                                   const uint8_t *in,
                                   uint8_t *out) {
  size_t i;
  cryerror_t r;

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }
  if ((size % 16U) != 0U) {
    return CRY_ERR_OP_FAILURE;
  }

  for (i = 0U; i < size; i += 16U) {
    r = ecdc_block(cryp, true, in + i, out + i);
    if (r != CRY_NOERROR) {
      return r;
    }
  }

  return CRY_NOERROR;
}
#endif

#if (CRY_LLD_SUPPORTS_AES_CTR == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   AES-CTR encryption.
 * @note    size is in bytes and must be a multiple of 16, iv is the
 *          128-bit big-endian counter block.
 *
 * @notapi
 */
cryerror_t cry_lld_encrypt_AES_CTR(CRYDriver *cryp,
                                   crykey_t key_id,
                                   size_t size,
                                   const uint8_t *in,
                                   uint8_t *out,
                                   const uint8_t *iv) {

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }

  return ctr_common(cryp, size, in, out, iv);
}

/**
 * @brief   AES-CTR decryption (identical to encryption).
 *
 * @notapi
 */
cryerror_t cry_lld_decrypt_AES_CTR(CRYDriver *cryp,
                                   crykey_t key_id,
                                   size_t size,
                                   const uint8_t *in,
                                   uint8_t *out,
                                   const uint8_t *iv) {

  if (key_id != 0U) {
    return CRY_ERR_INV_KEY_ID;
  }
  if (!cryp->key_valid) {
    return CRY_ERR_INV_KEY_TYPE;
  }

  return ctr_common(cryp, size, in, out, iv);
}
#endif

#endif /* HAL_USE_CRY == TRUE */

/** @} */
