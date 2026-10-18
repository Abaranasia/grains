#pragma once

#include <grains/Grain.h>

#include <array>
#include <utility>

namespace grains {

// Fixed-capacity grain storage. No allocation; skip-on-full (acquire returns nullptr).
class GrainPool {
public:
    static constexpr int kCapacity = 128;

    // Lowest-index free slot, reset to a default active grain; nullptr when full.
    Grain* acquire() noexcept
    {
        for (Grain& g : grains_) {
            if (!g.active) {
                g = Grain{};
                g.active = true;
                ++activeCount_;
                return &g;
            }
        }
        return nullptr;
    }

    void release(Grain& g) noexcept
    {
        if (g.active) {
            g.active = false;
            --activeCount_;
        }
    }

    int activeCount() const noexcept { return activeCount_; }

    void reset() noexcept
    {
        for (Grain& g : grains_) g = Grain{};
        activeCount_ = 0;
    }

    // Visits active grains in index order. `f` may release the grain it is given.
    template <class F>
    void forEachActive(F&& f) noexcept(noexcept(f(std::declval<Grain&>())))
    {
        for (Grain& g : grains_)
            if (g.active) f(g);
    }

private:
    std::array<Grain, kCapacity> grains_{};
    int activeCount_ = 0;
};

} // namespace grains
