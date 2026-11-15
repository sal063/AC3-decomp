#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <array>

namespace ac3::sim {
namespace {
constexpr u32 kCounts = 0x800BD074u, kPackets = 0x800BD078u;
constexpr u32 kColor = 0x800C6D68u, kCompassLetters = 0x800B2C38u, kGlyphWidths = 0x800BCDD4u;
constexpr u32 kScaleScratch = 0x1F800000u;
}

u32 Game::hud_compass(u32 hud) {
    cover(0x80065598u);
    Frame frame(*this, 0x58);
    const u32 matrix = frame.at(0x10), fog = frame.at(0x30), flag = frame.at(0x34);
    rot_matrix_yxz_t(m.r32(m.r32(g::kWorld) + 40) + 20, matrix);
    set_rot_matrix(matrix);
    m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    set_trans_matrix(matrix);
    const u32 pitch = (0u - static_cast<u32>(m.r16s(m.r32(hud + 8)))) & 0x3FFFu;
    const s32 height = (3 * (m.r16s(g::kSinCos + 2 * pitch) >> 2)) / 4;
    m.w16(kScaleScratch + 2, static_cast<u32>(height));
    m.w16(kScaleScratch, 0); m.w16(kScaleScratch + 4, 2048);
    rot_trans_pers(kScaleScratch, kScaleScratch + 36, fog, flag);
    for (u32 heading = 512; heading <= 16384; heading += 512) {
        const u32 angle = heading & 0x3FFFu;
        m.w16(kScaleScratch, static_cast<u32>(m.r16s(g::kSinCos + 2 * angle) >> 3));
        m.w16(kScaleScratch + 4, static_cast<u32>(m.r16s(g::kSinCos + 2 * (angle + 0x1000)) >> 3));
        rot_trans_pers(kScaleScratch, kScaleScratch + 24, fog, flag);
        if (m.r32s(flag) < 0) {
            m.w32(kScaleScratch + 36, m.r32(kScaleScratch + 24));
            continue;
        }
        const u32 packet = m.r32(kPackets) + 984 + 16 * m.r32(m.r32(kCounts) + 20);
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, m.r8(kColor + component));
        m.w32(packet + 8, m.r32(kScaleScratch + 36)); m.w32(packet + 12, m.r32(kScaleScratch + 24));
        const u32 counts = m.r32(kCounts);
        m.w32(counts + 20, m.r32(counts + 20) + 1);
        m.w32(kScaleScratch + 36, m.r32(kScaleScratch + 24));
        hud_compass_label(hud, heading);
    }
    return 0;
}

u32 Game::hud_compass_label(u32 hud, u32 heading) {
    cover(0x800657B0u);
    Frame frame(*this, 0x80);
    const u32 angle = heading & 0x3FFFu;
    if (angle & 0x7FFu) return 0xFFFFFFFFu;
    const bool diagonal = (angle & 0x800u) != 0;
    const u32 width = diagonal ? 192u : 128u;
    const u32 left = (angle - width) & 0x3FFFu, right = (angle + width) & 0x3FFFu;
    const s32 leftX = m.r16s(g::kSinCos + 2 * left) >> 3, leftZ = m.r16s(g::kSinCos + 2 * (left + 0x1000)) >> 3;
    const s32 rightX = m.r16s(g::kSinCos + 2 * right) >> 3, rightZ = m.r16s(g::kSinCos + 2 * (right + 0x1000)) >> 3;
    const s32 centerX = diagonal ? m.r16s(g::kSinCos + 2 * angle) >> 3 : rightX;
    const s32 centerZ = diagonal ? m.r16s(g::kSinCos + 2 * (angle + 0x1000)) >> 3 : rightZ;
    const u32 pitch = (0u - static_cast<u32>(m.r16s(m.r32(hud + 8)))) & 0x3FFFu;
    const s32 bottom = (3 * (m.r16s(g::kSinCos + 2 * pitch) >> 2)) >> 2;
    const s32 top = bottom - (diagonal ? 144 : 196);
    const u32 vertices = frame.at(0x20), screens = frame.at(0x50), fog = frame.at(0x68), flag = frame.at(0x6C);
    const std::array<s32, 6> xs = {leftX, centerX, diagonal ? rightX : leftX, diagonal ? leftX : rightX, centerX, rightX};
    const std::array<s32, 6> zs = {leftZ, centerZ, diagonal ? rightZ : leftZ, diagonal ? leftZ : rightZ, centerZ, rightZ};
    const u32 vertexCount = diagonal ? 6u : 4u;
    for (u32 i = 0; i < vertexCount; ++i) {
        m.w16(vertices + 8 * i, static_cast<u32>(xs[i]));
        m.w16(vertices + 8 * i + 4, static_cast<u32>(zs[i]));
    }
    for (u32 i = 0; i < vertexCount; ++i)
        m.w16(vertices + 8 * i + 2, static_cast<u32>(i < (diagonal ? 3u : 2u) ? top : bottom));
    auto projectThree = [&](u32 first) {
        m.w32(frame.at(0x10), screens + 4 * (first + 1));
        m.w32(frame.at(0x14), screens + 4 * (first + 2));
        m.w32(frame.at(0x18), fog); m.w32(frame.at(0x1C), flag);
        rot_trans_pers3(vertices + 8 * first, vertices + 8 * (first + 1), vertices + 8 * (first + 2),
            screens + 4 * first, screens + 4 * (first + 1), screens + 4 * (first + 2), fog, flag);
    };
    projectThree(0);
    if (diagonal) projectThree(3);
    else rot_trans_pers(vertices + 24, screens + 12, fog, flag);
    u32 packet = m.r32(kPackets) + 504 + 40 * m.r32(m.r32(kCounts) + 16);
    u32 result = 0;
    for (u32 letter = 0; letter < (diagonal ? 2u : 1u); ++letter) {
        m.w16(packet + 22, 54); m.w16(packet + 14, 0x7F98);
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, m.r8(kColor + 3 + component) >> 2);
        const std::array<u32, 4> corners = diagonal ? std::array<u32, 4>{letter, letter + 1, letter + 3, letter + 4} : std::array<u32, 4>{0, 1, 2, 3};
        for (u32 corner = 0; corner < 4; ++corner) m.w32(packet + 8 + 8 * corner, m.r32(screens + 4 * corners[corner]));
        const u32 glyph = kGlyphWidths + 2 * m.r8(kCompassLetters + 2 * (angle >> 11) + letter);
        m.w8(packet + 13, 40); m.w8(packet + 12, m.r8(glyph));
        const u32 uRight = m.r8(glyph) + m.r8(glyph + 1);
        m.w8(packet + 21, 40); m.w8(packet + 20, uRight);
        m.w8(packet + 29, 50); m.w8(packet + 28, m.r8(glyph));
        m.w8(packet + 36, m.r8(glyph) + m.r8(glyph + 1)); m.w8(packet + 37, 50);
        const u32 counts = m.r32(kCounts);
        result = m.r32(counts + 16) + 1;
        m.w32(counts + 16, result);
        packet += 40;
    }
    return result;
}
u32 Game::hud_pitch_ladder(u32 hud) {
    cover(0x80064D98u);
    Frame frame(*this, 0xB8);
    const u32 matrix = frame.at(0x20), controlMatrix = frame.at(0x50);
    const u32 fog = frame.at(0x70), flag = frame.at(0x74), lastScreen = kScaleScratch + 32;
    m.w32(frame.at(0x7C), lastScreen); m.w32(frame.at(0x80), kScaleScratch + 8);
    const s32 pitch = m.r16s(m.r32(hud + 8));
    m.w32(frame.at(0x84), fog);
    m.w16(kScaleScratch + 26, 0); m.w32(frame.at(0x78), static_cast<u32>(pitch));
    m.w16(kScaleScratch + 24, static_cast<u32>(pitch));
    m.w32(frame.at(0x88), flag);
    m.w16(kScaleScratch + 28, m.r16(m.r32(hud + 8) + 4));
    rot_matrix_yxz_t(kScaleScratch + 24, matrix);
    rot_matrix_yxz_t(m.r32(hud + 16), controlMatrix);
    mul_matrix2(controlMatrix, matrix);
    set_rot_matrix(matrix);
    m.w32(matrix + 28, 0); m.w32(matrix + 24, 0); m.w32(matrix + 20, 0);
    set_trans_matrix(matrix);
    u32 packet = m.r32(kPackets) + 984 + 16 * m.r32(m.r32(kCounts) + 20);
    m.w16(kScaleScratch + 24, static_cast<u32>(227 * (pitch / 227 - 4)));
    auto projectRung = [&]() {
        m.w32(frame.at(0x10), packet + 12); m.w32(frame.at(0x14), lastScreen);
        m.w32(frame.at(0x18), fog); m.w32(frame.at(0x1C), flag);
        rot_trans_pers3(kScaleScratch, kScaleScratch + 8, kScaleScratch + 16, packet + 8, packet + 12, lastScreen, fog, flag);
    };
    auto finishRung = [&](const std::array<u32, 3>& color, bool advance) {
        if (m.r32s(flag) < 0) return;
        for (u32 component = 0; component < 3; ++component) m.w8(packet + 4 + component, color[component]);
        const u32 counts = m.r32(kCounts);
        m.w32(counts + 20, m.r32(counts + 20) + 1);
        if (advance) packet += 16;
    };
    auto setArc = [&](u32 angle) {
        const s32 y = -m.r16s(g::kSinCos + 2 * angle) >> 4;
        const s32 z = m.r16s(g::kSinCos + 2 * (angle + 0x1000)) >> 4;
        for (u32 point : {16u, 8u, 0u}) m.w16(kScaleScratch + point + 2, static_cast<u32>(y));
        for (u32 point : {16u, 8u, 0u}) m.w16(kScaleScratch + point + 4, static_cast<u32>(z));
    };
    for (s32 rung = -4; rung <= 4; ++rung) {
        const s32 weight = rung < 0 ? rung + 5 : 5 - rung;
        const u32 brightness = static_cast<u32>(std::min(255, 16 * weight * weight));
        const std::array<u32, 3> color = {m.r8(kColor) * brightness >> 8,
            m.r8(kColor + 1) * brightness >> 8, m.r8(kColor + 2) * brightness >> 8};
        setArc(m.r16(kScaleScratch + 24) & 0x3FFFu);
        const s32 angle = m.r16s(kScaleScratch + 24);
        if (!angle) {
            const s32 distance = std::max(445, pitch < 0 ? -pitch : pitch);
            m.w16(kScaleScratch, static_cast<u32>(-100 - (distance >> 2)));
            m.w16(kScaleScratch + 8, static_cast<u32>(-100 - (distance >> 1)));
        } else {
            const s32 distance = std::abs(angle - pitch);
            m.w16(kScaleScratch, static_cast<u32>(-200 - (distance >> 4)));
            m.w16(kScaleScratch + 8, static_cast<u32>(-200 - (distance >> 3)));
        }
        m.w16(kScaleScratch + 16, m.r16(kScaleScratch + 8) + 20u);
        projectRung(); finishRung(color, true);
        m.w16(kScaleScratch + 8, 0u - m.r16(kScaleScratch + 8));
        m.w16(kScaleScratch, 0u - m.r16(kScaleScratch));
        m.w16(kScaleScratch + 16, m.r16(kScaleScratch + 8) - 20u);
        projectRung(); finishRung(color, true);
        m.w16(kScaleScratch + 24, m.r16(kScaleScratch + 24) + 227u);
    }
    m.w16(kScaleScratch + 24, static_cast<u32>(pitch));
    m.w16(kScaleScratch, static_cast<u32>(-208)); m.w16(kScaleScratch + 8, static_cast<u32>(-192));
    m.w16(kScaleScratch + 16, static_cast<u32>(-152));
    setArc(static_cast<u32>(pitch) & 0x3FFFu);
    projectRung();
    std::array<u32, 3> color = {m.r8(kColor), m.r8(kColor + 1), m.r8(kColor + 2)};
    finishRung(color, true);
    const u32 leftFlags = m.r32(flag);
    m.w32(frame.at(0x40), m.r32(lastScreen));
    for (u32 point : {0u, 8u, 16u}) m.w16(kScaleScratch + point, 0u - m.r16(kScaleScratch + point));
    projectRung();
    color = {m.r8(kColor), m.r8(kColor + 1), m.r8(kColor + 2)};
    finishRung(color, false);
    m.w32(frame.at(0x48), m.r32(lastScreen));
    if ((leftFlags | m.r32(flag)) & 0x80000000u) return 0;
    u32 digitPacket = m.r32(kPackets) + 2208 + 20 * m.r32(m.r32(kCounts) + 32);
    s32 degrees = 360 * pitch / 16384;
    auto drawDigitPair = [&](s32 digit, s32 xOffset) {
        for (u32 screen : {frame.at(0x40), frame.at(0x48)}) {
            gauge_digit(frame.at(0x10), digit, static_cast<u32>(m.r16s(screen) + xOffset),
                static_cast<u32>(m.r16s(screen + 2) - 3), 3, digitPacket, m.r8(kColor + 9));
            digitPacket += 20;
            const u32 counts = m.r32(kCounts);
            m.w32(counts + 32, m.r32(counts + 32) + 1);
        }
    };
    if (degrees < 0) { drawDigitPair(-1, -9); degrees = -degrees; }
    drawDigitPair((degrees / 10) % 10, -4);
    drawDigitPair(degrees % 10, 1);
    return 0;
}
}
