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
using grains_test::allFinite;
using grains_test::allZero;
using grains_test::kPi;
using grains_test::noise;
using grains_test::Out;
using grains_test::render;

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

// Position 1.0 puts the start on the last frame, so the shrunk length is 0 and the spawn takes the
// move-back fallback: start = last - 2 * rate, length = kMinGrainLength (2 samples). Density 0.1
// gives exactly one grain at sample 0. The grain renders window(0) = 0 then window(0.5) = 1 at
// source position start + rate, so the only non-zero output is L[1] = src * cos(pi/4).
TEST_CASE("engine_bounds: move-back fallback renders a 2-sample grain inside the source", "[engine_bounds]")
{
    const int frames = 1000;
    std::vector<float> ramp(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) ramp[static_cast<std::size_t>(i)] = static_cast<float>(i) / 1000.0f;
    const SampleBuffer src(ramp, {}, 48000.0);
    struct Case {
        float semitones;
        int rate;
    };
    for (const Case c : {Case{0.0f, 1}, Case{12.0f, 2}}) {
        GranularEngine e;
        e.prepare(48000.0, 1);
        e.setSource(&src);
        EngineParams p;
        p.density = 0.1f;
        p.grainSizeMs = 100.0f;
        p.position = 1.0f;
        p.pitchSemitones = c.semitones;
        e.setParams(p);
        const Out o = render(e, 200);

        const int readIndex = (frames - 1) - 2 * c.rate + c.rate; // start + rate, still inside the source
        const double expected = static_cast<double>(readIndex) / 1000.0 * std::cos(kPi / 4.0);
        CHECK(o.L[0] == 0.0f);
        CHECK(std::abs(static_cast<double>(o.L[1]) - expected) <= 1e-4);
        CHECK(o.L[1] == o.R[1]);
        for (int i = 2; i < 200; ++i) REQUIRE(o.L[static_cast<std::size_t>(i)] == 0.0f); // exactly 2 samples long
    }
}

// Tiny sources: which frame counts can host a grain depends on the move-back fallback. With 1 ms
// grains at density 1000 and position 1.0 (start on the last frame), a source of F frames hosts a
// grain only if last - 2 * rate >= 0, i.e. F >= 3 at rate 1 and F >= 2 at rate 0.25.
TEST_CASE("engine_bounds: tiny sources are silent or sound exactly where the fallback can fit", "[engine_bounds]")
{
    struct Case {
        int frames;
        float semitones;
        bool sounds;
    };
    const Case cases[] = {
        {1, 0.0f, false},   {1, -24.0f, false}, // last = 0: no start fits 2 samples
        {2, 0.0f, false},                       // start = 1 - 2 < 0
        {2, -24.0f, true},                      // rate 0.25: start = 0.5
        {3, 0.0f, true},                        // start = 0
        {3, 24.0f, false},                      // rate 4: start = 2 - 8 < 0
    };
    for (const Case& c : cases) {
        INFO("frames=" << c.frames << " semitones=" << c.semitones);
        const SampleBuffer src(std::vector<float>(static_cast<std::size_t>(c.frames), 0.8f), {}, 48000.0);
        GranularEngine e;
        e.prepare(48000.0, 5);
        e.setSource(&src);
        EngineParams p;
        p.density = 1000.0f;
        p.grainSizeMs = 1.0f;
        p.pitchSemitones = c.semitones;
        p.position = 1.0f;
        e.setParams(p);
        const Out o = render(e, 4800, 128);
        REQUIRE(allFinite(o));
        CHECK(grains_test::peak(o) <= 4.0f);
        if (c.sounds) {
            // 0.8 * w(0.5) * cos(pi/4); grains are 2 samples long so they never overlap at 48 samples spacing.
            CHECK(std::abs(static_cast<double>(grains_test::peak(o)) - 0.8 * std::cos(kPi / 4.0)) <= 1e-4);
        } else {
            CHECK(allZero(o));
        }
    }
}

// R-GR-7 grain length: round(size * sampleRate) with literal expectations (10 ms). 22.05 kHz is a
// .5 tie (220.5) and rounds up. A source whose rate differs from the engine rate keeps the length,
// because the length is counted in output samples. Position 0 and a long source never shrink it.
TEST_CASE("engine_bounds: grain length follows size and engine sample rate", "[engine_bounds]")
{
    struct Case {
        double engineRate;
        double sourceRate;
        int expectedLen;
    };
    const Case cases[] = {
        {22050.0, 22050.0, 221},   {44100.0, 44100.0, 441},   {48000.0, 48000.0, 480},
        {96000.0, 96000.0, 960},   {192000.0, 192000.0, 1920}, {48000.0, 44100.0, 480},
    };
    for (const Case& c : cases) {
        INFO("engineRate=" << c.engineRate << " sourceRate=" << c.sourceRate);
        const SampleBuffer src(std::vector<float>(20000, 1.0f), {}, c.sourceRate);
        GranularEngine e;
        e.prepare(c.engineRate, 1);
        e.setSource(&src);
        EngineParams p;
        p.density = 0.1f; // one grain at sample 0
        p.grainSizeMs = 10.0f;
        p.position = 0.0f;
        e.setParams(p);
        const Out o = render(e, 4000);

        int lastNonZero = -1;
        for (int i = 0; i < 4000; ++i)
            if (o.L[static_cast<std::size_t>(i)] != 0.0f) lastNonZero = i;
        CHECK(lastNonZero + 1 == c.expectedLen);
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
