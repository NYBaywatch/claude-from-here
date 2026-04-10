---
phase: 03-installer
plan: 01
subsystem: infra
tags: [inno-setup, msix, azure-trusted-signing, installer, powershell]

# Dependency graph
requires:
  - phase: 01-foundation
    provides: COM DLL, stub EXE, AppxManifest, MSIX sparse package architecture
  - phase: 02-launcher-and-config
    provides: ClaudeFromHereConfig.exe, registry settings pattern
provides:
  - Inno Setup installer script (installer/ClaudeFromHere.iss)
  - Build orchestration script (build-installer.ps1)
  - Full install/upgrade/uninstall lifecycle for ClaudeFromHere
affects: [03-installer]

# Tech tracking
tech-stack:
  added: [inno-setup-6.7, azure-trusted-signing]
  patterns: [per-user-install, msix-sparse-registration, upgrade-dll-lock-prevention]

key-files:
  created:
    - installer/ClaudeFromHere.iss
    - build-installer.ps1
  modified: []

key-decisions:
  - "AppId GUID 648d3dc8-04a3-461b-b4f8-23753c3ffa60 chosen for Inno Setup upgrade tracking (separate from COM CLSID)"
  - "OutputDir set to dist/ at project root for clean separation of build artifacts from release artifacts"
  - "Azure Trusted Signing env vars use CFH_ prefix (CFH_SIGNING_ENDPOINT, CFH_SIGNING_ACCOUNT, CFH_SIGNING_PROFILE)"

patterns-established:
  - "Per-user install pattern: PrivilegesRequired=lowest + {localappdata} install dir"
  - "Upgrade DLL lock prevention: CurStepChanged(ssInstall) unregisters MSIX and kills Explorer before file copy"
  - "Dual Explorer restart entries: skipifsilent+postinstall for interactive, skipifnotsilent for silent mode"
  - "Build pipeline: CMake -> dotnet -> assets -> MakeAppx -> sign -> ISCC"

requirements-completed: [INST-01, INST-02, INST-03, INST-04]

# Metrics
duration: 3min
completed: 2026-04-10
---

# Phase 3 Plan 1: Installer Infrastructure Summary

**Inno Setup installer script and build orchestration for per-user deployment with MSIX registration, upgrade DLL lock prevention, and Azure Trusted Signing**

## Performance

- **Duration:** 3 min
- **Started:** 2026-04-10T16:27:49Z
- **Completed:** 2026-04-10T16:30:21Z
- **Tasks:** 2
- **Files created:** 2

## Accomplishments
- Complete Inno Setup script handling install, upgrade, and uninstall with MSIX sparse package registration
- Build orchestration script with 6-step pipeline: compile DLL, build config app, copy assets, pack MSIX, sign with Azure Trusted Signing, invoke ISCC
- Upgrade path that unregisters old MSIX and kills Explorer before file copy to prevent DLL lock errors
- All 11 locked decisions (D-01 through D-11) implemented

## Task Commits

Each task was committed atomically:

1. **Task 1: Create Inno Setup installer script** - `088d5f9` (feat)
2. **Task 2: Create build-installer.ps1 orchestration script** - `14dd0f8` (feat)

## Files Created/Modified
- `installer/ClaudeFromHere.iss` - Inno Setup script with [Setup], [Files], [Icons], [Run], [UninstallRun], and [Code] sections
- `build-installer.ps1` - 6-step build pipeline with SkipBuild and SkipSign switches

## Decisions Made
- AppId GUID `{648d3dc8-04a3-461b-b4f8-23753c3ffa60}` assigned for Inno Setup upgrade tracking (distinct from COM CLSID)
- Installer output goes to `dist/ClaudeFromHere-Setup.exe` via `OutputDir=..\dist` in ISS (per Task 2 note to update Task 1)
- CFH_ prefix for Azure Trusted Signing env vars to avoid collision with scanner project's AGRUS_ prefix
- Build script gracefully skips signing when env vars are not set (warning, not error) for dev builds

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

None - no external service configuration required. Azure Trusted Signing env vars are documented in the build script header and are only needed at build time, not at install time.

## Next Phase Readiness
- Both artifacts ready for Plan 2 (if applicable) or end-to-end testing
- Inno Setup must be installed on the build machine before ISCC can compile the .iss
- Azure Trusted Signing env vars must be configured before signed builds
- AppxManifest Publisher CN may need updating to match Azure Trusted Signing certificate Subject

---
*Phase: 03-installer*
*Completed: 2026-04-10*
