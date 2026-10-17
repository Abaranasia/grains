#pragma once

#include <cstdint>

namespace grains {

// PCG32 (XSH-RR). Deterministic across standard libraries, no global state.
class Rng {
public:
    explicit Rng(uint64_t seed) noexcept { this->seed(seed); }

    // Equivalent to pcg32_srandom_r(seed, kStream).
    void seed(uint64_t s) noexcept
    {
        state_ = 0;
        inc_ = (kStream << 1u) | 1u;
        nextU32();
        state_ += s;
        nextU32();
    }

    uint32_t nextU32() noexcept
    {
        const uint64_t old = state_;
        state_ = old * 6364136223846793005ULL + inc_;
        const uint32_t xorshifted = static_cast<uint32_t>(((old >> 18u) ^ old) >> 27u);
        const uint32_t rot = static_cast<uint32_t>(old >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((~rot + 1u) & 31u));
    }

    // Uniform in [0, 1), 24 bits of resolution.
    float uniform01() noexcept
    {
        return static_cast<float>(nextU32() >> 8u) * (1.0f / 16777216.0f);
    }

    // Uniform in [-1, 1).
    float bipolar() noexcept { return uniform01() * 2.0f - 1.0f; }

private:
    static constexpr uint64_t kStream = 54u;
    uint64_t state_ = 0;
    uint64_t inc_ = 0;
};

} // namespace grains
