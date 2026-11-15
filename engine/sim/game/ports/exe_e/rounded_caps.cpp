#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCircle = 0x800B7104u;
constexpr u32 kWidth = 0, kColour = 4, kCode = 8, kCentre = 12, kTangent0 = 20, kTangent1 = 28;
void cap_rotation(Gte& gte) {
    const u32 sine = gte.mfc2(25), cosine = gte.mfc2(26) & 0xFFFFu;
    gte.ctc2(0, cosine | ((0u - sine) << 16));
    gte.ctc2(2, cosine);
    gte.ctc2(1, sine << 16);
    gte.ctc2(3, 0);
    gte.ctc2(4, 4096);
}
void cap_quad(Memory& m, u32 packet, u32 colour, u32 centre, u32 a, u32 b, u32 c) {
    m.w32(packet, ((packet + 36) & 0xFFFFFFu) | 0x08000000u);
    m.w32(packet + 8, a);
    m.w32(packet + 16, centre);
    m.w32(packet + 12, colour);
    m.w32(packet + 4, 0x3A020202u);
    m.w32(packet + 20, 0x3A020202u);
    m.w32(packet + 28, 0x3A020202u);
    m.w32(packet + 24, b);
    m.w32(packet + 32, c);
}
}

u32 Game::effect_round_disc(u32 packet, u32 work, u32 endpoint) {
    cover(0x80091A70u);
    Frame frame(*this, 56);
    const u32 width = m.r32(endpoint + kWidth), centre = m.r32(endpoint + kCentre);
    if (!width) return packet;
    gte.ctc2(5, static_cast<u32>(static_cast<s16>(centre)));
    gte.ctc2(6, static_cast<u32>(static_cast<s32>(centre) >> 16));
    const u32 angle = m.r32(kCircle + 4 * (m.r32(work + 32) & 0xFFFu));
    gte.mtc2(8, width);
    gte.mtc2(9, static_cast<u32>(static_cast<s16>(angle)));
    gte.mtc2(10, static_cast<u32>(static_cast<s32>(angle) >> 16));
    m.w32(work + 40, 0x10000000u);
    gte.op(0x198003Du);
    const u32 outside = screen_outcode(endpoint + kCentre);
    cap_rotation(gte);
    m.w32(work + 44, 0x0EC8061Fu);
    m.w32(work + 48, 0x0B500B50u);
    for (u32 wedge = 0; wedge < 8; ++wedge) {
        gte.mtc2(0, m.r32(work + 40));
        gte.mtc2(2, m.r32(work + 44));
        gte.mtc2(4, m.r32(work + 48));
        const u32 colour = m.r32(endpoint + kColour), origin = m.r32(endpoint + kCentre);
        gte.op(0x280030u);
        m.w32(packet + 20, 0x00030303u);
        m.w32(packet + 28, 0x00030303u);
        m.w32(packet + 4, 0x3A030303u);
        m.w32(packet + 12, colour);
        m.w32(packet + 16, origin);
        if (wedge & 1u) {
            const u32 angle_bytes = 1024 * (wedge + 1);
            m.w32(work + 40, m.r32(kCircle + angle_bytes));
            m.w32(work + 44, m.r32(kCircle + angle_bytes + 1024));
            m.w32(work + 48, m.r32(kCircle + angle_bytes + 2048));
        } else {
            for (u32 i = 0; i < 3; ++i) m.w16(work + 40 + 4 * i, 0u - m.r16(work + 40 + 4 * i));
        }
        bool visible = true;
        if (outside) {
            for (u32 i = 0; i < 3; ++i) m.w32(work + 52 + 4 * i, gte.mfc2(12 + i));
            u32 shared_edge = outside;
            for (u32 i = 0; i < 3 && shared_edge; ++i) shared_edge &= screen_outcode(work + 52 + 4 * i);
            visible = !shared_edge;
            if (visible) for (u32 i = 0; i < 3; ++i) m.w32(packet + (i ? 16 + 8 * i : 8), m.r32(work + 52 + 4 * i));
        } else {
            for (u32 i = 0; i < 3; ++i) m.w32(packet + (i ? 16 + 8 * i : 8), gte.mfc2(12 + i));
        }
        if (visible) {
            m.w32(packet, ((packet + 36) & 0xFFFFFFu) | 0x08000000u);
            packet += 36;
        }
    }
    return packet;
}

u32 Game::effect_round_end(u32 packet, u32 work, u32 endpoint) {
    cover(0x800916A8u);
    Frame frame(*this, 48);
    const u32 width = m.r32(endpoint), centre = m.r32(endpoint + kCentre);
    if (!width) {
        m.w32(endpoint + kCode, screen_outcode(endpoint + kCentre));
        m.w32(endpoint + 24, m.r32(endpoint + kCode));
        m.w32(endpoint + 28, m.r32(endpoint + kCentre));
        m.w32(endpoint + 16, m.r32(endpoint + 24));
        m.w32(endpoint + 20, m.r32(endpoint + 28));
        return packet;
    }
    gte.ctc2(5, static_cast<u32>(static_cast<s16>(centre)));
    gte.ctc2(6, static_cast<u32>(static_cast<s32>(centre) >> 16));
    const u32 angle = m.r32(kCircle + 4 * (m.r32(work + 32) & 0xFFFu));
    gte.mtc2(8, width);
    gte.mtc2(9, static_cast<u32>(static_cast<s16>(angle)));
    gte.mtc2(10, static_cast<u32>(static_cast<s32>(angle) >> 16));
    gte.op(0x198003Du);
    u32 outside = screen_outcode(endpoint + kCentre);
    u32 clip_mask = gte.mfc2(25);
    cap_rotation(gte);
    gte.mtc2(0, 0x10000000u);
    m.w32(endpoint + kCode, outside);
    const s32 sweep = m.r32s(work + 36);
    gte.op(0x180001u);
    const s32 twice_sweep = wrap::add(sweep, sweep);
    auto third = [](s32 value) {
        return wrap::sub(static_cast<s32>(wrap::mult(value, 0x55555556).hi), value >> 31);
    };
    m.w32(work + 44, m.r32(kCircle + 4 * (static_cast<u32>(third(twice_sweep)) & 0xFFFu)));
    m.w32(work + 48, m.r32(kCircle + 4 * (static_cast<u32>(sweep) & 0xFFFu)));
    m.w32(work + 40, m.r32(kCircle + 4 * (static_cast<u32>(third(sweep)) & 0xFFFu)));
    m.w32(work + 52, gte.mfc2(14));
    for (u32 i = 0; i < 3; ++i) gte.mtc2(2 * i, m.r32(work + 40 + 4 * i));
    clip_mask = static_cast<u32>(m.r16s(work + 44));
    gte.op(0x280030u);
    for (u32 i = 0; i < 3; ++i) m.w16(work + 40 + 4 * i, 0u - m.r16(work + 40 + 4 * i));
    if (outside) m.w32(work + 64, screen_outcode(work + 52));
    m.w32(work + 56, gte.mfc2(12));
    m.w32(work + 60, gte.mfc2(13));
    m.w32(endpoint + kTangent0, gte.mfc2(14));
    for (u32 i = 0; i < 3; ++i) gte.mtc2(2 * i, m.r32(work + 40 + 4 * i));
    u32 tangent_code = screen_outcode(endpoint + kTangent0);
    gte.op(0x280030u);
    m.w32(endpoint + 16, tangent_code);
    if (outside) {
        clip_mask = screen_outcode(work + 56);
        m.w32(work + 68, clip_mask);
        clip_mask &= outside & tangent_code;
        if (clip_mask) clip_mask &= screen_outcode(work + 60);
    }
    auto emit = [&](u32 first, u32 second, u32 third_point) {
        cap_quad(m, packet, m.r32(endpoint + kColour), m.r32(endpoint + kCentre), first, second, third_point);
        packet += 36;
    };
    if (!(outside & clip_mask)) {
        emit(m.r32(endpoint + kTangent0), m.r32(work + 60), m.r32(work + 56));
        tangent_code = m.r32(work + 60);
        clip_mask = m.r32(work + 56);
    }
    m.w32(work + 60, gte.mfc2(12));
    u32 shared = outside & tangent_code;
    if (outside) {
        outside &= screen_outcode(work + 60);
        tangent_code = m.r32(work + 64);
        clip_mask = m.r32(work + 68);
        shared = outside & tangent_code;
    }
    if (!(shared & clip_mask)) {
        emit(m.r32(work + 56), m.r32(work + 52), m.r32(work + 60));
    }
    m.w32(work + 56, gte.mfc2(13));
    m.w32(endpoint + kTangent1, gte.mfc2(14));
    const u32 last_code = screen_outcode(endpoint + kTangent1);
    outside &= last_code;
    m.w32(endpoint + 24, last_code);
    if (!outside || !(outside & screen_outcode(work + 56)))
        emit(m.r32(endpoint + kTangent1), m.r32(work + 56), m.r32(work + 60));
    return packet;
}
}
