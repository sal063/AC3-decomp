#pragma once

#include "core/bytes.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ac3::vfs {
class DiscImage;

class Translation {
public:
    static constexpr u32 kNone = 0xFFFFFFFFu;

    static std::shared_ptr<const Translation> open(const std::string& image, const std::string& patch, bool* cached,
                                                   std::string* error);
    static std::shared_ptr<const Translation> build(const std::string& image, const std::string& patch, std::string* error);

    static std::string find_patch(int disc, const std::vector<std::string>& dirs);

    u32 sectors() const { return static_cast<u32>(map_.size()); }
    u32 literals() const { return static_cast<u32>(literal_.size() / 2352); }
    const u8* sector(u32 lba, u32& source) const;
    bool same(const Translation& o) const { return map_ == o.map_ && literal_ == o.literal_; }

private:
    std::vector<u32> map_;
    Bytes literal_;
};

bool translate(DiscImage& image, const std::vector<std::string>& dirs, std::string& note);
}
