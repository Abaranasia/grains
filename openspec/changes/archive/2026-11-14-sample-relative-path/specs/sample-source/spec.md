# Sample Source Specification (delta)

## MODIFIED Requirements

### R-SS-8 Path persistence and async reload
The loaded file's reference MUST be stored as properties on the processor's `apvts_.state` and MUST be saved and restored through `getStateInformation` / `setStateInformation`. The reference MUST consist of: `samplePath` (the absolute path, always written, unchanged from the previous format), `sampleName` (the file name without directories), and, only when the file lies under an anchor (Music, Documents, Desktop, or Home; the longest matching anchor wins), `sampleAnchor` (the anchor identifier) and `sampleRel` (the path relative to that anchor). After state restore, the file MUST be reloaded asynchronously through the loader without blocking the calling thread, by trying the ordered candidate list defined in R-SS-12 and loading the first candidate that exists. State written by the previous format (only `samplePath`) MUST still restore. State written by this format MUST remain readable by code that knows only `samplePath`. Embedded audio, a last-used directory, rewriting state after a relocated restore, recursive search, and a content hash or size check are not provided.

#### Scenario: State round trip restores file [Manual]
- Given a loaded file and non-default parameters
- When state is saved and restored
- Then the reference properties are restored, the file reloads asynchronously, and sound resumes with the same parameters

#### Scenario: Anchored file writes all reference fields [Manual]
- Given a file loaded from under the user's Music folder
- When state is saved
- Then the state holds `samplePath`, `sampleName`, `sampleAnchor` (Music) and `sampleRel`

#### Scenario: File outside every anchor writes no anchor fields [Manual]
- Given a file loaded from a folder that is under no anchor
- When state is saved
- Then the state holds `samplePath` and `sampleName` and holds no `sampleAnchor` or `sampleRel`

#### Scenario: Old-format state restores [Manual]
- Given saved state that holds only `samplePath` of an existing file
- When state is restored
- Then the file loads, the label shows its name without "(relocated)", and sound resumes

## ADDED Requirements

### R-SS-12 Sample path resolver: reference and candidates
A JUCE-free core unit `grains::SamplePathResolver` MUST use `std::string` only (no JUCE, no `std::filesystem`) and MUST provide three operations.

`makeRef(absolute, anchors)` MUST produce a reference holding the absolute path, the file name, and, when the path lies under one of the supplied anchors, that anchor's identifier and the path relative to it. The longest matching anchor MUST win. Anchor matching MUST be on whole path components only (an anchor `C:\Users\a` MUST NOT match `C:\Users\ab\x.wav`). An anchor that equals the whole path (no file component) MUST NOT match. A path containing a `..` component MUST NOT get an anchor. The stored relative path MUST use `/` as separator, and restoring a stored relative path MUST normalise any `\` in it to `/`.

`candidates(ref, anchors, searchDirs)` MUST return an ordered, deduplicated list of paths: (1) the reference's anchor, resolved against this machine's `anchors`, joined with its relative path, when the reference has an anchor and this machine supplies that anchor; (2) the stored absolute path; (3) the file name joined to each search directory, in the given order. The list MUST NOT hold more than 10 entries; entries beyond the cap MUST be dropped from the end. The list MUST be empty when the reference has no usable path and no usable name.

`resolve(candidates, exists)` MUST call the injected predicate on candidates in order and return the first candidate for which it returns true, together with its index; it MUST return a not-found result when no candidate exists, and MUST NOT call the predicate on later candidates once one matches. No operation MAY touch the filesystem itself.

#### Scenario: makeRef finds an anchor [Catch2]
- Given anchors Music=`C:\Users\a\Music` and Home=`C:\Users\a`, and the path `C:\Users\a\Music\loops\kick.wav`
- When `makeRef` runs
- Then the anchor is Music, the relative path is `loops/kick.wav` (stored relative paths always use `/`), the name is `kick.wav`, and the absolute path is unchanged

#### Scenario: Longest anchor wins [Catch2]
- Given the same anchors and the path `C:\Users\a\Music\kick.wav` (which lies under both)
- When `makeRef` runs
- Then the anchor is Music and not Home

#### Scenario: Anchor matches whole components only [Catch2]
- Given the anchor `C:\Users\a` and the path `C:\Users\ab\kick.wav`
- When `makeRef` runs
- Then the reference has no anchor

#### Scenario: Path outside every anchor [Catch2]
- Given the path `D:\samples\kick.wav` and anchors that do not contain it
- When `makeRef` runs
- Then the reference has no anchor and no relative path, and its name is `kick.wav`

#### Scenario: Windows drive match ignores case [Catch2]
- Given the anchor `C:\Users\a\Music` and the path `c:\users\A\MUSIC\Kick.wav`
- When `makeRef` runs
- Then the anchor is Music and the relative path is `Kick.wav` (original case preserved)

#### Scenario: UNC path matches case-insensitively [Catch2]
- Given the anchor `\\Server\Share\Music` and the path `\\server\share\music\loops\kick.wav`
- When `makeRef` runs
- Then the anchor matches and the relative path is `loops/kick.wav`

#### Scenario: Drive paths with forward slashes match [Catch2]
- Given the anchor `C:\Users\a\Music` and the path `C:/Users/a/Music/loops/kick.wav`
- When `makeRef` runs
- Then the anchor matches and the relative path has the same components `loops` and `kick.wav`

#### Scenario: POSIX path matches case-sensitively [Catch2]
- Given the anchor `/home/a/Music` and the paths `/home/a/Music/kick.wav` and `/home/a/music/kick.wav`
- When `makeRef` runs on each
- Then the first path matches the anchor with relative path `kick.wav`, and the second has no anchor

#### Scenario: Backslash is a filename character on POSIX [Catch2]
- Given the POSIX path `/home/a/Music/we\ird.wav`
- When `makeRef` runs
- Then the name is `we\ird.wav` and the relative path is `we\ird.wav`

#### Scenario: Dot-dot path gets no anchor [Catch2]
- Given the anchor `C:\Users\a\Music` and the path `C:\Users\a\Music\..\Other\kick.wav`
- When `makeRef` runs
- Then the reference has no anchor and no relative path, and its name is `kick.wav`

#### Scenario: Name extraction across path shapes [Catch2]
- Given `C:\x\kick.wav`, `\\srv\sh\kick.wav`, `/x/kick.wav`, and `kick.wav`
- When names are extracted via `makeRef`
- Then each name is `kick.wav`

#### Scenario: Candidate order is anchor, absolute, then search directories [Catch2]
- Given a reference with anchor Music, relative path `loops/kick.wav`, and absolute path `C:\old\kick.wav`; this machine's Music is `D:\Music`; search directories `E:\a` and `E:\b`
- When `candidates` runs
- Then the list is `D:\Music\loops\kick.wav`, `C:\old\kick.wav`, `E:\a\kick.wav`, `E:\b\kick.wav`, in that order

#### Scenario: Missing anchor on this machine is skipped [Catch2]
- Given a reference with anchor Music and this machine's anchors lacking Music
- When `candidates` runs
- Then the list starts with the stored absolute path

#### Scenario: Duplicates are removed keeping first position [Catch2]
- Given a reference whose anchor-joined path equals its absolute path, and a search directory that contains that file's own directory
- When `candidates` runs
- Then each distinct path appears once, at its first position

#### Scenario: Dedup treats Windows paths case-insensitively [Catch2]
- Given candidates `C:\Music\Kick.wav` and `c:\music\kick.wav`
- When `candidates` deduplicates
- Then only the first remains

#### Scenario: Candidate list is capped [Catch2]
- Given a reference and 20 distinct search directories
- When `candidates` runs
- Then the list holds exactly 10 entries and the entries kept are the first 10 in order

#### Scenario: Search directory trailing separator [Catch2]
- Given the search directory `E:\a\` and `E:\a`
- When `candidates` runs with both
- Then exactly one `E:\a\kick.wav` entry appears

#### Scenario: Resolve returns the first existing candidate [Catch2]
- Given candidates A, B, C and a fake predicate that is true for B and C
- When `resolve` runs
- Then it returns B with index 1, and the predicate was not called on C

#### Scenario: Resolve reports not found [Catch2]
- Given candidates A and B and a predicate that is always false
- When `resolve` runs
- Then the result is not-found, and the predicate was called on A then B

#### Scenario: Resolve on an empty list [Catch2]
- Given an empty candidate list
- When `resolve` runs
- Then the result is not-found and the predicate is never called

#### Scenario: Empty and garbage input does not fail [Catch2]
- Given an empty string, a string of only separators, a string with embedded NUL or control characters, and a very long path
- When `makeRef`, `candidates`, and `resolve` run on each
- Then no call throws or reads out of bounds; an empty path yields an empty name, no anchor, and an empty candidate list

### R-SS-13 Legacy state compatibility
Restoring state that holds `samplePath` but neither `sampleName` nor the anchor fields MUST behave as a reference built from that absolute path alone: the name MUST be derived from `samplePath` (splitting on both `\` and `/` according to the path's shape), no anchor MUST be assumed, and the candidate list MUST be the absolute path followed by the name in each search directory. Restoring state with an empty or absent `samplePath` MUST behave per R-SS-9 (empty path), regardless of any other reference fields.

#### Scenario: Legacy reference derives the name [Catch2]
- Given a reference built from only `C:\old\kick.wav`
- When `candidates` runs with search directory `E:\a`
- Then the list is `C:\old\kick.wav` then `E:\a\kick.wav`

#### Scenario: Legacy state with a moved file [Manual]
- Given old-format state whose `samplePath` no longer exists but whose file name exists in a search directory
- When state is restored
- Then the file loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Stray fields with empty path [Manual]
- Given state with an empty `samplePath` and a non-empty `sampleName`
- When state is restored
- Then output is silent and the label reads "No sample"

### R-SS-14 Candidate loading and relocated status
The loader MUST accept an ordered candidate list for a request and MUST load the first candidate that exists, probing sequentially on the loader thread only (never the audio or message thread). The load result MUST report the path actually loaded. A user load MUST use a single-entry list. The generation gate, locking, atomic commit, and clear semantics of R-SS-7, R-SS-9, and R-SS-11 MUST be unchanged by the candidate list. When a Restore loads any candidate other than the stored absolute path (the anchor-joined path on this machine, when it differs from the stored path, or a search-directory path), the status label MUST read "Loaded: <name> (relocated)", where `<name>` is the file name; a Restore that loads the stored absolute path MUST NOT show "(relocated)". When no candidate exists or loads, behavior MUST be that of R-SS-9: the source is cleared, the stored reference is kept unchanged, and the label reads "Missing: <name>" (the stored `sampleName` or, for legacy state, the name derived from `samplePath`). A relocated Restore MUST NOT modify the stored reference. The "(relocated)" text and the truncation text of R-SS-10 MUST both appear when both apply.

#### Scenario: Relocated by anchor [Manual]
- Given a sample under the user's Music folder saved on one account, and the same relative layout under a different account's Music folder
- When state is restored on the second account
- Then the sample loads from the new location and the label reads "Loaded: <name> (relocated)"

#### Scenario: Moved parent folder reloads with hint [Manual]
- Given a project saved with a sample under Documents, then the sample moved to another folder inside a search directory
- When state is reloaded
- Then the sample loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Unmoved file shows no hint [Manual]
- Given state whose stored absolute path still exists
- When state is restored
- Then the label reads "Loaded: <name>" with no "(relocated)"

#### Scenario: Missing file label unchanged [Manual]
- Given state whose file exists at no candidate
- When state is restored
- Then output is silent, the label reads "Missing: <name>", and re-saving state keeps the same stored reference

#### Scenario: Relocated restore does not rewrite state [Manual]
- Given a relocated restore has loaded from a new path
- When state is saved again with no new user load
- Then the stored `samplePath`, `sampleName`, `sampleAnchor`, and `sampleRel` equal the values that were restored

#### Scenario: Superseded candidate load does not publish [Manual]
- Given a restore with several candidates in progress and the user requests another file
- When both requests finish
- Then only the user's file is active, and the restore neither published nor cleared the source

#### Scenario: Relocated and truncated together [Manual]
- Given a relocated restore of a file longer than 5 minutes
- When it loads
- Then the label shows the name with both "(relocated)" and "(truncated to 5 min)"

## Out of scope (explicit)
Waveform display, custom parameter UI, recent files, embedded audio in state, dropping `samplePath` from state, a content hash or file-size check, a persisted last-used directory, rewriting state after a relocated restore, a Downloads anchor, recursive directory search, load-time resampling, MIDI control, lookahead limiting and any user-adjustable output ceiling or drive.
