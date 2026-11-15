#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPhaseKind = 0x800BCEA8u, kSinCos = 0x800104ACu;
u32 ms_of(Memory& m) { return m.r32(g::kMission); }
u32 world_of(Memory& m) { return m.r32(g::kWorld); }
u32 scene_of(Memory& m) { return m.r32(g::kScene); }
s32 mission_of(Memory& m) { return m.r16s(scene_of(m) + 908); }
void scene_or(Memory& m, u32 flags) { m.w32(scene_of(m), m.r32(scene_of(m)) | flags); }
}

void Game::restart_setup() { restart_setup_result(inherited_restart_heading); }

u32 Game::restart_setup_result(u32 inheritedHeading) {
    cover(0x8005B654u);
    Frame frame(*this, 64);
    const u32 placement = frame.at(0x10);
    const s32 missionId = mission_of(m);
    const u32 variant = m.r32(world_of(m) + 8);
    m.w8(ms_of(m) + 40, m.r8(ms_of(m) + 37));
    u32 restartFlags = 0;
    bool holdControls = false;
    s32 phaseKind = -1;
    switch (missionId) {
    case 4:
        if (variant == 0) { m.w32(kPhaseKind, 0); restartFlags = 519; }
        else if (variant == 1) { m.w32(kPhaseKind, 1); restartFlags = 256; }
        else if (variant == 2) { m.w32(kPhaseKind, 2); restartFlags = 0x8000; }
        break;
    case 7:
        if (variant == 0) { restartFlags = 8193; holdControls = true; phaseKind = 3; }
        else if (variant == 1) { restartFlags = 0x8000; phaseKind = 4; }
        else if (variant == 2) { restartFlags = 257; phaseKind = 5; }
        break;
    case 9:
        if (variant == 0) { restartFlags = 1024; phaseKind = 6; }
        else if (variant == 1) { restartFlags = 1025; phaseKind = 7; }
        break;
    case 14: restartFlags = 257; phaseKind = 8; break;
    case 24: restartFlags = 1026; phaseKind = 9; break;
    case 25: restartFlags = 258; phaseKind = 10; break;
    case 29:
        if (variant == 0) { restartFlags = 520; phaseKind = 11; }
        else if (variant == 1) { restartFlags = 2051; phaseKind = 12; }
        else if (variant == 2) { restartFlags = 0x8000; phaseKind = 13; }
        break;
    case 31: restartFlags = 16393; holdControls = true; phaseKind = 14; break;
    case 33: restartFlags = 260; phaseKind = 15; break;
    case 34: restartFlags = 8195; holdControls = true; phaseKind = 16; break;
    case 41:
        if (variant == 0) { restartFlags = 8198; holdControls = true; phaseKind = 17; }
        else if (variant == 1) { restartFlags = 261; phaseKind = 18; }
        break;
    case 42: restartFlags = 4096; phaseKind = 19; break;
    case 44:
        if (variant == 0) { restartFlags = 2051; phaseKind = 20; }
        else if (variant == 1) { restartFlags = 523; phaseKind = 21; }
        else if (variant == 2) { restartFlags = 0x8000; phaseKind = 22; }
        break;
    case 50: restartFlags = 8195; holdControls = true; phaseKind = 23; break;
    default: break;
    }
    if (phaseKind >= 0) m.w32(kPhaseKind, static_cast<u32>(phaseKind));
    m.w32(0x800BD058u, restartFlags);
    prompt_reset();
    const u32 missionState = ms_of(m);
    if (restartFlags & 0x6000u) {
        m.w8(missionState + 28, 0xFF);
        m.w32(missionState + 20, 0);
    } else if (restartFlags & 0x8000u) {
        m.w8(missionState + 28, 0);
    } else {
        m.w8(missionState + 28, 1);
        m.w32(missionState + 20, 5400);
    }
    effects_reset();
    message_reset();
    if (!(restartFlags & 0x8000u)) player_start_mode(restartFlags);
    view_start(restartFlags);
    if (holdControls) player_mode_80076DBC();
    else if (restartFlags & 0x8000u) {
        player_mode_800762D0();
        m.w16(ms_of(m) + 8, 0);
    } else if (restartFlags & 0x1000u) player_mode_800762B0();
    else player_mode_800762D0();
    scene_or(m, 0x10000000u);
    alert_channels_init();
    radio_face(0, 0, 0);
    if (restartFlags & 0x300u) {
        const u32 startRecord = 0x800B3B94u + 56 * (restartFlags & 0xFFu);
        m.w32(placement, m.r32(startRecord));
        m.w32(placement + 4, m.r32(startRecord + 4) - 0x1000000u);
        m.w32(placement + 8, m.r32(startRecord + 8));
        m.w16(placement + 16, 0);
        m.w16(placement + 18, m.r16(startRecord + 18));
        m.w16(placement + 20, 0);
        const s32 startDistance = missionId == 33 ? 36864 : 24576;
        const s32 difficulty = m.r32s(scene_of(m) + 916);
        u32 heading = inheritedHeading;
        if (difficulty == 0) {
            heading = m.r16(startRecord + 18) + 0x2000u;
        } else if (difficulty == 1) {
            heading = m.r16(startRecord + 18) + static_cast<u32>(rand() & 0x3FF) + 7680u;
        } else if (difficulty >= 2) {
            heading = m.r16(startRecord + 18) + static_cast<u32>(rand() & 0xFFF) + 6144u;
            m.w16(placement + 18, heading - 0x2000u);
        }
        const u32 angleIndex = heading & 0x3FFFu;
        const s32 cosine = m.r16s(kSinCos + 0x2000 + 2 * angleIndex);
        m.w32(placement, static_cast<u32>(m.r32s(placement) + ((startDistance * m.r16s(kSinCos + 2 * angleIndex)) >> 2)));
        m.w32(placement + 8, static_cast<u32>(m.r32s(placement + 8) + ((startDistance * cosine) >> 2)));
    } else if (restartFlags & 0x1C00u) {
        m.w32(placement, 0);
        m.w32(placement + 4, static_cast<u32>(-40960000));
        m.w32(placement + 8, 0);
        m.w32(placement + 16, 0);
        m.w16(placement + 20, 0);
    } else {
        constexpr u32 kSaved = 0x800C79B0u;
        if (m.r8(0x800BD05Cu)) {
            for (u32 o = 0; o < 24; o += 4) m.w32(placement + o, m.r32(kSaved + o));
        } else {
            const u32 player = slot(0);
            m.w32(placement, m.r32(player + 8));
            m.w32(placement + 8, m.r32(player + 16));
            m.w32(placement + 12, m.r32(player + 20));
            m.w32(placement + 4, static_cast<u32>(-40960000));
            m.w16(placement + 16, 0);
            m.w16(placement + 18, m.r16(player + 26));
            m.w16(placement + 20, 0);
            for (u32 o = 0; o < 24; o += 4) m.w32(kSaved + o, m.r32(placement + o));
        }
        m.w8(0x800BD05Cu, m.r8(0x800BD05Cu) + 1u);
    }
    if (!(restartFlags & 0x6000u)) player_place(placement);
    wingmen_start();
    ground_start_units();
    target_hud_reset();
    return 0;
}
}
