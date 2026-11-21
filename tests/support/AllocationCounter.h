// SPDX-License-Identifier: AGPL-3.0-or-later
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

    // Number of non-null operator delete calls (any variant) since this scope began.
    std::size_t frees() const noexcept;
};

// Fault injection (R-BH-6). Arms a one-shot failure: the n-th allocation (1-based, any operator new
// variant, counted from this call whether or not an AllocationScope is alive) returns null, so the
// throwing forms raise std::bad_alloc and the nothrow forms return nullptr. It fires once and then
// disarms itself. Process-wide (atomic); arm and disarm from the test thread only, tightly around
// the code under test, because the test framework allocates too. Never linked into grains_tests.
void armFailure(std::size_t n) noexcept;
void disarmFailure() noexcept;   // cancels an armed failure; failureFired() keeps its value
bool failureFired() noexcept;    // true if the failure armed by the last armFailure() has fired

} // namespace grains_test
