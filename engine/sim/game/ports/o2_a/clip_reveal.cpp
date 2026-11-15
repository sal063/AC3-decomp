#include "sim/game/game.hpp"

namespace ac3::sim {
bool Game::o2_clip_reveal_width() {
    cover(0x800DF2E8u);
    Frame frame(*this, 8);
    constexpr u32 kScreens = 0x80112DC0u, kScratch = 0x80112DB4u;
    const u32 screens = m.r32(kScreens), initial = m.r32(screens + 60);
    const u32 fade = static_cast<u32>(m.r16s(initial + 12));
    const s32 value = m.r16s(m.r32(screens + 80) + 2 * fade);
    m.w32(frame.at(0), m.r16(initial + 4) | (static_cast<u32>(m.r16s(initial + 6)) << 16));
    m.w32(frame.at(4), m.r16(m.r32(screens + 60) + 8));
    const u32 clip = m.r32(screens + 60), scratch = m.r32(kScratch);
    const s32 state = m.r16s(m.r32(screens + 88) + 2 * static_cast<u32>(m.r16s(clip + 12)));
    if (state == 0) return false;
    if (state == 2) return true;
    const s32 start = m.r16s(frame.at(0)), end = m.r16s(frame.at(2));
    if (value < start) return false;
    if (value >= end) return true;
    const u32 distance = static_cast<u32>(m.r16s(scratch) - m.r16s(clip));
    const s32 product = static_cast<s32>(distance * static_cast<u32>(value - start));
    m.w16(scratch, m.r16(clip) + static_cast<u32>(product / (end - start)));
    const s32 nextStart = m.r16s(frame.at(0)), nextEnd = m.r16s(frame.at(2));
    const s32 widthProduct = static_cast<s32>(static_cast<u32>(m.r16s(scratch + 4)) * static_cast<u32>(value - nextStart));
    const s32 duration = nextEnd - nextStart;
    if (!duration) unported(0x800DF42Cu, "zero duration in horizontal clip reveal");
    if (duration == -1 && widthProduct == (-2147483647 - 1))
        unported(0x800DF444u, "overflow in horizontal clip reveal");
    m.w16(scratch + 4, static_cast<u32>(widthProduct / duration));
    return true;
}
}
