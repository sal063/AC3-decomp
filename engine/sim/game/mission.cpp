#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSavedHp = 0x800BD018u;
constexpr u32 kBlips = 0x800C67A0u;
constexpr u32 kMarks = 0x800C6A60u;
constexpr u32 kSoundQueue = 0x800C6570u;
constexpr u32 kRadioQueue = 0x800BD020u;
constexpr u32 kMusicTable = 0x800B2334u;
constexpr u32 kCdState = 0x800BCBBCu;

u32 sflag(u32 k) { return g::kSlotFlags + 4 * k; }
u32 ms_base(Memory& m) { return m.r32(g::kMission); }
}

s32 Game::msn_s16(u32 pc) {
    cover(0x80056148u);
    return static_cast<s16>(m.r8(pc) | (m.r8(pc + 1) << 8));
}

u32 Game::msn_u32(u32 pc) {
    cover(0x80056164u);
    return m.r8(pc) | (m.r8(pc + 1) << 8) | (m.r8(pc + 2) << 16) | (static_cast<u32>(m.r8(pc + 3)) << 24);
}

u32 Game::msn_if_less(u32 pc, s32 value) {
    cover(0x80055C4Cu);
    Frame f(*this, 32);
    const s32 v = static_cast<s32>(msn_u32(pc));
    pc += 4;
    const u32 end = pc + m.r8(pc);
    const u32 p = pc + 1;
    msn_vm_exec(v < value ? p + m.r8(p) : p + 1);
    return end;
}

u32 Game::msn_if_score(u32 pc) {
    cover(0x80055CBCu);
    Frame f(*this, 32);
    const s32 v = static_cast<s32>(msn_u32(pc));
    pc += 4;
    const u32 end = pc + m.r8(pc);
    const u32 p = pc + 1;
    msn_vm_exec(m.r32s(ms_base(m) + 4) < v ? p + m.r8(p) : p + 1);
    return end;
}

u32 Game::msn_if_counter(u32 pc, u32 addr, u32 fn, bool counter_first) {
    cover(fn);
    Frame f(*this, 24);
    const u32 end = pc + m.r8(pc + 1) + 1;
    const u32 c = m.r8(ms_base(m) + addr);
    const bool take = counter_first ? c < m.r8(pc) : m.r8(pc) < c;
    msn_vm_exec(take ? pc + m.r8(pc + 2) + 2 : pc + 3);
    return end;
}

u32 Game::msn_if_flag47(u32 pc) {
    cover(0x80055E64u);
    Frame f(*this, 32);
    msn_u32(pc);
    pc += 4;
    const u32 end = pc + m.r8(pc);
    const u32 p = pc + 1;
    msn_vm_exec(m.r8(ms_base(m) + 47) == 1 ? p + 1 : p + m.r8(p));
    return end;
}

u32 Game::msn_if_kills(u32 pc) {
    cover(0x80055ED8u);
    Frame f(*this, 24);
    const u32 end = pc + m.r8(pc);
    const u32 ms = ms_base(m);
    msn_vm_exec(m.r8(ms + 45) >= m.r8(ms + 46) ? pc + m.r8(pc + 1) + 1 : pc + 2);
    return end;
}

u32 Game::msn_if_scene_flag(u32 pc) {
    cover(0x80055F3Cu);
    Frame f(*this, 24);
    const u32 end = pc + m.r8(pc + 1) + 1;
    msn_vm_exec(m.r8(m.r8(pc) + m.r32(g::kScene) + 3340) ? pc + 3 : pc + m.r8(pc + 2) + 2);
    return end;
}

u32 Game::msn_if_random(u32 pc) {
    cover(0x80055FA4u);
    Frame f(*this, 32);
    const u32 end = pc + m.r8(pc);
    const u32 p = pc + 1;
    const s32 r = rand();
    msn_vm_exec((r & 1) ? p + 1 : p + m.r8(p));
    return end;
}

u32 Game::msn_if_hp(u32 pc) {
    cover(0x80056004u);
    Frame f(*this, 24);
    const u32 end = pc + m.r8(pc + 2) + 2;
    const bool take = m.r16s(slot(m.r8(pc)) + 78) < static_cast<s32>(m.r8(pc + 1));
    msn_vm_exec(take ? pc + m.r8(pc + 3) + 3 : pc + 4);
    return end;
}

u32 Game::msn_if_equal(u32 pc, u32 value, u32 fn) {
    cover(fn);
    Frame f(*this, 24);
    const u32 end = pc + m.r8(pc + 1) + 1;
    msn_vm_exec(m.r8(pc) == value ? pc + 3 : pc + m.r8(pc + 2) + 2);
    return end;
}

void Game::air_show(u32 k) {
    cover(0x8004E0FCu);
    const u32 e = slot(k);
    m.w32(e, m.r32(e) | 0x10000u);
}

void Game::ground_show(u32 k) {
    cover(0x8004E900u);
    const u32 e = slot(k + 32);
    if (m.r8(m.r32(m.r32(g::kGroundRecords) + 40 * k + 8) + 2) != 255) m.w32(e, m.r32(e) | 0x10000u);
}

void Game::air_activate(u32 k) {
    cover(0x8004E124u);
    Frame f(*this, 32);
    const u32 rec = m.r32(g::kSlotRecords) + (k << 6);
    const u32 fl = sflag(k);
    const u32 e = slot(k);
    const u32 def = m.r32(rec + 12);
    const u32 old = m.r32(fl);
    const u32 pl = m.r32(rec + 16);
    m.w32(fl, old | 1u);
    if (m.r8(def + 28) & 1u) m.w32(fl, old | 0x101u);
    if (m.r8(def + 28) & 2u) m.w32(fl, m.r32(fl) | 0x200u);
    if (m.r8(def + 28) & 4u) m.w32(fl, m.r32(fl) | 0x80000u);
    if (m.r8(pl + 7)) m.w32(fl, m.r32(fl) | 0x1000u);
    if (m.r8(pl + 8)) m.w32(fl, m.r32(fl) | 0x2000u);
    if (m.r8(pl + 6) & 2u) m.w32(fl, m.r32(fl) | 0x4000u);
    if (m.r8(pl + 6) & 1u) m.w32(fl, m.r32(fl) | 0x8000u);
    if (!m.r8(pl + 10)) m.w32(fl, m.r32(fl) | 0x10u);
    if (m.r8(pl + 3) != 255) m.w32(fl, m.r32(fl) | 0x800000u);
    if (!m.r8(pl + 4)) m.w32(fl, m.r32(fl) | 0x400u);
    if ((m.r32(fl) & 0x1000u) && m.r16(e + 72)) m.w32(fl, m.r32(fl) | 0x800u);
    blip_setup(k, pl);
    map_mark_setup(k, pl);
    const u32 kind = m.r8(pl + 9);
    if (kind == 1) m.w32(e, m.r32(e) | 0x800u);
    else if (kind == 2) m.w32(e, m.r32(e) | 0x1000u);
    if (m.r16s(pl + 12) == -1) {
        m.w16(e + 74, 0);
    } else {
        const u32 v = m.r32(e);
        m.w16(e + 74, m.r16(pl + 12));
        m.w32(e, v | 0x200u);
    }
    if (m.r16s(pl + 14) == -1) {
        m.w16(e + 76, 0);
    } else {
        const u32 v = m.r32(e);
        m.w16(e + 76, m.r16(pl + 14));
        m.w32(e, v | 0x400u);
    }
}

void Game::link_engage(u32 k) {
    cover(0x8004C2C8u);
    const u32 lf = m.r32(g::kLinkFlags) + 4 * k;
    m.w32(lf, m.r32(lf) | 1u);
    const u32 l = m.r32(g::kLinks) + 20 * k;
    m.w32(sflag(m.r8(l + 2)), m.r32(sflag(m.r8(l + 2))) | 0x200000u);
    m.w32(sflag(m.r8(l + 3)), m.r32(sflag(m.r8(l + 3))) | 0x400000u);
}

void Game::radio_queue(u32 kind, s32 delay) {
    cover(0x80056234u);
    for (u32 k = 0; k < 4; ++k) {
        if (m.r8(m.r32(kRadioQueue) + k + 20) != 255) continue;
        m.w8(m.r32(kRadioQueue) + k + 20, kind);
        const u32 q = m.r32(kRadioQueue) + 2 * k;
        m.w16(q + 12, static_cast<u32>(delay));
        m.w16(q + 4, 0);
        return;
    }
}

void Game::sound_queue_push(u32 id, u32 arg) {
    cover(0x8002F294u);
    const u32 n = m.r8(0x800BCE99u);
    const u32 p = kSoundQueue + 4 * n;
    m.w16(p + 2, id);
    m.w8(p, arg);
    m.w8(0x800BCE99u, (m.r8(0x800BCE99u) + 1) & 0x1Fu);
}

u32 Game::cd_current() {
    cover(0x8002CAD0u);
    return m.r32(m.r32(kCdState) + 0x174);
}

void Game::music_play(u32 k) {
    cover(0x8005697Cu);
    Frame f(*this, 32);
    if (k == 255) {
        cd_cancel_streams();
        return;
    }
    const u32 p = m.r32(kMusicTable + 4 * k);
    if ((m.r16(p) & 0x7FFu) == (cd_current() & 0x7FFu)) return;
    cd_cancel_streams();
    cd_request_stream(m.r32(kMusicTable + 4 * k), 0, 0);
}

void Game::world_flag4(u32 on) {
    cover(0x8005F0E4u);
    const u32 w = m.r32(g::kWorld);
    m.w32(w, on ? (m.r32(w) | 4u) : (m.r32(w) & ~4u));
}

void Game::player_view_angles(u32 e, u32 st) {
    cover(0x8006EE38u);
    Frame f(*this, 0x78);
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

void Game::player_place(u32 params) {
    cover(0x80072780u);
    Frame f(*this, 0x58);
    const u32 rot = f.at(0x18), v = f.at(0x10), out = f.at(0x38);
    const u32 e = slot(0);
    const u32 st = m.r32(g::kPlayer);
    const u32 c88 = m.r32(0x800BD088u), c8c = m.r32(0x800BD08Cu), c90 = m.r32(0x800BD090u);
    m.w32(e, m.r32(e) | 0x1C000u);
    for (u32 o : {0x30u, 0x32u, 0x34u, 0x38u, 0x3Au, 0x3Cu}) m.w16(st + o, 0);
    m.w16(st + 0x4C, 0x1000);
    for (u32 o : {4u, 6u, 8u, 0xAu, 0xCu}) m.w16(c8c + o, 0);
    m.w16(c8c + 0xE, 0x1000);
    for (u32 o : {0u, 2u, 4u, 6u, 8u}) m.w16(c90 + o, 0);
    m.w32(c90 + 0xC, 0);
    for (u32 o : {0u, 2u, 4u, 6u}) m.w16(c88 + o, 0);
    for (u32 k = 0; k < 16; k += 4) m.w32(e + 8 + k, m.r32(params + k));
    m.w32(e + 0x18, m.r32(params + 0x10));
    m.w32(e + 0x1C, m.r32(params + 0x14));
    m.w32(st + 0x48, 0x21000);
    m.w16(v, 0);
    m.w16(v + 2, 0);
    m.w16(v + 4, 0x1080);
    rot_matrix_yxz(e + 0x18, rot);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w32(out, gte.mfc2(25));
    m.w32(out + 4, gte.mfc2(26));
    m.w32(out + 8, gte.mfc2(27));
    for (u32 k = 0; k < 12; k += 4) m.w32(out + k, m.r32(out + k) << 5);
    m.w32(e + 0x38, m.r32(out));
    m.w32(e + 0x3C, m.r32(out + 4));
    m.w32(e + 0x40, m.r32(out + 8));
    m.w32(m.r32(g::kPlayer) + 0x10, 0);
    m.w32(m.r32(0x800BD098u) + 0x24, 0);
    player_view_angles(e, st);
}

void Game::mission_log_kill(u32 k) {
    cover(0x80048758u);
    const u32 sc = m.r32(g::kScene);
    const u32 lg = sc + 952;
    const u32 e = slot(k);
    const u32 p = sc + 8 * m.r8(sc + 962) + 1864;
    const u32 ms = ms_base(m);
    m.w8(p, m.r8(e + 72));
    if (!(m.r32(sflag(k)) & 0x800u)) return;
    if (m.r16(e + 72) == 45) {
        const s32 mission = m.r16s(sc + 908);
        m.w8(ms + 60, m.r8(ms + 60) + 1u);
        if (m.r8(ms + 60) < 4) return;
        if (mission != 17 && mission != 31 && mission != 38 && mission != 46 && mission != 52) return;
    }
    if (!(m.r32(e) & 0x80u)) {
        m.w8(p, 116);
    } else {
        const u32 t = m.r8(p);
        if (t == 48) m.w8(p, 114);
        else if (t == 49) m.w8(p, 115);
        else if (t >= 49) m.w8(p, (m.r32(sflag(k)) & 0x400u) ? 112u : 113u);
    }
    bool friendly = false, counted_46 = false;
    if (m.r16(sc + 908) == 33 && m.r16(e + 72) == 27) {
        friendly = true;
    } else if (!(m.r32(sflag(k)) & 0x100u)) {
        if (m.r32(e) & 0x100u) {
            m.w8(p + 1, 3);
            if (!(m.r32(e + 80) & 0x3Fu)) m.w8(ms + 11, m.r8(ms + 11) + 1u);
        } else if (!(m.r32(e + 80) & 0x3Fu)) {
            m.w8(p + 1, 0);
            counted_46 = true;
        } else if (m.r32(e) & 0x40u) {
            m.w8(p + 1, 2);
        } else {
            friendly = true;
        }
    } else if (m.r32(e + 80) & 0x3Fu) {
        friendly = true;
    } else {
        m.w8(p + 1, 4);
        counted_46 = true;
    }
    if (friendly) {
        m.w8(p + 1, 1);
        m.w8(ms + 45, m.r8(ms + 45) + 1u);
    }
    if (counted_46) m.w8(ms + 46, m.r8(ms + 46) + 1u);
    m.w8(p + 6, k);
    auto cell = [](s32 v) {
        const s32 c = ((v >> 12) + 0x40000) >> 11;
        return c < 0 ? 0u : c >= 256 ? 255u : static_cast<u32>(c);
    };
    m.w8(p + 4, cell(m.r32s(e + 8)));
    m.w8(p + 5, cell(m.r32s(e + 16)));
    m.w16(p + 2, m.r16(lg + 2) / 30u);
    m.w8(lg + 10, m.r8(lg + 10) + 1u);
}

u32 Game::mission_grade() {
    cover(0x80054DBCu);
    const u32 ms = ms_base(m);
    const u32 rule = m.r8(ms + 52);
    if (rule == 255) return 5;
    if (rule == 0) return 4;
    if (rule == 1) {
        if (m.r8(ms + 55) >= m.r8(ms + 11) + m.r8(ms + 33)) {
            const u32 v = m.r8(ms + 46) + m.r8(ms + 45);
            if (v >= m.r8(ms + 54)) return v < m.r8(ms + 53) ? 2u : 1u;
        }
        return 3;
    }
    if (rule == 2) {
        const s32 t = m.r32s(ms + 12);
        if (t >= static_cast<s32>(m.r16(ms + 56))) return t < static_cast<s32>(m.r16(ms + 58)) ? 2u : 3u;
        return 1;
    }
    if (rule == 4) {
        const s32 s = m.r32s(ms + 4);
        if (s >= static_cast<s32>(m.r16(ms + 56))) return 1;
        return s >= static_cast<s32>(m.r16(ms + 58)) ? 2u : 3u;
    }
    return 4;
}

void Game::mission_result() {
    cover(0x80048A40u);
    Frame f(*this, 24);
    const u32 lg = m.r32(g::kScene) + 952;
    m.w8(lg + 11, mission_grade());
    m.w16(lg + 2, m.r16(ms_base(m) + 12));
}

void Game::request_exit(u32 what, u32 arg) {
    cover(0x80032F18u);
    const u32 x = m.r32(g::kExit);
    m.w32(x + 112, what);
    m.w32(x + 116, arg);
    const u32 w = m.r32(g::kWorld);
    m.w32(w, m.r32(w) | 0x20000000u);
}

void Game::set_next_936(s32 mission, u32 k) {
    cover(0x80038078u);
    m.w8(m.r32(g::kScene) + 936, m.r8(0x800B0E94u + static_cast<u32>(2 * mission) + k));
}

void Game::set_next_4152(s32 mission, u32 k) {
    cover(0x80038054u);
    m.w32(m.r32(g::kScene) + 4152, m.r8(0x800B0DA0u + static_cast<u32>(4 * mission) + k));
}

void Game::fade_reset(u32 a) {
    cover(0x8002E834u);
    m.w32(a + 4, 0);
    m.w32(a, 0x100000);
    m.w16(a + 8, 0);
    m.w16(a + 10, 0);
}

void Game::voice_clear(u32 v) {
    cover(0x8002E5B4u);
    if (v == 255) return;
    const u32 p = g::kVoices + 20 * v;
    m.w16(p, 255);
    m.w8(p + 2, 0);
    m.w8(p + 16, 0x80);
    m.w8(p + 17, 0x80);
    m.w16(p + 18, 0);
    m.w8(p + 3, 0);
}

void Game::sound_reset() {
    cover(0x8003048Cu);
    Frame f(*this, 24);
    engine_voices_stop();
    alert_channels_stop();
    {
        cover(0x8002DDE0u);
        Frame g3(*this, 24);
        for (u32 v = 0; v < 24; ++v) voice_clear(v);
        spu_set_key(0, 0xFFFFFF);
    }
    snd_delayed_reset();
    sound_queue_reset();
}

void Game::view_mark(u32 cam) {
    cover(0x8007786Cu);
    m.w32(cam, m.r32(cam) | 0x400u);
}

void Game::view_set(u32 cam, u32 mode, u32 subject) {
    cover(0x800793C4u);
    Frame f(*this, 40);
    u32 follow = 0xFFFFFFFFu;
    view_mark(cam);
    if (mode & 0x401u) follow = m.r32(subject) & 0x3Fu;
    if (mode & 0xDFFu) {
        const u32 c = m.r32(cam) & 0xFFFFFFC0u;
        m.w32(cam, c);
        m.w32(cam, c | 0x100u | (m.r32(subject) & 0x3Fu));
        const u32 s = m.r32(subject);
        m.w32(cam + 52, subject + 8);
        m.w32(cam + 56, 0);
        m.w32(cam + 60, 0);
        m.w32(cam + 64, 0);
        m.w16(cam + 72, 0);
        m.w16(cam + 74, 0);
        m.w16(cam + 76, 0);
        m.w32(cam + 200, s & 0x3Fu);
        m.w32(cam + 196, follow);
        m.w32(cam + 200, m.r32(subject) & 0x3Fu);
    } else {
        const u32 c = m.r32(cam);
        m.w32(cam + 196, 0xFFFFFFFFu);
        m.w32(cam, c & 0xFFFFFEFFu);
    }
    m.w32(cam + 80, mode);
    m.w32(cam + 208, 0);
    m.w32(cam + 204, 0);
    m.w16(cam + 212, 0);
    m.w16(cam + 214, 0);
    m.w16(cam + 216, 0);
    m.w32(cam + 220, 0);
    m.w32(cam + 224, 0);
    m.w32(cam + 228, 0);
}

void Game::view_flag(u32 a) {
    cover(0x80035740u);
    const u32 w = m.r32(g::kWorld);
    if (!a || m.r8(m.r32(g::kScene) + 926) || (m.r32(m.r32(w + 40) + 80) & 0x800u)) m.w32(w, m.r32(w) | 1u);
    else m.w32(w, m.r32(w) & 0xFFFFFFFEu);
}

void Game::view_player() {
    cover(0x80079924u);
    Frame f(*this, 24);
    view_set(m.r32(m.r32(g::kWorld) + 40), 0x2000, m.r32(m.r32(g::kSlots)));
    view_flag(0);
}

void Game::kill_camera(u32 who) {
    cover(0x800797FCu);
    Frame f(*this, 24);
    if (!(m.r32(m.r32(g::kPlayer)) & 1u) || static_cast<s32>(who) < 0) return;
    if (!m.r8(m.r32(g::kScene) + 925)) return;
    const u32 k = who & 0x3Fu;
    if (!(m.r32(slot(k)) & 0x4000u)) return;
    const u32 cam = m.r32(m.r32(g::kWorld) + 40);
    m.w32(cam + 192, k);
    view_set(cam, 2048, m.r32(m.r32(g::kSlots)));
    view_flag(0);
}

void Game::mission_end() {
    cover(0x80037E50u);
    Frame f(*this, 40);
    const u32 ms = ms_base(m);
    m.w8(ms + 63, m.r8(ms + 63) + 1u);
    const u32 fl = m.r8(ms + 42);
    if (fl & 1u) {
        m.w8(ms + 42, fl & 0xFEu);
        request_exit(128, 0);
        return;
    }
    if (m.r8(ms + 42) & 2u) {
        m.w8(ms + 42, fl & 0xFDu);
        request_exit(0x100000, 0);
        return;
    }
    if (m.r8(ms + 42) & 4u) {
        m.w8(ms + 42, fl & 0xFBu);
        view_player();
        request_exit(0x200000, 0);
        return;
    }
    const u32 how = m.r8(ms + 41);
    const u32 sc = m.r32(g::kScene);
    if (!how) {
        const u32 v = m.r8(ms + 35);
        m.w8(ms + 61, v);
        request_exit(2, v);
        return;
    }
    switch (how) {
    case 1: {
        const u32 v = m.r8(ms + 36);
        set_next_4152(m.r16s(sc + 908), v);
        request_exit(16, v);
        return;
    }
    case 2: {
        const u32 v = m.r8(ms + 38);
        set_next_936(m.r16s(sc + 908), v);
        request_exit(0x4000, v);
        return;
    }
    case 3:
        request_exit(1024, m.r8(ms + 37));
        return;
    case 4:
        m.w32(sc + 928, 3);
        set_next_936(m.r16s(sc + 908), m.r8(ms + 38));
        mission_result();
        sound_reset();
        request_exit(0x1000000, 0);
        return;
    case 5:
        mission_result();
        m.w16(sc + 908, m.r8(sc + 957));
        sound_reset();
        if (m.r8(ms + 41) == 5 && m.r16(sc + 908) == 54) {
            m.w8(sc + 937, 5);
            m.w16(sc + 910, 25);
            m.w8(sc + 938, 1);
            m.w8(sc + 939, 1);
        }
        request_exit(0x2000000, 0);
        return;
    case 9:
        mission_result();
        sound_reset();
        request_exit(0x800000, 0);
        return;
    default:
        unported(0x80037E50u, "mission_end with an unknown ending");
        return;
    }
}

u32 Game::msn_vm_exec(u32 pc) {
    cover(0x80054EBCu);
    Frame f(*this, 80);
    if (script_observer) script_observer->script_begin(pc);
    const u32 sc = m.r32(g::kScene);
    auto ms = [&] { return ms_base(m); };
    auto setb = [&](u32 off) { m.w8(ms() + off, m.r8(pc)); ++pc; };
    auto sf_or = [&](u32 k, u32 bit) { m.w32(sflag(k), m.r32(sflag(k)) | bit); };
    auto sf_clr = [&](u32 k, u32 bit) { m.w32(sflag(k), m.r32(sflag(k)) & ~bit); };
    for (u32 op = m.r8(pc); op; op = m.r8(pc)) {
        if (script_observer) script_observer->script_op(pc, op);
        ++pc;
        switch (op) {
        case 0x01: {
            m.w8(ms() + 41, m.r8(pc));
            const u32 mode = m.r32(m.r32(g::kWorld) + 4);
            ++pc;
            if (mode == 4) {
                request_exit(8, m.r8(ms() + 39));
                m.w32(sc, m.r32(sc) | 0x10000000u);
                if (m.r8(ms() + 41)) cd_cancel_streams();
            } else if (mode == 0) {
                mission_end();
                const u32 w = m.r32(g::kWorld);
                m.w32(w, m.r32(w) & 0xDFFFFFFFu);
            }
            break;
        }
        case 0x02: setb(36); break;
        case 0x03: setb(35); break;
        case 0x04: setb(37); break;
        case 0x05: setb(38); break;
        case 0x06: {
            static constexpr u8 kMap[8] = {0, 1, 2, 8, 4, 16, 32, 64};
            const u32 v = m.r8(pc);
            if (v < 8) m.w8(ms() + 39, kMap[v]);
            ++pc;
            break;
        }
        case 0x07:
            m.w8(ms() + 42, m.r8(ms() + 42) | 4u);
            m.w8(ms() + 41, 9);
            break;
        case 0x08:
            m.w8(ms() + 41, 9);
            m.w8(sc + 957, m.r8(pc));
            ++pc;
            break;
        case 0x09: m.w8(ms() + 42, m.r8(ms() + 42) | 1u); break;
        case 0x0A:
            m.w8(sc + 2390, m.r8(pc));
            ++pc;
            break;
        case 0x0B: pc = msn_if_less(pc, m.r32s(ms() + 16)); break;
        case 0x0C: pc = msn_if_score(pc); break;
        case 0x0D: pc = msn_if_counter(pc, 34, 0x80055D2Cu, true); break;
        case 0x0E: pc = msn_if_counter(pc, 33, 0x80055D94u, false); break;
        case 0x0F: pc = msn_if_counter(pc, 32, 0x80055DFCu, true); break;
        case 0x10: pc = msn_if_flag47(pc); break;
        case 0x11: pc = msn_if_kills(pc); break;
        case 0x12: pc = msn_if_scene_flag(pc); break;
        case 0x13: pc = msn_if_random(pc); break;
        case 0x14: pc = msn_if_hp(pc); break;
        case 0x15: {
            const u32 k = m.r8(pc);
            ++pc;
            if (k >= 32) {
                ground_show(k - 32);
                ground_activate(k - 32);
            } else {
                air_show(k);
                air_activate(k);
            }
            break;
        }
        case 0x16: {
            const u32 k = m.r8(pc);
            ++pc;
            const u32 e = slot(k);
            m.w32(e, m.r32(e) & ~0x4000u);
            if (k < 32) m.w32(slot(k), m.r32(slot(k)) & 0xFFFEFFFFu);
            break;
        }
        case 0x17:
        case 0x19: ++pc; break;
        case 0x18: {
            const u32 k = m.r8(pc);
            ++pc;
            const u32 e = slot(k);
            const s32 v = msn_s16(pc);
            if (v == -1) {
                const u32 p = k >= 32 ? m.r32(m.r32(g::kGroundRecords) + 40 * k - 1268)
                                      : m.r32((k << 6) + m.r32(g::kSlotRecords) + 16);
                m.w16(e + 78, m.r16(p));
            } else if (v == -2) {
                m.w16(e + 78, m.r16(kSavedHp));
            } else if (v == -3) {
                m.w16(kSavedHp, m.r16(e + 78));
            } else {
                m.w16(e + 78, static_cast<u32>(v));
            }
            pc += 2;
            break;
        }
        case 0x1A: {
            const u32 k = m.r8(pc);
            ++pc;
            sf_or(k, 0x20000u);
            break;
        }
        case 0x1B: {
            const u32 k = m.r8(pc);
            ++pc;
            if (m.r8(pc) == 1) {
                const u32 e = slot(k);
                sf_or(k, 0x100u);
                m.w32(e, m.r32(e) & 0xFFFFFE3Fu);
                m.w32(e, m.r32(e) | 0x80u);
                m.w8(m.r32(kBlips + 4 * k) + 5, 1);
                m.w8(kMarks + 12 * k + 2, 1);
                sf_clr(k, 0x200u);
            } else if (m.r8(pc) == 0) {
                sf_clr(k, 0x100u);
                m.w8(m.r32(kBlips + 4 * k) + 5, 0);
                m.w8(kMarks + 12 * k + 2, 0);
            }
            ++pc;
            break;
        }
        case 0x1C: {
            const u32 k = m.r8(pc);
            ++pc;
            if (m.r8(pc) == 1) {
                const u32 e = slot(k);
                sf_or(k, 0x200u);
                m.w32(e, m.r32(e) & 0xFFFFFE3Fu);
                m.w32(e, m.r32(e) | 0x40u);
                m.w8(m.r32(kBlips + 4 * k) + 5, 3);
                m.w8(kMarks + 12 * k + 2, 2);
                sf_clr(k, 0x100u);
                if (k == m.r8(g::kCurrentTarget)) m.w8(g::kCurrentTarget, pair_drop_target(0));
            } else if (m.r8(pc) == 0) {
                sf_clr(k, 0x200u);
                m.w8(m.r32(kBlips + 4 * k) + 5, 0);
                m.w8(kMarks + 12 * k + 2, 0);
            }
            ++pc;
            break;
        }
        case 0x1D: {
            const u32 k = m.r8(pc);
            const u32 v = m.r8(pc + 1);
            if (v & 2u) sf_or(k, 0x4000u);
            else sf_clr(k, 0x4000u);
            if (v & 1u) sf_or(k, 0x8000u);
            else m.w32(sflag(k), m.r32(sflag(k)) & 0xFFFF7FFFu);
            pc += 2;
            if (m.r8(g::kCurrentTarget) == k) {
                m.w8(g::kCurrentTarget, 255);
                pair_drop_target(0);
                pair_add(0, 255);
            }
            break;
        }
        case 0x1E: pc += 2; break;
        case 0x1F: {
            const u32 k = m.r8(pc);
            const u32 v = m.r8(pc + 1);
            pc += 2;
            m.w8(184 * k + m.r32(g::kAiInfo) + 28, 255);
            m.w32((k << 6) + m.r32(g::kSlotRecords) + 36, v - 1);
            break;
        }
        case 0x20: {
            const u32 k = m.r8(pc), r = m.r8(pc + 1);
            pc += 2;
            m.w32(40 * k + m.r32(g::kGroundRecords) + 20, m.r32(0x800BCF30u) + 48 * r);
            break;
        }
        case 0x21: {
            const u32 k = m.r8(pc);
            ++pc;
            const u32 v = m.r8(pc);
            const u32 at = (k << 6) + m.r32(g::kSlotRecords) + 20;
            if (v == 1) m.w32(at, 0x800B20D4u);
            else if (v == 2) m.w32(at, 0x800B20FCu);
            else m.w32(at, m.r32(0x800BCF24u) + 40 * m.r8(44 * k + m.r32(0x800BCF00u) + 3));
            ++pc;
            break;
        }
        case 0x22:
        case 0x23: {
            const u32 k = m.r8(pc);
            ++pc;
            const u32 bit = op == 0x22 ? 0x1000u : 0x2000u;
            if (m.r8(pc) == 1) sf_or(k, bit);
            else sf_clr(k, bit);
            ++pc;
            break;
        }
        case 0x24: {
            const u32 k = m.r8(pc);
            const u32 v = m.r8(pc + 1);
            pc += 2;
            if (v == 1) link_engage(k);
            else link_release(k);
            break;
        }
        case 0x25: {
            const u32 k = m.r8(pc);
            u32 p = pc + 1;
            const u32 buf = f.at(0x10);
            m.w32(buf, msn_u32(p) << 12);
            p += 4;
            m.w32(buf + 4, msn_u32(p) << 12);
            p += 4;
            m.w32(buf + 8, msn_u32(p) << 12);
            p += 4;
            m.w16(buf + 0x10, static_cast<u32>(msn_s16(p)));
            p += 2;
            m.w16(buf + 0x12, static_cast<u32>(msn_s16(p)));
            p += 2;
            m.w16(buf + 0x14, static_cast<u32>(msn_s16(p)));
            pc = p + 2;
            if (k) {
                const u32 e = slot(k);
                for (u32 o = 0; o < 24; o += 4) m.w32(e + 8 + o, m.r32(buf + o));
            } else {
                player_place(buf);
            }
            break;
        }
        case 0x29: {
            const u32 kind = m.r8(pc);
            const s32 delay = msn_s16(pc + 1);
            pc += 3;
            radio_queue(kind, delay);
            break;
        }
        case 0x2A: {
            const u32 a = m.r8(pc), b = m.r8(pc + 1);
            pc += 2;
            sound_queue_push(a, b);
            break;
        }
        case 0x2B: {
            const u32 n = m.r8(pc);
            ++pc;
            for (u32 k = 0; k < n; ++k) {
                const u32 a = m.r8(pc), b = m.r8(pc + 1);
                pc += 2;
                sound_queue_push(a, b);
            }
            break;
        }
        case 0x2D: setb(30); break;
        case 0x2E: m.w8(ms() + 32, 0); break;
        case 0x2F: setb(31); break;
        case 0x30: m.w8(ms() + 33, 0); break;
        case 0x31: m.w8(ms() + 34, m.r8(ms() + 34) + 1u); break;
        case 0x32: m.w8(ms() + 34, 0); break;
        case 0x33: message_show(m.r8(ms() + 43)); break;
        case 0x34: setb(43); break;
        case 0x35: m.w8(ms() + 32, m.r8(ms() + 32) * 2u); break;
        case 0x36: setb(44); break;
        case 0x37: music_play(m.r8(ms() + 44)); break;
        case 0x38:
            music_play(m.r8(pc));
            ++pc;
            break;
        case 0x39: setb(28); break;
        case 0x3A: {
            const s32 v = msn_s16(pc);
            pc += 2;
            m.w32(ms() + 20, static_cast<u32>(v));
            break;
        }
        case 0x3B: world_flag4(1); break;
        case 0x3C: pc = msn_if_less(pc, m.r32s(ms() + 12)); break;
        case 0x3D: pc = msn_if_equal(pc, m.r8(m.r32(g::kWorld) + 8), 0x80056080u); break;
        case 0x3E: pc = msn_if_equal(pc, m.r8(sc + 916), 0x800560E4u); break;
        case 0x46:
            m.w8(ms() + 42, m.r8(ms() + 42) | 2u);
            setb(62);
            break;
        case 0x47:
            ++pc;
            m.w8(ms() + 42, m.r8(ms() + 42) | 4u);
            break;
        case 0x48: {
            m.w8(ms() + 49, m.r8(pc));
            const s32 v = msn_s16(pc + 1);
            pc += 3;
            m.w16(ms() + 50, static_cast<u32>(v));
            break;
        }
        case 0x49:
            m.w8(ms() + 48, 1);
            m.w16(ms() + 26, 0);
            break;
        case 0x4A:
            for (u32 k = 0; k < 4; ++k) m.w8(ms() + 52 + k, m.r8(pc + k));
            pc += 4;
            break;
        case 0x4B: {
            const s32 v = msn_s16(pc);
            pc += 2;
            m.w32(ms() + 4, m.r32(ms() + 4) + static_cast<u32>(v));
            break;
        }
        case 0x4C: {
            m.w8(ms() + 52, m.r8(pc));
            const s32 a = msn_s16(pc + 1);
            m.w16(ms() + 56, static_cast<u32>(a));
            const s32 b = msn_s16(pc + 3);
            pc += 5;
            m.w16(ms() + 58, static_cast<u32>(b));
            break;
        }
        default: break;
        }
    }
    if (script_observer) script_observer->script_end(pc);
    return pc;
}

u32 Game::hud_alert_take(u32 priority) {
    cover(0x8002939Cu);
    u32 a = m.r32(0x800BCBA4u);
    u32 pri = a + 1;
    for (u32 k = 0; k < 6; ++k, pri += 4, a += 4) {
        if (!m.r8(a) || !(static_cast<s32>(priority) < static_cast<s32>(m.r8(pri)))) {
            m.w8(pri, priority);
            return a;
        }
    }
    return 0;
}

void Game::hud_alert_damage() {
    cover(0x80029588u);
    Frame f(*this, 24);
    const u32 p = hud_alert_take(16);
    if (!p) return;
    m.w8(p, 10);
    m.w8(p + 2, static_cast<u32>((rand() & 0x3F) + 168));
}

void Game::rumble_add(u32 kind) {
    cover(0x8006CB44u);
    const u32 w = m.r32(g::kWorld);
    if (!(m.r32(m.r32(w + 40) + 80) & 3u)) return;
    const u32 k = m.r32(m.r32(g::kScene) + 40) & 7u;
    auto add = [&](u32 v) {
        m.w16(w + 652, m.r16(w + 652) + m.r16(v));
        m.w16(w + 654, m.r16(w + 654) + m.r16(v + 2));
        m.w16(w + 656, m.r16(w + 656) + m.r16(v));
        m.w16(w + 658, m.r16(w + 658) + m.r16(v + 2));
    };
    if (kind == 1) add(0x800B2CBCu + 4 * k);
    add((kind ? 0x800B2CFCu : 0x800B2CDCu) + 4 * k);
    for (u32 o = 652; o <= 658; o += 2)
        if (m.r16s(w + o) >= 7) m.w16(w + o, 6);
}

void Game::zone_penalty() {
    cover(0x80076FA4u);
    Frame f(*this, 24);
    if (m.r32(m.r32(g::kPlayer)) & 0x7000u) return;
    const u32 p = slot(0);
    m.w16(p + 78, m.r16(p + 78) - 1u);
    hud_alert_damage();
    rumble_add(2);
}

u32 Game::fade_overlay(u32 ot, u32 packet, s32 level) {
    cover(0x8008FC9Cu);
    Frame f(*this, 48);
    if (level < 0) return packet;
    if (level >= 0x1001) level = 0x1000;
    gte.ctc2(21, 0);
    gte.ctc2(22, 0);
    gte.ctc2(23, 0);
    gte.mtc2(6, 0x62FFFFFFu);
    s32 ir0;
    if (level < 0x800) ir0 = 0x1000 - (rsin(level >> 1) >> 1);
    else ir0 = rsin(level >> 1) >> 1;
    gte.mtc2(8, static_cast<u32>(ir0));
    const u32 semi = m.r32(m.r32(g::kScene) + 4) == 0 ? 0x100u : 0u;
    gte.op(0x780010);
    const u32 s2 = packet + 0xC;
    u32 s0 = packet + 0x1C;
    m.w32(packet, (s2 & 0xFFFFFFu) | 0x2000000u);
    m.w32(packet + 0xC, (s0 & 0xFFFFFFu) | 0x3000000u);
    m.w32(packet + 4, 0xE6000001u);
    m.w32(packet + 8, 0xE1000340u);
    m.w32(s2 + 8, 0);
    m.w32(s2 + 0xC, 0xF00140u);
    if (level < 0x1000) m.w32(s2 + 4, gte.mfc2(22));
    else m.w32(s2 + 4, 0x60000000u);
    u32 t0 = s0;
    if (level & 0xFFF) {
        gte.mtc2(8, static_cast<u32>(0x1000 - rsin(level >> 1)));
        gte.mtc2(6, 0x2E7C7F7Du);
        t0 = s0 + 0xA0;
        gte.op(0x780010);
        const u32 tp = semi >> 4;
        for (u32 k = 0; k < 4; ++k, s0 += 0x28) {
            const u32 v1 = s0 + 0x1A;
            m.w32(s0, (((s0 & 0xFFFFFFu) + 0x28) & 0xFFFFFFu) | 0x9000000u);
            m.w16(v1 - 4, (k & 2u) | 0x120u | tp);
            if (k & 2u) {
                m.w16(v1 - 2, 0xA0);
                m.w16(v1 - 0x12, 0xA0);
                m.w16(v1 + 6, 0x140);
                m.w16(v1 - 0xA, 0x140);
            } else {
                m.w16(v1 - 2, 0);
                m.w16(v1 - 0x12, 0);
                m.w16(v1 + 6, 0xA0);
                m.w16(v1 - 0xA, 0xA0);
            }
            if (k & 1u) {
                m.w16(v1 - 8, 0x78);
                m.w16(v1 - 0x10, 0x78);
                m.w16(v1 + 8, 0xF0);
                m.w16(v1, 0xF0);
            } else {
                m.w16(v1 - 8, 0);
                m.w16(v1 - 0x10, 0);
                m.w16(v1 + 8, 0x78);
                m.w16(v1, 0x78);
            }
        }
        const u32 rgb = gte.mfc2(22);
        const u32 q = s2 + 0x10;
        m.w32(q + 0x7C, rgb);
        m.w32(q + 0x54, rgb);
        m.w32(q + 0x2C, rgb);
        m.w32(q + 4, rgb);
        static constexpr u16 kUv[16][2] = {
            {0x0C, 0x203}, {0x14, 0x3A1}, {0x1C, 0x7905}, {0x24, 0x79A1}, {0x34, 0x7705}, {0x3C, 0x77A1},
            {0x44, 0xEE03}, {0x4C, 0xEDA1}, {0x5C, 0x31F}, {0x64, 0x2BD}, {0x6C, 0x791F}, {0x74, 0x79BB},
            {0x84, 0x771F}, {0x8C, 0x77BB}, {0x94, 0xED1F}, {0x9C, 0xEEBD},
        };
        for (const auto& [o, v] : kUv) m.w16(q + o, v);
    }
    m.w32(t0, ((t0 + 0xC) & 0xFFFFFFu) | 0x2000000u);
    m.w32(t0 + 4, 0xE6000000u);
    m.w32(t0 + 8, 0xE1000220u);
    m.w32(t0, (m.r32(t0) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
    return t0 + 0xC;
}
}
