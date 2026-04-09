# Requirements: Claude From Here

**Defined:** 2026-04-08
**Core Value:** Right-click any folder in Windows 11 Explorer → Claude Code opens in that directory. One click, no terminal juggling.

## v1 Requirements

Requirements for initial release. Each maps to roadmap phases.

### Context Menu

- [ ] **MENU-01**: User sees "Claude from here" in the Windows 11 modern (top-level) right-click menu when right-clicking a folder
- [ ] **MENU-02**: User sees "Claude from here" when right-clicking the background inside a folder
- [ ] **MENU-03**: Menu item displays a custom Claude icon next to the text

### Launch

- [ ] **LNCH-01**: Clicking the menu item opens Windows Terminal in the right-clicked directory with `claude` running
- [ ] **LNCH-02**: Installer auto-detects the claude.exe path (no manual configuration required)
- [ ] **LNCH-03**: Installer auto-detects the wt.exe path (no manual configuration required)
- [ ] **LNCH-04**: User sees a clear error dialog if Claude Code is not found when clicking the menu item
- [ ] **LNCH-05**: User sees a clear error dialog if Windows Terminal is not found when clicking the menu item

### Configuration

- [ ] **CONF-01**: User can open a simple GUI app to configure Claude CLI flags passed to the `claude` command
- [ ] **CONF-02**: Common flags are exposed as checkboxes/dropdowns (e.g., --model, --verbose, --allowedTools)
- [ ] **CONF-03**: Configured flags are persisted across sessions and applied on every launch

### Installation

- [ ] **INST-01**: User can install via a single `.exe` installer that handles all MSIX registration, cert import, and file deployment
- [ ] **INST-02**: User can cleanly uninstall, removing all MSIX registration, files, and registry entries
- [ ] **INST-03**: Installer works across different Windows 11 setups (different user profiles, install locations)
- [ ] **INST-04**: Upgrade installs work (reinstall over existing version without manual uninstall)

### Distribution

- [ ] **DIST-01**: Project is available on GitHub with downloadable installer in Releases
- [ ] **DIST-02**: README documents installation, uninstallation, and troubleshooting

## v2 Requirements

Deferred to future release. Tracked but not in current roadmap.

### Enhanced Launch

- **LNCH-06**: "Open as Administrator" context menu variant
- **LNCH-07**: Path override registry key for non-standard install locations

### Advanced Configuration

- **CONF-04**: System tray icon for quick access to settings
- **CONF-05**: Per-directory configuration profiles

### CI/CD

- **DIST-03**: GitHub Actions CI producing versioned installer artifacts per release tag

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
| (populated by roadmapper) | | |

**Coverage:**
- v1 requirements: 16 total
- Mapped to phases: 0
- Unmapped: 16 (pending roadmap)

---
*Requirements defined: 2026-04-08*
*Last updated: 2026-04-08 after initial definition*
