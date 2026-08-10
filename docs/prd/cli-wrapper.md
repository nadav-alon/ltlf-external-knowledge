# PRD: CLI wrapper (`ltlf-ek-synth`)

**Status:** implemented — branch `master`, uncommitted (`src/ltlf_ek_synth.cpp`, `src/cli.cpp`, `include/ltlf_ek/cli.hpp`)
**Interface:** new executable target `ltlf-ek-synth` (not a `Synthesis` method); wraps the `Synthesis` interface + `parse_transducer` + `VariablePartition`
**main.tex ref:** no dedicated algorithm — the CLI drives §Problem Definition (the $\mathcal{I}/\mathcal{O}/\mathcal{V}$ split, $\Tin,\Tout,T_C$) and Method 2 (`alg:dfa_product`) through `Synthesis::synthesize`

**Gates:**
- [x] glossary        — /glossary (2026-07-05): `sigma_slices`/`SigmaSlices` (the observed/produced slice in name-set form + its `(partition, role)` derivation) folded into docs/GLOSSARY.md "Observed / produced slice" + "Role" C++ columns; align-block §-refs corrected to §124–133 (glossary + `transducer_io.hpp` + `transducer-file-format.md`). `parse_partition_file`/`trivial_transducer`/`make_synthesis_method` confirmed plumbing (no entry).
- [x] tests           — `tests/cli_test.cpp` (24 cases: `parse_partition_file`,
  `trivial_transducer`, `make_synthesis_method`) + `tests/ltlf_ek_synth_test.cpp`
  (25 subprocess cases: golden HOA, verdict codes, exit-code matrix, known-
  transducer path, cross-check vs direct `DfaProduct::synthesize`, empty-O) +
  3 direct `sigma_slices` cases added to `tests/transducer_io_test.cpp`; 118
  tests total, `ctest` green (branch `master`, uncommitted)
- [x] code-review     — generic /code-review (high, 2026-07-05) clean modulo one optional cleanup (redundant `ap_registrar` in `src/ltlf_ek_synth.cpp`, `DfaProduct::synthesize` already registers I∪O); domain /code-reviewer (2026-07-05) clean after the glossary fix above; no broken invariants / Spot-idiom hazards.
- [x] theory-review   — theory-reviewer (2026-07-05, faithfulness mode) clean: `sigma_slices` orientation matches the align block (`latex/main.tex:127–137`) and `trivial_transducer`'s empty-Σ1 `consistent`-satisfaction (lambda=bddtrue = empty commitment) is faithful; no code-bug/doc-bug. One optional `underspecified` `\cl` note on the degenerate empty-known-set case surfaced, not applied.

## Goal
Give the project a command-line front end so a user can run a synthesis method
end-to-end without writing C++: parse a Goal formula $\varphi$, assemble the
four-way `VariablePartition`, materialise the two external-knowledge transducers
$\Tin,\Tout$, dispatch to a `Synthesis` method, and print the synthesized
controller $T_C$ — or just a realizability verdict. The tool is a thin
orchestration layer over existing library entry points (`parse_transducer`,
`VariablePartition`, `DfaProduct::synthesize`); it introduces **no** new
synthesis semantics. Today only Method 2 (`DfaProduct`) is wired; the other four
method flags are recognised but report "not yet implemented".

Invocation shape (chosen in the interview):

```
ltlf-ek-synth --dfa-product --formula="G(i -> o)" \
    [ --part-file P | --inputs i,... --outputs o,... ] \
    [ --known-input-transducer TIN ] [ --known-output-transducer TOUT ] \
    [ --model-check ] [ --realizable ]
```

## Ubiquitous-language terms used
All already in `docs/GLOSSARY.md` — no gaps:

- **Goal formula** $\varphi$ → `phi` (parsed with `spot::parse_formula`; finiteness
  is handled downstream by `ltlf_to_dfa`).
- **Inputs / Outputs** $\mathcal{I},\mathcal{O}$ and the four-way split
  $\Ifree,\Iknown,\Ofree,\Oknown$ → `VariablePartition` (+ `::split`, `::known`,
  `::inputs`, `::outputs`).
- **External knowledge strategy** → `Transducer` (`t_in`, `t_out`), materialised
  by `parse_transducer(in, partition, role, dict)` with `Role::t_in` / `Role::t_out`.
- **Controller** $T_C$ → `Controller` (holds `strategy`, a Spot Mealy `twa_graph`).
- **DFA product** (Method 2) → `DfaProduct` implementing `Synthesis::synthesize`.
- **Transducer file format (`%%LAMBDA` block)** → the on-disk form the
  transducer flags point at (see `docs/prd/transducer-file-format.md`).

New **infrastructure** identifiers this feature adds are CLI plumbing, **not**
domain concepts, so they do not need glossary entries (confirm with `/glossary`
if any is contested): the arg-parsing/orchestration in `ltlf-ek-synth`, a
part-file parser, and a trivial-transducer factory (see *Interfaces & types*).

## Behaviour / semantics (from main.tex)
The CLI must preserve the invariants the library already enforces; it adds only
input assembly and output formatting.

1. **One shared `bdd_dict`.** Create a single `spot::bdd_dict` and use it for
   *both* transducers (parsed or trivial). `DfaProduct::synthesize` reads the
   dict off `t_in.dict()` and requires `t_out.dict()` to match
   (`src/dfa_product.cpp:68`), and builds $\varphi$'s DFA on it
   (`ltlf_to_dfa(phi, dict)`). Cross-automaton `(v & guard)` tests are
   meaningless otherwise (glossary "Parse a transducer", `output_labeled_transducer.hpp:26`).

2. **Partition assembly.** Exactly one of two mutually exclusive sources:
   - `--part-file P` gives the full four-way split $\Ifree,\Iknown,\Ofree,\Oknown$
     directly (line-based format below). Used as-is to build `VariablePartition`.
   - `--inputs`/`--outputs` give $\mathcal{I},\mathcal{O}$ with $\mathcal{V}=\emptyset$
     (the empty-knowledge / monolithic case): call
     `VariablePartition::split(inputs, outputs, /*governed=*/{})` so everything is
     free. This is the shorthand path only.

3. **Transducer roles fix the slices.** `parse_transducer` orients $\lambda$ from
   `(partition, role)`: `Role::t_in` ⇒ $\Sigma_0=\Ifree,\Sigma_1=\Iknown$;
   `Role::t_out` ⇒ $\Sigma_0=\mathcal{I}\cup\Ofree,\Sigma_1=\Oknown$ (glossary
   "Role"). The CLI passes the role; the file never restates the slices.

4. **Empty known set ⇒ Trivial transducer.** When $\Iknown=\emptyset$ (resp.
   $\Oknown=\emptyset$) and no `--known-input-transducer` (resp.
   `--known-output-transducer`) is given, substitute the **Trivial** transducer:
   single state, $\delta$ self-loops, $\lambda$ commits the empty cube — so
   $\cons$ is trivially satisfied (mirrors `Trivial` in
   `tests/dfa_product_test.cpp:33`). It must be built on the shared dict.

5. **Dispatch.** Exactly one method flag selects the `Synthesis` implementation.
   `--dfa-product` → `DfaProduct`. The other four (`--nfa-product`,
   `--otf-dfa-product`, `--otf-agg-product`, `--otf-dyn-agg-product`) are
   recognised but exit with "method not yet implemented".

6. **Output.** `synthesize` returns `std::optional<Controller>`:
   - realizable + default mode → print `Controller::strategy` as Spot HOA
     (`print_hoa`) to **stdout**, exit 0. (This is the honest Mealy machine
     `solve_dfa` returns; it does **not** round-trip through `parse_transducer` —
     a `%%LAMBDA` writer is out of scope, see *Open questions*.)
   - realizable + `--realizable` → print `REALIZABLE` to stdout, exit 0.
   - unrealizable (`nullopt`) + default mode → print `UNREALIZABLE` to **stderr**,
     no HOA, exit 20.
   - unrealizable + `--realizable` → print `UNREALIZABLE` to stdout, exit 20.

## Interfaces & types
New **executable target** `ltlf-ek-synth` in `CMakeLists.txt`
(`add_executable(...)`, `target_link_libraries(... ltlf_ek)`). To keep the pure
logic unit-testable (not only drivable as a subprocess), split it:

- **Part-file parser** — `VariablePartition parse_partition_file(std::istream&)`
  (proposed home: `include/ltlf_ek/cli.hpp` / `src/cli/…`). Reads the line-based
  format, validates well-formedness (below), returns a `VariablePartition`.
- **Trivial-transducer factory** — a helper that builds the empty-output
  `OutputLabeledTransducer` on a given `dict` for a given `Role`/partition
  (so $\Sigma_0/\Sigma_1$ cubes match the role). This lifts the test's `Trivial`
  helper into reusable code; `/test-writer` should reuse it.
- **Method dispatch** — map a selected flag to a `std::unique_ptr<Synthesis>`
  (only `DfaProduct` constructible today; others → error).
- **`int main(...)`** — thin: parse argv, assemble partition + transducers on one
  dict, dispatch, format output, return the exit code.

Reused as-is (do **not** reimplement): `spot::parse_formula`, `collect_aps`
(`variables.hpp:34`), `VariablePartition::split`, `parse_transducer`,
`DfaProduct::synthesize`, `spot::print_hoa`.

**Part-file format** (line-based; no JSON/TOML dependency):

```
# comment; blank lines ignored
input_free:   a b
input_known:  c
output_free:  x
output_known: y
```

Four keys, one per set; value = space-separated AP names; a missing key or empty
value = empty set; `#` starts a comment.

## Edge cases
- **No method flag** / **more than one** method flag → usage error, exit 2.
- **Both** `--part-file` and `--inputs`/`--outputs` (or neither) → usage error,
  exit 2 (they are mutually exclusive; exactly one source required).
- **`--inputs`/`--outputs` overlap** ($\mathcal{I}\cap\mathcal{O}\neq\emptyset$) →
  error (violates the partition disjointness invariant).
- **Part-file malformed** — unknown key, an AP listed in two sets, non-disjoint
  sets → error, exit 2.
- **Known set non-empty but transducer flag missing** — $\Iknown\neq\emptyset$
  and no `--known-input-transducer` (resp. output) → error, exit 2.
- **Transducer flag given but its known set is empty** — e.g.
  `--known-input-transducer` with $\Iknown=\emptyset$ → error, exit 2 (ambiguous
  intent; use a part-file if a known input is intended).
- **Transducer file malformed** — `parse_transducer` throws
  `std::invalid_argument` (non-deterministic $\delta$, non-functional $\lambda$,
  AP outside $\Sigma_0\cup\Sigma_1$, bad `%%LAMBDA`); catch → stderr message,
  exit 2.
- **Formula parse error** — `spot::parse_formula` fails → stderr, exit 2.
- **Formula AP ∉ $\mathcal{I}\cup\mathcal{O}$** — already rejected inside
  `DfaProduct::synthesize` (`src/dfa_product.cpp:65`); surface its message, exit
  2 (usage-class, not internal).
- **Partition AP absent from $\varphi$ and from both transducers** — the AP may
  be unregistered on the shared dict; the CLI should register **every** $\mathcal{I}\cup\mathcal{O}$
  AP on the dict before `synthesize` so letters/vars are well-defined. Flag for
  `/developer` to verify against `DfaProduct`'s var lookup.
- **`--model-check`** — recognised but errors "model-check not yet implemented
  (see Verifier backlog item)", non-zero exit. Deferred to its own PRD.
- **Unrealizable** — not an error: exit 20, distinct from usage (2) and internal
  (1). See *Behaviour* §6.
- **Empty $\mathcal{O}$** (`--outputs` omitted / empty) — legal; the game has no
  system moves (cf. `tests/dfa_product_test.cpp:127` "1" realizable, "i" not).

## Test oracles (for /test-writer)
- **Part-file parser units** — round-trip a well-formed file → expected
  `VariablePartition`; each malformed case (unknown key, duplicate AP across
  sets, overlap) throws.
- **Trivial-transducer factory unit** — single state, $\delta$ self-loop,
  $\lambda$ = empty cube; $\cons$ trivially true; built on the passed dict with
  role-correct $\Sigma_0/\Sigma_1$.
- **End-to-end golden (realizable)** — run the built binary on `G(i -> o)`,
  empty-V shorthand; assert exit 0 and that stdout parses back as a Spot
  automaton (feed `print_hoa` output to `spot::automaton_stream_parser` /
  reload) — structural, not string-exact.
- **End-to-end verdict codes** — `--realizable` on a realizable formula → stdout
  `REALIZABLE`, exit 0; on `X[!] i` (unrealizable, `tests/dfa_product_test.cpp:113`)
  → `UNREALIZABLE`, exit 20; default-mode unrealizable → stderr `UNREALIZABLE`,
  exit 20.
- **Exit-code matrix** — bad/no/duplicate method flag, both partition sources,
  missing required transducer, malformed formula → all exit 2;
  `--nfa-product`/`--model-check` → their "not implemented" non-zero exits.
- **Cross-check vs library** — CLI verdict on a formula == `DfaProduct::synthesize(...).has_value()`
  invoked directly with the same inputs (the CLI adds no semantics). Reuse the
  monolithic-baseline oracle (`EmptyKnowledgeMatchesMonolithicBaseline`,
  `tests/dfa_product_test.cpp:160`) indirectly through the CLI.
- **Known-transducer path** — a fixture part-file with non-empty $\Iknown$ plus a
  small `%%LAMBDA` transducer file, asserting realizability matches the
  hand-computed expectation.

## Open theory questions touched
None new. The CLI only *drives* existing code, so it inherits — but does not
resolve — the already-tracked items (glossary "Open theory questions"): the
Method-2 arena input partition ($\Ifree$ vs full $\mathcal{I}$) deferred in
`docs/prd/dfa-product.md`, and `FP`/aggregation stubs (untouched — those methods
are not wired). No `\na` in `main.tex` is affected. `/theory-review` scope here
is only to confirm the CLI faithfully passes $\varphi,\mathcal{I},\mathcal{O},\mathcal{V},\Tin,\Tout$
through without altering the semantics.

## Definition of done
- `ltlf-ek-synth` builds as a new CMake target linking `ltlf_ek`.
- All flags behave per *Behaviour*: `--<method>` (5, one required), `--formula`,
  `--part-file` **xor** `--inputs`/`--outputs`, `--known-input-transducer`,
  `--known-output-transducer`, `--model-check` (deferred error), `--realizable`.
- Default prints the controller as Spot HOA; `--realizable` prints the verdict;
  exit codes 0 / 20 / 2 / 1 as specified.
- One shared `bdd_dict`; Trivial transducers substituted for empty known sets.
- Part-file parser + trivial-transducer factory are library-level and unit-tested;
  end-to-end golden + exit-code matrix pass (`/test-writer`).
- `README.md` gains a short "CLI" usage section.
- Glossary unchanged or updated via `/glossary` if any added identifier is judged
  a domain term (expected: none).
- The **Verifier** (for `--model-check`) is logged in `docs/BACKLOG.md` as a
  separate feature to `/grill-prd`.

## Developer comments / PRD disagreements

**2026-07-05:**

- **File layout differs from the "proposed home".** The PRD suggested
  `include/ltlf_ek/cli.hpp` / `src/cli/…` (the trailing `/…` reads as a
  directory). Implemented flat instead: `include/ltlf_ek/cli.hpp` +
  `src/cli.cpp` hold the three library-level, unit-testable pieces
  (`parse_partition_file`, `trivial_transducer`, `make_synthesis_method`), and
  `int main` lives in its own `src/ltlf_ek_synth.cpp` (argv parsing +
  orchestration only, not linked into `unit_tests`). This matches every other
  header/`.cpp` pair in the tree (`transducer_io.hpp`/`.cpp`, etc.) rather than
  introducing the project's first subdirectory under `src/`. `/test-writer`
  should add `tests/cli_test.cpp` for the three library-level functions and
  drive the built `ltlf-ek-synth` binary as a subprocess for the end-to-end
  oracles.
- **`transducer_io.hpp`/`.cpp` touched as a supporting refactor, not a
  behaviour change.** `trivial_transducer` needs the exact same
  role+partition -> (Sigma0, Sigma1) derivation `parse_transducer` already
  computes (the align block, main.tex §122-128). Rather than re-deriving it in
  `src/cli.cpp` (risking the two definitions drifting apart), the previously
  file-private `derive_slices`/`SliceNames` in `src/transducer_io.cpp` were
  promoted to a public `sigma_slices`/`SigmaSlices` in
  `include/ltlf_ek/transducer_io.hpp`, with identical logic — `parse_transducer`
  itself is behaviourally unchanged (all 22 existing `transducer_io_test.cpp`
  cases still pass). Flagging here since it touches an already-`implemented`
  PRD (`docs/prd/transducer-file-format.md`); no gate there needs reopening
  since no behaviour changed, only a private helper became a public one.
- **`--model-check` short-circuits before method dispatch.** When both
  `--model-check` and an unwired method flag (e.g. `--nfa-product`) are given,
  the CLI reports the model-check deferral, not "method not yet implemented" —
  an arbitrary but harmless priority the PRD left unspecified.
