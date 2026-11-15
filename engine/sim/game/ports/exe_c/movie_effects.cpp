#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMoviePlayer = 0x800BD054u;
constexpr u32 kBank = 159, kEffect = 10, kEffectAge = 12, kCue = 16;
u32 cue(Memory& m, u32 player, u32 indices) {
    return player + 70 * m.r8(indices + kBank) + 14 * m.r8(indices + kCue);
}
s32 shortClamp(s32 value, s32 maximum) {
    value = static_cast<s16>(value);
    if (value > maximum) value = maximum;
    if (value < 0) value = 0;
    return value;
}
u32 gray(s32 value) {
    const u32 channel = static_cast<u32>(value);
    return (channel << 16) | (channel << 8) | channel;
}
void link(Memory& m, u32 packet, u32 head) {
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(head) & 0xFFFFFFu));
    m.w32(head, (m.r32(head) & 0xFF000000u) | (packet & 0xFFFFFFu));
}
struct Strip { u32 width, screenY, textureV; };
Strip strip(u32 index) {
    const u32 band = index / 4;
    if (band == 3) return {46, 310, 210};
    if (band == 4) return {24, 356, 0};
    return {70, 100 + 70 * band, 70 * band};
}
void texture(Memory& m, u32 player, u32 bank, u32 index, bool blend, u32& page, u32& clut) {
    if (bank > 1) return;
    page = (bank ? 142u : 140u) + (index >= 16 ? 16u : 0u) + (blend ? 32u : 0u);
    clut = (bank ? 18096u : 17968u) + (m.r8(player + 162 + 20 * bank + index) ? 64u : 0u);
}
}

u32 Game::movie_draw_bank(u32 player, u32 packet, u32 ot) {
    cover(0x80059488u);
    Frame frame(*this, 0x10);
    auto divide = [&](s32 n, s32 d, u32 zero, u32 overflow) {
        if (!d) unported(zero, "movie brightness division by zero");
        if (d == -1 && static_cast<u32>(n) == 0x80000000u) unported(overflow, "movie brightness overflow");
        return static_cast<s32>(wrap::div(n, d).lo);
    };
    s32 brightness = 128;
    if (m.r8(player + kEffect) == 1) {
        const u32 record = cue(m, player, m.r32(kMoviePlayer));
        const s32 duration = m.r16s(record + 22) - m.r16s(record + 20) - 1;
        brightness = shortClamp(wrap::add(128, divide(wrap::mul(-128, m.r32s(player + kEffectAge)), duration,
            0x80059528u, 0x80059540u)), 128);
    }
    if (m.r8(player + kEffect) == 8) {
        const u32 record = cue(m, player, player);
        s32 duration = m.r16s(record + 24), amplitude = m.r16s(record + 26);
        if (!duration) { duration = 16; amplitude = 8; }
        const s32 age = m.r32s(player + kEffectAge) < 2 * duration ? m.r16s(player + kEffectAge) : 0;
        const s32 quadratic = divide(wrap::mul(wrap::mul(age, age), -amplitude), duration * duration,
            0x80059640u, 0x80059658u);
        const s32 linear = divide(wrap::mul(age, 2 * amplitude), duration, 0x80059684u, 0x8005969Cu);
        brightness = shortClamp(wrap::add(wrap::add(quadratic, linear), 128), 255);
    }
    const u32 color = gray(brightness);
    u32 page = 0, clut = 0;
    for (u32 index = 0; index < 20; ++index, packet += 24) {
        const Strip band = strip(index);
        texture(m, player, m.r8(player + kBank), index, false, page, clut);
        const u32 textureU = (index % 4) * 64;
        m.w32(packet + 12, (band.screenY << 16) | (textureU | 32u));
        m.w32(packet + 20, (band.width << 16) | 64u);
        m.w32(packet + 8, color);
        m.w8(packet + 11, 0x64);
        m.w32(packet + 4, page);
        m.w32(packet + 16, (clut << 16) | (band.textureV << 8) | textureU);
        m.w8(packet + 7, 0xE1);
        m.w8(packet + 6, 0);
        m.w32(packet, m.r32(ot + 12) | 0x05000000u);
        m.w32(ot + 12, packet & 0xFFFFFFu);
    }
    return packet;
}

u32 Game::movie_draw_crossfade(u32 player, u32 packet, u32 ot) {
    cover(0x800598BCu);
    Frame frame(*this, 0x20);
    const u32 record = cue(m, player, m.r32(kMoviePlayer));
    const s32 duration = m.r16s(record + 22) - m.r16s(record + 20) - 1;
    const s32 numerator = static_cast<s32>(m.r32(player + kEffectAge) << 7);
    if (!duration) unported(0x80059940u, "movie crossfade division by zero");
    if (duration == -1 && static_cast<u32>(numerator) == 0x80000000u)
        unported(0x80059958u, "movie crossfade overflow");
    const s32 brightness = shortClamp(static_cast<s32>(wrap::div(numerator, duration).lo), 128);
    const u32 bank = m.r8(player + kBank) ^ 1u;
    u32 page = 0, clut = 0;
    for (u32 index = 0; index < 20; ++index, packet += 28) {
        const Strip band = strip(index);
        texture(m, player, bank, index, true, page, clut);
        const u32 textureU = (index % 4) * 64;
        m.w32(packet + 8, (band.screenY << 16) | (textureU | 32u));
        m.w32(packet + 4, gray(brightness) | 0x66000000u);
        m.w32(packet + 12, (clut << 16) | (band.textureV << 8) | textureU);
        m.w32(packet + 16, (band.width << 16) | 64u);
        m.w8(packet + 7, m.r8(packet + 7) | 2u);
        m.w32(packet, m.r32(ot + 8) | 0x04000000u);
        m.w32(ot + 8, packet & 0xFFFFFFu);
        m.w8(packet + 23, 1);
        m.w32(packet + 24, page | 0xE1000200u);
        link(m, packet + 20, ot + 8);
    }
    return packet;
}

u32 Game::movie_draw_fade(u32 player, u32 packet, u32 ot) {
    cover(0x80059BD8u);
    const u32 record = cue(m, player, player);
    const s32 start = static_cast<s16>(m.r16(record + 20) + m.r16(record + 24));
    const s32 end = m.r16s(record + 26) ? m.r16s(record + 26) : m.r16s(record + 22);
    const s32 duration = end - start;
    const u32 effect = m.r8(player + kEffect);
    s32 slope = 0, brightness = 0;
    if (effect == 2 || effect == 4) slope = 255;
    if (effect == 3 || effect == 5) { slope = -255; brightness = 255; }
    if (m.r32s(player + 4) >= start) {
        const s32 numerator = wrap::mul(m.r32s(player + kEffectAge), slope);
        if (!duration) unported(0x80059CE8u, "movie fade division by zero");
        if (duration == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(0x80059D00u, "movie fade overflow");
        brightness = wrap::add(brightness, static_cast<s32>(wrap::div(numerator, duration).lo));
    }
    brightness = shortClamp(brightness, 255);
    m.w32(packet + 4, gray(brightness) | 0x62000000u);
    m.w32(packet + 8, 0x00640020u);
    m.w32(packet + 12, 0x01180100u);
    m.w32(packet, m.r32(ot + 4) | 0x03000000u);
    m.w32(ot + 4, packet & 0xFFFFFFu);
    const u32 blend = effect >= 2 && effect < 4 ? 32u : effect >= 4 && effect < 6 ? 64u : 0u;
    m.w8(packet + 19, 1);
    m.w32(packet + 20, blend | 0xE1000200u);
    link(m, packet + 16, ot + 4);
    return packet + 24;
}

u32 Game::movie_draw_zoom(u32 player, u32 packet, u32 ot) {
    cover(0x80059E1Cu);
    const u32 bank = m.r8(player + kBank), record = cue(m, player, player);
    const s32 scale = m.r16s(record + 28);
    if (!scale) { unported(0x80059E60u, "movie zoom division by zero"); unported(0x80059E94u, "movie zoom division by zero"); }
    const s16 width = static_cast<s16>(wrap::div(256, scale).lo), height = static_cast<s16>(wrap::div(280, scale).lo);
    const u32 u = static_cast<u8>(m.r8(record + 24) - width / 2), v = static_cast<u8>(m.r8(record + 26) - height / 2);
    const u32 page = bank == 0 ? 140u : bank == 1 ? 142u : 0u;
    const u32 clut = bank == 0 ? 17968u : bank == 1 ? 18096u : 0u;
    const u32 right = u + static_cast<u32>(width) - 1u, bottom = (v + static_cast<u32>(height) - 1u) << 8;
    m.w32(packet + 4, 0x00808080u);
    m.w32(packet + 8, 0x00640020u);
    m.w32(packet + 12, (clut << 16) | (v << 8) | u);
    m.w32(packet + 16, 0x00640120u);
    m.w32(packet + 20, (page << 16) | (v << 8) | right);
    m.w32(packet + 24, 0x017C0020u);
    m.w32(packet + 28, bottom | u);
    m.w32(packet + 32, 0x017C0120u);
    m.w32(packet + 36, bottom | right);
    m.w8(packet + 3, 9);
    m.w8(packet + 7, 0x2C);
    link(m, packet, ot + 12);
    return packet + 40;
}

u32 Game::movie_draw_shrink(u32 player, u32 packet, u32 ot) {
    cover(0x8005A034u);
    Frame frame(*this, 0x38);
    const u32 age = m.r32(player + kEffectAge);
    s16 screenY = 100, extent;
    u8 textureV = static_cast<u8>(4 * age), textureHeight;
    const bool split = textureV + static_cast<s16>(280 - 8 * age) >= 256;
    if (split) { extent = static_cast<s16>(4 * age + 256); textureHeight = static_cast<u8>(~(4 * age)); }
    else { extent = 280; textureHeight = static_cast<u8>(24 - 8 * age); }
    const u32 textureU = static_cast<u8>(3 * age), right = textureU + static_cast<u8>(~(6 * age));
    u32 page = 0, clut = 0;
    for (u32 part = 0; part < (split ? 2u : 1u); ++part, packet += 40) {
        if (part == 1) {
            textureV = 0;
            const u32 offset = 4 * m.r32(player + kEffectAge);
            screenY = static_cast<s16>(offset + 356);
            extent = static_cast<s16>(24 - offset);
            textureHeight = static_cast<u8>(24 - offset);
        }
        const u32 bank = m.r8(player + kBank);
        if (bank <= 1) { page = (bank ? 142u : 140u) + (part ? 16u : 0u); clut = bank ? 18096u : 17968u; }
        const u32 top = static_cast<u32>(screenY) << 16;
        const u32 bottom = static_cast<u32>(static_cast<s32>(screenY) + extent) << 16;
        m.w32(packet + 8, top | 32u);
        m.w32(packet + 12, (clut << 16) | (static_cast<u32>(textureV) << 8) | textureU);
        m.w32(packet + 16, top | 288u);
        m.w32(packet + 20, (page << 16) | (static_cast<u32>(textureV) << 8) | right);
        m.w32(packet + 24, bottom | 32u);
        m.w32(packet + 28, (static_cast<u32>(textureV + textureHeight) << 8) | textureU);
        m.w32(packet + 32, bottom | 288u);
        m.w32(packet + 36, (static_cast<u32>(textureV + textureHeight) << 8) | right);
        m.w8(packet + 3, 9);
        m.w32(packet + 4, 0x00808080u);
        m.w8(packet + 7, 0x2C);
        link(m, packet, ot + 12);
    }
    return packet;
}
}
