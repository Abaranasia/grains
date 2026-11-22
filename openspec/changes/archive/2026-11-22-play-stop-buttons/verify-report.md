# Verify Report: play-stop-buttons

Verdict: PASS WITH WARNINGS (0 CRITICAL, 4 WARNING, 3 SUGGESTION). Mode: Strict TDD.
Worktree: Grains-worktrees/play-stop-buttons (branch feat/play-stop-buttons), all uncommitted.

## Tests (fresh builds)
- Debug core: 295/295 passed (psb-verify-dbg).
- Release core: 300/300 passed (psb-verify-rel; includes realtime and alloc-failure targets).
- Plugin Release build (VST3 + Standalone) rebuilt cleanly in psb-plugin-build.
- T0 gate: 85ab354 is an ancestor of HEAD.

## Compliance matrix
| Requirement | Evidence | Status |
|---|---|---|
| R-TG-1 states, ramp, endpoints, reversal, initial Playing, default Stopped | TransportGate.h:19-61,92; test_transport_gate.cpp (default, ramp length, endpoints, reversal); test_engine_transport.cpp:95,122,222 | COVERED |
| R-TG-2 steady bit-identity | TransportGate.h:67; test_engine_transport.cpp:222 | COVERED |
| R-TG-3 exact +0 and reset once | GranularEngine.cpp:90-94; test_engine_transport.cpp:148 | COVERED |
| R-TG-4 deterministic restart, latch, redundant no-op | GranularEngine.cpp:70-74; tests :172,:193,:248 | COVERED |
| R-TG-5 no draws while stopped | GranularEngine.cpp:80; rngState(); test :109 | COVERED |
| R-TG-6 bounded | gate before softLimit (cpp:90); test :285 | COVERED |
| R-TG-7 block-split invariance | test :371 (5 splits) | COVERED (see S2) |
| R-TG-8 prepare recompute/reset | TransportGate.h:23; gate tests | COVERED |
| R-TG-9 realtime safety | noexcept throughout; test_engine_realtime.cpp:401 | COVERED |
| R-TG-10 hard stop, setSource neutral | cpp:46-52; tests :311,:328 | COVERED |
| R-PS-4 processor wiring | PluginProcessor.cpp:65-66,84-89,138,180 | CODE MATCHES SPEC, no automated test (W2) |
| R-PS-4 empty snapshot every block while stopped | publishGrains runs after every render (cpp:117); engine returns early inside process only | CODE OK, manual M4 |
| R-PS-6 editor | PluginEditor.cpp:20-35,46-47,58-65; .h:28-31 | CODE MATCHES SPEC, manual M1/M6 pending |
| R-PS-3/R-PS-7 unchanged, not persisted | playing_ is a plain atomic, no APVTS/ValueTree | OK |
| R-WD-4 empty fresh frame no repaint | pre-existing GrainDisplayModel (empty after empty returns no repaint), WaveformView.cpp:84; test_grain_display_model.cpp:60 | COVERED, no change needed |
| R-WD-7 empty snapshot clears with one repaint | pre-existing model; test_grain_display_model.cpp | COVERED |

Deviation judged: onLoadResult clears playing_ only when r.loaded || r.cleared (PluginProcessor.cpp:180). This satisfies R-PS-4 ("a failed load MUST keep the previous source and leave the plugin Stopped"): a failed load does not change the source, and processBlock/prepare/restore already force Stopped; keeping the flag on a failed load never un-stops a stopped plugin because a failed load cannot start playback. Acceptable. Note: setStateInformation clears playing_ unconditionally, so a failed restore also ends Stopped (matches spec).

## Design coherence
D1-D10 implemented as designed (names, signatures, reset points, gate before limiter, seed_ stored, stopNow, 10 Hz sync on change with dontSendNotification, timer stopped in dtor). Only wording drift: D8 says onLoadResult always clears; code clears on loaded/cleared (see S1).

## Tasks
T0, T1.1-T1.3, T2.1-T2.4, T3.1-T3.2, T4.1, T5.1 all [x] and match code. M1-M7 and D1-D4 pending by design (not failures).

## Drone / always-on sweep
- Source/ and tests/: no occurrences of drone / always-on.
- README.md:31 still says "The engine plays continuously once a sample loads (no MIDI gating)": stale (W3).
- Main specs (openspec/specs/plugin-shell/spec.md:5,46,179) still contain drone / always-on / renders continuously; resolved at archive via D1/D4 (pending by design).

## Diff size
Tracked: +204/-53 (18 files). Untracked code: TransportGate.h 100, test_engine_transport.cpp 385, test_transport_gate.cpp 209. Total about 898 added / 951 changed lines, above the 800 budget and the design estimate of 585 (W1). Plugin glue is only 53 changed lines, so the tasks.md Phase 3/4 split would leave PR A (core + tests) at about 845 added lines, still over budget.

## Findings
CRITICAL: none.
WARNING
- W1: change size about 898 added lines exceeds the 800-line review budget (tests are about 650 of it). Needs maintainer-approved size exception or a split (for example gate + gate tests / engine + engine tests / plugin glue as chained PRs).
- W2: processor and editor requirements (R-PS-4 scenarios tagged Catch2/Manual, R-PS-6) have no automated test; no test target links the processor/editor. Covered only by engine tests plus manual M1-M7 (all pending).
- W3: README.md:31 describes the old always-on behaviour; not listed in D1-D4. Update with the docs commit.
- W4: manual checks M1-M7 and docs D1-D4 still pending; must run before archive (M3 and M6 confirm the accepted prepare and Play-click races).
SUGGESTION
- S1: align design D8 text with the onLoadResult behaviour (clears on loaded/cleared only).
- S2: in the mono chunked path (host block larger than scratchSize_) a latched Play can restart in a later chunk of the same block rather than the next block; only reachable when a host exceeds its declared max block size. Document or ignore.
- S3: T2.3 asks to record mutation checks for tests green on first run; apply-progress does not state them explicitly.

## Next
sdd-archive after M1-M7 and D1-D4 are done and the size exception is accepted; no CRITICAL blockers.
