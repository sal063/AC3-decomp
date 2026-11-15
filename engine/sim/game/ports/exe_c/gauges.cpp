#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kGaugeClock = 0x800C6D60u;
constexpr u32 kGaugeClockMode = 0x800C6D64u;
constexpr u32 kGaugeColors = 0x800C6D68u;
constexpr u32 kGaugeBuffers = 0x800BD060u;
constexpr u32 kAltitudeOrigin = 0x800BCEB0u;
}

void Game::gauges_args(u32 hud) {
    cover(0x8005E31Cu);
    Frame frame(*this, 0x48);
    const u32 instruments = frame.at(0x10);
    m.w32(instruments, 0x7F);
    m.w32(instruments + 4, m.r32(m.r32(hud + 4) + 4)
        + m.r32(m.r32(kAltitudeOrigin) + 4) + 270336u);
    m.w32(instruments + 8, m.r32(hud + 20));
    m.w32(instruments + 12, m.r32(hud + 24));
    m.w32(instruments + 16, m.r32(hud + 28));
    m.w32(instruments + 20, m.r32(hud + 36));
    const u32 dimmedBars = m.r32(hud + 40);
    const u32 clockMode = m.r16(kGaugeClockMode), clock = m.r32(kGaugeClock);
    const u32 digitMode = m.r8(kGaugeColors + 9);
    const u32 secondaryRed = m.r8(kGaugeColors), secondaryGreen = m.r8(kGaugeColors + 1);
    const u32 secondaryBlue = m.r8(kGaugeColors + 2);
    const u32 red = m.r8(kGaugeColors + 3), green = m.r8(kGaugeColors + 4);
    const u32 blue = m.r8(kGaugeColors + 5);
    m.w16(instruments + 38, clockMode);
    m.w32(instruments + 40, clock);
    m.w16(instruments + 36, digitMode);
    m.w8(instruments + 28, secondaryRed);
    m.w8(instruments + 29, secondaryGreen);
    m.w8(instruments + 30, secondaryBlue);
    m.w8(instruments + 32, red);
    m.w8(instruments + 33, green);
    m.w32(instruments + 24, dimmedBars);
    m.w8(instruments + 34, blue);
    gauges_draw(instruments, m.r32(m.r32(g::kWorld) + 16),
        m.r32(m.r32(kGaugeBuffers) + 4 * m.r32(m.r32(g::kScene) + 4)));
}
}
