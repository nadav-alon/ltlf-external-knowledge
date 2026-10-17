# Is a knowledge transducer exponentially more succinct than any LTLf/LDLf ψ_in?

Desk research for #146 (origin #145). No build.
Sources are tagged **[fetched]** (read this session), **[snippet]** (seen only as a search-result summary) or **[memory]** (recalled, not re-checked; verify before quoting to the advisor).
Hand-written formulas in §3 were checked by brute force (script not kept) against the intended language on every trace of length ≤ 8 over `{a,k}`, for N ≤ 4.

## Framing that decides most of the answer

ψ_in is a formula over the *whole trace*: the free inputs (`a`) **and** the known inputs the transducer emits (`k`) are both visible letters.
The language of T_in (the *knowledge transducer*, i.e. an external knowledge strategy represented as a `Transducer` `t_in`, per `docs/GLOSSARY.md`) is therefore the DFA over `(a,k)` letters, not the 2-state counter hidden behind it.
Whatever T_in's state *reveals* through `k` becomes a local check in ψ_in.
Only state that `k` does not reveal can force ψ_in to count.
This is why parity, delays and visible-flag counters (§3) are all cheap as formulas.

## 1. Succinctness: DFA vs LDLf / regular expression

**Known DFA → regular-expression lower bounds.**

- Ehrenfeucht and Zeiger (1976), *Complexity measures for regular expressions*, JCSS 12(2): a family of n-state DFAs whose every regular expression has size 2^Ω(n). The witness is a dense, complete-digraph automaton over an alphabet of size n² **[memory]**.
- Gruber and Holzer, *Finite automata, digraph connectivity, and regular expression size* (ICALP 2008) and *Tight bounds on the descriptional complexity of regular expressions* (DLT 2009): the exponential lower bound is strengthened and extended to a **binary** alphabet, which is best possible; the length of any regular expression is at least 2^{Ω(star height)} of its language, a *lower* bound **[snippet]**: <https://www.springerprofessional.de/en/finite-automata-digraph-connectivity-and-regular-expression-size/2759840>. The binary-alphabet exponent was not checked; as recalled it is 2^{Ω(n)} for n-state DFAs over {0,1} **[memory]**.

**Do they need contrived alphabets?**
Ehrenfeucht–Zeiger does, in the sense that the witness is a dense automaton with n² letters.
For us a letter is a valuation of the propositions, so n² letters cost only 2·log n propositions; the alphabet is not the obstacle.
The witness automaton is, because it is a dense permutation-like graph that no natural knowledge source produces.
The binary-alphabet result removes the alphabet objection but the witnesses stay graph-theoretic constructions, not counters or trackers.

**Why this does not settle LDLf.**
LDLf is not plain regular expressions.
It adds tests, conjunction and negation, so `⟨ρ1⟩end ∧ ⟨ρ2⟩end` is regex intersection and `¬⟨ρ⟩end` is complement (De Giacomo and Vardi, IJCAI 2013, <https://www.ijcai.org/Abstract/13/132>; LDLf satisfiability is PSPACE-complete **[snippet]**).
Intersection and complement make expressions more succinct than plain regexes: intersection by a double-exponential factor (Gelade and Neven 2008), complement by a non-elementary one; Fürer 1980 is about the complexity of inequivalence for extended regular expressions rather than succinctness as such **[memory, all three; re-check before quoting]**.
I found no lower bound for DFA → LDLf, and none stating that poly-size LDLf cannot encode every family of N-state DFAs.
So the lower bounds above are an upper limit on what a reviewer can be told, not a result about LDLf.

**Natural families.** Verdicts on the four candidates, sizes in §3:

| family | LTLf ψ_in | LDLf ψ_in | |T_in| |
|---|---|---|---|
| parity of the `a`s so far, `k` visible | O(1) | O(1) | 2 |
| mod-N counter, flag `k` visible | O(N) | O(N) | N |
| `k` = XOR of last N `a`s (delay/window) | O(N) | O(N) | Θ(2^N) |
| `k` = `a` from N steps back | O(N) | O(N) | Θ(2^N) |
| tracker whose state `k` does not reveal | not found | not found | — |

Rows 1–4 give formulas that are polynomial, and in rows 3–4 *exponentially smaller than T_in*, the opposite of the desired direction.
The last row is the only candidate for a positive result, and I could not decide it (see §3, last paragraph).

## 2. Prior work

- LTLf synthesis under environment specifications is studied with the specification given as a **formula** (LTL or LTLf): Aminof, De Giacomo, Di Stasio, Francon, Rubin, Zhu, *LTLf Synthesis Under Environment Specifications for Reachability and Safety Properties*, <https://arxiv.org/abs/2308.15184> **[snippet]**; the PDF text could not be extracted and the abstract does not say how the specification is represented, so the "specified as formulas" reading is unverified. Also Aminof, De Giacomo, Rubin, *Best-Effort Synthesis* (assumptions in LTL/LTLf) **[snippet]**.
- De Giacomo and Favorito, *Compositional approach to translate LTLf/LDLf into DFA* (ICAPS 2021), <https://ojs.aaai.org/index.php/ICAPS/article/view/15954>: LDLf is handled by building the DFA, so the automaton is the working object, not the input **[snippet]**.
- Searched for: a result that exploits DFA-vs-formula succinctness of an *environment assumption* for LTLf/LDLf synthesis. **None found.** Not a proof of absence; the search was two queries plus the tool docs.
- The reverse direction (formulas exponentially more succinct than DFAs) is the classical one and is what the synthesis literature leans on.

## 3. Hand sizes

Variables: free input `a`, known input `k`. `X` is strong next, `X^m` its m-fold nesting, `⊕` is xor, `α_0 = ⊤`.
|ψ| counts temporal and boolean operators; constants are not claimed.

**Window parity** (parity family): `k_t = a_t ⊕ a_{t-1} ⊕ … ⊕ a_{t-N+1}`, with `a_s = 0` for s < 0.

```
α_m   = X⊤ → ( (k ⊕ Xk ↔ Xa) ∧ X α_{m-1} )
ψ_N   = (k ↔ a) ∧ α_{N-1} ∧ G( X^N⊤ → ( X^N k ⊕ X^{N-1} k ↔ X^N a ⊕ a ) )
```

- N = 1: with `α_0 = ⊤`, `ψ_1 = (k ↔ a) ∧ G(X⊤ → (Xk ⊕ k ↔ Xa ⊕ a))`, which collapses to `G(k ↔ a)`.
- N = 2: `ψ_2 = (k ↔ a) ∧ (X⊤ → ((k ⊕ Xk ↔ Xa) ∧ X⊤)) ∧ G(XX⊤ → (XXk ⊕ Xk ↔ XXa ⊕ a))`.
- N = 3: `ψ_3 = (k ↔ a) ∧ α_2 ∧ G(XXX⊤ → (XXXk ⊕ XXk ↔ XXXa ⊕ a))`, with `α_2 = X⊤ → ((k ⊕ Xk ↔ Xa) ∧ X(X⊤ → ((k ⊕ Xk ↔ Xa) ∧ X⊤)))`.

Sizes: |ψ_N| = O(N) in LTLf (an upper bound, not the smallest formula).
LDLf ψ_in, by the linear LTLf → LDLf translation (De Giacomo and Vardi, IJCAI 2013) with `X φ ↦ ⟨true⟩φ` and `G φ ↦ [true*]φ`, so `X^m φ ↦ ⟨true; …; true⟩φ` (m steps):
Written out for N = 2, with `p@m` the atom `p` read after m steps, i.e. `⟨true^m⟩p` under that translation:
```
ψ_2^LDLf = (k ↔ a) ∧ (⟨true⟩tt → ((k ⊕ k@1 ↔ a@1) ∧ ⟨true⟩tt))
         ∧ [true*]( ⟨true;true⟩tt → ( k@2 ⊕ k@1 ↔ a@2 ⊕ a ) )
```
and for N = 3 the same shape with `α_2` as above and the guard `⟨true;true;true⟩tt`, with each `X^m p` read as `p@m`.
These are the translations, not hand-optimised LDLf; I found no LDLf formula for window parity shorter than the LTLf one (a regex-test version would still need the window's `a_{t-N}` bit, which costs a path of length N). The LDLf formulas were not run through a checker, only derived from the checked LTLf ones.
|ψ_N^LDLf| = O(N). Pure parity (window = whole prefix) is `(k ↔ a) ∧ G(X⊤ → (Xk ↔ k ⊕ Xa))`, size O(1).
|T_in| = Θ(2^N): as a Mealy transducer it must remember the whole window `a_{t-N+1..t}`, because `k_{t+1}` reads `a_{t+1-N}`, so 2^N states.
The language over `(a,k)` letters, which is what ψ_in describes, has 2^{N-1} Myhill–Nerode classes once the current `k_t` is known (`k_t` already carries `a_{t-N+1}`, so the future depends only on `a_{t-N+2..t}`); at N = 1, `G(k ↔ a)` has one live state. The warm-up adds fewer than 2^{N-1} more. Both are Θ(2^N).
A smaller LTLf formula for this language cannot be ruled out here, but the language has a Θ(N) one, which is what matters for the kill rule.

**Mod-N counter**: `k_t = 1` iff `#{s ≤ t : a_s} ≡ 0 (mod N)`, N ≥ 2.
`A(φ) = X(¬a U (a ∧ φ))` is "φ at the next `a`"; `A^w(φ) = ¬X(¬a U a) ∨ A(φ)` is its weak form (vacuous when no `a` follows).
`Θ_1 = A^w(k)`, `Θ_j = A^w(¬k ∧ Θ_{j-1})`.

```
ψ_N = (k ↔ ¬a)
    ∧ G( X⊤ → ( ¬Xa → (Xk ↔ k) ) )
    ∧ G( k → Θ_N )
    ∧ ( a → Θ_{N-1} )
```

- N = 2: `ψ_2 = (k ↔ ¬a) ∧ G(X⊤ → (¬Xa → (Xk ↔ k))) ∧ G(k → A^w(¬k ∧ A^w(k))) ∧ (a → A^w(k))`.
- N = 3, written out, with `A^w(φ) = ¬X(¬a U a) ∨ X(¬a U (a ∧ φ))`:
```
ψ_3 = (k ↔ ¬a)
    ∧ G( X⊤ → ( ¬Xa → (Xk ↔ k) ) )
    ∧ G( k → A^w(¬k ∧ A^w(¬k ∧ A^w(k))) )
    ∧ ( a → A^w(¬k ∧ A^w(k)) )
```
- Delay: `k_t = a_{t-N}` for t ≥ N, `k_t = 0` for t < N. With `β_0 = ⊤`, `β_m = ¬k ∧ (X⊤ → X β_{m-1})`:
  `ψ_N^delay = β_N ∧ G( X^N⊤ → (X^N k ↔ a) )`, size O(N); checked by brute force like the others (N ≤ 4, traces of length ≤ 8). N = 1: `¬k ∧ G(X⊤ → (Xk ↔ a))`. The LDLf version is the same translation, O(N).

|ψ_N| = O(N) in LTLf and LDLf (an upper bound; the counter is never hidden: `k` re-anchors it at every flag).
|T_in| = N.
That the mod-N counter language is star-free, once `k` is visible, is witnessed by ψ_N above being an LTLf formula.

**What is not decided.** The families above fail the positive result for one reason: `k` reveals the state.
The remaining candidate is a counter of N bits (period 2^N) with only the top bit visible: `k` toggles exactly every 2^{N-1} `a`-steps. T_in has 2^N states.
- Plain regexes over `{a}` need 2^{N-1} copies of `a`, but LDLf conjunction can take the intersection of mod-p_i constraints, so `lcm`-style periods are poly-size in LDLf (a *reverse* succinctness gap: formula ≪ DFA). Exactly 2^N is not of that form.
- For one visible variable, LTL is still PSPACE-hard (Demri and Schnoebelen, *The complexity of propositional linear temporal logics in simple cases*, Inf. Comput. 2002 **[memory]**), so one cannot conclude that exponential-period behavior needs exponential-size formulas, nor the contrary.
I found no source and no argument for either size. This is the single open point; the verdict below does not depend on it, but tier 2 would.

## 4. Automaton assumptions in LydiaSyft and others

LydiaSyft (<https://github.com/whitemech/LydiaSyft>, README **[fetched]**):

- `-f` takes the goal as an LTLf formula in TLSF.
- Environment assumptions come as `-a` (fairness/stability assumption file), `-g` (GR(1) conditions) and `-e` (an LTLf safety specification), selected by the subcommands `fairness`, `stability`, `gr1`.
- The README mentions **no LDLf input and no direct automaton input**. Not seen, therefore not claimed absent: the source was not read, and the TLSF/`-e` files could embed forms the README does not describe.

Other tools: Spot's `ltlfsynt` 2.15.1 (the tool this repo wraps): its `--help` lists formula (`-f`, `-F`, `--lbt-input`) and TLSF (`--tlsf`) inputs only, with no option for an environment assumption, let alone an automaton **[fetched]**. No tool with a documented "assumption given as a DFA/transducer" option was found. The Lisa and Zenodo hits in the search (<https://github.com/liyong31/lisa>) were not opened.
Unchecked lead: LTLf/LDLf FOND planning (De Giacomo and Rubin, IJCAI 2018, **[memory]**) gives the environment as a nondeterministic domain, a transition system close to an automaton-valued assumption. I did not check whether any such tool accepts it as an environment assumption for synthesis; if it does, Q4 could flip.

Question 4 is therefore **no, as far as the documentation I read shows**; the "any other synthesizer" half (Lisa, FOND tools, LydiaSyft's source) is not covered.

## Verdict

| kill rule | status |
|---|---|
| Blowup is known **and** already exploited for synthesis | **Not fired.** The DFA → regex blowup is known (§1); no exploitation for LTLf/LDLf synthesis was found (§2), with the caveat that the search was shallow. The blowup is also not known for LDLf. |
| No natural family shows it; only contrived alphabets | **Undecided.** The families whose state `k` exposes (parity, mod-N counter, delay, window) have O(N) or O(1) formulas, and the known witnesses are dense-graph constructions (§1). The ticket's fourth candidate, a tracker of hidden state, is the hidden-top-bit counter (§3), for which I have neither a source nor an argument, so "no natural family shows it" is not established. |
| LDLf ψ_in for both parity and counter stays polynomial | **Fires, for the visible-flag variants only.** Parity: O(1). Counter: O(N). Both checked on small N (§3). Both families reveal their state through `k` (§3 says that is why they are cheap); whether a parity or counter family with its state hidden behind `k` stays polynomial is open. |
| Q4 yes | **Not fired.** LydiaSyft documents no automaton-valued assumption (§4). |

One of four fires outright (rule 3, for the visible-flag families), rule 2 is undecided, and rules 1 and 4 do not fire. The **thesis-only** recommendation therefore rests on rule 3 alone, and is weaker than a two-rule verdict would be: no family with a visible state beats folding the knowledge into the goal formula φ on formula size, which agrees with the sweep (nk-product ratio ≈ 1, `docs/runs/2026-10-16-four-family-sweep.md`).

**If the advisor wants tier 2 anyway**, the ticket's instance is a LydiaSyft smoke test on the parity family. Run it as specified; I also recommend, for the advisor to decide, a second instance on the hidden-top-bit counter. Parity itself cannot show a win: ψ_in is O(1) there, so folding it in is free. The hidden-top-bit counter is the only open family. Measure what formula size cannot show:

- compare DFA states and wall time of `ψ_in → φ` (determinization of the folded goal formula) against the goal automaton crossed with T_in, per N. A transducer win, if any, is in construction cost, not in |ψ_in|;
- report |ψ_in| next to |T_in| so the formula-size claim is checked in the same run.
