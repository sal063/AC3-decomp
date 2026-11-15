#pragma once

#include "render/vk.hpp"

namespace ac3::render {
class Ocean {
public:
    static constexpr u32 kN = 256;
    static constexpr u32 kCascades = 3;
    static constexpr u32 kMips = 9;

    bool init(const Gpu& gpu);
    void destroy();
    bool ready() const { return ready_; }
    VkDescriptorSetLayout set_layout() const { return set_layout_; }
    VkDescriptorSet set() const { return set_; }

    void simulate(VkCommandBuffer cb, double time);

    static constexpr u32 kShore = 2048;
    bool set_shore(const u8* cells);

    static void eye_uv(double x, double z, float out[2 * kCascades]);

private:
    struct Layers {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
        VkImageView store = VK_NULL_HANDLE;
    };
    bool make_layers(VkFormat format, u32 mips, VkImageUsageFlags usage, Layers& out);
    void free_layers(Layers& l);
    bool make_compute();
    void make_spectrum(float* out);

    const Gpu* gpu_ = nullptr;
    bool ready_ = false, first_ = true;
    double last_time_ = 0;
    Layers h0_, p_, q_, disp_, deriv_;
    VkSampler sampler_ = VK_NULL_HANDLE, shore_sampler_ = VK_NULL_HANDLE;
    Image shore_;
    Buffer params_buffer_;
    float params_[9][4] = {};
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout sim_layout_ = VK_NULL_HANDLE;
    VkDescriptorSet sim_set_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline spectrum_ = VK_NULL_HANDLE, fft_ = VK_NULL_HANDLE, finalize_ = VK_NULL_HANDLE;
};
}
