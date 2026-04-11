---
status: partial
phase: 06-dll-integration
source: [06-02-VERIFICATION.md]
started: 2026-04-10
updated: 2026-04-10
---

## Current Test

[awaiting fresh v1.1.0 release binary — deferred per user decision 2026-04-10]

## Tests

### 1. Test A — All new flags enabled (Success Criterion #1)
expected: Settings app with Continue=on, Resume=on, DangerSkip=on, AllowDangerSkip=on, RemotePrefix="myprefix", Channels=[]. Right-click any folder. The `claude ...` line in Windows Terminal must contain (in this relative order): `-c -r --dangerously-skip-permissions --allow-dangerously-skip-permissions --remote-control-session-name-prefix myprefix`
result: [pending]

### 2. Test B — Multiple channels (Success Criterion #2 / CHN-04)
expected: Add three channels in Settings (e.g. `plugin:telegram@claude-plugins-official`, `plugin:discord@claude-plugins-official`, `freeform-test`). Right-click any folder. The `claude ...` line must contain exactly three `--channels` tokens, one per entry. No CSV/bundled form.
result: [pending]

### 3. Test C — All-off byte-identical (Success Criterion #3 / D-13)
expected: Settings app with every new flag off and channel list empty. Leave Model/Verbose/AllowedTools/ExtraFlags at Phase 5 values. Right-click any folder. The `claude ...` line must contain NONE of: `-c`, `-r`, `--dangerously-skip-permissions`, `--allow-dangerously-skip-permissions`, `--remote-control-session-name-prefix`, `--channels`. Line must be byte-for-byte identical to pre-v1.1.0 output for the same baseline settings.
result: [pending]

## Summary

total: 3
passed: 0
issues: 0
pending: 3
skipped: 0
blocked: 0

## Gaps

## Execution Context

These three tests were deferred from plan 06-02 at the user's direction on 2026-04-10:
> "yea do it at the end through a fresh binary"

Per D-16 and `.github/workflows/release.yml:110`, visual context-menu verification is explicitly out of CI scope. These tests must be run manually against the installed v1.1.0 binary after the release tag is cut and the signed Inno Setup installer has been run on the target machine.

When ready to execute:
1. Cut v1.1.0 release tag (triggers `.github/workflows/release.yml`)
2. Download `ClaudeFromHere-Setup.exe` from the GitHub Release
3. Run the installer (uninstall any prior dev copy from `%LOCALAPPDATA%\ClaudeFromHere\` first)
4. Restart Explorer
5. Run tests A, B, C above and record observed `claude ...` lines
6. Update this file's results and `06-02-VERIFICATION.md`, mark requirements CHN-04 and INT-01 runtime-verified in REQUIREMENTS.md
7. `/gsd:verify-work 06` to close the UAT
