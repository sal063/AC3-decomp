#include "sim/game/game.hpp"
#include "sim/precise.hpp"
#include "sim/game/globals.hpp"
#include <array>
#include <cmath>

namespace ac3::sim {
namespace {
constexpr u32 kScratch = 0x1F800000u;
constexpr u32 kTileOriginX = 0x800BD068u, kTileOriginZ = 0x800BD06Cu;
constexpr u32 kEndTiles = 0x00200000u, kFirstInRow = 0x00400000u;
}

u32 Game::terrain_tile_list(u32 grid, u32 outline, u32 output) {
    cover(0x8005E4CCu);
    Frame frame(*this, 0x38);
    m.w32(frame.at(0x38), grid);
    const u32 fractionX = m.r32(outline + 64) - 1024u, fractionZ = m.r32(outline + 68) - 1024u;
    u32 z = m.r32(outline + 76);
    const u32 height = m.r32(outline + 4);
    m.w32(frame.at(8), m.r32(outline + 56));
    m.w32(frame.at(0), m.r32(outline + 60));
    u32 previousMask = 0;
    auto border = [](u32 index) {
        if (static_cast<s16>(index) < 0) index &= 7u;
        if (static_cast<s16>(index) >= 256) index = (index & 7u) | 248u;
        return static_cast<s16>(index);
    };
    for (s32 row = 0; row < m.r32s(outline + 92); ++row, ++z) {
        u32 x = m.r32(outline + 72), currentMask = 0;
        bool first = true;
        const u32 gridZ = m.r32(frame.at(0)) + z;
        for (s32 column = 0; column < m.r32s(outline + 88); ++column, ++x) {
            const u32 bit = 1u << (static_cast<u32>(column) & 31u);
            if (!(m.r32(outline + 96 + 4u * static_cast<u32>(row)) & bit)) continue;
            const s32 tileX = border(m.r32(frame.at(8)) + x), tileZ = border(gridZ);
            const u32 address = m.r32(frame.at(0x38)) + static_cast<u32>(512 * tileZ + 2 * tileX);
            const u32 tile = static_cast<u32>(m.r16s(address));
            if (!(tile & 511u)) continue;
            currentMask |= bit;
            u32 seams = kFirstInRow;
            if (!first) {
                const u32 pair = previousMask >> ((static_cast<u32>(column) - 1u) & 31u);
                seams = (pair & 3u) == 3u ? 0x0E000000u : 0x08000000u;
            }
            first = false;
            m.w16(output, (x << 11) - fractionX);
            m.w16(output + 2, 0u - height);
            m.w16(output + 4, (z << 11) - fractionZ);
            m.w32(output + 8, tile | (static_cast<u32>(column) << 16) | seams);
            output += 12;
        }
        previousMask = currentMask;
    }
    m.w32(output + 8, kEndTiles);
    return output + 12;
}

u32 Game::terrain_draw_tiles(u32 tiles, u32 models, u32 work) {
    cover(0x80061768u);
    Frame frame(*this, 0x28);
    for (u32 channel = 21; channel <= 23; ++channel) gte.ctc2(channel, 2096);
    terrain_row_ = 0;
    const bool exact = gte.precise && player_settings;
    for (u32 tile = tiles;; tile += 12) {
        const u32 flags = m.r32(tile + 8);
        if (flags & kEndTiles) return 0;
        const u32 model = m.r32(models + 4 * (flags & 511u));
        const std::array<u32, 3> translation = {m.r32(work + 20), m.r32(work + 24), m.r32(work + 28)};
        for (u32 axis = 0; axis < 3; ++axis) gte.ctc2(5 + axis, translation[axis]);
        gte.mtc2(0, m.r32(tile)); gte.mtc2(1, m.r32(tile + 4));
        if (flags & kFirstInRow) {
            const u32 previous = m.r32(work + 12), current = m.r32(work + 16);
            m.w32(work + 16, previous); m.w32(work + 12, current);
            terrain_row_ ^= 1u;
        }
        gte.op(0x480012);
        if (exact) {
            const s64 v[3] = {m.r16s(tile), m.r16s(tile + 2), m.r16s(tile + 4)};
            const u32 r[5] = {gte.cfc2(0), gte.cfc2(1), gte.cfc2(2), gte.cfc2(3), gte.cfc2(4)};
            const s64 rt[3][3] = {{static_cast<s16>(r[0]), static_cast<s16>(r[0] >> 16), static_cast<s16>(r[1])},
                                  {static_cast<s16>(r[1] >> 16), static_cast<s16>(r[2]), static_cast<s16>(r[2] >> 16)},
                                  {static_cast<s16>(r[3]), static_cast<s16>(r[3] >> 16), static_cast<s16>(r[4])}};
            for (u32 i = 0; i < 3; ++i) {
                const s64 mac = static_cast<s64>(static_cast<s32>(translation[i])) * 4096 + rt[i][0] * v[0] +
                                rt[i][1] * v[1] + rt[i][2] * v[2];
                terrain_delta_[i] = static_cast<double>(mac) / 4096.0 - static_cast<double>(static_cast<s32>(gte.mfc2(25 + i)));
            }
        }
        const u32 stream = m.r32(model + 4), column = (flags >> 16) & 31u;
        const u32 sharedScreen = m.r32(work + 80);
        const u32 originX = gte.mfc2(25), originY = gte.mfc2(26), originZ = gte.mfc2(27);
        gte.ctc2(5, originX); gte.ctc2(6, originY); gte.ctc2(7, originZ);
        if (static_cast<s32>(originZ) >= terrain_near_cut_) {
            gte.mtc2(0, m.r32(stream + 24)); gte.mtc2(1, m.r32(stream + 36));
            gte.op(0x180001);
            m.w32(work + 80, gte.mfc2(14));
            m.w32(m.r32(work + 16) + 4 * column, m.r32(work + 80));
            if (exact) {
                terrain_shared_exact_ = terrain_exact(gte.exact_fifo[2]);
                terrain_rows_exact_[terrain_row_][column] = terrain_shared_exact_;
            }
            continue;
        }
        terrain_project_vertices(stream, work);
        const u32 seams = flags ^ (flags & (flags << 16));
        if (seams & 0x08000000u) m.w32(work + 64, sharedScreen);
        if (seams & 0x02000000u) m.w32(work + 32, m.r32(m.r32(work + 12) + 4 * column - 4));
        if (seams & 0x04000000u) m.w32(work + 48, m.r32(m.r32(work + 12) + 4 * column));
        m.w32(m.r32(work + 16) + 4 * column, m.r32(work + 80));
        if (exact) {
            const Gte::Exact* previous = terrain_rows_exact_[terrain_row_ ^ 1u];
            if (seams & 0x08000000u) terrain_exact_[2] = terrain_shared_exact_;
            if ((seams & 0x02000000u) && column) terrain_exact_[0] = previous[column - 1];
            if (seams & 0x04000000u) terrain_exact_[1] = previous[column];
            terrain_rows_exact_[terrain_row_][column] = terrain_exact_[3];
            terrain_shared_exact_ = terrain_exact_[3];
        }
        m.w32(work + 560, m.r32(model + 4) + 4);
        terrain_tiles_ = true;
        m.w32(work + 8, m.r32(model + 16)); terrain_gouraud_polygons(work);
        m.w32(work + 8, m.r32(model + 12)); terrain_flat_polygons(work);
        terrain_tiles_ = false;
    }
}

u32 Game::terrain_draw(u32 map, u32 outline, u32 orderTable, u32 packets) {
    cover(0x8005E6DCu);
    Frame frame(*this, 0x128);
    water_frame();
    m.w32(kScratch + 4, terrain_tile_list(m.r32(map + 4), outline, packets));
    m.w32(kScratch + 12, frame.at(0x10));
    m.w32(kScratch + 16, frame.at(0x90));
    m.w32(kScratch + 20, m.r32(kTileOriginX) + 31744u);
    m.w32(kScratch, orderTable);
    m.w32(kScratch + 28, m.r32(kTileOriginZ) + 31744u);
    m.w32(kScratch + 24, m.r32(m.r32(g::kWorld) + 100));
    apply_rot_matrix_lv(kScratch + 20, kScratch + 20);
    terrain_draw_tiles(packets, m.r32(map + 8), kScratch);
    m.w8(m.r32(kScratch + 4) + 3, 1);
    m.w32(m.r32(kScratch + 4) + 4, 0xE1000228u);
    m.w32(m.r32(kScratch + 4), m.r32(orderTable + 16376) | 0x01000000u);
    m.w32(orderTable + 16376, m.r32(kScratch + 4) & 0xFFFFFFu);
    const u32 end = m.r32(kScratch + 4) + 8;
    m.w32(kScratch + 4, end);
    return end;
}
void Game::water_frame() {
    if (!gte.precise || !player_settings) return;
    PreciseCache::Water& w = gte.precise->water;
    w.valid = false;
    const u32 world = m.r32(g::kWorld);
    const u32 view = m.r32(world + 40), haze = m.r32(world + 580);
    if (!world || !view) return;
    for (u32 i = 0; i < 9; ++i) w.rot[i / 3][i % 3] = static_cast<float>(m.r16s(world + 76 + 2 * i) / 4096.0);
    for (u32 i = 0; i < 3; ++i) w.eye[i] = m.r32s(view + 4 + 4 * i) / 4096.0;
    double sun[3], length = 0;
    for (u32 i = 0; i < 3; ++i) {
        sun[i] = m.r16s(world + 108 + 2 * i) / 4096.0;
        length += sun[i] * sun[i];
    }
    length = length > 0 ? 1.0 / std::sqrt(length) : 0.0;
    for (u32 i = 0; i < 3; ++i) {
        w.sun[i] = static_cast<float>(sun[i] * length);
        w.sun_colour[i] = static_cast<float>(2.0 * m.r16s(world + 140 + 2 * (3 * i)) / 4096.0);
        w.back[i] = static_cast<float>(m.r32s(world + 172 + 4 * i) / 128.0);
        w.horizon[i] = haze ? static_cast<float>(m.r8(haze + 8 + i) / 255.0) : 0.5f;
    }
    w.time = static_cast<float>((m.r32(m.r32(g::kScene) + 40) - (1.0 - frame_fraction_)) / 30.0);
    if (w.sky_age < 255) ++w.sky_age;
    w.valid = true;
    water_shore();
}
}
