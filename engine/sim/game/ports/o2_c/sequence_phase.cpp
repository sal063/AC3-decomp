#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kMenu = 0x80112DC4u, kSequence = 0x80112E04u;
constexpr u32 kLoaders = 0x80111710u, kVoiceChoices = 0x8010E674u;
u32 voiceChoice(Memory& m) {
    const u32 scene = m.r32(g::kScene);
    return kVoiceChoices + 2 * (4 * static_cast<u32>(m.r16s(scene + 0x38C)) + m.r8(scene + 0x956));
}
}

void Game::o2_sequence_setup(u32 rects) {
    const u32 sequence = mem_alloc(0x3D8);
    m.w32(kSequence, sequence);
    m.w8(sequence, 0); m.w8(sequence + 1, 0); m.w16(sequence + 2, 0);
    m.w8(m.r32(kSequence), 0);
    m.w16(m.r32(kSequence) + 0x3D4, 0);
    m.w32(m.r32(kMenu) + 0x268, 0);
    for (u32 buffer = 0; buffer < 2; ++buffer) {
        const u32 allocated = mem_alloc(0x1000);
        m.w32(m.r32(kSequence) + 4 + 4 * buffer, allocated);
    }
    o2_sequence_reset();
    o2_sequence_clear_banks();
    o2_text_font(7);
    o2_text_semi(1);
    o2_text_fold(1, 1, 1);
    o2_text_frame_mode(1);
    m.w32(rects, m.r32(0x800CD184u));
    m.w32(rects + 4, m.r32(0x800CD188u));
    m.w32(rects + 8, m.r32(0x800CD18Cu));
    m.w32(rects + 12, m.r32(0x800CD190u));
    hw->move_image(rects, 960, 256);
    hw->move_image(rects + 8, 960, 384);
}

u32 Game::o2_front_loader_call(u32 callback) {
    switch (callback) {
    case 0x800FB4F0u: return o2_ld_common();
    case 0x800FB574u: return o2_ld_wait();
    case 0x800FB58Cu: return o2_ld_common_unpack();
    case 0x800FB6E8u: return o2_ld_brief();
    case 0x800FB7A4u: return o2_ld_progress();
    case 0x800FB7F4u: return o2_ld_brief_unpack();
    case 0x800FB948u: return o2_ld_hangar();
    case 0x800FB9C4u: return o2_ld_hangar_wait();
    case 0x800FBA30u: return o2_ld_hangar_unpack();
    case 0x800FBB74u: return o2_ld_done();
    case 0x800FBBE0u: return o2_ld_fade();
    case 0x800FBC0Cu: return o2_loader_fade_ready();
    case 0x800FBC40u: return o2_loader_idle();
    default:
        if (callback >= 0x800F0000u && callback < 0x80110000u)
            unported(callback, "invalid front-end loading table entry");
        return call_by_address(callback, "front-end loader outside o2_c");
    }
}

u32 Game::o2_sequence_phase() {
    o2_text_rgb(5, 50, 53);
    o2_text_frame(m.r32(m.r32(kMenu) + 8));
    const s32 mode = m.r16s(m.r32(kMenu) + 0x1C6);
    const u32 loaded = o2_front_loader_call(m.r32(kLoaders + 4 * static_cast<u32>(mode)));
    const u32 choice = m.r8(voiceChoice(m));
    if (loaded - 1 < 2) {
        const u32 menu = m.r32(kMenu), next = m.r16(menu + 0x1C6) + 1;
        m.w16(menu + 0x1C6, next);
        if (static_cast<s16>(next) == 7 && choice) {
            o2_voice_control(1, m.r8(voiceChoice(m)));
            const u32 current = m.r32(kMenu);
            m.w32(current + 0x268, 1);
            if (m.r8(voiceChoice(m) + 1) == 0)
                o2_upload_packed_tims(m.r32(m.r32(current + 0x20) + 0x10));
            if (m.r8(voiceChoice(m) + 1) == 1)
                o2_upload_packed_tims(m.r32(m.r32(m.r32(kMenu) + 0x20) + 0x14));
        }
    }
    u32 menu = m.r32(kMenu);
    const u32 faceState = m.r32(menu + 0x268);
    if (faceState == 1) {
        if (stream_played()) {
            radio_face(3, 0, 0);
            m.w32(m.r32(kMenu) + 0x268, 2);
        }
    } else if (faceState == 2) {
        const u32 packet = face_draw(m.r32(menu + 8), m.r32(menu + 0x274));
        m.w32(m.r32(kMenu) + 0x274, packet);
    }
    menu = m.r32(kMenu);
    u32 packet = o2_screens_frame(m.r32(menu + 0x274));
    m.w32(m.r32(kMenu) + 0x274, packet);
    o2_sequence_events();
    o2_sequence_labels(0, 0);
    o2_sequence_clock_draw();
    packet = o2_menu_draw_area(m.r32(m.r32(kMenu) + 0x274), 0);
    m.w32(m.r32(kMenu) + 0x274, packet);
    packet = o2_sequence_markers(packet);
    m.w32(m.r32(kMenu) + 0x274, packet);
    packet = o2_sequence_trace(packet);
    m.w32(m.r32(kMenu) + 0x274, packet);
    packet = o2_sequence_map(packet);
    m.w32(m.r32(kMenu) + 0x274, packet);
    packet = o2_menu_draw_area(packet, 1);
    m.w32(m.r32(kMenu) + 0x274, packet);
    o2_text_at(20, 41); o2_text_printf(0x800CD224u, 0, 0, 0);
    o2_text_at(139, 343); o2_text_printf(0x800CD230u, 0, 0, 0);
    o2_sequence_tick();
    const u32 scene = m.r32(g::kScene);
    if (m.r32(scene + 0x12C) && !(m.r32(scene + 0x128) & 15u) && m.r16s(m.r32(kMenu) + 0x1C6) == 7) {
        o2_group_set(2, 0);
        o2_fade_set(1, 0, 0, 0);
        cd_reset(0, 0);
        return 0x04000000u;
    }
    o2_sequence_seconds();
    return 0;
}
}
