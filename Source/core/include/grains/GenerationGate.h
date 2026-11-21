// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <type_traits>

namespace grains {

// Generation counter plus a critical section, so "is this request still the latest?" and "act on
// it" are one atomic step with respect to next(). Used by the loader thread (commitIf) against the
// requesting threads (next).
//
// commitIf runs f while holding the gate mutex: f MUST be short, MUST NOT allocate or free, and
// MUST NOT re-enter the gate (std::mutex is not recursive). next() blocks only while f runs.
class GenerationGate {
public:
    // Any thread. Returns the new generation (the first call returns 1).
    std::uint64_t next() noexcept
    {
        const std::lock_guard<std::mutex> lock(mutex_);
        const std::uint64_t v = gen_.load(std::memory_order_relaxed) + 1;
        gen_.store(v, std::memory_order_relaxed);
        return v;
    }

    // Advisory snapshot; only commitIf gives an authoritative answer.
    std::uint64_t current() const noexcept { return gen_.load(std::memory_order_relaxed); }

    // Runs f exactly once and returns true iff gen is still the current generation, atomically with
    // respect to next(). Otherwise f is not run and false is returned.
    template <typename F>
    bool commitIf(std::uint64_t gen, F&& f) noexcept
    {
        static_assert(std::is_nothrow_invocable_v<F&>, "commitIf callback must be noexcept");
        const std::lock_guard<std::mutex> lock(mutex_);
        if (gen != gen_.load(std::memory_order_relaxed)) return false;
        f();
        return true;
    }

private:
    std::mutex mutex_;
    std::atomic<std::uint64_t> gen_{0}; // written only under mutex_
};

} // namespace grains
