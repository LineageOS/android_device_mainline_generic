#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(USES_DEVICE_MAINLINE_GENERIC),true)

CMDLINE_EXTRA_TXT := $(PRODUCT_OUT)/cmdline-extra.txt

ifeq ($(TARGET_ARCH),arm64)
UKIFY_EFI_ARCH := aa64
UKIFY_OUT_FILENAME := BOOTAA64.EFI
else ifeq ($(TARGET_ARCH),x86_64)
UKIFY_EFI_ARCH := x64
UKIFY_OUT_FILENAME := BOOTX64.EFI
endif
UKIFY_OUT := $(PRODUCT_OUT)/$(UKIFY_OUT_FILENAME)

UKIFY_DEPS := \
    $(INSTALLED_RAMDISK_ALL_COMBINED_TARGET) \
    $(PRODUCT_OUT)/kernel

# $(1): output file
define make-ukify-out
	if [ -f "$(CMDLINE_EXTRA_TXT)" ]; then \
		echo "WARNING: $(CMDLINE_EXTRA_TXT) does not exist yet. Extra parameters are required to boot up, and you can put these on that file."; \
	fi
	PATH=/usr/local/bin:/usr/bin:/bin ukify build --efi-arch=$(UKIFY_EFI_ARCH) --linux=$(PRODUCT_OUT)/kernel --initrd=$(INSTALLED_RAMDISK_ALL_COMBINED_TARGET) --cmdline="$(BOARD_KERNEL_CMDLINE) $$(cat $(CMDLINE_EXTRA_TXT) 2>/dev/null)" --output=$(1)
endef

$(UKIFY_OUT): $(UKIFY_DEPS)

.PHONY: ukify_build
ukify_build: $(UKIFY_OUT)
	$(call make-ukify-out,$@)

.PHONY: ukify_build-nodeps
ukify_build-nodeps:
	$(call make-ukify-out,$(UKIFY_OUT))

endif # USES_DEVICE_MAINLINE_GENERIC
