# Verify report: juce-fetch-license

Branch feat/juce-fetch-license, HEAD 03fe028 (931eae6 build, f8b48be licensing, 03fe028 records).

## Verdict: PASS. CRITICAL 0, WARNING 0, SUGGESTION 8.

## Re-run evidence (this phase)
| Check | Expected | Result |
|---|---|---|
| Fresh default build + ctest (build/verify-jf, MSVC Debug, plugin OFF) | 122/122 | 100% passed, 122/122 |
| `git show HEAD:LICENSE \| sha256sum` | 0d96a4ff...abcb0 | Match |
| SPDX AGPL-3.0-or-later over `git ls-files Source tests cmake CMakeLists.txt README.md` | 56 files, once each | 56 files, 56 matches, none missing or duplicated |
| Fetch OFF + empty GRAINS_JUCE_DIR + plugin ON | FATAL_ERROR | CMake Error at CMakeLists.txt:54, message names the three remedies |
| Working tree after checks | clean | Clean (build/ is git-ignored) |

Note: the "Visual Studio 17 2022" generator was not found here, so the default generator was used (still MSVC Debug).

## Requirement compliance
- R-BH-3 (modified, opt-in plugin, local JUCE or pinned fetch): COMPLIANT. Plugin OFF by default and default build is JUCE-free (122/122). Resolution order: local JUCE, then FetchContent of pinned commit f8f88641... with URL_HASH SHA256, then FATAL_ERROR when fetch is OFF. A post-fetch existence guard is also present.
- R-BH-5 (modified, CI): COMPLIANT. ci.yml has an informational plugin job with a pinned-SHA actions/cache keyed on the JUCE SHA plus hashFiles of CMakeLists.txt. Run 40119183345 on f8b48be: all three jobs succeeded, including job 120946789265 (cache miss about 1 s, configure 1m53s including download, build 1m17s). Log unreadable (admin rights); runner has no local JUCE, so a successful configure implies the fetch path.
- R-LIC-1 (LICENSE text): COMPLIANT. HEAD:LICENSE hash matches.
- R-LIC-2 (SPDX headers): COMPLIANT. 56 of 56, once each.
- R-LIC-3 (README licensing statement): COMPLIANT. README line 38 states AGPL-3.0-or-later and links LICENSE; line 40 states the JUCE AGPLv3 option.

## Limits
- No full fetch build re-run here (G1 at apply, CI exercised it).
- CI log content inferred from job results, not read.

## Suggestions (non-blocking follow-ups)
1. Single CMake variable for the JUCE SHA (duplicated in URL, status message, CI cache key).
2. README "so ... therefore" wording at line 40 is redundant.
3. design.md shows a placeholder hash where the real hash is now known.
4. G2/G4 "_deps" wording is imprecise (SOURCE_SUBDIR download-only layout).
5. Include the URL_HASH digest in the CI cache key.
6. A full local-JUCE build is not recorded (G4 only configured).
7. R-LIC-2 "YAML-free scripts" wording is unclear.
8. apply-progress.md has no task 2.8 entry.
