// SPDX-License-Identifier: AGPL-3.0-or-later
// R-WD-2: pixel <-> normalised position mapping and the jitter band.
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/WaveformGeometry.h>

#include <cmath>
#include <limits>

using Catch::Approx;
using grains::Band;
using grains::jitterBand;
using grains::positionToX;
using grains::xToPosition;

TEST_CASE("geometry: x to position and back", "[waveform]")
{
    CHECK(xToPosition(100.0f, 400.0f) == Approx(0.25f));
    CHECK(positionToX(xToPosition(100.0f, 400.0f), 400.0f) == Approx(100.0f));
    CHECK(xToPosition(300.0f, 400.0f) == Approx(0.75f));
    for (const float p : {0.0f, 0.1f, 0.5f, 0.999f, 1.0f})
        CHECK(xToPosition(positionToX(p, 333.0f), 333.0f) == Approx(p).margin(1e-6));
}

TEST_CASE("geometry: clamps outside the view", "[waveform]")
{
    CHECK(xToPosition(-20.0f, 400.0f) == 0.0f);
    CHECK(xToPosition(500.0f, 400.0f) == 1.0f);
    CHECK(positionToX(-0.5f, 400.0f) == 0.0f);
    CHECK(positionToX(1.5f, 400.0f) == 400.0f);
}

TEST_CASE("geometry: zero or negative width does not divide by zero", "[waveform]")
{
    CHECK(xToPosition(10.0f, 0.0f) == 0.0f);
    CHECK(positionToX(0.5f, 0.0f) == 0.0f);
    CHECK(xToPosition(10.0f, -5.0f) == 0.0f);
    CHECK(positionToX(0.5f, -5.0f) == 0.0f);
}

TEST_CASE("geometry: jitter band is clamped to [0, 1]", "[waveform]")
{
    const Band low = jitterBand(0.1f, 0.3f);
    CHECK(low.lo == Approx(0.0f));
    CHECK(low.hi == Approx(0.4f));
    const Band high = jitterBand(0.9f, 0.3f);
    CHECK(high.lo == Approx(0.6f));
    CHECK(high.hi == Approx(1.0f));
    const Band mid = jitterBand(0.5f, 0.1f);
    CHECK(mid.lo == Approx(0.4f));
    CHECK(mid.hi == Approx(0.6f));
}

TEST_CASE("geometry: non-finite and out-of-range input stays finite in [0, 1]", "[waveform]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float inf = std::numeric_limits<float>::infinity();
    CHECK(xToPosition(nan, 400.0f) == 0.0f);
    CHECK(xToPosition(10.0f, nan) == 0.0f);
    CHECK(positionToX(nan, 400.0f) == 0.0f);
    CHECK(positionToX(0.5f, inf) == 0.0f);

    for (const Band b : {jitterBand(nan, 0.2f), jitterBand(0.5f, nan), jitterBand(inf, -inf), jitterBand(-3.0f, 9.0f)}) {
        CHECK(std::isfinite(b.lo));
        CHECK(std::isfinite(b.hi));
        CHECK(b.lo >= 0.0f);
        CHECK(b.hi <= 1.0f);
        CHECK(b.lo <= b.hi);
    }
    // NaN is treated as 0: a NaN position with jitter 0.2 gives [0, 0.2]; a NaN jitter gives a point.
    CHECK(jitterBand(nan, 0.2f).lo == 0.0f);
    CHECK(jitterBand(nan, 0.2f).hi == Approx(0.2f));
    CHECK(jitterBand(0.5f, nan).lo == Approx(0.5f));
    CHECK(jitterBand(0.5f, nan).hi == Approx(0.5f));
    // A negative jitter is clamped to 0 so lo <= hi still holds.
    CHECK(jitterBand(0.5f, -0.2f).lo == Approx(0.5f));
    CHECK(jitterBand(0.5f, -0.2f).hi == Approx(0.5f));
}
