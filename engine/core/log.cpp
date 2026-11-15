#include "core/log.hpp"

#include <cstdio>
#include <mutex>

namespace ac3::log {
namespace {
std::mutex g_mutex;
std::FILE* g_file = nullptr;

const char* tag(Level level) {
    switch (level) {
    case Level::debug: return "debug";
    case Level::info:  return "info ";
    case Level::warn:  return "WARN ";
    case Level::error: return "ERROR";
    }
    return "?    ";
}
}

bool open_file(const char* path) {
    std::lock_guard lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = std::fopen(path, "a");
    return g_file != nullptr;
}

void close_file() {
    std::lock_guard lock(g_mutex);
    if (g_file) std::fclose(g_file);
    g_file = nullptr;
}

void write(Level level, std::string_view message) {
    std::lock_guard lock(g_mutex);
    const int n = static_cast<int>(message.size());
    std::fprintf(stderr, "[%s] %.*s\n", tag(level), n, message.data());
    if (g_file) {
        std::fprintf(g_file, "[%s] %.*s\n", tag(level), n, message.data());
        std::fflush(g_file);
    }
}
}
