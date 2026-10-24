# Sample Source Specification

## Purpose

Defines how an audio file becomes the granular engine's source: a JUCE-free ownership hand-off (`SourceExchange`) between a producer thread and the audio thread, a JUCE-free sample builder that validates and normalizes decoded planar data, and the plugin-side loader, persistence, missing-file, and editor behaviour. Ownership and validation logic is JUCE-free and covered by Catch2; JUCE glue (decode, thread, chooser, drag-and-drop, state) is verified manually.

Test-layer legend: **[Catch2]** = automated, JUCE-free. **[Manual]** = manual verification in verify phase (JUCE glue).

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
The builder MUST cap length at 5 minutes measured at the file's sample rate (`floor(300 * rate)` frames). Longer input MUST be truncated to the cap and the result MUST report that truncation occurred. Input at or below the cap MUST be kept in full with no truncation reported. The builder MUST NOT resample.

#### Scenario: Over-length input truncated [Catch2]
- Given input longer than 300 seconds at its sample rate
- When the builder runs
- Then the result has exactly `floor(300 * rate)` frames and reports truncation

#### Scenario: Input at cap not truncated [Catch2]
- Given input of exactly `floor(300 * rate)` frames
- When the builder runs
- Then all frames are kept and no truncation is reported

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
The plugin MUST decode a requested file on a background thread, never on the audio thread or the message thread. When several requests arrive, the latest request MUST win and earlier unfinished requests MUST NOT publish. On success the decoded data MUST pass through the sample builder and the result MUST be published to `SourceExchange`. On decode failure or builder rejection, the previously published source MUST stay active, an error status MUST be shown, and nothing MUST be published. An empty buffer MUST NEVER be published. Completion callbacks to the message thread MUST be guarded so they are safe after the processor is destroyed, and the loader thread MUST be stopped before the members it uses are destroyed.

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
- Then the previous source keeps playing, an error status is shown, and nothing new is published

#### Scenario: Destruction during load [Manual]
- Given a load in progress
- When the plugin or Standalone is closed
- Then there is no crash, hang, or callback into destroyed objects

### R-SS-8 Path persistence and async reload
The absolute path of the loaded file MUST be stored as a property on the processor's `apvts_.state` and MUST be saved and restored through `getStateInformation` / `setStateInformation`. After state restore, the file MUST be reloaded asynchronously through the loader without blocking the calling thread. Absolute paths are used; relative-path lookup and embedded audio are not provided.

#### Scenario: State round trip restores file [Manual]
- Given a loaded file and non-default parameters
- When state is saved and restored
- Then the path property is restored, the file reloads asynchronously, and sound resumes with the same parameters

### R-SS-9 Missing file behaviour
If restoring the path fails for any reason (the file is missing, unreadable, or corrupt), the output MUST be silent (no source is active; the exchange is cleared), the stored path MUST be kept unchanged in state, and the file label MUST read "Missing: <name>" where `<name>` is the file name (not the full path). A later successful load MUST replace the path.

#### Scenario: Missing file on reload [Manual]
- Given saved state whose file path no longer exists
- When state is restored
- Then output is silent, the label reads "Missing: <name>", and re-saving state keeps the same path

### R-SS-10 Editor load UI
The editor MUST provide a Load button that opens an asynchronous `FileChooser` filtered to wav, aiff, and aif, and MUST accept drag-and-drop of such files onto the editor. Both routes MUST issue a load request to the loader. Files with other extensions dropped on the editor MUST be ignored. The editor MUST show a label with the current file name, a status (loading, error, or missing), and MUST NOT block the message thread while loading.

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

## Out of scope (explicit)
Waveform display, custom parameter UI, recent files, relative-path lookup, embedded audio in state, load-time resampling, MIDI control, soft limiter.
