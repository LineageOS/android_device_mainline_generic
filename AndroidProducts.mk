#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

PRODUCT_MAKEFILES := \
    $(LOCAL_DIR)/Generic_arm64/aosp_Generic_arm64.mk \
    $(LOCAL_DIR)/Generic_arm64/lineage_Generic_arm64.mk \
    $(LOCAL_DIR)/Generic_x86_64/aosp_Generic_x86_64.mk \
    $(LOCAL_DIR)/Generic_x86_64/lineage_Generic_x86_64.mk

$(foreach build_type, user userdebug eng, \
    $(eval COMMON_LUNCH_CHOICES += aosp_Generic_arm64-$(build_type)) \
    $(eval COMMON_LUNCH_CHOICES += aosp_Generic_x86_64-$(build_type)) \
    $(eval COMMON_LUNCH_CHOICES += lineage_Generic_arm64-$(build_type)) \
    $(eval COMMON_LUNCH_CHOICES += lineage_Generic_x86_64-$(build_type)))
