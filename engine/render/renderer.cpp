#include "render/renderer.hpp"

#include "core/log.hpp"
#include "core/png.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace ac3::render {
namespace {
constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

VKAPI_ATTR VkBool32 VKAPI_CALL on_validation(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void*) {
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        log::error("vulkan: {}", data->pMessage);
    else
        log::warn("vulkan: {}", data->pMessage);
    return VK_FALSE;
}
}

struct Renderer::Impl {
    SDL_Window* window = nullptr;
    Config config;
    DeviceInfo info;
    Gpu gpu;

    VkInstance instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swap_format = VK_FORMAT_UNDEFINED;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkSemaphore> render_done;
    bool stale = true;

    Image color, depth;
    Target target;

    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
    };
    Frame frames[kFramesInFlight];

    u64 frame_index = 0;
    bool drew_once = false;
    bool fifo = false;
    PFN_vkWaitForPresentKHR wait_for_present = nullptr;
    u64 present_id = 0;

    bool create_instance();
    bool pick_device();
    bool create_device();
    bool create_frames();
    bool create_target(VkExtent2D e);
    bool create_swapchain();
    void destroy_swapchain_images();
    bool rebuild_swapchain();
    void record(VkCommandBuffer cb, const float clear[3], const SceneFn& scene, const PrepFn& prep);
    void destroy();
};

bool Renderer::Impl::create_instance() {
    std::vector<const char*> exts;
    if (window) {
        Uint32 n = 0;
        const char* const* sdl_exts = SDL_Vulkan_GetInstanceExtensions(&n);
        if (!sdl_exts) {
            log::error("SDL_Vulkan_GetInstanceExtensions: {}", SDL_GetError());
            return false;
        }
        exts.assign(sdl_exts, sdl_exts + n);
    }
    std::vector<const char*> layers;
    if (config.validation) {
        u32 n = 0;
        vkEnumerateInstanceLayerProperties(&n, nullptr);
        std::vector<VkLayerProperties> props(n);
        vkEnumerateInstanceLayerProperties(&n, props.data());
        const bool have = std::any_of(props.begin(), props.end(), [](const VkLayerProperties& p) {
            return std::strcmp(p.layerName, kValidationLayer) == 0;
        });
        if (have) {
            layers.push_back(kValidationLayer);
            exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            log::warn("validation requested but {} is not installed", kValidationLayer);
            config.validation = false;
        }
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "ac3engine";
    app.pEngineName = "ac3engine";
    app.apiVersion = VK_API_VERSION_1_3;
    VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = static_cast<u32>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = static_cast<u32>(layers.size());
    ci.ppEnabledLayerNames = layers.data();
    if (!vk_ok(vkCreateInstance(&ci, nullptr, &instance), "vkCreateInstance")) return false;

    if (config.validation) {
        VkDebugUtilsMessengerCreateInfoEXT mi{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
        mi.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        mi.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        mi.pfnUserCallback = on_validation;
        auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (create) create(instance, &mi, nullptr, &messenger);
    }
    if (window && !SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        log::error("SDL_Vulkan_CreateSurface: {}", SDL_GetError());
        return false;
    }
    return true;
}

bool Renderer::Impl::pick_device() {
    u32 n = 0;
    vkEnumeratePhysicalDevices(instance, &n, nullptr);
    std::vector<VkPhysicalDevice> all(n);
    vkEnumeratePhysicalDevices(instance, &n, all.data());
    int best = -1;
    for (VkPhysicalDevice pd : all) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(pd, &props);
        if (props.apiVersion < VK_API_VERSION_1_3) continue;
        VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        f2.pNext = &f13;
        vkGetPhysicalDeviceFeatures2(pd, &f2);
        if (!f13.dynamicRendering || !f13.synchronization2 || !f13.shaderDemoteToHelperInvocation) continue;
        if (surface) {
            u32 en = 0;
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &en, nullptr);
            std::vector<VkExtensionProperties> exts(en);
            vkEnumerateDeviceExtensionProperties(pd, nullptr, &en, exts.data());
            if (std::none_of(exts.begin(), exts.end(), [](const VkExtensionProperties& e) {
                    return std::strcmp(e.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0;
                }))
                continue;
        }
        u32 qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> qf(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, qf.data());
        int family = -1;
        for (u32 i = 0; i < qn && family < 0; ++i) {
            VkBool32 present = VK_TRUE;
            if (surface) vkGetPhysicalDeviceSurfaceSupportKHR(pd, i, surface, &present);
            if ((qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) family = static_cast<int>(i);
        }
        if (family < 0) continue;
        const int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 2 : 1;
        if (score > best) {
            best = score;
            gpu.phys = pd;
            gpu.queue_family = static_cast<u32>(family);
        }
    }
    if (!gpu.phys) {
        log::error("no Vulkan 1.3 device with dynamic rendering, synchronization2 and a graphics queue");
        return false;
    }
    VkPhysicalDeviceDriverProperties drv{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceProperties2 p2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    p2.pNext = &drv;
    vkGetPhysicalDeviceProperties2(gpu.phys, &p2);
    info.name = p2.properties.deviceName;
    info.driver = std::string(drv.driverName) + " " + drv.driverInfo;
    info.api_major = VK_API_VERSION_MAJOR(p2.properties.apiVersion);
    info.api_minor = VK_API_VERSION_MINOR(p2.properties.apiVersion);
    info.api_patch = VK_API_VERSION_PATCH(p2.properties.apiVersion);
    return true;
}

bool Renderer::Impl::create_device() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qi.queueFamilyIndex = gpu.queue_family;
    qi.queueCount = 1;
    qi.pQueuePriorities = &priority;
    VkPhysicalDeviceVulkan13Features f13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    f13.dynamicRendering = VK_TRUE;
    f13.synchronization2 = VK_TRUE;
    f13.shaderDemoteToHelperInvocation = VK_TRUE;
    VkPhysicalDeviceFeatures2 f2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    f2.pNext = &f13;
    {
        VkPhysicalDeviceFeatures have{};
        vkGetPhysicalDeviceFeatures(gpu.phys, &have);
        gpu.anisotropy = have.samplerAnisotropy == VK_TRUE;
        f2.features.samplerAnisotropy = have.samplerAnisotropy;
    }
    std::vector<const char*> exts;
    VkPhysicalDevicePresentIdFeaturesKHR pid{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_ID_FEATURES_KHR};
    VkPhysicalDevicePresentWaitFeaturesKHR pw{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PRESENT_WAIT_FEATURES_KHR};
    bool present_wait = false;
    if (surface) {
        exts.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        u32 en = 0;
        vkEnumerateDeviceExtensionProperties(gpu.phys, nullptr, &en, nullptr);
        std::vector<VkExtensionProperties> have(en);
        vkEnumerateDeviceExtensionProperties(gpu.phys, nullptr, &en, have.data());
        auto has = [&](const char* name) {
            return std::any_of(have.begin(), have.end(),
                               [&](const VkExtensionProperties& e) { return std::strcmp(e.extensionName, name) == 0; });
        };
        if (has(VK_KHR_PRESENT_ID_EXTENSION_NAME) && has(VK_KHR_PRESENT_WAIT_EXTENSION_NAME)) {
            pid.pNext = &pw;
            VkPhysicalDeviceFeatures2 q{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
            q.pNext = &pid;
            vkGetPhysicalDeviceFeatures2(gpu.phys, &q);
            present_wait = pid.presentId && pw.presentWait;
        }
        if (present_wait) {
            exts.push_back(VK_KHR_PRESENT_ID_EXTENSION_NAME);
            exts.push_back(VK_KHR_PRESENT_WAIT_EXTENSION_NAME);
            pid.pNext = &pw;
            pw.pNext = nullptr;
            f13.pNext = &pid;
        }
    }
    VkDeviceCreateInfo ci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    ci.pNext = &f2;
    ci.queueCreateInfoCount = 1;
    ci.pQueueCreateInfos = &qi;
    ci.enabledExtensionCount = static_cast<u32>(exts.size());
    ci.ppEnabledExtensionNames = exts.data();
    if (!vk_ok(vkCreateDevice(gpu.phys, &ci, nullptr, &gpu.device), "vkCreateDevice")) return false;
    vkGetDeviceQueue(gpu.device, gpu.queue_family, 0, &gpu.queue);
    if (present_wait)
        wait_for_present = reinterpret_cast<PFN_vkWaitForPresentKHR>(vkGetDeviceProcAddr(gpu.device, "vkWaitForPresentKHR"));
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pi.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    pi.queueFamilyIndex = gpu.queue_family;
    return vk_ok(vkCreateCommandPool(gpu.device, &pi, nullptr, &gpu.transient_pool), "vkCreateCommandPool");
}

bool Renderer::Impl::create_frames() {
    for (Frame& f : frames) {
        VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pi.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pi.queueFamilyIndex = gpu.queue_family;
        if (!vk_ok(vkCreateCommandPool(gpu.device, &pi, nullptr, &f.pool), "vkCreateCommandPool")) return false;
        VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = f.pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 1;
        if (!vk_ok(vkAllocateCommandBuffers(gpu.device, &ai, &f.cmd), "vkAllocateCommandBuffers")) return false;
        VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        if (!vk_ok(vkCreateFence(gpu.device, &fi, nullptr, &f.fence), "vkCreateFence")) return false;
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (!vk_ok(vkCreateSemaphore(gpu.device, &si, nullptr, &f.acquired), "vkCreateSemaphore")) return false;
    }
    return true;
}

bool Renderer::Impl::create_target(VkExtent2D e) {
    render::destroy(gpu, color);
    render::destroy(gpu, depth);
    target.extent = e;
    return create_image(gpu, e.width, e.height, target.color_format,
                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                        VK_IMAGE_ASPECT_COLOR_BIT, color) &&
           create_image(gpu, e.width, e.height, target.depth_format, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                        VK_IMAGE_ASPECT_DEPTH_BIT, depth);
}

bool Renderer::Impl::create_swapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu.phys, surface, &caps);
    extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        int w = 0, h = 0;
        SDL_GetWindowSizeInPixels(window, &w, &h);
        extent.width = std::clamp(static_cast<u32>(w), caps.minImageExtent.width, caps.maxImageExtent.width);
        extent.height = std::clamp(static_cast<u32>(h), caps.minImageExtent.height, caps.maxImageExtent.height);
    }
    if (extent.width == 0 || extent.height == 0) return false;

    u32 fn = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.phys, surface, &fn, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(fn);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu.phys, surface, &fn, formats.data());
    VkSurfaceFormatKHR chosen = formats.empty() ? VkSurfaceFormatKHR{} : formats[0];
    for (const VkSurfaceFormatKHR& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) chosen = f;
    swap_format = chosen.format;

    u32 pn = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.phys, surface, &pn, nullptr);
    std::vector<VkPresentModeKHR> modes(pn);
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu.phys, surface, &pn, modes.data());
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!config.vsync) {
        auto has = [&](VkPresentModeKHR m) { return std::find(modes.begin(), modes.end(), m) != modes.end(); };
        if (has(VK_PRESENT_MODE_MAILBOX_KHR)) mode = VK_PRESENT_MODE_MAILBOX_KHR;
        else if (has(VK_PRESENT_MODE_IMMEDIATE_KHR)) mode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    }
    u32 count = caps.minImageCount + 1;
    if (caps.maxImageCount && count > caps.maxImageCount) count = caps.maxImageCount;
    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & alpha))
        alpha = static_cast<VkCompositeAlphaFlagBitsKHR>(caps.supportedCompositeAlpha & -caps.supportedCompositeAlpha);

    VkSwapchainKHR old = swapchain;
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = surface;
    ci.minImageCount = count;
    ci.imageFormat = chosen.format;
    ci.imageColorSpace = chosen.colorSpace;
    ci.imageExtent = extent;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = caps.currentTransform;
    ci.compositeAlpha = alpha;
    ci.presentMode = mode;
    fifo = mode == VK_PRESENT_MODE_FIFO_KHR;
    ci.clipped = VK_TRUE;
    ci.oldSwapchain = old;
    const bool ok = vk_ok(vkCreateSwapchainKHR(gpu.device, &ci, nullptr, &swapchain), "vkCreateSwapchainKHR");
    if (old) vkDestroySwapchainKHR(gpu.device, old, nullptr);
    if (!ok) {
        swapchain = VK_NULL_HANDLE;
        return false;
    }
    u32 n = 0;
    vkGetSwapchainImagesKHR(gpu.device, swapchain, &n, nullptr);
    images.resize(n);
    vkGetSwapchainImagesKHR(gpu.device, swapchain, &n, images.data());
    render_done.resize(n);
    for (u32 i = 0; i < n; ++i) {
        VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (!vk_ok(vkCreateSemaphore(gpu.device, &si, nullptr, &render_done[i]), "vkCreateSemaphore")) return false;
    }
    if (!create_target(extent)) return false;
    present_id = 0;
    stale = false;
    return true;
}

void Renderer::Impl::destroy_swapchain_images() {
    for (VkSemaphore s : render_done) vkDestroySemaphore(gpu.device, s, nullptr);
    render_done.clear();
    images.clear();
}

bool Renderer::Impl::rebuild_swapchain() {
    vkDeviceWaitIdle(gpu.device);
    destroy_swapchain_images();
    return create_swapchain();
}

void Renderer::Impl::record(VkCommandBuffer cb, const float clear[3], const SceneFn& scene, const PrepFn& prep) {
    if (prep) prep(cb, static_cast<u32>(frame_index % kFramesInFlight));
    image_barrier(cb, color.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    image_barrier(cb, depth.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 0,
                  VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                  VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT);

    VkRenderingAttachmentInfo ca{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    ca.imageView = color.view;
    ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ca.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ca.clearValue.color = {{clear[0], clear[1], clear[2], 1.0f}};
    VkRenderingAttachmentInfo da{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    da.imageView = depth.view;
    da.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    da.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    da.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    da.clearValue.depthStencil = {0.0f, 0};
    VkRenderingInfo ri{VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea = {{0, 0}, target.extent};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &ca;
    ri.pDepthAttachment = &da;
    vkCmdBeginRendering(cb, &ri);
    VkViewport vp{0, 0, static_cast<float>(target.extent.width), static_cast<float>(target.extent.height), 0, 1};
    VkRect2D sc{{0, 0}, target.extent};
    vkCmdSetViewport(cb, 0, 1, &vp);
    vkCmdSetScissor(cb, 0, 1, &sc);
    if (scene) scene(cb, target);
    vkCmdEndRendering(cb);
    image_barrier(cb, color.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_READ_BIT);
}

void Renderer::Impl::destroy() {
    if (gpu.device) {
        vkDeviceWaitIdle(gpu.device);
        render::destroy(gpu, color);
        render::destroy(gpu, depth);
        destroy_swapchain_images();
        if (swapchain) vkDestroySwapchainKHR(gpu.device, swapchain, nullptr);
        for (Frame& f : frames) {
            if (f.acquired) vkDestroySemaphore(gpu.device, f.acquired, nullptr);
            if (f.fence) vkDestroyFence(gpu.device, f.fence, nullptr);
            if (f.pool) vkDestroyCommandPool(gpu.device, f.pool, nullptr);
            f = Frame{};
        }
        if (gpu.transient_pool) vkDestroyCommandPool(gpu.device, gpu.transient_pool, nullptr);
        vkDestroyDevice(gpu.device, nullptr);
    }
    if (surface) SDL_Vulkan_DestroySurface(instance, surface, nullptr);
    if (messenger) {
        auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy_fn) destroy_fn(instance, messenger, nullptr);
    }
    if (instance) vkDestroyInstance(instance, nullptr);
    gpu = Gpu{};
    swapchain = VK_NULL_HANDLE;
    surface = VK_NULL_HANDLE;
    messenger = VK_NULL_HANDLE;
    instance = VK_NULL_HANDLE;
}

Renderer::Renderer() : impl_(std::make_unique<Impl>()) {}
Renderer::~Renderer() { shutdown(); }

bool Renderer::init(SDL_Window* window, const Config& config) {
    Impl& I = *impl_;
    I.window = window;
    I.config = config;
    if (!I.create_instance() || !I.pick_device() || !I.create_device() || !I.create_frames()) {
        I.destroy();
        return false;
    }
    if (!window && !I.create_target({config.headless_width, config.headless_height})) {
        I.destroy();
        return false;
    }
    I.target.extent = window ? VkExtent2D{} : VkExtent2D{config.headless_width, config.headless_height};
    I.stale = window != nullptr;
    return true;
}

void Renderer::shutdown() {
    if (impl_) impl_->destroy();
}

const DeviceInfo& Renderer::device() const { return impl_->info; }
const Gpu& Renderer::gpu() const { return impl_->gpu; }
Target Renderer::target() const { return impl_->target; }
void Renderer::resized() { impl_->stale = true; }

bool Renderer::frame(const float clear[3], const SceneFn& scene, const PrepFn& prep) {
    Impl& I = *impl_;
    Impl::Frame& f = I.frames[I.frame_index % kFramesInFlight];

    if (!I.window) {
        vkWaitForFences(I.gpu.device, 1, &f.fence, VK_TRUE, UINT64_MAX);
        vkResetFences(I.gpu.device, 1, &f.fence);
        vkResetCommandBuffer(f.cmd, 0);
        VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(f.cmd, &bi);
        I.record(f.cmd, clear, scene, prep);
        vkEndCommandBuffer(f.cmd);
        VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
        cbi.commandBuffer = f.cmd;
        VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
        si.commandBufferInfoCount = 1;
        si.pCommandBufferInfos = &cbi;
        if (!vk_ok(vkQueueSubmit2(I.gpu.queue, 1, &si, f.fence), "vkQueueSubmit2")) return false;
        ++I.frame_index;
        I.drew_once = true;
        return true;
    }

    if (I.stale && !I.rebuild_swapchain()) return false;
    if (I.wait_for_present && I.fifo && I.present_id >= 2) {
        const VkResult w = I.wait_for_present(I.gpu.device, I.swapchain, I.present_id - 1, 100'000'000ull);
        if (w == VK_ERROR_OUT_OF_DATE_KHR || w == VK_ERROR_SURFACE_LOST_KHR) I.stale = true;
    }
    vkWaitForFences(I.gpu.device, 1, &f.fence, VK_TRUE, UINT64_MAX);
    u32 image = 0;
    VkResult acq = vkAcquireNextImageKHR(I.gpu.device, I.swapchain, UINT64_MAX, f.acquired, VK_NULL_HANDLE, &image);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) {
        I.stale = true;
        return false;
    }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return vk_ok(acq, "vkAcquireNextImageKHR");
    vkResetFences(I.gpu.device, 1, &f.fence);

    VkCommandBuffer cb = f.cmd;
    vkResetCommandBuffer(cb, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    I.record(cb, clear, scene, prep);

    const VkImage swap = I.images[image];
    image_barrier(cb, swap, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0,
                  VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT);
    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {static_cast<s32>(I.target.extent.width), static_cast<s32>(I.target.extent.height), 1};
    blit.dstOffsets[1] = {static_cast<s32>(I.extent.width), static_cast<s32>(I.extent.height), 1};
    vkCmdBlitImage(cb, I.color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swap,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
    image_barrier(cb, swap, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                  VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_NONE, 0);
    vkEndCommandBuffer(cb);

    VkSemaphoreSubmitInfo wait{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    wait.semaphore = f.acquired;
    wait.stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT | VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSemaphoreSubmitInfo signal{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signal.semaphore = I.render_done[image];
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkCommandBufferSubmitInfo cbi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cbi.commandBuffer = cb;
    VkSubmitInfo2 si{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    si.waitSemaphoreInfoCount = 1;
    si.pWaitSemaphoreInfos = &wait;
    si.commandBufferInfoCount = 1;
    si.pCommandBufferInfos = &cbi;
    si.signalSemaphoreInfoCount = 1;
    si.pSignalSemaphoreInfos = &signal;
    if (!vk_ok(vkQueueSubmit2(I.gpu.queue, 1, &si, f.fence), "vkQueueSubmit2")) return false;

    VkPresentInfoKHR pi{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &I.render_done[image];
    pi.swapchainCount = 1;
    pi.pSwapchains = &I.swapchain;
    pi.pImageIndices = &image;
    const u64 id = I.present_id + 1;
    VkPresentIdKHR pid{VK_STRUCTURE_TYPE_PRESENT_ID_KHR};
    pid.swapchainCount = 1;
    pid.pPresentIds = &id;
    if (I.wait_for_present) pi.pNext = &pid;
    const VkResult pr = vkQueuePresentKHR(I.gpu.queue, &pi);
    if (I.wait_for_present) I.present_id = id;
    ++I.frame_index;
    I.drew_once = true;
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR || acq == VK_SUBOPTIMAL_KHR) I.stale = true;
    else if (pr != VK_SUCCESS) vk_ok(pr, "vkQueuePresentKHR");
    return pr == VK_SUCCESS || pr == VK_SUBOPTIMAL_KHR;
}

bool Renderer::paced() const { return impl_->window && impl_->fifo; }

void Renderer::set_vsync(bool on) {
    if (impl_->config.vsync == on) return;
    impl_->config.vsync = on;
    impl_->stale = true;
}

bool Renderer::draw_clear(float r, float g, float b) {
    const float c[3] = {r, g, b};
    return frame(c, {});
}

bool Renderer::read_back(std::vector<u8>& rgba, u32& width, u32& height) {
    Impl& I = *impl_;
    if (!I.drew_once || !I.color.image) return false;
    vkDeviceWaitIdle(I.gpu.device);
    width = I.target.extent.width;
    height = I.target.extent.height;
    const VkDeviceSize size = static_cast<VkDeviceSize>(width) * height * 4;
    Buffer buf;
    if (!create_buffer(I.gpu, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, true, buf)) return false;
    const bool ok = one_shot(I.gpu, [&](VkCommandBuffer cb) {
        VkBufferImageCopy r{};
        r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        r.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(cb, I.color.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf.buffer, 1, &r);
    });
    if (ok) {
        rgba.resize(size);
        std::memcpy(rgba.data(), buf.mapped, size);
        for (size_t i = 3; i < rgba.size(); i += 4) rgba[i] = 255;
    }
    destroy(I.gpu, buf);
    return ok;
}

bool Renderer::screenshot(const std::string& png_path) {
    std::vector<u8> rgba;
    u32 w = 0, h = 0;
    return read_back(rgba, w, h) && png::write_rgba(png_path, rgba.data(), w, h);
}
}
