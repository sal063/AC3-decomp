#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kScratch = 0x1F800000u, kTrig = 0x800B7104u;
constexpr u32 kOrientation = 0x40, kSize = 0x70, kDepth = 0x68, kMagnification = 0x6C;
s32 absolute(s32 x) { return x < 0 ? wrap::neg(x) : x; }
bool relative_position(Memory& m, Gte& gte, u32 particle, u32 view, u32 rotation) {
    const s32 x = wrap::sub(m.r32(particle + 4), m.r32(view + 8)) >> 12;
    if (static_cast<u32>(x) + 32768u > 65535u) return false;
    const s32 y = wrap::sub(m.r32(particle + 8), m.r32(view + 12)) >> 12;
    if (static_cast<u32>(y) + 32768u > 65535u) return false;
    const s32 z = wrap::sub(m.r32(particle + 12), m.r32(view + 16)) >> 12;
    if (static_cast<u32>(z) + 32768u > 65535u) return false;
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(rotation + 4 * i));
    for (u32 i = 5; i < 8; ++i) gte.ctc2(i, 0);
    gte.mtc2(0, (static_cast<u32>(x) & 65535u) | (static_cast<u32>(y) << 16));
    gte.mtc2(1, z);
    return true;
}
u32 layer(s32 depth) { return static_cast<u32>(std::clamp(depth >> 3, 0, 4094)); }
void link(Memory& m, u32 view, u32 packet, u32 bucket, u32 words) {
    const u32 head = m.r32(view) + 4 * bucket;
    m.w32(packet, m.r32(head) | (words << 24));
    m.w32(head, packet & 0xFFFFFFu);
}
s32 low_component(u32 v) { return static_cast<s32>(v << 21) >> 21; }
s32 middle_component(u32 v) { return static_cast<s32>(v << 11) >> 21; }
s32 high_component(u32 v) { return static_cast<s32>(v) >> 21; }
void mesh_half(Memory& m, u32 mesh, u32 context, bool second) {
    const u32 a = m.r32(mesh), b = m.r32(mesh + 4), c = m.r32(mesh + 8);
    m.w16(context + 0x78, second ? high_component(a) : middle_component(a));
    m.w16(context + 0x7A, second ? middle_component(a) : low_component(a));
    m.w16(context + 0x7C, second ? low_component(a) : high_component(a));
    m.w16(context + 0x80, second ? middle_component(b) : low_component(b));
    m.w16(context + 0x82, second ? low_component(b) : high_component(b));
    m.w16(context + 0x84, second ? high_component(b) : middle_component(b));
    m.w16(context + 0x88, second ? low_component(c) : high_component(c));
    m.w16(context + 0x8A, second ? high_component(c) : middle_component(c));
    m.w16(context + 0x8C, second ? middle_component(c) : low_component(c));
}
void mesh_vectors(Memory& m, Gte& gte, u32 context) {
    gte.mtc2(0, m.r32(context + 0x78)); gte.mtc2(1, m.r32(context + 0x7C));
    gte.mtc2(2, m.r32(context + 0x80)); gte.mtc2(3, m.r32(context + 0x84));
    gte.mtc2(4, m.r32(context + 0x88)); gte.mtc2(5, m.r32(context + 0x8C));
}
}

u32 Game::particle_debris(u32 particle, u32 view, u32 context) {
    cover(0x80086320u);
    Frame frame(*this, 0x30);
    if (!relative_position(m, gte, particle, view, context)) return 1;
    const u32 seed = m.r32(m.r32(kGp + 0x41C) + (particle & 0x1FCu));
    gte.op(0x180001u);
    m.w32(context + kOrientation, seed);
    m.w16(context + kOrientation + 4, (m.r16(particle + 22) & 256u) ? m.r16(particle) << 8 : 0);
    const s32 size = m.r16s(particle + 26), nearPlane = m.r16s(view + 26);
    m.w32(context + kSize, size);
    const s32 depth = static_cast<s32>(gte.mfc2(19));
    if (depth <= 0) return static_cast<u32>(nearPlane);
    const s32 radius = static_cast<s32>(wrap::div(wrap::mul(size, nearPlane), depth).lo);
    const s32 front = wrap::sub(depth, radius);
    if (front < (nearPlane >> 4)) return 1;
    const u32 screen = gte.mfc2(14);
    if (absolute(static_cast<s32>(screen) >> 16) > wrap::add(radius, 120)) return 1;
    if (absolute(static_cast<s16>(screen)) > wrap::add(radius, 160)) return 1;
    const u32 packet = m.r32(view + 4);
    if (radius < 3) {
        link(m, view, packet, layer(depth), 2);
        const u32 phase = seed + static_cast<u32>(static_cast<s32>(seed) >> 16)
            + static_cast<u32>(m.r16s(context + kOrientation + 4));
        m.w32(packet + 4, (phase & 2048u) ? 0x68636363u : 0x68181818u);
        m.w32(packet + 8, screen);
        m.w32(view + 4, packet + 12);
        return packet + 12;
    }
    m.w32(context + 0x74, radius); m.w32(context + kDepth, depth);
    const u32 enlargement = front < nearPlane ? 4u : 0u;
    m.w32(context + kMagnification, enlargement);
    if (enlargement) m.w32(context + kSize, static_cast<u32>(size) << 4);
    const u32 tx = gte.mfc2(25) << enlargement, ty = gte.mfc2(26) << enlargement, tz = gte.mfc2(27) << enlargement;
    particle_rotation(context + kOrientation, m.r32(context + kSize), context + 32);
    particle_compose_rotation(context + 32);
    gte.ctc2(5, tx); gte.ctc2(6, ty); gte.ctc2(7, tz);
    gte.mtc2(0, 0); gte.mtc2(1, -2048);
    gte.mtc2(2, 0x04000000); gte.mtc2(3, 512);
    gte.mtc2(4, 0xFD000000); gte.mtc2(5, (m.r16(context + kOrientation) & 7u) << 9);
    gte.op(0x280030u);
    const bool textured = static_cast<s32>(m.r32(context + 0x74)) >= 7;
    link(m, view, packet, layer(static_cast<s32>(m.r32(context + kDepth))), textured ? 7 : 4);
    if (textured) {
        m.w32(packet + 12, (m.r16(context + kOrientation) & 1u) ? 0x45D610E0u : 0x45D61FEFu);
        m.w32(packet + 20, 0x003810EFu); m.w16(packet + 28, 0x1FE0);
        m.w32(packet + 8, gte.mfc2(12)); m.w32(packet + 16, gte.mfc2(13)); m.w32(packet + 24, gte.mfc2(14));
    } else {
        m.w32(packet + 8, gte.mfc2(12)); m.w32(packet + 12, gte.mfc2(13)); m.w32(packet + 16, gte.mfc2(14));
    }
    gte.op(0x1400006u);
    m.w32(packet + 4, (textured ? 0x24000000u : 0x20000000u)
        | (static_cast<s32>(gte.mfc2(24)) < 0 ? 0x636363u : 0x181818u));
    const u32 end = packet + (textured ? 32 : 20);
    m.w32(view + 4, end);
    return end;
}

u32 Game::particle_smoke(u32 particle, u32 view, u32 context) {
    cover(0x80086A0Cu);
    Frame frame(*this, 0x30);
    if (!relative_position(m, gte, particle, view, context)) return 1;
    const u32 randomTable = m.r32(kGp + 0x41C);
    gte.op(0x180001u);
    const u32 seed = m.r32(randomTable + (particle & 0x1FCu));
    m.w32(context + kOrientation, seed);
    const s32 age = m.r16s(particle), lifetime = m.r16s(particle + 2);
    m.w16(context + kOrientation + 4, (m.r16(particle + 22) & 256u) ? static_cast<u32>(age) << 6 : 0);
    u32 mesh = randomTable + 0x420 + ((static_cast<s32>(seed) >> 4) & 28u);
    u32 noise = randomTable + 4 * (seed & 63u);
    const s32 progress = static_cast<s32>(wrap::div(static_cast<s32>(static_cast<u32>(age) << 12), lifetime ? lifetime : 1).lo);
    m.w32(context + 0x90, progress);
    if (progress >= 2731) noise += (static_cast<u32>(age) << 1) & 12u;
    s32 product = wrap::mul(m.r16s(particle + 26), m.r16s(kTrig + static_cast<u32>(progress >> 2) * 4));
    if (product < 0) product = wrap::add(product, 4095);
    const s32 size = product >> 12;
    const s32 depth = static_cast<s32>(gte.mfc2(19));
    m.w32(context + kSize, size);
    if (depth <= 0) return static_cast<u32>(product);
    const s32 nearPlane = m.r16s(view + 26);
    const s32 radius = static_cast<s32>(wrap::div(wrap::mul(size, nearPlane), depth).lo);
    const s32 front = wrap::sub(depth, radius);
    if (front < (nearPlane >> 4)) return 1;
    const u32 screen = gte.mfc2(14);
    if (absolute(static_cast<s32>(screen) >> 16) > wrap::add(radius, 120)) return 1;
    if (absolute(static_cast<s16>(screen)) > wrap::add(radius, 160)) return static_cast<u32>(depth - 1024);
    s32 fade = (depth - 1024) >> 2;
    if (fade >= 4096) return 0;
    fade = std::max(fade, 0);
    gte.mtc2(8, fade);
    m.w32(context + 0x58, gte.mfc2(25)); m.w32(context + 0x5C, gte.mfc2(26)); m.w32(context + 0x60, gte.mfc2(27));
    gte.ctc2(21, 48); gte.ctc2(22, 48); gte.ctc2(23, 48); gte.mtc2(6, 0x6A000000);
    const u32 kind = m.r16(particle + 24);
    const u32 palette = 0x800B43ACu + 12 * std::min(kind & 255u, 6u);
    for (u32 i = 0; i < 3; ++i) gte.mtc2(20 + i, m.r32(palette + 4 * i));
    u32 triangles = (kind >> 8) & 31u;
    gte.op(0xF8002Au);
    const u32 enlargement = front < nearPlane ? 4u : 0u;
    m.w32(context + kMagnification, enlargement);
    if (enlargement) m.w32(context + kSize, static_cast<u32>(size) << 4);
    if (triangles & 16u) triangles = std::min(triangles & 15u, static_cast<u32>(std::max((radius - 4) >> 1, 1)));
    m.w32(context + kDepth, layer(depth));
    for (u32 i = 0; i < 3; ++i) m.w32(context + 0x94 + 4 * i, gte.mfc2(20 + i));
    particle_rotation(context + kOrientation, m.r32(context + kSize) << 2, context + 32);
    particle_compose_rotation(context + 32);
    const u32 tx = m.r32(context + 0x58) << enlargement;
    gte.ctc2(5, tx); gte.ctc2(6, m.r32(context + 0x5C) << enlargement); gte.ctc2(7, m.r32(context + 0x60) << enlargement);
    noise += 4;
    const u32 baseFade = m.r32(context + 0x90) - 512;
    u32 currentFade = baseFade + (tx & 1023u), packet = m.r32(view + 4);
    for (u32 n = 0; n < triangles; ++n) {
        gte.mtc2(8, currentFade);
        for (u32 i = 0; i < 3; ++i) gte.mtc2(20 + i, m.r32(context + 0x94 + 4 * i));
        gte.op(0xF8002Au);
        mesh_half(m, mesh, context, false); mesh += 12;
        for (u32 i = 0; i < 3; ++i) {
            const u32 color = gte.mfc2(20 + i);
            m.w32(packet + 4 + 16 * i, color); m.w32(packet + 12 + 16 * i, color);
        }
        mesh_vectors(m, gte, context); gte.op(0x280030u);
        mesh_half(m, mesh, context, true); mesh += 12;
        m.w32(packet + 8, gte.mfc2(12)); m.w32(packet + 16, gte.mfc2(13)); m.w32(packet + 24, gte.mfc2(14));
        mesh_vectors(m, gte, context);
        m.w32(packet, ((packet + 52) & 0xFFFFFFu) | 0x0C000000u);
        gte.op(0x280030u);
        currentFade = baseFade + (m.r32(noise) & 1023u); noise += 4;
        m.w32(packet + 32, gte.mfc2(12)); m.w32(packet + 40, gte.mfc2(13)); m.w32(packet + 48, gte.mfc2(14));
        packet += 52;
    }
    const u32 head = m.r32(view) + 4 * m.r32(context + kDepth);
    const u32 tail = packet - 52;
    m.w32(tail, (m.r32(tail) & 0xFF000000u) | (m.r32(head) & 0xFFFFFFu));
    const u32 start = m.r32(view + 4);
    m.w32(head, (m.r32(head) & 0xFF000000u) | (start & 0xFFFFFFu));
    m.w32(view + 4, packet);
    return start << 8;
}

u32 Game::particle_meshes_draw(u32 bank) {
    cover(0x8008700Cu);
    Frame frame(*this, 0x38);
    constexpr u32 context = kScratch + 28;
    for (u32 i = 0; i < 5; ++i) m.w32(context + 4 * i, gte.cfc2(i));
    m.w32(kScratch + 0x3FC, sp); sp = kScratch + 0x3F8;
    const u32 saved = sp - 0x30;
    auto prologue = [&](u32 particle, u32 active, u32 group, u32 ret) {
        m.w32(saved + 0x10, particle);
        m.w32(saved + 0x14, active);
        m.w32(saved + 0x18, context);
        m.w32(saved + 0x1C, bank + 4 * group);
        m.w32(saved + 0x20, 40 + 896 * group);
        m.w32(saved + 0x24, group);
        m.w32(saved + 0x28, ret);
    };
    for (u32 group = 0; group < 10; ++group) {
        u32 active = m.r32(bank + 4 * group), particle = bank + 40 + 896 * group;
        while (active) {
            if (active & 1u) {
                if (m.r16(particle + 24) == 0) {
                    prologue(particle, active, group, 0x800870C8u);
                    particle_debris(particle, kScratch, context);
                } else {
                    prologue(particle, active, group, 0x800870DCu);
                    particle_smoke(particle, kScratch, context);
                }
            }
            active >>= 1; particle += 28;
        }
    }
    sp = m.r32(kScratch + 0x3FC);
    for (u32 i = 0; i < 5; ++i) gte.ctc2(i, m.r32(context + 4 * i));
    for (u32 i = 5; i < 8; ++i) gte.ctc2(i, 0);
    return 0;
}
}
