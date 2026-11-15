#pragma once

#include "core/types.hpp"

#include <functional>
#include <vector>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
struct StateReader;

class Spu {
public:
    static constexpr u32 kRamSize = 0x80000;
    static constexpr int kVoices = 24;
    static constexpr int kSampleRate = 44100;

    static constexpr int kOff = 0, kOn = 1, kOffEnvOn = 2, kOnEnvOff = 3;

    struct Voice {
        u32 start_addr = 0, loop_addr = 0;
        u16 pitch = 0, note = 0, sample_note = 0;
        s16 vol_left = 0, vol_right = 0, volmode_left = 0, volmode_right = 0;
        u16 adsr1 = 0, adsr2 = 0;
        int keyed_on = 0;
    };

    struct Chan {
        u32 cur = 0;
        int have_block = 0;
        u8 flags = 0;
        s16 dec[28] = {};
        s16 hist[2] = {};
        int idx = 0;
        u32 counter = 0;
        s16 win[4] = {};
        int ended = 0;
        int phase = 0;
        s32 env = 0;
        u32 env_div = 0;
        s32 out = 0;
        s32 vol[2] = {};
        s32 vol_cyc[2] = {};
    };

    explicit Spu(sim::Memory& m);

    std::function<void(const s16*, int)> submit;
    std::function<void(u32 fn)> call_guest;

    void SpuInit();
    void SpuInitHot();
    void SpuStart() { ready_ = true; }
    void SpuQuit();
    u32 SpuSetMute(u32 on);
    u32 SpuGetMute() const { return static_cast<u32>(muted_); }
    u32 SpuSetTransferMode(u32 mode) { return transfer_mode_ = mode; }
    u32 SpuGetTransferMode() const { return transfer_mode_; }
    u32 SpuSetTransferStartAddr(u32 addr);
    u32 SpuGetTransferStartAddr() const { return transfer_addr_; }
    u32 SpuWrite(u32 src, u32 size);
    u32 SpuWrite0(u32 size);
    u32 SpuRead(u32 dst, u32 size);
    u32 SpuIsTransferCompleted() const { return 1; }
    u32 SpuSetTransferCallback(u32 fn);
    u32 SpuReadDecodedData(u32 dst, s32 flag);
    void SpuSetVoiceAttr(u32 attr);
    void SpuNSetVoiceAttr(int v, u32 attr);
    u32 SpuRSetVoiceAttr(int lo, int hi, u32 attr);
    void SpuGetVoiceAttr(u32 attr);
    void SpuNGetVoiceAttr(int v, u32 attr);
    void SpuSetKey(s32 on, u32 bits);
    void SpuSetKeyOnWithAttr(u32 attr);
    void SpuGetAllKeysStatus(u32 out);
    u32 SpuRGetAllKeysStatus(int lo, int hi, u32 out);
    void SpuGetVoiceEnvelope(int v, u32 out);
    void SpuGetVoiceEnvelopeAttr(int v, u32 status, u32 envx);
    u32 SpuGetKeyStatus(u32 bits) const;
    void SpuSetCommonAttr(u32 attr);
    void SpuGetCommonAttr(u32 attr);
    u32 SpuInitMalloc(s32 num);
    u32 SpuMalloc(u32 size);
    u32 SpuMallocWithStartAddr(u32 addr, u32 size);
    void SpuFree(u32 addr);
    u32 SpuClearReverbWorkArea(u32 mode);
    u32 SpuSetReverb(s32 on);
    u32 SpuGetReverb() const { return static_cast<u32>(rev_flag_); }
    u32 SpuSetReverbModeParam(u32 attr);
    void SpuGetReverbModeParam(u32 attr);
    void SpuSetReverbDepth(u32 attr);
    u32 SpuSetReverbVoice(s32 on, u32 bits);
    u32 SpuGetReverbVoice() const { return reverb_voices_; }
    u32 SpuReserveReverbWorkArea(s32 on);
    u32 SpuIsReverbWorkAreaReserved(s32 on) const;
    u32 SpuSetNoiseVoice(s32 on, u32 bits);
    u32 SpuSetNoiseClock(int clock);
    u32 SpuSetPitchLFOVoice(s32 on, u32 bits);
    void spu_setReverbAttr(u32 attr);
    u32 spu_FsetRXX(u32 reg, u32 value, u32 bytes);
    u32 SpuIsInAllocateArea_(u32 start_units) const { return rev_area_allocated(start_units) ? 1u : 0u; }

    int field_frames();
    void pump();
    void mix(s16* out, int nframes);

    void cd_push(const s16* interleaved, int nframes);
    int cd_queued() const { return static_cast<int>(cd_wr_ - cd_rd_); }
    void cd_flush() { cd_rd_ = cd_wr_; }

    int key_status(int v) const;
    const std::vector<u8>& ram() const { return ram_; }
    std::vector<u8>& ram() { return ram_; }

    bool load_state(StateReader& r);
    bool ready() const { return ready_; }

    static u8 decode_block(const u8* block, s16* out28, s16 hist[2]);
    static u16 note_to_pitch(u16 note, u16 sample_note);

private:
    void core_reset();
    void key_on(int v);
    void key_off(int v);
    void set_loop_addr(int v, u32 addr);
    void attr_changed(int v);
    s16 envx(int v) const { return static_cast<s16>(chan_[v].env); }
    void set_master(s16 l, s16 r, s16 ml, s16 mr);
    int block_load(Chan& c, int v);
    int voice_pull(Chan& c, int v);
    s32 gauss_interp(const Chan& c) const;
    s16 noise_next();
    void env_tick(Chan& c, const Voice& a);
    static void sweep_tick(s32* level, s32* cyc, int mode, s32 rate);
    void decoded_write(int which, s16 v);
    void cd_next(s32* l, s32* r);
    u32 rvb_addr(u32 in) const;
    s16 rvb_rd(u16 reg, s32 extra) const;
    void rvb_wr(u16 reg, s16 v);
    void rvb_side(int lr, s32 in);
    void rvb_tick(const s32 in[2], s32 out[2]);
    void rvb_reset();
    void rvb_set_base(u16 v);
    void apply_voice_attr(u32 p, int v, u32 mask);
    void store_voice_attr(u32 p, int v);
    void transfer_done();
    void rev_lib_reset();
    bool block_overlaps(u32 a, u32 size) const;
    bool reverb_area_free() const;
    bool reverb_blocks(u32 a, u32 size) const;
    s32 heap_alloc(u32 from, u32 size);
    bool rev_area_allocated(u32 start_units) const;
    void set_reverb_attr(u32 mask, const u16* regs);
    int clear_work_area(u32 mode);
    static u32 reverb_work_start(int mode);
    static u32 reverb_work_size(int mode);

    sim::Memory& m;
    std::vector<u8> ram_;

    Chan chan_[kVoices];
    u32 repeat_addr_[kVoices] = {};
    u8 ignore_loop_start_[kVoices] = {};
    s32 master_[2] = {0x3FFF, 0x3FFF};
    s16 master_mode_[2] = {0, 0};
    s32 master_cur_[2] = {0x3FFF, 0x3FFF};
    s32 master_cyc_[2] = {1, 1};
    int muted_ = 0;
    s32 cd_vol_[2] = {0x7FFF, 0x7FFF};
    int cd_reverb_on_ = 0, cd_mix_on_ = 1;
    static constexpr u32 kCdFifoFrames = 32768;
    std::vector<s16> cd_fifo_;
    u32 cd_rd_ = 0, cd_wr_ = 0;
    u32 noise_voices_ = 0, reverb_voices_ = 0, lfo_voices_ = 0;
    int noise_clock_ = 0;
    u16 noise_lfsr_ = 0x8000;
    s32 noise_timer_ = 0;
    u32 decoded_pos_ = 0;
    u32 field_acc_ = 0;
    u16 rvb_reg_[32] = {};
    u16 rvb_mbase_ = 0;
    u32 rvb_wa_ = 0, rvb_cur_ = 0;
    s32 rvb_vol_[2] = {};
    int rvb_enable_ = 0;
    s16 rvb_down_[2][128] = {};
    s16 rvb_up_[2][64] = {};
    u32 rvb_pos_ = 0;

    Voice voice_[kVoices];
    u32 transfer_addr_ = 0x1010, transfer_mode_ = 0;
    bool ready_ = false;
    u32 transfer_callback_fn_ = 0;
    s16 mvol_[2] = {0x3FFF, 0x3FFF}, mvolmode_[2] = {0, 0}, cdvol_[2] = {0x7FFF, 0x7FFF};
    struct Block {
        u32 addr = 0, size = 0;
        int used = 0;
    };
    Block heap_[64];
    int heap_limit_ = 0;
    int rev_flag_ = 0, rev_reserved_ = 0;
    u32 rev_offsetaddr_ = 0xFFFE;
    int rev_mode_ = 0;
    s16 rev_depth_[2] = {};
    s32 rev_delay_ = 0, rev_feedback_ = 0;
};
}
