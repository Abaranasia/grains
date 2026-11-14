// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::Out;
using grains_test::peak;
using grains_test::render;

namespace {

constexpr float kKnee = GranularEngine::kLimiterKnee;
constexpr float kFlush = GranularEngine::kFlushThreshold;

static_assert(kKnee >= 0.7f && kKnee <= 0.9f, "R-GR-10: the knee threshold T must lie in [0.7, 0.9]");

std::uint32_t bitsOf(float f)
{
    std::uint32_t u = 0;
    std::memcpy(&u, &f, sizeof u);
    return u;
}

float fromBits(std::uint32_t u)
{
    float f = 0.0f;
    std::memcpy(&f, &u, sizeof f);
    return f;
}

float limit(float x)
{
    return GranularEngine::softLimit(x);
}

// Walks `x` through `steps` neighbouring floats on each side of `centre`, ascending.
std::vector<float> neighbours(float centre, int steps)
{
    std::vector<float> below;
    float x = centre;
    for (int i = 0; i < steps; ++i) {
        x = std::nextafter(x, -std::numeric_limits<float>::infinity());
        below.push_back(x);
    }
    std::vector<float> v(below.rbegin(), below.rend());
    v.push_back(centre);
    x = centre;
    for (int i = 0; i < steps; ++i) {
        x = std::nextafter(x, std::numeric_limits<float>::infinity());
        v.push_back(x);
    }
    return v;
}

// Checks the R-GR-10 curve properties over an ascending list of inputs. `slack` is the rounding
// allowance added to the input step when bounding the output step (slope <= 1).
// Returns the number of adjacent pairs that were compared, so callers can prove the loop ran.
int checkCurve(const std::vector<float>& xs, double slack)
{
    int pairs = 0;
    float prevY = 0.0f;
    float prevX = 0.0f;
    for (std::size_t i = 0; i < xs.size(); ++i) {
        const float x = xs[i];
        const float y = limit(x);
        INFO("x=" << x << " y=" << y);
        REQUIRE(std::isfinite(y));
        REQUIRE(std::fabs(y) < 1.0f);
        REQUIRE(limit(-x) == -y); // exact oddness
        if (i > 0) {
            REQUIRE(y >= prevY); // monotone non-decreasing
            const double dx = static_cast<double>(x) - static_cast<double>(prevX);
            REQUIRE(static_cast<double>(y) - static_cast<double>(prevY) <= dx + slack);
            ++pairs;
        }
        prevX = x;
        prevY = y;
    }
    return pairs;
}

} // namespace

// R-GR-10 / R-GR-7: the ceiling is C = 1.0. A runtime CHECK (not a static_assert) so a wrong
// constant shows up as a failing test case instead of breaking the whole test build.
TEST_CASE("soft_limiter: the output ceiling is 1.0 and the knee sits inside [0.7, 0.9]", "[soft_limiter]")
{
    CHECK(GranularEngine::kOutputCeiling == 1.0f);
    CHECK(GranularEngine::kLimiterKnee == 0.8f);
    CHECK(GranularEngine::kLimiterKnee < GranularEngine::kOutputCeiling);
}

// Every float in [0.5, T] (about 5M values, both signs) plus a bit-pattern stride from the flush
// threshold up to 0.5 must pass through unchanged.
TEST_CASE("soft_limiter: bit-exact transparency from the flush threshold to the knee", "[soft_limiter]")
{
    std::uint64_t checked = 0;
    std::uint64_t wrong = 0;
    auto visit = [&](float x) {
        ++checked;
        if (limit(x) != x) ++wrong;
        if (limit(-x) != -x) ++wrong;
    };

    // Dense: every float in [0.5, T].
    for (float x = 0.5f; x <= kKnee; x = std::nextafter(x, 1.0f)) visit(x);
    const std::uint64_t dense = checked;
    CHECK(dense > 5000000u); // not a ghost loop: [0.5, 0.8] holds about 5.03M floats

    // Strided: from kFlushThreshold up to 0.5 by bit pattern, step 4099 (coprime with 2^23).
    const std::uint32_t end = bitsOf(0.5f);
    for (std::uint32_t b = bitsOf(kFlush); b < end; b += 4099u) visit(fromBits(b));
    CHECK(checked - dense > 100000u);

    // Edges: +-kFlushThreshold, +-T and the float just below T.
    for (float x : {kFlush, kKnee, std::nextafter(kKnee, 0.0f), std::nextafter(kFlush, 1.0f)}) visit(x);

    CHECK(wrong == 0u);
    // x == 0 is transparent bit-exactly, so the sign of zero survives.
    CHECK(bitsOf(limit(0.0f)) == bitsOf(0.0f));
    CHECK(bitsOf(limit(-0.0f)) == bitsOf(-0.0f));
}

TEST_CASE("soft_limiter: inputs below the flush threshold give exactly zero", "[soft_limiter]")
{
    for (float x : {std::nextafter(kFlush, 0.0f), 1.0e-25f, 1.0e-30f, 1.0e-38f, 1.0e-40f}) {
        INFO("x=" << x);
        CHECK(limit(x) == 0.0f);
        CHECK(limit(-x) == 0.0f);
    }
    // Control: the first value at or above the threshold is not flushed.
    CHECK(limit(kFlush) == kFlush);
}

// Dense sweep [-64, 64] in steps of 2^-10 plus neighbouring floats around +-T and +-1.
TEST_CASE("soft_limiter: curve is monotone, odd, below 1 and has slope at most 1", "[soft_limiter]")
{
    constexpr float h = 1.0f / 1024.0f;
    std::vector<float> sweep;
    for (int i = -64 * 1024; i <= 64 * 1024; ++i) sweep.push_back(static_cast<float>(i) * h);
    const int sweepPairs = checkCurve(sweep, 1.0 / 4194304.0); // 2^-22
    CHECK(sweepPairs == 131072);

    for (float centre : {kKnee, -kKnee, 1.0f, -1.0f}) {
        INFO("centre=" << centre);
        const int pairs = checkCurve(neighbours(centre, 1000), 1.0 / 4194304.0);
        CHECK(pairs == 2000);
    }

    // The curve really approaches the ceiling (it is not merely a flat attenuation).
    CHECK(limit(8.0f) > 0.99f);
    CHECK(limit(64.0f) > 0.999f);
    CHECK(limit(64.0f) < 1.0f);
    CHECK(limit(1.0f) < 1.0f);
}

TEST_CASE("soft_limiter: NaN, infinities and extreme magnitudes stay finite and below 1", "[soft_limiter]")
{
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    constexpr float inf = std::numeric_limits<float>::infinity();

    for (float n : {nan, -nan}) {
        const float y = limit(n);
        CHECK(y == 0.0f);
        CHECK_FALSE(std::signbit(y)); // +0, whatever the NaN sign
    }

    const float top = limit(64.0f);
    REQUIRE(top < 1.0f);
    REQUIRE(top > 0.999f);
    for (float big : {inf, FLT_MAX}) {
        INFO("big=" << big);
        CHECK(limit(big) == top);
        CHECK(limit(-big) == -top);
    }
}

TEST_CASE("soft_limiter: subnormal inputs of both signs give zero", "[soft_limiter]")
{
    const float subnormals[] = {std::numeric_limits<float>::denorm_min(), FLT_MIN / 2.0f, FLT_MIN / 1024.0f, 1.0e-40f};
    for (float x : subnormals) {
        INFO("x=" << x);
        CHECK(limit(x) == 0.0f);
        CHECK(limit(-x) == 0.0f);
    }
    // Control: the smallest normal float is still below the flush threshold, so it is flushed too.
    CHECK(limit(FLT_MIN) == 0.0f);
}

// R-GR-6 with the limiter engaged: a coherent DC source at gain 2 drives the sum far above the
// knee, and the output must still not depend on how the stream is cut into blocks.
TEST_CASE("soft_limiter: block-split invariance holds above the knee", "[soft_limiter]")
{
    const SampleBuffer dc(std::vector<float>(200000, 1.0f), {}, 48000.0);
    auto run = [&](int block) {
        GranularEngine e;
        e.prepare(48000.0, 1);
        e.setSource(&dc);
        EngineParams p;
        p.density = 200.0f;
        p.grainSizeMs = 1000.0f;
        p.position = 0.0f;
        p.gain = 2.0f;
        e.setParams(p);
        return render(e, 48000, block);
    };

    const Out whole = run(0);
    REQUIRE(peak(whole) > kKnee); // the limiter's non-linear region was reached
    const Out b512 = run(512);
    const Out b37 = run(37);
    CHECK(b512.L == whole.L);
    CHECK(b512.R == whole.R);
    CHECK(b37.L == whole.L);
    CHECK(b37.R == whole.R);
}
