# LZSS contextual Blocked Huffman 4 MiB window

Status: implementation in progress after completion of the Dynamic Range,
canonical contextual rANS, and contextual tANS four-MiB vertical paths. The
descriptor, entropy-operation, direct typed-token, complete-frame, profile,
streaming-lifecycle, public, CLI, benchmark, and bounded-fuzz boundaries are
complete; interoperability remains closed.

## Purpose and exact identity

This design admits Contextual Blocked Huffman as the fourth entropy backend
for the four-MiB typed-token LZSS family. It uses exact Format 2 identity:

```text
dictionary algorithm/variant 2/4
context-model algorithm/variant 1/3
entropy algorithm/variant 2/2
```

No existing identity is widened or reinterpreted. The entropy representation
retains its four pooled field models, 31 optional per-context overrides,
canonical code construction, maximum code length 15, LSB-first payload, and
strict zero-padding rule. Only the externally selected field-context layout
widens the distance alphabet from 21 to 23 symbols.

Crossed dictionary/context pairs and every other entropy identity remain
contradictory or unsupported. Match-finder strategy remains encoder-local and
is not serialized.

## Descriptor grammar and table bound

Context variant 3 retains the 16-byte descriptor prefix, model-record version
1, ascending override mask, and canonical Single/sparse/dense record choice.
The descriptor remains between 24 bytes and a layout-selected maximum.

Moving from the 21-symbol variant-2 distance alphabet to 23 symbols increases
the canonical dense record from eleven bytes to twelve. At most one pooled
distance model and eight distance-context overrides are present, so the exact
maximum grows by nine bytes:

```text
variant 1 (17 distance symbols) maximum  2,561 bytes
variant 2 (21 distance symbols) maximum  2,579 bytes
variant 3 (23 distance symbols) maximum  2,588 bytes
```

The internal model object continues to reserve 256 code lengths. No native
layout is serialized. At most four pooled models plus 31 overrides require 35
caller-owned `HuffmanDecodeTable` objects, containing 17,885 bounded decode
nodes in total. The table count and node capacity do not grow with the selected
distance alphabet.

Variant selection must precede prefix, record, table, allocation, and payload
work. Variant 3 accepts symbols 0 through 22 only for the distance field and
must reject odd dense-record high padding, noncanonical sparse choice, invalid
tables, truncation, trailing descriptor bytes, and every crossed selection
without publishing a descriptor or table.

## Decision and payload bounds

For raw frame size `F`, shared context variant 3 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 32*token_count
```

With maximum canonical code length 15, the exact conservative payload and
complete-frame ceilings are:

```text
payload_size <= ceil(7F*15/8)
             = ceil(105F/8)
complete_frame_size <= ceil(105F/8) + 2,652
```

The complete-frame constant is the 64-byte frame header plus the 2,588-byte
maximum descriptor. For `F = 4,194,304`, the payload ceiling is 55,050,240
bytes and the complete-frame ceiling is 55,052,892 bytes. Both remain below
the existing 64-MiB compressed-payload limit.

Older variants retain their frozen `6F` decision and `ceil(90F/8)` payload
rules. The new bounds are allocation and validation rules; they do not pad a
payload or add a serialized field.

## Workspace proof

On the supported 64-bit layout, the encoder owns:

```text
raw frame                          4,194,304 bytes
4,194,304 typed tokens            50,331,648 bytes
HashChain heads and links          17,301,504 bytes
views total                        67,633,152 bytes
complete encoded frame             55,052,892 bytes
aggregate                         126,880,348 bytes
```

This is 7,337,380 bytes below the unchanged 128-MiB default. Runtime sizing
must use native `sizeof`, alignment, and the checked HashChain workspace
calculator. Require default success, one-byte-short failure, and exact-limit
success. HashChain Exact remains the production finder; complete and sparse
HashTree routes remain private experiments.

On the current supported layout, each `HuffmanDecodeTable` is 4,092 bytes.
The conservative decoder reservation is:

```text
35 decode tables                     143,220 bytes
4,194,304 typed tokens            50,331,648 bytes
views total                        50,474,868 bytes
complete encoded frame             55,052,892 bytes
raw frame                           4,194,304 bytes
aggregate                         109,722,064 bytes
```

This is 24,495,664 bytes below 128 MiB. Decoder sizing must derive raw, token,
payload, and descriptor extents from the caller's selected smaller limits so
bounded fuzzing does not allocate a four-MiB frame merely to admit the stream
identity.

A full profile sets `max_frame_size` and LZ distance to four MiB,
`max_block_size` to at least `7F = 29,360,128`, compressed payload to at least
55,050,240 bytes, entropy table entries to at least 35, and retains the
128-MiB aggregate default. No library default is silently increased.

## Staged implementation

1. **Complete.** Expand descriptor selection and capacity to context variant 3. Prove the
   exact 2,588-byte maximum, canonical 23-symbol records, unchanged old bytes,
   and atomic rejection.
2. **Complete.** Carry the immutable selection through model building, operation coding,
   direct typed-token encode/decode, and their hand vectors.
3. **Complete.** Admit exact complete-frame identity `2/4 + 1/3 + 2/2`, then add checked
   profile calculation and one-byte streaming with exact aggregate tests.
4. **Complete.** Admit common public C window selector value 2 only for Contextual Blocked
   Huffman, then add the explicit CLI name
   `lzss-contextual-blocked-huffman-4m`.
5. **Complete.** Add the dependency-free benchmark and bounded dual-path decoder fuzzer;
   retain generated fuzz mutations outside the repository.
6. Append exactly one interoperability archive only after every earlier
   boundary passes. Preserve all old archive bytes and schema order.

Each stage leaves incomplete outward surfaces closed. Public exact-profile
decoders reject older and newer identities reciprocally before raw
publication. Encoder and decoder use the same selected layout for their full
lifetime.

## Deferred decisions

This design does not change canonical Huffman construction, record modes,
maximum code length, table shape, field-context mapping, token grammar,
match-finder strategy, aggregate default, or released profile identities.
Contextual Adaptive Huffman remains deferred until its four-MiB payload and
workspace memory gate is solved independently.
