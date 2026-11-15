#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSelection = 0x80112E30u, kModelTable = 0x80112C08u;
constexpr u32 kZoom = 0xFC, kAngles = 0x100, kCamera = 0x28;
s32 slideOffset(s32 fade) {
    const s32 remaining = wrap::sub(25, fade);
    return wrap::mul(wrap::mul(-325, remaining), remaining) / 625;
}
u32 selectedModel(Memory& m) { return kModelTable + 8 * m.r8(m.r32(kSelection) + 0x4000); }
u32 linkPacket(Memory& m, u32 packet, u32 ot) {
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    const u32 head = (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu);
    m.w32(ot, head);
    return head;
}
}

u32 Game::o2_selector_model_input() {
    cover(0x801084D0u);
    Frame frame(*this, 32);
    const u32 rates = frame.at(16);
    for (u32 byte = 0; byte < 8; ++byte) m.w8(rates + byte, 0);
    u32 buttons = m.r32(m.r32(g::kScene) + 0x128), menu = m.r32(kMenu);
    if ((buttons & 0x200u) && m.r32s(menu + kZoom) > -0x200000)
        m.w32(menu + kZoom, m.r32(menu + kZoom) - 4096u);
    buttons = m.r32(m.r32(g::kScene) + 0x128);
    menu = m.r32(kMenu);
    if ((buttons & 0x100u) && m.r32s(menu + kZoom) < -0x80000)
        m.w32(menu + kZoom, m.r32(menu + kZoom) + 4096u);
    buttons = m.r32(m.r32(g::kScene) + 0x128);
    if (buttons & 8u) m.w16(rates + 2, 128);
    if (buttons & 4u) m.w16(rates + 2, static_cast<u32>(-128));
    if (buttons & 1u) m.w16(rates, static_cast<u32>(-128));
    if (buttons & 2u) m.w16(rates, 128);
    attitude_integrate(m.r32(kMenu) + kAngles, rates);
    return 0;
}

u32 Game::o2_selector_model_view(u32 mode) {
    cover(0x801085F8u);
    Frame frame(*this, 40);
    const s32 fade = o2_fade_value(2);
    const u32 model = selectedModel(m);
    const u32 variantSlot = m.r16s(model + 4) == 0 ? 2u : 0u;
    if (mode > 2) return 2;
    if (mode == 1) {
        o2_selector_model_input();
        set_geom_offset(160, 240);
        const u32 menu = m.r32(kMenu);
        for (u32 component = 0; component < 3; ++component) {
            const u32 entity = m.r32(m.r32(g::kSlots) + 4 * variantSlot);
            m.w16(entity + 24 + 2 * component, m.r16(menu + kAngles + 2 * component));
        }
        const u32 world = m.r32(g::kWorld);
        m.w32(m.r32(world + kCamera) + 4, 0);
        m.w32(m.r32(world + kCamera) + 8, m.r32(menu + kZoom));
    } else {
        set_geom_offset(static_cast<u32>(slideOffset(fade) + 80), 132);
        const u32 slot = mode == 0 ? static_cast<u32>(m.r16s(selectedModel(m) + 2)) : 2u;
        const u32 slots = m.r32(g::kSlots);
        m.w16(m.r32(slots + 4 * slot) + 24, 0);
        m.w16(m.r32(slots + 4 * slot) + 26, 4096);
        m.w16(m.r32(slots + 4 * slot) + 28, 0);
        const u32 world = m.r32(g::kWorld);
        m.w32(m.r32(world + kCamera) + 4, 0);
        m.w32(m.r32(world + kCamera) + 8, 0xFFF00000u);
    }
    const u32 camera = m.r32(m.r32(g::kWorld) + kCamera);
    m.w32(camera + 12, 0);
    return camera;
}

u32 Game::o2_selector_model_draw(u32 animated) {
    cover(0x80108874u);
    Frame frame(*this, 0x258);
    set_rot_matrix(m.r32(g::kWorld) + 0x4C);
    set_trans_matrix(m.r32(g::kWorld) + 0x4C);
    u32 world = m.r32(g::kWorld);
    set_back_color(m.r32(world + 0xAC), m.r32(world + 0xB0), m.r32(world + 0xB4));
    set_color_matrix(m.r32(g::kWorld) + 0x8C);
    set_rot_matrix(m.r32(g::kWorld) + 0x4C);
    set_trans_matrix(m.r32(g::kWorld) + 0x4C);
    for (u32 dimension = 0; dimension < 3; ++dimension) {
        constexpr u32 values[] = {320, 480, 250};
        m.w16(m.r32(m.r32(g::kWorld) + kCamera) + 0x20 + 2 * dimension, values[dimension]);
    }
    if (animated == 1) {
        const s32 fade = o2_fade_value(1);
        world = m.r32(g::kWorld);
        const u32 packet = m.r32(world + 20), rect = frame.at(0x220);
        m.w32(world + 20, packet + 12);
        m.w16(rect + 2, 58); m.w16(rect + 4, 155); m.w16(rect + 6, 147);
        m.w16(rect, static_cast<u32>(slideOffset(fade) + 23));
        set_draw_area(packet, rect);
        linkPacket(m, packet, m.r32(m.r32(g::kWorld) + 16) + 0xFC);
    }
    world = m.r32(g::kWorld);
    u32 packet = m.r32(world + 20);
    m.w32(world + 20, packet + 12);
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, 0, 0, 0, 0);
    linkPacket(m, packet, m.r32(m.r32(g::kWorld) + 16) + 4);
    world = m.r32(g::kWorld);
    packet = o2_model_scene_draw(world + 0x244, m.r32(world + 16) + 4,
                               m.r32(world + 20), frame.at(24));
    m.w32(m.r32(g::kWorld) + 20, packet + 12);
    m.w32(frame.at(16), 0);
    set_draw_mode(packet, 0, 1, 0, 0);
    u32 result = linkPacket(m, packet, m.r32(m.r32(g::kWorld) + 16));
    if (animated == 1) {
        world = m.r32(g::kWorld);
        packet = m.r32(world + 20);
        const u32 rect = frame.at(0x228);
        m.w32(world + 20, packet + 12);
        m.w16(rect, 0); m.w16(rect + 2, 0); m.w16(rect + 4, 320); m.w16(rect + 6, 480);
        set_draw_area(packet, rect);
        result = linkPacket(m, packet, m.r32(m.r32(g::kWorld) + 16));
    }
    return result;
}

u32 Game::o2_selector_model_tick() {
    cover(0x800F7BE8u);
    Frame frame(*this, 24);
    if (!o2_fade_state(2)) return 0;
    o2_selector_model_view(2);
    o2_selector_model_draw(1);
    return 1;
}

u32 Game::o2_selector_palette_draw(u32 weight) {
    cover(0x800F7C24u);
    Frame frame(*this, 0x238);
    u32 menu = m.r32(kMenu);
    const u32 colours = frame.at(16), rect = frame.at(0x210);
    for (s32 colour = 0; colour < (m.r16s(menu + 0xDE) * 16); ++colour) {
        const u32 from = m.r16(m.r32(menu + 0xD4) + 2 * static_cast<u32>(colour));
        const u32 to = m.r16(m.r32(menu + 0xD0) + 2 * static_cast<u32>(colour));
        u32 mixed = 0;
        for (u32 channel = 0; channel < 3; ++channel) {
            const u32 shift = 5 * channel;
            const s32 first = (from >> shift) & 31u, last = (to >> shift) & 31u;
            const s32 value = first + wrap::mul(last - first, static_cast<s32>(weight)) / 25;
            mixed |= static_cast<u32>(value) << shift;
        }
        m.w16(colours + 2 * static_cast<u32>(colour), mixed);
    }
    m.w32(rect, m.r32(m.r32(kMenu) + 0xD8));
    m.w32(rect + 4, m.r32(m.r32(kMenu) + 0xDC));
    m.w16(rect + 6, 1);
    const s32 rows = m.r16s(m.r32(kMenu) + 0xDE);
    if (rows <= 0) return static_cast<u32>(rows);
    for (s32 row = 0; row < m.r16s(m.r32(kMenu) + 0xDE); ++row) {
        const u32 world = m.r32(g::kWorld), packet = m.r32(world + 20);
        m.w32(world + 20, packet + 68);
        m.w16(rect + 2, m.r16(m.r32(kMenu) + 0xDA) + static_cast<u32>(row));
        set_draw_load(packet, rect);
        for (u32 word = 0; word < 8; ++word)
            m.w32(packet + 16 + 4 * word, m.r32(colours + 32 * static_cast<u32>(row) + 4 * word));
        linkPacket(m, packet, m.r32(m.r32(g::kWorld) + 16));
    }
    return 0;
}
u32 Game::o2_selector_commit_variants() {
    cover(0x800F9B10u);
    Frame frame(*this, 24);
    constexpr u32 kSelector = 0x80112E10u;
    u32 scene = m.r32(g::kScene);
    const u32 aircraftKind = o2_aircraft_label(static_cast<u32>(m.r16s(scene + 0x38C)),
                                             static_cast<u32>(m.r16s(scene + 0x38E)), 0,
                                             m.r8(m.r32(kSelector) + 20));
    if (aircraftKind < 6) m.w8(m.r32(g::kScene) + 0x3A9, aircraftKind + 1);
    scene = m.r32(g::kScene);
    const u32 weaponKind = o2_aircraft_label(static_cast<u32>(m.r16s(scene + 0x38C)),
                                           static_cast<u32>(m.r16s(scene + 0x38E)), 1,
                                           m.r8(m.r32(kSelector) + 21));
    if (weaponKind >= 9) return 0x800D0000u;
    constexpr u32 weaponType[] = {0, 1, 2, 1, 4, 10, 7, 1, 6};
    constexpr u32 weaponVariant[] = {0, 1, 2, 3, 4, 10, 7, 5, 6};
    m.w8(m.r32(g::kScene) + 0x3AA, weaponType[weaponKind]);
    scene = m.r32(g::kScene);
    m.w8(scene + 0x3AB, weaponVariant[weaponKind]);
    return weaponKind == 3 || weaponKind == 7 ? weaponVariant[weaponKind] : scene;
}
}
