#pragma once

#include "assets/tim.hpp"
#include "assets/vram.hpp"

#include <array>
#include <optional>
#include <string>
#include <vector>

namespace ac3::vfs {
class AceDiscs;
}

namespace ac3::assets {
constexpr int kMissionCount = 61;
constexpr u32 kFirstMissionRecord = 5;

struct TilePoly {
    u8 idx[4];
    u8 u[4], v[4];
    u16 clut, tpage;
    u32 rgb[4];
    bool gouraud = false;
    u8 head[16];
};

struct TileVertex {
    s16 x, y, z;
};

struct TileType {
    s16 base = 0;
    u32 head = 0;
    std::vector<TileVertex> verts;
    std::vector<TilePoly> polys;
};

struct Mission {
    int index = 0;
    u32 record = 0;
    std::array<Bytes, 17> child;
    Bytes env;
    std::vector<u16> grid;
    std::vector<TileType> tiles;
    std::array<std::vector<Tim>, 5> texture_sets;
};

std::optional<Mission> load_mission(vfs::AceDiscs& discs, int m, std::string* error = nullptr);

std::optional<TileType> parse_tile(ByteView tile);

std::vector<Tim> boot_textures(vfs::AceDiscs& discs);

void upload_mission_textures(const Mission& m, Vram& vram);
}
