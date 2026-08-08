# Booting process

## Kernel

This is device-dependant.

Use a kernel that is built from a kernel tree that supports the device,
with Android configurations (maybe with some Android-specific patches too) included.

Boot the kernel like how the other working Linux OS does, or use other methods that's supported by the device to boot the kernel.

Use the kernel cmdline from the output of `get_build_var BOARD_KERNEL_CMDLINE` command
in the Android source tree after selecting the target device, and maybe with some
custom specified parameters too.

Use `ramdisk-all-combined.img` from the Android target device output directory as ramdisk.

Ways to boot the kernel includes but not limited to:
- Boot loaders (U-Boot, device-specific boot loader, etc)
- Boot managers (GRUB, rEFInd, syslinux, systemd-boot, etc)
- Execute the kernel directly in EFI environment (if the kernel have EFI stub support enabled)

## Ramdisk

Ramdisk could be a concatenation of multiple ramdisk images in the same format.

Ramdisk contains init program and its configuration, possibly with kernel modules and firmwares too.

Kernel executes the init program in ramdisk specified by `rdinit=` parameter.
By default it's `/init`, here we have it set to `/system/bin/generic_init`.

Normal Android uses `/init` program which is built from `system/core/init`.

Here we have and uses our own fork of it, which is more flexible and have more features included, named `generic_init`.

Its source code is at `services/generic_init` in this repository.

The `generic_init` is basically a merge of AOSP `init_first_stage` and `ueventd`, with some wiring up in between,
additionally with a mount handler that replaces `first_stage_mount.cpp` that automatically scans partitions
and generates fstab entries on-the-fly.

### Basic environment setup in ramdisk

This is done by the init program, and the implementation in `generic_init`
does not really differ from the implementation in AOSP `/init`.

### Loading basic kernel modules in ramdisk

Kernel modules that are required to probe the boot media, and the kernel modules for basic hardware platform support,
and the kernel modules required for outputting the logs (i.e. serial device driver or basic display driver),
should get installed to `/lib/modules` in the ramdisk.

The `generic_init` will firstly load the modules listed on `modules.load`
and the modules with prefix listed on `modules.load_prefix` in `/lib/modules`.

Then, it will start the `ueventd` component, to listen for uevents from the kernel,
and load modules depending on the `MODALIAS` field in the uevents.

If the loaded kernel modules requests for firmware, the `ueventd` component will
try to provide the requested firmware from several directories, specified by the
ueventd configuration at `/system/etc/ueventd.ramdisk.rc` in ramdisk.

The `ueventd` component will also inform the mount handler about added block devices,
then the mount handler will scan the block devices and save the information to memory.

Once the mount handler have collected block devices that's enough to mount every required
Android partitions depending on its parameters, the `ueventd` component will quit,
and proceed with the next step.

### Mounting Android partitions

The mount handler gets called again, and it will do some preparations
(e.g. mounting base partitions and setting up loop devices), then construct a virtual fstab
(with the entries on the specified addon fstab included), finally pass the result fstab to
`MountHelpers::MountPartitions()` which is basically a copy of
`FirstStageMountVBootV2::MountPartitions()` from AOSP `first_stage_mount.cpp`,
and let it finally do the rest of mounts.

After mounting completes, the root directory will be switched over to the mounted Android OS.

### Executing custom vendor init program

The `generic_init` will execute `/vendor/bin/vendor_init`.

Preparations such as generating firmwares can be done in that program.

### Loading kernel modules from the mounted partitions

The `generic_init` will firstly load the modules listed on `modules.load`
and the modules with prefix listed on `modules.load_prefix`, again, but
in `/vendor/lib/modules` instead of in `/lib/modules` which is in ramdisk.

And the `ueventd` component in `generic_init` starts again, but it uses configuration
from `/system/etc/ueventd.rc` from the mounted partitions instead.

The `/system/etc/ueventd.rc` imports `/vendor/etc/ueventd.rc`, in our case it's copied from
`device/mainline/common/init/ueventd.rc`, it imports hardware-specific configuration file
based on properties. However, properties aren't loaded at this moment, so we convert from
`androidboot.` parameters instead. Refer to the `TranslatePropName()` call in `ExpandProps()`
on `util.cpp` in `generic_init` for details of how it works.

The `ueventd` component in `generic_init` will load modules depending on received uevents once again.

Additionally, since the partitions are mounted, we have a usermode helper at `/vendor/bin/modprobe_kernel`
which can handle kernel requesting for modules with exact name. Some non-basic drivers uses that method.

Firmware requests gets handled as well in the previous way, but searching in
the directories specified by the newly loaded ueventd configuration instead.

We want to get as many as possible modules loaded as early as possible, because
there's a service that will execute very early in the mounted Android OS
does HAL selections depending on the probed hardwares.

If there is no new uevents to handle in consecutive 5 seconds,
the `ueventd` component in `generic_init` will quit, and proceed with the next step.

### Start the mounted Android OS

The `generic_init` executes `/system/bin/init` same as how AOSP `/init` does.

## The mounted Android OS

Here we only document about the different parts.

### The hardware_detect service

We have a service that does hardware related configurations and selects the HALs to be used,
named `hardware_detect`, with service name `init_dev_config`, located at `/vendor/bin/hardware_detect`.

Its source code is at `services/hardware_detect` in this repository.

For details of how it works, please check out the source code.

If this service didn't execute successfully, Android will finally crash due to unselected APEXes.

### The get_display_ppi service

It calculates PPI (Pixels-Per-Inch) value from the EDID from the first connected connector
from the DRM card that is selected, and sets the PPI value to LCD density property.

Its source code is at `services/get_display_ppi` in this repository.
