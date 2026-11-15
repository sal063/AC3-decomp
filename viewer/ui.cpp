#include "ui.hpp"

#include "core/log.hpp"

#include <SDL3/SDL.h>

namespace ac3::viewer {
namespace {
constexpr float kScale = 2.0f;
constexpr int kCell = SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE;
constexpr int kLine = kCell + 3;
}

Ui::~Ui() { destroy(); }

bool Ui::create(const char* title, int width, int height, bool hidden) {
    const SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | (hidden ? SDL_WINDOW_HIDDEN : 0);
    if (!SDL_CreateWindowAndRenderer(title, width, height, flags, &window_, &renderer_)) {
        log::error("the viewer's window: {}", SDL_GetError());
        return false;
    }
    id_ = SDL_GetWindowID(window_);
    SDL_SetRenderScale(renderer_, kScale, kScale);
    SDL_SetRenderVSync(renderer_, 0);
    return true;
}

void Ui::destroy() {
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    renderer_ = nullptr;
    window_ = nullptr;
}

bool Ui::handle(const SDL_Event& e) {
    switch (e.type) {
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
        if (e.window.windowID != id_) return false;
        closed_ = true;
        return true;
    case SDL_EVENT_MOUSE_MOTION:
        if (e.motion.windowID != id_) return false;
        mx_ = e.motion.x / kScale;
        my_ = e.motion.y / kScale;
        return true;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e.button.windowID != id_) return false;
        if (e.button.button == SDL_BUTTON_LEFT) {
            click_ = true;
            cx_ = e.button.x / kScale;
            cy_ = e.button.y / kScale;
        }
        return true;
    case SDL_EVENT_MOUSE_BUTTON_UP:
        return e.button.windowID == id_;
    case SDL_EVENT_MOUSE_WHEEL:
        if (e.wheel.windowID != id_) return false;
        wheel_ += e.wheel.y;
        wx_ = e.wheel.mouse_x / kScale;
        wy_ = e.wheel.mouse_y / kScale;
        return true;
    case SDL_EVENT_KEY_DOWN:
        if (e.key.windowID != id_) return false;
        if (nkeys_ < kKeys) keys_[nkeys_++] = e.key.key;
        return true;
    case SDL_EVENT_KEY_UP:
        return e.key.windowID == id_;
    case SDL_EVENT_TEXT_INPUT:
        return e.text.windowID == id_;
    default:
        if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) return e.window.windowID == id_;
        return false;
    }
}

void Ui::begin() {
    int w = 0, h = 0;
    SDL_GetRenderOutputSize(renderer_, &w, &h);
    cols_ = static_cast<int>(w / kScale) / kCell;
    rows_ = static_cast<int>(h / kScale) / kLine;
    SDL_SetRenderDrawColor(renderer_, 18, 20, 24, 255);
    SDL_RenderClear(renderer_);
}

void Ui::end() {
    if (!snapshot.empty()) {
        if (SDL_Surface* s = SDL_RenderReadPixels(renderer_, nullptr)) {
            if (!SDL_SaveBMP(s, snapshot.c_str())) log::warn("cannot save {}: {}", snapshot, SDL_GetError());
            SDL_DestroySurface(s);
        }
        snapshot.clear();
    }
    SDL_RenderPresent(renderer_);
    click_ = false;
    wheel_ = 0;
    nkeys_ = 0;
}

void Ui::text(int col, int row, const std::string& s, Rgb c) {
    if (row < 0 || row >= rows_ || col >= cols_) return;
    std::string clipped = col < 0 ? std::string() : s.substr(0, static_cast<size_t>(cols_ - col));
    if (clipped.empty()) return;
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, 255);
    SDL_RenderDebugText(renderer_, static_cast<float>(col * kCell), static_cast<float>(row * kLine + 2), clipped.c_str());
}

void Ui::fill(int col, int row, int w, int h, Rgb c) {
    SDL_SetRenderDrawColor(renderer_, c.r, c.g, c.b, 255);
    const SDL_FRect r{static_cast<float>(col * kCell), static_cast<float>(row * kLine), static_cast<float>(w * kCell),
                      static_cast<float>(h * kLine)};
    SDL_RenderFillRect(renderer_, &r);
}

bool Ui::inside(int col, int row, int w, int h, float x, float y) const {
    return x >= col * kCell && x < (col + w) * kCell && y >= row * kLine && y < (row + h) * kLine;
}

bool Ui::button(int col, int row, const std::string& label, bool lit) {
    const int w = static_cast<int>(label.size()) + 2;
    const bool hover = inside(col, row, w, 1, mx_, my_);
    fill(col, row, w, 1, lit ? Rgb{110, 70, 20} : hover ? Rgb{60, 66, 80} : Rgb{40, 44, 54});
    text(col + 1, row, label, lit ? kAccent : kText);
    return click_ && inside(col, row, w, 1, cx_, cy_);
}

int Ui::clicked_row(int col, int row, int w, int h, bool* twice) {
    if (twice) *twice = false;
    if (!click_ || !inside(col, row, w, h, cx_, cy_)) return -1;
    const int r = static_cast<int>(cy_ / kLine) - row;
    const u64 now = SDL_GetTicksNS();
    if (twice) *twice = r == last_click_row_ && now - last_click_ns_ < 400000000ull;
    last_click_row_ = r;
    last_click_ns_ = now;
    return r;
}

int Ui::wheel(int col, int row, int w, int h) const {
    if (wheel_ == 0 || !inside(col, row, w, h, wx_, wy_)) return 0;
    return wheel_ > 0 ? 1 : -1;
}

bool Ui::pressed(u32 key) const {
    for (int i = 0; i < nkeys_; ++i)
        if (keys_[i] == key) return true;
    return false;
}
}
