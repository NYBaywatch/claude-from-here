---
gsd_state_version: 1.0
milestone: v1.1.0
milestone_name: Enhanced Settings
status: roadmap_ready
stopped_at: ""
last_updated: "2026-04-10"
last_activity: 2026-04-10
progress:
  total_phases: 2
  completed_phases: 0
  total_plans: 0
  completed_plans: 0
  percent: 0
---

# Project State

## Project Reference

See: .planning/PROJECT.md (updated 2026-04-10)

**Core value:** Right-click any folder in Windows 11 Explorer -> Claude Code opens in that directory. One click, no terminal juggling.
**Current focus:** Milestone v1.1.0 — Enhanced Settings (Phase 5: Enhanced Settings UI)

## Current Position

Phase: Not started (roadmap ready)
Plan: —
Status: Ready for Phase 5
Last activity: 2026-04-10 — v1.1.0 roadmap created (2 phases, 11 requirements mapped)

## Progress Bar

[----------] 0% — 0/2 phases complete

## Accumulated Context

### Decisions

Carried from v1.0.0:
- Settings app is WPF/.NET 8, stores config in registry at HKCU:\Software\ClaudeFromHere
- C++ DLL reads registry at invoke time to build CLI arguments
- Self-signed cert fallback for local dev builds (added during Phase 4 debug)
- Azure Trusted Signing via OIDC for CI builds (agrussigning/agrus-public profile)

v1.1.0 decisions:
- CFG-03 and CFG-04 (dangerous permission flags) get red/warning visual treatment in WPF UI
- Phase 5 covers all Settings UI work including INT-02 (persistence); Phase 6 covers DLL reads (INT-01) and multi-channel flag passing (CHN-04)
- Channel entries stored as a delimited registry value under HKCU:\Software\ClaudeFromHere

### Pending Todos

None yet.

### Blockers/Concerns

None yet.

## Session Continuity

Last session: 2026-04-10
Stopped at: —
Resume file: None
