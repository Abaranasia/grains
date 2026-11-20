// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/GenerationGate.h>

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace grains {

enum class LoadOrigin { User, Restore, Reload };
enum class LoadOutcome { Published, Cleared, Failed, Aborted };

struct LoadRequest {
    std::vector<std::string> candidates;
    std::string original;
    std::string displayName;
    LoadOrigin origin = LoadOrigin::User;
    std::uint64_t generation = 0;
    double targetRate = 0.0; // 0 = publish at the file's own rate
    std::string label;       // Reload only: the status line to repost if the reload fails
};

// What is live in the exchange, for rate-change reloads.
struct PublishedSource {
    std::string path; // the file actually loaded
    std::string original;
    std::string displayName;
    std::string label; // the "Loaded: ..." line
    double rate = 0.0; // the rate of the published buffer
};

// JUCE-free request bookkeeping for the sample loader: the generation gate, the pending and latest
// requests, the target rate and what is currently published. The loader thread drives it through
// take()/settle(); any thread may call request()/retarget().
//
// Lock order: mutex_, then the gate mutex. Nothing may call the planner from inside a commitIf
// callback (the gate mutex is held there).
class LoadPlanner {
public:
    // Any thread. Stamps a generation and the current target rate; replaces any pending request.
    // May throw std::bad_alloc, in which case the planner is unchanged.
    std::uint64_t request(LoadRequest r);

    // Any thread except the audio thread. True when a request was issued; invalid rates and the
    // same rate again are no-ops. Issues nothing (and leaves state unchanged) if a copy fails.
    bool retarget(double rate) noexcept;

    // Loader thread: moves out the pending request, if any.
    std::optional<LoadRequest> take() noexcept;

    // Loader thread, after an accepted commit and outside the gate callback.
    void published(PublishedSource p) noexcept;
    // Loader thread, after an accepted clear and outside the gate callback.
    void cleared() noexcept;

    // Loader thread, when the run for gen has finished. Marks it settled and applies R4-001: a
    // Failed request that is still the latest, is not a Reload, has a known target and left a
    // published source at another rate gets one Reload at the target. True when it issued one.
    // failureStatus is the status line the failed request posted. The reissued Reload carries it as
    // its label, so the Reload (which supersedes the failed request and makes its own post stale)
    // keeps showing the failure instead of the previous file's "Loaded: ..." line (R-SS-15). Empty
    // keeps the published label.
    bool settle(std::uint64_t gen, LoadOutcome outcome, std::string failureStatus = {}) noexcept;

    GenerationGate& gate() noexcept { return gate_; }
    const GenerationGate& gate() const noexcept { return gate_; }
    double target() const noexcept;

private:
    mutable std::mutex mutex_;
    GenerationGate gate_;
    double target_ = 0.0;
    std::optional<LoadRequest> latest_;
    std::optional<LoadRequest> pending_;
    std::uint64_t settled_ = 0;
    std::optional<PublishedSource> published_;
};

} // namespace grains
