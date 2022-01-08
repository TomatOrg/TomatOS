#!/usr/bin/env bash

set -e

# Create the disk
rm -f test.hdd
dd if=/dev/zero bs=1M count=0 seek=64 of=test.hdd
parted -s test.hdd mklabel gpt
parted -s test.hdd mkpart primary 2048s 100%

# Setup ext2 with limine
echo $SUDO_PASSWORD | sudo -S rm -rf test_image/
sudo mkdir test_image
sudo losetup -Pf --show test.hdd > loopback_dev
sudo partprobe `cat loopback_dev`
sudo mkfs.ext4 `cat loopback_dev`p1
sudo mount `cat loopback_dev`p1 test_image
sudo mkdir test_image/boot
sudo cp -rv pentagon ../test/limine.cfg ../limine/limine.sys test_image/boot/
sudo cp -rv ../CoreLib/bin/Release/net5.0/CoreLib.dll test_image/boot/
sync
sudo umount test_image/
sudo losetup -d `cat loopback_dev`
rm -rf test_image loopback_dev
../limine/limine-install-linux-x86_64 test.hdd
nohup qemu-system-x86_64 \
  -hda test.hdd \
  -monitor telnet:localhost:1235,server,nowait \
  -serial telnet:localhost:1236,server,nowait \
  -machine q35 \
  -smp 4 \
  -m 4G \
  -no-reboot \
  -no-shutdown \
  -s -S 0<&- &>/dev/null &
sleep 1