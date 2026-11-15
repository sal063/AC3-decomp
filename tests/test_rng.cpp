#include "test.hpp"

#include "sim/rng.hpp"

using namespace ac3;

AC3_TEST(rng_matches_c_standard_sequence) {
    sim::PsyqRand r(1);
    const s32 expected[] = {16838, 5758, 10113, 17515, 31051, 5627, 23010, 7419, 16212, 4086};
    for (s32 e : expected) CHECK_EQ(r.next(), e);
}

AC3_TEST(rng_zero_seed_start_up_state) {
    sim::PsyqRand r;
    CHECK_EQ(r.state(), 0u);
    CHECK_EQ(r.next(), 0);
    CHECK_EQ(r.state(), 12345u);
}

AC3_TEST(rng_srand_resets_stream) {
    sim::PsyqRand a(1), b;
    a.next();
    a.next();
    b.seed(1);
    a.seed(1);
    for (int i = 0; i < 100; ++i) CHECK_EQ(a.next(), b.next());
}

AC3_TEST(rng_output_range) {
    sim::PsyqRand r(0xDEADBEEFu);
    for (int i = 0; i < 100000; ++i) {
        const s32 v = r.next();
        CHECK(v >= 0 && v <= 0x7FFF);
    }
}
