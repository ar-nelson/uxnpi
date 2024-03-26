#!/bin/bash	
set -euo pipefail

dd if=/dev/zero count=128 bs=1M > roms.img
(echo o; echo n; echo p; echo 1; echo 2048; echo 99999; echo t; echo 6; echo w; echo q) | fdisk roms.img
LOOPDEV=$(sudo losetup --partscan --find --show roms.img)
sudo mkfs.fat -F 16 "${LOOPDEV}p1"
mkdir temp-mount
sudo mount -o uid=$UID,sync ${LOOPDEV}p1 temp-mount
cp roms/* temp-mount/
sudo umount temp-mount
rmdir temp-mount
sudo losetup -d ${LOOPDEV}
