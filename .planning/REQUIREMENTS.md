# Requirements: Claude From Here

**Defined:** 2026-04-08
**Core Value:** Right-click any folder in Windows 11 Explorer -> Claude Code opens in that directory. One click, no terminal juggling.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Context Menu

- [x] **MENU-01**: User sees "Claude from here" in the Windows 11 modern (top-level) right-click menu when right-clicking a folder
- [x] **MENU-02**: User sees "Claude from here" when right-clicking the background inside a folder
- [x] **MENU-03**: Menu item displays a custom Claude icon next to the text

### Launch

- [x] **LNCH-01**: Clicking the menu item opens Windows Terminal in the right-clicked directory with `claude` running
- [x] **LNCH-02**: Installer auto-detects the claude.exe path (no manual configuration required)
- [x] **LNCH-03**: Installer auto-detects the wt.exe path (no manual configuration required)
- [x] **LNCH-04**: User sees a clear error dialog if Claude Code is not found when clicking the menu item
- [x] **LNCH-05**: User sees a clear error dialog if Windows Terminal is not found when clicking the menu item

### Configuration

- [x] **CONF-01**: User can open a simple GUI app to configure Claude CLI flags passed to the `claude` command
- [x] **CONF-02**: Common flags are exposed as checkboxes/dropdowns (e.g., --model, --verbose, --allowedTools)
- [x] **CONF-03**: Configured flags are persisted across sessions and applied on every launch

### Installation

- [x] **INST-01**: User can install via a single `.exe` installer that handles all MSIX registration, cert import, and file deployment
- [x] **INST-02**: User can cleanly uninstall, removing all MSIX registration, files, and registry entries
- [x] **INST-03**: Installer works across different Windows 11 setups (different user profiles, install locations)
- [x] **INST-04**: Upgrade installs work (reinstall over existing version without manual uninstall)

### Distribution

- [x] **DIST-01**: Project is available on GitHub with downloadable installer in Releases
- [x] **DIST-02**: README documents installation, uninstallation, and troubleshooting

## v1.1 Requirements

Requirements for Enhanced Settings milestone. Each maps to roadmap phases.

### CLI Flags

- [ ] **CFG-01**: User can enable `-c` (continue last conversation) via checkbox in Settings
- [ ] **CFG-02**: User can enable `-r` (resume) via checkbox in Settings
- [ ] **CFG-03**: User can enable `--dangerously-skip-permissions` via checkbox, visually flagged as unsafe (red/warning styling)
- [ ] **CFG-04**: User can enable `--allow-dangerously-skip-permissions` via checkbox, visually flagged as unsafe (red/warning styling)
- [ ] **CFG-05**: User can set `--remote-control-session-name-prefix <prefix>` via text input in Settings

### Channels

- [ ] **CHN-01**: User can add a channel from preset list (telegram, discord) via dropdown in Settings
- [ ] **CHN-02**: User can add a custom channel entry via freeform text input
- [ ] **CHN-03**: User can remove individual channel entries from the list
- [ ] **CHN-04**: Multiple `--channels` flags are passed to Claude Code (one per entry)

### Integration

- [ ] **INT-01**: C++ DLL reads new registry keys and appends corresponding flags to the Claude Code command line
- [ ] **INT-02**: Settings persist across app restarts (registry storage, consistent with existing pattern)

## Future Requirements

Deferred to future release. Tracked but not in current roadmap.

### Enhanced Launch

- **LNCH-06**: "Open as Administrator" context menu variant
- **LNCH-07**: Path override registry key for non-standard install locations

### Advanced Configuration

- **CONF-04**: System tray icon for quick access to settings
- **CONF-05**: Per-directory configuration profiles

### Platform

- **PLAT-01**: ARM64 build for Surface/Copilot+ PCs

## Out of Scope

| Feature | Reason |
|---------|--------|
| Windows 10 support | Existing .reg files handle Win 10; modern menu requires Win 11 |
| Desktop right-click | Not a common workflow for launching CLI tools |
| Submenu with multiple options | Single menu item keeps it simple; submenu API is experimental |
| Microsoft Store distribution | Too much overhead for a dev tool |
| Auto-update mechanism | Overkill for v1; users can re-download from GitHub |
| WSL/Linux shell variant | Different platform, different project |
| GUI settings panel beyond CLI flags | Keep config app focused on the one thing it does |

## Traceability

Which phases cover which requirements. Updated during roadmap creation.

| Requirement | Phase | Status |
|-------------|-------|--------|
| MENU-01 | Phase 1 | Complete |
| MENU-02 | Phase 1 | Complete |
| MENU-03 | Phase 1 | Complete |
| LNCH-01 | Phase 2 | Complete |
| LNCH-02 | Phase 2 | Complete |
| LNCH-03 | Phase 2 | Complete |
| LNCH-04 | Phase 2 | Complete |
| LNCH-05 | Phase 2 | Complete |
| CONF-01 | Phase 2 | Complete |
| CONF-02 | Phase 2 | Complete |
| CONF-03 | Phase 2 | Complete |
| INST-01 | Phase 3 | Complete |
| INST-02 | Phase 3 | Complete |
| INST-03 | Phase 3 | Complete |
| INST-04 | Phase 3 | Complete |
| DIST-01 | Phase 4 | Complete |
| DIST-02 | Phase 4 | Complete |
| CFG-01 | Phase 5 | Pending |
| CFG-02 | Phase 5 | Pending |
| CFG-03 | Phase 5 | Pending |
| CFG-04 | Phase 5 | Pending |
| CFG-05 | Phase 5 | Pending |
| CHN-01 | Phase 5 | Pending |
| CHN-02 | Phase 5 | Pending |
| CHN-03 | Phase 5 | Pending |
| CHN-04 | Phase 6 | Pending |
| INT-01 | Phase 6 | Pending |
| INT-02 | Phase 5 | Pending |

**Coverage:**
- v1 requirements: 16 total, 16 complete
- v1.1 requirements: 11 total, 11 mapped
- Unmapped: 0

---
*Requirements defined: 2026-04-08*
*Last updated: 2026-04-10 — v1.1.0 roadmap phases assigned*
