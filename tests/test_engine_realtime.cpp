#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>

#include "support/AllocationCounter.h"
#include "support/EngineHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::AllocationScope;

namespace {

constexpr double kSr = 48000.0;

// Anything above this counts as audible output (the noise source and 1 ms grains stay well above it).
constexpr float kSoundThreshold = 1.0e-3f;

struct RunStats {
    std::size_t allocs;
    int maxActive; // highest activeGrainCount() seen after any block
    float peak;    // largest |sample| seen in any block
};

// Renders `blocks` blocks of `block` samples inside an AllocationScope and returns the number of
// allocations plus what the run produced. Engine setup and buffer sizing happen before the scope;
// checks happen after it. Observation is per block (plain loops, no allocation): grains shorter
// than a block are released inside it and the buffers are overwritten by the next block, so an
// end-of-run poll or scan would miss them.
RunStats allocationsWhileProcessing(GranularEngine& e, int block, int blocks)
{
    std::vector<float> l(static_cast<std::size_t>(block)), r(static_cast<std::size_t>(block));
    float* const pl = l.data();
    float* const pr = r.data();
    RunStats stats{0, 0, 0.0f};
    {
        AllocationScope scope;
        for (int i = 0; i < blocks; ++i) {
            e.process(pl, pr, block);
            stats.maxActive = std::max(stats.maxActive, e.activeGrainCount());
            for (int k = 0; k < block; ++k)
                stats.peak = std::max({stats.peak, std::abs(pl[k]), std::abs(pr[k])});
        }
        stats.allocs = scope.count();
    }
    return stats;
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

// The free counter ignores null deletes and frees outside a scope (R-SS-3).
TEST_CASE("realtime: the allocation counter counts non-null frees inside its scope only", "[realtime]")
{
    auto* outside = new int(1);
    std::size_t frees = 99;
    {
        AllocationScope scope;
        delete outside; // freed inside the scope: counted
        int* none = nullptr;
        delete none; // null: not counted
        frees = scope.frees();
    }
    CHECK(frees == 1);
    auto* q = new int(2);
    {
        AllocationScope scope;
        CHECK(scope.frees() == 0);
    }
    delete q; // outside any scope
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
    CHECK(allocationsWhileProcessing(e, 512, 400).allocs == 0);
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
    CHECK(allocationsWhileProcessing(e, 512, 200).allocs == 0);
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
                        const RunStats stats = allocationsWhileProcessing(e, 512, 20);
                        total += stats.allocs;
                        if (density == 0.0f) {
                            CHECK(stats.maxActive == 0);
                            CHECK(stats.peak == 0.0f);
                        } else {
                            CHECK(stats.peak > kSoundThreshold);
                            // 1000 ms grains from position 0 outlive every block; 1 ms grains and the
                            // 2-sample move-back grains at position 1 are released inside the block.
                            if (size == 1000.0f && position == 0.0f) CHECK(stats.maxActive > 0);
                        }
                    }
    CHECK(total == 0);
}

// Deterministic saturation: grains outlive the 128-slot pool refill time, so the pool fills at
// both pitch extremes while process() still allocates nothing.
TEST_CASE("realtime: a saturating density fills the pool without allocating", "[realtime][pool_full]")
{
    const SampleBuffer src(grains_test::noise(48000, 9), grains_test::noise(48000, 10), kSr);
    for (float pitch : {-24.0f, 24.0f}) {
        INFO("pitch=" << pitch);
        GranularEngine e;
        e.prepare(kSr, 3);
        e.setSource(&src);
        EngineParams p;
        p.grainSizeMs = 1000.0f;
        p.density = 1000.0f;
        p.pitchSemitones = pitch;
        p.position = 0.0f;
        e.setParams(p);
        const RunStats stats = allocationsWhileProcessing(e, 512, 20);
        CHECK(stats.allocs == 0);
        CHECK(stats.maxActive == grains::GrainPool::kCapacity);
        CHECK(stats.peak > kSoundThreshold);
    }
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
        CHECK(allocationsWhileProcessing(e, block, block == 1 ? 4000 : 40).allocs == 0);
    }
}

namespace {

std::unique_ptr<SampleBuffer> noiseBuffer(std::uint32_t seed)
{
    return std::make_unique<SampleBuffer>(grains_test::noise(48000, seed), grains_test::noise(48000, seed + 1), kSr);
}

// R-SS-3: the consumer side of a swap (acquire, retire, setSource, process) neither allocates nor
// frees. publish allocates the Box, so it runs before the scope; collect (the free) never runs in it.
void checkSwapIsRealtimeSafe(bool clear)
{
    grains::SourceExchange<SampleBuffer> x;
    GranularEngine e;
    e.prepare(kSr, 1);
    EngineParams p;
    p.density = 40.0f;
    p.grainSizeMs = 80.0f;
    e.setParams(p);
    x.publish(noiseBuffer(21));
    e.setSource(x.acquire().source);
    x.publish(clear ? nullptr : noiseBuffer(23));
    std::vector<float> l(512), r(512);
    std::size_t allocs = 99, frees = 99;
    bool changed = false;
    {
        AllocationScope scope;
        const auto acquired = x.acquire();
        changed = acquired.changed;
        e.setSource(acquired.source);
        for (int i = 0; i < 50; ++i) e.process(l.data(), r.data(), 512);
        allocs = scope.count();
        frees = scope.frees();
    }
    CHECK(changed);
    CHECK(allocs == 0);
    CHECK(frees == 0);
    x.collect();
}

} // namespace

TEST_CASE("realtime: acquiring a swapped source and processing allocates and frees nothing", "[realtime][exchange]")
{
    checkSwapIsRealtimeSafe(false);
}

TEST_CASE("realtime: acquiring a cleared source and processing allocates and frees nothing", "[realtime][exchange]")
{
    checkSwapIsRealtimeSafe(true);
}
