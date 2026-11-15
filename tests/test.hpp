#pragma once

#include <format>
#include <string>
#include <vector>

namespace ac3::test {
struct Case {
    const char* name;
    void (*fn)();
};

std::vector<Case>& registry();
void fail(const char* file, int line, const char* expr, const std::string& detail);

struct Registrar {
    Registrar(const char* name, void (*fn)()) { registry().push_back({name, fn}); }
};
}

#define AC3_TEST(name)                                                \
    static void name();                                               \
    static ::ac3::test::Registrar name##_registrar(#name, name);      \
    static void name()

#define CHECK(expr)                                                   \
    do {                                                              \
        if (!(expr)) ::ac3::test::fail(__FILE__, __LINE__, #expr, {}); \
    } while (0)

#define CHECK_EQ(actual, expected)                                                        \
    do {                                                                                  \
        const auto ac3_a_ = (actual);                                                     \
        const auto ac3_e_ = (expected);                                                   \
        if (!(ac3_a_ == ac3_e_))                                                          \
            ::ac3::test::fail(__FILE__, __LINE__, #actual " == " #expected,               \
                              std::format("got {}, expected {}", ac3_a_, ac3_e_));        \
    } while (0)
