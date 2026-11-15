#include "sim/game/game.hpp"

namespace ac3::sim {
u32 Game::o2_selector_packets_init() {
    cover(0x800F8AC4u);
    Frame frame(*this, 72);
    constexpr u32 kSelector = 0x80112E10u, kSpriteBytes = 20, kBankBytes = 40;
    for (u32 bank = 0; bank < 2; ++bank) {
        for (u32 slot = 0; slot < 2; ++slot) {
            for (u32 side = 0; side < 2; ++side) {
                const u32 sprite = m.r32(kSelector) + (side ? 0x68 : 0x18)
                                   + kBankBytes * bank + kSpriteBytes * slot;
                m.w8(sprite + 3, 4);
                m.w8(sprite + 7, 0x66);
                for (u32 channel = 0; channel < 3; ++channel) m.w8(sprite + 4 + channel, 128);
                if (!side) m.w16(sprite + 14, 0xA18);
                m.w16(sprite + 16, 48);
                m.w16(sprite + 18, 88);
                if (side) m.w16(sprite + 14, 0xA58);
            }
            m.w32(frame.at(16), 0);
            m.w32(frame.at(24), 0x66);
            m.w32(frame.at(28), 4);
            set_draw_mode(m.r32(kSelector) + 0xB8 + 12 * bank, 0, 0, 0x4C, 0);
        }
    }
    return 0;
}
}
