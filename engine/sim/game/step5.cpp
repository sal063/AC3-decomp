#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::slot(u32 i) const { return m.r32(m.r32(g::kSlots) + 4 * i); }

void Game::entity_links_update() {
    cover(0x8004BED4u);
    for (u32 i = 0; i < m.r8(g::kLinkCount); ++i) {
        const u32 flags_at = m.r32(g::kLinkFlags) + 4 * i;
        const u32 flags = m.r32(flags_at);
        if (!(flags & 1u)) continue;
        const u32 link = m.r32(g::kLinks) + 20 * i;
        const u32 parent_slot = m.r8(link + 2);
        const u32 parent = slot(parent_slot);
        const u32 child = slot(m.r8(link + 3));
        switch (m.r16(link)) {
        case 3:
            if (parent_slot == 0) {
                m.w8(child + 4, 0);
                m.w8(child + 5, m.r8(child + 5) | 1u);
            }
            m.w32(child + 8, m.r32(parent + 8));
            m.w32(child + 12, 0xFFFBE000u - m.r32(parent + 12));
            m.w32(child + 16, m.r32(parent + 16));
            m.w16(child + 24, 0u - m.r16(parent + 24));
            m.w16(child + 26, m.r16(parent + 26));
            m.w16(child + 28, 0x2000u - m.r16(parent + 28));
            if (~m.r32(parent) & 0x10000u) m.w32(child, m.r32(child) & 0xFFFEFFFFu);
            break;
        case 4:
            from_local(parent + 8, link + 4, child + 8);
            m.w16(child + 24, m.r16(parent + 24));
            m.w16(child + 26, m.r16(parent + 26));
            m.w16(child + 28, m.r16(parent + 28) + 0x1FFFu);
            for (u32 k = 0; k < 16; k += 4) m.w32(child + 56 + k, m.r32(parent + 56 + k));
            m.w32(child + 84, m.r32(child + 84) & 0x7FFFFFFFu);
            break;
        case 5: {
            const u32 d = static_cast<u32>(m.r16s(child + 26) - m.r16s(parent + 26)) & 0x3FFFu;
            if (d - 0x1C01u < 0x7FFu) m.w32(flags_at, flags ^ 0x10u);
            u32 yaw = static_cast<u32>(m.r16s(parent + 26) + 0x1000) & 0x3FFFu;
            if (m.r32(flags_at) & 0x10u) yaw = (yaw + 0x2000u) & 0x3FFFu;
            auto t = [&](u32 k) { return static_cast<s32>(m.r16s(g::kSinCos + 2u * k)); };
            const u32 pitch = static_cast<u32>(m.r16s(parent + 24)) & 0x3FFFu;
            const s32 cp = t(pitch + 0x1000);
            m.w32(child + 8, static_cast<u32>(m.r32s(parent + 8) + ((t(yaw) * cp) >> 9)));
            m.w32(child + 12, static_cast<u32>(m.r32s(parent + 12) + (t(pitch) << 5)));
            m.w32(child + 16, static_cast<u32>(m.r32s(parent + 16) + ((t(yaw + 0x1000) * cp) >> 9)));
            m.w32(child + 24, m.r32(parent + 24));
            m.w32(child + 28, m.r32(parent + 28));
            m.w8(child + 5, (m.r8(parent + 5) & 8u) ? (m.r8(child + 5) | 8u) : (m.r8(child + 5) & 0xF7u));
            if (m.r32(m.r32(g::kPlayer) + 16) & 0x4000u) {
                u32 target = 0xFFFFFFFFu;
                if (m.r8(0x800C6D72u) != 0xFF) target = m.r32(slot(m.r8(0x800C6D6Eu)));
                spawn_weapon(m.r32(parent), child + 8, parent + 56, static_cast<s32>(target), 0x102);
            }
            break;
        }
        case 6:
            from_local(parent + 8, link + 4, child + 8);
            m.w32(child + 24, m.r32(parent + 24));
            m.w32(child + 28, m.r32(parent + 28));
            m.w16(child + 28, (m.r16(m.r32(g::kWorld) + 12) & 0x3Fu) << 8);
            break;
        default:
            from_local(parent + 8, link + 4, child + 8);
            m.w32(child + 24, m.r32(parent + 24));
            m.w32(child + 28, m.r32(parent + 28));
            break;
        }
    }
}

void Game::relations_to_player() {
    cover(0x80045278u);
    const u32 player = slot(0);
    rot_matrix_yxz_t(player + 24, g::kPlayerInvRot);
    const u32 tick = m.r32(m.r32(g::kWorld) + 12);
    for (u32 i = 2; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 rel = m.r32(g::kRelation + 4 * i);
        const u32 ef = m.r32(e) & 0xFFFFDFFFu;
        m.w32(e, ef);
        if (!(ef & 0x4000u) || (m.r32(g::kSlotFlags + 4 * i) & 0x400000u)) continue;
        const s32 prev = m.r32s(rel + 4);
        m.w32(rel, 0);
        if (prev == 0x100000) {
            if (i != (tick & 0x3Fu)) continue;
        } else if (prev > 34000 && (i & 0xFu) != (tick & 0xFu)) {
            continue;
        }
        const s32 d = distance(e + 8, player + 8);
        m.w32(rel + 4, static_cast<u32>(d));
        if (i < 32 ? d <= 33999 : d < 12288) m.w32(rel, m.r32(rel) | 0x80u);
        if (m.r32s(rel + 4) > 0x7FFF) continue;
        m.w32(rel, m.r32(rel) | 1u);
        m.w32(e, m.r32(e) | 0x2000u);
        m.w32(rel, m.r32(rel) | 0x1000000u);
        to_frame(player + 8, g::kPlayerInvRot, e + 8, rel + 8);
        const s32 ang = off_axis_angle(rel + 8);
        m.w16(rel + 24, static_cast<u32>(ang));
        const s32 a = m.r16s(rel + 24), dist = m.r32s(rel + 4);
        if (static_cast<s16>(ang) < 1536) m.w32(rel, m.r32(rel) | 2u);
        if (dist < 12288 && a < 1536) m.w32(rel, m.r32(rel) | 0x40u);
        if (dist < 0x2000 && a < 1536) m.w32(rel, m.r32(rel) | 4u);
        if (dist < m.r32s(g::kNearDist)) {
            const u32 f = m.r32(rel);
            m.w32(rel, f | 0x100u);
            if (a < m.r32s(g::kNearAngle)) m.w32(rel, f | 0x108u);
            if (a < 1536) m.w32(rel, m.r32(rel) | 0x10u);
        }
        if (dist < m.r32s(g::kFarDist) && (m.r32(rel) & 2u)) m.w32(rel, m.r32(rel) | 0x20u);
    }
}

void Game::relations_follow_links() {
    cover(0x8004C5B8u);
    for (u32 i = 0; i < m.r8(g::kLinkCount); ++i) {
        if (!(m.r32(m.r32(g::kLinkFlags) + 4 * i) & 1u)) continue;
        const u32 link = m.r32(g::kLinks) + 20 * i;
        const u32 child = m.r8(link + 3), parent = m.r8(link + 2);
        const u32 dst = m.r32(g::kRelation + 4 * child);
        if (parent) {
            const u32 src = m.r32(g::kRelation + 4 * parent);
            for (u32 k = 0; k < 40; k += 4) m.w32(dst + k, m.r32(src + k));
        } else {
            m.w32(dst + 4, 1);
        }
        const u32 pf = m.r32(slot(parent));
        if (pf & 0x4000u) {
            const u32 c = slot(child);
            if ((m.r32(c) & 0x4000u) && (pf & 0x2000u)) m.w32(c, m.r32(c) | 0x2000u);
        }
    }
}

void Game::ai_targets_update() {
    cover(0x800455B8u);
    for (u32 i = 2; i < 32; ++i) {
        const u32 e = slot(i);
        const u32 ai = m.r32(g::kAiInfo) + 184 * i;
        const u32 sf = g::kSlotFlags + 4 * i;
        if ((m.r32(e) & 0xC000u) != 0xC000u) continue;
        if (m.r32(sf) & 0x20u) {
            m.w32(ai + 136, static_cast<u32>(distance(e + 8, ai + 104)));
            to_local(e + 8, ai + 104, ai + 140);
            m.w16(ai + 156, static_cast<u32>(off_axis_angle(ai + 140)));
            m.w32(sf, m.r32(sf) & ~0x20u);
        }
        if (m.r8(ai + 96) == 0xFF) {
            m.w32(ai + 160, m.r32(ai + 136));
            for (u32 k = 0; k < 16; k += 4) m.w32(ai + 164 + k, m.r32(ai + 140 + k));
            m.w16(ai + 182, 0);
            m.w16(ai + 180, m.r16(ai + 156));
        } else {
            const u32 target = slot(m.r8(ai + 96)) + 8;
            m.w32(ai + 160, static_cast<u32>(distance(e + 8, target)));
            to_local(e + 8, target, ai + 164);
            m.w16(ai + 180, static_cast<u32>(off_axis_angle(ai + 164)));
            if (m.r32(ai + 160) < 0x2000u) m.w16(ai + 182, m.r16(ai + 182) + 16u);
        }
    }
}

void Game::relations_timers() {
    cover(0x80045768u);
    for (u32 i = 2; i < 64; ++i) {
        const u32 rec = m.r32(g::kSlotRecords) + 64 * i;
        const u32 rel = m.r32(g::kRelation + 4 * i);
        if ((m.r16(slot(i)) & 0xC000u) != 0xC000u || !rel) continue;
        const u32 f = m.r32(rel);
        if (f & 0x40u) {
            if (m.r16(rel + 30) != 0x7FFF) m.w16(rel + 30, m.r16(rel + 30) + 1u);
        } else {
            m.w16(rel + 30, 0);
        }
        if (m.r32(rel) & 8u) {
            if (m.r16(rel + 28) != 0x7FFF) m.w16(rel + 28, m.r16(rel + 28) + 1u);
        } else {
            m.w16(rel + 28, 0);
        }
        if (i < 32) {
            if (m.r32(rel) & 4u) {
                const u32 limit_at = m.r32(rec + 24);
                const s16 n = static_cast<s16>(m.r16(rel + 26) + 1u);
                m.w16(rel + 26, static_cast<u32>(n));
                if (n >= m.r16s(limit_at + 12)) m.w32(rel, m.r32(rel) | 0x2000000u);
            } else {
                m.w16(rel + 26, 0);
            }
        }
    }
}

void Game::relations_update() {
    cover(0x80045240u);
    relations_to_player();
    relations_follow_links();
    ai_targets_update();
    relations_timers();
}

void Game::track_record() {
    cover(0x80048658u);
    const u32 scene = m.r32(g::kScene);
    const u32 pos = slot(0) + 8;
    const s32 tick = m.r32s(m.r32(g::kWorld) + 12);
    if (tick == 120 * static_cast<s32>(wrap::div(tick, 120).lo)) {
        const s32 n = m.r16s(scene + 960);
        if (n < 450) {
            auto cell = [](s32 v) {
                const s32 c = ((v >> 12) + 0x40000) >> 11;
                return c < 0 ? 0u : c >= 256 ? 0xFFu : static_cast<u32>(c);
            };
            m.w8(scene + 964 + 2 * static_cast<u32>(n), cell(m.r32s(pos)));
            m.w8(scene + 965 + 2 * static_cast<u32>(m.r16s(scene + 960)), cell(m.r32s(pos + 8)));
            m.w16(scene + 960, m.r16(scene + 960) + 1u);
        }
    }
    m.w16(scene + 954, m.r16(scene + 954) + 1u);
}

void Game::sim_step5() {
    cover(0x8004658Cu);
    entity_links_update();
    relations_update();
    const u32 world = m.r32(g::kWorld);
    if (m.r32(world + 4) & 4u) {
        if (m.r16(m.r32(g::kScene) + 908) != 9 || m.r32s(world + 8) <= 0) track_record();
    }
}
}
