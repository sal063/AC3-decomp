#pragma once

#include "core/types.hpp"

namespace ac3::sim {
class PreciseCache;
struct PreciseVertex;

class Gte {
public:
    PreciseCache* precise = nullptr;
    s32 x_squeeze = 0x10000;
    bool project_exact(const double view[3], PreciseVertex& out) const;
    struct Exact {
        float sx = 0, sy = 0, vz = 0;
        float vx = 0, vy = 0;
        bool ok = false;
    };
    Exact exact_fifo[3];

    u32 mfc2(u32 reg) const;
    void mtc2(u32 reg, u32 value);
    u32 cfc2(u32 reg) const;
    void ctc2(u32 reg, u32 value);

    void op(u32 command);

    void save(u32 out[64]) const;
    void load(const u32 in[64]);

private:
    s16 v_[3][3] = {};
    u32 rgbc_ = 0;
    u16 otz_ = 0;
    s16 ir_[4] = {};
    s16 sxy_[3][2] = {};
    u16 sz_[4] = {};
    u32 rgb_[3] = {};
    u32 res1_ = 0;
    s32 mac_[4] = {};
    s32 lzcs_ = 0, lzcr_ = 0;
    s16 rt_[3][3] = {};
    s32 tr_[3] = {};
    s16 llm_[3][3] = {};
    s32 bk_[3] = {};
    s16 lcm_[3][3] = {};
    s32 fc_[3] = {};
    s32 ofx_ = 0, ofy_ = 0;
    u16 h_ = 0;
    s16 dqa_ = 0;
    s32 dqb_ = 0;
    s16 zsf3_ = 0, zsf4_ = 0;
    u32 flag_ = 0;

    s64 chk_mac(int n, s64 v);
    s32 chk_mac0(s64 v);
    s16 chk_ir(int n, s32 v, bool lm);
    s16 chk_ir0(s32 v);
    u16 chk_sz3(s32 v);
    s16 chk_sxy(s32 v, bool y);
    u8 chk_col(s32 v, int i);
    void push_sz(s32 v);
    void push_sxy(s32 x, s32 y);
    void push_rgb_from_mac();
    u32 divide(u16 h, u16 sz3);
    void set_mac_ir(int n, s64 v, int shift, bool lm);
    void mat_vec(const s16 m[3][3], const s32 t[3], const s16 vec[3], int shift, bool lm);
    void interpolate(s64 m1, s64 m2, s64 m3, int shift, bool lm);
    void from_rgb(s64& r, s64& g, s64& b) const;
    void rtp(int vi, int shift, bool lm, bool last);
    void record_precise(const s64 view[3]);
    void light(int vi, int shift, bool lm, int mode);
};
}
