# LZSS contextual rANS 16 MiB window

Status: private stream/header, frame preflight, and complete-frame decoder
implemented after the compact descriptor/model boundary.
Complete-frame encoding remains closed.

## Purpose and identity

This design applies canonical contextual rANS to the reserved 16-MiB typed-
token family. It uses exact Format 2 identity:

```text
dictionary algorithm/variant 2/5
context-model algorithm/variant 1/4
entropy algorithm/variant 4/3
```

No existing identity is widened or reinterpreted. Entropy variant 3 retains
its canonical scalar representation; only the selected context layout grows.
Crossed dictionary/context pairs and every other entropy identity remain
contradictory or unsupported.

## Selected model and descriptor

Context variant 4 contains 4,582 flattened frequency entries, sixteen more
than context variant 3. Its compact descriptor therefore grows by exactly 32
bytes, from 9,121 to 9,153 bytes. The 20-byte prefix, active-mask rules,
dense/sparse canonical choice, table log 12, total frequency 4,096, single
scalar state, byte renormalization, reverse encode order, and 8-byte little-
endian final state remain unchanged.

The decoder table bank remains 31 contexts times 4,096 states, or 126,976
`RansDecodeEntry` values. No additional context or state table is introduced.

## Count and payload bounds

For a raw frame of `F` bytes, shared context variant 4 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 34*token_count
```

The conservative scalar rANS payload ceiling remains two bytes per decision
plus the final state. Consequently this profile requires:

```text
payload_size <= 14F + 8
complete_frame_size <= 14F + 9,225
```

The complete-frame constant is the 64-byte frame header plus the 9,153-byte
maximum descriptor plus the 8-byte final state. Earlier profiles retain their
frozen descriptor and complete-frame ceilings.

## Workspace proof

At `F = 16,777,216`, the supported 64-bit native encoder layout is:

```text
raw frame                         16,777,216 bytes
16,777,216 typed tokens          201,326,592 bytes
HashChain heads and links         67,633,152 bytes
complete encoded frame           234,890,249 bytes
aggregate                        520,627,209 bytes
```

This is 16,243,703 bytes below a 512-MiB aggregate policy. Runtime sizing must
still use checked arithmetic, native object extents, and the authoritative
HashChain workspace query. Require default-policy failure where applicable,
one-byte-short failure, and exact-limit success without partial publication.

The supported decoder layout uses six-byte `RansDecodeEntry` values:

```text
126,976 rANS table entries           761,856 bytes
16,777,216 typed tokens          201,326,592 bytes
views total                      202,088,448 bytes
complete encoded frame           234,890,249 bytes
raw frame                         16,777,216 bytes
aggregate                        453,755,913 bytes
```

This is 83,114,999 bytes below 512 MiB. Decoder sizing must derive raw and
token extents from the caller's smaller frame/block limit, so a fixed-memory
fuzzer need not allocate a 16-MiB frame merely to select the identity.

The eventual public helper applies frame, window, and distance 16,777,216;
`7F = 117,440,512` decisions; payload 234,881,032; 4,582 flattened context
entries; 126,976 entropy decode entries; and aggregate buffered bytes to 512 MiB.
Initializers remain 64 KiB, profile choice remains explicit, and stream
fields never enlarge local hard limits.

## Staged implementation

1. Expand contextual rANS descriptor storage and canonical parse/serialize
   bounds for context variant 4 without admitting an outer frame (complete).
2. Admit exact triple `2/5 + 1/4 + 4/3` in private stream/header parsing and
   serialization, frame preflight, and complete-frame decoding (complete).
3. Admit complete-frame encoding while retaining later lifecycle boundaries.
4. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
5. Admit public C profile value 3 for contextual rANS, then add the explicit
   CLI and dependency-free benchmark name.
6. Extend bounded decoder fuzzing and append one interoperability archive only
   after every preceding boundary passes.

Each stage preserves old serialized bytes and keeps incomplete public
boundaries closed. HashChain Exact remains the production match finder;
private match-finder experiments do not change the stream identity.

## Deferred decisions

This design does not change rANS normalization, interleave states, add SIMD,
infer a profile from stream sizes, or alter the global initializer defaults.
The 512-MiB policy belongs only to this explicit backend profile. Contextual
tANS and both Huffman backends retain independent format and memory proofs.
