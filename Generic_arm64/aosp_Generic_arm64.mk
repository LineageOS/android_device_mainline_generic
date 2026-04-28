#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

# Inherit from those products. Most specific first.
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit_only.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Inherit from device
$(call inherit-product, device/mainline/generic/Generic_arm64/device.mk)

PRODUCT_NAME := aosp_Generic_arm64
PRODUCT_DEVICE := Generic_arm64
PRODUCT_BRAND := Generic
PRODUCT_MANUFACTURER := Generic
PRODUCT_MODEL := Generic arm64
