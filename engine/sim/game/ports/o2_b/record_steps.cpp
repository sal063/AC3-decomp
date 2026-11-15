#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DF8u, kScreens = 0x80112DC0u, kMenu = 0x80112DC4u;
u32 dialog(Memory& m) { return m.r32(kDialog); }
u32 flags(Memory& m) { return m.r32(m.r32(kScreens) + 92); }
void advance(Memory& m, u32 steps = 1) {
    const u32 record = dialog(m);
    m.w8(record + 1, m.r8(record + 1) + steps);
}
void restart(Memory& m, u32 phase) { m.w8(dialog(m), phase); m.w8(dialog(m) + 1, 0); }
void details(Game& game, Memory& m) {
    const u32 record = dialog(m);
    game.o2_pilot_record_text(m.r8(record + 6), m.r8(record + 5), m.r8(record + 10));
}
void highlights(Memory& m, bool blink) {
    for (u32 row = 0; row < 6; ++row) {
        const bool selected = m.r8(dialog(m) + 6) == row;
        const u32 value = selected ? ((blink && (m.r32(m.r32(g::kScene) + 292) & 16)) ? 3u : 0u) : 1u;
        m.w8(flags(m) + 2 + row, value);
    }
}
void record_markers(Memory& m) {
    m.w8(flags(m) + 9, m.r8(dialog(m) + 6) + 1);
    m.w8(flags(m) + 30, m.r8(dialog(m) + 6) + 1);
}
bool pressed(Memory& m, u32 button, u32 chord) {
    const u32 scene = m.r32(g::kScene);
    return (m.r32(scene + 300) & button) && !(m.r32(scene + 296) & chord);
}
template<class Sound>
void confirmation(Game& game, Memory& m, u32 fade, u32 fadeIndex,
                  u32 cancelButton, u32 cancelChord, u32 cancelPhase, u32 confirmPhase, bool paired, Sound&& sound) {
    if (pressed(m, cancelButton, cancelChord) || !fade) {
        if (fade && fade != 3) {
            game.o2_fade_set(fadeIndex, 3, -1, -1);
            if (paired) game.o2_fade_set(4, 3, -1, -1);
            m.w8(dialog(m) + 3, 1); sound(60);
        } else if (m.r8(dialog(m) + 3) == 1) {
            game.o2_fade_set(fadeIndex, 0, 0, 0);
            if (paired) game.o2_fade_set(4, 0, 0, 0);
            restart(m, cancelPhase);
        }
    }
    if (pressed(m, 32, 0x5F) || !fade) {
        if (fade && fade != 3) {
            game.o2_fade_set(fadeIndex, 3, -1, -1);
            if (paired) game.o2_fade_set(4, 3, -1, -1);
            m.w8(dialog(m) + 3, 2); sound(59);
        } else if (m.r8(dialog(m) + 3) == 2) {
            game.o2_fade_set(fadeIndex, 0, 0, 0);
            if (paired) game.o2_fade_set(4, 0, 0, 0);
            restart(m, confirmPhase);
        }
    }
}
}

u32 Game::o2_save_record_input() {
    cover(0x800EA3F4u);
    Frame frame(*this, 32);
    const u32 fade = o2_fade_state(1), status = o2_card_status(m.r8(dialog(m) + 4));
    highlights(m, false); details(*this, m); m.w8(flags(m) + 30, 0);
    if (status == 2 || status == 6) restart(m, 3);
    if (!status) return 0;
    if (fade == 2) {
        if (m.r32(m.r32(g::kScene) + 304) & 8) {
            const u32 record = dialog(m), index = m.r8(record + 6);
            m.w8(record + 6, index < 5 ? index + 1 : 0); sound_play(61);
        }
        if (m.r32(m.r32(g::kScene) + 304) & 4) {
            const u32 record = dialog(m), index = m.r8(record + 6);
            m.w8(record + 6, index ? index - 1 : 5); sound_play(61);
        }
    }
    confirmation(*this, m, fade, 1, 16, 0x2F, 14, 0, false, [this](u32 id) { sound_play(id); });
    return 0;
}

void Game::o2_save_record_confirm_begin() {
    cover(0x800EA744u);
    Frame frame(*this, 24);
    details(*this, m); m.w8(dialog(m) + 3, 0);
    const u32 record = dialog(m), unavailable = m.r8(record + 10);
    u32 present = 0;
    if (unavailable == 0) present = o2_pilot_progress_bit(m.r8(record + 5), m.r8(record + 6));
    else if (unavailable == 1) present = o2_pilot_progress_bit(0xFFFFFFFFu, m.r8(record + 6));
    if (present == 0 || present == 1) { advance(m, present + 1); o2_fade_set(2, 1, 0, 20); }
    o2_fade_set(4, 1, 0, 20);
    highlights(m, false);
}

u32 Game::o2_save_empty_record_input() {
    cover(0x800EA8ACu);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + 4));
    details(*this, m);
    const u32 fade = o2_fade_state(2);
    m.w8(flags(m) + 1, 63); highlights(m, true); record_markers(m);
    if (status == 2 || status == 6) restart(m, 3);
    if (status) confirmation(*this, m, fade, 2, 64, 0, 15, 13, true, [this](u32 id) { sound_play(id); });
    return 0;
}

void Game::o2_save_existing_record_input() {
    cover(0x800EAC04u);
    Frame frame(*this, 32);
    const u32 fade = o2_fade_state(2), status = o2_card_status(m.r8(dialog(m) + 4));
    details(*this, m);
    m.w8(flags(m) + 1, 64); highlights(m, true); record_markers(m);
    if (status == 2 || status == 6) { restart(m, 3); return; }
    if (status) confirmation(*this, m, fade, 2, 64, 0, 15, 13, true, [this](u32 id) { sound_play(id); });
}

void Game::o2_save_record_write_begin() {
    cover(0x800EAF44u);
    Frame frame(*this, 32);
    const u32 progress = o2_pilot_progress();
    details(*this, m); advance(m); m.w8(flags(m) + 1, 65); m.w8(flags(m) + 30, 0);
    o2_pilot_snapshot_store(); highlights(m, false);
    m.w8(dialog(m) + 10, (progress >> (m.r8(dialog(m) + 6) & 31)) & 1 ? 0u : 1u);
}

void Game::o2_save_record_pilot_step() {
    cover(0x800EB064u);
    Frame frame(*this, 24);
    details(*this, m);
    const u32 record = dialog(m);
    const u32 result = o2_card_slot_write(m.r8(record + 4), m.r8(record + 5), m.r8(record + 6), 1);
    if (result == 0xFFFFFFFFu) advance(m, 2);
    if (result == 1) { advance(m); o2_pilot_summary_store(); }
    highlights(m, false);
}

void Game::o2_save_record_summary_step() {
    cover(0x800EB174u);
    Frame frame(*this, 24);
    details(*this, m);
    const u32 record = dialog(m);
    const u32 result = o2_card_record_write(m.r8(record + 4), m.r8(record + 5), m.r8(record + 6), 1);
    if (result == 0xFFFFFFFFu) advance(m);
    if (result == 1) restart(m, 16);
    highlights(m, false);
}

u32 Game::o2_save_record_error_input() {
    cover(0x800EB280u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + 4));
    o2_fade_set(1, 2, 20, 20); o2_group_set(1, 0xC80); o2_entry_reset(1, 7);
    if (status == 2 || status == 6) restart(m, 1);
    if (status) {
        m.w8(flags(m) + 1, 67);
        if (m.r32(m.r32(g::kScene) + 300)) {
            o2_fade_set(1, 0, 0, 0); restart(m, 0); sound_play(59);
        }
    }
    return 1;
}

void Game::o2_save_pilot_display_begin() {
    cover(0x800EB388u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xE00); o2_entry_reset(1, 9); advance(m);
    m.w8(dialog(m) + 7, 0); m.w8(dialog(m) + 9, 0);
    m.w8(flags(m) + 1, 68); m.w8(flags(m) + 30, 0);
    m.w8(m.r32(g::kScene) + 2397, m.r8(dialog(m) + 6));
}

void Game::o2_save_pilot_display_wait() {
    cover(0x800EB42Cu);
    Frame frame(*this, 24);
    o2_text_at(74, 214); o2_text_printf(o2_pilot_name(0), 0, 0, 0);
    if (m.r8(dialog(m) + 7) >= 121 || m.r32(m.r32(g::kScene) + 300)) {
        restart(m, o2_save_exit(static_cast<s32>(m.r32(m.r32(kMenu) + 224))));
        m.w8(dialog(m) + 7, 0);
    }
    const u32 record = dialog(m); m.w8(record + 7, m.r8(record + 7) + 1);
}

void Game::o2_save_exit_confirm_begin() {
    cover(0x800EB4F4u);
    Frame frame(*this, 24);
    m.w8(dialog(m) + 8, 1); o2_group_set(1, 0xC01); o2_entry_reset(1, 0);
    o2_fade_set(2, 1, 0, 20); advance(m); m.w8(flags(m) + 1, 69);
}

void Game::o2_save_exit_confirm_input() {
    cover(0x800EB57Cu);
    Frame frame(*this, 32);
    const u32 fade = o2_fade_state(2);
    if (pressed(m, 16, 0x2F) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(2, 3, -1, -1); m.w8(dialog(m) + 3, 1); sound_play(60);
        } else if (m.r8(dialog(m) + 3) == 1) { o2_fade_set(2, 0, 0, 0); restart(m, 18); }
    }
    if (pressed(m, 32, 0x5F) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(2, 3, -1, -1); m.w8(dialog(m) + 3, 2); sound_play(59);
        } else if (m.r8(dialog(m) + 3) == 2) {
            o2_fade_set(2, 0, 0, 0); m.w8(dialog(m) + 2, 2);
            const u32 scene = m.r32(g::kScene); m.w32(scene + 2412, m.r32(scene + 2412) | 2);
        }
    }
}
void Game::o2_save_audio_fade_begin() {
    cover(0x800EBBD0u);
    if (m.r32(m.r32(0x80112DA8u) + 20)) return;
    const u32 menu = m.r32(kMenu); m.w16(menu + 134, m.r16(menu + 134) + 1);
    m.w8(dialog(m) + 7, m.r8(m.r32(0x800BCE48u) + 16) == 143 ? 32u : 1u);
}

u32 Game::o2_save_audio_fade_step() {
    cover(0x800EBC3Cu);
    const u32 record = dialog(m), countdown = (m.r8(record + 7) - 1) & 255;
    m.w8(record + 7, countdown);
    if (!countdown) { const u32 menu = m.r32(kMenu); m.w16(menu + 134, m.r16(menu + 134) + 1); }
    m.w8(m.r32(0x800BCE48u) + 16, m.r8(dialog(m) + 7) * 143 / 32);
    m.w8(m.r32(0x800BCE48u) + 17, m.r8(dialog(m) + 7) * 135 / 32);
    m.w8(m.r32(0x800BCE48u) + 18, m.r8(dialog(m) + 7) * 83 / 32);
    const u32 audio = m.r32(0x800BCE48u), result = m.r32(audio) | 8;
    m.w32(audio, result); return result;
}

void Game::o2_save_card_select_begin() {
    cover(0x800E7B8Cu);
    Frame frame(*this, 32);
    m.w8(dialog(m) + 8, 0); m.w8(dialog(m) + 9, 0);
    const u32 first = o2_card_status(0), second = o2_card_status(1);
    const auto bad = [](u32 status) { return status == 2 || status == 6; };
    if (bad(first) && bad(second)) {
        o2_group_set(1, 0xC01); o2_entry_reset(1, 0); restart(m, 1); m.w8(dialog(m) + 8, 1);
    } else {
        o2_group_set(1, 0xC04); o2_entry_reset(1, 2); o2_fade_set(1, 1, 0, 20);
        advance(m); m.w8(flags(m) + 1, 46);
    }
    if (first != 5 && second == 5) m.w8(dialog(m) + 4, 1);
    m.w8(flags(m) + 30, 0);
}

void Game::o2_save_card_select_input() {
    cover(0x800E7CDCu);
    Frame frame(*this, 40);
    const u32 fade = o2_fade_state(1), oldCard = m.r8(dialog(m) + 4);
    const u32 first = o2_card_status(0), second = o2_card_status(1);
    const auto bad = [](u32 status) { return status == 2 || status == 6; };
    if (m.r32(m.r32(g::kScene) + 304) & 3) {
        const u32 record = dialog(m); m.w8(record + 4, m.r8(record + 4) ^ 1);
    }
    if (bad(first)) m.w8(dialog(m) + 4, 1);
    if (bad(second)) m.w8(dialog(m) + 4, 0);
    if (bad(first) && bad(second)) {
        o2_fade_set(1, 0, 0, 0); o2_group_set(1, 0xC01); o2_entry_reset(1, 0); restart(m, 1); return;
    }
    m.w8(flags(m) + 2, bad(first) ? 2u : (m.r8(dialog(m) + 4) == 0 ? 0u : 1u));
    m.w8(flags(m) + 3, bad(second) ? 2u : (m.r8(dialog(m) + 4) == 1 ? 0u : 1u));
    if (pressed(m, 16, 0x2F) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(1, 3, -1, -1); m.w8(dialog(m) + 3, 1); sound_play(60);
        } else if (m.r8(dialog(m) + 3) == 1) { o2_fade_set(1, 0, 0, 0); restart(m, 5); return; }
    }
    if (pressed(m, 32, 0x5F) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(1, 3, -1, -1); m.w8(dialog(m) + 3, 2); sound_play(59);
        } else if (m.r8(dialog(m) + 3) == 2) { o2_fade_set(1, 0, 0, 0); restart(m, 0); return; }
    }
    if ((m.r32(m.r32(g::kScene) + 304) & 3) && oldCard != m.r8(dialog(m) + 4)) sound_play(61);
}
}
