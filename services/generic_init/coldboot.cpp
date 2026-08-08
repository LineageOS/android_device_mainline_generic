/*
 * Copyright (C) 2025 The Android Open Source Project
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

#include "coldboot.h"
#include "coldboot_runner.h"

#include <android-base/chrono_utils.h>
#include <android-base/logging.h>
#include <android-base/properties.h>
#include <sys/stat.h>

namespace android {
namespace init {


void ColdBoot::RegenerateUevents() {
    uevent_listener_.RegenerateUevents([this](const Uevent& uevent) {
        uevent_queue_.emplace_back(uevent);
        return ListenerAction::kContinue;
    });
}

void ColdBoot::Run(bool use_parallel) {
    android::base::Timer cold_boot_timer;

    RegenerateUevents();

    std::unique_ptr<ColdbootRunner> runner;

    if (use_parallel) {
        unsigned int parallelism = std::thread::hardware_concurrency() ?: 4;
        if (false) {
            runner = std::make_unique<ColdbootRunnerThreadPool>(
                    parallelism, uevent_queue_, uevent_handlers_);
        } else {
            runner = std::make_unique<ColdbootRunnerSubprocess>(
                    parallelism, uevent_queue_, uevent_handlers_);
        }
    } else {
        runner = std::make_unique<ColdbootRunnerNoParallel>(
                uevent_queue_, uevent_handlers_);
    }

    runner->StartInBackground();

    runner->Wait();

    LOG(INFO) << "Coldboot took " << cold_boot_timer.duration().count() / 1000.0f << " seconds";
}

}  // namespace init
}  // namespace android
