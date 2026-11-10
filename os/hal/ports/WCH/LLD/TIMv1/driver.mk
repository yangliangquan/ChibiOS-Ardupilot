ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_GPT TRUE,$(HALCONF))$(findstring HAL_USE_ICU TRUE,$(HALCONF))$(findstring HAL_USE_PWM TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_gpt_lld.c \
               $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_icu_lld.c \
               $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_pwm_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_tim.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_gpt_lld.c \
               $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_icu_lld.c \
               $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/hal_pwm_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_tim.c
endif


PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1
