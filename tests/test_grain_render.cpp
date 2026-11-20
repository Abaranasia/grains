// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/Grain.h>
#include <grains/GranularEngine.h>
#include <grains/Interpolator.h>
#include <grains/SampleBuffer.h>
#include <grains/SampleBuilder.h>
#include <grains/WindowTable.h>

#include "support/EngineHelpers.h"

#include <cmath>
#include <cstring>
#include <functional>
#include <vector>

using grains::Grain;
using grains::LinearInterpolator;
using grains::WindowTable;

namespace {

std::vector<float> ramp(int frames)
{
    std::vector<float> r(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) r[static_cast<std::size_t>(i)] = static_cast<float>(i);
    return r;
}

Grain makeGrain(double start, double rate, int length)
{
    Grain g;
    g.active = true;
    g.pos = start;
    g.rate = rate;
    g.length = length;
    g.gainL = 1.0f;
    g.gainR = 1.0f;
    return g;
}

} // namespace

TEST_CASE("grain_render: unity pitch equals window times source", "[grain_render]")
{
    const WindowTable win;
    const auto src = ramp(4000);
    Grain g = makeGrain(1000.0, 1.0, 256);
    std::vector<float> L(256, 0.0f), R(256, 0.0f);
    const int n = grains::renderGrain<LinearInterpolator>(g, src.data(), src.data(), 4000, win, L.data(), R.data(), 256);
    REQUIRE(n == 256);
    for (int i = 0; i < 256; ++i) {
        const float expected = win.at(static_cast<float>(i) / 256.0f) * src[static_cast<std::size_t>(1000 + i)];
        REQUIRE(L[static_cast<std::size_t>(i)] == Catch::Approx(expected).margin(1e-6).epsilon(1e-6));
        REQUIRE(R[static_cast<std::size_t>(i)] == L[static_cast<std::size_t>(i)]);
    }
    CHECK(g.age == 256);
}

TEST_CASE("grain_render: +12 st reads every second source sample", "[grain_render]")
{
    const WindowTable win;
    const auto src = ramp(4000);
    Grain g = makeGrain(1000.0, 2.0, 256); // rate for +12 st
    std::vector<float> L(256, 0.0f), R(256, 0.0f);
    grains::renderGrain<LinearInterpolator>(g, src.data(), src.data(), 4000, win, L.data(), R.data(), 256);
    for (int i = 0; i < 256; ++i) {
        const float expected = win.at(static_cast<float>(i) / 256.0f) * static_cast<float>(1000 + 2 * i);
        REQUIRE(L[static_cast<std::size_t>(i)] == Catch::Approx(expected).margin(1e-5).epsilon(1e-6));
    }
}

TEST_CASE("grain_render: pitch changes the consumed span, not the start", "[grain_render]")
{
    const WindowTable win;
    const auto src = ramp(8000);
    for (double rate : {0.5, 1.0, 2.0}) {
        Grain g = makeGrain(1000.0, rate, 256);
        // Sample 0 has window 0 and reads the start position; after two samples pos advanced 2*rate.
        std::vector<float> L2(2, 0.0f), R2(2, 0.0f);
        grains::renderGrain<LinearInterpolator>(g, src.data(), src.data(), 8000, win, L2.data(), R2.data(), 2);
        CHECK(L2[0] == 0.0f); // first sample always starts at the start position, windowed to 0
        CHECK(g.pos == Catch::Approx(1000.0 + 2.0 * rate));
    }
}

TEST_CASE("grain_render: renders in segments and respects remaining length", "[grain_render]")
{
    const WindowTable win;
    const auto src = ramp(4000);
    Grain whole = makeGrain(100.0, 1.5, 64);
    Grain split = whole;
    std::vector<float> a(64, 0.0f), aR(64, 0.0f), b(64, 0.0f), bR(64, 0.0f);
    grains::renderGrain<LinearInterpolator>(whole, src.data(), src.data(), 4000, win, a.data(), aR.data(), 64);
    int done = 0;
    for (int chunk : {5, 1, 30, 100}) {
        const int n = grains::renderGrain<LinearInterpolator>(split, src.data(), src.data(), 4000, win,
                                                              b.data() + done, bR.data() + done, chunk);
        done += n;
    }
    CHECK(done == 64);
    CHECK(a == b);
    CHECK(grains::renderGrain<LinearInterpolator>(split, src.data(), src.data(), 4000, win, b.data(), bR.data(), 8) == 0);
}

TEST_CASE("grain_render: stereo source uses its own channels and per-channel gains", "[grain_render]")
{
    const WindowTable win;
    std::vector<float> left(512, 1.0f), right(512, -0.5f);
    Grain g = makeGrain(10.0, 1.0, 64);
    g.gainL = 0.5f;
    g.gainR = 2.0f;
    std::vector<float> L(64, 0.0f), R(64, 0.0f);
    grains::renderGrain<LinearInterpolator>(g, left.data(), right.data(), 512, win, L.data(), R.data(), 64);
    const float w = win.at(32.0f / 64.0f);
    CHECK(L[32] == Catch::Approx(0.5f * w));
    CHECK(R[32] == Catch::Approx(-1.0f * w));
}

namespace {

constexpr double kEngineRate = 48000.0;

std::vector<float> tone1k(std::size_t frames, double rate)
{
    std::vector<float> v(frames);
    for (std::size_t i = 0; i < frames; ++i)
        v[i] = static_cast<float>(std::sin(2.0 * grains_test::kPi * 1000.0 * static_cast<double>(i) / rate));
    return v;
}

grains::EngineParams steadyParams()
{
    grains::EngineParams p;
    p.density = 50.0f;
    p.grainSizeMs = 100.0f;
    p.position = 0.5f;
    p.gain = 1.0f;
    return p;
}

grains_test::Out renderSource(const grains::SampleBuffer& src, int frames)
{
    grains::GranularEngine e;
    e.prepare(kEngineRate, 4242);
    e.setSource(&src);
    e.setParams(steadyParams());
    return grains_test::render(e, frames, 512);
}

// Frequency of the strongest DFT bin on a 5 Hz grid in [500, 2500] Hz, over out[begin, end).
double dominantHz(const std::vector<float>& out, std::size_t begin, std::size_t end)
{
    double bestHz = 0.0, bestMag = -1.0;
    for (double hz = 500.0; hz <= 2500.0; hz += 5.0) {
        double re = 0.0, im = 0.0;
        for (std::size_t i = begin; i < end; ++i) {
            const double w = 2.0 * grains_test::kPi * hz * static_cast<double>(i) / kEngineRate;
            re += static_cast<double>(out[i]) * std::cos(w);
            im += static_cast<double>(out[i]) * std::sin(w);
        }
        const double mag = re * re + im * im;
        if (mag > bestMag) {
            bestMag = mag;
            bestHz = hz;
        }
    }
    return bestHz;
}

} // namespace

TEST_CASE("grain_render: a mismatched source rate keeps pitch and duration", "[grain_render]")
{
    const std::vector<float> at96 = tone1k(96000, 96000.0);
    const std::vector<float> at48 = tone1k(48000, 48000.0);
    const int frames = 48000;

    // Reference: authored at the engine rate.
    const grains::SampleBuffer native(std::vector<float>(at48), {}, 48000.0);
    const auto refOut = renderSource(native, frames);
    REQUIRE(grains_test::peak(refOut) > 0.1f); // grains were produced
    const double refHz = dominantHz(refOut.L, 12000, 36000);
    CHECK(refHz == Catch::Approx(1000.0).epsilon(0.01));

    // Mismatched: the same tone authored at 96 kHz still plays at 1 kHz (read-rate compensation).
    const grains::SampleBuffer wide(std::vector<float>(at96), {}, 96000.0);
    const auto wideOut = renderSource(wide, frames);
    REQUIRE(grains_test::peak(wideOut) > 0.1f);
    CHECK(dominantHz(wideOut.L, 12000, 36000) == Catch::Approx(1000.0).epsilon(0.01));

    // The loader path: the 96 kHz file resampled to 48 kHz by the builder (base rate 1) also plays at 1 kHz.
    const grains::ChannelView view[] = {{at96.data(), at96.size()}};
    auto built = grains::buildSampleBuffer(view, 1, 96000.0, kEngineRate, [] { return false; });
    REQUIRE(built.buffer != nullptr);
    REQUIRE(built.buffer->sampleRate() == kEngineRate);
    const auto builtOut = renderSource(*built.buffer, frames);
    REQUIRE(grains_test::peak(builtOut) > 0.1f);
    CHECK(dominantHz(builtOut.L, 12000, 36000) == Catch::Approx(1000.0).epsilon(0.01));
}

TEST_CASE("grain_render: a matched source rate leaves the render bit-identical", "[grain_render]")
{
    const auto samples = grains_test::noise(48000, 21);
    const int frames = 24000;

    const grains::SampleBuffer plain(std::vector<float>(samples), {}, 48000.0);
    const auto base = renderSource(plain, frames);
    REQUIRE(grains_test::peak(base) > 0.01f);

    // Equal rates pass through the builder untouched, so the engine output must not change.
    const grains::ChannelView view[] = {{samples.data(), samples.size()}};
    auto built = grains::buildSampleBuffer(view, 1, 48000.0, 48000.0, [] { return false; });
    REQUIRE(built.buffer != nullptr);
    const auto viaBuilder = renderSource(*built.buffer, frames);
    CHECK(viaBuilder.L == base.L);
    CHECK(viaBuilder.R == base.R);

    // Negative control: the same samples tagged with another rate do change the render, so the
    // equality above is not an artefact of the rate being ignored.
    const grains::SampleBuffer tagged(std::vector<float>(samples), {}, 96000.0);
    CHECK_FALSE(renderSource(tagged, frames).L == base.L);
}
