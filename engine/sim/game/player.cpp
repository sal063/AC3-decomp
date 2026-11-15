#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCtlStick = 0x800BD088u;
constexpr u32 kCtl = 0x800BD08Cu;
constexpr u32 kCtlSurf = 0x800BD090u;
constexpr u32 kPlayerWeapons = 0x800BD098u;
constexpr u32 kApproach = 0x800BD0A0u;
constexpr u32 kCarrierKind = 0x800BCE14u;
constexpr u32 kCarrier = 0x800BCE18u;
constexpr u32 kTanker = 0x800BCE1Cu;
constexpr u32 kTankerTarget = 0x800BCE20u;
constexpr u32 kHitNormal = 0x800BD0C0u;
constexpr u32 kHitFlags = 0x800BD0C4u;
constexpr u32 kSideNormals = 0x80024630u;
constexpr u32 kEngineSound = 0x800C7BE8u;
s32 q12(s32 x) { return x / 4096; }
s32 absv(s32 v) { return v < 0 ? -v : v; }
s32 clampv(s32 v, s32 lo, s32 hi) { return v < lo ? lo : hi < v ? hi : v; }
u32 P(Memory& m) { return m.r32(g::kPlayer); }
u32 CS(Memory& m) { return m.r32(kPlayerWeapons); }
s32 u8of(s32 v) { return v & 0xFF; }
s32 mix(s32 r) { return (r >> 8) | (r << 7); }

s32 wrap_near(s32 ref, s32 a) {
    if (ref - a < 0x2000) a -= 0x4000;
    if (ref - a >= 0x2001) a += 0x4000;
    return a;
}
}

void Game::hud_alert(u32 priority, u32 kind, u32 level) {
    const u32 p = hud_alert_take(priority);
    if (!p) return;
    m.w8(p, kind);
    m.w8(p + 2, level);
}

void Game::hud_alert_speed(s32 a) {
    cover(0x800293F0u);
    Frame f(*this, 24);
    if (a < 32) return;
    const u32 p = hud_alert_take(5);
    if (!p) return;
    m.w8(p, 1);
    s32 v = a >> 5;
    if (v >= 109) v = 105;
    m.w8(p + 2, static_cast<u32>(v));
}

void Game::hud_alert_launch(u32 kind) {
    cover(0x80029448u);
    Frame f(*this, 24);
    if ((m.r32(m.r32(g::kScene) + 288) & 3u) != 3) return;
    if (kind == 7 || kind == 11) return;
    const u32 p = hud_alert_take(9);
    if (!p) return;
    m.w8(p + 2, 255);
    m.w8(p, (kind == 6 || kind == 10) ? 2u : 1u);
}

void Game::hud_alert_gun(u32 kind) {
    cover(0x800294C4u);
    Frame f(*this, 40);
    if ((m.r32(m.r32(g::kScene) + 288) & 3u) != 3) return;
    const u32 p = hud_alert_take(3);
    if (!p) return;
    for (u32 k = 0; k < 12; k += 4) m.w32(f.at(0x10 + k), m.r32(0x80010380u + k));
    const u32 i = kind - 1;
    if (i < 6) {
        const u32 e = f.at(0x10) + 2 * i;
        m.w8(p + 2, m.r8(e));
        m.w8(p, m.r8(e + 1));
        if (i != 2) m.w8(p + 1, 1);
    } else {
        m.w8(p, 0);
    }
}

void Game::view_end(u32 k) {
    cover(0x80079564u);
    Frame f(*this, 40);
    for (u32 n = 0; n < 3; ++n) m.w32(f.at(0x10 + 4 * n), m.r32(0x8002474Cu + 4 * n));
    view_set(m.r32(m.r32(g::kWorld) + 40), m.r32(f.at(0x10 + 4 * k)), m.r32(m.r32(g::kSlots)));
    view_flag(0);
}

void Game::view_landing(u32 a) {
    cover(0x800795C8u);
    Frame f(*this, 24);
    const u32 mode = a ? 256u : 128u;
    view_flag(a ? 0u : 1u);
    view_set(m.r32(m.r32(g::kWorld) + 40), mode, m.r32(m.r32(g::kSlots)));
}

void Game::engine_voices_stop() {
    cover(0x800301A8u);
    Frame f(*this, 24);
    voice_release(m.r8(0x800BCEA1u));
    voice_release(m.r8(0x800BCEA2u));
    voice_release(m.r8(0x800BCEA3u));
    m.w16(0x800BCEA2u, 0xFFFF);
    m.w8(0x800BCEA1u, 255);
}

void Game::player_out() {
    cover(0x800763CCu);
    Frame f(*this, 24);
    const u32 p = P(m);
    const u32 fl = m.r32(p);
    m.w32(p + 4, 0x100000);
    m.w32(p + 8, 0);
    m.w32(p, fl | 0x1000u);
    engine_voices_stop();
}

void Game::player_mode_flying() {
    cover(0x80076274u);
    const u32 p = P(m);
    m.w32(p, 1);
    m.w32(p + 4, 1);
    m.w32(p + 8, 0);
}

void Game::player_mode_end(u32 k) {
    cover(0x80076360u);
    const u32 p = P(m);
    if (k) {
        if (k == 1) {
            const u32 fl = m.r32(p);
            m.w32(p + 4, 0x8000000);
            m.w32(p, fl | 0x40u);
        } else {
            m.w32(p + 4, 0x10000000);
        }
    } else {
        m.w32(p, 16);
        m.w32(p + 4, 0x4000000);
        m.w32(p + 8, 0);
        m.w16(p + 78, 0);
        m.w16(p + 80, 0);
    }
}

void Game::player_mode_from_launch(u32 a, u32 b) {
    cover(0x80076DECu);
    Frame f(*this, 48);
    for (u32 n = 0; n < 6; ++n) m.w32(f.at(0x10 + 4 * n), m.r32(0x80024718u + 4 * n));
    const u32 k = 2 * b + a;
    const u32 mode = m.r32(f.at(0x10 + 4 * k));
    const u32 p = P(m);
    if (mode == 0x4000 || mode == 0x8000) m.w16(p + 82, 0xFFFF);
    m.w32(p, 2);
    if (mode == 0x10000 || mode == 4) {
        if (mode == 0x10000) {
            m.w32(p, 66);
        } else {
            player_mode_flying();
            m.w32(P(m), m.r32(P(m)) | 0x280u);
        }
        const s32 v = m.r16s(P(m) + 82);
        if (v >= 0) voice_release(static_cast<u32>(v));
    }
    m.w32(P(m) + 4, mode);
    m.w32(P(m) + 8, 0);
}

void Game::player_mode_from_refuel(u32 a, u32 b) {
    cover(0x80076C54u);
    Frame f(*this, 32);
    for (u32 n = 0; n < 8; ++n) m.w32(f.at(4 * n), m.r32(0x800246F8u + 4 * n));
    const u32 k = 2 * b + a;
    const u32 mode = m.r32(f.at(4 * k));
    const u32 p = P(m);
    if (mode == 256) m.w32(p, 8);
    if (mode == 512 || mode == 1024 || mode == 4096) m.w32(p, m.r32(p) | 0x40u);
    else if (mode == 2048) m.w32(p, m.r32(p) & ~0x40u);
    if (mode == 2048 || mode == 4096) m.w32(p, m.r32(p) | 0x200u);
    const u32 a2 = m.r32(kApproach);
    m.w32(a2, 0);
    m.w32(a2 + 4, 0);
    m.w32(a2 + 8, 0);
    m.w16(a2 + 16, 0);
    m.w16(a2 + 18, 0);
    m.w16(a2 + 20, 0);
    m.w32(p + 8, 0);
    m.w32(p + 4, mode);
}

void Game::player_mode_from_carrier(u32 a, u32 b) {
    cover(0x80076B18u);
    Frame f(*this, 24);
    for (u32 n = 0; n < 6; ++n) m.w32(f.at(4 * n), m.r32(0x800246E0u + 4 * n));
    const u32 k = 2 * b + a;
    const u32 mode = m.r32(f.at(4 * k));
    const u32 p = P(m);
    if (mode == 8) m.w32(p, 4);
    if (mode == 16) m.w32(p, m.r32(p) | 0x40u);
    else if (mode == 32 || mode == 64) m.w32(p, (m.r32(p) & 0xFFFFFDBFu) | 0x200u);
    const u32 c = m.r32(kCarrier), a2 = m.r32(kApproach);
    m.w32(a2, m.r32(c));
    m.w32(a2 + 4, m.r32(c + 4));
    m.w32(a2 + 8, m.r32(c + 8));
    m.w16(a2 + 16, m.r16(c + 16));
    m.w16(a2 + 18, m.r16(c + 18));
    m.w16(a2 + 20, m.r16(c + 20));
    m.w32(p + 8, 0);
    m.w32(p + 4, mode);
}

bool Game::on_deck(u32 pos) {
    cover(0x80076AC0u);
    if (m.r32s(pos + 4) < 0) return false;
    if (absv(m.r16s(pos + 16) - 1820) >= 228) return false;
    return absv(m.r16s(pos + 20)) < 456;
}

bool Game::off_deck(u32 pos, u32 c) {
    cover(0x80076A38u);
    bool out = m.r32s(pos) < m.r32s(c + 24);
    if (m.r32s(c + 24) + m.r32s(c + 32) < m.r32s(pos)) out = true;
    if (m.r32s(pos + 8) < m.r32s(c + 28)) out = true;
    if (m.r32s(c + 28) + m.r32s(c + 36) < m.r32s(pos + 8)) out = true;
    if (m.r32s(pos + 4) - m.r32s(c + 4) < -409600) return true;
    return out;
}

u32 Game::landing_check(u32 pos, u32 c) {
    cover(0x80076428u);
    const s32 pitch = m.r16s(pos + 16), roll = m.r16s(pos + 20);
    if (pitch >= 683 || pitch < -364) return 0;
    if (absv(roll) >= 1366) return 0;
    const s32 x = m.r32s(pos), z = m.r32s(pos + 8);
    u32 r = 1;
    if (x < m.r32s(c + 40)) return 0;
    if (m.r32s(c + 40) + m.r32s(c + 48) < x) return 0;
    if (z < m.r32s(c + 44) || m.r32s(c + 44) + m.r32s(c + 52) < z || m.r32s(c + 4) < m.r32s(pos + 4)) return 0;
    if (x < m.r32s(c + 24)) r = 2;
    if (m.r32s(c + 24) + m.r32s(c + 32) < x) r = 2;
    if (z < m.r32s(c + 28)) r = 2;
    if (m.r32s(c + 28) + m.r32s(c + 36) < z) r = 2;
    const s32 ref = m.r16s(c + 18);
    s32 h = m.r16s(pos + 18);
    if (ref - h < 0x2000) h -= 0x4000;
    if (!(ref - h < 8193)) h += 0x4000;
    const s32 d = ref - h;
    if (d < 0) return h - ref < 683 ? r : 2u;
    return d >= 683 ? 2u : r;
}

void Game::relative_state(u32 obj, u32 a, u32 me, u32 b, u32 out) {
    cover(0x800765B0u);
    Frame f(*this, 0xA0);
    const s32 h0 = m.r16s(obj + 0x10), h1 = m.r16s(obj + 0x12), h2 = m.r16s(obj + 0x14);
    const s32 p0 = wrap_near(h0, m.r16s(me + 0x10));
    const s32 p1 = wrap_near(h1, m.r16s(me + 0x12));
    const s32 p2 = wrap_near(h2, m.r16s(me + 0x14));
    m.w16(out + 0x34, static_cast<u32>(p0) - m.r16(obj + 0x10));
    m.w16(out + 0x36, static_cast<u32>(p1) - m.r16(obj + 0x12));
    m.w16(out + 0x38, static_cast<u32>(p2) - m.r16(obj + 0x14));
    const u32 m1 = f.at(0x10), m2 = f.at(0x30), va = f.at(0x50), vb = f.at(0x58), ra = f.at(0x60), rb = f.at(0x70);
    rot_matrix_yxz(obj + 0x10, m1);
    rot_matrix_yxz(me + 0x10, m2);
    for (u32 k = 0; k < 3; ++k) m.w16(va + 2 * k, static_cast<u32>(m.r32s(a + 4 * k) >> 6));
    for (u32 k = 0; k < 3; ++k) m.w16(vb + 2 * k, static_cast<u32>(m.r32s(b + 4 * k) >> 6));
    auto turn = [&](u32 mat, u32 v, u32 o) {
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(mat + 4 * r));
        gte.mtc2(0, m.r32(v));
        gte.mtc2(1, m.r32(v + 4));
        gte.op(0x486012);
        m.w32(o, gte.mfc2(25));
        m.w32(o + 4, gte.mfc2(26));
        m.w32(o + 8, gte.mfc2(27));
    };
    turn(m1, va, ra);
    turn(m2, vb, rb);
    s32 dx = (m.r32s(me) - m.r32s(obj)) >> 6;
    s32 dy = (m.r32s(me + 4) - m.r32s(obj + 4)) >> 6;
    s32 dz = (m.r32s(me + 8) - m.r32s(obj + 8)) >> 6;
    dx += m.r32s(rb) - m.r32s(ra);
    dy += m.r32s(rb + 4) - m.r32s(ra + 4);
    m.w32(out + 0x28, static_cast<u32>(dy << 6));
    dy >>= 6;
    dz += m.r32s(rb + 8) - m.r32s(ra + 8);
    m.w32(out + 0x24, static_cast<u32>(dx << 6));
    dx >>= 6;
    m.w32(out + 0x2C, static_cast<u32>(dz << 6));
    dz >>= 6;
    for (u32 k = 0; k < 2; ++k) {
        m.w32(out, static_cast<u32>((m.r32s(ra) << 6) + m.r32s(obj)));
        m.w32(out + 4, static_cast<u32>((m.r32s(ra + 4) << 6) + m.r32s(obj + 4)));
        m.w32(out + 8, static_cast<u32>((m.r32s(ra + 8) << 6) + m.r32s(obj + 8)));
    }
    const s32 man = absv(dx) + absv(dy) + absv(dz);
    m.w32(out + 0x20, static_cast<u32>(0xB000 < man ? 0x10000 : length3(dx, dy, dz)));
}

bool Game::refuel_window(u32 e, u32 tanker, u32 target, u32 out) {
    cover(0x80076924u);
    Frame f(*this, 32);
    m.w32(f.at(0x10), out);
    relative_state(slot(m.r32(tanker)) + 8, tanker + 4, e + 8, target, out);
    bool ok = absv(m.r16s(out + 52)) < 365;
    if (absv(m.r16s(out + 54)) >= 365) ok = false;
    if (absv(m.r16s(out + 56)) >= 365) ok = false;
    if (absv(m.r32s(out + 36)) > 204800) ok = false;
    if (absv(m.r32s(out + 40)) > 327680) ok = false;
    if (absv(m.r32s(out + 44)) > 614400) return false;
    return ok;
}

u32 Game::voice_pitch(u32 v, s32 pitch) {
    cover(0x8002DE24u);
    if (v >= 24) return 255;
    const u32 p = g::kVoices + 20 * v;
    const u32 b = m.r8(p + 1);
    m.w16(p + 12, static_cast<u32>(pitch));
    m.w8(p + 1, b | 0x10u);
    return v;
}

s32 Game::sound_def_8(u32 id) {
    cover(0x8002DE68u);
    return m.r16s(0x800C5A10u + 20 * id + 8);
}

s32 Game::sound_def_get(u32 id, u32 a, u32 b) {
    cover(0x8002DE8Cu);
    const u32 d = 0x800C5A10u + 20 * id;
    m.w16(a, m.r16(d + 4));
    m.w16(b, m.r16(d + 6));
    return m.r16s(d + 8);
}

u32 Game::voice_volume(u32 v, s32 left, s32 right) {
    cover(0x8002DEC8u);
    if (v >= 24) return 255;
    s16 l = static_cast<s16>(left), r = static_cast<s16>(right);
    if (l >= 0x4000) l = 0x3FFF;
    if (r >= 0x4000) r = 0x3FFF;
    const u32 p = g::kVoices + 20 * v;
    const u32 b = m.r8(p + 1);
    m.w16(p + 4, static_cast<u32>(l));
    m.w16(p + 6, static_cast<u32>(r));
    m.w8(p + 1, b | 0x10u);
    return v;
}

void Game::engine_sound(u32 in) {
    cover(0x800301F0u);
    Frame f(*this, 32);
    constexpr u32 kTable = 0x800C5A10u;
    if (!m.r8(0x800BCEA0u)) {
        m.w8(0x800BCEA1u, sound_play(m.r8(0x800BCEA4u)));
        m.w8(0x800BCEA2u, sound_play(m.r8(0x800BCEA5u)));
        m.w8(0x800BCEA3u, sound_play(m.r8(0x800BCEA6u)));
        m.w8(0x800BCEA0u, m.r8(0x800BCEA0u) + 1u);
    }
    if (m.r8(0x800BCEA3u) != 255) {
        const u32 d = kTable + 20 * m.r8(0x800BCEA6u);
        const s32 k = m.r16s(in + 8);
        voice_pitch(m.r8(0x800BCEA3u), pitch_add(m.r16s(d + 8), static_cast<s16>(k >> 4)));
        voice_volume(m.r8(0x800BCEA3u), m.r16(d + 4) + (k >> 2), m.r16(d + 6) + (k >> 2));
    }
    if (m.r8(0x800BCEA2u) != 255) {
        const u32 d = kTable + 20 * m.r8(0x800BCEA5u);
        voice_pitch(m.r8(0x800BCEA2u), pitch_add(m.r16s(d + 8), static_cast<s16>((m.r32s(in) - 147456) >> 7)));
        voice_volume(m.r8(0x800BCEA2u), m.r16(d + 4) - 4 * m.r16(in + 10), m.r16(d + 6) + 4 * m.r16(in + 10));
    }
    if (m.r8(0x800BCEA1u) != 255) {
        const u32 d = kTable + 20 * m.r8(0x800BCEA4u);
        const s32 g1 = absv(m.r16s(in + 12));
        voice_pitch(m.r8(0x800BCEA1u), pitch_add(m.r16s(d + 8), static_cast<s16>(4 * g1)));
        const s32 g2 = absv(m.r16s(in + 12));
        const s32 alt = clampv(m.r32s(in + 4) - 0x200000, 0, 0x400000);
        voice_volume(m.r8(0x800BCEA1u), m.r16(d + 4) + 8 * g2 + 4096 - (alt >> 10), m.r16(d + 6) + 8 * g2 + 4096 - (alt >> 10));
    }
}

void Game::engine_sound_update() {
    cover(0x80072114u);
    Frame f(*this, 24);
    m.w32(kEngineSound + 4, m.r32(CS(m) + 44));
    m.w32(kEngineSound, m.r32(P(m) + 72));
    m.w16(kEngineSound + 8, m.r16(m.r32(kCtl) + 14));
    m.w16(kEngineSound + 12, m.r16(P(m) + 48));
    m.w16(kEngineSound + 10, m.r16(P(m) + 52));
    engine_sound(kEngineSound);
}

void Game::smoke_puff(u32 pos, u32 vel) {
    cover(0x80082A3Cu);
    Frame f(*this, 32);
    const u32 q = fx_debris();
    if (!q) return;
    const s32 r = u8of(rand());
    m.w32(q, static_cast<u32>(((r & 3) + 9) << 16));
    m.w16(q + 24, 7172);
    m.w16(q + 26, static_cast<u32>((r & 0x3F) + 50));
    const s32 s = rand();
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    m.w32(q + 4, static_cast<u32>(m.r32s(pos) + vx + ((u8of(s) - 128) << 8)));
    m.w32(q + 8, static_cast<u32>(m.r32s(pos + 4) + vy + ((u8of(s >> 3) - 128) << 8)));
    m.w16(q + 20, static_cast<u32>(vz >> 10));
    m.w16(q + 22, 138);
    m.w16(q + 16, static_cast<u32>(vx >> 10));
    m.w16(q + 18, static_cast<u32>((vy >> 10) - 144));
    m.w32(q + 12, static_cast<u32>(((u8of(s >> 6) - 128) << 8) + m.r32s(pos + 8) + vz));
}

void Game::smoke_trail(u32 pos, u32 vel, u32 to, u32 b) {
    cover(0x80082BACu);
    Frame f(*this, 40);
    const u32 p = f.at(0x10);
    for (u32 k = 0; k < 12; k += 4) {
        const s32 a = m.r32s(pos + k);
        m.w32(p + k, static_cast<u32>(a + ((m.r32s(to + k) - a) >> 2)));
    }
    smoke_trail_puff(p, vel, b);
}

void Game::smoke_trail_puff(u32 pos, u32 vel, u32 b) {
    cover(0x80082B54u);
    Frame f(*this, 32);
    m.w8(record_motion(4, 7, pos, vel) + 2, b);
    smoke_puff(pos, vel);
}

void Game::player_explosion(u32 pos, u32 vel) {
    cover(0x80081F94u);
    Frame f(*this, 32);
    record_motion(4, 3, pos, vel);
    player_explosion_fx(pos, vel);
}

void Game::player_explosion_fx(u32 pos, u32 vel) {
    cover(0x80081D4Cu);
    Frame g2(*this, 56);
    const s32 x = m.r32s(pos), y = m.r32s(pos + 4), z = m.r32s(pos + 8);
    if (m.r32(m.r32(g::kWorld)) & 0x100u) {
        fx_80089504(x, y, z);
        return;
    }
    if (const u32 p = fx_flashes()) {
        m.w32(p, 0xFFFFF);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w32(p + 24, 26214656);
    }
    if (const u32 q = fx_light()) {
        m.w32(q, 786432);
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w32(q + 20, 45875200);
    }
    const s32 dy = (0 - m.r32s(vel + 4)) >> 12, dz = (0 - m.r32s(vel + 8)) >> 12, dx = (0 - m.r32s(vel)) >> 12;
    for (s32 k = 0; k < 20; ++k) {
        const u32 d = fx_debris();
        if (!d) break;
        m.w16(d, 0);
        m.w16(d + 2, static_cast<u32>(k + 20));
        const s32 r = rand();
        const s32 r2 = mix(r);
        m.w16(d + 16, static_cast<u32>(dx + 2 * (u8of(r2) - 128)));
        m.w16(d + 18, static_cast<u32>(dy + ((r2 >> 2) & 0x1FE) - 288));
        m.w16(d + 20, static_cast<u32>(dz + 2 * (u8of(r2 >> 6) - 128)));
        m.w32(d + 4, static_cast<u32>(x + ((u8of(r) - 128) << 8)));
        m.w32(d + 8, static_cast<u32>(y + ((u8of(r >> 3) - 128) << 8)));
        m.w32(d + 12, static_cast<u32>(z + ((u8of(r >> 6) - 128) << 8)));
        if (k >= 6) {
            m.w16(d + 22, 392);
            m.w16(d + 24, 0);
            m.w16(d + 26, static_cast<u32>((k >> 1) + 18));
        } else {
            m.w16(d + 22, 138);
            m.w16(d + 24, 6148);
            m.w16(d + 26, static_cast<u32>((dx & 0x3F) + 600));
        }
    }
}

void Game::wreck_flames(u32 pos, u32 vel, u32 tick) {
    cover(0x80081FDCu);
    Frame f(*this, 56);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) + vx + ((rand() & 0x3F) << 11);
    const s32 y = m.r32s(pos + 4) + vy + ((rand() & 0x3F) << 11);
    const s32 z = m.r32s(pos + 8) + vz + ((rand() & 0x3F) << 11);
    const s32 lo = rand();
    const s32 r = lo | (rand() << 15);
    if (!(tick & 7u)) {
        if (const u32 p = fx_flashes()) {
            m.w32(p, 917504);
            m.w16(p + 24, 256);
            m.w32(p + 4, static_cast<u32>(x));
            m.w32(p + 8, static_cast<u32>(y));
            m.w32(p + 12, static_cast<u32>(z));
            m.w16(p + 22, 0);
            m.w16(p + 26, static_cast<u32>((r & 0x3F) + 58));
        }
    }
    if (const u32 q = fx_flashes()) {
        m.w32(q, 0x100000);
        m.w16(q + 24, 528);
        m.w32(q + 4, static_cast<u32>(x));
        m.w32(q + 8, static_cast<u32>(y));
        m.w32(q + 12, static_cast<u32>(z));
        m.w16(q + 22, 0);
        m.w16(q + 26, static_cast<u32>((r & 0x3F) + 68));
    }
    if (!(r & 3)) return;
    const u32 d = fx_debris();
    if (!d) return;
    m.w32(d, 1572864);
    m.w16(d + 16, static_cast<u32>((vx >> 9) - 64 + (r & 0x7F)));
    m.w16(d + 18, static_cast<u32>((vy >> 9) + ((r >> 3) & 0x7F) - 96));
    m.w32(d + 4, static_cast<u32>(x));
    m.w32(d + 8, static_cast<u32>(y));
    m.w32(d + 12, static_cast<u32>(z));
    m.w16(d + 20, static_cast<u32>((vz >> 9) - 64 + ((r >> 6) & 0x7F)));
    if (r & 1) {
        m.w16(d + 22, 392);
        m.w32(d + 24, 917504);
    } else {
        m.w16(d + 22, 139);
        m.w16(d + 24, 5892);
        m.w16(d + 26, static_cast<u32>((r & 0x1F) + 60));
    }
}

void Game::wreck_burn(u32 pos, u32 vel, u32 tick) {
    cover(0x800821ECu);
    Frame f(*this, 32);
    if (m.r32(m.r32(g::kWorld)) & 0x100u) {
        puff_spawn_recorded(pos, vel, 9);
        smoke(pos, vel, 0, 1);
        return;
    }
    if (tick & 1u) return;
    const u32 r = record_motion(4, 4, pos, vel);
    m.w32(r, (m.r32(r) & 0xFFFF01FFu) | ((tick & 0x7Fu) << 9));
    wreck_flames(pos, vel, tick);
}

void Game::wreck_scatter(u32 pos, u32 kind) {
    cover(0x8008276Cu);
    Frame f(*this, 32);
    if (m.r32(m.r32(g::kWorld)) & 0x100u) return;
    const u32 r0 = record_effect(2, pos);
    m.w32(r0 + 4, (m.r32(r0 + 4) & 0xFFFFF0FFu) | ((kind & 0xFu) << 8));
    wreck_scatter_fx(pos, kind);
}

void Game::wreck_scatter_fx(u32 pos, u32 kind) {
    cover(0x80082594u);
    Frame g2(*this, 40);
    const s32 x = m.r32s(pos) + (((rand() & 0x3F) - 32) << 12);
    const s32 y = m.r32s(pos + 4) + (((rand() & 0x3F) - 32) << 12);
    const s32 z = m.r32s(pos + 8) + (((rand() & 0x3F) - 32) << 12);
    const s32 r = rand();
    if (kind & 1u) {
        if (const u32 p = fx_flashes()) {
            m.w32(p, 8519680);
            m.w16(p + 16, static_cast<u32>(r & 0x3F));
            m.w16(p + 18, static_cast<u32>(-112));
            m.w16(p + 20, static_cast<u32>((r >> 6) & 0x3F));
            m.w16(p + 22, 128);
            m.w16(p + 24, 528);
            m.w32(p + 4, static_cast<u32>(x));
            m.w32(p + 8, static_cast<u32>(y));
            m.w32(p + 12, static_cast<u32>(z));
            m.w16(p + 26, static_cast<u32>(((r >> 3) & 0x3F) + 78));
        }
    } else if (const u32 p = fx_flashes()) {
        m.w32(p, 720892);
        m.w16(p + 24, 256);
        m.w32(p + 4, static_cast<u32>(x));
        m.w32(p + 8, static_cast<u32>(y));
        m.w32(p + 12, static_cast<u32>(z));
        m.w16(p + 22, 0);
        m.w16(p + 26, static_cast<u32>((r & 0x3F) + 48));
    }
    if (!(kind & 1u) || !(r & 1)) return;
    const u32 d = fx_debris();
    if (!d) return;
    m.w16(d, 10);
    m.w16(d + 2, static_cast<u32>((r & 0x1F) + 40));
    m.w16(d + 16, static_cast<u32>(r & 0x3F));
    m.w16(d + 18, static_cast<u32>(-112));
    m.w16(d + 20, static_cast<u32>((r >> 6) & 0x3F));
    m.w16(d + 22, 128);
    m.w16(d + 24, 5636);
    m.w32(d + 4, static_cast<u32>(x));
    m.w32(d + 8, static_cast<u32>(y));
    m.w32(d + 12, static_cast<u32>(z));
    m.w16(d + 26, static_cast<u32>(((r >> 3) & 0x3F) + 100));
}

s32 Game::ground_height(u32 pos, u32 nrm, u32 flags) {
    cover(0x800711FCu);
    Frame f(*this, 80);
    auto cell = [](s32 v) {
        s32 c = (v >> 23) + 128;
        if (c < 0) c &= 7;
        if (c >= 256) c = (c & 7) + 248;
        return c;
    };
    const s32 cx = cell(m.r32s(pos)), cz = cell(m.r32s(pos + 8));
    auto flat = [&] {
        if (nrm) {
            m.w16(nrm, 0);
            m.w16(nrm + 2, 0xF000);
            m.w16(nrm + 4, 0);
        }
    };
    if (!(m.r16(2u * static_cast<u32>(cx + (cz << 8)) + m.r32(m.r32(g::kWorld) + 584)) & 0x1FFu)) {
        flat();
        if (flags) m.w32(flags, 1);
        return 0;
    }
    const u32 a = f.at(0x10), b = f.at(0x20), out = f.at(0x30);
    const s32 top = m.r32s(pos + 4) > -12288000 ? m.r32s(pos + 4) : -12288000;
    m.w32(a, m.r32(pos));
    m.w32(a + 4, static_cast<u32>(top));
    m.w32(a + 8, m.r32(pos + 8));
    m.w32(b + 4, 0);
    m.w32(b, m.r32(pos));
    m.w32(b + 8, m.r32(pos + 8));
    if (ray_cell(a, b, out, g::kScratch) == 0x7FFFFFFF) {
        flat();
        if (flags) m.w32(flags, 0);
        return 0;
    }
    if (nrm) {
        const u32 n = m.r32(kHitNormal);
        m.w16(nrm, m.r16(n));
        m.w16(nrm + 2, m.r16(n + 2));
        m.w16(nrm + 4, m.r16(n + 4));
    }
    if (flags) m.w32(flags, m.r32(kHitFlags));
    return m.r32s(out + 4);
}

void Game::apply_velocity(u32 e) {
    cover(0x80070A34u);
    if (m.r32s(e + 84) >= 0) return;
    const u32 vy = m.r32(e + 60), vz = m.r32(e + 64);
    m.w32(e + 8, m.r32(e + 8) + m.r32(e + 56));
    m.w32(e + 12, m.r32(e + 12) + vy);
    m.w32(e + 16, m.r32(e + 16) + vz);
}

void Game::collide_world(u32 e, u32 st) {
    cover(0x80070CF4u);
    Frame f(*this, 72);
    const u32 w = m.r32(g::kWorld);
    s32 x = m.r32s(e + 56) + m.r32s(e + 8), y = m.r32s(e + 60) + m.r32s(e + 12), z = m.r32s(e + 64) + m.r32s(e + 16);
    u32 side = 0, surface = 0;
    if (x < m.r32s(w + 672) + 132) { surface = 4; side = 1; x = m.r32s(w + 672) + 132; }
    if (m.r32s(w + 676) - 132 < x) { surface = 4; side = 2; x = m.r32s(w + 676) - 132; }
    if (y < m.r32s(w + 680) + 132) { surface = 3; side = 3; y = m.r32s(w + 680) + 132; }
    if (m.r32s(w + 684) - 132 < y) { surface = 1; side = 4; y = m.r32s(w + 684) - 132; }
    if (z < m.r32s(w + 688) + 132) { surface = 4; side = 5; z = m.r32s(w + 688) + 132; }
    if (m.r32s(w + 692) - 132 < z) { surface = 4; side = 6; z = m.r32s(w + 692) - 132; }
    if (side) {
        const u32 n = kSideNormals + 8 * side;
        m.w32(e + 8, static_cast<u32>(x));
        m.w32(e + 12, static_cast<u32>(y));
        m.w32(e + 16, static_cast<u32>(z));
        m.w16(st + 20, m.r16(n));
        m.w16(st + 22, m.r16(n + 2));
        const u32 nz = m.r16(n + 4);
        const u32 fl = m.r32(st + 16);
        m.w32(kHitFlags, surface);
        m.w32(st + 44, surface);
        m.w32(st + 16, fl | 2u);
        m.w16(st + 24, nz);
    }
    if (m.r32s(e + 12) < -12288000) return;
    const u32 grid = m.r32(w + 584);
    const u32 next = f.at(0x10), alt = f.at(0x20);
    const s32 nx = m.r32s(e + 8) + m.r32s(e + 56);
    const s32 ny = m.r32s(e + 12) + m.r32s(e + 60);
    const s32 nz = m.r32s(e + 16) + m.r32s(e + 64);
    m.w32(next, static_cast<u32>(nx));
    m.w32(next + 4, static_cast<u32>(ny));
    m.w32(next + 8, static_cast<u32>(nz));
    auto cell = [](s32 v) {
        s32 c = (v >> 23) + 128;
        if (c < 0) c &= 7;
        if (c >= 256) c = (c & 7) + 248;
        return c;
    };
    const s32 c0 = cell(m.r32s(e + 8)) + (cell(m.r32s(e + 16)) << 8);
    const s32 c1 = cell(nx) + (cell(nz) << 8);
    const u32 tile = m.r16(2u * static_cast<u32>(c0) + grid) & 0x1FFu;
    if (!tile) return;
    const u32 tp = m.r32(4 * tile + m.r32(w + 588));
    const s32 top = m.r16s(tp + 22);
    const s32 ny12 = ny >> 12;
    if (!((m.r32s(e + 12) >> 12) + 132 >= top || ny12 + 132 >= top || c0 != c1)) return;
    const s32 r = c0 == c1 ? ray_cell(e + 8, next, st + 28, g::kScratch) : terrain_raycast(e + 8, next, st + 28, g::kScratch);
    auto take_hit = [&](u32 bit) {
        const u32 n = m.r32(kHitNormal);
        const u32 hf = m.r32(kHitFlags);
        m.w16(st + 20, m.r16(n));
        m.w16(st + 22, m.r16(n + 2));
        const u32 nz2 = m.r16(n + 4);
        const u32 fl = m.r32(st + 16);
        m.w32(st + 44, hf);
        m.w32(st + 16, fl | bit);
        m.w16(st + 24, nz2);
    };
    if (r != 0x7FFFFFFF) take_hit(1);
    if (m.r16s(tp + 22) < ny12 + 99) {
        const s32 vy = m.r32s(e + 60);
        m.w32(next, m.r32(e + 8));
        m.w32(next + 4, static_cast<u32>(m.r32s(e + 12) + vy + 278528));
        m.w32(next + 8, m.r32(e + 16));
        m.w32(alt, m.r32(e + 8));
        m.w32(alt + 4, static_cast<u32>(m.r32s(e + 12) + vy - 278528));
        m.w32(alt + 8, m.r32(e + 16));
        const bool down = vy <= 0;
        if (ray_cell(down ? next : alt, down ? alt : next, st + 28, g::kScratch) != 0x7FFFFFFF) {
            take_hit(2);
            m.w32(e + 12, static_cast<u32>(m.r32s(st + 32) + (down ? 270336 : -270336)));
        }
    }
    const s32 floor = m.r16s(tp + 20);
    if (floor < ny12 - 60) {
        m.w32(e + 12, static_cast<u32>((floor - 60) << 12));
        const u32 fl = m.r32(st + 16);
        m.w16(st + 20, 0);
        m.w16(st + 22, 0xF000);
        m.w16(st + 24, 0);
        m.w32(st + 16, fl | 4u);
        m.w32(st + 28, m.r32(e + 8));
        m.w32(st + 32, m.r32(e + 12));
        m.w32(st + 44, 0);
        m.w32(st + 36, m.r32(e + 16));
    }
}

void Game::player_hit_objects(u32 e, u32 st) {
    cover(0x80070A80u);
    Frame f(*this, 64);
    const s32 vx = m.r32s(e + 56), vy = m.r32s(e + 60), vz = m.r32s(e + 64);
    const s32 reach = (absv(vx) + absv(vy) + absv(vz)) >> 12;
    const s32 ny = (m.r32s(e + 12) + vy) >> 12, nx = (m.r32s(e + 8) + vx) >> 12, nz = (m.r32s(e + 16) + vz) >> 12;
    for (u32 k = 0; k < 64; ++k) {
        const u32 t = slot(k);
        if ((m.r32(t) & 0x4400u) != 0x4400u) continue;
        const s32 r = m.r16s(t + 76);
        const s32 dx = (m.r32s(t + 8) >> 12) - nx, dy = (m.r32s(t + 12) >> 12) - ny, dz = (m.r32s(t + 16) >> 12) - nz;
        if (!(3 * r + reach >= absv(dx) + absv(dy) + absv(dz))) continue;
        if (!(r * r + reach * reach / 9 >= dx * dx + dy * dy + dz * dz)) continue;
        if ((m.r32(t) & 0x1020u) == 0x1020u && !(m.r8(t + 5) & 2u)) {
            m.w32(f.at(0x10), g::kScratch);
            if (model_raycast(e + 8, e + 56, t, st + 28, g::kScratch, static_cast<u32>(reach)) != 0x7FFFFFFF) {
                const u32 fl = m.r32(st + 16) | 0x20u;
                m.w32(st + 44, m.r32(kHitFlags));
                m.w32(st + 16, fl);
            }
        } else {
            m.w32(st + 16, m.r32(st + 16) | 0x20u);
            m.w32(st + 28, m.r32(e + 8));
            m.w32(st + 32, m.r32(e + 12));
            m.w32(st + 44, 0);
            m.w32(st + 36, m.r32(e + 16));
        }
    }
}

void Game::player_attitude_update(u32 e, u32 st) {
    cover(0x8007084Cu);
    Frame f(*this, 0x80);
    if ((m.r32(e) & 0xC000u) != 0xC000u) return;
    if (m.r32(e + 0x54) & 0x400000u) attitude_integrate(e + 0x18, e + 0x30);
    if (m.r32(e + 0x54) & 0x100000u) {
        attitude_integrate(st + 0x40, e + 0x30);
        if (m.r32(e + 0x54) & 0x100000u) attitude_integrate_to(e + 0x18, e + 0x30, st + 0x40);
    }
    if (m.r32(e + 0x54) & 0x20000u) rotate_by_angles(e + 0x20, e + 0x18, e + 0x38);
    if (m.r32(e + 0x54) & 0x10000u) rotate_by_angles(e + 0x20, st + 0x40, e + 0x38);
    if (m.r32(e + 0x54) & 0xF80000u) {
        const u32 rt = f.at(0x10), r2 = f.at(0x30), v = f.at(0x50), off = f.at(0x60);
        m.w32(off, m.r32(0x800BCE0Cu));
        m.w32(off + 4, m.r32(0x800BCE10u));
        rot_matrix_yxz_t(e + 0x18, rt);
        rot_matrix_yxz(st + 0x40, r2);
        mul_matrix(rt, r2);
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rt + 4 * r));
        gte.mtc2(0, m.r32(off));
        gte.mtc2(1, m.r32(off + 4));
        gte.op(0x486012);
        m.w32(v, gte.mfc2(25));
        m.w32(v + 4, gte.mfc2(26));
        m.w32(v + 8, gte.mfc2(27));
        m.w16(st + 0x38, static_cast<u32>(ratan2(-m.r32s(v + 4), m.r32s(v + 8))));
        m.w16(st + 0x3A, static_cast<u32>(ratan2(m.r32s(v), m.r32s(v + 8))));
        m.w16(st + 0x3C, 0);
    }
    m.w32(e + 0x54, m.r32(e + 0x54) & 0xFF000000u);
}

u32 Game::slot_take(u32 at, s32 n, u32 value) {
    cover(0x800718A0u);
    u32 free = 0;
    for (s32 k = 0; k < n; ++k, ++at) {
        if (!m.r8(at)) {
            m.w8(at, value);
            value = 0;
            ++free;
        }
    }
    return free;
}

void Game::reload_tick() {
    cover(0x800718E0u);
    for (u32 k = 0; k < 12; ++k) {
        const u32 at = P(m) + 88 + k;
        const u32 v = m.r8(at);
        if (v) m.w8(at, v - 1);
    }
    for (u32 k = 0; k < 12; ++k) {
        const u32 at = P(m) + 100 + k;
        const u32 v = m.r8(at);
        if (v) m.w8(at, v - 1);
    }
}

s32 Game::special_weapon_override() {
    cover(0x8003A62Cu);
    if (m.r16(m.r32(g::kScene) + 908) != 22) return -1;
    const u32 t = m.r8(0x800C6D72u);
    if (t == 255) return -1;
    const u32 sf = m.r32(g::kSlotFlags + 4 * t);
    if (!(sf & 0x100u)) return -1;
    if (sf & 0x400u) return -1;
    return 11;
}

void Game::player_post(u32 mask) {
    cover(0x80072178u);
    Frame f(*this, 40);
    const u32 e = slot(0);
    const u32 fp = m.r32(CS(m));
    if (mask & 1u) {
        u32 hits = m.r32(e + 84), ev = 0;
        if (hits & 0x3Fu) {
            m.w32(e + 84, hits & 0xFFFFFFC0u);
            ev = 1;
            m.w16(e + 78, m.r16(e + 78) - m.r16(fp + 136));
            hits = m.r32(e + 84);
        }
        if (hits & 0x7FC0u) {
            m.w32(e + 84, hits & 0xFFFF803Fu);
            ev |= 2u;
            m.w16(e + 78, m.r16(e + 78) - m.r16(fp + 138));
        }
        const s32 hp = m.r16s(P(m) + 76);
        if (hp >= 8) {
            if (hp < 1536) ev |= 8u;
        } else {
            ev |= 0x10u;
        }
        m.w32(CS(m) + 36, ev);
    }
    if (mask & 4u) player_attitude_update(e, P(m));
    if (m.r32(P(m) + 16) & 0x1000u) {
        m.w32(P(m) + 16, m.r32(P(m) + 16) & 0xFFFFEFFFu);
        m.w8(e + 5, m.r8(e + 5) | 8u);
    } else {
        m.w8(e + 5, m.r8(e + 5) & 0xF7u);
    }
    m.w32(P(m) + 16, m.r32(P(m) + 16) & ~0x20u);
    player_hit_objects(e, P(m));
    m.w32(P(m) + 16, m.r32(P(m) + 16) & 0xFFFFFFF0u);
    if (mask & 2u) collide_world(e, P(m));
    if (mask & 4u) apply_velocity(e);
    if (mask & 2u) {
        const s32 h = ground_height(e + 8, CS(m) + 48, CS(m) + 56);
        if ((h - m.r32s(e + 12)) >> 12 < 66) m.w32(P(m) + 16, m.r32(P(m) + 16) | 8u);
        m.w32(CS(m) + 40, static_cast<u32>(h));
        m.w32(CS(m) + 44, static_cast<u32>(h - m.r32s(e + 12)));
    }
    if (m.r32(P(m) + 16) & 0x20u) {
        const u32 p = P(m);
        m.w16(e + 78, 0xFFFF);
        m.w32(e + 56, 0);
        m.w32(e + 60, 0);
        m.w32(e + 64, 0);
        m.w32(e + 8, m.r32(p + 28));
        m.w32(e + 12, m.r32(p + 32));
        m.w32(e + 16, m.r32(p + 36));
    }
    if (mask & 2u) {
        const u32 fl = m.r32(P(m) + 16);
        if (fl & 0xFu) {
            if ((fl & 1u) && (mask & 0x40u)) m.w16(e + 78, 0xFFFF);
            if (mask & 0x10u) {
                const s32 roll = 3840 * m.r16s(e + 28);
                m.w16(e + 24, static_cast<u32>(3840 * m.r16s(e + 24) / 4096));
                const s32 vy = m.r32s(e + 60);
                m.w16(e + 28, static_cast<u32>(roll / 4096));
                if (vy > 0) m.w32(e + 60, 0);
                if (mask & 0x20u) {
                    const u32 sc = m.r32(g::kScene);
                    const s32 diff = m.r32s(sc + 916);
                    s32 d = diff <= 0 ? 3 : 5;
                    if (diff >= 2) d = 100;
                    m.w16(e + 78, m.r16(e + 78) - static_cast<u32>(d));
                    m.w32(CS(m) + 36, m.r32(CS(m) + 36) | 4u);
                    if (!(m.r32(sc + 40) & 7u)) voice_for_repeat(39);
                    smoke_trail(e + 8, e + 56, P(m) + 28, 0);
                }
            }
        }
    }
    if (!(m.r32(P(m)) & 0x7000u)) {
        if (m.r32(CS(m) + 36) & 1u) {
            hud_alert_k3();
            m.w16(P(m) + 84, 1);
        }
        if (m.r32(CS(m) + 36) & 2u) {
            hud_alert_k15();
            m.w16(P(m) + 84, 12);
        }
        if (m.r32(CS(m) + 36) & 4u) {
            hud_alert_damage();
            m.w16(P(m) + 84, 1);
        }
        if (m.r16s(P(m) + 76) < 1024) rumble_add(0);
        engine_sound_update();
    }
    if (m.r16(P(m) + 84)) {
        m.w16(P(m) + 84, m.r16(P(m) + 84) - 1u);
        rumble_add(1);
    }
    if (m.r32s(P(m) + 72) > 573440) {
        rumble_add(2);
        hud_alert_speed(2048);
    }
    const bool lockable = target_is_lockable();
    const u32 sc = m.r32(g::kScene);
    s32 gun = m.r8(sc + 937), special, stock;
    u32 racks;
    if (lockable) {
        special = m.r8(sc + 939);
        stock = m.r16s(fp + 126);
        racks = P(m) + 100;
    } else {
        special = m.r8(sc + 938);
        stock = m.r16s(fp + 124);
        racks = P(m) + 88;
    }
    cover(0x8003A624u);
    const s32 ov = special_weapon_override();
    if (ov >= 0) special = ov;
    if (m.r16s(0x800B3B28u + 2u * static_cast<u32>(special)) < stock) stock = m.r16s(0x800B3B28u + 2u * static_cast<u32>(special));
    m.w32(CS(m) + 4, static_cast<u32>(stock));
    reload_tick();
    if (mask & 8u) {
        if (m.r16(P(m) + 86)) m.w16(P(m) + 86, m.r16(P(m) + 86) - 1u);
        m.w32(P(m) + 16, m.r32(P(m) + 16) & ~0x2000u);
        if (m.r32(m.r32(g::kScene) + 296) & 0x800u) fire_gun(static_cast<u32>(gun));
        m.w32(P(m) + 16, m.r32(P(m) + 16) & ~0x4000u);
        if (m.r32(m.r32(g::kScene) + 300) & 0x1000u) fire_special(static_cast<u32>(special), racks);
    }
    const u32 free = slot_take(racks, m.r32s(CS(m) + 4), 0);
    m.w32(CS(m) + 32, m.r32(CS(m) + 4) - free);
}

void Game::place_on_deck(u32 e, u32 st, u32 where) {
    cover(0x8007599Cu);
    Frame f(*this, 64);
    const u32 a = f.at(0x10), b = f.at(0x20);
    m.w32(st + 72, 0);
    m.w32(a, m.r32(where));
    m.w32(a + 4, m.r32(where + 4) - 409600u);
    m.w32(a + 8, m.r32(where + 8));
    m.w32(b, m.r32(where));
    m.w32(b + 4, m.r32(where + 4) + 409600u);
    m.w32(b + 8, m.r32(where + 8));
    if (ray_cell(a, b, st + 28, g::kScratch) == 0x7FFFFFFF) {
        m.w32(st + 28, m.r32(where));
        m.w32(st + 32, m.r32(where + 4));
        m.w32(st + 36, m.r32(where + 8));
    } else {
        const u32 n = m.r32(kHitNormal);
        m.w16(st + 20, m.r16(n));
        m.w16(st + 22, m.r16(n + 2));
        const u32 nz = m.r16(n + 4);
        m.w32(st + 44, m.r32(kHitFlags));
        m.w16(st + 24, nz);
    }
    m.w32(e + 12, m.r32(st + 32) - 270336u);
    m.w32(e + 8, m.r32(st + 28));
    const u32 z = m.r32(st + 36);
    m.w16(e + 24, 0);
    const u32 pz = m.r16(e + 24);
    m.w32(e + 16, z);
    m.w16(e + 28, 0);
    m.w16(e + 26, m.r16(where + 18));
    m.w16(st + 64, pz);
    m.w16(st + 66, m.r16(e + 26));
    m.w16(st + 68, m.r16(e + 28));
    player_view_angles(e, st);
}

void Game::runway_start(u32 e, u32 st, u32 ctl, u32 surf, u32 where, u32 short_field) {
    cover(0x80075E08u);
    Frame f(*this, 0x68);
    place_on_deck(e, st, where);
    const u32 rot = f.at(0x18), v = f.at(0x10), out = f.at(0x38);
    for (u32 o : {0x30u, 0x32u, 0x34u, 0x38u, 0x3Au, 0x3Cu}) m.w16(st + o, 0);
    m.w16(st + 0x4C, 0x1000);
    for (u32 o : {4u, 6u, 8u, 0xAu, 0xCu}) m.w16(ctl + o, 0);
    m.w16(ctl + 0xE, 0x1000);
    m.w16(surf + 6, 0x1000);
    for (u32 o : {0u, 2u, 4u, 8u}) m.w16(surf + o, 0);
    m.w32(st + 0x48, 0x20000);
    m.w32(st + 0xC, 0);
    m.w16(v, 0);
    m.w16(v + 2, 0);
    m.w16(v + 4, 0x1000);
    rot_matrix_yxz(e + 0x18, rot);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
    auto turn = [&] {
        gte.mtc2(0, m.r32(v));
        gte.mtc2(1, m.r32(v + 4));
        gte.op(0x486012);
        m.w32(out, gte.mfc2(25));
        m.w32(out + 4, gte.mfc2(26));
        m.w32(out + 8, gte.mfc2(27));
    };
    turn();
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) << 5);
    m.w32(e + 0x38, m.r32(out));
    m.w32(e + 0x3C, m.r32(out + 4));
    m.w32(e + 0x40, m.r32(out + 8));
    m.w16(v + 2, short_field ? 0xF830u : 0xF254u);
    m.w16(v, 0);
    m.w16(v + 4, short_field ? 0x1388u : 0x32C8u);
    turn();
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) << 12);
    m.w32(e + 8, m.r32(e + 8) + m.r32(out));
    m.w32(e + 12, m.r32(e + 12) + m.r32(out + 4));
    m.w32(e + 16, m.r32(e + 16) + m.r32(out + 8));
    player_view_angles(e, st);
}

bool Game::catapult(u32 e, u32 st, u32 ctl, u32 surf, u32 where, u32 strong) {
    cover(0x80075B04u);
    Frame f(*this, 0x68);
    const s32 phase = m.r32s(st + 8);
    if (phase == 3) return true;
    if (phase == 0) {
        place_on_deck(e, st, where);
        m.w32(st + 0x48, 0);
        m.w32(e + 0x38, 0);
        m.w32(e + 0x3C, 0);
        m.w32(e + 0x40, 0);
        for (u32 o : {0x30u, 0x32u, 0x34u, 0x38u, 0x3Au, 0x3Cu, 0x4Cu}) m.w16(st + o, 0);
        for (u32 o : {4u, 6u, 8u, 0xAu, 0xCu}) m.w16(ctl + o, 0);
        m.w16(ctl + 0xE, 0xF000);
        m.w16(surf, 0);
        m.w16(surf + 2, 0);
        m.w16(surf + 4, 0);
        m.w16(surf + 6, 0xF000);
        m.w16(surf + 8, 0);
        m.w32(st + 0xC, 0);
        m.w32(st + 8, 1);
    } else if (phase == 1) {
        const s32 thr = static_cast<s32>(wrap::div(m.r32s(st + 0xC) << 13, 90).lo) - 0x1000;
        m.w16(ctl + 0xE, static_cast<u32>(thr));
        if (static_cast<s16>(thr) >= 0x1001) m.w16(ctl + 0xE, 0x1000);
        m.w16(surf + 6, m.r16(ctl + 0xE));
        m.w32(st + 0xC, m.r32(st + 0xC) + 1);
        if (m.r32s(st + 0xC) >= 91) {
            m.w32(st + 8, 2);
            m.w32(st + 0xC, 0);
            const s32 v = m.r16s(P(m) + 0x52);
            if (v >= 0) voice_release(static_cast<u32>(v));
            m.w16(P(m) + 0x52, sound_play(0x5C));
        }
    } else if (phase == 2) {
        const u32 push = strong ? 0x30Cu : 0x11Cu;
        const u32 rot = f.at(0x18), v = f.at(0x10), out = f.at(0x38);
        m.w32(st + 0x48, m.r32(st + 0x48) + push);
        m.w16(v, 0);
        m.w16(v + 2, 0);
        m.w16(v + 4, push);
        rot_matrix_yxz(e + 0x18, rot);
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
        gte.mtc2(0, m.r32(v));
        gte.mtc2(1, m.r32(v + 4));
        gte.op(0x486012);
        m.w32(out, gte.mfc2(25));
        m.w32(out + 4, gte.mfc2(26));
        m.w32(out + 8, gte.mfc2(27));
        m.w32(e + 0x38, m.r32(e + 0x38) + m.r32(out));
        m.w32(e + 0x3C, m.r32(e + 0x3C) + m.r32(out + 4));
        m.w32(e + 0x40, m.r32(e + 0x40) + m.r32(out + 8));
        m.w32(e + 8, m.r32(e + 8) + m.r32(e + 0x38));
        m.w32(e + 12, m.r32(e + 12) + m.r32(e + 0x3C));
        m.w32(e + 16, m.r32(e + 16) + m.r32(e + 0x40));
        if (0xF000 < m.r32s(st + 0x48)) {
            m.w32(st + 8, 3);
            m.w32(st + 0xC, 0);
            const s32 vv = m.r16s(P(m) + 0x52);
            if (vv >= 0) voice_release(static_cast<u32>(vv));
        }
    }
    player_view_angles(e, st);
    return false;
}

bool Game::carrier_launch(u32 e, u32 st, u32 surf, u32 c) {
    cover(0x80074BA8u);
    Frame f(*this, 48);
    m.w16(surf, 0);
    m.w16(surf + 2, 0);
    m.w16(surf + 4, 0);
    m.w16(surf + 6, 2048);
    m.w16(surf + 8, 0);
    m.w16(surf + 10, 0);
    m.w32(surf + 12, 0);
    const s32 phase = m.r32s(st + 8);
    if (phase == 1) {
        m.w32(st + 12, m.r32(st + 12) + 1);
        if (m.r32s(st + 12) >= 31) {
            m.w32(st + 12, 0);
            m.w32(st + 8, 2);
        }
    } else if (phase == 0) {
        const s32 dy = m.r32(kCarrierKind) ? -2990080 : -3399680;
        const u32 h = m.r16(c + 18);
        const s32 sn = m.r16s(g::kSinCos + 2 * (h & 0x3FFFu)) >> 2;
        const s32 cs = m.r16s(g::kSinCos + 0x2000 + 2 * (h & 0x3FFFu)) >> 2;
        m.w32(e + 8, static_cast<u32>(-1920 * sn + m.r32s(c)));
        m.w32(e + 12, static_cast<u32>(dy + m.r32s(c + 4)));
        m.w32(e + 16, static_cast<u32>(-1920 * cs + m.r32s(c + 8)));
        m.w32(e + 60, static_cast<u32>((-65536 * rsin(-318)) >> 12));
        m.w16(e + 24, static_cast<u32>(-273));
        m.w32(e + 56, static_cast<u32>(16 * sn));
        m.w32(e + 64, static_cast<u32>(16 * cs));
        m.w16(e + 26, h);
        m.w16(e + 28, 0);
        m.w32(st + 72, 0x10000);
        m.w32(st + 12, 0);
        m.w32(st + 8, 1);
    } else if (phase == 2) {
        const s32 t = m.r32s(st + 12);
        const s32 k = t >= 76 ? 4096 : (t << 12) / 75;
        const s32 v = (rsin((k >> 1) + 1024) >> 1) + 2048;
        const s32 pitch = -409 * v;
        const s32 s = rsin(-182 * v / 4096 - 136);
        m.w16(e + 24, static_cast<u32>(pitch / 4096 + 136));
        m.w32(e + 60, static_cast<u32>((-65536 * s) >> 12));
        const s32 t2 = m.r32s(st + 12);
        if (!(t2 < 121) || (m.r32(st + 16) & 9u)) return true;
        m.w32(st + 12, static_cast<u32>(t2 + 1));
    }
    vec_to_angles(e + 56, st + 64);
    player_view_angles(e, st);
    return false;
}

void Game::landing_roll(u32 e, u32 st, u32 hard) {
    cover(0x80074878u);
    Frame f(*this, 48);
    s32 vy = m.r32s(e + 60);
    const s32 k = hard ? 3936 : 4084;
    const s32 vx = m.r32s(e + 56) * k, vz = m.r32s(e + 64) * k;
    if (!(m.r32(st + 16) & 0xFu)) {
        vy += 669;
    } else {
        s32 decay = 3840;
        s32 sink = 1024;
        if (vy < 1025) sink = vy;
        if (sink < 0) sink = 0;
        const s32 roll = m.r16s(e + 28);
        if (absv(roll) < 1536) decay = (sink >> 3) + 3776;
        m.w16(e + 28, static_cast<u32>(roll * decay / 4096));
        const s32 pitch = m.r16s(e + 24);
        if (!m.r32(st + 8)) {
            if (pitch >= 0) {
                if (pitch < 182) {
                    m.w32(st + 8, 3);
                    m.w32(st + 12, static_cast<u32>((pitch << 10) / 182 + 4096));
                } else {
                    m.w32(st + 8, 2);
                }
            } else {
                m.w32(st + 8, 1);
            }
        }
        const s32 ph = m.r32s(st + 8);
        if (ph == 2) {
            s32 d = q12((pitch - 182) * 2 * sink);
            if (d < 8) d = 8;
            m.w16(e + 24, m.r16(e + 24) - static_cast<u32>(d));
            if (pitch < 183) {
                m.w16(e + 24, 182);
                m.w32(st + 12, 5120);
                m.w32(st + 8, 3);
            }
        } else if (ph == 1) {
            s32 d = (182 - pitch) / 2;
            if (d < 2) d = 2;
            m.w16(e + 24, m.r16(e + 24) + static_cast<u32>(d));
            if (pitch >= 182) {
                m.w16(e + 24, 182);
                m.w32(st + 12, 5120);
                m.w32(st + 8, 3);
            }
        } else if (ph == 3) {
            const s32 t = m.r32s(st + 12);
            const s32 w = t >= 4097 ? 4096 : t >= 0 ? t : 0;
            const s32 s = q12(182 * rsin(t));
            m.w16(e + 24, static_cast<u32>(q12(s * w)));
            m.w32(st + 12, static_cast<u32>(m.r32s(st + 12) - (hard ? 128 : 64)));
            if (m.r32s(st + 12) < -512) {
                view_landing(1);
                m.w32(st + 8, 4);
            }
        }
    }
    m.w32(e + 56, static_cast<u32>(vx / 4096));
    m.w32(e + 60, static_cast<u32>(vy));
    m.w32(e + 64, static_cast<u32>(vz / 4096));
    player_view_angles(e, st);
}

void Game::formation_hold(u32 e, u32 st, u32 surf, u32 tanker) {
    cover(0x80074E64u);
    Frame f(*this, 0x68);
    m.w16(surf, 0);
    m.w16(surf + 2, 0);
    m.w16(surf + 4, 0);
    m.w16(surf + 6, 0x800);
    m.w16(surf + 8, 0);
    m.w16(surf + 0xA, 0);
    m.w32(surf + 0xC, 0);
    const u32 t = slot(m.r32(tanker));
    const u32 rot = f.at(0x28), v = f.at(0x20), out = f.at(0x10);
    rot_matrix_yxz(t + 0x18, rot);
    m.w16(v, static_cast<u32>(m.r32s(tanker + 4) >> 6));
    m.w16(v + 2, static_cast<u32>(m.r32s(tanker + 8) >> 6));
    m.w16(v + 4, static_cast<u32>((m.r32s(tanker + 12) - 0x64000) >> 6));
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
    auto turn = [&] {
        gte.mtc2(0, m.r32(v));
        gte.mtc2(1, m.r32(v + 4));
        gte.op(0x486012);
        m.w32(out, gte.mfc2(25));
        m.w32(out + 4, gte.mfc2(26));
        m.w32(out + 8, gte.mfc2(27));
    };
    turn();
    m.w32(e + 8, m.r32(t + 8) + (m.r32(out) << 6));
    m.w32(e + 12, m.r32(t + 12) + (m.r32(out + 4) << 6));
    m.w32(e + 16, m.r32(t + 16) + (m.r32(out + 8) << 6));
    const u32 t2 = slot(m.r32(tanker));
    const s32 s = length3(m.r32s(t2 + 0x38) >> 6, m.r32s(t2 + 0x3C) >> 6, m.r32s(t2 + 0x40) >> 6);
    const s32 sp6 = s << 6;
    m.w16(v, 0);
    m.w16(v + 2, 0);
    m.w16(v + 4, static_cast<u32>(sp6 >> 5));
    m.w32(st + 0x48, static_cast<u32>(sp6));
    turn();
    m.w32(e + 0x38, m.r32(out) << 5);
    m.w32(e + 0x3C, m.r32(out + 4) << 5);
    m.w32(e + 0x40, m.r32(out + 8) << 5);
    m.w16(e + 0x18, m.r16(t + 0x18));
    m.w16(e + 0x1A, m.r16(t + 0x1A));
    m.w16(e + 0x1C, m.r16(t + 0x1C));
}

bool Game::refuel(u32 e, u32 st, u32 rel, u32 tvel) {
    cover(0x80075084u);
    Frame f(*this, 32);
    const s32 ox = clampv(m.r32s(rel) >> 5, -4096, 4096);
    const s32 oy = clampv(m.r32s(rel + 4) >> 5, -4096, 4096);
    const s32 oz = clampv(m.r32s(rel + 8) >> 5, -4096, 4096);
    auto nudge = [&](u32 at) {
        const s32 v = m.r16s(at);
        s32 d = (0 - v) >> 6;
        if (!d) {
            if (v > 0) d = -1;
            if (!d) d = v < 0 ? 1 : 0;
        }
        return d;
    };
    const s32 dp = nudge(rel + 16), dh = nudge(rel + 18), dr = nudge(rel + 20);
    const u32 roll = m.r16(e + 28);
    m.w16(e + 24, m.r16(e + 24) + static_cast<u32>(dp));
    const u32 head = m.r16(e + 26);
    m.w16(e + 28, roll + static_cast<u32>(dr));
    m.w16(e + 26, head + static_cast<u32>(dh));
    const s32 phase = m.r32s(st + 8);
    if (phase == 0) {
        for (u32 k = 0; k < 12; k += 4) {
            const s32 t = m.r32s(tvel + k);
            s32 v = m.r32s(e + 56 + k);
            if (v < t) {
                m.w32(e + 56 + k, static_cast<u32>(v + 256));
                v = m.r32s(e + 56 + k);
            }
            if (t < v) m.w32(e + 56 + k, static_cast<u32>(v - 256));
        }
        bool close = true;
        for (u32 k = 0; k < 12 && close; k += 4) {
            const s32 d = m.r32s(e + 56 + k) - m.r32s(tvel + k);
            close = d < 0 ? -d < 256 : d < 256;
        }
        if (close) {
            m.w32(st + 8, 1);
            m.w32(st + 12, 0);
        }
    } else if (phase == 1) {
        m.w32(e + 56, static_cast<u32>(m.r32s(tvel) - ox));
        m.w32(e + 60, static_cast<u32>(m.r32s(tvel + 4) - oy));
        m.w32(e + 64, static_cast<u32>(m.r32s(tvel + 8) - oz));
        if (absv(ox) + absv(oy) + absv(oz) < 12288) {
            if (m.r32s(st + 12) == 150) sound_play(m.r32(kCarrierKind) == 2 ? 94u : 95u);
            const s32 c = m.r32s(st + 12) + 1;
            m.w32(st + 12, static_cast<u32>(c));
            if (c >= 301) return true;
        }
    }
    player_view_angles(e, st);
    return false;
}

void Game::ending_climb(u32 e, u32 st) {
    cover(0x8007601Cu);
    Frame f(*this, 48);
    const s32 phase = m.r32s(st + 8);
    if (phase == 0) {
        if (m.r32(st + 4) != 0x4000000) return;
        m.w32(st + 8, 1);
        m.w32(st + 12, 0);
        m.w16(st + 78, 0);
        m.w16(st + 80, 0);
        m.w32(st, 16);
        return;
    }
    if (phase != 1) return;
    s32 t = m.r32s(st + 12);
    if (t >= 301) t = 300;
    const s32 lift = 5120 * t;
    const s32 left = t - 300;
    const s32 vy = m.r32s(e + 60) * left, vz = m.r32s(e + 64) * left;
    const s32 climb = (lift / 300) << 6;
    const s32 y = m.r32s(e + 12);
    m.w32(e + 60, static_cast<u32>(vy / 300));
    m.w32(e + 64, static_cast<u32>(vz / 300));
    if (y > 245759999) {
        m.w32(e + 12, 245760000);
    } else {
        m.w32(e + 12, static_cast<u32>(y + (climb >> 1)));
        m.w32(e + 16, m.r32(e + 16) + static_cast<u32>((lift / 300) << 7));
    }
    const u32 save = f.at(0x10);
    for (u32 k = 0; k < 16; k += 4) m.w32(save + k, m.r32(e + 56 + k));
    m.w32(e + 60, static_cast<u32>(climb));
    player_view_angles(e, st);
    m.w32(st + 72, static_cast<u32>(length3((0 - m.r32s(e + 56)) >> 6, (0 - m.r32s(e + 60)) >> 6, (0 - m.r32s(e + 64)) >> 6) << 6));
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 56 + k, m.r32(save + k));
    m.w32(st + 12, m.r32(st + 12) + 1);
    if (m.r32s(e + 12) <= 0) return;
    const s16 c = static_cast<s16>(m.r16(st + 78) + 1u);
    m.w16(st + 78, static_cast<u32>(c));
    if (c >= 301 && m.r32(st + 4) == 0x4000000) {
        m.w16(e + 78, 0xFFFF);
        return;
    }
    if (m.r16s(st + 78) >= 31 && m.r32(st + 4) == 0x8000000) m.w32(st, m.r32(st) | 0x200u);
}

void Game::shot_down(u32 e, u32 st) {
    cover(0x80074560u);
    Frame f(*this, 40);
    const s32 phase = m.r32s(st + 8);
    if (phase == 1 || phase == 2) {
        if (!(m.r32(st + 16) & 0xFu)) m.w32(e + 60, m.r32(e + 60) + static_cast<u32>(669 * (m.r32s(st + 12) >> 5)));
        const s32 pitch = m.r16s(e + 24);
        if (phase == 1) {
            m.w16(e + 24, static_cast<u32>(pitch + q12(30 * (-4096 - pitch))));
            s32 r = m.r16s(st + 52) + m.r16s(st + 52) / 8;
            r = clampv(r, -512, 512);
            m.w16(st + 52, static_cast<u32>(r));
            m.w16(e + 28, m.r16(e + 28) + static_cast<u32>(r));
        } else {
            m.w16(e + 24, static_cast<u32>(pitch + q12(15 * (-4096 - pitch))));
            const u32 at = (m.r32(st + 12) << 9) & 0x7E00u;
            m.w16(e + 28, m.r16(e + 28) + static_cast<u32>(m.r16s(g::kSinCos + at) >> 6));
        }
    } else if (phase == 16) {
        wreck_scatter(e + 8, m.r32(st + 12));
        m.w32(e, m.r32(e) & ~0x10000u);
    } else if (phase == 0) {
        const u32 sc = m.r32(g::kScene);
        m.w32(st + 12, 0);
        m.w32(st + 8, (m.r32(sc + 36) & 4u) ? 1u : 2u);
        const s32 r = m.r16s(st + 52);
        if (m.r16(st + 52)) {
            if (absv(r) < 32) m.w16(st + 52, m.r16(st + 52) * 4u);
        } else {
            m.w16(st + 52, (m.r16(sc + 36) & 0x7Fu) - 64u);
        }
        player_explosion(e + 8, e + 56);
        hud_alert_k9();
        sound_play(32);
    }
    const u32 t = m.r32(st + 12);
    m.w32(st + 12, t + 1);
    if (!(m.r32(st + 8) & 3u)) return;
    wreck_burn(e + 8, e + 56, t + 1);
    hud_alert_burning();
    if (!(m.r32(st + 16) & 0xFu)) return;
    m.w32(st + 12, 0);
    m.w32(st + 8, 16);
    player_explosion(e + 8, e + 56);
    hud_alert_k60();
    sound_play(34);
    m.w32(e + 56, 0);
    m.w32(e + 60, 0);
    m.w32(e + 64, 0);
}

void Game::sim_step1() {
    cover(0x800729DCu);
    Frame frame(*this, 0x128);
    const u32 e = slot(0);
    const u32 st = P(m);
    const u32 fp = m.r32(CS(m));
    const u32 mode = m.r32(st + 4);
    auto ctl = [&] { return m.r32(kCtl); };
    auto surf = [&] { return m.r32(kCtlSurf); };
    auto stick = [&] { return m.r32(kCtlStick); };
    auto zero_ctl = [&] {
        const u32 c = ctl();
        for (u32 o : {8u, 6u, 4u, 0xEu, 0xCu, 0xAu}) m.w16(c + o, 0);
    };
    auto clamp_roll_input = [&] {
        if (m.r32(P(m) + 0x10) & 0xFu) {
            const u32 c = ctl();
            if (m.r16s(c + 8) >= 0x101) m.w16(c + 8, 0x100);
            if (m.r16s(m.r32(kCtl) + 8) < -0x100) m.w16(m.r32(kCtl) + 8, 0xFF00);
        }
    };
    const u32 carrier_flag = m.r32(kCarrierKind);
    auto kind_ne2 = [&] { return m.r32(kCarrierKind) == 2 ? 0u : 1u; };
    auto approach = [&] { return m.r32(kApproach); };
    switch (mode) {
    case 1:
    case 4:
        player_controls(e, fp);
        scale_by_speed(m.r32s(P(m) + 0x48), surf(), 0x8006E874u);
        clamp_roll_input();
        flight_model(e, ctl(), fp, P(m));
        player_post(mode == 1 ? 0xFFFFFFFFu : 0xFFFFFFF7u);
        break;
    case 2:
        zero_ctl();
        flight_model(e, ctl(), fp, st);
        player_post(0x47);
        break;
    case 8: {
        player_controls(e, fp);
        const u32 s0 = landing_check(e + 8, m.r32(kCarrier));
        if (m.r32(P(m) + 0x10) & 9u) {
            if (s0 == 0) {
                m.w16(e + 0x4E, 0xFFFF);
            } else if (s0 == 1) {
                m.w32(P(m), m.r32(P(m)) | 0x80u);
                sound_play(0x5A);
                if (m.r32(kCarrierKind) == 1) sound_play(0x5B);
            } else {
                m.w32(P(m), m.r32(P(m)) | 0x100u);
            }
            player_mode_from_carrier(m.r32(kCarrierKind) ? 1u : 0u, 2);
            m.w16(P(m) + 0x4E, 0x1E);
        }
        flight_model(e, ctl(), fp, P(m));
        player_post(s0 ? 0xFFFFFFE7u : 0xFFFFFFF7u);
        m.w32(approach() + 0x34, s0);
        break;
    }
    case 0x10: {
        const bool done = carrier_launch(e, st, surf(), m.r32(kCarrier));
        if (done) player_mode_from_carrier(m.r32(kCarrierKind) ? 1u : 0u, 2);
        player_post(6);
        m.w32(approach() + 0x34, 1);
        break;
    }
    case 0x20: {
        landing_roll(e, st, 0);
        player_post(0x46);
        const u32 s0 = e + 8;
        const u32 v = landing_check(s0, m.r32(kCarrier));
        if ((m.r32(P(m) + 0x10) & 9u) && !v) {
            const u32 cs = CS(m);
            m.w32(cs + 0x24, m.r32(cs + 0x24) | 4u);
            if (!(m.r32(m.r32(g::kScene) + 0x28) & 7u)) voice_for_repeat(0x27);
            smoke_trail(s0, e + 0x38, P(m) + 0x1C, 0);
            const s16 n = static_cast<s16>(m.r16(P(m) + 0x4E) - 1u);
            m.w16(P(m) + 0x4E, static_cast<u32>(n));
            if (!(n > 0)) m.w16(e + 0x4E, 0xFFFF);
        }
        break;
    }
    case 0x40: {
        landing_roll(e, st, 1);
        const u32 v = landing_check(e + 8, m.r32(kCarrier));
        if ((m.r32(P(m) + 0x10) & 9u) && !v) m.w16(e + 0x4E, 0xFFFF);
        player_post(6);
        break;
    }
    case 0x100: {
        player_controls(e, fp);
        flight_model(e, ctl(), fp, P(m));
        const bool s0 = refuel_window(e, m.r32(kTanker), m.r32(kTankerTarget), frame.at(0x18));
        player_post(0xFFFFFFF7u);
        const u32 p = P(m);
        if (!m.r32(p + 8)) {
            m.w32(p + 8, 1);
            m.w32(p + 0xC, 0);
        }
        if (s0 && m.r32(P(m) + 8) == 1) m.w32(P(m) + 0xC, m.r32(P(m) + 0xC) + 1);
        else m.w32(P(m) + 0xC, 0);
        if (0x2D < m.r32s(P(m) + 0xC)) {
            m.w32(P(m), m.r32(P(m)) | 0x80u);
            player_mode_from_refuel(kind_ne2(), 2);
        }
        m.w32(approach() + 0x34, s0 ? 1u : 0u);
        break;
    }
    case 0x200:
        formation_hold(e, st, surf(), m.r32(kTanker));
        player_post(4);
        player_mode_from_refuel(kind_ne2(), 2);
        break;
    case 0x400: {
        const u32 tanker = m.r32(kTanker);
        const u32 t = slot(m.r32(tanker));
        refuel_window(e, tanker, m.r32(kTankerTarget), frame.at(0x58));
        const bool done = refuel(e, P(m), frame.at(0x7C), t + 0x38);
        player_post(4);
        if (done) player_mode_from_refuel(kind_ne2(), 3);
        break;
    }
    case 0x800:
        m.w32(frame.at(0x10), m.r32(kCtlSurf));
        controls_launch(e, st, stick(), ctl(), surf());
        flight_model(e, ctl(), fp, P(m));
        player_post(4);
        m.w32(approach() + 0x34, 0);
        break;
    case 0x1000:
        player_post(4);
        break;
    case 0x2000:
        place_on_deck(e, st, m.r32(kCarrier));
        scale_by_speed(m.r32s(P(m) + 0x48), surf(), 0x8006E874u);
        m.w32(CS(m) + 0x2C, 0);
        m.w32(P(m) + 0x48, 0);
        m.w16(ctl() + 0xE, 0xF000);
        engine_sound_update();
        break;
    case 0x4000:
    case 0x8000: {
        m.w16(stick() + 6, 0x1000);
        const u32 strong = m.r32(st + 4) == 0x8000 ? 1u : 0u;
        m.w32(frame.at(0x10), m.r32(kCarrier));
        m.w32(frame.at(0x14), strong);
        if (!catapult(e, st, ctl(), surf(), m.r32(kCarrier), strong)) {
            engine_sound_update();
            break;
        }
        player_controls(e, fp);
        scale_by_speed(m.r32s(P(m) + 0x48), ctl() + 4, 0x8006E7E0u);
        scale_by_speed(m.r32s(P(m) + 0x48), surf(), 0x8006E874u);
        flight_model(e, ctl(), fp, P(m));
        player_post(0xFFFFFFD7u);
        if (off_deck(e + 8, m.r32(kCarrier))) player_mode_from_launch(strong, 2);
        break;
    }
    case 0x10000:
        m.w32(frame.at(0x10), m.r32(kCarrier));
        m.w32(frame.at(0x14), 0);
        runway_start(e, st, ctl(), surf(), m.r32(kCarrier), 0);
        player_mode_from_launch(0, 2);
        break;
    case 0x100000:
        shot_down(e, st);
        player_post(6);
        break;
    case 0x200000:
    case 0x400000:
        m.w32(frame.at(0x10), mode == 0x200000 ? 1u : 0u);
        controls_level(e, stick(), ctl(), surf(), mode == 0x200000 ? 1u : 0u);
        flight_model(e, ctl(), fp, P(m));
        player_post(0x47);
        break;
    case 0x1000000:
    case 0x2000000:
    case 0x4000000:
        controls_ground(m.r32(g::kScene) + 0x120, stick(), ctl(), surf());
        scale_by_speed(m.r32s(P(m) + 0x48), surf(), 0x8006E874u);
        flight_simple(e, ctl(), fp, P(m));
        if (mode == 0x1000000) {
            player_post(0xFFFFFFADu);
            break;
        }
        if (mode == 0x4000000) ending_climb(e, P(m));
        player_post(0xFFFFFFA5u);
        if (mode == 0x4000000) {
            const bool on = on_deck(e + 8);
            if (m.r32(P(m)) == 0x10 && on) {
                m.w16(P(m) + 0x50, m.r16(P(m) + 0x50) + 2u);
                if (m.r16s(P(m) + 0x50) >= 0x3D) {
                    player_mode_end(1);
                    m.w32(P(m), m.r32(P(m)) | 0x280u);
                }
            }
        }
        break;
    case 0x8000000: {
        m.w16(e + 0x1A, 0);
        if (m.r16s(e + 0x1C) < 4) m.w16(e + 0x1C, m.r16(e + 0x1C) + 4u);
        if (!(m.r16s(e + 0x1C) < -3)) m.w16(e + 0x1C, m.r16(e + 0x1C) - 4u);
        if (m.r32(m.r32(g::kScene) + 4) != 0 || absv(m.r16s(e + 0x1C)) >= 0x223) {
            controls_hold(e, stick(), ctl(), surf());
        } else {
            const u32 c = ctl(), s = stick();
            for (u32 o : {8u, 6u, 4u, 0xEu, 0xCu, 0xAu}) m.w16(c + o, 0);
            for (u32 o : {0u, 2u, 4u, 6u}) m.w16(s + o, 0);
        }
        scale_by_speed(m.r32s(P(m) + 0x48), surf(), 0x8006E874u);
        flight_simple(e, ctl(), fp, P(m));
        ending_climb(e, P(m));
        player_post(0xFFFFFFA5u);
        break;
    }
    case 0x10000000:
        zero_ctl();
        flight_simple(e, ctl(), fp, st);
        ending_climb(e, P(m));
        player_post(0xFFFFFFA5u);
        break;
    case 0x20000000:
        engine_sound_update();
        break;
    default:
        break;
    }
    (void)carrier_flag;
    if (m.r32(P(m) + 4) & 0x18u) {
        const u32 c = m.r32(kCarrier);
        const u32 rot = frame.at(0x98), v = frame.at(0xB8), out = frame.at(0xC0);
        rot_matrix_yxz(c + 0x10, rot);
        m.w16(v, static_cast<u32>((m.r32s(e + 8) - m.r32s(c)) >> 12));
        m.w16(v + 2, static_cast<u32>((m.r32s(e + 12) - m.r32s(c + 4)) >> 12));
        m.w16(v + 4, static_cast<u32>((m.r32s(e + 16) - m.r32s(c + 8)) >> 12));
        for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
        gte.mtc2(0, m.r32(v));
        gte.mtc2(1, m.r32(v + 4));
        gte.op(0x486012);
        m.w32(out, gte.mfc2(25));
        m.w32(out + 4, gte.mfc2(26));
        m.w32(out + 8, gte.mfc2(27));
        const u32 a = approach();
        m.w32(a + 0x1C, m.r32(out) << 12);
        m.w32(a + 0x20, m.r32(out + 4) << 12);
        m.w32(a + 0x24, m.r32(out + 8) << 12);
        const u32 p = P(m);
        s32 a0 = m.r16s(p + 0x40), a2 = m.r16s(p + 0x42), v1 = m.r16s(p + 0x44);
        a0 = wrap_near(m.r16s(c + 0x10), a0);
        a2 = wrap_near(m.r16s(c + 0x12), a2);
        v1 = wrap_near(m.r16s(c + 0x14), v1);
        m.w16(a + 0x2C, static_cast<u32>(a0) - m.r16(c + 0x10));
        m.w16(a + 0x2E, static_cast<u32>(a2) - m.r16(c + 0x12));
        m.w16(a + 0x30, static_cast<u32>(v1) - m.r16(c + 0x14));
        m.w32(approach() + 0x18, static_cast<u32>(distance(m.r32(kCarrier), e + 8)));
    }
    if (m.r32(P(m) + 4) & 0x1F00u) {
        const u32 tanker = m.r32(kTanker);
        const u32 t = slot(m.r32(tanker));
        const u32 rs = frame.at(0xD0);
        const bool ok = refuel_window(e, tanker, m.r32(kTankerTarget), rs);
        const u32 a = approach();
        m.w16(a + 0x10, m.r16(t + 0x18));
        m.w16(a + 0x12, m.r16(t + 0x1A));
        m.w16(a + 0x14, m.r16(t + 0x1C));
        m.w32(a, m.r32(rs));
        m.w32(a + 4, m.r32(rs + 4));
        m.w32(a + 8, m.r32(rs + 8));
        m.w32(a + 0x1C, m.r32(rs + 0x24));
        m.w32(a + 0x20, m.r32(rs + 0x28));
        m.w32(a + 0x24, m.r32(rs + 0x2C));
        m.w16(a + 0x2C, m.r16(rs + 0x34));
        m.w16(a + 0x2E, m.r16(rs + 0x36));
        m.w16(a + 0x30, m.r16(rs + 0x38));
        m.w32(a + 0x34, ok ? 1u : 0u);
        m.w32(a + 0x18, m.r32(rs + 0x20));
    }
    if (m.r16s(e + 0x4E) <= 0 && !(m.r32(P(m)) & 0x7000u)) {
        const u32 sc = m.r32(g::kScene);
        m.w16(sc + 0x958, m.r16(sc + 0x958) + 1u);
        player_out();
        u32 k = 2;
        if (!(m.r32(m.r32(g::kWorld)) & 0x10u)) k = (m.r32(P(m) + 0x10) & 0x2Fu) ? 1u : 0u;
        view_end(k);
    }
}
}
