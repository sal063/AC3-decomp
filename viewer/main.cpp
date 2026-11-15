#include "play.hpp"
#include "scripts.hpp"
#include "ui.hpp"

#include "core/log.hpp"
#include "core/settings.hpp"
#include "machine/machine.hpp"
#include "platform/window.hpp"
#include "render/renderer.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <format>
#include <string>
#include <vector>

using namespace ac3;
using namespace ac3::viewer;

namespace {
constexpr u32 kMissionTitles = 0x800B0CA0u;
constexpr u32 kMissionRecords = 0x800B0B08u;
constexpr u32 kMenu = 0x80112DC4u;
constexpr u32 kO2Phase8 = 0x800CDF40u;
constexpr int kMissions = 61;
constexpr u32 kAircraftRecords = 0x800B0B84u;
constexpr int kAircraftSlots = 38;

const char* aircraft_name(int record) {
    static const char* const kNames[] = {
        "YR-900", "FA-18I", "F-16XF (GR)", "F-16XA (GR)", "F-15SMT (GR)", "AF-117X", "F-22C (GR)", "FA-32C (GR)",
        "XFA-36A (GR)", "RF-12A2 (GR)", "EF-2000E", "MiG-33", "FA-18U", "F-16XF (UPEO)", "R-101 (UPEO G)",
        "R-201 (UPEO G)", "Su-37 (UPEO)", "Su-43 (UPEO)", "R-101 (Neu)", "R-201 (Neu)", "R-102 (Neu)", "R-211 (Neu)",
        "R-103 (Neu)", "R-311", "R-352 (Neu)", "XR-900", "X-49", "UI-4054", "XFA-36A (O)", "Su-43 (O)", "R-103 (O)",
        "F-15SMT (O)", "F-22C (O)", "Su-37 (Rena)", "R-102-C", "R-103-C", "R-311-C",
    };
    return record >= 66 && record <= 102 ? kNames[record - 66] : nullptr;
}

std::vector<int> aircraft_list(const sim::Memory& m) {
    std::vector<int> out;
    for (int i = 0; i < kAircraftSlots; ++i)
        if (aircraft_name(m.r16s(kAircraftRecords + 2 * static_cast<u32>(i)))) out.push_back(i);
    return out;
}

std::string aircraft_label(const sim::Memory& m, int index) {
    if (index < 0 || index >= kAircraftSlots) return "?";
    const int record = m.r16s(kAircraftRecords + 2 * static_cast<u32>(index));
    const char* name = aircraft_name(record);
    return name ? std::string(name) : std::format("aircraft {} (record {})", index, record);
}

bool in_ram(u32 a) { return a >= 0x80010000u && a < 0x80200000u; }

std::string guest_string(const sim::Memory& m, u32 a, size_t max = 40) {
    std::string s;
    if (!in_ram(a)) return s;
    for (size_t i = 0; i < max; ++i) {
        const u32 c = m.r8(a + static_cast<u32>(i));
        if (!c) break;
        s += static_cast<char>(c >= 32 && c < 127 ? c : '?');
    }
    return s;
}

std::string title(const sim::Memory& m, int mission) {
    if (mission < 0 || mission >= 60) return "(no title)";
    std::string t = guest_string(m, m.r32(kMissionTitles + 4 * static_cast<u32>(mission)));
    return t.empty() ? std::string("(no title)") : t;
}

std::string sub_phase_name(u32 sp) {
    switch (sp) {
    case 0: return "none";
    case 2: return "intro";
    case 4: return "flight";
    case 8: return "ending";
    case 0x10: return "sub-phase 16";
    case 0x40: return "sub-phase 64";
    case 0x80: return "sub-phase 128";
    case 0x2000: return "sub-phase 0x2000";
    case 0x4000: return "sub-phase 0x4000";
    case 0x10000: return "mission end";
    case 0x80000: return "mission end (b)";
    case 0x100000: return "scripted ending";
    case 0x800000: return "quit";
    case 0x10000000: return "retry";
    default: return std::format("0x{:X}", sp);
    }
}

struct Launcher {
    int pending = -1;
    int aircraft = -1;
    bool quit_sent = false;
    std::string status = "booting";

    void request(int mission) {
        if (!launchable(mission)) {
            status = std::format("mission {} is not started from the menus: {}", mission, why_not(mission));
            return;
        }
        pending = mission;
        quit_sent = false;
        status = std::format("mission {} requested", mission);
    }

    static bool launchable(int mission) { return mission >= 1 && mission < kMissions && mission != 59; }
    static const char* why_not(int mission) {
        return mission == 59 ? "the special mission needs the game's special mode" : "not a mission the game flies";
    }

    int flying = -1;

    void frame(sim::Memory& m, u32 next) {
        const u32 sc = m.r32(sim::g::kScene);
        const u32 group = m.r32(sc + 44), phase = m.r32(sc + 48);
        if (flying >= 0 && group == 2 && phase == 7) {
            status = std::format("flying mission {}", flying);
            flying = -1;
        }
        if (pending < 0) return;
        if (next == kO2Phase8 && m.r32(sc + 56) == 2 && in_ram(m.r32(kMenu))) {
            m.w16(sc + 908, static_cast<u32>(pending));
            if (aircraft >= 0) m.w16(sc + 910, static_cast<u32>(aircraft));
            m.w32(sc + 944, 2);
            m.w32(sc + 948, 0);
            m.w16(sc + 2388, 1);
            m.w32(m.r32(kMenu) + 0x94, 0x60000002u);
            log::info("viewer: mission {} launched from the menus, aircraft {} ({})", pending, m.r16s(sc + 910),
                      aircraft_label(m, m.r16s(sc + 910)));
            status = std::format("launching mission {}", pending);
            flying = pending;
            pending = -1;
            return;
        }
        if (group == 2 && phase == 7 && !quit_sent) {
            const u32 w = m.r32(sim::g::kWorld);
            if (in_ram(w)) {
                m.w32(w, m.r32(w) | 0x80000000u);
                quit_sent = true;
                status = std::format("leaving the mission for mission {}", pending);
                log::info("viewer: quitting the mission (for mission {})", pending);
            }
        } else if (!quit_sent) {
            status = std::format("mission {}: waiting for the menus", pending);
        }
    }
};

enum class Tab { Status, Units, Trace, Scripts };

struct Viewer {
    Ui ui;
    Launcher launch;
    Tracer* tracer = nullptr;
    sim::Memory* mem = nullptr;
    machine::Machine* mach = nullptr;
    u32 next = 0;
    bool turbo = false;
    bool booted = false;
    int auto_mission = -1;
    int selected = 1;
    int list_top = 0;
    Tab tab = Tab::Status;
    int trace_back = 0;
    int units_top = 0;
    std::vector<std::pair<u32, std::string>> scripts;
    int script_sel = 0, script_top = 0, listing_top = 0;
    u32 scripts_mission = 0xFFFFFFFFu, scripts_group = 0xFFFFFFFFu;
    std::vector<std::string> listing;
    u32 listing_pc = 0;

    void draw();
    void draw_missions(int col, int row, int w, int h);
    void draw_status(int col, int row, int w, int h);
    void draw_units(int col, int row, int w, int h);
    void draw_trace(int col, int row, int w, int h);
    void draw_scripts(int col, int row, int w, int h);
};

void Viewer::draw() {
    ui.begin();
    const sim::Memory& m = *mem;
    const u32 sc = m.r32(sim::g::kScene);
    const int mission = m.r16s(sc + 908);
    ui.fill(0, 0, ui.cols(), 1, Rgb{30, 34, 44});
    ui.text(1, 0, "AC3 MISSION VIEWER", kAccent);
    ui.text(21, 0, std::format("mission {} {}   |   {}", mission, title(m, mission), launch.status), kText);

    const int left = 34;
    draw_missions(0, 2, left, ui.rows() - 2);
    const int col = left + 1, w = ui.cols() - col;
    static const char* kTabs[] = {"STATUS", "UNITS", "SCRIPT TRACE", "MISSION SCRIPTS"};
    int x = col;
    for (int t = 0; t < 4; ++t) {
        if (ui.button(x, 2, kTabs[t], static_cast<int>(tab) == t)) tab = static_cast<Tab>(t);
        x += static_cast<int>(std::strlen(kTabs[t])) + 3;
    }
    const int top = 4, h = ui.rows() - top;
    switch (tab) {
    case Tab::Status: draw_status(col, top, w, h); break;
    case Tab::Units: draw_units(col, top, w, h); break;
    case Tab::Trace: draw_trace(col, top, w, h); break;
    case Tab::Scripts: draw_scripts(col, top, w, h); break;
    }
    ui.end();
}

void Viewer::draw_missions(int col, int row, int w, int h) {
    const sim::Memory& m = *mem;
    const u32 sc = m.r32(sim::g::kScene);
    const int current = m.r16s(sc + 908);
    ui.text(col + 1, row, "MISSIONS", kHead);
    ui.text(col + 11, row, "(double-click: fly)", kDim);
    const int list_row = row + 2, list_h = h - 10;
    list_top = std::clamp(list_top - 3 * ui.wheel(col, list_row, w, list_h), 0, std::max(0, kMissions - list_h));
    if (ui.pressed(SDLK_UP)) selected = std::max(0, selected - 1);
    if (ui.pressed(SDLK_DOWN)) selected = std::min(kMissions - 1, selected + 1);
    if (selected < list_top) list_top = selected;
    if (selected >= list_top + list_h) list_top = selected - list_h + 1;
    bool twice = false;
    if (const int r = ui.clicked_row(col, list_row, w, list_h, &twice); r >= 0 && list_top + r < kMissions) {
        selected = list_top + r;
        if (twice) launch.request(selected);
    }
    for (int i = 0; i < list_h && list_top + i < kMissions; ++i) {
        const int n = list_top + i;
        if (n == selected) ui.fill(col, list_row + i, w, 1, Rgb{50, 56, 72});
        const Rgb c = n == current ? kAccent : Launcher::launchable(n) ? kText : kDim;
        ui.text(col + 1, list_row + i, std::format("{:2} {}{}", n, title(m, n), n == 60 ? "  (a replay)" : ""), c);
    }
    const int b = row + h - 5;
    {
        const std::vector<int> planes = aircraft_list(m);
        const int n = static_cast<int>(planes.size());
        int at = -1;
        for (int i = 0; i < n; ++i)
            if (planes[static_cast<size_t>(i)] == launch.aircraft) at = i;
        const int a = b - 2;
        ui.text(col + 1, a, "PLANE", kHead);
        const bool back = ui.button(col + 7, a, "<") || ui.pressed(SDLK_LEFT);
        const bool forward = ui.button(col + 11, a, ">") || ui.pressed(SDLK_RIGHT);
        if (n && (back || forward)) {
            at = back ? (at < 0 ? n - 1 : at - 1) : (at + 1 >= n ? -1 : at + 1);
            launch.aircraft = at < 0 ? -1 : planes[static_cast<size_t>(at)];
        }
        const int chosen = m.r16s(sc + 910);
        ui.text(col + 15, a,
                launch.aircraft < 0 ? std::format("hangar's: {}", aircraft_label(m, chosen)).substr(0, static_cast<size_t>(std::max(0, w - 16)))
                                    : aircraft_label(m, launch.aircraft),
                launch.aircraft < 0 ? kDim : kText);
    }
    ui.text(col + 1, b, std::format("{:2} {}  record {}", selected, title(m, selected),
                                    m.r16s(kMissionRecords + 2 * static_cast<u32>(selected))), kAccent);
    if (ui.button(col + 1, b + 1, "FLY") || ui.pressed(SDLK_RETURN)) launch.request(selected);
    const u32 w_ = m.r32(sim::g::kWorld);
    const bool flying = m.r32(sc + 44) == 2 && m.r32(sc + 48) == 7 && in_ram(w_);
    if (ui.button(col + 7, b + 1, "RETRY") && flying) {
        mem->w32(w_, m.r32(w_) | 0x40000000u);
        launch.status = "retry";
    }
    if (ui.button(col + 15, b + 1, "QUIT") && flying) {
        mem->w32(w_, m.r32(w_) | 0x80000000u);
        launch.status = "quit to the menus";
    }
    if (ui.button(col + 22, b + 1, turbo ? "TURBO ON" : "TURBO", turbo)) turbo = !turbo;
    ui.text(col + 1, b + 3, "the game window has the controls", kDim);
}

void Viewer::draw_status(int col, int row, int  , int  ) {
    const sim::Memory& m = *mem;
    const u32 sc = m.r32(sim::g::kScene);
    int r = row;
    auto line = [&](const std::string& k, const std::string& v, Rgb c = kText) {
        ui.text(col, r, k, kDim);
        ui.text(col + 22, r, v, c);
        ++r;
    };
    ui.text(col, r++, "THE GAME", kHead);
    line("field", std::to_string(mach->vblank()));
    line("group / phase", std::format("{} / {}   (phase function {:08X})", m.r32(sc + 44), m.r32(sc + 48), next));
    line("overlay", std::format("{} resident, {} wanted", m.r32s(sc + 56), m.r32s(sc + 52)));
    const int mission = m.r16s(sc + 908);
    line("mission", std::format("{} {}  (record {})", mission, title(m, mission),
                                mission >= 0 && mission < kMissions ? m.r16s(kMissionRecords + 2 * static_cast<u32>(mission)) : -1));
    line("aircraft", std::format("{} {}", m.r16s(sc + 910), aircraft_label(m, m.r16s(sc + 910))));
    line("next mission (script)", std::to_string(m.r8(sc + 957)));
    ++r;
    if (m.r32(sc + 44) != 2) {
        ui.text(col, r, "not in a mission (the flight group is 2)", kDim);
        return;
    }
    const u32 w = m.r32(sim::g::kWorld), ms = m.r32(sim::g::kMission);
    ui.text(col, r++, "THE MISSION", kHead);
    if (in_ram(w)) {
        line("sub-phase", std::format("{}   variant {}   {} frames in it", sub_phase_name(m.r32(w + 4)), m.r32s(w + 8),
                                      m.r32(w + 12)));
        line("world flags", std::format("{:08X}", m.r32(w)));
    }
    if (in_ram(ms)) {
        const u32 clock = m.r32(ms + 12);
        line("clock", std::format("{} frames  ({}:{:02})", clock, clock / 30 / 60, clock / 30 % 60));
        line("timer", std::to_string(m.r32s(ms + 16)));
        line("score", std::to_string(m.r32s(ms + 4)));
        line("kills / losses", std::format("{} / {}", m.r8(ms + 45), m.r8(ms + 46)));
        line("counter A", std::format("{} of {}", m.r8(ms + 32), m.r8(ms + 30)));
        line("counter B", std::format("{} of {}", m.r8(ms + 33), m.r8(ms + 31)));
        line("counter C", std::to_string(m.r8(ms + 34)));
        line("result / ending", std::format("{} / {}", m.r8(ms + 41), m.r8(ms + 39)));
        line("music / message", std::format("{} / {}", m.r8(ms + 44), m.r8(ms + 43)));
        line("mode", std::to_string(m.r8(ms + 28)));
    }
    ++r;
    if (tracer) line("scripts run", std::format("{}   ({} lines traced)", tracer->runs(), tracer->total()));
}

void Viewer::draw_units(int col, int row, int w, int h) {
    const sim::Memory& m = *mem;
    const u32 sc = m.r32(sim::g::kScene);
    if (m.r32(sc + 44) != 2) {
        ui.text(col, row, "not in a mission", kDim);
        return;
    }
    ui.text(col, row, "UNIT         KIND    HP  FLAGS          X       Y       Z  ON DEATH", kHead);
    const u32 slots = m.r32(sim::g::kSlots);
    std::vector<u32> shown;
    for (u32 k = 0; k < 64 && in_ram(slots); ++k) {
        const u32 e = m.r32(slots + 4 * k);
        if (!in_ram(e)) continue;
        if (!(m.r32(e) & 0x4000u) && m.r16s(e + 78) <= 0) continue;
        shown.push_back(k);
    }
    const int list_h = h - 2;
    units_top = std::clamp(units_top - 3 * ui.wheel(col, row + 1, w, list_h), 0,
                           std::max(0, static_cast<int>(shown.size()) - list_h));
    const u32 air = m.r32(sim::g::kSlotRecords), ground = m.r32(sim::g::kGroundRecords);
    for (int i = 0; i < list_h && units_top + i < static_cast<int>(shown.size()); ++i) {
        const u32 k = shown[static_cast<size_t>(units_top + i)];
        const u32 e = m.r32(slots + 4 * k);
        const bool active = (m.r32(e) & 0x4000u) != 0;
        const u32 death = k < 32 ? (in_ram(air) ? m.r32(air + 64 * k + 52) : 0) : (in_ram(ground) ? m.r32(ground + 40 * (k - 32) + 28) : 0);
        ui.text(col, row + 1 + i,
                std::format("{:<10} {:<6} {:6}  {:08X} {:7} {:7} {:7}  {}", unit_name(k),
                            k < 32 ? "air" : (m.r8(e + 5) & 2u) ? "model" : "static", m.r16s(e + 78), m.r32(e),
                            m.r32s(e + 8) >> 12, m.r32s(e + 12) >> 12, m.r32s(e + 16) >> 12,
                            death ? std::format("{:08X}", death) : std::string("-")),
                active ? kText : kDim);
    }
    ui.text(col, row + h - 1, std::format("{} units; bright: on the field (flag 0x4000), dim: not yet or no more", shown.size()),
            kDim);
}

void Viewer::draw_trace(int col, int row, int w, int h) {
    if (!tracer) return;
    if (ui.button(col, row, tracer->frozen ? "FROZEN" : "FREEZE", tracer->frozen)) tracer->frozen = !tracer->frozen;
    if (ui.button(col + 10, row, "CLEAR")) {
        tracer->clear();
        trace_back = 0;
    }
    ui.text(col + 19, row, std::format("{} scripts run, newest at the bottom; wheel scrolls", tracer->runs()), kDim);
    const auto& lines = tracer->lines();
    const int list_h = h - 2, n = static_cast<int>(lines.size());
    trace_back = std::clamp(trace_back + 3 * ui.wheel(col, row + 1, w, list_h), 0, std::max(0, n - list_h));
    const int first = std::max(0, n - list_h - trace_back);
    for (int i = 0; i < list_h && first + i < n; ++i) {
        const Tracer::Line& l = lines[static_cast<size_t>(first + i)];
        if (l.header) {
            ui.text(col, row + 2 + i, std::format("f{:<7}", l.field), kDim);
            ui.text(col + 9, row + 2 + i, l.text, kAccent);
        } else {
            ui.text(col + 9 + 2 * l.depth, row + 2 + i, l.text, kText);
        }
    }
}

void Viewer::draw_scripts(int col, int row, int w, int h) {
    const sim::Memory& m = *mem;
    const u32 sc = m.r32(sim::g::kScene);
    const u32 group = m.r32(sc + 44), mission = m.r16(sc + 908);
    if (group != 2 || m.r32(sc + 48) < 7) {
        ui.text(col, row, "the scripts are the loaded mission's: fly a mission to see them", kDim);
        scripts.clear();
        scripts_group = group;
        return;
    }
    if (scripts.empty() || scripts_mission != mission || scripts_group != group) {
        scripts = entries(m);
        scripts_mission = mission;
        scripts_group = group;
        script_sel = 0;
        script_top = 0;
        listing_pc = 0;
    }
    const int lw = std::min(44, w / 2);
    ui.text(col, row, std::format("{} SCRIPTS, by where they run from", scripts.size()).substr(0, static_cast<size_t>(lw - 1)), kHead);
    const int list_h = h - 2;
    script_top = std::clamp(script_top - 3 * ui.wheel(col, row + 1, lw, list_h), 0,
                            std::max(0, static_cast<int>(scripts.size()) - list_h));
    if (const int r = ui.clicked_row(col, row + 1, lw, list_h); r >= 0 && script_top + r < static_cast<int>(scripts.size()))
        script_sel = script_top + r;
    for (int i = 0; i < list_h && script_top + i < static_cast<int>(scripts.size()); ++i) {
        const int n = script_top + i;
        if (n == script_sel) ui.fill(col, row + 1 + i, lw, 1, Rgb{50, 56, 72});
        ui.text(col, row + 1 + i, scripts[static_cast<size_t>(n)].second.substr(0, static_cast<size_t>(lw - 1)), kText);
    }
    if (scripts.empty()) return;
    const auto& [pc, what] = scripts[static_cast<size_t>(std::min(script_sel, static_cast<int>(scripts.size()) - 1))];
    if (pc != listing_pc) {
        listing.clear();
        viewer::listing(m, pc, listing);
        listing_pc = pc;
        listing_top = 0;
    }
    const int lc = col + lw + 1, lh = h - 2;
    ui.text(lc, row, std::format("{:08X}  {}", pc, what).substr(0, static_cast<size_t>(w - lw - 1)), kAccent);
    listing_top = std::clamp(listing_top - 3 * ui.wheel(lc, row + 1, w - lw - 1, lh), 0,
                             std::max(0, static_cast<int>(listing.size()) - lh));
    for (int i = 0; i < lh && listing_top + i < static_cast<int>(listing.size()); ++i)
        ui.text(lc, row + 1 + i, listing[static_cast<size_t>(listing_top + i)], kText);
}

struct Args {
    std::string disc, cards, boot, settings;
    int mission = -1, aircraft = -1;
    bool headless = false;
    u32 width = 1280, height = 960;
    long frames = 0;
    std::string shot_at, screenshot;
    bool shot_mid = false;
    std::vector<std::pair<int, u32>> requests;
    u32 vram_check = 0;
    std::string vram_dump, pad_script;
};

bool parse(int argc, char** argv, Args& a) {
    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        const bool more = i + 1 < argc;
        if (!std::strcmp(s, "--disc") && more) a.disc = argv[++i];
        else if (!std::strcmp(s, "--cards") && more) a.cards = argv[++i];
        else if (!std::strcmp(s, "--boot") && more) a.boot = argv[++i];
        else if (!std::strcmp(s, "--settings") && more) a.settings = argv[++i];
        else if (!std::strcmp(s, "--mission") && more) a.mission = std::atoi(argv[++i]);
        else if (!std::strcmp(s, "--aircraft") && more) a.aircraft = std::atoi(argv[++i]);
        else if (!std::strcmp(s, "--headless")) a.headless = true;
        else if (!std::strcmp(s, "--size") && more) {
            const char* v = argv[++i];
            const char* x = std::strchr(v, 'x');
            a.width = static_cast<u32>(std::strtoul(v, nullptr, 10));
            a.height = x ? static_cast<u32>(std::strtoul(x + 1, nullptr, 10)) : 0;
            if (!a.width || !a.height) return log::error("--size wants WxH"), false;
        }
        else if (!std::strcmp(s, "--vram-check") && more) a.vram_check = static_cast<u32>(std::atoi(argv[++i]));
        else if (!std::strcmp(s, "--vram-dump") && more) a.vram_dump = argv[++i];
        else if (!std::strcmp(s, "--pad-script") && more) a.pad_script = argv[++i];
        else if (!std::strcmp(s, "--frames") && more) a.frames = std::atol(argv[++i]);
        else if (!std::strcmp(s, "--shot-at") && more) a.shot_at = argv[++i];
        else if (!std::strcmp(s, "--shot-mid")) a.shot_mid = true;
        else if (!std::strcmp(s, "--screenshot") && more) a.screenshot = argv[++i];
        else if (!std::strcmp(s, "--request") && more) {
            const std::string r = argv[++i];
            const size_t at = r.find('@');
            if (at == std::string::npos) return log::error("--request wants M@F"), false;
            a.requests.emplace_back(std::atoi(r.substr(0, at).c_str()),
                                    static_cast<u32>(std::strtoul(r.substr(at + 1).c_str(), nullptr, 10)));
        } else {
            log::error("unknown or incomplete argument: {}", s);
            return false;
        }
    }
    if (a.disc.empty()) return log::error("--disc is needed: disc 1's image"), false;
    return true;
}
}

int main(int argc, char** argv) {
    Args args;
    if (!parse(argc, argv, args)) return 2;
    const std::string base = SDL_GetBasePath() ? SDL_GetBasePath() : "";
    if (!log::open_file((base + "viewer.log").c_str())) log::warn("cannot open the log file in {}", base);
    if (args.cards.empty()) args.cards = base + "cards";
    if (args.settings.empty()) args.settings = base + "settings.ini";
    if (args.boot.empty() && SDL_GetPathInfo((base + "boot.ac3i").c_str(), nullptr)) args.boot = base + "boot.ac3i";

    Settings settings;
    if (!load_settings(args.settings, settings)) save_settings(args.settings, settings);

    platform::Window window;
    if (args.headless) {
        if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) return log::error("SDL video: {}", SDL_GetError()), 1;
    } else {
        if (!window.create("AC3 Mission Viewer -- the game", 1280, 960)) return 1;
        if (settings.display) window.set_fullscreen(true);
    }
    render::Renderer renderer;
    render::Config rc;
    rc.vsync = settings.vsync != 0;
    if (args.headless) {
        rc.headless_width = args.width;
        rc.headless_height = args.height;
    }
    if (!renderer.init(args.headless ? nullptr : window.handle(), rc)) return 1;

    Viewer v;
    if (!v.ui.create("AC3 Mission Viewer", 1500, 940, args.headless)) return 1;
    v.auto_mission = args.mission;
    v.launch.aircraft = args.aircraft;
    window.event_hook = [&](const SDL_Event& e) { return v.ui.handle(e); };

    GameContext gc;
    gc.disc = args.disc;
    gc.cards_dir = args.cards + "/";
    SDL_CreateDirectory(gc.cards_dir.c_str());
    gc.window = args.headless ? nullptr : &window;
    gc.headless = args.headless;
    gc.audio = !args.headless;
    gc.frames = args.frames;
    gc.vram_check = args.vram_check;
    gc.vram_dump = args.vram_dump;
    gc.pad_script = args.pad_script;
    gc.shot_at = args.shot_at;
    gc.shot_mid = args.shot_mid;
    gc.shot_path = args.screenshot;
    gc.renderer = &renderer;
    gc.settings = &settings;
    gc.settings_path = args.settings;
    gc.textures_dir = base + "textures";
    if (!args.boot.empty()) {
        gc.input = args.boot;
        gc.input_until_phase = kO2Phase8;
        v.turbo = true;
        v.launch.status = "booting to the menus (the boot recording, fast)";
    } else {
        v.launch.status = "booting: play to the menus (no boot recording)";
    }
    gc.turbo = &v.turbo;

    Tracer* tracer = nullptr;
    gc.on_frame = [&](sim::Game& game, machine::Machine& mach, u32 next) {
        if (!tracer) {
            tracer = new Tracer(game.m);
            game.script_observer = tracer;
            v.tracer = tracer;
            v.mem = &game.m;
            v.mach = &mach;
        }
        tracer->field = mach.vblank();
        v.next = next;
        if (!v.booted && next == kO2Phase8) {
            v.booted = true;
            v.turbo = false;
            v.launch.status = "at the menus: pick a mission and FLY";
            log::info("viewer: the menus reached at field {}", mach.vblank());
            if (v.auto_mission >= 0) v.launch.request(v.auto_mission);
        }
        for (auto& [mission, field] : args.requests)
            if (field && mach.vblank() >= field) {
                v.launch.request(mission);
                field = 0;
            }
        v.launch.frame(game.m, next);
    };
    std::vector<u32> panel_shots;
    for (size_t at = 0; at < args.shot_at.size();) {
        size_t end = args.shot_at.find(',', at);
        if (end == std::string::npos) end = args.shot_at.size();
        panel_shots.push_back(static_cast<u32>(std::strtoul(args.shot_at.substr(at, end - at).c_str(), nullptr, 10)));
        at = end + 1;
    }
    std::sort(panel_shots.begin(), panel_shots.end());
    Uint64 last_draw = 0;
    gc.on_loop = [&] {
        if (v.ui.closed()) return false;
        const Uint64 now = SDL_GetTicksNS();
        if (v.mem && now - last_draw >= 33000000ull) {
            last_draw = now;
            v.draw();
        }
        while (v.mem && !panel_shots.empty() && v.mach->vblank() >= panel_shots.front()) {
            const std::string stem = args.screenshot.empty() ? std::string("viewer") : args.screenshot.substr(0, args.screenshot.rfind('.'));
            const Tab was = v.tab;
            for (int t = 0; t < 4; ++t) {
                v.tab = static_cast<Tab>(t);
                v.draw();
                v.ui.snapshot = std::format("{}_{}_tab{}.bmp", stem, panel_shots.front(), t);
                v.draw();
            }
            v.tab = was;
            panel_shots.erase(panel_shots.begin());
        }
        return true;
    };
    log::info("ac3viewer: disc {}, cards {}, boot {}", args.disc, gc.cards_dir, args.boot.empty() ? "(none)" : args.boot);
    const int code = play_game(gc);
    delete tracer;
    v.ui.destroy();
    renderer.shutdown();
    window.destroy();
    SDL_Quit();
    log::close_file();
    return code;
}
