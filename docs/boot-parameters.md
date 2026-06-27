# Boot parameters

## generic_init

### androidboot.addon_fstab_suffix

Specifies the suffix for the fstab file in ramdisk to be loaded as addon.

### androidboot.android_dir

Specifies the name of the directory containing files for booting Android.

It will be the first directory to be checked in the directory list containing possible android directories.

### androidboot.board_platform

When `generic_init` is parsing `.rc` files, it have to expand properties for the lines that refers to properties.

Given properties are not ready yet at this stage, we workaround that by looking up from `androidboot.` parameters instead.

The property name `ro.board.platform` gets translated to this parameter key. The value set to this parameter will be used when looking up that property.

### androidboot.init_fatal_pause

When this is set to `true`, if `generic_init` crashes, it will end up hanging forever instead of restarting the system.
Useful for reading crash related logs.

### androidboot.modalias_handling_delay_ms

This sets how many milliseconds to delay before modalias handler loads a module.

Might be useful for finding out the module that crashes the system quickly.

### androidboot.mount_firmware

Specifies how it's supposed to mount `/mnt/vendor/firmware` mountpoint.

Here is a table of supported options:

| Value | Description |
|-------|-------------|
| `all_possible_dirs` or `true` | Search for all possible directories containing firmware files, and then bind mount the found directory to this mountpoint. the possible directories list is `std::list<std::string> possible_firmware_dirs` on `mount_handler.cpp`. |
| `disable` or `false` | Skip mounting this mountpoint. |
| `img` | Mount the mountpoint from firmware image inside android directory. The firmware image name could be either `firmware.img` or `linux-firmware.img`. |
| `img_ram` | Similar to `img` option, but the firmware image will be copied to RAM and mounted from RAM. |
| `only_android_dir` | Similar to `all_possible_dirs` option, but it will only check inside the android directory. |

### androidboot.mount_system

Specifies how it's supposed to mount the entire android system.

Here is a table of supported options:

| Value | Description |
|-------|-------------|
| `std_parts` | Map the `PARTNAME` uevent field (typically the name of the partition entry set on GPT partition table) of block devices to android system partitions. |
| `blk_devices` | Automatically determine which android system partition the block device is, from every block devices, based on the `build.prop` file inside of them. |
| `imgs` | Map all possible android system images in android directory to android system partitions based on theirs name. |
| `imgs_ram` | Similar to `imgs` option, but the images will be copied to RAM and mounted from RAM. |

### androidboot.mount_userdata

Specifies how it's supposed to mount the partitions for user's data in android.

Here is a table of supported options:

| Value | Description |
|-------|-------------|
| `std_parts` | Map the `PARTNAME` uevent field (typically the name of the partition entry set on GPT partition table) of block devices to android userdata partitions. |
| `imgs` | Map all possible android userdata images in android directory to android userdata partitions based on theirs name. |
| `bind_mount_dir` | Bind mount directories with android userdata partition name in android directory to the corresponding mountpoint. |
| `tmpfs` | Use `tmpfs` type of mount for android userdata mountpoints, for storing the content in RAM. |

## hardware_detect

### androidboot.prefer_drm_card_name

Use DRM card with the specified name if exists.

### androidboot.prefer_drm_render_name

Use DRM render with the specified name if exists.

### androidboot.enum_drm_in_reverse

When this is set to `true`, DRM devices will be enumerated in reverse order. Use this if you want to pick up DRM devices starting from the ending ones instead of the beginning ones instead.

### androidboot.graphics

When this is set to `swiftshader`, Swiftshader graphics will be used if possible, which is for CPU rendering.

### androidboot.use_fb_display

When this is set to `true`, display output will be set to framebuffer devices, which is the option that has max compatibility and least functionality. 

### Overrides for selections in hardware_detect service

Please check out the `kBootOverridesProp` map in `hardware_detect` source code.

## Init rc

### androidboot.lcd_density

Passes the set value to property `ro.sf.lcd_density`.
If this is not set, the property `ro.sf.lcd_density` will be set by the `get_display_ppi` service.

### androidboot.save_early_logs

When this is set to `1`, The logs (only logcat for now) before booting completes will be saved to `/metadata`.

### androidboot.seriallogging

If you want logcat getting printed to a serial device, set this to the name of the serial device.

### androidboot.seriallogging.logcat_buffer

Sets the logcat buffer(s) to be printed to the serial device.

The set value will be passed to the `-b` option of `logcat`.

### androidboot.wifi_impl

When this is set to `virt_wifi`, `setup_wifi` service will be started and it will create a wlan interface that is bridged to a existing ethernet interface.

## libinit

### androidboot.insecure_adb

When this is set to `true`, adb authentication is disabled, adb runs as root by default, and adb service will be available earlier during boot.
