// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/ParamSpec.h>

#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>

using namespace grains;
namespace pm = grains::params;

TEST_CASE("param spec: ten stable, unique, ordered ids", "[param]")
{
    static_assert(pm::kCount == 10);
    CHECK(pm::kSpecs.size() == 10);

    const char* const expected[pm::kCount] = {"grainSize", "density",     "position",
                                              "positionJitter", "pitch", "pitchJitter",
                                              "timingJitter", "panSpread", "gain", "scan"};
    for (int i = 0; i < pm::kCount; ++i) {
        INFO("index " << i);
        CHECK(std::string(pm::kSpecs[static_cast<std::size_t>(i)].id) == expected[i]);
        CHECK(std::strlen(pm::kSpecs[static_cast<std::size_t>(i)].name) > 0);
        for (int j = i + 1; j < pm::kCount; ++j)
            CHECK(std::strcmp(pm::kSpecs[static_cast<std::size_t>(i)].id,
                              pm::kSpecs[static_cast<std::size_t>(j)].id) != 0);
    }
}

TEST_CASE("param spec: ranges and defaults come from limits", "[param]")
{
    const auto& s = pm::kSpecs;
    auto at = [&](pm::Id id) -> const pm::Spec& { return s[static_cast<std::size_t>(id)]; };

    CHECK(at(pm::Id::GrainSize).min == limits::kSizeMsMin);
    CHECK(at(pm::Id::GrainSize).max == limits::kSizeMsMax);
    CHECK(at(pm::Id::GrainSize).def == limits::kSizeMsDefault);
    CHECK(at(pm::Id::Density).min == 0.0f);
    CHECK(at(pm::Id::Density).max == limits::kDensityMax);
    CHECK(at(pm::Id::Density).def == limits::kDensityDefault);
    CHECK(at(pm::Id::Pitch).min == -limits::kPitchSemitonesMax);
    CHECK(at(pm::Id::Pitch).max == limits::kPitchSemitonesMax);
    CHECK(at(pm::Id::PitchJitter).max == limits::kPitchJitterSemitonesMax);
    CHECK(at(pm::Id::Gain).max == limits::kGainMax);
    CHECK(at(pm::Id::Gain).def == limits::kGainDefault);
    CHECK(at(pm::Id::Position).max == 1.0f);
    CHECK(at(pm::Id::Scan).min == -limits::kScanMax);
    CHECK(at(pm::Id::Scan).max == limits::kScanMax);
    CHECK(at(pm::Id::Scan).def == EngineParams{}.scan);
    CHECK(at(pm::Id::Scan).def == 0.0f);

    for (const auto& spec : s) {
        INFO(spec.id);
        CHECK(spec.min < spec.max);
        CHECK(spec.min <= spec.def);
        CHECK(spec.def <= spec.max);
        CHECK(std::isfinite(spec.skew));
        CHECK(spec.skew > 0.0f);
    }
}

TEST_CASE("param spec: table defaults equal EngineParams{}", "[param]")
{
    const auto d = [](pm::Id id) { return pm::kSpecs[static_cast<std::size_t>(id)].def; };
    const EngineParams e;
    CHECK(d(pm::Id::GrainSize) == e.grainSizeMs);
    CHECK(d(pm::Id::Density) == e.density);
    CHECK(d(pm::Id::Position) == e.position);
    CHECK(d(pm::Id::PositionJitter) == e.positionJitter);
    CHECK(d(pm::Id::Pitch) == e.pitchSemitones);
    CHECK(d(pm::Id::PitchJitter) == e.pitchJitterSemitones);
    CHECK(d(pm::Id::TimingJitter) == e.timingJitter);
    CHECK(d(pm::Id::PanSpread) == e.panSpread);
    CHECK(d(pm::Id::Gain) == e.gain);
    CHECK(d(pm::Id::Scan) == e.scan);
}

namespace {
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr float kInf = std::numeric_limits<float>::infinity();

// Same maths as juce::NormalisableRange<float>(min, max, 0, skew, symmetric), written out without JUCE.
float juceToPlain(const pm::Spec& s, float n)
{
    if (n <= 0.0f) return s.min;
    if (n >= 1.0f) return s.max;
    if (!s.symmetric) return s.min + (s.max - s.min) * std::exp(std::log(n) / s.skew);
    float d = 2.0f * n - 1.0f;
    if (d != 0.0f) d = std::exp(std::log(std::abs(d)) / s.skew) * (d < 0.0f ? -1.0f : 1.0f);
    return s.min + (s.max - s.min) / 2.0f * (1.0f + d);
}

float juceToNorm(const pm::Spec& s, float v)
{
    const float n = (v - s.min) / (s.max - s.min);
    if (!s.symmetric) return n <= 0.0f ? 0.0f : std::pow(n, s.skew);
    const float d = 2.0f * n - 1.0f;
    return (1.0f + std::pow(std::abs(d), s.skew) * (d < 0.0f ? -1.0f : 1.0f)) / 2.0f;
}
} // namespace

TEST_CASE("param spec: endpoints map exactly", "[param]")
{
    for (const auto& s : pm::kSpecs) {
        INFO(s.id);
        CHECK(pm::toNormalised(s, s.min) == 0.0f);
        CHECK(pm::toNormalised(s, s.max) == 1.0f);
        CHECK(pm::fromNormalised(s, 0.0f) == s.min);
        CHECK(pm::fromNormalised(s, 1.0f) == s.max);
    }
}

TEST_CASE("param spec: normalised round trip and monotonic", "[param]")
{
    for (const auto& s : pm::kSpecs) {
        INFO(s.id);
        float prev = s.min;
        for (int i = 0; i <= 100; ++i) {
            const float n = static_cast<float>(i) / 100.0f;
            const float plain = pm::fromNormalised(s, n);
            CHECK(plain >= prev);
            prev = plain;
            CHECK(std::fabs(pm::toNormalised(s, plain) - n) < 1e-4f);
        }
    }
}

TEST_CASE("param spec: skew midpoint matches the JUCE NormalisableRange formula", "[param]")
{
    for (const auto& s : pm::kSpecs) {
        INFO(s.id);
        const float span = s.max - s.min;
        for (const float n : {0.1f, 0.25f, 0.5f, 0.75f, 0.9f}) {
            CHECK(std::fabs(pm::fromNormalised(s, n) - juceToPlain(s, n)) < 1e-4f * span);
            const float v = s.min + 0.3f * span;
            CHECK(std::fabs(pm::toNormalised(s, v) - juceToNorm(s, v)) < 1e-4f);
        }
    }
    // The skewed ranges really are skewed: the knob midpoint is far below the arithmetic midpoint.
    const auto& size = pm::kSpecs[static_cast<std::size_t>(pm::Id::GrainSize)];
    CHECK(pm::fromNormalised(size, 0.5f) < 0.2f * (size.min + size.max));
}

TEST_CASE("param spec: scan is symmetric about the centre of the control", "[param]")
{
    const auto& scan = pm::kSpecs[static_cast<std::size_t>(pm::Id::Scan)];
    CHECK(scan.symmetric);
    CHECK(scan.versionHint == 2);
    CHECK(std::string(scan.unit) == "len/s");
    for (std::size_t i = 0; i < pm::kCount; ++i)
        if (i != static_cast<std::size_t>(pm::Id::Scan)) {
            INFO(pm::kSpecs[i].id);
            CHECK_FALSE(pm::kSpecs[i].symmetric);
            CHECK(pm::kSpecs[i].versionHint == 1);
        }

    // The 0 default sits at the exact centre, and the centre maps back to exactly 0.
    CHECK(pm::toNormalised(scan, scan.def) == 0.5f);
    CHECK(pm::fromNormalised(scan, 0.5f) == 0.0f);

    // Finer resolution near 0: a quarter-turn from the centre is well below half a length per second.
    const float quarter = pm::fromNormalised(scan, 0.25f);
    CHECK(quarter < 0.0f);
    CHECK(std::fabs(quarter) < 0.5f);
    CHECK(std::fabs(pm::fromNormalised(scan, 0.75f) + quarter) < 1e-6f);

    // Matches the JUCE symmetric NormalisableRange maths in both directions.
    for (const float n : {0.05f, 0.25f, 0.4f, 0.6f, 0.75f, 0.95f}) {
        INFO("norm " << n);
        CHECK(std::fabs(pm::fromNormalised(scan, n) - juceToPlain(scan, n)) < 1e-5f);
        CHECK(std::fabs(pm::toNormalised(scan, quarter) - juceToNorm(scan, quarter)) < 1e-5f);
    }
    for (const float v : {-0.9f, -0.3f, -0.01f, 0.01f, 0.3f, 0.9f})
        CHECK(std::fabs(pm::toNormalised(scan, v) - juceToNorm(scan, v)) < 1e-5f);
}

TEST_CASE("param spec: out-of-range and NaN are clamped", "[param]")
{
    for (const auto& s : pm::kSpecs) {
        INFO(s.id);
        CHECK(pm::toNormalised(s, s.min - 1000.0f) == 0.0f);
        CHECK(pm::toNormalised(s, s.max + 1000.0f) == 1.0f);
        CHECK(pm::toNormalised(s, -kInf) == 0.0f);
        CHECK(pm::toNormalised(s, kInf) == 1.0f);
        CHECK(pm::fromNormalised(s, -0.5f) == s.min);
        CHECK(pm::fromNormalised(s, 1.5f) == s.max);
        CHECK(pm::fromNormalised(s, -kInf) == s.min);
        CHECK(pm::fromNormalised(s, kInf) == s.max);

        CHECK(pm::fromNormalised(s, kNaN) == s.def);
        const float nd = pm::toNormalised(s, kNaN);
        CHECK(std::isfinite(nd));
        CHECK(nd == pm::toNormalised(s, s.def));
    }
}

namespace {
using Plain = std::array<float, pm::kCount>;

Plain defaults()
{
    Plain p{};
    for (std::size_t i = 0; i < p.size(); ++i) p[i] = pm::kSpecs[i].def;
    return p;
}

void checkSame(const EngineParams& a, const EngineParams& b)
{
    CHECK(a.grainSizeMs == b.grainSizeMs);
    CHECK(a.density == b.density);
    CHECK(a.position == b.position);
    CHECK(a.positionJitter == b.positionJitter);
    CHECK(a.pitchSemitones == b.pitchSemitones);
    CHECK(a.pitchJitterSemitones == b.pitchJitterSemitones);
    CHECK(a.timingJitter == b.timingJitter);
    CHECK(a.panSpread == b.panSpread);
    CHECK(a.gain == b.gain);
    CHECK(a.scan == b.scan);
}
} // namespace

TEST_CASE("param map: defaults give EngineParams{}", "[param]")
{
    checkSame(pm::toEngineParams(defaults()), EngineParams{});
}

TEST_CASE("param map: each slot routes to its own field", "[param]")
{
    const Plain p{50.0f, 100.0f, 0.1f, 0.2f, 7.0f, 3.0f, 0.3f, 0.4f, 1.5f, -0.6f};
    const EngineParams e = pm::toEngineParams(p);
    CHECK(e.grainSizeMs == 50.0f);
    CHECK(e.density == 100.0f);
    CHECK(e.position == 0.1f);
    CHECK(e.positionJitter == 0.2f);
    CHECK(e.pitchSemitones == 7.0f);
    CHECK(e.pitchJitterSemitones == 3.0f);
    CHECK(e.timingJitter == 0.3f);
    CHECK(e.panSpread == 0.4f);
    CHECK(e.gain == 1.5f);
    CHECK(e.scan == -0.6f);
}

TEST_CASE("param map: density dead zone snaps up, exact zero stays off", "[param]")
{
    Plain p = defaults();
    const auto d = static_cast<std::size_t>(pm::Id::Density);

    p[d] = 0.0f;
    CHECK(pm::toEngineParams(p).density == 0.0f);

    p[d] = limits::kDensityMin * 0.5f;
    CHECK(pm::toEngineParams(p).density == limits::kDensityMin);

    p[d] = limits::kDensityMin;
    CHECK(pm::toEngineParams(p).density == limits::kDensityMin);
}

TEST_CASE("param map: extremes are finite and clamp-stable", "[param]")
{
    for (const float v : {-1.0e9f, -1.0f, 0.0f, 1.0e9f, kInf, -kInf, kNaN}) {
        for (std::size_t slot = 0; slot < pm::kCount; ++slot) {
            Plain p = defaults();
            p[slot] = v;
            const EngineParams e = pm::toEngineParams(p);
            INFO("slot " << slot << " value " << v);
            CHECK(std::isfinite(e.grainSizeMs));
            CHECK(std::isfinite(e.density));
            CHECK(std::isfinite(e.position));
            CHECK(std::isfinite(e.positionJitter));
            CHECK(std::isfinite(e.pitchSemitones));
            CHECK(std::isfinite(e.pitchJitterSemitones));
            CHECK(std::isfinite(e.timingJitter));
            CHECK(std::isfinite(e.panSpread));
            CHECK(std::isfinite(e.gain));
            CHECK(std::isfinite(e.scan));
            checkSame(e, e.clamped());
        }
    }
}
