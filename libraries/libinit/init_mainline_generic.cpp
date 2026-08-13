/*
 * Copyright (C) 2026 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "vendor_init.h"

#include <libinit_mainline_common.h>
#include <libinit_utils.h>

#include <unistd.h>

void enable_memfd_if_ashmem_is_absent(void) {
    // ashmem driver can only be built-in in the kernel
    if (access("/dev/ashmem", F_OK) != 0) {
        property_override("sys.use_memfd", "true");
    }
}

void vendor_process_bootenv(void) {
    vendor_process_bootenv_mainline_common();
}

void vendor_load_properties() {
    vendor_load_properties_mainline_common();
    enable_memfd_if_ashmem_is_absent();
}
