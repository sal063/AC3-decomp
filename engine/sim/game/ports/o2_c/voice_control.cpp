#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kVoice = 0x80112E0Cu;
constexpr u32 kFlags = 0, kCurrent = 4, kRequested = 8, kScript = 0x1018;
}

u32 Game::o2_voice_control(u32 action, u32 index) {
    cover(0x800F7524u);
    Frame frame(*this, 40);
    const u32 pending = cd_pending(1);
    m.w32(m.r32(kVoice) + kRequested, index);
    switch (action) {
    case 1:
        if (!pending) {
            const u32 entry = m.r32(m.r32(kVoice) + kScript) + 12 * index;
            cd_request_stream(entry, 1, 0);
            const u32 voice = m.r32(kVoice);
            m.w32(voice + kCurrent, m.r32(voice + kRequested));
            m.w32(voice + kFlags, m.r32(voice + kFlags) | 1u);
        }
        break;
    case 2:
        cd_pause((m.r32(m.r32(kVoice) + kFlags) & 2u) ? 0 : 1);
        break;
    case 3:
        cd_cancel_streams();
        m.w32(m.r32(kVoice) + kFlags, m.r32(m.r32(kVoice) + kFlags) & ~3u);
        break;
    default: break;
    }
    const u32 voice = m.r32(kVoice);
    if (m.r32(voice + kCurrent) == m.r32(voice + kRequested)) {
        if (pending) return static_cast<s32>(pending) < 2 ? 1 : 0;
        if (!(m.r32(voice + kFlags) & 1u)) return 1;
    }
    if (static_cast<s32>(pending) >= 2) return 0x800C0000u;
    const u32 blocked = m.r32(m.r32(g::kScene) + 0x128) & 12u;
    if (blocked || action) return blocked;
    cd_cancel_streams();
    const u32 entry = m.r32(m.r32(kVoice) + kScript) + 12 * m.r32(m.r32(kVoice) + kRequested);
    cd_request_stream(entry, 1, 0);
    const u32 current = m.r32(kVoice);
    m.w32(current + kCurrent, m.r32(current + kRequested));
    const u32 flags = (m.r32(current + kFlags) | 1u) & ~2u;
    m.w32(current + kFlags, flags);
    return flags;
}
}
