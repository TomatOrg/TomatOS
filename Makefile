########################################################################################################################
# Build constants
########################################################################################################################

PREFIX		?=
CC 			:= ccache $(PREFIX)gcc
LD			:= $(PREFIX)gcc

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

CFLAGS 		+= -Os -flto -g3 -mtune=nehalem -march=nehalem -flto
CFLAGS		+= -ffreestanding -static -fshort-wchar
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
# Zydis
#-----------------------------------------------------------------------------------------------------------------------

SRCS 		+= $(shell find lib/zydis/dependencies/zycore/src -name '*.c')
CFLAGS		+= -Ilib/zydis/dependencies/zycore/include

SRCS 		+= $(shell find lib/zydis/src -name '*.c')
CFLAGS		+= -Ilib/zydis/include
CFLAGS		+= -Ilib/zydis/src

CFLAGS 		+= -DZYAN_NO_LIBC
CFLAGS 		+= -DZYCORE_STATIC_BUILD
CFLAGS 		+= -DZYDIS_STATIC_BUILD

#-----------------------------------------------------------------------------------------------------------------------
# Mir library
#-----------------------------------------------------------------------------------------------------------------------

## Add MIR sources
#SRCS 		+= lib/mir/mir.c
#SRCS 		+= lib/mir/mir-gen.c
#
#CFLAGS		+= -DMIR_NO_SCAN

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
