#pragma once

#include "machine/cd.hpp"
#include "machine/gpu.hpp"
#include "machine/spu.hpp"
#include "machine/card.hpp"
#include "machine/mdec.hpp"
#include "sim/hw.hpp"

#include <functional>
#include <string>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
struct PadInput {
    bool connected = false;
    u8 type = 0x73;
    u16 buttons = 0;
    u8 lx = 0x80, ly = 0x80, rx = 0x80, ry = 0x80;
};

enum : u16 {
    kBtnSelect = 1u << 0, kBtnL3 = 1u << 1, kBtnR3 = 1u << 2, kBtnStart = 1u << 3,
    kBtnUp = 1u << 4, kBtnRight = 1u << 5, kBtnDown = 1u << 6, kBtnLeft = 1u << 7,
    kBtnL2 = 1u << 8, kBtnR2 = 1u << 9, kBtnL1 = 1u << 10, kBtnR1 = 1u << 11,
    kBtnTriangle = 1u << 12, kBtnCircle = 1u << 13, kBtnCross = 1u << 14, kBtnSquare = 1u << 15,
};

class Machine final : public sim::Hw {
public:
    explicit Machine(sim::Memory& m);

    void set_clock(u32 vblank, u32 vsync_ref) {
        vblank_ = vblank;
        vsync_ref_ = vsync_ref;
    }
    u32 vblank() const { return vblank_; }

    bool mount(const std::string& booted, const std::string& other, std::string* error);

    bool load(const std::string& path, std::string* error);
    u32 loaded_tick = 0, loaded_field = 0, loaded_checkpoint = 0, play_offset = 0;

    std::function<void(u32 vblank, PadInput pads[2])> poll_input;
    std::function<void()> on_field;
    std::function<void(u32 vblank)> on_vblank;
    std::function<void(u32 ot)> on_draw;
    std::function<void(u32 fn, u32 a0, u32 a1)> call_guest;

    PadInput pads[2];
    Spu spu;
    Cd cd;
    Gpu gpu;
    Card card;
    Mdec mdec;

    void field_advance();

    u32 vsync(s32 mode) override;
    u32 get_rcnt(u32 spec) override;
    void put_disp_env(u32 env) override;
    void put_draw_env(u32 env) override;
    void draw_otag(u32 ot) override;
    void clear_image(u32 rect, u32 r, u32 g, u32 b) override;
    void load_image(u32 rect, u32 src) override;
    void store_image(u32 rect, u32 dst) override;
    void move_image(u32 rect, u32 x, u32 y) override;
    void draw_prim(u32 prim) override;
    u32 reset_callback() override {
        vsync_hook_ = 0;
        callbacks_stopped_ = false;
        return 0;
    }
    u32 stop_callback() override {
        callbacks_stopped_ = true;
        return 0;
    }
    u32 restart_callback() override {
        callbacks_stopped_ = false;
        return 0;
    }
    u32 check_callback() override { return callbacks_stopped_ ? 0u : 1u; }
    u32 interrupt_callback(u32 irq, u32 fn) override {
        const u32 prev = irq_handler_[irq & 15u];
        irq_handler_[irq & 15u] = fn;
        return prev;
    }
    u32 vsync_callbacks(u32, u32 fn) override {
        vsync_hook_ = fn;
        return 0;
    }
    u32 get_intr_mask() override { return intr_mask_; }
    u32 wait_event(u32 desc) override;
    u32 cd_ready(u32 mode, u32 result) override { return cd.CdReady(mode, result); }
    u32 cd_ready_raw(u32 result) override { return cd.CD_ready(result); }
    u32 spu_fset_rxx(u32 reg, u32 value, u32 shift) override { return spu.spu_FsetRXX(reg, value, shift); }
    u32 spu_is_in_allocate_area(u32 a) override { return spu.SpuIsInAllocateArea_(a); }
    u32 spu_rget_all_keys_status(s32 lo, s32 hi, u32 p) override { return spu.SpuRGetAllKeysStatus(lo, hi, p); }
    void get_draw_env(u32 env) override;
    void get_disp_env(u32 env) override;
    u32 dec_dct_put_env(u32 env) override { return mdec.put_env(env); }
    u32 set_rcnt(u32, u32, u32) override { return 1; }
    u32 start_rcnt(u32) override { return 1; }
    u32 cd_init() override { return cd.CdInit(); }
    u32 cd_both_discs() override { return cd.both_discs(); }
    void cd_pack_follow(u32 number) override { cd.pack_follow(static_cast<int>(number)); }
    void spu_init() override { spu.SpuInit(); }
    u32 spu_set_reverb_mode_param(u32 attr) override { return spu.SpuSetReverbModeParam(attr); }
    u32 pad_start_com() override { return 0; }
    u32 pad_enable_com(u32 mode) override {
        const u32 prev = pad_com_;
        pad_com_ = mode;
        return prev;
    }
    void init_card(u32) override { card.InitCARD(); }
    u32 start_card() override { return card.StartCARD(); }
    u32 dma_callback(u32 ch, u32 fn) override {
        const u32 prev = dma_handler_[ch & 7u];
        dma_handler_[ch & 7u] = fn;
        return prev;
    }
    void spu_set_reverb_attr(u32 attr) override { spu.spu_setReverbAttr(attr); }
    void spu_start() override { spu.SpuStart(); }
    void spu_quit() override { spu.SpuQuit(); }
    u32 spu_write0(u32 size) override { return spu.SpuWrite0(size); }
    u32 stop_card() override { return card.StopCARD(); }
    void deliver_event_hw(u32 cls, u32 spec) override { deliver_event(cls, spec); }
    u32 vsync_callback(u32 fn) override {
        const u32 prev = vsync_hook_;
        vsync_hook_ = fn;
        return prev;
    }
    u32 set_intr_mask(u32 mask) override {
        const u32 prev = intr_mask_;
        intr_mask_ = mask;
        return prev;
    }
    void reset_graph(u32 mode) override;
    void set_disp_mask(u32 on) override;
    u32 pad_get_state(u32 port) override;
    u32 pad_info_mode(u32 port, u32 term, u32 offs) override;
    u32 pad_info_act(u32 port, u32 act, u32 term) override;
    u32 pad_chk_vsync() override { return 1; }
    void spu_set_key(s32 on, u32 voices) override { spu.SpuSetKey(on, voices); }
    void spu_set_voice_attr(u32 attr) override { spu.SpuSetVoiceAttr(attr); }
    void spu_set_common_attr(u32 attr) override { spu.SpuSetCommonAttr(attr); }
    void spu_get_all_keys_status(u32 out) override { spu.SpuGetAllKeysStatus(out); }
    u32 spu_set_transfer_mode(u32 mode) override { return spu.SpuSetTransferMode(mode); }
    u32 spu_set_transfer_start_addr(u32 addr) override { return spu.SpuSetTransferStartAddr(addr); }
    u32 spu_write(u32 src, u32 size) override { return spu.SpuWrite(src, size); }
    u32 spu_is_transfer_completed(u32) override { return spu.SpuIsTransferCompleted(); }
    u32 spu_set_reverb(s32 on) override { return spu.SpuSetReverb(on); }
    void spu_set_reverb_depth(u32 attr) override { spu.SpuSetReverbDepth(attr); }
    u32 spu_set_reverb_voice(s32 on, u32 voices) override { return spu.SpuSetReverbVoice(on, voices); }
    u32 spu_clear_reverb_work_area(u32 mode) override { return spu.SpuClearReverbWorkArea(mode); }
    u32 spu_read_decoded_data(u32 dst, s32 flag) override { return spu.SpuReadDecodedData(dst, flag); }
    u32 open_event(u32 cls, u32 spec, u32 mode, u32 func) override;
    u32 close_event(u32 desc) override;
    u32 enable_event(u32 desc) override;
    u32 disable_event(u32 desc) override;
    u32 test_event(u32 desc) override;
    u32 enter_critical_section() override { return critical_depth_++ == 0 ? 1u : 0u; }
    u32 exit_critical_section() override {
        if (critical_depth_ > 0) --critical_depth_;
        return 0;
    }
    u32 card_info(u32 chan) override { return card.card_info(chan); }
    u32 card_load(u32 chan) override { return card.card_load(chan); }
    u32 card_clear(u32 chan) override { return card.card_clear(chan); }
    u32 card_read(u32 chan, u32 frame, u32 dst) override { return card.card_read(chan, frame, dst); }
    u32 card_write(u32 chan, u32 frame, u32 src) override { return card.card_write(chan, frame, src); }
    u32 card_chan() override { return card.card_chan(); }
    u32 file_open(u32 path, u32 flags) override { return card.open(path, flags); }
    u32 file_close(u32 fd) override { return card.close(fd); }
    u32 file_lseek(u32 fd, u32 off, u32 whence) override { return card.lseek(fd, off, whence); }
    u32 file_read(u32 fd, u32 dst, u32 n) override { return card.read(fd, dst, n); }
    u32 file_write(u32 fd, u32 src, u32 n) override { return card.write(fd, src, n); }
    u32 first_file(u32 path, u32 out) override { return card.firstfile(path, out); }
    u32 next_file(u32 out) override { return card.nextfile(out); }
    u32 cd_control(u32 com, u32 param, u32 result) override { return cd.CdControl(com, param, result); }
    u32 cd_control_b(u32 com, u32 param, u32 result) override { return cd.CdControlB(com, param, result); }
    u32 cd_control_f(u32 com, u32 param) override { return cd.CdControlF(com, param); }
    u32 cd_sync(u32 mode, u32 result) override { return cd.CdSync(mode, result); }
    u32 cd_get_sector(u32 dst, u32 words) override { return cd.CdGetSector(dst, words); }
    u32 cd_ready_callback(u32 fn) override { return cd.CdReadyCallback(fn); }
    u32 cd_sync_callback(u32 fn) override { return cd.CdSyncCallback(fn); }
    u32 cd_data_callback(u32 fn) override { return cd.CdDataCallback(fn); }
    u32 cd_search_file(u32 fp, u32 name) override { return cd.CdSearchFile(fp, name); }
    s32 cd_pos_to_int(u32 loc) override { return cd.CdPosToInt(loc); }
    u32 cd_int_to_pos(s32 lba, u32 loc) override { return cd.CdIntToPos(lba, loc); }
    u32 cd_read(u32 sectors, u32 dst, u32 mode) override { return cd.CdRead(sectors, dst, mode); }
    u32 cd_read_sync(u32 mode, u32 result) override { return cd.CdReadSync(mode, result); }
    u32 cd_reset(u32 mode) override { return cd.CdReset(mode); }
    u32 cd_mix(u32 vol) override { return cd.CdMix(vol); }
    u32 dec_dct_out_sync(u32) override { return 0; }
    u32 dec_dct_reset(u32) override { return mdec.reset(); }
    u32 dec_dct_in(u32 buf, u32 mode) override;
    u32 dec_dct_out(u32 dst, u32 words) override { return mdec.out(dst, words); }
    u32 dec_dct_in_callback(u32 fn) override { return mdec.in_callback(fn); }
    u32 dec_dct_out_callback(u32 fn) override { return mdec.out_callback(fn); }

    void deliver_event(u32 cls, u32 spec);

    u32 disp_env() const { return disp_env_; }
    u32 draw_env() const { return draw_env_; }

private:
    void pads_fill();
    void pad_fill_buffer(int port, u32 buf);
    u32 direct_buffer(int port) const;

    struct Event {
        int used = 0;
        u32 cls = 0, spec = 0, mode = 0, func = 0, state = 0;
    };
    Event events_[32];
    s32 critical_depth_ = 0;
    u32 dma_handler_[8] = {};
    u32 irq_handler_[16] = {};
    bool callbacks_stopped_ = false;
    u32 vsync_hook_ = 0, intr_mask_ = 0;
    u32 pad_com_ = 1;

    sim::Memory& m;
    u32 vblank_ = 0, vsync_ref_ = 0;
    u32 disp_env_ = 0, draw_env_ = 0;
};
}
