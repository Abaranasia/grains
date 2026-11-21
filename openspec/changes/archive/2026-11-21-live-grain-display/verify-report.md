# Verification Report: live-grain-display

**Change**: live-grain-display (all slices)
**Mode**: Strict TDD, hybrid artifact store (engram + openspec)
**Base**: fd0adb5. Commits verified: 03b622e (docs), bc248ff (core snapshot + TripleBuffer), 3cd047d (GrainDisplayModel), 85ab354 (plugin glue). Branch feat/live-grain-view.
**Verdict**: PASS WITH WARNINGS (0 CRITICAL, 4 WARNING, 5 SUGGESTION)

## Delivery split (recorded as intentional)

The originally planned PR 1 (core) ran about 882 lines against the 800-line budget. By the tasks.md fallback, Phase 5 (GrainDisplayModel) shipped as its own slice, so delivery is three stacked slices: core snapshot + mailbox (bc248ff), display model (3cd047d), plugin glue (85ab354), plus the docs commit (03b622e). This is an intentional, documented split. Task 6.2 is treated as satisfied for verification purposes: its gates are green (grains_tests, grains_realtime_tests, R-BH-1 JUCE-free check at configure, render path unchanged by git diff). The orchestrator ticks 6.2 in tasks.md; this verify did not edit it.

## Completeness

| Item | State |
|------|-------|
| Tasks 1.1-1.4, 2.1-2.5, 3.1-3.3, 4.1-4.2, 5.1-5.2, 6.1, 7.1-7.4, 8.1-8.4, 9.1 | [x] done |
| Task 6.2 | [ ] in file; satisfied by the documented split; orchestrator to tick |
| Phase 10 (10.1-10.5 and sub-items) | [ ] CARRIED AS PENDING USER ACTIONS (manual); not failed |

## Build and test evidence (fresh dirs, real output)

Fresh build dirs under the session scratchpad (verify3-*). Repo build dirs untouched; no repo file edited except this report.

| Command | Exit | Result |
|---------|------|--------|
| core Debug: cmake -S . -B verify3-core-Debug -A x64 -DGRAINS_BUILD_PLUGIN=OFF; cmake --build --config Debug --clean-first; ctest -C Debug | 0 / 0 / 0 | 100% tests passed, 271 of 271 |
| core Release: same with Release | 0 / 0 / 0 | 100% tests passed, 276 of 276 (5 extra are the alloc-failure tests, compiled out under the MSVC debug runtime) |
| grains_realtime_tests.exe (Release) | 0 | All tests passed (115 assertions in 12 test cases), includes the new snapshot+publish zero-alloc case |
| grains_alloc_failure_tests.exe (Release) | 0 | All tests passed (232 assertions in 7 test cases) |
| ctest -R "triple buffer: two-thread stress" --repeat until-fail:30 (Release) | 0 | 30 Start/Passed pairs in the log (each about 0.02 s), "100% tests passed out of 1": the repeats really ran |
| plugin configure -DGRAINS_BUILD_PLUGIN=ON | 0 | R-BH-1 deferred JUCE-free check ran at configure, no FATAL_ERROR |
| plugin build Debug: GrainsPlugin_Standalone + GrainsPlugin_VST3 | 0 | Debug Grains.exe (27.6 MB) and Debug Grains.vst3/Contents/x86_64-win/Grains.vst3 (27.1 MB) exist |
| plugin build Release: GrainsPlugin_Standalone + GrainsPlugin_VST3 | 0 | Release Grains.exe (7.8 MB) and Grains.vst3 (7.6 MB) exist |

Compiler diagnostics: no C/C++ warnings in any build log. The only warnings are MSB8029 (intermediate dir under the temp directory), caused by the verify scratch location, not by the repo.

New test cases: triple_buffer 7, grain_snapshot 10, grain_display_model 6, realtime 1 (24 total; the ctest name regex for "display model" matches one extra pre-existing case).

Coverage tool: none configured. Quality metrics: no linter or type checker beyond the compiler; skipped (not a failure).

## Strict TDD compliance

| Check | Result | Details |
|-------|--------|---------|
| TDD evidence table reported | OK | apply-progress (#724) contains a per-task evidence table for the plugin slice (7.x-9.x) |
| Core slice per-task table | WARNING | Batch 1 evidence is prose only (271/276 green, mutation-RED for 1.3, 3.x, 4.x); no per-task RED/GREEN table for phases 1-5 (W2) |
| Test files exist for 1.x-5.x | OK | 4 files present and run |
| GREEN confirmed now | OK | all pass in Debug and Release; stress passed 30 repeats |
| Triangulation | OK | tests assert distinct values (empty vs saturated 128, held vs stale tick, 7 vs 8 idle ticks, density 40 vs 1000) |
| Safety net for modified files | OK | existing suite stayed green after modifying GrainPool, GranularEngine, test_engine_realtime |
| Assertion quality | OK | no tautologies, no ghost loops (loops guarded by REQUIRE(count > 0) or max_element > 0); stress asserts torn == 0, regressions == 0, last == kFrames |

## Spec compliance matrix

Counted from the delta specs: 7 requirements (R-WD-4 modified, R-WD-5, R-WD-6, R-WD-7, R-PS-4, R-PS-5, R-GR-11) and 45 scenarios (waveform-display 31, plugin-shell 10, grain-rendering 4). Automated/Catch2: 21, all COMPLIANT. Manual: 24, PENDING.

### R-GR-11 Read-only grain introspection (4, all Automated)

| Scenario | Tag | Evidence | Result |
|----------|-----|----------|--------|
| Bit-identical render with and without snapshot | Automated | test_grain_snapshot.cpp "rendering with a snapshot after every block is bit-identical" (400 blocks, memcmp both channels, per-block active counts) | COMPLIANT |
| Snapshot does not alter pool or RNG | Automated | "repeated snapshots between blocks do not alter pool or RNG" (5 extra snapshots per block, bitwise equal) | COMPLIANT |
| Snapshot is allocation-free | Automated | test_engine_realtime.cpp "snapshotting and publishing grains allocates nothing" (density 40 and 1000, AllocationScope, allocs == 0) | COMPLIANT |
| Compiles as const noexcept | Automated | static_assert on noexcept(snapshotGrains) of a const GranularEngine in test_grain_snapshot.cpp, compiled in both configs | COMPLIANT |

### R-WD-5 Grain snapshot contract (6 Automated)

| Scenario | Tag | Evidence | Result |
|----------|-----|----------|--------|
| Count equals active grains | Automated | "count equals the active grains and every mark is normalised" | COMPLIANT |
| Marks normalised and finite | Automated | "marks stay normalised at extreme parameters", "position clamps at the end of the buffer" | COMPLIANT |
| Slots distinct and stable | Automated | "slots are distinct, ordered and stable while a grain lives" | COMPLIANT |
| Empty and no source | Automated | "no source, an empty source or density 0 give an empty snapshot", "setSource and prepare empty the snapshot" | COMPLIANT |
| Saturated pool | Automated | "a saturated pool fills every slot exactly once" (realtime test also maxCount == 128) | COMPLIANT |
| Snapshot is versioned | Automated | "a default snapshot is empty and versioned" plus is_trivially_copyable and sizeof(GrainMark) == 16 static_asserts | COMPLIANT |

### R-WD-6 Snapshot publication (4 Automated, 2 Manual)

| Scenario | Tag | Evidence | Result |
|----------|-----|----------|--------|
| Reader sees last published frame | Automated | triple buffer "the latest of several publishes wins", "a published frame is fresh once" | COMPLIANT |
| Not fresh before any publish | Automated | "update reports not fresh before any publish" | COMPLIANT |
| Tear-free under two-thread stress | Automated | "two-thread stress never tears..." 100k frames of 512 bytes, 30 Release repeats | COMPLIANT |
| Publish is allocation-free | Automated | realtime test (see R-GR-11) | COMPLIANT |
| Publish on mono and stereo paths | Manual | Static: processBlock is one if / else-if / else chain then a single publishGrains(); stereo no longer early-returns | PENDING (10.x), static OK |
| Empty snapshot after prepare | Manual | Static: prepareToPlay calls publishGrains() after engine_.prepare | PENDING, static OK |

### R-WD-7 Live lane-dot view (5 Automated, 5 Manual)

| Scenario | Tag | Evidence | Result |
|----------|-----|----------|--------|
| Fresh frame displays | Automated | display model "a fresh frame is shown and requests a repaint" | COMPLIANT |
| Marks held until the timeout | Automated | "marks are held until the stale tick, which clears once" (7 idle ticks, no repaint) | COMPLIANT |
| Stale clears markers once | Automated | same test: 8th idle tick clears and returns true once, later ticks false | COMPLIANT |
| Fresh frame resumes after stale | Automated | "a fresh frame resumes after stale" | COMPLIANT |
| Constants named and distinct | Automated | static_asserts kPollHz == 30, kStaleMs == 250, kStaleTicks == 8, distinct | COMPLIANT |
| Dots appear and follow grains | Manual | Static: row = slot % 8, x via positionToX, alpha 0.25 + 0.75 * level, mint 0xff7ee0a1 | PENDING (10.2), static OK |
| Host stopped clears markers | Manual | Static: idle ticks reach kStaleTicks, tick clears once, view repaints the strip | PENDING (10.2), static OK |
| No markers without a waveform | Manual | Static: dots drawn only under if (showsWaveform()); timer not wanted otherwise; setWaveform resets model | PENDING (10.2), static OK |
| Static layers not recomputed per tick | Manual | Static: cached juce::Image rebuilt only when dirty or width-at-scale differs; tick repaints stripBounds() only | PENDING (10.4), static OK |
| CPU with multiple instances | Manual | Static: timer only while showing + Waveform + poll set | PENDING (10.4) |

### R-WD-4 Waveform view, modified (9 Manual)

All nine scenarios are Manual and PENDING (click jump, drag clamp, gestures, read-only jitter band, marker follows automation, empty hint, timer only while showing and fixed size, idle tick no repaint, editor open during load). Static evidence: mouse and attachment code unchanged from the archived change; setResizable(false, false) retained in PluginEditor.cpp; only WaveformView owns a juce::Timer; it starts via startTimerHz(kPollHz) only while showing, stops in updateTimer on hide or no-waveform and in the destructor; timerCallback repaints only when tick() returns true.

### R-PS-4 Processing and output (7 Manual), R-PS-5 Real-time safety (1 Manual, 2 Catch2)

| Scenario | Tag | Evidence | Result |
|----------|-----|----------|--------|
| R-PS-4: silence without sample, sound after load, source set only on change, parameter sweep, multi-rate prepare (5) | Manual | Engine call order unchanged (diff check below) | PENDING (10.1, 10.5) |
| R-PS-4: publish runs on mono and stereo paths | Manual | Static OK (see R-WD-6) | PENDING, static OK |
| R-PS-4: empty snapshot after prepare | Manual | Static OK | PENDING, static OK |
| R-PS-5: code review for RT rules | Manual | Static: publishGrains() = snapshotGrains(back()) + publish(): const reader, fixed inline storage, atomic exchange on atomic<uint8_t> with is_always_lock_free static_assert; no lock, alloc, I/O or throw; ScopedNoDenormals still first statement | COMPLIANT by review |
| R-PS-5: engine/mapper allocation-free | Catch2 | existing realtime tests pass (Release 12 test cases) | COMPLIANT |
| R-PS-5: snapshot and publish allocation-free | Catch2 | realtime snapshot+publish test | COMPLIANT |

## Correctness (static checks requested)

| Check | Result |
|-------|--------|
| Single publish after render on all 3 processBlock paths; engine call order unchanged | OK. git diff fd0adb5 HEAD on PluginProcessor.cpp: the two early returns became if (stereo) process / else if (unprepared mono) buffer.clear() / else unchanged chunk loop, then one publishGrains(). acquire/setSource, setParams mapping and the getNumChannels() <= 0 return are unchanged. |
| prepareToPlay publishes the empty snapshot | OK, after engine_.prepare |
| No Timer outside WaveformView | OK. rg over Source/plugin: only WaveformView.h/.cpp |
| Markers cleared on stale and on timer stop | OK. Stale: tick clears on the 8th idle tick. Stop: updateTimer calls model_.reset() and repaints the strip if marks were shown; setWaveform also resets. |
| No dots in Empty/NoWaveform | OK (dots only inside showsWaveform(); hint text from the cached image; timer not started) |
| Strip geometry vs design D7/D8 | OK. kStripHeight 48, kWaveformHeight 140, kViewHeight 188, kLaneRows 8, pitch 6, dot 4, alpha 0.25 + 0.75 * level, mint 0xff7ee0a1, static_assert tying rows * pitch to strip height; band and marker span full height; paint order image, band, dots, marker; editor uses WaveformView::kViewHeight |
| R-PS-3 still exactly 9 parameters | OK. static_assert(pm::kCount == 9) and CHECK(kSpecs.size() == 9) in test_param_spec.cpp (passing); no parameter file in the diff |
| Audio-path core unchanged except additive snapshot code | OK. git diff fd0adb5 HEAD on Source/core: 205 insertions, 0 deletions. GranularEngine.cpp adds only clamp01 and snapshotGrains; GrainPool.h adds const forEachActiveSlot; process, renderSegment, spawn untouched |
| JUCE-free core (R-BH-1) | OK. No JUCE include in Source/core; configure-time grains_assert_juce_free passed for core-only and plugin configs |
| R-WD-4 amendment consistent with code | OK. Spec: timer only while showing, single 30 Hz constant, repaint only on fresh or stale transition, static layers cached. Code does exactly that (and is stricter, see S1) |
| Doc hygiene (no absolute local paths, usernames, emails) | OK. rg -F over openspec/, Source/, tests/ for the local account name, the git author name, user-profile directory prefixes, the repository drive-root path, common mail domains and application-data directories: zero hits in lines added by this change. Remaining hits are pre-existing archived docs (repo-root path in archived tasks/explore/archive-report files) and synthetic Windows-style path fixtures in the sample-source spec and path-resolver test |
| Doc numbers agree with specs | OK. design and tasks cite R-WD-4..7, R-PS-4/5, R-GR-11; scenario text in tasks 2.1 and 5.1 matches the automated scenarios |

## Design coherence

| Decision | Followed | Notes |
|----------|----------|-------|
| D1 16-byte mark, versioned snapshot | Yes | static_assert 16 bytes; kVersion nested in GrainSnapshot (apply deviation, equivalent) |
| D2 forEachActiveSlot const | Yes | |
| D3 const noexcept snapshotGrains | Yes | progress expression equals renderGrain |
| D4 TripleBuffer (acq_rel exchange, 64-byte alignment, lock-free static_assert) | Yes | C4324 suppressed locally in the header |
| D5 single publish, publish after prepare | Yes | |
| D6 tick-counted model (30 Hz, 250 ms, 8 ticks) | Yes | |
| D7/D8 lanes and paint | Yes | |
| D9 std::function poll via pollGrains() | Yes | assigned once in the editor constructor |
| Apply deviations | Accepted | setWaveform also resets the model (strengthens "no stale dots after swap"); hint text centred in the 140 px area |

Docs drift: proposal.md and explore.md still carry the superseded numbers (about 250 ms clear, 8192-sample flicker threshold, 12-byte mark). The design section "Proposal Corrections" records the correct values: 8 ticks (about 267 ms), flicker above about 233 ms blocks (about 10k samples at 44.1 kHz), 16-byte mark. Code and specs agree with the design.

## Issues

### CRITICAL
None.

### WARNING
- W1. Manual phase 10 (10.1 audio A/B, 10.2 lane dots, 10.3 timer lifecycle, 10.4 CPU/DPI/large blocks, 10.5 regression) is unrun, leaving 24 manual spec scenarios unverified at runtime. Carried as pending user actions per instruction, not failing the change. Archive should wait for the user to complete or explicitly waive them.
- W2. apply-progress lacks a per-task RED/GREEN table for the core slice (phases 1-6): only prose plus the plugin-slice table. The earlier "no table at all" warning did not recur, but TDD evidence for 1.x-5.x cannot be audited from a table, and the mutation-RED claims are not reproducible here.
- W3. Spec vs design wording: R-PS-4 says the publish is the final step "on every render path ... including any early return", while design D5 and the code deliberately keep the getNumChannels() <= 0 return before the chain with no publish. Runtime impact is nil (no render, nothing to show). Reword the spec to "every path that renders" at sync or archive.
- W4. tasks.md still forecasts two stacked PRs (PR 1 about 705 lines) and shows 6.2 unticked; actual delivery is three slices with PR 1 at about 882 lines. Orchestrator to tick 6.2 and note the split before archive.

### SUGGESTION
- S1. R-WD-4 text does not state that the timer also requires the Waveform display state and a poll function (code and R-WD-7 do). Add at sync.
- S2. proposal.md and explore.md keep the superseded numbers; add a one-line pointer to the design Proposal Corrections.
- S3. The R-GR-11 allocation scenario says "extreme parameters"; the realtime snapshot test covers density 40 and 1000 with 1000 ms grains (saturated pool) but not pitch or position extremes. The existing realtime extremes test and the snapshot normalisation extremes test cover those parts separately.
- S4. Stress test check observed >= 1 is weak; the meaningful guards are torn == 0, regressions == 0 and last == kFrames. Acceptable. A TSAN CI job remains explicitly out of scope.
- S5. Tick-based staleness gives about 267 ms and can stretch under a loaded message thread. Documented and accepted.

## Pending manual actions for the user (carried, not failing)

- 10.1 Audio A/B bounce, mono and stereo, same seed: bit-identical or null result (R-GR-11, R-PS-4).
- 10.2 Lane dots: appear under the waveform and align with the marker; geometry 48 px strip, 8 rows, 6 px pitch, 4 px mint dots, dimmer new dots; none in Empty/NoWaveform; markers clear about 267 ms after transport stop; no flicker when idle.
- 10.3 Timer lifecycle: minimise stops the timer and resets markers, resumes on restore; editor close and reopen; source swap with editor open.
- 10.4 CPU with 4 or more instances; DPI change rebuilds the cached image and the window stays non-resizable; large-block flicker accepted.
- 10.5 Regression (R-PS-6): 9 generic controls, Load button, status label, waveform click/drag.

## Final verdict

PASS WITH WARNINGS. All 21 automated scenarios pass in Debug and Release, builds are clean for core and plugin in both configurations (Debug VST3 artifact confirmed), the stress test passed 30 consecutive repeats, and static reading of the glue code matches the specs and design. Safe to proceed to archive once the orchestrator ticks 6.2 and the user completes or explicitly waives Phase 10.
