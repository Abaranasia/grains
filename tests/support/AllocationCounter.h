#pragma once

// Heap-allocation counter for real-time safety tests (R-GR-8).
//
// AllocationCounter.cpp replaces the global operator new/delete family, so it must only be linked
// into the dedicated grains_realtime_tests executable, never into the shared grains_tests binary.
// Allocations are counted only while an AllocationScope is alive; allocations made by the test
// framework outside a scope are ignored. Scopes must not be nested and are process-wide (atomic).

#include <cstddef>

namespace grains_test {

class AllocationScope {
public:
    AllocationScope() noexcept;
    ~AllocationScope();
    AllocationScope(const AllocationScope&) = delete;
    AllocationScope& operator=(const AllocationScope&) = delete;

    // Number of operator new calls (any variant) since this scope began.
    std::size_t count() const noexcept;
};

} // namespace grains_test
