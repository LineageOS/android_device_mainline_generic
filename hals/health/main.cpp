/*
 * Copyright (C) 2021 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android/binder_interface_utils.h>
#include <health-impl/Health.h>
#include <health/utils.h>

#include "CuttlefishHealth.h"

#include <filesystem>

#ifndef CHARGER_FORCE_NO_UI
#define CHARGER_FORCE_NO_UI 0
#endif

#if !CHARGER_FORCE_NO_UI
#include <health-impl/ChargerUtils.h>
#endif

namespace fs = std::filesystem;

using android::base::ReadFileToString;

using aidl::android::hardware::health::CuttlefishHealth;
using aidl::android::hardware::health::HalHealthLoop;
using aidl::android::hardware::health::Health;

#if !CHARGER_FORCE_NO_UI
using aidl::android::hardware::health::charger::ChargerCallback;
using aidl::android::hardware::health::charger::ChargerModeMain;
#endif

static constexpr const char* gInstanceName = "default";
static constexpr std::string_view gChargerArg{"--charger"};

bool isBatteryDevicePresent(void) {
    for (const auto& entry : fs::directory_iterator("/sys/class/power_supply")) {
        std::string name = entry.path().filename();
        if (!entry.is_directory() || name == "." || name == "..") continue;

        std::string type;
        if (!ReadFileToString(entry.path().string() + "/type", &type)) continue;

        type.pop_back();
        if (type == "Battery") return true;
    }
    return false;
}

int main(int argc, char** argv) {
#ifdef __ANDROID_RECOVERY__
    android::base::InitLogging(argv, android::base::KernelLogger);
#endif

    // make a default health service
    auto config = std::make_unique<healthd_config>();
    ::android::hardware::health::InitHealthdConfig(config.get());

    std::shared_ptr<Health> binder;
    if (isBatteryDevicePresent()) {
        LOG(INFO) << "Battery is present, use default health implementation";
        binder = ndk::SharedRefBase::make<Health>(gInstanceName, std::move(config));
    } else {
        LOG(INFO) << "Battery is absent, use cuttlefish health implementation";
        binder = ndk::SharedRefBase::make<CuttlefishHealth>(gInstanceName, std::move(config));
    }

    if (argc >= 2 && argv[1] == gChargerArg) {
#if !CHARGER_FORCE_NO_UI
        // If charger shouldn't have UI for your device, simply drop the line below
        // for your service implementation. This corresponds to
        // ro.charger.no_ui=true
        return ChargerModeMain(binder, std::make_shared<ChargerCallback>(binder));
#endif

        LOG(INFO) << "Starting charger mode without UI.";
    } else {
        LOG(INFO) << "Starting health HAL.";
    }

    auto hal_health_loop = std::make_shared<HalHealthLoop>(binder, binder);
    return hal_health_loop->StartLoop();
}
