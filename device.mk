#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/mainline/generic

# Inherit from mainline/common
TARGET_ENABLE_LOGCAT_TO_SERIAL := true
TARGET_SUPPORTS_SUSPEND := false
TARGET_SUPPORTS_USB_ACCESSORY_MODE := false
TARGET_USES_FRAMEBUFFER_DISPLAY := true
TARGET_USES_TABLET_INPUT_AS_TOUCHSCREEN := true
include device/mainline/common/optional/options.mk
$(call inherit-product, device/mainline/common/mainline_common.mk)

# Bootanimation
TARGET_SCREEN_WIDTH := 300
TARGET_SCREEN_HEIGHT := 300

# Dalvik heap
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

# HIDL
PRODUCT_PACKAGES += \
    vndservicemanager

# Init
PRODUCT_COPY_FILES += \
    system/core/rootdir/ueventd.rc:$(TARGET_COPY_OUT_RAMDISK)/system/etc/ueventd.ramdisk.rc

PRODUCT_PACKAGES += \
    generic_init_first_stage

PRODUCT_PACKAGES += \
    fstab.generic \
    init.generic.rc \
    init.recovery.generic.rc

$(call soong_config_set,mainline_common_libinit,set_properties_from,dmi_id)

ifeq ($(MAINLINE_GENERIC_USE_PRISTINE_KERNEL),true)
PRODUCT_PACKAGES += \
    use_memfd.rc
endif

# Images
PRODUCT_BUILD_BOOT_IMAGE := false
PRODUCT_BUILD_RAMDISK_IMAGE := true
PRODUCT_BUILD_RECOVERY_IMAGE := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

# Kernel
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlays/overlay

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/pc_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/pc_core_hardware.xml

# Scoped Storage
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# Shipping API level
PRODUCT_SHIPPING_API_LEVEL := 33

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(DEVICE_PATH)
