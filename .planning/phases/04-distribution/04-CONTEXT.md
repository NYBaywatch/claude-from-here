# Phase 4: Distribution - Context

**Gathered:** 2026-04-10
**Status:** Ready for planning

<domain>
## Phase Boundary

Ship the project publicly on GitHub under the NYBaywatch org with a polished README, MIT license, GitHub Actions CI/CD pipeline (triggered on tag push), and a published Release with the installer `.exe`. Includes full E2E smoke testing in CI (silent install, MSIX verification, UI automation for context menu, screenshot capture, uninstall verification). Repo is cleaned up for public consumption — no dev scaffolding or planning artifacts.

</domain>

<decisions>
## Implementation Decisions

### README
- **D-01:** Style and tone matches the Scanner project README (`D:\Working\Projects\scanner\README.md`). Dev-casual, feature-focused, concise.
- **D-02:** Sections: title + badges (download counter + stars), one-liner description, Why, screenshot + GIF, Install, Features, Usage, Troubleshooting, License.
- **D-03:** Badges include GitHub Downloads counter (all releases, flat style) and GitHub Stars — same format as Scanner.
- **D-04:** Both a static screenshot (context menu showing "Claude from here") and an animated GIF (right-click -> click -> Terminal opens with Claude) included in a `docs/` folder.

### Release Packaging
- **D-05:** Single artifact: `ClaudeFromHere-Setup.exe` attached to the GitHub Release. No source zip (GitHub auto-generates those).
- **D-06:** Release notes follow Scanner style: categorized changes with an "Install" line pointing to the download.
- **D-07:** First release tagged `v1.0.0`. Standard semver.

### Repo Presentation
- **D-08:** License: MIT (same as Scanner). Include LICENSE file in repo root.
- **D-09:** Repo lives under `NYBaywatch` org on GitHub.
- **D-10:** Heavy cleanup before going public — remove: `.planning/`, `.claude/`, `GETTING-STARTED.md`, old `.reg` files, dev scripts (`scripts/`). Only ship what end users and contributors need: source code, build files, package manifest, assets, installer config, README, LICENSE.
- **D-11:** Repo name: `claude-from-here` (matches current project name).

### CI/CD Pipeline
- **D-12:** GitHub Actions on Windows runner, triggered on version tag push (e.g., `v*`).
- **D-13:** Pipeline stages: build C++ DLL (CMake + MSVC) -> build config app (.NET) -> sign MSIX (Azure Trusted Signing) -> package installer (Inno Setup) -> smoke test -> create GitHub Release with installer attached.
- **D-14:** DIST-03 (previously v2) pulled into Phase 4 scope.

### Validation / E2E Testing
- **D-15:** Full E2E smoke testing in CI on Windows runner: silent install, verify MSIX registered (`Get-AppxPackage`), verify files deployed to `%LOCALAPPDATA%\ClaudeFromHere\`, verify registry entries, attempt UI automation to confirm context menu appears in Explorer, capture screenshots as CI artifacts, uninstall and verify clean removal.
- **D-16:** UI automation for context menu verification is best-effort — if it's too fragile in CI, fall back to non-UI verification with a note that manual context menu testing is needed.

### Claude's Discretion
- GitHub Actions workflow YAML structure and job organization
- Screenshot/GIF capture tooling and approach
- README troubleshooting section content (common issues to document)
- Repo description and topic tags for GitHub
- .gitignore adjustments for public repo
- Whether to keep `build-installer.ps1` or fold its logic into CI

</decisions>

<canonical_refs>
## Canonical References

**Downstream agents MUST read these before planning or implementing.**

### Style Reference
- `D:\Working\Projects\scanner\README.md` -- README style, tone, badge format, section structure to match
- Scanner GitHub Release format (NYBaywatch/AgrusScanner v0.2.2) -- Release notes style, single-artifact pattern

### Prior Phase Outputs
- `.planning/phases/01-foundation/01-CONTEXT.md` -- Foundation architecture decisions
- `.planning/phases/02-launcher-and-config/02-CONTEXT.md` -- Config app and launcher decisions
- `.planning/phases/03-installer/03-CONTEXT.md` -- Installer decisions (Azure Trusted Signing, no-admin, Inno Setup)

### Build Infrastructure
- `build-installer.ps1` -- Current build + sign + package script (may inform CI pipeline)
- `installer/ClaudeFromHere.iss` -- Inno Setup script
- `CMakeLists.txt` -- C++ build configuration
- `scripts/register.ps1` -- MSIX registration workflow (test verification reference)
- `scripts/unregister.ps1` -- Unregistration workflow (uninstall verification reference)

### Package Definition
- `package/AppxManifest.xml` -- MSIX manifest (verification target)

</canonical_refs>

<code_context>
## Existing Code Insights

### Reusable Assets
- `build-installer.ps1`: Full build pipeline (CMake, .NET build, MakeAppx, SignTool, Inno Setup) — can be adapted to GitHub Actions steps
- `scripts/register.ps1` / `scripts/unregister.ps1`: Registration/unregistration logic for test verification assertions
- `installer/ClaudeFromHere.iss`: Inno Setup script already supports `/SILENT` and `/VERYSILENT`

### Established Patterns
- Azure Trusted Signing already configured for MSIX signing — needs secrets in GitHub Actions
- `build/` and `dist/` directories already in `.gitignore`
- Inno Setup produces `dist/ClaudeFromHere-Setup.exe`

### Integration Points
- GitHub Actions needs Azure Trusted Signing credentials as repository secrets
- CI must replicate the `build-installer.ps1` workflow: CMake -> .NET build -> MakeAppx -> SignTool -> Inno Setup
- E2E tests verify the same MSIX registration flow that `register.ps1` performs

</code_context>

<specifics>
## Specific Ideas

- Match Scanner README exactly in style — badges, "Why" section, screenshot placement, Install section format
- Download counter badge is important — same shield.io format as Scanner
- Full corporate-style CI/CD pipeline with comprehensive automated testing
- UI automation in CI for context menu verification (best-effort, with fallback)
- Screenshots captured as CI artifacts for debugging test failures

</specifics>

<deferred>
## Deferred Ideas

- **Config app UI redesign**: Dark-theme WPF app (from Phase 3 deferred). Post-v1.
- **Config app content redesign**: Replace model dropdown with practical CLI flags. Post-v1.
- **Classic context menu toggle**: Registry key checkbox in config app. Post-v1.
- **ARM64 build** (PLAT-01): Surface/Copilot+ PC support. v2.
- **Auto-update mechanism**: Overkill for v1.

</deferred>

---

*Phase: 04-distribution*
*Context gathered: 2026-04-10*
