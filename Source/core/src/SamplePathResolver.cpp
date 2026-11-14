// SPDX-License-Identifier: AGPL-3.0-or-later
#include <grains/SamplePathResolver.h>

#include <string>
#include <utility>
#include <vector>

namespace grains {

namespace {

bool isSep(char c) noexcept { return c == '\\' || c == '/'; }

bool isAsciiAlpha(char c) noexcept { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

char asciiLower(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

bool isPosixShaped(const std::string& p) noexcept { return !p.empty() && p[0] == '/'; }

// Comparison key: Windows shape uses '\' and ASCII lowercase; every shape collapses repeated
// separators (after a UNC prefix) and drops one trailing separator unless it is the root.
std::string normalise(const std::string& p)
{
    const bool win = SamplePathResolver::isWindowsShaped(p);
    const char sep = win ? '\\' : '/';
    const bool unc = win && p.size() >= 2 && p[0] == '\\' && p[1] == '\\';
    std::string out;
    out.reserve(p.size());
    if (unc) out.assign(2, sep);
    for (std::size_t i = unc ? 2 : 0; i < p.size(); ++i) {
        char c = p[i];
        if (win) {
            c = isSep(c) ? sep : asciiLower(c);
        }
        if (c == sep && !out.empty() && out.back() == sep && !(unc && out.size() < 2)) continue;
        out.push_back(c);
    }
    const std::size_t rootLen = unc ? 2 : (win ? 3 : 1);
    if (out.size() > rootLen && out.back() == sep) out.pop_back();
    return out;
}

// Non-empty, non-"." components of p, split by the separators of its shape. sawDotDot reports a
// ".." component.
std::vector<std::string> components(const std::string& p, bool& sawDotDot)
{
    const bool posix = isPosixShaped(p);
    std::vector<std::string> out;
    sawDotDot = false;
    std::string cur;
    const auto flush = [&] {
        if (cur == "..") sawDotDot = true;
        if (!cur.empty() && cur != ".") out.push_back(cur);
        cur.clear();
    };
    for (const char c : p) {
        if (posix ? c == '/' : isSep(c)) {
            flush();
        } else {
            cur.push_back(c);
        }
    }
    flush();
    return out;
}

bool sameComponent(const std::string& a, const std::string& b, bool caseInsensitive) noexcept
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const bool eq = caseInsensitive ? asciiLower(a[i]) == asciiLower(b[i]) : a[i] == b[i];
        if (!eq) return false;
    }
    return true;
}

} // namespace

bool SamplePathResolver::isWindowsShaped(const std::string& p) noexcept
{
    if (p.size() >= 2 && p[0] == '\\' && p[1] == '\\') return true;
    return p.size() >= 3 && isAsciiAlpha(p[0]) && p[1] == ':' && isSep(p[2]);
}

bool SamplePathResolver::isAbsolute(const std::string& p) noexcept { return isWindowsShaped(p) || isPosixShaped(p); }

std::string SamplePathResolver::fileName(const std::string& p)
{
    const bool posix = isPosixShaped(p);
    std::size_t start = 0;
    for (std::size_t i = 0; i < p.size(); ++i) {
        if (posix ? p[i] == '/' : isSep(p[i])) start = i + 1;
    }
    return p.substr(start);
}

bool SamplePathResolver::samePath(const std::string& a, const std::string& b) { return normalise(a) == normalise(b); }

SampleRef SamplePathResolver::makeRef(const std::string& absolute, const std::vector<PathAnchor>& anchors)
{
    SampleRef ref;
    ref.absolute = absolute;
    ref.name = fileName(absolute);

    bool pathDotDot = false;
    const std::vector<std::string> parts = components(absolute, pathDotDot);
    if (ref.name.empty() || pathDotDot || !isAbsolute(absolute)) return ref;

    const bool win = isWindowsShaped(absolute);
    const PathAnchor* best = nullptr;
    std::size_t bestLen = 0;
    for (const PathAnchor& a : anchors) {
        if (a.key.empty() || !isAbsolute(a.dir) || isWindowsShaped(a.dir) != win) continue;
        bool anchorDotDot = false;
        const std::vector<std::string> base = components(a.dir, anchorDotDot);
        if (anchorDotDot || base.size() <= bestLen || base.size() >= parts.size()) continue;
        bool match = true;
        for (std::size_t i = 0; match && i < base.size(); ++i) match = sameComponent(base[i], parts[i], win);
        if (!match) continue;
        best = &a;
        bestLen = base.size();
    }
    if (best == nullptr) return ref;

    ref.anchorKey = best->key;
    for (std::size_t i = bestLen; i < parts.size(); ++i) {
        if (i > bestLen) ref.relative.push_back('/');
        ref.relative += parts[i];
    }
    return ref;
}

namespace {

// A stored relative path: '/'-separated, not absolute, no empty, "." or ".." component.
bool validRelative(const std::string& rel, bool posixShape)
{
    if (rel.empty() || rel.front() == '/' || (!posixShape && SamplePathResolver::isWindowsShaped(rel))) return false;
    std::size_t start = 0;
    while (start <= rel.size()) {
        std::size_t end = rel.find('/', start);
        if (end == std::string::npos) end = rel.size();
        const std::string part = rel.substr(start, end - start);
        if (part.empty() || part == "." || part == "..") return false;
        start = end + 1;
    }
    return true;
}

bool validName(const std::string& name, bool posixShape)
{
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos) return false;
    return posixShape || name.find('\\') == std::string::npos;
}

// dir + separator + tail, in the separator style of dir ('/' in tail becomes '\' for Windows dirs).
std::string join(const std::string& dir, const std::string& tail)
{
    const bool win = SamplePathResolver::isWindowsShaped(dir);
    const auto styled = [win](char c) { return win && c == '/' ? '\\' : c; };
    std::string out;
    for (const char c : dir) out.push_back(styled(c));
    while (!out.empty() && out.back() == (win ? '\\' : '/')) out.pop_back();
    out.push_back(win ? '\\' : '/');
    for (const char c : tail) out.push_back(styled(c));
    return out;
}

} // namespace

SampleRef SamplePathResolver::restoreRef(std::string absolute, std::string name, std::string anchorKey,
                                         std::string relative)
{
    SampleRef ref;
    if (absolute.empty()) return ref;
    const bool posix = isPosixShaped(absolute);
    if (!posix) {
        for (char& c : relative) {
            if (c == '\\') c = '/';
        }
    }
    ref.name = name.empty() ? fileName(absolute) : std::move(name);
    if (!anchorKey.empty() && validRelative(relative, posix)) {
        ref.anchorKey = std::move(anchorKey);
        ref.relative = std::move(relative);
    }
    ref.absolute = std::move(absolute);
    return ref;
}

std::vector<std::string> SamplePathResolver::candidates(const SampleRef& ref, const std::vector<PathAnchor>& anchors,
                                                        const std::vector<std::string>& searchDirs)
{
    std::vector<std::string> out;
    if (ref.absolute.empty()) return out;
    const bool posix = isPosixShaped(ref.absolute);

    const auto add = [&out](std::string path) {
        if (out.size() >= kMaxCandidates) return;
        for (const std::string& seen : out) {
            if (samePath(seen, path)) return;
        }
        out.push_back(std::move(path));
    };

    if (!ref.anchorKey.empty() && validRelative(ref.relative, posix)) {
        for (const PathAnchor& a : anchors) {
            if (a.key == ref.anchorKey && isAbsolute(a.dir)) {
                add(join(a.dir, ref.relative));
                break;
            }
        }
    }
    if (isAbsolute(ref.absolute)) add(ref.absolute);

    const std::string name = validName(ref.name, posix) ? ref.name : fileName(ref.absolute);
    if (validName(name, posix)) {
        for (const std::string& dir : searchDirs) {
            if (isAbsolute(dir)) add(join(dir, name));
        }
    }
    return out;
}
std::optional<std::size_t> SamplePathResolver::resolve(const std::vector<std::string>& candidates,
                                                       const std::function<bool(const std::string&)>& exists)
{
    for (std::size_t i = 0; i < candidates.size(); ++i) {
        if (exists(candidates[i])) return i;
    }
    return std::nullopt;
}

} // namespace grains
