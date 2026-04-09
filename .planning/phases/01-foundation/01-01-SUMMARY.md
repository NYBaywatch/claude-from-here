---
phase: 01-foundation
plan: 01
subsystem: infra
tags: [cpp, com, windows-shell, msix, cmake, iexplorercommand, iobjectwithsite, win11]

# Dependency graph
requires: []
provides:
  - C++ COM DLL source (IExplorerCommand + IObjectWithSite) implementing "Claude from here" context menu handler
  - CMake build configuration targeting MSVC/Win64
  - Sparse MSIX AppxManifest.xml wiring CLSID to Directory and Directory\Background item types
  - Multi-size Claude sparkle icon (16x16, 32x32, 48x48) in ICO format
  - DLL exports (.def), resource script (.rc), side-by-side manifest (.manifest)
affects: [02-scripts, 03-installer, 04-distribution]

# Tech tracking
tech-stack:
  added: [C++17, CMake 3.28+, Windows SDK 10.0.26100.0, MSVC 14.44, IExplorerCommand, IObjectWithSite, sparse MSIX, COM surrogate server, Pillow (icon generation)]
  patterns:
    - IExplorerCommand with IObjectWithSite for both folder and background right-click
    - PATH-based executable discovery (SearchPathW) with MessageBoxW error fallback
    - GetModuleFileNameW-based icon path derivation (relative to DLL location)
    - Sparse MSIX manifest with desktop4/desktop5 schema for Windows 11 modern menu
    - COM factory pattern via CClassFactory + CreateClaudeFromHereInstance factory function

key-files:
  created:
    - CMakeLists.txt
    - src/ClaudeFromHere.cpp
    - src/dllmain.cpp
    - src/ClaudeFromHere.def
    - src/ClaudeFromHere.rc
    - src/ClaudeFromHere.manifest
    - package/AppxManifest.xml
    - package/Assets/StoreLogo.png
    - assets/claude.ico
  modified: []

key-decisions:
  - "CLSID {b2dd8803-e848-41d5-bb0b-598086308dcf} generated via PowerShell [System.Guid]::NewGuid() and used consistently across AppxManifest.xml and dllmain.cpp"
  - "IObjectWithSite traversal chain implemented for Directory\\Background: IServiceProvider -> SID_STopLevelBrowser -> IShellBrowser -> QueryActiveShellView -> IFolderView -> GetFolder"
  - "SearchPathW checks for wt.exe and claude.exe at invoke time per D-02; MessageBoxW error dialogs for missing executables"
  - "Launch command: wt.exe -d \"<path>\" cmd /k claude per D-01"
  - "Icon uses Claude coral/orange brand color (#E07A5F) sparkle/asterisk on transparent background per D-03/D-04"
  - "Side-by-side manifest embedded as RT_MANIFEST resource (ID 2) to satisfy Explorer DLL loading requirement"

patterns-established:
  - "Pattern 1: DLL globals (g_hModule, g_cDllRef) in dllmain.cpp, extern in ClaudeFromHere.cpp"
  - "Pattern 2: Factory function CreateClaudeFromHereInstance() decouples dllmain.cpp from ClaudeFromHere.cpp"
  - "Pattern 3: Two-path Invoke — psiItemArray path for folder right-click, _GetFolderPathFromSite for background right-click"

requirements-completed: [MENU-01, MENU-02, MENU-03]

# Metrics
duration: 25min
completed: 2026-04-09
---

# Phase 01 Plan 01: C++ COM DLL source, CMake build, and sparse MSIX manifest for Windows 11 context menu extension

**C++ COM DLL implementing IExplorerCommand + IObjectWithSite with CMake/MSVC build, sparse MSIX manifest wiring CLSID {b2dd8803} to both Directory and Directory\\Background, and multi-size Claude sparkle icon**

## Performance

- **Duration:** ~25 min
- **Started:** 2026-04-09T14:39:29Z
- **Completed:** 2026-04-09T15:04:00Z
- **Tasks:** 3
- **Files modified:** 9

## Accomplishments

- Complete C++ COM DLL source: `CClaudeFromHere` implementing `IExplorerCommand` and `IObjectWithSite`, covering both folder right-click and folder-background right-click launch paths
- Sparse MSIX `AppxManifest.xml` with `desktop4`/`desktop5` schema wiring the generated CLSID to `Directory` and `Directory\Background` item types, enabling Windows 11 top-level context menu placement
- Multi-size Claude sparkle icon (16x16, 32x32, 48x48) as valid ICO file with coral/orange brand color (#E07A5F) on transparent background, visible in both light and dark Explorer themes

## Task Commits

Each task was committed atomically:

1. **Task 1: Create build infrastructure and manifest** - `62b8f05` (feat)
2. **Task 2: Implement IExplorerCommand COM DLL** - `1d2ade8` (feat)
3. **Task 3: Create Claude icon asset** - `3b966af` (feat)

## Files Created/Modified

- `CMakeLists.txt` - CMake build config for ClaudeFromHere SHARED DLL targeting build/ output dir
- `src/ClaudeFromHere.cpp` - CClaudeFromHere: IExplorerCommand + IObjectWithSite, PATH discovery, CreateProcessW launch
- `src/dllmain.cpp` - DllMain, CClassFactory, DllGetClassObject, DllCanUnloadNow, CLSID definition
- `src/ClaudeFromHere.def` - DLL exports: DllGetClassObject PRIVATE, DllCanUnloadNow PRIVATE
- `src/ClaudeFromHere.rc` - Resource script: IDI_CLAUDE ICON, RT_MANIFEST embedding
- `src/ClaudeFromHere.manifest` - Side-by-side manifest with msix element (publisher=CN=ClaudeFromHere)
- `package/AppxManifest.xml` - Sparse MSIX manifest with COM surrogate server and two desktop5:ItemType entries
- `package/Assets/StoreLogo.png` - 50x50 orange placeholder PNG required by manifest VisualElements
- `assets/claude.ico` - Multi-size sparkle icon (16, 32, 48px) in ICO format

## Decisions Made

- CLSID `{b2dd8803-e848-41d5-bb0b-598086308dcf}` generated fresh via PowerShell and used consistently in AppxManifest.xml (3 occurrences) and dllmain.cpp CLSID constant
- IObjectWithSite traversal chain implemented as `_GetFolderPathFromSite`: IServiceProvider -> SID_STopLevelBrowser -> IShellBrowser -> QueryActiveShellView -> IFolderView -> GetFolder -> GetDisplayName
- Factory function pattern: `CreateClaudeFromHereInstance()` in ClaudeFromHere.cpp called by CClassFactory::CreateInstance in dllmain.cpp — decouples the two translation units cleanly
- `strsafe.h` included in ClaudeFromHere.cpp for `StringCbPrintfW` safe string formatting of launch command line

## Deviations from Plan

### Auto-fixed Issues

**1. [Rule 2 - Missing Critical] Added strsafe.h include for safe string formatting**
- **Found during:** Task 2 (IExplorerCommand implementation)
- **Issue:** Used `StringCbPrintfW` for building the wt.exe command line but plan did not specify including `strsafe.h`
- **Fix:** Added `#include <strsafe.h>` to ClaudeFromHere.cpp
- **Files modified:** src/ClaudeFromHere.cpp
- **Committed in:** 1d2ade8 (Task 2 commit)

---

**Total deviations:** 1 auto-fixed (1 missing critical include)
**Impact on plan:** Necessary for safe buffer-limited string formatting of the command line. No scope creep.

## Issues Encountered

- Pillow's `Image.save(format='ICO', sizes=[...])` only embedded the first size when using `append_images`. Resolved by manually building the ICO binary format (ICONDIR + ICONDIRENTRY headers + PNG data chunks) which correctly embeds all three sizes.

## User Setup Required

None — no external service configuration required. Build environment (MSVC, CMake, Windows SDK) is already confirmed on the machine per RESEARCH.md.

## Known Stubs

None — all source files are complete implementations, not placeholders. The icon is a functional multi-size ICO. The DLL source implements the full launch path.

## Next Phase Readiness

- All source files are ready to compile with `cmake -G "Visual Studio 17 2022" -A x64 .. && cmake --build .`
- Phase 02 (scripts) can now create `scripts/register.ps1` and `scripts/unregister.ps1` using the CLSID `{b2dd8803-e848-41d5-bb0b-598086308dcf}` from AppxManifest.xml
- The DLL, AppxManifest.xml, and icon must all land in the same directory (build/) for GetIcon path derivation to work
- Self-signed cert import to LocalMachine\\TrustedPeople is still required before MSIX registration (handled in Phase 02 scripts per D-07/D-08)

---
*Phase: 01-foundation*
*Completed: 2026-04-09*
