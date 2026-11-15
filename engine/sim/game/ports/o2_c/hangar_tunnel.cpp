#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kHangar = 0x80112DA8u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kAngles = 0x80, kPositions = 0x84, kRingVertices = 0x104;
constexpr u32 kCurrentXY = 4, kCurrentDepth = 0x84, kCurrentFog = 0x104;
constexpr u32 kRingMatrix = 0x1BC, kViewMatrix = 0x1DC;
constexpr s32 kSegmentLength = 390;

bool outsideScreen(Memory& m, u32 a, u32 b, u32 c, u32 d) {
    const u32 points[] = {a, b, c, d};
    bool left = true, right = true, above = true, below = true;
    for (u32 point : points) {
        const s32 x = m.r16s(point), y = m.r16s(point + 2);
        left &= x < 0; right &= x > 320;
        above &= y < 0; below &= y > 480;
    }
    return left || right || above || below;
}
}

u32 Game::o2_hangar_tunnel_camera(u32 hangar, u32 view) {
    cover(0x800FF4ECu);
    Frame frame(*this, 56);
    s32 speed = static_cast<s32>(m.r32(hangar + 12) & 511) - 256;
    if (speed < 0) speed = -speed;
    speed += 128;
    const s32 accumulated = static_cast<s16>(m.r16(hangar + 0x72) + speed);
    const s32 advanced = accumulated / kSegmentLength;
    m.w16(hangar + 0x70, speed);
    m.w16(hangar + 0x74, advanced);
    m.w16(hangar + 0x72, accumulated - advanced * kSegmentLength);
    if (advanced) {
        m.w32(hangar + 0x7C, (m.r32(hangar + 0x7C) + static_cast<u32>(advanced)) & 7);
        const u32 tunnelState = m.r32(hangar + 0x14), starState = m.r32(hangar + 0x18);
        if (tunnelState == 1 || tunnelState == 3) m.w32(hangar + 0x20, m.r32(hangar + 0x20) + advanced);
        if (starState == 1 || starState == 3) m.w32(hangar + 0x24, m.r32(hangar + 0x24) + advanced);
        const u32 angles = m.r32(hangar + kAngles), positions = m.r32(hangar + kPositions);
        const u32 anchorX = m.r32(positions + 62 * 16), anchorY = m.r32(positions + 62 * 16 + 4),
                  anchorZ = m.r32(positions + 62 * 16 + 8);
        for (s32 sample = 63; sample >= advanced; --sample) {
            const u32 priorAngle = angles + static_cast<u32>(sample - 1) * 8;
            const u32 angle0 = m.r32(priorAngle), angle1 = m.r32(priorAngle + 4);
            m.w32(priorAngle + 8, angle0); m.w32(priorAngle + 12, angle1);
            const u32 priorPosition = positions + static_cast<u32>(sample - 1) * 16;
            const u32 x = m.r32(priorPosition), y = m.r32(priorPosition + 4), z = m.r32(priorPosition + 8);
            m.w32(priorPosition + 16, x - anchorX);
            m.w32(priorPosition + 20, y - anchorY);
            m.w32(priorPosition + 24, z - anchorZ);
        }
        const u32 newAngles = m.r32(hangar + kAngles), newPositions = m.r32(hangar + kPositions);
        s32 pathAngle = m.r16s(hangar + 0x6C) + 1;
        gte.mtc2(0, 0); gte.mtc2(1, kSegmentLength);
        s32 remaining = advanced;
        do {
            const s32 sine = rsin(pathAngle * 16);
            const u32 angle = newAngles + static_cast<u32>(remaining - 1) * 8;
            m.w16(angle, sine >> 6); m.w16(angle + 2, sine >> 5); m.w16(angle + 4, 0);
            rot_matrix_gte(angle + 8, view + kRingMatrix);
            for (u32 word = 0; word < 5; ++word) gte.ctc2(word, m.r32(view + kRingMatrix + 4 * word));
            const u32 prior = newPositions + static_cast<u32>(remaining) * 16;
            for (u32 component = 0; component < 3; ++component) gte.ctc2(5 + component, m.r32(prior + 4 * component));
            --remaining;
            gte.op(0x480012u);
            const u32 position = newPositions + static_cast<u32>(remaining) * 16;
            for (u32 component = 0; component < 3; ++component) m.w32(position + 4 * component, gte.mfc2(25 + component));
            ++pathAngle;
        } while (remaining > 0);
        m.w16(hangar + 0x6C, pathAngle - 1);
    }
    const s32 distance = m.r16s(hangar + 0x72);
    const u32 positions = m.r32(hangar + kPositions);
    const s32 dx = wrap::sub(m.r32s(positions + 62 * 16), m.r32s(positions + 63 * 16));
    const s32 dy = wrap::sub(m.r32s(positions + 62 * 16 + 4), m.r32s(positions + 63 * 16 + 4));
    const s32 dz = wrap::sub(m.r32s(positions + 62 * 16 + 8), m.r32s(positions + 63 * 16 + 8));
    gte.mtc2(8, (distance * 4096) / kSegmentLength);
    gte.mtc2(9, dx); gte.mtc2(10, dy); gte.mtc2(11, dz);
    m.w16(view + 0x198, 0); m.w16(hangar + 0x8C, 0);
    gte.op(0x198003Du);
    const u32 cameraX = gte.mfc2(25), cameraY = gte.mfc2(26), cameraZ = gte.mfc2(27);
    const u32 behind = m.r32(hangar + kPositions);
    for (u32 component = 0; component < 3; ++component) {
        gte.mtc2(9 + component, m.r32(behind + 42 * 16 + 4 * component));
        gte.ctc2(21 + component, m.r32(behind + 43 * 16 + 4 * component));
    }
    m.w32(hangar + 0x90, cameraX); m.w32(hangar + 0x94, cameraY);
    gte.op(0x980011u);
    m.w32(hangar + 0x98, cameraZ);
    const u32 camera[] = {cameraX, cameraY, cameraZ};
    for (u32 component = 0; component < 3; ++component) {
        m.w32(hangar + 0x60 + 4 * component, 0u - camera[component]);
        m.w32(view + 0x1AC + 4 * component, 0u - camera[component]);
    }
    const s32 lookX = wrap::sub(gte.mfc2(25), cameraX), lookY = wrap::sub(gte.mfc2(26), cameraY),
              lookZ = wrap::sub(gte.mfc2(27), cameraZ);
    const u32 pitch = ratan2_lib(lookY, lookZ);
    m.w16(view + 0x194, pitch); m.w16(hangar + 0x88, pitch);
    const u32 yaw = ratan2_lib(lookX, lookZ);
    m.w16(view + 0x196, yaw); m.w16(hangar + 0x8A, yaw);
    rot_matrix(view + 0x194, hangar + 0x4C);
    transpose_matrix(hangar + 0x4C, hangar + 0x2C);
    apply_matrix_lv(hangar + 0x2C, view + 0x1AC, view + 0x1AC);
    const u32 x = m.r32(view + 0x1AC), y = m.r32(view + 0x1B0), z = m.r32(view + 0x1B4);
    m.w32(hangar + 0x40, x); m.w32(hangar + 0xA0, x);
    m.w32(hangar + 0x44, y); m.w32(hangar + 0xA4, y);
    m.w32(hangar + 0x48, z); m.w32(hangar + 0xA8, z);
    return z;
}

void Game::o2_hangar_project_ring(u32 hangar, u32 view, u32 section, bool bend) {
    for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(view + kViewMatrix + 4 * word));
    for (u32 first = 0; first < 16; first += 3) {
        const u32 count = first == 15 ? 1 : 3;
        for (u32 vertex = 0; vertex < count; ++vertex) {
            const u32 point = m.r32(hangar + kRingVertices) + 8 * (first + vertex);
            if (bend) {
                const s32 time = m.r32s(hangar + 0x20), segment = static_cast<s32>(section) - 1;
                const u32 state = m.r32(hangar + 0x14);
                if (time / 8 == segment) {
                    const s32 extent = wrap::sub(7, wrap::sub(time, static_cast<u32>(segment) * 8));
                    m.w16(point + 4, state == 1 ? wrap::mul(extent, 25000) / 64 : 0);
                } else if (time / 8 == static_cast<s32>(section)) {
                    const s32 reverse = wrap::neg(time), fraction = reverse % 8;
                    m.w16(point + 4, state == 3 ? wrap::mul(fraction, 25000) / 64 : 0);
                }
            }
            gte.mtc2(2 * vertex, m.r32(point));
            gte.mtc2(2 * vertex + 1, m.r32(point + 4));
        }
        gte.op(count == 1 ? 0x180001u : 0x280030u);
        for (u32 vertex = 0; vertex < count; ++vertex)
            m.w32(view + kCurrentXY + 4 * (first + vertex), gte.mfc2(count == 1 ? 14 : 12 + vertex));
        for (u32 vertex = 0; vertex < count; ++vertex)
            m.w32(view + kCurrentDepth + 4 * (first + vertex), gte.mfc2(count == 1 ? 19 : 17 + vertex));
        const u32 fog = gte.mfc2(count == 1 ? 24 : 8);
        for (u32 vertex = count; vertex > 0; --vertex) m.w32(view + kCurrentFog + 4 * (first + vertex - 1), fog);
    }
}

u32 Game::o2_hangar_tunnel_draw(u32 hangar, u32 view, u32 packet) {
    cover(0x800FF95Cu);
    Frame frame(*this, 72);
    const u32 textureX = m.r16(hangar + 0xB0), textureY = m.r16(hangar + 0xB2);
    const u32 v = 224 - ((m.r32(hangar + 0x7C) << 2) & 31), ot = m.r32(hangar + 8);
    m.w32(frame.at(16), textureX); m.w32(frame.at(20), textureY);
    m.w32(frame.at(24), v); m.w32(frame.at(28), ot);
    for (u32 section = 0; section < 7; ++section) {
        rot_matrix_gte(m.r32(m.r32(kHangar) + kAngles) + section * 64, view + kRingMatrix);
        trans_matrix(view + kRingMatrix, m.r32(hangar + kPositions) + section * 128);
        comp_matrix(hangar + 0x2C, view + kRingMatrix, view + kViewMatrix);
        o2_hangar_project_ring(hangar, view, section, true);
        if (section) {
            for (u32 colour = 21; colour <= 23; ++colour) gte.ctc2(colour, 2048);
            for (u32 face = 0; face < 16; ++face) {
                const s32 state = m.r32s(hangar + 0x14), time = m.r32s(hangar + 0x20) / 8,
                          segment = static_cast<s32>(section) - 1;
                if ((state == 1 && time < segment) || (state == 3 && segment < time)) break;
                const u32 next = (face + 1) & 15;
                const u32 previous = view + 0x44 + 4 * face, previousNext = view + 0x44 + 4 * next;
                const u32 current = view + 4 + 4 * face, currentNext = view + 4 + 4 * next;
                if (outsideScreen(m, previous, previousNext, current, currentNext)) continue;
                const u32 a = m.r32(previous), c = m.r32(current), d = m.r32(currentNext);
                gte.mtc2(12, a); gte.mtc2(13, c); gte.mtc2(14, d);
                m.w32(packet + 8, a); m.w32(packet + 24, c);
                gte.op(0x1400006u);
                m.w32(packet + 4, 0x2C838383u);
                m.w32(packet + 32, d); m.w32(packet + 16, m.r32(previousNext));
                if (static_cast<s32>(gte.mfc2(24)) < 0) continue;
                m.w32(packet + 12, (v << 8) | (textureY << 16));
                m.w32(packet + 20, (v << 8) | (textureX << 16) | 31);
                m.w32(packet + 28, (v + 31) << 8); m.w32(packet + 36, ((v + 31) << 8) | 31);
                s32 depth = m.r32s(view + 0xC4 + 4 * face) >> 8;
                if (depth >= 256) depth = 255;
                const u32 bucket = ot + static_cast<u32>(depth) * 4;
                m.w32(packet, m.r32(bucket) | 0x09000000u);
                m.w32(bucket, packet & 0x00FFFFFFu);
                packet += 40;
            }
            const u32 upload = packet;
            m.w32(upload, 0x0C000000u); m.w32(upload + 4, 0xA0000000u);
            m.w32(upload + 48, 0x01000000u);
            m.w32(upload + 12, 0x00010010u); m.w32(upload + 8, 0x00D103F0u);
            const u32 display = m.r32(kDisplay);
            for (u32 component = 0; component < 3; ++component) gte.ctc2(21 + component, m.r8(display + 16 + component) << 4);
            packet += 52;
            for (u32 colour = 0; colour < 16; ++colour) {
                gte.mtc2(28, m.r16s(hangar + 0xBC + 2 * colour));
                gte.mtc2(8, m.r32(view + kCurrentFog));
                gte.op(0x980011u);
                m.w16(upload + 16 + 2 * colour, gte.mfc2(29));
            }
            s32 depth = (m.r32s(view + 0xC4) >> 8) + 5;
            if (depth >= 256) depth = 255;
            const u32 bucket = ot + static_cast<u32>(depth) * 4;
            m.w32(upload, (m.r32(upload) & 0xFF000000u) | (m.r32(bucket) & 0x00FFFFFFu));
            m.w32(bucket, (m.r32(bucket) & 0xFF000000u) | (upload & 0x00FFFFFFu));
        }
        for (u32 vertex = 0; vertex < 16; ++vertex) {
            const u32 point = view + 4 * vertex;
            const u32 xy = m.r32(point + 4), depth = m.r32(point + 0x84), fog = m.r32(point + 0x104);
            m.w32(point + 0x44, xy); m.w32(point + 0xC4, depth); m.w32(point + 0x144, fog);
        }
    }
    return packet;
}

u32 Game::o2_hangar_lights_draw(u32 hangar, u32 view, u32 packet) {
    cover(0x80100304u);
    Frame frame(*this, 64);
    const u32 textureX = m.r16(hangar + 0xB0), textureY = m.r16(hangar + 0xB2),
              scroll = (m.r32(hangar + 0x7C) & 7) * 4, ot = m.r32(hangar + 8);
    m.w32(frame.at(16), textureX); m.w32(frame.at(20), scroll);
    for (u32 band = 0; band < 16; ++band) {
        rot_matrix_gte(m.r32(hangar + kAngles) + (48 + band) * 8, view + kRingMatrix);
        trans_matrix(view + kRingMatrix, m.r32(hangar + kPositions) + (48 + band) * 16);
        comp_matrix(hangar + 0x2C, view + kRingMatrix, view + kViewMatrix);
        o2_hangar_project_ring(hangar, view, band, false);
        if (band) {
            for (u32 colour = 21; colour <= 23; ++colour) gte.ctc2(colour, 2048);
            const u32 v = (((band - 1) & 7) * 4 - scroll) & 31;
            for (u32 face = 0; face < 16; ++face) {
                const u32 state = m.r32(hangar + 0x14);
                const s32 time = wrap::sub(m.r32(hangar + 0x20), 48);
                if ((state == 1 && time < static_cast<s32>(band)) || (state == 3 && static_cast<s32>(band) < time)) break;
                const u32 next = (face + 1) & 15;
                const u32 previous = view + 0x44 + 4 * face, previousNext = view + 0x44 + 4 * next;
                const u32 current = view + 4 + 4 * face, currentNext = view + 4 + 4 * next;
                if (outsideScreen(m, previous, previousNext, current, currentNext)) continue;
                m.w32(packet + 8, m.r32(previous)); m.w32(packet + 16, m.r32(previousNext));
                const u32 c = m.r32(current), d = m.r32(currentNext);
                m.w32(packet + 4, 0x2C838383u);
                m.w32(packet + 12, ((v + 192) << 8) | (textureY << 16));
                m.w32(packet + 28, (v + 199) << 8); m.w32(packet + 36, ((v + 199) << 8) | 31);
                m.w32(packet + 20, ((v + 192) << 8) | (textureX << 16) | 31);
                m.w32(packet + 24, c); m.w32(packet + 32, d);
                s32 depth = m.r32s(view + 0xC4 + 4 * face) >> 8;
                if (depth >= 256) depth = 255;
                const u32 bucket = ot + static_cast<u32>(depth) * 4;
                m.w32(packet, m.r32(bucket) | 0x09000000u); m.w32(bucket, packet & 0x00FFFFFFu);
                packet += 40;
            }
        }
        for (u32 vertex = 0; vertex < 16; ++vertex) {
            const u32 point = view + 4 * vertex;
            const u32 xy = m.r32(point + 4), depth = m.r32(point + 0x84), fog = m.r32(point + 0x104);
            m.w32(point + 0x44, xy); m.w32(point + 0x144, fog); m.w32(point + 0xC4, depth);
        }
    }
    return packet;
}

u32 Game::o2_hangar_stars_draw(u32 hangar, u32 view, u32 packet) {
    cover(0x80100B94u);
    Frame frame(*this, 64);
    const s32 advanced = m.r16s(hangar + 0x74);
    const u32 ot = m.r32(hangar + 8);
    m.w32(frame.at(16), ot);
    if (advanced) {
        for (u32 star = 0; star < 48; ++star) {
            const u32 index = m.r32(hangar + 0xDC) + 2 * star;
            s32 segment = m.r16s(index) + advanced;
            if (segment >= 62) segment %= 62;
            m.w16(index, segment);
        }
    }
    const u32 display = m.r32(kDisplay);
    for (u32 component = 0; component < 3; ++component) gte.ctc2(21 + component, m.r8(display + 16 + component) << 4);
    gte.mtc2(6, 0x2CB4B4B4u);
    m.w32(frame.at(20), view + 4);
    for (u32 star = 0; star < 48; ++star) {
        const u32 state = m.r32(hangar + 0x18);
        const s32 segment = m.r16s(m.r32(hangar + 0xDC) + 2 * star), timer = m.r32s(hangar + 0x24);
        if ((state == 1 && timer < segment) || (state == 3 && segment < timer)) continue;
        const u32 current = m.r32(kHangar);
        rot_matrix(current + 0x88, view + 0x44);
        trans_matrix(view + 0x44, m.r32(m.r32(kHangar) + 0xE0) + 16 * star);
        const s32 index = m.r16s(m.r32(hangar + 0xDC) + 2 * star);
        rot_matrix(m.r32(hangar + kAngles) + static_cast<u32>(index) * 8, view + 0x64);
        trans_matrix(view + 0x64, m.r32(hangar + kPositions) + static_cast<u32>(index) * 16);
        comp_matrix(view + 0x64, view + 0x44, view + 4);
        comp_matrix(hangar + 0x2C, view + 4, view + 0x24);
        for (u32 word = 0; word < 8; ++word) gte.ctc2(word, m.r32(view + 0x24 + 4 * word));
        gte.mtc2(0, 0); gte.mtc2(1, 0);
        gte.op(0x180001u);
        m.w32(packet + 20, 0x000FE02Fu);
        m.w16(packet + 28, 0xEF20); m.w32(packet + 12, 0x343FE020u); m.w16(packet + 36, 0xEF2F);
        const s32 depth = static_cast<s32>(gte.mfc2(19));
        m.w32(view + 0xA0, gte.mfc2(14));
        if (depth <= 0) continue;
        gte.op(0x780010u);
        s32 size = 64000 / depth;
        if (size < 2) size = 2;
        const s32 left = m.r16s(view + 0xA0) - size / 4, right = left + size / 2;
        const s32 top = m.r16s(view + 0xA2) - size / 2, bottom = top + size;
        m.w32(view, depth >> 8);
        if (right < 0 || left > 320 || bottom < 0 || top > 480) continue;
        m.w32(packet + 16, (static_cast<u32>(top) << 16) | static_cast<u32>(right));
        m.w32(packet + 8, (static_cast<u32>(top) << 16) | static_cast<u32>(left));
        m.w32(packet + 24, (static_cast<u32>(bottom) << 16) | static_cast<u32>(left));
        m.w32(packet + 32, (static_cast<u32>(bottom) << 16) | static_cast<u32>(right));
        m.w32(packet + 4, gte.mfc2(22));
        s32 bucketIndex = m.r32s(view);
        if (bucketIndex >= 256) bucketIndex = 255;
        const u32 bucket = ot + static_cast<u32>(bucketIndex) * 4;
        m.w32(packet, m.r32(bucket) | 0x09000000u); m.w32(bucket, packet & 0x00FFFFFFu);
        packet += 40;
    }
    if (m.r32(hangar + 0x18) == 1 && m.r32s(hangar + 0x24) >= 64) {
        m.w32(hangar + 0x24, 0); m.w32(hangar + 0x18, 2);
    }
    if (m.r32(hangar + 0x18) == 3 && m.r32s(hangar + 0x24) >= 64) {
        m.w32(hangar + 0x24, 0); m.w32(hangar + 0x18, 0);
    }
    return packet;
}
}
