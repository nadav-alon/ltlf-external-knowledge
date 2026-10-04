# Grilling contract

What a grilling in this repo produces. A feature is a supertask and its sub-issues; no PRD file is
written. `docs/prd/` is history.

## The supertask

Its body carries what every ticket under it builds against:

- **The `main.tex` ref**: the section, `\cref` label or algorithm the feature implements.
- **Frozen interfaces**: each public signature — parameter order, return type, const-ness, header —
  naming every type by its glossary term. Each carries a **freeze confidence**: _high_ when the
  signature falls straight out of the glossary types, _tentative_ when the interface is being
  invented here.
- **The Stop-list**: each condition under which a run stops rather than guesses. Anything the
  grilling left open is on it. Each item is also written into the body of every ticket it applies to.

A mechanism not lifted from `main.tex` is grilled to the code: iteration bounds and their boundary
behavior, result type, what passes and what fails, empty and degenerate inputs, determinism. If a
run would have to execute it to learn how it should behave, the grilling is not finished.

## The tickets

All are flat sub-issues of the supertask, and every pull request targets `master`.

- **Seam tickets**: one seam each (`docs/agents/ticket-scope.md`).
- **A composing ticket per phase**, blocked by that phase's seam tickets. It carries the phase's
  **green checkpoint** as named `ctest` targets.
- The next phase's seam tickets are blocked by the previous composing ticket, so each phase lands
  green on `master` before the next starts.

A feature that fits one ticket has no phases and no composing ticket.

Experiment work — a sweep, a probe, a recon — is an ordinary ticket whose deliverable is a committed
report.

A ticket that needs a particular model carries a `model:` label. `turboable` is the developer's
call per ticket, made here.

## The launch gate

A ticket is labelled `ready-for-agent` only when all three hold. The grilling checks them as it
files the ticket:

1. Every domain identifier the ticket introduces is already in `docs/GLOSSARY.md`, spelled exactly.
2. Its checkpoint is evaluable: named `ctest` targets, or a named file a report lands in.
3. No Stop-list condition already holds.

A ticket that fails one is `needs-grilling`, saying which.
