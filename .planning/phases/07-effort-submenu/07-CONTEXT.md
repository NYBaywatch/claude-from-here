# Phase 7: Effort-Level Submenu - Context

**Gathered:** 2026-06-24
**Status:** Source complete (build-verified); runtime UAT deferred to release binary

<domain>
## Phase Boundary

Add the ability to launch Claude Code at a chosen reasoning **effort level** from the
context menu, without losing the existing one-click behavior.

`claude --help` confirms a real `--effort <level>` flag ("Effort level for the current
session") with values `low / medium / high / xhigh / max`. (`ultracode` and `fast` are
interactive slash-commands, not flags — deliberately excluded.)

Scope:
- Keep the existing one-click **"Claude from here"** verb (CLSID `b2dd8803-...`)
  unchanged — it launches with default effort (no `--effort` flag).
- Add a second top-level flyout verb **"Claude from here: effort"** (new CLSID
  `d6aefcec-...`) whose subitems are the five explicit levels, each launching
  `claude --effort <level>` plus the user's existing configured flags.

Out of scope: any Settings-app change (default = no flag honors the user's global
`CLAUDE_CODE_EFFORT_LEVEL`/settings.json); the free-text-field escaping debate (D-05/
D-06 from Phase 6 stand unchanged).
</domain>

<decisions>
## Implementation Decisions

### Menu architecture
- **D-01:** Two top-level entries, implemented as **two distinct CLSIDs/COM classes**
  sharing one DLL. A single IExplorerCommand cannot reliably present as both a
  direct-launch command and a flyout, and each `<desktop5:Verb>` maps to exactly one
  command. `CClaudeFromHere` (b2dd8803, unchanged) + `CClaudeEffortMenu` (d6aefcec, new).
- **D-02:** The effort flyout returns `ECF_HASSUBCOMMANDS` from `GetFlags` and a
  `CEnumEffortCommands` (`IEnumExplorerCommand`) from `EnumSubCommands`. Each subitem is
  a full `CClaudeEffortItem` (`IExplorerCommand`) returning `ECF_DEFAULT`. One nesting
  level only (Explorer does not support deeper nesting).

### Effort levels and default semantics
- **D-03:** Flyout offers exactly five levels — `low / medium / high / xhigh / max` —
  shown as Low / Medium / High / **Extra high** / Max. Tokens are fixed code constants
  in a static table (`kEffortLevels`), never user free text.
- **D-04:** "Default" is NOT a flyout item; it is the separate one-click
  "Claude from here" verb, which passes `effortOverride = nullptr` → emits **no**
  `--effort` flag, honoring the user's global effort configuration.

### Command line
- **D-05:** `--effort <level>` is appended immediately **after `--model`** (extends the
  Phase 6 D-08 ordering); `ExtraFlags` remains last as the user override.
- **D-06:** Defense-in-depth: `LaunchClaudeInDir` validates `effortOverride` against the
  whitelist (`IsValidEffort`) before appending. The flyout only ever passes table
  constants, but a non-whitelisted value is silently ignored — guarantees no new
  command-line-injection surface (contrast the free-text registry fields, Phase 6 F1).
- **D-07:** Backward compatibility: with `effortOverride = nullptr` and all Phase 5/6
  settings off, the assembled command line is byte-for-byte identical to pre-Phase-7
  output (preserves the Phase 6 D-13 "all-off" guarantee).

### Folder-background path resolution
- **D-08:** Path resolution (`ResolveFolderPath`) and launch (`LaunchClaudeInDir`) are
  extracted to anonymous-namespace free functions shared by all classes — no behavior
  change for the existing direct-launch path.
- **D-09:** For the `Directory\Background` case (null `IShellItemArray`), the flyout
  parent **propagates its `IObjectWithSite` site** into the enumerator → each subitem,
  and subitems also implement `IObjectWithSite` in case the shell sets the site on them
  directly. At `Invoke`, a subitem prefers the item array, else falls back to the stored
  site. Belt-and-suspenders, since the shell is not guaranteed to call `SetSite` on
  subcommands.

### Packaging
- **D-10:** `AppxManifest.xml` gains a second `<com:Class>` (same DLL) under the existing
  `<com:SurrogateServer>` and a second `<desktop5:Verb Id="ClaudeFromHereEffort">` under
  **both** `Directory` and `Directory\Background`. No installer / build-script change —
  same DLL + manifest are copied. Verified: MSIX packs cleanly with the new manifest.

### Claude's Discretion (exercised)
- Subitem titles ("Extra high" for xhigh); flyout label "Claude from here: effort".
- Subitems carry no icon (`GetIcon → E_NOTIMPL`); the parent flyout carries claude.ico.
- `CClassFactory` parameterized with a creator function pointer; `DllGetClassObject`
  dispatches on CLSID.

</decisions>

<canonical_refs>
## Canonical References

- `src/ClaudeFromHere.cpp` — shared helpers (`ResolveFolderPath`, `LaunchClaudeInDir`,
  `IsValidEffort`, `kEffortLevels`) + four classes: `CClaudeFromHere` (unchanged
  behavior), `CClaudeEffortItem`, `CEnumEffortCommands`, `CClaudeEffortMenu`.
- `src/dllmain.cpp` — `CLSID_ClaudeEffortMenu`, `CreateClaudeEffortMenuInstance`,
  CLSID-dispatch in `DllGetClassObject`, factory parameterized by creator fn.
- `package/AppxManifest.xml` — second `com:Class` + second `desktop5:Verb` per ItemType.
- Phase 6: `.planning/phases/06-dll-integration/06-CONTEXT.md` (D-08 flag ordering,
  D-13 all-off parity) which this phase extends.

</canonical_refs>

---

*Phase: 07-effort-submenu*
*Context gathered: 2026-06-24*
