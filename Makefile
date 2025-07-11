########################################################################################################################
# TomatOS
########################################################################################################################

# Nuke built-in rules and variables.
override MAKEFLAGS += -rR

KERNEL			:= tomatos

#-----------------------------------------------------------------------------------------------------------------------
# General Config
#-----------------------------------------------------------------------------------------------------------------------

# Are we compiling as debug or not 
DEBUG 			?= 1

ifeq ($(DEBUG),1)
OPTIMIZE		?= 0
else
OPTIMIZE		?= 1
endif

#-----------------------------------------------------------------------------------------------------------------------
# Directories
#-----------------------------------------------------------------------------------------------------------------------

BUILD_DIR		:= build
BIN_DIR			:= $(BUILD_DIR)/bin
OBJS_DIR		:= $(BUILD_DIR)/obj

#-----------------------------------------------------------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------------------------------------------------------

#
# Toolchain
#
CC				:= clang
AR				:= llvm-ar
LD				:= ld.lld

ifeq ($(DEBUG),1)
TDN_BIN_DIR		:= lib/TomatoDotNet/out/debug/bin
else
TDN_BIN_DIR		:= lib/TomatoDotNet/out/release/bin
endif

#
# Common compilation flags, also passed to the libraries
# 
COMMON_CFLAGS	:= -target x86_64-pc-none-elf
COMMON_CFLAGS	+= -mgeneral-regs-only
COMMON_CFLAGS	+= -march=x86-64-v3 -mxsave -mxsaveopt
COMMON_CFLAGS	+= -fno-pie -fno-pic -ffreestanding -fno-builtin -static
COMMON_CFLAGS	+= -mcmodel=kernel -mno-red-zone
COMMON_CFLAGS	+= -nostdlib
COMMON_CFLAGS	+= -flto -g
COMMON_CFLAGS	+= -fno-omit-frame-pointer -fvisibility=hidden

# Optimization flags
ifeq ($(OPTIMIZE),1)
COMMON_CFLAGS	+= -O3
else
COMMON_CFLAGS	+= -O0
endif

# Debug flags
ifeq ($(DEBUG),1)
COMMON_CFLAGS	+= -fsanitize=undefined
COMMON_CFLAGS 	+= -fno-sanitize=alignment
endif

# Our compilation flags
CFLAGS			:= $(COMMON_CFLAGS)
CFLAGS			+= -Wall -Werror -std=gnu11
CFLAGS 			+= -Wno-address-of-packed-member
CFLAGS			+= -Ikernel
CFLAGS			+= -DLIMINE_API_REVISION=2

# Debug flags
# ifeq ($(DEBUG),1)
CFLAGS			+= -Wno-unused-function -Wno-unused-label -Wno-unused-variable
# endif

# We are relying on frame pointers for proper stack unwinding
# in both managed and unmanaged environment
CFLAGS 			+=
CFLAGS			+= -Ilib/flanterm/src
CFLAGS			+= -Ilib/buddy_alloc -DBUDDY_HEADER
CFLAGS			+= -I$(BUILD_DIR)/limine

# Things required by TDN
CFLAGS			+= -fms-extensions -Wno-microsoft
CFLAGS			+= -Ilib/TomatoDotNet/include
CFLAGS			+= -Ilib/TomatoDotNet/libs/spidir/c-api/include

ifeq ($(DEBUG),1)
CFLAGS			+= -D__DEBUG__
endif

#
# Linker flags
# 
LDFLAGS			:= -Tkernel/linker.ld -nostdlib -static

#-----------------------------------------------------------------------------------------------------------------------
# Stb printf
#-----------------------------------------------------------------------------------------------------------------------

CFLAGS 			+= -DSTB_SPRINTF_NOFLOAT

#-----------------------------------------------------------------------------------------------------------------------
# Sources
#-----------------------------------------------------------------------------------------------------------------------

# Get list of source files
SRCS 		:= $(shell find kernel -name '*.c')
SRCS 		+= $(shell find kernel -name '*.S')

# Add the flanterm code for early console
SRCS 		+= lib/flanterm/src/flanterm.c
SRCS 		+= lib/flanterm/src/flanterm_backends/fb.c
CFLAGS 		+= -DFLANTERM_FB_DISABLE_BUMP_ALLOC

# The objects/deps 
OBJS 		:= $(SRCS:%=$(OBJS_DIR)/%.o)
DEPS 		:= $(OBJS:%.o=%.d)

# Link against TomatoDotNet
OBJS 		+= $(TDN_BIN_DIR)/libtdn.a

# The C# DLLs we need
DLLS		:= $(BIN_DIR)/Tomato.Kernel.dll
DLLS		+= $(BIN_DIR)/System.Private.CoreLib.dll

# Default target.
.PHONY: all
all: $(BIN_DIR)/$(KERNEL).elf

# Get the header deps
-include $(DEPS)

# Link rules for the final kernel executable.
$(BIN_DIR)/$(KERNEL).elf: kernel/linker.ld $(OBJS)
	@echo LD $@
	@mkdir -p "$$(dirname $@)"
	@$(LD) $(OBJS) $(LDFLAGS) -o $@

$(OBJS_DIR)/%.c.o: %.c
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

$(OBJS_DIR)/%.S.o: %.S
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -MMD -MP $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJS_DIR) $(BIN_DIR)
	$(MAKE) -C lib/TomatoDotNet clean

.PHONY: distclean
distclean: clean
	rm -rf $(BUILD_DIR)

#-----------------------------------------------------------------------------------------------------------------------
# TomatoDotNet
#-----------------------------------------------------------------------------------------------------------------------

.PHONY: force

$(TDN_BIN_DIR)/libtdn.a: force
	@echo MAKE $@
	@$(MAKE) -C lib/TomatoDotNet \
		CC="$(CC)" \
		AR="$(AR)" \
		LD="$(LD)" \
		DEBUG="$(DEBUG)" \
		CFLAGS="$(COMMON_CFLAGS)" \
		SPIDIR_CARGO_FLAGS="--target=$(abspath artifacts/target.json) -Zbuild-std=core,alloc" \
		SPIDIR_RUSTUP_TOOLCHAIN="nightly-2025-05-07" \

#-----------------------------------------------------------------------------------------------------------------------
# All the binaries
#-----------------------------------------------------------------------------------------------------------------------

# Just build the debug one
$(BIN_DIR)/Tomato.Kernel.dll: ManagedKernel/Tomato.Kernel/bin/Debug/net8.0/Tomato.Kernel.dll
	@mkdir -p $(@D)
	cp $^ $@

# Copy the corelib outside
$(BIN_DIR)/System.Private.CoreLib.dll: ManagedKernel/Tomato.Kernel/bin/Debug/net8.0/System.Private.CoreLib.dll
	@mkdir -p $(@D)
	cp $^ $@

# Build the kernel itself
# TODO: build the entire solution instead?
ManagedKernel/Tomato.Kernel/bin/Debug/net8.0/Tomato.Kernel.dll: force
	cd ManagedKernel/Tomato.Kernel && dotnet build --configuration Debug

# The C# build system handles this by copying it properly
ManagedKernel/Tomato.Kernel/bin/Debug/net8.0/System.Private.CoreLib.dll: ManagedKernel/Tomato.Kernel/bin/Debug/net8.0/Tomato.Kernel.dll

#-----------------------------------------------------------------------------------------------------------------------
# Quick test
#-----------------------------------------------------------------------------------------------------------------------

$(BUILD_DIR)/limine/limine.h: $(BUILD_DIR)/limine

# Clone and build limine utils
$(BUILD_DIR)/limine:
	mkdir -p $(@D)
	cd $(BUILD_DIR) && git clone https://github.com/limine-bootloader/limine.git --branch=v8.x-binary --depth=1
	$(MAKE) -C $(BUILD_DIR)/limine

# The name of the image we are building
IMAGE_NAME 	:= $(BIN_DIR)/$(KERNEL)

# Build a limine image with both bios and uefi boot options
$(IMAGE_NAME).hdd: artifacts/limine.conf $(BUILD_DIR)/limine $(BIN_DIR)/$(KERNEL).elf $(DLLS)
	mkdir -p $(@D)
	rm -f $(IMAGE_NAME).hdd
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_NAME).hdd
	sgdisk $(IMAGE_NAME).hdd -n 1:2048 -t 1:ef00
	./$(BUILD_DIR)/limine/limine bios-install $(IMAGE_NAME).hdd
	mformat -i $(IMAGE_NAME).hdd@@1M
	mmd -i $(IMAGE_NAME).hdd@@1M ::/EFI ::/EFI/BOOT
	mcopy -i $(IMAGE_NAME).hdd@@1M $(BIN_DIR)/$(KERNEL).elf artifacts/limine.conf $(BUILD_DIR)/limine/limine-bios.sys ::/
	mcopy -i $(IMAGE_NAME).hdd@@1M $(DLLS) ::/
	mcopy -i $(IMAGE_NAME).hdd@@1M $(BUILD_DIR)/limine/BOOTX64.EFI ::/EFI/BOOT

.PHONY: run
run: $(IMAGE_NAME).hdd
	qemu-system-x86_64 \
		--enable-kvm \
		-cpu host,+invtsc,+tsc-deadline \
		-machine q35 \
		-smp 1 \
		-s \
		-hda $(IMAGE_NAME).hdd \
		-debugcon stdio \
		-no-reboot \
	 	-no-shutdown
