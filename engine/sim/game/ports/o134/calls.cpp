#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call_o134(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    (void)a0; (void)a1; (void)a2; (void)a3; (void)arg; (void)out;
    if (m.r32(m.r32(g::kScene) + 56) == 3) {
        switch (fn) {
        case 0x8011B328u: out = o3_block_reset(a0); return true;
        case 0x8011B39Cu: out = o3_credits_setup(a0); return true;
        case 0x8011B690u: out = o3_pictures_upload(); return true;
        case 0x8011BB7Cu: out = o3_phase0(); return true;
        case 0x8011BD94u: out = o3_step(); return true;
        case 0x8011C05Cu: out = o3_player_open(a0, a1, a2, a3); return true;
        case 0x8011C100u: out = o3_player_reset(a0); return true;
        case 0x8011C260u: out = o3_player_request(); return true;
        case 0x8011C2A0u: out = o3_player_done(); return true;
        case 0x8011C2B4u: out = o3_player_stop(); return true;
        case 0x8011C2D8u: out = o3_player_first(); return true;
        case 0x8011C2F0u: out = o3_player_wait(a0); return true;
        case 0x8011C39Cu: out = o3_sector(a0); return true;
        case 0x8011CB5Cu: out = o3_frame(); return true;
        case 0x8011D474u: out = o3_cue(a0, a1, a2); return true;
        case 0x8011D59Cu: out = o3_movie_tiles(a0, a1, a2); return true;
        case 0x8011D8F4u: out = o3_trail(a0, a1, a2); return true;
        case 0x8011E474u: out = o3_credits(a0, a1, a2); return true;
        case 0x8011E744u: out = o3_phase1(); return true;
        case 0x8011E7FCu: out = o3_end(); return true;
        default: return false;
        }
    }
    if (m.r32(m.r32(g::kScene) + 56) != 1) return false;
    switch (fn) {
    case 0x800D0074u: out = o1_account_card_select(); return true;
    case 0x800CCFA4u: out = t1_unpack(); return true;
    case 0x800CD1A0u: t1_sprites_setup(); out = 0; return true;
    case 0x800CD2F0u: out = o1_text_sprites(a0, a1, a2, a3, arg(4), arg(5)); return true;
    case 0x800CD9A8u: t1_menu_init(); out = 0; return true;
    case 0x800CDB64u: t1_menu_run(); out = 0; return true;
    case 0x800CE290u: out = o1_transition_step(); return true;
    case 0x800CE5ACu: t1_name_run(); out = 0; return true;
    case 0x800CF06Cu: t1_sound_settings(); out = 0; return true;
    case 0x800CF7B4u: t1_screen_position(); out = 0; return true;
    case 0x800D181Cu: t1_new_run(); out = 0; return true;
    case 0x800D3B3Cu: out = o1_saved_account_run(); return true;
    case 0x800D4FF0u: t1_account_setup(a0, a1); out = 0; return true;
    case 0x800D5F6Cu: t1_card_poll(); out = 0; return true;
    case 0x800D9A08u: out = t1_logo(static_cast<s32>(a0), static_cast<s32>(a1), static_cast<s32>(a2), a3); return true;
    case 0x800DA9DCu: out = t1_clip_wipe() ? 1u : 0u; return true;
    case 0x800DACD0u: out = t1_clip_open() ? 1u : 0u; return true;
    case 0x800DAE44u: out = t1_op(0, a0, a1, a2); return true;
    case 0x800DB024u: out = t1_op(1, a0, a1, a2); return true;
    case 0x800DB254u: out = t1_op(2, a0, a1, a2); return true;
    case 0x800DB374u: out = t1_op(3, a0, a1, a2); return true;
    case 0x800DB834u: out = t1_op(4, a0, a1, a2); return true;
    case 0x800DB928u: out = t1_op(6, a0, a1, a2); return true;
    case 0x800DC098u: t1_new_flags(a0, a1); out = 0; return true;
    case 0x800DC6C4u: t1_screen(a0); out = 0; return true;
    case 0x800DC7ACu: t1_screens_scripts(); out = 0; return true;
    case 0x800DCAA4u: t1_screens_run(); out = 0; return true;
    case 0x800D0E54u: out = o1_account_pilot_select(); return true;
    case 0x800D1108u: out = o1_account_overwrite_choice(); return true;
    case 0x800D2FB8u: out = o1_saved_rank_choice(); return true;

    case 0x800D41C8u:
        t1_pilot_serialise(); out = m.r8(m.r32(g::kScene) + 2394) == 1
            ? m.r32(m.r32(m.r32(0x800DDA48u) + 760) + 1128) : 1; return true;
    case 0x800D4808u: out = o1_pilot_load(a0); return true;
    case 0x800D2994u: out = o1_saved_pilot_load(); return true;
    case 0x800D1300u: out = o1_account_directory_write_poll(); return true;
    case 0x800D13B8u: out = o1_account_pilot_write_poll(); return true;

    case 0x800CC938u: t1_ranks(a0); out = a0; return true;
    case 0x800CCB38u: out = t1_press_start(a0, a1); return true;
    case 0x800CCC90u: t1_title_init(); out = 0; return true;
    case 0x800CCD48u:
        t1_title_run(); out = (m.r32(m.r32(g::kScene) + 300) & 64u)
            ? m.r32(0x800DD9D0u) : m.r32(m.r32(0x800DD9D0u) + 120); return true;
    case 0x800CE3F4u: t1_pilot_reset(); out = m.r32(m.r32(g::kScene) + 2412); return true;
    case 0x800CEB90u: out = o1_name_choice(); return true;
    case 0x800CECF8u: t1_difficulty(); out = m.r32(m.r32(0x800DD9D0u) + 72); return true;
    case 0x800DC810u: t1_screens_setup(); out = 0; return true;

    case 0x800D4658u: out = o1_profile_pack(); return true;
    case 0x800D4DF4u: out = o1_profile_unpack(a0); return true;
    case 0x800D73B8u: out = o1_profile_write(a0, a1, a2, a3); return true;
    case 0x800D7644u: out = o1_kanji_setup(a0, a1); return true;
    case 0x800D7868u: out = o1_kanji_upload(a0); return true;
    case 0x800D7A58u: out = o1_kanji_strip(a0, a1, a2, a3, arg(4)); return true;

    case 0x800D5AC8u: out = o1_profile_time_text(a0); return true;
    case 0x800D2C84u: out = o1_profile_details(a0, a1); return true;
    case 0x800D3584u: out = o1_new_pilot_difficulty(); return true;
    case 0x800DA0E0u: out = o1_screenshot(a0); return true;
    case 0x800DC434u: t1_menu_text(); out = m.r32(0x800DDA58u); return true;
    case 0x800DC5ECu: t1_text_list(); out = 0; return true;

    case 0x800D94C4u: out = o1_gouraud_quad(a0, a1, a2); return true;
    case 0x800D9624u: out = o1_gouraud_triangle(a0, a1, a2); return true;
    case 0x800D9748u: out = o1_textured_quad(a0, a1, a2); return true;
    case 0x800DBB30u: out = o1_colour_bars(); return true;
    case 0x800CFA94u: out = o1_display_position_choice(); return true;
    case 0x800DBD20u: out = o1_pilot_name(a0, a1, a2); return true;
    case 0x800DBFE4u: out = o1_new_pilot_draw(); return true;

    case 0x800D82A4u: out = o1_glyph(a0); return true;
    case 0x800DC9ECu: out = o1_screens_frame_begin(a0); return true;
    case 0x800D80C0u: t1_prims_setup(); out = m.r32(0x800DDA58u); return true;
    case 0x800D8980u: {
        const s32 count = m.r32s(m.r32(0x800DDA58u) + 16);
        t1_text(a0); out = count <= 0 ? static_cast<u32>(count) : 0; return true;
    }
    case 0x800D8AA0u: t1_printf(a0, a1, a2, a3); out = 0; return true;
    case 0x800D7D08u:
        t1_prim_mode(static_cast<s32>(a0));
        switch (a0) {
        case 1: out = 20; break;
        case 2: out = 24; break;
        case 3: case 4: out = 4; break;
        case 5: case 6: out = 0x800D808Cu; break;
        case 7: out = m.r32(0x800DDA58u); break;
        default: out = 0; break;
        }
        return true;

    case 0x800D6AB4u: out = o1_card_profiles_read(a0, a1, a2); return true;
    case 0x800D29FCu: out = o1_save_profiles_read_poll(); return true;
    case 0x800D2E74u: out = o1_rank_menu_begin(); return true;
    case 0x800D3840u: out = o1_mission_disc_choice(); return true;
    case 0x800D1DCCu: out = o1_saved_card_choice(); return true;
    case 0x800D26F0u: out = o1_saved_pilot_choice(); return true;

    case 0x800D06D4u: out = o1_account_free_blocks_poll(); return true;
    case 0x800D0848u: out = o1_account_file_create_poll(); return true;
    case 0x800D08DCu: out = o1_account_header_write_poll(); return true;
    case 0x800D0978u: out = o1_account_empty_directory_write_poll(); return true;
    case 0x800D0B0Cu: out = o1_account_directory_read_poll(); return true;
    case 0x800D0BFCu: out = o1_account_pilots_read_poll(); return true;
    case 0x800D23CCu: out = o1_save_directory_read_poll(); return true;
    case 0x800D248Cu: out = o1_save_pilots_read_poll(); return true;
    case 0x800D64C4u: out = static_cast<u32>(t1_card_free(a0)); return true;
    case 0x800D6604u: out = static_cast<u32>(t1_save_read_dir(a0, a1)); return true;
    case 0x800D6814u: out = static_cast<u32>(t1_save_read_pilots(a0, a1)); return true;
    case 0x800D6D78u: out = static_cast<u32>(t1_save_header(a0, a1)); return true;
    case 0x800D6F04u: out = static_cast<u32>(t1_save_dir(a0, static_cast<s32>(a1), a2)); return true;
    case 0x800D713Cu: out = static_cast<u32>(t1_save_pilot(a0, a1, a2, a3)); return true;

    case 0x800D8AF0u: out = o1_resource_read(a0, a1, a2, a3); return true;
    case 0x800D8BA8u: out = o1_texture_set_upload(a0); return true;
    case 0x800D8C30u: out = o1_texture_directory_upload(a0); return true;
    case 0x800D8CECu: out = o1_texture_upload(a0); return true;
    case 0x800D8D64u: out = o1_texture_upload_at(a0, a1, a2, a3, arg(4)); return true;
    case 0x800D8E38u: out = o1_texture_palette_upload(a0, a1, a2); return true;
    case 0x800D8F3Cu: out = o1_draw_mode_packet(a0, a1, a2); return true;
    case 0x800DA520u: out = o1_flow_disc_check(); return true;

    case 0x800D076Cu: out = o1_account_empty_panel_choice(); return true;
    case 0x800D0A10u: out = o1_account_saved_panel_choice(); return true;
    case 0x800D0CECu: out = o1_account_alternate_panel_choice(); return true;
    case 0x800CFEA4u: out = o1_title_missing_account_choice(); return true;
    case 0x800D1B2Cu: out = o1_unavailable_card_menu_begin(); return true;
    case 0x800CFF90u: out = o1_title_saved_card_panel_begin(); return true;
    case 0x800D2600u: out = o1_save_progress_panel_begin(); return true;
    case 0x800D2BA0u: out = o1_mission_feature_complete(); return true;
    case 0x800D1618u: out = o1_disc_choice_poll(); return true;

    case 0x800CFC94u: out = o1_title_new_account_choice(); return true;
    case 0x800CEF44u: out = o1_title_button_choice(); return true;
    case 0x800D1488u: out = o1_account_create_choice(); return true;
    case 0x800D1CF0u: out = o1_saved_account_chooser_begin(); return true;
    case 0x800D1C18u: out = o1_cards_unavailable_poll(); return true;
    case 0x800D0548u: out = o1_account_format_choice(); return true;
    case 0x800D2AE8u: out = o1_save_dialog_choice(); return true;
    case 0x800CFDCCu: out = o1_title_unavailable_cards_begin(); return true;
    case 0x800D0384u: out = o1_account_missing_panel_choice(); return true;
    case 0x800D0460u: out = o1_account_unformatted_panel_begin(); return true;
    case 0x800D4F0Cu: t1_cards_alloc(); out = 0; return true;
    case 0x800D56E0u: out = o1_card_events_open(); return true;
    case 0x800D5550u: t1_cards_reset(); out = 0; return true;
    case 0x800D5880u: out = o1_cards_initialise(); return true;
    case 0x800D5EC4u: out = o1_card_file_create(a0); return true;
    case 0x800D3EA8u: out = t1_card_wait(4, a0, a1); return true;
    case 0x800D4060u: out = t1_card_wait(20, a0, a1); return true;
    case 0x800D5E74u: out = o1_card_format(a0); return true;
    case 0x800D061Cu: out = o1_account_card_format_poll(); return true;
    case 0x800D90E8u: out = o1_double_line(a0, a1, a2); return true;
    case 0x800D9140u: out = o1_polyline3(a0, a1, a2); return true;
    case 0x800D9230u: out = o1_double_polyline3(a0, a1, a2); return true;
    case 0x800D9294u: out = o1_polyline4(a0, a1, a2); return true;
    case 0x800D939Cu: out = o1_double_polyline4(a0, a1, a2); return true;
    case 0x800D9400u: out = o1_tile_packet(a0, a1, a2); return true;
    case 0x800D81F0u: out = o1_glyph_frame_begin(a0); return true;
    case 0x800DA2A8u: out = t1_decoded(static_cast<s32>(a0)); return true;
    case 0x800DA340u: out = t1_level(); return true;
    case 0x800D5DB4u: out = o1_card_matching_slot(); return true;
    case 0x800DA25Cu: t1_queue_setup(); out = m.r32(0x800DDA60u); return true;
    case 0x800CF750u: out = o1_title_panel15_begin(); return true;
    case 0x800CFA34u: out = o1_title_panel16_begin(); return true;
    case 0x800CF004u: out = o1_title_panel14_begin(); return true;
    case 0x800DA7A8u: out = o1_text_item_reverse(a0); return true;
    case 0x800CCE38u: out = dir_pointers(a0, a1); return true;
    case 0x800CEB20u: out = o1_title_panel12_begin(); return true;
    case 0x800CEECCu: out = o1_title_panel13_begin(); return true;
    case 0x800CFBD0u: out = o1_title_panel_choice_poll(); return true;
    case 0x800DA418u: out = o1_animation_setup(a0); return true;
    case 0x800CEC70u: out = o1_title_panel10_begin(); return true;
    case 0x800D1080u: out = o1_account_panel21_begin(); return true;
    case 0x800CE518u: out = o1_title_panel11_begin(); return true;
    case 0x800D0DC8u: out = o1_account_panel20_begin(); return true;
    case 0x800D02DCu: out = o1_account_missing_panel_begin(); return true;
    case 0x800CFB2Cu: out = o1_title_panel17_begin(); return true;
    case 0x800D255Cu: out = o1_card_save_prompt_poll(); return true;
    case 0x800DA1B8u: t1_stats_reset(); out = 0; return true;
    case 0x800DA8E0u: t1_fades(); out = 0; return true;
    case 0x800D9024u: out = o1_line_packet(a0, a1, a2); return true;
    case 0x800DBF6Cu: out = o1_marker_set(a0, a1, a2, a3); return true;
    case 0x800DC028u: out = o1_marker_mode(a0); return true;
    case 0x800D16D4u: out = o1_drive_prompt_begin(); return true;
    case 0x800D15C4u: out = o1_account_marker_begin(); return true;
    case 0x800CFD60u: out = o1_title_cards_poll_finish(); return true;
    case 0x800D1738u: out = o1_drive_prompt_poll(); return true;
    case 0x800D1798u: out = o1_account_cards_begin(); return true;
    case 0x800D3A24u: out = o1_account_drive_prompt_begin(); return true;
    case 0x800D1A5Cu: out = o1_card_selection_fade_begin(); return true;
    case 0x800D34FCu: out = o1_account_confirmation_begin(); return true;
    case 0x800D37FCu: out = o1_account_marker_reset(); return true;
    case 0x800D3AE8u: out = o1_account_exit_panel_begin(); return true;
    case 0x800D2154u: out = o1_card_format_prompt_begin(); return true;
    case 0x800D2268u: out = o1_card_empty_prompt_begin(); return true;
    case 0x800D202Cu: out = o1_card_missing_prompt_begin(); return true;
    case 0x800D20BCu: out = o1_card_missing_prompt_poll(); return true;
    case 0x800D21D0u: out = o1_card_format_prompt_poll(); return true;
    case 0x800D22E4u: out = o1_card_empty_prompt_poll(); return true;
    case 0x800D1554u: out = o1_account_timeout_poll(); return true;
    case 0x800D0668u: out = o1_card_formatted_poll(); return true;
    case 0x800D3D90u: out = t1_card_status(a0); return true;
    case 0x800D3CE8u: t1_card_flags_reset(); out = 1; return true;
    case 0x800D51FCu: out = o1_card_directory_reset(); return true;
    case 0x800D5290u: t1_card_states_reset(); out = 0; return true;
    case 0x800D5270u: out = o1_card_directory_reset_callback(); return true;
    case 0x800D5528u: out = o1_card_saves_reset_callback(); return true;
    case 0x800D3F88u: out = o1_card_software_events_drain(); return true;
    case 0x800D3FF4u: out = o1_card_hardware_events_drain(); return true;
    case 0x800D4D6Cu: out = o1_card_save_name_commit(a0); return true;
    case 0x800D1ADCu: out = o1_card_initial_poll_finish(); return true;
    case 0x800D237Cu: out = o1_card_creation_begin(); return true;
    case 0x800D58B0u: out = o1_card_events_close(); return true;
    case 0x800D7600u: out = o1_account_exit_poll(); return true;
    case 0x800DA9A8u: out = o1_animation_command(a0, a1); return true;
    case 0x800D5A38u: out = o1_card_operation_bit(a0); return true;
    case 0x800DAC98u: out = o1_clip_fade_done(); return true;
    case 0x800CFD28u: out = o1_title_animation_begin(); return true;
    case 0x800D4158u: out = o1_save_name_to_scene(); return true;
    case 0x800DA49Cu: out = o1_animation_history_push(a0); return true;
    case 0x800D2954u: out = o1_account_command_begin(); return true;
    case 0x800CD930u: out = o1_resource_allocate(a0); return true;
    case 0x800D5A70u: out = o1_card_account_type(a0); return true;
    case 0x800D5BF4u: out = o1_card_secondary_type(a0); return true;
    case 0x800CE24Cu: out = o1_title_selection_begin(); return true;
    case 0x800DA6B8u: out = o1_drive_transition(); return true;
    case 0x800CFC38u: out = o1_title_attract_begin(); return true;
    case 0x800D5970u: out = o1_save_name_address(a0); return true;
    case 0x800D7C88u: out = o1_text_style(a0); return true;
    case 0x800D12A4u: out = o1_account_selection_begin(); return true;
    case 0x800DA838u: out = o1_fade_set(a0, a1, a2, a3); return true;
    case 0x800DA714u: out = o1_text_item_set(a0, a1, a2, a3, arg(4)); return true;
    case 0x800DA77Cu: out = o1_text_item_enable(a0); return true;
    case 0x800D3A8Cu: out = o1_account_drive_transition(); return true;
    case 0x800D5548u: o1_empty_5548(); out = r[2]; return true;
    case 0x800DA9D4u: out = o1_draw_complete(); return true;
    case 0x800D7CD4u: out = o1_text_fold_position(a0); return true;
    case 0x800DA4F4u: out = o1_flow_auxiliary(a0); return true;
    case 0x800DA828u: out = o1_screen_text_select(a0); return true;
    case 0x800D4140u: out = t1_clock(); return true;
    case 0x800DA4DCu: out = t1_flow(); return true;
    case 0x800DA6FCu: out = o1_animation_table(); return true;
    case 0x800DB910u: out = o1_clip_select(a0); return true;
    case 0x800D0AECu: out = o1_menu_timer_advance(); return true;
    case 0x800D5C80u: out = o1_card_operation_status(); return true;
    case 0x800DA484u: out = o1_menu_delay(); return true;
    case 0x800D75E4u: out = o1_account_exit_begin(); return true;
    case 0x800D7C40u: out = o1_text_origin_set(a0, a1); return true;
    case 0x800D7CE4u: out = o1_text_fold_set(a0, a1, a2); return true;
    case 0x800D7C60u: out = o1_text_rgb_set(a0, a1, a2); return true;
    case 0x800D8094u: out = o1_text_origin_get(a0, a1); return true;
    case 0x800DA898u: out = t1_fade_state(static_cast<s32>(a0)); return true;
    case 0x800DA8BCu: out = t1_fade_value(static_cast<s32>(a0)); return true;
    case 0x800D3D60u: out = t1_cards_idle(); return true;
    case 0x800D5D54u: out = o1_account_byte_set(a0); return true;
    case 0x800D5D6Cu: out = o1_save_byte_45c_set(a0); return true;
    case 0x800D5D84u: out = o1_save_byte_45d_set(a0); return true;
    case 0x800D5D9Cu: out = o1_save_byte_45e_set(a0); return true;
    case 0x800D5AA8u: out = o1_account_byte(); return true;
    case 0x800D5CA0u: out = o1_save_flags(); return true;
    case 0x800DA504u: out = o1_flow_clear(); return true;
    case 0x800CD914u: out = o1_resource_arena_set(a0); return true;
    case 0x800DA694u: out = o1_drive_ready(); return true;
    case 0x800D783Cu: out = o1_glyph_rom_address(a0); return true;
    case 0x800D75B4u: out = o1_save_name_set(a0); return true;
    case 0x800D7C18u: out = o1_primitive_buffer_select(); return true;
    case 0x800DA800u: out = o1_text_item_field(a0); return true;
    case 0x800CD978u: out = o1_resource_release(a0); return true;
    case 0x800D4190u: out = o1_save_scene_name(); return true;
    case 0x800D5CC0u: out = o1_save_route(a0); return true;
    case 0x800D59C0u: out = o1_save_flag_test(a0, a1); return true;
    case 0x800CD8C8u: out = o1_resource_buffer_select(); return true;
    case 0x800D5C2Cu: out = o1_card_profile_type(a0); return true;
    case 0x800D47B8u: out = o1_save_slot_load(a0); return true;
    case 0x800CD968u: out = o1_resource_handle(); return true;
    case 0x800D4D54u: out = o1_card_operation_clear(); return true;
    case 0x800D5870u: out = o1_card_callback_clear(); return true;
    case 0x800D5A20u: out = o1_card_cursor(); return true;
    case 0x800D5C74u: out = o1_save_pointer_address(); return true;
    case 0x800D5CF4u: out = o1_save_byte_45c(); return true;
    case 0x800D5D0Cu: out = o1_save_byte_45d(); return true;
    case 0x800D5D24u: out = o1_save_byte_45e(); return true;
    case 0x800D5D3Cu: out = o1_save_byte_460(); return true;
    case 0x800D5968u: o1_empty_5968(); out = r[2]; return true;
    case 0x800D8F34u: o1_empty_8f34(); out = r[2]; return true;
    case 0x800DC424u: o1_empty_c424(); out = r[2]; return true;
    case 0x800DC42Cu: o1_empty_c42c(); out = r[2]; return true;
    case 0x800DC5E4u: o1_empty_c5e4(); out = r[2]; return true;
    case 0x800DC694u: o1_empty_c694(); out = r[2]; return true;
    case 0x800DC69Cu: o1_empty_c69c(); out = r[2]; return true;
    case 0x800DC6A4u: o1_empty_c6a4(); out = r[2]; return true;
    case 0x800DC6ACu: o1_empty_c6ac(); out = r[2]; return true;
    case 0x800DC6B4u: o1_empty_c6b4(); out = r[2]; return true;
    case 0x800DC6BCu: o1_empty_c6bc(); out = r[2]; return true;
    default: return false;
    }
}

bool Game::port_phase_o134(u32 fn, bool run) {
    if (m.r32(m.r32(g::kScene) + 56) != 3) return false;
    switch (fn) {
    case 0x8011BB7Cu: if (run) phase_result = o3_phase0(); return true;
    case 0x8011E744u: if (run) phase_result = o3_phase1(); return true;
    default: return false;
    }
}
}
