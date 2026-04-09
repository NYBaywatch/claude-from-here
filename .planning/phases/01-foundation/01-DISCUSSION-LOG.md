# Phase 1: Foundation - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Decisions are captured in CONTEXT.md -- this log preserves the alternatives considered.

**Date:** 2026-04-09
**Phase:** 01-foundation
**Areas discussed:** Click behavior, Icon approach, Build toolchain, Dev registration flow

---

## Click Behavior in Phase 1

| Option | Description | Selected |
|--------|-------------|----------|
| Working launcher | Wire up the existing wt.exe command from the .reg file. Phase 1 proves end-to-end, Phase 2 adds error handling. | |
| Placeholder command | Open a basic cmd.exe or notepad with the path. Proves invocation without depending on claude.exe. | |
| No action | Menu item appears but clicking does nothing. Pure visual proof-of-concept. | |

**User's choice:** Working launcher (Recommended)
**Notes:** None

### Follow-up: Path handling

| Option | Description | Selected |
|--------|-------------|----------|
| Hardcode for now | Use known paths. Phase 2 owns path detection. | |
| Basic PATH lookup | Have the DLL do a simple PATH search for wt.exe and claude.exe. | |

**User's choice:** Other -- "hardcoded to look in the path, if its not seen in path option to update the path, or help user update"
**Notes:** Resolved as: DLL searches PATH for both executables. If not found, shows a basic Windows MessageBox with a clear error. Phase 2 replaces with polished error dialogs and auto-detection beyond PATH.

---

## Icon Approach

### Icon Source

| Option | Description | Selected |
|--------|-------------|----------|
| Custom .ico file | Create a proper multi-size .ico from the Claude/Anthropic logo. Ships with package. | |
| Extract from claude.exe | Use claude.exe's embedded icon at runtime. Risk: icon may not exist or wrong sizes. | |
| You decide | Claude picks the best approach. | |

**User's choice:** Custom .ico file (Recommended)
**Notes:** None

### Icon Design

| Option | Description | Selected |
|--------|-------------|----------|
| Claude logo mark | The recognizable Claude sparkle/asterisk symbol. | |
| Terminal + Claude hybrid | A terminal icon with Claude branding. | |
| You decide | Claude picks something appropriate. | |

**User's choice:** Claude logo mark
**Notes:** None

---

## Build Toolchain

### Compiler

| Option | Description | Selected |
|--------|-------------|----------|
| MSVC Build Tools | Visual Studio 2022 Build Tools (free). Better COM compatibility. | |
| MinGW-w64 | GCC for Windows. No Microsoft tooling. Less predictable for COM. | |
| You decide | Claude picks based on COM shell extension needs. | |

**User's choice:** MSVC Build Tools (Recommended)
**Notes:** None

### Build System

| Option | Description | Selected |
|--------|-------------|----------|
| CMake | Cross-platform, portable, version-control friendly. | |
| VS project files (.vcxproj) | Simpler for single-DLL project but less portable. | |
| You decide | Claude picks based on project needs. | |

**User's choice:** CMake (Recommended)
**Notes:** None

---

## Dev Registration Flow

### Script Approach

| Option | Description | Selected |
|--------|-------------|----------|
| PowerShell scripts | Separate register.ps1 and unregister.ps1 for cert, MSIX, and registration. | |
| Single build script | One script that compiles, packs, signs, and registers in one step. | |
| You decide | Claude designs the dev workflow. | |

**User's choice:** PowerShell scripts (Recommended)
**Notes:** None

### Admin Elevation

| Option | Description | Selected |
|--------|-------------|----------|
| Admin is fine for dev | Dev scripts always run elevated. Installer handles it for end users. | |
| Minimize admin usage | One-time admin for cert import, then unelevated for day-to-day. | |

**User's choice:** Minimize admin usage
**Notes:** One-time elevation for cert import to LocalMachine\TrustedPeople. Subsequent rebuilds and re-registrations run without admin.

---

## Claude's Discretion

- DLL project structure and COM boilerplate organization
- AppxManifest.xml namespace details and schema versions
- Exact CMakeLists.txt structure
- Script error handling details beyond the MessageBox approach

## Deferred Ideas

None -- discussion stayed within phase scope
