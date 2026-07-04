# LTLf Synthesis with External Information

C++ implementation of the synthesis methods described in `main.tex`: given an
$\text{LTL}_f$ Goal $\varphi$ over inputs $\mathcal{I}$ and outputs $\mathcal{O}$
and two external-knowledge transducers $T_{in}, T_{out}$, synthesize a
controller $T_C$ such that every trace agreeing with $T_{in}, T_{out}, T_C$
satisfies $\varphi$.

Built on [Spot](https://spot.lre.epita.fr/) (thin domain wrappers), with custom
types for the non-standard lambda-split transducers and NFAs.

## Methods

1. NFA product (`NfaProduct`) — §nfa
2. **DFA product (`DfaProduct`) — §fulldfa  *(implemented first)***
3. On-the-fly: no aggregation / aggregated / dynamic aggregation — §otf

All share `Synthesis::synthesize(phi, vars, t_in, t_out)`.

## Build

Requires a C++20 compiler, CMake ≥ 3.16, and Spot (`libspot` on the
`pkg-config` path). GoogleTest is fetched automatically.

```sh
cmake -S . -B build
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CLI

`ltlf-ek-synth` is a thin command-line front end over `Synthesis::synthesize`
(docs/prd/cli-wrapper.md) — no new synthesis semantics, just argument assembly
and output formatting. Today only `--dfa-product` is wired; the other four
method flags are recognised but report "not yet implemented".

```sh
build/ltlf-ek-synth --dfa-product --formula="G(i -> o)" \
    --inputs i --outputs o
# realizable -> the controller as Spot HOA on stdout, exit 0

build/ltlf-ek-synth --dfa-product --formula="G(i -> o)" \
    --inputs i --outputs o --realizable
# realizable -> "REALIZABLE" on stdout, exit 0; unrealizable -> "UNREALIZABLE", exit 20
```

The variable partition comes from exactly one of two mutually exclusive
sources: `--inputs`/`--outputs` (comma-separated, $\mathcal{V}=\emptyset$
shorthand) or `--part-file` (the full four-way $\Ifree,\Iknown,\Ofree,\Oknown$
split, line-based format):

```
# comment; blank lines ignored
input_free:   a b
input_known:  c
output_free:  x
output_known: y
```

A non-empty known set requires the matching transducer file
(`--known-input-transducer`, `--known-output-transducer`, `%%LAMBDA` format —
see docs/prd/transducer-file-format.md); an empty known set is filled in with
the Trivial transducer automatically. Exit codes: `0` realizable, `20`
unrealizable, `2` usage error, `1` internal / not-yet-implemented.

## Layout

```
include/ltlf_ek/   public headers (domain types + Synthesis interface)
src/               implementations
tests/             GoogleTest unit tests
docs/GLOSSARY.md   ubiquitous language: math ↔ prose ↔ C++
latex/             the theory (git submodule → Overleaf); main.tex is the
                   source of reference, not word of god — see
                   docs/overleaf-sync.md for the two-way sync workflow
```

## Working in this repo (skills)

Project-scoped Claude Code skills encode the workflow and the ubiquitous
language. Trigger them with a slash:

| Skill | Phase |
|---|---|
| `/grill-prd` | interview → PRD (in ubiquitous language) for a feature |
| `/developer` | implement a method/feature against the PRD + glossary |
| `/test-writer` | unit tests (+ metamorphic / verifier oracles) |
| `/code-reviewer` | Spot/BDD + glossary + domain-invariant review (spawns theory review on semantic diffs) |
| `/theory-review` | code↔math faithfulness &/or LaTeX soundness; may edit `main.tex` under `\cl` notes |
| `/glossary` | maintain the 3-column glossary |
| `/backlog` | capture personal "what to do next" items in `docs/BACKLOG.md` (no grilling) |

See `.claude/skills/*/SKILL.md`.
