#include "core/wrap.hpp"
#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kStreamPlayer = 0x800BD054u;
constexpr u32 kMovieOptions = 0x800BCDA0u;

void projectTriple(Memory& memory, Gte& gte, u32 vertices) {
    for (u32 vertex = 0; vertex < 3; ++vertex) gte.mtc2(2 * vertex, memory.r32(vertices + 4 * vertex));
    const u32 firstDepths = memory.r32(vertices + 12);
    gte.mtc2(5, memory.r32(vertices + 16));
    gte.mtc2(1, firstDepths); gte.mtc2(3, static_cast<u32>(static_cast<s32>(firstDepths) >> 16));
    gte.op(0x280030u);
}
}

u32 Game::stream_video_request() {
    cover(0x80058800u);
    Frame frame(*this, 0x20);
    const u32 flags = m.r32(m.r32(kMovieOptions) + 4) ? 0x3000 : 0x2000;
    const u32 player = m.r32(kStreamPlayer);
    m.w32(frame.at(0x10), 0x80058854u);
    return kcd_request_video(m.r32(player + 0xCC), m.r32(player + 0xD4), m.r32(player + 0xD0), flags, 0x80058854u);
}

u32 Game::stream_video_complete() {
    cover(0x80058854u);
    m.w8(m.r32(kStreamPlayer) + 0xA0, 1);
    return 1;
}

u32 Game::stream_video_reset() {
    cover(0x80058864u);
    Frame frame(*this, 0x18);
    cd_reset(0, 0);
    return 1;
}

u32 Game::stream_video_ready() {
    cover(0x80058888u);
    return m.r8(m.r32(kStreamPlayer) + 0x9E) == 0;
}

u32 Game::terrain_project_model(u32 vertices, u32 origin, u32 output) {
    cover(0x800616B0u);
    gte.mtc2(0, m.r32(origin)); gte.mtc2(1, m.r32(origin + 4));
    gte.op(0x480012u);
    s32 remaining = m.r32s(vertices);
    const u32 x = gte.mfc2(25), y = gte.mfc2(26), z = gte.mfc2(27);
    gte.ctc2(5, x); gte.ctc2(6, y); gte.ctc2(7, z);
    if (static_cast<s32>(z) >= 28673) return 0;
    vertices += 4;
    u32 last = output;
    do {
        projectTriple(m, gte, vertices); vertices += 20;
        remaining = wrap::sub(remaining, 3);
        for (u32 i = 0; i < 3; ++i) {
            m.w32(output + 16 * i, gte.mfc2(12 + i));
            m.w32(output + 16 * i + 4, gte.mfc2(17 + i));
            m.w32(output + 16 * i + 8, gte.mfc2(8));
        }
        last = output; output += 48;
    } while (remaining > 0);
    return last;
}

u32 Game::model_project_screens(u32 vertices, u32 work) {
    cover(0x80062828u);
    s32 remaining = m.r32s(vertices);
    vertices += 4;
    u32 screen = work, depth = work + 0x294, result;
    do {
        projectTriple(m, gte, vertices); vertices += 20;
        remaining = wrap::sub(remaining, 3);
        for (u32 i = 0; i < 3; ++i) m.w32(screen + 4 * i, gte.mfc2(12 + i));
        for (u32 i = 0; i < 3; ++i) {
            result = gte.mfc2(17 + i); m.w16(depth + 2 * i, result);
        }
        screen += 12; depth += 6;
    } while (remaining > 0);
    return result;
}

u32 Game::weapon_set_position(u32 weapon, u32 position) {
    cover(0x80069018u);
    u32 value = 0;
    for (u32 axis = 0; axis < 3; ++axis) {
        value = m.r32(position + 4 * axis); m.w32(weapon + 0x48 + 4 * axis, value);
    }
    return value;
}

void Game::weapon_unused_callback() {
    cover(0x80069FC0u);
}
}
