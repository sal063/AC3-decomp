#include "render/hd_textures.hpp"

#include "core/file.hpp"
#include "core/hash.hpp"
#include "core/log.hpp"
#include "core/png.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace ac3::render {
namespace {
Bytes to_layer(const Bytes& rgba, u32 w, u32 h) {
    constexpr u32 N = HdTextures::kSize;
    if (w == N && h == N) return rgba;
    Bytes out(static_cast<size_t>(N) * N * 4);
    for (u32 y = 0; y < N; ++y) {
        const float fy = std::clamp((static_cast<float>(y) + 0.5f) * static_cast<float>(h) / N - 0.5f, 0.0f,
                                    static_cast<float>(h - 1));
        const u32 y0 = static_cast<u32>(fy), y1 = std::min(y0 + 1, h - 1);
        const float ty = fy - static_cast<float>(y0);
        for (u32 x = 0; x < N; ++x) {
            const float fx = std::clamp((static_cast<float>(x) + 0.5f) * static_cast<float>(w) / N - 0.5f, 0.0f,
                                        static_cast<float>(w - 1));
            const u32 x0 = static_cast<u32>(fx), x1 = std::min(x0 + 1, w - 1);
            const float tx = fx - static_cast<float>(x0);
            for (u32 c = 0; c < 4; ++c) {
                auto at = [&](u32 xx, u32 yy) { return static_cast<float>(rgba[(static_cast<size_t>(yy) * w + xx) * 4 + c]); };
                const float v = (at(x0, y0) * (1 - tx) + at(x1, y0) * tx) * (1 - ty) + (at(x0, y1) * (1 - tx) + at(x1, y1) * tx) * ty;
                out[(static_cast<size_t>(y) * N + x) * 4 + c] = static_cast<u8>(std::clamp(v + 0.5f, 0.0f, 255.0f));
            }
        }
    }
    return out;
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else if (c != '\r') {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

u32 hex(const std::string& s, size_t at, size_t n) {
    return static_cast<u32>(std::stoul(s.substr(at, n), nullptr, 16));
}

void barrier(VkCommandBuffer cb, VkImage image, u32 level, u32 levels, u32 layer, VkImageLayout from, VkImageLayout to,
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
    b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, levels, layer, 1};
    VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    d.imageMemoryBarrierCount = 1;
    d.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &d);
}
}

bool HdTextures::load(const std::string& dir) {
    namespace fs = std::filesystem;
    planes_.clear();
    images_.clear();
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return false;
    for (const auto& sub : fs::directory_iterator(dir, ec)) {
        const fs::path manifest = sub.path() / "manifest.txt";
        const auto bytes = file::read_all(manifest.string());
        if (!bytes) continue;
        std::vector<std::pair<std::string, int>> named;
        auto image = [&](const std::string& file) {
            if (file == "-") return -1;
            for (const auto& [n, i] : named)
                if (n == file) return i;
            Image im;
            im.path = (sub.path() / file).string();
            images_.push_back(std::move(im));
            named.emplace_back(file, static_cast<int>(images_.size() - 1));
            return static_cast<int>(images_.size() - 1);
        };
        std::istringstream in(std::string(bytes->begin(), bytes->end()));
        std::string line;
        u32 count = 0;
        while (std::getline(in, line)) {
            const auto t = split(line, ' ');
            if (t.size() < 3 || t[0] != "plane") continue;
            Plane p;
            p.record = std::atoi(t[1].c_str());
            p.name = t[2];
            bool ok = true;
            try {
                for (size_t k = 3; k < t.size(); ++k) {
                    const size_t eq = t[k].find('=');
                    if (eq == std::string::npos) continue;
                    const std::string key = t[k].substr(0, eq), value = t[k].substr(eq + 1);
                    if (key == "base") p.base = image(value);
                    else if (key == "variant") p.variant = image(value);
                    else if (key == "normal" && value != "-") p.normal = (sub.path() / value).string();
                    else if (key == "rough" && value != "-") p.rough = (sub.path() / value).string();
                    else if (key == "rows")
                        for (const auto& r : split(value, ',')) p.rows.push_back(std::stoull(r, nullptr, 16));
                    else if (key == "samples" || key == "probe") {
                        auto& list = key == "samples" ? p.samples : p.probe;
                        if (value == "-") continue;
                        for (size_t i = 0; i + 5 <= value.size(); i += 5)
                            list.push_back({static_cast<u8>(hex(value, i, 2)), static_cast<u8>(hex(value, i + 2, 2)),
                                            static_cast<u8>(hex(value, i + 4, 1))});
                    }
                }
            } catch (...) {
                ok = false;
            }
            if (!ok || p.base < 0 || p.samples.size() < 64 || p.rows.empty()) {
                log::warn("hd textures: {}: a plane line not understood", manifest.string());
                continue;
            }
            for (const int im : {p.base, p.variant})
                if (im >= 0) {
                    images_[static_cast<size_t>(im)].normal = p.normal;
                    images_[static_cast<size_t>(im)].rough = p.rough;
                }
            planes_.push_back(std::move(p));
            ++count;
        }
        if (count) log::info("hd textures: {} planes from {}", count, manifest.string());
    }
    return !planes_.empty();
}

bool HdTextures::init(const Gpu& gpu) {
    gpu_ = &gpu;
    const bool any = !images_.empty();
    size_ = any ? kSize : 1;
    layers_ = any ? 2 * kLayers : 1;
    levels_ = 1;
    for (u32 s = size_; s > 1; s >>= 1) ++levels_;

    VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ci.imageType = VK_IMAGE_TYPE_2D;
    ci.format = VK_FORMAT_R8G8B8A8_UNORM;
    ci.extent = {size_, size_, 1};
    ci.mipLevels = levels_;
    ci.arrayLayers = layers_;
    ci.samples = VK_SAMPLE_COUNT_1_BIT;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(gpu.device, &ci, nullptr, &array_), "vkCreateImage (hd textures)")) return false;
    VkMemoryRequirements mr;
    vkGetImageMemoryRequirements(gpu.device, array_, &mr);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = find_memory(gpu, mr.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!vk_ok(vkAllocateMemory(gpu.device, &ai, nullptr, &memory_), "vkAllocateMemory (hd textures)")) return false;
    vkBindImageMemory(gpu.device, array_, memory_, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = array_;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vi.format = ci.format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, levels_, 0, layers_};
    if (!vk_ok(vkCreateImageView(gpu.device, &vi, nullptr, &view_), "vkCreateImageView (hd textures)")) return false;
    one_shot(gpu, [&](VkCommandBuffer cb) {
        const VkImageSubresourceRange all{VK_IMAGE_ASPECT_COLOR_BIT, 0, levels_, 0, layers_};
        VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        b.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = array_;
        b.subresourceRange = all;
        VkDependencyInfo d{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
        d.imageMemoryBarrierCount = 1;
        d.pImageMemoryBarriers = &b;
        vkCmdPipelineBarrier2(cb, &d);
        VkClearColorValue c{};
        vkCmdClearColorImage(cb, array_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &c, 1, &all);
        b.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        b.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        b.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        b.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier2(cb, &d);
    });

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = VK_LOD_CLAMP_NONE;
    si.anisotropyEnable = gpu.anisotropy ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy = gpu.anisotropy ? 16.0f : 1.0f;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &sampler_), "vkCreateSampler (hd textures)")) return false;

    VkDescriptorSetLayoutBinding lb{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 1;
    dl.pBindings = &lb;
    if (!vk_ok(vkCreateDescriptorSetLayout(gpu.device, &dl, nullptr, &set_layout_), "vkCreateDescriptorSetLayout"))
        return false;
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (!vk_ok(vkCreateDescriptorPool(gpu.device, &pi, nullptr, &pool_), "vkCreateDescriptorPool")) return false;
    VkDescriptorSetAllocateInfo sa{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    sa.descriptorPool = pool_;
    sa.descriptorSetCount = 1;
    sa.pSetLayouts = &set_layout_;
    if (!vk_ok(vkAllocateDescriptorSets(gpu.device, &sa, &set_), "vkAllocateDescriptorSets")) return false;
    const VkDescriptorImageInfo ii{sampler_, view_, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set_;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(gpu.device, 1, &w, 0, nullptr);
    if (any) log::info("hd textures: {} images, {} layers of {}x{}", images_.size(), layers_, size_, size_);
    return true;
}

void HdTextures::destroy() {
    for (Image& im : images_)
        if (im.decoding.valid()) im.decoding.wait();
    if (!gpu_) return;
    const VkDevice d = gpu_->device;
    for (Layer& l : layer_) render::destroy(*gpu_, l.staging);
    if (pool_) vkDestroyDescriptorPool(d, pool_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(d, set_layout_, nullptr);
    if (sampler_) vkDestroySampler(d, sampler_, nullptr);
    if (view_) vkDestroyImageView(d, view_, nullptr);
    if (array_) vkDestroyImage(d, array_, nullptr);
    if (memory_) vkFreeMemory(d, memory_, nullptr);
    pool_ = VK_NULL_HANDLE;
    set_layout_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    view_ = VK_NULL_HANDLE;
    array_ = VK_NULL_HANDLE;
    memory_ = VK_NULL_HANDLE;
    set_ = VK_NULL_HANDLE;
    gpu_ = nullptr;
}

void HdTextures::written(s32 x, s32 y, s32 w, s32 h) {
    if (planes_.empty() || w <= 0 || h <= 0) return;
    const s32 x0 = std::clamp(x, 0, 1023) / 64, x1 = std::clamp(x + w - 1, 0, 1023) / 64;
    const s32 y0 = std::clamp(y, 0, 511) / 256, y1 = std::clamp(y + h - 1, 0, 511) / 256;
    for (s32 py = y0; py <= y1; ++py)
        for (s32 px = x0; px <= x1; ++px) ++gen_[py * 16 + px];
}

int HdTextures::identify(const machine::Gpu& g, u32 page_x, u32 page_y, bool& variant) const {
    auto index = [&](const Texel& t) {
        const u16 w = g.vram[(page_y + t.y) * 1024 + page_x + (t.x >> 2)];
        return static_cast<u8>((w >> ((t.x & 3) * 4)) & 15);
    };
    for (size_t i = 0; i < planes_.size(); ++i) {
        const Plane& p = planes_[i];
        const size_t allowed = p.samples.size() / 32;
        size_t wrong = 0;
        for (const Texel& t : p.samples)
            if (index(t) != t.index && ++wrong > allowed) break;
        if (wrong > allowed) continue;
        variant = false;
        if (p.variant >= 0 && !p.probe.empty()) {
            size_t same = 0;
            for (const Texel& t : p.probe) same += index(t) == t.index;
            variant = same * 4 < p.probe.size() * 3;
        }
        return static_cast<int>(i);
    }
    return -1;
}

u32 HdTextures::layer_for(const machine::Gpu& g, u16 tpage, u16 clut, u64 frame) {
    if (planes_.empty() || ((tpage >> 7) & 3u) != 0) return 0;
    const u32 page = tpage & 31u;
    PageState& s = pages_[page];
    if (s.seen != gen_[page]) {
        s.seen = gen_[page];
        s.plane = identify(g, (page & 15u) * 64, (page >> 4) * 256, s.variant);
        if (s.plane >= 0)
            log::info("hd textures: page {:#x} is {} ({}{})", page, planes_[s.plane].name, planes_[s.plane].record,
                      s.variant ? ", the other emblem" : "");
    }
    if (s.plane < 0) return 0;
    const Plane& p = planes_[s.plane];
    u8 row[32];
    const u32 cx = (clut & 63u) * 16, cy = (clut >> 6) & 511u;
    for (u32 i = 0; i < 16; ++i) {
        const u16 c = g.vram[cy * 1024 + ((cx + i) & 1023u)];
        row[2 * i] = static_cast<u8>(c);
        row[2 * i + 1] = static_cast<u8>(c >> 8);
    }
    const u64 h = fnv1a64(row, sizeof row);
    if (std::find(p.rows.begin(), p.rows.end(), h) == p.rows.end()) return 0;
    const int image = s.variant && p.variant >= 0 ? p.variant : p.base;
    const int layer = layer_of(image, frame);
    return layer < 0 ? 0 : static_cast<u32>(layer) + 1;
}

int HdTextures::layer_of(int image, u64 frame) {
    Image& im = images_[static_cast<size_t>(image)];
    if (im.layer >= 0) {
        layer_[im.layer].used = frame;
        return im.layer;
    }
    if (im.failed || !gpu_ || layers_ < 2 * kLayers) return -1;
    if (!im.decoding.valid()) {
        im.decoding = std::async(std::launch::async, [path = im.path, normal = im.normal, rough = im.rough] {
            Bytes rgba;
            u32 w = 0, h = 0;
            std::string error;
            if (!png::read_rgba(path, rgba, w, h, &error)) return Bytes{};
            Bytes out = to_layer(rgba, w, h);
            constexpr size_t N = static_cast<size_t>(kSize) * kSize;
            Bytes material(N * 4);
            for (size_t i = 0; i < N; ++i) {
                material[4 * i] = 128;
                material[4 * i + 1] = 128;
                material[4 * i + 2] = 255;
                material[4 * i + 3] = 153;
            }
            if (!normal.empty()) {
                if (png::read_rgba(normal, rgba, w, h, &error)) {
                    const Bytes n = to_layer(rgba, w, h);
                    for (size_t i = 0; i < N; ++i)
                        for (size_t c = 0; c < 3; ++c) material[4 * i + c] = n[4 * i + c];
                } else {
                    log::warn("hd textures: {}: {}", normal, error);
                }
            }
            if (!rough.empty()) {
                if (png::read_rgba(rough, rgba, w, h, &error)) {
                    const Bytes r = to_layer(rgba, w, h);
                    for (size_t i = 0; i < N; ++i) material[4 * i + 3] = r[4 * i];
                } else {
                    log::warn("hd textures: {}: {}", rough, error);
                }
            }
            out.insert(out.end(), material.begin(), material.end());
            return out;
        });
        return -1;
    }
    if (im.decoding.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return -1;
    int pick = -1;
    for (u32 i = 0; i < kLayers; ++i) {
        const Layer& l = layer_[i];
        if (l.image < 0) {
            pick = static_cast<int>(i);
            break;
        }
        if (l.used + 3 < frame && (pick < 0 || l.used < layer_[pick].used)) pick = static_cast<int>(i);
    }
    if (pick < 0) return -1;
    Bytes rgba = im.decoding.get();
    if (rgba.size() != static_cast<size_t>(kSize) * kSize * 8) {
        log::warn("hd textures: {}: not a PNG read here", im.path);
        im.failed = true;
        return -1;
    }
    Layer& l = layer_[pick];
    if (!l.staging.buffer && !create_buffer(*gpu_, rgba.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, l.staging)) {
        im.failed = true;
        return -1;
    }
    std::memcpy(l.staging.mapped, rgba.data(), rgba.size());
    if (l.image >= 0) images_[static_cast<size_t>(l.image)].layer = -1;
    l.image = image;
    l.used = frame;
    l.pending = true;
    im.layer = pick;
    log::info("hd textures: {} in layer {}", im.path, pick);
    return pick;
}

void HdTextures::record_uploads(VkCommandBuffer cb) {
    for (u32 i = 0; i < kLayers; ++i) {
        Layer& l = layer_[i];
        if (!l.pending) continue;
        l.pending = false;
        for (u32 half = 0; half < 2; ++half) {
            const u32 layer = i + half * kLayers;
            barrier(cb, array_, 0, levels_, layer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
            VkBufferImageCopy copy{};
            copy.bufferOffset = static_cast<VkDeviceSize>(half) * kSize * kSize * 4;
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
            copy.imageExtent = {size_, size_, 1};
            vkCmdCopyBufferToImage(cb, l.staging.buffer, array_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
            for (u32 m = 1; m < levels_; ++m) {
                barrier(cb, array_, m - 1, 1, layer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_READ_BIT);
                const s32 from = static_cast<s32>(std::max(1u, size_ >> (m - 1))),
                          to = static_cast<s32>(std::max(1u, size_ >> m));
                VkImageBlit blit{};
                blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m - 1, layer, 1};
                blit.srcOffsets[1] = {from, from, 1};
                blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, layer, 1};
                blit.dstOffsets[1] = {to, to, 1};
                vkCmdBlitImage(cb, array_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, array_,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
            }
            if (levels_ > 1)
                barrier(cb, array_, 0, levels_ - 1, layer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        VK_ACCESS_2_TRANSFER_READ_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_2_SHADER_READ_BIT);
            barrier(cb, array_, levels_ - 1, 1, layer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                    VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                    VK_ACCESS_2_SHADER_READ_BIT);
        }
    }
}
}
