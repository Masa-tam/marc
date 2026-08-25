# LZSS contextual Blocked Huffman 16 MiB window

Status: design accepted after schema-50 interoperability admission. All
implementation, public, tooling, fuzzing, and interoperability boundaries
remain closed.

## Purpose and exact identity

This design applies the existing Contextual Blocked Huffman representation to
the reserved 16-MiB typed-token LZSS family. Its exact Format 2 identity is:

```text
dictionary algorithm/variant 2/5
context-model algorithm/variant 1/4
entropy algorithm/variant 2/2
```

No completed identity is widened or reinterpreted. Entropy variant 2 retains
four pooled field models, 31 optional per-context overrides, canonical code
construction, maximum code length 15, LSB-first payload, and strict zero
padding. Only the externally selected layout grows the distance alphabet from
23 to 25 symbols. Crossed dictionary/context pairs and every other entropy
identity remain contradictory or unsupported. Match-finder strategy remains
encoder-local and is not serialized.

## Descriptor grammar and table bound

Context variant 4 retains the 16-byte prefix, model-record version 1,
ascending override mask, and canonical Single/sparse/dense record choice. A
25-symbol dense distance record occupies thirteen bytes, one more than the
23-symbol record. At most one pooled distance model and eight distance-context
overrides are present, so the exact maximum grows by nine bytes:

```text
variant 1 (17 distance symbols) maximum  2,561 bytes
variant 2 (21 distance symbols) maximum  2,579 bytes
variant 3 (23 distance symbols) maximum  2,588 bytes
variant 4 (25 distance symbols) maximum  2,597 bytes
```

The internal model object still reserves 256 code lengths. Four pooled models
plus 31 overrides require at most 35 caller-owned `HuffmanDecodeTable`
objects containing 17,885 bounded decode nodes. Neither bound grows with this
distance alphabet. Variant selection must precede prefix, record, table,
allocation, and payload work. Parsing must reject symbols outside 0..24 in the
distance field, odd dense-record high padding, noncanonical sparse choice,
invalid tables, truncation, trailing descriptor bytes, and every crossed
selection without publishing a descriptor or table.

## Decision and payload bounds

For raw frame size `F`, shared context variant 4 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 34*token_count
```

With maximum canonical code length 15, the conservative payload and complete-
frame ceilings are:

```text
payload_size <= ceil(7F*15/8)
             = ceil(105F/8)
complete_frame_size <= ceil(105F/8) + 2,661
```

The complete-frame constant contains the 64-byte frame header and 2,597-byte
maximum descriptor. At `F = 16,777,216`, the payload ceiling is 220,200,960
bytes and the complete-frame ceiling is 220,203,621 bytes. Earlier contextual
Blocked Huffman profiles retain their frozen descriptor, count, and complete-
frame bounds.

## Workspace proof

On the supported 64-bit native encoder layout:

```text
raw frame                         16,777,216 bytes
16,777,216 typed tokens          201,326,592 bytes
HashChain heads and links         67,633,152 bytes
views total                      268,959,744 bytes
complete encoded frame           220,203,621 bytes
aggregate                        505,940,581 bytes
```

This is 30,930,331 bytes below an explicit 512-MiB aggregate policy. Runtime
sizing must use checked arithmetic, native object extents and alignment, and
the authoritative HashChain workspace query. Exact capacity must succeed and
one byte short must fail before allocation or publication.

On the supported decoder layout, each `HuffmanDecodeTable` remains 4,092
bytes:

```text
35 decode tables                     143,220 bytes
16,777,216 typed tokens          201,326,592 bytes
views total                      201,469,812 bytes
complete encoded frame           220,203,621 bytes
raw frame                         16,777,216 bytes
aggregate                        438,450,649 bytes
```

This is 98,420,263 bytes below 512 MiB. Decoder sizing must derive frame,
token, payload, descriptor, and table extents from the caller's smaller local
limits. Selecting the identity in a bounded fuzzer must not allocate a
16-MiB history.

The future additive public helper for common selector value 3 will apply
frame, window, and distance 16,777,216; block limit `7F = 117,440,512`;
payload limit 220,200,960; at least 35 entropy-table entries; and a 512-MiB
aggregate policy. It must preserve direction, original size, total-output
policy, ABI metadata, and reserved zeros. Initializers remain 64 KiB, callers
may tighten returned hard limits, and stream fields never enlarge local hard
limits.

## Staged implementation

1. Extend descriptor selection, canonical parsing, and serialization to
   context variant 4; prove the 2,597-byte maximum and unchanged older bytes.
2. Carry the immutable layout through model building, operation coding, and
   direct typed-token encode/decode with a class-24 hand vector.
3. Admit only exact complete-frame identity `2/5 + 1/4 + 2/2`, initially for
   bounded decoding and then encoding after exact preflight tests pass.
4. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
5. Admit common public C selector value 3 only for Contextual Blocked Huffman.
6. Add explicit CLI and dependency-free benchmark names.
7. Extend bounded dual-path decoder fuzzing without profile-sized allocation.
8. Append exactly one interoperability archive after every earlier boundary
   passes, preserving all schema-50 archive bytes and order.

Every stage keeps later surfaces closed. Public exact-profile decoders must
reject the 64-KiB, one-MiB, four-MiB, and sixteen-MiB identities reciprocally
before raw publication.

## Deferred decisions

This design does not change canonical Huffman construction, record modes,
maximum code length, table shape, field-context mapping, token grammar,
match-finder strategy, initializer defaults, or automatic limit policy.
Contextual Adaptive Huffman retains an independent 16-MiB payload, node-bank,
workspace, public, fuzz, and interoperability proof.
