#!/usr/bin/env python3
"""Encode a FOND/PDDL domain + problem as an external-knowledge T_in (docs/research/2026-11-21-fond-pddl-ltlf-goals.md).

The domain is the knowledge, the goal is phi:

  free input     ev           nature's choice among a `oneof`'s two branches
  known inputs   the fluents the goal mentions, emitted by T_in each step
  free outputs   a0..a{k-1}   the agent's action, binary-coded; 0 = no-op,
                              a code past the last ground action = no-op

One T_in state per reachable PDDL state, found by exhaustive search from
the problem's init.  An action whose precondition fails is a no-op.  The
state's lambda is the observed fluents' valuation and reads nothing of the
current letter, so the Moore condition holds, as in scripts/slippery_world.py.

Supported PDDL: :typing with a type hierarchy, and / not / forall
preconditions, effects built of and / not / atoms with at most one top-level
`oneof` of two branches.  Anything else raises.

Subcommands:
  encode DOMAIN PROBLEM STEM GOAL   write STEM.{part,tin,goal.ltlf} and print the state count
  measure BINARY DOMAIN PROBLEM GOAL
                                    print one JSON line: product states with and without T_in
"""
import itertools
from collections import deque, namedtuple
import json
import math
import os
import pathlib
import re
import subprocess
import sys
import tempfile

MAX_STATES = 20000
VERDICT = {0: "realizable", 20: "unrealizable"}


# ---------------------------------------------------------------- s-expressions

def sexp(text):
    text = re.sub(r";[^\n]*", "", text)
    toks = re.findall(r"\(|\)|[^\s()]+", text.lower())
    pos = 0

    def parse():
        nonlocal pos
        tok = toks[pos]
        pos += 1
        if tok != "(":
            return tok
        out = []
        while toks[pos] != ")":
            out.append(parse())
        pos += 1
        return out

    return parse()


def typed_list(items):
    """['a','b','-','t','c'] -> [('a','t'),('b','t'),('c','object')]."""
    out, pending, i = [], [], 0
    while i < len(items):
        if items[i] == "-":
            out += [(n, items[i + 1]) for n in pending]
            pending, i = [], i + 2
        else:
            pending.append(items[i])
            i += 1
    return out + [(n, "object") for n in pending]


def section(tree, key):
    return [x for x in tree if isinstance(x, list) and x and x[0] == key]


# ---------------------------------------------------------------- the model

class Model:
    def __init__(self, domain_text, problem_text):
        dom, prob = sexp(domain_text), sexp(problem_text)
        self.parent = {}
        for sec in section(dom, ":types"):
            for n, t in typed_list(sec[1:]):
                self.parent[n] = t
        objs = typed_list(section(prob, ":objects")[0][1:])
        self.objects = {}
        for o, t in objs:
            while True:
                self.objects.setdefault(t, []).append(o)
                if t not in self.parent:
                    break
                t = self.parent[t]
        self.actions = []
        for a in section(dom, ":action"):
            self.actions.append(self._action(a))
        init = section(prob, ":init")[0][1:]
        self.init = {tuple(f) for f in init if f[0] != "not"}
        # a goal is given on the command line; the problem's own is ignored.
        written = set()
        for _, _, _, eff in self.actions:
            self._atoms_of(eff, written)
        self.dynamic_preds = {a[0] for a in written}
        self.ground = [g for a in self.actions for g in self._ground(a)]
        self._branches = [self._outcomes(eff) for _, _, eff in self.ground]

    def _action(self, a):
        kv = {a[i]: a[i + 1] for i in range(2, len(a) - 1, 2)}
        params = typed_list(kv.get(":parameters", []))
        return (a[1], params, kv.get(":precondition", ["and"]), kv[":effect"])

    def _atoms_of(self, e, acc):
        if e[0] in ("and", "oneof"):
            for x in e[1:]:
                self._atoms_of(x, acc)
        elif e[0] == "not":
            self._atoms_of(e[1], acc)
        else:
            acc.add(tuple(e))

    def _sub(self, e, env):
        if isinstance(e, str):
            return env.get(e, e)
        return [self._sub(x, env) for x in e]

    def _eval(self, p, state):
        if p[0] == "and":
            return all(self._eval(x, state) for x in p[1:])
        if p[0] == "not":
            return not self._eval(p[1], state)
        if p[0] == "forall":
            (v, t), = typed_list(p[1])
            return all(self._eval(self._sub(p[2], {v: o}), state)
                       for o in self.objects.get(t, []))
        if p[0] not in self.dynamic_preds:
            return tuple(p) in self.init
        return tuple(p) in state

    def _ground(self, action):
        name, params, pre, eff = action
        doms = [self.objects.get(t, []) for _, t in params]
        out = []
        for combo in itertools.product(*doms):
            env = {v: o for (v, _), o in zip(params, combo)}
            # static preconditions are checked now, against init, to keep
            # the ground set small; the rest are checked per state.
            if not self._static_ok(self._sub(pre, env)):
                continue
            out.append((name + "_" + "_".join(combo), self._sub(pre, env),
                        self._sub(eff, env)))
        return out

    def _static_ok(self, p):
        """False iff a static atom in p already contradicts init."""
        if p[0] == "and":
            return all(self._static_ok(x) for x in p[1:])
        if p[0] == "forall":
            return True
        if p[0] == "not":
            q = p[1]
            if q[0] in ("and", "not", "forall") or q[0] in self.dynamic_preds:
                return True
            return tuple(q) not in self.init
        return p[0] in self.dynamic_preds or tuple(p) in self.init

    def _outcomes(self, eff):
        """The effect as a list of branches, each (adds, dels); one branch means no oneof."""
        branches = [([], [])]
        for e in (eff[1:] if eff[0] == "and" else [eff]):
            if e[0] == "oneof":
                if len(e) != 3 or len(branches) != 1:
                    raise ValueError("only one top-level oneof of two branches")
                alts = [self._flat(b) for b in e[1:]]
                branches = [([*branches[0][0], *a], [*branches[0][1], *d])
                            for a, d in alts]
            else:
                a, d = self._flat(e)
                branches = [([*x, *a], [*y, *d]) for x, y in branches]
        return branches

    def _flat(self, e):
        adds, dels = [], []
        for x in (e[1:] if e[0] == "and" else [e]):
            if x[0] == "not":
                dels.append(tuple(x[1]))
            elif x[0] == "and":
                a, d = self._flat(x)
                adds += a
                dels += d
            else:
                adds.append(tuple(x))
        return adds, dels

    def step(self, state, action, nat):
        """action: index into self.ground, or None for the no-op."""
        if action is None:
            return state
        _, pre, eff = self.ground[action]
        if not self._eval(pre, state):
            return state
        branches = self._branches[action]
        adds, dels = branches[min(nat, len(branches) - 1)]
        return frozenset((state - set(dels)) | set(adds))

    def reachable(self):
        start = frozenset(a for a in self.init if a[0] in self.dynamic_preds)
        order, index, todo = [start], {start: 0}, deque([start])
        while todo:
            s = todo.popleft()
            for act in [None, *range(len(self.ground))]:
                for nat in (0, 1):
                    t = self.step(s, act, nat)
                    if t not in index:
                        if len(index) >= MAX_STATES:
                            raise RuntimeError("more than %d reachable states" % MAX_STATES)
                        index[t] = len(order)
                        order.append(t)
                        todo.append(t)
        return order, index


# ---------------------------------------------------------------- encoding

def ap(atom):
    return "_".join(atom).replace("-", "_")


def observed_atoms(states, goal):
    names = {ap(a): a for s in states for a in s}
    return sorted(n for n in names if re.search(r"\b%s\b" % re.escape(n), goal)), names


def quotient(labels, trans):
    """Coarsest partition refining `labels` that respects `trans` (Moore bisimulation).

    labels[s] is the state's observed valuation, trans[s] a tuple of successors,
    one per letter.  Returns (block of each state, block count), the start
    state's block numbered 0.
    """
    block = [0] * len(labels)
    ids = {}
    for s, lab in enumerate(labels):
        block[s] = ids.setdefault(lab, len(ids))
    while True:
        sigs = {}
        new = [sigs.setdefault((block[s], tuple(block[t] for t in trans[s])), len(sigs))
               for s in range(len(labels))]
        if len(sigs) == len(ids):
            break
        block, ids = new, sigs
    renum = {}
    renum[block[0]] = 0
    for b in block:
        renum.setdefault(b, len(renum))
    return [renum[b] for b in block], len(renum)


Encoded = namedtuple("Encoded", "part hoa inputs outputs t_in_states ground_actions")


def encode(domain_text, problem_text, goal, moore_quotient=False):
    """Return an Encoded for the case.

    With `moore_quotient`, states that no goal fluent can tell apart are merged.
    """
    m = Model(domain_text, problem_text)
    order, index = m.reachable()
    obs, names = observed_atoms(order, goal)
    missing = set(re.findall(r"[a-z][a-z0-9_]*", goal)) - set(names) - {"f", "g", "x", "u", "r", "w", "t", "true", "false"}
    if missing:
        raise ValueError("goal mentions unknown fluents: %s" % sorted(missing))
    k = max(1, math.ceil(math.log2(len(m.ground) + 1)))
    acts = ["a%d" % i for i in range(k)]
    aps = obs + ["ev"] + acts
    idx = {a: i for i, a in enumerate(aps)}
    letters = [(c, n) for c in range(2 ** k) for n in (0, 1)]

    def code_lits(c):
        return ["%s%d" % ("" if (c >> i) & 1 else "!", idx[acts[i]]) for i in range(k)]

    trans = [tuple(index[m.step(s, c - 1 if 1 <= c <= len(m.ground) else None, n)]
                   for c, n in letters) for s in order]
    labels = [tuple(names[o] in s for o in obs) for s in order]
    block, n_blocks = quotient(labels, trans) if moore_quotient else (list(range(len(order))), len(order))
    rep = {}
    for s, b in enumerate(block):
        rep.setdefault(b, s)

    out = ["HOA: v1", "States: %d" % n_blocks, "Start: 0",
           "AP: %d %s" % (len(aps), " ".join('"%s"' % a for a in aps)),
           "acc-name: all", "Acceptance: 0 t",
           "properties: trans-labels explicit-labels state-acc complete",
           "properties: deterministic", "--BODY--"]
    for b in range(n_blocks):
        out.append("State: %d" % b)
        by_dst = {}
        s = rep[b]
        for (c, n), t in zip(letters, trans[s]):
            by_dst.setdefault(block[t], {})[(c, n)] = True
        for d, ls in by_dst.items():
            by_dst[d] = []
            for c in range(2 ** k):
                got = [(c, n) in ls for n in (0, 1)]
                if all(got):
                    by_dst[d].append("&".join(code_lits(c)))
                else:
                    for n in (0, 1):
                        if got[n]:
                            by_dst[d].append("&".join(code_lits(c) + [("" if n else "!") + str(idx["ev"])]))
        base = max(by_dst, key=lambda d: len(by_dst[d]))
        for d in sorted(by_dst):
            if d == base:
                others = [g for e, gs in by_dst.items() if e != d for g in gs]
                label = "!(%s)" % " | ".join(others) if others else "t"
            else:
                label = " | ".join(by_dst[d])
            out.append("[%s] %d" % (label, d))
    out += ["--END--", "%%LAMBDA"]
    for b in range(n_blocks):
        lits = [("" if names[o] in order[rep[b]] else "!") + o for o in obs] or ["true"]
        out.append("state %d: %s" % (b, " & ".join("(%s)" % x for x in lits)))
    part = "input_free: ev\ninput_known: %s\noutput_free: %s\noutput_known:\n" % (
        " ".join(obs), " ".join(acts))
    return Encoded(part, "\n".join(out) + "\n", ",".join(["ev"] + obs), ",".join(acts), n_blocks, len(m.ground))


# ---------------------------------------------------------------- measuring

def product_states(binary, args, env):
    with tempfile.NamedTemporaryFile("r", suffix=".json", dir=env["TMPDIR"]) as f:
        r = subprocess.run([binary, "--dfa-product", *args, "--benchmark=" + f.name],
                           capture_output=True, text=True, env=env)
        text = f.read()
    metrics = {x["label"]: x["value"] for x in json.loads(text)["metrics"]} if text.strip() else {}
    if "product_states" not in metrics:
        raise RuntimeError("%s reported no product_states (rc=%d): %s" % (binary, r.returncode, r.stderr.strip()))
    return r.returncode, metrics["product_states"]


def measure(binary, domain_text, problem_text, goal, workdir):
    env = dict(os.environ, TMPDIR=workdir)
    row = {}
    for tag, mini in (("", False), ("_moore", True)):
        enc = encode(domain_text, problem_text, goal, mini)
        part, hoa, ins, outs, n_states, n_actions = enc
        stem = os.path.join(workdir, "case")
        pathlib.Path(stem + ".part").write_text(part)
        pathlib.Path(stem + ".tin").write_text(hoa)
        rc, n = product_states(binary, [
            "--formula", goal, "--part-file", stem + ".part",
            "--known-input-transducer", stem + ".tin"], env)
        row["t_in_states" + tag] = n_states
        row["product_with" + tag] = n
        row["verdict_with" + tag] = VERDICT.get(rc, rc)
    rc, without = product_states(binary, [
        "--formula", goal, "--inputs", ins, "--outputs", outs], env)
    row.update(ground_actions=n_actions, product_without=without,
               verdict_without=VERDICT.get(rc, rc))
    for tag in ("", "_moore"):
        row["prune_ratio" + tag] = round(row["product_with" + tag] / without, 2)
    return row


def main(argv):
    if len(argv) == 6 and argv[1] == "encode":
        _, _, dom, prob, stem, goal = argv
        enc = encode(pathlib.Path(dom).read_text(), pathlib.Path(prob).read_text(), goal)
        pathlib.Path(stem + ".part").write_text(enc.part)
        pathlib.Path(stem + ".tin").write_text(enc.hoa)
        pathlib.Path(stem + ".goal.ltlf").write_text(goal + "\n")
        print("%d T_in states" % enc.t_in_states)
        return 0
    if len(argv) == 6 and argv[1] == "measure":
        _, _, binary, dom, prob, goal = argv
        with tempfile.TemporaryDirectory(dir=os.environ.get("TMPDIR")) as w:
            print(json.dumps(measure(binary, pathlib.Path(dom).read_text(), pathlib.Path(prob).read_text(), goal, w)))
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
