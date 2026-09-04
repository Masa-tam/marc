# LZSS contextual Blocked Huffman 64 MiB window

Status: descriptor admission complete; operation and typed-token paths verified;
bounded frame decoding admitted; Exact finder frame encoding admitted.

## Purpose and exact identity

This design applies the existing Contextual Blocked Huffman representation to
the reserved 64-MiB typed-token LZSS family. Its eventual exact Format 2
identity is:

```text
dictionary algorithm/variant 2/6
context-model algorithm/variant 1/5
entropy algorithm/variant 2/2
```

The pair `2/6 + 1/5` remains inseparable. Entropy variant 2 retains four pooled
field models, 31 optional per-context overrides, canonical code construction,
maximum code length 15, LSB-first payload, and strict zero padding. Only the
externally selected layout grows the distance alphabet from 25 to 27 symbols.
No earlier identity, descriptor, initializer, selector, or archive byte is
widened or reinterpreted. Match-finder strategy remains encoder-local and is
not serialized.

## Descriptor grammar and table bound

Context variant 5 retains the 16-byte prefix, model-record version 1,
ascending override mask, and canonical Single/sparse/dense record choice. A
27-symbol dense distance record occupies fourteen bytes, one more than the
25-symbol variant-4 record. At most one pooled distance model and eight
distance-context overrides are present, so the exact maximum grows by nine
bytes:

```text
variant 1 (17 distance symbols) maximum  2,561 bytes
variant 2 (21 distance symbols) maximum  2,579 bytes
variant 3 (23 distance symbols) maximum  2,588 bytes
variant 4 (25 distance symbols) maximum  2,597 bytes
variant 5 (27 distance symbols) maximum  2,606 bytes
```

The internal model object still reserves 256 code lengths. Four pooled models
plus 31 overrides require at most 35 caller-owned `HuffmanDecodeTable` objects
containing 17,885 bounded decode nodes. Neither bound grows with the selected
distance alphabet. Variant selection must precede prefix, record, table,
allocation, and payload work. Parsing must reject symbols outside 0..26 in the
distance field, odd dense-record high padding, noncanonical sparse choice,
invalid tables, truncation, trailing descriptor bytes, and every crossed
selection without publishing a descriptor or table.

Descriptor capacity is now 2,606 bytes and variant-5 parsing and serialization
are admitted. Tests fix the exact maximum, canonical record choice, unchanged
older Single bytes, reciprocal variant-4 rejection, strict padding, and
exact/one-short table and aggregate limits. Bounded frame decoding is now
admitted; Exact finder frame encoding is admitted with equal-byte and capacity
proofs. Long-distance encoder coverage and internal profile/streaming are complete.

## Decision and payload bounds

For raw frame size `F`, shared context variant 5 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 8F
decision_count <= 36*token_count
```

With maximum canonical code length 15, the conservative payload and complete-
frame ceilings are:

```text
payload_size <= ceil(8F*15/8)
             = 15F
complete_frame_size <= 15F + 2,670
```

The complete-frame constant contains the 64-byte frame header and 2,606-byte
maximum descriptor. At `F = 67,108,864`, the payload ceiling is 1,006,632,960
bytes and the complete-frame ceiling is 1,006,635,630 bytes. Both fit the
existing 32-bit serialized payload and descriptor-related fields. Earlier
profiles retain their frozen descriptor, count, and complete-frame bounds.

## Workspace proof

On the supported 64-bit native layout, HashChain Exact encoding requires:

```text
raw frame                          67,108,864 bytes
67,108,864 typed tokens          805,306,368 bytes
HashChain heads and links        268,959,744 bytes
views total                    1,074,266,112 bytes
complete encoded frame         1,006,635,630 bytes
aggregate                      2,148,010,606 bytes
```

BinaryTree Exact substitutes its 1,946,157,056-byte finder, producing
2,751,463,424 view bytes and a 3,825,207,918-byte aggregate. Both fit an
explicit four-GiB aggregate policy; BinaryTree retains 469,759,378 bytes of
headroom. Runtime queries must use checked native extents, alignment, and the
selected finder's authoritative calculator. Exact capacity must succeed and
one byte short must fail before allocation or publication.

On the supported decoder layout, each `HuffmanDecodeTable` remains 4,092
bytes:

```text
35 decode tables                     143,220 bytes
67,108,864 typed tokens          805,306,368 bytes
views total                      805,449,588 bytes
complete encoded frame         1,006,635,630 bytes
raw frame                         67,108,864 bytes
aggregate                      1,879,194,082 bytes
```

The future public helper applies frame, window, and distance 67,108,864;
block limit `8F = 536,870,912`; payload limit 1,006,632,960; 17,885 entropy-
table entries; and a four-GiB aggregate policy. It preserves direction,
original size, total-output policy, ABI metadata, reserved zeros, and selected
Exact finder. Initializers remain 64 KiB, callers may tighten returned hard
limits, and stream fields never enlarge local policy. Selecting the identity
in a bounded fuzzer must not allocate a 64-MiB frame or history.

## Staged implementation

1. **Complete:** expand descriptor selection, canonical parsing, and serialization to
   context variant 5; prove the 2,606-byte maximum and unchanged older bytes.
2. **Complete:** carry the immutable layout through model building, operation coding, and
   direct typed-token encode/decode with a class-26 hand vector.
3. **Complete:** admit exact complete-frame identity `2/6 + 1/5 + 2/2`
   for bounded decoding. Exact finder encoding and capacity proofs are complete;
   both Exact finders also prove distance 16,777,221 and equal complete frames.
   Exhaustive stays closed.
4. **Complete:** add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests for both Exact finders. Exhaustive remains
   closed.
5. **Complete:** admit common public C selector value 4 for Contextual Blocked Huffman.
6. **Complete:** add exact CLI and dependency-free benchmark name
   `lzss-contextual-blocked-huffman-64m` through the public lifecycle.
7. **Complete:** extend bounded dual-path decoder fuzzing without profile-sized allocation.
8. Append exactly one interoperability archive after every earlier boundary
   passes, preserving all schema-55 archive bytes and order.

Each stage must retain all earlier bytes, reject crossed profiles before token
or raw publication, and keep every incomplete outward boundary closed.

## Required validation

- prove the 2,606-byte exact descriptor ceiling and canonical 27-symbol record;
- accept `8F` and `36T` at equality and reject one above without overflow;
- prove table counts, payload, complete-frame, and all workspace ceilings at
  equality and one byte short;
- preserve HashChain/BinaryTree Exact token and completed-stream identity;
- decode the first newly reachable distance 16,777,217 and retain the shared
  distance-67,108,864 primitive proof;
- reject every reciprocal crossing with the four published Contextual Blocked
  Huffman profiles;
- keep bounded fuzz frame, table, token, raw, output, and call storage
  independent of the selected 64-MiB maximum; and
- retain schema 55 unchanged until the complete public lifecycle passes.

## Deferred decisions

This design does not change canonical Huffman construction, record modes,
maximum code length, table shape, field-context mapping, token grammar,
match-finder serialization, initializer defaults, resource helpers, or
automatic limit policy. Contextual Adaptive Huffman retains an independent
64-MiB payload, node-bank, workspace, public, fuzz, and interoperability proof.
