########################################################################################################################
# Build constants
########################################################################################################################

CC 			:= gcc
LD			:= gcc

OUT_DIR		:= out
BIN_DIR		:= $(OUT_DIR)/bin
BUILD_DIR	:= $(OUT_DIR)/build

#-----------------------------------------------------------------------------------------------------------------------
# General configurations
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS		:= -Werror -std=gnu11
CFLAGS 		+= -Wno-unused-label
CFLAGS 		+= -Wno-address-of-packed-member
CFLAGS 		+= -Wno-psabi

CFLAGS 		+= -O1 -g
CFLAGS		+= -ffreestanding -flto -static -fshort-wchar
CFLAGS		+= -mno-red-zone -nostdlib
CFLAGS		+= -lgcc
CFLAGS		+= -Tkernel/linker.ld
CFLAGS 		+= -Ikernel -Ilib

SRCS 		:= $(shell find kernel -name '*.c')

# For the printf library
CFLAGS		+= -DPRINTF_NTOA_BUFFER_SIZE=64
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_FLOAT
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL

#-----------------------------------------------------------------------------------------------------------------------
# Mir library
#-----------------------------------------------------------------------------------------------------------------------

# Add MIR sources
SRCS 		+= lib/mir/mir.c
SRCS 		+= lib/mir/mir-gen.c

CFLAGS		+= -DMIR_NO_SCAN

########################################################################################################################
# Targets
########################################################################################################################

all: $(BIN_DIR)/pentagon.elf

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)
BINS ?=
-include $(DEPS)

$(BIN_DIR)/pentagon.elf: $(BINS) $(OBJS)
	@echo LD $@
	@mkdir -p $(@D)
	@$(LD) $(CFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf out
