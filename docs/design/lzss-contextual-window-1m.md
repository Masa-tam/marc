# LZSS contextual 1 MiB window

Status: accepted design after project version 0.3.0. Implementation remains
staged and MUST NOT begin until the decoder-visible reservations and validation
vectors below are complete.

## Purpose

The existing typed-token LZSS contextual family limits match distance to
65,536 bytes. This document defines an additive family whose window may reach
1,048,576 bytes while preserving the existing maximum match length of 258
bytes. The wider history is intended to improve compression when useful
repetition is separated by more than 64 KiB.

The extension changes dictionary and context-model value ranges. It is not an
encoder-only match-finder setting and MUST NOT reinterpret an existing stream.
Existing dictionary variant 2 plus context variant 1 remains frozen and must
decode and encode byte-identically.

## Decoder-visible identity

The extended family retains Format 2.0 and selects this exact pair:

```text
dictionary algorithm ID 2, dictionary variant 3
context-model algorithm ID 1, context variant 2
```

Dictionary variant 3 and context variant 2 are coupled. A decoder MUST reject
`2/3 + 1/1`, `2/2 + 1/2`, or any other unreserved pairing before frame
allocation. The feature flag, 16-byte dictionary parameter layout, 16-byte
context extension layout, frame header, little-endian integer rule, LSB-first
bypass rule, and frame-atomic publication contract remain unchanged.

The entropy algorithm and variant identify only the backend representation.
Each existing contextual backend variant may be composed with the new context
variant because the complete stream header also identifies the context layout.
The planned pairings are:

| Backend | Entropy algorithm/variant |
|---|---:|
| Dynamic Range | `3/2` |
| canonical contextual rANS | `4/3` |
| contextual tANS | `5/2` |
| Contextual Blocked Huffman | `1/2` |
| Contextual Adaptive Huffman | `2/2` |

This parameterization does not change any old tuple. A backend descriptor in a
`2/2 + 1/1` stream still has exactly the old 31-context, 4,518-entry meaning.
The new 4,550-entry meaning is admitted only after the stream parser accepts
`2/3 + 1/2`.

## Dictionary variant 3

Variant 3 retains variant 2's typed `Literal` and `Match` values,
deterministic greedy longest-match parse, nearest-distance tie break,
beneficial-match rule, overlap-copy semantics, empty history, and per-frame
reset. Its parameter region requires:

```text
minimum_match_length = 5
5 <= maximum_match_length <= 258
1 <= window_size <= 1,048,576 bytes
```

The reference profile uses a 1,048,576-byte frame and a 1,048,576-byte window.
The final frame may be shorter. A configured window larger than a particular
frame prefix creates no history: every Match distance must also be at most the
number of raw bytes already reconstructed in that frame. No token or match may
cross a frame boundary.

HashChain Exact remains the initial production match finder and Exhaustive
remains the private oracle. Match-finder strategy is not serialized. Bucket
count, chain representation, search effort, and later optimization may change
only if they retain the selected strategy's deterministic token contract.

## Context variant 2

Variant 2 retains variant 1's state: previous token kind and latest Literal,
both reset at every frame. Context IDs 0 through 30 and all selection rules are
unchanged. Only the distance-class alphabets expand:

| Context IDs | Count | Alphabet | Meaning |
|---:|---:|---:|---|
| 0..2 | 3 | 2 | token kind |
| 3..19 | 17 | 256 | Literal value |
| 20..22 | 3 | 8 | match-length class |
| 23..30 | 8 | 21 | match-distance class |

The flattened Symbol-model layout therefore contains exactly 4,550 entries:

```text
3*2 + 17*256 + 3*8 + 8*21 = 4,550
```

Length coding is unchanged. For distance `D`:

```text
distance_class = floor(log2(D))       # 0..20
distance_extra = D - 2^distance_class
```

The distance class is emitted in context `23 + length_class`. A nonzero class
is followed by exactly `distance_class` LSB-first bypass bits, so context
variant 2 permits bypass widths 1 through 20. A distance of 1,048,576 uses
class 20 and extra value zero. Variant 1 continues to reject classes above 16
and bypass widths above 16.

A Match produces at most five modeled events and 30 entropy decisions:
three Symbol decisions, at most seven length-extra bits, and at most twenty
distance-extra bits. The existing conservative frame bound remains valid:

```text
event_count <= 2 * raw_frame_size
decision_count <= 6 * raw_frame_size
```

Variant-specific validation also requires `decision_count <= 30 * token_count`.
All multiplication, offsets, counts, and reconstruction extents use checked
arithmetic.

## Backend consequences

Dynamic Range keeps its 16-byte descriptor, 31 independent models, maximum
model total 32,768, arithmetic, and finalization. Its model bank grows from
4,518 to 4,550 frequency counters.

Canonical contextual rANS and contextual tANS retain their dense/sparse model
record rules. With eight distance alphabets growing from 17 to 21 symbols, the
exact all-dense model maximum grows by 64 bytes. The corresponding descriptor
ceilings are 9,089 bytes for rANS and 9,093 bytes for tANS. Their context count
and decode-table entry counts remain unchanged; validators must select the
frequency-entry count and descriptor ceiling from context variant 2.

Contextual Blocked Huffman retains four shared field tables and optional
per-context overrides. Its distance-field alphabet becomes 21 and all exact
descriptor, table, code-length, payload, and workspace bounds must be derived
from the selected context layout before its format reservation is admitted.

Contextual Adaptive Huffman retains one reset FGK tree per Symbol context.
Variant 2 requires 4,550 symbol slots and `2*4,550 + 31 = 9,131` nodes across
the 31 trees. NYT, swap, rescale, payload, and descriptor rules remain the
backend variant's rules.

No backend may infer the layout from descriptor length alone. Stream identity
is validated first, then the selected context layout controls every alphabet,
frequency slice, table extent, tree extent, and descriptor bound.

## Limits and workspace policy

The existing default decoder limits already admit a 1 MiB frame and more than
a 1 MiB LZ distance, but every profile must validate its complete aggregate
before allocation or construction. Encoder admission includes raw frame,
typed tokens, exact HashChain workspace, entropy staging, serialized frame,
and alignment allowance. Decoder admission includes serialized frame, entropy
tables or trees, typed tokens, private raw reconstruction, and alignment.

The default 128 MiB aggregate internal-buffer limit is retained initially. A
backend whose conservative one-frame requirements do not fit must tighten its
workspace derivation or remain unimplemented; the common safety limit is not
silently raised to make a profile pass.

## Staged implementation order

Implementation proceeds one complete vertical path at a time:

1. context-layout selection and variant-pair preflight;
2. typed-token and context-model reference vectors and validators;
3. Dynamic Range complete-frame decoder, then encoder and streaming lifecycle;
4. canonical rANS;
5. tANS;
6. Contextual Blocked Huffman;
7. Contextual Adaptive Huffman;
8. public C lifecycle, CLI, benchmark, fuzzing, and interoperability admission
   for each completed backend.

Each stage must leave old variant output byte-identical. A backend is not
publicly named merely because shared dictionary and context primitives exist.
Public profile names and C declarations are reserved only when that backend's
format, bounds, decoder, encoder, malformed-input tests, and benchmark are
complete.

The first shared-groundwork stage is implemented internally. Typed-token
validation selects the 64 KiB or 1 MiB parameter ceiling explicitly, and the
field-context layer owns one validated layout for each reserved pair. Existing
callers default to the frozen variants. The selector accepts only
`2/2 + 1/1` and `2/3 + 1/2`.

The first Dynamic Range decoder slice is also implemented internally. Its
stream parser admits the exact new pair, its frame preflight derives table and
decision bounds from that pair, and its raw range decoder, typed-token inverse,
reconstructor, complete-frame decoder, and streaming decoder carry the
selected layout without inferring it from a descriptor. One-byte input and
output chunking is covered. This slice does not yet provide the new encoder,
public profile, workspace factory, CLI, benchmark, fuzz target, or
interoperability archive; those remain later steps in the same vertical path.

The matching internal encoder slice is implemented next. Typed-token parsing
now receives the selected dictionary variant for both Exhaustive and
HashChain routes; field modeling and Dynamic Range planning/encoding receive
the selected context variant. The bounded model bank has maximum storage for
4,550 entries but addresses only the selected layout. Complete-frame encoding
produces and round-trips a real distance-65,537 Match, and the streaming
encoder preserves the exact new header under one-byte output chunking. The
public profile, workspace query, CLI, benchmark, fuzzing, and interoperability
admission remain intentionally absent.

The internal profile/workspace stage is implemented independently of the C
ABI. A profile enum selects the 64 KiB or 1 MiB layout; the selected layout
sets stream dictionary/context IDs and typed-parameter validation. Encoder
requirements derive raw, serialized-frame, token, operation, aligned
HashChain, and aggregate extents with checked arithmetic. Decoder requirements
select the corresponding frequency-table limit. At `F = 1,048,576`, the
conservative serialized-frame ceiling is 12,582,997 bytes and the complete
encoder aggregate remains below the default 128 MiB limit. Existing C queries
selected the old profile until the following explicit ABI field was
introduced.

The Contextual Dynamic Range C lifecycle now allocates the former 64-bit
reserved tail as a 32-bit `window_profile` followed by a 32-bit reserved word,
retaining the ABI-1 structure extent and the prior all-zero default. Value 0
selects only the frozen `2/2 + 1/1` identity; value 1 selects only the extended
`2/3 + 1/2` identity. Workspace queries derive the selected 4,518- or
4,550-entry table requirement, encoders serialize that exact pair, and C
decoders reject the other pair before frame allocation. `window_size` does
not select or override the profile. CLI, benchmark, fuzz, and interoperability
admission remain separate stages.

## Required validation

In addition to ordinary Format 2 coverage, require:

- distances 65,535, 65,536, 65,537, 131,071, 131,072, 1,048,575,
  and 1,048,576;
- the class-16-to-17 transition at distances 131,071 and 131,072 and the
  class-19-to-20 transition at 1,048,575 and 1,048,576;
- rejection of distance 1,048,577 and every reference beyond frame history;
- exact rejection by variant 1 of every distance above 65,536, including a
  class-16 value whose reconstructed distance is 65,537, and of classes 17
  through 20;
- rejection of crossed dictionary/context variant pairs;
- deterministic identity between Exhaustive and HashChain Exact on inputs with
  useful matches beyond 64 KiB;
- frame sizes immediately below, equal to, and above 1 MiB policy limits;
- one-byte input/output chunking, final short frames, sticky terminal states,
  and frame-atomic malformed final-frame rejection;
- one-byte-short workspace and every relevant alias pair;
- descriptor-frequency-count, alphabet, bypass-width, padding, terminal-state,
  truncation, overflow, and trailing-data failures for every backend;
- bounded sanitizer fuzzing of complete-frame and public streaming decoders;
- same-input measurements against the 64 KiB variant for ratio, encode and
  decode throughput, and peak caller-owned workspace;
- cross-platform decoding and byte-identical re-encoding before release.

The primary hand vector is a frame containing a Match at distance 65,537 so
use of the extended window is unavoidable even though that distance remains
in class 16. A separate distance-131,072 vector must exercise the first new
class, 17, and a maximum-distance vector must exercise class 20. Small vectors
remain necessary for exact header and model serialization, but they cannot
establish that the extended window is used.
