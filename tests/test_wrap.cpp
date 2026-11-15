#include "test.hpp"

#include "core/wrap.hpp"

#include <cstdint>

using namespace ac3;

AC3_TEST(wrap_add_sub_overflow) {
    CHECK_EQ(wrap::add(INT32_MAX, 1), INT32_MIN);
    CHECK_EQ(wrap::sub(INT32_MIN, 1), INT32_MAX);
    CHECK_EQ(wrap::neg(INT32_MIN), INT32_MIN);
    CHECK_EQ(wrap::add(-5, 3), -2);
}

AC3_TEST(wrap_mul_keeps_low_word) {
    CHECK_EQ(wrap::mul(0x10000, 0x10000), 0);
    CHECK_EQ(wrap::mul(0x7FFFFFFF, 2), -2);
    CHECK_EQ(wrap::mul(-3, 7), -21);
}

AC3_TEST(wrap_shifts_mask_amount) {
    CHECK_EQ(wrap::shl(1u, 33u), 2u);
    CHECK_EQ(wrap::shr(0x80000000u, 31u), 1u);
    CHECK_EQ(wrap::shr(0x80000000u, 32u), 0x80000000u);
    CHECK_EQ(wrap::sar(-8, 1u), -4);
    CHECK_EQ(wrap::sar(-1, 31u), -1);
    CHECK_EQ(wrap::sar(INT32_MIN, 31u), -1);
}

AC3_TEST(wrap_mult_hi_lo) {
    CHECK((wrap::mult(-2, 3) == wrap::HiLo{0xFFFFFFFFu, 0xFFFFFFFAu}));
    CHECK((wrap::mult(0x40000000, 4) == wrap::HiLo{1u, 0u}));
    CHECK((wrap::multu(0xFFFFFFFFu, 0xFFFFFFFFu) == wrap::HiLo{0xFFFFFFFEu, 1u}));
}

AC3_TEST(wrap_div_defined_cases) {
    CHECK((wrap::div(-7, 2) == wrap::HiLo{static_cast<u32>(-1), static_cast<u32>(-3)}));
    CHECK((wrap::div(7, 0) == wrap::HiLo{7u, 0xFFFFFFFFu}));
    CHECK((wrap::div(-7, 0) == wrap::HiLo{static_cast<u32>(-7), 1u}));
    CHECK((wrap::div(INT32_MIN, -1) == wrap::HiLo{0u, 0x80000000u}));
    CHECK((wrap::divu(5u, 0u) == wrap::HiLo{5u, 0xFFFFFFFFu}));
    CHECK((wrap::divu(0xFFFFFFFFu, 16u) == wrap::HiLo{15u, 0x0FFFFFFFu}));
}

static_assert(wrap::add(INT32_MAX, 1) == INT32_MIN);
static_assert(wrap::div(INT32_MIN, -1).lo == 0x80000000u);
