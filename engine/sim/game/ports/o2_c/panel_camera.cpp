#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_panel_scene_alloc() {
    cover(0x800FC148u);
    Frame frame(*this, 32);
    constexpr u32 kMenu = 0x80112DC4u, kSlots = 0x800BCE5Cu, kEffects = 0x800BCE4Cu;
    u32 menu = m.r32(kMenu);
    m.w16(menu + 0xD8, 1008); m.w16(menu + 0xDA, 248);
    m.w16(menu + 0xDC, 16); m.w16(menu + 0xDE, 8);
    m.w32(m.r32(kMenu) + 0xD0, mem_alloc(256));
    menu = m.r32(kMenu);
    const u32 paletteBytes = static_cast<u32>(m.r16s(menu + 0xDE)) << 5;
    m.w32(m.r32(kMenu) + 0xD4, mem_alloc(paletteBytes));
    for (u32 bank = 0; bank < 2; ++bank) {
        m.w32(m.r32(g::kWorld) + 0x18 + 4 * bank, mem_alloc(256));
        m.w32(m.r32(g::kWorld) + 0x20 + 4 * bank, mem_alloc(0x4800));
    }
    const u32 world = m.r32(g::kWorld), bank = m.r32(m.r32(g::kScene) + 4);
    m.w32(world + 16, m.r32(world + 0x18 + 4 * bank));
    m.w32(world + 20, m.r32(world + 0x20 + 4 * bank));
    const u32 tables = mem_alloc(256);
    m.w32(kSlots, tables);
    m.w32(kEffects, tables + 128);
    for (u32 slot = 0; slot < 32; ++slot) {
        const u32 entity = mem_alloc(slot < 2 ? 88 : 92);
        m.w32(m.r32(kSlots) + 4 * slot, entity);
        m.w32(entity, 0);
    }
    for (u32 slot = 0; slot < 32; ++slot) {
        const u32 entity = m.r32(m.r32(kSlots) + 4 * slot);
        m.w8(entity + 5, 0);
        m.w16(entity + 6, 0);
    }
    for (u32 effect = 0; effect < 32; ++effect) {
        const u32 record = mem_alloc(148);
        m.w32(m.r32(kEffects) + 4 * effect, record);
        m.w32(record, 0);
    }
    m.w32(m.r32(g::kWorld) + 0x250, mem_alloc(4));
    for (u32 record = 0; record < 64; ++record) {
        const u32 data = mem_alloc(28);
        m.w32(m.r32(m.r32(g::kWorld) + 0x250) + 4 * record, data);
    }
    return 0;
}

u32 Game::o2_panel_camera_init() {
    cover(0x800FC488u);
    Frame frame(*this, 40);
    const u32 angles = frame.at(24);
    for (u32 byte = 0; byte < 8; ++byte) m.w8(angles + byte, 0);
    u32 world = m.r32(g::kWorld);
    m.w32(world + 0x28, world + 0xB8);
    m.w32(frame.at(16), angles);
    view_init(m.r32(world + 0x28), 200, 8000, 28000, angles);
    o2_panel_lighting();
    world = m.r32(g::kWorld);
    const u32 camera = m.r32(world + 0x28);
    m.w32(camera + 4, 0); m.w32(camera + 8, 0xFFE00000u); m.w32(camera + 12, 0);
    m.w16(camera + 20, 0xF000); m.w16(camera + 22, 0); m.w16(camera + 24, 0);
    for (u32 offset : {0x48u, 0x44u, 0x40u, 0x68u, 0x64u, 0x60u}) m.w32(world + offset, 0);
    rot_matrix_yxz(camera + 20, world + 0x2C);
    world = m.r32(g::kWorld);
    transpose_matrix(world + 0x2C, world + 0x4C);
    o2_aircraft_light_half(m.r32(g::kWorld) + 0x4C);
    set_geom_offset(0, 0);
    set_geom_screen(m.r16s(m.r32(m.r32(g::kWorld) + 0x28) + 0x24));
    return 0;
}

u32 Game::o2_panel_lighting() {
    cover(0x800FC37Cu);
    Frame frame(*this, 88);
    const u32 world = m.r32(g::kWorld);
    constexpr u32 kLightMatrix = 0x6C, kColourMatrix = 0x8C, kBackColour = 0xAC;
    const u32 vector = frame.at(16), angles = frame.at(24), rotation = frame.at(32);
    m.w16(vector, 0);
    m.w16(vector + 2, 0);
    m.w16(vector + 4, 4096);
    for (u32 row = 0; row < 3; ++row)
        for (u32 column = 0; column < 3; ++column)
            m.w16(world + kColourMatrix + 6 * row + 2 * column, column == 0 ? 2048 : 0);
    for (u32 component = 0; component < 9; ++component)
        m.w16(world + kLightMatrix + 2 * component, 0);
    for (u32 channel = 0; channel < 3; ++channel) m.w32(world + kBackColour + 4 * channel, 84);
    for (u32 component = 0; component < 3; ++component) m.w16(angles + 2 * component, 0);
    rot_matrix_yxz_gte(angles, rotation);
    apply_matrix_sv(rotation, vector, world + kLightMatrix);
    for (u32 component = 0; component < 3; ++component) m.w16(angles + 2 * component, 0);
    rot_matrix_yxz_gte(angles, rotation);
    return apply_matrix_sv(rotation, vector, world + kLightMatrix + 6);
}
}
