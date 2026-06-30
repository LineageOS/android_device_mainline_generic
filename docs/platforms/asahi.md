# Asahi platform

This platform is for devices which Asahi Linux supports.

Support for this platform is based on Asahi Linux.

This documentation was initially written based on `Apple Mac mini (M1, 2020)` device.
More testing and feedbacks are welcome.

## Before getting started with Android on the target device

You'll need to install and run a variant of Asahi Linux on the target device first.
For details, check out [the official website of Asahi Linux](https://asahilinux.org).

Here we will use the "Fedora Linux Asahi Remix" variant as example.

During installation, create a empty partition that will be used to store Android OS later.
The size of it should be like 8 GiB and more. We will call it "android partition" later.

After booting into the Linux OS, format the android partition to either EXT4 or F2FS, and mount it.

Note that if you want USB support in U-Boot and GRUB, you'll need to rollback U-Boot by the following command:
```
sudo dnf install uboot-images-armv8-1:2025.10-101.fc42
```

## Build Android

Follow the instructions on [Build](../build.md) page.

We will need to build these two build targets: `all_images` and `bootmgr-configs`.

Except:
1. Copy the content in `/usr/lib/firmware/vendor` in Linux OS on the target device to `prebuilts/ramdisk/vendor/firmware`
in this device tree, right after this [step](../build.md#cloning-this-device-tree-and-its-dependencies-in-lineageos-org).
2. Before this [step](../build.md#select-the-target-device), clone the Asahi Linux kernel source from
[here](https://github.com/AsahiLinux/linux) to `kernel/apple/asahi`, apply the patches listed on this
[table](https://github.com/LineageOS/android_device_xiaomi_mi7150-mainline/#kernel-patches),
do edits according to [this](https://github.com/LineageOS/android_device_xiaomi_mi7150-mainline/#kernel-edits),
finally specify to use kernel build configuration for Asahi Linux kernel by running this command:
`export MAINLINE_GENERIC_KERNEL_USE=asahi`.

## Install Android

Do the following on the target device:

1. Enter the mountpoint of the android partition.

2. Create a directory named `android-Generic_arm64`, and enter it.

3. Copy `kernel` `ramdisk-all-combined.img` `system.img` `vendor.img` `system_dlkm.img` `vendor_dlkm.img` from Android build output to this directory.

4. Create these empty directories: `metadata` `userdata`.

5. Copy `/usr/lib/firmware/vendor/` to `firmware`.

6. Copy `bootmgr-configs/single-partition/grub.cfg` to `/boot/grub2/custom.cfg`.

7. Edit `/boot/grub2/custom.cfg`:

- Create a empty line right after the line `function boot_android {`, and write `    search --file --no-floppy --set=root /android-Generic_arm64/kernel`.
- On the line beginning with `    linux`, move the cursor to right before `$@` on the end of the line, and write `androidboot.mount_firmware=only_android_dir androidboot.prefer_drm_render_name=asahi `.
- Remove the entire block containing `Mount userdata from images`, as it does not match with our setup.

## Boot Android

Restart the target device to GRUB menu, and select the option containing `Mounting userdata from subdirs`.
