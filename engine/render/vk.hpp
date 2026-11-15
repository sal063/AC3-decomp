#pragma once

#include "core/types.hpp"

#include <vulkan/vulkan.h>

#include <functional>

namespace ac3::render {
struct Gpu {
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    u32 queue_family = 0;
    VkCommandPool transient_pool = VK_NULL_HANDLE;
    bool anisotropy = false;
};

struct Buffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* mapped = nullptr;
    VkDeviceSize size = 0;
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
    u32 width = 0, height = 0;
};

bool vk_ok(VkResult r, const char* what);

u32 find_memory(const Gpu& gpu, u32 type_bits, VkMemoryPropertyFlags want);

bool create_buffer(const Gpu& gpu, VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buffer& out);
void destroy(const Gpu& gpu, Buffer& b);

bool create_image(const Gpu& gpu, u32 w, u32 h, VkFormat format, VkImageUsageFlags usage,
                  VkImageAspectFlags aspect, Image& out);
void destroy(const Gpu& gpu, Image& img);

bool one_shot(const Gpu& gpu, const std::function<void(VkCommandBuffer)>& record);

bool upload_buffer(const Gpu& gpu, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, Buffer& out);

bool upload_image(const Gpu& gpu, Image& img, const void* data, VkDeviceSize size);

void image_barrier(VkCommandBuffer cb, VkImage image, VkImageAspectFlags aspect, VkImageLayout from,
                   VkImageLayout to, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                   VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access);

VkShaderModule make_shader(const Gpu& gpu, const u32* code, size_t bytes);
}
