##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -Os -g2 -msave-restore
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT =
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
endif

# Enable this if you want the linker to remove unused code and data.
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT =
endif

# Enable this if you want link time optimizations (LTO).
ifeq ($(USE_LTO),)
  USE_LTO = no
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

# If enabled, this option makes the build process faster by not compiling
# modules not used in the current configuration.
ifeq ($(USE_SMART_BUILD),)
  USE_SMART_BUILD = yes
endif

#
# Build global options
##############################################################################

##############################################################################
# Project, target, sources and paths
#

# Define project name here
PROJECT = v3fboot

# Target settings.
MCU_ARCH = rv32imac
MCU_ABI = ilp32
MCU_CMODEL = medlow

# Imported source files and paths.
CHIBIOS := ../../..
BUILDDIR := ./build
DEPDIR   := ./.dep

# Sources.
CSRC = main.c \
       $(CHIBIOS)/os/common/ext/WCH/CH32H417/v3f/system_ch32h417_v3f.c \
       $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_gpio.c \
       $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_rcc.c \
       $(CHIBIOS)/os/common/ext/WCH/CH32H417/ch32h417_flash.c

ASMXSRC = startup_ch32h417_v3f.S

# Inclusion directories.
INCDIR = $(CHIBIOS)/os/common/ext/WCH/CH32H417/v3f \
         $(CHIBIOS)/os/common/ext/WCH/CH32H417

# Define C warning options here.
CWARN = -Wall -Wextra -Wundef -Wstrict-prototypes

# Define C++ warning options here.
CPPWARN = -Wall -Wextra -Wundef

# Linker script: local copy (WCH EVT V3F layout: image at 0x00000000,
# 64K). STARTUPLD still points at the shared ld directory because the
# common rules pass it as --library-path.
STARTUPLD := $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/ld
LDSCRIPT= CH32H417_V3F_FLASH.ld

#
# Project, target, sources and paths
##############################################################################

##############################################################################
# Start of user section
#

UDEFS =
UADEFS =
UINCDIR =
ULIBDIR =
ULIBS =

#
# End of user section
##############################################################################

##############################################################################
# Common rules
#

RULESPATH = $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/mk
include $(RULESPATH)/riscv-wch-elf.mk
include $(RULESPATH)/rules.mk

#
# Common rules
##############################################################################

##############################################################################
# Custom rules
#

#
# Custom rules
###############################################################################
