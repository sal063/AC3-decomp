#include <array>
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSelector = 0x80112E10u, kVoice = 0x80112E0Cu;
constexpr u32 kMode = 0x1C6, kCurrentDescriptor = 8, kSavedDescriptor = 0x124;
constexpr u32 kSelectorCallbacks = 0x8010ECF8u, kModelCallbacks = 0x8010984Cu;
}

u32 Game::o2_voice_samples(u32 channel) {
    cover(0x800F770Cu);
    Frame frame(*this, 32);
    const u32 decoded = hw->spu_read_decoded_data(m.r32(kVoice) + 24, 5);
    hw->spu_is_transfer_completed(1);
    const u32 voice = m.r32(kVoice);
    if (channel == 0) return voice + (decoded ? 24 : 0xE0);
    if (channel == 1) return voice + (decoded ? 0x418 : 0x4E0);
    return 1;
}

u32 Game::o2_voice_amplitude() {
    cover(0x800F77A4u);
    Frame frame(*this, 32);
    const u32 left = o2_voice_samples(0), right = o2_voice_samples(1);
    u32 leftTotal = 0, rightTotal = 0;
    for (u32 sample = 0; sample < 100; ++sample) {
        const s32 l = m.r16s(left + 2 * sample), r = m.r16s(right + 2 * sample);
        leftTotal += static_cast<u32>(l < 0 ? -l : l);
        rightTotal += static_cast<u32>(r < 0 ? -r : r);
    }
    return static_cast<u32>(static_cast<s16>((leftTotal / 100 + rightTotal / 100) / 2));
}

u32 Game::o2_selector_model_stage(u32 request) {
    cover(0x800F7994u);
    Frame frame(*this, 32);
    if (o2_fade_state(2) && request == 1) return 0;
    const s32 mode = m.r16s(m.r32(kMenu) + kMode);
    if (mode != 3) {
        u32 selector = m.r32(kSelector);
        if (mode == 0) {
            const u32 source = m.r32(selector + kCurrentDescriptor);
            std::array<u32, 4> first{};
            for (u32 i = 0; i < 4; ++i) first[i] = m.r32(source + 4 * i);
            for (u32 i = 0; i < 4; ++i) m.w32(selector + kSavedDescriptor + 4 * i, first[i]);
            const u32 fifth = m.r32(source + 16), sixth = m.r32(source + 20);
            m.w32(selector + kSavedDescriptor + 16, fifth);
            m.w32(selector + kSavedDescriptor + 20, sixth);
            m.w32(m.r32(kSelector) + 0x120, 1);
        }
        selector = m.r32(kSelector);
        m.w32(selector + kCurrentDescriptor, selector + kSavedDescriptor);
        const u32 callback = m.r32(kModelCallbacks + 4 * static_cast<u32>(m.r16s(m.r32(kMenu) + kMode)));
        call_by_address(callback, "prepare selector model for menu mode", 1);
        return 0;
    }
    u32 menu = m.r32(kMenu);
    const u32 group = o2_mission_aircraft_group_index(static_cast<u32>(m.r16s(m.r32(g::kScene) + 0x38C)),
                                                    m.r32(menu + 0xC0));
    const u32 pack = ld_relocate_dir(m.r32(m.r32(menu + 0xF4) + 4 * group));
    o2_upload_menu_palette(m.r32(pack));
    ulz_decode(m.r32(pack + 4), m.r32(m.r32(kSelector) + 4));
    const u32 world = m.r32(g::kWorld);
    m.w32(m.r32(world + 0x250) + 0x20, m.r32(m.r32(kSelector) + 4));
    const u32 model = m.r32(m.r32(world + 0x250) + 0x20);
    ld_relocate_words(model, model + 4, 6);
    const u32 objects = m.r32(0x800BCE5Cu);
    m.w32(m.r32(objects), 0);
    const u32 preview = m.r32(objects + 8);
    m.w32(preview, m.r32(preview) | 0x1C000u);
    o2_fade_set(2, 1, 0, 25);
    o2_fade_set(8, 1, 0, 25);
    const u32 selector = m.r32(kSelector);
    if (m.r32(selector + 0x120)) {
        m.w32(selector + 12, selector + kSavedDescriptor);
        m.w32(selector + kCurrentDescriptor, selector + kSavedDescriptor);
        m.w32(selector + 0x120, 0);
    } else m.w32(selector + 12, m.r32(selector + kCurrentDescriptor));
    m.w32(m.r32(kSelector) + kCurrentDescriptor, m.r32(m.r32(kMenu) + 0xB8) + 24 * group);
    return 1;
}

u32 Game::o2_selector_tick() {
    cover(0x800FB10Cu);
    Frame frame(*this, 32);
    if (!(m.r32(m.r32(g::kScene) + 0x96C) & 16u) && m.r16s(m.r32(kSelector)) != 5)
        o2_voice_control(1, 0);
    if (m.r32(m.r32(kSelector)) != 0x00020002u) {
        const u32 world = m.r32(g::kWorld);
        clear_otag_r(m.r32(world + 16), 64);
        const u32 activeWorld = m.r32(g::kWorld), menu = m.r32(kMenu);
        const u32 table = m.r32(activeWorld + 16), parent = m.r32(menu + 8) + 12;
        m.w32(table, (m.r32(table) & 0xFF000000u) | (m.r32(parent) & 0xFFFFFFu));
        m.w32(parent, (m.r32(parent) & 0xFF000000u) | ((table + 252) & 0xFFFFFFu));
        o2_selector_model_tick();
    }
    const u32 state = m.r16(m.r32(kSelector));
    if (state - 3 < 3) {
        o2_selector_variants_tick();
        const u32 menu = m.r32(kMenu);
        const u32 packet = o2_selector_cards(m.r32(menu + 0x274), m.r32(menu + 8));
        m.w32(m.r32(kMenu) + 0x274, packet);
    }
    const u32 packet = o2_selector_chart(m.r32(m.r32(kMenu) + 0x274));
    m.w32(m.r32(kMenu) + 0x274, packet);
    const u32 callback = m.r32(kSelectorCallbacks + 4 * static_cast<u32>(m.r16s(m.r32(kSelector))));
    switch (callback) {
    case 0x800F9E68u: o2_selector_begin(); break;
    case 0x800F9EC8u: o2_selector_choose(); break;
    case 0x800FA4E4u: o2_selector_commit_tick(); break;
    case 0x800FA860u: o2_selector_fade_in(); break;
    case 0x800FA8E0u: o2_selector_confirm(); break;
    case 0x800FAA48u: o2_selector_exit(); break;
    case 0x800FABE8u: o2_selector_alloc(); break;
    case 0x800FB10Cu: o2_selector_tick(); break;
    default:
        if (callback >= 0x800F0000u && callback < 0x80110000u)
            unported(callback, "invalid selector callback table entry");
        call_by_address(callback, "selector callback outside o2_c");
    }
    return o2_voice_control(0, 0);
}
}
