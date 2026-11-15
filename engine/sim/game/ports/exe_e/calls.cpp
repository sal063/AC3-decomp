#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call_exe_e(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    (void)a0; (void)a1; (void)a2; (void)a3; (void)arg; (void)out;
    switch (fn) {
    case 0x80096A48u: out = draw_terrain_mesh(a0, a1, a2); return true;
    case 0x8008E4F8u: draw_high_cloud_streaks(); out = 0; return true;
    case 0x8008EB58u: draw_rain_streaks(); out = 0; return true;
    case 0x8008F1B8u: draw_snow_flakes(); out = 0; return true;
    case 0x8008F580u: draw_large_flecks(); out = 0; return true;
    case 0x8008C9D4u: out = draw_horizon_fan(a0); return true;
    case 0x800976F8u: out = draw_sky_rings(); return true;
    case 0x80098044u: out = draw_warp_sky(a0, a1); return true;
    case 0x8008D678u: out = draw_sun_cross(a0); return true;
    case 0x80096630u: out = subdivide_terrain_quad(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80097EB4u: out = draw_night_sky(a0, a1); return true;
    case 0x80098414u: out = draw_ring_mesh(a0, a1); return true;
    case 0x80098850u: out = draw_slice_mesh(a0); return true;
    case 0x80098DBCu: ray_cells(a0, a1, a2, a3, arg(4)); out = 0; return true;
    case 0x80099894u: out = ray_hit_tiles(a0, a1, a2, a3, arg(4), r[17]); return true;
    case 0x8009A5D0u: out = ray_cell(a0, a1, a2, a3); return true;
    case 0x8009A800u: out = terrain_raycast(a0, a1, a2, a3); return true;
    case 0x8009A944u: out = model_hit_tiles(a0, a1, a2, a3, arg(4), arg(5), r[20]); return true;
    case 0x8009B4ECu: out = model_raycast(a0, a1, a2, a3, arg(4), r[20]); return true;
    case 0x80098C68u: out = ray_plane_clip(a0, a1, static_cast<s32>(a2), a3); return true;
    case 0x80099428u: ray_matrix(a0, a1); out = 0; return true;
    case 0x80099750u: tile_transform(a0, a1, a2); out = 0; return true;
    case 0x8009A484u: out = raycast_tile(a0, a1, a2, a3, arg(4)); return true;
    case 0x800928F4u: screen_flash_update(a0); out = 0; return true;
    case 0x8009423Cu: scenery_fires(a0); out = 0; return true;
    case 0x8009B6B8u: out = overlay_load_begin(a0, a1); return true;
    case 0x8009B7CCu: out = overlay_load_poll(); return true;
    case 0x800916A8u: out = effect_round_end(a0, a1, a2); return true;
    case 0x80091A70u: out = effect_round_disc(a0, a1, a2); return true;
    case 0x80091CE8u: out = effect_rounded_segment(a0, a1, a2, a3); return true;
    case 0x80092014u: out = static_cast<u32>(effect_plane_distance(a0, a1)); return true;
    case 0x80092168u: out = clip_effect_segment(a0, a1, static_cast<s32>(a2), a3); return true;
    case 0x8009236Cu: out = draw_effect_segment(a0, a1, a2); return true;
    case 0x80092850u: draw_effect_segments(a0); out = 0; return true;
    case 0x80092D90u: out = emit_fire_smoke(a0); return true;
    case 0x80093040u: out = emit_rising_smoke(a0); return true;
    case 0x8009319Cu: out = emit_trailing_smoke(a0); return true;
    case 0x80093484u: out = emit_billboard(a0, a1); return true;
    case 0x8009351Cu: out = emit_transient_particle(a0, static_cast<s32>(a1)); return true;
    case 0x80093944u: out = emit_short_segment(a0); return true;
    case 0x80093B2Cu: out = effect_flash_tile(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80093BC4u: out = effect_masked_flash(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x80093C90u: out = effect_red_flash(a0, a1, static_cast<s32>(a2)); return true;
    case 0x800939D4u: out = effect_letterbox(a0, a1, static_cast<s32>(a2)); return true;
    case 0x80092B80u: out = emit_hot_puff(a0); return true;
    case 0x80093324u: out = emit_travelling_debris(a0, a1); return true;
    case 0x800935ACu: emit_seeded_cloud(a0, static_cast<s32>(a1), a2); out = 0; return true;
    case 0x800937D8u: emit_debris_burst(a0, static_cast<s32>(a1)); out = 0; return true;
    case 0x80095FA0u: out = ground_horizon_setup(a0, a1); return true;
    case 0x80096184u: out = ground_horizon_draw(a0, a1); return true;
    case 0x8009B6ACu: out = overlay_record_done(); return true;
    case 0x8009B904u: out = scene_record_done(); return true;
    case 0x80094380u: scenery_fire_trigger(static_cast<s32>(a0)); out = 0; return true;
    case 0x8008CD80u: out = annular_glow_setup(a0, a1, static_cast<s32>(a2)); return true;
    case 0x8008CEFCu: out = annular_glow_draw(a0, a1, static_cast<s32>(a2), static_cast<s32>(a3), r[2]); return true;
    case 0x8008D140u: out = draw_central_annular_glow(a0); return true;
    case 0x8008D244u: out = draw_small_annular_glow(a0); return true;
    case 0x8008D348u: out = draw_paired_annular_glows(a0); return true;
    case 0x800963F8u: out = quad_exceeds_gpu_span(a0, a1, a2, a3); return true;
    case 0x80096518u: out = mesh_vertex_midpoint(a0, a1, a2); return true;
    case 0x8009757Cu: out = sky_ring_project(a0, a1, static_cast<s32>(a2)); return true;
    case 0x8009830Cu: out = mesh_ring_project(a0, a1, a2); return true;
    case 0x80098714u: out = mesh_slice_project(a0, a1, a2); return true;
    case 0x800962B8u: out = draw_ground_horizon(a0, a1); return true;
    case 0x8009754Cu: out = draw_terrain_and_horizon(a0, a1, a2); return true;
    case 0x80093D1Cu: out = emit_cinematic_fire(a0, a1, a2, a3); return true;
    case 0x80093FC0u: out = cinematic_fire_update(static_cast<s32>(a0)); return true;
    case 0x8008C0A0u: out = draw_engine_flames(a0); return true;
    case 0x8008C644u: out = draw_exhaust_particles(a0, a1); return true;
    case 0x80091654u: sparks_update(a0); out = 0; return true;
    case 0x8009211Cu: out = effect_interpolate(a0, a1, a2, a3); return true;
    case 0x80092AB8u: out = transient_particles_clear(); return true;
    case 0x80092B04u: out = transient_particle_alloc(); return true;
    case 0x800965D4u: out = screen_outcode(a0); return true;
    case 0x8008F950u: out = framebuffer_filter(a0, a1, a2); return true;
    case 0x8008FAA8u: out = framebuffer_blend(a0, a1, a2, static_cast<s32>(a3)); return true;
    case 0x8008FC9Cu: out = fade_overlay(a0, a1, static_cast<s32>(a2)); return true;
    case 0x8008FFACu: out = draw_view_filters(a0); return true;
    case 0x800900A4u: out = muzzle_geometry(a0); return true;
    case 0x80090110u: out = muzzle_start(a0); return true;
    case 0x80090148u: muzzle_advance(a0); out = 0; return true;
    case 0x80090210u: muzzle_light(static_cast<s32>(a0), a1, a2); out = 0; return true;
    case 0x8009044Cu: draw_muzzle(a0); out = 0; return true;
    case 0x8008D4E8u: out = projected_glow(a0); return true;
    case 0x8008DE18u: out = beam_quad_corners(a0, a1); return true;
    case 0x8008D9F4u: out = draw_cloud_layers(); return true;
    case 0x8008E038u: out = draw_speed_streaks(); return true;
    case 0x80090A74u: out = draw_fading_lines(a0, a1, a2); return true;
    case 0x80090BF8u: out = draw_narrow_beams(a0, a1, a2); return true;
    case 0x80090ED0u: out = draw_medium_beams(a0, a1, a2); return true;
    case 0x800911B0u: out = draw_beam_squares(a0, a1, a2); return true;
    case 0x80091370u: out = draw_broad_beams(a0, a1, a2); return true;
    case 0x800943A8u: out = sky_copy_strips(a0, a1, a2, a3); return true;
    case 0x80094718u: out = horizon_view_setup(a0, a1); return true;
    case 0x800948A0u: out = horizon_project(a0, static_cast<s32>(a1)); return true;
    case 0x800949ACu: out = horizon_clip_codes(a0); return true;
    case 0x80094AC8u: out = horizon_boundaries(a0, a1); return true;
    case 0x80094D10u: out = horizon_colours(a0, a1); return true;
    case 0x80094F50u: out = sky_altitude_colour(a0, static_cast<s32>(a1)); return true;
    case 0x80095018u: out = packet_fade(a0, a1, static_cast<s32>(a2)); return true;
    case 0x80095074u: horizon_draw(a0, a1); out = 0; return true;
    case 0x80095298u: out = horizon_flat_band(a0, a1, a2); return true;
    case 0x8009546Cu: out = horizon_shaded_band(a0, a1, a2); return true;
    case 0x800957A4u: out = draw_sky(a0, a1); return true;
    case 0x80095930u: out = sky_cylinder(a0, a1); return true;
    case 0x80095CB8u: out = sky_points(a0, a1, static_cast<s32>(a2)); return true;
    default: return false;
    }
}

bool Game::port_phase_exe_e(u32 fn, bool run) {
    (void)run;
    switch (fn) {
    default: return false;
    }
}
}
