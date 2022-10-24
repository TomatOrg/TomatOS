#!/usr/bin/env bash

# Create the disk, single partition which is the ESP
rm -f out/build/test.hdd
dd if=/dev/zero bs=1M count=0 seek=64 of=out/build/test.hdd
parted -s out/build/test.hdd mklabel gpt
parted -s out/build/test.hdd mkpart ESP fat32 2048s 100%
parted -s out/build/test.hdd set 1 esp on

# Setup limine
limine/limine-deploy out/build/test.hdd

# Setup the mount
sudo losetup -Pf --show out/build/test.hdd >out/build/loopback_dev
sudo mkfs.fat -F 32 `cat out/build/loopback_dev`p1
mkdir -p out/build/test_image
sudo mount `cat out/build/loopback_dev`p1 out/build/test_image

# Create dirs
sudo mkdir -p out/build/test_image/EFI/BOOT
sudo mkdir -p out/build/test_image/boot

# Copy the limine UEFI bootloader
sudo cp -rv limine/BOOTX64.EFI out/build/test_image/EFI/BOOT/
sudo cp -rv limine/limine.sys out/build/test_image/boot/

# Copy the kernel contents
sudo cp -rv \
  out/bin/tomatos.elf \
  artifacts/limine.cfg \
  lib/tinydotnet/corelib/Corelib/bin/Release/net6.0/Corelib.dll \
  TomatOS/Tomato.Hal/bin/Release/net6.0/Tomato.Hal.dll \
  artifacts/kbd.dat \
  artifacts/ubuntu-regular.sdfnt \
  out/build/test_image/boot/

# Finish with the disk, umount it
sync
sudo umount out/build/test_image
sudo losetup -d `cat out/build/loopback_dev`
rm -rf out/build/loopback_dev out/build/test_image

/mnt/c/Program\ Files/qemu/qemu-system-x86_64.exe \
  -drive if=virtio,file=Z:$(pwd)/out/build/test.hdd \
  -serial mon:stdio \
  -machine q35,kernel-irqchip=off \
  -accel whpx \
  -cpu qemu64,vendor=GenuineIntel,+invtsc,+tsc-deadline \
  -smp 4 \
  -m 2G \
  -s \
  -no-reboot \
  -no-shutdown