#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
namespace {
constexpr u32 kCursor = 4, kDepthShift = 12, kFlameWork = 16;
constexpr u32 kSubjectDepth = 1014, kSubjectOT = 1016, kFlameState = 1020;
void flame_link(Memory& memory, u32 at, u32 next) {
    for (u32 byte = 0; byte < 3; ++byte) memory.w8(at + byte, next >> (8 * byte));
}
}

u32 Game::draw_exhaust_particles(u32 context, u32 work) {
    cover(0x8008C644u);
    const u32 start = m.r32(context + kCursor), noise = m.r32(0x800BCF90u);
    u32 packet = start + 12;
    m.w32(start, (packet & 0xFFFFFFu) | 0x02000000u);
    m.w32(start + 4, 0xE6000001u);
    m.w32(start + 8, 0xE1000217u);
    u32 sample = (static_cast<u32>(m.r32s(context + kFlameState) >> 3) -
                  2u * m.r32(work + 4)) & 255u;
    for (u32 i = 0; i < 3; ++i) gte.ctc2(21 + i, static_cast<u32>(m.r16s(work + 28 + 2 * i)));
    s32 velocity = m.r16s(work + 78);
    u32 history = m.r32(work + 16);
    s32 distance = m.r16s(work + 76) - (m.r16(noise + 2 * sample) & 127u);
    const u32 buffer_y = m.r32(m.r32(g::kScene) + 4) ? 256u : 0u;
    for (u32 age = 0; age < 16 && history; ++age, history >>= 2) {
        distance = wrap::add(distance, velocity);
        velocity = wrap::sub(velocity, velocity >> 6);
        sample += 2;
        if (!(history & 1u)) continue;
        gte.mtc2(8, static_cast<u32>(distance));
        for (u32 i = 0; i < 3; ++i) gte.mtc2(9 + i, static_cast<u32>(m.r16s(work + 20 + 2 * i)));
        gte.op(0x980011u);
        const s32 spread_x = m.r16s(work + 72), spread_y = m.r16s(work + 74);
        s32 jitter_x = 0, jitter_y = 0;
        if (spread_x) {
            const s32 remainder = m.r16s(noise + 2 * (sample & 255u)) % spread_x;
            jitter_x = wrap::mul(remainder, wrap::add(distance, 512)) >> 11;
        }
        m.w16(work + 84, static_cast<u32>(jitter_x));
        if (spread_y) {
            const s32 remainder = m.r16s(noise + 2 * ((sample + 1) & 255u)) % spread_y;
            jitter_y = wrap::add(remainder, wrap::mul(remainder, distance) >> 12);
        }
        m.w16(work + 86, static_cast<u32>(jitter_y));
        m.w16(work + 84, m.r16(work + 84) + gte.mfc2(25));
        m.w16(work + 86, m.r16(work + 86) + gte.mfc2(26));
        gte.mtc2(0, m.r32(work + 84));
        gte.mtc2(1, gte.mfc2(27));
        m.w32(packet, ((packet + 40) & 0xFFFFFFu) | 0x09000000u);
        m.w32(packet + 4, 0x01000000u);
        m.w32(packet + 8, 0x80000000u);
        gte.op(0x180001u);
        m.w32(packet + 20, 0x00010010u);
        m.w32(packet + 24, 0x67000000u);
        m.w32(packet + 16, 0x012B0160u);
        m.w16(packet + 34, 0x4AD6);
        m.w32(packet + 36, 0x00050010u);
        m.w32(work + 92, gte.mfc2(14));
        if (std::abs(m.r16s(work + 92)) >= 153 || std::abs(m.r16s(work + 94)) >= 119) continue;
        const u32 screen_x = m.r16(work + 92);
        m.w16(work + 92, screen_x - 8);
        m.w16(packet + 12, (screen_x + 152) & 0x3FFu);
        m.w16(packet + 14, (m.r16(work + 94) + buffer_y + 120) & 0x1FFu);
        m.w16(work + 94, m.r16(work + 94) - 2);
        m.w8(packet + 32, 0);
        m.w8(packet + 33, (sample >> 1) & 15u);
        m.w32(packet + 28, m.r32(work + 92));
        packet += 40;
    }
    if (packet == start + 12) return start + 12;
    m.w32(packet, 0x02000000u);
    m.w32(packet + 4, 0xE6000000u);
    m.w32(packet + 8, 0xE1000220u);
    const s32 depth = std::max(-19, wrap::sub(m.r32s(work + 68), 2));
    const u32 bucket = m.r32(context + kSubjectOT) + 4u * static_cast<u32>(depth);
    flame_link(m, packet, m.r32(bucket));
    flame_link(m, bucket, m.r32(context + kCursor));
    m.w32(context + kCursor, packet + 12);
    return packet + 12;
}

u32 Game::draw_engine_flames(u32 context) {
    cover(0x8008C0A0u);
    Frame frame(*this, 56);
    const u32 definition = m.r32(m.r32(g::kExit) + 12);
    const u32 specification = m.r32(definition + 12);
    const u32 count = std::min(2u, specification & 255u);
    if (!count) return 1;
    const u32 work = context + kFlameWork, state = m.r32(context + kFlameState);
    m.w32(work, count);
    s32 thrust = m.r16s(state + 6);
    if (thrust <= 0) return (specification & 255u) < 3 ? 1u : 0u;
    thrust = std::min(4096, thrust);
    m.w32(work + 8, (specification & 0xFF00u) ? m.r32(state) : 0u);
    m.w32(work + 4, m.r32(m.r32(g::kScene) + 40));
    u32 colour = 0x00333333u;
    if (!(m.r32(work + 4) & 1u)) {
        gte.mtc2(8, static_cast<u32>(thrust));
        gte.mtc2(6, m.r32(definition + 16));
        const u32 target = m.r32(definition + 20);
        for (u32 i = 0; i < 3; ++i) gte.ctc2(21 + i, ((target >> (8 * i)) & 255u) << 4);
        gte.op(0x780010u);
        colour = gte.mfc2(22) & 0xFFFFFFu;
    }
    m.w32(work + 80, m.r32(m.r32(g::kScene)) & 0x20000000u);
    m.w32(work + 12, m.r32(m.r32(context + kFlameState) + 12));
    if (!m.r32(work + 80)) m.w32(work + 12, m.r32(work + 12) << 2);
    m.w32(work + 76, m.r32(definition + 120));
    for (u32 engine = 0; engine < count; ++engine) {
        const u32 aircraft = m.r32(m.r32(g::kExit) + 12), geometry = aircraft + 32 * engine;
        const u32 axis = aircraft + 88 + 16 * engine;
        const u32 origin_xy = m.r32(axis), origin_z = m.r32(axis + 4);
        const u32 target_xy = m.r32(axis + 8), target_z = m.r32(axis + 12);
        m.w32(work + 20, origin_xy);
        m.w32(work + 24, origin_z);
        m.w32(work + 28, target_xy);
        m.w32(work + 32, target_z);
        const u32 steering = m.r32(work + 8);
        for (u32 component = 0; component < 2; ++component) {
            const s32 angle = static_cast<s16>(steering >> (16 * component));
            if (!angle) continue;
            const u32 offset = component ? (angle < 0 ? 54u : 46u) : (angle < 0 ? 30u : 38u);
            const s32 adjustment = wrap::mul(m.r16s(geometry + offset), std::abs(angle)) >> 12;
            m.w16(work + 30, m.r16(work + 30) + static_cast<u32>(adjustment));
        }
        for (u32 i = 0; i < 3; ++i) {
            gte.mtc2(9 + i, static_cast<u32>(m.r16s(work + 20 + 2 * i)));
            gte.ctc2(21 + i, static_cast<u32>(m.r16s(work + 28 + 2 * i)));
        }
        gte.mtc2(8, static_cast<u32>(thrust));
        gte.op(0x980011u);
        const u32 vertex0_xy = m.r32(geometry + 24), vertex0_z = m.r32(geometry + 28);
        const u32 vertex1_xy = m.r32(geometry + 32), vertex1_z = m.r32(geometry + 36);
        m.w32(work + 44, vertex0_xy);
        m.w32(work + 48, vertex0_z);
        m.w32(work + 52, vertex1_xy);
        m.w32(work + 56, vertex1_z);
        for (u32 i = 0; i < 3; ++i) m.w16(work + 36 + 2 * i, gte.mfc2(25 + i));
        gte.mtc2(0, m.r32(work + 44));
        gte.mtc2(1, m.r32(work + 48));
        gte.mtc2(2, m.r32(work + 52));
        gte.mtc2(3, m.r32(work + 56));
        gte.mtc2(4, m.r32(geometry + 40));
        gte.mtc2(5, m.r32(geometry + 44));
        const u32 packet = m.r32(context + kCursor), second = packet + 36;
        m.w32(packet + 20, colour);
        gte.op(0x280030u);
        m.w32(packet, (second & 0xFFFFFFu) | 0x08000000u);
        m.w32(packet + 56, colour);
        m.w32(packet + 4, 0x3A030303u);
        m.w32(second, ((packet + 72) & 0xFFFFFFu) | 0x08000000u);
        m.w32(second + 4, 0x3A030303u);
        if (!m.r32(work + 80) && (rand() & 0xFFF) >= 2 * (3966 - thrust))
            m.w32(work + 12, m.r32(work + 12) | (1u << engine));
        m.w32(work + 16, (m.r32(work + 12) >> engine) & 0x55555555u);
        m.w32(second + 8, gte.mfc2(12));
        m.w32(second + 32, gte.mfc2(13));
        m.w32(packet + 8, gte.mfc2(14));
        m.w32(packet + 12, 0x00010101u);
        m.w32(second + 12, 0x00010101u);
        gte.op(0x158002Du);
        m.w16(work + 72, m.r16(work + 20) - m.r16(work + 52));
        m.w16(work + 74, m.r16(work + 22) - m.r16(work + 46));
        const s32 depth = static_cast<s32>(gte.mfc2(24));
        gte.mtc2(0, m.r32(work + 20));
        gte.mtc2(1, m.r32(work + 24));
        gte.mtc2(2, m.r32(work + 36));
        gte.mtc2(3, m.r32(work + 40));
        gte.mtc2(4, m.r32(geometry + 48));
        gte.mtc2(5, m.r32(geometry + 52));
        m.w32(packet + 28, 0x00030303u);
        m.w32(second + 28, 0x00030303u);
        gte.op(0x280030u);
        const s32 relative_depth = wrap::sub(wrap::add(wrap::sar(depth, m.r32(context + kDepthShift) + 10), 4), m.r16s(context + kSubjectDepth));
        const s32 bucket_index = std::clamp(relative_depth, -75, 76) >> 2;
        m.w32(work + 68, static_cast<u32>(bucket_index));
        m.w32(packet + 24, gte.mfc2(12));
        m.w32(second + 24, gte.mfc2(12));
        m.w32(packet + 16, gte.mfc2(13));
        m.w32(second + 16, gte.mfc2(13));
        m.w32(packet + 32, gte.mfc2(14));
        const u32 bucket = m.r32(context + kSubjectOT) + 4u * static_cast<u32>(bucket_index);
        flame_link(m, second, m.r32(bucket));
        flame_link(m, bucket, m.r32(context + kCursor));
        m.w32(context + kCursor, packet + 72);
        if (m.r32(work + 16)) draw_exhaust_particles(context, work);
        m.w32(work + 4, m.r32(work + 4) + 32);
    }
    const u32 paused = m.r32(work + 80);
    if (paused) return paused;
    const u32 history = m.r32(work + 12);
    m.w32(m.r32(context + kFlameState) + 12, history);
    return history;
}
}
