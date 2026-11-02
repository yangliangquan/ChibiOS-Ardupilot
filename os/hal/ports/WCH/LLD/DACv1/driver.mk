ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_DAC TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/DACv1/hal_dac_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_dac.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/DACv1/hal_dac_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_dac.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/DACv1
