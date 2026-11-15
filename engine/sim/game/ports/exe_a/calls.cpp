#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call_exe_a(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    auto s = [](u32 v) { return static_cast<s32>(v); };
    switch (fn) {
    case 0x80025BC8u: display_env_update(); return true;
    case 0x80026050u: buffers_flip(); return true;
    case 0x800260C0u: frame_present(); return true;
    case 0x80026628u: out = pad_id_lookup(a0); return true;
    case 0x8002685Cu: out = pad_setup(a0); return true;
    case 0x80026BA4u: pad_parse(a0, a1); return true;
    case 0x80026E84u: pad_poll(); return true;
    case 0x800272F0u: out = ld_texture_set(a0, a1); return true;
    case 0x800273FCu: mem_heap_reset(a0); return true;
    case 0x80027584u: out = heap_free(); return true;
    case 0x80027608u: out = debug_text(a0, a1, a2); return true;
    case 0x800276C8u: out = debug_number(a0, a1, s(a2), a3); return true;
    case 0x800277F0u: out = debug_hex(a0, a1, a2, s(a3)); return true;
    case 0x800278A8u: out = overlay_packets(a0, a1); return true;
    case 0x8002895Cu: axes_calibrate(a0, a1); return true;
    case 0x80028C24u: input_map(a0, a1, a2); return true;
    case 0x8002937Cu: rumble_clear(); return true;
    case 0x8002939Cu: out = hud_alert_take(a0); return true;
    case 0x800293F0u: hud_alert_speed(s(a0)); return true;
    case 0x80029448u: hud_alert_launch(a0); return true;
    case 0x800294C4u: hud_alert_gun(a0); return true;
    case 0x80029588u: hud_alert_damage(); return true;
    case 0x80029734u: rumble_update(a0); return true;
    case 0x8002995Cu: out = kcd_entry(a0); return true;
    case 0x8002999Cu: kcd_cd_mix(a0); return true;
    case 0x800299D8u: kcd_cd_volume(s(a0), a1); return true;
    case 0x80029BD8u: kcd_stream_reader(); return true;
    case 0x80029DFCu: kcd_seek(); return true;
    case 0x8002A2C0u: kcd_frame(); return true;
    case 0x8002A79Cu: out = static_cast<u32>(kcd_request_start(a0, a1, a2, a3, arg(4))); return true;
    case 0x8002ABE0u: out = static_cast<u32>(kcd_pause(a0)); return true;
    case 0x8002AC3Cu: out = kcd_stop(a0); return true;
    case 0x8002ACD4u: out = static_cast<u32>(kcd_find_files(a0, s(a1), a2, a3)); return true;
    case 0x8002AD90u: kcd_read_blocking(a0, a1, a2); return true;
    case 0x8002AE0Cu: out = static_cast<u32>(kcd_read_index()); return true;
    case 0x8002B094u: out = static_cast<u32>(kcd_spin_up(a0)); return true;
    case 0x8002B194u: out = static_cast<u32>(kcd_spin_ready(a0)); return true;
    case 0x8002B428u: kcd_open_drive(); return true;
    case 0x8002B504u: out = static_cast<u32>(disc_number()); return true;
    case 0x8002B528u: out = kcd_drive_state(); return true;
    case 0x8002B5B0u: out = stream_face_record(); return true;
    case 0x8002B5FCu: out = stream_position(); return true;
    case 0x8002B610u: kcd_ring_init(a0, s(a1), a2); return true;
    case 0x8002B6D0u: kcd_ring_clear(); return true;
    case 0x8002B760u: kcd_ring_put(); return true;
    case 0x8002BEE4u: out = kcd_cmd_voice(a0); return true;
    case 0x8002C2E4u: out = kcd_cmd_fade_stop(a0); return true;
    case 0x8002C45Cu: out = kcd_run(a0); return true;
    case 0x8002C5E0u: out = ld_record_size(a0); return true;
    case 0x8002C7ACu: out = ld_request(a0, a1, a2); return true;
    case 0x8002C8B8u: out = ld_request_record(a0, a1, a2); return true;
    case 0x8002C980u: out = stream_face_word(a0); return true;
    case 0x8002CAD0u: out = cd_current(); return true;
    case 0x8002CB58u: out = static_cast<u32>(kcd_stream_on_this_disc(a0)); return true;
    case 0x8002CD24u: face_animate(); return true;
    case 0x8002CE70u: out = face_sprites(a0, a1); return true;
    case 0x8002D10Cu: out = face_quads(a0, a1); return true;
    case 0x8002D330u: out = face_copy(a0, a1); return true;
    case 0x8002D544u: out = radio_face_busy(); return true;
    case 0x8002D560u: out = face_label(a0, a1, a2); return true;
    case 0x8002D80Cu: snd_master_volume(a0); return true;
    case 0x8002D844u: snd_cd_volume(a0); return true;
    case 0x8002D910u: snd_spu_upload(a0, a1, a2); return true;
    case 0x8002DAF4u: reverb_off(); return true;
    case 0x8002DBE0u: out = voice_for_repeat(a0); return true;
    case 0x8002DC7Cu: out = voice_start_on(a0, a1); return true;
    case 0x8002DCB0u: out = sound_play_free(a0); return true;
    case 0x8002DCF8u: out = voice_find_playing(a0); return true;
    case 0x8002DD38u: out = voice_find_free(); return true;
    case 0x8002DD7Cu: out = voice_playing(a0); return true;
    case 0x8002DDE0u: voices_off(); return true;
    case 0x8002DEC8u: out = voice_volume(a0, s(a1), s(a2)); return true;
    case 0x8002DF98u: snd_voice_update(a0); return true;
    case 0x8002E0D0u: snd_voice_start(a0); return true;
    case 0x8002E1F4u: sound_driver(); return true;
    case 0x8002E514u: out = voice_start(a0, a1); return true;
    case 0x8002E5B4u: voice_clear(a0); return true;
    case 0x8002E64Cu: out = static_cast<u32>(pitch_add(s(a0), s(a1))); return true;
    case 0x8002E67Cu: voice_reset_mix(a0); return true;
    case 0x8002E6DCu: sound_listener(a0, a1, a2); return true;
    case 0x8002E834u: fade_reset(a0); return true;
    case 0x8002E84Cu: out = sound_attenuate(a0, a1, a2); return true;
    case 0x8002E8DCu: out = sound_pan(a0, a1); return true;
    case 0x8002EA24u: out = sound_side(a0, s(a1)); return true;
    case 0x8002EA74u: out = stereo_split(a0, a1, a2); return true;
    case 0x8002EC48u: out = sound_height(a0, a1); return true;
    case 0x8002ED08u: alert_init(a0, a1, a2, a3, arg(4)); return true;
    case 0x8002ED2Cu: out = alert_sound(a0, a1); return true;
    case 0x8002EE7Cu: mod_init(a0, a1); return true;
    case 0x8002EE88u: mod_volume(a0, a1, a2); return true;
    case 0x8002EEA8u: mod_pitch(a0, a1, a2); return true;
    case 0x8002EEC8u: out = modulate(a0); return true;
    case 0x8002F0D4u: out = sound_detune(a0, s(a1)); return true;
    case 0x8002F178u: snd_delayed_clear(a0); return true;
    case 0x8002F19Cu: out = static_cast<u32>(cue_add(a0, s(a1))); return true;
    case 0x8002F1E0u: snd_delayed_update(); return true;
    case 0x8002F294u: sound_queue_push(a0, a1); return true;
    case 0x8002F2CCu: snd_queue_update(); return true;
    case 0x8002F3C0u: out = sound_at(a0, a1, a2); return true;
    case 0x8002F460u: out = sound_at_repeat(a0, a1, a2); return true;
    case 0x8002F500u: alert_channels_open(a0); return true;
    case 0x8002F620u: alert_channels_stop(); return true;
    case 0x8002F70Cu: engine_set_kind(a0, a1); return true;
    case 0x8002F770u: engine_assign(a0, a1); return true;
    case 0x8002F9ACu: engine_mix(a0); return true;
    case 0x800300E0u: vol_slew(a0, a1, a2, s(a3)); return true;
    case 0x80030178u: alert_levels_reset(); return true;
    case 0x800301A8u: engine_voices_stop(); return true;
    case 0x800301F0u: engine_sound(a0); return true;
    case 0x80030464u: alert_channels_init(); return true;
    case 0x8003048Cu: sound_reset(); return true;
    case 0x800304CCu: alert_channels_stop_frame(); return true;
    case 0x80030A68u: rot_matrix_yxz_14(a0, a1); return true;
    case 0x80030BDCu: rot_matrix_xzy_14(a0, a1); return true;
    case 0x80030D54u: out = static_cast<u32>(ratan2(s(a0), s(a1))); return true;
    case 0x80030EB8u: out = static_cast<u32>(asin_table(s(a0))); return true;
    case 0x80030EF8u: rotate_by_angles(a0, a1, a2); return true;
    case 0x800313A8u: attitude_integrate_to(a0, a1, a2); return true;
    case 0x800317F0u: out = static_cast<u32>(aim_angles(a0, a1, a2)); return true;
    case 0x800319C8u: out = static_cast<u32>(steer_to(a0, a1, a2)); return true;
    case 0x80031BB4u: vec_to_angles(a0, a1); return true;
    case 0x80031CD8u: vector_angles(a0, a1); return true;
    case 0x80031E64u: phase_enter_intro(a0); return true;
    case 0x80031EA4u: phase_enter_16(a0); return true;
    case 0x80031F24u: phase_enter_restart(a0); return true;
    case 0x80031F64u: phase_enter_4000(a0); return true;
    case 0x80032080u: out = phase_change(a0, a1); return true;
    case 0x800323F8u: display_buffers_alloc_recorded(a0); return true;
    case 0x800324ACu: display_buffers_alloc(a0); return true;
    case 0x80032518u: mission_view_limits(a0); return true;
    case 0x80032F18u: request_exit(a0, a1); return true;
    case 0x800332E4u: view_matrices(a0, a1, a2); return true;
    case 0x80033DC0u: out = world_frame(); return true;
    case 0x80034360u: out = pause_text(a0, a1, a2, a3, arg(4), arg(5)); return true;
    case 0x800343C8u: out = pause_box(a0, a1, a2, a3); return true;
    case 0x800346C0u: out = pause_mode(a0, a1, a2); return true;
    case 0x80034718u: pause_menu(); return true;
    case 0x80034930u: out = pause_draw(a0, a1); return true;
    case 0x80035044u: continue_menu_input(); return true;
    case 0x80035184u: out = continue_menu_draw(a0, a1); return true;
    case 0x8003545Cu: out = replay_caption(a0, a1); return true;
    case 0x80035578u: out = replay_banner(a0, a1, s(a2)); return true;
    case 0x8003563Cu: out = text_line(a0, a1, a2, a3, arg(4), arg(5)); return true;
    case 0x80035740u: view_flag(a0); return true;
    case 0x80036130u: demo_next_mission(); return true;
    case 0x800362A0u: out = loading_clear(a0, a1, s(a2)); return true;
    case 0x800363B0u: out = vram_copy_packet(a0, a1, a2, a3, arg(4), arg(5), arg(6), arg(7)); return true;
    case 0x80036440u: out = loading_copy(a0, a1); return true;
    case 0x800364C4u: out = loading_stars(a0, a1); return true;
    case 0x800365FCu: out = loading_bars(a0, a1, s(a2), a3, arg(4)); return true;
    case 0x80036824u: out = loading_text(a0, a1); return true;
    case 0x800369C8u: loading_reset(); return true;
    case 0x80036A84u: out = loading_panel(a0, a1); return true;
    case 0x80036E84u: out = loading_zoom(a0, a1); return true;
    case 0x80037070u: loading_start(); return true;
    case 0x80037108u: loading_draw(); return true;
    case 0x800371C4u: mission_mode_update(); return true;
    case 0x80037E50u: mission_end(); return true;
    case 0x800257E8u: flow_globals_init(); return true;
    case 0x80025B00u: hw_init(); return true;
    case 0x80025DDCu: out = display_init(); return true;
    case 0x80025E38u: out = volumes_init(); return true;
    case 0x80025EA0u: boot_record_load(); return true;
    case 0x80025F68u: flow_init(); return true;
    case 0x8002654Cu: out = pads_init(a0, a1, a2, a3); return true;
    case 0x800275C8u: debug_text_reset(); return true;
    case 0x800287BCu: copy_halfwords_32(a0, a1); return true;
    case 0x800287E4u: out = copy_words_6(a0, a1); return true;
    case 0x80028848u: out = input_config_default(a0, a1); return true;
    case 0x80028BC0u: input_configs_default(a0); return true;
    case 0x8002B034u: out = static_cast<u32>(kcd_init(a0)); return true;
    case 0x8002BCD8u: kcd_requests_init(); return true;
    case 0x8002D75Cu: sound_init(); return true;
    case 0x8002D79Cu: snd_volumes_set(a0); return true;
    case 0x8002D880u: snd_cd_mix(a0); return true;
    case 0x8002DA20u: out = reverb_setup(); return true;
    case 0x8002E604u: voices_reset(); return true;
    case 0x80027C24u: out = text_put(a0, a1); return true;
    case 0x80027C3Cu: out = number_digits(a0, a1, a2, s(a3), arg(4), arg(5), arg(6)); return true;
    case 0x80027E80u: out = static_cast<u32>(text_field(a0, a1, a2, a3, s(arg(4)), s(arg(5)), s(arg(6)))); return true;
    case 0x80028074u: out = text_format(a0, a1, a2); return true;
    case 0x800339E0u: flight_frame(a0, r[31], r[20]); return true;
    case 0x80033AB8u: draw_passes(); return true;
    case 0x80033324u: effects_pass(); return true;
    case 0x800333D8u: segments_pass(); return true;
    case 0x8003342Cu: model_pass(); return true;
    case 0x8003346Cu: world_pass(); return true;
    case 0x800337BCu: out = messages_pass(); return true;
    case 0x800337DCu: face_pass(); return true;
    case 0x80033814u: hud_cockpit(); return true;
    case 0x8002B59Cu: out = stream_played(); return true;
    case 0x8002CAE4u: out = stream_played_of(a0); return true;
    case 0x800361BCu: out = demo_request_replay(); return true;
    case 0x8003623Cu: out = demo_start_replay(); return true;
    case 0x80025724u: main_nothing(); return true;
    case 0x80025FE8u: flow_shutdown(); return true;
    case 0x80027064u: pads_stop(); return true;
    case 0x800265F0u: out = static_cast<u32>(pad_id_cmp(a0, a1)); return true;
    case 0x80026694u: out = pad_mode_code(a0); return true;
    case 0x80026700u: out = pad_act_byte(a0, 1); return true;
    case 0x80026754u: out = pad_act_byte(a0, 2); return true;
    case 0x800267A8u: out = bytes3_differ(a0, a1); return true;
    case 0x800267F0u: out = pad_act_bit(a0); return true;
    case 0x80027084u: load_tim_at(a0, a1, a2, a3, arg(4)); return true;
    case 0x800273BCu: out = record_checksum(a0, a1, a2); return true;
    case 0x800274E0u: out = mem_low_check(a0); return true;
    case 0x800275FCu: debug_font_page(a0); return true;
    case 0x80027990u: out = debug_print_at(a0, a1, a2, a3); return true;
    case 0x80027AE0u: out = debug_print(a0, a1, a2, a3); return true;
    case 0x80028A7Cu: out = static_cast<u32>(axis_curve(s(a0), false)); return true;
    case 0x80028B1Cu: out = static_cast<u32>(axis_curve(s(a0), true)); return true;
    case 0x800296F0u: hud_alert_burning_hard(); return true;
    case 0x8002B46Cu: out = kcd_lid_closed(); return true;
    case 0x8002B718u: out = kcd_ring_release(a0); return true;
    case 0x8002BCA4u: out = stream_ring_mark(); return true;
    case 0x8002BD28u: req_stereo_set(a0); return true;
    case 0x8002BD34u: req_volume_set(s(a0)); return true;
    case 0x8002BD5Cu: req_volume_apply(); return true;
    case 0x8002BD88u: out = req_stereo(); return true;
    case 0x8002BD9Cu: out = req_volume(); return true;
    case 0x8002CBA4u: out = stream_kind(a0); return true;
    case 0x8002D6E0u: out = snd_nothing_a(); return true;
    case 0x8002D6E8u: spu_memory_clear(); return true;
    case 0x8002D71Cu: spu_on(); return true;
    case 0x8002D73Cu: spu_off(); return true;
    case 0x8002D8C8u: snd_cd_reverb(a0); return true;
    case 0x8002DA60u: reverb_none(); return true;
    case 0x8002F6E4u: engine_set_word(a0, a1); return true;
    case 0x800304ECu: out = snd_nothing_b(); return true;
    case 0x800304F4u: out = sound_scene_reset(); return true;
    case 0x80030528u: out = sound_scene(a0); return true;
    case 0x80030750u: out = sound_scene_stop(); return true;
    case 0x80031E08u: out = height_curve(s(a0)); return true;
    case 0x80031E9Cu: case 0x80031F1Cu: case 0x80031F5Cu: case 0x80031FA0u:
    case 0x80032008u: case 0x80032070u: case 0x80032078u:
        phase_hook_nothing(fn);
        return true;
    case 0x80026350u: out = flow_scene_machine(); return true;
    case 0x800332A4u: phase11_frame(); return true;
    case 0x800332C4u: phase12_frame(); return true;
    case 0x8003423Cu: out = phase11(); return true;
    case 0x80034274u: out = phase12(); return true;
    case 0x8003514Cu: retry_prompt_input(); return true;
    case 0x800353E8u: out = retry_prompt_draw(a0, a1); return true;
    case 0x80034000u:
    case 0x80034160u:
    case 0x800342ACu:
    case 0x800342ECu:
        part_target = fn;
        out = flight_phase_frame(fn);
        return true;
    case 0x800295CCu: hud_alert_k3(); return true;
    case 0x80029604u: hud_alert_k15(); return true;
    case 0x8002963Cu: hud_alert_k60(); return true;
    case 0x80029674u: hud_alert_burning(); return true;
    case 0x800296B8u: hud_alert_k9(); return true;
    case 0x8002F140u: snd_delayed_reset(); return true;
    case 0x8002F254u: sound_queue_reset(); return true;
    case 0x8002BDC4u: out = kcd_cmd_none(a0); return true;
    case 0x8002BDCCu: out = kcd_cmd_stop(a0); return true;
    case 0x8002BE14u: out = kcd_cmd_stream(a0); return true;
    case 0x8002C1D4u: out = kcd_cmd_video(a0); return true;
    case 0x8002C41Cu: out = kcd_cmd_pause(a0); return true;
    case 0x80029A44u:
    case 0x80029D70u:
    case 0x80029E74u:
    case 0x80029FE4u:
    case 0x8002A200u:
    case 0x8002A498u:
    case 0x8002BDB0u:
    case 0x800357B4u:
        out = call_guest(fn, a0, a1);
        return true;
    case 0x80031EDCu: phase_leave_64(); return true;
    case 0x80031EFCu: phase_enter_128(); return true;
    case 0x80031FA8u: phase_script_8(a0); return true;
    case 0x80031FD8u: phase_script_64(a0); return true;
    case 0x80032010u: phase_script_2000(a0); return true;
    case 0x80032040u: phase_script_end(a0); return true;
    case 0x80026044u: out = boot_phase(); return true;
    case 0x80032754u: out = mission_init(); return true;
    case 0x800357C0u: out = loading_frame(); return true;
    case 0x800358F8u: out = request_mission(); return true;
    case 0x80035A38u: out = unpack_mission(); return true;
    case 0x80035F18u: out = request_briefing(); return true;
    case 0x80035F98u: out = unpack_briefing(); return true;
    case 0x80033EF0u:
    case 0x80034040u:
        part_target = fn;
        out = flight_phase_frame(fn);
        return true;
    case 0x80032F3Cu:
    case 0x800330CCu:
    case 0x8003307Cu:
    case 0x80033104u:
    case 0x800331E0u:
    case 0x8003329Cu:
        reg_s0 = r[16];
        reg_s4 = r[20];
        reg_ra = r[31];
        run_tick(fn);
        return true;
    case 0x8002572Cu:
    case 0x800261A8u:
        for (u32 next = fn == 0x8002572Cu ? boot() : main_start(r[31]);;) {
            if (!run_phase(next)) unported(next, "a phase function");
            next = main_loop(phase_result);
        }
    default: return false;
    }
}

bool Game::port_phase_exe_a(u32 fn, bool run) {
    switch (fn) {
    case 0x800361BCu:
        if (run) phase_result = demo_request_replay();
        return true;
    case 0x8003623Cu:
        if (run) phase_result = demo_start_replay();
        return true;
    case 0x80034000u:
    case 0x80034160u:
    case 0x800342ACu:
        if (run) phase_result = flight_phase_frame(fn);
        return true;
    case 0x8003423Cu:
        if (run) phase_result = phase11();
        return true;
    case 0x80034274u:
        if (run) phase_result = phase12();
        return true;
    default: return false;
    }
}
}
