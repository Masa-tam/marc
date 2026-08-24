# LZSS contextual tANS 16 MiB window

Status: private complete-frame encoder and decoder implemented after project
version 0.4.0. Profile, streaming, public, tooling, fuzzing, and
interoperability boundaries remain closed.

## Purpose and identity

This design applies the existing single-state contextual tANS representation
to the reserved 16-MiB typed-token family. Its exact Format 2 identity is:

```text
dictionary algorithm/variant 2/5
context-model algorithm/variant 1/4
entropy algorithm/variant 5/2
```

No completed identity is widened or reinterpreted. Entropy variant 2 retains
table log 12, deterministic spreading, one state, reverse encode order,
LSB-first additional bits, the implicit bypass table, and its two-byte little-
endian initial state. Crossed dictionary/context pairs and every other entropy
identity remain contradictory or unsupported.

## Selected model and descriptor

Context variant 4 contains 4,582 flattened frequency entries, sixteen more
than context variant 3. Its compact records grow by exactly 32 bytes. With the
unchanged 24-byte tANS prefix, it grows from 9,125 to 9,157 bytes.

The fixed table bank remains 32 tables times 4,096 states: 31 Symbol contexts
and one implicit bypass context. It contains exactly 131,072 entries in both
directions. Encoder entries remain `uint16_t`; supported-layout decoder entries
remain four bytes. No context, table, state, or serialized selector is added.

## Count and payload bounds

For a raw frame of `F` bytes, context variant 4 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 34*token_count
```

The conservative ceiling remains twelve bits per decision plus the two-byte
initial state:

```text
payload_size <= ceil(7F*12/8) + 2
             = ceil(21F/2) + 2
complete_frame_size <= ceil(21F/2) + 9,223
```

The complete-frame constant contains the 64-byte frame header, 9,157-byte
maximum descriptor, and two-byte initial state. Earlier contextual tANS
profiles retain their frozen descriptor and complete-frame ceilings.

## Workspace proof

At `F = 16,777,216`, the supported 64-bit native encoder layout is:

```text
raw frame                         16,777,216 bytes
16,777,216 typed tokens          201,326,592 bytes
131,072 uint16 encode entries        262,144 bytes
HashChain heads and links         67,633,152 bytes
views total                      269,221,888 bytes
complete encoded frame           176,169,991 bytes
aggregate                        462,169,095 bytes
```

This is 74,701,817 bytes below an explicit 512-MiB aggregate policy. Runtime
sizing must still use checked arithmetic, native object extents and alignment,
and the authoritative HashChain workspace query. Exact capacity must succeed;
one byte short must fail before allocation or publication.

The supported decoder layout is:

```text
131,072 decode entries                524,288 bytes
16,777,216 typed tokens          201,326,592 bytes
views total                      201,850,880 bytes
complete encoded frame           176,169,991 bytes
raw frame                         16,777,216 bytes
aggregate                        394,798,087 bytes
```

This is 142,072,825 bytes below 512 MiB. Decoder sizing must derive raw and
token extents from the caller's smaller frame/block limit. Selecting this
identity in a bounded decoder fuzzer must not allocate a 16-MiB history.

The eventual additive public helper applies frame, window, and distance
16,777,216; block limit `7F = 117,440,512`; payload limit 176,160,770;
4,582 flattened model entries; 131,072 entropy-table entries; and a 512-MiB
aggregate policy. It preserves direction, original size, and total-output
policy. Initializers remain 64 KiB and stream fields never enlarge local hard
limits.

## Staged implementation

1. Expand descriptor capacity and canonical compact-model parsing and
   serialization for context variant 4 without admitting an outer frame
   (complete).
2. Carry the selected layout through contextual tANS coding and direct typed-
   token encode/decode tests (complete).
3. Admit exact triple `2/5 + 1/4 + 5/2` in stream/header validation, frame
   preflight, and complete-frame decoding while keeping encoding explicitly
   closed (complete).
4. Admit complete-frame encoding for the exact triple (complete).
5. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
6. Admit public C profile value 3 only for contextual tANS, then add the
   explicit CLI and dependency-free benchmark names.
7. Extend bounded decoder fuzzing without profile-sized allocation.
8. Append exactly one interoperability archive only after all preceding
   boundaries pass.

Every stage preserves existing stream bytes and keeps later public boundaries
closed. HashChain Exact remains the production match finder; BinaryTree and
HashTree variants remain private experiments.

## Deferred decisions

This design does not change tANS normalization, spreading, state count, table
log, bypass coding, match-finder strategy, initializer defaults, or automatic
limit policy. Contextual Blocked Huffman and Contextual Adaptive Huffman retain
independent descriptor, payload, memory, public, fuzz, and interoperability
proofs.
