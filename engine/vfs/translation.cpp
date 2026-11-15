#include "vfs/translation.hpp"

#include "core/file.hpp"
#include "vfs/disc_image.hpp"
#include "vfs/iso9660.hpp"
#include "vfs/xdelta.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>

namespace ac3::vfs {
namespace {
constexpr u32 kRaw = 2352;
constexpr u32 kProbe = 1024;
constexpr u32 kLiteral = 0x80000000u;

class Source {
public:
    bool open(const std::string& path) {
        f_ = std::fopen(path.c_str(), "rb");
        if (!f_) return false;
        _fseeki64(f_, 0, SEEK_END);
        size_ = static_cast<u64>(_ftelli64(f_));
        return size_ > 0;
    }
    ~Source() {
        if (f_) std::fclose(f_);
    }
    u64 size() const { return size_; }
    const u8* stretch(u64 pos, u64 len) {
        if (pos + len > size_) return nullptr;
        if (pos >= at_ && pos + len <= at_ + have_.size()) return have_.data() + (pos - at_);
        Bytes next(static_cast<size_t>(len));
        u64 done = 0;
        if (pos >= at_ && pos < at_ + have_.size()) {
            done = at_ + have_.size() - pos;
            std::memcpy(next.data(), have_.data() + (pos - at_), static_cast<size_t>(done));
        }
        if (done < len) {
            if (_fseeki64(f_, static_cast<long long>(pos + done), SEEK_SET) != 0) return nullptr;
            if (std::fread(next.data() + done, 1, static_cast<size_t>(len - done), f_) != len - done) return nullptr;
        }
        have_ = std::move(next);
        at_ = pos;
        return have_.data();
    }
    bool sector(u32 lba, u8* out) {
        if (static_cast<u64>(lba + 1) * kRaw > size_) return false;
        if (_fseeki64(f_, static_cast<long long>(lba) * kRaw, SEEK_SET) != 0) return false;
        return std::fread(out, 1, kRaw, f_) == kRaw;
    }

private:
    std::FILE* f_ = nullptr;
    u64 size_ = 0, at_ = 0;
    Bytes have_;
};

void msf_of(u32 lba, u8* m) {
    const u32 t = lba + 150;
    const u32 mm = t / 4500, ss = (t / 75) % 60, ff = t % 75;
    m[0] = static_cast<u8>(((mm / 10) << 4) | (mm % 10));
    m[1] = static_cast<u8>(((ss / 10) << 4) | (ss % 10));
    m[2] = static_cast<u8>(((ff / 10) << 4) | (ff % 10));
}

bool has_token(const std::string& s, const std::string& token) {
    for (size_t at = 0; at + token.size() <= s.size(); ++at) {
        size_t i = 0;
        while (i < token.size() && std::tolower(static_cast<unsigned char>(s[at + i])) == token[i]) ++i;
        if (i == token.size() && (at + i >= s.size() || !std::isdigit(static_cast<unsigned char>(s[at + i])))) return true;
    }
    return false;
}

int patch_disc(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    for (int d = 1; d <= 2; ++d)
        if (has_token(name, "cd" + std::to_string(d)) || has_token(name, "disc " + std::to_string(d))) return d;
    std::string head(640, '\0');
    if (std::FILE* f = std::fopen(path.string().c_str(), "rb")) {
        head.resize(std::fread(head.data(), 1, head.size(), f));
        std::fclose(f);
    } else {
        return 0;
    }
    for (char& c : head)
        if (!c) c = ' ';
    for (int d = 1; d <= 2; ++d)
        if (has_token(head, "cd" + std::to_string(d))) return d;
    return 0;
}

std::string patch_in(const std::filesystem::path& dir, int disc, int depth) {
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return {};
    std::vector<std::filesystem::path> below;
    for (const auto& e : std::filesystem::directory_iterator(dir, ec)) {
        if (e.is_directory(ec)) {
            below.push_back(e.path());
            continue;
        }
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".xdelta" && patch_disc(e.path()) == disc) return e.path().string();
    }
    if (depth > 0)
        for (const auto& d : below)
            if (std::string found = patch_in(d, disc, depth - 1); !found.empty()) return found;
    return {};
}

constexpr char kMagic[8] = {'A', 'C', '3', 'E', 'N', 'O', 'V', '1'};
constexpr u32 kSamples = 64;

struct CacheHead {
    char magic[8];
    u32 version;
    u32 patch_adler;
    u64 patch_size;
    u64 image_size;
    u32 image_sample;
    u32 sectors;
    u32 literals;
    u32 body_adler;
};
static_assert(sizeof(CacheHead) == 48);

bool cache_key(const std::string& image, const Bytes& patch, CacheHead& h) {
    h = CacheHead{};
    std::memcpy(h.magic, kMagic, 8);
    h.version = 1;
    h.patch_size = patch.size();
    h.patch_adler = adler32(patch.data(), patch.size());
    Source src;
    if (!src.open(image) || src.size() < kRaw) return false;
    const u64 sectors = src.size() / kRaw;
    u32 a = 1;
    u8 sec[kRaw];
    for (u32 i = 0; i < kSamples; ++i) {
        u64 lba = i < 32 ? i : sectors * (i - 32) / (kSamples - 32);
        if (lba >= sectors) lba = sectors - 1;
        if (!src.sector(static_cast<u32>(lba), sec)) return false;
        a = adler32(sec, kRaw) ^ (a * 2654435761u);
    }
    h.image_size = src.size();
    h.image_sample = a;
    return true;
}
}

const u8* Translation::sector(u32 lba, u32& source) const {
    source = kNone;
    if (lba >= map_.size()) return nullptr;
    const u32 m = map_[lba];
    if (m & kLiteral) return literal_.data() + static_cast<size_t>(m & ~kLiteral) * kRaw;
    source = m;
    return nullptr;
}

std::string Translation::find_patch(int disc, const std::vector<std::string>& dirs) {
    for (const std::string& d : dirs) {
        if (d.empty()) continue;
        const std::filesystem::path dir(d);
        std::error_code ec;
        if (std::filesystem::is_regular_file(dir, ec)) {
            if (patch_disc(dir) == disc) return dir.string();
            continue;
        }
        if (std::string found = patch_in(dir, disc, 2); !found.empty()) return found;
    }
    return {};
}

std::shared_ptr<const Translation> Translation::build(const std::string& image, const std::string& patch_path,
                                                      std::string* error) {
    auto fail = [&](const std::string& why) {
        if (error) *error = why;
        return std::shared_ptr<const Translation>();
    };
    const auto patch = file::read_all(patch_path);
    if (!patch) return fail("cannot read " + patch_path);
    Xdelta x;
    if (!x.parse(*patch)) return fail(patch_path + " is not an xdelta patch this can read");
    if (x.target_size() % kRaw || x.target_size() / kRaw > 0x7FFFFFFFu) return fail(patch_path + " does not make a disc image");
    Source src;
    if (!src.open(image)) return fail("cannot open " + image);
    const u32 sectors = static_cast<u32>(x.target_size() / kRaw), src_sectors = static_cast<u32>(src.size() / kRaw);

    auto out = std::make_shared<Translation>();
    out->map_.assign(sectors, 0);
    std::vector<u32> hint(sectors, kNone);
    const Xdelta::Copied copied = [&](u64 target, u32 size, u64 source) {
        const u64 end = target + size;
        for (u64 k = target > kProbe ? (target - kProbe + kRaw - 1) / kRaw : 0; k < sectors; ++k) {
            const u64 probe = k * kRaw + kProbe;
            if (probe >= end) break;
            if (probe < target) continue;
            const u64 s = source + (probe - target);
            if (s >= kProbe && (s - kProbe) % kRaw == 0) hint[static_cast<size_t>(k)] = static_cast<u32>((s - kProbe) / kRaw);
        }
    };
    u8 theirs[kRaw];
    auto same_as = [&](const u8* sec, u32 s) {
        return s < src_sectors && src.sector(s, theirs) && std::memcmp(sec, theirs, 12) == 0 &&
               std::memcmp(sec + 15, theirs + 15, kRaw - 15) == 0;
    };
    u32 last = kNone - 1;
    auto classify = [&](u32 k, const u8* sec) {
        u8 m[3];
        msf_of(k, m);
        if (std::memcmp(sec + 12, m, 3) == 0) {
            for (const u32 candidate : {k, hint[k], last + 1}) {
                if (candidate == kNone || !same_as(sec, candidate)) continue;
                out->map_[k] = candidate;
                last = candidate;
                return;
            }
        }
        out->map_[k] = kLiteral | static_cast<u32>(out->literal_.size() / kRaw);
        out->literal_.insert(out->literal_.end(), sec, sec + kRaw);
        last = kNone - 1;
    };

    Bytes target(x.largest_window() ? x.largest_window() : 1);
    u8 part[kRaw];
    u32 part_fill = 0, next = 0;
    for (const XdeltaWindow& w : x.windows()) {
        const u8* source = w.src_len ? src.stretch(w.src_pos, w.src_len) : target.data();
        if (!source) return fail(image + " is smaller than the disc the patch was made for");
        if (!Xdelta::apply(w, source, target.data(), copied) ||
            (w.has_adler && adler32(target.data(), w.target_len) != w.adler))
            return fail(image + " does not match the disc image the patch was made for (it wants the Redump \"Rev 1\" image)");
        u32 off = 0;
        if (part_fill) {
            const u32 take = std::min(kRaw - part_fill, w.target_len);
            std::memcpy(part + part_fill, target.data(), take);
            part_fill += take;
            off = take;
            if (part_fill == kRaw) {
                if (next >= sectors) return fail(patch_path + ": malformed");
                classify(next++, part);
                part_fill = 0;
            }
        }
        while (off + kRaw <= w.target_len) {
            if (next >= sectors) return fail(patch_path + ": malformed");
            classify(next++, target.data() + off);
            off += kRaw;
        }
        if (off < w.target_len) {
            std::memcpy(part, target.data() + off, w.target_len - off);
            part_fill = w.target_len - off;
        }
    }
    if (next != sectors || part_fill) return fail(patch_path + ": malformed");
    return out;
}

std::shared_ptr<const Translation> Translation::open(const std::string& image, const std::string& patch_path, bool* cached,
                                                     std::string* error) {
    if (cached) *cached = false;
    const std::string cache_path = image + ".en.cache";
    const auto patch = file::read_all(patch_path);
    if (!patch) {
        if (error) *error = "cannot read " + patch_path;
        return nullptr;
    }
    CacheHead key;
    const bool have_key = cache_key(image, *patch, key);
    if (have_key) {
        if (const auto b = file::read_all(cache_path); b && b->size() >= sizeof(CacheHead)) {
            CacheHead h;
            std::memcpy(&h, b->data(), sizeof h);
            const size_t need = sizeof h + static_cast<size_t>(h.sectors) * 4 + static_cast<size_t>(h.literals) * kRaw;
            if (std::memcmp(h.magic, key.magic, 8) == 0 && h.version == key.version && h.patch_adler == key.patch_adler &&
                h.patch_size == key.patch_size && h.image_size == key.image_size && h.image_sample == key.image_sample &&
                b->size() == need && adler32(b->data() + sizeof h, need - sizeof h) == h.body_adler) {
                auto out = std::make_shared<Translation>();
                out->map_.resize(h.sectors);
                std::memcpy(out->map_.data(), b->data() + sizeof h, static_cast<size_t>(h.sectors) * 4);
                out->literal_.assign(b->begin() + static_cast<std::ptrdiff_t>(sizeof h + static_cast<size_t>(h.sectors) * 4), b->end());
                bool sound = true;
                for (const u32 m : out->map_)
                    sound = sound && ((m & kLiteral) ? (m & ~kLiteral) < h.literals : static_cast<u64>(m) * kRaw < h.image_size);
                if (sound) {
                    if (cached) *cached = true;
                    return out;
                }
            }
        }
    }
    auto out = build(image, patch_path, error);
    if (out && have_key) {
        CacheHead h = key;
        h.sectors = out->sectors();
        h.literals = out->literals();
        Bytes body(static_cast<size_t>(h.sectors) * 4 + out->literal_.size());
        std::memcpy(body.data(), out->map_.data(), static_cast<size_t>(h.sectors) * 4);
        if (!out->literal_.empty())
            std::memcpy(body.data() + static_cast<size_t>(h.sectors) * 4, out->literal_.data(), out->literal_.size());
        h.body_adler = adler32(body.data(), body.size());
        Bytes whole(sizeof h + body.size());
        std::memcpy(whole.data(), &h, sizeof h);
        std::memcpy(whole.data() + sizeof h, body.data(), body.size());
        const std::string tmp = cache_path + ".tmp";
        if (file::write_all(tmp, whole)) {
            std::remove(cache_path.c_str());
            if (std::rename(tmp.c_str(), cache_path.c_str()) != 0) std::remove(tmp.c_str());
        }
    }
    return out;
}

bool translate(DiscImage& image, const std::vector<std::string>& dirs, std::string& note) {
    std::string id = iso_volume_id(image);
    while (!id.empty() && id.back() == ' ') id.pop_back();
    const int disc = id.rfind("ACE3ES", 0) == 0 && id.back() == '1' ? 1 : id.rfind("ACE3ES", 0) == 0 && id.back() == '2' ? 2 : 0;
    if (!disc) {
        note = image.path() + " is not an Ace Combat 3 disc; left as it is";
        return false;
    }
    const std::string patch = Translation::find_patch(disc, dirs);
    if (patch.empty()) {
        note = "no Team NEMO patch (.xdelta) for disc " + std::to_string(disc) + " was found; it stays Japanese";
        return false;
    }
    bool cached = false;
    std::string error;
    auto t = Translation::open(image.path(), patch, &cached, &error);
    if (!t) {
        note = error + "; disc " + std::to_string(disc) + " stays Japanese";
        return false;
    }
    note = "disc " + std::to_string(disc) + " in English, from " + patch + " (" + std::to_string(t->literals()) +
           " new sectors" + (cached ? ", cached)" : ")");
    image.set_translation(std::move(t));
    return true;
}
}
