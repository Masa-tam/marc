# LZSS contextual rANS 4 MiB window

Status: accepted staged design after completion of the four-MiB Contextual
Dynamic Range vertical path. The descriptor/model, private complete-frame,
profile, and one-byte streaming boundaries are implemented; public admission
remains closed.

## Purpose and identity

This design admits canonical contextual rANS as the second entropy backend for
the existing four-MiB typed-token family. It uses exact Format 2 identity:

```text
dictionary algorithm/variant 2/4
context-model algorithm/variant 1/3
entropy algorithm/variant 4/3
```

No existing identity is widened or reinterpreted. Entropy variant 3 remains
the canonical scalar contextual rANS representation; only its selected field-
context layout expands. Crossed dictionary/context pairs and every other
entropy identity remain contradictory or unsupported at this boundary.

## Selected model and descriptor

Context variant 3 contains 4,566 flattened frequency entries, sixteen more
than variant 2. The compact-model descriptor therefore grows by exactly 32
bytes, from 9,089 to 9,121 bytes. The 20-byte descriptor prefix, active-mask
rules, dense/sparse canonical choice, table log 12, total frequency 4,096,
single scalar state, byte renormalization, reverse encode order, and 8-byte
little-endian final state remain unchanged.

The fixed decoder table bank remains 31 contexts times 4,096 states, or
126,976 `RansDecodeEntry` values. No additional context or state table is
introduced.

## Count and payload bounds

For a raw frame of `F` bytes, shared context variant 3 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 32*token_count
```

The conservative scalar rANS payload ceiling remains two bytes per decision
plus the final state. Consequently this profile requires:

```text
payload_size <= 14F + 8
complete_frame_size <= 14F + 9,193
```

The complete-frame constant is the 64-byte frame header plus the 9,121-byte
maximum descriptor plus the 8-byte final state. Older contextual rANS profiles
retain `12F + 8` and their frozen 9,025/9,089-byte descriptor ceilings.

## Workspace proof

At the reference `F = 4,194,304` frame/window, the supported 64-bit native
encoder layout is:

```text
raw frame                         4,194,304 bytes
4,194,304 typed tokens           50,331,648 bytes
HashChain heads and links         17,301,504 bytes
complete encoded frame            58,729,449 bytes
aggregate                        130,556,905 bytes
```

This is 3,660,823 bytes below the unchanged 128-MiB default. The encoder must
use native `sizeof` values and the checked HashChain workspace calculator; the
numeric result is a tested supported-layout boundary, not a replacement for
runtime calculation. Require default success, one-byte-short failure, and
exact-limit success.

The supported decoder layout uses six-byte `RansDecodeEntry` values:

```text
126,976 rANS table entries          761,856 bytes
4,194,304 typed tokens           50,331,648 bytes
views total                      51,093,504 bytes
complete encoded frame           58,729,449 bytes
raw frame                         4,194,304 bytes
aggregate                        114,017,257 bytes
```

This is 20,200,471 bytes below 128 MiB. Decoder sizing must continue to derive
raw/token extents from the caller's smaller frame/block limit, so a bounded
fuzzer need not allocate a four-MiB frame merely to select the identity.

The library defaults for maximum frame size, compressed payload, LZ distance,
entropy table entries, and internal buffered bytes are otherwise sufficient.
A caller selecting a full four-MiB frame must explicitly raise the one-MiB
`max_block_size` default to four MiB. No default is silently increased.

## Staged implementation

1. Expand contextual rANS descriptor storage and canonical parse/serialize
   bounds for context variant 3 without admitting an outer frame.
2. Admit exact triple `2/4 + 1/3 + 4/3` in stream/frame preflight and complete-
   frame decoding, then encoding.
3. Add checked profile/workspace calculation and one-byte streaming with exact
   and one-short aggregate tests.
4. Allow public C selector value 2 only for contextual rANS, then add the
   explicit CLI and dependency-free benchmark name.
5. Extend the bounded decoder fuzzer and append one interoperability archive
   only after all preceding boundaries pass.

Each stage preserves old serialized bytes and keeps incomplete public
boundaries closed. HashChain Exact remains the production match finder;
complete and sparse HashTree implementations remain private experiments.

## Deferred decisions

This design does not select a new match finder, change rANS normalization,
interleave states, add SIMD, raise the aggregate default, or infer profiles
from stream sizes. Contextual tANS and both Huffman backends retain separate
memory and format admission work.
