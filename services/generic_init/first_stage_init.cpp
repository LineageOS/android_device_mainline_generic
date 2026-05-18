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

#include "first_stage_init.h"
#include "mount_handler.h"

#include <dirent.h>
#include <fcntl.h>
#include <paths.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <chrono>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <android-base/chrono_utils.h>
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/stringify.h>
#include <android-base/stringprintf.h>
#include <android-base/strings.h>
#include <modprobe/modprobe.h>
#include <private/android_filesystem_config.h>

#include "debug_ramdisk.h"
#include "first_stage_console.h"
#include "reboot_utils.h"
#include "second_stage_resources.h"
#include "switch_root.h"
#include "ueventd.h"
#include "ueventd_parser.h"

using android::base::boot_clock;

using namespace std::literals;

namespace fs = std::filesystem;

namespace android {
namespace init {

namespace {

enum class BootMode {
    NORMAL_MODE,
    RECOVERY_MODE,
    CHARGER_MODE,
};

void FreeRamdisk(DIR* dir, dev_t dev) {
    int dfd = dirfd(dir);

    dirent* de = nullptr;
    while ((de = readdir(dir)) != nullptr) {
        if (de->d_name == "."s || de->d_name == ".."s) {
            continue;
        }

        bool is_dir = false;

        if (de->d_type == DT_DIR || de->d_type == DT_UNKNOWN) {
            struct stat info {};
            if (fstatat(dfd, de->d_name, &info, AT_SYMLINK_NOFOLLOW) != 0) {
                continue;
            }

            if (info.st_dev != dev) {
                continue;
            }

            if (S_ISDIR(info.st_mode)) {
                is_dir = true;
                auto fd = openat(dfd, de->d_name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
                if (fd >= 0) {
                    auto subdir =
                            std::unique_ptr<DIR, decltype(&closedir)>{fdopendir(fd), closedir};
                    if (subdir) {
                        FreeRamdisk(subdir.get(), dev);
                    } else {
                        close(fd);
                    }
                }
            }
        }
        unlinkat(dfd, de->d_name, is_dir ? AT_REMOVEDIR : 0);
    }
}

bool ForceNormalBoot(const std::string& cmdline, const std::string& bootconfig) {
    return bootconfig.find("androidboot.force_normal_boot = \"1\"") != std::string::npos ||
           cmdline.find("androidboot.force_normal_boot=1") != std::string::npos;
}

/*
static void Copy(const char* src, const char* dst) {
    if (link(src, dst) == 0) {
        LOG(INFO) << "hard linking " << src << " to " << dst << " succeeded";
        return;
    }
    PLOG(FATAL) << "hard linking " << src << " to " << dst << " failed";
}
*/

std::string GetPageSizeSuffix() {
    static const size_t page_size = sysconf(_SC_PAGE_SIZE);
    if (page_size <= 4096) {
        return "";
    }
    return android::base::StringPrintf("_%zuk", page_size / 1024);
}

constexpr bool EndsWith(const std::string_view str, const std::string_view suffix) {
    return str.size() >= suffix.size() &&
           0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

constexpr std::string_view GetPageSizeSuffix(std::string_view dirname) {
    if (EndsWith(dirname, "_16k")) {
        return "_16k";
    }
    if (EndsWith(dirname, "_64k")) {
        return "_64k";
    }
    return "";
}

// From util.cpp

static void InitAborter(const char* abort_message) {
    InitFatalReboot(SIGABRT);
}

void InitKernelLogging(char** argv) {
    SetFatalRebootTarget();
    android::base::InitLogging(argv, &android::base::KernelLogger, InitAborter);
}

bool IsRecoveryMode() {
    return access("/system/bin/recovery", F_OK) == 0;
}

// The kernel opens /dev/console and uses that fd for stdin/stdout/stderr if there is a serial
// console enabled and no initramfs, otherwise it does not provide any fds for stdin/stdout/stderr.
// SetStdioToDevNull() is used to close these existing fds if they exist and replace them with
// /dev/null regardless.
//
// In the case that these fds are provided by the kernel, the exec of second stage init causes an
// SELinux denial as it does not have access to /dev/console.  In the case that they are not
// provided, exec of any further process is potentially dangerous as the first fd's opened by that
// process will take the stdin/stdout/stderr fileno's, which can cause issues if printf(), etc is
// then used by that process.
//
// Lastly, simply calling SetStdioToDevNull() in first stage init is not enough, since first
// stage init still runs in kernel context, future child processes will not have permissions to
// access any fds that it opens, including the one opened below for /dev/null.  Therefore,
// SetStdioToDevNull() must be called again in second stage init.
void SetStdioToDevNull(char** argv) {
    // Make stdin/stdout/stderr all point to /dev/null.
    int fd = open("/dev/null", O_RDWR);  // NOLINT(android-cloexec-open)
    if (fd == -1) {
        int saved_errno = errno;
        android::base::InitLogging(argv, &android::base::KernelLogger, InitAborter);
        errno = saved_errno;
        PLOG(FATAL) << "Couldn't open /dev/null";
    }
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > STDERR_FILENO) close(fd);
}

void ExecuteSecondStageInit(void) {
    const char* path = "/system/bin/init";
    const char* args[] = {path, "selinux_setup", nullptr};

#ifdef HWASAN_OPTIONS
    // (second stage) init is the first process to use HWASan. It gets run too
    // early for the HWASAN_OPTIONS in init.environ.rc.gen to apply, so we
    // have to do it here manually.
    //
    // This is especially important for disable_coredump, which defaults to 1.
    // If the user sets it to `0` in HWASAN_OPTIONS, and we didn't also apply
    // it to (second stage) init, the setting would not take effect. This is
    // because `0` is in fact a noop, while `1` applies changes. These changes
    // are inherited beyond exec.
    setenv("HWASAN_OPTIONS", STRINGIFY(HWASAN_OPTIONS), true);
#endif

    execv(path, const_cast<char**>(args));

    // execv() only returns if an error happened, in which case we
    // panic and never fall through this conditional.
    PLOG(FATAL) << "execv(\"" << path << "\") failed";
}

}  // namespace

std::vector<std::string> GetModulesToLoad(const std::string& module_base_dir, const std::string& module_load_list) {
    std::string prefix_list_path = module_base_dir + "/" + module_load_list + "_prefix";
    std::string prefix_list_buf;
    std::vector<std::string> prefix_list;
    std::vector<std::string> result;

    if (!android::base::ReadFileToString(prefix_list_path, &prefix_list_buf)) return result;
    prefix_list = android::base::Split(prefix_list_buf, "\n");
    if (prefix_list.empty()) return result;

    for (const auto& entry : fs::directory_iterator(module_base_dir)) {
        std::string filename = entry.path().filename();
        if (!android::base::EndsWith(filename, ".ko")) continue;
        for (const auto& prefix : prefix_list) {
            if (prefix.empty()) continue;
            if (android::base::StartsWith(filename, prefix)) {
                LOG(INFO) << "Add kernel module " << filename
                          << " to load list based on prefix match: "
                          << prefix;
                result.push_back(filename);
            }
        }
    }

    return result;
}

std::string GetModuleLoadList(BootMode boot_mode, const std::string& dir_path) {
    std::string module_load_file;

    switch (boot_mode) {
        case BootMode::NORMAL_MODE:
            module_load_file = "modules.load";
            break;
        case BootMode::RECOVERY_MODE:
            module_load_file = "modules.load.recovery";
            break;
        case BootMode::CHARGER_MODE:
            module_load_file = "modules.load.charger";
            break;
    }

    if (module_load_file != "modules.load") {
        struct stat fileStat {};
        std::string load_path = dir_path + "/" + module_load_file;
        // Fall back to modules.load if the other files aren't accessible
        if (stat(load_path.c_str(), &fileStat)) {
            module_load_file = "modules.load";
        }
    }

    return module_load_file;
}

bool LoadKernelModules(BootMode boot_mode, bool strict, bool want_parallel, const std::string& module_base_dir) {
    boot_clock::time_point module_start_time = boot_clock::now();
    int modules_loaded = 0;
    struct utsname uts {};
    if (uname(&uts)) {
        LOG(FATAL) << "Failed to get kernel version.";
    }
    int major = 0, minor = 0;
    if (sscanf(uts.release, "%d.%d", &major, &minor) != 2) {
        LOG(FATAL) << "Failed to parse kernel version " << uts.release;
    }

    std::unique_ptr<DIR, decltype(&closedir)> base_dir(opendir(module_base_dir.c_str()), closedir);
    if (!base_dir) {
        LOG(INFO) << "Unable to open /lib/modules, skipping module loading.";
        return true;
    }
    dirent* entry = nullptr;
    std::vector<std::string> module_dirs;
    const auto page_size_suffix = GetPageSizeSuffix();
    const std::string release_specific_module_dir = uts.release + page_size_suffix;
    while ((entry = readdir(base_dir.get()))) {
        if (entry->d_type != DT_DIR) {
            continue;
        }
        if (entry->d_name == release_specific_module_dir) {
            LOG(INFO) << "Release specific kernel module dir " << release_specific_module_dir
                      << " found, loading modules from here with no fallbacks.";
            module_dirs.clear();
            module_dirs.emplace_back(entry->d_name);
            break;
        }
        // Is a directory does not have page size suffix, it does not mean this directory is for 4K
        // kernels. Certain 16K kernel builds put all modules in /lib/modules/`uname -r` without any
        // suffix. Therefore, only ignore a directory if it has _16k/_64k suffix and the suffix does
        // not match system page size.
        const auto dir_page_size_suffix = GetPageSizeSuffix(entry->d_name);
        if (!dir_page_size_suffix.empty() && dir_page_size_suffix != page_size_suffix) {
            continue;
        }
        int dir_major = 0, dir_minor = 0;
        if (sscanf(entry->d_name, "%d.%d", &dir_major, &dir_minor) != 2 || dir_major != major ||
            dir_minor != minor) {
            continue;
        }
        module_dirs.emplace_back(entry->d_name);
    }

    // Sort the directories so they are iterated over during module loading
    // in a consistent order. Alphabetical sorting is fine here because the
    // kernel version at the beginning of the directory name must match the
    // current kernel version, so the sort only applies to a label that
    // follows the kernel version, for example /lib/modules/5.4 vs.
    // /lib/modules/5.4-gki.
    std::sort(module_dirs.begin(), module_dirs.end());

    for (const auto& module_dir : module_dirs) {
        std::string dir_path = module_base_dir + "/";
        dir_path.append(module_dir);
        auto module_dir_module_load_list = GetModuleLoadList(boot_mode, dir_path);
        Modprobe m({dir_path}, module_dir_module_load_list);
        for (const auto& mod : GetModulesToLoad(module_base_dir, module_dir_module_load_list)) {
            m.LoadWithAliases(mod, strict);
        }
        bool retval = m.LoadListedModules(strict);
        modules_loaded = m.GetModuleCount();
        if (modules_loaded > 0) {
            LOG(INFO) << "Loaded " << modules_loaded << " modules from " << dir_path;
            return retval;
        }
    }

    auto module_load_list = GetModuleLoadList(boot_mode, module_base_dir);
    Modprobe m({module_base_dir}, module_load_list);
    for (const auto& mod : GetModulesToLoad(module_base_dir, module_load_list)) {
        m.LoadWithAliases(mod, strict);
    }
    bool retval = (want_parallel) ? m.LoadModulesParallel(std::thread::hardware_concurrency())
                                  : m.LoadListedModules(strict);
    modules_loaded = m.GetModuleCount();
    if (modules_loaded > 0) {
        auto module_elapse_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                boot_clock::now() - module_start_time);
        setenv(kEnvInitModuleDurationMs, std::to_string(module_elapse_time.count()).c_str(), 1);
        LOG(INFO) << "Loaded " << modules_loaded << " kernel modules"
                  << " from " << module_base_dir
                  << " took " << module_elapse_time.count() << " ms";
    }
    return retval;
}

static bool IsChargerMode(const std::string& cmdline, const std::string& bootconfig) {
    return bootconfig.find("androidboot.mode = \"charger\"") != std::string::npos ||
            cmdline.find("androidboot.mode=charger") != std::string::npos;
}

static BootMode GetBootMode(const std::string& cmdline, const std::string& bootconfig)
{
    if (IsChargerMode(cmdline, bootconfig))
        return BootMode::CHARGER_MODE;
    else if (IsRecoveryMode() && !ForceNormalBoot(cmdline, bootconfig))
        return BootMode::RECOVERY_MODE;

    return BootMode::NORMAL_MODE;
}

int FirstStageMain(int argc, char** argv) {
    if (REBOOT_BOOTLOADER_ON_PANIC) {
        InstallRebootSignalHandlers();
    }

    boot_clock::time_point start_time = boot_clock::now();

    std::vector<std::pair<std::string, int>> errors;
#define CHECKCALL(x) \
    if ((x) != 0) errors.emplace_back(#x " failed", errno);

    // Clear the umask.
    umask(0);

    CHECKCALL(setenv("PATH", _PATH_DEFPATH, 1));
    // Get the basic filesystem setup we need put together in the initramdisk
    // on / and then we'll let the rc file figure out the rest.
    CHECKCALL(mount("tmpfs", "/dev", "tmpfs", MS_NOSUID, "mode=0755"));
    CHECKCALL(mkdir("/dev/block", 0755));
    CHECKCALL(mkdir("/dev/pts", 0755));
    CHECKCALL(mkdir("/dev/socket", 0755));
    CHECKCALL(mkdir("/dev/dm-user", 0755));
    CHECKCALL(mount("devpts", "/dev/pts", "devpts", 0, NULL));
#define MAKE_STR(x) __STRING(x)
    CHECKCALL(mount("proc", "/proc", "proc", 0, "hidepid=2,gid=" MAKE_STR(AID_READPROC)));
#undef MAKE_STR
    std::string cmdline;
    android::base::ReadFileToString("/proc/cmdline", &cmdline);
    // Don't expose the raw bootconfig to unprivileged processes.
    chmod("/proc/bootconfig", 0440);
    std::string bootconfig;
    android::base::ReadFileToString("/proc/bootconfig", &bootconfig);
    gid_t groups[] = {AID_READPROC};
    CHECKCALL(setgroups(arraysize(groups), groups));
    CHECKCALL(mount("sysfs", "/sys", "sysfs", 0, NULL));
    CHECKCALL(mount("selinuxfs", "/sys/fs/selinux", "selinuxfs", 0, NULL));

    CHECKCALL(mknod("/dev/kmsg", S_IFCHR | 0600, makedev(1, 11)));

    if constexpr (WORLD_WRITABLE_KMSG) {
        CHECKCALL(mknod("/dev/kmsg_debug", S_IFCHR | 0622, makedev(1, 11)));
    }

    CHECKCALL(mknod("/dev/random", S_IFCHR | 0666, makedev(1, 8)));
    CHECKCALL(mknod("/dev/urandom", S_IFCHR | 0666, makedev(1, 9)));

    // This is needed for log wrapper, which gets called before ueventd runs.
    CHECKCALL(mknod("/dev/ptmx", S_IFCHR | 0666, makedev(5, 2)));
    CHECKCALL(mknod("/dev/null", S_IFCHR | 0666, makedev(1, 3)));

    // These below mounts are done in first stage init so that first stage mount can mount
    // subdirectories of /mnt/{vendor,product}/.  Other mounts, not required by first stage mount,
    // should be done in rc files.
    // Mount staging areas for devices managed by vold
    // See storage config details at http://source.android.com/devices/storage/
    CHECKCALL(mount("tmpfs", "/mnt", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=1000"));
    // /mnt/vendor is used to mount vendor-specific partitions that can not be
    // part of the vendor partition, e.g. because they are mounted read-write.
    CHECKCALL(mkdir("/mnt/vendor", 0755));
    // /mnt/product is used to mount product-specific partitions that can not be
    // part of the product partition, e.g. because they are mounted read-write.
    CHECKCALL(mkdir("/mnt/product", 0755));

    // /debug_ramdisk is used to preserve additional files from the debug ramdisk
    CHECKCALL(mount("tmpfs", "/debug_ramdisk", "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"));

    // /second_stage_resources is used to preserve files from first to second
    // stage init
    CHECKCALL(mount("tmpfs", kSecondStageRes, "tmpfs", MS_NOEXEC | MS_NOSUID | MS_NODEV,
                    "mode=0755,uid=0,gid=0"));
#undef CHECKCALL

    SetStdioToDevNull(argv);
    // Now that tmpfs is mounted on /dev and we have /dev/kmsg, we can actually
    // talk to the outside world...
    InitKernelLogging(argv);

    if (!errors.empty()) {
        for (const auto& [error_string, error_errno] : errors) {
            LOG(ERROR) << error_string << " " << strerror(error_errno);
        }
        LOG(FATAL) << "Init encountered errors starting first stage, aborting";
    }

    LOG(INFO) << "generic init first stage started!";

    auto old_root_dir = std::unique_ptr<DIR, decltype(&closedir)>{opendir("/"), closedir};
    if (!old_root_dir) {
        PLOG(ERROR) << "Could not opendir(\"/\"), not freeing ramdisk";
    }

    struct stat old_root_info {};
    if (stat("/", &old_root_info) != 0) {
        PLOG(ERROR) << "Could not stat(\"/\"), not freeing ramdisk";
        old_root_dir.reset();
    }

    auto want_console = ALLOW_FIRST_STAGE_CONSOLE ? FirstStageConsole(cmdline, bootconfig) : 0;
    auto want_parallel =
            bootconfig.find("androidboot.load_modules_parallel = \"true\"") != std::string::npos;

    BootMode boot_mode = GetBootMode(cmdline, bootconfig);
    if (!LoadKernelModules(boot_mode, false,
                           want_parallel, "/lib/modules")) {
        LOG(ERROR) << "Failed to load kernel modules from ramdisk";
    }

    if (want_console == FirstStageConsoleParam::CONSOLE_ON_FAILURE) {
        StartConsole(cmdline);
    }

    if (access(kBootImageRamdiskProp, F_OK) == 0) {
        std::string dest = GetRamdiskPropForSecondStage();
        std::string dir = android::base::Dirname(dest);
        std::error_code ec;
        if (!fs::create_directories(dir, ec) && !!ec) {
            LOG(FATAL) << "Can't mkdir " << dir << ": " << ec.message();
        }
        if (!fs::copy_file(kBootImageRamdiskProp, dest, ec)) {
            LOG(FATAL) << "Can't copy " << kBootImageRamdiskProp << " to " << dest << ": "
                       << ec.message();
        }
        LOG(INFO) << "Copied ramdisk prop to " << dest;
    }

    // If "/force_debuggable" is present, the second-stage init will use a userdebug
    // sepolicy and load adb_debug.prop to allow adb root, if the device is unlocked.
    if (access("/force_debuggable", F_OK) == 0) {
        constexpr const char adb_debug_prop_src[] = "/adb_debug.prop";
        constexpr const char userdebug_plat_sepolicy_cil_src[] = "/userdebug_plat_sepolicy.cil";
        std::error_code ec;  // to invoke the overloaded copy_file() that won't throw.
        if (access(adb_debug_prop_src, F_OK) == 0 &&
            !fs::copy_file(adb_debug_prop_src, kDebugRamdiskProp, ec)) {
            LOG(WARNING) << "Can't copy " << adb_debug_prop_src << " to " << kDebugRamdiskProp
                         << ": " << ec.message();
        }
        if (access(userdebug_plat_sepolicy_cil_src, F_OK) == 0 &&
            !fs::copy_file(userdebug_plat_sepolicy_cil_src, kDebugRamdiskSEPolicy, ec)) {
            LOG(WARNING) << "Can't copy " << userdebug_plat_sepolicy_cil_src << " to "
                         << kDebugRamdiskSEPolicy << ": " << ec.message();
        }
        // setenv for second-stage init to read above kDebugRamdisk* files.
        setenv("INIT_FORCE_DEBUGGABLE", "true", 1);
    }

    if (boot_mode != BootMode::RECOVERY_MODE) {
        // Kernel module loading and partition mounting are handled here
        MountHandler::OnPreBlockDevices();
        ueventd_main(ParseConfig({"/system/etc/ueventd.ramdisk.rc"}), true);

        mkdir("/first_stage_ramdisk", 0755);
        // SwitchRoot() must be called with a mount point as the target, so we bind mount the
        // target directory to itself here.
        if (mount("/first_stage_ramdisk", "/first_stage_ramdisk", nullptr, MS_BIND, nullptr) != 0) {
            PLOG(FATAL) << "Could not bind mount /first_stage_ramdisk to itself";
        }
        SwitchRoot("/first_stage_ramdisk");

        MountHandler::OnPostBlockDevices();

        if (!LoadKernelModules(boot_mode, false,
                            want_parallel, "/vendor/lib/modules")) {
            LOG(ERROR) << "Failed to load kernel modules from vendor partition";
        }

        ueventd_main(ParseConfig({"/system/etc/ueventd.rc"}), false);
    }

    struct stat new_root_info {};
    if (stat("/", &new_root_info) != 0) {
        PLOG(ERROR) << "Could not stat(\"/\"), not freeing ramdisk";
        old_root_dir.reset();
    }

    if (old_root_dir && old_root_info.st_dev != new_root_info.st_dev) {
        FreeRamdisk(old_root_dir.get(), old_root_info.st_dev);
    }

    setenv(kEnvFirstStageStartedAt, std::to_string(start_time.time_since_epoch().count()).c_str(),
           1);

    auto fd = open("/dev/kmsg", O_WRONLY | O_CLOEXEC);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    ExecuteSecondStageInit();

    return 1;
}

}  // namespace init
}  // namespace android
