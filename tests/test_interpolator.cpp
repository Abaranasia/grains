#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/Interpolator.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

using grains::LinearInterpolator;

TEST_CASE("interp: exact at integer positions", "[interp]")
{
    const std::vector<float> d{0.0f, 1.0f, -2.0f, 4.0f, 0.5f};
    for (int i = 0; i < 5; ++i)
        CHECK(LinearInterpolator::read(d.data(), 5, static_cast<double>(i)) == d[static_cast<std::size_t>(i)]);
}

TEST_CASE("interp: midpoints and fractions are linear", "[interp]")
{
    const std::vector<float> d{0.0f, 2.0f, -2.0f, 4.0f};
    CHECK(LinearInterpolator::read(d.data(), 4, 0.5) == Catch::Approx(1.0f));
    CHECK(LinearInterpolator::read(d.data(), 4, 1.5) == Catch::Approx(0.0f).margin(1e-7));
    CHECK(LinearInterpolator::read(d.data(), 4, 2.25) == Catch::Approx(-0.5f));
}

TEST_CASE("interp: reads at the last sample stay in range", "[interp]")
{
    const std::vector<float> d{1.0f, 3.0f, 5.0f};
    CHECK(LinearInterpolator::read(d.data(), 3, 2.0) == 5.0f);
    const float mid = LinearInterpolator::read(d.data(), 3, 1.999);
    CHECK(mid >= 3.0f);
    CHECK(mid <= 5.0f);
    // Past the end and before the start clamp to the edge samples.
    CHECK(LinearInterpolator::read(d.data(), 3, 2.5) == 5.0f);
    CHECK(LinearInterpolator::read(d.data(), 3, 1.0e12) == 5.0f);
    CHECK(LinearInterpolator::read(d.data(), 3, -4.0) == 1.0f);
    CHECK(LinearInterpolator::read(d.data(), 3, std::numeric_limits<double>::quiet_NaN()) == 1.0f);
}

TEST_CASE("interp: no out-of-bounds access under a guarded buffer", "[interp]")
{
    // Sentinels are NaN: any read outside [guard, guard + frames) would poison the result.
    const int guard = 8;
    const int frames = 5;
    const float nan = std::numeric_limits<float>::quiet_NaN();
    std::vector<float> mem(static_cast<std::size_t>(frames + 2 * guard), nan);
    for (int i = 0; i < frames; ++i) mem[static_cast<std::size_t>(guard + i)] = static_cast<float>(i + 1);
    const float* d = mem.data() + guard;
    for (double p = -3.0; p <= 8.0; p += 0.125) {
        const float v = LinearInterpolator::read(d, frames, p);
        REQUIRE(std::isfinite(v));
        CHECK(v >= 1.0f);
        CHECK(v <= 5.0f);
    }
}

TEST_CASE("interp: tiny and empty sources", "[interp]")
{
    const float one = 3.0f;
    CHECK(LinearInterpolator::read(&one, 1, 0.0) == 3.0f);
    CHECK(LinearInterpolator::read(&one, 1, 0.7) == 3.0f);
    CHECK(LinearInterpolator::read(nullptr, 0, 0.0) == 0.0f);
}
