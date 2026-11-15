#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>
#include <array>
#include <limits>

namespace ac3::sim {
namespace {
constexpr s32 kLowerPlane = -3000;
constexpr u32 kGroundHits = 0xF00u, kLowerHits = 0xF000u;
constexpr u32 kTileOriginX = 0x800BD068u, kTileOriginZ = 0x800BD06Cu;
constexpr s32 kWidestFar = 29500;

struct Unsqueezed {
    Gte& gte;
    s32 squeeze;
    explicit Unsqueezed(Gte& g) : gte(g), squeeze(g.x_squeeze) { g.x_squeeze = 0x10000; }
    ~Unsqueezed() { gte.x_squeeze = squeeze; }
    Unsqueezed(const Unsqueezed&) = delete;
    Unsqueezed& operator=(const Unsqueezed&) = delete;
};
}

u32 Game::terrain_outline_build(u32 viewport, u32 position, u32 angles, u32 outline) {
    cover(0x8005F90Cu);
    Frame frame(*this, 0x3D0);
    const Unsqueezed unsqueezed(gte);
    const u32 points = frame.at(0x10), compact = frame.at(0x190);
    const u32 selected = frame.at(0x1D0), corners = frame.at(0x1F0), horizon = frame.at(0x210);
    const u32 rotation = frame.at(0x230), inverse = frame.at(0x250), projectionMatrix = frame.at(0x270);
    const u32 camera = frame.at(0x290), screens = frame.at(0x2A0);
    const u32 order = frame.at(0x2C0), spare = frame.at(0x2D0), others = frame.at(0x2E0);
    const u32 groundCompact = frame.at(0x2E8), resultPoints = frame.at(0x318);
    const u32 flag = frame.at(0x388), fog = frame.at(0x38C);
    auto point = [&](u32 index) { return points + 16u * index; };
    auto select = [&](u32 slot, u32 index) { m.w32(selected + 4u * slot, index); };
    auto indexAt = [&](u32 list, u32 index) { return m.r32(list + 4u * index); };
    auto copyWords = [&](u32 source, u32 dest, u32 count) {
        for (u32 i = 0; i < count; ++i) m.w32(dest + 4 * i, m.r32(source + 4 * i));
    };
    m.w32(frame.at(0x3D0), viewport); m.w32(frame.at(0x3D8), angles); m.w32(frame.at(0x3DC), outline);
    m.w32(outline + 24, 0);
    const s32 projection = static_cast<s32>(read_geom_screen());
    m.w32(frame.at(0x394), static_cast<u32>(projection));
    s32 halfWidth = m.r16s(viewport + 4) >> 1;
    s32 halfHeight = m.r16s(viewport + 6) >> 1;
    if (terrain_widen_ != 1.0f) {
        halfWidth = static_cast<s32>(static_cast<float>(halfWidth) * terrain_widen_);
        halfHeight = static_cast<s32>(static_cast<float>(halfHeight) * terrain_widen_);
    }
    s32 far = 32767;
    if (unsqueezed.squeeze != 0x10000 && unsqueezed.squeeze > 0 && projection > 0) {
        halfWidth = static_cast<s32>(static_cast<s64>(halfWidth) * 65536 / unsqueezed.squeeze);
        const s32 wide = halfWidth * 32767 / projection;
        if (wide > kWidestFar) far = static_cast<s32>(32767LL * kWidestFar / wide);
    }
    terrain_far_ = far;
    auto extent = [&](s32 half, u32 zeroAddress, u32 overflowAddress) {
        const s32 product = half * far;
        if (!projection) unported(zeroAddress, "terrain frustum division by zero");
        if (projection == -1 && static_cast<u32>(product) == 0x80000000u)
            unported(overflowAddress, "terrain frustum division overflow");
        return wrap::div(product, projection).lo;
    };
    const u32 width = extent(halfWidth, 0x8005F990u, 0x8005F9A8u);
    for (u32 axis = 0; axis < 3; ++axis) {
        const u32 value = static_cast<u32>(m.r32s(position + 4 * axis) >> 12);
        m.w32(camera + 4 * axis, value); m.w32(outline + 4 * axis, value);
    }
    m.w32(frame.at(0x390), camera);
    m.w16(outline + 16, m.r16(angles)); m.w16(outline + 18, m.r16(angles + 2));
    m.w32(frame.at(0x3A0), 0xC0);
    const u32 height = extent(halfHeight, 0x8005FA44u, 0x8005FA5Cu);
    m.w32(frame.at(0x3A4), 0x80); m.w16(outline + 20, m.r16(angles + 4));
    const s32 cameraY = m.r32s(camera + 4);
    const bool belowSea = cameraY < kLowerPlane;
    m.w32(frame.at(0x398), belowSea);
    m.w16(corners + 2, 0); m.w16(corners, m.r16(angles)); m.w16(corners + 4, m.r16(angles + 4));
    rot_matrix_yxz_gte(corners, rotation); transpose_matrix(rotation, inverse);
    for (u32 matrix : {rotation, inverse}) {
        m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    }
    for (u32 i = 0; i < 4; ++i) m.w16(corners + 8 * i + 4, static_cast<u32>(far));
    for (u32 i = 0; i < 4; ++i) m.w16(corners + 8 * i, (i == 0 || i == 3) ? 0u - width : width);
    for (u32 i = 0; i < 4; ++i) m.w16(corners + 8 * i + 2, i < 2 ? 0u - height : height);
    set_rot_matrix(rotation); set_trans_matrix(rotation);
    for (u32 i = 0; i < 4; ++i) rot_trans(corners + 8 * i, point(i), flag);
    copyWords(0x800B2BF8u, projectionMatrix, 8);
    select(0, 0xFFFFFFFFu); select(2, static_cast<u32>(outline_min_x(points, 4)));
    select(5, static_cast<u32>(outline_max_x(points, 4)));
    u32 hits = 15, selection = 0x25;
    auto hit = [&](u32 plane, u32 index) { return (hits & (plane << (index & 31))) != 0; };
    for (u32 i = 0; i < 4; ++i) {
        if (clip_segment_height(camera, point(i), 0, point(8 + i))) hits |= 0x100u << i;
        if (clip_segment_height(camera, point(i), kLowerPlane, point(12 + i))) hits |= 0x1000u << i;
        m.w32(frame.at(0x3A0), 0xD0 + 16 * i); m.w32(frame.at(0x3A4), 0x90 + 16 * i);
    }
    for (u32 plane = 0; plane < 2; ++plane) {
        if (!(hits & (plane ? kLowerHits : kGroundHits))) continue;
        set_rot_matrix(projectionMatrix); set_trans_matrix(projectionMatrix);
        for (u32 i = 0; i < 4; ++i) project_rotated(point(8 + 4 * plane + i), screens + 16 * plane + 4 * i);
        for (u32 i = 0; i < 4; ++i) {
            const u32 dest = point(16 + 4 * plane + i), screen = screens + 16 * plane + 4 * i;
            m.w32(dest + 4, 0); m.w32(dest, static_cast<u32>(m.r16s(screen)));
            m.w32(dest + 8, static_cast<u32>(m.r16s(screen + 2)));
        }
    }
    if (belowSea && !(hits & (kGroundHits | kLowerHits))) return 0;
    for (u32 i = 0; i < 4; ++i) {
        m.w16(corners + 8 * i, (i & 1) ? 32767 : 0x8000);
        m.w16(corners + 8 * i + 4, static_cast<u32>(far));
        m.w16(corners + 8 * i + 2, 0u - static_cast<u32>(cameraY) + (i < 2 ? 0u : static_cast<u32>(kLowerPlane)));
    }
    set_rot_matrix(inverse);
    for (u32 i = 0; i < 4; ++i) {
        if (i == 3) m.w32(frame.at(0x39C), horizon + 24);
        rot_trans_pers(corners + 8 * i, horizon + 8 * i, fog, flag);
    }
    set_rot_matrix(rotation);
    for (u32 plane = 0; plane < 2; ++plane) {
        const u32 left = horizon + 16 * plane, right = left + 8;
        if (!clip_screen_line(left, right, halfWidth, halfHeight)) continue;
        m.w16(left + 4, static_cast<u32>(far)); m.w16(right + 4, static_cast<u32>(far));
        unproject_rotated(left, point(4 + 2 * plane), projection);
        unproject_rotated(right, point(5 + 2 * plane), projection);
        hits |= plane ? 0xC0 : 0x30;
        if (!plane) {
            selection |= 0x18; select(3, 4); select(4, 5);
        } else {
            if (!(selection & 8) || m.r32s(point(6)) < m.r32s(point(4))) { select(3, 6); selection |= 8; }
            if (!(selection & 16) || m.r32s(point(5)) < m.r32s(point(7))) { select(4, 7); selection |= 16; }
        }
    }
    const bool fullPlane = (hits & kGroundHits) == kGroundHits ||
        ((hits & kLowerHits) == kLowerHits && cameraY != (belowSea ? 0 : kLowerPlane));
    if (!fullPlane && belowSea) {
        selection &= ~1u;
        u32 count = 0, last = 0;
        for (u32 i = 0; i < 4; ++i) if (hit(0x1000, i)) {
            last = i + 12; m.w32(order + 4 * count, last);
            copyWords(point(last), compact + 16 * count, 3); ++count;
        }
        if (count == 1) {
            if (selection & 8) selection &= ~4u;
            if (selection & 16) selection &= ~32u;
            selection |= 1; select(0, indexAt(order, 0));
        } else if (count == 2 || count == 3) {
            const u32 maximum = static_cast<u32>(outline_max_x(compact, static_cast<s32>(count)));
            const u32 minimum = static_cast<u32>(outline_min_x(compact, static_cast<s32>(count)));
            const u32 left = indexAt(order, minimum), right = indexAt(order, maximum);
            select(1, left); select(6, right); selection |= 0x42;
            if (count == 3) {
                for (u32 i = 0; i < 3; ++i) if (i != minimum && i != maximum) { last = i; break; }
                select(0, indexAt(order, last)); selection |= 1;
                if (hit(0x100, left - 12)) { select(2, left - 4); selection |= 4; }
                if (hit(0x100, right - 12)) { select(5, right - 4); selection |= 32; }
            }
        } else if (count == 4) {
            const u32 maximum = static_cast<u32>(outline_max_x(compact, 4));
            const u32 minimum = static_cast<u32>(outline_min_x(compact, 4));
            u32 n = 0;
            for (u32 i = 0; i < 4; ++i) if (i != minimum && i != maximum) m.w32(others + 4 * n++, i);
            const u32 a = indexAt(others, 0), b = indexAt(others, 1);
            const bool opposite = std::abs(static_cast<s32>(minimum) - static_cast<s32>(maximum)) == 2;
            std::array<u32, 4> sorted;
            if (opposite) {
                const bool bFirst = m.r32s(compact + 16 * b + 8) < m.r32s(compact + 16 * a + 8);
                sorted = {indexAt(order, bFirst ? b : a), indexAt(order, minimum),
                    indexAt(order, bFirst ? a : b), indexAt(order, maximum)};
            } else {
                const bool bLow = m.r32s(compact + 16 * b) < m.r32s(compact + 16 * a);
                const u32 low = bLow ? b : a, high = bLow ? a : b;
                sorted = m.r32s(point(high) + 8) < m.r32s(point(maximum) + 8)
                    ? std::array<u32, 4>{low, minimum, maximum, high}
                    : std::array<u32, 4>{minimum, low, high, maximum};
            }
            for (u32 i = 0; i < 4; ++i) m.w32(spare + 4 * i, sorted[i]);
            constexpr std::array<u32, 4> slots = {1, 2, 5, 6};
            for (u32 i = 0; i < 4; ++i) {
                const u32 index = indexAt(spare, i), value = indexAt(order, index);
                select(slots[i], hit(0x100, index) ? value - 4 : value);
            }
            selection = (selection | 0x66) & ~0x18u;
            for (u32 slot : {2u, 5u}) {
                const u32 index = indexAt(selected, slot);
                if (hit(0x100, index)) select(slot, index + 8);
            }
        }
    } else if (!fullPlane) {
        u32 count = 0;
        for (u32 i = 0; i < 4; ++i) if (hit(0x100, i)) {
            m.w32(order + 4 * count, i + 8); copyWords(point(i + 8), groundCompact + 16 * count, 4); ++count;
        }
        if (count >= 3) {
            const u32 minimum = static_cast<u32>(outline_min_x(groundCompact, static_cast<s32>(count)));
            const u32 maximum = static_cast<u32>(outline_max_x(groundCompact, static_cast<s32>(count)));
            select(2, indexAt(order, minimum)); select(5, indexAt(order, maximum));
        }
    } else {
        u32 minimum = 0, maximum = 0, planes = 0;
        if ((hits & kLowerHits) == kLowerHits) {
            minimum = static_cast<u32>(outline_min_x(point(12), 4));
            maximum = static_cast<u32>(outline_max_x(point(12), 4)); planes = 2;
        }
        if ((hits & kGroundHits) == kGroundHits) {
            minimum = static_cast<u32>(outline_min_x(point(8), 4));
            maximum = static_cast<u32>(outline_max_x(point(8), 4)); planes |= 1;
        }
        u32 n = 0;
        for (u32 i = 0; i < 4; ++i) if (i != minimum && i != maximum) m.w32(spare + 4 * n++, i);
        const u32 a = indexAt(spare, 0), b = indexAt(spare, 1);
        const bool opposite = std::abs(static_cast<s32>(minimum) - static_cast<s32>(maximum)) == 2;
        std::array<u32, 4> sorted;
        if (opposite) {
            const bool bFirst = m.r32s(point(b) + 8) < m.r32s(point(a) + 8);
            sorted = {bFirst ? b : a, minimum, bFirst ? a : b, maximum};
        } else {
            const bool bLow = m.r32s(point(b)) < m.r32s(point(a));
            const u32 low = bLow ? b : a, high = bLow ? a : b;
            sorted = m.r32s(point(high) + 8) < m.r32s(point(maximum) + 8)
                ? std::array<u32, 4>{low, minimum, maximum, high}
                : std::array<u32, 4>{minimum, low, high, maximum};
        }
        for (u32 i = 0; i < 4; ++i) m.w32(order + 4 * i, sorted[i]);
        if (outline_same_facing(points, 4)) {
            for (u32 i = 0; i < 4; ++i) {
                const u32 index = indexAt(order, i);
                select(i, index + (belowSea ? (hit(0x100, index) ? 8 : 0) : (m.r16s(outline + 16) > 0 ? 12 : 8)));
            }
            selection = 15;
        } else if (belowSea || cameraY == kLowerPlane) {
            select(0, indexAt(order, 0) + 12);
            if (opposite) {
                for (u32 i = 1; i < 4; ++i) select(i, indexAt(order, i) + 8);
                selection = 15;
            } else {
                for (u32 i = 0; i < 4; ++i) select(i + 1, indexAt(order, i) + 8);
                select(5, indexAt(order, 3) + 12); selection = 63;
            }
        } else {
            select(0, 0xFFFFFFFFu);
            for (u32 i = 0; i < 4; ++i) select(i + 1, indexAt(order, i) + ((planes & 2) ? 12 : 8));
            selection = 31;
        }
    }
    m.w16(corners, 0); m.w16(corners + 4, 0); m.w16(corners + 2, m.r16(angles + 2));
    rot_matrix_yxz_gte(corners, rotation); set_rot_matrix(rotation);
    s32 minX = std::numeric_limits<s32>::max(), minZ = minX;
    for (u32 slot = 0; slot < 7; ++slot) if (selection & (1u << slot)) {
        const u32 count = m.r32(outline + 24), dest = resultPoints + 16 * count;
        const s32 index = m.r32s(selected + 4 * slot);
        if (index < 0) { m.w32(dest + 8, 0); m.w32(dest, 0); }
        else {
            copyWords(point(static_cast<u32>(index)), order, 3); apply_rot_matrix_lv(order, compact);
            m.w32(dest, m.r32(compact)); m.w32(dest + 8, m.r32(compact + 8));
        }
        minX = std::min(minX, m.r32s(dest)); minZ = std::min(minZ, m.r32s(dest + 8));
        m.w32(outline + 24, count + 1);
    }
    u32 result = m.r32(outline + 24);
    for (s32 i = 0; i < m.r32s(outline + 24); ++i) {
        const u32 dest = resultPoints + 16 * static_cast<u32>(i);
        m.w32(dest, m.r32(dest) - 32768u - static_cast<u32>(minX));
        m.w32(dest + 8, m.r32(dest + 8) - 32768u - static_cast<u32>(minZ));
        for (u32 axis = 0; axis < 3; ++axis)
            m.w32(dest + 4 * axis, static_cast<u32>(std::clamp(m.r32s(dest + 4 * axis), -32768, 32767)));
        m.w16(outline + 28 + 4 * static_cast<u32>(i), m.r16(dest));
        m.w16(outline + 30 + 4 * static_cast<u32>(i), m.r16(dest + 8));
        result = i + 1 < m.r32s(outline + 24);
    }
    m.w32(kTileOriginX, static_cast<u32>(minX)); m.w32(kTileOriginZ, static_cast<u32>(minZ));
    return result;
}

u32 Game::terrain_outline(u32 position, u32 angles, u32 viewport, u32 outline) {
    cover(0x80061614u);
    Frame frame(*this, 0x18);
    terrain_outline_build(viewport, position, angles, outline);
    for (s32 i = 0; i < m.r32s(outline + 24); ++i) {
        const u32 polygon = outline + 4 * static_cast<u32>(i), vertex = outline + 8 * static_cast<u32>(i);
        m.w16(vertex + 0xE0, m.r16(polygon + 28));
        m.w16(vertex + 0xE2, 0u - m.r16(outline + 4));
        m.w16(vertex + 0xE4, m.r16(polygon + 30));
    }
    terrain_outline_place(outline);
    return terrain_outline_grid(outline);
}
}
