#!/usr/bin/env python3
"""Keep `main.tex:NNN` citations across the repo pointing at what they meant.

The paper is an Overleaf submodule edited out of band, so every pull shifts line
numbers and silently invalidates the ~70 `main.tex:NNN` citations in docs/,
include/, src/ and tests/.  Nothing used to detect that: drift was found by a
human tripping over a wrong line months later, and repairing it meant re-reading
every target by hand.  Worse, the drift is NOT a uniform offset -- one 2026-07-30
pull moved the align block -3, the consistency block +3 and the commented-out
input-dependency block +50 -- so "add N to everything" produces garbage.

The fix is to stop trusting the number and remember the *content* it named.  For
every cited line this script records a FINGERPRINT: that line's text, extended
over following lines only as far as needed to be unique.  `--check` then verifies
each citation still lands on its fingerprint, and `--fix` relocates the
fingerprint and rewrites every citation of that line (shifting range ends to
match).  Citation sites stay untouched, which is why this scales to ~70 of them.

Usage:
  check-main-tex-refs.py --check      # exit 1 if any citation drifted (pre-commit hook)
  check-main-tex-refs.py --fix        # relocate fingerprints, rewrite citations, re-snapshot
  check-main-tex-refs.py --snapshot   # (re)record fingerprints for the CURRENT main.tex

Run `--fix` as the first thing after every Overleaf pull, i.e. in the same commit
that bumps the `latex` submodule pointer.

Dangling `\cref{...}` targets are reported too, as WARNINGS -- they are a content
question for a human, not drift this script can repair, and must not block an
unrelated commit.

If `latex/main.tex` is absent this exits 0 without complaint: agent worktrees get
an uninitialised submodule, and a commit there must not be blocked by a file that
was never checked out.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAIN_TEX = ROOT / "latex" / "main.tex"
SNAPSHOT = ROOT / "docs" / "main-tex-anchors.json"

# Where citations live.  README included; latex/ and build/ deliberately are not.
SEARCH_DIRS = ("docs", "include", "src", "tests")
SEARCH_FILES = ("README.md",)
SUFFIXES = {".md", ".hpp", ".cpp", ".h", ".cc", ".py", ".txt"}

# `main.tex:NNN`, `main.tex:NNN-MMM`, `main.tex:NNN–MMM` (en dash), each
# The `latex/` prefix is optional so that `latex/main.tex:548-553` matches too.
# Deliberately NO inline-quote form: prose routinely quotes main.tex right after
# citing it (`main.tex:135 "known soundness boundary" is a stale misdiagnosis`),
# and reading that as an anchor produced false failures.  The snapshot covers
# every cited line anyway, so a per-site anchor bought nothing.
_CITE = re.compile(
    r"(?P<prefix>(?:latex/)?main\.tex:)"
    r"(?P<start>\d+)"
    r"(?P<dash>[-\u2013])?(?P<end>\d+)?"
)
# \cref{foo} / \cref{foo,bar} in prose -- every target must have a \label.
_CREF = re.compile(r"\\cref\{([^}]+)\}")
_LABEL = re.compile(r"\\label\{([^}]+)\}")

# How many following lines a fingerprint may absorb before we give up on making
# it unique.  Cited lines are occasionally just `\[`, which needs its neighbour.
_MAX_FINGERPRINT_LINES = 4


def _load_lines():
    return MAIN_TEX.read_text(encoding="utf-8").splitlines()


def _norm(s):
    """Whitespace-insensitive form, so reflowing a line does not read as drift."""
    return " ".join(s.split())


def _significant(lines, start):
    """Stripped non-blank lines from `start` (0-based), for fingerprinting."""
    out = []
    for raw in lines[start:]:
        n = _norm(raw)
        if n:
            out.append(n)
        if len(out) >= _MAX_FINGERPRINT_LINES:
            break
    return out


def _find(lines, fingerprint):
    """Every 0-based index where `fingerprint` starts.  Blank lines are skipped
    between its parts, so inserting a blank line inside a display is not drift."""
    hits = []
    for i in range(len(lines)):
        if _norm(lines[i]) != fingerprint[0]:
            continue
        j, matched = i + 1, 1
        while matched < len(fingerprint) and j < len(lines):
            n = _norm(lines[j])
            if n:
                if n != fingerprint[matched]:
                    break
                matched += 1
            j += 1
        if matched == len(fingerprint):
            hits.append(i)
    return hits


def _fingerprint_for(lines, lineno):
    """Shortest unique fingerprint anchored at 1-based `lineno`."""
    parts = _significant(lines, lineno - 1)
    if not parts:
        return None
    for n in range(1, len(parts) + 1):
        if len(_find(lines, parts[:n])) == 1:
            return parts[:n]
    return parts  # ambiguous even at max length; reported by --check


def _iter_files():
    for name in SEARCH_FILES:
        p = ROOT / name
        if p.is_file():
            yield p
    for d in SEARCH_DIRS:
        for p in sorted((ROOT / d).rglob("*")):
            if p.is_file() and p.suffix in SUFFIXES:
                yield p


def _citations():
    """[(path, text, [match, ...])] for every file that cites main.tex."""
    out = []
    for p in _iter_files():
        try:
            text = p.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            continue
        matches = list(_CITE.finditer(text))
        if matches:
            out.append((p, text, matches))
    return out


def _cited_lines(cites):
    return sorted({int(m.group("start")) for _, _, ms in cites for m in ms})


def _rel(p):
    return p.relative_to(ROOT)


def snapshot():
    lines = _load_lines()
    cites = _citations()
    anchors, problems = {}, []
    for lineno in _cited_lines(cites):
        if lineno > len(lines):
            problems.append(f"  main.tex:{lineno} is past end of file ({len(lines)} lines)")
            continue
        fp = _fingerprint_for(lines, lineno)
        if fp is None:
            problems.append(f"  main.tex:{lineno} is blank -- cite a line with content")
            continue
        if len(_find(lines, fp)) != 1:
            problems.append(f"  main.tex:{lineno} is not uniquely identifiable: {fp[0][:60]!r}")
            continue
        anchors[str(lineno)] = fp
    SNAPSHOT.write_text(
        json.dumps(
            {
                "_comment": (
                    "GENERATED by scripts/check-main-tex-refs.py -- do not edit. "
                    "Maps each cited latex/main.tex line to the text it named, so "
                    "an Overleaf pull that shifts lines can be repaired with --fix."
                ),
                "anchors": anchors,
            },
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )
    for msg in problems:
        print(msg, file=sys.stderr)
    print(f"snapshotted {len(anchors)} cited lines -> {_rel(SNAPSHOT)}")
    return 1 if problems else 0


def _load_snapshot():
    if not SNAPSHOT.is_file():
        return None
    return json.loads(SNAPSHOT.read_text(encoding="utf-8")).get("anchors", {})


def _resolve(lines, anchors):
    """-> (moved {old: new}, stale [msg], ok count).  `moved` needs --fix."""
    moved, stale, ok = {}, [], 0
    for key, fp in sorted(anchors.items(), key=lambda kv: int(kv[0])):
        lineno = int(key)
        if 1 <= lineno <= len(lines) and _significant(lines, lineno - 1)[: len(fp)] == fp:
            ok += 1
            continue
        hits = _find(lines, fp)
        if len(hits) == 1:
            moved[lineno] = hits[0] + 1
        elif not hits:
            stale.append(
                f"  main.tex:{lineno} -- content is GONE from main.tex, cannot repair "
                f"automatically: {fp[0][:70]!r}"
            )
        else:
            stale.append(
                f"  main.tex:{lineno} -- content now appears {len(hits)}x, ambiguous: {fp[0][:70]!r}"
            )
    return moved, stale, ok


def _check_crefs(lines):
    labels = {m.group(1) for line in lines for m in _LABEL.finditer(line)}
    bad = []
    for path, text, _ in _citations():
        for m in _CREF.finditer(text):
            for target in (t.strip() for t in m.group(1).split(",")):
                if target and target not in labels:
                    bad.append(f"  {_rel(path)}: \\cref{{{target}}} has no \\label in main.tex")
    return bad


def check():
    lines = _load_lines()
    cites = _citations()
    anchors = _load_snapshot()
    if anchors is None:
        print(f"{_rel(SNAPSHOT)} is missing -- run: python3 scripts/check-main-tex-refs.py --snapshot",
              file=sys.stderr)
        return 1

    failed = False
    unrecorded = [n for n in _cited_lines(cites) if str(n) not in anchors]
    if unrecorded:
        failed = True
        print("New main.tex citations with no recorded anchor:", file=sys.stderr)
        for n in unrecorded:
            print(f"  main.tex:{n}", file=sys.stderr)
        print("  run: python3 scripts/check-main-tex-refs.py --snapshot", file=sys.stderr)

    moved, stale, ok = _resolve(lines, anchors)
    if moved:
        failed = True
        print("main.tex citations have DRIFTED (content moved):", file=sys.stderr)
        for old, new in sorted(moved.items()):
            print(f"  main.tex:{old} -> :{new}", file=sys.stderr)
        print("  run: python3 scripts/check-main-tex-refs.py --fix", file=sys.stderr)
    if stale:
        failed = True
        print("main.tex citations need a HUMAN (content changed, not just moved):", file=sys.stderr)
        for msg in stale:
            print(msg, file=sys.stderr)

    # WARNING, not failure: a \cref with no \label is a content question for a
    # human (the target was renamed or removed upstream), not drift this script
    # can repair -- and it must not block an unrelated commit.
    dangling = _check_crefs(lines)
    if dangling:
        print("warning: \\cref targets with no \\label in main.tex:", file=sys.stderr)
        for msg in sorted(set(dangling)):
            print(msg, file=sys.stderr)

    if not failed:
        print(f"main.tex refs OK ({ok} anchors, {sum(len(m) for _, _, m in cites)} citations)")
    return 1 if failed else 0


def fix():
    lines = _load_lines()
    cites = _citations()
    anchors = _load_snapshot()
    if anchors is None:
        return snapshot()

    moved, stale, _ = _resolve(lines, anchors)
    for msg in stale:
        print(msg, file=sys.stderr)
    if not moved:
        print("nothing to relocate" + ("; stale entries remain (see above)" if stale else ""))
        return snapshot() if not stale else 1

    def repl(m):
        start = int(m.group("start"))
        if start not in moved:
            return m.group(0)
        new = moved[start]
        out = f"{m.group('prefix')}{new}"
        if m.group("end"):  # keep the span, shifted with its start
            out += f"{m.group('dash')}{int(m.group('end')) + new - start}"
        return out

    touched = 0
    for path, text, _ in cites:
        new_text = _CITE.sub(repl, text)
        if new_text != text:
            path.write_text(new_text, encoding="utf-8")
            touched += 1
    for old, new in sorted(moved.items()):
        print(f"  main.tex:{old} -> :{new}")
    print(f"rewrote {len(moved)} anchors across {touched} files")
    return snapshot() if not stale else 1


def main(argv):
    modes = {"--check": check, "--fix": fix, "--snapshot": snapshot}
    mode = argv[1] if len(argv) == 2 else None
    if mode not in modes:
        print(__doc__.strip().splitlines()[0], file=sys.stderr)
        print("usage: check-main-tex-refs.py (--check | --fix | --snapshot)", file=sys.stderr)
        return 2
    if not MAIN_TEX.is_file():
        # Uninitialised submodule (agent worktrees).  Nothing to check against.
        return 0
    return modes[mode]()


if __name__ == "__main__":
    sys.exit(main(sys.argv))
