#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kRadarBlocks = 0x800BCFA8u;
constexpr u32 kRadarPrims = 0x800BCFACu;
constexpr u32 kRadarCenter = 0x800BCFB0u;
constexpr u32 kRadarRange = 0x800C6D7Cu;
constexpr u32 kHudColor = 0x800C6D68u;
constexpr u32 kMarks = 0x800C6A60u;
constexpr u32 kMissileMarks = 0x800C68E0u;
constexpr u32 kBeacon = 0x80023DECu;
constexpr u32 kBeaconArms = 0x800BCC68u;
constexpr u32 kMapTiles = 0x800BCC70u;

void link_area(Memory& m, u32 ot, u32 p) {
    m.w32(p, m.r32(ot) | 0x02000000u);
    m.w32(ot, p & 0x00FFFFFFu);
}
void link_chain(Memory& m, u32 ot, u32 first, u32 last) {
    m.w32(last, (m.r32(last) & 0xFF000000u) | (m.r32(ot) & 0x00FFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (first & 0x00FFFFFFu));
}
}

u32 Game::radar_begin() {
    cover(0x8003C650u);
    const u32 blocks = m.r32(kRadarBlocks);
    const u32 buffer = m.r32(m.r32(g::kScene) + 4);
    m.w32(blocks + 20, 0);
    m.w32(blocks + 16, 0);
    m.w32(blocks + 12, 0);
    m.w32(blocks + 8, 0);
    m.w32(blocks + 24, 0);
    const u32 block = m.r32(blocks + 4 * buffer);
    m.w32(kRadarPrims, block);
    return block;
}

void Game::radar_frame_draw() {
    cover(0x8003D458u);
    u32 t = m.r32(kRadarPrims) + 16 * m.r32(m.r32(kRadarBlocks) + 16) + 2328;
    m.w8(t + 4, m.r8(kHudColor) >> 1);
    m.w8(t + 5, m.r8(kHudColor + 1) >> 1);
    m.w16(t + 8, 0);
    m.w16(t + 10, 45);
    m.w16(t + 14, 45);
    m.w8(t + 6, m.r8(kHudColor + 2) >> 1);
    m.w16(t + 12, 5);
    u32 blocks = m.r32(kRadarBlocks);
    m.w32(blocks + 16, m.r32(blocks + 16) + 1);
    t += 16;
    m.w8(t + 4, m.r8(kHudColor) >> 1);
    m.w8(t + 5, m.r8(kHudColor + 1) >> 1);
    m.w16(t + 8, 86);
    m.w8(t + 6, m.r8(kHudColor + 2) >> 1);
    blocks = m.r32(kRadarBlocks);
    m.w16(t + 10, 14);
    m.w16(t + 12, 320);
    m.w16(t + 14, 14);
    m.w32(blocks + 16, m.r32(blocks + 16) + 1);
    const u32 label = m.r32(kRadarPrims) + 3084;
    m.w8(label + 4, m.r8(kHudColor + 3));
    m.w8(label + 5, m.r8(kHudColor + 4));
    m.w8(label + 6, m.r8(kHudColor + 5));
    const u32 scale = m.r32(kRadarPrims) + 3104;
    m.w8(scale + 4, m.r8(kHudColor + 3) >> 1);
    m.w8(scale + 5, m.r8(kHudColor + 4) >> 1);
    m.w8(scale + 6, m.r8(kHudColor + 5) >> 1);
    const s32 range = m.r32s(kRadarRange);
    m.w8(scale + 12, range < 16385 ? 24u : range > 0x8000 ? 0u : 12u);
}

void Game::radar_ground_draw() {
    cover(0x8003D5C0u);
    Frame f(*this, 8);
    const u32 c = m.r32(kRadarCenter);
    const s32 range = m.r32s(kRadarRange);
    const s32 cx = m.r32s(c), cz = m.r32s(c + 4);
    s32 x0 = ((cx - range) >> 12) << 12;
    s32 x1 = (((cx + range) >> 12) << 12) + 4096;
    s32 z0 = (((cz + range) >> 12) << 12) + 4096;
    s32 z1 = ((cz - range) >> 12) << 12;
    if (x0 > 0x3FFFF) x0 = 0x3FFFF;
    if (x0 < -262144) x0 = -262144;
    if (x1 > 0x3FFFF) x1 = 0x3FFFF;
    if (x1 < -262144) x1 = -262144;
    if (z0 > 0x40000) z0 = 0x40000;
    if (!(z0 > -262144)) z0 = -262143;
    if (z1 > 0x40000) z1 = 0x40000;
    if (!(z1 > -262144)) z1 = -262143;
    const s32 cs = m.r32s(c + 12), sn = m.r32s(c + 8);
    const s32 dx0 = x0 - cx, dx1 = x1 - cx, dz0 = cz - z0, dz1 = cz - z1;
    auto sx = [&](s32 dx, s32 dz) { return wrap::div(32 * ((dx * cs + dz * sn) >> 12), range).lo; };
    auto sy = [&](s32 dx, s32 dz) { return wrap::div(32 * ((-dx * sn + dz * cs) >> 12), range).lo; };
    const s32 ax = sx(dx0, dz0), ay = sy(dx0, dz0);
    const s32 bx = sx(dx1, dz0), by = sy(dx1, dz0);
    const s32 ex = sx(dx0, dz1), ey = sy(dx0, dz1);
    const s32 fx = sx(dx1, dz1), fy = sy(dx1, dz1);
    const u32 q = m.r32(kRadarPrims) + 2724;
    m.w16(q + 8, static_cast<u32>(ax + 44));
    m.w16(q + 10, static_cast<u32>(ay + 46));
    m.w16(q + 16, static_cast<u32>(bx + 44));
    m.w16(q + 18, static_cast<u32>(by + 46));
    m.w16(q + 24, static_cast<u32>(ex + 44));
    m.w16(q + 26, static_cast<u32>(ey + 46));
    m.w16(q + 32, static_cast<u32>(fx + 44));
    m.w16(q + 34, static_cast<u32>(fy + 46));
    const u32 u0 = static_cast<u32>(((dx0 + m.r32s(c)) >> 12) + 64);
    m.w8(q + 28, u0);
    m.w8(q + 12, u0);
    const u32 u1 = static_cast<u32>(((dx1 + m.r32s(m.r32(kRadarCenter))) >> 12) + 64);
    m.w8(q + 36, u1);
    m.w8(q + 20, u1);
    const u32 v0 = static_cast<u32>(((dz0 - m.r32s(m.r32(kRadarCenter) + 4)) >> 12) + 64);
    m.w8(q + 21, v0);
    m.w8(q + 13, v0);
    const u32 v1 = static_cast<u32>(((dz1 - m.r32s(m.r32(kRadarCenter) + 4)) >> 12) + 64);
    m.w8(q + 37, v1);
    m.w8(q + 29, v1);
}

void Game::radar_marks_place() {
    cover(0x8003D9C8u);
    Frame f(*this, 24);
    radar_marks();
    if (m.r32s(kRadarRange) <= 0x8000) radar_missile_marks();
}

u32 Game::radar_mark_draw(u32 mark) {
    cover(0x8003DFF8u);
    const u32 flags = m.r8(mark);
    if (!(flags & 1u)) return 0;
    const u32 p = m.r32(kRadarPrims) + 20 * m.r32(m.r32(kRadarBlocks) + 8) + 24;
    m.w16(p + 14, 32664);
    auto dot = [&](u32 size) {
        m.w8(p + 12, 98);
        m.w8(p + 13, 6);
        m.w16(p + 16, size);
        m.w16(p + 18, size);
    };
    if (flags & 8u) {
        dot(m.r8(mark + 2) == 1 ? 3u : 2u);
    } else {
        const u32 kind = m.r8(mark + 1);
        if (kind == 0) {
            const u32 band = m.r8(mark + 5), heading = m.r8(mark + 4);
            if (band == 0) {
                m.w8(p + 13, 51);
                m.w16(p + 16, 4);
                m.w16(p + 18, 4);
                m.w8(p + 12, 4 * heading + 62);
            } else if (band == 1) {
                m.w8(p + 13, 11);
                m.w16(p + 16, 6);
                m.w16(p + 18, 6);
                m.w8(p + 12, 6 * heading);
            } else if (band == 2) {
                m.w8(p + 13, 17);
                m.w16(p + 16, 8);
                m.w16(p + 18, 8);
                m.w8(p + 12, 8 * heading);
            }
        } else if (kind == 1) {
            dot(m.r8(mark + 5) + 2u);
        } else if (kind == 2) {
            dot(1);
        }
    }
    u32 level = (m.r8(mark) & 0x10u) ? m.r8(mark + 3) : 131u;
    if ((m.r8(mark) & 4u) && (m.r32(m.r32(g::kWorld) + 12) & 2u)) level >>= 3;
    switch (m.r8(mark + 2)) {
    case 0: pkt_rgb(p, level >> 1, level >> 1, level >> 1); break;
    case 1: pkt_rgb(p, level, 0, 0); break;
    case 2: pkt_rgb(p, 0, 0, level); break;
    case 3: pkt_rgb(p, level, level, 0); break;
    default: break;
    }
    const s32 half_h = m.r16s(p + 18) >> 1;
    m.w16(p + 8, m.r8(mark + 6) - static_cast<u32>(m.r16s(p + 16) >> 1));
    const u32 blocks = m.r32(kRadarBlocks);
    m.w16(p + 10, m.r8(mark + 7) - static_cast<u32>(half_h));
    m.w32(blocks + 8, m.r32(blocks + 8) + 1);
    return 0;
}

u32 Game::radar_beacon_draw(u32 beacon) {
    cover(0x8003E274u);
    Frame f(*this, 0x58);
    const u32 c = m.r32(kRadarCenter);
    const s32 dx = (m.r32s(beacon) >> 12) - m.r32s(c);
    const s32 dz = m.r32s(c + 4) - (m.r32s(beacon + 8) >> 12);
    const s32 ax = dx < 0 ? m.r32s(c) - (m.r32s(beacon) >> 12) : dx;
    const s32 az = dz < 0 ? (m.r32s(beacon + 8) >> 12) - m.r32s(c + 4) : dz;
    s32 d;
    if (ax >= 0x4000 || az >= 0x4000) d = 16 * square_root0((dx >> 4) * (dx >> 4) + (dz >> 4) * (dz >> 4));
    else d = square_root0(dx * dx + dz * dz);
    const s32 range = m.r32s(kRadarRange);
    if (range < d) return 0;
    const u32 c1 = m.r32(kRadarCenter);
    const s32 cs = m.r32s(c1 + 12), sn = m.r32s(c1 + 8);
    const s32 px = wrap::div(32 * ((dx * cs + dz * sn) >> 12), range).lo;
    const s32 py = wrap::div(32 * ((-dx * sn + dz * cs) >> 12), range).lo;
    if (static_cast<u32>(px + 31) >= 0x3Fu) return 0;
    if (static_cast<u32>(py + 31) >= 0x3Fu) return 0;
    const u32 x = static_cast<u32>(px + 44), y = static_cast<u32>(py + 46);
    for (u32 i = 0; i < 2; ++i) {
        const u32 arm = m.r16(kBeaconArms + 2 * i);
        const u32 shift = m.r8(kBeaconArms + 4 + i) & 31u;
        const u32 line = m.r32(kRadarPrims) + 3156 + 16 * i;
        const u32 heading = m.r16(m.r32(kRadarCenter) + 16);
        m.w8(line + 5, 0);
        m.w8(line + 6, 0);
        m.w16(line + 8, x);
        m.w16(line + 10, y);
        const u32 a = (arm + heading - m.r16(beacon + 18)) & 0x3FFFu;
        const s32 sa = m.r16s(g::kSinCos + 2 * a);
        m.w8(line + 4, 0xFF);
        m.w16(line + 14, y + static_cast<u32>(m.r16s(g::kSinCos + 0x2000 + 2 * a) >> shift));
        m.w16(line + 12, x + static_cast<u32>(sa >> shift));
    }
    m.w32(m.r32(kRadarBlocks) + 24, 1);
    const u32 w = m.r32(g::kWorld);
    const s32 away = distance(beacon, m.r32(w + 40) + 4);
    if (away > 0x7FFF) return 0;
    const u32 mat = f.at(0x10), v = f.at(0x30), sxy = f.at(0x38), p = f.at(0x40), flag = f.at(0x44);
    m.w16(v, static_cast<u32>((m.r32s(beacon) - m.r32s(m.r32(m.r32(g::kWorld) + 40) + 4)) >> 12));
    m.w16(v + 2, static_cast<u32>((m.r32s(beacon + 4) - m.r32s(m.r32(m.r32(g::kWorld) + 40) + 8)) >> 12));
    const s32 bz = m.r32s(beacon + 8), vz = m.r32s(m.r32(m.r32(g::kWorld) + 40) + 12);
    m.w32(mat + 28, 0);
    m.w32(mat + 24, 0);
    m.w32(mat + 20, 0);
    m.w16(v + 4, static_cast<u32>((bz - vz) >> 12));
    set_rot_matrix(m.r32(g::kWorld) + 76);
    set_trans_matrix(mat);
    rot_trans_pers(v, sxy, p, flag);
    if (m.r32s(flag) < 0) return 0;
    s32 size = (0x8000 - away) >> 10;
    if (size >= 9) size = 8;
    else if (size < 2) size = 2;
    for (u32 k = 0; k < 3; ++k) {
        const u32 t = m.r32(kRadarPrims) + 3188 + 20 * k;
        const u32 tick = static_cast<u32>(m.r32s(m.r32(g::kWorld) + 12) >> 2);
        m.w8(t + 6, 0);
        const u32 level = (k + (tick & 3u)) << 6;
        m.w8(t + 4, level);
        m.w8(t + 5, (level & 0xC0u) >> 1);
        m.w16(t + 8, m.r16(sxy));
        m.w16(t + 10, m.r16(sxy + 2) - (2 * k + 1) * static_cast<u32>(size));
        m.w16(t + 12, m.r16(sxy) + static_cast<u32>(size));
        m.w16(t + 16, m.r16(sxy) - static_cast<u32>(size));
        const u32 base_y = m.r16(sxy + 2) - (2 * k + 2) * static_cast<u32>(size);
        m.w16(t + 18, base_y);
        m.w16(t + 14, base_y);
    }
    const u32 blocks = m.r32(kRadarBlocks);
    m.w32(blocks + 24, m.r32(blocks + 24) | 2u);
    return 0;
}

void Game::radar_marks_draw() {
    cover(0x8003DE5Cu);
    Frame f(*this, 0x38);
    for (u32 k = 2; k < 64; ++k)
        if ((m.r16(g::kSlotFlags + 4 * k) & 0x2000u) && !m.r8(kMarks + 12 * k + 2)) radar_mark_draw(kMarks + 12 * k);
    for (u32 k = 2; k < 64; ++k)
        if ((m.r16(g::kSlotFlags + 4 * k) & 0x2000u) && m.r8(kMarks + 12 * k + 2)) radar_mark_draw(kMarks + 12 * k);
    if (m.r32s(kRadarRange) <= 0x8000)
        for (u32 k = 0; k < 32; ++k) radar_mark_draw(kMissileMarks + 12 * k);
    const s32 mission = m.r16s(m.r32(g::kScene) + 908);
    if ((mission == 18 || mission == 37) && m.r32(m.r32(g::kWorld) + 8) == 1) {
        const u32 beacon = f.at(0x10);
        for (u32 k = 0; k < 24; k += 4) m.w32(beacon + k, m.r32(kBeacon + k));
        radar_beacon_draw(beacon);
    }
}

void Game::radar_link() {
    cover(0x8003C690u);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    if (m.r32(m.r32(kRadarBlocks) + 24) & 2u)
        for (u32 k = 0; k < 3; ++k) ot_link(ot, m.r32(kRadarPrims) + 3188 + 20 * k);
    const u32 blocks = m.r32(kRadarBlocks);
    for (s32 k = 0; k < m.r32s(blocks + 16); ++k) ot_link(ot, m.r32(kRadarPrims) + 2328 + 16 * static_cast<u32>(k));
    const u32 b = m.r32(kRadarPrims);
    ot_link(ot, b + 3084);
    ot_link(ot, b + 3104);
    ot_link(ot, b + 12);
    link_area(m, ot, b + 2696);
    if (m.r32(m.r32(kRadarBlocks) + 24) & 1u)
        for (u32 k = 0; k < 2; ++k) ot_link(ot, m.r32(kRadarPrims) + 3156 + 16 * k);
    const u32 blocks1 = m.r32(kRadarBlocks);
    for (s32 k = 0; k < m.r32s(blocks1 + 20); ++k) ot_link(ot, m.r32(kRadarPrims) + 2456 + 24 * static_cast<u32>(k));
    const u32 blocks2 = m.r32(kRadarBlocks);
    for (s32 k = 0; k < m.r32s(blocks2 + 8); ++k) ot_link(ot, m.r32(kRadarPrims) + 24 + 20 * static_cast<u32>(k));
    link_area(m, ot, m.r32(kRadarPrims) + 2684);
    const u32 b1 = m.r32(kRadarPrims);
    link_chain(m, ot, b1 + 2924, b1 + 3044);
    ot_link(ot, b1 + 3140);
    link_area(m, ot, b1 + 2672);
    const u32 b2 = m.r32(kRadarPrims);
    ot_link(ot, b2 + 2708);
    ot_link(ot, b2 + 2724);
    link_area(m, ot, b2 + 2660);
    const u32 b3 = m.r32(kRadarPrims);
    link_chain(m, ot, b3 + 2764, b3 + 2884);
    ot_link(ot, b3 + 3124);
    link_area(m, ot, b3 + 2648);
    const u32 b4 = m.r32(kRadarPrims);
    ot_link(ot, b4);
    ot_link(ot, b4 + 2716);
}

s32 Game::map_place(u32 pos, u32 out, u32 scroll) {
    cover(0x8003EC0Cu);
    s32 x = (m.r32s(pos) >> 23) + 128;
    s32 y = (-m.r32s(pos + 8) >> 23) + 128;
    if (x >= 256) x = 255;
    if (x < 0) x = 0;
    if (y >= 256) y = 255;
    if (y < 0) y = 0;
    m.w16(out, static_cast<u32>(x) - m.r16(scroll));
    const s32 sx = m.r16s(out);
    const s32 sy = static_cast<s16>(static_cast<u32>(y) - m.r16(scroll + 2));
    m.w16(out + 2, static_cast<u32>(sy));
    if (sx < 0 || sy < 0) return -1;
    if (sx >= 128) return -1;
    return sy < 128 ? 0 : -1;
}

void Game::map_draw() {
    cover(0x8003E69Cu);
    Frame f(*this, 0x40);
    const u32 place = f.at(0x10), scroll = f.at(0x18);
    const u32 player = m.r32(m.r32(g::kSlots));
    s32 cell_x = (m.r32s(player + 8) >> 23) + 128;
    s32 cell_y = (-m.r32s(player + 16) >> 23) + 128;
    if (cell_x >= 256) cell_x = 255;
    if (cell_x < 0) cell_x = 0;
    if (cell_y >= 256) cell_y = 255;
    if (cell_y < 0) cell_y = 0;
    m.w16(scroll, cell_x < 64 ? 0u : cell_x >= 193 ? 128u : static_cast<u32>(cell_x - 64));
    m.w16(scroll + 2, cell_y < 64 ? 0u : cell_y >= 193 ? 128u : static_cast<u32>(cell_y - 64));
    const u32 half_x = static_cast<u32>(m.r16s(scroll) + static_cast<s32>(m.r16(scroll) >> 15)) >> 1;
    const u32 half_y = static_cast<u32>(m.r16s(scroll + 2) + static_cast<s32>(m.r16(scroll + 2) >> 15)) >> 1;
    for (u32 k = 0; k < 7; ++k) {
        const u32 q = m.r32(kRadarPrims) + 3284 + 40 * k;
        const u32 u = m.r8(kMapTiles + k), v = m.r8(kMapTiles + 8 + k);
        const u32 w = m.r8(kMapTiles + 16 + k), h = m.r8(kMapTiles + 24 + k);
        m.w8(q + 12, u + half_x);
        m.w8(q + 13, v + half_y);
        m.w8(q + 20, w + u + half_x);
        m.w8(q + 21, v + half_y);
        m.w8(q + 28, u + half_x);
        m.w8(q + 29, h + v + half_y);
        m.w8(q + 36, w + u + half_x);
        m.w8(q + 37, h + v + half_y);
    }
    const u32 arrow = m.r32(kRadarPrims) + 4092;
    map_place(player + 8, place, scroll);
    const u32 ax = m.r16(place) + 96u, ay = m.r16(place + 2) + 56u;
    m.w16(arrow + 8, m.r16(place) + 92u);
    m.w16(arrow + 10, m.r16(place + 2) + 52u);
    m.w8(arrow + 13, 59);
    m.w8(arrow + 12, (((0x4200u - (m.r16(player + 26) & 0x3FFFu)) >> 7) & 0x78u) - 0x80u);
    u32 a = static_cast<u32>(0x1AAC - m.r16s(player + 26)) & 0x3FFFu;
    auto sn = [&](u32 i) { return static_cast<u32>(m.r16s(g::kSinCos + 2 * i) >> 10); };
    auto cs = [&](u32 i) { return static_cast<u32>(m.r16s(g::kSinCos + 0x2000 + 2 * i) >> 10); };
    for (u32 k = 0; k < 2; ++k) {
        const u32 q = m.r32(kRadarPrims) + 4044 + 24 * k;
        const u32 a0 = a & 0x3FFFu;
        const u32 a1 = (a + 682u) & 0x3FFFu;
        a = (a1 + 682u) & 0x3FFFu;
        m.w16(q + 12, ax);
        m.w16(q + 14, ay);
        m.w16(q + 10, ay + cs(a0));
        m.w16(q + 8, ax + sn(a0));
        m.w16(q + 18, ay + cs(a1));
        m.w16(q + 16, ax + sn(a1));
        m.w16(q + 20, ax + sn(a));
        m.w16(q + 22, ay + cs(a));
    }
    u32 t = m.r32(kRadarPrims) + 16 * m.r32(m.r32(kRadarBlocks) + 12) + 1304;
    auto shown = [&](u32 k) {
        const u32 sf = m.r32(g::kSlotFlags + 4 * k);
        return (m.r16(slot(k)) & 0x4000u) && (sf & 1u) && (sf & 0x2000u);
    };
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        if (!(m.r32(g::kSlotFlags + 4 * k) & 0x100u) || !shown(k)) continue;
        if (map_place(e + 8, place, scroll) < 0) continue;
        m.w16(t + 8, m.r16(place) + 96u);
        pkt_rgb(t, 0xFF, 0x40, 0x40);
        m.w16(t + 12, 2);
        m.w16(t + 14, 2);
        m.w8(t + 7, m.r8(t + 7) & 0xFDu);
        const u32 blocks = m.r32(kRadarBlocks);
        m.w16(t + 10, m.r16(place + 2) + 56u);
        m.w16(t + 8, m.r16(t + 8) - 1u);
        m.w16(t + 10, m.r16(place + 2) + 56u - 1u);
        m.w32(blocks + 12, m.r32(blocks + 12) + 1);
        t += 16;
    }
    for (u32 k = 2; k < 64; ++k) {
        const u32 e = slot(k);
        if ((m.r32(g::kSlotFlags + 4 * k) & 0x100u) || !shown(k)) continue;
        if (map_place(e + 8, place, scroll) < 0) continue;
        m.w16(t + 8, m.r16(place) + 96u);
        m.w16(t + 10, m.r16(place + 2) + 56u);
        const u32 ef = m.r32(e);
        if (ef & 0x40u) pkt_rgb(t, 0, 0, 0xFF);
        else if (ef & 0x100u) pkt_rgb(t, 0xFF, 0xFF, 0);
        else pkt_rgb(t, 0x40, 0x40, 0x40);
        m.w16(t + 12, 2);
        m.w16(t + 14, 2);
        m.w8(t + 7, m.r8(t + 7) | 2u);
        const u32 blocks = m.r32(kRadarBlocks);
        m.w32(blocks + 12, m.r32(blocks + 12) + 1);
        t += 16;
    }
}

void Game::map_link() {
    cover(0x8003CB90u);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    if (m.r32(m.r32(g::kWorld) + 12) & 8u) ot_link(ot, m.r32(kRadarPrims) + 4092);
    const u32 b = m.r32(kRadarPrims);
    const u32 blocks = m.r32(kRadarBlocks);
    ot_link(ot, b + 4216);
    for (s32 k = 0; k < m.r32s(blocks + 12); ++k) ot_link(ot, b + 1304 + 16 * static_cast<u32>(k));
    const u32 b1 = m.r32(kRadarPrims);
    ot_link(ot, b1 + 4208);
    for (u32 k = 0; k < 2; ++k) ot_link(ot, b1 + 4044 + 24 * k);
    const u32 b2 = m.r32(kRadarPrims);
    for (u32 k = 0; k < 4; ++k) ot_link(ot, b2 + 4112 + 20 * k);
    const u32 b3 = m.r32(kRadarPrims);
    ot_link(ot, b3 + 4200);
    for (u32 k = 0; k < 10; ++k) ot_link(ot, b3 + 3884 + 16 * k);
    const u32 b4 = m.r32(kRadarPrims);
    for (u32 k = 0; k < 4; ++k) ot_link(ot, b4 + 3788 + 24 * k);
    const u32 b5 = m.r32(kRadarPrims);
    for (u32 k = 0; k < 3; ++k) ot_link(ot, b5 + 3284 + 40 * k);
    link_area(m, ot, m.r32(kRadarPrims) + 3272);
    const u32 b6 = m.r32(kRadarPrims);
    for (u32 k = 3; k < 7; ++k) ot_link(ot, b6 + 3284 + 40 * k);
    link_area(m, ot, m.r32(kRadarPrims) + 3260);
    const u32 b7 = m.r32(kRadarPrims);
    for (u32 k = 0; k < 4; ++k) ot_link(ot, b7 + 3564 + 40 * k);
    const u32 b8 = m.r32(kRadarPrims);
    for (u32 k = 0; k < 4; ++k) ot_link(ot, b8 + 3724 + 16 * k);
    link_area(m, ot, m.r32(kRadarPrims) + 3248);
}
}
