#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kScratch = 0x1F800000u;
constexpr u32 kOt = kScratch + 0x00;
constexpr u32 kPackets = kScratch + 0x04;
constexpr u32 kEye = kScratch + 0x08;
constexpr u32 kEyeAngles = kScratch + 0x14;
constexpr u32 kEyeH = kScratch + 0x1A;
constexpr u32 kWork = kScratch + 0x1C;

constexpr u32 wColour = 0x00;
constexpr u32 wRadius = 0x04;
constexpr u32 wInner = 0x06;
constexpr u32 wFlag = 0x08;
constexpr u32 wEntry = 0x0C;
constexpr u32 wRelative = 0x10;
constexpr u32 wScreen = 0x18;
constexpr u32 wInnerXY = 0x1C;
constexpr u32 wOuterXY = 0x20;
constexpr u32 wCircle = 0x24;
constexpr u32 wMaskOff = 0x78;
constexpr u32 wFirstInner = 0x7C;
constexpr u32 wFirstOuter = 0x80;
constexpr u32 wFirstFlag = 0x84;
constexpr u32 wViewRotation = 0x88;
constexpr u32 wMatrix = 0xA8;
constexpr u32 wAngles = 0xC8;
constexpr u32 wScale = 0xD0;

constexpr u32 lAge = 0x00, lLife = 0x02, lPosition = 0x04, lAngles = 0x10, lType = 0x14, lSize = 0x16;

constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kFxFlags = 1480;
constexpr u32 kArea = 0x800BCEB0u;
constexpr u32 kTrig = 0x800B7104u;
constexpr u32 kWeather = 0x800B4388u;
constexpr u32 kLightDrawers = 0x800B4400u;
constexpr u32 kFlareTable = 0x800B440Cu;

constexpr u32 kRtps = 0x180001u, kRtpt = 0x280030u, kDpcs = 0x780010u;
constexpr u32 kRotate = 0x486012u;
constexpr u32 kRotateMove = 0x480012u;

s32 iabs(s32 v) { return v < 0 ? -v : v; }
s32 trunc12(s32 v) { return (v < 0 ? v + 0xFFF : v) >> 12; }
s32 trunc13(s32 v) { return (v < 0 ? v + 0x1FFF : v) >> 13; }
s32 trunc2(s32 v) { return (v < 0 ? v + 3 : v) >> 2; }
u32 grey(u32 level) { return level | (level << 8) | (level << 16); }
u32 tag(u32 next, u32 words) { return (next & 0xFFFFFFu) | (words << 24); }

u32 depth_entry(Memory& m, s32 sz) {
    s32 d = (sz >> 3) - 12;
    if (d >= 4095) d = 4094;
    else if (d < 0) d = 0;
    return m.r32(kOt) + 4 * static_cast<u32>(d);
}

void splice(Memory& m, u32 entry, u32 first, u32 last) {
    m.w32(last, (m.r32(last) & 0xFF000000u) | (m.r32(entry) & 0xFFFFFFu));
    m.w32(entry, (m.r32(entry) & 0xFF000000u) | (first & 0xFFFFFFu));
}

void link(Memory& m, u32 entry, u32 packet, u32 words) {
    m.w32(packet, m.r32(entry) | (words << 24));
    m.w32(entry, packet & 0xFFFFFFu);
}

bool relative_position(Memory& m, u32 light, u32 work) {
    for (u32 k = 0; k < 3; ++k) {
        const s32 d = (m.r32s(light + lPosition + 4 * k) - m.r32s(kEye + 4 * k)) >> 12;
        m.w16(work + wRelative + 2 * k, static_cast<u32>(d));
        if (iabs(d) > 0x7FFF) return false;
    }
    return true;
}

s32 age_level(s32 age, s32 life) { return (life >> 1) < age ? 72 * (life - age) / life : 36; }

u32 disc_shift(s32 radius) { return radius < 5 ? 3u : radius < 10 ? 2u : radius < 44 ? 1u : 0u; }

void load_rotation(Gte& gte, Memory& m, u32 src) {
    for (u32 k = 0; k < 5; ++k) gte.ctc2(k, m.r32(src + 4 * k));
}
void load_rotation_translation(Gte& gte, Memory& m, u32 src) {
    for (u32 k = 0; k < 8; ++k) gte.ctc2(k, m.r32(src + 4 * k));
}
}

u32 Game::effects_draw(u32 ot, u32 packets) {
    cover(0x80085204u);
    Frame f(*this, 0x20);
    m.w32(kOt, ot);
    m.w32(kPackets, packets);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    m.w32(kEye + 0, m.r32(view + 4));
    m.w32(kEye + 4, m.r32(view + 8));
    m.w32(kEye + 8, m.r32(view + 12));
    m.w16(kEyeAngles + 0, m.r16(view + 20));
    m.w16(kEyeAngles + 2, m.r16(view + 22));
    m.w16(kEyeAngles + 4, m.r16(view + 24));
    m.w16(kEyeH, m.r16(view + 36));
    if ((m.r32(view + 80) & 0x870u) || (m.r32(m.r32(g::kScene)) & 0x20000000u)) {
        const u32 fx = m.r32(kFx);
        m.w32(fx + kFxFlags, m.r32(fx + kFxFlags) | 1u);
    }
    const s32 sun = m.r16s(m.r32(m.r32(g::kWorld) + 580) + 108);
    if (sun) {
        const u32 rec = m.r32(kFx) + kFxFlags;
        sun_flare(rec, sun, -m.r32s(m.r32(kArea) + 4));
        const u32 world = m.r32(g::kWorld);
        if (const u32 colour = m.r32(m.r32(world + 580) + 112)) {
            if (m.r16s(m.r32(g::kScene) + 910) == 25 && m.r32(m.r32(world + 40) + 80) == 2)
                m.w32(rec, m.r32(rec) | 4u);
            else
                sun_disc(rec, colour);
        }
    }
    if (m.r8(m.r32(m.r32(g::kWorld) + 580) + 110)) draw_cloud_layers();
    const u32 weather = m.r8(m.r32(m.r32(g::kWorld) + 580) + 111);
    if (weather > 0 && weather < 6)
        call_by_address(m.r32(kWeather + 4 * (weather - 1)), "a weather effect", 0, weather);
    particle_spray_draw(m.r32(kFx) + 0x600);
    const u32 left = particle_meshes_draw(m.r32(kFx) + 0x1098);
    lights_draw(m.r32(kFx) + 0x33C0, left);
    draw_muzzle(m.r32(kFx) + 0x3948);
    draw_effect_segments(m.r32(kFx) + 0x3964);
    if (m.r8(m.r32(g::kScene) + 933) || (m.r32(m.r32(m.r32(g::kWorld) + 40) + 80) & 0x200u))
        draw_view_filters(kScratch);
    return m.r32(kPackets);
}

u32 Game::lights_draw(u32 bank, u32 v0) {
    cover(0x8008B048u);
    Frame f(*this, 0x28);
    u32 live = m.r32(bank);
    if (!live) return v0;
    for (u32 k = 0; k < 32; ++k) {
        const u32 s = m.r16(kTrig + 0x200 * k);
        m.w16(kWork + wCircle + 2 * k, s);
        if (k < 9) m.w16(kWork + wCircle + 2 * (32 + k), s);
    }
    m.w32(kWork + wMaskOff, (m.r32(m.r32(kFx) + kFxFlags) & 4u) ? 0xE6000001u : 0xE6000000u);
    for (u32 k = 0; k < 5; ++k) m.w32(kWork + wViewRotation + 4 * k, gte.cfc2(k));
    u32 result = 0;
    for (u32 light = bank + 4; live; live >>= 1, light += 24) {
        if (!(live & 1u)) continue;
        const u32 type = static_cast<u32>(static_cast<s32>(m.r16s(light + lType)));
        if (type >= 3) {
            result = type << 2;
            continue;
        }
        const u32 fn = m.r32(kLightDrawers + 4 * type);
        switch (fn) {
        case 0x80089F44u: result = light_glow(light, kWork); break;
        case 0x8008A61Cu: result = light_ring(light, kWork); break;
        case 0x8008ABF0u: result = light_flash(light, kWork); break;
        default: result = call_by_address(fn, "a light's drawer", light, kWork); break;
        }
    }
    return result;
}

u32 Game::light_glow(u32 light, u32 work) {
    cover(0x80089F44u);
    Frame f(*this, 0x28);
    if (!relative_position(m, light, work)) return 1;
    gte.mtc2(0, m.r32(work + wRelative));
    gte.mtc2(1, m.r32(work + wRelative + 4));
    gte.op(kRtps);
    s32 life = m.r16s(light + lLife);
    const s32 age = m.r16s(light + lAge);
    if (life <= 0) life = 1;
    m.w32(work + wColour, static_cast<u32>(age_level(age, life)));
    const u32 flag = gte.cfc2(31);
    if (static_cast<s32>(flag) < 0) return flag;
    s32 sz = static_cast<s32>(gte.mfc2(19));
    m.w32(work + wScreen, gte.mfc2(14));
    const Gte::Exact centre = gte.exact_fifo[2];
    if (sz == 0) sz = 1;
    const s32 phase = static_cast<s32>(static_cast<u32>(age) << 10) / life;
    const s32 scale = m.r16s(light + lSize) * m.r16s(kEyeH) / sz;
    const s32 radius = trunc12(rsin(phase) * scale);
    if (radius + 160 < iabs(m.r16s(work + wScreen))) return static_cast<u32>(radius + 120);
    if (radius + 120 < iabs(m.r16s(work + wScreen + 2))) return static_cast<u32>(sz >> 3);
    m.w16(work + wRadius, static_cast<u32>(radius));
    const u32 entry = depth_entry(m, sz);
    m.w32(work + wEntry, entry);
    u32 level = m.r32(work + wColour);
    if (sz >= 10001) level = static_cast<u8>(static_cast<s32>(level) * (46000 - sz) / 46000);
    const u32 colour = grey(level);
    m.w32(work + wColour, colour);
    const s32 size = m.r16s(work + wRadius);
    const u32 p = m.r32(kPackets);
    if (size < 2) {
        link(m, m.r32(work + wEntry), p, 2);
        m.w32(p + 4, m.r32(work + wColour) | 0x6A000000u);
        const u32 xy = m.r32(work + wScreen);
        m.w32(p + 8, xy);
        m.w32(kPackets, p + 12);
        return xy;
    }
    m.w32(p, tag(p + 8, 1));
    m.w32(p + 4, 0xE6000001u);
    const u32 shift = disc_shift(m.r16s(work + wRadius));
    const s32 n = 32 >> shift;
    const u32 cx = m.r16(work + wScreen), cy = m.r16(work + wScreen + 2);
    auto sine = [&](u32 k) { return static_cast<s32>(m.r16s(work + wCircle + 2 * k)); };
    auto at = [&](u32 base, u32 k, u32 radius_at) {
        return base + static_cast<u32>(trunc12(sine(k) * m.r16s(work + radius_at)));
    };
    const u32 first = p + 8;
    u32 q = first;
    if (m.r16s(work + wRadius) < 25) {
        const u32 last = first + 28 * static_cast<u32>(n - 1);
        m.w16(last + 0x18, cx);
        m.w16(first + 0x10, cx);
        const u32 bottom = cy + m.r16(work + wRadius);
        m.w16(last + 0x1A, bottom);
        m.w16(first + 0x12, bottom);
        for (s32 k = 1;; ++k) {
            m.w32(q, tag(q + 28, 6));
            m.w32(q + 4, 0x32000000u);
            m.w32(q + 0x14, m.r32(work + wColour));
            m.w32(q + 0xC, m.r32(work + wColour));
            m.w32(q + 8, m.r32(work + wScreen));
            if (k >= n) break;
            const u32 i = static_cast<u32>(k) << shift;
            const u32 x = at(cx, i, wRadius), y = at(cy, i + 8, wRadius);
            m.w16(q + 28 + 0x10, x);
            m.w16(q + 0x18, x);
            m.w16(q + 28 + 0x12, y);
            m.w16(q + 0x1A, y);
            q += 28;
        }
        q += 28;
    } else {
        m.w16(work + wInner, m.r16(work + wRadius) - 24u);
        const u32 last = first + 36 * static_cast<u32>(n - 1);
        m.w16(last + 0x20, cx);
        m.w16(last + 0x18, cx);
        m.w16(first + 0x10, cx);
        m.w16(first + 0x08, cx);
        const u32 inner = cy + m.r16(work + wInner);
        m.w16(last + 0x1A, inner);
        m.w16(first + 0x0A, inner);
        const u32 outer = cy + m.r16(work + wRadius);
        m.w16(last + 0x22, outer);
        m.w16(first + 0x12, outer);
        for (s32 k = 1;; ++k) {
            m.w32(q, tag(q + 36, 8));
            m.w32(q + 4, 0x3A000000u);
            m.w32(q + 0x14, 0);
            m.w32(q + 0x1C, m.r32(work + wColour));
            m.w32(q + 0xC, m.r32(work + wColour));
            if (k >= n) break;
            const u32 i = static_cast<u32>(k) << shift, next = q + 36;
            const u32 xi = at(cx, i, wInner);
            m.w16(next + 0x08, xi);
            m.w16(q + 0x18, xi);
            const u32 yi = at(cy, i + 8, wInner);
            m.w16(next + 0x0A, yi);
            m.w16(q + 0x1A, yi);
            const u32 xo = at(cx, i, wRadius);
            m.w16(next + 0x10, xo);
            m.w16(q + 0x20, xo);
            const u32 yo = at(cy, i + 8, wRadius);
            m.w16(next + 0x12, yo);
            m.w16(q + 0x22, yo);
            q = next;
        }
        q += 36;
    }
    m.w32(q, 0x01000000u);
    m.w32(q + 4, m.r32(work + wMaskOff));
    if (centre.ok && centre.vz > 0 && size > 0) {
        const double exact = rsin(phase) * (m.r16s(light + lSize) * m.r16s(kEyeH) / static_cast<double>(centre.vz)) / 4096.0;
        fx_begin(m.r32(work + wScreen), centre, exact / size);
        fx_record(first, q);
        fx_end();
    }
    splice(m, m.r32(work + wEntry), m.r32(kPackets), q);
    m.w32(kPackets, q + 8);
    return q + 8;
}

u32 Game::light_ring(u32 light, u32 work) {
    cover(0x8008A61Cu);
    Frame f(*this, 0x30);
    if (!relative_position(m, light, work)) return 1;
    gte.mtc2(0, m.r32(work + wRelative));
    gte.mtc2(1, m.r32(work + wRelative + 4));
    gte.op(kRotate);
    s32 life = m.r16s(light + lLife);
    const s32 age = m.r16s(light + lAge);
    if (life <= 0) life = 1;
    gte.ctc2(5, gte.mfc2(25));
    gte.ctc2(6, gte.mfc2(26));
    gte.ctc2(7, gte.mfc2(27));
    m.w16(work + wAngles + 0, m.r16(light + lAngles));
    m.w16(work + wAngles + 4, static_cast<u32>(age) << 7);
    m.w16(work + wAngles + 2, m.r16(light + lAngles + 2));
    rot_matrix_yxz_gte(work + wAngles, work + wMatrix);
    const s32 scale = trunc12(rsin(static_cast<s32>(static_cast<u32>(age) << 10) / life) * m.r16s(light + lSize));
    m.w32(work + wScale + 8, static_cast<u32>(scale));
    m.w32(work + wScale + 4, static_cast<u32>(scale));
    m.w32(work + wScale + 0, static_cast<u32>(scale));
    scale_matrix(work + wMatrix, work + wScale);
    set_mul_matrix(work + wViewRotation, work + wMatrix);
    gte.mtc2(0, 0);
    gte.mtc2(1, 0);
    gte.mtc2(2, 0x0C000000u);
    gte.mtc2(3, 0);
    gte.mtc2(4, 0x10000000u);
    gte.mtc2(5, 0);
    u32 q = m.r32(kPackets);
    gte.op(kRtpt);
    m.w32(work + wColour, static_cast<u32>(age_level(age, life)));
    m.w32(work + wFlag, gte.cfc2(31));
    auto edge = [&] {
        m.w32(work + wInnerXY, gte.mfc2(13));
        m.w32(work + wOuterXY, gte.mfc2(14));
    };
    if (m.r32s(work + wFlag) >= 0) {
        edge();
        m.w32(work + wFirstInner, m.r32(work + wInnerXY));
        m.w32(work + wFirstOuter, m.r32(work + wOuterXY));
        const s32 sz = static_cast<s32>(gte.mfc2(19));
        s32 d = (sz >> 3) - 12;
        if (d >= 4095) d = 4094;
        else if (d < 0) d = 0;
        m.w32(work + wEntry, m.r32(kOt) + 4 * static_cast<u32>(d));
        if (d >= 10001) m.w32(work + wColour, static_cast<u32>(m.r32s(work + wColour) * (46000 - sz)) / 46000u & 0xFFu);
    }
    m.w32(work + wFirstFlag, m.r32(work + wFlag));
    m.w32(work + wColour, grey(m.r32(work + wColour)));
    for (u32 k = 1; k < 16; ++k) {
        const s32 s = m.r16s(work + wCircle + 4 * k), c = m.r16s(work + wCircle + 4 * k + 16);
        gte.mtc2(2, (static_cast<u32>(trunc2(3 * s)) & 0xFFFFu) | (static_cast<u32>(trunc2(3 * c)) << 16));
        gte.mtc2(4, (static_cast<u32>(s) & 0xFFFFu) | (static_cast<u32>(c) << 16));
        gte.op(kRtpt);
        const bool last_on_screen = m.r32s(work + wFlag) >= 0;
        if (last_on_screen) {
            m.w32(q + 0x08, 0x3A000000u);
            m.w32(q + 0x18, 0);
            m.w32(q + 0x0C, m.r32(work + wInnerXY));
            m.w32(q + 0x04, 0xE6000001u);
            m.w32(q + 0x28, 0xE6000000u);
            m.w32(q + 0x14, m.r32(work + wOuterXY));
        }
        m.w32(work + wFlag, gte.cfc2(31));
        if (m.r32s(work + wFlag) < 0) continue;
        if (last_on_screen) {
            m.w32(q + 0x10, m.r32(work + wColour));
            m.w32(q + 0x20, m.r32(work + wColour));
            edge();
            m.w32(q + 0x1C, m.r32(work + wInnerXY));
            m.w32(q + 0x24, m.r32(work + wOuterXY));
            link(m, m.r32(work + wEntry), q, 10);
            q += 0x2C;
        } else {
            edge();
        }
        m.w32(work + wEntry, depth_entry(m, static_cast<s32>(gte.mfc2(19))));
    }
    if (m.r32s(work + wFlag) >= 0 && m.r32s(work + wFirstFlag) >= 0) {
        m.w32(q + 0x08, 0x3A000000u);
        m.w32(q + 0x18, 0);
        m.w32(q + 0x04, 0xE6000001u);
        m.w32(q + 0x20, m.r32(work + wColour));
        m.w32(q + 0x10, m.r32(work + wColour));
        m.w32(q + 0x28, m.r32(work + wMaskOff));
        m.w32(q + 0x0C, m.r32(work + wInnerXY));
        m.w32(q + 0x14, m.r32(work + wOuterXY));
        m.w32(q + 0x1C, m.r32(work + wFirstInner));
        m.w32(q + 0x24, m.r32(work + wFirstOuter));
        link(m, m.r32(work + wEntry), q, 10);
        q += 0x2C;
    }
    load_rotation(gte, m, work + wViewRotation);
    gte.ctc2(5, 0);
    gte.ctc2(6, 0);
    gte.ctc2(7, 0);
    m.w32(kPackets, q);
    return work + wViewRotation;
}

u32 Game::light_flash(u32 light, u32 work) {
    cover(0x8008ABF0u);
    Frame f(*this, 0x28);
    const s32 dx = (m.r32s(light + lPosition + 0) - m.r32s(kEye + 0)) >> 12;
    if (static_cast<u32>(dx) + 0x8000u > 0xFFFFu) return 1;
    const s32 dy = (m.r32s(light + lPosition + 4) - m.r32s(kEye + 4)) >> 12;
    if (static_cast<u32>(dy) + 0x8000u > 0xFFFFu) return 1;
    const s32 dz = (m.r32s(light + lPosition + 8) - m.r32s(kEye + 8)) >> 12;
    if (static_cast<u32>(dz) + 0x8000u > 0xFFFFu) return static_cast<u32>(dx) & 0xFFFFu;
    gte.mtc2(0, (static_cast<u32>(dx) & 0xFFFFu) | (static_cast<u32>(dy) << 16));
    gte.mtc2(1, static_cast<u32>(dz));
    const s32 age = m.r16s(light + lAge);
    s32 life = m.r16s(light + lLife);
    gte.op(kRtps);
    if (life <= 0) life = 1;
    const s32 phase = static_cast<s32>(static_cast<u32>(age) << 12) / life;
    const s32 span = m.r16s(light + lSize) * m.r16s(kEyeH);
    s32 radius = trunc12(span * rsin(trunc2(phase)));
    const u32 flag = gte.cfc2(31);
    if (static_cast<s32>(flag) < 0) return flag;
    s32 sz = static_cast<s32>(gte.mfc2(19));
    const u32 xy = gte.mfc2(14);
    const Gte::Exact centre = gte.exact_fifo[2];
    const double exact = centre.ok && centre.vz > 0 ? std::min(180.0, radius / static_cast<double>(centre.vz)) : 0.0;
    if (sz == 0) sz = 1;
    radius /= sz;
    if (radius <= 0) return 1;
    if (radius >= 181) radius = 180;
    if (iabs(static_cast<s32>(xy) >> 16) - radius >= 121) return xy << 16;
    if (iabs(static_cast<s16>(xy)) - radius >= 161) return 0;
    gte.ctc2(21, 0x30);
    gte.ctc2(22, 0x30);
    gte.ctc2(23, 0x30);
    gte.mtc2(6, 0x326F2FAFu);
    gte.op(kDpcs);
    s32 d = (((sz >> 3) - 12) >> 3) - 12;
    if (d >= 4095) d = 4094;
    else if (d < 0) d = 0;
    m.w32(work + wEntry, m.r32(kOt) + 4 * static_cast<u32>(d));
    const u32 tinted = gte.mfc2(22);
    gte.mtc2(8, static_cast<u32>(phase));
    gte.mtc2(6, tinted);
    m.w32(work + wScreen, xy);
    m.w16(work + wRadius, static_cast<u32>(radius));
    gte.op(kDpcs);
    const u32 shift = disc_shift(radius);
    const s32 n = 32 >> shift;
    const u32 colour = gte.mfc2(22);
    const u32 p = m.r32(kPackets);
    m.w32(p, tag(p + 8, 1));
    m.w32(p + 8, tag(p + 16, 1));
    m.w32(p + 4, 0xE6000001u);
    m.w32(p + 8 + 4, 0xE1000240u);
    const u32 first = p + 16, last = first + 28 * static_cast<u32>(n - 1);
    const u32 cx = m.r16(work + wScreen), cy = m.r16(work + wScreen + 2);
    m.w16(last + 0x18, cx);
    m.w16(first + 0x10, cx);
    const u32 bottom = cy + m.r16(work + wRadius);
    m.w16(last + 0x1A, bottom);
    m.w16(first + 0x12, bottom);
    u32 q = first;
    for (s32 k = 1;; ++k) {
        m.w32(q, tag(q + 28, 6));
        m.w32(q + 4, colour);
        m.w32(q + 0x14, 0x030303u);
        m.w32(q + 0xC, 0x030303u);
        m.w32(q + 8, xy);
        if (k >= n) break;
        const u32 i = static_cast<u32>(k) << shift;
        const u32 x = cx + static_cast<u32>(trunc12(m.r16s(work + wCircle + 2 * i) * m.r16s(work + wRadius)));
        m.w16(q + 28 + 0x10, x);
        m.w16(q + 0x18, x);
        const u32 y = cy + static_cast<u32>(trunc12(m.r16s(work + wCircle + 2 * (i + 8)) * m.r16s(work + wRadius)));
        m.w16(q + 28 + 0x12, y);
        m.w16(q + 0x1A, y);
        q += 28;
    }
    if (exact > 0) {
        fx_begin(xy, centre, exact / m.r16s(work + wRadius));
        fx_record(first, q + 28);
        fx_end();
    }
    const u32 page = q + 0x1C, mask = q + 0x24;
    m.w32(page, tag(mask, 1));
    m.w32(page + 4, 0xE1000220u);
    m.w32(mask, 0x01000000u);
    m.w32(mask + 4, m.r32(work + wMaskOff));
    splice(m, m.r32(work + wEntry), m.r32(kPackets), mask);
    m.w32(kPackets, q + 0x2C);
    return q + 0x2C;
}

u32 Game::sun_disc(u32 rec, u32 colour) {
    cover(0x8008B310u);
    Frame f(*this, 0x28);
    const s32 eye_y = m.r32s(kEye + 4);
    if (eye_y < static_cast<s32>(0xF8800000u)) return 1;
    const u32 w = kWork;
    m.w32(w + 0, m.r32(rec + 8));
    m.w32(w + 4, m.r32(rec + 12));
    s32 height = -(eye_y >> 12) - m.r16s(w + 2);
    if (height > 0x7FFF) {
        m.w16(w + 2, static_cast<u32>(height >> 1));
        m.w16(w + 0, static_cast<u32>(m.r16s(w + 0) >> 1));
        m.w16(w + 4, static_cast<u32>(m.r16s(w + 4) >> 1));
    } else {
        m.w16(w + 2, static_cast<u32>(height));
    }
    gte.mtc2(0, m.r32(w + 0));
    gte.mtc2(1, m.r32(w + 4));
    gte.op(kRtps);
    const s32 wide = 1200 * m.r16s(kEyeH);
    const s32 radius = trunc12(wide);
    m.w32(w + 0x10, static_cast<u32>(radius));
    if (height < 8193) {
        const s32 third = static_cast<s32>((static_cast<s64>(radius) * 0x55555556LL) >> 32) - (wide >> 31);
        m.w32(w + 0xC, static_cast<u32>(trunc13((radius - third) * height) + third));
    } else {
        m.w32(w + 0xC, static_cast<u32>(radius));
        if (height >= 16385) height = 0x4000;
    }
    if (static_cast<s32>(gte.cfc2(31)) < 0) return w + 8;
    m.w32(w + 8, gte.mfc2(14));
    const s32 size = m.r32s(w + 0x10);
    if (size + 0xA0 < iabs(m.r16s(w + 8))) return static_cast<u32>(size + 0x78);
    if (size + 0x78 < iabs(m.r16s(w + 0xA))) return 1;
    gte.ctc2(21, 0);
    gte.ctc2(22, 0);
    gte.ctc2(23, 0);
    gte.mtc2(6, colour);
    gte.mtc2(8, static_cast<u32>(height >> 3));
    const u32 p = m.r32(kPackets);
    gte.op(kDpcs);
    m.w32(p, tag(p + 8, 1));
    m.w32(p + 4, 0xE6000002u);
    const u32 tint = gte.mfc2(22);
    m.w32(w + 0, 0);
    m.w16(w + 4, 0u - m.r16(kEyeAngles + 4));
    rot_matrix_yxz(w, w + 0x1C);
    m.w32(w + 0x14, 0);
    scale_matrix(w + 0x1C, w + 0xC);
    load_rotation(gte, m, w + 0x1C);
    m.w32(w + 0x38, 0);
    m.w32(w + 0x30, static_cast<u32>(static_cast<s32>(m.r16s(w + 8))));
    m.w32(w + 0x34, static_cast<u32>(static_cast<s32>(m.r16s(w + 0xA))));
    gte.ctc2(5, m.r32(w + 0x30));
    gte.ctc2(6, m.r32(w + 0x34));
    gte.ctc2(7, m.r32(w + 0x38));
    const u32 a = (m.r32(m.r32(g::kScene) + 0x24) << 6) & 0xFFFu;
    u32 angle = (a << 1) & 0xFFFu;
    m.w16(w + 8, m.r16(w + 8) + static_cast<u32>(m.r16s(kTrig + 2 + 4 * a) >> 9));
    const s32 spin = m.r16s(kTrig + 4 * angle);
    m.w16(w + 2, 0x1000);
    m.w16(w + 4, 0);
    m.w16(w + 0, static_cast<u32>(spin >> 4));
    for (u32 k = 0; k < 16; ++k) {
        gte.mtc2(0, m.r32(w + 0));
        gte.mtc2(1, m.r32(w + 4));
        gte.op(kRotateMove);
        angle = (angle + 0x555u) & 0xFFFu;
        m.w32(w + 0, m.r32(kTrig + ((k + 1) << 10)));
        m.w16(w + 0, m.r16(w + 0) + static_cast<u32>(m.r16s(kTrig + 4 * angle) >> 3));
        const u32 point = w + 0x3C + 4 * k;
        m.w16(point + 0, gte.mfc2(9));
        m.w16(point + 2, gte.mfc2(10));
        m.w16(point + 4, gte.mfc2(11));
    }
    m.w32(w + 0x7C, m.r32(w + 0x3C));
    u32 q = p + 8;
    for (u32 k = 0; k < 16; k += 2) {
        m.w32(q, tag(q + 0x24, 8));
        m.w32(q + 0x04, 0x3A030303u);
        m.w32(q + 0x0C, tint);
        m.w32(q + 0x14, 0x030303u);
        m.w32(q + 0x1C, 0x030303u);
        m.w32(q + 0x08, m.r32(w + 0x3C + 4 * k));
        m.w32(q + 0x10, m.r32(w + 8));
        m.w32(q + 0x18, m.r32(w + 0x3C + 4 * (k + 1)));
        m.w32(q + 0x20, m.r32(w + 0x3C + 4 * (k + 2)));
        q += 0x24;
    }
    m.w32(q, 0x01000000u);
    m.w32(q + 4, 0xE6000000u);
    splice(m, m.r32(kOt) + 4 * 30, m.r32(kPackets), q);
    const u32 world = m.r32(g::kWorld);
    m.w32(kPackets, q + 8);
    load_rotation_translation(gte, m, world + 76);
    return world + 76;
}

u32 Game::sun_flare(u32 rec, s32 kind, s32 floor) {
    sun_on_screen_ = false;
    const u32 result = sun_flare_original(rec, kind, floor);
    if (sun_on_screen_ && (option(&Settings::hdr_sun) || option(&Settings::lens_flare))) sun_enhance(kind);
    return result;
}

u32 Game::sun_flare_original(u32 rec, s32 kind, s32 floor) {
    cover(0x8008BA8Cu);
    Frame f(*this, 0x38);
    const u32 w = kWork;
    m.w32(w + 0, m.r32(rec + 8));
    m.w32(w + 4, m.r32(rec + 12));
    gte.mtc2(0, m.r32(w + 0));
    gte.mtc2(1, m.r32(w + 4));
    gte.op(kRtps);
    const u32 p = m.r32(kPackets);
    m.w32(p + 0x14, 0x0039209Fu);
    m.w16(p + 0x1C, 0x3F80u);
    m.w32(p + 0x04, 0x2E808080u);
    m.w32(p + 0x0C, 0x4F162080u);
    m.w16(p + 0x24, 0x3F9Fu);
    if (static_cast<s32>(gte.cfc2(31)) < 0) return 0x3F9Fu;
    m.w32(w + 8, gte.mfc2(14));
    const s32 ax = iabs(m.r16s(w + 8));
    if (ax >= 177) return 0;
    const s32 ay = iabs(m.r16s(w + 0xA));
    if (ay >= 137) return static_cast<u32>(kind) & 0xFF00u;
    sun_on_screen_ = true;
    auto brightness = [&] { m.w32(w + 0xC, static_cast<u32>(((312 - (ax + ay)) << 12) / 312)); };
    u32 left;
    if (kind & 0xFF00) {
        for (u32 k = 0; k < 6; ++k) gte.mtc2(k, m.r32(rec + 0x10 + 4 * k));
        gte.op(kRtpt);
        brightness();
        m.w32(p + 0x08, gte.mfc2(12));
        m.w32(p + 0x10, gte.mfc2(13));
        m.w32(p + 0x18, gte.mfc2(14));
        gte.mtc2(0, m.r32(rec + 0x28));
        gte.mtc2(1, m.r32(rec + 0x2C));
        gte.op(kRtps);
        u32 q = p + 0x28;
        m.w32(p, tag(q, 9));
        const s32 centred = 0xD0 - (iabs(m.r16s(w + 8)) + iabs(m.r16s(w + 0xA)));
        m.w32(p + 0x20, gte.mfc2(14));
        if (centred > 0) {
            constexpr u32 kAngles = kScratch + 0x4C, kTurn = kScratch + 0x64, kQuarter = kScratch + 0x84;
            m.w32(kAngles, 0);
            m.w16(kAngles + 4, (m.r16(w + 8) << 2) + (m.r16(w + 0xA) << 3));
            rot_matrix(kAngles, kTurn);
            load_rotation(gte, m, kTurn);
            const s32 level = (centred >> 2) + 23, reach = (centred >> 2) + 14;
            m.w32(w + 0x10, grey(static_cast<u32>(level)) | 0x2E000000u);
            m.w16(kAngles + 4, 0x400);
            rot_matrix(kAngles, kQuarter);
            gte.ctc2(5, static_cast<u32>(static_cast<s32>(m.r16s(w + 8))));
            gte.ctc2(6, static_cast<u32>(static_cast<s32>(m.r16s(w + 0xA))));
            gte.ctc2(7, m.r16(kEyeH));
            const u32 back = 0u - static_cast<u32>(reach);
            m.w16(kAngles + 0x0, back);
            m.w16(kAngles + 0x2, back);
            m.w16(kAngles + 0x4, 0);
            m.w16(kAngles + 0x8, 0);
            m.w16(kAngles + 0xA, back);
            m.w16(kAngles + 0xC, 0);
            m.w16(kAngles + 0x10, back);
            m.w16(kAngles + 0x12, 0);
            m.w16(kAngles + 0x14, 0);
            for (u32 k = 0; k < 6; ++k) gte.mtc2(k, m.r32(kAngles + 4 * k));
            for (u32 k = 0; k < 4; ++k, q += 0x28) {
                if (k) particle_compose_rotation(kQuarter);
                m.w32(q + 0x20, m.r32(w + 8));
                gte.op(kRtpt);
                m.w32(q, tag(q + 0x28, 9));
                m.w16(q + 0x1C, 0x3FA0u);
                m.w32(q + 0x0C, 0x4F5620A0u);
                m.w32(q + 0x14, 0x003920BFu);
                m.w16(q + 0x24, 0x3FBFu);
                m.w32(q + 0x04, m.r32(w + 0x10));
                m.w32(q + 0x08, gte.mfc2(12));
                m.w32(q + 0x10, gte.mfc2((k & 1) ? 14 : 13));
                m.w32(q + 0x18, gte.mfc2((k & 1) ? 13 : 14));
            }
            load_rotation_translation(gte, m, m.r32(g::kWorld) + 0x4C);
        }
        splice(m, m.r32(kOt) + 4 * 4094, m.r32(kPackets), q - 0x28);
        left = m.r32(kPackets) << 8;
        m.w32(kPackets, q);
    } else {
        if (!(kind & 0xFF)) return 0;
        brightness();
        left = 0;
    }
    const u32 flare = static_cast<u32>(kind) & 0xFFu;
    if (!flare) return left;
    if (m.r32s(w + 0xC) >= 2049) {
        if (flare == 1) {
            m.w32(rec, m.r32(rec) | 1u);
            const s32 v = (m.r32s(w + 0xC) - 0x800) * 80;
            const u32 level = static_cast<u32>((v < 0 ? v + 0x7FF : v) >> 11);
            const u32 t = m.r32(kPackets);
            link(m, m.r32(kOt), t, 3);
            m.w16(t + 8, 0xFF60u);
            m.w32(t + 4, grey(level) | 0x62000000u);
            m.w16(t + 0xA, 0xFF88u);
            m.w32(t + 0xC, 0x00F00140u);
            m.w32(kPackets, t + 0x10);
        } else {
            m.w32(rec, m.r32(rec) | 2u);
            m.w32(rec + 4, static_cast<u32>(m.r32s(w + 0xC) - 0x800) << 1);
        }
        const s32 b = m.r32s(w + 0xC);
        if (b >= 3073) m.w32(w + 0xC, static_cast<u32>(0x3000 - 3 * b));
    }
    s32 size = ((m.r32s(w + 0xC) - 0x200) >> 5) + 24;
    if (size < 241) size += rand() & 7;
    else size = 240;
    const s32 low = floor - 0x380000;
    const s32 eye_y = m.r32s(kEye + 4);
    u32 below = static_cast<u32>(eye_y - low);
    if (low < eye_y) {
        below = static_cast<u32>((eye_y - low) >> 14);
        size -= static_cast<s32>(below);
    }
    if (size <= 0) return below;
    m.w32(w + 0x10, static_cast<u32>(size));
    return flare_sprites(w, kScratch);
}

u32 Game::flare_sprites(u32 work, u32 scratch) {
    cover(0x8008B7ACu);
    Frame f(*this, 8);
    u32 q = m.r32(scratch + 4);
    for (u32 i = 0; i < 10; ++i) {
        const u32 e = kFlareTable + 8 * i;
        const s32 level = m.r32s(work + 0x10) - m.r8(e + 1);
        if (level <= 0) continue;
        const s32 along = m.r16s(e + 6);
        m.w16(work + 0x1C, static_cast<u32>(trunc12(m.r16s(work + 8) * along)));
        m.w16(work + 0x1E, static_cast<u32>(trunc12(m.r16s(work + 0xA) * along)));
        const u32 colour = grey(static_cast<u32>(level)) | 0x2E000000u;
        const u32 texture = m.r8(e);
        if (texture < 2) {
            const u32 u = texture << 5;
            m.w16(work + 0x2C, (u + 0xC0) | 0x3F00u);
            m.w32(work + 0x28, (u + 0xDF) | 0x00382000u);
            m.w16(work + 0x2E, (u + 0xDF) | 0x3F00u);
            m.w32(work + 0x24, (m.r16(e + 2) << 16) | ((u + 0xC0) | 0x2000u));
            const s32 half = m.r16s(e + 4);
            for (u32 j = 0; j < 4; ++j, q += 0x28) {
                m.w32(q, tag(q + 0x28, 9));
                m.w32(q + 0x04, colour);
                m.w32(q + 0x0C, m.r32(work + 0x24));
                m.w32(q + 0x14, m.r32(work + 0x28));
                m.w16(q + 0x1C, m.r16(work + 0x2C));
                m.w16(q + 0x24, m.r16(work + 0x2E));
                m.w32(q + 0x20, m.r32(work + 0x1C));
                const s32 x = m.r16s(work + 0x1C);
                m.w16(q + 0x18, static_cast<u32>((j & 1) ? x + half : x - half));
                m.w16(q + 0x08, m.r16(q + 0x18));
                m.w16(q + 0x10, m.r16(work + 0x1C));
                const s32 y = m.r16s(work + 0x1E);
                m.w16(q + 0x12, static_cast<u32>((j >> 1) ? y + half : y - half));
                m.w16(q + 0x0A, m.r16(q + 0x12));
                m.w16(q + 0x1A, m.r16(work + 0x1E));
            }
        } else {
            const u32 u = (texture & 1u) << 4;
            m.w32(q, tag(q + 0x28, 9));
            m.w32(q + 0x04, colour);
            m.w32(q + 0x14, (u + 0xEF) | 0x380000u);
            m.w16(q + 0x1C, (u + 0xE0) | 0xF00u);
            m.w16(q + 0x24, (u + 0xEF) | 0xF00u);
            m.w32(q + 0x0C, (m.r16(e + 2) << 16) | (u + 0xE0));
            const u32 half = static_cast<u32>(static_cast<s32>(m.r16s(e + 4)));
            const u32 x = m.r16(work + 0x1C), y = m.r16(work + 0x1E);
            m.w16(q + 0x08, x - half);
            m.w16(q + 0x0A, y - half);
            m.w16(q + 0x10, x + half);
            m.w16(q + 0x12, y - half);
            m.w16(q + 0x18, x - half);
            m.w16(q + 0x1A, y + half);
            m.w16(q + 0x20, x + half);
            m.w16(q + 0x22, y + half);
            q += 0x28;
        }
    }
    const u32 start = m.r32(scratch + 4);
    splice(m, m.r32(scratch + 0), start, q - 0x28);
    m.w32(scratch + 4, q);
    return start << 8;
}
}

namespace ac3::sim {
namespace {
constexpr u32 kHaloClut = 0x4F56u, kHaloPage = 0x39u, kFlarePage = 0x38u;

u32 rgb_scaled(s32 r, s32 g, s32 b, s32 scale) {
    auto c = [&](s32 v) { return static_cast<u32>(std::clamp(v * scale / 4096, 0, 255)); };
    return c(r) | (c(g) << 8) | (c(b) << 16);
}

struct Ghost {
    float along;
    s32 size;
    u32 texture;
    s32 r, g, b;
};
constexpr Ghost kGhosts[] = {
    {1.35f, 26, 1, 0x30, 0x40, 0x58}, {0.62f, 9, 2, 0x40, 0x68, 0x90}, {0.30f, 20, 0, 0x30, 0x78, 0x48},
    {-0.12f, 7, 3, 0x90, 0x58, 0x20}, {-0.38f, 32, 1, 0x20, 0x34, 0x68}, {-0.66f, 14, 2, 0x78, 0x30, 0x78},
    {-0.95f, 44, 0, 0x40, 0x40, 0x58}, {-1.30f, 11, 3, 0x88, 0x64, 0x30},
};
}

void Game::sun_enhance(s32 kind) {
    const u32 w = kWork;
    const s32 sx = m.r16s(w + 8), sy = m.r16s(w + 0xA);
    const s32 bright = std::clamp((312 - (iabs(sx) + iabs(sy))) * 4096 / 312, 0, 4096);
    const float across = static_cast<float>(gte.x_squeeze) / 65536.0f;
    auto wide = [&](s32 r) { return std::max(1, static_cast<s32>(static_cast<float>(r) * across)); };
    u32 q = m.r32(kPackets), last = 0;
    auto quad = [&](s32 x0, s32 y0, s32 x1, s32 y1, u32 u0, u32 v0, u32 u1, u32 v1, u32 clut, u32 page, u32 colour) {
        last = q;
        m.w32(q, tag(q + 0x28, 9));
        m.w32(q + 0x04, colour | 0x2E000000u);
        m.w16(q + 0x08, static_cast<u32>(x0));
        m.w16(q + 0x0A, static_cast<u32>(y0));
        m.w32(q + 0x0C, u0 | (v0 << 8) | (clut << 16));
        m.w16(q + 0x10, static_cast<u32>(x1));
        m.w16(q + 0x12, static_cast<u32>(y0));
        m.w32(q + 0x14, u1 | (v0 << 8) | (page << 16));
        m.w16(q + 0x18, static_cast<u32>(x0));
        m.w16(q + 0x1A, static_cast<u32>(y1));
        m.w16(q + 0x1C, u0 | (v1 << 8));
        m.w16(q + 0x20, static_cast<u32>(x1));
        m.w16(q + 0x22, static_cast<u32>(y1));
        m.w16(q + 0x24, u1 | (v1 << 8));
        q += 0x28;
    };
    auto disc = [&](s32 cx, s32 cy, s32 rx, s32 ry, u32 u0, u32 clut, u32 page, u32 colour) {
        for (u32 j = 0; j < 4; ++j) {
            const s32 ex = (j & 1) ? cx + rx : cx - rx, ey = (j & 2) ? cy + ry : cy - ry;
            quad(ex, ey, cx, cy, u0, 0x20, u0 + 0x1F, 0x3F, clut, page, colour);
        }
    };
    auto halo = [&](s32 cx, s32 cy, s32 r, u32 colour) { disc(cx, cy, wide(r), r, 0xA0, kHaloClut, kHaloPage, colour); };

    if (option(&Settings::hdr_sun) && (kind & 0xFF00)) {
        const u32 first = q;
        const s32 lift = 2048 + bright / 2;
        halo(sx, sy, 92, rgb_scaled(0x2C, 0x22, 0x16, bright));
        halo(sx, sy, 50, rgb_scaled(0xA0, 0x80, 0x50, lift));
        halo(sx, sy, 20, rgb_scaled(0xFF, 0xF4, 0xDC, 4096));
        splice(m, m.r32(kOt) + 4 * 4094, first, last);
        last = 0;
    }
    const u32 front = q;
    if (option(&Settings::lens_flare) && (kind & 0xFF)) {
        auto clut_of = [&](u32 texture) {
            for (u32 i = 0; i < 10; ++i)
                if (m.r8(kFlareTable + 8 * i) == texture) return static_cast<u32>(m.r16(kFlareTable + 8 * i + 2));
            return static_cast<u32>(m.r16(kFlareTable + 2));
        };
        const s32 spread = std::clamp((iabs(sx) + iabs(sy)) * 4096 / 80, 1024, 4096);
        for (const Ghost& g : kGhosts) {
            const s32 x = static_cast<s32>(static_cast<float>(sx) * g.along);
            const s32 y = static_cast<s32>(static_cast<float>(sy) * g.along);
            const u32 colour = rgb_scaled(g.r, g.g, g.b, bright * spread / 4096);
            if (g.texture < 2) {
                disc(x, y, wide(g.size), g.size, (g.texture << 5) + 0xC0, clut_of(g.texture), kFlarePage, colour);
            } else {
                const u32 u = ((g.texture & 1u) << 4) + 0xE0;
                const s32 hx = wide(g.size), hy = g.size;
                quad(x - hx, y - hy, x + hx, y + hy, u, 0, u + 0xF, 0xF, clut_of(g.texture), kFlarePage, colour);
            }
        }
        disc(sx, sy, wide(150), 3, 0xA0, kHaloClut, kHaloPage, rgb_scaled(0x58, 0x68, 0x90, 2048 + bright / 2));
    }
    if (option(&Settings::hdr_sun) && (kind & 0xFF00) && bright > 2048) {
        const s32 v = (bright - 2048) * 16 / 2048;
        m.w32(q, tag(q + 0x08, 1));
        m.w32(q + 4, 0xE1000000u | kFlarePage);
        q += 0x08;
        last = q;
        m.w32(q, tag(q + 0x10, 3));
        m.w32(q + 4, rgb_scaled(v, v * 7 / 8, v * 5 / 8, 4096) | 0x62000000u);
        m.w32(q + 8, 0xFF88FF60u);
        m.w32(q + 0xC, 0x00F00140u);
        q += 0x10;
    }
    if (last) splice(m, m.r32(kOt), front, last);
    m.w32(kPackets, q);
}
}
