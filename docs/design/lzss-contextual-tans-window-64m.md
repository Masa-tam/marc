# LZSS contextual tANS 64 MiB window

Status: private profile and streaming stages complete; public lifecycle remains closed.

## Purpose and identity

This design applies the existing single-state contextual tANS representation
to the reserved 64-MiB typed-token family. Its eventual exact Format 2 identity
is:

```text
dictionary algorithm/variant 2/6
context-model algorithm/variant 1/5
entropy algorithm/variant 5/2
```

The pair `2/6 + 1/5` remains inseparable. Entropy variant 2 retains table log
12, deterministic spreading, one state, reverse encode order, LSB-first
additional bits, the implicit bypass table, and its two-byte little-endian
initial state. No earlier identity, descriptor, initializer, selector, or
archive byte is widened or reinterpreted.

## Selected model and descriptor

Context variant 5 contains 4,598 flattened frequency entries, sixteen more
than variant 4. The all-dense compact records grow by exactly 32 bytes from
9,133 to 9,165 bytes. The unchanged 24-byte tANS prefix therefore produces an
exact maximum descriptor size of 9,189 bytes.

The fixed table bank remains 32 tables times 4,096 states: 31 Symbol contexts
and one implicit bypass context. It contains exactly 131,072 entries in both
directions. Encoder entries remain `uint16_t`; supported-layout decoder entries
remain four bytes. No context, table, state, or serialized selector is added.

Descriptor capacity is now 9,189 bytes and the internal descriptor parser and
serializer admit variant 5. Tests require its canonical 4,598-entry grammar,
the exact all-dense maximum, one-byte-short aggregate rejection, and reciprocal
variant-4 rejection. Internal stream-header validation, serialization, and
parsing now admit exact triple `2/6 + 1/5 + 5/2`. Frame preflight selects the
9,189-byte descriptor ceiling and the variant-5 `8F`/`36T` count bounds before
descriptor, table, token, or raw publication.

The existing selected-layout table builder and single-state coding core now
carry variant 5 without a separate production branch. Direct tests construct
the maximum distance symbol 26 in context 23, encode and decode a 26-bit
bypass value, and prove direct typed-token output is byte-identical to the
generic modeled-operation path. A boundary stream first establishes exactly
16,777,217 bytes of history and then codes distance 16,777,217; reciprocal
variant-4 decoding rejects it before changing caller-owned token output.

## Count and payload bounds

For a raw frame of `F` bytes, context variant 5 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 8F
decision_count <= 36*token_count
```

The conservative tANS ceiling remains twelve bits per decision plus the
two-byte initial state:

```text
payload_size <= ceil(8F*12/8) + 2
             = 12F + 2
complete_frame_size <= 12F + 9,255
```

The complete-frame constant contains the 64-byte frame header, 9,189-byte
maximum descriptor, and two-byte initial state. At `F = 67,108,864`, the
payload ceiling is 805,306,370 bytes and the complete-frame ceiling is
805,315,623 bytes. Both fit the existing 32-bit serialized payload and
descriptor-related fields. Earlier profiles retain their frozen ceilings.

## Workspace proof

On the supported 64-bit native layout, HashChain Exact encoding requires:

```text
raw frame                          67,108,864 bytes
67,108,864 typed tokens          805,306,368 bytes
131,072 uint16 encode entries        262,144 bytes
views total                      805,568,512 bytes
HashChain heads and links        268,959,744 bytes
complete encoded frame           805,315,623 bytes
aggregate                      1,946,952,743 bytes
```

BinaryTree Exact substitutes its 1,946,157,056-byte finder and raises the
aggregate to 3,624,150,055 bytes. Both fit an explicit four-GiB aggregate
policy. Runtime queries must use checked native extents, alignment, and the
selected finder's authoritative calculator; exact capacity must succeed and
one byte short must fail before publication.

Decoding requires:

```text
131,072 decode entries                524,288 bytes
67,108,864 typed tokens          805,306,368 bytes
views total                      805,830,656 bytes
complete encoded frame           805,315,623 bytes
raw frame                         67,108,864 bytes
aggregate                      1,678,255,143 bytes
```

The future public helper applies frame, window, and distance 67,108,864;
block limit `8F = 536,870,912`; payload limit 805,306,370; 4,598 flattened
model entries; 131,072 entropy-table entries; and a four-GiB aggregate policy.
It preserves direction, original size, total-output policy, and the selected
Exact finder. Initializers remain 64 KiB, callers may tighten returned hard
limits, and stream fields never enlarge local policy.

## Staged implementation

1. **Complete:** expand compact descriptor analysis, parse/serialize storage,
   and exact variant-5 bounds without admitting an outer frame.
2. **Complete:** carry the selected layout through contextual tANS table
   construction and direct typed-token encode/decode tests.
3. **Complete:** admit exact triple `2/6 + 1/5 + 5/2` in private stream/header
   parsing, frame preflight, and complete-frame decoding while encoding remains
   closed.
4. **Complete:** admit complete-frame encoding with HashChain Exact and
   BinaryTree Exact; require their canonical frames to match.
   Exhaustive remains closed.
5. **Complete:** add checked profile/workspace calculation and one-byte
   streaming with exact and one-short aggregate tests.
6. Admit common public profile value 4 only for Contextual tANS.
7. Add exact CLI and dependency-free benchmark name
   `lzss-contextual-tans-64m` through the public lifecycle.
8. Extend bounded decoder fuzzing without profile-sized allocation.
9. Append one interoperability archive only after all preceding boundaries
   pass.

Each stage must retain all earlier bytes, reject crossed profiles before token
or raw publication, and keep every incomplete outward boundary closed.

## Required validation

- prove the 4,598-entry layout and 9,189-byte exact descriptor ceiling;
- accept `8F` and `36T` at equality and reject one above without overflow;
- prove table counts, payload, complete-frame, and all workspace ceilings at
  equality and one byte short;
- preserve HashChain/BinaryTree Exact token and completed-stream identity;
- decode the first newly reachable distance 16,777,217 and retain the shared
  distance-67,108,864 primitive proof;
- reject every reciprocal crossing with the four published tANS profiles;
- keep bounded fuzz frame, token, raw, output, and call storage independent of
  the selected 64-MiB maximum; and
- retain schema 54 unchanged until the complete public lifecycle passes.

## Deferred decisions

This design does not change tANS normalization, spreading, state count, table
log, bypass coding, match-finder serialization, initializer defaults, resource
helpers, or automatic limit policy. Contextual Blocked Huffman and Contextual
Adaptive Huffman retain independent descriptor, payload, memory, public, fuzz,
and interoperability proofs.
