# Phase 1: Foundation - Research

**Researched:** 2026-04-09
**Domain:** Windows 11 Shell Extension — C++ COM DLL + Sparse MSIX Package
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Phase 1 wires the full working launcher (`wt.exe -d "<dir>" cmd /k claude`), not a placeholder. This proves end-to-end functionality; Phase 2 adds error handling and path detection on top.
- **D-02:** The DLL searches PATH for `wt.exe` and `claude.exe` at invocation time. If either is not found, a basic Windows MessageBox displays a clear error (e.g., "claude.exe not found in PATH"). Phase 2 replaces this with polished error dialogs and auto-detection beyond PATH.
- **D-03:** Ship a custom `.ico` file with the package (multi-size: 16x16, 32x32, 48x48) rather than extracting from `claude.exe` at runtime.
- **D-04:** Icon design uses the Claude logo mark (sparkle/asterisk symbol). Must work in both light and dark Explorer themes.
- **D-05:** C++ compiler is MSVC via Visual Studio 2022 Build Tools (free). Recommended by CLAUDE.md for in-process COM compatibility.
- **D-06:** Build system is CMake. More portable and version-control-friendly than `.vcxproj` files; standard for open-source C++ projects.
- **D-07:** Separate PowerShell scripts for dev workflow: `register.ps1` and `unregister.ps1`. These handle cert generation, MSIX signing via MakeAppx/SignTool, and `Add-AppxPackage -ExternalLocation` registration.
- **D-08:** Admin elevation required only once for initial cert import to `LocalMachine\TrustedPeople`. Subsequent re-registrations (rebuild DLL, re-pack MSIX, re-register) run unelevated. Scripts should detect whether cert is already imported and skip the admin step.

### Claude's Discretion

- DLL project structure and COM boilerplate organization
- AppxManifest.xml namespace details and schema versions
- Exact CMakeLists.txt structure
- Script error handling details beyond the MessageBox approach

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| MENU-01 | User sees "Claude from here" in the Windows 11 modern (top-level) right-click menu when right-clicking a folder | `desktop5:ItemType Type="Directory"` in AppxManifest wires CLSID to folder right-clicks; IExplorerCommand::GetTitle returns the label |
| MENU-02 | User sees "Claude from here" when right-clicking the background inside a folder | `desktop5:ItemType Type="Directory\Background"` is an officially supported value; IObjectWithSite required to get folder path from background context |
| MENU-03 | Menu item displays a custom Claude icon next to the text | IExplorerCommand::GetIcon returns a path string like `C:\path\to\claude.ico,0`; .ico file must be multi-size and bundled with the MSIX external location |
</phase_requirements>

## Summary

Phase 1 builds the complete foundational stack for the Windows 11 modern context menu integration: a C++ COM DLL implementing `IExplorerCommand`, a sparse MSIX package with `AppxManifest.xml` wiring the DLL's CLSID to both folder and folder-background right-clicks, a self-signed certificate for signing, and PowerShell dev scripts for the full register/unregister workflow.

The core technical architecture is well-documented by Microsoft and proven by reference implementations (VS Code's own explorer command). The one non-trivial discovery is the `Directory\Background` context: the `IShellItemArray` passed to `Invoke` is empty for background clicks, so the DLL must implement `IObjectWithSite` and traverse the shell browser hierarchy (`IServiceProvider` → `IShellBrowser` → `IShellView` → `IFolderView`) to get the current folder path. This must be accounted for in the C++ implementation.

All required build tools are present on the machine: Windows SDK 10.0.26100.0 provides `MakeAppx.exe` and `SignTool.exe`, VS 2022 Build Tools (14.44.35207) provides MSVC and CMake, Windows Terminal is installed, and `claude.exe` is at `C:\Users\jfago\.local\bin\claude.exe`.

**Primary recommendation:** Implement the DLL with both `IExplorerCommand` and `IObjectWithSite`, use two `desktop5:ItemType` entries in the manifest (`Directory` and `Directory\Background`), and build the PowerShell scripts around the established `New-SelfSignedCertificate` → `Export-PfxCertificate` → `Import-PfxCertificate` → `MakeAppx` → `SignTool` → `Add-AppxPackage -ExternalLocation` pipeline.

## Standard Stack

### Core

| Library / Tool | Version | Purpose | Why Standard |
|----------------|---------|---------|--------------|
| MSVC (cl.exe) | 14.44.35207 (VS 2022 Build Tools) | Compile the COM DLL | Microsoft recommends C++ for in-process shell extensions; managed code is explicitly discouraged; MSVC produces stable in-process COM |
| Windows SDK | 10.0.26100.0 | Shell APIs, `IExplorerCommand`, COM headers, `MakeAppx.exe`, `SignTool.exe` | Required to build against current Windows shell interfaces and package tools |
| CMake | Bundled with VS 2022 Build Tools | Build system for the DLL | Locked by D-06; portable, version-control-friendly |
| MakeAppx.exe | SDK 10.0.26100.0 x64 | Pack `AppxManifest.xml` + assets into `.msix` | Official Microsoft tool; `/nv` flag skips payload validation for sparse packages |
| SignTool.exe | SDK 10.0.26100.0 x64 | Sign `.msix` with SHA-256 | Required; unsigned MSIX cannot be registered |
| PowerShell / pwsh | 7.6.0 | `register.ps1` / `unregister.ps1` dev scripts | Built-in; `New-SelfSignedCertificate`, `Add-AppxPackage`, `Import-PfxCertificate` are PS cmdlets |

### Supporting

| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| `New-SelfSignedCertificate` | Built-in Win 11 PS | Generate self-signed code-signing cert | Dev workflow and Phase 1 distribution |
| `Export-PfxCertificate` | Built-in Win 11 PS | Export cert to PFX for SignTool | Run once after cert generation |
| `Import-PfxCertificate` | Built-in Win 11 PS | Import cert to `LocalMachine\TrustedPeople` | Run once with elevation; D-08 says skip if already present |
| `Add-AppxPackage -ExternalLocation` | Win 11 PS | Register sparse MSIX pointing to build dir | Run after every re-pack |
| `Get-AppxPackage | Remove-AppxPackage` | Win 11 PS | Unregister for `unregister.ps1` | Teardown |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| MSVC | MinGW-w64 | MSVC is locked by D-05; MinGW can also produce COM DLLs but MSVC is better tested for in-process Explorer extensions |
| CMake | VS .vcxproj | CMake is locked by D-06; .vcxproj is simpler for a single DLL but not version-control-friendly |
| Self-signed cert | Azure Trusted Signing | Self-signed requires one-time cert import; Azure Trusted Signing costs ~$10/month and requires US/Canada business — overkill for Phase 1 |

**SDK tool paths (verified on this machine):**
```
C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe
C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe
C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe
```

## Architecture Patterns

### Recommended Project Structure

```
claude-from-here/
├── src/
│   ├── ClaudeFromHere.cpp     # IExplorerCommand + IObjectWithSite implementation
│   ├── ClaudeFromHere.def     # DLL exports: DllGetClassObject, DllCanUnloadNow
│   ├── ClaudeFromHere.rc      # Resource script embedding the .ico
│   └── dllmain.cpp            # DllMain entry point (minimal)
├── assets/
│   └── claude.ico             # Multi-size icon (16, 32, 48 px)
├── package/
│   ├── AppxManifest.xml        # Sparse MSIX manifest
│   └── Assets/
│       └── StoreLogo.png       # Required by manifest VisualElements
├── scripts/
│   ├── register.ps1            # Full dev register workflow
│   └── unregister.ps1          # Full dev unregister workflow
├── CMakeLists.txt
├── claude-from-here.reg        # Existing (keep as Win10/legacy fallback)
└── claude-from-here-uninstall.reg
```

### Pattern 1: IExplorerCommand COM Class

**What:** A C++ class implementing `IExplorerCommand` (and `IObjectWithSite`) registered as a COM surrogate server in the MSIX manifest.

**When to use:** Required for top-level Windows 11 modern context menu placement.

**Key methods to implement:**

```cpp
// Source: https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/integrate-packaged-app-with-file-explorer
// and https://github.com/microsoft/Windows-AppConsult-Samples-DesktopBridge

class CClaudeFromHere : 
    public IExplorerCommand,
    public IObjectWithSite   // Required for Directory\Background path retrieval
{
public:
    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();
    
    // IExplorerCommand
    IFACEMETHODIMP GetTitle(IShellItemArray* psiItemArray, LPWSTR* ppszName);
    IFACEMETHODIMP GetIcon(IShellItemArray* psiItemArray, LPWSTR* ppszIcon);
    IFACEMETHODIMP GetToolTip(IShellItemArray* psiItemArray, LPWSTR* ppszInfotip);
    IFACEMETHODIMP GetCanonicalName(GUID* pguidCommandName);
    IFACEMETHODIMP GetState(IShellItemArray* psiItemArray, BOOL fOkToBeSlow, EXPCMDSTATE* pCmdState);
    IFACEMETHODIMP Invoke(IShellItemArray* psiItemArray, IBindCtx* pbc);
    IFACEMETHODIMP GetFlags(EXPCMDFLAGS* pFlags);
    IFACEMETHODIMP EnumSubCommands(IEnumExplorerCommand** ppEnum);
    
    // IObjectWithSite — required for background context
    IFACEMETHODIMP SetSite(IUnknown* punkSite);
    IFACEMETHODIMP GetSite(REFIID riid, void** ppvSite);

private:
    long _cRef = 1;
    IUnknown* _punkSite = nullptr;
    
    HRESULT _GetFolderPathFromSite(PWSTR* ppszPath);  // fallback for background clicks
};
```

### Pattern 2: Path Retrieval — Folder vs Background Click

**What:** Two different code paths are required in `Invoke` because `IShellItemArray` is NULL or empty for `Directory\Background` clicks.

**Source:** Verified via https://www.simonmourier.com/blog/IExploreCommand-context-menu-with-submenus-returns-empty-IShellItemArray-for-dir/ (MEDIUM confidence); consistent with IObjectWithSite documentation (HIGH confidence).

```cpp
HRESULT CClaudeFromHere::Invoke(IShellItemArray* psiItemArray, IBindCtx* /*pbc*/)
{
    PWSTR pszPath = nullptr;
    HRESULT hr = E_FAIL;
    
    // Path A: folder right-click — psiItemArray contains the selected folder
    if (psiItemArray)
    {
        IShellItem* psi;
        hr = GetItemAt(psiItemArray, 0, IID_PPV_ARGS(&psi));
        if (SUCCEEDED(hr))
        {
            hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, &pszPath);
            psi->Release();
        }
    }
    
    // Path B: folder background right-click — use IObjectWithSite chain
    if (FAILED(hr) || !pszPath)
    {
        hr = _GetFolderPathFromSite(&pszPath);
    }
    
    if (SUCCEEDED(hr) && pszPath)
    {
        // Launch: wt.exe -d "<pszPath>" cmd /k claude
        // Use SearchPath for wt.exe and claude.exe; MessageBox on failure (D-02)
        _LaunchClaude(pszPath);
        CoTaskMemFree(pszPath);
    }
    return S_OK;
}

// IObjectWithSite chain for background clicks
HRESULT CClaudeFromHere::_GetFolderPathFromSite(PWSTR* ppszPath)
{
    if (!_punkSite) return E_FAIL;
    
    IServiceProvider* psp;
    HRESULT hr = _punkSite->QueryInterface(IID_PPV_ARGS(&psp));
    if (SUCCEEDED(hr))
    {
        IShellBrowser* psb;
        hr = psp->QueryService(SID_STopLevelBrowser, IID_PPV_ARGS(&psb));
        psp->Release();
        if (SUCCEEDED(hr))
        {
            IShellView* psv;
            hr = psb->QueryActiveShellView(&psv);
            psb->Release();
            if (SUCCEEDED(hr))
            {
                IFolderView* pfv;
                hr = psv->QueryInterface(IID_PPV_ARGS(&pfv));
                psv->Release();
                if (SUCCEEDED(hr))
                {
                    IShellItem* psi;
                    hr = pfv->GetFolder(IID_PPV_ARGS(&psi));
                    pfv->Release();
                    if (SUCCEEDED(hr))
                    {
                        hr = psi->GetDisplayName(SIGDN_DESKTOPABSOLUTEPARSING, ppszPath);
                        psi->Release();
                    }
                }
            }
        }
    }
    return hr;
}
```

### Pattern 3: AppxManifest.xml — Complete Sparse Package

**What:** The manifest that wires the DLL CLSID to both `Directory` and `Directory\Background` item types.

**Source:** https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-desktop5-itemtype (HIGH — official schema), https://github.com/ikas-mc/ContextMenuForWindows11/blob/main/ContextMenuCustom/ContextMenuCustomPackage/Package.appxmanifest (MEDIUM — real-world example)

```xml
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:uap10="http://schemas.microsoft.com/appx/manifest/uap/windows10/10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  xmlns:com="http://schemas.microsoft.com/appx/manifest/com/windows10"
  xmlns:desktop4="http://schemas.microsoft.com/appx/manifest/desktop/windows10/4"
  xmlns:desktop5="http://schemas.microsoft.com/appx/manifest/desktop/windows10/5"
  IgnorableNamespaces="uap uap10 rescap com desktop4 desktop5">

  <Identity
    Name="ClaudeFromHere"
    Publisher="CN=ClaudeFromHere"
    Version="1.0.0.0"
    ProcessorArchitecture="x64" />

  <Properties>
    <DisplayName>Claude From Here</DisplayName>
    <PublisherDisplayName>Claude From Here</PublisherDisplayName>
    <Logo>Assets\StoreLogo.png</Logo>
    <uap10:AllowExternalContent>true</uap10:AllowExternalContent>
  </Properties>

  <Resources>
    <Resource Language="en-us" />
  </Resources>

  <Dependencies>
    <TargetDeviceFamily
      Name="Windows.Desktop"
      MinVersion="10.0.22000.0"
      MaxVersionTested="10.0.26100.0" />
  </Dependencies>

  <Capabilities>
    <rescap:Capability Name="runFullTrust" />
    <rescap:Capability Name="unvirtualizedResources" />
  </Capabilities>

  <Applications>
    <Application
      Id="ClaudeFromHere"
      Executable="ClaudeFromHere.dll"
      uap10:TrustLevel="mediumIL"
      uap10:RuntimeBehavior="win32App">

      <uap:VisualElements
        AppListEntry="none"
        DisplayName="Claude From Here"
        Description="Open Claude Code in any folder"
        BackgroundColor="transparent"
        Square150x150Logo="Assets\StoreLogo.png"
        Square44x44Logo="Assets\StoreLogo.png" />

      <Extensions>

        <!-- Register the COM surrogate server (the DLL) -->
        <com:Extension Category="windows.comServer">
          <com:ComServer>
            <com:SurrogateServer DisplayName="ClaudeFromHere">
              <com:Class
                Id="YOUR-CLSID-HERE"
                Path="ClaudeFromHere.dll"
                ThreadingModel="STA" />
            </com:SurrogateServer>
          </com:ComServer>
        </com:Extension>

        <!-- Wire CLSID to folder right-click AND folder-background right-click -->
        <desktop4:Extension Category="windows.fileExplorerContextMenus">
          <desktop4:FileExplorerContextMenus>
            <desktop5:ItemType Type="Directory">
              <desktop5:Verb Id="ClaudeFromHere" Clsid="YOUR-CLSID-HERE" />
            </desktop5:ItemType>
            <desktop5:ItemType Type="Directory\Background">
              <desktop5:Verb Id="ClaudeFromHere" Clsid="YOUR-CLSID-HERE" />
            </desktop5:ItemType>
          </desktop4:FileExplorerContextMenus>
        </desktop4:Extension>

      </Extensions>
    </Application>
  </Applications>
</Package>
```

**Critical notes:**
- `Publisher="CN=ClaudeFromHere"` in `<Identity>` MUST exactly match the `-Subject` used in `New-SelfSignedCertificate`
- `ProcessorArchitecture="x64"` matches the x64 DLL; must be consistent
- `MinVersion="10.0.22000.0"` targets Win 11 minimum; `AllowExternalContent` requires 10.0.19041.0+
- `MaxVersionTested` must exceed `10.0.21300.0` for Win 11 context menu features to activate

### Pattern 4: GetIcon Implementation

**What:** `GetIcon` returns a resource string that Explorer uses to find the icon. For a bundled `.ico` file use an absolute path to the file in the external location.

```cpp
IFACEMETHODIMP CClaudeFromHere::GetIcon(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszIcon)
{
    // Return absolute path to the .ico file in the install directory
    // Explorer accepts "C:\path\to\file.ico" or "C:\path\to\file.ico,0"
    // At dev time, the external location IS the build output directory
    WCHAR szModule[MAX_PATH];
    GetModuleFileNameW(g_hModule, szModule, ARRAYSIZE(szModule));
    
    // Replace DLL filename with icon filename
    PathRemoveFileSpecW(szModule);
    PathAppendW(szModule, L"claude.ico");
    
    return SHStrDupW(szModule, ppszIcon);
}
```

**Note:** `g_hModule` must be captured in `DllMain` (store the `hModule` from `DLL_PROCESS_ATTACH`).

### Pattern 5: DLL Exports (.def file)

```
LIBRARY ClaudeFromHere
EXPORTS
    DllGetClassObject  PRIVATE
    DllCanUnloadNow    PRIVATE
```

### Pattern 6: CMakeLists.txt Structure

```cmake
cmake_minimum_required(VERSION 3.28)
project(ClaudeFromHere LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(ClaudeFromHere SHARED
    src/dllmain.cpp
    src/ClaudeFromHere.cpp
    src/ClaudeFromHere.def
    src/ClaudeFromHere.rc    # embeds claude.ico as resource (optional but useful)
)

target_link_libraries(ClaudeFromHere PRIVATE
    shlwapi      # PathRemoveFileSpecW, PathAppendW, SHStrDupW
    shell32      # Shell APIs
    ole32        # COM
)

# Output directly to a known location for register.ps1 to consume
set_target_properties(ClaudeFromHere PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build"
)
```

### Pattern 7: Register/Unregister PowerShell Script Structure

```powershell
# register.ps1 — Full dev workflow (D-07, D-08)
param(
    [string]$BuildDir = "$PSScriptRoot\build",
    [string]$PackageDir = "$PSScriptRoot\package",
    [string]$CertSubject = "CN=ClaudeFromHere",
    [string]$CertFriendlyName = "ClaudeFromHere Dev Cert"
)

$SDK = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"
$MakeAppx = "$SDK\makeappx.exe"
$SignTool  = "$SDK\signtool.exe"

# Step 1: Generate cert if not already in LocalMachine\TrustedPeople (D-08)
$cert = Get-ChildItem "Cert:\LocalMachine\TrustedPeople" |
        Where-Object { $_.Subject -eq $CertSubject } | Select-Object -First 1

if (-not $cert) {
    Write-Host "Generating self-signed certificate (requires elevation for import)..."
    # New-SelfSignedCertificate -> Export-PfxCertificate -> Import-PfxCertificate (elevated)
    # ... (details in implementation)
}

# Step 2: Pack MSIX
& $MakeAppx pack /o /d $PackageDir /nv /p "$BuildDir\ClaudeFromHere.msix"

# Step 3: Sign
& $SignTool sign /fd SHA256 /a /f "$BuildDir\ClaudeFromHere.pfx" /p $certPassword "$BuildDir\ClaudeFromHere.msix"

# Step 4: Unregister existing (if registered)
$existing = Get-AppxPackage "ClaudeFromHere" -ErrorAction SilentlyContinue
if ($existing) { Remove-AppxPackage $existing.PackageFullName }

# Step 5: Register with ExternalLocation pointing to build dir
Add-AppxPackage -Path "$BuildDir\ClaudeFromHere.msix" -ExternalLocation $BuildDir

Write-Host "Registered. Restart explorer.exe or reboot to activate."
```

### Anti-Patterns to Avoid

- **Relying on IShellItemArray alone:** For `Directory\Background` clicks the array is NULL/empty. Always implement `IObjectWithSite` and use the fallback path.
- **Hardcoded paths in GetIcon:** Use `GetModuleFileNameW` to derive the icon path relative to the DLL location; the install path varies per user.
- **Registering MSIX at `CurrentUser\TrustedPeople` only:** MSIX installer checks `LocalMachine\TrustedPeople`. Install fails silently if only user store has the cert.
- **`ProcessorArchitecture="neutral"` with a DLL:** Use `x64` for a 64-bit DLL. `neutral` is appropriate only for manifest-only packages with no binaries.
- **Missing side-by-side manifest in the DLL:** The DLL needs a fusion/side-by-side manifest (`.manifest` file or embedded resource) with an `msix` element whose `packageName` and `publisher` attributes match the `AppxManifest.xml` `<Identity>` element. Without it, Windows Explorer will not load the extension.
- **Forgetting to restart Explorer after registration:** Shell extensions are loaded at Explorer startup. `Add-AppxPackage` alone does not hot-reload the extension; `Stop-Process -Name explorer` is needed in the dev loop.

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| CLSID generation | Manual GUID authoring | `[System.Guid]::NewGuid()` in PowerShell or `uuidgen.exe` | GUIDs must be unique; any hand-typed GUID risks collision |
| COM class factory | Custom IClassFactory boilerplate | WRL `RuntimeClass<ClassicCom>` or plain `IClassFactory` impl from Windows-classic-samples pattern | COM factory is pure boilerplate; copy from ExplorerCommandVerb sample |
| Icon conversion | PNG → ICO script | Use an ICO generator tool (IcoFX, ImageMagick, or online tool) to produce multi-size `.ico` | ICO format with multiple sizes is binary; hand-rolling is error-prone |
| MSIX packaging | Custom ZIP-based packer | `MakeAppx.exe /nv` | MakeAppx produces a correctly-structured APPX/MSIX container; custom ZIP packers miss metadata |
| Certificate trust | Manual registry injection | `Import-PfxCertificate` PowerShell cmdlet | Cert store manipulation has specific binary formats; the cmdlet handles it correctly |

**Key insight:** The COM factory, MSIX packaging, and cert management are all solved problems with official tooling. The only custom code is the `IExplorerCommand` implementation itself.

## Runtime State Inventory

> Phase 1 is greenfield — no existing runtime state beyond the `.reg` file entries which are separate from the MSIX approach. The `.reg` file approach (classic menu) and the sparse MSIX approach (modern menu) coexist independently.

| Category | Items Found | Action Required |
|----------|-------------|------------------|
| Stored data | None | — |
| Live service config | None | — |
| OS-registered state | `HKCU\Software\Classes\Directory\shell\ClaudeFromHere` and `Directory\Background\shell\ClaudeFromHere` (from `.reg` file) | These are the classic-menu entries; they coexist with MSIX and do not interfere. No action needed for Phase 1. |
| Secrets/env vars | None | — |
| Build artifacts | None (no prior build) | — |

## Common Pitfalls

### Pitfall 1: IShellItemArray Empty for Directory\Background

**What goes wrong:** `Invoke` receives a NULL or empty `IShellItemArray` when the user right-clicks the folder background. Attempting to call `GetItemAt` or `GetCount` crashes or returns zero items. The DLL appears to do nothing.

**Why it happens:** The shell passes selected items in the array; for a background click there is no "selected item" — the context is the folder itself. This is a known design characteristic of `IExplorerCommand`.

**How to avoid:** Implement `IObjectWithSite`. In `Invoke`, check if `psiItemArray` is usable first; if not, fall back to the `IServiceProvider` → `IShellBrowser` → `IShellView` → `IFolderView` → `IShellItem` chain to get the current folder.

**Warning signs:** Menu appears but clicking it does nothing; or works for folder right-click but not background right-click.

### Pitfall 2: Publisher CN Mismatch

**What goes wrong:** `Add-AppxPackage` fails with a signing error even though `SignTool` succeeded.

**Why it happens:** The `Publisher` attribute in `AppxManifest.xml` `<Identity>` must exactly match the `Subject` field of the signing certificate (e.g., `CN=ClaudeFromHere`). Even minor formatting differences (extra spaces, different case) cause failure.

**How to avoid:** Use the exact same string for both the `-Subject` parameter in `New-SelfSignedCertificate` and the `Publisher=` attribute in the manifest. Put both in a shared variable in `register.ps1`.

**Warning signs:** `SignTool Error: An unexpected internal error has occurred` or `Add-AppxPackage : Deployment failed with HRESULT: 0x80080205`.

### Pitfall 3: Cert in Wrong Store

**What goes wrong:** MSIX registration fails even though the cert was imported.

**Why it happens:** `Cert:\CurrentUser\TrustedPeople` is checked by some tools but the MSIX verifier requires `Cert:\LocalMachine\TrustedPeople`.

**How to avoid:** Always import to `LocalMachine\TrustedPeople` (requires elevation). The `register.ps1` script should check this store, not the user store.

**Warning signs:** `Add-AppxPackage` fails with trust/certificate errors despite the cert appearing in `Cert:\CurrentUser\My`.

### Pitfall 4: Missing DLL Side-by-Side Manifest

**What goes wrong:** Explorer silently fails to load the shell extension. The menu item never appears even though `Add-AppxPackage` succeeded.

**Why it happens:** Windows requires the DLL to declare its package identity via a fusion manifest (either embedded as resource ID 1 or as a `.manifest` file alongside the DLL). Without it, the COM activation chain cannot establish the package context.

**How to avoid:** Embed an `RT_MANIFEST` resource in the DLL (via `.rc` file) containing:
```xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly manifestVersion="1.0" xmlns="urn:schemas-microsoft-com:asm.v1">
  <assemblyIdentity version="1.0.0.0" name="ClaudeFromHere" />
  <msix xmlns="urn:schemas-microsoft-com:msix.v1"
        publisher="CN=ClaudeFromHere"
        packageName="ClaudeFromHere"
        applicationId="ClaudeFromHere" />
</assembly>
```

**Warning signs:** `Add-AppxPackage` succeeds, no errors, but right-clicking a folder shows no menu item.

### Pitfall 5: MaxVersionTested Too Low

**What goes wrong:** The context menu extension registers but the item appears under "Show more options" instead of the top-level modern menu.

**Why it happens:** `MaxVersionTested` in `<TargetDeviceFamily>` must exceed `10.0.21300.0` for Win 11 modern context menu features to activate.

**How to avoid:** Set `MaxVersionTested="10.0.26100.0"` (current Win 11 24H2 SDK).

**Warning signs:** Menu item is visible but only in the legacy "Show more options" submenu.

### Pitfall 6: Explorer Not Restarted After Registration

**What goes wrong:** `register.ps1` completes without errors but no menu item appears.

**Why it happens:** Shell extensions are loaded into `explorer.exe` at startup. `Add-AppxPackage` registers the package but does not reload shell extensions into the running process.

**How to avoid:** Include `Stop-Process -Name explorer -Force` at the end of `register.ps1` (Explorer auto-restarts). Document this in the script and in `GETTING-STARTED.md`.

**Warning signs:** Script runs cleanly, `Get-AppxPackage ClaudeFromHere` shows the package registered, but no menu item appears.

## Code Examples

### Full IExplorerCommand GetTitle / GetState / GetFlags Minimal Pattern

```cpp
// Source: https://github.com/microsoft/Windows-AppConsult-Samples-DesktopBridge
// (ExplorerCommandVerb pattern)

IFACEMETHODIMP CClaudeFromHere::GetTitle(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszName)
{
    return SHStrDupW(L"Claude from here", ppszName);
}

IFACEMETHODIMP CClaudeFromHere::GetState(IShellItemArray* /*psiItemArray*/, BOOL /*fOkToBeSlow*/, EXPCMDSTATE* pCmdState)
{
    *pCmdState = ECS_ENABLED;
    return S_OK;
}

IFACEMETHODIMP CClaudeFromHere::GetFlags(EXPCMDFLAGS* pFlags)
{
    *pFlags = ECF_DEFAULT;
    return S_OK;
}

IFACEMETHODIMP CClaudeFromHere::EnumSubCommands(IEnumExplorerCommand** ppEnum)
{
    *ppEnum = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP CClaudeFromHere::GetToolTip(IShellItemArray* /*psiItemArray*/, LPWSTR* ppszInfotip)
{
    *ppszInfotip = nullptr;
    return E_NOTIMPL;
}

IFACEMETHODIMP CClaudeFromHere::GetCanonicalName(GUID* pguidCommandName)
{
    *pguidCommandName = GUID_NULL;
    return S_OK;
}
```

### Launch Command Pattern (from existing .reg file)

```cpp
// Source: claude-from-here.reg — proven working command pattern
// cmd.exe /c start "Claude" wt.exe -d "%V" cmd /k claude
// Adapted for DLL use with SearchPath:

HRESULT _LaunchClaude(PCWSTR pszFolder)
{
    WCHAR wtPath[MAX_PATH], claudePath[MAX_PATH];
    
    // D-02: Search PATH, MessageBox on failure
    if (!SearchPathW(nullptr, L"wt.exe", nullptr, MAX_PATH, wtPath, nullptr))
    {
        MessageBoxW(nullptr, L"Windows Terminal (wt.exe) was not found in PATH.\n\nPlease install Windows Terminal from the Microsoft Store.",
                    L"Claude from here", MB_ICONERROR | MB_OK);
        return E_FAIL;
    }
    if (!SearchPathW(nullptr, L"claude.exe", nullptr, MAX_PATH, claudePath, nullptr))
    {
        MessageBoxW(nullptr, L"claude.exe was not found in PATH.\n\nPlease install Claude Code CLI and ensure it is in your PATH.",
                    L"Claude from here", MB_ICONERROR | MB_OK);
        return E_FAIL;
    }
    
    // Build: wt.exe -d "<folder>" cmd /k claude
    WCHAR szParams[32768];
    StringCchPrintfW(szParams, ARRAYSIZE(szParams), L"-d \"%s\" cmd /k claude", pszFolder);
    
    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"open";
    sei.lpFile = wtPath;
    sei.lpParameters = szParams;
    sei.nShow = SW_SHOWNORMAL;
    
    return ShellExecuteExW(&sei) ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}
```

### Self-Signed Certificate Creation (PowerShell)

```powershell
# Source: https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing
# Run in elevated PowerShell

$subject = "CN=ClaudeFromHere"
$cert = New-SelfSignedCertificate `
    -Type Custom `
    -KeyUsage DigitalSignature `
    -CertStoreLocation "Cert:\CurrentUser\My" `
    -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}") `
    -Subject $subject `
    -FriendlyName "ClaudeFromHere Dev Cert"

$password = ConvertTo-SecureString -String "DevCert123!" -Force -AsPlainText
Export-PfxCertificate -cert "Cert:\CurrentUser\My\$($cert.Thumbprint)" `
    -FilePath "$PSScriptRoot\build\ClaudeFromHere.pfx" `
    -Password $password

Import-PfxCertificate `
    -CertStoreLocation "Cert:\LocalMachine\TrustedPeople" `
    -Password $password `
    -FilePath "$PSScriptRoot\build\ClaudeFromHere.pfx"
```

### MakeAppx + SignTool Commands

```powershell
# Source: https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps
$SDK = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64"

# Pack (package/ dir contains AppxManifest.xml and Assets/)
& "$SDK\makeappx.exe" pack /o /d "$PSScriptRoot\package" /nv /p "$PSScriptRoot\build\ClaudeFromHere.msix"

# Sign with SHA-256
& "$SDK\signtool.exe" sign /fd SHA256 /a /f "$PSScriptRoot\build\ClaudeFromHere.pfx" /p "DevCert123!" "$PSScriptRoot\build\ClaudeFromHere.msix"

# Register (ExternalLocation = build dir, where the DLL lives)
Add-AppxPackage -Path "$PSScriptRoot\build\ClaudeFromHere.msix" -ExternalLocation "$PSScriptRoot\build"
```

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Registry `.reg` files for context menu | Sparse MSIX + COM DLL | Win 11 21H2 (Oct 2021) | Registry-only items appear under "Show more options" only; MSIX required for top-level placement |
| `IContextMenu` COM interface | `IExplorerCommand` interface | Win 11 modern menu (2021) | `IContextMenu` still works but only reaches the legacy submenu on Win 11 |
| `MakeCert.exe` for code signing | `New-SelfSignedCertificate` PowerShell | Deprecated since Win 10 | MakeCert.exe should not be used; Microsoft docs redirect to PS cmdlet |
| Full MSIX package (payload inside package) | Sparse package with `-ExternalLocation` | Available since Win 10 2004 (19041) | Full MSIX requires sandboxed layout; sparse package lets binaries live in any install dir |

**Deprecated/outdated:**
- `MakeCert.exe`: Deprecated since Windows 10; do not use
- `IContextMenu`: Still functional but only reaches legacy submenu on Windows 11; `IExplorerCommand` is required for top-level modern menu

## Open Questions

1. **IShellItemArray behavior on Windows 11 modern context menu for Directory\Background**
   - What we know: For the classic context menu, `IShellItemArray` is empty for background clicks (confirmed by Simon Mourier's blog). The `IObjectWithSite` workaround is well-documented.
   - What's unclear: Whether the modern (Win 11) menu also passes an empty array for `Directory\Background` or whether the behavior differs.
   - Recommendation: Implement both code paths (array + IObjectWithSite fallback) unconditionally; this is the safe approach and adds negligible complexity.

2. **Icon rendering in Win 11 modern context menu for dark/light themes**
   - What we know: `.ico` files with multiple sizes (16, 32, 48) are standard; `GetIcon` should return the path.
   - What's unclear: Whether the modern context menu applies automatic dark/light theming to icons, or whether a single `.ico` works for both themes.
   - Recommendation: Create the icon with a transparent background. Win 11 modern menu renders icons at small sizes where most icons read well on both light/dark backgrounds. Test visually after first registration; adjust if needed.

3. **`ProcessorArchitecture` in AppxManifest for a COM surrogate DLL**
   - What we know: `neutral` is recommended for manifest-only identity packages. But this package has an actual DLL payload reference in the `com:SurrogateServer`.
   - What's unclear: Whether `neutral` or `x64` is correct when the package contains no binaries itself (they're in `ExternalLocation`) but declares a specific architecture DLL.
   - Recommendation: Use `x64` to match the DLL architecture. If cross-architecture scenarios arise (Phase 3), revisit. LOW confidence — test with `Get-AppxPackage` after registration to verify.

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|---------|
| MSVC (cl.exe) | C++ DLL compilation | Yes | 14.44.35207 (VS 2022 Build Tools) | — |
| CMake | Build system (D-06) | Yes | Bundled with VS Build Tools | — |
| MakeAppx.exe | MSIX packing | Yes | SDK 10.0.26100.0 x64 | — |
| SignTool.exe | MSIX signing | Yes | SDK 10.0.26100.0 x64 | — |
| PowerShell (pwsh) | register.ps1 / unregister.ps1 | Yes | 7.6.0 | — |
| Windows Terminal (wt.exe) | Launch target | Yes | In PATH at `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` | — |
| claude.exe | Launch target | Yes | `C:\Users\jfago\.local\bin\claude.exe` | — |
| Windows 11 | Modern context menu | Yes | 10.0.26200 (Win 11 24H2) | — |

**Missing dependencies with no fallback:** None — all required tools are present.

**Missing dependencies with fallback:** None.

## Project Constraints (from CLAUDE.md)

- **Platform:** Windows 11 only (modern context menu requires Win 11)
- **No compilation dependencies for users:** Installer self-contained (Phase 3 concern; Phase 1 is dev-only)
- **MSIX signing:** Self-signed cert + machine trust store import required
- **Registry scope:** Phase 1 uses per-user `Add-AppxPackage` (no admin for registration after initial cert import)
- **C++ (MSVC):** Required for in-process shell extensions; managed code explicitly discouraged
- **IExplorerCommand:** Required for top-level modern menu placement; `IContextMenu` and registry-only approaches are explicitly in "What NOT to Use"
- **Sparse MSIX (not full MSIX):** Binaries live in external location; full MSIX package layout not viable
- **`LocalMachine\TrustedPeople`:** Required cert store (not `CurrentUser\TrustedPeople`)
- **`New-SelfSignedCertificate`:** Use this; do not use deprecated `MakeCert.exe`
- **`MakeAppx.exe` with `/nv` flag:** Skip validation; required for sparse packages with no payload files
- **`MaxVersionTested` must exceed `10.0.21300.0`:** Required for Win 11 modern context menu activation

## Sources

### Primary (HIGH confidence)
- https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/integrate-packaged-app-with-file-explorer — AppxManifest desktop4/desktop5 schema, COM DLL pattern, `CExplorerCommandVerb` code example
- https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps — Sparse package manifest template, `Add-AppxPackage -ExternalLocation`, MakeAppx/SignTool commands
- https://learn.microsoft.com/en-us/uwp/schemas/appxpackage/uapmanifestschema/element-desktop5-itemtype — Authoritative list of valid `Type` values: confirms `Directory`, `Directory\Background`, `*`, and file extension strings
- https://learn.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing — `New-SelfSignedCertificate` + `Import-PfxCertificate` + `SignTool` exact syntax
- CLAUDE.md — Technology stack, what not to use, version compatibility matrix (HIGH — project constraint document)

### Secondary (MEDIUM confidence)
- https://github.com/ikas-mc/ContextMenuForWindows11/blob/main/ContextMenuCustom/ContextMenuCustomPackage/Package.appxmanifest — Real-world manifest showing `Directory`, `Directory\Background`, and `*` item types with same CLSID
- https://www.zabkat.com/blog/win11-explorer-menu-package.htm — Real-world walkthrough; confirms DLL side-by-side manifest requirement and publisher mismatch as common pitfall
- https://github.com/microsoft/vscode-explorer-command — VS Code's reference C++ `IExplorerCommand` implementation; confirms `IObjectWithSite`, `DllGetClassObject`/`DllCanUnloadNow` exports, `ShellExecuteW` for launch
- https://www.simonmourier.com/blog/IExploreCommand-context-menu-with-submenus-returns-empty-IShellItemArray-for-dir/ — Confirms empty `IShellItemArray` for background clicks; documents `IObjectWithSite` → `IShellBrowser` → `IFolderView` workaround

### Tertiary (LOW confidence)
- WebSearch results confirming `Directory\Background` support — supported by official schema doc (elevated to HIGH)

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools verified present on machine; versions confirmed
- Architecture: HIGH — COM DLL + sparse MSIX pattern verified against official Microsoft docs and reference implementations
- Pitfalls: HIGH for cert/manifest issues (multiple real-world sources); MEDIUM for `Directory\Background` IShellItemArray behavior (documented but not tested on this specific Win 11 version)
- `Directory\Background` support: HIGH — official schema documentation explicitly lists it as a valid `Type` value

**Research date:** 2026-04-09
**Valid until:** 2026-10-09 (stable Windows shell APIs; check if Win 11 SDK version changes)
