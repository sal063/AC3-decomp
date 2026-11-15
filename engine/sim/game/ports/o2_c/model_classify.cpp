#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
u32 Game::o2_model_classify(u32 view, u32 objects, u32 submeshes, u32 aircraft,
                          u32 overlays, u32 models, u32 captions) {
    cover(0x80107518u);
    Frame frame(*this, 144);
    m.w32(frame.at(144), view);
    const u32 world = m.r32(g::kWorld), camera = m.r32(world + 40);
    const s32 screen = m.r16s(camera + 36), distanceStep = wrap::mul(screen, 12);
    const s32 halfWidth = m.r16s(camera + 32) >> 1, halfHeight = m.r16s(camera + 34) >> 1;
    const u32 rotation = frame.at(16), jitter = frame.at(48);
    auto absolute = [](s32 value) { return value < 0 ? wrap::neg(value) : value; };
    auto loadRotation = [&](u32 matrix) {
        for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(matrix + 4 * word));
    };
    auto composeRotation = [&](u32 destination) {
        for (u32 column = 0; column < 3; ++column) {
            for (u32 row = 0; row < 3; ++row) gte.mtc2(9 + row, m.r16(rotation + 6 * row + 2 * column));
            gte.op(0x49E012u);
            for (u32 row = 0; row < 3; ++row) m.w16(destination + 6 * row + 2 * column, gte.mfc2(9 + row));
        }
    };
    for (u32 entry = objects; m.r8(entry) != 255; entry += 16) {
        const u32 position = m.r32(entry + 8);
        s32 delta[3];
        s32 distance = 0;
        for (u32 component = 0; component < 3; ++component) {
            delta[component] = wrap::sub(m.r32s(position + 4 * component), m.r32s(camera + 4 + 4 * component)) >> 10;
            distance = wrap::add(distance, absolute(delta[component]));
        }
        if (distance > 0x2C000) continue;
        const u32 flags = m.r8(entry + 4);
        if (!(flags & 15)) continue;
        const u32 bank = 4 * m.r8(entry) + (distance > wrap::mul(distanceStep, 4) ? 2u :
                                          distance > wrap::mul(distanceStep, 2) ? 1u : 0u);
        const u32 tableEntry = m.r32(view + 12) + 4 * bank, descriptor = m.r32(tableEntry);
        const u32 lod = distance > distanceStep ? 0u : distance > (distanceStep >> 1) ? 1u : 2u;
        const u32 mesh = m.r32(descriptor + 4 + 4 * lod);
        const u32 shift = 2 - lod;
        loadRotation(world + 0x4C);
        gte.mtc2(0, (static_cast<u32>(delta[0] >> shift) & 0xFFFFu) |
                    (static_cast<u32>(delta[1] >> shift) << 16));
        gte.mtc2(1, delta[2] >> shift); gte.op(0x180001u);
        if (flags & 9) {
            m.w8(submeshes, m.r8(entry + 1)); m.w16(submeshes + 2, m.r16(entry + 2));
            m.w32(submeshes + 8, lod); m.w32(submeshes + 16, descriptor);
            m.w32(submeshes + 12, mesh); m.w32(submeshes + 52, 0); m.w32(submeshes + 4, tableEntry);
            for (u32 component = 0; component < 3; ++component) m.w32(submeshes + 40 + 4 * component, gte.mfc2(25 + component));
        } else {
            m.w32(overlays + 152, 0);
            const u32 destination = (flags & 2) ? aircraft : overlays;
            const u32 bundle = m.r32(m.r32(view + 12) + 12);
            for (u32 part = 0; part < 2; ++part) {
                const u32 partDescriptor = m.r32(bundle + 4 * part);
                m.w32(destination + 4 + 4 * part, m.r32(partDescriptor + 4 + 4 * lod));
                m.w32(destination + 24 + 4 * part, partDescriptor);
            }
            constexpr u32 special[] = {8, 28, 36};
            for (u32 part = 0; part < 3; ++part) m.w32(destination + 32 + 4 * part, m.r32(bundle + special[part]));
            if (flags & 2) {
                for (u32 part = 0; part < 10; ++part) {
                    const u32 selectedMesh = m.r32(m.r32(bundle + 8 + 4 * part) + 4 + 4 * lod);
                    m.w32(destination + 44 + 4 * part, selectedMesh);
                    if (part == 0) m.w32(destination + 12, selectedMesh);
                    if (part == 5) m.w32(destination + 16, selectedMesh);
                    if (part == 7) m.w32(destination + 20, selectedMesh);
                }
            } else for (u32 part = 0; part < 3; ++part)
                m.w32(destination + 12 + 4 * part, m.r32(m.r32(bundle + special[part]) + 4 + 4 * lod));
            m.w32(destination, lod);
            for (u32 component = 0; component < 3; ++component) m.w32(destination + 108 + 4 * component, gte.mfc2(25 + component));
        }
        const s32 depth = static_cast<s32>(gte.mfc2(27)) >> lod;
        const s32 radius = m.r16s(descriptor + 2) >> shift;
        if (depth < -radius) continue;
        const u32 screenPoint = gte.mfc2(14);
        if (flags & 2) m.w32(aircraft + 84, depth);
        else if (flags & 4) m.w32(overlays + 84, depth);
        s32 extent = wrap::mul(radius, screen) / (depth > 0 ? depth : 1);
        if (extent > wrap::mul(radius, 16)) extent = wrap::mul(radius, 4);
        const s32 x = static_cast<s16>(screenPoint), y = static_cast<s16>(screenPoint >> 16);
        if (absolute(x) > wrap::add(halfWidth, extent) || absolute(y) > wrap::add(halfHeight, extent)) continue;
        rot_matrix_yxz(position + 16, rotation);
        if (const u32 caption = m.r32(entry + 12)) {
            const u32 count = m.r32(captions), record = captions + 8 * count;
            m.w32(record + 4, caption); m.w16(record + 8, x); m.w16(record + 10, y);
            m.w32(captions, count + 1);
        }
        if (flags & 9) {
            composeRotation(submeshes + 20); submeshes += 56;
        } else if (flags & 2) {
            if (m.r8(entry + 5)) {
                s32 movement = 4 * (absolute(m.r16s(camera + 0xB4)) + absolute(m.r16s(camera + 0xB6)));
                if (movement > 4096) movement = 4096;
                const s32 amount = 4096 - movement;
                const u32 drift = m.r32(0x800BD090u);
                constexpr u32 offsets[] = {2, 4, 0}, shifts[] = {4, 5, 3};
                for (u32 angle = 0; angle < 3; ++angle) {
                    const s32 correction = wrap::mul(m.r16s(drift + offsets[angle]) >> shifts[angle], amount) / 4096;
                    m.w16(jitter + 2 * angle, m.r16(camera + 0xB4 + 2 * angle) - static_cast<u32>(correction));
                }
                rot_matrix_yxz_t(jitter, aircraft + 88);
                for (u32 component = 0; component < 3; ++component)
                    m.w32(aircraft + 108 + 4 * component, -m.r16s(camera + 44 + 2 * component) >> (6 - lod));
            } else composeRotation(aircraft + 88);
            loadRotation(world + 0x6C); composeRotation(aircraft + 120);
            aircraft += 160;
        } else {
            composeRotation(overlays + 88); loadRotation(world + 0x6C); composeRotation(overlays + 120);
            overlays += 160;
        }
    }
    m.w32(submeshes + 8, 0xFFFFFFFFu); m.w32(aircraft, 0xFFFFFFFFu); m.w32(overlays, 0xFFFFFFFFu);
    m.w32(models + 16, 0);
    return models;
}
}
