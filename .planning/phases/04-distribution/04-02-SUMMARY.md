---
phase: 04-distribution
plan: 02
subsystem: infra
tags: [github-actions, ci-cd, windows, cmake, dotnet, msix, azure-trusted-signing, inno-setup]

# Dependency graph
requires:
  - phase: 03-installer
    provides: installer build pipeline (build-installer.ps1, ClaudeFromHere.iss, scripts/register.ps1)
provides:
  - GitHub Actions release workflow (.github/workflows/release.yml)
  - Automated build, sign, smoke test, and release on v* tag push
affects: [04-distribution-plan-03-readme, public release process]

# Tech tracking
tech-stack:
  added:
    - actions/checkout@v4
    - actions/setup-dotnet@v4
    - azure/login@v2
    - azure/artifact-signing-action@v1
    - softprops/action-gh-release@v2
    - actions/upload-artifact@v4
  patterns:
    - Dynamic MakeAppx discovery via Get-ChildItem (handles SDK version variation on windows-latest)
    - OIDC workload identity federation for Azure signing (no stored client secrets)
    - Smoke test pattern: silent install -> Get-AppxPackage -> file check -> dynamic uninstaller -> verify removal

key-files:
  created:
    - .github/workflows/release.yml
  modified: []

key-decisions:
  - "Dynamic MakeAppx discovery via Get-ChildItem to handle varying Windows SDK versions on windows-latest runner"
  - "OIDC workload identity (id-token: write) for Azure Trusted Signing avoids stored client secrets"
  - "Smoke test verifies MSIX registration + file deployment only; UI context menu verification skipped per D-16 (fragile in headless CI)"
  - "Inno Setup installed via Chocolatey per-run (not pre-installed on windows-latest)"
  - "actions/upload-artifact with if: always() ensures installer artifact available even on smoke test failure for debugging"

patterns-established:
  - "CI pipeline mirrors build-installer.ps1 sequence exactly: CMake -> dotnet -> MakeAppx -> sign -> ISCC -> smoke test"
  - "Federated credential subject must match refs/tags/v* (not refs/heads/main) for tag-triggered OIDC auth"

requirements-completed: [DIST-01]

# Metrics
duration: 1min
completed: 2026-04-10
---

# Phase 4 Plan 02: GitHub Actions Release Workflow Summary

**GitHub Actions workflow on windows-latest that builds C++ DLL + .NET config app, packs + signs MSIX via Azure Trusted Signing OIDC, packages with Inno Setup, smoke-tests install/uninstall, and creates GitHub Release with ClaudeFromHere-Setup.exe on v* tag push**

## Performance

- **Duration:** ~1 min
- **Started:** 2026-04-10T18:01:07Z
- **Completed:** 2026-04-10T18:02:06Z
- **Tasks:** 1
- **Files modified:** 1

## Accomplishments

- Created complete GitHub Actions release workflow (.github/workflows/release.yml)
- Full pipeline: CMake DLL build -> dotnet config app -> MSIX packing -> Azure signing -> Inno Setup -> smoke test -> GitHub Release
- Smoke test verifies MSIX registration (`Get-AppxPackage`), all required files in `%LOCALAPPDATA%\ClaudeFromHere\`, and clean uninstall
- Dynamic MakeAppx path discovery protects against Windows SDK version drift on runners

## Task Commits

1. **Task 1: Create GitHub Actions release workflow** - `b03a3c2` (feat)

**Plan metadata:** (docs commit below)

## Files Created/Modified

- `.github/workflows/release.yml` - Complete release pipeline triggered on v* tag push

## Decisions Made

- Used dynamic `Get-ChildItem` pattern for MakeAppx discovery rather than hardcoded SDK path — handles runner SDK version drift (Pitfall 2 from research)
- OIDC workload identity federation (`id-token: write` + `azure/login@v2`) for Azure Trusted Signing — no stored client secrets, more secure
- Smoke test does not assert visual context menu appearance per D-16 — verifies MSIX registration and file deployment instead
- `actions/upload-artifact@v4` with `if: always()` ensures installer is available as CI artifact even when smoke test fails

## Deviations from Plan

None - plan executed exactly as written.

## Issues Encountered

None.

## User Setup Required

The workflow requires these GitHub repository secrets before it can run:
- `AZURE_CLIENT_ID` — Service principal client ID for OIDC
- `AZURE_TENANT_ID` — Azure tenant ID
- `AZURE_SUBSCRIPTION_ID` — Azure subscription ID
- `AZURE_SIGNING_ACCOUNT` — Azure Artifact Signing account name
- `AZURE_SIGNING_PROFILE` — Azure certificate profile name

The Azure federated credential must be configured with subject: `repo:NYBaywatch/claude-from-here:ref:refs/tags/v*`

## Known Stubs

None — the workflow is complete. Secrets and Azure configuration are external setup items (documented above), not code stubs.

## Next Phase Readiness

- CI/CD pipeline complete — ready to tag v1.0.0 once repo cleanup and README (plan 03) are done
- Federated credential must be verified before first tag push (subject must match refs/tags/v*)

---
*Phase: 04-distribution*
*Completed: 2026-04-10*
