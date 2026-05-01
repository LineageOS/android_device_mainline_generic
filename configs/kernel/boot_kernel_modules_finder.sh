#!/bin/bash

export PATH=/usr/local/bin:/usr/bin:/bin

KERNEL_MODULES_OUT=$1

if ! [ -d "$KERNEL_MODULES_OUT" ]; then
    echo "KERNEL_MODULES_OUT directory cannot be accessed"
    exit 1
fi

# Based on Debian trixie /usr/share/initramfs-tools/hook-functions auto_add_modules()
# Here we include board platform drivers, USB drivers, storage drivers.

# Base
MODULES="ext4
f2fs
virtio_mmio
virtio_pci"
MODULES_DIRS="drivers/bus
drivers/clk
drivers/devfreq
drivers/dma
drivers/extcon
drivers/gpio
drivers/hwspinlock
drivers/interconnect
drivers/i2c/busses
drivers/i2c/muxes
drivers/mailbox
drivers/memory
drivers/mfd
drivers/nvmem
drivers/pci/controller
drivers/phy
drivers/platform/chrome
drivers/power
drivers/pinctrl
drivers/regulator
drivers/reset
drivers/rpmsg
drivers/rtc
drivers/soc
drivers/spi
drivers/spmi
drivers/usb
drivers/watchdog
net/qrtr"

# ATA, Block, IDE, MMC, SCSI
MODULES+="
cxgb3i
cxgb4i
scsi_dh_alua
scsi_dh_emc
scsi_dh_rdac
scsi_transport_srp
vmd"
MODULES_DIRS+="
drivers/ata
drivers/block
drivers/ide
drivers/message/fusion
drivers/mmc
drivers/nvme
drivers/s390/scsi
drivers/scsi
drivers/ufs"

# Our additions

# Basic display
MODULES+="
efifb
simpledrm
simplefb
vesafb"
MODULES_DIRS+="
drivers/gpu/drm/sysfb
drivers/video/fbdev/core"

# Filesystems
MODULES+="
apfs
erofs
fat
isofs
msdos
ntfs3
overlay
squashfs
udf
vfat"
MODULES_DIRS+="
fs/nls"

for mod in $MODULES; do
    mod_filename="${mod}.ko"
    mod_find=$(find ${KERNEL_MODULES_OUT}/kernel/ -name $mod_filename)
    if [ "$mod_find" ]; then
        RESULT+=" $(basename $mod_find)"
    else
        echo "Module ${mod} not found" 1>&2
    fi
done

for dir in $MODULES_DIRS; do
    dir_path="${KERNEL_MODULES_OUT}/kernel/${dir}"
    if ! [ -d "$dir_path" ]; then continue; fi

    mod_finds=$(find $dir_path -name '*.ko')
    for mod_find in $mod_finds; do
        RESULT+=" $(basename $mod_find)"
    done
done

echo "$RESULT"

exit 0
