# Roadmap: Claude From Here

## Overview

Four phases deliver a working Windows 11 context menu extension from scratch to public release. Phase 1 resolves the critical technical unknown (Directory\Background coverage) and produces a manually-registerable proof-of-concept. Phase 2 wires the verb to a working launcher script with path detection, error handling, and a GUI config app for Claude CLI flags. Phase 3 wraps everything into a polished Inno Setup installer. Phase 4 ships to GitHub with documentation.

Phases 5-6 extend the Settings app (WPF/.NET 8) with additional CLI flags, channel management, and DLL-side registry reads to pass the new flags to Claude Code.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation** - Sparse MSIX manifest, self-signed cert, and proof-of-concept menu registration for both folder and folder-background targets
- [ ] **Phase 2: Launcher and Config** - Working launch script with path detection, error dialogs, and GUI app for Claude CLI flag configuration
- [ ] **Phase 3: Installer** - Inno Setup `.exe` that handles all MSIX registration, cert import, file deployment, upgrade, and uninstall
- [ ] **Phase 4: Distribution** - GitHub release with downloadable installer, README, and clean-machine validation
- [ ] **Phase 5: Enhanced Settings UI** - New CLI flag checkboxes (including danger-styled unsafe flags), remote-control prefix input, and channel add/remove list in the Settings app
- [ ] **Phase 6: DLL Integration** - C++ DLL reads new registry keys and appends the new flags and multiple --channels entries to the Claude Code command line

## Phase Details

### Phase 1: Foundation
**Goal**: A sparse MSIX package is manually registerable and the "Claude from here" menu item appears at the top level of the Windows 11 modern context menu for both folder right-clicks and folder-background right-clicks
**Depends on**: Nothing (first phase)
**Requirements**: MENU-01, MENU-02, MENU-03
**Success Criteria** (what must be TRUE):
  1. Right-clicking a folder shows "Claude from here" in the modern (top-level) context menu, not buried under "Show more options"
  2. Right-clicking inside a folder (background) shows "Claude from here" in the modern context menu
  3. The Claude icon appears next to the menu item text
  4. MSIX package registers successfully via `Add-AppxPackage -ExternalLocation` without errors
**Plans:** 2 plans

Plans:
- [x] 01-01-PLAN.md — C++ COM DLL source code, CMake build system, AppxManifest, and icon asset
- [x] 01-02-PLAN.md — PowerShell dev scripts, build, register, and end-to-end verification

### Phase 2: Launcher and Config
**Goal**: Clicking the menu item opens Windows Terminal in the correct directory running claude, path detection works automatically, errors are surfaced clearly, and users can configure Claude CLI flags via a GUI app
**Depends on**: Phase 1
**Requirements**: LNCH-01, LNCH-02, LNCH-03, LNCH-04, LNCH-05, CONF-01, CONF-02, CONF-03
**Success Criteria** (what must be TRUE):
  1. Clicking the menu item opens Windows Terminal at the right-clicked directory with `claude` running
  2. Path detection finds `claude.exe` and `wt.exe` without any manual configuration
  3. A clear error dialog appears if `claude.exe` is not found at launch time
  4. A clear error dialog appears if `wt.exe` is not found at launch time
  5. User can open a GUI app, configure Claude CLI flags (model, verbose, allowedTools), and settings persist across sessions and apply on every launch
**Plans:** 2/3 plans executed
**UI hint**: yes

Plans:
- [x] 02-01-PLAN.md — DLL enhancement: path detection, registry flag reads, improved error dialogs
- [x] 02-02-PLAN.md — WinForms config app: settings UI, registry read/write, path detection display
- [x] 02-03-PLAN.md — Integration: register.ps1 update and end-to-end verification

### Phase 3: Installer
**Goal**: A single `.exe` installer handles all setup steps (MSIX registration, file deployment) and a clean uninstaller removes everything, with upgrade installs working without manual intervention
**Depends on**: Phase 2
**Requirements**: INST-01, INST-02, INST-03, INST-04
**Success Criteria** (what must be TRUE):
  1. Running the installer once registers the MSIX, imports the signing cert, and deploys all files with no manual steps required
  2. Running the uninstaller removes all MSIX registrations, files, and registry entries cleanly
  3. The installer works on Windows 11 machines with different user profiles and non-standard install locations
  4. Reinstalling over an existing version (upgrade) completes without requiring a manual uninstall first
**Plans:** 2 plans

Plans:
- [x] 03-01-PLAN.md — Inno Setup script and build-installer.ps1 orchestration script
- [x] 03-02-PLAN.md — Build end-to-end, resolve signing CN, and verify installer

### Phase 4: Distribution
**Goal**: The project is publicly available on GitHub with a downloadable installer in Releases and documentation that enables users to install, use, and troubleshoot without assistance
**Depends on**: Phase 3
**Requirements**: DIST-01, DIST-02
**Success Criteria** (what must be TRUE):
  1. The GitHub repository has a published Release with the installer `.exe` as a downloadable artifact
  2. The README documents installation steps, uninstallation steps, and common troubleshooting scenarios
  3. A clean Windows 11 machine can install and use the extension using only the README and the installer from GitHub Releases
**Plans:** 2/3 plans executed

Plans:
- [x] 04-01-PLAN.md — README, LICENSE, and .gitignore for public repo
- [x] 04-02-PLAN.md — GitHub Actions release workflow (CI/CD pipeline)
- [ ] 04-03-PLAN.md — Media assets, repo cleanup, and v1.0.0 release

### Phase 5: Enhanced Settings UI
**Goal**: The Settings app exposes all new CLI flags and channel management so users can configure extended Claude Code behavior without touching the registry manually
**Depends on**: Phase 4
**Requirements**: CFG-01, CFG-02, CFG-03, CFG-04, CFG-05, CHN-01, CHN-02, CHN-03, INT-02
**Success Criteria** (what must be TRUE):
  1. User can check `-c` (continue) and `-r` (resume) checkboxes and see settings saved when the app reopens
  2. User can check `--dangerously-skip-permissions` and `--allow-dangerously-skip-permissions` checkboxes, each visually styled in red with a warning indicator
  3. User can type a prefix into the remote-control session name prefix field and have it persist across app restarts
  4. User can add channels from a preset dropdown (telegram, discord) or via freeform text, see them in a list, and remove individual entries
  5. All new settings are written to registry at HKCU:\Software\ClaudeFromHere using the existing storage pattern
**Plans:** 1/2 plans executed

Plans:
- [x] 05-01-PLAN.md — WPF rewrite: project retarget, dark theme, three-tab UI with all controls, code-behind logic
- [x] 05-02-PLAN.md — Build pipeline update and UI verification checkpoint

### Phase 6: DLL Integration
**Goal**: The C++ DLL reads all new registry keys written by Phase 5 and appends the corresponding flags to the Claude Code command line on every launch
**Depends on**: Phase 5
**Requirements**: CHN-04, INT-01
**Success Criteria** (what must be TRUE):
  1. Enabling any new CLI flag in Settings and right-clicking a folder causes that flag to appear in the launched `claude` command
  2. Adding multiple channels in Settings causes one `--channels` argument per entry to be passed to Claude Code
  3. Disabling all new flags and clearing channels results in a command line identical to the pre-v1.1.0 behavior
**Plans:** 2 plans

Plans:
- [ ] 06-01-extend-launchclaude-PLAN.md — Extend _LaunchClaude in src/ClaudeFromHere.cpp: bump szFlags buffer, read six new HKCU keys, append flags in D-08 order, channel splitter
- [ ] 06-02-build-and-verify-PLAN.md — Build the DLL, reinstall so Explorer loads it, manually verify the three Phase 6 success criteria including the all-off byte-identical case

## Progress

**Execution Order:**
Phases execute in numeric order: 1 -> 2 -> 3 -> 4 -> 5 -> 6

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 2/2 | Complete | |
| 2. Launcher and Config | 2/3 | In Progress | |
| 3. Installer | 0/2 | Not started | - |
| 4. Distribution | 2/3 | In Progress | |
| 5. Enhanced Settings UI | 1/2 | In Progress|  |
| 6. DLL Integration | 0/? | Not started | - |
