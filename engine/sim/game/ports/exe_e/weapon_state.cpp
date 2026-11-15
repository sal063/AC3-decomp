#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::muzzle_geometry(u32 record) {
    cover(0x800900A4u);
    const u32 aircraft = m.r32(m.r32(g::kExit) + 12);
    const u32 origin = m.r32(aircraft + 124), extent = m.r32(aircraft + 128);
    const u32 direction = m.r32(aircraft + 132), spread = m.r32(aircraft + 136);
    m.w32(record + 8, origin);
    m.w32(record + 12, extent);
    m.w32(record + 16, direction);
    m.w32(record + 20, spread);
    m.w32(record + 24, m.r32(aircraft + 140));
    return aircraft;
}

u32 Game::muzzle_start(u32 kind_and_duration) {
    cover(0x80090110u);
    Frame frame(*this, 24);
    const u32 effects = m.r32(0x800BCF90u);
    m.w32(effects + 14668, kind_and_duration);
    m.w32(effects + 14664, 1);
    return record_gun_flash(kind_and_duration & 15u, static_cast<u32>(static_cast<s32>(kind_and_duration) >> 16));
}

void Game::muzzle_advance(u32 record) {
    cover(0x80090148u);
    Frame frame(*this, 24);
    const s32 age = m.r32s(record);
    if (age <= 0) return;
    const u32 specification = m.r32(record + 4);
    m.w32(record, age < (static_cast<s32>(specification) >> 16) ?
                      static_cast<u32>(age) + 1u : 0u);
    const u32 world = m.r32(g::kWorld);
    if (!(m.r32(world) & 2u) || (specification & 0xFFFFu) != 5) return;
    u32 strength = (static_cast<u32>(age) - 1u) << 6;
    if (!(m.r32(m.r32(g::kScene) + 296) & 0x800u)) {
        strength += 256;
        if (static_cast<s32>(strength) >= 1025) strength = 1024;
    }
    const u32 player = slot(0);
    gun_ray(player + 8, player + 24, strength, m.r32(m.r32(world + 40) + 80) == 1 ? 1u : 0u);
}

void Game::muzzle_light(s32 age, u32 kind, u32 work) {
    cover(0x80090210u);
    Frame frame(*this, 40);
    if (age < 0 || kind >= 6) return;
    constexpr u32 kSavedView = 80, kAircraftRotation = 112;
    for (u32 i = 0; i < 8; ++i) m.w32(work + kSavedView + 4 * i, gte.cfc2(i));
    rot_matrix_yxz(slot(0) + 24, work + kAircraftRotation);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + kAircraftRotation + 4 * i));
    gte.mtc2(0, 0xFF9C0000u);
    gte.mtc2(1, 3896);
    gte.op(0x486012u);
    const u32 world = m.r32(g::kWorld);
    for (u32 i = 0; i < 3; ++i) m.w16(world + 120 + 2 * i, gte.mfc2(25 + i));
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(work + kSavedView + 4 * i));
    static constexpr u16 kLight[6][4] = {
        {2560, 1536, 768, 11}, {2560, 1536, 768, 11},
        {3584, 2048, 512, 11}, {2048, 1536, 2560, 10},
        {512, 1024, 3584, 10}, {512, 3072, 512, 8},
    };
    for (u32 i = 0; i < 3; ++i) gte.mtc2(9 + i, kLight[kind][i]);
    gte.mtc2(8, static_cast<u32>(age) << kLight[kind][3]);
    for (u32 i = 21; i < 24; ++i) gte.ctc2(i, 0);
    const u32 red_green = gte.cfc2(17), green_blue = gte.cfc2(18);
    gte.op(0x980011u);
    gte.ctc2(17, (red_green & 0xFFFF0000u) | (gte.mfc2(25) & 0xFFFFu));
    gte.ctc2(18, (green_blue & 0xFFFFu) | (gte.mfc2(26) << 16));
    gte.ctc2(20, gte.mfc2(27));
}
}
