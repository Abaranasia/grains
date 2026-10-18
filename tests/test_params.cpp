#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>

#include <limits>

using grains::EngineParams;

namespace {
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
}

TEST_CASE("params: defaults match the design", "[params]")
{
    const EngineParams p;
    CHECK(p.grainSizeMs == 80.0f);
    CHECK(p.density == 20.0f);
    CHECK(p.position == 0.5f);
    CHECK(p.positionJitter == 0.0f);
    CHECK(p.pitchSemitones == 0.0f);
    CHECK(p.pitchJitterSemitones == 0.0f);
    CHECK(p.timingJitter == 0.0f);
    CHECK(p.panSpread == 0.0f);
    CHECK(p.gain == 1.0f);
}

TEST_CASE("params: in-range values pass through unchanged", "[params]")
{
    EngineParams p;
    p.grainSizeMs = 50.0f;
    p.density = 300.0f;
    p.position = 0.25f;
    p.positionJitter = 0.1f;
    p.pitchSemitones = -7.0f;
    p.pitchJitterSemitones = 3.0f;
    p.timingJitter = 0.6f;
    p.panSpread = 0.9f;
    p.gain = 1.5f;
    const EngineParams c = p.clamped();
    CHECK(c.grainSizeMs == 50.0f);
    CHECK(c.density == 300.0f);
    CHECK(c.position == 0.25f);
    CHECK(c.positionJitter == 0.1f);
    CHECK(c.pitchSemitones == -7.0f);
    CHECK(c.pitchJitterSemitones == 3.0f);
    CHECK(c.timingJitter == 0.6f);
    CHECK(c.panSpread == 0.9f);
    CHECK(c.gain == 1.5f);
}

TEST_CASE("params: out-of-range values clamp", "[params]")
{
    EngineParams hi;
    hi.grainSizeMs = 1e9f;
    hi.density = 1e9f;
    hi.position = 2.0f;
    hi.positionJitter = 5.0f;
    hi.pitchSemitones = 100.0f;
    hi.pitchJitterSemitones = 99.0f;
    hi.timingJitter = 5.0f;
    hi.panSpread = 5.0f;
    hi.gain = 9.0f;
    const EngineParams h = hi.clamped();
    CHECK(h.grainSizeMs == 1000.0f);
    CHECK(h.density == 1000.0f);
    CHECK(h.position == 1.0f);
    CHECK(h.positionJitter == 1.0f);
    CHECK(h.pitchSemitones == 24.0f);
    CHECK(h.pitchJitterSemitones == 12.0f);
    CHECK(h.timingJitter == 1.0f);
    CHECK(h.panSpread == 1.0f);
    CHECK(h.gain == 2.0f);

    EngineParams lo;
    lo.grainSizeMs = 0.0f;
    lo.density = -3.0f;
    lo.position = -1.0f;
    lo.positionJitter = -1.0f;
    lo.pitchSemitones = -100.0f;
    lo.pitchJitterSemitones = -4.0f;
    lo.timingJitter = -1.0f;
    lo.panSpread = -1.0f;
    lo.gain = -1.0f;
    const EngineParams l = lo.clamped();
    CHECK(l.grainSizeMs == 1.0f);
    CHECK(l.density == 0.0f); // 0 means no spawns
    CHECK(l.position == 0.0f);
    CHECK(l.positionJitter == 0.0f);
    CHECK(l.pitchSemitones == -24.0f);
    CHECK(l.pitchJitterSemitones == 0.0f);
    CHECK(l.timingJitter == 0.0f);
    CHECK(l.panSpread == 0.0f);
    CHECK(l.gain == 0.0f);
}

TEST_CASE("params: NaN falls back to the default", "[params]")
{
    EngineParams p;
    p.grainSizeMs = kNaN;
    p.density = kNaN;
    p.position = kNaN;
    p.positionJitter = kNaN;
    p.pitchSemitones = kNaN;
    p.pitchJitterSemitones = kNaN;
    p.timingJitter = kNaN;
    p.panSpread = kNaN;
    p.gain = kNaN;
    const EngineParams c = p.clamped();
    const EngineParams d;
    CHECK(c.grainSizeMs == d.grainSizeMs);
    CHECK(c.density == d.density);
    CHECK(c.position == d.position);
    CHECK(c.positionJitter == d.positionJitter);
    CHECK(c.pitchSemitones == d.pitchSemitones);
    CHECK(c.pitchJitterSemitones == d.pitchJitterSemitones);
    CHECK(c.timingJitter == d.timingJitter);
    CHECK(c.panSpread == d.panSpread);
    CHECK(c.gain == d.gain);
}

TEST_CASE("params: pitch 100 clamps to the same value as 24", "[params]")
{
    EngineParams a, b;
    a.pitchSemitones = 100.0f;
    b.pitchSemitones = 24.0f;
    CHECK(a.clamped().pitchSemitones == b.clamped().pitchSemitones);
}
