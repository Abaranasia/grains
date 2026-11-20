# Build Harness Specification (delta)

## ADDED Requirements

### R-BH-6 Fail-injection test executable
Allocation-failure injection (an `armFailure(n)` facility that makes the n-th subsequent global allocation fail, with a way to disarm it) MUST be provided by the test-support `AllocationCounter` and MUST be linked only into a dedicated test executable separate from `grains_tests`. `grains_tests` MUST NOT replace or link the global `operator new`/`operator delete` or the fail-injection code. The dedicated executable MUST be registered with CTest so `ctest` runs it, MUST build without JUCE (R-BH-1), and MUST honour the sanitizer exclusion fallback of R-BH-5 (it MAY be excluded from the sanitizer job while still running on the Windows job). Arming MUST be thread-local-safe or documented single-thread, MUST be disarmed automatically after it fires once, and an unarmed run MUST behave exactly as the existing allocation counter. A failing injection test MUST make `ctest` return non-zero.

#### Scenario: Injection isolated from grains_tests [Catch2]
- Given the configured build
- When the link dependencies and sources of `grains_tests` are inspected
- Then neither the fail-injection code nor a replacement global `operator new` is present

#### Scenario: Executable registered with ctest [CI]
- Given a configured and built tree
- When `ctest` is run
- Then the fail-injection executable's cases are discovered and executed, and a deliberately failing case makes the exit code non-zero

#### Scenario: Nth allocation fails once [Catch2, separate executable]
- Given `armFailure(3)`
- When code performs five allocations
- Then only the third throws `std::bad_alloc`, the others succeed, and the facility is disarmed afterwards

#### Scenario: Unarmed behaviour unchanged [Catch2, separate executable]
- Given no failure armed
- When existing realtime allocation-counting cases run
- Then their counts and results match the previous behaviour

#### Scenario: Sanitizer fallback [CI]
- Given the sanitizer job cannot coexist with the replacement `operator new`
- When it runs
- Then it MAY exclude the fail-injection executable by CTest name, and the Windows job still runs it
