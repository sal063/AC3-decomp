#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kKcd = 0x800BCBA8u, kStrm = 0x800BCBACu, kFlag = 0x800BCBB0u, kRingState = 0x800BCBB4u;
constexpr u32 kIndexBuf = 0x800BCBB8u, kReq = 0x800BCBBCu;
constexpr u32 kMovieWait = 0x800BCE80u;
constexpr u32 kSectorReader = 0x80029A44u, kMovieReader = 0x80029D70u, kSyncCb = 0x8002A200u;
constexpr u32 kStepRead = 0x80029E74u, kStepStream = 0x80029FE4u, kPollStream = 0x8002A498u;
constexpr u32 kRequestDone = 0x8002BDB0u;
}

u32 Game::kcd_entry(u32 id) {
    cover(0x8002995Cu);
    const u32 kcd = m.r32(kKcd);
    return m.r32(kcd + ((id & 0x800u) ? 0x1Cu : 0x18u)) + 8u * (id & 0x7FFu);
}

void Game::kcd_cd_mix(u32 on) {
    cover(0x8002999Cu);
    Frame f(*this, 0x40);
    const u32 attr = f.at(0x10);
    m.w32(attr, 0x200);
    m.w32(attr + 24, on ? 0 : 1);
    hw->spu_set_common_attr(attr);
}

void Game::kcd_cd_volume(s32 level, u32 stereo) {
    cover(0x800299D8u);
    Frame f(*this, 0x20);
    if (level >= 0x97) level = 0x96;
    else if (level < 0) level = 0;
    const u32 v = f.at(0x10);
    if (stereo) {
        level >>= 1;
        for (u32 k = 0; k < 4; ++k) m.w8(v + k, static_cast<u32>(level));
    } else {
        m.w8(v + 2, static_cast<u32>(level));
        m.w8(v, static_cast<u32>(level));
        m.w8(v + 3, 0);
        m.w8(v + 1, 0);
    }
    hw->cd_mix(v);
}

void Game::kcd_keep_result(u32 res) {
    const u32 kcd = m.r32(kKcd);
    m.w32(kcd + 0x28, m.r32(res));
    m.w32(kcd + 0x2C, m.r32(res + 4));
}

void Game::kcd_read_fail(u32 err) {
    const u32 st = m.r32(kStrm);
    const u32 fl = m.r32(st);
    const u32 last = m.r32(st + 0x24);
    m.w32(st + 0x20, 0xFF);
    m.w32(st + 0x34, err);
    m.w32(st, (fl | 0x20u) & ~0x10u);
    if (last) {
        m.w32(st + 0x10, last + 1);
        m.w32(st + 0x24, 0);
    }
}

u32 Game::kcd_call(u32 fn, u32 a0, u32 a1) { return call_guest(fn, a0, a1); }

void Game::kcd_sector_reader(u32 intr, u32 res) {
    cover(kSectorReader);
    Frame f(*this, 0x30);
    u32 err = 0;
    kcd_keep_result(res);
    u32 n = 0x200;
    if ((intr & 0xFFu) != 1) {
        err = 2;
    } else {
        const u32 st = m.r32(kStrm);
        const u32 fl = m.r32(st) | 0x1000000u;
        m.w32(st, fl);
        if (fl & 0x10u) m.w32(st, fl & ~0x10u);
        const u32 head = f.at(0x10);
        hw->cd_get_sector(head, 3);
        const u32 pos = static_cast<u32>(hw->cd_pos_to_int(head));
        const u32 s2 = m.r32(kStrm);
        const u32 last = m.r32(s2 + 0x24);
        if (!last) {
            if (pos != m.r32(s2 + 0x10)) err = 3;
        } else if (pos != last + 1) {
            err = 4;
        }
        const u32 s3 = m.r32(kStrm);
        const s32 rem = m.r32s(s3 + 0x14);
        m.w32(s3 + 0x24, pos);
        if (rem < 0x200) n = static_cast<u32>(rem);
        hw->cd_get_sector(m.r32(s3 + 0x18), n);
        const u32 s4 = m.r32(kStrm);
        m.w32(s4 + 0x18, m.r32(s4 + 0x18) + 4 * n);
        const s32 left = m.r32s(s4 + 0x14) - static_cast<s32>(n);
        m.w32(s4 + 0x14, static_cast<u32>(left));
        if (left <= 0) {
            hw->cd_ready_callback(0);
            hw->cd_control_f(9, 0);
        }
    }
    if (err) {
        kcd_read_fail(err);
        hw->cd_ready_callback(0);
        hw->cd_control_f(9, 0);
    }
}

void Game::kcd_stream_reader() {
    cover(0x80029BD8u);
    Frame f(*this, 0x28);
    u32 chan = 0x100;
    const u32 st = m.r32(kStrm);
    const u32 fl = m.r32(st) | 0x1000000u;
    m.w32(st, fl);
    if (fl & 0x10u) m.w32(st, fl & ~0x10u);
    const u32 head = f.at(0x10);
    hw->cd_get_sector(head, 3);
    if (!(m.r8(head + 6) & 8u)) return;
    const u32 pos = static_cast<u32>(hw->cd_pos_to_int(head));
    const u32 s2 = m.r32(kStrm);
    if (m.r32(s2) & 0x1000u) chan = m.r8(head + 5);
    const s32 rem = m.r32s(s2 + 0x2C) - static_cast<s32>(pos);
    m.w32(s2 + 0x14, static_cast<u32>(rem));
    m.w32(s2 + 0x24, pos);
    const u32 now = m.r32(m.r32(m.r32(kKcd) + 0x20));
    m.w32(s2 + 0x44, now);
    m.w32(s2 + 0x3C, now + 2 + static_cast<u32>(6 * rem / 15));
    if (chan == 0 && (m.r32(s2) & 0x4000u)) {
        hw->cd_get_sector(m.r32(kFlag), 0x81);
    } else {
        const u32 nf = ~m.r32(m.r32(kStrm));
        if ((nf & 0x400u) && ((nf & 0x2000u) || chan == m.r8(m.r32(kStrm) + 0x1D))) kcd_ring_put();
    }
    if (m.r32s(m.r32(kStrm) + 0x14) <= 0) {
        kcd_cd_mix(1);
        hw->cd_ready_callback(0);
        hw->cd_control_f(9, 0);
    }
}

void Game::kcd_movie_reader(u32 intr, u32 res) {
    cover(kMovieReader);
    Frame f(*this, 0x18);
    kcd_keep_result(res);
    if ((intr & 0xFFu) != 1) {
        m.w32(m.r32(kStrm) + 0x34, 5);
        return;
    }
    if (m.r32(m.r32(kKcd) + 0x24) && hw->dec_dct_out_sync(1)) {
        m.w32(kMovieWait, 1);
        return;
    }
    kcd_stream_reader();
}

void Game::kcd_seek() {
    cover(0x80029DFCu);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kStrm);
    const u32 start = m.r32(st + 0x10);
    const u32 fl = m.r32(st);
    m.w32(kMovieWait, 0);
    m.w32(st + 0x20, 0);
    m.w32(st, (fl | 0x80000000u) & 0xFCFFEFEFu);
    const u32 loc = f.at(0x10);
    hw->cd_int_to_pos(static_cast<s32>(start), loc);
    hw->cd_sync_callback(kSyncCb);
    const u32 kcd = m.r32(kKcd);
    m.w32(kcd + 0x28, 0);
    m.w32(kcd + 0x2C, 0);
    m.w32(m.r32(kFlag), 0);
    hw->cd_control_f(2, loc);
}

void Game::kcd_sync_cb(u32 intr, u32 res) {
    cover(kSyncCb);
    Frame f(*this, 0x18);
    kcd_keep_result(res);
    intr &= 0xFFu;
    if (intr == 5) {
        if (m.r32(m.r32(kStrm)) & 0x80u) return;
        const u32 st = m.r32(kStrm);
        m.w32(st + 0x20, 0xFF);
        m.w32(st + 0x34, 1);
        const u32 fl = m.r32(st);
        const u32 last = m.r32(st + 0x24);
        m.w32(st, (fl | 0x20u) & ~0x10u);
        if (last) {
            m.w32(st + 0x10, last + 1);
            m.w32(st + 0x24, 0);
        }
    } else if (intr != 2) {
        return;
    }
    const u32 step = m.r32(m.r32(kStrm) + 0x0C);
    if (step) kcd_call(step, res, res);
}

void Game::kcd_stream_end(u32 result, bool clear_poll) {
    hw->cd_sync_callback(0);
    const u32 st = m.r32(kStrm);
    if (clear_poll) {
        m.w32(st + 0x04, 0);
        m.w32(m.r32(kFlag), 0);
    }
    const u32 done = m.r32(st + 0x08);
    if (done) kcd_call(done, result, 0);
}

void Game::kcd_step_fail(bool clear_poll) {
    const u32 st = m.r32(kStrm);
    const u32 fl = m.r32(st);
    if (fl & 0x20u) {
        m.w32(st, fl & ~0x20u);
        kcd_seek();
        return;
    }
    m.w32(st, fl | 0x80u);
    kcd_stream_end(0xFFFFFFFEu, clear_poll);
}

void Game::kcd_step_read() {
    cover(kStepRead);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kStrm);
    const s32 step = m.r32s(st + 0x20);
    if (step == 1) {
        m.w32(st + 0x24, 0);
        hw->cd_ready_callback(kSectorReader);
        m.w32(m.r32(kStrm) + 0x20, 2);
        hw->cd_control_f(6, 0);
    } else if (step == 0) {
        const u32 mode = f.at(0x10);
        m.w8(mode, 0xA0);
        m.w32(st + 0x20, 1);
        hw->cd_control_f(0xE, mode);
    } else if (step == 2) {
        m.w32(st + 0x30, 300);
        m.w32(st + 0x20, 3);
        m.w32(st, m.r32(st) | 0x10u);
    } else if (step == 3) {
        if (m.r32s(st + 0x14) > 0) return;
        m.w32(st, m.r32(st) & 0x7EFFFFFFu);
        kcd_stream_end(0, false);
    } else {
        kcd_step_fail(false);
    }
}

void Game::kcd_step_stream() {
    cover(kStepStream);
    Frame f(*this, 0x20);
    const u32 st = m.r32(kStrm);
    const s32 step = m.r32s(st + 0x20);
    switch (step) {
    case 0x20:
        m.w32(st + 0x24, 0);
        hw->cd_ready_callback(kMovieReader);
        m.w32(m.r32(kStrm) + 0x20, 0x30);
        hw->cd_control_f(0x1B, 0);
        return;
    case 0:
        m.w32(st + 0x20, 0x10);
        hw->cd_control_f(0xD, st + 0x1C);
        return;
    case 0x10: {
        const u32 mode = f.at(0x10);
        m.w8(mode, 0xE8);
        m.w32(st + 0x20, 0x20);
        m.w32(st, m.r32(st) | 0x1000u);
        hw->cd_control_f(0xE, mode);
        return;
    }
    case 0x30: {
        kcd_cd_mix(0);
        const u32 s2 = m.r32(kStrm);
        m.w32(s2 + 0x30, 300);
        m.w32(s2 + 0x20, 0x40);
        m.w32(s2, (m.r32(s2) & 0xBFFFFFFFu) | 0x10u);
        return;
    }
    case 0x40: {
        if (m.r32s(st + 0x14) <= 0) {
            m.w32(st, m.r32(st) & 0x7EFFFFFFu);
            kcd_stream_end(0, true);
            return;
        }
        const u32 fl = m.r32(st);
        if (!(fl & 0x40000000u)) return;
        if (fl & 0x2000000u) {
            m.w32(st, fl & 0xFCFFFFFFu);
            m.w32(st + 0x20, 0x30);
            hw->cd_control_f(0x1B, 0);
        } else {
            m.w32(st, fl | 0x2000000u);
        }
        return;
    }
    default:
        kcd_step_fail(true);
        return;
    }
}

u32 Game::kcd_poll_stream(u32 sync) {
    cover(kPollStream);
    Frame f(*this, 0x18);
    auto clock = [&]() { return m.r32(m.r32(m.r32(kKcd) + 0x20)); };
    const u32 st = m.r32(kStrm);
    const u32 fl = m.r32(st);
    if (fl & 0x1000000u) {
        if (!(fl & 0x40000000u))
            m.w32(st + 0x28, m.r32(st + 0x24) + static_cast<u32>(static_cast<s32>(5 * (clock() - m.r32(st + 0x44))) >> 1));
        const u32 s2 = m.r32(kStrm);
        if (m.r32(s2) & 0x10u) m.w32(s2, m.r32(s2) & ~0x10u);
        const u32 s3 = m.r32(kStrm);
        const bool due = m.r32s(s3 + 0x14) <= 0 || !(static_cast<s32>(clock()) < m.r32s(s3 + 0x3C));
        if (due && sync == 2 && !(m.r32(s3) & 0x40000000u)) {
            kcd_cd_mix(1);
            m.w32(m.r32(kStrm) + 0x14, 0);
            hw->cd_ready_callback(0);
            hw->cd_control_f(9, 0);
            return 1;
        }
        const u32 s4 = m.r32(kStrm);
        u32 f4 = m.r32(s4);
        if ((f4 & 0x40u) && sync == 2) {
            f4 &= ~0x40u;
            m.w32(s4, f4);
            if (!(f4 & 0x40000000u)) m.w32(s4, f4 | 0x40000000u);
            hw->cd_control_f(9, 0);
            return 1;
        }
        const u32 s5 = m.r32(kStrm);
        if (!(m.r32(s5) & 0x40000000u)) {
            const s32 done = static_cast<s32>(6 * (m.r32s(s5 + 0x24) - m.r32s(s5 + 0x10))) / 15;
            m.w32(s5 + 0x40, static_cast<u32>(done) + clock() - m.r32(s5 + 0x44));
        }
        return 0;
    }
    bool end = false;
    if (static_cast<s32>(fl) < 0) {
        const u32 nf = ~fl;
        if (nf & 0x40000000u) end = !(static_cast<s32>(clock()) < m.r32s(st + 0x3C));
        if ((nf & 0x40000010u) == 0x40000010u) {
            const s32 t = m.r32s(st + 0x30) - 1;
            m.w32(st + 0x30, static_cast<u32>(t));
            if (!(m.r8(m.r32(kKcd) + 0x28) & 2u) || t <= 0) end = true;
        }
    }
    if (!end) return 0;
    kcd_cd_mix(1);
    m.w32(m.r32(kStrm) + 0x14, 0);
    hw->cd_ready_callback(0);
    const u32 s6 = m.r32(kStrm);
    m.w32(s6, m.r32(s6) & 0x7FFFFFFFu);
    hw->cd_sync_callback(0);
    const u32 s7 = m.r32(kStrm);
    m.w32(s7 + 0x04, 0);
    m.w32(m.r32(kFlag), 0);
    const u32 done = m.r32(s7 + 0x08);
    if (done) kcd_call(done, 0xFFFFFFFFu, 0);
    if (!hw->cd_control(9, 0, m.r32(kKcd) + 0x28)) hw->cd_reset(0);
    return 1;
}

void Game::kcd_frame() {
    cover(0x8002A2C0u);
    Frame f(*this, 0x18);
    const u32 kcd = m.r32(kKcd);
    switch (m.r32(kcd)) {
    case 0: {
        if (m.r8(kcd + 0x28) & 0x10u) {
            m.w32(kcd + 0x0C, 4);
            kcd_lid_opened();
            return;
        }
        const u32 sync = hw->cd_sync(1, kcd + 0x28);
        const u32 poll = m.r32(m.r32(kStrm) + 0x04);
        u32 busy = 0;
        if (poll) busy = kcd_call(poll, sync, 0);
        const u32 st = m.r32(kStrm);
        if (!(m.r32(st) & 0x10u) || busy) return;
        const s32 t = m.r32s(st + 0x30);
        m.w32(st + 0x30, static_cast<u32>(t - 1));
        if (t > 0) return;
        m.w32(st + 0x20, 0xFF);
        m.w32(st, (m.r32(st) | 0x20u) & ~0x10u);
        hw->cd_control_f(9, 0);
        return;
    }
    case 2:
        if (kcd_spin_up(kcd + 0x28) == 1) {
            m.w32(m.r32(kKcd), 3);
            m.w32(m.r32(kKcd) + 4, 0);
        }
        return;
    case 3: {
        const s32 r = kcd_spin_ready(kcd + 0x28);
        if (r < 0) {
            m.w32(m.r32(kKcd), 2);
            m.w32(m.r32(kKcd) + 4, 0);
        } else if (r == 1) {
            m.w32(m.r32(kKcd), 4);
            m.w32(m.r32(kKcd) + 4, 0);
        }
        return;
    }
    case 4: {
        if (m.r8(m.r32(kKcd) + 0x28) & 0x10u) {
            kcd_lid_opened();
            return;
        }
        const s32 r = kcd_read_index();
        if (r == 1) {
            m.w32(m.r32(kKcd), 0);
            m.w32(m.r32(kKcd) + 4, 0);
        } else if (r < 0) {
            m.w32(m.r32(kKcd), 2);
            m.w32(m.r32(kKcd) + 4, 0);
        }
        return;
    }
    default: return;
    }
}

void Game::kcd_lid_opened() {
    cover(0x8002B428u);
    Frame f(*this, 0x18);
    if (m.r32(m.r32(kKcd))) return;
    kcd_stop(1);
    m.w32(m.r32(kKcd), 2);
    m.w32(m.r32(kKcd) + 4, 0);
}

s32 Game::kcd_spin_up(u32 res) {
    cover(0x8002B094u);
    Frame f(*this, 0x20);
    if (m.r8(res) & 0x10u) return 1;
    const u32 sync = hw->cd_sync(1, res);
    if (sync != 2) return 0;
    const u32 kcd = m.r32(kKcd);
    const s32 sub = m.r32s(kcd + 4);
    if (sub == 1) {
        if (hw->cd_control(8, 0, res)) m.w32(m.r32(kKcd) + 4, 2);
        return 0;
    }
    if (sub >= 2) {
        if (sub == 2) hw->cd_control_b(1, 0, res);
        return 0;
    }
    if (sub == 0) {
        m.w8(0x800BCE70u, 0);
        if (hw->cd_control_b(0xE, 0x800BCE70u, res)) m.w32(m.r32(kKcd) + 4, 1);
    }
    return 0;
}

s32 Game::kcd_spin_ready(u32 res) {
    cover(0x8002B194u);
    Frame f(*this, 0x18);
    if (((m.r8(res) & 1u) && (m.r8(res + 1) & 0x40u)) || ((m.r8(res) & 4u) && (m.r8(res + 1) & 4u))) {
        m.w32(m.r32(kKcd) + 0x0C, 5);
        return -1;
    }
    if (m.r8(res) & 0x10u) {
        const u32 kcd = m.r32(kKcd);
        m.w32(kcd + 4, 0);
        m.w32(kcd + 0x0C, 4);
    }
    if (hw->cd_sync(1, res) != 2) return 0;
    const u32 kcd = m.r32(kKcd);
    auto clock = [&]() { return m.r32(m.r32(m.r32(kKcd) + 0x20)); };
    switch (m.r32(kcd + 4)) {
    case 0:
        if (!(m.r8(res) & 0x10u)) {
            m.w32(kcd + 4, 1);
            m.w32(kcd + 0x0C, 0);
            m.w32(kcd + 8, clock());
        }
        hw->cd_control_b(1, 0, res);
        return 0;
    case 1:
        if (!(static_cast<s32>(m.r32(kcd + 8) + 0xB4) < static_cast<s32>(clock()))) {
            if (m.r8(res) & 2u) m.w32(kcd + 4, 2);
            hw->cd_control_b(1, 0, res);
            return 0;
        }
        m.w32(kcd + 0x0C, 4);
        return -2;
    case 2:
        if (hw->cd_control_b(0x13, 0, res)) m.w32(m.r32(kKcd) + 4, 3);
        return 0;
    case 3:
        m.w8(0x800BCE74u, 0);
        m.w8(0x800BCE75u, 2);
        m.w8(0x800BCE76u, 0);
        m.w8(0x800BCE77u, 0);
        if (hw->cd_control(2, 0x800BCE74u, res)) m.w32(m.r32(kKcd) + 4, 4);
        return 0;
    case 4:
        if (hw->cd_control(0x15, 0, res)) m.w32(m.r32(kKcd) + 4, 5);
        return 0;
    case 5:
        if (hw->cd_control(1, 0, res)) m.w32(m.r32(kKcd) + 4, 6);
        return 0;
    case 6:
        m.w8(0x800BCE71u, 0x80);
        if (hw->cd_control(0xE, 0x800BCE71u, res)) {
            m.w32(m.r32(kKcd) + 4, 7);
            m.w32(m.r32(kKcd) + 8, clock());
        }
        return 0;
    case 7:
        if (static_cast<s32>(m.r32(kcd + 8) + 3) < static_cast<s32>(clock())) return 1;
        return 0;
    default:
        return 0;
    }
}

s32 Game::kcd_find_files(u32 names, s32 n, u32 pos, u32 size) {
    cover(0x8002ACD4u);
    Frame f(*this, 0x48);
    const u32 file = f.at(0x10);
    for (s32 k = 0; k < n; ++k) {
        if (!hw->cd_search_file(file, m.r32(names))) return -1;
        names += 4;
        m.w32(pos, m.r32(file));
        m.w32(size, m.r32(file + 4));
        pos += 4;
        size += 4;
    }
    return 1;
}

void Game::kcd_read_blocking(u32 n, u32 pos, u32 dst) {
    cover(0x8002AD90u);
    Frame f(*this, 0x28);
    const u32 res = m.r32(kKcd) + 0x28;
    while (!hw->cd_control_b(2, pos, res)) {
    }
    while (!hw->cd_read(n, dst, 0x80)) {
    }
    hw->cd_read_sync(0, f.at(0x10));
}

s32 Game::kcd_read_index() {
    cover(0x8002AE0Cu);
    Frame f(*this, 0x18);
    const u32 bph = m.r32(kIndexBuf), sph = bph + 0x2000;
    m.w32(0x800BCE78u, bph);
    m.w32(0x800BCE7Cu, sph);
    hw->cd_sync_callback(0);
    hw->cd_data_callback(0);
    hw->cd_ready_callback(0);
    const s32 r = kcd_find_files(0x800B0380u, 4, 0x800C1368u, 0x800C1378u);
    if (r <= 0) {
        m.w32(m.r32(kKcd) + 0x0C, 5);
        return r;
    }
    kcd_read_blocking(4, 0x800C1368u, m.r32(0x800BCE78u));
    kcd_read_blocking(4, 0x800C136Cu, m.r32(0x800BCE7Cu));
    const u32 b = m.r32(0x800BCE78u), s = m.r32(0x800BCE7Cu);
    if (m.r32(b) != 0x45334341u || m.r32(s) != 0x45334341u) {
        m.w32(m.r32(kKcd) + 0x0C, 5);
        return 1;
    }
    const u32 kcd = m.r32(kKcd);
    m.w32(kcd + 0x10, static_cast<u32>(hw->cd_pos_to_int(0x800C1370u)));
    m.w32(kcd + 0x14, static_cast<u32>(hw->cd_pos_to_int(0x800C1374u)));
    m.w32(kcd + 0x18, b + 20);
    m.w32(kcd + 0x1C, s + 20);
    for (s32 k = 0; k < m.r32s(b + 16); ++k) m.w32(b + 24 + 8 * k, m.r32(b + 24 + 8 * k) + m.r32(kcd + 0x10));
    for (s32 k = 0; k < m.r32s(s + 16); ++k) m.w32(s + 24 + 8 * k, m.r32(s + 24 + 8 * k) + m.r32(kcd + 0x14));
    m.w32(kcd + 0x0C, 0);
    if (m.r32(s + 4)) m.w32(kcd + 0x0C, m.r32(s + 4));
    if (m.r32(b + 8) != 0x427) m.w32(kcd + 0x0C, 5);
    if (m.r32(s + 8) != 0x19990427u) m.w32(kcd + 0x0C, 5);
    return 1;
}

s32 Game::kcd_pause(u32 on) {
    cover(0x8002ABE0u);
    const u32 st = m.r32(kStrm);
    const u32 fl = m.r32(st);
    const s32 was = static_cast<s32>((fl >> 30) & 1u);
    if (!(fl & 0x80000000u)) return -1;
    if (on) {
        if (fl & 0x40000000u) return was;
    } else if (!(fl & 0x40000000u)) {
        return was;
    }
    m.w32(st, fl | 0x40u);
    return was;
}

u32 Game::kcd_stop(u32 how) {
    cover(0x8002AC3Cu);
    Frame f(*this, 0x18);
    hw->cd_ready_callback(0);
    hw->cd_sync_callback(0);
    const u32 st = m.r32(kStrm);
    m.w32(st, 0);
    m.w32(st + 0x04, 0);
    m.w32(st + 0x20, 0);
    m.w32(m.r32(kFlag), 0);
    kcd_cd_mix(1);
    if (hw->cd_sync(1, m.r32(kKcd) + 0x28) != 2) return 0;
    u32 com = 9;
    if (how) com = how == 1 ? 7 : 8;
    return hw->cd_control(com, 0, m.r32(kKcd) + 0x28) ? 1u : 0u;
}

void Game::kcd_ring_init(u32 base, s32 size, u32 form2) {
    cover(0x8002B610u);
    const s32 unit = form2 ? 0x918 : 0x800;
    const s32 slots = size / unit;
    const u32 r = m.r32(kRingState);
    m.w32(r, 8);
    m.w32(r + 0x10, base);
    m.w32(r + 0x08, form2);
    m.w32(r + 0x18, 0);
    m.w32(r + 0x28, 0);
    m.w32(r + 0x34, 0);
    m.w32(r + 0x38, 0);
    m.w32(r + 0x0C, static_cast<u32>(slots));
    m.w32(r + 0x14, base + static_cast<u32>(8 * slots) * 4);
    m.w32(r + 0x1C, base);
    m.w32(r + 0x04, static_cast<u32>((unit - 32) >> 2));
    m.w32(r + 0x20, m.r32(r + 0x14));
    m.w32(r + 0x24, 0xFFFFFFFFu);
    m.w32(r + 0x2C, m.r32(r + 0x1C));
    m.w32(r + 0x30, m.r32(r + 0x20));
    m.w32(r + 0x3C, m.r32(r + 0x1C));
    m.w32(r + 0x40, m.r32(r + 0x20));
}

void Game::kcd_ring_clear() {
    cover(0x8002B6D0u);
    const u32 r = m.r32(kRingState);
    u32 p = m.r32(r + 0x10);
    const u32 stride = m.r32(r) * 4;
    for (s32 k = 0; k < m.r32s(m.r32(kRingState) + 0x0C); ++k, p += stride) m.w8(p, 0);
}

void Game::kcd_ring_put() {
    cover(0x8002B760u);
    Frame f(*this, 0x20);
    u32 r = m.r32(kRingState);
    u32 e = m.r32(r + 0x2C);
    const u32 b = m.r8(e);
    if (b == 2 || b == 4) return;
    hw->cd_get_sector(e, m.r32(r));
    if (m.r8(e) != 1) {
        m.w8(e, 0);
        return;
    }
    r = m.r32(kRingState);
    if (m.r32(r + 0x24) != m.r16(e + 6)) {
        if (m.r8(e + 1)) return;
        u32 slot = m.r32(r + 0x1C);
        m.w32(r + 0x2C, slot);
        m.w32(r + 0x30, m.r32(r + 0x20));
        if (m.r32s(r + 0x0C) < m.r32s(r + 0x18) + static_cast<s32>(m.r8(e + 2))) {
            m.w8(e, 4);
            const u32 r2 = m.r32(kRingState);
            m.w32(r2 + 0x1C, m.r32(r2 + 0x10));
            const u32 data = m.r32(r2 + 0x14);
            slot = m.r32(r2 + 0x1C);
            m.w32(r2 + 0x18, 0);
            m.w32(r2 + 0x2C, slot);
            m.w32(r2 + 0x20, data);
            m.w32(r2 + 0x30, data);
            if (m.r8(slot) == 2) return;
        }
        const u32 r3 = m.r32(kRingState);
        m.w32(r3 + 0x24, m.r16(e + 6));
        if (e != slot) {
            u32 from = e, to = slot;
            e = m.r32(r3 + 0x1C);
            for (s32 k = 0; k < m.r32s(m.r32(kRingState)); ++k, from += 4, to += 4) m.w32(to, m.r32(from));
        }
    } else if (m.r32(r + 0x28) + 1 != m.r8(e + 1)) {
        return;
    }
    r = m.r32(kRingState);
    const u32 words = m.r32(r + 0x04), stride = m.r32(r);
    m.w32(r + 0x28, m.r8(e + 1));
    hw->cd_get_sector(m.r32(r + 0x30), words);
    r = m.r32(kRingState);
    if (m.r32(r + 0x28) + 1 == m.r8(e + 2)) {
        u32 slot = m.r32(r + 0x1C), data = m.r32(r + 0x20);
        u32 k = 0;
        do {
            m.w8(slot, 2);
            slot += stride * 4;
            ++k;
            data += words * 4;
        } while (k < m.r8(e + 2));
        r = m.r32(kRingState);
        m.w32(r + 0x34, m.r32(r + 0x34) + 1);
        const s32 used = m.r32s(r + 0x18) + static_cast<s32>(m.r8(e + 2));
        m.w32(r + 0x18, static_cast<u32>(used));
        if (used < m.r32s(r + 0x0C)) {
            m.w32(r + 0x1C, slot);
            m.w32(r + 0x20, data);
        } else {
            m.w32(r + 0x18, 0);
            m.w32(r + 0x1C, m.r32(r + 0x10));
            m.w32(r + 0x20, m.r32(r + 0x14));
        }
        r = m.r32(kRingState);
        m.w32(r + 0x2C, m.r32(r + 0x1C));
        m.w32(r + 0x30, m.r32(r + 0x20));
    } else {
        m.w32(r + 0x2C, m.r32(r + 0x2C) + stride * 4);
        m.w32(r + 0x30, m.r32(r + 0x30) + words * 4);
    }
}

s32 Game::kcd_request_start(u32 entry, u32 dst, u32 ring_size, u32 done, u32 flags) {
    cover(0x8002A79Cu);
    Frame f(*this, 0x30);
    if (m.r32(m.r32(kKcd))) return -1;
    const u32 sync = hw->cd_sync(1, m.r32(kKcd) + 0x28);
    if (sync != 2) return 0;
    if (!(m.r32(entry) & 0x80000000u)) {
        m.w32(m.r32(kFlag), 0);
        kcd_call(done, 0xFFFFFFFDu, 0);
        return 1;
    }
    const u32 kcd = m.r32(kKcd), st = m.r32(kStrm);
    m.w32(kcd + 0x24, 0);
    m.w32(st, 0);
    m.w32(st + 0x40, 0);
    m.w32(st + 0x34, 0);
    m.w32(st + 0x08, done);
    m.w32(st + 0x10, m.r32(entry + 4));
    const u32 kind = (m.r32(entry) >> 27) & 3u;
    const u32 now = m.r32(m.r32(kcd + 0x20));
    auto size_of = [&]() { return (m.r32(entry) & 0x7FFFFu) << ((m.r32(entry) >> 24) & 7u); };
    auto deadline = [&](u32 s, s32 n) { m.w32(s + 0x3C, now + 2 + static_cast<u32>(6 * n / 15)); };
    if (kind == 0) {
        m.w32(st + 0x0C, kStepRead);
        m.w32(st + 0x04, 0);
        m.w32(st + 0x18, dst);
        const s32 n = static_cast<s32>(m.r32(entry) & 0x7FFFFu);
        m.w32(st + 0x14, static_cast<u32>(n));
        s32 v = n - 1;
        if (v < 0) v = n + 0x1FE;
        m.w32(st + 0x2C, m.r32(st + 0x10) + static_cast<u32>(v >> 9));
        kcd_seek();
        return 1;
    }
    if (kind == 2) {
        kcd_cd_mix(1);
        if (flags & 0x1000u) m.w32(m.r32(kStrm), m.r32(m.r32(kStrm)) | 0x4000u);
        const u32 s = m.r32(kStrm);
        m.w32(s + 0x0C, kStepStream);
        m.w32(s + 0x04, kPollStream);
        m.w8(s + 0x1C, 1);
        m.w32(s, m.r32(s) | 0x400u);
        m.w8(m.r32(kStrm) + 0x1D, (m.r32(entry) >> 19) & 0x1Fu);
        const u32 s2 = m.r32(kStrm);
        const u32 n = size_of();
        m.w32(s2 + 0x14, n);
        m.w32(s2 + 0x3C, now + 2);
        m.w32(s2 + 0x2C, m.r32(s2 + 0x10) + n - 1);
        deadline(s2, static_cast<s32>(n));
        kcd_seek();
        return 1;
    }
    if (kind == 3) {
        kcd_cd_mix(1);
        if (flags & 0x1000u) m.w32(m.r32(kStrm), m.r32(m.r32(kStrm)) | 0x4000u);
        else if (flags & 0x4000u) m.w32(m.r32(kStrm), m.r32(m.r32(kStrm)) | 0x8000u);
    }
    if (kind == 1 || kind == 3) {
        if (flags & 0x800u) m.w32(m.r32(kKcd) + 0x24, 1);
        const u32 s = m.r32(kStrm);
        m.w32(s + 0x0C, kStepStream);
        m.w32(s + 0x04, kPollStream);
        if (kind == 1) m.w32(s + 0x18, dst);
        m.w8(s + 0x1C, 1);
        if (kind == 3) m.w32(s + 0x18, dst);
        m.w32(s, m.r32(s) | 0x2000u);
        m.w8(m.r32(kStrm) + 0x1D, (m.r32(entry) >> 19) & 0x1Fu);
        const u32 s2 = m.r32(kStrm);
        const u32 n = size_of();
        m.w32(s2 + 0x14, n);
        m.w32(s2 + 0x2C, m.r32(s2 + 0x10) + n - 1);
        kcd_ring_init(dst, static_cast<s32>(ring_size), (flags & 0x2000u) ? 1 : 0);
        kcd_ring_clear();
        const u32 s3 = m.r32(kStrm);
        m.w32(s3 + 0x3C, now + 2);
        deadline(s3, m.r32s(s3 + 0x14));
        kcd_seek();
        return 1;
    }
    return -1;
}

u32 Game::kcd_request_done(u32 result) {
    cover(kRequestDone);
    const u32 q = m.r32(kReq);
    m.w32(q + 0x168, 1);
    m.w32(q + 0x16C, result);
    return 1;
}

u32 Game::kcd_cmd_wait(u32 rec) {
    const u32 q = m.r32(kReq);
    if (!m.r32(q + 0x168)) return 0;
    if (m.r32(q + 0x16C) != 0xFFFFFFFEu) {
        m.w32(q + 0x174, 0xFFFFFFFFu);
        return 1;
    }
    m.w8(rec + 1, 0);
    return 0;
}

u32 Game::kcd_cmd_none(u32) {
    cover(0x8002BDC4u);
    return 1;
}

u32 Game::kcd_cmd_stop(u32 rec) {
    cover(0x8002BDCCu);
    Frame f(*this, 0x18);
    m.w32(m.r32(kReq) + 0x174, 0xFFFFFFFFu);
    const u32 r = kcd_stop(m.r32(rec + 4));
    if (r) m.w32(m.r32(kReq) + 0x178, 0);
    return r;
}

u32 Game::kcd_cmd_stream(u32 rec) {
    cover(0x8002BE14u);
    Frame f(*this, 0x20);
    const u32 st = m.r8(rec + 1);
    if (st == 1) return kcd_cmd_wait(rec);
    if (st) return 0;
    const u32 q = m.r32(kReq);
    m.w32(q + 0x174, static_cast<u32>(m.r16s(rec + 2)));
    m.w32(q + 0x168, 0);
    const u32 e = kcd_entry(static_cast<u32>(m.r16s(rec + 2)));
    m.w32(f.at(0x10), 0);
    if (kcd_request_start(e, m.r32(rec + 4), 0, kRequestDone, 0) == 1) m.w8(rec + 1, 1);
    return 0;
}

u32 Game::kcd_cmd_video(u32 rec) {
    cover(0x8002C1D4u);
    Frame f(*this, 0x28);
    const u32 st = m.r8(rec + 1);
    if (st == 1) return kcd_cmd_wait(rec);
    if (st) return 0;
    const u32 q = m.r32(kReq);
    m.w32(q + 0x164, 0xFF);
    kcd_cd_volume(0xFF, m.r32(q + 0x160));
    u32 flags = m.r32(rec + 12);
    if (!(m.r16(rec + 2) & 0x1000u)) flags &= 0xFFFFEFFFu;
    const u32 q2 = m.r32(kReq);
    m.w32(q2 + 0x168, 0);
    m.w32(q2 + 0x174, static_cast<u32>(m.r16s(rec + 2)));
    const u32 e = kcd_entry(static_cast<u32>(m.r16s(rec + 2)));
    m.w32(f.at(0x10), flags);
    if (kcd_request_start(e, m.r32(rec + 4), m.r32(rec + 8), kRequestDone, flags) == 1) m.w8(rec + 1, 1);
    return 0;
}

u32 Game::kcd_cmd_pause(u32 rec) {
    cover(0x8002C41Cu);
    Frame f(*this, 0x18);
    kcd_pause(m.r32(rec + 4));
    m.w32(m.r32(kReq) + 0x178, m.r32(rec + 4));
    return 1;
}

u32 Game::kcd_command(u32 rec) {
    switch (m.r8(rec)) {
    case 0: return kcd_cmd_none(rec);
    case 1: return kcd_cmd_stop(rec);
    case 2: return kcd_cmd_stream(rec);
    case 3: return kcd_cmd_voice(rec);
    case 4: return kcd_cmd_video(rec);
    case 5: return kcd_cmd_fade_stop(rec);
    case 6: return kcd_cmd_pause(rec);
    default:
        unported(0x800B0390u + 4 * m.r8(rec), "a KCd command past the table");
        return 0;
    }
}

u32 Game::kcd_cmd_voice(u32 rec) {
    cover(0x8002BEE4u);
    Frame f(*this, 0x30);
    const u32 q = m.r32(kReq);
    if (m.r32(q + 0x178)) return 0;
    const u32 st = m.r8(rec + 1);
    const u32 p = m.r32(rec + 4);
    if (st == 1) {
        if (kcd_stream_flags() & 0x1000000u) {
            const s32 d = m.r32s(m.r32(g::kScene) + 32) - m.r32s(m.r32(kReq) + 0x170);
            const s32 div = m.r16s(p + 4);
            s32 lvl = div ? (255 * d) / div : (255 * d) >> 7;
            if (lvl < 0) lvl = 0;
            if (lvl >= 256) {
                lvl = 0xFF;
                m.w8(rec + 1, 2);
            }
            const u32 q2 = m.r32(kReq);
            m.w32(q2 + 0x164, static_cast<u32>(lvl));
            kcd_cd_volume(lvl, m.r32(q2 + 0x160));
        } else {
            m.w32(m.r32(kReq) + 0x170, m.r32(m.r32(g::kScene) + 32));
        }
        if (m.r32(m.r32(kReq) + 0x168)) m.w8(rec + 1, 2);
        return 0;
    }
    if (st == 2) {
        const u32 q2 = m.r32(kReq);
        if (!m.r32(q2 + 0x168)) return 0;
        if (m.r32(q2 + 0x16C) == 0xFFFFFFFEu) {
            m.w8(rec + 1, 0);
        } else if (!m.r32(p + 8)) {
            m.w32(q2 + 0x174, 0xFFFFFFFFu);
            return 1;
        } else {
            m.w8(rec + 1, 0);
            m.w32(rec + 4, m.r32(p + 8));
        }
        return 0;
    }
    if (st) return 0;
    m.w32(q + 0x164, m.r16s(p + 2) ? 0u : 0xFFu);
    const u32 q2 = m.r32(kReq);
    kcd_cd_volume(m.r32s(q2 + 0x164), m.r32(q2 + 0x160));
    u32 id = static_cast<u32>(m.r16s(p));
    if (!m.r32(rec + 8)) id &= 0xFFFFEFFFu;
    m.w32(m.r32(kReq) + 0x174, id);
    const u32 e = kcd_entry(id);
    const u32 loc = f.at(0x18);
    const u32 w0 = m.r32(e);
    m.w32(loc, w0);
    m.w32(loc + 4, m.r32(e + 4) + static_cast<u32>(m.r16s(p + 6)));
    const u32 skip = static_cast<u32>(m.r16s(p + 6) >> (m.r8(e + 3) & 7u));
    m.w32(loc, (w0 & 0xFFF80000u) | (((w0 & 0x7FFFFu) - skip) & 0x7FFFFu));
    m.w32(m.r32(kReq) + 0x168, 0);
    const u32 flags = (id & 0x1000u) ? 0x1000u : 0u;
    m.w32(f.at(0x10), flags);
    if (kcd_request_start(loc, 0, 0, kRequestDone, flags) != 1) return 0;
    if (m.r16s(p + 2)) {
        m.w8(rec + 1, 1);
        m.w32(m.r32(kReq) + 0x170, m.r32(m.r32(g::kScene) + 32));
        return 0;
    }
    m.w8(rec + 1, 2);
    return 0;
}

u32 Game::kcd_cmd_fade_stop(u32 rec) {
    cover(0x8002C2E4u);
    Frame f(*this, 0x18);
    if (m.r32(m.r32(kReq) + 0x178)) return 0;
    const u32 st = m.r8(rec + 1);
    if (st == 2) {
        const u32 r = kcd_stop(0);
        if (r) m.w32(m.r32(kReq) + 0x178, 0);
        return r;
    }
    if (st > 2) return 0;
    if (st == 0) {
        m.w32(m.r32(kReq) + 0x170, m.r32(m.r32(g::kScene) + 32));
        m.w8(rec + 1, 1);
    }
    const s32 d = m.r32s(m.r32(g::kScene) + 32) - m.r32s(m.r32(kReq) + 0x170);
    s32 v = 0x1000 - (static_cast<s32>(static_cast<u32>(d) << 12) >> 6);
    if (v >= 0x1001) v = 0x1000;
    if (v < 0) v = 0;
    s32 lvl = (m.r32s(m.r32(kReq) + 0x164) * v) >> 12;
    if (lvl < 5) {
        m.w8(rec + 1, 2);
        lvl = 0;
        m.w32(m.r32(kReq) + 0x164, 0);
    }
    kcd_cd_volume(lvl, m.r32(m.r32(kReq) + 0x160));
    return 0;
}

u32 Game::kcd_stream_flags() {
    cover(0x8002B588u);
    return m.r32(m.r32(kStrm));
}

u32 Game::kcd_run(u32 rec) {
    cover(0x8002C45Cu);
    Frame f(*this, 0x20);
    const u32 r = kcd_command(rec);
    if (!r) return 0;
    const u32 cb = m.r32(rec + 16);
    if (cb) kcd_call(cb, m.r32(m.r32(kReq) + 0x16C), 0);
    return r;
}

void Game::kcd_pump() {
    cover(0x8002C4DCu);
    Frame f(*this, 0x18);
    kcd_frame();
    const u32 q = m.r32(kReq);
    if (m.r32(q + 0x148)) {
        if (kcd_run(q + 0x14C)) m.w32(m.r32(kReq) + 0x148, 0);
        return;
    }
    const s32 taken = m.r32s(q);
    if (taken >= m.r32s(q + 4)) return;
    if (kcd_run(q + 8 + 20 * (static_cast<u32>(taken) & 0xFu))) m.w32(m.r32(kReq), m.r32(m.r32(kReq)) + 1);
}

u32 Game::call_guest(u32 fn, u32 a0, u32 a1) {
    switch (fn) {
    case kSectorReader: kcd_sector_reader(a0, a1); return 0;
    case kMovieReader: kcd_movie_reader(a0, a1); return 0;
    case kSyncCb: kcd_sync_cb(a0, a1); return 0;
    case kStepRead: kcd_step_read(); return 0;
    case kStepStream: kcd_step_stream(); return 0;
    case kPollStream: return kcd_poll_stream(a0);
    case kRequestDone: return kcd_request_done(a0);
    case 0x8009B6ACu:
        cover(0x8009B6ACu);
        m.w32(0x800BD0C8u, 0);
        return 0;
    case 0x800357B4u:
        cover(0x800357B4u);
        m.w32(m.r32(0x800BCBE0u) + 8, 0);
        return m.r32(0x800BCBE0u);
    case 0x8009B904u:
        cover(0x8009B904u);
        m.w32(m.r32(0x800BCE60u) + 0x44, 0);
        return 0;
    case 0x80057170u: gmv_strip_done(); return 0;
    case 0x80056C9Cu: return gmv_stream_ended();
    case 0x80057CC4u: return gmv_subtitles_loaded();
    case 0x80056A44u: return radio_stream_ended();
    default: return call_by_address(fn, "a function called through a pointer", a0, a1, 0, 0);
    }
}
}
