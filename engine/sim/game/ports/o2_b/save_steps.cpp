#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DF8u, kMenu = 0x80112DC4u, kScreens = 0x80112DC0u;
constexpr u32 kPhase = 0, kStep = 1, kCard = 4, kSlot = 5;
u32 dialog(Memory& m) { return m.r32(kDialog); }
void advance(Memory& m, u32 steps = 1) {
    const u32 record = dialog(m);
    m.w8(record + kStep, m.r8(record + kStep) + steps);
}
void restart(Memory& m, u32 phase) {
    m.w8(dialog(m) + kPhase, phase);
    m.w8(dialog(m) + kStep, 0);
}
void screen_flag(Memory& m, u32 flag) { m.w8(m.r32(m.r32(kScreens) + 92) + 1, flag); }
void pilot_highlights(Memory& m) {
    for (u32 row = 0; row < 3; ++row) {
        const bool selected = m.r8(dialog(m) + kSlot) == row;
        const u32 flag = selected ? ((m.r32(m.r32(g::kScene) + 292) & 16) ? 3u : 0u) : 1u;
        m.w8(m.r32(m.r32(kScreens) + 92) + 2 + row, flag);
    }
}
}

void Game::o2_save_missing_begin() {
    cover(0x800E83ACu);
    Frame frame(*this, 24);
    advance(m);
    o2_fade_set(2, 1, 0, 20);
    screen_flag(m, 47);
}

void Game::o2_save_missing_wait() {
    cover(0x800E8404u);
    Frame frame(*this, 24);
    if (o2_card_status(m.r8(dialog(m) + kCard))) restart(m, 6);
}

void Game::o2_save_card_error_begin() {
    cover(0x800E8454u);
    Frame frame(*this, 24);
    if (o2_card_status(m.r8(dialog(m) + kCard)) == 1) {
        o2_group_set(1, 0xC01);
        o2_entry_reset(1, 0);
        screen_flag(m, 49);
        advance(m);
    } else {
        m.w8(dialog(m) + kPhase, 7);
    }
}

void Game::o2_save_card_error_input() {
    cover(0x800E84E8u);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    if (status == 2 || status == 6) { restart(m, 1); return; }
    if (!status) return;
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 0x10) && !(m.r32(scene + 296) & 0x2F)) {
        restart(m, o2_save_exit(m.r32s(m.r32(kMenu) + 224)));
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 0x20) && !(m.r32(scene + 296) & 0x5F)) restart(m, 3);
}

void Game::o2_save_unformatted_begin() {
    cover(0x800E8600u);
    Frame frame(*this, 24);
    if (o2_card_status(m.r8(dialog(m) + kCard)) == 3) {
        advance(m);
        o2_group_set(1, 0xC02);
        o2_entry_reset(1, 1);
        screen_flag(m, 51);
    } else {
        m.w8(dialog(m) + kPhase, 8);
    }
}

void Game::o2_save_error_fade() {
    cover(0x800E868Cu);
    Frame frame(*this, 24);
    const s32 state = o2_fade_state(1);
    if (state == 0) {
        o2_fade_set(2, 1, 0, 20);
        advance(m);
    } else if (state != 3) {
        o2_fade_set(1, 3, -1, -1);
    }
}

void Game::o2_save_header_choose() {
    cover(0x800E8B2Cu);
    Frame frame(*this, 24);
    if (o2_card_status(m.r8(dialog(m) + kCard)) == 4) restart(m, 9);
    else advance(m);
}

void Game::o2_save_slot_choose() {
    cover(0x800E8D08u);
    Frame frame(*this, 24);
    const u32 slot = o2_pilot_slot_find();
    if (slot == 0xFFFFFFFFu) restart(m, 9);
    else {
        m.w8(dialog(m) + kSlot, slot);
        advance(m);
    }
}

void Game::o2_save_create_poll() {
    cover(0x800E94E0u);
    Frame frame(*this, 24);
    const u32 result = o2_card_file_create(m.r8(dialog(m) + kCard));
    if (result == 1) advance(m);
    if (result == 0xFFFFFFFFu) advance(m, 3);
}

void Game::o2_save_create_error_begin() {
    cover(0x800E97A4u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xC10);
    o2_entry_reset(1, 4);
    o2_fade_set(1, 1, 0, 60);
    screen_flag(m, 57);
    m.w8(dialog(m) + 9, 1);
    advance(m);
}
void Game::o2_save_prompt_begin() {
    cover(0x800E7498u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xC01);
    o2_entry_reset(1, 11);
    o2_entry_reset(1, 0);
    o2_entry_reset(1, 10);
    o2_fade_set(1, 1, 0, 20);
    m.w8(dialog(m) + 3, 0);
    m.w8(dialog(m) + 8, 1);
    m.w8(m.r32(m.r32(kScreens) + 92) + 8, 0);
    m.w8(m.r32(m.r32(kScreens) + 92) + 30, 0);
    advance(m);
}

void Game::o2_save_prompt_input() {
    cover(0x800E7554u);
    Frame frame(*this, 32);
    screen_flag(m, 43);
    const s32 state = o2_fade_state(1);
    u32 scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F)) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(1, 3, -1, -1);
            m.w8(dialog(m) + 3, 1);
            sound_play(60);
        } else if (m.r8(dialog(m) + 3) == 1) {
            screen_flag(m, 47);
            o2_fade_set(1, 0, 0, 0);
            restart(m, 1);
        }
    }
    scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(1, 3, -1, -1);
            m.w8(dialog(m) + 3, 2);
            sound_play(59);
        } else if (m.r8(dialog(m) + 3) == 2) {
            screen_flag(m, 0);
            o2_fade_set(1, 0, 0, 0);
            restart(m, o2_save_exit(m.r32s(m.r32(kMenu) + 224)));
        }
    }
}

void Game::o2_save_presence_begin() {
    cover(0x800E7758u);
    Frame frame(*this, 24);
    m.w8(dialog(m) + 8, 1);
    o2_group_set(1, 0xC01);
    if (!o2_card_status(0) || !o2_card_status(1)) {
        o2_fade_set(5, 1, 0, 20);
        advance(m);
    } else {
        m.w8(dialog(m) + kPhase, 2);
    }
}

void Game::o2_save_presence_wait() {
    cover(0x800E77ECu);
    Frame frame(*this, 24);
    const s32 state = o2_fade_state(5);
    if (!o2_card_status(0) || !o2_card_status(1)) { screen_flag(m, 47); return; }
    if (state == 0) {
        o2_fade_set(5, 0, 0, 0);
        restart(m, 2);
    } else if (state != 3) {
        o2_fade_set(5, 3, -1, -1);
    }
}

void Game::o2_save_pair_error_begin() {
    cover(0x800E78A0u);
    Frame frame(*this, 24);
    const u32 first = o2_card_status(0), second = o2_card_status(1);
    if ((first == 2 || first == 6) && (second == 2 || second == 6)) {
        advance(m);
        o2_fade_set(4, 1, 0, 20);
        screen_flag(m, 44);
    } else {
        m.w8(dialog(m) + kPhase, 3);
    }
}
void Game::o2_save_pair_input(bool missingCard) {
    const s32 state = o2_fade_state(4);
    const u32 first = o2_card_status(0), second = o2_card_status(1);
    const bool bothError = (first == 2 || first == 6) && (second == 2 || second == 6);
    const bool problem = missingCard ? (!first || !second) : !bothError;
    if (problem) {
        if (state != 0 && state != 3) {
            o2_fade_set(4, 3, -1, -1);
            m.w8(dialog(m) + 3, 3);
        } else if (m.r8(dialog(m) + 3) == 3) {
            restart(m, missingCard ? 5u : 1u);
            return;
        }
    }
    u32 scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F)) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(4, 3, -1, -1);
            m.w8(dialog(m) + 3, 1);
        } else if (m.r8(dialog(m) + 3) == 1) {
            o2_fade_set(4, 0, 0, 0);
            restart(m, o2_save_exit(m.r32s(m.r32(kMenu) + 224)));
        }
    }
    scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(4, 3, -1, -1);
            m.w8(dialog(m) + 3, 2);
        } else if (m.r8(dialog(m) + 3) == 2) {
            o2_fade_set(4, 0, 0, 0);
            restart(m, missingCard ? 3u : 0u);
        }
    }
}

void Game::o2_save_pair_error_input() {
    cover(0x800E7948u);
    Frame frame(*this, 32);
    o2_save_pair_input(false);
}

void Game::o2_save_missing_input() {
    cover(0x800E8174u);
    Frame frame(*this, 32);
    o2_save_pair_input(true);
}

void Game::o2_save_io_error_input() {
    cover(0x800E89F8u);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    screen_flag(m, 80);
    if (status == 2 || status == 6) { restart(m, 1); return; }
    if (!status) return;
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F))
        restart(m, o2_save_exit(m.r32s(m.r32(kMenu) + 224)));
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) restart(m, 0);
}

void Game::o2_save_confirm_begin() {
    cover(0x800E8FC4u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xC02);
    o2_entry_reset(1, 1);
    o2_fade_set(2, 0, 0, 0);
    o2_fade_set(3, 1, 0, 20);
    screen_flag(m, 55);
    m.w8(dialog(m) + 9, 1);
    m.w8(dialog(m) + 3, 0);
    advance(m);
}

u32 Game::o2_save_confirm_input() {
    cover(0x800E9068u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    const s32 state = o2_fade_state(3);
    if (status == 2 || status == 6) { restart(m, 1); return 0; }
    if (!status) return 0;
    if ((m.r32(m.r32(g::kScene) + 300) & 64) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(3, 3, -1, -1);
            m.w8(dialog(m) + 3, 1);
            sound_play(60);
        } else if (m.r8(dialog(m) + 3) == 1) {
            o2_fade_set(3, 0, 0, 0);
            restart(m, 10);
            screen_flag(m, 47);
        }
    }
    const u32 scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) || state == 0) {
        if (state != 0 && state != 3) {
            o2_fade_set(3, 3, -1, -1);
            m.w8(dialog(m) + 3, 2);
            sound_play(59);
        } else if (m.r8(dialog(m) + 3) == 2) {
            o2_fade_set(3, 0, 0, 0);
            restart(m, 3);
        }
    }
    return 0;
}
void Game::o2_save_header_read_step() {
    cover(0x800E8B98u);
    Frame frame(*this, 24);
    const u32 result = o2_card_header_read(m.r8(dialog(m) + kCard), 0);
    if (result == 0xFFFFFFFFu || result == 2) {
        screen_flag(m, result == 2 ? 81u : 80u);
        m.w8(dialog(m) + kStep, 6);
    }
    if (result == 1) advance(m);
}

void Game::o2_save_icons_write_step() {
    cover(0x800E955Cu);
    Frame frame(*this, 24);
    const u32 result = o2_card_icons_write(m.r8(dialog(m) + kCard), 0);
    if (result == 0xFFFFFFFFu) advance(m, 2);
    if (result == 1) {
        advance(m);
        o2_pilot_header_init();
    }
}

void Game::o2_save_empty_header_step() {
    cover(0x800E95DCu);
    Frame frame(*this, 24);
    const u32 result = o2_card_header_write(m.r8(dialog(m) + kCard), -1, 1);
    if (result == 0xFFFFFFFFu) advance(m);
    if (result == 1) restart(m, 11);
}
void Game::o2_save_slots_read_step() {
    cover(0x800E8C50u);
    Frame frame(*this, 24);
    const u32 result = o2_card_slots_read(m.r8(dialog(m) + kCard), 1);
    if (result == 0xFFFFFFFFu || result == 2) {
        screen_flag(m, result == 2 ? 81u : 80u);
        m.w8(dialog(m) + kStep, 6);
    }
    if (result == 1) advance(m);
}

void Game::o2_save_records_read_step() {
    cover(0x800E8E28u);
    Frame frame(*this, 24);
    const u32 selected = dialog(m);
    const u32 result = o2_card_records_read(m.r8(selected + kCard), m.r8(selected + kSlot), 1);
    if (result == 0xFFFFFFFFu || result == 2) {
        m.w8(dialog(m) + kStep, 6);
        screen_flag(m, result == 2 ? 81u : 80u);
    }
    if (result == 1) {
        restart(m, 13);
        o2_fade_set(2, 0, 0, 0);
    }
}
void Game::o2_save_selected_error_begin() {
    cover(0x800E80B8u);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    if (status == 2 || status == 6) {
        advance(m);
        o2_fade_set(4, 1, 0, 20);
        o2_group_set(1, 0xC01);
        o2_entry_reset(1, 0);
        screen_flag(m, 44);
    } else {
        m.w8(dialog(m) + kPhase, 6);
    }
    o2_pilot_header_init();
    o2_pilot_records_init();
}
void Game::o2_save_unformatted_input() {
    cover(0x800E8708u);
    Frame frame(*this, 32);
    const u32 fade = o2_fade_state(2), status = o2_card_status(m.r8(dialog(m) + kCard));
    if (status == 2 || status == 6) { restart(m, 1); return; }
    if (!status) return;
    if ((m.r32(m.r32(g::kScene) + 300) & 64) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(2, 3, -1, -1);
            m.w8(dialog(m) + 3, 1);
            sound_play(60);
            return;
        }
        if (m.r8(dialog(m) + 3) == 1) {
            o2_fade_set(2, 0, 0, 0);
            o2_fade_set(3, 1, 0, 20);
            advance(m);
            screen_flag(m, 53);
            return;
        }
    }
    const u32 scene = m.r32(g::kScene);
    if (((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) || !fade) {
        if (fade && fade != 3) {
            o2_fade_set(2, 3, -1, -1);
            m.w8(dialog(m) + 3, 2);
            sound_play(59);
        } else if (m.r8(dialog(m) + 3) == 2) {
            o2_fade_set(2, 0, 0, 0);
            restart(m, 3);
        }
    }
}

void Game::o2_save_format_step() {
    cover(0x800E8910u);
    Frame frame(*this, 32);
    if (o2_fade_state(3) != 2) return;
    const u32 result = o2_card_format(m.r8(dialog(m) + kCard));
    if (result == 1) {
        o2_fade_set(3, 3, -1, -1);
        advance(m);
        m.w8(dialog(m) + 9, 1);
    } else if (result == 0xFFFFFFFFu) {
        o2_fade_set(3, 3, -1, -1);
        advance(m, 2);
        o2_group_set(1, 0xC01);
        screen_flag(m, 80);
    }
}

void Game::o2_save_slot_select_step() {
    cover(0x800E8D78u);
    Frame frame(*this, 24);
    o2_pilot_slot_load(m.r8(dialog(m) + kSlot));
    m.w8(dialog(m) + 10, 0);
    if (m.r32(m.r32(kMenu) + 224) - 2 < 3) {
        o2_pilot_snapshot_store();
        restart(m, 12);
        m.w8(dialog(m) + kStep, 6);
        screen_flag(m, 59);
    } else {
        advance(m);
    }
}
void Game::o2_save_read_error_input() {
    cover(0x800E8EF0u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xC01);
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F)) {
        restart(m, o2_save_exit(m.r32(m.r32(kMenu) + 224)));
        sound_play(60);
    }
    scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) {
        m.w8(dialog(m) + kPhase, 0);
        sound_play(59);
    }
}

void Game::o2_save_card_route_step() {
    cover(0x800E927Cu);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    if (status == 5) restart(m, 11);
    else if (status == 2 || status == 6) restart(m, 1);
    else if (status) advance(m);
}

void Game::o2_save_space_step() {
    cover(0x800E9310u);
    Frame frame(*this, 24);
    const s32 blocks = static_cast<s32>(o2_card_directory(m.r8(dialog(m) + kCard)));
    if (blocks == -1) return;
    if (blocks < 2) {
        advance(m);
        screen_flag(m, 56);
        o2_group_set(1, 0xC01);
        o2_cards_poll_begin();
    } else advance(m, 2);
}

u32 Game::o2_save_space_input() {
    cover(0x800E93B8u);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    if (status == 2 || status == 6) restart(m, 1);
    if (status) {
        u32 scene = m.r32(g::kScene);
        if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F))
            restart(m, o2_save_exit(m.r32(m.r32(kMenu) + 224)));
        scene = m.r32(g::kScene);
        if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) restart(m, 1);
    }
    return 0;
}

u32 Game::o2_save_write_error_input() {
    cover(0x800E965Cu);
    Frame frame(*this, 24);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    m.w8(dialog(m) + 9, 0);
    screen_flag(m, 80);
    if (status == 2 || status == 6) restart(m, 1);
    if (status) {
        u32 scene = m.r32(g::kScene);
        if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F)) {
            restart(m, o2_save_exit(m.r32(m.r32(kMenu) + 224)));
            sound_play(60);
        }
        scene = m.r32(g::kScene);
        if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) {
            m.w8(dialog(m) + kPhase, 0);
            sound_play(59);
        }
    }
    return 1;
}

u32 Game::o2_save_complete_input() {
    cover(0x800EA1C4u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    m.w8(dialog(m) + 9, 1);
    o2_fade_set(1, 2, 20, 20);
    o2_group_set(1, 0xC80);
    o2_entry_reset(1, 7);
    screen_flag(m, 61);
    if (status == 2 || status == 6) restart(m, 1);
    if (status && m.r32(m.r32(g::kScene) + 300)) {
        m.w8(dialog(m) + 9, 0);
        o2_fade_set(1, 0, 0, 0);
        restart(m, 0);
        sound_play(59);
    }
    return 1;
}

void Game::o2_save_records_begin() {
    cover(0x800EA2E8u);
    Frame frame(*this, 24);
    o2_group_set(1, 0xD00);
    o2_entry_reset(1, 8);
    o2_entry_reset(1, 11);
    o2_fade_set(1, 1, 0, 20);
    o2_fade_set(15, 1, 0, 20);
    screen_flag(m, 62);
    m.w8(dialog(m) + 9, 1);
    for (u32 row = 0; row < 6; ++row)
        m.w8(m.r32(m.r32(kScreens) + 92) + 2 + row, m.r8(dialog(m) + 6) == row ? 0u : 1u);
    advance(m);
}
u32 Game::o2_save_slot_input() {
    cover(0x800E9824u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    screen_flag(m, status ? 57u : 47u);
    m.w8(dialog(m) + 10, 0);
    pilot_highlights(m);
    if (status == 2 || status == 6) restart(m, 1);
    o2_pilot_slots_text(m.r8(dialog(m) + 10), 0);
    if (!status) return 0;
    u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 16) && !(m.r32(scene + 296) & 0x2F)) {
        restart(m, 12); sound_play(60);
    } else if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) {
        restart(m, 0); sound_play(59);
    } else {
        const u32 repeat = m.r32(scene + 304), selected = dialog(m);
        if (repeat & 2) {
            const u32 slot = m.r8(selected + kSlot);
            m.w8(selected + kSlot, slot < 2 ? slot + 1 : 0);
            sound_play(61);
        } else if (repeat & 1) {
            const u32 slot = m.r8(selected + kSlot);
            m.w8(selected + kSlot, slot ? slot - 1 : 2);
            sound_play(61);
        }
    }
    return 0;
}

void Game::o2_save_overwrite_begin() {
    cover(0x800E9AB4u);
    Frame frame(*this, 24);
    if (o2_pilot_slot_used(m.r8(dialog(m) + kSlot))) {
        advance(m);
        m.w16(dialog(m) + 26, 1);
        pilot_highlights(m);
        o2_group_set(1, 0xC20);
        o2_entry_reset(1, 5);
        o2_fade_set(1, 1, 0, 60);
        screen_flag(m, 58);
    } else {
        advance(m, 2);
        m.w16(dialog(m) + 26, 0);
        o2_pilot_slot_reset(m.r8(dialog(m) + kSlot));
    }
    o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
}

u32 Game::o2_save_overwrite_input() {
    cover(0x800E9C40u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    o2_pilot_slots_text(m.r8(dialog(m) + 10), 1);
    pilot_highlights(m);
    if (status == 2 || status == 6) { restart(m, 3); m.w16(dialog(m) + 26, 0); }
    if (!status) return 0;
    if (m.r32(m.r32(g::kScene) + 300) & 64) {
        advance(m); sound_play(60);
        o2_pilot_slot_reset(m.r8(dialog(m) + kSlot));
    }
    const u32 scene = m.r32(g::kScene);
    if ((m.r32(scene + 300) & 32) && !(m.r32(scene + 296) & 0x5F)) {
        restart(m, 11); m.w16(dialog(m) + 26, 0); sound_play(59);
    }
    return 0;
}

void Game::o2_save_file_route_step() {
    cover(0x800E9E20u);
    Frame frame(*this, 32);
    const u32 status = o2_card_status(m.r8(dialog(m) + kCard));
    o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
    if (status == 4) { advance(m); screen_flag(m, 59); }
    else advance(m, 3);
}

u32 Game::o2_save_file_create_step() {
    cover(0x800E9EC4u);
    Frame frame(*this, 24);
    o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
    const u32 result = o2_card_file_create(m.r8(dialog(m) + kCard));
    if (result == 0xFFFFFFFFu) advance(m, 4);
    if (result == 1) {
        const u32 next = m.r8(dialog(m) + kStep) + 1;
        advance(m);
        return next;
    }
    return 1;
}

void Game::o2_save_file_icons_step() {
    cover(0x800E9F64u);
    Frame frame(*this, 24);
    o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
    const u32 result = o2_card_icons_write(m.r8(dialog(m) + kCard), 0);
    if (result == 0xFFFFFFFFu) advance(m, 3);
    if (result == 1) { advance(m); o2_pilot_header_init(); }
}

void Game::o2_save_file_header_step() {
    cover(0x800EA004u);
    Frame frame(*this, 24);
    o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
    const u32 result = o2_card_header_write(m.r8(dialog(m) + kCard), m.r8(dialog(m) + kSlot), 0);
    if (result == 0xFFFFFFFFu) advance(m, 2);
    if (result == 1) {
        o2_pilot_snapshot_store(); o2_pilot_name_store();
        o2_pilot_slot_reset(m.r8(dialog(m) + kSlot));
        o2_pilot_slot_reset(-1);
        advance(m);
    }
}

void Game::o2_save_file_pilot_step() {
    cover(0x800EA0C8u);
    Frame frame(*this, 24);
    if (m.r32(m.r32(kMenu) + 224) < 2)
        o2_pilot_slots_text(m.r8(dialog(m) + 10), static_cast<u32>(m.r16s(dialog(m) + 26)));
    const u32 result = o2_card_slot_write(m.r8(dialog(m) + kCard), m.r8(dialog(m) + kSlot), 0xFFFFFFFFu, 1);
    if (result == 0xFFFFFFFFu) advance(m);
    if (result == 1) restart(m, m.r32(m.r32(kMenu) + 224) - 2 < 3 ? 16u : 13u);
}
}
