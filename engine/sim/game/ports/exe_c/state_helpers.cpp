#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include <algorithm>
#include <bit>

namespace ac3::sim {
namespace {
constexpr u32 kWorld = 0x800BCE54u, kEntities = 0x800BCE5Cu, kScene = 0x800BCE60u;
constexpr u32 kWeaponWork = 0x800BD080u;
constexpr s32 kScaleLimit = 86015;

s32 scaledShort(s32 value, s32 scale) {
    return wrap::mul(value, scale) / 4096;
}
}

void Game::mission_status_clear() {
    cover(0x8005B648u);
    m.w8(0x800BD05Cu, 0);
}

void Game::fog_table(s32 near, s32 far, u32 out) {
    cover(0x8005EAB8u);
    const s32 distance = wrap::sub(far, near);
    for (s32 step = 1; step <= 16; ++step, out += 2) {
        const s32 interpolated = wrap::add(wrap::mul(step, distance) / 16, near) >> 3;
        m.w16(out, static_cast<u32>(std::clamp(interpolated, 0, 4093)));
    }
    m.w16(out, 4094);
}

u32 Game::terrain_draw_modes(u32 orderTable, u32 packets) {
    cover(0x8005EFF0u);
    Frame frame(*this, 0x18);
    for (u32 i = 0; i < 6; ++i) m.w32(frame.at(4 * i), m.r32(0x80024580u + 4 * i));
    u32 indices = m.r32(m.r32(kWorld) + 0x288);
    for (u32 i = 0; i < 16; ++i, packets += 24, indices += 2) {
        const u32 bucket = orderTable + 4u * static_cast<u32>(m.r16s(indices));
        for (u32 word : {0u, 1u, 3u, 4u, 5u, 2u}) m.w32(packets + 4 * word, m.r32(frame.at(4 * word)));
        m.w32(packets, (m.r32(packets) & 0xFF000000u) | (m.r32(bucket) & 0xFFFFFFu));
        m.w32(bucket, (m.r32(bucket) & 0xFF000000u) | (packets & 0xFFFFFFu));
    }
    return packets;
}

u32 Game::weapon_queue_request(u32 first, u32 second, u32 third) {
    cover(0x8006AF58u);
    const u32 work = m.r32(kWeaponWork);
    const s32 count = m.r32s(work + 0x154C);
    if (count >= 8) return 0;
    u32 word = 0;
    while (word < 4 && m.r32(work + 0x33D0 + 4 * word) == 0xFFFFFFFFu) ++word;
    if (word == 4) return 0;
    const u32 bitmap = work + 0x33D0 + 4 * word;
    const u32 bit = static_cast<u32>(std::countr_one(m.r32(bitmap)));
    m.w32(bitmap, m.r32(bitmap) | (1u << bit));
    m.w32(work + 0x154C, static_cast<u32>(count) + 1);
    const u32 request = work + 0x1550 + 16u * static_cast<u32>(count);
    m.w16(request, 32 * word + bit); m.w16(request + 2, 0);
    m.w32(request + 4, first); m.w32(request + 8, second); m.w32(request + 12, third);
    return 1;
}

u32 Game::player_activate_parameter() {
    cover(0x8006D078u);
    Frame frame(*this, 0x18);
    const u32 player = m.r32(m.r32(kEntities));
    m.w32(player, m.r32(player) | 0x1C000u);
    const u32 definition = player_def(m.r16s(player + 0x48));
    m.w16(player + 0x4E, m.r16(definition + 0x84));
    if (m.r32s(m.r32(kScene) + 0x394) >= 2) m.w16(player + 0x4E, 30);
    return 30;
}

u32 Game::distance_scale_vector(s32 distance, u32 vector) {
    cover(0x8006E7E0u);
    const s32 gain = distance > kScaleLimit ? 4096 : distance / 21;
    const s32 x = scaledShort(m.r16s(vector + 4), gain);
    const s32 y = scaledShort(m.r16s(vector + 6), gain);
    const s32 z = scaledShort(m.r16s(vector + 8), gain);
    m.w16(vector + 4, static_cast<u32>(x)); m.w16(vector + 6, static_cast<u32>(y));
    m.w16(vector + 8, static_cast<u32>(z));
    return static_cast<u32>(y);
}

u32 Game::distance_scale_angles(s32 distance, u32 angles) {
    cover(0x8006E874u);
    const s32 gain = distance > kScaleLimit ? 4096 : distance / 21;
    const s32 y = scaledShort(m.r16s(angles + 2), gain);
    const s32 z = scaledShort(m.r16s(angles + 4), gain);
    const s32 x = scaledShort(m.r16s(angles), gain);
    m.w16(angles + 2, static_cast<u32>(y)); m.w16(angles + 4, static_cast<u32>(z));
    m.w16(angles, static_cast<u32>(x));
    return static_cast<u32>(z);
}
}
