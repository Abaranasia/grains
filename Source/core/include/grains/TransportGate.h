// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace grains {

// Play/Stop gain ramp (spec transport-gate). JUCE-free, noexcept, allocation-free.
//
// A linear fade of kRampSeconds, driven by an integer counter pos_ in [0, len_]: the gain is
// float(pos_) / float(len_), so the endpoints are exactly 0 and 1 and no step exceeds 1/len_.
// Sample rule: Starting multiplies by pos_/len_ and then increments (first sample exactly 0);
// Stopping decrements and then multiplies. A zero gain writes +0.0f. While Playing the gate does
// not touch the signal, so steady output is bit-identical to an ungated engine.
class TransportGate {
public:
    static constexpr double kRampSeconds = 0.020;
    enum class State : std::uint8_t { Stopped, Starting, Playing, Stopping };

    // Recomputes the ramp length for the rate and returns to Stopped (any ramp is discarded).
    void prepare(double sampleRate) noexcept
    {
        len_ = std::max(1, static_cast<int>(std::lround(kRampSeconds * sampleRate)));
        forceStopped();
    }

    void forcePlaying() noexcept
    {
        state_ = State::Playing;
        pos_ = len_;
    }

    void forceStopped() noexcept
    {
        state_ = State::Stopped;
        pos_ = 0;
    }

    // Level-triggered target. Returns true iff this call moved Stopped -> Starting, in which case
    // the caller restarts the engine. A Play during Stopping is latched by the caller's target: it
    // returns false here and the next request after the fade has completed returns true.
    bool request(bool play) noexcept
    {
        switch (state_) {
        case State::Stopped:
            if (!play) return false;
            state_ = State::Starting;
            return true;
        case State::Starting:
            if (!play) state_ = pos_ == 0 ? State::Stopped : State::Stopping; // reverse from pos_
            return false;
        case State::Playing:
            if (!play) state_ = State::Stopping;
            return false;
        case State::Stopping:
            return false; // never reverses (D3)
        }
        return false;
    }

    // Ramps n samples in place. May end in Playing (fade-in done) or Stopped (fade-out done).
    void apply(float* l, float* r, int n) noexcept
    {
        for (int i = 0; i < n; ++i) {
            if (state_ == State::Playing) return; // steady: untouched
            if (state_ == State::Stopped) {
                std::fill(l + i, l + n, 0.0f);
                std::fill(r + i, r + n, 0.0f);
                return;
            }
            if (state_ == State::Starting) {
                const float g = gain();
                l[i] = g == 0.0f ? 0.0f : l[i] * g;
                r[i] = g == 0.0f ? 0.0f : r[i] * g;
                if (++pos_ >= len_) state_ = State::Playing;
            } else { // Stopping
                if (pos_ > 0) --pos_;
                const float g = gain();
                l[i] = g == 0.0f ? 0.0f : l[i] * g;
                r[i] = g == 0.0f ? 0.0f : r[i] * g;
                if (pos_ == 0) state_ = State::Stopped;
            }
        }
    }

    State state() const noexcept { return state_; }
    bool steady() const noexcept { return state_ == State::Playing; }
    bool stopped() const noexcept { return state_ == State::Stopped; }
    int rampLength() const noexcept { return len_; }
    float gain() const noexcept { return static_cast<float>(pos_) / static_cast<float>(len_); }

private:
    State state_ = State::Stopped;
    int len_ = 960;
    int pos_ = 0;
};

} // namespace grains
