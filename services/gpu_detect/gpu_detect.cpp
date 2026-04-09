/*
  SPDX-FileCopyrightText: The LineageOS Project
  SPDX-License-Identifier: Apache-2.0
*/

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <list>
#include <set>
#include <string>
#include <unordered_map>

#define LOG_TAG "gpu_detect"
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include <i915_drm.h>
#include <virtgpu_drm.h>
#include <xf86drm.h>

using namespace android::base;

namespace {

// From system/core/libsystem/include/system/graphics-base-v1.0.h
typedef enum {
    HAL_PIXEL_FORMAT_RGBA_8888 = 1,
    HAL_PIXEL_FORMAT_RGBX_8888 = 2,
    HAL_PIXEL_FORMAT_RGB_888 = 3,
    HAL_PIXEL_FORMAT_RGB_565 = 4,
    HAL_PIXEL_FORMAT_BGRA_8888 = 5,
} android_pixel_format_t;

constexpr int kGlesVersion20 = 131072;
constexpr int kGlesVersion30 = 196608;
constexpr int kGlesVersion31 = 196609;
constexpr int kGlesVersion32 = 196610;

constexpr char kCtlStopProp[] = "ctl.stop";

constexpr char kGlesVersionProp[] = "ro.opengles.version";
constexpr char kHwEglProp[] = "ro.hardware.egl";
constexpr char kHwGrallocProp[] = "ro.hardware.gralloc";
constexpr char kHwHwcProp[] = "ro.hardware.hwcomposer";
constexpr char kHwVulkanProp[] = "ro.hardware.vulkan";

constexpr char kGrallocApexProp[] = "ro.boot.vendor.apex.org.lineageos.device.gralloc";
constexpr char kHwcApexProp[] = "ro.boot.vendor.apex.org.lineageos.device.hwcomposer";
constexpr char kVulkanApexProp[] = "ro.boot.vendor.apex.org.lineageos.device.graphics.vulkan";

constexpr char kGraphicsGpuNameProp[] = "ro.vendor.graphics.gpu.name";

constexpr char kBootGraphicsProp[] = "ro.boot.graphics";
constexpr char kBootOdmSkuProp[] = "ro.boot.product.hardware.sku";
constexpr char kBootUseFbDisplayProp[] = "ro.boot.use_fb_display";

constexpr char kHwcDrmDeviceProp[] = "vendor.hwc.drm.device";

constexpr char kMinigbmGenericBackendProp[] = "vendor.minigbm.generic_backend";

constexpr char kSfNativeWindowBuffersFormatProp[] =
        "ro.surface_flinger.native_window_buffers_format";
constexpr char kSfSupportsBackgroundBlurProp[] = "ro.surface_flinger.supports_background_blur";

const std::string kDmiIdPath = "/sys/devices/virtual/dmi/id/";

const std::set<std::string> kDrmSysfbNames = {"efidrm", "simpledrm", "vesadrm"};
const std::set<std::string> kMustUseFbDisplayGpus = {};

struct HalApex {
    std::string name;
    std::list<std::string> init_rc_services;
};

enum class HwEgl {
    Unset,
    Angle,
    Mesa,
};

const std::unordered_map<HwEgl, std::string> kHwEglMap = {
        {HwEgl::Angle, "angle"},
        {HwEgl::Mesa, "mesa"},
};

enum class HwGralloc {
    Unset,
    Default,
    Gbm,
    MinigbmUpstream,
};

const std::unordered_map<HwGralloc, std::string> kHwGrallocMap = {
        {HwGralloc::Default, "default"},
        {HwGralloc::Gbm, "gbm"},
        {HwGralloc::MinigbmUpstream, "minigbm_upstream"},
};

enum class HwHwc {
    Unset,
};

const std::unordered_map<HwHwc, std::string> kHwHwcMap = {
};

enum class HwVulkan {
    Unset,
    Intel,
    Intel_hasvk,
    Lvp,
    Nouveau,
    Radeon,
    Virtio,
};

const std::unordered_map<HwVulkan, std::string> kHwVulkanMap = {
        {HwVulkan::Intel, "intel"},     {HwVulkan::Intel_hasvk, "intel_hasvk"},
        {HwVulkan::Nouveau, "nouveau"}, {HwVulkan::Radeon, "radeon"},
        {HwVulkan::Virtio, "virtio"},   {HwVulkan::Lvp, "lvp_mesa3d"},
};

enum class GrallocApex {
    Unset,
    MinigbmUpstream,
    V2_0,
};

const std::unordered_map<GrallocApex, HalApex> kGrallocApexMap = {
        {GrallocApex::MinigbmUpstream,
         {"org.lineageos.device.gralloc.minigbm_upstream_nonapex",
          {"vendor.graphics.allocator.minigbm_upstream"}}},
        {GrallocApex::V2_0,
         {"org.lineageos.device.gralloc.v2_0", {"vendor.gralloc-2-0"}}},
};

enum class HwcApex {
    Unset,
    Drm,
    DrmApex,
    DrmFb,
    V2_2,
    V2_4,
};

const std::unordered_map<HwcApex, HalApex> kHwcApexMap = {
        {HwcApex::Drm,
         {"org.lineageos.device.hwcomposer.drm",
          {"vendor.hwcomposer-3.drm_upstream"}}},  // drm HWC AIDL HAL in /vendor
        {HwcApex::DrmApex,
         {"org.lineageos.device.hwcomposer.drm_apex",
          {"vendor.hwcomposer-3.drm_upstream-apex"}}},  // drm HWC AIDL HAL in /apex
        {HwcApex::DrmFb,
         {"org.lineageos.device.hwcomposer.drmfb", {"vendor.hwcomposer-2-1.drmfb"}}},
        {HwcApex::V2_2, {"org.lineageos.device.hwcomposer.v2_2", {"vendor.hwcomposer-2-2"}}},
        {HwcApex::V2_4, {"org.lineageos.device.hwcomposer.v2_4", {"vendor.hwcomposer-2-4"}}},
};

enum class VulkanApex {
    Unset,
    No_apex,
    Swiftshader,
};

const std::unordered_map<VulkanApex, std::string> kVulkanApexMap = {
        {VulkanApex::No_apex, "org.lineageos.device.graphics.vulkan.no_apex"},
        {VulkanApex::Swiftshader, "org.lineageos.device.graphics.vulkan.swiftshader"},
};

enum class MinigbmGenericBackend {
    Unset,
    DmabufHeap,
    DumbGeneric,
    GbmMesa,
};

const std::unordered_map<MinigbmGenericBackend, std::string> kMinigbmGenericBackendMap = {
        {MinigbmGenericBackend::DmabufHeap, "dmabuf_heap"},
        {MinigbmGenericBackend::DumbGeneric, "dumb_generic"},
        {MinigbmGenericBackend::GbmMesa, "gbm_mesa"},
};

int gGlesVersion = kGlesVersion20;
HwEgl gHwEgl = HwEgl::Unset;
HwGralloc gHwGralloc = HwGralloc::Unset;
HwHwc gHwHwc = HwHwc::Unset;
HwVulkan gHwVulkan = HwVulkan::Unset;
GrallocApex gGrallocApex = GrallocApex::Unset;
HwcApex gHwcApex = HwcApex::Unset;
VulkanApex gVulkanApex = VulkanApex::Unset;
MinigbmGenericBackend gMinigbmGenericBackend = MinigbmGenericBackend::Unset;
android_pixel_format_t gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_RGBA_8888;

const std::unordered_map<std::string, int*> kBootOverridesProp = {
        {"gles_version", &gGlesVersion},
        {"hw_egl", reinterpret_cast<int*>(&gHwEgl)},
        {"hw_gralloc", reinterpret_cast<int*>(&gHwGralloc)},
        {"hw_hwc", reinterpret_cast<int*>(&gHwHwc)},
        {"hw_vulkan", reinterpret_cast<int*>(&gHwVulkan)},
        {"gralloc_apex", reinterpret_cast<int*>(&gGrallocApex)},
        {"hwc_apex", reinterpret_cast<int*>(&gHwcApex)},
        {"vulkan_apex", reinterpret_cast<int*>(&gVulkanApex)},
        {"minigbm_generic_backend", reinterpret_cast<int*>(&gMinigbmGenericBackend)},
        {"sf_native_window_buffers_format", reinterpret_cast<int*>(&gSfNativeWindowBuffersFormat)},
};

void ProcessBootOverrides() {
    for (const auto& [name, pvar] : kBootOverridesProp) {
        std::string prop = "ro.boot." + name;
        int new_value = GetIntProperty<int>(prop, -1);
        if (new_value != -1) {
            LOG(INFO) << __FUNCTION__ << "(): Override " << name << " from "
                      << std::to_string(*pvar) << " to " << std::to_string(new_value);
            *pvar = new_value;
        }
    }
}

bool ApplySelections(void) {
    ProcessBootOverrides();

    bool ret = true;
    const std::string* strp;

    if (gSfNativeWindowBuffersFormat != HAL_PIXEL_FORMAT_RGBA_8888) {
        LOG(INFO) << "Set surfaceflinger native window buffers format to "
                  << std::to_string(gSfNativeWindowBuffersFormat);
        ret &= SetProperty(kSfNativeWindowBuffersFormatProp,
                           std::to_string(gSfNativeWindowBuffersFormat));
    }

    LOG(INFO) << "Set OpenGLES version to " << std::to_string(gGlesVersion);
    ret &= SetProperty(kGlesVersionProp, std::to_string(gGlesVersion));

    if (gHwEgl != HwEgl::Unset) {
        strp = &kHwEglMap.at(gHwEgl);
        LOG(INFO) << "Set EGL to " << *strp;
        ret &= SetProperty(kHwEglProp, *strp);
    } else {
        LOG(WARNING) << "EGL is unset";
    }

    if (gMinigbmGenericBackend != MinigbmGenericBackend::Unset) {
        strp = &kMinigbmGenericBackendMap.at(gMinigbmGenericBackend);
        LOG(INFO) << "Set minigbm generic backend to " << *strp;
        ret &= SetProperty(kMinigbmGenericBackendProp, *strp);
    } else {
        LOG(WARNING) << "Minigbm generic backend is unset";
    }

    if (gHwGralloc != HwGralloc::Unset) {
        strp = &kHwGrallocMap.at(gHwGralloc);
        LOG(INFO) << "Set Gralloc module to " << *strp;
        ret &= SetProperty(kHwGrallocProp, *strp);
    } else {
        LOG(WARNING) << "Gralloc module is unset";
    }

    if (gHwHwc != HwHwc::Unset) {
        strp = &kHwHwcMap.at(gHwHwc);
        LOG(INFO) << "Set Hwcomposer module to " << *strp;
        ret &= SetProperty(kHwHwcProp, *strp);
    } else {
        LOG(WARNING) << "Hwcomposer module is unset";
    }

    if (gVulkanApex != VulkanApex::Unset) {
        strp = &kVulkanApexMap.at(gVulkanApex);
        LOG(INFO) << "Set Vulkan APEX to " << *strp;
        ret &= SetProperty(kVulkanApexProp, *strp);
    } else if (gHwVulkan != HwVulkan::Unset || gVulkanApex == VulkanApex::Unset) {
        // Use dummy Vulkan APEX
        strp = &kVulkanApexMap.at(VulkanApex::No_apex);
        LOG(INFO) << "Set Vulkan APEX to " << *strp;
        ret &= SetProperty(kVulkanApexProp, *strp);

        if (gHwVulkan != HwVulkan::Unset) {
            strp = &kHwVulkanMap.at(gHwVulkan);
            LOG(INFO) << "Set Vulkan to " << *strp;
            ret &= SetProperty(kHwVulkanProp, *strp);
        }
    } else {
        LOG(WARNING) << "Vulkan is unset";
    }

    if (gGrallocApex != GrallocApex::Unset) {
        strp = &kGrallocApexMap.at(gGrallocApex).name;
        LOG(INFO) << "Set Graphics Allocator APEX to " << *strp;
        ret &= SetProperty(kGrallocApexProp, *strp);

        /*
            Disable init.rc services of the other Gralloc APEXes.
            Some Gralloc APEXes may not contain init.rc,
            and the actual init.rc to be used is in /vendor/etc/init.
            These APEXes may only contain vintf manifest.
        */
        for (const auto& [id, apex] : kGrallocApexMap) {
            if (apex.name == *strp) continue;

            for (const auto& svc : apex.init_rc_services) {
                LOG(INFO) << "Stop service " << svc;
                SetProperty(kCtlStopProp, svc);
            }
        }
    } else {
        LOG(WARNING) << "Graphics Allocator APEX is unset";
    }

    if (gHwcApex != HwcApex::Unset) {
        strp = &kHwcApexMap.at(gHwcApex).name;
        LOG(INFO) << "Set Graphics Composer APEX to " << *strp;
        ret &= SetProperty(kHwcApexProp, *strp);

        // Same as Gralloc APEX's logic
        for (const auto& [id, apex] : kHwcApexMap) {
            if (apex.name == *strp) continue;

            for (const auto& svc : apex.init_rc_services) {
                LOG(INFO) << "Stop service " << svc;
                SetProperty(kCtlStopProp, svc);
            }
        }
    } else {
        LOG(WARNING) << "Graphics Composer APEX is unset";
    }

    // Enablue blur if not using Swiftshader graphics
    if (gVulkanApex != VulkanApex::Swiftshader) {
        LOG(INFO) << "Enable blur";
        ret &= SetProperty(kSfSupportsBackgroundBlurProp, "1");
    }

    /*
     * HACK: IMapper loads the lib using openDeclaredPassthroughHal()
     *       If vintf manifest is in APEX and the lib is in /vendor, it will not load
     */
    if (gGrallocApex == GrallocApex::MinigbmUpstream) {
        LOG(INFO) << "HACK: Set odm sku to minigbm_imapper5";
        ret &= SetProperty(kBootOdmSkuProp, "minigbm_imapper5");
    }

    if (!ret) LOG(ERROR) << __FUNCTION__ << "(): Failed to set some properties";

    return ret;
}

bool IsForcedSwiftshader(void) {
    return GetProperty(kBootGraphicsProp, "") == "swiftshader";
}

bool IsForcedFramebufferDisplay(void) {
    return GetBoolProperty(kBootUseFbDisplayProp, false);
}

void UseSwiftshaderGraphics(void) {
    gHwEgl = HwEgl::Angle;
    gGlesVersion = kGlesVersion31;
    gVulkanApex = VulkanApex::Swiftshader;
}

void SetupFramebufferDisplay(void) {
    gHwGralloc = HwGralloc::Default;
    gHwHwc = HwHwc::Unset;

    gGrallocApex = GrallocApex::V2_0;
    gHwcApex = HwcApex::V2_2;

    gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_BGRA_8888;

    UseSwiftshaderGraphics();
}

void OnDetectDrmSysfb(void) {
    LOG(INFO) << "Detected DRM sysfb";

    /*
        gHwcApex = HwcApex::Drm;
        gGrallocApex = GrallocApex::MinigbmUpstream;
        gHwGralloc = HwGralloc::MinigbmUpstream;

        UseSwiftshaderGraphics();
    */

    SetupFramebufferDisplay();
}

void OnDetectUnknownGpu(void) {
    LOG(WARNING) << "GPU is unsupported, applying defaults";

    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwcApex = HwcApex::Drm;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    gHwEgl = HwEgl::Mesa;
}

void OnDetectAmdGpu(int fd) {
    int ret;

    ret = drmSetClientCap(fd, DRM_CLIENT_CAP_ATOMIC, 1);
    if (!ret) {
        gHwcApex = HwcApex::Drm;
    } else {
        gHwcApex = HwcApex::DrmFb;
    }

    gMinigbmGenericBackend = MinigbmGenericBackend::DumbGeneric;
    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    gGlesVersion = kGlesVersion32;
    gHwEgl = HwEgl::Mesa;
    gHwVulkan = HwVulkan::Radeon;
}

void OnDetectIntelGpu(int fd) {
    int ret = 0;

    gHwcApex = HwcApex::Drm;
    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    if (!IsForcedSwiftshader()) {
        gGlesVersion = kGlesVersion32;
        gHwEgl = HwEgl::Mesa;
        gHwVulkan = HwVulkan::Intel;  // May get overridden later
    } else {
        UseSwiftshaderGraphics();
    }

    int value;
    drm_i915_getparam_t get_param = {
            .value = &value,
    };

    get_param.param = I915_PARAM_CHIPSET_ID;
    ret = drmIoctl(fd, DRM_IOCTL_I915_GETPARAM, &get_param);
    if (!ret) {
        // Enable various workarounds
        /*
         * If the determination gets more complicated in future,
         * We can consider using minigbm's i915_info_from_device_id()
         */
        if (value < 0x1902 && value != 0x0f31) {
            // From Intel Core to pre-Skylake (HD Graphics 510)
            // Except for Atom Processor Z36xxx/Z37xxx
            SetProperty("ro.vendor.hwc.drm.avoid_using_alpha_bits_for_framebuffer", "1");
            SetProperty("ro.vendor.hwc.drm.disable_planes", "1");
        }
        if (value <= 0x0F33 || (value >= 0x1602 && value <= 0x162E) ||
            (value >= 0x22B0 && value <= 0x22B3)) {
            // Approximate of gen7_ids and gen8_ids according to minigbm/i915.c
            gHwVulkan = HwVulkan::Intel_hasvk;
        }
        // What about pre Intel Core? Those won't even boot...
    } else {
        LOG(ERROR) << "Failed to get I915_PARAM_CHIPSET_ID";
    }
}

void OnDetectNouveauGpu(void) {
    gHwcApex = HwcApex::DrmFb;
    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    gGlesVersion = kGlesVersion31;
    gHwEgl = HwEgl::Mesa;
    gHwVulkan = HwVulkan::Nouveau;
}

void OnDetectQxlGpu(void) {
    SetupFramebufferDisplay();
}

void OnDetectRadeonGpu(void) {
    gHwcApex = HwcApex::DrmFb;

    gMinigbmGenericBackend = MinigbmGenericBackend::DumbGeneric;
    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    gGlesVersion = kGlesVersion31;
    gHwEgl = HwEgl::Mesa;
}

void OnDetectVirtioGpu(int fd) {
    int ret = 0;

    gGrallocApex = GrallocApex::MinigbmUpstream;
    gHwcApex = HwcApex::Drm;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    uint32_t value;
    struct drm_virtgpu_getparam get_param = {
            .value = (uint64_t)(uintptr_t)&value,
    };

    get_param.param = VIRTGPU_PARAM_3D_FEATURES;
    ret = drmIoctl(fd, DRM_IOCTL_VIRTGPU_GETPARAM, &get_param);
    if (!ret) {
        if (value) {
            gHwEgl = HwEgl::Mesa;
            gHwVulkan = HwVulkan::Virtio;
            gGlesVersion = kGlesVersion32;
        } else {
            gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_BGRA_8888;
            UseSwiftshaderGraphics();
        }
    } else {
        LOG(ERROR) << "Failed to get 3D features parameter from virtio_gpu";
    }
}

void OnDetectVmwgfxGpu(void) {
    std::string smbios_product_name;
    ReadFileToString(kDmiIdPath + "product_name", &smbios_product_name);
    if (!smbios_product_name.empty()) smbios_product_name.pop_back();

    if (smbios_product_name == "VirtualBox" || IsForcedSwiftshader()) {
        // 3D acceleration does not work on VirtualBox
        SetupFramebufferDisplay();
    } else {
        gGrallocApex = GrallocApex::MinigbmUpstream;
        gHwcApex = HwcApex::Drm;
        gHwGralloc = HwGralloc::MinigbmUpstream;

        gGlesVersion = kGlesVersion31;
        gHwEgl = HwEgl::Mesa;
    }
}

}  // namespace

int main(int, char* argv[]) {
    InitLogging(argv, &KernelLogger);

    if (IsForcedFramebufferDisplay()) {
        LOG(INFO) << "Forced using framebuffer display";
        SetupFramebufferDisplay();
        return ApplySelections() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    std::string path;
    int fd;
    drmVersionPtr version;
    std::string name;
    std::list<unsigned int> drm_sysfb_cards;

    for (uint32_t i = 0; i < DRM_MAX_MINOR; i++) {
        path = std::string("/dev/dri/card" + std::to_string(i));
        fd = open(path.c_str(), O_RDONLY);
        if (fd >= 0) {
            version = drmGetVersion(fd);
            if (!version) {
                LOG(ERROR) << "Failed to get DRM version for card " << std::to_string(i);
                close(fd);
                fd = -1;
                continue;
            }

            name = std::string(version->name, version->name_len);
            LOG(INFO) << "Card " << std::to_string(i) << " is " << name;

            // Save DRM sysfb card, and close it for now
            if (kDrmSysfbNames.find(name) != kDrmSysfbNames.end()) {
                drm_sysfb_cards.push_back(i);
                drmFreeVersion(version);
                close(fd);
                fd = -1;
                continue;
            }

            break;
        }
    }

    if (fd < 0 && !drm_sysfb_cards.empty()) {
        for (const auto& i : drm_sysfb_cards) {
            path = std::string("/dev/dri/card" + std::to_string(i));
            fd = open(path.c_str(), O_RDONLY);
            if (fd >= 0) {
                version = drmGetVersion(fd);
                if (!version) {
                    LOG(ERROR) << "Failed to get DRM version for card " << std::to_string(i);
                    close(fd);
                    fd = -1;
                    continue;
                }
                name = std::string(version->name, version->name_len);
            }
        }
    }

    if (fd < 0) {
        LOG(ERROR) << "Failed to open any DRM device, falling back to framebuffer display";
        SetupFramebufferDisplay();
        return ApplySelections() ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    /* TODO:
     * HWC uses card nodes, Gralloc tries renderD nodes first
     * Handle the situation on i.e. laptops with discrete graphics
     */
    SetProperty(kHwcDrmDeviceProp, path);

    SetProperty(kGraphicsGpuNameProp, name);
    LOG(INFO) << "GPU name is " << name;
    if (kMustUseFbDisplayGpus.find(name) != kMustUseFbDisplayGpus.end()) {
        LOG(INFO) << "This GPU must use framebuffer display for now";
        SetupFramebufferDisplay();
    } else if (kDrmSysfbNames.find(name) != kDrmSysfbNames.end()) {
        OnDetectDrmSysfb();
    } else if (name == "amdgpu") {
        OnDetectAmdGpu(fd);
    } else if (name == "i915") {
        OnDetectIntelGpu(fd);
    } else if (name == "nouveau") {
        OnDetectNouveauGpu();
    } else if (name == "qxl") {
        OnDetectQxlGpu();
    } else if (name == "radeon") {
        OnDetectRadeonGpu();
    } else if (name == "virtio_gpu") {
        OnDetectVirtioGpu(fd);
    } else if (name == "vmwgfx") {
        OnDetectVmwgfxGpu();
    } else {
        OnDetectUnknownGpu();
    }

    drmFreeVersion(version);
    close(fd);
    return ApplySelections() ? EXIT_SUCCESS : EXIT_FAILURE;
}
