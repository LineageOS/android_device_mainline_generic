#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

TARGET_DEVICE_PATH := device/mainline/generic/Generic_arm64

# Inherit from parent
$(call inherit-product, device/mainline/generic/device.mk)

# Audio
PRODUCT_COPY_FILES += \
    $(TARGET_DEVICE_PATH)/../configs/audio/audio.generic.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio.Generic_arm64.xml
