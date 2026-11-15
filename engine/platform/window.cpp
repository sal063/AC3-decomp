#include "platform/window.hpp"

#include "core/log.hpp"

#include <SDL3/SDL.h>

namespace ac3::platform {
std::string pref_path() {
    char* p = SDL_GetPrefPath("AC3Native", "engine");
    if (!p) return {};
    std::string s(p);
    SDL_free(p);
    return s;
}

Window::~Window() { destroy(); }

bool Window::create(const char* title, int width, int height) {
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
        log::error("SDL video init failed: {}", SDL_GetError());
        return false;
    }
    video_ = true;
    const SDL_WindowFlags flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    window_ = SDL_CreateWindow(title, width, height, flags);
    if (!window_) {
        log::error("SDL_CreateWindow failed: {}", SDL_GetError());
        return false;
    }
    return true;
}

void Window::destroy() {
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (video_) SDL_QuitSubSystem(SDL_INIT_VIDEO);
    video_ = false;
}

void Window::set_fullscreen(bool on) {
    if (!window_ || on == fullscreen_) return;
    fullscreen_ = on;
    SDL_SetWindowFullscreen(window_, on);
}

bool Window::pump() {
    bool keep_running = true;
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (event_hook && event_hook(ev)) continue;
        switch (ev.type) {
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            keep_running = false;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (ev.key.key == SDLK_ESCAPE) keep_running = false;
            if (ev.key.key == SDLK_F11 && !ev.key.repeat) set_fullscreen(!fullscreen_);
            break;
        case SDL_EVENT_WINDOW_MINIMIZED:
            minimized_ = true;
            break;
        case SDL_EVENT_WINDOW_RESTORED:
        case SDL_EVENT_WINDOW_MAXIMIZED:
            minimized_ = false;
            resized_ = true;
            break;
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            resized_ = true;
            break;
        case SDL_EVENT_MOUSE_MOTION:
            if (ev.motion.state & SDL_BUTTON_RMASK) {
                look_dx_ += ev.motion.xrel;
                look_dy_ += ev.motion.yrel;
            }
            break;
        default:
            break;
        }
    }
    return keep_running;
}

void Window::wait(int ms) { SDL_WaitEventTimeout(nullptr, ms); }

bool Window::key(int scancode) const {
    if (window_ && SDL_GetKeyboardFocus() != window_) return false;
    int n = 0;
    const bool* k = SDL_GetKeyboardState(&n);
    return k && scancode >= 0 && scancode < n && k[scancode];
}

void Window::take_look(float& dx, float& dy) {
    dx = look_dx_;
    dy = look_dy_;
    look_dx_ = look_dy_ = 0;
}

bool Window::take_resized() {
    const bool r = resized_;
    resized_ = false;
    return r;
}
}
