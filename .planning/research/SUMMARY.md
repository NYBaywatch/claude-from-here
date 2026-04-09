# Project Research Summary

**Project:** Claude From Here
**Domain:** Windows 11 Shell Extension — Sparse MSIX Context Menu Launcher
**Researched:** 2026-04-08
**Confidence:** HIGH

## Executive Summary

Claude From Here is a Windows 11 context menu extension that adds "Open Claude Code here" to folder right-click menus. The viable technical path is a sparse MSIX package (AppxManifest + icon assets) registered per-user via `Add-AppxPackage -ExternalLocation`, with a `.bat` launcher script invoking `wt.exe`. Registry-only approaches land in "Show more options" and cannot reach the top-level modern context menu.

**Note on COM DLL:** Stack research found that a C++ COM DLL implementing `IExplorerCommand` may be required for full `Directory\Background` support. Architecture research suggests manifest-only may suffice. This is the main technical unknown — a proof-of-concept in Phase 1 will resolve it.

## Recommended Stack

- **AppxManifest.xml + sparse MSIX** — grants package identity for Win11 top-level context menu
- **launch.bat** — executes `wt.exe -d "<dir>" cmd /k claude`
- **MakeAppx.exe + SignTool.exe** (Windows SDK) — pack and sign the MSIX
- **New-SelfSignedCertificate** (PowerShell) — signing cert for dev and v1 distribution
- **Inno Setup 6.7.1** — `.exe` installer with path detection, cert import, MSIX registration

**What NOT to use:** Registry-only `.reg` (buried in "Show more options"), `uap2:SupportedVerbs` without COM (cannot target Directory), `Cert:\CurrentUser\TrustedPeople` (MSIX verifier checks machine store only).

## Table Stakes Features

- Menu item at top level of Win11 modern context menu (folder + folder background)
- Custom Claude icon next to menu item
- Opens Windows Terminal at correct directory running `claude`
- Auto-detect `claude.exe` and `wt.exe` at install time
- Graceful error dialog if dependencies not found
- `.exe` installer with automated MSIX registration
- Clean uninstaller removing MSIX registration and all files

## Critical Pitfalls

1. **Manifest Publisher must exactly match signing cert Subject** — silent registration failure otherwise
2. **Self-signed cert requires machine trust store import** — blocks all end-user installs without it
3. **Incomplete uninstaller leaves ghost MSIX registrations** — breaks reinstall
4. **Same MSIX version blocks upgrade installs** — must unregister before re-registering
5. **wt.exe has multiple locations** — use `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe`
6. **Missing AppxManifest namespace declarations** — silently ignored by Explorer

## Suggested Phase Structure

1. **Manifest and Certificate Foundation** — validate MSIX packaging and registration chain
2. **Launcher Script and Core Runtime** — wire verb to working command with path detection
3. **Installer Construction** — wrap into user-deliverable `.exe` with install/upgrade/uninstall
4. **Distribution and Release** — CI pipeline, clean VM validation, public release

## Research Flags

- **Directory\Background coverage:** Whether manifest-only approach covers folder background right-click without COM surrogate needs proof-of-concept in Phase 1
- **Per-step elevation in Inno Setup:** Cert import requires admin but rest of install may not — needs confirmation
- **Icon sizing:** Exact sizes for context menu rendering in light/dark mode need confirmation

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | Official MS docs + VS Code reference implementation |
| Features | HIGH | Verified against VS Code, Notepad++, community tools |
| Architecture | HIGH | One unknown: Directory\Background coverage |
| Pitfalls | HIGH | All sourced from official docs or verified implementations |

---
*Research completed: 2026-04-08*
*Ready for roadmap: yes*
