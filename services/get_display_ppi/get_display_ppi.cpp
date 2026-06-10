/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
*/

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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

    for (const auto& entry : fs::directory_iterator(drm))
    {
        if (!entry.is_directory())
            continue;

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

int main()
{
    DisplayInfo display;

    if (!findFirstConnectedDisplay(display))
    {
        std::cerr << "No connected display with valid EDID found\n";
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

    std::cout << "Resolution : "
              << display.width_px << "x"
              << display.height_px << "\n";

    std::cout << "Physical size : "
              << display.width_cm << " cm x "
              << display.height_cm << " cm\n";

    std::cout << "PPI : " << ppi << "\n";

    return 0;
}
