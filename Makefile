########################################################################################################################
# Build constants
########################################################################################################################
USE_GCC		:= 0
USE_KASAN	:= 0
USE_PROF	:= 0
DEBUG		:= 0

ifeq ($(USE_GCC), 1)
CC 			:= gcc
LD 			:= gcc
else
CC 			:= ccache clang
LD			:= ld.lld
endif


OUT_DIR		:= out
BIN_DIR		:= $(OUT_DIR)/bin
BUILD_DIR	:= $(OUT_DIR)/build

#-----------------------------------------------------------------------------------------------------------------------
# General configurations
#-----------------------------------------------------------------------------------------------------------------------
CFLAGS 		:=
ifeq ($(USE_GCC), 0)
CFLAGS 		+= -target x86_64-pc-none-elf
endif
CFLAGS		+= -Werror -std=gnu11
CFLAGS 		+= -Wno-unused-function
CFLAGS 		+= -Wno-unused-label
CFLAGS 		+= -Wno-address-of-packed-member
CFLAGS 		+= -Wno-psabi

CFLAGS 		+= -D__SERIAL_TRACE__
CFLAGS 		+= -D__GRAPHICS_TRACE__

ifeq ($(DEBUG),1)
	CFLAGS	+= -O0 -g3
	CFLAGS 	+= -fstack-protector-all
#	CFLAGS 	+= -fno-sanitize=alignment
#	ifeq ($(USE_GCC), 0)
#		CFLAGS	+= -fsanitize=undefined
#	endif
else
	CFLAGS	+= -O3 -g
	CFLAGS 	+= -DNDEBUG
	ifeq ($(USE_GCC), 0)
		CFLAGS		+= -flto
	endif
endif
ifeq ($(USE_KASAN),1)
	CFLAGS  += -DKASAN -fsanitize=kernel-address -fasan-shadow-offset=0xdfffe00000000000
endif
# NOTE: requires GCC!
# clang doesn't support exclude-file-list
ifeq ($(USE_PROF),1)
	CFLAGS	+= -DPROF -finstrument-functions \
	-finstrument-functions-exclude-file-list=kernel/ \
	-finstrument-functions-exclude-file-list=lib/tinydotnet/lib/
endif

CFLAGS 		+= -mno-avx -mno-avx2 -fno-pie -fno-pic -Wno-error=unused-but-set-variable
CFLAGS		+= -ffreestanding -static -fshort-wchar
CFLAGS		+= -mcmodel=kernel -mno-red-zone
CFLAGS 		+= -nostdlib -nostdinc
CFLAGS 		+= -Ikernel -Ilib -Ilimine
CFLAGS		+= -Ikernel/libc
CFLAGS 		+= -isystem lib/libc
CFLAGS 		+= -fms-extensions -Wno-microsoft-anon-tag
CFLAGS 		+= -Ilib/tinydotnet/lib

SRCS 		:= $(shell find kernel -name '*.c')

LDFLAGS		+= -Tkernel/linker.ld -nostdlib -static -z max-page-size=0x1000 

# For the printf library
CFLAGS		+= -DPRINTF_NTOA_BUFFER_SIZE=64
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL

#-----------------------------------------------------------------------------------------------------------------------
# tinydotnet
#-----------------------------------------------------------------------------------------------------------------------

SRCS 		+= $(shell find lib/tinydotnet/src/dotnet -name '*.c')
CFLAGS 		+= -Ilib/tinydotnet/src

#-----------------------------------------------------------------------------------------------------------------------
# utf8-utf16-converter
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS		+= -Ilib/tinydotnet/lib/utf8-utf16-converter/converter/include

SRCS		+= lib/tinydotnet/lib/utf8-utf16-converter/converter/src/converter.c

#-----------------------------------------------------------------------------------------------------------------------
# Zydis
#-----------------------------------------------------------------------------------------------------------------------

SRCS 		+= $(shell find lib/zydis/dependencies/zycore/src -name '*.c')
CFLAGS		+= -Ilib/zydis/dependencies/zycore/include

SRCS 		+= $(shell find lib/zydis/src -name '*.c')
CFLAGS		+= -Ilib/zydis/include

CFLAGS 		+= -DZYAN_NO_LIBC
CFLAGS 		+= -DZYCORE_STATIC_BUILD
CFLAGS 		+= -DZYDIS_STATIC_BUILD

#-----------------------------------------------------------------------------------------------------------------------
# Mir library
#-----------------------------------------------------------------------------------------------------------------------

# Add MIR sources
SRCS 		+= lib/tinydotnet/lib/mir/mir.c
SRCS 		+= lib/tinydotnet/lib/mir/mir-gen.c

CFLAGS 		+= -DMIR_NO_SCAN
CFLAGS 		+= -DMIR_PARALLEL_GEN

########################################################################################################################
# Targets
########################################################################################################################

all: $(BIN_DIR)/tomatos.elf

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)
BINS ?=
-include $(DEPS)

$(BIN_DIR)/tomatos.elf: $(OBJS) | Makefile
	@echo LD $@
	@mkdir -p $(@D)
	@$(LD) $(LDFLAGS) -o $@ $^

scripts/profiler_gen: scripts/profiler_gen.c
	@$(CC) -Ikernel -O2 $< -o $@

# For zyndis we tell it we are posix just so it will be happy
$(BUILD_DIR)/lib/zydis/%.c.o: lib/zydis/%.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -D__posix -Ilib/zydis/src -MMD -c $< -o $@

$(BUILD_DIR)/lib/tinydotnet/lib/mir/%.c.o: lib/tinydotnet/lib/mir/%.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -Wall $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf out
