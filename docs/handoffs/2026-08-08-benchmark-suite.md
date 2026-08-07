# Handoff: the parametric benchmark suite — grill continuation

**Written 2026-08-08**, from a `/grill-me` session that ranked the next slot and
then went too deep into this item's design. Everything below is *settled ground*
or *evidence found*; the open questions at the bottom are where the next grill
picks up. Read this **instead of** re-discovering the state — the discovery cost
is what this file exists to remove.

Feeds: `docs/BACKLOG.md` → "Parametric benchmark suite" (ranked **#2**,
2026-08-08). Its eventual PRD is `docs/prd/benchmark-suite.md` (not yet written).

---

## Why this item exists at all — the finding that started it

**The flagship empirical result of the project is not reproducible.**
`docs/prd/otf-mtdfa-product.md:702`, verbatim:

> Live `--benchmark` sweep over the CLI against the standing champion
> `MtdfaProduct`; min of 3 runs, 20 s timeout, realizable **and** unrealizable in
> both families. **Harness and raw JSON are throwaway (not committed)**;
> everything needed to reproduce is below.

So the **5488x** headline — the result that justifies Method 3.1, makes it "the
first method to beat the standing champion", and is destined for the paper —
survives only as a markdown table transcribed from a harness that no longer
exists. The same is true of `MtnfaProduct`'s **16x-slower** negative result
(`docs/prd/mtnfa-product.md:17`), which came from a *different* ad-hoc probe.

**What exists today.** `include/ltlf_ek/bench.hpp` is *observability only*: a
thread-local RAII span collector (`BenchScope` / `BenchTimer` / `BenchSpan` /
`BenchReport`) plus a closed `enum class Stage`
(`automaton_construction`, `product_construction`, `game_solving`,
`aggregation`). The CLI exposes it as `ltlf-ek-synth --benchmark=FILE`, which
writes one nested-JSON report per run (`src/ltlf_ek_synth.cpp:331-338`).

**What does not exist.** Any parametric formula family, any sweep runner, any
results store, any cross-method table, any plotting. `docs/runs/` holds exactly
two reports, both from the ltlfsynt-oracle work. There is no bench binary — only
`ltlf-ek-synth` and `ltlf-ek-deps`.

**Why it compounds, and why it outranked the method work.** The evening ranking
decisions are *already* being made on those vanished numbers: the backlog rejects
Method 3.1 Phase 2 because it "optimizes a term that is no longer the bottleneck",
citing a 501 ms vs 249 ms measurement that cannot be re-run. Scarce evenings are
being spent arbitrating between methods on evidence that no longer exists.

---

## Settled in the 2026-08-08 grill

### S1. Two-layer output, different lifecycles

Committing raw timings is worse than useless — they are machine- and
Spot-version-dependent, so every run dirties the diff and a regression is
indistinguishable from a warm cache. But "not committed" is the whole defect.
Resolution:

- **Layer 1 — structural metrics. Deterministic, committed, asserted in `ctest`.**
  State counts (goal automaton, product, NFA where applicable), plus $|\psi|$ for
  the ltlfsynt-comparable tiers. Machine-independent, reproducible to the integer.
  Committing them makes the family definitions **self-validating** and turns the
  suite into a **regression test**: a refactor that silently doubles a product
  size fails CI. Timings can never do that.
- **Layer 2 — timings. Nondeterministic, generated, snapshotted.** Written to a
  gitignored output dir; one report committed per sweep under `docs/runs/`,
  carrying provenance: machine, Spot version (**`ldd`-resolved, not
  `pkg-config`** — see `docs/BACKLOG.md` and CLAUDE.md on shadowing installs),
  repo commit, timeout, repetition count.

**The structural layer is not a supplement — it is the validity check on the
family.** The repo learned this twice, independently, and wrote it down both
times:

> **State counts (the decisive number; timing alone cannot separate "slower" from
> "builds more").** — `docs/prd/mtnfa-product.md:624`
>
> **Anyone re-running this must check the NFA size first — the intuitive family
> silently degenerates.** — `docs/prd/mtnfa-product.md:620`

and Method 3.1 then did exactly that, calling it "the check
`docs/prd/mtnfa-product.md` learned to run *first*"
(`docs/prd/otf-mtdfa-product.md:726`). A family whose structural numbers do not
discriminate produces timings that mean nothing.

### S2. ltlfsynt is a **tier**, not a phase — and comparability is *declared*

`docs/prd/benchmarking.md`'s own Goal scoped this in on day one — "and, via an
end-to-end wall total, against external tools such as `ltlfsynt`" — and it was
never done. Today `ltlfsynt` appears only in `tests/ltlfsynt_oracle_test.cpp`,
comparing **verdicts**, never timed.

**Not every benchmark can be handed to ltlfsynt, and the reason is structural,
not a matter of convenience.**

- The existing oracle already lives inside the encodable subset *silently*: every
  corpus row carries a **hand-authored** $\psi_{in}$ string. There are exactly
  **four** transducers — `const-true` → `G(k)`, `const-false` → `G(!k)`, `copy` →
  `G(k <-> a)`, `delay` → the corrected safety formula
  (`tests/ltlfsynt_oracle_test.cpp:293-371`). `random_tin` builds arbitrary DFAs
  and the ltlfsynt oracle **cannot consume one of them**; only the internal
  cross-method oracle can.
- LTLf $\equiv$ FO[$<$] $\equiv$ star-free / aperiodic languages. `Transducer`'s
  $\delta$ is an arbitrary deterministic transition over $\Sigma = 2^{I \cup O}$
  (`include/ltlf_ek/transducer.hpp:32`) — i.e. **arbitrary regular**. Regular
  $\supsetneq$ star-free, so there exist transducers with **no** $\psi_{in}$ at
  all.

**The three tiers, and the rule that matters:**

| Tier | Condition | ltlfsynt cell |
|---|---|---|
| **T1** | $\psi$ exists and is $O(\lvert\tau\rvert)$. All four current corpus transducers. | Honest wall-clock race. The only tier where an external comparison is a legitimate claim. |
| **T2** | $\tau$ aperiodic so $\psi$ exists, but DFA→LTLf is non-elementary in general. | Reportable **only** with $\lvert\psi\rvert$ as a column, never merged into the T1 table — racing here measures *your encoding*, not your method. |
| **T3** | $\tau$ non-aperiodic. No $\psi$ exists, at any size. | **n/a by expressibility** — a capability separation, not a timing result. |

**The load-bearing design rule: the tier is a property *declared* by the family,
never sniffed at runtime.** If T2 can silently drift into the T1 table the
headline external comparison becomes dishonest by accident — exactly the failure
mode `docs/BACKLOG.md`'s "why the oracles were blind to it" section already
documents.

### S3. Matched T1/T3 pairs, not post-hoc annotation

Families are designed as **controlled pairs**: the same $\varphi_n$ instantiated
once with an encodable $\Tin$ and once with a non-aperiodic one. This is the
pattern the repo has already proven — `docs/prd/otf-mtdfa-product.md:718` built
exactly such a pair (Family A where $\cons$ prunes vs Family B where it does not,
same $\varphi_n$, only the transducer moved), and that is what made the 3.1 result
legible rather than anecdotal.

It also converts "ltlfsynt can't run some of our benchmarks" from an apology into
a measurement: *same formula, same size, same game — swap a 2-state transducer and
the competing tool leaves the table.*

### S4. The T3 witness — **verify before it goes in the paper**

Proposed: $\Tin$ with **2 states**, $\delta$ toggling on input $a$, $\lambda$
pinning $k$ true in the even state and false in the odd one — "$k$ holds iff an
even number of $a$'s have occurred so far". Parity of a count is the canonical
non-FO[$<$] property (its syntactic monoid contains $\mathbb{Z}/2$, hence
non-aperiodic), so **no LTLf formula of any size expresses it**.

Two cautions recorded deliberately:

- This is a *reasoned* claim from the standard LTLf $=$ star-free correspondence,
  **not yet checked against a source or a proof in `main.tex`**. It would be a
  paper-level claim, so it wants a real check first.
- A near-miss to avoid: "$k$ simply alternates T,F,T,F" is **star-free** and
  therefore *not* a witness — alternation is locally definable
  (`k & G(k -> X !k) & G(!k -> X k)`). The counting must be over an
  *input-triggered* toggle for aperiodicity to actually fail.

### S5. Participants

All five `Synthesis` implementations: `DfaProduct`, `NfaProduct`, `MtdfaProduct`,
`MtnfaProduct`, `OtfMtdfaProduct`. **Including the two with negative results** — a
progression table that omits the losers is not a progression.

### S6. Ordering interaction with the acceptance-mark bug (`#1`)

Fixing the acceptance-mark bug changes `DfaProduct` and `NfaProduct` verdicts on
partial transducers. If the benchmark suite landed **first**, the fix would get a
committed structural baseline to diff against and you would see exactly which
cells moved; landing the fix first means re-baselining blind. Judged a *mild*
argument for swapping #1 and #2 — **rejected**, because the bug is the one that is
actually wrong and its blast radius grows with every method added. Recorded so the
next grill does not re-litigate it.

---

## Open — where the next grill starts

1. **Hard `ctest` assertions on the structural layer, or a report you read?**
   Assertions on machine-independent integers are cheap and catch silent
   regressions, but they will fail loudly the first time a Spot upgrade legitimately
   changes a state count. *This is the question the 2026-08-08 session stopped on.*
2. **Runner shape.** A new `ltlf-ek-bench` binary, a script driving
   `ltlf-ek-synth --benchmark=FILE`, or a `ctest` target? Interacts with (1).
3. **Family parameterisation.** What is the axis — formula size $n$ as in
   `\varphi_n = F(k \wedge X[!]^n k)$, transducer state count, alphabet width, or
   several? How many families is enough to call it a progression?
4. **Baseline for T2/T3.** With ltlfsynt off the table, do the methods race only
   each other (`MtdfaProduct` as standing champion), or does a monolithic internal
   baseline get built?
5. **`random_tin` and non-aperiodicity.** T3 needs a generator that can
   *deliberately* emit a non-aperiodic transducer; today `random_tin` has no such
   notion (and is deterministic + **total** by construction —
   `tests/ltlfsynt_oracle_test.cpp:1337`).
6. **Do the two existing results get re-derived?** The 5488x and the 16x-slower
   tables were transcribed from vanished harnesses. Reproducing them under the new
   suite would validate the suite *and* re-establish the numbers — or reveal that
   one of them does not reproduce, which is itself worth knowing before the paper.
7. **Phasing.** Provisionally: Phase 1 = families + runner + structural layer;
   Phase 2 = timing layer + the ltlfsynt T1 race. Not yet a decision.

## Known dependency, non-blocking

Racing ltlfsynt requires handing it the monolithic encoding
$\psi_{in} \rightarrow (\varphi \land \psi_{out})$, which is a **conjecture, not a
theorem** (`docs/BACKLOG.md` → "Prove the monolithic reduction"). This does **not**
block benchmarking — the corpus oracle already cross-checks verdict agreement
empirically, and there is no known divergence witness — but the paper's external
comparison carries a caveat until it is proved.
