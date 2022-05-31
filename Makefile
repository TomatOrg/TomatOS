########################################################################################################################
# User-modifiable build constants
########################################################################################################################

CC 					:= ccache clang
LD					:= ld.lld
HOSTED_CC			:= ccache clang -fuse-ld=lld

DEBUG				:= 1
OUT_DIR				:= out

BIN_DIR				:= $(OUT_DIR)/bin
BUILD_DIR			:= $(OUT_DIR)/build/kernel
HOSTED_BUILD_DIR	:= $(OUT_DIR)/build/hosted

#-----------------------------------------------------------------------------------------------------------------------
# General configurations
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS		:= -Werror -std=gnu11
CFLAGS 		+= -Wno-unused-label
CFLAGS 		+= -Wno-address-of-packed-member
CFLAGS 		+= -Wno-psabi

ifeq ($(DEBUG),1)
	CFLAGS	+= -O0 -g
	CFLAGS	+= -fsanitize=undefined
	CFLAGS 	+= -fno-sanitize=alignment
else
	CFLAGS	+= -O3 -g0 -flto
	CFLAGS 	+= -DNDEBUG
endif

CFLAGS		+= -fshort-wchar
CFLAGS 		+= -Ikernel -Ilib
CFLAGS 		+= -fms-extensions -Wno-microsoft-anon-tag
CFLAGS		+= -Wno-unused-command-line-argument

# General dependencies
# printf
CFLAGS		+= -DPRINTF_NTOA_BUFFER_SIZE=64
CFLAGS		+= -DPRINTF_DISABLE_SUPPORT_EXPONENTIAL
# unicode converter
CFLAGS		+= -Ilib/utf8-utf16-converter/converter/include
SRCS		+= lib/utf8-utf16-converter/converter/src/converter.c
# the MIR JIT compiler
SRCS 		+= lib/mir/mir.c
SRCS 		+= lib/mir/mir-gen.c
CFLAGS 		+= -DMIR_NO_SCAN
CFLAGS 		+= -DMIR_PARALLEL_GEN

#-----------------------------------------------------------------------------------------------------------------------
# Hosted-only build parameters
#-----------------------------------------------------------------------------------------------------------------------
HOSTED_CFLAGS	:= $(CFLAGS)
HOSTED_CFLAGS 	+= -DPENTAGON_HOSTED -DPENTAGON_DUMP_ASSEMBLIES
HOSTED_SRCS 	:= $(SRCS)
HOSTED_SRCS 	+= $(shell find kernel/runtime -type f \( -iname "*.c" ! -iname "heap.c" ! -iname "gc.c" \))
HOSTED_SRCS		+= kernel/util/printf.c
HOSTED_SRCS		+= kernel/util/stb_ds.c
HOSTED_SRCS		+= kernel/util/strbuilder.c
HOSTED_SRCS		+= kernel/hosted/main.c

#-----------------------------------------------------------------------------------------------------------------------
# Kernel-only build parameters
#-----------------------------------------------------------------------------------------------------------------------
CFLAGS 		+= -target x86_64-pc-none-elf
CFLAGS 		+= -isystem lib/libc
CFLAGS		+= -Ikernel/libc
CFLAGS 		+= -Ilimine
CFLAGS 		+= -nostdlib -nostdinc
CFLAGS		+= -mcmodel=kernel -mno-red-zone
CFLAGS		+= -ffreestanding -static
CFLAGS 		+= -mno-avx -mno-avx2
CFLAGS 		+= -mtune=skylake -march=skylake
SRCS 		+= $(shell find kernel -name '*.c' -not -path '*/hosted/*')

# Kernel-only dependencies
# zydis
SRCS 		+= $(shell find lib/zydis/dependencies/zycore/src -name '*.c')
CFLAGS		+= -Ilib/zydis/dependencies/zycore/include
SRCS 		+= $(shell find lib/zydis/src -name '*.c')
CFLAGS		+= -Ilib/zydis/include
CFLAGS 		+= -DZYAN_NO_LIBC
CFLAGS 		+= -DZYCORE_STATIC_BUILD
CFLAGS 		+= -DZYDIS_STATIC_BUILD

LDFLAGS		+= -Tkernel/linker.ld

########################################################################################################################
# Targets
########################################################################################################################

all: $(BIN_DIR)/pentagon.elf

hosted: $(BIN_DIR)/pentagon_hosted.elf

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)
BINS ?=
-include $(DEPS)

HOSTED_OBJS := $(HOSTED_SRCS:%=$(HOSTED_BUILD_DIR)/%.o)
HOSTED_DEPS := $(HOSTED_OBJS:%.o=%.d)
HOSTED_BINS ?=
-include $(HOSTED_DEPS)

$(BIN_DIR)/pentagon.elf: $(OBJS) | Makefile
	@echo LD $@
	@mkdir -p $(@D)
	@$(LD) $(LDFLAGS) -o $@ $^

$(BIN_DIR)/pentagon_hosted.elf: $(HOSTED_OBJS) | Makefile
	@echo LD $@
	@mkdir -p $(@D)
	@$(HOSTED_CC) $(HOSTED_CFLAGS) -o $@ $^

# For zyndis we tell it we are posix just so it will be happy
$(BUILD_DIR)/lib/zydis/%.c.o: lib/zydis/%.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -D__posix -Ilib/zydis/src -MMD -c $< -o $@

$(BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) -MMD -c $< -o $@

$(HOSTED_BUILD_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(HOSTED_CC) $(HOSTED_CFLAGS) -MMD -c $< -o $@

clean:
	rm -rf out
