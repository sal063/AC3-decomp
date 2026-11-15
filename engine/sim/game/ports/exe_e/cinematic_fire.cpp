#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kEffectsPointer = 0x800BCF90u, kSmokePool = 1536, kDebrisPool = 4248;
}

u32 Game::emit_cinematic_fire(u32 x, u32 y, u32 z, u32 phase) {
    cover(0x80093D1Cu);
    Frame frame(*this, 40);
    if (phase & 1) {
        const u32 smoke = pool_take(m.r32(kEffectsPointer) + kSmokePool, 3, 7);
        if (smoke) {
            const bool bright = (rand() & 1) != 0;
            m.w16(smoke, 0xFFFFu);
            m.w16(smoke + 2, (rand() & 7) + (bright ? 12 : 30));
            m.w32(smoke + 4, x + ((static_cast<u32>(rand()) - 16384u) << 9));
            m.w32(smoke + 8, y);
            m.w32(smoke + 12, z + ((static_cast<u32>(rand()) - 16384u) << 9));
            m.w16(smoke + 22, 0); m.w16(smoke + 24, bright ? 256 : 528);
            m.w16(smoke + 26, (rand() & (bright ? 511 : 255)) + (bright ? 1000 : 1600));
        }
    }
    if (rand() & 1) {
        const u32 debris = pool_take(m.r32(kEffectsPointer) + kDebrisPool, 10, 7);
        if (debris) {
            m.w16(debris, 0); m.w16(debris + 2, (rand() & 15) + 60);
            m.w32(debris + 4, x + ((static_cast<u32>(rand()) - 16384u) << 9));
            m.w32(debris + 8, y - 3276800u);
            m.w32(debris + 12, z + ((static_cast<u32>(rand()) - 16384u) << 9));
            m.w16(debris + 22, 152);
            m.w16(debris + 16, static_cast<s32>(rand() & 255) - 128);
            m.w16(debris + 20, static_cast<s32>(rand() & 255) - 128);
            m.w16(debris + 18, 0); m.w16(debris + 24, 0);
            m.w16(debris + 26, (rand() & 255) + 240);
        }
    }
    const u32 spark = pool_take(m.r32(kEffectsPointer) + kDebrisPool, 10, 7);
    if (!spark) return 30;
    m.w16(spark, 30); m.w16(spark + 2, (rand() & 7) + 80);
    const s32 offset_x = static_cast<s32>((static_cast<u32>(rand()) - 16384u) << 9) >> 1;
    m.w32(spark + 4, x + offset_x); m.w32(spark + 8, y - 819200u);
    const s32 offset_z = static_cast<s32>((static_cast<u32>(rand()) - 16384u) << 9) >> 1;
    m.w32(spark + 12, z + offset_z); m.w16(spark + 22, 152);
    m.w16(spark + 16, static_cast<s32>(rand() & 127) - 64);
    m.w16(spark + 20, static_cast<s32>(rand() & 127) - 64);
    m.w16(spark + 18, 0); m.w16(spark + 24, 7684);
    const u32 size = (rand() & 1023) + 2000;
    m.w16(spark + 26, size);
    return size;
}

u32 Game::cinematic_fire_update(s32 age) {
    cover(0x80093FC0u);
    Frame frame(*this, 24);
    if (age < 11) return 1;
    if (age >= 736) return 0;
    s32 pulse = rsin(age * 64);
    if (pulse < 0) pulse = -pulse;
    if (age >= 390) pulse = (pulse >> 1) + 2048;
    u32 world = m.r32(g::kWorld);
    const u32 next = effect_red_flash(m.r32(world + 16) + 8, m.r32(world + 20), 4096 - pulse);
    m.w32(m.r32(g::kWorld) + 20, next);
    m.w32(m.r32(kEffectsPointer) + 1480, 1);
    if (age >= 391) return 0;
    const u32 noise = static_cast<u32>(rand());
    if (noise & 1) {
        world = m.r32(g::kWorld);
        m.w16(world + 656, m.r16(world + 656) + ((noise >> 1) & 1));
        m.w16(world + 658, m.r16(world + 658) + ((noise >> 3) & 7));
    }
    if (!(age & 1)) return age < 111 ? 1 : 0;
    s32 x, y, z;
    if (age < 111) {
        x = -9500; y = -2400; z = -5000;
    } else if (age < 201) {
        x = -7000; y = -2500; z = -3500;
    } else if (age < 281) {
        const s32 progress = (age - 220) * 4096 / 50;
        x = -1000 * progress / 4096 - 16000;
        y = 500 * progress / 4096 - 2500;
        z = 12000 * progress / 4096 + 55000;
    } else {
        const s32 progress = (age - 320) * 4096 / 40;
        x = 1000 * progress / 4096 - 18000;
        y = 800 * progress / 4096 - 2500;
        z = 7000 * progress / 4096 + 63000;
    }
    return emit_cinematic_fire(static_cast<u32>(x) << 12, static_cast<u32>(y) << 12,
                               static_cast<u32>(z) << 12, static_cast<u32>(age >> 1));
}
}
