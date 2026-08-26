# LZSS contextual Adaptive Huffman 16 MiB window

Status: design accepted after schema-51 interoperability admission. All
implementation, public, tooling, fuzzing, and interoperability boundaries
remain closed.

## Purpose and exact identity

This design applies the existing Contextual Adaptive Huffman representation to
the reserved 16-MiB typed-token LZSS family. Its exact Format 2 identity is:

```text
dictionary algorithm/variant 2/5
context-model algorithm/variant 1/4
entropy algorithm/variant 1/2
```

No completed identity is widened or reinterpreted. Entropy variant 2 retains
the fixed 16-byte descriptor, 31 independent reset-per-frame FGK trees, NYT
rules, sibling-property updates, forward LSB-first payload, and no-rescale
frame policy. Only the externally selected field-context layout grows the
distance alphabet from 23 to 25 symbols. Crossed dictionary/context pairs and
every other entropy identity remain contradictory or unsupported. Match-
finder strategy remains encoder-local and is not serialized.

## Selected model bank and count bounds

Context variant 4 contains exactly 4,582 symbol slots. One FGK tree per Symbol
context therefore requires:

```text
symbol entries = 4,582
node entries   = 2*4,582 + 31 = 9,195
entropy entries                     = 13,777
```

On the supported 64-bit layout, `AdaptiveHuffmanNode` is 16 bytes and each
symbol index is two bytes. The fixed model-bank storage is therefore 147,120
node bytes plus 9,164 symbol bytes. Selection must precede model partitioning,
payload traversal, allocation, and mutation. Existing 64-KiB, one-MiB, and
four-MiB model-bank extents and bytes remain unchanged.

For raw frame size `F`, context variant 4 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 34*token_count
```

The existing stream field permits `max_symbol_events = 2^25`, exactly `2F`
at 16 MiB. The entropy descriptor's current global decision ceiling is
`3*2^25 = 100,663,296`, which is smaller than the required
`7F = 117,440,512`. Before complete-frame admission, the shared bounded
descriptor validator must raise that ceiling to exactly 117,440,512. The
field remains `uint32`, and this validation widening changes no serialized
descriptor or completed stream byte.

## Payload and complete-frame bounds

The conservative Adaptive Huffman payload proof remains:

```text
payload_size <= ceil(267F/8)
```

The 267-bit raw-byte bound is three bits for the worst new token-kind operation
plus 264 bits for the worst new 256-symbol Literal operation. It is a safe
format and workspace ceiling rather than an expected ratio. At
`F = 16,777,216`, division is exact:

```text
payload                              559,939,584 bytes
64-byte header + 16-byte descriptor          80 bytes
complete encoded frame              559,939,664 bytes
```

Both sizes fit their existing 32-bit serialized fields. Earlier profiles
retain their frozen bytes and may continue to apply smaller local hard limits.

## Workspace proof

On the supported 64-bit native encoder layout:

```text
raw frame                         16,777,216 bytes
16,777,216 typed tokens          201,326,592 bytes
9,195 FGK nodes                      147,120 bytes
4,582 symbol indices                    9,164 bytes
HashChain heads and links         67,633,152 bytes
alignment before HashChain workspace        4 bytes
views total                      269,116,032 bytes
complete encoded frame           559,939,664 bytes
aggregate                        845,832,912 bytes
```

This is 227,908,912 bytes below an explicit one-GiB aggregate policy. Runtime
sizing must use checked arithmetic, native object extents and alignment, and
the authoritative HashChain workspace query. Exact capacity must succeed and
one byte short must fail before allocation or publication.

The decoder needs:

```text
9,195 FGK nodes                      147,120 bytes
4,582 symbol indices                    9,164 bytes
16,777,216 typed tokens          201,326,592 bytes
views total                      201,482,876 bytes
complete encoded frame           559,939,664 bytes
raw frame                         16,777,216 bytes
aggregate                        778,199,756 bytes
```

This is 295,542,068 bytes below one GiB. Decoder sizing must derive encoded
frame, raw, token, node, and symbol extents from the caller's smaller local
limits. Selecting the identity in a bounded fuzzer must not allocate a 16-MiB
history or reserve the full profile workspace.

The future additive public helper for common selector value 3 will apply
frame, window, block, and distance 16,777,216; payload limit 559,939,584;
entropy-entry limit 13,777; and a one-GiB aggregate policy. It must preserve
direction, original size, total-output policy, ABI metadata, and reserved
zeros. Initializers remain 64 KiB, callers may tighten returned hard limits,
and stream fields never enlarge local hard limits.

## Staged implementation

1. Extend selected model-bank and descriptor/count validation to context
   variant 4; prove 9,195/4,582 model extents, the 117,440,512 decision ceiling,
   and unchanged older bytes.
2. Carry the immutable layout through FGK operation coding and direct typed-
   token encode/decode with a class-24 hand vector.
3. Admit only exact complete-frame identity `2/5 + 1/4 + 1/2`, initially for
   bounded decoding and then encoding after exact preflight tests pass.
4. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
5. Admit common public C selector value 3 only for Contextual Adaptive Huffman.
6. Add explicit CLI and dependency-free benchmark names.
7. Extend bounded dual-path decoder fuzzing without profile-sized allocation.
8. Append exactly one interoperability archive after every earlier boundary
   passes, preserving all schema-51 archive bytes and order.

Every stage keeps later surfaces closed. Public exact-profile decoders must
reject the 64-KiB, one-MiB, four-MiB, and sixteen-MiB identities reciprocally
before raw publication.

## Required validation

Require model selection, NYT, existing-symbol, bypass, and distance-class-24
hand vectors; exact decision and payload ceilings; deterministic Exhaustive
and HashChain Exact agreement; all required binary classes; one-byte and mixed
chunking; terminal stability; exact and one-short workspaces; malformed
descriptor, count, padding, truncation, overflow, alias, and trailing-data
rejection; reciprocal four-profile public rejection; atomic helper behavior;
bounded sanitizer fuzzing; and four-direction interoperability.

The maximum-distance typed-token vector remains independent of the complete-
frame vector because a Match beginning after 16 MiB of history cannot also fit
inside a frame whose raw extent is at most 16 MiB. A complete-frame encoder
test instead proves a semantically necessary distance strictly greater than
four MiB, while the direct token test proves class 24 and distance 16,777,216.

## Deferred decisions

This design does not change FGK tree arithmetic, reset policy, descriptor
layout, payload representation, context mapping, typed-token grammar, match-
finder strategy, ABI extent, released identities, or library defaults. A
tighter provable payload bound, incremental entropy publication, rescaling,
or a window beyond 16 MiB remains a separate design and safety review.
