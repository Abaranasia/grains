// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GrainSnapshot.h>
#include <grains/GranularEngine.h>
#include <grains/Rng.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>

using grains::EngineParams;
using grains::GrainSnapshot;
using grains::GranularEngine;
using grains::InitialTransport;
using grains::SampleBuffer;
using grains_test::noise;
using Catch::Approx;

namespace {

constexpr double kSr = 48000.0;

// FNV-1a over raw bytes: the golden value must not depend on a float-to-text conversion.
struct Fnv1a {
    std::uint64_t h = 1469598103934665603ULL;
    void bytes(const void* p, std::size_t n)
    {
        const auto* b = static_cast<const unsigned char*>(p);
        for (std::size_t i = 0; i < n; ++i) {
            h ^= b[i];
            h *= 1099511628211ULL;
        }
    }
    template <class T>
    void add(T v)
    {
        bytes(&v, sizeof v);
    }
};

// Hashes what the display sees of every grain (slot and normalised head position) after each block
// of `block` samples over `total` samples. The head is start + rate * age with rate exactly 1 (pitch
// 0, equal rates), and the start comes from the rng draws, + - * / only: the value is independent of
// libm. The window and the pan law (cos, sin) never enter it.
std::uint64_t positionsDigest(const EngineParams& p, int total, int block)
{
    const SampleBuffer src(noise(60001, 3), noise(60001, 4), kSr);
    GranularEngine e;
    e.prepare(kSr, 21, InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(p);
    std::vector<float> l(static_cast<std::size_t>(block)), r(static_cast<std::size_t>(block));
    GrainSnapshot snap;
    Fnv1a f;
    for (int done = 0; done < total; done += block) {
        e.process(l.data(), r.data(), block);
        e.snapshotGrains(snap);
        f.add(snap.count);
        for (int i = 0; i < snap.count; ++i) {
            f.add(snap.marks[static_cast<std::size_t>(i)].slot);
            f.add(snap.marks[static_cast<std::size_t>(i)].position);
        }
    }
    return f.h;
}

EngineParams goldenParams(float position)
{
    EngineParams p;
    p.density = 60.0f;
    p.grainSizeMs = 30.0f;
    p.position = position;
    p.positionJitter = 0.4f;
    p.timingJitter = 0.3f;
    return p;
}

// A source of kFrames frames at the engine rate (pitch 0 then reads at exactly rate 1), and a
// scan-friendly parameter set: density 2 gr/s spawns on a known grid (every sr/2 samples), 50 ms
// grains never overlap, and a jitter-free spawn start is exactly position + offset.
constexpr int kFrames = 100001;
constexpr double kLast = kFrames - 1;

EngineParams gridParams(float position, float scan, float jitter = 0.0f)
{
    EngineParams p;
    p.density = 2.0f;
    p.grainSizeMs = 50.0f;
    p.position = position;
    p.positionJitter = jitter;
    p.scan = scan;
    return p;
}

// Drives an engine on the spawn grid and reads back where the newest grain started.
class Rig {
public:
    explicit Rig(const EngineParams& p, double sr = kSr, std::uint64_t seed = 5)
        : src_(noise(kFrames, 7), noise(kFrames, 8), sr)
    {
        e_.prepare(sr, seed, InitialTransport::Playing);
        e_.setSource(&src_);
        e_.setParams(p);
    }

    GranularEngine& engine() { return e_; }
    int now() const { return now_; }

    void advanceTo(int sample)
    {
        std::vector<float> l(512), r(512);
        while (now_ < sample) {
            const int b = std::min(512, sample - now_);
            e_.process(l.data(), r.data(), b);
            now_ += b;
        }
    }

    // Start of the grain spawned at output sample `sample` (must be a grid sample >= now()), as a
    // fraction of the source length. The grain has rendered exactly one sample.
    double startAt(int sample)
    {
        advanceTo(sample);
        advanceTo(sample + 1);
        GrainSnapshot snap;
        e_.snapshotGrains(snap);
        REQUIRE(snap.count >= 1);
        int newest = 0;
        for (int i = 1; i < snap.count; ++i)
            if (snap.marks[static_cast<std::size_t>(i)].progress < snap.marks[static_cast<std::size_t>(newest)].progress) newest = i;
        return (static_cast<double>(snap.marks[static_cast<std::size_t>(newest)].position) * kLast - 1.0) / kLast;
    }

private:
    SampleBuffer src_;
    GranularEngine e_;
    int now_ = 0;
};

// Tolerance of one source sample is 1e-5; the float head position adds only ~1e-7.
constexpr double kTol = 1.0e-6;

} // namespace

// Characterisation, captured on the base commit before any scan code existed (R-GR-12 "Zero scan is
// identical to today"). position 1.0 is the case a naive wrap would break.
TEST_CASE("scan: zero scan reproduces the pre-scan grain start positions", "[scan][golden]")
{
    CHECK(positionsDigest(goldenParams(1.0f), 48000, 64) == 0x6308479d6279ead8ULL);
    CHECK(positionsDigest(goldenParams(0.3f), 48000, 64) == 0x9b496ff5a4d267fdULL);
}

// R-GR-12 "Offset advances at the stated rate": offset = scan * elapsed / sampleRate.
TEST_CASE("scan: the offset is scan * 1 s after one second at any sample rate", "[scan]")
{
    for (const double sr : {44100.0, 48000.0, 96000.0}) {
        INFO("sr=" << sr);
        Rig rig(gridParams(0.5f, 0.25f), sr);
        CHECK(rig.engine().scanOffset() == 0.0);
        rig.advanceTo(static_cast<int>(sr));
        CHECK(rig.engine().scanOffset() == Approx(0.25).margin(1e-12));
    }
    Rig neg(gridParams(0.5f, -0.5f));
    neg.advanceTo(24000);
    CHECK(neg.engine().scanOffset() == Approx(-0.25).margin(1e-12));
}

TEST_CASE("scan: grains start at anchor + scan * t / sampleRate", "[scan]")
{
    Rig rig(gridParams(0.0f, 0.5f)); // anchor 0, jitter 0, +0.5 length/s
    CHECK(rig.startAt(0) == Approx(0.0).margin(kTol));
    CHECK(rig.startAt(24000) == Approx(0.25).margin(kTol));
    CHECK(rig.startAt(48000) == Approx(0.5).margin(kTol));
    CHECK(rig.startAt(72000) == Approx(0.75).margin(kTol));
}

TEST_CASE("scan: a negative scan moves backwards from the anchor", "[scan]")
{
    Rig rig(gridParams(0.5f, -0.25f));
    CHECK(rig.startAt(0) == Approx(0.5).margin(kTol));
    CHECK(rig.startAt(48000) == Approx(0.25).margin(kTol)); // 1 s: 0.5 - 0.25
}

namespace {

struct Pcm {
    std::vector<float> l, r;
};

// Renders `total` samples with blocks drawn cyclically from `sizes` (engine params fixed up front).
Pcm renderSplit(const EngineParams& p, int total, const std::vector<int>& sizes)
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    GranularEngine e;
    e.prepare(kSr, 9, InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(p);
    Pcm out;
    int done = 0;
    std::size_t k = 0;
    while (done < total) {
        const int b = std::min(sizes[k++ % sizes.size()], total - done);
        std::vector<float> l(static_cast<std::size_t>(b)), r(static_cast<std::size_t>(b));
        e.process(l.data(), r.data(), b);
        out.l.insert(out.l.end(), l.begin(), l.end());
        out.r.insert(out.r.end(), r.begin(), r.end());
        done += b;
    }
    return out;
}

bool bitEqual(const std::vector<float>& a, const std::vector<float>& b)
{
    return a.size() == b.size() && (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0);
}

EngineParams busyScan(float scan)
{
    EngineParams p;
    p.density = 90.0f;
    p.grainSizeMs = 40.0f;
    p.position = 0.2f;
    p.positionJitter = 0.1f;
    p.timingJitter = 0.4f;
    p.panSpread = 0.6f;
    p.scan = scan;
    return p;
}

} // namespace

// R-GR-6 / R-GR-12 "Block-split and sample-rate independence".
TEST_CASE("scan: a non-zero scan is bit-identical for any block split", "[scan][block_split]")
{
    const Pcm whole = renderSplit(busyScan(0.37f), 48000, {48000});
    const Pcm plain = renderSplit(busyScan(0.0f), 48000, {48000});
    REQUIRE_FALSE(bitEqual(whole.l, plain.l)); // the scan really moved the read point
    for (const std::vector<int>& sizes : {std::vector<int>{512}, {37}, {1}, {37, 101, 509, 3}}) {
        const Pcm o = renderSplit(busyScan(0.37f), 48000, sizes);
        CHECK(bitEqual(o.l, whole.l));
        CHECK(bitEqual(o.r, whole.r));
    }
}

TEST_CASE("scan: grain starts agree across sample rates at equal elapsed seconds", "[scan]")
{
    // Density 2 puts a spawn exactly at 1 s at every rate.
    Rig lo(gridParams(0.2f, 0.37f), 44100.0);
    Rig mid(gridParams(0.2f, 0.37f), 48000.0);
    Rig hi(gridParams(0.2f, 0.37f), 96000.0);
    const double a = lo.startAt(44100);
    const double b = mid.startAt(48000);
    const double c = hi.startAt(96000);
    CHECK(a == Approx(0.57).margin(kTol));
    CHECK(b == Approx(a).margin(1.0 / 44100.0)); // within one sample at the lower rate
    CHECK(c == Approx(a).margin(1.0 / 44100.0));
}

// R-GR-12 "Wrap at both ends".
TEST_CASE("scan: the read point wraps around both source ends", "[scan][wrap]")
{
    struct Case {
        float anchor, scan;
        double expected;
    };
    const Case cases[] = {{0.9f, 1.0f, 0.4},   // 0.9 + 0.5
                          {0.05f, -1.0f, 0.55}, // 0.05 - 0.5
                          {0.75f, 1.0f, 0.25},  // 1.25
                          {0.25f, -1.0f, 0.75}}; // -0.25
    for (const Case& c : cases) {
        INFO("anchor=" << c.anchor << " scan=" << c.scan);
        Rig rig(gridParams(c.anchor, c.scan));
        CHECK(rig.startAt(24000) == Approx(c.expected).margin(kTol)); // 0.5 s
    }
}

// R-GR-12 "Wrap precedes jitter clamp": start = clamp01(fold(anchor + offset) + jitter * draw). The
// oracle replays the engine's rng: per spawn it draws pos, pitch, pan, then the scheduler draws one
// value for the next interval, so the position draw of spawn k is the 4k-th value.
TEST_CASE("scan: the wrap happens before the jitter clamp", "[scan][wrap]")
{
    constexpr float kAnchor = 0.95f;
    constexpr float kJitter = 0.5f;
    constexpr std::uint64_t kSeed = 5;
    constexpr int kPeriod = 2400; // density 20 at 48 kHz
    EngineParams p = gridParams(kAnchor, 1.0f, kJitter);
    p.density = 20.0f;
    Rig rig(p, kSr, kSeed);
    grains::Rng rng(kSeed);
    int clampedAtSeam = 0;
    int wrapped = 0;
    for (int k = 0; k < 20; ++k) {
        const float draw = rng.bipolar();
        rng.bipolar();
        rng.bipolar();
        rng.bipolar();
        const double sum = static_cast<double>(kAnchor) + static_cast<double>(k * kPeriod) / kSr;
        const double folded = (sum >= 0.0 && sum <= 1.0) ? sum : sum - std::floor(sum);
        const float raw = static_cast<float>(folded) + kJitter * draw;
        double expected = raw < 0.0f ? 0.0 : (raw > 1.0f ? 1.0 : static_cast<double>(raw));
        // spawn() moves a start within 2 samples of the last frame back so that the grain fits.
        expected = std::min(expected, (kLast - 2.0) / kLast);
        if (sum > 1.0) ++wrapped;
        if (sum > 1.0 && raw < 0.0f) ++clampedAtSeam;
        INFO("k=" << k << " sum=" << sum);
        const double got = rig.startAt(k * kPeriod);
        CHECK(got >= -kTol);
        CHECK(got <= 1.0 + kTol);
        CHECK(got == Approx(expected).margin(kTol));
    }
    CHECK(wrapped > 5);       // the run really crosses the seam
    CHECK(clampedAtSeam > 0); // and at least one jitter draw reaches below 0 there: clamped, not wrapped
}

// R-GR-12 "Scan change is continuous": a changed scan only affects the time after the change.
TEST_CASE("scan: the offset is continuous across a scan change", "[scan][rebase]")
{
    Rig rig(gridParams(0.25f, 0.5f));
    rig.advanceTo(48000); // 1 s
    const double before = rig.engine().scanOffset();
    REQUIRE(before == Approx(0.5).margin(1e-12));
    EngineParams p = gridParams(0.25f, -0.5f);
    rig.engine().setParams(p);
    CHECK(rig.engine().scanOffset() == before); // no jump at the change
    rig.advanceTo(72000);                       // 0.5 s later at -0.5
    CHECK(rig.engine().scanOffset() == Approx(0.25).margin(1e-12));
    // The grain spawned at 1.5 s starts 0.25 of a source length behind the one at 1 s, i.e. where
    // anchor 0.25 + 0.25 puts it.
    CHECK(rig.startAt(72000) == Approx(0.5).margin(kTol));
}

TEST_CASE("scan: a rebase keeps the offset when the change lands on the wrap", "[scan][rebase]")
{
    Rig rig(gridParams(0.5f, 1.0f));
    rig.advanceTo(72000); // offset 1.5, past one full length
    rig.engine().setParams(gridParams(0.5f, 0.25f));
    // The base is kept modulo 1, so the read point (anchor + offset folded) is unchanged.
    const double off = rig.engine().scanOffset();
    CHECK(off - std::floor(off) == Approx(0.5).margin(1e-12));
}

// R-GR-12 "Offset holds when scan returns to 0".
TEST_CASE("scan: the offset holds when scan returns to 0", "[scan][rebase]")
{
    Rig rig(gridParams(0.25f, 0.5f));
    rig.advanceTo(48000);
    rig.engine().setParams(gridParams(0.25f, 0.0f));
    CHECK(rig.engine().scanOffset() == Approx(0.5).margin(1e-12));
    rig.advanceTo(96000);
    CHECK(rig.engine().scanOffset() == Approx(0.5).margin(1e-12)); // still held a second later
    CHECK(rig.startAt(96000) == Approx(0.75).margin(kTol));         // fold(0.25 + 0.5), not the anchor
    // Applying the same scan twice is not a change.
    rig.engine().setParams(gridParams(0.25f, 0.0f));
    CHECK(rig.engine().scanOffset() == Approx(0.5).margin(1e-12));
}

namespace {

// Scan 0.5 length/s on the busy parameter set; Stopped after prepare so Play paths are reachable.
void prepareStopped(GranularEngine& e, const SampleBuffer& src, std::uint64_t seed = 9)
{
    e.prepare(kSr, seed); // Stopped
    e.setSource(&src);
    e.setParams(busyScan(0.5f));
}

void renderBlocks(GranularEngine& e, int total, int block = 128)
{
    std::vector<float> l(static_cast<std::size_t>(block)), r(static_cast<std::size_t>(block));
    for (int done = 0; done < total; done += block) e.process(l.data(), r.data(), std::min(block, total - done));
}

Pcm renderCapture(GranularEngine& e, int total, int block = 128)
{
    Pcm out;
    for (int done = 0; done < total; done += block) {
        const int b = std::min(block, total - done);
        std::vector<float> l(static_cast<std::size_t>(b)), r(static_cast<std::size_t>(b));
        e.process(l.data(), r.data(), b);
        out.l.insert(out.l.end(), l.begin(), l.end());
        out.r.insert(out.r.end(), r.begin(), r.end());
    }
    return out;
}

} // namespace

// R-TG-4 "Play resets": a Stopped engine neither advances the offset nor keeps one across a stop.
TEST_CASE("scan: Play starts from offset 0 and a completed stop resets it", "[scan][reset]")
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    GranularEngine e;
    prepareStopped(e, src);
    renderBlocks(e, 48000); // Stopped: nothing advances
    CHECK(e.scanOffset() == 0.0);
    e.setPlaying(true);
    const Pcm first = renderCapture(e, 48000);
    CHECK(e.scanOffset() == Approx(0.5).margin(1e-12)); // exactly the time since Play
    e.setPlaying(false);
    renderBlocks(e, 4800); // the ~20 ms fade-out completes inside
    REQUIRE(e.transportState() == grains::TransportGate::State::Stopped);
    CHECK(e.scanOffset() == 0.0);
    renderBlocks(e, 48000); // idle: still 0
    CHECK(e.scanOffset() == 0.0);
    e.setPlaying(true);
    const Pcm second = renderCapture(e, 48000);
    CHECK(bitEqual(second.l, first.l)); // the offset restarted with the rng and the pool
    CHECK(bitEqual(second.r, first.r));
}

TEST_CASE("scan: a Play latched during the fade-out restarts the offset too", "[scan][reset]")
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    GranularEngine e;
    prepareStopped(e, src);
    e.setPlaying(true);
    const Pcm first = renderCapture(e, 20000);
    renderBlocks(e, 3000);
    e.setPlaying(false);
    renderBlocks(e, 256);
    REQUIRE(e.transportState() == grains::TransportGate::State::Stopping);
    e.setPlaying(true); // latched: no reversal
    for (int guard = 0; guard < 100 && e.transportState() != grains::TransportGate::State::Stopped; ++guard)
        renderBlocks(e, 128); // the fade-out completes; the restart happens at the start of the next call
    REQUIRE(e.transportState() == grains::TransportGate::State::Stopped);
    const Pcm again = renderCapture(e, 20000);
    CHECK(bitEqual(again.l, first.l));
    CHECK(bitEqual(again.r, first.r));
}

TEST_CASE("scan: stopNow, setSource and prepare reset the offset", "[scan][reset]")
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    const SampleBuffer other(noise(kFrames, 17), noise(kFrames, 18), kSr);
    GranularEngine e;
    e.prepare(kSr, 9, InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(busyScan(0.5f));

    renderBlocks(e, 48000);
    REQUIRE(e.scanOffset() == Approx(0.5).margin(1e-12));
    e.stopNow();
    CHECK(e.scanOffset() == 0.0);

    e.setPlaying(true);
    renderBlocks(e, 24000);
    REQUIRE(e.scanOffset() == Approx(0.25).margin(1e-12));
    e.setSource(&other); // a new load restarts the scan; the transport keeps playing
    CHECK(e.scanOffset() == 0.0);
    CHECK(e.transportState() == grains::TransportGate::State::Playing);
    renderBlocks(e, 12000);
    CHECK(e.scanOffset() == Approx(0.125).margin(1e-12)); // counts from the load, not from Play

    e.prepare(kSr, 9, InitialTransport::Playing);
    CHECK(e.scanOffset() == 0.0);
    renderBlocks(e, 24000);
    CHECK(e.scanOffset() == Approx(0.25).margin(1e-12)); // the stored scan survives prepare
}

TEST_CASE("scan: a reset keeps the stored scan and a held offset is dropped", "[scan][reset]")
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    GranularEngine e;
    e.prepare(kSr, 9, InitialTransport::Playing);
    e.setSource(&src);
    e.setParams(busyScan(0.5f));
    renderBlocks(e, 48000);
    e.setParams(busyScan(0.0f)); // offset held at 0.5
    REQUIRE(e.scanOffset() == Approx(0.5).margin(1e-12));
    e.stopNow();
    CHECK(e.scanOffset() == 0.0); // reset beats hold
    e.setParams(busyScan(0.5f));
    e.setPlaying(true);
    renderBlocks(e, 24000);
    CHECK(e.scanOffset() == Approx(0.25).margin(1e-12));
}

// R-TG-4 "Redundant Play keeps the offset".
TEST_CASE("scan: a redundant Play does not reset the offset", "[scan][reset]")
{
    const SampleBuffer src(noise(kFrames, 7), noise(kFrames, 8), kSr);
    GranularEngine a, b;
    prepareStopped(a, src);
    prepareStopped(b, src);
    a.setPlaying(true);
    b.setPlaying(true);
    Pcm oa, ob;
    for (int i = 0; i < 375; ++i) { // 1 s in 128-sample blocks
        a.setPlaying(true);         // requested every block
        const Pcm x = renderCapture(a, 128);
        const Pcm y = renderCapture(b, 128);
        oa.l.insert(oa.l.end(), x.l.begin(), x.l.end());
        ob.l.insert(ob.l.end(), y.l.begin(), y.l.end());
    }
    CHECK(a.scanOffset() == Approx(0.5 * 375 * 128 / 48000.0).margin(1e-12));
    CHECK(a.scanOffset() == b.scanOffset());
    CHECK(bitEqual(oa.l, ob.l));
}
