# Phase 1: Foundation - Context

**Gathered:** 2026-04-09
**Status:** Ready for planning

<domain>
## Phase Boundary

Produce a sparse MSIX package with a C++ COM DLL implementing `IExplorerCommand` that registers "Claude from here" in the Windows 11 modern (top-level) context menu for both folder right-clicks and folder-background right-clicks. The menu item must have a custom Claude icon and launch a working `wt.exe` session with `claude` running in the target directory. Includes self-signed certificate, signing, and PowerShell dev scripts for registration.

</domain>

<decisions>
## Implementation Decisions

### Click Behavior
- **D-01:** Phase 1 wires the full working launcher (`wt.exe -d "<dir>" cmd /k claude`), not a placeholder. This proves end-to-end functionality; Phase 2 adds error handling and path detection on top.
- **D-02:** The DLL searches PATH for `wt.exe` and `claude.exe` at invocation time. If either is not found, a basic Windows MessageBox displays a clear error (e.g., "claude.exe not found in PATH"). Phase 2 replaces this with polished error dialogs and auto-detection beyond PATH.

### Icon
- **D-03:** Ship a custom `.ico` file with the package (multi-size: 16x16, 32x32, 48x48) rather than extracting from `claude.exe` at runtime.
- **D-04:** Icon design uses the Claude logo mark (sparkle/asterisk symbol). Must work in both light and dark Explorer themes.

### Build Toolchain
- **D-05:** C++ compiler is MSVC via Visual Studio 2022 Build Tools (free). Recommended by CLAUDE.md for in-process COM compatibility.
- **D-06:** Build system is CMake. More portable and version-control-friendly than `.vcxproj` files; standard for open-source C++ projects.

### Dev Registration Flow
- **D-07:** Separate PowerShell scripts for dev workflow: `register.ps1` and `unregister.ps1`. These handle cert generation, MSIX signing via MakeAppx/SignTool, and `Add-AppxPackage -ExternalLocation` registration.
- **D-08:** Admin elevation required only once for initial cert import to `LocalMachine\TrustedPeople`. Subsequent re-registrations (rebuild DLL, re-pack MSIX, re-register) run unelevated. Scripts should detect whether cert is already imported and skip the admin step.

### Claude's Discretion
- DLL project structure and COM boilerplate organization
- AppxManifest.xml namespace details and schema versions
- Exact CMakeLists.txt structure
- Script error handling details beyond the MessageBox approach

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Shell Extension Architecture
- `CLAUDE.md` -- Technology stack, recommended stack table, AppxManifest schema, "What NOT to Use" section, installation command reference, version compatibility matrix
- `GETTING-STARTED.md` -- Prior work context, existing .reg approach, prerequisites

### Existing Code
- `claude-from-here.reg` -- Working registry approach showing the command pattern and both Directory targets (folder + background)
- `claude-from-here-uninstall.reg` -- Registry cleanup pattern

### Microsoft Documentation (external)
- [Integrate packaged app with File Explorer](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/integrate-packaged-app-with-file-explorer) -- `desktop4`/`desktop5` schema, COM DLL approach
- [Grant identity to non-packaged apps](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps) -- Sparse package manifest template, build/sign/register workflow
- [IExplorerCommand](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers) -- Modern shell extension interface

### Reference Implementation
- [microsoft/vscode-explorer-command](https://github.com/microsoft/vscode-explorer-command) -- VS Code's C++ `IExplorerCommand` implementation for Win 11 via sparse package

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `claude-from-here.reg`: Establishes the command pattern (`cmd.exe /c start "Claude" wt.exe -d "%V" cmd /k claude`) and both registry targets (`Directory\shell` and `Directory\Background\shell`)
- Command pattern can be adapted for the DLL's `Invoke` method

### Established Patterns
- No C++ code exists yet -- this phase creates the foundation
- The `.reg` file confirms both `Directory` and `Directory\Background` targets are needed

### Integration Points
- The DLL will be referenced by the AppxManifest.xml via CLSID
- The sparse MSIX package registers via `Add-AppxPackage -ExternalLocation` pointing to the build output directory
- Self-signed cert must be in `LocalMachine\TrustedPeople` before MSIX registration succeeds

</code_context>

<specifics>
## Specific Ideas

- The existing `.reg` file's command pattern (`wt.exe -d "%V" cmd /k claude`) is the proven launch command -- reuse in the DLL
- PATH lookup for executables rather than hardcoded paths, with MessageBox fallback if not found
- Claude logo mark (sparkle/asterisk) for the icon, not a terminal hybrid

</specifics>

<deferred>
## Deferred Ideas

None -- discussion stayed within phase scope

</deferred>

---

*Phase: 01-foundation*
*Context gathered: 2026-04-09*
