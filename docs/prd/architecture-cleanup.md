# PRD: Architecture cleanup (post-accretion refactor)

**Status:** draft
**Interface:** cross-cutting refactor — no new `Synthesis` method; adds `LetterAlphabet` + `VariablePartition::universe()` (public), `detail/util.hpp` (internal), `role.hpp` (relocation), `tests/support/fixtures.hpp` (test-only); relocates `controller_as_transducer` and `trivial_transducer`
**main.tex ref:** no dedicated algorithm — this PRD moves code, it does not change semantics. The invariants preserved are the alphabet $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$ (§Transducers), the align-block slices (§124–133), $\cons$ = `\cref{def:consistency}` (with its §211 partiality note), `\cref{def:enabled}`, and the `\cref{def:probDefTransducer}` postcondition — all exactly as already implemented.

**Gates:**
- [x] glossary        — /glossary (2026-07-08): *Closed universe of APs* (`VariablePartition::universe()`) and *Letter alphabet* (`LetterAlphabet`, `all_letters` retired to its "Do not call it" line) landed in docs/GLOSSARY.md ahead of implementation (deliberate — the glossary drives /developer naming); existing *Product* / *Role* / *Observed-produced slice* / *Controller-as-transducer view* entries checked, no file references go stale under this PRD's relocations. Branch `master`, uncommitted.
- [ ] tests           — unit + oracle coverage
- [ ] code-review     — domain (/code-reviewer) + generic (/code-review)
- [ ] theory-review   — code ↔ math faithfulness vs main.tex

## Goal

Repay the structural drift accumulated over the PRD-by-PRD history (see
`docs/prd/transducer-product.md`, `docs/prd/controller-verifier.md`,
`docs/prd/cli-wrapper.md` — all stand; this PRD supersedes none of them).
An architectural review (2026-07-08) found three accretion patterns:
**half-finished migrations** (`verify_controller` still hand-rolls the
validation that `validate_product_inputs` was extracted to share, and shadows
`ProductState` with a local tuple), **utilities re-spelled per file**
(`cube_of` ×4, `trim` ×3, the `inputs() ∪ outputs()` universe ×4), and
**module placement drift** (`Role`/`sigma_slices` stranded in the HOA-parsing
header, `controller_as_transducer` in the verifier header,
`trivial_transducer` in the CLI header). It also found a comment-contract
(`build_product` letter indices + Ifree-first `io_vars` ordering + the
verifier's `idx & ifree_mask`) that should become a type before Methods 3.x
add a third product consumer. This PRD fixes all of it, **behavior-preserving
throughout**, sliced so three worktrees can implement it concurrently.

## Ubiquitous-language terms used

Existing glossary terms (identifiers unchanged, only files move): *Transducer*,
*Output-labeled transducer*, *Controller*, *Controller-as-transducer view*
(`controller_as_transducer`), *Role* / *Observed / produced slice*
(`sigma_slices`, `SigmaSlices`), *Product* (`ProductState`,
`agreeing_successor`, `build_product`), *Consistency* (`consistent`), *Output
agreement* (`emits`), *Letter*, *Cube*, *Goal DFA construction*
(`ltlf_to_dfa`), *Game solving* (`solve_dfa`), *Controller verifier*
(`verify_controller`, `VerifyResult`, `Witness`), the `VariablePartition`
family.

**Glossary gaps — run `/glossary` before or with Phase 0:**
- **Closed universe of APs** — $\mathcal{I}\cup\mathcal{O}$ as the set every
  AP (of $\varphi$, of a transducer file, of a lambda formula) must lie in.
  The error messages already speak this term; the new
  `VariablePartition::universe()` makes it a C++ identifier.
- **Letter alphabet** — the enumerated full-letter alphabet
  $\Sigma=2^{\mathcal{I}\cup\mathcal{O}}$ with its Ifree-first index
  structure; new C++ type `LetterAlphabet`.

`cube_of` / `trim` are BuDDy/string infrastructure, not domain concepts — no
glossary entries (same policy as `Transducer::dict()`).

## Behaviour / semantics (from main.tex)

**This PRD changes no observable semantics.** The binding invariants are:

1. **Verdict preservation.** For every `(phi, vars, t_in, t_out)`:
   `DfaProduct::synthesize` returns a controller iff it did before, and
   `verify_controller` returns the same `ok` verdict. The whole existing test
   suite passes in every phase/worktree.
2. **Controller validity, not controller identity.** BDD-variable
   registration order may shift (see Edge cases), so a *different but
   equivalent* controller or witness is acceptable — the arbiter is the
   metamorphic round-trip (`synthesize` → `verify_controller` accepts) and
   the differential oracle vs `ltlfsynt`, not byte-identical HOA.
3. **The product filter stays $\cons$-shaped** (`\cref{def:consistency}`,
   partiality per `\cref{def:enabled}`): `LetterAlphabet` only re-packages the
   letter enumeration; `agreeing_successor` / `emits` logic is untouched.
4. **The verifier stays independent of `solve_dfa`/`solve_game`**
   (`docs/prd/controller-verifier.md`): migrating it onto
   `validate_product_inputs` and the shared `ProductState` must not introduce
   any game-solving reuse.
5. **Comment anchors survive.** The comment sweep rewrites *tense*, never
   deletes a `main.tex` `\cref`/§ anchor or glossary pointer.

## Interfaces & types

### New: `LetterAlphabet` (in `include/ltlf_ek/product.hpp`)

Replaces the cross-module comment-contract (letter indices + caller-owned
`io_vars` ordering + `idx & ifree_mask`). Owning class; the Ifree-first
ordering becomes its unforgeable invariant:

```cpp
class LetterAlphabet {
 public:
  // Registers every AP of `vars` on `registrar` (idempotent for APs already
  // on the dict), in fixed block order: input_free, input_known, output_free,
  // output_known — each block in std::set (lexicographic) order.  The
  // Ifree-first ordering is THIS class's invariant, not the caller's.
  LetterAlphabet(const VariablePartition& vars,
                 const spot::twa_graph_ptr& registrar);

  const std::vector<bdd>& letters() const;   // all 2^|I∪O| full letters,
                                             // LSB-first in registration order
  std::size_t size() const;                  // letters().size()
  std::size_t n_ifree_combos() const;        // 2^|Ifree|
  std::size_t ifree_index(std::size_t idx) const;  // idx & (n_ifree_combos()-1)
};
```

Pinned behaviour (no run-to-find-out):
- `size() == 1 << |I∪O|`; **empty universe** ⇒ `letters() == {bddtrue}`,
  `size() == 1` (preserves today's `all_letters({})`).
- **Empty $\Ifree$** ⇒ `n_ifree_combos() == 1`, `ifree_index(idx) == 0` for
  all `idx`.
- `ifree_index` precondition: `idx < size()`; plain mask, no bounds check
  (assert allowed).
- Fully deterministic: `std::set` iteration fixes the order; no seeds, no
  dict-state dependence beyond `register_ap` idempotence.
- `build_product` changes signature to take `const LetterAlphabet&` in place
  of `const std::vector<bdd>&`; `ProductNode::edges` keeps storing `size_t`
  indices, now into `alphabet.letters()`.
- The free function `all_letters` **leaves the public header** (becomes a
  file-local helper inside `product.cpp` or a private member); its unit tests
  in `tests/product_test.cpp` migrate to `LetterAlphabet::letters()`.

### New: `VariablePartition::universe()` (in `variables.hpp`)

`std::set<std::string> universe() const` — $\mathcal{I}\cup\mathcal{O}$, the
closed universe of APs. Adopted at the four hand-built sites
(`product.cpp` `validate_product_inputs`, `dfa_product.cpp`,
`verify_controller.cpp`, `transducer_io.cpp`).

### New: `include/ltlf_ek/detail/util.hpp` (internal, header-only)

- `bdd cube_of(const std::set<std::string>& names, const spot::twa_graph_ptr& aut)`
  — the positive-literal variable cube, registering each name (idempotent).
  Absorbs the copies in `solve_dfa.cpp` and `transducer_io.cpp` and the
  inlined loops in `cli.cpp` (`trivial_transducer`) and
  `verify_controller.cpp` (`controller_as_transducer`).
- `std::string trim(const std::string&)` — absorbs the copies in
  `transducer_io.cpp` and `cli.cpp`; `SplitCsv` in `ltlf_ek_synth.cpp` adopts
  it for its inline trimming.
- `namespace ltlf_ek::detail`; not a domain API, tests may include it but the
  glossary does not know it.

### Relocations (declarations move, canonical names do not)

- `Role`, `SigmaSlices`, `sigma_slices` → new `include/ltlf_ek/role.hpp`
  (+ `src/role.cpp` for `sigma_slices`); `transducer_io.hpp` includes
  `role.hpp` so no consumer breaks transitively. Consumers that include
  `transducer_io.hpp` *only* for these (`verify_controller.cpp`, `cli.cpp`)
  retarget to `role.hpp`.
- `controller_as_transducer` declaration → `synthesis.hpp` (next to its
  subject, `Controller`); implementation moves to new `src/synthesis.cpp`.
- `trivial_transducer` declaration → `output_labeled_transducer.hpp`
  (it is a general transducer factory, not CLI plumbing); implementation to
  `output_labeled_transducer.cpp`. `cli.hpp` keeps only genuine CLI plumbing
  (`parse_partition_file`, `make_synthesis_method`).

### `verify_controller.cpp` migration finish

- Replace the hand-rolled universe/dict validation (`verify_controller.cpp`
  lines 167–180) with `validate_product_inputs(phi, vars, {&t_in, &t_out,
  &t_c})` — the helper already takes N transducers.
- **Drop the local `using ProductState = std::tuple<…>` entirely**: key
  `StateInfo` (and `compute_bad` / `extract_witness`) on the shared
  `ltlf_ek::ProductState` (it has `operator<` / `operator==`). The reshaping
  in `build_verifier_graph`, the shadowing comments, and the spelled-out
  tuple at the call site all disappear. The Bad $\nu$-fixpoint, virtual-start
  split, and witness tie-break (least Ifree combo, via
  `LetterAlphabet::ifree_index`) are behaviorally untouched.

### Small nits (ride along)

- `ltlf_ek_synth.cpp` `main()`: hoist the duplicated
  `make_synthesis_method` try/catch (lines 297–303 and 329–335) into one
  helper or one early construction.
- `Controller` becomes a `struct` (one public member, no invariants); name
  and glossary entry unchanged. Its stale skeleton comment ("lambda_C output
  map is filled in as methods are implemented") is rewritten to point at
  `controller_as_transducer` as the lambda-materialisation.
- `VerifyResult::ok` stays; add one comment stating the redundancy with
  `counterexample.has_value()` is deliberate (explicit verdict field).

### Comment tense sweep

Rule: a comment states what **is** and why (keeping every `main.tex` /
glossary anchor), never what changed relative to a former revision. Known
instances to rewrite (not exhaustive — sweep each owned file):
`consistency.hpp` ("consistent is **now** exactly … no behaviour change"),
`product.hpp`/`product.cpp` ("Shared spelling of both call sites' **old**
dfa_delta", "fires exactly as DfaProduct's dfa_delta does **today**"),
`verify_controller.cpp` ("reproduces the **pre-migration** tie-break"),
`synthesis.hpp` (stale skeleton note, see above), `cli.hpp` ("Mirrors the
`Trivial` test helper" — stale once fixtures dedup lands).

### New: `tests/support/fixtures.hpp` (test-only, header-only)

Policy: **prefer the library factory over a fixture duplicate** — the
`Trivial` helpers in `tests/dfa_product_test.cpp` / `tests/product_test.cpp`
become calls to `ltlf_ek::trivial_transducer` where the roles fit. The header
holds only what the library does not provide: `Phi(s)` (parse a formula),
the shared partitions (e.g. `IoFreeVars()`), `TinAlwaysI(dict)`, and the
constant-output `Role::t_c` builder from `verify_controller_test.cpp`.
No `ltlfsynt_oracle_test.cpp` split in this PRD — that waits for the
soak-mode backlog item, which will reshape the file anyway.

## Implementation phases

Designed for **concurrency**: Phase 0 lands on `master` first; worktrees A, B,
C then fork from it and run **simultaneously** with disjoint file territories.
A and B merge in either order; C rebases and merges **last** (near-zero
overlap by construction). A short sequential post-merge pass closes the loop.
Every phase/worktree leaves the tree compiling with the full suite green.

- **Phase 0 — shared foundations (sequential, on `master`, small):**
  `VariablePartition::universe()` (+ unit test in `variables_test.cpp`),
  `detail/util.hpp` (`cube_of`, `trim`), `role.hpp`/`role.cpp` extraction with
  `transducer_io.hpp` re-including `role.hpp` (no call sites retargeted yet —
  everything compiles transitively). CMake: add `src/role.cpp`.
  **Checkpoint:** builds; full suite passes unmodified.

- **Worktree A — product core.** *Owns:* `product.{hpp,cpp}`,
  `dfa_product.{hpp,cpp}`, `synthesis.hpp` (+ new `src/synthesis.cpp`),
  `verify_controller.{hpp,cpp}`, `solve_dfa.hpp`, `CMakeLists.txt`,
  `tests/{product,dfa_product,verify_controller}_test.cpp`. *Does:*
  `LetterAlphabet` + `build_product` signature change + both consumers
  adopting it; `verify_controller` onto `validate_product_inputs` + shared
  `ProductState` (de-shadowing); `controller_as_transducer` →
  `synthesis.hpp`/`src/synthesis.cpp`; `Controller` → struct;
  `VerifyResult::ok` comment; `universe()`/`cube_of` adoption and the comment
  sweep **within owned files**; migrate owned tests (including
  `all_letters` → `LetterAlphabet` test migration + new `LetterAlphabet`
  unit tests). **Checkpoint:** builds; full suite green; new alphabet tests
  green.

- **Worktree B — I/O + CLI.** *Owns:* `transducer_io.{hpp,cpp}`,
  `cli.{hpp,cpp}`, `ltlf_ek_synth.cpp`, `solve_dfa.cpp`,
  `output_labeled_transducer.{hpp,cpp}`,
  `tests/{transducer_io,cli,ltlf_ek_synth,output_labeled_transducer}_test.cpp`.
  *Does:* adopt `detail/util.hpp` (`trim`, `cube_of`) and `universe()` in
  owned files; retarget `role.hpp` includes; `trivial_transducer` →
  `output_labeled_transducer.{hpp,cpp}`; `main()` try/catch dedup; comment
  sweep within owned files; fix owned tests' includes. (Note:
  `ltlf_ek_synth.cpp` already includes both `synthesis.hpp` and
  `verify_controller.hpp`, so A's `controller_as_transducer` move does not
  break B in either merge order.) **Checkpoint:** builds; full suite green.

- **Worktree C — tests + residual comments (merges last).** *Owns:*
  `tests/support/fixtures.hpp` (new), `tests/{variables,consistency,ltlf_to_dfa}_test.cpp`,
  and the comment sweep in library files A/B do **not** own:
  `consistency.{hpp,cpp}`, `transducer.hpp`, `ltlf_to_dfa.{hpp,cpp}`,
  `variables.{hpp,cpp}`. *Does:* create the fixtures header, migrate its
  owned test files onto it, sweep its owned comments. **Checkpoint:** builds;
  full suite green; rebases cleanly on merged A+B.

- **Post-merge micro-pass (sequential, small):** A/B-owned test files adopt
  `tests/support/fixtures.hpp` (replacing their local `Phi`/`Trivial`/
  `TinAlwaysI` duplicates); grep-checks from Definition of done run clean.
  **Checkpoint:** full suite green on `master`.

## Edge cases

- **BDD variable numbering may shift.** `LetterAlphabet` registers APs in a
  fixed Ifree-first block order; previously `DfaProduct` registered the
  sorted universe and the verifier registered Ifree/Iknown/Ofree/Oknown
  blocks. `register_ap` is idempotent, so numbering only changes for APs not
  already on the dict at construction time (e.g. an AP appearing only in
  $\varphi$ in a library-direct call). Consequences: letter enumeration order
  — hence solver/witness tie-breaks — may legitimately change, yielding a
  *different but equivalent* controller or witness. Any test asserting exact
  HOA text or an exact witness must be checked: if it breaks, re-verify
  semantically (round-trip / verdict) and re-pin the golden value; do **not**
  contort `LetterAlphabet` to reproduce the old order. (The CLI flow is
  unaffected: `ap_registrar` in `main()` registers the whole partition before
  anything else, fixing the numbering exactly as today.)
- **Empty universe / empty $\Ifree$:** pinned in the `LetterAlphabet` spec
  above (single `bddtrue` letter; `ifree_index ≡ 0`).
- **Partial transducers / unrealizable / empty $\Ofree$:** untouched paths;
  the existing suites covering them are the regression net.
- **Merge-order hazards:** only C touches files A and B also compile against
  (none it *edits*); the one deliberate overlap — `CMakeLists.txt` (Phase 0
  adds `role.cpp`, A adds `synthesis.cpp`) — is sequenced (Phase 0 before
  fork; only A edits it in flight).
- **`detail/util.hpp` double-adoption race:** A and B both *use* it but
  neither edits it after Phase 0 — no conflict.

## Test oracles (for /test-writer)

1. **The existing suite is the primary oracle** — every phase/worktree ends
   with the full GoogleTest suite green; behavior preservation is the claim
   under test.
2. **`LetterAlphabet` unit tests** (Worktree A): `size()==2^|I∪O|`;
   `letters()` are pairwise-disjoint full letters covering `bddtrue`
   (OR of all == `bddtrue`); LSB-first ordering (letter 0 is the all-negative
   cube); `n_ifree_combos()`/`ifree_index` against a hand-computed 2-var
   example; empty-universe and empty-$\Ifree$ cases; idempotent registration
   (constructing on a dict with APs pre-registered yields identical letters).
3. **`universe()` unit test** (Phase 0): equals `inputs() ∪ outputs()` on a
   4-way partition.
4. **Metamorphic / differential nets stay green unmodified:** the generated
   corpus (round-trip `synthesize` → `verify_controller` accepts;
   differential vs `ltlfsynt`) and the faithfulness guard in
   `tests/ltlfsynt_oracle_test.cpp` run untouched in every worktree — they
   are the semantic arbiter if a golden value shifts (Edge cases).
5. **Relocation checks:** compile-level (headers self-sufficient: a TU
   including only `role.hpp` / only `synthesis.hpp` compiles) — covered by
   the moved declarations' existing tests re-pointed at the new includes.
6. **Grep gates** (post-merge micro-pass, can be a script or manual):
   exactly one definition each of `cube_of`, `trim`; no
   `inputs() ∪ outputs()` hand-builds outside `universe()`; no
   `using ProductState = std::tuple` anywhere; no `all_letters` in public
   headers.

## Open theory questions touched

None — this PRD is semantics-preserving. The four tracked `main.tex` open
questions (FP stub, aggregated final-state overwrite, on-the-fly solving,
trace-termination `\na` §96) are unaffected; the termination reading shared
by `solve_dfa` and the verifier is preserved by invariant #4. Nothing here
for `/theory-review` beyond confirming the migration did not alter the
$\cons$/`\cref{def:enabled}` filter or the `\cref{def:probDefTransducer}`
check.

## Definition of done

- Phase 0 + worktrees A, B, C merged to `master` (C last) + post-merge
  micro-pass done; full suite green at every merge point.
- No duplicate `cube_of`/`trim`/universe spellings; no `ProductState`
  shadowing; `all_letters` gone from public headers (grep gates, oracle #6).
- `Role`/`sigma_slices` live in `role.hpp`; `controller_as_transducer` in
  `synthesis.hpp`; `trivial_transducer` in `output_labeled_transducer.hpp`;
  `verify_controller` calls `validate_product_inputs`.
- Comment sweep applied: no history-relative comments in swept files; all
  `main.tex`/glossary anchors intact.
- `tests/support/fixtures.hpp` exists and all test files use it (or the
  library factories) instead of local duplicates.
- `/glossary` run: *Closed universe of APs* (`VariablePartition::universe()`)
  and *Letter alphabet* (`LetterAlphabet`) entries landed; *Product* /
  *Observed / produced slice* / *Controller-as-transducer view* C++ columns
  checked for stale file references.
- Gates ticked by their owning skills as they run.

## Developer comments / PRD disagreements

- **2026-07-08 — Phase 0 landed.** `VariablePartition::universe()`
  (`include/ltlf_ek/variables.hpp` + `src/variables.cpp`) with its unit test
  (`tests/variables_test.cpp`, `UniverseIsInputsUnionOutputs`, 4-way
  partition); `include/ltlf_ek/detail/util.hpp` (`ltlf_ek::detail::cube_of`,
  `ltlf_ek::detail::trim`, header-only, lifted verbatim from
  `src/solve_dfa.cpp` / `src/cli.cpp` — no call sites retargeted, per phase
  scope); `Role`/`SigmaSlices`/`sigma_slices` extracted to new
  `include/ltlf_ek/role.hpp` + `src/role.cpp`, with `transducer_io.hpp` now
  `#include`ing `role.hpp` so its consumers keep compiling transitively (the
  duplicate `sigma_slices` definition was removed from `transducer_io.cpp`
  since the symbol now lives in `role.cpp` — required to avoid a link-time
  duplicate-definition error, not a call-site retarget); `CMakeLists.txt`
  gained `src/role.cpp` in the `ltlf_ek` library sources. No other call sites
  touched, per phase scope. Green checkpoint: `cmake --build build` clean,
  full suite 200/200 via `ctest`.
  One self-caught bug along the way, worth recording: the first draft of the
  `universe()` unit test built its expected set via
  `expected.insert(p.inputs().begin(), p.inputs().end())` followed by the
  same for `p.outputs()` — since `inputs()`/`outputs()` return `std::set` *by
  value*, `.begin()` and `.end()` in that one call each evaluate a *separate*
  temporary, pairing mismatched-container iterators (UB; reproduced as both a
  segfault and a livelock across runs). Fixed by dropping that scaffolding
  and asserting `universe()` against a literal `{"i0","i1","o0","o1"}`
  directly, per the PRD's stated oracle (#3 under "Test oracles"). Not a PRD
  deviation — a test-authoring bug caught and fixed before landing, noted
  here since the codebase already documents the same by-value-temporary
  hazard elsewhere (`ltlf_ek_synth.cpp` `PrintWitness`).
  Status intentionally left at `draft`: Worktrees A/B/C (product core, I/O +
  CLI, tests + residual comments) are out of scope for this session and
  remain to land before this PRD is `implemented`.
