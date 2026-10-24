// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/ParamLimits.h>

#include <cmath>
#include <limits>

using namespace grains::limits;

namespace {
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();
} // namespace

TEST_CASE("params_limits: density clamps and zero means off", "[params_limits]")
{
    CHECK(sanitizeDensity(20.0f) == 20.0f);
    CHECK(sanitizeDensity(0.0f) == 0.0f);
    CHECK(sanitizeDensity(-5.0f) == 0.0f);
    CHECK(sanitizeDensity(0.05f) == Catch::Approx(kDensityMin));
    CHECK(sanitizeDensity(1.0e9f) == kDensityMax);
    CHECK(sanitizeDensity(kInf) == kDensityMax);
    CHECK(sanitizeDensity(kNaN) == kDensityDefault);
    CHECK(kDensityDefault == 20.0f);
    CHECK(kDensityMax == 1000.0f);
}

TEST_CASE("params_limits: grain size clamps to 1-1000 ms", "[params_limits]")
{
    CHECK(sanitizeSizeMs(80.0f) == 80.0f);
    CHECK(sanitizeSizeMs(0.0f) == 1.0f);
    CHECK(sanitizeSizeMs(-3.0f) == 1.0f);
    CHECK(sanitizeSizeMs(5000.0f) == 1000.0f);
    CHECK(sanitizeSizeMs(kNaN) == kSizeMsDefault);
}

TEST_CASE("params_limits: unit-range parameters clamp to 0-1", "[params_limits]")
{
    CHECK(sanitizeUnit(0.3f, 0.5f) == 0.3f);
    CHECK(sanitizeUnit(-1.0f, 0.5f) == 0.0f);
    CHECK(sanitizeUnit(7.0f, 0.5f) == 1.0f);
    CHECK(sanitizeUnit(kNaN, 0.5f) == 0.5f);
    CHECK(sanitizeUnit(kNaN, 0.0f) == 0.0f);
}

TEST_CASE("params_limits: pitch, pitch jitter and gain ranges", "[params_limits]")
{
    CHECK(sanitizePitchSemitones(12.0f) == 12.0f);
    CHECK(sanitizePitchSemitones(100.0f) == 24.0f);
    CHECK(sanitizePitchSemitones(-100.0f) == -24.0f);
    CHECK(sanitizePitchSemitones(kNaN) == 0.0f);

    CHECK(sanitizePitchJitterSemitones(5.0f) == 5.0f);
    CHECK(sanitizePitchJitterSemitones(-1.0f) == 0.0f);
    CHECK(sanitizePitchJitterSemitones(50.0f) == 12.0f);
    CHECK(sanitizePitchJitterSemitones(kNaN) == 0.0f);

    CHECK(sanitizeGain(1.0f) == 1.0f);
    CHECK(sanitizeGain(-1.0f) == 0.0f);
    CHECK(sanitizeGain(9.0f) == 2.0f);
    CHECK(sanitizeGain(kNaN) == 1.0f);
}

TEST_CASE("params_limits: sample rate clamps to 8 kHz - 384 kHz", "[params_limits]")
{
    CHECK(sanitizeSampleRate(48000.0) == 48000.0);
    CHECK(sanitizeSampleRate(100.0) == 8000.0);
    CHECK(sanitizeSampleRate(-1.0) == 8000.0);
    CHECK(sanitizeSampleRate(1.0e7) == 384000.0);
    CHECK(sanitizeSampleRate(std::numeric_limits<double>::quiet_NaN()) == 48000.0);
}
