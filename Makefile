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

#ifeq ($(DEBUG),1)
OPTIMIZE		?= 0
#else
#OPTIMIZE		?= 1
#endif

#-----------------------------------------------------------------------------------------------------------------------
# Directories
#-----------------------------------------------------------------------------------------------------------------------

BUILD_DIR		:= build
BIN_DIR			:= $(BUILD_DIR)/bin
OBJ_DIR			:= $(BUILD_DIR)/obj

#-----------------------------------------------------------------------------------------------------------------------
# Flags
#-----------------------------------------------------------------------------------------------------------------------

#
# Toolchain
#
CC				:= clang
LD				:= ld.lld

#
# Compiler flags
# 
CFLAGS			:= -target x86_64-pc-none-elf
CFLAGS			+= -Wall -Werror -std=gnu11 -fshort-wchar
CFLAGS 			+= -Wno-address-of-packed-member
CFLAGS			+= -mgeneral-regs-only -msse2
CFLAGS			+= -fno-pie -fno-pic -ffreestanding -fno-builtin -static
CFLAGS			+= -mcmodel=kernel -mno-red-zone -mgeneral-regs-only
CFLAGS			+= -nostdlib
CFLAGS			+= -Ikernel
CFLAGS			+= -flto
CFLAGS			+= -g
CFLAGS			+= -march=x86-64-v3

# We are relying on frame pointers for proper stack unwinding
# in both managed and unmanaged environment
CFLAGS 			+= -fno-omit-frame-pointer
CFLAGS			+= -Ilib
CFLAGS			+= -I$(BUILD_DIR)/limine

CFLAGS			+= -fms-extensions -Wno-microsoft
CFLAGS			+= -Ilib/TomatoDotNet/include

# Debug flags
ifeq ($(DEBUG),1)
CFLAGS			+= -Wno-unused-function -Wno-unused-label 
CFLAGS			+= -D__DEBUG__
else
CFLAGS			+= -DNDEBUG
endif

# Optimization flags
ifeq ($(OPTIMIZE),1)
CFLAGS			+= -Os
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
SRCS 	:=
SRCS 	+= kernel/acpi/acpi.c
SRCS 	+= kernel/arch/apic.c
SRCS 	+= kernel/arch/gdt.c
SRCS 	+= kernel/arch/idt.c
SRCS 	+= kernel/arch/smp.c
SRCS 	+= kernel/debug/log.c
SRCS 	+= kernel/lib/except.c
SRCS 	+= kernel/lib/stb_sprintf.c
SRCS 	+= kernel/lib/string.c
SRCS 	+= kernel/mem/gc/gc.c
SRCS 	+= kernel/mem/alloc.c
SRCS 	+= kernel/mem/phys.c
SRCS 	+= kernel/mem/virt.c
SRCS 	+= kernel/runtime/tdn.c
SRCS 	+= kernel/sync/spinlock.c
SRCS 	+= kernel/thread/pcpu.c
SRCS 	+= kernel/thread/scheduler.c
SRCS 	+= kernel/thread/thread.c
SRCS 	+= kernel/time/timer.c
SRCS 	+= kernel/main.c

# TomatoDotNet sources
SRCS 		+= lib/TomatoDotNet/src/dotnet/gc/gc.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/jit/jit.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/jit/jit_internal.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/metadata/metadata.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/metadata/pe.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/metadata/sig.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/types/assembly.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/types/method.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/types/string.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/types/type.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/disasm.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/loader.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/types.c
SRCS 		+= lib/TomatoDotNet/src/util/except.c
SRCS 		+= lib/TomatoDotNet/src/util/list.c
SRCS 		+= lib/TomatoDotNet/src/util/stb_ds.c
SRCS 		+= lib/TomatoDotNet/src/util/string_builder.c

# TomatoDotNet jit (spidir)
SRCS 		+= lib/TomatoDotNet/src/dotnet/jit/spidir/jit.c
SRCS 		+= lib/TomatoDotNet/src/dotnet/jit/spidir/platform.c
TDN_CFLAGS 	+= -D__JIT_SPIDIR__ -Ilib/TomatoDotNet/libs/spidir/c-api/include

# Add the flanterm code for early console
SRCS 		+= lib/flanterm/flanterm.c
SRCS 		+= lib/flanterm/backends/fb.c

# The objects/deps 
OBJ 		:= $(addprefix $(OBJ_DIR)/,$(SRCS:.c=.c.o))
DEPS		:= $(addprefix $(OBJ_DIR)/,$(SRCS:.c=.c.d))

OBJ			+= lib/TomatoDotNet/libs/spidir/target/x86_64-unknown-none/release/libspidir.a

# Default target.
.PHONY: all
all: $(BIN_DIR)/$(KERNEL).elf

# Get the header deps
-include $(DEPS)

# Link rules for the final kernel executable.
$(BIN_DIR)/$(KERNEL).elf: Makefile kernel/linker.ld $(OBJ)
	@echo LD $@
	@mkdir -p "$$(dirname $@)"
	@$(LD) $(OBJ) $(LDFLAGS) -o $@

# Compilation rules for TomatoDotNet *.c files.
$(OBJ_DIR)/lib/TomatoDotNet/src/%.c.o: lib/TomatoDotNet/src/%.c Makefile
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -MMD $(CFLAGS) $(TDN_CFLAGS) -Ilib/TomatoDotNet/src -c $< -o $@

# Compilation rules for *.c files.
$(OBJ_DIR)/%.c.o: %.c Makefile $(BUILD_DIR)/limine/limine.h
	@echo CC $@
	@mkdir -p $(@D)
	@$(CC) -MMD $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: distclean
distclean: clean
	rm -rf $(BUILD_DIR)

#-----------------------------------------------------------------------------------------------------------------------
# Spidir lib
#-----------------------------------------------------------------------------------------------------------------------

.PHONY: force

# spidir targets
lib/TomatoDotNet/libs/spidir/target/x86_64-unknown-none/release/libspidir.a: force
	cd lib/TomatoDotNet/libs/spidir/c-api && cargo build --release -p c-api --target x86_64-unknown-none

lib/TomatoDotNet/libs/spidir/target/x86_64-unknown-none/debug/libspidir.a: force
	cd lib/TomatoDotNet/libs/spidir/c-api && cargo build -p c-api --target x86_64-unknown-none

#-----------------------------------------------------------------------------------------------------------------------
# C# binaries
#-----------------------------------------------------------------------------------------------------------------------

lib/TomatoDotNet/TdnCoreLib/System.Private.CoreLib/bin/Debug/net8.0/System.Private.CoreLib.dll: force
	cd lib/TomatoDotNet/TdnCoreLib/System.Private.CoreLib && dotnet build

DLLS 	:= lib/TomatoDotNet/TdnCoreLib/System.Private.CoreLib/bin/Debug/net8.0/System.Private.CoreLib.dll

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
		-m 2G \
		-smp 4 \
		-hda $(IMAGE_NAME).hdd \
		-debugcon stdio \
		-no-reboot \
	 	-no-shutdown | ./scripts/addr_translate.py $(BIN_DIR)/$(KERNEL).elf
