#!/usr/bin/env bash

./scripts/image.sh

qemu-system-x86_64 \
  -drive if=virtio,file=out/build/test.hdd \
  -monitor telnet:localhost:1235,server,nowait \
  -serial stdio \
  -machine q35 \
  --enable-kvm \
  -cpu host,+invtsc,+tsc-deadline \
  -smp 4 \
  -m 2G \
  -s \
  -no-reboot \
  -no-shutdown