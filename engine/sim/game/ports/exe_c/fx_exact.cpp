#include "sim/game/game.hpp"
#include "sim/precise.hpp"

#include <cmath>

namespace ac3::sim {
void Game::fx_begin(u32 word, const Gte::Exact& centre, double ratio) {
    fx_on_ = gte.precise && player_settings && centre.ok && std::isfinite(ratio) && ratio > 0.0 && ratio < 64.0;
    fx_word_ = word;
    fx_centre_ = centre;
    fx_ratio_ = ratio;
}

void Game::fx_record(u32 from, u32 to) {
    if (!fx_on_) return;
    const double cx = static_cast<s16>(fx_word_ & 0xFFFFu), cy = static_cast<s16>(fx_word_ >> 16);
    for (u32 packet = from, guard = 0; packet < to && guard < 4096; ++guard) {
        const u32 words = m.r8(packet + 3);
        if (!words) break;
        for (u32 at = packet + 4, end = packet + 4 + 4 * words; at < end;) {
            const u32 code = m.r8(at + 3);
            if (code >= 0xE0u) {
                at += 4;
                continue;
            }
            if (code < 0x20u || code >= 0x40u) break;
            const bool gouraud = code & 0x10u, quad = code & 0x08u, textured = code & 0x04u;
            const u32 n = quad ? 4u : 3u;
            PreciseCache::Packet kept;
            u32 v = at + 4;
            for (u32 i = 0; i < n; ++i) {
                if (gouraud && i) v += 4;
                const u32 xy = m.r32(v);
                const double x = static_cast<s16>(xy & 0xFFFFu), y = static_cast<s16>(xy >> 16);
                kept.v[i] = PreciseVertex{static_cast<float>(fx_centre_.sx + (x - cx) * fx_ratio_),
                                          static_cast<float>(fx_centre_.sy + (y - cy) * fx_ratio_), fx_centre_.vx,
                                          fx_centre_.vy, fx_centre_.vz};
                v += textured ? 8 : 4;
            }
            kept.ok = static_cast<u8>((1u << n) - 1u);
            kept.fx = 1;
            gte.precise->record_packet(packet, kept);
            break;
        }
        packet += 4 * (words + 1);
    }
}
}
