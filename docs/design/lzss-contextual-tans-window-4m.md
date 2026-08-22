# LZSS contextual tANS 4 MiB window

Status: implementation in progress; descriptor, coding-core, direct typed-
token, complete-frame, private profile, and streaming boundaries complete;
every public boundary remains closed.

## Purpose and identity

This design admits contextual tANS as the third entropy backend for the
existing four-MiB typed-token family. It uses exact Format 2 identity:

```text
dictionary algorithm/variant 2/4
context-model algorithm/variant 1/3
entropy algorithm/variant 5/2
```

No existing identity is widened or reinterpreted. Entropy variant 2 retains
the established single-state contextual tANS representation; only its selected
field-context layout expands. Crossed dictionary/context pairs and every other
entropy identity remain contradictory or unsupported at this boundary.

## Selected model and descriptor

Context variant 3 contains 4,566 flattened frequency entries, sixteen more
than variant 2. The compact-model records therefore grow by exactly 32 bytes,
and the 24-byte tANS descriptor prefix produces a maximum descriptor size of
9,125 bytes instead of 9,093.

The active-context mask, dense/sparse canonical choice, table log 12, total
frequency 4,096, deterministic spreading, single state, reverse encode order,
LSB-first additional bits, implicit bypass table, and two-byte little-endian
initial state remain unchanged. The descriptor must be selected from the
validated context variant; its byte length never infers the variant.

The fixed table bank remains 32 tables times 4,096 states: 31 Symbol contexts
and one implicit bypass context. It contains 131,072 entries in both
directions. No additional context, table, or state is introduced.

## Count and payload bounds

For a raw frame of `F` bytes, shared context variant 3 requires:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 32*token_count
```

The conservative tANS ceiling remains twelve bits per decision plus the
two-byte initial state. Consequently this profile requires:

```text
payload_size <= ceil(7F*12/8) + 2
             = ceil(21F/2) + 2
complete_frame_size <= ceil(21F/2) + 9,191
```

The complete-frame constant contains the 64-byte frame header, 9,125-byte
maximum descriptor, and two-byte initial state. Older contextual tANS profiles
retain `9F + 2` payloads and their frozen 9,029/9,093-byte descriptor ceilings.

## Workspace proof

At the reference `F = 4,194,304` frame/window, the supported 64-bit native
encoder layout is:

```text
raw frame                          4,194,304 bytes
4,194,304 typed tokens            50,331,648 bytes
131,072 uint16 encode entries        262,144 bytes
HashChain heads and links          17,301,504 bytes
views total                        67,895,296 bytes
complete encoded frame             44,049,383 bytes
aggregate                         116,138,983 bytes
```

This is 18,078,745 bytes below the unchanged 128-MiB default. Runtime sizing
must use native `sizeof` values, alignment, and the checked HashChain workspace
calculator; these recorded extents are a tested supported-layout boundary,
not a serialized ABI. Require default success, one-byte-short failure, and
exact-limit success.

The supported decoder layout uses four-byte `TansDecodeEntry` values:

```text
131,072 decode entries                524,288 bytes
4,194,304 typed tokens             50,331,648 bytes
views total                        50,855,936 bytes
complete encoded frame             44,049,383 bytes
raw frame                           4,194,304 bytes
aggregate                          99,099,623 bytes
```

This is 35,118,105 bytes below 128 MiB. Decoder sizing must continue to derive
raw/token extents from the caller's smaller frame limit, so bounded fuzzing
does not allocate a four-MiB frame merely to select the identity.

The existing defaults for compressed payload, LZ distance, entropy table
entries, and internal buffered bytes admit these extents. A full profile sets
`max_frame_size` to four MiB and raises the common block/decision limit to
`7F = 29,360,128`. No library default is silently increased.

## Staged implementation

1. **Complete.** Expand contextual tANS descriptor capacity and canonical
   parse/serialize bounds for context variant 3 without admitting an outer
   frame.
2. **Complete.** Carry the selected layout through the Contextual tANS coding
   core and direct typed-token encode/decode boundary.
3. **Complete.** Carry the selected layout through stream/frame preflight,
   complete-frame decoding, and then complete-frame encoding.
4. **Complete.** Add checked profile/workspace calculation and one-byte
   streaming with exact and one-short aggregate tests.
5. Allow public C selector value 2 only for contextual tANS, then add the
   explicit CLI and dependency-free benchmark name.
6. Extend the bounded decoder fuzzer and append one interoperability archive
   only after all preceding boundaries pass.

Each stage preserves old serialized bytes and keeps incomplete public
boundaries closed. HashChain Exact remains the production match finder;
complete and sparse HashTree implementations remain private experiments.

## Deferred decisions

This design does not change tANS normalization, spreading, state count, table
log, bypass coding, match finder, aggregate default, or profile selection.
Contextual Blocked Huffman and Contextual Adaptive Huffman retain separate
memory and format admission work.
