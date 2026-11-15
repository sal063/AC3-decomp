#include "test.hpp"

#include <cstdio>
#include <cstring>

namespace ac3::test {
namespace {
int g_failures = 0;
}

std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

void fail(const char* file, int line, const char* expr, const std::string& detail) {
    ++g_failures;
    std::fprintf(stderr, "  FAIL %s:%d  %s%s%s\n", file, line, expr, detail.empty() ? "" : "  -- ",
                 detail.c_str());
}
}

int main(int argc, char** argv) {
    using namespace ac3::test;
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int run = 0, failed_cases = 0;
    for (const Case& c : registry()) {
        if (filter && !std::strstr(c.name, filter)) continue;
        const int before = g_failures;
        c.fn();
        ++run;
        const bool ok = g_failures == before;
        if (!ok) ++failed_cases;
        std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", c.name);
    }
    std::printf("%d tests, %d failed\n", run, failed_cases);
    return failed_cases == 0 && run > 0 ? 0 : 1;
}
