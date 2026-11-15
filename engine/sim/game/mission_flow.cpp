#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCdRing = 0x800BCBBCu;
constexpr u32 kPhaseKind = 0x800BCEA8u;
constexpr u32 kMissionParams = 0x800BCEB0u;
constexpr u32 kEndTimers = 0x800BCED0u;
constexpr u32 kAirStarts = 0x800BCF00u;
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kMessages = 0x800BCFECu;
constexpr u32 kRadioQueue = 0x800BD020u;
constexpr u32 kRadioState = 0x800BD028u;
constexpr u32 kRadioStream = 0x800BD01Cu;
constexpr u32 kRadioScripts = 0x800BD02Cu;
constexpr u32 kRadioScript = 0x800BD030u;
constexpr u32 kRadioCalls = 0x800BD034u;
constexpr u32 kRadioHold = 0x800BCFD4u;
constexpr u32 kSubtitleLine = 0x800BCE8Eu;
constexpr u32 kFace = 0x800BCE88u;
constexpr u32 kWarning = 0x800C7180u;
constexpr u32 kWreckTimer = 0x800C70D0u;
constexpr u32 kPlayerWeapons = 0x800BD098u;
constexpr u32 kHeapLimit = 0x801FB700u;

u32 ms_of(Memory& m) { return m.r32(g::kMission); }
u32 md_of(Memory& m) { return m.r32(g::kMissionData); }
u32 params_of(Memory& m) { return m.r32(kMissionParams); }
u32 world_of(Memory& m) { return m.r32(g::kWorld); }
u32 scene_of(Memory& m) { return m.r32(g::kScene); }
s32 mission_of(Memory& m) { return m.r16s(m.r32(g::kScene) + 908); }
bool paused(Memory& m) { return (m.r32(m.r32(g::kScene)) & 0x20000000u) != 0; }
void scene_or(Memory& m, u32 v) { m.w32(m.r32(g::kScene), m.r32(m.r32(g::kScene)) | v); }
void link_prim(Memory& m, u32 ot, u32 p, u32 len) {
    m.w32(p, m.r32(ot) | (len << 24));
    m.w32(ot, p & 0x00FFFFFFu);
}
}

void Game::cd_pause(u32 on) {
    cover(0x8002C6F4u);
    const u32 r = m.r32(kCdRing);
    m.w8(r + 0x14C, 6);
    m.w32(r + 0x148, 1);
    m.w32(r + 0x150, on);
    m.w32(r + 0x15C, 0);
    m.w8(r + 0x14D, 0);
}

void Game::cd_reset(u32 a, u32 b) {
    cover(0x8002C6A8u);
    for (u32 k = 0; k < 16; ++k) m.w8(m.r32(kCdRing) + 8 + 20 * k, 0);
    const u32 r = m.r32(kCdRing);
    m.w8(r + 0x14C, 1);
    m.w32(r + 0x148, 1);
    m.w32(r + 0x150, a);
    m.w32(r + 0x15C, b);
    m.w8(r + 0x14D, 0);
}

void Game::pause_menu() {
    cover(0x80034718u);
    Frame f(*this, 24);
    const u32 w = world_of(m);
    const u32 cur = w + 668;
    m.w32(w, m.r32(w) | 0x40u);
    if (m.r32(scene_of(m) + 304) & 1u) m.w16(cur, m.r16(cur) - 1u);
    if (m.r32(scene_of(m) + 304) & 2u) m.w16(cur, m.r16(cur) + 1u);
    if (m.r16s(cur) < 0) m.w16(cur, 4);
    if (m.r16s(cur) >= 5) m.w16(cur, 0);
    if (m.r16(cur) == 1 && (m.r32(scene_of(m) + 304) & 0xCu)) m.w8(scene_of(m) + 925, m.r8(scene_of(m) + 925) == 0);
    if (m.r16(cur) == 2 && (m.r32(scene_of(m) + 304) & 0xCu)) {
        m.w8(scene_of(m) + 926, m.r8(scene_of(m) + 926) == 0);
        view_flag(m.r8(scene_of(m) + 926) == 0);
    }
    if (m.r32(scene_of(m) + 300) & 0x40u) m.w16(cur, 0);
    if (!(m.r32(scene_of(m) + 300) & 0x10u)) return;
    const s32 c = m.r16s(cur);
    if (c == 3) {
        scene_or(m, 0x8000000u);
        m.w32(world_of(m), m.r32(world_of(m)) | 0x40000000u);
    } else if (c == 0) {
        scene_or(m, 0x8000000u);
    } else if (c == 4) {
        scene_or(m, 0x8000000u);
        m.w32(world_of(m), m.r32(world_of(m)) | 0x80000000u);
    }
}

u32 Game::pause_text(u32 x, u32 y, u32 ot, u32 p, u32 uv, u32 level) {
    cover(0x80034360u);
    m.w16(p + 8, x);
    m.w16(p + 10, y);
    m.w32(p + 4, level | (level << 8) | (level << 16) | 0x66000000u);
    m.w32(p + 12, m.r32(uv));
    m.w32(p + 16, m.r32(uv + 4));
    link_prim(m, ot, p, 4);
    return p + 20;
}

u32 Game::pause_mode(u32 ot, u32 p, u32 mode) {
    cover(0x800346C0u);
    m.w8(p + 3, 1);
    m.w32(p + 4, (mode & 0x9FFu) | 0xE1000000u);
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return p + 8;
}

u32 Game::pause_box(u32 ot, u32 p, u32 rect, u32 kind) {
    cover(0x800343C8u);
    Frame f(*this, 16);
    const u32 x = m.r16(rect), y = m.r16(rect + 2), w = m.r16(rect + 4), h = m.r16(rect + 6);
    u32 clut, corner, edge, fill;
    if (kind == 1) {
        clut = 0x5DD6;
        corner = 0x76808080u;
        edge = 0x62000000u;
        fill = 0x62484848u;
    } else if (kind == 2) {
        clut = 0x5D56;
        corner = 0x76808080u;
        edge = 0x62000098u;
        fill = 0x620030F8u;
    } else {
        clut = 0x5D56;
        corner = 0x74808080u;
        edge = 0x60000098u;
        fill = 0x600030F8u;
    }
    const u32 cw = clut << 16;
    auto sprite = [&](u32 q, u32 sx, u32 sy, u32 uv) {
        m.w32(q + 4, corner);
        m.w16(q + 8, sx);
        m.w16(q + 10, sy);
        m.w32(q + 12, cw | uv);
        link_prim(m, ot, q, 3);
    };
    auto rectangle = [&](u32 q, u32 col, u32 rx, u32 ry, u32 rw, u32 rh) {
        m.w32(q + 4, col);
        m.w16(q + 8, rx);
        m.w16(q + 10, ry);
        m.w16(q + 12, rw);
        m.w16(q + 14, rh);
        link_prim(m, ot, q, 3);
    };
    sprite(p, x, y, 0x40E8);
    sprite(p + 16, x + w - 8, y, 0x40F8);
    sprite(p + 32, x, y + h - 8, 0x50E8);
    sprite(p + 48, x + w - 8, y + h - 8, 0x50F8);
    rectangle(p + 64, edge, x, y + 8, 2, h - 16);
    rectangle(p + 80, edge, x + w - 2, y + 8, 2, h - 16);
    rectangle(p + 96, edge, x + 8, y, w - 16, 2);
    rectangle(p + 112, edge, x + 8, y + h - 2, w - 16, 2);
    rectangle(p + 128, fill, x + 8, y + 2, w - 16, 6);
    rectangle(p + 144, fill, x + 2, y + 8, w - 4, h - 16);
    rectangle(p + 160, fill, x + 8, y + h - 8, w - 16, 6);
    return p + 176;
}

u32 Game::pause_draw(u32 ot, u32 p) {
    cover(0x80034930u);
    Frame f(*this, 64);
    const u32 box = f.at(0x18), menu = f.at(0x20);
    m.w32(box, m.r32(0x800BCBC0u));
    m.w32(box + 4, m.r32(0x800BCBC4u));
    m.w32(menu, m.r32(0x800BCBC8u));
    m.w32(menu + 4, m.r32(0x800BCBCCu));
    const u32 cur = world_of(m) + 668;
    constexpr u32 kRowY = 0x800B0AE0u;
    auto text = [&](s32 dx, u32 row, u32 uv, u32 level) {
        m.w32(f.at(0x10), uv);
        m.w32(f.at(0x14), level);
        const u32 x = static_cast<u32>(m.r16s(menu) + dx);
        const u32 y = static_cast<u32>(m.r16s(menu + 2)) + m.r32(kRowY + 4 * row) + 5;
        p = pause_text(x, y, ot, p, uv, level);
    };
    auto rows_end = [&](u32 row) { p = pause_mode(ot, p, m.r16s(cur) == static_cast<s32>(row) ? 0x35 : 0x55); };
    auto lit = [&](u32 row) { return m.r16s(cur) == static_cast<s32>(row) ? 0x60u : 0x40u; };
    text(6, 0, 0x800B0A60u, lit(0));
    rows_end(0);
    const u32 l1 = lit(1);
    text(6, 1, 0x800B0A98u, l1);
    text(51, 1, 0x800B0A90u, l1);
    text(113, 1, 0x800B0A70u + 8 * m.r8(scene_of(m) + 925), l1);
    rows_end(1);
    const u32 l2 = lit(2);
    text(6, 2, 0x800B0A68u, l2);
    text(114, 2, m.r8(scene_of(m) + 926) ? 0x800B0A70u : 0x800B0A78u, l2);
    rows_end(2);
    const u32 l3 = lit(3);
    text(6, 3, 0x800B0AA0u, l3);
    text(25, 3, 0x800B0AA8u, l3);
    text(80, 3, 0x800B0A88u, l3);
    rows_end(3);
    const u32 l4 = lit(4);
    text(6, 4, 0x800B0A80u, l4);
    text(80, 4, 0x800B0A88u, l4);
    rows_end(4);
    m.w16(menu + 2, m.r16(menu + 2) + m.r16(kRowY + 4 * static_cast<u32>(m.r16s(cur))));
    p = pause_box(ot, p, menu, 1);
    p = pause_mode(ot, p, 0x55);
    p = pause_box(ot, p, box, 2);
    m.w32(f.at(0x10), 0x800B0A58u);
    m.w32(f.at(0x14), 0xC0);
    p = pause_text(0x7D, 0x4A, ot, p, 0x800B0A58u, 0xC0);
    return pause_mode(ot, p, 0x15);
}

u32 Game::world_frame() {
    cover(0x80033DC0u);
    Frame f(*this, 24);
    if (!m.r8(scene_of(m) + 933)) mission_mode_update();
    if (!paused(m)) m.w32(world_of(m) + 12, m.r32(world_of(m) + 12) + 1);
    const u32 wf = m.r32(world_of(m));
    if (wf & 0x40000000u) {
        m.w32(world_of(m), wf & 0xBFFFFFFFu);
        request_exit(0x10000000u, 0);
    } else if (wf & 0x80000000u) {
        m.w8(scene_of(m) + 2391, 1);
        m.w32(world_of(m), m.r32(world_of(m)) & 0x7FFFFFFFu);
        request_exit(0x800000u, 0);
    }
    const u32 w = world_of(m);
    if (!(m.r32(w) & 0x20000000u)) return 0;
    const u32 x = m.r32(g::kExit);
    const u32 old = m.r32(w + 4), old_k = m.r32(w + 8);
    m.w32(w, m.r32(w) & ~0x20000000u);
    m.w32(w + 4, m.r32(x + 112));
    m.w32(w + 12, 0);
    m.w32(w + 8, m.r32(x + 116));
    return phase_change(old, old_k);
}

u32 Game::phase_change(u32 old, u32 old_k) {
    cover(0x80032080u);
    Frame f(*this, 40);
    const u32 now = m.r32(world_of(m) + 4), k = m.r32(world_of(m) + 8);
    u32 flow = 0;
    if (old == 64) phase_leave_64();
    if (now == 2) phase_enter_intro(k);
    if (now == 16) phase_enter_16(k);
    if (now == 128) phase_enter_128();
    if (now == 1024) phase_enter_restart(k);
    if (now == 0x4000) phase_enter_4000(k);
    if (now == 8) phase_script_8(old_k);
    if (now == 64) phase_script_64(old_k);
    if (now == 0x2000) phase_script_2000(old_k);
    if (now == 0x10000 || now == 0x80000) phase_script_end(old_k);
    auto leave = [&](u32 code) {
        sound_reset();
        cd_reset(0, 0);
        return code;
    };
    if (now == 1) {
        flow = m.r32(0x800B09D4u);
        cd_reset(0, 0);
    }
    if (now == 2) flow = m.r32(0x800B09D8u);
    if (now == 16) flow = m.r32(0x800B09DCu);
    if (now == 128) flow = m.r32(0x800B09E0u);
    if (now == 1024) flow = m.r32(0x800B09E4u);
    if (now == 0x4000) flow = m.r32(0x800B09E8u);
    if (now == 0x20000) flow = m.r32(0x800B09ECu);
    if (now == 0x100000) flow = m.r32(0x800B09F0u);
    if (now == 0x200000) flow = m.r32(0x800B09F4u);
    if (now == 0x4000000) flow = m.r32(0x800B0A0Cu);
    if (now == 0x10000000) {
        const u32 c = scene_of(m) + 940;
        const u32 code = m.r32(0x800B0A00u);
        m.w16(c, m.r16(c) + 1u);
        flow = leave(code);
    }
    if (now == 0x400000) flow = leave(m.r32(0x800B09F8u));
    if (now == 0x800000) flow = leave(m.r32(0x800B09FCu));
    if (now == 0x1000000) flow = leave(m.r32(0x800B0A04u));
    if (now == 0x2000000) flow = leave(m.r32(0x800B0A08u));
    return flow;
}

void Game::phase_enter_intro(u32 k) {
    cover(0x80031E64u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(md_of(m) + 4 * k + 8));
    intro_setup();
}

void Game::phase_enter_16(u32 k) {
    cover(0x80031EA4u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(md_of(m) + 4 * k + 472));
    phase16_setup();
}

void Game::phase_enter_restart(u32 k) {
    cover(0x80031F24u);
    Frame f(*this, 24);
    msn_vm_exec(m.r32(md_of(m) + 4 * k + 568));
    restart_setup();
}

void Game::phase_enter_4000(u32 k) {
    cover(0x80031F64u);
    Frame f(*this, 24);
    view_reset();
    msn_vm_exec(m.r32(md_of(m) + 4 * k + 536));
}

void Game::view_reset() {
    cover(0x800798A8u);
    Frame f(*this, 32);
    const u32 w = world_of(m);
    const u32 a = m.r32(w + 580);
    view_init(m.r32(w + 40), 296, m.r16(a + 12), m.r16(a + 14), m.r32(m.r32(g::kExit) + 12) + 4);
}

void Game::view_init(u32 v, u32 h, u32 a3, u32 a4, u32 angles) {
    cover(0x80077880u);
    Frame f(*this, 24);
    m.w16(v + 28, static_cast<u32>(-160));
    m.w16(v + 30, static_cast<u32>(-120));
    m.w32(v, 0);
    m.w16(v + 36, h);
    m.w32(v + 196, 0);
    m.w32(v + 200, 0);
    m.w16(v + 32, 320);
    m.w16(v + 34, 240);
    m.w16(v + 40, a3);
    m.w16(v + 42, a4);
    m.w16(v + 44, m.r16(angles) << 6);
    m.w16(v + 46, m.r16(angles + 2) << 6);
    const u32 az = m.r16(angles + 4);
    m.w32(v + 8, static_cast<u32>(-4915200));
    m.w32(v + 4, 0);
    m.w32(v + 12, 0);
    for (u32 o : {20u, 22u, 24u, 180u, 182u, 184u}) m.w16(v + o, 0);
    m.w32(v + 188, 0xFFFFFFFFu);
    m.w32(v + 192, 0xFFFFFFFFu);
    m.w32(v + 236, 0);
    m.w32(v + 240, 0);
    for (u32 o : {284u, 286u, 288u}) m.w16(v + o, 0);
    m.w16(v + 48, az << 6);
    for (u32 o : {268u, 272u, 276u}) m.w32(v + o, 0);
    for (u32 o : {324u, 326u, 328u}) m.w16(v + o, 0);
    for (u32 o : {308u, 312u, 316u, 292u, 296u, 300u, 332u, 336u, 340u}) m.w32(v + o, 0);
    m.w16(v + 348, 0);
    m.w16(v + 350, 0);
    m.w32(v + 80, m.r8(scene_of(m) + 927) ? 2u : 1u);
    view_set(v, m.r32(v + 80), slot(m.r32(v + 200)));
}

void Game::mission_timer_setup() {
    cover(0x80053DA0u);
    const u32 ms = ms_of(m), k = m.r32(world_of(m) + 8);
    m.w32(ms + 16, 0);
    m.w8(ms + 28, m.r8(params_of(m) + k + 52));
    const u32 k2 = m.r32(world_of(m) + 8);
    if (m.r8(params_of(m) + k2 + 52) == 1) m.w32(ms_of(m) + 20, m.r32(params_of(m) + 4 * k2 + 56));
}

void Game::air_phase_units() {
    cover(0x8004DB40u);
    Frame f(*this, 40);
    const u32 want = m.r8(0x800B1FC0u + m.r32(world_of(m) + 8));
    const u32 any = want & 0xF0u;
    for (u32 k = 2; k < 32; ++k) {
        const u32 e = slot(k);
        if (!(m.r32(e) & 0x4000u) || m.r16s(e + 78) <= 0) continue;
        const u32 d = m.r32(m.r32(g::kSlotRecords) + (k << 6) + 12);
        const u32 a = m.r8(d + 29);
        if (a == want || a == any) air_show(k);
        const u32 b = m.r8(d + 30);
        if (b == want || b == any) air_activate(k);
    }
}

void Game::ground_phase_units() {
    cover(0x8004E620u);
    Frame f(*this, 40);
    const u32 want = m.r8(0x800B2024u + m.r32(world_of(m) + 8));
    const u32 any = want & 0xF0u;
    for (u32 k = 0; k < 32; ++k) {
        const u32 e = slot(32 + k);
        if (!(m.r32(e) & 0x4000u) || m.r16s(e + 78) <= 0) continue;
        const u32 d = m.r32(m.r32(g::kGroundRecords) + 40 * k + 8);
        const u32 a = m.r8(d + 29);
        if (a == want || a == any) ground_show(k);
        const u32 b = m.r8(d + 30);
        if (b == want || b == any) ground_activate(k);
    }
    const u32 k = m.r32(world_of(m) + 8);
    if (m.r16(scene_of(m) + 908) == 13 && !k) zone_timers_reset();
    if (m.r16(scene_of(m) + 908) == 21 && m.r32(world_of(m) + 8) == 1) run_script_328();
    if (m.r16(scene_of(m) + 908) == 43 && !m.r32(world_of(m) + 8)) scripts_run_reset();
    if (m.r16(scene_of(m) + 908) == 55 && !m.r32(world_of(m) + 8)) ground_shuffle();
    lock_reset();
}

void Game::lock_reset() {
    cover(0x80051824u);
    m.w8(0x800BCFF0u, 0xFF);
    m.w8(0x800BCFF1u, 0xFF);
    m.w32(0x800BCFF4u, 0x100000);
}

void Game::ground_swap(u32 a, u32 b) {
    cover(0x80053B24u);
    const u32 p = m.r32(g::kGroundRecords) + 40 * a + 24;
    const u32 q = m.r32(g::kGroundRecords) + 40 * b + 24;
    const u32 t = m.r32(p);
    m.w32(p, m.r32(q));
    m.w32(q, t);
}

void Game::ground_shuffle() {
    cover(0x800539D0u);
    Frame f(*this, 48);
    for (u32 i = 0; i < 4; ++i) {
        const u32 a = m.r8(0x800B20C8u + i);
        if (rand() & 1) ground_swap(a, a + 1);
    }
    for (u32 i = 0; i < 6; ++i) {
        const u32 a = m.r8(0x800B20CCu + i);
        switch (rand() % 6) {
        case 1: ground_swap(a + 1, a + 2); break;
        case 2: ground_swap(a, a + 1); break;
        case 3:
            ground_swap(a, a + 1);
            ground_swap(a + 1, a + 2);
            break;
        case 4:
            ground_swap(a + 1, a + 2);
            ground_swap(a, a + 1);
            break;
        case 5: ground_swap(a, a + 2); break;
        default: break;
        }
    }
}

void Game::alert_channels_init() {
    cover(0x80030464u);
    Frame f(*this, 24);
    alert_levels_reset();
    alert_channels_open(2);
}

void Game::alert_levels_reset() {
    cover(0x80030178u);
    m.w32(0x800BCEA0u, 0xFFFFFF00u);
    m.w16(0x800BCEA4u, 0x5400);
    m.w8(0x800BCEA6u, 0x55);
}

void Game::alert_channels_open(u32 n) {
    cover(0x8002F500u);
    Frame f(*this, 56);
    m.w32(0x800C6610u, n);
    u32 k = 0;
    for (; static_cast<s32>(k) < static_cast<s32>(n); ++k) {
        const u32 c = 0x800C6610u + k;
        m.w8(c + 4, 6 + 3 * k);
        m.w8(c + 8, 7 + 3 * k);
        m.w8(c + 12, 8 + 3 * k);
        const u32 a = 0x800C6630u + 92 * k;
        m.w32(a, 0);
        m.w32(a + 8, 0xFFFFFFFFu);
        fade_reset(0x800C6654u + 92 * k);
        m.w8(a + 48, 0);
        for (u32 o = 80; o <= 90; o += 2) m.w16(a + o, 0);
    }
    for (; k < 4; ++k) {
        const u32 c = 0x800C6610u + k;
        m.w8(c + 4, 0xFF);
        m.w8(c + 8, 0xFF);
        m.w8(c + 12, 0xFF);
    }
}

void Game::alert_channels_stop() {
    cover(0x8002F620u);
    Frame f(*this, 48);
    m.w32(0x800C6610u, 0);
    for (u32 k = 0; k < 4; ++k) {
        const u32 c = 0x800C6610u + k;
        voice_release(m.r8(c + 4));
        voice_release(m.r8(c + 8));
        voice_release(m.r8(c + 12));
        m.w8(c + 4, 255);
        m.w8(c + 8, 255);
        m.w8(c + 12, 255);
        const u32 a = 0x800C6630u + 92 * k;
        m.w32(a, 0);
        m.w32(a + 8, 0xFFFFFFFFu);
        fade_reset(0x800C6654u + 92 * k);
        m.w8(a + 48, 0);
    }
}

void Game::weapons_reset() {
    cover(0x8006C530u);
    const u32 wp = m.r32(g::kWeapons);
    m.w32(wp + 4, 0);
    m.w32(wp, 0);
    for (u32 k = 0; k < 64; ++k) {
        m.w8(wp + 8 + k, 0xFF);
        m.w8(wp + 72 + k, 0);
        m.w8(wp + 136 + k, 0);
    }
    m.w32(wp + 5452, 0);
    for (u32 o = 13276; o >= 13264; o -= 4) m.w32(wp + o, 0);
    m.w32(wp + 5448, 0);
    m.w32(wp + 13280, 0);
}

u32 Game::mem_alloc(u32 n) {
    cover(0x80027410u);
    Frame f(*this, 24);
    const u32 c = m.r32(scene_of(m) + 28);
    const u32 size = ((n + 3) >> 2) << 2;
    m.w32(scene_of(m) + 28, c + size);
    if (kHeapLimit >= c + size) return c;
    unported(0x800ADFB8u, "MemAlloc's overflow message");
    return 0;
}

u32 Game::mem_peek(u32 n) {
    cover(0x80027478u);
    Frame f(*this, 24);
    const u32 size = ((n + 3) >> 2) << 2;
    const u32 end = m.r32(scene_of(m) + 28) + size;
    if (kHeapLimit >= end) return m.r32(scene_of(m) + 28);
    unported(0x800ADFB8u, "MemNoAlloc's overflow message");
    return 0;
}

u32 Game::word_copy(u32 dst, u32 src, u32 n) {
    cover(0x8002759Cu);
    for (s32 i = 0; i < static_cast<s32>(n); ++i, dst += 4, src += 4) m.w32(dst, m.r32(src));
    return dst;
}

void Game::player_start_mode(u32 start) {
    cover(0x80073928u);
    const s32 type = m.r16s(slot(0) + 72);
    const u32 i = start & 0xFFu;
    const u32 rec = 0x800B3B94u + 56 * i;
    auto runway = [&](u32 kind) {
        m.w32(0x800BCE14u, kind);
        m.w32(0x800BCE1Cu, 0x800B3E6Cu + 20 * i);
        m.w32(0x800BCE20u, 0x800B3EBCu + static_cast<u32>(16 * type));
    };
    if (start & 0x100u) {
        m.w32(0x800BCE14u, 0);
        m.w32(0x800BCE18u, rec);
    } else if (start & 0x200u) {
        m.w32(0x800BCE14u, 1);
        m.w32(0x800BCE18u, rec);
    } else if (start & 0x400u) {
        runway(2);
    } else if (start & 0x800u) {
        runway(3);
    } else if (start & 0x1000u) {
        m.w32(0x800BCE14u, 4);
    } else if (start & 0x2000u) {
        m.w32(0x800BCE14u, 5);
        m.w32(0x800BCE18u, rec);
    } else if (start & 0x4000u) {
        m.w32(0x800BCE14u, 6);
        m.w32(0x800BCE18u, rec);
    }
}

void Game::view_start(u32 start) {
    cover(0x8007961Cu);
    Frame f(*this, 24);
    const u32 mode = (start & 0x6300u) ? 1024u : m.r8(scene_of(m) + 927) ? 2u : 1u;
    view_set(m.r32(world_of(m) + 40), mode, slot(0));
    view_flag(1);
}

void Game::view_follow_player(u32 alt) {
    cover(0x800794D8u);
    Frame f(*this, 24);
    const u32 v = m.r32(world_of(m) + 40);
    if (m.r32(v + 80) & 0x800u) return;
    view_set(v, alt ? 2u : 1u, slot(0));
    view_flag(1);
}

void Game::player_mode_80076290() {
    cover(0x80076290u);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p, 1);
    m.w32(p + 4, 0x1000000);
    m.w32(p + 8, 0);
}

void Game::player_mode_800762B0() {
    cover(0x800762B0u);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p, 1);
    m.w32(p + 4, 0x2000000);
    m.w32(p + 8, 0);
}

void Game::player_mode_800762D0() {
    cover(0x800762D0u);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p, 2048);
    m.w32(p + 4, 4);
    m.w32(p + 8, 0);
}

void Game::player_mode_80076DBC() {
    cover(0x80076DBCu);
    const u32 p = m.r32(g::kPlayer);
    m.w32(p + 16, 0);
    m.w32(m.r32(kPlayerWeapons) + 36, 0);
    m.w32(p, 1024);
    m.w32(p + 4, 0x2000);
    m.w32(p + 8, 0);
}

void Game::player_mode_hold(u32 a) {
    cover(0x800762F0u);
    const u32 p = m.r32(g::kPlayer);
    const u32 v = m.r32(p);
    m.w32(p + 4, a ? 2u : 0x400000u);
    m.w32(p + 8, 0);
    m.w32(p, v | 0x40u);
}

void Game::message_reset() {
    cover(0x8004CE9Cu);
    const u32 p = m.r32(kMessages);
    m.w32(p + 272, 0);
    m.w32(p + 284, 255);
}

void Game::effects_reset() {
    cover(0x800849ECu);
    const u32 fx = m.r32(kFx);
    for (u32 k = 0; k < 3; ++k) {
        m.w32(fx + 1536 + 4 * k, 0);
        m.w32(fx + 4236 + 4 * k, 0);
    }
    for (s32 k = 9; k >= 0; --k) m.w32(fx + 4248 + 4 * static_cast<u32>(k), 0);
    m.w32(fx + 14020, 0);
    for (s32 k = 31; k >= 0; --k) m.w32(fx + 14040 + 20 * static_cast<u32>(k), 0);
    m.w32(fx + 13248, 0);
    m.w32(fx + 14664, 0);
    m.w32(fx + 14668, 0);
    m.w32(fx + 14692, 0);
    m.w32(fx + 15144, 0);
}

void Game::target_hud_reset() {
    cover(0x8003A07Cu);
    m.w8(0x800C6D69u, 0xFF);
    m.w8(0x800C6D6Fu, 0);
    m.w16(0x800C6D66u, 0);
    m.w8(0x800C6D68u, 0);
    m.w8(0x800C6D6Au, 0);
    m.w8(0x800C6D6Bu, 0);
    m.w8(0x800C6D6Cu, 0x83);
    m.w8(0x800C6D6Du, 0);
    m.w8(0x800C6D71u, 0);
}

void Game::ground_start_units() {
    cover(0x8004E80Cu);
    Frame f(*this, 40);
    if (m.r8(0x800B1FC4u + 4 * m.r32(kPhaseKind)) - 5u >= 2u) return;
    for (u32 k = 0; k < 32; ++k) {
        const u32 e = slot(32 + k);
        if (!(m.r32(e) & 0x4000u) || m.r16s(e + 78) <= 0) continue;
        const u32 t = m.r8(m.r32(m.r32(g::kGroundRecords) + 40 * k + 8) + 29);
        if (t == 17 || t == 16) ground_show(k);
    }
}

void Game::wingmen_start() {
    cover(0x8004DC38u);
    Frame f(*this, 48);
    const u32 rec = 0x800B1FC4u + 4 * m.r32(kPhaseKind);
    const u32 kind = m.r8(rec);
    bool stand_down = true;
    if (kind - 2u >= 2u && kind >= 2 && (kind != 7 || mission_of(m) == 7)) stand_down = false;
    if (stand_down) {
        for (u32 k = 2; k < 32; ++k) {
            const u32 e = slot(k);
            const u32 v = m.r32(e);
            if ((v & 0x40u) && (m.r32(g::kSlotFlags + 4 * k) & 0x400000u)) m.w32(e, v & 0xFFFFFBFFu);
            else m.w32(e, m.r32(e) & 0xFFFFBFFFu);
        }
    }
    for (u32 i = 0; i < 2; ++i) {
        const u32 s = m.r8(rec + 2 + i);
        if (!s) continue;
        const u32 e = slot(s);
        m.w32(e, m.r32(e) | 0x4000u);
        const u32 ai = m.r32(g::kAiInfo) + 184 * s;
        air_show(s);
        air_activate(s);
        m.w8(ai + 28, 8);
        m.w8(ai + 29, 0);
        m.w8(ai + 30, 0);
        m.w32(ai + 56, 0);
        m.w8(ai + 62, 0);
        m.w8(ai + 100, 0);
        const u32 fl = g::kSlotFlags + 4 * s;
        m.w32(fl, m.r32(fl) & ~0x100000u);
        for (u32 o : {4u, 6u, 8u}) m.w16(ai + o, 0);
        for (u32 o : {12u, 16u, 20u}) m.w32(ai + o, 0);
        for (u32 o : {128u, 130u, 132u}) m.w16(ai + o, 0);
        switch (m.r8(rec)) {
        case 2:
        case 3: {
            const u32 w = slot(s), p = slot(0);
            for (u32 o = 8; o <= 20; o += 4) m.w32(w + o, m.r32(p + o));
            m.w32(w + 16, m.r32(w + 16) + 0x1000000u);
            m.w32(w + 12, m.r32(w + 12) - 0x100000u);
            if (m.r32s(scene_of(m) + 916) >= 2) {
                m.w32(w + 8, (rand() & 1) ? m.r32(w + 8) + 0x400000u : m.r32(w + 8) - 0x400000u);
            }
            m.w32(w + 24, m.r32(p + 24));
            m.w32(w + 28, m.r32(p + 28));
            for (u32 o = 56; o <= 68; o += 4) m.w32(w + o, m.r32(p + o));
            m.w32(w + 40, m.r32(p + 64));
            m.w8(ai + 61, 19);
            break;
        }
        case 5:
        case 6: {
            const u32 r = m.r32(kAirStarts) + 44 * s;
            for (u32 o = 0; o < 24; o += 4) m.w32(e + 8 + o, m.r32(r + 4 + o));
            m.w8(ai + 61, 20);
            break;
        }
        case 7: {
            const u32 w = slot(s), p = slot(0);
            for (u32 o = 8; o <= 20; o += 4) m.w32(w + o, m.r32(p + o));
            m.w32(w + 8, m.r32(w + 8) + 150u * m.r32(p + 56));
            m.w32(w + 16, m.r32(w + 16) + 150u * m.r32(p + 64));
            m.w32(w + 24, m.r32(p + 24));
            m.w32(w + 28, m.r32(p + 28));
            m.w8(ai + 61, i ? 22 : 21);
            break;
        }
        default: break;
        }
    }
}

u32 Game::player_mode() {
    cover(0x8006CB30u);
    return m.r32(m.r32(g::kPlayer));
}

u32 Game::radio_busy() {
    cover(0x8005650Cu);
    return m.r32(kRadioState);
}

void Game::alert_channels_stop_frame() {
    cover(0x800304CCu);
    Frame f(*this, 24);
    alert_channels_stop();
}

void Game::warning_reset() {
    cover(0x80042524u);
    m.w32(kWarning, 0xFFFFFFFFu);
    m.w32(kWarning + 4, 0);
    m.w8(kWarning + 8, 0);
    m.w8(kWarning + 12, 0);
}

void Game::warning_raise(u32 k) {
    cover(0x8004287Cu);
    const s32 cur = m.r32s(kWarning);
    if (cur != -1 && !(static_cast<s32>(k) < cur)) return;
    const u32 e = 0x800B149Cu + 4 * k;
    m.w32(kWarning, k);
    m.w8(kWarning + 8, 1);
    m.w32(kWarning + 4, static_cast<u32>(m.r16s(e)));
    m.w16(kWarning + 10, 0);
    m.w8(kWarning + 9, m.r8(e + 2));
    m.w8(kWarning + 12, 1);
    m.w16(kWarning + 14, 0);
    m.w8(kWarning + 13, m.r8(e + 3));
}

void Game::wrecks_clear() {
    cover(0x8004596Cu);
    Frame f(*this, 32);
    for (u32 k = 2; k < 32; ++k)
        if (m.r32(g::kSlotFlags + 4 * k) & 2u) entity_destroy(k);
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        const u32 fl = g::kSlotFlags + 4 * k;
        if (!(m.r32(fl) & 4u)) continue;
        m.w8(kWreckTimer + k, 0);
        m.w32(fl, m.r32(fl) & ~4u);
        m.w32(e, m.r32(e) & ~0x4000u);
    }
}

u32 Game::noise_fade(u32 ot, u32 p, s32 level) {
    cover(0x80084560u);
    Frame f(*this, 32);
    const s32 n = level >> 8;
    if (n <= 0) return p;
    const u32 t = p + 0x3C;
    const u32 q = p + 8;
    m.w32(p, (q & 0x00FFFFFFu) | 0x01000000u);
    m.w32(p + 4, 0xE6000000u);
    m.w32(p + 8, (t & 0x00FFFFFFu) | 0x0C000000u);
    m.w32(q + 4, 0xA0000000u);
    m.w32(q + 8, 0x010001D0u);
    m.w32(q + 12, 0x00010010u);
    m.w32(q + 0x30, 0x01000000u);
    for (s32 i = 0; i < 16; ++i) m.w16(q + 0x10 + 2 * static_cast<u32>(i), i < 16 - n ? 0u : 0x8000u);
    m.w32(t, ((t + 0x10) & 0x00FFFFFFu) | 0x03000000u);
    m.w32(t + 4, 0xE1000238u);
    m.w32(t + 8, 0xE20C4318u);
    m.w32(t + 12, 0);
    gte.mtc2(6, 0x64838383u);
    gte.mtc2(8, 0);
    const u32 end = noise_sprites(t + 0x10, 0x401D, 0xFF88FF60u, 0xF00140u);
    m.w32(end + 4, 0xE1000220u);
    m.w32(end, 0x03000000u);
    m.w32(end + 8, 0xE2000000u);
    m.w32(end + 12, 0);
    m.w32(end, (m.r32(end) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    return end + 0x10;
}

void Game::scenery_fires(u32 k) {
    cover(0x8009423Cu);
    Frame f(*this, 48);
    u32 t, n;
    switch (k) {
    case 0: t = 0x800B445Cu; n = 7; break;
    case 1: t = 0x800B445Cu; n = 5; break;
    case 2: t = 0x800B44B0u; n = 5; break;
    case 3: t = 0x800B44ECu; n = 12; break;
    case 4: t = 0x800B457Cu; n = 10; break;
    default: return;
    }
    for (u32 i = 0; i < n; ++i, t += 12) {
        const u32 e = pool_take(m.r32(kFx) + 14020, 1, 5);
        if (!e) break;
        m.w16(e, 2 * i + static_cast<u32>(rand() & 1));
        m.w16(e + 2, 0x7FFF);
        m.w32(e + 4, m.r32(t));
        m.w32(e + 8, m.r32(t + 4));
        m.w32(e + 16, 0x80089CD8u);
        m.w32(e + 12, m.r32(t + 8));
    }
}

void Game::mission_mode_update() {
    cover(0x800371C4u);
    Frame f(*this, 56);
    const u32 player = slot(0);
    mission_frame();
    const u32 w = world_of(m);
    const u32 sub = m.r32(w + 4);
    auto abort_to = [&](u32 arg) {
        m.w8(ms_of(m) + 42, 5);
        m.w8(ms_of(m) + 41, 9);
        request_exit(8, arg);
        scene_or(m, 0x10000000u);
    };
    if (sub & 2u) {
        const u32 ms = ms_of(m);
        scene_or(m, 0x10000000u);
        if (m.r8(ms + 63) == 1 && !m.r32(w + 8)) {
            const s32 t = m.r32s(w + 12);
            if (t < 16) m.w32(world_of(m) + 20, noise_fade(m.r32(w + 16) + 4, m.r32(w + 20), 4096 - (t << 8)));
        }
        if (!m.r32(world_of(m) + 12)) {
            const s32 mission = mission_of(m);
            if (mission == 42) player_mode_800762B0();
            else if (!m.r32(world_of(m) + 8) && (mission == 6 || mission == 14 || mission == 44)) player_mode_hold(0);
            else player_mode_800762D0();
            view_follow_player_by_option();
        }
        if (player_mode() & 0x7000u) {
            m.w8(ms_of(m) + 41, 9);
            m.w8(ms_of(m) + 42, 5);
            request_exit(8, 1);
            scene_or(m, 0x10000000u);
            radio_stop();
            cd_cancel_streams();
            return;
        }
        if (m.r32s(world_of(m) + 12) >= 61 && !radio_busy()) {
            if (mission_of(m) == 42) player_mode_80076290();
            else player_mode_flying();
            view_follow_player(m.r8(scene_of(m) + 927));
            request_exit(4, m.r32(world_of(m) + 8));
            const u32 w2 = world_of(m);
            m.w32(scene_of(m), m.r32(scene_of(m)) & ~0x10000000u);
            message_show(m.r8(params_of(m) + m.r32(w2 + 8) + 72));
        }
        return;
    }
    if (sub & 4u) {
        bool stop_radio = false, stop_cd = false;
        if (player_mode() & 0x7000u) {
            abort_to(1);
            stop_radio = stop_cd = true;
        } else if (m.r32(ms_of(m) + 16) == m.r32(params_of(m) + 4 * m.r32(world_of(m) + 8) + 36)) {
            abort_to(4);
            stop_radio = stop_cd = true;
        } else {
            const u32 pr = params_of(m);
            const s32 x = m.r32s(player + 8), z = m.r32s(player + 16);
            const s32 mission = mission_of(m);
            const bool out = x < m.r32s(pr + 16) || m.r32s(pr + 20) < x || z < m.r32s(pr + 24) ||
                             m.r32s(pr + 28) < z || (mission == 58 && m.r32s(player + 12) > -3145728) ||
                             (mission == 42 && m.r32(player + 12) + 245760000u > 0x124F8000u);
            if (out) {
                const s32 mi = mission_of(m);
                if (mi != 32 && mi != 47 && mi != 53) {
                    abort_to(8);
                    stop_radio = stop_cd = true;
                }
            } else if (m.r8(ms_of(m) + 41) == 9) {
                request_exit(8, m.r8(ms_of(m) + 39));
                const s32 mi = mission_of(m);
                scene_or(m, 0x10000000u);
                stop_radio = mi == 6;
                stop_cd = true;
            }
        }
        if (stop_radio) radio_stop();
        if (stop_cd) cd_cancel_streams();
        s32 left = m.r32s(params_of(m) + 4 * m.r32(world_of(m) + 8) + 36) - m.r32s(ms_of(m) + 16);
        bool beep = left < 1800;
        if (left == 1800) {
            message_show(10);
            m.w8(ms_of(m) + 28, 1);
            m.w32(ms_of(m) + 20, 1800);
            beep = false;
        }
        if (!beep && m.r8(ms_of(m) + 28) == 1) {
            const s32 t = m.r32s(ms_of(m) + 20);
            if (t < 1800) {
                left = t;
                beep = true;
            }
        }
        if (beep && left == 30 * (left / 30)) sound_play(left < 301 ? 57 : 56);
        return;
    }
    if (sub & 8u) {
        scene_or(m, 0x10000000u);
        const u32 k = m.r32(w + 8);
        if (!((k & 0x62u) == 0 && k)) {
            if (player_mode() & 0x7000u) {
                m.w8(ms_of(m) + 41, 9);
                request_exit(8, 1);
                const u32 ms = ms_of(m);
                scene_or(m, 0x10000000u);
                m.w8(ms + 48, 0);
                radio_stop();
                m.w8(ms_of(m) + 42, 5);
                return;
            }
        }
        if (m.r32(world_of(m) + 8)) {
            const u32 kk = m.r32(world_of(m) + 8);
            const u32 msg = (kk & 2u) ? 0u : (kk & 0x20u) ? 1u : (kk & 0x40u) ? 50u : 2u;
            if (!m.r32(world_of(m) + 12)) {
                if (mission_of(m) == 58 && (m.r32(world_of(m) + 8) & 8u)) player_mode_hold(0);
                else if (!(m.r32(world_of(m) + 8) & 1u)) {
                    if (mission_of(m) == 42) player_mode_800762B0();
                    else player_mode_800762D0();
                }
                m.w8(kEndTimers + 12, 0);
            }
            u32 done;
            if (mission_of(m) == 6) {
                m.w32(kEndTimers + 4, 60);
                m.w32(kEndTimers + 8, 0);
                done = 90;
            } else {
                m.w32(kEndTimers + 8, 60);
                m.w32(kEndTimers + 4, 180);
                done = 210;
            }
            m.w32(kEndTimers, done);
            if (m.r32(world_of(m) + 12) == m.r32(kEndTimers + 8)) message_show(msg);
            if (m.r32s(world_of(m) + 12) >= m.r32s(kEndTimers + 4) && !m.r8(kEndTimers + 12) && !radio_busy()) {
                m.w8(ms_of(m) + 49, 2);
                m.w8(ms_of(m) + 48, 1);
                const u32 ms = ms_of(m);
                m.w8(kEndTimers + 12, 1);
                m.w16(ms_of(m) + 50, 30);
                m.w16(ms + 26, 0);
            }
            if (m.r32s(world_of(m) + 12) >= m.r32s(kEndTimers) && m.r8(kEndTimers + 12) == 1 && !radio_busy() &&
                !m.r8(ms_of(m) + 48)) {
                if (m.r8(ms_of(m) + 41)) sound_reset();
                else alert_channels_stop_frame();
                radio_stop();
                warning_reset();
                mission_end();
            }
            return;
        }
        if (!m.r32(world_of(m) + 12)) {
            s32 first = -1, done = 90;
            if (m.r8(ms_of(m) + 41)) {
                const s32 mission = mission_of(m);
                player_mode_800762D0();
                if (mission == 6 && (m.r8(ms_of(m) + 42) & 2u)) {
                    first = 0;
                    done = 30;
                } else if (mission == 23) {
                    first = 120;
                    done = 150;
                } else {
                    first = 60;
                }
            }
            m.w32(kEndTimers + 4, static_cast<u32>(first));
            m.w32(kEndTimers, static_cast<u32>(done));
        }
        if (m.r32(world_of(m) + 12) == m.r32(kEndTimers + 4)) {
            m.w8(ms_of(m) + 49, 2);
            m.w8(ms_of(m) + 48, 1);
            const u32 ms = ms_of(m);
            m.w16(ms_of(m) + 50, 30);
            m.w16(ms + 26, 0);
        }
        if (m.r32s(world_of(m) + 12) >= m.r32s(kEndTimers) && !radio_busy() && !m.r8(ms_of(m) + 48)) {
            if (m.r8(ms_of(m) + 41)) {
                sound_reset();
                effects_reset();
                wrecks_clear();
            } else {
                alert_channels_stop_frame();
            }
            warning_reset();
            radio_stop();
            mission_end();
        }
        return;
    }
    if (sub & 0x10u) {
        if (!m.r32(world_of(m) + 12) && mission_of(m) == 46 && m.r32(world_of(m) + 8) == 2) terrain_patch(0);
        return;
    }
    if (sub & 0x0021FFE0u) return;
    if (!(sub & 0x100000u)) return;
    if (!m.r32(world_of(m) + 12)) {
        m.w8(kEndTimers + 13, 0);
        const s32 mission = mission_of(m);
        const u32 pl = f.at(0x10);
        if (mission == 6 || mission == 9) {
            if (mission == 6) {
                m.w32(pl, 0xC7C00000u);
                m.w32(pl + 8, 0xD7C00000u);
                m.w32(pl + 4, 0xFF448000u);
                m.w16(pl + 16, 0);
                m.w16(pl + 18, static_cast<u32>(-8192));
            } else {
                m.w32(pl, 0);
                m.w32(pl + 4, 0xFE2B4000u);
                m.w32(pl + 8, 0);
                m.w16(pl + 16, 0);
                m.w16(pl + 18, 0);
            }
            m.w16(pl + 20, 0);
            player_place(pl);
        }
        player_mode_hold(0);
        view_follow_player(m.r8(scene_of(m) + 927));
        alert_channels_init();
    }
    if (m.r32(world_of(m) + 12) == 30) radio_queue(m.r8(ms_of(m) + 62), 0);
    if (!m.r8(kEndTimers + 13) && m.r32s(world_of(m) + 12) >= 151) m.w8(kEndTimers + 13, radio_busy() == 0);
    if (m.r8(kEndTimers + 13) == 1) {
        m.w8(ms_of(m) + 49, 2);
        m.w8(ms_of(m) + 48, 1);
        m.w8(kEndTimers + 13, 2);
        const u32 ms = ms_of(m);
        m.w16(ms_of(m) + 50, 30);
        m.w16(ms + 26, 0);
    }
    if (m.r8(kEndTimers + 13) == 2 && !m.r8(ms_of(m) + 48)) {
        mission_end();
        sound_reset();
    }
}

void Game::mission_frame() {
    cover(0x800540E0u);
    Frame f(*this, 40);
    const u32 sub = m.r32(world_of(m) + 4);
    const u32 k = m.r32(world_of(m) + 8);
    if (sub == 2) {
        if (m.r16(scene_of(m) + 908) == 15 && !k && !m.r32(world_of(m) + 12)) scenery_fires(3);
        if (m.r16(scene_of(m) + 908) == 35 && !m.r32(world_of(m) + 8) && !m.r32(world_of(m) + 12)) scenery_fires(4);
        if (m.r16(scene_of(m) + 908) == 46 && !m.r32(world_of(m) + 12)) {
            if (!m.r32(world_of(m) + 8)) scenery_fires(1);
            if (m.r32(world_of(m) + 8) == 1) scenery_fires(2);
        }
    }
    if (sub == 4) mission_frame_flight(k);
    if (sub == 2048 && !paused(m)) {
        const u32 ms = ms_of(m);
        if (!m.r8(ms + 28)) {
            m.w32(ms + 20, m.r32(ms + 12));
        } else if (m.r8(ms + 28) != 1 || m.r32s(ms + 20) <= 0) {
            m.w32(ms + 20, 0);
        } else {
            m.w32(ms + 20, m.r32(ms + 20) - 1);
        }
    }
    if (m.r32(world_of(m) + 4) & 0x102C0Eu) radio_update();
}

void Game::mission_frame_flight(u32 k) {
    if (!paused(m)) {
        for (u32 i = 0; i < 16; ++i) {
            if (m.r32(ms_of(m) + 16) == m.r16(md_of(m) + 32 * k + 2 * i + 344))
                msn_vm_exec(m.r32(md_of(m) + 64 * k + 4 * i + 72));
        }
        u32 ms = ms_of(m);
        if (m.r8(ms + 30)) {
            if (m.r8(ms + 30) == m.r8(ms + 32)) msn_vm_exec(m.r32(md_of(m) + 4 * k + 40));
            ms = ms_of(m);
        }
        if (m.r8(ms + 31) && m.r8(ms + 31) == m.r8(ms + 33)) msn_vm_exec(m.r32(md_of(m) + 4 * k + 56));
        if (m.r16(scene_of(m) + 908) != 9 || !m.r32(world_of(m) + 8)) m.w32(ms_of(m) + 12, m.r32(ms_of(m) + 12) + 1);
        ms = ms_of(m);
        const u32 mode = m.r8(ms + 28);
        m.w32(ms + 16, m.r32(ms + 16) + 1);
        if (!mode) {
            m.w32(ms + 20, m.r32(ms + 12));
        } else if (mode == 1) {
            const s32 t = m.r32s(ms + 20);
            m.w32(ms + 20, t <= 0 ? 0u : static_cast<u32>(t - 1));
        }
    }
    const u32 ms0 = ms_of(m);
    m.w32(ms0, m.r32(ms0) & ~0x10u);
    if (m.r16(scene_of(m) + 908) == 3 && !m.r32(world_of(m) + 8)) {
        if (paused(m)) return;
        const s32 d = m.r32s(m.r32(g::kRelation + 8) + 4);
        if (d < 4096) {
            m.w32(ms0 + 4, m.r32(ms0 + 4) + (d < 256 ? 3u : d < 1024 ? 2u : 1u));
            m.w32(ms_of(m), m.r32(ms_of(m)) | 0x10u);
        }
    }
    if (paused(m)) return;
    const s32 mission = mission_of(m);
    if ((mission == 14 || mission == 53) && !m.r32(world_of(m) + 8)) {
        if (m.r32(world_of(m) + 12) == 60) {
            if (mission == 14) m.w16(ms_of(m) + 24, m.r16(params_of(m) + 32) - 151u);
            if (m.r16(scene_of(m) + 908) == 53) m.w16(ms_of(m) + 24, m.r16(params_of(m) + 32) - 121u);
            m.w32(ms_of(m), m.r32(ms_of(m)) & ~2u);
        }
        const u32 ms = ms_of(m);
        const u32 up = m.r32(ms) & 2u;
        m.w16(ms + 24, m.r16(ms + 24) + 1u);
        if (up) {
            m.w8(ms + 28, 1);
            const u32 pr = params_of(m);
            m.w32(ms_of(m) + 20, static_cast<u32>(m.r16s(pr + 34) - m.r16s(ms_of(m) + 24)));
            if (m.r16(scene_of(m) + 908) == 14) {
                if (m.r16s(ms_of(m) + 24) == m.r16s(pr + 34) - 600) msn_vm_exec(m.r32(md_of(m) + 336));
                if (m.r16s(ms_of(m) + 24) >= m.r16s(params_of(m) + 34) - 600 && m.r32s(slot(0) + 12) > -81920000)
                    warning_raise(6);
            }
            if (m.r16s(ms_of(m) + 24) == m.r16s(params_of(m) + 34)) {
                if (m.r16(scene_of(m) + 908) == 53) msn_vm_exec(m.r32(md_of(m) + 336));
                const u32 ms2 = ms_of(m);
                const u32 v = m.r32(ms2);
                m.w16(ms2 + 24, 0);
                m.w32(ms2, v & 0xFFFFFFFDu);
            }
        } else {
            m.w8(ms + 28, 0);
            if (m.r16s(ms_of(m) + 24) == m.r16s(params_of(m) + 32) - 150) msn_vm_exec(m.r32(md_of(m) + 328));
            if (m.r16s(ms_of(m) + 24) == m.r16s(params_of(m) + 32)) {
                msn_vm_exec(m.r32(md_of(m) + 332));
                const u32 ms2 = ms_of(m);
                const u32 v = m.r32(ms2);
                m.w16(ms2 + 24, 0);
                m.w32(ms2, v | 2u);
                message_show(m.r16(scene_of(m) + 908) == 53 ? 32u : 17u);
            }
        }
    }
    if (m.r16(scene_of(m) + 908) == 14 && !m.r32(world_of(m) + 8)) {
        const u32 v = m.r32(ms_of(m));
        if (!(v & 1u)) {
            bool noise = (v & 2u) != 0;
            if (!noise) {
                if (m.r32s(slot(0) + 12) > -81920000) {
                    const u32 md = md_of(m);
                    m.w32(ms_of(m), v | 1u);
                    msn_vm_exec(m.r32(md + 340));
                }
                noise = (m.r32(ms_of(m)) & 2u) != 0;
            }
            if (noise && m.r32s(slot(0) + 12) > -81920000) {
                const u32 w = world_of(m);
                m.w32(world_of(m) + 20, noise_overlay(m.r32(w + 16) + 4, m.r32(w + 20), 765));
            }
        }
    }
    if (m.r16(scene_of(m) + 908) == 6 && m.r32s(slot(0) + 12) < -10485760) {
        radio_stop();
        msn_vm_exec(m.r32(md_of(m) + 328));
    }
    if (m.r16(scene_of(m) + 908) == 13) {
        if (m.r32(ms_of(m)) & 0x20u) m.w32(0x800BD010u, m.r32(0x800BD010u) + 1);
        else m.w32(0x800BD010u, 0);
        if (m.r32(0x800BD010u) == 18000) msn_vm_exec(m.r32(md_of(m) + 328));
    }
    if (m.r16(scene_of(m) + 908) == 44 && !m.r32(world_of(m) + 8) && !(m.r32(ms_of(m)) & 1u)) {
        const u32 p = slot(0);
        if (m.r32s(p + 12) < -6291456) warning_raise(4);
        if (m.r32s(p + 12) < -8388608) {
            const u32 md = md_of(m);
            m.w32(ms_of(m), m.r32(ms_of(m)) | 1u);
            msn_vm_exec(m.r32(md + 328));
        }
        if (m.r32(p + 8) + 32767999u <= 0x7147FFEu) {
            const s32 z = m.r32s(p + 16);
            if (z > -73728000 && z <= 20479999) {
                const u32 md = md_of(m);
                m.w32(ms_of(m), m.r32(ms_of(m)) | 1u);
                msn_vm_exec(m.r32(md + 332));
            }
        }
    }
    if (m.r16(scene_of(m) + 908) == 30 && !m.r32(world_of(m) + 8) && m.r32(world_of(m) + 12) - 1200u < 0xA8Du) {
        constexpr u32 kLevel = 0x800BD014u;
        const u32 r = static_cast<u32>(rand()) & 0xFFu;
        scene_or(m, 0x10000000u);
        const s32 t = m.r32s(world_of(m) + 12);
        const s32 lv = m.r32s(kLevel);
        if (t == 1200) {
            music_briefing();
            m.w32(kLevel, 0);
        } else if (t < 1320) {
            if (lv < 2048) m.w32(kLevel, static_cast<u32>(lv + 18));
        } else if (t < 1440) {
            m.w32(kLevel, 2048);
        } else if (t < 3150) {
            m.w32(kLevel, r + 768);
        } else if (t < 3450) {
            if (lv < 2048) m.w32(kLevel, static_cast<u32>(lv + 8));
        } else if (t < 3540) {
            if (lv < 2048) m.w32(kLevel, static_cast<u32>(lv + 16));
        } else if (t < 3720) {
            m.w32(kLevel, static_cast<u32>(-2048));
        } else if (t < 3900) {
            if (lv < 0) m.w32(kLevel, static_cast<u32>(lv + 16));
        }
        const u32 w = world_of(m);
        m.w32(world_of(m) + 20, noise_overlay(m.r32(w + 16) + 4, m.r32(w + 20), m.r32s(kLevel)));
    }
    if (m.r16(scene_of(m) + 908) == 42) {
        const s32 y = m.r32s(slot(0) + 12);
        const u32 t = m.r32(world_of(m) + 12) & 0x1Fu;
        if (y >= 0 && !t) zone_penalty();
        if (y > 20479999 && t == 16) zone_penalty();
        if (y > 32767999 && (t == 8 || t == 24)) zone_penalty();
    }
    const u32 ms = ms_of(m);
    const u32 v = m.r32(ms) & 0xFFFFFFFBu;
    m.w32(ms, v);
    if (m.r16(scene_of(m) + 908) == 23 && !m.r32(world_of(m) + 8)) {
        const s32 x = m.r32s(slot(0) + 8);
        if ((x < 0 ? wrap::neg(x) : x) <= 155647999) {
            const s32 z = m.r32s(slot(0) + 16);
            if ((z < 0 ? wrap::neg(z) : z) <= 155647999 && m.r32s(slot(0) + 12) > -62914560) m.w32(ms, v | 4u);
        }
    }
    if (m.r16(scene_of(m) + 908) == 55) {
        if (m.r32(scene_of(m) + 916)) {
            if (m.r32s(slot(0) + 16) > 876544000) msn_vm_exec(m.r32(md_of(m) + 328));
        } else if (m.r32s(slot(0) + 16) > 299008000) {
            msn_vm_exec(m.r32(md_of(m) + 332));
        }
    }
}

void Game::radio_hold() {
    cover(0x80041654u);
    if (m.r8(kRadioHold) != 2) m.w8(kRadioHold, 1);
}

void Game::radio_unhold() {
    cover(0x8004167Cu);
    if (m.r8(kRadioHold) == 2) m.w8(kRadioHold, 3);
}

u32 Game::radio_hold_state() {
    cover(0x80041670u);
    return m.r8(kRadioHold);
}

void Game::radio_face(u32 k, u32 a, u32 b) {
    cover(0x8002CC44u);
    m.w8(0x800BCE96u, 0);
    m.w8(0x800BCE95u, 0);
    m.w8(0x800BCE94u, 0);
    m.w8(0x800BCE8Fu, 0);
    m.w8(kSubtitleLine, 0);
    for (u32 i = 0; i < 3; ++i) {
        m.w8(m.r32(kFace) + i + 4, 0);
        m.w8(m.r32(kFace) + i + 7, 0);
        m.w16(m.r32(kFace) + 2 * i + 10, 0);
    }
    const u32 p = m.r32(kFace);
    if (k == 255) {
        m.w8(0x800BCE8Cu, 0);
        return;
    }
    m.w8(0x800BCE8Cu, 1);
    const u32 t = 0x800B03ACu + 20 * k;
    m.w32(p, k);
    m.w32(p + 16, a);
    m.w32(p + 20, b);
    m.w16(p + 24, 4 * m.r8(t + 2));
    m.w16(p + 26, 6 * m.r8(t + 2));
    const u32 h = m.r16(t + 16);
    m.w32(0x800BCE90u, 0xFFFFFFFFu);
    m.w16(p + 28, ((h & 0x3FFu) >> 6) | 0x30u);
}

u32 Game::radio_face_busy() {
    cover(0x8002D544u);
    return (m.r32(m.r32(kFace) + 4) & 0x00FFFFFFu) != 0;
}

void Game::radio_play_script(u32 script) {
    cover(0x8005671Cu);
    Frame f(*this, 24);
    const u32 st = m.r32(kRadioScript);
    m.w8(st + 7, 0xFF);
    m.w8(st + 6, 0xFF);
    m.w32(st, script);
    m.w16(st + 4, 0);
    m.w8(m.r32(kRadioScript) + 9, 0);
    m.w8(m.r32(kRadioScript) + 8, 0);
    m.w32(kRadioState, 2);
    m.w32(m.r32(kRadioScript) + 12, 0);
    radio_face(0, 0, 0);
}

void Game::radio_stream_start(s32 mission, u32 k) {
    cover(0x80056A54u);
    Frame f(*this, 32);
    cd_cancel_streams();
    if (k == 255) return;
    cd_request_stream(m.r32(0x800B29F4u + 4 * static_cast<u32>(mission)) + 12 * k, 1, 0x80056A44u);
}

void Game::radio_play_stream(u32 k) {
    cover(0x800565E8u);
    Frame f(*this, 24);
    m.w32(kRadioState, 1);
    const s32 mission = mission_of(m);
    m.w32(kRadioStream, 2);
    radio_stream_start(mission, k);
    radio_face(0, 0, 0);
}

void Game::music_briefing() {
    cover(0x80056A10u);
    Frame f(*this, 24);
    cd_cancel_streams();
    cd_request_stream(0x800B2328u, 0, 0);
}

void Game::radio_stop() {
    cover(0x80056518u);
    Frame f(*this, 24);
    switch (m.r32(kRadioState)) {
    case 1: cd_cancel_streams(); break;
    case 2:
        if (m.r8(m.r32(kRadioScript) + 7) != 255) voice_release(m.r8(m.r32(kRadioScript) + 7));
        radio_play_script(0);
        break;
    case 3:
        m.w32(kRadioState, 0);
        radio_unhold();
        break;
    default: break;
    }
    subtitle_close(0);
    m.w32(m.r32(kRadioQueue), 255);
    for (u32 i = 0; i < 4; ++i) {
        const u32 q = m.r32(kRadioQueue);
        m.w8(q + i + 20, 0xFF);
        m.w16(q + 2 * i + 12, 0);
        m.w16(q + 2 * i + 4, 0);
    }
}

u32 Game::radio_stream_update() {
    cover(0x80056634u);
    Frame f(*this, 24);
    const u32 s = m.r32(kRadioStream);
    if (s == 2) {
        subtitle_open(m.r8(kSubtitleLine), m.r8(scene_of(m) + 924) == 1 ? 82u : 268u, 30);
        return 1;
    }
    if (s != 1) return 0;
    m.w32(kRadioStream, 0);
    radio_face(255, 0, 0);
    if (m.r32(world_of(m) + 4) == 4) music_play(m.r8(ms_of(m) + 44));
    subtitle_close(1);
    return 0;
}

u32 Game::radio_script_update() {
    cover(0x80056778u);
    Frame f(*this, 32);
    u32 st = m.r32(kRadioScript);
    u32 p = m.r32(st);
    if (!p) {
        const u32 v = m.r8(st + 7);
        if (v != 255 && !voice_playing(v)) {
            radio_face(255, 0, 0);
            m.w8(m.r32(kRadioScript) + 7, 0xFF);
            subtitle_close(1);
        }
        return 0;
    }
    if (m.r16s(p) == m.r16s(st + 4)) {
        for (;;) {
            const u32 op = m.r8(p + 2), arg = m.r8(p + 3);
            if (op == 0) {
                m.w8(m.r32(kRadioScript) + 6, arg);
                const u32 v = sound_play(arg);
                m.w8(m.r32(kRadioScript) + 7, v);
            } else if (op - 1u < 2u) {
                m.w8(op + m.r32(kRadioScript) + 7, arg);
            } else if (op == 4) {
                m.w8(kSubtitleLine, arg);
                subtitle_open(m.r8(kSubtitleLine), m.r8(scene_of(m) + 924) == 1 ? 82u : 268u, 30);
                m.w32(m.r32(kRadioScript) + 12, m.r8(p + 3));
            }
            st = m.r32(kRadioScript);
            p += 4;
            m.w32(st, p);
            const s32 t = m.r16s(p);
            if (t == -1) {
                m.w32(st, 0);
                break;
            }
            if (t != m.r16s(st + 4)) break;
        }
    }
    st = m.r32(kRadioScript);
    m.w8(0x800BCE94u, m.r8(st + 8));
    m.w8(0x800BCE96u, 0);
    m.w8(0x800BCE95u, m.r8(m.r32(kRadioScript) + 9));
    if (m.r32(m.r32(kRadioScript))) {
        const u32 s2 = m.r32(kRadioScript);
        const s32 t = static_cast<s16>(m.r16(s2 + 4) + 1u);
        m.w16(s2 + 4, static_cast<u32>(t));
        if (m.r16s(p) < t) m.w32(st, 0);
    }
    return 1;
}

void Game::radio_update() {
    cover(0x8005628Cu);
    Frame f(*this, 32);
    if (!paused(m)) {
        for (u32 i = 0; i < 4; ++i) {
            const u32 kind = m.r8(m.r32(kRadioQueue) + i + 20);
            if (kind == 255) continue;
            const u32 q = m.r32(kRadioQueue) + 2 * i;
            if (m.r16s(q + 4) != m.r16s(q + 12)) {
                if (!paused(m)) m.w16(m.r32(kRadioQueue) + 2 * i + 4, m.r16(m.r32(kRadioQueue) + 2 * i + 4) + 1u);
                continue;
            }
            if (m.r32(m.r32(kRadioQueue)) != 255) continue;
            bool go = m.r8(scene_of(m) + 924) != 1;
            if (!go) {
                radio_hold();
                go = radio_hold_state() == 2;
            }
            if (!go) {
                m.w32(kRadioState, 3);
                continue;
            }
            const u32 c = m.r32(kRadioCalls) + 4 * kind;
            if (m.r8(c)) radio_play_script(m.r32(kRadioScripts) + 4 * static_cast<u32>(m.r16s(c + 2)));
            else radio_play_stream(m.r8(c + 1));
            const u32 rq = m.r32(kRadioQueue);
            scene_or(m, 0x10000000u);
            m.w32(rq, kind);
            m.w8(rq + i + 20, 0xFF);
        }
    }
    const u32 stream = radio_stream_update();
    const u32 script = radio_script_update();
    if (!stream && !script && !radio_face_busy() && m.r32(kRadioState) != 3) {
        if (m.r32(kRadioState) && (m.r32(world_of(m) + 4) & 4u))
            m.w32(scene_of(m), m.r32(scene_of(m)) & ~0x10000000u);
        m.w32(kRadioState, 0);
        m.w32(m.r32(kRadioQueue), 255);
    }
    if (m.r32(m.r32(kRadioQueue)) == 255 && m.r8(kSubtitleLine)) {
        m.w8(kSubtitleLine, 0);
        subtitle_close(1);
    }
    if (m.r8(scene_of(m) + 924) == 1) {
        if (m.r32(m.r32(kRadioQueue)) == 255) radio_unhold();
        else radio_hold();
    }
}
}
