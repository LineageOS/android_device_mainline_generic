/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#include <android-base/logging.h>
#include <android-base/properties.h>
#include <android-base/strings.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

constexpr char kLcdDensityProp[] = "ro.vendor.get_display_ppi.lcd_density";

using namespace android::base;

namespace fs = std::filesystem;

struct DisplayInfo
{
    int width_px = 0;
    int height_px = 0;
    int width_cm = 0;
    int height_cm = 0;
};

static bool readFile(const fs::path& path, std::vector<uint8_t>& data)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
        return false;

    data.assign(
        std::istreambuf_iterator<char>(f),
        std::istreambuf_iterator<char>());

    return !data.empty();
}

static bool parseEDID(const std::vector<uint8_t>& edid, DisplayInfo& info)
{
    if (edid.size() < 128)
        return false;

    // EDID bytes 21-22: physical size in cm
    info.width_cm  = edid[21];
    info.height_cm = edid[22];

    if (info.width_cm == 0 || info.height_cm == 0)
        return false;

    // First detailed timing descriptor starts at byte 54
    const size_t dtd = 54;

    uint16_t hactive =
        edid[dtd + 2] |
        ((edid[dtd + 4] & 0xF0) << 4);

    uint16_t vactive =
        edid[dtd + 5] |
        ((edid[dtd + 7] & 0xF0) << 4);

    if (hactive == 0 || vactive == 0)
        return false;

    info.width_px = hactive;
    info.height_px = vactive;

    return true;
}

static bool findFirstConnectedDisplay(DisplayInfo& info)
{
    const fs::path drm("/sys/class/drm");

    auto hwc_device = GetProperty("vendor.hwc.drm.device", "");
    if (!hwc_device.empty()) {
        hwc_device = hwc_device.substr(hwc_device.find_last_of('/') + 1);
        LOG(INFO) << __FUNCTION__ << "(): Filter " << hwc_device;
    }

    for (const auto& entry : fs::directory_iterator(drm))
    {
        if (!entry.is_directory())
            continue;

        std::string name = entry.path().filename();
        if (!hwc_device.empty()) {
            if (!StartsWith(name, hwc_device)) {
                LOG(INFO) << __FUNCTION__ << "(): Skipping " << name;
                continue;
            }
        }

        fs::path status = entry.path() / "status";
        fs::path edid   = entry.path() / "edid";

        if (!fs::exists(status) || !fs::exists(edid))
            continue;

        std::ifstream sf(status);
        std::string state;
        std::getline(sf, state);

        if (state != "connected")
            continue;

        std::vector<uint8_t> edidData;
        if (!readFile(edid, edidData))
            continue;

        if (parseEDID(edidData, info))
            return true;
    }

    return false;
}

int main(int, char* argv[])
{
    InitLogging(argv, &KernelLogger);

    DisplayInfo display;

    if (!findFirstConnectedDisplay(display))
    {
        LOG(ERROR) << "No connected display with valid EDID found";
        return 1;
    }

    double width_in  = display.width_cm / 2.54;
    double height_in = display.height_cm / 2.54;

    double pixel_diag =
        std::sqrt(
            static_cast<double>(display.width_px) * display.width_px +
            static_cast<double>(display.height_px) * display.height_px);

    double inch_diag =
        std::sqrt(
            width_in * width_in +
            height_in * height_in);

    double ppi = pixel_diag / inch_diag;

    LOG(INFO) << "Resolution : "
              << display.width_px << "x"
              << display.height_px;

    LOG(INFO) << "Physical size : "
              << display.width_cm << " cm x "
              << display.height_cm << " cm";

    LOG(INFO) << "PPI : " << ppi;

    int dpi = static_cast<int>(ppi);
    if (dpi < 160) dpi = 160;

    LOG(INFO) << "Set LCD density property to: " << dpi;
    if (!SetProperty(kLcdDensityProp, std::to_string(dpi))) {
        LOG(ERROR) << "Failed to set property";
        return 1;
    }

    return 0;
}
