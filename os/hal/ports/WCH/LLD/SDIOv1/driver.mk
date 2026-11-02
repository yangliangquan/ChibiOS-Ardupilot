ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_SDC TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SDIOv1/hal_sdc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_sdio.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_sdmmc.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SDIOv1/hal_sdc_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_sdio.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_sdmmc.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SDIOv1
