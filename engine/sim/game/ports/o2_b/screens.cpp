#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kClipScratch = 0x80112DB4u;
constexpr u32 kEntries = 0x16Cu, kMotionTimer = 0x1A4u;
constexpr u32 kEntryBytes = 12u;
constexpr u32 kX = 4u, kY = 6u, kSpeed = 8u, kState = 10u;
constexpr u32 kEntering = 0u, kStill = 1u, kLeaving = 2u, kGone = 3u;
u32 entry(Memory& m, u32 screen) {
    return m.r32(m.r32(kMenu) + kEntries) + kEntryBytes * screen;
}
}

u32 Game::o2_line_command(u32 command, u32 ot, u32 screenEntry) {
    cover(0x800E01C4u);
    Frame frame(*this, 0x30);
    if (!o2_op_on(command, 12)) return command + 20;
    const u32 screens = m.r32(kScreens);
    const u32 packet = m.r32(screens + 4);
    m.w32(screens + 4, packet + 16);
    const u32 scratch = m.r32(kClipScratch);
    m.w32(scratch, m.r32(command));
    m.w32(scratch + 4, m.r32(command + 4));
    if (!o2_clip()) return command + 20;
    const u32 clipped = m.r32(kClipScratch);
    for (u32 point = 0; point < 2; ++point) {
        m.w16(packet + 8 + 4 * point, m.r16(clipped + 4 * point) + m.r16(screenEntry + kX));
        m.w16(packet + 10 + 4 * point, m.r16(clipped + 2 + 4 * point) + m.r16(screenEntry + kY));
    }
    m.w8(packet + 3, 3);
    m.w32(packet + 4, m.r8(command + 8) | (m.r8(command + 9) << 8) | (m.r8(command + 10) << 16));
    m.w8(packet + 7, m.r8(command + 11) ? 0x42u : 0x40u);
    ot_link(o2_op_slot(ot), packet);
    return command + 20;
}

void Game::o2_slide_screen_side(u32 screen) {
    cover(0x800E03D4u);
    Frame frame(*this, 0x20);
    u32 e = entry(m, screen);
    switch (m.r16s(e + kState)) {
    case kEntering: {
        const s32 timer = m.r32s(m.r32(kMenu) + kMotionTimer);
        if (timer <= 0) return;
        if (timer < 27) {
            m.w16(e + kX, m.r16(e + kX) + m.r16(e + kSpeed));
            e = entry(m, screen);
            if (m.r16s(e + kSpeed) > 0) m.w16(e + kSpeed, m.r16(e + kSpeed) - 1);
        } else if (timer < 29) {
            m.w16(e + kX, m.r16(e + kX) + 3);
        } else if (timer < 31) {
            if (timer == 29) sound_play(92);
            e = entry(m, screen);
            m.w16(e + kX, m.r16(e + kX) - 1);
        } else {
            m.w16(e + kSpeed, 0);
            m.w16(entry(m, screen) + kState, kStill);
        }
        return;
    }
    case kLeaving:
        m.w16(e + kX, m.r16(e + kX) - m.r16(e + kSpeed));
        e = entry(m, screen);
        if (m.r16s(e + kX) < -319) m.w16(e + kState, kGone);
        else m.w16(e + kSpeed, m.r16(e + kSpeed) + 2);
        return;
    default: return;
    }
}

void Game::o2_slide_screen_top(u32 screen) {
    cover(0x800E056Cu);
    Frame frame(*this, 0x20);
    u32 e = entry(m, screen);
    switch (m.r16s(e + kState)) {
    case kEntering: {
        if (m.r32(m.r32(kMenu) + kMotionTimer) == 43) sound_play(92);
        const s32 timer = m.r32s(m.r32(kMenu) + kMotionTimer);
        e = entry(m, screen);
        if (timer < 31) {
            m.w16(e + kY, m.r16(e + kY) + m.r16(e + kSpeed));
            e = entry(m, screen);
            if (m.r16s(e + kSpeed) > 0) m.w16(e + kSpeed, m.r16(e + kSpeed) - 1);
        } else if (timer < 33) m.w16(e + kY, m.r16(e + kY) + 8);
        else if (timer < 35) m.w16(e + kY, m.r16(e + kY) - 4);
        else {
            m.w16(e + kState, kStill);
            m.w16(m.r32(m.r32(kMenu) + kEntries) + 0x5C, static_cast<u32>(-44));
        }
        if (m.r32(m.r32(kMenu) + kMotionTimer) == 33) sound_play(92);
        const u32 menu = m.r32(kMenu);
        m.w32(menu + kMotionTimer, m.r32(menu + kMotionTimer) + 1);
        return;
    }
    case kStill: {
        const u32 menu = m.r32(kMenu);
        if (m.r32s(menu + kMotionTimer) < 400)
            m.w32(menu + kMotionTimer, m.r32(menu + kMotionTimer) + 1);
        return;
    }
    case kLeaving: {
        m.w16(e + kY, m.r16(e + kY) + m.r16(e + kSpeed));
        e = entry(m, screen);
        if (m.r16s(e + kY) < -479) m.w16(e + kState, kGone);
        else m.w16(e + kSpeed, m.r16(e + kSpeed) - 2);
        const u32 menu = m.r32(kMenu);
        if (m.r32s(menu + kMotionTimer) > 0)
            m.w32(menu + kMotionTimer, m.r32(menu + kMotionTimer) - 1);
        return;
    }
    default: return;
    }
}

void Game::o2_slide_screen_bottom(u32 screen) {
    cover(0x800E0798u);
    u32 e = entry(m, screen);
    switch (m.r16s(e + kState)) {
    case kEntering:
        if (m.r16s(m.r32(m.r32(kMenu) + kEntries) + 0x46) != kStill) return;
        m.w16(e + kY, m.r16(e + kY) + m.r16(e + kSpeed));
        e = entry(m, screen);
        if (m.r16s(e + kY) < 0) m.w16(e + kY, 0);
        e = entry(m, screen);
        if (m.r16s(e + kSpeed) < 0) m.w16(e + kSpeed, m.r16(e + kSpeed) + 2);
        else m.w16(e + kState, kStill);
        return;
    case kStill:
        if (m.r16s(e + kY) < 0) m.w16(e + kY, 0);
        return;
    case kLeaving:
        m.w16(e + kY, m.r16(e + kY) + m.r16(e + kSpeed));
        e = entry(m, screen);
        if (m.r16s(e + kY) >= 480) m.w16(e + kState, kGone);
        else m.w16(e + kSpeed, m.r16(e + kSpeed) + 4);
        return;
    default: return;
    }
}

u32 Game::o2_menu_line(u32 x0, u32 y0, u32 x1, u32 y1) {
    cover(0x800E0F88u);
    const u32 menu = m.r32(kMenu), packet = m.r32(menu + 0x274);
    m.w32(menu + 0x274, packet + 16);
    m.w8(packet + 3, 3);
    m.w8(packet + 7, 0x40);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 0x60);
    m.w16(packet + 8, x0); m.w16(packet + 10, y0);
    m.w16(packet + 12, x1); m.w16(packet + 14, y1);
    const u32 slot = o2_op_slot(m.r32(menu + 8) + 0x24);
    ot_link(slot, packet);
    return m.r32(slot);
}

u32 Game::o2_menu_tile(u32 x, u32 y, u32 w, u32 h, u32 semi) {
    cover(0x800E1040u);
    const u32 menu = m.r32(kMenu), packet = m.r32(menu + 0x274);
    m.w32(menu + 0x274, packet + 16);
    m.w8(packet + 3, 3);
    m.w8(packet + 7, (semi & 0xFFu) ? 0x62u : 0x60u);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, 0x60);
    m.w16(packet + 8, x); m.w16(packet + 10, y);
    m.w16(packet + 12, w); m.w16(packet + 14, h);
    const u32 slot = o2_op_slot(m.r32(menu + 8) + 0x24);
    ot_link(slot, packet);
    return m.r32(slot);
}

u32 Game::o2_marker_select(u32 state, u32 timer) {
    cover(0x800E11F4u);
    const u32 screens = m.r32(kScreens);
    m.w16(screens + 0x172, state);
    m.w16(screens + 0x170, timer);
    const u32 mission = static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C));
    const u32 mapping = 0x8010ADACu + 4 * mission;
    const u32 row = static_cast<u32>(m.r16s(mapping));
    const u32 bit = wrap::shl(1u, static_cast<u32>(m.r16s(mapping + 2)));
    const u32 visible = screens + 0xF0 + 4 * row;
    const u32 mask = m.r32(visible) | bit;
    m.w32(visible, mask);
    m.w16(screens + 0x174, m.r16(mapping));
    m.w16(screens + 0x176, m.r16(mapping + 2));
    return mask;
}
}
