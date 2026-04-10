# Phase 4: Distribution - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md — this log preserves the alternatives considered.

**Date:** 2026-04-10
**Phase:** 04-distribution
**Areas discussed:** README structure, Release packaging, Repo presentation, Validation scope

---

## README Structure

### Tone and Audience

| Option | Description | Selected |
|--------|-------------|----------|
| Dev-casual | Friendly, concise, assumes technical comfort. Like popular dev tool repos. | |
| Polished product | More formal, feature highlights, screenshots/GIFs, badges. | |
| Minimal/utilitarian | Bare minimum: what it does, how to install, how to uninstall. | |

**User's choice:** Copy the style and tone from Scanner project (`D:\Working\Projects\scanner\README.md`). Also add a download counter badge.
**Notes:** Scanner README is dev-casual with badges (Downloads + Stars), "Why" narrative section, screenshot, feature list, usage instructions.

### Visuals

| Option | Description | Selected |
|--------|-------------|----------|
| Screenshot | Static screenshot of the right-click menu | |
| GIF demo | Animated GIF showing the full flow | |
| Both | Screenshot for quick visual, GIF for full flow | ✓ |
| No visuals | Text-only README | |

**User's choice:** Both
**Notes:** None

### Sections

| Option | Description | Selected |
|--------|-------------|----------|
| Scanner-style | Why, Install, Features, Usage, Troubleshooting, License | ✓ |
| Lean | Install, Usage, Troubleshooting, License | |
| Comprehensive | All Scanner-style plus How It Works, Configuration, Building, Contributing | |

**User's choice:** Scanner-style (Recommended)
**Notes:** None

---

## Release Packaging

### Artifacts

| Option | Description | Selected |
|--------|-------------|----------|
| Installer only | Just ClaudeFromHere-Setup.exe | |
| Installer + source zip | .exe plus source archive | |
| Installer + loose files | .exe plus individual DLL, config app, MSIX | |

**User's choice:** "Like scanner" — single installer artifact only.
**Notes:** Scanner releases contain just the MSI installer. Same pattern here with the .exe.

### Version Tag

| Option | Description | Selected |
|--------|-------------|----------|
| v1.0.0 | Standard semver, first public release | ✓ |
| v0.1.0 | Signal early/beta | |
| v1.0 | Two-part version | |

**User's choice:** v1.0.0 (Recommended)
**Notes:** None

---

## Repo Presentation

### License

| Option | Description | Selected |
|--------|-------------|----------|
| MIT | Same as Scanner, permissive | ✓ |
| Apache 2.0 | Permissive with patent protection | |
| Unlicense | Public domain | |

**User's choice:** "Like scanner" — MIT License.
**Notes:** None

### Cleanup

| Option | Description | Selected |
|--------|-------------|----------|
| Remove .planning/ only | Strip GSD artifacts, keep everything else | |
| Minimal cleanup | Just .gitignore coverage | |
| Heavy cleanup | Remove .planning/, .claude/, GETTING-STARTED.md, old .reg files, dev scripts | ✓ |

**User's choice:** Heavy cleanup
**Notes:** None

### Repo Owner

| Option | Description | Selected |
|--------|-------------|----------|
| NYBaywatch | Same org as Scanner | ✓ |
| Personal account | Under personal GitHub username | |
| New org | Create a dedicated org | |

**User's choice:** NYBaywatch
**Notes:** None

---

## Validation Scope

### CI Scope Decision

| Option | Description | Selected |
|--------|-------------|----------|
| Pull DIST-03 into Phase 4 | Full CI/CD in this phase | ✓ |
| Phase 4 manual + Phase 5 CI | Ship manual first, automate later | |
| Phase 4 with basic CI only | Build+test in CI, manual release | |

**User's choice:** Pull DIST-03 into Phase 4
**Notes:** User wants "full corp style CI/CD, testing everything possible, VMs, the full monty."

### CI Platform

| Option | Description | Selected |
|--------|-------------|----------|
| GitHub Actions | Native to GitHub, free for public repos | ✓ |
| Azure DevOps | More enterprise-oriented | |

**User's choice:** GitHub Actions (Recommended)
**Notes:** None

### Triggers

| Option | Description | Selected |
|--------|-------------|----------|
| Tag push only | Pipeline on version tag push | ✓ |
| Tag push + PR checks | Tag for release, PRs for build+test | |
| Every push | Build+test on every push | |

**User's choice:** Tag push only (Recommended)
**Notes:** None

### VM Testing Level

| Option | Description | Selected |
|--------|-------------|----------|
| Install + verify | Silent install, verify MSIX/files/registry, uninstall | |
| Full E2E with screenshots | Above plus UI automation for context menu, screenshots | ✓ |
| Build verification only | Just verify build produces valid .exe | |

**User's choice:** Full E2E with screenshots
**Notes:** None

---

## Claude's Discretion

- GitHub Actions workflow YAML structure and job organization
- Screenshot/GIF capture tooling and approach
- README troubleshooting section content
- Repo description and topic tags
- .gitignore adjustments for public repo
- Whether to keep build-installer.ps1 or fold into CI

## Deferred Ideas

- Config app UI redesign (dark-theme WPF) — post-v1
- Config app content redesign (practical CLI flags) — post-v1
- Classic context menu toggle — post-v1
- ARM64 build — v2
- Auto-update mechanism — overkill for v1
