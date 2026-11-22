// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <grains/GrainPool.h>

#include <array>
#include <cstdint>

namespace grains {

// One active grain as the display sees it (R-WD-5). All floats are finite and in [0, 1].
struct GrainMark {
    float position = 0.0f; // grain head in the source, normalised to the source length
    float progress = 0.0f; // grain age fraction
    float level = 0.0f;    // envelope level at `progress`
    std::uint16_t slot = 0;     // pool slot index, below GrainPool::kCapacity
    std::uint16_t reserved = 0; // keeps the mark at 16 bytes; room for a later field (grain direction)
};

// Plain value copied from the audio thread to the message thread through a TripleBuffer. Fixed size,
// so publishing never allocates. New fields are appended and kVersion is bumped, so a reader that
// serialises the layout can tell which one it got.
struct GrainSnapshot {
    static constexpr std::uint32_t kVersion = 1;

    std::uint32_t version = kVersion;
    std::int32_t count = 0; // equals the engine's activeGrainCount() at capture
    std::array<GrainMark, GrainPool::kCapacity> marks{}; // the first `count` entries are valid, in slot order
};

} // namespace grains
