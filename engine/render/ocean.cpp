#include "render/ocean.hpp"

#include "core/log.hpp"
#include "shaders.hpp"

#include <cmath>
#include <cstring>
#include <vector>

namespace ac3::render {
namespace {
constexpr u32 N = Ocean::kN, NC = Ocean::kCascades, MIPS = Ocean::kMips;
constexpr double kPi = 3.14159265358979323846;

constexpr float kPatchM[NC] = {500.0f, 85.0f, 17.0f};
constexpr float kBand[NC + 1] = {0.0f, 0.296f, 1.48f, 20.0f};
constexpr double kTurn[NC] = {0.0, 8.0, -13.0};
constexpr double kWind[2] = {0.8253, 0.5646};
constexpr double kWindSpeed = 10.0;
constexpr double kHeight = 2.0;
constexpr float kChoppy = 1.2f;
constexpr double kLoop = 200.0;
constexpr double kFoamLife = 2.0;
constexpr float kUnitsPerMetre = 5.0f;

static_assert(2 + (NC * MIPS + 3) / 4 <= 9, "the roughness table does not fit the parameters");

struct Rng {
    u32 s;
    double next() {
        s ^= s << 13;
        s ^= s >> 17;
        s ^= s << 5;
        return (static_cast<double>(s >> 8) + 0.5) / 16777216.0;
    }
    void gauss(double& a, double& b) {
        const double u = next(), v = next();
        const double r = std::sqrt(-2.0 * std::log(u));
        a = r * std::cos(2.0 * kPi * v);
        b = r * std::sin(2.0 * kPi * v);
    }
};

void memory_barrier(VkCommandBuffer cb, VkPipelineStageFlags from, VkAccessFlags from_access, VkPipelineStageFlags to,
                    VkAccessFlags to_access) {
    VkMemoryBarrier b{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    b.srcAccessMask = from_access;
    b.dstAccessMask = to_access;
    vkCmdPipelineBarrier(cb, from, to, 0, 1, &b, 0, nullptr, 0, nullptr);
}

constexpr VkPipelineStageFlags kReaders = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
constexpr VkPipelineStageFlags kCompute = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
constexpr VkPipelineStageFlags kTransfer = VK_PIPELINE_STAGE_TRANSFER_BIT;
constexpr VkAccessFlags kShaderRW = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
constexpr VkAccessFlags kTransferRW = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
}

void Ocean::eye_uv(double x, double z, float out[2 * kCascades]) {
    for (u32 c = 0; c < NC; ++c) {
        const double a = kTurn[c] * kPi / 180.0, cs = std::cos(a), sn = std::sin(a);
        const double patch = static_cast<double>(kPatchM[c]) * kUnitsPerMetre;
        const double u = (cs * x - sn * z) / patch, v = (sn * x + cs * z) / patch;
        out[2 * c] = static_cast<float>(u - std::floor(u));
        out[2 * c + 1] = static_cast<float>(v - std::floor(v));
    }
}

void Ocean::make_spectrum(float* out) {
    const double g = 9.81, longest = kWindSpeed * kWindSpeed / g;
    const size_t cells = static_cast<size_t>(N) * N;
    std::vector<double> re(NC * cells), im(NC * cells), kk(NC * cells);
    double variance = 0.0;
    for (u32 c = 0; c < NC; ++c) {
        const double a = kTurn[c] * kPi / 180.0;
        const double wx = std::cos(a) * kWind[0] - std::sin(a) * kWind[1];
        const double wz = std::sin(a) * kWind[0] + std::cos(a) * kWind[1];
        const double dk = 2.0 * kPi / kPatchM[c];
        Rng rng{0x9E3779B9u * (c + 1) + 12345u};
        for (u32 j = 0; j < N; ++j)
            for (u32 i = 0; i < N; ++i) {
                const size_t o = c * cells + static_cast<size_t>(j) * N + i;
                const double kx = dk * (static_cast<int>(i) - static_cast<int>(N / 2));
                const double kz = dk * (static_cast<int>(j) - static_cast<int>(N / 2));
                const double k = std::sqrt(kx * kx + kz * kz);
                double ga, gb;
                rng.gauss(ga, gb);
                re[o] = im[o] = 0.0;
                kk[o] = k;
                if (i == 0 || j == 0 || k <= 0.0 || k < kBand[c] || k >= kBand[c + 1]) continue;
                const double d = (kx * wx + kz * wz) / k;
                double p = std::exp(-1.0 / (k * longest * k * longest)) / (k * k * k * k) * d * d *
                           std::exp(-(k * 0.04) * (k * 0.04));
                if (d < 0.0) p *= 0.07;
                const double amp = std::sqrt(p * dk * dk * 0.5);
                re[o] = amp * ga;
                im[o] = amp * gb;
                variance += 2.0 * (re[o] * re[o] + im[o] * im[o]);
            }
    }
    const double scale = variance > 0.0 ? (kHeight / 4.0) / std::sqrt(variance) : 0.0;
    double slope_total = 0.0;
    float lost[NC][MIPS];
    for (u32 c = 0; c < NC; ++c) {
        double band_lost[MIPS] = {};
        for (u32 j = 0; j < N; ++j)
            for (u32 i = 0; i < N; ++i) {
                const size_t o = c * cells + static_cast<size_t>(j) * N + i;
                const size_t n = c * cells + static_cast<size_t>((N - j) & (N - 1)) * N + ((N - i) & (N - 1));
                float* t = out + o * 4;
                t[0] = static_cast<float>(re[o] * scale);
                t[1] = static_cast<float>(im[o] * scale);
                t[2] = static_cast<float>(re[n] * scale);
                t[3] = static_cast<float>(-im[n] * scale);
                const double s = 2.0 * kk[o] * kk[o] * (re[o] * re[o] + im[o] * im[o]) * scale * scale;
                slope_total += s;
                for (u32 m = 0; m < MIPS; ++m) {
                    const double cut = 0.6 * kPi * N / (static_cast<double>(kPatchM[c]) * static_cast<double>(1u << m));
                    if (kk[o] > cut) band_lost[m] += s;
                }
            }
        for (u32 m = 0; m < MIPS; ++m) lost[c][m] = static_cast<float>(band_lost[m]);
    }
    const double base = std::max(0.006, 0.003 + 0.00512 * kWindSpeed - slope_total);
    std::memset(params_, 0, sizeof params_);
    for (u32 c = 0; c < NC; ++c) params_[0][c] = kPatchM[c] * kUnitsPerMetre;
    params_[0][3] = kChoppy;
    params_[1][0] = static_cast<float>(base);
    params_[1][1] = kUnitsPerMetre;
    for (u32 c = 0; c < NC; ++c)
        for (u32 m = 0; m < MIPS; ++m) {
            const u32 k = c * MIPS + m;
            params_[2 + k / 4][k % 4] = lost[c][m];
        }
    log::info("water: waves of {:.1f} m (significant), slope variance {:.4f} in them, {:.4f} below them", kHeight,
              slope_total, base);
}

bool Ocean::make_layers(VkFormat format, u32 mips, VkImageUsageFlags usage, Layers& out) {
    const Gpu& gpu = *gpu_;
    VkImageCreateInfo ii{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ii.imageType = VK_IMAGE_TYPE_2D;
    ii.format = format;
    ii.extent = {N, N, 1};
    ii.mipLevels = mips;
    ii.arrayLayers = NC;
    ii.samples = VK_SAMPLE_COUNT_1_BIT;
    ii.tiling = VK_IMAGE_TILING_OPTIMAL;
    ii.usage = usage;
    ii.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ii.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (!vk_ok(vkCreateImage(gpu.device, &ii, nullptr, &out.image), "vkCreateImage (water)")) return false;
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(gpu.device, out.image, &req);
    VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = req.size;
    ai.memoryTypeIndex = find_memory(gpu, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ai.memoryTypeIndex == UINT32_MAX ||
        !vk_ok(vkAllocateMemory(gpu.device, &ai, nullptr, &out.memory), "vkAllocateMemory (water)"))
        return false;
    vkBindImageMemory(gpu.device, out.image, out.memory, 0);
    VkImageViewCreateInfo vi{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    vi.image = out.image;
    vi.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    vi.format = format;
    vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, NC};
    if (!vk_ok(vkCreateImageView(gpu.device, &vi, nullptr, &out.store), "vkCreateImageView (water)")) return false;
    vi.subresourceRange.levelCount = mips;
    return vk_ok(vkCreateImageView(gpu.device, &vi, nullptr, &out.view), "vkCreateImageView (water)");
}

void Ocean::free_layers(Layers& l) {
    const VkDevice d = gpu_->device;
    if (l.view) vkDestroyImageView(d, l.view, nullptr);
    if (l.store) vkDestroyImageView(d, l.store, nullptr);
    if (l.image) vkDestroyImage(d, l.image, nullptr);
    if (l.memory) vkFreeMemory(d, l.memory, nullptr);
    l = Layers{};
}

bool Ocean::make_compute() {
    const Gpu& gpu = *gpu_;
    u32 qn = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu.phys, &qn, nullptr);
    std::vector<VkQueueFamilyProperties> qf(qn);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu.phys, &qn, qf.data());
    if (gpu.queue_family >= qn || !(qf[gpu.queue_family].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
        log::warn("water: the queue cannot compute; the game's own water is drawn");
        return false;
    }
    VkDescriptorSetLayoutBinding b[5] = {};
    for (u32 i = 0; i < 5; ++i) b[i] = {i, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 5;
    dl.pBindings = b;
    if (!vk_ok(vkCreateDescriptorSetLayout(gpu.device, &dl, nullptr, &sim_layout_), "vkCreateDescriptorSetLayout (water)"))
        return false;
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &sim_layout_;
    if (!vk_ok(vkAllocateDescriptorSets(gpu.device, &ai, &sim_set_), "vkAllocateDescriptorSets (water)")) return false;
    const VkDescriptorImageInfo ii[5] = {{VK_NULL_HANDLE, h0_.store, VK_IMAGE_LAYOUT_GENERAL},
                                         {VK_NULL_HANDLE, p_.store, VK_IMAGE_LAYOUT_GENERAL},
                                         {VK_NULL_HANDLE, q_.store, VK_IMAGE_LAYOUT_GENERAL},
                                         {VK_NULL_HANDLE, disp_.store, VK_IMAGE_LAYOUT_GENERAL},
                                         {VK_NULL_HANDLE, deriv_.store, VK_IMAGE_LAYOUT_GENERAL}};
    VkWriteDescriptorSet w[5] = {};
    for (u32 i = 0; i < 5; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = sim_set_;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        w[i].pImageInfo = &ii[i];
    }
    vkUpdateDescriptorSets(gpu.device, 5, w, 0, nullptr);
    VkPushConstantRange pr{VK_SHADER_STAGE_COMPUTE_BIT, 0, 32};
    VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    pl.setLayoutCount = 1;
    pl.pSetLayouts = &sim_layout_;
    pl.pushConstantRangeCount = 1;
    pl.pPushConstantRanges = &pr;
    if (!vk_ok(vkCreatePipelineLayout(gpu.device, &pl, nullptr, &layout_), "vkCreatePipelineLayout (water)")) return false;
    auto pipeline = [&](const u32* code, size_t bytes) {
        VkPipeline p = VK_NULL_HANDLE;
        const VkShaderModule m = make_shader(gpu, code, bytes);
        if (!m) return p;
        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        ci.stage.module = m;
        ci.stage.pName = "main";
        ci.layout = layout_;
        vk_ok(vkCreateComputePipelines(gpu.device, VK_NULL_HANDLE, 1, &ci, nullptr, &p), "vkCreateComputePipelines (water)");
        vkDestroyShaderModule(gpu.device, m, nullptr);
        return p;
    };
    spectrum_ = pipeline(spv_ocean_spectrum_comp, spv_ocean_spectrum_comp_size);
    fft_ = pipeline(spv_ocean_fft_comp, spv_ocean_fft_comp_size);
    finalize_ = pipeline(spv_ocean_finalize_comp, spv_ocean_finalize_comp_size);
    return spectrum_ && fft_ && finalize_;
}

bool Ocean::init(const Gpu& gpu) {
    gpu_ = &gpu;
    ready_ = false;
    first_ = true;
    constexpr VkImageUsageFlags kStore = VK_IMAGE_USAGE_STORAGE_BIT;
    constexpr VkImageUsageFlags kTexture = kStore | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                           VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (!make_layers(VK_FORMAT_R32G32B32A32_SFLOAT, 1, kStore | VK_IMAGE_USAGE_TRANSFER_DST_BIT, h0_) ||
        !make_layers(VK_FORMAT_R32G32B32A32_SFLOAT, 1, kStore, p_) ||
        !make_layers(VK_FORMAT_R32G32B32A32_SFLOAT, 1, kStore, q_) ||
        !make_layers(VK_FORMAT_R16G16B16A16_SFLOAT, MIPS, kTexture, disp_) ||
        !make_layers(VK_FORMAT_R16G16B16A16_SFLOAT, MIPS, kTexture, deriv_))
        return destroy(), false;

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.maxLod = static_cast<float>(MIPS - 1);
    si.anisotropyEnable = gpu.anisotropy ? VK_TRUE : VK_FALSE;
    si.maxAnisotropy = gpu.anisotropy ? 16.0f : 1.0f;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &sampler_), "vkCreateSampler (water)")) return destroy(), false;

    const VkDeviceSize bytes = static_cast<VkDeviceSize>(NC) * N * N * 16;
    Buffer staging;
    if (!create_buffer(gpu, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, true, staging)) return destroy(), false;
    make_spectrum(static_cast<float*>(staging.mapped));
    const bool filled = one_shot(gpu, [&](VkCommandBuffer cb) {
        const Layers* all[5] = {&h0_, &p_, &q_, &disp_, &deriv_};
        for (u32 i = 0; i < 5; ++i) {
            VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            b.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            b.image = all[i]->image;
            b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, i >= 3 ? MIPS : 1u, 0, NC};
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, kTransfer, 0, 0, nullptr, 0, nullptr, 1, &b);
        }
        const VkClearColorValue zero{};
        const VkImageSubresourceRange mips{VK_IMAGE_ASPECT_COLOR_BIT, 0, MIPS, 0, NC};
        vkCmdClearColorImage(cb, disp_.image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &mips);
        vkCmdClearColorImage(cb, deriv_.image, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &mips);
        VkBufferImageCopy r{};
        r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, NC};
        r.imageExtent = {N, N, 1};
        vkCmdCopyBufferToImage(cb, staging.buffer, h0_.image, VK_IMAGE_LAYOUT_GENERAL, 1, &r);
        memory_barrier(cb, kTransfer, VK_ACCESS_TRANSFER_WRITE_BIT, kCompute | kTransfer | kReaders,
                       kShaderRW | kTransferRW);
    });
    render::destroy(gpu, staging);
    if (!filled) return destroy(), false;

    si = VkSamplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &shore_sampler_), "vkCreateSampler (water)")) return destroy(), false;
    if (!create_image(gpu, kShore, kShore, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                      VK_IMAGE_ASPECT_COLOR_BIT, shore_))
        return destroy(), false;
    {
        const std::vector<u8> open(static_cast<size_t>(kShore) * kShore, 255);
        if (!set_shore(open.data())) return destroy(), false;
    }

    if (!create_buffer(gpu, sizeof params_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, true, params_buffer_))
        return destroy(), false;
    std::memcpy(params_buffer_.mapped, params_, sizeof params_);

    const VkDescriptorSetLayoutBinding b[4] = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 4;
    dl.pBindings = b;
    if (!vk_ok(vkCreateDescriptorSetLayout(gpu.device, &dl, nullptr, &set_layout_), "vkCreateDescriptorSetLayout (water)"))
        return destroy(), false;
    const VkDescriptorPoolSize sizes[3] = {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
                                           {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},
                                           {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 5}};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 2;
    pi.poolSizeCount = 3;
    pi.pPoolSizes = sizes;
    if (!vk_ok(vkCreateDescriptorPool(gpu.device, &pi, nullptr, &pool_), "vkCreateDescriptorPool (water)"))
        return destroy(), false;
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &set_layout_;
    if (!vk_ok(vkAllocateDescriptorSets(gpu.device, &ai, &set_), "vkAllocateDescriptorSets (water)")) return destroy(), false;
    const VkDescriptorImageInfo images[4] = {{sampler_, deriv_.view, VK_IMAGE_LAYOUT_GENERAL},
                                             {sampler_, disp_.view, VK_IMAGE_LAYOUT_GENERAL},
                                             {},
                                             {shore_sampler_, shore_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
    const VkDescriptorBufferInfo block{params_buffer_.buffer, 0, sizeof params_};
    VkWriteDescriptorSet w[4] = {};
    for (u32 i = 0; i < 4; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = set_;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = i != 2 ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        if (i != 2) w[i].pImageInfo = &images[i];
        else w[i].pBufferInfo = &block;
    }
    vkUpdateDescriptorSets(gpu.device, 4, w, 0, nullptr);

    ready_ = make_compute();
    return true;
}

bool Ocean::set_shore(const u8* cells) {
    if (!gpu_ || !shore_.image) return false;
    return upload_image(*gpu_, shore_, cells, static_cast<VkDeviceSize>(kShore) * kShore);
}

void Ocean::destroy() {
    if (!gpu_ || !gpu_->device) return;
    const VkDevice d = gpu_->device;
    if (shore_sampler_) vkDestroySampler(d, shore_sampler_, nullptr);
    shore_sampler_ = VK_NULL_HANDLE;
    render::destroy(*gpu_, shore_);
    for (VkPipeline p : {spectrum_, fft_, finalize_})
        if (p) vkDestroyPipeline(d, p, nullptr);
    if (layout_) vkDestroyPipelineLayout(d, layout_, nullptr);
    if (pool_) vkDestroyDescriptorPool(d, pool_, nullptr);
    if (sim_layout_) vkDestroyDescriptorSetLayout(d, sim_layout_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(d, set_layout_, nullptr);
    if (sampler_) vkDestroySampler(d, sampler_, nullptr);
    render::destroy(*gpu_, params_buffer_);
    for (Layers* l : {&h0_, &p_, &q_, &disp_, &deriv_}) free_layers(*l);
    spectrum_ = fft_ = finalize_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    sim_layout_ = set_layout_ = VK_NULL_HANDLE;
    set_ = sim_set_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    ready_ = false;
    gpu_ = nullptr;
}

void Ocean::simulate(VkCommandBuffer cb, double time) {
    if (!ready_) return;
    memory_barrier(cb, kReaders | kTransfer | kCompute,
                   VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, kCompute,
                   kShaderRW);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, layout_, 0, 1, &sim_set_, 0, nullptr);

    struct {
        float patch_m[4];
        float time, n, w0, pad;
    } sp{};
    for (u32 c = 0; c < 4; ++c) sp.patch_m[c] = c < NC ? kPatchM[c] : 1.0f;
    double t = std::fmod(time, kLoop);
    if (t < 0) t += kLoop;
    sp.time = static_cast<float>(t);
    sp.n = static_cast<float>(N);
    sp.w0 = static_cast<float>(2.0 * kPi / kLoop);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, spectrum_);
    vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof sp, &sp);
    vkCmdDispatch(cb, N / 16, N / 16, NC);
    memory_barrier(cb, kCompute, VK_ACCESS_SHADER_WRITE_BIT, kCompute, kShaderRW);

    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, fft_);
    for (s32 columns = 0; columns < 2; ++columns) {
        const s32 fp[2] = {columns, static_cast<s32>(NC)};
        vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof fp, fp);
        vkCmdDispatch(cb, 1, N, 2 * NC);
        memory_barrier(cb, kCompute, VK_ACCESS_SHADER_WRITE_BIT, kCompute, kShaderRW);
    }

    double dt = time - last_time_;
    if (dt < 0.0 || dt > 1.0) dt = 1.0 / 30.0;
    last_time_ = time;
    const struct {
        float choppy, decay, first, pad;
    } fp{kChoppy, static_cast<float>(std::exp(-dt / kFoamLife)), first_ ? 1.0f : 0.0f, 0.0f};
    first_ = false;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, finalize_);
    vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof fp, &fp);
    vkCmdDispatch(cb, N / 16, N / 16, NC);
    memory_barrier(cb, kCompute, VK_ACCESS_SHADER_WRITE_BIT, kTransfer, kTransferRW);

    for (u32 m = 1; m < MIPS; ++m) {
        VkImageBlit bl{};
        bl.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m - 1, 0, NC};
        bl.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, m, 0, NC};
        bl.srcOffsets[1] = {static_cast<s32>(N >> (m - 1)), static_cast<s32>(N >> (m - 1)), 1};
        bl.dstOffsets[1] = {static_cast<s32>(N >> m), static_cast<s32>(N >> m), 1};
        vkCmdBlitImage(cb, disp_.image, VK_IMAGE_LAYOUT_GENERAL, disp_.image, VK_IMAGE_LAYOUT_GENERAL, 1, &bl,
                       VK_FILTER_LINEAR);
        vkCmdBlitImage(cb, deriv_.image, VK_IMAGE_LAYOUT_GENERAL, deriv_.image, VK_IMAGE_LAYOUT_GENERAL, 1, &bl,
                       VK_FILTER_LINEAR);
        memory_barrier(cb, kTransfer, VK_ACCESS_TRANSFER_WRITE_BIT, kTransfer, kTransferRW);
    }
    memory_barrier(cb, kTransfer | kCompute, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, kReaders,
                   VK_ACCESS_SHADER_READ_BIT);
}
}
