#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::recorder_attach(u32 allocation) {
    cover(0x8007DBC4u);
    constexpr u32 kRecorder = 0x800BD0B8u;
    constexpr u32 kCapacity = 8272;
    constexpr u32 kFoldClock = 4120;
    constexpr u32 kSubject = 6220;
    const u32 recorder = m.r32(allocation);
    const u32 buffer = m.r32(allocation + 4);
    const s32 writeOffset = m.r32s(recorder + 16);
    const u32 capacity = m.r32(recorder + kCapacity) & ~3u;
    m.w32(recorder + 4, buffer);
    const u32 base = m.r32(recorder + 4);
    m.w32(kRecorder, recorder);
    m.w32(recorder + kFoldClock, 0);
    m.w32(recorder + kSubject, 0xFFFFFFFFu);
    m.w32(recorder + 8, buffer + capacity - 64);
    if (writeOffset >= 0) m.w32(recorder + 16, base + static_cast<u32>(writeOffset));
    const s32 readOffset = m.r32s(m.r32(kRecorder) + 12);
    const u32 read = base + static_cast<u32>(readOffset);
    if (readOffset >= 0) m.w32(m.r32(kRecorder) + 12, read);
    return read;
}
}
