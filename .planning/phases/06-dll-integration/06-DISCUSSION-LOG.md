# Phase 6: DLL Integration - Discussion Log

> **Audit trail only.** Do not use as input to planning, research, or execution agents.
> Downstream agents read `06-CONTEXT.md` instead. This file exists for human review.

**Gathered:** 2026-04-10
**Mode:** discuss (interactive, no advisor, no batch, no auto)

---

## Gray Areas Presented

Four gray areas identified from phase analysis and scout of `src/ClaudeFromHere.cpp`:

1. Channel parsing rules
2. Value quoting / shell safety
3. Flag ordering on command line
4. Verification approach

**User selection:** All four selected.

---

## Area 1: Channel parsing rules

### Q1.1: How should the DLL handle empty/whitespace entries when splitting the pipe-delimited Channels string?

Options presented:
- **Trim + skip empty (Recommended)** — Trim each entry, skip if empty after trimming. Handles trailing pipe, accidental `||`, and spaces gracefully.
- Skip empty only, no trim — Preserve whitespace, skip zero-length only.
- Pass through raw — Split and emit everything, even empty pieces.

**User selected:** Trim + skip empty (Recommended)

### Q1.2: Should there be a maximum channel count (buffer safety)?

Options presented:
- **Fixed cap, silently truncate (Recommended)** — Cap at ~16–32, extras ignored, no dynamic allocation.
- Grow buffer dynamically — Measure Channels length and allocate accordingly.
- No cap — enlarge fixed buffer — Just make szFlags large (e.g., 16KB).

**User selected:** Fixed cap, silently truncate (Recommended)
**Claude follow-up decision:** Cap chosen as 32; buffer sized to 16384 bytes to comfortably hold this.

### Q1.3: Should the DLL emit --channels followed by each entry separately, or use another format?

Options presented:
- **One --channels per entry (Recommended)** — Matches CHN-04 and roadmap success criterion #2.
- Single --channels with delimited list — Would require Claude Code to support CSV; not what roadmap specifies.

**User selected:** One --channels per entry (Recommended)

---

## Area 2: Value quoting / shell safety

### Q2.1: Should the DLL wrap RemoteControlPrefix in double-quotes on the command line?

Options presented:
- **No quoting — mirror ExtraFlags (Recommended)** — Raw append, consistent with Model/AllowedTools/ExtraFlags.
- Always quote the value — Safer against whitespace but inconsistent with other fields.
- Quote only if value contains whitespace — Conditional; adds branching.

**User selected:** No quoting — mirror ExtraFlags (Recommended)

### Q2.2: Should channel entries be quoted when emitted?

Options presented:
- **No quoting (Recommended)** — Channel strings are `plugin:name@source` format; no spaces expected.
- Always quote each channel value — Defensive but unnecessary.

**User selected:** No quoting (Recommended)

### Q2.3: Should the DLL validate/sanitize user input before appending?

Options presented:
- **No — trust Settings app (Recommended)** — Settings app already warns on metacharacters for ExtraFlags. Defense in UI, not DLL.
- Yes — silently strip dangerous chars — Belt-and-suspenders; may eat intended characters.
- Yes — show error and skip the flag — Most defensive but surfaces as silent omissions.

**User selected:** No — trust Settings app (Recommended)

---

## Area 3: Flag ordering on command line

### Q3.1: Where should the new flags sit relative to the existing ones?

Options presented:
- **Grouped by type, ExtraFlags last (Recommended)** — New flags inserted per logical grouping; ExtraFlags remains the final override slot.
- Append new flags after ExtraFlags — Smallest diff; loses ExtraFlags "last word" property.
- Strict Settings UI tab order — Matches General→Advanced→Channels; scatters booleans.

**User selected:** Grouped by type, ExtraFlags last (Recommended)

Ordering finalized in D-08:
1. `--model`, 2. `--verbose`, 3. `-c`, 4. `-r`, 5. `--dangerously-skip-permissions`, 6. `--allow-dangerously-skip-permissions`, 7. `--remote-control-session-name-prefix`, 8. `--allowedTools`, 9. `--channels` (repeated), 10. `ExtraFlags` (raw).

---

## Area 4: Verification approach

### Q4.1: How should we verify the DLL appends flags correctly?

Options presented:
- **Visual: `cmd /k claude` shows args (Recommended)** — Terminal window naturally shows the run command; no new code.
- Add debug log to %TEMP% — Always-on filesystem writes on every invoke.
- Gated debug log behind a reg key — Opt-in logging; more code.
- Both visual + temporary log during phase — Add log for Phase 6 only, then remove.

**User selected:** Visual: `cmd /k claude` shows args (Recommended)

### Q4.2: Should verification cover the "all off" case (success criterion #3 — command line identical to pre-v1.1.0)?

Options presented:
- **Yes, explicit check (Recommended)** — Protects against empty-value bugs (e.g., dangling flag for zero DWORD).
- Implied by unit of other tests — Skip explicit check; rely on code review.

**User selected:** Yes, explicit check (Recommended)

---

## Wrap-up

**User response to "Ready to create context?":** Create context.

No additional gray areas surfaced. No scope creep raised. No user-referenced ADRs or external specs introduced during discussion — canonical refs come from existing phase artifacts and source code.

---

*Phase: 06-dll-integration*
*Discussion logged: 2026-04-10*
