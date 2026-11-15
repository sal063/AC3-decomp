#include "render/terrain.hpp"

#include "core/log.hpp"
#include "shaders.hpp"

#include <cmath>

namespace ac3::render {
namespace {
struct Vertex {
    s16 x, y, z, pad;
    u8 u, v, pad2[2];
    u8 r, g, b, a;
    u16 tpage, clut;
};
static_assert(sizeof(Vertex) == 20);

struct Push {
    float rot0[4], rot1[4], rot2[4];
    float cam[4];
    float proj[4];
};
static_assert(sizeof(Push) <= 128);

constexpr float kTileSize = 2048.0f;
}

Camera Camera::from_angles(float x, float y, float z, float heading, float pitch, float h) {
    const float yaw = heading * 0.0174532925f, pt = pitch * 0.0174532925f;
    const float f[3] = {std::sin(yaw) * std::cos(pt), std::sin(pt), std::cos(yaw) * std::cos(pt)};
    float r[3] = {std::cos(yaw), 0.0f, -std::sin(yaw)};
    const float d[3] = {f[1] * r[2] - f[2] * r[1], f[2] * r[0] - f[0] * r[2], f[0] * r[1] - f[1] * r[0]};
    Camera c;
    for (int i = 0; i < 3; ++i) {
        c.rot[i] = r[i];
        c.rot[3 + i] = d[i];
        c.rot[6 + i] = f[i];
    }
    c.pos[0] = x;
    c.pos[1] = y;
    c.pos[2] = z;
    c.h = h;
    return c;
}

bool TerrainRenderer::init(const Gpu& gpu, const Target& target, const assets::Mission& mission,
                           const assets::Vram& vram) {
    gpu_ = &gpu;

    std::vector<Vertex> verts;
    std::vector<u32> first(mission.tiles.size() + 1, 0), count(mission.tiles.size() + 1, 0);
    for (size_t t = 0; t < mission.tiles.size(); ++t) {
        const assets::TileType& tile = mission.tiles[t];
        first[t + 1] = static_cast<u32>(verts.size());
        for (const assets::TilePoly& p : tile.polys) {
            const bool tri = p.idx[3] == p.idx[2] || p.idx[3] == p.idx[1];
            const int corners[2][3] = {{0, 1, 2}, {1, 2, 3}};
            for (int k = 0; k < (tri ? 1 : 2); ++k)
                for (int j : corners[k]) {
                    if (p.idx[j] >= tile.verts.size()) continue;
                    const assets::TileVertex& tv = tile.verts[p.idx[j]];
                    const u32 c = p.gouraud ? p.rgb[j] : p.rgb[0];
                    verts.push_back({tv.x, tv.y, tv.z, 0, p.u[j], p.v[j], {0, 0},
                                     static_cast<u8>(c), static_cast<u8>(c >> 8), static_cast<u8>(c >> 16), 255,
                                     p.tpage, p.clut});
                }
        }
        count[t + 1] = static_cast<u32>(verts.size()) - first[t + 1];
    }

    std::vector<std::vector<float>> by_type(mission.tiles.size() + 1);
    for (int row = 0; row < 256; ++row)
        for (int col = 0; col < 256; ++col) {
            const u16 t = mission.grid[static_cast<size_t>(row) * 256 + col] & 0x1FF;
            if (t == 0 || t > mission.tiles.size()) continue;
            by_type[t].push_back((static_cast<float>(col) - 128.0f) * kTileSize + 1024.0f);
            by_type[t].push_back((static_cast<float>(row) - 128.0f) * kTileSize + 1024.0f);
        }
    std::vector<float> inst;
    for (size_t t = 1; t < by_type.size(); ++t) {
        if (by_type[t].empty() || count[t] == 0) continue;
        const u32 n = static_cast<u32>(by_type[t].size() / 2);
        draws_.push_back({first[t], count[t], static_cast<u32>(inst.size() / 2), n});
        inst.insert(inst.end(), by_type[t].begin(), by_type[t].end());
        triangles_ += count[t] / 3 * n;
        instances_ += n;
    }
    if (verts.empty() || inst.empty()) {
        log::error("terrain: nothing to draw");
        return false;
    }
    if (!upload_buffer(gpu, verts.data(), verts.size() * sizeof(Vertex), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices_) ||
        !upload_buffer(gpu, inst.data(), inst.size() * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, tiles_))
        return false;

    if (!create_image(gpu, 1024, 512, VK_FORMAT_R16_UINT,
                      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, vram_) ||
        !upload_image(gpu, vram_, vram.pixels().data(), vram.pixels().size() * 2))
        return false;

    VkSamplerCreateInfo si{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    if (!vk_ok(vkCreateSampler(gpu.device, &si, nullptr, &sampler_), "vkCreateSampler")) return false;

    VkDescriptorSetLayoutBinding b{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    dl.bindingCount = 1;
    dl.pBindings = &b;
    if (!vk_ok(vkCreateDescriptorSetLayout(gpu.device, &dl, nullptr, &set_layout_), "vkCreateDescriptorSetLayout"))
        return false;
    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    pi.maxSets = 1;
    pi.poolSizeCount = 1;
    pi.pPoolSizes = &ps;
    if (!vk_ok(vkCreateDescriptorPool(gpu.device, &pi, nullptr, &pool_), "vkCreateDescriptorPool")) return false;
    VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    ai.descriptorPool = pool_;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts = &set_layout_;
    if (!vk_ok(vkAllocateDescriptorSets(gpu.device, &ai, &set_), "vkAllocateDescriptorSets")) return false;
    VkDescriptorImageInfo ii{sampler_, vram_.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    w.dstSet = set_;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &ii;
    vkUpdateDescriptorSets(gpu.device, 1, &w, 0, nullptr);

    VkPushConstantRange pr{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Push)};
    VkPipelineLayoutCreateInfo li{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &set_layout_;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &pr;
    if (!vk_ok(vkCreatePipelineLayout(gpu.device, &li, nullptr, &layout_), "vkCreatePipelineLayout")) return false;

    VkShaderModule vs = make_shader(gpu, spv_terrain_vert, spv_terrain_vert_size);
    VkShaderModule fs = make_shader(gpu, spv_terrain_frag, spv_terrain_frag_size);
    VkPipelineShaderStageCreateInfo stages[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                                 {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vs;
    stages[0].pName = "main";
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fs;
    stages[1].pName = "main";

    VkVertexInputBindingDescription binds[2] = {{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX},
                                                {1, 8, VK_VERTEX_INPUT_RATE_INSTANCE}};
    VkVertexInputAttributeDescription attrs[5] = {
        {0, 0, VK_FORMAT_R16G16B16A16_SINT, 0},
        {1, 0, VK_FORMAT_R8G8_UINT, 8},
        {2, 0, VK_FORMAT_R8G8B8A8_UNORM, 12},
        {3, 0, VK_FORMAT_R16G16_UINT, 16},
        {4, 1, VK_FORMAT_R32G32_SFLOAT, 0},
    };
    VkPipelineVertexInputStateCreateInfo vi{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 2;
    vi.pVertexBindingDescriptions = binds;
    vi.vertexAttributeDescriptionCount = 5;
    vi.pVertexAttributeDescriptions = attrs;
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
    ds.depthTestEnable = VK_TRUE;
    ds.depthWriteEnable = VK_TRUE;
    ds.depthCompareOp = VK_COMPARE_OP_GREATER;
    VkPipelineColorBlendAttachmentState cba{};
    cba.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cb{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1;
    cb.pAttachments = &cba;
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dy{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dy.dynamicStateCount = 2;
    dy.pDynamicStates = dyn;
    VkPipelineRenderingCreateInfo rinfo{VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    rinfo.colorAttachmentCount = 1;
    rinfo.pColorAttachmentFormats = &target.color_format;
    rinfo.depthAttachmentFormat = target.depth_format;
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
    gp.pColorBlendState = &cb;
    gp.pDynamicState = &dy;
    gp.layout = layout_;
    const bool ok = vk_ok(vkCreateGraphicsPipelines(gpu.device, VK_NULL_HANDLE, 1, &gp, nullptr, &pipeline_),
                          "vkCreateGraphicsPipelines");
    vkDestroyShaderModule(gpu.device, vs, nullptr);
    vkDestroyShaderModule(gpu.device, fs, nullptr);
    return ok;
}

void TerrainRenderer::destroy() {
    if (!gpu_) return;
    const VkDevice d = gpu_->device;
    vkDeviceWaitIdle(d);
    if (pipeline_) vkDestroyPipeline(d, pipeline_, nullptr);
    if (layout_) vkDestroyPipelineLayout(d, layout_, nullptr);
    if (pool_) vkDestroyDescriptorPool(d, pool_, nullptr);
    if (set_layout_) vkDestroyDescriptorSetLayout(d, set_layout_, nullptr);
    if (sampler_) vkDestroySampler(d, sampler_, nullptr);
    render::destroy(*gpu_, vram_);
    render::destroy(*gpu_, vertices_);
    render::destroy(*gpu_, tiles_);
    pipeline_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    pool_ = VK_NULL_HANDLE;
    set_layout_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    gpu_ = nullptr;
}

void TerrainRenderer::draw(VkCommandBuffer cb, const Target& target, const Camera& cam) {
    if (!pipeline_) return;
    Push p{};
    for (int i = 0; i < 3; ++i) {
        p.rot0[i] = cam.rot[i];
        p.rot1[i] = cam.rot[3 + i];
        p.rot2[i] = cam.rot[6 + i];
        p.cam[i] = cam.pos[i];
    }
    p.cam[3] = cam.h;
    p.proj[0] = static_cast<float>(target.extent.width) / static_cast<float>(target.extent.height);
    p.proj[1] = cam.near_z;
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set_, 0, nullptr);
    vkCmdPushConstants(cb, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof p, &p);
    const VkBuffer bufs[2] = {vertices_.buffer, tiles_.buffer};
    const VkDeviceSize offs[2] = {0, 0};
    vkCmdBindVertexBuffers(cb, 0, 2, bufs, offs);
    for (const Draw& d : draws_) vkCmdDraw(cb, d.vertex_count, d.instance_count, d.first_vertex, d.first_instance);
}
}
