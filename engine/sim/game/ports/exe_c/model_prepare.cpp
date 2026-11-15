#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
namespace {
constexpr u32 kDisplayStride = 16, kSubmeshStride = 56, kAircraftStride = 160, kStaticStride = 20;
constexpr u32 kSubmeshCallbacks = 0x800B2C48u, kAircraftCallbacks = 0x800B2C60u;
}

u32 Game::prepare_model_entries(u32 models, u32 display, u32 submeshes, u32 selectedAircraft,
    u32 otherAircraft, u32 statics, u32 hitList) {
    cover(0x80067494u);
    Frame frame(*this, 0x98);
    const u32 world = m.r32(g::kWorld), view = m.r32(world + 40);
    const s32 projection = m.r16s(view + 36), nearDistance = 12 * projection;
    s32 halfWidth = m.r16s(view + 32) >> 1, halfHeight = m.r16s(view + 34) >> 1;
    const s32 lod = player_settings ? 1 << option(&Settings::view_distance) : 1;
    const u32 objectMatrix = frame.at(0x10), cockpitAngles = frame.at(0x30);
    m.w32(frame.at(0x98), models);
    m.w32(frame.at(0x48), static_cast<u32>(projection)); m.w32(frame.at(0x44), static_cast<u32>(nearDistance));
    m.w32(frame.at(0x38), view + 4); m.w32(frame.at(0x3C), world + 76); m.w32(frame.at(0x40), world + 108);
    m.w32(frame.at(0x4C), static_cast<u32>(halfWidth)); m.w32(frame.at(0x50), static_cast<u32>(halfHeight));
    if (player_settings) {
        halfWidth += halfWidth / 2;
        halfHeight += halfHeight / 2;
    }
    auto projectOrigin = [&](s32 x, s32 y, s32 z) {
        for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(world + 76 + 4 * word));
        gte.mtc2(0, (static_cast<u32>(x) & 0xFFFFu) | (static_cast<u32>(y) << 16));
        gte.mtc2(1, static_cast<u32>(z)); gte.op(0x180001u);
    };
    auto storeTranslation = [&](u32 entry) {
        for (u32 axis = 0; axis < 3; ++axis) m.w32(entry + 4 * axis, gte.mfc2(25 + axis));
    };
    auto composeRotation = [&](u32 output) {
        for (u32 column = 0; column < 3; ++column) {
            for (u32 row = 0; row < 3; ++row) gte.mtc2(9 + row, m.r16(objectMatrix + 2 * column + 6 * row));
            gte.op(0x49E012u);
            for (u32 row = 0; row < 3; ++row) m.w16(output + 2 * column + 6 * row, gte.mfc2(9 + row));
        }
    };
    auto rememberCallLocals = [&]() {
        m.w32(frame.at(0x58), view); m.w32(frame.at(0x5C), statics + 12);
        m.w32(frame.at(0x64), submeshes); m.w32(frame.at(0x68), display);
    };
    auto screenVisible = [&](s32 depth, s32 radius) {
        if (depth < -radius) return false;
        const u32 screen = gte.mfc2(14);
        s32 margin = static_cast<s32>(wrap::div(wrap::mul(radius, projection), std::max(1, depth)).lo);
        if (wrap::mul(radius, 16) < margin) margin = wrap::mul(radius, 4);
        return wrap::add(halfWidth, margin) >= std::abs(static_cast<s16>(screen))
            && wrap::add(halfHeight, margin) >= std::abs(static_cast<s32>(screen) >> 16);
    };
    auto appendHit = [&]() {
        const u32 entity = m.r32(display + 12);
        if (!entity) return;
        const u32 count = m.r32(hitList), screen = gte.mfc2(14), out = hitList + 8 * count;
        m.w32(out + 4, entity); m.w16(out + 8, screen); m.w16(out + 10, screen >> 16);
        m.w32(hitList, count + 1);
    };
    while (m.r8(display) != 255) {
        const u32 position = m.r32(display + 8);
        const s32 dx = wrap::sub(m.r32s(position), m.r32s(view + 4));
        const s32 dy = wrap::sub(m.r32s(position + 4), m.r32s(view + 8));
        const s32 dz = wrap::sub(m.r32s(position + 8), m.r32s(view + 12));
        const s32 distance = std::abs(dx >> 10) + std::abs(dy >> 10) + std::abs(dz >> 10);
        if (distance <= 139288) {
            const u32 kind = m.r8(display + 4);
            if (kind & 15) {
                const u32 thrust = m.r32(0x800BD090u);
                m.w32(frame.at(0x6C), thrust);
                const u32 modelIndex = 4 * m.r8(display) + (wrap::mul(nearDistance, 4 * lod) < distance ? 2u
                                                          : wrap::mul(nearDistance, 2 * lod) < distance ? 1u : 0u);
                const u32 model = m.r32(m.r32(models + 12) + 4 * modelIndex);
                const u32 detail = nearDistance < distance ? 0u : (nearDistance >> 1) < distance ? 1u : 2u;
                const u32 shift = 2 - detail, vertices = m.r32(model + 4 + 4 * detail);
                projectOrigin((dx >> 10) >> shift, (dy >> 10) >> shift, (dz >> 10) >> shift);
                if (kind & 9) {
                    m.w8(submeshes, m.r8(display + 1)); m.w16(submeshes + 2, m.r16(display + 2));
                    m.w32(submeshes + 8, detail); m.w32(submeshes + 16, model); m.w32(submeshes + 12, vertices);
                    m.w32(submeshes + 4, m.r32(models + 12) + 4 * modelIndex);
                    m.w32(submeshes + 52, m.r32(kSubmeshCallbacks + 4 * m.r8(display + 6)));
                    storeTranslation(submeshes + 40);
                }
                const bool selected = (kind & 2) != 0;
                const u32 aircraft = selected ? selectedAircraft : otherAircraft;
                m.w32(aircraft + 152, selected ? thrust : 0);
                m.w32(aircraft + 156, selected ? m.r32(kAircraftCallbacks + 4 * m.r8(display + 7)) : 0);
                const u32 parts = m.r32(m.r32(models + 12) + 12);
                for (u32 part = 0; part < 2; ++part) {
                    const u32 header = m.r32(parts + 4 * part);
                    m.w32(aircraft + 4 + 4 * part, m.r32(header + 4 + 4 * detail));
                    m.w32(aircraft + 24 + 4 * part, header);
                }
                m.w32(aircraft + 32, m.r32(parts + 8)); m.w32(aircraft + 36, m.r32(parts + 28)); m.w32(aircraft + 40, m.r32(parts + 36));
                if (selected) {
                    for (u32 control = 0; control < 10; ++control) {
                        const u32 stream = m.r32(m.r32(parts + 8 + 4 * control) + 4 + 4 * detail);
                        m.w32(aircraft + 44 + 4 * control, stream);
                        if (control == 0) m.w32(aircraft + 12, stream);
                        if (control == 5) m.w32(aircraft + 16, stream);
                        if (control == 7) m.w32(aircraft + 20, stream);
                    }
                } else {
                    const u32 front = m.r32(m.r32(parts + 8) + 4 + 4 * detail);
                    const u32 middle = m.r32(m.r32(parts + 28) + 4 + 4 * detail);
                    m.w32(aircraft + 20, m.r32(m.r32(parts + 36) + 4 + 4 * detail));
                    m.w32(aircraft + 12, front); m.w32(aircraft + 16, middle);
                }
                m.w32(aircraft, detail); storeTranslation(aircraft + 108);
                const s32 depth = static_cast<s32>(gte.mfc2(27)) >> detail, radius = m.r16s(model + 2) >> shift;
                if (depth >= -radius) {
                    if (selected || (kind & 4)) m.w32(aircraft + 84, static_cast<u32>(depth));
                    if (screenVisible(depth, radius)) {
                        rememberCallLocals(); rot_matrix_yxz(position + 16, objectMatrix);
                        appendHit();
                        if (kind & 9) {
                            composeRotation(submeshes + 20); submeshes += kSubmeshStride;
                        } else {
                            if (selected && m.r8(display + 5)) {
                                const s32 damp = 4096 - std::min(4096, 4 * (std::abs(m.r16s(view + 180)) + std::abs(m.r16s(view + 182))));
                                const s32 pitchCorrection = wrap::mul(m.r16s(thrust + 2) >> 4, damp) / 4096;
                                const s32 yawCorrection = wrap::mul(m.r16s(thrust + 4) >> 6, damp) / 4096;
                                const s32 rollCorrection = wrap::mul(m.r16s(thrust) >> 3, damp) / 4096;
                                m.w16(cockpitAngles, m.r16(view + 180) - static_cast<u32>(pitchCorrection));
                                m.w16(cockpitAngles + 2, m.r16(view + 182) - static_cast<u32>(yawCorrection));
                                rememberCallLocals();
                                m.w16(cockpitAngles + 4, m.r16(view + 184) - static_cast<u32>(rollCorrection));
                                rot_matrix_yxz_t(cockpitAngles, aircraft + 88);
                                for (u32 axis = 0; axis < 3; ++axis)
                                    m.w32(aircraft + 108 + 4 * axis, static_cast<u32>(-m.r16s(view + 44 + 2 * axis) >> (6 - detail)));
                            } else composeRotation(aircraft + 88);
                            for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(world + 108 + 4 * word));
                            composeRotation(aircraft + 120);
                            if (selected) selectedAircraft += kAircraftStride; else otherAircraft += kAircraftStride;
                        }
                    }
                }
            } else {
                projectOrigin(dx >> 12, dy >> 12, dz >> 12);
                m.w8(statics, m.r8(display + 1)); m.w16(statics + 2, m.r16(display + 2));
                const u32 modelRef = m.r32(models + 20) + 4 * m.r8(display), model = m.r32(modelRef);
                m.w32(statics + 4, modelRef); m.w32(statics + 16, model);
                for (u32 axis = 0; axis < 3; ++axis) m.w16(statics + 8 + 2 * axis, gte.mfc2(25 + axis));
                if (screenVisible(static_cast<s32>(gte.mfc2(27)), m.r16s(model + 2))) {
                    if (far_statics_cut_ && far_tile_depth(position) >= static_cast<double>(far_statics_cut_) - 64.0)
                        far_statics_.push_back({position, modelRef, m.r8(display + 1), m.r16(display + 2)});
                    else {
                        statics += kStaticStride;
                        m.w32(frame.at(0xAC), statics);
                    }
                    appendHit();
                }
            }
        }
        display += kDisplayStride;
    }
    m.w32(submeshes + 8, 0xFFFFFFFFu); m.w32(selectedAircraft, 0xFFFFFFFFu); m.w32(otherAircraft, 0xFFFFFFFFu);
    m.w32(statics + 16, 0);
    return 0xFFFFFFFFu;
}

u32 Game::draw_model_pass(u32 models, u32 orderTable, u32 packets, u32 hitList) {
    cover(0x80068334u);
    Frame frame(*this, 0x1AD0);
    const u32 display = frame.at(0x20), submeshes = frame.at(0x620), selected = frame.at(0x1458);
    const u32 other = frame.at(0x1638), statics = frame.at(0x1818);
    m.w32(hitList, 0);
    display_ground_list(display_aircraft_list(display));
    m.w32(frame.at(0x18), hitList); m.w32(frame.at(0x10), other); m.w32(frame.at(0x14), statics);
    prepare_model_entries(models, display, submeshes, selected, other, statics, hitList);
    constexpr u32 work = 0x1F800000u;
    m.w32(work, orderTable); m.w32(work + 4, packets);
    animate_models(statics, work); animate_submeshes(submeshes, work); animate_aircraft(selected, work);
    set_rot_matrix(m.r32(g::kWorld) + 76);
    terrain_models(statics, work); draw_submeshes(submeshes, work);
    m.w16(work + 1012, m.r16(m.r32(g::kWorld) + 12));
    draw_aircraft_models(selected, work); draw_aircraft_models(other, work);
    return m.r32(work + 4);
}
}
