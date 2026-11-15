#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kCdDebug = 0x800B6264u;
constexpr u32 kCdStatus = 0x800B6268u;
constexpr u32 kCdStatus2 = 0x800B626Cu;
constexpr u32 kCdShellOpens = 0x800B6270u;
constexpr u32 kCdMode = 0x800B6278u;
constexpr u32 kCdLastCom = 0x800B6279u;
constexpr u32 kSyncCallback = 0x800B625Cu, kReadyCallback = 0x800B6260u;
constexpr u32 kReadCallback = 0x800B61C4u;
constexpr u32 kReadMode = 0x800B61C8u;

constexpr u32 kComSyncs = 0x800B6320u;
constexpr u32 kComStatus = 0x800B6420u;
constexpr u32 kRegIndex = 0x800B6520u;
constexpr u32 kRegResponse = 0x800B6524u;
constexpr u32 kRegEnable = 0x800B6528u;
constexpr u32 kRegFlags = 0x800B652Cu;
constexpr u32 kSyncStatus = 0x800B6538u;
constexpr u32 kReadyStatus = 0x800B6539u;
constexpr u32 kEndStatus = 0x800B653Au;
constexpr u32 kSyncResult = 0x800C7EC0u, kReadyResult = 0x800C7EC8u, kEndResult = 0x800C7ED0u;
constexpr u32 kStatusErrors = 0x1Du;
constexpr u32 kStatusShellOpen = 0x10u;
constexpr u32 kIntrNone = 0, kIntrAck = 1, kIntrSync = 2, kIntrReady = 4, kIntrError = 6;
constexpr u32 kCdlDataReady = 1, kCdlComplete = 2, kCdlAcknowledge = 3, kCdlDataEnd = 4, kCdlDiskError = 5;

constexpr u32 kSector = 0x800C9AF8u;
constexpr u32 kSectorSize = 0x800u;
constexpr u32 kDirs = 0x800C84F8u;
constexpr u32 kDirSize = 44u, kDirMax = 128u;
constexpr u32 kDirParent = 4, kDirExtent = 8, kDirName = 12;
constexpr u32 kFiles = 0x800C7EF8u;
constexpr u32 kFileSize = 24u, kFileMax = 64u;
constexpr u32 kFileBytes = 4, kFileName = 8;
constexpr u32 kCachedDir = 0x800B6574u;
constexpr u32 kIsoId = 0x80024EDCu;
constexpr u32 kDotName = 0x80024FCCu, kDotDotName = 0x80024FD0u;

constexpr u32 kRead = 0x800B6584u;
constexpr u32 kReadSectors = kRead + 0x00;
constexpr u32 kReadBuffer = kRead + 0x04;
constexpr u32 kReadCursor = kRead + 0x08;
constexpr u32 kReadModeByte = kRead + 0x0C;
constexpr u32 kReadWords = kRead + 0x10;
constexpr u32 kReadLeft = kRead + 0x14;
constexpr u32 kReadTick = kRead + 0x18;
constexpr u32 kReadStart = kRead + 0x1C;
constexpr u32 kReadLba = kRead + 0x20;
constexpr u32 kReadDone = kRead + 0x24;
constexpr u32 kReadSavedSync = kRead + 0x28, kReadSavedReady = kRead + 0x2C, kReadSavedData = kRead + 0x30;
constexpr u32 kReadResult = kRead + 0x34;
constexpr s32 kReadTimeout = 0x4B0;
constexpr u32 kCdReadSyncDoneFn = 0x800A5598u, kCdReadReadyFn = 0x800A55CCu, kCdReadDataFn = 0x800A5840u;
constexpr u32 kCdlNop = 1, kCdlSetloc = 2, kCdlReadN = 6, kCdlPause = 9, kCdlSetmode = 14;
}

u32 Game::cd_init_try() {
    cover(0x800A2DE8u);
    Frame f(*this, 0x18);
    return 1;
}

u32 Game::cd_deliver(u32 fn, u32 spec) {
    cover(fn);
    Frame f(*this, 0x18);
    hw->deliver_event_hw(0xF0000003u, spec);
    return 0;
}

u32 Game::cd_status() {
    cover(0x800A2EA8u);
    return m.r8(kCdStatus);
}

u32 Game::cd_mode() {
    cover(0x800A2EB8u);
    return m.r8(kCdMode);
}

u32 Game::cd_last_com() {
    cover(0x800A2EC8u);
    return m.r8(kCdLastCom);
}

u32 Game::cd_set_debug(u32 level) {
    cover(0x800A2F70u);
    const u32 old = m.r32(kCdDebug);
    m.w32(kCdDebug, level);
    return old;
}

u32 Game::cd_read_callback(u32 fn) {
    cover(0x800A5F38u);
    const u32 old = m.r32(kReadCallback);
    m.w32(kReadCallback, fn);
    return old;
}

u32 Game::cd_read_mode(u32 mode) {
    cover(0x800A5F4Cu);
    const u32 old = m.r32(kReadMode);
    m.w32(kReadMode, mode);
    return old;
}

u32 Game::cd_getintr() {
    cover(0x800A34B8u);
    Frame f(*this, 0x30);
    const u32 irq = f.at(0x10), res = f.at(0x18);
    m.w8(m.r32(kRegIndex), 1);
    const u32 flags = m.r32(kRegFlags);
    m.w8(irq, m.r8(flags) & 7u);
    if (!m.r8(irq)) return kIntrNone;
    while (m.r8(irq) != (m.r8(flags) & 7u)) m.w8(irq, m.r8(flags) & 7u);
    u32 n = 0;
    while (n < 8 && (m.r8(m.r32(kRegIndex)) & 0x20u)) {
        m.w8(res + n, m.r8(m.r32(kRegResponse)));
        ++n;
    }
    for (u32 k = n; k < 8; ++k) m.w8(res + k, 0);
    m.w8(m.r32(kRegIndex), 1);
    m.w8(m.r32(kRegFlags), 7);
    m.w8(m.r32(kRegEnable), 7);

    u32 errors = 0;
    if (m.r8(irq) != kCdlAcknowledge || m.r32(kComStatus + 4 * m.r8(kCdLastCom))) {
        if (!(m.r32(kCdStatus) & kStatusShellOpen) && (m.r8(res) & kStatusShellOpen))
            m.w32(kCdShellOpens, m.r32(kCdShellOpens) + 1);
        const u32 status = m.r8(res), second = m.r8(res + 1);
        errors = status & kStatusErrors;
        m.w32(kCdStatus, status);
        m.w32(kCdStatus2, second);
    }
    auto keep = [&](u32 dst) { m.copy(dst, res, 8); };
    switch (m.r8(irq)) {
    case kCdlAcknowledge:
        if (errors) {
            m.w8(kSyncStatus, kCdlDiskError);
            keep(kSyncResult);
            return kIntrSync;
        }
        if (m.r32(kComSyncs + 4 * m.r8(kCdLastCom))) {
            m.w8(kSyncStatus, kCdlAcknowledge);
            keep(kSyncResult);
            return kIntrAck;
        }
        m.w8(kSyncStatus, kCdlComplete);
        keep(kSyncResult);
        return kIntrSync;
    case kCdlComplete:
        m.w8(kSyncStatus, errors ? kCdlDiskError : kCdlComplete);
        keep(kSyncResult);
        return kIntrSync;
    case kCdlDataReady:
        if (errors && n == 1) errors = 0;
        m.w8(kReadyStatus, errors ? kCdlDiskError : kCdlDataReady);
        keep(kReadyResult);
        m.w8(m.r32(kRegIndex), 0);
        m.w8(m.r32(kRegFlags), 0);
        return kIntrReady;
    case kCdlDataEnd:
        m.w8(kEndStatus, kCdlDataEnd);
        m.w8(kReadyStatus, m.r8(kEndStatus));
        keep(kEndResult);
        keep(kReadyResult);
        return kIntrReady;
    case kCdlDiskError:
        m.w8(kReadyStatus, kCdlDiskError);
        m.w8(kSyncStatus, m.r8(kReadyStatus));
        keep(kSyncResult);
        keep(kReadyResult);
        return kIntrError;
    default:
        return kIntrNone;
    }
}

u32 Game::cd_intr() {
    cover(0x800A4B48u);
    Frame f(*this, 0x28);
    const u32 index = m.r8(m.r32(kRegIndex)) & 3u;
    for (;;) {
        const u32 got = cd_getintr();
        if (!got) break;
        if (got & kIntrReady)
            if (const u32 cb = m.r32(kReadyCallback)) lib_call(cb, m.r8(kReadyStatus), kReadyResult);
        if (got & kIntrSync)
            if (const u32 cb = m.r32(kSyncCallback)) lib_call(cb, m.r8(kSyncStatus), kSyncResult);
    }
    const u32 reg = m.r32(kRegIndex);
    m.w8(reg, index);
    return reg;
}

u32 Game::cd_name_equal(u32 a, u32 b) {
    cover(0x800A4F00u);
    Frame f(*this, 0x18);
    return libc_strncmp(a, b, 12) == 0 ? 1u : 0u;
}

u32 Game::cd_read_sectors(u32 sectors, u32 lba, u32 dst) {
    cover(0x800A5524u);
    Frame f(*this, 0x28);
    const u32 pos = f.at(0x10);
    hw->cd_int_to_pos(static_cast<s32>(lba), pos);
    hw->cd_control(kCdlSetloc, pos, 0);
    hw->cd_read(sectors, dst, 0x80);
    return hw->cd_read_sync(0, 0) == 0 ? 1u : 0u;
}

u32 Game::cd_newmedia() {
    cover(0x800A4F20u);
    Frame f(*this, 0x40);
    if (cd_read_sectors(1, 16, kSector) != 1) return 0;
    if (libc_strncmp(kSector + 1, kIsoId, 5) != 0) return 0;
    const u32 path_table = f.at(0x18);
    m.copy(path_table, kSector + 0x8C, 4);
    if (cd_read_sectors(1, m.r32(path_table), kSector) != 1) return 0;
    u32 n = 0;
    for (u32 rec = kSector; rec < kSector + kSectorSize && m.r8(rec);) {
        const u32 dir = kDirs + kDirSize * n;
        m.copy(dir + kDirExtent, rec + 2, 4);
        const u32 parent = m.r8(rec + 6);
        m.w32(dir, n + 1);
        m.w32(dir + kDirParent, parent);
        m.copy(dir + kDirName, rec + 8, m.r8(rec));
        m.w8(dir + kDirName + m.r8(rec), 0);
        const u32 length = m.r8(rec);
        rec += length + (length & 1u) + 8;
        if (++n >= kDirMax) break;
    }
    if (n < kDirMax) m.w32(kDirs + kDirSize * n + kDirParent, 0);
    m.w32(kCachedDir, 0);
    return 1;
}

s32 Game::cd_searchdir(u32 parent, u32 name) {
    cover(0x800A51E4u);
    Frame f(*this, 0x28);
    for (u32 i = 0; i < kDirMax; ++i) {
        const u32 dir = kDirs + kDirSize * i;
        const u32 p = m.r32(dir + kDirParent);
        if (!p) return -1;
        if (p == parent && libc_strcmp(name, dir + kDirName) == 0) return static_cast<s32>(i + 1);
    }
    return -1;
}

s32 Game::cd_cachefile(u32 dir) {
    cover(0x800A5288u);
    Frame f(*this, 0x40);
    if (dir == m.r32(kCachedDir)) return 1;
    if (cd_read_sectors(1, m.r32(kDirs + kDirSize * (dir - 1) + kDirExtent), kSector) != 1) return -1;
    const u32 extent = f.at(0x18);
    u32 n = 0;
    for (u32 rec = kSector; rec < kSector + kSectorSize && m.r8(rec);) {
        const u32 file = kFiles + kFileSize * n;
        m.copy(extent, rec + 2, 4);
        hw->cd_int_to_pos(m.r32s(extent), file);
        m.copy(file + kFileBytes, rec + 10, 4);
        if (n == 0) {
            m.w16(kFiles + kFileName, m.r16(kDotName));
        } else if (n == 1) {
            const u32 dots = m.r16(kDotDotName), end = m.r8(kDotDotName + 2);
            m.w16(kFiles + kFileSize + kFileName, dots);
            m.w8(kFiles + kFileSize + kFileName + 2, end);
        } else {
            m.copy(file + kFileName, rec + 0x21, m.r8(rec + 0x20));
            m.w8(file + kFileName + m.r8(rec + 0x20), 0);
        }
        rec += m.r8(rec);
        if (++n >= kFileMax) break;
    }
    m.w32(kCachedDir, dir);
    if (n < kFileMax) m.w8(kFiles + kFileSize * n + kFileName, 0);
    return 1;
}

void Game::cdread_finish(bool from_ready, u32 result) {
    hw->cd_ready_callback(m.r32(kReadSavedReady));
    if (m.r32(kReadMode) & 1u) hw->cd_data_callback(m.r32(kReadSavedData));
    hw->cd_sync_callback(kCdReadSyncDoneFn);
    hw->cd_control_f(kCdlPause, 0);
    const u32 callback = m.r32(kReadCallback);
    m.w32(kReadDone, 1);
    if (!callback) return;
    if (from_ready) lib_call(callback, m.r32(kReadLeft) != 0 ? kCdlDiskError : kCdlComplete, result);
    else lib_call(callback, kCdlComplete, m.r32(kReadResult));
}

u32 Game::cdread_pause_done() {
    cover(0x800A5598u);
    Frame f(*this, 0x18);
    const u32 v = hw->cd_sync_callback(m.r32(kReadSavedSync));
    m.w32(kReadDone, 0);
    return v;
}

void Game::cdread_ready(u32 intr, u32 result) {
    cover(0x800A55CCu);
    Frame f(*this, 0x30);
    const u32 header = f.at(0x10);
    m.w32(kReadResult, result);
    if ((intr & 0xFFu) != kCdlDataReady) {
        m.w32(kReadLeft, 0xFFFFFFFFu);
    } else if (m.r32s(kReadLeft) > 0) {
        if (m.r32(kReadWords) == 0x200u) {
            if (m.r32(kReadMode) & 1u) {
                hw->cd_data_callback(0);
                hw->cd_get_sector(header, 3);
                hw->cd_data_callback(kCdReadDataFn);
            } else {
                hw->cd_get_sector(header, 3);
            }
            if (hw->cd_pos_to_int(header) != m.r32s(kReadLba)) m.w32(kReadLeft, 0xFFFFFFFFu);
        }
        hw->cd_get_sector(m.r32(kReadCursor), m.r32(kReadWords));
        if (!(m.r32(kReadMode) & 1u)) {
            m.w32(kReadCursor, m.r32(kReadCursor) + (m.r32(kReadWords) << 2));
            m.w32(kReadLeft, m.r32(kReadLeft) - 1);
            m.w32(kReadLba, m.r32(kReadLba) + 1);
        }
    }
    m.w32(kReadTick, hw->vsync(-1));
    if (m.r32s(kReadLeft) < 0) cdread_start(1);
    if (m.r32s(kReadStart) + kReadTimeout < static_cast<s32>(hw->vsync(-1))) m.w32(kReadLeft, 0xFFFFFFFFu);
    if (m.r32(kReadLeft) != 0 && !(m.r32s(kReadStart) + kReadTimeout < static_cast<s32>(hw->vsync(-1)))) return;
    cdread_finish(true, result);
}

void Game::cdread_data() {
    cover(0x800A5840u);
    Frame f(*this, 0x18);
    m.w32(kReadCursor, m.r32(kReadCursor) + (m.r32(kReadWords) << 2));
    m.w32(kReadLeft, m.r32(kReadLeft) - 1);
    m.w32(kReadLba, m.r32(kReadLba) + 1);
    if (m.r32(kReadLeft) == 0) cdread_finish(false, 0);
}

s32 Game::cdread_start(u32 retry) {
    cover(0x800A5910u);
    Frame f(*this, 0x28);
    const u32 mode_byte = f.at(0x10);
    hw->cd_sync_callback(0);
    hw->cd_ready_callback(0);
    if (m.r32(kReadMode) & 1u) hw->cd_data_callback(0);
    if (cd_status() & kStatusShellOpen) {
        (void)(hw->vsync(-1) & 0x3Fu);
        hw->cd_control_f(kCdlNop, 0);
        m.w32(kReadStart, hw->vsync(-1));
        m.w32(kReadLeft, 0xFFFFFFFFu);
        return m.r32s(kReadLeft);
    }
    bool ok = true;
    if (retry) {
        hw->cd_control(kCdlPause, 0, 0);
        ok = hw->cd_control(kCdlSetloc, 0, 0) != 0;
    }
    if (ok) {
        const u32 mode = m.r32(kReadModeByte);
        m.w8(mode_byte, mode);
        if ((mode & 0xFFu) != cd_mode() || retry) ok = hw->cd_control(kCdlSetmode, mode_byte, 0) != 0;
    }
    if (!ok) {
        m.w32(kReadLeft, 0xFFFFFFFFu);
        return m.r32s(kReadLeft);
    }
    m.w32(kReadLba, static_cast<u32>(hw->cd_pos_to_int(0)));
    hw->cd_ready_callback(kCdReadReadyFn);
    if (m.r32(kReadMode) & 1u) hw->cd_data_callback(kCdReadDataFn);
    m.w32(kReadCursor, m.r32(kReadBuffer));
    hw->cd_control_f(kCdlReadN, 0);
    m.w32(kReadLeft, m.r32(kReadSectors));
    m.w32(kReadTick, hw->vsync(-1));
    return m.r32s(kReadLeft);
}
}
