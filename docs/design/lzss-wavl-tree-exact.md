# LZSS WAVL Tree Exact feasibility contract

## 1. Decision boundary

This document authorizes design of a private `WAVL Tree Exact` match finder
for deletion-heavy LZSS experiments. It does not yet authorize mutation code,
public exposure, or an ordinary Silesia matrix.

The candidate addresses one narrow hypothesis: weak-AVL rank repair may reduce
sliding-window retirement work while preserving logarithmic query depth and
AVL-sized storage. It is not expected to improve the current public
`frame_size == window_size` profiles, because those frames reset before an
ordinary within-frame retirement occurs and insertion-only WAVL has AVL
behavior under the referenced rank-balanced-tree result.

## 2. Existing evidence

The completed Red-Black comparison showed no aggregate advantage over AVL at
equal workspace. The completed Scapegoat comparison produced only 55.90% to
60.66% of AVL throughput, used 1.241379 times AVL workspace, and increased
maximum single-update structural work from 197,017 to 872,473 nodes as the
window grew. These results reject relaxed height or amortized rebuilding as a
general replacement for the current AVL finder.

The repository now has a deterministic deletion-heavy synthetic input with
`frame_size > window_size`, physical retirement checks, process isolation,
Exact token fingerprints, and independent AVL, Red-Black, and Scapegoat
oracles. That infrastructure partially satisfies the earlier WAVL deferral
condition. It supports a private deletion experiment, not a public workload
claim.

## 3. Shared Exact behavior

WAVL MUST retain the existing ordered-tree Exact contract:

- unsigned finite-suffix lexicographic ordering capped at maximum match length;
- shorter remaining suffix first, then increasing absolute position;
- active positions exactly in `[max(0, p - window_size), p)`;
- longest match followed by nearest-distance tie breaking;
- predecessor/successor LCP search plus prefix-interval maximum position;
- fixed caller-owned slots selected by absolute position modulo capacity;
- structural successor transplant for two-child deletion, never payload swap;
- deterministic output identical to Exhaustive, HashChain Exact, and AVL;
- no recursion, steady-state allocation, native pointers, or floating point.

The candidate is encoder-local. It cannot change token bytes, archives,
algorithm or variant IDs, entropy coding, decoder behavior, the C ABI, CLI
codec selection, profiles, initializer defaults, or interoperability schemas.

## 4. Proposed bounded representation

Use the AVL six-array layout: left child, right child, parent, one-byte balance
metadata, absolute position, and subtree maximum position. Replace the AVL
height byte with a one-byte WAVL rank. The null child has mathematical rank
`-1`; inactive slots use the reserved stored value `255`; active ranks are
nonnegative and must remain below that sentinel.

For every active parent-child edge, the rank difference MUST be exactly one or
two. Every settled leaf is a rank-zero `1,1` node; a rank-one `2,2` leaf is
permitted only as the transient deletion violation immediately before repair.
Settled `2,2` nodes are non-leaves. The root has no special rank beyond the
same invariant. Checked workspace arithmetic,
alignment, overlap rejection, equality/one-short limits, platform-width
limits, initialization, and aggregate accounting must match the existing AVL
contract. The intended workspace is byte-for-byte equal in extent to AVL for
the same input and window; any exception stops the equal-workspace hypothesis
and requires a new decision.

`subtree_maximum_position` remains independent of rank repair. Every rotation,
transplant, promotion, and demotion must restore it bottom-up before the finder
can be queried again.

## 5. Mutation specification gate

No WAVL mutation implementation may begin until the separate hand-checkable
[mutation transition table](lzss-wavl-tree-exact-transitions.md) is committed.
The table must derive insertion and deletion
only from the recorded rank-balanced-tree paper and must specify, for every
case and its mirror:

- triggering rank differences;
- promotion and demotion order;
- single- and double-rotation links and resulting ranks;
- propagation parent and termination condition;
- null-child handling;
- root replacement;
- successor-transplant repair origin;
- metadata repair order and maximum upward extent.

Ambiguous shorthand such as “standard WAVL repair” is insufficient. The
transition table must be reviewable without consulting implementation code and
must include small before/after rank diagrams. Code is then written from this
repository-owned table, not transcribed from another implementation.

## 6. Validation and diagnostics

The independent iterative validator must check initialization, active-slot
identity, indices, parents, connectivity, cycles, total key order, exact active
interval, subtree maxima, stored-rank validity, and every rank difference.
Small mutation fixtures validate after every insertion and retirement.

Diagnostics must separate insertion and retirement repair rather than expose
only a combined rotation total. At minimum record:

- promotions and demotions by operation kind;
- single and double rotations by operation kind;
- maximum upward repair steps for one insertion and one retirement;
- maximum query nodes, final height, and query-depth histogram;
- common comparison, LCP, insertion, retirement, and token-summary fields;
- saturating-counter overflow.

Timed paths receive a null statistics pointer and perform no diagnostic
updates or validation traversal.

## 7. Staged experiment

1. Commit the complete mutation transition table and independent hand vectors.
2. Add checked equal-extent workspace and empty-state initialization.
3. Implement insertion, rank validation, and every-position differential tests.
4. Implement physical retirement and successor transplant, validating after
   every mutation in deletion-heavy fixtures.
5. Add the existing Exact query and skipped-position advancement contracts.
6. Open a private typed-token route and run only the process-isolated synthetic
   matrix, including `frame_size > window_size` deletion-heavy cases.
7. Stop unless WAVL preserves Exact identity, retains AVL workspace, has
   bounded per-update repair, and improves a deletion-heavy metric without a
   material regression on the paired non-deleting controls.
8. Only after that gate may a separate fixed Corpus or public-admission design
   be proposed.

Ordinary Silesia frames are intentionally excluded from the first WAVL gate:
their equal frame and window sizes exercise insertion but not the hypothesized
retirement advantage.

The stage-6 benchmark adapter is available under the experimental spelling
`wavl-tree-exact`. It validates and fingerprints one untimed pass before
measuring, calculates WAVL workspace independently, and reports insertion,
retirement, rank repair, preflight, query, height, and depth diagnostics. Its
smoke contract compares AVL, Red-Black, and WAVL token identity under identical
input, frame, window, parsing, and iteration settings. This is measurement
infrastructure only; the deletion-heavy performance gate has not yet been
decided.

## 8. Interpretation

A correct WAVL implementation is not automatically useful. Failure to beat
AVL on the deletion-heavy gate is a valid negative result and ends the
candidate before broader integration. A win permits further measurement only;
it does not select a public strategy or change a default. Public consideration
would additionally require a supported persistent-window use case and a
separate latency, memory, API, and compatibility decision.

## 9. Independent reference

- Bernhard Haeupler, Siddhartha Sen, and Robert E. Tarjan,
  *Rank-Balanced Trees*:
  <https://www.cs.princeton.edu/courses/archive/fall09/cos521/Handouts/WADSrb-trees.pdf>

The paper supplies mathematical invariants, update transitions, and complexity
results only. No WAVL implementation, compressor, match finder, source code,
pseudocode outside the paper, or external test suite is an implementation
reference.
