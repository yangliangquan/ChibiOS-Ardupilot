/*
    ChibiOS - Copyright (C) 2026 ChibiOS contributors.
    Concepts and parts of this file have been contributed by
    Uladzimir Pylinsky aka barthess.

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
 * @file    RTCv1/hal_rtc_lld.c
 * @brief   CH32H417 RTC subsystem low level driver source.
 *
 * @addtogroup RTC
 * @{
 */

#include "hal.h"

#if (HAL_USE_RTC == TRUE) || defined(__DOXYGEN__)

#include <time.h>
#include "ch32h417_rtc.h"

/*===========================================================================*/
/* Driver exported variables.                                                */
/*===========================================================================*/

#if (CH32H417_RTC_USE_RTC1 == TRUE) && !defined(__DOXYGEN__)
/**
 * @brief RTC driver identifier.
 */
RTCDriver RTCD1;
#endif

/*===========================================================================*/
/* Driver local functions.                                                   */
/*===========================================================================*/

#if (CH32H417_RTC_USE_RTC1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   Bounded poll limit (~0.3s at HCLK=400MHz).
 * @note    All RTC flag waits are bounded: a missing RTC clock must
 *          never wedge the boot or an ISR (the vendor helpers loop
 *          forever on a dead clock).
 */
#define RTC_SPIN_LIMIT       ((uint32_t)40000000U)

/**
 * @brief   Waits (bounded) for the RTC registers to be synchronized.
 *
 * @return              false on timeout.
 */
static bool rtc_bus_sync(void) {
  uint32_t spin = RTC_SPIN_LIMIT;

  while ((RTC_GetFlagStatus(RTC_FLAG_RSF) == RESET) && (spin > 0U)) {
    spin--;
  }
  return (RTC_GetFlagStatus(RTC_FLAG_RSF) != RESET);
}

/**
 * @brief   Waits (bounded) until the last RTC write finished.
 */
static void rtc_wait_last_task(void) {
  uint32_t spin = RTC_SPIN_LIMIT;

  while (((RTC->CTLRL & RTC_FLAG_RTOFF) == (uint16_t)RESET) &&
         (spin > 0U)) {
    spin--;
  }
}

/**
 * @brief   Converts time from timespec to seconds counter.
 */
static time_t rtc_encode(const RTCDateTime *timespec) {
  struct tm tim;

  rtcConvertDateTimeToStructTm(timespec, &tim, NULL);
  return mktime(&tim);
}

/**
 * @brief   Converts time from seconds/milliseconds to timespec.
 */
static void rtc_decode(uint32_t tv_sec,
                       uint32_t tv_msec,
                       RTCDateTime *timespec) {
  struct tm tim;
  struct tm *t;
  const time_t time = (const time_t)tv_sec;

  t = localtime_r(&time, &tim);
  osalDbgAssert(t != NULL, "conversion failed");
  (void)t;

  rtcConvertStructTmToDateTime(&tim, tv_msec, timespec);
}

/**
 * @brief   Reads a stable seconds and the fractional value.
 */
static void rtc_get_sec_msec(uint32_t *tv_sec, uint32_t *tv_msec) {
  uint32_t frac;

  (void)rtc_bus_sync();

  /* Stable seconds read. */
  do {
    *tv_sec = RTC_GetCounter();
  } while (*tv_sec != RTC_GetCounter());

  /* Stable divider read, the divider counts downwards. */
  do {
    frac = ((uint32_t)RTC->DIVH << 16) | RTC->DIVL;
  } while (frac != (((uint32_t)RTC->DIVH << 16) | RTC->DIVL));

  if (tv_msec != NULL) {
    *tv_msec = ((CH32H417_RTCCLK - 1U - frac) * 1000U) / CH32H417_RTCCLK;
  }
}
#endif

/*===========================================================================*/
/* Driver interrupt handlers.                                                */
/*===========================================================================*/

#if (CH32H417_RTC_USE_RTC1 == TRUE) || defined(__DOXYGEN__)
/**
 * @brief   RTC interrupt handler.
 *
 * @isr
 */
OSAL_IRQ_HANDLER(RTC_IRQHandler) {

  OSAL_IRQ_PROLOGUE();

  (void)rtc_bus_sync();

  if (RTC_GetITStatus(RTC_IT_SEC) != RESET) {
    RTC_ClearITPendingBit(RTC_IT_SEC);
    if (RTCD1.callback != NULL) {
      RTCD1.callback(&RTCD1, RTC_EVENT_SECOND);
    }
  }
  if (RTC_GetITStatus(RTC_IT_ALR) != RESET) {
    RTC_ClearITPendingBit(RTC_IT_ALR);
    if (RTCD1.callback != NULL) {
      RTCD1.callback(&RTCD1, RTC_EVENT_ALARM);
    }
  }
  if (RTC_GetITStatus(RTC_IT_OW) != RESET) {
    RTC_ClearITPendingBit(RTC_IT_OW);
    if (RTCD1.callback != NULL) {
      RTCD1.callback(&RTCD1, RTC_EVENT_OVERFLOW);
    }
  }

  OSAL_IRQ_EPILOGUE();
}
#endif

/*===========================================================================*/
/* Driver exported functions.                                                */
/*===========================================================================*/

/**
 * @brief   Initialize RTC: LSI clock, prescaler, interrupt vector.
 *
 * @notapi
 */
void rtc_lld_init(void) {

#if (CH32H417_RTC_USE_RTC1 == TRUE) || defined(__DOXYGEN__)
  /* RTC object initialization.*/
  rtcObjectInit(&RTCD1);

  /* RTC pointer initialization.*/
  RTCD1.rtc = RTC;

  /* Unlock the backup domain first: RCC_BDCTLR's RTCSEL/RTCEN are
     write protected after reset until PWR_CTLR_DBP is set (RM note
     below the BDCTLR description). Without this the clock selection
     below is silently dropped and the RSF wait never completes -
     observed as a hang inside halInit() on hardware. */
  RCC_HB1PeriphClockCmd(RCC_HB1Periph_PWR, ENABLE);
  PWR->CTLR |= PWR_CTLR_DBP;

  /* LSI based RTC clock, no external crystal required. */
  RCC_LSICmd(ENABLE);
  RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
  RCC_RTCCLKCmd(ENABLE);

  /* Clear RSF and wait (bounded) for the RTC domain to synchronize;
     RSF only sets once the RTC clock is actually running. */
  RTC->CTLRL &= (uint16_t)~RTC_FLAG_RSF;
  if (!rtc_bus_sync()) {
    osalDbgAssert(false, "RTC clock not running (BDCTLR/LSI)");
  }

  /* One second time base (the prescaler is reloaded on every boot,
     time keeping does not persist across resets on this platform). */
  rtc_wait_last_task();
  RTC_EnterConfigMode();
  RTC_SetPrescaler(CH32H417_RTCCLK - 1U);
  RTC_ExitConfigMode();

  /* All interrupts initially disabled.*/
  RTC_ITConfig(RTC_IT_OW | RTC_IT_ALR | RTC_IT_SEC, DISABLE);

  /* Callback initially disabled.*/
  RTCD1.callback = NULL;

  /* IRQ vector permanently assigned to this driver.*/
  NVIC_EnableIRQ(RTC_IRQn);
#endif
}

/**
 * @brief   Set current time.
 * @note    Fractional part will be silently ignored.
 *
 * @notapi
 */
void rtc_lld_set_time(RTCDriver *rtcp, const RTCDateTime *timespec) {
  time_t tv_sec = rtc_encode(timespec);

  (void)rtcp;
  RTC_WaitForLastTask();
  RTC_EnterConfigMode();
  RTC_SetCounter((uint32_t)tv_sec);
  RTC_ExitConfigMode();
}

/**
 * @brief   Get current time.
 *
 * @notapi
 */
void rtc_lld_get_time(RTCDriver *rtcp, RTCDateTime *timespec) {
  uint32_t tv_sec, tv_msec;

  (void)rtcp;
  rtc_get_sec_msec(&tv_sec, &tv_msec);
  rtc_decode(tv_sec, tv_msec, timespec);
}

/**
 * @brief   Set alarm time.
 *
 * @notapi
 */
void rtc_lld_set_alarm(RTCDriver *rtcp,
                       rtcalarm_t alarm_number,
                       const RTCAlarm *alarmspec) {
  (void)rtcp;
  (void)alarm_number;

  RTC_WaitForLastTask();
  RTC_EnterConfigMode();
  if (alarmspec != NULL) {
    RTC_SetAlarm(alarmspec->tv_sec);
  }
  else {
    RTC_SetAlarm(0U);
  }
  RTC_ExitConfigMode();
}

/**
 * @brief   Get current alarm.
 *
 * @notapi
 */
void rtc_lld_get_alarm(RTCDriver *rtcp,
                       rtcalarm_t alarm_number,
                       RTCAlarm *alarmspec) {
  (void)rtcp;
  (void)alarm_number;

  (void)rtc_bus_sync();
  alarmspec->tv_sec = ((uint32_t)RTC->ALRMH << 16) | RTC->ALRML;
}

/**
 * @brief   Enables or disables RTC callbacks.
 *
 * @notapi
 */
void rtc_lld_set_callback(RTCDriver *rtcp, rtccb_t callback) {

  if (callback != NULL) {
    /* IRQ sources enabled only after setting up the callback.*/
    rtcp->callback = callback;

    RTC_ClearFlag(RTC_FLAG_OW);
    RTC_ClearFlag(RTC_FLAG_ALR);
    RTC_ClearFlag(RTC_FLAG_SEC);
    RTC_ITConfig(RTC_IT_OW | RTC_IT_ALR | RTC_IT_SEC, ENABLE);
  }
  else {
    RTC_ITConfig(RTC_IT_OW | RTC_IT_ALR | RTC_IT_SEC, DISABLE);

    /* Callback set to NULL only after disabling the IRQ sources.*/
    rtcp->callback = NULL;
  }
}

#endif /* HAL_USE_RTC == TRUE */

/** @} */
