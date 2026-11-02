# Required platform files.
PLATFORMSRC := $(CHIBIOS)/os/hal/ports/WCH/CH32H417/hal_lld.c \
               $(CHIBIOS)/os/hal/ports/WCH/CH32H417/hal_st_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rcc.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/system_ch32h417.c

# Embedded flash driver (smart build gated).
ifeq ($(USE_SMART_BUILD),yes)
ifneq ($(findstring HAL_USE_EFL TRUE,$(HALCONF)),)
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/CH32H417/hal_efl_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_flash.c
endif
else
PLATFORMSRC += $(CHIBIOS)/os/hal/ports/WCH/CH32H417/hal_efl_lld.c \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_flash.c
endif

# Required include directories.
PLATFORMINC := $(CHIBIOS)/os/hal/ports/WCH/CH32H417 \
               $(CHIBIOS)/os/common/ext/WCH/CH32H417 \
               $(CHIBIOS)/os/common/startup/RISCV-ECLIC/devices/CH32H417

# Drivers compatible with the platform.
include $(CHIBIOS)/os/hal/ports/WCH/LLD/GPIOv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/USARTv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/IWDGv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/TIMv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/SPIv2/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Cv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/ADCv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/RTCV1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/CANv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/DACv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/TRNGv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/SDIOv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/QSPIv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/I2Sv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/ECDCv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/MACv1/driver.mk
include $(CHIBIOS)/os/hal/ports/WCH/LLD/USBFSv1/driver.mk

# Shared variables
ALLCSRC += $(PLATFORMSRC)
ALLINC  += $(PLATFORMINC)
