# Sample Source Specification (delta)

Terms: a *request* is one load order with a monotonically increasing generation; kinds are `User`, `Restore` and `Reload`. *Latest* is the highest generation issued. *Target* is the most recently supplied host rate. *Published* is the rate of the currently active buffer, or none after a clear. *Settled* means a request finished as published, failed, or aborted (superseded). *Planner* is the host-free decision logic (see R-SS-16); all planner scenarios are **[Catch2]** under tag `[loadplan]`.

## MODIFIED Requirements

### R-SS-15 Loader target rate and rate-change reload
The loader MUST accept a target rate for each request, captured at request time, and the published `SampleBuffer::sampleRate()` MUST equal that target rate. Resampling MUST run on the loader thread only, MUST check request currency (the generation gate) between chunks, and a superseded request MUST abort without publishing or clearing. A request issued before any target rate is known (a restore before the first `prepareToPlay`) MUST publish the file at its own rate (passthrough). The decode buffer MUST be released as soon as resampling ends. When a reload is requested for a rate change the loader MUST re-decode the file from disk; if the file can no longer be read, the currently published buffer MUST stay active, nothing MUST be published or cleared, and the label MUST NOT read "Missing". The status label rules of R-SS-10 and R-SS-14 are unchanged.

Stale-rate recovery (R4-001): when a request of kind `User` or `Restore` that is still the latest settles as failed, and the published rate is known and differs from the current target, the loader MUST issue exactly one `Reload` for the current target. The loader MUST NOT issue a `Reload` after a failed `Reload` (a deleted file MUST NOT cause a reload loop). A failed `Restore` clears the published buffer (published rate becomes none), so no reissue occurs. A request that settles as aborted (superseded), or that is no longer the latest, MUST NOT trigger a reissue. A request that settles as published MUST NOT trigger a reissue. Retargeting to the rate already equal to the target MUST remain a no-op. The reissued `Reload` supersedes the failed request, so the failed request's own status post is dropped as stale; the `Reload` MUST therefore carry the failed request's status line (for example "Error: <name>") and post it whether it succeeds or fails, so the failure status MUST remain visible and MUST NOT be replaced by the previous file's "Loaded: ..." line. The published source's own label MUST NOT change.
(Previously: no recovery; a failed `User` request that superseded an in-flight `Reload` left a stale published rate until the next rate change.)

#### Scenario: Failed user load keeps its error status across the reissued Reload [Catch2 + Manual]
- Given a published 44.1 kHz source, target 48 kHz, and a User request that settles as failed with status "Error: u.wav"
- When the planner reissues the Reload
- Then the Reload's label is "Error: u.wav" (Catch2); in the plugin the status line keeps reading "Error: u.wav" after the silent Reload finishes (Manual, D.6)

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

#### Scenario: Stale rate after failed superseding User request is recovered [Catch2]
- Given published rate 44100, target retargeted to 48000 (a `Reload` R is issued), then a `User` request U is issued at generation above R and R is superseded
- When U settles as failed while still latest
- Then exactly one new `Reload` for target 48000 is issued, at a generation above U

#### Scenario: Same stale case after failed Restore does not reissue [Catch2]
- Given a `Restore` request that is still latest and whose settle outcome is failed, which clears the published rate
- When it settles
- Then no `Reload` is issued (published rate is none)

#### Scenario: No reissue after a failed Reload [Catch2]
- Given published rate 44100, target 48000, and a `Reload` that is still latest
- When that `Reload` settles as failed
- Then no further request is issued, and repeating the failure notification issues none either

#### Scenario: No reload loop on a deleted file [Catch2]
- Given the stale-recovery `Reload` issued by the first scenario of this group
- When that `Reload` settles as failed
- Then no request is issued (at most one recovery `Reload` per failure)

#### Scenario: Failed request that is no longer latest does not reissue [Catch2]
- Given `User` request U at generation g and a newer request at generation g+1
- When U settles as failed
- Then no `Reload` is issued on behalf of U

#### Scenario: Failed request with matching published rate does not reissue [Catch2]
- Given published rate 48000, target 48000, and a latest `User` request
- When it settles as failed
- Then no `Reload` is issued

#### Scenario: Aborted and published outcomes do not reissue [Catch2]
- Given a latest `User` request whose published rate differs from target
- When it settles as aborted, and separately as published
- Then no `Reload` is issued in either case

## ADDED Requirements

### R-SS-16 Host-free load planner
The decisions of R-SS-15 MUST be implemented by a planner in the core library that depends on no JUCE header and no JUCE type, and MUST be testable from `grains_tests` alone. The planner MUST own the target rate, latest generation, settled generation, published rate and pending request, and MUST provide: issuing a request (capturing the current target; no target known means passthrough), retargeting (issuing a `Reload` only when a rate is published, the new target is a valid rate, and the published rate differs from the new target), taking the pending request, and recording a settle outcome (published, failed, aborted) together with the resulting published state. The planner MUST be safe to call from the message thread and the loader thread concurrently. The loader MUST keep the thread and all file I/O, and MUST preserve the existing lock order (loader lock before the gate mutex) with no planner callback acquiring the loader lock. The observable loader behaviour for all scenarios of R-SS-7, R-SS-8, R-SS-9, R-SS-11 and R-SS-15 MUST be unchanged.

#### Scenario: Planner builds without JUCE [Catch2]
- Given the core library and `grains_tests` configured with `GRAINS_BUILD_PLUGIN` `OFF`
- When the planner tests are built and run
- Then they compile and pass with no JUCE target, header or include path

#### Scenario: Request captures the target rate [Catch2]
- Given target 44100 set on the planner
- When a request is issued
- Then the pending request carries target 44100 and a generation above all previous ones

#### Scenario: Request before any target is passthrough [Catch2]
- Given no target has been set
- When a `Restore` request is issued
- Then the pending request carries no target (passthrough)

#### Scenario: Retarget with nothing published issues nothing [Catch2]
- Given no published buffer
- When the target changes to 48000
- Then no request is issued and the target is 48000

#### Scenario: Retarget to a different rate issues one Reload [Catch2]
- Given published rate 96000 and target 96000
- When the target changes to 48000
- Then exactly one `Reload` with target 48000 is pending

#### Scenario: Retarget to the same rate is a no-op [Catch2]
- Given published rate 48000
- When the target is set to 48000, twice
- Then no request is issued

#### Scenario: Retarget with invalid rate is rejected [Catch2]
- Given published rate 48000
- When the target is set to 0, -1, NaN or Inf
- Then no request is issued and the previous target is retained

#### Scenario: Latest wins, taking pending [Catch2]
- Given two requests issued before the loader takes work
- When the pending request is taken
- Then it is the newer request and no older request remains pending

#### Scenario: Concurrent use is race-free [Catch2]
- Given one thread issuing requests and retargeting while another takes and settles
- When run for a bounded number of iterations (and under the sanitizer job)
- Then no data race, deadlock or invariant violation occurs: settled never exceeds latest, and pending is never older than latest

#### Scenario: Loader behaviour parity [Manual]
- Given the refactored loader in the standalone build
- When the manual checks of R-SS-15 (published rate, restore before prepare, superseded abort, file gone on reload) are repeated
- Then each behaves exactly as before the refactor

## Out of scope (delta)
Loader paths that allocate on the loader thread (`setSize` allocation failure, null staged `Box`) remain manually verified; only the planner seam is automated. Manual host checks D.1-D.5 are unchanged.
