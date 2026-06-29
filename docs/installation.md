# Installation

## Before getting started

You'll need to understand the fundamentals, and make decision on the available options, before starting with the actual installation.

The counterparts are: kernel, ramdisk, system, userdata, firmware.

In these counterparts, firmware can be borrowed from existing Linux OS installation or created during installation or ignored, userdata will be created during installation or ignored, the rest comes from android build output.

Installation of kernel and ramdisk depends on how the bootloader can load these, which varies and is not in our control.

Installation of the rest of the counterparts is defined by us, and here we'll explain about it:

### Details of each of the counterparts

The system counterpart consists of these android partitions: `odm`, `odm_dlkm`, `product`, `system`, `system_ext`, `system_dlkm`, `vendor`, `vendor_dlkm`.

In the current configuration, `product` and `system_ext` are included in `system`, `odm` and `odm_dlkm` are included in `vendor`, `system_dlkm` and `vendor_dlkm` are standalone.

The userdata counterpart consists of these android partitions: `cache` (ignore for now), `userdata`, `metadata`.

The firmware counterpart is all of the firmware files wanted by device drivers, being flattened in a subdirectory or packed into a image.

### Available options to install the counterparts

The term "android dir" is a directory containing the android-specific counterparts. The name of the directory can be customized via a boot parameter. If multiple block devices with android dir is found, the last found one is used.

system & userdata & firmware can be installed as images inside android dir. Except for userdata, there is also a option to copy the images to RAM and load the images from RAM instead of directly from the media. The corresponding boot parameter values are `img` and `img_ram` (firmware), `imgs` and `imgs_ram` (system & userdata).

userdata & firmware can be installed as subdirectories inside android dir. However, for userdata, this is unavailable if the filesystem holding android dir does not support Linux's file attributes and permissions. The corresponding boot parameter value for userdata is `bind_mount_dir`, and the corresponding boot parameter value for firmware is `only_android_dir`.

system & userdata can be installed onto partitions on disk(s) with GPT partition table. Partition name on GPT partition table will be used to identify the android partition which the disk partition corresponds to. (for example, the disk partition which we install `system` android partition on should have partition name `system`). The corresponding boot parameter value is `std_parts`.

system can be installed onto any block devices, which can be partitions or directly on disks. The corresponding boot parameter value is `blk_devices`.

firmware can be pre-existing in certain directories in the filesystem on any block device. The corresponding boot parameter value is `all_possible_dirs`.

firmware can be ignored, in such case firmware files will not be available. The corresponding boot parameter value is `disable`.

userdata can be ignored, in such case the user's data will be stored in RAM and will not persist across reboot. The corresponding boot parameter value is `tmpfs`.

## Kernel and ramdisk

These should be placed at a location that the bootloader can access.

Kernel should be booted with kernel cmdline and ramdisk from the source described below.

Kernel cmdline comes from both of the following two sources:
1. The common part, which you can obtain from the android source tree, by executing `get_build_var BOARD_KERNEL_CMDLINE` after selecting the target device.
2. The custom part, which you have to construct on your own, according to [Boot parameters](boot-parameters.md) page.

For ramdisk, use `ramdisk-all-combined.img`.

Additionally, some SoC platforms may require booting the kernel with a devicetree blob.

The actual steps to configurate the bootloader to boot these depends on the bootloader, please visit the bootloader's documentation for details.

This device tree provides example boot manager configuration files and a build target that generates the combination of kernel & kernel cmdline & ramdisk
for EFI boot environment, please visit [Build](build.md#build-the-wanted-target) page for details.

## System

| Chosen method | To do |
|---------------|-------|
| Block devices | Write the android system images to the block devices which you wish to install on. |
| Images | Copy the android system images to the android dir. |
| Standard partitions | Write the android system images to the partitions on disk(s) with GPT partition table which you wish to install on, and set partition names on GPT partition table to the corresponding android system partition name. |

## Userdata

Android usually needs 1 GiB space for userdata at minimum.

| Chosen method | To do |
|---------------|-------|
| Ignore | Ensure the target system have enough total RAM to hold userdata in RAM. |
| Images | Create empty images with every android userdata partitions' name inside android dir. These created images should either contain only zeroes or formatted with EXT4 or F2FS filesystem. |
| Standard partitions | Set partition name of the partitions used for userdata on GPT partition table to the corresponding android userdata partition name, and then either ensure these partitions contain only zeroes or format these partitions with EXT4 or F2FS filesystem. |
| Subdirectories | Create empty subdirectories with every android userdata partitions' name in android dir. The filesystem holding android dir should be either EXT4 or F2FS. |

## Firmware

| Chosen method | To do |
|---------------|-------|
| Directories | Ensure there is at least one block device containing a directory listed on `std::list<std::string> possible_firmware_dirs` on `mount_handler.cpp`. Some of the directories in that list are normally available if the block device contain a GNU/Linux OS installation. |
| Ignore | Ensure the peripherals will require no firmware or will load its wanted firmware(s) via other ways. |
| Image | Build a image with firmware files included and place it to android dir with filename `firmware.img` or `linux-firmware.img`. Tips: 1. The filesystem on the image can be EROFS. 2. If you obtain firmware files from linux-firmware repository, use its `copy-firmware.sh` script. |
| Only android dir | Ensure the `firmware` subdirectory in android dir exists and contains needed firmware files. |
