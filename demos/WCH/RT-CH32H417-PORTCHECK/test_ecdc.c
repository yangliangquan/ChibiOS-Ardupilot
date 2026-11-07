/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.

    This file is part of ChibiOS.

    ChibiOS is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file    test_ecdc.c
 * @brief   CH32H417 V5F port check: ECDC test thread.
 *
 * @details Checks the AES engine of the board against the published
 *          test vectors:
 *          - the ECB mode of FIPS-197 appendix B and of the appendix C
 *            vectors of the 128, 192 and 256 bit keys,
 *          - the CTR mode of the NIST SP 800-38A vector F.5.1.
 *
 *          The engine is used through the generic crypto driver, the
 *          key is loaded as a transient key and the operations are
 *          checked byte by byte.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Period of the measurement round. */
#define ECDC_TEST_ROUND_MS    2000U

/* Key identifiers of the transient keys. */
#define ECDC_TEST_KEY_128     0U
#define ECDC_TEST_KEY_192     1U
#define ECDC_TEST_KEY_256     2U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t cry_blocks;
volatile uint32_t cry_failures;
volatile uint32_t cry_rounds;
volatile uint32_t cry_last_error;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/* The crypto unit of this port has no configuration fields. */
static const CRYConfig cry_config;

/* FIPS-197 appendix B, the 128 bit key. */
static const uint8_t ec_key128[16] = {
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
  0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU
};

static const uint8_t ec_plain128[16] = {
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
  0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU
};

static const uint8_t ec_cipher128[16] = {
  0x69U, 0xC4U, 0xE0U, 0xD8U, 0x6AU, 0x7BU, 0x04U, 0x30U,
  0xD8U, 0xCDU, 0xB7U, 0x80U, 0x70U, 0xB4U, 0xC5U, 0x5AU
};

/* FIPS-197 appendix C, the 192 bit key. */
static const uint8_t ec_key192[24] = {
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
  0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU,
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U
};

static const uint8_t ec_cipher192[16] = {
  0xDDU, 0xA9U, 0x7CU, 0xA4U, 0x86U, 0x4CU, 0xDFU, 0xE0U,
  0x6EU, 0xAFU, 0x70U, 0xA0U, 0xECU, 0x0DU, 0x71U, 0x91U
};

/* FIPS-197 appendix C, the 256 bit key. */
static const uint8_t ec_key256[32] = {
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
  0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU,
  0x00U, 0x11U, 0x22U, 0x33U, 0x44U, 0x55U, 0x66U, 0x77U,
  0x88U, 0x99U, 0xAAU, 0xBBU, 0xCCU, 0xDDU, 0xEEU, 0xFFU
};

static const uint8_t ec_cipher256[16] = {
  0x8EU, 0xA2U, 0xB7U, 0xCAU, 0x51U, 0x67U, 0x45U, 0xBFU,
  0xEAU, 0xFCU, 0x49U, 0x90U, 0x4BU, 0x49U, 0x60U, 0x89U
};

/* NIST SP 800-38A, the CTR mode with the 128 bit key. */
static const uint8_t ctr_key128[16] = {
  0x2BU, 0x7EU, 0x15U, 0x16U, 0x28U, 0xAEU, 0xD2U, 0xA6U,
  0xABU, 0xF7U, 0x15U, 0x88U, 0x09U, 0xCFU, 0x4FU, 0x3CU
};

static const uint8_t ctr_counter[16] = {
  0xF0U, 0xF1U, 0xF2U, 0xF3U, 0xF4U, 0xF5U, 0xF6U, 0xF7U,
  0xF8U, 0xF9U, 0xFAU, 0xFBU, 0xFCU, 0xFDU, 0xFEU, 0xFFU
};

static const uint8_t ctr_plain[16] = {
  0x6BU, 0xC1U, 0xBEU, 0xE2U, 0x2EU, 0x40U, 0x9FU, 0x96U,
  0xE9U, 0x3DU, 0x7EU, 0x11U, 0x73U, 0x93U, 0x17U, 0x2AU
};

static const uint8_t ctr_cipher[16] = {
  0x87U, 0x4DU, 0x61U, 0x91U, 0xB6U, 0x20U, 0xE3U, 0x26U,
  0x1BU, 0xEFU, 0x68U, 0x64U, 0x99U, 0x0DU, 0xB6U, 0xCEU
};

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   Compares two buffers and returns the index of the first
 *          difference, or the length when they are equal.
 */
static unsigned ec_diff(const uint8_t *a, const uint8_t *b, unsigned len) {
  unsigned i;

  for (i = 0U; i < len; i++) {
    if (a[i] != b[i]) {
      return i;
    }
  }

  return len;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waEcdcTest, 1024);

static THD_FUNCTION(ecdc_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  uint8_t out[16];
  cryerror_t err;
  unsigned diff;

  (void)arg;

  if (cryStart(&CRYD1, &cry_config) != MSG_OK) {
    chprintf(chp, "ECDC: driver start failed\r\n");
    return;
  }

  while (true) {
    /* The ECB mode, the three key sizes of FIPS-197. */
    err = cryLoadAESTransientKey(&CRYD1, sizeof ec_key128, ec_key128);
    if (err == CRY_NOERROR) {
      err = cryEncryptAES_ECB(&CRYD1, ECDC_TEST_KEY_128, sizeof ec_plain128,
                              ec_plain128, out);
    }
    cry_blocks++;
    diff = (err == CRY_NOERROR) ? ec_diff(out, ec_cipher128, sizeof out) : 0U;
    if ((err != CRY_NOERROR) || (diff != sizeof out)) {
      cry_failures++;
      cry_last_error = (uint32_t)err;
      chprintf(chp, "ECDC: ECB 128 failed (err %u, byte %u)\r\n",
               (unsigned)err, (unsigned)diff);
    }
    else {
      chprintf(chp, "ECDC: ECB 128 ok\r\n");
    }

    err = cryLoadAESTransientKey(&CRYD1, sizeof ec_key192, ec_key192);
    if (err == CRY_NOERROR) {
      err = cryEncryptAES_ECB(&CRYD1, ECDC_TEST_KEY_192, sizeof ec_plain128,
                              ec_plain128, out);
    }
    cry_blocks++;
    diff = (err == CRY_NOERROR) ? ec_diff(out, ec_cipher192, sizeof out) : 0U;
    if ((err != CRY_NOERROR) || (diff != sizeof out)) {
      cry_failures++;
      cry_last_error = (uint32_t)err;
      chprintf(chp, "ECDC: ECB 192 failed (err %u, byte %u)\r\n",
               (unsigned)err, (unsigned)diff);
    }
    else {
      chprintf(chp, "ECDC: ECB 192 ok\r\n");
    }

    err = cryLoadAESTransientKey(&CRYD1, sizeof ec_key256, ec_key256);
    if (err == CRY_NOERROR) {
      err = cryEncryptAES_ECB(&CRYD1, ECDC_TEST_KEY_256, sizeof ec_plain128,
                              ec_plain128, out);
    }
    cry_blocks++;
    diff = (err == CRY_NOERROR) ? ec_diff(out, ec_cipher256, sizeof out) : 0U;
    if ((err != CRY_NOERROR) || (diff != sizeof out)) {
      cry_failures++;
      cry_last_error = (uint32_t)err;
      chprintf(chp, "ECDC: ECB 256 failed (err %u, byte %u)\r\n",
               (unsigned)err, (unsigned)diff);
    }
    else {
      chprintf(chp, "ECDC: ECB 256 ok\r\n");
    }

    /* The CTR mode, NIST SP 800-38A. */
    err = cryLoadAESTransientKey(&CRYD1, sizeof ctr_key128, ctr_key128);
    if (err == CRY_NOERROR) {
      err = cryEncryptAES_CTR(&CRYD1, ECDC_TEST_KEY_128, sizeof ctr_plain,
                              ctr_plain, out, ctr_counter);
    }
    cry_blocks++;
    diff = (err == CRY_NOERROR) ? ec_diff(out, ctr_cipher, sizeof out) : 0U;
    if ((err != CRY_NOERROR) || (diff != sizeof out)) {
      cry_failures++;
      cry_last_error = (uint32_t)err;
      chprintf(chp, "ECDC: CTR failed (err %u, byte %u)\r\n",
               (unsigned)err, (unsigned)diff);
    }
    else {
      chprintf(chp, "ECDC: CTR ok\r\n");
    }

    cry_rounds++;
    chThdSleepMilliseconds(ECDC_TEST_ROUND_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the ECDC test thread.
 */
void ecdc_test_start(void) {

  chThdCreateStatic(waEcdcTest, sizeof(waEcdcTest), NORMALPRIO, ecdc_test_thd,
                    NULL);
}
