#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kTrig = 0x800B7104u, kScratch = 0x1F800000u;
constexpr u32 kContext = kScratch + 28, kSavedRotation = kContext + 56;
struct Product { s32 x, y, z; };
Product products(Gte& gte) {
    return {static_cast<s32>(gte.mfc2(25)), static_cast<s32>(gte.mfc2(26)), static_cast<s32>(gte.mfc2(27))};
}
struct Column { u32 x, y, z; };
Column rotated_column(Gte& gte) { return {gte.mfc2(9), gte.mfc2(10), gte.mfc2(11)}; }
s32 magnitude(s32 x) { return x < 0 ? wrap::neg(x) : x; }
}

u32 Game::particle_rotation(u32 angles, u32 scale, u32 matrix) {
    cover(0x800867E4u);
    const u32 pitch = 4 * (m.r16(angles) & 4095u), yaw = 4 * (m.r16(angles + 2) & 4095u);
    const s32 cp = m.r16s(kTrig + pitch + 2), spitch = m.r16s(kTrig + pitch);
    const s32 cy = m.r16s(kTrig + yaw + 2), sy = m.r16s(kTrig + yaw);
    gte.mtc2(8, scale); gte.mtc2(9, spitch); gte.mtc2(10, cp); gte.mtc2(11, cy);
    gte.op(0x198003Du); const Product scaled = products(gte);
    const u32 roll = 4 * (m.r16(angles + 4) & 4095u);
    const s32 cr = m.r16s(kTrig + roll + 2), sr = m.r16s(kTrig + roll);
    gte.mtc2(8, scaled.y); gte.mtc2(9, sy); gte.mtc2(10, sr); gte.mtc2(11, cr);
    m.w16(matrix + 10, wrap::neg(scaled.x));
    gte.op(0x198003Du); const Product pitchProducts = products(gte);
    const s32 scaledYawSine = wrap::mul(static_cast<s32>(scale), sy) >> 12;
    gte.mtc2(8, scaled.z); gte.mtc2(9, sr); gte.mtc2(10, cr); gte.mtc2(11, cp);
    m.w16(matrix + 4, pitchProducts.x); m.w16(matrix + 6, pitchProducts.y);
    gte.op(0x198003Du); m.w16(matrix + 8, pitchProducts.z);
    const Product yawProducts = products(gte);
    gte.mtc2(8, scaledYawSine); gte.mtc2(9, cr); gte.mtc2(10, sr);
    m.w16(matrix + 16, yawProducts.z); gte.op(0x198003Du);
    const Product crossed = products(gte);
    gte.mtc2(8, spitch); gte.mtc2(9, crossed.x); gte.mtc2(10, yawProducts.x); gte.mtc2(11, yawProducts.y);
    gte.op(0x198003Du); const Product pitchCrossed = products(gte);
    gte.mtc2(8, spitch); gte.mtc2(9, crossed.y);
    m.w16(matrix + 2, wrap::sub(pitchCrossed.x, yawProducts.x)); gte.op(0x198003Du);
    m.w16(matrix + 12, wrap::sub(pitchCrossed.y, crossed.x));
    m.w16(matrix + 14, wrap::add(crossed.y, pitchCrossed.z));
    const u32 lastProduct = gte.mfc2(25);
    m.w16(matrix, yawProducts.y + lastProduct);
    return lastProduct;
}

u32 Game::particle_compose_rotation(u32 matrix) {
    cover(0x80086948u);
    gte.mtc2(9, m.r16s(matrix)); gte.mtc2(10, m.r16s(matrix + 6)); gte.mtc2(11, m.r16s(matrix + 12));
    const s32 secondX = m.r16s(matrix + 2), secondY = m.r16s(matrix + 8);
    gte.op(0x49E012u); const s32 secondZ = m.r16s(matrix + 14);
    const Column first = rotated_column(gte);
    gte.mtc2(9, secondX); gte.mtc2(10, secondY); gte.mtc2(11, secondZ); gte.op(0x49E012u);
    const s32 thirdX = m.r16s(matrix + 4), thirdY = m.r16s(matrix + 10), thirdZ = m.r16s(matrix + 16);
    const Column second = rotated_column(gte);
    gte.mtc2(9, thirdX); gte.mtc2(10, thirdY); gte.mtc2(11, thirdZ); gte.op(0x49E012u);
    const Column third = rotated_column(gte);
    gte.ctc2(0, (first.x & 65535u) | (second.x << 16));
    gte.ctc2(1, (first.y << 16) | (third.x & 65535u));
    gte.ctc2(2, (second.y & 65535u) | (third.y << 16));
    gte.ctc2(3, (first.z & 65535u) | (second.z << 16)); gte.ctc2(4, third.z);
    return third.x & 65535u;
}

u32 Game::particle_spray_draw(u32 bank) {
    cover(0x800854B8u);
    Frame frame(*this, 0x40);
    u32 count = 0;
    struct Exact {
        Gte::Exact centre;
        double radius = 0;
    };
    std::array<Exact, 256> exact{};
    for (u32 group = 0; group < 3; ++group) {
        u32 mask = m.r32(bank + 4 * group);
        m.w32(kContext + 40 + 4 * group, 0);
        for (u32 bit = 0; mask; ++bit, mask >>= 1) {
            if (!(mask & 1u)) continue;
            const u32 particle = bank + 12 + 896 * group + 28 * bit;
            if (m.r16s(particle) < 0) continue;
            const s32 x = wrap::sub(m.r32s(particle + 4), m.r32s(kScratch + 8)) >> 12;
            if (static_cast<u32>(x) + 32768u > 65535u) continue;
            const s32 y = wrap::sub(m.r32s(particle + 8), m.r32s(kScratch + 12)) >> 12;
            if (static_cast<u32>(y) + 32768u > 65535u) continue;
            const s32 z = wrap::sub(m.r32s(particle + 12), m.r32s(kScratch + 16)) >> 12;
            if (static_cast<u32>(z) + 32768u > 65535u) continue;
            const s32 size = m.r16s(particle + 26);
            const u32 sample = kContext + 100 + 8 * count;
            m.w16(sample + 6, size);
            const bool close = size < 256 && magnitude(x) + magnitude(y) + magnitude(z) < 1024;
            gte.mtc2(0, (static_cast<u32>(close ? 2 * x : x) & 65535u) |
                (static_cast<u32>(close ? 2 * y : y) << 16));
            gte.mtc2(1, close ? 2 * z : z);
            const s32 nearPlane = m.r16s(kScratch + 26);
            gte.op(0x180001u);
            const s32 depth = static_cast<s32>(gte.mfc2(27));
            if (depth < nearPlane) continue;
            const s32 adjustedDepth = depth >> (close ? 1 : 0);
            const s32 numerator = wrap::mul(size, nearPlane);
            if (!adjustedDepth) unported(0x80085688u, "particle projection division by zero");
            if (adjustedDepth == -1 && numerator == static_cast<s32>(0x80000000u))
                unported(0x800856A0u, "particle projection division overflow");
            const s32 radius = static_cast<s32>(wrap::div(numerator, adjustedDepth).lo);
            if (!radius) continue;
            const u32 screen = gte.mfc2(14);
            if (radius + 160 < magnitude(static_cast<s16>(screen))) { m.w16(sample + 6, radius); continue; }
            m.w16(sample + 6, radius);
            if (radius + 120 < magnitude(static_cast<s32>(screen) >> 16)) continue;
            s32 layer = (adjustedDepth >> 3) - 1;
            m.w32(sample, screen);
            if (layer >= 4095) layer = 4094;
            else if (layer < 0) layer = 0;
            m.w16(sample + 4, layer);
            if (count < exact.size()) {
                const Gte::Exact& e = gte.exact_fifo[2];
                exact[count].centre = e;
                exact[count].radius = e.ok && e.vz > 0 ? static_cast<double>(numerator) / (e.vz / (close ? 2.0 : 1.0)) /
                                                             static_cast<double>(radius)
                                                       : 0.0;
            }
            ++count;
            const u32 visible = kContext + 40 + 4 * group;
            m.w32(visible, m.r32(visible) | (1u << bit));
        }
    }
    if (!count) return kSavedRotation;
    for (u32 word = 0; word < 5; ++word) m.w32(kSavedRotation + 4 * word, gte.cfc2(word));
    gte.mtc2(1, 0);
    m.w32(kContext + 88, 0xFFFFFFFFu);
    m.w32(kContext, m.r32(kScratch)); m.w32(kContext + 4, m.r32(kScratch + 4));
    const u32 colors = m.r32(m.r32(g::kWorld) + 616);
    m.w32(kContext + 96, (m.r32(colors + 16) & 0xFFFFFFu) | 0x2E000000u);
    u32 sampleIndex = 0;
    for (u32 group = 0; group < 2; ++group) {
        u32 mask = m.r32(kContext + 40 + 4 * group);
        for (u32 bit = 0; mask; ++bit, mask >>= 1) {
            if (!(mask & 1u)) continue;
            const u32 particle = bank + 12 + 896 * group + 28 * bit;
            m.w32(kContext + 16, 0u - static_cast<u32>(m.r16s(kScratch + 24)));
            const u32 kind = m.r16(particle + 24);
            const u32 type = ((kind >> 4) & 15u) < 4 ? (kind >> 4) & 15u : 3u;
            const u32 index = sampleIndex;
            const u32 sample = kContext + 100 + 8 * sampleIndex++;
            m.w32(kContext + 8, (kind >> 8) & 3u); m.w32(kContext + 12, kind & 15u);
            m.w32(frame.at(0x10), bank);
            const u32 first = m.r32(kContext + 4);
            if (index < exact.size()) fx_begin(m.r32(sample), exact[index].centre, exact[index].radius);
            switch (type) {
            case 0: particle_puff(kContext, sample, particle); break;
            case 1: particle_growing_puff(kContext, sample, particle); break;
            case 2: particle_splash(kContext, sample, particle); break;
            case 3: particle_pulse(kContext, sample, particle); break;
            }
            fx_record(first, m.r32(kContext + 4));
            fx_end();
        }
    }
    m.w32(kScratch + 4, m.r32(kContext + 4));
    for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(kSavedRotation + 4 * word));
    return kSavedRotation;
}
}
