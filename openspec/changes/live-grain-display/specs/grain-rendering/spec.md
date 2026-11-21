# Delta for grain-rendering

Delivery tag: **PR1** = core PR. The next free id after R-GR-10 is R-GR-11 (verified; no collision).

## ADDED Requirements

### R-GR-11 Read-only grain introspection [PR1]
Reading the active grains for display (`GranularEngine::snapshotGrains(GrainSnapshot&) const noexcept`) MUST be a pure observation: it MUST NOT change the rendered output, MUST NOT consume or advance any RNG draw, MUST NOT modify pool state (slot assignment, free list, grain phase, active count), and MUST NOT allocate, lock, throw, or block. It MUST iterate the pool as const, with at most pool-capacity iterations. Calling it any number of times between blocks, or not at all, MUST yield bit-identical audio for the same source, parameters, seed, sample rate, and block partition (R-GR-6 is preserved).

#### Scenario: Bit-identical render with and without snapshot [Catch2]
- Given two engines with the same source, parameters, seed, sample rate, and block partition
- When engine A renders 2 s taking a snapshot after every block (and extra snapshots between blocks) and engine B renders the same 2 s with no snapshot calls
- Then the two outputs are bit-identical on both channels

#### Scenario: Snapshot does not alter pool or RNG [Catch2]
- Given an engine rendering with active grains
- When snapshots are taken repeatedly without rendering
- Then `activeGrainCount()` is unchanged and the next rendered block equals the block rendered by an identical engine that was never snapshotted

#### Scenario: Snapshot is allocation-free [Catch2]
- Given the test binary that counts global operator new/delete calls
- When the counter is reset after preparation and snapshots are taken while the pool is saturated and at extreme parameters
- Then the allocation count is 0

#### Scenario: Snapshot compiles as const noexcept [Catch2]
- Given a `const GranularEngine`
- When `snapshotGrains` is called on it
- Then it compiles and the call is `noexcept`
