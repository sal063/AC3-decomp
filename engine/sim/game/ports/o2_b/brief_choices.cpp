#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kBriefing = 0x80112DB0u, kMenu = 0x80112DC4u;
constexpr u32 kChoice = 5808, kSide = 5809, kPosition = 5810;
constexpr u32 kChoiceTable = 0x8010B388u, kPageTable = 0x8010B400u;
u32 brief(Memory& m) { return m.r32(kBriefing); }
void link(Memory& m, u32 packet, u32 ot) {
    m.w32(packet, (m.r32(packet) & 0xFF000000u) | (m.r32(ot) & 0xFFFFFFu));
    m.w32(ot, (m.r32(ot) & 0xFF000000u) | (packet & 0xFFFFFFu));
}
u32 choice_field(Memory& m, u32 side) {
    return kChoiceTable + 20 * m.r8(brief(m) + kChoice)
        + 2 * static_cast<u32>(m.r16s(brief(m) + kPosition)) + 4 + side;
}
template<class DrawMode>
void choice_tick(Game& game, Memory& m, u32 stackWord, bool finalPage, DrawMode&& drawMode) {
    const u32 buffer = m.r32(m.r32(g::kScene) + 4), state = brief(m);
    const u32 ot = m.r32(m.r32(kMenu) + 8), modes = state + 6252 + 24 * buffer;
    link(m, modes, ot);
    const s32 page = m.r16s(m.r32(state + 6300));
    if (finalPage || m.r8(kPageTable + 8 * static_cast<u32>(page) + 6) != 8) {
        for (u32 sprite = 0; sprite < 11; ++sprite) {
            const u32 packet = state + 5812 + 220 * buffer + 20 * sprite;
            const s32 index = m.r16s(state + kPosition);
            m.w16(packet + 14, (static_cast<u32>(index % 4 + 296) << 6) | 22);
            link(m, packet, ot);
        }
        m.w32(stackWord, 0);
        const s32 half = m.r16s(brief(m) + kPosition) / 2;
        const u32 pageWord = ((static_cast<u32>((half % 2) * 64 + 576) & 1023) >> 6) | 16;
        drawMode(modes + 12, pageWord);
        link(m, modes + 12, ot);
    }
    if (!game.o2_fade_state(5)) {
        const u32 current = brief(m), choice = m.r8(current + kChoice);
        const s32 end = static_cast<s32>(m.r32(kChoiceTable + 20 * choice) - (finalPage ? 1u : 2u));
        if (m.r16s(current + kPosition) < end) {
            m.w8(current + kSide, 0);
            const u32 next = brief(m); m.w16(next + kPosition, m.r16(next + kPosition) + 1);
            game.o2_fade_set(5, 1, 0, 10);
        } else m.w32(m.r32(kMenu) + 296, 1);
    }
    for (u32 side = 0; side < 2; ++side) {
        game.o2_flag(side + 1, m.r8(choice_field(m, side)));
        if (m.r8(choice_field(m, side)) && !game.o2_fade_state(side + 3)) {
            if (!m.r32(m.r32(kMenu) + 296)) game.o2_fade_set(side + 3, 1, 0, 10);
            m.w8(brief(m) + kSide, side + 1);
        }
    }
    game.o2_flag(3, m.r8(brief(m) + kSide));
    if (m.r32(m.r32(g::kScene) + 300) & 16) {
        if (game.o2_fade_state(3) == 2) game.o2_fade_set(3, 3, -1, -1);
        if (game.o2_fade_state(4) == 2) game.o2_fade_set(4, 3, -1, -1);
        game.o2_fade_set(5, 3, -1, -1);
    }
}
}

void Game::o2_brief_choices_tick() {
    cover(0x800EE0D8u);
    Frame frame(*this, 40);
    choice_tick(*this, m, frame.at(16), false,
        [this](u32 packet, u32 page) { set_draw_mode(packet, 0, 0, page, 0); });
}

void Game::o2_brief_choices_final_tick() {
    cover(0x800EE570u);
    Frame frame(*this, 40);
    choice_tick(*this, m, frame.at(16), true,
        [this](u32 packet, u32 page) { set_draw_mode(packet, 0, 0, page, 0); });
}

void Game::o2_brief_script_tick() {
    cover(0x800EDBCCu);
    Frame frame(*this, 32);
    for (;;) {
        const u32 state = brief(m), command = m.r32(state + 20);
        const s32 due = m.r16s(command + 2);
        const u32 opcode = static_cast<u32>(m.r16s(command));
        if (m.r32s(state + 16) < due) return;
        m.w32(state + 20, command + 4);
        switch (opcode) {
        case 0: o2_brief_animation_reset(); return;
        case 1: o2_brief_image_command(); break;
        case 2: o2_brief_image_style_command(); break;
        case 3: o2_brief_image_expand_command(); break;
        case 4: o2_brief_image_collapse_command(); break;
        case 5: o2_brief_image_close(); break;
        case 6: o2_brief_panel_texture_command(0x80110000u); break;
        case 7: o2_brief_panel_expand_command(); break;
        case 8: o2_brief_panel_move_command(); break;
        case 9: o2_brief_panel_collapse_command(); break;
        case 10: o2_brief_panel_cel_command(); break;
        case 11: o2_brief_panel_flipbook_command(); break;
        case 12: o2_brief_image_layout_command(0x800EDC3Cu); break;
        default: call_guest(m.r32(0x8010B980u + 4 * opcode), static_cast<u32>(due), 0x80110000u); break;
        }
    }
}
}
