#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kScreens = 0x800DD9DCu;
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kCards = 0x800DDA48u;
constexpr u32 kAccount = 0x800DDA44u;
constexpr u32 kQueue = 0x800DDA60u;
constexpr u32 kSoundItem = 0x800DD4C0u;
constexpr u32 kSaveSum = 0x41434533u;
constexpr u32 kPad = 0x12C, kPadHeld = 0x128, kPadRepeat = 0x130;

u32 scene(Memory& m) { return m.r32(g::kScene); }
u32 pilot(Memory& m) { return scene(m) + 0x95C; }
}

void Game::t1_new_sub(u32 fn) {
    switch (fn) {
    case 0x800CE518u: t1_name_init(); return;
    case 0x800CE5ACu: t1_name_run(); return;
    case 0x800CEB20u: t1_n1_init(); return;
    case 0x800CEB90u: t1_n1_run(); return;
    case 0x800CEC70u: t1_n2_init(); return;
    case 0x800CECF8u: t1_difficulty(); return;
    case 0x800CEECCu: t1_n3_init(); return;
    case 0x800CEF44u: t1_n3_run(); return;
    case 0x800CF004u: t1_n4_init(); return;
    case 0x800CF06Cu: t1_sound_settings(); return;
    case 0x800CF750u: t1_n5_init(); return;
    case 0x800CF7B4u: t1_screen_position(); return;
    case 0x800CFA34u: t1_n6_init(); return;
    case 0x800CFA94u: t1_n6_run(); return;
    case 0x800CFB2Cu: t1_n7_init(); return;
    case 0x800CFBD0u: t1_n7_run(); return;
    case 0x800CFC38u: t1_n8_init(); return;
    case 0x800CFC94u: t1_n8_run(); return;
    case 0x800CFD28u: t1_n9_init(); return;
    case 0x800CFD60u: t1_n9_run(); return;
    case 0x800CFDCCu: t1_n10_init(); return;
    case 0x800CFEA4u: t1_n10_run(); return;
    case 0x800CFF90u: t1_n11_init(); return;
    case 0x800D0074u: t1_slot_select(); return;
    case 0x800D02DCu: t1_n12_init(); return;
    case 0x800D0384u: t1_confirm_run(0x800D0384u); return;
    case 0x800D0460u: t1_n13_init(); return;
    case 0x800D0548u: t1_n13_run(); return;
    case 0x800D061Cu: t1_n13_format(); return;
    case 0x800D0668u: t1_n14_0(); return;
    case 0x800D06D4u: t1_n14_1(); return;
    case 0x800D076Cu: t1_confirm_run(0x800D076Cu); return;
    case 0x800D0848u: t1_n14_3(); return;
    case 0x800D08DCu: t1_n14_4(); return;
    case 0x800D0978u: t1_n14_5(); return;
    case 0x800D0A10u: t1_confirm_run(0x800D0A10u); return;
    case 0x800D0AECu:
        cover(0x800D0AECu);
        m.w16(m.r32(kState) + 0x4E, m.r16(m.r32(kState) + 0x4E) + 1);
        return;
    case 0x800D0B0Cu: t1_n15_1(); return;
    case 0x800D0BFCu: t1_n15_2(); return;
    case 0x800D0CECu: t1_confirm_run(0x800D0CECu); return;
    case 0x800D0DC8u: t1_n16_init(); return;
    case 0x800D0E54u: t1_pilot_select(); return;
    case 0x800D1080u: t1_n17_init(); return;
    case 0x800D1108u: t1_n17_run(); return;
    case 0x800D12A4u: t1_n18_0(); return;
    case 0x800D1300u: t1_n18_1(); return;
    case 0x800D13B8u: t1_n18_2(); return;
    case 0x800D1488u: t1_n18_3(); return;
    case 0x800D1554u: t1_n18_4(); return;
    case 0x800D15C4u: t1_n19_0(); return;
    case 0x800D1618u: t1_n19_1(); return;
    case 0x800D16D4u: t1_n19_2(); return;
    case 0x800D1738u: t1_n19_3(); return;
    default:
        call_by_address(fn, "a sub-state of overlay 1's new account");
    }
}

void Game::t1_goto(u32 major) { m.w32(m.r32(kState) + 0x4C, major); }

void Game::t1_minor_add(s32 n) {
    const u32 st = m.r32(kState);
    m.w16(st + 0x4E, m.r16(st + 0x4E) + static_cast<u32>(n));
}

static u32 pad_bits(Memory& m, u32 at) { return m.r32(scene(m) + at); }

void Game::t1_confirm_run(u32 fn) {
    cover(fn);
    Frame f(*this, 0x18);
    const s32 v = t1_card_status(m.r8(m.r32(kState) + 0x5A));
    if (v == 2 || v == 6) {
        t1_goto(9);
        return;
    }
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        t1_goto(19);
        t1_stats_reset();
        m.w16(scene(m) + 0x954, 2);
        m.w32(scene(m) + 0x968, t1_clock());
        t1_name_to_scene();
        sound_play(0x59);
    } else if (pad & 0x20u) {
        t1_goto(7);
        sound_play(0x3B);
    }
}

void Game::t1_name_init() {
    cover(0x800CE518u);
    Frame f(*this, 0x18);
    const u32 st = m.r32(kState);
    m.w16(st + 0x10, 0);
    m.w32(st + 0x48, 0);
    m.w16(st + 0x4E, m.r16(st + 0x4E) + 1);
    t1_sprite_reset(0xB);
    t1_sprite_reset(6);
    t1_set_1c(0x85E);
    t1_fade_set(0xF, 1, 0, 0x3C);
    t1_flag(1, 0xF);
    t1_flag(4, 0x16);
    t1_cursor(0x6F, 0x96, 1, 1);
    t1_anim_push(0xE);
}

void Game::t1_name_run() {
    cover(0x800CE5ACu);
    Frame f(*this, 0x28);
    const u32 S = kState;
    t1_flag(2, m.r8(m.r32(S) + 0x5B));
    const u32 name = t1_name_buf(0);
    const s32 n = static_cast<s32>(libc_strlen(name));
    const u32 rep = pad_bits(m, kPadRepeat);
    auto key = [&]() { return m.r8(m.r32(S) + 0x5B); };
    auto set_key = [&](u32 v) { m.w8(m.r32(S) + 0x5B, v); };
    s32 snd = -1;
    if (rep & 8u) {
        const u32 k = key();
        if (k < 14) set_key(k == 13 ? 0 : k + 1);
        else if (k < 27) set_key(k == 26 ? 14 : k + 1);
        else if (k < 39) set_key(k == 38 ? 26 : k + 1);
        else if (k < 50) set_key(k == 49 ? 39 : k + 1);
        else if (k < 52) set_key(k == 51 ? 50 : k + 1);
        snd = 0x3D;
    } else if (rep & 4u) {
        const u32 k = key();
        if (k < 14) set_key(k == 0 ? 13 : k - 1);
        else if (k < 27) set_key(k == 14 ? 26 : k - 1);
        else if (k < 39) set_key(k == 27 ? 26 : k - 1);
        else if (k < 50) set_key(k == 39 ? 49 : k - 1);
        else if (k < 52) set_key(k == 50 ? 51 : k - 1);
        snd = 0x3D;
    } else if (rep & 2u) {
        const u32 k = key();
        if (k < 14) set_key(k == 13 ? k + 13 : k + 14);
        else if (k < 27) set_key(k == 26 ? k + 23 : k + 13);
        else if (k < 39) set_key(k == 38 ? k + 11 : k + 12);
        else if (k < 48) set_key(50);
        else if (k < 50) set_key(51);
        else if (k < 51) set_key(2);
        else if (k < 52) set_key(9);
        snd = 0x3D;
    } else if (rep & 1u) {
        const u32 k = key();
        if (k < 9) set_key(50);
        else if (k < 14) set_key(51);
        else if (k < 27) set_key(k - 14);
        else if (k < 39) set_key(k - 13);
        else if (k < 50) set_key(k - 12);
        else if (k < 51) set_key(41);
        else if (key() < 52) set_key(48);
        snd = 0x3D;
    } else {
        const u32 pad = pad_bits(m, kPad);
        auto finish = [&]() {
            if (n > 0) {
                m.w16(m.r32(S) + 0x4C, 1);
                m.w16(m.r32(S) + 0x4E, 0);
                snd = 0x59;
            } else {
                snd = 0x30;
            }
        };
        auto erase = [&]() {
            sound_play(0x2E);
            m.w8(name + static_cast<u32>(n - 1), 0);
        };
        if (pad & 0x40u) {
            finish();
        } else if (pad & 0x300u) {
            m.w8(m.r32(S) + 0x5C, m.r8(m.r32(S) + 0x5C) ^ 1u);
            snd = 0x2F;
        } else if (pad & 0x10u) {
            const u32 k = m.r8(m.r32(S) + 0x5B);
            if (k == 26) {
                finish();
            } else if (k < 27 && k == 13) {
                if (n > 0) erase();
                else m.w8(name + static_cast<u32>(n), 0);
            } else if (k == 49) {
                m.w8(m.r32(S) + 0x5C, m.r8(m.r32(S) + 0x5C) ^ 1u);
                snd = 0x2F;
            } else if (k == 51) {
                const u32 st = m.r32(S);
                m.w8(st + 0x5B, 0);
                m.w16(m.r32(S) + 0xC, 2);
                m.w16(m.r32(S) + 0xE, 0);
                m.w16(m.r32(S) + 0x10, 0);
                t1_sprite_reset(7);
                t1_sprite_reset(9);
                t1_set_1c(0xBE);
                sound_play(0x3B);
                heap_set(m.r32(0x800DDA30u));
            } else if (n < 8) {
                const u32 st = m.r32(S);
                const u32 c = m.r8(m.r32(st + 4 * m.r8(st + 0x5C) + 0x64) + 8 * m.r8(st + 0x5B) + 4);
                if (c == 0x20 && n == 0) {
                    snd = 0x30;
                } else {
                    m.w8(name + static_cast<u32>(n), c);
                    m.w8(name + static_cast<u32>(n) + 1, 0);
                    snd = 0x2F;
                }
            }
        } else if (pad & 0x20u) {
            if (n > 0) {
                erase();
            } else if (key() != 51) {
                set_key(51);
                snd = 0x3D;
            }
        }
    }
    if (snd >= 0) sound_play(static_cast<u32>(snd));
    const u32 st = m.r32(S);
    m.w32(st + 0x48, m.r32(st + 0x48) + 1);
}

u32 Game::t1_name_buf(s32 k) {
    cover(0x800D5970u);
    if (k == 0) return m.r32(m.r32(kCards) + 0x2F8) + 8;
    if (k < 4) return m.r32(m.r32(kCards) + 4u * static_cast<u32>(k - 1) + 0x2EC) + 8;
    return 0x800CC504u;
}

void Game::t1_cursor(u32 x, u32 y, u32 on, u32 frame) {
    cover(0x800DBF6Cu);
    const u32 t = m.r32(kScreens);
    m.w16(t + 0x50, x);
    m.w16(t + 0x52, y);
    m.w32(t + 0x54, on == 1 ? (m.r32(t + 0x54) | 1u) : (m.r32(t + 0x54) & ~1u));
    const u32 t2 = m.r32(kScreens);
    m.w32(t2 + 0x54, frame == 1 ? (m.r32(t2 + 0x54) | 4u) : (m.r32(t2 + 0x54) & ~4u));
}

void Game::t1_n1_init() {
    cover(0x800CEB20u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_sprite_reset(0xC);
    t1_set_1c(0x105E);
    t1_flag(1, 0x10);
    t1_flag(4, 0x16);
    t1_cursor(0x6F, 0x96, 1, 0);
    t1_anim_push(0xE);
}

void Game::t1_n1_run() {
    cover(0x800CEB90u);
    Frame f(*this, 0x18);
    if ((pad_bits(m, kPadHeld) & 0x10u) && (pad_bits(m, kPad) & 0x10u)) {
        m.w16(m.r32(kState) + 0x4C, 2);
        m.w16(m.r32(kState) + 0x4E, 0);
        libc_strcpy(scene(m) + 0x948, t1_name_buf(0));
        t1_pilot_reset();
        m.w32(scene(m) + 0x964, m.r32(scene(m) + 0x964) & ~1u);
        sound_play(0x59);
    }
    if ((pad_bits(m, kPadHeld) & 0x20u) && (pad_bits(m, kPad) & 0x20u)) {
        m.w16(m.r32(kState) + 0x4C, 0);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x3B);
    }
}

void Game::t1_pilot_reset() {
    cover(0x800CE3F4u);
    const u32 P = pilot(m);
    m.w8(P, 0);
    m.w8(P + 1, 6);
    m.w8(P + 2, 0);
    for (u32 i = 0; i < 60; ++i)
        for (u32 o : {0x68u, 0xE0u, 0x158u, 0x1D0u, 0x248u}) m.w16(P + o + 2 * i, 0x8CA0);
    for (u32 i = 0; i < 60; ++i)
        for (u32 o : {0x2C0u, 0x2FCu, 0x338u, 0x374u, 0x3B0u}) m.w8(P + o + i, 0);
    for (u32 i = 0; i < 60; ++i) m.w8(P + 0x3EC + i, 0);
    for (u32 i = 0; i < 180; ++i) m.w8(P + 0x43C + i, 0);
    for (u32 i = 0; i < 60; ++i) {
        m.w32(P + 0x4F0 + 4 * i, 0);
        m.w32(P + 0x5E0 + 4 * i, 0);
    }
    m.w32(P + 0x6D0, 0);
    m.w32(P + 0x6D4, 0);
    m.w32(P + 0x6D8, 0);
    for (u32 i = 0; i < 64; ++i) m.w8(P + 0x18 + i, 0);
    for (u32 i = 0; i < 16; ++i) m.w8(P + 0x58 + i, 0);
    m.w32(scene(m) + 0x96C, m.r32(scene(m) + 0x96C) & ~0x10u);
}

void Game::t1_n2_init() {
    cover(0x800CEC70u);
    Frame f(*this, 0x18);
    m.w32(m.r32(kState) + 0x48, 0);
    t1_minor_add(1);
    t1_sprite_reset(10);
    t1_set_1c(0x43E);
    t1_fade_set(0xF, 1, 0, 0x3C);
    t1_flag(1, 0xB);
    t1_flag(4, 6);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x10);
}

void Game::t1_difficulty() {
    cover(0x800CECF8u);
    Frame f(*this, 0x20);
    s32 v = m.r32s(scene(m) + 0x394);
    const u32 rep = pad_bits(m, kPadRepeat), pad = pad_bits(m, kPad);
    if (v < 0) v = 0;
    else if (v >= 3) v = 2;
    t1_prim_mode(4);
    t1_prim_rgb(0x80, 0x80, 0x80);
    t1_text_semi(2);
    t1_prim_origin(0x6A, 0xA6);
    t1_printf(0x800CBFF4u);
    t1_prim_origin(0x6A, 0xDA);
    t1_printf(0x800CBFFCu);
    t1_prim_origin(0x6A, 0x10E);
    t1_printf(0x800CC004u);
    if (pad & 0x20u) {
        m.w16(m.r32(kState) + 0x4C, 0);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x3B);
    } else if (pad & 0x10u) {
        m.w16(m.r32(kState) + 0x4C, 3);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x59);
    } else if (rep & 3u) {
        if (rep & 2u) v = (v + 1) % 3;
        if (rep & 1u) v = (v + 2) % 3;
        m.w32(scene(m) + 0x394, static_cast<u32>(v));
        sound_play(0x3D);
    }
    for (s32 i = 0; i < 3; ++i) t1_flag(6 + static_cast<u32>(i), i != v ? 1u : 0u);
    const u32 st = m.r32(kState);
    m.w32(st + 0x48, m.r32(st + 0x48) + 1);
}

void Game::t1_n3_init() {
    cover(0x800CEECCu);
    Frame f(*this, 0x18);
    const u32 st = m.r32(kState);
    m.w32(st + 0x50, 0);
    m.w32(st + 0x48, 0);
    t1_minor_add(1);
    t1_sprite_reset(13);
    t1_set_1c(0x2056);
    t1_flag(1, 0x11);
    t1_flag(4, 0x17);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x11);
}

void Game::t1_n3_run() {
    cover(0x800CEF44u);
    Frame f(*this, 0x18);
    auto go = [&](u32 major, u32 snd) {
        m.w16(m.r32(kState) + 0x4C, major);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(snd);
    };
    if ((pad_bits(m, kPadHeld) & 0x10u) && (pad_bits(m, kPad) & 0x10u)) go(4, 0x59);
    else if ((pad_bits(m, kPadHeld) & 0x20u) && (pad_bits(m, kPad) & 0x20u)) go(7, 0x3B);
    const u32 st = m.r32(kState);
    m.w32(st + 0x48, m.r32(st + 0x48) + 1);
}

void Game::t1_n4_init() {
    cover(0x800CF004u);
    Frame f(*this, 0x18);
    m.w32(m.r32(kState) + 0x48, 0);
    t1_minor_add(1);
    t1_sprite_reset(14);
    t1_set_1c(0x4016);
    t1_flag(1, 0x12);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x12);
}

u32 Game::t1_tile(u32 d, u32 p, u32 ot) {
    cover(0x800D9400u);
    m.w8(p + 3, 3);
    m.w8(p + 7, 0x60);
    m.w8(p + 7, m.r8(d + 0xB) ? 0x62u : 0x60u);
    pkt_rgb(p, m.r8(d + 8), m.r8(d + 9), m.r8(d + 0xA));
    m.w16(p + 8, m.r16(d));
    m.w16(p + 0xA, m.r16(d + 2));
    m.w16(p + 0xC, m.r16(d + 4));
    m.w16(p + 0xE, m.r16(d + 6));
    ot_link(ot, p);
    return p + 16;
}

u32 Game::t1_decoded(s32 k) {
    cover(0x800DA2A8u);
    Frame f(*this, 0x20);
    const u32 q = m.r32(kQueue);
    const u32 half = hw->spu_read_decoded_data(q + 0x18, 5);
    hw->spu_is_transfer_completed(1);
    const u32 l = half ? q + 0x18 : q + 0xE0, r = half ? q + 0x418 : q + 0x4E0;
    if (k == 0) return l;
    if (k == 1) return r;
    return 1;
}

s32 Game::t1_level() {
    cover(0x800DA340u);
    Frame f(*this, 0x20);
    u32 l = t1_decoded(0);
    u32 r = t1_decoded(1);
    s32 sl = 0, sr = 0;
    for (u32 i = 0; i < 100; ++i, l += 2, r += 2) {
        const s32 a = m.r16s(l), b = m.r16s(r);
        sl += a < 0 ? -a : a;
        sr += b < 0 ? -b : b;
    }
    const s32 v = sl / 100 + sr / 100;
    return static_cast<s32>((static_cast<u32>(v) + (static_cast<u32>(v) >> 31)) << 15) >> 16;
}

void Game::t1_sound_settings() {
    cover(0x800CF06Cu);
    Frame f(*this, 0x60);
    cover(0x800DA6FCu);
    const u32 ot = m.r32(m.r32(kScreens) + 0x10);
    const s32 level = t1_level();
    const u32 meter = f.at(0x28);
    m.w16(meter, 0x44);
    m.w16(meter + 2, 0xE0);
    m.w16(meter + 6, 0xC);
    m.w8(meter + 8, 0xC8);
    m.w8(meter + 9, 0xC8);
    m.w8(meter + 0xA, 0xC8);
    m.w16(meter + 4, static_cast<u32>(7 * ((15 * level) / 0x7FFF)));
    m.w32(m.r32(kState) + 8, t1_tile(meter, m.r32(m.r32(kState) + 8), ot));
    const bool mono = (m.r8(scene(m) + 0x39A) & 1u) != 0;
    const s32 item = m.r32s(kSoundItem);
    t1_flag(6, mono ? 2u : (item ? 1u : 0u));
    const s32 item2 = m.r32s(kSoundItem);
    t1_flag(7, (m.r8(scene(m) + 0x39A) & 1u) ? (item2 ? 1u : 0u) : 2u);
    m.w32(m.r32(0x800BCBBCu) + 0x160, (m.r8(scene(m) + 0x39A) & 1u) ? 1u : 0u);
    kcd_cd_volume(m.r32s(m.r32(0x800BCBBCu) + 0x164), m.r32(m.r32(0x800BCBBCu) + 0x160));
    const u32 d = f.at(0x10);
    auto bars = [&](u32 vol_at, u32 y, s32 which) {
        const s32 w = (221 * static_cast<s32>(m.r8(scene(m) + vol_at))) / 255;
        m.w16(d + 2, y);
        m.w8(d + 0xA, 0);
        m.w8(d + 9, 0);
        m.w8(d + 8, 0);
        m.w16(d + 4, 4);
        m.w16(d + 6, 0x40);
        m.w16(d, static_cast<u32>(w + 0x45));
        m.w32(m.r32(kState) + 8, t1_tile(d, m.r32(m.r32(kState) + 8), ot + 4));
        m.w16(d, 0x46);
        m.w16(d + 2, y);
        if (m.r32s(kSoundItem) == which) {
            m.w8(d + 8, 0xC8);
            m.w8(d + 9, 0xC8);
            m.w8(d + 0xA, 0xC8);
        } else {
            m.w8(d + 8, 8);
            m.w8(d + 9, 0xC8);
            m.w8(d + 0xA, 0xC0);
        }
        m.w16(d + 6, 0x40);
        m.w16(d + 4, static_cast<u32>((221 * static_cast<s32>(m.r8(scene(m) + vol_at))) / 255));
        m.w32(m.r32(kState) + 8, t1_tile(d, m.r32(m.r32(kState) + 8), ot + 4));
    };
    bars(0x398, 0xE5, 1);
    bars(0x399, 0x133, 2);
    const u32 rep = pad_bits(m, kPadRepeat);
    if (rep & 2u) {
        const s32 v = m.r32s(kSoundItem);
        m.w32(kSoundItem, v < 2 ? static_cast<u32>(v + 1) : 0u);
        sound_play(0x3D);
    }
    if (pad_bits(m, kPadRepeat) & 1u) {
        const s32 v = m.r32s(kSoundItem);
        m.w32(kSoundItem, v > 0 ? static_cast<u32>(v - 1) : 2u);
        sound_play(0x3D);
    }
    const u32 lr = pad_bits(m, kPadRepeat);
    if (lr & 0xCu) {
        const s32 it = m.r32s(kSoundItem);
        if (it == 0) {
            if (lr & 8u) m.w8(scene(m) + 0x39A, m.r8(scene(m) + 0x39A) ^ 1u);
            if (lr & 4u) m.w8(scene(m) + 0x39A, m.r8(scene(m) + 0x39A) ^ 1u);
            sound_play(0x3D);
        } else if (it == 1 || it == 2) {
            const u32 at = it == 1 ? 0x398u : 0x399u;
            s32 v = m.r8(scene(m) + at);
            if (lr & 8u) v += 0x10;
            if (lr & 4u) v -= 0x10;
            if (v >= 0x100) v = 0xFF;
            else if (v < 0) v = 0;
            if (static_cast<s32>(m.r8(scene(m) + at)) != v) {
                sound_play(0x2E);
                m.w8(scene(m) + at, static_cast<u32>(v));
            }
        }
    }
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        m.w16(m.r32(kState) + 0x4C, 6);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x59);
    } else if (pad & 0x20u) {
        m.w16(m.r32(kState) + 0x4C, 3);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x3B);
    }
    t1_flag(8, 0);
    t1_flag(9, 0);
    t1_flag(0xA, 0);
    t1_flag(m.r32(kSoundItem) + 8, 1);
    const u32 st = m.r32(kState);
    m.w32(st + 0x48, m.r32(st + 0x48) + 1);
}

void Game::t1_n5_init() {
    cover(0x800CF750u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_sprite_reset(15);
    t1_set_1c(0x8010);
    t1_flag(1, 0x14);
    t1_cursor(0x40, 0x32, 0, 0);
    t1_anim_push(0x13);
}

void Game::t1_screen_position() {
    cover(0x800CF7B4u);
    Frame f(*this, 0x28);
    const u32 rep = pad_bits(m, kPadRepeat);
    const u32 d = m.r32(kDisplay);
    const s32 x = m.r32s(d + 8), y = m.r32s(d + 0xC);
    s32 dx = 0, dy = 0;
    if (rep & 0xFu) {
        if (rep & 8u) dx = x < 12 ? 1 : 0;
        if ((rep & 4u) && !(x < -11)) dx = -1;
        if ((rep & 2u) && y < 8) dy = 1;
        if ((rep & 1u) && y > 0) dy = -1;
        if (dx || dy) {
            sound_play(0x2E);
            const u32 d2 = m.r32(kDisplay);
            m.w32(d2 + 8, static_cast<u32>(x + dx));
            m.w32(d2 + 0xC, static_cast<u32>(y + dy));
        }
    }
    t1_flag(6, dy == -1 ? 1u : 0u);
    t1_flag(7, dx == 1 ? 1u : 0u);
    t1_flag(8, dy == 1 ? 1u : 0u);
    t1_flag(9, dx == -1 ? 1u : 0u);
    const u32 sc = scene(m);
    if (m.r32(sc + 0x124) & 0x10u) {
        if (m.r16s(sc + 0x68) >= 0x81) {
            const u32 d2 = m.r32(kDisplay);
            m.w32(d2 + 8, 0xFFFFFFFEu);
            m.w32(d2 + 0xC, 0);
        }
    } else if (m.r16(sc + 0x60) & 0x1000u) {
        const u32 d2 = m.r32(kDisplay);
        m.w32(d2 + 8, 0xFFFFFFFEu);
        m.w32(d2 + 0xC, 0);
        sound_play(0x30);
    }
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x30u) {
        const u32 d2 = m.r32(kDisplay);
        m.w32(d2, m.r32(d2) | 2u);
        if (pad & 0x10u) {
            m.w16(m.r32(kState) + 0x4C, 7);
            m.w16(m.r32(kState) + 0x4E, 0);
            sound_play(0x59);
        } else {
            m.w16(m.r32(kState) + 0x4C, 6);
            m.w16(m.r32(kState) + 0x4E, 0);
            sound_play(0x3B);
        }
    }
}

void Game::t1_n6_init() {
    cover(0x800CFA34u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_set_1c(0x10016);
    t1_flag(1, 0x13);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x14);
}

void Game::t1_n6_run() {
    cover(0x800CFA94u);
    Frame f(*this, 0x18);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        const u32 st = m.r32(kState), d = m.r32(kDisplay);
        m.w16(st + 0x4C, 5);
        m.w16(st + 0x4E, 0);
        m.w16(st + 0xDC, m.r16(d + 8));
        m.w16(st + 0xDE, m.r16(d + 0xC));
        sound_play(0x59);
    } else if (pad & 0x20u) {
        t1_goto(4);
        sound_play(0x3B);
    }
    t1_colour_bars();
}

void Game::t1_colour_bars() {
    cover(0x800DBB30u);
    Frame f(*this, 0x58);
    const u32 d = f.at(0x10);
    const u32 c[4][3] = {{0xFF, 0, 0}, {0, 0xFF, 0}, {0, 0, 0xFF}, {0xFF, 0xFF, 0xFF}};
    const u32 x0[4] = {90, 102, 113, 129};
    for (u32 k = 0; k < 4; ++k) {
        m.w16(d + 4, x0[k] + 5);
        m.w16(d + 0xC, x0[k] + 5);
        m.w16(d, x0[k]);
        m.w16(d + 8, x0[k]);
        m.w16(d + 2, 0xD5);
        m.w16(d + 6, 0xD5);
        m.w16(d + 0xA, 0x17D);
        m.w16(d + 0xE, 0x17D);
        if (k == 0) {
            m.w8(d + 0x13, 0);
            for (u32 i = 0; i < 3; ++i) {
                m.w8(d + 0x10 + i, 0);
                m.w8(d + 0x14 + i, 0);
            }
        }
        for (u32 i = 0; i < 3; ++i) {
            m.w8(d + 0x18 + i, c[k][i]);
            m.w8(d + 0x1C + i, c[k][i]);
        }
        m.w32(m.r32(kState) + 8, t1_poly_g4(d, m.r32(m.r32(kState) + 8), m.r32(m.r32(kScreens) + 0x10)));
    }
}

void Game::t1_n7_init() {
    cover(0x800CFB2Cu);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_sprite_reset(0x11);
    t1_set_1c(0x20056);
    t1_fade_set(2, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    t1_flag(2, 1);
    t1_flag(1, 0x16);
    t1_flag(4, 0x16);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x15);
    m.w8(m.r32(kState) + 0xD1, 1);
}

void Game::t1_n7_run() {
    cover(0x800CFBD0u);
    Frame f(*this, 0x18);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        m.w16(m.r32(kState) + 0x4C, 9);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x59);
    } else if (pad & 0x20u) {
        m.w16(m.r32(kState) + 0x4C, 8);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x3B);
    }
}

void Game::t1_n8_init() {
    cover(0x800CFC38u);
    Frame f(*this, 0x18);
    t1_flag(2, 2);
    t1_flag(1, 0x17);
    t1_minor_add(1);
    t1_anim_push(0x16);
    t1_flag(4, 0x16);
}

void Game::t1_n8_run() {
    cover(0x800CFC94u);
    Frame f(*this, 0x18);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        t1_goto(19);
        t1_stats_reset();
        m.w16(scene(m) + 0x954, 2);
        m.w32(scene(m) + 0x968, t1_clock());
        t1_name_to_scene();
        sound_play(0x59);
    } else if (pad & 0x20u) {
        m.w16(m.r32(kState) + 0x4C, 0);
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x3B);
    }
}

void Game::t1_n9_init() {
    cover(0x800CFD28u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_flag(1, 0x1A);
}

void Game::t1_n9_run() {
    cover(0x800CFD60u);
    Frame f(*this, 0x18);
    const s32 a = t1_card_status(0), b = t1_card_status(1);
    if (a && a != 6 && b && b != 6) {
        t1_goto(10);
        t1_flag(4, 0x15);
    }
}

void Game::t1_n10_init() {
    cover(0x800CFDCCu);
    Frame f(*this, 0x18);
    const s32 a = t1_card_status(0), b = t1_card_status(1);
    if ((a == 2 || a == 6) && (b == 2 || b == 6)) {
        t1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
        t1_fade_set(2, 1, 0, 0x14);
        t1_minor_add(1);
        t1_flag(1, 0x18);
        t1_anim_push(0x17);
        m.w8(m.r32(kState) + 0xD1, 1);
        t1_set_1c(0x20056);
    } else {
        t1_goto(11);
    }
}

void Game::t1_n10_run() {
    cover(0x800CFEA4u);
    Frame f(*this, 0x18);
    const s32 a = t1_card_status(0), b = t1_card_status(1);
    if (!((a == 2 || a == 6) && (b == 2 || b == 6))) {
        t1_goto(9);
        return;
    }
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        t1_goto(19);
        t1_stats_reset();
        m.w16(scene(m) + 0x954, 2);
        m.w32(scene(m) + 0x968, t1_clock());
        t1_name_to_scene();
        sound_play(0x59);
    } else if (pad & 0x20u) {
        t1_goto(7);
        sound_play(0x3B);
    }
}

void Game::t1_n11_init() {
    cover(0x800CFF90u);
    Frame f(*this, 0x20);
    const s32 a = t1_card_status(0), b = t1_card_status(1);
    t1_minor_add(1);
    t1_sprite_reset(0x12);
    t1_set_1c(0x40036);
    t1_fade_set(4, 1, 0, 0x3C);
    t1_flag(1, 0x19);
    t1_flag(4, 2);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x18);
    if (a != 5 && b == 5) m.w8(m.r32(kState) + 0x5A, 1);
    m.w8(m.r32(kState) + 0xD1, 0);
}

void Game::t1_slot_select() {
    cover(0x800D0074u);
    Frame f(*this, 0x28);
    const u32 old = m.r8(m.r32(kState) + 0x5A);
    if (pad_bits(m, kPadRepeat) & 3u) m.w8(m.r32(kState) + 0x5A, old ^ 1u);
    const s32 a = t1_card_status(0), b = t1_card_status(1);
    if ((a == 2 || a == 6) && (b == 2 || b == 6)) {
        m.w8(m.r32(kState) + 0xD1, 1);
        t1_goto(10);
        t1_sprite_reset(0x11);
        t1_set_1c(0x20056);
        t1_fade_set(2, 1, 0, 0x14);
        t1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
        return;
    }
    auto slot = [&]() { return m.r8(m.r32(kState) + 0x5A); };
    if (!a || a == 2 || a == 6) m.w8(m.r32(kState) + 0x5A, 1);
    if (!b || b == 2 || b == 6) m.w8(m.r32(kState) + 0x5A, 0);
    if (slot() != old) sound_play(0x3D);
    if (!slot()) {
        t1_flag(6, 0);
        t1_flag(7, 1);
    }
    if (slot() == 1) {
        t1_flag(6, 1);
        t1_flag(7, 0);
    }
    if (!a || a == 2) t1_flag(6, 2);
    if (!b || b == 2) t1_flag(7, 2);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        t1_goto(12);
        sound_play(0x59);
        t1_anim_push(0x19);
    } else if (pad & 0x20u) {
        t1_goto(7);
        sound_play(0x3B);
    }
}

void Game::t1_n12_init() {
    cover(0x800D02DCu);
    Frame f(*this, 0x18);
    t1_flag(1, 0x1A);
    if (t1_card_status(m.r8(m.r32(kState) + 0x5A)) != 1) {
        m.w16(m.r32(kState) + 0x4C, 13);
        return;
    }
    t1_minor_add(1);
    t1_sprite_reset(0x11);
    t1_set_1c(0x20056);
    t1_flag(1, 0x1C);
    t1_flag(4, 0x15);
    t1_anim_push(0x1A);
}

void Game::t1_n13_init() {
    cover(0x800D0460u);
    Frame f(*this, 0x18);
    if (t1_card_status(m.r8(m.r32(kState) + 0x5A)) != 3) {
        m.w16(m.r32(kState) + 0x4C, 14);
        return;
    }
    t1_minor_add(1);
    t1_sprite_reset(0x13);
    t1_set_1c(0x80056);
    t1_fade_set(1, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    t1_fade_set(2, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    t1_fade_set(3, 3, 0xFFFFFFFFu, 0xFFFFFFFFu);
    t1_fade_set(4, 1, 0, 0x14);
    t1_flag(1, 0x1E);
    t1_flag(4, 0x15);
    t1_anim_push(0x1B);
}

void Game::t1_n13_run() {
    cover(0x800D0548u);
    Frame f(*this, 0x18);
    const s32 v = t1_card_status(m.r8(m.r32(kState) + 0x5A));
    if (v == 2 || v == 6) {
        t1_goto(9);
        return;
    }
    if (!v) return;
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x40u) {
        t1_flag(1, 0x20);
        t1_minor_add(1);
        sound_play(0x59);
        t1_anim_push(0x1C);
    } else if (pad & 0x20u) {
        t1_goto(7);
        sound_play(0x3B);
    }
}

void Game::t1_n13_format() {
    cover(0x800D061Cu);
    Frame f(*this, 0x18);
    if (!t1_card_format(m.r8(m.r32(kState) + 0x5A))) return;
    m.w16(m.r32(kState) + 0x4C, 14);
    m.w16(m.r32(kState) + 0x4E, 3);
}

void Game::t1_n14_0() {
    cover(0x800D0668u);
    Frame f(*this, 0x18);
    if (t1_card_status(m.r8(m.r32(kState) + 0x5A)) == 4) {
        t1_minor_add(1);
        return;
    }
    m.w16(m.r32(kState) + 0x4C, 15);
    t1_flag(1, 0x1A);
}

void Game::t1_n14_1() {
    cover(0x800D06D4u);
    Frame f(*this, 0x18);
    const s32 v = t1_card_free(m.r8(m.r32(kState) + 0x5A));
    if (v == -1) return;
    if (v >= 2) {
        t1_minor_add(2);
        return;
    }
    t1_minor_add(1);
    t1_flag(1, 0x22);
    t1_anim_push(0x1E);
    t1_set_1c(0x20016);
}

void Game::t1_n14_3() {
    cover(0x800D0848u);
    Frame f(*this, 0x18);
    const s32 v = t1_save_create(m.r8(m.r32(kState) + 0x5A));
    if (v == 1) {
        t1_minor_add(1);
    } else if (v == -1) {
        t1_minor_add(3);
        t1_flag(1, 0x23);
        t1_anim_push(0x20);
        t1_set_1c(0x20016);
    }
}

void Game::t1_n14_4() {
    cover(0x800D08DCu);
    Frame f(*this, 0x20);
    const s32 v = t1_save_header(m.r8(m.r32(kState) + 0x5A), 1);
    if (v == -1) {
        t1_minor_add(2);
        t1_flag(1, 0x23);
        t1_anim_push(0x20);
        t1_set_1c(0x20016);
    }
    if (v == 1) t1_minor_add(1);
}

void Game::t1_n14_5() {
    cover(0x800D0978u);
    Frame f(*this, 0x20);
    const s32 v = t1_save_dir(m.r8(m.r32(kState) + 0x5A), -1, 1);
    if (v == -1) {
        t1_minor_add(1);
        t1_flag(1, 0x23);
        t1_anim_push(0x20);
        t1_set_1c(0x20016);
    }
    if (v == 1) t1_goto(16);
}

void Game::t1_n15_1() {
    cover(0x800D0B0Cu);
    Frame f(*this, 0x20);
    const s32 v = t1_save_read_dir(m.r8(m.r32(kState) + 0x5A), 1);
    auto fail = [&](u32 msg) {
        t1_minor_add(2);
        t1_flag(1, msg);
        t1_set_1c(0x20056);
        t1_anim_push(0x20);
        t1_flag(4, 0x15);
    };
    if (v == -1) fail(0x23);
    if (v == 2) fail(0x25);
    if (v == 1) t1_minor_add(1);
}

void Game::t1_n15_2() {
    cover(0x800D0BFCu);
    Frame f(*this, 0x20);
    const s32 v = t1_save_read_pilots(m.r8(m.r32(kState) + 0x5A), 1);
    if (v == -1) {
        t1_minor_add(1);
        t1_flag(1, 0x23);
        t1_set_1c(0x20056);
        t1_anim_push(0x20);
        t1_flag(4, 0x15);
    }
    if (v == 2) {
        t1_minor_add(1);
        t1_flag(1, 0x25);
        t1_sprite_reset(6);
        t1_set_1c(0x20056);
        t1_anim_push(0x20);
        t1_flag(4, 0x15);
    }
    if (v == 1) t1_goto(16);
}

void Game::t1_n16_init() {
    cover(0x800D0DC8u);
    Frame f(*this, 0x18);
    m.w32(m.r32(kState) + 0x48, 0);
    t1_minor_add(1);
    t1_sprite_reset(0x14);
    t1_set_1c(0x100056);
    t1_flag(1, 0x24);
    t1_flag(4, 0x16);
    t1_list_mode(1);
    m.w8(m.r32(kState) + 0xD2, 0);
    t1_cursor(0x40, 0x32, 1, 0);
    t1_anim_push(0x1F);
}

void Game::t1_pilot_select() {
    cover(0x800D0E54u);
    Frame f(*this, 0x18);
    const s32 v = t1_card_status(m.r8(m.r32(kState) + 0x5A));
    if (v == 2 || v == 6) {
        t1_goto(9);
        t1_list_mode(0);
        return;
    }
    if (pad_bits(m, kPadRepeat) & 2u) {
        const u32 k = m.r8(m.r32(kState) + 0x5D);
        m.w8(m.r32(kState) + 0x5D, k >= 2 ? 0u : k + 1);
        sound_play(0x3D);
    }
    if (pad_bits(m, kPadRepeat) & 1u) {
        const u32 k = m.r8(m.r32(kState) + 0x5D);
        m.w8(m.r32(kState) + 0x5D, k ? k - 1 : 2u);
        sound_play(0x3D);
    }
    const u32 k = m.r8(m.r32(kState) + 0x5D);
    t1_flag(6, k != 0 ? 1u : 0u);
    t1_flag(7, m.r8(m.r32(kState) + 0x5D) != 1 ? 1u : 0u);
    t1_flag(8, m.r8(m.r32(kState) + 0x5D) != 2 ? 1u : 0u);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x40u) {
        if (t1_pilot_saved(m.r8(m.r32(kState) + 0x5D))) {
            m.w16(m.r32(kState) + 0x4C, 17);
        } else {
            m.w16(m.r32(kState) + 0x4C, 18);
            m.w8(m.r32(kState) + 0xD2, 1);
        }
        m.w16(m.r32(kState) + 0x4E, 0);
        sound_play(0x59);
    } else if (pad & 0x20u) {
        t1_goto(11);
        t1_list_mode(0);
        sound_play(0x3B);
    }
}

void Game::t1_n17_init() {
    cover(0x800D1080u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_sprite_reset(0x15);
    t1_set_1c(0x200056);
    t1_flag(1, 0x26);
    t1_flag(4, 0x18);
    t1_fade_set(m.r8(m.r32(kState) + 0x5D) + 6, 1, 0, 10);
    t1_list_mode(2);
    t1_anim_push(0x21);
}

void Game::t1_n17_run() {
    cover(0x800D1108u);
    Frame f(*this, 0x18);
    const s32 v = t1_card_status(m.r8(m.r32(kState) + 0x5A));
    if (v == 2 || v == 6) {
        t1_goto(9);
        t1_list_mode(0);
        return;
    }
    t1_flag(6, m.r8(m.r32(kState) + 0x5D) != 0 ? 1u : 0u);
    t1_flag(7, m.r8(m.r32(kState) + 0x5D) != 1 ? 1u : 0u);
    t1_flag(8, m.r8(m.r32(kState) + 0x5D) != 2 ? 1u : 0u);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x40u) {
        const u32 k = m.r8(m.r32(kState) + 0x5D);
        t1_goto(18);
        t1_fade_set(k + 6, 0, 0, 0);
        sound_play(0x59);
    } else if (pad & 0x20u) {
        const u32 k = m.r8(m.r32(kState) + 0x5D);
        t1_goto(16);
        t1_fade_set(k + 6, 0, 0, 0);
        sound_play(0x3B);
    }
    const u32 st = m.r32(kState);
    m.w32(st + 0x48, m.r32(st + 0x48) + 1);
}

void Game::t1_n18_0() {
    cover(0x800D12A4u);
    Frame f(*this, 0x18);
    t1_minor_add(1);
    t1_flag(1, 0x27);
    t1_flag(4, 0x18);
    m.w32(scene(m) + 0x968, t1_clock());
    t1_name_to_scene();
}

void Game::t1_n18_1() {
    cover(0x800D1300u);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kState);
    const s32 v = t1_save_dir(m.r8(st + 0x5A), static_cast<s32>(m.r8(st + 0x5D)), 1);
    if (v == -1) {
        t1_minor_add(2);
        t1_flag(1, 0x28);
        t1_set_1c(0x20016);
        t1_anim_push(0x23);
        t1_list_mode(0);
    }
    if (v == 1) {
        t1_name_to_scene();
        t1_pilot_serialise();
        t1_minor_add(1);
    }
}

void Game::t1_n18_2() {
    cover(0x800D13B8u);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kState);
    const s32 v = t1_save_pilot(m.r8(st + 0x5A), m.r8(st + 0x5D), 6, 1);
    if (v == -1) {
        t1_minor_add(1);
        t1_flag(1, 0x28);
        t1_set_1c(0x20056);
        t1_anim_push(0x23);
        t1_list_mode(0);
    }
    if (v == 1) {
        const u32 s2 = m.r32(kState);
        const u32 minor = m.r16(s2 + 0x4E);
        m.w32(s2 + 0x88, 120);
        m.w16(s2 + 0x4E, minor + 2);
        t1_stats_reset();
        m.w16(scene(m) + 0x954, 2);
        m.w16(m.r32(kState) + 0x10, 0);
        t1_anim_push(0x24);
    }
}

void Game::t1_n18_3() {
    cover(0x800D1488u);
    Frame f(*this, 0x20);
    const s32 v = t1_card_status(m.r8(m.r32(kState) + 0x5A));
    if (v == 2 || v == 6) {
        m.w16(m.r32(kState) + 0x4C, 9);
        m.w16(m.r32(kState) + 0x4E, 0);
        return;
    }
    t1_flag(1, 0x28);
    const u32 pad = pad_bits(m, kPad);
    if (pad & 0x10u) {
        t1_goto(19);
        t1_stats_reset();
        m.w16(scene(m) + 0x954, 2);
        m.w16(m.r32(kState) + 0x10, 0);
    } else if (pad & 0x20u) {
        m.w16(m.r32(kState) + 0x4C, 11);
        m.w16(m.r32(kState) + 0x4E, 0);
    }
}

void Game::t1_n18_4() {
    cover(0x800D1554u);
    Frame f(*this, 0x18);
    t1_flag(1, 0x2A);
    const s32 t = m.r32s(m.r32(kState) + 0x88);
    if (t < 0 || (pad_bits(m, kPad) & 0x30u)) {
        t1_goto(19);
        return;
    }
    m.w32(m.r32(kState) + 0x88, static_cast<u32>(t - 1));
}

void Game::t1_n19_0() {
    cover(0x800D15C4u);
    Frame f(*this, 0x18);
    const u32 st = m.r32(kState);
    m.w32(st + 0xD4, 0);
    m.w16(st + 0x4E, m.r16(st + 0x4E) + 1);
    m.w16(scene(m) + 0x38C, 1);
    t1_cursor(0x6F, 0x96, 0, 1);
}

void Game::t1_n19_1() {
    cover(0x800D1618u);
    Frame f(*this, 0x18);
    const s32 v = disc_number();
    if (v == 0) t1_flag(1, 0x49);
    if (v == 3 || v == 1) {
        m.w32(m.r32(kState) + 0xC, 6);
        return;
    }
    if (v == 4 || v == -1 || v == 2) {
        t1_flag(1, m.r32(m.r32(kState) + 0xD4) + 0x46);
        kcd_open_drive();
        t1_minor_add(1);
        t1_anim_push(0x25);
    }
}

void Game::t1_n19_2() {
    cover(0x800D16D4u);
    Frame f(*this, 0x18);
    t1_set_1c(0x400036);
    t1_flag(4, 4);
    t1_list_mode(0);
    if (kcd_drive_state() == 3) t1_minor_add(1);
}

void Game::t1_n19_3() {
    cover(0x800D1738u);
    Frame f(*this, 0x18);
    if (kcd_drive_state() != 1) return;
    t1_flag(1, 0x49);
    t1_minor_add(-2);
    t1_anim_push(0x26);
    m.w32(m.r32(kState) + 0xD4, 0x1F);
}

void Game::kcd_open_drive() {
    cover(0x8002B428u);
    Frame f(*this, 0x18);
    const u32 k = m.r32(0x800BCBA8u);
    if (m.r32(k)) return;
    kcd_stop(1);
    const u32 k2 = m.r32(0x800BCBA8u);
    m.w32(k2, 2);
    m.w32(k2 + 4, 0);
}

u32 Game::kcd_drive_state() {
    cover(0x8002B528u);
    const u32 k = m.r32(0x800BCBA8u);
    const u32 s = m.r32(k);
    if (s == 2) return m.r32(k + 4) == s ? 2u : 1u;
    if (s == 3) return m.r32(k + 4) ? 1u : 3u;
    return s == 4 ? 1u : 0u;
}

u32 Game::t1_clock() {
    cover(0x800D4140u);
    return m.r32(scene(m) + 0x20);
}

void Game::t1_name_to_scene() {
    cover(0x800D4158u);
    Frame f(*this, 0x18);
    libc_strcpy(scene(m) + 0x948, m.r32(m.r32(kCards) + 0x2F8) + 8);
}

void Game::t1_stats_reset() {
    cover(0x800DA1B8u);
    const u32 P = pilot(m);
    for (u32 i = 0; i < 180; ++i) m.w8(P + 0x43C + i, 0);
    for (u32 i = 0; i < 80; ++i) m.w8(P + 0x3EC + i, 0);
    for (u32 i = 0; i < 60; ++i) {
        for (u32 o : {0x374u, 0x338u, 0x2FCu, 0x2C0u, 0x3B0u}) m.w8(P + i + o, 0);
        for (u32 o : {0x1D0u, 0x158u, 0xE0u, 0x68u, 0x248u}) m.w16(P + o + 2 * i, 0xD2F0);
        m.w32(P + 0x4F0 + 4 * i, 0);
        m.w32(P + 0x5E0 + 4 * i, 0);
    }
    m.w32(P + 0x6D0, 0);
    m.w32(P + 0x6D4, 0);
    m.w32(P + 0x6D8, 0);
}

void Game::t1_pilot_serialise() {
    cover(0x800D41C8u);
    Frame f(*this, 8);
    const u32 p = m.r32(m.r32(kCards) + 0x2F8), sc = scene(m), P = pilot(m);
    m.w32(p + 4, m.r32(sc + 0x968));
    for (u32 k = 0; k < 6; ++k) {
        const u32 t = p + 0x2C4 + 0x44 * k, s = sc + 0x5C * k;
        for (u32 i = 0; i < 20; ++i) m.w16(t + 2 * i, m.r16(s + 0x172 + 2 * i));
        for (u32 o = 0; o < 0x1C; o += 4) m.w32(t + 0x28 + o, m.r32(s + 0x19C + o));
        m.w8(p + 0x464, (m.r32(sc + 0x120) & 4u) ? 0u : 1u);
    }
    for (u32 j = 0; j < 8; ++j) {
        m.w8(p + 0x1A + j, 0);
        for (u32 b = 0; b < 8; ++b) m.w8(p + 0x1A + j, m.r8(p + 0x1A + j) | ((m.r8(P + 8 * j + b + 0x18) & 1u) << b));
    }
    for (u32 j = 0; j < 2; ++j) {
        m.w8(p + 0x18 + j, 0);
        for (u32 b = 0; b < 8; ++b) m.w8(p + 0x18 + j, m.r8(p + 0x18 + j) | ((m.r8(P + 8 * j + b + 0x58) & 1u) << b));
    }
    for (u32 i = 0; i < 60; ++i) {
        m.w16(p + 0x22 + 2 * i, m.r16(P + 0x68 + 2 * i));
        m.w16(p + 0x9A + 2 * i, m.r16(P + 0xE0 + 2 * i));
        m.w16(p + 0x112 + 2 * i, m.r16(P + 0x158 + 2 * i));
        m.w16(p + 0x18A + 2 * i, m.r16(P + 0x1D0 + 2 * i));
    }
    for (u32 j = 0; j < 30; ++j) {
        const u32 o[4] = {0x2C0, 0x2FC, 0x338, 0x374}, d[4] = {0x202, 0x220, 0x23E, 0x25C};
        for (u32 k = 0; k < 4; ++k)
            m.w8(p + d[k] + j, (m.r8(P + o[k] + 2 * j) & 0xFu) | ((m.r8(P + o[k] + 2 * j + 1) & 0xFu) << 4));
    }
    for (u32 j = 0; j < 20; ++j) {
        m.w8(p + 0x27A + j, 0);
        for (u32 b = 0; b < 4; ++b)
            m.w8(p + 0x27A + j, m.r8(p + 0x27A + j) | ((m.r8(P + 4 * j + b + 0x3EC) & 3u) << (2 * b)));
    }
    for (u32 j = 0; j < 45; ++j) {
        m.w8(p + 0x28E + j, 0);
        for (u32 b = 0; b < 4; ++b)
            m.w8(p + 0x28E + j, m.r8(p + 0x28E + j) | ((m.r8(P + 4 * j + b + 0x43C) & 3u) << (2 * b)));
    }
    const u32 d = m.r32(kDisplay);
    m.w16(p + 0x460, m.r16(d + 8));
    m.w16(p + 0x462, m.r16(d + 0xC));
    m.w8(p + 0x45D, m.r8(sc + 0x398));
    m.w8(p + 0x45E, m.r8(sc + 0x399));
    m.w8(p + 0x45C, m.r8(sc + 0x39A));
    m.w16(p + 0x2C0, m.r16(P + 0x6D4));
    m.w16(p + 0x2C2, m.r16(P + 0x6D8));
    m.w32(p + 0x468, 0);
    m.w16(p + 0x14, m.r8(P + 1));
    if (m.r8(sc + 0x39C) & 1u) m.w32(p + 0x468, 1);
    if (m.r32(sc + 0x120) & 1u) m.w32(p + 0x468, m.r32(p + 0x468) | 2u);
    if (m.r32(sc + 0x120) & 2u) m.w32(p + 0x468, m.r32(p + 0x468) | 4u);
    if (m.r8(sc + 0x95A) == 1) m.w32(p + 0x468, m.r32(p + 0x468) | 8u);
    for (s32 i = 16; i >= 0; --i) m.w8(p + 0x46C + static_cast<u32>(i), 0);
}

s32 Game::t1_pilot_saved(u32 k) {
    cover(0x800D5A38u);
    if (k >= 3) return -1;
    return static_cast<s32>(m.r8(m.r32(m.r32(kCards) + 0x2E8) + 8) >> k) & 1;
}

u32 Game::t1_pilots_saved() {
    cover(0x800D5C80u);
    return m.r8(m.r32(m.r32(kCards) + 0x2E8) + 8);
}

void Game::t1_list_mode(u32 mode) {
    cover(0x800DC028u);
    const u32 t = m.r32(kScreens);
    if (mode == 2) m.w32(t + 0x54, m.r32(t + 0x54) | 0xAu);
    else if (mode == 1) m.w32(t + 0x54, (m.r32(t + 0x54) | 2u) & ~8u);
    else m.w32(t + 0x54, m.r32(t + 0x54) & ~0xAu);
}

static u32 save_path(Memory& m, u32 slot) { return m.r32(kCards) + 26 * slot + 0x2B4; }

void Game::t1_io_begin() {
    const u32 b = m.r32(kCards);
    if (!m.r16(b + 0x35A)) {
        m.w16(b + 0x35A, 1);
        m.w16(b + 0x358, 0);
    }
}

s32 Game::t1_io_retry() {
    const u32 b = m.r32(kCards);
    if (m.r16s(b + 0x358) < 3) {
        m.w16(b + 0x358, m.r16(b + 0x358) + 1);
        return 0;
    }
    t1_card_flags_reset();
    m.w16(m.r32(kCards) + 0x35A, 0);
    return -1;
}

u32 Game::t1_save_sum(u32 p, u32 n) {
    u32 v = kSaveSum;
    for (u32 i = 0; i <= n; ++i) v ^= m.r32(p + 4 + 4 * i);
    return v;
}

s32 Game::t1_card_format(u32 slot) {
    cover(0x800D5E74u);
    Frame f(*this, 0x18);
    if (!t1_cards_idle()) return 0;
    unported(0x800AD808u, "libcard's _card_format");
    (void)slot;
    t1_card_flags_reset();
    return -1;
}

s32 Game::t1_save_create(u32 slot) {
    cover(0x800D5EC4u);
    Frame f(*this, 0x20);
    if (!t1_cards_idle()) return 0;
    const u32 fd = hw->file_open(save_path(m, slot), 0x20200);
    m.w32(m.r32(kCards) + 0x3C, fd);
    hw->file_close(fd);
    t1_card_flags_reset();
    return fd == 0xFFFFFFFFu ? -1 : 1;
}

s32 Game::t1_card_free(u32 slot) {
    cover(0x800D64C4u);
    Frame f(*this, 0x68);
    if (!t1_cards_idle()) return -1;
    m.w16(m.r32(kCards) + 2 * slot + 0x54, 15);
    const u32 pat = f.at(0x10), ent = f.at(0x20);
    libc_sprintf(pat, 0x800CC55Cu, {slot});
    u32 n = 0;
    if (hw->first_file(pat, ent)) {
        u32 at = 0;
        do {
            libc_strcpy(m.r32(kCards) + 300 * slot + 92 + at, ent);
            const s32 size = m.r32s(ent + 0x18);
            s32 blocks = size + 0x1FFF;
            if (blocks < 0) blocks = size + 0x3FFE;
            at += 20;
            ++n;
            const u32 b = m.r32(kCards) + 2 * slot + 0x54;
            m.w16(b, m.r16(b) - static_cast<u32>(blocks >> 13));
        } while (hw->next_file(ent));
    }
    m.w16(m.r32(kCards) + 2 * slot + 0x58, n);
    t1_card_flags_reset();
    return m.r16s(m.r32(kCards) + 2 * slot + 0x54);
}

s32 Game::t1_io_end(u32 slot, u32 reset, bool keep_on) {
    const s32 r = t1_card_wait(4, slot, 0);
    if (r == -1) return 0;
    hw->file_close(m.r32(m.r32(kCards) + 0x3C));
    m.w8(m.r32(kCards) + 0x336, 0);
    if (r) {
        const u32 b = m.r32(kCards);
        if (m.r16s(b + 0x358) < 3) {
            m.w16(b + 0x358, m.r16(b + 0x358) + 1);
            return 0;
        }
        m.w16(m.r32(kCards) + 0x35A, 0);
        t1_card_flags_reset();
        return -1;
    }
    if (!keep_on) m.w16(m.r32(kCards) + 0x35A, 0);
    if (reset == 1) t1_card_flags_reset();
    return 1;
}

s32 Game::t1_save_header(u32 slot, u32 reset) {
    cover(0x800D6D78u);
    Frame f(*this, 0x20);
    if (!t1_cards_idle()) return 0;
    t1_io_begin();
    if (!m.r8(m.r32(kCards) + 0x336)) {
        const u32 fd = hw->file_open(save_path(m, slot), 0x8002);
        m.w32(m.r32(kCards) + 0x3C, fd);
        if (fd == 0xFFFFFFFFu) return t1_io_retry();
        hw->file_write(fd, m.r32(kAccount), 640);
        m.w8(m.r32(kCards) + 0x336, 1);
    }
    return t1_io_end(slot, reset, true);
}

s32 Game::t1_save_dir(u32 slot, s32 k, u32 reset) {
    cover(0x800D6F04u);
    Frame f(*this, 0x20);
    if (!t1_cards_idle()) return 0;
    t1_io_begin();
    if (!m.r8(m.r32(kCards) + 0x336)) {
        const u32 d = m.r32(m.r32(kCards) + 0x2E8);
        if (k < 0) {
            m.w8(d + 8, 0);
            m.w32(m.r32(m.r32(kCards) + 0x2E8) + 0xC, 0);
        } else {
            m.w8(d + 8, m.r8(d + 8) | (1u << (static_cast<u32>(k) & 31u)));
            const u32 d2 = m.r32(m.r32(kCards) + 0x2E8);
            m.w32(d2 + 0xC, m.r32(d2 + 0xC) + 1);
        }
        const u32 d3 = m.r32(m.r32(kCards) + 0x2E8);
        m.w32(d3, t1_save_sum(d3, 30));
        const u32 fd = hw->file_open(save_path(m, slot), 0x8002);
        m.w32(m.r32(kCards) + 0x3C, fd);
        if (fd == 0xFFFFFFFFu) return t1_io_retry();
        hw->file_lseek(fd, 640, 0);
        hw->file_write(m.r32(m.r32(kCards) + 0x3C), m.r32(m.r32(kCards) + 0x2E8), 128);
        m.w8(m.r32(kCards) + 0x336, 1);
    }
    return t1_io_end(slot, reset, true);
}

s32 Game::t1_save_pilot(u32 slot, u32 k, u32 rank, u32 reset) {
    cover(0x800D713Cu);
    Frame f(*this, 0x28);
    if (!t1_cards_idle()) return 0;
    t1_io_begin();
    if (rank < 6) {
        const u32 b = m.r32(kCards);
        m.w16(m.r32(b + 0x2F8) + 0x14, rank);
        m.w16(m.r32(b + 0x2F8) + 0x16, m.r16(m.r32(b + 4 * k + 0x2EC) + 0x16));
        m.w16(m.r32(b + 0x2F8) + 0x16, m.r16(m.r32(b + 0x2F8) + 0x16) | (1u << rank));
    } else if (rank == 6) {
        const u32 b = m.r32(kCards);
        m.w16(m.r32(b + 0x2F8) + 0x14, 0);
        m.w16(m.r32(b + 0x2F8) + 0x16, 0);
    }
    if (!m.r8(m.r32(kCards) + 0x336)) {
        const u32 p = m.r32(m.r32(kCards) + 0x2F8);
        m.w32(p, t1_save_sum(p, 286));
        const u32 fd = hw->file_open(save_path(m, slot), 0x8002);
        m.w32(m.r32(kCards) + 0x3C, fd);
        if (fd == 0xFFFFFFFFu) return t1_io_retry();
        hw->file_lseek(fd, 1152 * k + 768, 0);
        hw->file_write(m.r32(m.r32(kCards) + 0x3C), m.r32(m.r32(kCards) + 0x2F8), 1152);
        m.w8(m.r32(kCards) + 0x336, 1);
    }
    return t1_io_end(slot, reset, true);
}

s32 Game::t1_save_read_dir(u32 slot, u32 reset) {
    cover(0x800D6604u);
    Frame f(*this, 0x20);
    if (!t1_cards_idle()) return 0;
    t1_io_begin();
    if (!m.r8(m.r32(kCards) + 0x336)) {
        const u32 fd = hw->file_open(save_path(m, slot), 0x8001);
        m.w32(m.r32(kCards) + 0x3C, fd);
        if (fd == 0xFFFFFFFFu) {
            const u32 b = m.r32(kCards);
            if (m.r16s(b + 0x358) < 3) {
                m.w16(b + 0x358, m.r16(b + 0x358) + 1);
                return 0;
            }
            t1_card_flags_reset();
            m.w16(m.r32(kCards) + 0x35A, 0);
            return -1;
        }
        hw->file_lseek(fd, 640, 0);
        hw->file_read(m.r32(m.r32(kCards) + 0x3C), m.r32(m.r32(kCards) + 0x2E8), 128);
        m.w8(m.r32(kCards) + 0x336, 1);
    }
    const s32 r = t1_card_wait(4, slot, 0);
    if (r == -1) return 0;
    hw->file_close(m.r32(m.r32(kCards) + 0x3C));
    m.w8(m.r32(kCards) + 0x336, 0);
    const u32 d = m.r32(m.r32(kCards) + 0x2E8);
    if (m.r32(d) == t1_save_sum(d, 30) && m.r32(d + 4) == 0x990426u) {
        if (r) {
            const u32 b = m.r32(kCards);
            if (m.r16s(b + 0x358) < 3) {
                m.w16(b + 0x358, m.r16(b + 0x358) + 1);
                return 0;
            }
            m.w16(m.r32(kCards) + 0x35A, 0);
            t1_card_flags_reset();
            return -1;
        }
        m.w16(m.r32(kCards) + 0x35A, 0);
        if (reset == 1) t1_card_flags_reset();
        return 1;
    }
    t1_card_flags_reset();
    m.w16(m.r32(kCards) + 0x35A, 0);
    return 2;
}

s32 Game::t1_save_read_pilots(u32 slot, u32 reset) {
    cover(0x800D6814u);
    Frame f(*this, 0x20);
    if (!t1_cards_idle()) return 0;
    const u32 b0 = m.r32(kCards);
    if ((m.r8(m.r32(b0 + 0x2E8) + 8) >> (m.r16(b0 + 0x50) & 31u)) & 1u) {
        if (!t1_cards_idle()) return 0;
        t1_io_begin();
        auto fail = [&](s32 r) {
            m.w16(m.r32(kCards) + 0x35A, 0);
            m.w16(m.r32(kCards) + 0x50, 0);
            return r;
        };
        auto retry = [&]() -> s32 {
            const u32 b = m.r32(kCards);
            if (m.r16s(b + 0x358) < 3) {
                m.w16(b + 0x358, m.r16(b + 0x358) + 1);
                return 0;
            }
            t1_card_flags_reset();
            return fail(-1);
        };
        if (!m.r8(m.r32(kCards) + 0x336)) {
            const u32 fd = hw->file_open(save_path(m, slot), 0x8001);
            m.w32(m.r32(kCards) + 0x3C, fd);
            if (fd == 0xFFFFFFFFu) return retry();
            const u32 b = m.r32(kCards);
            hw->file_lseek(fd, 1152u * static_cast<u32>(m.r16s(b + 0x50)) + 768, 0);
            const u32 b2 = m.r32(kCards);
            hw->file_read(m.r32(b2 + 0x3C), m.r32(b2 + 4u * static_cast<u32>(m.r16s(b2 + 0x50)) + 0x2EC), 1152);
            m.w8(m.r32(kCards) + 0x336, 1);
        }
        const s32 r = t1_card_wait(4, slot, 0);
        if (r == -1) return 0;
        hw->file_close(m.r32(m.r32(kCards) + 0x3C));
        m.w8(m.r32(kCards) + 0x336, 0);
        if (r) return retry();
        const u32 b = m.r32(kCards);
        const u32 p = m.r32(b + 4u * static_cast<u32>(m.r16s(b + 0x50)) + 0x2EC);
        if (m.r32(p) != t1_save_sum(p, 286)) {
            t1_card_flags_reset();
            return fail(2);
        }
    }
    const u32 b = m.r32(kCards);
    if (m.r16s(b + 0x50) < 2) {
        m.w16(b + 0x50, m.r16(b + 0x50) + 1);
        return 0;
    }
    m.w16(b + 0x50, 0);
    if (reset == 1) t1_card_flags_reset();
    return 1;
}

void Game::t1_new_pilot() {
    cover(0x800DBFE4u);
    Frame f(*this, 0x18);
    const u32 t = m.r32(kScreens);
    if (m.r32(t + 0x54) & 1u) t1_pilot_name(m.r16s(t + 0x50), m.r16s(t + 0x52), 0);
}

void Game::t1_pilot_name(s32 x, s32 y, s32 k) {
    cover(0x800DBD20u);
    Frame f(*this, 0x38);
    t1_prim_mode(4);
    t1_text_semi(2);
    t1_text_fold(1, 0x1E, 0x28);
    t1_text_fold_at(static_cast<u32>(t1_fade_value(0xF)));
    t1_prim_rgb(0x80, 0x80, 0x80);
    t1_prim_origin(static_cast<u32>(static_cast<s16>(x)), static_cast<u32>(static_cast<s16>(y)));
    t1_printf(t1_name_buf(k));
    if (!(m.r32(m.r32(kScreens) + 0x54) & 4u)) return;
    const u32 d = f.at(0x10), pen = f.at(0x20);
    m.w8(d + 0xB, 0);
    m.w8(d + 8, 0x28);
    m.w8(d + 9, 0x28);
    m.w8(d + 0xA, 0x28);
    cover(0x800D8094u);
    m.w16(pen, m.r16(m.r32(0x800DDA58u) + 0x20));
    m.w16(pen + 2, m.r16(m.r32(0x800DDA58u) + 0x22));
    const u32 X = m.r16(pen), Y = m.r16(pen + 2);
    const u32 seg[6][4] = {{X, Y, X + 8, Y},          {X, Y + 9, X + 8, Y + 9}, {X, Y + 18, X + 8, Y + 18},
                           {X, Y, X, Y + 18},          {X + 4, Y, X + 4, Y + 18}, {X + 8, Y, X + 8, Y + 18}};
    for (const auto& s : seg) {
        m.w16(d, s[0]);
        m.w16(d + 2, s[1]);
        m.w16(d + 4, s[2]);
        m.w16(d + 6, s[3]);
        m.w32(m.r32(kState) + 8, t1_line2(d, m.r32(m.r32(kState) + 8), m.r32(m.r32(kScreens) + 0x10)));
    }
}

u32 Game::t1_line2(u32 d, u32 p, u32 ot) {
    cover(0x800D90E8u);
    Frame f(*this, 0x20);
    p = t1_line(d, p, ot);
    m.w16(d + 2, m.r16(d + 2) + 1);
    m.w16(d + 6, m.r16(d + 6) + 1);
    return t1_line(d, p, ot);
}

u32 Game::t1_line(u32 d, u32 p, u32 ot) {
    cover(0x800D9024u);
    m.w8(p + 3, 3);
    m.w8(p + 7, 0x40);
    m.w8(p + 7, m.r8(d + 0xB) ? 0x42u : 0x40u);
    pkt_rgb(p, m.r8(d + 8), m.r8(d + 9), m.r8(d + 0xA));
    m.w16(p + 8, m.r16(d));
    m.w16(p + 0xA, m.r16(d + 2));
    m.w16(p + 0xC, m.r16(d + 4));
    m.w16(p + 0xE, m.r16(d + 6));
    ot_link(ot, p);
    return p + 16;
}

void Game::t1_new_flags(u32 mode, u32 skip) {
    cover(0x800DC098u);
    Frame f(*this, 0x20);
    u32 saved = t1_pilots_saved();
    if (skip == 1) saved &= ~(1u << m.r8(m.r32(kState) + 0x5D));
    if (!(m.r32(m.r32(kScreens) + 0x54) & 2u)) return;
    if (mode == 0) {
        t1_prim_mode(4);
        t1_text_fold(1, 1, 1);
        t1_text_semi(2);
        t1_text_fold_at(1);
        const u32 y[3] = {0x96, 0xCC, 0x101};
        for (u32 k = 0; k < 3; ++k) {
            t1_prim_origin(0x6A, y[k]);
            if ((saved >> k) & 1u) {
                t1_prim_rgb(0x80, 0x80, 0x80);
                t1_printf(t1_name_buf(static_cast<s32>(k + 1)));
            } else {
                t1_prim_rgb(0x40, 0x40, 0x40);
                t1_printf(0x800CC5B0u);
            }
        }
    }
    if (mode == 1) {
        t1_prim_mode(4);
        t1_text_semi(2);
        t1_text_fold(1, 1, 1);
        t1_text_fold_at(1);
        const u32 y[3] = {0xBD, 0xF2, 0x125};
        for (u32 k = 0; k < 3; ++k) {
            const u32 x = (m.r32(m.r32(kScreens) + 0x54) & 8u) ? 0x51u : 0x6Au;
            t1_prim_origin(x, y[k]);
            if ((saved >> k) & 1u) {
                t1_prim_rgb(0x80, 0x80, 0x80);
                if (k == 0) t1_flag(2, 0);
                if (k == 2) t1_flag(4, 0);
                t1_printf(t1_name_buf(static_cast<s32>(k + 1)));
            } else {
                t1_prim_rgb(0x40, 0x40, 0x40);
                if (k == 0) t1_flag(2, 1);
                if (k == 2) t1_flag(4, 1);
                t1_printf(0x800CC5B0u);
            }
        }
    }
}
}
