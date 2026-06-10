#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(USES_DEVICE_MAINLINE_GENERIC),true)

# product and system_ext could be here if we add them later on
LOCAL_WANTED_SYSTEM_IMAGES := \
    system

LOCAL_WANTED_SYSTEM_IMAGES_FILES := \
    $(addsuffix .img,$(LOCAL_WANTED_SYSTEM_IMAGES))

# We aren't GKI compatible so kernel and system_dlkm counts into vendor for us...
LOCAL_WANTED_VENDOR_IMAGES := \
    ramdisk-all-combined \
    system_dlkm \
    vendor \
    vendor_dlkm

LOCAL_WANTED_VENDOR_IMAGES_FILES := \
    $(addsuffix .img,$(LOCAL_WANTED_VENDOR_IMAGES)) \
    kernel

.PHONY: all_images all_system_images all_vendor_images
all_images: $(addprefix $(PRODUCT_OUT)/,$(LOCAL_WANTED_SYSTEM_IMAGES_FILES) $(LOCAL_WANTED_VENDOR_IMAGES_FILES))
all_system_images: $(addprefix $(PRODUCT_OUT)/,$(LOCAL_WANTED_SYSTEM_IMAGES_FILES))
all_vendor_images: $(addprefix $(PRODUCT_OUT)/,$(LOCAL_WANTED_VENDOR_IMAGES_FILES))

endif # USES_DEVICE_MAINLINE_GENERIC
