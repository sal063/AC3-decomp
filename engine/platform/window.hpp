#pragma once

#include <functional>
#include <string>

struct SDL_Window;
union SDL_Event;

namespace ac3::platform {
std::string pref_path();

class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const char* title, int width, int height);
    void destroy();

    SDL_Window* handle() const { return window_; }

    bool pump();
    std::function<bool(const SDL_Event&)> event_hook;

    void wait(int ms);

    bool minimized() const { return minimized_; }
    bool fullscreen() const { return fullscreen_; }
    void set_fullscreen(bool on);
    bool take_resized();

    bool key(int scancode) const;
    void take_look(float& dx, float& dy);

private:
    SDL_Window* window_ = nullptr;
    bool video_ = false;
    bool minimized_ = false;
    bool resized_ = false;
    bool fullscreen_ = false;
    float look_dx_ = 0, look_dy_ = 0;
};
}
