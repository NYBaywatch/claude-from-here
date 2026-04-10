---
status: resolved
trigger: "context menu not showing after phase 4 rebuild"
created: 2026-04-10T00:00:00Z
updated: 2026-04-10T00:00:00Z
---

## Current Focus

hypothesis: CONFIRMED — AppxManifest Publisher CN was changed in phase 3 (d4ed25d) to match Azure Trusted Signing. Local builds using build-installer.ps1 without Azure env vars produce an unsigned MSIX. The Inno Setup [Run] step calls Add-AppxPackage without -AllowUnsigned, so registration fails. The failure is hidden (runhidden, no exit code check), so install appears to succeed but context menu is never activated.
test: N/A — root cause confirmed via git log and code inspection
expecting: Fix: add self-signing capability to build-installer.ps1 for local builds, using the same CN as AppxManifest Publisher, and import cert to LocalMachine\TrustedPeople
next_action: implement fix

## Symptoms

expected: Right-click folder in Explorer shows "Claude from here" in the modern context menu
actual: Context menu item does not appear after installing the new build
errors: None reported (failure is silent — runhidden PowerShell, no exit code check)
reproduction: Install from dist/ClaudeFromHere-Setup.exe, right-click any folder
started: Worked in phase 3 via register.ps1 dev workflow; broken when using build-installer.ps1 for local builds after Publisher CN change

## Eliminated

- hypothesis: Phase 4 code changes broke build-relevant files
  evidence: git diff c7ed951..HEAD for all build-relevant files (CMakeLists.txt, src/**, AppxManifest.xml, installer/ClaudeFromHere.iss, build-installer.ps1, assets/**) shows zero diff
  timestamp: 2026-04-10

- hypothesis: .gitignore changes affected build
  evidence: .gitignore only added .planning/ and .claude/ exclusions — neither affects build inputs
  timestamp: 2026-04-10

## Evidence

- timestamp: 2026-04-10
  checked: git log -- package/AppxManifest.xml
  found: Publisher CN changed from "CN=ClaudeFromHere" to "CN=Joseph Fago, O=Joseph Fago, L=Newark, S=New Jersey, C=US" in phase 3 commit d4ed25d
  implication: Local self-signed cert (CN=ClaudeFromHere) no longer matches manifest Publisher

- timestamp: 2026-04-10
  checked: build-installer.ps1 Step 5 (signing)
  found: When Azure env vars are not set, MSIX is left unsigned; no self-signing fallback
  implication: Local builds produce unsigned MSIX

- timestamp: 2026-04-10
  checked: installer/ClaudeFromHere.iss [Run] section
  found: Add-AppxPackage called without -AllowUnsigned; runs with runhidden flag; Inno Setup does not check PowerShell exit code
  implication: Registration failure is completely silent — user sees successful install but MSIX is never registered

- timestamp: 2026-04-10
  checked: register.ps1 (dev workflow)
  found: Dev script generates self-signed cert with CN=ClaudeFromHere, imports to LocalMachine\TrustedPeople, signs with that cert — this workflow still works
  implication: The break is specific to the build-installer.ps1 + Inno Setup production path

## Resolution

root_cause: AppxManifest Publisher CN was updated in phase 3 to match Azure Trusted Signing subject ("CN=Joseph Fago, O=Joseph Fago, L=Newark, S=New Jersey, C=US"). build-installer.ps1 has no self-signing fallback for local dev builds, so without Azure env vars the MSIX is unsigned. The installer calls Add-AppxPackage without -AllowUnsigned, and the error is silently swallowed (runhidden + no exit code check), so the shell extension is never registered.
fix: |
  build-installer.ps1 Step 5: When Azure env vars absent, create self-signed cert with CN matching
  AppxManifest Publisher ("CN=Joseph Fago, O=Joseph Fago, L=Newark, S=New Jersey, C=US"), sign MSIX
  with it, export .cer to build/, import PFX to LocalMachine\TrustedPeople (UAC if non-admin).
  
  installer/ClaudeFromHere.iss: Add ClaudeFromHere-dev.cer to [Files] with skipifsourcedoesntexist
  (no-op for Azure-signed CI builds). Add [Run] step before Add-AppxPackage to import .cer via
  certutil -addstore (admin elevation via Start-Process -Verb RunAs).
  
  .gitignore: Add *.cer and *cert-password.txt patterns to prevent dev cert artifacts from being committed.
verification:
files_changed: [build-installer.ps1, installer/ClaudeFromHere.iss, .gitignore]
