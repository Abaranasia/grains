// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/EngineParams.h>
#include <grains/GranularEngine.h>
#include <grains/SampleBuffer.h>

#include "support/EngineHelpers.h"

#include <cstdint>
#include <vector>

using grains::EngineParams;
using grains::GranularEngine;
using grains::SampleBuffer;
using grains_test::allZero;
using grains_test::noise;
using grains_test::Out;
using grains_test::render;

namespace {

constexpr double kSr = 48000.0;

// Every random source (position, pitch, pan, timing) is active, so any hidden state or
// block-size dependence shows up in the output.
EngineParams busyParams()
{
    EngineParams p;
    p.density = 180.0f;
    p.grainSizeMs = 35.0f;
    p.position = 0.4f;
    p.positionJitter = 0.6f;
    p.pitchSemitones = 3.0f;
    p.pitchJitterSemitones = 7.0f;
    p.timingJitter = 0.5f;
    p.panSpread = 0.8f;
    return p;
}

void setup(GranularEngine& e, const SampleBuffer& src, std::uint64_t seed)
{
    e.prepare(kSr, seed);
    e.setSource(&src);
    e.setParams(busyParams());
}

} // namespace

TEST_CASE("engine_determinism: same seed is bit-identical, different seed differs", "[engine_determinism]")
{
    const SampleBuffer src(noise(40000, 7), noise(40000, 8), kSr);
    GranularEngine a, b, c;
    setup(a, src, 1234);
    setup(b, src, 1234);
    setup(c, src, 1235);
    const Out oa = render(a, 96000, 512); // 2 s
    const Out ob = render(b, 96000, 512);
    const Out oc = render(c, 96000, 512);
    CHECK_FALSE(allZero(oa)); // sound was produced, so equality is meaningful
    CHECK(oa.L == ob.L);
    CHECK(oa.R == ob.R);
    CHECK_FALSE(oa.L == oc.L);
}

TEST_CASE("engine_determinism: block split does not change the output", "[engine_determinism]")
{
    const SampleBuffer src(noise(40000, 7), {}, kSr);
    const int total = 48000; // 1 s
    GranularEngine ref;
    setup(ref, src, 99);
    const Out whole = render(ref, total); // one block
    REQUIRE_FALSE(allZero(whole));
    for (int block : {512, 37, 1}) {
        INFO("block=" << block);
        GranularEngine e;
        setup(e, src, 99);
        const Out o = render(e, total, block);
        CHECK(o.L == whole.L);
        CHECK(o.R == whole.R);
    }
}

TEST_CASE("engine_determinism: interleaved instances equal their solo runs", "[engine_determinism]")
{
    const SampleBuffer srcA(noise(30000, 1), {}, kSr);
    const SampleBuffer srcB(noise(50000, 2), noise(50000, 3), kSr);
    const int total = 24000;
    const int block = 200;

    GranularEngine soloA, soloB;
    setup(soloA, srcA, 11);
    setup(soloB, srcB, 22);
    const Out refA = render(soloA, total, block);
    const Out refB = render(soloB, total, block);

    GranularEngine a, b;
    setup(a, srcA, 11);
    setup(b, srcB, 22);
    Out ia{std::vector<float>(static_cast<std::size_t>(total)), std::vector<float>(static_cast<std::size_t>(total))};
    Out ib = ia;
    for (int done = 0; done < total; done += block) {
        a.process(ia.L.data() + done, ia.R.data() + done, block);
        b.process(ib.L.data() + done, ib.R.data() + done, block);
    }
    REQUIRE_FALSE(allZero(refA));
    CHECK(ia.L == refA.L);
    CHECK(ia.R == refA.R);
    CHECK(ib.L == refB.L);
    CHECK(ib.R == refB.R);
    CHECK_FALSE(refA.L == refB.L); // different seeds and sources really are different
}
