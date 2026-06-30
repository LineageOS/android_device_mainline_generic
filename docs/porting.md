# Porting

This page describes the steps in general to port Android OS to your target device using this device tree.

## Make sure the device can already boot up Linux OS

If there is existing Linux OS ports that works for your target device and you can reference from it, go ahead.

## Get the necessary bits from the working Linux OS

You'll need to know:
- List of its hardwares and what does these require to function
- How does the bootloader start up a Linux OS
- What firmware files do the hardwares want
- What partitions can be used to store Android OS

You'll need to obtain:
- Firmware files that are wanted by the hardwares, if exist
- Kernel source code
- Source code of out-of-tree kernel modules for hardware drivers, if exist
- Toolchain for compiling the kernel, if needed

## Make custom kernel build configuration

If the target device will not use the default kernel build configuration, make its own build kernel configuration as follows.

Create a `.mk` file in the Android source tree for the place to put kernel build configuration on, and specify the path to the file by this command:

```
export MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK=<path to the kernel build configuration file relative to the Android source tree>
```

For the available kernel build configuration options, please check out `vendor/lineage/build/tasks/kernel.mk` and `vendor/lineage/config/BoardConfigKernel.mk`.
Note that some options are still being set by us by default even if custom build kernel configuration is used.

Kernel configuration must have Android-specific options enabled. You can get the Android-specific options enabled by including some kernel config fragments from
`kernel/configs` and `kernel/mainline/configs`. The kernel build configuration file `Generic_arm64/kernels/asahi/board.mk` in this device tree is an example of
how it's supposed to be done.

Note that some of the options on kernel config fragments may not actually get applied to the main kernel config,
due to unsatisfied dependencies. To automatically find out the unapplied options on kernel config fragments, use the utility at
`kernel/mainline/configs/utilities/validate_kernel_config.py`.

## Kernel modules configuration

### Kernel modules installation

Some kernel modules should be installed into ramdisk. For details, please visit [Loading basic kernel modules in ramdisk](booting-process.md#loading-basic-kernel-modules-in-ramdisk).

By default, we decide which kernel modules to be installed in ramdisk using approach like Debian's `initramfs-tools`, which is mostly based on the module's source directory.
You can view and modify the implementation, on `configs/kernel/boot_kernel_modules_finder.sh` in this device tree.

All of the kernel modules are installed in `vendor_dlkm` partition.

### Kernel modules loading list

By default, kernel modules are loaded upon kernel's requests. You can also specify to load some specific modules, by editing
`configs/modprobe/modules.load.ramdisk` for ramdisk modules,
and `configs/modprobe/modules.load` `configs/modprobe/modules.load_prefix` for post-ramdisk modules in this device tree.

### Kernel modules load options

If the target device needs to load kernel modules with specific options, specify the options on a file named `modules.options`.

Check out the file `configs/modprobe/modules.options` in this device tree for the file's syntax.

If modules loaded in ramdisk needs specifying options, install the file to `/lib/modules/modules.options` in ramdisk;

If modules loaded after ramdisk needs specifying options, install the file to `/lib/modules/modules.options` in `vendor_dlkm` partition.

## Include firmware files in ramdisk

Kernel modules loaded in ramdisk may request for firmware files, and such of firmware files should be included in ramdisk.

To include firmware files in ramdisk, copy these into `prebuilts/ramdisk/vendor/firmware` path (with directory structure preserved) in this device tree.

## Build

Please visit [Build](build.md) page.

## Install

Please visit [Installation](installation.md) page.

## Troubleshooting with booting

For some possible ways to debug it, please visit [Debugging](debugging.md) page.

You can read the [Booting process](booting-process.md) page for better understanding of the issue.

Here is a table of common issues that we may encounter during initial bringup:

| Issue | Suggestion |
|-------|------------|
| Instant crash right after bootloader stage | Check and fix bootloader configuration and kernel build configuration. |
| No display after the kernel is started | Confirm whether if the system is still responding via some other ways. If the system is still responding, display driver might be missing or there is failures in loading. |
| Kernel log have `generic_init: CanQuitUeventd: ` lines repeating at the end | Check for issues related with installation. |
| Android does not play bootanimation successfully | Try to use framebuffer display as temporary workaround. |
