// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/GranularEngine.h>
#include <grains/EngineParams.h>
#include <grains/GrainPool.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::InitialTransport;
using grains::SampleBuffer;
using grains::TransportGate;
using grains_test::allFinite;
using grains_test::allZero;
using grains_test::noise;
using grains_test::Out;
using grains_test::render;
using State = TransportGate::State;

namespace {

constexpr double kSr = 48000.0;
constexpr int kLen = 960; // 0.020 s at 48 kHz

EngineParams busyParams()
{
    EngineParams p;
    p.density = 180.0f;
    p.grainSizeMs = 35.0f;
    p.position = 0.4f;
    p.positionJitter = 0.6f;
    p.pitchJitterSemitones = 5.0f;
    p.timingJitter = 0.5f;
    p.panSpread = 0.8f;
    return p;
}

void setup(GranularEngine& e, const SampleBuffer& src, InitialTransport t, std::uint64_t seed = 11)
{
    e.prepare(kSr, seed, t);
    e.setSource(&src);
    e.setParams(busyParams());
}

bool bitEqual(float a, float b)
{
    return std::memcmp(&a, &b, sizeof a) == 0;
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        if (!bitEqual(a[i], b[i])) return false;
    return true;
}

bool allPlusZero(const std::vector<float>& v, std::size_t from)
{
    for (std::size_t i = from; i < v.size(); ++i)
        if (!bitEqual(v[i], 0.0f)) return false;
    return true;
}

// Renders n samples in blocks of `block`, appending to out.
void renderInto(GranularEngine& e, Out& out, int n, int block = 128)
{
    for (int done = 0; done < n; done += block) {
        const int b = std::min(block, n - done);
        std::vector<float> l(static_cast<std::size_t>(b)), r(static_cast<std::size_t>(b));
        e.process(l.data(), r.data(), b);
        out.L.insert(out.L.end(), l.begin(), l.end());
        out.R.insert(out.R.end(), r.begin(), r.end());
    }
}

// Renders blocks until the engine reports Stopped at the end of a call (bounded).
void renderUntilStopped(GranularEngine& e, Out& out, int block = 128)
{
    for (int guard = 0; guard < 100 && e.transportState() != State::Stopped; ++guard) renderInto(e, out, block, block);
}

} // namespace

TEST_CASE("engine_transport: prepare defaults to Stopped and renders exact zeros", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    e.prepare(kSr, 11);
    e.setSource(&src);
    e.setParams(busyParams());
    CHECK(e.transportState() == State::Stopped);
    const Out o = render(e, 4800, 256);
    CHECK(allPlusZero(o.L, 0));
    CHECK(allPlusZero(o.R, 0));
    CHECK(e.activeGrainCount() == 0);
}

TEST_CASE("engine_transport: a stopped engine draws no random numbers and renders no grains", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    setup(e, src, InitialTransport::Stopped);
    const std::uint64_t before = e.rngState();
    Out o;
    renderInto(e, o, 48000, 512);
    CHECK(e.rngState() == before);
    CHECK(e.activeGrainCount() == 0);
    CHECK(allPlusZero(o.L, 0));
}

TEST_CASE("engine_transport: Play fades in from exactly zero over the ramp length", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    setup(e, src, InitialTransport::Stopped);
    e.setPlaying(true);
    Out o;
    renderInto(e, o, kLen - 1, kLen - 1);
    CHECK(e.transportState() == State::Starting);
    renderInto(e, o, 1, 1);
    CHECK(e.transportState() == State::Playing);
    CHECK(bitEqual(o.L[0], 0.0f));
    CHECK(bitEqual(o.R[0], 0.0f));
    CHECK(grains_test::peak(o) > 0.0f);
}

TEST_CASE("engine_transport: Play without a source neither sounds nor advances the ramp", "[engine_transport]")
{
    GranularEngine e;
    e.prepare(kSr, 11);
    e.setPlaying(true);
    const Out o = render(e, 8 * kLen, 256);
    CHECK(allZero(o));
    CHECK(e.transportState() == State::Starting);
}

TEST_CASE("engine_transport: Stop fades to +0, empties the pool and stays silent", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    setup(e, src, InitialTransport::Playing);
    Out warm;
    renderInto(e, warm, 4800);
    REQUIRE(e.activeGrainCount() > 0);
    e.setPlaying(false);
    Out o;
    renderInto(e, o, kLen + 128, 128); // the ramp completes within kLen samples
    CHECK(e.transportState() == State::Stopped);
    CHECK(e.activeGrainCount() == 0);
    CHECK(allPlusZero(o.L, kLen));
    CHECK(allPlusZero(o.R, kLen));
    const std::uint64_t rng = e.rngState();
    Out later;
    renderInto(e, later, 24000, 512);
    CHECK(allPlusZero(later.L, 0));
    CHECK(allPlusZero(later.R, 0));
    CHECK(e.activeGrainCount() == 0); // reset once; nothing is scheduled afterwards
    CHECK(e.rngState() == rng);
}

TEST_CASE("engine_transport: a redundant Play is a no-op", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    for (const InitialTransport t : {InitialTransport::Playing, InitialTransport::Stopped}) {
        GranularEngine a, b;
        setup(a, src, t);
        setup(b, src, t);
        a.setPlaying(true);
        b.setPlaying(true);
        Out oa, ob;
        for (int i = 0; i < 60; ++i) {
            a.setPlaying(true); // repeated every block: no reseed, no reset
            renderInto(a, oa, 128, 128);
            renderInto(b, ob, 128, 128);
        }
        CHECK(bitEqual(oa.L, ob.L));
        CHECK(bitEqual(oa.R, ob.R));
        CHECK(grains_test::peak(oa) > 0.0f);
    }
}

TEST_CASE("engine_transport: Play during Stopping finishes the fade, then restarts fresh", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    setup(e, src, InitialTransport::Stopped);
    e.setPlaying(true);
    Out first;
    renderInto(e, first, 4000);
    const Out reference{std::vector<float>(first.L.begin(), first.L.begin() + 3000),
                        std::vector<float>(first.R.begin(), first.R.begin() + 3000)};

    e.setPlaying(false);
    Out fade;
    renderInto(e, fade, 256, 128); // mid fade-out
    REQUIRE(e.transportState() == State::Stopping);
    e.setPlaying(true); // latched, no reversal
    renderUntilStopped(e, fade);
    CHECK(e.transportState() == State::Stopped);
    CHECK(e.activeGrainCount() == 0);
    CHECK(allPlusZero(fade.L, static_cast<std::size_t>(kLen)));

    Out second; // the next call restarts from scratch
    renderInto(e, second, 3000);
    CHECK(bitEqual(second.L, reference.L));
    CHECK(bitEqual(second.R, reference.R));
}

// R-TG-2: a Play from Stopped is the ungated stream with a 0 -> 1 ramp on top, and past the ramp it
// is bit-identical to an engine that started Playing.
TEST_CASE("engine_transport: steady output is bit-identical to an engine started Playing", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine a, b;
    setup(a, src, InitialTransport::Playing);
    setup(b, src, InitialTransport::Stopped);
    b.setPlaying(true);
    const int total = 6 * kLen;
    Out oa, ob;
    renderInto(a, oa, total);
    renderInto(b, ob, total);
    bool attenuated = false;
    for (std::size_t i = 0; i < static_cast<std::size_t>(total); ++i) {
        if (i >= static_cast<std::size_t>(kLen)) {
            REQUIRE(bitEqual(ob.L[i], oa.L[i]));
            REQUIRE(bitEqual(ob.R[i], oa.R[i]));
        } else {
            REQUIRE(std::abs(ob.L[i]) <= std::abs(oa.L[i]));
            REQUIRE(std::abs(ob.R[i]) <= std::abs(oa.R[i]));
            attenuated = attenuated || std::abs(ob.L[i]) < std::abs(oa.L[i]);
        }
    }
    CHECK(attenuated); // the ramp really shaped the first samples
}

// R-TG-4: every Play, after any history and also a Play latched during Stopping, repeats the first.
TEST_CASE("engine_transport: every Play reproduces the first Play bit-exactly", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    constexpr int kN = 5000;
    GranularEngine e;
    setup(e, src, InitialTransport::Stopped);
    e.setPlaying(true);
    Out first;
    renderInto(e, first, kN);
    REQUIRE(grains_test::peak(first) > 0.0f);

    // History: play on, change nothing but time, stop, idle, play again.
    Out scratch;
    renderInto(e, scratch, 10000);
    e.setPlaying(false);
    renderUntilStopped(e, scratch);
    renderInto(e, scratch, 5000);
    e.setPlaying(true);
    Out second;
    renderInto(e, second, kN);
    CHECK(bitEqual(second.L, first.L));
    CHECK(bitEqual(second.R, first.R));

    // Stop mid fade-in, then Play latched during the fade-out.
    renderInto(e, scratch, 700);
    e.setPlaying(false);
    renderInto(e, scratch, 256);
    REQUIRE(e.transportState() == State::Stopping);
    e.setPlaying(true);
    renderUntilStopped(e, scratch);
    Out third;
    renderInto(e, third, kN);
    CHECK(bitEqual(third.L, first.L));
    CHECK(bitEqual(third.R, first.R));
}

// R-TG-6: the gate runs before the limiter, so ramps cannot exceed the ceiling even with the DC worst case.
TEST_CASE("engine_transport: output stays below 1 through repeated Play and Stop at the worst case", "[engine_transport]")
{
    const SampleBuffer dc(std::vector<float>(200000, 1.0f), {}, kSr);
    GranularEngine e;
    e.prepare(kSr, 1);
    e.setSource(&dc);
    EngineParams p;
    p.density = 200.0f;
    p.grainSizeMs = 1000.0f;
    p.position = 0.0f;
    p.gain = 2.0f;
    e.setParams(p);
    float peak = 0.0f;
    for (int cycle = 0; cycle < 6; ++cycle) {
        e.setPlaying(true);
        Out o;
        renderInto(e, o, 50000, 256);
        e.setPlaying(false);
        renderInto(e, o, 3000, 256);
        REQUIRE(allFinite(o));
        peak = std::max(peak, grains_test::peak(o));
        REQUIRE(grains_test::peak(o) < GranularEngine::kOutputCeiling);
    }
    CHECK(peak > GranularEngine::kLimiterKnee); // the limiter was actually engaged
}

TEST_CASE("engine_transport: stopNow silences the next block with no fade", "[engine_transport]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    GranularEngine e;
    setup(e, src, InitialTransport::Playing);
    Out warm;
    renderInto(e, warm, 4800);
    REQUIRE(e.activeGrainCount() > 0);
    e.stopNow();
    CHECK(e.transportState() == State::Stopped);
    CHECK(e.activeGrainCount() == 0);
    const Out o = render(e, 512);
    CHECK(allPlusZero(o.L, 0));
    CHECK(allPlusZero(o.R, 0));
    CHECK(e.transportState() == State::Stopped); // the target was cleared, so nothing restarts
}

TEST_CASE("engine_transport: setSource leaves the transport alone", "[engine_transport]")
{
    const SampleBuffer a(noise(40000, 1), noise(40000, 2), kSr);
    const SampleBuffer b(noise(30000, 3), noise(30000, 4), kSr);
    GranularEngine e;
    setup(e, a, InitialTransport::Playing);
    e.setSource(&b);
    CHECK(e.transportState() == State::Playing);
    Out o;
    renderInto(e, o, 4800);
    CHECK(grains_test::peak(o) > 0.0f);
    e.setPlaying(false);
    e.setSource(&a);
    CHECK(e.transportState() == State::Playing); // still unchanged until the next process()
}

namespace {

// Renders total samples in blocks drawn cyclically from `sizes`. Play is requested before sample 0
// and Stop before sample stopAt; blocks never straddle stopAt, so the toggle lands on a block start
// in every partition (R-TG-7).
Out renderWithToggle(const SampleBuffer& src, int total, int stopAt, const std::vector<int>& sizes)
{
    GranularEngine e;
    setup(e, src, InitialTransport::Stopped);
    e.setPlaying(true);
    Out out;
    int done = 0;
    std::size_t k = 0;
    while (done < total) {
        if (done == stopAt) e.setPlaying(false);
        int b = sizes[k++ % sizes.size()];
        b = std::min(b, total - done);
        if (done < stopAt) b = std::min(b, stopAt - done);
        renderInto(e, out, b, b);
        done += b;
    }
    return out;
}

} // namespace

// R-GR-6 / R-TG-7: the same Play at 0 and Stop at S give bit-identical output for any block split.
TEST_CASE("engine_transport: block splits across a Play and Stop toggle are bit-identical", "[engine_transport][block_split]")
{
    const SampleBuffer src(noise(40000, 1), noise(40000, 2), kSr);
    constexpr int kStopAt = 12000;
    constexpr int kTotal = kStopAt + 6000; // the fade-out and the exact-zero tail
    const Out whole = renderWithToggle(src, kTotal, kStopAt, {kTotal});
    REQUIRE(grains_test::peak(whole) > 0.0f);
    REQUIRE(allPlusZero(whole.L, static_cast<std::size_t>(kStopAt + kLen)));
    const std::vector<std::vector<int>> splits = {{64}, {128}, {1}, {37, 101, 509, 3}, {4096, 1, 255}};
    for (const std::vector<int>& sizes : splits) {
        const Out o = renderWithToggle(src, kTotal, kStopAt, sizes);
        CHECK(bitEqual(o.L, whole.L));
        CHECK(bitEqual(o.R, whole.R));
    }
}
