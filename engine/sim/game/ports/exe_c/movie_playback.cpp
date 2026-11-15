#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPlayer = 0x800BD054u, kWork = 0x800BD050u;
constexpr u32 kRectangles = 0x800C7800u, kPixels = 0x800C78A0u;
u32 cue(Memory& m, u32 player) { return player + 70 * m.r8(player + 159) + 14 * m.r8(player + 16); }
}

u32 Game::movie_poll(u32 player) {
    cover(0x80058938u);
    Frame frame(*this, 0x28);
    if (kcd_ring_get(frame.at(0x10), frame.at(0x14))) return 0;
    const u32 data = m.r32(frame.at(0x10)), sector = m.r32(frame.at(0x14));
    if (!m.r16(sector + 6)) m.w8(player + 160, 1);
    const u32 header = m.r8(sector + 12), event = header >> 4, encoding = header & 15u;
    const u32 firstFrame = m.r8(sector + 13), strip = m.r8(sector + 15), finalStrip = m.r8(sector + 19) & 15u;
    if (strip >= 2) {
        for (u32 word = 573; word < 1125; ++word) m.w32(data + 4 * word, m.r32(data + 4 * (word + 1)));
    } else {
        for (u32 index = 0; index < 256; ++index) {
            const u32 color = data + 20 + 2 * index;
            if (m.r16(color)) m.w16(color, m.r16(color) | 0x8000u);
        }
    }
    if (strip == finalStrip + 15) {
        m.w8(player + 158, m.r8(player + 158) == 0);
        if (firstFrame == 1) m.w8(player + 158, 0);
        for (u32 index = 0; index < 5; ++index) {
            auto record = [&] { return player + 70 * m.r8(player + 158) + 14 * index; };
            m.w8(record() + 18, 0);
            m.w16(record() + 20, 0xFFFF); m.w16(record() + 22, 0xFFFF);
            for (u32 parameter = 0; parameter < 4; ++parameter) m.w16(record() + 24 + 2 * parameter, 0);
        }
    }
    if (strip >= 2 && strip <= 17) {
        const u32 palette = m.r8(sector + 19) >> 4;
        m.w8(player + 20 * m.r8(player + 158) + 179 - strip, palette);
        if (strip <= 5) m.w8(player + 20 * m.r8(player + 158) + 183 - strip, m.r8(sector + 19) >> 4);
    }
    const u32 cueHeader = m.r8(sector + 16), cueIndex = cueHeader >> 4;
    if ((cueHeader & 15u) == 1) {
        auto record = [&] { return player + 70 * m.r8(player + 158) + 14 * cueIndex; };
        m.w8(record() + 18, m.r8(sector + 17) & 15u);
        m.w16(record() + 20, m.r16(sector + 20));
        m.w16(record() + 22, m.r16(sector + 22));
        for (u32 parameter = 0; parameter < 4; ++parameter)
            m.w16(record() + 24 + 2 * parameter, m.r16(sector + 24 + 2 * parameter));
    }
    if (!encoding) {
        const u32 rectangle = kRectangles + 8 * strip;
        if (strip < 2) {
            m.w16(rectangle, 768); m.w16(rectangle + 4, 256); m.w16(rectangle + 6, 1);
            const u32 bank = m.r8(player + 158);
            if (strip == 0) m.w16(kRectangles + 2, bank ? 283 : 281);
            else m.w16(kRectangles + 10, bank ? 282 : 280);
        } else {
            const s32 column = static_cast<s32>(strip) - 2, band = column / 4;
            m.w16(rectangle, (m.r8(player + 158) ? 896u : 768u) - 32u * static_cast<u32>(column % 4 - 3));
            m.w16(rectangle + 2, static_cast<u32>(-70 * (band - 3)));
            m.w16(rectangle + 4, 32); m.w16(rectangle + 6, 70);
        }
        m.w32(kPixels + 4 * strip, data + 20);
        hw->load_image(rectangle, data + 20);
    }
    if (event == 1) m.w8(player, firstFrame == 1 ? 2u : 1u);
    kcd_ring_free(sector);
    return 1;
}

u32 Game::movie_wait(u32 once) {
    cover(0x8005889Cu);
    Frame frame(*this, 0x20);
    if (once) return movie_poll(m.r32(kPlayer)) != 0;
    m.w32(frame.at(0x10), 120);
    while (!movie_poll(m.r32(kPlayer))) {
        kcd_pump();
        hw->vsync(0);
        const s32 remaining = wrap::sub(m.r32s(frame.at(0x10)), 1);
        m.w32(frame.at(0x10), static_cast<u32>(remaining));
        if (remaining < 0) { m.w8(m.r32(kPlayer) + 160, 1); break; }
    }
    return 1;
}

u32 Game::movie_draw_cues(u32 player, u32 packet, u32 ot) {
    cover(0x800592E0u);
    Frame frame(*this, 0x20);
    u32 record = cue(m, player);
    if (m.r32s(player + 4) == m.r16s(record + 22) + m.r16s(record + 30)) {
        m.w8(player + 10, 0); m.w32(player + 12, 0);
        m.w8(player + 16, m.r8(player + 16) + 1u);
    }
    record = cue(m, player);
    if (m.r32s(player + 4) == m.r16s(record + 20)) {
        m.w32(player + 12, 0); m.w8(player + 10, m.r8(record + 18));
    }
    const u32 effect = m.r8(player + 10);
    switch (effect) {
    case 0: packet = movie_draw_bank(player, packet, ot); break;
    case 1: packet = movie_draw_crossfade(player, movie_draw_bank(player, packet, ot), ot); break;
    case 2: case 3: case 4: case 5: packet = movie_draw_fade(player, movie_draw_bank(player, packet, ot), ot); break;
    case 6: packet = movie_draw_zoom(player, packet, ot); break;
    case 7: packet = movie_draw_shrink(player, packet, ot); break;
    case 8: case 9: packet = movie_draw_bank(player, packet, ot); break;
    default: break;
    }
    if (effect >= 1 && effect <= 9) m.w32(player + 12, m.r32(player + 12) + 1u);
    return movie_draw_border(packet, ot);
}

u32 Game::movie_tick() {
    cover(0x8005911Cu);
    Frame frame(*this, 0x20);
    const u32 player = m.r32(kPlayer);
    if (m.r8(player)) {
        if (m.r8(player) == 1) { m.w8(player + 159, m.r8(player + 159) ^ 1u); m.w16(player + 8, m.r16(player + 8) + 1u); }
        else { m.w8(player + 159, 0); m.w16(player + 8, 0); m.w8(player + 1, 1); }
        m.w8(player, 0); m.w8(player + 16, 0); m.w32(player + 4, 0); m.w8(player + 10, 0);
    }
    u32 packet;
    if (m.r8(player + 1) == 1) {
        packet = movie_draw_cues(player, m.r32(m.r32(kWork) + 52), m.r32(m.r32(kWork) + 56));
    } else {
        m.w32(frame.at(0x10), m.r32(0x800BCDA4u)); m.w32(frame.at(0x14), m.r32(0x800BCDA8u));
        const u32 age = m.r32(player + 4);
        if (age - 30u < 181u) {
            s32 alpha = age - 30u < 64u ? 4096 - static_cast<s32>((age - 30u) << 6) :
                age - 94u < 52u ? 0 : age - 146u < 64u ? static_cast<s32>((age - 146u) << 6) : 4096;
            if (alpha < 0) alpha = 0;
            m.w32(m.r32(kWork) + 52, noise_rect(m.r32(m.r32(kWork) + 56) + 12, m.r32(m.r32(kWork) + 52),
                frame.at(0x10), static_cast<u32>(alpha)));
        }
        packet = movie_draw_border(m.r32(m.r32(kWork) + 52), m.r32(m.r32(kWork) + 56));
    }
    m.w32(m.r32(kWork) + 52, packet);
    movie_poll(m.r32(kPlayer));
    const u32 finished = m.r8(m.r32(kPlayer) + 160) == 1;
    m.w32(m.r32(kPlayer) + 4, m.r32(m.r32(kPlayer) + 4) + 1u);
    return finished;
}
}
