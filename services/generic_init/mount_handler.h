/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "uevent.h"

#include <string>
#include <vector>

namespace MountHandler {

void OnPreBlockDevices(void);

void OnBlockDeviceAdd(const android::init::Uevent& uevent, const std::string& devpath, const std::vector<std::string>& links);

bool CanQuitUeventd(bool print_log);

void OnPostBlockDevices(void);

}  // namespace
