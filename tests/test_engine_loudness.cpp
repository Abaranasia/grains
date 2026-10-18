#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::allZero;
using grains_test::kPi;
using grains_test::noise;
using grains_test::Out;
using grains_test::render;

namespace {

constexpr double kSr = 48000.0;

// RMS over both channels, skipping the first 0.5 s (cloud ramp-up).
double steadyRms(const SampleBuffer& src, float density, std::uint64_t seed)
{
    GranularEngine e;
    e.prepare(kSr, seed);
    e.setSource(&src);
    EngineParams p;
    p.density = density;
    p.grainSizeMs = 50.0f;
    p.positionJitter = 1.0f;
    p.panSpread = 0.5f;
    e.setParams(p);
    const Out o = render(e, 5 * 48000, 1024); // 5 s
    const std::size_t skip = 24000;
    double acc = 0.0;
    for (std::size_t i = skip; i < o.L.size(); ++i)
        acc += static_cast<double>(o.L[i]) * o.L[i] + static_cast<double>(o.R[i]) * o.R[i];
    return std::sqrt(acc / (2.0 * static_cast<double>(o.L.size() - skip)));
}

} // namespace

// R-GR-5: gain = 1/sqrt(max(1, density * size)) keeps an incoherent cloud's power constant.
TEST_CASE("engine_loudness: dense and sparse clouds are within 3 dB for noise and sine sources", "[engine_loudness]")
{
    std::vector<float> sine(48000);
    for (std::size_t i = 0; i < sine.size(); ++i)
        sine[i] = static_cast<float>(std::sin(2.0 * kPi * 440.0 * static_cast<double>(i) / kSr));
    const SampleBuffer sources[] = {SampleBuffer(noise(48000, 21), {}, kSr), SampleBuffer(sine, {}, kSr)};
    for (const SampleBuffer& src : sources)
        for (std::uint64_t seed : {4u, 5u}) {
            const double sparse = steadyRms(src, 20.0f, seed);
            const double dense = steadyRms(src, 400.0f, seed);
            REQUIRE(sparse > 0.01); // grains actually sounded
            REQUIRE(dense > 0.01);
            CHECK(std::abs(20.0 * std::log10(dense / sparse)) <= 3.0);
        }
}

// Amended R-GR-5: gain is fixed per grain at spawn, so a density sweep cannot cause a step from
// active-count changes. With a constant source the only per-sample change is the window slope:
// each active grain moves by at most gain * cos(pi/4) * pi/len per sample. Density times size
// gives at most 20 + 2 overlapping grains here and the per-grain norm is <= 1, hence the bound.
TEST_CASE("engine_loudness: a density sweep produces no step above the window slope bound", "[engine_loudness]")
{
    const SampleBuffer dc(std::vector<float>(60000, 1.0f), {}, kSr);
    const int len = static_cast<int>(0.050 * kSr);
    const double maxGrains = 400.0 * 0.050 + 2.0;
    const double bound = maxGrains * std::cos(kPi / 4.0) * kPi / len + 1e-4;

    for (bool upward : {true, false}) {
        GranularEngine e;
        e.prepare(kSr, 8);
        e.setSource(&dc);
        EngineParams p;
        p.grainSizeMs = 50.0f;
        p.position = 0.0f;
        const int block = 480, blocks = 200; // 2 s, density re-set every block
        std::vector<float> L(static_cast<std::size_t>(block * blocks)), R = L;
        for (int b = 0; b < blocks; ++b) {
            const double t = static_cast<double>(upward ? b : blocks - 1 - b) / (blocks - 1);
            p.density = static_cast<float>(20.0 * std::pow(20.0, t)); // 20 -> 400 gr/s, geometric
            e.setParams(p);
            e.process(L.data() + b * block, R.data() + b * block, block);
        }
        double maxStep = std::abs(static_cast<double>(L[0]));
        float maxLevel = 0.0f;
        for (std::size_t i = 1; i < L.size(); ++i) {
            maxStep = std::max(maxStep, std::abs(static_cast<double>(L[i] - L[i - 1])));
            maxLevel = std::max(maxLevel, L[i]);
        }
        INFO("upward=" << upward << " maxStep=" << maxStep << " bound=" << bound);
        REQUIRE(maxLevel > 0.5f); // the sweep produced a real signal, not near-silence
        CHECK(maxStep <= bound);
    }
}
