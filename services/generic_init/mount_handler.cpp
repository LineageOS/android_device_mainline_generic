/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mount_handler.h"
#include "mount_helpers.h"

#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/macros.h>
#include <android-base/strings.h>
#include <android-base/unique_fd.h>
#include <fs_mgr.h>
#include <fstab/fstab.h>
#include <libdm/loop_control.h>
#include <linux/loop.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <list>
#include <map>
#include <memory>
#include <system_error>
#include <unordered_map>
#include <utility>
#include <vector>

extern bool fs_mgr_get_boot_config(const std::string& key, std::string* out_val);

namespace MountHandler {

namespace fs = std::filesystem;
using namespace ::android::fs_mgr;
using android::base::StartsWithIgnoreCase;
using android::base::EndsWithIgnoreCase;
using android::base::Join;
using android::base::ReadFileToString;
using android::base::StartsWith;
using android::base::unique_fd;
using android::dm::LoopControl;
using std::chrono_literals::operator""s;

namespace {

std::string config_android_dir;

struct BlockDeviceInfo {
    // uevent fields
    std::string devname;
    std::string devpath;
    std::string partname;
    std::string partuuid;

    // uevent handler fields
    std::vector<std::string> links;

    // our fields
    bool is_partition;
    bool rw;
    std::string fs_type;

    std::string android_system_partition;
    std::vector<std::string> android_system_partition_have_subdirs;

    std::string have_android_dir;
    std::string have_firmware_dir;
    std::vector<std::string> android_dir_have_images;
    std::vector<std::string> android_dir_have_subdirs;
};

// devname, BlockDeviceInfo
using BlockDevices = std::unordered_map<std::string, std::shared_ptr<BlockDeviceInfo>>;
std::shared_ptr<BlockDevices> block_devices;

bool need_android_dir = false;
bool need_firmware_dir = false;
bool need_mount_cache = false;

// TODO: `discard` flag for RW filesystems
constexpr char kMountOpts[] = "";

std::shared_ptr<BlockDeviceInfo> block_device_for_android_dir = nullptr;
std::shared_ptr<BlockDeviceInfo> block_device_for_firmware_dir = nullptr;

const std::list<std::string> fs_types_for_system_partitions = {
    "ext4", "erofs", "squashfs"
};

const std::list<std::string> fs_types_for_userdata_partitions = {
    "ext4", "f2fs"
};

const std::list<std::string> android_system_partitions = {
    "product", "system", "system_dlkm", "system_ext",
    "odm", "odm_dlkm", "vendor", "vendor_dlkm"
};

const std::list<std::string> android_userdata_partitions = {
    "cache", "data", "metadata"
};

const std::list<std::string> possible_subdirs_in_system = {
    "product", "system_dlkm", "system_ext", "vendor"
};

const std::list<std::string> possible_subdirs_in_vendor = {
    "odm", "odm_dlkm", "vendor_dlkm"
};

std::map<std::string, std::shared_ptr<BlockDeviceInfo>> android_system_part_to_bdev_map = {
    {"system", nullptr},
    {"vendor", nullptr},

    {"product", nullptr},
    {"system_dlkm", nullptr},
    {"system_ext", nullptr},

    {"odm", nullptr},
    {"odm_dlkm", nullptr},
    {"vendor_dlkm", nullptr},
};

std::map<std::string, std::shared_ptr<BlockDeviceInfo>> android_userdata_part_to_bdev_map = {
    {"cache", nullptr},
    {"metadata", nullptr},
    {"userdata", nullptr},
};

constexpr char kAndroidDirParam[] = "android_dir";
constexpr char kMountFirmwareParam[] = "mount_firmware";
constexpr char kMountSystemParam[] = "mount_system";
constexpr char kMountUserdataParam[] = "mount_userdata";

enum class MountSystemParam {
    STANDARD_PARTITIONS_WITH_PARTNAME = 0,
    ANY_BLOCK_DEVICES_AS_PARTITION = 1,
    IMAGES = 2,
    IMAGES_COPY_TO_RAM = 3,
};

enum class MountUserdataParam {
    STANDARD_PARTITIONS_WITH_PARTNAME = 0,
    IMAGES = 1,
    BIND_MOUNT_DIR = 2,
    TMPFS = 3,
};

const std::unordered_map<std::string, MountSystemParam> kStringToMountSystemParamMap = {
    {"std_parts", MountSystemParam::STANDARD_PARTITIONS_WITH_PARTNAME},
    {"blk_devices", MountSystemParam::ANY_BLOCK_DEVICES_AS_PARTITION},
    {"imgs", MountSystemParam::IMAGES},
    {"imgs_ram", MountSystemParam::IMAGES_COPY_TO_RAM},
};

const std::unordered_map<std::string, MountUserdataParam> kStringToMountUserdataParamMap = {
    {"std_parts", MountUserdataParam::STANDARD_PARTITIONS_WITH_PARTNAME},
    {"imgs", MountUserdataParam::IMAGES},
    {"bind_mount_dir", MountUserdataParam::BIND_MOUNT_DIR},
    {"tmpfs", MountUserdataParam::TMPFS},
};

bool param_mount_firmware = false;
MountSystemParam param_mount_system = MountSystemParam::STANDARD_PARTITIONS_WITH_PARTNAME;
MountUserdataParam param_mount_userdata = MountUserdataParam::STANDARD_PARTITIONS_WITH_PARTNAME;

const std::string kAndroidMountTarget = "/mnt/android";
const std::string kFirmwareMountTarget = "/mnt/firmware";
const std::string kTmpfsImgDir = "/mnt/img";
const std::string kTryMountTarget = "/mnt/try";
const std::string kImageSuffix = ".img";

Fstab fstab_userdata_on_tmpfs = {
    {
        .blk_device = "cache",
        .mount_point = "/cache",
        .fs_type = "tmpfs",
        .fs_mgr_flags = {
            .no_fail = true,
            .first_stage_mount = true,
        }
    },
    {
        .blk_device = "data",
        .mount_point = "/data",
        .fs_type = "tmpfs",
        .fs_mgr_flags = {
            .first_stage_mount = true,
        }
    },
    {
        .blk_device = "metadata",
        .mount_point = "/metadata",
        .fs_type = "tmpfs",
        .fs_mgr_flags = {
            .no_fail = true,
            .first_stage_mount = true,
        }
    }
};

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

void ParseConfig(void) {
    bool ret;
    std::string tmp;

    fs_mgr_get_boot_config(kAndroidDirParam, &config_android_dir);

    ret = fs_mgr_get_boot_config(kMountFirmwareParam, &tmp);
    if (ret) {
        if (tmp == "true") {
            need_firmware_dir = true;
            param_mount_firmware = true;
        }
    }

    ret = fs_mgr_get_boot_config(kMountSystemParam, &tmp);
    if (ret) {
        if (kStringToMountSystemParamMap.contains(tmp)) {
            param_mount_system = kStringToMountSystemParamMap.at(tmp);
        } else {
            LOG(ERROR) << "Parameter " << kMountSystemParam << " value is invalid";
        }
    } else {
        LOG(INFO) << "Parameter " << kMountSystemParam << " is unset";
    }

    ret = fs_mgr_get_boot_config(kMountUserdataParam, &tmp);
    if (ret) {
        if (kStringToMountUserdataParamMap.contains(tmp)) {
            param_mount_userdata = kStringToMountUserdataParamMap.at(tmp);
        } else {
            LOG(ERROR) << "Parameter " << kMountUserdataParam << " value is invalid";
        }
    } else {
        LOG(INFO) << "Parameter " << kMountUserdataParam << " is unset";
    }

    if (param_mount_system == MountSystemParam::IMAGES ||
        param_mount_system == MountSystemParam::IMAGES_COPY_TO_RAM ||
        param_mount_userdata == MountUserdataParam::IMAGES ||
        param_mount_userdata == MountUserdataParam::BIND_MOUNT_DIR) {
        need_android_dir = true;
    }
}

std::string DetermineAndroidPartitionFromBuildProp(const std::string& path) {
    std::string buf;
    if (!ReadFileToString(path, &buf)) {
        LOG(ERROR) << "Failed to read build.prop " << path;
        return "";
    }
    for (const auto& part : android_system_partitions) {
        std::string prop = "ro." + part + ".build.id=";
        if (buf.find(prop) != std::string::npos) return part;
    }
    LOG(ERROR) << "Failed to determine android partition from build.prop " << path;
    return "";
}

std::string TryMountAndReturnFsType(const std::string& devpath, unsigned long mountflags) {
    int ret, save_errno;
    const std::list<std::string> try_mount_fs_types = {
        "ext4", "f2fs", "erofs", "squashfs", "vfat", "ntfs", "iso9660", "udf"
    };
    for (const auto& try_fs_type : try_mount_fs_types) {
        errno = 0;
        ret = mount(devpath.c_str(), kTryMountTarget.c_str(), try_fs_type.c_str(), mountflags, kMountOpts);
        save_errno = errno;
        if (ret && (save_errno == EINVAL || save_errno == ENODEV)) continue;
        if (ret) {
            PLOG(ERROR) << "Unable to try to mount " << devpath;
            return "";
        } else {
            return try_fs_type;
        }
    }
    if (ret) {
        LOG(ERROR) << "No known filesystem detected for " << devpath;
        return "";
    }
    return "";
}

void ParseBlockDevice(BlockDeviceInfo* info) {
    unsigned long mountflags = MS_RDONLY;
    std::string tmp_path;
    std::string& devpath = info->devpath;

    // Mount as RO, and fill fs_type
    info->fs_type = TryMountAndReturnFsType(devpath, mountflags);
    if (info->fs_type.empty()) return;

    // Check the contents
    if (TryAccessFile(kTryMountTarget + "/system/build.prop")) {
        info->android_system_partition = "system";
    } else {
        const std::list<std::string> possible_build_props = {
            "build.prop", "etc/build.prop"
        };
        for (const auto& build_prop : possible_build_props) {
            std::string build_prop_path = kTryMountTarget + "/" + build_prop;
            if (TryAccessFile(build_prop_path)) {
                info->android_system_partition = DetermineAndroidPartitionFromBuildProp(build_prop_path);
            }
        }
    }

    if (!info->android_system_partition.empty()) {
        LOG(INFO) << "Block device " << devpath << " detected as system partition " << info->android_system_partition;
        if (info->android_system_partition == "system") {
            for (const auto& subdir : possible_subdirs_in_system) {
                tmp_path = kTryMountTarget + "/system/" + subdir;
                if (TryAccessDir(tmp_path)) {
                    info->android_system_partition_have_subdirs.push_back(subdir);
                }
            }
            for (const auto& subdir : possible_subdirs_in_vendor) {
                tmp_path = kTryMountTarget + "/system/vendor/" + subdir;
                if (TryAccessDir(tmp_path)) {
                    info->android_system_partition_have_subdirs.push_back(subdir);
                }
            }
            if (TryAccessDir(kTryMountTarget + "/cache")) {
                LOG(INFO) << "/cache is a directory, will need to be mounted";
                need_mount_cache = true;
            }
        } else if (info->android_system_partition == "vendor") {
            for (const auto& subdir : possible_subdirs_in_vendor) {
                tmp_path = kTryMountTarget + "/" + subdir;
                if (TryAccessDir(tmp_path)) {
                    info->android_system_partition_have_subdirs.push_back(subdir);
                }
            }
        }
        if (!info->android_system_partition_have_subdirs.empty()) {
            LOG(INFO) << "Block device " << devpath << " have subdirs: " << Join(info->android_system_partition_have_subdirs, ", ");
        }
    }

    if (info->android_system_partition.empty()) {
        std::list<std::string> possible_android_dirs = {
            config_android_dir, "android", "boot/android"
        };
        for (const auto& android_dir : possible_android_dirs) {
            if (android_dir.empty()) continue;
            if (TryAccessDir(kTryMountTarget + "/" + android_dir)) {
                LOG(INFO) << "Block device " << devpath << " have android directory: " << android_dir;
                info->have_android_dir = android_dir;
                break;
            }
        }

        if (!info->have_android_dir.empty()) {
            for (const auto& entry : fs::directory_iterator(kTryMountTarget + "/" + info->have_android_dir)) {
                std::string name = entry.path().filename();
                if (entry.is_regular_file() && EndsWithIgnoreCase(name, kImageSuffix)) {
                    if (StartsWithIgnoreCase(name, "initrd") || StartsWithIgnoreCase(name, "ramdisk")) continue;
                    info->android_dir_have_images.push_back(name);
                } else if (entry.is_directory() && name != "firmware") {
                    info->android_dir_have_subdirs.push_back(name);
                }
            }
        }

        std::list<std::string> possible_firmware_dirs = {
            info->have_android_dir.empty() ? "" : info->have_android_dir + "/firmware",
            "lib/firmware", "linux-firmware", "firmware"
        };
        for (const auto& fw_dir : possible_firmware_dirs) {
            if (fw_dir.empty()) continue;
            if (TryAccessDir(kTryMountTarget + "/" + fw_dir)) {
                LOG(INFO) << "Block device " << devpath << " have firmware directory: " << fw_dir;
                info->have_firmware_dir = fw_dir;
                break;
            }
        }
    }

    // Try remount as RW
    mountflags &= ~MS_RDONLY;
    mountflags |= MS_REMOUNT;
    info->rw = mount(NULL, kTryMountTarget.c_str(), NULL, mountflags, NULL) == 0;
    LOG(INFO) << "Block device " << devpath << " can be mounted as " << (info->rw ? "read-write" : "read-only");

    // Cleanup and exit
    if (umount(kTryMountTarget.c_str()) == -1) {
        PLOG(FATAL) << "Failed to umount " << kTryMountTarget << " from block device " << devpath;
    }
}

void UpdateMountInfo(std::shared_ptr<BlockDeviceInfo> bdinfo) {
    if (param_mount_system == MountSystemParam::STANDARD_PARTITIONS_WITH_PARTNAME &&
        !bdinfo->partname.empty() &&
        (std::find(android_system_partitions.begin(), android_system_partitions.end(),
        bdinfo->partname) != android_system_partitions.end())) {
        LOG(INFO) << "Set block device for system partition " << bdinfo->partname
                  << " to " << bdinfo->devname;
        android_system_part_to_bdev_map[bdinfo->partname] = bdinfo;
        goto process_android_system_partition_subdirs;
    }

    if (param_mount_system == MountSystemParam::ANY_BLOCK_DEVICES_AS_PARTITION &&
        !bdinfo->android_system_partition.empty() &&
        (std::find(android_system_partitions.begin(), android_system_partitions.end(),
        bdinfo->android_system_partition) != android_system_partitions.end())) {
        LOG(INFO) << "Set block device for system partition "
                  << bdinfo->android_system_partition << " to " << bdinfo->devname;
        android_system_part_to_bdev_map[bdinfo->android_system_partition] = bdinfo;
        goto process_android_system_partition_subdirs;
    }

    if (param_mount_userdata == MountUserdataParam::STANDARD_PARTITIONS_WITH_PARTNAME &&
        !bdinfo->partname.empty() &&
        (std::find(android_userdata_partitions.begin(), android_userdata_partitions.end(),
        bdinfo->partname) != android_userdata_partitions.end())) {
        LOG(INFO) << "Set block device for userdata partition " << bdinfo->partname
                  << " to " << bdinfo->devname;
        android_userdata_part_to_bdev_map[bdinfo->partname] = bdinfo;
        return;
    }

    if (need_android_dir && !bdinfo->have_android_dir.empty()) {
        LOG(INFO) << "Set block device with android dir to " << bdinfo->devname;
        block_device_for_android_dir = bdinfo;
    }
    if (need_firmware_dir && !bdinfo->have_firmware_dir.empty()) {
        LOG(INFO) << "Set block device with firmware dir to " << bdinfo->devname;
        block_device_for_firmware_dir = bdinfo;
    }

    return;

process_android_system_partition_subdirs:
    for (const auto& subdir : bdinfo->android_system_partition_have_subdirs) {
        LOG(INFO) << "Set block device for system partition subdir " << subdir
                  << " to " << bdinfo->devname;
        android_system_part_to_bdev_map[subdir] = bdinfo;
    }
    return;
}

std::string SetupLoopDevice(const std::string& image, bool rw) {
    unique_fd image_fd(TEMP_FAILURE_RETRY(open(image.c_str(), (rw ? O_RDWR : O_RDONLY) | O_CLOEXEC, (rw ? 0600 : 0400))));
    if (image_fd.get() == -1) {
        PLOG(ERROR) << "Cannot open image path: " << image;
        return "";
    }

    LoopControl loop_control;
    std::string loop_device;
    if (!loop_control.Attach(image_fd.get(), 5s, &loop_device)) {
        return "";
    }

    unique_fd loop_fd(TEMP_FAILURE_RETRY(open(loop_device.c_str(), O_RDWR | O_CLOEXEC)));
    if (loop_fd.get() == -1) {
        PLOG(ERROR) << "Cannot open " << loop_device;
        return "";
    }

    struct loop_info64 info = {
        .lo_flags = 0
    };
    if (!rw) info.lo_flags |= LO_FLAGS_READ_ONLY;
    if (ioctl(loop_fd.get(), LOOP_SET_STATUS64, &info)) {
        PLOG(ERROR) << "Failed set loop flags for " << loop_device;
        return "";
    }

    LoopControl::EnableDirectIo(loop_fd.get());

    return loop_device;
}

}  // namespace

void OnPreBlockDevices(void) {
    mkdir(kAndroidMountTarget.c_str(), 0755);
    mkdir(kFirmwareMountTarget.c_str(), 0755);
    mkdir(kTmpfsImgDir.c_str(), 0755);
    mkdir(kTryMountTarget.c_str(), 0755);
    block_devices = std::make_shared<BlockDevices>();
    ParseConfig();
}

void OnBlockDeviceAdd(const android::init::Uevent& uevent, const std::string& devpath, const std::vector<std::string>& links) {
    if (block_devices->contains(devpath)) {
        LOG(ERROR) << "Block device " << devpath << " has already been parsed";
        return;
    }

    static const std::list<std::string> kIgnoredDevnamePrefixs = {
        "dm-", "loop", "ram", "zram"
    };
    for (const auto& prefix : kIgnoredDevnamePrefixs) {
        if (StartsWith(uevent.device_name, prefix)) {
            return;
        }
    }

    BlockDeviceInfo info;
    info.devname = uevent.device_name;
    info.devpath = devpath;
    info.partname = uevent.partition_name;
    info.partuuid = uevent.partition_uuid;
    info.links = links;
    info.is_partition = std::isdigit(static_cast<unsigned char>(info.devname.back()));
    ParseBlockDevice(&info);
    block_devices->insert({devpath, std::make_shared<BlockDeviceInfo>(info)});
    UpdateMountInfo(block_devices->at(devpath));
}

// TODO: Handle block device removal?

bool CanQuitUeventd(bool print_log) {
    bool ret = true;
    if (param_mount_system == MountSystemParam::STANDARD_PARTITIONS_WITH_PARTNAME ||
        param_mount_system == MountSystemParam::ANY_BLOCK_DEVICES_AS_PARTITION) {
        for (const auto& [part, bdev] : android_system_part_to_bdev_map) {
            if (bdev == nullptr) {
                if (print_log) {
                    LOG(INFO) << __FUNCTION__ << ": Missing block device for system partition " << part;
                    ret = false;
                } else {
                    return false;
                }
            }
        }
    }
    if (param_mount_userdata == MountUserdataParam::STANDARD_PARTITIONS_WITH_PARTNAME) {
        for (const auto& [part, bdev] : android_userdata_part_to_bdev_map) {
            if (part == "cache" && !need_mount_cache) continue;
            if (bdev == nullptr) {
                if (print_log) {
                    LOG(INFO) << __FUNCTION__ << ": Missing block device for userdata partition " << part;
                    ret = false;
                } else {
                    return false;
                }
            }
        }
    }
    if (need_android_dir && block_device_for_android_dir == nullptr) {
        if (print_log) {
            LOG(INFO) << __FUNCTION__ << ": Missing block device for android dir";
            ret = false;
        } else {
            return false;
        }
    }
    if (need_firmware_dir && block_device_for_firmware_dir == nullptr) {
        if (print_log) {
            LOG(INFO) << __FUNCTION__ << ": Missing block device for firmware dir";
            ret = false;
        } else {
            return false;
        }
    }
    return ret;
}

void OnPostBlockDevices(void) {
    Fstab fstab;
    int ret;
    std::error_code ec;
    std::string android_dir_path, firmware_dir_path;

    if (need_android_dir) {
        std::shared_ptr<BlockDeviceInfo> bdinfo = block_device_for_android_dir;
        ret = mount(bdinfo->devpath.c_str(), kAndroidMountTarget.c_str(), bdinfo->fs_type.c_str(), bdinfo->rw ? 0 : MS_RDONLY, kMountOpts);
        if (ret) {
            PLOG(FATAL) << "Unable to mount block device for android dir";
        }
        android_dir_path = kAndroidMountTarget + "/" + bdinfo->have_android_dir;
    }

    if (param_mount_system == MountSystemParam::STANDARD_PARTITIONS_WITH_PARTNAME ||
        param_mount_system == MountSystemParam::ANY_BLOCK_DEVICES_AS_PARTITION) {
        for (const auto& [partition, bdinfo] : android_system_part_to_bdev_map) {
            if (partition != bdinfo->android_system_partition) continue;
            FstabEntry entry = {
                .blk_device = bdinfo->devpath,
                .mount_point = "/" + bdinfo->android_system_partition,
                .fs_mgr_flags = {
                    .first_stage_mount = true
                }
            };
            if (!bdinfo->rw) entry.flags |= MS_RDONLY;
            for (const auto& fs_type : fs_types_for_system_partitions) {
                entry.fs_type = fs_type;
                fstab.push_back(entry);
            }
        }
    }

    if (param_mount_userdata == MountUserdataParam::STANDARD_PARTITIONS_WITH_PARTNAME) {
        for (const auto& [partition, bdinfo] : android_userdata_part_to_bdev_map) {
            if (partition == "cache" && !need_mount_cache) continue;
            FstabEntry entry = {
                .blk_device = bdinfo->devpath,
                .fs_mgr_flags = {
                    .first_stage_mount = true
                }
            };
            entry.mount_point = bdinfo->partname == "userdata" ? "/data" : "/" + bdinfo->partname;
            entry.fs_mgr_flags.no_fail = entry.mount_point != "/data";
            for (const auto& fs_type : fs_types_for_userdata_partitions) {
                entry.fs_type = fs_type;
                fstab.push_back(entry);
            }
        }
    }

    if (param_mount_system == MountSystemParam::IMAGES ||
        param_mount_system == MountSystemParam::IMAGES_COPY_TO_RAM ||
        param_mount_userdata == MountUserdataParam::IMAGES) {
        std::shared_ptr<BlockDeviceInfo> bdinfo = block_device_for_android_dir;
        std::list<std::pair<std::string, std::string>> partition_img_list;

        for (const auto& image : bdinfo->android_dir_have_images) {
            std::string image_partition = image.substr(0, image.find_last_of('.'));
            std::string image_src = android_dir_path + "/" + image;

            // Ignore images that matches with no partition
            if (std::find(android_system_partitions.begin(),
                           android_system_partitions.end(),
                           image_partition)
                           == android_system_partitions.end() &&
                std::find(android_userdata_partitions.begin(),
                           android_userdata_partitions.end(),
                           image_partition)
                           == android_userdata_partitions.end()) {
                continue;
            }

            // Ignore system images while not mounting system from images
            if (param_mount_system != MountSystemParam::IMAGES &&
                param_mount_system != MountSystemParam::IMAGES_COPY_TO_RAM &&
                (std::find(android_system_partitions.begin(),
                           android_system_partitions.end(),
                           image_partition)
                           != android_system_partitions.end())) {
                continue;
            }

            // Ignore userdata images while not mounting userdata from images
            if (param_mount_userdata != MountUserdataParam::IMAGES &&
                (std::find(android_userdata_partitions.begin(),
                           android_userdata_partitions.end(),
                           image_partition)
                           != android_userdata_partitions.end())) {
                continue;
            }

            if (param_mount_system == MountSystemParam::IMAGES_COPY_TO_RAM) {
                LOG(INFO) << "Copying image " << image << " to RAM";
                std::string image_dst = kTmpfsImgDir + "/" + image;
                fs::copy(image_src, image_dst, ec);
                if (ec) LOG(FATAL) << "Failed to copy image " << image << " to RAM";
                image_src = image_dst;
            }

            partition_img_list.push_back({image_partition, image_src});
        }

        for (const auto& [part, img] : partition_img_list) {
            if (part == "cache" && !need_mount_cache) continue;

            FstabEntry entry = {
                .fs_mgr_flags = {
                    .first_stage_mount = true
                }
            };
            if (!bdinfo->rw) entry.flags |= MS_RDONLY;

            entry.blk_device = SetupLoopDevice(img, bdinfo->rw);
            if (entry.blk_device.empty()) {
                LOG(FATAL) << "Failed to setup loop device for image " << img;
            }

            entry.mount_point = part == "userdata" ? "/data" : "/" + part;

            for (const auto& fs_type : fs_types_for_system_partitions) {
                entry.fs_type = fs_type;
                fstab.push_back(entry);
            }
        }
    }

    if (param_mount_userdata == MountUserdataParam::BIND_MOUNT_DIR) {
        std::shared_ptr<BlockDeviceInfo> bdinfo = block_device_for_android_dir;
        for (const auto& subdir : bdinfo->android_dir_have_subdirs) {
            if (std::find(android_userdata_partitions.begin(),
                          android_userdata_partitions.end(),
                          subdir)
                          == android_userdata_partitions.end()) {
                continue;
            }
            if (subdir == "cache" && !need_mount_cache) continue;
            FstabEntry entry = {
                .blk_device = android_dir_path + "/" + subdir,
                .fs_type = "none",
                .flags = MS_BIND,
                .fs_mgr_flags = {
                    .first_stage_mount = true
                }
            };
            entry.mount_point = subdir == "userdata" ? "/data" : "/" + subdir;
            fstab.push_back(std::move(entry));
        }
    }

    if (param_mount_userdata == MountUserdataParam::TMPFS) {
        for (const auto& entry : fstab_userdata_on_tmpfs) {
            fstab.push_back(std::move(entry));
        }
    }

    if (need_firmware_dir) {
        std::shared_ptr<BlockDeviceInfo> bdinfo = block_device_for_firmware_dir;
        if (bdinfo == block_device_for_android_dir) {
            firmware_dir_path = kAndroidMountTarget + "/" + bdinfo->have_firmware_dir;
        } else {
            ret = mount(bdinfo->devpath.c_str(), kFirmwareMountTarget.c_str(), bdinfo->fs_type.c_str(), MS_RDONLY, kMountOpts);
            if (ret) {
                PLOG(FATAL) << "Unable to mount block device for firmware dir";
            }
            firmware_dir_path = kFirmwareMountTarget + "/" + bdinfo->have_firmware_dir;
        }

        FstabEntry entry = {
            .blk_device = firmware_dir_path,
            .mount_point = "/vendor/firmware",
            .fs_type = "none",
            .flags = MS_BIND,
            .fs_mgr_flags = {
                .no_fail = true,
                .first_stage_mount = true
            }
        };
        fstab.push_back(std::move(entry));
    }

    // Unmount block device for android dir if it's no longer used
    // Allowing live boot users to eject the boot media afterwards
    if (umount(kAndroidMountTarget.c_str()) == 0) {
        PLOG(INFO) << "umount " << kAndroidMountTarget << " successfully";
    }

    for (const auto& entry : fstab) {
        LOG(INFO) << "Fstab entry: blk_device=" << entry.blk_device << " mount_point=" << entry.mount_point << " fs_type=" << entry.fs_type;
    }

    if (!MountHelpers::MountPartitions(fstab)) {
        LOG(FATAL) << "Failed to mount partitions";
    }

    fstab.clear();
}

}  // namespace
