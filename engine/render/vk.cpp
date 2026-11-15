#include "render/vk.hpp"

#include "core/log.hpp"

#include <cstring>

namespace ac3::render {
bool vk_ok(VkResult r, const char* what) {
    if (r == VK_SUCCESS) return true;
    log::error("{} failed: VkResult {}", what, static_cast<int>(r));
    return false;
}

u32 find_memory(const Gpu& gpu, u32 type_bits, VkMemoryPropertyFlags want) {
    VkPhysicalDeviceMemoryProperties mp;
    vkGetPhysicalDeviceMemoryProperties(gpu.phys, &mp);
    for (u32 i = 0; i < mp.memoryTypeCount; ++i)
        if ((type_bits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & want) == want) return i;
    return UINT32_MAX;
}

bool create_buffer(const Gpu& gpu, VkDeviceSize size, VkBufferUsageFlags usage, bool host, Buffer& out) {
    out = Buffer{};
    VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bi.size = size;
    bi.usage = usage;
    bi.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (!vk_ok(vkCreateBuffer(gpu.device, &bi, nullptr, &out.buffer), "vkCreateBuffer")) return false;
    VkMemoryRequirements req;
    vkGetBufferMemoryRequirements(gpu.device, out.buffer, &req);
    const VkMemoryPropertyFlags want = host ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                                            : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory(gpu, req.memoryTypeBits, want);
    if (ai.memoryTypeIndex == UINT32_MAX || !vk_ok(vkAllocateMemory(gpu.device, &ai, nullptr, &out.memory), "vkAllocateMemory")) {
        destroy(gpu, out);
        return false;
    }
    vkBindBufferMemory(gpu.device, out.buffer, out.memory, 0);
    if (host) vkMapMemory(gpu.device, out.memory, 0, VK_WHOLE_SIZE, 0, &out.mapped);
    out.size = size;
    return true;
}

void destroy(const Gpu& gpu, Buffer& b) {
    if (b.buffer) vkDestroyBuffer(gpu.device, b.buffer, nullptr);
    if (b.memory) vkFreeMemory(gpu.device, b.memory, nullptr);
    b = Buffer{};
}

bool create_image(const Gpu& gpu, u32 w, u32 h, VkFormat format, VkImageUsageFlags usage,
                  VkImageAspectFlags aspect, Image& out) {
    out = Image{};
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = {w, h, 1};
    ii.mipLevels = 1;
    ii.arrayLayers = 1;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(gpu.device, &ii, nullptr, &out.image), "vkCreateImage")) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu.device, out.image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX || !vk_ok(vkAllocateMemory(gpu.device, &ai, nullptr, &out.memory), "vkAllocateMemory")) {
        destroy(gpu, out);
        return false;
    }
    vkBindImageMemory(gpu.device, out.image, out.memory, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = out.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vi.format = format;
    vi.subresourceRange = {aspect, 0, 1, 0, 1};
    if (!vk_ok(vkCreateImageView(gpu.device, &vi, nullptr, &out.view), "vkCreateImageView")) {
        destroy(gpu, out);
        return false;
    }
    out.format = format;
    out.width = w;
    out.height = h;
    return true;
}

void destroy(const Gpu& gpu, Image& img) {
    if (img.view) vkDestroyImageView(gpu.device, img.view, nullptr);
    if (img.image) vkDestroyImage(gpu.device, img.image, nullptr);
    if (img.memory) vkFreeMemory(gpu.device, img.memory, nullptr);
    img = Image{};
}

bool one_shot(const Gpu& gpu, const std::function<void(VkCommandBuffer)>& record) {
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    ai.commandPool = gpu.transient_pool;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;
    VkCommandBuffer cb;
    if (!vk_ok(vkAllocateCommandBuffers(gpu.device, &ai, &cb), "vkAllocateCommandBuffers")) return false;
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    record(cb);
    vkEndCommandBuffer(cb);
    VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cbi.commandBuffer = cb;
    VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cbi;
    const bool ok = vk_ok(vkQueueSubmit2(gpu.queue, 1, &si, VK_NULL_HANDLE), "vkQueueSubmit2") &&
                    vk_ok(vkQueueWaitIdle(gpu.queue), "vkQueueWaitIdle");
    vkFreeCommandBuffers(gpu.device, gpu.transient_pool, 1, &cb);
    return ok;
}

bool upload_buffer(const Gpu& gpu, const void* data, VkDeviceSize size, VkBufferUsageFlags usage, Buffer& out) {
    Buffer staging;
    if (!create_buffer(gpu, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, staging)) return false;
    std::memcpy(staging.mapped, data, size);
    bool ok = create_buffer(gpu, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, false, out);
    if (ok)
        ok = one_shot(gpu, [&](VkCommandBuffer cb) {
            VkBufferCopy c{0, 0, size};
            vkCmdCopyBuffer(cb, staging.buffer, out.buffer, 1, &c);
        });
    destroy(gpu, staging);
    return ok;
}

bool upload_image(const Gpu& gpu, Image& img, const void* data, VkDeviceSize size) {
    Buffer staging;
    if (!create_buffer(gpu, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, staging)) return false;
    std::memcpy(staging.mapped, data, size);
    const bool ok = one_shot(gpu, [&](VkCommandBuffer cb) {
        image_barrier(cb, img.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_NONE, 0,
                      VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
        VkBufferImageCopy r{};
        r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        r.imageExtent = {img.width, img.height, 1};
        vkCmdCopyBufferToImage(cb, staging.buffer, img.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r);
        image_barrier(cb, img.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                      VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                      VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);
    });
    destroy(gpu, staging);
    return ok;
}

void image_barrier(VkCommandBuffer cb, VkImage image, VkImageAspectFlags aspect, VkImageLayout from,
                   VkImageLayout to, VkPipelineStageFlags2 src_stage, VkAccessFlags2 src_access,
                   VkPipelineStageFlags2 dst_stage, VkAccessFlags2 dst_access) {
    VkImageMemoryBarrier2 b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = src_stage;
    b.srcAccessMask = src_access;
    b.dstStageMask = dst_stage;
    b.dstAccessMask = dst_access;
    b.oldLayout = from;
    b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {aspect, 0, 1, 0, 1};
    VkDependencyInfo dep{VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cb, &dep);
}

VkShaderModule make_shader(const Gpu& gpu, const u32* code, size_t bytes) {
    VkShaderModuleCreateInfo ci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = bytes;
    ci.pCode = code;
    VkShaderModule m = VK_NULL_HANDLE;
    vk_ok(vkCreateShaderModule(gpu.device, &ci, nullptr, &m), "vkCreateShaderModule");
    return m;
}
}
