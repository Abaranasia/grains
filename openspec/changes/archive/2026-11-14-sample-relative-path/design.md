# Design: Relocatable sample path

## Technical Approach

A pure, JUCE-free core unit (`grains::SamplePathResolver`, `std::string` only) owns every path rule: shape detection, normalisation, anchor matching, candidate ordering, dedup and the first-existing choice through an injected predicate. The plugin only gathers the local anchors, reads and writes four APVTS properties, and passes a candidate list to `SampleLoader`, which probes on its own thread. Note: the plugin files live in `Source/plugin/`, not `Source/` as the proposal says.

**JUCE check.** The pin is JUCE 9.0.0 (`cmake/JucePin.cmake`), checked in `build/plugin-jf2/_deps/juce-src/modules`. `juce_audio_processors` and `juce_audio_processors_headless/processors/juce_AudioProcessor.h` have no project or session directory API. The only hit is PreSonus `pslextensions/ipslcontextinfo.h:181` (`kDocumentFolder`). It is VST3-only, works only in Studio One, and `AudioProcessor` does not expose it, so it is rejected. `File::getSpecialLocation` provides `userMusicDirectory`, `userDocumentsDirectory`, `userDesktopDirectory` and `userHomeDirectory` (`juce_File.h:886-898`).

## Architecture Decisions

| Decision | Options | Tradeoff | Choice |
|---|---|---|---|
| Unit shape | free functions; a stateless `struct` with static members | The orchestrator named `grains::SamplePathResolver` | `struct SamplePathResolver final` with static functions only |
| Path shape | host OS; shape of the path itself | Host OS breaks restore across OSes | Shape: drive `X:\` or `X:/`, or UNC `\\` means Windows; a leading `/` means POSIX; anything else is not absolute |
| Case | always sensitive; Windows-shaped only | NTFS is case-insensitive | ASCII-lowercase the comparison key for Windows-shaped paths only. The original case is kept in the output |
| Name split | both separators; shape-aware | `\` is legal in POSIX names | Windows-shaped or unknown shape: split on both separators. POSIX-shaped paths split on `/` only. This is a justified deviation from explore.md, backed by the spec scenario "Backslash is a filename character on POSIX" |
| Stored `relative` | native separators; always `/` | Must be portable | Always `/` (design wins over the old spec text `loops\kick.wav`; the spec now says `loops/kick.wav`). A join converts it to the anchor's style. `restoreRef` normalises any `\` in a stored relative path to `/` when the ref's shape is Windows or unknown; for a POSIX-shaped absolute path `\` is kept (legal filename character) |
| Empty `samplePath` | honour name/anchor; ignore them | R-SS-13 / R-SS-9 | `restoreRef` and `candidates` give no candidates whenever `absolute` is empty, regardless of name, anchorKey or relative (spec wins) |
| Anchor match | plain prefix; component boundary | A plain prefix lets `C:\Music2` match `C:\Music` | Normalised prefix followed by a separator. Both paths must have the same shape. The longest match wins; on a tie, the first anchor in the list wins |
| Search dirs | recursive; anchors only | Recursion is slow and ambiguous | Exactly the local anchor directories, non-recursive |
| Probing | host thread; loader thread | Network shares can stall | Loader thread, sequential, at most `kMaxCandidates = 10` (4 anchors give at most 6) |
| Relocated flag | processor; loader | The loader builds the status text | The loader compares the chosen path with the stored absolute path using `samePath` |

## Interfaces / Contracts

```cpp
// Source/core/include/grains/SamplePathResolver.h
namespace grains {
struct PathAnchor { std::string key; std::string dir; };   // key is stable across machines: "music", ...
struct SampleRef {
    std::string absolute;   // as stored in samplePath; may be empty
    std::string name;       // last component
    std::string anchorKey;  // empty <=> relative empty
    std::string relative;   // '/'-separated; no leading separator; no "", ".", ".." components
};
struct SamplePathResolver final {
    static constexpr std::size_t kMaxCandidates = 10;
    static bool isWindowsShaped(const std::string& p) noexcept;
    static bool isAbsolute(const std::string& p) noexcept;     // shape-based
    static std::string fileName(const std::string& p);
    static bool samePath(const std::string& a, const std::string& b); // normalised key equality
    // Paths containing a ".." component never get an anchor.
    static SampleRef makeRef(const std::string& absolute, const std::vector<PathAnchor>& anchors);
    // Sanitises loaded state: a missing name comes from absolute (legacy); an invalid anchor pair is cleared;
    // '\' in relative becomes '/' (non-POSIX shapes); empty absolute => name/anchor/relative cleared (no candidates).
    static SampleRef restoreRef(std::string absolute, std::string name, std::string anchorKey, std::string relative);
    // Order: local anchor[ref.anchorKey] + relative, then absolute (if isAbsolute), then dir + name
    // for each searchDir. Deduplicated by samePath and truncated to kMaxCandidates.
    static std::vector<std::string> candidates(const SampleRef& ref, const std::vector<PathAnchor>& anchors,
                                               const std::vector<std::string>& searchDirs);
    static std::optional<std::size_t> resolve(const std::vector<std::string>& candidates,
                                              const std::function<bool(const std::string&)>& exists);
};
}
```

Normalisation (used for keys only): Windows-shaped paths turn `/` into `\`, collapse repeated separators after the UNC prefix, drop a trailing separator unless it is the root `C:\`, and lowercase ASCII. POSIX paths collapse repeated `/` and drop a trailing `/` unless it is the root.

**Plugin.** APVTS properties: `samplePath`, `sampleName`, `sampleAnchor`, `sampleRel`. `localAnchors()` lives in `PluginProcessor.cpp` and uses the order music, documents, desktop, home. Empty locations are skipped. Strings convert with `toStdString()` and `String::fromUTF8`.

- `SampleLoader::request(juce::StringArray candidates, juce::String original, juce::String displayName, Origin)` (`displayName` is the "Missing:" label name). The list is built before `lock_` is taken and moved into `pending_` under `lock_`, followed by `gate_.next()`. The lock order and `commitGated` stay unchanged. An empty list with Restore clears the source ("No sample").
- `load` uses `resolve` with `File::existsAsFile` (absolute paths only). If no candidate exists, it fails as today with `"Missing: <name>"`, where `<name>` is the stored `sampleName` if non-empty, else `fileName(samplePath)` (spec wins; the processor passes the name to the loader request). Decode errors do not fall through to the next candidate.
- `Result` gets `path` (the loaded path, or `original` on failure) and `bool relocated`. Status text: `"Loaded: <name>"`, then `" (relocated)"` if relocated, then the truncation suffix.
- `onLoadResult`: after a successful User load, it writes all four properties from `makeRef(r.path, localAnchors())`. When no anchor matches, it removes `sampleAnchor` and `sampleRel`.
- `loadFile` calls `request({path}, path, User)`.

## Data Flow

    setStateInformation -> restoreRef(props) -> candidates(ref, localAnchors, anchorDirs)
        -> loader_.request(list, absolute, Restore) --loader thread--> resolve(existsAsFile)
        -> decode/commitGated -> Result{path, relocated} -> onLoadResult -> status

## File Changes

| Commit | File | Action | Lines |
|---|---|---|---|
| 1 `feat(core): add SamplePathResolver` | `Source/core/include/grains/SamplePathResolver.h` | Create (CRLF, SPDX) | ~65 |
| 1 | `Source/core/src/SamplePathResolver.cpp` | Create | ~190 |
| 1 | `tests/test_sample_path_resolver.cpp` | Create | ~280 |
| 1 | `Source/core/CMakeLists.txt`, `tests/CMakeLists.txt` | Add sources | 2 |
| 2 `feat(plugin): restore samples from relocated paths` | `Source/plugin/SampleLoader.{h,cpp}` | Candidate request, Result fields | ~55 |
| 2 | `Source/plugin/PluginProcessor.{h,cpp}` | Properties, anchors, state | ~60 |
| 2 | `openspec/specs` deltas (sample-source, plugin-shell) | Spec text | ~80 |

Total is about 730 lines, under the 800-line budget. The cuts listed in the proposal still apply.

## Testing Strategy

Strict TDD. Write the RED Catch2 cases first, `[sample_path]`, in `tests/test_sample_path_resolver.cpp`:

1. Shape: drive with either separator, UNC, POSIX, `C:foo`, empty, relative.
2. `fileName`: mixed separators, trailing separator, a POSIX name containing `\`.
3. `makeRef`: longest anchor (`home` vs `music`), `C:\Music2` vs `C:\Music`, case-insensitive on Windows only, mixed `/`, a path equal to the anchor, `..` gives no anchor, a POSIX anchor with a Windows path gives no match.
4. `restoreRef`: legacy (only absolute), `..`, absolute or empty `relative`, a key without a relative.
5. `candidates`: order, anchor join in each style, unknown key, a non-absolute `absolute`, dedup on case and separators, cap at 10, empty everything.
6. `resolve`: the first true wins, all false gives `nullopt`, the predicate is called in order and stops early.

Plugin checks are manual: move the anchored folder's parent, save in an old-format state, the missing-file label, and the R-SS-7 loader checks.

## Threat Matrix

N/A: the change has no routing, shell, subprocess, VCS/PR automation, executable-file classification or process-integration boundary. Path traversal is handled by rejecting `..` (tested).

## Migration / Rollout

No migration is needed. The change is additive. Old builds ignore the new properties, and `samplePath` is still written.

## Open Questions

- [x] JUCE has no portable host project-directory API (see above).
- [x] Same-named files in search dirs are accepted with the "(relocated)" flag (proposal assumption 1).
- [ ] Non-ASCII case folding on Windows is not handled (ASCII only). This is accepted as low risk.
