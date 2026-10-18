#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::allFinite;
using grains_test::allZero;
using grains_test::noise;
using grains_test::Out;
using grains_test::peak;
using grains_test::render;

namespace {

constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kCeiling = GranularEngine::kOutputCeiling;

bool noSubnormals(const Out& o)
{
    for (std::size_t i = 0; i < o.L.size(); ++i)
        if (std::fpclassify(o.L[i]) == FP_SUBNORMAL || std::fpclassify(o.R[i]) == FP_SUBNORMAL) return false;
    return true;
}

} // namespace

// R-GR-7 grid (rendered for 0.25 s per combination instead of 1 s to keep the Debug run short; the
// worst case, a saturated pool, is reached after about 0.13 s). Size 1 sample at 48 kHz is 1000/48000 ms. The fixed extras (jitters at their
// maximum, gain 2) push every combination as hard as the ranges allow.
TEST_CASE("engine_extremes: parameter grid stays finite and within the ceiling", "[engine_extremes]")
{
    // Mostly DC (0.9) plus a little noise: coherent grains make the output ceiling matter in the grid.
    auto coherent = [](std::uint32_t seed) {
        std::vector<float> v = noise(20000, seed);
        for (float& x : v) x = 0.9f + 0.1f * x;
        return v;
    };
    const SampleBuffer src(coherent(5), coherent(6), 48000.0);
    const float sizes[] = {0.0f, 1000.0f / 48000.0f, 2000.0f, 1.0e9f, kNaN};
    const float pitches[] = {-100.0f, -24.0f, 24.0f, 100.0f, kNaN};
    const float densities[] = {0.0f, 2000.0f, 1.0e9f};
    const float positions[] = {-1.0f, 0.0f, 1.0f, 2.0f};

    int combos = 0;
    int sounding = 0;
    for (float size : sizes)
        for (float pitch : pitches)
            for (float density : densities)
                for (float position : positions) {
                    INFO("size=" << size << " pitch=" << pitch << " density=" << density << " position=" << position);
                    GranularEngine e;
                    e.prepare(48000.0, 17);
                    e.setSource(&src);
                    EngineParams p;
                    p.grainSizeMs = size;
                    p.pitchSemitones = pitch;
                    p.density = density;
                    p.position = position;
                    p.positionJitter = 1.0f;
                    p.pitchJitterSemitones = 12.0f;
                    p.timingJitter = 1.0f;
                    p.panSpread = 1.0f;
                    p.gain = 2.0f;
                    e.setParams(p);
                    const Out o = render(e, 12000, 1000); // 0.25 s: the 128-grain pool saturates after ~0.13 s at density 1000
                    REQUIRE(allFinite(o));
                    REQUIRE(peak(o) <= kCeiling);
                    REQUIRE(noSubnormals(o));
                    if (density == 0.0f) REQUIRE(allZero(o));
                    ++combos;
                    if (!allZero(o)) ++sounding;
                }
    CHECK(combos == 300);
    CHECK(sounding > 150); // most combinations really render grains, so the bounds are meaningful
}

TEST_CASE("engine_extremes: sample rates 22.05 kHz to 192 kHz stay finite and bounded", "[engine_extremes]")
{
    const SampleBuffer src(noise(30000, 9), {}, 44100.0);
    for (double sr : {22050.0, 44100.0, 48000.0, 96000.0, 192000.0}) {
        INFO("sr=" << sr);
        GranularEngine e;
        e.prepare(sr, 3);
        e.setSource(&src);
        EngineParams p;
        p.density = 400.0f;
        p.grainSizeMs = 60.0f;
        p.positionJitter = 1.0f;
        p.pitchJitterSemitones = 12.0f;
        p.panSpread = 1.0f;
        e.setParams(p);
        const Out o = render(e, static_cast<int>(sr / 2.0), 777); // 0.5 s
        REQUIRE(allFinite(o));
        CHECK(peak(o) <= kCeiling);
        CHECK(peak(o) > 0.01f); // grains actually sounded at every rate
    }
}

TEST_CASE("engine_extremes: sub-threshold and subnormal sources never yield denormal output", "[engine_extremes]")
{
    // 1e-25 is a normal float below the flush threshold (1e-20), 1e-40 is subnormal, and the decaying
    // ramp passes through all of those ranges inside one grain.
    std::vector<float> decay(20000);
    for (std::size_t i = 0; i < decay.size(); ++i) decay[i] = std::pow(10.0f, -static_cast<float>(i) * 0.0033f);
    struct Case {
        std::vector<float> data;
        bool silent;
    };
    const Case cases[] = {
        {std::vector<float>(5000, 1.0e-25f), true},
        {std::vector<float>(5000, 1.0e-40f), true},
        {decay, false}, // starts at 1.0, so the early part of each grain is audible
    };
    for (const Case& c : cases) {
        const SampleBuffer src(c.data, {}, 48000.0);
        GranularEngine e;
        e.prepare(48000.0, 4);
        e.setSource(&src);
        EngineParams p;
        p.density = 300.0f;
        p.grainSizeMs = 100.0f;
        p.position = 0.0f;
        e.setParams(p);
        const Out o = render(e, 48000, 480);
        CHECK(noSubnormals(o));
        CHECK(allZero(o) == c.silent);
    }
}

// Coherent DC through a saturated pool is the worst case for the output ceiling: every grain adds
// the same polarity, and a constant-1 source makes the window weight the only variation. The
// output is linear in `gain` until the clamp engages, so the pre-clamp peak is measured at gain
// 0.25 (well below the ceiling) and scaled by 8 (an exact power of two).
TEST_CASE("engine_extremes: worst-case coherent DC engages the ceiling clamp but never exceeds it", "[engine_extremes]")
{
    const SampleBuffer dc(std::vector<float>(200000, 1.0f), {}, 48000.0);
    auto run = [&](float gain) {
        GranularEngine e;
        e.prepare(48000.0, 1);
        e.setSource(&dc);
        EngineParams p;
        p.density = 200.0f; // density * size = 200 > 128 pool slots: the pool saturates
        p.grainSizeMs = 1000.0f;
        p.position = 0.0f;
        p.gain = gain;
        e.setParams(p);
        return render(e, 48000 * 3, 480);
    };

    const Out probe = run(0.25f);
    const double preClampPeak = static_cast<double>(peak(probe)) * 8.0; // what gain 2 would produce without the clamp
    REQUIRE(peak(probe) < kCeiling);                                   // the probe itself is unclamped
    CHECK(preClampPeak > 9.0);   // measured 9.28: the clamp does engage for coherent material at gain 2
    CHECK(preClampPeak < 9.6);

    const Out full = run(2.0f);
    REQUIRE(allFinite(full));
    CHECK(peak(full) == kCeiling); // clipped exactly at the ceiling, never above
    int clipped = 0;
    for (float v : full.L)
        if (std::abs(v) == kCeiling) ++clipped;
    CHECK(clipped > 0);

    // Gain 1 (default) with this material is also above the ceiling (about 4.64), gain 0.5 is not.
    CHECK(preClampPeak / 2.0 > kCeiling);
    CHECK(preClampPeak / 4.0 < kCeiling);
}
