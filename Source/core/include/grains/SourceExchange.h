#pragma once

#include <atomic>
#include <memory>
#include <new>
#include <utility>

namespace grains {

// Single-producer / single-consumer mailbox that hands ownership of an immutable T from a loader
// thread to the audio thread without locks, allocation or frees on the consumer side.
//
// Producer: publish(next) or stage(next) + commit(staged); then collect(). collect MUST follow every
//           accepted publish/commit (a commit the caller rejected changes nothing, so needs none).
// Consumer: acquire() once per block; call engine.setSource(r.source) whenever r.changed.
//
// The consumer keeps the adopted Box in current_. Adopting a newer Box moves the old one into the
// single retired_ slot; only the producer deletes it (collect). If that slot is still full,
// acquire defers adoption (keeps current, pending stays), so the consumer never frees.
// commit is exchange-only: it never touches retired_ and never frees; it returns the displaced
// unseen Box (inside a Staged) so the caller can free it outside any lock. publish is stage +
// commit and frees that Box itself. Because the producer collects after the new Box is pending, a
// deferral never strands a pending Box.
// Threading: one producer and one consumer at a time; the destructor needs neither to be active.
template <typename T>
class SourceExchange {
    struct Box {
        std::unique_ptr<T> value;
    };

public:
    // Move-only handle to an allocated, not yet published Box (or empty). Destroying it frees the Box.
    class Staged {
    public:
        Staged() noexcept = default;
        Staged(Staged&&) noexcept = default;
        Staged& operator=(Staged&&) noexcept = default;
        explicit operator bool() const noexcept { return box_ != nullptr; }

    private:
        friend class SourceExchange;
        explicit Staged(Box* box) noexcept : box_(box) {}
        std::unique_ptr<Box> box_;
    };

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

    // Producer. Allocates the Box (no lock needed). nullptr stages a clear. Empty on allocation
    // failure.
    Staged stage(std::unique_ptr<T> next) noexcept { return Staged(new (std::nothrow) Box{std::move(next)}); }
    Staged stageClear() noexcept { return stage(nullptr); }

    // Producer. Makes s the pending source and returns the displaced unseen Box (empty if none).
    // Never allocates or frees; an empty s is a no-op (never exchanges a null Box into pending_).
    Staged commit(Staged&& s) noexcept
    {
        if (!s) return Staged();
        return Staged(pending_.exchange(s.box_.release(), std::memory_order_acq_rel));
    }

    // Producer. nullptr clears the source. Returns false only if the Box allocation fails (state
    // unchanged). An unseen pending Box is superseded and freed here.
    bool publish(std::unique_ptr<T> next) noexcept
    {
        Staged s = stage(std::move(next));
        if (!s) return false;
        commit(std::move(s)); // the displaced Box is freed by the returned Staged
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
