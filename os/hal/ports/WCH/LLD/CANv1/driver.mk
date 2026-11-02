ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_CAN TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/CANv1/hal_can_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_can.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/CANv1/hal_can_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_can.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/CANv1
