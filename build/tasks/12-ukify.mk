#
# SPDX-FileCopyrightText: The LineageOS Project
# SPDX-License-Identifier: Apache-2.0
#

ifeq ($(USES_DEVICE_MAINLINE_GENERIC),true)

OUT_CMDLINE_TXT := $(PRODUCT_OUT)/cmdline.txt
OUT_CMDLINE_EXTRA_TXT := $(PRODUCT_OUT)/cmdline-extra.txt

ifeq ($(TARGET_ARCH),arm64)
UKIFY_OUT_FILENAME := BOOTAA64.EFI
else ifeq ($(TARGET_ARCH),x86_64)
UKIFY_OUT_FILENAME := BOOTX64.EFI
endif
UKIFY_OUT := $(PRODUCT_OUT)/$(UKIFY_OUT_FILENAME)

UKIFY_DEPS := \
    $(PRODUCT_OUT)/kernel \
    $(PRODUCT_OUT)/ramdisk.img \
    $(OUT_CMDLINE_TXT)

$(OUT_CMDLINE_TXT):
	echo "$(BOARD_KERNEL_CMDLINE) " > $@
	if [ -f "$(OUT_CMDLINE_EXTRA_TXT)" ]; then \
		echo "WARNING: $(OUT_CMDLINE_EXTRA_TXT) does not exist yet. Extra parameters are required to boot up and you can put these on that file."; \
	else \
		cat $(OUT_CMDLINE_EXTRA_TXT) >> $@ \
	fi

$(UKIFY_OUT): $(OUT_CMDLINE_TXT)
	/usr/bin/ukify build --linux=$(PRODUCT_OUT)/kernel --initrd=$(PRODUCT_OUT)/ramdisk.img --cmdline=$(OUT_CMDLINE_TXT) --output=$@

.PHONY: ukify_build
ukify_build: $(UKIFY_OUT)

endif # USES_DEVICE_MAINLINE_GENERIC
