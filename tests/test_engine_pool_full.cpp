#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <cmath>
#include <cstddef>
#include <vector>

using grains::EngineParams;
using grains::GrainPool;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::allZero;
using grains_test::noise;
using grains_test::Out;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 100;

// Renders `total` samples in kBlock blocks. When stopAt > 0 the density is set to 0 once that many
// samples were rendered (a multiple of kBlock), which ends spawn attempts for the rest of the run.
// `activeAtStop` receives the engine's active grain count right before the density is zeroed.
Out renderWithStop(GranularEngine& e, EngineParams p, int total, int stopAt, int* activeAtStop = nullptr)
{
    Out o{std::vector<float>(static_cast<std::size_t>(total)), std::vector<float>(static_cast<std::size_t>(total))};
    for (int done = 0; done < total; done += kBlock) {
        if (stopAt > 0 && done == stopAt) {
            if (activeAtStop != nullptr) *activeAtStop = e.activeGrainCount();
            p.density = 0.0f;
            e.setParams(p);
        }
        e.process(o.L.data() + done, o.R.data() + done, kBlock);
    }
    return o;
}

} // namespace

// R-GS-6: a spawn that finds the pool full is skipped. Density 1000 spawns every ~48 samples, so
// the 128 slots fill within ~0.13 s while every grain lives for the whole window under test.
// Run A keeps attempting (and skipping) spawns; run B stops attempting once the pool is full.
// Until the first grain ends, both must be bit-identical: skipped spawns add nothing.
TEST_CASE("engine_pool_full: skipped spawns leave the 128 active grains untouched", "[engine_pool_full]")
{
    // Long source: no grain is shrunk by the buffer end, so every grain lives its full requested length.
    const SampleBuffer src(noise(200000, 12), noise(200000, 13), kSr);
    struct Case {
        float sizeMs;
        float timingJitter;
        float positionJitter;
        float pitchJitter;
        double stopMargin; // stop attempts after this multiple of the nominal pool-fill time
    };
    const Case cases[] = {
        {1000.0f, 0.0f, 0.0f, 0.0f, 1.05},  // regular spawns: pool fills at ~kCapacity * period
        {500.0f, 0.5f, 0.2f, 12.0f, 1.6},   // irregular spawns, all draws in use: wider margin
    };
    for (const Case& c : cases) {
        INFO("sizeMs=" << c.sizeMs);
        EngineParams p;
        p.density = 1000.0f;
        p.grainSizeMs = c.sizeMs;
        p.timingJitter = c.timingJitter;
        p.position = 0.1f;
        p.positionJitter = c.positionJitter;
        p.pitchJitterSemitones = c.pitchJitter;
        p.panSpread = 1.0f;
        // Nominal fill time: kCapacity spawns at one per (kSr / density) samples, scaled by the
        // margin and rounded up to a whole block so setParams lands on a block boundary.
        const double period = kSr / p.density;
        const int fillSamples = static_cast<int>(std::ceil(GrainPool::kCapacity * period * c.stopMargin));
        const int stopAt = (fillSamples + kBlock - 1) / kBlock * kBlock;
        const int window = static_cast<int>(c.sizeMs * 0.001 * kSr); // first grain's lifetime
        REQUIRE(window % kBlock == 0);

        GranularEngine a, b;
        a.prepare(kSr, 31);
        b.prepare(kSr, 31);
        a.setSource(&src);
        b.setSource(&src);
        a.setParams(p);
        b.setParams(p);
        REQUIRE(stopAt < window); // the first grains are still alive when attempts stop
        int activeAtStop = 0;
        const Out oa = renderWithStop(a, p, window, 0);
        const Out ob = renderWithStop(b, p, window, stopAt, &activeAtStop);
        // The pool saturated before attempts stopped (so later spawns were skipped); no grain had ended.
        CHECK(activeAtStop == GrainPool::kCapacity);
        REQUIRE_FALSE(allZero(oa));
        CHECK(oa.L == ob.L);
        CHECK(oa.R == ob.R);
    }
}
