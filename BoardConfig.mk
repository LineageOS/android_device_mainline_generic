#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

USES_DEVICE_MAINLINE_GENERIC := true

# Inherit from mainline/common
include device/mainline/common/BoardConfigMainlineCommon.mk

# A/B
AB_OTA_UPDATER := false

# Audio
$(call soong_config_set_bool,tinyhal,libaudiohalcm_continue_on_ctl_open_error,true)

# Boot manager
TARGET_BOOT_MANAGER := grub
TARGET_GRUB_LIVE_CONFIGS := $(DEVICE_PATH)/configs/bootmgr/live/grub.cfg

# Boot parameters
BOARD_KERNEL_CMDLINE := \
    $(MAINLINE_COMMON_ANDROIDBOOT_PARAMS) \
    $(filter-out firmware_class.path=%,$(MAINLINE_COMMON_KERNEL_PARAMS)) \
    androidboot.addon_fstab_suffix=basic \
    androidboot.console=tty0 \
    androidboot.hardware=generic \
    androidboot.hypervisor.version=1 \
    androidboot.hypervisor.vm.supported=1 \
    androidboot.hypervisor.protected_vm.supported=0 \
    androidboot.init_fatal_pause=true \
    androidboot.selinux=permissive \
    androidboot.verifiedbootstate=orange \
    audit=0 \
    console=tty0 \
    firmware_class.path=/mnt/vendor/firmware/ \
    mitigations=off \
    rdinit=/system/bin/generic_init \
    sysctl.kernel.firmware_config.force_sysfs_fallback=1 \
    sysctl.kernel.modprobe=/vendor/bin/modprobe_kernel

# Filesystem
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_USERIMAGES_USE_EXT4 := true

# Graphics (Mesa)
BOARD_MESA3D_BUILD_LIBGBM := true
BOARD_MESA3D_GALLIUM_DRIVERS += \
    nouveau \
    radeonsi \
    r300 \
    r600 \
    svga \
    virgl
BOARD_MESA3D_VULKAN_DRIVERS += \
    amd \
    nouveau \
    virtio

# Graphics allocator
$(call soong_config_set_bool,minigbm_upstream,disable_virgl_native_yuv,true)

# Kernel
ifneq ($(MAINLINE_GENERIC_KERNEL_USE),)
MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK ?= $(TARGET_DEVICE_PATH)/kernels/$(MAINLINE_GENERIC_KERNEL_USE)/board.mk
endif
ifneq ($(MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK),)
include $(MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK)
else
TARGET_KERNEL_CONFIG := gki_defconfig
TARGET_KERNEL_SOURCE ?= kernel/mainline/android-mainline
endif

# Kernel modules
BOARD_KERNEL_MODULES_LOAD_ALLOW_MISSING := true
BOARD_VENDOR_KERNEL_MODULES_LOAD := $(shell cat $(DEVICE_PATH)/configs/modprobe/modules.load)
BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(shell cat $(DEVICE_PATH)/configs/modprobe/modules.load.ramdisk)
BOOT_KERNEL_MODULES_FINDER := $(DEVICE_PATH)/configs/kernel/boot_kernel_modules_finder.sh
TARGET_AUTO_COLLECT_KERNEL_MODULE_DEPS := true

# OTA
TARGET_SKIP_OTA_PACKAGE := true

# Partitions
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_USES_METADATA_PARTITION := true
BOARD_USES_SYSTEM_DLKMIMAGE := true
BOARD_USES_VENDOR_DLKMIMAGE := true
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_SYSTEM_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := erofs
BOARD_VENDOR_DLKMIMAGE_FILE_SYSTEM_TYPE := erofs
TARGET_COPY_OUT_SYSTEM_DLKM := system_dlkm
TARGET_COPY_OUT_VENDOR := vendor
TARGET_COPY_OUT_VENDOR_DLKM := vendor_dlkm

# Platform
TARGET_BOARD_PLATFORM := generic

# Properties
TARGET_PRODUCT_PROP += $(DEVICE_PATH)/configs/properties/product.prop
TARGET_VENDOR_PROP += \
    $(DEVICE_PATH)/configs/properties/vendor.prop \
    $(DEVICE_PATH)/configs/properties/vendor_bluetooth_profiles.prop

# Ramdisk
BOARD_RAMDISK_USE_LZ4 := true

# SELinux
BOARD_VENDOR_SEPOLICY_DIRS += \
    $(DEVICE_PATH)/sepolicy/vendor

SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/private
SYSTEM_EXT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/public

# VINTF
DEVICE_MANIFEST_FILE := \
    $(DEVICE_PATH)/configs/vintf/manifest.xml
