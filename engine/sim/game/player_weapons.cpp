#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPlayerWeapons = 0x800BD098u;
constexpr u32 kGunMuzzle = 0x800C7BC8u;
constexpr u32 kFx = 0x800BCF90u;
u32 P(Memory& m) { return m.r32(g::kPlayer); }
u32 CS(Memory& m) { return m.r32(kPlayerWeapons); }
}

void Game::gun_flash(u32 code) {
    cover(0x80090110u);
    Frame f(*this, 24);
    const u32 fx = m.r32(kFx);
    m.w32(fx + 14668, code);
    m.w32(fx + 14664, 1);
    record_gun_flash(code & 0xFu, static_cast<u32>(static_cast<s32>(code) >> 16));
}

u32 Game::record_gun_flash(u32 kind, u32 t) {
    cover(0x8007EFBCu);
    Frame f(*this, 32);
    const u32 r = recorder_alloc(1);
    m.w32(r, (m.r32(r) & 0xE0000000u) | 0x31u | ((kind & 0xFFu) << 9) | ((t & 0xFFFu) << 17));
    return r;
}

void Game::gun_fire(u32 gun) {
    cover(0x800713B0u);
    Frame f(*this, 0x40);
    const u32 e = slot(0);
    const u32 at = m.r32(m.r32(g::kExit) + 0xC) + 0x90;
    const u32 rot = f.at(0x10);
    rot_matrix_yxz(e + 0x18, rot);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
    gte.mtc2(0, m.r32(at));
    gte.mtc2(1, m.r32(at + 4));
    gte.op(0x486012);
    m.w32(kGunMuzzle, gte.mfc2(25));
    m.w32(kGunMuzzle + 4, gte.mfc2(26));
    m.w32(kGunMuzzle + 8, gte.mfc2(27));
    for (u32 k = 0; k < 12; k += 4) m.w32(kGunMuzzle + k, m.r32(kGunMuzzle + k) << 4);
    m.w32(kGunMuzzle, m.r32(kGunMuzzle) + m.r32(e + 8));
    m.w32(kGunMuzzle + 4, m.r32(kGunMuzzle + 4) + m.r32(e + 12));
    m.w32(kGunMuzzle + 8, m.r32(kGunMuzzle + 8) + m.r32(e + 16));
    m.w16(kGunMuzzle + 0x10, m.r16(e + 0x18));
    m.w16(kGunMuzzle + 0x12, m.r16(e + 0x1A));
    m.w16(kGunMuzzle + 0x14, m.r16(e + 0x1C));
    bullet_request(m.r32(e), kGunMuzzle, e + 0x38, gun);
}

void Game::gun_simple(u32 fn, u32 gun, u32 snd, u32 cue, u32 flash, u32 refire) {
    cover(fn);
    Frame f(*this, 24);
    if (m.r16(P(m) + 86)) return;
    m.w32(P(m) + 16, m.r32(P(m) + 16) | 0x2000u);
    gun_fire(gun);
    sound_play(snd);
    hud_alert_gun(cue);
    gun_flash(flash);
    m.w16(P(m) + 86, refire);
}

void Game::gun_rapid() {
    cover(0x8007167Cu);
    Frame f(*this, 24);
    const s32 t = m.r16s(P(m) + 86);
    if (m.r16(P(m) + 86)) {
        if (t >= 17 && !(t & 1)) gun_fire(4);
        return;
    }
    m.w32(P(m) + 16, m.r32(P(m) + 16) | 0x2000u);
    gun_flash(1048581);
    gun_fire(4);
    m.w16(P(m) + 86, 32);
    sound_play(21);
    hud_alert_gun(5);
}

bool Game::player_gun(u32 fn) {
    switch (fn) {
    case 0x800714E0u: gun_simple(fn, 0, 17, 1, 0x20000, 2); return true;
    case 0x80071544u: gun_simple(fn, 1, 18, 2, 196609, 3); return true;
    case 0x800715ACu: gun_simple(fn, 2, 19, 3, 196610, 6); return true;
    case 0x80071614u: gun_simple(fn, 3, 20, 4, 262147, 2); return true;
    case 0x8007167Cu: gun_rapid(); return true;
    case 0x80071708u: gun_simple(fn, 5, 21, 6, 262148, 3); return true;
    default: return false;
    }
}

void Game::fire_gun(u32 k) {
    cover(0x80072094u);
    Frame f(*this, 24);
    const u32 fn = m.r32(0x800B3B44u + 4 * k);
    if (fn && !player_gun(fn)) call_by_address(fn, "player gun", fn);
}

void Game::missile_mount(u32 e, u32 out) {
    cover(0x80071770u);
    Frame f(*this, 0x40);
    const u32 rot = f.at(0x10);
    const u32 at = m.r32(m.r32(g::kExit) + 0xC) + ((m.r32(CS(m) + 0x1C) & 1u) << 3) + 0x98;
    rot_matrix_yxz(e + 0x18, rot);
    for (u32 r = 0; r < 5; ++r) gte.ctc2(r, m.r32(rot + 4 * r));
    gte.mtc2(0, m.r32(at));
    gte.mtc2(1, m.r32(at + 4));
    gte.op(0x486012);
    m.w32(out, gte.mfc2(25));
    m.w32(out + 4, gte.mfc2(26));
    m.w32(out + 8, gte.mfc2(27));
    const u32 x = m.r32(out) << 4, z = m.r32(out + 8) << 4;
    m.w32(out, x);
    m.w32(out + 8, z);
    m.w32(out + 4, m.r32(out + 4) << 4);
    m.w32(out, m.r32(out) + m.r32(e + 8));
    m.w32(out + 4, m.r32(out + 4) + m.r32(e + 12));
    m.w32(out + 8, m.r32(out + 8) + m.r32(e + 16));
    m.w16(out + 0x10, m.r16(e + 0x18));
    m.w16(out + 0x12, m.r16(e + 0x1A));
    m.w16(out + 0x14, m.r16(e + 0x1C));
}

u32 Game::launch_68FA8(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068FA8u);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 0x4000);
    m.w8(o + 5, 4);
    m.w8(o + 7, 3);
    const u32 fl = m.r32(o);
    m.w8(o + 52, 0);
    m.w16(o + 24, 0);
    m.w32(o + 56, 0);
    m.w32(o + 60, 0);
    m.w32(o + 64, 0);
    m.w32(o + 156, 1);
    m.w32(o + 160, 1);
    m.w32(o, fl | 0x20u);
    return o;
}

u32 Game::launch_68F34(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068F34u);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 2048);
    m.w8(o + 5, 4);
    m.w8(o + 7, 2);
    m.w32(o + 156, 16);
    const u32 fl = m.r32(o);
    m.w8(o + 52, 0);
    m.w16(o + 24, 0);
    m.w32(o + 56, 0);
    m.w32(o + 60, 0);
    m.w32(o + 64, 0);
    m.w32(o + 160, 1);
    m.w32(o, fl | 0x20u);
    return o;
}

u32 Game::launch_bomb(u32 fn, u32 owner, u32 pos, u32 vel, u32 target, u32 dmg, u32 kind7, u32 blast, bool extra_flag) {
    cover(fn);
    Frame f(*this, 24);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    const s32 vx = m.r32s(o + 56);
    m.w16(o + 26, dmg);
    m.w8(o + 52, 4);
    m.w8(o + 5, 2);
    m.w8(o + 7, kind7);
    m.w16(o + 24, 240);
    m.w32(o + 156, blast);
    m.w32(o + 160, 192);
    const s32 vy = m.r32s(o + 60);
    m.w8(o + 6, 0);
    m.w32(o + 56, static_cast<u32>(vx + (vx >> 1)));
    m.w32(o + 60, static_cast<u32>(vy + (vy >> 1)));
    const s32 vz = m.r32s(o + 64);
    const u32 fl = m.r32(o) | 0x10u;
    m.w32(o, fl);
    m.w32(o + 64, static_cast<u32>(vz + (vz >> 1)));
    if (extra_flag) m.w32(o, fl | 0x40u);
    return o;
}

u32 Game::launch_dispenser(u32 owner, u32 pos, u32 vel, u32 target) {
    cover(0x80068D0Cu);
    Frame f(*this, 32);
    const u32 o = weapon_alloc(owner, pos, vel, static_cast<s32>(target));
    if (!o) return 0;
    m.w16(o + 26, 256);
    m.w8(o + 52, 2);
    m.w8(o + 6, 3);
    m.w8(o + 5, 1);
    m.w16(o + 24, 150);
    m.w16(o + 124, 30);
    m.w16(o + 126, 7);
    m.w32(o + 132, 4608);
    const u32 b53 = m.r8(o + 53);
    m.w32(o + 128, 294912);
    m.w8(o + 7, 0);
    m.w32(o + 136, 2560);
    m.w8(o + 53, b53 | 0x18u);
    if (target != 0xFFFFFFFFu) {
        const s32 d = distance(pos, slot(target & 0x3Fu) + 8);
        const u32 splitRange = (target & 0x20u)
            ? static_cast<u32>((d >> 2) + (d >> 1))
            : static_cast<u32>(static_cast<s32>(972u * static_cast<u32>(d)) >> 10);
        m.w32(o + 152, splitRange);
        m.w32(o + 104, static_cast<u32>(d));
    }
    return o;
}

void Game::laser_beam(u32 pos, u32 vel) {
    cover(0x8008350Cu);
    Frame f(*this, 32);
    record_motion(4, 9, pos, vel);
    laser_beam_fx(pos, vel);
}

void Game::laser_beam_fx(u32 pos, u32 vel) {
    cover(0x800833C8u);
    Frame g2(*this, 64);
    const u32 fx = m.r32(kFx);
    const u32 b = fx + 15144;
    m.w32(b, 1);
    m.w16(b + 6, 0);
    const s32 vz = m.r32s(vel + 8);
    const s32 x = m.r32s(pos) + 8 * m.r32s(vel);
    const s32 y = m.r32s(pos + 4) + 8 * m.r32s(vel + 4);
    const s32 z0 = m.r32s(pos + 8);
    m.w32(b + 8, static_cast<u32>(x));
    m.w32(b + 12, static_cast<u32>(y));
    const s32 z = z0 + 8 * vz;
    m.w32(b + 20, static_cast<u32>(x - (x >> 8)));
    m.w32(b + 24, static_cast<u32>(y - 40960000));
    m.w32(b + 28, static_cast<u32>(z - (z >> 8)));
    m.w32(b + 32, static_cast<u32>(x + (x >> 8)));
    m.w32(b + 36, static_cast<u32>(y + 40960000));
    m.w32(b + 16, static_cast<u32>(z));
    m.w32(b + 40, static_cast<u32>(z + (z >> 8)));
    const bool miss = terrain_raycast(fx + 15164, fx + 15176, g2.at(0x10), g::kScratch) == 0x7FFFFFFF;
    if (!miss) {
        m.w32(b + 32, m.r32(g2.at(0x10)));
        m.w32(b + 36, m.r32(g2.at(0x14)));
        m.w32(b + 40, m.r32(g2.at(0x18)));
    }
    m.w32(b + 20, static_cast<u32>(x - 8 * (x >> 8)));
    m.w32(b + 24, static_cast<u32>(y - 327680000));
    m.w32(b + 28, static_cast<u32>(z - 8 * (z >> 8)));
}

void Game::fire_missile(u32 fn, u32 racks, u32 reload, u32 snd, u32 cue, u32 frame, u32 mount_at, u32 which) {
    cover(fn);
    Frame f(*this, frame);
    if (!m.r32(CS(m) + 28)) return;
    if (!slot_take(racks, m.r32s(CS(m) + 4), reload)) return;
    m.w32(P(m) + 16, m.r32(P(m) + 16) | 0x4000u);
    m.w32(CS(m) + 28, m.r32(CS(m) + 28) - 1);
    const u32 e = slot(0);
    sound_play(snd);
    hud_alert_launch(cue);
    const u32 mount = f.at(mount_at);
    missile_mount(e, mount);
    const u32 owner = m.r32(e), vel = e + 0x38, target = m.r32(CS(m) + 24);
    switch (which) {
    case 0: spawn_weapon(owner, mount, vel, static_cast<s32>(target), 65794); break;
    case 1: launch_sam_12(owner, mount, vel, target); break;
    case 2: launch_sam_13(owner, mount, vel, target); break;
    case 3: launch_dispenser(owner, mount, vel, target); break;
    case 4: launch_bomb(0x80068A58u, owner, mount, vel, target, 512, 3, 1024, false); break;
    case 5: launch_bomb(0x80068AFCu, owner, mount, vel, target, 1024, 1, 1260, true); break;
    case 6: launch_rocket_18(owner, mount, vel, target); break;
    case 7: launch_rocket_19(owner, mount, vel, target); break;
    case 8: spawn_weapon(owner, mount, vel, static_cast<s32>(target), 117506306); break;
    default: break;
    }
}

bool Game::player_special(u32 fn, u32 racks) {
    switch (fn) {
    case 0x80071950u: fire_missile(fn, racks, 150, 14, 1, 56, 0x18, 0); return true;
    case 0x80071AB0u: fire_missile(fn, racks, 75, 14, 2, 48, 0x10, 1); return true;
    case 0x80071B54u: fire_missile(fn, racks, 150, 16, 3, 48, 0x10, 2); return true;
    case 0x80071BF8u: fire_missile(fn, racks, 250, 14, 4, 48, 0x10, 3); return true;
    case 0x80071C9Cu: fire_missile(fn, racks, 180, 16, 5, 48, 0x10, 4); return true;
    case 0x80071D40u: fire_missile(fn, racks, 180, 15, 6, 48, 0x10, 5); return true;
    case 0x80071EBCu: fire_missile(fn, racks, 90, 14, 8, 48, 0x10, 6); return true;
    case 0x80071F60u: fire_missile(fn, racks, 180, 16, 9, 48, 0x10, 7); return true;
    case 0x80071A00u: fire_missile(fn, racks, 150, 26, 10, 56, 0x18, 8); return true;
    case 0x80071DE4u: {
        cover(fn);
        Frame g2(*this, 24);
        if (!m.r32(CS(m) + 28) || m.r32(CS(m) + 24) == 0xFFFFFFFFu) return true;
        if (!slot_take(racks, m.r32s(CS(m) + 4), 180)) return true;
        m.w32(P(m) + 16, m.r32(P(m) + 16) | 0x4000u);
        m.w32(CS(m) + 28, m.r32(CS(m) + 28) - 1);
        const u32 e = slot(0);
        sound_play(25);
        hud_alert_launch(7);
        launch_68F34(m.r32(e), e + 8, e + 0x38, m.r32(CS(m) + 24));
        const u32 t = slot(m.r32(CS(m) + 24) & 0x3Fu);
        laser_beam(t + 8, t + 56);
        return true;
    }
    case 0x80072004u: {
        cover(fn);
        Frame g2(*this, 24);
        if (!m.r32(CS(m) + 28)) return true;
        if (!slot_take(racks, m.r32s(CS(m) + 4), 180)) return true;
        m.w32(P(m) + 16, m.r32(P(m) + 16) | 0x4000u);
        m.w32(CS(m) + 28, m.r32(CS(m) + 28) - 1);
        const u32 e = slot(0);
        sound_play(88);
        launch_68FA8(m.r32(e), e + 8, e + 0x38, m.r32(CS(m) + 24));
        return true;
    }
    default: return false;
    }
}

void Game::fire_special(u32 k, u32 racks) {
    cover(0x800720D4u);
    Frame f(*this, 24);
    const u32 fn = m.r32(0x800B3B60u + 4 * k);
    if (fn && !player_special(fn, racks)) call_by_address(fn, "player special weapon", racks);
}
}
