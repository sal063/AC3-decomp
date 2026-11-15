#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kScreens = 0x800DD9DCu;
constexpr u32 kPrims = 0x800DDA58u;
constexpr u32 kState = 0x800DD9D0u;
constexpr u32 kSceneLoaded = 0x8009B904u;
constexpr u32 kAccount = 0x800DDA44u;
constexpr u32 kAnim = 0x800DDA68u;
constexpr u32 kScratchPtr = 0x800DD9D8u;
constexpr u32 kUlzMagic = 0x1A7A6C55u;
constexpr u32 kCards = 0x800DDA48u;
constexpr u32 kSprites = 0x800DDA18u;
constexpr u32 kQueue = 0x800DDA60u;
}

u32 Game::run_title_phase(u32 fn) {
    switch (fn) {
    case 0x800CCE98u: return t1_phase0();
    case 0x800CD07Cu: return t1_phase1();
    case 0x800CC5D8u: return t1_phase2();
    case 0x800CC764u: return t1_phase3();
    default: return call_by_address(fn, "a phase of overlay 1", 0, 0, 0, 0);
    }
}

u32 Game::t1_phase0() {
    cover(0x800CCE98u);
    Frame f(*this, 0x18);
    const u32 d = m.r32(kDisplay);
    m.w32(d + 4, 1);
    const u32 flags = m.r32(d);
    m.w8(d + 0x10, 0);
    m.w32(d + 0x14, 0);
    m.w32(d, flags | 9u);
    m.w8(m.r32(kDisplay) + 0x11, 0);
    m.w8(m.r32(kDisplay) + 0x12, 0);
    cd_reset(0, 0);
    t1_screens_setup();
    m.w32(m.r32(kScreens) + 0x24, ld_relocate_dir(0x800DCE68u));
    t1_set_1c((m.r32(m.r32(g::kScene) + 0x96C) & 2u) ? 1u : 0u);
    t1_prims_setup();
    const u32 x = m.r32(g::kExit);
    m.w32(x + 0x60, 0x70);
    const u32 at = mem_alloc(records_size(x + 0x60, 1));
    m.w32(m.r32(g::kScene) + 0x44, 1);
    request_records(m.r32(g::kExit) + 0x60, 1, at, kSceneLoaded);
    m.w32(m.r32(kState) + 0x48, 0x78);
    return 0x04000000u;
}

void Game::t1_screens_setup() {
    cover(0x800DC810u);
    Frame f(*this, 0x20);
    m.w32(kScreens, mem_alloc(0x58));
    m.w32(m.r32(kScreens) + 0x20, mem_alloc(0x180));
    m.w32(m.r32(kScreens) + 0x24, mem_alloc(0x80));
    {
        const u32 t = m.r32(kScreens);
        for (u32 i = 0; i < 32; ++i) {
            m.w16(m.r32(t + 0x20) + 12 * i, 0);
            m.w16(m.r32(t + 0x20) + 12 * i + 2, 0);
        }
    }
    for (u32 i = 0; i < 2; ++i) {
        const u32 a = mem_alloc(0x40);
        m.w32(m.r32(kScreens) + 4 * i + 0x14, a);
        const u32 b = mem_alloc(0x8000);
        m.w32(m.r32(kScreens) + 4 * i + 8, b);
    }
    {
        const u32 t = m.r32(kScreens);
        m.w16(t + 0x3E, 1);
        for (u32 o = 0x30; o <= 0x3C; o += 2) m.w16(t + o, 0);
        m.w32(t + 0x2C, t + 0x30);
    }
    m.w32(m.r32(kScreens) + 0x40, mem_alloc(0x20));
    m.w32(m.r32(kScreens) + 0x48, mem_alloc(0x20));
    m.w32(m.r32(kScreens) + 0x44, mem_alloc(0x20));
    {
        const u32 t = m.r32(kScreens);
        for (u32 i = 0; i < 16; ++i) {
            m.w16(m.r32(t + 0x40) + 2 * i, 0);
            m.w16(m.r32(t + 0x48) + 2 * i, 0);
            m.w16(m.r32(t + 0x44) + 2 * i, 0);
        }
    }
    m.w32(m.r32(kScreens) + 0x4C, mem_alloc(0x40));
    for (u32 i = 0; i < 64; ++i) m.w8(m.r32(m.r32(kScreens) + 0x4C) + i, 0);
    m.w32(m.r32(kScreens) + 0x1C, 0);
    m.w32(m.r32(kScreens) + 0x54, 0);
    for (u32 i = 0; i < 32; ++i) t1_sprite_reset(i);
}

void Game::t1_sprite_set(u32 i, u32 a1, u32 a2, u32 a3, u32 a4) {
    cover(0x800DA714u);
    if (i >= 32) return;
    const u32 e = m.r32(m.r32(kScreens) + 0x20) + 12 * i;
    m.w16(e + 0xA, a1);
    m.w16(e + 4, a2);
    m.w16(e + 6, a3);
    m.w16(e + 8, a4);
}

void Game::t1_sprite_reset(u32 i) {
    cover(0x800DA77Cu);
    Frame f(*this, 0x20);
    m.w32(f.at(0x10), 0);
    t1_sprite_set(i, 1, 0, 0, 0);
}

void Game::t1_set_1c(u32 v) {
    cover(0x800DA828u);
    m.w32(m.r32(kScreens) + 0x1C, v);
}

void Game::t1_prims_setup() {
    cover(0x800D80C0u);
    Frame f(*this, 0x28);
    m.w32(kPrims, mem_alloc(0x4C));
    for (u32 k = 0; k < 2; ++k) {
        m.w32(m.r32(kPrims) + 4 * k, mem_alloc(40000));
        const u32 w = mem_alloc(4);
        const u32 d = m.r32(kPrims);
        const u32 s = d + 4 * k;
        const u32 buf = m.r32(s);
        m.w32(s + 0x14, w);
        m.w8(d + 0x40, 0);
        m.w32(d + 8, buf);
        m.w8(m.r32(kPrims) + 0x41, 0);
        t1_prim_mode(4);
        for (u32 i = 0; i < 1000; ++i) {
            const u32 d2 = m.r32(kPrims);
            m.w8(m.r32(d2 + 8) + 3, 9);
            m.w8(m.r32(d2 + 8) + 7, 0x2C);
            m.w32(d2 + 8, m.r32(d2 + 8) + 40);
        }
    }
    const u32 d = m.r32(kPrims);
    m.w32(d + 0x10, 0);
    m.w8(d + 0x43, 0);
    t1_prim_origin(10, 0xC8);
    t1_prim_rgb(0x80, 0x80, 0x80);
}

void Game::t1_prim_mode(s32 k) {
    cover(0x800D7D08u);
    const u32 d = m.r32(kPrims);
    auto set = [&](u32 b38, u32 b39, u32 h2c, u32 h2e, u32 x, u32 y, u32 cx, u32 cy, u32 b3a) {
        m.w8(d + 0x38, b38);
        m.w8(d + 0x39, b39);
        m.w16(d + 0x2C, h2c);
        m.w16(d + 0x2E, h2e);
        m.w16(d + 0x30, x);
        m.w16(d + 0x32, y);
        m.w16(d + 0x34, cx);
        m.w16(d + 0x36, cy);
        m.w8(d + 0x3A, b3a);
        const u32 py = m.r16(d + 0x32), px = m.r16(d + 0x30);
        m.w16(d + 0x28, ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) | ((py & 0x200u) << 2));
        m.w16(d + 0x2A, (m.r16(d + 0x36) << 6) | ((m.r16(d + 0x34) >> 4) & 0x3Fu));
    };
    switch (k) {
    case 1:
        set(6, 6, 0x10, 2, 0x140, 0x140, 0x140, 0x158, 0x20);
        m.w8(m.r32(kPrims) + 0x3B, 0);
        break;
    case 2:
        set(6, 6, 0x20, 2, 0x188, 0x119, 0x180, 0x1FE, 0x20);
        m.w8(m.r32(kPrims) + 0x3B, 0);
        break;
    case 3:
    case 4:
        set(0x10, 0x15, 0x10, 1, 0x300, k == 3 ? 0x17Eu : 0x100u, 0x190, 0x2D, 0x21);
        m.w8(m.r32(kPrims) + 0x3B, 1);
        m.w32(m.r32(kPrims) + 0x3C, k == 3 ? 0x800DD870u : 0x800DD810u);
        m.w8(m.r32(kPrims) + 0x42, 4);
        break;
    case 7:
        set(6, 0xA, 0x19, 1, 0x180, 0x180, 0x190, 0x1FF, 0x30);
        m.w8(m.r32(kPrims) + 0x3B, 0);
        m.w8(m.r32(kPrims) + 0x42, 6);
        m.w8(m.r32(kPrims) + 0x40, 0);
        m.w8(m.r32(kPrims) + 0x41, 0);
        break;
    default: break;
    }
}

void Game::t1_prim_origin(u32 x, u32 y) {
    cover(0x800D7C40u);
    const u32 d = m.r32(kPrims);
    m.w16(d + 0x20, x);
    m.w16(d + 0x24, x);
    m.w16(d + 0x22, y);
    m.w16(d + 0x26, y);
}

void Game::t1_prim_rgb(u32 r, u32 g, u32 b) {
    cover(0x800D7C60u);
    m.w8(m.r32(kPrims) + 0xC, r);
    m.w8(m.r32(kPrims) + 0xD, g);
    m.w8(m.r32(kPrims) + 0xE, b);
}

u32 Game::t1_phase1() {
    cover(0x800CD07Cu);
    Frame f(*this, 0x18);
    const u32 st = m.r32(kState);
    const s32 left = m.r32s(st + 0x48) - 1;
    m.w32(st + 0x48, static_cast<u32>(left));
    const u32 sc = m.r32(g::kScene);
    bool wait = m.r32(sc + 0x44) != 0;
    if (!wait && (m.r32(sc + 0x96C) & 2u) && !(m.r32(sc + 0x12C) & 0x10u) && left >= 0) wait = true;
    if (!wait) return t1_unpack();
    t1_text_frame(m.r32(m.r32(g::kScene) + 8));
    t1_prim_mode(7);
    t1_text_semi(-1);
    t1_text_fold(1, 1, 1);
    t1_text_fold_at(1);
    t1_prim_rgb(0x18, 0xB8, 0xD0);
    if (m.r32(m.r32(g::kScene) + 0x96C) & 2u) {
        t1_prim_origin(0xE9, 0x91);
        t1_printf(0x800CBFECu);
    }
    t1_screens_frame(m.r32(m.r32(g::kScene) + 8));
    t1_screens_run();
    return 0;
}

u32 Game::t1_unpack() {
    cover(0x800CCFA4u);
    Frame f(*this, 0x20);
    const u32 d = ld_relocate_dir(m.r32(m.r32(g::kExit) + 0x60));
    snd_load_bank(m.r32(d + 0x20));
    reverb_on();
    m.w32(m.r32(kScreens), m.r32(m.r32(d)));
    m.w32(m.r32(kScreens) + 0x24, ld_relocate_dir(m.r32(d)));
    t1_account_setup(d + 4, m.r32(d + 8));
    t1_tims(m.r32(d + 0x10));
    t1_tim(m.r32(d + 0x18));
    heap_set(m.r32(d + 0x10));
    t1_anim_setup(m.r32(d + 0xC));
    const u32 sc = m.r32(g::kScene);
    m.w32(sc + 0x96C, m.r32(sc + 0x96C) & ~2u);
    return 0x04000000u;
}

void Game::reverb_on() {
    cover(0x8002DA94u);
    Frame f(*this, 0x20);
    hw->spu_clear_reverb_work_area(m.r32(0x800C65F4u));
    m.w32(0x800C65F0u, 6);
    hw->spu_set_reverb_depth(0x800C65F0u);
    hw->spu_set_reverb(1);
    hw->spu_set_reverb_voice(1, 0xFFFFFF);
}

s32 Game::dir_pointers(u32 pp, u32 out) {
    cover(0x800CCE38u);
    const u32 d = m.r32(pp);
    const s32 n = m.r32s(d);
    for (s32 i = 0; i < m.r32s(d); ++i, out += 4) {
        const s32 o = m.r32s(d + 4 + 4u * static_cast<u32>(i));
        m.w32(out, d + (static_cast<u32>((o < 0 ? o + 3 : o) >> 2) << 2));
    }
    return n;
}

void Game::t1_account_setup(u32 icons, u32 names) {
    cover(0x800D4FF0u);
    Frame f(*this, 0x30);
    const u32 a = mem_alloc(0x280);
    m.w32(kAccount, a);
    m.w16(a, m.r16(0x800CC488u));
    m.w8(a + 2, m.r8(0x800CC48Au));
    m.w8(m.r32(kAccount) + 2, 0x13);
    m.w8(m.r32(kAccount) + 3, 2);
    {
        const u32 t = m.r32(kAccount) + 4;
        for (u32 o = 0; o < 0x20; o += 4) m.w32(t + o, m.r32(0x800CC48Cu + o));
        m.w32(t + 0x20, m.r32(0x800CC4ACu));
        m.w16(t + 0x24, m.r16(0x800CC4B0u));
        m.w8(t + 0x26, m.r8(0x800CC4B2u));
    }
    {
        const u32 t = m.r32(kAccount);
        for (u32 o = 0x4C; o >= 0x44; o -= 4) m.w32(t + o, 0);
        m.w16(t + 0x50, 1);
        m.w32(t + 0x52, m.r32(0x800CC4B4u));
        m.w8(t + 0x56, m.r8(0x800CC4B8u));
        m.w8(t + 0x56, 0);
        m.w8(m.r32(kAccount) + 0x57, 0);
        m.w32(m.r32(kAccount) + 0x58, 0);
        m.w32(m.r32(kAccount) + 0x5C, 0);
    }
    const u32 ptrs = f.at(0x10);
    const s32 n = dir_pointers(icons, ptrs);
    m.w8(m.r32(kAccount) + 2, static_cast<u32>(n + 0x10));
    for (u32 i = 0; i < 8; ++i) m.w32(m.r32(kAccount) + 0x60 + 4 * i, m.r32(m.r32(ptrs) + 4 * i + 0x14));
    for (s32 k = 0; k < n; ++k)
        for (u32 i = 0; i < 32; ++i)
            m.w32(m.r32(kAccount) + (static_cast<u32>(k) << 7) + 0x80 + 4 * i,
                  m.r32(m.r32(ptrs + 4u * static_cast<u32>(k)) + 4 * i + 0x40));
    for (u32 i = 0; i < 32; ++i) m.w32(m.r32(kAccount) + 0x200 + 4 * i, m.r32(names + 4 * i));
}

u32 Game::t1_tims(u32 src) {
    cover(0x800D8BA8u);
    Frame f(*this, 0x20);
    const u32 buf = mem_alloc(0x30000);
    u32 dir = src;
    s32 n = m.r32s(src);
    if (m.r32(src) == kUlzMagic) {
        ulz_decode(src, buf);
        n = m.r32s(buf);
        dir = buf;
    }
    ld_upload_tims(ld_relocate_dir(dir), n);
    heap_set(buf);
    return 1;
}

void Game::t1_tim(u32 src) {
    cover(0x800D8CECu);
    Frame f(*this, 0x20);
    const u32 buf = mem_alloc(0x10000);
    u32 tim = src;
    if (m.r32(src) == kUlzMagic) {
        ulz_decode(src, buf);
        tim = buf;
    }
    load_tim(tim);
    heap_set(buf);
}

void Game::load_tim(u32 tim) {
    cover(0x800271A0u);
    Frame f(*this, 0x30);
    const u32 img = f.at(0x10);
    open_tim(tim);
    read_tim(img);
    if (m.r32(img + 0x10)) hw->load_image(m.r32(img + 0xC), m.r32(img + 0x10));
    if (m.r32(img + 8)) hw->load_image(m.r32(img + 4), m.r32(img + 8));
}

void Game::t1_anim_setup(u32 dir) {
    cover(0x800DA418u);
    Frame f(*this, 0x18);
    m.w32(kAnim, mem_alloc(0xC));
    m.w32(m.r32(kAnim) + 8, ld_relocate_dir(dir));
    for (u32 i = 0; i < 8; ++i) m.w8(m.r32(kAnim) + i, 0);
    cover(0x800DA484u);
}

void Game::t1_text_frame(u32 ot) {
    cover(0x800D81F0u);
    Frame f(*this, 0x20);
    const u32 d = m.r32(kPrims), b = m.r32(m.r32(g::kScene) + 4);
    m.w32(d + 8, m.r32(d + 4 * b));
    const u32 t = m.r32(d + 4 * b + 0x14);
    m.w32(d + 0x1C, t);
    clear_otag_r(t, 1);
    ot_link(ot, m.r32(m.r32(kPrims) + 0x1C));
}

void Game::t1_text_semi(s32 abr) {
    cover(0x800D7C88u);
    if (static_cast<s16>(abr) == -1) {
        m.w8(m.r32(kPrims) + 0x40, 0);
        m.w8(m.r32(kPrims) + 0x41, 0);
    } else {
        m.w8(m.r32(kPrims) + 0x40, 1);
        m.w8(m.r32(kPrims) + 0x41, static_cast<u32>(abr));
    }
}

void Game::t1_text_fold_at(u32 t) {
    cover(0x800D7CD4u);
    m.w16(m.r32(kPrims) + 0x48, t);
}

void Game::t1_text_fold(u32 on, u32 from, u32 to) {
    cover(0x800D7CE4u);
    m.w8(m.r32(kPrims) + 0x43, on);
    m.w16(m.r32(kPrims) + 0x44, from);
    m.w16(m.r32(kPrims) + 0x46, to);
}

void Game::t1_printf(u32 fmt, u32 a1, u32 a2, u32 a3) {
    cover(0x800D8AA0u);
    m.w32(sp, fmt);
    m.w32(sp + 4, a1);
    m.w32(sp + 8, a2);
    m.w32(sp + 12, a3);
    Frame f(*this, 0x218);
    const u32 buf = f.at(0x10);
    m.w32(m.r32(kPrims) + 0x10, vsprintf(buf, fmt, f.at(0x21C)));
    t1_text(buf);
}

void Game::t1_text(u32 s) {
    cover(0x800D8980u);
    Frame f(*this, 0x20);
    for (s32 i = 0; i < m.r32s(m.r32(kPrims) + 0x10); ++i) {
        const u32 c = m.r8(s + static_cast<u32>(i));
        const u32 d = m.r32(kPrims);
        if (c == 10) {
            const u32 dy = m.r8(d + 0x39) * static_cast<u32>(m.r16s(d + 0x2E));
            const u32 x0 = m.r16(d + 0x24), y = m.r16(d + 0x22);
            m.w16(d + 0x20, x0);
            m.w16(d + 0x22, y + dy);
        } else if (c == 9) {
            m.w16(d + 0x20, m.r16(d + 0x24));
        } else if (c == 0x20) {
            const u32 w = m.r8(d + 0x3B) ? m.r8(d + 0x42) : m.r8(d + 0x38);
            m.w16(d + 0x20, m.r16(d + 0x20) + w);
        } else {
            o1_glyph(c);
        }
    }
}

void Game::t1_glyph(u32 c) {
    cover(0x800D82A4u);
    const u32 d = m.r32(kPrims);
    const u32 k = (c - m.r8(d + 0x3A)) & 0xFFu;
    const s32 cols = m.r16s(d + 0x2C);
    const s32 q = static_cast<s32>(k) / cols, r = static_cast<s32>(k) % cols;
    const u32 W = m.r8(d + 0x38), H = m.r8(d + 0x39);
    const u32 u = static_cast<u32>(r) * W + (static_cast<u32>(static_cast<s16>(m.r16s(d + 0x30) % 64)) << 2);
    const u32 v = static_cast<u32>(q) * H + static_cast<u32>(m.r16s(d + 0x32) % 256);
    const u32 p = m.r32(d + 8);
    m.w8(p + 0xC, u);
    m.w8(p + 0xD, v);
    m.w8(p + 0x14, W + u + 0xFF);
    m.w8(p + 0x15, v);
    m.w8(p + 0x1C, u);
    m.w8(p + 0x1D, H + v + 0xFF);
    m.w8(p + 0x24, W + u + 0xFF);
    m.w8(p + 0x25, H + v + 0xFF);
    m.w8(p + 7, m.r8(d + 0x40) ? (m.r8(p + 7) | 2u) : (m.r8(p + 7) & 0xFDu));
    pkt_rgb(p, m.r8(d + 0xC), m.r8(d + 0xD), m.r8(d + 0xE));
    const u32 py = m.r16(d + 0x32), px = m.r16(d + 0x30);
    m.w16(p + 0x16, ((m.r8(d + 0x41) & 3u) << 5) | ((py & 0x100u) >> 4) | ((px & 0x3FFu) >> 6) | ((py & 0x200u) << 2));
    m.w16(p + 0xE, m.r16(d + 0x2A));
    const u32 X = m.r16(d + 0x20), Y = m.r16(d + 0x22);
    const s32 L = m.r16s(d + 0x2E);
    u32 y0 = Y, y1 = Y + (H - 1) * static_cast<u32>(L);
    if (m.r8(d + 0x43) == 1) {
        const s32 T = m.r16s(d + 0x48), A = m.r16s(d + 0x44), B = m.r16s(d + 0x46);
        if (T < A) return;
        if (T < B) {
            const s32 hh = static_cast<s32>(H) * L;
            const s32 half = (hh + static_cast<s32>(static_cast<u32>(hh) >> 31)) >> 1;
            y0 = Y + static_cast<u32>((half * (B - T)) / (B - A));
            y1 = y0 + static_cast<u32>(((static_cast<s32>(H) - 1) * L * (T - A)) / (B - A));
        }
    }
    m.w16(p + 8, X);
    m.w16(p + 0xA, y0);
    m.w16(p + 0x10, X + W - 1);
    m.w16(p + 0x12, y0);
    m.w16(p + 0x18, X);
    m.w16(p + 0x1A, y1);
    m.w16(p + 0x20, X + W - 1);
    m.w16(p + 0x22, y1);
    ot_link(m.r32(d + 0x1C), p);
    const u32 adv = m.r8(d + 0x3B) ? m.r8(m.r32(d + 0x3C) + k) : W;
    m.w16(d + 0x20, m.r16(d + 0x20) + adv);
    m.w32(d + 8, m.r32(d + 8) + 40);
}

void Game::t1_screens_frame(u32 ot) {
    cover(0x800DC9ECu);
    Frame f(*this, 0x20);
    const u32 t = m.r32(kScreens), b = m.r32(m.r32(g::kScene) + 4);
    m.w32(t + 0x10, m.r32(t + 4 * b + 0x14));
    m.w32(t + 4, m.r32(t + 4 * b + 8));
    clear_otag_r(m.r32(t + 0x10), 16);
    const u32 o = m.r32(m.r32(kScreens) + 0x10);
    m.w32(o, (m.r32(o) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | ((o + 0x3C) & 0x00FFFFFFu));
}

void Game::t1_screens_run() {
    cover(0x800DCAA4u);
    Frame f(*this, 0x18);
    m.w32(kScratchPtr, g::kScratch);
    hw->vsync(1);
    const u32 t = m.r32(kScreens);
    m.w32(t + 4, m.r32(t + 4 * m.r32(m.r32(g::kScene) + 4) + 8));
    t1_screens_scripts();
    t1_fades();
}

void Game::t1_screens_scripts() {
    cover(0x800DC7ACu);
    Frame f(*this, 0x20);
    u32 bits = m.r32(m.r32(kScreens) + 0x1C);
    for (u32 i = 0; i < 32 && bits; ++i, bits >>= 1)
        if (bits & 1u) t1_screen(i);
}

void Game::t1_screen(u32 i) {
    cover(0x800DC6C4u);
    Frame f(*this, 0x28);
    const u32 t = m.r32(kScreens);
    u32 at = m.r32(m.r32(t + 0x24) + 4 * i);
    const u32 ot = m.r32(t + 0x10);
    while (m.r32(at) != 0xFFFFFFFFu) {
        const u32 op = m.r32(at);
        if (op >= 7) {
            unported(0x800DC6C4u, "a screen command past the table (the original spins)");
            return;
        }
        at = t1_op(op, at + 4, ot, m.r32(m.r32(kScreens) + 0x20) + 12 * i);
    }
    t1_screen_cb(i);
}

void Game::t1_fades() {
    cover(0x800DA8E0u);
    const u32 t = m.r32(kScreens);
    for (u32 i = 0; i < 16; ++i) {
        const u32 st = m.r32(t + 0x48) + 2 * i, v = m.r32(t + 0x40) + 2 * i;
        if (m.r16s(st) == 1) {
            if (m.r16s(v) < m.r16s(m.r32(t + 0x44) + 2 * i)) m.w16(v, m.r16(v) + 1);
            else m.w16(st, 2);
        }
        const u32 st2 = m.r32(t + 0x48) + 2 * i;
        if (m.r16s(st2) == 3) {
            const u32 v2 = m.r32(t + 0x40) + 2 * i;
            if (m.r16s(v2) > 0) m.w16(v2, m.r16(v2) - 1);
            else m.w16(st2, 0);
        }
    }
}

u32 Game::vsprintf(u32 buf, u32 fmt, u32 args) {
    cover(0x8002877Cu);
    Frame f(*this, 0x20);
    const u32 cursor = f.at(0x10);
    m.w32(cursor + 4, buf);
    m.w32(cursor, buf);
    const u32 n = text_format(fmt, args, cursor);
    m.w8(buf + n, 0);
    return n;
}

u32 Game::t1_phase2() {
    cover(0x800CC5D8u);
    Frame f(*this, 0x20);
    {
        m.w8(m.r32(g::kScene) + 0x95E, 0);
        m.w8(m.r32(g::kScene) + 0x95F, 0);
        const u32 sc = m.r32(g::kScene);
        m.w32(sc + 0x96C, m.r32(sc + 0x96C) & ~8u);
        m.w16(sc + 0x38C, m.r16(sc + 0x390));
        m.w16(sc + 0x38E, m.r16(sc + 0x392));
    }
    m.w32(kState, mem_alloc(0xE4));
    for (u32 i = 0; i < 2; ++i) {
        const u32 b = mem_alloc(0x2000);
        m.w32(m.r32(kState) + 4 * i, b);
    }
    t1_queue_setup();
    {
        const u32 st = m.r32(kState);
        m.w8(st + 0x58, 0);
        m.w16(st + 0xC, 1);
        m.w16(st + 0xE, 0);
        m.w32(st + 0x44, 0);
        m.w16(st + 0x12, 0);
        m.w32(st + 0x40, 0);
        m.w32(st + 0x78, 0);
        for (u32 o : {0x59u, 0x5Au, 0x5Du, 0x5Fu, 0x60u, 0x5Eu}) m.w8(st + o, 0);
        m.w32(st + 0x64, 0x800DCB30u);
        m.w8(st + 0x5C, 0);
        m.w32(st + 0x68, 0x800DCCC8u);
        m.w8(st + 0x5B, 0);
        m.w8(st + 0xD0, 0);
        m.w8(st + 0xD1, 1);
    }
    m.w8(m.r32(g::kScene) + 0x971, 0);
    t1_cards_alloc();
    t1_cards_reset();
    m.w32(m.r32(kState) + 0x18, 0);
    t1_prims_setup();
    t1_sprites_setup();
    return 0x04000000u;
}

void Game::t1_queue_setup() {
    cover(0x800DA25Cu);
    Frame f(*this, 0x18);
    const u32 q = mem_alloc(0x101C);
    m.w32(q + 0xC, 0x3FFF);
    m.w32(q + 4, 0);
    m.w32(q + 8, 0);
    m.w32(q, 0);
    m.w32(q + 0x10, 0xFF);
    m.w32(q + 0x14, 0);
    m.w32(kQueue, q);
}

void Game::t1_cards_alloc() {
    cover(0x800D4F0Cu);
    Frame f(*this, 0x20);
    m.w32(kCards, mem_alloc(0x360));
    m.w32(m.r32(kCards) + 0x2E8, mem_alloc(0x80));
    for (u32 i = 0; i < 4; ++i) {
        const u32 p = mem_alloc(0x480);
        m.w32(m.r32(kCards) + 0x2EC + 4 * i, p);
    }
    for (u32 i = 0; i < 7; ++i) {
        const u32 p = mem_alloc(0x180);
        m.w32(m.r32(kCards) + 0x2FC + 4 * i, p);
    }
}

void Game::t1_cards_reset() {
    cover(0x800D5550u);
    Frame f(*this, 0x18);
    {
        const u32 b = m.r32(kCards);
        m.w16(b + 0x358, 0);
        m.w16(b + 0x35A, 0);
        m.w16(b + 0x35C, 0);
    }
    t1_card_dir_reset();
    t1_card_states_reset();
    cover(0x800D5548u);
    t1_card_flags_reset();
    const u32 b = m.r32(kCards);
    m.w8(b + 0x336, 0);
    m.w32(b + 0x320, 0);
    m.w16(b + 0x4C, 0);
    m.w16(b + 0x4E, 0);
    m.w16(b + 0x50, 0);
    m.w16(b + 0x52, 0);
    m.w8(b + 0x334, 0);
    for (u32 o = 0; o < 12; o += 4) m.w32(b + 0x338 + o, m.r32(0x800CC4C4u + o));
    m.w16(b + 0x344, m.r16(0x800CC4D0u));
    for (u32 o = 0; o < 0x14; o += 4) m.w32(b + 0x2B4 + o, m.r32(0x800CC4D4u + o));
    m.w16(b + 0x2C8, m.r16(0x800CC4E8u));
    m.w8(b + 0x2CA, m.r8(0x800CC4EAu));
    for (u32 o = 0; o < 0x14; o += 4) m.w32(b + 0x2CE + o, m.r32(0x800CC4ECu + o));
    m.w16(b + 0x2E2, m.r16(0x800CC500u));
    m.w8(b + 0x2E4, m.r8(0x800CC502u));
    for (u32 i = 0; i < 2; ++i) {
        m.w16(b + 2 * i + 0x2C, 2);
        m.w16(b + 2 * i + 0x30, 2);
    }
}

void Game::t1_card_flags_reset() {
    cover(0x800D3CE8u);
    const u32 b = m.r32(kCards);
    m.w8(b + 0x34C, 1);
    m.w32(b + 0x28, 1);
    m.w8(b + 0x348, 0);
    m.w8(b + 0x34A, 0);
    m.w8(b + 0x350, 0);
    m.w16(b + 0x30, 0);
    m.w8(b + 0x34D, 1);
    m.w8(b + 0x349, 0);
    m.w8(b + 0x34B, 0);
    m.w8(b + 0x351, 0);
    m.w16(b + 0x32, 0);
    m.w32(b + 0x354, 0);
}

void Game::t1_card_dir_reset() {
    cover(0x800D51FCu);
    const u32 p = m.r32(m.r32(kCards) + 0x2E8);
    m.w32(p + 4, 0x990426);
    m.w8(p + 8, 0);
    m.w8(p + 9, 0);
    for (u32 i = 0; i < 0x70; ++i) m.w8(p + 0x10 + i, 0);
    m.w32(p + 0xC, 0);
}

void Game::t1_card_states_reset() {
    cover(0x800D5290u);
    Frame f(*this, 8);
    for (u32 k = 0; k < 4; ++k) {
        const u32 a = m.r32(m.r32(kCards) + 0x2EC + 4 * k);
        m.w32(a + 8, m.r32(0x800CC4BCu));
        m.w8(a + 0xC, m.r8(0x800CC4C0u));
        m.w16(a + 0x14, 0);
        m.w16(a + 0x16, 0);
        for (u32 i = 0; i < 0x3C; ++i) {
            m.w16(a + 2 * i + 0x18A, 0);
            m.w16(a + 2 * i + 0x112, 0);
            m.w16(a + 2 * i + 0x9A, 0);
            m.w16(a + 2 * i + 0x22, 0);
            m.w8(a + i + 0x202, 0);
            m.w8(a + i + 0x220, 0);
            m.w8(a + i + 0x23E, 0);
            m.w8(a + i + 0x25C, 0);
        }
        m.w16(a + 0x2C0, 0);
        m.w16(a + 0x2C2, 0);
        for (u32 j = 0; j < 6; ++j) {
            const u32 t = a + 0x2C4 + 0x44 * j;
            const u32 sc = m.r32(g::kScene) + 0x5C * j;
            for (u32 i = 0; i < 0x14; ++i) m.w16(t + 2 * i, m.r16(sc + 0x172 + 2 * i));
            for (u32 o = 0; o < 0x1C; o += 4) m.w32(t + 0x28 + o, m.r32(sc + 0x19C + o));
        }
        m.w8(a + 0x45C, 0);
        m.w8(a + 0x45D, 0xCC);
        m.w8(a + 0x45E, 0xCC);
        m.w16(a + 0x462, 0);
        m.w16(a + 0x460, 0);
        m.w16(a + 0x16, 0);
        const u32 sc = m.r32(g::kScene);
        m.w8(a + 0x464, (m.r32(sc + 0x120) & 4u) ? 0u : 1u);
        u32 fl = (m.r8(sc + 0x39C) & 1u) ? 1u : 0u;
        if (m.r32(sc + 0x120) & 1u) fl |= 2u;
        if (m.r32(sc + 0x120) & 2u) fl |= 4u;
        if (m.r8(sc + 0x95A) == 1) fl |= 8u;
        m.w32(a + 0x468, fl);
        for (s32 i = 0x10; i >= 0; --i) m.w8(a + 0x46C + static_cast<u32>(i), 0);
    }
}

void Game::t1_sprites_setup() {
    cover(0x800CD1A0u);
    Frame f(*this, 0x38);
    const u32 q = mem_alloc(0x24);
    m.w32(kSprites, q);
    m.w32(q + 0x18, 0);
    m.w32(q + 0x1C, 0);
    m.w32(q + 0x20, 0);
    for (u32 k = 0; k < 2; ++k) {
        const u32 b = mem_alloc(0x4E20);
        m.w32(m.r32(kSprites) + 4 * k, b);
        const u32 md = mem_alloc(0x4B0);
        const u32 e = m.r32(kSprites) + 4 * k;
        m.w32(e + 0xC, md);
        u32 p = m.r32(e);
        for (u32 i = 0; i < 1000; ++i, p += 20) {
            m.w8(p + 3, 4);
            m.w8(p + 7, 0x64);
            m.w16(p + 0x10, 8);
            m.w16(p + 0x12, 0xE);
            m.w16(p + 0xE, 0x41A8);
        }
        u32 dm = m.r32(m.r32(kSprites) + 4 * k + 0xC);
        for (u32 i = 0; i < 100; ++i, dm += 12) {
            m.w32(f.at(0x10), 0);
            set_draw_mode(dm, 0, 0, 0x1B, 0);
        }
    }
}
}
