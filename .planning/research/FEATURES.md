# Feature Research

**Domain:** Windows 11 Shell Extension — Developer CLI Launcher (Context Menu)
**Researched:** 2026-04-08
**Confidence:** HIGH (core features), MEDIUM (differentiators), HIGH (anti-features)

## Feature Landscape

### Table Stakes (Users Expect These)

Features users assume exist. Missing these = product feels incomplete.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| Menu item appears in Win11 modern context menu (top level) | Every "open with X" tool does this; appearing in "Show more options" only is a failure mode — users won't find it | MEDIUM | Requires sparse MSIX package + AppxManifest; classic registry-only approaches land in the buried submenu |
| Works on right-click of a folder | Primary use case for all "open here" tools | LOW | HKEY_CLASSES_ROOT\Directory registration |
| Works on right-click inside a folder (background) | Secondary but equally expected use case; VS Code, terminal, git tools all support it | LOW | HKEY_CLASSES_ROOT\Directory\Background registration |
| Custom icon next to menu item | Every polished context menu entry has one; missing icon looks broken/unofficial | LOW | Path to icon in AppxManifest or registry; use Claude/Anthropic logo |
| Opens in the correct directory | Core contract — opens at the path the user right-clicked, not home dir | LOW | Pass `%V` (folder) or `%W` (background) to command; both must work correctly |
| Clean installer (no manual steps) | Dev tools on Windows are distributed as installers; users should not run scripts or edit registry manually | MEDIUM | Inno Setup or similar wrapping MSIX registration + any registry writes |
| Clean uninstaller (no orphan entries) | Users trust a tool that removes itself completely; leftover registry keys/packages erode trust | MEDIUM | Must remove MSIX sparse package registration, any registry entries written, and deployed files |
| Works without elevation (no UAC prompt for install) | Per-user install is expected for dev tools; requiring admin is friction | MEDIUM | HKEY_CURRENT_USER scope; `Add-AppxPackage` per-user works without elevation |
| Graceful error if Claude Code not found | Silent failure (nothing happens on click) destroys trust; a clear message tells users what to fix | LOW | Check for `claude.exe` at known paths before registering, or show dialog at launch time |
| Works across common Windows 11 setups | Tool must handle scoop installs, winget installs, manual path installs — not just one known path | MEDIUM | Auto-detect claude.exe via PATH + known install directories; store resolved path at install time |

### Differentiators (Competitive Advantage)

Features that set the product apart. Not required, but valuable.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| Auto-detects Claude Code path at install time | Other "open with" tools require users to specify the path or hardcode it; auto-detection reduces setup to zero config | MEDIUM | Search PATH, `~/.local/bin`, winget install dir, scoop shims; store result in registry at install time |
| Auto-detects Windows Terminal path | wt.exe can live in different places (Store, winget, manually installed); not finding it causes silent failure | LOW | wt.exe is nearly always on PATH on Win11; check %LOCALAPPDATA%\Microsoft\WindowsApps\ as fallback |
| Graceful "dependency not found" dialog | Instead of silently doing nothing, show a targeted message: "Claude Code not found. Install it with: npm i -g @anthropic-ai/claude-code" | MEDIUM | Implement a small launcher wrapper that checks before exec; cleaner than putting logic in a .bat |
| GitHub release with versioned artifacts | Community tools expect a GitHub Releases page with tagged installer downloads; makes it easy to reference in blog posts and issues | LOW | CI/CD producing .exe installer artifact per tag |
| Inno Setup installer with progress UI | .exe installer with a real UI (not just a silent script) signals this is a maintained, trustworthy tool | MEDIUM | Standard Inno Setup wizard; include license page, install dir selection |
| Support for Windows Terminal profiles (future) | Power users want to choose their terminal profile (PowerShell vs cmd vs WSL) | HIGH | Submenu via SplitMenuFlyoutItem (Windows App SDK 2.0); out of scope for v1 but architecture should not block it |

### Anti-Features (Commonly Requested, Often Problematic)

Features that seem good but create problems.

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| Submenu with multiple options (profiles, admin mode) | Power users want "Open in PowerShell", "Open in WSL", "Open as Admin" | Submenus require SplitMenuFlyoutItem (Windows App SDK 2.0 experimental) — technically complex, and this tool's value is one-click simplicity; adds scope that delays shipping | Keep v1 to single menu item; architecture the AppxManifest to support additional verbs later if demanded |
| Windows 10 support | Broader audience | Win10 support requires a different approach (.reg file only, no sparse MSIX); the project already has a .reg solution for that; mixing both approaches in one installer adds complexity for a feature that's explicitly out of scope | Point users to the existing .reg file for Win10; document in README |
| GUI settings panel | Users want to change target terminal, profile, icon | Config UI is a separate app surface with its own testing burden; for a single-purpose tool, there's nothing to configure — the value is zero-configuration | If a setting is ever needed (e.g., path override), use a simple registry key that advanced users can edit; document it |
| Desktop right-click support | "Open Claude here" from the desktop | The desktop in Windows 11 uses a different shell surface (CLSID_DesktopWallpaper); not a useful workflow for a CLI tool (desktop is not a code project directory) | Explicitly not supported; document why in README |
| Silent install without any UI | IT/enterprise users want silent deployment | Silent install of per-user MSIX + Inno Setup is possible but complicates the installer and is not the target user (individual developer); adds /SILENT flag complexity | Support it as a stretch goal only if there's demand; default to full installer UI |
| Microsoft Store distribution | Users expect Store apps to be discoverable | Store submission requires code signing, privacy policy, Microsoft review process, and ongoing compliance; too much overhead for a community dev tool | Distribute via GitHub Releases; this is where the target audience (developers) looks anyway |
| Auto-update | Nice to have | Adds background process or scheduled task complexity; shell extensions that auto-update can cause Explorer instability if the MSIX is swapped while loaded | Use GitHub Releases; users can reinstall over the top when they want an upgrade |

## Feature Dependencies

```
[Win11 Modern Context Menu Entry]
    └──requires──> [Sparse MSIX Package registered]
                       └──requires──> [AppxManifest.xml with verb declaration]
                       └──requires──> [Icon file deployed to known path]

[Correct Directory Launch]
    └──requires──> [Claude Code path resolved]
                       └──requires──> [Auto-detect at install time]
    └──requires──> [Windows Terminal path resolved]

[Clean Uninstall]
    └──requires──> [MSIX package registration tracked]
    └──requires──> [Registry entries written by installer are known]

[Graceful Error Dialog]
    └──requires──> [Launcher wrapper script or binary]
    └──enhances──> [Auto-detect Claude Code path] (fallback when detection fails)

[GitHub Release Artifacts]
    └──requires──> [Inno Setup installer]
    └──requires──> [Signed or self-signed MSIX package]
```

### Dependency Notes

- **Win11 Modern Context Menu requires Sparse MSIX:** There is no registry-only path to the Win11 top-level context menu. The MSIX package is the gate; everything else depends on it being registered correctly.
- **Correct Directory Launch requires resolved paths:** The command template (`wt.exe -d "<dir>" cmd /k claude`) needs both `wt.exe` and `claude.exe` to be findable. Resolution must happen at install time, not click time, to avoid per-click latency.
- **Graceful Error Dialog enhances Auto-detect:** Detection happens at install time; the error dialog is the fallback that fires at click time if the resolved path no longer exists (e.g., user uninstalled Claude Code after installing the extension).
- **Clean Uninstall requires installer to track what it wrote:** The uninstaller must know exactly what was deployed. Using Inno Setup's built-in uninstall tracking is the correct approach — avoid ad-hoc cleanup scripts.

## MVP Definition

### Launch With (v1)

Minimum viable product — what's needed to validate the concept.

- [ ] Menu item in Win11 top-level context menu (folder + folder background) — core value delivery
- [ ] Custom Claude icon — looks legitimate, not a bare text entry
- [ ] Opens Windows Terminal at the correct directory running `claude` — the whole point
- [ ] Auto-detect Claude Code and Windows Terminal paths at install time — zero-config is the promise
- [ ] Graceful error dialog if Claude Code not found at click time — prevents silent failure confusion
- [ ] Installer (.exe) that handles MSIX registration automatically — required for public distribution
- [ ] Uninstaller that removes everything cleanly — required for users to trust it

### Add After Validation (v1.x)

Features to add once core is working.

- [ ] GitHub Actions CI producing release artifacts automatically — when manual release becomes tedious
- [ ] Path override registry key (for non-standard installs) — when users report detection failures
- [ ] "Open as Administrator" variant — if there's clear community demand post-launch

### Future Consideration (v2+)

Features to defer until product-market fit is established.

- [ ] Submenu with terminal profile selection — only after Windows App SDK 2.0 SplitMenuFlyoutItem stabilizes and if users demand it
- [ ] WSL/Linux shell variant — if there is demand from WSL users

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| Win11 top-level context menu entry | HIGH | MEDIUM | P1 |
| Opens correct directory via Windows Terminal | HIGH | LOW | P1 |
| Custom icon | HIGH | LOW | P1 |
| Auto-detect Claude Code path | HIGH | MEDIUM | P1 |
| Graceful error dialog | HIGH | LOW | P1 |
| .exe installer | HIGH | MEDIUM | P1 |
| Clean uninstaller | HIGH | MEDIUM | P1 |
| Works on folder background right-click | MEDIUM | LOW | P1 |
| GitHub release artifacts | MEDIUM | LOW | P2 |
| Path override registry key | MEDIUM | LOW | P2 |
| CI/CD for releases | LOW | MEDIUM | P2 |
| Submenu / profile selection | LOW | HIGH | P3 |
| Auto-update | LOW | HIGH | P3 |

**Priority key:**
- P1: Must have for launch
- P2: Should have, add when possible
- P3: Nice to have, future consideration

## Competitor Feature Analysis

| Feature | VS Code ("Open with Code") | Notepad++ (nppShell) | Our Approach |
|---------|---------------------------|----------------------|--------------|
| Win11 modern context menu | Yes (sparse MSIX via Inno) | Yes (sparse MSIX + COM DLL) | Yes (sparse MSIX, no DLL) |
| Custom icon | Yes | Yes | Yes |
| Right-click folder | Yes | Yes (files) | Yes |
| Right-click folder background | Yes | No (file-centric) | Yes |
| Auto-detect target path | Bundled — path is known | Bundled — path is known | Must detect claude.exe dynamically |
| Graceful error on missing tool | Silent (editor is always present) | N/A | Explicit error dialog — differentiator |
| Installer | Inno Setup .exe | Bundled with Notepad++ installer | Inno Setup .exe |
| Uninstaller | Full uninstall via Inno | Bundled with Notepad++ uninstall | Full uninstall via Inno |
| Admin/elevated variant | No | No | Out of scope v1 |
| Submenu profiles | No | No | Out of scope v1 |

**Key observation:** VS Code's Aug 2025 regression (issues #260389, #260449, #260580) — where "Open with Code" disappeared after a routine update — shows that sparse MSIX registration is fragile and can silently break. The lesson: the installer must be easy to re-run, and users should know how to re-register if needed.

## Sources

- GitHub: microsoft/vscode-explorer-command — reference implementation of sparse MSIX context menu via Inno Setup
- GitHub: notepad-plus-plus/nppShell — COM DLL + sparse MSIX approach; confirms icon + clean uninstall are baseline expectations
- VS Code GitHub Issues #260389, #260449, #260580 (Aug 2025) — real-world evidence that MSIX context menu entries can silently break; informs error handling and re-register features
- Windows 11 Forum (elevenforum.com) — "Open in Terminal" threads confirm folder + folder-background are both required; admin variant is common request but adds complexity
- Microsoft Developer Blog (2021): "Extending the Context Menu and Share Dialog in Windows 11" — confirms sparse MSIX is the only path to top-level Win11 menu
- WinBuzzer (Nov 2025): Microsoft SplitMenuFlyoutItem — confirms submenu support requires Windows App SDK 2.0 exp3; not stable for v1

---
*Feature research for: Windows 11 shell extension — "Claude from here" context menu launcher*
*Researched: 2026-04-08*
