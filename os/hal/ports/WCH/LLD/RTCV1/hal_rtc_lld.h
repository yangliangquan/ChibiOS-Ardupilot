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
 * @file    RTCv1/hal_rtc_lld.h
 * @brief   CH32H417 RTC subsystem low level driver header.
 *
 * @details F1-compatible counter RTC clocked from the internal 40kHz
 *          LSI (no external 32.768kHz crystal required).
 *
 * @addtogroup RTC
 * @{
 */

#ifndef HAL_RTC_LLD_H
#define HAL_RTC_LLD_H

#if (HAL_USE_RTC == TRUE) || defined(__DOXYGEN__)

/*===========================================================================*/
/* Driver constants.                                                         */
/*===========================================================================*/

/**
 * @name    Implementation capabilities
 */
/**
 * @brief   This RTC implementation supports callbacks.
 */
#define RTC_SUPPORTS_CALLBACKS      TRUE

/**
 * @brief   One alarm comparator available.
 */
#define RTC_ALARMS                  1

/**
 * @brief   Presence of a local persistent storage.
 */
#define RTC_HAS_STORAGE             FALSE
/** @} */

/*===========================================================================*/
/* Driver pre-compile time settings.                                         */
/*===========================================================================*/

/**
 * @name    Configuration options
 * @{
 */
/**
 * @brief   RTC1 driver enable switch.
 */
#if !defined(CH32H417_RTC_USE_RTC1) || defined(__DOXYGEN__)
#define CH32H417_RTC_USE_RTC1               FALSE
#endif

/**
 * @brief   RTC clock source frequency in Hz (the internal LSI).
 * @note    Datasheet: 25..60kHz, 40kHz typical. The time keeping
 *          accuracy follows the LSI tolerance, an external 32.768kHz
 *          LSE crystal can be selected by overriding this value and
 *          the source selection.
 */
#if !defined(CH32H417_RTCCLK) || defined(__DOXYGEN__)
#define CH32H417_RTCCLK                     40000U
#endif
/** @} */

/*===========================================================================*/
/* Derived constants and error checks.                                       */
/*===========================================================================*/

#if !CH32H417_RTC_USE_RTC1
#error "RTC driver activated but no RTC unit assigned"
#endif

/*===========================================================================*/
/* Driver data structures and types.                                         */
/*===========================================================================*/

/**
 * @brief   Type of an RTC event.
 */
typedef enum {
  RTC_EVENT_SECOND = 0,                 /**< Triggered every second.         */
  RTC_EVENT_ALARM = 1,                  /**< Triggered on alarm.             */
  RTC_EVENT_OVERFLOW = 2                /**< Triggered on counter overflow.  */
} rtcevent_t;

/**
 * @brief   Type of a generic RTC callback.
 */
typedef void (*rtccb_t)(RTCDriver *rtcp, rtcevent_t event);

/**
 * @brief   Type of a structure representing an RTC alarm time stamp.
 */
typedef struct hal_rtc_alarm {
  /**
   * @brief Seconds since UNIX epoch.
   */
  uint32_t          tv_sec;
} RTCAlarm;

/**
 * @brief   Implementation-specific @p RTCDriver fields.
 */
#define rtc_lld_driver_fields                                               \
  /* Pointer to the RTC registers block.*/                                  \
  RTC_TypeDef               *rtc;                                           \
  /* Callback pointer.*/                                                    \
  rtccb_t                   callback;

/*===========================================================================*/
/* External declarations.                                                    */
/*===========================================================================*/

#if (CH32H417_RTC_USE_RTC1 == TRUE) && !defined(__DOXYGEN__)
extern RTCDriver RTCD1;
#endif

#ifdef __cplusplus
extern "C" {
#endif
  void rtc_lld_init(void);
  void rtc_lld_set_time(RTCDriver *rtcp, const RTCDateTime *timespec);
  void rtc_lld_get_time(RTCDriver *rtcp, RTCDateTime *timespec);
  void rtc_lld_set_alarm(RTCDriver *rtcp,
                         rtcalarm_t alarm_number,
                         const RTCAlarm *alarmspec);
  void rtc_lld_get_alarm(RTCDriver *rtcp,
                         rtcalarm_t alarm_number,
                         RTCAlarm *alarmspec);
  void rtc_lld_set_callback(RTCDriver *rtcp, rtccb_t callback);
#ifdef __cplusplus
}
#endif

#endif /* HAL_USE_RTC == TRUE */

#endif /* HAL_RTC_LLD_H */

/** @} */
