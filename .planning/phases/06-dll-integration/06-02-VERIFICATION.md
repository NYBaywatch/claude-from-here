# Phase 6 Verification Results

Date: 2026-04-10
DLL build: build/Release/ClaudeFromHere.dll (26112 bytes, 2026-04-10 19:52)
Deployed to: %LOCALAPPDATA%\ClaudeFromHere\ClaudeFromHere.dll
Status: DEFERRED — manual verification postponed to post-release fresh-binary testing

## Deferral Rationale

Per user decision on 2026-04-10, the three Phase 6 runtime success criteria
will be verified against a freshly-installed v1.1.0 release binary rather
than against the locally-built dev DLL. Rationale:

- Fresh-binary path exercises the real signed installer flow end-to-end
- Per D-16 and `.github/workflows/release.yml:110`, visual context-menu
  verification is not automatable in CI and is explicitly out of CI scope
- Source-level changes are already verified by plan 06-01's acceptance grep
  (flag emission order, registry reads, D-08 ordering)

The three test cases below are preserved verbatim so they survive into
post-release UAT. They will be executed against the installed v1.1.0 binary
once the release tag is cut and the signed installer is available.

## Test A — All new flags enabled (Success Criterion #1)

Status: DEFERRED

Settings: Continue=on, Resume=on, DangerSkip=on, AllowDangerSkip=on, RemotePrefix="myprefix", Channels=[]

Expected observed line must contain (in order):

> ... -c -r --dangerously-skip-permissions --allow-dangerously-skip-permissions --remote-control-session-name-prefix myprefix ...

Observed: [pending — post-release]

## Test B — Multiple channels (Success Criterion #2 / CHN-04)

Status: DEFERRED

Settings: 3 channels added (plugin:telegram@claude-plugins-official, plugin:discord@claude-plugins-official, freeform-test)

Expected: exactly three `--channels` tokens, one per entry, no CSV form

> ... --channels plugin:telegram@claude-plugins-official --channels plugin:discord@claude-plugins-official --channels freeform-test ...

Observed: [pending — post-release]

## Test C — All-off byte-identical (Success Criterion #3 / D-13)

Status: DEFERRED

Settings: every new flag off, channels empty

Expected: NONE of `-c`, `-r`, `--dangerously-skip-permissions`, `--allow-dangerously-skip-permissions`, `--remote-control-session-name-prefix`, `--channels` present; line byte-identical to pre-v1.1.0 output for the same Model/Verbose/AllowedTools/ExtraFlags state.

Observed: [pending — post-release]

## Conclusion

Phase 6 success criteria met: deferred (pending post-release fresh-binary verification)
