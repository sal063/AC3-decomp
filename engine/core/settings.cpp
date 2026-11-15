#include "core/settings.hpp"

#include "core/file.hpp"

#include <cstdlib>
#include <cstring>
#include <string>

namespace ac3 {
const SettingInfo kSettings[] = {
    {"resolution", "RESOLUTION", 0, &Settings::resolution, 8, {"1X", "2X", "3X", "4X", "5X", "6X", "7X", "8X"}},
    {"antialias", "ANTI ALIASING", 0, &Settings::antialias, 3, {"OFF", "2X SSAA", "4X SSAA"}},
    {"display", "DISPLAY", 0, &Settings::display, 2, {"WINDOW", "FULLSCREEN"}},
    {"vsync", "VSYNC", 0, &Settings::vsync, 2, {"OFF", "ON"}},
    {"perspective", "TEXTURES", 0, &Settings::perspective, 2, {"PS1 WARP", "CORRECT"}},
    {"precision", "GEOMETRY", 0, &Settings::precision, 2, {"PS1 WOBBLE", "PRECISE"}},
    {"hd_textures", "HD TEXTURES", 0, &Settings::hd_textures, 2, {"OFF", "ON"}},
    {"hd_lighting", "HD LIGHTING", 0, &Settings::hd_lighting, 2, {"OFF", "ON"}},
    {"ground_filter", "GROUND FILTER", 0, &Settings::ground_filter, 2, {"NEAREST", "BILINEAR"}},
    {"model_depth", "MODEL DEPTH", 0, &Settings::model_depth, 2, {"PS1 SORT", "EXACT"}},
    {"widescreen", "WIDESCREEN", 1, &Settings::widescreen, 4, {"OFF", "16:9", "16:10", "21:9"}},
    {"view_distance", "VIEW DISTANCE", 1, &Settings::view_distance, 4, {"ORIGINAL", "FAR", "VERY FAR", "HORIZON"}},
    {"water", "WATER", 1, &Settings::water, 2, {"ORIGINAL", "ENHANCED"}},
    {"fog", "FOG", 1, &Settings::fog, 2, {"OFF", "ON"}},
    {"hdr_sun", "HDR SUN", 1, &Settings::hdr_sun, 2, {"OFF", "ON"}},
    {"lens_flare", "LENS FLARE", 1, &Settings::lens_flare, 2, {"ORIGINAL", "ENHANCED"}},
    {"frame_rate", "FRAME RATE", 1, &Settings::frame_rate, 2, {"ORIGINAL", "60 FPS"}},
    {"language", "LANGUAGE", 1, &Settings::language, 2, {"JAPANESE", "ENGLISH"}},
    {"normal_controls", "NORMAL CONTROLS", 1, &Settings::normal_controls, 2, {"ORIGINAL", "PITCH AND ROLL"}},
    {"analog_sticks", "ANALOG STICKS", 1, &Settings::analog_sticks, 2, {"ORIGINAL", "MODERN"}},
};
const u32 kSettingCount = sizeof kSettings / sizeof kSettings[0];

float aspect(const Settings& s) {
    switch (s.widescreen) {
    case 1: return 16.0f / 9.0f;
    case 2: return 16.0f / 10.0f;
    case 3: return 21.0f / 9.0f;
    default: return 4.0f / 3.0f;
    }
}

bool load_settings(const std::string& path, Settings& out) {
    out = Settings{};
    const auto bytes = file::read_all(path);
    if (!bytes) return false;
    const std::string text(bytes->begin(), bytes->end());
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (const size_t hash = line.find('#'); hash != std::string::npos) line.resize(hash);
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto trim = [](std::string v) {
            const size_t a = v.find_first_not_of(" \t\r");
            const size_t b = v.find_last_not_of(" \t\r");
            return a == std::string::npos ? std::string() : v.substr(a, b - a + 1);
        };
        const std::string key = trim(line.substr(0, eq)), value = trim(line.substr(eq + 1));
        for (u32 i = 0; i < kSettingCount; ++i) {
            const SettingInfo& o = kSettings[i];
            if (key != o.key) continue;
            for (u32 v = 0; v < o.count; ++v)
                if (value == o.labels[v]) out.*o.field = v;
            char* stop = nullptr;
            const unsigned long n = std::strtoul(value.c_str(), &stop, 10);
            if (stop && !*stop && !value.empty() && n < o.count) out.*o.field = static_cast<u32>(n);
        }
    }
    return true;
}

bool save_settings(const std::string& path, const Settings& s) {
    std::string text = "# AC3 native engine settings. Each value is one of the labels after it.\n";
    for (u32 i = 0; i < kSettingCount; ++i) {
        const SettingInfo& o = kSettings[i];
        text += std::string(o.key) + " = " + o.labels[s.*o.field] + "    #";
        for (u32 v = 0; v < o.count; ++v) text += std::string(" ") + o.labels[v] + (v + 1 < o.count ? " |" : "");
        text += "\n";
    }
    return file::write_text(path, text);
}
}
