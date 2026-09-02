#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

DEVICE_PATH := device/mainline/generic

# Inherit from mainline/common
TARGET_AUDIO_HAL := tinyhal
TARGET_AUDIO_POLICY := custom
TARGET_CONSOLE_AS_ROOT := true
TARGET_ENABLE_LOGCAT_TO_SERIAL := true
TARGET_ENABLE_FBKEYBOARD := true
TARGET_ENABLE_VIRT_WIFI := true
TARGET_EXTERNAL_CAMERA_PROVIDER_HAL := default-aidl
TARGET_GRAPHICS_ALLOCATOR_HAL := minigbm-upstream
TARGET_GRAPHICS_COMPOSER_HAL := drm_hwcomposer
TARGET_HEALTH_HAL := custom
TARGET_HOSTAPD_AND_WPA_SUPPLICANT_FORM := legacy
TARGET_MESA_DO_NOT_SET_AS_DEFAULT := true
TARGET_SUPPORTS_SUSPEND := false
TARGET_USES_TABLET_INPUT_AS_TOUCHSCREEN := true
include device/mainline/common/optional/options.mk
$(call inherit-product, device/mainline/common/mainline_common.mk)

# APEX
OVERRIDE_PRODUCT_COMPRESSED_APEX := false

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

# Display
PRODUCT_PACKAGES += \
    get_display_ppi

# Graphics
PRODUCT_PACKAGES += \
    amdgpu.ids

# Graphics (Mesa)
TARGET_MESA_ENABLE_SOFTWARE_RENDERER := true

# Graphics allocator
PRODUCT_PACKAGES += \
    com.android.hardware.graphics.allocator.fb

PRODUCT_PACKAGES += \
    mapper.fb

TARGET_MINIGBM_UPSTREAM_ENABLE_GBM_MESA_DRIVER := true

$(call soong_config_set,fb_graphics,RELEASE_SM_OPEN_DECLARED_PASSTHROUGH_HAL,$(RELEASE_SM_OPEN_DECLARED_PASSTHROUGH_HAL))

# Graphics composer
PRODUCT_PACKAGES += \
    com.android.hardware.graphics.composer.drmfb \
    com.android.hardware.graphics.composer.empty

# Graphics Vulkan
PRODUCT_PACKAGES += \
    org.lineageos.device.graphics.vulkan.no_apex \
    org.lineageos.device.graphics.vulkan.swiftshader

## TODO(b/65201432): Swiftshader needs to create executable memory.
PRODUCT_REQUIRES_INSECURE_EXECMEM_FOR_SWIFTSHADER := true

# Health
PRODUCT_PACKAGES += \
    android.hardware.health-service.generic \
    android.hardware.health-service.generic_recovery \
    charger_res_images_vendor

# HIDL
PRODUCT_PACKAGES += \
    vndservicemanager

# HWDB
PRODUCT_PACKAGES += \
    60-sensor.hwdb \
    hwdb_d_pci_ids \
    hwdb_d_usb_ids

$(call soong_config_set_bool,hwdb_d,pci_ids_enabled,true)
$(call soong_config_set_bool,hwdb_d,usb_ids_enabled,true)

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
    console_override.rc \
    device_generic_logging.rc \
    device_generic_settings.rc \
    device_generic_settings.sh \
    hal_services.rc \
    init-chowns.sh \
    init_dev_config_override.rc

PRODUCT_PACKAGES += \
    use_memfd.rc \
    zram.rc

PRODUCT_PACKAGES += \
    hardware_detect \
    overlay_remounter_override

$(call soong_config_set,mainline_common_libinit,set_properties_from,both)

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
    $(DEVICE_PATH)/configs/modprobe/modules.load_prefix:$(TARGET_COPY_OUT_VENDOR_DLKM)/lib/modules/modules.load_prefix \
    $(DEVICE_PATH)/configs/modprobe/modules.options:$(TARGET_COPY_OUT_RAMDISK)/lib/modules/modules.options \
    $(DEVICE_PATH)/configs/modprobe/modules.options:$(TARGET_COPY_OUT_VENDOR_DLKM)/lib/modules/modules.options

PRODUCT_PACKAGES += \
    modprobe_kernel

# Overlays
DEVICE_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlays/overlay

PRODUCT_PACKAGE_OVERLAYS += \
    $(DEVICE_PATH)/overlays/product_overlay-tablet

PRODUCT_PACKAGES += \
    MainlineGenericWifiOverlay

PRODUCT_PACKAGES += \
    AodDefaultOnOverlay

# Permissions
PRODUCT_COPY_FILES += \
    frameworks/native/data/etc/pc_core_hardware.xml:$(TARGET_COPY_OUT_VENDOR)/etc/permissions/pc_core_hardware.xml

# Scoped Storage
$(call inherit-product, $(SRC_TARGET_DIR)/product/emulated_storage.mk)

# Sensors
$(call soong_config_set_bool,sensors_hal_mainline,include_all_permission_xmls,true)
$(call soong_config_set_bool,sensors_hal_mainline,run_as_root,true)

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
PRODUCT_PACKAGES += \
    sh_vendor_bootstrap \
    toybox_vendor_bootstrap

# Virtualization
$(call inherit-product, packages/modules/Virtualization/apex/product_packages.mk)

# Wi-Fi
PRODUCT_COPY_FILES += \
    $(DEVICE_PATH)/configs/wifi/wpa_supplicant.conf:$(TARGET_COPY_OUT_VENDOR)/etc/wifi/wpa_supplicant.conf
