#include "assets/mission.hpp"
#include "assets/tim.hpp"
#include "assets/vram.hpp"
#include "core/file.hpp"
#include "core/hash.hpp"
#include "core/png.hpp"
#include "vfs/ace_discs.hpp"
#include "vfs/container.hpp"
#include "vfs/iso9660.hpp"
#include "vfs/translation.hpp"
#include "vfs/ulz.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using namespace ac3;
using namespace ac3::vfs;

namespace {
int usage() {
    std::fputs("usage: ac3cat --disc <image.bin> [--one-disc] [--english <dir>] "
               "info|records|tree <n>|streams|export <n> <dir>|catalog <out.json>|dump-ulz <dir>|verify|"
               "file <name> <out>|raw <dir>|english-check\n",
               stderr);
    return 2;
}

std::string json_escape(const std::string& s) {
    std::string o;
    for (char c : s) {
        if (c == '"' || c == '\\') o += '\\';
        o += c;
    }
    return o;
}

std::string describe(ByteView b) {
    if (ulz::is_ulz(b)) return "ulz(" + std::to_string(ulz::decoded_size(b)) + ")";
    if (is_directory(b)) return "dir[" + std::to_string(le32(b.data())) + "]";
    if (assets::is_tim(b)) return "tim";
    return "data";
}

int cmd_info(AceDiscs& discs) {
    DiscImage& d = discs.image(0);
    std::printf("disc image      %s\n", d.path().c_str());
    std::printf("volume          %s, %u sectors\n", iso_volume_id(d).c_str(), d.sectors());
    std::printf("booted disc     %d%s\n", discs.booted_number(),
                discs.both_discs() ? "; the other disc is mounted too" : "; one disc only");
    for (const IsoEntry& e : iso_list(d))
        std::printf("  %s %-16s lba %7u  %10u bytes\n", e.directory ? "D" : "F", e.path.c_str(), e.lba, e.size);
    const PackIndex& b = discs.base_index();
    std::printf("base pack       %zu records (index disc %u, date %08X, unknown %08X)\n", b.entries.size(),
                b.disc, b.date, b.unknown);
    for (int w = 0; w < (discs.both_discs() ? 2 : 1); ++w) {
        const PackIndex& s = discs.stream_index(w);
        size_t present = 0;
        for (const auto& e : s.entries) present += e.present;
        std::printf("stream pack %d   %zu streams, %zu on this disc (index disc %u, date %08X)\n", w,
                    s.entries.size(), present, s.disc, s.date);
    }
    size_t available = 0;
    for (u32 i = 0; i < discs.stream_count(); ++i) available += discs.stream(i).available;
    std::printf("streams         %zu of %u available\n", available, discs.stream_count());
    return 0;
}

int cmd_records(AceDiscs& discs) {
    Bytes data;
    for (u32 i = 0; i < discs.base_count(); ++i) {
        const IndexEntry& e = discs.base_index().entries[i];
        if (!discs.read_base(i, data)) {
            std::printf("%4u  sector %6u  %8u bytes  UNREADABLE\n", i, e.sector, e.load_size);
            continue;
        }
        std::printf("%4u  sector %6u  %8u bytes  %s\n", i, e.sector, e.load_size, describe(data).c_str());
    }
    return 0;
}

int cmd_tree(AceDiscs& discs, u32 record) {
    Bytes data;
    if (!discs.read_base(record, data)) return std::fprintf(stderr, "cannot read record %u\n", record), 1;
    walk_tree(data, std::to_string(record), [](const Node& n, ByteView b) {
        std::printf("%*s%-14s %-9s %8u bytes", n.depth * 2, "", n.path.c_str(), kind_name(n.kind), n.size);
        if (n.kind == NodeKind::ulz) std::printf(" -> %u", n.decoded_size);
        if (n.kind == NodeKind::directory) std::printf(" [%u]", n.children);
        if (n.kind == NodeKind::tim) {
            if (auto t = assets::parse_tim(b))
                std::printf("  %ubpp %ux%u at vram (%u,%u)%s", t->depth == 0 ? 4 : t->depth == 1 ? 8 : t->depth == 2 ? 16 : 24,
                            t->pixel_width(), t->img_h, t->img_x, t->img_y,
                            t->has_palette ? (" palette " + std::to_string(t->pal_w) + "x" + std::to_string(t->pal_h) +
                                              " at (" + std::to_string(t->pal_x) + "," + std::to_string(t->pal_y) + ")").c_str()
                                           : "");
        }
        std::printf("\n");
    });
    return 0;
}

struct Census {
    u32 data = 0, audio = 0, video = 0, other = 0, unreadable = 0;
    std::map<u32, u32> channels;
};

Census census(AceDiscs& discs, const StreamRef& r) {
    Census c;
    for (u32 s = 0; s < r.sectors; ++s) {
        SectorHeader h;
        if (!discs.image(r.image).read_header(r.lba + s, h)) {
            ++c.unreadable;
            continue;
        }
        if (h.submode & kSubAudio) ++c.audio;
        else if (h.submode & kSubVideo) ++c.video;
        else if (h.submode & kSubData) ++c.data;
        else ++c.other;
        ++c.channels[(static_cast<u32>(h.file) << 8) | h.channel];
    }
    return c;
}

int cmd_streams(AceDiscs& discs) {
    for (u32 i = 0; i < discs.stream_count(); ++i) {
        const StreamRef r = discs.stream(i);
        if (!r.available) {
            std::printf("%4u  not on the mounted disc(s)\n", i);
            continue;
        }
        const Census c = census(discs, r);
        std::printf("%4u  disc %d lba %6u  %6u sectors  load %7u  flags %X  data %u audio %u video %u other %u  channels %zu\n",
                    i, r.image == 0 ? discs.booted_number() : 3 - discs.booted_number(), r.lba, r.sectors,
                    r.load_size, (r.word >> 27) & 0xF, c.data, c.audio, c.video, c.other, c.channels.size());
    }
    return 0;
}

int cmd_export(AceDiscs& discs, u32 record, const std::string& dir) {
    Bytes data;
    if (!discs.read_base(record, data)) return std::fprintf(stderr, "cannot read record %u\n", record), 1;
    file::make_dirs(dir);
    int files = 0;
    walk_tree(data, std::to_string(record), [&](const Node& n, ByteView b) {
        std::string base = dir + "/" + n.path;
        for (char& c : base)
            if (c == '/') c = '_';
        base = dir + "/" + base.substr(dir.size() + 1);
        if (n.kind == NodeKind::data || n.kind == NodeKind::ulz_error) {
            file::write_all(base + ".bin", b);
            ++files;
        } else if (n.kind == NodeKind::tim) {
            file::write_all(base + ".tim", b);
            ++files;
            if (auto t = assets::parse_tim(b)) {
                const u32 rows = (t->depth <= 1 && t->pal_h) ? t->pal_h : 1;
                for (u32 row = 0; row < rows && row < 16; ++row) {
                    const assets::Image img = assets::to_image(*t, row);
                    const std::string name = rows > 1 ? base + "_pal" + std::to_string(row) + ".png" : base + ".png";
                    png::write_rgba(name, reinterpret_cast<const u8*>(img.pixels.data()), img.width, img.height);
                    ++files;
                }
            }
        }
    });
    std::printf("wrote %d files to %s\n", files, dir.c_str());
    return 0;
}

int cmd_catalog(AceDiscs& discs, const std::string& out_path) {
    std::string j = "{\n  \"format\": 1,\n";
    j += "  \"booted_disc\": " + std::to_string(discs.booted_number()) + ",\n";
    j += "  \"both_discs\": " + std::string(discs.both_discs() ? "true" : "false") + ",\n";
    j += "  \"records\": [\n";
    Bytes data;
    std::map<std::string, u32> kinds;
    for (u32 i = 0; i < discs.base_count(); ++i) {
        const IndexEntry& e = discs.base_index().entries[i];
        j += "    {\"index\": " + std::to_string(i) + ", \"sector\": " + std::to_string(e.sector) +
             ", \"size\": " + std::to_string(e.load_size);
        if (!discs.read_base(i, data)) {
            j += ", \"error\": \"unreadable\"}";
        } else {
            j += ", \"hash\": \"" + hex64(fnv1a64(data)) + "\", \"nodes\": [";
            bool first = true;
            walk_tree(data, std::to_string(i), [&](const Node& n, ByteView b) {
                ++kinds[kind_name(n.kind)];
                j += first ? "" : ", ";
                first = false;
                j += "{\"path\": \"" + json_escape(n.path) + "\", \"kind\": \"" + kind_name(n.kind) +
                     "\", \"size\": " + std::to_string(n.size);
                if (n.kind == NodeKind::ulz || n.kind == NodeKind::ulz_error)
                    j += ", \"decoded\": " + std::to_string(n.decoded_size);
                if (n.kind == NodeKind::directory) j += ", \"children\": " + std::to_string(n.children);
                if (n.kind == NodeKind::tim)
                    if (auto t = assets::parse_tim(b))
                        j += ", \"tim\": {\"depth\": " + std::to_string(t->depth) + ", \"x\": " + std::to_string(t->img_x) +
                             ", \"y\": " + std::to_string(t->img_y) + ", \"w\": " + std::to_string(t->img_w) +
                             ", \"h\": " + std::to_string(t->img_h) + ", \"pal\": [" + std::to_string(t->pal_x) + ", " +
                             std::to_string(t->pal_y) + ", " + std::to_string(t->pal_w) + ", " + std::to_string(t->pal_h) + "]}";
                if (n.kind == NodeKind::data) j += ", \"hash\": \"" + hex64(fnv1a64(b)) + "\"";
                j += "}";
            });
            j += "]}";
        }
        j += i + 1 < discs.base_count() ? ",\n" : "\n";
    }
    j += "  ],\n  \"streams\": [\n";
    for (u32 i = 0; i < discs.stream_count(); ++i) {
        const StreamRef r = discs.stream(i);
        j += "    {\"index\": " + std::to_string(i) + ", \"available\": " + (r.available ? "true" : "false");
        if (r.available) {
            const Census c = census(discs, r);
            j += ", \"disc\": " + std::to_string(r.image == 0 ? discs.booted_number() : 3 - discs.booted_number()) +
                 ", \"lba\": " + std::to_string(r.lba) + ", \"sectors\": " + std::to_string(r.sectors) +
                 ", \"load_size\": " + std::to_string(r.load_size) + ", \"word\": \"" + hex64(r.word).substr(8) +
                 "\", \"data\": " + std::to_string(c.data) + ", \"audio\": " + std::to_string(c.audio) +
                 ", \"video\": " + std::to_string(c.video) + ", \"other\": " + std::to_string(c.other) +
                 ", \"channels\": " + std::to_string(c.channels.size());
            Bytes block;
            if (r.load_size && discs.read_stream_block(i, block))
                j += ", \"block\": \"" + json_escape(describe(block)) + "\", \"block_hash\": \"" + hex64(fnv1a64(block)) + "\"";
        }
        j += i + 1 < discs.stream_count() ? "},\n" : "}\n";
    }
    j += "  ],\n  \"node_kinds\": {";
    bool first = true;
    for (const auto& [k, n] : kinds) {
        j += (first ? "" : ", ") + std::string("\"") + k + "\": " + std::to_string(n);
        first = false;
    }
    j += "}\n}\n";
    if (!file::write_text(out_path, j)) return std::fprintf(stderr, "cannot write %s\n", out_path.c_str()), 1;
    std::printf("catalogue written to %s\n", out_path.c_str());
    for (const auto& [k, n] : kinds) std::printf("  %-9s %u nodes\n", k.c_str(), n);
    return 0;
}

int cmd_node(AceDiscs& discs, const std::string& path, const std::string& out) {
    const u32 record = static_cast<u32>(std::strtoul(path.c_str(), nullptr, 10));
    Bytes data;
    if (!discs.read_base(record, data)) return std::fprintf(stderr, "cannot read record %u\n", record), 1;
    bool found = false;
    walk_tree(data, std::to_string(record), [&](const Node& n, ByteView b) {
        if (n.path == path && !found) {
            found = file::write_all(out, b);
            std::printf("%s: %s, %zu bytes -> %s\n", path.c_str(), kind_name(n.kind), b.size(), out.c_str());
        }
    });
    if (!found) std::fprintf(stderr, "no node %s\n", path.c_str());
    return found ? 0 : 1;
}

assets::Rgba8 tile_colour(const assets::TileType& t, const assets::Vram& vram) {
    double r = 0, g = 0, b = 0, n = 0;
    for (const assets::TilePoly& p : t.polys) {
        const int corners = (p.idx[3] == p.idx[2] || p.idx[3] == p.idx[1]) ? 3 : 4;
        u32 su = 0, sv = 0;
        for (int k = 0; k < corners; ++k) { su += p.u[k]; sv += p.v[k]; }
        for (int k = 0; k <= corners; ++k) {
            const u32 u = k < corners ? p.u[k] : su / static_cast<u32>(corners);
            const u32 v = k < corners ? p.v[k] : sv / static_cast<u32>(corners);
            const u16 c = vram.texel(p.tpage, p.clut, u, v);
            if (c == 0) continue;
            const double mr = (p.rgb[0] & 255) / 128.0, mg = ((p.rgb[0] >> 8) & 255) / 128.0,
                         mb = ((p.rgb[0] >> 16) & 255) / 128.0;
            r += (c & 31) * 8.2 * mr;
            g += ((c >> 5) & 31) * 8.2 * mg;
            b += ((c >> 10) & 31) * 8.2 * mb;
            n += 1;
        }
    }
    if (n == 0) return {0, 0, 0, 255};
    auto cl = [](double v) { return static_cast<u8>(v > 255 ? 255 : v); };
    return {cl(r / n), cl(g / n), cl(b / n), 255};
}

int cmd_mission(AceDiscs& discs, int m, const std::string& dir) {
    std::string err;
    auto ms = assets::load_mission(discs, m, &err);
    if (!ms) return std::fprintf(stderr, "mission %d: %s\n", m, err.c_str()), 1;
    file::make_dirs(dir);
    std::printf("mission %d = record %u\n", m, ms->record);
    for (int i = 0; i < 17; ++i) std::printf("  child %2d  %7zu bytes\n", i, ms->child[i].size());
    size_t verts = 0, polys = 0;
    for (const auto& t : ms->tiles) { verts += t.verts.size(); polys += t.polys.size(); }
    std::vector<int> used(512, 0);
    for (u16 g : ms->grid) used[g & 0x1FF] = 1;
    int n_used = 0;
    for (int u : used) n_used += u;
    std::printf("terrain: %zu tile types (%zu vertices, %zu polygons); %d types used in the grid\n",
                ms->tiles.size(), verts, polys, n_used);
    for (int k = 0; k < 5; ++k) std::printf("texture set %d (child %d): %zu TIMs\n", k, 11 + k, ms->texture_sets[k].size());

    assets::Vram vram;
    for (const auto& t : assets::boot_textures(discs)) vram.upload_tim(t);
    assets::upload_mission_textures(*ms, vram);
    const assets::Image vimg = vram.to_image();
    png::write_rgba(dir + "/vram.png", reinterpret_cast<const u8*>(vimg.pixels.data()), vimg.width, vimg.height);

    std::vector<assets::Rgba8> colours(512, {255, 0, 255, 255});
    std::vector<float> top(512, 0);
    for (size_t i = 0; i < ms->tiles.size(); ++i) {
        colours[i + 1] = tile_colour(ms->tiles[i], vram);
        float mn = 1e9f;
        for (const auto& v : ms->tiles[i].verts) mn = std::min(mn, static_cast<float>(v.y));
        top[i + 1] = ms->tiles[i].verts.empty() ? 0.0f : mn;
    }
    std::vector<assets::Rgba8> map(65536), height(65536);
    float lo = 0, hi = 0;
    for (u16 g : ms->grid) { lo = std::min(lo, top[g & 0x1FF]); hi = std::max(hi, top[g & 0x1FF]); }
    for (int row = 0; row < 256; ++row)
        for (int col = 0; col < 256; ++col) {
            const u16 t = ms->grid[static_cast<size_t>(row) * 256 + col] & 0x1FF;
            const size_t o = static_cast<size_t>(255 - row) * 256 + col;
            map[o] = colours[t];
            const u8 h = static_cast<u8>(hi > lo ? 255.0f * (hi - top[t]) / (hi - lo) : 0);
            height[o] = {h, h, h, 255};
        }
    png::write_rgba(dir + "/map.png", reinterpret_cast<const u8*>(map.data()), 256, 256);
    png::write_rgba(dir + "/height.png", reinterpret_cast<const u8*>(height.data()), 256, 256);
    std::printf("wrote vram.png, map.png, height.png to %s\n", dir.c_str());
    return 0;
}

int cmd_vram_compare(AceDiscs& discs, int m, const std::string& oracle_path) {
    auto oracle = file::read_all(oracle_path);
    if (!oracle || oracle->size() != 1024u * 512u * 2u)
        return std::fprintf(stderr, "%s is not a 1024x512 VRAM dump\n", oracle_path.c_str()), 1;
    auto ov = [&](int x, int y) { return le16(oracle->data() + (static_cast<size_t>(y) * 1024 + x) * 2); };
    std::string err;
    auto ms = assets::load_mission(discs, m, &err);
    if (!ms) return std::fprintf(stderr, "mission %d: %s\n", m, err.c_str()), 1;

    assets::Vram mv;
    assets::upload_mission_textures(*ms, mv);
    u32 written = 0, wrong = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 1024; ++x)
            if (mv.written(x, y)) {
                ++written;
                if (mv.at(x, y) != ov(x, y)) ++wrong;
            }
    std::printf("mission %d textures: %u VRAM pixels written, %u differ from the oracle\n", m, written, wrong);
    int shown = 0;
    for (int k = 4; k >= 0 && shown < 12; --k)
        for (const auto& t : ms->texture_sets[k]) {
            u32 bad = 0, all = 0;
            for (int y = 0; y < t.img_h; ++y)
                for (int x = 0; x < t.img_w; ++x, ++all)
                    if (mv.at(t.img_x + x, t.img_y + y) != ov((t.img_x + x) & 1023, (t.img_y + y) & 511)) ++bad;
            if (bad && shown++ < 12)
                std::printf("  set %d TIM at (%u,%u) %ux%u: %u of %u differ\n", k, t.img_x, t.img_y, t.img_w, t.img_h, bad, all);
        }

    assets::Vram bv;
    for (const auto& t : assets::boot_textures(discs)) bv.upload_tim(t);
    u32 b_written = 0, b_wrong = 0;
    for (int y = 0; y < 512; ++y)
        for (int x = 0; x < 1024; ++x)
            if (bv.written(x, y) && !mv.written(x, y)) {
                ++b_written;
                if (bv.at(x, y) != ov(x, y)) ++b_wrong;
            }
    std::printf("boot textures (not covered by the mission's): %u pixels, %u differ\n", b_written, b_wrong);
    const bool pass = written > 0 && wrong == 0;
    std::printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

int cmd_dump_ulz(AceDiscs& discs, const std::string& dir) {
    file::make_dirs(dir);
    Bytes data;
    int n = 0, bad = 0;
    std::string list;
    for (u32 i = 0; i < discs.base_count(); ++i) {
        if (!discs.read_base(i, data)) continue;
        walk_tree(data, std::to_string(i), [&](const Node& node, ByteView b) {
            if (node.kind != NodeKind::ulz && node.kind != NodeKind::ulz_error) return;
            std::string name = node.path;
            for (char& c : name)
                if (c == '/') c = '_';
            file::write_all(dir + "/" + name + ".ulz", b);
            Bytes out;
            const ulz::Error err = ulz::decode(b, out);
            if (err == ulz::Error::none) file::write_all(dir + "/" + name + ".mine", out);
            else ++bad;
            list += name + "\t" + ulz::error_name(err) + "\n";
            ++n;
        });
    }
    file::write_text(dir + "/list.tsv", list);
    std::printf("%d ULZ streams written to %s (%d failed to decode here)\n", n, dir.c_str(), bad);
    return 0;
}

int cmd_verify(AceDiscs& discs) {
    Bytes data;
    u32 records_ok = 0, nodes = 0, ulz_ok = 0, ulz_bad = 0, tims = 0, tims_bad = 0;
    for (u32 i = 0; i < discs.base_count(); ++i) {
        if (!discs.read_base(i, data)) {
            std::printf("  record %u: unreadable\n", i);
            continue;
        }
        ++records_ok;
        walk_tree(data, std::to_string(i), [&](const Node& n, ByteView b) {
            ++nodes;
            if (n.kind == NodeKind::ulz) ++ulz_ok;
            if (n.kind == NodeKind::ulz_error) {
                ++ulz_bad;
                Bytes out;
                std::printf("  %s: ULZ %s\n", n.path.c_str(), ulz::error_name(ulz::decode(b, out)));
            }
            if (n.kind == NodeKind::tim) {
                ++tims;
                if (!assets::parse_tim(b)) ++tims_bad;
            }
        });
    }
    u32 streams_ok = 0, streams_missing = 0, blocks_bad = 0;
    for (u32 i = 0; i < discs.stream_count(); ++i) {
        const StreamRef r = discs.stream(i);
        if (!r.available) {
            ++streams_missing;
            continue;
        }
        Bytes block;
        const Census c = census(discs, r);
        if (c.unreadable || (r.load_size && !discs.read_stream_block(i, block))) ++blocks_bad;
        else ++streams_ok;
    }
    std::printf("records  %u/%u read and walked (%u nodes)\n", records_ok, discs.base_count(), nodes);
    std::printf("ULZ      %u decoded, %u failed\n", ulz_ok, ulz_bad);
    std::printf("TIM      %u parsed, %u failed\n", tims - tims_bad, tims_bad);
    std::printf("streams  %u/%u read (%u not on the mounted discs, %u unreadable)\n", streams_ok,
                discs.stream_count(), streams_missing, blocks_bad);
    const bool pass = records_ok == discs.base_count() && ulz_bad == 0 && tims_bad == 0 &&
                      streams_ok == discs.stream_count();
    std::printf("%s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
}

int main(int argc, char** argv) {
    std::string disc, english;
    bool one_disc = false;
    std::vector<std::string> args;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--disc") == 0 && i + 1 < argc) disc = argv[++i];
        else if (std::strcmp(argv[i], "--one-disc") == 0) one_disc = true;
        else if (std::strcmp(argv[i], "--english") == 0 && i + 1 < argc) english = argv[++i];
        else args.emplace_back(argv[i]);
    }
    if (disc.empty() || args.empty()) return usage();
    const std::string& cmd = args[0];

    if (cmd == "english-check" && !english.empty()) {
        DiscImage img;
        if (!img.open(disc)) return std::fprintf(stderr, "ac3cat: cannot open %s\n", disc.c_str()), 1;
        std::string note;
        if (!ac3::vfs::translate(img, {english}, note)) return std::fprintf(stderr, "ac3cat: %s\n", note.c_str()), 1;
        std::printf("%s\n", note.c_str());
        const std::string id = ac3::vfs::iso_volume_id(img);
        const int number = !id.empty() && id.back() == '2' ? 2 : 1;
        std::string error;
        const auto built = ac3::vfs::Translation::build(disc, ac3::vfs::Translation::find_patch(number, {english}), &error);
        if (!built) return std::fprintf(stderr, "ac3cat: %s\n", error.c_str()), 1;
        const bool same = built->same(*img.translation());
        std::printf("built from the patch: %u sectors, %u new -- %s\n", built->sectors(), built->literals(),
                    same ? "the same as the cache's" : "NOT the cache's");
        return same ? 0 : 1;
    }

    AceDiscs discs;
    if (!english.empty()) discs.translation_dirs = {english};
    if (!discs.mount(disc, {}, one_disc)) {
        std::fprintf(stderr, "ac3cat: %s\n", discs.error().c_str());
        return 1;
    }
    auto arg_u32 = [&](size_t k) { return k < args.size() ? static_cast<u32>(std::strtoul(args[k].c_str(), nullptr, 0)) : 0u; };
    if (cmd == "file" && args.size() >= 3) {
        const auto e = ac3::vfs::iso_find(discs.image(0), args[1]);
        ac3::Bytes data;
        if (!e || !discs.image(0).read_bytes(e->lba, e->size, data) || !ac3::file::write_all(args[2], data))
            return std::fprintf(stderr, "ac3cat: cannot read %s\n", args[1].c_str()), 1;
        std::printf("%s: %u bytes from sector %u\n", args[1].c_str(), e->size, e->lba);
        return 0;
    }
    if (cmd == "raw" && args.size() >= 2) {
        ac3::file::make_dirs(args[1]);
        ac3::Bytes data;
        u32 written = 0;
        for (u32 i = 0; i < discs.base_count(); ++i) {
            char name[32];
            std::snprintf(name, sizeof name, "/%04u.bin", i);
            if (discs.read_base(i, data) && ac3::file::write_all(args[1] + name, data)) ++written;
        }
        std::printf("%u of %u records written to %s\n", written, discs.base_count(), args[1].c_str());
        return 0;
    }
    if (cmd == "info") return cmd_info(discs);
    if (cmd == "records") return cmd_records(discs);
    if (cmd == "tree" && args.size() >= 2) return cmd_tree(discs, arg_u32(1));
    if (cmd == "streams") return cmd_streams(discs);
    if (cmd == "export" && args.size() >= 3) return cmd_export(discs, arg_u32(1), args[2]);
    if (cmd == "catalog" && args.size() >= 2) return cmd_catalog(discs, args[1]);
    if (cmd == "dump-ulz" && args.size() >= 2) return cmd_dump_ulz(discs, args[1]);
    if (cmd == "node" && args.size() >= 3) return cmd_node(discs, args[1], args[2]);
    if (cmd == "mission" && args.size() >= 3) return cmd_mission(discs, static_cast<int>(arg_u32(1)), args[2]);
    if (cmd == "vram-compare" && args.size() >= 3) return cmd_vram_compare(discs, static_cast<int>(arg_u32(1)), args[2]);
    if (cmd == "verify") return cmd_verify(discs);
    return usage();
}
