# Proposal: Sample Loader Fixes

## Intent

Close follow-up W1 from `sample-loader`: a superseded request can still publish, violating R-SS-7. Also clarify spec wording that conflates "clear" with "publish", and document the truncation status already shown in the UI.

## Current state

- `SampleLoader::load` checks `isCurrent()` (`gen_ == r.generation`, line 63/122), then calls `exchange_.publish(...)` (line 124) without a lock. `request()` bumps `gen_` under `lock_`. A request arriving between check and publish lets the stale buffer publish; if that newer request is a failing user load, the stale buffer stays active.
- The empty-path restore branch (line 65) clears without any generation check.
- `SourceExchange::publish` allocates a `Box` and frees a superseded pending `Box` inline.
- R-SS-7 says "nothing MUST be published" on failure; R-SS-9 says restore failure "clears the exchange", which code does via `publish(nullptr)`. Truncation label `" (truncated to 5 min)"` (line 129) is unspecified. Legend lines and R-PS-2 use "verify phase" / "in this change".

## Scope

### In Scope
- Generation-gated publish: check and publish are atomic with respect to `request()` bumping the generation; applies to every publish/clear site.
- JUCE-free core primitive covering the gate, with Catch2 tests (Strict TDD).
- Spec deltas below.

### Out of Scope
- W2 `stopThread(4000)` timeout; privacy of stored absolute paths (follow-up); S1-S5 test nits; limiter.

## Capabilities

### New Capabilities
None

### Modified Capabilities
- `sample-source`: R-SS-7 (failure keeps previous source, publishes nothing; superseded requests never publish or clear), R-SS-9 (restore failure clears via explicit clear, not "publishing an empty buffer"), truncation status "(truncated to 5 min)"; remove process wording in legend/modified requirements.
- `plugin-shell`: R-PS-2 wording cleanup only ("in this change").

## Approach

Recommended (final shape in design):
1. Core `grains` gate owning generation + `std::mutex`: `next()` (bump, used by `request()`), and a commit-if-current call that runs a short critical section only when the generation matches.
2. Split `SourceExchange` publish into allocate-outside-lock (`stage`) and a cheap pointer `exchange` inside the gate; the superseded `Box` is returned and freed after the lock is released, on the loader thread. Audio thread contract unchanged (never frees).
3. Explicit `clear()` naming for the restore-failure path, gated the same way.

Rejected: atomic compare then publish without a lock (still a TOCTOU window); freeing under the lock (message thread may block in `request()`).

## Affected Areas

| Area | Impact | Description |
|------|--------|-------------|
| `Source/core/include/grains/SourceExchange.h` | Modified | stage/commit split, clear |
| `Source/core/include/grains/` (new gate header) | New | generation gate |
| `Source/plugin/SampleLoader.{h,cpp}` | Modified | use gate at all publish sites |
| `tests/test_source_exchange.cpp`, new gate test | Modified/New | race and supersede tests |
| `openspec/specs/sample-source`, `plugin-shell` | Modified | deltas |

## Risks

| Risk | Likelihood | Mitigation |
|------|------------|------------|
| SPSC invariants broken by API split | Low | Keep existing tests; add stage/commit tests |
| Lock held during free | Low | Return superseded Box, free outside |

## Rollback Plan

Revert the branch commits; `publish()` path from `sample-loader` is restored unchanged.

## Success Criteria

- [ ] Catch2: a commit with a stale generation does not change exchange state; superseded Box freed outside the lock.
- [ ] No unguarded publish/clear remains in `SampleLoader`.
- [ ] Specs state clear-vs-publish and truncation status; no process wording in modified requirements.

## Proposal question round (auto mode, assumptions for review)
- A superseded restore must not clear the source either (assumed yes).
- Truncation is status-only; no extra warning UI (assumed).
