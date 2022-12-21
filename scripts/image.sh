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
  lib/tinydotnet/lib/utf8-utf16-converter/tester/test-cases/two-way/bible.utf8.txt \
  lib/tinydotnet/corelib/Corelib/bin/Release/net6.0/Corelib.dll \
  TomatOS/Tomato.Hal/bin/Release/net6.0/Tomato.Hal.dll \
  TomatOS/Tomato.Graphics/bin/Release/net6.0/Tomato.Graphics.dll \
  TomatOS/Tomato.Terminal/bin/Release/net6.0/Tomato.Terminal.dll \
  TomatOS/Tomato.Drivers.Virtio/bin/Release/net6.0/Tomato.Drivers.Virtio.dll \
  TomatOS/Tomato.Drivers.Fat/bin/Release/net6.0/Tomato.Drivers.Fat.dll \
  out/build/test_image/boot/

# Finish with the disk, umount it
sync
sudo umount out/build/test_image
sudo losetup -d `cat out/build/loopback_dev`
rm -rf out/build/loopback_dev out/build/test_image
