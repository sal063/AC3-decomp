#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kEngines = 0x800C6610u;
constexpr u32 kEngineState = 0x800C6630u;
constexpr u32 kSoundSets = 0x800B0900u;
constexpr u32 kSoundSetByKind = 0x800B0964u;
constexpr u32 kMsgState = 0x800C7194u;
constexpr u32 kMsgDelay = 0x800C7196u;
constexpr u32 kMsgVoice = 0x800C7198u;
constexpr u32 kMsgSound = 0x800C7199u;
constexpr u32 kMsgText = 0x800C719Au;
constexpr u32 kMsgTexts = 0x800B17E8u;
constexpr u32 kSubtitle = 0x800C7170u;

u32 voice_at(u32 v) { return g::kVoices + 20 * v; }
s32 sin_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 2 * static_cast<u32>(i & 0x3FFF)); }
s32 cos_at(Memory& m, s32 i) { return m.r16s(g::kSinCos + 0x2000 + 2 * static_cast<u32>(i & 0x3FFF)); }
}

void Game::ai_engine_sounds() {
    cover(0x8004833Cu);
    Frame f(*this, 64);
    const u32 cand = f.at(0x10), out = f.at(0x18);
    s32 c1 = -1, c2 = -1, d1 = 0x8000, d2 = 0x8000;
    m.w32(cand, 0xFFFFFFFFu);
    m.w32(cand + 4, 0xFFFFFFFFu);
    for (u32 i = 2; i < 32; ++i) {
        const u32 sf = m.r32(g::kSlotFlags + 4 * i);
        if (!(sf & 1u) || (sf & 0x400000u)) continue;
        s32 d = 0x8000;
        if (m.r32(m.r32(g::kRelation + 4 * i) + 32) & 0x10000u)
            d = distance(m.r32(m.r32(g::kWorld) + 40) + 4, slot(i) + 8);
        if (d >= d1) {
            if (d < d2) {
                d2 = d;
                c2 = static_cast<s32>(i);
            }
        } else {
            const s32 old = c1;
            d2 = d1;
            d1 = d;
            c1 = static_cast<s32>(i);
            c2 = old;
        }
        if (d1 == 0x8000) c1 = -1;
        if (d2 == 0x8000) c2 = -1;
    }
    m.w32(cand, static_cast<u32>(c1));
    m.w32(cand + 4, static_cast<u32>(c2));
    m.w32(f.at(0x20), static_cast<u32>(d1));
    m.w32(f.at(0x24), static_cast<u32>(d2));
    engine_assign(cand, out);
    for (u32 k = 0; k < 2; ++k) {
        const u32 st = kEngineState + 92 * k;
        const s32 s = m.r32s(st + 8);
        if (s == -1) continue;
        const u32 e = slot(static_cast<u32>(s));
        for (u32 w = 0; w < 24; w += 4) m.w32(st + 12 + w, m.r32(e + 8 + w));
    }
    engine_mix(m.r32(m.r32(g::kWorld) + 40) + 4);
}

void Game::engine_assign(u32 cand, u32 out) {
    cover(0x8002F770u);
    Frame f(*this, 56);
    u32 used = 0, taken = 0;
    for (s32 k = 0; k < m.r32s(kEngines); ++k) {
        if ((used & 0xFFu) >> k & 1u) continue;
        const u32 st = kEngineState + 92 * static_cast<u32>(k);
        for (s32 j = 0; j < m.r32s(kEngines); ++j) {
            if (((taken & 0xFFu) >> j & 1u) || m.r32(st + 8) != m.r32(cand + 4 * static_cast<u32>(j))) continue;
            m.w32(out + 4 * static_cast<u32>(j), static_cast<u32>(k));
            used |= 1u << k;
            taken |= 1u << j;
            break;
        }
    }
    for (s32 k = 0; k < m.r32s(kEngines); ++k) {
        if ((used & 0xFFu) >> k & 1u) continue;
        const u32 st = kEngineState + 92 * static_cast<u32>(k);
        s32 j = 0;
        while (j < m.r32s(kEngines) && ((taken & 0xFFu) >> j & 1u)) ++j;
        if (j >= m.r32s(kEngines)) continue;
        const u32 s = m.r32(cand + 4 * static_cast<u32>(j));
        m.w32(st + 8, s);
        if (s == 0xFFFFFFFFu) {
            m.w32(st, m.r32(st) | 2u);
            fade_reset(st + 36);
        } else {
            m.w32(st, m.r32(st) | 1u);
            engine_set_kind(static_cast<u32>(k), s);
        }
        m.w32(out + 4 * static_cast<u32>(j), static_cast<u32>(k));
        used |= 1u << k;
        taken |= 1u << j;
    }
}

void Game::engine_set_kind(u32 k, u32 s) {
    cover(0x8002F70Cu);
    const u32 kind = m.r16(slot(s) + 72);
    if (kind < 0x30u) m.w32(kEngineState + 92 * k + 4, m.r8(kSoundSetByKind + (kind & 0x3Fu)));
}

void Game::engine_mix(u32 ear) {
    cover(0x8002F9ACu);
    Frame f(*this, 88);
    for (s32 k = 0; k < m.r32s(kEngines); ++k) {
        const u32 ku = static_cast<u32>(k);
        const u32 st = kEngineState + 92 * ku;
        const u32 set = kSoundSets + 20 * m.r32(st + 4);
        const u32 src = st + 12, buf = st + 36, whoosh = st + 48, mod_a = st + 56, mod_b = st + 68;
        const u32 va = m.r8(kEngines + 4 + ku), vb = m.r8(kEngines + 8 + ku), vc = m.r8(kEngines + 12 + ku);
        if (va == 255 || vb == 255 || vc == 255) continue;
        const u32 fl = m.r32(st);
        if (fl & 1u) {
            m.w32(st, fl & ~1u);
            voice_start_on(m.r8(set), va);
            voice_start_on(m.r8(set + 1), vb);
            voice_start_on(m.r8(set + 2), vc);
            for (u32 v : {va, vb, vc})
                m.w16(voice_at(v) + 14, static_cast<u32>(pitch_add(m.r16(voice_at(v) + 14), static_cast<s32>(16 * v))));
            alert_init(whoosh, m.r8(set + 3), m.r8(set + 4), m.r16(set + 6), m.r8(set + 5));
            mod_init(mod_a, va);
            mod_volume(mod_a, m.r8(set + 8), m.r8(set + 12));
            mod_pitch(mod_a, m.r8(set + 9), m.r8(set + 14));
            mod_init(mod_b, vc);
            mod_volume(mod_b, m.r8(set + 10), m.r8(set + 16));
            mod_pitch(mod_b, m.r8(set + 11), m.r8(set + 18));
        } else if (fl & 2u) {
            m.w32(st, fl & ~2u);
            voice_volume(va, 0, 0);
            voice_volume(vb, 0, 0);
            voice_volume(vc, 0, 0);
            continue;
        }
        if (m.r32(st + 8) == 0xFFFFFFFFu) {
            voice_volume(va, 0, 0);
            voice_volume(vb, 0, 0);
            voice_volume(vc, 0, 0);
            continue;
        }
        sound_listener(buf, ear, src);
        voice_volume(vc, 4096, 4096);
        modulate(mod_b);
        sound_attenuate(vc, buf, 4);
        sound_pan(vc, buf);
        voice_reset_mix(va);
        modulate(mod_a);
        sound_attenuate(va, buf, 2);
        sound_pan(va, buf);
        sound_height(va, buf);
        stereo_split(va, vb, buf);
        const bool quiet = m.r32(m.r32(g::kWorld) + 4) & 0x20u;
        s32 step = 512;
        bool scale = quiet;
        if (m.r32s(src + 4) < -12582912) {
            alert_sound(buf, whoosh);
            if (m.r8(whoosh) == 1) {
                const u32 w = m.r8(whoosh + 3);
                voice_reset_mix(w);
                sound_pan(w, buf);
                sound_height(w, buf);
                if (quiet) {
                    const u32 wv = voice_at(w);
                    for (u32 o : {4u, 6u}) m.w16(wv + o, static_cast<u32>(m.r16s(wv + o) / 2));
                }
            }
        }
        if (scale) {
            for (u32 v : {va, vb, vc}) {
                const u32 at = voice_at(v);
                const s32 right = static_cast<s16>(30 * m.r16(at + 6));
                m.w16(at + 4, static_cast<u32>(static_cast<s16>(30 * m.r16(at + 4)) / 100));
                m.w16(at + 6, static_cast<u32>(right / 100));
            }
            step = 2048;
        }
        vol_slew(va, voice_at(va) + 4, st + 80, step);
        vol_slew(vb, voice_at(vb) + 4, st + 84, step);
        vol_slew(vc, voice_at(vc) + 4, st + 88, step);
        m.w32(st + 80, m.r32(voice_at(va) + 4));
        m.w32(st + 84, m.r32(voice_at(vb) + 4));
        m.w32(st + 88, m.r32(voice_at(vc) + 4));
    }
}

u32 Game::voice_start_on(u32 id, u32 v) {
    cover(0x8002DC7Cu);
    Frame f(*this, 24);
    if (v == 255) return 255;
    voice_start(id, v);
    return v;
}

void Game::vol_slew(u32 v, u32 now, u32 prev, s32 step) {
    cover(0x800300E0u);
    Frame f(*this, 24);
    const s32 pl = m.r16s(prev);
    s32 dl = m.r16s(now) - pl;
    if (dl < -step) dl = -step;
    if (step < dl) dl = step;
    const s32 pr = m.r16s(prev + 2);
    s32 dr = m.r16s(now + 2) - pr;
    if (dr < -step) dr = -step;
    if (step < dr) dr = step;
    voice_volume(v & 0xFFu, static_cast<s16>(dl + pl), static_cast<s16>(dr + pr));
}

u32 Game::stereo_split(u32 a, u32 b, u32 buf) {
    cover(0x8002EA74u);
    if (a == 255 || b == 255) return 255;
    const u32 pa = voice_at(a), pb = voice_at(b);
    m.w8(pa + 1, m.r8(pa + 1) | 0x10u);
    const s32 bl = m.r16s(pb + 4);
    m.w8(pb + 1, m.r8(pb + 1) | 0x10u);
    m.w16(pb + 12, m.r16(pa + 12));
    u32 inv = bl >= 0 ? (m.r16(pb + 6) >> 14) & 2u : 1u;
    const s32 side = m.r16s(buf + 10);
    if (side >= 1025) {
        m.w16(pb + 4, 0);
        m.w16(pb + 6, 0);
    } else if (side < -1024) {
        m.w16(pb + 4, m.r16(pa + 4));
        m.w16(pb + 6, m.r16(pa + 6));
        m.w16(pa + 4, 0);
        m.w16(pa + 6, 0);
    } else {
        const s32 k = (2 * (side + 1024)) & 0x3FFF;
        const s32 c = cos_at(m, k);
        m.w16(pb + 4, static_cast<u32>((m.r16s(pa + 4) * c) >> 14));
        const s32 s = sin_at(m, k);
        m.w16(pb + 6, static_cast<u32>((m.r16s(pa + 6) * c) >> 14));
        const s32 r = m.r16s(pa + 6) * s;
        m.w16(pa + 4, static_cast<u32>((m.r16s(pa + 4) * s) >> 14));
        m.w16(pa + 6, static_cast<u32>(r >> 14));
    }
    inv &= 0xFFu;
    if (inv == 1) {
        m.w16(pb + 4, 0u - m.r16(pb + 4));
    } else if (inv == 2) {
        m.w16(pb + 6, 0u - m.r16(pb + 6));
    } else if (m.r16s(pb + 6) >= m.r16s(pb + 4)) {
        m.w16(pb + 4, 0u - m.r16(pb + 4));
    } else {
        m.w16(pb + 6, 0u - m.r16(pb + 6));
    }
    return a;
}

void Game::mod_init(u32 st, u32 v) {
    cover(0x8002EE7Cu);
    m.w8(st, 0);
    m.w8(st + 1, v);
}

void Game::mod_volume(u32 st, u32 period, u32 depth) {
    cover(0x8002EE88u);
    const u32 fl = m.r8(st);
    m.w8(st + 2, period);
    m.w16(st + 4, depth & 0xFFu);
    m.w16(st + 8, 0);
    m.w8(st, fl | 1u);
}

void Game::mod_pitch(u32 st, u32 period, u32 depth) {
    cover(0x8002EEA8u);
    const u32 fl = m.r8(st);
    m.w8(st + 3, period);
    m.w16(st + 6, depth & 0xFFu);
    m.w16(st + 10, 0);
    m.w8(st, fl | 2u);
}

u32 Game::modulate(u32 st) {
    cover(0x8002EEC8u);
    Frame f(*this, 32);
    const u32 v = m.r8(st + 1);
    if (v == 255) return 255;
    const u32 at = voice_at(v);
    if (m.r8(st) & 1u) {
        const s32 period = m.r8(st + 2);
        const s32 k = wrap::div(m.r16s(st + 8) << 14, period).lo & 0x3FFF;
        const s32 d = (sin_at(m, k) * m.r16s(st + 4)) >> 14;
        m.w16(at + 4, m.r16(at + 4) + static_cast<u32>(d));
        m.w16(at + 6, m.r16(at + 6) + static_cast<u32>(d));
        for (u32 o : {4u, 6u})
            if (m.r16s(at + o) >= 0x4000) m.w16(at + o, 0x3FFF);
        for (u32 o : {4u, 6u})
            if (m.r16s(at + o) < 0) m.w16(at + o, 0);
        const u32 ph = m.r16(st + 8) + 1u;
        m.w16(st + 8, ph);
        if (static_cast<s16>(ph) >= period) m.w16(st + 8, 0);
    }
    if (m.r8(st) & 2u) {
        const s32 period = m.r8(st + 3);
        const s32 k = wrap::div(m.r16s(st + 10) << 14, period).lo & 0x3FFF;
        m.w16(at + 12, static_cast<u32>(pitch_add(m.r16(at + 12), (sin_at(m, k) * m.r16s(st + 6)) >> 14)));
        const u32 ph = m.r16(st + 10) + 1u;
        m.w16(st + 10, ph);
        if (static_cast<s16>(ph) >= period) m.w16(st + 10, 0);
    }
    return m.r8(st + 1);
}

void Game::radio_message_update() {
    cover(0x80049438u);
    Frame f(*this, 24);
    auto skip = [&] { return radio_busy() != 0; };
    if (m.r16s(kMsgState) == 1 && m.r16s(kMsgDelay) >= 0 && (skip() || (player_mode() & 0x7000u)))
        m.w16(kMsgDelay, 0xFFFF);
    if (m.r16s(kMsgState) == 2) {
        s32 how = -1;
        if (skip()) {
            voice_release(m.r8(kMsgVoice));
            how = 0;
        } else if (!voice_playing(m.r8(kMsgVoice)) || m.r8(kMsgVoice) == 255) {
            how = 1;
        }
        if (how >= 0) {
            subtitle_close(static_cast<u32>(how));
            m.w16(kMsgState, 1);
            m.w16(kMsgDelay, 0xFFFF);
            m.w8(kMsgVoice, 255);
        }
    }
    if (!m.r16(kMsgDelay) && (m.r32(m.r32(g::kWorld) + 4) & 4u) && m.r16s(kMsgState) == 1) {
        m.w8(kMsgVoice, sound_play(m.r8(kMsgSound)));
        subtitle_open(m.r8(kMsgTexts + m.r8(kMsgText)), 268, 30);
        m.w16(kMsgState, 2);
        m.w16(kMsgDelay, 0xFFFF);
    }
    if (m.r16s(kMsgDelay) == 12 && m.r16s(kMsgState) == 1 && !skip()) sound_play(53);
    if (m.r16s(kMsgDelay) >= 0) m.w16(kMsgDelay, m.r16(kMsgDelay) - 1u);
}

u32 Game::voice_playing(u32 v) {
    cover(0x8002DD7Cu);
    if (v == 255) return 255;
    return m.r8(voice_at(v) + 3);
}

void Game::subtitle_open(u32 line, u32 x, u32 y) {
    cover(0x80041920u);
    Frame f(*this, 24);
    if (!line) {
        subtitle_close(1);
        return;
    }
    const u32 k = line - 1;
    if (m.r8(kSubtitle + 3) == k) return;
    m.w8(kSubtitle + 2, 1);
    m.w16(kSubtitle, 0);
    m.w16(kSubtitle + 8, x);
    m.w16(kSubtitle + 10, y);
    m.w8(kSubtitle + 3, k);
}

void Game::subtitle_close(u32 how) {
    cover(0x8004197Cu);
    m.w16(kSubtitle, 0);
    if (!m.r8(kSubtitle + 2)) return;
    m.w8(kSubtitle + 2, how == 1 ? 3 : 0);
    m.w8(kSubtitle + 3, 255);
}
}
