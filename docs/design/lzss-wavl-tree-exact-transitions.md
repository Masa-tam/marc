# LZSS WAVL Tree Exact mutation transitions

## 1. Scope and notation

This document is the repository-owned mutation specification required by the
WAVL feasibility gate. It defines bottom-up rank repair for the private
`WAVL Tree Exact` candidate. Implementation must be written from this table,
not from an external implementation.

For an active node `x`, `r(x)` is its nonnegative rank. A null child has
mathematical rank `-1`. The difference on parent-child edge `p -> x` is
`d(p,x) = r(p) - r(x)`. Every settled edge has difference one or two. Every
settled leaf is a rank-zero `1,1` node. A rank-one `2,2` leaf exists only as
the transient deletion violation described below; settled `2,2` nodes must be
non-leaves.

The diagrams use `A` through `D` for unchanged subtrees and show absolute
ranks relative to `k`. The insertion diagrams choose `q` as the left child of
`p`. The deletion diagrams choose deficient `q` as the right child of `p`,
with sibling `s`, outer nephew `u = left(s)`, and inner nephew
`t = right(s)`. Every row has the mirror obtained by exchanging left and
right; the mirror performs the opposite rotations and exactly the same rank
changes.

Promotion adds one to a rank. Demotion subtracts one. Rank arithmetic must be
checked before mutation. Stored rank `255` is reserved for inactive slots and
may never result from promotion.

## 2. Insertion initialization and transitions

A new node is inserted at the reached null link with rank zero and two null
children. Let `q` be this node, or the root of the already repaired subtree
being propagated upward, and let `p` be its parent. Only a zero-difference edge
`p -> q` can be unsettled.

| ID | Trigger before the step | Rank and link action | Continue from |
| --- | --- | --- | --- |
| I0 | `p` is null, or `d(p,q) != 0` | None | Stop. `q` is the root when `p` is null. |
| I1 | `d(p,q) = 0`, sibling `s` is a 1-child | Promote `p` once. No links change. | Set `q = p`, `p = parent(q)`; continue. |
| I2L | `q = left(p)`, `d(p,q) = 0`, `d(p,s) = 2`, and inner child `t = right(q)` is a 2-child | Rotate right at `q`; demote old root `p` once. | Stop at new local root `q`. |
| I2R | Mirror of I2L | Rotate left at `q`; demote old root `p` once. | Stop at new local root `q`. |
| I3L | Same outer shape, but inner `t = right(q)` is a 1-child | Rotate left at `t`, then right at `t`; promote `t` once and demote `q` and `p` once each. | Stop at new local root `t`. |
| I3R | Mirror of I3L | Rotate right at `t`, then left at `t`; apply the same rank changes. | Stop at new local root `t`. |

I1 is the only nonterminal insertion case. The settled rank diagrams are:

```text
I1 promotion (symmetric in orientation)

before: p:k                         after: p:k+1
        /   \                              /     \
     q:k   s:k-1                        q:k     s:k-1
     d=0    d=1                         d=1      d=2
```

```text
I2L single rotation

before:        p:k                  after:       q:k
              /   \                            /   \
           q:k     s:k-2                       A   p:k-1
          /   \                                  /   \
       A:k-1  t:k-2                           t:k-2 s:k-2
```

```text
I3L double rotation

before:        p:k                  after:       t:k
              /   \                            /   \
           q:k     s:k-2                    q:k-1 p:k-1
          /   \                            /  \   /  \
       A:k-2  t:k-1                     A:k-2 B  C  s:k-2
              /   \
           B,C:(k-2 or k-3)
```

`I2R` and `I3R` are hand vectors formed by reflecting these diagrams. The
absolute ranks, promotions, and demotions do not change.

## 3. Deletion normalization

First reduce deletion to removal of a node with at most one active child. The
replacement `q` may be null. If a node with two children is requested, use the
physical in-order successor normalization in Section 5; do not swap payloads.
Let `p` be the parent of `q` after replacement.

Deletion can initially produce either:

- a rank-one `2,2` leaf `p`, with both `q` and its sibling null; or
- a 3-child `q` of `p`.

After the first nonterminal step, only the 3-child form can propagate.

## 4. Deletion transitions

| ID | Trigger before the step | Rank and link action | Continue from |
| --- | --- | --- | --- |
| D0 | `p` is null, or `q` is not a 3-child and `p` is not a rank-one `2,2` leaf | None | Stop. If `p` is null, `q` is the replacement root or the tree is empty. |
| D1 | `q` is a 3-child and sibling `s` is a 2-child; or `p` is the transient rank-one `2,2` leaf, where both null children are 2-children | Demote `p` once. In the leaf form this makes `p` a settled rank-zero `1,1` leaf. | Set `q = p`, `p = parent(q)`; continue. |
| D2L | `q = right(p)` is a 3-child, `s` is a 1-child, and both `u = left(s)` and `t = right(s)` are 2-children | Demote `p` and `s` once each. No links change. | Set `q = p`, `p = parent(q)`; continue. |
| D2R | Mirror of D2L | Apply the same two demotions. | Continue as D2L. |
| D3L | Same deficient orientation and `u` is a 1-child; `t` is non-null | Rotate right at `s`; promote `s` once and demote `p` once. | Stop at new local root `s`. |
| D3R | Mirror of D3L | Rotate left at `s`; apply the same rank changes. | Stop at new local root `s`. |
| D4L | Same as D3L but `t` is null | Here `q` is also null and pre-step `p` has rank two. Rotate right at `s`; promote `s` once and demote `p` twice, making `p` a rank-zero leaf. | Stop at new local root `s`. |
| D4R | Mirror of D4L | Rotate left at `s`; apply the same rank changes. | Stop at new local root `s`. |
| D5L | `q = right(p)` is a 3-child, `s` is a 1-child, inner `t` is a 1-child, and outer `u` is a 2-child | Rotate left at `t`, then right at `t`; promote `t` twice, demote `s` once, and demote `p` twice. | Stop at new local root `t`. |
| D5R | Mirror of D5L | Rotate right at `t`, then left at `t`; apply the same rank changes. | Stop at new local root `t`. |

D1 and D2 are the only nonterminal deletion cases. Representative settled
rank diagrams are:

```text
D1 demotion, shown with a 3-child q

before: p:k                         after: p:k-1
        /   \                              /     \
     s:k-2 q:k-3                       s:k-2   q:k-3
      d=2   d=3                         d=1     d=2
```

```text
D2L double demotion

before:         p:k                 after:        p:k-1
               /   \                            /     \
            s:k-1  q:k-3                    s:k-2     q:k-3
            /   \                           /   \
         u:k-3 t:k-3                     u:k-3 t:k-3
```

```text
D3L single rotation, t non-null

before:         p:k                 after:        s:k
               /   \                            /   \
            s:k-1  q:k-3                    u:k-2  p:k-1
            /   \                                  /   \
         u:k-2  t:(k-2 or k-3)       t:(k-2 or k-3) q:k-3
```

```text
D4L single rotation, null t and q

before:         p:2                 after:        s:2
               /   \                            /   \
            s:1    null:-1                     u:0  p:0
            /  \                                    / \
         u:0  null:-1                           null null
```

```text
D5L double rotation

before:         p:k                 after:        t:k
               /   \                            /   \
            s:k-1  q:k-3                    s:k-2  p:k-2
            /   \                           /  \    /  \
         u:k-3  t:k-2                    u:k-3 B   C   q:k-3
                 /   \
              B,C:(k-3 or k-4)
```

`D2R` through `D5R` are independent mirror vectors: reflect each canonical
diagram, exchange left/right link updates, and retain every shown rank. Tests
must instantiate both orientations rather than treating the mirror as a code
coverage assumption.

## 5. Physical successor transplant

Slot identity is part of Exact behavior, so a two-child retirement cannot use
the paper's item-swap presentation. Normalize it as follows:

1. Let `z` be the requested physical slot and `y` the leftmost node in
   `right(z)`. Record `z_rank`, `y`'s old parent `yp`, and replacement
   `q = right(y)` before changing links. The successor has no left child.
2. If `yp != z`, replace `y` by `q` at `yp`, attach `right(z)` as `right(y)`,
   and choose `p = yp` as the deletion-repair parent.
3. If `yp == z`, retain `q` as `right(y)` and choose the moved `y` itself as
   deletion-repair parent `p`.
4. Replace `z` by the physical slot `y`, attach `left(z)` as `left(y)`, and set
   `r(y) = z_rank`. Update every reciprocal parent link and the root if needed.
5. Apply Section 4 to edge `p -> q`. The rank copied to `y` makes its new
   top-level edges equivalent to the old `z` edges; the only unsettled edge is
   the splice edge identified above.
6. Retire slot `z` only after its links can no longer be reached. Reset all its
   stored fields, including rank `255`, without changing `y`'s payload or
   absolute position.

For a zero- or one-child retirement, structurally replace `z` by its child
`q`, set `p` to the resulting parent, and begin Section 4 there. Removing the
only root makes `p` null and is D0. Replacing a root by its only child also
reparents that child to null before D0.

## 6. Rotation attachment and root replacement

Every rotation first records the former grandparent. The new local root is
attached to that grandparent through the exact link that formerly named the
old local root. If there is no grandparent, it becomes the tree root and its
parent is null. Child-parent reciprocity is restored for every moved subtree,
including non-null `B` and `C` subtrees in double rotations.

Rank changes are conceptually those in the tables and may be assigned before
or after link rewiring only if checked preconditions are evaluated from the
unchanged pre-step state. A failed precondition or checked-arithmetic failure
must leave the complete tree byte-stable.

## 7. Subtree-maximum metadata repair

Rank is not an input to `subtree_maximum_position`, but structural replacement
and rotation are. Mutation is transactional until all affected metadata are
valid, and no query may observe an intermediate state.

- With no rotation, recompute the current `p` after its child replacement and
  then each propagated ancestor once, stopping at the root.
- After a single rotation, recompute the demoted old root first, then the new
  local root, then ancestors beginning at the new root's parent.
- After a double rotation, recompute the two demoted side nodes first, then
  the promoted middle node, then its ancestors.
- For a non-direct successor, the deepest dirty origin is old parent `yp`;
  the current-parent walk reaches the transplanted successor and every higher
  ancestor. For a direct successor, the origin is the moved successor.

The maximum metadata walk is one current parent chain from the splice origin
to the root plus the constant two or three local nodes of a terminal rotation.
It is bounded by validated tree height and must be iterative. Tests recompute
every subtree maximum independently after each mutation.

## 8. Required hand-vector assertions

For I0-I3 and D0-D5, tests must instantiate each listed orientation and assert:

- exact precondition classification and selected transition ID;
- exact post-step links, root, parents, and absolute ranks;
- exact promotion, demotion, single-rotation, and double-rotation counters;
- continuation parent for I1, D1, and D2, and termination for all other cases;
- null-child and rank-one-leaf behavior in D0, D1, and D4;
- direct and non-direct successor origins from Section 5;
- byte-stable rejection of an impossible difference, rank overflow, bad
  reciprocal link, or incorrect claimed transition;
- independent rank-rule, ordering, connectivity, active-slot, and subtree-
  maximum validation after the step.

The transition suite is a specification gate. Passing round trip alone cannot
replace these assertions.

## 9. Source boundary

The transitions and diagrams are independently restated from Section 3 and
Figures 2 and 3 of:

- Bernhard Haeupler, Siddhartha Sen, and Robert E. Tarjan,
  *Rank-Balanced Trees*:
  <https://www.cs.princeton.edu/courses/archive/fall09/cos521/Handouts/WADSrb-trees.pdf>

Only the mathematical rank rule and bottom-up update cases were used. The
successor-transplant, slot identity, checked mutation, metadata, diagnostics,
and test contracts are marc-owned adaptations. No external WAVL source code,
compressor, match finder, or test suite was consulted.
