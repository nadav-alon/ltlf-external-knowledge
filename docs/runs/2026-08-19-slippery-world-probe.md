# Day-run 2026-08-19 — slippery-world feasibility probe (idea 1 de-risk)

Wednesday's entry in `docs/plans/2026-08-17-week.md`. Unattended; no human in
the loop. Raw results: `docs/runs/2026-08-19-slippery-world-probe.json`.

- Repo `master` at `e3dffc0`; `ltlfsynt` = `~/opt/spot-2.15.1/bin/ltlfsynt`,
  asserted **2.15.1** before the run; `build/ltlf-ek-synth` `ldd`-confirmed
  against `~/opt/spot-2.15.1/lib`.
- **Nothing existing was modified.** Four new files: the fixtures under
  `tests/fixtures/slippery-world/`, their generator `scripts/slippery_world.py`,
  the certificate tool `scripts/slippery_world_cert.cpp` (compiled out of tree —
  it is deliberately *not* a CMake target), and this report + its JSON.
- Runner scripts (`sw_probe.py`, `sw_bench.py`, `sw_aggregate.py`) are in
  `build/scratch/`, as Monday's and Tuesday's were.

**Verdict: all four things idea 1 rests on hold.** 1 and 2 cleanly; 3 fully
(12/12 three-way agreement, and the realizable/unrealizable pair is free as
predicted for $N \ge 3$); 4 with a caveat that is the main finding of the day —
the gap opens, but it is a **constant factor and the encoding control does not
separate**, because the $A_N$ this probe generates does not exhibit Q5's
$\mathrm{DFA}(A) = 2^{\Theta(\lvert A \rvert)}$ blowup in *either* encoding.

---

## The domain, as built

$N \times N$ grid, `slip` in $\Ifree$, the position APs in $\Iknown$,
`mvl/mvr/mvu/mvd` in $\Ofree$, $\Oknown = \emptyset$. A move advances 1 cell, or
2 when `slip` holds. Both grill defaults are taken: **walls are no-ops**
(bumping = stay) and **$\delta$ is total over all 16 `mv` combinations** via the
fixed priority `l > r > u > d` (no direction = stay). Neither restricts an
output, so $A_{\text{rest}} = \top$ and the whole assumption lives in $\Tin$ —
which the run then confirms operationally: the EK side is handed $\gamma_N$
*alone*, with no residual conjunct.

$\Tin$ has one state per cell; $\lambda$ emits that cell's position literals and
does **not** read $\Sigma_0 = \{`slip`\}$ at all, so position at $t$ is a
function of the run's state, i.e. of history strictly before $t$ — the Moore
check under `\cref{def:indep}` (input-dependencies PRD *I3*) is satisfied by
construction, not by argument.

Two encodings, per Q5: **binary** ($2\lceil \log N \rceil$ APs, `bx*`/`by*`) and
the matched **one-hot** control ($2N$ APs, `hx*`/`hy*`).

Goals: `corner` = $\mathsf{F}(\text{pos} = (N{-}1, N{-}1))$ and
`centre` = $\mathsf{F}(\text{pos} = (c,c))$, $c = \lfloor (N{-}1)/2 \rfloor$.

$A_N$ is generated as the same transition table in LTLf, **factored per
coordinate** — $x$ moves iff the priority class is L or R and $y$ iff it is U or
D, so the two rules are independent and $A_N$ is $5N$ implications per
coordinate rather than $16N^2$. It uses **weak `X`**: with `X[!]` the rule would
be violated at the last position of every trace (the guards are total),
collapsing $A_N$ to `false`. That is not a stylistic choice — it is what makes
$A_N$ agree with "$\delta$ is undefined past the end" on the transducer side.

---

## 1. $\Tin$ is expressible in the transducer file format — **yes**

All six $\Tin$ (three $N$ × two encodings) parse under `Role::t_in` with the
part file's $\Ifree = \{`slip`\}$ / $\Iknown = \text{position}$ split, explicit
$\delta$ (HOA) and per-state $\lambda$ (`%%LAMBDA`). Guards mention `mv*` and
`slip`; $\lambda$ mentions position APs only, so the reader's
"$\lambda$'s APs $\subseteq \Sigma_0 \cup \Sigma_1$" check passes without
widening anything. No format extension was needed and nothing in
`transducer_io` was touched.

## 2. The Q3 certificate computes — **yes, in microseconds**

`scripts/slippery_world_cert.cpp` builds `emits_dfa(T_in)` and
`ltlf_to_dfa(A_N)` on one shared `bdd_dict` and runs a synchronous product
hunting a reachable state pair whose finality differs (a missing edge on either
side is an implicit rejecting sink). All six:

| N | encoding | position APs | $\lvert \Tin \rvert$ | $\lvert \mathrm{emits\_dfa}(\Tin) \rvert$ | $\lvert \mathrm{DFA}(A_N) \rvert$ | product | verdict |
|---|---|---|---|---|---|---|---|
| 2 | binary | 2 | 4 | 4 | 6 | 6 | EQUIVALENT_ON_NONEMPTY |
| 2 | onehot | 4 | 4 | 4 | 6 | 6 | EQUIVALENT_ON_NONEMPTY |
| 3 | binary | 4 | 9 | 9 | 11 | 11 | EQUIVALENT_ON_NONEMPTY |
| 3 | onehot | 6 | 9 | 9 | 11 | 11 | EQUIVALENT_ON_NONEMPTY |
| 4 | binary | 4 | 16 | 16 | 18 | 18 | EQUIVALENT_ON_NONEMPTY |
| 4 | onehot | 8 | 16 | 16 | 18 | 18 | EQUIVALENT_ON_NONEMPTY |

Every run is under 10 ms wall including process start. **Q3's guarantee is a
machine check, not an argument** — and it is cheap enough to be a per-domain
test rather than a sweep-time cost.

**The empty word is excluded and reported separately, for every instance and
every formula.** `emits_dfa`'s initial state is final by construction (a run of
length 0 vacuously agrees with $\lambda$), while the repo's LTLf convention
rejects the empty word — so the two disagree there *always*. That is a mismatch
between two encodings of "language", not a fact about this domain; the tool
prints `empty_word_Tin accepts` / `empty_word_A rejects` and compares only
non-empty words. A PRD adopting this certificate must say so explicitly, or the
first person to run it will read a spurious failure.

## 3. Three-way verdicts agree — **12/12**

Columns: `ltlfsynt --semantics=Mealy` on $A_N \to \gamma_N$;
ours-no-knowledge on $A_N \to \gamma_N$ (everything `input_free`);
ours-EK on $(\gamma_N, \Tin)$ — each under both `otf-mtdfa-product` and
`mtdfa-product`, so five verdicts per row, median of 3 runs.

| N | encoding | corner | centre | agree | $\gamma$ alone, $\Tin$ withheld |
|---|---|---|---|---|---|
| 2 | binary | REALIZABLE | REALIZABLE\* | ✓ | UNREALIZABLE |
| 2 | onehot | REALIZABLE | REALIZABLE\* | ✓ | UNREALIZABLE |
| 3 | binary | REALIZABLE | **UNREALIZABLE** | ✓ | UNREALIZABLE |
| 3 | onehot | REALIZABLE | **UNREALIZABLE** | ✓ | UNREALIZABLE |
| 4 | binary | REALIZABLE | **UNREALIZABLE** | ✓ | UNREALIZABLE |
| 4 | onehot | REALIZABLE | **UNREALIZABLE** | ✓ | UNREALIZABLE |

\* degenerate: at $N = 2$, $c = 0$, so the centre *is* the start cell and the
goal is met at $t = 0$. The realizable/unrealizable pair is free from $N = 3$ on,
not from $N = 2$.

The last column is a control the plan did not ask for and that seems worth
keeping: run $\gamma_N$ with the position APs demoted to `input_free` and no
transducer. It is UNREALIZABLE in all 12 cases, while the EK column is
REALIZABLE for `corner` — so **$\Tin$ is load-bearing**, not quietly ignored by
a method that would have said REALIZABLE anyway.

Two things this settles that the grill left as reasoning:

- **Mealy turn order does not collapse the pair.** `turn_order.hpp` puts
  $\Ifree$ strictly above every controllable, so the system sees `slip`$(t)$
  *before* committing to `mv`$(t)$ — it knows the step size in advance. That
  looked like it should hand the system the centre; it does not, because the
  environment adapts rather than fixing a slip pattern. At $N = 4$, for
  instance, playing `slip` at $x \in \{0,2\}$ and $\lnot$`slip` at $x = 3$ keeps
  $x \in \{0,2,3\}$ forever.
- The obstruction at $N = 3$ is the parity argument the grill gave (clamping at
  an even index preserves parity); at $N = 4$ it is the adaptive argument above.
  Different reasons, same verdict — worth knowing before a family generator
  pins "centre is the unrealizable member" as an invariant across $N$.

## 4. First timing signal — a gap, but a **constant** one

Wall-clock at these sizes is ~5–8 ms and is dominated by ~2.3–2.7 ms of process
start + input parsing, so the table below is the **in-process canonical** time
(`--benchmark`: `product_construction` + `game_solving`), median of 5, in µs.

| N | enc | goal | nk otf | ek otf | ×  | nk mtdfa | ek mtdfa | ×  |
|---|---|---|---|---|---|---|---|---|
| 2 | binary | corner | 1226 | 620 | 1.98 | 1250 | 610 | 2.05 |
| 2 | binary | centre | 983 | 578 | 1.70 | 1017 | 601 | 1.69 |
| 2 | onehot | corner | 1458 | 668 | 2.18 | 1270 | 632 | 2.01 |
| 2 | onehot | centre | 1079 | 592 | 1.82 | 1099 | 654 | 1.68 |
| 3 | binary | corner | 2469 | 956 | 2.58 | 2332 | 835 | 2.79 |
| 3 | binary | centre | 2398 | 818 | 2.93 | 2373 | 766 | 3.10 |
| 3 | onehot | corner | 2543 | 989 | 2.57 | 2504 | 865 | 2.90 |
| 3 | onehot | centre | 2469 | 937 | 2.63 | 2447 | 825 | 2.96 |
| 4 | binary | corner | 3895 | 1244 | 3.13 | 3696 | 974 | 3.80 |
| 4 | binary | centre | 3812 | 1141 | 3.34 | 3689 | 932 | 3.96 |
| 4 | onehot | corner | 3966 | 1671 | 2.37 | 3814 | 1267 | 3.01 |
| 4 | onehot | centre | 3962 | 1524 | 2.60 | 3946 | 1261 | 3.13 |

**Does the binary gap open by $N = 4$?** It widens — 2.0 → 2.6 → 3.1 (otf),
2.1 → 2.8 → 3.8 (mtdfa). **Does the one-hot control's gap stay closed?** *No,
and this is the finding.* One-hot tracks binary almost exactly (2.0 → 2.9 → 3.0
under mtdfa), so the separation the encoding control was designed to expose is
**not present at these sizes**.

The structural layer says why, and says it unambiguously:
$\lvert \mathrm{DFA}(A_N) \rvert = N^2 + 2$ in **both** encodings (§2's table).
The generated $A_N$ enumerates cells per coordinate, so it is $\Theta(N)$
symbols with a $\Theta(N^2)$ DFA — polynomial, in binary too. Q5's claim that
binary is the only encoding with $\mathrm{DFA}(A) = 2^{\Theta(\lvert A\rvert)}$
needs $A$ written **compactly over the position bits** (ripple-carry style,
$O(\log N)$), which this probe did not build and which is real generator work.
**Under an enumerated $A$ the two encodings are the same problem**, and any
family built this way will show a constant-factor EK win at every $N$ and no
encoding separation ever.

Where the constant factor comes from is visible and matches finding 3 of the
week plan — it is *translation cost*, not representation size:

- `goal_mtdfa_roots` is $N^2$ for ours-no-knowledge and **1** for ours-EK: the
  no-knowledge side translates $A_N \to \gamma_N$ and pays for all of $A_N$;
  the EK side translates $\gamma_N$ alone and is handed the rest as $\Tin$.
- `product_bdd_nodes` runs **higher** for EK (e.g. 289 vs 225 at $N=4$ binary),
  which is the same finding from the other direction: EK is not winning by
  carrying a smaller object, it is winning by not doing a translation.
- Transducer cost (Q1's own column) is not measured here: $\Tin$ is generated,
  not extracted. Its *parse* cost sits in the non-canonical column and is
  ~0.1–0.3 ms.

---

## For tonight's grill (Q9 onwards)

Nothing here is a blocker for the day-run; these are inputs to decisions the
user owns, recorded rather than guessed at.

1. **The encoding control is inert until $A_N$ is written compactly.** Either
   the new PRD carries a compact/arithmetic $A_N$ generator as a phase (with the
   Q3 certificate as its correctness test — which is exactly what makes that
   safe to attempt unattended), or Q5's binary-vs-one-hot control should be
   dropped from the headline rather than reported as a closed gap.
2. **The certificate is cheap enough to be a per-domain test**, and it must
   document the empty-word exclusion. It also hardens tier T1's hand-declared
   $\psi_{in}$ for the existing families (week-plan finding 2) at negligible
   cost.
3. **Structural layer for these families** (an open question in the plan): use
   ours-no-knowledge as the structural baseline and keep `ltlfsynt`
   timing-only, as the plan leaned. `goal_mtdfa_roots` ($N^2$ vs 1) is the
   sharpest single number the probe produced and it is exactly the
   translation-cost claim; `product_bdd_nodes` is worth asserting too,
   *including* that EK's is larger.
4. **"Centre is unrealizable" is not an invariant of $N$** — it fails at
   $N = 2$ for a degenerate reason. A family generator should assert the pair's
   verdicts per $N$ (cheaply, via `ltlfsynt`) rather than hard-code them.
5. **Amortization has no measurement yet.** Q1's reusable-knowledge column and
   the break-even $k$ need an *extraction* cost, and this probe hand-builds
   $\Tin$. Friday's break-even number needs either an extractor run on $A_N$ or
   an explicit decision that a hand-built domain transducer has zero amortized
   cost.

## Stop-list compliance

- No verdict disagreement occurred, so nothing was "fixed" (there was nothing
  to record under Stop-list 4).
- $N$ was not scaled past 4 chasing a gap — the $N = 4$ row is the last one, and
  the gap's shape is reported as measured.
- `turn_order.hpp` untouched; no existing file modified at all.
