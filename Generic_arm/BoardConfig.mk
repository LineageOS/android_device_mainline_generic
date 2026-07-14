#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from parent
include device/mainline/generic/BoardConfig.mk

# Architecture
TARGET_ARCH := arm
TARGET_ARCH_VARIANT := armv7-a-neon
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_VARIANT := generic

# Graphics (Mesa)
BOARD_MESA3D_GALLIUM_DRIVERS += \
    freedreno \
    lima \
    panfrost \
    v3d \
    vc4

BOARD_MESA3D_VULKAN_DRIVERS += \
    broadcom \
    freedreno \
    panfrost

# Graphics allocator (minigbm)
$(call soong_config_set,minigbm_upstream,platform,all_arm)

# Kernel
ifeq ($(MAINLINE_GENERIC_KERNEL_BOARDCONFIG_MK),)
BOARD_KERNEL_IMAGE_NAME ?= zImage
TARGET_KERNEL_CONFIG :=
TARGET_KERNEL_CONFIG_EXT := \
    $(PRODUCT_OUT)/obj/KCONFIG_OBJ/debian.config \
    kernel/mainline/configs/fragments/android-base-pre/common.config \
    kernel/configs/b/android-6.12/android-base.config \
    kernel/mainline/configs/fragments/android-base-conditional/CONFIG_ARM-y.config \
    kernel/mainline/configs/fragments/common.config \
    $(DEVICE_PATH)/configs/kernel/fix-build.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    $(DEVICE_PATH)/configs/kernel/customizations.config
TARGET_KERNEL_CLANG_COMPILE := false
TARGET_KERNEL_NO_GCC := false
TARGET_KERNEL_USE_HOST_GCC := true
endif

# Kernel modules
BOARD_VENDOR_KERNEL_MODULES_LOAD += $(shell cat $(TARGET_DEVICE_PATH)/configs/modprobe/modules.load)
