ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_RTC TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/RTCV1/hal_rtc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rtc.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/RTCV1/hal_rtc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rtc.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/RTCV1
