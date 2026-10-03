# ltlf-external-knowledge

Instructions for agents working in this repo. The files below are the conventions this repo is written to; read the one that covers what you are about to do.

## Coding standards

Comments that outlive the review (`TODO[#n]`, never ticket narration). See `docs/agents/coding-standards.md`.

Its "Brand your primitives" section is TypeScript-only and does not apply to this C++ repo. The C++ rules are the `/code-reviewer` skill (`.claude/skills/code-reviewer/`). Tracked upstream as nadav-alon/side-projects-manager#1212.

## Issue tracker

Where this repo's issues live and how to drive them. See `docs/agents/issue-tracker.md`.

## Ticket scope

One seam per ticket: acceptance criteria describe behaviors of one seam, never a list of them. See `docs/agents/ticket-scope.md`.

## Triage labels

The five canonical triage roles, used verbatim as label strings. See `docs/agents/triage-labels.md`.

## Domain docs

Single-context. `CONTEXT.md` at the repo root is a pointer to `docs/GLOSSARY.md`, which holds the vocabulary; there is no `docs/adr/`. See `docs/agents/domain.md`.

## Apply review

Commenting `/apply-review` on a draft pull request opens a ticket asking an agent to work every open review thread on it. See `.github/workflows/apply-review.yml`.

## Rebase

Commenting `/rebase` on an open pull request, draft or ready, opens a ticket asking an agent to rebase it — leaving its draft state as it was — and labels the pull request `needs-rebase`. See `.github/workflows/rebase.yml`, whose logic is `.github/workflows/scripts/rebase.sh`.
