#include "core/log.hpp"
#include "sim/game/game.hpp"
#include "sim/game/globals.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace ac3::sim {
namespace {
constexpr u32 kDispBuffers = 0x800BCE58u;
constexpr u32 kDirector = 0x800BD0A8u;
constexpr u32 kTrails = 0x800BD0B0u;
constexpr u32 kLiveWeapons = 5448, kWeaponBytes = 164, kWeaponFirst = 200;
constexpr u32 kSegments = 5584, kSegmentBytes = 60, kSegmentMasks = 13264;
constexpr u32 kTrailPoolB = 74020, kTrailSamples = 264;
constexpr u32 kTrailBytesA = 2312, kTrailBytesB = 1288;
constexpr u32 kFx = 0x800BCF90u;
constexpr u32 kSpray = 0x600, kMeshes = 0x1098, kLights = 0x33C0;
constexpr u32 kMotes = 32 * (3 + 10 + 1);

u32 mote(Memory& m, u32 fx, u32 i, bool& live, u32& kind_at) {
    const u32 group = i >> 5, bit = i & 31u;
    kind_at = 24;
    if (group < 3) {
        live = (m.r32(fx + kSpray + 4 * group) >> bit) & 1u;
        return fx + kSpray + 12 + 896 * group + 28 * bit;
    }
    if (group < 13) {
        live = (m.r32(fx + kMeshes + 4 * (group - 3)) >> bit) & 1u;
        return fx + kMeshes + 40 + 896 * (group - 3) + 28 * bit;
    }
    kind_at = 20;
    live = (m.r32(fx + kLights) >> bit) & 1u;
    return fx + kLights + 4 + 24 * bit;
}

s32 turn(s32 d) { return ((d + 0x2000) & 0x3FFF) - 0x2000; }

class DryHw final : public Hw {
public:
    explicit DryHw(Hw& real) : real_(real) {}

    u32 vsync(s32 mode) override { return mode < 0 || mode == 1 ? real_.vsync(mode) : 0; }
    u32 get_rcnt(u32 spec) override { return real_.get_rcnt(spec); }
    u32 reset_callback() override { return 0; }
    u32 set_rcnt(u32, u32, u32) override { return 0; }
    u32 start_rcnt(u32) override { return 0; }
    u32 cd_init() override { return 1; }
    u32 cd_both_discs() override { return real_.cd_both_discs(); }
    void cd_pack_follow(u32) override {}
    void spu_init() override {}
    u32 spu_set_reverb_mode_param(u32) override { return 0; }
    u32 pad_start_com() override { return 0; }
    u32 pad_enable_com(u32) override { return 0; }
    void init_card(u32) override {}
    u32 start_card() override { return 0; }
    u32 dma_callback(u32, u32) override { return 0; }
    void spu_set_reverb_attr(u32) override {}
    void spu_start() override {}
    void spu_quit() override {}
    u32 spu_write0(u32) override { return 0; }
    u32 stop_card() override { return 0; }
    void deliver_event_hw(u32, u32) override {}
    u32 vsync_callback(u32) override { return 0; }
    u32 set_intr_mask(u32) override { return 0; }

    void put_disp_env(u32) override {}
    void put_draw_env(u32 env) override { real_.put_draw_env(env); }
    void draw_otag(u32 ot) override { real_.draw_otag(ot); }
    void clear_image(u32 rect, u32 r, u32 g, u32 b) override { real_.clear_image(rect, r, g, b); }
    void load_image(u32 rect, u32 src) override { real_.load_image(rect, src); }
    void store_image(u32 rect, u32 dst) override { real_.store_image(rect, dst); }
    void move_image(u32 rect, u32 x, u32 y) override { real_.move_image(rect, x, y); }
    void draw_prim(u32 prim) override { real_.draw_prim(prim); }
    void reset_graph(u32) override {}
    void set_disp_mask(u32) override {}

    u32 pad_get_state(u32 port) override { return real_.pad_get_state(port); }
    u32 pad_info_mode(u32 port, u32 term, u32 offs) override { return real_.pad_info_mode(port, term, offs); }
    u32 pad_info_act(u32 port, u32 act, u32 term) override { return real_.pad_info_act(port, act, term); }
    u32 pad_chk_vsync() override { return 0; }

    void spu_set_key(s32, u32) override {}
    void spu_set_voice_attr(u32) override {}
    void spu_set_common_attr(u32) override {}
    void spu_get_all_keys_status(u32 out) override { real_.spu_get_all_keys_status(out); }
    u32 spu_set_transfer_mode(u32) override { return 0; }
    u32 spu_set_transfer_start_addr(u32) override { return 0; }
    u32 spu_write(u32, u32) override { return 0; }
    u32 spu_is_transfer_completed(u32) override { return 1; }
    u32 spu_set_reverb(s32) override { return 0; }
    void spu_set_reverb_depth(u32) override {}
    u32 spu_set_reverb_voice(s32, u32) override { return 0; }
    u32 spu_clear_reverb_work_area(u32) override { return 0; }
    u32 spu_read_decoded_data(u32, s32) override { return 0; }

    u32 open_event(u32, u32, u32, u32) override { return 0; }
    u32 close_event(u32) override { return 0; }
    u32 enable_event(u32) override { return 0; }
    u32 disable_event(u32) override { return 0; }
    u32 test_event(u32) override { return 0; }
    u32 enter_critical_section() override { return 0; }
    u32 exit_critical_section() override { return 0; }

    u32 card_info(u32) override { return 0; }
    u32 card_load(u32) override { return 0; }
    u32 card_clear(u32) override { return 0; }
    u32 card_read(u32, u32, u32) override { return 0; }
    u32 card_write(u32, u32, u32) override { return 0; }
    u32 card_chan() override { return 0; }
    u32 file_open(u32, u32) override { return 0xFFFFFFFFu; }
    u32 file_close(u32) override { return 0; }
    u32 file_lseek(u32, u32, u32) override { return 0; }
    u32 file_read(u32, u32, u32) override { return 0; }
    u32 file_write(u32, u32, u32) override { return 0; }
    u32 first_file(u32, u32) override { return 0; }
    u32 next_file(u32) override { return 0; }

    u32 cd_control(u32, u32, u32) override { return 1; }
    u32 cd_control_b(u32, u32, u32) override { return 1; }
    u32 cd_control_f(u32, u32) override { return 1; }
    u32 cd_sync(u32, u32) override { return 2; }
    u32 cd_get_sector(u32, u32) override { return 0; }
    u32 cd_ready_callback(u32) override { return 0; }
    u32 cd_sync_callback(u32) override { return 0; }
    u32 cd_data_callback(u32) override { return 0; }
    u32 cd_search_file(u32, u32) override { return 0; }
    s32 cd_pos_to_int(u32 loc) override { return real_.cd_pos_to_int(loc); }
    u32 cd_int_to_pos(s32 lba, u32 loc) override { return real_.cd_int_to_pos(lba, loc); }
    u32 cd_read(u32, u32, u32) override { return 0; }
    u32 cd_read_sync(u32, u32) override { return 0; }
    u32 cd_reset(u32) override { return 1; }
    u32 cd_mix(u32) override { return 1; }
    u32 dec_dct_out_sync(u32) override { return 0; }
    u32 dec_dct_reset(u32) override { return 0; }
    u32 dec_dct_in(u32, u32) override { return 0; }
    u32 dec_dct_out(u32, u32) override { return 0; }
    u32 dec_dct_in_callback(u32) override { return 0; }
    u32 dec_dct_out_callback(u32) override { return 0; }

private:
    Hw& real_;
};
}

struct Game::SmoothPose {
    bool valid = false;
    u32 frame = 0;
    u32 world = 0;
    bool paused = false;

    u32 flags = 0, mode = 0, subject = 0, target = 0, director = 0, shot = 0, shots = 0;
    s32 pos[3] = {};
    s16 ang[3] = {}, off[3] = {}, look[3] = {};
    s32 rel[3] = {};
    s16 bars[4] = {};

    struct Body {
        bool live = false;
        u32 kind = 0;
        s32 pos[3] = {};
        s16 ang[3] = {};
    };
    Body bodies[64], weapons[32];

    struct Segment {
        bool live = false;
        u32 flags = 0;
        s32 a[3] = {}, b[3] = {};
    };
    Segment segments[128];

    struct Trail {
        bool live = false;
        s32 end = 0;
        s32 p[4][3] = {};
    };
    Trail trails[2][32];

    struct Mote {
        bool live = false;
        s16 age = 0, life = 0;
        u16 kind = 0;
        s32 pos[3] = {};
    };
    Mote motes[32 * 14];
};

struct Game::SmoothState {
    SmoothPose before, now;
    s32 moved[3] = {};
    bool moved_known = false;
    bool cut = false;
    std::vector<u8> ram, scratch, keep_ram, keep_scratch;
    Gte gte;
    u32 sp = 0, reg_s0 = 0, reg_ra = 0, reg_s4 = 0, part_target = 0, fn = 0;
    bool have = false;
    bool warned = false;
};

Game::~Game() { delete smooth_; }

void Game::smooth_reset() {
    if (!smooth_) return;
    smooth_->have = false;
    smooth_->now.valid = smooth_->before.valid = false;
    smooth_->moved_known = false;
}

void Game::smooth_read(SmoothPose& p) {
    const u32 scene = m.r32(g::kScene), world = m.r32(g::kWorld), v = m.r32(world + 40);
    p.valid = true;
    p.frame = m.r32(scene + 36);
    p.world = world;
    p.paused = (m.r32(scene) & 0x20000000u) != 0;
    p.flags = m.r32(v) & 0x13Fu;
    p.mode = m.r32(v + 80);
    p.subject = m.r32(v + 52);
    p.target = m.r32(v + 192);
    p.director = m.r32(kDirector);
    p.shot = m.r32(v + 236);
    p.shots = m.r32(v + 240);
    for (u32 k = 0; k < 3; ++k) {
        p.pos[k] = m.r32s(v + 4 + 4 * k);
        p.ang[k] = m.r16s(v + 20 + 2 * k);
        p.off[k] = m.r16s(v + 44 + 2 * k);
        p.look[k] = m.r16s(v + 180 + 2 * k);
        p.rel[k] = m.r32s(v + 56 + 4 * k);
    }
    for (u32 k = 0; k < 4; ++k) p.bars[k] = m.r16s(world + 652 + 2 * k);
    for (u32 i = 0; i < 64; ++i) {
        SmoothPose::Body& b = p.bodies[i];
        const u32 e = slot(i);
        b.live = e >= 0x80000000u && e < 0x80200000u && (m.r32(e) & 0x4000u);
        if (!b.live) continue;
        b.kind = m.r8(e + 4) | (m.r16(e + 72) << 8);
        for (u32 k = 0; k < 3; ++k) {
            b.pos[k] = m.r32s(e + 8 + 4 * k);
            b.ang[k] = m.r16s(e + 24 + 2 * k);
        }
    }
    const u32 weapons = m.r32(g::kWeapons);
    const bool armed = weapons >= 0x80000000u && weapons < 0x80200000u;
    const u32 live = armed ? m.r32(weapons + kLiveWeapons) : 0;
    for (u32 i = 0; i < 32; ++i) {
        SmoothPose::Body& b = p.weapons[i];
        b.live = (live >> i) & 1u;
        if (!b.live) continue;
        const u32 w = weapons + kWeaponFirst + kWeaponBytes * i;
        b.kind = m.r8(w + 52);
        for (u32 k = 0; k < 3; ++k) {
            b.pos[k] = m.r32s(w + 28 + 4 * k);
            b.ang[k] = m.r16s(w + 44 + 2 * k);
        }
    }
    for (u32 i = 0; i < 128; ++i) {
        SmoothPose::Segment& s = p.segments[i];
        s.live = armed && ((m.r32(weapons + kSegmentMasks + 4 * (i >> 5)) >> (i & 31u)) & 1u);
        if (!s.live) continue;
        const u32 at = weapons + kSegments + kSegmentBytes * i;
        s.flags = m.r32(at);
        for (u32 k = 0; k < 3; ++k) {
            s.a[k] = m.r32s(at + 8 + 4 * k);
            s.b[k] = m.r32s(at + 24 + 4 * k);
        }
    }
    const u32 pool = m.r32(kTrails);
    const bool trails = pool >= 0x80000000u && pool < 0x80200000u;
    for (u32 set = 0; set < 2; ++set) {
        const u32 base = pool + (set ? kTrailPoolB : 0u);
        const u32 mask = trails ? m.r32(base) : 0;
        for (u32 i = 0; i < 32; ++i) {
            SmoothPose::Trail& t = p.trails[set][i];
            const u32 ring = base + 4 + i * (set ? kTrailBytesB : kTrailBytesA);
            t.live = ((mask >> i) & 1u) && m.r32s(ring) > m.r32s(ring + 4);
            if (!t.live) continue;
            t.end = m.r32s(ring);
            const u32 sample = ring + kTrailSamples + (set ? 32u : 64u) * (static_cast<u32>(t.end - 1) & 31u);
            for (u32 q = 0; q < (set ? 2u : 4u); ++q)
                for (u32 k = 0; k < 3; ++k) t.p[q][k] = m.r32s(sample + 16 * q + 4 * k);
        }
    }
    const u32 fx = m.r32(kFx);
    const bool effects = fx >= 0x80000000u && fx < 0x801F0000u;
    for (u32 i = 0; i < kMotes; ++i) {
        SmoothPose::Mote& o = p.motes[i];
        o.live = false;
        if (!effects) continue;
        u32 kind_at = 0;
        const u32 at = mote(m, fx, i, o.live, kind_at);
        if (!o.live) continue;
        o.age = m.r16s(at);
        o.life = m.r16s(at + 2);
        o.kind = static_cast<u16>(m.r16(at + kind_at));
        for (u32 k = 0; k < 3; ++k) o.pos[k] = m.r32s(at + 4 + 4 * k);
    }
}

void Game::smooth_capture(u32 fn) {
    if (!smooth_on) {
        if (smooth_) smooth_reset();
        return;
    }
    if (!smooth_) smooth_ = new SmoothState;
    SmoothState& s = *smooth_;
    const SmoothPose last = s.before;
    s.before = s.now;
    smooth_read(s.now);
    const SmoothPose& a = s.before;
    const SmoothPose& b = s.now;
    s.have = a.valid && b.frame == a.frame + 1 && b.world == a.world && !b.paused;
    s.cut = !s.have || a.flags != b.flags || a.mode != b.mode || a.subject != b.subject || a.target != b.target ||
            a.director != b.director || a.shot != b.shot || a.shots != b.shots;
    s32 moved[3];
    for (u32 k = 0; k < 3; ++k) moved[k] = static_cast<s32>(static_cast<u32>(b.pos[k]) - static_cast<u32>(a.pos[k]));
    if (!s.cut) {
        constexpr s32 kFar = 768 << 12, kJolt = 96 << 12;
        for (u32 k = 0; k < 3; ++k) {
            if (std::abs(moved[k]) > kFar) s.cut = true;
            if (s.moved_known && std::abs(moved[k] - s.moved[k]) > kJolt + std::abs(s.moved[k])) s.cut = true;
        }
    }
    s.moved_known = s.have && !s.cut && last.valid;
    if (s.have)
        for (u32 k = 0; k < 3; ++k) s.moved[k] = moved[k];
    if (s.have && !s.cut) {
        s.ram.resize(Memory::kRamSize);
        s.scratch.resize(Memory::kScratchSize);
        std::memcpy(s.ram.data(), m.ram(), Memory::kRamSize);
        std::memcpy(s.scratch.data(), m.scratch(), Memory::kScratchSize);
        s.gte = gte;
        s.sp = sp;
        s.reg_s0 = reg_s0;
        s.reg_ra = reg_ra;
        s.reg_s4 = reg_s4;
        s.part_target = part_target;
        s.fn = fn;
    }
}

bool Game::smooth_ready() const {
    return smooth_on && smooth_ && smooth_->have && !smooth_->cut &&
           m.r32(m.r32(g::kScene) + 36) == smooth_->now.frame + 1;
}

namespace {
s32 mix(s32 a, s32 b, double t) {
    const s32 d = static_cast<s32>(static_cast<u32>(b) - static_cast<u32>(a));
    return static_cast<s32>(static_cast<u32>(a) + static_cast<u32>(static_cast<s32>(std::lround(d * t))));
}
bool near_enough(const s32 a[3], const s32 b[3], s32 limit) {
    for (u32 k = 0; k < 3; ++k)
        if (std::abs(static_cast<s32>(static_cast<u32>(b[k]) - static_cast<u32>(a[k]))) > limit) return false;
    return true;
}
void mix_angles(const s16 a_in[3], const s16 b[3], double t, s16 out[3]) {
    s32 a[3] = {a_in[0], a_in[1], a_in[2]};
    if (std::abs(turn(b[1] - a[1])) > 0x1000 && std::abs(turn(b[2] - a[2])) > 0x1000) {
        a[0] = (a[0] >= 0 ? 0x2000 : -0x2000) - a[0];
        a[1] += 0x2000;
        a[2] += 0x2000;
    }
    s32 r[3];
    r[0] = a[0] + static_cast<s32>(std::lround((b[0] - a[0]) * t));
    for (u32 k = 1; k < 3; ++k) r[k] = a[k] + static_cast<s32>(std::lround(turn(b[k] - a[k]) * t));
    if (r[0] > 0x1000) {
        r[0] = 0x2000 - r[0];
        r[1] += 0x2000;
        r[2] += 0x2000;
    } else if (r[0] < -0x1000) {
        r[0] = -0x2000 - r[0];
        r[1] += 0x2000;
        r[2] += 0x2000;
    }
    out[0] = static_cast<s16>(r[0]);
    for (u32 k = 1; k < 3; ++k) out[k] = static_cast<s16>(turn(r[k]));
}
}

void Game::smooth_blend(double t) {
    const SmoothPose& a = smooth_->before;
    const SmoothPose& b = smooth_->now;
    const u32 world = m.r32(g::kWorld), v = m.r32(world + 40);
    constexpr s32 kBodyFar = 1024 << 12;
    s16 ang[3];
    mix_angles(a.ang, b.ang, t, ang);
    for (u32 k = 0; k < 3; ++k) {
        m.w32(v + 4 + 4 * k, static_cast<u32>(mix(a.pos[k], b.pos[k], t)));
        m.w16(v + 20 + 2 * k, static_cast<u16>(ang[k]));
        m.w16(v + 44 + 2 * k, static_cast<u16>(mix(a.off[k], b.off[k], t)));
        m.w16(v + 180 + 2 * k, static_cast<u16>(mix(a.look[k], b.look[k], t)));
        m.w32(v + 56 + 4 * k, static_cast<u32>(mix(a.rel[k], b.rel[k], t)));
    }
    for (u32 k = 0; k < 4; ++k) m.w16(world + 652 + 2 * k, static_cast<u16>(mix(a.bars[k], b.bars[k], t)));
    for (u32 i = 0; i < 64; ++i) {
        const SmoothPose::Body& p = a.bodies[i];
        const SmoothPose::Body& q = b.bodies[i];
        if (!p.live || !q.live || p.kind != q.kind || !near_enough(p.pos, q.pos, kBodyFar)) continue;
        const u32 e = slot(i);
        mix_angles(p.ang, q.ang, t, ang);
        for (u32 k = 0; k < 3; ++k) {
            m.w32(e + 8 + 4 * k, static_cast<u32>(mix(p.pos[k], q.pos[k], t)));
            m.w16(e + 24 + 2 * k, static_cast<u16>(ang[k]));
        }
    }
    const u32 weapons = m.r32(g::kWeapons);
    for (u32 i = 0; i < 32; ++i) {
        const SmoothPose::Body& p = a.weapons[i];
        const SmoothPose::Body& q = b.weapons[i];
        if (!p.live || !q.live || p.kind != q.kind || !near_enough(p.pos, q.pos, kBodyFar)) continue;
        const u32 w = weapons + kWeaponFirst + kWeaponBytes * i;
        mix_angles(p.ang, q.ang, t, ang);
        for (u32 k = 0; k < 3; ++k) {
            m.w32(w + 28 + 4 * k, static_cast<u32>(mix(p.pos[k], q.pos[k], t)));
            m.w16(w + 44 + 2 * k, static_cast<u16>(ang[k]));
        }
    }
    for (u32 i = 0; i < 128; ++i) {
        const SmoothPose::Segment& p = a.segments[i];
        const SmoothPose::Segment& q = b.segments[i];
        if (!p.live || !q.live || p.flags != q.flags || !near_enough(p.a, q.a, kBodyFar) ||
            !near_enough(p.b, q.b, kBodyFar))
            continue;
        const u32 at = weapons + kSegments + kSegmentBytes * i;
        for (u32 k = 0; k < 3; ++k) {
            m.w32(at + 8 + 4 * k, static_cast<u32>(mix(p.a[k], q.a[k], t)));
            m.w32(at + 24 + 4 * k, static_cast<u32>(mix(p.b[k], q.b[k], t)));
        }
    }
    const u32 pool = m.r32(kTrails);
    for (u32 set = 0; set < 2; ++set) {
        const u32 base = pool + (set ? kTrailPoolB : 0u);
        for (u32 i = 0; i < 32; ++i) {
            const SmoothPose::Trail& p = a.trails[set][i];
            const SmoothPose::Trail& q = b.trails[set][i];
            if (!p.live || !q.live || (q.end != p.end && q.end != p.end + 1)) continue;
            const u32 points = set ? 2u : 4u;
            bool near_by = true;
            for (u32 n = 0; n < points; ++n) near_by = near_by && near_enough(p.p[n], q.p[n], kBodyFar);
            if (!near_by) continue;
            const u32 ring = base + 4 + i * (set ? kTrailBytesB : kTrailBytesA);
            const u32 sample = ring + kTrailSamples + (set ? 32u : 64u) * (static_cast<u32>(q.end - 1) & 31u);
            for (u32 n = 0; n < points; ++n)
                for (u32 k = 0; k < 3; ++k) m.w32(sample + 16 * n + 4 * k, static_cast<u32>(mix(p.p[n][k], q.p[n][k], t)));
        }
    }
    const u32 fx = m.r32(kFx);
    for (u32 i = 0; i < kMotes; ++i) {
        const SmoothPose::Mote& p = a.motes[i];
        const SmoothPose::Mote& q = b.motes[i];
        if (!p.live || !q.live || p.kind != q.kind || p.life != q.life || q.age < p.age || q.age > p.age + 2 ||
            !near_enough(p.pos, q.pos, kBodyFar))
            continue;
        bool live = false;
        u32 kind_at = 0;
        const u32 at = mote(m, fx, i, live, kind_at);
        for (u32 k = 0; k < 3; ++k) m.w32(at + 4 + 4 * k, static_cast<u32>(mix(p.pos[k], q.pos[k], t)));
    }
}

bool Game::smooth_draw(float t) {
    if (!smooth_ready() || !hw) return false;
    SmoothState& s = *smooth_;
    s.keep_ram.resize(Memory::kRamSize);
    s.keep_scratch.resize(Memory::kScratchSize);
    std::memcpy(s.keep_ram.data(), m.ram(), Memory::kRamSize);
    std::memcpy(s.keep_scratch.data(), m.scratch(), Memory::kScratchSize);
    const Gte keep_gte = gte;
    const u32 keep_sp = sp, keep_s0 = reg_s0, keep_ra = reg_ra, keep_s4 = reg_s4, keep_target = part_target;
    const u32 keep_result = phase_result, keep_unported = unported_hits;
    const bool keep_coverage = track_coverage;
    Hw* const keep_hw = hw;
    ScriptObserver* const keep_observer = script_observer;
    SoundSink keep_sound = std::move(sound);
    const s32 keep_squeeze = gte.x_squeeze;

    std::memcpy(m.ram(), s.ram.data(), Memory::kRamSize);
    std::memcpy(m.scratch(), s.scratch.data(), Memory::kScratchSize);
    gte = s.gte;
    gte.precise = keep_gte.precise;
    sp = s.sp;
    reg_s0 = s.reg_s0;
    reg_ra = s.reg_ra;
    reg_s4 = s.reg_s4;
    part_target = s.part_target;
    track_coverage = false;
    script_observer = nullptr;
    sound = SoundSink{};
    DryHw dry(*keep_hw);
    hw = &dry;
    frame_fraction_ = std::clamp(t, 0.0f, 1.0f);

    const u32 scene = m.r32(g::kScene);
    const u32 drawn = m.r32(scene + 4) & 1u, shown = drawn ^ 1u;
    m.w32(scene + 4, shown);
    smooth_blend(frame_fraction_);
    const u32 world = m.r32(g::kWorld);
    view_matrices(m.r32(world + 40), world + 44, world + 76);
    draw_passes();
    phase_leave(s.fn);
    const u32 sc = m.r32(g::kScene);
    m.w32(sc + 20, overlay_packets(m.r32(sc + 12), m.r32(sc + 20)));
    const u32 buffers = m.r32(kDispBuffers);
    const u32 env = m.r32(buffers + 4116u * shown);
    build_dr_env(env);
    m.w32(env + 28, m.r32(env + 28) | 0x00FFFFFFu);
    hw->put_draw_env(env);
    if (!(m.r32(sc) & 4u)) hw->draw_otag(buffers + 4116u * drawn + 16);

    frame_fraction_ = 1.0f;
    hw = keep_hw;
    sound = std::move(keep_sound);
    script_observer = keep_observer;
    track_coverage = keep_coverage;
    std::memcpy(m.ram(), s.keep_ram.data(), Memory::kRamSize);
    std::memcpy(m.scratch(), s.keep_scratch.data(), Memory::kScratchSize);
    gte = keep_gte;
    gte.x_squeeze = keep_squeeze;
    sp = keep_sp;
    reg_s0 = keep_s0;
    reg_ra = keep_ra;
    reg_s4 = keep_s4;
    part_target = keep_target;
    phase_result = keep_result;
    if (unported_hits != keep_unported && !s.warned) {
        s.warned = true;
        log::warn("an in-between frame reached code not reimplemented yet");
    }
    unported_hits = keep_unported;
    return true;
}
}
