<!-- GSD:project-start source:PROJECT.md -->
## Project

**Claude From Here**

A Windows 11 shell extension that adds "Claude from here" to the top-level right-click context menu in File Explorer. When clicked, it opens Claude Code in the selected directory via Windows Terminal. Distributed as a polished installer for public use.

**Core Value:** Right-click any folder or folder background in Windows 11 Explorer → Claude Code opens in that directory. One click, no terminal juggling.

### Constraints

- **Platform**: Windows 11 only (modern context menu requires Win 11)
- **No compilation dependencies for users**: Installer should be self-contained, users shouldn't need dev tools
- **MSIX signing**: Sparse packages may need self-signing or a workaround for unsigned packages
- **Registry scope**: Use HKEY_CURRENT_USER (no admin required for per-user install if possible)
<!-- GSD:project-end -->

<!-- GSD:stack-start source:research/STACK.md -->
## Technology Stack

## The Core Constraint
## Recommended Stack
### Core Technologies
| Technology | Version | Purpose | Why Recommended |
|------------|---------|---------|-----------------|
| C++ (MSVC or MinGW-w64) | C++17 | Shell extension DLL (`IExplorerCommand`) | Required: Microsoft explicitly recommends C++ for in-process shell extensions; managed code (.NET/C#) is explicitly discouraged due to CLR loading risks in Explorer's process |
| Windows SDK | 10.0.26100.0 (Win 11 24H2) | Shell APIs, COM, `IExplorerCommand`, `IShellItemArray` | Required to build against current Windows shell interfaces |
| `MakeAppx.exe` | Bundled with Windows SDK | Packs AppxManifest.xml + assets into `.msix` | Official Microsoft tool for creating MSIX/sparse packages; `/nv` flag skips validation so no payload files are needed |
| `SignTool.exe` | Bundled with Windows SDK | Signs the `.msix` with SHA-256 | Required: unsigned MSIX cannot be registered; must match the `Publisher` CN in AppxManifest |
| Inno Setup | 6.4+ (currently 6.7.1) | `.exe` installer wrapper | Free, scriptable, produces a single self-contained EXE; can run PowerShell inline to import cert and register MSIX; widely trusted by users |
### AppxManifest Extension Schema
| Namespace | Prefix | Purpose |
|-----------|--------|---------|
| `http://schemas.microsoft.com/appx/manifest/foundation/windows10` | (default) | Core package identity |
| `http://schemas.microsoft.com/appx/manifest/com/windows10` | `com` | Registers COM server (the DLL + CLSID) |
| `http://schemas.microsoft.com/appx/manifest/desktop/windows10/4` | `desktop4` | `windows.fileExplorerContextMenus` extension |
| `http://schemas.microsoft.com/appx/manifest/desktop/windows10/5` | `desktop5` | `ItemType="Directory"` verb targeting |
| `http://schemas.microsoft.com/appx/manifest/uap/windows10` | `uap` | `VisualElements`, `uap10:AllowExternalContent` |
| `http://schemas.microsoft.com/appx/manifest/uap/windows10/10` | `uap10` | `AllowExternalContent`, `TrustLevel`, `RuntimeBehavior` |
| `http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities` | `rescap` | `runFullTrust`, `unvirtualizedResources` |
- `com:Extension` (`windows.comServer`) — registers the DLL CLSID as a surrogate COM server
- `desktop4:Extension` (`windows.fileExplorerContextMenus`) with `desktop5:ItemType Type="Directory"` — wires the CLSID verb to folder right-clicks
### Supporting Libraries / Tools
| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| `New-SelfSignedCertificate` (PowerShell) | Built-in Win 11 | Generates self-signed PFX for dev/testing | Development and public distribution (with user-side cert import) |
| Azure Trusted Signing | Current (GA in 2024) | CA-backed code signing; no cert management | If the project grows and SmartScreen reputation matters; costs ~$10/month; restricted to US/Canada businesses |
| Windows SDK (full) | 10.0.26100.0 | Provides `MakeAppx.exe`, `SignTool.exe`, C++ headers | Build machine only; not shipped |
| Visual Studio 2022 Build Tools (MSVC) | 17.x | C++ compiler + linker for the COM DLL | Produces smaller, more compatible binaries than MinGW for in-process COM |
| CMake | 3.28+ | Cross-platform build system for the DLL | Optional; VS project files are simpler for a single-DLL project |
### Installer Stack Detail
## Certificate Strategy for Public Distribution
- Cert import into `Cert:\CurrentUser\TrustedPeople` alone is insufficient — MSIX verifier checks machine store
- Workaround: Inno Setup can request elevation just for the cert import step, then register the package per-user
- Alternative: Skip MSIX signing entirely and use `Add-AppxPackage -AllowUnsigned` — available from Windows 11 22H2+ for developer scenarios but **not** a viable public distribution strategy
## Alternatives Considered
| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| C++ COM DLL | C# / .NET COM server | Microsoft explicitly discourages managed in-process shell extensions; CLR loading into Explorer can cause instability and hangs; COM interop adds friction |
| C++ COM DLL | Pure manifest approach (no DLL) | Impossible for Directory verb targets — `uap2:SupportedVerbs` only works for file type associations the app owns |
| Inno Setup | NSIS | Both are valid; Inno Setup has a cleaner Pascal-like scripting model, active 2025 development (6.7.1), and better Unicode/modern Windows support |
| Inno Setup | WiX Toolset v4 | WiX produces MSI/MSIX natively but has a steep learning curve; overkill for a single context menu extension |
| Inno Setup | Advanced Installer | Commercial license required for context menu features; unnecessary cost for an open-source community tool |
| MakeAppx.exe | Advanced Installer GUI | Advanced Installer wraps MSIX packaging but is commercial; MakeAppx.exe is free, scriptable, in the SDK |
| Self-signed cert + cert import | Azure Trusted Signing | Useful for SmartScreen reputation but requires US/Canada business verification and monthly cost; overkill for v1 |
## What NOT to Use
| Avoid | Why | Use Instead |
|-------|-----|-------------|
| Registry-only approach (`.reg` files) | Since Windows 11 21H2, registry-registered context menu items appear under "Show more options" only — never at the top level of the modern menu | Sparse MSIX + COM DLL |
| `IContextMenu` (classic COM interface) | Works but only reaches the legacy menu on Win 11; `IExplorerCommand` is the modern interface for top-level placement | `IExplorerCommand` |
| `uap2:SupportedVerbs` in AppxManifest (no DLL) | Only works for file types registered to the app; cannot target generic `Directory` or folder background | `desktop4:Extension` + `desktop5:ItemType` + COM DLL |
| Full MSIX package (not sparse) | Requires the entire app to live inside the package sandbox; breaks when the app lives in arbitrary paths (user's AppData, etc.) | Sparse package with `-ExternalLocation` pointing to the install dir |
| Self-signed cert in `Cert:\CurrentUser\TrustedPeople` | MSIX installer does not check the user certificate store for package identity verification; install fails | Import to `Cert:\LocalMachine\TrustedPeople` |
| MakeCert.exe | Deprecated since Windows 10; Microsoft's own docs redirect to `New-SelfSignedCertificate` PowerShell cmdlet | `New-SelfSignedCertificate` |
## Build Artifact Structure
## Version Compatibility
| Component | Minimum | Recommended | Notes |
|-----------|---------|-------------|-------|
| Windows | 11 21H2 (10.0.22000) | 11 24H2 (10.0.26100) | Modern context menu requires Win 11; `AllowExternalContent` requires 10.0.19041.0 minimum |
| AppxManifest `MaxVersionTested` | 10.0.21300.0 | 10.0.26100.0 | Must exceed 10.0.21300.0 for Win 11 context menu features to activate |
| AppxManifest `MinVersion` | 10.0.19041.0 | 10.0.22000.0 | 19041 = Win 10 2004 (first with AllowExternalContent); 22000 = Win 11 |
| Inno Setup | 6.4.0 | 6.7.1 | 6.4+ for ISSigTool; 6.7.1 is latest stable as of April 2026 |
| Windows SDK | 10.0.22621.0 | 10.0.26100.0 | Either works; 26100 matches Win 11 24H2 headers |
## Installation Command Reference
# Create self-signed cert (run once, on build machine)
# Export to PFX
# Pack the sparse MSIX (no payload files, only manifest + assets)
# Sign with SHA-256
# Install (Inno Setup runs these via PowerShell inline)
# Uninstall
## Sources
- [Microsoft Docs: Integrate a packaged desktop app with File Explorer](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/integrate-packaged-app-with-file-explorer) — Authoritative; `desktop4`/`desktop5` schema, COM DLL approach, `Directory` verb targeting — HIGH confidence
- [Microsoft Docs: Grant package identity by packaging with external location](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps) — Authoritative; full sparse package manifest template, build/sign/register workflow — HIGH confidence
- [Microsoft Docs: Creating Shortcut Menu Handlers](https://learn.microsoft.com/en-us/windows/win32/shell/context-menu-handlers) — Authoritative; `IExplorerCommand` vs `IContextMenu`, Win 11 requirements — HIGH confidence
- [GitHub: microsoft/vscode-explorer-command](https://github.com/microsoft/vscode-explorer-command) — VS Code's reference C++ implementation of `IExplorerCommand` for Win 11 via sparse pkg — HIGH confidence
- [Windows Developer Blog: Extending the Context Menu and Share Dialog in Windows 11](https://blogs.windows.com/windowsdeveloper/2021/07/19/extending-the-context-menu-and-share-dialog-in-windows-11/) — Official Microsoft announcement; confirms `IExplorerCommand` + sparse package is the canonical approach — HIGH confidence
- [Microsoft Docs: Create a certificate for package signing](https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing) — `New-SelfSignedCertificate` approach; `SignTool` usage — HIGH confidence
- [xplorer² blog: Packaged DLL for Windows 11 Explorer Context Menu](https://www.zabkat.com/blog/win11-explorer-menu-package.htm) — Real-world implementation walkthrough; confirms `MAKEAPPX` + `SIGNTOOL` + PowerShell registration pattern — MEDIUM confidence
- [Advanced Installer: Shell Context Menu Support in MSIX Packaging](https://www.advancedinstaller.com/msix-shell-context-menu.html) — Confirms manifest namespace requirements; signing is mandatory — MEDIUM confidence
- [GitHub: WindowsAppSDK issue #1082](https://github.com/microsoft/WindowsAppSDK/issues/1082) — Confirms no manifest-only approach exists for non-owned file types; `IExplorerCommand` is required — MEDIUM confidence
- [Inno Setup Downloads](https://jrsoftware.org/isdl.php) — Current version 6.7.1 confirmed — HIGH confidence
<!-- GSD:stack-end -->

<!-- GSD:conventions-start source:CONVENTIONS.md -->
## Conventions

Conventions not yet established. Will populate as patterns emerge during development.
<!-- GSD:conventions-end -->

<!-- GSD:architecture-start source:ARCHITECTURE.md -->
## Architecture

Architecture not yet mapped. Follow existing patterns found in the codebase.
<!-- GSD:architecture-end -->

<!-- GSD:workflow-start source:GSD defaults -->
## GSD Workflow Enforcement

Before using Edit, Write, or other file-changing tools, start work through a GSD command so planning artifacts and execution context stay in sync.

Use these entry points:
- `/gsd:quick` for small fixes, doc updates, and ad-hoc tasks
- `/gsd:debug` for investigation and bug fixing
- `/gsd:execute-phase` for planned phase work

Do not make direct repo edits outside a GSD workflow unless the user explicitly asks to bypass it.
<!-- GSD:workflow-end -->



<!-- GSD:profile-start -->
## Developer Profile

> Profile not yet configured. Run `/gsd:profile-user` to generate your developer profile.
> This section is managed by `generate-claude-profile` -- do not edit manually.
<!-- GSD:profile-end -->
