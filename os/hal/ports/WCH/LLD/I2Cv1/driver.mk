ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_I2C TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Cv1/hal_i2c_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_i2c.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Cv1/hal_i2c_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_i2c.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Cv1
