#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>

namespace ac3::sim {
u32 Game::effect_rounded_segment(u32 ot, u32 a, u32 b, u32 work) {
    cover(0x80091CE8u);
    Frame frame(*this, 56);
    const s32 dx = m.r16s(a + 12) - m.r16s(b + 12);
    const s32 dy = m.r16s(a + 14) - m.r16s(b + 14);
    m.w32(work + 32, static_cast<u32>(ratan2_lib(-dx, dy)));
    const s32 length = length3(dx, dy, 0);
    for (u32 i = 0; i < 8; ++i) m.w32(work + 4 * i, gte.cfc2(i));
    const u32 h = m.r16(g::kScratch + 26);
    gte.mtc2(1, h);
    gte.mtc2(3, h);
    gte.mtc2(5, h);
    gte.ctc2(7, 0);
    const u32 start = m.r32(g::kScratch + 4);
    u32 packet = start;
    const s32 width_difference = wrap::sub(m.r32s(a), m.r32s(b));
    const s32 magnitude = width_difference < 0 ? wrap::neg(width_difference) : width_difference;
    if (magnitude >= length) {
        packet = effect_round_disc(packet, work, width_difference < 0 ? b : a);
    } else {
        const s32 numerator = static_cast<s32>(static_cast<u32>(width_difference) << 14);
        if (!length) unported(0x80091E18u, "rounded segment divide by zero");
        if (length == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(0x80091E30u, "rounded segment divide overflow");
        const s32 tangent_angle = asin_table(static_cast<s32>(wrap::div(numerator, length).lo)) >> 2;
        m.w32(work + 36, static_cast<u32>(1024 + tangent_angle));
        packet = effect_round_end(packet, work, a);
        m.w32(work + 36, static_cast<u32>(1024 - tangent_angle));
        m.w32(work + 32, m.r32(work + 32) + 2048);
        packet = effect_round_end(packet, work, b);
        const u32 shared = m.r32(a + 8) & m.r32(b + 8);
        for (u32 side = 0; side < 2; ++side) {
            const u32 a_code = side ? 24 : 16, b_code = side ? 16 : 24;
            if (shared & m.r32(a + a_code) & m.r32(b + b_code)) continue;
            m.w32(packet, ((packet + 36) & 0xFFFFFFu) | 0x08000000u);
            m.w32(packet + 8, m.r32(a + 12));
            m.w32(packet + 16, m.r32(b + 12));
            m.w32(packet + 24, m.r32(a + a_code + 4));
            m.w32(packet + 32, m.r32(b + b_code + 4));
            m.w32(packet + 28, 0x00020202u);
            m.w32(packet + 20, 0x00020202u);
            m.w32(packet + 4, (m.r32(a + 4) & 0xFFFFFFu) | 0x3A000000u);
            m.w32(packet + 12, m.r32(b + 4));
            packet += 36;
        }
    }
    u32 result = m.r32(g::kScratch + 4);
    if (result != packet) {
        const u32 first = result;
        result <<= 8;
        for (u32 byte = 0; byte < 3; ++byte) {
            m.w8(packet - 36 + byte, m.r8(ot + byte));
            m.w8(ot + byte, first >> (8 * byte));
        }
        m.w32(g::kScratch + 4, packet);
    }
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + 4 * i));
    gte.ctc2(5, 0);
    gte.ctc2(6, 0);
    return result;
}

u32 Game::draw_effect_segment(u32 context, u32 work, u32 record) {
    cover(0x8009236Cu);
    Frame frame(*this, 56);
    const s32 kind = m.r16s(record + 2), angle = m.r16s(record);
    const u32 shift = kind == 2 ? 15 : 12;
    s32 camera[3], first[3], second[3];
    for (u32 i = 0; i < 3; ++i) {
        camera[i] = m.r32s(context + 8 + 4 * i);
        first[i] = wrap::sub(m.r32s(record + 4 + 4 * i), camera[i]) >> shift;
        if (static_cast<u32>(first[i]) + 0x8000u > 0xFFFFu) return 1;
    }
    for (u32 i = 0; i < 3; ++i) gte.mtc2(9 + i, static_cast<u32>(first[i]));
    second[0] = wrap::sub(m.r32s(record + 16), camera[0]) >> shift;
    gte.op(0x49E012u);
    if (static_cast<u32>(second[0]) + 0x8000u > 0xFFFFu) return 1;
    for (u32 i = 1; i < 3; ++i) {
        second[i] = wrap::sub(m.r32s(record + 16 + 4 * i), camera[i]) >> shift;
        if (static_cast<u32>(second[i]) + 0x8000u > 0xFFFFu) return 1;
    }
    for (u32 i = 0; i < 3; ++i) m.w32(work + 4 + 4 * i, gte.mfc2(25 + i));
    for (u32 i = 0; i < 3; ++i) gte.mtc2(9 + i, static_cast<u32>(second[i]));
    const s32 near_depth = std::max(1, m.r16s(context + 26) >> 2);
    gte.op(0x49E012u);
    for (u32 i = 0; i < 3; ++i) m.w32(work + 20 + 4 * i, gte.mfc2(25 + i));
    if (!clip_effect_segment(work + 4, work + 20, near_depth, work + 36)) return 4096;
    constexpr u32 kIdentity = 284, kSavedRotation = 252, kOuterA = 116, kOuterB = 148, kRoundWork = 180;
    m.w32(work + kIdentity, 4096);
    m.w32(work + kIdentity + 8, 4096);
    m.w32(work + kIdentity + 16, 4096);
    m.w32(work + kIdentity + 4, 0);
    m.w32(work + kIdentity + 12, 0);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + kIdentity + 4 * i));
    s32 az = m.r32s(work + 12), bz = m.r32s(work + 28);
    gte.mtc2(0, m.r16(work + 4) | (m.r32(work + 8) << 16));
    gte.mtc2(1, static_cast<u32>(az));
    gte.mtc2(2, m.r16(work + 20) | (m.r32(work + 24) << 16));
    gte.mtc2(3, static_cast<u32>(bz));
    gte.op(0x280030u);
    const s32 depth = std::clamp(std::min(az, bz) >> (15 - shift), 0, 4094);
    m.w32(work, m.r32(context) + 4u * static_cast<u32>(depth));
    s32 fade = angle;
    if (kind != 3) {
        fade = m.r16s(0x800B7104u + 4 * (static_cast<u32>(angle) & 0xFFFu));
        if (!fade) fade = -4096;
    }
    m.w32(work + kOuterA + 12, gte.mfc2(12));
    m.w32(work + kOuterB + 12, gte.mfc2(13));
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + kSavedRotation + 4 * i));
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 48);
    gte.mtc2(8, static_cast<u32>(fade));
    gte.mtc2(6, 0);
    gte.mtc2(20, 0x00279327u);
    gte.mtc2(21, 0x00838383u);
    if (!az) az = 1;
    if (!bz) bz = 1;
    gte.op(0xF8002Au);
    const s32 h = m.r16s(context + 26);
    const s32 scale_a = kind == 3 ? m.r8(record + 4) : kind == 2 ? 12 : 8;
    const s32 scale_b = kind == 3 ? m.r8(record + 8) : kind == 2 ? 12 : 8;
    const s32 width_a = std::max(2, static_cast<s32>(wrap::div(scale_a * h, az).lo));
    const s32 width_b = std::max(2, static_cast<s32>(wrap::div(scale_b * h, bz).lo));
    m.w32(work + kOuterA, static_cast<u32>(width_a));
    m.w32(work + kOuterB, static_cast<u32>(width_b));
    const u32 glow = gte.mfc2(20), core = gte.mfc2(21);
    m.w32(work + kOuterB + 4, glow);
    m.w32(work + kOuterA + 4, glow);
    effect_rounded_segment(m.r32(work), work + kOuterA, work + kOuterB, work + kRoundWork);
    m.w32(work + kOuterA, static_cast<u32>(width_a >> 1));
    m.w32(work + kOuterB + 4, core);
    m.w32(work + kOuterA + 4, core);
    m.w32(work + kOuterB, static_cast<u32>(width_b >> 1));
    return effect_rounded_segment(m.r32(work), work + kOuterA, work + kOuterB, work + kRoundWork);
}

void Game::draw_effect_segments(u32 records) {
    cover(0x80092850u);
    Frame frame(*this, 40);
    u32 mask = m.r16(records);
    if (!mask) return;
    for (u32 i = 0; i < 5; ++i) m.w32(g::kScratch + 280 + 4 * i, gte.cfc2(i));
    for (u32 record = records + 4; mask; mask >>= 1, record += 28)
        if (mask & 1u) draw_effect_segment(g::kScratch, g::kScratch + 28, record);
}
}
