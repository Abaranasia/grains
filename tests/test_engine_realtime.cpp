// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/GrainSnapshot.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>
#include <grains/TripleBuffer.h>

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

namespace {
// Writing to and reading back a volatile makes the pointer escape, so the compiler cannot elide
// the new/delete pair (allowed since C++14) and the free counter really sees the delete.
int* volatile g_sink = nullptr;
int* escape(int* p) noexcept
{
    g_sink = p;
    return g_sink;
}
} // namespace

// The free counter ignores null deletes and frees outside a scope (R-SS-3).
TEST_CASE("realtime: the allocation counter counts non-null frees inside its scope only", "[realtime]")
{
    auto* outside = escape(new int(1));
    std::size_t frees = 99;
    {
        AllocationScope scope;
        delete outside; // freed inside the scope: counted
        int* none = nullptr;
        delete none; // null: not counted
        frees = scope.frees();
    }
    CHECK(frees == 1);
    auto* q = escape(new int(2));
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
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
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
    e.prepare(kSr, 2, grains::InitialTransport::Playing);
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

// R-GR-10: the limiter is stateless and allocation-free while it is actually engaged. A coherent DC
// source at gain 2 drives the sum far above the knee, so the measured peak proves the non-linear
// region ran inside the scope.
TEST_CASE("realtime: process() performs zero allocations while the limiter is engaged", "[realtime][soft_limiter]")
{
    const SampleBuffer dc(std::vector<float>(200000, 1.0f), {}, kSr);
    GranularEngine e;
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
    e.setSource(&dc);
    EngineParams p;
    p.density = 200.0f;
    p.grainSizeMs = 1000.0f;
    p.position = 0.0f;
    p.gain = 2.0f;
    e.setParams(p);
    std::vector<float> l(512), r(512);
    for (int i = 0; i < 100; ++i) e.process(l.data(), r.data(), 512); // warm up outside the scope
    const RunStats stats = allocationsWhileProcessing(e, 512, 100);
    CHECK(stats.allocs == 0);
    CHECK(stats.peak > GranularEngine::kLimiterKnee);
    CHECK(stats.peak < GranularEngine::kOutputCeiling);
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
                        e.prepare(kSr, 3, grains::InitialTransport::Playing);
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
        e.prepare(kSr, 3, grains::InitialTransport::Playing);
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
        e.prepare(kSr, 4, grains::InitialTransport::Playing);
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
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
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

// R-SS-11: the gated commit path is allocation-free too. stage() allocates, so it runs before the
// scope; commit() only swaps a pointer and returns the displaced Box, which is dropped after it.
TEST_CASE("realtime: committing a staged source and acquiring it allocates and frees nothing", "[realtime][exchange]")
{
    grains::SourceExchange<SampleBuffer> x;
    x.publish(noiseBuffer(31));
    x.acquire();
    x.collect();
    x.publish(noiseBuffer(33)); // unseen pending: commit will displace it
    auto staged = x.stage(noiseBuffer(35));
    REQUIRE(static_cast<bool>(staged));
    grains::SourceExchange<SampleBuffer>::Staged displaced;
    std::size_t allocs = 99, frees = 99;
    bool changed = false;
    {
        AllocationScope scope;
        displaced = x.commit(std::move(staged));
        const auto acquired = x.acquire();
        changed = acquired.changed;
        allocs = scope.count();
        frees = scope.frees();
    }
    CHECK(changed);
    CHECK(allocs == 0);
    CHECK(frees == 0);
    CHECK(static_cast<bool>(displaced));
    displaced = grains::SourceExchange<SampleBuffer>::Staged{};
    x.collect();
}

// R-GR-11, R-PS-5, R-WD-6: the audio thread's whole display hook (snapshot into the mailbox's back
// slot, publish) and the consumer's poll (update, front) allocate nothing, with a light and a
// saturated pool. The mailbox is plain member storage, so it exists before the scope.
TEST_CASE("realtime: snapshotting and publishing grains allocates nothing", "[realtime][grain_snapshot]")
{
    const SampleBuffer src(grains_test::noise(200000, 13), grains_test::noise(200000, 14), kSr);
    for (float density : {40.0f, 1000.0f}) {
        INFO("density=" << density);
        GranularEngine e;
        e.prepare(kSr, 6, grains::InitialTransport::Playing);
        e.setSource(&src);
        EngineParams p;
        p.density = density;
        p.grainSizeMs = 1000.0f;
        p.position = 0.1f;
        e.setParams(p);
        grains::TripleBuffer<grains::GrainSnapshot> mailbox;
        std::vector<float> l(512), r(512);
        for (int i = 0; i < 40; ++i) e.process(l.data(), r.data(), 512); // warm up outside the scope
        std::size_t allocs = 99;
        int fresh = 0, mismatches = 0, maxCount = 0;
        {
            AllocationScope scope;
            for (int i = 0; i < 100; ++i) {
                e.process(l.data(), r.data(), 512);
                e.snapshotGrains(mailbox.back());
                mailbox.publish();
                if (mailbox.update()) {
                    ++fresh;
                    if (mailbox.front().count != e.activeGrainCount()) ++mismatches;
                    maxCount = std::max(maxCount, static_cast<int>(mailbox.front().count));
                }
            }
            allocs = scope.count();
        }
        CHECK(allocs == 0);
        CHECK(fresh == 100);
        CHECK(mismatches == 0);
        CHECK(maxCount > 0);
        if (density == 1000.0f) CHECK(maxCount == grains::GrainPool::kCapacity);
    }
}

// R-TG-9: Play and Stop requests, restarts, fades (also reversed mid-fade), stopNow and idle
// stopped blocks never allocate. Sound is asserted so the toggles really exercised the render.
TEST_CASE("realtime: process() performs zero allocations across Play, Stop and stopNow toggles", "[realtime][transport]")
{
    const SampleBuffer src(grains_test::noise(96000, 5), grains_test::noise(96000, 6), kSr);
    GranularEngine e;
    e.prepare(kSr, 1); // Stopped
    e.setSource(&src);
    EngineParams p;
    p.density = 80.0f;
    p.grainSizeMs = 60.0f;
    p.positionJitter = 0.2f;
    p.timingJitter = 0.3f;
    e.setParams(p);
    constexpr int kBlock = 256;
    std::vector<float> l(kBlock), r(kBlock);
    float* const pl = l.data();
    float* const pr = r.data();
    float peak = 0.0f;
    std::size_t allocs = 0;
    {
        AllocationScope scope;
        auto run = [&](int blocks) {
            for (int i = 0; i < blocks; ++i) {
                e.process(pl, pr, kBlock);
                for (int k = 0; k < kBlock; ++k) peak = std::max({peak, std::abs(pl[k]), std::abs(pr[k])});
            }
        };
        run(4); // stopped
        e.setPlaying(true);
        run(2); // mid fade-in (ramp is 960 samples)
        e.setPlaying(false); // reverses the fade-in
        run(1);
        e.setPlaying(true); // latched during Stopping
        run(12);            // fade-out completes, then the restart fades in and plays
        e.setPlaying(true); // redundant
        run(8);
        e.setPlaying(false);
        run(10); // fade-out and idle stopped blocks
        e.setPlaying(true);
        run(6);
        e.stopNow();
        run(3);
        allocs = scope.count();
    }
    CHECK(allocs == 0);
    CHECK(peak > kSoundThreshold);
}
