#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kTarget = 0x800C6D6Eu;
constexpr u32 kShown = 0x800C6D73u;
constexpr u32 kLockedBy = 0x800C6D72u;
constexpr u32 kPairs = 0x800C6D80u;
constexpr u32 kHeldTicks = 0x800BCF98u;
constexpr u32 kAlertSound = 0x800BCFA0u;
constexpr u32 kAlertEar = 0x800C7160u;
constexpr u32 kAlertFlags = 0x800C6ECCu;
constexpr u32 kAlertTick = 0x800C6D60u;
constexpr u32 kAlertLevel = 0x800C6D64u;
constexpr u32 kVoices = 0x800C5830u;
constexpr u32 kRecorder = 0x800BD0B8u;
}

s32 Game::target_score(u32 i) {
    cover(0x8003B2E4u);
    const u32 rel = m.r32(g::kRelation + 4 * i);
    if (!rel || !(m.r32(rel) & 1u)) return static_cast<s32>(i);
    s32 v = static_cast<s32>(i);
    if (m.r32(rel) & 2u) v += 0x400000;
    return v + ((0x8000 - m.r32s(rel + 4)) << 6);
}

u32 Game::target_search(u32 current, u32 prev, u32 need_new) {
    cover(0x8003AFC0u);
    Frame f(*this, 336);
    const u32 scores = f.at(0x10);
    m.w32(f.at(0x154), prev);
    m.w32(f.at(0x158), need_new);
    u32 cur = current & 0xFFu;
    s32 cur_score = 0;
    s32 best_low = 0x800000, best_above = 0x800000, top = 0, below = 0;
    u32 next = 0xFF, before = 0xFF, top_slot = 0xFF, low_slot = 0xFF;
    bool cone = false;
    if (cur != 255) {
        cur_score = target_score(cur);
        cone = cur_score > 0x3FFFFF;
    }
    for (u32 i = 2; i < 64; ++i) {
        const u32 at = scores + 4 * i;
        m.w32(at, 0xFFFFFFFFu);
        if (i == cur) continue;
        const u32 ef = m.r32(slot(i));
        if (!(ef & 0x4000u)) continue;
        const u32 sf = m.r32(g::kSlotFlags + 4 * i);
        if (!(sf & 1u) || !(sf & 0x8000u) || (ef & 0x40u)) continue;
        if ((sf & 0x40000u) && !(sf & 0x100u)) continue;
        if (need_new == 1 && (ef & 0x100u)) continue;
        if (!(m.r32(m.r32(g::kRelation + 4 * i)) & 1u)) continue;
        const s32 sc = target_score(i);
        m.w32(at, static_cast<u32>(sc));
        if (!cone && sc > 0x3FFFFF) {
            cur = 255;
            cone = true;
        }
    }
    for (u32 i = 2; i < 64; ++i) {
        const s32 v = m.r32s(scores + 4 * i);
        if (v == -1 || (cone && !(v > 0x3FFFFF))) continue;
        if (top < v) {
            top = v;
            top_slot = i;
        }
        if (cur == 255) continue;
        if (v < best_low) {
            best_low = v;
            low_slot = i;
        }
        if (cur_score >= v || v >= best_above) {
            if (below < v && v < cur_score) {
                below = v;
                before = i;
            }
        } else {
            best_above = v;
            next = i;
        }
    }
    m.w32(f.at(0x110), static_cast<u32>(below));
    m.w32(f.at(0x114), static_cast<u32>(best_above));
    m.w32(f.at(0x118), static_cast<u32>(top));
    m.w32(f.at(0x11C), static_cast<u32>(best_low));
    m.w8(f.at(0x120), top_slot);
    m.w8(f.at(0x121), low_slot);
    u32 chosen = before;
    if (cur == 255) {
        before = top_slot;
        chosen = top_slot;
    }
    if (chosen == 255) before = top_slot;
    if (next == 255) next = low_slot;
    if (before == 255) before = cur;
    if (next == 255) next = cur;
    return prev ? next : before;
}

u32 Game::pair_add(u32 who, u32 whom) {
    cover(0x800463B8u);
    Frame f(*this, 32);
    if (whom == 255) return whom;
    if (!whom && m.r8(kPairs + 1) == who) return 255;
    const u32 e = kPairs + 4 * whom;
    if (m.r8(e + 2) == 255) {
        m.w8(e + 2, who);
    } else if (m.r8(e + 3) != 255) {
        if (who) return 255;
        pair_drop_target(m.r8(e + 3));
        m.w8(e + 3, 0);
    } else {
        if (!m.r32(m.r32(g::kScene) + 916) && !whom) return 255;
        m.w8(kPairs + 4 * whom + 3, who);
    }
    m.w8(kPairs + 4 * who + 1, whom);
    return whom;
}

void Game::target_switch() {
    cover(0x8003ABECu);
    Frame f(*this, 32);
    const u32 buttons = m.r32(m.r32(g::kScene) + 300);
    u32 prev = 0;
    if (!(buttons & 0x200000u)) {
        if (!(buttons & 0x400000u)) return;
        prev = 1;
    }
    const u32 t = target_search(m.r8(kTarget), prev, 0);
    if (t == 255 || t == m.r8(kTarget)) return;
    m.w8(kTarget, t);
    pair_drop_target(0);
    pair_add(0, t);
}

void Game::target_auto() {
    cover(0x8003AC8Cu);
    Frame f(*this, 24);
    const u32 cur = m.r8(kTarget);
    if (cur != 255) {
        const u32 rel = m.r32(g::kRelation + 4 * cur);
        if ((m.r32(rel) & 2u) || (m.r32(rel + 32) & 0x20000u)) {
            m.w32(kHeldTicks, 0);
            return;
        }
        m.w32(kHeldTicks, m.r32(kHeldTicks) + 1);
    }
    const u32 t = target_search(255, 0, 1);
    if (t == 255 || t == m.r8(kTarget)) return;
    if (m.r8(kTarget) == 255 ||
        (m.r32s(kHeldTicks) >= 31 && m.r16s(m.r32(g::kRelation + 4 * t) + 30) >= 31)) {
        m.w8(kTarget, t);
        pair_drop_target(0);
        pair_add(0, t);
        m.w32(kHeldTicks, 0);
    }
}

void Game::target_update() {
    cover(0x8003AA60u);
    Frame f(*this, 24);
    const u32 cur = m.r8(kTarget);
    if (cur != 255) {
        const u32 sf = m.r32(g::kSlotFlags + 4 * cur);
        if (!(sf & 1u) || !(m.r32(m.r32(g::kRelation + 4 * cur)) & 1u) || (sf & 0x40000u))
            m.w8(kTarget, pair_drop_target(0));
    }
    const u32 wf = m.r32(m.r32(g::kWorld) + 4);
    u32 t;
    if (wf & 4u) {
        if (m.r16(m.r32(g::kScene) + 908) != 3 || m.r32(m.r32(g::kWorld) + 8)) {
            target_switch();
            target_auto();
            return;
        }
        if (m.r8(kTarget) == 2) return;
        m.w8(kTarget, 2);
        pair_drop_target(0);
        t = 2;
    } else if (wf & 0x800u) {
        t = m.r8(0x800B1054u + m.r32(0x800BCEA8u));
        if (t == m.r8(kTarget)) return;
        m.w8(kTarget, t);
        pair_drop_target(0);
    } else {
        m.w8(kTarget, 0xFF);
        pair_drop_target(0);
        t = 255;
    }
    pair_add(0, t);
}

u32 Game::nearest_on_radar() {
    cover(0x8003A4E8u);
    s32 best = 0x1000000;
    u32 who = 255;
    for (u32 i = 2; i < 32; ++i) {
        const u32 rel = m.r32(g::kRelation + 4 * i);
        if ((m.r16(slot(i)) & 0xC000u) == 0xC000u && (m.r32(g::kSlotFlags + 4 * i) & 0x800000u) && rel &&
            m.r32s(rel + 4) < best) {
            best = m.r32s(rel + 4);
            who = i;
        }
    }
    return who;
}

s32 Game::target_shown() {
    cover(0x8003A444u);
    Frame f(*this, 24);
    u32 v = m.r8(kShown);
    if (v == 255) {
        u32 t = m.r8(kTarget);
        bool take = true;
        if (t == 255) {
            if (m.r32(m.r32(g::kScene) + 300) & 0x2000u) t = nearest_on_radar();
            else take = false;
        }
        if (take) v = t;
    }
    if (!(m.r32(m.r32(g::kScene) + 296) & 0x2000u)) v = m.r8(kTarget);
    m.w8(kShown, v);
    return v != 255 ? static_cast<s32>(v) : -1;
}

void Game::alert_init(u32 st, u32 near_sound, u32 far_sound, u32 range, u32 doppler) {
    cover(0x8002ED08u);
    m.w8(st, 0);
    m.w8(st + 1, near_sound);
    m.w8(st + 2, far_sound);
    m.w16(st + 4, range);
    m.w8(st + 3, 0xFF);
    m.w8(st + 6, doppler);
}

u32 Game::sound_play_free(u32 id) {
    cover(0x8002DCB0u);
    Frame f(*this, 32);
    const u32 v = voice_find_free();
    if (v == 255) return 255;
    voice_start(id, v);
    return v;
}

void Game::voice_reset_mix(u32 v) {
    cover(0x8002E67Cu);
    if (v == 255) return;
    const u32 e = kVoices + 20 * v;
    m.w8(e + 1, m.r8(e + 1) | 0x10u);
    m.w32(e + 4, m.r32(e + 8));
    m.w16(e + 12, m.r16(e + 14));
}

u32 Game::alert_sound(u32 ear, u32 st) {
    cover(0x8002ED2Cu);
    Frame f(*this, 24);
    if (!m.r8(st) && m.r32(ear) < static_cast<u32>(static_cast<s32>(m.r16s(st + 4)))) {
        const s32 limit = m.r8(st + 6);
        const s32 doppler = m.r32s(ear + 4);
        if (limit >= doppler) {
            if ((limit >> 1) < doppler) {
                m.w8(st, 1);
                const s32 r = rand();
                m.w8(st + 3, sound_play(static_cast<u32>(r % 3 + 10)));
            }
        } else {
            m.w8(st, 1);
            m.w8(st + 3, sound_play_free(m.r16s(ear + 10) < 0 ? m.r8(st + 2) : m.r8(st + 1)));
        }
        const s32 r = rand();
        sound_detune(m.r8(st + 3), -(r & 0x3FF));
    }
    if (m.r8(st) == 1) {
        const u32 v = m.r8(st + 3);
        if (v == 255) return 255;
        if (!m.r8(kVoices + 20 * v + 3)) {
            m.w8(st + 3, 0xFF);
            m.w8(st, 0);
        }
    }
    return m.r8(st + 3);
}

void Game::missile_alert() {
    cover(0x8003ADB4u);
    Frame f(*this, 64);
    const u32 cfg = m.r32(0x800BCEB4u);
    m.w32(kAlertTick, m.r32(cfg + 20));
    const u32 level = m.r8(cfg + 28);
    m.w32(kAlertFlags, m.r32(kAlertFlags) & ~2u);
    m.w16(kAlertLevel, level);
    const u32 world = m.r32(g::kWorld);
    if (!(m.r8(world + 4) & 4u)) return;
    if (!m.r32(world + 12)) alert_init(kAlertSound, 3, 3, 1024, 16);
    const u32 w = m.r32(g::kWeapons);
    if (!(m.r32(w) & 1u)) return;
    const u32 missile = 164 * m.r8(w + 8) + w + 228;
    const u32 player = slot(0);
    sound_listener(kAlertEar, m.r32(m.r32(g::kWorld) + 40) + 4, missile);
    alert_sound(kAlertEar, kAlertSound);
    const u32 v = m.r8(kAlertSound + 3);
    if (v != 255) {
        voice_reset_mix(v);
        sound_pan(v, kAlertEar);
    }
    const u32 pos = player + 8;
    const s32 d = distance(missile, pos);
    if (d >= 0x2000) return;
    const u32 local = f.at(0x18);
    to_local(missile, pos, local);
    if (m.r32s(local + 8) < 0) return;
    if (!(off_axis_angle(local) < 1536 || d < 2048)) return;
    s32 period = d >> 9;
    m.w32(kAlertFlags, m.r32(kAlertFlags) | 2u);
    if (period < 2) period = 2;
    if (!static_cast<s32>(wrap::div(m.r32s(kAlertTick), period).hi)) voice_for_repeat(44);
}

s32 Game::locked_by() {
    cover(0x8003A598u);
    const u32 v = m.r8(kLockedBy);
    return v == 255 ? -1 : static_cast<s32>(v);
}

bool Game::target_is_lockable() {
    cover(0x8003A5C0u);
    const u32 t = m.r8(kTarget);
    if (t == 255) return false;
    if (m.r16(slot(t) + 72) == 81) return false;
    return !(m.r32(g::kSlotFlags + 4 * t) & 0x400u);
}

void Game::targeting_update(u32 args) {
    cover(0x8003A23Cu);
    Frame f(*this, 32);
    target_update();
    missile_alert();
    m.w32(args + 16, static_cast<u32>(target_shown()));
    m.w32(args + 20, static_cast<u32>(locked_by()));
    m.w32(g::kNearAngle, m.r32(args + 4));
    m.w32(g::kNearDist, target_is_lockable() ? m.r32(args + 12) : m.r32(args + 8));
}

u32 Game::record_word(u32 kind, s32 slot_or_value, u32 a, u32 b) {
    cover(0x8007DFC4u);
    Frame f(*this, 48);
    const u32 r = recorder_alloc(1);
    auto head = [&](u32 v) {
        v = ((v & 0xFFFFFFF0u) | 1u) & 0xFFFFFF8Fu;
        v &= 0xFFFFFC7Fu;
        return v | ((kind & 7u) << 7);
    };
    const u32 s = static_cast<u32>(slot_or_value);
    switch (kind) {
    case 0: m.w32(r, 1); return 1;
    case 1: {
        u32 v = head(m.r32(r)) & 0xFFFF03FFu;
        v |= (s & 0x3Fu) << 10;
        v = (v & 0xFFFEFFFFu) | ((a & 1u) << 16);
        m.w32(r, v & 0x1FFFFu);
        return v & 0x1FFFFu;
    }
    case 2: {
        u32 v = head(m.r32(r)) & 0xFFFF03FFu;
        const u32 flags = v | ((s & 0x3Fu) << 10);
        m.w32(r, flags);
        m.w8(r + 2, a);
        m.w8(r + 3, b);
        return flags;
    }
    case 3: {
        u32 v = head(m.r32(r)) & 0xFFFF03FFu;
        v |= (s & 0x3Fu) << 10;
        const u32 flags = (v & 0xFF81u) | ((a & 0x7FFFu) << 16);
        m.w32(r, flags);
        return flags;
    }
    case 4: {
        const u32 v = head(m.r32(r));
        m.w32(r, v);
        const u32 k = slot_or_value < 0 ? 0u : s;
        m.w32(r, (v & 0xFFFF03FFu) | ((k & 0x3Fu) << 10));
        m.w16(r + 2, 0);
        return (k & 0x3Fu) << 10;
    }
    default: return 0x80020000u;
    }
}

void Game::record_view_subject(s32 i) {
    cover(0x8007E890u);
    Frame f(*this, 24);
    m.w32(m.r32(kRecorder) + 6220, static_cast<u32>(i));
    record_word(4, i, 0, 0);
}

u32 Game::entity_changes(u32 old, u32 now) {
    cover(0x8007E5BCu);
    u32 v = ((m.r32(now) ^ m.r32(old)) & 0x14000u) ? 1u : 0u;
    if (m.r16s(old + 4) != m.r16s(now + 4)) v |= 4u;
    if ((m.r8(now + 5) & 4u) && m.r16s(old + 6) != m.r16s(now + 6)) v |= 8u;
    for (u32 k = 8; k <= 16; k += 4)
        if (m.r32s(old + k) >> 12 != m.r32s(now + k) >> 12) v |= 0x10u;
    for (u32 k = 24; k <= 28; k += 2)
        if (m.r16s(old + k) >> 2 != m.r16s(now + k) >> 2) v |= 0x10u;
    return v;
}

void Game::record_move(u32 kind, u32 i, u32 pos) {
    cover(0x8007E1CCu);
    Frame f(*this, 64);
    const u32 r = recorder_alloc(4);
    if (kind) return;
    m.w32(r, (m.r32(r) & 0xFFFF8000u) | 0x14u | ((i & 0x3Fu) << 9));
    const u32 y = ((static_cast<u32>(m.r16(pos + 18)) >> 2) & 0xFFFu) | static_cast<u32>((m.r32s(pos + 4) >> 12) << 12);
    const s32 z = m.r32s(pos + 8);
    const u32 az = (static_cast<u32>(m.r16(pos + 20)) >> 2) & 0xFFFu;
    m.w32(r + 4, ((static_cast<u32>(m.r16(pos + 16)) >> 2) & 0xFFFu) | static_cast<u32>((m.r32s(pos) >> 12) << 12));
    m.w32(r + 8, y);
    m.w32(r + 12, az | static_cast<u32>((z >> 12) << 12));
}

void Game::record_remember(u32 e, u32 to) {
    cover(0x8007E6D8u);
    const u32 f0 = m.r32(e);
    if (!(m.r16(e) & 0x4000u)) {
        m.w32(to, f0);
        return;
    }
    m.w32(to, f0);
    for (u32 k = 4; k < 20; k += 4) m.w32(to + k, m.r32(e + k));
}

void Game::record_entities() {
    cover(0x8007E720u);
    Frame f(*this, 48);
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        const u32 last = m.r32(kRecorder) + 2072 + 32 * i;
        const s32 subject = m.r32s(m.r32(kRecorder) + 6220);
        const bool is_subject = i == (static_cast<u32>(subject) & 0x3Fu) && subject >= 0;
        if ((m.r32(last) & 0x14000u) != 0x14000u && (m.r32(e) & 0x14000u) != 0x14000u && !is_subject) {
            record_remember(e, last);
            continue;
        }
        const u32 ch = entity_changes(last, e);
        if (!(m.r16(e) & 0x2000u) && !(ch & 1u)) continue;
        if (ch & 1u) record_word(1, static_cast<s32>(i), (m.r32(e) & 0x14000u) == 0x14000u ? 1u : 0u, 0);
        if (ch & 4u) record_word(2, static_cast<s32>(i), m.r8(e + 4), m.r8(e + 5));
        if (ch & 8u) record_word(3, static_cast<s32>(i), m.r16(e + 6), 0);
        if (ch & 0x10u) record_move(0, i, e + 8);
        record_remember(e, last);
    }
}

void Game::record_tick() {
    cover(0x8007F0F8u);
    Frame f(*this, 40);
    const u32 r = m.r32(kRecorder);
    if (!m.r32(r)) {
        const u32 start = m.r32(r + 4);
        m.w32(r + 16, start);
        m.w32(r + 12, start);
        for (u32 i = 0; i < 64; ++i) {
            const u32 e = slot(i);
            for (u32 k = 0; k < 32; k += 4) m.w32(m.r32(kRecorder) + 24 + 32 * i + k, m.r32(e + k));
            for (u32 k = 0; k < 32; k += 4) m.w32(m.r32(kRecorder) + 2072 + 32 * i + k, m.r32(e + k));
        }
    }
    record_entities();
    record_word(0, 0, 0, 0);
    m.w32(m.r32(kRecorder), m.r32(m.r32(kRecorder)) + 1);
}
}
