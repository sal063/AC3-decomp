#pragma once

#include "core/bytes.hpp"

#include <cstdio>
#include <memory>
#include <string>

namespace ac3::vfs {
constexpr u32 kRawSector = 2352;
constexpr u32 kUserOffset = 24;
constexpr u32 kUserSize = 2048;
constexpr u32 kForm2Size = 2324;

constexpr u8 kSubEOR = 0x01, kSubVideo = 0x02, kSubAudio = 0x04, kSubData = 0x08,
             kSubTrigger = 0x10, kSubForm2 = 0x20, kSubRealTime = 0x40, kSubEOF = 0x80;

struct SectorHeader {
    u8 file, channel, submode, coding;
};

class Translation;

class DiscImage {
public:
    DiscImage() = default;
    ~DiscImage();
    DiscImage(const DiscImage&) = delete;
    DiscImage& operator=(const DiscImage&) = delete;
    DiscImage(DiscImage&& o) noexcept;
    DiscImage& operator=(DiscImage&& o) noexcept;

    bool open(const std::string& path);
    bool is_open() const { return f_ != nullptr; }
    const std::string& path() const { return path_; }
    u32 sectors() const { return sectors_; }

    void set_translation(std::shared_ptr<const Translation> t);
    const Translation* translation() const { return translation_.get(); }
    u32 own_sectors() const { return own_sectors_; }
    bool read_own_user(u32 lba, u8 out[kUserSize]);

    bool read_raw(u32 lba, u8 out[kRawSector]);
    bool read_user(u32 lba, u8 out[kUserSize]);
    bool read_bytes(u32 lba, u32 n, Bytes& out);
    bool read_header(u32 lba, SectorHeader& out);

private:
    const u8* resolve(u32 lba, u32& source) const;

    std::FILE* f_ = nullptr;
    std::string path_;
    u32 sectors_ = 0, own_sectors_ = 0;
    std::shared_ptr<const Translation> translation_;
};
}
