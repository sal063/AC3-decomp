#pragma once

#include "machine/gpu.hpp"
#include "render/hd_textures.hpp"
#include "render/ocean.hpp"
#include "render/renderer.hpp"
#include "render/vk.hpp"
#include "sim/draw_hints.hpp"
#include "sim/precise.hpp"

#include <vector>

namespace ac3::render {
class HdRenderer final : public machine::Gpu::Observer {
public:
    bool init(const Gpu& gpu, const Target& present, u32 scale, const sim::PreciseCache* precise);
    void destroy();
    bool ready() const { return gpu_ != nullptr; }
    u32 scale() const { return s_; }
    bool set_scale(u32 scale);
    void set_precise(const sim::PreciseCache* precise) { precise_ = precise; }
    void set_hints(const sim::DrawHints* hints) { hints_ = hints; }

    bool perspective = true;
    bool hd_textures = true;
    std::string texture_dir;
    bool hd_lighting = true;
    bool ground_filter = true;
    bool water = true;
    bool model_depth = true;

    void gpu_poly(const machine::Gpu& g, const machine::Gpu::Poly& p) override;
    void gpu_rect(const machine::Gpu& g, const machine::Gpu::Rect& r) override;
    void gpu_line(const machine::Gpu& g, const machine::Gpu::Line& l) override;
    void gpu_fill(const machine::Gpu& g, s32 x, s32 y, s32 w, s32 h, u16 pixel) override;
    void gpu_copy(const machine::Gpu& g, s32 sx, s32 sy, s32 w, s32 h, s32 dx, s32 dy) override;
    void gpu_upload(const machine::Gpu& g, s32 x, s32 y, s32 w, s32 h) override;
    void gpu_list(const machine::Gpu&, bool begin, u32) override {
        if (begin) {
            in_hud_ = false;
            ++list_serial_;
        }
    }
    void gpu_node(const machine::Gpu&, u32 addr) override;
    void gpu_read(machine::Gpu& g, s32 x, s32 y, s32 w, s32 h) override;

    void record(VkCommandBuffer cb, u32 slot);
    void displayed(const machine::Gpu::Display& d);
    void present(VkCommandBuffer cb, const Target& target, const machine::Gpu::Display& d,
                 float aspect = 4.0f / 3.0f);

    u32 prims() const { return last_prims_; }
    u32 precise_prims() const { return last_precise_; }

    void check(const machine::Gpu& g, u32 frame);
    std::string check_dump;

    struct Perf {
        u32 downs = 0, reads = 0;
        double read_ms = 0;
    };
    Perf perf;

private:
    enum Pipe : u8 { kOpaque, kHalf, kAdd, kSub, kPipes };
    enum Op : u8 { kDraw, kFill, kSync, kCopy, kDown, kClearDepth };
    enum Depth : u8 { kNoDepth, kDepthWrite, kDepthTest };
    struct Vtx {
        float x = 0, y = 0, w = 1, depth = 0;
        float u, v;
        u32 rgba;
        u32 info[4] = {0, 0, 0, 0xFFFF0000u};
        float view[3] = {0, 0, 0};
        float normal[3] = {0, 0, 0};
    };
    struct Cmd {
        Op op = kDraw;
        u8 pipe = kOpaque;
        s32 x = 0, y = 0, w = 0, h = 0;
        s32 sx = 0, sy = 0;
        u32 first = 0, count = 0;
        float color[4] = {};
        s32 light = -1;
        u8 depth = kNoDepth;
    };
    struct Frame {
        Buffer vertices, staging;
    };

    void emit(Pipe pipe, const machine::Gpu& g, const Vtx* v, u32 n);
    void mark_dirty(s32 x0, s32 y0, s32 x1, s32 y1);
    void mark_drawn(s32 x0, s32 y0, s32 x1, s32 y1);
    void set_blocks(std::vector<u8>& map, s32 x0, s32 y0, s32 x1, s32 y1, bool whole, u8 v);
    bool any_block(const std::vector<u8>& map, s32 x0, s32 y0, s32 x1, s32 y1) const;
    void need(s32 x0, s32 y0, s32 x1, s32 y1);
    void need_texture(u16 tpage, u16 clut, u32 twin_mask, int u0, int v0, int u1, int v1);
    void sync(const machine::Gpu& g, s32 x, s32 y, s32 w, s32 h);
    void copy_quad(s32 x, s32 y, s32 w, s32 h);
    static Pipe pipe_for(bool semi, u16 tpage);
    bool strip_of(u16 tpage, u16 clut) const;

    bool grow(Buffer& b, VkDeviceSize need, VkBufferUsageFlags usage);
    bool make_pipelines(const Target& present);
    void begin_pass(VkCommandBuffer cb);
    bool replay(VkCommandBuffer cb, Frame& f);
    bool make_scaled();
    void make_picture(VkCommandBuffer cb);
    bool create_picture(u32 w, u32 h);
    void destroy_picture();
    bool read_native(s32 x, s32 y, s32 w, s32 h, u16* out);

    const Gpu* gpu_ = nullptr;
    const sim::PreciseCache* precise_ = nullptr;
    const sim::DrawHints* hints_ = nullptr;
    HdTextures hd_tex_;
    Ocean ocean_;
    double ocean_time_ = 0;
    bool ocean_due_ = false;
    u32 shore_seen_ = 0;
    u64 frame_ = 0;
    bool in_hud_ = false;
    u32 node_ = 0;
    float hud_squeeze(const machine::Gpu& g, float& centre) const;
    u32 s_ = 4, w_ = 0, h_ = 0;
    Image target_, temp_, mirror_;
    Image depth_;
    VkFormat depth_format_ = VK_FORMAT_UNDEFINED;
    u32 list_serial_ = 0, depth_list_ = ~0u, depth_polys_ = 0;
    s32 depth_area_[4] = {};
    u8 emit_depth_ = kNoDepth;
    VkSampler nearest_ = VK_NULL_HANDLE, linear_ = VK_NULL_HANDLE, trilinear_ = VK_NULL_HANDLE;
    VkImage picture_ = VK_NULL_HANDLE;
    VkDeviceMemory picture_memory_ = VK_NULL_HANDLE;
    VkImageView picture_view_ = VK_NULL_HANDLE;
    VkDescriptorSet picture_set_ = VK_NULL_HANDLE;
    u32 picture_w_ = 0, picture_h_ = 0, picture_levels_ = 0;
    machine::Gpu::Display shown_{};
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet mirror_set_ = VK_NULL_HANDLE, target_set_ = VK_NULL_HANDLE;
    VkDescriptorSet strip_set_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE, present_layout_ = VK_NULL_HANDLE, down_layout_ = VK_NULL_HANDLE,
                     present24_layout_ = VK_NULL_HANDLE;
    VkPipeline pipes_[kPipes] = {};
    VkPipeline present_ = VK_NULL_HANDLE, present24_ = VK_NULL_HANDLE, down_ = VK_NULL_HANDLE;
    Frame frames_[kFramesInFlight];
    Frame flush_;
    Buffer readback_;

    std::vector<Vtx> verts_;
    std::vector<Cmd> cmds_;
    std::vector<sim::PreciseCache::Light> lights_;
    std::vector<const sim::PreciseCache::Light*> light_from_;
    std::vector<sim::PreciseCache::Water> waters_;
    struct WaterMean {
        u32 texture = 0, window = 0, texels = 0;
        float rgb[3] = {-1, 0, 0};
    };
    std::vector<WaterMean> water_means_;
    void water_mean(const machine::Gpu& g, u16 tpage, u16 clut, u16 twin_mask, u16 twin_off, int u0, int v0, int u1,
                    int v1, float rgb[3]);
    s32 emit_light_ = -1;
    struct Strip {
        s32 x = 0, y = 0, w = 0, h = 0;
        bool valid = false;
    } strip_;
    std::vector<u16> staging_;
    static constexpr int kBlock = 16, kBlocksX = 1024 / kBlock, kBlocksY = 512 / kBlock;
    std::vector<u8> dirty_;
    std::vector<u8> drawn_;
    u32 prims_ = 0, precise_count_ = 0, last_prims_ = 0, last_precise_ = 0;
};
}
