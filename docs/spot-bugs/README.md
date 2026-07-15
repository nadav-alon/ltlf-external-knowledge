# Spot bug reproducers

Self-contained reproducers for upstream Spot bugs we have hit. **Nothing here is
part of the build** — these are kept so a bug can be reported to
`spot@lrde.epita.fr` (or re-checked after a Spot upgrade) without rebuilding the
diagnosis from scratch.

## The MTDFA backprop winning-strategy segfault

**Status:** open, **trigger not fully pinned**, not yet reported upstream. Found
2026-07-15 while integrating `docs/prd/mtdfa-product.md` Phase 1 (see that PRD's
*Phase 1 blocker* section).

`spot::mtdfa_winning_strategy(P, /*backprop=*/true)` **segfaults** when `P` is a
`spot::product()` of MTDFAs. All documented preconditions are met — operands are
deterministic (and `twadfa_to_mtdfa` throws otherwise) and marked
`prop_state_acc(true)` — so this is a genuine upstream bug, not a misuse.

**Crash site** (linked `libspot 2.14.4.dev`; the `2.15.1.dev` source differs by
two lines but the code path is unchanged, so an upgrade is **not** expected to
fix it):

```
spot/twaalgos/ltlf2dfa.cc:1593  strategy_finalize
  -> global_backprop->root_winner(term / 2)
  -> spot/twaalgos/backprop.hh:102  winner(s) { return (*this)[s].winner; }
```

`winner()` indexes the adjlist **without a bounds check**. `root_winner()` is
itself guarded — it returns `-1` when the root is absent from
`rootnum_to_backprop_state` — so the map entry *exists* but resolves to an
out-of-range backprop state. `global_backprop` is valid (verified in gdb), so
this is not a null/dangling-global problem.

**Best remaining lead** (unverified): the encode loop at `ltlf2dfa.cc:3571-3573`

```cpp
for (unsigned i = 0; i < ns; ++i)
  if (enc.encode_state(i, dfa->states[i]))
    break;              // <-- encodes only a PREFIX of the roots
```

breaks early, while the `apply1` loop that follows runs over **all** `ns` roots.

## The two reproducers

| file | needs `ltlf_ek`? | shows |
|---|---|---|
| `mtdfa-backprop-segfault.cc` | no — **pure Spot** | a rejecting sink triggers it on a 5-root product |
| `mtdfa-backprop-segfault-sinkfree.cc` | yes (real `emits_dfa`) | a **sink-free** operand still triggers it on a 10-root product |

```console
# pure Spot; sink vs no-sink vs ltlf_to_mtdfa, same filter language
$ g++ -std=c++17 -g mtdfa-backprop-segfault.cc -o r $(pkg-config --cflags --libs libspot)
$ ./r spot     # -> SURVIVED
$ ./r nosink   # -> SURVIVED
$ ./r sink     # -> Segmentation fault

# sink-free replay of generated-corpus case i=48
$ g++ -std=c++17 -g mtdfa-backprop-segfault-sinkfree.cc -o rs \
      -I../../include $(pkg-config --cflags libspot) \
      ../../build/libltlf_ek.a $(pkg-config --libs libspot)
$ ./rs                                  # -> Segmentation fault
$ ./rs "p1 | p6 | F(Gp1 & Fp7)" false   # -> SURVIVED  (backprop=false)
```

## What the trigger is NOT — a correction worth reading

The first diagnosis (2026-07-15) concluded **"the materialised rejecting sink is
the trigger."** That was **wrong**, and the way it was wrong is instructive: the
sink hypothesis explained every case then in evidence, and dropping the sink did
fix two of the three failing tests — but the third kept crashing, on a **total**
transducer whose `emits_dfa` never had a sink at all.

Ruled out by experiment:

- **the rejecting sink** — sink-free products crash too (second reproducer);
- **root count alone** — sink-free 5-root products survive, and the 5-root *sink*
  product crashes; the sink-free crash needs 10;
- **`X[!]` in the goal** — the corpus trigger `p1 | p6 | F(Gp1 & Fp7)` has none;
- **a Spot version gap** — the `2.15.1` source has the same unchecked `winner()`
  and the same early-breaking encode loop;
- **the trivial `t_out` operand** (an all-accepting `bddtrue` self-loop);
- **the goal automaton alone** — with no product at all it survives.

What survives every observed case is **`backprop=false`**, since
`strategy_finalize` is exclusive to the backprop path. That is a **workaround
with a real price**, not a fix: the header promises the backprop route is a
*"linear-time resolution"* where `false` *"does not have linear complexity"*, and
cost is the entire premise of the mtdfa route.

Dropping the sink is still worth doing on its own merits — it is cheaper
(Phase 0/Q1 already called the sink "wasted") and `main.tex` *skips* non-enabled
letters, which an incomplete automaton does literally. It is simply **not** a fix
for this bug.
