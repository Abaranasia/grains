// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/LoadPlanner.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>
#include <grains/WaveformPeaks.h>

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

// Background decoder: reads a file, builds a SampleBuffer and publishes it to the exchange. The
// latest request wins (generation counter). Results are delivered on the message thread.
// Every request carries the target rate in force when it was issued; the published buffer is
// resampled to it (0 = unknown, so the file is published at its own rate).
class SampleLoader final : private juce::Thread {
public:
    using Origin = grains::LoadOrigin;

    struct Result {
        std::uint64_t generation;
        Origin origin;
        bool loaded;       // true only for a successful publish
        juce::String path; // the loaded path, or the original request path on failure
        juce::String text; // status line for the UI
        bool relocated;    // loaded from a candidate other than the original path
        // Overview of the committed source (also redelivered on a Reload). Null when not loaded, or when
        // computing it failed: the source still plays and the view shows "no waveform".
        std::shared_ptr<const grains::WaveformPeaks> peaks;
        bool cleared; // true only when a clear commit was accepted (Restore with nothing or a missing file)
    };
    using Callback = std::function<void(const Result&)>;

    SampleLoader(grains::SourceExchange<grains::SampleBuffer>& exchange, Callback onResult);
    ~SampleLoader() override { stop(); }

    // Any thread. Loads the first existing absolute candidate; none existing counts as missing and
    // is labelled with displayName. An empty list + Restore clears the source. original is the
    // path the state stored; loading any other candidate is reported as relocated.
    void request(const juce::StringArray& candidates, const juce::String& original,
                 const juce::String& displayName, Origin origin);
    std::uint64_t latestGeneration() const noexcept { return planner_.gate().current(); }

    // Any thread except the audio thread (call from prepareToPlay). Sets the target rate for later
    // requests. A request still in flight is reissued at the new rate (so a restore before prepare
    // decodes once); otherwise a published source whose rate differs is re-decoded from disk as a
    // Reload. Returns true when a request was issued; the same rate again is a no-op.
    bool retarget(double rate);

    // Message thread only. Idempotent; blocks until the loader thread exits. Call before the exchange is destroyed.
    void stop();

private:
    void run() override;
    void load(const grains::LoadRequest& r); // loadImpl, then planner_.settle (which may issue a Reload)
    grains::LoadOutcome loadImpl(const grains::LoadRequest& r);
    // Commits s to the exchange only if r is still the latest request (atomic with request()).
    // Frees any displaced or rejected Box after the gate unlocks. True when accepted.
    bool commitGated(const grains::LoadRequest& r, grains::SourceExchange<grains::SampleBuffer>::Staged s);
    void post(const grains::LoadRequest& r, bool loaded, const juce::String& text, const juce::String& path = {},
              bool relocated = false, std::shared_ptr<const grains::WaveformPeaks> peaks = nullptr,
              bool cleared = false);

    grains::SourceExchange<grains::SampleBuffer>& exchange_;
    Callback onResult_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    std::string lastText_; // loader thread only: the last status line posted by the current run
    grains::LoadPlanner planner_; // request bookkeeping, the generation gate and the published source
};
