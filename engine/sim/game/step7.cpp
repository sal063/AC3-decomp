#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kWorld = 0x800BCE54u;
constexpr u32 kSlots = 0x800BCE5Cu;
constexpr u32 kWeapons = 0x800BD080u;
constexpr u32 kTrails = 0x800BD0B0u;

constexpr u32 kRingA = 2312u, kRingB = 1288u;
constexpr u32 kPoolB = 74020u;
constexpr u32 kHandlesA = 73988u, kHandlesB = 115240u;
constexpr u32 kScratch = 0x1F800000u;

s32 lowest_free(u32 mask) {
    if (!(mask & 1u)) return 0;
    s32 k = 1;
    while (mask & (1u << k)) ++k;
    return k;
}
}

s32 Game::trail_a_push(s32 ring, u32 sample) {
    cover(0x8007BC88u);
    const u32 pool = m.r32(kTrails);
    const u32 mask = m.r32(pool);
    u32 r;
    if (ring == -1) {
        if (mask == 0xFFFFFFFFu) return -1;
        ring = lowest_free(mask);
        r = pool + 4 + kRingA * static_cast<u32>(ring);
        m.w32(r + 4, 0);
        m.w32(r, 0);
        m.w32(pool, mask | wrap::shl(1u, static_cast<u32>(ring)));
    } else {
        if (~mask & wrap::shl(1u, static_cast<u32>(ring))) return -1;
        r = pool + 4 + kRingA * static_cast<u32>(ring);
    }
    const s32 head = m.r32s(r);
    if (head - m.r32s(r + 4) >= 32) return ring;
    const u32 i = static_cast<u32>(head) & 0x1Fu;
    m.w32(r + 8 + 4 * i, 0);
    m.w32(r + 136 + 4 * i, 90);
    m.w32(r, static_cast<u32>(head + 1));
    const u32 e = r + 264 + 64 * i;
    for (u32 w : {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u}) m.w32(e + 4 * w, m.r32(sample + 4 * w));
    return ring;
}

s32 Game::trail_a_replace(s32 ring, u32 sample) {
    cover(0x8007BE3Cu);
    const u32 pool = m.r32(kTrails);
    if (ring < 0 || (~m.r32(pool) & wrap::shl(1u, static_cast<u32>(ring)))) return -1;
    const u32 r = pool + 4 + kRingA * static_cast<u32>(ring);
    const u32 e = r + 264 + 64 * ((static_cast<u32>(m.r8(r)) - 1u) & 0x1Fu);
    for (u32 w : {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u, 12u, 13u, 14u}) m.w32(e + 4 * w, m.r32(sample + 4 * w));
    return ring;
}

s32 Game::trail_b_push(s32 ring, u32 sample) {
    cover(0x8007CC6Cu);
    const u32 pool = m.r32(kTrails);
    const u32 mask = m.r32(pool + kPoolB);
    u32 r;
    if (ring == -1) {
        if (mask == 0xFFFFFFFFu) return -1;
        ring = lowest_free(mask);
        r = m.r32(kTrails) + kPoolB + 4 + kRingB * static_cast<u32>(ring);
        m.w32(r + 4, 0);
        m.w32(r, 0);
        m.w32(m.r32(kTrails) + kPoolB, mask | wrap::shl(1u, static_cast<u32>(ring)));
    } else {
        if (~m.r32(pool + kPoolB) & wrap::shl(1u, static_cast<u32>(ring))) return -1;
        r = pool + kPoolB + 4 + kRingB * static_cast<u32>(ring);
    }
    const s32 head = m.r32s(r);
    if (head - m.r32s(r + 4) >= 32) return ring;
    const u32 i = static_cast<u32>(head) & 0x1Fu;
    m.w32(r + 8 + 4 * i, 0);
    m.w32(r + 136 + 4 * i, 63);
    m.w32(r, static_cast<u32>(head + 1));
    const u32 e = r + 264 + 32 * i;
    for (u32 w : {0u, 1u, 2u, 4u, 5u, 6u}) m.w32(e + 4 * w, m.r32(sample + 4 * w));
    return ring;
}

s32 Game::trail_b_replace(s32 ring, u32 sample) {
    cover(0x8007CDF8u);
    const u32 pool = m.r32(kTrails);
    if (ring < 0 || (~m.r32(pool + kPoolB) & wrap::shl(1u, static_cast<u32>(ring)))) return -1;
    const u32 r = pool + kPoolB + 4 + kRingB * static_cast<u32>(ring);
    const u32 e = r + 264 + 32 * ((static_cast<u32>(m.r8(r)) - 1u) & 0x1Fu);
    for (u32 w : {0u, 1u, 2u, 4u, 5u, 6u}) m.w32(e + 4 * w, m.r32(sample + 4 * w));
    return ring;
}

void Game::trails_sample_weapons() {
    cover(0x8007C9E8u);
    const u32 base = m.r32(kWeapons);
    const u32 live = m.r32(base + 5448);
    for (u32 i = 0; i < 32; ++i) {
        const u32 obj = base + 200 + 164 * i;
        const u32 handle = m.r32(kTrails) + i + kHandlesA;
        if ((~live & (1u << i)) || !(m.r8(obj + 53) & 8u)) {
            m.w8(handle, 0xFF);
            continue;
        }
        rot_matrix_yxz(obj + 44, kScratch);
        load_scratch_rotation();
        const s32 px = m.r32s(obj + 28), py = m.r32s(obj + 32), pz = m.r32s(obj + 36);
        for (u32 k = 0; k < 4; ++k) {
            gte.mtc2(0, m.r32(0x800B4124u + 8 * k));
            gte.op(0x486012);
            m.w32(kScratch + 0x20 + 16 * k, static_cast<u32>((static_cast<s32>(gte.mfc2(25)) << 3) + px));
            m.w32(kScratch + 0x24 + 16 * k, static_cast<u32>((static_cast<s32>(gte.mfc2(26)) << 3) + py));
            m.w32(kScratch + 0x28 + 16 * k, static_cast<u32>((static_cast<s32>(gte.mfc2(27)) << 3) + pz));
        }
        const s32 ring = m.r8s(m.r32(kTrails) + i + kHandlesA);
        if ((m.r32(m.r32(kWorld) + 12) & 3u) != (i & 3u) && ring >= 0) {
            trail_a_replace(ring, kScratch + 0x20);
            continue;
        }
        const s32 r = trail_a_push(ring, kScratch + 0x20);
        m.w8(m.r32(kTrails) + i + kHandlesA, static_cast<u32>(r));
    }
}

void Game::trails_sample_aircraft() {
    cover(0x8007D7E0u);
    for (u32 i = 0; i < 32; ++i) {
        const u32 e = m.r32(m.r32(kSlots) + 4 * i);
        const u32 handle = m.r32(kTrails) + i + kHandlesB;
        if ((m.r32(e) & 0x14000u) != 0x14000u || !(m.r8(e + 5) & 8u)) {
            m.w8(handle, 0xFF);
            continue;
        }
        const s32 type = m.r16s(e + 72);
        if (!(2 * type < 62) || type < 0) continue;
        u32 offsets = 0x800B4144u + 16u * static_cast<u32>(type);
        rot_matrix_yxz(e + 24, kScratch);
        load_scratch_rotation();
        const s32 px = m.r32s(e + 8), py = m.r32s(e + 12), pz = m.r32s(e + 16);
        for (u32 k = 0; k < 2; ++k) {
            gte.mtc2(0, m.r32(offsets));
            gte.mtc2(1, m.r32(offsets + 4));
            gte.op(0x486012);
            m.w32(kScratch + 0x40, gte.mfc2(25));
            m.w32(kScratch + 0x44, gte.mfc2(26));
            m.w32(kScratch + 0x48, gte.mfc2(27));
            offsets += 8;
            m.w32(kScratch + 0x20 + 16 * k, static_cast<u32>((m.r32s(kScratch + 0x40) << 3) + px));
            m.w32(kScratch + 0x24 + 16 * k, static_cast<u32>((m.r32s(kScratch + 0x44) << 3) + py));
            m.w32(kScratch + 0x28 + 16 * k, static_cast<u32>((m.r32s(kScratch + 0x48) << 3) + pz));
        }
        if ((m.r32(m.r32(kWorld) + 12) & 1u) == (i & 1u)) {
            const s32 r = trail_b_push(m.r8s(m.r32(kTrails) + i + kHandlesB), kScratch + 0x20);
            m.w8(m.r32(kTrails) + i + kHandlesB, static_cast<u32>(r));
        } else {
            const s32 ring = m.r8s(m.r32(kTrails) + i + kHandlesB);
            if (ring >= 0) trail_b_replace(ring, kScratch + 0x20);
        }
    }
}

void Game::trails_age(u32 mask_at, u32 ring_bytes) {
    u32 mask = m.r32(mask_at);
    for (u32 k = 0; k < 32; ++k) {
        const u32 head = mask_at + 4 + ring_bytes * k;
        const u32 tail = head + 4;
        if (mask & (1u << k)) {
            for (s32 n = m.r32s(tail); n < m.r32s(head); ++n) {
                const u32 i = static_cast<u32>(n) & 0x1Fu;
                const s32 life = m.r32s(head + 136 + 4 * i) - 1;
                m.w32(head + 8 + 4 * i, m.r32(head + 8 + 4 * i) + 1);
                m.w32(head + 136 + 4 * i, static_cast<u32>(life));
            }
            if (m.r32s(head + 136 + 4 * (m.r32(tail) & 0x1Fu)) < 0) m.w32(tail, m.r32(tail) + 1);
            if (m.r32s(tail) >= m.r32s(head)) mask &= ~(1u << k);
        }
    }
    m.w32(mask_at, mask);
}

u32 Game::sim_step7() {
    cover(0x8007DAC8u);
    Frame frame(*this, 24);
    trails_sample_weapons();
    trails_age_weapons();
    trails_sample_aircraft();
    return trails_age_aircraft();
}
}
