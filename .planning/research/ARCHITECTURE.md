# Architecture Research

**Domain:** Windows 11 Shell Extension — Sparse MSIX context menu with EXE installer
**Researched:** 2026-04-08
**Confidence:** HIGH (sourced from official Microsoft documentation and verified examples)

## Standard Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                      INSTALLER (EXE)                             │
│  ┌──────────────────┐  ┌────────────────┐  ┌─────────────────┐  │
│  │  Path Detection  │  │  File Deploy   │  │ MSIX Register   │  │
│  │  (claude, wt)    │  │  (assets, pkg) │  │ Add-AppxPackage │  │
│  └──────────────────┘  └────────────────┘  └─────────────────┘  │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼ deploys to
┌─────────────────────────────────────────────────────────────────┐
│                   INSTALL DIRECTORY (~\AppData\Local\...)        │
│  ┌───────────────┐  ┌────────────────┐  ┌────────────────────┐  │
│  │ AppxManifest  │  │  launcher.bat  │  │  Assets/           │  │
│  │ .xml          │  │  (or .ps1/.cmd)│  │  claude-icon.png   │  │
│  └───────────────┘  └────────────────┘  └────────────────────┘  │
│  ┌───────────────┐                                               │
│  │ ClaudeFrom    │                                               │
│  │ Here.msix     │  (signed sparse package referencing above)    │
│  └───────────────┘                                               │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼ registered with
┌─────────────────────────────────────────────────────────────────┐
│                   WINDOWS PACKAGE MANAGER                        │
│  (AppxPackage registry — per-user, no admin required)            │
│                                                                  │
│  ExternalLocation → install directory                            │
│  PackageIdentity  → ClaudeFromHere_1.0.0.0_neutral               │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼ enables
┌─────────────────────────────────────────────────────────────────┐
│                   WINDOWS 11 CONTEXT MENU                        │
│  Right-click Directory → "Claude from here"    [icon]            │
│  Right-click Dir Background → "Claude from here"  [icon]         │
└─────────────────────────────────────────────────────────────────┘
                               │
                               ▼ user clicks
┌─────────────────────────────────────────────────────────────────┐
│                   LAUNCH CHAIN                                   │
│                                                                  │
│  manifest verb → launcher script                                 │
│      → wt.exe -d "<selected directory>" cmd /k claude            │
└─────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Typical Implementation |
|-----------|----------------|------------------------|
| AppxManifest.xml | Declares package identity, context menu verbs, icon, verb→command binding | Hand-authored XML, no tooling required |
| Sparse MSIX package | Carries the manifest + assets; grants package identity to the install dir | Built with `MakeAppx.exe /nv`, signed with `SignTool.exe` |
| Launcher script | Resolves and executes `wt.exe -d "<dir>" cmd /k claude`; handles errors | `.bat` or `.ps1` file in install dir |
| Icon asset | PNG shown next to menu item in Explorer | `logo.png` (150x150 recommended), referenced by manifest |
| Installer (EXE) | Detects paths, copies files, signs+registers MSIX package, runs uninstaller | Inno Setup or NSIS script |
| Uninstaller | Removes MSIX registration (`Remove-AppxPackage`), deletes files | Inno Setup uninstall section or bundled script |

## Recommended Project Structure

```
claude-from-here/
├── package/                    # Sparse MSIX package sources
│   ├── AppxManifest.xml        # Core manifest — declares identity + context menu verbs
│   └── Assets/
│       └── logo.png            # Icon shown in context menu (also Square150x150Logo)
│
├── launcher/
│   └── launch.bat              # Executes wt.exe -d "%1" cmd /k claude
│
├── installer/
│   └── setup.iss               # Inno Setup script (or setup.nsi for NSIS)
│
├── scripts/
│   ├── build-msix.ps1          # Calls MakeAppx.exe + SignTool.exe → ClaudeFromHere.msix
│   ├── create-cert.ps1         # Creates self-signed cert for dev builds
│   └── register-package.ps1    # Calls Add-AppxPackage -ExternalLocation (for dev testing)
│
├── cert/
│   └── dev-cert.pfx            # Self-signed cert for dev/CI (gitignored in public builds)
│
└── dist/                       # Build output (gitignored)
    ├── ClaudeFromHere.msix
    └── ClaudeFromHereSetup.exe
```

### Structure Rationale

- **package/:** All sources that go into or are referenced by the MSIX. `AppxManifest.xml` lives here so `MakeAppx.exe` can pack it in one pass. Assets/ mirrors the path expected by the manifest's `Logo` attributes.
- **launcher/:** Kept separate from package/ because it is deployed to the external location (the install dir), not packed inside the MSIX. The manifest references it by relative path via `ExternalLocation`.
- **installer/:** The Inno Setup script that orchestrates everything — it copies files, calls `build-msix.ps1` (or ships a pre-built MSIX), registers the package, and wires up the uninstaller.
- **scripts/:** Build and dev-time automation. Keeps PowerShell logic out of the installer script and makes individual steps testable.
- **dist/:** Clean output directory for CI artifacts — the `.exe` and `.msix` that get attached to a GitHub release.

## Architectural Patterns

### Pattern 1: Manifest-Only Sparse Package (no COM DLL)

**What:** The AppxManifest declares context menu verbs that launch an executable directly via `Application` element + `uap3:Verb`, bypassing the COM IExplorerCommand interface entirely.

**When to use:** When the action is "launch an executable with the selected path as an argument." This covers the "Claude from here" use case exactly — no in-process DLL is needed.

**Trade-offs:** Simpler (no C++ DLL), but the verb launches the registered Application executable, so the launcher script must be declared as the `Executable` in the manifest's `<Application>` element.

**Manifest structure:**
```xml
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:uap2="http://schemas.microsoft.com/appx/manifest/uap/windows10/2"
  xmlns:uap3="http://schemas.microsoft.com/appx/manifest/uap/windows10/3"
  xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap uap2 uap3 uap10 rescap">

  <Identity Name="ClaudeFromHere" Publisher="CN=YourName"
            Version="1.0.0.0" ProcessorArchitecture="neutral" />

  <Properties>
    <DisplayName>Claude from here</DisplayName>
    <PublisherDisplayName>YourName</PublisherDisplayName>
    <Logo>Assets\logo.png</Logo>
    <uap10:AllowExternalContent>true</uap10:AllowExternalContent>
  </Properties>

  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="10.0.22000.0"
                        MaxVersionTested="10.0.26100.0" />
  </Dependencies>

  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
    <rescap:Capability Name="unvirtualizedResources" />
  </Capabilities>

  <Applications>
    <Application Id="ClaudeFromHere" Executable="launch.bat"
                 uap10:TrustLevel="mediumIL" uap10:RuntimeBehavior="win32App">
      <uap:VisualElements AppListEntry="none" DisplayName="Claude from here"
        Description="Open Claude Code in selected folder"
        BackgroundColor="transparent"
        Square150x150Logo="Assets\logo.png"
        Square44x44Logo="Assets\logo.png" />
      <Extensions>
        <uap3:Extension Category="windows.fileTypeAssociation">
          <!-- Directory verb (right-click a folder) -->
        </uap3:Extension>
      </Extensions>
    </Application>
  </Applications>
</Package>
```

### Pattern 2: Desktop4/Desktop5 FileExplorerContextMenus Verb (for Directory + Background)

**What:** Uses `desktop4:FileExplorerContextMenus` with `desktop5:ItemType Type="Directory"` and `Type="Directory\Background"` to register verbs for both folder right-click and inside-folder background right-click. This requires a COM CLSID, which means a companion DLL — but for simple command dispatch, Microsoft provides a workaround via `uap3:Verb` on the Application instead.

**When to use:** Mandatory if you need both "right-click folder" and "right-click background inside folder" to appear in the Windows 11 modern menu. The `Directory` type covers folders in the nav pane; `Directory\Background` covers the white space inside an open folder.

**Trade-offs:** The cleanest manifest-only approach for simple launch commands uses `uap3:FileTypeAssociation` targeting `Directory`, but covering `Directory\Background` may require a COM surrogate or a separate manifest extension. Research this during implementation — it is the main technical unknown.

### Pattern 3: Per-User Registration via Add-AppxPackage -ExternalLocation

**What:** The installer registers the sparse package by calling:
```powershell
Add-AppxPackage -Path ".\ClaudeFromHere.msix" -ExternalLocation "C:\Users\...\AppData\Local\ClaudeFromHere"
```
The `-ExternalLocation` tells Windows where the files referenced by the manifest live. The package itself contains only `AppxManifest.xml` and `Assets/`.

**When to use:** Always — this is the standard registration mechanism for sparse packages.

**Trade-offs:** Per-user only (one call per user account). Machine-wide requires `-Stage` + `ProvisionPackageForAllUsersAsync`, which needs admin rights.

## Data Flow

### Install Flow

```
User runs Setup.exe
    │
    ├─→ Detect claude.exe path (search %LOCALAPPDATA%\.local\bin, PATH)
    ├─→ Detect wt.exe path (search WindowsApps, PATH)
    ├─→ Write install dir (e.g., %LOCALAPPDATA%\ClaudeFromHere\)
    │     ├── launch.bat  (with detected paths baked in, or dynamic detection)
    │     ├── AppxManifest.xml
    │     ├── Assets\logo.png
    │     └── ClaudeFromHere.msix
    └─→ Register MSIX:
          powershell Add-AppxPackage -Path <msix> -ExternalLocation <install dir>
```

### Context Menu Invocation Flow

```
User right-clicks a folder in Explorer
    │
    ▼
Windows Shell reads registered AppxPackage verbs
    │
    ▼
"Claude from here" appears in modern context menu (top level)
    │
    ▼ user clicks
Windows Shell invokes verb → executes launch.bat "%V"
    │   (%V = selected directory path)
    ▼
launch.bat resolves wt.exe path, calls:
    wt.exe -d "<selected dir>" cmd /k claude
    │
    ▼
Windows Terminal opens in target directory, runs claude
```

### Uninstall Flow

```
User runs uninstaller (from Add/Remove Programs)
    │
    ├─→ powershell Get-AppxPackage ClaudeFromHere | Remove-AppxPackage
    └─→ Delete install directory
```

### Key Data Flows

1. **Path resolution:** The installer detects `claude.exe` and `wt.exe` at install time. The launcher script either has these baked in or re-detects at runtime. Runtime re-detection is more robust for path changes but adds complexity.
2. **Directory argument:** Windows passes the selected path as `%V` (folder background) or `%1` (folder) to the verb command. The launcher must handle both forms consistently.
3. **Package identity propagation:** The MSIX `Identity` element must exactly match the signing certificate's `Subject`. Mismatch causes silent registration failure.

## Scaling Considerations

This is a single-user local tool — traditional scale concerns (users, load, throughput) do not apply. The relevant "scale" is across different Windows configurations.

| Scenario | Approach |
|---|---|
| Standard Win 11 + WT in WindowsApps | Happy path — auto-detect wt.exe from known path |
| WT installed via Scoop/Chocolatey (different path) | Fall back to PATH resolution or `where wt` at launch time |
| claude.exe in non-standard path | Search common locations; show a clear error if not found |
| Multiple user accounts on one machine | Per-user registration only covers current user; document this |
| Windows 11 24H2 strict MSIX enforcement | Sparse package still works; unsigned packages require `-AllowUnsigned` + Developer Mode (not suitable for distribution) — must sign |

## Anti-Patterns

### Anti-Pattern 1: Hardcoding Paths in the Manifest

**What people do:** Put absolute paths to `wt.exe` or `claude.exe` directly in `AppxManifest.xml` or embedded in the MSIX.

**Why it's wrong:** The manifest's `ExternalLocation` already resolves relative paths against the install directory. Absolute paths in the manifest make the package non-portable across user accounts and break when users have non-standard installs.

**Do this instead:** Bake the install-time-detected paths into `launch.bat` during installation, or have `launch.bat` perform its own detection at runtime with a helpful error message on failure.

### Anti-Pattern 2: Using a COM DLL for a Simple Launch Command

**What people do:** Implement the full `IExplorerCommand` COM interface in a C++ DLL because tutorials show this pattern.

**Why it's wrong:** Compiling a DLL requires C++ toolchain knowledge, introduces a binary distribution artifact, and adds complexity around architecture (x64 vs ARM64). For a command that just runs `wt.exe -d "<dir>" cmd /k claude`, this is massive overkill.

**Do this instead:** Use the manifest-only verb approach with a `.bat` launcher. The sparse package grants identity without requiring any in-process COM component.

### Anti-Pattern 3: Skipping Signing for Public Distribution

**What people do:** Ship the MSIX unsigned, relying on `-AllowUnsigned` or Developer Mode.

**Why it's wrong:** `-AllowUnsigned` requires users to enable Developer Mode system-wide, which is a security posture change that no reasonable end user should make for a community tool. Windows will also block installation on non-developer machines entirely.

**Do this instead:** Sign with a self-signed cert for development/testing. For public distribution, use Azure Trusted Signing (low cost, no hardware token required) or any code signing cert. The installer must install the cert to the user's Trusted Root store if using self-signed, or ship a cert the user trusts.

### Anti-Pattern 4: Targeting HKEY_LOCAL_MACHINE Registry

**What people do:** Write context menu registry keys to `HKLM` to cover all users, requiring admin elevation.

**Why it's wrong:** This project's stated goal is per-user install with no admin prompt. `HKCU` registry writes (for the classic menu fallback) and per-user `Add-AppxPackage` (for the modern menu) both work without elevation.

**Do this instead:** Restrict all writes to `HKCU` and use per-user `Add-AppxPackage` registration.

## Integration Points

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| Windows Package Manager (AppX) | `Add-AppxPackage` PowerShell cmdlet | Called by installer; no network required |
| Windows Terminal (`wt.exe`) | Process launch via `launch.bat` | Typically at `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` or `%PROGRAMFILES%\WindowsApps\...` |
| Claude Code CLI (`claude.exe`) | Process invocation via `wt.exe` | Typically at `~\.local\bin\claude.exe`; must be in PATH or detected |
| SignTool.exe / MakeAppx.exe | Build-time only — Windows SDK tools | Required in CI/CD pipeline; not needed by end users |
| Azure Trusted Signing (optional) | Code signing for public release | Replaces self-signed cert; integrates with GitHub Actions |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| Installer ↔ MSIX package | Installer copies `.msix` file to install dir, then registers it via PowerShell | The `.msix` is a static build artifact; installer does not modify it |
| AppxManifest ↔ launch.bat | Manifest `Executable=` attribute names `launch.bat`; Windows invokes it directly | Both must be in the same `ExternalLocation` directory |
| launch.bat ↔ Windows Terminal | Shell `start` command or direct invocation of `wt.exe` with `-d` flag | Pass `%1` or `%V` as the directory argument |
| Manifest ↔ signing cert | `Publisher` in `<Identity>` must exactly match cert `Subject` | Mismatch causes silent registration failure — test this early |

## Build Order Implications

Dependencies flow in this order, which should drive phase sequencing:

1. **AppxManifest.xml + Assets** — must exist before MSIX can be built; defines what the context menu looks like and how verbs work. Build and validate this first.
2. **launch.bat** — must exist before the manifest verb can be tested end-to-end. Keep simple initially (hardcoded paths), make dynamic later.
3. **Signing cert + MakeAppx/SignTool pipeline** — needed before any real-machine registration test. Dev self-signed cert is fine at this stage.
4. **MSIX registration (manual PowerShell)** — validate the full install chain works on a real Win 11 machine before building the installer.
5. **Installer (Inno Setup/NSIS)** — wraps everything above into the end-user deliverable. Build last, after all components are individually validated.
6. **Path detection logic** — can be developed in parallel with the installer; integrate during step 5.

## Sources

- [Grant package identity by packaging with external location (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps) — Authoritative, updated 2025-07-02. PRIMARY reference for manifest structure, registration, and signing.
- [Integrate a packaged desktop app with File Explorer (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/integrate-packaged-app-with-file-explorer) — Desktop4/Desktop5 ItemType patterns for Directory and Directory\Background verbs.
- [Create an unsigned MSIX package (Microsoft Learn)](https://learn.microsoft.com/en-us/windows/msix/package/unsigned-package) — `-AllowUnsigned` flag details and limitations.
- [Shell Context Menu Support in MSIX Packaging (Advanced Installer)](https://www.advancedinstaller.com/msix-shell-context-menu.html) — Community resource on sparse package context menus.
- [MSIX with Context Menus with caveat (tmurgent.com)](https://www.tmurgent.com/TmBlog/?p=3376) — Practitioner notes on caveats and gotchas.
- [Windows 11 Shell Menu using Sparse Package and multiple Users (Microsoft Q&A)](https://learn.microsoft.com/en-us/answers/questions/1094326/windows-11-shell-menu-using-sparse-package-and-mul) — Per-user vs. per-machine registration discussion.
- [open-with-claude-code (GitHub, avelops)](https://github.com/avelops/open-with-claude-code) — Existing registry-based approach; confirms sparse MSIX is the missing piece for Win 11 modern menu.

---
*Architecture research for: Windows 11 sparse MSIX shell extension (Claude From Here)*
*Researched: 2026-04-08*
