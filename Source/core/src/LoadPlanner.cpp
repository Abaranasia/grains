// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/LoadPlanner.h>

#include <cmath>
#include <utility>

namespace grains {

namespace {
LoadRequest makeReload(const PublishedSource& p, double rate)
{
    LoadRequest req;
    req.candidates = {p.path};
    req.original = p.original;
    req.displayName = p.displayName;
    req.origin = LoadOrigin::Reload;
    req.targetRate = rate;
    req.label = p.label;
    return req;
}
} // namespace

std::uint64_t LoadPlanner::request(LoadRequest r)
{
    LoadRequest latest = r; // copies are made before mutex_ is taken, so a throw changes nothing
    const std::lock_guard<std::mutex> lock(mutex_);
    r.generation = gate_.next(); // mutex_ then gate mutex, never the reverse
    r.targetRate = target_;
    latest.generation = r.generation;
    latest.targetRate = r.targetRate;
    latest_ = std::move(latest);
    pending_ = std::move(r);
    return latest_->generation;
}

bool LoadPlanner::retarget(double rate) noexcept
{
    if (!std::isfinite(rate) || rate <= 0.0) return false;
    try {
        const std::lock_guard<std::mutex> lock(mutex_);
        if (rate == target_) return false;
        if (latest_ && latest_->generation != settled_) {
            // Still in flight (or just committed and not yet marked): redo it at the new rate. The
            // older run notices the new generation and aborts, so the file is decoded once.
            LoadRequest req = *latest_;
            LoadRequest copy = req; // both copies exist before any state changes
            target_ = rate;
            req.targetRate = rate;
            req.generation = gate_.next();
            copy.targetRate = rate;
            copy.generation = req.generation;
            latest_ = std::move(copy);
            pending_ = std::move(req);
        } else if (published_ && published_->rate != rate) {
            LoadRequest req = makeReload(*published_, rate);
            LoadRequest copy = req;
            target_ = rate;
            req.generation = gate_.next();
            copy.generation = req.generation;
            latest_ = std::move(copy);
            pending_ = std::move(req);
        } else {
            target_ = rate;
            return false;
        }
        return true;
    } catch (...) {
        return false; // a copy failed before any state changed: nothing issued
    }
}

std::optional<LoadRequest> LoadPlanner::take() noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    std::optional<LoadRequest> out;
    out.swap(pending_);
    return out;
}

void LoadPlanner::published(PublishedSource p) noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    published_ = std::move(p);
}

void LoadPlanner::cleared() noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    published_.reset();
}

bool LoadPlanner::settle(std::uint64_t gen, LoadOutcome outcome, std::string failureStatus) noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    settled_ = gen;
    // R4-001: only a failure of the latest, non-Reload request with a known target and a published
    // source at another rate. A failed Reload is never retried, so a deleted file cannot loop.
    if (outcome != LoadOutcome::Failed || !latest_ || latest_->generation != gen) return false;
    if (latest_->origin == LoadOrigin::Reload || !(target_ > 0.0)) return false;
    if (!published_ || published_->rate == target_) return false;
    try {
        LoadRequest req = makeReload(*published_, target_);
        // published_->label is untouched: only this request carries the failure line
        if (!failureStatus.empty()) req.label = std::move(failureStatus);
        LoadRequest copy = req; // both copies exist before any state changes
        req.generation = gate_.next();
        copy.generation = req.generation;
        latest_ = std::move(copy);
        pending_ = std::move(req);
        return true;
    } catch (...) {
        return false; // a copy failed before any state changed: nothing issued
    }
}

double LoadPlanner::target() const noexcept
{
    const std::lock_guard<std::mutex> lock(mutex_);
    return target_;
}

} // namespace grains
