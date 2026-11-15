#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kVertices = 0x800CBC5Cu;
constexpr u32 kNormals = 0x800CBC60u;
constexpr u32 kCursor = 0x800CBC64u;
constexpr u32 kCount = 0x800CBC68u;

constexpr u32 kObjects = 12u, kObjectSize = 28u;
constexpr u32 kObjVerts = 0, kObjNormals = 8, kObjPrims = 16, kObjPrimCount = 20;

constexpr u32 kPrimSize = 0x78u;
constexpr u32 kPrimColour = 4;
constexpr u32 kPrimTpage = 0x14, kPrimClut = 0x16, kPrimUv = 0x18;
constexpr u32 kPrimVertex = 0x20;
constexpr u32 kPrimNormal = 0x40;
constexpr u32 kPrimVertexTable = 0x60, kPrimNormalTable = 0x64;
constexpr u32 kPrimVertexIndex = 0x68;
constexpr u32 kPrimNormalIndex = 0x70;

struct Layout {
    u32 id[2];
    u32 bytes;
    u32 corners;
    bool texture;
    s32 flat;
    s32 gouraud;
    bool colours_last;
    u8 vertex[4];
    bool lit;
    u8 normal[4];
};
constexpr Layout kLayouts[16] = {
    {{0x20000304u, 0x20020304u}, 0x10, 3, false, 4, -1, false, {0x0A, 0x0C, 0x0E, 0}, true, {8, 8, 8, 0}},
    {{0x30000406u, 0x30020406u}, 0x14, 3, false, 4, -1, false, {0x0A, 0x0E, 0x12, 0}, true, {8, 0x0C, 0x10, 0}},
    {{0x24000507u, 0x24020507u}, 0x18, 3, true, -1, -1, false, {0x12, 0x14, 0x16, 0}, true, {0x10, 0x10, 0x10, 0}},
    {{0x34000609u, 0x34020609u}, 0x1C, 3, true, -1, -1, false, {0x12, 0x16, 0x1A, 0}, true, {0x10, 0x14, 0x18, 0}},
    {{0x28000405u, 0x28020405u}, 0x14, 4, false, 4, -1, false, {0x0A, 0x0C, 0x0E, 0x10}, true, {8, 8, 8, 8}},
    {{0x38000508u, 0x38020508u}, 0x18, 4, false, 4, -1, false, {0x0A, 0x0E, 0x12, 0x16}, true, {8, 0x0C, 0x10, 0x14}},
    {{0x2C000709u, 0x2C020709u}, 0x20, 4, true, -1, -1, false, {0x16, 0x18, 0x1A, 0x1C}, true, {0x14, 0x14, 0x14, 0x14}},
    {{0x3C00080Cu, 0x3C02080Cu}, 0x24, 4, true, -1, -1, false, {0x16, 0x1A, 0x1E, 0x22}, true, {0x14, 0x18, 0x1C, 0x20}},
    {{0x21010304u, 0x21030304u}, 0x10, 3, false, 4, -1, false, {8, 0x0A, 0x0C, 0}, false, {}},
    {{0x31010506u, 0x31030506u}, 0x18, 3, false, -1, 4, false, {0x10, 0x12, 0x14, 0}, false, {}},
    {{0x25010607u, 0x25030607u}, 0x1C, 3, true, 0x10, -1, false, {0x14, 0x16, 0x18, 0}, false, {}},
    {{0x35010809u, 0x35030809u}, 0x24, 3, true, -1, 0x10, true, {0x1C, 0x1E, 0x20, 0}, false, {}},
    {{0x29010305u, 0x29030305u}, 0x10, 4, false, 4, -1, false, {8, 0x0A, 0x0C, 0x0E}, false, {}},
    {{0x39010608u, 0x39030608u}, 0x1C, 4, false, -1, 4, false, {0x14, 0x16, 0x18, 0x1A}, false, {}},
    {{0x2D010709u, 0x2D030709u}, 0x20, 4, true, 0x14, -1, false, {0x18, 0x1A, 0x1C, 0x1E}, false, {}},
    {{0x3D010A0Cu, 0x3D030A0Cu}, 0x2C, 4, true, -1, 0x14, true, {0x24, 0x26, 0x28, 0x2A}, false, {}},
};
}

u32 Game::tmd_object(u32 tmd, u32 obj, u32 prims, u32 vertices, u32 normals) {
    cover(0x800AB1BCu);
    Frame f(*this, 0x38);
    const u32 objects = tmd + kObjects;
    get_graph_debug();
    get_graph_debug();
    m.w32(f.at(0x10), obj);
    get_graph_debug();
    get_graph_debug();
    get_graph_debug();
    const u32 o = objects + kObjectSize * obj;
    m.w32(vertices, objects + m.r32(o + kObjVerts));
    m.w32(normals, objects + m.r32(o + kObjNormals));
    m.w32(prims, objects + m.r32(o + kObjPrims));
    return m.r32(o + kObjPrimCount);
}

u32 Game::tmd_open(u32 tmd, u32 obj) {
    cover(0x800AADBCu);
    Frame f(*this, 0x20);
    m.w32(f.at(0x10), kNormals);
    const u32 n = tmd_object(tmd, obj, kCursor, kVertices, kNormals);
    m.w32(kCount, n);
    return n;
}

s32 Game::tmd_prim(u32 packet, u32 prim) {
    cover(0x800AB330u);
    Frame f(*this, 0x20);
    memset(prim, 0, static_cast<s32>(kPrimSize));
    const u32 id = m.r32(packet);
    m.w32(prim, id);
    const Layout* layout = nullptr;
    for (const Layout& l : kLayouts)
        if ((id & 0xFDFFFFFFu) == l.id[0] || (id & 0xFDFFFFFFu) == l.id[1]) layout = &l;
    if (!layout) {
        (void)m.r32(prim);
        return -1;
    }
    get_graph_debug();
    const Layout& l = *layout;
    auto colours = [&] {
        for (u32 k = 0; k < l.corners; ++k) {
            const u32 from = l.flat >= 0 ? static_cast<u32>(l.flat) : static_cast<u32>(l.gouraud) + 4 * k;
            for (u32 c = 0; c < 3; ++c) m.w8(prim + kPrimColour + 4 * k + c, m.r8(packet + from + c));
        }
    };
    if (l.texture) {
        m.w16(prim + kPrimTpage, m.r16(packet + 10));
        m.w16(prim + kPrimClut, m.r16(packet + 6));
        for (u32 k = 0; k < l.corners; ++k) {
            m.w8(prim + kPrimUv + 2 * k, m.r8(packet + 4 + 4 * k));
            m.w8(prim + kPrimUv + 2 * k + 1, m.r8(packet + 5 + 4 * k));
        }
    }
    const bool has_colours = l.flat >= 0 || l.gouraud >= 0;
    if (has_colours && !l.colours_last) colours();
    for (u32 k = 0; k < l.corners; ++k) m.w16(prim + kPrimVertexIndex + 2 * k, m.r16(packet + l.vertex[k]));
    if (l.lit)
        for (u32 k = 0; k < l.corners; ++k) m.w16(prim + kPrimNormalIndex + 2 * k, m.r16(packet + l.normal[k]));
    if (has_colours && l.colours_last) colours();
    return static_cast<s32>(l.bytes);
}

u32 Game::tmd_read(u32 prim) {
    cover(0x800AADFCu);
    Frame f(*this, 0x18);
    const s32 step = tmd_prim(m.r32(kCursor), prim);
    if (step < 0) return 0;
    const u32 vertices = m.r32(kVertices), normals = m.r32(kNormals);
    m.w32(prim + kPrimVertexTable, vertices);
    m.w32(prim + kPrimNormalTable, normals);
    auto fetch = [&](u32 table, u32 index_at, u32 dst) {
        for (u32 c = 0; c < 3; ++c) m.w16(dst + 2 * c, m.r16(table + 8 * m.r16(index_at) + 2 * c));
    };
    for (u32 k = 0; k < 3; ++k) fetch(normals, prim + kPrimNormalIndex + 2 * k, prim + kPrimNormal + 8 * k);
    const u32 fourth = m.r16(normals + 8 * m.r16(prim + kPrimNormalIndex + 6));
    m.w32(kCursor, m.r32(kCursor) + static_cast<u32>(step));
    m.w16(prim + kPrimNormal + 24, fourth);
    for (u32 c = 1; c < 3; ++c)
        m.w16(prim + kPrimNormal + 24 + 2 * c, m.r16(normals + 8 * m.r16(prim + kPrimNormalIndex + 6) + 2 * c));
    for (u32 k = 0; k < 4; ++k) fetch(vertices, prim + kPrimVertexIndex + 2 * k, prim + kPrimVertex + 8 * k);
    return prim;
}
}
