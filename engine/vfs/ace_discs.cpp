#include "vfs/ace_discs.hpp"

#include "core/file.hpp"
#include "vfs/iso9660.hpp"
#include "vfs/translation.hpp"

#include <algorithm>

namespace ac3::vfs {
std::string AceDiscs::other_disc_path(const std::string& path) {
    std::string p = path;
    for (const auto& [from, to] : {std::pair{"(Disc 1)", "(Disc 2)"}, std::pair{"(Disc 2)", "(Disc 1)"}}) {
        if (const size_t at = p.rfind(from); at != std::string::npos) {
            p.replace(at, std::strlen(from), to);
            return p;
        }
    }
    return {};
}

bool AceDiscs::mount_one(int which, const std::string& path) {
    DiscImage& d = images_[which];
    if (!d.open(path)) {
        error_ = "cannot open " + path;
        return false;
    }
    if (!translation_dirs.empty()) {
        std::string note;
        if (!translate(d, translation_dirs, note)) {
            error_ = note;
            return false;
        }
    }
    auto sph = iso_find(d, "ACE.SPH");
    auto spb = iso_find(d, "ACE.SPB");
    if (!sph || !spb) {
        error_ = path + " has no ACE.SPH/ACE.SPB: not an Ace Combat 3 disc";
        return false;
    }
    Bytes data;
    if (!d.read_bytes(sph->lba, sph->size, data)) return false;
    auto idx = parse_index(data, true);
    if (!idx) {
        error_ = path + ": ACE.SPH is not a valid index";
        return false;
    }
    stream_index_[which] = std::move(*idx);
    spb_lba_[which] = spb->lba;
    spb_sectors_[which] = (spb->size + kUserSize - 1) / kUserSize;

    if (which == 0) {
        auto bph = iso_find(d, "ACE.BPH");
        auto bpb = iso_find(d, "ACE.BPB");
        if (!bph || !bpb || !d.read_bytes(bph->lba, bph->size, data)) {
            error_ = path + " has no ACE.BPH/ACE.BPB";
            return false;
        }
        auto bidx = parse_index(data, false);
        if (!bidx) {
            error_ = path + ": ACE.BPH is not a valid index";
            return false;
        }
        base_index_ = std::move(*bidx);
        base_lba_ = bpb->lba;
        base_sectors_ = (bpb->size + kUserSize - 1) / kUserSize;
    }
    return true;
}

bool AceDiscs::mount(const std::string& booted, const std::string& other, bool one_disc) {
    error_.clear();
    if (!mount_one(0, booted)) return false;
    if (one_disc) return true;
    const std::string second = other.empty() ? other_disc_path(booted) : other;
    if (second.empty() || !file::exists(second)) return true;
    if (!mount_one(1, second)) return false;
    const u32 a = stream_index_[0].disc, b = stream_index_[1].disc;
    if (!((a == 1 && b == 2) || (a == 2 && b == 1)) ||
        stream_index_[0].entries.size() != stream_index_[1].entries.size()) {
        error_ = second + " is not the other disc of " + booted;
        images_[1] = DiscImage{};
        return false;
    }
    return true;
}

bool AceDiscs::read_base(u32 i, Bytes& out) {
    if (i >= base_count()) return false;
    const IndexEntry& e = base_index_.entries[i];
    if (e.sector >= base_sectors_) return false;
    return images_[0].read_bytes(base_lba_ + e.sector, e.load_size, out);
}

StreamRef AceDiscs::stream(u32 i) const {
    StreamRef r;
    if (i >= stream_count()) return r;
    for (int which = 0; which < 2; ++which) {
        if (which == 1 && !images_[1].is_open()) break;
        const auto& entries = stream_index_[which].entries;
        const IndexEntry& e = entries[i];
        if (!e.present) continue;
        u32 next = spb_sectors_[which];
        for (const IndexEntry& o : entries)
            if (o.present && o.sector > e.sector && o.sector < next) next = o.sector;
        r.available = true;
        r.image = which;
        r.lba = spb_lba_[which] + e.sector;
        r.sectors = next - e.sector;
        r.load_size = e.load_size;
        r.word = e.word;
        return r;
    }
    return r;
}

bool AceDiscs::read_stream_block(u32 i, Bytes& out) {
    const StreamRef r = stream(i);
    if (!r.available) return false;
    return images_[r.image].read_bytes(r.lba, r.load_size, out);
}
}
