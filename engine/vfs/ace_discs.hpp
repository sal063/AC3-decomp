#pragma once

#include "vfs/disc_image.hpp"
#include "vfs/pack_index.hpp"

#include <string>
#include <vector>

namespace ac3::vfs {
struct StreamRef {
    bool available = false;
    int image = 0;
    u32 lba = 0;
    u32 sectors = 0;
    u32 load_size = 0;
    u32 word = 0;
};

class AceDiscs {
public:
    bool mount(const std::string& booted, const std::string& other = {}, bool one_disc = false);

    bool both_discs() const { return images_[1].is_open(); }
    int booted_number() const { return static_cast<int>(stream_index_[0].disc); }
    DiscImage& image(int which) { return images_[which]; }

    const PackIndex& base_index() const { return base_index_; }
    u32 base_count() const { return static_cast<u32>(base_index_.entries.size()); }
    u32 base_lba(u32 record) const { return base_lba_ + base_index_.entries[record].sector; }
    bool read_base(u32 i, Bytes& out);

    u32 stream_count() const { return static_cast<u32>(stream_index_[0].entries.size()); }
    const PackIndex& stream_index(int which) const { return stream_index_[which]; }
    StreamRef stream(u32 i) const;
    bool read_stream_block(u32 i, Bytes& out);

    std::vector<std::string> translation_dirs;

    static std::string other_disc_path(const std::string& path);
    const std::string& error() const { return error_; }

private:
    bool mount_one(int which, const std::string& path);

    DiscImage images_[2];
    PackIndex base_index_;
    PackIndex stream_index_[2];
    u32 base_lba_ = 0, base_sectors_ = 0;
    u32 spb_lba_[2] = {0, 0}, spb_sectors_[2] = {0, 0};
    std::string error_;
};
}
