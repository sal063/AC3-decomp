#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kStreamPlayer = 0x800BD054u;
constexpr u32 kVideoWork = 0x800BD050u;
constexpr u32 kVideoOptions = 0x800BCDA0u;
constexpr u32 kSavedImages = 0x800BCDB4u;
constexpr u32 kStream = 204, kRingBytes = 208, kRing = 212;
constexpr u32 kDecoderMode = 161;
constexpr u32 kRecordGroups = 2, kRecordsPerGroup = 5, kRecordStride = 14;
}

u32 Game::stream_player_reset(u32 configuration) {
    cover(0x800586ACu);
    const u32 player = m.r32(kStreamPlayer);
    m.w32(player + kStream, m.r32(configuration + 4));
    m.w32(player + kRingBytes, m.r32(configuration + 8));
    m.w32(player + kRing, m.r32(configuration + 12));
    m.w8(player + kDecoderMode, m.r8(configuration));
    m.w8(player + 158, 0); m.w8(player + 159, 0); m.w8(player + 160, 0);
    m.w8(player + 10, 0); m.w8(player + 1, 0);
    m.w32(player + 12, 0); m.w32(player + 4, 0);
    m.w8(player, 0); m.w8(player + 16, 0); m.w16(player + 8, 0);
    for (u32 group = 0; group < kRecordGroups; ++group) {
        for (u32 index = 0; index < kRecordsPerGroup; ++index) {
            const u32 record = player + 18 + kRecordStride * (group * kRecordsPerGroup + index);
            m.w8(record, 0);
            m.w16(record + 2, 0); m.w16(record + 4, 0);
            for (u32 sector = 0; sector < 4; ++sector) m.w16(record + 6 + 2 * sector, 0xFFFF);
        }
    }
    for (u32 index = 0; index < 40; ++index) m.w8(player + 162 + index, 0);
    return 0;
}

u32 Game::stream_player_setup(u32 stream, u32 mode, u32 player, u32 bytes,
    u32 x0, u32 y0, u32 w0, u32 h0, u32 x1, u32 y1, u32 w1, u32 h1) {
    cover(0x8005860Cu);
    Frame frame(*this, 0x38);
    m.w32(kStreamPlayer, player);
    const u32 ring = player + 216, alignedBytes = (bytes + 3u) & ~3u;
    const u32 configuration = frame.at(0x10);
    m.w32(configuration, mode); m.w32(configuration + 4, stream);
    m.w32(configuration + 8, alignedBytes); m.w32(configuration + 12, ring);
    m.w16(frame.at(0x20), x0); m.w16(frame.at(0x22), y0);
    m.w16(frame.at(0x24), w0); m.w16(frame.at(0x26), h0);
    m.w16(frame.at(0x28), x1); m.w16(frame.at(0x2A), y1);
    m.w16(frame.at(0x2C), w1); m.w16(frame.at(0x2E), h1);
    stream_player_reset(configuration);
    cd_reset(0, 0);
    return ring + alignedBytes;
}

u32 Game::video_restore_image() {
    cover(0x8005B5BCu);
    Frame frame(*this, 0x20);
    cd_reset(0, 0);
    const u32 rectangle = frame.at(0x10);
    m.w16(rectangle, 0); m.w16(rectangle + 2, 0);
    m.w16(rectangle + 4, 320); m.w16(rectangle + 6, 480);
    hw->clear_image(rectangle, 0, 0, 0);
    hw->load_image(kSavedImages + 16, m.r32(m.r32(kVideoWork) + 88));
    if (m.r32(m.r32(kVideoOptions) + 4)) radio_face(255, 0, 0);
    return 1;
}

u32 Game::video_restore_mission() {
    cover(0x8005B234u);
    Frame frame(*this, 0x20);
    cd_reset(0, 0);
    heap_set(m.r32(m.r32(g::kWorld) + 696));
    display_buffers_alloc_recorded(m.r32(m.r32(g::kWorld) + 696));
    const u32 rectangle = frame.at(0x10);
    m.w16(rectangle, 0); m.w16(rectangle + 2, 0);
    m.w16(rectangle + 4, 320); m.w16(rectangle + 6, 480);
    hw->clear_image(rectangle, 0, 0, 0);
    hw->load_image(kSavedImages, m.r32(m.r32(kVideoWork) + 88));
    hw->load_image(kSavedImages + 8, m.r32(m.r32(kVideoWork) + 92));
    if (m.r32(m.r32(kVideoOptions) + 4)) radio_face(255, 0, 0);
    mission_end();
    return 0;
}
}
