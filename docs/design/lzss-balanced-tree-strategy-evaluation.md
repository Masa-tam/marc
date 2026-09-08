# LZSS ordered-tree Exact strategy evaluation

## 1. Purpose and scope

This document defines the implementation and measurement boundary for ordered-
tree alternatives to the existing LZSS `BinaryTree Exact` AVL match finder.
The first implementation candidate is `RedBlack Exact`; `Scapegoat Exact` is
a later batch-oriented experiment; WAVL remains deferred until a deletion-
heavy production workload exists.

All candidates are encoder-local match-finder policies. They MUST NOT change
the LZSS token representation, Format 1 or Format 2 stream bytes, algorithm or
variant identifiers, entropy coding, decoder behavior, interoperability schema,
public C ABI, or initializer defaults. `HashChain Exact` remains the public
default and the existing AVL `BinaryTree Exact` remains the only public ordered-
tree selector until a separate adoption decision is completed.

The implementation sequence follows the repository policy: references and
exact contracts precede code; Exhaustive and the existing two Exact finders
remain independent correctness oracles; optimization is admitted only after
determinism, bounds, malformed-state rejection, and measurement are complete.

## 2. Evidence and hypotheses

The fixed Silesia measurements show that the current global AVL finder becomes
relatively stronger as the window grows. Its aggregate throughput relative to
HashChain was `0.694925`, `1.456408`, and `3.371567` at one, four, and sixteen
MiB in the 16-MiB experiment, and `3.801` and `8.053` at sixteen and sixty-four
MiB in the 64-MiB experiment. Results remain member-dependent, and the 64-MiB
AVL workspace is approximately 1.81 GiB.

Three distinct hypotheses therefore require separate tests:

1. Red-Black relaxation may reduce rotations and metadata writes enough to
   offset its potentially greater height and additional finite-suffix byte
   comparisons.
2. Scapegoat subtree rebuilding may replace dispersed rotations with more
   cache-friendly sequential reconstruction in complete-frame batch encoding,
   but may introduce unacceptable single-operation work spikes.
3. WAVL may improve deletion-heavy sliding-window maintenance, but provides no
   clear benefit for public profiles whose frame and window extents are equal
   and therefore perform no ordinary within-frame retirement.

No asymptotic result decides these hypotheses because the dominant cost may be
finite-suffix comparison, range metadata maintenance, memory traffic, or tree
mutation depending on the input and window.

## 3. Shared Exact contract

Every ordered-tree candidate MUST use the existing finite key order:

1. compare unsigned bytes lexicographically up to `max_match_length`;
2. order the shorter remaining suffix first when compared bytes are equal;
3. order equal capped suffixes by increasing absolute input position.

For query position `p`, the active set is exactly every indexable position in
`[max(0, p - window_size), p)`. The selected result is the longest match, then
the greatest candidate position (nearest distance) among equal lengths. A
maximum-length match does not permit early return before the nearest-distance
tie break is resolved.

The existing predecessor/successor LCP proof and prefix-interval maximum query
remain authoritative. Every node therefore retains `subtree_maximum_position`.
All mutation policies MUST update it transactionally after link changes and
MUST pass the same independent validator and Exhaustive differential oracle.

Nodes remain fixed caller-owned array slots selected by absolute position
modulo capacity. Payloads MUST NOT be copied or swapped between slots during
two-child deletion. Native pointers, recursion, allocation after initialization,
floating-point balance decisions, locale, and unspecified library-container
ordering are forbidden.

## 4. Red-Black Exact first candidate

`RedBlack Exact` is the first implementation because it gives a bounded-latency
comparison with small structural distance from the proven AVL finder.

Its storage uses the same separate arrays for left, right, parent, absolute
position, and subtree maximum position. One `uint8_t` color entry replaces the
AVL height entry, so its initial workspace extent is expected to match the AVL
extent on supported layouts. Color values are exactly black and red; inactive
slots use a separate inactive value. The null sentinel is black.

Insertion and deletion use deterministic bottom-up cases. Left/right symmetry
MUST be explicit and tested. A two-child retirement structurally transplants
the in-order successor node; it never exchanges slot payloads. Link, color,
parent, root, and subtree-maximum updates are ordered so that no public finder
state is exposed between steps. The validator checks root color, red-parent
rules, equal black height, total key order, active-slot identity, parent links,
connectivity, cycles, subtree maxima, and protocol position.

The first implementation is private. It SHOULD share stateless key/LCP/range-
query helpers with AVL only after tests demonstrate unchanged AVL output; AVL
mutation code is not refactored merely to make the new implementation shorter.

## 5. Scapegoat Exact second candidate

`Scapegoat Exact` is initially a private, batch-oriented experiment. It uses a
fixed rational balance parameter of `alpha = 2/3`; integer checked comparisons
replace floating-point arithmetic. Parameter sweeps are a later benchmark
question and MUST NOT alter one named strategy silently.

The implementation-level policy, including the conservative integral depth
budget, scratch layout, deterministic lower-median shape, failure boundary,
and staged tests, is frozen in `lzss-scapegoat-tree-exact.md`.

The first performance-oriented layout retains left, right, parent, absolute
position, and subtree maximum position, and adds a checked `uint32_t` subtree-
size entry. This intentionally gives up the textbook no-per-node-balance-field
advantage: finding a scapegoat by repeatedly scanning large subtrees would mix
a second hypothesis into the first benchmark. The additional memory is part of
the reported workspace and may make this strategy unavailable under existing
hard limits.

After an insertion exceeds the deterministic depth bound, the first qualifying
ancestor is rebuilt into one exact shape. Rebuilding is iterative, operates on
caller-owned storage, recomputes every affected subtree size and maximum, and
uses no input-controlled recursion. Deletion is physical, not tombstoned. A
whole-tree rebuild caused by retirement is allowed only by the documented
scapegoat policy and is measured separately.

Search retains logarithmic worst-case depth, but one insertion or retirement
may rebuild a large subtree. The strategy therefore cannot become a public
streaming recommendation without a separate latency decision. An aggregate
amortized result is insufficient: diagnostics MUST report the largest rebuild,
total rebuilt nodes, rebuild count, and maximum nodes touched by one update.

## 6. Deletion-focused WAVL candidate

WAVL is not implemented in the first cycle. With insertion only, the
published rank-balanced-tree result specializes to AVL behavior. Current
public LZSS Contextual profiles reset at a frame boundary equal to their window
boundary, so the deletion-side advantage is normally absent.

The repository now has a deterministic private workload with
`frame_size > window_size` and physical retirement diagnostics. This satisfies
only the experimental half of the deferral condition: no public persistent-
window workload has been admitted. WAVL may therefore proceed as a private,
deletion-focused candidate under `lzss-wavl-tree-exact.md`. It must retain AVL
workspace, receive its own rank-difference validator and differential deletion
matrix, and pass an early synthetic gate before any Corpus or public work.

## 7. Diagnostic contract

Untimed diagnostic passes and timed passes remain separate. All strategies
report the existing input, frame, token summary, lowercase SHA-256 token
fingerprint, query count, key comparisons, key bytes, LCP bytes, range-boundary
comparisons, maximum query nodes, insertions, and retirements. AVL retains its
incrementally maintained maximum height. Red-Black instead reports the exact
final height of every frame and their maximum: a lifetime maximum would require
extra metadata writes, while a full scan after every mutation would make the
diagnostic pass quadratic and cease to be a practical one-MiB comparison.

Red-Black adds recolorings, rotations, and maximum upward fix-up
steps. Scapegoat adds depth violations, scapegoat searches, subtree rebuilds,
total rebuilt nodes, maximum rebuilt nodes, whole-tree rebuilds, and maximum
nodes touched by one update. Counters are saturating `uint64_t` values with an
overflow flag. Production paths with a null statistics pointer perform no
diagnostic counter updates.

## 8. Validation matrix

Implementation is staged as follows:

1. checked workspace layout, equality/one-short limits, alignment, overlap,
   platform width, and initialization without large allocation;
2. hand-checkable insertion, deletion, recoloring/rank/rebuild, root, and two-
   child transplant fixtures;
3. independent structural validator after every mutation for small inputs;
4. every-position differential results against Exhaustive, HashChain Exact,
   and AVL BinaryTree Exact;
5. empty, short, all-byte, zero, periodic, random-like, long-common-prefix,
   monotone-key, and adversarial retirement fixtures;
6. identical tokens, summaries, fingerprints, serialized streams, and round
   trips for every completed strategy;
7. ordinary MSVC and ClangCL compilation plus bounded sanitizer fuzzing, with a
   permanent regression for every finding;
8. fixed Silesia measurements in separate processes with immutable checkpoint
   identity and no repository-owned Corpus bytes.

The performance matrix MUST distinguish equal frame/window profiles from at
least one deletion-heavy case with a larger frame than window. Record end-to-
end throughput and workspace separately from deterministic structural counts.
No strategy wins merely by aggregate throughput if it changes an Exact result,
exceeds its hard limit, or has an undocumented unbounded operation.

## 9. Admission sequence

1. Freeze this design, provenance, and test contract.
2. Add a private Red-Black workspace calculator and empty finder.
3. Implement Red-Black insertion, deletion, validation, and Exact query.
4. Add typed-token private dispatch and focused synthetic benchmarks.
5. Run the fixed Silesia and adversarial comparison before considering public
   exposure.
6. Design and implement private Scapegoat storage and bounded iterative rebuild.
7. Compare AVL, Red-Black, and Scapegoat under the same executable, revision,
   input, frame/window, compiler, and process-isolation rules.
8. Make any public selector, default, or automatic-policy change only through a
   separate design decision after the complete evidence is recorded.

## 10. Independent references

- Igal Galperin and Ronald L. Rivest, *Scapegoat Trees*, SODA 1993:
  <https://people.csail.mit.edu/rivest/pubs/GR93.pdf>
- Bernhard Haeupler, Siddhartha Sen, and Robert E. Tarjan,
  *Rank-Balanced Trees*:
  <https://www.cs.princeton.edu/courses/archive/fall09/cos521/Handouts/WADSrb-trees.pdf>
- Robert E. Tarjan, *Efficient Top-Down Updating of Red-Black Trees*,
  Princeton technical report TR-006-85:
  <https://www.cs.princeton.edu/research/techreps/534>

These references supply data-structure definitions and complexity results only.
No external compressor, match finder, library implementation, source code, or
test suite is an implementation reference.

## 11. Initial focused Red-Black checkpoint

On 2026-09-07, the completed private candidate ran the full deterministic
synthetic matrix under ClangCL 22.1.3 Release: five one-MiB generated inputs,
one iteration, one-MiB frames, and 65,536-, 262,144-, and 1,048,576-byte
windows. All 45 independent processes completed. At each window, HashChain,
AVL, and Red-Black produced identical aggregate token counts and every
case-specific token fingerprint agreed.

| window bytes | strategy | aggregate MiB/s | maximum workspace bytes | height diagnostic |
| ---: | --- | ---: | ---: | ---: |
| 65,536 | HashChain | 22.72 | 786,432 | n/a |
| 65,536 | AVL | 1.10 | 1,900,544 | max 19 |
| 65,536 | Red-Black | 0.69 | 1,900,544 | max final 29 |
| 262,144 | HashChain | 14.27 | 1,572,864 | n/a |
| 262,144 | AVL | 0.94 | 7,602,176 | max 22 |
| 262,144 | Red-Black | 0.56 | 7,602,176 | max final 33 |
| 1,048,576 | HashChain | 10.02 | 4,718,592 | n/a |
| 1,048,576 | AVL | 1.02 | 30,408,704 | max 24 |
| 1,048,576 | Red-Black | 0.58 | 30,408,704 | max final 37 |

These one-iteration synthetic rates are an admission checkpoint, not a general
performance conclusion. They show that the intended equal-workspace comparison
is functioning and that Red-Black is slower and taller than AVL on this
particular aggregate. Member-level Silesia and deletion-heavy evidence remain
required before any admission decision.
