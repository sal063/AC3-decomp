#include "test.hpp"

#include "sim/game/game.hpp"

#include <cstdlib>

using namespace ac3;

AC3_TEST(stick_modern_corners_are_full_on_both_axes) {
    s32 a = 0, b = 0;
    sim::Game::stick_modern(221, 40, a, b);
    CHECK(a >= 4000);
    CHECK(b <= -3700);
    sim::Game::stick_modern(40, 40, a, b);
    CHECK(a <= -3900);
    CHECK(b <= -3900);
}

AC3_TEST(stick_modern_dead_zone_is_small_and_round) {
    s32 a = 0, b = 0;
    sim::Game::stick_modern(128, 128, a, b);
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 0);
    sim::Game::stick_modern(136, 134, a, b);
    CHECK_EQ(a, 0);
    CHECK_EQ(b, 0);
    sim::Game::stick_modern(128 + 40, 128, a, b);
    CHECK(a > 600 && a < 1600);
    CHECK_EQ(b, 0);
}

AC3_TEST(stick_modern_straight_is_full_and_signed) {
    s32 a = 0, b = 0;
    sim::Game::stick_modern(255, 128, a, b);
    CHECK_EQ(a, 4095);
    CHECK_EQ(b, 0);
    sim::Game::stick_modern(128, 0, a, b);
    CHECK_EQ(a, 0);
    CHECK_EQ(b, -4096);
}
