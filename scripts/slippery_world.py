#!/usr/bin/env python3
"""Generate the slippery-world fixtures (docs/plans/2026-08-17-week.md, Wed).

An N x N grid.  Position is an INPUT (input_known), `slip` is the free input
(input_free), `mvl/mvr/mvu/mvd` are the outputs (output_free).  A move advances
1 cell, or 2 when `slip` holds; walls are no-ops (bumping = stay); delta is
total over all 16 mv-combinations via the fixed priority l > r > u > d (no
direction = stay).  Both defaults exist to keep A_rest = top: nothing here
restricts an OUTPUT, so the whole environment assumption can live in T_in.

Two encodings of the position, per Q5:
  binary --- 2*ceil(log2 N) APs  (bx0.., by0..), the headline;
  onehot --- 2*N APs             (hx0.., hy0..), the matched control.

Emits, per (N, encoding), into tests/fixtures/slippery-world/:
  <stem>.part          the four-key part file
  <stem>.tin           T_in in the transducer file format (HOA + %%LAMBDA)
  <stem>.A.ltlf        A_N, the environment assumption as an LTLf formula
  <stem>.corner.ltlf   gamma: reach (N-1, N-1)
  <stem>.centre.ltlf   gamma: reach (c, c), c = (N-1)//2
  <stem>.ins           comma-separated input APs   (for ltlfsynt --ins)
  <stem>.outs          comma-separated output APs  (for ltlfsynt --outs)

A_N uses WEAK X for the transition rules: with X[!] the rule would be violated
at the last position of every trace (the guards are total), collapsing A to
false.  Weak X is vacuously true at the end, which is exactly the "delta is
undefined past the end" of the transducer side.
"""
import math
import os
import sys

MOVES = ["mvl", "mvr", "mvu", "mvd"]
SLIP = "slip"

# The five priority classes of a mv-letter, as (name, literal-guard) pairs.
# Exactly one holds for any of the 16 valuations, so they partition 2^{mv}.
CLASSES = [
    ("L", ["mvl"]),
    ("R", ["!mvl", "mvr"]),
    ("U", ["!mvl", "!mvr", "mvu"]),
    ("D", ["!mvl", "!mvr", "!mvu", "mvd"]),
    ("S", ["!mvl", "!mvr", "!mvu", "!mvd"]),
]


def step(x, y, cls, slip, n):
    """The transition table: one cell, one priority class, one slip bit."""
    d = 2 if slip else 1
    if cls == "L":
        return max(x - d, 0), y
    if cls == "R":
        return min(x + d, n - 1), y
    if cls == "U":
        return x, max(y - d, 0)
    if cls == "D":
        return x, min(y + d, n - 1)
    return x, y


class Enc:
    """Position APs and the `coordinate == k` predicate, per encoding."""

    def __init__(self, kind, n):
        self.kind, self.n = kind, n
        if kind == "binary":
            self.bits = max(1, math.ceil(math.log2(n)))
            self.aps = (["bx%d" % i for i in range(self.bits)] +
                        ["by%d" % i for i in range(self.bits)])
        else:
            self.aps = (["hx%d" % i for i in range(n)] +
                        ["hy%d" % i for i in range(n)])

    def lits(self, axis, k):
        """`coordinate `axis` == k` as a list of literals (a conjunction)."""
        if self.kind == "binary":
            return [("" if (k >> i) & 1 else "!") + "b%s%d" % (axis, i)
                    for i in range(self.bits)]
        return [("" if j == k else "!") + "h%s%d" % (axis, j)
                for j in range(self.n)]

    def at(self, axis, k):
        return " & ".join(self.lits(axis, k))

    def cell(self, x, y):
        return "(%s) & (%s)" % (self.at("x", x), self.at("y", y))


def hoa(enc, n):
    """T_in as HOA(delta) + %%LAMBDA(lambda), one state per cell.

    State (x, y) is numbered x * n + y.  lambda emits the cell's position
    literals and does not read Sigma0 = {slip} at all -- position at t is a
    function of the run's state alone, i.e. of the history strictly before t
    (the Moore check under \\cref{def:indep}).
    """
    aps = enc.aps + [SLIP] + MOVES
    idx = {a: i for i, a in enumerate(aps)}

    def g(lits):
        return "&".join(("!" if s.startswith("!") else "") +
                        str(idx[s.lstrip("!")]) for s in lits)

    out = ["HOA: v1", "States: %d" % (n * n), "Start: 0",
           "AP: %d %s" % (len(aps), " ".join('"%s"' % a for a in aps)),
           "acc-name: all", "Acceptance: 0 t",
           "properties: trans-labels explicit-labels state-acc complete",
           "properties: deterministic", "--BODY--"]
    for x in range(n):
        for y in range(n):
            out.append("State: %d" % (x * n + y))
            by_dst = {}
            for cname, clits in CLASSES:
                for s in (0, 1):
                    dx, dy = step(x, y, cname, s, n)
                    guard = g(clits + ([SLIP] if s else ["!" + SLIP]))
                    by_dst.setdefault(dx * n + dy, []).append(guard)
            for dst in sorted(by_dst):
                out.append("[%s] %d" % (" | ".join(by_dst[dst]), dst))
    out += ["--END--", "%%LAMBDA"]
    for x in range(n):
        for y in range(n):
            out.append("state %d: %s" % (x * n + y, enc.cell(x, y)))
    return "\n".join(out) + "\n"


def assumption(enc, n):
    """A_N: the same table as an LTLf formula, factored per coordinate.

    x moves iff the class is L or R, y iff it is U or D, so the two
    coordinates' rules are independent -- 5N implications each rather than
    16 N^2.
    """
    conj = ["(%s)" % enc.cell(0, 0)]
    for axis, movers in (("x", ("L", "R")), ("y", ("U", "D"))):
        for k in range(n):
            here = enc.at(axis, k)
            for cname, clits in CLASSES:
                slips = (0, 1) if cname in movers else (0,)
                for s in slips:
                    px, py = (k, 0) if axis == "x" else (0, k)
                    nx, ny = step(px, py, cname, s, n)
                    dst = nx if axis == "x" else ny
                    guard = " & ".join(clits)
                    if cname in movers:
                        guard += " & " + (SLIP if s else "!" + SLIP)
                    conj.append("G(((%s) & (%s)) -> X(%s))"
                                % (here, guard, enc.at(axis, dst)))
    return " & ".join(conj)


def main():
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..",
                        "tests", "fixtures", "slippery-world")
    root = os.path.normpath(root)
    os.makedirs(root, exist_ok=True)
    for n in (2, 3, 4):
        for kind in ("binary", "onehot"):
            enc = Enc(kind, n)
            stem = os.path.join(root, "n%d-%s" % (n, kind))
            w = lambda ext, txt: open(stem + ext, "w").write(txt)
            w(".part", "input_free: %s\ninput_known: %s\noutput_free: %s\n"
                       "output_known:\n" % (SLIP, " ".join(enc.aps),
                                            " ".join(MOVES)))
            w(".tin", hoa(enc, n))
            w(".A.ltlf", assumption(enc, n) + "\n")
            w(".corner.ltlf", "F(%s)\n" % enc.cell(n - 1, n - 1))
            c = (n - 1) // 2
            w(".centre.ltlf", "F(%s)\n" % enc.cell(c, c))
            w(".ins", ",".join([SLIP] + enc.aps) + "\n")
            w(".outs", ",".join(MOVES) + "\n")
            print("wrote %s.* (%d states, %d position APs)"
                  % (os.path.basename(stem), n * n, len(enc.aps)))


if __name__ == "__main__":
    sys.exit(main())
