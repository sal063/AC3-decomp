#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBlips = 0x800C67A0u;
constexpr u32 kMarks = 0x800C6A60u;
constexpr u32 kMissileMarks = 0x800C68E0u;
constexpr u32 kAlerts = 0x800C6ECCu;
constexpr u32 kHudTimer = 0x800C6D60u;
constexpr u32 kHudBlink = 0x800C6D66u;
constexpr u32 kHudColor = 0x800C6D68u;
constexpr u32 kLockBlocked = 0x800C6D70u;
constexpr u32 kLockedOn = 0x800C6D72u;
constexpr u32 kRadarRange = 0x800C6D7Cu;
constexpr u32 kLock = 0x800BCEC8u;
constexpr u32 kLockBox = 0x800BCECCu;
constexpr u32 kHold = 0x800BCFD4u;
constexpr u32 kHoldCounter = 0x800BCFD0u;
constexpr u32 kWarnHold = 0x800BCFC0u;
constexpr u32 kPanel = 0x800C7170u;
constexpr u32 kWarning = 0x800C7180u;
constexpr u32 kMessages = 0x800BCFECu;
constexpr u32 kMessagePrims = 0x800BCFE8u;
constexpr u32 kFace = 0x800BCE88u;
constexpr u32 kFaceWord = 0x800BCE90u;
constexpr u32 kCdRing = 0x800BCBBCu;
constexpr u32 kPlayerWeapons = 0x800BD098u;
constexpr u32 kGuidance = 0x800BD0A0u;
constexpr u32 kRadarCenter = 0x1F800100u;
constexpr u32 kSinTable = 0x800104ACu;

u32 world_of(Memory& m) { return m.r32(g::kWorld); }
u32 scene_of(Memory& m) { return m.r32(g::kScene); }
bool paused(Memory& m) { return (m.r32(m.r32(g::kScene)) & 0x20000000u) != 0; }
u32 flags_of(u32 k) { return g::kSlotFlags + 4 * k; }
u32 rel_of(Memory& m, u32 k) { return m.r32(g::kRelation + 4 * k); }
}

s32 Game::blip_size(s32 z) {
    cover(0x8003B758u);
    if (z >= 4097) return 2;
    const s32 d = 4096 - z;
    s32 v = (3 * ((d * d) >> 12)) >> 9;
    if (v < 2) return 2;
    return v < 25 ? v : 24;
}

void Game::hud_blips() {
    cover(0x8003B34Cu);
    Frame f(*this, 0x68);
    const u32 mat = f.at(0x10), v = f.at(0x30), p = f.at(0x38), flag = f.at(0x3C);
    const u32 w = world_of(m);
    set_rot_matrix(w + 76);
    m.w32(mat + 20, 0);
    m.w32(mat + 24, 0);
    m.w32(mat + 28, 0);
    set_trans_matrix(mat);
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        const u32 rel = rel_of(m, k);
        const u32 blip = m.r32(kBlips + 4 * k);
        if (!(m.r32(e) & 0x4000u) || !rel || !blip) continue;
        m.w32(rel + 32, 0);
        const u32 cam = m.r32(world_of(m) + 40);
        const s32 dx = (m.r32s(e + 8) - m.r32s(cam + 4)) >> 12;
        if (static_cast<u32>(dx + 0x8000) > 0xFFFFu) continue;
        m.w16(v, static_cast<u32>(dx));
        const s32 dy = (m.r32s(e + 12) - m.r32s(cam + 8)) >> 12;
        if (static_cast<u32>(dy + 0x8000) > 0xFFFFu) continue;
        m.w16(v + 2, static_cast<u32>(dy));
        const s32 dz = (m.r32s(e + 16) - m.r32s(cam + 12)) >> 12;
        if (static_cast<u32>(dz + 0x8000) > 0xFFFFu) continue;
        m.w16(v + 4, static_cast<u32>(dz));
        m.w32(rel + 32, m.r32(rel + 32) | 0x10000u);
        const s32 z = rot_trans_pers(v, blip + 8, p, flag);
        m.w16(blip + 6, static_cast<u32>(blip_size(z)));
        if (m.r32s(flag) >= 0) {
            const u32 fl = m.r32(rel + 32) | 0x40000u;
            m.w32(rel + 32, fl);
            u32 left = 0, across = 0x141u;
            if (hints && hints->wide) {
                const float half = 160.0f / hints->squeeze;
                left = static_cast<u32>(static_cast<s32>(160.0f - half));
                across = static_cast<u32>(2.0f * half) + 1u;
            }
            if (static_cast<u32>(m.r16(blip + 8) - left) % 0x10000u < across &&
                static_cast<u32>(m.r16s(blip + 10)) <= 0xF0u)
                m.w32(rel + 32, fl | 0x20000u);
        }
    }
    for (u32 k = 2; k < 64; ++k) {
        const u32 rel = rel_of(m, k);
        const u32 fl = flags_of(k);
        if (!(m.r32(slot(k)) & 0x4000u)) continue;
        if (!(m.r32(rel + 32) & 0x20000u) || !(m.r32(world_of(m) + 4) & 4u)) {
            m.w32(rel + 36, 0);
            continue;
        }
        if (!paused(m)) m.w32(rel + 36, m.r32(rel + 36) + 1);
        const s32 due = m.r16(scene_of(m) + 908) == 6 ? 0 : 90;
        const bool ground = k >= 32;
        auto script = [&](u32 air_off, u32 ground_off) {
            return ground ? m.r32(m.r32(g::kGroundRecords) + 40 * (k - 32) + ground_off)
                          : m.r32(m.r32(g::kSlotRecords) + 64 * k + air_off);
        };
        if (!(m.r32(fl) & 0x1000000u) && m.r32s(rel + 36) >= due) {
            m.w32(fl, m.r32(fl) | 0x1000000u);
            if (const u32 s = script(56, 32)) msn_vm_exec(s);
        }
        if (m.r32s(world_of(m) + 8) > 0 && !(m.r32(fl) & 0x2000000u) && m.r32s(rel + 36) >= due) {
            m.w32(fl, m.r32(fl) | 0x2000000u);
            if (const u32 s = script(60, 36)) msn_vm_exec(s);
        }
    }
}

u32 Game::lock_start(u32 k) {
    cover(0x800409D4u);
    if (m.r8(kLock)) return 0;
    m.w8(kLock, 1);
    m.w32(kLockBox, 0x007800A0u);
    m.w8(kLock + 1, k);
    m.w16(kLock + 2, 2);
    return 1;
}

u32 Game::lock_clear() {
    cover(0x80040A18u);
    m.w8(kLock, 0);
    return 255;
}

void Game::hud_lock_targets() {
    cover(0x8003F2CCu);
    Frame f(*this, 40);
    m.w8(kLockedOn, 0xFF);
    for (u32 k = 2; k < 64; ++k) {
        const u32 blip = m.r32(kBlips + 4 * k);
        const u32 rel = rel_of(m, k);
        if (!blip) continue;
        m.w8(blip, 0);
        if (!rel) continue;
        const u32 fl = m.r32(flags_of(k));
        if (!(m.r32(slot(k)) & 0x4000u) || !(fl & 0x1000u) || !m.r8(blip + 2) || !(m.r32(rel + 32) & 0x20000u) ||
            !(m.r32(rel) & 1u))
            continue;
        m.w8(blip, 1);
        if (m.r8(g::kCurrentTarget) == k) {
            m.w8(blip, 2);
            if (!(m.r8(slot(k)) & 0x40u) && (m.r32(rel) & 8u) && (m.r32(flags_of(k)) & 0x4000u) &&
                !(m.r8(m.r32(g::kMission)) & 4u) && !m.r8(kLockBlocked)) {
                if (m.r8(blip + 1) && m.r8(kLock + 1) == k) {
                    if (m.r8(kLock) == 2) {
                        m.w8(blip, 3);
                        m.w8(kLockedOn, k);
                    } else if (!m.r8(kLock)) {
                        m.w8(blip + 1, 0);
                    }
                } else {
                    m.w8(blip + 1, lock_start(k));
                }
            }
        } else if (m.r8(blip + 1) && m.r8(kLock + 1) == k) {
            m.w8(blip + 1, lock_clear());
        }
    }
}

void Game::lock_box_update() {
    cover(0x80040A24u);
    Frame f(*this, 32);
    const u32 k = m.r8(kLock + 1);
    const u32 blip = m.r32(kBlips + 4 * k);
    if (!blip || !m.r8(kLock)) return;
    if (!m.r8(blip)) {
        lock_clear();
        return;
    }
    if (!(m.r32(flags_of(k)) & 1u)) {
        m.w8(kLock, 0);
        return;
    }
    const u32 rel = rel_of(m, k);
    if (!rel) return;
    if (!(m.r32(rel) & 8u)) {
        m.w8(kLock, 0);
        return;
    }
    if (m.r8(kLock) == 1 && !(m.r32(world_of(m) + 12) & 7u)) voice_for_repeat(45);
    if (m.r8(kLock) == 2) {
        if (!(m.r32(world_of(m) + 12) & 7u)) voice_for_repeat(50);
        if (m.r8(kLock) == 2) {
            m.w32(kLockBox, m.r32(blip + 8));
            m.w16(kLock + 2, m.r16(blip + 6));
            return;
        }
    }
    auto arrived = [&](s32 dy) {
        if (m.r8(kLock) != 2) {
            const s32 mission = m.r16s(scene_of(m) + 908);
            if ((mission == 4 || mission == 8 || mission == 15 || mission == 30) && (m.r32(slot(m.r8(kLock + 1))) & 0x100u))
                message_show(14);
        }
        const u32 t = m.r8(kLock + 1);
        if (m.r32(flags_of(t)) & 0x800000u)
            radio_call(8, 0, m.r8(m.r32(m.r32(g::kSlotRecords) + (t << 6) + 16) + 3));
        (void)dy;
        m.w8(kLock, 2);
    };
    const s32 dy = m.r16s(blip + 10) - m.r16s(kLockBox + 2);
    const s32 dx = m.r16s(blip + 8) - m.r16s(kLockBox);
    const s32 ds = m.r16s(blip + 6) - m.r16s(kLock + 2);
    if (m.r16s(blip + 8) == m.r16s(kLockBox) && !dy && !ds) {
        arrived(dy);
        return;
    }
    const s32 step = m.r16s(kLock + 2);
    u32 x;
    if (step >= dx) x = dx >= -step ? m.r16(blip + 8) : m.r16(kLockBox) - m.r16(kLock + 2);
    else x = m.r16(kLockBox) + m.r16(kLock + 2);
    m.w16(kLockBox, x);
    u32 y;
    if (step >= dy) y = dy >= -step ? m.r16(kLockBox + 2) + static_cast<u32>(dy) : m.r16(kLockBox + 2) - m.r16(kLock + 2);
    else y = m.r16(kLockBox + 2) + m.r16(kLock + 2);
    m.w16(kLockBox + 2, y);
    u32 s;
    if (ds < 5) s = ds >= -4 ? m.r16(kLock + 2) + static_cast<u32>(ds) : m.r16(kLock + 2) - 4u;
    else s = m.r16(kLock + 2) + 4u;
    m.w16(kLock + 2, s);
    const s32 dy2 = m.r16s(blip + 10) - m.r16s(kLockBox + 2);
    if (m.r16s(blip + 8) == m.r16s(kLockBox) && !dy2 && m.r16s(blip + 6) == static_cast<s16>(s)) arrived(dy2);
}

void Game::hud_lock() {
    cover(0x8003EF40u);
    Frame f(*this, 24);
    const u32 a = m.r8(scene_of(m) + 938);
    bool blocked = a - 5u < 2u || a == 9;
    if (!blocked && target_is_lockable()) {
        const u32 b = m.r8(scene_of(m) + 939);
        blocked = b - 5u < 2u || b == 9;
    }
    m.w8(kLockBlocked, blocked);
    hud_lock_targets();
    lock_box_update();
}

void Game::radio_hold_update() {
    cover(0x80041698u);
    Frame f(*this, 24);
    if (paused(m)) return;
    const u32 st = m.r8(kHold);
    if (st == 1) {
        if (!m.r16(kHold + 2)) sound_play(62);
        m.w16(kHold + 2, m.r16(kHold + 2) + 16u);
        if (m.r16s(kHold + 2) >= 200) {
            m.w8(kHold, 2);
            m.w16(kHold + 2, 200);
            m.w32(kHoldCounter, 0);
        }
    } else if (st == 3) {
        if (m.r16(kHold + 2) == 200) sound_play(54);
        m.w16(kHold + 2, m.r16(kHold + 2) - 32u);
        if (m.r16s(kHold + 2) <= 0) {
            m.w16(kHold + 2, 0);
            m.w8(kHold, 0);
        }
    }
}

void Game::hud_panel_update() {
    cover(0x800419C0u);
    if (paused(m)) return;
    const u32 st = m.r8(kPanel + 2);
    if (st == 1) {
        switch (m.r16(kPanel)) {
        case 0: m.w8(kPanel + 6, 43); m.w8(kPanel + 7, 6); break;
        case 1: m.w8(kPanel + 6, 12); m.w8(kPanel + 7, 41); break;
        case 2: m.w8(kPanel + 6, 21); m.w8(kPanel + 7, 20); break;
        case 3:
            m.w8(kPanel + 6, 43);
            m.w8(kPanel + 7, 41);
            m.w8(kPanel + 2, 2);
            break;
        default: break;
        }
        m.w16(kPanel, m.r16(kPanel) + 1u);
        return;
    }
    if (st == 3) {
        m.w8(kPanel + 6, 43);
        const u32 hgt = m.r8(kPanel + 7) >> 2;
        m.w8(kPanel + 7, hgt);
        if (!hgt) m.w8(kPanel + 2, 0);
    }
}

void Game::hud_update() {
    cover(0x8003A2B8u);
    Frame f(*this, 24);
    hud_blips();
    hud_lock();
    radio_hold_update();
    hud_panel_update();
    if (m.r32(world_of(m)) & 1u) warning_reset();
    const u32 ms = m.r32(g::kMission);
    if (m.r8(ms + 48) != 1 || radio_busy()) return;
    const u32 ms1 = m.r32(g::kMission);
    const u32 kind = m.r8(ms1 + 49);
    if (kind == 1 || kind == 2) {
        const s32 end = m.r16s(ms1 + 50), now = m.r16s(ms1 + 26);
        const s32 level = kind == 1 ? wrap::div(1365 * (end - now), end).lo : wrap::div(now << 12, end).lo;
        const u32 w = world_of(m);
        const u32 p = fade_overlay(m.r32(w + 16), m.r32(w + 20), level);
        m.w32(world_of(m) + 20, p);
    }
    const u32 ms2 = m.r32(g::kMission);
    const s32 end = m.r16s(ms2 + 50);
    const s16 t = static_cast<s16>(m.r16(ms2 + 26) + 1u);
    m.w16(ms2 + 26, static_cast<u32>(t));
    if (!(t < end)) m.w8(ms2 + 48, 0);
}

void Game::hud_colors(u32 hud) {
    cover(0x8003A12Cu);
    if (m.r32(hud) & 7u) m.w16(kHudBlink, 15);
    if (m.r32(kAlerts) & 2u) {
        m.w8(kHudColor + 7, 1);
    } else if (m.r16s(kHudBlink) <= 0) {
        m.w8(kHudColor + 7, 0);
    } else {
        m.w8(kHudColor + 7, 2);
        m.w16(kHudBlink, m.r16(kHudBlink) - 1u);
    }
    auto alarm = [&](u32 c3, u32 c0) {
        m.w8(kHudColor + 3, c3);
        m.w8(kHudColor + 4, 0);
        m.w8(kHudColor + 5, 0);
        m.w8(kHudColor, c0);
        m.w8(kHudColor + 1, 0);
        m.w8(kHudColor + 2, 0);
        m.w8(kHudColor + 9, 1);
    };
    const u32 mode = m.r8(kHudColor + 7);
    if (mode == 1) {
        alarm(0x83, 255);
        return;
    }
    if (mode == 2 && m.r32s(kHudTimer) % 6 < 4) {
        alarm(67, 131);
        return;
    }
    m.w8(kHudColor + 4, 0x83);
    m.w8(kHudColor + 3, 0);
    m.w8(kHudColor + 5, 0);
    m.w8(kHudColor, 0);
    m.w8(kHudColor + 1, 0xFF);
    m.w8(kHudColor + 2, 0);
    m.w8(kHudColor + 9, 0);
}

void Game::hud_name_tag() {
    cover(0x8003A68Cu);
    Frame f(*this, 0x38);
    constexpr u32 kName = 0x800C7150u, kRivalName = 0x800BCC1Cu, kPointLabel = 0x800BCC24u;
    const u32 color = (static_cast<u32>(m.r8(kHudColor + 5) >> 1) << 16) |
                      (static_cast<u32>(m.r8(kHudColor + 4) >> 1) << 8) | (m.r8(kHudColor + 3) >> 1);
    const u32 digit = f.at(0x18);
    auto line = [&](u32 x, u32 y, u32 str) {
        m.w32(f.at(0x10), color);
        m.w32(f.at(0x14), str);
        const u32 w = world_of(m);
        const u32 p = text_line(m.r32(w + 16), m.r32(w + 20), x, y, color, str);
        m.w32(world_of(m) + 20, p);
    };
    auto two_digits = [&](u32 y, u32 value_at) {
        m.w8(digit, m.r8(m.r32(g::kMission) + value_at) / 10u + 48u);
        line(296, y, digit);
        m.w8(digit, m.r8(m.r32(g::kMission) + value_at) % 10u + 48u);
        line(304, y, digit);
    };
    if (m.r16s(scene_of(m) + 908) == 30) {
        const u32 w = world_of(m);
        if (!m.r32(w + 8) && !m.r32(w + 12)) {
            for (u32 i = 0; i < 16; ++i) {
                u32 c = m.r8(scene_of(m) + i + 2376);
                if (c - 97u < 26u) c -= 32;
                m.w8(kName + i, c);
            }
        }
        if (m.r16s(scene_of(m) + 908) == 30 &&
            (!m.r8(m.r32(g::kMission) + 61) || (m.r32(world_of(m) + 4) & 2u))) {
            line(222, 80, kName);
            m.w8(digit + 1, 0);
            two_digits(80, 46);
            line(222, 88, kRivalName);
            two_digits(88, 45);
        }
    }
    if (m.r16s(scene_of(m) + 908) != 3) return;
    const u32 st = m.r8(m.r32(g::kMission) + 61);
    if (st && st != 3 && !(m.r32(world_of(m) + 4) & 2u)) return;
    m.w8(digit + 1, 0);
    line(272, 80, kPointLabel);
    u32 x = 304;
    for (s32 v = m.r32s(m.r32(g::kMission) + 4); v;) {
        const s32 q = v / 10;
        m.w8(digit, static_cast<u32>(v - 10 * q + 48));
        line(x, 88, digit);
        x -= 8;
        v = q;
    }
}

u32 Game::alert_blink(u32 a) {
    cover(0x80041F88u);
    Frame f(*this, 32);
    constexpr u32 kKinds = 0x800B1254u, kGlyphs = 0x800B1424u;
    constexpr u32 kWarnBlocks = 0x800BCFC4u, kWarnPrims = 0x800BCFC8u;
    const u32 ot = m.r32(world_of(m) + 16);
    if (!m.r8(a) || m.r8(a + 1) == 255) return 0;
    const u32 t = kKinds + 16 * m.r8(a + 1);
    if (!paused(m)) {
        if (m.r16s(a + 2) == m.r16s(t + 14) && message_shown() == 255) sound_play(static_cast<u32>(m.r16s(t + 12)));
        const s16 n = static_cast<s16>(m.r16(a + 2) + 1u);
        m.w16(a + 2, static_cast<u32>(n));
        if (n >= m.r16s(t + 2)) m.w8(a, 0);
    }
    const u32 caption = m.r8(t) & 0xF0u, blink = m.r8(t) & 0xFu;
    if (blink == 1) {
        if (!(m.r16(a + 2) & 6u)) return m.r8(a);
    } else if (blink == 2 && !(m.r16(a + 2) & 2u)) {
        return m.r8(a);
    }
    auto sprite_done = [&](u32 p) {
        ot_link(ot, p);
        const u32 blocks = m.r32(kWarnBlocks);
        m.w32(blocks + 8, m.r32(blocks + 8) + 1);
    };
    auto glyph = [&](u32 p, u32 g_index, u32 dx) {
        pkt_rgb(p, 80, 80, 80);
        m.w16(p + 14, 32667);
        m.w16(p + 8, m.r16(t + 4) + dx);
        const u32 g = kGlyphs + 4 * g_index;
        m.w16(p + 10, m.r16(t + 6) + 2u);
        m.w8(p + 12, m.r8(g));
        m.w8(p + 13, m.r8(g + 1));
        m.w16(p + 16, m.r8(g + 2));
        m.w16(p + 18, m.r8(g + 3));
        sprite_done(p);
    };
    auto box_colour = [&](u32 p) {
        switch (m.r8(t + 1)) {
        case 0: pkt_rgb(p, 0x80, 0, 0); break;
        case 1: pkt_rgb(p, 0x80, 0x40, 0); break;
        case 2: pkt_rgb(p, 0, 0, 0x80); break;
        default: pkt_rgb(p, 0xFF, 0xFF, 0xFF); break;
        }
    };
    auto mode = [&](u32 q, u32 code) {
        m.w32(q + 4, code);
        m.w8(q + 3, 1);
        ot_link(ot, q);
        const u32 blocks = m.r32(kWarnBlocks);
        m.w32(blocks + 12, m.r32(blocks + 12) + 1);
    };
    u32 p = m.r32(kWarnPrims) + 20 * m.r32(m.r32(kWarnBlocks) + 8) + 104;
    glyph(p, m.r8(t + 8), m.r8(t + 9));
    if (m.r8(t + 10) != 255) {
        p += 20;
        glyph(p, m.r8(t + 10), m.r8(t + 11));
    }
    if (caption) {
        p += 20;
        box_colour(p);
        m.w16(p + 14, 32667);
        m.w16(p + 8, m.r16(t + 4) + 28u);
        m.w16(p + 10, caption == 16 ? m.r16(t + 6) - 10u : m.r16(t + 6) + 10u);
        m.w8(p + 12, 28);
        m.w16(p + 16, 74);
        m.w8(p + 13, 0);
        m.w16(p + 18, 9);
        sprite_done(p);
    }
    const u32 q = m.r32(kWarnPrims) + 8 * m.r32(m.r32(kWarnBlocks) + 12) + 424;
    mode(q, 0xE1000636u);
    const u32 box = p + 20;
    box_colour(box);
    m.w16(box + 14, 32667);
    m.w16(box + 8, m.r16(t + 4));
    m.w8(box + 12, 0);
    m.w8(box + 13, 10);
    m.w16(box + 16, 132);
    m.w16(box + 18, 9);
    m.w16(box + 10, m.r16(t + 6));
    sprite_done(box);
    mode(q + 8, 0xE1000616u);
    return m.r8(a);
}

void Game::warning_select(u32 hud) {
    cover(0x80042544u);
    Frame f(*this, 40);
    const u32 p = slot(0);
    bool low = false;
    bool raise = true;
    u32 k = 0;
    const s32 mission = m.r16s(scene_of(m) + 908);
    const u32 pr = m.r32(0x800BCEB0u);
    if (m.r32(kAlerts) & 2u) {
        k = 1;
    } else if (m.r32(kAlerts) & 1u) {
        k = 2;
    } else if (m.r32(hud) & 7u) {
        warning_raise(0);
        if (m.r32(hud) & 2u) radio_call(1, 60, 0);
        if (m.r32(hud) & 4u) radio_call(2, 60, 0);
        raise = false;
    } else if (ai_out_of_area(0) == 1 && mission != 32 && mission != 47 && mission != 53) {
        k = 3;
    } else if (m.r32s(p + 12) < m.r32s(pr + 8)) {
        k = 4;
    } else if (m.r32(hud) & 0x18u) {
        k = 5;
    } else if (m.r32s(hud + 48) < -m.r32s(pr + 12) ||
               ((mission == 42 || mission == 58) && m.r32s(pr + 12) < m.r32s(p + 12))) {
        m.w8(kWarnHold, m.r8(kWarnHold) + 1u);
        low = true;
        raise = m.r8(kWarnHold) >= 0x1Eu;
        k = 6;
    } else if (!m.r32(hud + 28) && (m.r32(scene_of(m) + 300) & 0x1000u)) {
        k = 7;
    } else if (m.r32(m.r32(g::kMission)) & 8u) {
        k = 8;
    } else if (m.r32s(scene_of(m) + 296) < 0) {
        if (m.r32(scene_of(m) + 300) & 0x80000000u) sound_play(55);
        k = 9;
    } else if (!(m.r32(m.r32(g::kMission)) & 0x10u)) {
        raise = false;
    } else if (mission == 3 && !m.r32(world_of(m) + 8)) {
        k = 10;
    } else {
        raise = false;
    }
    if (raise) warning_raise(k);
    if (!low) m.w8(kWarnHold, 0);
    const u32 ms = m.r32(g::kMission);
    if (m.r32(hud + 28)) m.w32(ms, m.r32(ms) & ~0x20u);
    else m.w32(ms, m.r32(ms) | 0x20u);
    if (m.r32s(kWarning) == -1) return;
    const u32 a = alert_blink(kWarning + 8);
    const u32 b = alert_blink(kWarning + 12);
    if (!paused(m)) m.w32(kWarning + 4, m.r32(kWarning + 4) - 1);
    if (!a && !b && !m.r32(kWarning + 4)) m.w32(kWarning, 0xFFFFFFFFu);
}

void Game::hud_warnings(u32 hud) {
    cover(0x800413FCu);
    Frame f(*this, 24);
    hud_warn_begin();
    const u32 sub = m.r32(world_of(m) + 4);
    if ((sub & 4u) && !paused(m)) warning_select(hud);
    if (m.r32(world_of(m) + 4) & 0x2C00u) prompt_blink();
    radio_hold_draw(m.r16s(kHold + 2));
    subtitle_panel_draw();
    hud_warn_link();
}

void Game::radar_center(u32 hud) {
    cover(0x8003D08Cu);
    const u32 pos = m.r32(hud + 4), ang = m.r32(hud + 8);
    m.w32(kRadarCenter, static_cast<u32>(m.r32s(pos) >> 12));
    m.w32(kRadarCenter + 4, static_cast<u32>(m.r32s(pos + 8) >> 12));
    const u32 i = m.r16(ang + 2) & 0x3FFFu;
    m.w32(kRadarCenter + 8, static_cast<u32>(m.r16s(kSinTable + 2 * i) >> 2));
    m.w32(kRadarCenter + 12, static_cast<u32>(m.r16s(kSinTable + 0x2000 + 2 * i) >> 2));
    m.w32(0x800BCFB0u, kRadarCenter);
    m.w32(kRadarCenter + 16, static_cast<u32>(m.r16s(ang + 2)));
}

void Game::radar_mark_ranges() {
    cover(0x8003D138u);
    Frame f(*this, 32);
    const u32 c = m.r32(0x800BCFB0u);
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        const u32 mk = kMarks + 12 * k;
        if (!(m.r32(flags_of(k)) & 0x2000u) || (m.r32(e) & 0x40u)) {
            m.w32(mk + 8, 0xFFFFFFFFu);
            continue;
        }
        const s32 dx = (m.r32s(e + 8) >> 12) - m.r32s(c);
        const s32 dz = m.r32s(c + 4) - (m.r32s(e + 16) >> 12);
        const s32 ax = dx < 0 ? m.r32s(c) - (m.r32s(e + 8) >> 12) : dx;
        const s32 az = dz < 0 ? (m.r32s(e + 16) >> 12) - m.r32s(c + 4) : dz;
        if (ax >= 0x4000 || az >= 0x4000)
            m.w32(mk + 8, static_cast<u32>(16 * square_root0((dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4))));
        else
            m.w32(mk + 8, static_cast<u32>(square_root0(dx * dx + dz * dz)));
    }
}

void Game::radar_zoom() {
    cover(0x8003D280u);
    s32 r = 0x40000, want;
    const u32 t = m.r8(g::kCurrentTarget);
    auto nearest = [&](u32 last, bool shown_only) {
        for (u32 k = 2; k < last; ++k) {
            if (shown_only && !(m.r32(flags_of(k)) & 0x100u)) continue;
            const s32 v = m.r32s(kMarks + 12 * k + 8);
            if (v != -1 && v < r) r = v;
        }
    };
    if (m.r32(scene_of(m) + 296) & 0x8000u) {
        if (t != 255 && (m.r32(flags_of(t)) & 0x100u) && m.r32s(kMarks + 12 * t + 8) != -1) r = m.r32s(kMarks + 12 * t + 8);
        else nearest(64, true);
        want = r + (r >> 2);
    } else {
        bool close = false;
        if (t == 255) {
            nearest(32, false);
            close = r < 12288;
        } else if (m.r32s(kMarks + 12 * t + 8) != -1) {
            r = m.r32s(kMarks + 12 * t + 8);
            close = r < 12288;
        }
        want = close ? 0x4000 : r < 24576 ? 0x8000 : 0x10000;
    }
    const s32 cur = m.r32s(kRadarRange);
    s32 v = (cur + want) >> 1;
    if (v & 1) v = cur >= v ? v - 1 : v + 1;
    if (v < 0x2000) v = 0x2000;
    if (v > 0x40000) v = 0x40000;
    m.w32(kRadarRange, static_cast<u32>(v));
}

void Game::radar_marks() {
    cover(0x8003DA08u);
    const s32 range = m.r32s(kRadarRange);
    const bool wide = range > 0x7FFF;
    const u32 p0 = slot(0);
    const u32 c = m.r32(0x800BCFB0u);
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        const u32 mk = kMarks + 12 * k;
        const u32 fl = m.r32(flags_of(k));
        m.w8(mk, m.r8(mk) & 0xF0u);
        if (!(m.r32(e) & 0x4000u) || !(fl & 1u) || !(fl & 0x2000u) || range < m.r32s(mk + 8)) continue;
        const s32 cs = m.r32s(c + 12), sn = m.r32s(c + 8);
        const s32 dz = m.r32s(c + 4) - (m.r32s(e + 16) >> 12);
        const s32 x = 32 * ((((m.r32s(e + 8) >> 12) - m.r32s(c)) * cs + dz * sn) >> 12) / range;
        const s32 y = 32 * (((m.r32s(c) - (m.r32s(e + 8) >> 12)) * sn + dz * cs) >> 12) / range;
        if (static_cast<u32>(x + 31) >= 0x3Fu || static_cast<u32>(y + 31) >= 0x3Fu) continue;
        const u32 b = m.r8(mk);
        m.w8(mk + 6, static_cast<u32>(x + 44));
        m.w8(mk + 7, static_cast<u32>(y + 46));
        m.w8(mk, b | 1u);
        if (wide) m.w8(mk, b | 9u);
        if (m.r8(g::kCurrentTarget) == k) m.w8(mk, m.r8(mk) | 4u);
        const s32 dh = (m.r32s(p0 + 12) - m.r32s(e + 12)) >> 12;
        m.w8(mk + 5, dh < 4097 ? (dh < -4096 ? 0u : 1u) : 2u);
        m.w8(mk + 4, ((m.r16(e + 26) - m.r16(p0 + 26) + 512u) & 0x3FFFu) >> 10);
        if (!(m.r8(mk) & 0x10u)) continue;
        const u32 rel = rel_of(m, k);
        if (!rel) continue;
        u32 col = 0x83;
        if (!(m.r32(rel) & 2u) && !(m.r32(rel + 32) & 0x20000u)) {
            const s32 v = 131 - ((m.r32s(rel + 4) - 0x8000) >> 6);
            if (v >= 132) col = 0x83;
            else col = static_cast<u32>(v < 19 ? 19 : v) & 0xFFu;
        }
        m.w8(mk + 3, col);
    }
}

void Game::radar_missile_marks() {
    cover(0x8003DCF0u);
    const s32 range = m.r32s(kRadarRange);
    const u32 c = m.r32(0x800BCFB0u);
    for (u32 k = 0; k < 32; ++k) {
        const u32 mk = kMissileMarks + 12 * k;
        m.w8(mk, 0);
        if (!(m.r32(m.r32(g::kWeapons) + 5448) & (1u << k))) continue;
        const u32 wp = m.r32(g::kWeapons) + 200 + 164 * k;
        const s32 cs = m.r32s(c + 12), sn = m.r32s(c + 8);
        const s32 dz = m.r32s(c + 4) - (m.r32s(wp + 36) >> 12);
        const s32 x = 32 * ((((m.r32s(wp + 28) >> 12) - m.r32s(c)) * cs + dz * sn) >> 12) / range;
        const s32 y = 32 * (((m.r32s(c) - (m.r32s(wp + 28) >> 12)) * sn + dz * cs) >> 12) / range;
        if (static_cast<u32>(x + 31) >= 0x3Fu || static_cast<u32>(y + 31) >= 0x3Fu) continue;
        m.w8(mk + 7, static_cast<u32>(y + 46));
        m.w8(mk + 6, static_cast<u32>(x + 44));
        m.w8(mk + 1, 2);
        m.w8(mk + 2, 0);
        m.w8(mk, m.r8(mk) | 1u);
    }
}

void Game::radar_update(u32 hud) {
    cover(0x8003C5B0u);
    Frame f(*this, 24);
    radar_begin();
    radar_center(hud);
    radar_mark_ranges();
    radar_zoom();
    radar_frame_draw();
    radar_ground_draw();
    radar_marks_place();
    radar_marks_draw();
    radar_link();
    const u32 sc = scene_of(m);
    if (!m.r8(sc + 2394) && (m.r32(sc + 296) & 0x8000u)) {
        map_draw();
        map_link();
    }
}

void Game::hud_cockpit() {
    cover(0x80033814u);
    Frame f(*this, 0x68);
    const u32 hud = f.at(0x10), spare = f.at(0x50);
    m.w32(spare, 0);
    m.w32(spare + 4, 0);
    const u32 cam = m.r32(kPlayerWeapons);
    const s32 hp_max = m.r32s(cam + 20);
    const s32 hp = hp_max ? (m.r16s(slot(0) + 78) << 12) / hp_max : 0;
    const u32 view = m.r32(world_of(m) + 40);
    m.w32(hud + 24, static_cast<u32>(hp >= 4097 ? 4096 : hp < 0 ? 0 : hp));
    m.w32(hud, m.r32(cam + 36));
    const s32 a = m.r32s(cam + 4), b = m.r32s(cam + 32);
    m.w32(hud + 36, static_cast<u32>(a >= 7 ? 6 : a));
    m.w32(hud + 40, static_cast<u32>(b >= 7 ? 6 : b));
    m.w32(hud + 28, m.r32(cam + 28));
    m.w32(hud + 32, m.r32(cam + 8));
    m.w32(hud + 52, view);
    m.w32(hud + 16, view + 180);
    m.w32(hud + 56, m.r32(kGuidance));
    m.w32(hud + 44, m.r32(cam + 40));
    m.w32(hud + 48, m.r32(cam + 44));
    if (m.r32(view) & 0x100u) {
        const u32 s = m.r32(view) & 0x3Fu;
        const u32 e = slot(s);
        m.w32(hud + 4, e + 8);
        m.w32(hud + 8, e + 24);
        if (s < 2) {
            m.w32(hud + 12, m.r32(g::kPlayer) + 56);
            m.w32(hud + 20, m.r32(m.r32(g::kPlayer) + 72));
        } else {
            m.w32(hud + 12, spare);
            m.w32(hud + 20, m.r32(e + 40));
        }
    } else {
        m.w32(hud + 4, view + 4);
        m.w32(hud + 8, view + 20);
        m.w32(hud + 12, spare);
        m.w32(hud + 20, 0);
    }
    hud_draw(hud, 0);
}

void Game::messages_update() {
    cover(0x8004CA28u);
    u32 active = 0;
    const u32 clut = m.r16(m.r32(kMessages) + 280) + 32600u;
    for (s32 i = 0; i < m.r32s(m.r32(kMessages) + 272); ++i) {
        const u32 ms = m.r32(kMessages);
        const u32 e = ms + 8 + 8 * static_cast<u32>(i);
        const u32 st = m.r8(e);
        if (!st) continue;
        const u32 glyph = 0x800B1914u + 4 * m.r8(e + 1);
        const bool run = !paused(m);
        auto quad = [&](u32 q, u32 x, u32 y, u32 w, u32 h) {
            m.w16(q + 10, y);
            m.w16(q + 18, y);
            m.w16(q + 8, x);
            m.w16(q + 16, x + w);
            m.w16(q + 24, x);
            m.w16(q + 26, y + h);
            m.w16(q + 32, x + w);
            m.w16(q + 34, y + h);
            m.w8(q + 7, m.r8(q + 7) | 2u);
            m.w8(q + 12, m.r8(glyph));
            m.w8(q + 13, m.r8(glyph + 1));
            m.w8(q + 20, m.r8(glyph) + m.r8(glyph + 2));
            m.w8(q + 21, m.r8(glyph + 1));
            m.w8(q + 28, m.r8(glyph));
            m.w8(q + 29, m.r8(glyph + 1) + m.r8(glyph + 3));
            m.w8(q + 36, m.r8(glyph) + m.r8(glyph + 2));
            m.w8(q + 37, m.r8(glyph + 1) + m.r8(glyph + 3));
            m.w16(q + 14, clut);
            m.w16(q + 22, 86);
        };
        if (st == 1) {
            const u32 p = m.r32(kMessagePrims) + 20 * m.r32(ms + 268);
            pkt_rgb(p, 0x80, 0x80, 0x80);
            m.w8(p + 7, m.r8(p + 7) | 2u);
            m.w16(p + 8, m.r16(e + 4));
            m.w16(p + 10, m.r16(e + 6));
            m.w8(p + 12, m.r8(glyph));
            m.w16(p + 14, clut);
            m.w8(p + 13, m.r8(glyph + 1));
            m.w16(p + 16, m.r8(glyph + 2));
            m.w16(p + 18, m.r8(glyph + 3));
            const u32 ms1 = m.r32(kMessages);
            m.w32(ms1 + 268, m.r32(ms1 + 268) + 1);
            if (run) m.w16(e + 2, m.r16(e + 2) + 1u);
            if (!(m.r16s(e + 2) < m.r32s(m.r32(kMessages) + 276))) {
                m.w8(e, 3);
                m.w16(e + 2, 0);
            }
        } else if (st == 2) {
            const u32 q = m.r32(kMessagePrims) + 40 * m.r32(ms + 264) + 640;
            quad(q, m.r16(e + 4), m.r16(e + 6), m.r8(glyph + 2), m.r8(glyph + 3));
            const u32 level = 32u * m.r16(e + 2);
            pkt_rgb(q, level, level, level);
            const u32 ms1 = m.r32(kMessages);
            m.w32(ms1 + 264, m.r32(ms1 + 264) + 1);
            if (run) m.w16(e + 2, m.r16(e + 2) + 1u);
            if (!(m.r16s(e + 2) < 8)) {
                m.w8(e, 1);
                m.w16(e + 2, 0);
            }
        } else if (st == 3) {
            const u32 sh = m.r16(e + 2) & 31u;
            const s32 full_w = m.r8(glyph + 2), full_h = m.r8(glyph + 3);
            const s32 wv = full_w >> sh, hv = full_h >> sh;
            const s32 x = m.r16s(e + 4) + ((full_w - wv) >> 1);
            const s32 y = m.r16s(e + 6) + ((full_h - hv) >> 1);
            const u32 q = m.r32(kMessagePrims) + 40 * m.r32(ms + 264) + 640;
            pkt_rgb(q, 0x80, 0x80, 0x80);
            quad(q, static_cast<u32>(x), static_cast<u32>(y), static_cast<u32>(wv), static_cast<u32>(hv));
            const u32 ms1 = m.r32(kMessages);
            m.w32(ms1 + 264, m.r32(ms1 + 264) + 1);
            if (run) m.w16(e + 2, m.r16(e + 2) + 1u);
            if (!wv && !hv) {
                m.w8(e, 0);
                m.w16(e + 2, 0);
            }
        }
        if (m.r8(e)) ++active;
    }
    if (!active) {
        const u32 ms = m.r32(kMessages);
        m.w32(ms + 272, 0);
        m.w32(ms + 284, 255);
    }
}

void Game::messages_frame() {
    cover(0x8004C998u);
    Frame f(*this, 24);
    if (m.r32(world_of(m) + 4) & 0x2C0Eu) {
        messages_begin();
        messages_update();
        if (!paused(m)) messages_link();
    } else if (m.r32(m.r32(kMessages) + 284) != 255) {
        message_reset();
    }
}

u32 Game::stream_face_record() {
    cover(0x8002B5B0u);
    const u32 i = m.r8(m.r32(0x800BCBACu) + 0x1D);
    const u32 base = m.r32(0x800BCBB0u);
    if (i < 0x20u && (m.r32(base) & (1u << i))) return base + 4 + 16 * i;
    return 0;
}

u32 Game::stream_face_word(u32 out) {
    cover(0x8002C980u);
    Frame f(*this, 24);
    const u32 ring = m.r32(kCdRing);
    if (m.r32s(ring) >= m.r32s(ring + 4)) return 0;
    if (m.r32(ring + 0x148)) return 0;
    const u32 rec = stream_face_record();
    if (!rec || !m.r16(rec)) return 0;
    m.w32(out, m.r32(rec + 4));
    return 1;
}

void Game::face_animate() {
    cover(0x8002CD24u);
    s8 held = 0;
    const u32 p = m.r32(kFace);
    for (u32 i = 0; i < 3; ++i) {
        const u32 cur = m.r8(0x800BCE94u + i);
        if (cur == m.r8(p + i + 7)) {
            const u32 kind = m.r32(p);
            if (m.r16s(p + 2 * i + 10) >= static_cast<s32>(30 * m.r8(0x800B03ACu + 20 * kind + 14))) ++held;
            else if (!paused(m)) m.w16(p + 2 * i + 10, m.r16(p + 2 * i + 10) + 1u);
        } else {
            m.w8(p + i + 7, cur);
            m.w8(p + i + 4, m.r8(0x800BCE94u + i));
            held = static_cast<s8>(i == 0 ? 0x80 : 0);
            m.w16(p + 2 * i + 10, 0);
        }
    }
    if (held == 3) {
        for (u32 i = 0; i < 3; ++i) m.w8(p + i + 4, 0);
    } else if (held < 0) {
        for (u32 i = 1; i < 3; ++i) m.w8(p + i + 4, 0);
    }
}

void Game::face_update() {
    cover(0x8002D3F4u);
    Frame f(*this, 24);
    const u32 word = f.at(0x10);
    if (stream_face_word(word)) {
        const u32 v = m.r32(word);
        if (v != 0xFFFFFFFFu && m.r32(kFaceWord) != v) {
            m.w8(0x800BCE94u, v >> 25);
            m.w8(0x800BCE95u, (v >> 18) & 0x7Fu);
            m.w8(0x800BCE96u, (v >> 11) & 0x7Fu);
            m.w8(0x800BCE8Eu, (v >> 5) & 0x3Fu);
            m.w8(0x800BCE8Fu, (v >> 2) & 7u);
        }
        m.w32(kFaceWord, v);
    }
    face_animate();
}
}
