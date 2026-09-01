# LZSS contextual rANS 64 MiB window

Status: private stream-header parsing, frame preflight, and complete-frame
decoding implemented; serialization, encoding, streaming, public, tooling,
fuzzing, and interoperability boundaries remain closed.

## Purpose and identity

This design applies canonical scalar contextual rANS to the reserved 64-MiB
typed-token family. Its eventual exact Format 2 identity is:

```text
dictionary algorithm/variant 2/6
context-model algorithm/variant 1/5
entropy algorithm/variant 4/3
```

The pair `2/6 + 1/5` remains inseparable. Supporting its internal compact
model does not yet admit the complete entropy triple. Every earlier identity,
descriptor, archive byte, initializer, and public selector retains its meaning.

## Selected model and descriptor

Context variant 5 contains 4,598 flattened frequency entries, sixteen more
than variant 4. The all-dense compact records grow by exactly 32 bytes from
9,133 to 9,165 bytes, and the 20-byte rANS prefix produces an exact maximum
descriptor size of 9,185 bytes.

The active mask, dense/sparse canonical choice, table log 12, total frequency
4,096, one scalar state, byte renormalization, reverse encode order, and
eight-byte little-endian final state remain unchanged. The decoder table bank
also remains 31 contexts times 4,096 states, or 126,976 six-byte entries.

Variant 5 descriptor analysis, serialization, parsing, exact-limit handling,
and reciprocal variant-4 rejection are implemented independently. The private
stream-header parser, frame preflight, and complete-frame decoder now admit
exact triple `2/6 + 1/5 + 4/3`. The first newly reachable distance
16,777,217 decodes exactly after bounded overlap-built history. Stream-header
serialization, complete-frame encoding, streaming, profiles, and every public
surface remain closed.

## Count and payload bounds

For a raw frame of `F` bytes, shared context variant 5 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 8F
decision_count <= 36*token_count
```

The scalar rANS payload ceiling remains two renormalization bytes per decision
plus the final state. The future complete frame therefore requires:

```text
payload_size <= 16F + 8
complete_frame_size <= 16F + 9,257
```

At `F = 67,108,864`, these are 1,073,741,832 payload bytes and
1,073,751,081 complete-frame bytes. Both remain representable by the existing
32-bit serialized payload field. Earlier descriptor and frame ceilings remain
frozen.

## Workspace proof

On the supported 64-bit native layout, HashChain encoding requires:

```text
raw frame                          67,108,864 bytes
67,108,864 typed tokens           805,306,368 bytes
HashChain heads and links          268,959,744 bytes
complete encoded frame           1,073,751,081 bytes
aggregate                        2,215,126,057 bytes
```

BinaryTree Exact substitutes its 1,946,157,056-byte finder and raises the
aggregate to 3,892,323,369 bytes. Both fit an explicit four-GiB aggregate
policy. Runtime queries must still use checked arithmetic and the selected
finder's authoritative calculator; one-byte-short aggregate limits fail before
publication.

Decoding requires:

```text
126,976 rANS table entries             761,856 bytes
67,108,864 typed tokens            805,306,368 bytes
views total                         806,068,224 bytes
complete encoded frame            1,073,751,081 bytes
raw frame                            67,108,864 bytes
aggregate                          1,946,928,169 bytes
```

The future public helper may apply frame/window/distance 67,108,864,
`8F = 536,870,912` decisions, payload 1,073,741,832, 4,598 model entries,
126,976 entropy table entries, and a four-GiB aggregate policy. Initialization
must remain 64 KiB, application must be explicit, and stream metadata must
never enlarge local limits.

## Staged implementation

1. Expand compact descriptor analysis, parse/serialize storage, and exact
   variant-5 bounds without admitting an outer frame (complete).
2. Admit exact triple `2/6 + 1/5 + 4/3` in private stream/header parsing,
   frame preflight, and complete-frame decoding (complete).
3. Admit complete-frame encoding with both explicit Exact finders.
4. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
5. Admit common public profile value 4 for contextual rANS.
6. Add exact CLI and benchmark names.
7. Extend bounded decoder fuzzing without profile-sized allocation.
8. Append one interoperability archive after every preceding boundary passes.

Each stage must retain old bytes, prove malformed-input atomicity, and keep
incomplete outward boundaries closed.

## Deferred decisions

This design does not change rANS normalization, add interleaving or SIMD,
serialize the match finder, infer limits from the stream, add a resource
helper, or alter defaults. tANS and both Huffman backends retain separate
64-MiB format and memory proofs.
