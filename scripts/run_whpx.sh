#!/usr/bin/env bash

./scripts/image.sh

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