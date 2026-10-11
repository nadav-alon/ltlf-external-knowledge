# The monolithic reduction, under totality

Draft for the developer to carry into `latex/main.tex` on Overleaf, after `def:probDefTransducer` (#37 carries that step).
`latex/` is untouched.
The macros are `main.tex`'s own: `\Tin`, `\Tout`, `\Ifree`, `\Iknown`, `\Ofree`, `\Oknown`, `\psiin`, `\psiout`.

**Status.**
The note after `def:probDefTransducer` says "To be proven".
This file proves it under the totality hypothesis below.
Partial transducers are out of scope and are not proved: the divergence witness of the `\cl` note after `def:probDefTransducer` (oracle case O5) stays a pinned boundary, not a counterexample to the theorem.

## Hypotheses and conventions

**(H1) Totality.**
$\delta_{in}$, $\lambda_{in}$, $\delta_{out}$, $\lambda_{out}$ are total on the domains declared in `def:probDefTransducer`.

**(H2) Termination.**
The note after `def:probDef` leaves open who ends the trace.
This file reads both problems with the standard $\text{LTL}_f$ synthesis convention, the one `ltlfsynt` implements: the system chooses when the trace ends, and traces are non-empty.
A *play* is an infinite sequence $w = v_0 v_1 \cdots$ of letters of $2^{\mathcal{I}\cup\mathcal{O}}$, and $w$ *satisfies* a formula $\theta$ if some non-empty finite prefix of $w$ satisfies $\theta$.
A strategy *wins* if every play consistent with it satisfies the objective.
Under H2, `def:probDefTransducer` asks for $T_C$ such that the play that agrees with $\Tin$, $\Tout$, $T_C$ on each infinite $\Ifree$-sequence satisfies $\varphi$.

**(H3) Definability.**
$\psiin$ and $\psiout$ are the languages $L_{in}$, $L_{out}$ defined below.
They are regular but not always $\text{LTL}_f$-definable.
The proof uses only that they are the languages, so it holds verbatim with any regular objective in place of the formula; for an $\text{LTL}_f$ formula, assume $\psiin$ and $\psiout$ define $L_{in}$ and $L_{out}$.

## Setup

For a finite trace $\pi = v_0\cdots v_n$ let $q^{in}_0 = q_{in,0}$, $q^{in}_{t+1} = \delta_{in}(q^{in}_t, v_t)$, and likewise $q^{out}_t$ from $q_{out,0}$ and $\delta_{out}$.
Both are determined by $\pi$ alone.

$$L_{in} = \{\pi \;:\; \forall t\le n.\; v_t\cap\Iknown = \lambda_{in}(q^{in}_t,\, v_t\cap\Ifree)\},$$
$$L_{out} = \{\pi \;:\; \forall t\le n.\; v_t\cap\Oknown = \lambda_{out}(q^{out}_t,\, v_t\cap(\mathcal{I}\cup\Ofree))\}.$$

These are the "traces produced by $\Tin$, $\Tout$", and by the definition of *agreement*, $\pi\in L_{in}$ iff $\pi$ agrees with $\Sin$ at every step, and $\pi\in L_{out}$ iff it agrees with $\Sout$.
Both are prefix-closed.

The plain game $G$ has environment inputs $\mathcal{I}=\Ifree\cup\Iknown$, system outputs $\mathcal{O}=\Ofree\cup\Oknown$, Mealy turn order, and objective
$$\theta := \psiin \rightarrow (\varphi\land\psiout).$$
A plain strategy is $\sigma:(2^{\mathcal{I}\cup\mathcal{O}})^*\times2^{\mathcal{I}}\to2^{\mathcal{O}}$.
The play of $\sigma$ against an input sequence $(i_t)$ has $u_t = i_t\cup\sigma(u_0\cdots u_{t-1}, i_t)$.

**Lemma 1 (uniqueness).**
Under H1, for a strategy $S_C$ and an $\Ifree$-sequence $(f_t)$ there is exactly one play agreeing with $\Tin$, $\Tout$, $S_C$.

*Proof.*
By induction on $t$: given $v_0\cdots v_{t-1}$, the states $q^{in}_t$, $q^{out}_t$ exist by totality of $\delta$.
Agreement forces $v_t\cap\Ifree = f_t$, then $v_t\cap\Iknown = \lambda_{in}(q^{in}_t,f_t)$, then $v_t\cap\Ofree = S_C(v_0\cdots v_{t-1}, v_t\cap\mathcal{I})$, then $v_t\cap\Oknown=\lambda_{out}(q^{out}_t, v_t\cap(\mathcal{I}\cup\Ofree))$.
Each is a value of a total function, so $v_t$ exists and is unique. $\square$

This is the canonical turn order of the Problem Definition section, and it is the only place the Mealy order is used: $\Tout$ sees the system's current $\Ofree$ and the full current $\mathcal{I}$, which is exactly what a plain strategy that picks $\Ofree$ and $\Oknown$ together may compute.

## Theorem

Under H1--H3, `def:probDefTransducer` for $(\varphi,\Tin,\Tout)$ has a solution iff $G$ is realizable.
Moreover, a finite-state solution gives a finite-state plain strategy and conversely.

### Direction ($\Rightarrow$)

Let $T_C$ solve the problem, with strategy $S_C$.
Define
$$\sigma(h, i) = S_C(h,i)\;\cup\;\lambda_{out}\big(q^{out}(h),\, i\cup S_C(h,i)\big),$$
where $q^{out}(h)$ is the $\Tout$ state after $h$; $\sigma$ is defined by H1.

Let $(i_t)$ be any input sequence and $u$ the play of $\sigma$ against it.
By construction every step of $u$ agrees with $\Tout$ and with $S_C$ on $\Ofree$, so $u$ satisfies $\psiout$ at every prefix.
Take the first $t$ with $u_t\cap\Iknown\neq\lambda_{in}(q^{in}_t,u_t\cap\Ifree)$, if any.

- *Such a $t$ exists.* The prefix $u_0\cdots u_t\notin L_{in}$, so it satisfies $\theta$ vacuously.
- *No such $t$ exists.* Every step of $u$ agrees with $\Tin$, so $u$ is the agreeing play of Lemma 1 for $(u_t\cap\Ifree)$.
  Since $S_C$ solves the problem, some prefix $\pi$ satisfies $\varphi$.
  All prefixes lie in $L_{in}\cap L_{out}$, so $\pi\models\psiin\land\varphi\land\psiout$, hence $\pi\models\theta$.

So $u$ satisfies $\theta$ for every input sequence, and $\sigma$ wins.

### Direction ($\Leftarrow$)

Let $\sigma$ win $G$.
Define $S_C(h,i) = \sigma(h,i)\cap\Ofree$.
This is the same transducer as $\sigma$ with its output projected, so finiteness is preserved.

Fix an $\Ifree$-sequence $(f_t)$ and let $w$ be the agreeing play (Lemma 1).
We build an input sequence adaptively against $\sigma$: let $u$ be the play defined by $u_t\cap\Ifree = f_t$, $u_t\cap\Iknown = \lambda_{in}(q^{in}_t, f_t)$ with $q^{in}_t$ computed along $u$ itself, and $u_t\cap\mathcal{O}=\sigma(u_0\cdots u_{t-1},u_t\cap\mathcal{I})$.
This uses H1 twice: $\lambda_{in}$ is defined at the state reached, and $\delta_{in}$ keeps producing states.
(The environment may *adapt*: $\Iknown$ is chosen after the system's earlier $\Oknown$ moves, which is why a fixed copy of $w$'s inputs would not do. $\sigma$ could deviate in $\Oknown$ so that $\delta_{in}$ leaves the states $w$ passes through.)

Every prefix of $u$ is in $L_{in}$ by construction, so $\psiin$ holds at every prefix.
Since $\sigma$ wins, some prefix $\pi=u_0\cdots u_n$ satisfies $\theta$, hence satisfies $\varphi\land\psiout$.

Because $\pi\in L_{out}$, each $u_t$ with $t\le n$ agrees with $\Tout$; $\pi\in L_{in}$ gives agreement with $\Tin$; and $u_t\cap\Ofree = S_C(u_0\cdots u_{t-1},u_t\cap\mathcal{I})$ by definition of $S_C$.
So $\pi$ is a prefix of an agreeing trace for $(f_t)$, and by the uniqueness in Lemma 1, $w_0\cdots w_n=\pi$.
Hence $w$ has a prefix satisfying $\varphi$.
This holds for every $(f_t)$, so $T_C$ solves the problem. $\square$

## Where the asymmetry shows

- $\Tin$ constrains an environment-chosen letter, so a violation hands the system a win (the vacuous branch of $\Rightarrow$, and the adaptive adversary of $\Leftarrow$): it is an assumption.
- $\Tout$ constrains a system-chosen letter, so a violation hands the system nothing: $\varphi\land\psiout$ fails on the violating prefix, and in $\Leftarrow$ the winning prefix is forced to be a faithful one. It is a guarantee.
- In the corpus: the load-bearing flip direction inverts between the two halves (`docs/prd/ltlfsynt-oracle-known-output.md`).

## Out of scope: partial transducers

H1 is used in Lemma 1 (existence of the agreeing play), in $\Rightarrow$ (definition of $\sigma$) and in $\Leftarrow$ (the adaptive environment).
Without it, $L_{in}$ stops being the set of traces on which every step agrees, because a step at a $\delta$-dead state has no letter that agrees; "the language of the produced traces" then has to be chosen, and the choice decides whether the system can step past the boundary.
That is the O5 divergence, and it bottoms out on the termination question the note after `def:probDef` leaves open.
It is not argued here.
