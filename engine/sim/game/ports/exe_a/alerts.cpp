#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kSoundQueue = 0x800C6570u;
constexpr u32 kQueueHead = 0x800BCE98u;
}

void Game::hud_alert_k3() {
    cover(0x800295CCu);
    Frame f(*this, 24);
    hud_alert(20, 3, 230);
}

void Game::hud_alert_k15() {
    cover(0x80029604u);
    Frame f(*this, 24);
    hud_alert(30, 15, 250);
}

void Game::hud_alert_k60() {
    cover(0x8002963Cu);
    Frame f(*this, 24);
    hud_alert(250, 60, 255);
}

void Game::hud_alert_burning() {
    cover(0x80029674u);
    Frame f(*this, 24);
    const u32 p = hud_alert_take(160);
    if (!p) return;
    m.w8(p, 1);
    m.w8(p + 2, static_cast<u32>((rand() & 0x3F) + 65));
}

void Game::hud_alert_k9() {
    cover(0x800296B8u);
    Frame f(*this, 24);
    hud_alert(180, 9, 255);
}

void Game::snd_delayed_reset() {
    cover(0x8002F140u);
    Frame f(*this, 24);
    for (u32 k = 0; k < 8; ++k) snd_delayed_clear(k);
}

void Game::sound_queue_reset() {
    cover(0x8002F254u);
    m.w8(kQueueHead, 0);
    m.w8(kQueueHead + 1, 0);
    for (u32 k = 0; k < 32; ++k) {
        const u32 p = kSoundQueue + 4 * k;
        m.w8(p, 255);
        m.w8(p + 1, 255);
        m.w16(p + 2, 0xFFFF);
    }
}
}
