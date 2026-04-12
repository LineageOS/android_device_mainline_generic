#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/mainline/generic

# Inherit from mainline/common
TARGET_AUDIO_HAL := tinyhal
TARGET_AUDIO_POLICY := custom
TARGET_CAMERA_PROVIDER_HAL := external
TARGET_ENABLE_LOGCAT_TO_SERIAL := true
TARGET_GRAPHICS_ALLOCATOR_HAL := minigbm-upstream
TARGET_GRAPHICS_COMPOSER_HAL := custom
TARGET_MESA_DO_NOT_SET_AS_DEFAULT := true
TARGET_SUPPORTS_SUSPEND := false
TARGET_USES_TABLET_INPUT_AS_TOUCHSCREEN := true
include device/mainline/common/optional/options.mk
$(call inherit-product, device/mainline/common/mainline_common.mk)

# Audio
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/audio/primary_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/primary_audio_policy_configuration.xml \
    device/google/cuttlefish/shared/config/audio/policy/audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_configuration.xml \
    frameworks/av/services/audiopolicy/config/audio_policy_volumes.xml:$(TARGET_COPY_OUT_VENDOR)/etc/audio_policy_volumes.xml \
    frameworks/av/services/audiopolicy/config/bluetooth_with_le_audio_policy_configuration_7_0.xml:$(TARGET_COPY_OUT_VENDOR)/etc/bluetooth_with_le_audio_policy_configuration_7_0.xml \
    frameworks/av/services/audiopolicy/config/default_volume_tables.xml:$(TARGET_COPY_OUT_VENDOR)/etc/default_volume_tables.xml \
    frameworks/av/services/audiopolicy/config/r_submix_audio_policy_configuration.xml:$(TARGET_COPY_OUT_VENDOR)/etc/r_submix_audio_policy_configuration.xml

TARGET_TINYHAL_DO_NOT_SET_AS_DEFAULT := true

# Bootanimation
TARGET_SCREEN_WIDTH := 300
TARGET_SCREEN_HEIGHT := 300

# Dalvik heap
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

# Graphics
PRODUCT_PACKAGES += \
    amdgpu.ids

# Graphics (Mesa)
TARGET_MESA_ENABLE_SOFTWARE_RENDERER := true

# Graphics allocator
PRODUCT_PACKAGES += \
    org.lineageos.device.gralloc.minigbm_upstream_nonapex \
    org.lineageos.device.gralloc.v2_0

PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.allocator@2.0-service \
    android.hardware.graphics.mapper@2.0-impl-2.1

PRODUCT_PACKAGES += \
    gralloc.gbm

TARGET_MINIGBM_UPSTREAM_ENABLE_GBM_MESA_DRIVER := true
TARGET_MINIGBM_UPSTREAM_INSIDE_APEX := false

$(call soong_config_set_bool,minigbm_upstream,include_vintf_fragments,false)

# Graphics composer
PRODUCT_PACKAGES += \
    org.lineageos.device.hwcomposer.drm \
    org.lineageos.device.hwcomposer.drm.rc \
    org.lineageos.device.hwcomposer.drm_apex \
    org.lineageos.device.hwcomposer.drmfb \
    org.lineageos.device.hwcomposer.v2_2 \
    org.lineageos.device.hwcomposer.v2_4

PRODUCT_PACKAGES += \
    android.hardware.graphics.composer@2.2-service \
    android.hardware.graphics.composer@2.4-service

PRODUCT_PACKAGES += \
    android.hardware.composer.hwc3-service.drm_upstream \
    android.hardware.graphics.composer@2.1-service.drmfb

$(call soong_config_set_bool,drm_hwcomposer_upstream,include_init_rc,false)
$(call soong_config_set_bool,drm_hwcomposer_upstream,include_vintf_fragments,false)
$(call soong_config_set_bool,drmfb_composer,include_vintf_fragments,false)

# Graphics Vulkan
PRODUCT_PACKAGES += \
    org.lineageos.device.graphics.vulkan.no_apex \
    org.lineageos.device.graphics.vulkan.swiftshader

## TODO(b/65201432): Swiftshader needs to create executable memory.
PRODUCT_REQUIRES_INSECURE_EXECMEM_FOR_SWIFTSHADER := true

# HIDL
PRODUCT_PACKAGES += \
    vndservicemanager

# Init
PRODUCT_COPY_FILES += \
    system/core/rootdir/ueventd.rc:$(TARGET_COPY_OUT_RAMDISK)/system/etc/ueventd.ramdisk.rc

PRODUCT_PACKAGES += \
    fstab.generic_init.addon.basic \
    generic_init_first_stage

PRODUCT_PACKAGES += \
    fstab.generic \
    init.generic.rc \
    ueventd.generic.rc

PRODUCT_PACKAGES += \
    device_generic_settings.rc \
    device_generic_settings.sh \
    init_dev_config_override.rc

PRODUCT_PACKAGES += \
    use_memfd.rc \
    zram.rc

PRODUCT_PACKAGES += \
    hardware_detect

$(call soong_config_set,mainline_common_libinit,set_properties_from,dmi_id)

# Input
PRODUCT_COPY_FILES += \
    $(call find-copy-subdir-files,*.kl,$(DEVICE_PATH)/configs/input/,$(TARGET_COPY_OUT_VENDOR)/usr/keylayout/)

# Images
PRODUCT_BUILD_BOOT_IMAGE := false
PRODUCT_BUILD_RAMDISK_IMAGE := true
PRODUCT_USE_DYNAMIC_PARTITION_SIZE := true

# Kernel
PRODUCT_OTA_ENFORCE_VINTF_KERNEL_REQUIREMENTS := false

# Kernel modules
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/modprobe/modules.options:$(TARGET_COPY_OUT_RAMDISK)/lib/modules/modules.options \
    $(DEVICE_PATH)/configs/modprobe/modules.options:$(TARGET_COPY_OUT_VENDOR)/lib/modules/modules.options

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlays/overlay

PRODUCT_PACKAGES += \
    AodDefaultOnOverlay

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/pc_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/pc_core_hardware.xml

# Scoped Storage
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# Shipping API level
PRODUCT_SHIPPING_API_LEVEL := 33

# Soong namespaces
PRODUCT_SOONG_NAMESPACES += \
    $(DEVICE_PATH) \
    kernel/mainline/configs

# USB
PRODUCT_PACKAGES += \
    com.android.hardware.usb.gadget.none

# Utilities
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/misc/pci.ids:$(TARGET_COPY_OUT_VENDOR)/pci.ids \
    $(DEVICE_PATH)/configs/misc/usb.ids:$(TARGET_COPY_OUT_VENDOR)/usb.ids

# Virtualization
$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)
