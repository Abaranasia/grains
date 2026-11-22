// SPDX-License-Identifier: AGPL-3.0-or-later
#include <catch2/catch_test_macros.hpp>

#include <grains/TransportGate.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

using grains::TransportGate;
using State = TransportGate::State;

namespace {

bool isPlusZero(float v)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof bits);
    return bits == 0u;
}

// Applies the gate to a block of ones and returns the resulting gains (the gate is a pure
// multiplier, so ones expose the gain sequence).
std::vector<float> gains(TransportGate& g, int n)
{
    std::vector<float> l(static_cast<std::size_t>(n), 1.0f), r(static_cast<std::size_t>(n), 1.0f);
    g.apply(l.data(), r.data(), n);
    REQUIRE(l == r);
    return l;
}

// One step is 1/len; the slack covers rounding of float subtraction near 1.
float maxStep(int len)
{
    return 1.0f / static_cast<float>(len) + 2.0f * std::numeric_limits<float>::epsilon();
}

} // namespace

TEST_CASE("transport_gate: default state is Stopped with gain 0", "[transport_gate]")
{
    TransportGate g;
    CHECK(g.state() == State::Stopped);
    CHECK(g.stopped());
    CHECK_FALSE(g.steady());
    CHECK(g.gain() == 0.0f);
    CHECK(TransportGate::kRampSeconds == 0.020);
}

TEST_CASE("transport_gate: ramp length follows the sample rate", "[transport_gate]")
{
    TransportGate g;
    g.prepare(44100.0);
    CHECK(g.rampLength() == 882);
    g.prepare(48000.0);
    CHECK(g.rampLength() == 960);
    g.prepare(96000.0);
    CHECK(g.rampLength() == 1920);
    g.prepare(1.0); // tiny rate: lround(0.02) == 0, clamped to 1
    CHECK(g.rampLength() == 1);
}

TEST_CASE("transport_gate: prepare resets to Stopped and discards a ramp", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    g.forcePlaying();
    CHECK(g.state() == State::Playing);
    CHECK(g.steady());
    CHECK(g.gain() == 1.0f);
    g.prepare(48000.0);
    CHECK(g.state() == State::Stopped);
    CHECK(g.gain() == 0.0f);

    REQUIRE(g.request(true));
    gains(g, 100);
    CHECK(g.state() == State::Starting);
    g.prepare(96000.0);
    CHECK(g.state() == State::Stopped);
    CHECK(g.gain() == 0.0f);
    CHECK(g.rampLength() == 1920);

    g.forcePlaying();
    g.forceStopped();
    CHECK(g.state() == State::Stopped);
    CHECK(g.gain() == 0.0f);
}

TEST_CASE("transport_gate: fade-in endpoints are exact", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    const int len = g.rampLength();
    CHECK(g.request(true));
    CHECK(g.state() == State::Starting);
    const std::vector<float> y = gains(g, len + 1);
    CHECK(isPlusZero(y.front()));
    CHECK(y.back() == 1.0f);
    CHECK(g.state() == State::Playing);
    for (std::size_t i = 1; i < y.size(); ++i) {
        CHECK(y[i] >= y[i - 1]);
        CHECK(y[i] - y[i - 1] <= maxStep(len));
    }
}

TEST_CASE("transport_gate: fade-out is monotonic from 1 to exactly +0", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    const int len = g.rampLength();
    g.forcePlaying();
    CHECK(g.request(false) == false);
    CHECK(g.state() == State::Stopping);
    const std::vector<float> y = gains(g, len + 8);
    CHECK(y.front() < 1.0f);
    CHECK(y.front() >= 1.0f - maxStep(len));
    for (std::size_t i = 1; i < y.size(); ++i) {
        CHECK(y[i] <= y[i - 1]);
        CHECK(y[i - 1] - y[i] <= maxStep(len));
    }
    CHECK(g.state() == State::Stopped);
    CHECK(g.gain() == 0.0f);
    for (std::size_t i = static_cast<std::size_t>(len - 1); i < y.size(); ++i) CHECK(isPlusZero(y[i]));
}

TEST_CASE("transport_gate: zero gain writes +0 even for negative input", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    std::vector<float> l(4, -0.5f), r(4, -0.5f);
    g.apply(l.data(), r.data(), 4); // Stopped
    for (float v : l) CHECK(isPlusZero(v));
    for (float v : r) CHECK(isPlusZero(v));
    REQUIRE(g.request(true));
    std::vector<float> l2(2, -0.5f), r2(2, -0.5f);
    g.apply(l2.data(), r2.data(), 2); // first Starting sample has gain 0
    CHECK(isPlusZero(l2[0]));
    CHECK(isPlusZero(r2[0]));
    CHECK(l2[1] < 0.0f);
}

TEST_CASE("transport_gate: Playing leaves the signal untouched", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    g.forcePlaying();
    std::vector<float> l{0.25f, -0.5f, 0.75f}, r{-0.25f, 0.5f, -0.75f};
    const auto l0 = l, r0 = r;
    g.apply(l.data(), r.data(), 3);
    CHECK(l == l0);
    CHECK(r == r0);
    CHECK(g.state() == State::Playing);
}

TEST_CASE("transport_gate: redundant requests are no-ops", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    g.forcePlaying();
    CHECK_FALSE(g.request(true));
    CHECK(g.state() == State::Playing);

    g.forceStopped();
    CHECK(g.request(true));
    CHECK_FALSE(g.request(true)); // already Starting: no restart
    CHECK(g.state() == State::Starting);

    g.forceStopped();
    CHECK_FALSE(g.request(false)); // Stop while Stopped
    CHECK(g.state() == State::Stopped);
}

TEST_CASE("transport_gate: Stop mid fade-in reverses from the current position", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    const int len = g.rampLength();
    REQUIRE(g.request(true));
    const std::vector<float> up = gains(g, len / 2);
    const float last = up.back();
    CHECK_FALSE(g.request(false));
    CHECK(g.state() == State::Stopping);
    const std::vector<float> down = gains(g, 1);
    CHECK(down[0] <= last);
    CHECK(last - down[0] <= maxStep(len));
    gains(g, len);
    CHECK(g.state() == State::Stopped);
}

TEST_CASE("transport_gate: Play during Stopping latches until the fade completes", "[transport_gate]")
{
    TransportGate g;
    g.prepare(48000.0);
    const int len = g.rampLength();
    g.forcePlaying();
    g.request(false);
    gains(g, len / 2);
    REQUIRE(g.state() == State::Stopping);
    CHECK_FALSE(g.request(true)); // no reversal, no restart yet
    CHECK(g.state() == State::Stopping);
    const std::vector<float> y = gains(g, len); // runs to 0 and stays there
    CHECK(g.state() == State::Stopped);
    CHECK(isPlusZero(y.back()));
    CHECK(g.request(true)); // the caller restarts on the next evaluation
    CHECK(g.state() == State::Starting);
}
