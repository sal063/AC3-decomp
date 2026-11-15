#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>

namespace ac3::sim {
namespace {
constexpr u32 kPrimsPtr = 0x800BCE58u;
constexpr u32 kTag = 0x00FFFFFFu;
constexpr u32 kOffsetCode = 0xE5000000u;
constexpr u32 kBulletOffset = 0x800B09B4u;
constexpr u32 kSkySlot = 16380;

u32 draw_env(Memory& m) { return m.r32(m.r32(kPrimsPtr) + 4116 * m.r32(m.r32(g::kScene) + 4)); }

void offset_packet(Memory& m, u32 p, u32 at, s32 x, s32 y) {
    m.w32(p + 4, (static_cast<u32>(y) << 11) | static_cast<u32>(x) | kOffsetCode);
    m.w32(p + 8, 0);
    m.w32(p, m.r32(at) | 0x02000000u);
    m.w32(at, p & kTag);
}
}

void Game::effects_pass() {
    cover(0x80033324u);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    set_rot_matrix(w + 76);
    set_trans_matrix(w + 76);
    set_back_color(m.r32(w + 172), m.r32(w + 176), m.r32(w + 180));
    set_color_matrix(w + 140);
    m.w32(w + 20, trails_draw(m.r32(w + 16) + 4, m.r32(w + 20)));
    set_rot_matrix(w + 76);
    set_trans_matrix(w + 76);
    m.w32(w + 20, effects_draw(m.r32(w + 16) + 4, m.r32(w + 20)));
}

void Game::segments_pass() {
    cover(0x800333D8u);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    set_rot_matrix(w + 76);
    set_trans_matrix(kBulletOffset);
    m.w32(w + 20, draw_effect_segments(m.r32(w + 16) + 4, m.r32(w + 20)));
}

void Game::model_pass() {
    cover(0x8003342Cu);
    Frame f(*this, 544);
    const u32 w = m.r32(g::kWorld);
    far_statics_.clear();
    for (u32 i = 0; i < 3; ++i) far_statics_tr_[i] = static_cast<s32>(gte.cfc2(5 + i));
    far_statics_cut_ = option(&Settings::view_distance) && !(m.r32(w) & 0x100u) ? std::min(28673, terrain_far_ - 2048) : 0;
    m.w32(w + 20,
          draw_model_pass(w + 580, m.r32(w + 16) + 4, m.r32(w + 20), f.at(0x10)));
}

void Game::world_pass() {
    cover(0x8003346Cu);
    Frame f(*this, 328);
    const u32 tiles = f.at(0x10), half = f.at(0x128), from = f.at(0x130);
    const u32 view = m.r32(m.r32(g::kWorld) + 40);
    for (u32 k = 0; k < 3; ++k) m.w16(half + 2 * k, static_cast<u32>(m.r16s(view + 20 + 2 * k) >> 2));
    auto world = [&] { return m.r32(g::kWorld); };
    auto terrain = [&](u32 pos, bool main) {
        terrain_widen_ = main && option(&Settings::view_distance) ? 1.1f : 1.0f;
        terrain_outline(pos, half, view + 28, tiles);
        const u32 w = world();
        for (u32 k = 0; k < 8; ++k) gte.ctc2(k, m.r32(w + 76 + 4 * k));
        const u32 rings = main ? option(&Settings::view_distance) : 0u;
        const s32 cut = rings ? std::min(28673, terrain_far_ - 2048) : 28673;
        terrain_widen_ = 1.0f;
        terrain_near_cut_ = cut;
        m.w32(w + 20, terrain_draw(w + 580, tiles, m.r32(w + 16) + 4, m.r32(w + 20)));
        terrain_near_cut_ = 28673;
        if (rings) far_terrain(pos, rings, cut);
    };
    if (!(m.r32(world()) & 0x100u)) terrain(view + 4, true);
    if (m.r32(world()) & 8u) {
        m.w32(from, m.r32(view + 4) + 0x40000000u);
        m.w32(from + 4, m.r32(view + 8) - 0xC00000u);
        m.w32(from + 8, m.r32(view + 12));
        terrain(from, false);
    }
    u32 w = world();
    if (m.r32(w) & 4u)
        m.w32(w + 20, terrain_draw_modes(m.r32(w + 16) + 4, m.r32(w + 20)));
    w = world();
    const u32 map = m.r32(w + 580);
    if (const u32 n = m.r32(map + 116))
        m.w32(w + 20, palette_animate(m.r32(w + 16) + 4, m.r32(w + 20), map + 120, static_cast<s32>(n)));
    w = world();
    if (!(m.r32(w) & 0x200u))
        m.w32(w + 20, sky_copy_strips(m.r32(w + 16) + 4, m.r32(w + 20), draw_env(m), w + 664));
    w = world();
    const u32 sky = m.r32(w + 16) + kSkySlot, p = m.r32(w + 20);
    switch (m.r16(m.r32(w + 580) + 2) & 7u) {
    case 1: m.w32(w + 20, draw_sky(sky, p)); break;
    case 2: m.w32(w + 20, draw_terrain_and_horizon(tiles, sky, p)); break;
    case 3: m.w32(w + 20, draw_night_sky(sky, p)); break;
    case 4: m.w32(w + 20, draw_warp_sky(sky, p)); break;
    default: break;
    }
}

u32 Game::messages_pass() {
    cover(0x800337BCu);
    Frame f(*this, 24);
    messages_frame();
    return 0;
}

void Game::face_pass() {
    cover(0x800337DCu);
    Frame f(*this, 24);
    const u32 w = m.r32(g::kWorld);
    m.w32(w + 20, face_draw(m.r32(w + 16), m.r32(w + 20)));
}

void Game::draw_passes() {
    cover(0x80033AB8u);
    const u32 w = m.r32(g::kWorld);
    const u32 env = draw_env(m);
    u32 p = m.r32(w + 20);
    const s32 x = m.r16s(env + 8), y = m.r16s(env + 10);
    const s32 width = m.r16s(env + 4), height = m.r16s(env + 6);
    m.w16(w + 660, m.r16(w + 652) + static_cast<u32>(x));
    m.w16(w + 662, m.r16(w + 654) + static_cast<u32>(y));
    m.w16(w + 666, m.r16(w + 658) + static_cast<u32>(y + (height >> 1)));
    m.w16(w + 664, m.r16(w + 656) + static_cast<u32>(x + (width >> 1)));
    auto bar = [&](u32 bw, u32 bh) {
        m.w8(p + 3, 3);
        m.w8(p + 7, 0x60);
        m.w8(p + 4, 0);
        m.w8(p + 5, 0);
        m.w8(p + 6, 0);
        m.w16(p + 8, 0);
        m.w16(p + 10, 0);
        m.w16(p + 12, bw);
        m.w16(p + 14, bh);
        const u32 ot = m.r32(m.r32(g::kWorld) + 16);
        m.w32(p, (m.r32(p) & ~kTag) | (m.r32(ot) & kTag));
        m.w32(ot, (m.r32(ot) & ~kTag) | (p & kTag));
        p += 16;
    };
    if (m.r16s(w + 656)) bar(m.r16(m.r32(g::kWorld) + 656), 240);
    if (m.r16s(m.r32(g::kWorld) + 658)) bar(static_cast<u32>(width), m.r16(m.r32(g::kWorld) + 658));
    u32 v = m.r32(g::kWorld);
    m.w32(v + 20, p);
    offset_packet(m, p, m.r32(v + 16), x, y);
    v = m.r32(g::kWorld);
    m.w32(v + 20, p + 12);
    offset_packet(m, p + 12, m.r32(v + 16) + 4, m.r16s(v + 660), m.r16s(v + 662));
    m.w32(v + 20, p + 24);
    set_geom_offset(0, 0);
    if (const u32 ws = option(&Settings::widescreen); ws && hints) {
        static constexpr s32 kSqueeze[] = {0x10000, 0xC000, 0xD555, 0x9249};
        gte.x_squeeze = kSqueeze[ws & 3u];
        hints->squeeze = static_cast<float>(gte.x_squeeze) / 65536.0f;
        hints->wide = true;
        hints->centre = static_cast<float>(x) + static_cast<float>(width) * 0.5f;
        hints->screen[0] = x;
        hints->screen[1] = y;
        hints->screen[2] = width;
        hints->screen[3] = height;
        const u32 slot0 = m.r32(m.r32(g::kWorld) + 16) & 0x00FFFFFFu;
        if (hints->hud_slot[0] != slot0 && hints->hud_slot[1] != slot0) {
            hints->hud_slot[1] = hints->hud_slot[0];
            hints->hud_slot[0] = slot0;
        }
    }
    const bool clear = fog_off();
    const u32 cueSlope = gte.cfc2(27), cueOffset = gte.cfc2(28);
    if (clear) {
        gte.ctc2(27, 0);
        gte.ctc2(28, 0);
    }
    effects_pass();
    segments_pass();
    model_pass();
    world_pass();
    if (clear) {
        gte.ctc2(27, cueSlope);
        gte.ctc2(28, cueOffset);
    }
    gte.x_squeeze = 0x10000;
    v = m.r32(g::kWorld);
    const u32 q = m.r32(v + 20);
    offset_packet(m, q, m.r32(v + 16) + kSkySlot, m.r16s(v + 664), m.r16s(v + 666));
    m.w32(v + 20, q + 12);
    set_geom_offset(160, 120);
    hud_update();
    messages_pass();
    face_pass();
    if (!(m.r32(m.r32(g::kWorld)) & 1u)) hud_cockpit();
}

void Game::flight_frame(u32 tick, u32 ra, u32 s4) {
    cover(0x800339E0u);
    flight_frame_head(ra);
    reg_s0 = tick;
    reg_ra = 0x80033A8Cu;
    reg_s4 = s4;
    run_tick(tick);
    flight_frame_view();
    draw_passes();
    sp += 32;
}
}
