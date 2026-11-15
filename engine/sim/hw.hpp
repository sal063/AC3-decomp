#pragma once

#include "core/types.hpp"

namespace ac3::sim {
class Hw {
public:
    virtual ~Hw() = default;

    virtual u32 vsync(s32 mode) = 0;
    virtual u32 get_rcnt(u32 spec) = 0;

    virtual u32 reset_callback() = 0;
    virtual u32 set_rcnt(u32 spec, u32 target, u32 mode) = 0;
    virtual u32 start_rcnt(u32 spec) = 0;
    virtual u32 cd_init() = 0;
    virtual u32 cd_both_discs() { return 0; }
    virtual void cd_pack_follow(u32 number) { (void)number; }
    virtual void spu_init() = 0;
    virtual u32 spu_set_reverb_mode_param(u32 attr) = 0;
    virtual u32 pad_start_com() = 0;
    virtual u32 pad_enable_com(u32 mode) = 0;
    virtual void init_card(u32 pad_enable) = 0;
    virtual u32 start_card() = 0;
    virtual u32 dma_callback(u32 ch, u32 fn) = 0;
    virtual void spu_set_reverb_attr(u32 attr) = 0;

    virtual void spu_start() = 0;
    virtual void spu_quit() = 0;
    virtual u32 spu_write0(u32 size) = 0;
    virtual u32 stop_card() = 0;
    virtual void deliver_event_hw(u32 cls, u32 spec) = 0;
    virtual u32 vsync_callback(u32 fn) = 0;
    virtual u32 set_intr_mask(u32 mask) = 0;

    virtual void put_disp_env(u32 env) = 0;
    virtual void put_draw_env(u32 env) = 0;
    virtual void draw_otag(u32 ot) = 0;
    virtual void clear_image(u32 rect, u32 r, u32 g, u32 b) = 0;
    virtual void load_image(u32 rect, u32 src) = 0;
    virtual void store_image(u32 rect, u32 dst) = 0;
    virtual void move_image(u32 rect, u32 x, u32 y) = 0;
    virtual void draw_prim(u32 prim) = 0;
    virtual void reset_graph(u32 mode) = 0;
    virtual void set_disp_mask(u32 on) = 0;

    virtual u32 pad_get_state(u32 port) = 0;
    virtual u32 pad_info_mode(u32 port, u32 term, u32 offs) = 0;
    virtual u32 pad_info_act(u32 port, u32 act, u32 term) = 0;
    virtual u32 pad_chk_vsync() = 0;

    virtual void spu_set_key(s32 on, u32 voices) = 0;
    virtual void spu_set_voice_attr(u32 attr) = 0;
    virtual void spu_set_common_attr(u32 attr) = 0;
    virtual void spu_get_all_keys_status(u32 out) = 0;
    virtual u32 spu_set_transfer_mode(u32 mode) = 0;
    virtual u32 spu_set_transfer_start_addr(u32 addr) = 0;
    virtual u32 spu_write(u32 src, u32 size) = 0;
    virtual u32 spu_is_transfer_completed(u32 flag) = 0;
    virtual u32 spu_set_reverb(s32 on) = 0;
    virtual void spu_set_reverb_depth(u32 attr) = 0;
    virtual u32 spu_set_reverb_voice(s32 on, u32 voices) = 0;
    virtual u32 spu_clear_reverb_work_area(u32 mode) = 0;
    virtual u32 spu_read_decoded_data(u32 dst, s32 flag) = 0;

    virtual u32 open_event(u32 cls, u32 spec, u32 mode, u32 func) = 0;
    virtual u32 close_event(u32 desc) = 0;
    virtual u32 enable_event(u32 desc) = 0;
    virtual u32 disable_event(u32 desc) = 0;
    virtual u32 test_event(u32 desc) = 0;
    virtual u32 enter_critical_section() = 0;
    virtual u32 exit_critical_section() = 0;

    virtual u32 card_info(u32 chan) = 0;
    virtual u32 card_load(u32 chan) = 0;
    virtual u32 card_clear(u32 chan) = 0;
    virtual u32 card_read(u32 chan, u32 frame, u32 dst) = 0;
    virtual u32 card_write(u32 chan, u32 frame, u32 src) = 0;
    virtual u32 card_chan() = 0;
    virtual u32 file_open(u32 path, u32 flags) = 0;
    virtual u32 file_close(u32 fd) = 0;
    virtual u32 file_lseek(u32 fd, u32 off, u32 whence) = 0;
    virtual u32 file_read(u32 fd, u32 dst, u32 n) = 0;
    virtual u32 file_write(u32 fd, u32 src, u32 n) = 0;
    virtual u32 first_file(u32 path, u32 out) = 0;
    virtual u32 next_file(u32 out) = 0;

    virtual u32 cd_control(u32 com, u32 param, u32 result) = 0;
    virtual u32 cd_control_b(u32 com, u32 param, u32 result) = 0;
    virtual u32 cd_control_f(u32 com, u32 param) = 0;
    virtual u32 cd_sync(u32 mode, u32 result) = 0;
    virtual u32 cd_get_sector(u32 dst, u32 words) = 0;
    virtual u32 cd_ready_callback(u32 fn) = 0;
    virtual u32 cd_sync_callback(u32 fn) = 0;
    virtual u32 cd_data_callback(u32 fn) = 0;
    virtual u32 cd_search_file(u32 fp, u32 name) = 0;
    virtual s32 cd_pos_to_int(u32 loc) = 0;
    virtual u32 cd_int_to_pos(s32 lba, u32 loc) = 0;
    virtual u32 cd_read(u32 sectors, u32 dst, u32 mode) = 0;
    virtual u32 cd_read_sync(u32 mode, u32 result) = 0;
    virtual u32 cd_reset(u32 mode) = 0;
    virtual u32 cd_mix(u32 vol) = 0;
    virtual u32 dec_dct_out_sync(u32 mode) = 0;
    virtual u32 dec_dct_reset(u32 mode) = 0;
    virtual u32 dec_dct_in(u32 buf, u32 mode) = 0;
    virtual u32 dec_dct_out(u32 dst, u32 words) = 0;
    virtual u32 dec_dct_in_callback(u32 fn) = 0;
    virtual u32 dec_dct_out_callback(u32 fn) = 0;

    virtual u32 stop_callback() { return 0; }
    virtual u32 restart_callback() { return 0; }
    virtual u32 check_callback() { return 1; }
    virtual u32 interrupt_callback(u32, u32) { return 0; }
    virtual u32 vsync_callbacks(u32, u32) { return 0; }
    virtual u32 get_intr_mask() { return 0; }
    virtual u32 wait_event(u32) { return 1; }
    virtual u32 cd_ready(u32, u32) { return 0; }
    virtual u32 cd_ready_raw(u32) { return 0; }
    virtual u32 spu_fset_rxx(u32 reg, u32, u32) { return 0x1F801C00u + 2u * reg; }
    virtual u32 spu_is_in_allocate_area(u32) { return 0; }
    virtual u32 spu_rget_all_keys_status(s32, s32, u32) { return 0; }
    virtual void get_draw_env(u32) {}
    virtual void get_disp_env(u32) {}
    virtual u32 dec_dct_put_env(u32) { return 0; }
};
}
