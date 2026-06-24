---
status: partial
phase: 07-effort-submenu
source: [07-CONTEXT.md]
started: 2026-06-24
updated: 2026-06-24
---

## Current Test

[dev-registered to %LOCALAPPDATA%\ClaudeFromHere; awaiting visual confirmation]

## Tests

### 1. Test A — Single top-level flyout with real icon (EFRT-01)
expected: Right-click a folder. ONE top-level "Claude from here" entry with the Claude
icon (NOT a grouped "Claude From Here" parent, NOT a pink/placeholder block) and a
submenu arrow. Hovering reveals: Default, a separator, then Low, Medium, High,
Extra high, Max.
result: [pending]

### 2. Test B — Each level emits --effort (EFRT-02)
expected: Click each level in turn. The `claude ...` line in Windows Terminal contains
`--effort <level>` with the matching token (low/medium/high/xhigh/max), right after
`--model` (or first, if no model set), plus any other configured flags.
result: [pending]

### 3. Test C — Default emits no --effort (EFRT-03, D-07)
expected: Click "Default" with all settings off. The `claude ...` line contains NO
`--effort` token and is byte-for-byte identical to the pre-Phase-7 baseline output.
result: [pending]

### 4. Test D — Folder-background flyout (EFRT-01, D-09)
expected: Right-click empty space *inside* a folder (Directory\Background). Same single
flyout; clicking a level launches Claude in that folder with the correct `--effort`.
Exercises site propagation to subitems.
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Execution Context

Per D-16, visual context-menu verification is out of CI scope. Source is build-verified
(DLL compiles clean; MSIX packs) and the package has been dev-registered (Developer Mode)
to `%LOCALAPPDATA%\ClaudeFromHere` with Explorer restarted. Run tests A–D by right-clicking
a folder and recording the observed menu + `claude ...` lines, then mark EFRT-01..04
runtime-verified in REQUIREMENTS.md.

Note: the two-CLSID approach was tried first and reverted — Windows 11 grouped the two
verbs under a "Claude From Here" parent badged with the placeholder package logo (the
pink rectangle), and would have nested the effort levels too deep. The single-flyout
design here is what resolved both issues.
