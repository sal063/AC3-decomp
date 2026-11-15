#include <algorithm>
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kState = 0x800D1544u;
constexpr u32 kSceneState = 0, kCursorLook = 0x452, kBlink = 0x453, kPackets = 0x454, kPacketCursor = 0x45C,
              kDelay = 0x460, kCommand = 0x462;
constexpr u32 kTables = 0x800D1548u;
constexpr u32 kPurgeLine = 0x800D1520u;
constexpr u32 kFade = 0x800D1524u;
constexpr u32 kOpeningFrame = 0x800D1528u;
constexpr u32 kPictureTpages = 0x800D1530u;
constexpr u32 kBufferRects = 0x800D1504u;
constexpr u32 kFontTim = 0x800CF4D0u;
constexpr u32 kDefaultName = 0x800CD6FCu;
constexpr u32 kScript = 0x800D0D50u, kTexts = 0x800D0D10u;
constexpr u32 kDisplay = 0x800BCE48u;
constexpr u32 kPlayerName = 0x948, kNextGroup = 0x3A0;
constexpr u32 kTag = 0x00FFFFFFu;
constexpr s32 kFadeBlack = -0x1000;

void link_new(Memory& m, u32 ot, u32 p, u32 words) {
    m.w32(p, m.r32(ot) | (words << 24));
    m.w32(ot, p & kTag);
}
void link_sized(Memory& m, u32 ot, u32 p) {
    m.w32(p, (m.r32(p) & ~kTag) | (m.r32(ot) & kTag));
    m.w32(ot, (m.r32(ot) & ~kTag) | (p & kTag));
}
}

u32 Game::o4_music_init() {
    cover(0x800CD734u);
    Frame frame(*this, 24);
    const u32 state = mem_alloc(24);
    m.w32(state + 12, 0x3FFF);
    m.w32(state + 4, 0);
    m.w32(state + 8, 0);
    m.w32(state, 0);
    m.w32(state + 16, 0xFF);
    m.w32(state + 20, 0);
    m.w32(0x800D1518u, state);
    return state;
}

u32 Game::o4_music_update() {
    cover(0x800CD780u);
    Frame frame(*this, 0x50);
    const auto state = [&] { return m.r32(0x800D1518u); };
    const auto pad = [&](u32 offset) { return m.r32(m.r32(g::kScene) + offset); };
    constexpr u32 kHeld = 0x128, kPressed = 0x12C, kRepeat = 0x130;
    constexpr u32 kTracks = 0x800CF2B0u;
    if (pad(kRepeat) & 4) m.w32(state() + 8, m.r32(state() + 8) - 1);
    if (pad(kRepeat) & 8) m.w32(state() + 8, m.r32(state() + 8) + 1);
    s32 volume = m.r32s(state() + 0xC);
    if (pad(kHeld) & 1) volume += 0x80;
    if (pad(kHeld) & 2) volume -= 0x80;
    s32 stream = static_cast<s32>(req_volume());
    if (pad(kHeld) & 0x100) --stream;
    if (pad(kHeld) & 0x200) ++stream;
    u32 mono = req_stereo();
    if (pad(kPressed) & 0x800) mono ^= 1;
    u32 command = 0;
    if (pad(kPressed) & 0x10) command = (m.r32(state()) & 1) ? 2 : 1;
    if ((pad(kPressed) & 0x80) && (m.r32(state()) & 1)) command = 3;
    const u32 pending = cd_pending(1);
    if (!pending && (m.r32(state()) & 1)) m.w32(state() + 8, m.r32(state() + 8) + 1);
    if (m.r32s(state() + 8) < 0) m.w32(state() + 8, 33);
    if (m.r32s(state() + 8) >= 34) m.w32(state() + 8, 0);
    volume = std::clamp(volume, 0, 0x7FFF);
    if (static_cast<u32>(volume) != m.r32(state() + 0xC)) {
        snd_cd_volume(static_cast<u32>(volume));
        m.w32(state() + 0xC, static_cast<u32>(volume));
    }
    if (static_cast<u32>(stream) != m.r32(state() + 0x10)) {
        stream = std::clamp(stream, 0, 255);
        req_volume_set(stream);
        req_volume_apply();
        m.w32(state() + 0x10, static_cast<u32>(stream));
    }
    if (m.r32(state() + 0x14) != mono) {
        req_stereo_set(mono);
        req_volume_apply();
        m.w32(state() + 0x14, mono);
    }
    if (kcd_stream_flags() & 0x40000000u) m.w32(state(), m.r32(state()) | 2u);
    else m.w32(state(), m.r32(state()) & ~2u);
    const auto play = [&] {
        cd_request_stream(kTracks + 12 * m.r32(state() + 8), 1, 0);
        const u32 s = state(), flags = m.r32(s) | 1u;
        m.w32(s + 4, m.r32(s + 8));
        return flags;
    };
    if (command == 2) {
        cd_pause((m.r32(state()) & 2) ? 0 : 1);
    } else if (command == 1) {
        if (!pending) m.w32(state(), play());
    } else if (command == 3) {
        cd_cancel_streams();
        m.w32(state(), m.r32(state()) & ~3u);
    }
    if (m.r32(state() + 4) != m.r32(state() + 8) && static_cast<s32>(pending) < 2 && !(pad(kHeld) & 0xC)
        && command == 0) {
        cd_cancel_streams();
        m.w32(state(), play() & ~2u);
    }
    debug_print(0x800CC0FCu, 0, 0, 0);
    const u32 s = state();
    u32 label = 0x800CC148u;
    if (m.r32(s) & 1) {
        if (m.r32(s) & 2) label = 0x800CC118u;
        else if (m.r32(s + 4) == m.r32(s + 8)) label = 0x800CC124u;
        else label = (pad(0x20) & 4) ? 0x800CC130u : 0x800CC13Cu;
    }
    debug_print(label, 0, 0, 0);
    if (!(m.r32(state()) & 2) || (pad(0x20) & 0x10)) {
        const s32 played = static_cast<s32>(stream_played());
        m.w32(frame.at(0x10), m.r32(0x800CF448u + 4 * m.r32(state() + 4)));
        debug_print(0x800CC154u, m.r32(state() + 8), static_cast<u32>(played / 3600),
                    static_cast<u32>(played / 60 - 60 * (played / 3600)));
    }
    debug_print(0x800CC170u, 0, 0, 0);
    const s32 cd = m.r32s(state() + 0xC);
    debug_print(0x800CC174u, static_cast<u32>(100 * cd / 32767), static_cast<u32>(cd), 0);
    const s32 at = m.r32s(state() + 0x10);
    debug_print(0x800CC18Cu, static_cast<u32>(100 * at / 255), static_cast<u32>(at),
                m.r32(state() + 0x14) ? 0x800CC1A8u : 0x800CC1B0u);
    const u32 record = stream_face_record();
    if (!record) return 0;
    m.w32(frame.at(0x10), static_cast<u32>(m.r16s(record + 8)));
    for (u32 k = 0; k < 6; ++k) m.w32(frame.at(0x14 + 4 * k), m.r8(record + 0xA + k));
    return debug_print(0x800CC1B8u, m.r16(record), static_cast<u32>(m.r16s(record + 2)), m.r32(record + 4));
}

void Game::o4_picture_keep() {
    cover(0x800CE9B4u);
    Frame f(*this, 24);
    hw->move_image(kBufferRects + (m.r32(m.r32(g::kScene) + 4) << 3), 0x2C0, 0);
}

u32 Game::o4_opening(u32 ot, u32 p) {
    cover(0x800CE9F4u);
    Frame f(*this, 0x38);
    const s32 n = m.r32s(kOpeningFrame);
    s32 half_width = 0x20;
    s32 half_height;
    u32 color;
    if (n < 11) {
        const s32 squeeze = rsin(static_cast<s32>(static_cast<u32>(n) << 10) / 10) * 240;
        half_height = 240 - squeeze / 4096;
        const u32 k = m.r32(kOpeningFrame);
        color = (k << 19) + (k << 11) + 0x808080u + (k << 3);
    } else {
        half_height = 1;
        half_width -= static_cast<s32>((static_cast<u32>(n) - 10) << 1);
        if (half_width < 2) {
            half_width = 1;
            m.w32(m.r32(kState) + kSceneState, 2);
        }
        color = 0xD8D8D8u;
    }
    const u32 top = static_cast<u32>(240 - half_height) << 16, bottom = static_cast<u32>(half_height + 239) << 16;
    if (half_height >= 2) {
        const u32 left = static_cast<u32>(160 - 5 * half_width), right = static_cast<u32>(5 * half_width + 159);
        m.w32(p + 4, 0x4EA0A0A0u);
        m.w32(p + 8, left | top);
        m.w32(p + 12, right | top);
        m.w32(p + 16, right | bottom);
        m.w32(p + 20, left | bottom);
        m.w32(p + 24, 0x55555555u);
        link_new(m, ot, p, 6);
        m.w32(p + 0x1C + 4, 0x42A0A0A0u);
        m.w32(p + 0x1C + 8, left | top);
        m.w32(p + 0x1C + 12, left | bottom);
        link_new(m, ot, p + 0x1C, 3);
        m.w32(f.at(0x10), 0);
        set_draw_mode(p + 0x2C, 0, 1, 0x120, 0);
        link_sized(m, ot, p + 0x2C);
        p += 0x38;
    }
    for (s32 i = 0; i < 5; ++i, p += 0x28) {
        m.w32(p + 4, color | 0x2C000000u);
        m.w32(p + 12, 0);
        m.w32(p + 20, m.r32(kPictureTpages + 4 * static_cast<u32>(i)) | 0x40u);
        m.w32(p + 28, 0xF000);
        m.w32(p + 36, 0xF040);
        if (half_width < 2) half_width = i == 2 ? 1 : 0;
        const u32 x0 = static_cast<u32>(2 * i * half_width - (5 * half_width - 160));
        const u32 x1 = x0 + static_cast<u32>(2 * half_width);
        m.w32(p + 8, x0 | top);
        m.w32(p + 16, x1 | top);
        m.w32(p + 24, x0 | bottom);
        m.w32(p + 32, x1 | bottom);
        link_new(m, ot, p, 9);
    }
    m.w32(p + 4, 0x62000000u);
    m.w32(p + 8, 0);
    m.w32(p + 12, 0x01E00140u);
    link_new(m, ot, p, 3);
    m.w32(f.at(0x10), 0);
    set_draw_mode(p + 0x10, 0, 1, 0x100, 0);
    link_sized(m, ot, p + 0x10);
    return p + 0x1C;
}

u32 Game::o4_phase0() {
    cover(0x800CED74u);
    Frame f(*this, 0x20);
    const u32 d = m.r32(kDisplay), sc = m.r32(g::kScene);
    m.w32(d + 4, 1);
    m.w32(d + 20, 0);
    m.w32(d, m.r32(d) | 1u);
    m.w32(sc, m.r32(sc) | 0x10000000u);
    m.w32(kState, mem_alloc(0x464));
    m.w32(kTables, mem_alloc(12));
    for (u32 k = 0; k < 2; ++k) m.w32(m.r32(kState) + kPackets + 4 * k, mem_alloc(0x65F0));
    for (u32 k = 0; k < 2; ++k) m.w32(m.r32(kTables) + 4 * k, mem_alloc(0x11C));
    o4_text_clear();
    const u32 st = m.r32(kState);
    m.w8(st + kCursorLook, 0);
    m.w32(st + kSceneState, 0);
    m.w8(st + kBlink, 0);
    m.w16(st + kDelay, 0);
    m.w16(st + kCommand, 0);
    load_tim(kFontTim);
    const u32 scene = m.r32(g::kScene);
    if (!m.r8(scene + kPlayerName)) {
        m.w32(scene + kPlayerName, m.r32(kDefaultName));
        m.w8(scene + kPlayerName + 4, m.r8(kDefaultName + 4));
    }
    m.w32(kPurgeLine, 0);
    m.w32(kFade, 0);
    m.w32(kOpeningFrame, 0);
    for (u32 i = 0; i < 5; ++i) m.w32(kPictureTpages + 4 * i, get_tpage(2, 1, 0x2C0 + 0x40 * i, 0) << 16);
    o4_picture_keep();
    return 0x04000000u;
}

u32 Game::o4_phase1() {
    cover(0x800CEEFCu);
    Frame f(*this, 0x28);
    const u32 screen = g::kScratch;
    m.w16(screen + 4, 320);
    m.w16(screen + 6, 480);
    m.w16(screen, 0);
    m.w16(screen + 2, 0);
    const u32 scene = m.r32(g::kScene);
    const u32 buffer = m.r32(scene + 4) << 2;
    const u32 s0 = m.r32(kState), tables = m.r32(kTables);
    m.w32(s0 + kPacketCursor, m.r32(s0 + buffer + kPackets));
    m.w32(tables + 8, m.r32(tables + buffer));
    m.w32(f.at(0x10), 0x808080);
    const u32 after_text = o4_draw_text(m.r32(scene + 8), m.r32(s0 + kPacketCursor), 4, 40, 0x808080);
    const u32 st = m.r32(kState);
    const u32 state = m.r32(st + kSceneState);
    m.w32(st + kPacketCursor, after_text);
    auto become = [&](u32 next) { m.w32(m.r32(kState) + kSceneState, next); };
    auto fade_color_clear = [&] {
        m.w8(m.r32(kDisplay) + 16, 0);
        m.w8(m.r32(kDisplay) + 17, 0);
        m.w8(m.r32(kDisplay) + 18, 0);
    };
    switch (state) {
    case 0:
        m.w32(kFade, 0);
        m.w32(kOpeningFrame, 0);
        become(1);
        break;
    case 1: {
        const u32 end = o4_opening(m.r32(m.r32(g::kScene) + 8), m.r32(m.r32(kTables) + 8));
        m.w32(kFade, 0);
        m.w32(m.r32(kTables) + 8, end);
        m.w32(kOpeningFrame, m.r32(kOpeningFrame) + 1);
        break;
    }
    case 2: {
        const s32 fade = m.r32s(kFade) - 0x88;
        m.w32(kFade, static_cast<u32>(fade));
        if (fade < kFadeBlack) become(3);
        break;
    }
    case 3: {
        fade_color_clear();
        const u32 d = m.r32(kDisplay);
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        m.w32(d, m.r32(d) | 8u);
        become(4);
        break;
    }
    case 4:
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        become(5);
        break;
    case 5:
        o4_script_step(kScript, kTexts);
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        become(6);
        m.w8(m.r32(kState) + kCursorLook, 1);
        break;
    case 6: {
        m.w8(m.r32(kState) + kCursorLook, 1);
        const s32 fade = m.r32s(kFade) + 0x2A;
        m.w32(kFade, static_cast<u32>(fade));
        if (fade < 0) break;
        m.w32(kFade, 0);
        become(7);
        break;
    }
    case 7:
        if (o4_script_step(kScript, kTexts)) become(8);
        m.w32(kFade, 0);
        break;
    case 8: {
        m.w8(m.r32(kState) + kCursorLook, 1);
        const s32 fade = m.r32s(kFade) - 0x2A;
        m.w32(kFade, static_cast<u32>(fade));
        if (fade >= kFadeBlack + 1) break;
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        become(9);
        break;
    }
    case 9:
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        become(10);
        break;
    case 10: {
        cd_reset(0, 0);
        const u32 d = m.r32(kDisplay);
        m.w8(d + 16, 0);
        m.w32(d + 4, 0);
        m.w32(d + 20, 0);
        m.w8(m.r32(kDisplay) + 17, 0);
        m.w8(m.r32(kDisplay) + 18, 0);
        const u32 d2 = m.r32(kDisplay);
        m.w32(kFade, static_cast<u32>(kFadeBlack));
        m.w32(d2, m.r32(d2) | 9u);
        become(11);
        break;
    }
    case 11: return 0x60000000u | m.r32(m.r32(g::kScene) + kNextGroup);
    default: break;
    }
    const u32 s2 = m.r32(kState);
    const u32 end = effect_flash_tile(m.r32(m.r32(g::kScene) + 12), m.r32(s2 + kPacketCursor), screen, m.r32s(kFade));
    m.w32(m.r32(kState) + kPacketCursor, end);
    return 0;
}
}
