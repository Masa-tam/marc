# LZSS Scapegoat Tree Exact Design

## 1. Purpose and admission boundary

This document freezes the first private `Scapegoat Exact` LZSS match-finder
variant. It tests whether infrequent deterministic subtree reconstruction can
outperform AVL and Red-Black mutation while preserving the established Exact
parse.

This stage adds no public strategy, C ABI value, profile, CLI codec selector,
frame encoder, interoperability archive, stream metadata, or default. The
finder is encoder-local. HashChain remains the public default and AVL remains
the only public ordered-tree strategy.

## 2. Exact key and match contract

Scapegoat Exact uses the existing finite-suffix order without modification:

1. compare unsigned bytes lexicographically up to `max_match_length`;
2. when compared bytes are equal, order the shorter remaining suffix first;
3. order equal capped suffixes by increasing absolute input position.

At query position `p`, the active set is every indexable position in
`[max(0, p - window_size), p)`. Select the longest match, then the greatest
candidate position (nearest distance) on equal length. The existing
predecessor/successor LCP and prefix-interval maximum proof remains
authoritative. Every candidate result and typed token MUST match Exhaustive,
HashChain Exact, AVL Exact, and Red-Black Exact for identical input and
parameters.

## 3. Fixed balancing policy

The balance parameter is exactly `alpha = 2/3`. Tests use integer products in
`uint64_t`:

```text
child is alpha-heavy iff 3 * child_size > 2 * parent_size
whole rebuild after deletion iff 3 * active_count < 2 * q
```

`q` is the greatest active count since the last whole-tree rebuild. Insertion
increments the active count and raises `q` when necessary. A whole-tree rebuild
sets `q` to the current active count. Emptying the tree sets both values to
zero.

The insertion depth is counted in edges from the root. Floating-point
logarithms and generated approximation tables are forbidden. An insertion
violates the deterministic depth budget when:

```text
depth > 2 * bit_width(q)
```

For positive `q`, this is a conservative integral upper bound on
`log_(3/2)(q)`. Because `3/2 > sqrt(2)`, a violation also exceeds the standard
Scapegoat depth threshold, so an alpha-heavy ancestor must exist. Starting at
the inserted node and walking toward the root, rebuild the first parent whose
path child is alpha-heavy. Failure to find one is an internal-state error, not
permission to continue with an unbounded tree.

This named variant fixes both `alpha` and the depth budget. Later parameter
sweeps require separate benchmark labels and cannot silently change it.

## 4. Fixed-slot storage and workspace

Node slots are selected by absolute input position modulo capacity. Payloads
are never copied or swapped between slots, including two-child deletion.
Permanent caller-owned arrays contain:

- left child: `uint32_t[node_capacity]`;
- right child: `uint32_t[node_capacity]`;
- parent: `uint32_t[node_capacity]`;
- subtree size: `uint32_t[node_capacity]`;
- absolute position: `size_t[node_capacity]`;
- subtree maximum position: `size_t[node_capacity]`.

Inactive slots use the common null index and no-position sentinels. Subtree
size is deliberately stored: repeatedly scanning subtrees to locate a
scapegoat would mix a second performance hypothesis into the comparison.

One additional `uint32_t[node_capacity]` scratch array holds an in-order node-
index sequence during rebuilding. Interval reconstruction uses a fixed local
task stack of
`2 * numeric_limits<uint32_t>::digits + 1` entries. The implementation MUST
reject an unrepresentable node count and MUST NOT allocate after
initialization. The checked workspace calculator exposes every offset,
alignment, permanent region, scratch region, and total byte count.

The initial private regime is bounded to inputs and windows admitted by the
existing limits. A profile whose caller aggregate cannot contain this larger
workspace simply cannot select the private experiment; limits are not raised
implicitly.

## 5. Deterministic subtree rebuilding

Rebuilding a subtree of `k` nodes has three iterative phases:

1. Traverse the original subtree in order using parent links and its boundary
   parent. Write exactly `k` slot indices to scratch and verify strictly
   increasing finite-suffix order.
2. Reconnect half-open sorted intervals. Select the lower median
   `lo + (hi - lo - 1) / 2` as each subtree root, attach it to the original
   boundary parent on the original side, and create children with the fixed
   task stack.
3. Traverse the rebuilt subtree in iterative post-order and recompute subtree
   size and subtree maximum position bottom-up before returning.

The scratch sequence preserves slot identity; only structural indices and
metadata change. Empty intervals produce the null sentinel. The phase must
consume exactly the expected node count, stay within the fixed task bound, and
leave the boundary parent metadata correct through the root.

All capacity, alignment, overlap, arithmetic, and slot-availability failures
are detected before structural mutation. Once rebuilding begins, its loops
have bounds derived from `k` and no expected failure path. Detection of an
impossible link, count, order, or task bound makes finder state sticky-invalid;
partially mutated private workspace is not reused.

## 6. Insertion, retirement, and advancement

Insertion follows the common deterministic BST path, initializes one inactive
fixed slot, and updates size and maximum metadata toward the root. It then
applies the depth rule and at most one subtree rebuild. Duplicate positions,
occupied modulo slots, backward protocol movement, or invalid positions are
rejected before mutation.

Retirement physically removes the exact slot. A two-child node is replaced
structurally by its in-order successor; payloads remain in their original
slots. Metadata is repaired along every affected ancestor path. After the
active count is reduced, apply the whole-rebuild inequality. A whole rebuild
is counted separately and resets `q`.

Advancement retains the common ordering: for every skipped raw position,
retire the position that has reached `window_size` distance before inserting
the current position when the five-byte index prefix remains. Bulk and one-
byte advancement MUST reach identical root, `q`, active population, complete
slot state, and Exact results.

## 7. Validation and diagnostics

The independent validator is iterative and checks:

- initialization, root, active count, and `q >= active_count`;
- active/inactive slot identity and position modulo capacity;
- index bounds, parent links, connectivity, and absence of cycles;
- strict total key order;
- exact subtree sizes and subtree maximum positions;
- the protocol position and active interval;
- the fixed task/scratch capacity contract.

The untimed diagnostic path reports the common query, comparison, LCP,
prefix-range, insertion, retirement, histogram, and final-height fields. It
also reports depth violations, scapegoat ancestor steps, subtree rebuilds,
whole-tree rebuilds, total rebuilt nodes, maximum rebuilt nodes, and maximum
structural node visits in one update. A structural visit counts each node read
or written by insertion/retirement metadata repair, scapegoat search, flatten,
reconnect, or post-order repair; repeated visits count repeatedly. Query work
is excluded from this update metric.

All counters are saturating `uint64_t` with the common overflow flag. The timed
path receives a null statistics pointer and performs no counter writes,
validation scan, or final-height traversal.

## 8. Staged verification

Implementation proceeds only through these separately reviewable stages:

1. checked workspace layout, exact/one-short limits, alignment, overlap, and
   empty initialization;
2. fixed-slot insertion and metadata without rebuilding;
3. deterministic iterative subtree rebuilding and hand-checkable shapes;
4. physical retirement and whole-tree rebuilding;
5. independent structural validation after every small mutation;
6. Exact query and differential every-position comparison;
7. skipped-position and one-byte/bulk advancement equivalence;
8. private typed-token integration, sentinel and aggregate-limit tests;
9. synthetic process-isolated comparison including deletion-heavy input;
10. fixed local Silesia comparison before any admission decision.

Sanitizer fuzzing is added only after the deterministic state-machine tests.
Every finding requires a permanent regression. No benchmark result can admit
the strategy if Exact identity, a hard limit, or the single-update work report
fails.
