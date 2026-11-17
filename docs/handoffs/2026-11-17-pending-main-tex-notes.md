# Pending `main.tex` notes, as one patch (#318)

Apply from the main checkout, inside the Overleaf submodule, uncommitted:

```sh
git -C latex apply ../docs/handoffs/2026-11-17-pending-main-tex-notes.patch
```

It applies cleanly to `latex/main.tex` at the committed revision `56127df`.
The 2026-08-09 partiality notes are already in that revision, so they are not in the patch.

## What it adds, and where

- Preamble: `\Lin`, `\Lout`.
- After the first `\na` of `def:probDef`: who ends the trace.
- After the divergence-witness note under `def:probDefTransducer`: the totality hypothesis, `thm:monolithic` with its proof (#36, under totality only, termination and definability confirmed 2026-10-11), and a closing `\cl` note.
- After the reachability `\textbf{Note}` of Method 1: `\algname{NfaToDfa}`'s empty-subset rule.
- Inside the `\algname{FP}` `\cl` note of Method 3: the `X[!]\mathtt{tt}` idiom, cited by `\cref` label, not line number.
- Section `indep`: notes 1 and 2, after the `lem:indep-diagonal` and `lem:indep-transducer` notes.

## Left out

- §`indep` note 3 edits `docs/prd/input-dependencies-tool.md` I2 and `docs/GLOSSARY.md`'s *Violation automaton* entry, not `main.tex`.
- Term drift: `docs/GLOSSARY.md` and prose elsewhere should say "cons with its partiality clause", not "enabled".
- The `docs/prd` note on the oracle's assumptions for #36, which is not a `main.tex` edit.
- The `docs/GLOSSARY.md` *Produced-trace language* entry: once the patch is applied it needs a symbol row for `\Lin`/`\Lout` and its definition must drop "$\text{LTL}_f$", since the language is regular but not always $\text{LTL}_f$-definable.
- #37's question of whether the system may step outside a transducer's domain to end the trace there is **still open**: the patch declares partial transducers out of scope, it does not answer it, so #37 should not be closed on this patch.
- Compilation could not be run here (Overleaf only); the patch was checked for balanced braces and environments, and that every `\cref` label it uses exists. #318's "`main.tex` still compiles with the patch applied" criterion stays unticked until the developer has compiled on Overleaf.
