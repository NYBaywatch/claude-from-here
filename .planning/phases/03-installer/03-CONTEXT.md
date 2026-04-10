# Phase 3: Installer - Context

**Gathered:** 2026-04-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Produce an Inno Setup `.exe` installer that deploys all files to `%LOCALAPPDATA%\ClaudeFromHere\`, registers the MSIX package, creates a Start Menu shortcut for the config app, and provides a clean uninstaller. The MSIX is signed with Azure Trusted Signing (CA-backed) so no self-signed cert import is needed and no admin elevation is required.

</domain>

<decisions>
## Implementation Decisions

### Signing Strategy
- **D-01:** Use Azure Trusted Signing (already configured) to sign the MSIX package at build time. No self-signed certificate, no cert import at install time, no SmartScreen warnings.
- **D-02:** The installer ships a pre-signed MSIX — signing happens during the build process, not at install time.

### Elevation
- **D-03:** No admin required. The entire installer runs per-user without UAC prompts. Azure Trusted Signing eliminates the cert import step that was the only reason for admin elevation.

### Install Location
- **D-04:** Files install to `%LOCALAPPDATA%\ClaudeFromHere\`. Per-user, no admin needed. Standard pattern (Discord, VS Code user install, Slack).
- **D-05:** Files deployed: `ClaudeFromHere.dll`, `ClaudeFromHere.exe` (stub), `ClaudeFromHereConfig.exe`, `claude.ico`, `AppxManifest.xml`, `Assets\` folder, `ClaudeFromHere.msix` (pre-signed).

### Start Menu
- **D-06:** Create a Start Menu shortcut for `ClaudeFromHereConfig.exe` labeled "Claude From Here Settings" so users can discover and launch the config app.

### Upgrade Behavior
- **D-07:** Overwrite in place. Installer detects existing installation, unregisters old MSIX, deploys new files over old ones, re-registers new MSIX. User's config registry settings (`HKCU\Software\ClaudeFromHere`) are preserved automatically.

### Uninstall Behavior
- **D-08:** Remove MSIX registration, all installed files, and Start Menu shortcut. Leave `HKCU\Software\ClaudeFromHere` registry key intact so reinstalling preserves user settings.

### Installer UX
- **D-09:** Minimal wizard: Welcome page -> Install -> Done. No unnecessary options screens.
- **D-10:** Support `/SILENT` and `/VERYSILENT` flags for power users and scripted deployment.
- **D-11:** Final page tells the user Explorer needs to restart, user clicks a button to restart it, then the wizard finishes. In silent mode, Explorer restarts automatically.

### Claude's Discretion
- Inno Setup script structure and helper functions
- Exact MSIX registration PowerShell commands (adapt from register.ps1)
- Welcome page branding and text
- Build script that signs with Azure Trusted Signing before packaging

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Phase 1 & 2 Outputs
- `.planning/phases/01-foundation/01-CONTEXT.md` -- Phase 1 decisions (cert, MSIX, COM DLL architecture)
- `.planning/phases/02-launcher-and-config/02-CONTEXT.md` -- Phase 2 decisions (config app, registry, path detection)
- `.planning/phases/01-foundation/01-02-SUMMARY.md` -- Dev registration workflow, patterns established
- `.planning/phases/02-launcher-and-config/02-03-SUMMARY.md` -- Build integration, register.ps1 structure

### Existing Scripts (adapt for installer)
- `scripts/register.ps1` -- Full registration workflow (cert -> pack -> sign -> register -> restart Explorer). Steps 3-7 are the template for installer post-deploy actions.
- `scripts/unregister.ps1` -- Unregistration workflow (remove MSIX, optionally remove cert)

### Package Definition
- `package/AppxManifest.xml` -- MSIX manifest with COM server and context menu extensions
- `package/Assets/` -- Package visual assets

### Project Documentation
- `CLAUDE.md` -- Technology stack, Inno Setup version (6.7.1), certificate strategy, build artifact structure

### Reference: Scanner Installer (user's preferred style)
- `D:\Working\Projects\scanner\Installer\Product.wxs` -- User's other project installer (WiX-based, for reference on structure)
- `D:\Working\Projects\scanner\build-installer.ps1` -- Build script pattern

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `scripts/register.ps1`: Steps 3-7 (pack MSIX, sign, unregister old, register new, restart Explorer) can be adapted to PowerShell inline in Inno Setup
- `scripts/unregister.ps1`: Unregistration logic reusable for uninstaller
- `build/` directory: Contains all compiled artifacts ready to package

### Established Patterns
- MSIX registration via `Add-AppxPackage -Path $msix -ExternalLocation $installDir`
- MSIX unregistration via `Get-AppxPackage -Name "ClaudeFromHere" | Remove-AppxPackage`
- Explorer restart via `Stop-Process -Name explorer -Force`

### Integration Points
- Installer must set `-ExternalLocation` to the install directory (`%LOCALAPPDATA%\ClaudeFromHere\`)
- AppxManifest.xml `uap10:AllowExternalContent=true` enables external DLL location
- Config app and DLL share `HKCU\Software\ClaudeFromHere` registry key (not managed by installer)

</code_context>

<specifics>
## Specific Ideas

- Azure Trusted Signing is already configured and available -- use it for MSIX signing at build time
- No admin/UAC required at all -- purely per-user install
- Adapt the working register.ps1 flow rather than reinventing MSIX registration
- Support silent install for scripted/enterprise deployment

</specifics>

<deferred>
## Deferred Ideas

- **Config app UI redesign**: User wants a polished dark-theme WPF app (reference: Agrus Scanner at `D:\Working\Projects\scanner\`) instead of the current bland WinForms. Should be resizable. Consider as Phase 2.1 or post-v1.
- **Config app content redesign**: Replace model dropdown with practical CLI flags like `--dangerously-skip-permissions`, MCP/telegram configs. More useful toggles for real workflows.
- **Classic context menu toggle**: Registry key `{86ca1aa0-34aa-4e8b-a509-50c905bae2a2}` can force legacy Win 11 menu. Could be a checkbox in the config app.

</deferred>

---

*Phase: 03-installer*
*Context gathered: 2026-04-10*
