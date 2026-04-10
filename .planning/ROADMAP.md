# Roadmap: Claude From Here

## Overview

Four phases deliver a working Windows 11 context menu extension from scratch to public release. Phase 1 resolves the critical technical unknown (Directory\Background coverage) and produces a manually-registerable proof-of-concept. Phase 2 wires the verb to a working launcher script with path detection, error handling, and a GUI config app for Claude CLI flags. Phase 3 wraps everything into a polished Inno Setup installer. Phase 4 ships to GitHub with documentation.

## Phases

**Phase Numbering:**
- Integer phases (1, 2, 3): Planned milestone work
- Decimal phases (2.1, 2.2): Urgent insertions (marked with INSERTED)

Decimal phases appear between their surrounding integers in numeric order.

- [ ] **Phase 1: Foundation** - Sparse MSIX manifest, self-signed cert, and proof-of-concept menu registration for both folder and folder-background targets
- [ ] **Phase 2: Launcher and Config** - Working launch script with path detection, error dialogs, and GUI app for Claude CLI flag configuration
- [ ] **Phase 3: Installer** - Inno Setup `.exe` that handles all MSIX registration, cert import, file deployment, upgrade, and uninstall
- [ ] **Phase 4: Distribution** - GitHub release with downloadable installer, README, and clean-machine validation

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
**Plans:** 1/3 plans executed
**UI hint**: yes

Plans:
- [x] 02-01-PLAN.md — DLL enhancement: path detection, registry flag reads, improved error dialogs
- [ ] 02-02-PLAN.md — WinForms config app: settings UI, registry read/write, path detection display
- [ ] 02-03-PLAN.md — Integration: register.ps1 update and end-to-end verification

### Phase 3: Installer
**Goal**: A single `.exe` installer handles all setup steps (cert import, MSIX registration, file deployment) and a clean uninstaller removes everything, with upgrade installs working without manual intervention
**Depends on**: Phase 2
**Requirements**: INST-01, INST-02, INST-03, INST-04
**Success Criteria** (what must be TRUE):
  1. Running the installer once registers the MSIX, imports the signing cert, and deploys all files with no manual steps required
  2. Running the uninstaller removes all MSIX registrations, files, and registry entries cleanly
  3. The installer works on Windows 11 machines with different user profiles and non-standard install locations
  4. Reinstalling over an existing version (upgrade) completes without requiring a manual uninstall first
**Plans**: TBD

### Phase 4: Distribution
**Goal**: The project is publicly available on GitHub with a downloadable installer in Releases and documentation that enables users to install, use, and troubleshoot without assistance
**Depends on**: Phase 3
**Requirements**: DIST-01, DIST-02
**Success Criteria** (what must be TRUE):
  1. The GitHub repository has a published Release with the installer `.exe` as a downloadable artifact
  2. The README documents installation steps, uninstallation steps, and common troubleshooting scenarios
  3. A clean Windows 11 machine can install and use the extension using only the README and the installer from GitHub Releases
**Plans**: TBD

## Progress

**Execution Order:**
Phases execute in numeric order: 1 → 2 → 3 → 4

| Phase | Plans Complete | Status | Completed |
|-------|----------------|--------|-----------|
| 1. Foundation | 2/2 | Complete |  |
| 2. Launcher and Config | 1/3 | In Progress|  |
| 3. Installer | 0/TBD | Not started | - |
| 4. Distribution | 0/TBD | Not started | - |
