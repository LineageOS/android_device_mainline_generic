#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

BOARD_KERNEL_IMAGE_NAME := Image
TARGET_KERNEL_SOURCE := kernel/apple/asahi

TARGET_KERNEL_CONFIG_EXT := \
    kernel/mainline/configs/defconfigs/arm64/gki_defconfig \
    $(TARGET_DEVICE_PATH)/kernels/asahi/fixup-deps.config \
    $(TARGET_KERNEL_SOURCE)/arch/arm64/configs/asahi.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    kernel/mainline/configs/fragments/n/disable-clang-hardening-features.config \
    kernel/mainline/configs/fragments/n/faster-build-time.config \
    $(DEVICE_PATH)/configs/kernel/customizations.config
