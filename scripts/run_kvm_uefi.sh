#!/usr/bin/env bash

./scripts/image.sh
sudo ip tuntap add mode tap tap0
sudo ip link set dev tap0 up promisc on
sudo ip addr add 10.1.1.2/24 dev tap0

qemu-system-x86_64 \
  -drive if=virtio,file=out/build/test.hdd \
  -serial mon:stdio \
  -machine q35 \
  --enable-kvm \
  -cpu host,+invtsc,+tsc-deadline \
  -smp 1 \
  -device virtio-net-pci,netdev=net1 \
  -netdev tap,id=net1,ifname=tap0,script=no,downscript=no \
  -bios /usr/share/ovmf/bios.bin \
  -m 2G \
  -s \
  -no-reboot \
  -no-shutdown \