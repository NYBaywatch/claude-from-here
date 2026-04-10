---
phase: 02-launcher-and-config
plan: 02
subsystem: ui
tags: [winforms, dotnet, net48, registry, csharp, config-app]

# Dependency graph
requires:
  - phase: 02-launcher-and-config plan 01
    provides: Registry contract (HKCU\Software\ClaudeFromHere) that DLL reads and config app writes

provides:
  - ClaudeFromHereConfig.exe WinForms net48 config app
  - Registry write logic for Model, Verbose, AllowedTools, ExtraFlags values
  - Path detection for claude.exe and wt.exe mirroring DLL logic
  - SDK-style .csproj targeting net48 outputting to build/ root

affects: [03-packaging-and-installer, install-step, register.ps1]

# Tech tracking
tech-stack:
  added: [WinForms (.NET Framework 4.8), Microsoft.Win32.Registry (net48 built-in), dotnet SDK net48 targeting]
  patterns: [SDK-style .csproj with AppendTargetFrameworkToOutputPath=false, SystemColors for theme compliance, Color.Red only for error state]

key-files:
  created:
    - src/ClaudeFromHereConfig/ClaudeFromHereConfig.csproj
    - src/ClaudeFromHereConfig/Program.cs
    - src/ClaudeFromHereConfig/MainForm.cs
  modified:
    - .gitignore (added src/*/obj/ and src/*/bin/ patterns)

key-decisions:
  - "AppendTargetFrameworkToOutputPath=false ensures exe lands in build/ not build/net48/"
  - "Model dropdown stores raw model ID in registry; index 0 (default) stores empty string to omit --model flag"
  - "Discard Changes closes without confirmation - no data is deleted, only uncommitted form state is lost"
  - "dotnet obj/ build artifacts added to .gitignore to prevent noise in git status"

patterns-established:
  - "Pattern: net48 WinForms single-file form (no Designer.cs) — all controls initialized inline in InitializeComponent"
  - "Pattern: Path detection chain — PATH env -> HKCU App Paths -> HKLM App Paths -> execution alias (wt.exe only)"
  - "Pattern: Registry read via OpenSubKey (null if absent = silently use defaults); write via CreateSubKey (creates if missing)"

requirements-completed: [CONF-01, CONF-02, CONF-03]

# Metrics
duration: 15min
completed: 2026-04-10
---

# Phase 02 Plan 02: Config App Summary

**WinForms net48 ClaudeFromHereConfig.exe with model dropdown, verbose checkbox, allowed tools and extra flags fields writing to HKCU\Software\ClaudeFromHere, plus live path detection for claude.exe and wt.exe**

## Performance

- **Duration:** ~15 min
- **Started:** 2026-04-10T09:09:14Z
- **Completed:** 2026-04-10T09:13:13Z
- **Tasks:** 2
- **Files modified:** 4

## Accomplishments

- Config app builds to `build/ClaudeFromHereConfig.exe` (not `build/net48/`) via `dotnet build`
- Full WinForms UI with all controls per UI-SPEC Surface 1: model dropdown, verbose checkbox, allowed tools textbox, extra flags textbox, apply/discard buttons
- Registry read on load (loads saved settings) and write on apply (all four values under HKCU\Software\ClaudeFromHere)
- Path Detection GroupBox shows resolved paths for claude.exe and wt.exe with red "Not found" for missing executables
- All colors use SystemColors except Color.Red for the "Not found" error state (per UI-SPEC constraint)

## Task Commits

1. **Task 1: Create .csproj and Program.cs entry point** - `114598c` (feat)
2. **Task 2: Implement MainForm.cs with settings UI, registry read/write, and path detection** - `542a9b5` (feat)
3. **Deviation: Add dotnet obj/ and bin/ to .gitignore** - `dc6ceb7` (chore)

## Files Created/Modified

- `src/ClaudeFromHereConfig/ClaudeFromHereConfig.csproj` - SDK-style net48 WinForms project, output to build/ root
- `src/ClaudeFromHereConfig/Program.cs` - STAThread entry point, Application.Run(new MainForm())
- `src/ClaudeFromHereConfig/MainForm.cs` - Full form: controls, registry read/write, path detection
- `.gitignore` - Added src/*/obj/ and src/*/bin/ dotnet build artifact exclusions

## Decisions Made

- Used `AppendTargetFrameworkToOutputPath=false` to ensure exe lands directly in `build/` alongside the DLL
- Model dropdown index 0 stores empty string in registry (omits `--model` flag); all other indices store the model ID as-is
- Form layout uses inline InitializeComponent with no generated Designer.cs (single-file approach, simpler)
- Added dotnet obj/ directory to .gitignore as a Rule 2 deviation (missing critical: generated build artifacts should not be tracked)

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added dotnet obj/ and bin/ patterns to .gitignore**
- **Found during:** Task 2 (post-build git status check)
- **Issue:** `src/ClaudeFromHereConfig/obj/` appeared as untracked after build; dotnet build artifacts should not be committed
- **Fix:** Added `src/*/obj/` and `src/*/bin/` to .gitignore
- **Files modified:** .gitignore
- **Verification:** git status shows obj/ no longer untracked
- **Committed in:** dc6ceb7

---

**Total deviations:** 1 auto-fixed (1 missing critical)
**Impact on plan:** Minor cleanup only - prevents generated build artifacts from polluting git history. No scope creep.

## Issues Encountered

None — build succeeded on first attempt, all acceptance criteria met.

## Known Stubs

None — all controls are wired to real registry reads/writes and live path detection. No placeholder data.

## Next Phase Readiness

- `build/ClaudeFromHereConfig.exe` ready to be included in the Phase 3 Inno Setup installer
- `register.ps1` Step 0 should be updated to call `dotnet build src/ClaudeFromHereConfig/...` (Phase 3 task)
- Config app and DLL share the same registry contract — both sides are now implemented

---
*Phase: 02-launcher-and-config*
*Completed: 2026-04-10*
