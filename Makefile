########################################################################################################################
# Build constants
########################################################################################################################

# Should we compile with no-optimizations
DEBUG		?= 0

# Should GCC be used instead of clang
# needed for some debug utilities
USE_GCC		?= 0

# Should UBSAN be enabled
USE_UBSAN	?= 0

# Should KASAN be enabled
USE_KASAN	?= 0

# Should we enable the profiler
USE_PROF	?= 0

# Should LTO be enabled
USE_LTO		?= 1

OUT_DIR		:= out
BIN_DIR		:= $(OUT_DIR)/bin
BUILD_DIR	:= $(OUT_DIR)/build

#-----------------------------------------------------------------------------------------------------------------------
# General configurations
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS 		:=

# Choose a compiler
ifeq ($(USE_GCC),1)
CC 			:= gcc
LD 			:= ld.lld # FIXME: TODO: GNU ld is broken
CFLAGS 		+= -U __linux__ # undefine linux, otherwise mimalloc uses Linux syscalls
else
CC 			:= ccache clang
LD			:= ld.lld
endif

# For clang we need a target
ifeq ($(USE_GCC), 0)
CFLAGS 		+= -target x86_64-pc-none-elf
endif

CFLAGS		+= -Werror -std=gnu11
CFLAGS 		+= -Wno-unused-function
CFLAGS 		+= -Wno-unused-label
CFLAGS 		+= -Wno-address-of-packed-member
CFLAGS 		+= -Wno-psabi

# TODO: do we always want this? I think we do
CFLAGS 		+= -fno-omit-frame-pointer

CFLAGS 		+= -D__SERIAL_TRACE__
CFLAGS 		+= -D__GRAPHICS_TRACE__

# ------------------
# Set debug options
# ------------------
ifeq ($(DEBUG),1)
# No optimizations at all and full debug info
CFLAGS	+= -O0 -g

# Enable a full stack protector for debugging
CFLAGS 	+= -fstack-protector-all

# Mark that we are in debug
CFLAGS 	+= -D__DEBUG__
else
# full optimizations, but still emit debug info
CFLAGS	+= -O3 -g

# set no debugging, mostly used for the libraries we use
CFLAGS 	+= -DNDEBUG
endif

# ------------------
# Set UBSAN option
# ------------------
ifeq ($(USE_UBSAN),1)
CFLAGS		+= -fsanitize=undefined -fno-sanitize=alignment
endif

# ------------------
# Set LTO option
# ------------------
ifeq ($(USE_LTO),1)
CFLAGS		+= -flto
endif

# if we want to use kasan
ifeq ($(USE_KASAN),1)
ifeq ($(USE_GCC),0)
	# it inlines the checks so we can't disable them for the start of the kernel
	$(error "GCC is required to build with KASAN")
endif
	CFLAGS  += -D__KASAN__
	CFLAGS  += -fsanitize=kernel-address
	CFLAGS  += -fasan-shadow-offset=0xdfffe00000000000
endif

ifeq ($(USE_PROF),1)
ifeq ($(USE_GCC),0)
	# clang doesn't support exclude-file-list
	$(error "GCC is required to build with profiler")
endif
	CFLAGS	+= -D__PROF__
	CFLAGS 	+= -finstrument-functions
	CFLAGS 	+= -finstrument-functions-exclude-file-list=kernel/
	CFLAGS 	+= -finstrument-functions-exclude-file-list=lib/mimalloc/
	CFLAGS 	+= -finstrument-functions-exclude-file-list=lib/tinydotnet/lib/
endif

# Set the cflags
CFLAGS 		+= -mno-avx -mno-avx2 -fno-pie -fno-pic -Wno-error=unused-but-set-variable
CFLAGS		+= -ffreestanding -static -fshort-wchar
CFLAGS		+= -mcmodel=kernel -mno-red-zone
CFLAGS 		+= -nostdlib -nostdinc
CFLAGS 		+= -Ikernel -Ilib -Ilimine
CFLAGS		+= -Ikernel/libc
CFLAGS 		+= -isystem lib/libc
CFLAGS 		+= -fms-extensions -Wno-microsoft-anon-tag
CFLAGS 		+= -Ilib/tinydotnet/lib
CFLAGS 		+= -Ilib/inc

# Include all the sources
# TODO: for reproducible build we should sort this
SRCS 		:= $(shell find kernel -name '*.c')

# Set the linker flags
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
# mimalloc
#-----------------------------------------------------------------------------------------------------------------------
	
SRCS 		+= lib/mimalloc/src/bitmap.c
SRCS 		+= lib/mimalloc/src/arena.c
SRCS 		+= lib/mimalloc/src/segment.c
SRCS 		+= lib/mimalloc/src/page.c
SRCS 		+= lib/mimalloc/src/alloc.c
SRCS 		+= lib/mimalloc/src/alloc-aligned.c
SRCS 		+= lib/mimalloc/src/heap.c
SRCS 		+= lib/mimalloc/src/random.c
SRCS 		+= lib/mimalloc/src/region.c
SRCS 		+= lib/mimalloc/src/init.c
CFLAGS 		+= -DMADV_NORMAL
CFLAGS 		+= -Ilib/mimalloc/src
CFLAGS 		+= -Ilib/mimalloc/include
CFLAGS		+= -DMI_DEBUG=0 -DMI_SECURE=1

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
CFLAGS 		+= -DMIR_NO_RED_ZONE_ABI

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

# MIR has compilation warnings so we will just not have Wall
$(BUILD_DIR)/lib/tinydotnet/lib/mir/%.c.o: lib/tinydotnet/lib/mir/%.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

# For our main code we will have Wall
$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -Wall $(CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf out
