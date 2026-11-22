# Design: Play and Stop Buttons

## Technical Approach

Explore Approach 1 (decisions #730-#736). A JUCE-free `TransportGate` member of `GranularEngine` ramps the pre-limit grain sum. The engine runs a level-triggered Play/Stop target and evaluates it once per `process()` call. Stop fades out, then empties the pool. Play reseeds the rng, resets pool and scheduler, and fades in. Because a Play is a full restart, nothing that happens while stopped can affect later output. The processor holds a non-persisted atomic flag, and the editor adds a Play/Stop radio pair.

## Architecture Decisions

| # | Topic | Choice | Rejected (why) |
|---|---|---|---|
| D1 | Gate shape | Integer counter `pos_` in `[0, len_]`, gain `float(pos_)/float(len_)`: exact 0 and 1, step 1/len, no drift. `len_ = max(1, lround(kRampSeconds*sr))`, `kRampSeconds = 0.020` (960 samples at 48 kHz) | Float accumulator (drift, endpoints not exact); exponential (never reaches 0) |
| D2 | Gate placement | Multiply the raw sum before the `softLimit` loop. Skipped while `Playing`, so steady output is bit-identical. A zero gain writes `+0.0f` | After softLimit (would add a second clamp path, and -0 or NaN handling gets fragile) |
| D3 | Play during a fade-out | Latched: the fade completes, and the restart happens on the next call | Reverse the ramp (resumes old grains, so restarts are not identical) |
| D4 | Stop during a fade-in | Reverse from the current `pos_` (no click, finishes sooner) | Finish the fade-in first (slow) |
| D5 | Reset points | Play (Stopped to Starting, at the start of the call): `rng_.seed(seed_)`, `pool_.reset()`, `scheduler_.reset()`. This is the same state as `prepare`. Stop complete (end of the call): `pool_.reset()`, `scheduler_.reset()`. Stopped: early return before scheduling (no rng draws, zero CPU) | Reset mid-block at the exact zero crossing (needs a split segment loop, and the output is already 0) |
| D6 | Initial state | `prepare(sr, seed, InitialTransport initial = Stopped)`. Existing tests that expect sound pass `Playing`: no ramp, exactly today's path, assertions unchanged | Default Playing (contradicts the proposal); Stopped then `setPlaying(true)` in old tests (the ramp changes early-sample assertions) |
| D7 | Source swap | New `stopNow()`: gate hard to Stopped, target false, pool and scheduler reset. The processor calls it when it adopts a changed source. `setSource` itself is unchanged | Faded stop on swap (the new sample would blip for 20 ms); stop inside `setSource` (breaks the prepare-then-setSource test fixtures) |
| D8 | Plugin flag | `std::atomic<bool> playing_{false}`. Message thread: `setPlaying` and `isPlaying`. Audio thread: one relaxed load per block, then `engine_.setPlaying`. Cleared in `prepareToPlay`, `setStateInformation`, on source adoption in `processBlock`, and in `onLoadResult` only when the result is `loaded` or `cleared` (a failed load keeps the previous source and state, which is already Stopped) | APVTS parameter (breaks R-PS-3, needs persistence); ValueTree property (gets persisted) |
| D9 | UI sync | Editor `juce::Timer` at `kTransportPollHz = 10`. Toggle and enable states are set only on change, with `dontSendNotification`. The change callback syncs immediately | WaveformView's 30 Hz poll (runs only while the waveform is showing, couples concerns); AsyncUpdater from the audio thread (not realtime-safe) |
| D10 | Spec home | Separate `transport-gate` capability, cross-referencing R-GR-6 and R-GR-10 | R-GR-12 in grain-rendering (that spec is per-grain; the state machine needs its own rules) |

## Data Flow

    Play/Stop click --> proc.setPlaying(b) --> playing_ (atomic)
    processBlock: acquire source --(changed)--> setSource, stopNow, playing_=false
                  load playing_ once --> engine.setPlaying --> process():
                  request -> [restart] -> schedule+render -> gate.apply -> softLimit
                  -> publishGrains (always; count 0 while stopped)
    Editor timer/ChangeListener --> isPlaying / sampleLoaded --> buttons

The mono path calls `process` in chunks. Each chunk sees the same target, and the restart happens only once because the state is already `Starting` after the first chunk.

## Interfaces / Contracts

```cpp
// Source/core/include/grains/TransportGate.h (header-only, noexcept, no allocation)
class TransportGate {
public:
    static constexpr double kRampSeconds = 0.020;
    enum class State : std::uint8_t { Stopped, Starting, Playing, Stopping };
    void prepare(double sampleRate) noexcept;   // len from rate; Stopped, pos 0
    void forcePlaying() noexcept;               // Playing, pos = len
    void forceStopped() noexcept;               // Stopped, pos 0
    bool request(bool play) noexcept;           // true iff Stopped -> Starting (caller restarts)
    void apply(float* l, float* r, int n) noexcept; // ramps; may end in Playing or Stopped
    State state() const noexcept; bool steady() const noexcept; bool stopped() const noexcept;
    int rampLength() const noexcept; float gain() const noexcept;
};
// GranularEngine additions
enum class InitialTransport : std::uint8_t { Stopped, Playing };
void prepare(double sr, std::uint64_t seed, InitialTransport = InitialTransport::Stopped) noexcept; // stores seed_
void setPlaying(bool play) noexcept;  // target, evaluated at the next process()
void stopNow() noexcept;
TransportGate::State transportState() const noexcept;
```

Ramp sample rule: `Starting` multiplies by `pos/len` and then increments, so the first sample is exactly 0. `Stopping` decrements and then multiplies. With no source, `request` is still evaluated, but the ramp does not advance, so the output stays zeros.

## File Changes

| File | Action | ~Lines |
|---|---|---|
| `Source/core/include/grains/TransportGate.h` | Create | 90 |
| `Source/core/include/grains/GranularEngine.h`, `src/GranularEngine.cpp` | Modify | 45 |
| `tests/test_transport_gate.cpp` | Create | 140 |
| `tests/test_engine_transport.cpp` | Create | 160 |
| `tests/test_engine_realtime.cpp` | Modify (toggles inside AllocationScope) | 25 |
| `tests/test_engine_*.cpp`, `test_grain_render`, `test_soft_limiter` | Modify (`InitialTransport::Playing`) | 30 |
| `tests/CMakeLists.txt` | Modify | 3 |
| `Source/plugin/PluginProcessor.h/.cpp` | Modify | 35 |
| `Source/plugin/PluginEditor.h/.cpp` | Modify | 55 |

Code total is about 585 lines, one PR under the 800-line budget. Spec deltas (about 150 lines) go in a separate documentation commit, as in earlier changes.

## Testing Strategy (Strict TDD, Catch2, unit layer only)

| Test | Assertion |
|---|---|
| Gate endpoints | `Starting`: first gain exactly 0; reaches exactly 1 and turns `Playing` after `len`; each step at most 1/len plus 1 ulp; `len` at 44.1, 48 and 96 kHz |
| Gate reversal and latch | Stop mid fade-in continues from `pos`; Play mid fade-out stays `Stopping` until 0 |
| Steady bit-identity | Engine A `prepare(Playing)` vs engine B `prepare(Stopped)` + Play: bitwise equal for `i >= len`, and `abs(B) <= abs(A)` before that |
| Exact zeros | After Stop: every sample from completion onward is `+0` (bitwise), the active count is 0, and Stopped calls stay zeros |
| Deterministic restart | Play, Stop, Play after arbitrary history equals the first Play, bitwise |
| Block split | Two partitions with Play and Stop at the same absolute samples (block boundaries in both) give bitwise-equal output |
| Bounds | Every sample satisfies `abs(y) < 1` during ramps with the DC worst case (density 200, size 1000, gain 2) |
| `stopNow` / `prepare` | Stopped immediately; `prepare` recomputes `len` |
| Realtime | 0 allocations across toggles |

Manual: buttons disabled before a load, Stopped after load, restore and prepare, dots clear on Stop, no clicks.

## Threat Matrix

N/A: no routing, shell, subprocess, VCS/PR automation, executable-file classification, or process-integration boundary.

## Migration / Rollout

No migration (nothing is persisted). Depends on live-grain-display. The archived plugin glue (85ab354, `feat/live-grain-view`) owns `publishGrains` and the if/else processBlock chain (its design D5). Base the change on a branch that contains that commit, or on main after the merge. Rebase the core edits onto the `snapshotGrains` version of `GranularEngine`.

## Open Questions

- [ ] Some hosts call `prepareToPlay` mid-session (bypass, offline render), which silently resets to Stopped, as specified. Confirm that this is acceptable.
- [ ] Accepted race: a Play click between the loader's publish and the audio thread adopting the source is cleared. The UI resyncs within 100 ms.
- [x] D6 differs from the proposal wording: `prepare` defaults to Stopped, but tests may start in Playing without a ramp. Resolved: `transport-gate` R-TG-1 words it (default Stopped after construction and `prepare`; the engine/test API MAY offer an explicit initial Playing with no ramp).
- [ ] A Load or Restore that fails keeps the old source. `sampleLoaded` stays true and the state is still Stopped.
