# LZSS contextual Adaptive Huffman 64 MiB window

Status: design and checked bounds fixed; implementation remains closed.

## Purpose and exact identity

This design applies the existing Contextual Adaptive Huffman representation to
the reserved 64-MiB typed-token LZSS family. Its eventual exact Format 2
identity is:

```text
dictionary algorithm/variant 2/6
context-model algorithm/variant 1/5
entropy algorithm/variant 1/2
```

The dictionary/context pair `2/6 + 1/5` remains inseparable. Entropy variant 2
retains the fixed 16-byte descriptor, 31 independent reset-per-frame FGK
trees, NYT rules, sibling-property updates, forward LSB-first payload, and
no-rescale frame policy. Only the externally selected field-context layout
grows the distance alphabet from 25 to 27 symbols. No earlier identity,
descriptor, initializer, selector, archive byte, or default is widened or
reinterpreted. Match-finder strategy remains encoder-local and is not
serialized.

## Selected model bank and count bounds

Context variant 5 contains exactly 4,598 symbol slots. One FGK tree per Symbol
context therefore requires:

```text
symbol entries = 4,598
node entries   = 2*4,598 + 31 = 9,227
entropy entries                     = 13,825
```

On the supported 64-bit layout, `AdaptiveHuffmanNode` is 16 bytes and each
symbol index is two bytes. The fixed model-bank storage is therefore 147,632
node bytes plus 9,196 symbol bytes. Selection must precede model partitioning,
payload traversal, allocation, and mutation. Existing 64-KiB, one-MiB,
four-MiB, and sixteen-MiB model-bank extents and bytes remain unchanged.

For raw frame size `F`, context variant 5 requires:

```text
token_count    <= F
event_count    <= 2F
decision_count <= 8F
decision_count <= 36*token_count
```

At `F = 67,108,864`, the exact event and common decision ceilings are
134,217,728 and 536,870,912. All counts and every per-tree weight remain below
`UINT32_MAX`; the no-rescale policy therefore remains safe for one bounded
frame. Before complete-frame admission, descriptor and operation validation
must select these limits with checked arithmetic. That validation change does
not alter the fixed descriptor or any earlier serialized byte.

## Payload and complete-frame bounds

The conservative Adaptive Huffman payload proof remains:

```text
payload_size <= ceil(267F/8)
```

The 267-bit raw-byte bound is three bits for the worst new token-kind operation
plus 264 bits for the worst new 256-symbol Literal operation. It is a safe
format and workspace ceiling rather than an expected compression ratio. At
`F = 67,108,864`, division is exact:

```text
payload                              2,239,758,336 bytes
64-byte header + 16-byte descriptor            80 bytes
complete encoded frame              2,239,758,416 bytes
```

Both values fit the existing 32-bit serialized payload field. A future
fourfold window cannot reuse this proof because its payload ceiling would
exceed `UINT32_MAX`. Earlier profiles retain their frozen bounds and may
continue to enforce smaller caller policy.

## Workspace proof

On the supported 64-bit native encoder layout, HashChain Exact requires:

```text
raw frame                            67,108,864 bytes
67,108,864 typed tokens            805,306,368 bytes
9,227 FGK nodes                        147,632 bytes
4,598 symbol indices                    9,196 bytes
HashChain heads and links           268,959,744 bytes
alignment before finder workspace              4 bytes
views total                       1,074,422,944 bytes
complete encoded frame           2,239,758,416 bytes
aggregate                        3,381,290,224 bytes
```

BinaryTree Exact substitutes its 1,946,157,056-byte finder, producing
2,751,620,256 view bytes and a 5,058,487,536-byte aggregate. Both fit an
explicit eight-GiB aggregate policy. Runtime sizing must use checked native
extents, canonical alignment, and the selected finder's authoritative query.
Exact capacity must succeed and one byte short must fail before allocation or
publication. A platform whose `size_t` cannot represent all regions rejects
the profile without publishing partial requirements.

The decoder needs:

```text
9,227 FGK nodes                        147,632 bytes
4,598 symbol indices                    9,196 bytes
67,108,864 typed tokens            805,306,368 bytes
views total                         805,463,196 bytes
complete encoded frame           2,239,758,416 bytes
raw frame                            67,108,864 bytes
aggregate                        3,112,330,476 bytes
```

The future public helper applies frame, window, block, and distance
67,108,864; payload limit 2,239,758,336; entropy-entry limit 13,825; and the
eight-GiB aggregate policy. It preserves direction, original size,
total-output policy, ABI metadata, reserved zeros, and selected Exact finder.
Initializers remain 64 KiB, callers may tighten returned hard limits, and
stream fields never enlarge local limits. Selecting the identity in a bounded
fuzzer must not allocate a 64-MiB frame or history.

## Staged implementation

1. Extend selected model-bank and descriptor/count validation to context
   variant 5; prove 9,227/4,598 model extents, exact count ceilings, and
   unchanged older bytes.
2. Carry the immutable layout through FGK operation coding and direct typed-
   token encode/decode with class-26 and first-new-distance hand vectors.
3. Admit only exact complete-frame identity `2/6 + 1/5 + 1/2`, initially for
   bounded decoding and then encoding after exact preflight tests pass.
4. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests for both Exact finders. Exhaustive remains
   closed.
5. Admit common public C selector value 4 only for Contextual Adaptive Huffman.
6. Add exact CLI and dependency-free benchmark name
   `lzss-contextual-adaptive-huffman-64m` through the public lifecycle.
7. Extend bounded dual-path decoder fuzzing without profile-sized allocation.
8. Append exactly one interoperability archive after every earlier boundary
   passes, preserving all schema-56 archive bytes and order.

Each stage keeps later surfaces closed, preserves all earlier bytes, and
rejects crossed profiles before token or raw publication.

## Required validation

- prove the 9,227-node and 4,598-symbol model-bank extents at equality and one
  entry short;
- accept `2F`, `8F`, and `36T` at equality and reject one above without
  overflow or mutation;
- exercise NYT, existing-symbol, bypass, and distance-class-26 operations;
- prove payload, complete-frame, and every workspace ceiling at equality and
  one byte short;
- preserve HashChain/BinaryTree Exact token and completed-stream identity;
- decode the first newly reachable distance 16,777,217 and retain the shared
  distance-67,108,864 primitive proof;
- reject every reciprocal crossing with the four published Contextual
  Adaptive Huffman profiles;
- keep bounded fuzz model, frame, token, raw, output, and call storage
  independent of the selected 64-MiB maximum; and
- retain schema 56 unchanged until the complete public lifecycle passes.

## Deferred decisions

This design does not change FGK tree arithmetic, reset policy, descriptor
layout, payload representation, context mapping, typed-token grammar,
match-finder strategy, ABI extent, released identities, or library defaults.
A tighter payload proof, incremental entropy publication, rescaling, or a
window beyond 64 MiB remains a separate format-width and safety review.
