# Pitfalls Research

**Domain:** Windows 11 sparse MSIX shell extension / context menu installer
**Researched:** 2026-04-08
**Confidence:** HIGH (official Microsoft docs + multiple verified community sources)

---

## Critical Pitfalls

### Pitfall 1: Manifest Identity Must Exactly Match the Signing Certificate

**What goes wrong:**
The `Publisher` attribute in the AppxManifest.xml `<Identity>` element must be an exact string match to the `Subject` field of the certificate used to sign the MSIX package. Even minor differences (e.g., extra spaces, wrong CN= format) cause registration to silently fail or throw 0x80073CF6. The package appears to register successfully but the app gets no identity, leading to bizarre symptoms like the context menu never appearing.

**Why it happens:**
Developers copy a Publisher string from docs or examples without realizing it must match their specific certificate subject. The CN= prefix must be present in the manifest (`CN=MyCompany`) but is sometimes omitted when creating the cert. The error messages don't clearly point to the mismatch.

**How to avoid:**
Generate the signing certificate first, then copy the exact Subject string into the manifest — not the other way around. Run `Get-ChildItem Cert:\CurrentUser\My | Select Subject` to retrieve the exact string. Validate before building the MSIX.

**Warning signs:**
- `Add-AppxPackage` succeeds but `Get-AppxPackage <name>` returns nothing
- Context menu never appears after registration reports success
- App restarts in an endless loop (identity check fails, tries to re-register, repeats)

**Phase to address:** Phase: MSIX Package Construction (manifest authoring step)

---

### Pitfall 2: Self-Signed Certificate Requires Explicit Trust Installation on End-User Machines

**What goes wrong:**
A self-signed certificate works on the developer's machine (where the cert is already trusted) but fails silently on any other machine. End users cannot install an MSIX signed with an untrusted certificate — the `Add-AppxPackage` command fails with a trust error. This is a hard blocker for public distribution.

**Why it happens:**
Self-signed certs are valid for development but not for public trust chains. The workaround (import cert into `Cert:\LocalMachine\TrustedPeople`) requires admin elevation and is not acceptable for a public tool targeting zero-friction install.

**How to avoid:**
For public/community distribution, use a production code-signing certificate from a trusted CA, or use Azure Trusted Signing (formerly Azure Code Signing). The MSIX must be signed with a publicly trusted certificate before the installer ships. Do not ship a self-signed cert to end users and expect them to manually trust it.

**Warning signs:**
- Install works on dev machine but fails on a clean VM
- Error message references "untrusted certificate" or HRESULT 0x800B0109
- Testers report the installer fails immediately after launch

**Phase to address:** Phase: Installer / Distribution (before any external testing begins)

---

### Pitfall 3: Per-User MSIX Registration Does Not Propagate to Other Users

**What goes wrong:**
When the sparse MSIX is registered per-user (the default with `Add-AppxPackage`), the context menu only appears for the user who ran the installer. Other users on the same machine, and any new user accounts created later, will not see the menu item.

**Why it happens:**
MSIX package registrations are stored in per-user app model state. The context menu handler registration is tied to the package identity of the installing user. There is no automatic propagation to other user profiles.

**How to avoid:**
For a machine-wide install (all users), use `Add-AppxPackage -Stage ... ; Add-AppxProvisionedPackage -Online` which stages the package and provisions it for all current and future users. This requires admin elevation. Design the installer to offer per-user (no elevation) or machine-wide (with elevation prompt) modes, and document the limitation clearly.

**Warning signs:**
- "It works for me but not my wife's account" bug reports
- QA testing with a secondary Windows account shows no menu item
- Installer completes without errors but other users see nothing

**Phase to address:** Phase: Installer (registration strategy must be decided before coding the installer)

---

### Pitfall 4: ExternalLocation Path Must Be Absolute and Match the Actual Install Directory

**What goes wrong:**
The `Add-AppxPackage -ExternalLocation <path>` argument must point to the exact directory where the application's executable lives. If the installer places files in one directory and passes a different path (or a relative path), the package identity registration succeeds but the app cannot claim its identity at runtime, and the context menu never fires.

**Why it happens:**
Installer scripts often use relative paths, environment variable expansions, or trailing slashes inconsistently. The OS does exact string or path comparison — any mismatch breaks the linkage between the identity package and the external files.

**How to avoid:**
Always use fully-resolved absolute paths when calling `Add-AppxPackage -ExternalLocation`. In Inno Setup or NSIS, use the `{app}` constant (Inno) or `$INSTDIR` (NSIS) which are guaranteed to be the correct absolute install path. Test with `Get-AppxPackage <name> | Select InstallLocation` to verify the recorded path.

**Warning signs:**
- Package registers but `GetCurrentPackageFullName()` returns error 15700 (APPMODEL_ERROR_NO_PACKAGE)
- Context menu item appears but clicking it does nothing
- Event log shows COM activation failures

**Phase to address:** Phase: Installer (registration command construction)

---

### Pitfall 5: Version Bump Required for Every Re-Registration of the Same Package

**What goes wrong:**
You cannot register a sparse MSIX package with the same `Version` attribute in the manifest if that version is already registered on the system. Attempting to do so with `Add-AppxPackage` fails. This means reinstalling the app, or running the installer a second time on the same machine, will fail at the MSIX registration step unless the old version is first removed.

**Why it happens:**
The MSIX app model treats the combination of (Name + Publisher + Version) as a unique package identifier. Duplicate registration of an identical version is explicitly rejected.

**How to avoid:**
Two approaches: (1) Always increment the MSIX manifest version to match the application version, and unregister the old package before registering the new one. (2) In the installer's registration step, run `Remove-AppxPackage` for any existing registration before calling `Add-AppxPackage`. Make the unregister step non-fatal (ignore errors if not installed).

**Warning signs:**
- Second install attempt fails with 0x80073CF3 or similar
- Upgrade path testing breaks on the registration step
- CI/CD deployment fails on a machine that already has the tool installed

**Phase to address:** Phase: Installer (must handle upgrade path, not just first install)

---

### Pitfall 6: wt.exe Has Multiple Possible Locations and `where.exe` Can Return the Wrong One

**What goes wrong:**
Windows Terminal (`wt.exe`) can be installed in at least two locations: `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` (MSIX app alias, always present) and inside `%ProgramFiles%\WindowsApps\Microsoft.WindowsTerminal_*\wt.exe` (actual binary). If both Windows Terminal and Windows Terminal Preview are installed, `where.exe wt.exe` can return multiple results, and the first result may be the Preview version or a stale alias.

**Why it happens:**
Windows Terminal is an MSIX app, so its actual binary is version-stamped and lives in a protected directory. The alias in `WindowsApps` is the reliable entry point but is per-user. The app may not be installed at all on some systems (pre-22H2 the Store app had to be manually installed).

**How to avoid:**
Resolve `wt.exe` by checking `%LOCALAPPDATA%\Microsoft\WindowsApps\wt.exe` first (the MSIX app execution alias — always points to the correct installed version). Fall back to `where.exe wt.exe` only if needed. If not found, show a clear error: "Windows Terminal not found. Please install it from the Microsoft Store." Do not hardcode any path with a version string in the `WindowsApps` folder.

**Warning signs:**
- Context menu works on dev machine but silently does nothing on a clean install
- Wrong terminal version launches (Preview instead of stable)
- `wt.exe` command errors out on machines without Windows Terminal installed

**Phase to address:** Phase: Core Logic (path detection and command construction)

---

### Pitfall 7: Uninstaller Must Remove Both the MSIX Registration and All File Artifacts

**What goes wrong:**
The MSIX registration creates OS-level state (app model registration) that is separate from the files on disk. If the uninstaller only deletes files or only runs `Remove-AppxPackage`, leftover state causes problems: the context menu item appears but does nothing (dangling MSIX registration), or re-installation fails because a registration for a non-existent package is still present.

**Why it happens:**
Developers test installation thoroughly but treat uninstall as an afterthought. The MSIX registration is invisible — it does not appear in Programs and Features in the same way a normal app does, so it is easy to forget.

**How to avoid:**
The uninstaller must: (1) Run `Remove-AppxPackage` (or `Remove-AppxPackage -AllUsers` for machine-wide installs), (2) delete all installed files, (3) remove any registry keys written outside of MSIX (e.g., HKCU for classic context menu fallback). Test the uninstall → reinstall round-trip explicitly as part of QA.

**Warning signs:**
- After uninstall, re-install fails at MSIX registration step
- After uninstall, context menu item still appears but errors on click
- `Get-AppxPackage` still shows the package after uninstall

**Phase to address:** Phase: Installer (uninstall path must be built alongside install path)

---

### Pitfall 8: AppxManifest Namespace Declarations Must Be Complete and Correct

**What goes wrong:**
The AppxManifest for a sparse package with context menu support requires multiple namespace declarations (`uap`, `uap10`, `rescap`, `desktop4` or `desktop6`). Missing or incorrect namespace URIs cause the manifest to be invalid. The package builds and registers without error, but the context menu extension is silently ignored by Explorer.

**Why it happens:**
Sparse package manifests are hand-authored XML, not generated by a wizard. Docs show different namespace sets for different feature combinations. Copy-pasting snippets from multiple sources results in namespace mismatches or missing declarations. The OS ignores extension elements from unknown or mismatched namespaces without logging a useful error.

**How to avoid:**
Use the official Microsoft sparse package manifest template from the docs verbatim as a starting point. Add only the namespaces you need. For context menu handler extensions, the required extension category is `windows.fileExplorerContextMenus` under `desktop4:Extension`. Validate the manifest with `MakeAppx.exe pack /nv` and review Windows Event Log (`Microsoft-Windows-AppxDeploymentServer`) for deployment errors.

**Warning signs:**
- Manifest builds and registers without errors, but context menu item never appears
- No errors in PowerShell, but Explorer shows no change after registration
- Event Viewer > Applications and Services > Microsoft > Windows > AppXDeployment shows warnings about ignored extensions

**Phase to address:** Phase: MSIX Package Construction (manifest validation)

---

## Technical Debt Patterns

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Hardcode `claude.exe` path | Simpler launch command | Breaks on any non-standard install; user bug reports | Never — always auto-detect |
| Hardcode `wt.exe` path with version number | Works on dev machine | Breaks on every Windows Terminal update | Never |
| Skip machine-wide install option | Simpler installer code | Per-user only install annoys IT admins and shared machines | MVP only, with documented limitation |
| Self-signed cert for distribution | Fast to implement | Blocks all end-user installs; installer looks malicious | Dev/testing only, never ship |
| Skip uninstall testing | Saves time during dev | Uninstaller leaves ghost MSIX registrations; re-installs fail | Never |
| Same MSIX version across releases | No version management | Upgrade installs fail on machines with old version registered | Never |

---

## Integration Gotchas

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| Windows Terminal (`wt.exe`) | Calling `wt.exe -d "<path>" cmd /k claude` without quoting paths with spaces | Always quote the directory path; paths with spaces in the username are extremely common |
| Claude Code (`claude.exe`) | Assuming `~\.local\bin\claude.exe` is always the path | Check `%LOCALAPPDATA%\..\local\bin\claude.exe`, `where claude`, and PATH; show an actionable error if not found |
| `Add-AppxPackage` in installer | Running without `-ExecutionPolicy Bypass` and `-NonInteractive` | Always include `-NoProfile -NonInteractive -WindowStyle Hidden -ExecutionPolicy Bypass` to prevent PowerShell execution policy from blocking the registration |
| MSIX signing with `signtool.exe` | Not including `/fd SHA256` flag | SHA256 is required; SHA1 signatures are rejected by modern Windows |
| Inno Setup `[Run]` section | Calling PowerShell without capturing exit code | Use `Flags: runhidden` and check return codes; a failed MSIX registration should not silently succeed to the user |

---

## Security Mistakes

| Mistake | Risk | Prevention |
|---------|------|------------|
| Shipping with a self-signed certificate embedded and prompting users to install it | Users trained to blindly install certs; opens phishing vector | Use a trusted CA cert; never ask users to install certs manually |
| Writing to HKLM without an explicit elevation prompt | Installer silently fails if not elevated, or unexpected UAC prompt | Be explicit: request elevation if HKLM writes are needed, or stay in HKCU scope |
| Command construction with unquoted paths in the context menu verb | Path injection if install directory contains special characters | Always quote all paths in registry verb strings; test with a path containing spaces and parentheses |
| Storing the `claude.exe` path in a predictable, world-writable location | Another process could replace the path, redirecting execution | Store paths in HKCU (not world-writable); the verb should resolve the executable at install time |

---

## UX Pitfalls

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Silent failure when Claude Code is not found | User clicks menu item, nothing happens, no feedback | Check for `claude.exe` at install time and warn; at runtime, show a message box with install instructions |
| Silent failure when Windows Terminal is not installed | Same — clicks menu, nothing happens | Check for `wt.exe` at install time; at runtime, show actionable error with Microsoft Store link |
| Context menu item appearing for files as well as folders | Confusing — Claude Code operates on directories | The AppxManifest verb should be scoped to `Directory` and `Directory\Background` only, not `*` |
| No icon on the context menu item | Menu item looks unpolished; hard to spot | Include a properly sized icon (16x16 and 32x32 PNG) referenced in the manifest; test icon visibility in both light and dark mode |
| Installer requiring admin when per-user would suffice | Friction for users without admin rights | Default to per-user install (no elevation); offer machine-wide as opt-in |

---

## "Looks Done But Isn't" Checklist

- [ ] **MSIX registration:** Context menu appears for the installing user — verify it also appears for a second Windows user account on the same machine
- [ ] **Uninstall:** Running the uninstaller then reinstalling works cleanly — `Get-AppxPackage` returns nothing after uninstall
- [ ] **Upgrade path:** Installing v1.0 then running the v1.1 installer completes without errors — the old MSIX version must be removed before registering the new one
- [ ] **Path with spaces:** The context menu works when the folder path contains spaces (e.g., `C:\Users\John Smith\Projects`) — test this explicitly
- [ ] **No Claude Code installed:** Clicking the menu item shows a clear error message rather than silently doing nothing
- [ ] **No Windows Terminal installed:** Same — actionable error shown
- [ ] **Code-signing:** The installer runs on a clean VM without any developer certificates installed — self-signed cert was not accidentally shipped
- [ ] **Icon visible:** The custom icon appears in both Windows light mode and dark mode, and does not appear as a broken image

---

## Recovery Strategies

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| Manifest/cert mismatch shipped to users | HIGH | Re-sign with correct cert, re-build MSIX, re-ship installer; users must uninstall and reinstall |
| Self-signed cert shipped publicly | HIGH | Obtain commercial cert, re-sign, re-ship; document cert change in release notes |
| Broken uninstall leaves ghost MSIX registration | MEDIUM | Publish a cleanup script (`Get-AppxPackage <name> \| Remove-AppxPackage`); include in troubleshooting docs |
| Hardcoded path breaks on Windows Terminal update | MEDIUM | Patch installer to use the MSIX app alias path; ship updated release |
| Per-user only install causes multi-user complaints | MEDIUM | Add machine-wide install option in next release; document workaround (each user runs installer) |
| Same MSIX version blocks upgrades | LOW | Bump manifest version, re-build MSIX, ship patched installer |

---

## Pitfall-to-Phase Mapping

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Manifest/cert identity mismatch | MSIX manifest authoring | `Get-AppxPackage <name>` succeeds; identity confirmed with `GetCurrentPackageFullName()` |
| Self-signed cert for distribution | Installer/distribution planning | Install succeeds on a clean VM with no dev certs |
| Per-user registration scope | Installer design | Context menu appears for a second user account on the same machine |
| ExternalLocation path mismatch | Installer implementation | `Get-AppxPackage | Select InstallLocation` matches actual install dir |
| Version re-registration failure | Installer implementation | Upgrade install (v1 → v2) completes cleanly on an already-installed machine |
| wt.exe wrong path | Core logic / path detection | Works on machines with both Terminal and Terminal Preview installed |
| Incomplete uninstaller | Installer implementation | Uninstall → reinstall round-trip succeeds; no ghost entries in `Get-AppxPackage` |
| Incomplete manifest namespaces | MSIX manifest authoring | Context menu appears immediately after first registration on a clean system |

---

## Sources

- [Microsoft Docs: Grant package identity by packaging with external location](https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/grant-identity-to-nonpackaged-apps) — official registration steps, manifest template, PowerShell commands
- [TMurgent Blog: MSIX with Context Menus with caveat](https://www.tmurgent.com/TmBlog/?p=3376) — Windows 11 vs Windows 10 silent failure behavior
- [TMurgent Blog: Notepad++ Context Menu Twists](https://www.tmurgent.com/TmBlog/?p=3643) — ExplorerCommandHandler vs. standard registration paths
- [Microsoft Q&A: Windows 11 Shell Menu using Sparse Package and multiple Users](https://learn.microsoft.com/en-us/answers/questions/1094326/windows-11-shell-menu-using-sparse-package-and-mul) — per-user scope limitation confirmed
- [Microsoft Q&A: Context menu shell extensions installed for "Just me" not showing](https://learn.microsoft.com/en-us/answers/questions/1685103/context-menu-shell-extensions-installed-on-windows) — HKCU vs. HKLM shell extension registration regression
- [Microsoft Q&A: Sparse Package registration succeeds but GetCurrentPackageFullName returns 15700](https://learn.microsoft.com/en-us/answers/questions/1163477/sparse-package-registration-succeeds-but-getcurren) — ExternalLocation path mismatch symptom
- [Nick's .NET Travels: Unpackaged Windows Apps with Identity using a Sparse Package](https://nicksnettravels.builttoroam.com/sparse-package/) — identity metadata matching requirements
- [Kenji Mouri: Implementing context menu support for File Explorer in Windows 11](https://mouri.moe/en/2021/12/25/Share-my-experience-of-implementing-context-menu-support-for-File-Explorer-in-Windows-11/) — IExplorerCommand platform immaturity, 16-item limit, Explorer reload requirement
- [Advanced Installer: MSIX Shell Context Menu Support](https://www.advancedinstaller.com/msix-shell-context-menu.html) — manifest namespace requirements
- [Microsoft Terminal GitHub: wt.exe path issues #11777](https://github.com/microsoft/terminal/issues/11777) — multiple wt.exe locations problem
- [Advanced Installer: MSIX Certificates for Developers](https://www.advancedinstaller.com/msix-certificates-developer.html) — certificate trust chain requirements for distribution

---
*Pitfalls research for: Windows 11 sparse MSIX shell extension (Claude From Here)*
*Researched: 2026-04-08*
