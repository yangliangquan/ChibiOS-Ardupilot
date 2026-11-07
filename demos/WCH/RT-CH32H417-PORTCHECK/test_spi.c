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
 * @file    test_spi.c
 * @brief   CH32H417 V5F port check: SPI test thread.
 *
 * @details Exchanges frames with SPI4 in the master mode, the clock and
 *          the data lines are taken from a jumper that loops the
 *          output back to the input:
 *          - PE6 is the clock and PE5 is the master input, the jumper
 *            connects PE5 to the slave output,
 *          - the frame size is walked from 8 to 16 bits,
 *          - the four frame modes are walked, the clock polarity and the
 *            phase are changed on every round.
 *
 *          The unit is polled, the exchange of a frame is the
 *          transmission of the frame followed by the reception of the
 *          answer, the content of the answer is compared with the
 *          content of the frame.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Clock of the SPI unit. */
#define SPI_TEST_CLOCK_HZ     1000000U

/* Number of frames exchanged per mode and frame size. */
#define SPI_TEST_FRAMES       8U

/* Period of the measurement round. */
#define SPI_TEST_ROUND_MS     1000U

/* Frame sizes walked by the test. */
#define SPI_TEST_DATABITS_8   8U
#define SPI_TEST_DATABITS_16  16U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t spi_transfers;
volatile uint32_t spi_failures;
volatile uint32_t spi_rounds;
volatile uint32_t spi_last_error;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waSpiTest, 1024);

static THD_FUNCTION(spi_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  SPIConfig spi_config;
  uint8_t frame[2];
  uint8_t answer[2];
  uint32_t pattern;
  uint32_t mode;
  unsigned databits;
  unsigned i;
  unsigned bytes;
  uint32_t expected;
  uint32_t received;

  (void)arg;

  /* The clock of the unit is the output, the data input is the input
     of the master. */
  palSetPadMode(GPIOE, 6U, PAL_MODE_ALTERNATE(5));  /* SPI4_SCK.  */
  palSetPadMode(GPIOE, 5U, PAL_MODE_ALTERNATE(5));  /* SPI4_MISO. */

  spi_config.circular = FALSE;
  spi_config.data_cb = NULL;
  spi_config.error_cb = NULL;
  spi_config.clock_freq = SPI_TEST_CLOCK_HZ;
  spi_config.databits = SPI_TEST_DATABITS_8;
  spi_config.mode = 0U;
  spi_config.lsb_first = FALSE;

  if (spiStart(&SPID4, &spi_config) != MSG_OK) {
    chprintf(chp, "SPI: driver start failed\r\n");
    return;
  }

  chprintf(chp, "SPI: SPI4 master, clock %u Hz, PE6=SCK PE5=MISO\r\n",
           (unsigned)SPI_TEST_CLOCK_HZ);

  pattern = 0xA5U;
  while (true) {
    for (databits = SPI_TEST_DATABITS_8; databits <= SPI_TEST_DATABITS_16;
         databits += 8U) {
      bytes = (databits == SPI_TEST_DATABITS_8) ? 1U : 2U;

      for (mode = 0U; mode < 4U; mode++) {
        spi_config.mode = (uint8_t)mode;
        spi_config.databits = (uint8_t)databits;

        /* The configuration is taken by the driver at the start, the
           unit is stopped and started again with the new frame mode
           and the new frame size. */
        spiStop(&SPID4);
        if (spiStart(&SPID4, &spi_config) != MSG_OK) {
          spi_failures++;
          chprintf(chp, "SPI: restart failed\r\n");
          return;
        }

        expected = 0U;
        received = 0U;

        for (i = 0U; i < SPI_TEST_FRAMES; i++) {
          pattern = (pattern * 1103515245U) + 12345U;
          frame[0] = (uint8_t)pattern;
          frame[1] = (uint8_t)(pattern >> 8);
          answer[0] = 0U;
          answer[1] = 0U;

          /* The polled exchange returns the frame that was received
             while the frame was sent, the loopback returns the frame
             itself. */
          spiSelect(&SPID4);
          if (bytes == 1U) {
            answer[0] = (uint8_t)spiPolledExchange(&SPID4, frame[0]);
            expected = frame[0];
            received = answer[0];
          }
          else {
            uint16_t exchanged = spiPolledExchange(&SPID4,
                                                   (uint16_t)(frame[0] |
                                                              ((uint16_t)frame[1] << 8)));
            answer[0] = (uint8_t)exchanged;
            answer[1] = (uint8_t)(exchanged >> 8);
            expected = (uint32_t)frame[0] | ((uint32_t)frame[1] << 8);
            received = (uint32_t)answer[0] | ((uint32_t)answer[1] << 8);
          }
          spiUnselect(&SPID4);

          spi_transfers++;
        }

        spi_rounds++;
        spi_last_error = expected ^ received;

        if (spi_last_error != 0U) {
          spi_failures++;
          chprintf(chp, "SPI: mode %u, %u bits, mismatch %08x\r\n",
                   (unsigned)mode, (unsigned)databits,
                   (unsigned)spi_last_error);
          continue;
        }

        chprintf(chp, "SPI: mode %u, %u bits, %u frames, pattern %08x\r\n",
                 (unsigned)mode, (unsigned)databits,
                 (unsigned)SPI_TEST_FRAMES, (unsigned)expected);
      }
    }

    chThdSleepMilliseconds(SPI_TEST_ROUND_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the SPI test thread.
 */
void spi_test_start(void) {

  chThdCreateStatic(waSpiTest, sizeof(waSpiTest), NORMALPRIO, spi_test_thd,
                    NULL);
}
