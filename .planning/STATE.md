---
gsd_state_version: 1.0
milestone: v1.0
milestone_name: milestone
status: executing
stopped_at: Completed 02-01-PLAN.md
last_updated: "2026-04-10T09:12:45.909Z"
last_activity: 2026-04-10
progress:
  total_phases: 4
  completed_phases: 1
  total_plans: 5
  completed_plans: 3
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-08)

**Core value:** Right-click any folder in Windows 11 Explorer -> Claude Code opens in that directory. One click, no terminal juggling.
**Current focus:** Phase 02 — launcher-and-config

## Current Position

Phase: 02 (launcher-and-config) — EXECUTING
Plan: 2 of 3
Status: Ready to execute
Last activity: 2026-04-10

Progress: [░░░░░░░░░░] 0%

## Performance Metrics

**Velocity:**

- Total plans completed: 0
- Average duration: -
- Total execution time: 0 hours

**By Phase:**

| Phase | Plans | Total | Avg/Plan |
|-------|-------|-------|----------|
| - | - | - | - |

**Recent Trend:**

- Last 5 plans: -
- Trend: -

*Updated after each plan completion*
| Phase 01-foundation P01 | 25 | 3 tasks | 9 files |
| Phase 01-foundation P02 | 18 | 2 tasks | 7 files |
| Phase 02-launcher-and-config P01 | 8 | 1 tasks | 2 files |

## Accumulated Context

### Decisions

Decisions are logged in PROJECT.md Key Decisions table.
Recent decisions affecting current work:

- Sparse MSIX over COM DLL: cleaner build path, but Directory\Background coverage is the Phase 1 unknown to resolve
- Self-signed cert required: machine trust store import needed for end-user installs (Inno Setup handles this in Phase 3)
- [Phase 01-foundation]: CLSID {b2dd8803-e848-41d5-bb0b-598086308dcf} generated and used consistently across AppxManifest.xml and dllmain.cpp
- [Phase 01-foundation]: IObjectWithSite traversal chain for Directory\Background: IServiceProvider -> SID_STopLevelBrowser -> IShellBrowser -> IFolderView -> GetFolder
- [Phase 01-foundation]: SearchPathW checks wt.exe and claude.exe at invoke time; MessageBoxW error dialogs for missing executables (D-02)
- [Phase 01-foundation]: AppxManifest GUID format: no curly braces in com:Class Id and desktop5:Verb Clsid attributes (MakeAppx schema validation)
- [Phase 01-foundation]: /MANIFEST:NO linker flag required when .rc embeds RT_MANIFEST to prevent CVT1100 duplicate resource error
- [Phase 01-foundation]: WIN32 stub exe required by AppxManifest Application Executable .exe constraint; pure COM surrogate DLL cannot be listed as Executable
- [Phase 02-launcher-and-config]: FindExecutable checks HKCU App Paths before HKLM for wt.exe (Store installs are per-user)
- [Phase 02-launcher-and-config]: CreateProcessW uses lpApplicationName=szWt to handle spaces in WindowsApps path

### Pending Todos

None yet.

### Blockers/Concerns

- Phase 1: Directory\Background manifest-only coverage is unconfirmed — proof-of-concept must resolve before Phase 2
- Phase 1: Icon sizing for light/dark mode context menu rendering needs confirmation

## Session Continuity

Last session: 2026-04-10T09:12:45.906Z
Stopped at: Completed 02-01-PLAN.md
Resume file: None
