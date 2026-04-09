# Phase 2: Launcher and Config - Context

**Gathered:** 2026-04-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Enhance the existing DLL launcher with robust path detection (PATH + registry fallbacks), improved error dialogs with install instructions, and add a WinForms GUI config app that lets users configure Claude CLI flags (stored in Windows Registry) which are applied on every launch.

</domain>

<decisions>
## Implementation Decisions

### Config App Technology
- **D-01:** GUI config app built with WinForms (.NET). .NET Framework 4.8 ships with Windows 11, so no additional runtime required. Small footprint (~50KB .exe).
- **D-02:** Config app is a standalone .exe (e.g., ClaudeFromHereConfig.exe) that users can launch from Start Menu or a shortcut.

### Config Storage
- **D-03:** CLI flag settings stored in Windows Registry under `HKCU\Software\ClaudeFromHere`. DLL reads via `RegGetValueW` at Invoke time with zero file I/O overhead.
- **D-04:** Registry key structure: `Model` (REG_SZ), `Verbose` (REG_DWORD), `AllowedTools` (REG_SZ), `ExtraFlags` (REG_SZ for free-text additional flags).

### Config App UI
- **D-05:** Core flags exposed as structured controls: Model as dropdown, Verbose as checkbox, AllowedTools as text field.
- **D-06:** Free-text field for additional/custom flags the user wants to append to every `claude` invocation. Power user escape hatch.

### Flag Application
- **D-07:** DLL reads registry values in `Invoke()` before building the command line. If registry key doesn't exist or values are empty, launch with no extra flags (current behavior).
- **D-08:** Command line format becomes: `wt.exe -d "<path>" -- cmd /k claude <configured-flags>`

### Path Detection
- **D-09:** DLL tries PATH first via `SearchPathW`, then falls back to App Paths registry keys (`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\wt.exe` and `...\claude.exe`) and Windows Terminal's App Execution Alias.
- **D-10:** Detection order: SearchPathW (PATH) -> App Paths registry -> known execution alias locations. First match wins.

### Error Dialogs
- **D-11:** Keep `MessageBoxW` but improve the text to include install instructions. For claude.exe: `npm i -g @anthropic-ai/claude-code`. For wt.exe: point to Microsoft Store. Include "Then restart Explorer" guidance.
- **D-12:** No custom dialog windows for errors -- MessageBox is sufficient and keeps the DLL simple.

### Claude's Discretion
- WinForms project structure and .csproj configuration
- Exact registry value names and types (within the D-04 framework)
- App Paths / execution alias lookup implementation details
- Config app icon and window sizing

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase 1 Outputs
- `.planning/phases/01-foundation/01-CONTEXT.md` -- Phase 1 decisions (D-01 through D-08)
- `.planning/phases/01-foundation/01-01-SUMMARY.md` -- DLL source code details, CLSID, file structure
- `.planning/phases/01-foundation/01-02-SUMMARY.md` -- Build workflow, auto-fixed issues, patterns discovered

### Existing Source Code
- `src/ClaudeFromHere.cpp` -- Current `_LaunchClaude()` method with SearchPathW + MessageBoxW + CreateProcessW (lines 255-310)
- `src/dllmain.cpp` -- DLL entry point, class factory, CLSID definition
- `CMakeLists.txt` -- Current build configuration

### Project Documentation
- `CLAUDE.md` -- Technology stack, constraints, certificate strategy

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `_LaunchClaude()` in `ClaudeFromHere.cpp`: Current launcher method to be enhanced with registry reads and improved error text
- `SearchPathW` calls already in place for PATH detection
- `StringCbPrintfW` command line builder ready to accept additional flags

### Established Patterns
- DLL uses `SearchPathW` for executable lookup -- new registry fallbacks extend this pattern
- `MessageBoxW` for user-facing errors -- improved text keeps the same pattern
- `CreateProcessW` for launching wt.exe -- command line format extends to include flags

### Integration Points
- Config app writes to `HKCU\Software\ClaudeFromHere`; DLL reads from same key
- Config app .exe ships alongside the DLL in the build directory
- Installer (Phase 3) will need to register the config app in Start Menu

</code_context>

<specifics>
## Specific Ideas

- Core flags as structured controls (dropdown for model, checkbox for verbose, text field for allowedTools) plus free-text for anything else
- Registry under HKCU so no admin needed for config changes
- Error messages should include actual install commands users can copy

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 02-launcher-and-config*
*Context gathered: 2026-04-09*
