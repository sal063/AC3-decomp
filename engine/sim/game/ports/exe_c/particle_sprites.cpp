#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kAngles = 0x800B7104u;
constexpr u32 kOrderTable = 0, kPackets = 4, kBlend = 8, kVariant = 12;
constexpr u32 kAngle = 16, kWidth = 20, kHeight = 22, kOffset = 24, kCenter = 28;
constexpr u32 kCachedAngle = 88, kColor = 96;
u32 signed_word(s32 value) { return static_cast<u32>(value); }
u32 packed(s32 x, s32 y) { return (signed_word(x) & 0xFFFFu) | (signed_word(y) << 16); }
void atlas_frame(Memory& m, u32 packet, s32 index, s32 size) {
    const u32 shift = size < 17 ? 1u : 0u;
    const s32 left = static_cast<s32>((signed_word(index) & 3u) << 6) >> shift;
    const s32 right = (static_cast<s32>(((signed_word(index) & 3u) << 6) + 64) >> shift) - 1;
    const s32 top = static_cast<s32>(signed_word(index >> 2) << 6) >> shift;
    const s32 bottom = (static_cast<s32>((signed_word(index >> 2) << 6) + 64) >> shift) - 1;
    m.w8(packet + 21, top); m.w8(packet + 13, top);
    m.w8(packet + 37, bottom); m.w8(packet + 29, bottom);
    m.w8(packet + 28, left); m.w8(packet + 12, left);
    m.w8(packet + 36, right); m.w8(packet + 20, right);
}
u32 link_sprite(Memory& m, u32 context, u32 sample, u32 packet, u32 words) {
    const u32 head = m.r32(context + kOrderTable) + 4u * signed_word(m.r16s(sample + 4));
    m.w32(packet, m.r32(head) | (words << 24));
    m.w32(head, packet & 0xFFFFFFu);
    const u32 next = packet + 4 * (words + 1);
    m.w32(context + kPackets, next);
    return next;
}
}

u32 Game::particle_quad(u32 context, u32 packet) {
    cover(0x8008591Cu);
    s32 centerX = m.r16s(context + kCenter), centerY = m.r16s(context + kCenter + 2);
    const u32 angle = (m.r32(context + kAngle) >> 2) & 0xFFFu;
    s32 upperX, upperY, lowerX, lowerY;
    if (angle) {
        const u32 offset = m.r32(context + kOffset);
        if (angle != m.r32(context + kCachedAngle)) {
            const u32 sineCosine = m.r32(kAngles + 4 * angle);
            m.w32(context + kCachedAngle, angle);
            const u32 cosine = sineCosine >> 16;
            gte.ctc2(0, cosine | ((0u - sineCosine) << 16));
            gte.ctc2(2, cosine); gte.ctc2(1, sineCosine << 16);
            gte.ctc2(3, 0); gte.ctc2(4, 4096);
        }
        if (offset) {
            gte.mtc2(0, offset); gte.op(0x486012u);
            centerX = wrap::add(centerX, static_cast<s32>(gte.mfc2(25)));
            centerY = wrap::add(centerY, static_cast<s32>(gte.mfc2(26)));
        }
        const s32 halfWidth = m.r16(context + kWidth) >> 1;
        const s32 height = m.r16s(context + kHeight);
        gte.mtc2(0, packed(halfWidth, height >> 1)); gte.op(0x486012u);
        upperX = static_cast<s32>(gte.mfc2(25)); upperY = static_cast<s32>(gte.mfc2(26));
        gte.mtc2(0, packed(halfWidth, -((height + 1) >> 1))); gte.op(0x486012u);
        lowerX = static_cast<s32>(gte.mfc2(25)); lowerY = static_cast<s32>(gte.mfc2(26));
    } else {
        centerX += m.r16s(context + kOffset); centerY += m.r16s(context + kOffset + 2);
        upperX = lowerX = m.r16s(context + kWidth) >> 1;
        const s32 height = m.r16s(context + kHeight);
        upperY = height >> 1; lowerY = (1 - height) >> 1;
    }
    m.w16(packet + 8, wrap::sub(centerX, upperX)); m.w16(packet + 10, wrap::sub(centerY, upperY));
    m.w16(packet + 16, wrap::add(centerX, lowerX)); m.w16(packet + 18, wrap::add(centerY, lowerY));
    m.w16(packet + 24, wrap::sub(centerX, lowerX)); m.w16(packet + 26, wrap::sub(centerY, lowerY));
    m.w16(packet + 32, wrap::add(centerX, upperX));
    const u32 bottom = signed_word(wrap::add(centerY, upperY)); m.w16(packet + 34, bottom);
    return bottom;
}

u32 Game::particle_puff(u32 context, u32 sample, u32 particle) {
    cover(0x80085A94u);
    Frame frame(*this, 0x28);
    const s32 age = m.r16s(particle), lifetime = m.r16s(particle + 2);
    const s32 index = 10 * age / (lifetime ? lifetime : 1) + 6;
    const u32 packet = m.r32(context + kPackets);
    const s32 size = m.r16s(sample + 6);
    m.w32(context + kOffset, 0); m.w32(context + kCenter, m.r32(sample));
    m.w16(context + kHeight, size); m.w16(context + kWidth, size);
    if (size < 8) m.w32(context + kAngle, 0);
    particle_quad(context, packet); atlas_frame(m, packet, index, size);
    m.w32(packet + 4, 0x2E838383u);
    const u32 blend = m.r32(context + kBlend);
    m.w16(packet + 14, blend ? 0x4016 : 0x4056);
    m.w16(packet + 22, blend ? 0x17u | ((blend & 3u) << 5) : 0x17u);
    if (!(blend & 1u)) m.w32(packet + 40, 0xE1000220u);
    return link_sprite(m, context, sample, packet, (blend & 1u) ? 9 : 10);
}

u32 Game::particle_growing_puff(u32 context, u32 sample, u32 particle) {
    cover(0x80085C8Cu);
    Frame frame(*this, 0x18);
    const s32 age = m.r16s(particle), lifetime = m.r16s(particle + 2);
    const s32 progress = age * 4096 / (lifetime ? lifetime : 1);
    const u32 packet = m.r32(context + kPackets), variant = m.r32(context + kVariant);
    const s32 size = m.r16s(sample + 6);
    gte.mtc2(25, 6); gte.mtc2(9, 10); gte.mtc2(8, signed_word(progress)); gte.mtc2(26, signed_word(size));
    gte.mtc2(10, signed_word(variant ? 2 * size : size));
    m.w32(context + kAngle, m.r32(context + kAngle) + (signed_word(age) << ((variant + 5) & 31u)));
    gte.op(0x1A8003Eu);
    gte.ctc2(21, 60); gte.ctc2(22, 60); gte.ctc2(23, 60); gte.mtc2(6, 0x2E535353u);
    const s32 grownSize = static_cast<s32>(gte.mfc2(26)), index = static_cast<s32>(gte.mfc2(25));
    m.w16(context + kHeight, grownSize); m.w16(context + kWidth, grownSize);
    gte.op(0x780010u);
    const u32 blend = m.r32(context + kBlend);
    m.w16(packet + 22, (blend & 1u) ? 0x37u : 0x17u | ((blend & 3u) << 5));
    if (!(blend & 1u)) m.w32(packet + 40, 0xE1000220u);
    link_sprite(m, context, sample, packet, (blend & 1u) ? 9 : 10);
    if (grownSize < 8) m.w32(context + kAngle, 0);
    atlas_frame(m, packet, index, grownSize); m.w16(packet + 14, 0x40D6u);
    m.w32(packet + 4, gte.mfc2(22));
    m.w32(context + kOffset, 0); m.w32(context + kCenter, m.r32(sample));
    return particle_quad(context, packet);
}

u32 Game::particle_splash(u32 context, u32 sample, u32 particle) {
    cover(0x80085EACu);
    Frame frame(*this, 0x18);
    const s32 age = m.r16s(particle), lifetime = m.r16s(particle + 2);
    const u32 variant = m.r32(context + kVariant), packet = m.r32(context + kPackets);
    m.w16(packet + 14, ((variant + 260) << 6) | 0x16u);
    const s32 index = age * (variant == 3 ? 8 : 6) / (lifetime ? lifetime : 1);
    const u32 textureX = signed_word(index) << 5;
    const u32 textureY = static_cast<s32>(variant) < 2 ? 0u : variant == 3 ? 64u : 32u;
    m.w16(packet + 20, (textureY << 8) | (textureX + 31));
    m.w16(packet + 12, (textureY << 8) | textureX);
    m.w16(packet + 28, ((textureY + 31) << 8) | textureX);
    m.w16(packet + 36, ((textureY + 31) << 8) | (textureX + 31));
    const u32 blend = m.r32(context + kBlend);
    m.w32(packet + 4, m.r32(context + kColor));
    m.w16(packet + 22, (blend & 1u) ? 0x38u : 0x18u | ((blend & 3u) << 5));
    if (!(blend & 1u)) m.w32(packet + 40, 0xE1000220u);
    link_sprite(m, context, sample, packet, (blend & 1u) ? 9 : 10);
    const s32 width = m.r16s(sample + 6), height = textureY < 64 ? width + (width >> 1) : width;
    m.w16(context + kWidth, width); m.w16(context + kHeight, height);
    m.w16(context + kOffset, 0); m.w16(context + kOffset + 2, ((height >> 3) - height) >> 1);
    m.w32(context + kCenter, m.r32(sample));
    return particle_quad(context, packet);
}

u32 Game::particle_pulse(u32 context, u32 sample, u32 particle) {
    cover(0x800860A4u);
    Frame frame(*this, 0x20);
    const s32 age = m.r16s(particle), originalLifetime = m.r16s(particle + 2);
    const u32 packet = m.r32(context + kPackets);
    m.w16(context + 32, (age + originalLifetime) & 2);
    m.w16(context + 34, ((originalLifetime & 1) << 6) + 128);
    const s32 progress = age * 4096 / (originalLifetime ? originalLifetime : 1);
    const u32 lookup = signed_word(progress < 2048 ? progress : progress - 2048);
    const s32 cosine = m.r16s(kAngles + 2 + ((lookup << 1) & 0x3FFCu));
    const s32 envelope = progress < 2048 ? 4096 - cosine : cosine;
    const u32 color = m.r32(context + kColor);
    gte.mtc2(8, signed_word(4608 - (envelope >> 1))); gte.mtc2(6, color);
    gte.ctc2(21, (color & 255u) << 2); gte.ctc2(22, ((color >> 8) & 255u) << 2);
    gte.ctc2(23, (color >> 14) & 0x3FCu);
    const s32 size = m.r16s(sample + 6);
    const s32 width = progress < 2048 ? wrap::mul(size, wrap::add(progress, 682)) / 2730 : size;
    gte.op(0x780010u);
    s32 scaledHeight = wrap::mul(size, envelope + 682);
    if (scaledHeight < 0) scaledHeight = wrap::add(scaledHeight, 4095);
    const s32 height = scaledHeight >> 12;
    m.w16(context + kWidth, width); m.w16(context + kHeight, height);
    m.w16(context + kOffset + 2, ((scaledHeight >> 15) - height) >> 1);
    m.w16(context + kOffset, 0); m.w32(context + kCenter, m.r32(sample));
    m.w16(packet + 14, 0x40D6u); m.w16(packet + 22, 0x37u);
    const u32 shift = height < 17 ? 1u : 0u;
    const s32 textureX = m.r16s(context + 34);
    const s32 left = textureX >> shift, right = ((textureX + 64) >> shift) - 1;
    m.w8(packet + 21, 0); m.w8(packet + 13, 0);
    m.w8(packet + 37, (64 >> shift) - 1); m.w8(packet + 29, (64 >> shift) - 1);
    const bool mirrored = m.r16s(context + 32) == 0;
    m.w8(packet + 28, mirrored ? right : left); m.w8(packet + 12, mirrored ? right : left);
    m.w8(packet + 36, mirrored ? left : right); m.w8(packet + 20, mirrored ? left : right);
    m.w32(packet + 4, gte.mfc2(22));
    particle_quad(context, packet);
    return link_sprite(m, context, sample, packet, 9);
}
}
