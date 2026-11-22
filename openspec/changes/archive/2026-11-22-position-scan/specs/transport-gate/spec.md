# Delta for Transport Gate

## MODIFIED Requirements

### R-TG-4 Deterministic restart
Every transition to Starting from Stopped MUST reseed the engine rng with the engine's configured seed and start from a fresh pool and scheduler, so that two Plays with the same source and parameters produce bit-identical output sequences, each beginning with gain 0.0. The scan offset of `grain-rendering` R-GR-12 (offset, base and elapsed-sample counter) MUST also be reset to 0 on every such transition, so a Play always restarts reading from the `position` anchor; the same reset MUST occur on every new source load (hard stop on source swap, R-TG-10) and on `prepare`. A Play while already Playing or Starting MUST be a no-op (no reseed, no reset, no scan offset reset). A Play requested during Stopping MUST NOT reverse the ramp: the fade-out MUST run to completion (exact zeros, pool and scheduler reset as in R-TG-3) and the pending Play MUST then restart from fresh exactly like a Play from Stopped (rng reseed, pool and scheduler reset, scan offset reset, fade-in from 0.0). Consequently every Play that starts a fade-in, whether requested from Stopped or latched during Stopping, produces the identical output sequence. A Stop requested during Starting is unaffected (it reverses from the current gain, R-TG-1).
(Previously: restart reset rng, pool and scheduler only; no scan offset existed.)

#### Scenario: Two Plays identical [Catch2]
- Given an engine that plays, is stopped to exact zeros, and is played again, with unchanged parameters and source
- When the first N samples after each Play are compared
- Then they are bitwise identical

#### Scenario: Play during fade-out finishes the fade, then restarts fresh [Catch2]
- Given an engine in the middle of Stopping
- When Play is requested
- Then the gain keeps decreasing to exactly 0.0 (no reversal, no jump), and afterwards a fade-in starts whose output equals, bitwise, the first Play's output for the same source and parameters

#### Scenario: Redundant Play is a no-op [Catch2]
- Given a gate in Playing
- When Play is requested again
- Then the output is bit-identical to the run without the extra request and no rng reseed occurs

#### Scenario: Play resets the scan offset [Catch2]
- Given `scan` = 0.5 played for 2 s, then Stop to exact zeros, then Play with unchanged parameters and source
- When the first N samples after each Play are compared
- Then they are bitwise identical and the first grain after the second Play starts at the `position` anchor

#### Scenario: Redundant Play keeps the scan offset [Catch2]
- Given `scan` = 0.5 Playing for 2 s
- When Play is requested again
- Then the output is bit-identical to the run without the extra request (offset not reset)

#### Scenario: New load resets the scan offset [Catch2]
- Given `scan` = 0.5 Playing for 2 s, then a new source is set and Play requested
- When the output is compared with a fresh engine playing the new source with the same seed and parameters
- Then the outputs are bitwise identical
