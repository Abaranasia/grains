// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/GrainPool.h>

#include "support/SchedulerHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

using grains_test::run;
using grains_test::Spawn;

TEST_CASE("scheduler: periodic 100 gr/s gives 100 spawns 480 apart", "[scheduler]")
{
    const auto sp = run(1, 48000.0, 100.0f, 0.0f, 48000, {512});
    REQUIRE(sp.size() == 100);
    CHECK(sp.front().time == 0); // first spawn at sample 0
    for (std::size_t i = 1; i < sp.size(); ++i)
        CHECK(sp[i].time - sp[i - 1].time == 480);
}

TEST_CASE("scheduler: fractional period has no drift over 10 s", "[scheduler]")
{
    const double sr = 48000.0, period = sr / 70.0;
    const auto sp = run(1, sr, 70.0f, 0.0f, 480000, {512});
    CHECK(std::abs(static_cast<int>(sp.size()) - 700) <= 1);
    for (std::size_t k = 0; k < sp.size(); ++k)
        CHECK(std::abs(static_cast<double>(sp[k].time) - static_cast<double>(k) * period) <= 1.0);
}

TEST_CASE("scheduler: drift stays below 1 sample over 1e6 samples", "[scheduler]")
{
    const double sr = 44100.0, period = sr / 33.0;
    const auto sp = run(3, sr, 33.0f, 0.0f, 1000000, {480});
    REQUIRE(sp.size() > 700);
    for (std::size_t k = 0; k < sp.size(); ++k)
        REQUIRE(std::abs(static_cast<double>(sp[k].time) - static_cast<double>(k) * period) <= 1.0);
}

TEST_CASE("scheduler: zero density spawns nothing", "[scheduler]")
{
    CHECK(run(1, 48000.0, 0.0f, 0.5f, 96000, {512}).empty());
}

TEST_CASE("scheduler: offsets stay inside the block", "[scheduler]")
{
    // The bounds are asserted inside run() for every spawn.
    for (int b : {1, 7, 64, 480, 481, 4096})
        CHECK_FALSE(run(5, 48000.0, 300.0f, 0.7f, 20000, {b}).empty());
}

TEST_CASE("scheduler: block split invariance", "[scheduler]")
{
    const std::int64_t total = 48000;
    const auto whole = run(9, 48000.0, 150.0f, 0.6f, total, {48000});
    REQUIRE(whole.size() > 100);
    CHECK(run(9, 48000.0, 150.0f, 0.6f, total, {64}) == whole);
    CHECK(run(9, 48000.0, 150.0f, 0.6f, total, {1}) == whole);
    CHECK(run(9, 48000.0, 150.0f, 0.6f, total, {1, 3, 4095, 17, 512, 2, 999}) == whole);
}

TEST_CASE("scheduler: jitter 0 ignores the seed", "[scheduler]")
{
    const auto a = run(1, 48000.0, 90.0f, 0.0f, 48000, {256});
    const auto b = run(2, 48000.0, 90.0f, 0.0f, 48000, {256});
    REQUIRE(a.size() == b.size());
    for (std::size_t i = 0; i < a.size(); ++i)
        CHECK(a[i].time == b[i].time);
}

TEST_CASE("scheduler: same seed reproducible, different seed differs", "[scheduler]")
{
    const auto a = run(12345, 48000.0, 100.0f, 0.5f, 48000, {256});
    const auto b = run(12345, 48000.0, 100.0f, 0.5f, 48000, {256});
    const auto c1 = run(1, 48000.0, 100.0f, 0.5f, 48000, {256});
    const auto c2 = run(2, 48000.0, 100.0f, 0.5f, 48000, {256});
    CHECK(a == b);
    CHECK_FALSE(c1 == c2);
}

TEST_CASE("scheduler: jitter 1 preserves the mean rate", "[scheduler]")
{
    const std::int64_t total = 60 * 48000;
    double meanPerSecond = 0.0;
    for (std::uint64_t seed = 1; seed <= 20; ++seed)
        meanPerSecond += static_cast<double>(run(seed, 48000.0, 100.0f, 1.0f, total, {512}).size()) / 60.0;
    meanPerSecond /= 20.0;
    CHECK(meanPerSecond > 95.0);
    CHECK(meanPerSecond < 105.0);
}

TEST_CASE("scheduler: spawn times are monotonic under jitter", "[scheduler]")
{
    const auto sp = run(4, 48000.0, 1000.0f, 1.0f, 96000, {128});
    REQUIRE(sp.size() > 100);
    for (std::size_t i = 1; i < sp.size(); ++i)
        CHECK(sp[i].time >= sp[i - 1].time);
}

TEST_CASE("scheduler: extreme parameters are clamped", "[scheduler]")
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(run(1, 48000.0, -5.0f, 0.0f, 48000, {512}).empty());
    const auto fast = run(1, 48000.0, 1e9f, 0.0f, 48000, {512});
    CHECK(fast.size() >= 1000);
    CHECK(fast.size() <= 1001);
    const auto def = run(1, 48000.0, nan, 0.0f, 48000, {512});
    CHECK(def.size() >= 20);
    CHECK(def.size() <= 21);
    CHECK(run(1, 48000.0, 100.0f, -1.0f, 48000, {512}) == run(1, 48000.0, 100.0f, 0.0f, 48000, {512}));
    CHECK(run(1, 48000.0, 100.0f, 7.0f, 48000, {512}) == run(1, 48000.0, 100.0f, 1.0f, 48000, {512}));
    CHECK(run(1, 48000.0, 100.0f, nan, 48000, {512}) == run(1, 48000.0, 100.0f, 0.0f, 48000, {512}));
}

TEST_CASE("scheduler: tiny interval floors at one sample", "[scheduler]")
{
    // 1000 gr/s at the minimum sample rate has an 8-sample period; with jitter 1 the raw
    // interval is 8*(1+bipolar) in [0, 16), so without the floor intervals below one sample
    // (and therefore coincident spawn times) would occur.
    const auto sp = run(6, 8000.0, 1000.0f, 1.0f, 80000, {64});
    REQUIRE(sp.size() > 5000);
    std::int64_t minGap = std::numeric_limits<std::int64_t>::max();
    for (std::size_t i = 1; i < sp.size(); ++i) {
        CHECK(sp[i].time > sp[i - 1].time); // strictly increasing
        minGap = std::min(minGap, sp[i].time - sp[i - 1].time);
    }
    CHECK(minGap >= 1);
}

namespace {

struct PoolRun {
    std::vector<std::int64_t> due;   // every due spawn time, accepted or skipped
    int accepted = 0;
    int skipped = 0;
    int maxActive = 0;
    bool skippedWhileNotFull = false;
};

// Drives a scheduler that shares its Rng with the spawn callback (as the engine does). The
// callback always takes one draw per due spawn; with a pool it then tries to acquire a slot and
// frees grains whose end time has passed. `skipDrawOnFull` mimics a wrong engine that skips the
// draw when the pool is full, which desynchronises the shared stream.
PoolRun simulate(bool usePool, std::int64_t grainLength, bool skipDrawOnFull = false)
{
    grains::Rng rng(8);
    grains::GrainScheduler sched(rng);
    sched.configure(48000.0, 1000.0f, 0.3f);
    sched.reset();

    grains::GrainPool pool;
    std::vector<std::pair<grains::Grain*, std::int64_t>> live; // slot and absolute end time
    PoolRun r;
    const std::int64_t total = 96000;
    const int block = 256;
    for (std::int64_t done = 0; done < total; done += block) {
        sched.advance(block, [&](int offset) {
            const std::int64_t t = done + offset;
            r.due.push_back(t);
            bool ok = true;
            if (usePool) {
                for (std::size_t i = 0; i < live.size();) {
                    if (live[i].second <= t) {
                        pool.release(*live[i].first);
                        live[i] = live.back();
                        live.pop_back();
                    } else {
                        ++i;
                    }
                }
                grains::Grain* g = pool.acquire();
                ok = g != nullptr;
                if (ok) {
                    live.emplace_back(g, t + grainLength);
                    ++r.accepted;
                } else {
                    ++r.skipped;
                    r.skippedWhileNotFull = r.skippedWhileNotFull || pool.activeCount() != grains::GrainPool::kCapacity;
                }
                r.maxActive = std::max(r.maxActive, pool.activeCount());
            }
            if (ok || !skipDrawOnFull) (void)rng.uniform01();
        });
    }
    return r;
}

} // namespace

TEST_CASE("scheduler: skip-on-full leaves the due times unchanged and reuses freed slots", "[scheduler]")
{
    // ~1000 gr/s with 10000-sample grains wants ~208 concurrent grains: a 128 pool must saturate.
    const std::int64_t grainLength = 10000;
    const PoolRun free = simulate(false, grainLength);
    const PoolRun pooled = simulate(true, grainLength);

    // The pool really participated: it filled up and spawns were skipped.
    CHECK(pooled.maxActive == grains::GrainPool::kCapacity);
    CHECK_FALSE(pooled.skippedWhileNotFull);

    // Skipped spawns do not shift later due times: identical schedule with and without a pool.
    REQUIRE(pooled.due.size() == free.due.size());
    CHECK(pooled.due == free.due);

    // Skips happened and freed slots were reused (more accepted spawns than capacity).
    CHECK(pooled.skipped > 0);
    CHECK(pooled.accepted > grains::GrainPool::kCapacity);
    CHECK(pooled.accepted + pooled.skipped == static_cast<int>(pooled.due.size()));

    // Sensitivity: a callback that skips its draw on a full pool desynchronises the shared Rng
    // and changes later due times, so this test would catch it.
    CHECK_FALSE(simulate(true, grainLength, true).due == free.due);
}
