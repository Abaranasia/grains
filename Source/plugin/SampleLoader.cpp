// SPDX-License-Identifier: AGPL-3.0-or-later
#include "SampleLoader.h"

#include <grains/SampleBuilder.h>
#include <grains/SamplePathResolver.h>

#include <juce_events/juce_events.h>

#include <algorithm>
#include <new>
#include <string>
#include <vector>

namespace {
constexpr juce::int64 kChunkFrames = 65536;
constexpr int kHousekeepingMs = 250;
} // namespace

SampleLoader::SampleLoader(grains::SourceExchange<grains::SampleBuffer>& exchange, Callback onResult)
    : juce::Thread("GrainsSampleLoader"), exchange_(exchange), onResult_(std::move(onResult))
{
    startThread();
}

void SampleLoader::request(const juce::StringArray& candidates, const juce::String& original,
                           const juce::String& displayName, Origin origin)
{
    Request req{candidates, original, displayName, origin, 0}; // copies are made before lock_ is taken
    {
        const juce::ScopedLock sl(lock_);
        req.generation = gate_.next(); // lock_ then gate mutex, never the reverse
        pending_ = std::move(req);
    }
    notify();
}

void SampleLoader::stop()
{
    alive_->store(false);
    signalThreadShouldExit();
    notify();
    // No timeout, as in juce::Thread::~Thread: on timeout JUCE kills the thread by force, possibly
    // while it holds the gate mutex or the heap lock, just before gate_ and exchange_ are destroyed.
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
        std::optional<Request> req;
        {
            const juce::ScopedLock sl(lock_);
            req.swap(pending_);
        }
        if (req) load(*req);
    }
}

void SampleLoader::post(const Request& r, bool loaded, const juce::String& text, const juce::String& path,
                        bool relocated)
{
    Result res{r.generation, r.origin, loaded, path.isEmpty() ? r.original : path, text, relocated};
    juce::MessageManager::callAsync([this, alive = alive_, res = std::move(res)] {
        if (alive->load()) onResult_(res); // cleared by stop() on this same thread
    });
}

bool SampleLoader::commitGated(const Request& r, grains::SourceExchange<grains::SampleBuffer>::Staged s)
{
    grains::SourceExchange<grains::SampleBuffer>::Staged displaced;
    const bool accepted = gate_.commitIf(r.generation, [&]() noexcept { displaced = exchange_.commit(std::move(s)); });
    // displaced and any rejected s are destroyed here, after the gate unlocked
    if (accepted) exchange_.collect();
    return accepted;
}

void SampleLoader::load(const Request& r)
{
    const auto isCurrent = [&] { return !threadShouldExit() && gate_.current() == r.generation; };

    // Nothing stored at all: "No sample". A stored path with no usable candidate falls through
    // to the "Missing: <name>" failure below (R-SS-14).
    if (r.candidates.isEmpty() && r.origin == Origin::Restore && r.original.isEmpty()) {
        auto clear = exchange_.stageClear();
        if (!clear) { // Box allocation failed: keep the previous source
            if (isCurrent()) post(r, false, "Error: No sample");
            return;
        }
        if (commitGated(r, std::move(clear))) post(r, false, "No sample");
        return;
    }

    // Labels use the stored name until a candidate is found, then the found file's name.
    juce::String name = r.displayName.isNotEmpty() ? r.displayName : r.original;

    // Restore failure clears the source ("Missing"); a user load failure keeps the previous one.
    const auto fail = [&] {
        if (!isCurrent()) return;
        if (r.origin == Origin::Restore) {
            auto clear = exchange_.stageClear();
            if (!clear) { // Box allocation failed: keep the previous source
                post(r, false, "Error: " + name);
                return;
            }
            if (commitGated(r, std::move(clear))) post(r, false, "Missing: " + name);
        } else {
            post(r, false, "Error: " + name);
        }
    };

    std::vector<std::string> list;
    for (const auto& c : r.candidates) list.push_back(c.toStdString());
    const auto hit = grains::SamplePathResolver::resolve(list, [](const std::string& p) {
        const auto path = juce::String::fromUTF8(p.c_str());
        return juce::File::isAbsolutePath(path) && juce::File(path).existsAsFile();
    });
    if (!hit) return fail();
    const juce::String loadedPath = r.candidates[static_cast<int>(*hit)];
    const juce::File file(loadedPath);
    const bool relocated = !grains::SamplePathResolver::samePath(list[*hit], r.original.toStdString());
    name = file.getFileName();
    post(r, false, "Loading: " + name);

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
            if (!isCurrent()) return; // aborted: shutdown or a newer request
            const int n = static_cast<int>(std::min(kChunkFrames, len - pos));
            if (!reader->read(&buf, static_cast<int>(pos), n, pos, true, chans > 1)) return fail();
        }

        const grains::ChannelView views[2] = {{buf.getReadPointer(0), static_cast<std::size_t>(len)},
                                              {chans > 1 ? buf.getReadPointer(1) : nullptr, static_cast<std::size_t>(len)}};
        built = grains::buildSampleBuffer(views, static_cast<std::size_t>(chans), reader->sampleRate);
    }
    if (built.error != grains::BuildError::None) return fail();
    if (!isCurrent()) return;

    auto staged = exchange_.stage(std::move(built.buffer));
    if (!staged) { // Box allocation failed: keep the previous source
        post(r, false, "Error: " + name);
        return;
    }
    if (!commitGated(r, std::move(staged))) return; // superseded: the Box is freed here, not published
    post(r, true,
         "Loaded: " + name + (relocated ? " (relocated)" : "") + (truncated ? " (truncated to 5 min)" : ""),
         loadedPath, relocated);
}
