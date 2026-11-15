#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kWorld = 0x800BCE54u, kObjects = 0x800BCE5Cu;
constexpr u32 kActive = 0x14000u, kCamera = 0x28, kHeightMap = 0x80112DC8u;
}

u32 Game::o2_model_scene_list(u32 output) {
    cover(0x801073B0u);
    const u32 camera = m.r32(m.r32(kWorld) + kCamera);
    const u32 excluded = m.r32(camera + 0xC4), selected = m.r32(camera + 0xC8);
    for (u32 slot = 0; slot < 32; ++slot) {
        const u32 object = m.r32(m.r32(kObjects) + 4 * slot);
        if ((m.r32(object) & kActive) != kActive || slot == excluded) continue;
        u32 category = 1;
        if (m.r8(object + 5) & 1u) {
            category = 4;
            if (slot == selected && (m.r32(m.r32(m.r32(kWorld) + kCamera) + 0x50) & 6u))
                category = 2;
        }
        m.w8(output + 4, category);
        m.w8(output, m.r8(object + 4));
        m.w8(output + 1, m.r8(object + 5));
        m.w32(output + 8, object + 8);
        m.w16(output + 2, m.r16(object + 6));
        m.w32(output + 12, m.r32(object));
        const s32 kind = m.r16s(object + 0x48);
        const u32 material = kind == 43 ? 3 : kind == 35 ? 4 : kind == 36 ? 5 : 0;
        m.w16(output + 6, material);
        m.w8(output + 5, slot == selected ? 1 : 0);
        output += 16;
    }
    m.w8(output, 255);
    return output;
}

u32 Game::o2_model_scene_draw(u32 view, u32 ot, u32 packet, u32 count) {
    cover(0x8010817Cu);
    Frame frame(*this, 0x1740);
    const u32 objects = frame.at(0x20), submeshes = frame.at(0x420);
    const u32 aircraft = frame.at(0x1220), overlays = frame.at(0x1360);
    const u32 models = frame.at(0x14A0);
    m.w32(count, 0);
    o2_model_scene_list(objects);
    m.w32(frame.at(16), overlays);
    m.w32(frame.at(20), models);
    m.w32(frame.at(24), count);
    o2_model_classify(view, objects, submeshes, aircraft, overlays, models, count);
    constexpr u32 scratch = 0x1F800000u;
    m.w32(scratch, ot);
    m.w32(scratch + 4, packet);
    animate_models(models, scratch);
    animate_submeshes(submeshes, scratch);
    animate_aircraft(aircraft, scratch);
    set_rot_matrix(m.r32(kWorld) + 0x4C);
    terrain_models(models, scratch);
    draw_submeshes(submeshes, scratch);
    m.w16(scratch + 0x3F4, m.r16(m.r32(kWorld) + 12));
    draw_aircraft_models(aircraft, scratch);
    draw_aircraft_models(overlays, scratch);
    return m.r32(scratch + 4);
}

u32 Game::o2_hangar_camera_reset(u32 hangar, u32 view) {
    cover(0x801016A4u);
    Frame frame(*this, 40);
    constexpr u32 kAngles = 0x88, kPosition = 0x90, kCameraMatrix = 0x4C;
    constexpr u32 kInverse = 0x2C, kInverseTranslation = 0xA0, kViewTranslation = 0xD8;
    for (u32 offset = kAngles; offset <= 0x98; offset += 4) m.w32(hangar + offset, 0);
    rot_matrix(hangar + kAngles, hangar + kCameraMatrix);
    for (u32 component = 0; component < 3; ++component)
        m.w32(view + kViewTranslation + 4 * component, 0u - m.r32(hangar + kPosition + 4 * component));
    trans_matrix(hangar + kCameraMatrix, view + kViewTranslation);
    for (u32 word = 0; word < 8; ++word)
        m.w32(hangar + kInverse + 4 * word, m.r32(hangar + kCameraMatrix + 4 * word));
    transpose_matrix(hangar + kCameraMatrix, hangar + kInverse);
    apply_matrix_lv(hangar + kInverse, view + kViewTranslation, hangar + kInverseTranslation);
    return trans_matrix(hangar + kInverse, hangar + kInverseTranslation);
}

u32 Game::o2_hangar_points_visible(u32 points) {
    cover(0x80101A0Cu);
    bool visible = false;
    for (u32 point = 0; point < 15; ++point) {
        const u32 position = points + 16 * point;
        const s32 x = m.r32s(position), y = m.r32s(position + 4), z = m.r32s(position + 8);
        if (z < 4096 || z > 32768) continue;
        const s32 halfWidth = z * 160 / 250, halfHeight = z * 240 / 250;
        if (x >= -halfWidth && x <= halfWidth && y >= -halfHeight && y <= halfHeight)
            visible = true;
    }
    return visible ? 1 : 0;
}

u32 Game::o2_aircraft_height_sample(u32 x, u32 y, u32 corner) {
    cover(0x8010417Cu);
    const s32 selectedCorner = static_cast<s16>(corner);
    if (selectedCorner == 1 || selectedCorner == 3) ++x;
    if (selectedCorner == 2 || selectedCorner == 3) ++y;
    const s32 gridX = static_cast<s16>(x), gridY = static_cast<s16>(y);
    const s32 pixel = gridX + 258 * (gridY + 128) + 128;
    const s32 byteOffset = static_cast<s16>(pixel / 2);
    const u32 packed = m.r8(m.r32(kHeightMap) + static_cast<u32>(byteOffset));
    return ((packed >> ((x & 1u) * 4)) & 15u) * 128;
}
}
