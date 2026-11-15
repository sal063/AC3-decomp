#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kEffectsPointer = 0x800BCF90u;
constexpr u32 kSmokePool = 1536, kDebrisPool = 4248, kBillboardPool = 13248;
constexpr u32 kFlags = 0, kPhase = 2, kVelocity = 4, kScale = 10, kPosition = 12;
constexpr u32 kParticleKind = 0, kLife = 2, kParticlePosition = 4;
constexpr u32 kParticleVelocity = 16, kDrag = 22, kStyle = 24, kSize = 26;

u32 fire_debris(Game& game, u32 source, const u32 (&position)[3]) {
    auto& memory = game.m;
    if (!(memory.r32(source + kFlags) & (1u << 13))) return 0;
    if (!(game.rand() & 1)) return 0;
    const u32 debris = game.pool_take(memory.r32(kEffectsPointer) + kDebrisPool, 10, 7);
    if (!debris) return 0;
    s32 velocity[3];
    for (u32 axis = 0; axis < 3; ++axis) velocity[axis] = memory.r16s(source + kVelocity + 2 * axis);
    memory.w16(debris + kParticleKind, 0);
    memory.w16(debris + kLife, (game.rand() & 7) + 20);
    memory.w16(debris + kStyle, 0);
    memory.w16(debris + kSize, (game.rand() & 7) + 12);
    for (u32 axis = 0; axis < 3; ++axis) memory.w32(debris + kParticlePosition + 4 * axis, position[axis] + velocity[axis]);
    for (u32 axis = 0; axis < 3; ++axis)
        memory.w16(debris + kParticleVelocity + 2 * axis, (velocity[axis] >> 1) + static_cast<s32>(game.rand() & 255) - 128);
    memory.w16(debris + kDrag, 392);
    return 392;
}
}

u32 Game::emit_fire_smoke(u32 source) {
    cover(0x80092D90u);
    Frame frame(*this, 48);
    const u32 phase = (m.r32(m.r32(g::kWorld) + 12) + m.r16s(source + kPhase)) & 1;
    if (phase) return phase;
    const s32 scale = m.r16s(source + kScale);
    u32 position[3];
    for (u32 axis = 0; axis < 3; ++axis) {
        const s32 noise = static_cast<s32>(rand() & 255) - 128;
        position[axis] = m.r32(source + kPosition + 4 * axis) + (scale * noise >> 2);
    }
    if ((m.r32(source + kFlags) & (1u << 14)) && (rand() & 3)) {
        const u32 particle = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
        if (particle) {
            m.w16(particle + kLife, 10);
            m.w16(particle + kParticleKind, 0);
            for (u32 axis = 0; axis < 3; ++axis) m.w32(particle + kParticlePosition + 4 * axis, position[axis]);
            m.w16(particle + kDrag, 0);
            m.w16(particle + kStyle, 256);
            m.w16(particle + kSize, scale * static_cast<s32>((rand() & 63) + 58) >> 12);
        }
    }
    const u32 smoke = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
    if (smoke) {
        m.w16(smoke + kParticleKind, static_cast<u32>(-3));
        m.w16(smoke + kLife, 40);
        for (u32 axis = 0; axis < 3; ++axis) m.w32(smoke + kParticlePosition + 4 * axis, position[axis]);
        m.w16(smoke + kDrag, 0);
        m.w16(smoke + kStyle, 528);
        m.w16(smoke + kSize, scale * static_cast<s32>((rand() & 127) + 76) >> 12);
    }
    return fire_debris(*this, source, position);
}

u32 Game::emit_hot_puff(u32 source) {
    cover(0x80092B80u);
    Frame frame(*this, 48);
    const s32 scale = m.r16s(source + kScale);
    u32 position[3];
    for (u32 axis = 0; axis < 3; ++axis) {
        const s32 noise = static_cast<s32>(rand() & 255) - 128;
        position[axis] = m.r32(source + kPosition + 4 * axis) + (scale * noise >> 3);
    }
    const u32 particle = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
    if (particle) {
        m.w16(particle + kLife, 10); m.w16(particle + kParticleKind, 0);
        for (u32 axis = 0; axis < 3; ++axis) m.w32(particle + kParticlePosition + 4 * axis, position[axis]);
        m.w16(particle + kDrag, 0); m.w16(particle + kStyle, 256);
        m.w16(particle + kSize, scale * static_cast<s32>((rand() & 63) + 78) >> 12);
    }
    return fire_debris(*this, source, position);
}

u32 Game::emit_travelling_debris(u32 source, u32 mode) {
    cover(0x80093324u);
    Frame frame(*this, 32);
    const u32 particle = pool_take(m.r32(kEffectsPointer) + kDebrisPool, 10, 7);
    if (!particle) return 0;
    m.w16(particle + kParticleKind, 0);
    s32 velocity[3];
    for (u32 axis = 0; axis < 3; ++axis) velocity[axis] = m.r16s(source + kVelocity + 2 * axis);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(particle + kParticleVelocity + 2 * axis, velocity[axis]);
    for (u32 axis = 0; axis < 3; ++axis)
        m.w32(particle + kParticlePosition + 4 * axis, m.r32(source + kPosition + 4 * axis) - static_cast<u32>(320 * velocity[axis]));
    const s32 scale = m.r16s(source + kScale);
    m.w16(particle + kSize, static_cast<s32>((rand() & 127) + 160) * scale / 4096);
    const s32 duration = m.r16s(source + kPhase);
    const u32 style = mode == 2 ? 2564 : mode ? 5635 : 5632;
    m.w16(particle + kLife, mode == 2 ? duration / 256 : 80 * duration / 4096);
    m.w16(particle + kDrag, mode == 2 ? 140 : 397);
    m.w16(particle + kStyle, style);
    return style;
}

void Game::emit_seeded_cloud(u32 source, s32 count, u32 mode) {
    cover(0x800935ACu);
    Frame frame(*this, 72);
    if (count <= 0) return;
    const u32 saved_seed = static_cast<u32>(rand());
    m.w32(sp + 28, saved_seed);
    srand(static_cast<u32>(m.r16s(source + kPhase)));
    for (u32 axis = 0; axis < 3; ++axis) m.w32(sp + 16 + 4 * axis, m.r32(source + kPosition + 4 * axis));
    const s32 age = m.r16s(source + 4), spread = m.r16s(source + 6), scale = m.r16s(source + kScale);
    for (s32 index = 0; index < count; ++index) {
        const u32 choice = static_cast<u32>(rand());
        const s32 particle_age = wrap::sub(age, wrap::add(index, static_cast<s32>(choice & (mode ? 3 : 1))));
        const s32 life = mode ? static_cast<s32>((choice >> 2) & 7) + 20 : static_cast<s32>(choice & 7) + 8;
        if (particle_age < 0 || particle_age >= life) {
            rand(); rand();
            continue;
        }
        const u32 particle = transient_particle_alloc();
        if (!particle) continue;
        m.w32(particle, static_cast<u32>(particle_age) | (static_cast<u32>(life) << 16));
        const u32 noise = static_cast<u32>(rand());
        m.w16(particle + kDrag, 0);
        for (u32 axis = 0; axis < 3; ++axis) {
            const s32 displacement = (static_cast<s32>((noise >> (2 * axis)) & 511) - 256) * spread;
            m.w32(particle + kParticlePosition + 4 * axis, m.r32(sp + 16 + 4 * axis) + (static_cast<u32>(displacement) << 4));
        }
        const u32 size_noise = static_cast<u32>(rand());
        m.w16(particle + kSize, scale * static_cast<s32>(((size_noise >> 3) & 2047) + 3072) >> 12);
        m.w16(particle + kStyle, mode ? (mode == 1 ? 272 : 528) : ((size_noise & 3) ? 256 : 0));
    }
    srand(m.r32(sp + 28));
}

void Game::emit_debris_burst(u32 source, s32 count) {
    cover(0x800937D8u);
    Frame frame(*this, 56);
    if (count <= 0) return;
    u32 position[3];
    for (u32 axis = 0; axis < 3; ++axis) position[axis] = m.r32(source + kPosition + 4 * axis);
    const s32 spread = m.r16s(source + 6), scale = m.r16s(source + kScale);
    u32 life = static_cast<u32>(m.r16s(source + 4));
    for (s32 index = 0; index < count; ++index, life += 2) {
        const u32 particle = pool_take(m.r32(kEffectsPointer) + kDebrisPool, 10, 7);
        if (!particle) break;
        m.w32(particle, life << 16);
        const u32 noise = static_cast<u32>(rand());
        const s32 x = (static_cast<s32>((noise >> 2) & 255) - 128) * spread;
        const s32 y = (static_cast<s32>((noise >> 4) & 255) - 128) * spread;
        const s32 z = (static_cast<s32>(noise & 255) - 128) * spread >> 8;
        m.w16(particle + kDrag, 392);
        m.w32(particle + 4, position[0] + (static_cast<u32>(x >> 8) << 9));
        m.w32(particle + 8, position[1] + (static_cast<u32>(y >> 8) << 9));
        m.w16(particle + kParticleVelocity + 2, (x >> 8) - (x >> 10));
        m.w16(particle + kParticleVelocity + 4, y >> 8);
        m.w32(particle + 12, position[2] + (static_cast<u32>(z) << 9));
        m.w16(particle + kParticleVelocity, z);
        m.w16(particle + kSize, scale * static_cast<s32>((rand() & 4095) + 2048) / 4096);
    }
}

u32 Game::emit_rising_smoke(u32 source) {
    cover(0x80093040u);
    Frame frame(*this, 32);
    const u32 phase = (m.r32(m.r32(g::kWorld) + 12) + m.r16s(source + kPhase)) & 3;
    if (phase) return phase;
    const s32 scale = m.r16s(source + kScale);
    const u32 particle = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
    if (!particle) return 0x00500000u;
    m.w32(particle, 0x00500000u);
    for (u32 axis = 0; axis < 3; ++axis) {
        const s32 noise = static_cast<s32>(rand() & 63) - 32;
        m.w32(particle + kParticlePosition + 4 * axis, m.r32(source + kPosition + 4 * axis) + (scale * noise >> 1));
    }
    const s32 x_velocity = scale * (static_cast<s32>(rand() & 63) - 32) >> 12;
    m.w16(particle + kParticleVelocity + 2, -144 * scale >> 12);
    m.w16(particle + kParticleVelocity, x_velocity);
    const s32 z_velocity = scale * (static_cast<s32>(rand() & 63) - 32) >> 12;
    m.w16(particle + kDrag, 128);
    m.w16(particle + kStyle, 528);
    m.w16(particle + kParticleVelocity + 4, z_velocity);
    const s32 size = scale * static_cast<s32>((rand() & 63) + 88) >> 12;
    m.w16(particle + kSize, size);
    return static_cast<u32>(size);
}

u32 Game::emit_trailing_smoke(u32 source) {
    cover(0x8009319Cu);
    Frame frame(*this, 48);
    if (m.r32(m.r32(g::kWorld) + 12) & 1) return 1;
    const u32 particle = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
    if (!particle) return 0;
    const s32 duration = m.r16s(source + kPhase), scale = m.r16s(source + kScale);
    m.w16(particle + kParticleKind, duration / 1024);
    m.w16(particle + kLife, static_cast<s32>((rand() & 15) + 64) * duration / 4096);
    s32 velocity[3];
    for (u32 axis = 0; axis < 3; ++axis) velocity[axis] = m.r16s(source + kVelocity + 2 * axis);
    m.w16(particle + kDrag, 6);
    for (u32 axis = 0; axis < 3; ++axis) m.w16(particle + kParticleVelocity + 2 * axis, velocity[axis]);
    u32 position[3];
    for (u32 axis = 0; axis < 3; ++axis)
        position[axis] = m.r32(source + kPosition + 4 * axis) + (static_cast<u32>(velocity[axis]) << 9);
    for (u32 axis = 0; axis < 3; ++axis) {
        const s32 noise = static_cast<s32>(rand() & 255) - 128;
        if (axis == 2) m.w16(particle + kStyle, 273);
        m.w32(particle + kParticlePosition + 4 * axis, position[axis] + (noise * scale >> 3));
    }
    const s32 size = static_cast<s32>((rand() & 15) + 240) * scale >> 12;
    m.w16(particle + kSize, size);
    return static_cast<u32>(size);
}

u32 Game::emit_billboard(u32 source, u32 oriented) {
    cover(0x80093484u);
    Frame frame(*this, 32);
    const u32 record = pool_take(m.r32(kEffectsPointer) + kBillboardPool, 1, 6);
    if (!record) return 0;
    const u32 x = m.r32(source + kPosition), y = m.r32(source + kPosition + 4), z = m.r32(source + kPosition + 8);
    m.w32(record + 4, x); m.w32(record + 8, y); m.w32(record + 12, z);
    if (oriented) {
        const u32 orientation = m.r32(source + 4);
        m.w16(record + 20, 1);
        m.w32(record + 16, orientation);
    } else {
        m.w16(record + 20, 0);
    }
    const u32 kind = static_cast<u32>(m.r16s(source + kPhase)) | 0x10000000u;
    const u32 size = m.r16(source + kScale);
    m.w32(record, kind);
    m.w16(record + 22, size);
    return kind;
}

u32 Game::emit_transient_particle(u32 source, s32 mode) {
    cover(0x8009351Cu);
    Frame frame(*this, 32);
    const u32 record = transient_particle_alloc();
    if (!record) return 0;
    const u32 x = m.r32(source + kPosition), y = m.r32(source + kPosition + 4);
    m.w32(record + 4, x); m.w32(record + 8, y);
    const u32 z = m.r32(source + kPosition + 8), kind_and_life = m.r32(source + 4);
    m.w32(record + 12, z); m.w32(record, kind_and_life);
    const u32 size = m.r16(source + kScale);
    m.w16(record + kDrag, 0); m.w16(record + kSize, size);
    const u32 style = mode < 4 ? (static_cast<u32>(mode) & 3) << 8 : (((static_cast<u32>(mode) & 1) + 1) << 8) | 0x10;
    m.w16(record + kStyle, style);
    return style;
}

u32 Game::emit_short_segment(u32 source) {
    cover(0x80093944u);
    Frame frame(*this, 24);
    const u32 record = spark_take();
    if (!record) return 3;
    const u32 kind = m.r16(source + 2);
    m.w16(record + 2, 3); m.w16(record, kind);
    const u32 x = m.r32(source + 8), y = m.r32(source + 12), z = m.r32(source + 16);
    const u32 first_width = m.r8(source + 4), second_width = m.r16(source + 6) & 255;
    m.w32(record + 4, (x & 0xFFFFFF00u) | first_width);
    m.w32(record + 12, z);
    const u32 packed_y = (y & 0xFFFFFF00u) | second_width;
    m.w32(record + 8, packed_y);
    const u32 end_x = m.r32(source + 20), end_y = m.r32(source + 24), end_z = m.r32(source + 28);
    m.w32(record + 16, end_x); m.w32(record + 20, end_y); m.w32(record + 24, end_z);
    return packed_y;
}
}
