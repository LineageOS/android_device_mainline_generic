# Installation

## Before getting started

You'll need to understand the fundamentals, and make decision on the available options, before starting with the actual installation.

The counterparts are: kernel, ramdisk, system, userdata, firmware.

In these counterparts, firmware can be borrowed from existing Linux OS installation or created during installation or ignored, userdata will be created during installation, the rest comes from Android build output.

Installation of kernel and ramdisk depends on how the bootloader can load these, which varies and is not in our control.

Installation of the rest of the counterparts is defined by us, and here we'll explain about it:

### Details of each of the counterparts

The system counterpart consists of these android partitions: `odm`, `odm_dlkm`, `product`, `system`, `system_ext`, `system_dlkm`, `vendor`, `vendor_dlkm`.

In the current configuration, `product` `system_ext` are included in `system`, `odm` `odm_dlkm` are included in `vendor`, `system_dlkm` `vendor_dlkm` are standalone.

The userdata counterpart consists of these android partitions: `cache` (ignore for now), `userdata`, `metadata`.

The firmware counterpart is all of the firmware files wanted by device drivers, being flattened in a subdirectory or packed into a image.

### Available options to install the counterparts

The term "android dir" is a directory containing the android-specific counterparts. The name of the directory can be customized via a boot parameter. If multiple block devices with android dir is found, the last found one is used.

system & userdata & firmware can be installed as images inside android dir. Except for userdata, there is also a option to copy the images to RAM and load the images from RAM instead of directly from the media.

userdata & firmware can be installed as subdirectories inside android dir. However, for userdata, this is unavailable if the file system holding android dir does not capable of Linux's file attributes and permissions.

system & userdata can be installed onto partitions on disk(s) with GPT partition table. For each of the installed partitions, the partition name on its entry on GPT partition table must be equal to the name of the corresponding android partition (for example, the partition which we install `system` android partition on should have partition name `system`).

system can be installed onto any block devices, which can be partitions or directly on disks.

firmware can be pre-existing in certain directories in the filesystem on any block device.

## Kernel and ramdisk

These should be placed at a location that the bootloader can access.

(TODO: kernel cmdline, making it bootable)
