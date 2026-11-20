# Sample Source Specification

## Purpose

Defines how an audio file becomes the granular engine's source: a JUCE-free ownership hand-off (`SourceExchange`) between a producer thread and the audio thread, a JUCE-free sample builder that validates and normalizes decoded planar data, and the plugin-side loader, persistence, missing-file, and editor behaviour. Ownership and validation logic is JUCE-free and covered by Catch2; JUCE glue (decode, thread, chooser, drag-and-drop, state) is verified manually.

Test-layer legend: **[Catch2]** = automated, JUCE-free. **[Manual]** = manual verification (JUCE glue).

## Requirements

### R-SS-1 Publish and acquire (latest wins)
`SourceExchange` MUST let a producer thread publish an owned sample buffer and the audio thread acquire it. `acquire` MUST return the most recently published buffer not yet acquired, or a no-change result when none is pending. When several buffers are published before one `acquire`, only the latest MUST be delivered. Each buffer MUST be delivered to the consumer at most once.

#### Scenario: Acquire returns published buffer [Catch2]
- Given an exchange with no pending buffer
- When a producer publishes buffer A and the consumer acquires
- Then the consumer receives A, and a second acquire reports no change

#### Scenario: Publish twice, latest wins [Catch2]
- Given buffers A and B published in that order with no acquire between them
- When the consumer acquires
- Then it receives B and never receives A

### R-SS-2 Ownership and deallocation
Every published buffer MUST be freed exactly once. A buffer superseded before being acquired (unseen) MUST be freed by the producer side. A buffer retired by the consumer after a newer buffer is acquired MUST be handed back through a retire slot and freed off the audio thread (by the producer side or the destructor). The exchange destructor MUST free all buffers it still owns (pending, current, and retired). No buffer MAY leak or be double-freed.

#### Scenario: Unseen buffer freed by producer [Catch2]
- Given buffers A then B published with no acquire between them
- When the publish of B completes
- Then A has been freed and ownership counters show A freed exactly once

#### Scenario: Retired buffer freed off audio thread [Catch2]
- Given the consumer has acquired A and then acquires newer buffer B
- When the consumer returns from acquire
- Then A has not been freed on the consumer thread, and A is freed by the producer side or destructor exactly once

#### Scenario: Destructor frees everything [Catch2]
- Given an exchange holding pending, current, and retired buffers
- When the exchange is destroyed
- Then every buffer ever published has been freed exactly once

### R-SS-3 Real-time safe consumer
The consumer-side `acquire` MUST be wait-free and allocation-free. It MUST NOT lock, block, allocate, free, or throw. The audio thread MUST NOT free any sample buffer at any time. The audio path acquire, `GranularEngine::setSource`, and `processBlock` MUST be allocation-free.

#### Scenario: Allocation-free acquire path [Catch2]
- Given an allocation-counting hook and a pending buffer
- When the consumer runs acquire, `setSource`, and a render block
- Then the allocation counter is unchanged across the call

#### Scenario: Two-thread stress [Catch2]
- Given a producer repeatedly publishing and a consumer repeatedly acquiring on separate threads
- When the run completes and the exchange is destroyed
- Then no buffer is delivered twice, the last delivered buffer equals the last published, and all buffers are freed exactly once

### R-SS-4 Sample builder: channels
The sample builder MUST accept planar float data with an explicit sample rate and produce an engine source. One channel MUST yield a mono source; two channels MUST yield a stereo source. With more than two channels, only the first two MUST be kept and the rest discarded.

#### Scenario: Mono and stereo accepted [Catch2]
- Given planar input with 1 channel, then with 2 channels
- When the builder runs
- Then the results are a mono source and a stereo source with unchanged samples

#### Scenario: More than two channels [Catch2]
- Given planar input with 6 channels
- When the builder runs
- Then the result is stereo equal to channels 0 and 1 of the input

### R-SS-5 Sample builder: length cap
The builder MUST cap length at 5 minutes measured at the file's sample rate (`floor(300 * rate)` frames), and the cap MUST be clamped to `INT_MAX` frames. A rate whose `floor(300 * rate)` is 0 (a positive rate below 1/300 Hz) yields a cap of 0 frames and MUST be rejected as `BadSampleRate`, without producing a source. A rate large enough that `300 * rate` overflows the frame-count type (including `DBL_MAX`) MUST yield a cap of exactly `INT_MAX`, with no undefined behaviour. Longer input MUST be truncated to the cap and the result MUST report that truncation occurred. Input at or below the cap MUST be kept in full with no truncation reported. When a target rate is supplied, the builder MUST resample the capped input to that rate (see `sample-resampling`), the published source MUST report the target rate, and the resampled output MUST be capped at `floor(300 * target)` frames (clamped to `INT_MAX`), reporting truncation if the cap bites. When no target rate is supplied, or it equals the file rate, the builder MUST NOT alter the samples. A target rate whose downsampling ratio exceeds 1000:1 (file rate / target rate > 1000) MUST be rejected as `BadSampleRate` without producing a source (see `sample-resampling` R-RS-8).

#### Scenario: Over-length input truncated [Catch2]
- Given input longer than 300 seconds at its sample rate
- When the builder runs
- Then the result has exactly `floor(300 * rate)` frames and reports truncation

#### Scenario: Input at cap not truncated [Catch2]
- Given input of exactly `floor(300 * rate)` frames
- When the builder runs
- Then all frames are kept and no truncation is reported

#### Scenario: Cap is floored [Catch2]
- Given a sample rate for which `300 * rate` is not an integer (for example 44100.5 Hz)
- When `maxSourceFrames` is evaluated for that rate
- Then it returns `floor(300 * rate)`, not a rounded-up value

#### Scenario: Cap floors to zero is rejected [Catch2]
- Given a finite sample rate greater than 0 for which `floor(300 * rate)` is 0 (for example 0.001 Hz)
- When `maxSourceFrames` is evaluated, and the builder runs on otherwise valid data at that rate
- Then `maxSourceFrames` returns 0, and the builder reports `BadSampleRate` and produces no source

#### Scenario: Cap clamped to INT_MAX [Catch2]
- Given a finite sample rate for which `floor(300 * rate)` exceeds `INT_MAX` (for example 1e9 Hz)
- When `maxSourceFrames` is evaluated
- Then it returns exactly `INT_MAX`

#### Scenario: Extreme rate does not overflow [Catch2]
- Given a sample rate of `DBL_MAX`
- When `maxSourceFrames` is evaluated
- Then it returns exactly `INT_MAX` with no undefined behaviour (clean under UBSan float-cast-overflow)

#### Scenario: Resampled to target rate [Catch2]
- Given 1 s of stereo input at 96000 Hz and target rate 48000
- When the builder runs
- Then the source has 48000 frames, its rate is 48000, and no truncation is reported

#### Scenario: Equal rate is bit-exact [Catch2]
- Given input at 48000 Hz and target 48000
- When the builder runs
- Then the source samples are bit-identical to the input

#### Scenario: Output cap at target rate [Catch2]
- Given a file at 22050 Hz holding `floor(300 * 22050)` frames and target rate 192000
- When the builder runs
- Then the source has exactly `floor(300 * 192000)` frames and truncation is reported only if the resampled length exceeded that cap

#### Scenario: Invalid target rate rejected [Catch2]
- Given a target rate of 0, -1, NaN or Inf
- When the builder runs on otherwise valid data
- Then it is rejected with `BadSampleRate` and no source is produced

### R-SS-6 Sample builder: rejection
The builder MUST reject, without producing a source: zero channels, zero frames, a non-finite sample rate (NaN or Inf), a sample rate less than or equal to 0, and channels whose lengths differ. Rejection MUST be reported to the caller with a distinguishable failure result and MUST NOT throw.

#### Scenario: Empty input rejected [Catch2]
- Given input with zero channels, and input with zero frames
- When the builder runs on each
- Then both are rejected and no source is produced

#### Scenario: Bad rate rejected [Catch2]
- Given rates 0, -44100, NaN, and Inf
- When the builder runs on otherwise valid data for each
- Then every call is rejected

#### Scenario: Mismatched channel lengths rejected [Catch2]
- Given two channels with different frame counts
- When the builder runs
- Then it is rejected

### R-SS-7 Loader behaviour
The plugin MUST decode a requested file on a background thread, never on the audio thread or the message thread. Only the current request (the most recently issued, whether from the user or from a state restore) MAY publish a source or clear the source. A superseded request MUST NOT publish and MUST NOT clear, whether it finishes successfully or with failure. The check that a request is current and the commit of its result MUST be atomic with respect to the issuing of new requests: no new request MAY be issued between the check and the commit of the same result. On success of the current request the decoded data MUST pass through the sample builder and the result MUST be published to `SourceExchange`. On decode failure or builder rejection of a user load, the previously published source MUST stay active, an error status MUST be shown, and nothing MUST be published or cleared. An empty buffer MUST NEVER be published. Completion callbacks to the message thread MUST be guarded so they are safe after the processor is destroyed, and the loader thread MUST be stopped before the members it uses are destroyed. Stopping the loader MUST wait, without a finite timeout, for any in-flight read or decode to finish, and MUST NOT forcibly terminate the loader thread; the generation gate and exchange are therefore only destroyed after no loader code can still hold their locks. A stalled read (for example on an unresponsive network share) MAY delay shutdown until the read returns or fails; this is an accepted tradeoff.

#### Scenario: Load publishes on success [Manual]
- Given the Standalone running with a valid wav file
- When the file is loaded
- Then the engine starts rendering the file and the UI shows its name

#### Scenario: Latest request wins [Manual]
- Given two load requests issued in quick succession
- When both finish
- Then only the second file is active

#### Scenario: Decode failure keeps previous source [Manual]
- Given a source is playing and a corrupt or unsupported file is selected
- When decoding fails
- Then the previous source keeps playing, an error status is shown, and nothing new is published or cleared

#### Scenario: Superseded successful load does not publish [Manual]
- Given a source A is playing and a slow load of file B is in progress
- When a load of a corrupt file C is requested before B finishes and B then finishes decoding
- Then B is not published, C fails, and A keeps playing with an error status shown

#### Scenario: Destruction during load [Manual]
- Given a load in progress
- When the plugin or Standalone is closed
- Then there is no crash, hang, or callback into destroyed objects

#### Scenario: Stalled read at shutdown waits, never kills [Manual]
- Given a load of a file on a source that stalls longer than 4 seconds (for example a throttled or disconnected network share)
- When the plugin or Standalone is closed during the stall
- Then shutdown waits for the read to return or fail, no "killing thread by force" message is emitted, and once the read ends the process exits without crash or undefined behaviour

### R-SS-8 Path persistence and async reload
The loaded file's reference MUST be stored as properties on the processor's `apvts_.state` and MUST be saved and restored through `getStateInformation` / `setStateInformation`. The reference MUST consist of: `samplePath` (the absolute path, always written, unchanged from the previous format), `sampleName` (the file name without directories), and, only when the file lies under an anchor (Music, Documents, Desktop, or Home; the longest matching anchor wins), `sampleAnchor` (the anchor key: `music`, `documents`, `desktop`, or `home`) and `sampleRel` (the path relative to that anchor). After state restore, the file MUST be reloaded asynchronously through the loader without blocking the calling thread, by trying the ordered candidate list defined in R-SS-12 and loading the first candidate that exists. The plugin's search directories MUST be exactly this machine's four anchor directories (music, documents, desktop, home, in that order); the name search MUST look only directly inside each of them (non-recursive). State written by the previous format (only `samplePath`) MUST still restore. State written by this format MUST remain readable by code that knows only `samplePath`. Embedded audio, a last-used directory, rewriting state after a relocated restore, recursive search, and a content hash or size check are not provided.

#### Scenario: State round trip restores file [Manual]
- Given a loaded file and non-default parameters
- When state is saved and restored
- Then the reference properties are restored, the file reloads asynchronously, and sound resumes with the same parameters

#### Scenario: Anchored file writes all reference fields [Manual]
- Given a file loaded from under the user's Music folder
- When state is saved
- Then the state holds `samplePath`, `sampleName`, `sampleAnchor` (`music`) and `sampleRel`

#### Scenario: File outside every anchor writes no anchor fields [Manual]
- Given a file loaded from a folder that is under no anchor
- When state is saved
- Then the state holds `samplePath` and `sampleName` and holds no `sampleAnchor` or `sampleRel`

#### Scenario: Old-format state restores [Manual]
- Given saved state that holds only `samplePath` of an existing file
- When state is restored
- Then the file loads, the label shows its name without "(relocated)", and sound resumes

### R-SS-9 Missing file behaviour
If the restored path is empty, the output MUST be silent (no source is active) and the file label MUST read "No sample". If restoring a non-empty path fails (the file is missing, unreadable, or corrupt), the output MUST be silent (no source is active), the stored path MUST be kept unchanged in state, and the file label MUST read "Missing: <name>" where `<name>` is the file name (not the full path). In both cases the source MUST be cleared through an explicit clear operation, which is distinct from publishing a buffer; an empty buffer MUST NEVER be published to achieve the clear. If the allocation required to perform the clear fails, the previously published source MUST stay active and nothing MUST be cleared or published; the file label MUST then read "Error: No sample" for an empty restored path, or "Error: <name>" (file name, not the full path) for a non-empty restored path, and the stored path MUST be kept unchanged in state. A restore request that has been superseded MUST NOT clear the source. A later successful load MUST replace the path.

#### Scenario: Empty path on reload [Manual]
- Given saved state with an empty sample path
- When state is restored
- Then output is silent and the label reads "No sample"

#### Scenario: Missing file on reload [Manual]
- Given saved state whose file path no longer exists
- When state is restored
- Then output is silent, the label reads "Missing: <name>", and re-saving state keeps the same path

#### Scenario: Superseded restore does not clear [Manual]
- Given a restore of a missing file is in progress and the user then requests a valid file
- When both requests finish
- Then the valid file is active and the source was not cleared by the restore

#### Scenario: Failed clear allocation keeps previous source [Manual]
- Given a source A is playing and a state restore with an empty path or a missing file whose clear allocation fails (induced, for example, by a debug allocation-failure hook or a low-memory test run)
- When the restore request completes
- Then A keeps playing, nothing is cleared or published, the label reads "Error: No sample" (empty path) or "Error: <name>" (missing file), and re-saving state keeps the same stored path

### R-SS-10 Editor load UI
The editor MUST provide a Load button that opens an asynchronous `FileChooser` filtered to wav, aiff, and aif, and MUST accept drag-and-drop of such files onto the editor. Both routes MUST issue a load request to the loader. Files with other extensions dropped on the editor MUST be ignored. The editor MUST show a label with the current file name, a status (loading, error, or missing), and MUST NOT block the message thread while loading. When a loaded file was truncated by the length cap (R-SS-5), the status label MUST include the text "(truncated to 5 min)" after the file name; when it was not truncated, the label MUST NOT include that text. Truncation is reported by status text only; no other warning UI is required.

#### Scenario: Load via button [Manual]
- Given the Standalone editor open
- When the user presses Load and selects a wav file
- Then the file loads and the label shows its name

#### Scenario: Load via drag-and-drop [Manual]
- Given the Standalone editor open
- When the user drops an aiff file on the editor
- Then the file loads and the label shows its name

#### Scenario: Unsupported file dropped [Manual]
- Given the Standalone editor open
- When the user drops a `.txt` file
- Then nothing is loaded and the active source is unchanged

#### Scenario: Replace while playing [Manual]
- Given a source is playing
- When another file is loaded
- Then the new source replaces the old with no crash

#### Scenario: Truncated file shows status [Manual]
- Given a wav file longer than 5 minutes
- When it is loaded
- Then the label shows its name followed by "(truncated to 5 min)" and the source plays

#### Scenario: Short file shows no truncation text [Manual]
- Given a wav file shorter than 5 minutes
- When it is loaded
- Then the label shows its name without "(truncated to 5 min)"

### R-SS-11 Generation gate and staged exchange
A JUCE-free core primitive (the generation gate) MUST own a request generation counter and a mutex. It MUST provide `next()`, which advances the generation and returns the new value, and a commit-if-current operation that, given a generation, runs a caller-supplied commit action only when that generation equals the current one, while holding the mutex. `next()` and the commit action MUST be mutually exclusive, so a generation cannot advance between the currency check and the commit action. The commit-if-current operation MUST report whether the commit ran. The commit action MUST be limited to cheap, non-allocating, non-freeing work (a pointer exchange).

`SourceExchange` MUST support staging a buffer (allocation of its owned storage) outside the gate, committing the staged buffer inside the gate by pointer exchange, and an explicit clear operation. A commit that is rejected because the generation is superseded MUST leave the exchange state unchanged and MUST return ownership of the staged buffer to the caller, who frees it; no buffer MAY leak or be double-freed. A pending buffer superseded by a successful commit MUST be returned to the caller instead of being freed inside the gate. Neither staging, committing, clearing, nor freeing MAY occur on the audio thread, and the consumer-side contract of R-SS-1 to R-SS-3 MUST be unchanged.

#### Scenario: Current generation commits [Catch2]
- Given a gate whose generation was obtained from `next()` and no later `next()` call
- When commit-if-current runs with that generation
- Then the commit action runs exactly once and the call reports that it committed

#### Scenario: Stale generation does not commit [Catch2]
- Given a generation G obtained from `next()` and a later `next()` call
- When commit-if-current runs with G
- Then the commit action does not run and the call reports that it did not commit

#### Scenario: Rejected commit leaves exchange unchanged [Catch2]
- Given an exchange with a current buffer A and a staged buffer B committed with a stale generation
- When the commit is rejected
- Then the consumer still sees A (or no change), B is returned to the caller, and B is freed exactly once by the caller

#### Scenario: Superseded pending buffer returned, not freed in gate [Catch2]
- Given a pending buffer A not yet acquired and a staged buffer B committed with the current generation
- When the commit completes
- Then the consumer acquires B, A is returned to the caller after the gate is released, and A is freed exactly once by the caller and not inside the commit action

#### Scenario: Superseded clear is rejected [Catch2]
- Given a gate generation G superseded by a later `next()` and an exchange holding source A
- When a clear gated by G is attempted
- Then the clear does not run and the consumer still sees A

#### Scenario: Current clear leaves no source without publishing [Catch2]
- Given an exchange with an acquired source A
- When a clear gated by the current generation runs and the consumer acquires
- Then the consumer observes no active source, no empty buffer was published, and all buffers are freed exactly once after destruction

#### Scenario: No new generation between check and commit [Catch2]
- Given one thread repeatedly calling `next()` and another running commit-if-current whose action records the current generation
- When the run completes
- Then in every committed action the recorded generation equals the generation passed to commit-if-current

#### Scenario: Staged buffer allocation-free on consumer thread [Catch2]
- Given an allocation-counting hook and a committed pending buffer
- When the consumer runs acquire
- Then the allocation counter is unchanged across the call and no buffer is freed on the consumer thread

### R-SS-12 Sample path resolver: reference and candidates
A JUCE-free core unit `grains::SamplePathResolver` MUST use `std::string` only (no JUCE, no `std::filesystem`) and MUST provide three operations.

`makeRef(absolute, anchors)` MUST produce a reference holding the absolute path, the file name, and, when the path lies under one of the supplied anchors, that anchor's identifier and the path relative to it. The longest matching anchor MUST win. Anchor matching MUST be on whole path components only (an anchor `C:\Users\a` MUST NOT match `C:\Users\ab\x.wav`). An anchor that equals the whole path (no file component) MUST NOT match. A path containing a `..` component MUST NOT get an anchor. The stored relative path MUST use `/` as separator. Restoring a stored relative path MUST normalise any `\` in it to `/` only when the stored absolute path is not POSIX-shaped; when it is POSIX-shaped, `\` is a file-name character and MUST be kept.

`candidates(ref, anchors, searchDirs)` MUST return an ordered, deduplicated list of paths: (1) the reference's anchor, resolved against this machine's `anchors`, joined with its relative path, when the reference has an anchor and this machine supplies that anchor; (2) the stored absolute path; (3) the file name joined to each search directory, in the given order. The list MUST NOT hold more than 10 entries; entries beyond the cap MUST be dropped from the end. The list MUST be empty when the stored absolute path is empty (see R-SS-13), regardless of the other reference fields.

Stored values MUST be validated before use. A stored relative path that is empty, absolute, or contains an empty, `.` or `..` component MUST be ignored, and no anchor candidate MUST be produced from it. A stored name that is empty, `.`, `..`, or contains a separator (`/`, or `\` when the stored absolute path is not POSIX-shaped) MUST be ignored in favour of the name derived from the absolute path. When joining onto a Windows-shaped directory (an anchor or a search directory), a relative path or name containing `\` or `:` MUST be skipped, so that no candidate escapes that directory.

`resolve(candidates, exists)` MUST call the injected predicate on candidates in order and return the first candidate for which it returns true, together with its index; it MUST return a not-found result when no candidate exists, and MUST NOT call the predicate on later candidates once one matches. No operation MAY touch the filesystem itself.

#### Scenario: makeRef finds an anchor [Catch2]
- Given anchors `music`=`C:\Users\a\Music` and `home`=`C:\Users\a`, and the path `C:\Users\a\Music\loops\kick.wav`
- When `makeRef` runs
- Then the anchor is `music`, the relative path is `loops/kick.wav` (stored relative paths always use `/`), the name is `kick.wav`, and the absolute path is unchanged

#### Scenario: Longest anchor wins [Catch2]
- Given the same anchors and the path `C:\Users\a\Music\kick.wav` (which lies under both)
- When `makeRef` runs
- Then the anchor is `music` and not `home`

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
- Then the anchor is `music` and the relative path is `Kick.wav` (original case preserved)

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
- Given a reference with anchor `music`, relative path `loops/kick.wav`, and absolute path `C:\old\kick.wav`; this machine's `music` anchor is `D:\Music`; search directories `E:\a` and `E:\b`
- When `candidates` runs
- Then the list is `D:\Music\loops\kick.wav`, `C:\old\kick.wav`, `E:\a\kick.wav`, `E:\b\kick.wav`, in that order

#### Scenario: Missing anchor on this machine is skipped [Catch2]
- Given a reference with anchor `music` and this machine's anchors lacking `music`
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

#### Scenario: Unsafe stored name or relative path is ignored [Catch2]
- Given a reference with anchor `music`, absolute path `C:\old\kick.wav`, and either relative path `../../kick.wav` with name `..`, or name `x\kick.wav`; this machine's `music` anchor is `D:\Music`; search directory `E:\a`
- When `candidates` runs
- Then no anchor candidate is produced and the list is `C:\old\kick.wav` then `E:\a\kick.wav` (the name derived from the absolute path is used)

#### Scenario: Backslash or colon never escapes a Windows directory [Catch2]
- Given a POSIX-shaped reference `/old/x.wav` whose relative path is `a\..\..\x` under anchor `music`=`D:\Music`, a POSIX name `..\..\x.wav` with search directories `E:\a` and `/m`, and a name `a:b.wav` with search directories `E:\a` and `/m`
- When `candidates` runs on each
- Then no candidate is joined onto `D:\Music` or `E:\a`, while the POSIX directory `/m` still receives the name unchanged

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
The loader MUST accept an ordered candidate list for a request and MUST load the first candidate that exists, probing sequentially on the loader thread only (never the audio or message thread). The load result MUST report the path actually loaded. A user load MUST use a single-entry list. The generation gate, locking, atomic commit, and clear semantics of R-SS-7, R-SS-9, and R-SS-11 MUST be unchanged by the candidate list. When a Restore loads any candidate other than the stored absolute path (the anchor-joined path on this machine, when it differs from the stored path, or a search-directory path), the status label MUST read "Loaded: <name> (relocated)", where `<name>` is the file name; a Restore that loads the stored absolute path MUST NOT show "(relocated)". The label MUST read "No sample" only when the stored `samplePath` is empty. Otherwise, when the candidate list is empty or no candidate exists or loads, behavior MUST be that of R-SS-9: the source is cleared, the stored reference is kept unchanged, and the label reads "Missing: <name>", where `<name>` is the stored `sampleName`, else the name derived from `samplePath`, else the stored `samplePath` itself. The search directories (R-SS-8) are the four anchor directories, searched non-recursively. A relocated Restore MUST NOT modify the stored reference. The "(relocated)" text and the truncation text of R-SS-10 MUST both appear when both apply.

#### Scenario: Relocated by anchor [Manual]
- Given a sample under the user's Music folder saved on one account, and the same relative layout under a different account's Music folder
- When state is restored on the second account
- Then the sample loads from the new location and the label reads "Loaded: <name> (relocated)"

#### Scenario: Moved parent folder reloads with hint [Manual]
- Given a project saved with a sample in a subfolder of Documents, then the sample file moved directly into another anchor folder (for example directly into Music)
- When state is reloaded
- Then the sample loads and the label reads "Loaded: <name> (relocated)"

#### Scenario: Nested subfolder is not searched [Manual]
- Given a project saved with a sample in a subfolder of Documents, then the sample file moved into a nested subfolder of another anchor folder (for example `Music/new/`)
- When state is reloaded
- Then the name search does not find it, output is silent, and the label reads "Missing: <name>"

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

### R-SS-15 Loader target rate and rate-change reload
The loader MUST accept a target rate for each request, captured at request time, and the published `SampleBuffer::sampleRate()` MUST equal that target rate. Resampling MUST run on the loader thread only, MUST check request currency (the generation gate) between chunks, and a superseded request MUST abort without publishing or clearing. A request issued before any target rate is known (a restore before the first `prepareToPlay`) MUST publish the file at its own rate (passthrough). The decode buffer MUST be released as soon as resampling ends. When a reload is requested for a rate change the loader MUST re-decode the file from disk; if the file can no longer be read, the currently published buffer MUST stay active, nothing MUST be published or cleared, and the label MUST NOT read "Missing". The status label rules of R-SS-10 and R-SS-14 are unchanged.

#### Scenario: Published rate equals target [Manual]
- Given a 96 kHz wav and a host at 44.1 kHz
- When the file is loaded
- Then the published source reports 44100 Hz and the file plays at its original pitch and duration

#### Scenario: Restore before prepare publishes at file rate [Manual]
- Given state with a 96 kHz sample restored before the first `prepareToPlay`
- When the restore completes
- Then the source is published at 96000 Hz, plays, and the first `prepareToPlay` at 48000 Hz triggers one reload to 48000 Hz

#### Scenario: Restore before prepare, rates equal [Manual]
- Given a 48 kHz sample restored before the first `prepareToPlay` at 48000 Hz
- When `prepareToPlay` runs
- Then no reload is requested

#### Scenario: Superseded resample aborts [Manual]
- Given a long file being resampled and a newer load request issued
- When the older request next checks currency
- Then it aborts, publishes nothing, clears nothing, and the newer file becomes active

#### Scenario: File gone on rate-change reload [Manual]
- Given a published buffer at 44100 Hz whose file has been deleted
- When the host rate changes to 48000 Hz
- Then the 44100 Hz buffer keeps playing, nothing is cleared, and the label does not read "Missing"

#### Scenario: Peak memory released [Manual]
- Given a 5 minute stereo 192 kHz file loaded at a 48 kHz target
- When the load finishes
- Then the decode buffer has been freed and only the 48 kHz source remains

## Out of scope (explicit)
Waveform display, custom parameter UI, recent files, embedded audio in state, dropping `samplePath` from state, a content hash or file-size check, a persisted last-used directory, rewriting state after a relocated restore, a Downloads anchor, recursive directory search, runtime (pitch-shift) aliasing fixes, keeping the original decode in RAM, user-adjustable resampler quality, MIDI control, lookahead limiting and any user-adjustable output ceiling or drive.
