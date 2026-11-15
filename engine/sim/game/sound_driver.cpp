#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCommonAttr = 0x800C5800u;
constexpr u32 kDelayed = 0x800C6550u;
constexpr u32 kQueue = 0x800C6570u;
constexpr u32 kQueueIndex = 0x800BCE98u;
constexpr u32 kVoiceTable = 0x800C5A10u;
}

void Game::snd_master_volume(u32 v) {
    cover(0x8002D80Cu);
    Frame f(*this, 0x18);
    m.w32(kCommonAttr, 3);
    m.w16(kCommonAttr + 4, v);
    m.w16(kCommonAttr + 6, v);
    hw->spu_set_common_attr(kCommonAttr);
}

void Game::snd_cd_volume(u32 v) {
    cover(0x8002D844u);
    Frame f(*this, 0x18);
    m.w32(kCommonAttr, 0xC0);
    m.w16(kCommonAttr + 18, v << 6);
    m.w16(kCommonAttr + 16, v << 6);
    hw->spu_set_common_attr(kCommonAttr);
}

void Game::snd_voice_volumes(u32 v, u32 attr, bool faded) {
    const u32 sc = m.r32(g::kScene);
    const s32 vol = static_cast<s32>(m.r8(sc + 920));
    const s32 l = m.r16s(v + 4), r = m.r16s(v + 6);
    const s32 level = static_cast<s32>(m.r8(v + 17));
    if (m.r8(sc + 922) & 1u) {
        const s32 a = l < 0 ? -l : l, b = r < 0 ? -r : r;
        s32 x = ((a + b) * vol) >> 9;
        if (faded) x = (x * level) >> 7;
        m.w16(attr + 8, static_cast<u32>(x));
        m.w16(attr + 10, static_cast<u32>(x));
        return;
    }
    s32 xl = (l * vol) >> 8, xr = (r * vol) >> 8;
    if (faded && level != 128) {
        xl = (xl * level) >> 7;
        xr = (xr * level) >> 7;
    }
    m.w16(attr + 8, static_cast<u32>(xl));
    m.w16(attr + 10, static_cast<u32>(xr));
}

void Game::snd_voice_update(u32 i) {
    cover(0x8002DF98u);
    Frame f(*this, 0x58);
    const u32 attr = f.at(0x10);
    const u32 v = g::kVoices + 20 * i;
    m.w32(attr, 1u << i);
    m.w32(attr + 4, 0x23);
    snd_voice_volumes(v, attr, true);
    m.w16(attr + 22, m.r16(v + 12));
    hw->spu_set_voice_attr(attr);
}

void Game::snd_voice_start(u32 i) {
    cover(0x8002E0D0u);
    Frame f(*this, 0x58);
    const u32 attr = f.at(0x10);
    const u32 v = g::kVoices + 20 * i;
    const u32 s = kVoiceTable + 20u * m.r8(v);
    m.w32(attr, 1u << i);
    m.w32(attr + 4, 0x600E3);
    snd_voice_volumes(v, attr, false);
    m.w16(attr + 22, m.r16(v + 12));
    m.w16(attr + 24, m.r16(s + 10));
    m.w32(attr + 28, m.r32(s + 16));
    m.w16(attr + 58, m.r16(s + 12));
    m.w16(attr + 60, m.r16(s + 14));
    hw->spu_set_voice_attr(attr);
}

void Game::snd_delayed_clear(u32 k) {
    cover(0x8002F178u);
    const u32 p = kDelayed + 4 * k;
    m.w8(p, 255);
    m.w16(p + 2, 0xFFFF);
}

void Game::snd_delayed_update() {
    cover(0x8002F1E0u);
    Frame f(*this, 0x20);
    for (u32 k = 0; k < 8; ++k) {
        const u32 p = kDelayed + 4 * k;
        if (m.r16s(p + 2) < 0) continue;
        const u32 n = (m.r16(p + 2) - 1u) & 0xFFFFu;
        m.w16(p + 2, n);
        if (n) continue;
        sound_play(m.r8(p));
        snd_delayed_clear(k);
    }
}

void Game::snd_queue_update() {
    cover(0x8002F2CCu);
    Frame f(*this, 0x18);
    const u32 idx = m.r8(kQueueIndex);
    const u32 p = kQueue + 4 * idx;
    const u32 voice = m.r8(p + 1);
    if (voice != 255) {
        if (m.r8(g::kVoices + 20 * voice + 3)) return;
        m.w8(p + 1, 255);
        m.w8(p, 255);
        m.w16(p + 2, 0xFFFF);
        m.w8(kQueueIndex, (m.r8(kQueueIndex) + 1u) & 0x1Fu);
        return;
    }
    const s32 wait = m.r16s(p + 2);
    if (wait == -1) {
        m.w8(kQueueIndex + 1, idx);
        return;
    }
    if (m.r32(m.r32(g::kScene)) & 0x20000000u) return;
    if (!wait) m.w8(p + 1, sound_play(m.r8(p)) & 0xFFu);
    m.w16(p + 2, m.r16(p + 2) - 1u);
}

void Game::sound_driver() {
    cover(0x8002E1F4u);
    Frame f(*this, 0x38);
    const u32 cd = cd_current() & 0x7FFu;
    const u32 sc = m.r32(g::kScene);
    snd_cd_volume(cd == 0x1E6 || cd - 0x23u < 0x24u ? m.r8(sc + 921) : m.r8(sc + 920));
    snd_master_volume((m.r32(m.r32(g::kScene)) & 0x20000000u) ? 0 : 0x3FFF);
    if (!(m.r32(m.r32(g::kScene)) & 0x20000000u)) snd_delayed_update();
    snd_queue_update();
    u32 on = 0;
    for (u32 i = 0; i < 24; ++i) {
        const u32 v = g::kVoices + 20 * i;
        if (m.r8(v + 1) & 1u) {
            on |= 1u << i;
            m.w8(v + 1, m.r8(v + 1) & 0xFEu);
            snd_voice_start(i);
            if (m.r8(v + 1) & 0x10u) m.w8(v + 1, m.r8(v + 1) & 0xEFu);
        }
        const s32 frames = m.r16s(v + 18);
        if (frames <= 0) {
            m.w8(v + 17, m.r8(v + 16));
        } else {
            const s32 d = (static_cast<s32>(m.r8(v + 16)) - static_cast<s32>(m.r8(v + 17))) / frames;
            m.w8(v + 17, (m.r8(v + 17) + static_cast<u32>(d)) & 0xFFu);
            if (m.r16s(v + 18) > 0) m.w16(v + 18, m.r16(v + 18) - 1u);
        }
        if (m.r8(v + 17) != 128) m.w8(v + 1, m.r8(v + 1) | 0x10u);
        if (m.r8(v + 1) & 0x10u) {
            snd_voice_update(i);
            m.w8(v + 1, m.r8(v + 1) & 0xEFu);
        }
    }
    hw->spu_set_key(1, on);
    const u32 status = f.at(0x10);
    hw->spu_get_all_keys_status(status);
    for (u32 i = 0; i < 24; ++i) {
        const u32 v = g::kVoices + 20 * i;
        const u32 st = m.r8(status + i);
        if (st == 1) {
            m.w8(v + 2, 8);
        } else if (st == 2) {
        } else if (st == 0) {
            if (m.r8(v + 3)) m.w8(v + 1, 4);
        } else {
            const u32 n = m.r8(v + 2);
            if (!n) {
                voice_release(i);
            } else {
                m.w8(v + 2, n - 1);
                if (!((n - 1) & 0xFFu)) voice_release(i);
            }
        }
    }
    for (u32 i = 0; i < 24; ++i)
        if (m.r8(g::kVoices + 20 * i + 1) & 4u) voice_clear(i);
    u32 off = 0;
    for (u32 i = 0; i < 24; ++i)
        if (m.r8(g::kVoices + 20 * i + 1) & 2u) off |= 1u << i;
    hw->spu_set_key(0, off);
}
}
