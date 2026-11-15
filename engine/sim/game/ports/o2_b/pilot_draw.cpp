#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kPilots = 0x80112DF0u, kScreens = 0x80112DC0u, kMenu = 0x80112DC4u;
constexpr u32 kTimeText = 0x80112DE0u;
}

u32 Game::o2_pilot_time_text(u32 index) {
    cover(0x800E43B4u);
    Frame frame(*this, 32);
    const u32 record = m.r32(m.r32(kPilots) + 764 + 4 * (index & 255));
    s32 ticks = m.r32s(record + 276);
    if (ticks > 107999999) ticks = 107999999;
    if (ticks < 0) ticks = 0;
    const u32 hours = static_cast<u32>(ticks / 108000);
    const u32 minutes = static_cast<u32>((ticks / 1800) % 60);
    const u32 seconds = static_cast<u32>((ticks / 30) % 60);
    m.w32(frame.at(16), seconds);
    libc_sprintf(kTimeText, 0x800CCDDCu, {hours, minutes, seconds});
    for (u32 character = 0; character < 12; ++character)
        if (m.r8(kTimeText + character) == ' ') m.w8(kTimeText + character, '0');
    return kTimeText;
}

u32 Game::o2_pilot_record_load(u32 index) {
    cover(0x800E36E0u);
    const u32 scene = m.r32(g::kScene), selection = scene + 2396;
    const u32 record = m.r32(m.r32(kPilots) + 764 + 4 * (index & 255));
    m.w8(selection + 1, index);
    m.w32(selection + 1744, m.r32(record + 276));
    m.w16(m.r32(g::kScene) + 908, m.r8(record + 275));
    m.w32(m.r32(g::kScene) + 916, m.r8(record + 274));
    for (u32 mission = 0; mission < 60; ++mission) {
        m.w16(selection + 584 + 2 * mission, m.r16(record + 4 + 2 * mission));
        m.w8(selection + 944 + mission, m.r8(record + 124 + mission));
        m.w32(selection + 1264 + 4 * mission, m.r8(record + 154 + mission));
        m.w32(selection + 1504 + 4 * mission, m.r8(record + 214 + mission));
        const u32 currentScene = m.r32(g::kScene);
        m.w16(currentScene + 910, m.r8(record + 280));
        m.w8(currentScene + 937, m.r8(record + 281));
        m.w8(m.r32(g::kScene) + 938, m.r8(record + 282));
        m.w8(m.r32(g::kScene) + 939, m.r8(record + 283));
    }
    for (u32 mission = 0; mission < 30; ++mission) {
        m.w8(selection + 944 + 2 * mission, m.r8(record + 124 + mission) & 15);
        m.w8(selection + 945 + 2 * mission, m.r8(record + 124 + mission) >> 4);
    }
    return 0;
}

u32 Game::o2_screen_draw_mode(u32 mode) {
    cover(0x800E1118u);
    Frame frame(*this, 40);
    const u32 screens = m.r32(kScreens), packet = m.r32(screens + 4);
    const u32 ot = m.r32(m.r32(kMenu) + 8);
    m.w32(screens + 4, packet + 12); m.w32(frame.at(16), 0);
    set_draw_mode(packet, 0, 0, (mode & 3) << 5, 0);
    const u32 clip = m.r32(m.r32(kScreens) + 60);
    const u32 slot = ot + 4 * static_cast<u32>(m.r16s(clip + 14));
    ot_link(slot, packet);
    return m.r32(slot);
}

u32 Game::o2_marker_scan_draw() {
    cover(0x800E2124u);
    Frame frame(*this, 40);
    o2_screen_draw_mode(0);
    const u32 screens = m.r32(kScreens), definition = m.r32(m.r32(kMenu) + 364);
    const s32 scan = m.r32s(screens + 96) % 156;
    m.w32(frame.at(16), 1);
    o2_menu_tile(static_cast<u32>(static_cast<s16>(m.r16(definition + 16) + 14)),
                 static_cast<u32>(static_cast<s16>(m.r16(definition + 18) + 252 + scan)), 128, 4, 1);
    o2_screen_draw_mode(3);
    const u32 current = m.r32(kScreens);
    m.w32(current + 96, m.r32(current + 96) + 1);
    for (u32 point = 0; point < 20; ++point) {
        const s32 randomX = rand();
        const u32 entryX = m.r32(m.r32(kMenu) + 364);
        const u32 x = static_cast<u32>(static_cast<s16>(m.r16(entryX + 16) + 14 + randomX % 128));
        const s32 randomY = rand();
        const u32 entryY = m.r32(m.r32(kMenu) + 364);
        const u32 y = static_cast<u32>(static_cast<s16>(m.r16(entryY + 18) + 252 + randomY % 156));
        m.w32(frame.at(16), 1);
        o2_menu_tile(x, y, 1, 2, 1);
    }
    o2_screen_draw_mode(2);
    o2_marker_spread(); o2_marker_map_draw();
    return o2_marker_tick();
}
void Game::o2_menu_orbit_trails(u32 x, u32 y) {
    cover(0x800E5F88u);
    Frame frame(*this, 72);
    m.w16(frame.at(16), x); m.w16(frame.at(24), y);
    const u32 packets = m.r32(m.r32(kMenu) + 64);
    const u32 ot = m.r32(m.r32(g::kScene) + 8);
    constexpr u32 kTrailPackets = 592, kPacketBytes = 16, kTrailLength = 10;
    for (u32 trail = 0; trail < 2; ++trail) {
        const u32 head = packets + kTrailPackets + kPacketBytes * kTrailLength * trail;
        for (u32 age = kTrailLength - 1; age > 0; --age) {
            const u32 packet = head + kPacketBytes * age;
            m.w16(packet + 8, m.r16(packet - kPacketBytes + 8));
            m.w16(packet + 10, m.r16(packet - kPacketBytes + 10));
            ot_link(ot, packet);
        }
        const u32 angle = (m.r32(m.r32(kMenu) + 80) << 6) + trail * 1024;
        const s32 horizontal = rsin(static_cast<s32>(angle)) / 600;
        m.w16(head + 8, m.r16(frame.at(16)) + static_cast<u32>(horizontal));
        const s32 sine = rsin(static_cast<s32>((m.r32(m.r32(kMenu) + 80) << 6) + trail * 1024));
        const s32 cosine = rcos(static_cast<s32>((m.r32(m.r32(kMenu) + 80) << 6) + trail * 1024));
        const s32 vertical = (trail ? -sine : sine) / 300 + cosine / 600;
        m.w16(head + 10, m.r16(frame.at(24)) + static_cast<u32>(vertical));
        ot_link(ot, head);
    }
}
}
