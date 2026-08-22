# LZSS contextual Adaptive Huffman 4 MiB window

Status: accepted staged design after completion of the four-MiB Dynamic Range,
canonical contextual rANS, contextual tANS, and Contextual Blocked Huffman
vertical paths. Implementation has not started.

## Purpose and exact identity

This design admits Contextual Adaptive Huffman as the fifth entropy backend
for the four-MiB typed-token LZSS family. It uses exact Format 2 identity:

```text
dictionary algorithm/variant 2/4
context-model algorithm/variant 1/3
entropy algorithm/variant 1/2
```

No existing identity is widened or reinterpreted. Entropy variant 2 retains
the fixed 16-byte descriptor, 31 independent reset-per-frame FGK trees, NYT
rules, sibling-property updates, forward LSB-first payload, and no-rescale
frame policy. Only the externally selected field-context layout changes the
distance alphabet from 21 to 23 symbols.

Crossed dictionary/context pairs, entropy variants, and public window
selectors are contradictory or unsupported. Match-finder strategy remains
encoder-local and is not serialized.

## Selected model bank and bounds

Context variant 3 contains exactly 4,566 symbol slots. One FGK tree per Symbol
context therefore requires:

```text
symbol entries = 4,566
node entries   = 2*4,566 + 31 = 9,163
entropy entries                     = 13,729
```

On the supported 64-bit layout, `AdaptiveHuffmanNode` is 16 bytes and each
symbol index is two bytes. The fixed model-bank storage is therefore 146,608
node bytes plus 9,132 symbol bytes. Selection must precede model partitioning,
payload traversal, allocation, and mutation. Existing 64-KiB and one-MiB
model-bank extents and bytes remain unchanged.

For raw frame size `F`, context variant 3 retains:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 32*token_count
```

The conservative Adaptive Huffman payload proof also remains unchanged:

```text
payload_size <= ceil(267F/8)
```

The 267-bit raw-byte bound is three bits for the worst new token-kind operation
plus 264 bits for the worst new 256-symbol Literal operation. Matches consume
at least five raw bytes and do not exceed this per-byte bound. This proof is
deliberately conservative but already established, deterministic, and safe;
the four-MiB profile does not replace it with an empirical bound.

## Four-MiB memory proof

For `F = 4,194,304`, the exact conservative payload and complete-frame
ceilings are:

```text
payload                              139,984,896 bytes
64-byte header + 16-byte descriptor          80 bytes
complete encoded frame              139,984,976 bytes
```

The encoder owns these extents concurrently:

```text
raw frame                              4,194,304 bytes
4,194,304 typed tokens                50,331,648 bytes
9,163 FGK nodes                          146,608 bytes
4,566 symbol indices                       9,132 bytes
HashChain heads and links             17,301,504 bytes
views total                            67,788,892 bytes
complete encoded frame               139,984,976 bytes
aggregate                            211,968,172 bytes
```

The decoder needs:

```text
9,163 FGK nodes                          146,608 bytes
4,566 symbol indices                       9,132 bytes
4,194,304 typed tokens                50,331,648 bytes
views total                            50,487,388 bytes
complete encoded frame               139,984,976 bytes
raw frame                              4,194,304 bytes
aggregate                            194,666,668 bytes
```

Both aggregates fit a 256-MiB hard limit with 56,467,284 and 73,768,788 bytes
of headroom respectively. They do not fit the library's default 128-MiB
aggregate limit, and the payload does not fit the default 64-MiB compressed-
payload limit.

## Opt-in limit policy

The library defaults remain unchanged. Profile selection is applied explicitly
through this additive C helper:

```c
marc_status
marc_lzss_contextual_adaptive_huffman_config_apply_window_profile(
    marc_lzss_contextual_adaptive_huffman_config* config,
    marc_lzss_contextual_window_profile profile);
```

The caller first initializes the configuration, then applies one exact window
profile, then may override individual values before querying workspaces. The
helper allocates no memory. It updates a private copy and publishes the whole
configuration only after pointer, structure size, ABI version, direction,
reserved fields, and profile value validate. Failure leaves every caller byte
unchanged.

The helper overwrites `window_profile`, frame/window size, minimum/maximum
match length, frame/block/payload/aggregate limits, LZ distance/match limits,
and entropy-entry limit. It preserves direction, original size, total-output
limit, ABI metadata, and reserved fields. For the four-MiB profile it applies:

```text
max_frame_size               4,194,304 bytes
max_block_size               4,194,304 raw frame bytes
max_compressed_payload_size  139,984,896 bytes
max_entropy_table_entries         13,729 entries
max_internal_buffered_bytes  268,435,456 bytes
maximum LZ distance           4,194,304 bytes
```

The 64-KiB and one-MiB selections receive their already documented coherent
application presets through the same helper. `config_init()` retains its
64-KiB default and does not select a larger profile. A caller may deliberately
raise or lower any applied value afterward; the workspace query revalidates
the final configuration and returns `MARC_STATUS_LIMIT_EXCEEDED` when it no
longer satisfies the profile.

The concrete profile calculator derives its directional requirement and
rejects an aggregate limit one byte below that result. The CLI and benchmark
use the same canonical preset for their explicit `-4m` name. Merely changing
one field, encountering a wider identity, or using the generic library
defaults must not raise any limit automatically.

This bounded opt-in is preferred to inventing a tighter average-case payload
formula. A later bounded streaming redesign would require a distinct format
variant if it changes frame reset, atomic publication, or payload layout.

## Staged implementation

1. Select context variant 3 in the model bank and profile calculator. Prove
   9,163/4,566 model extents, exact directional aggregates, exact-limit
   success, one-byte-short failure, and unchanged older selections.
2. Carry the immutable selection through FGK operation coding and direct LZSS
   typed-token composition. Require a distance-4,194,304 token-boundary vector
   and reciprocal atomic rejection by older layouts.
3. Admit complete-frame identity `2/4 + 1/3 + 1/2`, then add selected profile
   and one-byte streaming lifecycles. A complete frame must contain a real
   Match beyond one MiB without exceeding its four-MiB raw extent.
4. Admit existing ABI-1 value `MARC_LZSS_CONTEXTUAL_WINDOW_4M` only for this
   backend, add the atomic profile-application helper, then add exact CLI name
   `lzss-contextual-adaptive-huffman-4m` through the same canonical preset.
5. Add the dependency-free benchmark and bounded dual-path decoder fuzzer.
   Fuzz identity and distance limits may widen, but raw/token storage and call
   count remain small and fixed.
6. Append exactly one interoperability archive after every earlier boundary
   passes. Preserve all older archive bytes and schema order.

Each stage leaves later outward surfaces closed. Exact-profile decoders reject
the other two window identities before raw publication. Encoder and decoder
hold one selected field layout for their complete lifetimes.

## Required validation

Require hand-checkable model selection, NYT, existing-symbol, and bypass
vectors; deterministic Exhaustive and HashChain Exact agreement; all required
binary input classes; one-byte and mixed chunking; terminal stability; exact
and one-short workspaces; malformed descriptor, count, padding, truncation,
overflow, alias, and trailing-data rejection; reciprocal three-profile public
rejection; atomic helper success/failure for all known and unknown profiles;
preservation of direction, original size, and total-output limit; deliberate
post-helper override and workspace-query rejection; bounded sanitizer fuzzing;
and four-direction interoperability.

The maximum-distance typed-token vector is independent of the complete-frame
vector because a Match beginning after four MiB of history cannot also fit
inside a frame whose raw extent is at most four MiB. The complete-frame vector
instead proves a semantically necessary distance strictly greater than one
MiB, while boundary tests prove class 22 and distance 4,194,304 directly.

## Deferred decisions

This design does not change FGK tree arithmetic, reset policy, descriptor,
payload representation, context mapping, typed-token grammar, match-finder
strategy, ABI extent, released identities, or library defaults. A tighter
provable payload bound, incremental entropy publication, larger default, or
window beyond four MiB remains a separate design and safety review.
