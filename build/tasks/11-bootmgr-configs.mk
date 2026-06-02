#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(USES_DEVICE_MAINLINE_GENERIC),true)

BOOTMGR_CONFIGS_SRC_DIR := $(DEVICE_PATH)/configs/bootmgr
BOOTMGR_CONFIGS_OUT_DIR := $(PRODUCT_OUT)/bootmgr-configs
BOOTMGR_CONFIGS_TIMESTAMP := $(BOOTMGR_CONFIGS_OUT_DIR)/.timestamp

$(BOOTMGR_CONFIGS_TIMESTAMP):
	rm -rf $(BOOTMGR_CONFIGS_OUT_DIR)

	cp -r $(BOOTMGR_CONFIGS_SRC_DIR) $(BOOTMGR_CONFIGS_OUT_DIR)
	$(call process-bootmgr-cfg-common,$$(find $(BOOTMGR_CONFIGS_OUT_DIR) -type f))

	touch $(BOOTMGR_CONFIGS_TIMESTAMP)

.PHONY: bootmgr-configs
bootmgr-configs: $(BOOTMGR_CONFIGS_TIMESTAMP)

endif # USES_DEVICE_MAINLINE_GENERIC
