#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

TARGET_KERNEL_SOURCE ?= kernel/apple/asahi

TARGET_KERNEL_CONFIG_EXT := \
    kernel/mainline/configs/defconfigs/arm64/gki_defconfig \
    kernel/apple/asahi/arch/arm64/configs/asahi.config \
    kernel/mainline/configs/fragments/y/arm64/pagesize-16k.config \
    kernel/mainline/configs/fragments/y/fbcon.config \
    kernel/mainline/configs/fragments/n/disable-clang-hardening-features.config \
    kernel/mainline/configs/fragments/n/faster-build-time.config
