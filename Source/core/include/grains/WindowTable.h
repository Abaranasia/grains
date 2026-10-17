#pragma once

#include <array>

namespace grains {

// Hann window lookup table with linear interpolation. at(0) == at(1) == 0.
class WindowTable {
public:
    static constexpr int kSize = 4096;

    WindowTable();

    // phase01 outside [0, 1] is clamped.
    float at(float phase01) const noexcept;

private:
    std::array<float, kSize> table_{};
};

} // namespace grains
