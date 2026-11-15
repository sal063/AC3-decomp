#include "play.hpp"

#include "core/bytes.hpp"
#include "core/file.hpp"
#include "core/log.hpp"
#include "machine/input_recording.hpp"
#include "machine/machine.hpp"
#include "render/hd.hpp"
#include "sim/game/game.hpp"
#include "sim/gte.hpp"
#include "sim/memory.hpp"
#include "sim/precise.hpp"
#include "sim/state_file.hpp"
#include "vfs/disc_image.hpp"
#include "vfs/iso9660.hpp"

#include <SDL3/SDL.h>

#include <filesystem>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ac3 {
namespace {
constexpr u32 kPhase7 = 0x80033EF0u;
constexpr u32 kWorld = 0x800BCE54u;
constexpr double kFieldSeconds = 1001.0 / 60000.0;

struct Pads {
    SDL_Gamepad* pad = nullptr;

    void open() {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) return;
        int n = 0;
        SDL_JoystickID* ids = SDL_GetGamepads(&n);
        if (ids && n > 0) {
            pad = SDL_OpenGamepad(ids[0]);
            if (pad) log::info("gamepad: {}", SDL_GetGamepadName(pad));
        }
        SDL_free(ids);
    }
    void close() {
        if (pad) SDL_CloseGamepad(pad);
        pad = nullptr;
    }

    void read(const platform::Window& w, machine::PadInput& out) {
        using namespace machine;
        if (!pad) open();
        u16 b = 0;
        s32 lx = 0, ly = 0, rx = 0, ry = 0;
        if (pad) {
            auto btn = [&](SDL_GamepadButton g, u16 bit) {
                if (SDL_GetGamepadButton(pad, g)) b |= bit;
            };
            btn(SDL_GAMEPAD_BUTTON_SOUTH, kBtnCross);
            btn(SDL_GAMEPAD_BUTTON_EAST, kBtnCircle);
            btn(SDL_GAMEPAD_BUTTON_WEST, kBtnSquare);
            btn(SDL_GAMEPAD_BUTTON_NORTH, kBtnTriangle);
            btn(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, kBtnL1);
            btn(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, kBtnR1);
            btn(SDL_GAMEPAD_BUTTON_BACK, kBtnSelect);
            btn(SDL_GAMEPAD_BUTTON_START, kBtnStart);
            btn(SDL_GAMEPAD_BUTTON_LEFT_STICK, kBtnL3);
            btn(SDL_GAMEPAD_BUTTON_RIGHT_STICK, kBtnR3);
            btn(SDL_GAMEPAD_BUTTON_DPAD_UP, kBtnUp);
            btn(SDL_GAMEPAD_BUTTON_DPAD_DOWN, kBtnDown);
            btn(SDL_GAMEPAD_BUTTON_DPAD_LEFT, kBtnLeft);
            btn(SDL_GAMEPAD_BUTTON_DPAD_RIGHT, kBtnRight);
            if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 12000) b |= kBtnL2;
            if (SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 12000) b |= kBtnR2;
            lx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
            ly = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
            rx = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTX);
            ry = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_RIGHTY);
        }
        auto key = [&](SDL_Scancode k, u16 bit) {
            if (w.key(k)) b |= bit;
        };
        key(SDL_SCANCODE_X, kBtnCross);
        key(SDL_SCANCODE_C, kBtnCircle);
        key(SDL_SCANCODE_Z, kBtnSquare);
        key(SDL_SCANCODE_V, kBtnTriangle);
        key(SDL_SCANCODE_Q, kBtnL1);
        key(SDL_SCANCODE_E, kBtnR1);
        key(SDL_SCANCODE_1, kBtnL2);
        key(SDL_SCANCODE_3, kBtnR2);
        key(SDL_SCANCODE_RETURN, kBtnStart);
        key(SDL_SCANCODE_TAB, kBtnSelect);
        key(SDL_SCANCODE_UP, kBtnUp);
        key(SDL_SCANCODE_DOWN, kBtnDown);
        key(SDL_SCANCODE_LEFT, kBtnLeft);
        key(SDL_SCANCODE_RIGHT, kBtnRight);
        auto axis = [&](SDL_Scancode neg, SDL_Scancode pos, s32& v) {
            if (w.key(neg)) v = -32768;
            if (w.key(pos)) v = 32767;
        };
        axis(SDL_SCANCODE_A, SDL_SCANCODE_D, lx);
        axis(SDL_SCANCODE_W, SDL_SCANCODE_S, ly);
        axis(SDL_SCANCODE_J, SDL_SCANCODE_L, rx);
        axis(SDL_SCANCODE_I, SDL_SCANCODE_K, ry);
        auto to8 = [](s32 v) { return static_cast<u8>(std::clamp((v + 32768) >> 8, 0, 255)); };
        out.connected = true;
        out.type = 0x73;
        out.buttons = b;
        out.lx = to8(lx);
        out.ly = to8(ly);
        out.rx = to8(rx);
        out.ry = to8(ry);
    }
};

render::Camera game_camera(const sim::Memory& m) {
    render::Camera c;
    const u32 w = m.r32(kWorld);
    for (u32 i = 0; i < 9; ++i) c.rot[i] = static_cast<float>(m.r16s(w + 76 + 2 * i)) / 4096.0f;
    for (u32 i = 0; i < 3; ++i) c.pos[i] = static_cast<float>(m.r32s(w + 188 + 4 * i)) / 4096.0f;
    c.h = static_cast<float>(m.r16s(w + 220));
    if (c.h <= 0) c.h = 256.0f;
    return c;
}
}

int play(const PlayContext& ctx) {
    sim::StateFile file;
    std::string err;
    if (!file.open(ctx.state_path, &err)) return log::error("{}", err), 1;
    sim::MachineState st;
    bool found = false;
    while (file.next(st))
        if (st.target == kPhase7 && st.checkpoint == 16) {
            found = true;
            break;
        }
    if (!found) return log::error("{}: no normal-flight frame in it", ctx.state_path), 1;
    log::info("booting from {}: tick {}, field {}", ctx.state_path, st.tick, st.field);

    sim::Memory mem;
    std::memcpy(mem.ram(), st.ram.data(), sim::Memory::kRamSize);
    std::memcpy(mem.scratch(), st.scratch.data(), sim::Memory::kScratchSize);
    sim::Gte gte;
    gte.load(st.gte);
    sim::Game game(mem, gte);
    machine::Machine mach(mem);
    game.hw = &mach;
    game.sp = st.regs[29];
    mach.set_clock(st.field, st.field);

    Pads pads;
    if (!ctx.headless) pads.open();
    mach.poll_input = [&](u32, machine::PadInput p[2]) {
        if (!ctx.headless) pads.read(*ctx.window, p[0]);
        else p[0].connected = true;
    };

    const float sky[3] = {0.38f, 0.55f, 0.80f};
    u32 next = kPhase7;
    long frames = 0, presented = 0;
    const Uint64 t0 = SDL_GetTicksNS();
    const u32 field0 = mach.vblank();
    render::Camera cam = game_camera(mem);
    while (ctx.headless || ctx.window->pump()) {
        if (!ctx.headless) {
            if (ctx.window->take_resized()) ctx.renderer->resized();
            if (ctx.window->minimized()) {
                ctx.window->wait(100);
                continue;
            }
        }
        const double now = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9;
        const double game_time = (mach.vblank() - field0) * kFieldSeconds;
        if (ctx.headless || game_time <= now) {
            if (!game.run_phase(next)) {
                log::info("the game reached phase function {:08X}, which is not reimplemented yet", next);
                break;
            }
            next = game.main_loop(game.phase_result);
            ++frames;
            if (game.unported_hits && frames == 1) log::warn("unported code reached: see the log");
            cam = game_camera(mem);
        }
        const bool drew = ctx.renderer->frame(sky, [&](VkCommandBuffer cb, const render::Target& t) {
            if (ctx.terrain) ctx.terrain->draw(cb, t, cam);
        });
        if (drew) ++presented;
        if (ctx.frames > 0 && frames >= ctx.frames) break;
    }
    const double secs = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9;
    log::info("played {} frames ({} fields) in {:.2f} s; presented {}", frames, mach.vblank() - field0, secs, presented);
    if (game.unported_hits) log::warn("{} calls reached code not reimplemented yet", game.unported_hits);
    pads.close();
    return 0;
}

int play_game(const GameContext& ctx) {
    sim::Memory mem;
    sim::Gte gte;
    sim::Game game(mem, gte);
    machine::Machine mach(mem);
    game.hw = &mach;
    mach.call_guest = [&game](u32 fn, u32 a0, u32 a1) { game.call_guest(fn, a0, a1); };
    Settings defaults;
    Settings& settings = ctx.settings ? *ctx.settings : defaults;
    game.player_settings = &settings;
    sim::DrawHints hints;
    game.hints = &hints;
    std::string err;
    if (settings.language) {
        std::vector<std::string>& dirs = mach.cd.translation_dirs;
        if (!ctx.english_patch.empty()) dirs.push_back(ctx.english_patch);
        std::filesystem::path at = std::filesystem::absolute(std::filesystem::path(ctx.disc)).parent_path();
        for (int up = 0; up < 3 && !at.empty(); ++up) {
            dirs.push_back(at.string());
            if (at == at.parent_path()) break;
            at = at.parent_path();
        }
        if (const char* base = SDL_GetBasePath()) {
            dirs.push_back(base);
            dirs.push_back((std::filesystem::path(base) / "..").string());
        }
    }
    if (!mach.mount(ctx.disc, {}, &err)) return log::error("{}", err), 1;
    mach.card.set_path(0, ctx.cards_dir + "card1.mcd", true);
    mach.card.set_path(1, ctx.cards_dir + "card2.mcd", true);
    {
        vfs::DiscImage img;
        Bytes exe;
        const auto e = img.open(ctx.disc) ? vfs::iso_find(img, "ACE3.BIN") : std::nullopt;
        if (!e || !img.read_bytes(e->lba, e->size, exe)) return log::error("{}: no ACE3.BIN on it", ctx.disc), 1;
        if (mach.cd.english(0) || mach.cd.english(1)) {
            Bytes en[2];
            bool have[2];
            for (int which = 0; which < 2; ++which)
                have[which] = mach.cd.english(which) && mach.cd.read_file(which, "ACE3.BIN", en[which]) &&
                              en[which].size() == exe.size() && exe.size() > 0x800 &&
                              std::memcmp(en[which].data(), exe.data(), 0x800) == 0;
            u32 changed = 0;
            for (size_t i = 0x800; i < exe.size() && (have[0] || have[1]); ++i) {
                const u8 was = exe[i];
                if (have[0] && en[0][i] != was) exe[i] = en[0][i];
                else if (have[1] && en[1][i] != was) exe[i] = en[1][i];
                changed += exe[i] != was;
            }
            log::info("english: {} byte{} of the executable's data from the translated ACE3.BIN", changed, changed == 1 ? "" : "s");
        }
        if (!game.load_exe(exe)) return log::error("{}: no ACE3.BIN on it", ctx.disc), 1;
    }

    machine::InputRecording rec;
    if (!ctx.input.empty() && !rec.load(ctx.input)) return log::error("cannot read {}", ctx.input), 1;
    Pads pads;
    bool recorded = !ctx.input.empty();
    if (!ctx.headless && (!recorded || ctx.input_until_phase)) pads.open();
    struct PadLine {
        u32 field;
        u16 buttons;
        u8 axes[4];
    };
    std::vector<PadLine> script;
    if (!ctx.pad_script.empty()) {
        static const std::pair<const char*, u16> kNames[] = {
            {"up", machine::kBtnUp}, {"down", machine::kBtnDown}, {"left", machine::kBtnLeft},
            {"right", machine::kBtnRight}, {"circle", machine::kBtnCircle}, {"cross", machine::kBtnCross},
            {"square", machine::kBtnSquare}, {"triangle", machine::kBtnTriangle}, {"start", machine::kBtnStart},
            {"select", machine::kBtnSelect}, {"l1", machine::kBtnL1}, {"r1", machine::kBtnR1},
            {"l2", machine::kBtnL2}, {"r2", machine::kBtnR2}};
        const auto text = file::read_all(ctx.pad_script);
        if (!text) return log::error("cannot read {}", ctx.pad_script), 1;
        std::string all(text->begin(), text->end());
        size_t at = 0;
        while (at < all.size()) {
            size_t end = all.find('\n', at);
            if (end == std::string::npos) end = all.size();
            std::string line = all.substr(at, end - at);
            at = end + 1;
            if (const size_t h = line.find('#'); h != std::string::npos) line.resize(h);
            char* rest = nullptr;
            const unsigned long field = std::strtoul(line.c_str(), &rest, 10);
            if (rest == line.c_str()) continue;
            u16 buttons = 0;
            u8 axes[4] = {128, 128, 128, 128};
            std::string word;
            for (const char* c = rest;; ++c) {
                if (*c && *c != ' ' && *c != '\t' && *c != '\r') {
                    word += *c;
                    continue;
                }
                for (const auto& [name, bit] : kNames)
                    if (word == name) buttons |= bit;
                static const char* const kAxes[] = {"lx=", "ly=", "rx=", "ry="};
                for (u32 k = 0; k < 4; ++k)
                    if (!word.compare(0, 3, kAxes[k]))
                        axes[k] = static_cast<u8>(std::clamp(std::atoi(word.c_str() + 3), 0, 255));
                word.clear();
                if (!*c) break;
            }
            script.push_back({static_cast<u32>(field), buttons, {axes[0], axes[1], axes[2], axes[3]}});
        }
        log::info("pad script: {} lines from field {}", script.size(), script.empty() ? 0 : script.front().field);
    }
    mach.poll_input = [&](u32 vb, machine::PadInput p[2]) {
        if (!script.empty() && vb >= script.front().field) {
            const PadLine* now = &script.front();
            for (const PadLine& line : script)
                if (vb >= line.field) now = &line;
            p[0] = machine::PadInput{};
            p[0].connected = true;
            p[0].type = 0x73;
            p[0].buttons = now->buttons;
            p[0].lx = now->axes[0];
            p[0].ly = now->axes[1];
            p[0].rx = now->axes[2];
            p[0].ry = now->axes[3];
            return;
        }
        if (recorded) rec.pads(vb, p);
        else if (!ctx.headless) pads.read(*ctx.window, p[0]);
        else p[0].connected = true;
    };

    SDL_AudioStream* audio = nullptr;
    if (ctx.audio && !ctx.headless && SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        const SDL_AudioSpec spec{SDL_AUDIO_S16LE, 2, machine::Spu::kSampleRate};
        audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
        if (audio) SDL_ResumeAudioStreamDevice(audio);
        else log::warn("no audio device: {}", SDL_GetError());
    }
    if (audio)
        mach.spu.submit = [audio](const s16* p, int n) {
            const int queued = SDL_GetAudioStreamQueued(audio) / 4;
            if (queued >= machine::Spu::kSampleRate / 5) return;
            if (queued > machine::Spu::kSampleRate / 16 && n > 1) --n;
            SDL_PutAudioStreamData(audio, p, n * 4);
        };

    sim::PreciseCache precise;
    render::HdRenderer hd;
    auto internal_scale = [&] {
        const u32 ss = settings.antialias == 1 ? 2 : settings.antialias == 2 ? 4 : 1;
        return std::min(8u, resolution_scale(settings) * ss);
    };
    hd.texture_dir = ctx.textures_dir;
    if (!hd.init(ctx.renderer->gpu(), ctx.renderer->target(), internal_scale(), nullptr))
        return log::error("the renderer could not be set up"), 1;
    mach.gpu.observer = &hd;
    hd.set_hints(&hints);
    auto apply_settings = [&] {
        if (!hd.set_scale(internal_scale())) {
            settings.resolution = 0;
            settings.antialias = 0;
            hd.set_scale(1);
        }
        hd.perspective = settings.perspective != 0;
        hd.hd_textures = settings.hd_textures != 0;
        hd.hd_lighting = settings.hd_lighting != 0;
        hd.ground_filter = settings.ground_filter != 0;
        hd.water = settings.water != 0;
        hd.model_depth = settings.model_depth != 0;
        game.smooth_on = settings.frame_rate != 0;
        gte.precise = settings.precision ? &precise : nullptr;
        hd.set_precise(settings.precision ? &precise : nullptr);
        if (!ctx.headless) {
            ctx.window->set_fullscreen(settings.display != 0);
            ctx.renderer->set_vsync(settings.vsync != 0);
        }
        if (!ctx.settings_path.empty()) save_settings(ctx.settings_path, settings);
        settings.changed = false;
    };
    apply_settings();
    game.track_coverage = ctx.trace_screens;
    std::vector<u32> shots;
    for (size_t at = 0; at < ctx.shot_at.size();) {
        size_t end = ctx.shot_at.find(',', at);
        if (end == std::string::npos) end = ctx.shot_at.size();
        shots.push_back(static_cast<u32>(std::strtoul(ctx.shot_at.substr(at, end - at).c_str(), nullptr, 10)));
        at = end + 1;
    }
    std::sort(shots.begin(), shots.end());
    bool f9_was = false;
    u32 trace_n = 0;
    Uint64 controls_logged = 0;
    mach.gpu.rasterize = ctx.vram_check != 0;
    hd.check_dump = ctx.vram_dump;

    log::info("booting {}", ctx.disc);
    u32 next = game.boot();
    log::info("booted: field {}", mach.vblank());

    const float black[3] = {0, 0, 0};
    long frames = 0;
    const Uint64 t0 = SDL_GetTicksNS();
    const u32 field0 = mach.vblank();
    int rc = 0;
    struct {
        Uint64 since = 0, last_present = 0;
        u32 iters = 0, game = 0, mids = 0, presented = 0, late = 0;
        u32 since_game = 0, holds[4] = {};
        double worst_gap = 0, game_ms = 0, game_max = 0, frame_ms = 0, frame_max = 0;
    } pf;
    pf.since = pf.last_present = t0;
    auto refresh_paced = [&] {
        if (ctx.headless || !ctx.renderer->paced()) return false;
        const SDL_DisplayMode* dm = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(ctx.window->handle()));
        return dm && dm->refresh_rate > 59.0f && dm->refresh_rate < 61.0f;
    };
    bool by_refresh = refresh_paced();
    log::info("pacing: {}", by_refresh ? "by the display's refreshes" : "by the clock");
    u64 refreshes = 0, loops = 0;
    s64 refresh_skew = 0;
    double clock_skew = 0;
    u32 turbo_loops = 0;
    u32 mids_left = 0, mids_of = 1, mid_from = 0;
    while (ctx.headless || ctx.window->pump()) {
        if (ctx.on_loop && !ctx.on_loop()) break;
        const bool turbo = ctx.turbo && *ctx.turbo;
        if (!ctx.headless && (ctx.window->fullscreen() ? 1u : 0u) != settings.display) {
            settings.display = ctx.window->fullscreen() ? 1u : 0u;
            settings.changed = true;
        }
        if (settings.changed) apply_settings();
        {
            const u32 sc = mem.r32(0x800BCE60u);
            const u32 in = sc + 288, pad = sc + 80, cfg = sc + 348, type = mem.r8(pad + 12);
            const bool f9 = !ctx.headless && ctx.window->key(SDL_SCANCODE_F9);
            const s32 lx = mem.r16s(pad + 24) - 128, ly = mem.r16s(pad + 22) - 128;
            const bool diagonal = type == 0x73 && std::abs(lx) >= 64 && std::abs(ly) >= 64;
            const Uint64 tnow = SDL_GetTicksNS();
            const bool auto_log = diagonal && tnow - controls_logged >= 1000000000ull;
            if ((f9 && !f9_was) || auto_log) {
                controls_logged = tnow;
                ++trace_n;
                log::info("controls {}{}: field {}, phase {:08X}", trace_n, f9 ? " (F9)" : " (stick diagonal)",
                          mach.vblank(), next);
                if (f9 && ctx.trace_screens) {
                    const u32 dlg = mem.r32(0x80112DDCu), menu = mem.r32(0x80112DC4u);
                    log::info("trace {}: field {}, phase {:08X}, overlay {}, {} functions", trace_n, mach.vblank(), next,
                              mem.r32(sc + 56), game.coverage.size());
                    if (dlg >= 0x80000000u && dlg < 0x80200000u)
                        log::info("trace {}: dialog {:08X}: {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X} {:02X}; menu {:08X}",
                                  trace_n, dlg, mem.r8(dlg), mem.r8(dlg + 1), mem.r8(dlg + 2), mem.r8(dlg + 3),
                                  mem.r8(dlg + 4), mem.r8(dlg + 5), mem.r8(dlg + 6), mem.r8(dlg + 7), menu);
                    std::string line;
                    for (const auto& [addr, n] : game.coverage) {
                        line += std::format(" {:08X}:{}", addr, n);
                        if (line.size() > 180) {
                            log::info("trace {}:{}", trace_n, line);
                            line.clear();
                        }
                    }
                    if (!line.empty()) log::info("trace {}:{}", trace_n, line);
                    game.coverage.clear();
                }
                const u32 table = type == 0x73 ? cfg + (mem.r16(pad + 10) == 7 ? 184 : 92) : type == 0x53 ? cfg + 276
                                : type == 0x23 ? cfg + 368 : cfg;
                log::info("controls {}: flags {:08X} kind {} live {:X} axes {} {} {} {} {} {} buttons {:08X}", trace_n,
                          mem.r32(in), mem.r32(in + 4), mem.r32(in + 24), mem.r32s(in + 28), mem.r32s(in + 32),
                          mem.r32s(in + 36), mem.r32s(in + 40), mem.r32s(in + 44), mem.r32s(in + 48), mem.r32(in + 8));
                log::info("controls {}: pad type {:02X} ex {} raw {} {} {} {}; table +{} axis map {} {} {} {} {} {} flags "
                          "{:04X} opts {:08X} cal {}/{}/{}/{} {}/{}/{}/{} {}/{}/{}/{} {}/{}/{}/{}", trace_n, type,
                          mem.r16(pad + 10), mem.r16s(pad + 18), mem.r16s(pad + 20), mem.r16s(pad + 22),
                          mem.r16s(pad + 24), table - cfg, mem.r8(table + 82), mem.r8(table + 83), mem.r8(table + 84),
                          mem.r8(table + 85), mem.r8(table + 86), mem.r8(table + 87), mem.r16(table + 80),
                          mem.r32(table + 88), mem.r8(table + 64), mem.r8(table + 65), mem.r8(table + 66),
                          mem.r8(table + 67), mem.r8(table + 68), mem.r8(table + 69), mem.r8(table + 70),
                          mem.r8(table + 71), mem.r8(table + 72), mem.r8(table + 73), mem.r8(table + 74),
                          mem.r8(table + 75), mem.r8(table + 76), mem.r8(table + 77), mem.r8(table + 78),
                          mem.r8(table + 79));
                const u32 st = mem.r32(0x800BD088u), e = mem.r32(mem.r32(0x800BCE5Cu));
                log::info("controls {}: stick {} {} {} attitude pitch {} heading {} roll {}", trace_n, mem.r16s(st),
                          mem.r16s(st + 2), mem.r16s(st + 4), mem.r16s(e + 24), mem.r16s(e + 26), mem.r16s(e + 28));
            }
            f9_was = f9;
        }
        if (!ctx.headless) {
            if (ctx.window->take_resized()) ctx.renderer->resized();
            if (ctx.window->minimized()) {
                ctx.window->wait(100);
                continue;
            }
        }
        const double now = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9;
        const u32 fields = mach.vblank() - field0;
        const double game_time = fields * kFieldSeconds;
        if (++loops % 64 == 0 && refresh_paced() != by_refresh) {
            by_refresh = !by_refresh;
            refresh_skew = static_cast<s64>(refreshes) - fields;
            clock_skew = now - game_time;
            log::info("pacing: {}", by_refresh ? "by the display's refreshes" : "by the clock");
        }
        const bool due = turbo || (by_refresh ? static_cast<s64>(fields) <= static_cast<s64>(refreshes) - refresh_skew
                                              : game_time <= now - clock_skew);
        const Uint64 ta = SDL_GetTicksNS();
        bool mid = false;
        if (mids_left && !turbo && !(ctx.headless ? false : due)) {
            const u32 at = mid_from + (mids_of - mids_left);
            if (ctx.headless || (by_refresh ? static_cast<s64>(at) <= static_cast<s64>(refreshes) - refresh_skew
                                            : at * kFieldSeconds <= now - clock_skew)) {
                precise.next_frame();
                mid = game.smooth_draw(static_cast<float>(mids_of - mids_left) / static_cast<float>(mids_of));
                precise.next_frame();
                precise.next_frame();
                --mids_left;
                pf.mids += mid;
            }
        }
        if (!mid && mids_left == 0 ? (ctx.headless || due) : (!ctx.headless && due)) {
            mids_left = 0;
            const u32 began = mach.vblank();
            hints.wide = false;
            ++pf.game;
            if (pf.since_game) ++pf.holds[std::min(pf.since_game, 4u) - 1];
            pf.since_game = 0;
            if (!game.run_phase(next)) {
                log::error("the game reached phase function {:08X}, which the engine does not have yet", next);
                rc = 1;
                break;
            }
            const u32 before = next;
            next = game.main_loop(game.phase_result);
            if (ctx.trace_phases && next != before) log::info("phase {:08X} at field {}", next, mach.vblank());
            precise.next_frame();
            ++frames;
            if (const u32 took = mach.vblank() - began; took >= 2 && took <= 4 && game.smooth_ready()) {
                mids_of = took;
                mids_left = took - 1;
                mid_from = began - field0;
            }
            if (recorded && ctx.input_until_phase && next == ctx.input_until_phase) {
                recorded = false;
                log::info("the recording hands over the controls at field {}", mach.vblank());
            }
            if (ctx.on_frame) ctx.on_frame(game, mach, next);
            if (game.unported_hits && frames == 1) log::warn("unported code reached: see the log");
            if (ctx.vram_check && frames % ctx.vram_check == 0) hd.check(mach.gpu, static_cast<u32>(frames));
        }
        hd.displayed(mach.gpu.display);
        if (turbo && (++turbo_loops & 7u)) continue;
        if (!turbo) {
            if (turbo_loops) {
                refresh_skew = static_cast<s64>(refreshes) - (mach.vblank() - field0);
                clock_skew = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9 - (mach.vblank() - field0) * kFieldSeconds;
            }
            turbo_loops = 0;
        }
        const Uint64 tb = SDL_GetTicksNS();
        const bool drew = ctx.renderer->frame(
            black,
            [&](VkCommandBuffer cb, const render::Target& t) {
                const float shown = hints.wide && !mach.gpu.display.rgb24 ? 4.0f / 3.0f / hints.squeeze : 4.0f / 3.0f;
                if (mach.gpu.display.enabled) hd.present(cb, t, mach.gpu.display, shown);
            },
            [&](VkCommandBuffer cb, u32 slot) { hd.record(cb, slot); });
        if (drew) ++refreshes;
        while (drew && !shots.empty() && mach.vblank() >= shots.front() && (!ctx.shot_mid || mid)) {
            std::string path = ctx.shot_path.empty() ? std::string("shot.png") : ctx.shot_path;
            const size_t dot = path.rfind(".png");
            path = (dot == std::string::npos ? path : path.substr(0, dot)) + "_" + std::to_string(shots.front()) + ".png";
            if (ctx.renderer->screenshot(path)) log::info("screenshot at field {}: {}", mach.vblank(), path);
            shots.erase(shots.begin());
        }
        if (ctx.perf) {
            const Uint64 tc = SDL_GetTicksNS();
            const double g = static_cast<double>(tb - ta) / 1e6, f = static_cast<double>(tc - tb) / 1e6;
            ++pf.iters;
            pf.game_ms += g;
            pf.game_max = std::max(pf.game_max, g);
            pf.frame_ms += f;
            pf.frame_max = std::max(pf.frame_max, f);
            if (drew) {
                const double gap = static_cast<double>(tc - pf.last_present) / 1e6;
                pf.worst_gap = std::max(pf.worst_gap, gap);
                if (gap > 1000.0 / 50.0) ++pf.late;
                pf.last_present = tc;
                ++pf.presented;
                ++pf.since_game;
            }
            if (tc - pf.since >= 1000000000ull) {
                log::info("perf: {} loops, {} game frames and {} in-between, {} presented ({} after more than 20 ms, "
                          "the longest {:.1f} ms); game {:.2f} ms a loop (most {:.1f}), frame {:.2f} (most {:.1f}); {} "
                          "downsamples, {} read-backs ({:.1f} ms); frames shown for 1/2/3/more presents: {}/{}/{}/{}; "
                          "sound queued {} ms",
                          pf.iters, pf.game, pf.mids, pf.presented, pf.late, pf.worst_gap, pf.game_ms / pf.iters, pf.game_max,
                          pf.frame_ms / pf.iters, pf.frame_max, hd.perf.downs, hd.perf.reads, hd.perf.read_ms,
                          pf.holds[0], pf.holds[1], pf.holds[2], pf.holds[3],
                          audio ? SDL_GetAudioStreamQueued(audio) / 4 * 1000 / machine::Spu::kSampleRate : 0);
                const Uint64 last = pf.last_present;
                const u32 since_game = pf.since_game;
                pf = {};
                pf.since = tc;
                pf.last_present = last;
                pf.since_game = since_game;
                hd.perf = {};
            }
        }
        if (ctx.frames > 0 && frames >= ctx.frames) break;
        if (recorded && !ctx.input_until_phase && mach.vblank() > rec.fields()) {
            log::info("the recording ends at field {}", rec.fields());
            break;
        }
    }
    const double secs = static_cast<double>(SDL_GetTicksNS() - t0) / 1e9;
    log::info("played {} frames ({} fields) in {:.2f} s", frames, mach.vblank() - field0, secs);
    log::info("renderer: {} primitives in the last frame, {} with every vertex precise", hd.prims(),
              hd.precise_prims());
    mach.gpu.observer = nullptr;
    hd.destroy();
    if (game.unported_hits) log::warn("{} calls reached code not reimplemented yet", game.unported_hits);
    if (audio) SDL_DestroyAudioStream(audio);
    pads.close();
    return rc;
}
}
