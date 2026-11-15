#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kHangar = 0x80112DA8u;
constexpr u32 kSelection = 0x80112E30u, kFrontState = 0x80112DDCu;
constexpr u32 kDisplay = 0x800BCE48u, kRecords = 0x800BCE50u;
constexpr u32 kFadeTicks = 0x80112BF4u, kModelTable = 0x80112C08u;
constexpr u32 kGpuAddress = 0x00FFFFFFu, kGpuTag = 0xFF000000u;
}

u32 Game::o2_hangar_ot_link(u32 ot) {
    cover(0x800FF100u);
    Frame frame(*this, 32);
    const u32 hangar = m.r32(kHangar);
    const u32 table = m.r32(hangar + 4 * m.r32(m.r32(g::kScene) + 4));
    m.w32(hangar + 8, table);
    clear_otag_r(table, 256);
    const u32 active = m.r32(m.r32(kHangar) + 8);
    m.w32(active, (m.r32(active) & kGpuTag) | (m.r32(ot) & kGpuAddress));
    const u32 tail = (active + 255 * 4) & kGpuAddress;
    m.w32(ot, (m.r32(ot) & kGpuTag) | tail);
    return tail;
}

u32 Game::o2_hangar_depart() {
    cover(0x800FF2F0u);
    Frame frame(*this, 24);
    const u32 hangar = m.r32(kHangar);
    m.w32(hangar + 0x14, 3);
    m.w32(hangar + 0x20, 0);
    sound_play(99);
    u32 result = 0;
    for (u32 voice = 0; voice < 5; ++voice)
        result = voice_release(m.r32(m.r32(kMenu) + 0x130 + 4 * voice));
    return result;
}

u32 Game::o2_front_fade_wait() {
    cover(0x80107200u);
    if (m.r32(m.r32(kHangar) + 0x14)) return 0x80110000u;
    const u32 state = m.r32(kFrontState);
    m.w8(state + 1, m.r8(state + 1) + 1);
    const u32 ticks = m.r8(m.r32(kDisplay) + 0x10) < 143 ? 1u : 32u;
    m.w32(kFadeTicks, ticks);
    return ticks;
}

u32 Game::o2_front_fade_tick() {
    cover(0x80107270u);
    const u32 remaining = m.r32(kFadeTicks) - 1;
    m.w32(kFadeTicks, remaining);
    if (remaining == 0) {
        const u32 state = m.r32(kFrontState);
        m.w8(state + 1, m.r8(state + 1) + 1);
    }
    constexpr s32 channels[] = {143, 135, 83};
    for (u32 channel = 0; channel < 3; ++channel) {
        const s32 ticks = m.r32s(kFadeTicks);
        m.w8(m.r32(kDisplay) + 0x10 + channel, wrap::mul(ticks, channels[channel]) / 32);
    }
    const u32 display = m.r32(kDisplay), flags = m.r32(display) | 8u;
    m.w32(display, flags);
    return flags;
}

u32 Game::o2_selector_model_request() {
    cover(0x80108298u);
    Frame frame(*this, 32);
    const u32 entry = kModelTable + 8 * m.r8(m.r32(kSelection) + 0x4000);
    const u32 record = m.r32(kRecords) + 0x60;
    m.w32(record, static_cast<u32>(m.r16s(entry)));
    const u32 destination = mem_alloc(records_size(record, 1));
    m.w32(m.r32(g::kScene) + 0x44, 1);
    request_records(m.r32(kRecords) + 0x60, 1, destination, 0x8009B904u);
    const u32 selection = m.r32(kSelection);
    m.w8(selection + 0x4001, 1);
    return selection;
}
}
