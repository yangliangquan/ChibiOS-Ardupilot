ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_WDG TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/IWDGv1/hal_wdg_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_iwdg.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_wwdg.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/IWDGv1/hal_wdg_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_iwdg.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_wwdg.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/IWDGv1
