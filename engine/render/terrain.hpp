#pragma once

#include "assets/mission.hpp"
#include "render/renderer.hpp"

#include <vector>

namespace ac3::render {
struct Camera {
    float rot[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    float pos[3] = {0, 0, 0};
    float h = 256.0f;
    float near_z = 16.0f;

    static Camera from_angles(float x, float y, float z, float heading, float pitch, float h = 256.0f);
};

class TerrainRenderer {
public:
    bool init(const Gpu& gpu, const Target& target, const assets::Mission& mission, const assets::Vram& vram);
    void destroy();
    void draw(VkCommandBuffer cb, const Target& target, const Camera& cam);

    u32 triangles() const { return triangles_; }
    u32 instances() const { return instances_; }

private:
    struct Draw {
        u32 first_vertex, vertex_count, first_instance, instance_count;
    };
    const Gpu* gpu_ = nullptr;
    Buffer vertices_, tiles_;
    Image vram_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout set_layout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::vector<Draw> draws_;
    u32 triangles_ = 0, instances_ = 0;
};
}
