/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include "mount_helpers.h"
#include "switch_root.h"

#include <android-base/logging.h>
#include <android-base/strings.h>
#include <fs_mgr.h>
#include <fs_mgr_overlayfs.h>
#include <fstab/fstab.h>
#include <linux/mount.h>
#include <sys/mount.h>
#include <sys/stat.h>

#include <filesystem>
#include <vector>

namespace android {
namespace fs_mgr {
struct OverlayfsCheckResult {
    bool supported;
    std::string mount_flags;
};
OverlayfsCheckResult CheckOverlayfs();
}  // namespace fs_mgr
}  // namespace android

namespace MountHelpers {

namespace fs = std::filesystem;

// From fs_mgr with edits
static bool mount_overlayfs_fstab_entry(const FstabEntry& entry) {
    const auto overlayfs_check_result = android::fs_mgr::CheckOverlayfs();
    if (!overlayfs_check_result.supported) {
        LOG(ERROR) << __FUNCTION__ << "(): kernel does not support overlayfs";
        return false;
    }

    if (!fs_mgr_create_canonical_mount_point(entry.mount_point)) {
        return false;
    }

    auto lowerdir = entry.lowerdir;
    if (entry.fs_mgr_flags.overlayfs_remove_missing_lowerdir) {
        bool removed_any = false;
        std::vector<std::string> lowerdirs;
        for (const auto& dir : android::base::Split(entry.lowerdir, ":")) {
            if (access(dir.c_str(), F_OK)) {
                PLOG(WARNING) << __FUNCTION__ << "(): remove missing lowerdir '" << dir << "'";
                removed_any = true;
            } else {
                lowerdirs.push_back(dir);
            }
        }
        if (removed_any) {
            lowerdir = android::base::Join(lowerdirs, ":");
        }
    }

    auto options = "lowerdir=" + lowerdir + overlayfs_check_result.mount_flags;
    for (const auto& option : android::base::Split(entry.fs_options, ",")) {
        if (android::base::StartsWith(option, "lowerdir=")) continue;
        options += "," + option;
    }

    // Use "overlay-" + entry.blk_device as the mount() source, so that adb-remout-test don't
    // confuse this with adb remount overlay, whose device name is "overlay".
    // Overlayfs is a pseudo filesystem, so the source device is a symbolic value and isn't used to
    // back the filesystem. However the device name would be shown in /proc/mounts.
    auto source = "overlay-" + entry.blk_device;
    auto report = "__mount(source=" + source + ",target=" + entry.mount_point + ",type=overlay," +
                  options + ")=";
    auto ret = mount(source.c_str(), entry.mount_point.c_str(), "overlay", entry.flags | MS_NOATIME,
                     options.c_str());
    if (ret) {
        PLOG(ERROR) << report << ret;
        return false;
    }
    LOG(INFO) << report << ret;
    return true;
}

static void CreateOverlayfsUpperdirAndWorkdirIfNotPresent(const FstabEntry& entry) {
    for (const auto& fs_option : android::base::Split(entry.fs_options, ",")) {
        auto equal_sign = fs_option.find('=');
        if (equal_sign == std::string::npos) continue;

        const auto param = fs_option.substr(0, equal_sign);
        const auto arg = fs_option.substr(equal_sign + 1);

        if (param != "upperdir" && param != "workdir") continue;
        if (TryAccessDir(arg)) continue;

        LOG(INFO) << "Creating " << param << " for overlayfs mount point "
                  << entry.mount_point << ": " << arg;

        // TODO: Bring mkdir_recursive() to util.cpp and use it
        if (mkdir(arg.c_str(), 0755)) {
            LOG(ERROR) << "Failed to create directory " << arg;
        }
    }
}

static bool GetRootEntry(FstabEntry* root_entry) {
    Fstab proc_mounts;
    if (!ReadFstabFromFile("/proc/mounts", &proc_mounts)) {
        LOG(ERROR) << "Could not read /proc/mounts and /system not in fstab, /system will not be "
                      "available for overlayfs";
        return false;
    }

    auto entry = std::find_if(proc_mounts.begin(), proc_mounts.end(), [](const auto& entry) {
        return entry.mount_point == "/" && entry.fs_type != "rootfs";
    });

    if (entry == proc_mounts.end()) {
        LOG(ERROR) << "Could not get mount point for '/' in /proc/mounts, /system will not be "
                      "available for overlayfs";
        return false;
    }

    *root_entry = std::move(*entry);

    return true;
}

bool TryAccessDir(const std::string& path) {
    std::error_code ec;
    fs::file_status s = fs::status(path, ec);
    return (!ec && fs::exists(s) && fs::is_directory(s));
}

bool TryAccessFile(const std::string& path) {
    std::error_code ec;
    fs::file_status s = fs::status(path, ec);
    return (!ec && fs::exists(s) && fs::is_regular_file(s));
}

bool MountPartition(Fstab& fstab_, const Fstab::iterator& begin,
                    bool erase_same_mounts, Fstab::iterator* end) {
    // Sets end to begin + 1, so we can just return on failure below.
    if (end) {
        *end = begin + 1;
    }

    if (!fs_mgr_create_canonical_mount_point(begin->mount_point)) {
        return false;
    }

    bool mounted = (fs_mgr_do_mount_one(*begin) == 0);

    // Try other mounts with the same mount point.
    Fstab::iterator current = begin + 1;
    for (; current != fstab_.end() && current->mount_point == begin->mount_point; current++) {
        if (!mounted) {
            // blk_device is already updated to /dev/dm-<N> by SetUpDmVerity() above.
            // Copy it from the begin iterator.
            current->blk_device = begin->blk_device;
            mounted = (fs_mgr_do_mount_one(*current) == 0);
        }
    }
    if (erase_same_mounts) {
        current = fstab_.erase(begin, current);
    }
    if (end) {
        *end = current;
    }
    return mounted;
}

bool MountPartitions(Fstab& fstab_) {
    if (!TrySwitchSystemAsRoot(fstab_)) return false;

    if (!SkipMountingPartitions(&fstab_, true /* verbose */)) return false;

    for (auto current = fstab_.begin(); current != fstab_.end();) {
        // We've already mounted /system above.
        if (current->mount_point == "/system") {
            ++current;
            continue;
        }

        // Handle overlayfs entries later.
        if (current->fs_type == "overlay") {
            ++current;
            continue;
        }

        // Skip raw partition entries such as boot, dtbo, etc.
        // Having emmc fstab entries allows us to probe current->vbmeta_partition
        // in InitDevices() when they are AVB chained partitions.
        if (current->fs_type == "emmc") {
            ++current;
            continue;
        }

        Fstab::iterator end;
        if (!MountPartition(fstab_, current, false /* erase_same_mounts */, &end)) {
            if (current->fs_mgr_flags.no_fail) {
                LOG(INFO) << "Failed to mount " << current->mount_point
                          << ", ignoring mount for no_fail partition";
            } else if (current->fs_mgr_flags.formattable) {
                LOG(INFO) << "Failed to mount " << current->mount_point
                          << ", ignoring mount for formattable partition";
            } else {
                PLOG(ERROR) << "Failed to mount " << current->mount_point;
                return false;
            }
        }
        current = end;
    }

    for (const auto& entry : fstab_) {
        if (entry.fs_type == "overlay") {
            CreateOverlayfsUpperdirAndWorkdirIfNotPresent(entry);
            mount_overlayfs_fstab_entry(entry);
        }
    }

    // If we don't see /system or / in the fstab, then we need to create an root entry for
    // overlayfs.
    if (!GetEntryForMountPoint(&fstab_, "/system") && !GetEntryForMountPoint(&fstab_, "/")) {
        FstabEntry root_entry;
        if (GetRootEntry(&root_entry)) {
            fstab_.emplace_back(std::move(root_entry));
        }
    }

    fs_mgr_overlayfs_mount_all(&fstab_);

    return true;
}

bool TrySwitchSystemAsRoot(Fstab& fstab_) {
    auto system_partition = std::find_if(fstab_.begin(), fstab_.end(), [](const auto& entry) {
        return entry.mount_point == "/system";
    });

    if (system_partition == fstab_.end()) return true;

    if (!MountPartition(fstab_, system_partition, false /* erase_same_mounts */)) {
        PLOG(ERROR) << "Failed to mount /system";
        return false;
    }

    android::init::SwitchRoot("/system");

    return true;
}

}  // namespace
