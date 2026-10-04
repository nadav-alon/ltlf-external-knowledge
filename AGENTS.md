# ltlf-external-knowledge

Instructions for agents working in this repo. The files below are the conventions this repo is written to; read the one that covers what you are about to do.

## Coding standards

Two files, both binding on an implementation run and on the review's standards axis:

- `docs/agents/coding-standards.md` — comments that outlive the review (`TODO[#n]`, never ticket narration).
- `docs/project-standards.md` — this repo's own: thin wrappers over Spot, glossary names, theory fidelity against `latex/main.tex`, the four oracle layers, the checks before committing, the stop rule.

## Grilling contract

What a grilling here must produce — frozen interfaces, a green checkpoint per composing ticket, the Stop-list, the `main.tex` ref, the ticket shape, the launch gate. See `docs/grilling-contract.md`.

## Issue tracker

Where this repo's issues live and how to drive them. See `docs/agents/issue-tracker.md`.

## Ticket scope

One seam per ticket: acceptance criteria describe behaviors of one seam, never a list of them. See `docs/agents/ticket-scope.md`.

## Triage labels

The five canonical triage roles, used verbatim as label strings. See `docs/agents/triage-labels.md`.

## Domain docs

Single-context: `CONTEXT.md` and `docs/adr/` at the repo root. See `docs/agents/domain.md`.

## Apply review

Commenting `/apply-review` on a draft pull request opens a ticket asking an agent to work every open review thread on it. See `.github/workflows/apply-review.yml`.

## Rebase

Commenting `/rebase` on an open pull request, draft or ready, opens a ticket asking an agent to rebase it — leaving its draft state as it was — and labels the pull request `needs-rebase`. See `.github/workflows/rebase.yml`, whose logic is `.github/workflows/scripts/rebase.sh`.
