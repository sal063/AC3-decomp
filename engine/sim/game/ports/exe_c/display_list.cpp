#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplayEntryBytes = 16;
constexpr u32 kVisibleFlags = 0x14000u;
constexpr u32 kGroundSlots = 0x800BCE4Cu;
u32 special_model(s32 type) { return type == 43 ? 3u : type == 35 ? 4u : type == 36 ? 5u : 0u; }
void display_entity(Memory& m, u32 out, u32 entity) {
    m.w8(out, m.r8(entity + 4));
    m.w8(out + 1, m.r8(entity + 5));
    const u32 index = m.r16(entity + 6);
    m.w32(out + 8, entity + 8);
    m.w16(out + 2, index);
    const u32 flags = m.r32(entity);
    m.w8(out + 6, 0);
    m.w8(out + 7, 0);
    m.w32(out + 12, flags);
    const u32 special = special_model(m.r16s(entity + 72));
    if (special) m.w8(out + 6, special);
}
}

u32 Game::display_aircraft_list(u32 out) {
    cover(0x80067144u);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    const u32 hidden = m.r32(view + 196), selected = m.r32(view + 200);
    for (u32 i = 0; i < 32; ++i) {
        const u32 entity = slot(i);
        if ((m.r32(entity) & kVisibleFlags) != kVisibleFlags || hidden == i) continue;
        u32 kind = 1;
        if (m.r8(entity + 5) & 1u) {
            kind = selected == i && (m.r32(m.r32(m.r32(g::kWorld) + 40) + 80) & 6u) ? 2u : 4u;
        }
        m.w8(out + 4, kind);
        display_entity(m, out, entity);
        m.w8(out + 7, m.r16s(entity + 72) == 25 ? 2u : 1u);
        m.w8(out + 5, selected == i);
        out += kDisplayEntryBytes;
    }
    const u32 live = m.r32(m.r32(g::kWeapons) + 5448);
    if (live) {
        for (u32 i = 0; i < 32; ++i) {
            if (!(live & (1u << i))) continue;
            const u32 weapon = m.r32(g::kWeapons) + 200 + 164 * i;
            m.w8(out + 4, 1);
            const u32 model = m.r8(weapon + 52);
            if (!model) continue;
            m.w8(out, model + 23);
            m.w8(out + 1, 0);
            m.w32(out + 8, weapon + 28);
            m.w32(out + 12, 0);
            m.w8(out + 5, 0);
            m.w8(out + 6, 0);
            m.w8(out + 7, 0);
            if (m.r8(weapon + 53) & 0x10u) m.w8(out + 6, 1);
            if (m.r8(weapon + 53) & 0x20u) m.w8(out + 6, 2);
            out += kDisplayEntryBytes;
        }
    }
    m.w8(out, 0xFF);
    return out;
}

u32 Game::display_ground_list(u32 out) {
    cover(0x80067380u);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    const u32 hidden = m.r32(view + 196) - 32, selected = m.r32(view + 200);
    for (u32 i = 0; i < 32; ++i) {
        const u32 entity = m.r32(m.r32(kGroundSlots) + 4 * i);
        if ((m.r32(entity) & kVisibleFlags) != kVisibleFlags || hidden == i) continue;
        m.w8(out + 4, (m.r8(entity + 5) & 2u) ? 8u : 16u);
        display_entity(m, out, entity);
        m.w8(out + 5, selected == i);
        out += kDisplayEntryBytes;
    }
    m.w8(out, 0xFF);
    return out;
}
}
