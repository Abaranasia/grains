// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/WindowTable.h>

#include <cmath>

namespace grains {

WindowTable::WindowTable()
{
    constexpr double kTwoPi = 6.283185307179586476925286766559;
    for (int i = 0; i < kSize; ++i) {
        const double p = static_cast<double>(i) / static_cast<double>(kSize - 1);
        table_[static_cast<std::size_t>(i)] = static_cast<float>(0.5 * (1.0 - std::cos(kTwoPi * p)));
    }
    // Guarantee exact zero endpoints regardless of libm rounding.
    table_.front() = 0.0f;
    table_.back() = 0.0f;
}

float WindowTable::at(float phase01) const noexcept
{
    if (!(phase01 > 0.0f)) return 0.0f; // also catches NaN
    if (phase01 >= 1.0f) return 0.0f;
    const float x = phase01 * static_cast<float>(kSize - 1);
    const int i = static_cast<int>(x);
    const float frac = x - static_cast<float>(i);
    const float a = table_[static_cast<std::size_t>(i)];
    const float b = table_[static_cast<std::size_t>(i + 1)];
    return a + (b - a) * frac;
}

} // namespace grains
