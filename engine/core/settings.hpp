#pragma once

#include "core/types.hpp"

#include <string>

namespace ac3 {
struct Settings {
    u32 resolution = 3;
    u32 antialias = 0;
    u32 display = 0;
    u32 vsync = 1;
    u32 perspective = 1;
    u32 precision = 1;
    u32 hd_textures = 1;
    u32 hd_lighting = 1;
    u32 ground_filter = 1;
    u32 model_depth = 1;
    u32 water = 1;
    u32 widescreen = 0;
    u32 view_distance = 0;
    u32 fog = 1;
    u32 hdr_sun = 0;
    u32 lens_flare = 0;
    u32 frame_rate = 1;
    u32 language = 1;
    u32 normal_controls = 1;
    u32 analog_sticks = 1;

    bool changed = false;
};

struct SettingInfo {
    const char* key;
    const char* name;
    u32 page;
    u32 Settings::*field;
    u32 count;
    const char* labels[8];
};

extern const SettingInfo kSettings[];
extern const u32 kSettingCount;

inline u32 resolution_scale(const Settings& s) { return s.resolution + 1; }

float aspect(const Settings& s);

bool load_settings(const std::string& path, Settings& out);
bool save_settings(const std::string& path, const Settings& s);
}
