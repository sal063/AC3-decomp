#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DDCu, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kStage = 252, kHighAxes = 236, kLowAxes = 240, kShape = 266;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
s32 axisOffset(s32 axis, s32 extent) { return (axis - 128) * extent / 128; }
}

u32 Game::o2_calibration_shape(u32 shape, u32 kind) {
    const u32 packet = m.r32(dialog(m) + 8), table = m.r32(m.r32(kMenu) + 8);
    u32 next = packet;
    if (kind == 0) next = o2_line_double(shape, packet, table);
    if (kind == 1) next = o2_poly_g4(shape, packet, table);
    if (kind == 2) next = o2_triangle_gouraud(shape, packet, table);
    if (kind == 3) next = o2_polyline4_double(shape, packet, table);
    m.w32(dialog(m) + 8, next);
    return next;
}

u32 Game::o2_controller_thresholds() {
    cover(0x800D91FCu);
    Frame frame(*this, 80);
    const u32 original = dialog(m), shape = original + kShape, stage = m.r32(original + kStage);
    const s32 axis = m.r16s(scene(m) + 100);
    const bool confirm = (m.r32(scene(m) + 300) & 64) != 0;
    if (stage == 0) {
        m.w32(frame.at(32), axis >= 128 ? static_cast<u32>(axisOffset(axis, 70)) : 0);
        m.w32(frame.at(36), axis >= 128 ? 0 : static_cast<u32>(axisOffset(axis, 70)));
        if (confirm && axis >= 145) { sound_play(60); m.w8(dialog(m) + 237, axis); m.w32(dialog(m) + kStage, 1); }
    } else if (stage == 1) {
        m.w32(frame.at(32), static_cast<u32>(axisOffset(m.r8(original + 237), 70)));
        m.w32(frame.at(36), axis >= 128 ? 0 : static_cast<u32>(axisOffset(axis, 70)));
        if (confirm && axis < 112) { sound_play(60); m.w8(dialog(m) + 241, axis); m.w32(dialog(m) + kStage, 2); }
    } else if (stage == 2) {
        m.w32(frame.at(32), static_cast<u32>(axisOffset(m.r8(original + 237), 70)));
        m.w32(frame.at(36), static_cast<u32>(axisOffset(m.r8(original + 241), 70)));
        if (confirm) {
            sound_play(60);
            const u32 target = scene(m) + 780;
            m.w8(target + 4, m.r8(dialog(m) + 241)); m.w8(target + 5, m.r8(scene(m) + 100)); m.w8(target + 6, m.r8(dialog(m) + 237));
            m.w8(dialog(m), 0); m.w8(dialog(m) + 1, 0); o2_entry_reset(2, 0); o2_entry_enter(0, 1);
        }
    }
    m.w8(flags(m) + 20, m.r8(dialog(m) + kStage) + 74);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(frame.at(24 + channel), 128);
    struct Line { u16 x0, y0, x1, y1; };
    constexpr Line outlines[] = {
        {130,40,150,40}, {130,110,150,110}, {130,180,150,180}, {130,40,130,180}, {150,40,150,180},
        {170,40,190,40}, {170,110,190,110}, {170,180,190,180}, {170,40,170,180}, {190,40,190,180}
    };
    for (const auto& line : outlines) {
        m.w16(frame.at(16), line.x0); m.w16(frame.at(18), line.y0);
        m.w16(frame.at(20), line.x1); m.w16(frame.at(22), line.y1);
        o2_calibration_shape(frame.at(16), 0);
    }
    const s32 right = m.r32s(frame.at(32)), left = m.r32s(frame.at(36));
    auto bar = [&](u32 x0, u32 x1, s32 offset) {
        m.w16(shape, x0); m.w16(shape + 2, 110); m.w16(shape + 4, x1); m.w16(shape + 6, 110);
        m.w16(shape + 8, x0); m.w16(shape + 10, static_cast<u32>(110 + offset));
        m.w16(shape + 12, x1); m.w16(shape + 14, static_cast<u32>(110 + offset));
        return o2_calibration_shape(shape, 1);
    };
    m.w8(shape + 24, 64); m.w8(shape + 26, 0); m.w8(shape + 28, 64); m.w8(shape + 30, 0);
    m.w8(shape + 25, 55); m.w8(shape + 29, 55);
    bar(130, 150, right); bar(130, 150, left); bar(170, 190, -left); bar(170, 190, -right);
    const s32 current = axisOffset(m.r16s(scene(m) + 100), 70);
    auto marker = [&](u32 x, u32 tip, s32 offset) {
        m.w16(shape, x); m.w16(shape + 2, static_cast<u32>(110 + offset));
        m.w16(shape + 4, tip); m.w16(shape + 6, static_cast<u32>(100 + offset));
        m.w16(shape + 8, tip); m.w16(shape + 10, static_cast<u32>(120 + offset));
        return o2_calibration_shape(shape, 2);
    };
    marker(130, 120, current);
    return marker(190, 200, -current);
}

u32 Game::o2_controller_extrema() {
    cover(0x800D9994u);
    Frame frame(*this, 80);
    const u32 original = dialog(m), shape = original + kShape;
    for (u32 vertex = 0; vertex < 4; ++vertex) for (u32 channel = 0; channel < 3; ++channel) m.w8(shape + 16 + 4 * vertex + channel, 128);
    auto diamond = [&](s32 centre, u32 xAxis, u32 yAxis) {
        const s32 x = axisOffset(m.r16s(scene(m) + xAxis), 35), y = axisOffset(m.r16s(scene(m) + yAxis), 70);
        const s32 points[] = {centre + x, 104 + y, centre + 3 + x, 110 + y, centre - 3 + x, 110 + y, centre + x, 116 + y};
        for (u32 i = 0; i < 8; ++i) m.w16(shape + 2 * i, static_cast<u32>(points[i]));
        o2_calibration_shape(shape, 1);
    };
    diamond(105, 104, 102); diamond(215, 100, 98);
    for (u32 i = 0; i < 4; ++i) {
        const s32 axis = m.r16s(scene(m) + 98 + 2 * i);
        const u32 low = dialog(m) + kLowAxes + i;
        if (axis < static_cast<s32>(m.r8(low))) m.w8(low, static_cast<u32>(axis));
        const u32 high = dialog(m) + kHighAxes + i;
        if (static_cast<s32>(m.r8(high)) < axis) m.w8(high, static_cast<u32>(axis));
    }
    m.w8(shape + 16, 240); m.w8(shape + 17, 110); m.w8(shape + 18, 0);
    for (u32 vertex = 1; vertex < 3; ++vertex) {
        m.w8(shape + 16 + 4 * vertex, 64); m.w8(shape + 17 + 4 * vertex, 55); m.w8(shape + 18 + 4 * vertex, 0);
    }
    struct ExtremaPoint { u16 yField, xField; };
    constexpr ExtremaPoint left[] = {{242,239}, {238,239}, {238,243}, {242,243}};
    constexpr ExtremaPoint right[] = {{240,237}, {236,237}, {236,241}, {240,241}};
    auto limitTriangle = [&](u32 centre, u32 origin, ExtremaPoint point) {
        const u32 current = dialog(m), y = 40 + m.r8(current + point.yField) * 140 / 256;
        const u32 x = origin + m.r8(current + point.xField) * 70 / 256;
        m.w16(shape, centre); m.w16(shape + 2, 110); m.w16(shape + 4, centre); m.w16(shape + 6, y);
        m.w16(shape + 8, x); m.w16(shape + 10, 110); o2_calibration_shape(shape, 2);
    };
    for (const auto& point : left) limitTriangle(105, 70, point);
    for (const auto& point : right) limitTriangle(215, 180, point);
    for (u32 channel = 0; channel < 3; ++channel) m.w8(frame.at(32 + channel), 128);
    m.w8(frame.at(35), 0); m.w8(frame.at(36), 0);
    constexpr u16 outlines[][8] = {
        {105,40,140,110,105,180,70,110}, {105,180,105,40,70,110,140,110},
        {215,40,250,110,215,180,180,110}, {215,180,215,40,180,110,250,110}
    };
    for (const auto& points : outlines) {
        for (u32 i = 0; i < 8; ++i) m.w16(frame.at(16 + 2 * i), points[i]);
        o2_calibration_shape(frame.at(16), 3);
    }
    const u32 current = dialog(m), stage = m.r32(current + kStage);
    bool advance = false;
    if (stage == 0) advance = m.r8(current + 240) < 64 && m.r8(current + 242) < 64;
    if (stage == 1) advance = m.r8(current + 236) >= 193 && m.r8(current + 238) >= 193;
    if (stage == 2) advance = m.r8(current + 241) < 64 && m.r8(current + 243) < 64;
    if (stage == 3) advance = m.r8(current + 237) >= 193 && m.r8(current + 239) >= 193;
    if (advance) m.w32(current + kStage, stage + 1);
    if (stage == 4 && (m.r32(scene(m) + 300) & 64)) {
        sound_play(60);
        const u32 target = scene(m) + 688;
        for (u32 i = 0; i < 4; ++i) {
            m.w8(target + 4 * i, m.r8(dialog(m) + kLowAxes + i));
            m.w8(target + 4 * i + 1, m.r8(scene(m) + 98 + 2 * i));
            m.w8(target + 4 * i + 2, m.r8(dialog(m) + kHighAxes + i));
        }
        m.w8(dialog(m), 0); m.w8(dialog(m) + 1, 0); o2_entry_reset(2, 0); o2_entry_enter(0, 1);
    }
    const u32 caption = m.r8(dialog(m) + kStage) + 69;
    m.w8(flags(m) + 20, caption);
    return caption;
}

u32 Game::o2_controller_calibration_tick() {
    cover(0x800DA444u);
    Frame frame(*this, 32);
    if (m.r16s(dialog(m) + 12) == 1) o2_controller_thresholds();
    if (m.r16s(dialog(m) + 12) == 3) o2_controller_extrema();
    const u32 state = scene(m), previous = m.r32(dialog(m) + 232);
    if (m.r16s(state + 342) == 2 || m.r32(state + 292) != previous) {
        m.w8(dialog(m), 1); const u32 current = dialog(m); m.w8(current + 1, 0); return current;
    }
    return previous;
}
}
