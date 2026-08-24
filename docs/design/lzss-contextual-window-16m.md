# LZSS contextual 16 MiB window

Status: Dynamic Range and canonical contextual rANS lifecycles, including
schema-48 and schema-49 interoperability admission, implemented after project
version 0.4.0. Contextual tANS has private complete-frame encoder and decoder
admission; its profile, streaming, and later boundaries remain closed.

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

Dynamic Range header validation and private complete-frame decoding now admit
the exact `2/5 + 1/4 + 3/2` triple. Preflight selects 4,582 model entries,
`7F`, and `34T`; complete decoding validates the entropy payload, reconstructs
typed tokens, and then reconstructs raw bytes into caller-owned disjoint
workspaces. The first backend-specific vector decodes distance 4,194,305, the
first distance outside variant 4, from overlap-built history.

The private Dynamic Range stream parser and serializer now admit only the
exact dictionary-variant-5/context-variant-4 pair. Complete-frame encoding,
HashChain encoding, and one-byte input/output streaming round trips use the
same existing Dynamic Range frame representation. Explicit stream admission
can select the 16-MiB identity, while every older explicit admission rejects
it after collecting the complete stream header.

The authoritative encoder workspace query uses the selected layout's `7F`
decision multiplier and returns the exact 1,057,488,981-byte full-profile
aggregate on the supported 64-bit object layout. The decoder query returns
452,984,917 bytes. Equality succeeds and one byte short fails before workspace
publication. No full-size workspace is allocated by boundary tests. Canonical
contextual rANS and tANS descriptor/model paths recognize context variant 4.
The rANS lifecycle is complete; the tANS private complete-frame encoder and
decoder are admitted while later boundaries remain independently staged.

The public C selector `MARC_LZSS_CONTEXTUAL_PROFILE_16M` has value 3 and is
admitted only by the Dynamic Range configuration loader and profile helper.
The helper applies the 16-MiB frame/window/distance limits, 234,881,029-byte
payload ceiling, 4,582 model entries, and one-GiB aggregate policy while
preserving direction, original size, and the caller's total-output limit.
The public encoder and decoder workspace queries return the same exact
1,057,488,981-byte and 452,984,917-byte aggregates proven privately; equality
succeeds and one byte short fails without allocation. Existing initializers
remain 64 KiB and the ABI-1 configuration structure extent is unchanged. The
other contextual codec factories reject this known selector until their own
backend admission is complete.

The explicit CLI name `lzss-contextual-dynamic-range-16m` selects only this
Dynamic Range identity. It uses the public helper and authoritative workspace
query, retains transactional output on malformed and profile-mismatched input,
and does not reproduce backend layout arithmetic. The CLI name and one-GiB
application policy are not serialized.

The matching dependency-free benchmark name measures the same public profile.
It uses checked `112 + 14N + 85K` complete-stream capacity, performs an exact
pre-timing round trip, and reports all query-owned workspace regions. Benchmark
availability changes neither match-finder selection nor stream identity.

The existing fixed-array Dynamic Range fuzz harness also admits the 16-MiB
identity. It retains one-KiB frame/token storage and limits the larger profile
to its scalar distance and 4,582-entry model bounds; it neither allocates a
16-MiB history buffer nor exercises the large encoder workspace.

Interoperability schema 48 appends only the matching Dynamic Range CLI archive
as entry 58 after the frozen schema-47 inventory. Generation validates exact
identity `2/5 + 1/4 + 3/2`; compatibility removes only entry 58 before
traversing the unchanged schema-47-through-1 chain.

## Staged implementation order

1. shared dictionary/context constants, layouts, validators, and hand vectors
   (complete);
2. Dynamic Range decoder preflight and complete-frame decode (complete);
3. Dynamic Range encoder, exact workspace query, and streaming lifecycle
   (complete);
4. Dynamic Range C helper, CLI, benchmark, bounded fuzzing, and schema entry
   (complete);
5. canonical contextual rANS (complete);
6. contextual tANS (operation and direct typed-token coding complete; outer
   frame closed);
7. Contextual Blocked Huffman;
8. Contextual Adaptive Huffman;
9. only then evaluate whether 16 MiB evidence justifies a later 64-MiB design.

Every stage preserves existing identities, bytes, defaults, and deterministic
selection. No stage is complete from a one-shot round trip alone.
