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

CFLAGS		:= -Werror 
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

OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:%.o=%.d)
BINS ?=
-include $(DEPS)

all: $(BIN_DIR)/pentagon.elf

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

########################################################################################################################
# Other targets
########################################################################################################################

IMAGE_PATH 		:= $(BUILD_DIR)/test.hdd
TEMP_IMAGE		:= $(BUILD_DIR)/test_image
LOOPBACK_FILE	:= $(BUILD_DIR)/loopback_dev

QEMU_ARGS 		:= -hda $(IMAGE_PATH)
QEMU_ARGS 		+= -monitor telnet:localhost:1235,server,nowait
QEMU_ARGS 		+= -serial stdio
QEMU_ARGS 		+= -machine q35
QEMU_ARGS 		+= -smp 4
QEMU_ARGS 		+= -m 4G
QEMU_ARGS 		+= -no-reboot
QEMU_ARGS 		+= -no-shutdown

qemu: $(BIN_DIR)/pentagon.elf
	rm -f $(IMAGE_PATH)
	dd if=/dev/zero bs=1M count=0 seek=64 of=$(IMAGE_PATH)
	parted -s $(IMAGE_PATH) mklabel gpt
	parted -s $(IMAGE_PATH) mkpart primary 2048s 100%
	rm -rf $(TEMP_IMAGE)
	mkdir $(TEMP_IMAGE)
	losetup -Pf --show $(IMAGE_PATH) > $(LOOPBACK_FILE)
	partprobe `cat $(LOOPBACK_FILE)`
	mkfs.ext4 `cat $(LOOPBACK_FILE)`p1
	mount `cat $(LOOPBACK_FILE)`p1 $(TEMP_IMAGE)
	mkdir $(TEMP_IMAGE)/boot
	cp -rv $(BIN_DIR)/pentagon.elf test/limine.cfg limine/limine.sys $(TEST_IMAGE)/boot/
	cp -rv CoreLib/bin/Release/net5.0/CoreLib.dll $(TEMP_IMAGE)/boot/
	sync
	umount $(TEMP_IMAGE)
	losetup -d `cat $(LOOPBACK_FILE)`
	rm -rf $(TEMP_IMAGE) $(LOOPBACK_FILE)
	./limine/limine-install-linux-x86_64 $(IMAGE_PATH)
	qemu-system-x86_64 $(QEMU_ARGS)
