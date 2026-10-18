#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/AllocationCounter.h"
#include "support/EngineHelpers.h"

#include <cstddef>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::AllocationScope;

namespace {

constexpr double kSr = 48000.0;

// Renders `blocks` blocks of `block` samples inside an AllocationScope and returns the number of
// allocations. Engine setup and buffer sizing happen before the scope; checks happen after it.
std::size_t allocationsWhileProcessing(GranularEngine& e, int block, int blocks)
{
    std::vector<float> l(static_cast<std::size_t>(block)), r(static_cast<std::size_t>(block));
    float* const pl = l.data();
    float* const pr = r.data();
    std::size_t n = 0;
    {
        AllocationScope scope;
        for (int i = 0; i < blocks; ++i) e.process(pl, pr, block);
        n = scope.count();
    }
    return n;
}

} // namespace

// Proves the counter can fail: a test that cannot see allocations would pass vacuously.
TEST_CASE("realtime: the allocation counter sees allocations inside its scope only", "[realtime]")
{
    std::size_t inside = 0;
    {
        AllocationScope scope;
        auto* p = new int(1);
        std::vector<int> v;
        v.push_back(2);
        inside = scope.count();
        delete p;
    }
    CHECK(inside >= 2);

    std::size_t fresh = 0;
    auto* q = new int(3); // outside any scope: not counted
    {
        AllocationScope scope;
        fresh = scope.count();
    }
    delete q;
    CHECK(fresh == 0);

    // Over-aligned new/delete pair stays consistent and is counted.
    struct alignas(64) Over {
        char c[64];
    };
    std::size_t aligned = 0;
    {
        AllocationScope scope;
        auto* o = new Over;
        aligned = scope.count();
        delete o;
    }
    CHECK(aligned >= 1);
}

TEST_CASE("realtime: process() performs zero allocations with normal settings", "[realtime]")
{
    const SampleBuffer src(grains_test::noise(96000, 5), grains_test::noise(96000, 6), kSr);
    GranularEngine e;
    e.prepare(kSr, 1);
    e.setSource(&src);
    EngineParams p;
    p.density = 40.0f;
    p.grainSizeMs = 80.0f;
    p.positionJitter = 0.2f;
    p.pitchJitterSemitones = 3.0f;
    p.timingJitter = 0.3f;
    p.panSpread = 0.8f;
    e.setParams(p);
    CHECK(allocationsWhileProcessing(e, 512, 400) == 0);
}

TEST_CASE("realtime: process() performs zero allocations with a full pool", "[realtime][pool_full]")
{
    const SampleBuffer src(grains_test::noise(200000, 7), grains_test::noise(200000, 8), kSr);
    GranularEngine e;
    e.prepare(kSr, 2);
    e.setSource(&src);
    EngineParams p;
    p.density = 1000.0f;
    p.grainSizeMs = 1000.0f;
    p.position = 0.1f;
    e.setParams(p);
    // Warm up outside the scope until the pool is saturated; the measured blocks then keep skipping spawns.
    std::vector<float> l(512), r(512);
    for (int i = 0; i < 40; ++i) e.process(l.data(), r.data(), 512);
    REQUIRE(e.activeGrainCount() == grains::GrainPool::kCapacity);
    CHECK(allocationsWhileProcessing(e, 512, 200) == 0);
    CHECK(e.activeGrainCount() == grains::GrainPool::kCapacity);
}

TEST_CASE("realtime: process() performs zero allocations at parameter extremes", "[realtime]")
{
    const SampleBuffer src(grains_test::noise(48000, 9), grains_test::noise(48000, 10), kSr);
    const float sizes[] = {1.0f, 1000.0f};
    const float pitches[] = {-24.0f, 24.0f};
    const float densities[] = {0.0f, 0.1f, 1000.0f};
    const float jitters[] = {0.0f, 1.0f};
    const float positions[] = {0.0f, 1.0f};
    std::size_t total = 0;
    for (float size : sizes)
        for (float pitch : pitches)
            for (float density : densities)
                for (float jitter : jitters)
                    for (float position : positions) {
                        GranularEngine e;
                        e.prepare(kSr, 3);
                        e.setSource(&src);
                        EngineParams p;
                        p.grainSizeMs = size;
                        p.pitchSemitones = pitch;
                        p.density = density;
                        p.timingJitter = jitter;
                        p.positionJitter = jitter;
                        p.pitchJitterSemitones = jitter * 24.0f;
                        p.position = position;
                        e.setParams(p);
                        total += allocationsWhileProcessing(e, 512, 20);
                    }
    CHECK(total == 0);
}

TEST_CASE("realtime: process() performs zero allocations for any block size", "[realtime]")
{
    const SampleBuffer src(grains_test::noise(96000, 11), grains_test::noise(96000, 12), kSr);
    for (int block : {1, 37, 512, 4096}) {
        INFO("block=" << block);
        GranularEngine e;
        e.prepare(kSr, 4);
        e.setSource(&src);
        EngineParams p;
        p.density = 500.0f;
        p.grainSizeMs = 200.0f;
        p.timingJitter = 0.5f;
        e.setParams(p);
        CHECK(allocationsWhileProcessing(e, block, block == 1 ? 4000 : 40) == 0);
    }
}
