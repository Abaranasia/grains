// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace grains {

// A named base directory on this machine (e.g. "music"). The key is stable across machines.
struct PathAnchor {
    std::string key;
    std::string dir;
};

// A sample's stored reference. anchorKey is empty exactly when relative is empty.
struct SampleRef {
    std::string absolute;  // as stored in samplePath; may be empty
    std::string name;      // last path component
    std::string anchorKey; // empty <=> relative empty
    std::string relative;  // '/'-separated; no leading separator; no "", ".", ".." components
};

// Pure path rules on std::string. The shape comes from the path text (drive or UNC = Windows,
// leading '/' = POSIX), never from the host OS, and nothing touches the filesystem.
struct SamplePathResolver final {
    static constexpr std::size_t kMaxCandidates = 10;

    static bool isWindowsShaped(const std::string& p) noexcept;
    static bool isAbsolute(const std::string& p) noexcept;
    static std::string fileName(const std::string& p);
    // Equality of the normalised comparison keys (Windows shape: case- and separator-insensitive).
    static bool samePath(const std::string& a, const std::string& b);
    // A path with a ".." component never gets an anchor.
    static SampleRef makeRef(const std::string& absolute, const std::vector<PathAnchor>& anchors);
    // Sanitises loaded state: a missing name is derived from absolute, an invalid anchor pair is
    // cleared, '\' in relative becomes '/' (non-POSIX shapes), and an empty absolute clears the rest.
    static SampleRef restoreRef(std::string absolute, std::string name, std::string anchorKey, std::string relative);
    // Order: local anchor + relative, the stored absolute path, then each search dir + name.
    // Deduplicated with samePath and capped at kMaxCandidates.
    static std::vector<std::string> candidates(const SampleRef& ref, const std::vector<PathAnchor>& anchors,
                                               const std::vector<std::string>& searchDirs);
    // Index of the first candidate for which exists() is true; stops at the first hit.
    static std::optional<std::size_t> resolve(const std::vector<std::string>& candidates,
                                              const std::function<bool(const std::string&)>& exists);
};

} // namespace grains
