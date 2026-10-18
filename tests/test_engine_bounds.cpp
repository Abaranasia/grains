#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;

namespace {

constexpr double kPi = 3.14159265358979323846;

struct Out {
    std::vector<float> L, R;
};

// Pre-fills the output with 7.0 so the test also proves that process() overwrites.
Out render(GranularEngine& e, int n, int block = 0)
{
    Out o{std::vector<float>(static_cast<std::size_t>(n), 7.0f), std::vector<float>(static_cast<std::size_t>(n), 7.0f)};
    if (block <= 0) block = n;
    for (int done = 0; done < n; done += block) {
        const int b = std::min(block, n - done);
        e.process(o.L.data() + done, o.R.data() + done, b);
    }
    return o;
}

std::vector<float> noise(int frames, std::uint32_t seed)
{
    std::vector<float> v(static_cast<std::size_t>(frames));
    std::uint32_t s = seed;
    for (float& x : v) {
        s = s * 1664525u + 1013904223u;
        x = static_cast<float>(static_cast<double>(s >> 8) / 8388608.0 - 1.0);
    }
    return v;
}

bool allFinite(const Out& o)
{
    for (std::size_t i = 0; i < o.L.size(); ++i)
        if (!std::isfinite(o.L[i]) || !std::isfinite(o.R[i])) return false;
    return true;
}

bool allZero(const Out& o)
{
    for (std::size_t i = 0; i < o.L.size(); ++i)
        if (o.L[i] != 0.0f || o.R[i] != 0.0f) return false;
    return true;
}

} // namespace

TEST_CASE("engine_bounds: near-end grain shrinks and its window completes", "[engine_bounds]")
{
    // Constant-1 source, +12 st (rate 2). Density 0.1 gives one grain at sample 0 (next after 10 s).
    const int frames = 10000;
    const SampleBuffer src(std::vector<float>(static_cast<std::size_t>(frames), 1.0f), {}, 48000.0);
    for (float position : {0.999f, 0.9f}) {
        GranularEngine e;
        e.prepare(48000.0, 1);
        e.setSource(&src);
        EngineParams p;
        p.density = 0.1f;
        p.grainSizeMs = 100.0f; // 4800 samples requested
        p.pitchSemitones = 12.0f;
        p.position = position;
        e.setParams(p);
        const Out o = render(e, 6000);

        int lastNonZero = -1;
        for (int i = 0; i < 6000; ++i)
            if (o.L[static_cast<std::size_t>(i)] != 0.0f) lastNonZero = i;
        const int len = lastNonZero + 1; // the last window sample is sin^2(pi/len) > 0
        const double start = static_cast<double>(position) * (frames - 1);
        const int expectedLen = static_cast<int>(std::floor((frames - 1 - start) / 2.0));
        REQUIRE(len >= 2);
        CHECK(len < 4800); // shrunk, not full length
        CHECK(std::abs(len - expectedLen) <= 1);

        const double centre = std::cos(kPi / 4.0); // panSpread 0: both channels at cos(pi/4)
        const double tail = std::pow(std::sin(kPi / len), 2.0) * centre;
        CHECK(std::abs(static_cast<double>(o.L[static_cast<std::size_t>(len - 1)])) <= tail + 1e-5);
        for (int i = len; i < 6000; ++i) REQUIRE(o.L[static_cast<std::size_t>(i)] == 0.0f);

        // No discontinuity: |step| <= max Hann slope per sample (pi/len) times the gain.
        const double bound = kPi / len * centre + 1e-4;
        double maxStep = std::abs(static_cast<double>(o.L[0])); // the grain starts from 0
        for (int i = 1; i < 6000; ++i)
            maxStep = std::max(maxStep, std::abs(static_cast<double>(o.L[static_cast<std::size_t>(i)] - o.L[static_cast<std::size_t>(i - 1)])));
        CHECK(maxStep <= bound);
    }
}

TEST_CASE("engine_bounds: tiny sources stay finite", "[engine_bounds]")
{
    for (int frames : {1, 2, 3}) {
        const SampleBuffer src(std::vector<float>(static_cast<std::size_t>(frames), 0.8f), {}, 48000.0);
        GranularEngine e;
        e.prepare(48000.0, 5);
        e.setSource(&src);
        EngineParams p;
        p.density = 1000.0f;
        p.grainSizeMs = 1.0f;
        p.pitchSemitones = 24.0f;
        p.positionJitter = 1.0f;
        e.setParams(p);
        const Out o = render(e, 4800, 128);
        REQUIRE(allFinite(o));
        for (float v : o.L) REQUIRE(std::abs(v) <= 4.0f);
    }
}

TEST_CASE("engine_bounds: density 0 and missing source give exact silence", "[engine_bounds]")
{
    const SampleBuffer src(std::vector<float>(5000, 1.0f), {}, 48000.0);
    GranularEngine e;
    e.prepare(48000.0, 3);
    e.setSource(&src);
    EngineParams p;
    p.density = 0.0f;
    e.setParams(p);
    CHECK(allZero(render(e, 2000, 100)));

    GranularEngine e2;
    e2.prepare(48000.0, 3);
    e2.setParams(EngineParams{}); // no source set
    CHECK(allZero(render(e2, 2000, 100)));

    const SampleBuffer empty({}, {}, 48000.0);
    e2.setSource(&empty);
    CHECK(allZero(render(e2, 500)));
}

TEST_CASE("engine_bounds: mono equals dual-mono bit for bit", "[engine_bounds]")
{
    const std::vector<float> data = noise(20000, 11);
    const SampleBuffer mono(data, {}, 44100.0);
    const SampleBuffer dual(data, data, 44100.0);
    EngineParams p;
    p.density = 150.0f;
    p.grainSizeMs = 30.0f;
    p.positionJitter = 0.5f;
    p.pitchJitterSemitones = 5.0f;
    p.timingJitter = 0.4f;
    p.panSpread = 1.0f;

    GranularEngine a, b;
    a.prepare(48000.0, 42);
    b.prepare(48000.0, 42);
    a.setSource(&mono);
    b.setSource(&dual);
    a.setParams(p);
    b.setParams(p);
    const Out oa = render(a, 24000, 256);
    const Out ob = render(b, 24000, 256);
    CHECK(oa.L == ob.L);
    CHECK(oa.R == ob.R);
    CHECK_FALSE(allZero(oa)); // sound was actually produced
}

TEST_CASE("engine_bounds: pan spread 0 centres a mono source, spread 1 does not", "[engine_bounds]")
{
    const SampleBuffer src(noise(20000, 3), {}, 48000.0);
    EngineParams p;
    p.density = 80.0f;
    p.grainSizeMs = 40.0f;
    p.positionJitter = 0.3f;

    GranularEngine e;
    e.prepare(48000.0, 9);
    e.setSource(&src);
    p.panSpread = 0.0f;
    e.setParams(p);
    const Out centred = render(e, 24000, 500);
    CHECK_FALSE(allZero(centred));
    CHECK(centred.L == centred.R);

    GranularEngine e2;
    e2.prepare(48000.0, 9);
    e2.setSource(&src);
    p.panSpread = 1.0f;
    e2.setParams(p);
    const Out spread = render(e2, 24000, 500);
    CHECK_FALSE(spread.L == spread.R);
}

TEST_CASE("engine_bounds: output ceiling and denormal flush", "[engine_bounds]")
{
    // Loud source at maximum gain/density/size stays inside +-4.
    const SampleBuffer loud(std::vector<float>(8000, 1.0f), {}, 48000.0);
    GranularEngine e;
    e.prepare(48000.0, 2);
    e.setSource(&loud);
    EngineParams p;
    p.density = 1000.0f;
    p.grainSizeMs = 1000.0f;
    p.gain = 2.0f;
    e.setParams(p);
    const Out o = render(e, 48000, 480);
    REQUIRE(allFinite(o));
    for (float v : o.L) REQUIRE(std::abs(v) <= 4.0f);

    // A source far below the flush threshold produces exact zeros, never denormals.
    const SampleBuffer tiny(std::vector<float>(4000, 1e-30f), {}, 48000.0);
    GranularEngine e2;
    e2.prepare(48000.0, 2);
    e2.setSource(&tiny);
    e2.setParams(EngineParams{});
    CHECK(allZero(render(e2, 24000, 480)));
}

TEST_CASE("engine_bounds: normalization keeps dense and sparse loudness close", "[engine_bounds]")
{
    const SampleBuffer src(noise(48000, 21), {}, 48000.0);
    auto rms = [&](float density) {
        GranularEngine e;
        e.prepare(48000.0, 4);
        e.setSource(&src);
        EngineParams p;
        p.density = density;
        p.grainSizeMs = 50.0f;
        p.positionJitter = 1.0f;
        e.setParams(p);
        const Out o = render(e, 48000 * 3, 1024);
        double acc = 0.0;
        for (std::size_t i = 24000; i < o.L.size(); ++i) acc += static_cast<double>(o.L[i]) * o.L[i];
        return std::sqrt(acc / static_cast<double>(o.L.size() - 24000));
    };
    const double ratioDb = 20.0 * std::log10(rms(400.0f) / rms(20.0f));
    CHECK(std::abs(ratioDb) <= 3.0);
}
