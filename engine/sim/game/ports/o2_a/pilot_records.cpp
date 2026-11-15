#include <algorithm>
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u, kList = 0x80112DBCu, kPanel = 0x80112DC0u;
constexpr u32 kHeld = 296, kPressed = 300, kRepeat = 304;
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 list(Memory& m) { return m.r32(kList); }
u32 mission(Memory& m) { return m.r8(list(m) + m.r8(list(m) + 0x3F)); }
u32 panel(Memory& m) { return m.r32(m.r32(kPanel) + 0x5C); }
}

u32 Game::o2_pilot_records() {
    cover(0x800DC43Cu);
    Frame frame(*this, 0x40);
    const u32 records = scene(m) + 0x95C;
    const u32 kind = o2_brief_kind(static_cast<s32>(mission(m)));
    const auto time = [&](u32 format, s32 t) {
        o2_text_printf_zero(format, static_cast<u32>(t / 1800), static_cast<u32>(t / 30 - 60 * (t / 1800)),
                            static_cast<u32>(100 * (t % 30) / 30));
    };
    o2_text_font(4);
    o2_text_semi(2);
    o2_text_rgb(0x80, 0x80, 0x80);
    o2_text_at(0x1D, 0x41);
    o2_text_printf(scene(m) + 0x948, 0, 0, 0);
    o2_text_semi(-1);
    o2_text_font(8);
    const u32 difficulty = m.r32(scene(m) + 0x394);
    const u32 table = difficulty == 0 ? 0x68u : difficulty == 1 ? 0xE0u : 0x158u;
    o2_text_at(0xEA, 0x98);
    time(0x800CC800u, m.r16(records + table + 2 * mission(m)));
    o2_text_font(3);
    o2_text_semi(2);
    o2_text_at(0xB4, 0x6E);
    o2_text_printf(0x800CC80Cu, m.r32(records + 0x6D4), 0, 0);
    o2_text_at(0xB4, 0x80);
    o2_text_printf(0x800CC80Cu, m.r32(records + 0x6D8), 0, 0);
    o2_text_at(0xB4, 0x93);
    o2_text_printf(0x800CC810u, m.r8(list(m) + 0x3D), 20, 0);
    o2_text_at(0xB4, 0xA7);
    o2_text_printf(0x800CC810u, m.r8(list(m) + 0x3E), 13, 0);
    o2_text_at(0x91, 0xD7);
    o2_text_printf(m.r32(0x8010A3BCu + 4 * mission(m)), 0, 0, 0);
    o2_text_at(0x5E, 0xFD);
    o2_text_printf(m.r32(0x8010A4B0u + 4 * kind), 0, 0, 0);
    o2_text_semi(1);
    o2_text_at(0xFD, 0xFB);
    time(0x800CC818u, m.r16(records + 0x248 + 2 * mission(m)));
    o2_text_at(0xAF, 0x11A);
    o2_text_printf(0x800CC80Cu, m.r32(records + 0x4F0 + 4 * mission(m)), 0, 0);
    o2_text_at(0xAF, 0x12F);
    o2_text_printf(0x800CC80Cu, m.r32(records + 0x5E0 + 4 * mission(m)), 0, 0);
    o2_text_font(8);
    o2_text_semi(2);
    o2_text_at(0x91, 0x167);
    const s32 total = std::clamp(m.r32s(records + 0x6D0), 0, 0x66FF2FF);
    o2_text_printf_zero(0x800CC828u, static_cast<u32>(total / 108000),
                        static_cast<u32>(total / 1800 - 60 * (total / 108000)),
                        static_cast<u32>(total / 30 - 60 * (total / 1800)));
    m.w8(panel(m) + 0x32, m.r8(records + 0x3B0 + mission(m)));
    m.w8(panel(m) + 0x33, m.r8(scene(m) + 0x394));
    m.w8(panel(m) + 0x35, m.r8(records + 1));
    const s32 cursor = m.r8(list(m) + 0x3F), last = static_cast<s32>(m.r8(list(m) + 0x3C)) - 1;
    o2_flag(0x31, (cursor < last ? 2u : 0u) | (cursor ? 1u : 0u));
    m.w8(panel(m) + 0x34, 0);
    const u32 repeat = m.r32(scene(m) + kRepeat);
    u32 step = 0;
    if (repeat & 8) {
        if (cursor < last) {
            m.w8(list(m) + 0x3F, static_cast<u32>(cursor + 1));
            sound_play(0x3D);
            step = 1;
        }
    } else if (repeat & 4) {
        if (cursor) {
            m.w8(list(m) + 0x3F, static_cast<u32>(cursor - 1));
            sound_play(0x3D);
            step = 2;
        }
    } else if ((m.r32(scene(m) + kPressed) & 0x20) && !(m.r32(scene(m) + kHeld) & 0x5F)) {
        o2_entry_enter(0, 1);
        m.w8(m.r32(kDialog), 0);
        m.w8(m.r32(kDialog) + 1, 0);
        heap_set(m.r32(m.r32(kMenu) + 0x28));
        sound_play(0x3B);
        m.w32(scene(m) + kPressed, 0);
    }
    if (step) {
        m.w8(panel(m) + 0x34, step);
        const u32 picture = o2_brief_kind(static_cast<s32>(mission(m)));
        m.w32(frame.at(0x10), 1);
        o2_upload_tim_positioned(m.r32(m.r32(m.r32(kMenu) + 0x1DC) + 4 * picture), 0x3C0, 0x100, 0x300, 1);
    }
    const u32 shown = m.r8(scene(m) + 0x95D);
    m.w8(panel(m) + 0x35, shown);
    return shown;
}
}
