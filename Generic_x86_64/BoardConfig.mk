# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from parent
include device/mainline/generic/BoardConfig.mk

# Architecture
TARGET_CPU_ABI := x86_64
TARGET_ARCH := x86_64
TARGET_ARCH_VARIANT := sandybridge

# Boot manager
TARGET_GRUB_ARCH := x86_64-efi
TARGET_GRUB_2ND_ARCH := i386-pc

# Boot parameters
BOARD_KERNEL_CMDLINE_SERIAL_CONSOLE := \
    8250.nr_uarts=1 \
    console=ttyS0

# Kernel
BOARD_KERNEL_IMAGE_NAME := bzImage
