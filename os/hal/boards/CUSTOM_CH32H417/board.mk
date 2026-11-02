# List of all the board related files.
BOARDSRC = $(CHIBIOS)/os/hal/boards/CUSTOM_CH32H417/board.c

# Required include directories.
BOARDINC = $(CHIBIOS)/os/hal/boards/CUSTOM_CH32H417

# Shared variables
ALLCSRC += $(BOARDSRC)
ALLINC  += $(BOARDINC)
