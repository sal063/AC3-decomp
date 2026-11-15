#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kScreens = 0x800DD9DCu;
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kCards = 0x800DDA48u;
constexpr u32 kScratchPtr = 0x800DD9D8u;
constexpr u32 kAnim = 0x800DDA68u;
constexpr u32 kFlow = 0x800DD910u;
constexpr u32 kKcd = 0x800BCBBCu;

constexpr u32 kEvSwCard = 0xF4000001u, kEvHwCard = 0xF0000011u;
constexpr u32 kEvMdNoIntr = 0x2000u;

u32 scene(Memory& m) { return m.r32(g::kScene); }
}

u32 Game::t1_phase3() {
    cover(0x800CC764u);
    Frame f(*this, 0x20);
    {
        const u32 st = m.r32(kState);
        m.w32(st + 0x44, m.r32(st + 0x44) + 1);
        m.w32(st + 8, m.r32(st + 4 * m.r32(scene(m) + 4)));
        t1_screens_frame(m.r32(scene(m) + 8) + 4);
    }
    cover(0x800DA6FCu);
    t1_text_frame(m.r32(m.r32(kScreens) + 0x10) + 4);
    t1_prim_origin(0x32, 0x12C);
    t1_prim_rgb(0x40, 0x40, 0x40);
    if (m.r32(m.r32(kState) + 0x18)) t1_card_poll();
    t1_screens_run();
    {
        const u32 st = m.r32(kState);
        const u32 tbl = m.r32(0x800DCB10u + 4u * static_cast<u32>(m.r16s(st + 0xC)));
        if (tbl) t1_state(m.r32(tbl + 4u * static_cast<u32>(m.r16s(st + 0xE))));
    }
    const u32 st = m.r32(kState);
    if (m.r32s(st + 0x78) >= 0x259) {
        if (m.r32(st + 0x18)) t1_events_close();
        const u32 sc = scene(m);
        m.w32(sc + 0x3A0, 4);
        m.w8(sc + 0x3A8, 0);
        return 0x60000005u;
    }
    if (!m.r8(st + 0x58)) return 0;
    cd_reset(0, 0);
    if (m.r32(m.r32(kState) + 0x18)) t1_events_close();
    const u32 sc = scene(m);
    m.w16(sc + 0x954, (m.r32(sc + 0x96C) & 0x10u) ? 0u : 2u);
    const u32 d = m.r32(kDisplay);
    m.w32(d, m.r32(d) | 4u);
    return 0x60000003u;
}

void Game::t1_state(u32 fn) {
    switch (fn) {
    case 0x800CCC90u: t1_title_init(); return;
    case 0x800CCD48u: t1_title_run(); return;
    case 0x800CD9A8u: t1_menu_init(); return;
    case 0x800CDB64u: t1_menu_run(); return;
    case 0x800D1798u: t1_new_init(); return;
    case 0x800D181Cu: t1_new_run(); return;
    case 0x800D75E4u: t1_exit_init(); return;
    case 0x800D7600u: t1_exit_run(); return;
    default:
        call_by_address(fn, "a state of overlay 1's account screen");
    }
}

void Game::t1_title_init() {
    cover(0x800CCC90u);
    Frame f(*this, 0x20);
    {
        const u32 st = m.r32(kState);
        m.w32(st + 0x48, 0);
        m.w16(st + 0x10, 0);
        m.w16(st + 0xE, 1);
    }
    t1_set_1c(1);
    const u32 d = m.r32(kDisplay);
    const u32 flags = m.r32(d);
    m.w8(d + 0x10, 0xE5);
    m.w32(d, flags | 8u);
    m.w8(m.r32(kDisplay) + 0x11, 0xE4);
    m.w8(m.r32(kDisplay) + 0x12, 0xDE);
    t1_ranks(m.r32(kState) + 0x94);
    for (u32 i = 0; i < 52; ++i) t1_flag(i + 1, m.r8(m.r32(kState) + i + 0x94));
}

void Game::t1_ranks(u32 out) {
    cover(0x800CC938u);
    Frame f(*this, 0x18);
    for (s32 i = 5; i >= 0; --i) m.w32(f.at(4u * static_cast<u32>(i)), 0);
    for (u32 k = 1; k < 56; ++k) {
        if (k == 19 || k == 49 || k == 54) continue;
        const u32 a = scene(m) + k;
        u32 v = 5;
        if (m.r8(a + 0xC1C)) v = m.r8(a + 0xC1C);
        const u32 b = m.r8(a + 0xC58);
        if (b && b < v) v = b;
        const u32 c = m.r8(a + 0xC94);
        if (c && c < v) v = c;
        const u32 slot = f.at(4 * v);
        m.w32(slot, m.r32(slot) + 1);
    }
    u32 n = 0;
    for (u32 r = 1; r <= 5; ++r)
        for (s32 i = 0; i < m.r32s(f.at(4 * r)); ++i) m.w8(out + n++, r);
    for (; n < 52; ++n) m.w8(out + n, 5);
}

void Game::t1_title_run() {
    cover(0x800CCD48u);
    Frame f(*this, 0x20);
    t1_press_start(m.r32(scene(m) + 8), m.r32(m.r32(kState) + 8));
    if (!(m.r32(scene(m) + 0x12C) & 0x40u)) {
        const u32 st = m.r32(kState);
        m.w32(st + 0x78, m.r32(st + 0x78) + 1);
        return;
    }
    {
        const u32 st = m.r32(kState);
        m.w16(st + 0xC, 2);
        m.w16(st + 0xE, 0);
        m.w16(st + 0x10, 0);
    }
    const u32 d = m.r32(kDisplay);
    const u32 flags = m.r32(d);
    m.w8(d + 0x10, 0);
    m.w32(d, flags | 8u);
    m.w8(m.r32(kDisplay) + 0x11, 0x70);
    m.w8(m.r32(kDisplay) + 0x12, 0x78);
    sound_play(0x58);
    for (u32 i = 1; i < 56; ++i) t1_flag(i, 0);
    m.w32(m.r32(kState) + 0x78, 0);
}

u32 Game::t1_press_start(u32 ot, u32 p) {
    cover(0x800CCB38u);
    m.w8(p + 3, 3);
    m.w8(p + 7, 0x60);
    m.w16(p + 8, 0x6E);
    m.w16(p + 0xA, 0xE5);
    m.w16(p + 0xC, 8);
    m.w16(p + 0xE, 0x10);
    const s32 t = m.r32s(scene(m) + 0x20);
    s32 a = t - (t / 120) * 120 - 0x3C;
    if (a < 0) a = -a;
    m.w8(p + 4, static_cast<u32>(-29 * a / 60 + 0xE5));
    m.w8(p + 5, static_cast<u32>(-132 * a / 60 + 0xE4));
    m.w8(p + 6, static_cast<u32>(-198 * a / 60 + 0xDE));
    ot_link(ot, p);
    return p + 16;
}

void Game::t1_menu_init() {
    cover(0x800CD9A8u);
    Frame f(*this, 0x48);
    {
        const u32 st = m.r32(kState);
        m.w8(st + 0x5E, 0);
        m.w16(st + 0xE, 1);
        m.w32(st + 0x48, 0);
        if (!m.r32(st + 0x18)) {
            t1_events_open();
            t1_card_flags_reset();
        }
    }
    m.w32(m.r32(kState) + 0x18, 1);
    t1_fade_set(1, 1, 0, 0x3C);
    t1_fade_set(3, 1, 0, 0x3C);
    t1_fade_set(0xF, 1, 0, 0x3C);
    cover(0x800DA6FCu);
    const u32 ot = m.r32(m.r32(kScreens) + 0x10);
    const u32 d = f.at(0x10);
    const u32 xy[8] = {0x10A, 0x1C, 0x137, 0x1C, 0x10A, 0x94, 0x137, 0x94};
    for (u32 i = 0; i < 8; ++i) m.w16(d + 2 * i, xy[i]);
    for (u32 k = 0; k < 4; ++k) {
        m.w8(d + 0x10 + 4 * k, 0);
        m.w8(d + 0x11 + 4 * k, 0x70);
        m.w8(d + 0x12 + 4 * k, 0x78);
    }
    m.w8(d + 0x13, 0);
    m.w32(m.r32(kState) + 8, t1_poly_g4(d, m.r32(m.r32(kState) + 8), ot + 0x38));
    t1_flag(5, (m.r32(scene(m) + 0x124) & 0x10u) ? 1u : 0u);
    t1_set_1c(0xBE);
    t1_flag(4, 1);
    {
        const u32 st = m.r32(kState);
        m.w16(st + 0x7C, 0x83D);
        m.w16(st + 0x7E, 0);
        m.w16(st + 0x80, 0);
        m.w16(st + 0x82, 0);
        m.w32(st + 0x84, 0);
    }
    t1_anim_push(2);
    m.w8(m.r32(kState) + 0xD1, 1);
    t1_flag(0x10, 2);
}

void Game::t1_menu_run() {
    cover(0x800CDB64u);
    Frame f(*this, 0x80);
    m.w32(f.at(0x40), static_cast<u32>(t1_card_status(0)));
    const s32 c0 = m.r32s(f.at(0x40));
    m.w32(f.at(0x44), static_cast<u32>(t1_card_status(1)));
    const s32 c1 = m.r32s(f.at(0x44));
    cover(0x800DA6FCu);
    const u32 ot = m.r32(m.r32(kScreens) + 0x10);
    const u32 d = f.at(0x10);
    for (u32 k = 0; k < 3; ++k) {
        m.w8(d + 0x11 + 4 * k, 0x70);
        m.w8(d + 0x12 + 4 * k, 0x78);
    }
    m.w8(d + 0x13, 0);
    m.w8(d + 0x10, 0);
    m.w8(d + 0x14, 0);
    m.w8(d + 0x18, 0);
    m.w32(f.at(0x48), m.r32(scene(m) + 0x964) & 1u);
    const bool reopen = m.r32(f.at(0x48)) != 0;
    if (m.r8(m.r32(kState) + 0x5E) < 0x1E) {
        for (s32 r = 0; r < 5; ++r) {
            const s32 y = 100 - 18 * r;
            for (s32 c = 0; c < 5; ++c) {
                const s32 k = 2 * (c + r) + 0x1E - static_cast<s32>(m.r8(m.r32(kState) + 0x5E));
                if (k < 0) continue;
                m.w16(d, static_cast<u32>(9 * c - (k - 0x113)));
                m.w16(d + 2, static_cast<u32>(y));
                m.w16(d + 4, static_cast<u32>(0x113 + 9 * c));
                m.w16(d + 6, static_cast<u32>(y));
                m.w16(d + 8, static_cast<u32>(0x113 + 9 * c));
                m.w16(d + 0xA, static_cast<u32>(2 * k + 100 - 18 * r));
                m.w32(m.r32(kState) + 8, t1_poly_g3(d, m.r32(m.r32(kState) + 8), ot + 0x38));
            }
        }
        const u32 st = m.r32(kState);
        m.w8(st + 0x5E, m.r8(st + 0x5E) + 1);
    }
    {
        const s32 t = m.r32s(scene(m) + 0x20);
        const s32 a = (t - ((t < 0 ? t + 0x3F : t) >> 6 << 6)) << 6;
        m.w32(m.r32(kState) + 8, t1_logo(0x46, 0x19A, a, m.r32(m.r32(kState) + 8)));
    }
    const s32 fade = t1_fade_state(1);
    const u32 pad = m.r32(scene(m) + 0x12C);
    bool go_on = true;
    if ((pad & 0x10u) || fade == 0) {
        if (static_cast<u32>(fade - 1) < 2u) {
            t1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
            t1_fade_set(3, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
            t1_fade_set(8, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
            sound_play(0x59);
        } else {
            t1_fade_set(1, 0, 0, 0);
            t1_fade_set(3, 0, 0, 0);
            t1_fade_set(8, 0, 0, 0);
            const u32 st = m.r32(kState);
            if (m.r8(st + 0x59) == 2) {
                m.w16(st + 0xE, m.r16(st + 0xE) + 1);
            } else {
                const u32 item = m.r8(st + 0x59);
                m.w16(st + 0xE, 0);
                m.w16(st + 0x10, 0);
                m.w16(st + 0xC, item + 3);
            }
        }
    } else if (pad & 0x20u) {
        const u32 st = m.r32(kState);
        m.w16(st + 0xC, 1);
        m.w16(st + 0xE, 0);
        m.w32(st + 0x78, 0);
        sound_play(0x3B);
        cd_reset(0, 0);
        m.w32(m.r32(kState) + 0x18, 0);
        t1_events_close();
        t1_card_flags_reset();
        go_on = false;
    }
    if (!go_on) return;
    const u32 old = m.r8(m.r32(kState) + 0x59);
    const s32 f1 = t1_fade_state(1);
    auto item = [&]() { return m.r8(m.r32(kState) + 0x59); };
    auto set_item = [&](u32 v) { m.w8(m.r32(kState) + 0x59, v); };
    const bool dl = c0 == 5 || c1 == 5;
    if ((m.r32(scene(m) + 0x12C) & 2u) && f1 == 2) {
        set_item(item() < 2 ? item() + 1 : 0);
        if (item() == 1 && !dl) set_item(2);
        if (item() == 2 && !reopen) set_item(0);
    }
    if ((m.r32(scene(m) + 0x12C) & 1u) && f1 == 2) {
        set_item(item() ? item() - 1 : 2);
        if (item() == 2 && !reopen) set_item(1);
        if (item() == 1 && !dl) set_item(0);
    }
    if (item() != old) sound_play(0x3D);
    if (item() == 1 && !dl && t1_fade_state(1) == 2) set_item(0);
    t1_flag(6, item() ? 1u : 0u);
    t1_flag(7, dl ? (item() == 1 ? 0u : 1u) : 2u);
    t1_flag(8, reopen ? (item() == 2 ? 0u : 1u) : 2u);
    t1_flag(1, 1);
    {
        const u32 st = m.r32(kState);
        m.w32(st + 0x48, m.r32(st + 0x48) + 1);
    }
    t1_flag(5, (m.r32(scene(m) + 0x124) & 0x10u) ? 1u : 0u);
    t1_music();
}

void Game::t1_music() {
    if (!kcd_stream_on_this_disc(static_cast<u32>(m.r16s(m.r32(kState) + 0x7C)))) return;
    if (m.r16s(m.r32(kState) + 0xC) != 2) return;
    if (cd_pending(1)) return;
    cd_request_stream(m.r32(kState) + 0x7C, 1, 0);
}

u32 Game::cd_pending(u32 peek) {
    cover(0x8002C580u);
    Frame f(*this, 0x18);
    if (peek) return m.r32(m.r32(kKcd) + 4) - m.r32(m.r32(kKcd));
    while (m.r32(m.r32(kKcd) + 4) != m.r32(m.r32(kKcd))) {
        kcd_pump();
        hw->vsync(0);
    }
    return 0;
}

void Game::t1_exit_init() {
    cover(0x800D75E4u);
    const u32 st = m.r32(kState);
    m.w32(st + 0x48, 0);
    m.w16(st + 0x10, 0);
    m.w16(st + 0xE, 1);
}

void Game::t1_exit_run() {
    cover(0x800D7600u);
    Frame f(*this, 0x18);
    if (!t1_cards_idle()) return;
    t1_events_close();
    const u32 st = m.r32(kState);
    m.w32(st + 0x18, 0);
    m.w8(st + 0x58, 1);
}

bool Game::t1_cards_idle() {
    cover(0x800D3D60u);
    const u32 b = m.r32(kCards);
    m.w32(b + 0x28, 0);
    if (m.r8(b + 0x34C)) return false;
    return m.r8(b + 0x34D) == 0;
}

void Game::t1_new_init() {
    cover(0x800D1798u);
    Frame f(*this, 0x18);
    {
        const u32 st = m.r32(kState);
        m.w32(st + 0x48, 0);
        m.w16(st + 0x10, 0);
        m.w16(st + 0xE, 1);
    }
    const u32 a = mem_alloc(4);
    m.w32(0x800DDA30u, a);
    m.w8(a, 0);
    m.w8(m.r32(0x800DDA30u) + 1, 0);
    m.w8(m.r32(kState) + 0xD2, 0);
    {
        const u32 st = m.r32(kState);
        m.w16(st + 0x4C, 0);
        m.w16(st + 0x4E, 0);
    }
    t1_card_dir_reset();
    t1_card_states_reset();
    cover(0x800D5548u);
}

void Game::t1_new_run() {
    cover(0x800D181Cu);
    Frame f(*this, 0x18);
    if (disc_number() == 0) {
        const u32 sc = scene(m);
        m.w32(sc + 0x12C, 0);
        m.w32(sc + 0x128, 0);
    }
    {
        const s32 k = static_cast<s16>(m.r16(m.r32(kState) + 0x4C) - 1);
        s32 x = -1, y = 0;
        if (k == 0 || k == 2 || (k >= 6 && k <= 9)) x = 0xF9, y = 0x169;
        else if (k == 1 || k == 10) x = 0x46, y = 0x19A;
        else if (k == 3) x = 0x3A, y = 0x172;
        if (x >= 0) {
            const s32 t = m.r32s(scene(m) + 0x20);
            const s32 a = (t - ((t < 0 ? t + 0x3F : t) >> 6 << 6)) << 6;
            m.w32(m.r32(kState) + 8, t1_logo(x, y, a, m.r32(m.r32(kState) + 8)));
        }
    }
    {
        const u32 st = m.r32(kState);
        const u32 tbl = m.r32(0x800DD598u + 4u * static_cast<u32>(m.r16s(st + 0x4C)));
        t1_new_sub(m.r32(tbl + 4u * static_cast<u32>(m.r16s(st + 0x4E))));
    }
    t1_new_pilot();
    t1_new_flags(1, m.r8(m.r32(kState) + 0xD2));
    t1_flag(0x11, m.r8(m.r32(kState) + 0xD0));
    {
        const u32 st = m.r32(kState);
        t1_flag(0x10, m.r8(st + 0xD1) == 1 ? 2u : m.r8(st + 0x5A));
    }
    t1_flag(3, m.r32(scene(m) + 0x394));
    t1_flag(5, (m.r32(scene(m) + 0x124) & 0x10u) ? 1u : 0u);
    if (!cd_pending(1)) cd_request_stream(m.r32(kState) + 0x7C, 1, 0);
}

void Game::t1_flag(u32 i, u32 v) {
    cover(0x800DA9A8u);
    if (i >= 64) return;
    m.w8(m.r32(m.r32(kScreens) + 0x4C) + i, v);
}

void Game::t1_fade_set(u32 i, u32 state, u32 value, u32 end) {
    cover(0x800DA838u);
    if (i >= 16) return;
    const u32 t = m.r32(kScreens);
    m.w16(m.r32(t + 0x48) + 2 * i, state);
    if (static_cast<s32>(end) >= 0) m.w16(m.r32(t + 0x44) + 2 * i, end);
    if (static_cast<s32>(value) >= 0) m.w16(m.r32(m.r32(kScreens) + 0x40) + 2 * i, value);
}

s32 Game::t1_fade_state(u32 i) {
    cover(0x800DA898u);
    return m.r16s(m.r32(m.r32(kScreens) + 0x48) + 2 * i);
}

s32 Game::t1_fade_value(u32 i) {
    cover(0x800DA8BCu);
    return m.r16s(m.r32(m.r32(kScreens) + 0x40) + 2 * i);
}

void Game::t1_anim_push(u32 v) {
    cover(0x800DA49Cu);
    Frame f(*this, 0x18);
    const u32 a = m.r32(kAnim);
    for (s32 i = 7; i >= 0; --i) m.w8(a + static_cast<u32>(i) + 1, m.r8(a + static_cast<u32>(i)));
    m.w8(a, v);
    cover(0x800DA484u);
}

u32 Game::t1_flow() {
    cover(0x800DA4DCu);
    return m.r32(m.r32(kFlow));
}

void Game::t1_flow_set8(u32 v) {
    cover(0x800DA4F4u);
    m.w32(m.r32(kFlow) + 8, v);
}

u32 Game::t1_poly_g4(u32 d, u32 p, u32 ot) {
    cover(0x800D94C4u);
    m.w8(p + 3, 8);
    m.w8(p + 7, 0x38);
    m.w8(p + 7, m.r8(d + 0x13) ? 0x3Au : 0x38u);
    for (u32 k = 0; k < 4; ++k) pkt_rgb(p + 8 * k, m.r8(d + 0x10 + 4 * k), m.r8(d + 0x11 + 4 * k), m.r8(d + 0x12 + 4 * k));
    for (u32 k = 0; k < 4; ++k) {
        m.w16(p + 8 + 8 * k, m.r16(d + 4 * k));
        m.w16(p + 0xA + 8 * k, m.r16(d + 4 * k + 2));
    }
    ot_link(ot, p);
    return p + 0x24;
}

u32 Game::t1_poly_g3(u32 d, u32 p, u32 ot) {
    cover(0x800D9624u);
    m.w8(p + 3, 6);
    m.w8(p + 7, 0x30);
    m.w8(p + 7, m.r8(d + 0x13) ? 0x32u : 0x30u);
    for (u32 k = 0; k < 3; ++k) pkt_rgb(p + 8 * k, m.r8(d + 0x10 + 4 * k), m.r8(d + 0x11 + 4 * k), m.r8(d + 0x12 + 4 * k));
    for (u32 k = 0; k < 3; ++k) {
        m.w16(p + 8 + 8 * k, m.r16(d + 4 * k));
        m.w16(p + 0xA + 8 * k, m.r16(d + 4 * k + 2));
    }
    ot_link(ot, p);
    return p + 0x1C;
}

u32 Game::t1_poly_ft4(u32 d, u32 p, u32 ot) {
    cover(0x800D9748u);
    m.w8(p + 3, 9);
    m.w8(p + 7, 0x2C);
    m.w8(p + 7, m.r8(d + 0x13) ? 0x2Eu : 0x2Cu);
    pkt_rgb(p, m.r8(d + 0x10), m.r8(d + 0x11), m.r8(d + 0x12));
    for (u32 k = 0; k < 4; ++k) {
        m.w16(p + 8 + 8 * k, m.r16(d + 4 * k));
        m.w16(p + 0xA + 8 * k, m.r16(d + 4 * k + 2));
    }
    const u32 u = static_cast<u32>(static_cast<s16>(m.r16s(d + 0x20) % 64)) << 2;
    const u32 v = static_cast<u32>(m.r16s(d + 0x22) % 256);
    const u32 w = m.r8(d + 0x24), h = m.r8(d + 0x26);
    m.w8(p + 0xC, u);
    m.w8(p + 0xD, v);
    m.w8(p + 0x14, w + u);
    m.w8(p + 0x15, v);
    m.w8(p + 0x1C, u);
    m.w8(p + 0x1D, v + h);
    m.w8(p + 0x24, w + u);
    m.w8(p + 0x25, v + h);
    const u32 py = m.r16(d + 0x22), px = m.r16(d + 0x20);
    m.w16(p + 0x16, ((m.r8(d + 0x17) & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) | ((py & 0x200u) << 2));
    m.w16(p + 0xE, (m.r16(d + 0x2A) << 6) | ((m.r16(d + 0x28) >> 4) & 0x3Fu));
    ot_link(ot, p);
    return p + 0x28;
}

u32 Game::t1_logo(s32 x, s32 y, s32 a, u32 p) {
    cover(0x800D9A08u);
    Frame f(*this, 0x68);
    cover(0x800DA6FCu);
    const u32 ot = m.r32(m.r32(kScreens) + 0x10);
    const u32 d = f.at(0x10);
    m.w16(d + 0x20, 0x2E3);
    m.w16(d + 0x22, 0x94);
    m.w16(d + 0x24, 7);
    m.w16(d + 0x26, 0xF);
    m.w16(d + 0x28, 0x180);
    m.w16(d + 0x2A, 0xA);
    m.w8(d + 0x10, 0x80);
    m.w8(d + 0x11, 0x80);
    m.w8(d + 0x12, 0x80);
    m.w8(d + 0x13, 0);
    m.w8(d + 0x17, 0);
    auto quad = [&](s32 k, s32 ang) {
        const s32 c = rcos(ang), s = rsin(ang);
        auto q = [](s32 v) { return v / 4096; };
        auto half = [](s32 v) { return v / 2; };
        m.w16(d + 0x0, static_cast<u32>(x + half(q(-k * c) + q(-k * s))));
        m.w16(d + 0x2, static_cast<u32>(y + q(k * s) + q(-k * c)));
        m.w16(d + 0x4, static_cast<u32>(x + half(q(k * c) + q(-k * s))));
        m.w16(d + 0x6, static_cast<u32>(y + q(-k * s) + q(-k * c)));
        m.w16(d + 0x8, static_cast<u32>(x + half(q(-k * c) + q(k * s))));
        m.w16(d + 0xA, static_cast<u32>(y + q(k * s) + q(k * c)));
        m.w16(d + 0xC, static_cast<u32>(x + half(q(k * c) + q(k * s))));
        m.w16(d + 0xE, static_cast<u32>(y + q(-k * s) + q(k * c)));
    };
    quad(7, a);
    p = t1_poly_ft4(d, p, ot);
    m.w16(d + 0x20, 0x2E2);
    m.w16(d + 0x22, 0xA4);
    m.w16(d + 0x24, 0xD);
    m.w16(d + 0x26, 0x1B);
    quad(13, -a);
    return t1_poly_ft4(d, p, ot);
}

void Game::t1_events_open() {
    cover(0x800D56E0u);
    Frame f(*this, 0x18);
    hw->enter_critical_section();
    const u32 sw[4] = {4, 0x8000, 0x100, 0x2000}, hwc[3] = {4, 0x8000, 0x100};
    for (u32 i = 0; i < 4; ++i) m.w32(m.r32(kCards) + 4 + 4 * i, hw->open_event(kEvSwCard, sw[i], kEvMdNoIntr, 0));
    for (u32 i = 0; i < 3; ++i) m.w32(m.r32(kCards) + 0x14 + 4 * i, hw->open_event(kEvHwCard, hwc[i], kEvMdNoIntr, 0));
    for (u32 i = 0; i < 7; ++i) hw->enable_event(m.r32(m.r32(kCards) + 4 + 4 * i));
    hw->exit_critical_section();
}

void Game::t1_events_close() {
    cover(0x800D58B0u);
    Frame f(*this, 0x18);
    hw->enter_critical_section();
    for (u32 i = 0; i < 7; ++i) hw->close_event(m.r32(m.r32(kCards) + 4 + 4 * i));
    hw->exit_critical_section();
}

s32 Game::t1_card_wait(u32 base, u32 slot, u32 next) {
    cover(base == 4 ? 0x800D3EA8u : 0x800D4060u);
    Frame f(*this, 0x28);
    for (u32 i = 0; i < 4; ++i) {
        if (hw->test_event(m.r32(m.r32(kCards) + base + 4 * i)) == 1) {
            m.w8(m.r32(kCards) + slot + 0x34C, next);
            return static_cast<s32>(i);
        }
    }
    return -1;
}

void Game::t1_card_drain(u32 base) {
    cover(base == 4 ? 0x800D3F88u : 0x800D3FF4u);
    Frame f(*this, 0x18);
    for (u32 i = 0; i < 4; ++i) hw->test_event(m.r32(m.r32(kCards) + base + 4 * i));
}

void Game::t1_card_poll() {
    cover(0x800D5F6Cu);
    Frame f(*this, 0x20);
    const u32 s = m.r32(m.r32(kCards) + 0x354);
    auto B = [&]() { return m.r32(kCards); };
    auto state = [&]() { return B() + s + 0x34C; };
    auto swap = [&]() { m.w32(B() + 0x354, 1u - m.r32(B() + 0x354)); };
    auto retry = [&]() {
        const u32 b = B() + s;
        if (m.r8(b + 0x350) < 0x11) {
            m.w8(b + 0x348, 0);
            m.w8(B() + s + 0x350, m.r8(B() + s + 0x350) + 1);
        } else {
            m.w8(b + 0x348, 1);
        }
    };
    switch (m.r8(state())) {
    case 0:
        swap();
        return;
    case 1:
        t1_card_drain(4);
        hw->card_info(s << 4);
        m.w8(state(), 2);
        m.w32(B() + 0x318, 0x12C);
        return;
    case 2: {
        const s32 r = t1_card_wait(4, s, 3);
        m.w8(B() + s + 0x34E, static_cast<u32>(r));
        const s32 t = m.r32s(B() + 0x318);
        if (t >= 0) m.w32(B() + 0x318, static_cast<u32>(t - 1));
        else m.w8(B() + s + 0x34C, 1);
        return;
    }
    case 3: {
        const u32 r = m.r8(B() + s + 0x34E);
        m.w16(B() + 2 * s + 0x2C, r);
        if (r == 1) {
            retry();
            m.w8(B() + s + 0x34C, m.r32(B() + 0x28) == 1 ? 1u : 0u);
            swap();
        } else if (r == 0) {
            if (m.r8(B() + s + 0x348) == 1) {
                m.w8(B() + s + 0x34C, m.r32(B() + 0x28) ? 1u : 0u);
                swap();
            } else {
                t1_card_drain(0x14);
                hw->card_clear(s << 4);
                m.w8(B() + s + 0x34C, 4);
            }
        } else if (r == 2) {
            retry();
            m.w8(B() + s + 0x34C, m.r32(B() + 0x28) ? 1u : 0u);
            m.w32(B() + 0x354, 1u - m.r32(B() + 0x354));
            m.w8(B() + s + 0x34A, 0);
        } else if (r == 3) {
            m.w16(B() + 2 * s + 0x2C, 0);
            t1_card_drain(0x14);
            hw->card_clear(s << 4);
            m.w8(B() + s + 0x34C, 4);
            m.w8(B() + s + 0x348, 0);
            m.w8(B() + s + 0x350, 0);
        } else {
            m.w8(B() + s + 0x348, 1);
            m.w8(B() + s + 0x34C, 1);
            swap();
        }
        return;
    }
    case 4:
        do {
            m.w8(B() + s + 0x34E, static_cast<u32>(t1_card_wait(0x14, s, 5)));
        } while (m.r8(B() + s + 0x34E) == 0xFF);
        return;
    case 5:
        m.w16(B() + 2 * s + 0x2C, m.r8(B() + s + 0x34E));
        t1_card_drain(4);
        hw->card_load(s << 4);
        m.w8(B() + s + 0x34C, 6);
        return;
    case 6:
        m.w8(B() + s + 0x34E, static_cast<u32>(t1_card_wait(4, s, 7)));
        return;
    case 7: {
        m.w16(B() + 2 * s + 0x30, m.r8(B() + s + 0x34E));
        m.w8(B() + s + 0x348, 1);
        const s32 v = m.r16s(B() + 2 * s + 0x30);
        if (v < 3) {
            if (v > 0) {
                retry();
                m.w8(B() + s + 0x34C, 1);
            } else if (v == 0) {
                m.w8(B() + s + 0x34C, 8);
            }
        } else if (v == 3) {
            m.w8(B() + s + 0x348, 1);
            m.w8(B() + s + 0x34C, 1);
            swap();
        }
        return;
    }
    case 8: {
        const u32 fd = hw->file_open(B() + 26 * s + 0x2B4, 0x8001);
        m.w32(B() + 0x3C, fd);
        if (fd != 0xFFFFFFFFu) m.w8(B() + s + 0x34A, 1);
        hw->file_close(m.r32(B() + 0x3C));
        m.w8(B() + s + 0x34C, 1);
        m.w8(B() + s + 0x348, 1);
        swap();
        return;
    }
    default: return;
    }
}

s32 Game::t1_card_status(u32 slot) {
    cover(0x800D3D90u);
    const u32 b = m.r32(kCards);
    if (!m.r8(b + slot + 0x348)) return m.r16s(b + 2 * slot + 0x2C) == 2 ? 6 : 0;
    const s32 info = m.r16s(b + 2 * slot + 0x2C);
    if (info == 1) return 1;
    if (info == 2) return 2;
    if (info != 0 && info != 3) return -1;
    const s32 load = m.r16s(m.r32(kCards) + 2 * slot + 0x30);
    if (load == 1) return 1;
    if (load == 2) return 2;
    if (load == 3) return 3;
    if (load != 0) return -1;
    const u32 has = m.r8(m.r32(kCards) + slot + 0x34A);
    if (has == 1) return 5;
    if (has == 0) return 4;
    return 1;
}

bool Game::t1_op_on(u32 args, u32 at) {
    const s32 fl = m.r16s(args + at);
    if (!fl) return true;
    return m.r16s(args + at + 2) == static_cast<s32>(m.r8(m.r32(m.r32(kScreens) + 0x4C) + static_cast<u32>(fl)));
}

u32 Game::t1_op_slot(u32 ot) { return ot + 4u * static_cast<u32>(m.r16s(m.r32(m.r32(kScreens) + 0x2C) + 0xE)); }

bool Game::t1_clip() {
    const u32 c = m.r32(m.r32(kScreens) + 0x2C);
    switch (m.r16s(c + 0xA)) {
    case 0: cover(0x800DA9D4u); return true;
    case 1: return t1_clip_wipe();
    case 2: {
        cover(0x800DAC98u);
        const u32 t = m.r32(kScreens);
        return m.r16s(m.r32(t + 0x48) + 2u * static_cast<u32>(m.r16s(m.r32(t + 0x2C) + 0xC))) == 2;
    }
    case 3: return t1_clip_open();
    default:
        unported(0x800DD924u, "a clip function past overlay 1's table");
        return false;
    }
}

bool Game::t1_clip_wipe() {
    cover(0x800DA9DCu);
    Frame f(*this, 8);
    const u32 t = m.r32(kScreens);
    const u32 c0 = m.r32(t + 0x2C);
    const s32 v = m.r16s(m.r32(t + 0x40) + 2u * static_cast<u32>(m.r16s(c0 + 0xC)));
    m.w32(f.at(0), m.r16(c0 + 4) | (static_cast<u32>(m.r16s(c0 + 6)) << 16));
    m.w32(f.at(4), m.r16(m.r32(t + 0x2C) + 8));
    const u32 c = m.r32(t + 0x2C);
    const s32 st = m.r16s(m.r32(t + 0x48) + 2u * static_cast<u32>(m.r16s(c + 0xC)));
    const u32 q = m.r32(kScratchPtr);
    if (st == 0) return false;
    if (st == 2) return true;
    const s32 t0 = m.r16s(f.at(0));
    if (v < t0) return false;
    const s32 t1 = m.r16s(f.at(2));
    if (v < t1) {
        m.w16(q, m.r16(c) + static_cast<u32>(((m.r16s(q) - m.r16s(c)) * (v - t0)) / (t1 - t0)));
        const u32 c2 = m.r32(t + 0x2C);
        const s32 dy = m.r16s(q + 2) - m.r16s(c2 + 2);
        const s32 ady = dy < 0 ? -dy : dy;
        m.w16(q + 2, m.r16(c2 + 2) + static_cast<u32>(dy / ady));
        m.w16(q + 4, static_cast<u32>((m.r16s(q + 4) * (v - t0)) / (t1 - t0)));
        m.w16(q + 6, 2);
        return true;
    }
    const s32 t2 = m.r16s(f.at(4));
    if (!(v < t2)) return true;
    m.w16(q + 2, m.r16(c + 2) + static_cast<u32>(((m.r16s(q + 2) - m.r16s(c + 2)) * (v - t1)) / (t2 - t1)));
    m.w16(q + 6, static_cast<u32>((m.r16s(q + 6) * (v - t1)) / (t2 - t1)));
    return true;
}

bool Game::t1_clip_open() {
    cover(0x800DACD0u);
    Frame f(*this, 8);
    const u32 t = m.r32(kScreens);
    const u32 c0 = m.r32(t + 0x2C);
    const s32 v = m.r16s(m.r32(t + 0x40) + 2u * static_cast<u32>(m.r16s(c0 + 0xC)));
    m.w32(f.at(0), m.r16(c0 + 4) | (static_cast<u32>(m.r16s(c0 + 6)) << 16));
    m.w32(f.at(4), m.r16(m.r32(t + 0x2C) + 8));
    const u32 c = m.r32(t + 0x2C);
    const s32 st = m.r16s(m.r32(t + 0x48) + 2u * static_cast<u32>(m.r16s(c + 0xC)));
    const u32 q = m.r32(kScratchPtr);
    if (st == 0) return false;
    if (st == 2) return true;
    const s32 t0 = m.r16s(f.at(0));
    if (v < t0) return false;
    const s32 t1 = m.r16s(f.at(2));
    if (!(v < t1)) return true;
    m.w16(q, m.r16(c) + static_cast<u32>(((m.r16s(q) - m.r16s(c)) * (v - t0)) / (t1 - t0)));
    m.w16(q + 4, static_cast<u32>((m.r16s(q + 4) * (v - t0)) / (t1 - t0)));
    return true;
}

u32 Game::t1_op(u32 op, u32 a, u32 ot, u32 e) {
    const u32 T = kScreens;
    auto take = [&](u32 n) {
        const u32 p = m.r32(m.r32(T) + 4);
        m.w32(m.r32(T) + 4, p + n);
        return p;
    };
    auto head = [&](u32 p, u32 len) {
        const u32 slot = t1_op_slot(ot);
        m.w32(p, m.r32(slot) | (len << 24));
        m.w32(t1_op_slot(ot), p & 0x00FFFFFFu);
    };
    auto rect_in = [&](u32 x, u32 wh) {
        const u32 q = m.r32(kScratchPtr);
        m.w32(q, m.r16(a + x) | (static_cast<u32>(m.r16s(a + x + 2)) << 16));
        m.w32(q + 4, m.r16(a + wh) | (static_cast<u32>(m.r16s(a + wh + 2)) << 16));
    };
    auto xy = [&](s32 dx, s32 dy) {
        const u32 q = m.r32(kScratchPtr);
        return (static_cast<u32>(m.r16s(q) + m.r16s(e + 4) + dx) & 0xFFFFu) |
               (static_cast<u32>(m.r16s(q + 2) + m.r16s(e + 6) + dy) << 16);
    };
    switch (op) {
    case 0: {
        cover(0x800DAE44u);
        Frame f(*this, 0x30);
        if (!t1_op_on(a, 0xC)) return a + 20;
        const u32 p = take(16);
        rect_in(0, 4);
        if (!t1_clip()) return a + 20;
        const u32 q = m.r32(kScratchPtr);
        m.w32(p + 8, xy(0, 0));
        m.w32(p + 0xC, m.r16(q + 4) | (static_cast<u32>(m.r16s(q + 6)) << 16));
        m.w8(p + 4, m.r8(a + 8));
        m.w8(p + 5, m.r8(a + 9));
        m.w8(p + 3, 3);
        m.w8(p + 7, 0x60);
        m.w8(p + 6, m.r8(a + 0xA));
        m.w8(p + 7, m.r8(a + 0xB) ? 0x62u : 0x60u);
        head(p, 3);
        return a + 20;
    }
    case 1: {
        cover(0x800DB024u);
        Frame f(*this, 0x28);
        if (!t1_op_on(a, 0x14)) return a + 0x1C;
        const u32 p = take(20);
        if (!t1_clip()) return a + 0x1C;
        m.w32(p + 8, (static_cast<u32>(m.r16s(a) + m.r16s(e + 4)) & 0xFFFFu) |
                         (static_cast<u32>(m.r16s(a + 2) + m.r16s(e + 6)) << 16));
        m.w32(p + 0x10, ((static_cast<u32>(m.r16s(a + 0x10)) << 2) & 0xFFFFu) |
                            (static_cast<u32>(m.r16s(a + 0x12)) << 16));
        const u32 u = static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16s(a + 0xC) % 64)) << 2);
        const u32 v = static_cast<u32>(static_cast<s32>(static_cast<s16>(m.r16s(a + 0xE) % 256)) << 8);
        const u32 clut = (static_cast<u32>(m.r16s(a + 0xA)) << 6) | ((m.r16(a + 8) >> 4) & 0x3Fu);
        m.w32(p + 0xC, u | v | (clut << 16));
        m.w8(p + 4, m.r8(a + 4));
        m.w8(p + 5, m.r8(a + 5));
        m.w8(p + 3, 4);
        m.w8(p + 7, 0x64);
        m.w8(p + 6, m.r8(a + 6));
        m.w8(p + 7, m.r8(a + 7) ? 0x66u : 0x64u);
        head(p, 4);
        return a + 0x1C;
    }
    case 2: {
        cover(0x800DB254u);
        Frame f(*this, 0x30);
        const u32 p = take(12);
        const u32 py = m.r16(a + 2), px = m.r16(a);
        const u32 tp = ((m.r8(a + 7) & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) | ((py & 0x200u) << 2);
        m.w32(f.at(0x10), 0);
        set_draw_mode(p, 0, 0, tp, 0);
        ot_link(t1_op_slot(ot), p);
        return a + 12;
    }
    case 3: {
        cover(0x800DB374u);
        Frame f(*this, 0x30);
        if (!t1_op_on(a, 0x18)) return a + 0x20;
        const u32 p = take(40);
        const bool semi = m.r8(a + 7) != 0xFF;
        const u32 abr = semi ? m.r8(a + 7) : 0u;
        rect_in(0, 0x14);
        if (!t1_clip()) return a + 0x20;
        const s32 k = m.r8(a + 0x1E) ? 2 : 4;
        s32 u0 = static_cast<s16>(m.r16s(a + 0xC) % 64) * k;
        if (m.r16s(a + 0x14) < 0) u0 -= 1;
        s32 v0 = m.r16s(a + 0xE) % 256;
        if (m.r16s(a + 0x16) < 0) v0 -= 1;
        const s32 wk = m.r16s(a + 0x10) * k;
        s32 w = wk - 1;
        if (m.r16s(a + 0x14) == wk) w = m.r16s(a + 0x14);
        s32 h = static_cast<s32>(m.r16(a + 0x12)) - 1;
        if (m.r16s(a + 0x16) == m.r16s(a + 0x12)) h = m.r16(a + 0x12);
        if (static_cast<s16>(u0) + static_cast<s16>(w) == 0x100) w -= 1;
        if (static_cast<s16>(v0) + static_cast<s16>(h) == 0x100) h -= 1;
        m.w8(p + 4, m.r8(a + 4));
        m.w8(p + 5, m.r8(a + 5));
        m.w8(p + 3, 9);
        m.w8(p + 7, 0x2C);
        m.w8(p + 6, m.r8(a + 6));
        m.w8(p + 7, semi ? 0x2Eu : 0x2Cu);
        const s32 u = static_cast<s16>(u0), vv = static_cast<s16>(v0);
        const u32 t0 = static_cast<u32>(vv << 8);
        const u32 clut = (static_cast<u32>(m.r16s(a + 0xA)) << 6) | ((m.r16(a + 8) >> 4) & 0x3Fu);
        m.w32(p + 0xC, static_cast<u32>(u) | t0 | (clut << 16));
        const u32 vb = static_cast<u32>((vv + static_cast<s16>(h)) << 8);
        m.w32(p + 0x1C, static_cast<u32>(u) | vb);
        const s32 ur = u + static_cast<s16>(w);
        m.w32(p + 0x24, static_cast<u32>(ur) | vb);
        const u32 py = m.r16(a + 0xE), px = m.r16(a + 0xC);
        const u32 tp = ((m.r8(a + 0x1E) & 3u) << 7) | ((abr & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) |
                       ((py & 0x200u) << 2);
        m.w32(p + 0x14, (static_cast<u32>(ur) | t0) | (tp << 16));
        const u32 q = m.r32(kScratchPtr);
        const s32 qw = m.r16s(q + 4), qh = m.r16s(q + 6);
        m.w32(p + 8, xy(0, 0));
        m.w32(p + 0x10, xy(qw, 0));
        m.w32(p + 0x18, xy(0, qh));
        m.w32(p + 0x20, xy(qw, qh));
        head(p, 9);
        return a + 0x20;
    }
    case 4: {
        cover(0x800DB834u);
        Frame f(*this, 0x30);
        const u32 p = take(12);
        const u32 r = f.at(0x10);
        for (u32 i = 0; i < 4; ++i) m.w16(r + 2 * i, m.r16(a + 4 + 2 * i));
        set_draw_area(p, r);
        ot_link(ot + 4 * m.r32(a + 0x10), p);
        return a + 20;
    }
    case 5:
        cover(0x800DB910u);
        m.w32(m.r32(T) + 0x2C, a);
        return a + 16;
    case 6: {
        cover(0x800DB928u);
        Frame f(*this, 0x28);
        if (!t1_op_on(a, 0xC)) return a + 20;
        const u32 p = take(16);
        rect_in(0, 4);
        if (!t1_clip()) return a + 20;
        const u32 q = m.r32(kScratchPtr);
        m.w16(p + 8, m.r16(q) + m.r16(e + 4));
        m.w16(p + 0xA, m.r16(q + 2) + m.r16(e + 6));
        m.w16(p + 0xC, m.r16(q + 4) + m.r16(e + 4));
        m.w16(p + 0xE, m.r16(q + 6) + m.r16(e + 6));
        m.w8(p + 4, m.r8(a + 8));
        m.w8(p + 5, m.r8(a + 9));
        m.w8(p + 3, 3);
        m.w8(p + 7, 0x40);
        m.w8(p + 6, m.r8(a + 0xA));
        m.w8(p + 7, m.r8(a + 0xB) ? 0x42u : 0x40u);
        ot_link(t1_op_slot(ot), p);
        return a + 20;
    }
    default: return a;
    }
}

void Game::t1_screen_cb(u32 i) {
    const u32 fn = m.r32(0x800DD950u + 4 * i);
    switch (fn) {
    case 0x800DC42Cu: case 0x800DC5E4u: case 0x800DC694u: case 0x800DC69Cu:
    case 0x800DC6A4u: case 0x800DC6ACu: case 0x800DC6B4u: case 0x800DC6BCu:
        cover(fn);
        return;
    case 0x800DC434u: t1_menu_text(); return;
    case 0x800DC5ECu: t1_text_list(); return;
    default:
        call_by_address(fn, "a screen callback of overlay 1");
    }
}

void Game::t1_menu_text() {
    cover(0x800DC434u);
    Frame f(*this, 0x20);
    const bool dl = t1_card_status(0) == 5 || t1_card_status(1) == 5;
    const bool reopen = (m.r32(scene(m) + 0x964) & 1u) != 0;
    t1_prim_mode(4);
    t1_text_semi(2);
    t1_text_fold(1, 0x1E, 0x28);
    t1_text_fold_at(static_cast<u32>(t1_fade_value(3)));
    t1_prim_rgb(0x80, 0x80, 0x80);
    t1_prim_origin(0x6A, 0x96);
    t1_printf(0x800CC5B8u);
    auto entry = [&](bool on, u32 y, u32 s) {
        t1_prim_origin(0x6A, y);
        if (on) {
            t1_printf(s);
            return;
        }
        t1_text_semi(1);
        t1_prim_rgb(8, 0x30, 0x28);
        t1_printf(s);
        t1_text_semi(2);
        t1_prim_rgb(0x80, 0x80, 0x80);
    };
    entry(dl, 0xCC, 0x800CC5C4u);
    entry(reopen, 0x101, 0x800CC5D0u);
    t1_text_fold(1, 0x1E, 0x28);
    t1_text_fold_at(static_cast<u32>(t1_fade_value(4)));
    t1_prim_rgb(0x80, 0x80, 0x80);
}

void Game::t1_text_list() {
    cover(0x800DC5ECu);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kState);
    u32 s = m.r32(st + 4 * m.r8(st + 0x5C) + 0x64);
    t1_prim_mode(3);
    t1_text_semi(2);
    t1_text_fold(1, 0x1E, 0x28);
    t1_text_fold_at(static_cast<u32>(t1_fade_value(0xF)));
    t1_prim_rgb(0x80, 0x80, 0x80);
    for (u32 i = 0; i < 0x32; ++i, s += 8) {
        t1_prim_origin(m.r16(s), m.r16(s + 2));
        t1_printf(s + 4);
    }
}
}
