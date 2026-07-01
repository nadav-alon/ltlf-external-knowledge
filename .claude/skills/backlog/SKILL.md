---
name: backlog
description: Capture or update personal "what I intend to do next" items in docs/BACKLOG.md — a lightweight intention log, not the developer task tracker and not a grilling session. Use to add an item, move one between Now/Later/Done, or jot seeds for a future grill. Fast and low-friction; do NOT interview.
---

# Backlog capture

Maintain `docs/BACKLOG.md`, the user's personal intention log. This skill is
**fast and low-friction** — the whole point is capturing an intent *without*
starting a grill. Do the minimum, keep the format, get out of the way.

## Hard rules

- **Do NOT grill or interview.** No decision trees, no "one question at a time".
  If the user's phrasing is thin, capture it as-is; add an empty **Seeds:** line
  they can fill later rather than interrogating them.
- **Do NOT design or implement.** You are recording an intention, not resolving
  it. Resist expanding scope.
- Ask at most **one** brief clarifying question, and only if you genuinely can't
  tell what to record (e.g. which section). Otherwise just act.

## What to do

1. **Read `docs/BACKLOG.md`** to match its current structure (sections:
   `## Now / next`, `## Later`, `## Done`).
2. Apply the request:
   - **Add:** create an item under the requested section (default `## Now / next`)
     using the item format below.
   - **Move:** relocate an existing item between sections (e.g. → `## Done`),
     preserving its body; don't rewrite it.
   - **Amend:** append a note or a **Seeds** bullet to an existing item.
   - Remove the section's `_(nothing yet)_` placeholder when you add the first
     real item; restore it if a section becomes empty.
3. Keep it terse. Match the existing tone.

## Item format

```markdown
### <short title>
- **Intent:** <one or two sentences: what and why>
- **Seeds for grilling:** <optional half-formed questions/ideas; omit or leave a
  single "_(tbd)_" if none>
```

## Definition of done

- `docs/BACKLOG.md` updated; format and section placeholders consistent.
- No grilling happened, no scope was added beyond what the user said.
- One-line confirmation of what was captured/moved (don't recite the whole file).
