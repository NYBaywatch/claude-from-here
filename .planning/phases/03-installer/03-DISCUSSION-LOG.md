# Phase 3: Installer - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-10
**Phase:** 03-installer
**Areas discussed:** Elevation strategy, Install location & files, Upgrade & uninstall behavior, Installer UX

---

## Elevation Strategy

| Option | Description | Selected |
|--------|-------------|----------|
| Full admin installer | Entire installer runs elevated. One UAC prompt. | |
| Elevate only for cert | Unelevated installer, elevated subprocess for cert | |
| Skip cert on non-admin | Fallback to AllowUnsigned if UAC declined | |

**Initial choice:** Full admin installer

**Key pivot:** User revealed they have Azure Trusted Signing already configured. This eliminates the need for self-signed cert import entirely, which was the only reason for admin elevation.

**Final decision:** No admin required at all. Per-user install with CA-signed MSIX.

### Cert Source (sub-question)

| Option | Description | Selected |
|--------|-------------|----------|
| Embed pre-built cert | Ship pre-generated .pfx in installer | |
| Generate per machine | Create unique cert via PowerShell at install time | |

**User's choice:** N/A — Azure Trusted Signing replaces both options. MSIX signed at build time with CA-backed cert.
**Notes:** User asked "is there a reasonable way around this?" regarding cert import. Answer: Azure Trusted Signing eliminates it entirely.

---

## Install Location & Files

| Option | Description | Selected |
|--------|-------------|----------|
| AppData\Local | Per-user, no admin, standard pattern | ✓ |
| User-chosen location | Let user pick in wizard | |

**User's choice:** AppData\Local
**Notes:** None

### Start Menu Shortcut

| Option | Description | Selected |
|--------|-------------|----------|
| Yes, Start Menu shortcut | Users find "Claude From Here Settings" in Start | ✓ |
| No shortcut | Launch from install folder only | |

**User's choice:** Yes, Start Menu shortcut
**Notes:** None

---

## Upgrade & Uninstall Behavior

### Upgrade

| Option | Description | Selected |
|--------|-------------|----------|
| Overwrite in place | Unregister old, deploy new, re-register | ✓ |
| Uninstall first | Force full uninstall before reinstall | |

**User's choice:** Overwrite in place
**Notes:** None

### Uninstall

| Option | Description | Selected |
|--------|-------------|----------|
| Everything except config | Remove files/MSIX/shortcuts, keep registry settings | ✓ |
| Full clean removal | Remove everything including registry config | |

**User's choice:** Everything except config
**Notes:** User settings survive uninstall/reinstall

---

## Installer UX

### Install Experience

| Option | Description | Selected |
|--------|-------------|----------|
| Minimal wizard | Welcome -> Install -> Done | |
| Standard wizard | Welcome -> License -> Location -> Install -> Done | |
| Silent install support | Minimal wizard + /SILENT /VERYSILENT flags | ✓ |

**User's choice:** Silent install support
**Notes:** None

### Post-Install

| Option | Description | Selected |
|--------|-------------|----------|
| Restart Explorer automatically | Shell extension loads immediately | |
| Ask user to restart Explorer | Show message, user does it manually | |
| Offer both options | Checkbox on final page | |

**User's choice:** Other — "tell the user must restart, let them click, then continue"
**Notes:** Final page informs user Explorer needs restart, button to restart, then wizard completes.

---

## Deferred Ideas

- Config app UI redesign to dark WPF theme (reference: Agrus Scanner)
- Config app content: practical CLI flags instead of model dropdown
- Classic context menu toggle as config app checkbox
