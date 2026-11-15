#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kModelBytes = 36;
constexpr u32 kFrameCount = 5;
constexpr u32 kCursorOffset = 4;
constexpr u32 kTerrainOriginX = 0x800BD068u;
constexpr u32 kTerrainOriginZ = 0x800BD06Cu;
constexpr u32 kColourAverageMask = 0xFEFEFEFFu;
}

u32 Game::terrain_outline_place(u32 outline) {
    cover(0x80061534u);
    const s32 x = static_cast<s32>(m.r32(outline) + 0x8000u + m.r32(kTerrainOriginX));
    const s32 z = static_cast<s32>(m.r32(outline + 8) + 0x8000u + m.r32(kTerrainOriginZ));
    const u32 tileX = static_cast<u32>(x) & 0x7FFu, tileZ = static_cast<u32>(z) & 0x7FFu;
    m.w32(outline + 60, static_cast<u32>((z >> 11) + 128));
    m.w32(outline + 56, static_cast<u32>((x >> 11) + 128));
    m.w32(outline + 64, tileX - 1024);
    m.w32(outline + 68, tileZ - 1024);
    if (m.r32s(outline + 24) <= 0) return tileZ - 1024;
    for (s32 i = 0; i < m.r32s(outline + 24); ++i) {
        const u32 point = outline + 28 + 4u * static_cast<u32>(i);
        const s32 px = m.r16s(point) + static_cast<s32>(tileX);
        const s32 pz = m.r16s(point + 2) + static_cast<s32>(tileZ);
        m.w16(point, static_cast<u32>(std::clamp(px, -32768, 30719)));
        m.w16(point + 2, static_cast<u32>(std::clamp(pz, -32768, 30719)));
    }
    return 0;
}

u32 Game::vertex_midpoint(u32 out, u32 a, u32 b) {
    cover(0x80061E50u);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w16(out + 2 * axis, static_cast<u32>((m.r16s(a + 2 * axis) + m.r16s(b + 2 * axis)) >> 1));
    auto average = [](u32 left, u32 right) {
        return static_cast<u32>(static_cast<s32>((left & kColourAverageMask) + (right & kColourAverageMask)) >> 1);
    };
    m.w32(out + 8, average(m.r32(a + 8), m.r32(b + 8)));
    const u32 attribute = average(static_cast<u32>(static_cast<s32>(m.r16s(a + 6))),
                                  static_cast<u32>(static_cast<s32>(m.r16s(b + 6))));
    m.w16(out + 6, attribute);
    return attribute;
}

u32 Game::animate_models(u32 entries, u32 work) {
    cover(0x80066C80u);
    Frame f(*this, 0x30);
    for (u32 entry = entries; m.r32(entry + 16); entry += 20) {
        if (!(m.r8(entry) & 4u)) continue;
        const s32 pose = m.r16s(entry + 2);
        const u32 frame = static_cast<u32>(std::clamp(pose >> 12, 0, 4));
        const s32 blend = pose & 0xFFF;
        const u32 frames = m.r32(entry + 4);
        if (!blend) {
            m.w32(entry + 16, m.r32(frames + 4 * frame));
            continue;
        }
        const u32 from = m.r32(frames + 4 * frame);
        const u32 to = m.r32(frames + 4 * std::min(frame + 1, kFrameCount - 1));
        const u32 model = m.r32(work + kCursorOffset);
        m.w32(work + kCursorOffset, model + kModelBytes);
        word_copy(model, from, 9);
        m.w32(model + 4, m.r32(work + kCursorOffset));
        const u32 end = morph_vertices(blend, m.r32(from + 4), m.r32(to + 4), m.r32(work + kCursorOffset));
        m.w32(work + kCursorOffset, end);
        m.w32(entry + 16, model);
    }
    return 0;
}

u32 Game::animate_submeshes(u32 entries, u32 work) {
    cover(0x80066DCCu);
    Frame f(*this, 0x20);
    u32 entry = entries;
    for (; m.r32s(entry + 8) >= 0; entry += 56) {
        if (!(m.r8(entry) & 4u)) continue;
        const u32 pose = m.r16(entry + 2);
        const u32 frame = std::min(pose >> 12, kFrameCount - 1);
        const s32 blend = static_cast<s32>(pose & 0xFFFu);
        const u32 frames = m.r32(entry + 4);
        const u32 from = m.r32(frames + 16 * frame);
        m.w32(entry + 16, from);
        if (!blend) continue;
        const u32 to = m.r32(frames + 16 * std::min(frame + 1, kFrameCount - 1));
        const u32 part = m.r32(entry + 8);
        const u32 verticesOffset = part == 0 ? 4u : part == 1 ? 8u : 12u;
        m.w32(entry + 12, m.r32(work + kCursorOffset));
        const u32 end = morph_vertices(blend, m.r32(from + verticesOffset), m.r32(to + verticesOffset),
                                       m.r32(work + kCursorOffset));
        m.w32(work + kCursorOffset, end);
    }
    return m.r32(entry + 8);
}

u32 Game::morph_pose_controls(s32 x, s32 y, u32 poses, u32 out, u32 work) {
    cover(0x80066EF8u);
    Frame f(*this, 0x28);
    const u32 base = m.r32(poses);
    const u32 poseX = m.r32(poses + (x < 0 ? 8u : 4u));
    const u32 poseY = m.r32(poses + (y < 0 ? 16u : 12u));
    if (x < 0) x = wrap::neg(x);
    if (y < 0) y = wrap::neg(y);
    s32 direction = 2048;
    if (x != y) {
        const bool xDominates = y < x;
        const s32 numerator = static_cast<s32>(static_cast<u32>(xDominates ? y : x) << 11);
        const s32 denominator = xDominates ? x : y;
        if (!denominator) unported(xDominates ? 0x80066F58u : 0x80066F90u, "pose division by zero");
        if (denominator == -1 && static_cast<u32>(numerator) == 0x80000000u)
            unported(xDominates ? 0x80066F70u : 0x80066FA8u, "pose division overflow");
        const s32 ratio = static_cast<s32>(wrap::div(numerator, denominator).lo);
        direction = xDominates ? ratio : wrap::sub(4096, ratio);
    }
    const s32 scale = direction < 2048 ? wrap::sub(4096, direction) : direction;
    const s32 strength = std::min(wrap::mul(wrap::add(x, y), scale) >> 12, 4096);
    m.w32(f.at(0x10), poseY);
    m.w32(f.at(0x14), out);
    m.w32(f.at(0x18), work);
    return morph_vertices_twice(direction, strength, base, poseX, poseY, out, work);
}

u32 Game::animate_aircraft(u32 entries, u32 work) {
    cover(0x80067088u);
    Frame f(*this, 0x30);
    u32 entry = entries;
    for (; m.r32s(entry) >= 0; entry += 160) {
        const u32 controls = m.r32(entry + 152);
        m.w32(entry + 12, m.r32(work + kCursorOffset));
        m.w32(f.at(0x10), work + 16);
        u32 end = morph_pose_controls(m.r16s(controls), m.r16s(controls + 2), entry + 44,
                                      m.r32(work + kCursorOffset), work + 16);
        m.w32(work + kCursorOffset, end);
        m.w32(entry + 16, end);
        end = morph_pose_pair(m.r16s(controls + 8), entry + 64, m.r32(work + kCursorOffset));
        m.w32(work + kCursorOffset, end);
        m.w32(entry + 20, end);
        end = morph_pose_signed(m.r16s(controls + 4), entry + 72, m.r32(work + kCursorOffset));
        m.w32(work + kCursorOffset, end);
    }
    return m.r32(entry);
}
}
