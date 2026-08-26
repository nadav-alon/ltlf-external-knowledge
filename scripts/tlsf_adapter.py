#!/usr/bin/env python3
"""SYNTCOMP `tlsf-fin` -> (LTLf formula, part-file) ingestion adapter.

Written for the 2026-08-17 reconnaissance day-run (docs/runs/
2026-08-17-syntcomp-fin-recon.md) and kept because the 2026-08-18 sweep is
specified to reuse it.  It is a *corpus* adapter, not a domain concept: nothing
here gets a docs/GLOSSARY.md entry, and nothing in include/ or src/ depends on
it.

## What it does, and the one thing it deliberately does not

Basic TLSF -- `INFO` plus `MAIN { INPUTS OUTPUTS GUARANTEES }` -- is a wrapper
around a single LTLf formula whose only divergence from Spot's syntax is `&&`
and `||`.  Stripping that wrapper covers **1717 of the 1742** `tlsf-fin` files
(everything in `Patterns`, `Random` and `Two-player-Game`).

The remaining 25 (`Scutella` x4, `chomp_game` x21) use real TLSF: `PARAMETERS`,
`DEFINITIONS` macros with `SIZEOF`/indexed conjunction, bit-vector signals
(`s[5]`, `os[N*M]`), and the `PRESET`/`REQUIRE`/`ASSERT`/`GUARANTEE` sections.
Those are **refused**, loudly, rather than approximated -- a silently
mis-expanded macro is a wrong benchmark number, which is worse than a missing
one.  `syfco -f ltlxba-fin -m fully` converts all 25 (verified 2026-08-17, and
it is what Spot's own `ltlfsynt --tlsf=` shells out to), but taking a
dependency on syfco is a decision the recon run does not own.

## Validation

Every one of the 1717 conversions was checked against `syfco -f ltlxba-fin -m
fully` -- an independent implementation that preserves the strong/weak next
distinction (`X[!]` vs `X`) this corpus leans on -- comparing Spot's canonical
rendering of both formulas plus the input/output signal sets: 1717/1717 match.

## Caveat the caller must handle

Both repo CLIs take the formula as `--formula <string>`, and Linux caps a
single argv entry at MAX_ARG_STRLEN = 131072 bytes.  164 `Two-player-Game`
formulas exceed that and cannot reach `ltlf-ek-deps` / `ltlf-ek-synth` at all
(`ltlfsynt` is unaffected -- it reads `-F -`).  `ARG_LIMIT` below is the
measured cutoff; check against it and report such instances separately rather
than as "no dependency found".

Usage:
  tlsf_adapter.py FILE.tlsf                    # JSON on stdout
  tlsf_adapter.py FILE.tlsf --emit-part P      # also write a part-file to P
"""
import argparse
import json
import re
import sys

# Sections that mean "this is not basic TLSF".  GUARANTEE (singular) is in the
# list on purpose: it is the real-TLSF spelling and always co-occurs with the
# macro machinery, while basic files use GUARANTEES.
HARD_SECTIONS = ("GLOBAL", "DEFINITIONS", "PARAMETERS", "PRESET", "REQUIRE",
                 "ASSERT", "ASSUME", "ASSUMPTIONS", "INVARIANTS", "GUARANTEE")

# execve fails at 131077 chars and succeeds at 131069 (measured 2026-08-17 on
# this machine); 131060 leaves room for the `--formula` entry itself.
ARG_LIMIT = 131060


class NeedsTlsf(Exception):
    """The file needs real TLSF machinery; the wrapper-strip refuses it."""


def strip_comments(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def find_block(text, name):
    """Body of `NAME { ... }`, brace-matched, or None if absent."""
    m = re.search(r"\b" + name + r"\s*\{", text)
    if not m:
        return None
    depth, i = 1, m.end()
    while i < len(text) and depth:
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        i += 1
    if depth:
        raise NeedsTlsf("unbalanced braces in " + name)
    return text[m.end():i - 1]


def signals(body):
    out = []
    for stmt in body.split(";"):
        s = stmt.strip()
        if not s:
            continue
        if "[" in s:
            raise NeedsTlsf("bit-vector signal: " + s)
        if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", s):
            raise NeedsTlsf("non-atomic signal declaration: " + s)
        out.append(s)
    return out


def convert(text):
    """Basic-TLSF source -> dict(inputs, outputs, formula, semantics, target).

    Raises NeedsTlsf if the file is outside the basic subset.
    """
    raw, text = text, strip_comments(text)
    for name in HARD_SECTIONS:
        if re.search(r"\b" + name + r"\s*\{", text):
            raise NeedsTlsf("section " + name)
    main = find_block(text, "MAIN")
    if main is None:
        raise NeedsTlsf("no MAIN block")
    ins, outs = find_block(main, "INPUTS"), find_block(main, "OUTPUTS")
    gua = find_block(main, "GUARANTEES")
    if ins is None or outs is None or gua is None:
        raise NeedsTlsf("missing INPUTS/OUTPUTS/GUARANTEES")
    parts = [p.strip() for p in gua.split(";") if p.strip()]
    if not parts:
        raise NeedsTlsf("empty GUARANTEES")
    # Several GUARANTEES entries are conjoined -- that is TLSF's own reading.
    phi = parts[0] if len(parts) == 1 else " & ".join("(%s)" % p for p in parts)
    sem = re.search(r"SEMANTICS:\s*([A-Za-z,]+)", raw)
    tgt = re.search(r"TARGET:\s*([A-Za-z]+)", raw)
    return {
        "inputs": signals(ins),
        "outputs": signals(outs),
        # `&&`/`||` are the ONLY operator spellings TLSF and Spot disagree on
        # here; ->, <->, X, X[!], G, F, U, R, W are already common.
        "formula": " ".join(phi.replace("&&", "&").replace("||", "|").split()),
        "semantics": sem.group(1) if sem else "?",
        "target": tgt.group(1) if tgt else "?",
        "n_guarantees": len(parts),
    }


def part_file_text(c):
    """The repo's four-key part-file (include/ltlf_ek/cli.hpp) with NO knowledge.

    input_free = TLSF INPUTS, output_free = TLSF OUTPUTS, both known sets
    empty -- the partition that asks what the specification carries on its own.
    """
    return ("input_free: %s\ninput_known:\noutput_free: %s\noutput_known:\n"
            % (" ".join(c["inputs"]), " ".join(c["outputs"])))


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    ap.add_argument("--emit-part", metavar="PATH")
    args = ap.parse_args()
    with open(args.file) as fh:
        try:
            c = convert(fh.read())
        except NeedsTlsf as e:
            json.dump({"file": args.file, "ok": False, "reason": str(e)},
                      sys.stdout)
            print()
            return 1
    if args.emit_part:
        with open(args.emit_part, "w") as fh:
            fh.write(part_file_text(c))
    c["file"], c["ok"] = args.file, True
    c["argv_safe"] = len(c["formula"]) <= ARG_LIMIT
    json.dump(c, sys.stdout)
    print()
    return 0


if __name__ == "__main__":
    sys.exit(main())
