/*
  SPDX-FileCopyrightText: The LineageOS Project
  SPDX-License-Identifier: Apache-2.0
*/

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include <filesystem>
#include <list>
#include <set>
#include <string>
#include <unordered_map>

#define LOG_TAG "hardware_detect"
#include <android-base/file.h>
#include <android-base/logging.h>
#include <android-base/properties.h>

#include <i915_drm.h>
#include <virtgpu_drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

extern "C" {
#include <freedreno_drmif.h>
}

namespace fs = std::filesystem;
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
constexpr char kHwAudioPrimaryProp[] = "ro.hardware.audio.primary";
constexpr char kHwEglProp[] = "ro.hardware.egl";
constexpr char kHwGrallocProp[] = "ro.hardware.gralloc";
constexpr char kHwHwcProp[] = "ro.hardware.hwcomposer";
constexpr char kHwVulkanProp[] = "ro.hardware.vulkan";

constexpr char kUsbGadgetApexProp[] = "ro.boot.vendor.apex.com.android.hardware.usb.gadget";
constexpr char kVulkanApexProp[] = "ro.boot.vendor.apex.org.lineageos.device.graphics.vulkan";

constexpr char kGraphicsCardNameProp[] = "ro.vendor.graphics.card.name";
constexpr char kGraphicsRenderNameProp[] = "ro.vendor.graphics.render.name";

constexpr char kBootGraphicsProp[] = "ro.boot.graphics";
constexpr char kBootOdmSkuProp[] = "ro.boot.product.hardware.sku";
constexpr char kBootUseFbDisplayProp[] = "ro.boot.use_fb_display";

constexpr char kHwcDrmDeviceProp[] = "vendor.hwc.drm.device";

constexpr char kMinigbmGenericBackendProp[] = "vendor.minigbm.generic_backend";

constexpr char kSfNativeWindowBuffersFormatProp[] =
        "ro.surface_flinger.native_window_buffers_format";
constexpr char kSfSupportsBackgroundBlurProp[] = "ro.surface_flinger.supports_background_blur";

constexpr char kUsbAdbDisabledProp[] = "vendor.sys.usb.adb.disabled";
constexpr char kUsbControllerProp[] = "sys.usb.controller";

const std::string kApexSelectPropPrefix = "ro.boot.vendor.apex.";

const std::string kDmiIdPath = "/sys/devices/virtual/dmi/id/";
const std::string kVintfDestDir = "/vendor/etc/vintf/manifest/";
const std::string kVintfSrcDir = "/vendor/etc/vintf_src/";

const std::set<std::string> kDrmSysfbNames = {"efidrm", "simpledrm", "vesadrm"};
const std::set<std::string> kMustUseFbDisplayCards = {};

struct HalService {
    std::string name;

    std::string apex_base_name;
    std::string apex_full_name;

    std::list<std::string> init_rc_services;
    std::list<std::string> vintf_fragments;
};

enum class GrallocHalServices {
    Unset,
    MinigbmUpstream,
    V2_0,
};

const HalService kMinigbmUpstreamHalService = {
    .name = "Minigbm (Upstream)",
    .init_rc_services = {"vendor.graphics.allocator.minigbm_upstream"},
    .vintf_fragments = {"allocator.minigbm_upstream.xml", "mapper.minigbm_upstream.xml"},
};

const HalService kGrallocV2_0HalService = {
    .name = "AOSP Gralloc v2.0",
    .init_rc_services = {"vendor.gralloc-2-0"},
    .vintf_fragments = {"manifest_mainline_common_graphics-allocator-hal_default-hidl-2.0.xml"},
};

const std::unordered_map<GrallocHalServices, const HalService*> kGrallocHalServiceMap = {
    {GrallocHalServices::MinigbmUpstream, &kMinigbmUpstreamHalService},
    {GrallocHalServices::V2_0, &kGrallocV2_0HalService},
};

enum class HwcHalServices {
    Unset,
    DrmFb,
    DrmUpstream,
    V2_2,
    V2_4,
};

const HalService kDrmFbHalService = {
    .name = "DRM Framebuffer",
    .init_rc_services = {"vendor.hwcomposer-2-1.drmfb"},
    .vintf_fragments = {"android.hardware.graphics.composer@2.1-service.drmfb.xml"},
};

const HalService kDrmUpstreamHalService = {
    .name = "DRM (Upstream)",
    .init_rc_services = {"vendor.hwcomposer-3.drm_upstream"},
    .vintf_fragments = {"hwc3-drm-upstream.xml"},
};

const HalService kHwcV2_2HalService = {
    .name = "AOSP HWComposer v2.2",
    .init_rc_services = {"vendor.hwcomposer-2-2"},
    .vintf_fragments = {"manifest_mainline_common_graphics-composer-hal_default-hidl-2.2.xml"},
};

const HalService kHwcV2_4HalService = {
    .name = "AOSP HWComposer v2.4",
    .init_rc_services = {"vendor.hwcomposer-2-4"},
    .vintf_fragments = {"manifest_mainline_common_graphics-composer-hal_default-hidl-2.4.xml"},
};

const std::unordered_map<HwcHalServices, const HalService*> kHwcHalServiceMap = {
    {HwcHalServices::DrmFb, &kDrmFbHalService},
    {HwcHalServices::DrmUpstream, &kDrmUpstreamHalService},
    {HwcHalServices::V2_2, &kHwcV2_2HalService},
    {HwcHalServices::V2_4, &kHwcV2_4HalService},
};

// `apex_base_name`, `apex_full_name` or full name of APEX that is empty
std::unordered_map<std::string, std::string> HalServiceApexSelections;

enum class HwAudioPrimary {
    Unset,
    Tinyhal,
};

const std::unordered_map<HwAudioPrimary, std::string> kHwAudioPrimaryMap = {
        {HwAudioPrimary::Tinyhal, "tinyhal"},
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
    Asahi,
    Broadcom,
    Freedreno,
    Imagination,
    Intel,
    Intel_hasvk,
    Lvp,
    Nouveau,
    Panfrost,
    Radeon,
    Virtio,
};

const std::unordered_map<HwVulkan, std::string> kHwVulkanMap = {
        {HwVulkan::Intel, "intel"},     {HwVulkan::Intel_hasvk, "intel_hasvk"},
        {HwVulkan::Nouveau, "nouveau"}, {HwVulkan::Radeon, "radeon"},
        {HwVulkan::Virtio, "virtio"},   {HwVulkan::Lvp, "lvp_mesa3d"},
        {HwVulkan::Freedreno, "freedreno"},
        {HwVulkan::Asahi, "asahi"},     {HwVulkan::Broadcom, "broadcom"},
        {HwVulkan::Imagination, "imagination"},
        {HwVulkan::Panfrost, "panfrost"},
};

enum class UsbGadgetApex {
    Unset,
    Mainline,
    None,
};

const std::unordered_map<UsbGadgetApex, std::string> kUsbGadgetApexMap = {
        {UsbGadgetApex::Mainline, "com.android.hardware.usb.gadget.mainline"},
        {UsbGadgetApex::None, "com.android.hardware.usb.gadget.none"},
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
GrallocHalServices gGrallocHalService = GrallocHalServices::Unset;
HwcHalServices gHwcHalService = HwcHalServices::Unset;
HwAudioPrimary gHwAudioPrimary = HwAudioPrimary::Unset;
HwEgl gHwEgl = HwEgl::Unset;
HwGralloc gHwGralloc = HwGralloc::Unset;
HwHwc gHwHwc = HwHwc::Unset;
HwVulkan gHwVulkan = HwVulkan::Unset;
UsbGadgetApex gUsbGadgetApex = UsbGadgetApex::Unset;
VulkanApex gVulkanApex = VulkanApex::Unset;
MinigbmGenericBackend gMinigbmGenericBackend = MinigbmGenericBackend::Unset;
android_pixel_format_t gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_RGBA_8888;

const std::unordered_map<std::string, int*> kBootOverridesProp = {
        {"gles_version", &gGlesVersion},
        {"gralloc_hal_service", reinterpret_cast<int*>(gGrallocHalService)},
        {"hwc_hal_service", reinterpret_cast<int*>(gHwcHalService)},
        {"hw_audio_primary", reinterpret_cast<int*>(&gHwAudioPrimary)},
        {"hw_egl", reinterpret_cast<int*>(&gHwEgl)},
        {"hw_gralloc", reinterpret_cast<int*>(&gHwGralloc)},
        {"hw_hwc", reinterpret_cast<int*>(&gHwHwc)},
        {"hw_vulkan", reinterpret_cast<int*>(&gHwVulkan)},
        {"usb_gadget_apex", reinterpret_cast<int*>(&gUsbGadgetApex)},
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

bool EnableHalService(const HalService* hal_service, bool enable) {
    bool ret = true;
    std::error_code ec;
    LOG(INFO) << (enable ? "Enable" : "Disable") << " HAL service: " << hal_service->name;
    if (enable) {
        if (!hal_service->apex_base_name.empty() && !hal_service->apex_full_name.empty()) {
            HalServiceApexSelections[hal_service->apex_base_name] = hal_service->apex_full_name;
        }
        for (const auto& vf : hal_service->vintf_fragments) {
            if (!fs::copy_file(kVintfSrcDir + vf, kVintfDestDir + vf, ec)) {
                LOG(ERROR) << "Failed to copy vintf fragment " << vf;
                ret = false;
            }
        }
    } else {
        for (const auto& svc : hal_service->init_rc_services) {
            if (!SetProperty(kCtlStopProp, svc)) {
                LOG(ERROR) << "Failed to stop service " << svc;
                ret = false;
            }
        }
    }
    return ret;
}

bool ApplySelections(void) {
    ProcessBootOverrides();

    bool ret = true;
    const std::string* strp;

    if (gHwAudioPrimary != HwAudioPrimary::Unset) {
        strp = &kHwAudioPrimaryMap.at(gHwAudioPrimary);
        LOG(INFO) << "Set Audio primary module to " << *strp;
        ret &= SetProperty(kHwAudioPrimaryProp, *strp);
    } else {
        LOG(WARNING) << "Audio primary module is unset";
    }

    if (gUsbGadgetApex != UsbGadgetApex::Unset) {
        strp = &kUsbGadgetApexMap.at(gUsbGadgetApex);
        LOG(INFO) << "Set USB Gadget APEX to " << *strp;
        ret &= SetProperty(kUsbGadgetApexProp, *strp);
        if (gUsbGadgetApex == UsbGadgetApex::None) {
            ret &= SetProperty(kUsbAdbDisabledProp, "true");
        }
    } else {
        LOG(WARNING) << "USB Gadget APEX is unset";
        ret &= SetProperty(kUsbAdbDisabledProp, "true");
    }

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

    switch (gVulkanApex) {
        case VulkanApex::Unset:
            gVulkanApex = VulkanApex::No_apex;
            break;
        case VulkanApex::No_apex:
            break;
        case VulkanApex::Swiftshader:
            // Swiftshader APEX will set vulkan module by itself
            gHwVulkan = HwVulkan::Unset;
            break;
    }

    if (gHwVulkan != HwVulkan::Unset) {
        strp = &kHwVulkanMap.at(gHwVulkan);
        LOG(INFO) << "Set Vulkan module to " << *strp;
        ret &= SetProperty(kHwVulkanProp, *strp);
    } else {
        LOG(WARNING) << "Vulkan module is unset";
    }

    if (gVulkanApex != VulkanApex::Unset) {
        strp = &kVulkanApexMap.at(gVulkanApex);
        LOG(INFO) << "Set Vulkan APEX to " << *strp;
        ret &= SetProperty(kVulkanApexProp, *strp);
    } else {
        LOG(WARNING) << "Vulkan APEX is unset";
    }

    if (gGrallocHalService != GrallocHalServices::Unset) {
        for (const auto& [ map_key, map_value ] : kGrallocHalServiceMap) {
            if (map_key == gGrallocHalService) {
                LOG(INFO) << "Set Graphics Allocator HAL service to " << map_value->name;
            }
            ret &= EnableHalService(map_value, map_key == gGrallocHalService);
        }
    } else {
        LOG(WARNING) << "Graphics Allocator HAL service is unset";
    }

    if (gHwcHalService != HwcHalServices::Unset) {
        for (const auto& [ map_key, map_value ] : kHwcHalServiceMap) {
            if (map_key == gHwcHalService) {
                LOG(INFO) << "Set Graphics Composer HAL service to " << map_value->name;
            }
            ret &= EnableHalService(map_value, map_key == gHwcHalService);
        }
    } else {
        LOG(WARNING) << "Graphics Composer HAL service is unset";
    }

    // Enablue blur if not using Swiftshader graphics
    if (gVulkanApex != VulkanApex::Swiftshader) {
        LOG(INFO) << "Enable blur";
        ret &= SetProperty(kSfSupportsBackgroundBlurProp, "1");
    }

    for (const auto& [ apex_base_name, apex_full_name ] : HalServiceApexSelections) {
        ret &= SetProperty(kApexSelectPropPrefix + apex_base_name, apex_full_name);
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

    gGrallocHalService = GrallocHalServices::V2_0;
    gHwcHalService = HwcHalServices::V2_2;

    gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_BGRA_8888;

    UseSwiftshaderGraphics();
}

void DrmSysfbCard(void) {
    LOG(INFO) << "Detected DRM sysfb card";
    SetupFramebufferDisplay();
}

void DrmUnknownCard(const std::string& card_name) {
    LOG(WARNING) << "DRM card is not directly supported";

    const std::unordered_map<std::string, HwVulkan> kCardNameToHwVulkanMap = {
        // TODO: HwVulkan::Imagination require matching card&render pair, see pvr_drm_configs[] in mesa
        {"mediatek", HwVulkan::Imagination},
        {"tidss", HwVulkan::Imagination},
    };
    if (kCardNameToHwVulkanMap.contains(card_name)) {
        gHwVulkan = kCardNameToHwVulkanMap.at(card_name);
    }
}

void DrmUnknownRender(const std::string& render_name) {
    LOG(WARNING) << "DRM render is not directly supported";

    const std::unordered_map<std::string, HwVulkan> kRenderNameToHwVulkanMap = {
        {"asahi", HwVulkan::Asahi},
        {"panfrost", HwVulkan::Panfrost},
        {"panthor", HwVulkan::Panfrost},
        {"v3d", HwVulkan::Broadcom},
    };
    if (kRenderNameToHwVulkanMap.contains(render_name)) {
        gHwVulkan = kRenderNameToHwVulkanMap.at(render_name);
    }
}

void DrmAmdgpuRender(void) {
    gMinigbmGenericBackend = MinigbmGenericBackend::DumbGeneric;
    gGlesVersion = kGlesVersion32;
    gHwVulkan = HwVulkan::Radeon;
}

void DrmI915(int fd, bool is_render) {
    int ret = 0;

    if (is_render) {
        if (IsForcedSwiftshader()) {
            UseSwiftshaderGraphics();
        } else {
            gGlesVersion = kGlesVersion32;
            gHwVulkan = HwVulkan::Intel;  // May get overridden later
        }
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
        if (!is_render && (value < 0x1902 && value != 0x0f31)) {
            // From Intel Core to pre-Skylake (HD Graphics 510)
            // Except for Atom Processor Z36xxx/Z37xxx
            SetProperty("ro.vendor.hwc.drm.avoid_using_alpha_bits_for_framebuffer", "1");
        }
        if (is_render && (value <= 0x0F33 ||
            (value >= 0x1602 && value <= 0x162E) ||
            (value >= 0x22B0 && value <= 0x22B3))) {
            // Approximate of gen7_ids and gen8_ids according to minigbm/i915.c
            gHwVulkan = HwVulkan::Intel_hasvk;
        }
        // What about pre Intel Core? Those won't even boot...
    } else {
        LOG(ERROR) << "Failed to get I915_PARAM_CHIPSET_ID";
    }
}

void DrmMsmCard(int card_fd) {
    gHwVulkan = HwVulkan::Freedreno;

    if (IsForcedSwiftshader()) {
        SetProperty("vendor.minigbm.debug", "nocompression");
    }

    // device/mainline/qcom-common/services/msm_drm_quirks begin
    struct fd_device* dev;
    struct fd_pipe* pipe;

    dev = fd_device_new(card_fd);
    if (!dev) {
        LOG(ERROR) << "fd_device_new() failed";
        goto err_fd_device_new;
    }

    pipe = fd_pipe_new(dev, FD_PIPE_3D);
    if (!pipe) {
        LOG(ERROR) << "fd_pipe_new() failed";
        goto err_fd_pipe_new;
    }

    uint64_t chip_id, gpu_id;
    fd_pipe_get_param(pipe, FD_CHIP_ID, &chip_id);
    fd_pipe_get_param(pipe, FD_GPU_ID, &gpu_id);
    LOG(INFO) << __FUNCTION__ << ": chip_id = " << std::to_string(chip_id)
              << " gpu_id = " << std::to_string(gpu_id);

    // Adreno 5xx Mesa Freedreno quirks
    if (gpu_id >= 500 && gpu_id <= 599) {
        SetProperty("vendor.mesa.fd.mesa.debug", "sysmem");
    }

    // Let minigbm avoid UBWC for pre Adreno 6xx
    if (gpu_id < 600) {
        SetProperty("vendor.minigbm.avoid_ubwc", "true");
    }

    fd_pipe_del(pipe);
err_fd_pipe_new:
    fd_device_del(dev);
err_fd_device_new:
    // device/mainline/qcom-common/services/msm_drm_quirks end
    return;
}

void DrmNouveauRender(void) {
    gGlesVersion = kGlesVersion31;
    gHwVulkan = HwVulkan::Nouveau;
}

void DrmQxlCard(void) {
    SetupFramebufferDisplay();
}

void DrmRadeonRender(void) {
    gMinigbmGenericBackend = MinigbmGenericBackend::DumbGeneric;
    gGlesVersion = kGlesVersion31;
}

void DrmVirtiogpu(int fd, bool is_render, const std::string& render_name) {
    bool value_3d_features = false;
    int ret = 0;

    uint32_t value;
    struct drm_virtgpu_getparam get_param = {
            .value = (uint64_t)(uintptr_t)&value,
    };

    get_param.param = VIRTGPU_PARAM_3D_FEATURES;
    ret = drmIoctl(fd, DRM_IOCTL_VIRTGPU_GETPARAM, &get_param);
    if (!ret) {
        value_3d_features = !!value;
    } else {
        LOG(ERROR) << "Failed to get 3D features parameter from virtio_gpu";
    }

    if (render_name.empty() ||
        (render_name == "virtio_gpu" && !value_3d_features)) {
        gSfNativeWindowBuffersFormat = HAL_PIXEL_FORMAT_BGRA_8888;
    }

    if (is_render) {
        if (value_3d_features) {
            gHwEgl = HwEgl::Mesa;
            gHwVulkan = HwVulkan::Virtio;
            gGlesVersion = kGlesVersion32;
        } else {
            UseSwiftshaderGraphics();
        }
    }
}

void DrmVmwgfxCard(void) {
    std::string smbios_product_name;
    ReadFileToString(kDmiIdPath + "product_name", &smbios_product_name);
    if (!smbios_product_name.empty()) smbios_product_name.pop_back();

    if (smbios_product_name == "VirtualBox" || IsForcedSwiftshader()) {
        // Swiftshader does not display directly via DRM
        // 3D acceleration does not work on VirtualBox
        SetupFramebufferDisplay();
    }
}

void DrmVmwgfxRender(void) {
    std::string smbios_product_name;
    ReadFileToString(kDmiIdPath + "product_name", &smbios_product_name);
    if (!smbios_product_name.empty()) smbios_product_name.pop_back();

    if (smbios_product_name == "VirtualBox" || IsForcedSwiftshader()) {
        // DRM render won't be used on VirtualBox
        // Forced Swiftshader does not need us setting gles version
    } else {
        gGlesVersion = kGlesVersion31;
    }
}

bool IsDrmMultiplePlanesSupported(int fd) {
    drmModePlaneRes* planeRes = drmModeGetPlaneResources(fd);
    if (!planeRes) {
        LOG(ERROR) << "Failed to get plane resources";
        return false;
    }

    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
        LOG(ERROR) << "Failed to get DRM resources";
        drmModeFreePlaneResources(planeRes);
        return false;
    }

    bool supportsMultiplePlanes = false;

    // For each CRTC, count how many planes can attach
    for (int crtc_idx = 0; crtc_idx < res->count_crtcs; ++crtc_idx) {
        int planeCountForCrtc = 0;

        for (uint32_t i = 0; i < planeRes->count_planes; ++i) {
            drmModePlane* plane = drmModeGetPlane(fd, planeRes->planes[i]);
            if (!plane) continue;

            if (plane->possible_crtcs & (1 << crtc_idx)) {
                planeCountForCrtc++;
            }

            drmModeFreePlane(plane);
        }

        if (planeCountForCrtc > 1) {
            supportsMultiplePlanes = true;
            break;
        }
    }

    drmModeFreeResources(res);
    drmModeFreePlaneResources(planeRes);

    return supportsMultiplePlanes;
}

void SetDefaultsForDrmDisplay(int card_fd, int render_fd) {
    /*
     * Gralloc
     *
     * What else is there other than minigbm-upstream for now?
     * - Unmodified external/minigbm (Quite useless for a generic port)
     * - Standalone Mesa GBM gralloc (Our minigbm currently have Mesa GBM backend)
     * - HBM gralloc (standalone form is still a draft, minigbm backend form is used only for amdgpu for now)
     * - DRM gralloc (RIP)
     */
    gGrallocHalService = GrallocHalServices::MinigbmUpstream;
    gHwGralloc = HwGralloc::MinigbmUpstream;

    /*
     * HwComposer
     *
     * drmfb-composer for legacy hardware that does not support
     * certain capabilities, according to its README
     *
     * drm_hwcomposer-upstream for full-fledged support
     * (however we have some hacks in there too)
     */
    if (!drmSetClientCap(card_fd, DRM_CLIENT_CAP_ATOMIC, 1)) {
        gHwcHalService = HwcHalServices::DrmUpstream;
        if (!IsDrmMultiplePlanesSupported(card_fd)) {
            SetProperty("ro.vendor.hwc.drm.disable_planes", "1");
        }
    } else {
        gHwcHalService = HwcHalServices::DrmFb;
    }

    /*
     * Graphics
     *
     * Mesa is the only one for hardware rendering
     * Swiftshader is only for software rendering
     *
     * Mesa may have software rendering backend built-in, and
     * performance wise it's bit better than Swiftshader,
     * but compatibility wise is still not as good as Swiftshader
     */
    if (render_fd >= 0) {
        gHwEgl = HwEgl::Mesa;
    } else {
        UseSwiftshaderGraphics();
    }
}

void DetectGraphics(void) {
    std::string card_path, render_path;
    int card_fd = -1, render_fd = -1;
    drmVersionPtr card_version = nullptr, render_version = nullptr;
    bool card_is_sysfb = false;
    std::string card_name, render_name;
    std::list<unsigned int> drm_sysfb_cards;

    if (IsForcedFramebufferDisplay()) {
        LOG(INFO) << "Forced using framebuffer display";
        SetupFramebufferDisplay();
        return;
    }

    for (uint32_t i = 0; i < DRM_MAX_MINOR; i++) {
        card_path = "/dev/dri/card" + std::to_string(i);
        card_fd = open(card_path.c_str(), O_RDONLY);
        if (card_fd >= 0) {
            card_version = drmGetVersion(card_fd);
            if (!card_version) {
                LOG(ERROR) << "Failed to get DRM version for card " << std::to_string(i);
                close(card_fd);
                card_fd = -1;
                continue;
            }

            card_name = std::string(card_version->name, card_version->name_len);
            LOG(INFO) << "Card " << std::to_string(i) << " is " << card_name;

            // Save DRM sysfb card, and close it for now
            if (kDrmSysfbNames.find(card_name) != kDrmSysfbNames.end()) {
                drm_sysfb_cards.push_back(i);
                drmFreeVersion(card_version);
                close(card_fd);
                card_fd = -1;
                continue;
            }

            break;
        }
    }

    for (uint32_t i = 0; i < DRM_MAX_MINOR; i++) {
        render_path = "/dev/dri/renderD" + std::to_string(128 + i);
        render_fd = open(card_path.c_str(), O_RDONLY);
        if (render_fd >= 0) {
            render_version = drmGetVersion(render_fd);
            if (!render_version) {
                LOG(ERROR) << "Failed to get DRM version for render " << std::to_string(i);
                close(render_fd);
                render_fd = -1;
                continue;
            }

            render_name = std::string(render_version->name, render_version->name_len);
            LOG(INFO) << "Render " << std::to_string(i) << " is " << render_name;

            break;
        }
    }

    // If we have sysfb cards only
    if (card_fd < 0 && !drm_sysfb_cards.empty()) {
        card_is_sysfb = true;
        for (const auto& i : drm_sysfb_cards) {
            card_path = "/dev/dri/card" + std::to_string(i);
            card_fd = open(card_path.c_str(), O_RDONLY);
            if (card_fd >= 0) {
                card_version = drmGetVersion(card_fd);
                if (!card_version) {
                    LOG(ERROR) << "Failed to get DRM version for card " << std::to_string(i);
                    close(card_fd);
                    card_fd = -1;
                    continue;
                }
                card_name = std::string(card_version->name, card_version->name_len);
            }
        }
    }

    if (card_fd < 0) {
        LOG(ERROR) << "Failed to open any DRM card device, falling back to framebuffer display";
        SetupFramebufferDisplay();
        return;
    }

    SetProperty(kGraphicsCardNameProp, card_name);
    SetProperty(kGraphicsRenderNameProp, render_name);

    SetDefaultsForDrmDisplay(card_fd, render_fd);

    // DRM HWC tries the first or the specified card node
    SetProperty(kHwcDrmDeviceProp, card_path);

    // Minigbm tries the first render node, and then the first card node

    // Card
    if (kMustUseFbDisplayCards.find(card_name) != kMustUseFbDisplayCards.end()) {
        LOG(INFO) << "This DRM card must use framebuffer display for now";
        SetupFramebufferDisplay();
    } else if (card_is_sysfb) {
        DrmSysfbCard();
    } else if (card_name == "amdgpu") {
        // nothing
    } else if (card_name == "i915") {
        DrmI915(card_fd, false);
    } else if (card_name == "msm") {
        DrmMsmCard(card_fd);
    } else if (card_name == "nouveau") {
        // nothing
    } else if (card_name == "qxl") {
        DrmQxlCard();
    } else if (card_name == "radeon") {
        // nothing
    } else if (card_name == "virtio_gpu") {
        DrmVirtiogpu(card_fd, false, render_name);
    } else if (card_name == "vmwgfx") {
        DrmVmwgfxCard();
    } else {
        DrmUnknownCard(card_name);
    }

    // Render
    if (render_fd < 0) {
        LOG(INFO) << "No DRM render found";
    } else if (render_name == "amdgpu") {
        DrmAmdgpuRender();
    } else if (render_name == "i915") {
        DrmI915(render_fd, true);
    } else if (render_name == "msm") {
        // nothing
    } else if (render_name == "nouveau") {
        DrmNouveauRender();
    } else if (render_name == "radeon") {
        DrmRadeonRender();
    } else if (render_name == "virtio_gpu") {
        DrmVirtiogpu(render_fd, true, render_name);
    } else if (render_name == "vmwgfx") {
        DrmVmwgfxRender();
    } else {
        DrmUnknownRender(render_name);
    }

    if (card_version) drmFreeVersion(card_version);
    if (render_version) drmFreeVersion(render_version);
    if (card_fd >= 0) close(card_fd);
    if (render_fd >= 0) close(render_fd);
}

}  // namespace

int main(int, char* argv[]) {
    InitLogging(argv, &KernelLogger);
    umask(000);

    if (access("/dev/snd/pcmC0D0p", F_OK) == 0) {
        LOG(INFO) << "Sound card 0 device 0 playback is present, enable audio output";
        gHwAudioPrimary = HwAudioPrimary::Tinyhal;
    }

    if (GetProperty(kUsbControllerProp, "").empty()) {
        gUsbGadgetApex = UsbGadgetApex::None;
    } else {
        LOG(INFO) << "Detected USB controller, enable USB Gadget support";
        gUsbGadgetApex = UsbGadgetApex::Mainline;
    }

    DetectGraphics();

    return ApplySelections() ? EXIT_SUCCESS : EXIT_FAILURE;
}
