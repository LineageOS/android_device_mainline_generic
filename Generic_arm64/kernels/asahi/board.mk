#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

BOARD_KERNEL_IMAGE_NAME := Image
TARGET_KERNEL_SOURCE := kernel/apple/asahi

ifeq ($(KERNEL_ASAHI_USE_GKI_DEFCONFIG),true)
# Untested yet
TARGET_KERNEL_CONFIG_EXT := \
    kernel/mainline/configs/defconfigs/arm64/gki_defconfig \
    $(TARGET_DEVICE_PATH)/kernels/asahi/fixup-deps-gki_defconfig.config \
    $(TARGET_KERNEL_SOURCE)/arch/arm64/configs/asahi.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    kernel/mainline/configs/fragments/n/disable-clang-hardening-features.config \
    kernel/mainline/configs/fragments/n/faster-build-time.config \
    $(DEVICE_PATH)/configs/kernel/customizations.config
else
TARGET_KERNEL_CONFIG := defconfig
TARGET_KERNEL_CONFIG_EXT := \
    $(TARGET_DEVICE_PATH)/kernels/asahi/fixup-deps-defconfig.config \
    $(TARGET_KERNEL_SOURCE)/arch/arm64/configs/asahi.config \
    kernel/mainline/configs/fragments/android-base-pre/common.config \
    kernel/mainline/configs/fragments/android-base-pre/arm64.config \
    kernel/configs/b/android-6.12/android-base.config \
    kernel/mainline/configs/fragments/android-base-conditional/CONFIG_ARM64-y.config \
    kernel/mainline/configs/fragments/common.config
endif

TARGET_KERNEL_CONFIG_EXT += \
    $(TARGET_DEVICE_PATH)/kernels/asahi/addons.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    kernel/mainline/configs/fragments/n/disable-clang-hardening-features.config \
    kernel/mainline/configs/fragments/n/faster-build-time.config \
    $(DEVICE_PATH)/configs/kernel/customizations.config
