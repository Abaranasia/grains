// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleLoader.h"

#include <grains/SampleBuilder.h>
#include <grains/SamplePathResolver.h>

#include <juce_events/juce_events.h>

#include <algorithm>
#include <cmath>
#include <new>
#include <string>
#include <vector>

namespace {
constexpr juce::int64 kChunkFrames = 65536;
constexpr int kHousekeepingMs = 250;

juce::String fromUtf8(const std::string& s) { return juce::String::fromUTF8(s.c_str()); }
} // namespace

SampleLoader::SampleLoader(grains::SourceExchange<grains::SampleBuffer>& exchange, Callback onResult)
    : juce::Thread("GrainsSampleLoader"), exchange_(exchange), onResult_(std::move(onResult))
{
    startThread();
}

void SampleLoader::request(const juce::StringArray& candidates, const juce::String& original,
                           const juce::String& displayName, Origin origin)
{
    grains::LoadRequest req;
    for (const auto& c : candidates) req.candidates.push_back(c.toStdString());
    req.original = original.toStdString();
    req.displayName = displayName.toStdString();
    req.origin = origin;
    planner_.request(std::move(req));
    notify();
}

bool SampleLoader::retarget(double rate)
{
    if (!planner_.retarget(rate)) return false;
    notify();
    return true;
}

void SampleLoader::stop()
{
    alive_->store(false);
    signalThreadShouldExit();
    notify();
    // No timeout, as in juce::Thread::~Thread: on timeout JUCE kills the thread by force, possibly
    // while it holds the gate mutex or the heap lock, just before the planner's gate and exchange_ are destroyed.
    // A read stalled on a network share can delay shutdown but never corrupt it. Cannot deadlock:
    // the loader never waits on the message thread (post() only queues callAsync) and we hold no lock.
    stopThread(-1);
}

void SampleLoader::run()
{
    while (!threadShouldExit()) {
        wait(kHousekeepingMs);
        if (threadShouldExit()) break;
        exchange_.collect(); // release a retired buffer promptly
        if (const auto req = planner_.take()) load(*req);
    }
}

void SampleLoader::load(const grains::LoadRequest& r)
{
    lastText_.clear();
    const auto outcome = loadImpl(r);
    // R4-001: a stale-rate source is being reloaded. The failed run's last post is its failure line;
    // the Reload (the new latest, so the only post the processor keeps) reposts it (R-SS-15).
    std::string failure = outcome == grains::LoadOutcome::Failed ? std::move(lastText_) : std::string();
    if (planner_.settle(r.generation, outcome, std::move(failure))) notify();
}

void SampleLoader::post(const grains::LoadRequest& r, bool loaded, const juce::String& text, const juce::String& path,
                        bool relocated)
{
    lastText_ = text.toStdString(); // loader thread only
    Result res{r.generation, r.origin, loaded, path.isEmpty() ? fromUtf8(r.original) : path, text, relocated};
    juce::MessageManager::callAsync([this, alive = alive_, res = std::move(res)] {
        if (alive->load()) onResult_(res); // cleared by stop() on this same thread
    });
}

bool SampleLoader::commitGated(const grains::LoadRequest& r, grains::SourceExchange<grains::SampleBuffer>::Staged s)
{
    grains::SourceExchange<grains::SampleBuffer>::Staged displaced;
    const bool accepted = planner_.gate().commitIf(r.generation, [&]() noexcept { displaced = exchange_.commit(std::move(s)); });
    // displaced and any rejected s are destroyed here, after the gate unlocked
    if (accepted) exchange_.collect();
    return accepted;
}

grains::LoadOutcome SampleLoader::loadImpl(const grains::LoadRequest& r)
{
    using grains::LoadOutcome;
    const auto isCurrent = [&] { return !threadShouldExit() && planner_.gate().current() == r.generation; };

    // Nothing stored at all: "No sample". A stored path with no usable candidate falls through
    // to the "Missing: <name>" failure below (R-SS-14).
    if (r.candidates.empty() && r.origin == Origin::Restore && r.original.empty()) {
        auto clear = exchange_.stageClear();
        if (!clear) { // Box allocation failed: keep the previous source
            if (!isCurrent()) return LoadOutcome::Aborted;
            post(r, false, "Error: No sample");
            return LoadOutcome::Failed;
        }
        if (!commitGated(r, std::move(clear))) return LoadOutcome::Aborted;
        planner_.cleared(); // after the gate released
        post(r, false, "No sample");
        return LoadOutcome::Cleared;
    }

    // Labels use the stored name until a candidate is found, then the found file's name.
    juce::String name = fromUtf8(r.displayName.empty() ? r.original : r.displayName);

    // Restore failure clears the source ("Missing"); a user load failure keeps the previous one.
    const auto fail = [&]() -> LoadOutcome {
        if (!isCurrent()) return LoadOutcome::Aborted;
        if (r.origin == Origin::Reload) { // keep the live buffer and its label; never "Missing"
            post(r, false, fromUtf8(r.label));
            return LoadOutcome::Failed;
        }
        if (r.origin == Origin::Restore) {
            auto clear = exchange_.stageClear();
            if (!clear) { // Box allocation failed: keep the previous source
                post(r, false, "Error: " + name);
                return LoadOutcome::Failed;
            }
            if (!commitGated(r, std::move(clear))) return LoadOutcome::Aborted;
            planner_.cleared(); // after the gate released
            post(r, false, "Missing: " + name);
        } else {
            post(r, false, "Error: " + name);
        }
        return LoadOutcome::Failed;
    };

    const auto hit = grains::SamplePathResolver::resolve(r.candidates, [](const std::string& p) {
        const auto path = juce::String::fromUTF8(p.c_str());
        return juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile();
    });
    if (!hit) return fail();
    const juce::String loadedPath = fromUtf8(r.candidates[*hit]);
    const juce::File file(loadedPath);
    const bool relocated = !grains::SamplePathResolver::samePath(r.candidates[*hit], r.original);
    name = file.getFileName();
    if (r.origin != Origin::Reload) post(r, false, "Loading: " + name); // a rate-change reload is silent

    grains::BuildResult built;
    bool truncated = false;
    {
        juce::AudioFormatManager formats;
        formats.registerBasicFormats();
        const std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        if (reader == nullptr || reader->numChannels == 0 || reader->lengthInSamples <= 0) return fail();

        const auto cap = static_cast<juce::int64>(grains::maxSourceFrames(reader->sampleRate));
        const juce::int64 len = std::min<juce::int64>(reader->lengthInSamples, cap);
        if (len <= 0) return fail();
        truncated = reader->lengthInSamples > len;

        const int chans = static_cast<int>(std::min<unsigned>(reader->numChannels, 2));
        juce::AudioBuffer<float> buf;
        try {
            buf.setSize(chans, static_cast<int>(len));
        } catch (const std::bad_alloc&) {
            return fail();
        }
        for (juce::int64 pos = 0; pos < len; pos += kChunkFrames) {
            if (!isCurrent()) return LoadOutcome::Aborted; // shutdown or a newer request
            const int n = static_cast<int>(std::min(kChunkFrames, len - pos));
            if (!reader->read(&buf, static_cast<int>(pos), n, pos, true, chans > 1)) return fail();
        }

        const grains::ChannelView views[2] = {{buf.getReadPointer(0), static_cast<std::size_t>(len)},
                                              {chans > 1 ? buf.getReadPointer(1) : nullptr, static_cast<std::size_t>(len)}};
        if (r.targetRate > 0.0)
            built = grains::buildSampleBuffer(views, static_cast<std::size_t>(chans), reader->sampleRate,
                                              r.targetRate, [&] { return !isCurrent(); });
        else
            built = grains::buildSampleBuffer(views, static_cast<std::size_t>(chans), reader->sampleRate);
    } // the decode buffer is freed here, before the source is staged
    if (built.error == grains::BuildError::Aborted) return LoadOutcome::Aborted; // superseded: nothing to publish or clear
    if (built.error != grains::BuildError::None) return fail();
    if (!isCurrent()) return LoadOutcome::Aborted;
    truncated = truncated || built.truncated;
    const double publishedRate = built.buffer->sampleRate();

    auto staged = exchange_.stage(std::move(built.buffer));
    if (!staged) { // Box allocation failed: keep the previous source
        post(r, false, "Error: " + name);
        return LoadOutcome::Failed;
    }
    if (!commitGated(r, std::move(staged))) return LoadOutcome::Aborted; // superseded: the Box is freed here
    const juce::String label =
        "Loaded: " + name + (relocated ? " (relocated)" : "") + (truncated ? " (truncated to 5 min)" : "");
    // After the gate released: the gate callback must not call the planner. Until this runs,
    // retarget() sees the request as in flight, which only costs a redundant reissue.
    planner_.published({loadedPath.toStdString(), r.original, r.displayName, label.toStdString(), publishedRate});
    // A Reload keeps the line it was issued with: the published label, or the failure line of the
    // request it follows up (R-SS-15). The published source's own label stays "Loaded: ...".
    const bool keepStatus = r.origin == Origin::Reload && !r.label.empty();
    post(r, true, keepStatus ? fromUtf8(r.label) : label, loadedPath, relocated);
    return LoadOutcome::Published;
}
