#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kZoneTimers = 0x800BCFFCu;
constexpr u32 kZone = 0x800BD008u;
constexpr u32 kScriptsRun = 0x800BD004u;
constexpr u32 kPrompt = 0x800BCFD8u;
constexpr u32 kHold = 0x800BCFD4u;
constexpr u32 kHoldCounter = 0x800BCFD0u;
constexpr u32 kSubtitle = 0x800C7170u;
constexpr u32 kRadioScript = 0x800BD030u;
constexpr u32 kRadioStreamDone = 0x800BD01Cu;
constexpr u32 kMovie = 0x800BD048u;
constexpr u32 kSavedView = 0x800BCF58u;
constexpr u32 kHeapMark = 0x800BCF5Cu;
constexpr u32 kSubtitleLoad = 0x800BCD8Cu;
}

void Game::ground_units_init_all() {
    cover(0x80038AD4u);
    Frame f(*this, 0x18);
    ground_units_setup();
}

void Game::intro_setup() {
    cover(0x80038C28u);
    Frame f(*this, 24);
    mission_timer_setup();
    air_phase_units();
    ground_phase_units();
    alert_channels_init();
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) | 0x10000000u);
}

void Game::phase16_setup() {
    cover(0x80038C78u);
    Frame f(*this, 32);
    const u32 sc = m.r32(g::kScene);
    m.w32(sc, m.r32(sc) | 0x10000000u);
    weapons_reset();
    m.w32(kHeapMark, mem_peek(0));
    const u32 buf = mem_alloc(2444);
    m.w32(kSavedView, buf);
    word_copy(buf, m.r32(m.r32(g::kWorld) + 40), 99);
    for (u32 i = 0; i < 64; ++i) word_copy(m.r32(kSavedView) + 396 + 32 * i, slot(i), 8);
}

void Game::radio_hold_reset() {
    cover(0x80041640u);
    m.w8(kHold, 0);
    m.w32(kHoldCounter, 0);
    m.w16(kHold + 2, 0);
}

u32 Game::subtitle_reset() {
    cover(0x80041900u);
    m.w8(kSubtitle + 3, 0xFF);
    m.w8(kSubtitle + 4, 0xFF);
    m.w8(kSubtitle + 2, 0);
    m.w16(kSubtitle, 0);
    return 0x800C0000u;
}

void Game::prompt_reset() {
    cover(0x800428ECu);
    m.w8(kPrompt, 0);
}

u32 Game::zone_timers_reset() {
    cover(0x80053B5Cu);
    m.w16(kZoneTimers + 4, 0x4000);
    m.w16(kZoneTimers + 2, 0x4000);
    m.w16(kZoneTimers, 0x4000);
    m.w16(kZone, 0);
    return 0x4000;
}

u32 Game::run_script_328() {
    cover(0x80053B78u);
    Frame f(*this, 24);
    return msn_vm_exec(m.r32(m.r32(g::kMissionData) + 328));
}

void Game::scripts_run_reset() {
    cover(0x80053BA0u);
    m.w32(kScriptsRun, 0);
}

void Game::radio_script_alloc() {
    cover(0x800566F0u);
    Frame f(*this, 0x18);
    m.w32(kRadioScript, mem_alloc(16));
    radio_play_script(0);
}

u32 Game::radio_stream_ended() {
    cover(0x80056A44u);
    m.w32(kRadioStreamDone, 1);
    return 1;
}

u32 Game::gmv_page_rect(u32 out) {
    cover(0x80056AC8u);
    const u32 c = m.r32(kMovie);
    const u32 page = c + (m.r8(c + 2) ? 0u : 8u);
    m.w16(out, m.r16(page + 0x18));
    m.w16(out + 2, m.r16(page + 0x1A));
    m.w16(out + 4, m.r16(c + 0x1C));
    m.w16(out + 6, m.r16(c + 0x1E));
    return 1;
}

u32 Game::gmv_stream_ended() {
    cover(0x80056C9Cu);
    m.w8(m.r32(kMovie) + 4, 1);
    return 1;
}

u32 Game::gmv_subtitles_loaded() {
    cover(0x80057CC4u);
    const u32 p = m.r32(kSubtitleLoad);
    m.w32(p + 8, 0);
    return p;
}
}
