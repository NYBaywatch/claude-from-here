# Phase 02: Launcher and Config - Research

**Researched:** 2026-04-09
**Domain:** Win32 Registry C++ API, WinForms .NET 4.8, Windows executable path detection, command line flag assembly
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** GUI config app built with WinForms (.NET Framework 4.8). .NET Framework 4.8 ships with Windows 11, so no additional runtime required. Small footprint (~50KB .exe).
- **D-02:** Config app is a standalone .exe (ClaudeFromHereConfig.exe) that users can launch from Start Menu or a shortcut.
- **D-03:** CLI flag settings stored in Windows Registry under `HKCU\Software\ClaudeFromHere`. DLL reads via `RegGetValueW` at Invoke time with zero file I/O overhead.
- **D-04:** Registry key structure: `Model` (REG_SZ), `Verbose` (REG_DWORD), `AllowedTools` (REG_SZ), `ExtraFlags` (REG_SZ for free-text additional flags).
- **D-05:** Core flags exposed as structured controls: Model as dropdown, Verbose as checkbox, AllowedTools as text field.
- **D-06:** Free-text field for additional/custom flags the user wants to append to every `claude` invocation.
- **D-07:** DLL reads registry values in `Invoke()` before building the command line. If registry key doesn't exist or values are empty, launch with no extra flags (current behavior).
- **D-08:** Command line format: `wt.exe -d "<path>" -- cmd /k claude <configured-flags>`
- **D-09:** DLL tries PATH first via `SearchPathW`, then falls back to App Paths registry keys (`HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\wt.exe` and `...\claude.exe`) and Windows Terminal's App Execution Alias.
- **D-10:** Detection order: SearchPathW (PATH) -> App Paths registry -> known execution alias locations. First match wins.
- **D-11:** Keep `MessageBoxW` but improve the text to include install instructions. For claude.exe: `npm i -g @anthropic-ai/claude-code`. For wt.exe: point to Microsoft Store. Include "Then restart Explorer" guidance.
- **D-12:** No custom dialog windows for errors -- MessageBox is sufficient and keeps the DLL simple.

### Claude's Discretion

- WinForms project structure and .csproj configuration
- Exact registry value names and types (within the D-04 framework)
- App Paths / execution alias lookup implementation details
- Config app icon and window sizing

### Deferred Ideas (OUT OF SCOPE)

None -- discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| LNCH-01 | Clicking the menu item opens Windows Terminal in the right-clicked directory with `claude` running | DLL `_LaunchClaude()` rewrite with registry flag reads + `lpApplicationName = szWt` + updated command line format |
| LNCH-02 | Installer auto-detects the claude.exe path (no manual configuration required) | SearchPathW -> HKCU/HKLM App Paths fallback chain for claude.exe |
| LNCH-03 | Installer auto-detects the wt.exe path (no manual configuration required) | SearchPathW -> HKCU App Paths (confirmed path for Store-installed wt.exe) -> HKLM -> execution alias fallback |
| LNCH-04 | User sees a clear error dialog if Claude Code is not found when clicking the menu item | Updated `MessageBoxW` copy per UI-SPEC.md Surface 2 |
| LNCH-05 | User sees a clear error dialog if Windows Terminal is not found when clicking the menu item | Updated `MessageBoxW` copy per UI-SPEC.md Surface 2 |
| CONF-01 | User can open a simple GUI app to configure Claude CLI flags passed to the `claude` command | New `ClaudeFromHereConfig.exe` WinForms net48 project |
| CONF-02 | Common flags are exposed as checkboxes/dropdowns (e.g., --model, --verbose, --allowedTools) | WinForms ComboBox + CheckBox + TextBox controls per UI-SPEC.md |
| CONF-03 | Configured flags are persisted across sessions and applied on every launch | C# `Registry.CurrentUser.CreateSubKey()` writes; C++ `RegGetValueW` reads in DLL |
</phase_requirements>

---

## Summary

Phase 2 has two implementation tracks running in parallel: (1) enhancing the existing C++ DLL's `_LaunchClaude()` method with registry flag reads, improved path detection, and updated error dialogs, and (2) creating a new C# WinForms config app that writes those registry values. Both tracks share a single registry namespace under `HKCU\Software\ClaudeFromHere`.

The .NET 4.8 WinForms build works on this machine via `dotnet build` with `<TargetFramework>net48</TargetFramework>` — confirmed in a live build test. The dotnet SDK (9.0.312) can target net48 even though `dotnet new winforms` only advertises net6-9; the .NET 4.8 runtime (`v4.0.30319`) handles compilation and the output is a standalone 4-5KB `.exe` with only a `.exe.config` companion (no extra DLLs needed, GAC provides WinForms assemblies).

A critical discovery about wt.exe path detection: Windows Terminal installed from the Microsoft Store registers its App Paths entry in **HKCU** (not HKLM as D-09 implies). The detection order for wt.exe must check HKCU first. Additionally, `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` is a reliable execution alias fallback — it exists on all machines with Store-installed wt.exe and is launchable via `CreateProcessW`.

**Primary recommendation:** Implement `_LaunchClaude()` refactor and config app as two separate plan tasks. Add `advapi32` to CMakeLists DLL link libraries. Build config app via `dotnet build` step added to `register.ps1`.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| Win32 Registry API (`RegGetValueW`) | Windows SDK 10.0.26100.0 | DLL reads config flags at invoke time | Zero-dependency, available in all Win32 apps; in `advapi32.dll` |
| WinForms (.NET Framework 4.8) | 4.8.9221 (pre-installed Win 11) | Config app GUI | D-01 locked; no runtime deployment friction; confirmed buildable |
| `Microsoft.Win32.Registry` | net48 built-in (mscorlib) | C# registry read/write in config app | Ships with .NET Framework 4.8; no NuGet packages needed |
| `strsafe.h` (`StringCbCatW`, `StringCbPrintfW`) | Windows SDK | Safe flag string assembly in C++ DLL | Already in use in Phase 1; prevents buffer overflows |

### Supporting

| Library / Tool | Version | Purpose | When to Use |
|----------------|---------|---------|-------------|
| `advapi32.lib` | Windows SDK | Links `RegGetValueW`, `RegOpenKeyExW` into DLL | Must be added to `CMakeLists.txt` target_link_libraries — not currently present |
| `ExpandEnvironmentStringsW` | kernel32 (via `windows.h`) | Resolves `%LOCALAPPDATA%` for execution alias fallback | Already available; no new include or lib needed |
| `PathFileExistsW` | `shlwapi.h` + `shlwapi.lib` | Validates execution alias path | Already linked in Phase 1 |
| `dotnet build` | dotnet SDK 9.0.312 | Compiles config app .csproj targeting net48 | Called from `register.ps1` Step 0 alongside CMake build |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `RegGetValueW` (one-call) | `RegOpenKeyExW` + `RegQueryValueExW` | Two calls vs one; `RegGetValueW` is simpler, handles missing key gracefully with `RRF_ZEROONFAILURE` |
| `dotnet build` in register.ps1 | Add C# project to CMakeLists via `execute_process` | CMakeLists approach is fragile for mixed C#/C++ builds; `dotnet build` in PS1 is the natural extension of existing Step 0 |
| net48 WinForms | net9.0-windows WinForms | net9 requires .NET 9 runtime on end-user machine; net48 uses pre-installed framework; D-01 locks net48 |

**Installation:**
```bash
# No new NuGet packages needed.
# Only CMakeLists.txt change: add advapi32 to DLL link libs.
# Config app uses dotnet build with net48 target.
dotnet build src/ClaudeFromHereConfig/ClaudeFromHereConfig.csproj -c Release -o build/
```

---

## Architecture Patterns

### Recommended Project Structure

```
src/
├── ClaudeFromHere.cpp       # Modify: _LaunchClaude() registry reads + path detection + flags
├── ClaudeFromHereConfig/    # New directory for WinForms config app
│   ├── ClaudeFromHereConfig.csproj
│   ├── Program.cs           # Entry point: [STAThread], Application.Run(new MainForm())
│   └── MainForm.cs          # WinForms form: settings UI + registry read/write
build/
├── ClaudeFromHere.dll       # Existing (C++ COM DLL)
├── ClaudeFromHere.exe       # Existing (AppxManifest stub)
├── ClaudeFromHereConfig.exe # New (config app output here)
├── ClaudeFromHereConfig.exe.config  # New (.exe.config for net48)
└── claude.ico               # Existing
scripts/
└── register.ps1             # Modify: add dotnet build step in Step 0
CMakeLists.txt               # Modify: add advapi32 to DLL link libraries
```

### Pattern 1: Registry Flag Read in C++ DLL (RegGetValueW)

**What:** Read all four config values from `HKCU\Software\ClaudeFromHere` at the start of `_LaunchClaude()`. Absence of key or empty value = omit that flag. Zero overhead path: one registry open, four value reads, close.

**When to use:** In `_LaunchClaude()` before building `szCmdLine`.

**Example:**
```cpp
// Source: Windows SDK winreg.h, advapi32.dll
// Add advapi32 to CMakeLists.txt target_link_libraries

WCHAR szModel[256]        = {};
WCHAR szAllowedTools[1024] = {};
WCHAR szExtraFlags[2048]  = {};
DWORD dwVerbose           = 0;

DWORD cb = sizeof(szModel);
RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Model",
    RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szModel, &cb);

cb = sizeof(dwVerbose);
RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"Verbose",
    RRF_RT_REG_DWORD | RRF_ZEROONFAILURE, nullptr, &dwVerbose, &cb);

cb = sizeof(szAllowedTools);
RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"AllowedTools",
    RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szAllowedTools, &cb);

cb = sizeof(szExtraFlags);
RegGetValueW(HKEY_CURRENT_USER, L"Software\\ClaudeFromHere", L"ExtraFlags",
    RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szExtraFlags, &cb);

// Build flags string
WCHAR szFlags[4096] = {};
if (szModel[0]) {
    StringCbCatW(szFlags, sizeof(szFlags), L" --model ");
    StringCbCatW(szFlags, sizeof(szFlags), szModel);
}
if (dwVerbose) {
    StringCbCatW(szFlags, sizeof(szFlags), L" --verbose");
}
if (szAllowedTools[0]) {
    StringCbCatW(szFlags, sizeof(szFlags), L" --allowedTools ");
    StringCbCatW(szFlags, sizeof(szFlags), szAllowedTools);
}
if (szExtraFlags[0]) {
    StringCbCatW(szFlags, sizeof(szFlags), L" ");
    StringCbCatW(szFlags, sizeof(szFlags), szExtraFlags);
}
```

### Pattern 2: Executable Path Detection Chain

**What:** Three-stage fallback for each executable. For wt.exe, HKCU App Paths is checked before HKLM — Store installs register per-user.

**When to use:** In `_LaunchClaude()` replacing the current `SearchPathW`-only approach.

**Example:**
```cpp
// Source: Windows SDK, verified on this machine with actual wt.exe Store install

// Returns TRUE and fills szOut if exe found. szOut = MAX_PATH buffer.
static BOOL FindExecutable(PCWSTR exeName, PCWSTR appPathsSubkey, PWSTR szOut, DWORD cchOut)
{
    // Stage 1: SearchPathW (covers PATH, including %LOCALAPPDATA%\Microsoft\WindowsApps)
    if (SearchPathW(nullptr, exeName, nullptr, cchOut, szOut, nullptr))
        return TRUE;

    // Stage 2: HKCU App Paths (Windows Terminal Store install registers here)
    DWORD cb = cchOut * sizeof(WCHAR);
    if (RegGetValueW(HKEY_CURRENT_USER, appPathsSubkey, nullptr,
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szOut, &cb) == ERROR_SUCCESS
        && szOut[0])
        return TRUE;

    // Stage 3: HKLM App Paths (winget / system-wide installs)
    cb = cchOut * sizeof(WCHAR);
    if (RegGetValueW(HKEY_LOCAL_MACHINE, appPathsSubkey, nullptr,
            RRF_RT_REG_SZ | RRF_ZEROONFAILURE, nullptr, szOut, &cb) == ERROR_SUCCESS
        && szOut[0])
        return TRUE;

    // Stage 4 (wt.exe only): known execution alias location
    // (only relevant if the caller passes the alias path pattern)
    return FALSE;
}

// Usage in _LaunchClaude():
WCHAR szWt[MAX_PATH] = {};
BOOL wtFound = FindExecutable(L"wt.exe",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\wt.exe",
    szWt, ARRAYSIZE(szWt));

// Stage 4 fallback for wt.exe: execution alias
if (!wtFound) {
    ExpandEnvironmentStringsW(
        L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\wt.exe",
        szWt, ARRAYSIZE(szWt));
    wtFound = PathFileExistsW(szWt);
    if (!wtFound) szWt[0] = L'\0';
}

// Note: Stage 4 is NOT applicable to claude.exe (no execution alias exists for it)
WCHAR szClaude[MAX_PATH] = {};
BOOL claudeFound = FindExecutable(L"claude.exe",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths\\claude.exe",
    szClaude, ARRAYSIZE(szClaude));
```

### Pattern 3: CreateProcessW with Full Path to wt.exe

**What:** Use `lpApplicationName = szWt` to avoid quoting issues when szWt contains spaces (e.g., `C:\Program Files\WindowsApps\Microsoft.WindowsTerminal_...\wt.exe`). Keep `szCmdLine` starting with `wt.exe` for argv[0] convention.

**When to use:** Always, after path detection resolves szWt.

**Example:**
```cpp
// Build command line with flags
// lpApplicationName = szWt avoids space-in-path quoting issues
WCHAR szCmdLine[32768] = {};
StringCbPrintfW(szCmdLine, sizeof(szCmdLine),
    L"wt.exe -d \"%s\" -- cmd /k claude%s", szPath, szFlags);

STARTUPINFOW si = { sizeof(si) };
PROCESS_INFORMATION pi = {};
CreateProcessW(
    szWt,      // lpApplicationName -- full path, avoids quoting
    szCmdLine, // lpCommandLine -- wt.exe args
    nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi);
```

### Pattern 4: WinForms net48 .csproj SDK-Style

**What:** Use the dotnet SDK project system with `<TargetFramework>net48</TargetFramework>`. No NuGet packages. Output path set to `../../build/` so the exe lands alongside the DLL.

**Example:**
```xml
<!-- src/ClaudeFromHereConfig/ClaudeFromHereConfig.csproj -->
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net48</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <AssemblyName>ClaudeFromHereConfig</AssemblyName>
    <RootNamespace>ClaudeFromHereConfig</RootNamespace>
    <Nullable>enable</Nullable>
    <OutputPath>..\..\build\</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
  </PropertyGroup>
</Project>
```

### Pattern 5: C# Registry Read/Write

**What:** `Microsoft.Win32.Registry` in net48. `OpenSubKey` for reads (returns null if missing — handle gracefully). `CreateSubKey` for writes (creates if absent).

**Example:**
```csharp
// Source: Microsoft.Win32.Registry, net48 (mscorlib) - no NuGet needed
using Microsoft.Win32;

private const string RegistryPath = @"Software\ClaudeFromHere";

// Load settings from registry on form open
private void LoadSettings()
{
    using var key = Registry.CurrentUser.OpenSubKey(RegistryPath);
    if (key == null) return; // key absent = defaults (all empty/unchecked)

    modelComboBox.SelectedItem = key.GetValue("Model", "") as string ?? "";
    verboseCheckBox.Checked = (int)(key.GetValue("Verbose", 0) ?? 0) != 0;
    allowedToolsTextBox.Text = key.GetValue("AllowedTools", "") as string ?? "";
    extraFlagsTextBox.Text   = key.GetValue("ExtraFlags",   "") as string ?? "";
}

// Write settings on Apply
private void ApplySettings()
{
    using var key = Registry.CurrentUser.CreateSubKey(RegistryPath);
    key.SetValue("Model",        modelComboBox.SelectedItem?.ToString() ?? "", RegistryValueKind.String);
    key.SetValue("Verbose",      verboseCheckBox.Checked ? 1 : 0,              RegistryValueKind.DWord);
    key.SetValue("AllowedTools", allowedToolsTextBox.Text ?? "",               RegistryValueKind.String);
    key.SetValue("ExtraFlags",   extraFlagsTextBox.Text   ?? "",               RegistryValueKind.String);
}
```

### Anti-Patterns to Avoid

- **Linking RegGetValueW without advapi32:** The DLL currently links only `shlwapi`, `shell32`, `ole32`. `RegGetValueW` is in `advapi32.dll` — must add `advapi32` to `target_link_libraries` in CMakeLists.txt or get LNK2019.
- **Checking HKLM before HKCU for wt.exe App Paths:** On this machine, wt.exe (Store install) has its App Paths entry in HKCU only. HKLM check returns nothing. Always check HKCU first.
- **Omitting the execution alias fallback for wt.exe:** `SearchPathW` relies on PATH containing `%LOCALAPPDATA%\Microsoft\WindowsApps`. If a user has customized PATH and removed that directory, SearchPathW fails even though wt.exe works. The Stage 4 `ExpandEnvironmentStringsW` fallback catches this.
- **Setting lpApplicationName = NULL when szWt contains spaces:** `C:\Program Files\WindowsApps\Microsoft.WindowsTerminal_...\wt.exe` has spaces in "Program Files". CreateProcessW with NULL lpApplicationName parses to `C:\Program` which fails. Use `lpApplicationName = szWt`.
- **Using `dotnet new winforms` with net48:** The template only lists net6-9 as options. Use `--framework net9.0` and then manually set `net48` in .csproj, OR create the .csproj manually. The dotnet SDK builds net48 fine — the template wizard just doesn't offer it.
- **Forgetting `<AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>`:** Without this, dotnet build outputs to `build/net48/ClaudeFromHereConfig.exe` instead of `build/ClaudeFromHereConfig.exe`. The DLL and config app must be in the same flat `build/` directory.
- **Deploying net48 with redistributable DLLs:** net48 WinForms uses the GAC. Output is only `.exe` + `.exe.config`. Do NOT add runtime DLLs — they will break or be redundant.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Registry read with missing-key handling | Manual `RegOpenKeyExW` + null check + `RegQueryValueExW` | `RegGetValueW` with `RRF_ZEROONFAILURE` | One call handles absent key, absent value, wrong type, and zero-fills the buffer on failure |
| PATH + registry executable discovery | Custom PATH scanner | `SearchPathW` + `RegGetValueW` on App Paths | `SearchPathW` internally handles the PATH env var correctly including Unicode paths; App Paths is the OS standard for per-user executable registration |
| Safe string concatenation | Manual `wcscat` with length tracking | `StringCbCatW` from `strsafe.h` | Already used in Phase 1; prevents buffer overrun on untrusted registry data |
| WinForms system color handling | `Color.FromArgb(...)` hardcoded values | `SystemColors.*` properties | Per UI-SPEC.md: must respect Windows light/dark mode and High Contrast |

---

## Common Pitfalls

### Pitfall 1: advapi32 Missing from DLL Link Libraries

**What goes wrong:** `LNK2019: unresolved external symbol RegGetValueW` when building the DLL after adding registry reads.

**Why it happens:** Phase 1 CMakeLists.txt only links `shlwapi`, `shell32`, `ole32`. `RegGetValueW` is in `advapi32.dll`/`advapi32.lib`.

**How to avoid:** Add `advapi32` to `target_link_libraries(ClaudeFromHere PRIVATE ...)` in CMakeLists.txt before writing any registry code.

**Warning signs:** LNK2019 error for `__imp_RegGetValueW` during cmake build.

---

### Pitfall 2: wt.exe App Paths in HKCU, Not HKLM

**What goes wrong:** The App Paths fallback for wt.exe finds nothing in HKLM (where D-09 directed), even though wt.exe is installed. The execution alias fallback then catches it, but if that's also absent, the user gets "Windows Terminal not found" even with WT installed.

**Why it happens:** Windows Terminal installed from the Microsoft Store performs a per-user install and registers App Paths under `HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe` — not HKLM. This is verified on this machine.

**How to avoid:** Query HKCU App Paths before HKLM in the detection chain. The `FindExecutable` helper pattern above handles this correctly.

**Warning signs:** `reg query HKLM\SOFTWARE\...\App Paths\wt.exe` returns nothing; `reg query HKCU\Software\...\App Paths\wt.exe` returns the path.

---

### Pitfall 3: OutputPath Nesting for net48 Config App

**What goes wrong:** Config app builds to `build/net48/ClaudeFromHereConfig.exe` instead of `build/ClaudeFromHereConfig.exe`. The installer (Phase 3) and register.ps1 won't find it in the expected flat `build/` directory.

**Why it happens:** dotnet SDK's default behavior appends `/<TargetFramework>/` to the output path.

**How to avoid:** Add `<AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>` to the .csproj PropertyGroup.

**Warning signs:** After `dotnet build`, the exe is in `build/net48/` subdirectory.

---

### Pitfall 4: `StringCbCatW` vs `StringCchCatW` Mixing

**What goes wrong:** Buffer overrun or silent truncation when assembling the flags string if `StringCbCatW` (byte count) and `StringCchCatW` (character count) are mixed, or if `sizeof()` is used where character count is expected.

**Why it happens:** Phase 1 uses `StringCbPrintfW` (byte-based). Maintaining consistency is important.

**How to avoid:** Use `StringCbCatW` throughout (byte-based). Pass `sizeof(szFlags)` as the buffer size parameter, not `ARRAYSIZE(szFlags)`.

**Warning signs:** Extra garbage characters at end of flags string, or truncation mid-flag.

---

### Pitfall 5: `RegGetValueW` cbData Parameter Units

**What goes wrong:** String buffer reads truncated or HRESULT fail with `ERROR_MORE_DATA` because `cbData` was passed as character count instead of byte count.

**Why it happens:** `RegGetValueW`'s `pcbData` parameter is in **bytes** for `REG_SZ`, not characters. `sizeof(szModel)` is correct; `ARRAYSIZE(szModel)` is wrong.

**How to avoid:** Always pass `sizeof(buffer)` (bytes) to `RegGetValueW`'s cbData parameter. For DWORD values, pass `sizeof(DWORD)`.

---

### Pitfall 6: Model Dropdown Showing Stale Model Names

**What goes wrong:** Dropdown lists model names from Q4 2025 that no longer exist; users get claude CLI errors when selecting them.

**Why it happens:** Model names are hardcoded in the config app source.

**How to avoid:** Per UI-SPEC.md, the dropdown items are a discretion area. As of April 2026, the running model is `claude-sonnet-4-6` and the UI-SPEC lists `claude-opus-4-5`, `claude-sonnet-4-5`, `claude-haiku-3-5`. Use those per the approved UI-SPEC, but also include the short aliases (`sonnet`, `opus`, `haiku`) since `claude --help` confirms they are accepted. The text field for ExtraFlags serves as the escape hatch for future model names.

---

## Code Examples

Verified patterns from build tests and system inspection:

### WinForms net48 .csproj (SDK-style, verified buildable)

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>WinExe</OutputType>
    <TargetFramework>net48</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <AssemblyName>ClaudeFromHereConfig</AssemblyName>
    <RootNamespace>ClaudeFromHereConfig</RootNamespace>
    <OutputPath>..\..\build\</OutputPath>
    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>
  </PropertyGroup>
</Project>
```

Build command (to add to register.ps1 Step 0):
```powershell
$DotNet = "dotnet"
Invoke-ExternalTool "dotnet build config app" $DotNet @(
    "build", "$PSScriptRoot\..\src\ClaudeFromHereConfig\ClaudeFromHereConfig.csproj",
    "--configuration", "Release",
    "--nologo",
    "-v", "minimal"
)
```

### CMakeLists.txt — Add advapi32

```cmake
target_link_libraries(ClaudeFromHere PRIVATE
    shlwapi
    shell32
    ole32
    advapi32   # <-- ADD: required for RegGetValueW
)
```

### Program.cs Entry Point (net48 WinForms)

```csharp
using System;
using System.Windows.Forms;

namespace ClaudeFromHereConfig
{
    class Program
    {
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            Application.Run(new MainForm());
        }
    }
}
```

### Path Detection Status in Config App (for "Path Detection" GroupBox)

```csharp
// Mirrors the DLL detection logic for display in the config app
// Source: verified against actual wt.exe HKCU App Paths on this machine
private string FindExecutablePath(string exeName, string appPathsSubkey)
{
    // Stage 1: PATH
    foreach (var dir in (Environment.GetEnvironmentVariable("PATH") ?? "").Split(';'))
    {
        try {
            var full = System.IO.Path.Combine(dir.Trim(), exeName);
            if (System.IO.File.Exists(full)) return full;
        } catch { }
    }

    // Stage 2: HKCU App Paths
    using (var key = Registry.CurrentUser.OpenSubKey(appPathsSubkey))
    {
        var path = key?.GetValue(null) as string;
        if (!string.IsNullOrEmpty(path) && System.IO.File.Exists(path)) return path;
    }

    // Stage 3: HKLM App Paths
    using (var key = Registry.LocalMachine.OpenSubKey(appPathsSubkey))
    {
        var path = key?.GetValue(null) as string;
        if (!string.IsNullOrEmpty(path) && System.IO.File.Exists(path)) return path;
    }

    // Stage 4: execution alias (wt.exe only)
    if (exeName == "wt.exe")
    {
        var alias = System.IO.Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            @"Microsoft\WindowsApps\wt.exe");
        if (System.IO.File.Exists(alias)) return alias;
    }

    return null; // Not found
}
```

---

## Environment Availability

| Dependency | Required By | Available | Version | Fallback |
|------------|------------|-----------|---------|----------|
| .NET Framework 4.8 runtime | WinForms config app (end-user) | ✓ | 4.8.09221 (pre-installed Win 11) | — |
| dotnet SDK | Build config app .csproj | ✓ | 9.0.312 | — |
| MSBuild 17.14 | dotnet SDK backend | ✓ | 17.14.40 | — |
| .NET 4.8 targeting pack (ref assemblies) | Build against net48 | ✓ (via dotnet SDK) | Verified: `dotnet build net48` succeeds | — |
| advapi32.lib | DLL links RegGetValueW | ✓ | Windows SDK 10.0.26100.0 | — |
| cmake | DLL C++ build | ✓ | 3.31.6 (in VS BuildTools) | — |
| MSVC compiler | DLL C++ build | ✓ | 14.44 (VS2022 BuildTools) | — |
| wt.exe | Launch target | ✓ | Installed via Store (HKCU App Paths) | Error dialog LNCH-05 |
| claude.exe | Launch target | ✓ | 2.1.98 (`~/.local/bin/`) | Error dialog LNCH-04 |

**Missing dependencies with no fallback:** None.

**Missing dependencies with fallback:** wt.exe and claude.exe are end-user dependencies — absence is expected on fresh installs and is handled by the error dialogs (LNCH-04, LNCH-05).

---

## Sources

### Primary (HIGH confidence)

- Windows SDK `winreg.h` + `advapi32.dll` — `RegGetValueW`, `RRF_ZEROONFAILURE`, `RRF_RT_REG_SZ`, `RRF_RT_REG_DWORD` — verified via live C++ compile
- Windows SDK `strsafe.h` — `StringCbCatW`, `StringCbPrintfW` — already in use, Phase 1
- Windows SDK `shlwapi.h` — `PathFileExistsW`, `ExpandEnvironmentStringsW` — already linked
- dotnet SDK 9.0.312 — `net48` WinForms build via `dotnet build` — verified live build test (4.5KB exe, no extra DLLs)
- HKCU App Paths for wt.exe — `HKCU\Software\Microsoft\Windows\CurrentVersion\App Paths\wt.exe` — verified via `reg query` on this machine
- claude CLI 2.1.98 `--help` — confirmed `--allowedTools`, `--model`, `--verbose` flag syntax
- `microsoft.win32.registry` C# API — net48 built-in (mscorlib) — standard API, no NuGet needed
- Phase 1 source: `src/ClaudeFromHere.cpp` (lines 255-318) — existing `_LaunchClaude()` to be modified

### Secondary (MEDIUM confidence)

- Windows Terminal execution alias location `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` — verified as symlink/reparse point on this machine; behavior on other machines is consistent with Store install pattern

### Tertiary (LOW confidence)

- Model names in dropdown (claude-opus-4-5, claude-sonnet-4-5, claude-haiku-3-5) — per UI-SPEC.md approved values; actual API availability not verified against Anthropic docs

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libs verified via live builds and system inspection
- Architecture: HIGH — patterns derived from existing Phase 1 code and verified APIs
- Pitfalls: HIGH for items 1-5 (discovered during research); MEDIUM for item 6 (model names)

**Research date:** 2026-04-09
**Valid until:** 2026-05-09 (stable Win32 APIs; model names may change sooner)
