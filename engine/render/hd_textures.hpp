#pragma once

#include "core/bytes.hpp"
#include "machine/gpu.hpp"
#include "render/vk.hpp"

#include <future>
#include <string>
#include <vector>

namespace ac3::render {
class HdTextures {
public:
    static constexpr u32 kSize = 1024;
    static constexpr u32 kLayers = 8;

    bool load(const std::string& dir);
    bool empty() const { return planes_.empty(); }

    bool init(const Gpu& gpu);
    void destroy();
    VkDescriptorSetLayout set_layout() const { return set_layout_; }
    VkDescriptorSet set() const { return set_; }

    void written(s32 x, s32 y, s32 w, s32 h);

    u32 layer_for(const machine::Gpu& g, u16 tpage, u16 clut, u64 frame);

    void record_uploads(VkCommandBuffer cb);

private:
    struct Texel {
        u8 x, y, index;
    };
    struct Plane {
        int record = 0;
        std::string name;
        int base = -1, variant = -1;
        std::string normal, rough;
        std::vector<u64> rows;
        std::vector<Texel> samples, probe;
    };
    struct Image {
        std::string path, normal, rough;
        std::future<Bytes> decoding;
        bool failed = false;
        int layer = -1;
    };
    struct Layer {
        int image = -1;
        u64 used = 0;
        bool pending = false;
        Buffer staging;
    };
    struct PageState {
        u32 seen = ~0u;
        int plane = -1;
        bool variant = false;
    };

    int identify(const machine::Gpu& g, u32 page_x, u32 page_y, bool& variant) const;
    int layer_of(int image, u64 frame);

    std::vector<Plane> planes_;
    std::vector<Image> images_;
    u32 gen_[32] = {};
    PageState pages_[32];

    const Gpu* gpu_ = nullptr;
    VkImage array_ = VK_NULL_HANDLE;
    VkDeviceMemory memory_ = VK_NULL_HANDLE;
    VkImageView view_ = VK_NULL_HANDLE;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    u32 levels_ = 1, layers_ = 1, size_ = 1;
    Layer layer_[kLayers];
};
}
