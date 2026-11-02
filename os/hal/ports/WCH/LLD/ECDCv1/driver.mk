ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_CRY TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ECDCv1/hal_crypto_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_ecdc.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ECDCv1/hal_crypto_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_ecdc.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/ECDCv1
