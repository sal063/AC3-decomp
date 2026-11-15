#pragma once

#include "core/types.hpp"

#include <string>

struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;

namespace ac3::viewer {
struct Rgb {
    u8 r, g, b;
};
inline constexpr Rgb kText{220, 220, 220}, kDim{130, 130, 130}, kAccent{255, 170, 60}, kGood{120, 220, 120},
    kBad{240, 100, 90}, kHead{120, 180, 255};

class Ui {
public:
    ~Ui();
    bool create(const char* title, int width, int height, bool hidden = false);
    void destroy();
    bool handle(const SDL_Event& e);
    bool closed() const { return closed_; }

    void begin();
    void end();
    int cols() const { return cols_; }
    int rows() const { return rows_; }

    void text(int col, int row, const std::string& s, Rgb c = kText);
    void fill(int col, int row, int w, int h, Rgb c);
    bool button(int col, int row, const std::string& label, bool lit = false);
    int clicked_row(int col, int row, int w, int h, bool* twice = nullptr);
    int wheel(int col, int row, int w, int h) const;
    bool pressed(u32 key) const;
    std::string snapshot;

private:
    bool inside(int col, int row, int w, int h, float x, float y) const;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    u32 id_ = 0;
    bool closed_ = false;
    int cols_ = 0, rows_ = 0;
    float mx_ = -1, my_ = -1;
    bool click_ = false;
    float cx_ = 0, cy_ = 0;
    float wheel_ = 0, wx_ = 0, wy_ = 0;
    u64 last_click_ns_ = 0;
    int last_click_row_ = -1;
    static constexpr int kKeys = 16;
    u32 keys_[kKeys] = {};
    int nkeys_ = 0;
};
}
