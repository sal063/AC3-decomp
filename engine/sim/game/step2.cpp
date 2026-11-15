#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kVoices = 0x800C5830u;
constexpr u32 kSoundDefs = 0x800C5A10u;
constexpr u32 kCueSlots = 0x800C6550u;
constexpr u32 kDeathTimer = 0x800C70D0u;
constexpr u32 kPairs = 0x800C6D80u;
constexpr u32 kGroundState = 0x800C7050u;
constexpr u32 kMsgState = 0x800BCFECu;
constexpr u32 kMessages = 0x800B1A10u;
constexpr u32 kMsgStreams = 0x800B19B0u;
constexpr u32 kGlyphs = 0x800B1914u;
constexpr u32 kCdRing = 0x800BCBBCu;
constexpr u32 kRadio = 0x800C7190u;
}

u32 Game::voice_find_free() {
    cover(0x8002DD38u);
    for (u32 v = 12; v < 24; ++v) {
        const u32 e = kVoices + 20 * v;
        if (!m.r8(e + 3)) {
            m.w8(e + 3, 2);
            return v;
        }
    }
    return 255;
}

u32 Game::voice_find_playing(u32 id) {
    cover(0x8002DCF8u);
    for (u32 v = 12; v < 24; ++v)
        if (m.r8(kVoices + 20 * v) == id) return v;
    return 255;
}

u32 Game::voice_start(u32 id, u32 v) {
    cover(0x8002E514u);
    if (v == 255) return 255;
    const u32 e = kVoices + 20 * v, d = kSoundDefs + 20 * id;
    m.w8(e + 1, 1);
    m.w8(e + 3, 1);
    m.w8(e + 2, 16);
    m.w8(e, id);
    const u32 vl = m.r16(d + 4);
    m.w16(e + 4, vl);
    m.w16(e + 8, vl);
    const u32 vr = m.r16(d + 6);
    m.w16(e + 6, vr);
    m.w16(e + 10, vr);
    const u32 pitch = m.r16(d + 8);
    m.w8(e + 16, 0x80);
    m.w8(e + 17, 0x80);
    m.w16(e + 18, 0);
    m.w16(e + 12, pitch);
    m.w16(e + 14, pitch);
    return v;
}

u32 Game::voice_for_repeat(u32 id) {
    cover(0x8002DBE0u);
    u32 v = m.r8(kSoundDefs + 20 * id);
    if (v == 255) {
        v = voice_find_playing(id);
        if (v == 255) {
            v = voice_find_free();
            if (v == 255) return 255;
        }
    }
    voice_start(id, v);
    return v;
}

u32 Game::sound_play(u32 id) {
    cover(0x8002DB4Cu);
    if (id - 46u < 4u) voice_for_repeat(id);
    u32 v = m.r8(kSoundDefs + 20 * id);
    if (v == 255) {
        v = voice_find_free();
        if (v == 255) return 255;
    }
    voice_start(id, v);
    return v;
}

void Game::sound_listener(u32 buf, u32 ear, u32 src) {
    cover(0x8002E6DCu);
    Frame f(*this, 96);
    const s32 d = distance(ear, src);
    const s32 prev = m.r32s(buf);
    m.w32(buf + 4, (prev && prev != 0x100000) ? static_cast<u32>(prev - d) : 0u);
    s32 dd = m.r32s(buf + 4);
    if (dd < 0) dd = -dd;
    if (dd >= 301) m.w32(buf + 4, 0);
    m.w32(buf, static_cast<u32>(d));
    const u32 mat = f.at(0x10), v = f.at(0x30), out = f.at(0x40);
    rot_matrix_yxz_t(ear + 16, mat);
    m.w32(v, static_cast<u32>((m.r32s(src) - m.r32s(ear)) >> 12));
    m.w32(v + 4, static_cast<u32>((m.r32s(src + 4) - m.r32s(ear + 4)) >> 12));
    m.w32(v + 8, static_cast<u32>((m.r32s(src + 8) - m.r32s(ear + 8)) >> 12));
    apply_matrix_lv(mat, v, out);
    m.w16(buf + 8, static_cast<u32>(ratan2(m.r32s(out + 8), m.r32s(out))) & 0x3FFFu);
    const s16 pan = static_cast<s16>(m.r16(buf + 10) + (m.r32s(out + 8) < 0 ? 0xFF00u : 0x100u));
    m.w16(buf + 10, static_cast<u32>(pan));
    if (pan >= 1025) m.w16(buf + 10, 1024);
    else if (pan < -1024) m.w16(buf + 10, static_cast<u32>(-1024));
}

u32 Game::sound_attenuate(u32 v, u32 buf, u32 shift) {
    cover(0x8002E84Cu);
    if (v == 255) return 255;
    const u32 e = kVoices + 20 * v;
    m.w8(e + 1, m.r8(e + 1) | 0x10u);
    s32 k = 0x3FFF - static_cast<s32>(wrap::shl(m.r32(buf), shift));
    bool in = k < 16385;
    if (k < 0) {
        k = 0;
        in = true;
    }
    const s32 sq = in ? k * k : 0x10000000;
    const s32 vol = (m.r16s(e + 4) * (sq >> 14)) >> 14;
    m.w16(e + 6, static_cast<u32>(vol));
    m.w16(e + 4, static_cast<u32>(vol));
    return v;
}

u32 Game::sound_pan(u32 v, u32 buf) {
    cover(0x8002E8DCu);
    if (v == 255) return 255;
    const u32 e = kVoices + 20 * v;
    m.w8(e + 1, m.r8(e + 1) | 0x10u);
    const s32 vol = m.r16s(e + 4);
    const u32 a = (static_cast<u32>(m.r16(buf + 8)) >> 1) & 0x3FFFu;
    const s32 c = vol * m.r16s(g::kSinCos + 2 * (a + 0x1000));
    m.w16(e + 4, static_cast<u32>((vol * m.r16s(g::kSinCos + 2 * a)) >> 14));
    m.w16(e + 6, static_cast<u32>(c >> 14));
    if (m.r16s(e + 4) < 0) m.w16(e + 4, 0u - m.r16(e + 4));
    if (m.r16s(e + 6) < 0) m.w16(e + 6, 0u - m.r16(e + 6));
    const s32 k = 0x4000 - static_cast<s32>(m.r32(buf) << 6);
    const s32 floor = (k < 0 ? 0 : vol * k) >> 14;
    if (m.r16s(e + 4) < floor) m.w16(e + 4, static_cast<u32>(floor));
    if (m.r16s(e + 6) < floor) m.w16(e + 6, static_cast<u32>(floor));
    if (m.r16s(e + 4) >= 0x4000) m.w16(e + 4, 0x3FFF);
    if (m.r16s(e + 6) >= 0x4000) m.w16(e + 6, 0x3FFF);
    return v;
}

u32 Game::sound_side(u32 v, s32 pan) {
    cover(0x8002EA24u);
    if (v == 255) return 255;
    const u32 e = kVoices + 20 * v;
    m.w8(e + 1, m.r8(e + 1) | 0x10u);
    if (pan < 0) m.w16(e + 4, 0u - m.r16(e + 4));
    return v;
}

u32 Game::sound_detune(u32 v, s32 delta) {
    cover(0x8002F0D4u);
    if (v == 255) return 255;
    const u32 e = kVoices + 20 * v;
    const s32 p = m.r16s(e + 12);
    m.w8(e + 1, m.r8(e + 1) | 0x10u);
    m.w16(e + 12, static_cast<u32>(pitch_add(p, static_cast<s16>(delta))));
    return v;
}

u32 Game::sound_at(u32 id, u32 pos, u32 shift) {
    cover(0x8002F3C0u);
    Frame f(*this, 48);
    const u32 v = sound_play(id);
    if (v == 255) return 255;
    const u32 buf = f.at(0x10);
    sound_listener(buf, m.r32(m.r32(g::kWorld) + 40) + 4, pos);
    sound_attenuate(v, buf, shift);
    sound_pan(v, buf);
    sound_side(v, m.r16s(buf + 10));
    const s32 r = rand();
    return sound_detune(v, static_cast<s16>((r & 0x3FF) - 512));
}

u32 Game::sound_at_repeat(u32 id, u32 pos, u32 shift) {
    cover(0x8002F460u);
    Frame f(*this, 48);
    const u32 v = voice_for_repeat(id);
    if (v == 255) return 255;
    const u32 buf = f.at(0x10);
    sound_listener(buf, m.r32(m.r32(g::kWorld) + 40) + 4, pos);
    sound_attenuate(v, buf, shift);
    sound_pan(v, buf);
    sound_side(v, m.r16s(buf + 10));
    const s32 r = rand();
    return sound_detune(v, static_cast<s16>((r & 0x3FF) - 512));
}

s32 Game::cue_add(u32 id, s32 time) {
    cover(0x8002F19Cu);
    for (u32 i = 0; i < 8; ++i) {
        const u32 e = kCueSlots + 4 * i;
        if (m.r16s(e + 2) == -1) {
            m.w8(e, id);
            m.w16(e + 2, static_cast<u32>(time));
            return static_cast<s32>(i);
        }
    }
    return -1;
}

void Game::cd_request_stream(u32 params, u32 a1, u32 a2) {
    cover(0x8002C8D8u);
    const u32 ring = m.r32(kCdRing);
    const u32 n = m.r32(ring + 4);
    m.w32(ring + 4, n + 1);
    const u32 e = ring + 8 + 20 * (n & 0xFu);
    m.w8(e, 3);
    m.w16(e + 2, 0);
    m.w32(e + 4, params);
    m.w32(e + 8, a1);
    m.w32(e + 16, a2);
    m.w8(e + 1, 0);
}

void Game::cd_cancel_streams() {
    cover(0x8002C71Cu);
    const u32 ring = m.r32(kCdRing);
    m.w8(ring + 0x14C, 5);
    m.w32(ring + 0x148, 1);
    m.w8(ring + 0x14D, 0);
    m.w32(ring + 0x15C, 0);
    s32 i = m.r32s(ring);
    while (m.r32s(m.r32(kCdRing) + 4) >= i) {
        const u32 e = m.r32(kCdRing) + 8 + 20 * (static_cast<u32>(i) & 0xFu);
        if (m.r8(e) - 3u < 2u) m.w8(e, 0);
        ++i;
    }
}

s32 Game::glyph_index(u32 c) {
    cover(0x8004D124u);
    c &= 0xFF;
    if (((c - 65) & 0xFF) < 26) return static_cast<s32>(c) - 65;
    if (((c - 48) & 0xFF) < 10) return static_cast<s32>(c) - 19;
    if (c == '.') return 26;
    if (c == '!') return 27;
    if (c == '?') return 28;
    return c == 32 ? 80 : -1;
}

void Game::message_layout(u32 line) {
    cover(0x8004D038u);
    s32 x = m.r16(line);
    const u32 y = m.r16(line + 2);
    for (u32 i = 0;; ++i) {
        const u32 c = m.r8(m.r32(line + 4) + i);
        if (!c) return;
        const s32 gl = glyph_index(c);
        if (gl == 80) {
            x += 10;
            continue;
        }
        if (gl == -1) return;
        const u32 st = m.r32(kMsgState);
        const u32 n = m.r32(st + 272);
        m.w32(st + 272, n + 1);
        const u32 e = st + 8 * n + 8;
        m.w8(e, 0);
        m.w8(e + 1, static_cast<u32>(gl));
        m.w16(e + 2, 0);
        m.w16(e + 4, static_cast<u32>(x));
        m.w16(e + 6, y);
        x = static_cast<s32>((m.r32(kGlyphs + 4 * static_cast<u32>(gl)) >> 16) & 0xFFu) + x - 1;
    }
}

void Game::message_show(u32 id) {
    cover(0x8004CEB0u);
    if (id == 255) return;
    const u32 st = m.r32(kMsgState);
    if (m.r32(st + 284) == id) return;
    const u32 msg = kMessages + 28 * id;
    m.w32(st + 284, id);
    m.w32(st + 276, m.r8(msg + 1));
    const s32 dur = m.r16s(msg + 2);
    m.w32(st + 272, 0);
    m.w32(st + 280, static_cast<u32>(dur));
    if (m.r8(msg) & 1u) {
        cue_add(m.r8(msg + 24), 30);
    } else if (m.r8(msg) & 2u) {
        cd_cancel_streams();
        cd_request_stream(kMsgStreams + 12 * m.r32(msg + 24), 0, 0);
    }
    for (s32 k = 0; k < m.r32s(msg + 4); ++k) message_layout(msg + 8 + 8 * static_cast<u32>(k));
    for (s32 i = m.r32s(m.r32(kMsgState) + 272); i < 32; ++i) m.w8(m.r32(kMsgState) + 8 * static_cast<u32>(i) + 8, 0);
    for (s32 i = 0; i < m.r32s(m.r32(kMsgState) + 272); ++i) m.w8(m.r32(kMsgState) + 8 * static_cast<u32>(i) + 8, 2);
}

u32 Game::radio_allowed(u32 what, s32 mission, u32 kind) {
    cover(0x8004925Cu);
    static constexpr struct { s32 mission; u32 index; } ids[] = {{31, 0}, {32, 1}, {33, 2}, {38, 3}, {39, 4},
                                                                 {47, 5}, {48, 6}, {53, 7}, {58, 8}};
    if (mission < 31) return 0;
    u32 idx = 99;
    for (const auto& e : ids)
        if (e.mission == mission) idx = e.index;
    if (idx == 99) return 0;
    const u32 w = m.r32(0x800B17C4u + 4 * idx);
    if (what == 1) {
        switch (kind) {
        case 6: return w & 0x10000000u;
        case 7: return w & 0x100000u;
        case 8: return w & 0x1000u;
        case 9: return w & 0x10u;
        default: return 0;
        }
    }
    if (what == 4) {
        switch (kind) {
        case 6: return w & 0x1000000u;
        case 7: return w & 0x10000u;
        case 8: return w & 0x100u;
        case 9: return w & 1u;
        default: return 0;
        }
    }
    return 0;
}

s32 Game::radio_call(u32 kind, u32 arg, u32 who, u32 call) {
    cover(0x80048C0Cu);
    const s32 mission = m.r16s(m.r32(g::kScene) + 908);
    if (!(m.r32(m.r32(g::kWorld) + 4) & 4u) || m.r16s(0x800C7194u) != 1) return 0;
    const u32 tick = m.r32(m.r32(g::kWorld) + 12);
    const u32 row = 0x800B1520u + 11 * static_cast<u32>(mission);
    u32 done = m.r32(kRadio);
    who &= 0xFF;
    auto raise = [&](u32 bit, u32 c) {
        m.w32(kRadio, done | bit);
        call = c;
    };
    switch (kind) {
    case 0:
        if (tick & 7u) return 0;
        if (!(done & 0x10u)) {
            who = m.r8(row);
            if (!who) return 0;
            raise(0x10u, 104);
        } else if (!(done & 0x20u)) {
            who = m.r8(row + 1);
            if (!who) return 0;
            raise(0x20u, 105);
        } else {
            return 0;
        }
        break;
    case 1:
        if (tick & 3u) return 0;
        if (!(done & 0x40u)) {
            who = m.r8(row + 2);
            if (!who) return 0;
            raise(0x40u, 106);
        } else if (!(done & 0x80u)) {
            who = m.r8(row + 3);
            if (!who) return 0;
            raise(0x80u, 107);
        } else {
            return 0;
        }
        break;
    case 2:
        if ((tick & 3u) || (done & 0x100u)) return 0;
        who = m.r8(row + 4);
        if (!who) return 0;
        raise(0x100u, 108);
        break;
    case 3: {
        if (tick & 3u) return 0;
        if (who >= 7) return 0;
        const u32 k = m.r8(0x800B1518u + who);
        if (who != m.r8(row + k)) return 0;
        if (k == 0xFF) return 0;
        const u32 bit = wrap::shl(1u, k);
        if (done & bit) return 0;
        raise(bit, m.r8(0x800B17C0u + k));
        break;
    }
    case 4:
        if ((tick & 7u) || (done & 0x200u) || who != m.r8(row + 9)) return 0;
        raise(0x200u, 117);
        break;
    case 5:
        if ((tick & 7u) || (done & 0x400u) || who != m.r8(row + 10)) return 0;
        raise(0x400u, 118);
        break;
    case 6: case 7: case 8: case 9: {
        static constexpr u32 cadence[4] = {7, 3, 15, 7};
        static constexpr u32 bit1[4] = {0x800, 0x2000, 0x8000, 0x20000};
        static constexpr u32 bit4[4] = {0x1000, 0x4000, 0x10000, 0x40000};
        static constexpr u32 call1[4] = {120, 122, 124, 126};
        static constexpr u32 call4[4] = {121, 123, 125, 127};
        const u32 k = kind - 6;
        if (tick & cadence[k]) return 0;
        if (!radio_allowed(who, mission, kind)) return 0;
        if (who == 1 && !(done & bit1[k])) {
            raise(bit1[k], call1[k]);
            break;
        }
        if (who != 4 || (done & bit4[k])) return 0;
        raise(bit4[k], call4[k]);
        break;
    }
    default:
        break;
    }
    m.w8(kRadio + 9, call);
    m.w16(kRadio + 6, arg);
    m.w8(kRadio + 10, who);
    return 1;
}

void Game::entity_destroy(u32 i) {
    cover(0x80038320u);
    const u32 e = slot(i);
    m.w32(e, m.r32(e) & 0xFFFF79FFu);
    const u32 sf = g::kSlotFlags + 4 * i;
    m.w32(sf, m.r32(sf) & 0xFF000000u);
    m.w32(e, m.r32(e) & ~0x10000u);
    m.w32(sf, m.r32(sf) | 4u);
    m.w8(kDeathTimer + i, 150);
}

void Game::death_timers() {
    cover(0x800458D0u);
    for (u32 i = 2; i < 64; ++i) {
        const u32 sf = g::kSlotFlags + 4 * i;
        if (!(m.r8(sf) & 4u)) continue;
        const u32 t = m.r8(kDeathTimer + i);
        if (t != 0) m.w8(kDeathTimer + i, t - 1);
        if (t == 0 || t - 1 == 0) {
            m.w32(sf, m.r32(sf) & ~4u);
            const u32 e = slot(i);
            m.w32(e, m.r32(e) & ~0x4000u);
        }
    }
}

void Game::damage_apply() {
    cover(0x80045A40u);
    Frame frame(*this, 48);
    const u32 scene = m.r32(g::kScene);
    for (u32 i = 2; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 sf = g::kSlotFlags + 4 * i;
        if (!(m.r16(e) & 0x200u) || !(m.r32(sf) & 1u) || m.r16s(e + 78) >= 10000) continue;
        u32 hits = m.r32(e + 84);
        const u32 by = m.r32(e + 80) & 0x3Fu;
        const s32 mission = m.r16s(scene + 908);
        auto explode_sound = [&] { sound_at(mission == 42 ? 88u : i < 32 ? 27u : 29u, e + 8, 0); };
        auto attacker_weapon = [&](u32 field) -> u32 {
            return by >= 32 ? m.r32(m.r32(0x800BCEFCu) + 40 * by - 1264) : m.r32(m.r32(g::kSlotRecords) + 64 * by + field);
        };
        if (hits & 0x3Fu) {
            if ((m.r32(m.r32(g::kWorld) + 4) & 4u) && mission != 13 && (mission != 22 || m.r16(e + 72) != 70)) {
                if (by) {
                    const u32 w = attacker_weapon(32);
                    if (w) m.w16(e + 78, m.r16(e + 78) - m.r16(w + 2));
                } else {
                    u32 dmg = 0;
                    if (hits & 1u) dmg = 10;
                    else if (hits & 2u) dmg = 16;
                    else if (hits & 4u) dmg = 50;
                    else if (hits & 8u) dmg = 18;
                    else if (hits & 0x10u) dmg = 32;
                    else if (hits & 0x20u) dmg = 130;
                    if (!m.r32(scene + 916)) dmg = static_cast<u32>(static_cast<s16>(dmg * 2));
                    m.w16(e + 78, m.r16(e + 78) - dmg);
                }
            }
            if (m.r16s(e + 78) <= 0) explode_sound();
            hits = m.r32(e + 84);
        }
        if (hits & 0x7FC0u) {
            if ((m.r32(m.r32(g::kWorld) + 4) & 4u) &&
                (m.r16(scene + 908) != 22 || m.r16(e + 72) != 70 || (hits & 0x4000u))) {
                if (!by) {
                    const u32 h = m.r32(e + 84);
                    s32 dmg = 0;
                    if (h & 0x40u) dmg = 70;
                    else if (h & 0x80u) dmg = 90;
                    else if (h & 0x100u) dmg = 40;
                    else if (h & 0x200u) dmg = 210;
                    else if ((h & 0x400u) || (h & 0x800u)) dmg = m.r16s(e + 78);
                    else if ((h & 0x1000u) || (h & 0x2000u)) dmg = 70;
                    else if (h & 0x4000u) dmg = m.r16s(e + 78);
                    if (!m.r32(scene + 916)) dmg = static_cast<s16>(dmg * 2);
                    m.w16(e + 78, m.r16(e + 78) - static_cast<u32>(dmg));
                } else {
                    const u32 w = attacker_weapon(28);
                    if (w) m.w16(e + 78, m.r16(e + 78) - m.r16(w + 2));
                }
            }
            if (!(m.r32(e + 84) & 0x4000u)) explode_sound();
        }
        const u32 rec = m.r32(g::kSlotRecords);
        if ((m.r32(e + 84) & 0x8000u) && (m.r32(sf) & 0x800000u))
            radio_call(5, 0, m.r8(m.r32(rec + 64 * i + 16) + 3));
        if ((m.r32(sf) & 0x800000u) && m.r16s(e + 78) > 0 && (m.r32(e + 84) & 0x7FC0u) && (m.r32(e) & 0x80u))
            radio_call(9, 0, m.r8(m.r32(m.r32(g::kSlotRecords) + 64 * by + 16) + 3));
        if (m.r32(e + 84) & 0x7FFFu) {
            if (by < 32 && (m.r32(g::kSlotFlags + 4 * by) & 0x800000u) && m.r16s(e + 78) <= 0)
                radio_call(4, 0, m.r8(m.r32(m.r32(g::kSlotRecords) + 64 * by + 16) + 3));
            const s32 ms = m.r16s(m.r32(g::kScene) + 908);
            if ((ms == 9 || ms == 41 || ms == 43) && (m.r32(e) & 0x40u)) {
                const s32 hp = m.r16s(e + 78);
                if (hp < 21) message_show(8);
                else if (hp < 60) message_show(36);
            }
        }
        m.w32(e + 84, m.r32(e + 84) & 0xFFFF8000u);
    }
}

void Game::link_release(u32 i) {
    cover(0x8004C338u);
    const u32 lf = m.r32(g::kLinkFlags) + 4 * i;
    const u32 v = m.r32(lf);
    if (!(v & 1u)) return;
    m.w32(lf, v & ~1u);
    const u32 link = m.r32(g::kLinks) + 20 * i;
    const u32 a = g::kSlotFlags + 4 * m.r8(link + 2);
    m.w32(a, m.r32(a) & ~0x200000u);
    const u32 b = g::kSlotFlags + 4 * m.r8(link + 3);
    m.w32(b, m.r32(b) & 0xFFBFFFFFu);
}

void Game::blip_setup(u32 i, u32 p) {
    cover(0x8004EBA4u);
    const u32 b = m.r32(0x800C67A0u + 4 * i);
    if (!b) return;
    const u32 e = slot(i);
    if (m.r8(p + 7)) {
        bool write = true;
        u32 v = 3;
        if (m.r8(p + 6) & 2u) {
            if (m.r8(p + 4)) {
                v = 2;
                if (m.r8(p + 4) != 1) write = false;
            } else {
                v = 1;
            }
        }
        if (write) m.w8(b + 2, v);
    } else {
        m.w8(b + 2, 0);
    }
    if (m.r8(p + 5) == 1) m.w8(b + 2, m.r8(b + 2) | 0x80u);
    m.w8(b + 4, m.r8(p + 3));
    m.w8(b + 3, m.r8(e + 72));
    if (m.r32(g::kSlotFlags + 4 * i) & 0x100u) m.w8(b + 5, 1);
    else if (m.r32(e) & 0x40u) m.w8(b + 5, 3);
    else if (m.r32(e) & 0x100u) m.w8(b + 5, 2);
    else m.w8(b + 5, 0);
}

void Game::map_mark_setup(u32 i, u32 p) {
    cover(0x8004ECC4u);
    const u32 r = 0x800C6A60u + 12 * i;
    if (m.r8(p + 5) == 1) m.w8(r, m.r8(r) | 0x10u);
    const u32 k = m.r8(p + 4);
    if (k == 0) m.w8(r + 1, 0);
    else if (k == 1) m.w8(r + 1, 1);
    if (m.r32(g::kSlotFlags + 4 * i) & 0x100u) {
        m.w8(r + 2, 1);
    } else {
        const u32 f = m.r32(slot(i));
        m.w8(r + 2, (f & 0x40u) ? 2u : (f & 0x100u) ? 3u : 0u);
    }
}

void Game::ground_activate(u32 k) {
    cover(0x8004E94Cu);
    const u32 st = kGroundState + 4 * k;
    const u32 rec = m.r32(0x800BCEFCu) + 40 * k;
    const u32 e = m.r32(m.r32(g::kSlots) + 4 * k + 128);
    const u32 type = m.r32(rec + 8), p = m.r32(rec + 12);
    const u32 v6 = m.r32(st);
    m.w32(st, v6 | 1u);
    if (m.r8(type + 28) & 1u) m.w32(st, v6 | 0x101u);
    if (m.r8(type + 28) & 2u) m.w32(st, m.r32(st) | 0x200u);
    if (m.r8(p + 7)) m.w32(st, m.r32(st) | 0x1000u);
    if (m.r8(p + 8)) m.w32(st, m.r32(st) | 0x2000u);
    if (m.r8(p + 6) & 2u) m.w32(st, m.r32(st) | 0x4000u);
    if (m.r8(p + 6) & 1u) m.w32(st, m.r32(st) | 0x8000u);
    if (!m.r8(p + 10)) m.w32(st, m.r32(st) | 0x10u);
    if (m.r8(p + 3) != 255) m.w32(st, m.r32(st) | 0x800000u);
    if (!m.r8(p + 4)) m.w32(st, m.r32(st) | 0x400u);
    if (m.r32(st) & 0x1000u) m.w32(st, m.r32(st) | 0x800u);
    blip_setup(k + 32, p);
    map_mark_setup(k + 32, p);
    const u32 kind = m.r8(p + 9);
    if (kind == 1) m.w32(e, m.r32(e) | 0x800u);
    else if (kind == 2) m.w32(e, m.r32(e) | 0x1000u);
    if (m.r16s(p + 12) == -1) {
        m.w16(e + 74, 0);
    } else {
        m.w16(e + 74, m.r16(p + 12));
        m.w32(e, m.r32(e) | 0x200u);
    }
    if (m.r16s(p + 14) == -1) {
        m.w16(e + 76, 0);
    } else {
        m.w16(e + 76, m.r16(p + 14));
        m.w32(e, m.r32(e) | 0x400u);
    }
}

void Game::links_propagate() {
    cover(0x8004C3C4u);
    for (u32 i = 0; i < m.r8(g::kLinkCount); ++i) {
        if (!(m.r32(m.r32(g::kLinkFlags) + 4 * i) & 1u)) continue;
        const u32 link = m.r32(g::kLinks) + 20 * i;
        const u32 pi = m.r8(link + 2), ci = m.r8(link + 3);
        const u32 parent = slot(pi), child = slot(ci);
        if (m.r16s(parent + 78) <= 0 && m.r16s(child + 78) > 0) {
            if (!(m.r32(g::kSlotFlags + 4 * ci) & 1u) && (m.r32(child) & 0x10000u)) {
                if (m.r8(link + 3) >= 32) ground_activate(ci - 32);
                else entity_destroy(m.r8(link + 3));
            }
            m.w16(child + 78, 0);
            if (m.r16(m.r32(g::kLinks) + 20 * i) == 5) link_release(i);
        }
        if ((m.r32(m.r32(g::kWorld) + 4) & 0xEu) && ci < 32) {
            if ((m.r32(g::kSlotFlags + 4 * pi) & 4u) && !(m.r8(g::kSlotFlags + 4 * ci) & 4u)) entity_destroy(ci);
            if (!(m.r16(parent) & 0x4000u) && (m.r32(child) & 0x4000u)) m.w32(child, m.r32(child) & ~0x4000u);
        }
    }
}

u32 Game::pair_drop_target(u32 i) {
    cover(0x800464B0u);
    if (i == 255) return 255;
    const u32 e = kPairs + 4 * i;
    const u32 t = m.r8(e + 1);
    m.w8(e + 1, 0xFF);
    if (t == 255) return 255;
    const u32 te = kPairs + 4 * t;
    if (m.r8(te + 2) == i) m.w8(te + 2, 0xFF);
    else if (m.r8(te + 3) == i) m.w8(te + 3, 0xFF);
    return 255;
}

void Game::pair_remove(u32 i) {
    cover(0x8004651Cu);
    const u32 e = kPairs + 4 * i;
    const u32 a = m.r8(e + 2);
    m.w8(e, 0);
    m.w8(e + 2, 0xFF);
    if (a != 255) m.w8(kPairs + 4 * a + 1, 0xFF);
    const u32 b = m.r8(e + 3);
    m.w8(e + 3, 0xFF);
    if (b != 255) m.w8(kPairs + 4 * b + 1, 0xFF);
    pair_drop_target(i);
}

void Game::pairs_cleanup() {
    cover(0x80046030u);
    for (u32 i = 2; i < 64; ++i) {
        const u32 e = kPairs + 4 * i;
        const u32 sf = m.r32(g::kSlotFlags + 4 * i);
        if (m.r8(e)) {
            if (!(sf & 1u)) pair_remove(i);
        } else if (sf & 1u) {
            m.w8(e, 0xFF);
        }
    }
    u32 t = m.r8(kPairs + 1);
    if (t == 255) return;
    if (t == m.r8(kPairs + 2)) {
        pair_drop_target(m.r8(kPairs + 2));
        t = m.r8(kPairs + 1);
    }
    if (t == m.r8(kPairs + 3)) pair_drop_target(m.r8(kPairs + 3));
}

void Game::sim_step2() {
    cover(0x80045898u);
    Frame frame(*this, 24);
    death_timers();
    damage_apply();
    links_propagate();
    pairs_cleanup();
}
}
