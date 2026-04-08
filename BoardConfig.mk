#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

USES_DEVICE_MAINLINE_GENERIC := true

# Inherit from mainline/common
include device/mainline/common/BoardConfigMainlineCommon.mk

# A/B
AB_OTA_UPDATER := false

# Boot manager
TARGET_BOOT_MANAGER := grub
TARGET_GRUB_LIVE_CONFIGS := $(DEVICE_PATH)/configs/bootmgr/grub-live.cfg

# Boot parameters
BOARD_KERNEL_CMDLINE := \
    $(MAINLINE_COMMON_ANDROIDBOOT_PARAMS) \
    $(MAINLINE_COMMON_KERNEL_PARAMS) \
    androidboot.hardware=generic \
    androidboot.hypervisor.version=1 \
    androidboot.hypervisor.vm.supported=1 \
    androidboot.hypervisor.protected_vm.supported=0 \
    androidboot.init_fatal_pause=true \
    androidboot.selinux=permissive \
    androidboot.verifiedbootstate=orange \
    audit=0 \
    console=tty0 \
    mitigations=off \
    rdinit=/system/bin/generic_init

ifneq ($(TARGET_LINUX_FIRMWARE_REPO),)
BOARD_KERNEL_CMDLINE += \
    androidboot.mount_firmware=true
endif

# Filesystem
TARGET_USERIMAGES_SPARSE_EXT_DISABLED := true
TARGET_USERIMAGES_USE_F2FS := true
TARGET_USERIMAGES_USE_EXT4 := true

# Kernel
TARGET_KERNEL_CONFIG_EXT := \
    $(DEVICE_PATH)/configs/kernel/debian.config
TARGET_KERNEL_SOURCE ?= kernel/mainline/linux

# Kernel modules
BOARD_KERNEL_MODULES_LOAD_ALLOW_MISSING := true
BOARD_VENDOR_RAMDISK_KERNEL_MODULES_LOAD := $(shell cat $(DEVICE_PATH)/configs/modprobe/modules.load.ramdisk)
BOOT_KERNEL_MODULES_FINDER := $(DEVICE_PATH)/configs/kernel/boot_kernel_modules_finder.sh
TARGET_AUTO_COLLECT_KERNEL_MODULE_DEPS := true

# OTA
TARGET_SKIP_OTA_PACKAGE := true

# Partitions
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_USES_METADATA_PARTITION := true
BOARD_SYSTEMIMAGE_EXTFS_INODE_COUNT := -1
BOARD_SYSTEMIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE := 67108864
BOARD_VENDORIMAGE_EXTFS_INODE_COUNT := -1
BOARD_VENDORIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_VENDORIMAGE_PARTITION_RESERVED_SIZE := 67108864
TARGET_COPY_OUT_VENDOR := vendor

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
SYSTEM_EXT_PRIVATE_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/private
SYSTEM_EXT_PUBLIC_SEPOLICY_DIRS += $(DEVICE_PATH)/sepolicy/public

# VINTF
DEVICE_MANIFEST_FILE := \
    $(DEVICE_PATH)/configs/vintf/manifest.xml
