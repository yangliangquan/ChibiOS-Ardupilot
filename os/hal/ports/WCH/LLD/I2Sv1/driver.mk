ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_I2S TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Sv1/hal_i2s_lld.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Sv1/hal_i2s_lld.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Sv1
