#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSavedView = 0x800BCF58u;
constexpr u32 kHeapMark = 0x800BCF5Cu;
constexpr u32 kCursor = 0x800BCF60u;
constexpr u32 kState = 0x800BCF64u;
constexpr u32 kLength = 0x800BCF68u;
constexpr u32 kPlaying = 0x800BCF6Cu;
constexpr u32 kSkipAsked = 0x800BCF70u;
constexpr u32 kFramesLeft = 0x800BCF74u;
constexpr u32 kLoaded = 0x800BCF78u;
constexpr u32 kChunk = 0x800BCF80u;
constexpr u32 kSector = 0x800BCF84u;
constexpr u32 kChunkFrames = 0x800BCF88u;
constexpr u32 kChunkFrame = 0x800BCF8Au;
constexpr u32 kStarved = 0x800BCF8Cu;
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kMask = 0x800BCC10u;
constexpr u32 kHandlers = 0x800B0F10u;
constexpr u32 kSceneStream = 0x800B0F34u;
constexpr u32 kSceneEntry = 0x800B0FF4u;
constexpr u32 kFrameBytes = 572u;

u32 world_of(Memory& m) { return m.r32(g::kWorld); }
u32 scene_of(Memory& m) { return m.r32(g::kScene); }
u32 scene_number(Memory& m) { return m.r32(m.r32(g::kScene) + 4152); }
}

void Game::scene_restore(u32 keep) {
    cover(0x80038D1Cu);
    Frame f(*this, 0x48);
    const u32 view = m.r32(world_of(m) + 40);
    word_copy(view, m.r32(kSavedView), 99);
    m.w32(f.at(0x10), m.r32(world_of(m) + 648));
    view_fog(view, m.r16s(view + 40), m.r16s(view + 42), m.r16s(view + 36), m.r32(f.at(0x10)));
    if (keep) {
        const u32 saved = f.at(0x18);
        for (u32 i = 0; i < 64; ++i) {
            word_copy(saved, m.r32(kSavedView) + 396 + 32 * i, 8);
            const u32 e = slot(i);
            m.w32(e, m.r32(saved));
            if (i == 0) player_place(e + 8);
        }
    } else {
        for (u32 i = 0; i < 64; ++i) word_copy(slot(i), m.r32(kSavedView) + 396 + 32 * i, 8);
    }
    heap_set(m.r32(kHeapMark));
    effects_reset();
    const u32 sc = scene_of(m);
    m.w32(sc, m.r32(sc) & ~0x10000000u);
    weapons_reset();
    mission_end();
}

u32 Game::scene_weapons_clear() {
    cover(0x80038E48u);
    const u32 w = m.r32(g::kWeapons);
    m.w32(w + 5448, 0);
    return w;
}

void Game::scene_entities_hide() {
    cover(0x80038E58u);
    for (u32 i = 0; i < 64; ++i) {
        const u32 e = slot(i);
        m.w32(e, m.r32(e) & ~0x10000u);
    }
}

u32 Game::scene_rec_view(u32 rec) {
    cover(0x80038E98u);
    const u32 view = m.r32(world_of(m) + 40);
    m.w16(view + 20, m.r16(rec + 4));
    m.w16(view + 22, m.r16(rec + 6));
    m.w16(view + 24, m.r16(rec + 8));
    m.w32(view + 4, m.r32(rec + 12) << 12);
    m.w32(view + 8, m.r32(rec + 16) << 12);
    m.w32(view + 12, m.r32(rec + 20) << 12);
    return rec + 24;
}

u32 Game::scene_rec_camera(u32 rec) {
    cover(0x80038F00u);
    Frame f(*this, 0x28);
    const u32 view = m.r32(world_of(m) + 40);
    s32 h = m.r16s(view + 36), near = m.r16s(view + 40), far = m.r16s(view + 42);
    u32 change = 0;
    if (m.r16(rec + 2) && h != m.r16s(rec + 2)) {
        h = m.r16s(rec + 2);
        change = 1;
    }
    if (m.r16(rec + 4) && near != m.r16s(rec + 4)) {
        near = m.r16s(rec + 4);
        change = 2;
    }
    if (m.r16(rec + 6) && far != m.r16s(rec + 6)) {
        far = m.r16s(rec + 6);
        change = 2;
    }
    if (change == 1) {
        view_projection(m.r32(world_of(m) + 40), static_cast<u32>(h));
    } else if (change == 2) {
        m.w32(f.at(0x10), m.r32(world_of(m) + 648));
        view_fog(m.r32(world_of(m) + 40), near, far, h, m.r32(f.at(0x10)));
    }
    u32 mode = 0x2000;
    const u32 k = (m.r32(rec) >> 8) & 3u;
    u32 subject = m.r32(m.r32(g::kSlots));
    if (k) {
        subject = m.r32(m.r32(g::kSlots) + ((m.r32(rec) >> 8) & 0xFCu));
        if (k == 1) mode = 1;
        else if (k == 2) mode = 2;
    }
    view_set(view, mode, subject);
    return rec + 8;
}

u32 Game::scene_rec_entity(u32 rec, bool trail) {
    cover(trail ? 0x80039028u : 0x800390E4u);
    const u32 e = m.r32(m.r32(g::kSlots) + ((m.r32(rec) >> 6) & 0xFCu));
    m.w32(e, m.r32(e) | 0x14000u);
    m.w16(e + 24, m.r16(rec + 4));
    m.w16(e + 26, m.r16(rec + 6));
    m.w16(e + 28, m.r16(rec + 8));
    m.w32(e + 8, m.r32(rec + 12) << 12);
    m.w32(e + 12, m.r32(rec + 16) << 12);
    m.w32(e + 16, m.r32(rec + 20) << 12);
    if (trail) m.w8(e + 5, ((m.r32(rec) >> 14) & 1u) ? (m.r8(e + 5) | 8u) : (m.r8(e + 5) & 0xF7u));
    return rec + 24;
}

u32 Game::scene_rec_missile(u32 rec) {
    cover(0x8003916Cu);
    Frame f(*this, 0x30);
    const u32 at = f.at(0x10);
    m.w32(at, m.r32(rec + 12) << 12);
    m.w32(at + 4, m.r32(rec + 16) << 12);
    m.w32(at + 8, m.r32(rec + 20) << 12);
    m.w16(at + 16, m.r16(rec + 4));
    m.w16(at + 18, m.r16(rec + 6));
    m.w16(at + 20, m.r16(rec + 8));
    const u32 w = m.r32(rec);
    if (((w >> 8) & 7u) == 0) {
        if ((w >> 21) & 1u) missile_replay_show((w & 0xFFFFu) >> 11, 6145, at);
        else missile_replay_hide((w & 0xFFFFu) >> 11);
    }
    return rec + 24;
}

u32 Game::scene_rec_effect(u32 rec) {
    cover(0x80039230u);
    Frame f(*this, 0x18);
    const u32 k = (m.r32(rec) >> 8) & 0x1Fu;
    switch (k) {
    case 0: case 1: case 2: case 3: case 4: case 5: emit_transient_particle(rec, static_cast<s32>(k)); break;
    case 8: case 9: emit_billboard(rec, k - 8); break;
    case 10: case 11: case 12: emit_seeded_cloud(rec, m.r16s(rec + 8), k - 10); break;
    case 13: case 17: emit_hot_puff(rec); break;
    case 14: case 18: case 19: emit_fire_smoke(rec); break;
    case 15: emit_rising_smoke(rec); break;
    case 20: emit_trailing_smoke(rec); break;
    case 21: case 22: case 23: emit_travelling_debris(rec, k - 21); break;
    case 31: emit_debris_burst(rec, 4); break;
    default: break;
    }
    return rec + 24;
}

u32 Game::scene_rec_skip(u32 rec, u32 fn) {
    cover(fn);
    return rec + 4;
}

u32 Game::scene_rec_segment(u32 rec) {
    cover(0x80039388u);
    Frame f(*this, 0x18);
    emit_short_segment(rec);
    return rec + 32;
}

u32 Game::scene_records_run(u32 rec) {
    cover(0x800393B8u);
    Frame f(*this, 0x18);
    for (u32 type = m.r32(rec) & 0x7Fu; type; type = m.r32(rec) & 0x7Fu) {
        const u32 fn = m.r32(kHandlers + 4 * type);
        switch (fn) {
        case 0x800393B0u: rec = scene_rec_skip(rec, fn); break;
        case 0x80038E98u: rec = scene_rec_view(rec); break;
        case 0x80039028u: rec = scene_rec_entity(rec, true); break;
        case 0x800390E4u: rec = scene_rec_entity(rec, false); break;
        case 0x8003916Cu: rec = scene_rec_missile(rec); break;
        case 0x80039230u: rec = scene_rec_effect(rec); break;
        case 0x80039380u: rec = scene_rec_skip(rec, fn); break;
        case 0x80038F00u: rec = scene_rec_camera(rec); break;
        case 0x80039388u: rec = scene_rec_segment(rec); break;
        default:
            unported(fn, "a cut-scene record of no known type");
            return rec;
        }
    }
    return rec;
}

u32 Game::scene_stream_loaded() {
    cover(0x80039420u);
    m.w32(kLoaded, 1);
    return 1;
}

void Game::scene_begin() {
    cover(0x80039430u);
    Frame f(*this, 0x20);
    m.w32(kCursor, 0x80201000u);
    const u32 entry = m.r32(m.r32(world_of(m) + 620) + 4 * m.r8(kSceneEntry + scene_number(m)));
    m.w32(kLength, m.r32(entry) - 1);
    m.w32(kSkipAsked, 0);
    m.w16(kFramesLeft, m.r16(kLength));
    view_fixed();
    const u32 w = world_of(m);
    m.w32(w, m.r32(w) | 1u);
    const u32 sc = scene_of(m);
    m.w32(sc, m.r32(sc) | 0x10000000u);
    cd_reset(0, 0);
    const u32 ring = mem_alloc(18624);
    m.w32(kLoaded, 0);
    const u32 stream = static_cast<u32>(m.r16s(kSceneStream + 2 * scene_number(m)));
    m.w32(f.at(0x10), 0x80039420u);
    kcd_request_video(stream, ring, 18624, 0x3000, 0x80039420u);
    sound_scene_reset();
    effects_reset();
    trails_reset();
    weapons_reset();
    radio_face(1, 0, 0);
    m.w32(kPlaying, 0);
    if (m.r16s(scene_of(m) + 940)) {
        switch (scene_number(m)) {
        case 5: case 15: case 19: case 21: case 37: case 39: case 40: case 46: case 54: case 60: case 72: case 75:
        case 76: case 84: case 88: case 89: case 90: case 92: case 93:
            request_exit(64, m.r32(world_of(m) + 8));
            m.w32(kState, 5);
            return;
        default: break;
        }
    }
    request_exit(32, m.r32(world_of(m) + 8));
    m.w32(kState, 0);
}

void Game::scene_end() {
    cover(0x800395D4u);
    Frame f(*this, 0x18);
    cd_reset(0, 0);
    m.w32(kCursor, m.r32(m.r32(world_of(m) + 620) + 4 * m.r8(kSceneEntry + scene_number(m))) + 4);
    scene_weapons_clear();
    scene_entities_hide();
    scene_records_run(m.r32(kCursor));
    radio_face(255, 0, 0);
    sound_scene_stop();
    const u32 sc = scene_of(m);
    m.w32(sc, m.r32(sc) & ~0x10000000u);
    const s32 n = m.r32s(sc + 4152);
    scene_restore(n == 2 || n == 4 || n == 20 || n == 30 || n == 68 || n == 85 ? 1u : 0u);
}

void Game::scene_fades() {
    cover(0x800399D4u);
    Frame f(*this, 0x20);
    const s32 frame = m.r32s(kLength) - static_cast<s32>(m.r16(kFramesLeft)) + 2;
    if (m.r32(scene_of(m) + 300) & 0x40u) {
        m.w32(kSkipAsked, 1);
        if (m.r32(kState) == 1) {
            m.w32(kState, 3);
        } else if (m.r32(kState) == 2) {
            m.w32(kState, 4);
            m.w32(kMask, 5760);
            m.w32(kMask + 4, 6144);
        }
    }
    if (m.r32(kState) - 1u >= 3u) {
        if (m.r32(kState) == 4) {
            m.w32(kMask, m.r32(kMask) - 128);
            if (m.r32s(kMask) <= 0) {
                m.w32(kMask, 0);
                m.w32(kState, 5);
                request_exit(64, m.r32(world_of(m) + 8));
            }
            if (!m.r32(kSkipAsked) && m.r16(kFramesLeft) < 0x19u) m.w32(kMask + 4, m.r32(kMask + 4) - 256);
            if (m.r32s(kMask + 4) <= 0) m.w32(kMask + 4, 0);
        } else if (m.r32(kState) == 0 || m.r32(kState) == 5) {
            m.w32(kMask, 0);
            m.w32(kMask + 4, 0);
            m.w32(kMask + 8, 0);
        }
    } else {
        const s32 shown = m.r32s(kLength) - static_cast<s32>(m.r16(kFramesLeft)) + 1;
        if (m.r32s(kLength) > 0) {
            m.w32(kMask, static_cast<u32>(shown) << 7);
            m.w32(kMask + 4, static_cast<u32>(shown) << 8);
        } else {
            m.w32(kMask, 0);
            m.w32(kMask + 4, 0);
        }
        if (m.r32s(kMask) < 0) m.w32(kMask, 0);
        if (m.r32s(kMask + 4) < 0) m.w32(kMask + 4, 0);
        if (m.r32s(kMask) >= 6144) {
            if (m.r32(kState) == 1) {
                m.w32(kState, 2);
            } else if (m.r32(kState) == 3) {
                m.w32(kState, 4);
                m.w32(kMask, 5760);
                m.w32(kMask + 4, 6144);
            }
        }
        if (m.r16(kFramesLeft) < 0x31u) {
            m.w32(kState, 4);
            m.w32(kMask, 5760);
            m.w32(kMask + 4, 6144);
        }
    }
    u32 kind = 0;
    const u32 st = m.r32(kState);
    if (st - 4u < 2u) {
        switch (scene_number(m)) {
        case 9: case 12: case 26: case 28: case 48: case 58: case 61: case 62: case 64: case 66: case 67: case 91:
            kind = 1;
            break;
        default: break;
        }
    } else if (st == 1 || st == 3 || st == 0) {
        kind = scene_number(m) == 42 ? 1u : 0u;
    }
    const u32 w = world_of(m);
    const u32 p0 = effect_masked_flash(m.r32(scene_of(m) + 12), m.r32(w + 20), kind, m.r32s(kMask));
    const u32 w1 = world_of(m);
    const u32 ot = m.r32(w1 + 16);
    const s32 level = m.r32s(kMask + 4) - 2048;
    m.w32(w1 + 20, p0);
    const u32 p1 = effect_letterbox(ot, p0, level);
    m.w32(world_of(m) + 20, p1);
    u32 fades = 0;
    s32 fade = 0;
    const s32 length = m.r32s(kLength);
    const s32 count = m.r32s(kMask + 8);
    auto tail = [&](s32 from, u32 which, u32 shift) {
        if (frame < from + 1 || length < frame) return;
        fades = which;
        fade = wrap::div(static_cast<s32>(static_cast<u32>(count) << shift), length - from).lo;
    };
    auto head90 = [&] {
        if (static_cast<u32>(frame - 1) >= 0x5Au) return;
        fades = 1;
        fade = 4096 - static_cast<s32>(static_cast<u32>(count) << 12) / 90;
    };
    switch (scene_number(m)) {
    case 39: tail(150, 1, 12); break;
    case 52: tail(150, 1, 12); break;
    case 58: tail(510, 2, 12); break;
    case 89: tail(570, 1, 12); break;
    case 91: tail(330, 2, 11); break;
    case 40: case 90: head90(); break;
    case 53: head90(); break;
    case 42:
        if (static_cast<u32>(frame - 1) < 0x70u) {
            fades = 2;
            const u32 fx = m.r32(kFx);
            m.w32(fx + 1480, m.r32(fx + 1480) | 1u);
            fade = 2560 - static_cast<s32>(static_cast<u32>(count) * 2560u) / 114;
        }
        if (static_cast<u32>(frame - 113) < 0x10u) {
            fades |= 1u;
            fade = 1024;
            const u32 fx = m.r32(kFx);
            m.w32(fx + 1480, m.r32(fx + 1480) | 1u);
        }
        break;
    default: break;
    }
    if (fades & 1u) {
        const u32 w2 = world_of(m);
        const u32 p = noise_overlay(m.r32(w2 + 16) + 8, m.r32(w2 + 20), fade);
        m.w32(world_of(m) + 20, p);
        const u32 fx = m.r32(kFx);
        const u32 flags = m.r32(fx + 1480);
        m.w32(kMask + 8, m.r32(kMask + 8) + 1);
        m.w32(fx + 1480, flags | 1u);
    }
    if (fades & 2u) {
        const u32 w2 = world_of(m);
        const u32 p = fade_overlay(m.r32(w2 + 16), m.r32(w2 + 20), fade);
        const u32 n = m.r32(kMask + 8) + 1;
        m.w32(world_of(m) + 20, p);
        m.w32(kMask + 8, n);
    }
}

void Game::scene_tick() {
    cover(0x800396E8u);
    Frame f(*this, 0x18);
    const u32 sub = m.r32(world_of(m) + 4);
    if (sub == 16) {
        scene_begin();
        scene_fades();
        m.w16(kChunkFrame, 0);
        m.w32(kStarved, 0);
        return;
    }
    if (sub == 64) {
        scene_fades();
        scene_end();
        m.w32(kState, 0);
        return;
    }
    if (m.r16(kFramesLeft) < 0x17u) radio_face(255, 0, 0);
    scene_fades();
    const s32 waiting = static_cast<s32>(stream_ring_mark());
    auto give_up = [&] {
        m.w32(kState, 5);
        request_exit(64, m.r32(world_of(m) + 8));
    };
    if (!m.r32(kPlaying)) {
        if (waiting < 4 && m.r32(kLoaded) != 1) {
            const s32 n = m.r32s(kStarved) + 1;
            m.w32(kStarved, static_cast<u32>(n));
            if (n >= 150) give_up();
            return;
        }
        const u32 st = m.r32(kState);
        m.w32(kPlaying, 1);
        m.w16(kChunkFrame, 0);
        m.w32(kStarved, 0);
        if (!st) m.w32(kState, 1);
        return;
    }
    if (waiting < 2 && m.r32(kLoaded) != 1) {
        m.w32(kPlaying, 0);
        return;
    }
    if (!m.r16s(kChunkFrame)) {
        if (kcd_ring_get(kChunk, kSector)) {
            const u32 loaded = m.r32(kLoaded);
            m.w32(kPlaying, 0);
            if (loaded) give_up();
            return;
        }
        m.w16(kChunkFrames, m.r8(m.r32(kSector) + 8));
    }
    m.w32(kCursor, m.r32(kChunk) + kFrameBytes * static_cast<u32>(m.r16s(kChunkFrame)));
    const u32 number = scene_number(m);
    m.w16(kFramesLeft, m.r16(m.r32(kSector) + 6) - m.r16(kChunkFrame));
    if (number == 38)
        cinematic_fire_update(static_cast<s32>(m.r32(kLength) - m.r16(kFramesLeft) + 2));
    else if (number == 72)
        scenery_fire_trigger(static_cast<s32>(m.r32(kLength) - m.r16(kFramesLeft) + 2));
    scene_weapons_clear();
    scene_entities_hide();
    const u32 head = m.r32(m.r32(kCursor));
    transient_particles_clear();
    scene_records_run(m.r32(kCursor) + 4);
    sim_step8();
    const u32 number2 = scene_number(m);
    if (number2 != 75 && number2 != 37) sound_scene(m.r32(kCursor) + ((head >> 2) << 2));
    const s16 next = static_cast<s16>(m.r16(kChunkFrame) + 1u);
    m.w16(kChunkFrame, static_cast<u32>(next));
    if (next >= m.r16s(kChunkFrames)) {
        kcd_ring_free(m.r32(kSector));
        m.w16(kChunkFrame, 0);
    }
}
}
