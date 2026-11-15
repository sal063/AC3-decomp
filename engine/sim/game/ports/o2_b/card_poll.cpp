#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u;
constexpr u32 kEnabled = 40, kInfo = 44, kLoad = 48, kFile = 60, kNames = 692;
constexpr u32 kTimer = 792, kPresent = 840, kFormatted = 842;
constexpr u32 kStage = 844, kResult = 846, kRetries = 848, kPort = 852;
u32 pilots(Memory& m) { return m.r32(kPilots); }
void alternate(Memory& m) {
    const u32 state = pilots(m);
    m.w32(state + kPort, 1u - m.r32(state + kPort));
}
void note_retry(Memory& m, u32 port) {
    const u32 state = pilots(m) + port;
    if (m.r8(state + kRetries) >= 17) m.w8(state + kPresent, 1);
    else {
        m.w8(state + kPresent, 0);
        const u32 current = pilots(m) + port;
        m.w8(current + kRetries, m.r8(current + kRetries) + 1);
    }
}
}

void Game::o2_card_poll() {
    cover(0x800E4858u);
    Frame frame(*this, 32);
    const u32 port = m.r32(pilots(m) + kPort);
    switch (m.r8(pilots(m) + port + kStage)) {
    case 0: alternate(m); break;
    case 1:
        o2_card_events_software(); hw->card_info(port << 4);
        m.w8(pilots(m) + port + kStage, 2);
        m.w32(pilots(m) + kTimer, 300);
        break;
    case 2: {
        const u32 result = o2_card_event_software(port, 3);
        m.w8(pilots(m) + port + kResult, result);
        const u32 state = pilots(m), timer = m.r32(state + kTimer);
        if (static_cast<s32>(timer) < 0) m.w8(state + port + kStage, 1);
        else m.w32(state + kTimer, timer - 1);
        break;
    }
    case 3: {
        const u32 state = pilots(m), result = m.r8(state + port + kResult);
        m.w16(state + kInfo + 2 * port, result);
        if (result == 0) {
            if (m.r8(state + port + kPresent) == 1) {
                m.w8(state + port + kStage, m.r32(state + kEnabled) != 0);
                alternate(m);
            } else {
                o2_card_events_hardware(); hw->card_clear(port << 4);
                m.w8(pilots(m) + port + kStage, 4);
            }
        } else if (result == 1 || result == 2) {
            note_retry(m, port);
            const u32 enabled = m.r32(pilots(m) + kEnabled);
            m.w8(pilots(m) + port + kStage, result == 1 ? enabled == 1 : enabled != 0);
            alternate(m);
            if (result == 2) m.w8(pilots(m) + port + kFormatted, 0);
        } else if (result == 3) {
            m.w16(state + kInfo + 2 * port, 0);
            o2_card_events_hardware(); hw->card_clear(port << 4);
            m.w8(pilots(m) + port + kStage, 4);
            m.w8(pilots(m) + port + kPresent, 0);
            m.w8(pilots(m) + port + kRetries, 0);
        } else {
            m.w8(pilots(m) + port + kPresent, 1);
            m.w8(pilots(m) + port + kStage, 1);
            alternate(m);
        }
        break;
    }
    case 4:
        do {
            const u32 result = o2_card_event_hardware(port, 5);
            m.w8(pilots(m) + port + kResult, result);
        } while (m.r8(pilots(m) + port + kResult) == 255);
        break;
    case 5:
        m.w16(pilots(m) + kInfo + 2 * port, m.r8(pilots(m) + port + kResult));
        o2_card_events_software(); hw->card_load(port << 4);
        m.w8(pilots(m) + port + kStage, 6);
        break;
    case 6: {
        const u32 result = o2_card_event_software(port, 7);
        m.w8(pilots(m) + port + kResult, result);
        break;
    }
    case 7: {
        const u32 state = pilots(m), result = m.r8(state + port + kResult);
        m.w16(state + kLoad + 2 * port, result);
        m.w8(state + port + kPresent, 1);
        if (!result) m.w8(pilots(m) + port + kStage, 8);
        else if (result == 1 || result == 2) {
            note_retry(m, port);
            m.w8(pilots(m) + port + kStage, 1);
        } else if (result == 3) {
            m.w8(pilots(m) + port + kPresent, 1);
            m.w8(pilots(m) + port + kStage, 1);
            alternate(m);
        }
        break;
    }
    case 8: {
        const u32 handle = hw->file_open(pilots(m) + kNames + 26 * port, 0x8001);
        m.w32(pilots(m) + kFile, handle);
        if (handle != 0xFFFFFFFFu) m.w8(pilots(m) + port + kFormatted, 1);
        hw->file_close(m.r32(pilots(m) + kFile));
        m.w8(pilots(m) + port + kStage, 1);
        m.w8(pilots(m) + port + kPresent, 1);
        alternate(m);
        break;
    }
    default: break;
    }
}
}
