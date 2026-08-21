# LZSS contextual 4 MiB window

Status: accepted design after project version 0.3.0. No public 4 MiB profile is
implemented by this document.

## Purpose

This document defines the next additive typed-token LZSS contextual family. It
raises the maximum match distance and reference frame/window size from
1,048,576 to 4,194,304 bytes while retaining the maximum match length of 258
bytes. The wider history targets useful repetitions separated by more than one
MiB.

The change is decoder-visible. It is not an encoder-only match-finder option
and MUST NOT reinterpret either released contextual family. Existing
dictionary/context pairs `2/2 + 1/1` and `2/3 + 1/2` remain frozen.

## Decoder-visible identity

Format 2.0 reserves this exact pair for the new family:

```text
dictionary algorithm ID 2, dictionary variant 4
context-model algorithm ID 1, context variant 3
```

Dictionary variant 4 and context variant 3 MUST occur together. Crossed old
and new pairs are contradictory parameters and must fail after the complete
stream header and before frame allocation. No match-finder strategy, public
window selector, or CLI spelling is serialized.

The candidate backend identities remain unchanged:

| Backend | Entropy algorithm/variant | Initial disposition |
|---|---:|---|
| Dynamic Range | `3/2` | first vertical path |
| canonical contextual rANS | `4/3` | staged |
| contextual tANS | `5/2` | staged |
| Contextual Blocked Huffman | `2/2` | staged |
| Contextual Adaptive Huffman | `1/2` | deferred pending a memory proof |

Reservation of the shared pair does not admit any backend by itself. Each
triple requires its own exact bounds, validator, encoder, decoder, public
lifecycle, benchmark, fuzzing, and interoperability evidence.

## Dictionary variant 4

Variant 4 retains the typed `Literal` and `Match` grammar, greedy
longest-match parse, nearest-distance tie break, beneficial-match rule,
overlap-copy semantics, empty history, and per-frame reset. Its parameter
region requires:

```text
minimum_match_length = 5
5 <= maximum_match_length <= 258
1 <= window_size <= 4,194,304 bytes
```

The reference profile uses a 4,194,304-byte frame and window. The final frame
may be shorter. Every distance is also bounded by the raw prefix already
reconstructed in the current frame; no Match crosses a frame boundary.

HashChain Exact is the initial production match finder and Exhaustive remains
the private oracle. Complete and sparse HashTree implementations remain
private experimental strategies. The earlier complete 4 MiB HashTree result
showed useful speed and match opportunities but exceeded the aggregate memory
policy, while sparse HashTree has not yet been measured at this window. A
later encoder-local strategy change requires fresh bounded evidence but no
format change when it retains Exact tokens.

## Context variant 3

Context state, context IDs 0 through 30, and selection rules remain unchanged.
Only the eight distance-class alphabets expand:

| Context IDs | Count | Alphabet | Meaning |
|---:|---:|---:|---|
| 0..2 | 3 | 2 | token kind |
| 3..19 | 17 | 256 | Literal value |
| 20..22 | 3 | 8 | match-length class |
| 23..30 | 8 | 23 | match-distance class |

The flattened Symbol-model layout contains exactly 4,566 entries:

```text
3*2 + 17*256 + 3*8 + 8*23 = 4,566
```

For distance `D`:

```text
distance_class = floor(log2(D))       # 0..22
distance_extra = D - 2^distance_class
```

A nonzero class is followed by exactly `distance_class` LSB-first bypass
bits. Distance 4,194,304 therefore uses class 22 and extra value zero. Older
context variants continue to reject values beyond their frozen limits.

A Match produces at most five modeled events and 32 entropy decisions: three
Symbol decisions, at most seven length-extra bits, and at most 22
distance-extra bits. For raw frame size `F` the common conservative bounds are:

```text
token_count <= F
event_count <= 2 * F
decision_count <= 7 * F
decision_count <= 32 * token_count
```

The former `6F` decision bound is not valid for this variant because one
minimum-length Match can require 32 decisions for five raw bytes. All count,
size, offset, and reconstruction arithmetic must remain checked.

## Backend and memory gates

Dynamic Range retains its descriptor and arithmetic. Its decision-derived
payload ceiling must be recalculated from `7F`; a provisional conservative
ceiling is `14F + 5` payload bytes and `14F + 85` complete-frame bytes. At a
4 MiB frame this remains below the existing 64 MiB compressed-payload limit,
but aggregate encoder and decoder workspace must still be calculated from the
actual implementation before admission.

Canonical contextual rANS grows its maximum descriptor from 9,089 to 9,121
bytes, and contextual tANS grows from 9,093 to 9,125 bytes. These values follow
from sixteen additional flattened frequencies at two serialized bytes each.
Their table counts do not change, but their payload, token staging, serialized
frame, and aggregate workspace ceilings must be rederived from the new
decision bound.

Contextual Blocked Huffman uses a 23-symbol distance alphabet. Its pooled and
override descriptor maxima, payload ceiling, decode-table allocation, and
aggregate workspace must be derived before its triple is admitted.

Contextual Adaptive Huffman would require 4,566 symbol slots and:

```text
2*4,566 + 31 = 9,163 nodes
```

The current conservative `ceil(267F/8)` payload rule exceeds 128 MiB for a
4 MiB frame even before other workspace is counted. This backend therefore
remains deferred. Admission requires a proven tighter safe bound or a bounded
streaming redesign; the default 128 MiB aggregate policy and 64 MiB compressed
payload limit are not raised merely to make it fit.

The 128 MiB aggregate limit is a current safety default, not a permanent
format constant. A later larger-window design may separate the profile's
checked minimum workspace, the caller-configured hard limit, and the library
default before proposing a higher value. Such a change requires measured peak
memory, overflow and one-short tests, denial-of-service analysis, and explicit
documentation; window growth alone does not silently increase the limit.

## Shared groundwork status

The shared dictionary and context stage is implemented internally. Typed-token
variant 4 enforces the 4,194,304-byte ceiling. Context variant 3 owns exact
23-symbol distance alphabets, 4,566 flattened entries, 22-bit bypass and
32-decision token ceilings, and the selected `7F` raw-frame bound. Pair
selection admits only `2/2 + 1/1`, `2/3 + 1/2`, and `2/4 + 1/3`.

The maximum-distance test constructs four MiB of history with bounded overlap
Matches, then models and reconstructs a length-258 Match at distance 4,194,304.
It therefore exercises class 22 and a 22-bit zero bypass without a
multi-million-Literal fixture.

The subsequent Dynamic Range decoder stage admits the exact private triple
`2/4 + 1/3 + 3/2` at stream-header preflight and complete-frame decode. Its
selected model storage has 4,566 entries, its count validator uses `7F`, and
its first complete-frame vector contains a real distance-1,048,577 Match.
Crossed identities and one-entry-short token/raw workspaces fail before any
raw publication. The operation-level Dynamic Range encoder is large enough to
serve as the deterministic test-vector oracle, but the complete-frame encoder,
profile/workspace lifecycle, public C selector, CLI profile, and
interoperability inventory remain closed.

## Planned public policy

After one backend completes its vertical path, the common C window-profile
selector may add value 2 for the exact 4 MiB identity. Value 0 remains 64 KiB
and value 1 remains 1 MiB. A completed CLI backend may add an explicit `-4m`
name. Neither source-level value nor CLI name is reserved as public API by this
design-only stage.

An exact public decoder admits only its selected dictionary/context pair; it
does not infer a profile from `window_size`, descriptor size, or distance seen
in a payload. A generic private decoder may auto-select any fully implemented
canonical pair from the validated stream header.

## Staged implementation order

1. shared dictionary/context enums, layouts, validators, and hand vectors;
2. Dynamic Range decoder-side preflight and complete-frame decode;
3. Dynamic Range encoder, profile/workspace, and streaming lifecycle;
4. Dynamic Range C API, CLI, benchmark, fuzzing, and interoperability;
5. canonical contextual rANS;
6. contextual tANS;
7. Contextual Blocked Huffman;
8. Contextual Adaptive Huffman only after its memory gate is solved.

Each stage must retain old stream bytes, source defaults, and exact admission
rules. Public names are added one backend at a time, not when shared groundwork
alone exists.

## Required validation

Require at least:

- distances 1,048,575, 1,048,576, 1,048,577, 2,097,151, 2,097,152,
  4,194,303, and 4,194,304;
- the class-20-to-21 and class-21-to-22 transitions;
- rejection of 4,194,305 and every reference beyond current frame history;
- rejection by each older variant of distances beyond its own frozen limit;
- rejection of every crossed dictionary/context pair;
- deterministic Exhaustive/HashChain Exact token equality on useful matches
  beyond one MiB;
- frame sizes immediately below, equal to, and above four MiB policy limits;
- one-byte chunking, final short frames, terminal-state stability, aliases,
  one-short workspaces, malformed descriptors, padding, truncation, overflow,
  and trailing data;
- bounded sanitizer fuzzing and four-direction interoperability before public
  release.

The primary format vector must contain a real Match at distance 1,048,577 so
the new identity is semantically necessary. Separate vectors exercise class
21 at distance 2,097,152 and class 22 at distance 4,194,304 without requiring
millions of Literal tokens.
