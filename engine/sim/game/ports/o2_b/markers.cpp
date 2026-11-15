#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kScreens = 0x80112DC0u, kMenu = 0x80112DC4u;
constexpr u32 kTimer = 368, kState = 370, kRow = 372, kBit = 374;
constexpr u32 kStaging = 112, kVisible = 240;
u32 fade_value(Memory& m, u32 fade) {
    return static_cast<u32>(m.r16s(m.r32(m.r32(kScreens) + 80) + 2 * fade));
}
}

u32 Game::o2_marker_tick() {
    cover(0x800E1258u);
    Frame frame(*this, 24);
    u32 screens = m.r32(kScreens);
    const s32 state = m.r16s(screens + kState);
    if (state == 0 || state == 2) return 0;
    if (state == 1) {
        if (m.r16s(screens + kTimer) < 120) m.w16(screens + kTimer, m.r16(screens + kTimer) + 1);
        else {
            m.w16(screens + kState, 2);
            o2_hangar_effect2_begin();
        }
    }
    screens = m.r32(kScreens);
    if (m.r16s(screens + kState) == 3) {
        if (m.r16s(screens + kTimer) > 0) m.w16(screens + kTimer, m.r16(screens + kTimer) - 1);
        else m.w16(screens + kState, 0);
    }
    screens = m.r32(kScreens);
    const u32 flags = m.r32(screens + 92);
    const bool visible = m.r16s(screens + kTimer) >= 31;
    m.w8(flags + 2, visible ? 1u : 0u);
    return visible ? 1u : flags;
}

u32 Game::o2_marker_spread() {
    cover(0x800E1334u);
    Frame frame(*this, 8);
    const u32 screens = m.r32(kScreens);
    if (m.r16s(screens + kState) != 1) return 0;
    for (u32 row = 0; row < 32; ++row) {
        const u32 staging = screens + kStaging + 4 * row;
        const u32 visible = m.r32(screens + kVisible + 4 * row);
        u32 previous = m.r32(staging - 4), current = m.r32(staging), next = m.r32(staging + 4);
        current |= (visible << 1) | (visible >> 1);
        const u32 wide = visible | (visible << 1) | (visible >> 1);
        if (row & 1) {
            if (row < 31) next |= visible;
            if (row > 0) previous |= wide;
        } else {
            if (row < 31) next |= wide;
            if (row > 0) previous |= visible;
        }
        m.w32(staging - 4, previous); m.w32(staging, current); m.w32(staging + 4, next);
    }
    const u32 updated = m.r32(kScreens);
    for (u32 row = 0; row < 32; ++row) {
        m.w32(updated + kVisible + 4 * row, m.r32(updated + kVisible + 4 * row) | m.r32(updated + kStaging + 4 * row));
        m.w32(updated + kStaging + 4 * row, 0);
    }
    return 0;
}

void Game::o2_marker_map_draw() {
    cover(0x800E14D0u);
    Frame frame(*this, 16);
    const u32 scene = m.r32(g::kScene), screens = m.r32(kScreens);
    const u32 buffer = m.r32(scene + 4), selected = screens + 22216 + 40 * buffer;
    const s32 row = m.r16s(screens + kRow), bit = m.r16s(screens + kBit);
    const s32 x = 150 + 5 * row, y = 190 + 10 * bit + 5 * ((row + 1) % 2);
    for (u32 point = 0; point < 4; ++point) {
        m.w16(selected + 8 + 8 * point, static_cast<u32>(x + ((point & 1) ? 12 : 0)));
        m.w16(selected + 10 + 8 * point, static_cast<u32>(y + ((point & 2) ? 20 : 0)));
    }
    s32 pulse = 64 - m.r32s(scene + 32) % 128;
    if (pulse < 0) pulse = -pulse;
    for (u32 channel = 0; channel < 3; ++channel) m.w8(selected + 4 + channel, static_cast<u32>(2 * pulse));
    ot_link(o2_op_slot(m.r32(m.r32(kMenu) + 8)), selected);
    const u32 current = m.r32(kScreens);
    u32 packet = current + 376 + 10920 * m.r32(m.r32(g::kScene) + 4);
    const s32 timer = m.r16s(current + kTimer);
    const u32 ot = o2_op_slot(m.r32(m.r32(kMenu) + 8));
    for (u32 cellRow = 0; cellRow < 32; ++cellRow) {
        const u32 cells = m.r32(0x8010AD2Cu + 4 * cellRow) & m.r32(m.r32(kScreens) + kVisible + 4 * cellRow);
        for (u32 cellBit = 0; cellBit < 32; ++cellBit) {
            if (!(cells & (1u << cellBit))) continue;
            const u32 cx = 150 + 5 * cellRow;
            const u32 cy = 190 + 5 * ((cellRow + 1) % 2) + 10 * cellBit;
            for (u32 point = 0; point < 4; ++point) {
                m.w16(packet + 8 + 8 * point, cx + ((point & 1) ? 8u : 0u));
                m.w16(packet + 10 + 8 * point, cy + ((point & 2) ? 12u : 0u));
            }
            if (timer >= 31) {
                const s32 colour = 128 - (128 * (timer - 30)) / 90;
                for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, static_cast<u32>(colour));
            }
            ot_link(ot, packet);
            packet += 40;
        }
    }
}

void Game::o2_marker_grid_draw() {
    cover(0x800E18B8u);
    Frame frame(*this, 40);
    const s32 value = static_cast<s32>(fade_value(m, 2));
    const s32 state = o2_fade_state(2);
    if (state != 1 && state != 2 && state != 3) return;
    const bool full = state == 2;
    for (s32 i = 0; i < 17 && (full || i <= value); ++i) {
        s32 width = full ? 140 : 7 * (value - i);
        if (width >= 141) width = 140;
        m.w32(frame.at(16), 0);
        o2_menu_tile(168, static_cast<u32>(258 + 9 * i), static_cast<u32>(static_cast<s16>(width)), 2, 0);
    }
    s32 width = full ? 75 : (75 * value) / 17;
    if (width >= 76) width = 75;
    m.w32(frame.at(16), 0);
    o2_menu_tile(232, 160, static_cast<u32>(static_cast<s16>(width)), 2, 0);
    for (s32 i = 0; i < 16 && (full || i <= value); ++i) {
        s32 height = full ? 242 : (242 * (value - i)) / 20;
        if (height >= 243) height = 242;
        const u32 x = static_cast<u32>(232 + 5 * i);
        o2_menu_line(x, 160, x, static_cast<u32>(static_cast<s16>(height + 160)));
    }
    s32 height = full ? 144 : (144 * value) / 17;
    if (height >= 145) height = 144;
    o2_menu_line(168, 258, 168, static_cast<u32>(static_cast<s16>(height + 258)));
}
}
