#pragma once

#include <grains/SampleBuffer.h>
#include <grains/SourceExchange.h>

#include <juce_audio_formats/juce_audio_formats.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

// Background decoder: reads a file, builds a SampleBuffer and publishes it to the exchange. The
// latest request wins (generation counter). Results are delivered on the message thread.
class SampleLoader final : private juce::Thread {
public:
    enum class Origin { User, Restore };

    struct Result {
        std::uint64_t generation;
        Origin origin;
        bool loaded;       // true only for a successful publish
        juce::String path; // the requested path
        juce::String text; // status line for the UI
    };
    using Callback = std::function<void(const Result&)>;

    SampleLoader(grains::SourceExchange<grains::SampleBuffer>& exchange, Callback onResult);
    ~SampleLoader() override { stop(); }

    // Any thread. Empty path + Restore clears the source; a non-absolute path counts as missing.
    void request(const juce::String& path, Origin origin);
    std::uint64_t latestGeneration() const noexcept { return gen_.load(); }

    // Message thread only. Idempotent; call before the exchange is destroyed.
    void stop();

private:
    struct Request {
        juce::String path;
        Origin origin;
        std::uint64_t generation;
    };

    void run() override;
    void load(const Request& r);
    void post(const Request& r, bool loaded, const juce::String& text);

    grains::SourceExchange<grains::SampleBuffer>& exchange_;
    Callback onResult_;
    std::shared_ptr<std::atomic<bool>> alive_ = std::make_shared<std::atomic<bool>>(true);
    std::atomic<std::uint64_t> gen_{0};
    juce::CriticalSection lock_;
    std::optional<Request> pending_; // guarded by lock_
};
