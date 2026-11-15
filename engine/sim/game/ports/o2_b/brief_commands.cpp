#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kCursor = 20;
constexpr u32 kImageRects = 5200, kPanelRecords = 1324;
u32 take_command(Memory& m, u32 bytes) {
    const u32 briefing = m.r32(kBriefing), command = m.r32(briefing + kCursor);
    m.w32(briefing + kCursor, command + bytes);
    return command;
}
u32 command_record(Memory& m, u32 command, u32 base, u32 stride) {
    return m.r32(kBriefing) + base + stride * static_cast<u32>(m.r16s(command));
}
u32 expand_rect(Memory& m, u32 record, u32 command) {
    m.w16(record, 1);
    for (u32 time = 2; time <= 6; time += 2) m.w16(record + time, m.r16(command + time));
    m.w16(record + 8, m.r16(command + 8) + m.r16s(command + 12) / 2);
    const s32 height = m.r16s(command + 14);
    const u32 y = m.r16(command + 10);
    m.w16(record + 12, 1); m.w16(record + 14, 2);
    m.w16(record + 10, y + height / 2 - 1);
    for (u32 half = 0; half < 4; ++half) m.w16(record + 16 + 2 * half, m.r16(command + 8 + 2 * half));
    return m.r16(command + 14);
}
u32 collapse_rect(Memory& m, u32 record, u32 command) {
    m.w16(record, 4); m.w16(record + 2, m.r16(command + 2));
    const u32 x = m.r16(record + 16), width = m.r16(record + 20), height = m.r16(record + 22);
    m.w16(record + 4, m.r16(command + 4));
    const u32 endTime = m.r16(command + 6), y = m.r16(record + 18);
    m.w16(record + 8, x); m.w16(record + 10, y);
    m.w16(record + 12, width); m.w16(record + 14, height);
    m.w16(record + 20, 1); m.w16(record + 22, 2);
    m.w16(record + 16, x + static_cast<s16>(width) / 2);
    const u32 bottom = y + static_cast<s16>(height) / 2 - 1;
    m.w16(record + 6, endTime); m.w16(record + 18, bottom);
    return bottom;
}
}

u32 Game::o2_brief_image_style_command() {
    cover(0x800ED190u);
    const u32 command = take_command(m, 16), record = command_record(m, command, kImageRects, 32);
    for (u32 half = 0; half < 4; ++half) m.w16(record + 24 + 2 * half, m.r16(command + 8 + 2 * half));
    const s32 mode = m.r16s(command + 2);
    const u32 abr = m.r16(command + 2) < 4 ? static_cast<u32>(mode) : 0;
    const u32 page = 0x11Du | ((abr & 3) << 5);
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packet = command_record(m, command, 3916 + 640 * buffer, 40);
        m.w8(packet + 3, 9); m.w8(packet + 7, mode == -1 ? 0x2Cu : 0x2Eu);
        m.w8(packet + 4, m.r8(command + 4)); m.w8(packet + 5, m.r8(command + 5));
        m.w16(packet + 22, page); m.w16(packet + 14, 0);
        m.w8(packet + 6, m.r8(command + 6));
    }
    return 0;
}

u32 Game::o2_brief_image_expand_command() {
    cover(0x800ED2B4u);
    const u32 command = take_command(m, 16);
    return expand_rect(m, command_record(m, command, kImageRects, 32), command);
}

u32 Game::o2_brief_image_collapse_command() {
    cover(0x800ED398u);
    const u32 command = take_command(m, 16);
    return collapse_rect(m, command_record(m, command, kImageRects, 32), command);
}

u32 Game::o2_brief_panel_texture_command(u32 inheritedScale) {
    cover(0x800ED498u);
    const u32 command = take_command(m, 24), record = command_record(m, command, kPanelRecords, 160);
    m.w16(record + 24, m.r16(command + 16));
    const u32 mode = m.r8(command + 23), scale = mode == 0 ? 4u : mode == 1 ? 2u : mode == 2 ? 1u : inheritedScale;
    const u32 cel = record + 32;
    const u32 startU = (m.r16(command + 4) & 63) * scale;
    const s32 transparency = m.r16s(command + 2);
    m.w16(cel, startU);
    m.w16(cel + 2, m.r8(command + 6));
    m.w16(cel + 4, static_cast<u32>(m.r16s(command + 8)) * scale);
    m.w16(cel + 6, m.r16(command + 10));
    const u32 flags = m.r16(command + 6);
    const u32 page = ((m.r8(command + 23) & 3) << 7) | ((m.r16(command + 2) & 3) << 5) |
                     ((flags & 0x100) >> 4) | ((m.r16(command + 4) & 0x3FF) >> 6) | ((flags & 0x200) << 2);
    m.w16(cel + 8, page);
    m.w16(cel + 10, (m.r16(command + 14) << 6) | ((m.r16(command + 12) >> 4) & 63));
    for (u32 channel = 0; channel < 3; ++channel) m.w8(cel + 12 + channel, m.r8(command + 20 + channel));
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 packet = command_record(m, command, 44 + 640 * buffer, 40);
        m.w8(packet + 3, 9); m.w8(packet + 7, transparency == -1 ? 0x2Cu : 0x2Eu);
        for (u32 channel = 0; channel < 3; ++channel) m.w8(packet + 4 + channel, m.r8(command + 20 + channel));
        m.w16(packet + 22, m.r16(cel + 8)); m.w16(packet + 14, m.r16(cel + 10));
        for (u32 point = 0; point < 4; ++point) {
            m.w8(packet + 12 + 8 * point, m.r8(cel) + ((point & 1) ? m.r8(cel + 4) - 1u : 0u));
            m.w8(packet + 13 + 8 * point, m.r8(cel + 2) + ((point & 2) ? m.r8(cel + 6) - 1u : 0u));
        }
    }
    return 0;
}

u32 Game::o2_brief_panel_expand_command() {
    cover(0x800ED6E8u);
    const u32 command = take_command(m, 16);
    return expand_rect(m, command_record(m, command, kPanelRecords, 160), command);
}

u32 Game::o2_brief_panel_move_command() {
    cover(0x800ED7D4u);
    const u32 command = take_command(m, 16), record = command_record(m, command, kPanelRecords, 160);
    m.w16(record, 3); m.w16(record + 2, m.r16(command + 2));
    const u32 x = m.r16(record + 16), width = m.r16(record + 20), height = m.r16(record + 22);
    m.w16(record + 4, m.r16(command + 4));
    const u32 endTime = m.r16(command + 6), y = m.r16(record + 18);
    m.w16(record + 8, x); m.w16(record + 12, width); m.w16(record + 14, height);
    m.w16(record + 10, y); m.w16(record + 6, endTime);
    for (u32 half = 0; half < 4; ++half) m.w16(record + 16 + 2 * half, m.r16(command + 8 + 2 * half));
    return m.r16(command + 14);
}

u32 Game::o2_brief_panel_collapse_command() {
    cover(0x800ED880u);
    const u32 command = take_command(m, 16);
    return collapse_rect(m, command_record(m, command, kPanelRecords, 160), command);
}

u32 Game::o2_brief_panel_cel_command() {
    cover(0x800ED958u);
    const u32 command = take_command(m, 24);
    const u32 cel = command_record(m, command, kPanelRecords, 160) + 32 +
                    16 * static_cast<u32>(m.r16s(command + 16));
    const u32 startU = (m.r16(command + 4) & 63) * 4;
    const bool semi = m.r16s(command + 2) != -1;
    m.w16(cel, startU);
    m.w16(cel + 2, m.r8(command + 6));
    m.w16(cel + 4, static_cast<u32>(m.r16s(command + 8)) * 4);
    m.w16(cel + 6, m.r16(command + 10));
    const u32 flags = m.r16(command + 6);
    const u32 page = ((m.r8(command + 23) & 3) << 7) | (semi ? 32u : 0u) |
                     ((flags & 0x100) >> 4) | ((flags & 0x200) << 2) | ((m.r16(command + 4) & 0x3FF) >> 6);
    m.w16(cel + 8, page);
    m.w16(cel + 10, (m.r16(command + 14) << 6) | ((m.r16(command + 12) >> 4) & 63));
    for (u32 channel = 0; channel < 3; ++channel) m.w8(cel + 12 + channel, m.r8(command + 20 + channel));
    return m.r8(command + 22);
}

u32 Game::o2_brief_panel_flipbook_command() {
    cover(0x800EDA60u);
    const u32 command = take_command(m, 4), record = command_record(m, command, kPanelRecords, 160);
    m.w16(record, 5);
    const u32 interval = m.r16(command + 2);
    const u32 rect[] = {m.r16(record + 16), m.r16(record + 18), m.r16(record + 20), m.r16(record + 22)};
    m.w16(record + 28, 0); m.w16(record + 30, 0); m.w16(record + 26, interval);
    for (u32 half = 0; half < 4; ++half) m.w16(record + 8 + 2 * half, rect[half]);
    return interval;
}

u32 Game::o2_brief_image_layout_command(u32 returnAddress) {
    cover(0x800EDAD4u);
    Frame frame(*this, 64);
    m.w32(frame.at(56), returnAddress);
    const u32 briefing = m.r32(kBriefing), command = take_command(m, 20);
    const u32 image = frame.at(24), uv = frame.at(32), palette = frame.at(40), screen = frame.at(48);
    for (u32 half = 0; half < 4; ++half) m.w16(image + 2 * half, m.r16(command + 2 * half));
    for (u32 half = 0; half < 4; ++half) m.w16(uv + 2 * half, m.r16(command + 2 * half));
    m.w16(palette, m.r16(command + 12)); m.w16(palette + 2, m.r16(command + 14));
    m.w16(palette + 4, 256); m.w16(palette + 6, 1);
    m.w16(screen, m.r16(command + 16)); m.w16(screen + 2, m.r16(command + 18));
    m.w16(screen + 4, m.r16(command + 4)); m.w16(screen + 6, static_cast<u32>(m.r16s(command + 6)) * 2);
    const u32 pack = m.r32(briefing + 3892);
    m.w32(frame.at(16), pack);
    return o2_image_open(image, uv, palette, screen, pack, m.r32s(frame.at(20)));
}
}
