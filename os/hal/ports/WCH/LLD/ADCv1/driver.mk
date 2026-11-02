ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_ADC TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ADCv1/hal_adc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_adc.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ADCv1/hal_adc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_adc.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ADCv1
