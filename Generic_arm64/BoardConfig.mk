#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from parent
include device/mainline/generic/BoardConfig.mk

# Architecture
TARGET_ARCH := arm64
TARGET_ARCH_VARIANT := armv8-a
TARGET_CPU_ABI := arm64-v8a
TARGET_CPU_ABI2 :=
TARGET_CPU_VARIANT := generic

# Boot manager
TARGET_GRUB_ARCH := arm64-efi

# Graphics (Mesa)
BOARD_MESA3D_GALLIUM_DRIVERS += \
    asahi \
    ethosu \
    etnaviv \
    freedreno \
    lima \
    nouveau \
    panfrost \
    tegra \
    v3d \
    vc4
#    rocket

BOARD_MESA3D_VULKAN_DRIVERS += \
    asahi \
    broadcom \
    freedreno \
    imagination \
    panfrost

# Graphics allocator (minigbm)
$(call soong_config_set,minigbm_upstream,platform,all_arm)

# Kernel
ifeq ($(MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK),)
BOARD_KERNEL_IMAGE_NAME := Image
TARGET_KERNEL_CONFIG_EXT := \
    $(TARGET_DEVICE_PATH)/configs/kernel/pre-debian.config \
    $(PRODUCT_OUT)/obj/KCONFIG_OBJ/debian-filtered.config \
    $(DEVICE_PATH)/configs/kernel/fix-build.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    $(DEVICE_PATH)/configs/kernel/customizations.config
endif
