# Why `OtfMtdfaProduct`'s `game_solving` is ~2x `MtdfaProduct`'s (2026-10-17)

**Verdict: the rows are not what differs; the heap state at the start of the solve is the
strongest candidate, not a proven mechanism.** On this family the two product mtdfas have the same
node count, no sharing difference and the same variable order (a degenerate shape, see below).
Consolidating glibc's free lists before the timed solve takes `OtfMtdfaProduct`'s `game_solving`
from ~550 ms to ~280 ms, so the slowdown goes away after a heap consolidation. What is *not*
established is why: `MtdfaProduct`'s path also starts its solve with ~2M free chunks and is fast,
so the free-chunk count does not predict the slowdown, and the experiments do not separate
free-list layout from the cache, TLB and page-fault side effects of a 320 ms heap walk. The
consolidation costs as much as the saving, so the gap is *deferred* cost, not avoided cost. The
fix below removes the small-chunk garbage `otf_product_to_mtdfa` leaves; whether that restores
parity is a hypothesis to test, not a result.

Instance: `cons-inert` (family B of `docs/prd/otf-mtdfa-product.md`, `k` in `Ofree`, trivial
transducers), realizable. Release build, Spot 2.15.1 (installed at `/usr/local` on the measuring machine), commit `ab08322`, i7-13700,
glibc malloc, one process per measurement unless stated.

## Reproduction

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSPOT_ROOT=~/opt/spot-2.15.1
cmake --build build -j --target ltlf-ek-bench
mkdir -p build/out
build/ltlf-ek-bench --families=cons-inert --subjects=mtdfa-product,otf-mtdfa-product \
    --n-min=20 --n-max=20 --repeat=3 --timeout=120 --out=build/out/gap.json
```

`game_solving`, realizable, min of 3 (ms): `mtdfa-product` **280.5**, `otf-mtdfa-product`
**551.5**: the recorded 249 vs 501, at 1.97x. (Unrealizable rows are ~40 us on both: the
solver returns immediately, so the gap is in the winning-strategy computation.)

The probe, `scripts/otf_game_solving_gap_probe.cpp`, reproduces it without the sweep runner:

```sh
c++ -std=c++20 -O2 -I include -isystem ~/opt/spot-2.15.1/include \
    scripts/otf_game_solving_gap_probe.cpp build/libltlf_ek.a \
    -L ~/opt/spot-2.15.1/lib -lspot -lbddx \
    -Wl,-rpath,$HOME/opt/spot-2.15.1/lib -o build/otf-gap-probe
build/otf-gap-probe 20 1 synth-spot    # real MtdfaProduct::synthesize, BenchScope span
build/otf-gap-probe 20 1 synth-otf     # real OtfMtdfaProduct::synthesize
```

| n=20 | `automaton_construction` | `product_construction` | `game_solving` |
|---|---|---|---|
| `synth-spot` | 5700.7 | 1913.7 | **291.5** |
| `synth-otf` | n/a | 6345.9 | **565.9** |

## What it is not: the rows

`build/otf-gap-probe 20 1 both`:

| n=20 | roots | BDD nodes | terminals | sum of per-root nodes / distinct nodes | edges against the variable order |
|---|---|---|---|---|---|
| `spot::product` | 1048577 | 1048576 | 1048577 | 1.00 | 0 |
| fused (`otf_product_to_mtdfa`) | 1048576 | 1048576 | 1048576 | 1.00 | 0 |

Every row is one decision node on `k` (variable 0) over two terminal leaves; no node is shared
between rows on either path, and no path descends to a variable at or above its parent's. That
is true by construction in this family (and the inversion check skips edges into terminals), so
this table only confirms the two products are equal here; it says nothing about "no sharing
difference, same variable order" for a family with nontrivial rows, such as #25's. Spot's
product has one extra root (the accepting sink), which the fused build collapses to `bddtrue`,
so the fused product is if anything smaller. Row order (root-to-next-root BuDDy id stride) differs
(`LAYOUT` was measured at n=18 only: 2.8e5 fused vs 2.4e5 Spot). Layout is not ruled out:
every consolidated run below changes only the heap, so nothing here tests row layout separately,
and the stride has not been measured at n=20.

Inside the solve the time is all `mtdfa_winning_strategy` (`strategy_to_mealy` is ~0.02 ms).
The solve allocates no BDD nodes (BuDDy `produced` and GC count are unchanged across it).

## The cause: free-list state at the start of the solve

The gap is not stable under a `printf` placed before the timed solve, which is how this was
found. `steps-spot` and `steps-otf` run `synthesize`'s steps by hand with nothing printed
before the timer:

| n=20, `MALLINFO=1` | free chunks before solve | `game_solving` (ms, 3 runs) |
|---|---|---|
| `steps-spot` | 2 059 810 | 274.7, 285.2, 285.9 |
| `steps-otf` | 2 406 952 | 549.7, 556.9, 557.9 |
| `steps-otf`, `TRIM=1` (`malloc_trim(0)` first) | 122 | 276.9, 281.7, 283.6 |
| `steps-otf`, `CONSOLIDATE=1` (free one 100000-byte block first) | 122 | 254.7, 247.5, 241.7 |

Not yet measured: the minor-page-fault column (the probe prints it for every `steps-*` run, but it
was not recorded for these rows), `steps-spot` with `TRIM=1` or `CONSOLIDATE=1` (so whether Spot's
path also pays a heap-state cost is unknown, and parity with `MtdfaProduct` may be the wrong
target: `steps-otf` consolidated runs at 241-254 ms, below plain `steps-spot`), and a control that
touches the same memory without consolidating (a read-only walk of the product).

- Consolidating the free lists takes `steps-otf` from 2x to parity with, or better than, Spot's.
  It is the same product; only the heap differs. The same flip was observed at n=18 by merely
  printing before the solve (a `printf` allocation triggers consolidation); `PRINT=1` now
  reproduces that in the probe, but it has not been re-run at n=18 since it was added.
- Consolidation is not free: `malloc_trim(0)` took 330 ms and the 100000-byte free took 320-330
  ms at n=20. Total `build-end -> solve-end` is therefore about the same, ~600 ms either way.
  The slow solve is where the unpaid bill lands, not a solver inefficiency.
- The 2.4M chunks come from the build's *teardown*. With a temporary `mallinfo2()` print in
  `otf_product_to_mtdfa`: 32 763 free chunks just before `return out;`,
  2 406 952 once it has returned. What is destroyed on return is `state_index` (a `std::map<Key,
  unsigned>` whose every node owns a heap `std::vector<unsigned>` for `Key::q`, so ~2 chunks per
  state), plus `pending` and the other locals. At 1M states that is the whole count.
- `MtdfaProduct`'s path also ends with ~2M free chunks (table above), yet its solve is fast. So
  the count alone does not predict the slowdown. One guess is that it is the layout of those
  chunks (the fused build's are all freed in one teardown, in map order), but nothing here
  tells the two apart: a measurement of the teardown's free order, or a probe that frees ~2.4M
  chunks in map order before the Spot solve and checks whether it slows down, would. Why the
  layout would matter to the solver is not established here.

What was *not* measured: cache misses or TLB behaviour (`perf` is not installed here), so the
step from "2.4M unconsolidated chunks" to "the solver's allocations are scattered" is inferred
from the consolidated runs being fast, not observed directly.

## Recommended fix

Make the fused build's per-state bookkeeping not leave small chunks behind. Allocate
`state_index` and `pending` from a `std::pmr::monotonic_buffer_resource` (or intern `Key::q`
inline: with two transducers it is a fixed-size array, so `Key` needs no vector at all), so that
teardown frees a few large blocks rather than millions of small ones. Two parts, in order of
effect:

1. `Key::q` as a fixed-capacity inline array (|taus| is 2 in `synthesize`): removes one of the
   two chunks per state and a pointer chase per `state_index` comparison.
2. A monotonic arena under the `std::map` nodes and the `deque`: removes the other, and makes
   teardown O(1).

Estimated effect, for the fused-build row of the `cons-inert` table: `game_solving` 550 ->
~250-280 ms at n=20, which is where the consolidated rows land. This is an upper bound on what the
fix can buy, not a prediction: it assumes the teardown garbage is the whole cause, and the caveats
above (free-chunk count not predictive, heap-walk side effects not separated) are exactly where
that assumption is weak. The acceptance check is the end-to-end one: `steps-otf` `game_solving`
within noise of `steps-spot`, with no `TRIM`/`CONSOLIDATE`. The free-chunk count before the
solve is a diagnostic to report beside it, not the pass criterion.

This applies unchanged to #25's on-the-fly solve: it will build the same `Key` table, so it
should take the same layout rather than inherit the hazard. It is independent of variable order,
which is not implicated.

Not recommended: a `malloc_trim(0)` between build and solve. It moves the cost (330 ms) into the
build and returns memory to the OS only to page it back in (35k faults).

## Addendum (#163): what the fix measured

The `Key`/arena hypothesis above was only partly right. Measured on `cons-inert` n=20
(`steps-otf`, `game_solving`; `steps-spot` ~260 ms, baseline ~550 ms):

| change | `game_solving` | free chunks |
|---|---|---|
| baseline | ~550 ms | ~2.2M |
| inline `Key::q` + pmr arena only | ~520 ms | ~2.2M |
| `ForwardProgression` kept alive only | ~260 ms | 2 |
| `ForwardProgression` freed at return, with `Key`/arena | ~400 ms | ~556k |
| both (shipped) | parity with `steps-spot` | few |

The larger cause was the teardown of Spot's `ltlf_translator` caches between build and solve.
The returned product now owns the `ForwardProgression`; its teardown is charged to
`game_solving` (the product is dropped inside that stage), so per-stage totals stay comparable
with `steps-spot`.
