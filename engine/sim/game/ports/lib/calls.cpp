#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

namespace ac3::sim {
bool Game::port_call_lib(u32 fn, const u32* r, u32& out) {
    const u32 a0 = r[4], a1 = r[5], a2 = r[6], a3 = r[7];
    auto arg = [&](u32 k) { return m.r32(r[29] + 0x10 + 4 * (k - 4)); };
    const u32 v0 = r[2];
    const u32 ra = r[31];
    auto s = [](u32 v) { return static_cast<s32>(v); };
    auto out64 = [&](u64 v) {
        port_regs.set(3, static_cast<u32>(v >> 32));
        out = static_cast<u32>(v);
    };
    auto u64of = [](u32 lo, u32 hi) { return (static_cast<u64>(hi) << 32) | lo; };
    auto set_reg = [&](u32 n, u32 v) { port_regs.set(n, v); };
    switch (fn) {
    case 0x800A6134u: out = static_cast<u32>(sin_1(s(a0))); return true;
    case 0x800A6268u:
    case 0x800A6270u: out = init_geom(ra); return true;
    case 0x800A6388u: out = static_cast<u32>(inv_square_root(s(a0), a1, a2)); return true;
    case 0x800A6414u: out = static_cast<u32>(vector_normal_s(a0, a1)); return true;
    case 0x800A6428u: out = static_cast<u32>(vector_normal(a0, a1)); return true;
    case 0x800A6458u: out = static_cast<u32>(vector_normal_ss(a0, a1)); return true;
    case 0x800A6464u:
    case 0x800A6488u: {
        s32 x = s(r[8]), y = s(r[9]), z = s(r[10]);
        if (fn == 0x800A6464u) {
            out = static_cast<u32>(normal_to_short(x, y, z, a1));
            x = m.r16s(a1), y = m.r16s(a1 + 2), z = m.r16s(a1 + 4);
        } else {
            out = static_cast<u32>(normalize_ss(x, y, z));
        }
        set_reg(8, static_cast<u32>(x));
        set_reg(9, static_cast<u32>(y));
        set_reg(10, static_cast<u32>(z));
        return true;
    }
    case 0x800A6548u: out = static_cast<u32>(matrix_normal(a0, a1)); return true;
    case 0x800A6638u: out = mul_matrix0(a0, a1, a2); return true;
    case 0x800A6A08u: set_mul_matrix(a0, a1); out = a0; return true;
    case 0x800A6B18u: out = apply_rot_matrix_lv(a0, a1); return true;
    case 0x800A6C58u: out = mul_matrix(a0, a1); return true;
    case 0x800A6D68u: out = mul_matrix2(a0, a1); return true;
    case 0x800A6F08u: out = scale_matrix(a0, a1); return true;
    case 0x800A70C8u: read_geom_offset(a0, a1); out = v0; return true;
    case 0x800A70E8u: out = read_geom_screen(); return true;
    case 0x800A7148u: light_color(a0, a1); out = v0; return true;
    case 0x800A7170u: dpq_color_light(a0, a1, a2, a3); out = v0; return true;
    case 0x800A7198u: dpq_color3(a0, a1, a2, a3, arg(4), arg(5), arg(6)); out = v0; return true;
    case 0x800A71D4u: intpl(a0, a1, a2); out = v0; return true;
    case 0x800A71F8u: out = square12(a0, a1); return true;
    case 0x800A7220u: out = square0(a0, a1); return true;
    case 0x800A7248u: out = average_z3(a0, a1, a2); return true;
    case 0x800A7268u: out = average_z4(a0, a1, a2, a3); return true;
    case 0x800A728Cu: outer_product12(a0, a1, a2); out = v0; return true;
    case 0x800A72E4u: outer_product0(a0, a1, a2); out = v0; return true;
    case 0x800A733Cu: out = lzc(a0); return true;
    case 0x800A7358u: out = static_cast<u32>(rot_trans_pers(a0, a1, a2, a3)); return true;
    case 0x800A7388u:
        out = static_cast<u32>(rot_trans_pers3(a0, a1, a2, a3, arg(4), arg(5), arg(6), arg(7)));
        return true;
    case 0x800A73E8u: out = rot_trans(a0, a1, a2); return true;
    case 0x800A7418u: out = normal_clip(a0, a1, a2); return true;
    case 0x8009BA28u: out = pad_set_vsync_param(); return true;
    case 0x8009CD10u: out = pad_chk_vsync_api(); return true;
    case 0x8009CD30u: out = pad_start_com_api(); return true;
    case 0x8009CD50u: out = pad_stop_com_api(); return true;
    case 0x8009DDD0u: out = pad_init_direct(a0, a1); return true;
    case 0x8009E430u: out = pad_direct_handlers(); return true;
    case 0x8009C0A4u: pad_xfer_step(a0); out = v0; return true;
    case 0x8009C194u: out = static_cast<u32>(pad_sio_next(a0, a1)); return true;
    case 0x8009C3A4u: out = static_cast<u32>(pad_sio_first(a0, a1)); return true;
    case 0x8009C6E0u: out = static_cast<u32>(pad_xfer_start(a0)); return true;
    case 0x8009C728u: out = static_cast<u32>(pad_xfer_command(a0)); return true;
    case 0x8009C800u: out = static_cast<u32>(pad_xfer_id(a0)); return true;
    case 0x8009C8ACu: out = static_cast<u32>(pad_xfer_ready(a0)); return true;
    case 0x8009C970u: out = static_cast<u32>(pad_xfer_data(a0)); return true;
    case 0x8009D0FCu: out = libpad_set_act_align(a0, a1); return true;
    case 0x8009D134u: out = libpad_set_main_mode(a0, a1, a2); return true;
    case 0x8009D17Cu: libpad_set_act(a0, a1, a2); out = v0; return true;
    case 0x8009D1D0u: pad_act_set(a0, a1, a2); out = v0; return true;
    case 0x8009D5ECu: pad_info_request(a0); out = v0; return true;
    case 0x8009D694u: out = pad_info_result(a0); return true;
    case 0x8009DA8Cu: out = pad_align_queue(a0, a1); return true;
    case 0x8009DAF4u: out = pad_align_request(a0); return true;
    case 0x8009DB10u: out = pad_align_result(a0); return true;
    case 0x8009DBD8u: out = pad_mode_queue(a0, a1, a2); return true;
    case 0x8009DC70u: out = pad_mode_request(a0); return true;
    case 0x8009DCC4u: out = pad_mode_result(a0); return true;
    case 0x8009DD44u:
    case 0x8009DD58u:
    case 0x8009DD78u:
    case 0x8009DD98u:
    case 0x8009DDB8u: out = pad_command(fn, a0, a1); return true;
    case 0x8009DF30u: pad_port_reset(a0); out = v0; return true;
    case 0x8009DF98u: out = pad_xfer_end(a0); return true;
    case 0x8009E08Cu: out = pad_command_done(a0); return true;
    case 0x8009E09Cu: out = pad_tx_byte(a0); return true;
    case 0x8009E158u: pad_act_prepare(a0); out = v0; return true;
    case 0x8009E3D4u: out = pad_port_number(a0); return true;
    case 0x8009E40Cu: out = libpad_port(a0); return true;
    case 0x8009E464u: out = pad_direct_request(a0); return true;
    case 0x8009E574u: pad_direct_result(a0); out = v0; return true;
    case 0x8009E8B8u: out = libpad_busy(a0); return true;
    case 0x8009E8F0u: out = pad_wait_set(a0); return true;
    case 0x8009E910u: out = pad_wait_over(); return true;
    case 0x8009E9B0u: out64(static_cast<u64>(divdi3(static_cast<s64>(u64of(a0, a1)), static_cast<s64>(u64of(a2, a3))))); return true;
    case 0x8009EA48u: out64(udivdi3(u64of(a0, a1), u64of(a2, a3))); return true;
    case 0x8009EA68u: out64(udivmoddi4(u64of(a0, a1), u64of(a2, a3), arg(4))); return true;
    case 0x800ADC18u: out = static_cast<u32>(libc_toupper(a0)); return true;
    case 0x800A7DE4u: out = gte_patch_template(r[26], r[1], r[2], r[3], ra, a0); return true;
    case 0x800AD5ACu: card_tpl_control(r[3], r[2]); out = v0; return true;
    case 0x800AD5D8u: out = card_tpl_wait(r[3]); return true;
    case 0x800AD61Cu: out = card_tpl_jump(); return true;
    case 0x800AD630u: out = card_tpl_call(); return true;
    case 0x800AD804u:
    case 0x800AD808u: out = card_format(fn, a0); return true;
    case 0x800ACB58u: out = mdec_reset(a0); return true;
    case 0x800ACC48u: out = mdec_in(a0, a1); return true;
    case 0x800ACCD8u: out = mdec_out(a0, a1); return true;
    case 0x800ACD64u: out = static_cast<u32>(mdec_in_sync()); return true;
    case 0x800ACDF8u: out = static_cast<u32>(mdec_out_sync()); return true;
    case 0x800ACE8Cu: out = mdec_status(); return true;
    case 0x800ACEA4u: out = mdec_timeout(a0); return true;
    case 0x800AF2F8u: out = api_remove96_entry(); return true;
    case 0x800AF794u: out = api_firstfile_patched(a0, a1, a2); return true;
    case 0x800AF8A8u: api_set_init_pad_flag(a0); out = v0; return true;
    case 0x800AF8B4u: out = api_read_init_pad_flag(); return true;
    case 0x800AF8C4u:
    case 0x800AF954u: out = api_pad_init(fn); return true;
    case 0x800AF9E4u: out = api_start_pad(); return true;
    case 0x800AFA14u: out = api_stop_pad(); return true;
    case 0x800AFA4Cu: out = api_pad_hook_install(); return true;
    case 0x800AFAC4u: out = api_pad_hook_remove(); return true;
    case 0x800AFAFCu: out = api_pad_intr(); return true;
    case 0x800AFB64u: out = api_pad_intr_pending(); return true;
    case 0x800AFCF8u: out = api_stub_minus1(a0); return true;
    case 0x8009F458u: spu_write_by_io(a0, a1); out = v0; return true;
    case 0x8009FC60u: spu_dma_start_a(); out = v0; return true;
    case 0x8009FC88u: spu_dma_start_b(); out = v0; return true;
    case 0x800A1E98u: out = spu_note2pitch(a0, a1, a2, a3); return true;
    case 0x800A1F68u: out = spu_pitch2note(a0, a1, a2, r[10], r[9]); return true;
    case 0x800ADC4Cu: out = static_cast<u32>(libc_tolower(a0)); return true;
    case 0x800A2210u: vsync_wait(s(a0), a1); out = v0; return true;
    case 0x800A243Cu: out = intr_install(r); return true;
    case 0x800A2514u: intr_dispatch(); out = v0; return true;
    case 0x800A26E4u: out = intr_callback_set(a0, a1); return true;
    case 0x800A282Cu: out = intr_stop(); return true;
    case 0x800A28CCu: out = intr_restart(); return true;
    case 0x800A2944u:
    case 0x800A2A58u:
    case 0x800A2D00u: out = lib_clear_words(fn, a0, a1); return true;
    case 0x800A29C0u: vsync_intr(); out = v0; return true;
    case 0x800A2A2Cu: out = vsync_callback_set(a0, a1); return true;
    case 0x800A2AD4u: dma_intr(); out = v0; return true;
    case 0x800A2C54u: out = dma_callback_set(a0, a1); return true;
    case 0x800A2D28u: out = set_video_mode(a0); return true;
    case 0x800A2D3Cu: out = get_video_mode(); return true;
    case 0x800A2F70u: out = cd_set_debug(a0); return true;
    case 0x800A2DE8u: out = cd_init_try(); return true;
    case 0x800A2E24u: out = cd_deliver(fn, 0x20); return true;
    case 0x800A2E4Cu:
    case 0x800A2E74u: out = cd_deliver(fn, 0x40); return true;
    case 0x800A2EA8u: out = cd_status(); return true;
    case 0x800A2EB8u: out = cd_mode(); return true;
    case 0x800A2EC8u: out = cd_last_com(); return true;
    case 0x800A34B8u: out = cd_getintr(); return true;
    case 0x800A4B48u: out = cd_intr(); return true;
    case 0x800A4F00u: out = cd_name_equal(a0, a1); return true;
    case 0x800A4F20u: out = cd_newmedia(); return true;
    case 0x800A51E4u: out = static_cast<u32>(cd_searchdir(a0, a1)); return true;
    case 0x800A5288u: out = static_cast<u32>(cd_cachefile(a0)); return true;
    case 0x800A5524u: out = cd_read_sectors(a0, a1, a2); return true;
    case 0x800A5598u: out = cdread_pause_done(); return true;
    case 0x800A55CCu: cdread_ready(a0, a1); out = v0; return true;
    case 0x800A5840u: cdread_data(); out = v0; return true;
    case 0x800A5910u: out = static_cast<u32>(cdread_start(a0)); return true;
    case 0x800A5F38u: out = cd_read_callback(a0); return true;
    case 0x800A5F4Cu: out = cd_read_mode(a0); return true;
    case 0x800A808Cu: out = get_graph_debug(); return true;
    case 0x800A81FCu: gpu_check_rect(a0, a1); out = v0; return true;
    case 0x800A8FE0u:
    case 0x800A91D8u: out = gpu_set_draw_env(fn, a0, a1); return true;
    case 0x800A9448u: out = gpu_mode_word(a0, a1, a2); return true;
    case 0x800A9468u:
    case 0x800A9500u: out = gpu_clip_word(fn, a0, a1); return true;
    case 0x800A9598u: out = gpu_offset_word(a0, a1); return true;
    case 0x800A95B4u: out = gpu_window_word(a0); return true;
    case 0x800A9634u: out = gpu_status(); return true;
    case 0x800A964Cu: out = static_cast<u32>(gpu_clear_otag(a0, a1)); return true;
    case 0x800A972Cu: out = gpu_clear_image(a0, a1); return true;
    case 0x800A995Cu: out = static_cast<u32>(gpu_load_image(a0, a1)); return true;
    case 0x800A9B98u: out = static_cast<u32>(gpu_store_image(a0, a1)); return true;
    case 0x800A9E18u: out = gpu_ctl(a0); return true;
    case 0x800A9E3Cu: out = gpu_ctl_last(a0); return true;
    case 0x800A9E50u: out = gpu_write_words(a0, a1); return true;
    case 0x800A9E90u: out = gpu_send_list(a0); return true;
    case 0x800A9ED8u: out = gpu_read_info(a0); return true;
    case 0x800A9F08u: out = static_cast<u32>(gpu_add_queue_ptr(a0, a1, a2)); return true;
    case 0x800A9F2Cu: out = static_cast<u32>(gpu_add_queue(a0, a1, a2, a3)); return true;
    case 0x800AA1DCu: out = gpu_run_queue(); return true;
    case 0x800AA43Cu: out = gpu_reset(a0); return true;
    case 0x800AA58Cu: out = static_cast<u32>(gpu_sync(a0)); return true;
    case 0x800AA6C8u: out = gpu_timeout_start(); return true;
    case 0x800AA6FCu: out = static_cast<u32>(gpu_timeout_check()); return true;
    case 0x800AA840u: out = gpu_version(a0); return true;
    case 0x800AAD20u: out = gpu_memset(a0, a1, a2); return true;
    case 0x800AAD48u: out = open_tim(a0); return true;
    case 0x800AAD58u: out = read_tim(a0); return true;
    case 0x800AB0A4u: out = static_cast<u32>(tim_parse(a0, a1)); return true;
    case 0x800AADBCu: out = tmd_open(a0, a1); return true;
    case 0x800AADFCu: out = tmd_read(a0); return true;
    case 0x800AB1BCu: out = tmd_object(a0, a1, a2, a3, arg(4)); return true;
    case 0x800AB330u: out = static_cast<u32>(tmd_prim(a0, a1)); return true;
    default: return port_call_lib_native(fn, r, out);
    }
}

bool Game::port_phase_lib(u32 fn, bool run) {
    (void)run;
    switch (fn) {
    default: return false;
    }
}
}
