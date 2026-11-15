#include "core/wrap.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBrief = 0x80112DB0u;
constexpr u32 kMenu = 0x80112DC4u;
constexpr u32 kScreens = 0x80112DC0u;
constexpr u32 kPageRecords = 0x8010B400u;
constexpr u32 kMinorState = 0x86;
constexpr u32 kAnimationTick = 0x10;
constexpr u32 kTimeout = 0xF44;
constexpr u32 kVoiceAvailable = 0x1698;
constexpr u32 kCurrentPage = 0x189C;
u32 pageRecord(Memory& m, u32 brief) {
    return kPageRecords + 8 * static_cast<u32>(m.r16s(m.r32(brief + kCurrentPage)));
}
}

u32 Game::o2_brief_begin() {
    cover(0x800F0898u);
    Frame frame(*this, 32);
    const u32 allocated = mem_alloc(0x18A0);
    m.w16(allocated, 0); m.w16(allocated + 2, 1); m.w32(allocated + kAnimationTick, 0);
    m.w32(kBrief, allocated);
    u32 scene = m.r32(g::kScene);
    m.w32(scene + 0x96C, (m.r32(scene + 0x96C) | 0x40u) & ~0x80u);
    kcd_voice_reset();
    scene = m.r32(g::kScene);
    u32 brief = m.r32(kBrief);
    const u32 missionList = m.r32(m.r32(kMenu) + 0x120);
    const u32 pages = m.r32(missionList + 4 * m.r8(scene + 0x956) + 4);
    m.w32(brief + 8, pages);
    m.w32(brief + 0x18, pages + 4);
    m.w32(brief + 12, m.r32(pages));
    const u32 skipped = m.r8(scene + 0x95E);
    m.w32(brief + kCurrentPage, pages + 4 + 4 * skipped);
    m.w32(brief + 12, m.r32(brief + 12) - skipped);
    o2_page_request();
    m.w8(m.r32(kBrief) + 0xF2E, 0);
    brief = m.r32(kBrief);
    m.w32(brief + 0xF38, 0);
    constexpr u32 kPictureBank = 0x52C, kPictureStride = 0xA0;
    for (u32 count = 16; count; --count) m.w16(brief + kPictureBank + kPictureStride * (count - 1), 0);
    brief = m.r32(kBrief);
    constexpr u32 kLabelBank = 0x1450, kLabelStride = 0x20;
    for (u32 count = 16; count; --count) m.w16(brief + kLabelBank + kLabelStride * (count - 1), 0);
    if (m.r32(m.r32(kBrief) + 12)) {
        m.w16(m.r32(kMenu) + kMinorState, 1);
        o2_group_set(0, 0);
    } else {
        m.w16(m.r32(kMenu) + kMinorState, 3);
        o2_entry_enter(0, 15);
        o2_group_set(0, 0x8000);
    }
    brief = m.r32(kBrief);
    m.w32(brief + 0xF30, 0x8000);
    m.w32(brief + 0x28, o2_page_has_picture(m.r8(pageRecord(m, brief) + 6)));
    brief = m.r32(kBrief);
    m.w32(brief + 0x1C, o2_page_layout(m.r8(pageRecord(m, brief) + 6)));
    return sound_play(90);
}

u32 Game::o2_brief_page_tick() {
    cover(0x800F1D04u);
    Frame frame(*this, 48);
    o2_brief_lines();
    if (m.r8(pageRecord(m, m.r32(kBrief)) + 6) == 8)
        o2_brief_choices_final_tick();
    if (m.r8(pageRecord(m, m.r32(kBrief)) + 6) == 11)
        o2_brief_choices_tick();
    u32 panelContext = 0x80110000u;
    if (m.r32(m.r32(kMenu) + 0x114)) {
        const u32 skip = m.r8(pageRecord(m, m.r32(kBrief)) + 6) == 9 ? 64u : 16u;
        o2_panel_movie_dispatch(skip);
        if (!m.r32(m.r32(kMenu) + 0x128)) {
            for (s32 quad = 0; quad < m.r32s(m.r32(kBrief) + 0x1650); ++quad) {
                const u32 brief = m.r32(kBrief), menu = m.r32(kMenu);
                const u32 packet = o2_panel_movie_quad(m.r32(menu + 0x274), m.r32(menu + 8) + 8,
                    brief + 0x1654 + 8 * static_cast<u32>(quad), brief + 0x1674 + 8 * static_cast<u32>(quad));
                m.w32(m.r32(kMenu) + 0x274, packet);
                panelContext = 0x1654 + 8 * (static_cast<u32>(quad) + 1);
            }
        }
    }
    const u32 panelResult = o2_brief_panels(panelContext);
    if (panelResult == 1) m.w32(m.r32(kBrief) + 0xF38, 2);
    else if (panelResult == 2) {
        o2_restore_frontend_heap();
        m.w32(m.r32(kBrief) + 0xF38, 0);
        m.w32(m.r32(kMenu) + 0x128, 1);
    }
    u32 brief = m.r32(kBrief);
    if (m.r32(brief + 0xF38) == 1 && (m.r32(m.r32(g::kScene) + 0x12C) & 16u))
        m.w32(brief + 0xF38, 2);
    u32 menu = m.r32(kMenu);
    const u32 facePacket = face_draw(m.r32(menu + 8), m.r32(menu + 0x274));
    menu = m.r32(kMenu); brief = m.r32(kBrief);
    m.w32(menu + 0x274, facePacket);
    if (m.r16s(brief + 2) && m.r32(menu + 0x128) != 1) {
        const u32 tick = m.r32(brief + kAnimationTick) + 1;
        m.w32(brief + kAnimationTick, tick);
        return tick;
    }
    const s32 page = m.r16s(m.r32(brief + kCurrentPage));
    m.w16(menu + kMinorState, 12);
    m.w32(brief + 0x24, m.r8(pageRecord(m, brief) + 6));
    const u32 progress = m.r32(g::kScene) + 0xD98 + static_cast<u32>(page);
    const u32 oldProgress = m.r8(progress);
    if (oldProgress == 1) m.w8(progress, 2);
    else if (!oldProgress && page != 88 && page != 167 && page != 127 && page != 149)
        m.w8(progress, 1);
    const u32 rectangle = frame.at(16);
    for (u32 word = 0; word < 2; ++word) m.w32(rectangle + 4 * word, m.r32(0x800CCF68u + 4 * word));
    const u32 palette = m.r32(m.r32(kBrief) + 0x1694);
    if (palette) hw->load_image(rectangle, palette);
    brief = m.r32(kBrief);
    u32 scene = m.r32(g::kScene);
    if (m.r16s(pageRecord(m, brief)) == 0x153 && (m.r32(scene + 0x96C) & 32u)) {
        m.w32(m.r32(kMenu) + 0x94, 0x60000005u);
        m.w32(scene + 0x3A0, 3); m.w8(scene + 0x3A8, 29); m.w16(scene + 0x954, 1);
        m.w8(m.r32(g::kScene) + 0x95E, 0);
        return 1;
    }
    m.w32(brief + kCurrentPage, m.r32(brief + kCurrentPage) + 4);
    m.w16(brief, m.r16(brief) + 1);
    scene = m.r32(g::kScene);
    m.w8(scene + 0x95E, m.r8(scene + 0x95E) + 1);
    brief = m.r32(kBrief);
    m.w16(brief + 2, 1); m.w32(brief + kAnimationTick, 0);
    o2_entry_leave(0, m.r32(brief + 0x1C));
    m.w32(m.r32(kMenu) + 0x1A4, 60);
    o2_fade_set(1, 3, -1, -1);
    const u32 extraFade = m.r32(m.r32(kBrief) + 0x16AC);
    if (extraFade) o2_fade_set(extraFade, 3, -1, -1);
    m.w32(m.r32(kBrief) + 0xF38, 0);
    heap_set(m.r32(m.r32(kMenu) + 0x28));
    brief = m.r32(kBrief);
    m.w32(brief + 12, m.r32(brief + 12) - 1);
    o2_brief_voice_stop();
    sound_play(101);
    u32 result = 0;
    for (u32 voice = 0; voice < 5; ++voice)
        result = voice_level(m.r32(m.r32(kMenu) + 0x130 + 4 * voice), 100, 30);
    return result;
}

u32 Game::o2_brief_background_in() {
    cover(0x800F0F5Cu);
    Frame frame(*this, 24);
    const u32 brief = m.r32(kBrief);
    const s32 tick = m.r32s(brief + kAnimationTick);
    m.w32(brief + kAnimationTick, static_cast<u32>(tick) + 1);
    if (tick < 32) {
        const s32 step = static_cast<s32>(m.r32(brief + kAnimationTick) + 1);
        const u32 display = m.r32(0x800BCE48u);
        m.w8(display + 16, wrap::mul(step, 143) / 32);
        m.w8(display + 17, wrap::mul(step, 135) / 32);
        m.w8(display + 18, wrap::mul(step, 83) / 32);
        const u32 flags = m.r32(display) | 8u;
        m.w32(display, flags);
        return flags;
    }
    o2_hangar_fly();
    m.w32(m.r32(kBrief) + kAnimationTick, 0);
    m.w16(m.r32(kMenu) + kMinorState, 2);
    return 2;
}

u32 Game::o2_brief_page_unpack() {
    cover(0x800F1430u);
    Frame frame(*this, 56);
    o2_brief_lines();
    if (m.r32(m.r32(g::kScene) + 0x44)) return 0;
    const u32 source = m.r32(m.r32(0x800BCE50u) + 0x60);
    const u32 page = ld_relocate_dir(source);
    m.w32(m.r32(kMenu) + 0x28, source);
    const u32 soundData = m.r32(page + 12);
    if (m.r32(soundData))
        snd_nothing_a();
    const u32 brief = m.r32(kBrief);
    m.w32(brief + 0xF34, m.r32(page));
    m.w8(m.r32(m.r32(kScreens) + 0x5C) + 51, m.r8(pageRecord(m, brief) + 5));
    const u32 images = m.r32(page + 4);
    const s32 imageCount = m.r32s(images);
    const u32 savedRect = frame.at(16), movedRect = frame.at(24);
    m.w32(savedRect, m.r32(0x800CCF60u));
    m.w32(savedRect + 4, m.r32(0x800CCF64u));
    m.w32(m.r32(kMenu) + 0xB0, 0);
    m.w32(m.r32(kMenu) + 0xB4, 0);
    const u32 imageTable = ld_relocate_dir(images);
    m.w32(movedRect, m.r32(0x800CCF68u));
    m.w32(movedRect + 4, m.r32(0x800CCF6Cu));
    hw->move_image(movedRect, 0x340, 0x100);
    m.w32(m.r32(kBrief) + 0x1694, 0);
    if (imageCount >= 1 && imageCount <= 4)
        for (s32 image = 0; image < imageCount; ++image)
            o2_upload_tim(m.r32(imageTable + 4 * static_cast<u32>(image)));
    const u32 textData = m.r32(page + 8);
    if (m.r32(textData)) {
        o2_upload_pack_sized(textData, 0x4000);
        o2_brief_choices_init();
    }
    heap_set(m.r32(imageTable));
    const u32 savedImage = mem_alloc(0x8100);
    m.w32(m.r32(kBrief) + 0x1694, savedImage);
    hw->store_image(savedRect, savedImage);
    m.w16(m.r32(kMenu) + kMinorState, 6);
    return 6;
}

u32 Game::o2_brief_picture_begin() {
    cover(0x800F1848u);
    Frame frame(*this, 32);
    const s32 fade = o2_fade_state(3);
    o2_brief_lines();
    u32 result = m.r32(m.r32(kBrief) + kVoiceAvailable);
    if (result) {
        if (!stream_played()) {
            const u32 brief = m.r32(kBrief);
            if (m.r32s(brief + kTimeout) > 0) {
                m.w32(brief + kTimeout, m.r32(brief + kTimeout) - 1);
                return 0;
            }
        }
        result = 0x80110000u;
        if (m.r32(m.r32(kBrief) + kVoiceAvailable)) {
            const u32 stream = m.r32(m.r32(kMenu) + 0x12C);
            const u32 status = stream_played_of(stream);
            if (status == 0xFFFFFFFFu) {
                o2_fade_set(3, 3, -1, -1);
                result = m.r32(m.r32(kScreens) + 0x58) + 6;
            } else {
                result = m.r32(m.r32(g::kScene) + 0x12C) & 16u;
                if (result) {
                    o2_fade_set(3, 3, -1, -1);
                    cd_reset(0, 0);
                    result = 0;
                }
            }
        }
    }
    if (fade) return result;
    o2_brief_voice_stop();
    const u32 brief = m.r32(kBrief);
    o2_group_set(0, m.r32(brief + 0xF30) | (1u << (m.r32(brief + 0x1C) & 31)));
    const u32 pictureFade = o2_brief_picture_fade(m.r8(pageRecord(m, m.r32(kBrief)) + 6));
    m.w32(m.r32(kBrief) + 0x16AC, pictureFade);
    o2_fade_set(1, 1, 0, 15);
    const u32 selectedFade = m.r32(m.r32(kBrief) + 0x16AC);
    if (selectedFade) o2_fade_set(selectedFade, 1, 0, 15);
    sound_play(100);
    const u32 kind = m.r8(pageRecord(m, m.r32(kBrief)) + 6);
    if (kind <= 1) m.w8(m.r32(m.r32(kScreens) + 0x5C) + 50, kind);
    m.w16(m.r32(kMenu) + kMinorState, 8);
    return 8;
}

u32 Game::o2_brief_outro_begin() {
    cover(0x800F2240u);
    Frame frame(*this, 24);
    o2_brief_lines();
    if (m.r32(m.r32(g::kScene) + 0x12C) & 16u) o2_fade_set(1, 0, 0, 0);
    if (o2_fade_state(1)) return 1;
    const u32 brief = m.r32(kBrief);
    o2_group_set(0, m.r32(brief + 0xF30));
    m.w32(m.r32(kBrief) + kVoiceAvailable, 1);
    const u32 previous = m.r32(m.r32(kBrief) + kCurrentPage) - 4;
    const s32 record = m.r16s(kPageRecords + 8 * static_cast<u32>(m.r16s(previous)));
    switch (record) {
    case 270: case 331: case 349: m.w32(m.r32(kMenu) + 0x12C, 0x914); break;
    case 309: m.w32(m.r32(kMenu) + 0x12C, 0x916); break;
    default: m.w32(m.r32(kBrief) + kVoiceAvailable, 0); break;
    }
    if (o2_brief_voice_request()) {
        o2_group_set(0, 0x808000);
        o2_fade_set(3, 1, 0, 10);
        m.w8(m.r32(m.r32(kScreens) + 0x5C) + 50, 1);
    } else {
        o2_fade_set(3, 0, 0, 0);
    }
    m.w16(m.r32(kMenu) + kMinorState, 13);
    m.w32(m.r32(kBrief) + kTimeout, 180);
    return 180;
}

u32 Game::o2_brief_arrival_wait() {
    cover(0x800F13DCu);
    Frame frame(*this, 24);
    const u32 brief = m.r32(kBrief);
    if (m.r32s(brief + kAnimationTick) < 60)
        m.w32(brief + kAnimationTick, m.r32(brief + kAnimationTick) + 1);
    else
        m.w16(m.r32(kMenu) + kMinorState, 5);
    o2_brief_lines();
    return 0;
}

u32 Game::o2_brief_voice_begin() {
    cover(0x800F16C4u);
    Frame frame(*this, 24);
    o2_brief_lines();
    const u32 brief = m.r32(kBrief);
    m.w32(brief + kVoiceAvailable, 1);
    const s32 record = m.r16s(pageRecord(m, brief));
    switch (record) {
    case 331: case 270: m.w32(m.r32(kMenu) + 0x12C, 0x913); break;
    case 309: m.w32(m.r32(kMenu) + 0x12C, 0x915); break;
    case 372: case 362: case 376: m.w32(m.r32(kMenu) + 0x12C, 0x912); break;
    default: m.w32(brief + kVoiceAvailable, 0); break;
    }
    if (o2_brief_voice_request()) {
        o2_group_set(0, m.r32(m.r32(kBrief) + 0xF30) | 0x800000u);
        o2_fade_set(3, 1, 0, 10);
        m.w8(m.r32(m.r32(kScreens) + 0x5C) + 50, 0);
    }
    if (m.r32(m.r32(kBrief) + kVoiceAvailable)) sound_play(55);
    m.w16(m.r32(kMenu) + kMinorState, 7);
    m.w32(m.r32(kBrief) + kTimeout, 180);
    return 180;
}

u32 Game::o2_brief_fade_wait() {
    cover(0x800F1A7Cu);
    Frame frame(*this, 32);
    o2_brief_lines();
    if (o2_fade_state(1) == 2) {
        const u32 menu = m.r32(kMenu);
        m.w16(menu + kMinorState, 9);
        m.w32(menu + 0x1A4, 0);
        m.w32(m.r32(kBrief) + kAnimationTick, 0xFFFFFFFFu);
        u32 result = 0;
        for (u32 voice = 0; voice < 5; ++voice)
            result = voice_level(m.r32(m.r32(kMenu) + 0x130 + 4 * voice), 50, 30);
        return result;
    }
    const u32 menu = m.r32(kMenu);
    m.w32(menu + 0x1A4, m.r32(menu + 0x1A4) + 1);
    const u32 rect = frame.at(16);
    m.w32(rect, m.r32(0x800CCF70u));
    m.w32(rect + 4, m.r32(0x800CCF74u));
    hw->clear_image(rect, 0, 0, 0);
    return 0;
}

u32 Game::o2_brief_voice_wait() {
    cover(0x800F1C8Cu);
    Frame frame(*this, 24);
    o2_brief_lines();
    if (stream_played()) {
        m.w16(m.r32(kMenu) + kMinorState, 11);
        return 11;
    }
    const u32 brief = m.r32(kBrief);
    const u32 remaining = m.r32(brief + kTimeout) - 1;
    m.w32(brief + kTimeout, remaining);
    if (static_cast<s32>(remaining) >= 0) return remaining;
    m.w16(m.r32(kMenu) + kMinorState, 11);
    m.w32(brief + 0xF38, 2);
    return 2;
}

u32 Game::o2_brief_page_delay() {
    cover(0x800F2618u);
    Frame frame(*this, 24);
    o2_brief_lines();
    const u32 brief = m.r32(kBrief);
    if (m.r32s(brief + kAnimationTick) > 0) {
        const u32 remaining = m.r32(brief + kAnimationTick) - 1;
        m.w32(brief + kAnimationTick, remaining);
        return remaining;
    }
    if (!m.r32(brief + 12) || m.r8(pageRecord(m, brief) + 6) == 10) {
        const u32 menu = m.r32(kMenu);
        m.w16(menu + kMinorState, m.r16(menu + kMinorState) + 1);
        o2_entry_leave(0, 15);
        return sound_play(91);
    }
    m.w16(m.r32(kMenu) + kMinorState, 4);
    m.w32(brief + 0x28, 1 - m.r32(brief + 0x28));
    o2_entry_enter(0, m.r32(brief + 0x1C));
    return m.r32(kMenu);
}
u32 Game::o2_brief_outro_wait() {
    cover(0x800F23CCu);
    Frame frame(*this, 32);
    const s32 fade = o2_fade_state(3);
    o2_brief_lines();
    u32 result = m.r32(m.r32(kBrief) + kVoiceAvailable);
    if (result) {
        if (!stream_played()) {
            const u32 brief = m.r32(kBrief);
            m.w32(brief + kTimeout, m.r32(brief + kTimeout) - 1u);
            return 0;
        }
        result = m.r32(m.r32(kBrief) + kVoiceAvailable);
        if (result) {
            const u32 stream = m.r32(m.r32(kMenu) + 0x12C);
            const u32 status = stream_played_of(stream);
            if (status == 0xFFFFFFFFu) {
                o2_fade_set(3, 3, -1, -1);
                result = 0;
            } else {
                result = m.r32(m.r32(g::kScene) + 0x12C) & 16u;
                if (result) {
                    o2_fade_set(3, 3, -1, -1);
                    cd_reset(0, 0);
                    result = 0;
                }
            }
        }
    }
    if (fade) return result;
    o2_brief_voice_stop();
    o2_page_request();
    u32 brief = m.r32(kBrief);
    const u32 kind = m.r8(pageRecord(m, brief) + 6);
    m.w32(brief + 0x1C, o2_page_layout(kind));
    if (!m.r32(brief + 12) || kind == 10) {
        const u32 menu = m.r32(kMenu);
        m.w16(menu + kMinorState, 14);
        m.w32(menu + 0x1A4, 30);
        return 0;
    }
    if (kind >= 12) return 0;
    const bool firstBank = m.r32(brief + 0x24) < 3;
    const bool sameBank = kind <= 2 ? firstBank : !firstBank;
    if (sameBank) {
        m.w16(m.r32(kMenu) + kMinorState, 5);
        return 5;
    }
    m.w16(m.r32(kMenu) + kMinorState, 14);
    if (kind <= 2) m.w32(m.r32(kBrief) + kAnimationTick, 30);
    else m.w32(m.r32(kMenu) + 0x1A4, 30);
    return 30;
}

u32 Game::o2_brief_exit() {
    cover(0x800F2820u);
    Frame frame(*this, 32);
    const s32 fade = o2_fade_state(1);
    if (fade) return static_cast<u32>(fade);
    for (u32 offset : {0x7Cu, 0x7Du, 0x7Eu}) m.w8(m.r32(kMenu) + offset, 0);
    heap_set(m.r32(kBrief));
    o2_entry_enter(0, 1);
    o2_group_set(0, 3);
    o2_fade_set(15, 1, 0, 25);
    o2_hangar_effect2_begin();
    const u32 brief = m.r32(kBrief);
    if (m.r32(brief + 12) && m.r8(pageRecord(m, brief) + 6) == 10) {
        m.w32(m.r32(kMenu) + 0x11C, 1);
        const u32 scene = m.r32(g::kScene);
        m.w8(scene + 0x95E, m.r8(scene + 0x95E) + 1u);
        m.w16(scene + 0x954, 2);
        const s32 page = m.r16s(m.r32(m.r32(kBrief) + kCurrentPage));
        const u32 mission = m.r8(0x8010B360u + static_cast<u32>(page));
        o2_unlock_by_id(page);
        m.w8(m.r32(g::kScene) + 0x3A8, mission);
        return 0;
    }
    m.w8(m.r32(g::kScene) + 0x95E, 0);
    const u32 scene = m.r32(g::kScene);
    const u32 flags = m.r32(scene + 0x96C) & ~0x80u;
    m.w16(scene + 0x954, 0);
    m.w32(scene + 0x96C, flags);
    return flags;
}
}
