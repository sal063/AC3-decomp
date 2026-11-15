#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kEntries = 364, kClip = 60, kFadeValues = 80, kFadeStates = 88, kFlags = 92;
}

s32 Game::o2_entry_state(u32 group, u32 index) {
    cover(0x800DED38u);
    const u32 entries = m.r32(m.r32(kMenu) + kEntries + 4 * group);
    return m.r16s(entries + 12 * index + 10);
}

u32 Game::o2_flag_value(u32 index) {
    cover(0x800DEE60u);
    return m.r8(m.r32(m.r32(kScreens) + kFlags) + index);
}

s32 Game::o2_fade_value(u32 index) {
    cover(0x800DEEA8u);
    return m.r16s(m.r32(m.r32(kScreens) + kFadeValues) + 2 * index);
}

u32 Game::o2_clip_accept() {
    cover(0x800DEFECu);
    return 1;
}

u32 Game::o2_clip_fade_complete() {
    cover(0x800DF2B0u);
    const u32 screens = m.r32(kScreens), clip = m.r32(screens + kClip);
    const u32 index = static_cast<u32>(m.r16s(clip + 12));
    return m.r16s(m.r32(screens + kFadeStates) + 2 * index) == 2;
}
}
