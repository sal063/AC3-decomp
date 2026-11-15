#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kCounts = 0x800BD074u, kPackets = 0x800BD078u;
constexpr u32 kSecondaryColor = 0x800C6D68u;
constexpr u32 kViewScreen = 0x800C6D74u, kViewDepth = 0x800C6D78u;
}

u32 Game::hud_packets_link() {
    cover(0x80063FCCu);
    Frame frame(*this, 0x18);
    const u32 ot = m.r32(m.r32(g::kWorld) + 16);
    struct Group { u32 countOffset, packetOffset, stride; };
    constexpr std::array<Group, 7> groups = {{{8, 8, 24}, {12, 392, 28}, {16, 504, 40},
        {20, 984, 16}, {24, 2008, 20}, {28, 2168, 40}, {32, 2208, 20}}};
    for (const auto& group : groups) {
        const u32 counts = m.r32(kCounts);
        if (group.countOffset == 8 && m.r32(counts + 8) >= 16) {
            continue;
        }
        const u32 buffer = m.r32(kPackets);
        for (s32 i = 0; i < m.r32s(counts + group.countOffset); ++i)
            ot_link(ot, buffer + group.packetOffset + static_cast<u32>(i) * group.stride);
    }
    if (m.r32(m.r32(kCounts) + 36) == 1) ot_link(ot, m.r32(kPackets) + 2368);
    ot_link(ot, m.r32(kPackets));
    return m.r32(ot);
}

void Game::hud_view_marker(u32 hud) {
    cover(0x8006447Cu);
    Frame frame(*this, 0x78);
    const u32 matrix = frame.at(0x10), controlMatrix = frame.at(0x30);
    const u32 angles = frame.at(0x50), forward = frame.at(0x58), screen = frame.at(0x60);
    const u32 fog = frame.at(0x68), flag = frame.at(0x6C);
    m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    set_trans_matrix(matrix);
    m.w16(forward, 0); m.w16(forward + 2, 0); m.w16(forward + 4, 256);
    rot_matrix_yxz_t(m.r32(hud + 16), matrix);
    set_rot_matrix(matrix);
    const s32 depth = rot_trans_pers(forward, screen, fog, flag);
    m.w32(kViewScreen, m.r32(screen)); m.w32(kViewDepth, static_cast<u32>(depth));
    m.w16(angles, 0u - m.r16(m.r32(hud + 12)));
    m.w16(angles + 4, 0);
    m.w16(angles + 2, 0u - m.r16(m.r32(hud + 12) + 2));
    if (!m.r16(angles) && !m.r16(angles + 2)) return;
    rot_matrix_yxz_t(angles, controlMatrix);
    mul_matrix2(controlMatrix, matrix);
    set_rot_matrix(matrix);
    if (rot_trans_pers(forward, screen, fog, flag) <= 0) return;
    u32 packet = m.r32(kPackets) + 984 + 16 * m.r32(m.r32(kCounts) + 20);
    for (u32 line = 0; line < 2; ++line) {
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, m.r8(kSecondaryColor + component));
        if (!line) {
            m.w16(packet + 8, m.r16(screen) - 2u);
            const u32 counts = m.r32(kCounts);
            m.w16(packet + 12, m.r16(screen) + 2u);
            m.w16(packet + 14, m.r16(screen + 2)); m.w16(packet + 10, m.r16(screen + 2));
            m.w32(counts + 20, m.r32(counts + 20) + 1);
        } else {
            m.w16(packet + 12, m.r16(screen)); m.w16(packet + 8, m.r16(screen));
            const u32 counts = m.r32(kCounts);
            m.w16(packet + 10, m.r16(screen + 2) - 3u); m.w16(packet + 14, m.r16(screen + 2));
            m.w32(counts + 20, m.r32(counts + 20) + 1);
        }
        packet += 16;
    }
}

u32 Game::hud_view_brackets(u32 hud) {
    cover(0x800660F8u);
    Frame frame(*this, 0x20);
    if (m.r32s(kViewDepth) <= 0) return m.r32(kViewDepth);
    const u32 angle = m.r32(hud + 32) & 0x3FFFu;
    const s32 numerator = wrap::mul(static_cast<s32>(gte.cfc2(26)), m.r16s(g::kSinCos + 2 * angle));
    const s32 denominator = m.r16s(g::kSinCos + 2 * (angle + 0x1000));
    if (!denominator) unported(0x80066170u, "view bracket tangent division by zero");
    if (denominator == -1 && numerator == static_cast<s32>(0x80000000u))
        unported(0x80066188u, "view bracket tangent division overflow");
    const s32 spread = wrap::mul(static_cast<s32>(wrap::div(numerator, denominator).lo), 3) >> 2;
    const u32 buffer = m.r32(kPackets), brackets = buffer + 2388;
    const s32 top = wrap::sub(m.r16s(kViewScreen + 2), spread), left = wrap::sub(m.r16s(kViewScreen), spread);
    m.w16(brackets + 10, static_cast<u32>(top)); m.w16(brackets + 18, static_cast<u32>(top));
    m.w16(brackets + 8, static_cast<u32>(wrap::add(left, 19))); m.w16(brackets + 16, static_cast<u32>(left));
    m.w16(brackets + 24, static_cast<u32>(left)); m.w16(brackets + 26, static_cast<u32>(wrap::add(top, 20)));
    const s32 right = wrap::add(m.r16s(kViewScreen), spread);
    m.w16(brackets + 42, static_cast<u32>(top)); m.w16(brackets + 50, static_cast<u32>(top));
    m.w16(brackets + 58, static_cast<u32>(wrap::add(top, 20)));
    m.w16(brackets + 40, static_cast<u32>(wrap::sub(right, 19))); m.w16(brackets + 48, static_cast<u32>(right)); m.w16(brackets + 56, static_cast<u32>(right));
    const s32 bottomLeft = wrap::sub(m.r16s(kViewScreen), spread), bottom = wrap::add(m.r16s(kViewScreen + 2), spread);
    m.w16(brackets + 72, static_cast<u32>(wrap::add(bottomLeft, 19))); m.w16(brackets + 74, static_cast<u32>(bottom));
    m.w16(brackets + 80, static_cast<u32>(bottomLeft)); m.w16(brackets + 82, static_cast<u32>(bottom));
    m.w16(brackets + 88, static_cast<u32>(bottomLeft)); m.w16(brackets + 90, static_cast<u32>(wrap::sub(bottom, 20)));
    const s32 bottomRight = wrap::add(m.r16s(kViewScreen), spread);
    m.w16(brackets + 106, static_cast<u32>(bottom)); m.w16(brackets + 114, static_cast<u32>(bottom));
    m.w16(brackets + 104, static_cast<u32>(wrap::sub(bottomRight, 19))); m.w16(brackets + 112, static_cast<u32>(bottomRight));
    m.w16(brackets + 120, static_cast<u32>(bottomRight)); m.w16(brackets + 122, static_cast<u32>(wrap::sub(bottom, 20)));
    const u32 world = m.r32(g::kWorld);
    m.w32(buffer + 2484, (m.r32(buffer + 2484) & 0xFF000000u) | (m.r32(m.r32(world + 16)) & 0xFFFFFFu));
    const u32 ot = m.r32(world + 16);
    const u32 result = (m.r32(ot) & 0xFF000000u) | (brackets & 0xFFFFFFu);
    m.w32(ot, result);
    return result;
}
u32 Game::hud_target_arrow() {
    cover(0x80065D8Cu);
    Frame frame(*this, 0x30);
    const u32 target = m.r8(g::kCurrentTarget);
    if (target == 255) return 0;
    const u32 relation = m.r32(0x800C6ED0u + 4 * target);
    if (!relation || !(m.r32(relation) & 1)) return 0;
    const bool hasScreenPosition = (m.r32(relation + 32) & 0x20000u) != 0;
    if (m.r16s(relation + 24) < 1024 && !hasScreenPosition) return 0;
    if (hasScreenPosition) {
        const u32 mark = m.r32(0x800C67A0u + 4 * target);
        if (!mark) return 0;
        const s32 x = m.r16s(mark + 8) - 160, y = m.r16s(mark + 10) - 120;
        if (square_root0(wrap::add(wrap::mul(x, x), wrap::mul(y, y))) < 64) return 0;
    }
    const u32 local = frame.at(0x18), points = frame.at(0x10);
    to_local(m.r32(m.r32(g::kWorld) + 40) + 4, slot(target) + 8, local);
    const s32 bearing = ratan2(m.r32s(local), m.r32s(local + 4));
    const s32 x = m.r32s(local), y = m.r32s(local + 4);
    const s32 radius = square_root0(wrap::add(wrap::mul(x, x), wrap::mul(y, y)));
    const u32 elevation = static_cast<u32>(ratan2(radius, m.r32s(local + 8)));
    const s32 height = m.r16s(g::kSinCos + (elevation & 0x7FFEu)) >> 9;
    m.w16(points, 0); m.w16(points + 2, static_cast<u32>(height));
    if (height < 6) m.w16(points + 2, 6);
    const s32 halfWidth = std::max(3, 11 - (height >> 1));
    m.w16(points + 4, static_cast<u32>(halfWidth));
    m.w16(points + 8, static_cast<u32>(-halfWidth));
    const u32 angle = static_cast<u32>(bearing) & 0x3FFFu;
    const s32 sine = m.r16s(g::kSinCos + 2 * angle);
    const s32 cosine = m.r16s(g::kSinCos + 2 * (angle + 0x1000));
    m.w16(points + 6, 0); m.w16(points + 10, 0);
    const s32 originX = (wrap::mul(sine, 3) >> 10) + 160;
    const s32 originY = (wrap::mul(cosine, 3) >> 10) + 120;
    for (u32 i = 0; i < 3; ++i) {
        const u32 point = points + 4 * i;
        const s32 px = m.r16s(point), py = m.r16s(point + 2);
        const s32 rotatedX = wrap::add(wrap::mul(cosine, px), wrap::mul(sine, py)) >> 14;
        const s32 rotatedY = wrap::add(wrap::mul(-sine, px), wrap::mul(cosine, py)) >> 14;
        m.w16(point, static_cast<u32>(wrap::add(rotatedX, originX)));
        m.w16(point + 2, static_cast<u32>(wrap::add(rotatedY, originY)));
    }
    const std::array<u8, 3> dim = {static_cast<u8>(m.r8(kSecondaryColor) >> 2),
        static_cast<u8>(m.r8(kSecondaryColor + 1) >> 2), static_cast<u8>(m.r8(kSecondaryColor + 2) >> 2)};
    const u32 packet = m.r32(kPackets) + 2168 + 40 * m.r32(m.r32(kCounts) + 28);
    for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, dim[component]);
    m.w8(packet + 12, m.r8(kSecondaryColor)); m.w8(packet + 13, m.r8(kSecondaryColor + 1));
    const u8 blue = m.r8(kSecondaryColor + 2);
    for (u32 offset : {20u, 28u})
        for (u32 component = 0; component < 3; ++component) m.w8(packet + offset + component, dim[component]);
    m.w8(packet + 14, blue);
    const u32 baseRight = m.r32(points + 4);
    m.w32(packet + 8, baseRight); m.w32(packet + 16, m.r32(points));
    const u32 baseLeft = m.r32(points + 8);
    m.w32(packet + 32, baseRight); m.w32(packet + 24, baseLeft);
    const u32 counts = m.r32(kCounts);
    m.w32(counts + 28, m.r32(counts + 28) + 1);
    return 0;
}

u32 Game::gun_ring_update() {
    cover(0x800662B4u);
    Frame frame(*this, 0x80);
    constexpr u32 ring = 0x800C79C8u, headAddress = 0x800BD070u, tailAddress = 0x800BD071u;
    const u32 player = slot(0), matrix = frame.at(0x10), forward = frame.at(0x30), rotated = frame.at(0x40);
    rot_matrix_yxz(player + 24, matrix);
    set_rot_matrix(matrix);
    m.w32(forward, 0); m.w32(forward + 4, 0); m.w32(forward + 8, 22544384);
    apply_rot_matrix_lv(forward, rotated);
    if (!(m.r32(m.r32(g::kScene)) & 0x20000000u)) m.w8(headAddress, m.r8(headAddress) + 1u);
    if (m.r8(headAddress) >= 32) m.w8(headAddress, 0);
    const u32 head = m.r8(headAddress), position = ring + 16 * head;
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(position + 4 * axis, m.r32(player + 8 + 4 * axis) + m.r32(rotated + 4 * axis) + 32u * m.r32(player + 56 + 4 * axis));
    const u32 relation = m.r32(0x800C6ED0u + 4 * m.r8(g::kCurrentTarget));
    if (!relation) return 1;
    const s32 numerator = static_cast<s32>(m.r32(relation + 4) << 5), denominator = m.r32s(g::kFarDist);
    if (!denominator) unported(0x800663FCu, "gun ring range division by zero");
    if (denominator == -1 && numerator == static_cast<s32>(0x80000000u))
        unported(0x80066414u, "gun ring range division overflow");
    const s32 delay = std::min(31, static_cast<s32>(wrap::div(numerator, denominator).lo));
    const u32 halfDelay = static_cast<u32>(delay >> 1);
    const u32 textureOffset = (halfDelay & 3) + ((halfDelay & 12) << 4);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    m.w8(tailAddress, (head - static_cast<u32>(delay)) & 31);
    rot_matrix_yxz_t(view + 20, matrix);
    m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    const u32 delayedPosition = ring + 16 * m.r8(tailAddress), point = frame.at(0x50), screen = frame.at(0x58);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w16(point + 2 * axis, static_cast<u32>(wrap::sub(m.r32s(delayedPosition + 4 * axis), m.r32s(view + 4 + 4 * axis)) >> 12));
    set_rot_matrix(matrix); set_trans_matrix(matrix);
    rot_trans_pers(point, screen, frame.at(0x60), frame.at(0x64));
    if (m.r32s(frame.at(0x64)) < 0 || !(m.r32(relation) & 0x20)) return 0;
    const u32 packet = m.r32(kPackets) + 2368;
    m.w8(packet + 4, m.r8(kSecondaryColor + 3)); m.w8(packet + 5, m.r8(kSecondaryColor + 4));
    const u8 blue = m.r8(kSecondaryColor + 5);
    m.w16(packet + 14, textureOffset + 0x7E18); m.w8(packet + 6, blue);
    m.w16(packet + 8, m.r16(screen) - 10u);
    const u32 counts = m.r32(kCounts);
    m.w16(packet + 10, m.r16(screen + 2) - 10u);
    const u32 count = m.r32(counts + 36) + 1;
    m.w32(counts + 36, count);
    return count;
}

u32 Game::hud_ballistic_ticks(s32 height, s32 velocity) {
    cover(0x80066A74u);
    s32 travelled = 0;
    u32 ticks = 0;
    while (travelled < height) {
        velocity = wrap::add(velocity, 669);
        travelled = wrap::add(travelled, velocity);
        ++ticks;
    }
    return ticks;
}

u32 Game::hud_impact_cue(u32 hud) {
    cover(0x80066590u);
    Frame frame(*this, 0x70);
    const u32 player = slot(0), target = m.r8(g::kCurrentTarget), scene = m.r32(g::kScene);
    s32 height = m.r32s(hud + 44);
    if (target != 255 && (m.r16s(scene + 908) == 13 || !(m.r32(g::kSlotFlags + 4 * target) & 0x400u))) {
        const s32 targetHeight = m.r32s(slot(target) + 12);
        if (targetHeight >= m.r32s(player + 12)) height = targetHeight;
    }
    const s32 playerHeight = m.r32s(player + 12), verticalVelocity = m.r32s(player + 60);
    const u32 ticks = hud_ballistic_ticks(playerHeight < height ? wrap::sub(height, playerHeight) : 0,
        wrap::add(verticalVelocity, verticalVelocity >> 1));
    const bool straightVelocity = m.r8(scene + 938) == 9 || (target_is_lockable() && m.r8(m.r32(g::kScene) + 939) == 9);
    const s32 vx = m.r32s(player + 56), vz = m.r32s(player + 64);
    const s32 dx = wrap::mul(straightVelocity ? vx : wrap::add(vx, vx >> 1), static_cast<s32>(ticks));
    m.w32(frame.at(0x40), static_cast<u32>(dx));
    const s32 dz = wrap::mul(straightVelocity ? vz : wrap::add(vz, vz >> 1), static_cast<s32>(ticks));
    m.w32(frame.at(0x48), static_cast<u32>(dz));
    const u32 view = m.r32(m.r32(g::kWorld) + 40), point = frame.at(0x10), screen = frame.at(0x18), matrix = frame.at(0x20);
    m.w16(point, static_cast<u32>(wrap::sub(wrap::add(m.r32s(player + 8), dx), m.r32s(view + 4)) >> 12));
    m.w16(point + 2, static_cast<u32>(wrap::sub(height, m.r32s(view + 8)) >> 12));
    m.w16(point + 4, static_cast<u32>(wrap::sub(wrap::add(m.r32s(player + 16), dz), m.r32s(view + 12)) >> 12));
    rot_matrix_yxz_t(view + 20, matrix);
    m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    set_rot_matrix(matrix); set_trans_matrix(matrix);
    rot_trans_pers(point, screen, frame.at(0x50), frame.at(0x54));
    u32 packet = m.r32(kPackets) + 984 + 16 * m.r32(m.r32(kCounts) + 20), result = 0;
    constexpr std::array<std::array<s32, 4>, 6> lines = {{{-10, -10, 10, 10}, {-10, 10, 10, -10},
        {0, -6, 6, 0}, {6, 0, 0, 6}, {0, 6, -6, 0}, {-6, 0, 0, -6}}};
    for (const auto& line : lines) {
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, m.r8(kSecondaryColor + component));
        m.w16(packet + 8, m.r16(screen) + static_cast<u32>(line[0]));
        m.w16(packet + 10, m.r16(screen + 2) + static_cast<u32>(line[1]));
        const u32 endX = m.r16(screen) + static_cast<u32>(line[2]);
        m.w16(packet + 12, endX);
        const u32 counts = m.r32(kCounts);
        m.w16(packet + 14, m.r16(screen + 2) + static_cast<u32>(line[3]));
        result = m.r32(counts + 20) + 1; m.w32(counts + 20, result);
        packet += 16;
    }
    return result;
}

u32 Game::hud_markers(u32 hud) {
    cover(0x80063E4Cu);
    Frame frame(*this, 0x20);
    hud_buffers_begin();
    if (m.r32(m.r32(hud + 52) + 80) & 0x401u) {
        hud_attitude_if_tilted(hud); hud_horizontal_marks();
        const u32 projectionDistance = read_geom_screen();
        hud_view_marker(hud); hud_pitch_ladder(hud);
        set_geom_screen(120); hud_compass(hud); set_geom_screen(static_cast<s32>(projectionDistance));
        hud_view_brackets(hud);
    }
    if (m.r32(m.r32(g::kWorld) + 4) & 4u) {
        const u32 target = m.r8(g::kCurrentTarget);
        hud_target_arrow();
        if (target != 255 && !(m.r32(slot(target)) & 0x40u)) gun_ring_update();
        if (m.r8(0x800C6D70u) == 1) hud_impact_cue(hud);
    }
    return hud_packets_link();
}
}
