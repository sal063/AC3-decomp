#include "machine/spu.hpp"

#include "machine/state_reader.hpp"
#include "sim/memory.hpp"

#include <cstring>

namespace ac3::machine {
namespace {
constexpr int kVmDirect = 0, kVmLinIncN = 1, kVmLinIncR = 2, kVmLinDecN = 3, kVmLinDecR = 4, kVmExpIncN = 5,
              kVmExpIncR = 6, kVmExpDec = 7;
constexpr s32 kVolMax = 0x3FFF;
enum { kEnvOff = 0, kEnvAttack, kEnvDecay, kEnvSustain, kEnvRelease };

constexpr u32 kFieldNum = 44100u * 1001u, kFieldDen = 60000u;

constexpr u8 kLoopEnd = 0x01, kSustain = 0x02, kLoopStart = 0x04;
constexpr u32 kAddrMask = Spu::kRamSize - 16u;
constexpr u32 kDecodedSize = 0x200;

constexpr u32 VA_VOICE = 0, VA_MASK = 4, VA_VOL_L = 8, VA_VOL_R = 10, VA_VOLMODE_L = 12, VA_VOLMODE_R = 14,
              VA_VOLX_L = 16, VA_VOLX_R = 18, VA_PITCH = 20, VA_NOTE = 22, VA_SAMPLE_NOTE = 24, VA_ENVX = 26,
              VA_ADDR = 28, VA_LOOP_ADDR = 32, VA_A_MODE = 36, VA_S_MODE = 40, VA_R_MODE = 44, VA_AR = 48,
              VA_DR = 50, VA_SR = 52, VA_RR = 54, VA_SL = 56, VA_ADSR1 = 58, VA_ADSR2 = 60;
constexpr u32 SV_VOLL = 1u << 0, SV_VOLR = 1u << 1, SV_VOLMODEL = 1u << 2, SV_VOLMODER = 1u << 3,
              SV_PITCH = 1u << 4, SV_NOTE = 1u << 5, SV_SAMPLE_NOTE = 1u << 6, SV_WDSA = 1u << 7,
              SV_AMODE = 1u << 8, SV_SMODE = 1u << 9, SV_RMODE = 1u << 10, SV_AR = 1u << 11, SV_DR = 1u << 12,
              SV_SR = 1u << 13, SV_RR = 1u << 14, SV_SL = 1u << 15, SV_LSAX = 1u << 16, SV_ADSR1 = 1u << 17,
              SV_ADSR2 = 1u << 18;
constexpr u32 CA_MASK = 0, CA_MVOL_L = 4, CA_MVOL_R = 6, CA_MVOLMODE_L = 8, CA_MVOLMODE_R = 10, CA_MVOLX_L = 12,
              CA_MVOLX_R = 14, CA_CD_VOL_L = 16, CA_CD_VOL_R = 18, CA_CD_REVERB = 20, CA_CD_MIX = 24;
constexpr u32 SC_MVOLL = 1u << 0, SC_MVOLR = 1u << 1, SC_MVOLMODEL = 1u << 2, SC_MVOLMODER = 1u << 3,
              SC_CDVOLL = 1u << 6, SC_CDVOLR = 1u << 7, SC_CDREV = 1u << 8, SC_CDMIX = 1u << 9;
constexpr u32 RA_MASK = 0, RA_MODE = 4, RA_DEPTH_L = 8, RA_DEPTH_R = 10, RA_DELAY = 12, RA_FEEDBACK = 16;
constexpr u32 SR_MODE = 1u << 0, SR_DEPTHL = 1u << 1, SR_DEPTHR = 1u << 2, SR_DELAYTIME = 1u << 3,
              SR_FEEDBACK = 1u << 4;
constexpr s32 kLibOff = 0, kLibOn = 1, kLibCdOnly = 5, kLibVoiceOnly = 6, kLibBit = 8;
constexpr s32 kSpuError = -1, kSpuDiag = -2, kSpuInvalidArgs = -3, kSpuSuccess = 0;
constexpr int kRevModeMax = 10, kRevEcho = 7, kRevDelay = 8;
constexpr u32 kRevClearWa = 0x100;
constexpr u32 kHeapLo = 0x01010;

u32 adsr_ar(u16 a1) { return (a1 >> 8) & 0x7Fu; }
u32 adsr_amode(u16 a1) { return (a1 >> 15) & 1u; }
u32 adsr_dr(u16 a1) { return (a1 >> 4) & 0x0Fu; }
u32 adsr_sl(u16 a1) { return a1 & 0x0Fu; }
u32 adsr_sr(u16 a2) { return (a2 >> 6) & 0x7Fu; }
u32 adsr_smode(u16 a2) { return (a2 >> 15) & 1u; }
u32 adsr_sdir(u16 a2) { return (a2 >> 14) & 1u; }
u32 adsr_rr(u16 a2) { return a2 & 0x1Fu; }
u32 adsr_rmode(u16 a2) { return (a2 >> 5) & 1u; }

s32 clamp16(s32 v) { return v > 32767 ? 32767 : (v < -32768 ? -32768 : v); }
s16 rvb_sat(s32 v) { return static_cast<s16>(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); }
s32 rvb_neg(s32 v) { return v == -32768 ? 0x7FFF : -v; }

const s16 kGauss[512] = {
#include "machine/spu_gauss.inc"
};

const int kAdpcmF0[16] = {0, 60, 115, 98, 122};
const int kAdpcmF1[16] = {0, 0, -52, -55, -60};

const s16 kRvbFir[20] = {-1, 2, -10, 35, -103, 266, -616, 1332, -2960, 10246,
                         10246, -2960, 1332, -616, 266, -103, 35, -10, 2, -1};

s32 rvb_downsample(const s16* src) {
    s32 out = 0;
    for (int i = 0; i < 20; i++) out += kRvbFir[i] * src[i * 2];
    out += 0x4000 * src[19];
    return rvb_sat(out >> 15);
}

s32 rvb_upsample(const s16* src) {
    s32 out = 0;
    for (int i = 0; i < 20; i++) out += kRvbFir[i] * src[i];
    return rvb_sat(out >> 14);
}

s32 rvb_iiasm(s16 alpha, s16 insamp) {
    if (alpha == -32768) return insamp == -32768 ? 0 : insamp * -65536;
    return insamp * (32768 - alpha);
}

struct Rate {
    s32 step, cycles;
};

Rate rate_of(int rate, int increase, int expo, s32 level) {
    Rate o;
    const int shift = (rate >> 2) & 31;
    const int sel = rate & 3;
    s32 step = increase ? (7 - sel) : (-8 + sel);
    o.cycles = 1;
    if (shift > 11) o.cycles = 1 << (shift - 11);
    else step <<= (11 - shift);
    if (expo) {
        if (increase) {
            if (level > 0x6000) o.cycles *= 4;
        } else {
            step = static_cast<s32>((static_cast<s64>(step) * static_cast<s64>(level)) >> 15);
            if (step == 0) step = -1;
        }
    }
    o.step = step;
    return o;
}

void env_delta(int zs, int speed, int expo, int dec, int inv, s32 level, s32* inc, s32* div_inc) {
    *inc = 7 - (speed & 3);
    if (inv) *inc = ~*inc;
    *div_inc = 32768;
    if (speed < 0x2C) *inc = static_cast<s32>(static_cast<u32>(*inc) << ((0x2F - speed) >> 2));
    if (speed >= 0x30) *div_inc >>= (speed - 0x2C) >> 2;
    if (expo) {
        if (dec) {
            *inc = (level * *inc) >> 15;
        } else if ((level & 0x7FFF) >= 0x6000) {
            if (speed < 0x28) *inc >>= 2;
            else if (speed >= 0x2C) *div_inc >>= 2;
            else {
                *inc >>= 1;
                *div_inc >>= 1;
            }
        }
    }
    if (*div_inc == 0 && speed < zs) *div_inc = 1;
}

const u16 kN2pSemitone[12] = {32768, 34716, 36780, 38967, 41285, 43740, 46340, 49096, 52015, 55108, 58385, 61857};
const u16 kN2pFine[128] = {
    32768, 32782, 32797, 32812, 32827, 32842, 32856, 32871, 32886, 32901, 32916, 32931, 32945, 32960, 32975, 32990,
    33005, 33020, 33035, 33050, 33065, 33080, 33094, 33109, 33124, 33139, 33154, 33169, 33184, 33199, 33214, 33229,
    33244, 33259, 33274, 33289, 33304, 33319, 33334, 33349, 33364, 33379, 33394, 33410, 33425, 33440, 33455, 33470,
    33485, 33500, 33515, 33530, 33546, 33561, 33576, 33591, 33606, 33621, 33636, 33652, 33667, 33682, 33697, 33712,
    33728, 33743, 33758, 33773, 33789, 33804, 33819, 33834, 33850, 33865, 33880, 33896, 33911, 33926, 33941, 33957,
    33972, 33987, 34003, 34018, 34033, 34049, 34064, 34080, 34095, 34110, 34126, 34141, 34157, 34172, 34187, 34203,
    34218, 34234, 34249, 34265, 34280, 34296, 34311, 34327, 34342, 34358, 34373, 34389, 34404, 34420, 34435, 34451,
    34466, 34482, 34497, 34513, 34528, 34544, 34560, 34575, 34591, 34606, 34622, 34638, 34653, 34669, 34685, 34700,
};

const u16 kRevStart[kRevModeMax] = {0xFFFE, 0xFB28, 0xFC18, 0xF6F8, 0xF204, 0xEA44, 0xE128, 0xCFF8, 0xCFF8, 0xF880};
const u16 kRevPreset[kRevModeMax][32] = {
    {0},
    {0x007D, 0x005B, 0x6D80, 0x54B8, 0xBED0, 0x0000, 0x0000, 0xBA80, 0x5800, 0x5300, 0x04D6, 0x0333, 0x03F0, 0x0227,
     0x0374, 0x01EF, 0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01B4, 0x0136,
     0x00B8, 0x005C, 0x8000, 0x8000},
    {0x0033, 0x0025, 0x70F0, 0x4FA8, 0xBCE0, 0x4410, 0xC0F0, 0x9C00, 0x5280, 0x4EC0, 0x03E4, 0x031B, 0x03A4, 0x02AF,
     0x0372, 0x0266, 0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135, 0x01D2, 0x00B7, 0x018F, 0x00B5, 0x00B4, 0x0080,
     0x004C, 0x0026, 0x8000, 0x8000},
    {0x00B1, 0x007F, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xB4C0, 0x5280, 0x4EC0, 0x0904, 0x076B, 0x0824, 0x065F,
     0x07A2, 0x0616, 0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305, 0x0462, 0x02B7, 0x042F, 0x0265, 0x0264, 0x01B2,
     0x0100, 0x0080, 0x8000, 0x8000},
    {0x00E3, 0x00A9, 0x6F60, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0xA680, 0x5680, 0x52C0, 0x0DFB, 0x0B58, 0x0D09, 0x0A3C,
     0x0BD9, 0x0973, 0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0, 0x06EF, 0x03D2, 0x05EA, 0x031D, 0x031C, 0x0238,
     0x0154, 0x00AA, 0x8000, 0x8000},
    {0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, 0xB800, 0xBC00, 0xC000, 0x6000, 0x5C00, 0x15BA, 0x11BB, 0x14C2, 0x10BD,
     0x11BC, 0x0DC1, 0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1, 0x0A00, 0x06CD, 0x09C2, 0x05C1, 0x05C0, 0x041A,
     0x0274, 0x013A, 0x8000, 0x8000},
    {0x033D, 0x0231, 0x7E00, 0x5000, 0xB400, 0xB000, 0x4C00, 0xB000, 0x6000, 0x5400, 0x1ED6, 0x1A31, 0x1D14, 0x183B,
     0x1BC2, 0x16B2, 0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D, 0x11F6, 0x0C5D, 0x1056, 0x0AE1, 0x0AE0, 0x07A2,
     0x0464, 0x0232, 0x8000, 0x8000},
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x8100, 0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005,
     0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002,
     0x0004, 0x0002, 0x8000, 0x8000},
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FFF, 0x0FFF, 0x1005, 0x0005,
     0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002,
     0x0004, 0x0002, 0x8000, 0x8000},
    {0x0017, 0x0013, 0x70F0, 0x4FA8, 0xBCE0, 0x4510, 0xBEF0, 0x8500, 0x5F80, 0x54C0, 0x0371, 0x02AF, 0x02E5, 0x01DF,
     0x02B0, 0x01D7, 0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1, 0x011F, 0x0059, 0x01A0, 0x00E3, 0x0058, 0x0040,
     0x0028, 0x0014, 0x8000, 0x8000},
};

enum {
    R_FB_SRC_A = 0, R_FB_SRC_B = 1, R_IIR_ALPHA = 2, R_ACC_COEF_A = 3, R_ACC_COEF_B = 4, R_ACC_COEF_C = 5,
    R_ACC_COEF_D = 6, R_IIR_COEF = 7, R_FB_ALPHA = 8, R_FB_X = 9, R_IIR_DEST_A = 10, R_ACC_SRC_A = 12,
    R_ACC_SRC_B = 14, R_IIR_SRC_A = 16, R_IIR_DEST_B = 18, R_ACC_SRC_C = 20, R_ACC_SRC_D = 22, R_IIR_SRC_B = 24,
    R_MIX_DEST_A = 26, R_MIX_DEST_B = 28, R_IN_COEF = 30
};

u32 apply_voice_switch(u32 cur, s32 on_off, u32 bits) {
    switch (on_off) {
    case kLibOn: return (cur | bits) & 0xFFFFFFu;
    case kLibOff: return (cur & ~bits) & 0xFFFFFFu;
    case kLibBit: return bits & 0xFFFFFFu;
    default: return cur;
    }
}

bool mode_is_exponential(s32 mode) { return mode == kVmExpIncN || mode == kVmExpIncR || mode == kVmExpDec; }
bool mode_is_decreasing(s32 mode) { return mode == kVmLinDecN || mode == kVmLinDecR || mode == kVmExpDec; }
}

Spu::Spu(sim::Memory& mem) : m(mem), ram_(kRamSize), cd_fifo_(kCdFifoFrames * 2) {
    core_reset();
}

u8 Spu::decode_block(const u8* block, s16* out28, s16 hist[2]) {
    int shift = block[0] & 0x0F;
    const int filter = (block[0] >> 4) & 0x0F;
    const u8 flags = block[1];
    int nibble_mask = 0x0F;
    if (shift > 12) {
        shift = 8;
        nibble_mask = 0x08;
    }
    for (int i = 0; i < 28; i++) {
        const int nibble = (block[2 + i / 2] >> ((i & 1) ? 4 : 0)) & nibble_mask;
        s32 sample = static_cast<s32>(static_cast<s16>(static_cast<u16>(nibble << 12))) >> shift;
        sample += (static_cast<s32>(hist[1]) * kAdpcmF1[filter]) >> 6;
        sample += (static_cast<s32>(hist[0]) * kAdpcmF0[filter]) >> 6;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        out28[i] = static_cast<s16>(sample);
        hist[1] = hist[0];
        hist[0] = static_cast<s16>(sample);
    }
    return flags;
}

int Spu::field_frames() {
    field_acc_ += kFieldNum;
    const int n = static_cast<int>(field_acc_ / kFieldDen);
    field_acc_ %= kFieldDen;
    return n;
}

void Spu::decoded_write(int which, s16 v) {
    const u32 off = static_cast<u32>(which) * 0x400u + decoded_pos_ * 2u;
    if (off + 2u > kRamSize) return;
    std::memcpy(ram_.data() + off, &v, 2);
}

int Spu::block_load(Chan& c, int v) {
    c.cur &= kAddrMask;
    c.flags = decode_block(ram_.data() + c.cur, c.dec, c.hist);
    if ((c.flags & kLoopStart) && !ignore_loop_start_[v]) repeat_addr_[v] = c.cur;
    c.idx = 0;
    c.have_block = 1;
    return 1;
}

int Spu::voice_pull(Chan& c, int v) {
    if (!c.have_block || c.idx >= 28) {
        if (c.have_block) {
            if (c.flags & kLoopEnd) {
                c.cur = repeat_addr_[v];
                if (!(c.flags & kSustain)) {
                    c.ended = 1;
                    c.env = 0;
                    c.phase = kEnvOff;
                    return 0;
                }
            } else {
                c.cur += 16u;
            }
        }
        if (!block_load(c, v)) {
            c.ended = 1;
            c.env = 0;
            c.phase = kEnvOff;
            return 0;
        }
    }
    c.win[0] = c.win[1];
    c.win[1] = c.win[2];
    c.win[2] = c.win[3];
    c.win[3] = c.dec[c.idx++];
    return 1;
}

s32 Spu::gauss_interp(const Chan& c) const {
    const int i = static_cast<int>((c.counter >> 4) & 0xFFu);
    const s32 out = static_cast<s32>(kGauss[0x0FF - i]) * c.win[0] + static_cast<s32>(kGauss[0x1FF - i]) * c.win[1] +
                    static_cast<s32>(kGauss[0x100 + i]) * c.win[2] + static_cast<s32>(kGauss[0x000 + i]) * c.win[3];
    return out >> 15;
}

s16 Spu::noise_next() {
    s32 period = 0x8000 >> (noise_clock_ >> 2);
    if (period < 1) period = 1;
    noise_timer_ -= 0x8000;
    while (noise_timer_ <= 0) {
        noise_timer_ += period * 0x100;
        const u16 bit =
            static_cast<u16>(((noise_lfsr_ >> 15) ^ (noise_lfsr_ >> 14) ^ (noise_lfsr_ >> 12) ^ (noise_lfsr_ >> 3)) & 1u);
        noise_lfsr_ = static_cast<u16>((noise_lfsr_ << 1) | bit);
    }
    return static_cast<s16>(noise_lfsr_);
}

u32 Spu::rvb_addr(u32 in) const {
    u32 off = rvb_cur_ + (in & 0x3FFFFu);
    off += rvb_wa_ & (0u - ((off >> 18) & 1u));
    return off & 0x3FFFFu;
}

s16 Spu::rvb_rd(u16 reg, s32 extra) const {
    s16 v;
    std::memcpy(&v, ram_.data() + rvb_addr((static_cast<u32>(reg) << 2) + static_cast<u32>(extra)) * 2u, 2);
    return v;
}

void Spu::rvb_wr(u16 reg, s16 v) {
    if (!rvb_enable_) return;
    std::memcpy(ram_.data() + rvb_addr(static_cast<u32>(reg) << 2) * 2u, &v, 2);
}

void Spu::rvb_side(int lr, s32 in) {
    auto S = [&](int i) { return static_cast<s32>(static_cast<s16>(rvb_reg_[i])); };
    const s32 in_coef = S(R_IN_COEF + lr);
    const s32 iir_coef = S(R_IIR_COEF);
    const s16 alpha = static_cast<s16>(rvb_reg_[R_IIR_ALPHA]);
    const s16 iir_in_a =
        rvb_sat((((rvb_rd(rvb_reg_[R_IIR_SRC_A + lr], 0) * iir_coef) >> 14) + ((in * in_coef) >> 14)) >> 1);
    const s16 iir_in_b =
        rvb_sat((((rvb_rd(rvb_reg_[R_IIR_SRC_B + (lr ^ 1)], 0) * iir_coef) >> 14) + ((in * in_coef) >> 14)) >> 1);
    const s16 iir_a =
        rvb_sat((((iir_in_a * alpha) >> 14) + (rvb_iiasm(alpha, rvb_rd(rvb_reg_[R_IIR_DEST_A + lr], -1)) >> 14)) >> 1);
    const s16 iir_b =
        rvb_sat((((iir_in_b * alpha) >> 14) + (rvb_iiasm(alpha, rvb_rd(rvb_reg_[R_IIR_DEST_B + lr], -1)) >> 14)) >> 1);
    rvb_wr(rvb_reg_[R_IIR_DEST_A + lr], iir_a);
    rvb_wr(rvb_reg_[R_IIR_DEST_B + lr], iir_b);
    const s32 acc = ((rvb_rd(rvb_reg_[R_ACC_SRC_A + lr], 0) * S(R_ACC_COEF_A)) >> 14) +
                    ((rvb_rd(rvb_reg_[R_ACC_SRC_B + lr], 0) * S(R_ACC_COEF_B)) >> 14) +
                    ((rvb_rd(rvb_reg_[R_ACC_SRC_C + lr], 0) * S(R_ACC_COEF_C)) >> 14) +
                    ((rvb_rd(rvb_reg_[R_ACC_SRC_D + lr], 0) * S(R_ACC_COEF_D)) >> 14);
    const s32 fb_alpha = S(R_FB_ALPHA), fb_x = S(R_FB_X);
    const s16 fb_a = rvb_rd(static_cast<u16>(rvb_reg_[R_MIX_DEST_A + lr] - rvb_reg_[R_FB_SRC_A]), 0);
    const s16 fb_b = rvb_rd(static_cast<u16>(rvb_reg_[R_MIX_DEST_B + lr] - rvb_reg_[R_FB_SRC_B]), 0);
    const s16 mda = rvb_sat((acc + ((fb_a * rvb_neg(fb_alpha)) >> 14)) >> 1);
    const s16 mdb = rvb_sat(fb_a + ((((mda * fb_alpha) >> 14) + ((fb_b * rvb_neg(fb_x)) >> 14)) >> 1));
    const s16 ivb = rvb_sat(fb_b + ((mdb * fb_x) >> 15));
    rvb_wr(rvb_reg_[R_MIX_DEST_A + lr], mda);
    rvb_wr(rvb_reg_[R_MIX_DEST_B + lr], mdb);
    rvb_up_[lr][(rvb_pos_ >> 1) | 0x20] = rvb_up_[lr][rvb_pos_ >> 1] = ivb;
}

void Spu::rvb_tick(const s32 in[2], s32 out[2]) {
    for (int lr = 0; lr < 2; lr++) rvb_down_[lr][rvb_pos_] = rvb_down_[lr][rvb_pos_ | 0x40] = static_cast<s16>(in[lr]);
    if (rvb_pos_ & 1) {
        rvb_side(0, rvb_downsample(&rvb_down_[0][(rvb_pos_ - 38) & 0x3F]));
    } else {
        rvb_side(1, rvb_downsample(&rvb_down_[1][(rvb_pos_ - 38) & 0x3F]));
        rvb_cur_ = (rvb_cur_ + 1) & 0x3FFFFu;
        if (!rvb_cur_) rvb_cur_ = rvb_wa_;
    }
    const s16* l = &rvb_up_[0][((rvb_pos_ >> 1) - 19) & 0x1F];
    const s16* r = &rvb_up_[1][((rvb_pos_ >> 1) - 19) & 0x1F];
    if (rvb_pos_ & 1) {
        out[0] = rvb_upsample(l);
        out[1] = r[9];
    } else {
        out[0] = l[9];
        out[1] = rvb_upsample(r);
    }
    rvb_pos_ = (rvb_pos_ + 1) & 0x3Fu;
}

void Spu::rvb_reset() {
    std::memset(rvb_reg_, 0, sizeof rvb_reg_);
    std::memset(rvb_down_, 0, sizeof rvb_down_);
    std::memset(rvb_up_, 0, sizeof rvb_up_);
    rvb_vol_[0] = rvb_vol_[1] = 0;
    rvb_enable_ = 0;
    rvb_pos_ = 0;
    rvb_mbase_ = 0;
    rvb_wa_ = 0;
    rvb_cur_ = 0;
}

void Spu::rvb_set_base(u16 v) {
    rvb_mbase_ = v;
    rvb_wa_ = (static_cast<u32>(v) << 2) & 0x3FFFFu;
    rvb_cur_ = rvb_wa_;
}

void Spu::env_tick(Chan& c, const Voice& v) {
    if (c.phase == kEnvOff) return;
    if (c.phase == kEnvAttack && c.env == 0x7FFF) c.phase = kEnvDecay;
    s32 inc = 0, div_inc = 0;
    u16 reset = 0;
    const int sdec = static_cast<int>(adsr_sdir(v.adsr2));
    switch (c.phase) {
    case kEnvAttack:
        env_delta(0x7F, static_cast<int>(adsr_ar(v.adsr1)), static_cast<int>(adsr_amode(v.adsr1)), 0, 0, c.env, &inc,
                  &div_inc);
        reset = 0x7FFF;
        break;
    case kEnvDecay:
        env_delta(0x1F << 2, static_cast<int>(adsr_dr(v.adsr1)) << 2, 1, 1, 1, c.env, &inc, &div_inc);
        reset = 0;
        break;
    case kEnvSustain:
        env_delta(0x7F, static_cast<int>(adsr_sr(v.adsr2)), static_cast<int>(adsr_smode(v.adsr2)), sdec, sdec, c.env,
                  &inc, &div_inc);
        reset = sdec ? 0 : 0x7FFF;
        break;
    case kEnvRelease:
        env_delta(0x1F << 2, static_cast<int>(adsr_rr(v.adsr2)) << 2, static_cast<int>(adsr_rmode(v.adsr2)), 1, 1,
                  c.env, &inc, &div_inc);
        reset = 0;
        break;
    default: return;
    }
    c.env_div += static_cast<u32>(div_inc);
    if (!(c.env_div & 0x8000u)) return;
    c.env_div = 0;
    const u16 prev = static_cast<u16>(c.env);
    u16 next = static_cast<u16>(prev + inc);
    if (c.phase == kEnvAttack) {
        if ((prev ^ next) & next & 0x8000u) next = reset;
    } else if (next & 0x8000u) {
        next = reset;
    }
    c.env = next;
    if (c.phase == kEnvDecay && next < static_cast<u16>((static_cast<s32>(adsr_sl(v.adsr1)) + 1) << 11))
        c.phase = kEnvSustain;
    if (c.phase == kEnvRelease && next == 0) c.phase = kEnvOff;
}

void Spu::sweep_tick(s32* level, s32* cyc, int mode, s32 rate) {
    if (mode == kVmDirect) return;
    if (--*cyc > 0) return;
    const int increase = (mode == kVmLinIncN || mode == kVmLinIncR || mode == kVmExpIncN || mode == kVmExpIncR);
    const int expo = (mode >= kVmExpIncN);
    const int neg = (*level < 0);
    s32 mag = neg ? -*level : *level;
    const Rate r = rate_of(static_cast<int>(rate & 0x7F), increase, expo, mag * 2);
    mag += r.step;
    if (mag > kVolMax) mag = kVolMax;
    if (mag < 0) mag = 0;
    *cyc = r.cycles;
    *level = neg ? -mag : mag;
}

void Spu::core_reset() {
    for (auto& c : chan_) c = Chan{};
    std::memset(repeat_addr_, 0, sizeof repeat_addr_);
    std::memset(ignore_loop_start_, 0, sizeof ignore_loop_start_);
    for (auto& c : chan_) {
        c.phase = kEnvOff;
        c.ended = 1;
    }
    master_[0] = master_[1] = 0x3FFF;
    master_cur_[0] = master_cur_[1] = 0x3FFF;
    master_mode_[0] = master_mode_[1] = kVmDirect;
    master_cyc_[0] = master_cyc_[1] = 1;
    muted_ = 0;
    cd_vol_[0] = cd_vol_[1] = 0x7FFF;
    cd_reverb_on_ = 0;
    cd_mix_on_ = 1;
    cd_rd_ = cd_wr_ = 0;
    noise_voices_ = reverb_voices_ = lfo_voices_ = 0;
    noise_clock_ = 0;
    noise_lfsr_ = 0x8000;
    noise_timer_ = 0;
    rvb_reset();
    std::memset(ram_.data(), 0, 0x1000u);
    decoded_pos_ = 0;
}

void Spu::key_on(int v) {
    Chan& c = chan_[v];
    const Voice& a = voice_[v];
    c = Chan{};
    ignore_loop_start_[v] = 0;
    c.cur = a.start_addr & kAddrMask;
    c.phase = kEnvAttack;
    c.env = 0;
    c.vol[0] = (a.volmode_left == kVmDirect) ? a.vol_left : 0;
    c.vol[1] = (a.volmode_right == kVmDirect) ? a.vol_right : 0;
    c.vol_cyc[0] = c.vol_cyc[1] = 1;
    c.ended = 0;
    voice_pull(c, v);
}

void Spu::key_off(int v) {
    if (chan_[v].phase == kEnvOff) return;
    chan_[v].phase = kEnvRelease;
    chan_[v].env_div = 0;
}

void Spu::set_loop_addr(int v, u32 addr) {
    repeat_addr_[v] = addr & kAddrMask;
    if (chan_[v].phase != kEnvOff) ignore_loop_start_[v] = 1;
}

void Spu::attr_changed(int v) {
    const Voice& a = voice_[v];
    if (a.volmode_left == kVmDirect) chan_[v].vol[0] = a.vol_left;
    if (a.volmode_right == kVmDirect) chan_[v].vol[1] = a.vol_right;
}

int Spu::key_status(int v) const {
    if (v < 0 || v >= kVoices) return kOff;
    const int on = voice_[v].keyed_on;
    const int env = chan_[v].env != 0;
    if (on) return env ? kOn : kOnEnvOff;
    return env ? kOffEnvOn : kOff;
}

void Spu::set_master(s16 l, s16 r, s16 ml, s16 mr) {
    master_[0] = l;
    master_[1] = r;
    master_mode_[0] = ml;
    master_mode_[1] = mr;
    if (ml == kVmDirect) master_cur_[0] = l;
    if (mr == kVmDirect) master_cur_[1] = r;
    if (master_cyc_[0] < 1) master_cyc_[0] = 1;
    if (master_cyc_[1] < 1) master_cyc_[1] = 1;
}

u16 Spu::note_to_pitch(u16 note, u16 sample_note) {
    const u32 fine = (static_cast<u32>(note & 0xFF) + static_cast<u32>(sample_note & 0xFF)) & 0xFFFFu;
    const s32 semis = static_cast<s16>((note >> 8) + (fine >> 7) - (sample_note >> 8));
    const s32 octave = semis / 12;
    s32 shift = octave - 2;
    s32 semi = semis - octave * 12;
    if (semi < 0) {
        semi += 12;
        shift = octave - 3;
    }
    const u32 p = (static_cast<u32>(kN2pSemitone[semi]) * static_cast<u32>(kN2pFine[fine & 0x7F])) >> 16;
    if (shift >= 0) return 0x3FFF;
    const u32 n = static_cast<u32>(-shift);
    return static_cast<u16>((p + (1u << (n - 1))) >> n);
}

void Spu::cd_push(const s16* interleaved, int nframes) {
    for (int i = 0; i < nframes; i++) {
        if (cd_wr_ - cd_rd_ >= kCdFifoFrames) cd_rd_ = cd_wr_ - (kCdFifoFrames - 1u);
        const u32 s = (cd_wr_ % kCdFifoFrames) * 2u;
        cd_fifo_[s + 0] = interleaved[i * 2 + 0];
        cd_fifo_[s + 1] = interleaved[i * 2 + 1];
        cd_wr_++;
    }
}

void Spu::cd_next(s32* l, s32* r) {
    if (cd_rd_ == cd_wr_) {
        *l = 0;
        *r = 0;
        return;
    }
    const u32 s = (cd_rd_ % kCdFifoFrames) * 2u;
    *l = cd_fifo_[s + 0];
    *r = cd_fifo_[s + 1];
    cd_rd_++;
}

void Spu::mix(s16* out, int nframes) {
    for (int f = 0; f < nframes; f++) {
        s32 dry[2] = {0, 0}, wet_in[2] = {0, 0};
        const s32 noise = noise_voices_ ? noise_next() : 0;
        s32 prev_out = 0;
        for (int v = 0; v < kVoices; v++) {
            Chan& c = chan_[v];
            const Voice& a = voice_[v];
            if (c.phase == kEnvOff) {
                c.out = 0;
                prev_out = 0;
                if (v == 1) decoded_write(2, 0);
                else if (v == 3) decoded_write(3, 0);
                continue;
            }
            const int is_noise = static_cast<int>((noise_voices_ >> v) & 1u);
            const s32 raw = is_noise ? noise : gauss_interp(c);
            const s32 shaped = (raw * c.env) >> 15;
            c.out = shaped;
            const s32 l = (shaped * c.vol[0]) >> 14;
            const s32 r = (shaped * c.vol[1]) >> 14;
            dry[0] += l;
            dry[1] += r;
            if (reverb_voices_ & (1u << v)) {
                wet_in[0] += l;
                wet_in[1] += r;
            }
            if (v == 1) decoded_write(2, static_cast<s16>(clamp16(shaped)));
            else if (v == 3) decoded_write(3, static_cast<s16>(clamp16(shaped)));
            sweep_tick(&c.vol[0], &c.vol_cyc[0], a.volmode_left, a.vol_left);
            sweep_tick(&c.vol[1], &c.vol_cyc[1], a.volmode_right, a.vol_right);
            env_tick(c, a);
            if (!is_noise) {
                u32 step = a.pitch;
                if ((lfo_voices_ & (1u << v)) && v > 0)
                    step = static_cast<u32>(static_cast<s32>(step) +
                                            ((static_cast<s32>(static_cast<s16>(step)) * prev_out) >> 15));
                if (step > 0x3FFFu) step = 0x3FFFu;
                c.counter += step;
                while (c.counter >= 0x1000u) {
                    c.counter -= 0x1000u;
                    if (!voice_pull(c, v)) break;
                }
            }
            prev_out = shaped;
        }
        if (muted_) {
            dry[0] = dry[1] = 0;
            wet_in[0] = wet_in[1] = 0;
        }
        s32 cl = 0, cr = 0;
        cd_next(&cl, &cr);
        decoded_write(0, static_cast<s16>(clamp16(cl)));
        decoded_write(1, static_cast<s16>(clamp16(cr)));
        if (cd_mix_on_) {
            const s32 gl = (cl * cd_vol_[0]) >> 15;
            const s32 gr = (cr * cd_vol_[1]) >> 15;
            dry[0] += gl;
            dry[1] += gr;
            if (cd_reverb_on_) {
                wet_in[0] += gl;
                wet_in[1] += gr;
            }
        }
        {
            const s32 rin[2] = {clamp16(wet_in[0]), clamp16(wet_in[1])};
            s32 rout[2];
            rvb_tick(rin, rout);
            for (int ch = 0; ch < 2; ch++) dry[ch] = clamp16(dry[ch] + ((rout[ch] * rvb_vol_[ch]) >> 15));
        }
        sweep_tick(&master_cur_[0], &master_cyc_[0], master_mode_[0], master_[0]);
        sweep_tick(&master_cur_[1], &master_cyc_[1], master_mode_[1], master_[1]);
        out[f * 2 + 0] = static_cast<s16>(clamp16((dry[0] * master_cur_[0]) >> 14));
        out[f * 2 + 1] = static_cast<s16>(clamp16((dry[1] * master_cur_[1]) >> 14));
        decoded_pos_ = (decoded_pos_ + 1u) % kDecodedSize;
    }
}

void Spu::pump() {
    if (!ready_) return;
    int want = field_frames();
    while (want > 0) {
        s16 buf[1024 * 2];
        const int n = want > 1024 ? 1024 : want;
        mix(buf, n);
        if (submit) submit(buf, n);
        want -= n;
    }
}

void Spu::SpuInit() {
    for (auto& v : voice_) {
        v = Voice{};
        v.pitch = 0x1000;
        v.volmode_left = kVmDirect;
        v.volmode_right = kVmDirect;
    }
    transfer_addr_ = 0x1010;
    transfer_mode_ = 0;
    transfer_callback_fn_ = 0;
    ready_ = true;
    core_reset();
    rev_lib_reset();
}

void Spu::SpuInitHot() {
    for (int v = 0; v < kVoices; v++) {
        voice_[v].keyed_on = 0;
        key_off(v);
    }
    transfer_addr_ = 0x1010;
    transfer_mode_ = 0;
    transfer_callback_fn_ = 0;
    ready_ = true;
    muted_ = 0;
    rev_lib_reset();
}

void Spu::SpuQuit() {
    ready_ = false;
    for (auto& v : voice_) v.keyed_on = 0;
    core_reset();
    rev_lib_reset();
}

u32 Spu::SpuSetMute(u32 on) {
    const u32 prev = static_cast<u32>(muted_);
    muted_ = static_cast<int>(on);
    return prev;
}

u32 Spu::SpuSetTransferStartAddr(u32 a) {
    const u32 addr = a & ~7u;
    if (addr >= kRamSize) return 0;
    transfer_addr_ = addr;
    return addr;
}

void Spu::transfer_done() {
    if (transfer_callback_fn_ && call_guest) call_guest(transfer_callback_fn_);
}

u32 Spu::SpuWrite(u32 src, u32 size) {
    if (transfer_addr_ + size > kRamSize) size = kRamSize - transfer_addr_;
    const u8* g = m.ptr(src, size);
    if (!g) return 0;
    std::memcpy(ram_.data() + transfer_addr_, g, size);
    transfer_addr_ += size;
    transfer_done();
    return size;
}

u32 Spu::SpuWrite0(u32 size) {
    if (transfer_addr_ + size > kRamSize) size = kRamSize - transfer_addr_;
    std::memset(ram_.data() + transfer_addr_, 0, size);
    transfer_addr_ += size;
    transfer_done();
    return size;
}

u32 Spu::SpuRead(u32 dst, u32 size) {
    if (transfer_addr_ + size > kRamSize) size = kRamSize - transfer_addr_;
    if (u8* g = m.ptr(dst, size)) std::memcpy(g, ram_.data() + transfer_addr_, size);
    transfer_addr_ += size;
    transfer_done();
    return size;
}

u32 Spu::SpuSetTransferCallback(u32 fn) {
    const u32 prev = transfer_callback_fn_;
    transfer_callback_fn_ = fn;
    return prev;
}

u32 Spu::SpuReadDecodedData(u32 dst, s32 flag) {
    if (!dst) return 0;
    u8* g = m.ptr(dst, 0x1000u);
    if (!g) return 0;
    if (flag != kLibVoiceOnly) std::memcpy(g, ram_.data(), 0x800u);
    if (flag != kLibCdOnly) std::memcpy(g + 0x800u, ram_.data() + 0x800u, 0x800u);
    transfer_done();
    return 0;
}

void Spu::apply_voice_attr(u32 p, int v, u32 mask) {
    Voice& sv = voice_[v];
    const bool all = (mask == 0);
    if (all || (mask & SV_VOLL)) sv.vol_left = static_cast<s16>(m.r16(p + VA_VOL_L));
    if (all || (mask & SV_VOLR)) sv.vol_right = static_cast<s16>(m.r16(p + VA_VOL_R));
    if (all || (mask & SV_VOLMODEL)) sv.volmode_left = static_cast<s16>(m.r16(p + VA_VOLMODE_L));
    if (all || (mask & SV_VOLMODER)) sv.volmode_right = static_cast<s16>(m.r16(p + VA_VOLMODE_R));
    if (all || (mask & SV_PITCH)) sv.pitch = m.r16(p + VA_PITCH);
    if (all || (mask & SV_SAMPLE_NOTE)) sv.sample_note = m.r16(p + VA_SAMPLE_NOTE);
    if (all || (mask & SV_NOTE)) {
        sv.note = m.r16(p + VA_NOTE);
        sv.pitch = note_to_pitch(sv.note, sv.sample_note);
    }
    if (all || (mask & SV_WDSA)) sv.start_addr = m.r32(p + VA_ADDR);
    if (all || (mask & SV_LSAX)) {
        sv.loop_addr = m.r32(p + VA_LOOP_ADDR);
        set_loop_addr(v, sv.loop_addr);
    }
    if (all || (mask & SV_AMODE)) {
        const int expo = mode_is_exponential(static_cast<s32>(m.r32(p + VA_A_MODE)));
        sv.adsr1 = static_cast<u16>((sv.adsr1 & 0x7FFFu) | (static_cast<u16>(expo) << 15));
    }
    if (all || (mask & SV_SMODE)) {
        const s32 mode = static_cast<s32>(m.r32(p + VA_S_MODE));
        const int expo = mode_is_exponential(mode), dec = mode_is_decreasing(mode);
        sv.adsr2 = static_cast<u16>((sv.adsr2 & 0x3FFFu) | (static_cast<u16>(expo) << 15) | (static_cast<u16>(dec) << 14));
    }
    if (all || (mask & SV_RMODE)) {
        const int expo = static_cast<s32>(m.r32(p + VA_R_MODE)) == kVmExpDec;
        sv.adsr2 = static_cast<u16>((sv.adsr2 & ~0x20u) | (static_cast<u16>(expo) << 5));
    }
    if (all || (mask & SV_AR)) sv.adsr1 = static_cast<u16>((sv.adsr1 & ~0x7F00u) | ((m.r16(p + VA_AR) & 0x7Fu) << 8));
    if (all || (mask & SV_DR)) sv.adsr1 = static_cast<u16>((sv.adsr1 & ~0x00F0u) | ((m.r16(p + VA_DR) & 0x0Fu) << 4));
    if (all || (mask & SV_SL)) sv.adsr1 = static_cast<u16>((sv.adsr1 & ~0x000Fu) | (m.r16(p + VA_SL) & 0x0Fu));
    if (all || (mask & SV_SR)) sv.adsr2 = static_cast<u16>((sv.adsr2 & ~0x1FC0u) | ((m.r16(p + VA_SR) & 0x7Fu) << 6));
    if (all || (mask & SV_RR)) sv.adsr2 = static_cast<u16>((sv.adsr2 & ~0x001Fu) | (m.r16(p + VA_RR) & 0x1Fu));
    if (all || (mask & SV_ADSR1)) sv.adsr1 = m.r16(p + VA_ADSR1);
    if (all || (mask & SV_ADSR2)) sv.adsr2 = m.r16(p + VA_ADSR2);
}

void Spu::SpuSetVoiceAttr(u32 p) {
    if (!p) return;
    const u32 voices = m.r32(p + VA_VOICE), mask = m.r32(p + VA_MASK);
    for (int v = 0; v < kVoices; v++) {
        if (!(voices & (1u << v))) continue;
        apply_voice_attr(p, v, mask);
        attr_changed(v);
    }
}

void Spu::SpuNSetVoiceAttr(int v, u32 p) {
    if (!p || v < 0 || v >= kVoices) return;
    apply_voice_attr(p, v, m.r32(p + VA_MASK));
    attr_changed(v);
}

u32 Spu::SpuRSetVoiceAttr(int lo, int hi, u32 p) {
    if (!p || lo < 0 || hi >= kVoices || lo > hi) return static_cast<u32>(kSpuInvalidArgs);
    const u32 mask = m.r32(p + VA_MASK);
    for (int v = lo; v <= hi; v++) {
        apply_voice_attr(p, v, mask);
        attr_changed(v);
    }
    return kSpuSuccess;
}

void Spu::store_voice_attr(u32 p, int v) {
    const Voice& sv = voice_[v];
    m.w32(p + VA_VOICE, 1u << v);
    m.w16(p + VA_VOL_L, static_cast<u16>(sv.vol_left));
    m.w16(p + VA_VOL_R, static_cast<u16>(sv.vol_right));
    m.w16(p + VA_VOLMODE_L, static_cast<u16>(sv.volmode_left));
    m.w16(p + VA_VOLMODE_R, static_cast<u16>(sv.volmode_right));
    m.w16(p + VA_VOLX_L, static_cast<u16>(chan_[v].vol[0]));
    m.w16(p + VA_VOLX_R, static_cast<u16>(chan_[v].vol[1]));
    m.w16(p + VA_PITCH, sv.pitch);
    m.w16(p + VA_NOTE, sv.note);
    m.w16(p + VA_SAMPLE_NOTE, sv.sample_note);
    m.w16(p + VA_ENVX, static_cast<u16>(envx(v)));
    m.w32(p + VA_ADDR, sv.start_addr);
    m.w32(p + VA_LOOP_ADDR, repeat_addr_[v]);
    m.w32(p + VA_A_MODE, adsr_amode(sv.adsr1) ? kVmExpIncN : kVmLinIncN);
    m.w32(p + VA_S_MODE, adsr_smode(sv.adsr2) ? (adsr_sdir(sv.adsr2) ? kVmExpDec : kVmExpIncN)
                                              : (adsr_sdir(sv.adsr2) ? kVmLinDecN : kVmLinIncN));
    m.w32(p + VA_R_MODE, adsr_rmode(sv.adsr2) ? kVmExpDec : kVmLinDecN);
    m.w16(p + VA_AR, adsr_ar(sv.adsr1));
    m.w16(p + VA_DR, adsr_dr(sv.adsr1));
    m.w16(p + VA_SR, adsr_sr(sv.adsr2));
    m.w16(p + VA_RR, adsr_rr(sv.adsr2));
    m.w16(p + VA_SL, adsr_sl(sv.adsr1));
    m.w16(p + VA_ADSR1, sv.adsr1);
    m.w16(p + VA_ADSR2, sv.adsr2);
}

void Spu::SpuGetVoiceAttr(u32 p) {
    if (!p) return;
    const u32 voices = m.r32(p + VA_VOICE);
    for (int v = 0; v < kVoices; v++)
        if (voices & (1u << v)) {
            store_voice_attr(p, v);
            break;
        }
}

void Spu::SpuNGetVoiceAttr(int v, u32 p) {
    if (p && v >= 0 && v < kVoices) store_voice_attr(p, v);
}

void Spu::SpuSetKey(s32 on, u32 bits) {
    for (int v = 0; v < kVoices; v++) {
        if (!(bits & (1u << v))) continue;
        if (on == kLibOn) {
            voice_[v].keyed_on = 1;
            key_on(v);
        } else {
            voice_[v].keyed_on = 0;
            key_off(v);
        }
    }
}

void Spu::SpuSetKeyOnWithAttr(u32 p) {
    if (!p) return;
    const u32 voices = m.r32(p + VA_VOICE);
    SpuSetVoiceAttr(p);
    SpuSetKey(kLibOn, voices);
}

void Spu::SpuGetAllKeysStatus(u32 p) {
    if (!p) return;
    for (int v = 0; v < kVoices; v++) m.w8(p + static_cast<u32>(v), static_cast<u32>(key_status(v)));
}

u32 Spu::SpuRGetAllKeysStatus(int lo, int hi, u32 p) {
    if (!p || lo < 0 || hi >= kVoices || lo > hi) return static_cast<u32>(kSpuInvalidArgs);
    for (int v = lo; v <= hi; v++) m.w8(p + static_cast<u32>(v), static_cast<u32>(key_status(v)));
    return kSpuSuccess;
}

void Spu::SpuGetVoiceEnvelope(int v, u32 p) {
    if (p && v >= 0 && v < kVoices) m.w16(p, static_cast<u16>(envx(v)));
}

void Spu::SpuGetVoiceEnvelopeAttr(int v, u32 status, u32 env) {
    if (v < 0 || v >= kVoices) return;
    if (status) m.w32(status, static_cast<u32>(key_status(v)));
    if (env) m.w16(env, static_cast<u16>(envx(v)));
}

u32 Spu::SpuGetKeyStatus(u32 bits) const {
    for (int v = 0; v < kVoices; v++)
        if (bits & (1u << v)) return static_cast<u32>(key_status(v));
    return kOff;
}

void Spu::SpuSetCommonAttr(u32 p) {
    if (!p) return;
    const u32 mask = m.r32(p + CA_MASK);
    const bool all = (mask == 0);
    if (all || (mask & SC_MVOLL)) mvol_[0] = static_cast<s16>(m.r16(p + CA_MVOL_L));
    if (all || (mask & SC_MVOLR)) mvol_[1] = static_cast<s16>(m.r16(p + CA_MVOL_R));
    if (all || (mask & SC_MVOLMODEL)) mvolmode_[0] = static_cast<s16>(m.r16(p + CA_MVOLMODE_L));
    if (all || (mask & SC_MVOLMODER)) mvolmode_[1] = static_cast<s16>(m.r16(p + CA_MVOLMODE_R));
    set_master(mvol_[0], mvol_[1], mvolmode_[0], mvolmode_[1]);
    if (all || (mask & SC_CDVOLL)) cdvol_[0] = static_cast<s16>(m.r16(p + CA_CD_VOL_L));
    if (all || (mask & SC_CDVOLR)) cdvol_[1] = static_cast<s16>(m.r16(p + CA_CD_VOL_R));
    cd_vol_[0] = cdvol_[0];
    cd_vol_[1] = cdvol_[1];
    if (all || (mask & SC_CDREV)) cd_reverb_on_ = static_cast<s32>(m.r32(p + CA_CD_REVERB)) == kLibOn;
    if (all || (mask & SC_CDMIX)) cd_mix_on_ = static_cast<s32>(m.r32(p + CA_CD_MIX)) == kLibOn;
    m.w16(p + CA_MVOLX_L, static_cast<u16>(master_cur_[0]));
    m.w16(p + CA_MVOLX_R, static_cast<u16>(master_cur_[1]));
}

void Spu::SpuGetCommonAttr(u32 p) {
    if (!p) return;
    m.w16(p + CA_MVOL_L, static_cast<u16>(mvol_[0]));
    m.w16(p + CA_MVOL_R, static_cast<u16>(mvol_[1]));
    m.w16(p + CA_MVOLMODE_L, static_cast<u16>(mvolmode_[0]));
    m.w16(p + CA_MVOLMODE_R, static_cast<u16>(mvolmode_[1]));
    m.w16(p + CA_MVOLX_L, static_cast<u16>(master_cur_[0]));
    m.w16(p + CA_MVOLX_R, static_cast<u16>(master_cur_[1]));
    m.w16(p + CA_CD_VOL_L, static_cast<u16>(cdvol_[0]));
    m.w16(p + CA_CD_VOL_R, static_cast<u16>(cdvol_[1]));
    m.w32(p + CA_CD_MIX, cd_mix_on_ ? kLibOn : kLibOff);
}

bool Spu::block_overlaps(u32 a, u32 size) const {
    for (const Block& b : heap_)
        if (b.used && a < b.addr + b.size && b.addr < a + size) return true;
    return false;
}

u32 Spu::reverb_work_start(int mode) {
    if (mode <= 0 || mode >= kRevModeMax) return kRamSize - 0x80u;
    return static_cast<u32>(kRevStart[mode]) * 8u;
}

u32 Spu::reverb_work_size(int mode) {
    if (mode < 0 || mode >= kRevModeMax) return 0;
    return kRamSize - reverb_work_start(mode);
}

bool Spu::reverb_area_free() const {
    return !block_overlaps(reverb_work_start(rev_mode_), reverb_work_size(rev_mode_));
}

bool Spu::reverb_blocks(u32 a, u32 size) const {
    if (rev_reserved_ != kLibOn) return false;
    const u32 rbase = reverb_work_start(rev_mode_), rsize = reverb_work_size(rev_mode_);
    return a < rbase + rsize && rbase < a + size;
}

s32 Spu::heap_alloc(u32 from, u32 size) {
    if (!size) return -1;
    int live = 0;
    for (const Block& b : heap_)
        if (b.used) live++;
    if (heap_limit_ && live >= heap_limit_) return -1;
    if (from < kHeapLo) from = kHeapLo;
    from = (from + 7u) & ~7u;
    for (u32 a = from; a + size <= kRamSize;) {
        if (block_overlaps(a, size) || reverb_blocks(a, size)) {
            a += 8u;
            continue;
        }
        for (Block& b : heap_) {
            if (b.used) continue;
            b.used = 1;
            b.addr = a;
            b.size = size;
            return static_cast<s32>(a);
        }
        return -1;
    }
    return -1;
}

u32 Spu::SpuInitMalloc(s32 num) {
    for (Block& b : heap_) b = Block{};
    if (num < 0) num = 0;
    if (num > 64) num = 64;
    heap_limit_ = num;
    return static_cast<u32>(num);
}

u32 Spu::SpuMalloc(u32 size) { return static_cast<u32>(heap_alloc(kHeapLo, size)); }
u32 Spu::SpuMallocWithStartAddr(u32 addr, u32 size) { return static_cast<u32>(heap_alloc(addr, size)); }

void Spu::SpuFree(u32 a) {
    for (Block& b : heap_)
        if (b.used && b.addr == a) {
            b.used = 0;
            return;
        }
}

bool Spu::rev_area_allocated(u32 start_units) const {
    const u32 a = (start_units * 8u) & (kRamSize - 1u);
    return block_overlaps(a, kRamSize - a);
}

void Spu::rev_lib_reset() {
    rev_flag_ = 0;
    rev_reserved_ = 0;
    rev_mode_ = 0;
    rev_depth_[0] = rev_depth_[1] = 0;
    rev_delay_ = rev_feedback_ = 0;
    rev_offsetaddr_ = 0xFFFE;
    rvb_vol_[0] = rvb_vol_[1] = 0;
    reverb_voices_ = 0;
    rvb_enable_ = 0;
    rvb_set_base(static_cast<u16>(rev_offsetaddr_));
}

void Spu::set_reverb_attr(u32 mask, const u16* regs) {
    for (int i = 0; i < 32; i++)
        if (!mask || (mask & (1u << i))) rvb_reg_[i] = regs[i];
}

void Spu::spu_setReverbAttr(u32 p) {
    u16 regs[32];
    for (int i = 0; i < 32; i++) regs[i] = m.r16(p + 4u + 2u * static_cast<u32>(i));
    set_reverb_attr(m.r32(p), regs);
}

int Spu::clear_work_area(u32 mode) {
    if (mode >= static_cast<u32>(kRevModeMax)) return kSpuError;
    if (rev_area_allocated(kRevStart[mode])) return kSpuError;
    const u32 start = reverb_work_start(static_cast<int>(mode));
    std::memset(ram_.data() + start, 0, kRamSize - start);
    return 0;
}

u32 Spu::SpuClearReverbWorkArea(u32 mode) { return static_cast<u32>(clear_work_area(mode)); }

u32 Spu::SpuSetReverb(s32 on) {
    if (on == kLibOff) {
        rev_flag_ = 0;
        rvb_enable_ = 0;
    } else if (on == kLibOn) {
        if (rev_reserved_ != kLibOn && rev_area_allocated(rev_offsetaddr_)) {
            rev_flag_ = 0;
            rvb_enable_ = 0;
        } else {
            rev_flag_ = 1;
            rvb_enable_ = 1;
        }
    }
    return static_cast<u32>(rev_flag_);
}

u32 Spu::SpuSetReverbModeParam(u32 p) {
    if (!p) return static_cast<u32>(kSpuError);
    const u32 mask = m.r32(p + RA_MASK);
    const bool all = (mask == 0);
    bool mode_set = false, delay_set = false, fb_set = false, clear = false;
    u32 regmask = 0;
    u16 regs[32] = {};
    if (all || (mask & SR_MODE)) {
        u32 mode = m.r32(p + RA_MODE);
        if (mode & kRevClearWa) {
            mode &= ~kRevClearWa;
            clear = true;
        }
        if (mode >= static_cast<u32>(kRevModeMax) || rev_area_allocated(kRevStart[mode])) return static_cast<u32>(kSpuError);
        mode_set = true;
        rev_mode_ = static_cast<int>(mode);
        rev_offsetaddr_ = kRevStart[mode];
        std::memcpy(regs, kRevPreset[mode], sizeof regs);
        if (static_cast<int>(mode) == kRevEcho) {
            rev_delay_ = 0x7F;
            rev_feedback_ = 0x7F;
        } else if (static_cast<int>(mode) == kRevDelay) {
            rev_delay_ = 0x7F;
            rev_feedback_ = 0;
        } else {
            rev_delay_ = 0;
            rev_feedback_ = 0;
        }
    }
    const bool echo = rev_mode_ == kRevEcho || rev_mode_ == kRevDelay;
    if ((all || (mask & SR_DELAYTIME)) && echo) {
        if (!mode_set) {
            delay_set = true;
            std::memcpy(regs, kRevPreset[rev_mode_], sizeof regs);
            regmask = 0x0C011C00u;
        }
        const s32 d = static_cast<s32>(m.r32(p + RA_DELAY));
        rev_delay_ = d;
        const s32 d13 = static_cast<s32>(static_cast<u32>(d) << 13) / 127;
        const s32 d12 = static_cast<s32>(static_cast<u32>(d) << 12) / 127;
        regs[10] = static_cast<u16>(d13 - regs[0]);
        regs[11] = static_cast<u16>(d12 - regs[1]);
        regs[16] = static_cast<u16>(regs[17] + d12);
        regs[12] = static_cast<u16>(regs[13] + d12);
        regs[27] = static_cast<u16>(regs[29] + d12);
        regs[26] = static_cast<u16>(regs[28] + d12);
    }
    if ((all || (mask & SR_FEEDBACK)) && echo) {
        fb_set = true;
        if (!mode_set) {
            if (!delay_set) {
                std::memcpy(regs, kRevPreset[rev_mode_], sizeof regs);
                regmask = 0x80u;
            } else {
                regmask |= 0x80u;
            }
        }
        const s32 fb = static_cast<s32>(m.r32(p + RA_FEEDBACK));
        rev_feedback_ = fb;
        regs[7] = static_cast<u16>(static_cast<s32>(static_cast<u32>(fb) * 0x8100u) / 127);
    }
    int was_on = 0;
    if (mode_set) {
        was_on = rvb_enable_;
        if (was_on) rvb_enable_ = 0;
        rvb_vol_[0] = rvb_vol_[1] = 0;
        rev_depth_[0] = rev_depth_[1] = 0;
    } else {
        if (all || (mask & SR_DEPTHL)) rev_depth_[0] = static_cast<s16>(m.r16(p + RA_DEPTH_L));
        if (all || (mask & SR_DEPTHR)) rev_depth_[1] = static_cast<s16>(m.r16(p + RA_DEPTH_R));
        if (all || (mask & SR_DEPTHL)) rvb_vol_[0] = rev_depth_[0];
        if (all || (mask & SR_DEPTHR)) rvb_vol_[1] = rev_depth_[1];
    }
    if (mode_set || delay_set || fb_set) set_reverb_attr(regmask, regs);
    if (clear) clear_work_area(static_cast<u32>(rev_mode_));
    if (mode_set) {
        rvb_set_base(static_cast<u16>(rev_offsetaddr_));
        if (was_on) rvb_enable_ = 1;
    }
    return 0;
}

void Spu::SpuGetReverbModeParam(u32 p) {
    if (!p) return;
    m.w32(p + RA_MODE, static_cast<u32>(rev_mode_));
    m.w16(p + RA_DEPTH_L, static_cast<u16>(rev_depth_[0]));
    m.w16(p + RA_DEPTH_R, static_cast<u16>(rev_depth_[1]));
    m.w32(p + RA_DELAY, static_cast<u32>(rev_delay_));
    m.w32(p + RA_FEEDBACK, static_cast<u32>(rev_feedback_));
}

void Spu::SpuSetReverbDepth(u32 p) {
    if (!p) return;
    const u32 mask = m.r32(p + RA_MASK);
    if (mask == 0 || (mask & SR_DEPTHL)) rvb_vol_[0] = rev_depth_[0] = static_cast<s16>(m.r16(p + RA_DEPTH_L));
    if (mask == 0 || (mask & SR_DEPTHR)) rvb_vol_[1] = rev_depth_[1] = static_cast<s16>(m.r16(p + RA_DEPTH_R));
}

u32 Spu::SpuSetReverbVoice(s32 on, u32 bits) {
    reverb_voices_ = apply_voice_switch(reverb_voices_, on, bits) & 0xFFFFFFu;
    return reverb_voices_;
}

u32 Spu::SpuReserveReverbWorkArea(s32 on) {
    if (on == kLibOn) {
        if (!reverb_area_free()) return kLibOff;
        rev_reserved_ = kLibOn;
    } else if (on == kLibOff) {
        rev_reserved_ = kLibOff;
    }
    return static_cast<u32>(on);
}

u32 Spu::SpuIsReverbWorkAreaReserved(s32 on) const {
    const bool answer = on == kSpuDiag ? reverb_area_free() : rev_reserved_ == kLibOn;
    return answer ? kLibOn : kLibOff;
}

u32 Spu::SpuSetNoiseVoice(s32 on, u32 bits) {
    noise_voices_ = apply_voice_switch(noise_voices_, on, bits);
    return noise_voices_;
}

u32 Spu::SpuSetNoiseClock(int clock) {
    noise_clock_ = clock & 0x3F;
    return static_cast<u32>(noise_clock_);
}

u32 Spu::SpuSetPitchLFOVoice(s32 on, u32 bits) {
    lfo_voices_ = apply_voice_switch(lfo_voices_, on, bits);
    return lfo_voices_;
}

u32 Spu::spu_FsetRXX(u32 reg, u32 value, u32 bytes) {
    const u32 v = bytes ? (value >> 3) : value;
    switch (reg) {
    case 0xD1: rvb_set_base(static_cast<u16>(v)); break;
    case 0xC2: rvb_vol_[0] = static_cast<s16>(v); break;
    case 0xC3: rvb_vol_[1] = static_cast<s16>(v); break;
    default: break;
    }
    return 0x1F801C00u + 2u * reg;
}

bool Spu::load_state(StateReader& r) {
    if (!r.tag("SPUL")) return false;
    for (Voice& v : voice_) {
        v.start_addr = r.u();
        v.loop_addr = r.u();
        v.pitch = static_cast<u16>(r.u());
        v.note = static_cast<u16>(r.u());
        v.sample_note = static_cast<u16>(r.u());
        v.vol_left = static_cast<s16>(r.u());
        v.vol_right = static_cast<s16>(r.u());
        v.volmode_left = static_cast<s16>(r.u());
        v.volmode_right = static_cast<s16>(r.u());
        v.adsr1 = static_cast<u16>(r.u());
        v.adsr2 = static_cast<u16>(r.u());
        v.keyed_on = r.s();
    }
    transfer_addr_ = r.u();
    transfer_mode_ = r.u();
    ready_ = r.u() != 0;
    transfer_callback_fn_ = r.u();
    r.u();
    r.u();
    r.u();
    for (s16& x : mvol_) x = static_cast<s16>(r.u());
    for (s16& x : mvolmode_) x = static_cast<s16>(r.u());
    for (s16& x : cdvol_) x = static_cast<s16>(r.u());
    for (Block& b : heap_) {
        b.addr = r.u();
        b.size = r.u();
        b.used = r.s();
    }
    heap_limit_ = r.s();
    rev_flag_ = r.s();
    rev_reserved_ = r.s();
    rev_offsetaddr_ = r.u();
    rev_mode_ = r.s();
    for (s16& x : rev_depth_) x = static_cast<s16>(r.u());
    rev_delay_ = r.s();
    rev_feedback_ = r.s();
    if (!r.tag("SPUR")) return false;
    {
        std::vector<u8> ram;
        if (!r.block(ram)) return false;
        if (!ram.empty()) {
            if (ram.size() != kRamSize) return r.fail("SPU RAM of unexpected size"), false;
            ram_ = std::move(ram);
        }
    }
    if (!r.tag("SPUC")) return false;
    for (int v = 0; v < kVoices; ++v) {
        Chan& c = chan_[v];
        c.cur = r.u();
        c.have_block = r.s();
        c.flags = static_cast<u8>(r.u());
        for (s16& x : c.dec) x = static_cast<s16>(r.u());
        for (s16& x : c.hist) x = static_cast<s16>(r.u());
        c.idx = r.s();
        c.counter = r.u();
        for (s16& x : c.win) x = static_cast<s16>(r.u());
        c.ended = r.s();
        c.phase = r.s();
        c.env = r.s();
        c.env_div = r.u();
        c.out = r.s();
        for (s32& x : c.vol) x = r.s();
        for (s32& x : c.vol_cyc) x = r.s();
        repeat_addr_[v] = r.u();
        ignore_loop_start_[v] = static_cast<u8>(r.u());
    }
    for (s32& x : master_) x = r.s();
    for (s16& x : master_mode_) x = static_cast<s16>(r.u());
    for (s32& x : master_cur_) x = r.s();
    for (s32& x : master_cyc_) x = r.s();
    muted_ = r.s();
    for (s32& x : cd_vol_) x = r.s();
    cd_reverb_on_ = r.s();
    cd_mix_on_ = r.s();
    cd_rd_ = r.u();
    cd_wr_ = r.u();
    {
        std::vector<u8> q;
        if (!r.block(q)) return false;
        if (q.size() != (cd_wr_ - cd_rd_) * 4u) return r.fail("CD input queue of unexpected size"), false;
        for (u32 k = 0; k < cd_wr_ - cd_rd_; ++k) {
            const u32 slot = ((cd_rd_ + k) % kCdFifoFrames) * 2u;
            std::memcpy(&cd_fifo_[slot], q.data() + 4 * k, 4);
        }
    }
    noise_voices_ = r.u();
    reverb_voices_ = r.u();
    lfo_voices_ = r.u();
    noise_clock_ = r.s();
    noise_lfsr_ = static_cast<u16>(r.u());
    noise_timer_ = r.s();
    decoded_pos_ = r.u();
    field_acc_ = r.u();
    for (u16& x : rvb_reg_) x = static_cast<u16>(r.u());
    rvb_mbase_ = static_cast<u16>(r.u());
    rvb_wa_ = r.u();
    rvb_cur_ = r.u();
    for (s32& x : rvb_vol_) x = r.s();
    rvb_enable_ = r.s();
    if (!r.block_into(rvb_down_, sizeof rvb_down_)) return false;
    if (!r.block_into(rvb_up_, sizeof rvb_up_)) return false;
    rvb_pos_ = r.u();
    return r.ok;
}
}
