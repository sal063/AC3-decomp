#include "play.hpp"

#include "core/settings.hpp"

#include <algorithm>

#include "assets/mission.hpp"
#include "assets/vram.hpp"
#include "core/log.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "render/terrain.hpp"
#include "vfs/ace_discs.hpp"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>

using namespace ac3;

namespace {
struct Args {
    bool validation = false, vsync = true;
    bool game = false, audio = true, trace_phases = false, perf = false, trace_screens = false;
    u32 hd = 0, vram_check = 0;
    bool original = false;
    bool shot_mid = false;
    int language = -1;
    std::string input, cards, vram_dump, settings, pad_script, shot_at, english_patch;
    long frames = 0;
    std::string disc, screenshot, play;
    int mission = 1;
    u32 headless_w = 0, headless_h = 0;
    std::optional<render::Camera> camera;
    float heading = 0, pitch = 15;
};

bool parse(int argc, char** argv, Args& a) {
    auto num = [&](int& i) { return static_cast<float>(std::atof(argv[++i])); };
    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        const int left = argc - 1 - i;
        if (!std::strcmp(s, "--validation")) a.validation = true;
        else if (!std::strcmp(s, "--no-vsync")) a.vsync = false;
        else if (!std::strcmp(s, "--frames") && left >= 1) a.frames = std::strtol(argv[++i], nullptr, 10);
        else if (!std::strcmp(s, "--disc") && left >= 1) a.disc = argv[++i];
        else if (!std::strcmp(s, "--play") && left >= 1) a.play = argv[++i];
        else if (!std::strcmp(s, "--game")) a.game = true;
        else if (!std::strcmp(s, "--no-audio")) a.audio = false;
        else if (!std::strcmp(s, "--input") && left >= 1) a.input = argv[++i];
        else if (!std::strcmp(s, "--cards") && left >= 1) a.cards = argv[++i];
        else if (!std::strcmp(s, "--hd") && left >= 1) a.hd = static_cast<u32>(std::strtoul(argv[++i], nullptr, 10));
        else if (!std::strcmp(s, "--original")) a.original = true;
        else if (!std::strcmp(s, "--english")) a.language = 1;
        else if (!std::strcmp(s, "--japanese")) a.language = 0;
        else if (!std::strcmp(s, "--english-patch") && left >= 1) a.english_patch = argv[++i];
        else if (!std::strcmp(s, "--vram-check") && left >= 1)
            a.vram_check = static_cast<u32>(std::strtoul(argv[++i], nullptr, 10));
        else if (!std::strcmp(s, "--vram-dump") && left >= 1) a.vram_dump = argv[++i];
        else if (!std::strcmp(s, "--trace-phases")) a.trace_phases = true;
        else if (!std::strcmp(s, "--perf")) a.perf = true;
        else if (!std::strcmp(s, "--trace-screens")) a.trace_screens = true;
        else if (!std::strcmp(s, "--pad-script") && left >= 1) a.pad_script = argv[++i];
        else if (!std::strcmp(s, "--shot-at") && left >= 1) a.shot_at = argv[++i];
        else if (!std::strcmp(s, "--shot-mid")) a.shot_mid = true;
        else if (!std::strcmp(s, "--settings") && left >= 1) a.settings = argv[++i];
        else if (!std::strcmp(s, "--mission") && left >= 1) a.mission = std::atoi(argv[++i]);
        else if (!std::strcmp(s, "--screenshot") && left >= 1) a.screenshot = argv[++i];
        else if (!std::strcmp(s, "--headless") && left >= 1) {
            const char* v = argv[++i];
            a.headless_w = static_cast<u32>(std::strtoul(v, nullptr, 10));
            const char* x = std::strchr(v, 'x');
            a.headless_h = x ? static_cast<u32>(std::strtoul(x + 1, nullptr, 10)) : 0;
            if (!a.headless_w || !a.headless_h) return log::error("--headless wants WxH"), false;
        } else if (!std::strcmp(s, "--camera") && left >= 5) {
            const float x = num(i), y = num(i), z = num(i);
            a.heading = num(i);
            a.pitch = num(i);
            a.camera = render::Camera::from_angles(x, y, z, a.heading, a.pitch);
        } else if (!std::strcmp(s, "--camera-game") && left >= 13) {
            render::Camera c;
            for (float& r : c.rot) r = num(i);
            for (float& p : c.pos) p = num(i);
            c.h = num(i);
            a.camera = c;
        } else {
            log::error("unknown or incomplete argument: {}", s);
            return false;
        }
    }
    return true;
}

void fly(const platform::Window& w, float dt, float look_x, float look_y, float pos[3], float& heading,
         float& pitch) {
    heading += look_x * 0.2f;
    pitch = std::fmax(-89.0f, std::fmin(89.0f, pitch + look_y * 0.2f));
    const float speed = (w.key(SDL_SCANCODE_LSHIFT) ? 40000.0f : 4000.0f) * dt;
    const float yaw = heading * 0.0174532925f;
    const float fx = std::sin(yaw), fz = std::cos(yaw);
    if (w.key(SDL_SCANCODE_W)) { pos[0] += fx * speed; pos[2] += fz * speed; }
    if (w.key(SDL_SCANCODE_S)) { pos[0] -= fx * speed; pos[2] -= fz * speed; }
    if (w.key(SDL_SCANCODE_D)) { pos[0] += fz * speed; pos[2] -= fx * speed; }
    if (w.key(SDL_SCANCODE_A)) { pos[0] -= fz * speed; pos[2] += fx * speed; }
    if (w.key(SDL_SCANCODE_SPACE)) pos[1] -= speed;
    if (w.key(SDL_SCANCODE_LCTRL)) pos[1] += speed;
}
}

int main(int argc, char** argv) {
    Args args;
    if (!parse(argc, argv, args)) return 2;
    const bool headless = args.headless_w != 0;

    const std::string prefs = platform::pref_path();
    if (!prefs.empty() && !log::open_file((prefs + "ac3engine.log").c_str()))
        log::warn("cannot open the log file in {}", prefs);
    const int sdl = SDL_GetVersion();
    log::info("ac3engine 0.1 -- SDL {}.{}.{}", SDL_VERSIONNUM_MAJOR(sdl), SDL_VERSIONNUM_MINOR(sdl),
              SDL_VERSIONNUM_MICRO(sdl));

    const std::string settings_path =
        !args.settings.empty() ? args.settings : prefs.empty() ? std::string() : prefs + "settings.ini";
    Settings settings;
    if (!settings_path.empty() && !load_settings(settings_path, settings)) save_settings(settings_path, settings);
    Settings session = settings;
    if (args.hd) session.resolution = std::clamp(args.hd, 1u, 8u) - 1;
    if (args.original) {
        session.resolution = 0;
        session.antialias = 0;
        session.perspective = 0;
        session.precision = 0;
        session.hd_textures = 0;
        session.ground_filter = 0;
        session.water = 0;
        session.model_depth = 0;
        session.frame_rate = 0;
        session.language = 0;
    }
    if (!args.input.empty()) session.language = 0;
    if (args.language >= 0) session.language = static_cast<u32>(args.language);
    if (!args.vsync) session.vsync = 0;

    platform::Window window;
    if (!headless && !window.create("Ace Combat 3 -- native engine", 1280, 720)) return 1;
    if (!headless && args.game && session.display) window.set_fullscreen(true);

    render::Renderer renderer;
    render::Config rc;
    rc.validation = args.validation;
    rc.vsync = args.game ? session.vsync != 0 : args.vsync;
    rc.headless_width = args.headless_w;
    rc.headless_height = args.headless_h;
    if (!renderer.init(headless ? nullptr : window.handle(), rc)) return 1;
    const render::DeviceInfo& dev = renderer.device();
    log::info("vulkan: {} (API {}.{}.{}, driver {}){}", dev.name, dev.api_major, dev.api_minor, dev.api_patch,
              dev.driver, headless ? ", headless" : "");

    if (args.game) {
        if (args.disc.empty()) return log::error("--game needs --disc"), 2;
        GameContext gc;
        gc.disc = args.disc;
        gc.cards_dir = args.cards.empty() ? prefs + "cards/" : args.cards + "/";
        SDL_CreateDirectory(gc.cards_dir.c_str());
        gc.input = args.input;
        gc.window = headless ? nullptr : &window;
        gc.renderer = &renderer;
        gc.headless = headless;
        gc.audio = args.audio;
        gc.settings = &session;
        if (!args.hd && !args.original && args.vsync && args.language < 0 && args.input.empty()) gc.settings_path = settings_path;
        gc.english_patch = args.english_patch;
        if (const char* base = SDL_GetBasePath()) gc.textures_dir = std::string(base) + "textures";
        gc.vram_check = args.vram_check;
        gc.vram_dump = args.vram_dump;
        gc.trace_phases = args.trace_phases;
        gc.perf = args.perf;
        gc.trace_screens = args.trace_screens;
        gc.pad_script = args.pad_script;
        gc.shot_at = args.shot_at;
        gc.shot_mid = args.shot_mid;
        gc.shot_path = args.screenshot;
        gc.frames = args.frames;
        const int rc2 = play_game(gc);
        if (!args.screenshot.empty() && renderer.screenshot(args.screenshot)) log::info("screenshot: {}", args.screenshot);
        renderer.shutdown();
        if (!headless) window.destroy();
        SDL_Quit();
        log::close_file();
        return rc2;
    }

    render::TerrainRenderer terrain;
    bool have_terrain = false;
    if (!args.disc.empty()) {
        vfs::AceDiscs discs;
        if (!discs.mount(args.disc)) return log::error("{}", discs.error()), 1;
        std::string err;
        auto mission = assets::load_mission(discs, args.mission, &err);
        if (!mission) return log::error("mission {}: {}", args.mission, err), 1;
        assets::Vram vram;
        for (const auto& t : assets::boot_textures(discs)) vram.upload_tim(t);
        assets::upload_mission_textures(*mission, vram);
        render::Target t = renderer.target();
        if (!terrain.init(renderer.gpu(), t, *mission, vram)) return 1;
        have_terrain = true;
        log::info("mission {}: {} tile instances, {} triangles", args.mission, terrain.instances(), terrain.triangles());
    }

    if (!args.play.empty()) {
        PlayContext pc;
        pc.state_path = args.play;
        pc.window = headless ? nullptr : &window;
        pc.renderer = &renderer;
        pc.terrain = have_terrain ? &terrain : nullptr;
        pc.headless = headless;
        pc.frames = args.frames;
        const int rc2 = play(pc);
        if (!args.screenshot.empty() && renderer.screenshot(args.screenshot)) log::info("screenshot: {}", args.screenshot);
        terrain.destroy();
        renderer.shutdown();
        if (!headless) window.destroy();
        SDL_Quit();
        log::close_file();
        return rc2;
    }

    float pos[3] = {0.0f, -12000.0f, -60000.0f};
    float heading = args.heading, pitch = args.pitch;
    render::Camera cam = args.camera ? *args.camera : render::Camera::from_angles(pos[0], pos[1], pos[2], heading, pitch);
    for (int i = 0; i < 3; ++i) pos[i] = cam.pos[i];

    const float sky[3] = {0.38f, 0.55f, 0.80f};
    long presented = 0;
    const Uint64 t0 = SDL_GetTicksNS();
    Uint64 last = t0;
    while (headless || window.pump()) {
        if (!headless) {
            if (window.take_resized()) renderer.resized();
            if (window.minimized()) {
                window.wait(100);
                continue;
            }
            const Uint64 now = SDL_GetTicksNS();
            const float dt = static_cast<float>(now - last) / 1e9f;
            last = now;
            float lx = 0, ly = 0;
            window.take_look(lx, ly);
            if (!args.camera || lx || ly || window.key(SDL_SCANCODE_W) || window.key(SDL_SCANCODE_S) ||
                window.key(SDL_SCANCODE_A) || window.key(SDL_SCANCODE_D)) {
                fly(window, dt, lx, ly, pos, heading, pitch);
                cam = render::Camera::from_angles(pos[0], pos[1], pos[2], heading, pitch, cam.h);
                args.camera.reset();
            }
        }
        const bool drew = renderer.frame(sky, [&](VkCommandBuffer cb, const render::Target& t) {
            if (have_terrain) terrain.draw(cb, t, cam);
        });
        if (drew) ++presented;
        if (args.frames > 0 && presented >= args.frames) break;
        if (headless && args.frames <= 0) break;
    }
    const double secs = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9;
    log::info("presented {} frames in {:.2f} s ({:.1f} fps)", presented, secs,
              secs > 0 ? static_cast<double>(presented) / secs : 0.0);
    if (!args.screenshot.empty()) {
        if (renderer.screenshot(args.screenshot)) log::info("screenshot: {}", args.screenshot);
        else log::error("screenshot failed");
    }
    terrain.destroy();
    renderer.shutdown();
    if (!headless) window.destroy();
    SDL_Quit();
    log::close_file();
    return 0;
}
