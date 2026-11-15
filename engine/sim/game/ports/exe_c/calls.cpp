#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"
#include <bit>

namespace ac3::sim {
bool Game::port_call_exe_c(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    (void)a0; (void)a1; (void)a2; (void)a3; (void)arg; (void)out;
    switch (fn) {
    case 0x80086320u: out = particle_debris(a0, a1, a2); return true;
    case 0x80086A0Cu: out = particle_smoke(a0, a1, a2); return true;
    case 0x8008700Cu: out = particle_meshes_draw(a0); return true;
    case 0x800867E4u: out = particle_rotation(a0, a1, a2); return true;
    case 0x80086948u: out = particle_compose_rotation(a0); return true;
    case 0x800854B8u: out = particle_spray_draw(a0); return true;
    case 0x8008591Cu: out = particle_quad(a0, a1); return true;
    case 0x80085A94u: out = particle_puff(a0, a1, a2); return true;
    case 0x80085C8Cu: out = particle_growing_puff(a0, a1, a2); return true;
    case 0x80085EACu: out = particle_splash(a0, a1, a2); return true;
    case 0x800860A4u: out = particle_pulse(a0, a1, a2); return true;
    case 0x8005B654u: out = restart_setup_result(r[19]); return true;
    case 0x8005BC64u: out = mission_ending(r[17]); return true;
    case 0x8005BE0Cu: out = mission_end_transition(r[16], r[17]); return true;
    case 0x8005C270u: out = mission_end_dispatch(r[16], r[17]); return true;
    case 0x800582F8u: out = movie_phase(); return true;
    case 0x8005AE8Cu: out = movie_mission_init(); return true;
    case 0x8005B124u: out = movie_mission_draw(); return true;
    case 0x8005B308u: out = movie_standalone_init(); return true;
    case 0x8005B4E4u: out = movie_standalone_draw(); return true;
    case 0x80058938u: out = movie_poll(a0); return true;
    case 0x8005889Cu: out = movie_wait(a0); return true;
    case 0x800592E0u: out = movie_draw_cues(a0, a1, a2); return true;
    case 0x8005911Cu: out = movie_tick(); return true;
    case 0x8005A2C4u: out = movie_draw_border(a0, a1); return true;
    case 0x80059488u: out = movie_draw_bank(a0, a1, a2); return true;
    case 0x800598BCu: out = movie_draw_crossfade(a0, a1, a2); return true;
    case 0x80059BD8u: out = movie_draw_fade(a0, a1, a2); return true;
    case 0x80059E1Cu: out = movie_draw_zoom(a0, a1, a2); return true;
    case 0x8005A034u: out = movie_draw_shrink(a0, a1, a2); return true;
    case 0x8006EF30u: flight_model(a0, a1, a2, a3); out = 0; return true;
    case 0x8006D0DCu: controls_standard(a0, a1, a2, a3); out = m.r16(a1 + 6); return true;
    case 0x8006D678u: out = controls_ground_result(a0, a1, a2, a3); return true;
    case 0x8006DD5Cu: controls_novice(a0, a1, a2, a3, arg(4), arg(5), arg(6)); out = m.r16(a1 + 6); return true;
    case 0x80058134u: gmv_end(); out = 1; return true;
    case 0x800581C4u: out = gmv_phase3(); return true;
    case 0x8005860Cu: out = stream_player_setup(a0, a1, a2, a3,
        arg(4), arg(5), arg(6), arg(7), arg(8), arg(9), arg(10), arg(11)); return true;
    case 0x800586ACu: out = stream_player_reset(a0); return true;
    case 0x80058800u: out = stream_video_request(); return true;
    case 0x80058854u: out = stream_video_complete(); return true;
    case 0x80058864u: out = stream_video_reset(); return true;
    case 0x80058888u: out = stream_video_ready(); return true;
    case 0x8005B234u: out = video_restore_mission(); return true;
    case 0x8005B5BCu: out = video_restore_image(); return true;
    case 0x8005B648u: mission_status_clear(); out = r[2]; return true;
    case 0x8005C5B0u: gauge_speed(a0); out = (a0 + 124u) & 0xFFFFFFu; return true;
    case 0x8005C680u: gauge_speed_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005C990u: gauge_altitude(a0); out = 0; return true;
    case 0x8005CB48u: gauge_altitude_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005CEDCu: gauge_heading(a0); out = 0; return true;
    case 0x8005D000u: gauge_heading_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005D344u: gauge_throttle(a0); out = 0; return true;
    case 0x8005D468u: gauge_throttle_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005D844u: gauge_weapons(a0); out = 0; return true;
    case 0x8005D998u: gauge_weapons_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005DCA8u: gauge_damage(a0); out = a0 + 56u; return true;
    case 0x8005DE40u: gauge_damage_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005DEFCu: gauge_timer(a0); out = a0; return true;
    case 0x8005E018u: gauge_timer_fill(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005E10Cu: gauge_templates(a0); out = a0; return true;
    case 0x8005E178u: gauges_draw(a0, a1, a2); out = m.r32(a1); return true;
    case 0x8005E2C0u: gauge_blocks(); out = 0; return true;
    case 0x8005E31Cu: gauges_args(a0); out = m.r32(m.r32(m.r32(g::kWorld) + 16)); return true;
    case 0x8005E40Cu: out = gauges_build(a0, a1, a2); return true;
    case 0x8005E460u: out = tile_set(a0, a1, static_cast<s32>(a2)); return true;
    case 0x8005E4CCu: out = terrain_tile_list(a0, a1, a2); return true;
    case 0x8005E6DCu: out = terrain_draw(a0, a1, a2, a3); return true;
    case 0x8005E7ECu: depth_cue(static_cast<s32>(a0), static_cast<s32>(a1), static_cast<s32>(a2));
        out = gte.cfc2(28); return true;
    case 0x8005EAB8u: fog_table(static_cast<s32>(a0), static_cast<s32>(a1), a2); out = 4094; return true;
    case 0x8005EB20u: {
        const u32 scratch = m.r8(a3 + 1) << 4;
        palette_fade(a0, a1, static_cast<s32>(a2), a3, arg(4));
        out = static_cast<s32>(a2) > 0 ? 0 : scratch; return true;
    }
    case 0x8005EBB8u: palette_fades(a0, a1, static_cast<s32>(a2)); out = 0; return true;
    case 0x8005EDC8u: out = palette_animate(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x8005EFF0u: out = terrain_draw_modes(a0, a1); return true;
    case 0x8005F0E4u: {
        const u32 world = m.r32(g::kWorld);
        world_flag4(a0); out = a0 ? m.r32(world) : world; return true;
    }
    case 0x8005F124u: out = static_cast<u32>(outline_max_x(a0, static_cast<s32>(a1))); return true;
    case 0x8005F174u: out = static_cast<u32>(outline_min_x(a0, static_cast<s32>(a1))); return true;
    case 0x8005F1C4u: out = outline_same_facing(a0, static_cast<s32>(a1)); return true;
    case 0x8005F2B4u: out = clip_screen_line(a0, a1, static_cast<s32>(a2), static_cast<s32>(a3)); return true;
    case 0x8005F510u: out = clip_segment_height(a0, a1, static_cast<s32>(a2), a3); return true;
    case 0x8005F734u: out = unproject_rotated(a0, a1, static_cast<s32>(a3)); return true;
    case 0x8005F7FCu: out = project_rotated(a0, a1); return true;
    case 0x8005F90Cu: out = terrain_outline_build(a0, a1, a2, a3); return true;
    case 0x80060CB8u: out = terrain_outline_grid(a0); return true;
    case 0x80061534u: out = terrain_outline_place(a0); return true;
    case 0x80061614u: out = terrain_outline(a0, a1, a2, a3); return true;
    case 0x800616B0u: out = terrain_project_model(a0, a1, a2); return true;
    case 0x80061768u: out = terrain_draw_tiles(a0, a1, a2); return true;
    case 0x800619E0u: out = terrain_flat_polygons(a0); return true;
    case 0x80061C3Cu: out = terrain_gouraud_polygons(a0); return true;
    case 0x80061E50u: out = vertex_midpoint(a0, a1, a2); return true;
    case 0x80061ED4u: out = terrain_triangle(a0, a1, a2, a3); return true;
    case 0x80061FA8u: out = terrain_patch(a0, a1, static_cast<s32>(a2), a3); return true;
    case 0x8006258Cu: out = terrain_polygon_patch(a0, a1); return true;
    case 0x800626E0u: out = terrain_models(a0, a1); return true;
    case 0x80062828u: out = model_project_screens(a0, a1); return true;
    case 0x800628BCu: out = direct_flat_quads(a0, a1, a2, a3, arg(4)); return true;
    case 0x80062A38u: out = direct_gouraud_quads(a0, a1, a2, a3, arg(4)); return true;
    case 0x80062BD4u: out = draw_submeshes(a0, a1); return true;
    case 0x80062D68u: out = model_stored_triangles(a0); return true;
    case 0x80062EC8u: out = model_stored_quads(a0); return true;
    case 0x8006305Cu: out = model_fog_triangles(a0); return true;
    case 0x800631F8u: out = model_fog_quads(a0); return true;
    case 0x800633D8u: out = model_lit_triangles(a0); return true;
    case 0x8006358Cu: out = model_lit_quads(a0); return true;
    case 0x800637B0u: out = draw_aircraft_models(a0, a1); return true;
    case 0x80063AF0u: target_box_templates(); out = 0; return true;
    case 0x80063E4Cu: out = hud_markers(a0); return true;
    case 0x80063F80u: out = hud_buffers_begin(); return true;
    case 0x80063FCCu: out = hud_packets_link(); return true;
    case 0x80064400u: out = hud_attitude_if_tilted(a0); return true;
    case 0x8006447Cu: hud_view_marker(a0); out = 0; return true;
    case 0x8006466Cu: out = hud_attitude_matrix(); return true;
    case 0x8006477Cu: out = hud_attitude_shape(static_cast<s32>(a0), static_cast<s32>(a1)); return true;
    case 0x80064CB4u: out = hud_horizontal_marks(); return true;
    case 0x80064D98u: out = hud_pitch_ladder(a0); return true;
    case 0x80065598u: out = hud_compass(a0); return true;
    case 0x800657B0u: out = hud_compass_label(a0, a1); return true;
    case 0x80065D8Cu: out = hud_target_arrow(); return true;
    case 0x800660F8u: out = hud_view_brackets(a0); return true;
    case 0x800662B4u: out = gun_ring_update(); return true;
    case 0x80066590u: out = hud_impact_cue(a0); return true;
    case 0x80066A74u: out = hud_ballistic_ticks(static_cast<s32>(a0), static_cast<s32>(a1)); return true;
    case 0x80066C18u: out = morph_vertices_twice(static_cast<s32>(a0), static_cast<s32>(a1), a2, a3, arg(4), arg(5), arg(6)); return true;
    case 0x80066C80u: out = animate_models(a0, a1); return true;
    case 0x80066DCCu: out = animate_submeshes(a0, a1); return true;
    case 0x80066EF8u: out = morph_pose_controls(static_cast<s32>(a0), static_cast<s32>(a1), a2, a3, arg(4)); return true;
    case 0x8006701Cu: out = morph_pose_pair(static_cast<s32>(a0), a1, a2); return true;
    case 0x8006704Cu: out = morph_pose_signed(static_cast<s32>(a0), a1, a2); return true;
    case 0x80067088u: out = animate_aircraft(a0, a1); return true;
    case 0x80067144u: out = display_aircraft_list(a0); return true;
    case 0x80067380u: out = display_ground_list(a0); return true;
    case 0x80067494u: out = prepare_model_entries(a0, a1, a2, a3, arg(4), arg(5), arg(6)); return true;
    case 0x80068334u: out = draw_model_pass(a0, a1, a2, a3); return true;
    case 0x80068454u: {
        const u32 work = m.r32(g::kWeapons), mask = m.r32(work + 0x33E0u);
        out = mask == 0xFFFFFFFFu ? 0 : work + 0x33E4u + 12u * std::countr_one(mask);
        damage_queue_add(a0, a1, a2, static_cast<s32>(a3)); return true;
    }
    case 0x800684D4u: damage_queue_update(); out = m.r32(g::kWeapons); return true;
    case 0x800685C0u: out = weapon_alloc(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80068800u: out = spawn_weapon(a0, a1, a2, static_cast<s32>(a3), arg(4)); return true;
    case 0x800689E8u: out = launch_rocket_19(a0, a1, a2, a3); return true;
    case 0x80068A58u: out = spawn_fast_bomb(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80068AFCu: out = spawn_large_bomb(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80068BA8u: out = launch_rocket_18(a0, a1, a2, a3); return true;
    case 0x80068C0Cu: out = launch_sam_12(a0, a1, a2, a3); return true;
    case 0x80068C80u: out = launch_sam_13(a0, a1, a2, a3); return true;
    case 0x80068D0Cu: out = launch_dispenser(a0, a1, a2, a3); return true;
    case 0x80068E28u: out = submunition_spawn(a0, a1, a2, a3); return true;
    case 0x80068F34u: out = launch_68F34(a0, a1, a2, a3); return true;
    case 0x80068FA8u: out = launch_68FA8(a0, a1, a2, a3); return true;
    case 0x8006B034u: out = bullet_request(a0, a1, a2, a3); return true;
    case 0x80069018u: out = weapon_set_position(a0, a1); return true;
    case 0x8006903Cu: {
        const bool levelRoll = (m.r32(a0) & 8u) != 0;
        wpn_dropped_then_thrust(a0); out = levelRoll ? m.r32(a0) : 0; return true;
    }
    case 0x80069158u: {
        const bool levelRoll = (m.r32(a0) & 8u) != 0;
        wpn_dropped_falling(a0); out = levelRoll ? m.r32(a0) : 0; return true;
    }
    case 0x800692C4u: out = missile_home(a0); return true;
    case 0x800694CCu: out = missile_home_second(a0); return true;
    case 0x800696D4u: out = weapon_cruise(a0); return true;
    case 0x80069900u: out = weapon_split(a0); return true;
    case 0x80069A9Cu: out = missile_home_always(a0); return true;
    case 0x80069C8Cu: out = weapon_explode(a0); return true;
    case 0x80069D88u: out = weapon_blast_small(a0); return true;
    case 0x80069E10u: out = weapon_explode_target(a0); return true;
    case 0x80069EC0u: out = weapon_blast_large(a0); return true;
    case 0x80069D18u: {
        const u32 phase = m.r16(a0 + 8);
        out = phase <= 1 ? m.r16(a0 + 44) - 32u : 1u;
        wpn_bomb(a0); return true;
    }
    case 0x80069D6Cu: out = m.r16(a0 + 8) + 1u; wpn_inert(a0); return true;
    case 0x80069F48u: {
        const bool active = m.r16(a0 + 8) <= 1;
        wpn_rocket(a0); out = active ? m.r32(a0 + 80) : 1; return true;
    }
    case 0x80069FC0u: weapon_unused_callback(); out = r[2]; return true;
    case 0x80069FC8u: weapons_behave(); out = 0; return true;
    case 0x8006A11Cu: weapons_collide(); out = 0; return true;
    case 0x8006A978u: blast_damage(a0, static_cast<s32>(a1)); out = 0; return true;
    case 0x8006AAFCu: {
        out = m.r32(a0) & 1u;
        if (!out) {
            const u32 targetFlags = m.r32(slot(m.r32(a0 + 112u) & 63u));
            out = 0x4200u;
            if ((targetFlags & 0x4200u) == 0x4200u) {
                out = targetFlags & m.r32(a0 + 108u) & 0x1C0u;
                if (!out) {
                    const u32 work = m.r32(g::kWeapons), mask = m.r32(work + 0x33E0u);
                    out = mask == 0xFFFFFFFFu ? 0 : work + 0x33E4u + 12u * std::countr_one(mask);
                }
            }
        }
        direct_damage(a0); return true;
    }
    case 0x8006ABB8u: weapons_move(); out = 0; return true;
    case 0x8006AC34u: {
        const u32 work = m.r32(g::kWeapons);
        out = m.r32(work + 0x1548u) ? work : r[2]; weapons_expire(); return true;
    }
    case 0x8006AF20u: {
        const u32 work = m.r32(g::kWeapons);
        out = m.r32(work + 0x1548u) ? work : 0; weapons_update(); return true;
    }
    case 0x8006B110u: bullets_spawn(); out = m.r32(g::kWeapons); return true;
    case 0x8006B504u: {
        const u32 masks = m.r32(g::kWeapons) + 0x33D0u;
        u32 before = 0, after = 0;
        for (u32 bank = 0; bank < 4; ++bank) before += std::popcount(m.r32(masks + 4u * bank));
        bullets_update();
        for (u32 bank = 0; bank < 4; ++bank) after += std::popcount(m.r32(masks + 4u * bank));
        out = before > after ? r[29] - 0xB8u + before - after : 0; return true;
    }
    case 0x8006C5B4u: sim_step6(); out = m.r32(g::kWeapons); return true;
    case 0x8006AF58u: out = weapon_queue_request(a0, a1, a2); return true;
    case 0x8006C530u: weapons_reset(); out = m.r32(g::kWeapons) - 4u; return true;
    case 0x8006C588u: weapons_alloc(); out = m.r32(g::kWeapons) - 4u; return true;
    case 0x8006C20Cu: out = effect_segment_lists(a0, a1, a2, a3, arg(4), arg(5)); return true;
    case 0x8006C5ECu: out = draw_effect_segments(a0, a1); return true;
    case 0x8006C6B8u: missile_replay_show(a0, a1, a2); out = m.r16(a2 + 20u); return true;
    case 0x8006C740u: missile_replay_hide(a0); out = ~(1u << (a0 & 31)); return true;
    case 0x8006C760u: bullet_replay_spawn(a0, a1, a2, a3); out = m.r32(a2 + 8u); return true;
    case 0x8006C99Cu: {
        const u32 result = m.r32(g::kWeapons) + 60u * a0;
        bullet_replay_kill(a0); out = result; return true;
    }
    case 0x8006C9B8u: out = bullet_replay_update(); return true;
    case 0x8006CB10u: bullets_replay(); out = 0; return true;
    case 0x8006CB30u: out = player_mode(); return true;
    case 0x8006CB44u: {
        const bool enabled = (m.r32(m.r32(m.r32(g::kWorld) + 40u) + 80u) & 3u) != 0;
        rumble_add(a0); out = enabled ? 6u : 0u; return true;
    }
    case 0x8006CCE0u: player_blocks_alloc(); out = m.r32(0x800BD0A0u); return true;
    case 0x8006CD6Cu: out = player_def(static_cast<s32>(a0)); return true;
    case 0x8006CE70u: player_setup(a0); out = 0; return true;
    case 0x8006D078u: out = player_activate_parameter(); return true;
    case 0x8006E7E0u: out = distance_scale_vector(static_cast<s32>(a0), a1); return true;
    case 0x8006E874u: out = distance_scale_angles(static_cast<s32>(a0), a1); return true;
    case 0x8006EE38u: player_view_angles(a0, a1); out = static_cast<u32>(m.r16s(a1 + 58u)); return true;
    case 0x8006E908u: out = static_cast<u32>(flight_thrust(static_cast<s32>(a0),
        static_cast<s32>(a2), static_cast<s32>(a1), static_cast<s32>(a3),
        static_cast<s32>(arg(4)), static_cast<s32>(arg(5)), static_cast<s32>(arg(6)))); return true;
    case 0x8006EA88u: out = static_cast<u32>(flight_envelope(static_cast<s32>(a0),
        static_cast<s32>(a1), a2, a3, arg(4), arg(5))); return true;
    case 0x8006EC70u: out = static_cast<u32>(speed_limits(static_cast<s32>(a0), a1, a2, a3)); return true;
    default: return false;
    }
}

bool Game::port_phase_exe_c(u32 fn, bool run) {
    (void)run;
    switch (fn) {
    case 0x800581C4u: if (run) phase_result = gmv_phase3(); return true;
    default: return false;
    }
}
}
