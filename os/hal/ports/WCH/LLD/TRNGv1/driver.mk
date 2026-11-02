ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_TRNG TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TRNGv1/hal_trng_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rng.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TRNGv1/hal_trng_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rng.c
endif

PLATFORMINC += $(CHIBIOS)/os/hal/ports/WCH/LLD/TRNGv1
