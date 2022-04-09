########################################################################################################################
# Build constants
########################################################################################################################

CC 			:= ccache clang
LD			:= ld.lld

OUT_DIR		:= out
BIN_DIR		:= $(OUT_DIR)/bin
BUILD_DIR	:= $(OUT_DIR)/build

#-----------------------------------------------------------------------------------------------------------------------
# General configurations
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS 		:= -target x86_64-pc-none-elf
CFLAGS		+= -Werror -std=gnu11
CFLAGS 		+= -Wno-unused-label
CFLAGS 		+= -Wno-address-of-packed-member
CFLAGS 		+= -Wno-psabi

#CFLAGS 		+= -Os -flto
CFLAGS 		+= -g3 -mtune=nehalem -march=nehalem -flto
CFLAGS		+= -ffreestanding -static -fshort-wchar
CFLAGS		+= -mcmodel=kernel -mno-red-zone
CFLAGS 		+= -nostdlib -nostdinc
CFLAGS 		+= -Ikernel -Ilib
CFLAGS 		+= -isystem lib/libc
CFLAGS 		+= -fms-extensions -Wno-microsoft-anon-tag

SRCS 		:= $(shell find kernel -name '*.c')

LDFLAGS		+= -Tkernel/linker.ld

# For the printf library
CFLAGS		+= -DPRINTF_NTOA_BUFFER_SIZE=64
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_FLOAT
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL

#-----------------------------------------------------------------------------------------------------------------------
# utf8-utf16-converter
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS		+= -Ilib/utf8-utf16-converter/converter/include

SRCS		+= lib/utf8-utf16-converter/converter/src/converter.c

#-----------------------------------------------------------------------------------------------------------------------
# Zydis
#-----------------------------------------------------------------------------------------------------------------------

#SRCS 		+= $(shell find lib/zydis/dependencies/zycore/src -name '*.c')
#CFLAGS		+= -Ilib/zydis/dependencies/zycore/include
#
#SRCS 		+= $(shell find lib/zydis/src -name '*.c')
#CFLAGS		+= -Ilib/zydis/include
#CFLAGS		+= -Ilib/zydis/src
#
#CFLAGS 		+= -DZYAN_NO_LIBC
#CFLAGS 		+= -DZYCORE_STATIC_BUILD
#CFLAGS 		+= -DZYDIS_STATIC_BUILD

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
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf out
