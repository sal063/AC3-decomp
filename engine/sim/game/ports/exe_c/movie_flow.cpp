#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kWork = 0x800BD050u, kPlayer = 0x800BD054u;
constexpr u32 kOptions = 0x800BCDA0u, kVideoTable = 0x800BCD94u;
constexpr u32 kSavedRectangle = 0x800BCDACu;
constexpr u32 kDisplay = 0x800BCE48u;
void rectangle(Memory& m, u32 at, u32 height) {
    m.w16(at, 0); m.w16(at + 2, 0); m.w16(at + 4, 320); m.w16(at + 6, height);
}
}

void Game::movie_initialize(u32 saved, u32 clear, bool mission) {
    const u32 work = mem_alloc(100);
    m.w32(kWork, work);
    m.w32(work + 4, mem_alloc(0x2000));
    m.w32(work + 8, mem_alloc(0x2000));
    m.w32(work, 0); m.w16(work + 76, 320); m.w16(work + 78, 480);
    if (mission) m.w32(work + 96, 120);
    const u32 scene = m.r32(g::kScene);
    const u32 entry = m.r32(kVideoTable) + 8 * m.r8(scene + 936);
    m.w32(work + 84, 0); m.w32(work + 80, static_cast<u32>(m.r16s(entry)));
    m.w32(saved, m.r32(kSavedRectangle)); m.w32(saved + 4, m.r32(kSavedRectangle + 4));
    const u32 image = mem_alloc(10240);
    m.w32(work + 88, image); hw->store_image(saved, image);
    if (mission) {
        m.w16(saved, 768); m.w16(saved + 2, 0); m.w16(saved + 4, 256); m.w16(saved + 6, 284);
        const u32 strips = mem_alloc(145408);
        m.w32(work + 92, strips); hw->store_image(saved, strips);
    }
    rectangle(m, clear, mission ? 496 : 480);
    hw->clear_image(clear, 0, 0, 0);
    const bool captions = (m.r8(scene + 924) & m.r8(scene + 935)) && m.r16s(entry + 2) == 1;
    m.w32(m.r32(kOptions) + 4, captions);
    if (captions) radio_face(10, 0, 0);
}

u32 Game::movie_phase() {
    cover(0x800582F8u);
    Frame frame(*this, 0x48);
    const u32 work = m.r32(kWork);
    switch (m.r32(work)) {
    case 0: {
        const u32 player = mem_peek(0);
        for (u32 dimension = 0; dimension < 8; ++dimension)
            m.w32(frame.at(0x10 + 4 * dimension), static_cast<u32>(m.r16s(work + 60 + 2 * dimension)));
        stream_player_setup(m.r32(work + 80), m.r32(work + 84), player, 18368,
            m.r32(frame.at(0x10)), m.r32(frame.at(0x14)), m.r32(frame.at(0x18)), m.r32(frame.at(0x1C)),
            m.r32(frame.at(0x20)), m.r32(frame.at(0x24)), m.r32(frame.at(0x28)), m.r32(frame.at(0x2C)));
        stream_video_request(); m.w32(m.r32(kWork), 1); break;
    }
    case 1:
        if (!m.r32(m.r32(g::kScene) + 4)) {
            m.w32(m.r32(g::kScene) + 4, 0);
            if (movie_wait(1)) m.w32(m.r32(kWork), 2);
            else {
                const u32 timeout = m.r32(m.r32(kWork) + 96) - 1;
                m.w32(m.r32(kWork) + 96, timeout);
                if (static_cast<s32>(timeout) <= 0) m.w8(m.r32(kPlayer) + 160, 1);
                if (m.r8(m.r32(kPlayer) + 160)) {
                    if (m.r32(m.r32(kOptions) + 4)) radio_face(255, 0, 0);
                    m.w32(m.r32(kWork), 3);
                }
            }
        }
        m.w32(m.r32(kPlayer) + 4, m.r32(m.r32(kPlayer) + 4) + 1); break;
    case 2:
        if (m.r32(m.r32(g::kScene) + 300) & 0x40u) m.w8(m.r32(kPlayer) + 160, 1);
        if (movie_tick()) { m.w32(m.r32(kWork), 3); m.w32(m.r32(kPlayer) + 4, 0); }
        break;
    case 3:
        stream_video_reset(); stream_video_ready();
        rectangle(m, frame.at(0x30), 480); hw->clear_image(frame.at(0x30), 0, 0, 0);
        if (!m.r32(m.r32(g::kScene) + 4)) m.w32(m.r32(kWork), 4);
        break;
    case 4:
        rectangle(m, frame.at(0x38), 480); hw->clear_image(frame.at(0x38), 0, 0, 0);
        m.w32(m.r32(kWork), 5); break;
    case 5: m.w32(m.r32(kWork), 6); break;
    case 6: {
        const u32 scene = m.r32(g::kScene), display = m.r32(kDisplay);
        m.w32(scene, m.r32(scene) & ~0x10000000u);
        const u32 flags = m.r32(display);
        m.w32(display + 20, 2); m.w32(display + 4, 0); m.w32(display, flags | 5);
        m.w32(m.r32(kWork), 7); break;
    }
    case 7: m.w32(m.r32(kWork), 8); break;
    case 8: m.w32(m.r32(kWork), 9); break;
    case 9: return 1;
    default: break;
    }
    return 0;
}

u32 Game::movie_mission_init() {
    cover(0x8005AE8Cu);
    Frame frame(*this, 0x30);
    const u32 scene = m.r32(g::kScene);
    m.w32(scene, m.r32(scene) | 0x10000000u); m.w32(m.r32(kDisplay) + 20, 0);
    heap_set(m.r32(m.r32(g::kWorld) + 696));
    const u32 display = m.r32(kDisplay);
    m.w8(display + 16, 0); m.w32(display + 4, 1); m.w8(display + 17, 0); m.w8(display + 18, 0);
    m.w32(display, m.r32(display) | 9);
    movie_initialize( frame.at(0x10), frame.at(0x18), true);
    const u32 video = m.r8(scene + 936);
    const bool skip = m.r16(scene + 940) && (video == 12 || video == 27);
    if (skip) {
        m.w32(display + 20, 2); m.w32(display + 4, 0);
        m.w32(display, m.r32(display) | 5);
    }
    request_exit(skip ? 0x80000u : 0x40000u, m.r32(m.r32(g::kWorld) + 8));
    return 0;
}

u32 Game::movie_standalone_init() {
    cover(0x8005B308u);
    Frame frame(*this, 0x30);
    const u32 scene = m.r32(g::kScene);
    if (!m.r32(scene + 4)) return 0;
    m.w32(scene, m.r32(scene) | 0x10000000u);
    const u32 display = m.r32(kDisplay);
    m.w8(display + 16, 0); m.w32(display + 20, 0); m.w32(display + 4, 1);
    m.w8(display + 17, 0); m.w8(display + 18, 0); m.w32(display, m.r32(display) | 9);
    movie_initialize( frame.at(0x10), frame.at(0x18), false);
    return 0x4000000u;
}

u32 Game::movie_mission_draw() {
    cover(0x8005B124u);
    Frame frame(*this, 0x18);
    const u32 mode = m.r32(m.r32(g::kWorld) + 4);
    if (mode == 0x20000u) return movie_mission_init();
    if (mode == 0x80000u) return video_restore_mission();
    const u32 field = m.r32(m.r32(g::kScene) + 4), work = m.r32(kWork);
    m.w32(work + 56, work + 12 + 20 * field); m.w32(work + 52, m.r32(work + 4 + 4 * field));
    clear_otag_r(work + 12 + 20 * field, 5);
    if (movie_phase()) {
        const u32 world = m.r32(g::kWorld);
        request_exit(0x80000u, m.r32(world + 8)); return 0;
    }
    if (m.r32(m.r32(kOptions) + 4)) {
        const u32 packet = face_draw(m.r32(m.r32(kWork) + 56), m.r32(m.r32(kWork) + 52));
        m.w32(m.r32(kWork) + 52, packet);
    }
    hw->draw_otag(m.r32(m.r32(kWork) + 56) + 16); return 0;
}

u32 Game::movie_standalone_draw() {
    cover(0x8005B4E4u);
    Frame frame(*this, 0x18);
    const u32 field = m.r32(m.r32(g::kScene) + 4), work = m.r32(kWork);
    m.w32(work + 56, work + 12 + 20 * field); m.w32(work + 52, m.r32(work + 4 + 4 * field));
    clear_otag_r(work + 12 + 20 * field, 5);
    if (movie_phase()) {
        video_restore_image(); return m.r32(m.r32(g::kScene) + 928) | 0x60000000u;
    }
    if (m.r32(m.r32(kOptions) + 4)) {
        const u32 packet = face_draw(m.r32(m.r32(kWork) + 56), m.r32(m.r32(kWork) + 52));
        m.w32(m.r32(kWork) + 52, packet);
    }
    hw->draw_otag(m.r32(m.r32(kWork) + 56) + 16); return 0;
}
}
