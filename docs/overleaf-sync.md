# Overleaf sync workflow

The theory lives in the `latex/` **git submodule**, whose `origin` *is* the
Overleaf git bridge (`git.overleaf.com/6a20…`). So "push to Overleaf" is just
`git push` inside `latex/`. The parent repo (`ltlf-external-knowledge`) stores
only a **pointer** to one commit of that submodule; after any sync you bump the
pointer with a commit in the parent.

Edits happen on **both sides** (local + Overleaf web editor), so the single rule
is: **pull-rebase before you push.** The submodule tracks `main` (`branch = main`
in `.gitmodules`, `update = rebase` locally), so it stays on a branch rather than
detaching.

## A. I edited locally → send to Overleaf

```sh
cd latex
git add -A && git commit -m "<what changed>"
git pull --rebase        # pick up any Overleaf-side edits first
git push                 # → now visible in Overleaf, PDF rebuilds there
cd ..
git add latex
git commit -m "Bump latex submodule to $(git -C latex rev-parse --short HEAD) (<summary>)"
```

## B. Someone edited in Overleaf web → pull into local

```sh
git submodule update --remote latex     # fetch + rebase onto origin/main, stays on main
git add latex
git commit -m "Bump latex submodule to $(git -C latex rev-parse --short HEAD) (<summary>)"
```

## Conflicts

If `git pull --rebase` (A) or `submodule update --remote` (B) reports a conflict,
both sides touched the same lines of `latex/main.tex`. Because the LaTeX style is
**one sentence per source line** (`/latex-style`), conflicts are line-scoped and
usually trivial:

```sh
cd latex
# edit latex/main.tex to resolve, then:
git add main.tex && git rebase --continue
git push
cd ..
git add latex && git commit -m "Bump latex submodule to $(git -C latex rev-parse --short HEAD) (resolve conflict)"
```

## Notes

- **Never** edit `latex/` files without committing inside `latex/` — a dirty
  submodule working tree shows up in the parent as an uncommitted-pointer diff and
  blocks a clean bump.
- The parent's pointer and the submodule's `origin/main` should always match after
  a sync; check with `git submodule status` (leading `+`/`-` means they diverge).
- A cloned checkout needs `git submodule update --init` once to populate `latex/`.
