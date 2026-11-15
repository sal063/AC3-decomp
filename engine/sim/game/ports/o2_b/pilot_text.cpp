#include "sim/game/game.hpp"

namespace ac3::sim {
namespace {
constexpr u32 kDialog = 0x80112DF8u;
constexpr u32 kEmptySlot = 0x800CCE40u, kEmptyRecord = 0x800CCE48u;
}

u32 Game::o2_pilot_slots_text(u32 excludeSelected, u32 layout) {
    cover(0x800E6A6Cu);
    Frame frame(*this, 40);
    u32 occupied = o2_pilot_slot_mask();
    if (excludeSelected == 1) occupied &= ~(1u << (m.r8(m.r32(kDialog) + 5) & 31));
    o2_text_font(4);
    o2_text_semi(2);
    o2_text_fold(1, 10, 20);
    o2_text_frame_mode(static_cast<u32>(o2_fade_value(15)));
    u32 result = 0;
    for (u32 row = 0; row < 3; ++row) {
        if (layout == 0 || layout == 1) o2_text_at(layout == 0 ? 81u : 56u, 214 + 53 * row);
        const bool present = (occupied & (1u << row)) != 0;
        const u32 brightness = present ? 128u : 64u;
        o2_text_rgb(brightness, brightness, brightness);
        result = o2_text_printf(present ? o2_pilot_name(row + 1) : kEmptySlot, 0, 0, 0);
    }
    return result;
}

u32 Game::o2_pilot_record_text(u32 index, u32 slot, u32 unavailable) {
    cover(0x800E6C4Cu);
    Frame frame(*this, 40);
    const u32 progress = o2_pilot_progress();
    o2_text_font(4);
    o2_text_semi(2);
    o2_text_fold(1, 10, 20);
    o2_text_frame_mode(static_cast<u32>(o2_fade_value(15)));
    o2_text_rgb(128, 128, 128);
    o2_text_font(3);
    const u32 record = index & 255;
    if (unavailable == 1 || !(progress & (1u << (record & 31)))) {
        o2_text_at(140, 331);
        return o2_text_printf(kEmptyRecord, 0, 0, 0);
    }
    o2_text_at(94, 294);
    o2_text_printf(o2_pilot_record_label(record), 0, 0, 0);
    o2_text_at(94, 317); o2_text_printf(0x800CCE50u, 0, 0, 0);
    o2_text_at(140, 317); o2_text_printf(o2_pilot_record_name(record), 0, 0, 0);
    o2_text_at(94, 334); o2_text_printf(0x800CCE58u, 0, 0, 0);
    o2_text_at(140, 334); o2_text_printf(o2_pilot_time_text(record), 0, 0, 0);
    o2_text_at(94, 352); o2_text_printf(0x800CCE60u, 0, 0, 0);
    o2_text_at(140, 353); o2_text_printf(o2_pilot_record_category(record), 0, 0, 0);
    o2_text_at(94, 369); o2_text_printf(0x800CCE68u, 0, 0, 0);
    o2_text_at(140, 369);
    return o2_text_printf(o2_pilot_ending_label(slot & 255), 0, 0, 0);
}
u32 Game::o2_pilot_selection_text(u32 slot, u32 index) {
    cover(0x800E1F94u);
    Frame frame(*this, 32);
    o2_text_font(4); o2_text_semi(2); o2_text_fold(1, 10, 20);
    o2_text_frame_mode(static_cast<u32>(o2_fade_value(15)));
    o2_text_rgb(128, 128, 128); o2_text_font(3);
    const u32 record = index & 255;
    if (!o2_pilot_progress_bit(slot & 255, record)) {
        o2_text_at(140, 321); return o2_text_printf(0x800CC934u, 0, 0, 0);
    }
    o2_text_at(96, 284); o2_text_printf(o2_pilot_record_label(record), 0, 0, 0);
    o2_text_at(96, 309); o2_text_printf(0x800CC914u, 0, 0, 0);
    o2_text_at(142, 309); o2_text_printf(o2_pilot_record_name(record), 0, 0, 0);
    o2_text_at(96, 325); o2_text_printf(0x800CC91Cu, 0, 0, 0);
    o2_text_at(142, 325); o2_text_printf(o2_pilot_time_text(record), 0, 0, 0);
    o2_text_at(96, 341); o2_text_printf(0x800CC924u, 0, 0, 0);
    o2_text_at(142, 341); o2_text_printf(o2_pilot_record_category(record), 0, 0, 0);
    o2_text_at(96, 357); o2_text_printf(0x800CC92Cu, 0, 0, 0);
    o2_text_at(142, 357); return o2_text_printf(o2_pilot_ending_label(record), 0, 0, 0);
}
}
