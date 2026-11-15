#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u;
constexpr u32 kAdvancePhase = 0x04000000u;
}

u32 Game::o2_loading_progress(u32 packet) {
    cover(0x800FB330u);
    Frame frame(*this, 0x48);
    const u32 gradient = frame.at(16);
    for (u32 vertex = 0; vertex < 4; ++vertex) {
        m.w8(gradient + 16 + 4 * vertex, 229);
        m.w8(gradient + 17 + 4 * vertex, 229);
        m.w8(gradient + 18 + 4 * vertex, 213);
    }
    m.w8(gradient + 19, 0);
    auto fraction = [&](u32 scale, u32 zeroBreak, u32 overflowBreak) {
        const u32 menu = m.r32(kMenu);
        const u32 total = m.r32(menu + 0x1C8);
        const u32 numerator = wrap::mul(m.r32(menu + 0x1CC), scale);
        if (!total) unported(zeroBreak, "loading progress divides by zero");
        if (total == 0xFFFFFFFFu && numerator == 0x80000000u)
            unported(overflowBreak, "loading progress division overflows");
        return static_cast<s32>(wrap::div(numerator, total).lo);
    };
    s32 width = fraction(115, 0x800FB3C0u, 0x800FB3D8u);
    if (width >= 115) width = 114;
    else if (width < 0) width = 0;
    m.w16(gradient, 192); m.w16(gradient + 2, 420);
    m.w16(gradient + 4, 192 + width); m.w16(gradient + 6, 420);
    m.w16(gradient + 8, 192); m.w16(gradient + 10, 426);
    m.w16(gradient + 12, 192 + width); m.w16(gradient + 14, 426);
    packet = o2_poly_g4(gradient, packet, m.r32(m.r32(kMenu) + 8));
    for (u32 indicator = 0; indicator < 6; ++indicator)
        o2_flag(indicator + 1, static_cast<s32>(indicator) < fraction(7, 0x800FB480u, 0x800FB498u));
    return packet;
}

u32 Game::o2_result_screen() {
    cover(0x800FBD74u);
    Frame frame(*this, 32);
    const u32 scene = m.r32(g::kScene), flags = m.r32(scene + 0x96C);
    if ((flags & 8) || (!m.r8(scene + 0x957) && !(flags & 1))) return kAdvancePhase;

    u32 menu = m.r32(kMenu);
    const u32 ot = m.r32(menu + 4 * m.r32(scene + 4));
    m.w32(menu + 8, ot);
    clear_otag_r(ot, 16);
    const u32 parent = m.r32(scene + 8);
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (m.r32(parent) & 0x00FFFFFFu));
    m.w32(parent, (m.r32(parent) & 0xFF000000u) | ((ot + 60) & 0x00FFFFFFu));
    m.w32(menu + 0x274, m.r32(menu + 0x278 + 4 * m.r32(scene + 4)));
    o2_text_frame(ot);
    o2_text_font(7);
    o2_text_semi(0xFFFFFFFFu);
    o2_text_fold(1, 1, 1);
    o2_text_frame_mode(1);
    o2_text_rgb(24, 184, 208);
    if (m.r8(m.r32(g::kScene) + 0x957) == 1) {
        o2_text_at(202, 155);
        o2_text_printf(0x800CD678u, 0, 0, 0);
    }
    if (m.r32(m.r32(g::kScene) + 0x96C) & 1) {
        o2_text_at(233, 145);
        o2_text_printf(0x800CD688u, 0, 0, 0);
    }
    const u32 currentScene = m.r32(g::kScene);
    if ((m.r32(currentScene + 0x12C) & 16) && !(m.r32(currentScene + 0x128) & 0x2F))
        return kAdvancePhase;
    menu = m.r32(kMenu);
    const u32 timer = m.r32(menu + 0x50);
    if (static_cast<s32>(timer) <= 0) return kAdvancePhase;
    m.w32(menu + 0x50, timer - 1);
    const u32 packet = o2_screens_frame(m.r32(menu + 0x274));
    m.w32(m.r32(kMenu) + 0x274, packet);
    return 0;
}
}
