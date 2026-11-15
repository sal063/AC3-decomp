#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kO = 0x80112DC4u, kQ = 0x80112DC0u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kSel = 0x80112DB0u;
constexpr u32 kRecordLoaded = 0x8009B904u;
u32 O(Memory& m) { return m.r32(kO); }
u32 sc(Memory& m) { return m.r32(g::kScene); }
}

void Game::voices_off() {
    cover(0x8002DDE0u);
    Frame f(*this, 24);
    for (u32 v = 0; v < 24; ++v) voice_clear(v);
    spu_set_key(0, 0xFFFFFF);
}

u32 Game::o2_phase8() {
    cover(0x800CDF40u);
    Frame f(*this, 0x28);
    const u32 k = m.r32(sc(m) + 4);
    {
        const u32 o = O(m);
        m.w32(o + 0x40, m.r32(o + 4 * k + 0x38));
        m.w32(o + 0x274, m.r32(o + 4 * k + 0x278));
        const u32 ot = m.r32(o + 4 * k);
        m.w32(o + 8, ot);
        clear_otag_r(ot, 16);
    }
    {
        const u32 o = O(m), top = m.r32(sc(m) + 8);
        const u32 ot = m.r32(o + 8);
        m.w32(ot, (m.r32(ot) & 0xFF000000u) | (m.r32(top) & 0x00FFFFFFu));
        m.w32(top, (m.r32(top) & 0xFF000000u) | ((ot + 0x3C) & 0x00FFFFFFu));
        o2_text_frame(ot);
    }
    o2_hangar_frame(m.r32(O(m) + 8) + 0x3C);
    {
        const u32 o = O(m);
        const u32 t = m.r32(o + 4 * m.r32(sc(m) + 4) + 0xC);
        m.w32(o + 0x14, t);
        clear_otag_r(t, 256);
        const u32 o2 = O(m);
        const u32 t2 = m.r32(o2 + 0x14), ot = m.r32(o2 + 8);
        m.w32(t2, (m.r32(t2) & 0xFF000000u) | (m.r32(ot + 4) & 0x00FFFFFFu));
        m.w32(ot + 4, (m.r32(ot + 4) & 0xFF000000u) | ((t2 + 0x3FC) & 0x00FFFFFFu));
    }
    {
        const u32 w = m.r32(g::kWorld), kk = m.r32(sc(m) + 4);
        m.w32(w + 0x10, m.r32(w + 4 * kk + 0x18));
        m.w32(w + 0x14, m.r32(w + 4 * kk + 0x20));
    }
    for (u32 r : {13u, 14u, 15u}) gte.ctc2(r, 0xC00);
    for (u32 r : {21u, 22u, 23u}) gte.ctc2(r, 0x600);
    gte.ctc2(27, 0xFFFF8800u);
    gte.ctc2(28, 0x3000000);
    gte.ctc2(8, 0x8000000);
    gte.ctc2(9, 0x4000000);
    gte.ctc2(10, 0x400);
    gte.ctc2(11, 0x400FC00);
    gte.ctc2(12, 0);
    gte.ctc2(16, 0x2000400);
    gte.ctc2(17, 0x4000200);
    gte.ctc2(18, 0x2000200);
    gte.ctc2(19, 0x2000400);
    gte.ctc2(20, 0x200);
    o2_flag(20, (m.r32(sc(m) + 0x124) & 0x10u) ? 1u : 0u);
    m.w8(m.r32(m.r32(kQ) + 0x5C) + 0x20, m.r8(sc(m) + 0x394));
    o2_state(m.r32(m.r32(0x80109814u + 4 * m.r8(O(m) + 0x7C)) + 4 * m.r8(O(m) + 0x7D)));
    {
        const u32 o = O(m);
        bool set = true;
        u32 v = 0;
        if (m.r32(sc(m) + 0x96C) & 0x10u) {
            if (m.r16s(o + 0x84) == 6) set = false;
            else v = m.r8(o + 0x7C) ? 5u : m.r8(o + 0x7E) + 9u;
        } else {
            if (m.r8(o + 0x7C) == 1 || m.r16s(o + 0x84) == 6) set = false;
            else v = m.r8(o + 0x7C) ? m.r8(o + 0x7E) + 5u : m.r8(o + 0x7E) + 1u;
        }
        if (set) o2_flag(0x2C, v);
    }
    {
        const u32 p = o2_hangar_draw(m.r32(O(m) + 0x274));
        m.w32(O(m) + 0x274, p);
        m.w32(O(m) + 0x274, o2_screens_frame(p));
    }
    m.w32(O(m) + 0x4C, m.r32(O(m) + 0x4C) + 1);
    m.w8(m.r32(m.r32(kQ) + 0x5C) + 0x28, (m.r32(sc(m) + 0x124) & 0x10u) ? 1u : 0u);
    const u32 o = O(m);
    if (m.r32(o + 0x94)) {
        for (u32 i = 0; i < 5; ++i) voice_release(m.r32(O(m) + 0x130 + 4 * i));
        reverb_off();
        voices_off();
        cd_reset(0, 0);
        const u32 d = m.r32(kDisplay);
        m.w32(d, m.r32(d) | 4u);
        if (m.r8(O(m) + 0x94) == 4) {
            const u32 s = sc(m);
            const u32 a = m.r16(s + 0x38C), b = m.r16(s + 0x38E);
            m.w16(s + 0x390, a);
            m.w16(s + 0x392, b);
        }
        return m.r32(O(m) + 0x94);
    }
    if (m.r8(o + 0x7F) == 1) {
        voices_off();
        cd_reset(0, 0);
        const u32 d = m.r32(kDisplay);
        m.w32(d, m.r32(d) | 4u);
        const u32 s = sc(m);
        const u32 a = m.r16(s + 0x38C), b = m.r16(s + 0x38E);
        m.w16(s + 0x390, a);
        m.w16(s + 0x392, b);
        return 0x60000004u;
    }
    if (m.r32(o + 0x11C) == 1) {
        voices_off();
        cd_reset(0, 0);
        const u32 d = m.r32(kDisplay);
        m.w32(sc(m) + 0x3A0, 3);
        m.w32(d, m.r32(d) | 4u);
        return 0x60000005u;
    }
    return 0;
}

void Game::o2_state(u32 fn) {
    switch (fn) {
    case 0x800E6658u: o2_menus_init(); return;
    case 0x800E6954u: o2_menus_run(); return;
    default:
        call_guest(fn, O(m), (m.r32(sc(m) + 292) & 16) ? 1u : 0u);
    }
}

void Game::o2_menus_init() {
    cover(0x800E6658u);
    const u32 o = O(m);
    m.w8(o + 0x7E, 0);
    m.w8(o + 0x7D, 1);
    const s32 st = m.r16s(sc(m) + 0x954);
    const u32 flags = m.r32(sc(m) + 0x96C);
    u32 major = 7;
    if (st == 1) major = 5;
    else if (st == 0) major = 1;
    else if (st == 2) major = (flags & 0x80u) ? 7u : (flags & 0x40u) ? 5u : 7u;
    else if (st == 3) major = 8;
    m.w16(O(m) + 0x84, major);
    const u32 f2 = m.r32(sc(m) + 0x96C);
    if (f2 & 0x20u) {
        m.w32(O(m) + 0x84, 5);
    } else if (f2 & 8u) {
        m.w16(O(m) + 0x84, 6);
    } else if (m.r16s(sc(m) + 0x38C) == 59) {
        m.w16(O(m) + 0x84, 8);
    }
    m.w16(O(m) + 0x86, 0);
}

void Game::o2_menus_run() {
    cover(0x800E6954u);
    Frame f(*this, 0x18);
    const u32 o = O(m);
    const u32 fn = m.r32(m.r32(0x8010B1E8u + 4u * static_cast<u32>(m.r16s(o + 0x84))) + 4u * static_cast<u32>(m.r16s(o + 0x86)));
    switch (fn) {
    case 0x800F0A7Cu: o2_brief_init(); return;
    case 0x800F108Cu: o2_brief_fade_in(); return;
    case 0x800F11BCu: o2_brief_arrive(); return;
    case 0x800F1384u: o2_brief_screen_in(); return;
    default:
        call_guest(fn, o, 0);
    }
}

void Game::o2_entry_enter(u32 g, u32 i) {
    cover(0x800DEBB8u);
    Frame f(*this, 0x20);
    s32 x = 0, y = 0, v = 0;
    switch (i) {
    case 0: y = 55; v = 10; break;
    case 1: x = -329; v = 25; break;
    case 5: y = -504; v = 31; break;
    case 8: case 13: x = 325; v = -25; break;
    case 15: y = 512; v = -31; break;
    default: break;
    }
    m.w32(f.at(0x10), static_cast<u32>(y));
    m.w32(f.at(0x14), static_cast<u32>(v));
    o2_entry_set(g, i, 0, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(v));
    m.w32(O(m) + 0x1A4, 0);
}

void Game::o2_entry_leave(u32 g, u32 i) {
    cover(0x800DECD4u);
    Frame f(*this, 0x20);
    const u32 e = m.r32(O(m) + 4 * g + 0x16C) + 12 * i;
    const s32 x = m.r16s(e + 4), y = m.r16s(e + 6), v = m.r16s(e + 8);
    m.w32(f.at(0x10), static_cast<u32>(y));
    m.w32(f.at(0x14), static_cast<u32>(-v));
    o2_entry_set(g, i, 2, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(-v));
}

void Game::o2_brief_fade_in() {
    cover(0x800F108Cu);
    Frame f(*this, 0x18);
    const u32 s = m.r32(kSel);
    const s32 t = m.r32s(s + 0x10);
    m.w32(s + 0x10, static_cast<u32>(t + 1));
    if (t >= 32) {
        o2_hangar_fly();
        m.w32(m.r32(kSel) + 0x10, 0);
        m.w16(O(m) + 0x86, 2);
        return;
    }
    const u32 d = m.r32(kDisplay);
    m.w8(d + 0x10, static_cast<u32>(143 * (t + 2) / 32));
    m.w8(d + 0x11, static_cast<u32>(135 * (m.r32s(m.r32(kSel) + 0x10) + 1) / 32));
    m.w8(d + 0x12, static_cast<u32>(83 * (m.r32s(m.r32(kSel) + 0x10) + 1) / 32));
    m.w32(d, m.r32(d) | 8u);
}

void Game::o2_brief_arrive() {
    cover(0x800F11BCu);
    Frame f(*this, 0x18);
    if (m.r32(m.r32(0x80112DA8u) + 0x14) != 2) return;
    const s32 st = m.r16s(sc(m) + 0x954);
    if (st == 1) {
        if (m.r32(m.r32(kSel) + 0xC)) {
            o2_entry_enter(0, 15);
            o2_group_set(0, 0x8000);
            m.w16(O(m) + 0x86, 3);
        } else {
            o2_entry_leave(0, 15);
            m.w16(O(m) + 0x86, 15);
        }
        return;
    }
    if (static_cast<u16>(st - 2) >= 2 || m.r32(m.r32(kSel) + 0xC)) {
        m.w16(O(m) + 0x86, 3);
        o2_entry_enter(0, 15);
        o2_group_set(0, 0x8000);
        return;
    }
    const u32 o = O(m);
    m.w8(o + 0x7C, 0);
    m.w8(o + 0x7D, 0);
    m.w8(o + 0x7E, 4);
    heap_set(m.r32(kSel));
    o2_entry_enter(0, 0);
    o2_entry_enter(0, 1);
    o2_group_set(0, 3);
    o2_hangar_effect2(1);
    m.w8(sc(m) + 0x95E, 0);
    m.w16(sc(m) + 0x954, 0);
}

void Game::o2_brief_screen_in() {
    cover(0x800F1384u);
    if (m.r16(m.r32(O(m) + 0x16C) + 190) != 1) return;
    const u32 s = m.r32(kSel);
    if (m.r32(s + 0xC)) {
        m.w16(O(m) + 0x86, 4);
        m.w32(s + 0x10, 0);
    } else {
        m.w16(O(m) + 0x86, 15);
    }
}

u32 Game::o2_page_layout(u32 kind) {
    cover(0x800F00A8u);
    switch (static_cast<s16>(kind)) {
    case 0: case 1: return 18;
    case 2: return 19;
    case 3: case 4: case 5: case 6: case 7: return 16;
    case 8: case 11: return 17;
    case 9: return 22;
    case 10: return 18;
    default: return 10;
    }
}

u32 Game::o2_page_has_picture(s32 kind) {
    cover(0x800F0144u);
    return kind >= 3 && kind < 11 ? 1u : 0u;
}

u32 Game::o2_page_request() {
    cover(0x800F07F4u);
    Frame f(*this, 0x18);
    const u32 s = m.r32(kSel);
    if (!m.r32(s + 0xC)) return 0;
    const u32 list = m.r32(g::kExit) + 0x60;
    const s32 page = m.r16s(m.r32(s + 0x189C));
    m.w32(list, static_cast<u32>(m.r16s(0x8010B400u + 8u * static_cast<u32>(page))));
    const u32 buf = mem_alloc(records_size(list, 1));
    m.w32(sc(m) + 0x44, 1);
    request_records(m.r32(g::kExit) + 0x60, 1, buf, kRecordLoaded);
    return 1;
}

void Game::o2_brief_init() {
    o2_campaign_brief_begin();
}
}
