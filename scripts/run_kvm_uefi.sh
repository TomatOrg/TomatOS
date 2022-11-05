#!/usr/bin/env bash

./scripts/image.sh

qemu-system-x86_64 \
  -drive if=virtio,file=out/build/test.hdd \
  -serial mon:stdio \
  -machine q35 \
  --enable-kvm \
  -cpu host,+invtsc,+tsc-deadline \
  -smp 1 \
  -bios /usr/share/ovmf/x64/OVMF_CODE.fd \
  -m 2G \
  -s \
  -no-reboot \
  -no-shutdown \