#!/usr/bin/env python3
"""Write the generated hard instances of the output-dependency race.

    python3 scripts/hard_instances.py [--out DIR] [--check]

Mealy LTLf specs read with weak X (the `syfco -f ltlxba` rendering, Spot LTLf
semantics), written from their SYNTCOMP TLSF definitions rather than through
syfco:

  mux    n = 16..64   out <-> in[i] under the binary-encoded select bus. `out` is
                      determined by the spec only at powers of 2; elsewhere the select
                      values >= n leave it free, and the compact Tout sets it false there.
  shift  n = 20..40   out[i+1] <-> in[i], out[0] <-> in[n-1].

Per instance NAME the directory holds NAME.ltlf (one formula), NAME.part (the
knowledge-free part file) and, for mux and shift, NAME.tout
(the hand-written compact Tout: one state, lambda one boolean formula).
TwoCountersDisButAC (SYNTCOMP tlsf_paper, commit 13336b3c) is committed in the
same directory as a copy, not generated.

--check rewrites nothing and exits 1 if a committed file differs.
"""
import argparse
import os
import sys

MUX_N = range(16, 65)
SHIFT_N = range(20, 41)
DEFAULT_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tests", "data", "hard-instances")


def nbits(n):
    """Bits to encode n values: syfco's `nbits`, with nbits(1) = 1."""
    return max(1, (n - 1).bit_length())


def _value(bus, v, width):
    """`bus` currently reads v in binary, least significant bit first."""
    return " & ".join(("%s%d" if (v >> i) & 1 else "!%s%d") % (bus, i) for i in range(width))


def _selects(n):
    """(the select bus reads i, i) for each input i of an n-way mux."""
    w = nbits(n)
    return [(_value("select", i, w), i) for i in range(n)]


def mux(n):
    w = nbits(n)
    body = " & ".join("((%s) -> (out <-> in%d))" % (sel, i) for sel, i in _selects(n))
    return {
        "name": "mux-%d" % n,
        "formula": "G(%s)" % body,
        "inputs": ["select%d" % i for i in range(w)] + ["in%d" % i for i in range(n)],
        "outputs": ["out"],
        "tout": "out <-> (%s)" % " | ".join("(%s & in%d)" % (sel, i) for sel, i in _selects(n)),
    }


def shift(n):
    eqs = ["(in%d <-> out%d)" % (i, i + 1) for i in range(n - 1)] + ["(in%d <-> out0)" % (n - 1)]
    return {
        "name": "shift-%d" % n,
        "formula": "G(%s)" % " & ".join(eqs),
        "inputs": ["in%d" % i for i in range(n)],
        "outputs": ["out%d" % i for i in range(n)],
        "tout": " & ".join(eqs),
    }


def part_text(inputs, outputs, known=()):
    free = [o for o in outputs if o not in known]
    return "input_free: %s\ninput_known:\noutput_free: %s\noutput_known: %s\n" % (
        " ".join(inputs), " ".join(free), " ".join(known))


def tout_text(inst):
    """One state, delta a [t] self-loop, lambda one formula (the one-formula %%LAMBDA form)."""
    aps = inst["inputs"] + inst["outputs"]
    return ("HOA: v1\nStates: 1\nStart: 0\nAP: %d %s\nacc-name: all\nAcceptance: 0 t\n--BODY--\n"
            "State: 0\n[t] 0\n--END--\n%%%%LAMBDA\nstate 0: %s\n"
            % (len(aps), " ".join('"%s"' % a for a in aps), inst["tout"]))


def write(dirname, inst):
    """Write inst's files into dirname; the path prefix of the instance (without extension)."""
    for name, text in files(inst).items():
        with open(os.path.join(dirname, name), "w") as f:
            f.write(text)
    return os.path.join(dirname, inst["name"])


def files(inst):
    out = {inst["name"] + ".ltlf": inst["formula"] + "\n",
           inst["name"] + ".part": part_text(inst["inputs"], inst["outputs"])}
    if inst["tout"]:
        out[inst["name"] + ".tout"] = tout_text(inst)
    return out


def all_instances():
    return [mux(n) for n in MUX_N] + [shift(n) for n in SHIFT_N]


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args(argv)
    stale = []
    for inst in all_instances():
        for name, text in files(inst).items():
            path = os.path.join(args.out, name)
            if args.check:
                if not os.path.exists(path):
                    stale.append(name)
                else:
                    with open(path) as f:
                        if f.read() != text:
                            stale.append(name)
            else:
                os.makedirs(args.out, exist_ok=True)
                with open(path, "w") as f:
                    f.write(text)
    if stale:
        print("stale: " + " ".join(stale), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
