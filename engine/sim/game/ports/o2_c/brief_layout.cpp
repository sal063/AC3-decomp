#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBrief = 0x80112DB0u, kMenu = 0x80112DC4u, kPageRecords = 0x8010B400u;
constexpr u32 kPictures = 0x1654, kTextures = 0x1674, kPictureCount = 0x1650;
constexpr u32 kVoiceAvailable = 0x1698, kHasPicture = 0xF38;
void picture(Memory& m, u32 brief, u32 index, u32 x, u32 y, u32 w, u32 h,
             u32 u, u32 v, u32 textureW, u32 textureH) {
    const u32 bounds = brief + kPictures + 8 * index, uv = brief + kTextures + 8 * index;
    m.w16(bounds, x); m.w16(bounds + 2, y); m.w16(bounds + 4, w); m.w16(bounds + 6, h);
    m.w16(uv, u); m.w16(uv + 2, v); m.w16(uv + 4, textureW); m.w16(uv + 6, textureH);
}
u32 pageRecord(Memory& m, u32 brief) {
    return kPageRecords + 8 * static_cast<u32>(m.r16s(m.r32(brief + 0x189C)));
}
}

u32 Game::o2_brief_layout_init(u32 pageKind) {
    cover(0x800F0244u);
    Frame frame(*this, 32);
    const u32 menu = m.r32(kMenu), brief = m.r32(kBrief);
    m.w32(menu + 0x114, 0);
    m.w32(brief + kHasPicture, 0);
    m.w32(menu + 0x114, 0);
    m.w32(brief + kVoiceAvailable, 0);
    o2_unlock_by_id(m.r16s(m.r32(brief + 0x189C)));
    const s32 kind = static_cast<s16>(pageKind);
    auto voicePicture = [&] {
        m.w32(brief + kHasPicture, 1);
        m.w32(brief + kVoiceAvailable, 1);
        o2_brief_image_open(static_cast<u32>(kind));
    };
    switch (kind) {
    case 0: case 1: case 2:
        m.w32(brief + kHasPicture, 1); m.w32(brief + kVoiceAvailable, 1);
        m.w32(brief + kPictureCount, kind == 2 ? 2 : 1);
        picture(m, brief, 0, kind == 2 ? 70 : 120, 93, 80, 160, 0, 0, 80, 80);
        if (kind == 2) picture(m, brief, 1, 170, 93, 80, 160, 80, 0, 80, 80);
        o2_brief_image_open(static_cast<u32>(kind));
        return sound_play(kind == 0 ? 106 : 103);
    case 3: {
        m.w32(brief + kPictureCount, 2);
        picture(m, brief, 0, 74, 99, 48, 96, 0, 0, 48, 48);
        picture(m, brief, 1, 150, 99, 96, 128, 48, 0, 96, 64);
        const u32 mode = m.r8(pageRecord(m, brief) + 7);
        if (mode == 2) voicePicture();
        else if (mode == 1) m.w32(m.r32(kMenu) + 0x114, 0x4000);
        const u32 currentMenu = m.r32(kMenu);
        radio_face(5, m.r32(currentMenu + 0xB0), m.r32(currentMenu + 0xB4));
        return 0;
    }
    case 4:
        m.w32(brief + kPictureCount, 1);
        picture(m, brief, 0, 96, 70, 128, 192, 0, 0, 128, 96);
        if (m.r8(pageRecord(m, brief) + 7) == 2) { voicePicture(); return 0; }
        m.w32(m.r32(kMenu) + 0x114, 0x4000);
        return 0x4000;
    case 5:
        m.w32(brief + kPictureCount, 3);
        picture(m, brief, 0, 52, 93, 48, 96, 0, 0, 48, 48);
        picture(m, brief, 1, 221, 93, 48, 96, 112, 0, 48, 48);
        picture(m, brief, 2, 128, 115, 64, 128, 48, 0, 64, 64);
        voicePicture(); return 0;
    case 6:
        m.w32(brief + kPictureCount, 1);
        picture(m, brief, 0, 120, 87, 80, 160, 0, 0, 80, 80);
        voicePicture(); return 0;
    case 7: case 11:
        m.w32(brief + kVoiceAvailable, 1); return 1;
    case 8: return 0x800F0718u;
    case 9:
        m.w32(brief + kPictureCount, 1);
        picture(m, brief, 0, 96, 100, 128, 192, 0, 0, 128, 96);
        m.w32(m.r32(kMenu) + 0x114, 0x4000); return 0x4000;
    case 10: {
        m.w32(m.r32(kMenu) + 0x11C, 1);
        u32 scene = m.r32(g::kScene);
        m.w8(scene + 0x95E, m.r8(scene + 0x95E) + 1u);
        m.w16(m.r32(g::kScene) + 0x954, 2); return 2;
    }
    default: return 0x800D0000u;
    }
}

u32 Game::o2_brief_page_start() {
    cover(0x800F1B8Cu);
    Frame frame(*this, 32);
    o2_brief_lines();
    o2_brief_layout_init(m.r8(pageRecord(m, m.r32(kBrief)) + 6));
    const u32 brief = m.r32(kBrief), menu = m.r32(kMenu);
    m.w32(menu + 0x12C, static_cast<u32>(m.r16s(pageRecord(m, brief) + 2)));
    m.w32(brief + 0x10, 0);
    radio_face(m.r8(pageRecord(m, brief) + 4), m.r32(menu + 0xB0), m.r32(menu + 0xB4));
    if (o2_brief_voice_request()) {
        m.w16(m.r32(kMenu) + 0x86, 10);
        m.w32(m.r32(kBrief) + 0xF44, 180);
    } else m.w16(m.r32(kMenu) + 0x86, 11);
    m.w32(m.r32(kMenu) + 0x128, 0);
    return 1;
}
}
