#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSineSamples = 0x800104ACu;
constexpr u32 kPaletteInfo = 580;
constexpr u32 kPaletteDepths = 648;
s64 product64(s64 a, s64 b) { return static_cast<s64>(static_cast<u64>(a) * static_cast<u64>(b)); }
}

void Game::depth_cue(s32 near, s32 far, s32 projection) {
    cover(0x8005E7ECu);
    Frame frame(*this, 0x60);
    m.w32(frame.at(0x64), static_cast<u32>(far));
    const s64 scaledInterval = static_cast<s64>(wrap::sub(near, far)) * 0x1000000;
    m.w32(frame.at(0x30), static_cast<u32>(scaledInterval));
    m.w32(frame.at(0x34), static_cast<u32>(static_cast<u64>(scaledInterval) >> 32));
    const s64 normalized = divdi3(scaledInterval, near);
    const s64 projected = divdi3(product64(normalized, projection), far);
    s32 slope = static_cast<s32>(divdi3(0x100000000ll, projected));
    s64 intercept;
    if (static_cast<u32>(slope) + 0x8000u > 0xFFFFu) {
        slope = slope > 0 ? 32767 : -32768;
        divdi3(product64(static_cast<s64>(wrap::neg(slope)) * 65536, projection), near);
        const s64 quotient = divdi3(product64(static_cast<s64>(slope) * 65536, projection), far);
        intercept = static_cast<s64>(0x1000000ull - static_cast<u64>(quotient));
    } else {
        intercept = divdi3(product64(static_cast<s64>(wrap::neg(slope)) * 65536, projection), near);
    }
    const s32 offset = intercept > 2147483647ll ? 2147483647
        : intercept < -2147483647ll ? -2147483647 : static_cast<s32>(intercept);
    gte.ctc2(27, static_cast<u32>(slope));
    gte.ctc2(28, static_cast<u32>(offset));
}

u32 Game::palette_animate(u32 orderTable, u32 packet, u32 list, s32 count) {
    cover(0x8005EDC8u);
    Frame frame(*this, 0x28);
    const u32 color = m.r32(m.r32(g::kWorld) + kPaletteInfo) + 8;
    const u32 red = m.r8(color) << 4, green = m.r8(color + 1) << 4, blue = m.r8(color + 2) << 4;
    const u32 clock = m.r32(m.r32(g::kScene) + 40);
    for (s32 index = 0; index < count; ++index, list += 12) {
        gte.mtc2(28, static_cast<u32>(m.r16s(list + 6)));
        gte.ctc2(21, m.r8(list + 8) << 4);
        gte.ctc2(22, m.r8(list + 9) << 4);
        gte.ctc2(23, m.r8(list + 10) << 4);
        const u32 phase = (wrap::shl(clock, static_cast<u32>(m.r16s(list + 2)))
            + static_cast<u32>(m.r16s(list + 4))) & 0x3FFFu;
        gte.mtc2(8, static_cast<u32>((m.r16s(kSineSamples + 2 * phase) >> 3) + 2048));
        gte.op(0x980011u);
        u32 depths = m.r32(m.r32(g::kWorld) + kPaletteDepths);
        const u32 paletteIndex = m.r16(list);
        const u32 destination = (static_cast<u32>((static_cast<s16>(paletteIndex) >> 4) + 256) << 16)
            | ((paletteIndex & 15u) + 320u);
        const u32 source = gte.mfc2(22);
        gte.ctc2(21, red); gte.ctc2(22, green); gte.ctc2(23, blue);
        gte.mtc2(6, source);
        for (u32 shade = 0; shade < 16; ++shade, depths += 2, packet += 24) {
            gte.mtc2(8, fog_off() ? 0u : 256 * shade);
            gte.op(0x780010u);
            const u32 bucket = orderTable + 4u * static_cast<u32>(m.r16s(depths));
            m.w32(packet, 0x05000000u);
            m.w32(packet + 16, gte.mfc2(29));
            m.w32(packet + 4, 0xA0000000u);
            m.w32(packet + 8, destination);
            m.w32(packet + 12, 0x00010001u);
            m.w32(packet + 20, 0x01000000u);
            ot_link(bucket, packet);
        }
    }
    return packet;
}
}
