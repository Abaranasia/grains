// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/GrainSnapshot.h>

namespace grains {

// R-WD-7: decides what the live grain strip shows, from poll results only. Time is counted in poll
// ticks (not wall-clock), so the behaviour is deterministic and testable. The view calls tick() once
// per timer callback with the frame the mailbox returned (or nullptr when there was none) and
// repaints the strip when it returns true.
class GrainDisplayModel {
public:
    static constexpr int kPollHz = 30; // the single poll-rate constant
    static constexpr int kStaleMs = 250;
    // ceil(kStaleMs * kPollHz / 1000): idle ticks before the marks clear (about 267 ms).
    static constexpr int kStaleTicks = (kStaleMs * kPollHz + 999) / 1000;

    // Fresh frame: shown at once. Returns true when a repaint is needed, i.e. the old or the new
    // frame has marks (empty to empty does not repaint). No frame: counts an idle tick; on the
    // kStaleTicks-th the marks clear and this returns true once. Later idle ticks return false.
    bool tick(const GrainSnapshot* fresh) noexcept;

    // Clears the marks (the view calls it when its timer stops). No repaint is requested.
    void reset() noexcept;

    const GrainSnapshot& shown() const noexcept { return shown_; }

private:
    GrainSnapshot shown_;
    int idleTicks_ = 0;
};

} // namespace grains
