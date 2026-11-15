#include "machine/cd.hpp"

#include "core/log.hpp"
#include "machine/spu.hpp"
#include "machine/state_reader.hpp"
#include "sim/memory.hpp"
#include "vfs/iso9660.hpp"
#include "vfs/translation.hpp"

#include <cstring>

namespace ac3::machine {
namespace {
constexpr u32 kUser = 2048;
constexpr u32 kModeSize0 = 0x10, kModeSize1 = 0x20, kModeRT = 0x40, kModeSpeed = 0x80;
enum : u32 {
    C_Nop = 0x01, C_Setloc = 0x02, C_Play = 0x03, C_Forward = 0x04, C_Backward = 0x05, C_ReadN = 0x06,
    C_Standby = 0x07, C_Stop = 0x08, C_Pause = 0x09, C_Mute = 0x0B, C_Demute = 0x0C, C_Setfilter = 0x0D,
    C_Setmode = 0x0E, C_Getparam = 0x0F, C_GetlocL = 0x10, C_GetlocP = 0x11, C_GetTN = 0x13, C_GetTD = 0x14,
    C_SeekL = 0x15, C_SeekP = 0x16, C_ReadS = 0x1B,
};
constexpr u32 kEvClassCd = 0xF0000003u, kEvSpecComplete = 0x0020u;
constexpr u32 kCdlDataReady = 1, kCdlComplete = 2;
constexpr u8 kStatError = 0x01, kStatMotor = 0x02, kStatShell = 0x10, kStatReading = 0x20;
constexpr u32 kSubFile = 4, kSubChan = 5, kSubMode = 6, kSubCoding = 7;
constexpr u8 kSubmodeAudio = 0x04, kSubmodeData = 0x08;
constexpr int kFramesPerSec = 60;
constexpr u32 kIndexId = 0x45334341u, kSphCode = 0x19990427u, kDiscEither = 3u, kOnThisDisc = 0x80000000u;
constexpr u32 kSphHeader = 20u, kSphMax = 4u * 2048u;
constexpr u32 kBphCode = 0x00000427u, kKcdState = 0x800BCBA8u, kKcdPackTable = 24u;

u8 to_bcd(u32 v) { return static_cast<u8>(((v / 10) << 4) | (v % 10)); }
u32 from_bcd(u8 v) { return static_cast<u32>((v >> 4) * 10 + (v & 15)); }
void lba_to_msf(u32 lba, u8* m, u8* s, u8* f) {
    const u32 t = lba + 150;
    *m = to_bcd(t / (75 * 60));
    *s = to_bcd((t / 75) % 60);
    *f = to_bcd(t % 75);
}
s32 msf_to_lba_signed(u8 m, u8 s, u8 f) {
    return static_cast<s32>((from_bcd(m) * 60 + from_bcd(s)) * 75 + from_bcd(f)) - 150;
}
u32 msf_to_lba(u8 m, u8 s, u8 f) {
    const s32 t = msf_to_lba_signed(m, s, f);
    return t > 0 ? static_cast<u32>(t) : 0u;
}
u32 rd32(const u8* p) { return static_cast<u32>(p[0]) | p[1] << 8 | p[2] << 16 | static_cast<u32>(p[3]) << 24; }
void wr32(u8* p, u32 v) {
    p[0] = static_cast<u8>(v);
    p[1] = static_cast<u8>(v >> 8);
    p[2] = static_cast<u8>(v >> 16);
    p[3] = static_cast<u8>(v >> 24);
}
s32 clamp16(s32 v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v); }
}

namespace {
constexpr int kXaGroups = 18;
constexpr int kXaMaxSamples = 4032;
const int kXaF0[4] = {0, 60, 115, 98};
const int kXaF1[4] = {0, 0, -52, -55};
}

void Xa::reset() {
    std::memset(hist_, 0, sizeof hist_);
    res_pos_ = 0;
    res_last_[0] = res_last_[1] = 0;
    res_have_last_ = 0;
    res_rate_ = 0;
    filter_active_ = 0;
    filter_file_ = filter_chan_ = 0;
    rt_enabled_ = 0;
    muted_ = 0;
    spu_.cd_flush();
}

void Xa::set_filter(int active, u8 file, u8 chan) {
    if (active && (!filter_active_ || file != filter_file_ || chan != filter_chan_)) {
        std::memset(hist_, 0, sizeof hist_);
        res_have_last_ = 0;
        res_pos_ = 0;
    }
    filter_active_ = active;
    filter_file_ = file;
    filter_chan_ = chan;
}

void Xa::set_enabled(int on) {
    if (rt_enabled_ && !on) stop();
    rt_enabled_ = on;
}

void Xa::stop() {
    std::memset(hist_, 0, sizeof hist_);
    res_have_last_ = 0;
    res_pos_ = 0;
    spu_.cd_flush();
}

void Xa::resample_push(const s16* l, const s16* r, int n, u32 src_rate) {
    if (src_rate != res_rate_) {
        res_rate_ = src_rate;
        res_pos_ = 0;
        res_have_last_ = 0;
    }
    constexpr s32 kRate = Spu::kSampleRate;
    s16 out[512 * 2];
    int fill = 0;
    for (int i = 0; i < n; i++) {
        const s16 cl = l[i], cr = r[i];
        if (!res_have_last_) {
            res_last_[0] = cl;
            res_last_[1] = cr;
            res_have_last_ = 1;
        }
        while (res_pos_ < kRate) {
            const s32 num = res_pos_;
            s32 a = (static_cast<s32>(res_last_[0]) * (kRate - num) + static_cast<s32>(cl) * num) / kRate;
            s32 b = (static_cast<s32>(res_last_[1]) * (kRate - num) + static_cast<s32>(cr) * num) / kRate;
            if (muted_) a = b = 0;
            out[fill * 2 + 0] = static_cast<s16>(clamp16(a));
            out[fill * 2 + 1] = static_cast<s16>(clamp16(b));
            if (++fill == 512) {
                spu_.cd_push(out, fill);
                fill = 0;
            }
            res_pos_ += static_cast<s32>(src_rate);
        }
        res_pos_ -= kRate;
        res_last_[0] = cl;
        res_last_[1] = cr;
    }
    if (fill) spu_.cd_push(out, fill);
}

int Xa::sector(const u8* s) {
    if (!s) return 0;
    const u8 submode = s[kSubMode];
    if (!(submode & kSubmodeAudio) || (submode & kSubmodeData)) return 0;
    if (!rt_enabled_) return 1;
    if (filter_active_ && (s[kSubFile] != filter_file_ || s[kSubChan] != filter_chan_)) return 1;
    const u8 coding = s[kSubCoding];
    const bool stereo = (coding & 0x03) == 1;
    const bool eight = ((coding >> 4) & 0x03) == 1;
    const u32 rate = (((coding >> 2) & 0x03) == 1) ? 18900u : 37800u;
    const u8* audio = s + 12;
    for (auto& p : pcm_) p.assign(kXaMaxSamples, 0);
    int n[2] = {0, 0};
    for (int g = 0; g < kXaGroups; g++) {
        const u8* par = audio + g * 128;
        const u8* dat = par + 16;
        const int units = eight ? 4 : 8;
        for (int u = 0; u < units; u++) {
            const u8 p = par[4 + u];
            const int shift = p & 0x0F;
            const int filt = (p >> 4) & 0x03;
            const int ch = stereo ? (u & 1) : 0;
            s32* h = hist_[ch];
            for (int k = 0; k < 28; k++) {
                s32 t;
                if (!eight) {
                    const int byte = dat[k * 4 + (u >> 1)];
                    const int nib = (u & 1) ? ((byte >> 4) & 0x0F) : (byte & 0x0F);
                    t = static_cast<s32>(static_cast<s16>(static_cast<u16>(nib << 12)));
                } else {
                    t = static_cast<s32>(static_cast<s16>(static_cast<u16>(static_cast<u32>(dat[k * 4 + u]) << 8)));
                }
                s32 v = (t >> shift) + ((kXaF0[filt] * h[0] + kXaF1[filt] * h[1] + 32) >> 6);
                v = clamp16(v);
                h[1] = h[0];
                h[0] = v;
                if (n[ch] < kXaMaxSamples) pcm_[ch][static_cast<size_t>(n[ch]++)] = static_cast<s16>(v);
            }
        }
    }
    if (stereo) resample_push(pcm_[0].data(), pcm_[1].data(), n[0] < n[1] ? n[0] : n[1], rate);
    else resample_push(pcm_[0].data(), pcm_[0].data(), n[0], rate);
    return 1;
}

Cd::Cd(sim::Memory& mem, Spu& spu) : m(mem), xa_(spu) {}

bool Cd::image_read(Image& d, u32 rel, u8* out) {
    if (!d.open() || rel >= d.sectors) return false;
    u8 raw[vfs::kRawSector];
    if (!d.disc.read_raw(rel, raw)) return false;
    std::memcpy(out, raw + 12, kWhole);
    return true;
}

Cd::Image* Cd::image_for(u32 lba) {
    for (int i = 1; i >= 0; i--) {
        Image& d = discs_[i];
        if (d.open() && lba >= d.base && lba - d.base < d.sectors) return &d;
    }
    return nullptr;
}

bool Cd::read_sector_whole(u32 lba, u8* out) {
    Image* d = image_for(lba);
    if (!d || !image_read(*d, lba - d->base, out)) return false;
    if (d->base) lba_to_msf(lba, &out[0], &out[1], &out[2]);
    if (merged_index_size_ && lba >= merged_index_lba_ && lba - merged_index_lba_ < kSphMax / kUser) {
        const u32 off = (lba - merged_index_lba_) * kUser;
        if (off < merged_index_size_) {
            const u32 n = merged_index_size_ - off;
            std::memcpy(out + 12, merged_index_.data() + off, n < kUser ? n : kUser);
        }
    }
    if (pack_index_size_ && lba >= pack_index_lba_ && lba - pack_index_lba_ < kSphMax / kUser) {
        const u32 off = (lba - pack_index_lba_) * kUser;
        if (off < pack_index_size_) {
            const u32 n = pack_index_size_ - off;
            std::memcpy(out + 12, pack_merged_.data() + off, n < kUser ? n : kUser);
        }
    }
    return true;
}

u32 Cd::delivery_offset() const {
    if (cd_mode_ & kModeSize1) return 0;
    if (cd_mode_ & kModeSize0) return 4;
    return 12;
}

int Cd::fifo_step() {
    if (!read_sector_whole(cur_lba_, fifo_)) return -1;
    fifo_lba_ = cur_lba_;
    cur_lba_++;
    if (!(fifo_[kSubMode] & kSubmodeData)) {
        xa_.sector(fifo_);
        return 0;
    }
    fifo_valid_ = 1;
    fifo_pos_ = delivery_offset();
    return 1;
}

int Cd::fifo_fill() {
    for (int tries = 0; tries < 4096; tries++) {
        const int r = fifo_step();
        if (r < 0) return 0;
        if (r > 0) return 1;
    }
    return 0;
}

u32 Cd::drive_sectors() const {
    if (discs_[1].open()) return discs_[1].base + discs_[1].sectors;
    return discs_[0].sectors;
}

int Cd::read_index(vfs::DiscImage& d, const char* index, const char* pack, u32 code, std::vector<u8>& buf, u32* lba,
                   u32* size, u32* pack_lba) {
    const auto sph = vfs::iso_find(d, index);
    const auto spb = vfs::iso_find(d, pack);
    if (!sph || !spb) return -1;
    *lba = sph->lba;
    *size = sph->size;
    *pack_lba = spb->lba;
    if (*size < kSphHeader || *size > kSphMax) return -1;
    buf.assign(kSphMax, 0);
    for (u32 s = 0; s * kUser < *size; s++)
        if (!d.read_user(*lba + s, buf.data() + s * kUser)) return -1;
    if (rd32(buf.data()) != kIndexId || rd32(buf.data() + 8) != code) return -1;
    const u32 count = rd32(buf.data() + 16);
    if (kSphHeader + 8u * count > *size) return -1;
    return static_cast<int>(count);
}

void Cd::pack_serve() {
    const u32 count = rd32(pack_index_[0].data() + 16);
    pack_merged_ = pack_index_[0];
    for (u32 k = 0; k < count; k++) {
        const int from = pack_kind_[k] == kPackOnlyOther ? 1 : pack_kind_[k] == kPackBoth ? pack_disc_ : 0;
        if (from) std::memcpy(pack_merged_.data() + kSphHeader + 8u * k, pack_index_[1].data() + kSphHeader + 8u * k, 8);
    }
}

void Cd::split_packs() {
    pack_index_size_ = 0;
    pack_disc_ = 0;
    if (!english(0) && !english(1)) return;
    vfs::DiscImage plain[2];
    if (!plain[0].open(discs_[0].disc.path()) || !plain[1].open(discs_[1].disc.path())) return;
    std::vector<u8> original[2];
    u32 lba, size, o_lba, o_size, o_pack, j_lba, j_size, j_pack[2] = {0, 0};
    const int count = read_index(discs_[0].disc, "ACE.BPH", "ACE.BPB", kBphCode, pack_index_[0], &lba, &size, &pack_lba_);
    const int o_count = read_index(discs_[1].disc, "ACE.BPH", "ACE.BPB", kBphCode, pack_index_[1], &o_lba, &o_size, &o_pack);
    bool same = count >= 0 && o_count == count && o_size == size;
    for (int i = 0; i < 2 && same; i++)
        same = read_index(plain[i], "ACE.BPH", "ACE.BPB", kBphCode, original[i], &j_lba, &j_size, &j_pack[i]) == count;
    if (!same) {
        log::warn("english: the discs' ACE.BPH do not match; ACE.BPB is read from disc {} only", discs_[0].number);
        return;
    }
    auto sectors_of = [](const u8* entry) { return ((rd32(entry) & 0x00FFFFFFu) * 4u + kUser - 1) / kUser; };
    auto record_same = [](vfs::DiscImage& a, u32 ra, vfs::DiscImage& b, u32 rb, u32 n, bool over) {
        u8 x[kUser], y[kUser];
        for (u32 s = 0; s < n; s++) {
            u32 from;
            if (over && a.translation() && !a.translation()->sector(ra + s, from) && from == rb + s) continue;
            if (!a.read_user(ra + s, x) || !b.read_user(rb + s, y) || std::memcmp(x, y, kUser) != 0) return false;
        }
        return true;
    };
    pack_kind_.assign(static_cast<size_t>(count), kPackSame);
    u32 n[4] = {0, 0, 0, 0};
    for (u32 k = 0; k < static_cast<u32>(count); k++) {
        const u8* e = pack_index_[0].data() + kSphHeader + 8u * k;
        const u8* o = pack_index_[1].data() + kSphHeader + 8u * k;
        const u8* j = original[0].data() + kSphHeader + 8u * k;
        const u8* p = original[1].data() + kSphHeader + 8u * k;
        bool here = english(0) && (rd32(e) != rd32(j) || !record_same(discs_[0].disc, pack_lba_ + rd32(e + 4), plain[0],
                                                                      j_pack[0] + rd32(j + 4), sectors_of(j), true));
        bool there = english(1) && (rd32(o) != rd32(p) || !record_same(discs_[1].disc, o_pack + rd32(o + 4), plain[1],
                                                                       j_pack[1] + rd32(p + 4), sectors_of(p), true));
        if (here && there && rd32(e) == rd32(o) &&
            record_same(discs_[0].disc, pack_lba_ + rd32(e + 4), discs_[1].disc, o_pack + rd32(o + 4), sectors_of(e), false))
            here = there = false;
        pack_kind_[k] = here && there ? kPackBoth : here ? kPackOnlyBooted : there ? kPackOnlyOther : kPackSame;
        n[pack_kind_[k]]++;
    }
    for (u32 k = 0; k < static_cast<u32>(count); k++) {
        u8* o = pack_index_[1].data() + kSphHeader + 8u * k + 4;
        wr32(o, discs_[1].base + o_pack + rd32(o) - pack_lba_);
    }
    pack_index_lba_ = lba;
    pack_index_size_ = size;
    pack_serve();
    log::info("english: ACE.BPB holds {} records translated on disc {} only, {} on disc {} only, {} that follow the disc "
              "being played",
              n[kPackOnlyBooted], discs_[0].number, n[kPackOnlyOther], discs_[1].number, n[kPackBoth]);
}

bool Cd::pack_follow(int number) {
    if (!pack_index_size_) return false;
    const int want = discs_[1].number == number ? 1 : discs_[0].number == number ? 0 : -1;
    if (want < 0) return false;
    if (want == pack_disc_) return true;
    const u32 count = rd32(pack_merged_.data() + 16);
    const u32 table = m.r32(m.r32(kKcdState) + kKcdPackTable);
    const u8* e = pack_merged_.data() + kSphHeader;
    bool known = m.ptr(table, 8u * count) != nullptr;
    for (u32 k = 0; known && k < count; k++)
        known = m.r32(table + 8u * k) == rd32(e + 8u * k) && m.r32(table + 8u * k + 4) == pack_lba_ + rd32(e + 8u * k + 4);
    if (!known) {
        static bool warned = false;
        if (!warned)
            log::warn("english: the game's copy of ACE.BPH is not where it was expected; the records that differ by "
                      "disc stay on disc {}",
                      discs_[pack_disc_].number);
        warned = true;
        return false;
    }
    pack_disc_ = want;
    pack_serve();
    e = pack_merged_.data() + kSphHeader;
    for (u32 k = 0; k < count; k++) {
        if (pack_kind_[k] != kPackBoth) continue;
        m.w32(table + 8u * k, rd32(e + 8u * k));
        m.w32(table + 8u * k + 4, pack_lba_ + rd32(e + 8u * k + 4));
    }
    return true;
}

void Cd::pack_note_read(u32 lba) {
    if (!pack_index_size_) return;
    const u32 count = rd32(pack_merged_.data() + 16);
    for (u32 k = 0; k < count; k++) {
        const u8* e = pack_merged_.data() + kSphHeader + 8u * k;
        const u32 sectors = ((rd32(e) & 0x00FFFFFFu) * 4u + kUser - 1) / kUser;
        if (lba - (pack_lba_ + rd32(e + 4)) >= sectors) continue;
        if (pack_kind_[k] == kPackOnlyBooted) pack_follow(discs_[0].number);
        if (pack_kind_[k] == kPackOnlyOther) pack_follow(discs_[1].number);
        return;
    }
}

bool Cd::read_file(int which, const std::string& name, std::vector<u8>& out) {
    Image& d = discs_[which & 1];
    if (!d.open()) return false;
    const auto e = vfs::iso_find(d.disc, name);
    return e && d.disc.read_bytes(e->lba, e->size, out);
}

bool Cd::merge_indices() {
    std::vector<u8> other;
    u32 lba, size, pack, o_lba, o_size, o_pack;
    const int count = read_index(discs_[0].disc, "ACE.SPH", "ACE.SPB", kSphCode, merged_index_, &lba, &size, &pack);
    const int o_count = read_index(discs_[1].disc, "ACE.SPH", "ACE.SPB", kSphCode, other, &o_lba, &o_size, &o_pack);
    if (count < 0 || o_count != count) return false;
    const u32 number = rd32(merged_index_.data() + 4), o_number = rd32(other.data() + 4);
    if (number - 1u > 1u || o_number - 1u > 1u || number == o_number) return false;
    for (u32 k = 0; k < static_cast<u32>(count); k++) {
        u8* e = merged_index_.data() + kSphHeader + 8u * k;
        const u8* o = other.data() + kSphHeader + 8u * k;
        if (rd32(e) & kOnThisDisc) continue;
        if (!(rd32(o) & kOnThisDisc)) continue;
        wr32(e, rd32(o));
        wr32(e + 4, discs_[1].base + o_pack + rd32(o + 4) - pack);
    }
    wr32(merged_index_.data() + 4, kDiscEither);
    discs_[0].number = static_cast<int>(number);
    discs_[1].number = static_cast<int>(o_number);
    merged_index_lba_ = lba;
    merged_index_size_ = size;
    return true;
}

bool Cd::mount(const std::string& booted, const std::string& other, std::string* error) {
    for (Image& d : discs_) d = Image{};
    merged_index_size_ = 0;
    pack_index_size_ = 0;
    pack_disc_ = 0;
    auto translate = [&](vfs::DiscImage& d) {
        if (translation_dirs.empty()) return;
        std::string note;
        if (vfs::translate(d, translation_dirs, note)) log::info("english: {}", note);
        else log::warn("english: {}", note);
    };
    if (!discs_[0].disc.open(booted)) {
        if (error) *error = "cannot open disc image " + booted;
        return false;
    }
    translate(discs_[0].disc);
    discs_[0].sectors = discs_[0].disc.sectors();
    discs_[0].number = 1;
    if (!other.empty() && discs_[1].disc.open(other)) {
        translate(discs_[1].disc);
        discs_[1].sectors = discs_[1].disc.sectors();
        discs_[1].base = discs_[0].sectors;
        if (!merge_indices()) {
            discs_[1] = Image{};
            merged_index_size_ = 0;
        } else {
            split_packs();
        }
    }
    return true;
}

u8 Cd::drive_status() const {
    u8 s = discs_[0].open() ? kStatMotor : static_cast<u8>(kStatShell | kStatError);
    if (discs_[0].open() && streaming_) s |= kStatReading;
    return s;
}

void Cd::write_status(u32 result) {
    if (!result) return;
    for (u32 k = 0; k < 8; ++k) m.w8(result + k, 0);
    m.w8(result, drive_status());
}

void Cd::invoke_cb(u32 fn, u32 intr, u32 result) {
    if (fn && call_guest) call_guest(fn, intr, result);
}

u32 Cd::CdInit() {
    cur_lba_ = 0;
    cd_mode_ = 0;
    return discs_[0].open() ? 1 : 0;
}

u32 Cd::CdSyncCallback(u32 fn) {
    const u32 prev = sync_callback_;
    sync_callback_ = fn;
    return prev;
}
u32 Cd::CdReadyCallback(u32 fn) {
    const u32 prev = ready_callback_;
    ready_callback_ = fn;
    return prev;
}
u32 Cd::CdDataCallback(u32 fn) {
    const u32 prev = data_callback_;
    data_callback_ = fn;
    return prev;
}

u32 Cd::CdSync(u32, u32 result) {
    write_status(result);
    return kCdlComplete;
}

u32 Cd::CD_ready(u32 result) {
    write_status(result);
    return streaming_ ? 1u : 0u;
}

u32 Cd::CdReady(u32, u32 result) {
    if (streaming_ && !fifo_valid_) fifo_fill();
    write_status(result);
    return (streaming_ && fifo_valid_) ? 1u : 0u;
}

s32 Cd::CdPosToInt(u32 loc) { return msf_to_lba_signed(m.r8(loc), m.r8(loc + 1), m.r8(loc + 2)); }

u32 Cd::CdIntToPos(s32 lba, u32 loc) {
    u8 mm, ss, ff;
    lba_to_msf(static_cast<u32>(lba), &mm, &ss, &ff);
    m.w8(loc, mm);
    m.w8(loc + 1, ss);
    m.w8(loc + 2, ff);
    m.w8(loc + 3, 0);
    return loc;
}

u32 Cd::CdSearchFile(u32 fp, u32 name_addr) {
    std::string name;
    for (u32 n = 0; n < 63; ++n) {
        const u8 c = static_cast<u8>(m.r8(name_addr + n));
        if (!c) break;
        name.push_back(static_cast<char>(c));
    }
    size_t base = 0;
    for (size_t i = 0; i < name.size(); ++i)
        if (name[i] == '\\' || name[i] == '/') base = i + 1;
    std::string clean;
    for (size_t i = base; i < name.size() && name[i] != ';' && clean.size() < 63; ++i) clean.push_back(name[i]);
    if (!fp || !discs_[0].open()) return 0;
    const auto e = vfs::iso_find(discs_[0].disc, clean);
    if (!e) return 0;
    u8 mm, ss, ff;
    lba_to_msf(e->lba, &mm, &ss, &ff);
    m.w8(fp, mm);
    m.w8(fp + 1, ss);
    m.w8(fp + 2, ff);
    m.w8(fp + 3, 0);
    m.w32(fp + 4, e->size);
    for (u32 k = 0; k < 16; ++k) m.w8(fp + 8 + k, 0);
    for (u32 k = 0; k < clean.size() && k < 15; ++k) m.w8(fp + 8 + k, static_cast<u8>(clean[k]));
    return fp;
}

u32 Cd::CdControl(u32 com, u32 param, u32 result) {
    command(com & 0xFFu, param, result);
    return 1;
}

u32 Cd::CdControlB(u32 com, u32 param, u32 result) {
    command(com & 0xFFu, param, result);
    return 1;
}

u32 Cd::CdControlF(u32 com, u32 param) {
    command(com & 0xFFu, param, 0);
    invoke_cb(sync_callback_, kCdlComplete, 0);
    return 1;
}

void Cd::command(u32 com, u32 param, u32 result) {
    switch (com) {
    case C_Setloc: {
        if (param) {
            const u32 lba = msf_to_lba(static_cast<u8>(m.r8(param)), static_cast<u8>(m.r8(param + 1)),
                                       static_cast<u8>(m.r8(param + 2)));
            if (lba != cur_lba_) {
                cur_lba_ = lba;
                fifo_valid_ = 0;
                consumed_known_ = 0;
                if (sector_credit_ > 0) sector_credit_ = 0;
            }
        }
        break;
    }
    case C_Setmode:
        cd_mode_ = param ? m.r8(param) : 0;
        xa_.set_enabled((cd_mode_ & kModeRT) != 0);
        break;
    case C_GetTN:
        if (result) {
            m.w8(result, 0x01);
            m.w8(result + 1, 0x01);
        }
        break;
    case C_GetTD:
        if (result) {
            u8 mm, ss, ff;
            lba_to_msf(drive_sectors(), &mm, &ss, &ff);
            m.w8(result, mm);
            m.w8(result + 1, ss);
            m.w8(result + 2, ff);
        }
        break;
    case C_GetlocL:
    case C_GetlocP:
        if (result) {
            u8 mm, ss, ff;
            lba_to_msf(cur_lba_, &mm, &ss, &ff);
            m.w8(result, mm);
            m.w8(result + 1, ss);
            m.w8(result + 2, ff);
            m.w8(result + 3, 1);
        }
        break;
    case C_ReadN:
    case C_ReadS:
        pack_note_read(cur_lba_);
        streaming_ = 1;
        break;
    case C_Pause:
    case C_Stop:
    case C_Standby:
        streaming_ = 0;
        xa_.stop();
        break;
    case C_Mute: xa_.set_muted(1); break;
    case C_Demute: xa_.set_muted(0); break;
    case C_Setfilter:
        if (param) {
            filter_file_ = static_cast<u8>(m.r8(param));
            filter_chan_ = static_cast<u8>(m.r8(param + 1));
            filter_active_ = 1;
            xa_.set_filter(1, filter_file_, filter_chan_);
        }
        break;
    default: break;
    }
    if (com != C_GetTN && com != C_GetTD && com != C_GetlocL && com != C_GetlocP) write_status(result);
    if (deliver_event) deliver_event(kEvClassCd, kEvSpecComplete);
}

u32 Cd::CdRead(u32 sectors, u32 dst, u32 mode) {
    cd_mode_ = mode;
    u8 whole[kWhole];
    u32 ok = 0;
    pack_note_read(cur_lba_);
    for (u32 i = 0; i < sectors; i++) {
        if (!read_sector_whole(cur_lba_ + i, whole)) break;
        u8* g = m.ptr(dst + i * kUser, kUser);
        if (!g) break;
        std::memcpy(g, whole + 12, kUser);
        ok++;
    }
    cur_lba_ += ok;
    if (deliver_event) deliver_event(kEvClassCd, kEvSpecComplete);
    return ok == sectors ? 1u : 0u;
}

u32 Cd::CdReadSync(u32, u32 result) {
    if (result) m.w8(result, 0x02);
    return 0;
}

u32 Cd::CdReset(u32) {
    streaming_ = 0;
    cur_lba_ = 0;
    xa_.reset();
    return 1;
}

u32 Cd::CdGetSector(u32 dst, u32 words) {
    u32 bytes = words * 4;
    if (!fifo_valid_ && !fifo_fill()) return 0;
    u8* g = m.ptr(dst, bytes);
    if (!g) return 0;
    if (fifo_pos_ >= kWhole) fifo_pos_ = delivery_offset();
    if (bytes > kWhole - fifo_pos_) bytes = kWhole - fifo_pos_;
    std::memcpy(g, fifo_ + fifo_pos_, bytes);
    if (bytes >= 512) {
        consumed_chan_ = fifo_[kSubChan];
        consumed_known_ = 1;
    }
    fifo_pos_ += bytes;
    if (fifo_pos_ >= kWhole) fifo_valid_ = 0;
    return 1;
}

int Cd::delivery_rate() const {
    if (rate_override_ > 0) return rate_override_;
    return (cd_mode_ & kModeSpeed) ? 150 : 75;
}

int Cd::deliver_one() {
    const int r = fifo_step();
    if (r < 0) return -1;
    if (r == 0) return 0;
    invoke_cb(ready_callback_, kCdlDataReady, 0);
    if (data_callback_) invoke_cb(data_callback_, kCdlDataReady, 0);
    return 1;
}

void Cd::tick() {
    sector_credit_ += delivery_rate();
    if (!streaming_) {
        if (sector_credit_ > 0) sector_credit_ = 0;
        return;
    }
    while (sector_credit_ >= kFramesPerSec) {
        const int r = deliver_one();
        if (r < 0) break;
        sector_credit_ -= kFramesPerSec;
        if (!streaming_) break;
    }
}

bool Cd::fifo_ends_record() const {
    if (consumed_known_ && fifo_[kSubChan] != consumed_chan_) return false;
    const u8* p = fifo_ + 12;
    const u32 chunks = static_cast<u32>(p[2]) | (static_cast<u32>(p[3]) << 8);
    return p[0] == 1 && chunks != 0 && static_cast<u32>(p[1]) + 1u >= chunks;
}

void Cd::stream_ahead() {
    if (!streaming_) return;
    for (int i = 0; i < 256; ++i) {
        const int r = deliver_one();
        if (r < 0) break;
        sector_credit_ -= kFramesPerSec;
        if (!streaming_) break;
        if (r == 1 && fifo_ends_record()) break;
    }
}

int Cd::debt_frames() const {
    const int rate = delivery_rate();
    if (sector_credit_ >= 0 || rate <= 0) return 0;
    return (-sector_credit_ + rate - 1) / rate;
}

bool Xa::load_state(StateReader& r) {
    if (!r.tag("XA")) return false;
    filter_active_ = r.s();
    filter_file_ = static_cast<u8>(r.u());
    filter_chan_ = static_cast<u8>(r.u());
    rt_enabled_ = r.s();
    muted_ = r.s();
    for (auto& h : hist_)
        for (s32& x : h) x = r.s();
    res_pos_ = r.s();
    for (s16& x : res_last_) x = static_cast<s16>(r.u());
    res_have_last_ = r.s();
    res_rate_ = r.u();
    return r.ok;
}

bool Cd::load_state(StateReader& r) {
    if (!r.tag("CD")) return false;
    streaming_ = r.s();
    cur_lba_ = r.u();
    cd_mode_ = r.u();
    filter_active_ = r.s();
    filter_file_ = static_cast<u8>(r.u());
    filter_chan_ = static_cast<u8>(r.u());
    consumed_chan_ = static_cast<u8>(r.u());
    consumed_known_ = r.s();
    sector_credit_ = r.s();
    rate_override_ = r.s();
    if (!r.block_into(fifo_, sizeof fifo_)) return false;
    fifo_lba_ = r.u();
    fifo_valid_ = r.s();
    fifo_pos_ = r.u();
    sync_callback_ = r.u();
    ready_callback_ = r.u();
    data_callback_ = r.u();
    for (int i = 0; i < 2; ++i) {
        const bool open = r.u() != 0;
        const u32 sectors = r.u(), base = r.u();
        r.u();
        if (open != discs_[i].open() || (open && (sectors != discs_[i].sectors || base != discs_[i].base)))
            return r.fail("the dump's discs are not the ones mounted"), false;
    }
    const u32 mlba = r.u(), msize = r.u();
    if (mlba != merged_index_lba_ || msize != merged_index_size_)
        return r.fail("the dump's merged index is not this mount's"), false;
    return r.ok;
}
}
