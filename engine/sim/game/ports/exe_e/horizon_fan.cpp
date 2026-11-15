#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::draw_horizon_fan(u32 context) {
    cover(0x8008C9D4u);
    Frame frame(*this, 56);
    constexpr u32 kSavedView = 16, kRotation = 48, kScale = 80, kCentre = 128, kRim = 136;
    const u32 descriptor = m.r32(m.r32(g::kWorld) + 580), colour = m.r32(descriptor + 112);
    if (!(m.r32(m.r32(0x800BCF90u) + 1480) & 4) || !colour) return descriptor;
    for (u32 reg = 0; reg < 8; ++reg) m.w32(context + kSavedView + 4 * reg, gte.cfc2(reg));
    m.w32(context + kCentre, gte.cfc2(8)); m.w32(context + kCentre + 4, gte.cfc2(9));
    m.w32(context + kCentre, surface_angles(context + kCentre)); m.w16(context + kCentre + 4, 0);
    rot_matrix_yxz_gte(context + kCentre, context + kRotation);
    m.w32(context + kScale, 4096); m.w32(context + kScale + 4, 0);
    m.w32(context + kScale + 8, 2457); m.w32(context + kScale + 12, 0);
    m.w32(context + kScale + 16, 3072);
    mul_matrix2(context + kScale, context + kRotation);
    m.w16(context + kCentre, 0); m.w16(context + kCentre + 2, 0); m.w16(context + kCentre + 4, 240);
    apply_matrix_sv(context + kRotation, context + kCentre, context + kCentre);
    for (u32 vertex = 0; vertex < 16; ++vertex) {
        const u32 point = context + kRim + 8 * vertex, trig = 0x800B7104u + 1024 * vertex;
        m.w16(point, m.r16s(trig) / 64); m.w16(point + 2, (96 * m.r16s(trig + 2)) / 4096);
        m.w16(point + 4, 220);
        apply_matrix_sv(context + kRotation, point, point);
    }
    for (u32 reg = 0; reg < 5; ++reg) gte.ctc2(reg, m.r32(context + kSavedView + 4 * reg));
    gte.mtc2(0, m.r32(context + kCentre)); gte.mtc2(1, m.r32(context + kCentre + 4));
    const u32 mask = m.r32(context + 4), anchor = m.r32(context + 1016);
    m.w32(mask + 4, 0xE6000000u); gte.op(0x180001u);
    m.w32(mask, m.r32(anchor + 76) | 0x01000000u); m.w32(anchor + 76, mask & 0xFFFFFFu);
    const u32 first = mask + 8;
    m.w32(first, 0x01000000u | ((first + 8) & 0xFFFFFFu)); m.w32(first + 4, 0xE6000002u);
    const u32 centre = gte.mfc2(14);
    m.w32(context + kCentre, centre);
    for (u32 vertex = 0; vertex < 16; ++vertex) {
        const u32 point = context + kRim + 8 * vertex;
        gte.mtc2(0, m.r32(point)); gte.mtc2(1, m.r32(point + 4)); gte.op(0x180001u);
        m.w32(point - 4, screen_outcode(point - 8)); m.w32(point, gte.mfc2(14));
    }
    m.w32(context + 260, screen_outcode(context + 256));
    m.w32(context + 264, m.r32(context + kRim)); m.w32(context + 268, m.r32(context + kRim + 4));
    gte.mtc2(12, centre);
    u32 packet = first + 8, previous = m.r32(context + kRim), previous_code = m.r32(context + kRim + 4);
    const u32 centre_code = m.r32(context + kCentre + 4);
    for (u32 vertex = 0; vertex < 16; ++vertex) {
        const u32 point = context + kRim + 8 * (vertex + 1), next = m.r32(point), next_code = m.r32(point + 4);
        if (!(centre_code & previous_code & next_code)) {
            gte.mtc2(13, previous); gte.mtc2(14, next);
            m.w32(packet + 8, previous); gte.op(0x1400006u);
            m.w32(packet + 16, next); m.w32(packet + 24, centre);
            m.w32(packet + 12, 0x00030303u); m.w32(packet + 20, colour);
            if (static_cast<s32>(gte.mfc2(24)) > 0) {
                m.w32(packet, 0x06000000u | ((packet + 28) & 0xFFFFFFu));
                m.w32(packet + 4, 0x32030303u); packet += 28;
            }
        }
        previous = next; previous_code = next_code;
    }
    m.w32(packet, 0x01000000u); m.w32(packet + 4, 0xE6000000u);
    for (u32 byte = 0; byte < 3; ++byte) {
        m.w8(packet + byte, m.r8(anchor - 76 + byte)); m.w8(anchor - 76 + byte, first >> (8 * byte));
    }
    m.w32(context + 4, packet + 8);
    return packet + 8;
}
}
