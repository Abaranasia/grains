// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <type_traits>

namespace grains {

// Wait-free single-producer / single-consumer "latest value" mailbox for a plain T (R-WD-6).
// Three slots rotate between the producer, the consumer and a middle hand-over slot, so neither side
// ever waits, locks, allocates or sees a torn value.
//
// Producer: fill back() in place, then publish(). back() always returns a slot nobody else reads.
// Consumer: update() adopts the newest published value, if any (true = a newer T than the last one
//           adopted); front() is then stable until the next update(). Without a publish in between,
//           update() returns false and front() keeps its value. Before any publish, front() is T{}.
// Unconsumed frames are overwritten: only the latest published value ever reaches the consumer.
// Threading: one producer and one consumer at a time, as in SourceExchange. The producer role may
// move between threads only when something serialises them (the host does so for prepareToPlay
// against processBlock).
template <class T>
class TripleBuffer {
    static_assert(std::is_nothrow_default_constructible<T>::value, "T must be nothrow default-constructible");
    static_assert(std::atomic<std::uint8_t>::is_always_lock_free, "TripleBuffer needs lock-free atomics");

    static constexpr std::uint8_t kIndex = 0x3; // bits 0-1: slot index held in the middle
    static constexpr std::uint8_t kFresh = 0x4; // bit 2: the middle slot holds an unconsumed frame

public:
    // Producer: the slot to fill in place. Stable until the next publish().
    T& back() noexcept { return slots_[back_].value; }

    // Producer: hands back() to the consumer and recycles the slot the consumer left in the middle.
    // The release half publishes the slot writes; the acquire half orders the consumer's earlier
    // reads of the returned slot before the producer's next writes to it.
    void publish() noexcept { back_ = middle_.exchange(static_cast<std::uint8_t>(back_ | kFresh), std::memory_order_acq_rel) & kIndex; }

    // Consumer: true if a newer T was adopted. A stale view of the flag only delays the frame by one
    // poll. The acquire half sees the producer's writes; the release half hands back the consumer's reads.
    bool update() noexcept
    {
        if ((middle_.load(std::memory_order_relaxed) & kFresh) == 0) return false;
        front_ = middle_.exchange(front_, std::memory_order_acq_rel) & kIndex;
        return true;
    }

    // Consumer: the adopted value, stable until the next update().
    const T& front() const noexcept { return slots_[front_].value; }

private:
// Each slot and control word gets its own cache line (no false sharing); MSVC reports the padding.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
    struct alignas(64) Slot {
        T value{};
    };
    Slot slots_[3];
    alignas(64) std::atomic<std::uint8_t> middle_{1}; // hand-over slot: bits 0-1 index, bit 2 fresh
    alignas(64) std::uint8_t back_ = 2;               // producer-owned
    alignas(64) std::uint8_t front_ = 0;              // consumer-owned
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
};

} // namespace grains
