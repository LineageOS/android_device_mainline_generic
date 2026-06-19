/*
 * Copyright (C) 2018 The Android Open Source Project
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

#pragma once

#include <health-impl/Health.h>

namespace aidl::android::hardware::health {

using ::aidl::android::hardware::health::BatteryHealth;
using ::aidl::android::hardware::health::BatteryStatus;
using ::aidl::android::hardware::health::Health;
using ::aidl::android::hardware::health::HealthInfo;
using ::ndk::ScopedAStatus;

class CuttlefishHealth : public Health {
 public:
  // Inherit constructor.
  using Health::Health;
  virtual ~CuttlefishHealth() {}

  ScopedAStatus getChargeCounterUah(int32_t* out) override;
  ScopedAStatus getCurrentNowMicroamps(int32_t* out) override;
  ScopedAStatus getCurrentAverageMicroamps(int32_t* out) override;
  ScopedAStatus getCapacity(int32_t* out) override;
  ScopedAStatus getChargeStatus(BatteryStatus* out) override;
  ScopedAStatus getBatteryHealthData(BatteryHealthData* out) override;

 protected:
  void UpdateHealthInfo(HealthInfo* health_info) override;
};

}  // namespace aidl::android::hardware::health
