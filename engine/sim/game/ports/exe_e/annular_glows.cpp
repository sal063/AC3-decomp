#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCircle = 0x800B7104u;
void save_view(Memory& memory, Gte& gte, u32 work) {
    for (u32 component = 0; component < 8; ++component) memory.w32(work + 4 * component, gte.cfc2(component));
}
void restore_view(Memory& memory, Gte& gte, u32 work) {
    for (u32 component = 0; component < 8; ++component) gte.ctc2(component, memory.r32(work + 4 * component));
}
void circle_vertex(Memory& memory, u32 vertex, u32 phase) {
    const u32 circle = memory.r32(kCircle + 4 * (phase & 4095));
    memory.w16(vertex + 4, circle >> 16);
    memory.w16(vertex, circle);
}
}

u32 Game::annular_glow_setup(u32 work, u32 shift, s32 focal) {
    cover(0x8008CD80u);
    Frame frame(*this, 40);
    const u32 x = wrap::shl(static_cast<u32>(m.r16s(work + 32)), shift);
    const u32 y = wrap::shl(static_cast<u32>(m.r16s(work + 34)), shift);
    const u32 z = wrap::shl(static_cast<u32>(m.r16s(work + 36)), shift);
    gte.mtc2(0, (x & 65535) | (y << 16)); gte.mtc2(1, z);
    const u32 phase = m.r32(work + 44);
    const u32 radius = m.r32(work + 40);
    gte.op(0x180001u);
    const u32 circle = m.r32(kCircle + 4 * (phase & 4095));
    const s32 scaled_radius = static_cast<s32>(wrap::shl(radius, shift));
    const s32 sine = wrap::mul(static_cast<s16>(circle), scaled_radius) >> 12;
    const s32 cosine = wrap::mul(static_cast<s16>(circle >> 16), scaled_radius) >> 12;
    const s32 projected_radius = wrap::mul(focal, scaled_radius);
    m.w16(work + 56, scaled_radius);
    const s32 depth = static_cast<s32>(gte.mfc2(27));
    const s32 bucket_depth = wrap::sar(depth, shift);
    if (static_cast<u32>(bucket_depth) - 31u >= 0x77D9u) return 0;
    if (!depth) unported(0x8008CE58u, "annular glow has zero depth");
    if (depth == -1 && projected_radius == static_cast<s32>(0x80000000u))
        unported(0x8008CE70u, "annular glow division overflow");
    const s32 size = static_cast<s32>(wrap::div(projected_radius, depth).lo);
    if (size < 3) return 0;
    m.w32(work + 84, gte.mfc2(14)); m.w32(work + 80, bucket_depth >> 3);
    m.w16(work + 52, sine); m.w16(work + 60, wrap::neg(sine));
    m.w16(work + 64, cosine); m.w16(work + 48, cosine);
    m.w16(work + 62, 0); m.w16(work + 58, 0);
    m.w16(work + 54, 0); m.w16(work + 50, 0);
    const u32 centre_x = gte.mfc2(25), centre_y = gte.mfc2(26);
    particle_compose_rotation(work + 48);
    gte.ctc2(5, centre_x); gte.ctc2(6, centre_y); gte.ctc2(7, static_cast<u32>(depth));
    return static_cast<u32>(size);
}

u32 Game::annular_glow_draw(u32 context, u32 work, s32 segments, s32 offset, u32 unchanged_result) {
    cover(0x8008CEFCu);
    Frame frame(*this, 24);
    if (segments <= 0) return unchanged_result;
    const u32 start = m.r32(context + 4);
    u32 packet = start + 8;
    m.w32(start + 4, 0xE1000240u);
    m.w32(start, (packet & 0x00FFFFFFu) | 0x01000000u);
    const u32 centre = m.r32(work + 84);
    m.w32(work + 88, 0); m.w16(work + 92, 4096);
    const u32 half_circle = m.r32(kCircle + ((static_cast<u32>(offset) << 1) & 0x3FFCu));
    m.w16(work + 98, 64); m.w16(work + 100, half_circle >> 16); m.w16(work + 96, half_circle);
    const u32 full_circle = m.r32(kCircle + 4 * (static_cast<u32>(offset) & 4095));
    m.w16(work + 106, 150); m.w16(work + 108, full_circle >> 16); m.w16(work + 104, full_circle);
    const u32 step = static_cast<u32>(4096 / segments);
    u32 phase = 0;
    for (s32 segment = 0; segment < segments; ++segment) {
        for (u32 vertex = 0; vertex < 3; ++vertex) {
            gte.mtc2(2 * vertex, m.r32(work + 88 + 8 * vertex));
            gte.mtc2(2 * vertex + 1, m.r32(work + 92 + 8 * vertex));
        }
        phase += step;
        m.w32(packet + 24, centre);
        gte.op(0x280030u);
        circle_vertex(m, work + 88, phase);
        circle_vertex(m, work + 96, phase + static_cast<u32>(offset >> 1));
        circle_vertex(m, work + 104, phase + static_cast<u32>(offset));
        m.w32(packet, ((packet + 36) & 0x00FFFFFFu) | 0x08000000u);
        m.w32(packet + 4, 0x3A242828u); m.w32(packet + 12, 0x001C1F1Fu);
        m.w32(packet + 20, 0x00050707u); m.w32(packet + 28, 0x00010303u);
        m.w32(packet + 8, gte.mfc2(12)); m.w32(packet + 16, gte.mfc2(13));
        m.w32(packet + 32, gte.mfc2(14));
        packet += 36;
    }
    m.w32(packet, 0x01000000u); m.w32(packet + 4, 0xE1000220u);
    const u32 bucket = m.r32(context) + 4 * m.r32(work + 80);
    const u32 link = static_cast<u32>(m.r8(bucket)) | (static_cast<u32>(m.r8(bucket + 1)) << 8) | (static_cast<u32>(m.r8(bucket + 2)) << 16);
    for (u32 byte = 0; byte < 3; ++byte) m.w8(packet + byte, link >> (8 * byte));
    const u32 head = m.r32(context + 4);
    for (u32 byte = 0; byte < 3; ++byte) m.w8(bucket + byte, head >> (8 * byte));
    m.w32(context + 4, packet + 8);
    return packet + 8;
}

u32 Game::draw_central_annular_glow(u32 context) {
    cover(0x8008D140u);
    Frame frame(*this, 32);
    const u32 work = context + 16, tick = m.r32(m.r32(g::kScene) + 40);
    save_view(m, gte, work);
    m.w16(work + 34, static_cast<u32>(-1)); m.w16(work + 36, 4); m.w16(work + 32, 0);
    m.w32(work + 40, 40); m.w32(work + 44, tick * 513u);
    const u32 visible = annular_glow_setup(work, m.r32(context + 12), static_cast<s32>(gte.cfc2(26)));
    if (!visible) return 0;
    const u32 next = annular_glow_draw(context, work, 3, 341);
    restore_view(m, gte, work);
    return next;
}

u32 Game::draw_small_annular_glow(u32 context) {
    cover(0x8008D244u);
    Frame frame(*this, 32);
    const u32 work = context + 16, tick = m.r32(m.r32(g::kScene) + 40);
    save_view(m, gte, work);
    m.w16(work + 34, static_cast<u32>(-1)); m.w16(work + 36, 2); m.w16(work + 32, 0);
    m.w32(work + 40, 36); m.w32(work + 44, tick * 513u);
    const u32 visible = annular_glow_setup(work, m.r32(context + 12), static_cast<s32>(gte.cfc2(26)));
    if (!visible) return 0;
    const u32 next = annular_glow_draw(context, work, 5, 318);
    restore_view(m, gte, work);
    return next;
}

u32 Game::draw_paired_annular_glows(u32 context) {
    cover(0x8008D348u);
    Frame frame(*this, 40);
    const u32 work = context + 16, tick = m.r32(m.r32(g::kScene) + 40);
    const s32 focal = static_cast<s32>(gte.cfc2(26));
    const u32 shift = m.r32(context + 12);
    save_view(m, gte, work);
    m.w16(work + 32, 40); m.w16(work + 34, static_cast<u32>(-15)); m.w16(work + 36, 0);
    m.w32(work + 40, 37); m.w32(work + 44, tick * 511u);
    if (!annular_glow_setup(work, shift, focal)) return 0;
    annular_glow_draw(context, work, 3, -318);
    restore_view(m, gte, work);
    const u32 opposite_phase = 1000u - m.r32(work + 44);
    m.w16(work + 32, 0u - m.r16(work + 32)); m.w32(work + 44, opposite_phase);
    if (!annular_glow_setup(work, shift, focal)) return 0;
    const u32 next = annular_glow_draw(context, work, 3, 318);
    restore_view(m, gte, work);
    return next;
}
}
