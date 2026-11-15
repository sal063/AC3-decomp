#pragma once

#include "render/vk.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

struct SDL_Window;

namespace ac3::render {
struct Config {
    bool validation = false;
    bool vsync = true;
    u32 headless_width = 1280;
    u32 headless_height = 720;
};

struct DeviceInfo {
    std::string name;
    std::string driver;
    unsigned api_major = 0, api_minor = 0, api_patch = 0;
};

struct Target {
    VkExtent2D extent{};
    VkFormat color_format = VK_FORMAT_R8G8B8A8_UNORM;
    VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
};

using SceneFn = std::function<void(VkCommandBuffer, const Target&)>;
using PrepFn = std::function<void(VkCommandBuffer, u32 slot)>;
constexpr u32 kFramesInFlight = 2;

class Renderer {
public:
    Renderer();
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool init(SDL_Window* window, const Config& config);
    void shutdown();

    const DeviceInfo& device() const;
    const Gpu& gpu() const;
    Target target() const;

    void resized();

    bool frame(const float clear[3], const SceneFn& scene, const PrepFn& prep = nullptr);
    bool paced() const;
    void set_vsync(bool on);

    bool draw_clear(float r, float g, float b);

    bool read_back(std::vector<u8>& rgba, u32& width, u32& height);
    bool screenshot(const std::string& png_path);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
