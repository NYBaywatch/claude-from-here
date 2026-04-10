# Phase 4: Distribution - Research

**Researched:** 2026-04-10
**Domain:** GitHub Actions CI/CD, GitHub Releases, repo public presentation, README authoring, E2E smoke testing on Windows runners
**Confidence:** HIGH

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

- **D-01:** Style and tone matches the Scanner project README (`D:\Working\Projects\scanner\README.md`). Dev-casual, feature-focused, concise.
- **D-02:** Sections: title + badges (download counter + stars), one-liner description, Why, screenshot + GIF, Install, Features, Usage, Troubleshooting, License.
- **D-03:** Badges include GitHub Downloads counter (all releases, flat style) and GitHub Stars — same format as Scanner.
- **D-04:** Both a static screenshot (context menu showing "Claude from here") and an animated GIF (right-click -> click -> Terminal opens with Claude) included in a `docs/` folder.
- **D-05:** Single artifact: `ClaudeFromHere-Setup.exe` attached to the GitHub Release. No source zip (GitHub auto-generates those).
- **D-06:** Release notes follow Scanner style: categorized changes with an "Install" line pointing to the download.
- **D-07:** First release tagged `v1.0.0`. Standard semver.
- **D-08:** License: MIT (same as Scanner). Include LICENSE file in repo root.
- **D-09:** Repo lives under `NYBaywatch` org on GitHub.
- **D-10:** Heavy cleanup before going public — remove: `.planning/`, `.claude/`, `GETTING-STARTED.md`, old `.reg` files, dev scripts (`scripts/`). Only ship what end users and contributors need: source code, build files, package manifest, assets, installer config, README, LICENSE.
- **D-11:** Repo name: `claude-from-here` (matches current project name).
- **D-12:** GitHub Actions on Windows runner, triggered on version tag push (e.g., `v*`).
- **D-13:** Pipeline stages: build C++ DLL (CMake + MSVC) -> build config app (.NET) -> sign MSIX (Azure Trusted Signing) -> package installer (Inno Setup) -> smoke test -> create GitHub Release with installer attached.
- **D-14:** DIST-03 (previously v2) pulled into Phase 4 scope.
- **D-15:** Full E2E smoke testing in CI on Windows runner: silent install, verify MSIX registered (`Get-AppxPackage`), verify files deployed to `%LOCALAPPDATA%\ClaudeFromHere\`, verify registry entries, attempt UI automation to confirm context menu appears in Explorer, capture screenshots as CI artifacts, uninstall and verify clean removal.
- **D-16:** UI automation for context menu verification is best-effort — if it's too fragile in CI, fall back to non-UI verification with a note that manual context menu testing is needed.

### Claude's Discretion

- GitHub Actions workflow YAML structure and job organization
- Screenshot/GIF capture tooling and approach
- README troubleshooting section content (common issues to document)
- Repo description and topic tags for GitHub
- .gitignore adjustments for public repo
- Whether to keep `build-installer.ps1` or fold its logic into CI

### Deferred Ideas (OUT OF SCOPE)

- Config app UI redesign: Dark-theme WPF app (from Phase 3 deferred). Post-v1.
- Config app content redesign: Replace model dropdown with practical CLI flags. Post-v1.
- Classic context menu toggle: Registry key checkbox in config app. Post-v1.
- ARM64 build (PLAT-01): Surface/Copilot+ PC support. v2.
- Auto-update mechanism: Overkill for v1.
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|------------------|
| DIST-01 | Project is available on GitHub with downloadable installer in Releases | GitHub Actions workflow with `softprops/action-gh-release@v2` uploads `ClaudeFromHere-Setup.exe` on `v*` tag push |
| DIST-02 | README documents installation, uninstallation, and troubleshooting | Scanner README structure and tone is the direct model; troubleshooting section covers the known failure modes from prior phases |
</phase_requirements>

---

## Summary

Phase 4 is a shipping and publication phase, not a feature-building phase. The work has three pillars: (1) clean up the repo for public consumption, (2) author a polished README and capture media assets, and (3) set up a GitHub Actions pipeline that builds, signs, packages, smoke-tests, and publishes the installer on every version tag push.

The technical foundation is already in place. `build-installer.ps1` contains the exact sequence the CI pipeline will replicate: CMake -> .NET build -> MakeAppx -> Azure Trusted Signing (`sign` dotnet tool) -> Inno Setup. The smoke test assertions are straightforward PowerShell using the same `Get-AppxPackage`, `%LOCALAPPDATA%\ClaudeFromHere\` path checks, and `Remove-AppxPackage` calls that the existing `scripts/register.ps1` and `scripts/unregister.ps1` already implement.

The repo cleanup is a one-time destructive operation that must be planned carefully. `.planning/` and `.claude/` directories contain the dev scaffolding, old `.reg` files are not needed by end users, and `scripts/` (the dev registration scripts) have no place in a public consumer repo. The `.gitignore` already excludes `build/` and `dist/`. A `docs/` folder must be created to hold the screenshot and GIF that the README references.

**Primary recommendation:** Translate `build-installer.ps1` directly into GitHub Actions steps (no abstraction layer needed), use `azure/artifact-signing-action@v1` (OIDC auth, no stored secrets), and `softprops/action-gh-release@v2` for release creation and asset upload. Keep `build-installer.ps1` in the repo for local dev — do not fold CI logic into it or delete it.

---

## Standard Stack

### Core

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| `actions/checkout` | v4 | Check out repo on CI runner | Standard first step for all GitHub Actions |
| `azure/artifact-signing-action` | v1 | Sign MSIX with Azure Trusted Signing | Official Microsoft action; replaces the `sign` dotnet tool call in build-installer.ps1; OIDC auth, no stored secrets |
| `softprops/action-gh-release` | v2 | Create GitHub Release and upload `ClaudeFromHere-Setup.exe` | De-facto standard for release+asset in one step; handles tag-triggered releases cleanly |
| `actions/upload-artifact` | v4 | Store CI build artifacts (installer, screenshots) between jobs | Standard CI artifact storage; v4 runs on Node.js 24 |
| `windows-latest` runner | Current (Server 2022) | Host the Windows-only build and smoke test | Only option for MSVC, MakeAppx, MSIX registration, Explorer restart |

### Supporting

| Tool | Version | Purpose | When to Use |
|------|---------|---------|-------------|
| `azure/login` | v2 | OIDC login to Azure before artifact-signing-action | Required when using Workload Identity Federation (no client secret) |
| `actions/setup-dotnet` | v4 | Pin .NET SDK version on runner | Use to ensure .NET 4.8 targeting pack is available for config app build |
| ScreenToGif | 2.41+ | Capture animated GIF of context menu interaction for docs | Manual, one-time, author's machine — free, records to GIF with frame editor |
| ShareX / Snipping Tool | Built-in | Static screenshot of context menu | Built-in Windows tools are sufficient for the static PNG |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `azure/artifact-signing-action@v1` | `sign` dotnet tool (as in build-installer.ps1) | Both work; the action wraps the tool and adds OIDC auth natively — less YAML boilerplate |
| `softprops/action-gh-release@v2` | `gh release create` (GitHub CLI) | gh CLI is fine but softprops handles draft/body/asset upload in one step |
| OIDC (Workload Identity Federation) | `AZURE_CLIENT_SECRET` stored as repo secret | OIDC is more secure — short-lived token, no rotation needed; preferred for new setups |

**Installation (CI — no local install needed):** GitHub Actions steps handle all tooling via `uses:` declarations. The only local dev tool not already installed is ScreenToGif for GIF capture.

---

## Architecture Patterns

### Recommended Workflow Structure

```
.github/
└── workflows/
    └── release.yml       # Single workflow: build -> sign -> smoke -> release
```

A single workflow file is sufficient. There is no need for separate build/release workflows given the project's small scope.

### Pattern 1: Tag-Triggered Release Workflow

**What:** The workflow triggers only on `v*` tag pushes, not on branch pushes or PRs.
**When to use:** Every public release of `ClaudeFromHere-Setup.exe`.

```yaml
# Source: github.com/softprops/action-gh-release
on:
  push:
    tags:
      - 'v*'

permissions:
  contents: write   # needed to create releases
  id-token: write   # needed for OIDC to Azure

jobs:
  release:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4

      # Step 1: Build C++ DLL via CMake + MSVC
      - name: Configure CMake
        run: cmake -S . -B build -G "Visual Studio 17 2022" -A x64

      - name: Build DLL
        run: cmake --build build --config Release

      - name: Copy DLL to build root
        run: |
          Copy-Item build\Release\ClaudeFromHere.dll build\ClaudeFromHere.dll
          Copy-Item build\Release\ClaudeFromHere.exe build\ClaudeFromHere.exe
        shell: pwsh

      # Step 2: Build config app (.NET 4.8)
      - uses: actions/setup-dotnet@v4
        with:
          dotnet-version: '8.x'   # SDK for build tooling; targets net48

      - name: Build config app
        run: dotnet build src/ClaudeFromHereConfig/ClaudeFromHereConfig.csproj --configuration Release --nologo

      # Step 3: Copy assets + pack MSIX
      - name: Copy assets and pack MSIX
        run: |
          Copy-Item assets\claude.ico build\claude.ico
          Copy-Item package\AppxManifest.xml build\AppxManifest.xml
          Copy-Item package\Assets build\Assets -Recurse
          & "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe" pack /o /d package /nv /p build\ClaudeFromHere.msix
        shell: pwsh

      # Step 4: Sign MSIX with Azure Trusted Signing (OIDC)
      - uses: azure/login@v2
        with:
          client-id: ${{ secrets.AZURE_CLIENT_ID }}
          tenant-id: ${{ secrets.AZURE_TENANT_ID }}
          subscription-id: ${{ secrets.AZURE_SUBSCRIPTION_ID }}

      - uses: azure/artifact-signing-action@v1
        with:
          endpoint: https://eus.codesigning.azure.net/
          signing-account-name: ${{ secrets.AZURE_SIGNING_ACCOUNT }}
          certificate-profile-name: ${{ secrets.AZURE_SIGNING_PROFILE }}
          files-folder: build
          files-folder-filter: msix
          file-digest: SHA256
          timestamp-rfc3161: http://timestamp.acs.microsoft.com
          timestamp-digest: SHA256

      # Step 5: Package installer with Inno Setup
      - name: Build installer
        run: |
          $iscc = if (Test-Path "C:\Program Files (x86)\Inno Setup 6\ISCC.exe") {
            "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
          } else {
            throw "Inno Setup not found. Add a setup step or pre-install on runner."
          }
          & $iscc installer\ClaudeFromHere.iss
        shell: pwsh

      # Step 6: Smoke test (silent install -> verify -> uninstall)
      - name: Smoke test installer
        run: |
          # Silent install
          Start-Process -Wait -FilePath "dist\ClaudeFromHere-Setup.exe" -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES"

          # Verify MSIX registered
          $pkg = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
          if (-not $pkg) { throw "MSIX package not registered after install" }
          Write-Host "Package registered: $($pkg.PackageFullName)"

          # Verify files deployed
          $installDir = "$env:LOCALAPPDATA\ClaudeFromHere"
          foreach ($f in @("ClaudeFromHere.dll", "ClaudeFromHere.exe", "ClaudeFromHere.msix", "claude.ico")) {
            if (-not (Test-Path "$installDir\$f")) { throw "Missing file: $installDir\$f" }
          }
          Write-Host "All expected files present in $installDir"

          # Silent uninstall
          $uninstaller = Get-ChildItem "$installDir" -Filter "unins*.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
          if ($uninstaller) {
            Start-Process -Wait -FilePath $uninstaller.FullName -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES"
          }

          # Verify MSIX unregistered
          $pkgAfter = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
          if ($pkgAfter) { throw "MSIX package still registered after uninstall" }
          Write-Host "Uninstall clean."
        shell: pwsh

      # Step 7: Upload installer artifact for debugging
      - uses: actions/upload-artifact@v4
        if: always()
        with:
          name: installer
          path: dist/ClaudeFromHere-Setup.exe

      # Step 8: Create GitHub Release with installer
      - uses: softprops/action-gh-release@v2
        with:
          files: dist/ClaudeFromHere-Setup.exe
          generate_release_notes: false
          body: |
            ## Install

            Download **ClaudeFromHere-Setup.exe** below and run it.

            Requires Windows 11.
```

### Pattern 2: Inno Setup on `windows-latest` Runner

The `windows-latest` runner (Windows Server 2022) does NOT have Inno Setup pre-installed. It must be installed as a step.

```yaml
- name: Install Inno Setup
  run: choco install innosetup --yes --no-progress
  shell: pwsh
```

Chocolatey is pre-installed on `windows-latest`. This is the standard approach — no need to pre-bake Inno Setup into the runner.

### Pattern 3: OIDC / Workload Identity Federation for Azure Trusted Signing

The Azure role names changed in 2025:
- Old: `Trusted Signing Identity Verifier` / `Trusted Signing Certificate Profile Signer`
- New: `Artifact Signing Identity Verifier` / `Artifact Signing Certificate Profile Signer`

The service principal must have `Artifact Signing Certificate Profile Signer` on the certificate profile resource. Configure the federated credential subject as:
```
repo:NYBaywatch/claude-from-here:ref:refs/tags/v*
```
This scopes OIDC trust to tag pushes only, not branch pushes.

### Pattern 4: Repo Cleanup (Pre-Public)

Files to remove before the repo goes public (D-10):
- `.planning/` — entire directory
- `.claude/` — entire directory
- `GETTING-STARTED.md`
- `claude-from-here.reg` and `claude-from-here-uninstall.reg`
- `scripts/` — entire directory (dev-only registration scripts)

Files to add:
- `LICENSE` (MIT)
- `docs/screenshot.png` (static context menu screenshot)
- `docs/demo.gif` (animated GIF of full interaction)

Files to keep and not modify:
- `build-installer.ps1` — valuable for contributors and local dev
- `CMakeLists.txt`, `src/`, `package/`, `assets/`, `installer/`

### Pattern 5: README Badge Format (from Scanner)

```markdown
[![GitHub Downloads (all assets, all releases)](https://img.shields.io/github/downloads/NYBaywatch/claude-from-here/total?style=flat&logo=github&label=Downloads)](https://github.com/NYBaywatch/claude-from-here/releases)
[![GitHub Stars](https://img.shields.io/github/stars/NYBaywatch/claude-from-here?style=flat&logo=github)](https://github.com/NYBaywatch/claude-from-here)
```

These are shields.io badges pulled at render time — no configuration required. They go live automatically once the repo and releases exist.

### Anti-Patterns to Avoid

- **Storing Azure client secrets as repo secrets:** Use OIDC workload identity federation instead. Secrets rotate and leak; OIDC tokens are ephemeral.
- **Using `actions/upload-release-asset` (deprecated):** Use `softprops/action-gh-release@v2` which combines release creation and asset upload.
- **Trusting `windows-latest` has MSVC pre-installed:** It does — Visual Studio 2022 Build Tools are included. CMake is also available at `C:\Program Files\CMake`. Do not add a separate MSVC setup step.
- **Blocking the release on UI automation failure:** D-16 is explicit — UI automation is best-effort. The smoke test must not fail the release because Explorer automation is fragile in headless CI.
- **Committing `.planning/` to the cleaned public repo:** The cleanup step is destructive. Plan it as a separate git operation before tagging v1.0.0, so history is clean.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Creating GitHub Releases with file uploads | Custom `gh` CLI scripting | `softprops/action-gh-release@v2` | Handles existing releases, draft mode, body, asset naming, error recovery |
| MSIX signing in CI | Custom PowerShell calling `sign` dotnet tool | `azure/artifact-signing-action@v1` | Official Microsoft action; integrates OIDC, timestamp, digest — fewer moving parts |
| Animated GIF capture | Custom screen recording script | ScreenToGif (manual, one-time) | Purpose-built, free, frame editor, direct GIF export |
| Inno Setup installation in CI | Bundling the installer binary | `choco install innosetup` | Chocolatey is pre-installed; handles version management |

**Key insight:** The CI pipeline is a translation of `build-installer.ps1` into YAML steps, not a new design. Every command already exists in the PowerShell script; the work is mapping them to `run:` steps.

---

## Common Pitfalls

### Pitfall 1: Inno Setup Not on Runner

**What goes wrong:** Workflow fails at the Inno Setup step with "ISCC.exe not found".
**Why it happens:** `windows-latest` does not pre-install Inno Setup.
**How to avoid:** Add `choco install innosetup --yes --no-progress` as a dedicated step before the installer build step.
**Warning signs:** Step output contains "not recognized" or "not found" for `ISCC.exe`.

### Pitfall 2: MakeAppx Path Hardcoded to SDK Version

**What goes wrong:** `makeappx.exe` path `C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\makeappx.exe` does not exist on the runner — Windows SDK on `windows-latest` may be a different version.
**Why it happens:** `windows-latest` ships a specific Windows SDK version that may not match the local dev machine.
**How to avoid:** Use `Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter makeappx.exe | Select-Object -First 1` to find MakeAppx dynamically, or use the Windows SDK setup action to pin a specific version.
**Warning signs:** Step fails with path not found on `makeappx.exe` or `signtool.exe`.

### Pitfall 3: OIDC Federated Credential Subject Mismatch

**What goes wrong:** Azure login fails with "AADSTS70021: No matching federated identity record found".
**Why it happens:** The federated credential subject filter must exactly match the OIDC claim. For tag-triggered workflows, the claim is `repo:NYBaywatch/claude-from-here:ref:refs/tags/v1.0.0`. If the credential was configured with `ref:refs/heads/main`, it will not match a tag push.
**How to avoid:** Set the federated credential subject to `repo:NYBaywatch/claude-from-here:ref:refs/tags/*` or use environment-based subject.
**Warning signs:** "401 Unauthorized" from the Azure login step.

### Pitfall 4: MSIX Smoke Test — Silent Install Requires Explorer Restart

**What goes wrong:** `Add-AppxPackage` (called by the Inno Setup installer) succeeds but the DLL is never loaded into Explorer because Explorer does not restart in a headless CI environment the same way as on a desktop.
**Why it happens:** The Inno Setup `[Run]` section handles Explorer restart in interactive mode. In VERYSILENT mode, the second Explorer restart step runs, but Explorer behavior in CI is unpredictable.
**How to avoid:** The smoke test verifies MSIX registration (`Get-AppxPackage`), file deployment, and uninstall — not whether the context menu visually appears. That is sufficient for CI. Note in test output that visual context menu verification requires manual testing.
**Warning signs:** False test pass (the MSIX is registered but the DLL never loaded — this is acceptable in CI context per D-16).

### Pitfall 5: Repo Cleanup Ordering

**What goes wrong:** `.planning/` and `.claude/` are removed in the same commit as the v1.0.0 tag, making the commit history look messy.
**Why it happens:** Cleanup and tagging are conflated.
**How to avoid:** Do cleanup in a dedicated commit (`chore: remove dev scaffolding for public release`), push that commit, then tag `v1.0.0` on the clean commit. This way the tag points to a clean tree.
**Warning signs:** Release notes show ".planning/ removed" as a change alongside v1.0.0 feature notes.

### Pitfall 6: Azure Trusted Signing Role Name Changes (2025)

**What goes wrong:** Signing fails with an authorization error despite having the old "Trusted Signing" roles assigned.
**Why it happens:** Microsoft renamed the Azure roles in 2025. Old role assignments on the old role names still work on existing resources but new setups require the new names.
**How to avoid:** Assign `Artifact Signing Certificate Profile Signer` (new name) to the service principal on the certificate profile resource.
**Warning signs:** HTTP 403 from the signing endpoint despite successful Azure login.

### Pitfall 7: Uninstaller Path in Smoke Test

**What goes wrong:** Smoke test cannot find the Inno Setup-generated uninstaller to call during CI teardown.
**Why it happens:** Inno Setup places the uninstaller at `%LOCALAPPDATA%\ClaudeFromHere\unins000.exe` by default, but the exact name (`unins000`, `unins001`, etc.) depends on previous install state.
**How to avoid:** Use `Get-ChildItem "$env:LOCALAPPDATA\ClaudeFromHere" -Filter "unins*.exe"` to find it dynamically. Alternatively, call `Remove-AppxPackage` directly and then delete the install directory manually in CI — no need to use the uninstaller in CI if the verification is package-level.
**Warning signs:** Smoke test passes install check but throws "file not found" on uninstall step.

---

## Code Examples

### Dynamic MakeAppx Discovery (CI-Safe)

```powershell
# Source: Verified pattern — handles any Windows SDK version on runner
$makeappx = Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin" -Recurse -Filter "makeappx.exe" |
    Where-Object { $_.FullName -match "x64" } |
    Sort-Object FullName -Descending |
    Select-Object -First 1 -ExpandProperty FullName

if (-not $makeappx) { throw "makeappx.exe not found in Windows Kits" }
Write-Host "Using MakeAppx: $makeappx"
```

### MSIX Registration Verification (Smoke Test Core)

```powershell
# Source: Adapted from scripts/register.ps1 — same assertion pattern
$pkg = Get-AppxPackage -Name "ClaudeFromHere" -ErrorAction SilentlyContinue
if (-not $pkg) { throw "MSIX not registered" }
Write-Host "OK: $($pkg.PackageFullName)"

$required = @("ClaudeFromHere.dll","ClaudeFromHere.exe","ClaudeFromHere.msix","claude.ico","AppxManifest.xml")
$dir = "$env:LOCALAPPDATA\ClaudeFromHere"
foreach ($f in $required) {
    if (-not (Test-Path "$dir\$f")) { throw "Missing: $dir\$f" }
}
Write-Host "OK: all files present"
```

### softprops/action-gh-release Minimal Config

```yaml
# Source: github.com/softprops/action-gh-release (v2 docs)
- uses: softprops/action-gh-release@v2
  with:
    files: dist/ClaudeFromHere-Setup.exe
    fail_on_unmatched_files: true
    generate_release_notes: false
```

The release title defaults to the tag name (`v1.0.0`). The `body:` field accepts multiline markdown for the release notes.

### README Install Section (Scanner-Style)

```markdown
## Install

Download the latest installer from [Releases](https://github.com/NYBaywatch/claude-from-here/releases):

**[ClaudeFromHere-Setup.exe](https://github.com/NYBaywatch/claude-from-here/releases/latest/download/ClaudeFromHere-Setup.exe)** — no admin required, installs per-user.

Requires Windows 11.
```

### Troubleshooting Section — Known Failure Modes

Based on prior phase learnings, the README troubleshooting section must cover:

1. **"Claude from here" doesn't appear after install** — Explorer was not restarted. Fix: Re-run the installer (it restarts Explorer) or manually restart Explorer via Task Manager.
2. **"Claude not found" error dialog** — Claude Code is not installed or not on the PATH. Fix: Install Claude Code from claude.ai and ensure `claude` is accessible in a new terminal.
3. **"Windows Terminal not found" error dialog** — `wt.exe` is not installed. Fix: Install Windows Terminal from the Microsoft Store.
4. **Install fails silently** — Running on Windows 10 (not supported). The extension requires Windows 11.
5. **Uninstall leaves the menu item** — Explorer needs to restart after uninstall. Fix: Log out and back in, or restart Explorer manually.

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `Trusted Signing Identity Verifier` role | `Artifact Signing Identity Verifier` role | 2025 | Existing assignments still work; new setups must use new name |
| `actions/upload-release-asset` | `softprops/action-gh-release@v2` | 2022-2023 | upload-release-asset is archived; softprops is the community standard |
| `azure/trusted-signing-action` | `azure/artifact-signing-action@v1` | 2025 (rename) | Service renamed from "Trusted Signing" to "Artifact Signing"; action renamed accordingly |

**Deprecated/outdated:**
- `actions/create-release` + `actions/upload-release-asset`: Both archived by GitHub. Use `softprops/action-gh-release@v2` exclusively.
- `MakeCert.exe`: Deprecated, not relevant here since Azure Trusted Signing is used for CI.

---

## Environment Availability

| Dependency | Required By | Available on `windows-latest` | Version | Fallback |
|------------|------------|-------------------------------|---------|----------|
| Visual Studio 2022 Build Tools (MSVC) | CMake C++ build | Yes (pre-installed) | 17.x | None needed |
| CMake | DLL build | Yes (pre-installed) | 3.x | None needed |
| .NET SDK | Config app build | Yes via `actions/setup-dotnet@v4` | Pinned | None needed |
| MakeAppx.exe | MSIX packing | Yes (Windows SDK ships on runner) | Varies — use dynamic discovery | Dynamic path search |
| Inno Setup (ISCC.exe) | Installer build | No — must be installed | 6.x | `choco install innosetup` |
| Chocolatey | Inno Setup install | Yes (pre-installed) | Current | None needed |
| PowerShell 5.1+ | All CI scripts | Yes | 5.1+ | None needed |
| Azure Artifact Signing | MSIX signing | Via action (cloud call) | Current | Skip signing only for dev builds |

**Missing dependencies with no fallback:**
- None that block execution — Inno Setup is the only gap and Chocolatey resolves it.

**Missing dependencies with fallback:**
- Inno Setup: Install via `choco install innosetup --yes --no-progress` as a workflow step.

---

## Open Questions

1. **Azure Trusted Signing — existing account vs. new setup**
   - What we know: `build-installer.ps1` uses env vars `CFH_SIGNING_ENDPOINT`, `CFH_SIGNING_ACCOUNT`, `CFH_SIGNING_PROFILE`, which implies an Azure Trusted Signing account already exists.
   - What's unclear: Whether Workload Identity Federation is already configured for the `NYBaywatch/claude-from-here` repo, or whether a federated credential needs to be created.
   - Recommendation: The planner should include a task to verify the federated credential subject matches the tag-push OIDC claim, and update it if it was previously scoped to a branch.

2. **MakeAppx SDK version on `windows-latest`**
   - What we know: `windows-latest` ships Windows SDK but the exact version varies as Microsoft updates the runner image.
   - What's unclear: Whether `10.0.26100.0` is always present or whether a different version is current.
   - Recommendation: Use dynamic path discovery (see Code Examples) rather than hardcoding the SDK version path.

3. **Screenshot / GIF capture — manual vs. CI**
   - What we know: D-04 requires a static screenshot and animated GIF in `docs/`.
   - What's unclear: Whether these are captured manually on the author's machine and committed, or generated in CI.
   - Recommendation: Capture manually using ScreenToGif on the author's machine. CI cannot reliably render the context menu visually (headless runner, no interactive desktop session). Commit `docs/screenshot.png` and `docs/demo.gif` directly.

---

## Sources

### Primary (HIGH confidence)

- [azure/artifact-signing-action README](https://github.com/Azure/trusted-signing-action/blob/main/README.md) — Full YAML workflow example, authentication inputs, OIDC setup
- [softprops/action-gh-release](https://github.com/softprops/action-gh-release) — `files:`, `body:`, `generate_release_notes` parameters, v2 usage
- [GitHub Docs: Configuring OpenID Connect in Azure](https://docs.github.com/actions/deployment/security-hardening-your-deployments/configuring-openid-connect-in-azure) — Federated credential subject format, `id-token: write` permission
- [Scott Hanselman: Automatically Signing with Azure Trusted Signing and GitHub Actions](https://www.hanselman.com/blog/automatically-signing-a-windows-exe-with-azure-trusted-signing-dotnet-sign-and-github-actions) — Real-world implementation reference, OIDC pattern
- `D:\Working\Projects\claude-from-here\build-installer.ps1` — Source of truth for build pipeline step sequence
- `D:\Working\Projects\claude-from-here\scripts\register.ps1` — Source of truth for smoke test assertion patterns
- `D:\Working\Projects\scanner\README.md` — Canonical style reference for README authoring

### Secondary (MEDIUM confidence)

- [Trusted Signing - DVLUP blog (2025-03-15)](https://dvlup.com/blog/2025.03.15-trustred-signing/) — Confirms 2025 role name changes
- [Code Signing With Azure Trusted Signing on GitHub Actions - Hendrik Erz](https://hendrik-erz.de/post/code-signing-with-azure-trusted-signing-on-github-actions) — Practical walkthrough confirming OIDC + artifact-signing-action pattern
- [Advanced Installer: GitHub Actions + Azure Signing](https://www.advancedinstaller.com/user-guide/qa-github-actions-azure-signing.html) — Confirms MSIX signing support in the action

### Tertiary (LOW confidence)

- WebSearch results on UI automation for Explorer context menus in CI — confirmed fragility; no authoritative pattern found. Supports D-16 best-effort stance.

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — official action docs verified, build-installer.ps1 is the ground truth
- Architecture: HIGH — direct translation of existing PowerShell script; no novel design needed
- Pitfalls: MEDIUM — runner environment details (SDK paths, Inno Setup) verified via WebSearch but not directly tested on `windows-latest`

**Research date:** 2026-04-10
**Valid until:** 2026-07-10 (stable domain; GitHub Actions action versions may update)
