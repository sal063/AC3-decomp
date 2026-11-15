#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/precise.hpp"
#include <algorithm>
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kProjectedVertices = 32;
constexpr u32 kSourceVertices = 560;
constexpr u32 kFlatAsGouraud = 564;
constexpr u32 kOrderDepth = 612;
constexpr u32 kTexturePage = 616;
constexpr u32 kPalette = 618;
constexpr u32 kPatch = 624;
constexpr u32 kPatchBytes = 72;
constexpr u32 kVertexBytes = 12;
constexpr u32 kScreens = 48;
constexpr u32 kDepths = 64;
constexpr s32 kSubdivideDepth = 911;
}

Gte::Exact Game::terrain_exact(const Gte::Exact& e) const {
    if (!e.ok || (terrain_delta_[0] == 0.0 && terrain_delta_[1] == 0.0 && terrain_delta_[2] == 0.0)) return e;
    const double view[3] = {e.vx + terrain_delta_[0], e.vy + terrain_delta_[1], e.vz + terrain_delta_[2]};
    PreciseVertex p;
    Gte::Exact out;
    out.ok = gte.project_exact(view, p);
    out.sx = p.sx;
    out.sy = p.sy;
    out.vx = p.vx;
    out.vy = p.vy;
    out.vz = p.vz;
    return out;
}

void Game::terrain_record(u32 packet, const Gte::Exact* const* corners, u32 n) {
    PreciseCache::Packet kept;
    for (u32 i = 0; i < n; ++i) {
        if (!corners[i]->ok) return;
        const Gte::Exact& e = *corners[i];
        kept.v[i] = PreciseVertex{e.sx, e.sy, e.vx, e.vy, e.vz};
    }
    kept.ok = static_cast<u8>((1u << n) - 1u);
    kept.fx = terrain_kind_ == 1 ? 6 : 2;
    gte.precise->record_packet(packet, kept);
}

u32 Game::terrain_patch(u32 patch, u32 work, s32 levels, u32 orientation) {
    cover(0x80061FA8u);
    Frame f(*this, 0x38);
    for (u32 i = 0; i < 3; ++i) {
        gte.mtc2(2 * i, m.r32(patch + kVertexBytes * i));
        gte.mtc2(2 * i + 1, m.r32(patch + kVertexBytes * i + 4));
    }
    gte.op(0x280030);
    const u32 level = std::min<u32>((patch - (work + kPatch)) / kPatchBytes, 7u);
    if (terrain_exact_on_)
        for (u32 i = 0; i < 3; ++i) patch_exact_[level][i] = terrain_exact(gte.exact_fifo[i]);
    const u32 packet = m.r32(work + 4), depthSlot = m.r32(work + kOrderDepth);
    m.w16(packet + 26, m.r16(work + kTexturePage));
    m.w16(packet + 14, m.r16(work + kPalette));
    for (u32 i = 0; i < 3; ++i) m.w32(patch + kScreens + 4 * i, gte.mfc2(12 + i));

    u32 outside = 0;
    bool inside = false;
    for (u32 i = 0; i < 4; ++i) {
        const s32 x = m.r16s(patch + kScreens + 4 * i), y = m.r16s(patch + kScreens + 4 * i + 2);
        u32 code = x < -160 ? 1u : x > 160 ? 16u : 0u;
        code |= y < -120 ? 256u : y > 120 ? 4096u : 0u;
        outside |= code << i;
        if (!code) { inside = true; break; }
    }
    if (!inside) {
        for (u32 shift : {0u, 4u, 8u, 12u}) if (((outside >> shift) & 15u) == 15u) return 0;
    }
    for (u32 i = 0; i < 3; ++i) m.w16(patch + kDepths + 2 * i, gte.mfc2(17 + i));
    gte.op(0x1400006);
    s32 farthest = m.r16s(patch + kDepths);
    for (u32 i = 1; i < 4; ++i) farthest = std::max(farthest, static_cast<s32>(m.r16s(patch + kDepths + 2 * i)));
    if (farthest < 31) return 0;
    s32 firstArea = static_cast<s32>(gte.mfc2(24));
    gte.mtc2(15, m.r32(patch + kScreens + 12));
    if (firstArea < 0) firstArea = wrap::neg(firstArea);
    gte.op(0x1400006);
    s32 secondArea = static_cast<s32>(gte.mfc2(24));
    if (secondArea < 0) secondArea = wrap::neg(secondArea);
    const s32 area = wrap::add(firstArea, secondArea);
    if (area <= 81918 && levels >= 4) levels = 3;
    if (area < 28735 && levels >= 3) levels = 2;
    if (area < 12415 && levels >= 2) levels = 1;
    if (area < 5375 && levels > 0) levels = 0;
    const s32 nextLevel = wrap::sub(levels, 1);
    if (nextLevel < 0) {
        const std::array<u32, 4> order = orientation & 1u ? std::array<u32, 4>{1, 3, 0, 2} : std::array<u32, 4>{0, 1, 2, 3};
        std::array<u32, 4> colours, uv, screens;
        for (u32 i = 0; i < 4; ++i) {
            colours[i] = m.r32(patch + kVertexBytes * order[i] + 8);
            uv[i] = m.r16(patch + kVertexBytes * order[i] + 6);
            screens[i] = m.r32(patch + kScreens + 4 * order[i]);
        }
        for (u32 i = 0; i < 4; ++i) m.w32(packet + 4 + 12 * i, colours[i]);
        for (u32 i = 0; i < 4; ++i) m.w16(packet + 12 + 12 * i, uv[i]);
        for (u32 i = 0; i < 4; ++i) m.w32(packet + 8 + 12 * i, screens[i]);
        m.w32(packet, m.r32(m.r32(work) + 4 * depthSlot) | 0x0C000000u);
        m.w32(m.r32(work) + 4 * depthSlot, packet & 0xFFFFFFu);
        m.w32(work + 4, m.r32(work + 4) + 52);
        if (terrain_exact_on_) {
            const Gte::Exact* corners[4];
            for (u32 i = 0; i < 4; ++i) corners[i] = &patch_exact_[level][order[i]];
            terrain_record(packet, corners, 4);
        }
        return 1;
    }

    const u32 child = patch + kPatchBytes;
    vertex_midpoint(child, patch + ((orientation & 1u) ? 0u : 12u), patch + ((orientation & 1u) ? 36u : 24u));
    vertex_midpoint(child + 12, patch, patch + 24);
    vertex_midpoint(child + 24, patch, patch + 12);
    constexpr std::array<u32, 4> corners = {0, 1, 3, 2};
    constexpr std::array<u32, 4> turns = {2, 3, 0, 1};
    for (u32 i = 0; i < 4; ++i) {
        if (i) {
            const u32 x = m.r32(child + 24), y = m.r32(child + 28), z = m.r32(child + 32);
            m.w32(child + 12, x); m.w32(child + 16, y); m.w32(child + 20, z);
            const u32 a = patch + kVertexBytes * corners[i], b = patch + kVertexBytes * corners[(i + 1) & 3u];
            vertex_midpoint(child + 24, i == 3 ? b : a, i == 3 ? a : b);
        }
        const u32 corner = corners[i];
        for (u32 offset : {0u, 4u, 8u}) m.w32(child + 36 + offset, m.r32(patch + kVertexBytes * corner + offset));
        m.w32(child + 60, m.r32(patch + kScreens + 4 * corner));
        m.w16(child + 70, m.r16(patch + kDepths + 2 * corner));
        if (terrain_exact_on_ && level + 1 < 8) patch_exact_[level + 1][3] = patch_exact_[level][corner];
        if (terrain_patch(child, work, nextLevel, orientation + turns[i])) {
            const u32 a = patch + kVertexBytes * corners[i], b = patch + kVertexBytes * corners[(i + 1) & 3u];
            terrain_triangle(a, b, child + 24, work);
        }
    }
    return 1;
}

u32 Game::terrain_polygon_patch(u32 polygon, u32 work) {
    cover(0x8006258Cu);
    Frame f(*this, 0x18);
    m.w16(work + kTexturePage, m.r16(polygon + 26));
    m.w16(work + kPalette, m.r16(polygon + 22));
    gte.mtc2(6, m.r32(polygon + 32));
    for (u32 i = 0; i < 3; ++i) gte.mtc2(20 + i, m.r32(polygon + 36 + 4 * i));
    const u32 vertices = m.r32(work + kSourceVertices), patch = work + kPatch;
    const u32 lastPair = m.r32(polygon + 12);
    const u32 xy2 = m.r32(vertices + (lastPair & 255u));
    const u32 z2 = m.r16(vertices + ((lastPair >> 8) & 255u));
    m.w16(patch + 28, z2);
    m.w32(patch + 24, xy2);
    const u32 xy3 = m.r32(vertices + ((lastPair >> 16) & 255u));
    const u32 z3 = m.r16(vertices + (lastPair >> 24));
    const u32 firstPair = m.r32(polygon + 8);
    gte.mtc2(0, xy3); gte.mtc2(1, z3);
    m.w32(patch + 36, xy3); m.w16(patch + 40, z3);
    gte.op(0x180001);
    if (terrain_exact_on_) patch_exact_[0][3] = terrain_exact(gte.exact_fifo[2]);
    const u32 xy0 = m.r32(vertices + (firstPair & 255u));
    const u32 z0 = m.r16(vertices + ((firstPair >> 8) & 255u));
    m.w32(patch, xy0); m.w16(patch + 4, z0);
    gte.op(0xF8002A);
    const u32 xy1 = m.r32(vertices + ((firstPair >> 16) & 255u));
    const u32 z1 = m.r16(vertices + (firstPair >> 24));
    for (u32 i = 0; i < 3; ++i) m.w32(patch + 20 + 12 * i, gte.mfc2(20 + i));
    m.w32(patch + 12, xy1); m.w16(patch + 16, z1);
    gte.op(0x780010);
    const u32 lastUv = m.r32(polygon + 28), uv0 = m.r16(polygon + 20), uv1 = m.r16(polygon + 24);
    m.w16(patch + 30, lastUv);
    m.w16(patch + 6, uv0); m.w16(patch + 18, uv1); m.w16(patch + 42, lastUv >> 16);
    m.w32(patch + 8, gte.mfc2(22));
    m.w32(patch + 60, gte.mfc2(14));
    m.w16(patch + 70, gte.mfc2(19));
    terrain_patch(patch, work, 4, 0);
    return m.r32(work + 4);
}

u32 Game::terrain_polygon_list(u32 work, bool gouraud) {
    const u32 list = m.r32(work + 8), ot = m.r32(work);
    const s32 count = m.r32s(list);
    u32 cursor = m.r32(work + 4);
    const u32 stride = gouraud ? 48u : 36u;
    for (s32 i = 0; i < count; ++i) {
        const u32 polygon = list + 4 + stride * static_cast<u32>(i);
        const u32 indices = m.r32(polygon + 16);
        terrain_kind_ = terrain_tiles_ ? (m.r32(polygon) & 15u) : 0u;
        std::array<u32, 4> points;
        for (u32 k = 0; k < 4; ++k) points[k] = work + kProjectedVertices + 16 * ((indices >> (8 * k)) & 255u);
        const Gte::Exact* exact[4] = {};
        bool facing = false;
        if (terrain_exact_on_) {
            bool known = true;
            for (u32 k = 0; k < 4; ++k) {
                const u32 index = (indices >> (8 * k)) & 255u;
                exact[k] = index < 64 ? &terrain_exact_[index] : nullptr;
                known = known && exact[k] && exact[k]->ok;
            }
            if (known) {
                auto nclip = [&](u32 a, u32 b, u32 c) {
                    const Gte::Exact &p = *exact[a], &q = *exact[b], &r = *exact[c];
                    return static_cast<double>(p.sx) * (q.sy - r.sy) + static_cast<double>(q.sx) * (r.sy - p.sy) +
                           static_cast<double>(r.sx) * (p.sy - q.sy);
                };
                facing = nclip(0, 1, 2) > 0.0 || nclip(3, 1, 2) < 0.0;
            } else {
                exact[0] = nullptr;
            }
        }
        const u32 screen0 = m.r32(points[0]), screen3 = m.r32(points[3]);
        gte.mtc2(13, m.r32(points[1])); gte.mtc2(14, m.r32(points[2])); gte.mtc2(12, screen0);
        gte.op(0x1400006);
        s32 depth = m.r32s(points[0] + 4);
        for (u32 k = 1; k < 4; ++k) depth = std::max(depth, m.r32s(points[k] + 4));
        const s32 area = static_cast<s32>(gte.mfc2(24));
        gte.mtc2(12, screen3);
        if (!facing && area <= 0) { gte.op(0x1400006); if (static_cast<s32>(gte.mfc2(24)) >= 0) continue; }
        if (static_cast<u32>(depth) - 121u >= 0x777Fu) continue;
        const s32 slot = depth >> 3;
        if (slot < kSubdivideDepth) {
            m.w32(work + kOrderDepth, static_cast<u32>(slot));
            u32 source = polygon;
            if (!gouraud) {
                source = work + kFlatAsGouraud;
                m.w32(source, m.r32(polygon));
                const u32 xy = m.r32(polygon + 8), z = m.r32(polygon + 12);
                m.w32(source + 8, xy); m.w32(source + 12, z);
                const u32 uv0 = m.r32(polygon + 20), uv1 = m.r32(polygon + 24), uv2 = m.r32(polygon + 28);
                m.w32(source + 28, uv2); m.w32(source + 20, uv0); m.w32(source + 24, uv1);
                const u32 colour = (m.r32(polygon + 32) & 0xFFFFFFu) | 0x3C000000u;
                for (u32 k = 0; k < 4; ++k) m.w32(source + 32 + 4 * k, colour);
            }
            m.w32(work + 4, cursor);
            cursor = terrain_polygon_patch(source, work);
            continue;
        }
        gte.mtc2(8, m.r32(points[0] + 8));
        gte.mtc2(6, m.r32(polygon + 32));
        if (gouraud) for (u32 k = 0; k < 3; ++k) gte.mtc2(20 + k, m.r32(polygon + 36 + 4 * k));
        m.w32(cursor + 8, screen0);
        if (gouraud) gte.op(0xF8002A);
        m.w32(cursor, m.r32(ot + 4 * static_cast<u32>(slot)) | (gouraud ? 0x0C000000u : 0x09000000u));
        m.w32(ot + 4 * static_cast<u32>(slot), cursor & 0xFFFFFFu);
        if (!gouraud) gte.op(0x780010);
        const u32 lastUv = m.r32(polygon + 28), firstUv = m.r32(polygon + 20), secondUv = m.r32(polygon + 24);
        if (gouraud) {
            for (u32 k = 0; k < 3; ++k) m.w32(cursor + 16 + 12 * k, gte.mfc2(20 + k));
            m.w32(cursor + 12, firstUv); m.w32(cursor + 24, secondUv);
            gte.op(0x780010);
            m.w32(cursor + 36, lastUv); m.w32(cursor + 48, lastUv >> 16);
        } else {
            m.w32(cursor + 28, lastUv); m.w32(cursor + 12, firstUv);
            m.w32(cursor + 20, secondUv); m.w32(cursor + 36, lastUv >> 16);
        }
        m.w32(cursor + 4, gte.mfc2(22));
        m.w32(cursor + (gouraud ? 20u : 16u), gte.mfc2(13));
        m.w32(cursor + (gouraud ? 32u : 24u), gte.mfc2(14));
        m.w32(cursor + (gouraud ? 44u : 32u), gte.mfc2(12));
        if (exact[0]) terrain_record(cursor, exact, 4);
        cursor += gouraud ? 52u : 40u;
    }
    m.w32(work + 4, cursor);
    return count > 0 ? 0u : list;
}

u32 Game::terrain_flat_polygons(u32 work) {
    cover(0x800619E0u);
    Frame f(*this, 0x38);
    return terrain_polygon_list(work, false);
}

u32 Game::terrain_gouraud_polygons(u32 work) {
    cover(0x80061C3Cu);
    Frame f(*this, 0x30);
    return terrain_polygon_list(work, true);
}
}
