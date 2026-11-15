#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include "sim/precise.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace ac3::sim {
namespace {
constexpr u32 kRegion = 0x80000000u | Memory::kExtBase;
constexpr u32 kRegionBytes = Memory::kExtSize / 2;
constexpr u32 kOtEntries = 4096, kOtBytes = 4 * kOtEntries;
constexpr u32 kRings = 3;
constexpr s32 kCut = 28673;
constexpr u32 kWorldSlot = 4093;
constexpr u32 kWorldDepth = 4093 * 8;
constexpr s32 kTile = 2048;
constexpr double kTileReach = 2200.0;
constexpr s32 kTileMiddle = 1500;
constexpr u32 kGroundSlots = 0x800BCE4Cu;
constexpr s32 kModelReach = 139288;
constexpr s32 kFarColour = 2096;

s32 border(s32 index) {
    if (static_cast<s16>(index) < 0) index &= 7;
    if (static_cast<s16>(index) >= 256) index = (index & 7) | 248;
    return static_cast<s16>(index);
}
s32 floor_div(s32 a, s32 b) { return a >= 0 ? a / b : -((-a + b - 1) / b); }

struct FarMesh {
    std::vector<std::array<s16, 3>> vertices;
    struct Polygon {
        u8 index[4];
        bool gouraud;
        u32 uv0, uv1, uv23;
        u32 rgbc;
        u32 colour[3];
        u8 kind;
    };
    std::vector<Polygon> polygons;
};

struct FarMeshes {
    u32 key[3] = {};
    std::unordered_map<u32, FarMesh> by_model;
} g_meshes;

const FarMesh& mesh_of(const Memory& m, u32 model) {
    auto it = g_meshes.by_model.find(model);
    if (it != g_meshes.by_model.end()) return it->second;
    FarMesh mesh;
    const u32 stream = m.r32(model + 4);
    const s32 count = std::clamp(m.r32s(stream), 1, 255);
    for (s32 g = 0; 3 * g < count; ++g) {
        const u32 at = stream + 4 + 20 * static_cast<u32>(g);
        const u32 w0 = m.r32(at), w1 = m.r32(at + 4), w2 = m.r32(at + 8), w3 = m.r32(at + 12), w4 = m.r32(at + 16);
        mesh.vertices.push_back({static_cast<s16>(w0), static_cast<s16>(w0 >> 16), static_cast<s16>(w3)});
        mesh.vertices.push_back({static_cast<s16>(w1), static_cast<s16>(w1 >> 16), static_cast<s16>(w3 >> 16)});
        mesh.vertices.push_back({static_cast<s16>(w2), static_cast<s16>(w2 >> 16), static_cast<s16>(w4)});
    }
    for (const bool gouraud : {true, false}) {
        const u32 list = m.r32(model + (gouraud ? 16 : 12)), stride = gouraud ? 48u : 36u;
        const s32 n = m.r32s(list);
        for (s32 i = 0; i < n && i < 4096; ++i) {
            const u32 p = list + 4 + stride * static_cast<u32>(i);
            FarMesh::Polygon poly{};
            const u32 indices = m.r32(p + 16);
            for (u32 k = 0; k < 4; ++k) poly.index[k] = static_cast<u8>(indices >> (8 * k));
            poly.gouraud = gouraud;
            poly.uv0 = m.r32(p + 20);
            poly.uv1 = m.r32(p + 24);
            poly.uv23 = m.r32(p + 28);
            poly.rgbc = m.r32(p + 32);
            poly.kind = static_cast<u8>(m.r32(p) & 15u);
            if (gouraud)
                for (u32 k = 0; k < 3; ++k) poly.colour[k] = m.r32(p + 36 + 4 * k);
            mesh.polygons.push_back(poly);
        }
    }
    return g_meshes.by_model.emplace(model, std::move(mesh)).first->second;
}

void sync_meshes(const u32 key[3]) {
    if (std::equal(key, key + 3, g_meshes.key)) return;
    g_meshes.by_model.clear();
    std::copy(key, key + 3, g_meshes.key);
}

struct FarUnit {
    u32 model, to;
    s32 blend;
    double view[3];
};
struct FarTile {
    double depth;
    s32 tx, tz;
};
}

double Game::far_tile_depth(u32 position) {
    const u32 world = m.r32(g::kWorld), view = m.r32(world + 40);
    const s32 camX = m.r32s(view + 4) >> 12, camY = m.r32s(view + 8) >> 12, camZ = m.r32s(view + 12) >> 12;
    const s32 tx = floor_div(m.r32s(position) >> 12, kTile), tz = floor_div(m.r32s(position + 8) >> 12, kTile);
    const double rel[3] = {static_cast<double>(tx * kTile + kTile / 2 - camX), static_cast<double>(m.r32s(world + 100) - camY),
                           static_cast<double>(tz * kTile + kTile / 2 - camZ)};
    double z = 0;
    for (u32 j = 0; j < 3; ++j) z += static_cast<double>(m.r16s(world + 76 + 2 * (6 + j))) * rel[j];
    return z / 4096.0;
}

void Game::water_shore() {
    if (!gte.precise) return;
    PreciseCache::Shore& shore = gte.precise->shore;
    const u32 world = m.r32(g::kWorld), map = world + 580;
    const u32 grid = m.r32(map + 4), models = m.r32(map + 8);
    if (!grid || !models) return;
    const u32 key[3] = {grid, models, m.r32(world + 600)};
    if (shore.generation && std::equal(key, key + 3, shore.key)) return;
    std::copy(key, key + 3, shore.key);
    sync_meshes(key);

    constexpr u32 C = PreciseCache::Shore::kCells, side = PreciseCache::Shore::kSide;
    constexpr double cell = static_cast<double>(kTile) / C;
    std::unordered_map<u32, u64> masks;
    auto mask_of = [&](u32 model) {
        const auto found = masks.find(model);
        if (found != masks.end()) return found->second;
        const FarMesh& mesh = mesh_of(m, model);
        u64 bits = 0;
        for (u32 cz = 0; cz < C; ++cz)
            for (u32 cx = 0; cx < C; ++cx) {
                const double px = -kTile / 2 + (cx + 0.5) * cell, pz = -kTile / 2 + (cz + 0.5) * cell;
                bool water = false, land = false;
                for (const FarMesh::Polygon& poly : mesh.polygons) {
                    double x[4], z[4];
                    bool ok = true;
                    for (u32 k = 0; k < 4 && ok; ++k) {
                        ok = poly.index[k] < mesh.vertices.size();
                        if (ok) {
                            x[k] = mesh.vertices[poly.index[k]][0];
                            z[k] = mesh.vertices[poly.index[k]][2];
                        }
                    }
                    if (!ok) continue;
                    auto inside = [&](u32 a, u32 b, u32 c) {
                        const double d0 = (x[b] - x[a]) * (pz - z[a]) - (z[b] - z[a]) * (px - x[a]);
                        const double d1 = (x[c] - x[b]) * (pz - z[b]) - (z[c] - z[b]) * (px - x[b]);
                        const double d2 = (x[a] - x[c]) * (pz - z[c]) - (z[a] - z[c]) * (px - x[c]);
                        return (d0 >= 0 && d1 >= 0 && d2 >= 0) || (d0 <= 0 && d1 <= 0 && d2 <= 0);
                    };
                    if (!inside(0, 1, 2) && !inside(1, 2, 3)) continue;
                    (poly.kind == 1 ? water : land) = true;
                }
                if (water && !land) bits |= 1ull << (cz * C + cx);
            }
        masks.emplace(model, bits);
        return bits;
    };
    std::vector<u16> far(static_cast<size_t>(side) * side);
    for (u32 tz = 0; tz < 256; ++tz)
        for (u32 tx = 0; tx < 256; ++tx) {
            const u32 index = static_cast<u32>(m.r16s(grid + 512 * tz + 2 * tx)) & 511u;
            u32 model = index ? m.r32(models + 4 * index) : 0;
            if ((model >> 24) != 0x80u || (model & 0xFFFFFFu) >= 0x200000u) model = 0;
            const u64 bits = model ? mask_of(model) : ~0ull;
            for (u32 cz = 0; cz < C; ++cz)
                for (u32 cx = 0; cx < C; ++cx)
                    far[static_cast<size_t>(tz * C + cz) * side + tx * C + cx] = (bits >> (cz * C + cx)) & 1u ? 0xFFFFu : 0u;
        }
    auto relax = [&](size_t at, s32 x, s32 z, s32 dx, s32 dz, u32 cost) {
        if (x + dx < 0 || z + dz < 0 || x + dx >= static_cast<s32>(side) || z + dz >= static_cast<s32>(side)) return;
        const u32 d = far[static_cast<size_t>(z + dz) * side + static_cast<size_t>(x + dx)] + cost;
        if (d < far[at]) far[at] = static_cast<u16>(d);
    };
    for (s32 z = 0; z < static_cast<s32>(side); ++z)
        for (s32 x = 0; x < static_cast<s32>(side); ++x) {
            const size_t at = static_cast<size_t>(z) * side + static_cast<size_t>(x);
            if (!far[at]) continue;
            relax(at, x, z, -1, 0, 3);
            relax(at, x, z, 0, -1, 3);
            relax(at, x, z, -1, -1, 4);
            relax(at, x, z, 1, -1, 4);
        }
    for (s32 z = static_cast<s32>(side) - 1; z >= 0; --z)
        for (s32 x = static_cast<s32>(side) - 1; x >= 0; --x) {
            const size_t at = static_cast<size_t>(z) * side + static_cast<size_t>(x);
            if (!far[at]) continue;
            relax(at, x, z, 1, 0, 3);
            relax(at, x, z, 0, 1, 3);
            relax(at, x, z, 1, 1, 4);
            relax(at, x, z, -1, 1, 4);
        }
    shore.cells.resize(far.size());
    for (size_t i = 0; i < far.size(); ++i) shore.cells[i] = static_cast<u8>(std::min<u32>(255, far[i] * 16u / 3u));
    ++shore.generation;
    if (!shore.generation) ++shore.generation;
}

void Game::far_terrain(u32 position, u32 rings, s32 nearCut) {
    rings = std::min(rings, kRings);
    if (!rings) return;
    const u32 world = m.r32(g::kWorld);
    const u32 map = world + 580;
    const u32 grid = m.r32(map + 4), models = m.r32(map + 8);
    m.extend();
    const u32 region = kRegion + (m.r32(m.r32(g::kScene) + 4) & 1u) * kRegionBytes;
    const u32 limit = region + kRegionBytes;
    {
        const u32 key[3] = {grid, models, m.r32(world + 600)};
        sync_meshes(key);
    }

    s32 r[3][3];
    for (u32 i = 0; i < 3; ++i)
        for (u32 j = 0; j < 3; ++j) r[i][j] = m.r16s(world + 76 + 2 * (3 * i + j));
    const s32 camX = m.r32s(position) >> 12, camY = m.r32s(position + 4) >> 12, camZ = m.r32s(position + 8) >> 12;
    const s32 lift = m.r32s(world + 100);
    const u32 view = m.r32(world + 40);
    const double h = std::max(1.0, static_cast<double>(static_cast<s32>(read_geom_screen())));
    double halfWidth = static_cast<double>(m.r16s(view + 32) >> 1), halfHeight = static_cast<double>(m.r16s(view + 34) >> 1);
    if (gte.x_squeeze > 0) halfWidth = halfWidth * 65536.0 / static_cast<double>(gte.x_squeeze);
    const double tanX = 1.1 * halfWidth / h, tanY = 1.1 * halfHeight / h;
    const double secX = std::sqrt(1.0 + tanX * tanX), secY = std::sqrt(1.0 + tanY * tanY);
    auto rotate = [&](u32 row, double x, double y, double z) {
        return (r[row][0] * x + r[row][1] * y + r[row][2] * z) / 4096.0;
    };

    const double nearest = static_cast<double>(nearCut) - 64.0;
    const double farthest = static_cast<double>(kCut << rings) - 128.0;
    const double reach = farthest + kTileReach;

    std::vector<FarTile> tiles;
    const s32 centreX = floor_div(camX, kTile) + 128, centreZ = floor_div(camZ, kTile) + 128;
    const s32 span = static_cast<s32>(reach) / kTile + 2;
    for (s32 tz = centreZ - span; tz <= centreZ + span; ++tz) {
        const double relZ = static_cast<double>((tz - 128) * kTile + kTile / 2 - camZ);
        for (s32 tx = centreX - span; tx <= centreX + span; ++tx) {
            const double relX = static_cast<double>((tx - 128) * kTile + kTile / 2 - camX);
            if (relX * relX + relZ * relZ > reach * reach) continue;
            const double relY = static_cast<double>(lift - camY);
            const double depth = rotate(2, relX, relY, relZ);
            if (depth < nearest || depth >= farthest) continue;
            const double vx = rotate(0, relX, relY - kTileMiddle, relZ);
            const double vy = rotate(1, relX, relY - kTileMiddle, relZ);
            const double vz = rotate(2, relX, relY - kTileMiddle, relZ);
            if (std::abs(vx) > vz * tanX + kTileReach * secX || std::abs(vy) > vz * tanY + kTileReach * secY) continue;
            if (!(m.r16s(grid + static_cast<u32>(512 * border(tz) + 2 * border(tx))) & 511)) continue;
            tiles.push_back({depth, tx, tz});
        }
    }
    std::sort(tiles.begin(), tiles.end(), [](const FarTile& a, const FarTile& b) { return a.depth < b.depth; });

    std::vector<FarUnit> statics;
    {
        auto add = [&](u32 at, u32 modelRef, u32 flags, u32 pose) {
            if (far_tile_depth(at) >= farthest) return;
            u32 model = m.r32(modelRef), to = 0;
            s32 blend = 0;
            if (flags & 4u) {
                const u32 frame = static_cast<u32>(std::clamp(static_cast<s32>(static_cast<s16>(pose)) >> 12, 0, 4));
                model = m.r32(modelRef + 4 * frame);
                blend = static_cast<s32>(pose & 0xFFFu);
                if (blend) to = m.r32(modelRef + 4 * std::min(frame + 1, 4u));
            }
            double rel[3];
            for (u32 i = 0; i < 3; ++i)
                rel[i] = static_cast<double>(static_cast<s32>(m.r32(at + 4 * i) - m.r32(view + 4 + 4 * i))) / 4096.0;
            FarUnit f{model, to, blend, {}};
            for (u32 i = 0; i < 3; ++i) f.view[i] = rotate(i, rel[0], rel[1], rel[2]) + far_statics_tr_[i];
            const double radius = static_cast<double>(std::max<s32>(m.r16s(model + 2), 1));
            if (f.view[2] < -radius) return;
            if (std::abs(f.view[0]) > f.view[2] * tanX + radius * secX || std::abs(f.view[1]) > f.view[2] * tanY + radius * secY) return;
            statics.push_back(f);
        };
        for (const auto& u : far_statics_) add(u.position, u.model_ref, u.flags, u.pose);
        const u32 hidden = m.r32(view + 196) - 32, table = m.r32(world + 600);
        for (u32 i = 0; i < 32; ++i) {
            const u32 entity = m.r32(m.r32(kGroundSlots) + 4 * i);
            if ((m.r32(entity) & 0x14000u) != 0x14000u || hidden == i || (m.r8(entity + 5) & 2u)) continue;
            const s32 dx = static_cast<s32>(m.r32(entity + 8) - m.r32(view + 4));
            const s32 dy = static_cast<s32>(m.r32(entity + 12) - m.r32(view + 8));
            const s32 dz = static_cast<s32>(m.r32(entity + 16) - m.r32(view + 12));
            if (std::abs(dx >> 10) + std::abs(dy >> 10) + std::abs(dz >> 10) <= kModelReach) continue;
            add(entity + 8, table + 4 * m.r8(entity + 4), m.r8(entity + 5), m.r16(entity + 6));
        }
    }

    const u32 worldOt = m.r32(world + 16) + 4, far = region;
    {
        const u32 slot = worldOt + 4 * kWorldSlot;
        m.w32(far, m.r32(slot) & 0x00FFFFFFu);
        for (u32 i = 1; i < kOtEntries; ++i) m.w32(far + 4 * i, (far + 4 * (i - 1)) & 0x00FFFFFFu);
        m.w32(slot, (m.r32(slot) & 0xFF000000u) | ((far + 4 * (kOtEntries - 1)) & 0x00FFFFFFu));
    }
    const double perEntry = std::max(1.0, (reach + 4096.0 - kWorldDepth) / kOtEntries);

    const double dqa = static_cast<double>(static_cast<s16>(gte.cfc2(27))), dqb = static_cast<double>(static_cast<s32>(gte.cfc2(28)));
    auto cue_of = [&](double z) {
        const double q = std::min(131071.0, h * 65536.0 / std::max(z, 1.0));
        return static_cast<s32>(std::clamp(std::floor((dqb + dqa * q) / 4096.0), 0.0, 4096.0));
    };
    auto cue = [&](u32 colour, s32 ir0) {
        u32 out = colour & 0xFF000000u;
        for (u32 c = 0; c < 3; ++c) {
            const s32 v = static_cast<s32>((colour >> (8 * c)) & 0xFFu) * 16;
            out |= static_cast<u32>(std::clamp((v + ir0 * (kFarColour - v) / 4096) / 16, 0, 255)) << (8 * c);
        }
        return out;
    };

    u32 cursor = region + kOtBytes;
    bool full = false;
    struct Corner {
        PreciseVertex p;
        bool ok;
        double z;
        u32 word;
    };
    std::vector<Corner> corners;

    auto draw = [&](const FarMesh& mesh, const double origin[3], const FarMesh* to, s32 blend, bool tile) {
        if (cursor + 52 * mesh.polygons.size() + 16 > limit) {
            full = true;
            return;
        }
        corners.resize(mesh.vertices.size());
        for (size_t i = 0; i < mesh.vertices.size(); ++i) {
            std::array<s16, 3> v = mesh.vertices[i];
            if (to && i < to->vertices.size())
                for (u32 k = 0; k < 3; ++k) {
                    const s32 a = v[k], b = to->vertices[i][k];
                    v[k] = static_cast<s16>(std::clamp(((a << 12) + blend * (b - a)) >> 12, -32768, 32767));
                }
            double at[3];
            for (u32 k = 0; k < 3; ++k) at[k] = origin[k] + rotate(k, v[0], v[1], v[2]);
            Corner& c = corners[i];
            c.z = at[2];
            c.ok = gte.project_exact(at, c.p);
            const s32 sx = c.ok ? std::clamp(static_cast<s32>(std::floor(c.p.sx)), -1024, 1023) : 0;
            const s32 sy = c.ok ? std::clamp(static_cast<s32>(std::floor(c.p.sy)), -1024, 1023) : 0;
            c.word = (static_cast<u32>(sx) & 0xFFFFu) | (static_cast<u32>(sy) << 16);
        }
        for (const FarMesh::Polygon& poly : mesh.polygons) {
            const Corner* k[4];
            bool ok = true;
            double depth = 0;
            for (u32 j = 0; j < 4; ++j) {
                if (poly.index[j] >= corners.size()) { ok = false; break; }
                k[j] = &corners[poly.index[j]];
                ok = ok && k[j]->ok;
                depth = std::max(depth, k[j]->z);
            }
            if (!ok || depth < 121.0) continue;
            auto turn = [](const PreciseVertex& a, const PreciseVertex& b, const PreciseVertex& c) {
                return (static_cast<double>(b.sx) - a.sx) * (static_cast<double>(c.sy) - a.sy) -
                       (static_cast<double>(b.sy) - a.sy) * (static_cast<double>(c.sx) - a.sx);
            };
            if (!(turn(k[0]->p, k[1]->p, k[2]->p) > 0.0 || turn(k[3]->p, k[1]->p, k[2]->p) < 0.0)) continue;
            const u32 truth = static_cast<u32>(depth);
            u32 table = worldOt, entry = truth >> 3;
            if (truth >= kWorldDepth) {
                table = far;
                entry = std::min<u32>(kOtEntries - 1, static_cast<u32>((depth - kWorldDepth) / perEntry));
            }
            const u32 g = poly.index[0] / 3u;
            const u32 last = std::min<u32>(3 * g + 2, static_cast<u32>(corners.size()) - 1);
            const s32 ir0 = cue_of(corners[last].z);
            const u32 code = poly.rgbc & 0xFF000000u;
            const u32 p = cursor;
            if (poly.gouraud) {
                m.w32(p + 4, cue(poly.rgbc, ir0));
                m.w32(p + 8, k[0]->word);
                m.w32(p + 12, poly.uv0);
                m.w32(p + 16, (cue(poly.colour[0], ir0) & 0xFFFFFFu) | code);
                m.w32(p + 20, k[1]->word);
                m.w32(p + 24, poly.uv1);
                m.w32(p + 28, (cue(poly.colour[1], ir0) & 0xFFFFFFu) | code);
                m.w32(p + 32, k[2]->word);
                m.w32(p + 36, poly.uv23);
                m.w32(p + 40, (cue(poly.colour[2], ir0) & 0xFFFFFFu) | code);
                m.w32(p + 44, k[3]->word);
                m.w32(p + 48, poly.uv23 >> 16);
            } else {
                m.w32(p + 4, cue(poly.rgbc, ir0));
                m.w32(p + 8, k[0]->word);
                m.w32(p + 12, poly.uv0);
                m.w32(p + 16, k[1]->word);
                m.w32(p + 20, poly.uv1);
                m.w32(p + 24, k[2]->word);
                m.w32(p + 28, poly.uv23);
                m.w32(p + 32, k[3]->word);
                m.w32(p + 36, poly.uv23 >> 16);
            }
            m.w32(p, m.r32(table + 4 * entry) | (poly.gouraud ? 0x0C000000u : 0x09000000u));
            m.w32(table + 4 * entry, p & 0x00FFFFFFu);
            if (gte.precise) {
                PreciseCache::Packet kept;
                for (u32 j = 0; j < 4; ++j) kept.v[j] = k[j]->p;
                kept.ok = 15;
                kept.fx = tile && poly.kind == 1 ? 6 : 2;
                gte.precise->record_packet(p, kept);
            }
            cursor += poly.gouraud ? 52u : 40u;
        }
    };

    for (const FarTile& t : tiles) {
        if (full) break;
        const u32 index = static_cast<u32>(m.r16s(grid + static_cast<u32>(512 * border(t.tz) + 2 * border(t.tx)))) & 511u;
        const double rel[3] = {static_cast<double>((t.tx - 128) * kTile + kTile / 2 - camX), static_cast<double>(lift - camY),
                               static_cast<double>((t.tz - 128) * kTile + kTile / 2 - camZ)};
        const double origin[3] = {rotate(0, rel[0], rel[1], rel[2]), rotate(1, rel[0], rel[1], rel[2]), rotate(2, rel[0], rel[1], rel[2])};
        draw(mesh_of(m, m.r32(models + 4 * index)), origin, nullptr, 0, true);
    }
    for (const FarUnit& f : statics) {
        if (full) break;
        const FarMesh& mesh = mesh_of(m, f.model);
        draw(mesh, f.view, f.to ? &mesh_of(m, f.to) : nullptr, f.blend, false);
    }

    if (cursor + 8 <= limit) {
        m.w32(cursor, 0x01000000u | (m.r32(far) & 0x00FFFFFFu));
        m.w32(cursor + 4, 0xE1000228u);
        m.w32(far, cursor & 0x00FFFFFFu);
    }
}
}
