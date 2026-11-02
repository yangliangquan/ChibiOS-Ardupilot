ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_WSPI TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/QSPIv1/hal_wspi_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_qspi.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/QSPIv1/hal_wspi_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_qspi.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/QSPIv1
