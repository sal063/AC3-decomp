#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDemoRecords = 0x800B0BD0u;
constexpr u32 kLoadState = 0x800BCBE0u;
constexpr u32 kRecordsLoaded = 0x800357B4u;
constexpr u32 kStrm = 0x800BCBACu;
constexpr u32 kReq = 0x800BCBBCu;
}

u32 Game::stream_played() {
    cover(0x8002B59Cu);
    return m.r32(m.r32(kStrm) + 0x40);
}

u32 Game::stream_played_of(u32 id) {
    cover(0x8002CAE4u);
    Frame f(*this, 24);
    const u32 q = m.r32(kReq);
    if (m.r32(q + 0x174) != id) return 0xFFFFFFFFu;
    if (!(m.r32s(q) < m.r32s(q + 4))) return 0xFFFFFFFFu;
    if (m.r32(q + 0x148)) return 0xFFFFFFFFu;
    if ((kcd_stream_flags() & 0x81000000u) != 0x81000000u) return 0xFFFFFFFFu;
    return stream_played();
}

u32 Game::demo_request_replay() {
    cover(0x800361BCu);
    Frame f(*this, 24);
    const u32 list = m.r32(g::kExit) + 96;
    m.w32(list, static_cast<u32>(static_cast<s32>(m.r16s(kDemoRecords + 4 * m.r8(m.r32(g::kScene) + 934)))));
    const u32 at = mem_alloc(records_size(list, 1));
    m.w32(m.r32(kLoadState) + 8, 1);
    request_records(m.r32(g::kExit) + 96, 1, at, kRecordsLoaded);
    loading_frame();
    return 0x04000000u;
}

u32 Game::demo_start_replay() {
    cover(0x8003623Cu);
    Frame f(*this, 24);
    loading_frame();
    const u32 dir = ld_relocate_dir(m.r32(m.r32(g::kExit) + 96));
    recorder_attach(ld_relocate_dir(m.r32(dir)));
    const u32 w = m.r32(g::kWorld);
    m.w32(w + 4, 128);
    m.w32(w + 8, 0);
    return phase_change(0, 0);
}
}
