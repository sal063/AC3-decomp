#include "render/hd.hpp"

#include "core/log.hpp"
#include "core/png.hpp"
#include "shaders.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace ac3::render {
namespace {
constexpr u32 F_TEXTURED = 1, F_RAW = 2, F_SEMI = 4, F_STP_ONLY = 8, F_OPAQUE_ONLY = 16, F_COPY = 32, F_MASK_OR = 64,
              F_CHECK = 128, F_FLAT = 1024, F_HD = 2048,
              F_LIT = 4096,
              F_SMOOTH = 8192,
              F_STRIP = 16384,
              F_WATER = 32768;

s32 sx11(s32 v) { return static_cast<s32>(static_cast<u32>(v) << 21) >> 21; }
u32 rgba(u8 r, u8 g, u8 b) { return r | (g << 8) | (b << 16) | 0xFF000000u; }

struct Chosen {
    sim::PreciseVertex v;
    bool ok = false;
};
void resolve_precise(const sim::PreciseCache& cache, const machine::Gpu::Poly& p, int n, Chosen pv[4]) {
    constexpr int kCand = 32;
    sim::PreciseVertex c[4][kCand];
    int nc[4] = {};
    bool fixed[4] = {};
    for (int i = 0; i < n; ++i) {
        const u32 key = static_cast<u16>(p.x[i]) | (static_cast<u32>(static_cast<u16>(p.y[i])) << 16);
        nc[i] = cache.lookup_all(key, c[i], kCand);
        fixed[i] = nc[i] == 1;
        if (fixed[i]) {
            pv[i].v = c[i][0];
            pv[i].ok = true;
        }
    }
    for (int round = 0; round < n; ++round) {
        int pick = -1;
        for (int i = 0; i < n; ++i)
            if (!fixed[i] && nc[i] > 1 && (pick < 0 || nc[i] < nc[pick])) pick = i;
        if (pick < 0) break;
        float cx = 0, cy = 0, cz = 0;
        int nf = 0;
        for (int i = 0; i < n; ++i)
            if (fixed[i] && pv[i].ok) {
                cx += pv[i].v.vx;
                cy += pv[i].v.vy;
                cz += pv[i].v.vz;
                ++nf;
            }
        float cost[kCand];
        float best_d = 1e30f;
        for (int k = 0; k < nc[pick]; ++k) {
            const sim::PreciseVertex& q = c[pick][k];
            float d = 0;
            if (nf) {
                const float dx = q.vx - cx / static_cast<float>(nf), dy = q.vy - cy / static_cast<float>(nf),
                            dz = q.vz - cz / static_cast<float>(nf);
                d = dx * dx + dy * dy + dz * dz;
            } else {
                for (int j = 0; j < n; ++j) {
                    if (j == pick || !nc[j]) continue;
                    float m = 1e30f;
                    for (int l = 0; l < nc[j]; ++l) {
                        const float dx = q.vx - c[j][l].vx, dy = q.vy - c[j][l].vy, dz = q.vz - c[j][l].vz;
                        m = std::min(m, dx * dx + dy * dy + dz * dz);
                    }
                    d += m;
                }
            }
            cost[k] = d;
            best_d = std::min(best_d, d);
        }
        int best = 0;
        for (int k = 0; k < nc[pick]; ++k) {
            const float tol = 0.05f * c[pick][k].vz;
            if (cost[k] <= best_d * 4.0f + tol * tol) {
                best = k;
                break;
            }
        }
        pv[pick].v = c[pick][best];
        pv[pick].ok = true;
        fixed[pick] = true;
    }
}

struct PushPrim {
    float to_ndc[2];
    float inv_scale;
    float spec;
    float dir[3][4];
    float colour[3][4];
    float back[4];
};
static_assert(sizeof(PushPrim) == 128);
static_assert(Ocean::kShore == sim::PreciseCache::Shore::kSide);
struct PushPresent {
    float src[4];
};
struct PushPresent24 {
    float src[4];
    s32 origin[4];
};
struct PushDown {
    s32 scale;
};
}

bool HdRenderer::init(const Gpu& gpu, const Target& present, u32 scale, const sim::PreciseCache* precise) {
    gpu_ = &gpu;
    precise_ = precise;
    s_ = std::clamp(scale, 1u, 8u);
    w_ = 1024 * s_;
    h_ = 512 * s_;
    dirty_.assign(kBlocksX * kBlocksY, 1);
    drawn_.assign(kBlocksX * kBlocksY, 0);
    if (!make_scaled() ||
        !create_image(gpu, 1024, 512, VK_FORMAT_R16_UINT,
                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                      VK_IMAGE_ASPECT_COLOR_BIT, mirror_)) {
        destroy();
        return false;
    }
    one_shot(gpu, [&](VkCommandBuffer cb) {
        const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        for (Image* img : {&mirror_}) {
            image_barrier(cb, img->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            VkClearColorValue c{};
            vkCmdClearColorImage(cb, img->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &c, 1, &all);
            image_barrier(cb, img->image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT);
        }
    });

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &nearest_), "vkCreateSampler")) return destroy(), false;
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &linear_), "vkCreateSampler")) return destroy(), false;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.maxLod = VK_LOD_CLAMP_NONE;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &trilinear_), "vkCreateSampler")) return destroy(), false;

    VkDescriptorSetLayoutBinding b{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 1;
    dl.pBindings = &b;
    if (!vk_ok(vkCreateDescriptorSetLayout(gpu.device, &dl, nullptr, &set_layout_), "vkCreateDescriptorSetLayout"))
        return destroy(), false;
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 4;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (!vk_ok(vkCreateDescriptorPool(gpu.device, &pi, nullptr, &pool_), "vkCreateDescriptorPool")) return destroy(), false;
    const VkDescriptorSetLayout layouts[4] = {set_layout_, set_layout_, set_layout_, set_layout_};
    VkDescriptorSet sets[4];
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = 4;
    ai.pSetLayouts = layouts;
    if (!vk_ok(vkAllocateDescriptorSets(gpu.device, &ai, sets), "vkAllocateDescriptorSets")) return destroy(), false;
    mirror_set_ = sets[0];
    target_set_ = sets[1];
    picture_set_ = sets[2];
    strip_set_ = sets[3];
    const VkDescriptorImageInfo ii[3] = {{nearest_, mirror_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                         {linear_, target_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                         {nearest_, temp_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    VkWriteDescriptorSet w[3] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                 {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
    const VkDescriptorSet dst[3] = {sets[0], sets[1], sets[3]};
    for (int i = 0; i < 3; ++i) {
        w[i].dstSet = dst[i];
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].pImageInfo = &ii[i];
    }
    vkUpdateDescriptorSets(gpu.device, 3, w, 0, nullptr);
    if (!texture_dir.empty()) hd_tex_.load(texture_dir);
    if (!hd_tex_.init(gpu)) return destroy(), false;
    if (!ocean_.init(gpu)) return destroy(), false;
    if (!make_pipelines(present)) return destroy(), false;
    log::info("enhanced renderer: {}x{} ({}x native)", w_, h_, s_);
    return true;
}

bool HdRenderer::make_scaled() {
    const Gpu& gpu = *gpu_;
    constexpr VkImageUsageFlags kTarget = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!create_image(gpu, w_, h_, VK_FORMAT_R8G8B8A8_UNORM, kTarget, VK_IMAGE_ASPECT_COLOR_BIT, target_) ||
        !create_image(gpu, w_, h_, VK_FORMAT_R8G8B8A8_UNORM, kTarget, VK_IMAGE_ASPECT_COLOR_BIT, temp_))
        return false;
    if (depth_format_ == VK_FORMAT_UNDEFINED && !layout_) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(gpu.phys, VK_FORMAT_D32_SFLOAT, &fp);
        if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) depth_format_ = VK_FORMAT_D32_SFLOAT;
        else log::warn("enhanced renderer: no 32-bit depth; the models in the console's order");
    }
    if (depth_format_ != VK_FORMAT_UNDEFINED &&
        !create_image(gpu, w_, h_, depth_format_, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                      VK_IMAGE_ASPECT_DEPTH_BIT, depth_))
        return false;
    one_shot(gpu, [&](VkCommandBuffer cb) {
        const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        if (depth_.image) {
            const VkImageSubresourceRange whole{VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
            image_barrier(cb, depth_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT);
            const VkClearDepthStencilValue far{0.0f, 0};
            vkCmdClearDepthStencilImage(cb, depth_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &far, 1, &whole);
            image_barrier(cb, depth_.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT,
                          VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
        }
        image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkClearColorValue c{};
        vkCmdClearColorImage(cb, target_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &c, 1, &all);
        image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT);
        image_barrier(cb, temp_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT);
        vkCmdClearColorImage(cb, temp_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &c, 1, &all);
        image_barrier(cb, temp_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT);
    });
    if (strip_set_) {
        const VkDescriptorImageInfo ii{nearest_, temp_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = strip_set_;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(gpu.device, 1, &w, 0, nullptr);
    }
    if (target_set_) {
        const VkDescriptorImageInfo ii{linear_, target_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        w.dstSet = target_set_;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w.pImageInfo = &ii;
        vkUpdateDescriptorSets(gpu.device, 1, &w, 0, nullptr);
    }
    return true;
}

bool HdRenderer::set_scale(u32 scale) {
    scale = std::clamp(scale, 1u, 8u);
    if (!gpu_ || scale == s_) return true;
    need(0, 0, 1024, 512);
    vkDeviceWaitIdle(gpu_->device);
    bool ok = true;
    one_shot(*gpu_, [&](VkCommandBuffer cb) { ok = replay(cb, flush_); });
    render::destroy(*gpu_, target_);
    render::destroy(*gpu_, temp_);
    render::destroy(*gpu_, depth_);
    depth_list_ = ~0u;
    destroy_picture();
    s_ = scale;
    w_ = 1024 * s_;
    h_ = 512 * s_;
    if (!make_scaled()) {
        log::error("renderer: no memory for {}x{} ({}x native)", w_, h_, s_);
        return false;
    }
    copy_quad(0, 0, 1024, 512);
    log::info("renderer: {}x{} ({}x native)", w_, h_, s_);
    return ok;
}

void HdRenderer::destroy_picture() {
    const VkDevice d = gpu_->device;
    if (picture_view_) vkDestroyImageView(d, picture_view_, nullptr);
    if (picture_) vkDestroyImage(d, picture_, nullptr);
    if (picture_memory_) vkFreeMemory(d, picture_memory_, nullptr);
    picture_view_ = VK_NULL_HANDLE;
    picture_ = VK_NULL_HANDLE;
    picture_memory_ = VK_NULL_HANDLE;
    picture_w_ = picture_h_ = picture_levels_ = 0;
}

bool HdRenderer::create_picture(u32 w, u32 h) {
    const VkDevice d = gpu_->device;
    vkDeviceWaitIdle(d);
    destroy_picture();
    u32 levels = 1;
    while ((std::max(w, h) >> levels) != 0) ++levels;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = VK_FORMAT_R8G8B8A8_UNORM;
    ii.extent = {w, h, 1};
    ii.mipLevels = levels;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(d, &ii, nullptr, &picture_), "vkCreateImage")) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(d, picture_, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory(*gpu_, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX || !vk_ok(vkAllocateMemory(d, &ai, nullptr, &picture_memory_), "vkAllocateMemory"))
        return destroy_picture(), false;
    vkBindImageMemory(d, picture_, picture_memory_, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = picture_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = VK_FORMAT_R8G8B8A8_UNORM;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels, 0, 1};
    if (!vk_ok(vkCreateImageView(d, &vi, nullptr, &picture_view_), "vkCreateImageView")) return destroy_picture(), false;
    const VkDescriptorImageInfo di{trilinear_, picture_view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet wd{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    wd.dstSet = picture_set_;
    wd.descriptorCount = 1;
    wd.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    wd.pImageInfo = &di;
    vkUpdateDescriptorSets(d, 1, &wd, 0, nullptr);
    picture_w_ = w;
    picture_h_ = h;
    picture_levels_ = levels;
    return true;
}

namespace {
void levels_barrier(VkCommandBuffer cb, VkImage image, u32 base, u32 count, VkImageLayout from, VkImageLayout to,
                    VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access, VkPipelineStageFlags2 dst_stage,
                    VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = src_stage;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst_stage;
    b.dstAccessMask = dst_access;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, base, count, 0, 1};
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &dep);
}
}

void HdRenderer::make_picture(VkCommandBuffer cb) {
    const machine::Gpu::Display& d = shown_;
    if (d.rgb24 || d.w <= 0 || d.h <= 0) return;
    const s32 x0 = std::clamp(d.x, 0, 1024), y0 = std::clamp(d.y, 0, 512);
    const s32 x1 = std::clamp(d.x + d.w, 0, 1024), y1 = std::clamp(d.y + d.h, 0, 512);
    if (x0 >= x1 || y0 >= y1) return;
    const u32 w = static_cast<u32>(x1 - x0) * s_, h = static_cast<u32>(y1 - y0) * s_;
    if ((w != picture_w_ || h != picture_h_) && !create_picture(w, h)) return;
    image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_READ_BIT);
    levels_barrier(cb, picture_, 0, picture_levels_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkImageCopy ic{};
    ic.srcSubresource = ic.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    ic.srcOffset = {x0 * static_cast<s32>(s_), y0 * static_cast<s32>(s_), 0};
    ic.extent = {w, h, 1};
    vkCmdCopyImage(cb, target_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, picture_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &ic);
    for (u32 i = 1; i < picture_levels_; ++i) {
        levels_barrier(cb, picture_, i - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                       VK_ACCESS_2_TRANSFER_READ_BIT);
        VkImageBlit b{};
        b.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i - 1, 0, 1};
        b.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, i, 0, 1};
        b.srcOffsets[1] = {static_cast<s32>(std::max(w >> (i - 1), 1u)), static_cast<s32>(std::max(h >> (i - 1), 1u)), 1};
        b.dstOffsets[1] = {static_cast<s32>(std::max(w >> i, 1u)), static_cast<s32>(std::max(h >> i, 1u)), 1};
        vkCmdBlitImage(cb, picture_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, picture_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &b, VK_FILTER_LINEAR);
    }
    if (picture_levels_ > 1)
        levels_barrier(cb, picture_, 0, picture_levels_ - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                       VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                       VK_ACCESS_2_SHADER_READ_BIT);
    levels_barrier(cb, picture_, picture_levels_ - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                   VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_READ_BIT);
}

bool HdRenderer::make_pipelines(const Target& present) {
    const Gpu& gpu = *gpu_;
    VkPushConstantRange pr{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushPrim)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    const VkDescriptorSetLayout prim_sets[4] = {set_layout_, hd_tex_.set_layout(), set_layout_, ocean_.set_layout()};
    li.setLayoutCount = 4;
    li.pSetLayouts = prim_sets;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pr;
    if (!vk_ok(vkCreatePipelineLayout(gpu.device, &li, nullptr, &layout_), "vkCreatePipelineLayout")) return false;
    li.setLayoutCount = 1;
    li.pSetLayouts = &set_layout_;
    VkPushConstantRange pp{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushPresent)};
    li.pPushConstantRanges = &pp;
    if (!vk_ok(vkCreatePipelineLayout(gpu.device, &li, nullptr, &present_layout_), "vkCreatePipelineLayout"))
        return false;

    VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                 {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].pName = "main";
    VkPipelineInputAssemblyStateCreateInfo ia{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vps{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vps.viewportCount = 1;
    vps.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo ms{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cbs{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cbs.attachmentCount = 1;
    cbs.pAttachments = &cba;
    for (float& k : cbs.blendConstants) k = 0.5f;

    VkVertexInputBindingDescription bind{0, sizeof(Vtx), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[6] = {
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Vtx, x)},
        {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vtx, u)},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Vtx, rgba)},
        {3, 0, VK_FORMAT_R32G32B32A32_UINT, offsetof(Vtx, info)},
        {4, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vtx, view)},
        {5, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vtx, normal)},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1;
    vi.pVertexBindingDescriptions = &bind;
    vi.vertexAttributeDescriptionCount = 6;
    vi.pVertexAttributeDescriptions = attrs;
    const VkFormat vram_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkPipelineRenderingCreateInfo rinfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &vram_format;
    rinfo.depthAttachmentFormat = depth_format_;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
    VkDynamicState prim_dyn[4] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                                  VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE};
    dy.dynamicStateCount = 4;
    dy.pDynamicStates = prim_dyn;
    VkGraphicsPipelineCreateInfo gp{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    gp.pNext = &rinfo;
    gp.stageCount = 2;
    gp.pStages = stages;
    gp.pVertexInputState = &vi;
    gp.pInputAssemblyState = &ia;
    gp.pViewportState = &vps;
    gp.pRasterizationState = &rs;
    gp.pMultisampleState = &ms;
    gp.pDepthStencilState = &ds;
    gp.pColorBlendState = &cbs;
    gp.pDynamicState = &dy;
    gp.layout = layout_;
    stages[0].module = make_shader(gpu, spv_hd_prim_vert, spv_hd_prim_vert_size);
    stages[1].module = make_shader(gpu, spv_hd_prim_frag, spv_hd_prim_frag_size);
    bool ok = true;
    for (int p = 0; p < kPipes && ok; ++p) {
        cba = VkPipelineColorBlendAttachmentState{};
        cba.colorWriteMask = 0xF;
        if (p != kOpaque) {
            cba.blendEnable = VK_TRUE;
            cba.srcColorBlendFactor = p == kHalf ? VK_BLEND_FACTOR_CONSTANT_COLOR : VK_BLEND_FACTOR_ONE;
            cba.dstColorBlendFactor = p == kHalf ? VK_BLEND_FACTOR_CONSTANT_COLOR : VK_BLEND_FACTOR_ONE;
            cba.colorBlendOp = p == kSub ? VK_BLEND_OP_REVERSE_SUBTRACT : VK_BLEND_OP_ADD;
            cba.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            cba.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            cba.alphaBlendOp = VK_BLEND_OP_ADD;
        }
        ok = vk_ok(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipes_[p]),
                   "vkCreateGraphicsPipelines");
    }
    vkDestroyShaderModule(gpu.device, stages[0].module, nullptr);
    vkDestroyShaderModule(gpu.device, stages[1].module, nullptr);
    if (!ok) return false;
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    ds = VkPipelineDepthStencilStateCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    VkPipelineVertexInputStateCreateInfo none{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    gp.pVertexInputState = &none;
    cba = VkPipelineColorBlendAttachmentState{};
    cba.colorWriteMask = 0xF;
    rinfo.pColorAttachmentFormats = &present.color_format;
    rinfo.depthAttachmentFormat = present.depth_format;
    gp.layout = present_layout_;
    stages[0].module = make_shader(gpu, spv_hd_present_vert, spv_hd_present_vert_size);
    stages[1].module = make_shader(gpu, spv_hd_present_frag, spv_hd_present_frag_size);
    ok = vk_ok(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &gp, nullptr, &present_),
               "vkCreateGraphicsPipelines");
    vkDestroyShaderModule(gpu.device, stages[1].module, nullptr);
    if (!ok) {
        vkDestroyShaderModule(gpu.device, stages[0].module, nullptr);
        return false;
    }
    VkPushConstantRange p24{VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushPresent24)};
    li.pPushConstantRanges = &p24;
    ok = vk_ok(vkCreatePipelineLayout(gpu.device, &li, nullptr, &present24_layout_), "vkCreatePipelineLayout");
    if (ok) {
        gp.layout = present24_layout_;
        stages[1].module = make_shader(gpu, spv_hd_present24_frag, spv_hd_present24_frag_size);
        ok = vk_ok(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &gp, nullptr, &present24_),
                   "vkCreateGraphicsPipelines");
        vkDestroyShaderModule(gpu.device, stages[1].module, nullptr);
    }
    vkDestroyShaderModule(gpu.device, stages[0].module, nullptr);
    if (!ok) return false;

    VkPushConstantRange pd{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushDown)};
    li.pPushConstantRanges = &pd;
    if (!vk_ok(vkCreatePipelineLayout(gpu.device, &li, nullptr, &down_layout_), "vkCreatePipelineLayout"))
        return false;
    const VkFormat native_format = VK_FORMAT_R16_UINT;
    rinfo.pColorAttachmentFormats = &native_format;
    rinfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
    gp.layout = down_layout_;
    stages[0].module = make_shader(gpu, spv_hd_down_vert, spv_hd_down_vert_size);
    stages[1].module = make_shader(gpu, spv_hd_down_frag, spv_hd_down_frag_size);
    ok = vk_ok(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &gp, nullptr, &down_),
               "vkCreateGraphicsPipelines");
    vkDestroyShaderModule(gpu.device, stages[0].module, nullptr);
    vkDestroyShaderModule(gpu.device, stages[1].module, nullptr);
    return ok;
}

void HdRenderer::destroy() {
    if (!gpu_) return;
    const VkDevice d = gpu_->device;
    vkDeviceWaitIdle(d);
    hd_tex_.destroy();
    ocean_.destroy();
    for (VkPipeline& p : pipes_)
        if (p) vkDestroyPipeline(d, p, nullptr), p = VK_NULL_HANDLE;
    if (present_) vkDestroyPipeline(d, present_, nullptr);
    if (down_) vkDestroyPipeline(d, down_, nullptr);
    if (present24_) vkDestroyPipeline(d, present24_, nullptr);
    if (present24_layout_) vkDestroyPipelineLayout(d, present24_layout_, nullptr);
    if (layout_) vkDestroyPipelineLayout(d, layout_, nullptr);
    if (present_layout_) vkDestroyPipelineLayout(d, present_layout_, nullptr);
    if (down_layout_) vkDestroyPipelineLayout(d, down_layout_, nullptr);
    if (pool_) vkDestroyDescriptorPool(d, pool_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(d, set_layout_, nullptr);
    destroy_picture();
    if (nearest_) vkDestroySampler(d, nearest_, nullptr);
    if (linear_) vkDestroySampler(d, linear_, nullptr);
    if (trilinear_) vkDestroySampler(d, trilinear_, nullptr);
    render::destroy(*gpu_, target_);
    render::destroy(*gpu_, temp_);
    render::destroy(*gpu_, depth_);
    depth_format_ = VK_FORMAT_UNDEFINED;
    render::destroy(*gpu_, mirror_);
    for (Frame& f : frames_) {
        render::destroy(*gpu_, f.vertices);
        render::destroy(*gpu_, f.staging);
    }
    render::destroy(*gpu_, flush_.vertices);
    render::destroy(*gpu_, flush_.staging);
    render::destroy(*gpu_, readback_);
    present_ = present24_ = down_ = VK_NULL_HANDLE;
    layout_ = present_layout_ = down_layout_ = present24_layout_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    set_layout_ = VK_NULL_HANDLE;
    nearest_ = linear_ = trilinear_ = VK_NULL_HANDLE;
    gpu_ = nullptr;
}

void HdRenderer::gpu_node(const machine::Gpu&, u32 addr) {
    node_ = addr & 0x00FFFFFFu;
    if (hints_ && hints_->wide && (node_ == hints_->hud_slot[0] || node_ == hints_->hud_slot[1])) in_hud_ = true;
}

float HdRenderer::hud_squeeze(const machine::Gpu& g, float& centre) const {
    if (!in_hud_ || !hints_ || !hints_->wide) return 1.0f;
    const s32* s = hints_->screen;
    if (g.regs.area_x0 < s[0] || g.regs.area_x1 >= s[0] + s[2] || g.regs.area_y0 < s[1] ||
        g.regs.area_y1 >= s[1] + s[3])
        return 1.0f;
    centre = hints_->centre;
    return hints_->squeeze;
}

HdRenderer::Pipe HdRenderer::pipe_for(bool semi, u16 tpage) {
    if (!semi) return kOpaque;
    switch ((tpage >> 5) & 3u) {
    case 0: return kHalf;
    case 2: return kSub;
    default: return kAdd;
    }
}

void HdRenderer::mark_dirty(s32 x0, s32 y0, s32 x1, s32 y1) {
    x0 = std::clamp(x0, 0, 1024);
    x1 = std::clamp(x1, 0, 1024);
    y0 = std::clamp(y0, 0, 512);
    y1 = std::clamp(y1, 0, 512);
    if (x0 >= x1 || y0 >= y1) return;
    for (s32 by = y0 / kBlock; by <= (y1 - 1) / kBlock; ++by)
        for (s32 bx = x0 / kBlock; bx <= (x1 - 1) / kBlock; ++bx) dirty_[static_cast<size_t>(by * kBlocksX + bx)] = 1;
}

void HdRenderer::mark_drawn(s32 x0, s32 y0, s32 x1, s32 y1) {
    mark_dirty(x0, y0, x1, y1);
    set_blocks(drawn_, x0, y0, x1, y1, false, 1);
}

void HdRenderer::set_blocks(std::vector<u8>& map, s32 x0, s32 y0, s32 x1, s32 y1, bool whole, u8 v) {
    x0 = std::clamp(x0, 0, 1024);
    x1 = std::clamp(x1, 0, 1024);
    y0 = std::clamp(y0, 0, 512);
    y1 = std::clamp(y1, 0, 512);
    if (x0 >= x1 || y0 >= y1) return;
    const s32 bx0 = whole ? (x0 + kBlock - 1) / kBlock : x0 / kBlock, bx1 = whole ? x1 / kBlock : (x1 - 1) / kBlock + 1;
    const s32 by0 = whole ? (y0 + kBlock - 1) / kBlock : y0 / kBlock, by1 = whole ? y1 / kBlock : (y1 - 1) / kBlock + 1;
    for (s32 by = by0; by < by1; ++by)
        for (s32 bx = bx0; bx < bx1; ++bx) map[static_cast<size_t>(by * kBlocksX + bx)] = v;
}

bool HdRenderer::any_block(const std::vector<u8>& map, s32 x0, s32 y0, s32 x1, s32 y1) const {
    x0 = std::clamp(x0, 0, 1024);
    x1 = std::clamp(x1, 0, 1024);
    y0 = std::clamp(y0, 0, 512);
    y1 = std::clamp(y1, 0, 512);
    if (x0 >= x1 || y0 >= y1) return false;
    for (s32 by = y0 / kBlock; by <= (y1 - 1) / kBlock; ++by)
        for (s32 bx = x0 / kBlock; bx <= (x1 - 1) / kBlock; ++bx)
            if (map[static_cast<size_t>(by * kBlocksX + bx)]) return true;
    return false;
}

void HdRenderer::need(s32 x0, s32 y0, s32 x1, s32 y1) {
    x0 = std::clamp(x0, 0, 1024);
    x1 = std::clamp(x1, 0, 1024);
    y0 = std::clamp(y0, 0, 512);
    y1 = std::clamp(y1, 0, 512);
    if (x0 >= x1 || y0 >= y1) return;
    const s32 bx0 = x0 / kBlock, bx1 = (x1 - 1) / kBlock, by0 = y0 / kBlock, by1 = (y1 - 1) / kBlock;
    bool any = false;
    for (s32 by = by0; by <= by1 && !any; ++by)
        for (s32 bx = bx0; bx <= bx1; ++bx)
            if (dirty_[static_cast<size_t>(by * kBlocksX + bx)]) {
                any = true;
                break;
            }
    if (!any) return;
    Cmd c;
    c.op = kDown;
    c.x = bx0 * kBlock;
    c.y = by0 * kBlock;
    c.w = (bx1 - bx0 + 1) * kBlock;
    c.h = (by1 - by0 + 1) * kBlock;
    cmds_.push_back(c);
    for (s32 by = by0; by <= by1; ++by)
        for (s32 bx = bx0; bx <= bx1; ++bx) dirty_[static_cast<size_t>(by * kBlocksX + bx)] = 0;
}

bool HdRenderer::strip_of(u16 tpage, u16 clut) const {
    const u32 depth = (tpage >> 7) & 3u;
    if (!strip_.valid || depth > 1) return false;
    const s32 cx = (clut & 63) * 16, cy = (clut >> 6) & 511;
    return cx == strip_.x && cy == strip_.y && strip_.w >= (depth == 0 ? 16 : 256);
}

void HdRenderer::need_texture(u16 tpage, u16 clut, u32 twin_mask, int u0, int v0, int u1, int v1) {
    if (twin_mask) {
        u0 = v0 = 0;
        u1 = v1 = 255;
    }
    const s32 page_x = (tpage & 15) * 64, page_y = ((tpage >> 4) & 1) * 256;
    const u32 depth = (tpage >> 7) & 3u;
    const int sh = depth == 0 ? 2 : depth == 1 ? 1 : 0;
    need(page_x + (u0 >> sh), page_y + v0, page_x + (u1 >> sh) + 1, page_y + v1 + 1);
    if (depth < 2) {
        const s32 cx = (clut & 63) * 16, cy = (clut >> 6) & 511;
        need(cx, cy, cx + (depth == 0 ? 16 : 256), cy + 1);
    }
}

void HdRenderer::sync(const machine::Gpu& g, s32 x, s32 y, s32 w, s32 h) {
    const s32 x1 = std::min(x + w, 1024), y1 = std::min(y + h, 512);
    x = std::max(x, 0);
    y = std::max(y, 0);
    if (x >= x1 || y >= y1) return;
    w = x1 - x;
    h = y1 - y;
    Cmd c;
    c.op = kSync;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.first = static_cast<u32>(staging_.size());
    for (s32 j = 0; j < h; ++j) {
        const u16* row = g.vram.data() + static_cast<size_t>(y + j) * 1024 + static_cast<size_t>(x);
        staging_.insert(staging_.end(), row, row + w);
    }
    cmds_.push_back(c);
    for (s32 by = (y + kBlock - 1) / kBlock; by < y1 / kBlock; ++by)
        for (s32 bx = (x + kBlock - 1) / kBlock; bx < x1 / kBlock; ++bx) dirty_[static_cast<size_t>(by * kBlocksX + bx)] = 0;
}

void HdRenderer::emit(Pipe pipe, const machine::Gpu& g, const Vtx* v, u32 n) {
    s32 x = g.regs.area_x0, w = g.regs.area_x1 + 1 - x;
    const s32 y = g.regs.area_y0, h = g.regs.area_y1 + 1 - y;
    float centre = 0;
    if (const float squeeze = hud_squeeze(g, centre);
        squeeze != 1.0f && !(x <= hints_->screen[0] && x + w >= hints_->screen[0] + hints_->screen[2])) {
        const s32 x0 = static_cast<s32>(std::floor(centre + (static_cast<float>(x) - centre) * squeeze));
        const s32 x1 = static_cast<s32>(std::ceil(centre + (static_cast<float>(x + w) - centre) * squeeze));
        x = x0;
        w = x1 - x0;
    }
    if (w <= 0 || h <= 0) return;
    const u32 first = static_cast<u32>(verts_.size());
    verts_.insert(verts_.end(), v, v + n);
    const u8 depth = emit_depth_ == kDepthWrite && pipe != kOpaque ? static_cast<u8>(kDepthTest) : emit_depth_;
    if (!cmds_.empty()) {
        Cmd& last = cmds_.back();
        if (last.op == kDraw && last.pipe == pipe && last.x == x && last.y == y && last.w == w && last.h == h &&
            last.light == emit_light_ && last.depth == depth && last.first + last.count == first) {
            last.count += n;
            return;
        }
    }
    Cmd c;
    c.op = kDraw;
    c.pipe = pipe;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.first = first;
    c.count = n;
    c.light = emit_light_;
    c.depth = depth;
    cmds_.push_back(c);
}

void HdRenderer::water_mean(const machine::Gpu& g, u16 tpage, u16 clut, u16 twin_mask, u16 twin_off, int u0, int v0,
                            int u1, int v1, float rgb[3]) {
    const u32 texture = tpage | (static_cast<u32>(clut) << 16), window = twin_mask | (static_cast<u32>(twin_off) << 16);
    const u32 texels = static_cast<u32>(u0) | (static_cast<u32>(v0) << 8) | (static_cast<u32>(u1) << 16) |
                       (static_cast<u32>(v1) << 24);
    for (const WaterMean& w : water_means_)
        if (w.texture == texture && w.window == window && w.texels == texels) {
            std::memcpy(rgb, w.rgb, sizeof w.rgb);
            return;
        }
    WaterMean w;
    w.texture = texture;
    w.window = window;
    w.texels = texels;
    const u32 depth = (tpage >> 7) & 3u;
    const s32 page_x = (tpage & 15) * 64, page_y = ((tpage >> 4) & 1) * 256;
    const s32 clut_x = (clut & 63) * 16, clut_y = (clut >> 6) & 511;
    const u32 mu = twin_mask & 0xFFu, mv = twin_mask >> 8, ou = twin_off & 0xFFu, ov = twin_off >> 8;
    const u32 shift = depth == 0 ? 2 : depth == 1 ? 1 : 0;
    s32 tx0 = page_x + (u0 >> shift), tx1 = page_x + (u1 >> shift) + 1, ty0 = page_y + v0, ty1 = page_y + v1 + 1;
    if (twin_mask) {
        tx0 = page_x;
        tx1 = page_x + (256 >> shift);
        ty0 = page_y;
        ty1 = page_y + 256;
    }
    auto exact_at = [&](s32 x0, s32 y0, s32 x1, s32 y1) {
        return !any_block(drawn_, x0, y0, x1, y1) || x0 > g.regs.area_x1 || x1 <= g.regs.area_x0 ||
               y0 > g.regs.area_y1 || y1 <= g.regs.area_y0;
    };
    const bool exact = exact_at(tx0, ty0, tx1, ty1) &&
                       (depth >= 2 || exact_at(clut_x, clut_y, clut_x + (depth == 0 ? 16 : 256), clut_y + 1));
    if (exact) {
        double sum[3] = {};
        u32 count = 0;
        for (int v = v0; v <= v1; ++v)
            for (int u = u0; u <= u1; ++u) {
                const u32 tu = (static_cast<u32>(u) & (~mu & 255u)) | (ou & mu);
                const u32 tv = (static_cast<u32>(v) & (~mv & 255u)) | (ov & mv);
                auto at = [&](s32 x, s32 y) { return g.vram[(static_cast<u32>(y) & 511u) * 1024u + (static_cast<u32>(x) & 1023u)]; };
                u16 t;
                if (depth == 0) t = at(clut_x + ((at(page_x + static_cast<s32>(tu >> 2), page_y + static_cast<s32>(tv)) >> ((tu & 3u) * 4u)) & 15), clut_y);
                else if (depth == 1) t = at(clut_x + ((at(page_x + static_cast<s32>(tu >> 1), page_y + static_cast<s32>(tv)) >> ((tu & 1u) * 8u)) & 255), clut_y);
                else t = at(page_x + static_cast<s32>(tu), page_y + static_cast<s32>(tv));
                if (!t) continue;
                sum[0] += t & 31;
                sum[1] += (t >> 5) & 31;
                sum[2] += (t >> 10) & 31;
                ++count;
            }
        if (count)
            for (int c = 0; c < 3; ++c) w.rgb[c] = static_cast<float>(sum[c] / (31.0 * count));
    }
    water_means_.push_back(w);
    std::memcpy(rgb, w.rgb, sizeof w.rgb);
}

void HdRenderer::gpu_poly(const machine::Gpu& g, const machine::Gpu::Poly& p) {
    const int n = p.nverts == 4 ? 4 : 3;
    const float S = static_cast<float>(s_);
    u32 flags = 0;
    if (p.textured) flags |= F_TEXTURED;
    if (p.textured && p.raw) flags |= F_RAW;
    if (p.semitrans) flags |= F_SEMI;
    if (g.regs.mask_or) flags |= F_MASK_OR;
    if (g.regs.mask_and) flags |= F_CHECK;
    if (!p.gouraud) flags |= F_FLAT;
    flags |= static_cast<u32>((p.tpage >> 5) & 3u) << 8;
    if (p.textured && strip_of(p.tpage, p.clut)) flags |= F_STRIP;
    if (p.textured && hd_textures && !p.twin_mask)
        if (const u32 layer = hd_tex_.layer_for(g, p.tpage, p.clut, frame_)) flags |= F_HD | ((layer - 1) << 16);

    Chosen pv[4];
    const sim::PreciseCache::Lit* lit = nullptr;
    const sim::PreciseCache::Light* light = nullptr;
    s32 water_index = -1;
    float depth_unit = 0.0f;
    if (precise_) {
        if (const sim::PreciseCache::Packet* kept = precise_->lookup_packet(node_, &lit, &light)) {
            if (p.textured && ((kept->fx & 1u) || ((kept->fx & 2u) && ground_filter))) flags |= F_SMOOTH;
            if ((kept->fx & 4u) && water && ocean_.ready() && precise_->water.valid) {
                const sim::PreciseCache::Water& w = precise_->water;
                if (waters_.empty() || std::memcmp(&waters_.back(), &w, sizeof w) != 0) waters_.push_back(w);
                water_index = static_cast<s32>(waters_.size() - 1);
                flags |= F_WATER;
                ocean_time_ = w.time;
                ocean_due_ = true;
            }
            for (int i = 0; i < n; ++i) {
                pv[i].ok = (kept->ok >> i) & 1u;
                if (pv[i].ok) pv[i].v = kept->v[i];
            }
            if ((kept->fx & 8u) && model_depth && depth_.image && kept->ok == (n == 4 ? 15u : 7u))
                depth_unit = std::ldexp(1.0f, -static_cast<int>(kept->depth_shift));
        } else {
            resolve_precise(*precise_, p, n, pv);
        }
    }
    s32 light_index = -1;
    if ((flags & F_HD) && hd_lighting && lit && light) {
        for (size_t i = 0; i < light_from_.size(); ++i)
            if (light_from_[i] == light) light_index = static_cast<s32>(i);
        if (light_index < 0) {
            light_from_.push_back(light);
            lights_.push_back(*light);
            light_index = static_cast<s32>(lights_.size() - 1);
        }
        flags |= F_LIT;
    }
    float centre = 0;
    const float squeeze = hud_squeeze(g, centre);
    s32 ix[4], iy[4];
    float fx[4], fy[4], fz[4];
    s32 x0 = 0x7FFFFFFF, y0 = 0x7FFFFFFF, x1 = -0x7FFFFFFF, y1 = -0x7FFFFFFF;
    int u0 = 255, v0 = 255, u1 = 0, v1 = 0;
    for (int i = 0; i < n; ++i) {
        ix[i] = sx11(p.x[i]) + g.regs.ofs_x;
        iy[i] = sx11(p.y[i]) + g.regs.ofs_y;
        if (pv[i].ok) {
            fx[i] = pv[i].v.sx + static_cast<float>(g.regs.ofs_x);
            fy[i] = pv[i].v.sy + static_cast<float>(g.regs.ofs_y);
            fz[i] = pv[i].v.vz;
        } else {
            fx[i] = static_cast<float>(ix[i]);
            fy[i] = static_cast<float>(iy[i]);
            fz[i] = 0.0f;
        }
        if (squeeze != 1.0f) {
            fx[i] = centre + (fx[i] - centre) * squeeze;
            ix[i] = static_cast<s32>(std::lround(centre + (static_cast<float>(ix[i]) - centre) * squeeze));
        }
        x0 = std::min(x0, ix[i]);
        x1 = std::max(x1, ix[i]);
        y0 = std::min(y0, iy[i]);
        y1 = std::max(y1, iy[i]);
        u0 = std::min<int>(u0, p.u[i]);
        u1 = std::max<int>(u1, p.u[i]);
        v0 = std::min<int>(v0, p.v[i]);
        v1 = std::max<int>(v1, p.v[i]);
    }

    float depth_bias = 1.0f;
    if (depth_unit > 0.0f) {
        const s32 area[4] = {g.regs.area_x0, g.regs.area_y0, g.regs.area_x1, g.regs.area_y1};
        if (depth_list_ != list_serial_ || std::memcmp(area, depth_area_, sizeof area) != 0) {
            depth_list_ = list_serial_;
            std::memcpy(depth_area_, area, sizeof area);
            depth_polys_ = 0;
            Cmd c;
            c.op = kClearDepth;
            c.x = area[0];
            c.y = area[1];
            c.w = area[2] + 1 - area[0];
            c.h = area[3] + 1 - area[1];
            cmds_.push_back(c);
        }
        depth_bias = 1.0f + 4e-6f * static_cast<float>(std::min(depth_polys_++, 20000u));
    }

    float mean[3] = {-1, 0, 0};
    if ((flags & F_WATER) && p.textured) water_mean(g, p.tpage, p.clut, p.twin_mask, p.twin_off, u0, v0, u1, v1, mean);

    Vtx tri[6];
    u32 count = 0;
    static constexpr int kTri[2][3] = {{0, 1, 2}, {1, 2, 3}};
    for (int t = 0; t < (n == 4 ? 2 : 1); ++t) {
        const int* k = kTri[t];
        s32 minx = ix[k[0]], maxx = minx, miny = iy[k[0]], maxy = miny;
        for (int j = 1; j < 3; ++j) {
            minx = std::min(minx, ix[k[j]]);
            maxx = std::max(maxx, ix[k[j]]);
            miny = std::min(miny, iy[k[j]]);
            maxy = std::max(maxy, iy[k[j]]);
        }
        const bool precise = pv[k[0]].ok && pv[k[1]].ok && pv[k[2]].ok;
        if (maxy - miny >= 512 || maxx - minx >= 1024) continue;
        if (precise) {
            const float a = (fx[k[1]] - fx[k[0]]) * (fy[k[2]] - fy[k[1]]) - (fx[k[2]] - fx[k[1]]) * (fy[k[1]] - fy[k[0]]);
            if (a == 0.0f) continue;
        } else {
            if (miny == maxy) continue;
            const s32 area = (ix[k[1]] - ix[k[0]]) * (iy[k[2]] - iy[k[1]]) - (ix[k[2]] - ix[k[1]]) * (iy[k[1]] - iy[k[0]]);
            if (!area) continue;
        }
        float px[3], py[3];
        for (int j = 0; j < 3; ++j) {
            px[j] = precise ? fx[k[j]] : static_cast<float>(ix[k[j]]);
            py[j] = precise ? fy[k[j]] : static_cast<float>(iy[k[j]]);
        }
        u32 umin = 255, vmin = 255, umax = 0, vmax = 0;
        for (int j = 0; j < 3; ++j) {
            umin = std::min<u32>(umin, p.u[k[j]]);
            umax = std::max<u32>(umax, p.u[k[j]]);
            vmin = std::min<u32>(vmin, p.v[k[j]]);
            vmax = std::max<u32>(vmax, p.v[k[j]]);
        }
        const u32 texels = umin | (vmin << 8) | (umax << 16) | (vmax << 24);
        float du = 0.0f, dv = 0.0f;
        if (p.textured) {
            const float ex1 = px[1] - px[0], ey1 = py[1] - py[0];
            const float ex2 = px[2] - px[0], ey2 = py[2] - py[0];
            const float det = ex1 * ey2 - ex2 * ey1;
            if (det != 0.0f) {
                const float a1 = static_cast<float>(p.u[k[1]]) - static_cast<float>(p.u[k[0]]);
                const float a2 = static_cast<float>(p.u[k[2]]) - static_cast<float>(p.u[k[0]]);
                const float b1 = static_cast<float>(p.v[k[1]]) - static_cast<float>(p.v[k[0]]);
                const float b2 = static_cast<float>(p.v[k[2]]) - static_cast<float>(p.v[k[0]]);
                const float ux = (a1 * ey2 - a2 * ey1) / det, uy = (ex1 * a2 - ex2 * a1) / det;
                const float vx = (b1 * ey2 - b2 * ey1) / det, vy = (ex1 * b2 - ex2 * b1) / det;
                du = 0.5f - 0.5f * (ux + uy);
                dv = 0.5f - 0.5f * (vx + vy);
            }
        }
        for (int j = 0; j < 3; ++j) {
            const int i = k[j];
            Vtx& o = tri[count++];
            o.x = px[j] * S;
            o.y = py[j] * S;
            o.w = precise && perspective ? fz[i] : 1.0f;
            o.depth = depth_unit > 0.0f ? std::min(1.0f, 16.0f * depth_bias / std::max(fz[i] * depth_unit, 16.0f)) : 0.0f;
            o.u = static_cast<float>(p.u[i]) + du;
            o.v = static_cast<float>(p.v[i]) + dv;
            o.rgba = rgba(p.r[i], p.g[i], p.b[i]);
            o.info[0] = p.tpage | (static_cast<u32>(p.clut) << 16);
            o.info[1] = p.twin_mask | (static_cast<u32>(p.twin_off) << 16);
            o.info[2] = flags;
            o.info[3] = texels;
            if (pv[i].ok) {
                o.view[0] = pv[i].v.vx;
                o.view[1] = pv[i].v.vy;
                o.view[2] = pv[i].v.vz;
            }
            if (light_index >= 0)
                for (int c = 0; c < 3; ++c) o.normal[c] = lit->n[i][c];
            else if (flags & F_WATER)
                for (int c = 0; c < 3; ++c) o.normal[c] = mean[c];
        }
        ++prims_;
        if (precise) ++precise_count_;
    }
    if (!count) return;
    if (p.textured) need_texture(p.tpage, p.clut, p.twin_mask, u0, v0, u1, v1);
    x0 -= 1;
    y0 -= 1;
    x1 += 1;
    y1 += 1;
    if (g.regs.mask_and) need(x0, y0, x1 + 1, y1 + 1);
    emit_light_ = light_index >= 0 ? light_index : water_index >= 0 ? -2 - water_index : -1;
    emit_depth_ = depth_unit > 0.0f ? kDepthWrite : kNoDepth;
    if (p.textured && p.semitrans) {
        for (u32 i = 0; i < count; ++i) tri[i].info[2] = flags | F_OPAQUE_ONLY;
        emit(kOpaque, g, tri, count);
        for (u32 i = 0; i < count; ++i) tri[i].info[2] = flags | F_STP_ONLY;
        emit(pipe_for(true, p.tpage), g, tri, count);
    } else {
        emit(pipe_for(p.semitrans != 0, p.tpage), g, tri, count);
    }
    emit_light_ = -1;
    emit_depth_ = kNoDepth;
    mark_drawn(std::max(x0, g.regs.area_x0), std::max(y0, g.regs.area_y0), std::min(x1, g.regs.area_x1) + 1,
               std::min(y1, g.regs.area_y1) + 1);
}

void HdRenderer::gpu_rect(const machine::Gpu& g, const machine::Gpu::Rect& r) {
    const s32 x = sx11(sx11(r.x) + g.regs.ofs_x), y = sx11(sx11(r.y) + g.regs.ofs_y);
    const s32 w = r.w & 0x3FF, h = r.h & 0x1FF;
    if (!w || !h) return;
    const float S = static_cast<float>(s_);
    int u_inc = 1, v_inc = 1, u = r.u, v = r.v;
    if (r.textured) {
        if (g.regs.texpage & 0x1000u) {
            u_inc = -1;
            u |= 1;
        }
        if (g.regs.texpage & 0x2000u) v_inc = -1;
    }
    const float ul = static_cast<float>(u_inc > 0 ? u : u + 1), vt = static_cast<float>(v_inc > 0 ? v : v + 1);
    const float ur = ul + static_cast<float>(u_inc * w), vb = vt + static_cast<float>(v_inc * h);
    u32 flags = 0;
    if (r.textured) flags |= F_TEXTURED;
    if (r.textured && r.raw) flags |= F_RAW;
    if (r.semitrans) flags |= F_SEMI;
    if (g.regs.mask_or) flags |= F_MASK_OR;
    if (g.regs.mask_and) flags |= F_CHECK;
    flags |= F_FLAT;
    flags |= static_cast<u32>((r.tpage >> 5) & 3u) << 8;
    if (r.textured && strip_of(r.tpage, r.clut)) flags |= F_STRIP;
    auto vert = [&](float px, float py, float pu, float pv) {
        Vtx o{};
        o.x = px * S;
        o.y = py * S;
        o.w = 1.0f;
        o.u = pu;
        o.v = pv;
        o.rgba = rgba(r.r, r.g, r.b);
        o.info[0] = r.tpage | (static_cast<u32>(r.clut) << 16);
        o.info[1] = r.twin_mask | (static_cast<u32>(r.twin_off) << 16);
        o.info[2] = flags;
        return o;
    };
    float fx = static_cast<float>(x), fw = static_cast<float>(w);
    const float fy = static_cast<float>(y), fh = static_cast<float>(h);
    float centre = 0;
    if (const float squeeze = hud_squeeze(g, centre); squeeze != 1.0f && w < hints_->screen[2] &&
        !(h >= hints_->screen[3] &&
          (x <= hints_->screen[0] || x + w >= hints_->screen[0] + hints_->screen[2]))) {
        fx = centre + (fx - centre) * squeeze;
        fw *= squeeze;
    }
    const Vtx tl = vert(fx, fy, ul, vt), tr = vert(fx + fw, fy, ur, vt), bl = vert(fx, fy + fh, ul, vb),
              br = vert(fx + fw, fy + fh, ur, vb);
    Vtx tri[6] = {tl, tr, bl, tr, br, bl};
    ++prims_;
    if (r.textured) {
        const int ua = u, ub = u + u_inc * (w - 1), va = v, vb2 = v + v_inc * (h - 1);
        const bool wide = w > 256 || h > 256 || std::min(ua, ub) < 0 || std::max(ua, ub) > 255 ||
                          std::min(va, vb2) < 0 || std::max(va, vb2) > 255;
        need_texture(r.tpage, r.clut, r.twin_mask, wide ? 0 : std::min(ua, ub), wide ? 0 : std::min(va, vb2),
                     wide ? 255 : std::max(ua, ub), wide ? 255 : std::max(va, vb2));
    }
    if (g.regs.mask_and) need(x, y, x + w, y + h);
    if (r.textured && r.semitrans) {
        for (Vtx& t : tri) t.info[2] = flags | F_OPAQUE_ONLY;
        emit(kOpaque, g, tri, 6);
        for (Vtx& t : tri) t.info[2] = flags | F_STP_ONLY;
        emit(pipe_for(true, r.tpage), g, tri, 6);
    } else {
        emit(pipe_for(r.semitrans != 0, r.tpage), g, tri, 6);
    }
    mark_drawn(std::max(x, g.regs.area_x0), std::max(y, g.regs.area_y0), std::min(x + w, g.regs.area_x1 + 1),
               std::min(y + h, g.regs.area_y1 + 1));
}

void HdRenderer::gpu_line(const machine::Gpu& g, const machine::Gpu::Line& l) {
    const float S = static_cast<float>(s_);
    u32 flags = l.semitrans ? F_SEMI : 0u;
    if (g.regs.mask_or) flags |= F_MASK_OR;
    if (g.regs.mask_and) flags |= F_CHECK;
    if (!l.gouraud) flags |= F_FLAT;
    flags |= static_cast<u32>((l.tpage >> 5) & 3u) << 8;
    const Pipe pipe = pipe_for(l.semitrans != 0, l.tpage);
    float centre = 0;
    const float squeeze = hud_squeeze(g, centre);
    auto across = [&](float x) { return squeeze == 1.0f ? x : centre + (x - centre) * squeeze; };
    for (int i = 0; i + 1 < l.nverts; ++i) {
        const float x0 = across(static_cast<float>(sx11(l.x[i]) + g.regs.ofs_x) + 0.5f);
        const float y0 = static_cast<float>(sx11(l.y[i]) + g.regs.ofs_y) + 0.5f;
        const float x1 = across(static_cast<float>(sx11(l.x[i + 1]) + g.regs.ofs_x) + 0.5f);
        const float y1 = static_cast<float>(sx11(l.y[i + 1]) + g.regs.ofs_y) + 0.5f;
        float dx = x1 - x0, dy = y1 - y0;
        const float len = std::sqrt(dx * dx + dy * dy);
        if (len < 1e-3f) {
            dx = 1;
            dy = 0;
        } else {
            dx /= len;
            dy /= len;
        }
        const float nx = -dy * 0.5f, ny = dx * 0.5f, ex = dx * 0.5f, ey = dy * 0.5f;
        const int j = l.gouraud ? i + 1 : i;
        auto vert = [&](float px, float py, int k) {
            Vtx o{};
            o.x = px * S;
            o.y = py * S;
            o.w = 1.0f;
            o.rgba = rgba(l.r[k], l.g[k], l.b[k]);
            o.info[2] = flags;
            return o;
        };
        const Vtx a = vert(x0 - ex + nx, y0 - ey + ny, i), b = vert(x0 - ex - nx, y0 - ey - ny, i),
                  c = vert(x1 + ex + nx, y1 + ey + ny, j), d = vert(x1 + ex - nx, y1 + ey - ny, j);
        Vtx tri[6] = {a, b, c, b, d, c};
        if (g.regs.mask_and)
            need(static_cast<s32>(std::min(x0, x1)) - 1, static_cast<s32>(std::min(y0, y1)) - 1,
                 static_cast<s32>(std::max(x0, x1)) + 2, static_cast<s32>(std::max(y0, y1)) + 2);
        emit(pipe, g, tri, 6);
        mark_drawn(static_cast<s32>(std::min(x0, x1)) - 1, static_cast<s32>(std::min(y0, y1)) - 1,
                   static_cast<s32>(std::max(x0, x1)) + 2, static_cast<s32>(std::max(y0, y1)) + 2);
    }
    ++prims_;
}

void HdRenderer::gpu_fill(const machine::Gpu&, s32 x, s32 y, s32 w, s32 h, u16 pixel) {
    hd_tex_.written(x, y, w, h);
    Cmd c;
    c.op = kFill;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    if (in_hud_ && hints_ && hints_->wide) {
        const s32* sc = hints_->screen;
        if (x >= sc[0] && x + w <= sc[0] + sc[2] && y >= sc[1] && y + h <= sc[1] + sc[3] && w < sc[2]) {
            const float centre = hints_->centre;
            const float k = hints_->squeeze;
            c.x = static_cast<s32>(std::floor(centre + (static_cast<float>(x) - centre) * k));
            c.w = static_cast<s32>(std::ceil(centre + (static_cast<float>(x + w) - centre) * k)) - c.x;
            mark_drawn(x, y, x + w, y + h);
            mark_drawn(c.x, y, c.x + c.w, y + h);
        }
    }
    c.color[0] = static_cast<float>(pixel & 31u) / 31.0f;
    c.color[1] = static_cast<float>((pixel >> 5) & 31u) / 31.0f;
    c.color[2] = static_cast<float>((pixel >> 10) & 31u) / 31.0f;
    c.color[3] = static_cast<float>(pixel >> 15);
    cmds_.push_back(c);
    mark_dirty(x, y, x + w, y + h);
    if (c.x == x && c.w == w) set_blocks(drawn_, x, y, x + w, y + h, true, 0);
}

void HdRenderer::gpu_copy(const machine::Gpu&, s32 sx, s32 sy, s32 w, s32 h, s32 dx, s32 dy) {
    hd_tex_.written(dx, dy, w, h);
    Cmd c;
    c.op = kCopy;
    c.sx = sx;
    c.sy = sy;
    c.x = dx;
    c.y = dy;
    c.w = w;
    c.h = h;
    cmds_.push_back(c);
    strip_ = {dx, dy, w, h, w > 0 && h > 0};
    mark_dirty(dx, dy, dx + w, dy + h);
    if (any_block(drawn_, sx, sy, sx + w, sy + h)) set_blocks(drawn_, dx, dy, dx + w, dy + h, false, 1);
    else set_blocks(drawn_, dx, dy, dx + w, dy + h, true, 0);
}

void HdRenderer::gpu_upload(const machine::Gpu& g, s32 x, s32 y, s32 w, s32 h) {
    hd_tex_.written(x, y, w, h);
    if (strip_.valid && x < strip_.x + strip_.w && strip_.x < x + w && y < strip_.y + strip_.h && strip_.y < y + h)
        strip_.valid = false;
    mark_dirty(x, y, x + w, y + h);
    set_blocks(drawn_, x, y, x + w, y + h, true, 0);
    sync(g, x, y, w, h);
    copy_quad(x, y, w, h);
}

void HdRenderer::copy_quad(s32 x, s32 y, s32 w, s32 h) {
    const float S = static_cast<float>(s_);
    auto vert = [&](s32 px, s32 py) {
        Vtx o{};
        o.x = static_cast<float>(px) * S;
        o.y = static_cast<float>(py) * S;
        o.w = 1.0f;
        o.u = static_cast<float>(px);
        o.v = static_cast<float>(py);
        o.info[2] = F_COPY;
        return o;
    };
    const Vtx tl = vert(x, y), tr = vert(x + w, y), bl = vert(x, y + h), br = vert(x + w, y + h);
    const Vtx tri[6] = {tl, tr, bl, tr, br, bl};
    const u32 first = static_cast<u32>(verts_.size());
    verts_.insert(verts_.end(), tri, tri + 6);
    Cmd c;
    c.op = kDraw;
    c.pipe = kOpaque;
    c.x = 0;
    c.y = 0;
    c.w = 1024;
    c.h = 512;
    c.first = first;
    c.count = 6;
    cmds_.push_back(c);
}

bool HdRenderer::grow(Buffer& b, VkDeviceSize need, VkBufferUsageFlags usage) {
    if (b.buffer && b.size >= need) return true;
    render::destroy(*gpu_, b);
    VkDeviceSize size = 1u << 20;
    while (size < need) size *= 2;
    return create_buffer(*gpu_, size, usage, true, b);
}

void HdRenderer::begin_pass(VkCommandBuffer cb) {
    VkRenderingAttachmentInfo ca{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    ca.imageView = target_.view;
    ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ca.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, {w_, h_}};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &ca;
    VkRenderingAttachmentInfo da{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    if (depth_.image) {
        da.imageView = depth_.view;
        da.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
        da.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        da.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        ri.pDepthAttachment = &da;
    }
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp{0, 0, static_cast<float>(w_), static_cast<float>(h_), 0, 1};
    vkCmdSetViewport(cb, 0, 1, &vp);
}

void HdRenderer::record(VkCommandBuffer cb, u32 slot) {
    if (!gpu_) return;
    ++frame_;
    if (ocean_due_ && precise_ && precise_->shore.generation != shore_seen_ &&
        precise_->shore.cells.size() == static_cast<size_t>(Ocean::kShore) * Ocean::kShore) {
        ocean_.set_shore(precise_->shore.cells.data());
        shore_seen_ = precise_->shore.generation;
    }
    if (ocean_due_) ocean_.simulate(cb, ocean_time_);
    ocean_due_ = false;
    if (!cmds_.empty()) {
        replay(cb, frames_[slot % kFramesInFlight]);
        last_prims_ = prims_;
        last_precise_ = precise_count_;
        prims_ = precise_count_ = 0;
    }
    make_picture(cb);
}

bool HdRenderer::replay(VkCommandBuffer cb, Frame& f) {
    const VkDeviceSize vbytes = verts_.size() * sizeof(Vtx), sbytes = staging_.size() * sizeof(u16);
    if (!grow(f.vertices, std::max<VkDeviceSize>(vbytes, 64), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) ||
        !grow(f.staging, std::max<VkDeviceSize>(sbytes, 64), VK_BUFFER_USAGE_TRANSFER_SRC_BIT)) {
        log::error("enhanced renderer: out of buffer memory");
        cmds_.clear();
        verts_.clear();
        lights_.clear();
        light_from_.clear();
        waters_.clear();
        water_means_.clear();
        staging_.clear();
        return false;
    }
    if (vbytes) std::memcpy(f.vertices.mapped, verts_.data(), vbytes);
    if (sbytes) std::memcpy(f.staging.mapped, staging_.data(), sbytes);

    const u32 S = s_;
    auto scaled = [&](s32 x, s32 y, s32 w, s32 h, VkRect2D& out) {
        const s32 x0 = std::clamp(x, 0, 1024), y0 = std::clamp(y, 0, 512);
        const s32 x1 = std::clamp(x + w, 0, 1024), y1 = std::clamp(y + h, 0, 512);
        if (x0 >= x1 || y0 >= y1) return false;
        out.offset = {static_cast<s32>(static_cast<u32>(x0) * S), static_cast<s32>(static_cast<u32>(y0) * S)};
        out.extent = {static_cast<u32>(x1 - x0) * S, static_cast<u32>(y1 - y0) * S};
        return true;
    };
    hd_tex_.record_uploads(cb);
    image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
    PushPrim push{};
    push.to_ndc[0] = 2.0f / static_cast<float>(w_);
    push.to_ndc[1] = 2.0f / static_cast<float>(h_);
    push.inv_scale = 1.0f / static_cast<float>(s_);
    push.spec = 1.0f;
    s32 pushed = -1;
    auto push_light = [&](s32 light) {
        if (light == pushed || light == -1) return;
        if (light <= -2) {
            const size_t i = static_cast<size_t>(-2 - light);
            if (i >= waters_.size()) return;
            const sim::PreciseCache::Water& w = waters_[i];
            auto pack = [](const float* c, float full) {
                float v = 0;
                for (int k = 2; k >= 0; --k) v = v * 256.0f + std::round(std::clamp(c[k] / full, 0.0f, 1.0f) * 255.0f);
                return v;
            };
            for (int r = 0; r < 3; ++r) {
                push.dir[0][r] = w.rot[r][0];
                push.dir[1][r] = -w.rot[r][1];
                push.dir[2][r] = w.rot[r][2];
                push.colour[0][r] = w.rot[r][0] * w.sun[0] + w.rot[r][1] * w.sun[1] + w.rot[r][2] * w.sun[2];
            }
            push.dir[0][3] = static_cast<float>(w.eye[0]);
            push.dir[1][3] = static_cast<float>(w.eye[2]);
            push.dir[2][3] = std::fmod(w.time, 3600.0f);
            push.colour[0][3] = pack(w.sun_colour, 2.0f);
            float sky[4][3], tan[3];
            if (w.sky_age <= 2 && w.sky_tan[0] > w.sky_tan[1] && w.sky_tan[1] > w.sky_tan[2] && w.sky_tan[2] > 0.0f) {
                std::memcpy(sky, w.sky, sizeof sky);
                std::memcpy(tan, w.sky_tan, sizeof tan);
            } else {
                static constexpr float kTint[3][3] = {{0.55f, 0.68f, 0.9f}, {0.72f, 0.82f, 0.96f}, {0.9f, 0.95f, 1.0f}};
                static constexpr float kTan[3] = {0.9f, 0.35f, 0.1f};
                for (int b = 0; b < 3; ++b) {
                    for (int c = 0; c < 3; ++c) sky[b][c] = w.horizon[c] * kTint[b][c];
                    tan[b] = kTan[b];
                }
                for (int c = 0; c < 3; ++c) sky[3][c] = w.horizon[c];
            }
            for (int b = 0; b < 4; ++b) push.colour[1][b] = pack(sky[b], 1.0f);
            float uv[2 * Ocean::kCascades];
            Ocean::eye_uv(w.eye[0], w.eye[2], uv);
            for (int k = 0; k < 4; ++k) push.colour[2][k] = uv[k];
            push.back[0] = uv[4];
            push.back[1] = uv[5];
            push.back[2] = tan[0];
            push.back[3] = tan[1];
            push.spec = tan[2];
            vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
            pushed = light;
            return;
        }
        if (light < 0 || static_cast<size_t>(light) >= lights_.size()) return;
        const sim::PreciseCache::Light& l = lights_[static_cast<size_t>(light)];
        for (int k = 0; k < 3; ++k)
            for (int c = 0; c < 3; ++c) {
                push.dir[k][c] = l.dir[k][c];
                push.colour[k][c] = l.colour[k][c];
            }
        for (int c = 0; c < 3; ++c) {
            push.back[c] = l.back[c];
            push.dir[c][3] = l.up[c];
            push.colour[c][3] = l.horizon[c];
        }
        push.back[3] = 1.0f;
        push.spec = 1.0f;
        vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        pushed = light;
    };
    u8 depth = kNoDepth;
    auto bind = [&] {
        const VkDeviceSize off = 0;
        vkCmdBindVertexBuffers(cb, 0, 1, &f.vertices.buffer, &off);
        const VkDescriptorSet sets[4] = {mirror_set_, hd_tex_.set(), strip_set_, ocean_.set()};
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 4, sets, 0, nullptr);
        vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push, &push);
        vkCmdSetDepthTestEnable(cb, VK_FALSE);
        vkCmdSetDepthWriteEnable(cb, VK_FALSE);
        depth = kNoDepth;
    };
    begin_pass(cb);
    bind();
    int bound = -1;
    for (const Cmd& c : cmds_) {
        switch (c.op) {
        case kDraw: {
            VkRect2D sc;
            if (!scaled(c.x, c.y, c.w, c.h, sc)) break;
            if (bound != c.pipe) {
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipes_[c.pipe]);
                bound = c.pipe;
            }
            vkCmdSetScissor(cb, 0, 1, &sc);
            push_light(c.light);
            if (depth != c.depth) {
                depth = c.depth;
                vkCmdSetDepthTestEnable(cb, depth != kNoDepth ? VK_TRUE : VK_FALSE);
                vkCmdSetDepthWriteEnable(cb, depth == kDepthWrite ? VK_TRUE : VK_FALSE);
            }
            vkCmdDraw(cb, c.count, 1, c.first, 0);
            break;
        }
        case kClearDepth: {
            VkClearRect cr{};
            if (!depth_.image || !scaled(c.x, c.y, c.w, c.h, cr.rect)) break;
            cr.layerCount = 1;
            VkClearAttachment ca{};
            ca.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            ca.clearValue.depthStencil = {0.0f, 0};
            vkCmdClearAttachments(cb, 1, &ca, 1, &cr);
            break;
        }
        case kFill: {
            VkClearRect cr{};
            if (!scaled(c.x, c.y, c.w, c.h, cr.rect)) break;
            cr.layerCount = 1;
            VkClearAttachment ca{};
            ca.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ca.colorAttachment = 0;
            std::memcpy(ca.clearValue.color.float32, c.color, sizeof c.color);
            vkCmdClearAttachments(cb, 1, &ca, 1, &cr);
            break;
        }
        case kSync: {
            vkCmdEndRendering(cb);
            image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT);
            VkBufferImageCopy bc{};
            bc.bufferOffset = static_cast<VkDeviceSize>(c.first) * sizeof(u16);
            bc.bufferRowLength = static_cast<u32>(c.w);
            bc.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            bc.imageOffset = {c.x, c.y, 0};
            bc.imageExtent = {static_cast<u32>(c.w), static_cast<u32>(c.h), 1};
            vkCmdCopyBufferToImage(cb, f.staging.buffer, mirror_.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bc);
            image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT);
            begin_pass(cb);
            bind();
            bound = -1;
            break;
        }
        case kDown: {
            ++perf.downs;
            VkRect2D sc{{c.x, c.y}, {static_cast<u32>(c.w), static_cast<u32>(c.h)}};
            vkCmdEndRendering(cb);
            image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT);
            image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
            VkRenderingAttachmentInfo ma{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
            ma.imageView = mirror_.view;
            ma.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ma.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
            ma.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
            ri.renderArea = sc;
            ri.layerCount = 1;
            ri.colorAttachmentCount = 1;
            ri.pColorAttachments = &ma;
            vkCmdBeginRendering(cb, &ri);
            const VkViewport vp{0, 0, 1024, 512, 0, 1};
            vkCmdSetViewport(cb, 0, 1, &vp);
            vkCmdSetScissor(cb, 0, 1, &sc);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, down_);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, down_layout_, 0, 1, &target_set_, 0, nullptr);
            const PushDown pd{static_cast<s32>(s_)};
            vkCmdPushConstants(cb, down_layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof pd, &pd);
            vkCmdDraw(cb, 3, 1, 0, 0);
            vkCmdEndRendering(cb);
            image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT);
            image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
            begin_pass(cb);
            bind();
            bound = -1;
            break;
        }
        case kCopy: {
            VkRect2D src, dst;
            if (!scaled(c.sx, c.sy, c.w, c.h, src) || !scaled(c.x, c.y, c.w, c.h, dst)) break;
            const VkExtent2D e{std::min(src.extent.width, dst.extent.width), std::min(src.extent.height, dst.extent.height)};
            vkCmdEndRendering(cb);
            image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT);
            image_barrier(cb, temp_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT | VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT);
            VkImageCopy ic{};
            ic.srcSubresource = ic.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            ic.srcOffset = {src.offset.x, src.offset.y, 0};
            ic.dstOffset = {0, 0, 0};
            ic.extent = {e.width, e.height, 1};
            vkCmdCopyImage(cb, target_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, temp_.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ic);
            image_barrier(cb, temp_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT);
            image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT);
            ic.srcOffset = {0, 0, 0};
            ic.dstOffset = {dst.offset.x, dst.offset.y, 0};
            vkCmdCopyImage(cb, temp_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, target_.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &ic);
            image_barrier(cb, temp_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_2_SHADER_READ_BIT);
            image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                          VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT);
            begin_pass(cb);
            bind();
            bound = -1;
            break;
        }
        }
    }
    vkCmdEndRendering(cb);
    image_barrier(cb, target_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                  VK_ACCESS_2_SHADER_READ_BIT);
    cmds_.clear();
    verts_.clear();
    lights_.clear();
    light_from_.clear();
    waters_.clear();
    water_means_.clear();
    strip_.valid = false;
    staging_.clear();
    return true;
}

bool HdRenderer::read_native(s32 x, s32 y, s32 w, s32 h, u16* out) {
    if (!gpu_ || w <= 0 || h <= 0) return false;
    need(x, y, x + w, y + h);
    const VkDeviceSize bytes = static_cast<VkDeviceSize>(w) * static_cast<VkDeviceSize>(h) * sizeof(u16);
    if (!grow(readback_, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT)) return false;
    const auto t0 = std::chrono::steady_clock::now();
    ++perf.reads;
    vkDeviceWaitIdle(gpu_->device);
    bool ok = true;
    one_shot(*gpu_, [&](VkCommandBuffer cb) {
        if (!cmds_.empty()) ok = replay(cb, flush_);
        image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
        VkBufferImageCopy bc{};
        bc.bufferRowLength = static_cast<u32>(w);
        bc.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        bc.imageOffset = {x, y, 0};
        bc.imageExtent = {static_cast<u32>(w), static_cast<u32>(h), 1};
        vkCmdCopyImageToBuffer(cb, mirror_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback_.buffer, 1, &bc);
        image_barrier(cb, mirror_.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                      VK_ACCESS_2_SHADER_READ_BIT);
    });
    perf.read_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (!ok) return false;
    std::memcpy(out, readback_.mapped, bytes);
    return true;
}

void HdRenderer::gpu_read(machine::Gpu& g, s32 x, s32 y, s32 w, s32 h) {
    if (!any_block(drawn_, x, y, x + w, y + h)) return;
    std::vector<u16> px(static_cast<size_t>(w) * static_cast<size_t>(h));
    if (!read_native(x, y, w, h, px.data())) return;
    for (s32 j = 0; j < h; ++j)
        std::memcpy(&g.at(x, y + j), px.data() + static_cast<size_t>(j) * static_cast<size_t>(w),
                    static_cast<size_t>(w) * sizeof(u16));
    set_blocks(drawn_, x, y, x + w, y + h, true, 0);
}

void HdRenderer::check(const machine::Gpu& g, u32 frame) {
    std::vector<u16> px(1024 * 512);
    if (!read_native(0, 0, 1024, 512, px.data())) return;
    u32 exact = 0, near = 0, far = 0, mask = 0;
    struct Tile {
        u32 far = 0, n = 0;
    };
    std::vector<Tile> tiles(16 * 8);
    for (u32 y = 0; y < 512; ++y)
        for (u32 x = 0; x < 1024; ++x) {
            const u16 a = px[y * 1024 + x], b = g.vram[y * 1024 + x];
            if (a == b) {
                ++exact;
                continue;
            }
            if ((a >> 15) != (b >> 15)) ++mask;
            u32 d = 0;
            for (u32 k = 0; k < 15; k += 5)
                d = std::max(d, static_cast<u32>(std::abs(static_cast<s32>((a >> k) & 31u) - static_cast<s32>((b >> k) & 31u))));
            if (d == 0) {
                continue;
            } else if (d <= 1) {
                ++near;
            } else {
                ++far;
                ++tiles[(y / 64) * 16 + x / 64].far;
            }
        }
    std::string worst;
    for (int k = 0; k < 4; ++k) {
        size_t best = 0;
        for (size_t t = 1; t < tiles.size(); ++t)
            if (tiles[t].far > tiles[best].far) best = t;
        if (!tiles[best].far) break;
        worst += " (" + std::to_string(best % 16 * 64) + "," + std::to_string(best / 16 * 64) + "):" +
                 std::to_string(tiles[best].far);
        tiles[best].far = 0;
    }
    log::info("vram-check frame {}: {} exact; colour within a step {}, further {}; mask bit {}{}{}; display {}x{} at "
              "({},{})", frame, exact, near, far, mask, worst.empty() ? "" : "; worst 64x64 tiles", worst, g.display.w,
              g.display.h, g.display.x, g.display.y);
    if (check_dump.empty()) return;
    std::vector<u8> rgba(1024 * 512 * 4);
    auto write = [&](const u16* v, const char* which) {
        for (u32 i = 0; i < 1024 * 512; ++i) {
            const u32 r = v[i] & 31u, gr = (v[i] >> 5) & 31u, b = (v[i] >> 10) & 31u;
            rgba[i * 4 + 0] = static_cast<u8>((r << 3) | (r >> 2));
            rgba[i * 4 + 1] = static_cast<u8>((gr << 3) | (gr >> 2));
            rgba[i * 4 + 2] = static_cast<u8>((b << 3) | (b >> 2));
            rgba[i * 4 + 3] = 255;
        }
        png::write_rgba(check_dump + "/vram_" + std::to_string(frame) + "_" + which + ".png", rgba.data(), 1024, 512);
    };
    write(px.data(), "hd");
    write(g.vram.data(), "gpu");
    for (u32 i = 0; i < 1024 * 512; ++i) {
        const u16 a = px[i], b = g.vram[i];
        u32 d = 0;
        for (u32 k = 0; k < 15; k += 5)
            d = std::max(d, static_cast<u32>(std::abs(static_cast<s32>((a >> k) & 31u) - static_cast<s32>((b >> k) & 31u))));
        const u8 v = d > 1 ? 255 : d ? 96 : 0;
        rgba[i * 4 + 0] = v;
        rgba[i * 4 + 1] = v;
        rgba[i * 4 + 2] = d == 0 && a != b ? 255 : v;
        rgba[i * 4 + 3] = 255;
    }
    png::write_rgba(check_dump + "/vram_" + std::to_string(frame) + "_diff.png", rgba.data(), 1024, 512);
}

void HdRenderer::displayed(const machine::Gpu::Display& d) {
    shown_ = d;
    if (gpu_ && d.rgb24) need(d.x, d.y, d.x + (d.w * 3 + 1) / 2 + 1, d.y + d.h);
}

void HdRenderer::present(VkCommandBuffer cb, const Target& target, const machine::Gpu::Display& d, float aspect) {
    if (!present_) return;
    const float tw = static_cast<float>(target.extent.width), th = static_cast<float>(target.extent.height);
    float w = th * aspect, h = th;
    if (w > tw) {
        w = tw;
        h = tw / aspect;
    }
    const VkViewport vp{(tw - w) / 2, (th - h) / 2, w, h, 0, 1};
    const VkRect2D sc{{static_cast<s32>(vp.x), static_cast<s32>(vp.y)},
                      {static_cast<u32>(w), static_cast<u32>(h)}};
    if (d.rgb24) {
        const PushPresent24 p{{0, 0, static_cast<float>(d.w), static_cast<float>(d.h)}, {d.x, d.y, 0, 0}};
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present24_);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present24_layout_, 0, 1, &mirror_set_, 0,
                                nullptr);
        vkCmdPushConstants(cb, present24_layout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof p, &p);
    } else if (picture_) {
        const PushPresent p{{0, 0, 1, 1}};
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present_);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present_layout_, 0, 1, &picture_set_, 0,
                                nullptr);
        vkCmdPushConstants(cb, present_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof p, &p);
    } else {
        const PushPresent p{{static_cast<float>(d.x) / 1024.0f, static_cast<float>(d.y) / 512.0f,
                             static_cast<float>(d.w) / 1024.0f, static_cast<float>(d.h) / 512.0f}};
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present_);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, present_layout_, 0, 1, &target_set_, 0,
                                nullptr);
        vkCmdPushConstants(cb, present_layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof p, &p);
    }
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
    vkCmdDraw(cb, 3, 1, 0, 0);
}
}
