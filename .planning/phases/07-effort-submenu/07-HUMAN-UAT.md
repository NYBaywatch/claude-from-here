---
status: partial
phase: 07-effort-submenu
source: [07-CONTEXT.md]
started: 2026-06-24
updated: 2026-06-24
---

## Current Test

[awaiting fresh release binary — visual context-menu checks are out of CI scope per D-16]

## Tests

### 1. Test A — Both top-level entries present (EFRT-01)
expected: Right-click a folder in Explorer. Two top-level items appear: "Claude from here"
and "Claude from here: effort" (the latter with a submenu arrow). Hovering "Claude from
here: effort" reveals exactly five subitems in order: Low, Medium, High, Extra high, Max.
result: [pending]

### 2. Test B — Each level emits --effort (EFRT-02)
expected: From the flyout, click each level in turn. The `claude ...` line shown in
Windows Terminal contains `--effort <level>` with the matching token
(low/medium/high/xhigh/max), positioned right after `--model` (or first, if no model set),
plus any other configured flags.
result: [pending]

### 3. Test C — Default item unchanged / no --effort (EFRT-03, D-07)
expected: Click the plain "Claude from here" (not the flyout) with all settings off. The
`claude ...` line contains NO `--effort` token and is byte-for-byte identical to the
pre-Phase-7 baseline output for the same settings.
result: [pending]

### 4. Test D — Folder-background flyout (EFRT-01, D-09)
expected: Right-click empty space *inside* a folder (Directory\Background). Both entries
appear; the effort flyout lists the five levels; clicking a level launches Claude in that
folder with the correct `--effort`. Exercises the site-propagation path for subitems.
result: [pending]

## Summary

total: 4
passed: 0
issues: 0
pending: 4
skipped: 0
blocked: 0

## Execution Context

Per D-16 and `.github/workflows/release.yml`, visual context-menu verification is out of
CI scope. Source is build-verified (DLL compiles clean; MSIX packs cleanly with the
two-CLSID manifest). These four tests must be run manually against an installed binary.

When ready to execute:
1. Build locally (`build-installer.ps1 -SkipSign`) or cut a release tag for a signed build.
2. Uninstall any prior copy from `%LOCALAPPDATA%\ClaudeFromHere\`, run the installer.
3. Restart Explorer.
4. Run tests A–D and record the observed `claude ...` lines.
5. Update this file's results and mark EFRT-01..EFRT-04 runtime-verified in REQUIREMENTS.md.
