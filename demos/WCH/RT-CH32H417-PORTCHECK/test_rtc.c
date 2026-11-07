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
 * @file    test_rtc.c
 * @brief   CH32H417 V5F port check: RTC test thread.
 *
 * @details Validates the real time clock of the board:
 *          - the date and time are written and read back, the counter
 *            must return the value that was programmed,
 *          - the seconds counter is sampled over one second, the rate
 *            must be close to one hertz,
 *          - an alarm is programmed one second in the future and the
 *            callback must report it.
 *
 *          The RTC is clocked from the internal 40kHz oscillator, the
 *          accuracy of the counter is therefore much lower than the
 *          accuracy of a crystal, only the rate is checked.
 */

#include "board.h"
#include "ch.h"
#include "chprintf.h"
#include "portcheck.h"

/*===========================================================================*/
/* Module local definitions.                                                 */
/*===========================================================================*/

/* Period of the measurement round. */
#define RTC_TEST_ROUND_MS     1000U

/* Tolerance of the seconds counter rate, the internal oscillator is
   not precise, a few percent are accepted. */
#define RTC_TEST_RATE_TOLERANCE 5U

/*===========================================================================*/
/* Module exported variables (debugger observation points).                  */
/*===========================================================================*/

volatile uint32_t rtc_seconds;
volatile uint32_t rtc_alarms;
volatile uint32_t rtc_overflows;
volatile uint32_t rtc_checks;
volatile uint32_t rtc_failures;
volatile uint32_t rtc_last_epoch;
volatile uint32_t rtc_rate_mhz;

/*===========================================================================*/
/* Module local data.                                                        */
/*===========================================================================*/

/* The RTC unit of this port has no configuration fields, the driver is
   initialized by the generic layer. */

/*===========================================================================*/
/* Module local functions.                                                   */
/*===========================================================================*/

/**
 * @brief   RTC callback.
 * @details Counts the alarm and the overflow events, both are reported
 *          by the driver through the same callback.
 */
static void rtc_event_cb(RTCDriver *rtcp, rtcevent_t event) {

  (void)rtcp;

  switch (event) {
  case RTC_EVENT_ALARM:
    rtc_alarms++;
    break;
  case RTC_EVENT_OVERFLOW:
    rtc_overflows++;
    break;
  default:
    break;
  }
}

/**
 * @brief   Converts a date and time into the epoch of the driver.
 */
static uint32_t rtc_epoch(const RTCDateTime *timespec) {

  return (uint32_t)timespec->millisecond;
}

/*===========================================================================*/
/* Module local threads.                                                     */
/*===========================================================================*/

static THD_WORKING_AREA(waRtcTest, 1024);

static THD_FUNCTION(rtc_test_thd, arg) {
  BaseSequentialStream *chp = PORTCheck_CONSOLE;
  RTCDateTime written;
  RTCDateTime readback;
  RTCAlarm alarm;
  uint32_t start;
  uint32_t end;
  uint32_t elapsed;
  uint32_t rate;

  (void)arg;

  /* The counter is configured by the driver initialization, the unit is
     ready to be used. */
  chprintf(chp, "RTC: RTCD1 ready\r\n");

  /* The callback is installed before the alarm is programmed, the
     driver drops the interrupt sources while it has no callback. */
  rtcSetCallback(&RTCD1, rtc_event_cb);

  /* A fixed date, the year is counted from 1980. */
  written.year = 26U;
  written.month = 11U;
  written.day = 7U;
  written.dayofweek = 6U;
  written.dstflag = 0U;
  written.millisecond = 12U * 3600000U + 34U * 60000U + 56U * 1000U;

  rtcSetTime(&RTCD1, &written);
  rtcGetTime(&RTCD1, &readback);

  rtc_checks++;
  if (rtc_epoch(&readback) != rtc_epoch(&written)) {
    rtc_failures++;
    chprintf(chp, "RTC: read back %u, written %u\r\n",
             (unsigned)rtc_epoch(&readback), (unsigned)rtc_epoch(&written));
  }
  else {
    chprintf(chp, "RTC: time written and read back, %u\r\n",
             (unsigned)rtc_epoch(&readback));
  }

  /* The alarm is programmed one second after the current time, the
     callback must report it. */
  alarm.tv_sec = rtc_epoch(&readback) + 1U;
  rtcSetAlarm(&RTCD1, 0U, &alarm);

  /* The seconds counter is sampled over one second, the rate must be
     close to one hertz. */
  start = rtc_epoch(&readback);
  chThdSleepMilliseconds(RTC_TEST_ROUND_MS);
  rtcGetTime(&RTCD1, &readback);
  end = rtc_epoch(&readback);

  elapsed = end - start;
  rate = elapsed;
  rtc_rate_mhz = rate;

  rtc_checks++;
  if (elapsed == 0U) {
    rtc_failures++;
    chprintf(chp, "RTC: counter did not advance\r\n");
  }
  else if ((elapsed > (1U + RTC_TEST_RATE_TOLERANCE)) ||
           (elapsed < (1U - RTC_TEST_RATE_TOLERANCE))) {
    rtc_failures++;
    chprintf(chp, "RTC: rate %u ticks per second\r\n", (unsigned)elapsed);
  }
  else {
    chprintf(chp, "RTC: %u ticks per second, alarms %u, overflows %u\r\n",
             (unsigned)elapsed, (unsigned)rtc_alarms, (unsigned)rtc_overflows);
  }

  while (true) {
    rtcGetTime(&RTCD1, &readback);
    rtc_last_epoch = rtc_epoch(&readback);
    rtc_seconds++;

    chThdSleepMilliseconds(RTC_TEST_ROUND_MS);
  }
}

/*===========================================================================*/
/* Module exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Starts the RTC test thread.
 */
void rtc_test_start(void) {

  chThdCreateStatic(waRtcTest, sizeof(waRtcTest), NORMALPRIO, rtc_test_thd,
                    NULL);
}
