# Phase 2: Launcher and Config - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-09
**Phase:** 02-launcher-and-config
**Areas discussed:** Config app technology, Config storage & flag application, Path detection strategy, Error dialog polish

---

## Config App Technology

| Option | Description | Selected |
|--------|-------------|----------|
| WinForms (.NET) | Simplest GUI framework, .NET 4.8 pre-installed on Win 11 | :heavy_check_mark: |
| C++ Win32 dialog | Pure native, matches DLL tech stack, more boilerplate | |
| PowerShell GUI | No compilation, ships as .ps1, slower launch | |

**User's choice:** WinForms (.NET)
**Notes:** .NET 4.8 ships with Windows 11, smallest development effort for a native-looking settings dialog.

---

## Config Storage

| Option | Description | Selected |
|--------|-------------|----------|
| JSON in AppData | Human-readable, portable, file I/O per click | |
| Windows Registry | Fast reads via RegGetValueW, native Windows pattern | :heavy_check_mark: |
| JSON next to DLL | Simple but write permission issues in Program Files | |

**User's choice:** Windows Registry (HKCU\Software\ClaudeFromHere)
**Notes:** DLL reads with zero file I/O overhead. Uninstaller handles cleanup.

---

## CLI Flags Exposed

| Option | Description | Selected |
|--------|-------------|----------|
| Core flags only | --model, --verbose, --allowedTools | |
| Extended flags | Core plus --max-turns, --system-prompt, --permission-mode | |
| Free-text field | Core flags as controls plus free-text for additional flags | :heavy_check_mark: |

**User's choice:** Free-text field (core flags as structured controls + free-text escape hatch)

---

## Path Detection Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| PATH-only | Keep SearchPathW as-is, simplest | |
| PATH + known locations | Fallback to common install dirs | |
| PATH + registry | App Paths registry keys + execution aliases | :heavy_check_mark: |

**User's choice:** PATH + registry (SearchPathW -> App Paths -> execution aliases)

---

## Error Dialog Polish

| Option | Description | Selected |
|--------|-------------|----------|
| MessageBox with links | Improved text with install instructions | :heavy_check_mark: |
| Custom dialog with button | Clickable Download button, more code | |
| Keep current MessageBox | Phase 1 errors are already clear enough | |

**User's choice:** MessageBox with improved text including install commands
**Notes:** Include npm install command for Claude Code, Microsoft Store for WT, and "restart Explorer" guidance.

---

## Claude's Discretion

- WinForms project structure and .csproj configuration
- Exact registry value names and types
- App Paths / execution alias lookup implementation
- Config app icon and window sizing

## Deferred Ideas

None
