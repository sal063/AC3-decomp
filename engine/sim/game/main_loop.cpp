#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>
#include <cmath>
#include "sim/hw.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDispGlobals = 0x800BCE48u;
constexpr u32 kDispBuffers = 0x800BCE58u;
constexpr u32 kPadBase = 0x800BCB84u;
constexpr u32 kRumble = 0x800BCBA4u;
constexpr u32 kOverlayList = 0x800BCB98u;
constexpr u32 kGroupTable = 0x800B0134u;
constexpr u32 kPadIdTables = 0x800B01C4u;
constexpr u32 kActTables = 0x800B01D4u;
constexpr u32 kRumblePattern = 0x800B0340u;
constexpr u32 kPadAct = 0x800C04DCu;

s32 s16v(u32 v) { return static_cast<s32>(static_cast<s16>(v)); }
}

s32 Game::pad_id_cmp(u32 a, u32 b) {
    cover(0x800265F0u);
    for (;;) {
        const u32 x = m.r16(a);
        const s32 d = s16v(x) - s16v(m.r16(b));
        if (d) return d;
        a += 2;
        if (!x) return 0;
        b += 2;
    }
}

u32 Game::pad_id_lookup(u32 table) {
    cover(0x80026628u);
    u32 e = kPadIdTables;
    for (u32 k = 0; k < 2; ++k, e += 8)
        if (!pad_id_cmp(table, m.r32(e + 4))) return m.r32(e);
    return 0xFFFFFFFEu;
}

u32 Game::pad_mode_code(u32 id) {
    cover(0x80026694u);
    switch (id) {
    case 1: return 3;
    case 2: return 2;
    case 3: return 5;
    case 4: return 1;
    case 5: return 4;
    case 6: return 7;
    case 7: return 6;
    default: return 0xFFFFFFFFu;
    }
}

u32 Game::pad_act_byte(u32 id, u32 which) {
    cover(which == 1 ? 0x80026700u : 0x80026754u);
    const u32 base = m.r32(kPadBase);
    const u32 n = m.r32(base + (kPadAct - 0x800C0418u));
    if (static_cast<s32>(n) <= 0) return 0;
    u32 e = m.r32(base + (kPadAct - 0x800C0418u) + 4);
    for (u32 k = 0;; e += 3) {
        ++k;
        if (m.r8(e) == id) return m.r8(e + which);
        if (static_cast<s32>(k) >= static_cast<s32>(n)) return 0;
    }
}

u32 Game::pad_act_bit(u32 info) {
    cover(0x800267F0u);
    Frame f(*this, 32);
    u32 e = kActTables;
    for (u32 k = 0; k < 3; ++k, e += 4)
        if (!bytes3_differ(info, e + 1)) return m.r8(e);
    return 0;
}

u32 Game::pad_setup(u32 rec) {
    cover(0x8002685Cu);
    Frame f(*this, 0x48);
    const u32 port = m.r8(rec);
    u32 result = 0;
    const u32 state = hw->pad_get_state(port);
    const u32 cur_id = hw->pad_info_mode(port, 1, 0);
    const u32 ex_id = hw->pad_info_mode(port, 2, 0);
    m.w8(rec + 2, state);
    const u32 pad = m.r32(rec + 4);
    if (!state) {
        m.w8(pad, 0);
        m.w8(pad + 1, 0);
        m.w16(pad + 10, 0);
        m.w8(rec + 1, 0);
        return 0;
    }
    const u32 phase = m.r8(rec + 1);
    if (phase == 1) {
        if (state != 6) return 0;
        for (u32 k = 8; k <= 13; ++k) m.w8(rec + k, 0xFF);
        const u32 acts = hw->pad_info_act(port, 0xFFFFFFFFu, 0) & 0xFFu;
        u32 found = 0;
        const u32 info = f.at(0x20);
        for (u32 k = 0; k < acts && k < 6; ++k) {
            m.w8(info, hw->pad_info_act(port, k, 1));
            m.w8(info + 1, hw->pad_info_act(port, k, 2));
            m.w8(info + 2, hw->pad_info_act(port, k, 3));
            const u32 bit = pad_act_bit(info);
            if (bit) m.w8(rec + 8 + (found++ & 0xFFu), k);
            m.w8(pad + 1, m.r8(pad + 1) | (bit & 0xFFu));
        }
        libpad_set_act_align(port, rec + 8);
        m.w8(rec + 1, 2);
        return 0;
    }
    if (phase >= 2) {
        if (phase != 2) {
            m.w8(rec + 1, 0);
            return 0;
        }
        if (state != 6) return 0;
        u32 n = 0;
        for (u32 k = 0; k < 6; ++k)
            if ((m.r8(pad + 1) >> k) & 1u) ++n;
        libpad_set_act(port, pad + 2, n);
        m.w16(pad + 10, ex_id);
        return cur_id;
    }
    if (state != 2) {
        if (state == 6) {
            const u32 ids = f.at(0x10);
            const u32 count = hw->pad_info_mode(port, 4, 0xFFFFFFFFu);
            for (u32 k = 0; k < 8; ++k) m.w16(ids + 2 * k, 0);
            for (u32 k = 0; static_cast<s32>(k) < static_cast<s32>(count) && k < 7; ++k)
                m.w16(ids + 2 * k, hw->pad_info_mode(port, 4, k));
            const u32 id = pad_id_lookup(ids);
            const u32 mode = pad_act_byte(id, 1);
            const u32 lock = pad_act_byte(id, 2);
            m.w8(pad, id & 0xFFu);
            libpad_set_main_mode(port, mode, lock);
            m.w8(pad + 1, 0);
            m.w8(rec + 1, 1);
        }
        return result;
    }
    m.w8(pad, pad_mode_code(cur_id) & 0xFFu);
    m.w8(pad + 1, m.r8(m.r32(kPadBase) + (0x800C04E4u - 0x800C0418u)) & 1u);
    libpad_set_act(port, pad + 2, 2);
    m.w16(pad + 10, ex_id);
    return cur_id;
}

void Game::pad_parse(u32 buf, u32 rec) {
    cover(0x80026BA4u);
    Frame f(*this, 0x28);
    const u32 pad = m.r32(rec + 4);
    u32 head = m.r16(buf);
    const u32 had = m.r8(pad + 12);
    m.w8(pad + 13, 0);
    bool setup_status = false;
    if (had || (head & 0xFFu) != 0) {
        if (m.r8(rec + 14)) {
            if (pad_setup(rec)) m.w8(rec + 14, 0);
            else setup_status = true;
        }
    } else {
        if (!pad_setup(rec)) {
            head = 0;
            setup_status = true;
        }
    }
    if (setup_status) m.w8(pad + 13, 2);
    auto clear_axes = [&](bool with24) {
        if (with24) m.w16(pad + 24, 0);
        m.w16(pad + 22, 0);
        m.w16(pad + 20, 0);
        m.w16(pad + 18, 0);
    };
    if ((head & 0xFFu) != 0) {
        m.w8(pad + 13, m.r8(pad + 12) ? 8 : 1);
        m.w8(rec + 1, 0);
        for (u32 k = 0; k < 8; ++k) m.w8(pad + k, 0);
        m.w16(pad + 8, 0);
        m.w16(pad + 10, 0);
        m.w8(pad + 12, 0);
        m.w16(pad + 16, 0);
        m.w16(pad + 14, 0);
        clear_axes(true);
        return;
    }
    u32 type = head >> 8;
    if (m.r8(pad + 12) && m.r16(pad + 10)) {
        hw->pad_get_state(m.r8(rec));
        if (hw->pad_info_mode(m.r8(rec), 2, 0) != m.r16(pad + 10)) {
            type = 0;
            m.w8(pad + 13, 4);
            m.w8(rec + 1, 1);
            m.w8(rec + 14, 1);
        }
    }
    m.w8(pad + 12, type & 0xFFu);
    const u32 held = ~m.r16(buf + 2) & 0xFFFFu;
    const u32 before = ~m.r16(pad + 14) & 0xFFFFu;
    m.w16(pad + 14, held);
    m.w16(pad + 16, held & before);
    const u32 w0 = m.r16(buf + 4), w1 = m.r16(buf + 6);
    switch (type & 0xFFFFu) {
    case 0x23: case 0x53: case 0x73:
        m.w16(pad + 18, w0 >> 8);
        m.w16(pad + 20, w0 & 0xFFu);
        m.w16(pad + 22, w1 >> 8);
        m.w16(pad + 24, w1 & 0xFFu);
        return;
    case 0x12: {
        const s32 dx = static_cast<s8>(w0 >> 8), dy = static_cast<s8>(w0 & 0xFFu);
        m.w16(pad + 18, static_cast<u32>(dx));
        m.w16(pad + 20, static_cast<u32>(dy));
        m.w16(pad + 22, m.r16(pad + 22) + static_cast<u32>(dx));
        m.w16(pad + 24, m.r16(pad + 24) + static_cast<u32>(dy));
        return;
    }
    case 0x63:
        m.w16(pad + 24, 0);
        m.w16(pad + 22, 0);
        m.w16(pad + 18, w0);
        m.w16(pad + 20, w1);
        return;
    case 0xE3:
        m.w16(pad + 24, 0);
        m.w16(pad + 22, w0 - m.r16(pad + 18));
        m.w16(pad + 18, w0);
        m.w16(pad + 20, w1);
        return;
    default:
        clear_axes(true);
        return;
    }
}

void Game::pad_poll() {
    cover(0x80026E84u);
    Frame f(*this, 0x28);
    if (!hw->pad_chk_vsync()) return;
    const u32 base = m.r32(kPadBase);
    auto rec = [&](u32 k) { return m.r32(kPadBase) + 0x44u + 16u * k; };
    u32 k = 0;
    const u32 h0 = m.r16(base);
    if ((h0 & 0xFFu) == 0 && (h0 >> 8) == 0x80u) {
        for (u32 i = 0; i < 4; ++i) pad_parse(base + 2 + 8 * i, rec(i));
        k = 4;
    } else {
        pad_parse(base, rec(0));
        for (u32 i = 1; i < 4; ++i) {
            m.w16(base + 8 * i, 0xFFFF);
            pad_parse(base + 8 * i, rec(i));
        }
        k = 4;
    }
    const u32 b1 = m.r32(kPadBase) + 0x22u;
    const u32 h1 = m.r16(b1);
    if ((h1 & 0xFFu) == 0 && (h1 >> 8) == 0x80u) {
        for (u32 i = k, p = m.r32(kPadBase) + 0x24u; i < 8; ++i, p += 8) pad_parse(p, rec(i));
        return;
    }
    pad_parse(b1, rec(k));
    for (u32 i = k + 1, p = b1 + 8; i < 8; ++i, p += 8) {
        m.w16(p, 0xFFFF);
        pad_parse(p, rec(i));
    }
}

void Game::axes_calibrate(u32 axes, u32 cal) {
    cover(0x8002895Cu);
    auto div = [](s32 a, s32 b) { return b ? a / b : (a >= 0 ? -1 : 1); };
    for (u32 k = 0; k < 4; ++k, axes += 2, cal += 4) {
        const s32 lo = m.r8(cal), mid = m.r8(cal + 1), hi = m.r8(cal + 2), dead = m.r8(cal + 3);
        if (s16v(m.r16(axes)) < lo) m.w16(axes, static_cast<u32>(lo));
        if (hi < s16v(m.r16(axes))) m.w16(axes, static_cast<u32>(hi));
        const s32 d = s16v(m.r16(axes) - static_cast<u32>(mid));
        if (dead < d) {
            m.w16(axes, static_cast<u32>(div((d - dead) << 13, 2 * (hi - mid - dead)) - 1));
        } else if (d >= -dead) {
            m.w16(axes, 0);
        } else {
            m.w16(axes, static_cast<u32>(div((d + dead) << 13, -2 * (lo - mid + dead))));
        }
    }
}

void Game::stick_modern(s32 raw_a, s32 raw_b, s32& a, s32& b) {
    constexpr float kDead = 0.10f, kFull = 0.95f;
    const float x = std::clamp((static_cast<float>(raw_a) - 128.0f) / 127.0f, -1.0f, 1.0f);
    const float y = std::clamp((static_cast<float>(raw_b) - 128.0f) / 127.0f, -1.0f, 1.0f);
    const float r = std::sqrt(x * x + y * y);
    a = b = 0;
    if (r <= kDead) return;
    const float len = std::min(1.0f, (r - kDead) / (kFull - kDead));
    const float dx = x / r, dy = y / r, edge = std::max(std::fabs(dx), std::fabs(dy));
    auto out = [](float v) { return std::clamp(static_cast<s32>(std::lround(v * 4096.0f)), -4096, 4095); };
    a = out(dx / edge * len);
    b = out(dy / edge * len);
}

s32 Game::axis_curve(s32 a, bool second) {
    cover(second ? 0x80028B1Cu : 0x80028A7Cu);
    const s32 half = s16v(static_cast<u32>(a)) / 2;
    const s32 s = rsin((second ? -half : half) & 0xFFF);
    s32 v = s16v(static_cast<u32>(a + 7 * s / 24));
    if (v < -4096) v = -4096;
    if (v >= 4096) v = 4095;
    return v;
}

void Game::rumble_update(u32 out) {
    cover(0x80029734u);
    u32 big = 0, small = 0;
    u32 e = m.r32(kRumble);
    for (u32 k = 0; k < 6; ++k, e += 4) {
        u32 n = m.r8(e);
        if (!n) continue;
        const u32 type = m.r8(e + 1), v = m.r8(e + 2);
        s32 decay = -1;
        bool dec_only = false;
        switch (type) {
        case 0x14: {
            if (big < v) big = v;
            if (small < (v >> 1)) small = v >> 1;
            decay = 4;
            break;
        }
        case 5:
            if (small < v) small = v;
            break;
        case 1: case 2: case 3: case 9:
            if ((type & 1) && big < v) big = v;
            if (type & 2) {
                if (small < v) small = v;
                decay = 2;
            }
            break;
        case 0x10:
            if (small < v) small = v;
            decay = 3;
            break;
        case 0xA1:
            if (big < v) big = v;
            if (small < v) small = v;
            break;
        case 0x1E:
            if (big < (v >> 1)) big = v >> 1;
            if (small < v) small = v;
            decay = 5;
            break;
        case 0xA0:
            if (small < v) small = v;
            break;
        case 0xB4:
            if (big < v) big = v;
            if (small < v) small = v;
            if (n < 6) decay = 4;
            break;
        case 0xFA:
            if (big < v) big = v;
            if (small < v) small = v;
            if (n < 0x39) decay = 5;
            break;
        case 0: case 4:
            break;
        default:
            dec_only = true;
            break;
        }
        (void)dec_only;
        if (decay >= 0) m.w8(e + 2, (v - (v >> decay)) & 0xFFu);
        --n;
        m.w8(e, n & 0xFFu);
        if (!n) m.w8(e + 1, 0);
    }
    if (big >= 0x100) big = 0xFF;
    if (small >= 0x100) small = 0xFF;
    m.w8(out, big);
    m.w8(out + 1, small);
}

void Game::input_map(u32 pad, u32 in, u32 cfg) {
    cover(0x80028C24u);
    Frame f(*this, 0x40);
    u32 table = cfg;
    u32 buttons = m.r16(pad + 14);
    m.w32(in + 24, 0);
    m.w16(in + 54, 0);
    const u32 type = m.r8(pad + 12);
    u32 bits = 0;
    const u32 axes = f.at(0x10);
    auto map_axes = [&](u32 tbl) {
        for (u32 k = 0; k < 6; ++k) {
            const u32 idx = m.r8(tbl + 82 + k);
            const u32 slot = in + 28 + 4 * k;
            if (idx == 255) {
                m.w32(slot, 0);
                continue;
            }
            s32 v = s16v(m.r16(axes + 2 * idx));
            m.w32(in + 24, m.r32(in + 24) | (1u << k));
            const u32 fl = m.r16(tbl + 80);
            if (fl & 1u) v = axis_curve(v, false);
            else if (fl & 2u) v = axis_curve(v, true);
            m.w32(slot, static_cast<u32>(v));
        }
    };
    auto load_axes = [&]() {
        for (u32 k = 0; k < 4; ++k) m.w16(axes + 2 * k, m.r16(pad + 18 + 2 * k));
    };
    auto clear_all = [&](u32 kind) {
        m.w32(in + 4, kind);
        for (u32 o : {16u, 12u, 8u, 24u, 36u, 32u, 28u, 48u, 44u, 40u}) m.w32(in + o, 0);
        m.w8(in + 56, 0);
        m.w8(in + 59, 0);
        m.w8(in + 58, 0);
        m.w8(in + 57, 0);
    };
    if (type == 0x41 || type == 0x43) {
        m.w32(in + 4, 1);
    } else if (type == 0x23) {
        m.w32(in + 4, 16);
        table = cfg + 368;
        load_axes();
        axes_calibrate(axes, cfg + 432);
        map_axes(table);
        const s32 a0 = s16v(m.r16(axes)), a2 = s16v(m.r16(axes + 4)), a3 = s16v(m.r16(axes + 6));
        if (a0 >= 1537) buttons |= 0x4000u;
        if (a3 >= 1537) buttons |= 0x8000u;
        if (a2 >= 1537) buttons |= 0x400u;
        if (buttons & 0x400u) buttons |= 0x100u;
        if (buttons & 0x800u) buttons |= 0x200u;
    } else if (type == 0x53 || type == 0x73) {
        u32 kind = 8;
        if (type == 0x73) {
            kind = 4;
            if (m.r16(pad + 10) == 7) table = cfg + 184;
            else {
                table = cfg + 92;
                kind = 2;
            }
        } else {
            table = cfg + 276;
        }
        m.w32(in + 4, kind);
        load_axes();
        if (type == 0x73 && option(&Settings::analog_sticks)) {
            for (u32 k = 0; k < 4; k += 2) {
                s32 a = 0, b = 0;
                stick_modern(s16v(m.r16(axes + 2 * k)), s16v(m.r16(axes + 2 * k + 2)), a, b);
                m.w16(axes + 2 * k, static_cast<u32>(a));
                m.w16(axes + 2 * k + 2, static_cast<u32>(b));
            }
        } else {
            axes_calibrate(axes, table + 64);
        }
        map_axes(table);
        const s32 x = s16v(m.r16(axes + (type == 0x53 ? 0 : 4)));
        const s32 y = s16v(m.r16(axes + (type == 0x53 ? 2 : 6)));
        const s32 t = type == 0x53 ? 1536 : 3754;
        if (x < -t) bits = 1;
        if (x > t) bits |= 2;
        if (y < -t) bits |= 4;
        if (t < y) bits |= 8;
    } else if (type == 0) {
        buttons = 0;
        clear_all(0);
    } else {
        buttons = 0;
        clear_all(32);
    }
    const u32 status = m.r8(pad + 13);
    if (status & 1u) m.w16(in + 54, m.r16(in + 54) | 2u);
    else if (status & 2u) m.w16(in + 54, m.r16(in + 54) | 8u);
    else if (status & 8u) m.w16(in + 54, m.r16(in + 54) | 1u);
    for (u32 i = 0; i < 32; ++i) {
        const s32 mask = s16v(m.r16(table + 2 * i));
        if (mask && (static_cast<s32>(buttons) & mask)) bits |= 1u << i;
    }
    const u32 opts = m.r32(table + 88);
    if (opts & 0x10u) {
        if (bits & 0x200000u) {
            const u32 c = m.r8(in + 57);
            bits &= ~0x200000u;
            if (c >= 7) bits |= 0x2000u;
            else {
                m.w8(in + 57, c + 1);
                if (((c + 1) & 0xFFu) >= 7) bits |= 0x2000u;
            }
        } else {
            if (m.r8(in + 57) - 1u < 6u) bits |= 0x200000u;
            m.w8(in + 57, 0);
        }
    }
    if (opts & 8u) {
        if (bits & 0x4000u) {
            const u32 c = m.r8(in + 58);
            bits &= ~0x4000u;
            if (c >= 10) bits |= 0x80000000u;
            else {
                m.w8(in + 58, c + 1);
                if (((c + 1) & 0xFFu) >= 10) bits |= 0x80000000u;
            }
        } else {
            if (m.r8(in + 58) - 1u < 9u) bits |= 0x4000u;
            m.w8(in + 58, 0);
        }
    }
    const u32 prev = m.r32(in + 8);
    const u32 dir = bits & 0xFu;
    m.w32(in + 8, bits);
    const u32 pressed = ~prev & bits;
    m.w32(in + 12, pressed);
    m.w32(in + 16, pressed);
    m.w32(in + 20, prev & ~bits);
    if (dir && dir == (prev & 0xFu)) m.w8(in + 56, m.r8(in + 56) + 1);
    else m.w8(in + 56, 0);
    const u32 rep = m.r8(in + 56);
    if (rep >= 0x17 && !(rep & 3u)) m.w32(in + 16, m.r32(in + 16) | dir);
    const u32 fl = m.r16(table + 80);
    if (fl & 4u) m.w32(in + 28, 0u - m.r32(in + 28));
    if (fl & 8u) m.w32(in + 40, 0u - m.r32(in + 40));
    if (fl & 0x10u) m.w32(in + 44, 0u - m.r32(in + 44));
    rumble_update(in + 52);
    if ((m.r32(in) & 1u) && !(m.r32(m.r32(g::kScene)) & 0x20000000u)) {
        u32 big = m.r8(in + 52), small = m.r8(in + 53);
        const u32 caps = m.r8(pad + 1);
        if (!(caps & 1u)) small = (small + big) >> 1;
        if (!(caps & 2u)) big = (big + small) >> 1;
        u32 out = pad + 2;
        const u32 pulse =
            (m.r32s(kRumblePattern + ((big >> 2) & 0x3Cu)) >> (m.r32(m.r32(g::kScene) + 36) & 31u)) & 1u;
        if (!m.r16(pad + 10) && m.r8(pad + 12)) {
            m.w8(pad + 2, 64);
            out = pad + 3;
        }
        if (caps & 1u) m.w8(out++, pulse);
        if (caps & 2u) m.w8(out, small);
    } else {
        for (u32 k = 2; k <= 7; ++k) m.w8(pad + k, 0);
    }
}

void Game::display_env_update() {
    cover(0x80025BC8u);
    const u32 d = m.r32(kDispGlobals);
    u32 fl = m.r32(d);
    if (!fl) return;
    const u32 bufs = m.r32(kDispBuffers);
    if (fl & 0x40u) {
        const u32 mode = m.r32(d + 4);
        m.w32(d, fl & ~0x40u);
        if (mode == 0) {
            m.w32(bufs + 4, 0x800BD1E4u);
            m.w32(bufs + 4120, 0x800BD1F8u);
        } else if (mode == 1) {
            m.w32(bufs + 4, 0x800BD20Cu);
            m.w32(bufs + 4120, 0x800BD20Cu);
        }
        fl = m.r32(d);
    }
    if (fl & 1u) {
        const u32 mode = m.r32(d + 4);
        m.w32(d, (fl & ~1u) | 0x40u);
        if (mode == 0) {
            m.w32(bufs, 0x800BD0D0u);
            m.w32(bufs + 4116, 0x800BD12Cu);
        } else if (mode == 1) {
            m.w32(bufs, 0x800BD188u);
            m.w32(bufs + 4116, 0x800BD188u);
        }
    }
    fl = m.r32(d);
    if (fl & 0x10u) {
        m.w32(d, fl & ~0x10u);
        m.w32(bufs + 4, d + 24);
        m.w32(bufs + 4120, d + 24);
        m.w32(d, m.r32(d) | 1u);
        fl = m.r32(d);
    }
    if (fl & 0x20u) {
        m.w32(d, fl & ~0x20u);
        m.w32(bufs, d + 44);
        m.w32(bufs + 4116, d + 44);
        m.w32(d, m.r32(d) | 1u);
    }
    if (m.r32(d) & 2u) {
        m.w32(d, m.r32(d) & ~2u);
        for (u32 a : {0x800BD1ECu, 0x800BD200u, 0x800BD214u}) {
            m.w16(a, m.r16(d + 8));
            m.w16(a + 2, m.r16(d + 12));
        }
    }
    fl = m.r32(d);
    if (fl & 4u) {
        m.w32(d, fl & ~4u);
        m.w8(0x800BD1A0u, 0);
        m.w8(0x800BD144u, 0);
        m.w8(0x800BD0E8u, 0);
        fl = m.r32(d);
    }
    if (fl & 8u) {
        m.w32(d, fl & ~8u);
        const u32 r = m.r8(d + 16), gg = m.r8(d + 17), b = m.r8(d + 18);
        for (u32 a : {0x800BD0E8u, 0x800BD144u, 0x800BD1A0u}) {
            m.w8(a, 1);
            m.w8(a + 1, r);
            m.w8(a + 2, gg);
            m.w8(a + 3, b);
        }
    }
}

void Game::buffers_flip() {
    cover(0x80026050u);
    Frame f(*this, 0x18);
    const u32 sc = m.r32(g::kScene);
    const u32 idx = m.r32(sc + 4) ^ 1u;
    m.w32(sc + 4, idx);
    const u32 b = m.r32(kDispBuffers) + 4116u * idx;
    m.w32(sc + 8, b + 12);
    m.w32(sc + 12, b + 8);
    m.w32(sc + 16, b + 16);
    m.w32(sc + 20, b + 20);
    clear_otag_r(b + 8, 3);
}

u32 Game::overlay_packets(u32 ot, u32 p) {
    cover(0x800278A8u);
    const u32 list = m.r32(kOverlayList);
    const s32 n = m.r32s(list);
    u32 src = list + 4;
    for (s32 k = 0; k < n; ++k, src += 8, p += 20) {
        m.w32(p + 4, 0x64808080u);
        m.w32(p + 8, m.r32(src));
        m.w32(p + 16, 0x00060006u);
        m.w32(p + 12, m.r32(src + 4));
        m.w32(p, m.r32(ot) | 0x04000000u);
        m.w32(ot, p & 0x00FFFFFFu);
    }
    m.w8(p + 3, 1);
    m.w32(p + 4, 0xE1000015u);
    m.w32(0x800BCE68u, 0x00100010u);
    m.w32(p, (m.r32(p) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (p & 0x00FFFFFFu));
    m.w32(list, 0);
    return p + 8;
}

void Game::build_dr_env(u32 env) {
    auto clampx = [](s32 v) { return v < 0 ? 0 : (v > 1023 ? 1023 : v); };
    auto clampy = [](s32 v) { return v < 0 ? 0 : (v > 511 ? 511 : v); };
    auto area = [&](u32 cmd, s32 x, s32 y) {
        return (cmd << 24) | ((static_cast<u32>(clampy(y)) & 0x3FFu) << 10) | (static_cast<u32>(clampx(x)) & 0x3FFu);
    };
    const u32 p = env + 28;
    const s32 cx = s16v(m.r16(env)), cy = s16v(m.r16(env + 2)), cw = s16v(m.r16(env + 4)), ch = s16v(m.r16(env + 6));
    const s32 ox = s16v(m.r16(env + 8)), oy = s16v(m.r16(env + 10));
    m.w32(p + 4, area(0xE3u, cx, cy));
    m.w32(p + 8, area(0xE4u, s16v(static_cast<u32>(cx + cw - 1)), s16v(static_cast<u32>(cy + ch - 1))));
    m.w32(p + 12, (0xE5u << 24) | ((static_cast<u32>(oy) & 0x7FFu) << 11) | (static_cast<u32>(ox) & 0x7FFu));
    const u32 tpage = m.r16(env + 20), dtd = m.r8(env + 22), dfe = m.r8(env + 23);
    m.w32(p + 16, (0xE1u << 24) | (dtd ? 0x200u : 0u) | (dfe ? 0x400u : 0u) | (tpage & 0x9FFu));
    u32 tw = 0;
    {
        const u32 t = env + 12;
        const u32 xlo = m.r8(t), ylo = m.r8(t + 2);
        const s32 w = s16v(m.r16(t + 4)), h = s16v(m.r16(t + 6));
        tw = (0xE2u << 24) | ((ylo >> 3) << 15) | ((xlo >> 3) << 10) | (static_cast<u32>((-h) << 2) & 0x3E0u) |
             ((static_cast<u32>(-w) & 0xFFu) >> 3);
    }
    m.w32(p + 20, tw);
    m.w32(p + 24, 0xE6000000u);
    u32 len = 6;
    if (m.r8(env + 24)) {
        const s32 w = cw < 0 ? 0 : (cw > 1023 ? 1023 : cw);
        const s32 h = ch < 0 ? 0 : (ch > 511 ? 511 : ch);
        const u32 rgb = m.r8(env + 25) | (m.r8(env + 26) << 8) | (m.r8(env + 27) << 16);
        const u32 wh = (static_cast<u32>(static_cast<u16>(h)) << 16) | static_cast<u16>(w);
        if ((cx & 0x3F) || (w & 0x3F)) {
            const u16 tx = static_cast<u16>(cx - ox), ty = static_cast<u16>(cy - oy);
            m.w32(p + 28, 0x60000000u | rgb);
            m.w32(p + 32, (static_cast<u32>(ty) << 16) | tx);
        } else {
            m.w32(p + 28, 0x02000000u | rgb);
            m.w32(p + 32, (static_cast<u32>(static_cast<u16>(cy)) << 16) | static_cast<u16>(cx));
        }
        m.w32(p + 36, wh);
        len = 9;
    }
    m.w8(p + 3, len);
}

void Game::frame_present() {
    cover(0x800260C0u);
    Frame f(*this, 0x18);
    const u32 sc = m.r32(g::kScene);
    const u32 b = m.r32(kDispBuffers) + 4116u * m.r32(sc + 4);
    hw->put_disp_env(m.r32(b + 4));
    const u32 env = m.r32(b);
    build_dr_env(env);
    m.w32(env + 28, m.r32(env + 28) | 0x00FFFFFFu);
    hw->put_draw_env(env);
    if (!(m.r32(sc) & 4u)) hw->draw_otag(b + 16);
    const u32 nf = ~m.r32(sc);
    if (nf & 0x20000000u) {
        if (nf & 0x2000000u) m.w32(sc + 40, m.r32(sc + 40) + 1);
        else m.w32(sc, m.r32(sc) & ~0x2000000u);
    }
    m.w32(sc + 36, m.r32(sc + 36) + 1);
}

void Game::flow_phase_return(u32 r) {
    if (!r) return;
    const u32 sc = m.r32(g::kScene);
    if (r & 0x20000000u) {
        m.w32(sc + 44, r & 0xFFu);
        m.w32(sc + 48, 0);
    } else if (r & 0x8000000u) {
        m.w32(sc + 48, 0);
        m.w32(sc + 44, m.r32(sc + 44) + 1);
    } else if (r & 0x10000000u) {
        m.w32(sc + 48, r & 0xFFu);
    } else if (r & 0x4000000u) {
        m.w32(sc + 48, m.r32(sc + 48) + 1);
    }
    if (r & 0x28000000u) {
        const u32 grp = kGroupTable + 16u * m.r32(sc + 44);
        m.w32(sc + 52, m.r32(grp + 4));
        m.w32(sc + 60, m.r32(grp + 8));
        m.w32(sc + 64, m.r32(grp + 12));
        if (m.r32s(sc + 52) < 0) m.w32(sc + 56, 0xFFFFFFFFu);
    }
    if (r & 0x40000000u) {
        mem_heap_reset(m.r32(sc + 64));
        m.w32(sc, m.r32(sc) | 8u);
    }
}

void Game::mem_heap_reset(u32 base) {
    cover(0x800273FCu);
    const u32 sc = m.r32(g::kScene);
    m.w32(sc + 24, base);
    m.w32(sc + 28, base);
}

u32 Game::scene_phase_fn() {
    const u32 sc = m.r32(g::kScene);
    if (m.r32(sc + 52) != m.r32(sc + 56)) return scene_loader_fn();
    const u32 phases = m.r32(kGroupTable + 16u * m.r32(sc + 44));
    return m.r32(phases + 4u * m.r32(sc + 48));
}

u32 Game::main_loop(u32 result) {
    if (is_loader(part_target) && (result & 0x2000000u)) {
        const u32 sc = m.r32(g::kScene);
        m.w32(sc + 48, 0);
        m.w32(sc + 56, m.r32(sc + 52));
    }
    flow_phase_return(result);
    sp += 0x18;
    kcd_pump();
    const u32 sc = m.r32(g::kScene);
    m.w32(sc + 20, overlay_packets(m.r32(sc + 12), m.r32(sc + 20)));
    m.w32(m.r32(g::kScene) + 72, hw->get_rcnt(0xF2000001u));
    m.w32(m.r32(g::kScene) + 76, hw->get_rcnt(0xF2000001u));
    hw->vsync(m.r32s(m.r32(kDispGlobals) + 20));
    m.w32(m.r32(g::kScene) + 32, hw->vsync(-1));
    frame_present();
    const u32 s2 = m.r32(g::kScene);
    if (m.r32(s2) & 8u) m.w32(s2, m.r32(s2) & ~8u);
    u32 fl = m.r32(s2);
    if (fl & 0x10000000u) {
        m.w32(s2, fl & 0xD3FFFFFFu);
    } else {
        if (fl & 0x4000000u) m.w32(s2, (fl & 0xFBFFFFFFu) | 0x20000000u);
        fl = m.r32(s2);
        if (fl & 0x8000000u) m.w32(s2, fl & 0xF7FFFFFFu & 0xDFFFFFFFu);
    }
    return frame_start();
}

u32 Game::frame_start() {
    display_env_update();
    buffers_flip();
    pad_poll();
    const u32 s3 = m.r32(g::kScene);
    input_map(s3 + 80, s3 + 288, s3 + 348);
    sound_driver();
    sp -= 0x18;
    return scene_phase_fn();
}
}
