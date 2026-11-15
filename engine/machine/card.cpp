#include "machine/card.hpp"

#include "core/log.hpp"
#include "sim/memory.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>

namespace ac3::machine {
namespace {
constexpr u32 kBlockSize = 8192;
constexpr int kDataBlocks = 15;

constexpr u32 kEvHwCard = 0xF0000011u, kEvSwCard = 0xF4000001u;
constexpr u32 kSpIOE = 0x0004u, kSpNew = 0x2000u, kSpError = 0x8000u;

constexpr u32 kDeState = 0x00, kDeSize = 0x04, kDeLink = 0x08, kDeName = 0x0A, kDeNameMax = 20;
constexpr u32 kLinkNone = 0xFFFFu;
constexpr u32 kStFree = 0xA0u, kStFirst = 0x51u, kStMiddle = 0x52u, kStLast = 0x53u;

constexpr u32 kOCreate = 0x0200u, kONoWait = 0x8000u;

constexpr u32 kDirName = 0x00, kDirSize = 0x18, kDirHead = 0x20;

u32 rd32(const u8* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<u32>(p[3]) << 24); }
u32 rd16(const u8* p) { return p[0] | (p[1] << 8); }
void wr32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
    p[2] = static_cast<u8>(v >> 16);
    p[3] = static_cast<u8>(v >> 24);
}
void wr16(u8* p, u32 v) {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
}

void frame_seal(u8* f) {
    u8 x = 0;
    for (u32 i = 0; i < Card::kFrameSize - 1; ++i) x ^= f[i];
    f[Card::kFrameSize - 1] = x;
}

bool name_eq(const u8* stored, const std::string& want) {
    for (u32 i = 0; i < kDeNameMax; ++i) {
        const char a = static_cast<char>(stored[i]), b = i < want.size() ? want[i] : '\0';
        if (a != b) return false;
        if (!a) return true;
    }
    return want.size() <= kDeNameMax;
}

bool name_match(const std::string& pat, const u8* stored) {
    char nm[kDeNameMax + 1];
    std::memcpy(nm, stored, kDeNameMax);
    nm[kDeNameMax] = 0;
    const char* n = nm;
    if (pat.empty()) return true;
    for (const char c : pat) {
        if (c == '*') return true;
        if (c == '?') {
            if (!*n) return false;
            ++n;
            continue;
        }
        if (c != *n) return false;
        ++n;
    }
    return *n == 0;
}

int chan_slot(u32 chan) { return (chan & 0x10u) ? 1 : 0; }
}

Card::Card(sim::Memory& mem) : m(mem) {
    for (auto& d : data_) d.assign(kSize, 0);
}

void Card::set_path(int slot, const std::string& path, bool persist) {
    if (slot < 0 || slot > 1) return;
    path_[slot] = path;
    persist_[slot] = persist;
    loaded_[slot] = false;
    flush_failed_[slot] = false;
}

void Card::ensure(int slot) {
    if (loaded_[slot]) return;
    loaded_[slot] = true;
    unconfirmed_[slot] = true;
    std::fill(data_[slot].begin(), data_[slot].end(), u8{0});
    bool had = false;
    if (std::FILE* f = std::fopen(path_[slot].c_str(), "rb")) {
        const size_t got = std::fread(data_[slot].data(), 1, kSize, f);
        std::fclose(f);
        had = true;
        log::info("memory card {}: loaded {} bytes from {}", slot + 1, got, path_[slot]);
    }
    if (!formatted(slot)) {
        format_image(slot);
        log::info("memory card {}: {} at {}", slot + 1, had ? "unformatted, formatting" : "new card, formatted empty",
                  path_[slot]);
        flush(slot);
    }
}

void Card::flush(int slot) {
    if (!persist_[slot]) return;
    std::error_code ec;
    const std::filesystem::path p(path_[slot]);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    bool bad = true;
    if (std::FILE* f = std::fopen(path_[slot].c_str(), "wb")) {
        const size_t put = std::fwrite(data_[slot].data(), 1, kSize, f);
        bad = std::ferror(f) != 0 || put != kSize;
        if (std::fclose(f) != 0) bad = true;
    }
    if (!bad) {
        flush_failed_[slot] = false;
        return;
    }
    if (flush_failed_[slot]) return;
    flush_failed_[slot] = true;
    log::warn("memory card {}: cannot write {} -- saves will NOT persist", slot + 1, path_[slot]);
}

bool Card::formatted(int slot) const { return data_[slot][0] == 'M' && data_[slot][1] == 'C'; }

void Card::format_image(int slot) {
    std::fill(data_[slot].begin(), data_[slot].end(), u8{0});
    u8* hdr = frame_at(slot, 0);
    hdr[0] = 'M';
    hdr[1] = 'C';
    frame_seal(hdr);
    for (int b = 1; b <= kDataBlocks; ++b) {
        u8* e = frame_at(slot, b);
        wr32(e + kDeState, kStFree);
        wr16(e + kDeLink, kLinkNone);
        frame_seal(e);
    }
    for (int fr = 16; fr < 36; ++fr) {
        u8* e = frame_at(slot, fr);
        wr32(e, 0xFFFFFFFFu);
        wr16(e + 8, kLinkNone);
        frame_seal(e);
    }
    std::memcpy(frame_at(slot, 63), hdr, kFrameSize);
}

void Card::done(u32 chan, u32 cls, u32 spec) {
    last_chan_ = chan;
    if (deliver_event) deliver_event(cls, spec);
}

void Card::InitCARD() { ensure(0); }
u32 Card::StartCARD() {
    started_ = true;
    ensure(0);
    return 1;
}
u32 Card::StopCARD() {
    started_ = false;
    return 1;
}

u32 Card::card_info(u32 chan) {
    const int slot = chan_slot(chan);
    ensure(slot);
    done(chan, kEvSwCard, unconfirmed_[slot] ? kSpNew : kSpIOE);
    return 1;
}

u32 Card::card_load(u32 chan) {
    const int slot = chan_slot(chan);
    ensure(slot);
    done(chan, kEvSwCard, formatted(slot) ? kSpIOE : kSpNew);
    return 1;
}

u32 Card::card_status(u32) { return 0x01; }
u32 Card::card_wait(u32) { return 1; }

u32 Card::card_read(u32 chan, u32 frame, u32 dst) {
    const int slot = chan_slot(chan);
    ensure(slot);
    if (frame >= kFrames) return 0;
    const u8* src = frame_at(slot, static_cast<int>(frame));
    for (u32 i = 0; i < kFrameSize; ++i) m.w8(dst + i, src[i]);
    done(chan, kEvHwCard, kSpIOE);
    return 1;
}

u32 Card::card_write(u32 chan, u32 frame, u32 src) {
    const int slot = chan_slot(chan);
    ensure(slot);
    if (frame >= kFrames) return 0;
    u8* dst = frame_at(slot, static_cast<int>(frame));
    for (u32 i = 0; i < kFrameSize; ++i) dst[i] = static_cast<u8>(m.r8(src + i));
    unconfirmed_[slot] = false;
    flush(slot);
    done(chan, kEvHwCard, kSpIOE);
    return 1;
}

u32 Card::card_clear(u32 chan) {
    const int slot = chan_slot(chan);
    ensure(slot);
    unconfirmed_[slot] = false;
    std::memcpy(frame_at(slot, 63), frame_at(slot, 0), kFrameSize);
    flush(slot);
    done(chan, kEvHwCard, kSpIOE);
    return 1;
}

u32 Card::card_format(u32 chan) {
    const int slot = chan_slot(chan);
    ensure(slot);
    format_image(slot);
    unconfirmed_[slot] = false;
    flush(slot);
    return 1;
}

u32 Card::card_auto(u32 on) {
    const u32 prev = auto_;
    auto_ = on;
    return prev;
}

int Card::dir_find(int slot, const std::string& name) const {
    for (int b = 1; b <= kDataBlocks; ++b) {
        const u8* e = frame_at(slot, b);
        if ((rd32(e + kDeState) & 0xFFu) != kStFirst) continue;
        if (name_eq(e + kDeName, name)) return b;
    }
    return 0;
}

int Card::dir_create(int slot, const std::string& name, int blocks) {
    int chain[kDataBlocks], n = 0;
    for (int b = 1; b <= kDataBlocks && n < blocks; ++b)
        if ((rd32(frame_at(slot, b) + kDeState) & 0xFFu) == kStFree) chain[n++] = b;
    if (n < blocks) return 0;
    for (int i = 0; i < blocks; ++i) {
        u8* e = frame_at(slot, chain[i]);
        std::memset(e, 0, kFrameSize);
        wr32(e + kDeState, i == 0 ? kStFirst : (i == blocks - 1 ? kStLast : kStMiddle));
        wr32(e + kDeSize, i == 0 ? static_cast<u32>(blocks) * kBlockSize : 0u);
        wr16(e + kDeLink, i + 1 < blocks ? static_cast<u32>(chain[i + 1] - 1) : kLinkNone);
        if (i == 0) std::memcpy(e + kDeName, name.data(), std::min<size_t>(name.size(), kDeNameMax));
        frame_seal(e);
        std::memset(data_[slot].data() + chain[i] * kBlockSize, 0, kBlockSize);
    }
    return chain[0];
}

int Card::block_step(int slot, int first, u32 index) const {
    int b = first;
    while (index-- && b) {
        const u32 link = rd16(frame_at(slot, b) + kDeLink);
        b = (link == kLinkNone || link >= static_cast<u32>(kDataBlocks)) ? 0 : static_cast<int>(link) + 1;
    }
    return b;
}

bool Card::path_split(u32 addr, int* slot, std::string* name) const {
    std::string raw;
    for (u32 n = 0; n < 63; ++n) {
        const u32 c = m.r8(addr + n);
        if (!c) break;
        raw.push_back(static_cast<char>(c));
    }
    *slot = 0;
    const size_t colon = raw.find(':');
    size_t from = 0;
    if (colon != std::string::npos) {
        if (colon >= 3 && raw[2] == '1') *slot = 1;
        from = colon + 1;
    }
    *name = raw.substr(from);
    return !raw.empty();
}

Card::Fd* Card::fd_get(u32 fd) {
    if (fd >= 8 || !fds_[fd].used) return nullptr;
    return &fds_[fd];
}

void Card::file_done(const Fd& f, bool ok) {
    if (!f.async) return;
    done(f.slot ? 0x10u : 0x00u, kEvSwCard, ok ? kSpIOE : kSpError);
}

u32 Card::open(u32 path, u32 flags) {
    std::string name;
    int slot;
    if (!path_split(path, &slot, &name) || name.empty()) return 0xFFFFFFFFu;
    ensure(slot);
    int first = dir_find(slot, name);
    if (!first && (flags & kOCreate)) {
        int blocks = static_cast<int>((flags >> 16) & 0xFFFFu);
        if (blocks < 1) blocks = 1;
        if (blocks > kDataBlocks) blocks = kDataBlocks;
        first = dir_create(slot, name, blocks);
        if (!first) return 0xFFFFFFFFu;
        flush(slot);
    }
    if (!first) return 0xFFFFFFFFu;
    for (u32 i = 0; i < 8; ++i) {
        if (fds_[i].used) continue;
        fds_[i].used = true;
        fds_[i].slot = slot;
        fds_[i].first = first;
        fds_[i].pos = 0;
        fds_[i].size = rd32(frame_at(slot, first) + kDeSize);
        fds_[i].async = (flags & kONoWait) != 0;
        return i;
    }
    return 0xFFFFFFFFu;
}

u32 Card::close(u32 fd) {
    Fd* f = fd_get(fd);
    if (!f) return 0xFFFFFFFFu;
    f->used = false;
    return fd;
}

u32 Card::lseek(u32 fd, u32 off, u32 whence) {
    Fd* f = fd_get(fd);
    if (!f) return 0xFFFFFFFFu;
    u32 pos;
    switch (whence) {
    case 0: pos = off; break;
    case 1: pos = f->pos + off; break;
    case 2: pos = f->size + off; break;
    default: return 0xFFFFFFFFu;
    }
    if (pos > f->size) return 0xFFFFFFFFu;
    f->pos = pos;
    return pos;
}

u32 Card::file_io(Fd& f, u32 guest, u32 len, bool writing) {
    u32 got = 0;
    if (f.pos >= f.size) return 0;
    if (len > f.size - f.pos) len = f.size - f.pos;
    while (got < len) {
        const int block = block_step(f.slot, f.first, f.pos / kBlockSize);
        if (!block) break;
        const u32 in_block = f.pos % kBlockSize;
        u32 chunk = kBlockSize - in_block;
        if (chunk > len - got) chunk = len - got;
        u8* card = data_[f.slot].data() + block * kBlockSize + in_block;
        for (u32 i = 0; i < chunk; ++i) {
            if (writing) card[i] = static_cast<u8>(m.r8(guest + got + i));
            else m.w8(guest + got + i, card[i]);
        }
        f.pos += chunk;
        got += chunk;
    }
    return got;
}

u32 Card::read(u32 fd, u32 dst, u32 n) {
    Fd* f = fd_get(fd);
    if (!f) return 0xFFFFFFFFu;
    const u32 got = file_io(*f, dst, n, false);
    file_done(*f, got == n);
    return got;
}

u32 Card::write(u32 fd, u32 src, u32 n) {
    Fd* f = fd_get(fd);
    if (!f) return 0xFFFFFFFFu;
    const u32 put = file_io(*f, src, n, true);
    flush(f->slot);
    file_done(*f, put == n);
    return put;
}

int Card::scan_next(int slot, const std::string& pat, int from) const {
    for (int b = from; b <= kDataBlocks; ++b) {
        const u8* e = frame_at(slot, b);
        if ((rd32(e + kDeState) & 0xFFu) != kStFirst) continue;
        if (name_match(pat, e + kDeName)) return b;
    }
    return 0;
}

void Card::fill_direntry(u32 out, int slot, int block) {
    const u8* e = frame_at(slot, block);
    for (u32 i = 0; i < 0x28; ++i) m.w8(out + i, 0);
    for (u32 i = 0; i < kDeNameMax; ++i) m.w8(out + kDirName + i, e[kDeName + i]);
    m.w32(out + kDirSize, rd32(e + kDeSize));
    m.w32(out + kDirHead, static_cast<u32>(block));
}

u32 Card::firstfile(u32 path, u32 out) {
    int slot;
    if (!path_split(path, &slot, &scan_pattern_)) return 0;
    ensure(slot);
    scan_slot_ = slot;
    const int b = scan_next(slot, scan_pattern_, 1);
    if (!b) {
        scan_block_ = 0;
        return 0;
    }
    scan_block_ = b + 1;
    fill_direntry(out, slot, b);
    return out;
}

u32 Card::nextfile(u32 out) {
    if (!scan_block_) return 0;
    const int b = scan_next(scan_slot_, scan_pattern_, scan_block_);
    if (!b) {
        scan_block_ = 0;
        return 0;
    }
    scan_block_ = b + 1;
    fill_direntry(out, scan_slot_, b);
    return out;
}
}
