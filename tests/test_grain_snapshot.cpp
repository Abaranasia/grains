// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/GrainSnapshot.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>
#include <grains/WindowTable.h>

#include "support/EngineHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <utility>
#include <vector>

using grains::EngineParams;
using grains::GrainMark;
using grains::GrainPool;
using grains::GrainSnapshot;
using grains::GranularEngine;
using grains::SampleBuffer;

namespace {

constexpr double kSr = 48000.0;
constexpr int kBlock = 512;

static_assert(sizeof(GrainMark) == 16, "a mark is 16 bytes");
static_assert(std::is_trivially_copyable<GrainSnapshot>::value, "the mailbox payload is a plain value");
static_assert(GrainSnapshot{}.marks.size() == static_cast<std::size_t>(GrainPool::kCapacity),
              "one mark per pool slot");
// R-GR-11: snapshotGrains is callable on a const engine and is noexcept.
static_assert(noexcept(std::declval<const GranularEngine&>().snapshotGrains(std::declval<GrainSnapshot&>())),
              "snapshotGrains is const noexcept");

SampleBuffer makeSource(std::uint32_t seed, int frames = 200000)
{
    return SampleBuffer(grains_test::noise(frames, seed), grains_test::noise(frames, seed + 1), kSr);
}

EngineParams longGrains(float density)
{
    EngineParams p;
    p.density = density;
    p.grainSizeMs = 1000.0f;
    p.position = 0.0f;
    return p;
}

void run(GranularEngine& e, int blocks)
{
    std::vector<float> l(kBlock), r(kBlock);
    for (int i = 0; i < blocks; ++i) e.process(l.data(), r.data(), kBlock);
}

GrainSnapshot snap(const GranularEngine& e)
{
    GrainSnapshot s;
    e.snapshotGrains(s);
    return s;
}

bool inUnit(float v) { return std::isfinite(v) && v >= 0.0f && v <= 1.0f; }

} // namespace

TEST_CASE("snapshot: a default snapshot is empty and versioned", "[grain_snapshot]")
{
    const GrainSnapshot s;
    CHECK(s.version == GrainSnapshot::kVersion);
    CHECK(GrainSnapshot::kVersion == 1u);
    CHECK(s.count == 0);
}

TEST_CASE("snapshot: count equals the active grains and every mark is normalised", "[grain_snapshot]")
{
    const SampleBuffer src = makeSource(1);
    GranularEngine e;
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(longGrains(30.0f));
    const grains::WindowTable window;
    int marksSeen = 0;
    for (int b = 0; b < 40; ++b) {
        run(e, 1);
        const GrainSnapshot s = snap(e);
        REQUIRE(s.version == GrainSnapshot::kVersion);
        REQUIRE(s.count == e.activeGrainCount());
        REQUIRE(s.count <= GrainPool::kCapacity);
        for (int i = 0; i < s.count; ++i) {
            const GrainMark& m = s.marks[static_cast<std::size_t>(i)];
            CHECK(inUnit(m.position));
            CHECK(inUnit(m.progress));
            CHECK(m.progress < 1.0f); // finished grains are released inside process()
            CHECK(inUnit(m.level));
            CHECK(m.level == window.at(m.progress));
            CHECK(m.slot < GrainPool::kCapacity);
            ++marksSeen;
        }
    }
    CHECK(marksSeen > 40); // the loops above really ran
}

TEST_CASE("snapshot: marks stay normalised at extreme parameters", "[grain_snapshot]")
{
    const SampleBuffer src = makeSource(2, 48000);
    int marksSeen = 0;
    for (float position : {-1.0f, 0.0f, 1.0f, 2.0f})
        for (float pitch : {-24.0f, 24.0f})
            for (float size : {1.0f, 1000.0f}) {
                GranularEngine e;
                e.prepare(kSr, 3, grains::InitialTransport::Playing);
                e.setSource(&src);
                EngineParams p;
                p.position = position;
                p.positionJitter = 1.0f;
                p.pitchSemitones = pitch;
                p.pitchJitterSemitones = 24.0f;
                p.grainSizeMs = size;
                p.density = 500.0f;
                e.setParams(p);
                for (int b = 0; b < 20; ++b) {
                    run(e, 1);
                    const GrainSnapshot s = snap(e);
                    REQUIRE(s.count == e.activeGrainCount());
                    for (int i = 0; i < s.count; ++i) {
                        const GrainMark& m = s.marks[static_cast<std::size_t>(i)];
                        CHECK(inUnit(m.position));
                        CHECK(inUnit(m.progress));
                        CHECK(inUnit(m.level));
                        CHECK(m.slot < GrainPool::kCapacity);
                        ++marksSeen;
                    }
                }
            }
    CHECK(marksSeen > 0);
}

// A slow grain started near the end of the source ends its life at the last frame: its mark must
// reach the end of the strip without passing it.
TEST_CASE("snapshot: the position clamps at the end of the buffer", "[grain_snapshot]")
{
    const SampleBuffer src = makeSource(3, 48000);
    GranularEngine e;
    e.prepare(kSr, 4, grains::InitialTransport::Playing);
    e.setSource(&src);
    EngineParams p = longGrains(40.0f);
    p.position = 0.9f;
    p.pitchSemitones = -24.0f; // rate 0.25: the grain runs for a long time
    e.setParams(p);
    float maxPos = 0.0f;
    for (int b = 0; b < 60; ++b) {
        run(e, 1);
        const GrainSnapshot s = snap(e);
        for (int i = 0; i < s.count; ++i) {
            const float pos = s.marks[static_cast<std::size_t>(i)].position;
            CHECK(inUnit(pos));
            maxPos = std::max(maxPos, pos);
        }
    }
    CHECK(maxPos > 0.9f);
    CHECK(maxPos <= 1.0f);
}

// No grain can finish within 60 blocks of 1000 ms grains, so the same slots must persist, stay
// distinct and in index order, and advance by exactly one block of progress per block.
TEST_CASE("snapshot: slots are distinct, ordered and stable while a grain lives", "[grain_snapshot]")
{
    const SampleBuffer src = makeSource(4);
    GranularEngine e;
    e.prepare(kSr, 5, grains::InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(longGrains(20.0f));
    const float perBlock = static_cast<float>(kBlock) / 48000.0f; // length is 1000 ms = 48000 samples
    GrainSnapshot prev;
    int checkedPairs = 0;
    for (int b = 0; b < 60; ++b) {
        run(e, 1);
        const GrainSnapshot s = snap(e);
        for (int i = 1; i < s.count; ++i)
            REQUIRE(s.marks[static_cast<std::size_t>(i)].slot > s.marks[static_cast<std::size_t>(i - 1)].slot);
        for (int i = 0; i < prev.count; ++i) {
            const GrainMark& before = prev.marks[static_cast<std::size_t>(i)];
            REQUIRE(s.count > i);
            const GrainMark& after = s.marks[static_cast<std::size_t>(i)];
            CHECK(after.slot == before.slot);
            CHECK_THAT(after.progress - before.progress, Catch::Matchers::WithinAbs(perBlock, 1.0e-5));
            ++checkedPairs;
        }
        prev = s;
    }
    CHECK(prev.count >= 2);
    CHECK(checkedPairs > 20);
}

TEST_CASE("snapshot: no source, an empty source or density 0 give an empty snapshot", "[grain_snapshot]")
{
    GrainSnapshot dirty;
    dirty.count = 5; // stale content must be overwritten, not appended to

    GranularEngine none;
    none.prepare(kSr, 1, grains::InitialTransport::Playing);
    none.snapshotGrains(dirty);
    CHECK(dirty.count == 0);
    CHECK(dirty.version == GrainSnapshot::kVersion);

    const SampleBuffer empty(std::vector<float>{}, std::vector<float>{}, kSr);
    GranularEngine emptySrc;
    emptySrc.prepare(kSr, 1, grains::InitialTransport::Playing);
    emptySrc.setSource(&empty);
    run(emptySrc, 4);
    dirty.count = 5;
    emptySrc.snapshotGrains(dirty);
    CHECK(dirty.count == 0);

    const SampleBuffer src = makeSource(5);
    GranularEngine silent;
    silent.prepare(kSr, 1, grains::InitialTransport::Playing);
    silent.setSource(&src);
    silent.setParams(longGrains(0.0f));
    run(silent, 20);
    dirty.count = 5;
    silent.snapshotGrains(dirty);
    CHECK(dirty.count == 0);
}

TEST_CASE("snapshot: a saturated pool fills every slot exactly once", "[grain_snapshot]")
{
    const SampleBuffer src = makeSource(6);
    GranularEngine e;
    e.prepare(kSr, 2, grains::InitialTransport::Playing);
    e.setSource(&src);
    EngineParams p = longGrains(1000.0f);
    p.position = 0.1f;
    e.setParams(p);
    run(e, 40);
    REQUIRE(e.activeGrainCount() == GrainPool::kCapacity);
    const GrainSnapshot s = snap(e);
    CHECK(s.count == GrainPool::kCapacity);
    for (int i = 0; i < s.count; ++i) CHECK(s.marks[static_cast<std::size_t>(i)].slot == i);
}

TEST_CASE("snapshot: setSource and prepare empty the snapshot", "[grain_snapshot]")
{
    const SampleBuffer a = makeSource(7);
    const SampleBuffer b = makeSource(8);
    GranularEngine e;
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
    e.setSource(&a);
    e.setParams(longGrains(100.0f));

    run(e, 20);
    REQUIRE(snap(e).count > 0);
    e.setSource(&b);
    CHECK(snap(e).count == 0);

    run(e, 20);
    REQUIRE(snap(e).count > 0);
    e.prepare(kSr, 1, grains::InitialTransport::Playing);
    CHECK(snap(e).count == 0);
}

// R-GR-11: observing the engine must not change what it renders. Engine A only renders; engine B
// renders and snapshots (extra snapshots between blocks too). Same seed, params and block partition.
namespace {

struct Run {
    std::vector<float> L, R;
    std::vector<int> active;
};

Run renderBlocks(bool snapshotting, int extraSnapshots, int blocks)
{
    const SampleBuffer src = makeSource(9);
    GranularEngine e;
    e.prepare(kSr, 77, grains::InitialTransport::Playing);
    e.setSource(&src);
    EngineParams p;
    p.density = 60.0f;
    p.grainSizeMs = 120.0f;
    p.positionJitter = 0.3f;
    p.pitchJitterSemitones = 5.0f;
    p.timingJitter = 0.4f;
    p.panSpread = 0.9f;
    e.setParams(p);
    Run out;
    std::vector<float> l(kBlock), r(kBlock);
    GrainSnapshot s;
    for (int b = 0; b < blocks; ++b) {
        e.process(l.data(), r.data(), kBlock);
        out.L.insert(out.L.end(), l.begin(), l.end());
        out.R.insert(out.R.end(), r.begin(), r.end());
        out.active.push_back(e.activeGrainCount());
        if (snapshotting)
            for (int k = 0; k <= extraSnapshots; ++k) e.snapshotGrains(s);
    }
    return out;
}

bool bitIdentical(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

} // namespace

TEST_CASE("snapshot: rendering with a snapshot after every block is bit-identical", "[grain_snapshot][bit_identical]")
{
    const Run a = renderBlocks(false, 0, 400);
    const Run b = renderBlocks(true, 0, 400);
    REQUIRE(a.L.size() == 400u * kBlock);
    CHECK(*std::max_element(a.active.begin(), a.active.end()) > 0); // the render produced grains
    CHECK(bitIdentical(a.L, b.L));
    CHECK(bitIdentical(a.R, b.R));
    CHECK(a.active == b.active);
}

TEST_CASE("snapshot: repeated snapshots between blocks do not alter pool or RNG", "[grain_snapshot][bit_identical]")
{
    const Run a = renderBlocks(false, 0, 400);
    const Run b = renderBlocks(true, 5, 400);
    CHECK(bitIdentical(a.L, b.L));
    CHECK(bitIdentical(a.R, b.R));
    CHECK(a.active == b.active);
}
