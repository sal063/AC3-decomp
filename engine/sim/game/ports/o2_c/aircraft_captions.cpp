#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_aircraft_captions_draw() {
    cover(0x80103854u);
    Frame frame(*this, 80);
    constexpr u32 kPanel = 0x80112DACu, kMenu = 0x80112DC4u;
    const s32 newestState = o2_fade_state(2), firstState = o2_fade_state(3), secondState = o2_fade_state(4);
    const u32 secondStateWord = static_cast<u32>(secondState);
    m.w32(frame.at(32), secondStateWord);
    const s32 firstTick = o2_fade_value(3);
    const s32 secondTick = o2_fade_value(4);
    const u32 buffer = m.r32(m.r32(g::kScene) + 4), panel = m.r32(kPanel);
    u32 photo = panel + 0x8D4 + buffer * 40, modes = panel + 0x924 + buffer * 36;
    u32 label = panel + 0x96C + buffer * 40;
    const u32 ot = m.r32(m.r32(kMenu) + 8) + 4;
    auto prepend = [&](u32 packet) {
        m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
        const u32 tag = (m.r32(ot) & 0xFF000000u) | (packet & 0x00FFFFFFu);
        m.w32(ot, tag);
        return tag;
    };
    prepend(modes); modes += 12;
    m.w8(frame.at(24), 128); m.w8(frame.at(25), 128); m.w8(frame.at(26), 128); m.w8(frame.at(27), 0);
    auto cards = [&](u32 portrait, u32 caption, s32 index, s32 photoY, s32 labelY) {
        const s32 column = index % 4, row = index / 4;
        m.w8(portrait + 12, column * 60); m.w8(portrait + 13, 187);
        m.w16(portrait + 14, (static_cast<u32>(row + 16) << 6) | 27);
        m.w16(portrait + 8, 17); m.w16(portrait + 10, photoY);
        m.w8(caption + 12, column * 56 + 32); m.w8(caption + 13, 180);
        m.w16(caption + 14, (static_cast<u32>(row + 51) << 6) | 25);
        m.w16(caption + 8, 17); m.w16(caption + 10, labelY);
    };
    u32 count = m.r8(panel + 0x9CE);
    const bool firstVisible = count >= 2 || (count == 1 && newestState != 1);
    if (firstVisible) {
        if (firstState == 0 || firstState == 2) {
            cards(photo, label, static_cast<s32>(count) - 1, 73, 147);
            const u32 current = m.r32(kPanel);
            const u32 caption = m.r32(current + 0xA5C + (m.r8(current + 0x9CE) - 1) * 4);
            const u32 marker = m.r32(current + 0x9DC + 4 * m.r8(caption));
            m.w16(frame.at(16), 77); m.w16(frame.at(18), 109);
            m.w16(frame.at(20), m.r16(marker + 28)); m.w16(frame.at(22), m.r16(marker + 30));
        } else {
            const s32 offset = wrap::mul(firstTick, 107) / 30;
            cards(photo, label, static_cast<s32>(count) - 2, wrap::add(offset, 73), wrap::add(offset, 147));
        }
    }
    photo += 20; label += 20;
    count = m.r8(m.r32(kPanel) + 0x9CE);
    const bool secondVisible = count >= 3 || (count == 2 && firstState != 1);
    if (secondVisible) {
        if (secondState == 0 || secondState == 2)
            cards(photo, label, static_cast<s32>(count) - 2, 180, 254);
        else {
            const s32 offset = wrap::mul(secondTick, 311) / 30;
            cards(photo, label, static_cast<s32>(count) - 3, wrap::add(offset, 180), wrap::add(offset, 254));
        }
    }
    const u32 current = m.r32(kPanel), currentBuffer = m.r32(m.r32(g::kScene) + 4);
    photo = current + 0x8D4 + 40 * currentBuffer;
    if (firstVisible && !m.r8(current + 0x9D6)) { prepend(photo); photo += 20; }
    if (secondVisible && !m.r8(m.r32(kPanel) + 0x9D6)) prepend(photo);
    if (!m.r8(m.r32(kPanel) + 0x9D6)) { prepend(modes); modes += 12; }
    label = m.r32(kPanel) + 0x96C + 40 * m.r32(m.r32(g::kScene) + 4);
    if (firstVisible && !m.r8(m.r32(kPanel) + 0x9D6)) { prepend(label); label += 20; }
    if (secondVisible && !m.r8(m.r32(kPanel) + 0x9D6)) prepend(label);
    return prepend(modes);
}
}
