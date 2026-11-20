# Sample Resampling Specification (delta)

## MODIFIED Requirements

### R-RS-9 Load-time budget
A 300 s stereo 96000 to 48000 conversion (integer ratio) MUST complete in at most 3 s in a Release build on the reference machine. A 300 s stereo 96000 to 44100 conversion (non-integer ratio, two filter rows per output frame) MUST be measured in a Release build on the reference machine, the measured time MUST be recorded in the change artifacts, and it MUST complete in at most 6 s; if the measurement exceeds 6 s the resampler MUST be optimised until it does, without changing its output beyond the quality bounds of R-RS-3 and the determinism of R-RS-5. A 300 s stereo 384000 to 44100 conversion MAY be measured the same way as an informational case; it has no pass/fail budget and its measured time MUST be recorded if run. All of these checks are tagged `[perf]`, hidden from the default run, and MUST NOT be part of the CI gate.
(Previously: only the integer-ratio 96000 to 48000 budget of 3 s.)

#### Scenario: Budget integer ratio [Perf]
- Given 300 s of stereo noise at 96000
- When resampled to 48000 in Release
- Then wall time is at most 3 s

#### Scenario: Budget non-integer ratio [Perf]
- Given 300 s of stereo noise at 96000
- When resampled to 44100 in Release
- Then wall time is at most 6 s and the measured time is recorded

#### Scenario: Optimisation preserves quality [Catch2]
- Given the resampler after any optimisation made to meet the non-integer budget
- When the existing `[resampler]` cases for R-RS-3, R-RS-4 and R-RS-5 run
- Then all still pass unchanged

#### Scenario: High input rate informational [Perf]
- Given 300 s of stereo noise at 384000
- When resampled to 44100 in Release (optional case)
- Then the wall time is recorded; no limit is asserted

## ADDED Requirements

### R-RS-10 Out-of-memory result
When the output cannot be allocated, resampling MUST NOT throw and MUST NOT produce output, and MUST report a result distinguishable from rejection (R-RS-8) and abort (R-RS-6), namely `OutOfMemory`. The builder MUST map that result to its own out-of-memory status, and MUST report the same status when it fails to allocate its own buffers. Both paths MUST be covered by automated tests that do not install allocation hooks in `grains_tests`.

#### Scenario: Real allocation failure in the resampler [Catch2]
- Given a small input and a test-only allocation hook (`armFailure(n)`, linked only into `grains_alloc_failure_tests`) that makes the n-th allocation throw `std::bad_alloc`, for n = 1, 2, ... until a run is not hit by the failure
- When resampled
- Then every run hit by the failure returns `OutOfMemory`, produces no output, lets no exception escape and leaks nothing
- And the run the failure missed returns `Ok`
- And the hook is never linked into `grains_tests`
- Note: a huge-ratio request (1000 frames from 1 Hz to 1e12 Hz) was dropped because the ASan allocator aborts on an absurd request even for a throwing `operator new`. Like the other allocation sweeps, this case is compiled out under the MSVC debug runtime (`GRAINS_ALLOC_SWEEPS`), where the injected failure reaches `std::terminate`.

#### Scenario: Out-of-memory differs from rejection and abort [Catch2]
- Given the result of the scenario above
- When compared with the results of an invalid-argument call and an aborted call
- Then the three results are pairwise distinct

#### Scenario: Builder maps resampler out-of-memory [Catch2, separate executable]
- Given the allocation failure hook armed to fail the Nth allocation inside the resample step of the builder
- When the builder runs on valid input with a different target rate
- Then it reports its out-of-memory status, produces no source, and does not throw

#### Scenario: Builder buffer allocation failure [Catch2, separate executable]
- Given the allocation failure hook armed to fail the Nth allocation inside the builder's own buffer construction, for each N from 1 up to the number of allocations of a successful run
- When the builder runs on valid input
- Then every run either succeeds or reports out-of-memory, none throws, and none leaks (the allocation counter returns to its starting balance)

## Out of scope (delta)
Loader-thread allocation failures outside the resampler and builder (see sample-source R-SS-16).
