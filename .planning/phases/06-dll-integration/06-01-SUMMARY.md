---
phase: 06-dll-integration
plan: 01
subsystem: infra
tags: [c++, win32, registry, shell-extension, dll, cli-flags]

# Dependency graph
requires:
  - phase: 05-enhanced-settings-ui
    provides: HKCU\Software\ClaudeFromHere registry writes for Continue, Resume, DangerouslySkipPermissions, AllowDangerouslySkipPermissions, RemoteControlPrefix, Channels
provides:
  - DLL reads the six new v1.1.0 registry keys via RegGetValueW with RRF_ZEROONFAILURE
  - Command line emits -c, -r, --dangerously-skip-permissions, --allow-dangerously-skip-permissions, --remote-control-session-name-prefix <value>, and repeated --channels <entry> in D-08 order
  - Channel splitter (wcstok_s on '|') with whitespace trim, empty-entry skip, and hard 32-entry cap
  - szFlags buffer bumped from 4096 to 16384 WCHARs
affects:
  - 06-02-rebuild-install-verify (must rebuild DLL and right-click verify)
  - Future phases touching shell extension command-line assembly

# Tech tracking
tech-stack:
  added: []
  patterns:
    - "Registry read pattern: RegGetValueW with RRF_ZEROONFAILURE so absent keys silently become 0/empty, no explicit fallback code"
    - "Flag append pattern: if (dw) or if (sz[0]) guards around StringCbCatW calls"
    - "Channel splitter pattern: wcstok_s with in-place tokenization + trim + empty-skip + hard cap"

key-files:
  created: []
  modified:
    - src/ClaudeFromHere.cpp

key-decisions:
  - "Inline the channel splitter directly in _LaunchClaude rather than extracting a helper (discretion granted in D-Context); matches existing inline style of the Model/AllowedTools/ExtraFlags block"
  - "Use wcstok_s for channel splitting (CRT-provided, no extra dependency) with in-place modification of szChannels"
  - "Hard-coded 32-entry channel cap enforced via loop counter, no dynamic allocation"
  - "RemoteControlPrefix and channel tokens appended raw (unquoted) per D-05, mirroring existing Model/AllowedTools/ExtraFlags behavior"

patterns-established:
  - "Six-new-keys read block sits immediately after the existing four reads; preserves visual grouping"
  - "D-08 flag ordering is encoded structurally as the order of if-blocks in the build-flags section; ExtraFlags append remains last"
  - "Whitespace trim + empty-skip guarantees trailing '|' and '||' in Channels produce no spurious --channels emissions"

requirements-completed:
  - CHN-04
  - INT-01

# Metrics
duration: 2min
completed: 2026-04-10
---

# Phase 6 Plan 1: Extend _LaunchClaude Summary

**C++ DLL now reads six new HKCU registry keys and emits -c, -r, dangerous-permission flags, --remote-control-session-name-prefix, and repeated --channels entries in D-08 canonical order, closing CHN-04 and INT-01.**

## Performance

- **Duration:** ~2 min
- **Started:** 2026-04-10T23:45:49Z
- **Completed:** 2026-04-10T23:47:13Z
- **Tasks:** 2
- **Files modified:** 1

## Accomplishments
- All six new HKCU\Software\ClaudeFromHere keys wired into _LaunchClaude (Continue, Resume, DangerouslySkipPermissions, AllowDangerouslySkipPermissions, RemoteControlPrefix, Channels)
- Flag emission block extended in exact D-08 order with ExtraFlags remaining last as user escape hatch
- Channel splitter implemented with whitespace trim, empty-entry skip, and hard 32-entry cap (D-03) via wcstok_s
- szFlags buffer bumped 4096 -> 16384 WCHARs (D-15) to absorb up to 32 channel entries plus other flags
- Byte-for-byte pre-v1.1.0 behavior preserved when every new key is absent/zero/empty (achieved via RRF_ZEROONFAILURE + guarded if-blocks, no special-case code)

## Edited Regions in src/ClaudeFromHere.cpp

Two regions inside `_LaunchClaude` were modified:

### Region 1 — Registry-read block (lines ~333-383)

Expanded the local declaration block and appended six new RegGetValueW calls after the existing four reads:

- Added locals: `dwContinue`, `dwResume`, `dwDangerSkip`, `dwAllowDangerSkip`, `szRemoteControlPrefix[1024]`, `szChannels[8192]`
- Added four DWORD reads (Continue, Resume, DangerouslySkipPermissions, AllowDangerouslySkipPermissions) using `RRF_RT_REG_DWORD | RRF_ZEROONFAILURE`
- Added two REG_SZ reads (RemoteControlPrefix, Channels) using `RRF_RT_REG_SZ | RRF_ZEROONFAILURE`
- Registry key names match `MainWindow.xaml.cs` lines 123-128 verbatim

### Region 2 — Build-flags block (lines ~385-456)

Extended the `// --- Build flags string ---` section. `szFlags[4096]` became `szFlags[16384]`. The final D-08 append order as it now appears in source:

1. `--model <value>` (if szModel[0])
2. `--verbose` (if dwVerbose)
3. `-c` (if dwContinue)  **NEW**
4. `-r` (if dwResume)  **NEW**
5. `--dangerously-skip-permissions` (if dwDangerSkip)  **NEW**
6. `--allow-dangerously-skip-permissions` (if dwAllowDangerSkip)  **NEW**
7. `--remote-control-session-name-prefix <value>` (if szRemoteControlPrefix[0])  **NEW**
8. `--allowedTools <value>` (if szAllowedTools[0]) — **MOVED DOWN** from position 3
9. `--channels <entry>` repeated per surviving channel (if szChannels[0])  **NEW**
10. `<szExtraFlags raw>` (if szExtraFlags[0]) — **still last**, user escape hatch per D-08

The channel splitter uses `wcstok_s(szChannels, L"|", &context)`, trims leading/trailing spaces and tabs per entry, skips entries whose trimmed length is zero (handles trailing `|` and `||`), and stops at `channelCount < 32`.

## Confirmations

- **Buffer bump:** `WCHAR szFlags[16384]` present exactly once; `szFlags[4096]` absent from the file.
- **Channel cap:** Loop guarded by `channelCount < 32`, counter incremented only on non-empty emissions (so 32 channels are *always emitted* even when the raw list contains empty entries).
- **No other files modified:** `git diff --stat` for this plan shows only `src/ClaudeFromHere.cpp`.
- **No new headers needed:** `wcstok_s` is provided by `<string.h>` which is transitively included via `<windows.h>`; build expected to succeed with existing include set.
- **No debug logging added** (per D-14).
- **No quoting or sanitization added** (per D-05, D-06) — raw append mirrors existing Model/AllowedTools/ExtraFlags pattern.

## Task Commits

1. **Task 1: Bump szFlags buffer to 16384 and add six new registry reads** — `a3f0023` (feat)
2. **Task 2: Append new flags in D-08 order with channel splitter** — `10baaac` (feat)

## Files Created/Modified
- `src/ClaudeFromHere.cpp` — Added six RegGetValueW reads, six guarded flag-append blocks, inline channel splitter, and bumped szFlags from 4096 to 16384 WCHARs. All changes localized to `_LaunchClaude`.

## Decisions Made
- Inlined the channel splitter rather than extracting a helper function, matching the existing inline style of Model/AllowedTools/ExtraFlags handling. Helper extraction was listed under "Claude's Discretion" in 06-CONTEXT.md.
- Used `wcstok_s` for pipe splitting (CRT-provided, no extra dependency, no dynamic allocation).
- Kept the six registry reads textually adjacent to the existing four reads rather than interleaving — easier to visually verify against the MainWindow.xaml.cs writer block.

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required. The DLL needs to be rebuilt and reinstalled; that is scoped to plan 06-02.

## Next Phase Readiness

- Plan 06-02 (rebuild and verify) is ready to run. Inputs required: clean CMake build of the DLL, reinstall, and manual right-click "all off" verification plus a "full load" verification with all six new keys populated.
- Phase 5 Settings app already writes the exact key names the DLL now reads; no cross-boundary changes required.

## Self-Check: PASSED

- `src/ClaudeFromHere.cpp`: FOUND
- Commit `a3f0023`: FOUND
- Commit `10baaac`: FOUND
- `WCHAR szFlags[16384]` exactly once: FOUND
- `szFlags[4096]` absent: CONFIRMED
- 6 new RegGetValueW calls with matching key names: FOUND
- 6 new guarded flag-append blocks in D-08 order: FOUND
- `wcstok_s` >= 2 occurrences: FOUND (2)
- `channelCount < 32` hard cap: FOUND
- No files outside `src/ClaudeFromHere.cpp` modified for this plan: CONFIRMED

---
*Phase: 06-dll-integration*
*Completed: 2026-04-10*
