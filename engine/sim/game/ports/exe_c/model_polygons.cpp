#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include "sim/precise.hpp"
#include <array>
#include <cmath>

namespace ac3::sim {
namespace {
constexpr u32 kScreens = 16;
constexpr u32 kDepths = 676;
constexpr u32 kNormals = 1008;
constexpr u32 kCenterDepth = 1014;
constexpr u32 kLocalOrderTable = 1016;
enum ColourPath : u32 { Stored, DepthCued, Lit };

void gte_matrix(const Gte& gte, u32 first, double out[3][3]) {
    s16 e[9];
    for (u32 i = 0; i < 4; ++i) {
        const u32 w = gte.cfc2(first + i);
        e[2 * i] = static_cast<s16>(w & 0xFFFFu);
        e[2 * i + 1] = static_cast<s16>(w >> 16);
    }
    e[8] = static_cast<s16>(gte.cfc2(first + 4) & 0xFFFFu);
    for (u32 i = 0; i < 9; ++i) out[i / 3][i % 3] = e[i] / 4096.0;
}
}

u32 Game::model_polygon_list(u32 work, bool quad, u32 colourPath) {
    const u32 list = m.r32(work + 8);
    u32 packet = m.r32(work + 4);
    const u32 orderTable = m.r32(work + (colourPath == Stored ? 0u : kLocalOrderTable));
    const u32 normals = colourPath == Lit ? m.r32(work + kNormals) : 0u;
    const s32 count = m.r32s(list);
    if (colourPath == Lit) gte.mtc2(6, quad ? 0x3C808080u : 0x34808080u);
    const u32 stride = colourPath == Lit ? 24u : quad ? 36u : 32u;
    int light = -1;
    double rotation[3][3] = {};
    if (colourPath == Lit && model_exact_on_) {
        double rt[3][3], llm[3][3], lcm[3][3];
        gte_matrix(gte, 0, rt);
        gte_matrix(gte, 8, llm);
        gte_matrix(gte, 16, lcm);
        double scale = 0;
        for (u32 c = 0; c < 3; ++c) scale += std::sqrt(rt[0][c] * rt[0][c] + rt[1][c] * rt[1][c] + rt[2][c] * rt[2][c]);
        scale /= 3;
        if (scale > 1e-6) {
            PreciseCache::Light l{};
            for (u32 r = 0; r < 3; ++r)
                for (u32 c = 0; c < 3; ++c) rotation[r][c] = rt[r][c] / scale;
            for (u32 k = 0; k < 3; ++k)
                for (u32 r = 0; r < 3; ++r) {
                    double d = 0;
                    for (u32 j = 0; j < 3; ++j) d += llm[k][j] * rotation[r][j];
                    l.dir[k][r] = static_cast<float>(d);
                }
            const u32 world = m.r32(g::kWorld);
            for (u32 r = 0; r < 3; ++r) {
                for (u32 k = 0; k < 3; ++k) l.colour[r][k] = static_cast<float>(2.0 * lcm[r][k]);
                l.back[r] = static_cast<float>(static_cast<s32>(gte.cfc2(13 + r)) / 2048.0);
                const double base = 2.0 * m.r16s(world + 140 + 2 * (3 * r + 2)) / 4096.0;
                l.colour[r][2] = static_cast<float>(base + 0.5 * (l.colour[r][2] - base));
            }
            double up[3], upLength = 0;
            for (u32 r = 0; r < 3; ++r) {
                up[r] = -m.r16s(world + 76 + 2 * (3 * r + 1)) / 4096.0;
                upLength += up[r] * up[r];
            }
            upLength = upLength > 0 ? 1.0 / std::sqrt(upLength) : 0.0;
            const u32 haze = m.r32(world + 580);
            for (u32 r = 0; r < 3; ++r) {
                l.up[r] = static_cast<float>(up[r] * upLength);
                l.horizon[r] = haze ? static_cast<float>(m.r8(haze + 8 + r) / 255.0) : 0.0f;
            }
            light = gte.precise->add_light(l);
            for (u32 k = 0; k < 3; ++k)
                for (u32 r = 0; r < 3; ++r) l.dir[k][r] = -l.dir[k][r];
            if (light >= 0 && gte.precise->add_light(l) < 0) light = -1;
        }
    }
    for (s32 i = 0; i < count; ++i) {
        const u32 polygon = list + 4 + static_cast<u32>(i) * stride;
        const u32 indices = m.r32(polygon + 4);
        const std::array<u32, 4> vertex = {indices & 255u, (indices >> 8) & 255u,
            quad ? (indices >> 16) & 255u : indices >> 16, indices >> 24};
        int exactFacing = -1;
        if (model_exact_on_ && vertex[0] < 256 && vertex[1] < 256 && vertex[2] < 256 && (!quad || vertex[3] < 256) &&
            model_exact_[vertex[0]].ok && model_exact_[vertex[1]].ok && model_exact_[vertex[2]].ok &&
            (!quad || model_exact_[vertex[3]].ok)) {
            auto nclip = [&](u32 a, u32 b, u32 c) {
                const Gte::Exact &p = model_exact_[a], &q = model_exact_[b], &r = model_exact_[c];
                return static_cast<double>(p.sx) * (q.sy - r.sy) + static_cast<double>(q.sx) * (r.sy - p.sy) +
                       static_cast<double>(r.sx) * (p.sy - q.sy);
            };
            const bool facing = nclip(vertex[0], vertex[1], vertex[2]) > 0.0 ||
                                (quad && nclip(vertex[3], vertex[1], vertex[2]) < 0.0);
            exactFacing = facing ? 1 : 0;
        }
        u32 firstScreen = 0;
        if (quad) {
            firstScreen = m.r32(work + kScreens + 4 * vertex[0]);
            gte.mtc2(13, m.r32(work + kScreens + 4 * vertex[1]));
            gte.mtc2(14, m.r32(work + kScreens + 4 * vertex[2]));
            gte.mtc2(12, firstScreen);
            const u32 fourthScreen = m.r32(work + kScreens + 4 * vertex[3]);
            gte.op(0x1400006);
            const s32 firstArea = static_cast<s32>(gte.mfc2(24));
            gte.mtc2(12, fourthScreen);
            if (exactFacing != 1 && firstArea <= 0) {
                gte.op(0x1400006);
                if (static_cast<s32>(gte.mfc2(24)) >= 0) continue;
            }
        } else {
            for (u32 corner = 0; corner < 3; ++corner)
                gte.mtc2(12 + corner, m.r32(work + kScreens + 4 * vertex[corner]));
            gte.op(0x1400006);
            if (exactFacing != 1 && static_cast<s32>(gte.mfc2(24)) <= 0) continue;
        }
        s32 depth;
        if (quad) {
            s32 total = 0;
            for (u32 corner = 0; corner < 4; ++corner) total += m.r16s(work + kDepths + 2 * vertex[corner]);
            depth = (total >> 2) >> (m.r32(work + 12) & 31u);
        } else {
            for (u32 corner = 0; corner < 3; ++corner)
                gte.mtc2(17 + corner, static_cast<u32>(m.r16s(work + kDepths + 2 * vertex[corner])));
            gte.op(0x158002D);
            depth = static_cast<s32>(gte.mfc2(24)) >> ((m.r32(work + 12) + 10u) & 31u);
        }
        s32 slot;
        if (colourPath == Stored) {
            if (static_cast<u32>(depth) - 31u >= 0x77D9u) continue;
            slot = depth >> 3;
        } else {
            const s32 relativeDepth = wrap::sub(wrap::add(depth, colourPath == DepthCued ? 4 : 0), m.r16s(work + kCenterDepth));
            if (static_cast<u32>(relativeDepth) + 75u >= 152u) continue;
            slot = relativeDepth >> 2;
        }
        auto loadNormal = [&](u32 normalSlot, u32 corner) {
            const u32 normal = normals + 8u * m.r8(polygon + 8 + corner);
            gte.mtc2(2 * normalSlot, m.r32(normal));
            gte.mtc2(2 * normalSlot + 1, m.r32(normal + 4));
        };
        if (colourPath == Lit) {
            if (quad) {
                loadNormal(0, 0);
                gte.op(0xC8041E);
                loadNormal(2, 3);
                loadNormal(1, 2);
                m.w32(packet + 4, gte.mfc2(22));
                loadNormal(0, 1);
            } else {
                for (u32 corner = 0; corner < 3; ++corner) loadNormal(corner, corner);
            }
            gte.op(0xD80420);
        }
        const u32 entry = orderTable + 4u * static_cast<u32>(slot);
        m.w32(packet, m.r32(entry) | (quad ? 0x0C000000u : 0x09000000u));
        m.w32(entry, packet & 0xFFFFFFu);

        if (colourPath == Stored) {
            if (quad) {
                const u32 colour0 = m.r32(polygon + 8), uv0 = m.r32(polygon + 24);
                m.w32(packet + 8, firstScreen); m.w32(packet + 4, colour0); m.w32(packet + 12, uv0);
                const u32 colour1 = m.r32(polygon + 12), uv1 = m.r32(polygon + 28), colour2 = m.r32(polygon + 16);
                m.w32(packet + 16, colour1); m.w32(packet + 20, gte.mfc2(13));
                m.w32(packet + 24, uv1); m.w32(packet + 28, colour2); m.w32(packet + 32, gte.mfc2(14));
                const u32 uv2 = m.r32(polygon + 32), colour3 = m.r32(polygon + 20);
                m.w32(packet + 36, uv2); m.w32(packet + 40, colour3);
                m.w32(packet + 44, gte.mfc2(12)); m.w32(packet + 48, uv2 >> 16);
            } else {
                const u32 colour0 = m.r32(polygon + 8), uv0 = m.r32(polygon + 20), colour1 = m.r32(polygon + 12);
                m.w32(packet + 4, colour0); m.w32(packet + 8, gte.mfc2(12));
                m.w32(packet + 12, uv0); m.w32(packet + 16, colour1);
                const u32 uv1 = m.r32(polygon + 24), colour2 = m.r32(polygon + 16), uv2 = m.r32(polygon + 28);
                m.w32(packet + 20, gte.mfc2(13)); m.w32(packet + 24, uv1); m.w32(packet + 28, colour2);
                m.w32(packet + 32, gte.mfc2(14)); m.w32(packet + 36, uv2);
            }
        } else if (colourPath == DepthCued) {
            m.w32(packet + 8, quad ? firstScreen : gte.mfc2(12));
            m.w32(packet + 20, gte.mfc2(13)); m.w32(packet + 32, gte.mfc2(14));
            if (quad) {
                m.w32(packet + 44, gte.mfc2(12));
                gte.mtc2(6, m.r32(polygon + 8));
                gte.op(0x780010);
                m.w32(packet + 12, m.r32(polygon + 24));
                m.w32(packet + 24, m.r32(polygon + 28));
                m.w32(packet + 4, gte.mfc2(22));
                for (u32 corner = 0; corner < 3; ++corner) gte.mtc2(20 + corner, m.r32(polygon + 12 + 4 * corner));
                gte.op(0xF8002A);
                const u32 uv = m.r32(polygon + 32);
                m.w32(packet + 36, uv); m.w32(packet + 48, uv >> 16);
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 16 + 12 * corner, gte.mfc2(20 + corner));
            } else {
                const u32 colour = m.r32(polygon + 8);
                gte.mtc2(6, colour); gte.mtc2(20, colour);
                gte.mtc2(21, m.r32(polygon + 12)); gte.mtc2(22, m.r32(polygon + 16));
                gte.op(0xF8002A);
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 12 + 12 * corner, m.r32(polygon + 20 + 4 * corner));
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 4 + 12 * corner, gte.mfc2(20 + corner));
            }
        } else {
            if (quad) {
                const u32 uv0 = m.r32(polygon + 12);
                m.w32(packet + 8, firstScreen); m.w32(packet + 12, uv0);
                const u32 uv2 = m.r32(polygon + 20), uv1 = m.r32(polygon + 16);
                m.w32(packet + 36, uv2); m.w32(packet + 24, uv1); m.w32(packet + 48, uv2 >> 16);
                m.w32(packet + 20, gte.mfc2(13)); m.w32(packet + 32, gte.mfc2(14)); m.w32(packet + 44, gte.mfc2(12));
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 16 + 12 * corner, gte.mfc2(20 + corner));
            } else {
                const std::array<u32, 3> uv = {m.r32(polygon + 12), m.r32(polygon + 16), m.r32(polygon + 20)};
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 12 + 12 * corner, uv[corner]);
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 8 + 12 * corner, gte.mfc2(12 + corner));
                for (u32 corner = 0; corner < 3; ++corner) m.w32(packet + 4 + 12 * corner, gte.mfc2(20 + corner));
            }
        }
        if (exactFacing >= 0) {
            PreciseCache::Packet kept;
            for (u32 corner = 0; corner < (quad ? 4u : 3u); ++corner) {
                const Gte::Exact& e = model_exact_[vertex[corner]];
                kept.v[corner] = PreciseVertex{e.sx, e.sy, e.vx, e.vy, e.vz};
            }
            kept.ok = quad ? 15 : 7;
            kept.fx = static_cast<u8>((model_fx_ ? 1 : 0) | (model_depth_ ? 8 : 0));
            kept.depth_shift = static_cast<u8>(m.r32(work + 12) & 31u);
            if (light >= 0) {
                PreciseCache::Lit lit;
                double sum[3] = {};
                for (u32 corner = 0; corner < (quad ? 4u : 3u); ++corner) {
                    const u32 normal = normals + 8u * m.r8(polygon + 8 + corner);
                    const double n[3] = {static_cast<double>(m.r16s(normal)), static_cast<double>(m.r16s(normal + 2)),
                        static_cast<double>(m.r16s(normal + 4))};
                    double v[3], len = 0;
                    for (u32 r = 0; r < 3; ++r) {
                        v[r] = rotation[r][0] * n[0] + rotation[r][1] * n[1] + rotation[r][2] * n[2];
                        len += v[r] * v[r];
                    }
                    len = len > 0 ? 1.0 / std::sqrt(len) : 0.0;
                    for (u32 r = 0; r < 3; ++r) {
                        lit.n[corner][r] = static_cast<float>(v[r] * len);
                        sum[r] += v[r] * len;
                    }
                }
                const PreciseVertex &p0 = kept.v[0], &p1 = kept.v[1], &p2 = kept.v[2];
                const double e1[3] = {p1.vx - p0.vx, p1.vy - p0.vy, p1.vz - p0.vz};
                const double e2[3] = {p2.vx - p0.vx, p2.vy - p0.vy, p2.vz - p0.vz};
                double face[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
                if (face[0] * p0.vx + face[1] * p0.vy + face[2] * p0.vz > 0)
                    for (double& f : face) f = -f;
                const bool inward = sum[0] * face[0] + sum[1] * face[1] + sum[2] * face[2] < 0;
                lit.light = static_cast<u16>(light + (inward ? 1 : 0));
                if (inward)
                    for (auto& n : lit.n)
                        for (float& c : n) c = -c;
                gte.precise->record_packet(packet, kept, &lit);
            } else {
                gte.precise->record_packet(packet, kept);
            }
        }
        packet += quad ? 52u : 40u;
    }
    m.w32(work + 4, packet);
    return count > 0 ? 0u : list;
}

u32 Game::model_stored_triangles(u32 work) {
    cover(0x80062D68u);
    return model_polygon_list(work, false, Stored);
}
u32 Game::model_stored_quads(u32 work) {
    cover(0x80062EC8u);
    return model_polygon_list(work, true, Stored);
}
u32 Game::model_fog_triangles(u32 work) {
    cover(0x8006305Cu);
    return model_polygon_list(work, false, DepthCued);
}
u32 Game::model_fog_quads(u32 work) {
    cover(0x800631F8u);
    Frame frame(*this, 8);
    return model_polygon_list(work, true, DepthCued);
}
u32 Game::model_lit_triangles(u32 work) {
    cover(0x800633D8u);
    return model_polygon_list(work, false, Lit);
}
u32 Game::model_lit_quads(u32 work) {
    cover(0x8006358Cu);
    Frame frame(*this, 8);
    return model_polygon_list(work, true, Lit);
}

u32 Game::terrain_models(u32 entries, u32 work) {
    cover(0x800626E0u);
    Frame frame(*this, 0x20);
    for (u32 channel = 21; channel <= 23; ++channel) gte.ctc2(channel, 2096);
    for (double& d : terrain_delta_) d = 0.0;
    u32 result = entries;
    for (u32 entry = entries; m.r32(entry + 16); entry += 20) {
        const u32 model = m.r32(entry + 16);
        for (u32 axis = 0; axis < 3; ++axis) gte.ctc2(5 + axis, static_cast<u32>(m.r16s(entry + 8 + 2 * axis)));
        terrain_project_vertices(m.r32(model + 4), work);
        m.w32(work + 560, m.r32(model + 4) + 4);
        m.w32(work + 8, m.r32(model + 16));
        terrain_gouraud_polygons(work);
        m.w32(work + 8, m.r32(model + 12));
        result = terrain_flat_polygons(work);
    }
    return result;
}

void Game::terrain_project_vertices(u32 vertices, u32 work) {
    s32 remaining = m.r32s(vertices);
    u32 source = vertices + 4, projected = work + 32;
    terrain_exact_on_ = gte.precise && player_settings;
    u32 index = 0;
    auto readGroup = [&](u32 address) {
        return std::array<u32, 5>{m.r32(address), m.r32(address + 4), m.r32(address + 8),
            m.r32(address + 12), m.r32(address + 16)};
    };
    auto group = readGroup(source);
    do {
        gte.mtc2(0, group[0]); gte.mtc2(2, group[1]); gte.mtc2(4, group[2]);
        gte.mtc2(1, group[3]); gte.mtc2(3, group[3] >> 16); gte.mtc2(5, group[4]);
        gte.op(0x280030);
        source += 20;
        group = readGroup(source);
        for (u32 corner = 0; corner < 3; ++corner) {
            m.w32(projected + 16 * corner, gte.mfc2(12 + corner));
            m.w32(projected + 16 * corner + 4, gte.mfc2(17 + corner));
            m.w32(projected + 16 * corner + 8, gte.mfc2(8));
        }
        if (terrain_exact_on_)
            for (u32 corner = 0; corner < 3; ++corner)
                if (index + corner < 64) terrain_exact_[index + corner] = terrain_exact(gte.exact_fifo[corner]);
        index += 3;
        projected += 48; remaining = wrap::sub(remaining, 3);
    } while (remaining > 0);
}
}
