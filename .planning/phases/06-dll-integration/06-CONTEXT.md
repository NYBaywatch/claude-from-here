# Phase 6: DLL Integration - Context

**Gathered:** 2026-04-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Extend the existing C++ shell extension DLL (`src/ClaudeFromHere.cpp`) to read the six new HKCU registry keys written by the Phase 5 Settings app, and append the corresponding flags/arguments to the `wt.exe ... claude ...` command line on every menu invocation.

Specifically, the DLL must:
- Read `Continue`, `Resume`, `DangerouslySkipPermissions`, `AllowDangerouslySkipPermissions` (DWORD) and emit their boolean flags when set.
- Read `RemoteControlPrefix` (REG_SZ) and emit `--remote-control-session-name-prefix <value>` when non-empty.
- Read `Channels` (REG_SZ, pipe-delimited) and emit one `--channels <entry>` per non-empty entry.
- Preserve exact pre-v1.1.0 behavior when all new keys are absent/zero/empty.

Settings UI, registry writes, and channel storage format are out of scope — that is Phase 5 and is complete.

</domain>

<decisions>
## Implementation Decisions

### Channel parsing (Channels REG_SZ)
- **D-01:** Split the `Channels` REG_SZ on `|` (pipe delimiter — matches Phase 5 storage format).
- **D-02:** Trim leading/trailing whitespace on each split entry, then skip any entry that is empty after trimming. Handles trailing pipe, accidental `||`, and accidental whitespace gracefully.
- **D-03:** Hard cap the number of channels processed at **32**. Entries beyond the cap are silently ignored. Prevents buffer overflow, avoids dynamic allocation, and is far above any realistic user count.
- **D-04:** Emit exactly one `--channels <entry>` per surviving entry (matches CHN-04 and roadmap success criterion #2). No CSV, no bundling.

### Value quoting and shell safety
- **D-05:** No quoting of `RemoteControlPrefix` or channel entries on the command line. Values are appended raw, mirroring the existing `Model` / `AllowedTools` / `ExtraFlags` pattern.
- **D-06:** No DLL-side validation or sanitization of user-provided strings. The Settings app already warns on shell metacharacters in ExtraFlags; extending that responsibility to the DLL is out of scope and inconsistent with existing flag handling.
- **D-07:** Rationale: the DLL is the "executor of last resort" — if Settings writes it, the DLL passes it. Defensive validation belongs in the UI layer.

### Flag ordering on command line
- **D-08:** Flags are grouped by type with `ExtraFlags` remaining last as the user-override escape hatch. Explicit order:
  1. `--model <value>` (if set)
  2. `--verbose` (if set)
  3. `-c` (if Continue set)
  4. `-r` (if Resume set)
  5. `--dangerously-skip-permissions` (if set)
  6. `--allow-dangerously-skip-permissions` (if set)
  7. `--remote-control-session-name-prefix <value>` (if non-empty)
  8. `--allowedTools <value>` (if non-empty)
  9. `--channels <entry>` (repeated per surviving entry)
  10. `<ExtraFlags raw>` (if non-empty) — stays last so user-provided overrides are visible and cannot be clobbered by a later grouped flag.

### Backward compatibility / "all off" behavior
- **D-09:** When every new registry key is missing, zero, or empty, the assembled command line must be byte-for-byte identical to the pre-v1.1.0 output. Continue the existing `RRF_ZEROONFAILURE` pattern so absent keys behave as zero/empty strings without any special-case code.
- **D-10:** Boolean flags (`-c`, `-r`, dangerous x2) are guarded by `if (dw) { StringCbCatW(...); }` — zero value means no append, never an empty flag.
- **D-11:** Empty `RemoteControlPrefix` or empty `Channels` → no flag appended at all (not even the flag name with a blank value).

### Verification approach
- **D-12:** Primary verification is visual: the existing `wt.exe -d "<path>" -- cmd /k claude <flags>` invocation causes Windows Terminal to display the exact `claude <flags>` line being run. Manually right-click a folder, inspect the terminal window, confirm flags match Settings state.
- **D-13:** Plan must include an explicit "all off" verification case: disable every new flag, clear every channel, right-click, and confirm the visible `claude` command line has no new flags and matches pre-v1.1.0 output exactly. Protects success criterion #3.
- **D-14:** No permanent debug logging added to the DLL. Shell extensions should not write to the filesystem on every invoke without opt-in. If debugging is needed during development, a temporary log can be added and removed before phase completion.

### Buffer sizing
- **D-15:** Expand `szFlags` buffer in `_LaunchClaude` from 4096 to **16384** bytes to comfortably hold 32 channel entries plus all other flags. `szCmdLine[32768]` remains unchanged — it already has headroom.

### Claude's Discretion
- Exact helper-function shape for channel splitting (inline in `_LaunchClaude` vs. extracted static helper).
- Whether to use `wcstok_s`, manual pointer walking, or another idiom for the split.
- Exact variable names for the new locals.
- Whether to introduce a small `AppendFlag` helper to reduce `StringCbCatW` duplication, or keep the existing inline style.

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### DLL source (primary target)
- `src/ClaudeFromHere.cpp` — The file being modified. `_LaunchClaude()` at line 281 is where flag reading and command line assembly happens. Review lines 333–393 (registry reads, flag building, command line assembly) for the exact pattern to extend.
- `src/dllmain.cpp` — DLL entry point and class factory. Reference only; no changes expected in Phase 6.

### Settings app (registry writer — already complete)
- `src/ClaudeFromHereConfig/MainWindow.xaml.cs` lines 70–128 — Authoritative source for new registry key names, types, and storage formats. The DLL must read exactly what this writes. Channels storage: `string.Join("|", _channels)`.

### Prior context
- `.planning/phases/05-enhanced-settings-ui/05-CONTEXT.md` — Phase 5 decisions including registry key names, channel delimiter, and danger-flag naming.

### Requirements
- `.planning/REQUIREMENTS.md` — CHN-04 (multiple `--channels` flags) and INT-01 (DLL reads new keys). These are the only two requirements this phase closes.
- `.planning/ROADMAP.md` §"Phase 6: DLL Integration" — Goal and three success criteria.

### Build system
- `CMakeLists.txt` — C++ build config. Phase 6 should not need changes here unless new source files are added. Expected: modify `ClaudeFromHere.cpp` only.

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `RegGetValueW` calls in `_LaunchClaude` (ClaudeFromHere.cpp:339–353) — established pattern with `RRF_ZEROONFAILURE`. New key reads follow the exact same shape.
- `StringCbCatW` appending to `szFlags` (lines 355–375) — extend this block with new flags per the D-08 ordering.
- `if (dwVerbose) { ... }` pattern (line 362) — model for all four new boolean flags.
- `if (szExtraFlags[0]) { ... }` pattern (line 371) — model for `RemoteControlPrefix` non-empty check.

### Established Patterns
- All registry reads use `HKEY_CURRENT_USER`, subkey `Software\ClaudeFromHere`, `RRF_ZEROONFAILURE` so missing keys degrade silently to zero/empty.
- String values get raw append, not quoted.
- Flags block is built incrementally via `StringCbCatW` into a fixed-size `WCHAR` buffer.
- Final command line is assembled with `StringCbPrintfW` into `szCmdLine[32768]` and launched via `CreateProcessW` with explicit `lpApplicationName`.

### Integration Points
- No new files created. All changes localized to `src/ClaudeFromHere.cpp` (and minor buffer size bump).
- The Phase 5 Inno Setup update already deploys the rebuilt DLL. Phase 6 just needs `cmake --build` to produce the new DLL, then reinstall.
- MSIX sparse package, AppxManifest, signing cert — all unchanged. Phase 6 is pure C++ edit.

### New code needed
- Registry reads for 6 new keys (4 DWORD, 2 REG_SZ) — ~15 lines.
- Boolean flag append blocks — ~12 lines.
- `RemoteControlPrefix` append block — ~5 lines.
- Channel splitter + per-entry `--channels` emission — ~20–30 lines (the only non-trivial addition).

</code_context>

<specifics>
## Specific Ideas

- "ExtraFlags must remain the last thing on the command line — it's the user's escape hatch and override."
- "Trust the Settings app for validation. If the user can type it there, the DLL passes it through."
- "The terminal window showing `cmd /k claude ...` is our natural observability — no log files needed."
- Channel cap of 32 is arbitrary but defensible; even power users are unlikely to configure more than 5–10 channels.

</specifics>

<deferred>
## Deferred Ideas

- **Opt-in DLL debug logging** (reg key gated) — useful for user troubleshooting post-release but not needed for Phase 6 verification. Candidate for a future maintenance phase if users report flag-application issues.
- **DLL-side shell metacharacter warning parity with Settings app** — defensive hardening, not required for this release.
- **Dynamic flags buffer sizing** — unnecessary complexity while the channel cap is finite.

</deferred>

---

*Phase: 06-dll-integration*
*Context gathered: 2026-04-10*
