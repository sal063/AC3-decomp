#pragma once

#include "vfs/disc_image.hpp"

#include <optional>
#include <string>
#include <vector>

namespace ac3::vfs {
struct IsoEntry {
    std::string path;
    u32 lba = 0;
    u32 size = 0;
    bool directory = false;
};

std::vector<IsoEntry> iso_list(DiscImage& disc);

std::optional<IsoEntry> iso_find(DiscImage& disc, const std::string& name);

std::string iso_volume_id(DiscImage& disc);
}
