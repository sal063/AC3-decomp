#include "machine/machine.hpp"

#include "machine/state_reader.hpp"
#include "sim/memory.hpp"

#include <cstdio>

namespace ac3::machine {
namespace {
constexpr u32 kHblanksPerField = 263;
constexpr u32 kLibpadPortArrayPtr = 0x800B46B8u;
constexpr u32 kLibpadStride = 240u, kLibpadBufOffset = 48u;
constexpr u32 kPadStateDiscon = 0, kPadStateStable = 6;
constexpr u32 kInfoModeCurID = 1, kInfoModeCurExID = 2, kInfoModeCurExOffs = 3, kInfoModeIdTable = 4;
}

Machine::Machine(sim::Memory& mem) : spu(mem), cd(mem, spu), gpu(mem), card(mem), mdec(mem), m(mem) {
    mdec.call_guest = [this](u32 fn, u32 a0) {
        if (call_guest) call_guest(fn, a0, 0);
    };
    card.deliver_event = [this](u32 cls, u32 spec) { deliver_event(cls, spec); };
    pads[0].connected = true;
    cd.call_guest = [this](u32 fn, u32 a0, u32 a1) {
        if (call_guest) call_guest(fn, a0, a1);
    };
    cd.deliver_event = [this](u32 cls, u32 spec) { deliver_event(cls, spec); };
}

bool Machine::mount(const std::string& booted, const std::string& other, std::string* error) {
    std::string second = other;
    if (second.empty()) {
        const size_t p = booted.find("(Disc ");
        if (p != std::string::npos && p + 7 < booted.size() && (booted[p + 6] == '1' || booted[p + 6] == '2') &&
            booted[p + 7] == ')') {
            second = booted;
            second[p + 6] = booted[p + 6] == '1' ? '2' : '1';
        }
    }
    return cd.mount(booted, second, error);
}

u32 Machine::dec_dct_in(u32 buf, u32 mode) {
    if (!buf) return 0;
    mdec.in(buf, mode);
    cd.stream_ahead();
    return 0;
}

void Machine::deliver_event(u32 cls, u32 spec) {
    for (Event& e : events_) {
        if (!e.used || e.state != 0x2000u) continue;
        if (e.cls != cls || !(e.spec & spec)) continue;
        if (e.mode == 0x1000u && e.func) {
            if (call_guest) call_guest(e.func, 0, 0);
        } else {
            e.state = 0x4000u;
        }
    }
}

namespace {
constexpr u32 kEvDescBase = 0xF1000000u;
}

u32 Machine::open_event(u32 cls, u32 spec, u32 mode, u32 func) {
    for (u32 i = 0; i < 32; ++i) {
        Event& e = events_[i];
        if (e.used) continue;
        e.used = 1;
        e.cls = cls;
        e.spec = spec;
        e.mode = mode;
        e.func = func;
        e.state = 0x1000u;
        return kEvDescBase + i;
    }
    return 0xFFFFFFFFu;
}

u32 Machine::close_event(u32 desc) {
    const u32 i = desc - kEvDescBase;
    if (i >= 32 || !events_[i].used) return 0;
    events_[i].used = 0;
    events_[i].state = 0;
    return 1;
}

u32 Machine::enable_event(u32 desc) {
    const u32 i = desc - kEvDescBase;
    if (i >= 32 || !events_[i].used) return 0;
    events_[i].state = 0x2000u;
    return 1;
}

u32 Machine::disable_event(u32 desc) {
    const u32 i = desc - kEvDescBase;
    if (i >= 32 || !events_[i].used) return 0;
    events_[i].state = 0x1000u;
    return 1;
}

u32 Machine::wait_event(u32 desc) {
    const u32 i = desc - kEvDescBase;
    if (i < 32 && events_[i].used && events_[i].state == 0x4000u) events_[i].state = 0x2000u;
    return 1;
}

u32 Machine::test_event(u32 desc) {
    const u32 i = desc - kEvDescBase;
    if (i >= 32 || !events_[i].used || events_[i].state != 0x4000u) return 0;
    events_[i].state = 0x2000u;
    return 1;
}

void Machine::field_advance() {
    if (on_vblank) on_vblank(vblank_);
    ++vblank_;
    if (poll_input) poll_input(vblank_, pads);
    pads_fill();
    cd.tick();
    spu.pump();
    if (on_field) on_field();
}

u32 Machine::vsync(s32 mode) {
    if (mode < 0) return vblank_;
    if (mode > 1) {
        for (int guard = 0; static_cast<s32>(vblank_ - vsync_ref_) < mode && guard < 600; ++guard) field_advance();
    } else if (mode == 0) {
        field_advance();
    }
    for (int guard = 0; guard < 600 && cd.debt_frames() > 0; ++guard) field_advance();
    const u32 elapsed = (vblank_ - vsync_ref_) * kHblanksPerField;
    if (mode != 1) vsync_ref_ = vblank_;
    return elapsed;
}

u32 Machine::get_rcnt(u32 spec) {
    switch (spec & 3u) {
    case 1: return (vblank_ * kHblanksPerField) & 0xFFFFu;
    case 3: return vblank_ & 0xFFFFu;
    default: return (vblank_ * kHblanksPerField * 8u) & 0xFFFFu;
    }
}

void Machine::put_disp_env(u32 env) {
    disp_env_ = env;
    gpu.put_disp_env(env);
}
void Machine::put_draw_env(u32 env) {
    draw_env_ = env;
    gpu.put_draw_env(env);
}
void Machine::draw_otag(u32 ot) {
    gpu.draw_otag(ot);
    if (on_draw) on_draw(ot);
}
void Machine::clear_image(u32 rect, u32 r, u32 g, u32 b) { gpu.clear_image(rect, r, g, b); }
void Machine::load_image(u32 rect, u32 src) { gpu.load_image(rect, src); }
void Machine::store_image(u32 rect, u32 dst) { gpu.store_image(rect, dst); }
void Machine::move_image(u32 rect, u32 x, u32 y) { gpu.move_image(rect, x, y); }
void Machine::draw_prim(u32 prim) { gpu.draw_prim(prim); }
void Machine::get_draw_env(u32 env) {
    for (u32 i = 0; i < gpu.cur_draw.size(); ++i) m.w8(env + i, gpu.cur_draw[i]);
}
void Machine::get_disp_env(u32 env) {
    for (u32 i = 0; i < gpu.cur_disp.size(); ++i) m.w8(env + i, gpu.cur_disp[i]);
}
void Machine::reset_graph(u32 mode) { gpu.reset_graph(mode); }
void Machine::set_disp_mask(u32 on) { gpu.set_disp_mask(on); }

void Machine::pad_fill_buffer(int port, u32 buf) {
    if (!buf) return;
    const PadInput& p = pads[port];
    if (!p.connected) {
        m.w8(buf + 0, 0xFF);
        m.w8(buf + 1, 0x00);
        m.w8(buf + 2, 0xFF);
        m.w8(buf + 3, 0xFF);
        return;
    }
    const u16 inv = static_cast<u16>(~p.buttons);
    m.w8(buf + 0, 0x00);
    m.w8(buf + 1, p.type);
    m.w8(buf + 2, inv & 0xFFu);
    m.w8(buf + 3, inv >> 8);
    if (p.type == 0x73) {
        m.w8(buf + 4, p.rx);
        m.w8(buf + 5, p.ry);
        m.w8(buf + 6, p.lx);
        m.w8(buf + 7, p.ly);
    }
}

u32 Machine::direct_buffer(int port) const {
    const u32 ports = m.r32(kLibpadPortArrayPtr);
    if (!ports) return 0;
    const u32 p = m.r32(ports + kLibpadBufOffset + static_cast<u32>(port) * kLibpadStride);
    return (p >= 0x80000000u && p < 0x80200000u) ? p : 0;
}

void Machine::pads_fill() {
    for (int port = 0; port < 2; ++port) pad_fill_buffer(port, direct_buffer(port));
}

u32 Machine::pad_get_state(u32 port) {
    return pads[(port & 0x10u) ? 1 : 0].connected ? kPadStateStable : kPadStateDiscon;
}

u32 Machine::pad_info_mode(u32 port, u32 term, u32 offs) {
    const PadInput& p = pads[(port & 0x10u) ? 1 : 0];
    if (!p.connected) return 0;
    switch (term) {
    case kInfoModeCurID: return p.type >> 4;
    case kInfoModeCurExID: return p.type == 0x73 ? 7u : 0u;
    case kInfoModeCurExOffs: return 0;
    case kInfoModeIdTable: return offs == 0 ? static_cast<u32>(p.type >> 4) : 0u;
    default: return 0;
    }
}

u32 Machine::pad_info_act(u32 port, u32, u32) {
    return pads[(port & 0x10u) ? 1 : 0].type == 0x73 ? 2u : 0u;
}

bool Machine::load(const std::string& path, std::string* error) {
    std::vector<u8> data;
    {
        std::FILE* f = std::fopen(path.c_str(), "rb");
        if (!f) {
            if (error) *error = "cannot open " + path;
            return false;
        }
        std::fseek(f, 0, SEEK_END);
        data.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        const bool ok = std::fread(data.data(), 1, data.size(), f) == data.size();
        std::fclose(f);
        if (!ok) {
            if (error) *error = "cannot read " + path;
            return false;
        }
    }
    StateReader r(data.data(), data.size());
    if (data.size() < 16 || std::memcmp(data.data(), "AC3M", 4) != 0) {
        if (error) *error = path + ": not an oracle machine dump";
        return false;
    }
    r.p += 4;
    const u32 version = r.u();
    if (version != 1 && version != 2) {
        if (error) *error = path + ": unknown version";
        return false;
    }
    loaded_tick = r.u();
    loaded_field = r.u();
    loaded_checkpoint = version >= 2 ? r.u() : 20u;
    r.tag("ETC");
    r.u();
    vblank_ = r.u();
    vsync_ref_ = r.u();
    r.u();
    r.u();
    r.tag("API");
    for (Event& e : events_) {
        e.used = r.s();
        e.cls = r.u();
        e.spec = r.u();
        e.mode = r.u();
        e.func = r.u();
        e.state = r.u();
    }
    for (u32 k = 0; k < 4 * 3; ++k) r.u();
    critical_depth_ = static_cast<s32>(r.u());
    spu.load_state(r);
    cd.xa().load_state(r);
    cd.load_state(r);
    r.tag("PAD");
    for (PadInput& p : pads) {
        p.connected = r.u() != 0;
        p.type = static_cast<u8>(r.u());
        p.buttons = static_cast<u16>(r.u());
        p.lx = static_cast<u8>(r.u());
        p.ly = static_cast<u8>(r.u());
        p.rx = static_cast<u8>(r.u());
        p.ry = static_cast<u8>(r.u());
        r.u();
        r.u();
    }
    for (u32 k = 0; k < 5; ++k) r.u();
    r.tag("PAD2");
    for (u32 k = 0; k < 3; ++k) r.u();
    r.tag("GPU");
    {
        std::vector<u8> draw, disp;
        r.block(draw);
        r.block(disp);
        gpu.load_envs(draw, disp, r.u());
    }
    if (version >= 2) gpu.load_state(r);
    r.tag("PLAY");
    play_offset = r.u();
    r.u();
    r.tag("END");
    if (!r.ok) {
        if (error) *error = path + ": " + r.error;
        return false;
    }
    return true;
}
}
