# Phase 7: Effort-Level Submenu - Context

**Gathered:** 2026-06-24
**Status:** Source complete + build-verified + dev-registered; visual UAT pending

<domain>
## Phase Boundary

Add the ability to launch Claude Code at a chosen reasoning **effort level** from the
context menu.

`claude --help` confirms a real `--effort <level>` flag ("Effort level for the current
session") with values `low / medium / high / xhigh / max`. (`ultracode` and `fast` are
interactive slash-commands, not flags — deliberately excluded.)

Scope: the single existing "Claude from here" command (CLSID `b2dd8803-...`) becomes a
**flyout submenu** whose subitems are **Default** plus the five explicit levels.
- "Default" launches with no `--effort` (honors the user's global
  `CLAUDE_CODE_EFFORT_LEVEL`/settings.json) — byte-identical to pre-feature output.
- Each level launches `claude --effort <level>` plus the user's existing configured flags.

Out of scope: any Settings-app change; the free-text-field escaping debate (Phase 6
D-05/D-06 stand unchanged).
</domain>

<decisions>
## Implementation Decisions

### Menu architecture
- **D-01:** **Single verb, single CLSID**, presented as a flyout via
  `GetFlags → ECF_HASSUBCOMMANDS` + `EnumSubCommands`. **Rationale (learned during
  testing):** Windows 11 auto-groups *multiple* verbs from one package under a
  cascading entry headed by the package DisplayName and badged with the *package logo*
  (`StoreLogo.png`, a placeholder) — not our per-command icon — and pushes our
  subcommands one level too deep. A single flyout verb avoids the grouping entirely: it
  shows at top level with `claude.ico` and its levels exactly one submenu deep
  (guaranteed to render). An earlier two-CLSID attempt was reverted after the grouped
  "Claude From Here" entry appeared with a pink placeholder icon.
- **D-02:** Subitems are full `CClaudeMenuOption` (`IExplorerCommand`) instances
  returning `ECF_DEFAULT`, enumerated by `CEnumMenuOptions` (`IEnumExplorerCommand`).
  One nesting level only (Explorer does not support deeper nesting — another reason the
  flyout must be the top-level verb, not nested under a group).

### Effort levels and default semantics
- **D-03:** Flyout items, in order: **Default**, (separator), **Low / Medium / High /
  Extra high / Max** (xhigh shown as "Extra high"). Effort tokens are fixed code
  constants in a static table (`kMenuOptions`), never user free text. The "Default"
  row has `effort = nullptr`.
- **D-04:** "Default" passes `effortOverride = nullptr` → emits **no** `--effort`,
  honoring the user's global effort configuration. A separator (`ECF_SEPARATORBEFORE`
  on "Low") divides Default from the explicit levels.

### Command line
- **D-05:** `--effort <level>` is appended immediately **after `--model`** (extends the
  Phase 6 D-08 ordering); `ExtraFlags` remains last as the user override.
- **D-06:** Defense-in-depth: `LaunchClaudeInDir` validates `effortOverride` against the
  whitelist (`IsValidEffort`) before appending. Subitems only pass table constants, but
  a non-whitelisted value is silently ignored — guarantees no new command-line-injection
  surface (contrast the free-text registry fields, Phase 6 F1).
- **D-07:** Backward compatibility: with the Default item (nullptr) and all Phase 5/6
  settings off, the assembled command line is byte-for-byte identical to pre-Phase-7
  output (preserves the Phase 6 D-13 "all-off" guarantee).

### Folder-background path resolution
- **D-08:** Path resolution (`ResolveFolderPath`) and launch (`LaunchClaudeInDir`) are
  extracted to anonymous-namespace free functions shared by the flyout and its subitems.
- **D-09:** For the `Directory\Background` case (null `IShellItemArray`), the flyout
  propagates its `IObjectWithSite` site into the enumerator → each subitem, and subitems
  also implement `IObjectWithSite` in case the shell sets the site on them directly. At
  `Invoke`, a subitem prefers the item array, else falls back to the stored site.

### Packaging
- **D-10:** **No manifest change** — the single existing `<com:Class>` and
  `<desktop5:Verb>` are unchanged; the flyout is a pure runtime concern
  (`EnumSubCommands`). (The reverted two-CLSID attempt had required a second class +
  verb; dropping it returns the manifest to its original form.)

</decisions>

<canonical_refs>
## Canonical References

- `src/ClaudeFromHere.cpp` — shared helpers (`ResolveFolderPath`, `LaunchClaudeInDir`,
  `IsValidEffort`, `kMenuOptions`) + three classes: `CClaudeFromHere` (the flyout),
  `CClaudeMenuOption` (subitem), `CEnumMenuOptions` (enumerator).
- `src/dllmain.cpp` — unchanged single-CLSID factory + `DllGetClassObject`.
- `package/AppxManifest.xml` — unchanged single `com:Class` + single `desktop5:Verb`.
- Phase 6: `.planning/phases/06-dll-integration/06-CONTEXT.md` (D-08 flag ordering,
  D-13 all-off parity) which this phase extends.

</canonical_refs>

---

*Phase: 07-effort-submenu*
*Context gathered: 2026-06-24*
