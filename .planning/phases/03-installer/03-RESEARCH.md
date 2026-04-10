# Phase 3: Installer - Research

**Researched:** 2026-04-10
**Domain:** Inno Setup 6.x, Azure Artifact Signing, MSIX sparse package registration, per-user Windows installer
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Use Azure Trusted Signing (already configured) to sign the MSIX package at build time. No self-signed certificate, no cert import at install time, no SmartScreen warnings.
- **D-02:** The installer ships a pre-signed MSIX — signing happens during the build process, not at install time.
- **D-03:** No admin required. The entire installer runs per-user without UAC prompts. Azure Trusted Signing eliminates the cert import step that was the only reason for admin elevation.
- **D-04:** Files install to `%LOCALAPPDATA%\ClaudeFromHere\`. Per-user, no admin needed.
- **D-05:** Files deployed: `ClaudeFromHere.dll`, `ClaudeFromHere.exe` (stub), `ClaudeFromHereConfig.exe`, `claude.ico`, `AppxManifest.xml`, `Assets\` folder, `ClaudeFromHere.msix` (pre-signed).
- **D-06:** Create a Start Menu shortcut for `ClaudeFromHereConfig.exe` labeled "Claude From Here Settings".
- **D-07:** Upgrade installs overwrite in place. Installer detects existing installation, unregisters old MSIX, deploys new files, re-registers new MSIX. User config in `HKCU\Software\ClaudeFromHere` preserved automatically.
- **D-08:** Uninstall removes MSIX registration, all installed files, and Start Menu shortcut. Leave `HKCU\Software\ClaudeFromHere` intact.
- **D-09:** Minimal wizard: Welcome page -> Install -> Done. No unnecessary options screens.
- **D-10:** Support `/SILENT` and `/VERYSILENT` flags for power users and scripted deployment.
- **D-11:** Final page tells the user Explorer needs to restart; user clicks button to restart it, then wizard finishes. Silent mode restarts Explorer automatically.

### Claude's Discretion
- Inno Setup script structure and helper functions
- Exact MSIX registration PowerShell commands (adapt from register.ps1)
- Welcome page branding and text
- Build script that signs with Azure Trusted Signing before packaging

### Deferred Ideas (OUT OF SCOPE)
- Config app UI redesign (dark-theme WPF)
- Config app content redesign (practical CLI flags)
- Classic context menu toggle registry key
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| INST-01 | Single `.exe` installer handles all MSIX registration, cert import, and file deployment | Inno Setup [Files] + [Run] PowerShell inline for Add-AppxPackage; no cert import needed with Azure Trusted Signing (D-01/D-02) |
| INST-02 | Clean uninstall removes all MSIX registration, files, and registry entries | Inno Setup [UninstallRun] + [UninstallDelete] sections; adapt unregister.ps1 logic |
| INST-03 | Installer works across different Windows 11 setups | PrivilegesRequired=lowest + {localappdata} constant; per-user MSIX registration via Add-AppxPackage |
| INST-04 | Upgrade installs work without manual uninstall first | Same AppId + [Run] pre-install step to Remove-AppxPackage existing; Inno Setup overwrites files with same AppId |
</phase_requirements>

---

## Summary

Phase 3 produces a single `ClaudeFromHere-Setup.exe` using Inno Setup 6.7 that: (1) deploys all files to `%LOCALAPPDATA%\ClaudeFromHere\`, (2) registers the pre-signed MSIX via `Add-AppxPackage -ExternalLocation`, (3) creates a Start Menu shortcut, and (4) restarts Explorer. The key insight from the CONTEXT.md decisions is that Azure Trusted Signing eliminates the previous design's only reason for admin elevation — no cert import to LocalMachine\TrustedPeople is needed. The installer runs fully per-user (`PrivilegesRequired=lowest`).

The build process has two distinct stages: first a **build script** (`build-installer.ps1`) that compiles artifacts and signs the MSIX with Azure Artifact Signing using the `sign` dotnet tool, then packages them into a `.exe` via Inno Setup's ISCC.exe. The `sign` tool is already installed globally (`sign 0.9.1-beta.26179.1`) and the same signing pattern is already proven in the scanner sibling project. Inno Setup itself is **not currently installed** on the build machine — the Wave 0 task must install it.

The upgrade path is clean: Inno Setup's same-AppId install naturally overwrites files; the installer adds a `[Run]` pre-install step to `Remove-AppxPackage` any existing registration before `Add-AppxPackage`, making the MSIX registration idempotent.

**Primary recommendation:** One Inno Setup `.iss` script (`installer/ClaudeFromHere.iss`) + one build script (`build-installer.ps1`). The `.iss` uses `PrivilegesRequired=lowest`, `{localappdata}` as install root, and `[Run]` / `[UninstallRun]` for PowerShell MSIX registration. The build script signs MSIX before calling ISCC.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Inno Setup | 6.7.1 | Produces the `.exe` installer | Project-mandated (CLAUDE.md); free, scriptable, active 2025 development |
| Inno Setup Compiler (ISCC.exe) | 6.7.1 | CLI compiler for `.iss` scripts | Bundled with Inno Setup; call from build script |
| `sign` dotnet tool | 0.9.1-beta.26179.1 | MSIX signing via Azure Artifact Signing | Already installed globally; same tool used in scanner sibling project |
| Azure CLI | (already authenticated) | `az account set` for subscription context during signing | Already authenticated (`az account show` returns valid account) |
| `Add-AppxPackage` / `Remove-AppxPackage` | PowerShell 5.1+ built-in | MSIX sparse package register/unregister | Established pattern from register.ps1 and unregister.ps1 |

### Supporting
| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| MakeAppx.exe | Windows SDK 10.0.26100.0 | Pack `AppxManifest.xml` + Assets into `.msix` | Build step before signing; already proven in register.ps1 |
| SignTool.exe | Windows SDK 10.0.26100.0 | Alternative signing path (not used here — `sign` tool used instead) | Available but not used; `sign` dotnet tool is already configured |

**Installation (build machine only):**
```
winget install --id JRSoftware.InnoSetup
# or download installer from: https://jrsoftware.org/isdl.php
```

**Default ISCC path after install:**
```
C:\Program Files (x86)\Inno Setup 6\ISCC.exe
```

---

## Architecture Patterns

### Recommended Project Structure
```
installer/
├── ClaudeFromHere.iss       # Inno Setup script
├── LICENSE.rtf              # License text shown in wizard (optional)
└── assets/                  # Any installer-specific images (banner.bmp etc.)
build-installer.ps1          # Orchestrates: compile -> pack MSIX -> sign -> ISCC
build/                       # Compiled artifacts (DLL, EXEs, MSIX, icons)
```

### Pattern 1: Per-User Inno Setup Script (PrivilegesRequired=lowest)

**What:** Core Inno Setup configuration for a per-user install to `%LOCALAPPDATA%\ClaudeFromHere\`.

**When to use:** Any installer that must run without UAC — no writes to HKLM or Program Files.

```pascal
[Setup]
AppId={{YOUR-GUID-HERE}
AppName=Claude From Here
AppVersion=1.0.0
AppPublisher=Claude From Here
DefaultDirName={localappdata}\ClaudeFromHere
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=commandline
OutputBaseFilename=ClaudeFromHere-Setup
Compression=lzma
SolidCompression=yes
DisableDirPage=yes
DisableProgramGroupPage=yes
```

Key directives:
- `PrivilegesRequired=lowest` — no elevation requested; maps directory constants to user-scoped paths
- `DefaultDirName={localappdata}\ClaudeFromHere` — installs to `C:\Users\<user>\AppData\Local\ClaudeFromHere\`
- `DisableDirPage=yes` — hides install path selection (location is fixed per D-04)
- `DisableProgramGroupPage=yes` — hides program group selection (handled by [Icons] section)

### Pattern 2: File Deployment Section

**What:** Deploy all payload files from the build output directory.

```pascal
[Files]
Source: "..\build\ClaudeFromHere.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHere.exe";     DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHereConfig.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\ClaudeFromHereConfig.exe.config"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\claude.ico";             DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\AppxManifest.xml";       DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\Assets\*";               DestDir: "{app}\Assets"; Flags: ignoreversion recursesubdirs
Source: "..\build\ClaudeFromHere.msix";    DestDir: "{app}"; Flags: ignoreversion
```

`{app}` resolves to the `DefaultDirName` value (`{localappdata}\ClaudeFromHere`).

### Pattern 3: MSIX Registration via Inline PowerShell

**What:** Post-install [Run] section that calls Add-AppxPackage. Mirrors the proven pattern from register.ps1 Steps 5-6.

**When to use:** Any Inno Setup installer that must register a sparse MSIX package.

```pascal
[Run]
; Step 1: Remove any existing registration (idempotent, handles upgrades)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""$pkg = Get-AppxPackage -Name 'ClaudeFromHere' -ErrorAction SilentlyContinue; if ($pkg) {{ Remove-AppxPackage -Package $pkg.PackageFullName }}"""; \
  Flags: runhidden waitprocuntilterminated; \
  StatusMsg: "Registering shell extension..."

; Step 2: Register new MSIX with ExternalLocation pointing to install dir
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Add-AppxPackage -Path '{app}\ClaudeFromHere.msix' -ExternalLocation '{app}'"""; \
  Flags: runhidden waitprocuntilterminated; \
  StatusMsg: "Registering shell extension..."

; Step 3: Restart Explorer (interactive mode: button on final page)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue"""; \
  Flags: runhidden waitprocuntilterminated skipifsilent; \
  Description: "Restart Explorer now (required to activate context menu)"; \
  StatusMsg: "Restarting Explorer..."

; Step 3 (silent mode): Restart Explorer automatically
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue"""; \
  Flags: runhidden waitprocuntilterminated skipifnotsilent
```

**Important:** Inno Setup escapes `{` and `}` in Parameters as `{{` and `}}` inside `[Run]` entries. PowerShell command inside double-quoted string uses `{{` for literal `{`.

**D-11 implementation note:** The `skipifsilent` + `Description` combination puts a checkbox on the final wizard page ("Restart Explorer now"). The `skipifnotsilent` duplicate entry handles silent mode where Explorer restarts unconditionally without user interaction.

### Pattern 4: MSIX Unregistration on Uninstall

**What:** [UninstallRun] section that removes MSIX registration and restarts Explorer.

```pascal
[UninstallRun]
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""$pkg = Get-AppxPackage -Name 'ClaudeFromHere' -ErrorAction SilentlyContinue; if ($pkg) {{ Remove-AppxPackage -Package $pkg.PackageFullName }}"""; \
  Flags: runhidden waitprocuntilterminated; \
  RunOnceId: "UnregisterMSIX"

Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue"""; \
  Flags: runhidden waitprocuntilterminated; \
  RunOnceId: "RestartExplorer"
```

Per D-08, `HKCU\Software\ClaudeFromHere` is **not** deleted by the uninstaller — Inno Setup will not touch registry keys not listed in a `[Registry]` section with `uninsdeletekey`.

### Pattern 5: Start Menu Shortcut

**What:** [Icons] section entry for the config app.

```pascal
[Icons]
Name: "{userprograms}\Claude From Here\Claude From Here Settings"; \
  Filename: "{app}\ClaudeFromHereConfig.exe"; \
  IconFilename: "{app}\claude.ico"
```

`{userprograms}` resolves to the current user's Programs folder in the Start Menu (correct for `PrivilegesRequired=lowest`). Does not require `{autoprograms}` since this is always a per-user install.

### Pattern 6: Build Script Structure (build-installer.ps1)

```powershell
# 1. Build DLL (CMake) and config app (dotnet)
# 2. Copy assets to build/
# 3. Pack MSIX: MakeAppx.exe pack /o /d package/ /nv /p build/ClaudeFromHere.msix
# 4. Sign MSIX: sign code artifact-signing build\ClaudeFromHere.msix
#      --artifact-signing-endpoint $env:SIGNING_ENDPOINT
#      --artifact-signing-account   $env:SIGNING_ACCOUNT
#      --artifact-signing-certificate-profile $env:SIGNING_PROFILE
#      --azure-credential-type azure-cli
# 5. Run ISCC: & "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" installer\ClaudeFromHere.iss
# 6. Report output path
```

Environment variables for signing (follow scanner pattern):
- `CFH_SIGNING_ENDPOINT` — e.g. `https://eus.codesigning.azure.net/`
- `CFH_SIGNING_ACCOUNT` — Artifact Signing account name
- `CFH_SIGNING_PROFILE` — Certificate profile name

### Anti-Patterns to Avoid

- **`PrivilegesRequired=admin`:** Forces UAC prompt — eliminated by Azure Trusted Signing (no cert import needed).
- **Hardcoded `%LOCALAPPDATA%` string in Inno Setup:** Use `{localappdata}` constant — Inno Setup resolves it correctly per user.
- **`Add-AppxPackage` without first removing existing:** Will fail on upgrade if same package name is already registered. Always `Remove-AppxPackage` first (idempotent).
- **Passing `{app}` directly to PowerShell with spaces:** Paths with spaces need quoting inside PowerShell string. Use `'{app}'` (single quotes inside double-quoted `-Command` argument). Inno Setup expands `{app}` before passing to the shell, so the path is resolved — but it may contain spaces (e.g. `C:\Users\Joe Smith\AppData\Local\...`). Wrap in single quotes inside the PowerShell command.
- **`[UninstallDelete]` for `HKCU\Software\ClaudeFromHere`:** Do NOT add this. Per D-08, user settings survive uninstall.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| MSIX signing | Custom Azure REST API calls | `sign code artifact-signing` (dotnet tool, already installed) | Handles auth (DefaultAzureCredential), retry, timestamp; already proven in scanner project |
| MSIX packing | Custom AppxManifest writer | `MakeAppx.exe pack /o /d <dir> /nv /p <output>` | Official SDK tool; `/nv` skips validation; already working in register.ps1 Step 3 |
| Installer creation | PowerShell self-extracting archive | Inno Setup 6.7.1 | Handles file overwrite, uninstall log, Add/Remove Programs entry, silent flags automatically |
| MSIX registration | Manual registry writes | `Add-AppxPackage -Path <msix> -ExternalLocation <dir>` | COM surrogate registration and shell extension wiring handled entirely by MSIX infrastructure |
| MSIX unregistration | Manual registry cleanup | `Get-AppxPackage -Name ClaudeFromHere \| Remove-AppxPackage` | Removes COM registration, context menu wiring, and AppModel entries cleanly |

---

## Common Pitfalls

### Pitfall 1: Inno Setup brace escaping in Parameters
**What goes wrong:** PowerShell commands with `{` and `}` (e.g., `if ($pkg) { ... }`) fail in Inno Setup `[Run]` Parameters because `{word}` is interpreted as an Inno Setup constant.
**Why it happens:** Inno Setup pre-processes `Parameters` values and expands `{app}`, `{tmp}`, etc. before passing to the process. Any `{...}` that isn't a recognized constant causes a compile error.
**How to avoid:** Double every brace in PowerShell code: `{` becomes `{{` and `}` becomes `}}` inside Inno Setup Parameters strings.
**Warning signs:** ISCC compile error "Unknown constant" or "Invalid constant" at any `[Run]` line with inline PowerShell.

### Pitfall 2: Add-AppxPackage path quoting with spaces
**What goes wrong:** `Add-AppxPackage -Path C:\Users\Joe Smith\AppData\...` fails because of the space in the username.
**Why it happens:** Inno Setup expands `{app}` to the actual path before passing to PowerShell, but PowerShell's `-Command` string still needs the path quoted.
**How to avoid:** Wrap the expanded path in single quotes inside the `-Command` string: `Add-AppxPackage -Path '{app}\ClaudeFromHere.msix' -ExternalLocation '{app}'`. The single quotes are part of the PowerShell string, not Inno Setup constants.
**Warning signs:** Package registration fails only on machines where the username or profile path contains a space.

### Pitfall 3: AppxManifest Publisher must match signing certificate CN
**What goes wrong:** `Add-AppxPackage` fails with "The publisher of the package does not match" or "The digital signature of the object did not verify."
**Why it happens:** Azure Trusted Signing issues a certificate with a specific Subject (CN). The `Publisher` attribute in `AppxManifest.xml` must be an exact match. Current manifest has `Publisher="CN=ClaudeFromHere"` — this must be updated to match the Azure Trusted Signing certificate's Subject before signing.
**How to avoid:** Check the Azure Trusted Signing certificate profile's Subject name. Update `package/AppxManifest.xml` `Identity Publisher` attribute to match. This is a one-time change.
**Warning signs:** `Add-AppxPackage` error 0x80080204 "The package is not valid in the current context" or CERT_E_CHAINING.

### Pitfall 4: MSIX registration fails silently in Inno Setup [Run]
**What goes wrong:** The installer appears to succeed but the context menu never appears; PowerShell MSIX commands returned a non-zero exit code but Inno Setup didn't surface the error.
**Why it happens:** `Add-AppxPackage` and `Remove-AppxPackage` throw terminating errors on failure; when run via `powershell.exe -Command "..."`, a thrown exception sets exit code 1 but Inno Setup only fails the step if the Flags include error-checking behavior.
**How to avoid:** Add `-ErrorAction Stop` to PowerShell cmdlets so failures propagate to exit code. Wrap in try/catch if you want a custom error message. Test the PowerShell commands manually before embedding them in the Inno Setup script.
**Warning signs:** Installer completes with no visible error but context menu item is absent.

### Pitfall 5: Upgrade leaves DLL locked by Explorer
**What goes wrong:** Inno Setup cannot overwrite `ClaudeFromHere.dll` because Explorer loaded it from the previous installation. File copy fails.
**Why it happens:** When the context menu extension is registered and Explorer has invoked it, Windows holds a file lock on the DLL.
**How to avoid:** The MSIX unregistration step (`Remove-AppxPackage`) + Explorer restart **must** happen in a `[Code]` section `CurStepChanged(ssInstall)` callback — i.e., **before** Inno Setup attempts to copy files — not in `[Run]` (which runs after file copy). Use Pascal scripting to call `Exec` for the PowerShell unregister + explorer stop before the file installation phase begins.
**Warning signs:** Inno Setup error "Access is denied" or "The process cannot access the file" when overwriting `ClaudeFromHere.dll` during upgrade.

### Pitfall 6: Silent mode Explorer restart skipped
**What goes wrong:** After silent install, the context menu never appears until the user manually restarts Explorer.
**Why it happens:** The `skipifsilent` flag on the Explorer restart entry correctly prevents the interactive checkbox, but if there's no separate entry with `skipifnotsilent` (without the checkbox), Explorer won't restart automatically.
**How to avoid:** Two separate `[Run]` entries for Explorer restart: one with `skipifsilent` + `postinstall` flag (interactive checkbox on wizard completion page), one with `skipifnotsilent` (runs automatically when silent). See Pattern 3 above.

---

## Code Examples

### Full [Run] Section (install)
```pascal
; Source: adapted from scripts/register.ps1 Steps 5-6

[Run]
; Unregister existing (handles upgrade — must be before file copy; see Pitfall 5)
; This entry is called from [Code] CurStepChanged, not here.

; Register MSIX
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Add-AppxPackage -Path '{app}\ClaudeFromHere.msix' -ExternalLocation '{app}' -ErrorAction Stop"""; \
  Flags: runhidden waitprocuntilterminated; \
  StatusMsg: "Activating shell extension..."

; Explorer restart — interactive (shows checkbox on Done page)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue"""; \
  Flags: runhidden waitprocuntilterminated skipifsilent postinstall; \
  Description: "Restart File Explorer now (required to activate 'Claude from here')"

; Explorer restart — silent mode (automatic, no checkbox)
Filename: "powershell.exe"; \
  Parameters: "-ExecutionPolicy Bypass -NonInteractive -Command ""Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue"""; \
  Flags: runhidden waitprocuntilterminated skipifnotsilent
```

### Pre-install Unregister in [Code] (handles upgrade DLL lock)
```pascal
// Source: Inno Setup Pascal scripting + register.ps1 Step 5 pattern
procedure CurStepChanged(CurStep: TSetupStep);
var
  ResultCode: Integer;
begin
  if CurStep = ssInstall then
  begin
    // Unregister existing MSIX so Explorer releases DLL lock
    Exec('powershell.exe',
      '-ExecutionPolicy Bypass -NonInteractive -Command ' +
      '"$pkg = Get-AppxPackage -Name ''ClaudeFromHere'' -ErrorAction SilentlyContinue; ' +
      'if ($pkg) { Remove-AppxPackage -Package $pkg.PackageFullName -ErrorAction SilentlyContinue }; ' +
      'Stop-Process -Name explorer -Force -ErrorAction SilentlyContinue; ' +
      'Start-Sleep -Seconds 2"',
      '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
    // ResultCode is not checked — failure here is non-fatal (fresh install has no existing package)
  end;
end;
```

### Azure Artifact Signing build step
```powershell
# Source: D:\Working\Projects\scanner\build-installer.ps1 (proven pattern)
# sign tool already installed: sign 0.9.1-beta.26179.1

$tsEndpoint = if ($env:CFH_SIGNING_ENDPOINT) { $env:CFH_SIGNING_ENDPOINT } else { "https://eus.codesigning.azure.net/" }
$tsAccount  = if ($env:CFH_SIGNING_ACCOUNT)  { $env:CFH_SIGNING_ACCOUNT  } else { "YourAccountName" }
$tsProfile  = if ($env:CFH_SIGNING_PROFILE)  { $env:CFH_SIGNING_PROFILE  } else { "YourProfileName" }

sign code artifact-signing "$BuildDir\ClaudeFromHere.msix" `
    --artifact-signing-endpoint $tsEndpoint `
    --artifact-signing-account $tsAccount `
    --artifact-signing-certificate-profile $tsProfile `
    --azure-credential-type azure-cli

if ($LASTEXITCODE -ne 0) { throw "MSIX signing failed" }
```

### MakeAppx pack step (already proven in register.ps1)
```powershell
$MakeAppx = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe"
& $MakeAppx pack /o /d "$PackageDir" /nv /p "$BuildDir\ClaudeFromHere.msix"
if ($LASTEXITCODE -ne 0) { throw "MakeAppx failed" }
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Self-signed cert + LocalMachine import (elevation required) | Azure Trusted Signing CA-backed cert (no import) | Phase 3 decision (D-01) | Eliminates the only reason for admin elevation; installer runs fully per-user |
| `signtool.exe /f pfx /p password` | `sign code artifact-signing` (dotnet tool) | CLAUDE.md recommended Azure; scanner project confirmed pattern | No local PFX file; auth via Azure CLI credential; cert validity handled by service |
| `PrivilegesRequired=admin` (needed for cert import) | `PrivilegesRequired=lowest` | Phase 3 decision (D-03) | Installer never triggers UAC; runs on locked-down enterprise machines |

**Deprecated/outdated:**
- Self-signed cert workflow from register.ps1 Steps 1-2: These steps (New-SelfSignedCertificate, Export-PfxCertificate, Import-PfxCertificate) are dev-only and do NOT appear in the installer. The installer ships a pre-signed MSIX.
- `MakeCert.exe`: Deprecated; docs redirect to `New-SelfSignedCertificate`. Not relevant here.

---

## Open Questions

1. **Azure Trusted Signing certificate Subject (Publisher CN)**
   - What we know: Current `AppxManifest.xml` has `Publisher="CN=ClaudeFromHere"`. The Azure Trusted Signing cert will have a different Subject issued by the CA.
   - What's unclear: The exact CN from the Azure Trusted Signing certificate profile. This must be confirmed and the AppxManifest must be updated to match before signing.
   - Recommendation: Wave 0 task — read the certificate Subject from the Azure Trusted Signing certificate profile and update `package/AppxManifest.xml` `Identity Publisher` attribute accordingly. Command: `az trustedsigning certificate-profile show --account-name <account> --profile-name <profile> --resource-group <rg>` or inspect via Azure Portal.

2. **Azure Trusted Signing environment variables (account/endpoint/profile)**
   - What we know: The `sign` tool is installed; Azure CLI is authenticated; the scanner project uses `AGRUS_SIGNING_ENDPOINT/ACCOUNT/PROFILE` env vars.
   - What's unclear: The specific account name, endpoint region, and certificate profile name for this project.
   - Recommendation: Document the env var names in the build script (`CFH_SIGNING_ENDPOINT`, `CFH_SIGNING_ACCOUNT`, `CFH_SIGNING_PROFILE`) and require them to be set at build time. Build script warns and skips signing if not set (same pattern as scanner).

3. **Inno Setup AppId GUID**
   - What we know: Inno Setup tracks upgrades by AppId. A stable GUID should be chosen for the lifetime of the product.
   - What's unclear: No AppId GUID has been established for this installer yet.
   - Recommendation: Generate a new GUID for the installer's AppId (separate from the COM CLSID `{b2dd8803-e848-41d5-bb0b-598086308dcf}`). Document in the `.iss` file and in CLAUDE.md conventions.

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| Inno Setup 6 (ISCC.exe) | Building the `.exe` installer | **Not installed** | — | Install via `winget install --id JRSoftware.InnoSetup` (Wave 0) |
| `sign` dotnet tool | MSIX signing via Azure Artifact Signing | **Yes** | 0.9.1-beta.26179.1 | — |
| Azure CLI | Auth for signing (`azure-cli` credential type) | **Yes** (authenticated) | Active subscription confirmed | — |
| MakeAppx.exe | Packing MSIX from AppxManifest | **Yes** | Windows SDK 10.0.26100.0 | — |
| SignTool.exe | (Alternative; not used) | Yes | Windows SDK 10.0.26100.0 | — |
| .NET 9 SDK | Build config app | **Yes** | 9.0.312 | — |
| VS Build Tools (MSVC) | Build DLL | **Yes** | 2022 | — |
| Existing MSIX registration | Upgrade test | Active (`ClaudeFromHere_1.0.0.0_x64__43dw3j1hc5yby`) | 1.0.0.0 | — |

**Missing dependencies with no fallback:**
- Inno Setup 6 (ISCC.exe): Wave 0 must install this before the installer can be built.

**Missing dependencies with fallback:**
- None.

---

## Project Constraints (from CLAUDE.md)

- Platform: Windows 11 only
- No compilation dependencies for users: installer must be self-contained
- Use HKCU (per-user install) — enforced by `PrivilegesRequired=lowest`
- Inno Setup version: 6.4+ (currently 6.7.1) — confirmed in CLAUDE.md
- C++ shell extension DLL is already compiled — installer deploys pre-built artifacts
- `AppxManifest.xml` `MaxVersionTested` must exceed 10.0.21300.0 — already set to 10.0.26100.0 in existing manifest

---

## Sources

### Primary (HIGH confidence)
- Inno Setup official docs (jrsoftware.org/ishelp): `[Setup]`, `[Files]`, `[Run]`, `[UninstallRun]`, `[Icons]`, `[Code]` sections, `PrivilegesRequired`, directory constants
- Microsoft Learn: [Set up signing integrations to use Artifact Signing](https://learn.microsoft.com/en-us/azure/artifact-signing/how-to-signing-integrations) — SignTool + Dlib workflow, JSON metadata format, timestamp URL
- Microsoft Learn: [Add-AppxPackage](https://learn.microsoft.com/en-us/powershell/module/appx/add-appxpackage) — `-ExternalLocation` parameter
- `scripts/register.ps1` (this project) — proven pack/sign/register/restart workflow
- `D:\Working\Projects\scanner\build-installer.ps1` — proven `sign code artifact-signing` pattern with env var parameterization

### Secondary (MEDIUM confidence)
- [Advanced Installer: Add-AppxPackage cmdlet page](https://www.advancedinstaller.com/hub/msix-packaging/add-appxpackage-cmdlet.html) — sparse package registration patterns
- [Kinook: Creating a Non-Admin Installer with Inno Setup](https://kinook.com/blog2/inno-setup.html) — PrivilegesRequired=lowest walkthrough
- [Melatonin Dev: Code signing with Azure Trusted Signing](https://melatonin.dev/blog/code-signing-on-windows-with-azure-trusted-signing/) — real-world signing integration walkthrough

### Tertiary (LOW confidence)
- None — all critical claims verified against official sources or working project code.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools confirmed installed/available via environment audit; Inno Setup version from CLAUDE.md
- Architecture patterns: HIGH — PowerShell registration logic proven in register.ps1; Inno Setup patterns from official docs
- Pitfalls: HIGH — brace escaping and path quoting are well-documented Inno Setup behaviors; DLL lock pitfall is from Phase 1-2 summary notes
- Signing workflow: HIGH — `sign` tool installed and `sign code artifact-signing --help` verified; same pattern proven in sibling project

**Research date:** 2026-04-10
**Valid until:** 2026-07-10 (stable tooling; Inno Setup and Azure Artifact Signing patterns are stable)
