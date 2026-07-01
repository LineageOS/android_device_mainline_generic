#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

BOARD_KERNEL_IMAGE_NAME := Image
TARGET_KERNEL_SOURCE ?= kernel/mainline/android-mainline

TARGET_KERNEL_CONFIG := defconfig
TARGET_KERNEL_CONFIG_EXT := \
    $(TARGET_DEVICE_PATH)/kernels/sun50i-h64-remix-mini-pc/sun50i-a64.config \
    $(TARGET_DEVICE_PATH)/kernels/sun50i-h64-remix-mini-pc/sun50i-h64-remix-mini-pc.config \
    kernel/mainline/configs/fragments/android-base-pre/common.config \
    kernel/mainline/configs/fragments/android-base-pre/arm64.config \
    kernel/configs/b/android-6.12/android-base.config \
    kernel/mainline/configs/fragments/android-base-conditional/CONFIG_ARM64-y.config \
    kernel/mainline/configs/fragments/common.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    kernel/mainline/configs/fragments/n/disable-clang-hardening-features.config \
    kernel/mainline/configs/fragments/n/faster-build-time.config
