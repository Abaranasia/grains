// SPDX-License-Identifier: AGPL-3.0-or-later
#include "AllocationCounter.h"

#include <atomic>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

std::atomic<bool> gActive{false};
std::atomic<std::size_t> gCount{0};
std::atomic<std::size_t> gFrees{0};

void note() noexcept
{
    if (gActive.load(std::memory_order_relaxed)) gCount.fetch_add(1, std::memory_order_relaxed);
}

void noteFree(void* p) noexcept
{
    if (p != nullptr && gActive.load(std::memory_order_relaxed)) gFrees.fetch_add(1, std::memory_order_relaxed);
}

void* plainAlloc(std::size_t n) noexcept
{
    note();
    return std::malloc(n != 0 ? n : 1);
}

// Aligned blocks must be released by the matching aligned free (MSVC: _aligned_malloc/_aligned_free).
void* alignedAlloc(std::size_t n, std::size_t align) noexcept
{
    note();
    if (n == 0) n = 1;
#if defined(_MSC_VER)
    return _aligned_malloc(n, align);
#else
    // aligned_alloc requires size to be a multiple of the alignment.
    return std::aligned_alloc(align, (n + align - 1) / align * align);
#endif
}

void plainFree(void* p) noexcept
{
    noteFree(p);
    std::free(p);
}

void alignedFree(void* p) noexcept
{
    noteFree(p);
#if defined(_MSC_VER)
    _aligned_free(p);
#else
    std::free(p);
#endif
}

void* orThrow(void* p)
{
    if (p == nullptr) throw std::bad_alloc();
    return p;
}

} // namespace

namespace grains_test {

AllocationScope::AllocationScope() noexcept
{
    gCount.store(0, std::memory_order_relaxed);
    gFrees.store(0, std::memory_order_relaxed);
    gActive.store(true, std::memory_order_relaxed);
}

AllocationScope::~AllocationScope() { gActive.store(false, std::memory_order_relaxed); }

std::size_t AllocationScope::count() const noexcept { return gCount.load(std::memory_order_relaxed); }

std::size_t AllocationScope::frees() const noexcept { return gFrees.load(std::memory_order_relaxed); }

} // namespace grains_test

// Replaced global allocation functions: plain and aligned families never mix.
void* operator new(std::size_t n) { return orThrow(plainAlloc(n)); }
void* operator new[](std::size_t n) { return orThrow(plainAlloc(n)); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept { return plainAlloc(n); }
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept { return plainAlloc(n); }
void* operator new(std::size_t n, std::align_val_t a) { return orThrow(alignedAlloc(n, static_cast<std::size_t>(a))); }
void* operator new[](std::size_t n, std::align_val_t a) { return orThrow(alignedAlloc(n, static_cast<std::size_t>(a))); }
void* operator new(std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return alignedAlloc(n, static_cast<std::size_t>(a)); }
void* operator new[](std::size_t n, std::align_val_t a, const std::nothrow_t&) noexcept { return alignedAlloc(n, static_cast<std::size_t>(a)); }

void operator delete(void* p) noexcept { plainFree(p); }
void operator delete[](void* p) noexcept { plainFree(p); }
void operator delete(void* p, std::size_t) noexcept { plainFree(p); }
void operator delete[](void* p, std::size_t) noexcept { plainFree(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { plainFree(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { plainFree(p); }
void operator delete(void* p, std::align_val_t) noexcept { alignedFree(p); }
void operator delete[](void* p, std::align_val_t) noexcept { alignedFree(p); }
void operator delete(void* p, std::size_t, std::align_val_t) noexcept { alignedFree(p); }
void operator delete[](void* p, std::size_t, std::align_val_t) noexcept { alignedFree(p); }
void operator delete(void* p, std::align_val_t, const std::nothrow_t&) noexcept { alignedFree(p); }
void operator delete[](void* p, std::align_val_t, const std::nothrow_t&) noexcept { alignedFree(p); }
