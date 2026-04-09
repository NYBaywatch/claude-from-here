---
phase: 01-foundation
plan: 02
subsystem: infra
tags: [powershell, cmake, msix, signtool, makeappx, com, windows-shell, build-workflow]

# Dependency graph
requires: [01-01]
provides:
  - PowerShell register.ps1 script: full dev build-sign-register workflow
  - PowerShell unregister.ps1 script: clean teardown
  - Compiled ClaudeFromHere.dll (COM IExplorerCommand)
  - Compiled ClaudeFromHere.exe (stub for AppxManifest Executable attribute)
  - Signed ClaudeFromHere.msix sparse package
  - Registered AppxPackage ClaudeFromHere_1.0.0.0_x64 on developer machine
affects: [03-installer, 04-distribution]

# Tech tracking
tech-stack:
  added: [PowerShell 5.1+, New-SelfSignedCertificate, Add-AppxPackage -ExternalLocation, MakeAppx /nv, SignTool SHA256]
  patterns:
    - CMake VS generator builds to Release/ subfolder; register.ps1 copies to build/ root for ExternalLocation
    - Cert import to LocalMachine\TrustedPeople via UAC-elevated subprocess when running unelevated
    - Idempotent registration: check/remove existing package before Add-AppxPackage
    - AppxManifest GUID format: no curly braces (validated by MakeAppx schema)
    - WIN32 stub exe required to satisfy AppxManifest Application Executable .exe constraint
    - /MANIFEST:NO linker flag prevents duplicate manifest when .rc embeds RT_MANIFEST

key-files:
  created:
    - scripts/register.ps1
    - scripts/unregister.ps1
    - src/stub_main.cpp
    - build/ClaudeFromHere.dll (generated, not tracked)
    - build/ClaudeFromHere.msix (generated, not tracked)
  modified:
    - CMakeLists.txt
    - package/AppxManifest.xml
    - src/dllmain.cpp

key-decisions:
  - "AppxManifest Application Executable must be a .exe, not a .dll -- added WIN32 stub exe target to CMakeLists.txt built alongside the DLL"
  - "AppxManifest GUID attributes (com:Class Id, desktop5:Verb Clsid) require no curly braces; CLSID constant in dllmain.cpp still uses braces for C++ registry -- two different formats for same GUID"
  - "/MANIFEST:NO linker flag required when .rc embeds RT_MANIFEST to prevent CVT1100 duplicate resource linker error"
  - "CLSID_ClaudeFromHere declared extern const (not __declspec(selectany)) since it is defined in exactly one translation unit (dllmain.cpp)"

requirements-completed: [MENU-01, MENU-02, MENU-03]

# Metrics
duration: ~18min
completed: 2026-04-09
---

# Phase 01 Plan 02: PowerShell dev scripts, build workflow, and MSIX registration for ClaudeFromHere shell extension

**Full dev workflow: register.ps1 automates CMake build, self-signed cert generation with UAC elevation for LocalMachine import, MakeAppx MSIX packing, SignTool SHA256 signing, Add-AppxPackage -ExternalLocation registration, and Explorer restart**

## Performance

- **Duration:** ~18 min
- **Started:** 2026-04-09T18:34:00Z
- **Completed:** 2026-04-09T18:52:00Z (Tasks 1-2; Task 3 pending human verification)
- **Tasks:** 2 of 3 complete (Task 3 is checkpoint:human-verify)
- **Files modified:** 7

## Accomplishments

- `scripts/register.ps1` (340 lines): complete dev build-sign-register workflow with cert reuse logic, idempotent package removal, and UAC elevation for cert import
- `scripts/unregister.ps1` (106 lines): clean teardown with optional cert removal from both certificate stores
- C++ compilation fixes: `__declspec(selectany)` -> `extern const`, added `#include <new>` for `std::nothrow`
- CMake fix: `/MANIFEST:NO` linker flag eliminates duplicate manifest conflict between `.rc` embedded RT_MANIFEST and auto-generated linker manifest
- Stub exe: `src/stub_main.cpp` WIN32 target satisfies AppxManifest `Application Executable` `.exe` constraint
- AppxManifest GUID fix: removed curly braces from `com:Class Id` and `desktop5:Verb Clsid` attributes (MakeAppx schema validates regex `[0-9a-fA-F]{8}-...`)
- Package successfully registered: `ClaudeFromHere_1.0.0.0_x64__43dw3j1hc5yby` at `C:\Program Files\WindowsApps\...`

## Task Commits

1. **Task 1: Create PowerShell register and unregister scripts** - `dac5ed4` (feat)
2. **Task 2: Build DLL and register package** - `7d91a3a` (feat)
3. **Task 3: Verify context menu in Explorer** - PENDING (checkpoint:human-verify)

## Files Created/Modified

- `scripts/register.ps1` - 340-line dev workflow: build (CMake), cert, pack (MakeAppx), sign (SignTool), register (Add-AppxPackage), restart Explorer
- `scripts/unregister.ps1` - Clean teardown: Remove-AppxPackage, optional cert removal, Explorer restart
- `src/stub_main.cpp` - Minimal WIN32 stub exe required by AppxManifest Executable attribute
- `CMakeLists.txt` - Added WIN32 stub exe target + /MANIFEST:NO linker flag
- `package/AppxManifest.xml` - Fixed: Executable="ClaudeFromHere.exe", GUIDs without braces
- `src/dllmain.cpp` - Fixed: #include <new>, extern const CLSID (was __declspec(selectany))

## Decisions Made

- AppxManifest `Application Executable` must be a `.exe` file (MakeAppx schema constraint). Since the shell extension is a pure COM surrogate DLL, a minimal WIN32 stub exe was added to CMakeLists.txt (`ClaudeFromHereStub` target, output name `ClaudeFromHere`)
- AppxManifest GUID attributes (`com:Class Id`, `desktop5:Verb Clsid`) must omit curly braces; the pattern constraint is `[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-...`
- `/MANIFEST:NO` linker option added for the DLL target to avoid `CVT1100: duplicate resource (MANIFEST, ID=2)` when the `.rc` file already embeds RT_MANIFEST
- `extern const CLSID CLSID_ClaudeFromHere` is the correct form for a symbol defined in one translation unit; `__declspec(selectany)` is only valid on data items with external linkage across multiple TUs in headers

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 1 - Bug] PowerShell `$Args` reserved variable shadowing**
- **Found during:** Task 2 (first run of register.ps1)
- **Issue:** `Invoke-ExternalTool` function parameter named `$Args` which is a PowerShell automatic variable — the function received empty args, causing cmake to print usage
- **Fix:** Renamed parameter to `$ToolArgs`
- **Files modified:** scripts/register.ps1
- **Committed in:** 7d91a3a (Task 2 commit)

**2. [Rule 1 - Bug] C++ compile error: `__declspec(selectany)` on const with internal linkage**
- **Found during:** Task 2 (CMake build)
- **Issue:** MSVC error C2496: `selectany` requires external linkage; `const` at namespace scope has internal linkage by default
- **Fix:** Changed to `extern const CLSID CLSID_ClaudeFromHere =` in dllmain.cpp
- **Files modified:** src/dllmain.cpp
- **Committed in:** 7d91a3a (Task 2 commit)

**3. [Rule 1 - Bug] C++ compile error: `std::nothrow` not declared**
- **Found during:** Task 2 (CMake build)
- **Issue:** MSVC error C2039: `nothrow` not a member of `std` because `<new>` was not included in dllmain.cpp
- **Fix:** Added `#include <new>` to dllmain.cpp
- **Files modified:** src/dllmain.cpp
- **Committed in:** 7d91a3a (Task 2 commit)

**4. [Rule 1 - Bug] Linker error: duplicate manifest resource CVT1100**
- **Found during:** Task 2 (CMake build after initial fixes)
- **Issue:** MSVC linker auto-generates a manifest (ID 2) which conflicts with the RT_MANIFEST already embedded via ClaudeFromHere.rc
- **Fix:** Added `target_link_options(ClaudeFromHere PRIVATE /MANIFEST:NO)` to CMakeLists.txt
- **Files modified:** CMakeLists.txt
- **Committed in:** 7d91a3a (Task 2 commit)

**5. [Rule 2 - Missing Critical] Stub exe required for AppxManifest Executable attribute**
- **Found during:** Task 2 (MakeAppx pack)
- **Issue:** AppxManifest schema requires `Application Executable` to match `*.exe` pattern; original manifest had `ClaudeFromHere.dll` which fails MakeAppx validation even with `/nv`
- **Fix:** Added WIN32 `stub_main.cpp` + CMake target `ClaudeFromHereStub` (output: ClaudeFromHere.exe); updated AppxManifest Executable to `ClaudeFromHere.exe`
- **Files modified:** CMakeLists.txt, package/AppxManifest.xml, src/stub_main.cpp (new)
- **Committed in:** 7d91a3a (Task 2 commit)

**6. [Rule 1 - Bug] AppxManifest GUID format: curly braces rejected by MakeAppx schema**
- **Found during:** Task 2 (MakeAppx pack)
- **Issue:** `com:Class Id` and `desktop5:Verb Clsid` attributes use regex pattern without braces; MakeAppx validation fails on `{b2dd8803-...}` format
- **Fix:** Removed curly braces from all GUID attribute values in AppxManifest.xml (3 occurrences)
- **Files modified:** package/AppxManifest.xml
- **Committed in:** 7d91a3a (Task 2 commit)

---

**Total deviations:** 6 auto-fixed (4 bugs, 2 missing critical)
**Impact:** All were blockers preventing the build from completing. No scope creep.

## Known Stubs

None — scripts are complete implementations. The stub exe (`ClaudeFromHere.exe`) is intentional: it exists solely to satisfy AppxManifest schema, is never invoked, and is documented as such in the source.

## Next Phase Readiness

- `register.ps1` and `unregister.ps1` are the primary dev workflow tools for Phase 3 (installer)
- The Inno Setup installer (Phase 3) will replicate the register.ps1 steps: cert import (elevated), MSIX registration, Explorer restart
- Patterns discovered here (GUID without braces, /MANIFEST:NO, stub exe) must be documented for the installer script
- Task 3 (human verification) is pending — context menu appearance in Windows 11 Explorer must be confirmed before Phase 1 is marked complete

---

*Phase: 01-foundation*
*Completed: 2026-04-09 (Tasks 1-2; Task 3 pending)*
