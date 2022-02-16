#!/usr/bin/env bash

# Create the disk
rm -f out/build/test.hdd
dd if=/dev/zero bs=1M count=0 seek=64 of=out/build/test.hdd
parted -s out/build/test.hdd mklabel gpt
parted -s out/build/test.hdd mkpart primary 2048s 100%

# Setup ext2 with limine
sudo umount out/build/test_image
sudo rm -rf out/build/test_image/
sudo mkdir out/build/test_image
sudo losetup -Pf --show out/build/test.hdd > out/build/loopback_dev
sudo partprobe `cat out/build/loopback_dev`
sudo mkfs.ext4 `cat out/build/loopback_dev`p1
sudo mount `cat out/build/loopback_dev`p1 out/build/test_image
sudo mkdir out/build/test_image/boot
sudo cp -rv out/bin/pentagon.elf test/limine.cfg limine/limine.sys out/build/test_image/boot/
sudo cp -rv CoreLib/bin/Release/net5.0/CoreLib.dll out/build/test_image/boot/
sync
sudo umount out/build/test_image/
sudo losetup -d `cat out/build/loopback_dev`
rm -rf out/build/test_image out/build/loopback_dev
./limine/limine-install-linux-x86_64 out/build/test.hdd
qemu-system-x86_64 \
  -hda out/build/test.hdd \
  -monitor telnet:localhost:1235,server,nowait \
  -serial stdio \
  -machine q35 \
  --enable-kvm \
  -cpu Nehalem,+invtsc,+tsc-deadline \
  -smp 4 \
  -m 4G \
  -s \
  -S \
  -no-reboot \
  -no-shutdown