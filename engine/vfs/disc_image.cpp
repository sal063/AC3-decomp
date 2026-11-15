#include "vfs/disc_image.hpp"

#include "vfs/translation.hpp"

#include <utility>

namespace ac3::vfs {
DiscImage::~DiscImage() {
    if (f_) std::fclose(f_);
}

DiscImage::DiscImage(DiscImage&& o) noexcept
    : f_(std::exchange(o.f_, nullptr)), path_(std::move(o.path_)), sectors_(o.sectors_), own_sectors_(o.own_sectors_),
      translation_(std::move(o.translation_)) {}

DiscImage& DiscImage::operator=(DiscImage&& o) noexcept {
    if (this != &o) {
        if (f_) std::fclose(f_);
        f_ = std::exchange(o.f_, nullptr);
        path_ = std::move(o.path_);
        sectors_ = o.sectors_;
        own_sectors_ = o.own_sectors_;
        translation_ = std::move(o.translation_);
    }
    return *this;
}

bool DiscImage::open(const std::string& path) {
    if (f_) std::fclose(f_);
    f_ = std::fopen(path.c_str(), "rb");
    if (!f_) return false;
    std::setvbuf(f_, nullptr, _IOFBF, 1 << 20);
    _fseeki64(f_, 0, SEEK_END);
    const long long size = _ftelli64(f_);
    sectors_ = own_sectors_ = size > 0 ? static_cast<u32>(size / kRawSector) : 0;
    translation_.reset();
    path_ = path;
    return sectors_ > 0;
}

void DiscImage::set_translation(std::shared_ptr<const Translation> t) {
    translation_ = std::move(t);
    sectors_ = translation_ ? translation_->sectors() : own_sectors_;
}

const u8* DiscImage::resolve(u32 lba, u32& source) const {
    source = lba;
    return translation_ ? translation_->sector(lba, source) : nullptr;
}

bool DiscImage::read_raw(u32 lba, u8 out[kRawSector]) {
    if (!f_ || lba >= sectors_) return false;
    u32 source;
    if (const u8* own = resolve(lba, source)) {
        std::memcpy(out, own, kRawSector);
        return true;
    }
    if (source >= own_sectors_) return false;
    if (_fseeki64(f_, static_cast<long long>(source) * kRawSector, SEEK_SET) != 0) return false;
    if (std::fread(out, 1, kRawSector, f_) != kRawSector) return false;
    if (source != lba) {
        const u32 t = lba + 150, mm = t / 4500, ss = (t / 75) % 60, ff = t % 75;
        out[12] = static_cast<u8>(((mm / 10) << 4) | (mm % 10));
        out[13] = static_cast<u8>(((ss / 10) << 4) | (ss % 10));
        out[14] = static_cast<u8>(((ff / 10) << 4) | (ff % 10));
    }
    return true;
}

bool DiscImage::read_user(u32 lba, u8 out[kUserSize]) {
    if (!f_ || lba >= sectors_) return false;
    u32 source;
    if (const u8* own = resolve(lba, source)) {
        std::memcpy(out, own + kUserOffset, kUserSize);
        return true;
    }
    return read_own_user(source, out);
}

bool DiscImage::read_own_user(u32 lba, u8 out[kUserSize]) {
    if (!f_ || lba >= own_sectors_) return false;
    if (_fseeki64(f_, static_cast<long long>(lba) * kRawSector + kUserOffset, SEEK_SET) != 0) return false;
    return std::fread(out, 1, kUserSize, f_) == kUserSize;
}

bool DiscImage::read_bytes(u32 lba, u32 n, Bytes& out) {
    out.resize(n);
    u32 done = 0;
    u8 sec[kUserSize];
    while (done < n) {
        if (!read_user(lba++, sec)) return false;
        const u32 take = std::min(kUserSize, n - done);
        std::memcpy(out.data() + done, sec, take);
        done += take;
    }
    return true;
}

bool DiscImage::read_header(u32 lba, SectorHeader& out) {
    u8 sub[4];
    if (!f_ || lba >= sectors_) return false;
    u32 source;
    if (const u8* own = resolve(lba, source)) {
        out = {own[16], own[17], own[18], own[19]};
        return true;
    }
    if (source >= own_sectors_) return false;
    if (_fseeki64(f_, static_cast<long long>(source) * kRawSector + 16, SEEK_SET) != 0) return false;
    if (std::fread(sub, 1, 4, f_) != 4) return false;
    out = {sub[0], sub[1], sub[2], sub[3]};
    return true;
}
}
