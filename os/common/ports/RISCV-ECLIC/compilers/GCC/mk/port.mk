# List of the ChibiOS/RT RISCV-ECLIC generic port files.
PORTSRC = $(CHIBIOS)/os/common/ports/RISCV-ECLIC/chcore.c

PORTASM = $(CHIBIOS)/os/common/ports/RISCV-ECLIC/compilers/GCC/chcoreasm.S

PORTINC = $(CHIBIOS)/os/common/ports/RISCV-ECLIC \
          $(CHIBIOS)/os/common/ports/RISCV-ECLIC/compilers/GCC

# Shared variables
ALLXASMSRC += $(PORTASM)
ALLCSRC    += $(PORTSRC)
ALLINC     += $(PORTINC)
