#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
void Game::draw_muzzle(u32 record) {
    cover(0x8009044Cu);
    Frame frame(*this, 48);
    constexpr u32 kWork = g::kScratch + 28;
    constexpr u32 kBeam = kWork + 16, kCorners = kWork + 40;
    const s32 kind = m.r16s(record + 4);
    const s32 age = static_cast<s32>(m.r32(record) - 2u);
    m.w32(kWork, static_cast<u32>(kind));
    m.w32(kWork + 4, static_cast<u32>(age));
    const u32 world = m.r32(g::kWorld);
    if (m.r32(m.r32(world + 40) + 80) != 1)
        muzzle_light(age, static_cast<u32>(kind), kWork);
    if (kind >= 5 || age < 0) return;
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    if (m.r32(view + 80) != 1 || (m.r32(view) & 0x200u)) return;
    u32 packet = m.r32(g::kScratch + 4);
    const u32 tick = m.r32(m.r32(g::kScene) + 40);
    const u32 noise = m.r32(m.r32(0x800BCF90u) + 4 * (tick & 255u));
    m.w32(kWork + 8, tick);
    m.w32(kWork + 12, noise);
    const u32 bucket = m.r32(g::kScratch) + 8;
    auto append_quad = [&](u32 at) {
        m.w32(at, m.r32(bucket) | 0x09000000u);
        m.w32(bucket, at & 0xFFFFFFu);
    };
    if (!age && kind != 3) {
        append_quad(packet);
        for (u32 i = 0; i < 4; ++i) m.w16(kWork + 24 + 2 * i, m.r16(record + 8 + 2 * i));
        if (kind == 0 || kind == 1 || kind == 2 || kind == 4) {
            static constexpr u32 kTexture[5] = {0x445660A0u, 0x449660B0u, 0x44D660C0u, 0, 0x455660D0u};
            static constexpr u32 kWidth[5][2] = {{1, 3}, {2, 4}, {3, 4}, {0, 0}, {6, 7}};
            const u32 u = 0xA0u + 16u * static_cast<u32>(kind == 4 ? 3 : kind);
            m.w32(kWork + 32, kWidth[kind][0]);
            m.w32(kWork + 36, kWidth[kind][1]);
            m.w32(packet + 4, 0x2E838383u);
            m.w32(packet + 12, kTexture[kind]);
            m.w32(packet + 20, 0x00386000u | (u + 15));
            m.w16(packet + 28, 0x9F00u | u);
            m.w16(packet + 36, 0x9F00u | (u + 15));
            if (kind <= 1) {
                m.w16(kWork + 24, m.r16(kWork + 24) + (noise & 1u));
                m.w16(kWork + 26, m.r16(kWork + 26) + ((noise >> 1) & 1u));
            }
        }
        beam_quad_corners(packet, kBeam);
        packet += 40;
    }
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 0);
    if (kind < 3) {
        gte.mtc2(8, noise & 0x7FFu);
        const u32 colour = kind == 0 ? 0x2E838383u : kind == 1 ? 0x2E738393u : 0x2E737383u;
        gte.mtc2(6, colour);
        m.w32(kWork + 32, kind == 0 ? 14 : kind == 1 ? 19 : 15);
        m.w32(kWork + 36, kind == 0 ? 16 : kind == 1 ? 21 : 18);
        gte.op(0x780010u);
        append_quad(packet);
        const u32 u = static_cast<u32>(static_cast<s32>(tick) % 5) << 5;
        m.w32(packet + 12, 0x40566000u | u);
        m.w32(packet + 20, 0x00386000u | (u + 31));
        m.w16(packet + 28, 0x9F00u | u);
        m.w16(packet + 36, 0x9F00u | (u + 31));
        m.w32(kWork + 24, m.r32(record + 16));
        m.w32(kWork + 28, m.r32(record + 20));
        m.w32(packet + 4, gte.mfc2(22));
        beam_quad_corners(kCorners, kBeam);
        const u32 first = (tick & 1u) ? 56u : 48u;
        const u32 second = (tick & 1u) ? 48u : 56u;
        const u32 third = (tick & 1u) ? 72u : 64u;
        const u32 fourth = (tick & 1u) ? 64u : 72u;
        m.w32(packet + 8, m.r32(kWork + first));
        m.w32(packet + 16, m.r32(kWork + second));
        m.w32(packet + 24, m.r32(kWork + third));
        m.w32(packet + 32, m.r32(kWork + fourth));
        packet += 40;
    } else {
        gte.mtc2(8, (static_cast<u32>(age) << 10) + (noise & 0x3FFu));
        gte.mtc2(6, kind == 3 ? 0x2E231333u : 0x2E332313u);
        const u32 centre = m.r32(record + 24);
        gte.op(0x780010u);
        const u32 jitter = (noise >> 7) & 7u;
        const u32 width = 100 - jitter, height = 90 - jitter;
        m.w32(kWork + 48, centre);
        m.w16(kWork + 56, m.r16(kWork + 48) - width);
        m.w16(kWork + 64, m.r16(kWork + 48) + width);
        m.w16(kWork + 58, m.r16(kWork + 50) - height);
        m.w16(kWork + 66, m.r16(kWork + 50) + height);
        const u32 colour = gte.mfc2(22), start = packet;
        for (u32 quadrant = 0; quadrant < 4; ++quadrant, packet += 40) {
            m.w32(packet, ((packet + 40) & 0xFFFFFFu) | 0x09000000u);
            m.w32(packet + 4, colour);
            m.w32(packet + 12, 0x459680E0u);
            m.w32(packet + 20, 0x003880FFu);
            m.w16(packet + 28, 0x9FE0);
            m.w16(packet + 36, 0x9FFF);
            m.w32(packet + 8, centre);
            m.w16(packet + 24, m.r16(kWork + 48));
            m.w16(packet + 18, m.r16(kWork + 50));
            const u32 x = m.r16(kWork + ((quadrant & 1u) ? 64 : 56));
            const u32 y = m.r16(kWork + ((quadrant & 2u) ? 66 : 58));
            m.w16(packet + 32, x);
            m.w16(packet + 16, x);
            m.w16(packet + 34, y);
            m.w16(packet + 26, y);
        }
        for (u32 i = 0; i < 3; ++i) {
            m.w8(packet - 40 + i, m.r8(bucket + i));
            m.w8(bucket + i, start >> (8 * i));
        }
    }
    m.w32(g::kScratch + 4, packet);
}
}
