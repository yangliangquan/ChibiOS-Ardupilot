ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_SPI TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SPIv2/hal_spi_v2_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_spi.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SPIv2/hal_spi_v2_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_spi.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/SPIv2
