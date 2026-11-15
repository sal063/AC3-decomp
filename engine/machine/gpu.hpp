#pragma once

#include "core/types.hpp"

#include <array>
#include <vector>

namespace ac3::sim {
class Memory;
}

namespace ac3::machine {
struct StateReader;

class Gpu {
public:
    static constexpr int kVramW = 1024, kVramH = 512;

    struct Regs {
        u16 texpage = 0;
        u32 texwin = 0;
        s32 area_x0 = 0, area_y0 = 0, area_x1 = 0, area_y1 = 0;
        s32 ofs_x = 0, ofs_y = 0;
        u16 mask_or = 0, mask_and = 0;
    };
    struct Display {
        int enabled = 1;
        int x = 0, y = 0, w = 320, h = 240;
        int rgb24 = 0;
    };
    static constexpr u32 kDrawEnvBytes = 92, kDispEnvBytes = 20;

    explicit Gpu(sim::Memory& mem);

    Regs regs;
    Display display;
    std::vector<u16> vram;
    std::array<u8, kDrawEnvBytes> cur_draw{};
    std::array<u8, kDispEnvBytes> cur_disp{};
    u32 draw_count = 0;
    bool rasterize = true;

    void draw_otag(u32 ot);
    void put_draw_env(u32 env);
    void put_disp_env(u32 env);
    void clear_image(u32 rect, u32 r, u32 g, u32 b);
    void load_image(u32 rect, u32 src);
    void store_image(u32 rect, u32 dst);
    void move_image(u32 rect, u32 x, u32 y);
    void draw_prim(u32 prim);
    void reset_graph(u32 mode);
    void set_disp_mask(u32 on);

    bool load_state(StateReader& r);
    void load_envs(const std::vector<u8>& draw, const std::vector<u8>& disp, u32 count);

    u16& at(s32 x, s32 y) { return vram[(static_cast<u32>(y) & 511u) * kVramW + (static_cast<u32>(x) & 1023u)]; }

    struct Poly {
        int nverts = 3, textured = 0, gouraud = 0, semitrans = 0, raw = 0;
        s16 x[4]{}, y[4]{};
        u8 r[4]{}, g[4]{}, b[4]{}, u[4]{}, v[4]{};
        u16 clut = 0, tpage = 0, twin_mask = 0, twin_off = 0;
    };
    struct Rect {
        int textured = 0, semitrans = 0, raw = 0;
        s16 x = 0, y = 0;
        u16 w = 0, h = 0;
        u8 r = 0, g = 0, b = 0, u = 0, v = 0;
        u16 clut = 0, tpage = 0, twin_mask = 0, twin_off = 0;
    };
    struct Line {
        int nverts = 0, gouraud = 0, semitrans = 0;
        s16 x[256]{}, y[256]{};
        u8 r[256]{}, g[256]{}, b[256]{};
        u16 tpage = 0;
    };

    class Observer {
    public:
        virtual ~Observer() = default;
        virtual void gpu_poly(const Gpu& g, const Poly& p) = 0;
        virtual void gpu_rect(const Gpu& g, const Rect& r) = 0;
        virtual void gpu_line(const Gpu& g, const Line& l) = 0;
        virtual void gpu_fill(const Gpu& g, s32 x, s32 y, s32 w, s32 h, u16 pixel) = 0;
        virtual void gpu_copy(const Gpu& g, s32 sx, s32 sy, s32 w, s32 h, s32 dx, s32 dy) = 0;
        virtual void gpu_upload(const Gpu& g, s32 x, s32 y, s32 w, s32 h) = 0;
        virtual void gpu_list(const Gpu& g, bool begin, u32 head) = 0;
        virtual void gpu_node(const Gpu& g, u32 addr) = 0;
        virtual void gpu_read(Gpu& g, s32 x, s32 y, s32 w, s32 h) = 0;
    };
    Observer* observer = nullptr;

private:
    sim::Memory& m;

    struct Shade {
        int textured = 0, raw = 0, dither = 0, blend = -1;
        u32 page_x = 0, page_y = 0, depth = 0, clut_x = 0, clut_y = 0;
        u32 and_u = 0, or_u = 0, and_v = 0, or_v = 0;
    };
    struct Vert {
        s32 x, y, u, v, r, g, b;
    };
    struct Interp {
        u32 u, v, r, g, b;
    };
    struct Grad {
        Interp dx, dy;
    };
    struct Upload {
        s32 x = 0, y = 0, w = 0, h = 0, cx = 0, cy = 0;
        u32 left = 0;
    };
    Upload up;
    u8 lut_dither[4][4][512]{};
    u8 lut_plain[512]{};
    int in_polyline = 0;

    void build_luts();
    void sw_reset();
    void gp0_state(u32 w);
    void texpage_attr(u16 attr);
    void texwin(u16* mask, u16* off) const;
    void shade_setup(Shade& s, int textured, int raw, int semitrans, u16 tpage, u16 clut, u16 tw_mask, u16 tw_off);
    u16 texel(const Shade& s, u32 u, u32 v);
    void plot(s32 x, s32 y, u16 c, int blend);
    void shade(const Shade& s, s32 x, s32 y, u32 r, u32 g, u32 b, u32 u, u32 v);
    static void interp_add(Interp& i, const Interp& d, u32 n);
    void draw_span(const Shade& s, s32 y, s32 x_start, s32 x_bound, Interp ig, const Grad& g);
    void draw_triangle(const Shade& s, int gouraud, Vert* v);
    void sw_poly(const Poly& p);
    void sw_rect(const Rect& r);
    void draw_segment(const Shade& s, int gouraud, int dither, s32 x0, s32 y0, u32 r0, u32 g0, u32 b0, s32 x1,
                      s32 y1, u32 r1, u32 g1, u32 b1);
    void sw_line(const Line& l);
    void sw_fill(s32 x, s32 y, s32 w, s32 h, u16 pixel);
    void sw_copy(s32 sx, s32 sy, s32 w, s32 h, s32 dx, s32 dy);
    void load_begin(s32 x, s32 y, s32 w, s32 h);
    void load_pixel(u16 p);
    void load_word(u32 w);
    u32 load_words_left() const { return (up.left + 1u) / 2u; }
    void load_cancel();

    void decode_poly(u32 addr, u8 code);
    void decode_rect(u32 addr, u8 code);
    void decode_line(u32 addr, u8 code, int len);
    void draw_rect(Rect& r);
    void gp0_env(u32 w);
    void gp0_fill(u32 c, u32 xy, u32 wh);
    void gp0_copy(u32 s, u32 d, u32 z);
    void gp0_load_begin(u32 xy, u32 wh);
    void decode_load(u32 addr, int len);
    int cmd_words(u32 addr, u8 code, int avail);
    void decode_one(u32 addr, u8 code, int words);
    void decode_packet(u32 addr, int len);
    void walk_list(u32 head);
};
}
