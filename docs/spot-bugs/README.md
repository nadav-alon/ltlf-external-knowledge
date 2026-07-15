# Spot bug reproducers

Self-contained reproducers for upstream Spot bugs we have hit. **Nothing here is
part of the build** — these are kept so a bug can be reported to
`spot@lrde.epita.fr` (or re-checked after a Spot upgrade) without rebuilding the
diagnosis from scratch.

Each file is pure Spot: if a reproducer needed `ltlf_ek` to trigger, it would not
yet be evidence of an upstream bug.

## `mtdfa-sink-segfault.cc`

**Status:** open, not yet reported upstream. Found 2026-07-15 while integrating
`docs/prd/mtdfa-product.md` Phase 1 (see that PRD's *Phase 1 blocker* section).

`spot::mtdfa_winning_strategy(P, /*backprop=*/true)` **segfaults** when `P` is a
`spot::product()` one of whose operands came from `twadfa_to_mtdfa()` on a DFA
that materialises a **rejecting sink** (a non-accepting state with a `bddtrue`
self-loop). The same language without the sink — an *incomplete* DFA, where a
missing edge is already an implicit reject — is fine, as is the same language
built by `ltlf_to_mtdfa()`.

All documented preconditions are met: the input is deterministic (checked, and
`twadfa_to_mtdfa` throws otherwise) and marked `prop_state_acc(true)`.

```console
$ g++ -std=c++17 -g mtdfa-sink-segfault.cc -o mtdfa-sink-segfault \
      $(pkg-config --cflags --libs libspot)
$ ./mtdfa-sink-segfault spot     # -> SURVIVED
$ ./mtdfa-sink-segfault nosink   # -> SURVIVED
$ ./mtdfa-sink-segfault sink     # -> Segmentation fault
```

**Crash site** (linked `libspot 2.14.4.dev`; the `2.15.1.dev` source differs by
two lines but the code path is unchanged, so an upgrade is *not* expected to fix
it):

```
spot/twaalgos/ltlf2dfa.cc:1593  strategy_finalize
  -> global_backprop->root_winner(term / 2)
  -> spot/twaalgos/backprop.hh:102  winner(s) { return (*this)[s].winner; }
```

`winner()` indexes the adjlist **without a bounds check**. `root_winner()` itself
is guarded — it returns `-1` when the root is absent from
`rootnum_to_backprop_state` — so the map entry exists but resolves to an
out-of-range backprop state. The exact bookkeeping that produces the bad entry
(likely the early-breaking `encode_state` loop at `ltlf2dfa.cc:3571-3573`, which
encodes only a prefix of roots while the `apply1` loop then runs over *all* of
them) is **not** pinned.

**Three conditions are needed**, which is why the failure looked erratic:

1. an operand with a materialised rejecting sink;
2. `backprop=true` — `strategy_finalize` is exclusive to that path, so
   `backprop=false` merely dodges the bug;
3. a goal that is **realizable** and forces a next step (`X[!]`).

Condition 3 is the subtle one: `strategy_finalize` returns early on accepting
terminals (`term & 1`) *without* touching `global_backprop`, so goals whose
strategy only ever finalizes accepting terminals (`G(!k)`, `F(!k)`, `!k`) never
crash, and unrealizable goals collapse before reaching it.

**Our side of it:** `emits_dfa` materialised the sink because the PRD pinned that
shape. The fix is to leave the automaton incomplete instead — which also costs
nothing (Phase 0/Q1 already called the sink "correct, but wasted") and keeps
`backprop_nodes=true`, so the PRD's linear-time claim survives.
