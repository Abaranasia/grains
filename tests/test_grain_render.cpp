// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/Grain.h>
#include <grains/Interpolator.h>
#include <grains/WindowTable.h>

#include <cmath>
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
