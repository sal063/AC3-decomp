#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>
#include <cstdlib>

namespace ac3::sim {
namespace {
constexpr u32 kView = g::kScratch + 28, kVertex = g::kScratch + 60;
constexpr u32 kMotion = g::kScratch + 92, kScreen = g::kScratch + 140;
constexpr u32 kEffects = 0x800BCF90u, kCameraEntity = 0x800BCEB0u;
struct WeatherPosition { u32 x, y, z; };

void restore_view(Game& game) {
    for (u32 reg = 0; reg < 5; ++reg) game.gte.ctc2(reg, game.m.r32(kView + 4 * reg));
}
WeatherPosition weather_space(Game& game, u32 depth, u32 shift, bool scroll) {
    for (u32 reg = 0; reg < 5; ++reg) game.m.w32(kView + 4 * reg, game.gte.cfc2(reg));
    const u32 world = game.m.r32(g::kWorld);
    for (u32 reg = 0; reg < 5; ++reg) game.gte.ctc2(reg, game.m.r32(world + 44 + 4 * reg));
    game.gte.mtc2(0, 0); game.gte.mtc2(1, depth); game.gte.op(0x486012u);
    auto offset = [&](u32 axis) {
        const s32 camera = wrap::neg(static_cast<s32>(game.m.r32(g::kScratch + 8 + 4 * axis))) >> shift;
        return static_cast<u32>(camera) - game.gte.mfc2(25 + axis);
    };
    WeatherPosition position{offset(0), offset(1), offset(2)};
    if (scroll) position.y += game.m.r32(game.m.r32(g::kScene) + 40) << 7;
    restore_view(game); game.gte.ctc2(7, depth);
    return position;
}
void weather_vertex(Game& game, u32 out, WeatherPosition position, u32 noise, u32 shift, u32 mask) {
    game.m.w16(out, ((position.x + noise) & mask) - ((mask + 1) / 2));
    const s32 y_noise = static_cast<s32>(noise) >> shift, z_noise = y_noise >> shift;
    game.m.w16(out + 2, ((position.y + static_cast<u32>(y_noise)) & mask) - ((mask + 1) / 2));
    game.m.w16(out + 4, ((position.z + static_cast<u32>(z_noise)) & mask) - ((mask + 1) / 2));
}
void load_vertex(Game& game, u32 out, u32 slot = 0) {
    game.gte.mtc2(2 * slot, game.m.r32(out)); game.gte.mtc2(2 * slot + 1, game.m.r32(out + 4));
}
void splice_weather(Game& game, u32 first, u32 end, u32 last_size, u32 ot_offset) {
    const u32 ot = game.m.r32(g::kScratch) + ot_offset;
    for (u32 byte = 0; byte < 3; ++byte) {
        game.m.w8(end - last_size + byte, game.m.r8(ot + byte)); game.m.w8(ot + byte, first >> (8 * byte));
    }
    game.m.w32(g::kScratch + 4, end);
}
}

void Game::weather_streak_mesh(u32 x, u32 y, u32 z, s32 count, u32 velocity_shift, s32 fall) {
    Game& game = *this;
    const WeatherPosition position{x, y, z};
    const u32 first = game.m.r32(g::kScratch + 4), noise = game.m.r32(kEffects);
    weather_vertex(game, kVertex, position, 0, 8, 8191);
    for (s32 streak = 0; streak < count; ++streak) {
        for (u32 vertex = 0; vertex < 3; ++vertex) {
            load_vertex(game, kVertex); const u32 random = game.m.r32(noise + 4 * (3 * streak + vertex));
            game.gte.op(0x180001u); weather_vertex(game, kVertex, position, random, 8, 8191);
            const s32 depth = static_cast<s32>(game.gte.mfc2(19)), fade = std::min(4096, std::abs(depth - 8192));
            const u32 packet = first + 52 * streak + 16 * vertex;
            game.m.w32(packet + 8, game.gte.mfc2(14)); game.gte.mtc2(8, fade); game.gte.op(0x780010u);
            game.m.w32(packet + 12, 0x00030303u); game.m.w32(packet + 4, game.gte.mfc2(22));
        }
    }
    const u32 params = game.m.r32(game.m.r32(g::kWorld) + 40);
    for (u32 axis = 0; axis < 3; ++axis)
        game.m.w16(kVertex + 2 * axis, game.m.r16(g::kScratch + 20 + 2 * axis) - game.m.r16(params + 172 + 2 * axis));
    game.rot_matrix_yxz_t(kVertex, kMotion);
    for (u32 reg = 0; reg < 5; ++reg) game.gte.ctc2(reg, game.m.r32(kMotion + 4 * reg));
    for (u32 axis = 0; axis < 3; ++axis) {
        s32 speed = static_cast<s32>(game.m.r32(params + 156 + 4 * axis)) >> velocity_shift;
        if (axis == 1) speed += fall;
        game.m.w16(kVertex + 2 * axis, speed);
    }
    load_vertex(game, kVertex); game.gte.op(0x486012u);
    game.gte.ctc2(5, game.gte.mfc2(25)); game.gte.ctc2(6, game.gte.mfc2(26));
    game.gte.ctc2(7, game.gte.mfc2(27) + 8192);
    weather_vertex(game, kVertex, position, 0, 8, 8191);
    weather_vertex(game, kVertex + 8, position, game.m.r32(noise), 8, 8191);
    weather_vertex(game, kVertex + 16, position, game.m.r32(noise + 4), 8, 8191);
    for (s32 streak = 0; streak < count; ++streak) {
        for (u32 vertex = 0; vertex < 3; ++vertex) load_vertex(game, kVertex + 8 * vertex, vertex);
        const u32 next_noise = game.m.r32(noise + 4 * (2 + 3 * streak));
        game.gte.op(0x280030u);
        weather_vertex(game, kVertex, position, next_noise, 8, 8191);
        weather_vertex(game, kVertex + 8, position, game.m.r32(noise + 4 * (3 + 3 * streak)), 8, 8191);
        weather_vertex(game, kVertex + 16, position, game.m.r32(noise + 4 * (4 + 3 * streak)), 8, 8191);
        const u32 packet = first + 52 * streak;
        game.m.w32(packet, 0x0C000000u | ((packet + 52) & 0xFFFFFFu));
        for (u32 vertex = 0; vertex < 3; ++vertex) game.m.w32(packet + 16 + 16 * vertex, game.gte.mfc2(12 + vertex));
    }
}

void Game::draw_high_cloud_streaks() {
    cover(0x8008E4F8u);
    Frame frame(*this, 48);
    const WeatherPosition position = weather_space(*this, 8192, 10, false);
    const s32 height = wrap::add(static_cast<s32>(m.r32(g::kScratch + 12)), static_cast<s32>(m.r32(m.r32(kCameraEntity) + 4)));
    const s32 blend = std::clamp(height >> 14, 0, 4096);
    if (blend > 0) {
        gte.ctc2(21, 48); gte.ctc2(22, 0); gte.ctc2(23, 0); gte.mtc2(6, 0x6203135Fu);
        gte.mtc2(8, std::max(0, 4224 - 2 * blend));
        const u32 packet = m.r32(g::kScratch + 4); m.w32(g::kScratch + 4, packet + 16); gte.op(0x780010u);
        const u32 ot = m.r32(g::kScratch) + 4;
        m.w32(packet, m.r32(ot) | 0x03000000u); m.w32(ot, packet & 0xFFFFFFu);
        m.w32(packet + 8, 0xFF88FF60u); m.w32(packet + 12, 0x00F00140u); m.w32(packet + 4, gte.mfc2(22));
    }
    gte.mtc2(6, 0x52534333u); gte.ctc2(21, 3120); gte.ctc2(22, 2864); gte.ctc2(23, 496);
    gte.mtc2(8, blend); gte.op(0x780010u);
    const u32 colour = gte.mfc2(22);
    gte.ctc2(21, 48); gte.ctc2(22, 48); gte.ctc2(23, 48); gte.mtc2(6, colour);
    const u32 first = m.r32(g::kScratch + 4);
    weather_streak_mesh(position.x, position.y, position.z, 64, 9, 0);
    splice_weather(*this, first, first + 64 * 52, 52, 16376);
    restore_view(*this); gte.ctc2(5, 0); gte.ctc2(6, 0); gte.ctc2(7, 0);
}

void Game::draw_rain_streaks() {
    cover(0x8008EB58u);
    Frame frame(*this, 48);
    const u32 descriptor = m.r32(m.r32(g::kWorld) + 616);
    s32 count = m.r8(descriptor + 29); if (!count) return;
    gte.ctc2(21, 48); gte.ctc2(22, 48); gte.ctc2(23, 48); gte.mtc2(6, 0x5225211Du);
    const s32 fade = static_cast<s32>(m.r32(descriptor + 28)) >> 16; gte.mtc2(8, fade);
    if (fade >= 4096) return;
    gte.op(0x780010u);
    const s32 camera_y = static_cast<s32>(m.r32(g::kScratch + 12));
    const s32 height = wrap::add(camera_y, static_cast<s32>(m.r32(m.r32(kCameraEntity) + 4)));
    count = std::min(count, 85);
    const u32 layer = m.r32(descriptor + 12);
    const s32 altitude = height >= 0 ? height >> 8 :
        wrap::sub(static_cast<s16>(layer), static_cast<s32>(((layer >> 17) & 127) * (layer >> 24))) - ((camera_y >> 12) + 2048);
    const u32 colour = gte.mfc2(22); if (altitude >= 4096) return;
    gte.mtc2(8, altitude); gte.mtc2(6, colour);
    if (altitude > 0) {
        gte.op(0x780010u); count -= (count * altitude) / 4096;
        const u32 faded = gte.mfc2(22); if (count <= 0) return;
        gte.mtc2(6, faded);
    }
    m.w32(g::kScratch + 144, count);
    const WeatherPosition position = weather_space(*this, 8192, 8, true);
    const u32 first = m.r32(g::kScratch + 4);
    weather_streak_mesh(position.x, position.y, position.z, count, 8, -128);
    splice_weather(*this, first, first + 52 * count, 52, 112);
    restore_view(*this); gte.ctc2(5, 0); gte.ctc2(6, 0); gte.ctc2(7, 0);
}

void Game::draw_snow_flakes() {
    cover(0x8008F1B8u);
    const u32 descriptor = m.r32(m.r32(g::kWorld) + 616);
    s32 count = m.r8(descriptor + 29); if (!count) return;
    gte.ctc2(21, 0); gte.ctc2(22, 0); gte.ctc2(23, 0); gte.mtc2(6, 0x2E838383u);
    const s32 fade = static_cast<s32>(m.r32(descriptor + 28)) >> 16; gte.mtc2(8, fade);
    if (fade >= 4096) return;
    gte.op(0x780010u);
    const s32 altitude = m.r16s(descriptor + 8) - (static_cast<s32>(m.r32(g::kScratch + 12)) >> 12);
    const u32 colour = gte.mfc2(22); if (altitude >= 4096) return;
    gte.mtc2(6, colour); gte.mtc2(8, altitude);
    if (altitude > 0) {
        gte.op(0x780010u); count -= (count * altitude) / 4096;
        const u32 faded = gte.mfc2(22); if (count <= 0) return;
        gte.mtc2(6, faded);
    }
    gte.ctc2(21, 256); gte.ctc2(22, 256); gte.ctc2(23, 256);
    const WeatherPosition position = weather_space(*this, 14336, 8, true);
    const u32 noise = m.r32(kEffects), first = m.r32(g::kScratch + 4);
    u32 packet = first;
    weather_vertex(*this, kVertex, position, 0, 9, 16383);
    for (s32 flake = 0; flake < count; ++flake) {
        load_vertex(*this, kVertex); const u32 random = m.r32(noise + 4 * flake); gte.op(0x180001u);
        weather_vertex(*this, kVertex, position, random, 9, 16383);
        m.w16(packet + 28, 0x1FF0); m.w32(packet + 12, 0x461610F0u);
        m.w32(packet + 20, 0x003810FFu); m.w16(packet + 36, 0x1FFF);
        const s32 depth = static_cast<s32>(gte.mfc2(19)); m.w32(kScreen, gte.mfc2(14));
        gte.mtc2(8, std::min(4096, std::abs(depth - 7168) >> 2));
        const s32 size = std::max(2, (23686 - depth) >> 12); gte.op(0x780010u);
        const s32 x = m.r16s(kScreen), y = m.r16s(kScreen + 2);
        if (std::abs(x) - size >= 161 || std::abs(y) - size >= 121) continue;
        m.w32(packet, 0x09000000u | ((packet + 40) & 0xFFFFFFu));
        const s32 left = x - (size >> 1), top = y - (size >> 1);
        m.w16(packet + 8, left); m.w16(packet + 24, left);
        m.w16(packet + 16, left + size); m.w16(packet + 32, left + size);
        m.w16(packet + 10, top); m.w16(packet + 18, top);
        m.w16(packet + 26, top + size); m.w16(packet + 34, top + size);
        m.w32(packet + 4, gte.mfc2(22)); packet += 40;
    }
    if (packet != first) splice_weather(*this, first, packet, 40, 112);
    restore_view(*this); gte.ctc2(7, 0);
}

void Game::draw_large_flecks() {
    cover(0x8008F580u);
    Frame frame(*this, 8);
    gte.mtc2(6, 0x2E838383u); gte.ctc2(21, 48); gte.ctc2(22, 48); gte.ctc2(23, 48);
    const WeatherPosition position = weather_space(*this, 9216, 12, false);
    const u32 descriptor = m.r32(m.r32(g::kWorld) + 616), noise = m.r32(kEffects);
    const s32 count = std::max(1, static_cast<s32>(m.r8(descriptor + 28)));
    u32 packet = m.r32(g::kScratch + 4);
    weather_vertex(*this, kVertex, position, 0, 9, 16383);
    for (u32 vertex = 0; vertex < 3; ++vertex) m.w16(kVertex + 8 * vertex + 2, position.y);
    for (s32 flake = 0; flake < count; ++flake) {
        load_vertex(*this, kVertex); const u32 random = m.r32(noise + 4 * flake); gte.op(0x180001u);
        const u32 old_x = m.r16(kVertex), old_z = m.r16(kVertex + 4);
        m.w16(kVertex, ((position.x + random) & 16383) - 8192);
        m.w16(kVertex + 8, old_x); m.w16(kVertex + 12, old_z);
        m.w16(kVertex + 16, old_x + (random & 255) + 800);
        m.w16(kVertex + 20, old_z + ((random >> 16) & 255) + 800);
        const s32 shifted = static_cast<s32>(random) >> 9;
        m.w16(kVertex + 4, ((position.z + static_cast<u32>(shifted)) & 16383) - 8192);
        const s32 depth = static_cast<s32>(gte.mfc2(19)); m.w32(kScreen, gte.mfc2(14));
        if (depth <= 0 || std::abs(m.r16s(kScreen + 2)) >= 161 || std::abs(m.r16s(kScreen)) >= 251) continue;
        const s32 fade = depth < 2192 ? 2 * (2192 - depth) : (depth - 4415) >> 2;
        if (fade >= 4097) continue;
        gte.mtc2(1, m.r32(kVertex + 20)); gte.mtc2(2, m.r32(kVertex + 16));
        gte.mtc2(3, m.r32(kVertex + 12)); gte.mtc2(4, m.r32(kVertex + 16)); gte.mtc2(5, m.r32(kVertex + 20));
        m.w32(packet + 8, m.r32(kScreen)); gte.op(0x280030u);
        if (fade <= 0) m.w32(packet + 4, 0x2E838383u);
        const u32 left = (shifted & 1) ? 0 : 63, right = 63 - left;
        const u32 top = (shifted & 2) ? 128 : 191, bottom = 319 - top;
        m.w8(packet + 12, left); m.w8(packet + 28, left); m.w8(packet + 20, right); m.w8(packet + 36, right);
        m.w8(packet + 13, top); m.w8(packet + 21, top); m.w8(packet + 29, bottom); m.w8(packet + 37, bottom);
        m.w16(packet + 14, 0x4ED6); m.w16(packet + 22, 57);
        const u32 ot = m.r32(g::kScratch) + 4 * std::min(depth >> 2, 4094);
        if (static_cast<s32>(gte.mfc2(19)) <= 0) continue;
        m.w32(packet, m.r32(ot) | 0x09000000u); m.w32(ot, packet & 0xFFFFFFu);
        m.w32(packet + 24, gte.mfc2(12)); m.w32(packet + 16, gte.mfc2(13)); m.w32(packet + 32, gte.mfc2(14));
        if (fade > 0) { gte.mtc2(8, fade); gte.op(0x780010u); m.w32(packet + 4, gte.mfc2(22)); }
        packet += 40;
    }
    m.w32(g::kScratch + 4, packet); restore_view(*this); gte.ctc2(7, 0);
}
}
