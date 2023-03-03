#!/usr/bin/env bash

./scripts/image.sh

qemu-system-x86_64 \
  -drive if=virtio,file=out/build/test.hdd \
  -serial tcp:localhost:1235,server,nowait \
  -monitor tcp:localhost:1236,server,nowait \
  -debugcon stdio \
  -machine q35 \
  --enable-kvm \
  -cpu host,+invtsc,+tsc-deadline \
  -bios /usr/share/ovmf/bios.bin \
  -smp 4 \
  -m 2G \
  -s \
  -no-reboot \
  -no-shutdown