# Claude From Here

## What This Is

A Windows 11 shell extension that adds "Claude from here" to the top-level right-click context menu in File Explorer. When clicked, it opens Claude Code in the selected directory via Windows Terminal. Distributed as a polished installer for public use.

## Core Value

Right-click any folder or folder background in Windows 11 Explorer → Claude Code opens in that directory. One click, no terminal juggling.

## Requirements

### Validated

- [x] "Claude from here" appears in the Windows 11 modern (top-level) context menu when right-clicking a folder — Validated in Phase 1
- [x] "Claude from here" appears when right-clicking the background inside a folder — Validated in Phase 1
- [x] Custom icon displayed next to the menu item — Validated in Phase 1
- [x] Sparse MSIX package approach for Windows 11 modern context menu integration — Validated in Phase 1
- [x] Clicking the menu item launches Claude Code in the target directory via Windows Terminal — Validated in Phase 2
- [x] Auto-detect Claude Code executable path — Validated in Phase 2
- [x] Auto-detect Windows Terminal path — Validated in Phase 2
- [x] Graceful error handling — clear messages if Claude Code or Windows Terminal not found — Validated in Phase 2
- [x] Installer (.exe) that registers everything automatically — Validated in Phase 3
- [x] Uninstaller that cleanly removes all registry entries, MSIX registration, and files — Validated in Phase 3
- [x] Works on various Windows 11 setups (handles different install locations, user profiles) — Validated in Phase 3

- [x] GitHub-ready: README, license, release artifacts — Validated in Phase 4

### Active

- [ ] Extended CLI flag controls in Settings app (continue, resume, skip-permissions, remote-control prefix)
- [ ] Multi-channel plugin support via add/remove list (--channels)

### Out of Scope

- Windows 10 support — the .reg files already handle that, and Win 11 modern menu is the goal
- Desktop right-click — not a common workflow for opening a CLI tool
- Submenu with multiple options — single menu item keeps it simple
- Microsoft Store distribution — too much overhead for a dev tool

## Current Milestone: v1.1.0 Enhanced Settings

**Goal:** Expand the Settings app with additional CLI flags and multi-channel plugin support.

**Target features:**
- Checkboxes for `-c` (continue), `-r` (resume)
- Checkboxes for `--dangerously-skip-permissions`, `--allow-dangerously-skip-permissions`
- Text input for `--remote-control-session-name-prefix <prefix>`
- Add/remove list for `--channels` entries (e.g. `plugin:telegram@claude-plugins-official`)

## Context

- Prior work exists: working `.reg` file approach that adds the menu item to the classic context menu (appears under "Show more options" on Windows 11)
- Windows 11 introduced a new modern context menu; getting into the top level requires a sparse MSIX package with an AppxManifest.xml declaring the context menu handler
- Claude Code CLI is typically installed at `~\.local\bin\claude.exe` but path should be auto-detected
- Windows Terminal (`wt.exe`) is the launch target — it's standard on Windows 11
- The command pattern is: `wt.exe -d "<directory>" cmd /k claude`
- This is a public/community tool — needs to be robust across different user setups

## Constraints

- **Platform**: Windows 11 only (modern context menu requires Win 11)
- **No compilation dependencies for users**: Installer should be self-contained, users shouldn't need dev tools
- **MSIX signing**: Sparse packages may need self-signing or a workaround for unsigned packages
- **Registry scope**: Use HKEY_CURRENT_USER (no admin required for per-user install if possible)

## Key Decisions

| Decision | Rationale | Outcome |
|----------|-----------|---------|
| Sparse MSIX package over COM shell extension | No DLL compilation, simpler to build and distribute | — Pending |
| Installer (.exe) over PowerShell script | Better UX for public distribution, feels professional | — Pending |
| Windows Terminal as launch target | Standard on Win 11, supports tabs, modern terminal | — Pending |
| Per-user install (no admin) | Lower friction for users, no elevation prompt | — Pending |

## Evolution

This document evolves at phase transitions and milestone boundaries.

**After each phase transition** (via `/gsd:transition`):
1. Requirements invalidated? → Move to Out of Scope with reason
2. Requirements validated? → Move to Validated with phase reference
3. New requirements emerged? → Add to Active
4. Decisions to log? → Add to Key Decisions
5. "What This Is" still accurate? → Update if drifted

**After each milestone** (via `/gsd:complete-milestone`):
1. Full review of all sections
2. Core Value check — still the right priority?
3. Audit Out of Scope — reasons still valid?
4. Update Context with current state

---
*Last updated: 2026-04-10 — Milestone v1.1.0 started*
