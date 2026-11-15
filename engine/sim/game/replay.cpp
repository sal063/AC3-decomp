#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kRecorder = 0x800BD0B8u;
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kRumble = 0x800BCBA4u;
constexpr u32 kKcd = 0x800BCBA8u;
constexpr u32 kDemoStreams = 0x800B2334u;
constexpr u32 kDemoStreamIndex = 0x800B23D8u;
constexpr u32 kReplayStreams = 0x800B4334u;
constexpr u32 kBullets = 0x15D0u;
constexpr u32 kBulletMasks = 0x33D0u;
constexpr u32 kMissileMask = 0x1548u;
constexpr u32 kTextPressStart = 0x80023610u;
constexpr u32 kTextAceCombat3 = 0x8002362Cu;
}

void Game::sim_tick_p9() {
    cover(0x800330CCu);
    Frame f(*this, 0x18);
    m.w32(sp + 0x10, reg_ra);
    replay_frame();
    bullets_replay();
    sim_step7();
    sim_step8();
}

void Game::sim_tick_p13() {
    cover(0x800331E0u);
    Frame f(*this, 0x30);
    m.w32(sp + 0x2C, reg_ra);
    m.w32(sp + 0x28, reg_s0);
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    const u32 view = m.r32(m.r32(g::kWorld) + 0x28);
    sim_step1();
    sim_step2();
    sim_step3();
    sim_step4();
    sim_step5();
    sim_step6();
    sim_step7();
    sim_step8();
    const u32 st = m.r32(0x800BD098u);
    m.w32(sp + 0x10, m.r32(st + 0x1C));
    targeting_update(sp + 0x10);
    m.w32(view + 0xBC, m.r32(sp + 0x20));
    m.w32(m.r32(0x800BD098u) + 0x18, m.r32(sp + 0x24));
}

void Game::replay_frame() {
    cover(0x8007F8E4u);
    Frame f(*this, 0x18);
    const u32 mode = m.r32(m.r32(g::kWorld) + 4);
    if (mode == 128) {
        replay_begin();
        return;
    }
    if (mode == 512) {
        replay_end();
        return;
    }
    const s32 done = replay_step();
    const u32 sc = m.r32(g::kScene);
    if (m.r8(sc + 933)) {
        if ((m.r32(sc + 300) & 0x10u) || done == 1) request_exit(512, m.r32(m.r32(g::kWorld) + 8));
        return;
    }
    if (m.r32(sc + 300) & 0x40u) {
        request_exit(512, m.r32(m.r32(g::kWorld) + 8));
        return;
    }
    if (done == 1) {
        view_replay();
        replay_rewind();
        replay_tiles(0);
    }
}

void Game::replay_begin() {
    cover(0x8007F448u);
    Frame f(*this, 0x20);
    rumble_clear();
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) | 0x10000000u);
    u32 params = kReplayStreams;
    if (m.r8(sc + 933)) {
        const u32 demo = static_cast<u32>(m.r16s(sc + 908));
        params = m.r32(kDemoStreams + 4 * demo_stream(demo));
        if (!kcd_stream_on_this_disc(static_cast<u32>(m.r16s(params)))) {
            const s32 k = static_cast<s16>(m.r16s(m.r32(g::kScene) + 908) % 6);
            params = kReplayStreams + 12 * static_cast<u32>(k + 1);
        }
    }
    cd_request_stream(params, 0, 0);
    weapons_reset();
    view_replay();
    for (u32 i = 0; i < 64; ++i)
        word_copy(m.r32(kRecorder) + 6224 + 32 * i, m.r32(m.r32(g::kSlots) + 4 * i), 8);
    replay_rewind();
    replay_tiles(0);
    const u32 w = m.r32(g::kWorld);
    const u32 k = m.r32(w + 8);
    m.w32(w, m.r32(w) & 0xEFFFFFFFu);
    request_exit(256, k);
}

void Game::replay_end() {
    cover(0x8007F808u);
    Frame f(*this, 0x20);
    cd_cancel_streams();
    replay_rewind();
    replay_tiles(1);
    const u32 r = m.r32(kRecorder);
    m.w32(r, 0);
    m.w32(r + 4120, 0);
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) & 0xEFFFFFFFu);
    for (u32 i = 0; i < 64; ++i)
        word_copy(m.r32(m.r32(g::kSlots) + 4 * i), m.r32(kRecorder) + 6224 + 32 * i, 8);
    weapons_reset();
    if (!m.r8(m.r32(g::kScene) + 933)) {
        mission_end();
        return;
    }
    const u32 w = m.r32(g::kWorld);
    m.w32(w, m.r32(w) | 0x10000000u);
}

void Game::replay_rewind() {
    cover(0x8007F298u);
    Frame f(*this, 0x48);
    u32 r = m.r32(kRecorder);
    const u32 oldest = m.r32(r + 16);
    m.w32(r + 0x1018, 0);
    m.w32(r + 0x1840, 0xFFFFFFFFu);
    m.w32(r + 0x1844, 0xFFFFFFFFu);
    m.w32(r + 0x1848, 0);
    m.w32(r + 0x101C, oldest);
    const u32 v = f.at(0x10), mat = f.at(0x18);
    memset(v, 0, 8);
    m.w16(v + 4, 0x4800);
    rot_matrix_yxz(m.r32(kRecorder) + 0x1038, mat);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(mat + 4 * k));
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    r = m.r32(kRecorder);
    m.w32(r + 0x1820, gte.mfc2(25));
    m.w32(r + 0x1824, gte.mfc2(26));
    m.w32(r + 0x1828, gte.mfc2(27));
    m.w32(r + 0x1830, 0);
    m.w32(r + 0x1834, 0);
    m.w32(r + 0x1838, 0);
    m.w32(r + 0x1820, m.r32(r + 0x1820) << 3);
    m.w32(r + 0x1828, m.r32(r + 0x1828) << 3);
    m.w32(r + 0x1824, m.r32(r + 0x1824) << 3);
    effects_reset();
    trails_reset();
    weapons_reset();
    for (u32 i = 0; i < 64; ++i) word_copy(m.r32(kRecorder) + 0x1020 + 32 * i, m.r32(kRecorder) + 24 + 32 * i, 8);
    for (u32 i = 0; i < 64; ++i) word_copy(m.r32(m.r32(g::kSlots) + 4 * i), m.r32(kRecorder) + 24 + 32 * i, 8);
    view_replay_origin(m.r32(m.r32(g::kSlots)) + 8, m.r32(kRecorder) + 0x1820);
    view_replay_subject(0, 0, 0);
}

s32 Game::replay_step() {
    cover(0x8007F5B4u);
    Frame f(*this, 0x20);
    u32 r = m.r32(kRecorder);
    const s32 n = m.r32s(r + 0x1018);
    if (n >= m.r32s(r)) return 1;
    const u32 subject = m.r32(r + 0x1840);
    u32 p = m.r32(r + 0x101C);
    m.w32(r + 0x1018, static_cast<u32>(n + 1));
    m.w32(r + 0x14, 0);
    do {
        r = m.r32(kRecorder);
        if (static_cast<s32>(p) >= m.r32s(r + 8)) p = m.r32(r + 4);
        p = replay_record(p);
    } while (!m.r32(m.r32(kRecorder) + 0x14));
    r = m.r32(kRecorder);
    m.w32(r + 0x101C, p);
    if (subject != m.r32(r + 0x1840)) {
        m.w32(r + 0x1848, 150);
        if (subject == 0xFFFFFFFFu) m.w32(r + 0x1844, m.r32(r + 0x1840));
    }
    if (m.r32(r + 0x1848)) m.w32(r + 0x1848, m.r32(r + 0x1848) - 1);
    else m.w32(r + 0x1844, m.r32(r + 0x1840));
    const u32 slots = m.r32(g::kSlots);
    const u32 player = m.r32(slots);
    const u32 play = r + 0x1020;
    m.w32(r + 0x1820, m.r32(play + 8) - m.r32(player + 8));
    m.w32(r + 0x1824, m.r32(play + 12) - m.r32(player + 12));
    m.w32(r + 0x1828, m.r32(play + 16) - m.r32(player + 16));
    const s32 k = m.r32s(r + 0x1844);
    if (k >= 0) {
        const u32 e = m.r32(slots + 4 * static_cast<u32>(k));
        const u32 s = r + 0x1020 + 32 * static_cast<u32>(k);
        m.w32(r + 0x1830, m.r32(s + 8) - m.r32(e + 8));
        m.w32(r + 0x1838, m.r32(s + 16) - m.r32(e + 16));
        m.w32(r + 0x1834, m.r32(s + 12) - m.r32(e + 12));
    }
    for (u32 i = 0; i < 64; ++i) word_copy(m.r32(m.r32(g::kSlots) + 4 * i), m.r32(kRecorder) + 0x1020 + 32 * i, 8);
    view_replay_origin(m.r32(m.r32(g::kSlots)) + 8, m.r32(kRecorder) + 0x1820);
    u32 subj = 0, kind = 0;
    const s32 shown = m.r32s(m.r32(kRecorder) + 0x1844);
    if (shown >= 0) {
        const u32 e = m.r32(m.r32(g::kSlots) + 4 * static_cast<u32>(shown));
        subj = e + 8;
        kind = 2;
        if (shown < 32) {
            kind = 1;
        } else {
            const s32 type = m.r16s(e + 0x48);
            if (type == 35 || type == 36 || type == 43 || type == 61) kind = 1;
        }
    }
    view_replay_subject(subj, m.r32(kRecorder) + 0x1830, kind);
    return 0;
}

u32 Game::replay_record(u32 rec) {
    cover(0x8007F1D4u);
    Frame f(*this, 0x20);
    const u32 base = m.r32(kRecorder) + 0x1020;
    switch ((m.r32(rec) >> 4) & 7u) {
    case 0: recorder_fold_state(rec, base); break;
    case 1: recorder_fold_move(rec, base); break;
    case 2: replay_weapon(rec); break;
    case 3: replay_effect(rec); break;
    case 4: tile_redo(rec); break;
    default: break;
    }
    return rec + 4 * (m.r32(rec) & 0xFu);
}

void Game::replay_weapon(u32 rec) {
    cover(0x8007E998u);
    Frame f(*this, 0x58);
    const u32 h = m.r32(rec);
    const u32 i = (h >> 9) & 0x7Fu;
    auto whole = [&](u32 w) { return static_cast<u32>((m.r32s(w) >> 12) << 12); };
    auto angle = [&](u32 w) { return static_cast<u32>(static_cast<s32>(m.r32(w) << 20) >> 18); };
    switch ((h >> 7) & 3u) {
    case 0: {
        const u32 vel = f.at(0x28), pos = f.at(0x10);
        m.w32(vel, static_cast<u32>(m.r16s(rec + 2) << 6));
        m.w32(vel + 4, static_cast<u32>(m.r16s(rec + 4) << 6));
        m.w32(vel + 8, static_cast<u32>(m.r16s(rec + 6) << 6));
        m.w32(pos, whole(rec + 8));
        m.w32(pos + 4, whole(rec + 12));
        m.w32(pos + 8, whole(rec + 16));
        m.w16(pos + 16, angle(rec + 8));
        m.w16(pos + 18, angle(rec + 12));
        m.w16(pos + 20, angle(rec + 16));
        bullet_replay_spawn(i, pos, vel, m.r32(rec + 20));
        break;
    }
    case 1: bullet_replay_kill(i); break;
    case 2: {
        const u32 pos = f.at(0x38);
        m.w32(pos, whole(rec + 4));
        m.w32(pos + 4, whole(rec + 8));
        m.w32(pos + 8, whole(rec + 12));
        m.w16(pos + 16, angle(rec + 4));
        m.w16(pos + 18, angle(rec + 8));
        m.w16(pos + 20, angle(rec + 12));
        missile_replay_show(i, m.r16(rec + 2), pos);
        break;
    }
    case 3: missile_replay_hide(i); break;
    default: break;
    }
}

void Game::replay_effect(u32 rec) {
    cover(0x8007F048u);
    Frame f(*this, 0x18);
    const u32 h = m.r32(rec);
    switch (h & 0xFu) {
    case 1: replay_gun_flash((h >> 9) & 0xFFu, (h >> 17) & 0xFFFu); break;
    case 3: replay_effect_at(m.r8(rec + 4), rec); break;
    case 4: replay_effect_moving(m.r8(rec + 3), rec); break;
    case 5: replay_effect_param(m.r8(rec + 3), rec); break;
    default: break;
    }
}

void Game::replay_gun_flash(u32 kind, u32 t) {
    cover(0x8007F9CCu);
    const u32 g2 = m.r32(kFx) + 14664;
    m.w32(g2, 1);
    m.w16(g2 + 4, kind);
    m.w16(g2 + 6, t);
}

void Game::replay_effect_at(u32 kind, u32 rec) {
    cover(0x8007F9E8u);
    Frame f(*this, 0x28);
    const u32 pos = f.at(0x18);
    m.w32(pos, static_cast<u32>((m.r32s(rec) >> 12) << 12));
    m.w32(pos + 4, static_cast<u32>((m.r32s(rec + 4) >> 12) << 12));
    m.w32(pos + 8, static_cast<u32>((m.r32s(rec + 8) >> 12) << 12));
    const u32 a = (m.r32(rec + 8) >> 4) & 0xFFu, b = (m.r32(rec + 4) >> 8) & 0xFu;
    switch (kind) {
    case 0: hit_sparks(pos, a); break;
    case 1: ground_hit(pos, m.r32(rec + 8) & 0xFu, a); break;
    case 2: wreck_scatter_fx(pos, b); break;
    case 3: spray_spawn(pos, static_cast<s32>(a)); break;
    case 4: destroy_effect(pos, a); break;
    case 5: blast_effect_fx(pos, a); break;
    case 6: scenery_effect(pos, a, b); break;
    default: break;
    }
}

void Game::replay_effect_moving(u32 kind, u32 rec) {
    cover(0x8007FB24u);
    Frame f(*this, 0x38);
    const u32 pos = f.at(0x10), vel = f.at(0x20);
    for (u32 k = 0; k < 12; k += 4) m.w32(pos + k, m.r32(rec + 4 + k) << 12);
    for (u32 k = 0; k < 12; k += 4) m.w32(vel + k, static_cast<u32>((m.r32s(rec + 4 + k) >> 20) << 12));
    const u32 tag = m.r8(rec + 2), n = (m.r32(rec) >> 9) & 0x7Fu;
    switch (kind) {
    case 0: air_burst(pos, vel); break;
    case 1: explosion_fall_fx(pos, vel, tag); break;
    case 2: smoke_fx(pos, vel, tag); break;
    case 3: player_explosion_fx(pos, vel); break;
    case 4: wreck_flames(pos, vel, n); break;
    case 5: debris_burst_fx(pos, vel, n); break;
    case 6: puff_spawn(pos, vel, tag); break;
    case 7: smoke_puff(pos, vel); break;
    case 8: spark_shower_fx(pos, vel); break;
    case 9: laser_beam_fx(pos, vel); break;
    default: break;
    }
}

void Game::replay_effect_param(u32 kind, u32 rec) {
    cover(0x8007FCA0u);
    Frame f(*this, 0x38);
    const u32 pos = f.at(0x10), vel = f.at(0x20);
    const u32 px = m.r32(rec + 4) << 12, py = m.r32(rec + 8) << 12, pz = m.r32(rec + 12) << 12;
    m.w32(pos, px);
    m.w32(pos + 4, py);
    m.w32(pos + 8, pz);
    const u32 vx = static_cast<u32>((m.r32s(rec + 4) >> 20) << 12);
    const u32 vy = static_cast<u32>((m.r32s(rec + 8) >> 20) << 12);
    const u32 vz = static_cast<u32>((m.r32s(rec + 12) >> 20) << 12);
    m.w32(vel, vx);
    m.w32(vel + 8, vz);
    m.w32(vel + 4, vy);
    const u32 w = m.r32(rec + 16);
    if (kind == 0) {
        fireball(pos, vel, (w & 0xFFFu) | (((w >> 12) & 0xFFFu) << 16));
    } else if (kind == 1) {
        const u32 s = w & 0xFFFu;
        if (s) {
            m.w32(vel, wrap::shl(vx, s & 31u));
            m.w32(vel + 4, wrap::shl(vy, s & 31u));
            m.w32(vel + 8, wrap::shl(vz, s & 31u));
        }
        m.w32(vel, m.r32(vel) + px);
        m.w32(vel + 8, m.r32(vel + 8) + pz);
        m.w32(vel + 4, m.r32(vel + 4) + py);
        spark_add(pos, vel, (w >> 12) & 0xFFFu, m.r8(rec + 2));
    }
}

void Game::tile_redo(u32 rec) {
    cover(0x8007EB50u);
    Frame f(*this, 0x18);
    tile_set(m.r8(rec + 4), m.r8(rec + 5), static_cast<s32>(m.r32(rec) >> 23));
}

void Game::tile_undo(u32 rec) {
    cover(0x8007EB80u);
    Frame f(*this, 0x18);
    tile_set(m.r8(rec + 4), m.r8(rec + 5), static_cast<s32>((m.r32(rec) >> 14) & 0x1FFu));
}

u32 Game::replay_tile_record(u32 rec, u32 end, u32 redo) {
    cover(0x8007EBB4u);
    Frame f(*this, 0x20);
    m.w32(end, 0);
    const u32 h = m.r32(rec);
    const u32 kind = (h >> 4) & 7u;
    if (kind == 0) {
        if (!((h >> 7) & 7u)) m.w32(end, 1);
    } else if (kind == 4) {
        if (redo) tile_redo(rec);
        else tile_undo(rec);
    }
    return rec + 4 * (m.r32(rec) & 0xFu);
}

void Game::replay_tiles(u32 redo) {
    cover(0x8007EC54u);
    Frame f(*this, 0x28);
    const u32 end = f.at(0x10);
    u32 p = m.r32(m.r32(kRecorder) + 16);
    for (s32 i = 0; i < m.r32s(m.r32(kRecorder)); ++i) {
        m.w32(end, 0);
        do {
            const u32 r = m.r32(kRecorder);
            if (static_cast<s32>(p) >= m.r32s(r + 8)) p = m.r32(r + 4);
            p = replay_tile_record(p, end, redo);
        } while (!m.r32(end));
    }
}

void Game::missile_replay_show(u32 i, u32 model, u32 at) {
    cover(0x8006C6B8u);
    const u32 wp = m.r32(g::kWeapons);
    const u32 o = wp + 200 + 164 * i;
    m.w32(wp + kMissileMask, m.r32(wp + kMissileMask) | wrap::shl(1u, i & 31u));
    m.w8(o + 0x34, model);
    m.w8(o + 0x35, model >> 8);
    m.w32(o + 0x1C, m.r32(at));
    m.w32(o + 0x20, m.r32(at + 4));
    m.w32(o + 0x24, m.r32(at + 8));
    m.w16(o + 0x2C, m.r16(at + 16));
    m.w16(o + 0x2E, m.r16(at + 18));
    m.w16(o + 0x30, m.r16(at + 20));
}

void Game::missile_replay_hide(u32 i) {
    cover(0x8006C740u);
    const u32 wp = m.r32(g::kWeapons);
    m.w32(wp + kMissileMask, m.r32(wp + kMissileMask) & ~wrap::shl(1u, i & 31u));
}

void Game::bullet_replay_spawn(u32 i, u32 at, u32 vel, u32 gun) {
    cover(0x8006C760u);
    Frame f(*this, 0x60);
    const u32 mask = m.r32(g::kWeapons) + kBulletMasks
        + 4u * static_cast<u32>(static_cast<s32>(i) >> 5);
    m.w32(mask, m.r32(mask) | wrap::shl(1u, i & 31u));
    const u32 rot = f.at(0x10), end = f.at(0x30), v = f.at(0x40);
    rot_matrix_yxz(at + 16, rot);
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(rot + 4 * k));
    const u32 b = m.r32(g::kWeapons) + kBullets + 60 * i;
    static constexpr struct { u32 flags, life, tracer; } kGuns[6] = {
        {0x20410u, 37, 0x800}, {0x20820u, 24, 0x990}, {0x41040u, 62, 0x4C0},
        {0x82080u, 35, 0},     {0x94103u, 16, 0},     {0x98202u, 16, 0x800},
    };
    if (gun < 6) {
        m.w32(b, kGuns[gun].flags);
        m.w32(b + 4, kGuns[gun].life);
        m.w16(v, 0);
        m.w16(v + 2, 0);
        m.w16(v + 4, kGuns[gun].tracer);
    }
    gte.mtc2(0, m.r32(v));
    gte.mtc2(1, m.r32(v + 4));
    gte.op(0x486012);
    m.w32(end, gte.mfc2(25));
    m.w32(end + 4, gte.mfc2(26));
    m.w32(end + 8, gte.mfc2(27));
    m.w32(b + 8, m.r32(at));
    m.w32(b + 12, m.r32(at + 4));
    m.w32(b + 16, m.r32(at + 8));
    m.w32(b + 0x18, m.r32(b + 8) + (m.r32(end) << 8));
    m.w32(b + 0x1C, m.r32(b + 12) + (m.r32(end + 4) << 8));
    m.w32(b + 0x20, m.r32(b + 16) + (m.r32(end + 8) << 8));
    m.w32(b + 0x28, m.r32(vel));
    m.w32(b + 0x2C, m.r32(vel + 4));
    m.w32(b + 0x30, m.r32(vel + 8));
}

void Game::bullet_replay_kill(u32 i) {
    cover(0x8006C99Cu);
    m.w32(m.r32(g::kWeapons) + kBullets + 60 * i + 4, 0);
}

void Game::bullets_replay() {
    cover(0x8006CB10u);
    Frame f(*this, 0x18);
    bullet_replay_update();
}

void Game::debris_burst(u32 pos, u32 vel, u32 flags, u32 kind) {
    cover(0x80082504u);
    Frame f(*this, 0x28);
    if (flags & 1u) return;
    const u32 r = record_motion(4, 5, pos, vel);
    m.w8(r + 2, flags);
    m.w32(r, (m.r32(r) & 0xFFFF01FFu) | ((kind & 0x7Fu) << 9));
    debris_burst_fx(pos, vel, kind);
}

void Game::debris_burst_fx(u32 pos, u32 vel, u32 kind) {
    cover(0x800822A4u);
    Frame f(*this, 0x38);
    const s32 vx = m.r32s(vel), vy = m.r32s(vel + 4), vz = m.r32s(vel + 8);
    const u32 x = m.r32(pos) + static_cast<u32>(vx) + static_cast<u32>(((rand() & 0x3F) - 32) << 12);
    const u32 y = m.r32(pos + 4) + static_cast<u32>(vy) + static_cast<u32>(((rand() & 0x3F) - 32) << 12);
    const u32 z = m.r32(pos + 8) + static_cast<u32>(vz) + static_cast<u32>(((rand() & 0x3F) - 32) << 12);
    const s32 sx = vx >> 9, sy = vy >> 9, sz = vz >> 9;
    const s32 r = rand();
    if (kind == 1) {
        spray_spawn(pos, 512);
    } else {
        const u32 fx = m.r32(kFx);
        if (const u32 a = pool_take(fx + 1536, 3, 7)) {
            m.w32(a, 0x100000);
            m.w16(a + 0x18, 0x100);
            m.w32(a + 4, x);
            m.w32(a + 8, y);
            m.w32(a + 12, z);
            m.w16(a + 0x16, 0);
            m.w16(a + 0x1A, static_cast<u32>((r & 0xF) + 0x5A));
        }
        if (const u32 a = pool_take(m.r32(kFx) + 1536, 3, 7)) {
            m.w32(a, 0x640000);
            m.w16(a + 0x10, static_cast<u32>(-sx - 0x40 + (r & 0x7F)));
            m.w16(a + 0x12, static_cast<u32>(((r >> 3) & 0x7F) - 0xB0));
            m.w16(a + 0x16, 0x80);
            m.w16(a + 0x18, 0x210);
            m.w32(a + 4, x);
            m.w32(a + 8, y);
            m.w32(a + 12, z);
            m.w16(a + 0x14, static_cast<u32>(-sz - 0x40 + ((r >> 6) & 0x7F)));
            m.w16(a + 0x1A, static_cast<u32>((r & 0x3F) + 0x44));
        }
    }
    if (!(r & 1)) return;
    const u32 d = pool_take(m.r32(kFx) + 4248, 10, 7);
    if (!d) return;
    m.w32(d, 0x180000);
    m.w32(d + 4, x);
    m.w32(d + 8, y);
    m.w32(d + 12, z);
    const s32 q = rand();
    m.w16(d + 0x10, static_cast<u32>(-sx - 0x80 + (q & 0xFF)));
    m.w16(d + 0x12, static_cast<u32>(-sy - 0x80 + ((q >> 3) & 0xFF)));
    m.w16(d + 0x16, 0x188);
    m.w16(d + 0x14, static_cast<u32>(-sz - 0x80 + ((q >> 6) & 0xFF)));
    m.w16(d + 0x18, 0);
    m.w16(d + 0x1A, static_cast<u32>((q & 7) + 0x10));
}

void Game::view_replay() {
    cover(0x80079680u);
    Frame f(*this, 0x18);
    view_set(m.r32(m.r32(g::kWorld) + 40), 0x200, m.r32(m.r32(g::kSlots)));
    view_flag(0);
}

void Game::view_replay_origin(u32 at, u32 off) {
    cover(0x800796B8u);
    const u32 v = m.r32(m.r32(g::kWorld) + 40);
    m.w32(v + 0x10C, m.r32(at));
    m.w32(v + 0x110, m.r32(at + 4));
    m.w32(v + 0x114, m.r32(at + 8));
    m.w16(v + 0x11C, m.r16(at + 16));
    m.w16(v + 0x11E, m.r16(at + 18));
    m.w16(v + 0x120, m.r16(at + 20));
    m.w32(v + 0x124, m.r32(off));
    m.w32(v + 0x128, m.r32(off + 4));
    m.w32(v + 0x12C, m.r32(off + 8));
}

void Game::view_replay_subject(u32 at, u32 off, u32 kind) {
    cover(0x80079730u);
    if (!at || !kind) {
        m.w16(m.r32(m.r32(g::kWorld) + 40) + 0x15C, 0);
        return;
    }
    const u32 v = m.r32(m.r32(g::kWorld) + 40);
    m.w32(v + 0x134, m.r32(at));
    m.w32(v + 0x138, m.r32(at + 4));
    m.w32(v + 0x13C, m.r32(at + 8));
    m.w16(v + 0x144, m.r16(at + 16));
    m.w16(v + 0x146, m.r16(at + 18));
    m.w16(v + 0x148, m.r16(at + 20));
    m.w32(v + 0x14C, m.r32(off));
    m.w32(v + 0x150, m.r32(off + 4));
    m.w32(v + 0x154, m.r32(off + 8));
    const u32 v2 = m.r32(m.r32(g::kWorld) + 40);
    if (!m.r16s(v2 + 0x15C)) m.w16(v2 + 0x15E, 1);
    m.w16(m.r32(m.r32(g::kWorld) + 40) + 0x15C, kind);
}

void Game::trails_reset() {
    cover(0x8007DA1Cu);
    m.w32(m.r32(g::kTrails), 0);
    m.w32(m.r32(g::kTrails) + 0x12124, 0);
    for (u32 i = 0; i < 32; ++i) m.w8(m.r32(g::kTrails) + 0x12104 + i, 0xFF);
    for (u32 i = 0; i < 32; ++i) m.w8(m.r32(g::kTrails) + 0x1C228 + i, 0xFF);
}

void Game::rumble_clear() {
    cover(0x8002937Cu);
    u32 p = m.r32(kRumble);
    for (int k = 0; k < 6; ++k, p += 4) m.w8(p, 0);
}

s32 Game::disc_number() {
    cover(0x8002B504u);
    const s32 v = m.r32s(m.r32(kKcd) + 12);
    return v == 5 ? -1 : v;
}

bool Game::kcd_stream_on_this_disc(u32 id) {
    cover(0x8002CB58u);
    Frame f(*this, 0x18);
    if (static_cast<u32>(disc_number() - 1) >= 3u) return false;
    return m.r32s(kcd_entry(id)) < 0;
}

u32 Game::demo_stream(u32 demo) {
    cover(0x80056964u);
    return m.r8(kDemoStreamIndex + demo);
}

u32 Game::text_line(u32 ot, u32 p, u32 x, u32 y, u32 color, u32 str) {
    cover(0x8003563Cu);
    Frame f(*this, 0x18);
    const u32 tag = color | 0x76000000u;
    for (u32 c = m.r8(str++); c; c = m.r8(str++)) {
        const s32 t = static_cast<s32>(c) - 0x20;
        if (t) {
            const u32 uv = ((static_cast<u32>(t & 0xF) << 3) + 0x80) | ((static_cast<u32>(t >> 4) << 11) + 0x4700) |
                           0x7F980000u;
            m.w32(p + 4, tag);
            m.w32(p + 12, uv);
            m.w16(p + 8, x);
            m.w16(p + 10, y);
            m.w32(p, m.r32(ot) | 0x03000000u);
            m.w32(ot, p & 0x00FFFFFFu);
            p += 16;
            if (t == 0x2D || t == 0x37) x += 1;
            if (t == 0x3B || t == 0x3D) x -= 4;
            if (static_cast<u32>(t - 0x1A) < 2u) x -= 5;
        }
        x += 8;
    }
    return pause_mode(ot, p, 0x36);
}

u32 Game::replay_caption(u32 ot, u32 p) {
    cover(0x8003545Cu);
    Frame f(*this, 0x28);
    const u32 i = (m.r32(m.r32(g::kScene) + 40) << 10) & 0x7C00u;
    const s32 v = (static_cast<s32>(static_cast<s16>(m.r16(g::kSinCos + i))) >> 3) + 0x800;
    const u32 level = static_cast<u32>((v * 9) >> 8);
    p = pause_mode(ot, p, 0x35);
    auto text = [&](u32 x, u32 uv) {
        m.w32(f.at(0x10), uv);
        m.w32(f.at(0x14), level);
        p = pause_text(x, 28, ot, p, uv, level);
    };
    text(16, 0x800B0AD0u);
    text(45, 0x800B0AD8u);
    p = pause_mode(ot, p, 0x35);
    const s32 t = m.r32s(m.r32(g::kWorld) + 12);
    if (t < 91) return p;
    s32 fade = (t - 90) * 4;
    if (fade >= 65) fade = 64;
    const u32 color = static_cast<u32>(fade) << 8;
    m.w32(f.at(0x10), color);
    m.w32(f.at(0x14), kTextPressStart);
    return text_line(ot, p, 48, 212, color, kTextPressStart);
}

u32 Game::replay_banner(u32 ot, u32 p, s32 level) {
    cover(0x80035578u);
    Frame f(*this, 0x20);
    if (level >= 4097) level = 4096;
    if (level < 0) level = 0;
    const s32 g2 = (level * 31 * 4) >> 13;
    m.w32(p + 4, (static_cast<u32>(g2) << 8) | 0x62000000u);
    m.w16(p + 10, 0xD8);
    m.w16(p + 12, 0x140);
    m.w16(p + 8, 0);
    m.w16(p + 14, 2);
    m.w32(p, m.r32(ot) | 0x03000000u);
    m.w32(ot, p & 0x00FFFFFFu);
    p += 16;
    const u32 color = static_cast<u32>(level >> 7) << 8;
    m.w32(f.at(0x10), color);
    m.w32(f.at(0x14), kTextAceCombat3);
    return text_line(ot, p, 224, 208, color, kTextAceCombat3);
}

void Game::replay_viewer_tail() {
    const u32 sc = m.r32(g::kScene);
    if (m.r8(sc + 933)) {
        const u32 w = m.r32(g::kWorld);
        m.w32(w + 20, replay_banner(m.r32(sc + 12), m.r32(w + 20), 0x1000));
        if (m.r32(m.r32(g::kWorld)) & 0x10000000u) {
            sound_reset();
            cd_reset(0, 0);
            const u32 s2 = m.r32(g::kScene);
            if (m.r8(s2 + 933) == 1 || (m.r32(s2 + 296) & 0x10u)) {
                m.w8(s2 + 933, 0);
                phase_result = m.r32(m.r32(g::kScene) + 928) | 0x60000000u;
                return;
            }
            phase_result = 0x50000000u;
            return;
        }
    } else if (!m.r8(sc + 926)) {
        const u32 w = m.r32(g::kWorld);
        m.w32(w + 20, replay_caption(m.r32(sc + 12), m.r32(w + 20)));
    }
    phase_result = world_frame();
}

void Game::continue_menu_input() {
    cover(0x80035044u);
    const u32 w = m.r32(g::kWorld);
    m.w32(w, m.r32(w) | 0x40u);
    const u32 cur = w + 670;
    if (m.r32(m.r32(g::kScene) + 304) & 1u) m.w16(cur, m.r16(cur) - 1);
    if (m.r32(m.r32(g::kScene) + 304) & 2u) m.w16(cur, m.r16(cur) + 1);
    if (m.r16s(cur) < 0) m.w16(cur, 1);
    if (m.r16s(cur) >= 2) m.w16(cur, 0);
    if (!(m.r32(m.r32(g::kScene) + 300) & 0x10u)) return;
    const s32 c = m.r16s(cur);
    if (c == 0) m.w32(m.r32(g::kWorld), m.r32(m.r32(g::kWorld)) | 0x40000000u);
    else if (c == 1) m.w32(m.r32(g::kWorld), m.r32(m.r32(g::kWorld)) | 0x80000000u);
}

u32 Game::continue_menu_draw(u32 ot, u32 p) {
    cover(0x80035184u);
    Frame f(*this, 0x38);
    const u32 title = f.at(0x18), menu = f.at(0x20);
    m.w32(title, m.r32(0x800BCBD0u));
    m.w32(title + 4, m.r32(0x800BCBD4u));
    m.w32(menu, m.r32(0x800BCBD8u));
    m.w32(menu + 4, m.r32(0x800BCBDCu));
    const u32 cur = m.r32(g::kWorld) + 670;
    constexpr u32 kRowY = 0x800B0AF4u;
    auto row = [&](u32 k, s32 dx, u32 uv) {
        const bool lit = m.r16s(cur) == static_cast<s32>(k);
        const u32 level = lit ? 96u : 64u;
        m.w32(f.at(0x10), uv);
        m.w32(f.at(0x14), level);
        const u32 x = static_cast<u32>(m.r16s(menu) + dx);
        const u32 y = static_cast<u32>(m.r16s(menu + 2)) + m.r32(kRowY + 4 * k) + 5;
        p = pause_text(x, y, ot, p, uv, level);
        p = pause_mode(ot, p, lit ? 0x35 : 0x55);
    };
    row(0, 58, 0x800B0AB8u);
    row(1, 64, 0x800B0AC0u);
    m.w16(menu + 2, m.r16(menu + 2) + m.r16(kRowY + 4 * static_cast<u32>(m.r16s(cur))));
    p = pause_box(ot, p, menu, 1);
    p = pause_mode(ot, p, 0x55);
    p = pause_box(ot, p, title, 2);
    m.w32(f.at(0x10), 0x800B0AC8u);
    m.w32(f.at(0x14), 0xC0);
    p = pause_text(125, 90, ot, p, 0x800B0AC8u, 0xC0);
    return pause_mode(ot, p, 0x15);
}
}
