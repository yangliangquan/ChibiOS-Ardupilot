ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_SERIAL TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/USARTv1/hal_serial_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_usart.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/USARTv1/hal_serial_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_usart.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/USARTv1
