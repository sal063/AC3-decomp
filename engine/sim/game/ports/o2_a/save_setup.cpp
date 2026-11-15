#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kDialog = 0x80112DDCu, kSave = 0x80112DD8u;
constexpr u32 kSummary = 0x80112DBCu;
}

void Game::o2_save_grid_setup() {
    cover(0x800DCE0Cu);
    Frame frame(*this, 64);
    const u32 allocation = mem_alloc(0x23BC);
    m.w32(kSave, allocation); m.w8(allocation + 0x23B8, 2);
    const u32 modeTarget = m.r32(kSave);
    m.w16(allocation + 0x23B6, 0); m.w8(modeTarget + 0x23B9, 0);
    o2_voice_script(0x8010A5ECu);
    auto sprite = [&](u32 packet, bool shadow, u32 row, u32 column) {
        m.w8(packet + 3, 4); m.w8(packet + 7, 0x66);
        m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
        m.w16(packet + 8, 156 + 8 * column); m.w16(packet + 10, 350 - 16 * row);
        m.w16(packet + 16, 8); m.w16(packet + 18, 16);
        m.w8(packet + 12, shadow ? 236 : row < 3 ? 220 : 228);
        m.w8(packet + 13, shadow ? 24 : row < 3 ? 36 : 20);
        m.w16(packet + 14, shadow ? 0xE9B : row < 3 ? 0xEDB : 0xF1B);
    };
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 save = m.r32(kSave);
        for (u32 column = 0; column < 18; ++column) for (u32 row = 0; row < 6; ++row) {
            const u32 offset = 20 * (6 * column + row);
            sprite(save + 0x870 * buffer + offset, false, row, column);
            sprite(save + 0x10E0 + 0x870 * buffer + offset, true, row, column);
        }
        m.w32(frame.at(16), 0); set_draw_mode(m.r32(kSave) + 0x21C0 + 12 * buffer, 0, 0, 0x1D, 0);
        m.w32(frame.at(16), 0); set_draw_mode(m.r32(kSave) + 0x21D8 + 12 * buffer, 0, 0, 0x5D, 0);
    }
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 save = m.r32(kSave);
        for (u32 i = 0; i < 5; ++i) {
            const u32 packet = save + 0x21F0 + 100 * buffer + 20 * i;
            m.w8(packet + 3, 4); m.w8(packet + 7, 0x66);
            m.w8(packet + 4, 128); m.w8(packet + 5, 128); m.w8(packet + 6, 128);
            m.w16(packet + 16, 76); m.w16(packet + 18, 84);
            m.w8(packet + 12, 0); m.w8(packet + 13, 0); m.w16(packet + 14, 0xD5B);
        }
        m.w32(frame.at(16), 0); set_draw_mode(m.r32(kSave) + 0x2380 + 12 * buffer, 0, 0, 0x5D, 0);
    }
    const u32 save = m.r32(kSave);
    m.w16(save + 0x23AC, 32);
    for (u32 field : {0x23A2u, 0x2398u, 0x23A4u, 0x239Au}) m.w16(save + field, 4);
    m.w16(save + 0x23AE, -48); m.w16(save + 0x23B0, 56); m.w16(save + 0x23B2, -24);
    m.w16(save + 0x23AA, 8); m.w16(save + 0x23A0, 8);
    for (u32 field : {0x23A6u, 0x239Cu, 0x23A8u, 0x239Eu}) m.w16(save + field, 12);
    m.w16(save + 0x23B4, 64);
    o2_entry_reset(2, 19); o2_group_set(2, 0x80000);
    o2_entry_reset(0, 14); o2_group_set(0, 0x4001);
    o2_upload_pack(m.r32(m.r32(kMenu) + 168));
    for (u32 i = 0; i < 5; ++i) voice_release(m.r32(m.r32(kMenu) + 304 + 4 * i));
    const u32 dialog = m.r32(kDialog); m.w8(dialog + 1, m.r8(dialog + 1) + 1);
}

void Game::o2_campaign_summary_setup() {
    cover(0x800DC144u);
    Frame frame(*this, 48);
    const u32 archive = m.r32(m.r32(0x800BCE50u) + 96), stats = m.r32(g::kScene) + 2396;
    m.w32(m.r32(kMenu) + 40, archive);
    const u32 entries = ld_relocate_dir(archive);
    o2_upload_pack(m.r32(entries + 4));
    o2_upload_pack(m.r32(m.r32(m.r32(kMenu) + 24) + 24));
    heap_set(m.r32(entries + 4));
    m.w32(m.r32(kMenu) + 476, ld_relocate_dir(m.r32(entries)));
    m.w32(kSummary, mem_alloc(64)); m.w8(m.r32(kSummary) + 60, 0);
    for (u32 id = 0; id < 59; ++id) if (m.r8(m.r32(g::kScene) + 3340 + id)) {
        u32 summary = m.r32(kSummary); m.w8(summary + m.r8(summary + 60), id);
        summary = m.r32(kSummary); m.w8(summary + 60, m.r8(summary + 60) + 1);
    }
    u32 summary = m.r32(kSummary);
    m.w8(summary + m.r8(summary + 60), m.r8(m.r32(g::kScene) + 908));
    summary = m.r32(kSummary); m.w8(summary + 60, m.r8(summary + 60) + 1);
    m.w8(m.r32(kSummary) + 61, 0);
    for (u32 i = 0; i < 64; ++i) if (m.r8(stats + 24 + i) == 1) {
        summary = m.r32(kSummary); m.w8(summary + 61, m.r8(summary + 61) + 1);
    }
    m.w8(m.r32(kSummary) + 62, 0);
    for (u32 i = 0; i < 16; ++i) if (m.r8(stats + 88 + i) == 1) {
        summary = m.r32(kSummary); m.w8(summary + 62, m.r8(summary + 62) + 1);
    }
    summary = m.r32(kSummary); m.w8(summary + 63, m.r8(summary + 60) - 1);
    summary = m.r32(kSummary);
    const u32 image = o2_brief_kind(m.r8(summary + m.r8(summary + 63)));
    const u32 tim = m.r32(m.r32(m.r32(kMenu) + 476) + 4 * image);
    m.w32(frame.at(16), 1); o2_upload_tim_positioned(tim, 960, 256, 768, 1);
    const u32 dialog = m.r32(kDialog); m.w8(dialog + 1, m.r8(dialog + 1) + 1);
    o2_entry_reset(2, 17); o2_entry_reset(2, 18); o2_group_set(2, 0x60000);
    summary = m.r32(kSummary);
    const u32 id = m.r8(summary + m.r8(summary + 63));
    o2_flag(50, m.r8(m.r32(g::kScene) + 3340 + id));
    o2_flag(51, m.r8(m.r32(g::kScene) + 916));
    o2_flag(53, m.r8(m.r32(g::kScene) + 2397));
}
}
