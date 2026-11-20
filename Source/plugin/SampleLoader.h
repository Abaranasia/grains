// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/GenerationGate.h>
#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

// Background decoder: reads a file, builds a SampleBuffer and publishes it to the exchange. The
// latest request wins (generation counter). Results are delivered on the message thread.
// Every request carries the target rate in force when it was issued; the published buffer is
// resampled to it (0 = unknown, so the file is published at its own rate).
class SampleLoader final : private juce::Thread {
public:
    enum class Origin { User, Restore, Reload };

    struct Result {
        std::uint64_t generation;
        Origin origin;
        bool loaded;       // true only for a successful publish
        juce::String path; // the loaded path, or the original request path on failure
        juce::String text; // status line for the UI
        bool relocated;    // loaded from a candidate other than the original path
    };
    using Callback = std::function<void(const Result&)>;

    SampleLoader(grains::SourceExchange<grains::SampleBuffer>& exchange, Callback onResult);
    ~SampleLoader() override { stop(); }

    // Any thread. Loads the first existing absolute candidate; none existing counts as missing and
    // is labelled with displayName. An empty list + Restore clears the source. original is the
    // path the state stored; loading any other candidate is reported as relocated.
    void request(const juce::StringArray& candidates, const juce::String& original,
                 const juce::String& displayName, Origin origin);
    std::uint64_t latestGeneration() const noexcept { return gate_.current(); }

    // Any thread except the audio thread (call from prepareToPlay). Sets the target rate for later
    // requests. A request still in flight is reissued at the new rate (so a restore before prepare
    // decodes once); otherwise a published source whose rate differs is re-decoded from disk as a
    // Reload. Returns true when a request was issued; the same rate again is a no-op.
    bool retarget(double rate);

    // Message thread only. Idempotent; blocks until the loader thread exits. Call before the exchange is destroyed.
    void stop();

private:
    struct Request {
        juce::StringArray candidates;
        juce::String original;
        juce::String displayName;
        Origin origin;
        std::uint64_t generation;
        double targetRate = 0.0; // 0 = publish at the file's own rate
        juce::String label;      // Reload only: the status line to repost if the reload fails
    };

    // What is live in the exchange, for rate-change reloads. Written under lock_ after commitGated.
    struct Published {
        juce::String path; // the file actually loaded
        juce::String original;
        juce::String displayName;
        juce::String label; // the "Loaded: ..." line
        double rate;        // the rate of the published buffer
    };

    void run() override;
    void load(const Request& r); // loadImpl, then marks r settled
    void loadImpl(const Request& r);
    void clearPublished(); // takes lock_; call after commitGated, never inside the gate callback
    // Commits s to the exchange only if r is still the latest request (atomic with request()).
    // Frees any displaced or rejected Box after the gate unlocks. True when accepted.
    bool commitGated(const Request& r, grains::SourceExchange<grains::SampleBuffer>::Staged s);
    void post(const Request& r, bool loaded, const juce::String& text, const juce::String& path = {},
              bool relocated = false);

    grains::SourceExchange<grains::SampleBuffer>& exchange_;
    Callback onResult_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    grains::GenerationGate gate_;
    juce::CriticalSection lock_;
    std::optional<Request> pending_; // guarded by lock_
    double target_ = 0.0;                      // guarded by lock_: rate for new requests
    std::optional<Request> latest_;            // guarded by lock_: the last request issued
    std::uint64_t settled_ = 0;                // guarded by lock_: generation whose load() has finished
    std::optional<Published> published_;       // guarded by lock_: nullopt when nothing is live
};
