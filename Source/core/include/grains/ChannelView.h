// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>

namespace grains {

// Non-owning view of one decoded channel.
struct ChannelView {
    const float* data;
    std::size_t frames;
};

} // namespace grains
