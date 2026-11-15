#pragma once

#include "core/settings.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "render/terrain.hpp"

#include <functional>
#include <string>

namespace ac3::sim { class Game; }
namespace ac3::machine { class Machine; }

namespace ac3 {
struct PlayContext {
    std::string state_path;
    platform::Window* window = nullptr;
    render::Renderer* renderer = nullptr;
    render::TerrainRenderer* terrain = nullptr;
    bool headless = false;
    long frames = 0;
};

int play(const PlayContext& ctx);

struct GameContext {
    std::string disc;
    std::string cards_dir;
    std::string input;
    platform::Window* window = nullptr;
    render::Renderer* renderer = nullptr;
    bool headless = false;
    bool audio = true;
    Settings* settings = nullptr;
    std::string settings_path;
    std::string textures_dir;
    std::string english_patch;
    bool shot_mid = false;
    u32 vram_check = 0;
    std::string vram_dump;
    bool trace_phases = false;
    bool perf = false;
    bool trace_screens = false;
    std::string pad_script;
    std::string shot_at, shot_path;
    long frames = 0;
    std::function<void(sim::Game&, machine::Machine&, u32 next)> on_frame;
    std::function<bool()> on_loop;
    const bool* turbo = nullptr;
    u32 input_until_phase = 0;
};

int play_game(const GameContext& ctx);
}
