---
phase: 02-launcher-and-config
plan: 01
subsystem: launcher
tags: [win32, registry, advapi32, RegGetValueW, SearchPathW, IExplorerCommand, cpp]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: ClaudeFromHere.cpp with _LaunchClaude, CMakeLists.txt DLL build
provides:
  - Multi-stage wt.exe and claude.exe path detection (SearchPathW -> HKCU App Paths -> HKLM App Paths -> execution alias)
  - Registry flag reads from HKCU\Software\ClaudeFromHere (Model, Verbose, AllowedTools, ExtraFlags)
  - Actionable error dialogs with install instructions per UI-SPEC Surface 2
  - CreateProcessW with lpApplicationName=szWt for space-safe launch
affects: [03-installer, 02-02]

# Tech tracking
tech-stack:
  added: [advapi32 (RegGetValueW for registry reads)]
  patterns:
    - FindExecutable static helper pattern (3-stage chain + Stage 4 alias for wt.exe)
    - RRF_ZEROONFAILURE for graceful missing registry key handling
    - StringCbCatW (byte-based) for safe flag string assembly

key-files:
  created: []
  modified:
    - src/ClaudeFromHere.cpp
    - CMakeLists.txt

key-decisions:
  - "advapi32 added to CMakeLists.txt target_link_libraries for RegGetValueW"
  - "FindExecutable checks HKCU App Paths before HKLM (Store-installed wt.exe registers per-user in HKCU)"
  - "Stage 4 execution alias fallback for wt.exe only via ExpandEnvironmentStringsW"
  - "CreateProcessW uses lpApplicationName=szWt to handle spaces in WindowsApps path"
  - "RRF_ZEROONFAILURE used so missing registry key/values silently produce empty buffers"

patterns-established:
  - "Pattern: Multi-stage exe detection via FindExecutable + Stage 4 alias for wt.exe"
  - "Pattern: RegGetValueW with RRF_ZEROONFAILURE for optional registry config reads"
  - "Pattern: StringCbCatW byte-based flag assembly consistent with Phase 1 strsafe usage"

requirements-completed: [LNCH-01, LNCH-02, LNCH-03, LNCH-04, LNCH-05]

# Metrics
duration: 8min
completed: 2026-04-10
---

# Phase 02 Plan 01: Launcher and Config - Executable Detection and Registry Flags Summary

**4-stage wt.exe path detection + HKCU registry flag reads (Model/Verbose/AllowedTools/ExtraFlags) + space-safe CreateProcessW in the C++ COM DLL**

## Performance

- **Duration:** ~8 min
- **Started:** 2026-04-10T09:09:14Z
- **Completed:** 2026-04-10T09:11:50Z
- **Tasks:** 1
- **Files modified:** 2

## Accomplishments

- Replaced `SearchPathW`-only approach with `FindExecutable` static helper implementing 3-stage chain (SearchPathW -> HKCU App Paths -> HKLM App Paths) plus Stage 4 execution alias fallback for wt.exe
- Added registry reads at Invoke time from `HKCU\Software\ClaudeFromHere` for Model, Verbose, AllowedTools, ExtraFlags using `RegGetValueW` with `RRF_ZEROONFAILURE`
- Improved error dialogs with install instructions: `npm i -g @anthropic-ai/claude-code` for claude.exe, `winget install Microsoft.WindowsTerminal` for wt.exe
- Fixed `CreateProcessW` to use `lpApplicationName=szWt` so Store-installed wt.exe paths with spaces work correctly

## Task Commits

1. **Task 1: Add advapi32, FindExecutable, registry flags, improved error dialogs** - `4dff6c6` (feat)

**Plan metadata:** (pending)

## Files Created/Modified

- `src/ClaudeFromHere.cpp` - Rewrote `_LaunchClaude`: added `FindExecutable` static helper, Stage 4 wt.exe alias fallback, registry flag reads, flag string assembly, updated error dialogs, `lpApplicationName=szWt`
- `CMakeLists.txt` - Added `advapi32` to DLL `target_link_libraries`

## Decisions Made

- HKCU App Paths checked before HKLM for wt.exe — Windows Terminal Store installs register per-user in HKCU (confirmed during Phase 2 research)
- Stage 4 execution alias (`%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe`) applied only to wt.exe — no equivalent exists for claude.exe
- `RRF_ZEROONFAILURE` chosen for all `RegGetValueW` calls so absent registry key yields empty buffer without error checking code
- `lpApplicationName=szWt` required because `C:\Program Files\WindowsApps\...` path contains spaces that would cause `CreateProcessW` to misparse the executable name

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None - CMake build succeeded on first attempt with zero errors and zero LNK2019 warnings. `build/Release/ClaudeFromHere.dll` produced successfully.

## User Setup Required

None - no external service configuration required.

## Known Stubs

None - all code paths are fully wired. Registry reads silently produce empty values when key is absent (desired behavior per D-07).

## Next Phase Readiness

- DLL now ready for Plan 02 (WinForms config app) which writes `HKCU\Software\ClaudeFromHere` values that this DLL will read
- DLL compiles cleanly with `advapi32` linked; no further CMakeLists changes needed for registry functionality
- Error dialogs and path detection fully functional for end-user testing in Phase 3 installer

---
*Phase: 02-launcher-and-config*
*Completed: 2026-04-10*

## Self-Check: PASSED

- FOUND: `D:\Working\Projects\claude-from-here\src\ClaudeFromHere.cpp`
- FOUND: `D:\Working\Projects\claude-from-here\CMakeLists.txt`
- FOUND: `D:\Working\Projects\claude-from-here\build\Release\ClaudeFromHere.dll`
- FOUND commit: `4dff6c6`
