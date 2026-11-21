# Verify Report: load-time-resampling

Mode: Strict TDD (hybrid store). Verdict: **PASS WITH WARNINGS** (0 CRITICAL, 5 WARNING, 3 SUGGESTION).
Task 4.6 (manual host checks) is pending for the user; R-SS-15 and R-PS-8 are reported as untested-by-automation, not failures.

## Completeness
Tasks 20/21 checked. The only unchecked task is 4.6 (manual, host-dependent, explicitly assigned to the user). Not counted as CRITICAL by agreement with the orchestrator.

## Execution evidence
| Command | Exit | Result | Output hash (sha256) |
|---|---|---|---|
| `cmake --build build --config Debug` | 0 | up to date, 0 warnings | 07a5b332e776d1f2d4b98a52526e2d8b3aa522d1d2c4432485e3093d2fd67f80 |
| `ctest --test-dir build -C Debug --output-on-failure` | 0 | 193/193 passed | 21e31637335986687c6dc3f3bb05100a0ef2ad5a8c40174cb19db16ed1e0e37e |
| `cmake --build build --config Release` | 0 | 0 warnings (/W4 /WX) | cb4ad3a87fcf92244e1d3a0bb1d07c1e45098f2630482a680c914055861e6643 |
| `ctest --test-dir build -C Release --output-on-failure` | 0 | 193/193 passed | 8abe20fce12606a9ad956b3c9ebe3a31d095ed852f61fa1e681dd50b81e95f39 |
| `grains_tests.exe "resample: 300 s stereo 96k to 48k within 3 s"` (Release) | 0 | 2.4663 s, pass | n/a |

Note: re-running `cmake -S . -B build -A x64` reported a generator-platform mismatch against the existing cache (exit 1, environmental). The existing configured tree was used for build and test; both succeeded.
Plugin target (JUCE) was not built by me; apply-progress claims a Debug Standalone compile. Not independently re-verified.

## Apply claims check
| Claim | Result |
|---|---|
| 193/193 Debug and Release | Confirmed |
| 20 new cases | Confirmed: 13 resampler visible (`--list-tests "[resampler]"` = 13) + 5 builder + 2 engine = 20; 173 + 20 = 193 (hidden perf test excluded from ctest) |
| Perf 2.41 s | Reproduced: 2.47 s (limit 3 s, thin margin) |
| Baseline 173 (not 78) | Consistent with the arithmetic above |

## TDD Compliance
| Check | Result | Details |
|---|---|---|
| TDD evidence table reported | OK | Present in apply-progress |
| All tasks have tests | WARNING | 4.1-4.5 have none (JUCE code not CI-built; compile-checked only) |
| RED confirmed | OK | test files exist; 2.6, 2.7, 3.3 disclosed as GREEN-on-arrival characterization tests |
| GREEN confirmed | OK | all listed files pass on my run |
| Triangulation | OK | multiple cases per behavior (rate pairs, tones, chunk sizes, 14 invalid-arg rows) |
| Safety net | OK | modified files report baseline runs |

## Assertion quality
Scanned test_resampler.cpp, test_sample_builder.cpp additions, test_grain_render.cpp additions. No tautologies. Loops iterate over fixed non-empty tables (`cases`, `pairs`, `tones`). Controls are present (non-identical stereo, mis-tagged rate, passband control, `REQUIRE(peak > ...)` guards). Assertion quality: 0 CRITICAL, 0 WARNING.

## Spec compliance matrix
| Req | Evidence | Status |
|---|---|---|
| R-RS-1 length | `resample: output length...` | COMPLIANT |
| R-RS-2 passthrough | `equal rates copy bytes exactly` | COMPLIANT |
| R-RS-3 passband/stopband/images | `passband is flat`, `impulse lands`, `out-of-band tones`, `upsampling leaves no image` | COMPLIANT |
| R-RS-4 finite | `finite input gives finite output at extreme ratios` | COMPLIANT |
| R-RS-5 determinism | `deterministic and independent of chunk size` | COMPLIANT |
| R-RS-6 abort / never-abort | `abort is honoured` (polls==3, polls==5) | COMPLIANT |
| R-RS-7 channels | `identical stereo channels stay bit-identical` | COMPLIANT |
| R-RS-8 rejection, no JUCE | `invalid arguments rejected`; core CMake `grains_assert_juce_free`, no juce include in Source/core | COMPLIANT (but see W1) |
| R-RS-9 perf [Perf] | hidden perf test, 2.47 s Release | COMPLIANT (manual tag, not CI gated, as specified) |
| R-SS-5 (modified) | builder tests: resample to 48000, equal-rate bit-exact, caps, invalid targets (0,-1,NaN,Inf) | COMPLIANT |
| R-SS-15 | no automated coverage; code inspection shows target capture, retarget, Reload, abort-silent, decode freed before stage | UNTESTED-BY-AUTOMATION (manual 4.6 pending) |
| R-GR-7 (modified) | `mismatched source rate keeps pitch and duration`, `matched source rate leaves render bit-identical` | COMPLIANT (see W3) |
| R-PS-8 | no automated coverage; code inspection: `retarget(sampleRate)` after `engine_.prepare`, same-rate no-op, `processBlock` untouched | UNTESTED-BY-AUTOMATION (manual 4.6 pending) |

## Design deviations
| # | Deviation | Assessment |
|---|---|---|
| 1 | `label` on `Request` instead of reading `published_->label` | Acceptable. Failure path reposts `r.label` and never "Missing" (SampleLoader.cpp fail lambda). Equivalent behavior. |
| 2 | `restoreGeneration_` in PluginProcessor | Acceptable and justified; not in spec (W4). |
| 3 | 1000:1 ratio limit, `InvalidArgument` / `BadSampleRate` | Tested (`ratio beyond the kernel limit`); not in spec (W1). |
| 4 | `retarget` ignores non-finite or non-positive rates | Acceptable, sensible hardening. |

## Issues
### CRITICAL
None.

### WARNING
- W1. The 1000:1 ratio limit is an unspecified rejection. R-RS-8 lists the rejection set and R-SS-5 says a valid target is resampled. A valid pair beyond 1000:1 (for example 192000 to 100) now yields `BadSampleRate`, and the user-facing loader would show "Error". Add the limit to the spec delta or document it when archiving.
- W2. Phase 4 loader and processor logic (retarget race handling, `settled_`/`published_` bookkeeping, `restoreGeneration_`) has zero automated tests and is JUCE-bound. Concurrency-sensitive code relies entirely on pending manual checks (4.6).
- W3. R-GR-7 "bit-identical to the output before this change" is verified against a plainly constructed buffer plus a negative control, not a pre-change golden. Reasonable given the libm portability argument, but it is a weaker proof than the spec wording.
- W4. `restoreGeneration_` behavior (skip persisting User results older than the last restore) is not covered by spec or tests. Minor residual race: `latestGeneration()` read after `request()` could include a concurrent `retarget()` bump from `prepareToPlay`, making the stored value slightly high. Low impact.
- W5. Perf margin is thin (2.4-2.5 s of 3 s) on the reference machine; a slower reference machine may fail the hidden check (not in CI).

### SUGGESTION
- S1. `OutOfMemory` handling in the resampler is untested (no allocation-failure injection). Disclosed by apply.
- S2. The R-RS-3 scenario text typo (0.55 x src) is already corrected in the spec delta (0.275 x src); task 5.2 asks to carry this into the main spec at archive.
- S3. Consider a small host-free test seam for `SampleLoader::retarget` state logic to cover R-SS-15 and R-PS-8 logic without JUCE.

## Manual checks still pending for the user (task 4.6)
Rate change reload (96k file, 44.1k to 48k); restore before prepare (publish at file rate, then exactly one reload; equal rates no reload); file deleted on reload (buffer stays, label not "Missing"); rapid rate changes (only last published); superseded long resample aborts; allocation-free `processBlock` during reload; peak memory release.

## Verdict
PASS WITH WARNINGS. Archive is possible once the user confirms 4.6 or accepts it as outstanding; W1 should be reflected in the specs at archive.
