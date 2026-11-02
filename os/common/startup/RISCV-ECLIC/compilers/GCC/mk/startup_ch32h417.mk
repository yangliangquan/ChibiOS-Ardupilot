# List of the ChibiOS CH32H417 (QingKe V5F) startup files.
STARTUPSRC = $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/crt1.c

STARTUPASM = $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/crt0.S \
             $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/vectors.S

STARTUPINC = $(CHIBIOS)/os/common/portability/GCC \
             $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC \
             $(CHIBIOS)/os/common/startup/RISCV-ECLIC/devices/CH32H417 \
             $(CHIBIOS)/os/common/ext/WCH/CH32H417

STARTUPLD  = $(CHIBIOS)/os/common/startup/RISCV-ECLIC/compilers/GCC/ld

# Shared variables
ALLXASMSRC += $(STARTUPASM)
ALLCSRC    += $(STARTUPSRC)
ALLINC     += $(STARTUPINC)
