#include <catch2/catch_test_macros.hpp>

#include <grains/Rng.h>

#include <cstdint>

TEST_CASE("rng: same seed gives identical sequence", "[rng]")
{
    grains::Rng a(42), b(42);
    for (int i = 0; i < 1000; ++i)
        REQUIRE(a.nextU32() == b.nextU32());
}

TEST_CASE("rng: different seeds differ", "[rng]")
{
    grains::Rng a(1), b(2);
    int differing = 0;
    for (int i = 0; i < 100; ++i)
        differing += (a.nextU32() != b.nextU32()) ? 1 : 0;
    CHECK(differing > 90);
}

TEST_CASE("rng: uniform01 stays in [0, 1)", "[rng]")
{
    grains::Rng r(7);
    for (int i = 0; i < 100000; ++i) {
        const float u = r.uniform01();
        REQUIRE(u >= 0.0f);
        REQUIRE(u < 1.0f);
    }
}

TEST_CASE("rng: bipolar stays in [-1, 1]", "[rng]")
{
    grains::Rng r(7);
    float lo = 1.0f, hi = -1.0f;
    for (int i = 0; i < 100000; ++i) {
        const float v = r.bipolar();
        REQUIRE(v >= -1.0f);
        REQUIRE(v <= 1.0f);
        lo = v < lo ? v : lo;
        hi = v > hi ? v : hi;
    }
    CHECK(lo < -0.9f);
    CHECK(hi > 0.9f);
}

TEST_CASE("rng: reseed restarts the sequence", "[rng]")
{
    grains::Rng r(99);
    const uint32_t first = r.nextU32();
    const uint32_t second = r.nextU32();
    r.seed(99);
    CHECK(r.nextU32() == first);
    CHECK(r.nextU32() == second);
}

TEST_CASE("rng: matches PCG32 reference vector", "[rng]")
{
    // pcg32 reference demo: seed 42, stream 54 -> 0xa15c02b7 first output.
    grains::Rng r(42);
    CHECK(r.nextU32() == 0xa15c02b7u);
}
