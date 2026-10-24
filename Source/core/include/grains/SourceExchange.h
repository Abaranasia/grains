#pragma once

#include <atomic>
#include <memory>
#include <new>
#include <utility>

namespace grains {

// Single-producer / single-consumer mailbox that hands ownership of an immutable T from a loader
// thread to the audio thread without locks, allocation or frees on the consumer side.
//
// Producer: publish(next); collect();  (collect MUST follow every publish, see below)
// Consumer: acquire() once per block; call engine.setSource(r.source) whenever r.changed.
//
// The consumer keeps the adopted Box in current_. Adopting a newer Box moves the old one into the
// single retired_ slot; only the producer deletes it (collect). If that slot is still full,
// acquire defers adoption (keeps current, pending stays), so the consumer never frees.
// publish is exchange-only: it frees an unseen pending Box but never touches retired_. Because the
// producer collects after the new Box is pending, a deferral never strands a pending Box.
// Threading: one producer and one consumer at a time; the destructor needs neither to be active.
template <typename T>
class SourceExchange {
public:
    struct Acquired {
        const T* source; // nullptr when cleared or nothing was ever published
        bool changed;    // true when `source` differs from the previous acquire
    };

    SourceExchange() = default;
    SourceExchange(const SourceExchange&) = delete;
    SourceExchange& operator=(const SourceExchange&) = delete;
    ~SourceExchange()
    {
        delete pending_.load(std::memory_order_relaxed);
        delete retired_.load(std::memory_order_relaxed);
        delete current_;
    }

    // Producer. nullptr clears the source. Returns false only if the Box allocation fails (state
    // unchanged). An unseen pending Box is superseded and freed here.
    bool publish(std::unique_ptr<T> next) noexcept
    {
        Box* box = new (std::nothrow) Box{std::move(next)};
        if (box == nullptr) return false;
        delete pending_.exchange(box, std::memory_order_acq_rel);
        return true;
    }

    // Producer. Deletes the retired Box (pairs with the consumer's release store).
    void collect() noexcept { delete retired_.exchange(nullptr, std::memory_order_acquire); }

    // Consumer. Wait-free; never allocates or frees.
    Acquired acquire() noexcept
    {
        if (retired_.load(std::memory_order_relaxed) != nullptr) return currentView(false);
        Box* next = pending_.exchange(nullptr, std::memory_order_acquire);
        if (next == nullptr) return currentView(false);
        if (current_ != nullptr) retired_.store(current_, std::memory_order_release);
        current_ = next;
        return currentView(true);
    }

private:
    struct Box {
        std::unique_ptr<T> value;
    };

    Acquired currentView(bool changed) const noexcept
    {
        return {current_ != nullptr ? current_->value.get() : nullptr, changed};
    }

    static_assert(std::atomic<Box*>::is_always_lock_free, "SourceExchange needs lock-free pointer atomics");

    std::atomic<Box*> pending_{nullptr};
    std::atomic<Box*> retired_{nullptr};
    Box* current_ = nullptr; // consumer-only
};

} // namespace grains
