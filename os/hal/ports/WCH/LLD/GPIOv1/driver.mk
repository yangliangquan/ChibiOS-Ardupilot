ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_PAL TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/GPIOv1/hal_pal_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_gpio.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/GPIOv1/hal_pal_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_gpio.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/GPIOv1
