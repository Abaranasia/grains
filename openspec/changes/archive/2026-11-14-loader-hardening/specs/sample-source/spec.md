# Sample Source Specification (delta)

## MODIFIED Requirements

### R-SS-5 Sample builder: length cap
The builder MUST cap length at 5 minutes measured at the file's sample rate (`floor(300 * rate)` frames), and the cap MUST be clamped to `INT_MAX` frames. A rate whose `floor(300 * rate)` is 0 (a positive rate below 1/300 Hz) yields a cap of 0 frames and MUST be rejected as `BadSampleRate`, without producing a source. A rate large enough that `300 * rate` overflows the frame-count type (including `DBL_MAX`) MUST yield a cap of exactly `INT_MAX`, with no undefined behaviour. Longer input MUST be truncated to the cap and the result MUST report that truncation occurred. Input at or below the cap MUST be kept in full with no truncation reported. The builder MUST NOT resample.

(Previously: no mention of the `INT_MAX` clamp, the floor-to-zero rejection, or overflow behaviour.)

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

### R-SS-7 Loader behaviour
The plugin MUST decode a requested file on a background thread, never on the audio thread or the message thread. Only the current request (the most recently issued, whether from the user or from a state restore) MAY publish a source or clear the source. A superseded request MUST NOT publish and MUST NOT clear, whether it finishes successfully or with failure. The check that a request is current and the commit of its result MUST be atomic with respect to the issuing of new requests: no new request MAY be issued between the check and the commit of the same result. On success of the current request the decoded data MUST pass through the sample builder and the result MUST be published to `SourceExchange`. On decode failure or builder rejection of a user load, the previously published source MUST stay active, an error status MUST be shown, and nothing MUST be published or cleared. An empty buffer MUST NEVER be published. Completion callbacks to the message thread MUST be guarded so they are safe after the processor is destroyed, and the loader thread MUST be stopped before the members it uses are destroyed. Stopping the loader MUST wait, without a finite timeout, for any in-flight read or decode to finish, and MUST NOT forcibly terminate the loader thread; the generation gate and exchange are therefore only destroyed after no loader code can still hold their locks. A stalled read (for example on an unresponsive network share) MAY delay shutdown until the read returns or fails; this is an accepted tradeoff.

(Previously: required the loader thread to be stopped before its members are destroyed, but did not forbid a finite stop timeout, so a stalled read could lead to the thread being killed by force while holding the gate mutex or heap lock.)

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

### R-SS-9 Missing file behaviour
If the restored path is empty, the output MUST be silent (no source is active) and the file label MUST read "No sample". If restoring a non-empty path fails (the file is missing, unreadable, or corrupt), the output MUST be silent (no source is active), the stored path MUST be kept unchanged in state, and the file label MUST read "Missing: <name>" where `<name>` is the file name (not the full path). In both cases the source MUST be cleared through an explicit clear operation, which is distinct from publishing a buffer; an empty buffer MUST NEVER be published to achieve the clear. If the allocation required to perform the clear fails, the previously published source MUST stay active and nothing MUST be cleared or published; the file label MUST then read "Error: No sample" for an empty restored path, or "Error: <name>" (file name, not the full path) for a non-empty restored path, and the stored path MUST be kept unchanged in state. A restore request that has been superseded MUST NOT clear the source. A later successful load MUST replace the path.

(Previously: did not define behaviour when the clear allocation fails. The error labels document the existing behaviour; no code change is required.)

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
