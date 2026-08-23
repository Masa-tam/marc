# LZSS contextual 16 MiB window

Status: shared dictionary/context primitives implemented after project
version 0.4.0. No entropy backend, public selector, CLI profile, or
interoperability archive is admitted yet.

## Purpose

This document defines the next candidate typed-token LZSS contextual family.
It raises the maximum match distance and reference frame/window size from
4,194,304 to 16,777,216 bytes while retaining maximum match length 258.

Sixteen MiB is the next deliberate boundary rather than an arbitrary larger
number. It is four times the completed profile, remains within distance class
24, preserves the common `7F` decision ceiling, and permits the first
conservative Dynamic Range encoder proof to fit under an explicit one-GiB
aggregate policy. A 64-MiB jump would require class 26 and up to 36 decisions
for a minimum-length Match, exceeding `7F`; it therefore needs a separate
future design.

The change is decoder-visible and MUST NOT reinterpret any completed profile.
Match-finder strategy remains encoder-local and is not serialized.

## Decoder-visible identity

Format 2.0 reserves this exact pair:

```text
dictionary algorithm ID 2, dictionary variant 5
context-model algorithm ID 1, context variant 4
```

Variants 5 and 4 MUST occur together. Crossed pairs are contradictory and
must fail after the complete stream header and before frame allocation. The
candidate entropy backends retain their established identities:

| Backend | Entropy algorithm/variant |
|---|---:|
| Dynamic Range | `3/2` |
| canonical contextual rANS | `4/3` |
| contextual tANS | `5/2` |
| Contextual Blocked Huffman | `2/2` |
| Contextual Adaptive Huffman | `1/2` |

Reserving the pair admits none of these triples. Every backend needs an exact
descriptor, payload, workspace, validator, encoder, decoder, C lifecycle,
tooling, fuzz, and interoperability proof before publication.

## Dictionary variant 5

Variant 5 retains Literal/Match tokens, greedy longest-match selection,
nearest-distance tie breaking, the beneficial-match rule, overlap copying,
per-frame reset, and maximum match length 258. Its parameter region is:

```text
minimum_match_length = 5
5 <= maximum_match_length <= 258
1 <= window_size <= 16,777,216 bytes
```

The reference profile uses a 16,777,216-byte frame and window. A Match cannot
cross the current frame's reconstructed prefix. HashChain Exact is the
initial production strategy and Exhaustive remains the small-input oracle.
BinaryTree, complete HashTree, and sparse HashTree remain private experiments
until fresh 16-MiB corpus evidence justifies a strategy change.

Because the reference profile resets at that frame boundary, its greatest
reachable distance is `16,777,216 - minimum_match_length`, not the inclusive
window parameter ceiling. The shared variant nevertheless defines class 24
and validates the full 16,777,216-byte distance for a caller-supplied frame
larger than its window. The hand vector uses that larger test frame; it does
not imply cross-frame history or widen the future fixed profile.

## Context variant 4

Context IDs and selection rules remain unchanged. Only the eight distance
alphabets expand:

| Context IDs | Count | Alphabet | Meaning |
|---:|---:|---:|---|
| 0..2 | 3 | 2 | token kind |
| 3..19 | 17 | 256 | Literal value |
| 20..22 | 3 | 8 | match-length class |
| 23..30 | 8 | 25 | match-distance class |

The flattened Symbol-model layout contains exactly 4,582 entries:

```text
3*2 + 17*256 + 3*8 + 8*25 = 4,582
```

For distance `D`:

```text
distance_class = floor(log2(D))       # 0..24
distance_extra = D - 2^distance_class
```

Distance 16,777,216 uses class 24 and a 24-bit zero extra. One Match has at
most three modeled symbols, seven length-extra bits, and 24 distance-extra
bits: 34 decisions for at least five raw bytes. The common checked bounds are:

```text
token_count <= F
event_count <= 2F
decision_count <= 7F
decision_count <= 34*token_count
```

The earlier variants retain their frozen alphabet and per-token limits.

## Aggregate and hard-limit policy

Profile choice remains explicit. Initializers retain the completed 64-KiB
default, stream fields never enlarge local limits automatically, and each
backend helper applies only its proven 16-MiB envelope. Caller-specific
direction, original size, and total-output policy remain preserved. A caller
may tighten a returned hard limit, but the authoritative workspace query must
then succeed before allocation.

The one-GiB value below is an explicit application profile, not a new library
default and not a serialized promise. Backend-specific calculations may use a
smaller proven aggregate; they must not inherit one GiB merely because the
shared dictionary identity exists.

For Dynamic Range on the supported 64-bit layout, `F = 16,777,216` gives:

```text
raw frame                         16,777,216 bytes
16,777,216 typed tokens          201,326,592 bytes
33,554,432 modeled operations    536,870,912 bytes
HashChain heads and links         67,633,152 bytes
complete encoded frame           234,881,109 bytes
aggregate                      1,057,488,981 bytes
```

The payload ceiling remains `14F+5 = 234,881,029` and the complete-frame
ceiling is `14F+85 = 234,881,109`. The aggregate leaves 16,252,843 bytes below
one GiB. A limit one byte below the calculated requirement must fail before
allocation or mutation.

The corresponding conservative decoder holds the complete encoded frame,
raw frame, and typed-token staging simultaneously:

```text
234,881,109 + 16,777,216 + 201,326,592 = 452,984,917 bytes
```

This fits 512 MiB, but the backend profile must still set payload and
aggregate limits explicitly and validate every sum and conversion. rANS,
tANS, Blocked Huffman, and Adaptive Huffman receive independent calculations;
no estimate in this shared document admits them.

## Required tests before any backend admission

- hand-check distance classes 0, 22, 23, and 24;
- reconstruct an exact distance-16,777,216 Match in an explicitly larger
  primitive-test frame without storing sixteen MiB of literal test source;
- reject variant-5/context-variant-4 crossings with every older pair;
- prove `7F` and `34T` count limits at equality and one above;
- verify every workspace extent at exact capacity and one byte short;
- preserve atomic failure for payload, model, aggregate, and output limits;
- prove old 64-KiB, one-MiB, and four-MiB archive bytes are unchanged;
- benchmark HashChain and any private candidate on the verified external
  Silesia Corpus before changing the production strategy;
- keep fuzz input and output fixed-memory even when admitting the 16-MiB
  identity; and
- append an interoperability archive only after the complete public surface
  is admitted.

## Shared primitive implementation

The repository now owns dictionary variant 5, context variant 4, the
25-symbol distance alphabets, 4,582-entry offsets, `24/34/7` checked layout
limits, and the exact pair selector. Typed-token validation admits a window no
larger than 16,777,216 bytes. The hand vector builds the required prefix from
bounded overlap Matches and verifies a class-24 symbol, 24 zero bypass bits,
34 added decisions, inversion, and rejection by variant 3.

The current Dynamic Range stream parser and serializer continue to reject
dictionary variant 5 before publication. Compact rANS/tANS model paths also
return their stable unsupported-context error for context variant 4. Internal
frequency storage has enough capacity for the shared layout, preventing a
reserved value from becoming an out-of-bounds access while backend-specific
descriptors remain unimplemented.

## Staged implementation order

1. shared dictionary/context constants, layouts, validators, and hand vectors
   (complete);
2. Dynamic Range decoder preflight and complete-frame decode;
3. Dynamic Range encoder, exact workspace query, and streaming lifecycle;
4. Dynamic Range C helper, CLI, benchmark, bounded fuzzing, and schema entry;
5. canonical contextual rANS;
6. contextual tANS;
7. Contextual Blocked Huffman;
8. Contextual Adaptive Huffman;
9. only then evaluate whether 16 MiB evidence justifies a later 64-MiB design.

Every stage preserves existing identities, bytes, defaults, and deterministic
selection. No stage is complete from a one-shot round trip alone.
