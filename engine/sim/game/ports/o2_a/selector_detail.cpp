#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSelector = 0x80112DD0u, kScreens = 0x80112DC0u, kVariants = 0x80109A90u;
u32 selector(Memory& m) { return m.r32(kSelector); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
}

u32 Game::o2_selector_variant_input() {
    cover(0x800D013Cu);
    Frame frame(*this, 40);
    const u32 original = selector(m), id = m.r8(original + 516 + 4 * m.r8(original + 2) + m.r8(original + 3));
    const u32 variants = kVariants + 12 * id;
    m.w8(flags(m) + 51, 0);
    const s32 last = static_cast<s32>(m.r32(variants) - 1);
    const u32 current = m.r8(selector(m) + 4);
    o2_flag(49, (static_cast<s32>(current) < last ? 2u : 0u) | (current ? 1u : 0u));
    if (m.r32(variants) != 0) {
        o2_selector_strips();
        if ((m.r32(m.r32(g::kScene) + 300) & 2) && static_cast<s32>(m.r8(selector(m) + 4)) < last) {
            m.w32(selector(m) + 600, 1); m.w8(flags(m) + 51, 2); sound_play(61);
            const u32 target = selector(m); m.w8(target + 4, m.r8(target + 4) + 1); m.w8(flags(m) + 51, 2);
        }
        if ((m.r32(m.r32(g::kScene) + 300) & 1) && m.r8(selector(m) + 4) != 0) {
            m.w32(selector(m) + 600, 2); m.w8(flags(m) + 51, 1); sound_play(61);
            const u32 target = selector(m); m.w8(target + 4, m.r8(target + 4) - 1); m.w8(flags(m) + 51, 1);
        }
        if (o2_fade_state(5) == 0) o2_fade_set(5, 1, 0, 10);
        for (u32 side = 0; side < 2; ++side) {
            const u32 skin = m.r8(selector(m) + 4), visible = m.r8(variants + 4 + 2 * skin + side);
            m.w8(flags(m) + 1 + side, visible);
            if (m.r8(variants + 4 + 2 * m.r8(selector(m) + 4) + side) && o2_fade_state(3 + side) == 0) {
                o2_fade_set(3 + side, 1, 0, 10); m.w32(selector(m) + 604, side + 1);
            }
        }
        m.w8(flags(m) + 3, m.r8(selector(m) + 604));
    } else m.w8(flags(m) + 3, 3);
    const u32 state = m.r32(g::kScene);
    if ((m.r32(state + 300) & 32) && !(m.r32(state + 296) & 0x5F)) {
        sound_play(59); m.w8(selector(m), 5);
        return 5;
    }
    return (m.r32(state + 300) & 32) ? m.r32(state + 296) & 0x5F : 0;
}
}
