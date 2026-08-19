# LZSS contextual 1 MiB window

Status: accepted design after project version 0.3.0. Dynamic Range, canonical
contextual rANS, and contextual tANS are complete through external
interoperability evidence. Contextual Blocked Huffman and Contextual Adaptive
Huffman remain staged.

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
| Contextual Blocked Huffman | `2/2` |
| Contextual Adaptive Huffman | `1/2` |

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
not select or override the profile.

The Dynamic Range CLI stage is now admitted under the explicit selector
`lzss-contextual-dynamic-range-1m`. It fixes both frame and window at
1,048,576 bytes and changes only the public C configuration before using the
ordinary requirements/factory/process lifecycle. The old unqualified selector
remains the 64 KiB profile. Each decoder name rejects the other's stream and
retains transactional output cleanup. Benchmark, fuzz, and interoperability
admission proceed as separate stages.

The Dynamic Range benchmark stage is now admitted under the same `-1m` name.
It changes only the public C profile/frame/window policy, derives all three
directional workspace extents from the requirements query, checks the common
Format 2 complete-stream capacity formula, and requires an exact untimed round
trip before measuring. The report makes ratio, both throughputs, every
workspace extent, and peak caller reservation available for same-input 64 KiB
versus 1 MiB comparison. Fuzz and interoperability admission remain separate.

The Dynamic Range fuzz stage reuses one fixed-memory dual-boundary target for
both profiles. It invokes the private complete-frame decoder after generic
header validation and separately creates the two strict public C decoders.
Supplied input remains capped at 8 KiB, published output at 4 KiB, and one raw
frame at 1 KiB; the extended identity, table ceiling, and distance limit do not
increase those allocations. Canonical extended truncations, cross-profile
admission, and descriptor corruption are permanent atomic regressions.
Interoperability admission remains separate.

The Dynamic Range interoperability stage appends the explicit `-1m` CLI
archive as schema-38 entry 48 after the exact schema-37 inventory. Generation
requires a local round trip, while verification enforces exact order, foreign
decode equality, and byte-identical local re-encoding. The 8,193-byte shared
fixture proves the new identity across producers but cannot force a distance
above 64 KiB; the dedicated distance-65,537 and larger vectors remain the
evidence for extended-window semantics. External cross-platform schema-38
exchange is complete at revision
`363a385168fcfab27adfc8eea3e302129cf01b15`: both CI bundles verified on
Ubuntu 26.04/Clang, and the Ubuntu 26.04 bundle verified there and on
Windows/MSVC, with all 48 archives reproduced byte-identically.

The canonical rANS path starts with a selected-layout descriptor stage. Its
entropy identity remains `4/3`, context count remains 31, and decode tables
remain 126,976 entries. Descriptor APIs receive the field-context variant
already selected from the stream header. Variant 1 retains 4,518 meaningful
frequencies and its exact 9,025-byte ceiling; variant 2 selects 4,550 and the
reserved 9,089-byte ceiling. One maximum-size backing array may serve both,
but variant 1 requires its unused 32-entry tail to be zero. Descriptor,
coding-core, and direct token-composition stages retain variant 1 as their
default while accepting an explicit selected layout.

The canonical rANS complete-frame and streaming-lifecycle stages are now
admitted internally. The stream header serializes and parses the reserved
`2/3 + 1/2 + 4/3` identity,
then selects one immutable layout for frame-count bounds, descriptor parsing,
exact HashChain tokenization, direct token coding, decode-table construction,
and typed-token reconstruction. The internal profile explicitly selects the
64 KiB or 1 MiB layout and derives caller-owned frame, token, HashChain,
descriptor, decode-table, and raw workspace bounds. A deterministic one-byte-
buffer lifecycle proves a generated distance above 65,536 and exact raw round
trip, while the frozen 64 KiB requirements and bytes remain unchanged. The
private decoder auto-selects a valid serialized identity; explicit profile
policy belongs to the public selector. The Contextual rANS C configuration now
splits its former 64-bit reserved tail into the shared 32-bit `window_profile`
and a 32-bit reserved word without changing ABI-1 extent. Public workspace
queries and encoders select the exact profile, while public decoders reject
the other identity before frame allocation. The CLI stage uses the explicit
`lzss-contextual-rans-1m` name for the selected 1 MiB public lifecycle and
retains the unqualified name as the 64 KiB profile. Both directions require
the same name, queried workspaces remain authoritative, and the two names
reject one another. The benchmark stage uses the same explicit 1 MiB name and
public configuration, with checked complete-stream capacity
`112 + 12N + 9,161K`. It performs an exact untimed round trip before reporting
descriptive speed, ratio, and caller-owned workspace values. Fuzz and
interoperability stages remain separate. The fuzz stage reuses the one
Contextual rANS target for private auto-selected frame parsing and both strict
public profiles. The 1 MiB identity and distance ceiling are admitted, but
input remains 32 KiB, output 4 KiB, and frame/token storage 1 KiB; only the
descriptor backing grows to the selected 9,089-byte maximum. Deterministic
malformed regressions cover both profiles and their reciprocal rejection.
The interoperability stage freezes schema 38 and appends the explicit
Contextual rANS 1 MiB CLI archive once as schema-39 entry 49. Generation checks
the 3/2 identity and local round trip; verification checks exact order, foreign
decode, and byte-identical re-encoding. Removing only that final entry recovers
schema 38 before the existing downgrade chain. Revision
`be940789f90b084bdf87ddd315b50da3e32fda55` completed the four-direction
exchange across the Windows/MSVC, Ubuntu 24.04 CI, and Ubuntu 26.04/Clang
producers with all 49 archives verified in every pass.

The next tANS vertical path begins at its descriptor boundary. The selected
field-context layout is supplied explicitly to descriptor analysis, parsing,
validation, and serialization; descriptor size or frequency-entry count never
selects it. Variant 1 retains 4,518 entries and 27 through 9,029 bytes, while
variant 2 uses 4,550 entries and 27 through 9,093 bytes. One fixed 4,550-entry
in-memory bank is permitted, but the unused variant-1 tail must remain zero.
Both layouts retain the same 131,072 transition-entry requirement because the
31 context IDs and one bypass table do not change. Coding tables, state
transitions, typed-token composition, frames, and public admission remain
separate later stages.

That descriptor stage is implemented. The default internal calls retain the
frozen 64 KiB layout and bytes; explicit variant-2 calls serialize and parse
the 4,550-entry compact model, reach the exact 9,093-byte dense ceiling, and
reject crossed counts, unsupported variants, nonzero frozen tails, malformed
records, truncation, trailing data, and short output atomically. No outer tANS
consumer selects variant 2 yet.

The following tANS coding-core stage passes that same immutable layout through
model counting and normalization, encode/decode table construction, reverse
state writing, forward state decoding, and operation-level entry points.
Counts reserve 4,550 entries but address only the selected layout; bypass width
is 16 or 20 bits. Both layouts retain 131,072 transitions, divided into 31
Symbol regions and one bypass region of 4,096 entries each. A class-20 Symbol
plus 20-bit bypass vector proves the new route before typed-token or frame
integration.

That coding-core stage is implemented. Variant 1 remains the default and
retains its frozen operation bytes. An explicit variant-2 operation sequence
normalizes distance context 23 over alphabet 21, writes and decodes class 20,
then writes and decodes the LSB-first value `0xabcde` using 20 bypass
decisions. Crossed alphabets and unsupported variants fail before publishing a
descriptor. Typed-token composition and every outer tANS boundary remain a
later stage.

The next tANS stage is the direct typed-token boundary. It selects one layout
before validating tokens, uses that layout's dictionary variant and distance
alphabet in both forward modeling and reverse state writing, and passes the
same selection into the completed coding core. Decode performs a validate-only
pass before publishing typed tokens, using the selected 26- or 30-decision
bound and dictionary reconstruction limit. Its maximum-distance vector grows a
1 MiB prefix with bounded distance-1 Matches before emitting distance
1,048,576, avoiding a million-Literal test while still forcing class 20 and 20
bypass bits. Frame and lifecycle integration remain separate.

That typed-token stage is implemented. The direct encoder is byte-identical to
the independently materialized operation route for the selected variant, and
the decoder validates the complete selected token sequence before publishing
it. The maximum-distance vector reaches a 1,048,576-byte prefix with one
Literal, 4,064 distance-1 Matches of length 258, and one of length 63; its
final length-5 Match at distance 1,048,576 round trips through class 20 and 20
bypass bits. Default calls retain the frozen 64 KiB route. No complete tANS
frame selects variant 2 yet.

The next tANS stage is complete-frame admission. The existing 112-byte stream
header exposes the reserved `2/3 + 1/2 + 5/2` identity as the sole layout
selector. The selected layout controls dictionary parsing, the 26/30 decision
bound, the 9,029/9,093-byte descriptor ceiling, direct token coding, and raw
reconstruction; the transition workspace remains 131,072 entries. With the
unchanged tANS payload ceiling, conservative complete-frame storage becomes
`9F + 9,095` or `9F + 9,159` bytes. The proving HashChain frame must contain a
distance above 65,536 and round trip atomically before lifecycle promotion.

The complete-frame stage is now admitted. Format validation selects only the
reserved paired identity; the encoder and decoder propagate that selection
through typed-token production, descriptor processing, the fixed transition
workspace, payload coding, and reconstruction. The proving frame contains a
distance-65,542 Match and rejects the crossed 64 KiB stream identity without
publishing raw output. Streaming construction and parsing still reject the
selected identity explicitly. This preserves a narrow review boundary: the
next stage must derive and validate its larger caller-owned workspaces before
promoting profile factories or partial-buffer lifecycle behavior.

That lifecycle stage uses the same explicit two-value profile selection as
Contextual rANS. Encoder and decoder calculators derive the selected
descriptor ceiling while retaining the fixed tANS transition extent. The
streaming encoder accepts the identity already validated by its profile; the
streaming decoder applies `any`, 64 KiB-only, or 1 MiB-only admission after
transactional header parsing and before frame collection. The stage must prove
one-byte chunking and a distance above 64 KiB, but it does not yet expose the
selection through the public C ABI, CLI, benchmark, fuzz, or schema.

Profile selection and workspace calculation are now implemented. The 1 MiB
profile publishes `2/3 + 1/2 + 5/2` with 4,550 frequency entries and increases
the conservative serialized-frame requirement by exactly 64 bytes over the
same 64 KiB raw extent. Unknown profile values and crossed dictionary limits
leave requirements empty.

The streaming half is now implemented as well. The selected encoder accepts
the profile identity, and the decoder's `any/64k/1m` policy rejects mismatches
before frame collection. A 65,546-byte selected frame is supplied and drained
one byte at a time, contains a HashChain Match beyond 65,536, and round trips
exactly. The private lifecycle is complete; public C selection remains next.

The public C stage preserves the existing tANS three-function family and
112-byte ABI-1 configuration. Its final zero-reserved eight bytes become the
shared 32-bit window selector and a 32-bit reserved word, matching Contextual
rANS without moving earlier fields. Workspace calculation and creation use the
selector explicitly, and public decoders require the same stream identity.
This stage is implemented: C11 tests retain the exact 64 KiB route, admit the
1 MiB route, and reject both crossed identities before raw publication.

The CLI stage adds `lzss-contextual-tans-1m` by the same explicit-name
rule as Contextual Dynamic Range and Contextual rANS. It uses a 1,048,576-byte
frame/window, `6F = 6,291,456` decision ceiling, `9F + 2 = 9,437,186` payload
ceiling, and 128 MiB internal-buffer policy. The established name remains
64 KiB and 8 MiB; decoding with either crossed name fails atomically. Runtime
inventory tests also keep both names ordered and reject near-miss spelling.

The benchmark stage mirrors `lzss-contextual-tans` and
`lzss-contextual-tans-1m`. Selected complete-stream capacity is
`112 + 9N + 9,159K`, where each frame reserves 64 header, 9,093 descriptor,
and two final-state bytes. The benchmark reports public queried workspace and
uses the existing pre-timing exact round trip; timings remain descriptive.

The fuzz stage retains the single Contextual tANS target and invokes both
strict public profile admissions beside the private profile-aware decoder.
Its 1 KiB frame/token and 131,072-entry transition ceilings remain unchanged;
only fixed descriptor backing grows by 64 bytes to 9,093. Both canonical
profiles, malformed variants, and reciprocal mismatches must preserve atomic
failure and sticky errors before a bounded sanitizer campaign is recorded.

That stage is now implemented. Seven permanent regressions cover both
canonical profiles, every truncation, saturated frame extents, descriptor
padding, reciprocal strict-policy rejection, sentinel preservation, and
sticky errors. The fixed-memory target also completed its bounded sanitizer
campaign; interoperability publication remains the next independent boundary.

That publication boundary freezes all 49 schema-39 archives and appends only
`lzss-contextual-tans-1m` as schema-40 entry 50. Its generator must verify
selected identity `2/3 + 1/2 + 5/2` and an immediate exact round trip. The
compatibility suite removes only entry 50 to reconstruct schema 39 before its
existing complete downgrade chain; no earlier archive or representation may
change.

Local schema-40 admission is now complete. Both MSVC and ClangCL generate and
verify all 50 archives, reject a reordered current manifest, remove only the
new final archive to recover schema 39, and complete every downgrade through
schema 1. Revision `e74473d1511990ed06ea43c739783d1c58daf065`
completed the four-direction external exchange across the Windows/MSVC,
Ubuntu 24.04 CI, and Ubuntu 26.04/Clang producers with all 50 archives
reproduced byte-identically in every pass.

The Contextual Blocked Huffman descriptor stage is implemented. Descriptor
APIs receive the selected field-context variant explicitly while retaining
64 KiB as the source-level default. Variant 1 preserves its exact 24-through-
2,561-byte grammar; variant 2 admits 21-symbol distance records and the exact
24-through-2,579-byte range. Both retain 35 tables and 17,885 decode nodes.
Crossed layouts, every selected-size violation, odd dense-record padding,
truncation, trailing bytes, and short output fail atomically. No entropy
coding, typed-token, frame, streaming, public, or schema boundary selects the
new descriptor yet.

The next Contextual Blocked Huffman stage carries that immutable selection
through the entropy coding core. Model builders, canonical payload writers,
operation planners/encoders, and decoder startup receive the explicit field-
context variant while retaining 64 KiB as the default. Variant 2 selects the
21-symbol pooled distance field and permits 20-bit bypass values; variant 1
retains 17 symbols and 16 bits. The exact extended hand payload is `de bc 0a`
for four required pooled Single fields, including distance class 20, followed
by bypass value `0xabcde`; its decision count is 24. Unknown and crossed
selections must fail before observable output. This stage does not yet admit
typed-token composition or any outer frame/profile surface.

That coding-core stage is implemented. The selected layout now remains
immutable through model collection, strict override selection, payload-size
planning, canonical code emission, table construction, symbol decoding, and
bypass decoding. The extended hand payload is exact, crossed requests and
unknown variants are atomic, and all frozen 64 KiB vectors remain unchanged.
The next independent boundary is direct typed-token composition.

That direct boundary will validate the selected typed-token dictionary
variant, derive distance alphabets and bypass widths from the same immutable
layout, and invoke the completed Contextual Blocked Huffman core without an
intermediate operation buffer. Decoding remains two-pass: validate entropy,
token grammar, distances, counts, and raw extent before publishing tokens.
The selected proving vector uses 131,072 literal bytes followed by a distance-
131,072 Match, forcing class 17 and a 17-bit bypass while keeping the permanent
test materially smaller than a maximum-distance token prefix.

That direct typed-token boundary is implemented. Its selected descriptor and
payload are byte-identical to the ModeledOperation reference path for the
distance-131,072 vector, and the two-pass decoder reconstructs every token
without partial publication. Variant 1, unknown variants, and crossed
descriptors fail atomically. The next independent boundary is complete-frame
admission for the exact `2/3 + 1/2 + 2/2` identity; streaming, public C, CLI,
benchmark, fuzz, and interoperability admission remain later stages.

That complete-frame boundary will serialize and validate the exact selected
identity, use the selected 30-decision token and 2,579-byte descriptor ceilings,
and pass the immutable layout through dictionary matching, direct entropy
coding, descriptor parsing, token decoding, and reconstruction. The permanent
frame vector repeats a five-byte marker after 65,536 filler bytes so the exact
HashChain encoder must exercise a distance above 64 KiB. Crossed stream
identities and descriptor layouts must fail before raw publication.

That complete-frame boundary is implemented. The selected stream header
round-trips exact `2/3 + 1/2 + 2/2`, frame validation applies the 30-decision
and 2,579-byte ceilings, and both match-finder and reconstruction paths use
the paired typed-token variant. The permanent marker-gap-marker vector emits
a Match beyond 64 KiB and round-trips through the selected complete decoder;
crossed decoding leaves raw output untouched. The next independent boundary
is selected profile workspace calculation and streaming lifecycle admission.

That lifecycle boundary now adds an explicit profile variant and exact decoder
admission policy. Selected encoder/decoder workspace differs from the frozen
layout only where required by the 2,579-byte descriptor maximum, selected
match-finder dictionary, and configured frame extent; table count and token
representation remain unchanged. The one-byte streaming proof reuses the
marker-gap-marker frame and requires reciprocal profile rejection before raw
publication. The public C lifecycle now carries the same exact selector in its
preserved ABI-1 extent and enforces reciprocal decoder admission. The next
independent boundary is the explicit CLI profile.

The explicit `lzss-contextual-blocked-huffman-1m` CLI profile now binds the
selected C lifecycle to a 1 MiB frame/window and bounded 128 MiB aggregate
policy. The legacy CLI name remains fixed to 64 KiB, and each name rejects the
reciprocal archive. The next independent boundary is benchmark admission.

The explicit 1 MiB Contextual Blocked Huffman benchmark profile now uses the
same selected C lifecycle and bounded policy. Its capacity accounts for the
2,579-byte selected descriptor, and its strict report smoke covers workspace
aggregation and exact naming without treating performance as a correctness
threshold. The next independent boundary is sanitizer fuzz admission.

The Contextual Blocked Huffman sanitizer target now exercises both exact
public profile admissions and private selected parsing within one fixed
workspace. Deterministic malformed regressions cover both layouts and crossed
public policies, and a bounded 1,000-input ASan/UBSan campaign completed
without a finding. The next independent boundary is interoperability
admission.

That publication boundary freezes all 50 schema-40 archives and appends only
`lzss-contextual-blocked-huffman-1m` as schema-41 entry 51. Its generator must
verify selected identity `2/3 + 1/2 + 2/2` and an immediate exact round trip.
The compatibility suite removes only entry 51 to reconstruct schema 40 before
its existing complete downgrade chain; no earlier archive or representation
may change.

Local schema-41 admission is now complete. Both MSVC and ClangCL generate and
verify all 51 archives, reject a reordered current manifest, remove only the
new final archive to recover schema 40, and complete every downgrade through
schema 1. Four-direction external exchange remains post-push evidence.

Revision `c3ea5f87784faaca8c93e98fe5e459df3290747c` completed that four-
direction exchange with all 51 archives reproduced byte-identically. This
closes the Contextual Blocked Huffman vertical path. The final backend begins
by selecting the Contextual Adaptive Huffman FGK model-bank layout explicitly:
variant 1 retains 4,518 symbols and 9,067 nodes, while variant 2 uses 4,550
symbols and 9,131 nodes across the same 31 reset-per-frame trees. No descriptor
or payload-ceiling change belongs to that foundation.

That model-bank foundation is implemented. Initialization now requires an
explicit canonical field-context variant or layout, derives and checks the
exact caller-owned node and symbol extents, and partitions all 31 FGK trees
from the selected alphabets and offsets. Existing entropy encoder and decoder
callers explicitly select variant 1, preserving every released stream byte;
the selected variant-2 entropy coding path remains the next independent
boundary.

The following operation boundary passes that selection explicitly through
planning, writing, and decoding. It uses the selected model-bank extents,
Symbol alphabets, and 16/20-bit bypass ceiling while leaving the descriptor
variant-neutral. The proving class-20 plus 20-bit bypass vector is
`D4 9B 57 01`; typed-token and frame composition remain separate later work.

That operation boundary is implemented. One-shot and forward two-pass coding
derive region extents and limits from the selected layout, and decoding retains
the same layout for every request. Existing composed callers name variant 1
explicitly; source-level lifecycle defaults preserve the frozen 64 KiB route.
The hand vector round trips identically through both encoding paths, while
crossed alphabets, unsupported selections, and short selected workspaces fail
without publishing descriptor, payload, or decoded values.

The direct Contextual Adaptive Huffman typed-token boundary next retains that
immutable selection for its full lifetime. It validates the matching
dictionary token variant, derives field alphabets and bypass widths from the
selected layout, uses the selected 9,067/4,518 or 9,131/4,550 model extents,
and applies the 26- or 30-decision-per-token bound during decode. Variant 1
remains the source-level default and byte-frozen.

Its maximum-distance proof builds exactly 1,048,576 history bytes from one
literal and bounded distance-1 Matches, then emits a length-5 Match at distance
1,048,576. The final distance field is class 20 in alphabet 21 followed by a
20-bit zero bypass value. Direct token coding must equal independent operation
modeling, and a validate-only pass must complete before decoded tokens become
caller-visible. Complete-frame and public selection remain later stages.

That direct typed-token boundary is implemented. Both planning and writing use
the selected dictionary variant and exact model slices; decoding resolves the
same layout before any caller-visible mutation and retains its two-pass
publication rule. The maximum-distance token sequence produces the same
descriptor and payload as its independently modeled operation sequence.
Legacy, unsupported, crossed, and one-short selected paths leave their
descriptor, payload, and token destinations unchanged. The next independent
boundary is complete-frame selection.

The selected Contextual Adaptive Huffman complete-frame boundary next exposes
the existing dictionary/context identity fields and accepts only paired
`2/2 + 1/1` or `2/3 + 1/2` layouts with entropy `1/2`. It carries that layout
through typed matching, token entropy coding, frame bounds, model workspace,
and reconstruction while retaining the fixed 16-byte descriptor and 64-byte
frame header. The permanent marker-gap-marker proof must require a distance
beyond 64 KiB and crossed decode must publish no raw byte. Profile, streaming,
and public admission remain later stages.

That complete-frame boundary is implemented. Stream parsing and serialization
now expose the existing identity fields at offsets 14, 96, and 98, and frame
validation resolves one canonical layout before capacity, workspace, token, or
raw-output work. Exact selected model extents and decision bounds flow through
Exhaustive and HashChain Exact encoding, Adaptive Huffman token coding, and
reconstruction. A marker, 65,536-byte gap, and repeated marker force an
extended-distance Match and round trip deterministically; crossed identity and
every independently one-short selected region fail without publishing output.
The next independent boundary is profile and streaming-lifecycle admission.

That next boundary uses one private profile selection for both sizing and
stream construction. Variant 1 remains the default with 9,067 nodes and 4,518
symbols; variant 2 uses 9,131 nodes and 4,550 symbols while sharing the fixed
descriptor and payload ceiling. Workspace partitioning accepts only those two
canonical pairs. Streaming encoders inherit the selected stream identity, and
decoders may accept either supported identity or require the exact 64 KiB or
1 MiB pair before frame collection. A one-byte-chunk marker-gap-marker round
trip proves the selected state survives the complete lifecycle. Public, CLI,
benchmark, fuzz, and interoperability admission remain later stages.

That private lifecycle boundary is implemented. Profile construction resolves
the selected layout before dictionary and workspace calculation, and its
partitioners admit only the canonical 9,067/4,518 or 9,131/4,550 model pairs.
The selected streaming encoder retains the exact identity through every frame;
decoder construction fixes `any`, exact 64 KiB, or exact 1 MiB admission and
rejects reciprocal headers before frame collection. The extended marker-gap-
marker vector produces a Match beyond 64 KiB and round trips with one-byte
buffers through both `any` and exact-1m modes. Public C admission is the next
independent boundary.

That next public boundary preserves the ABI-1 Contextual Adaptive Huffman
configuration extent by splitting its trailing reserved 64 bits into the
common window-profile selector and a 32-bit reserved word. Zero continues to
mean the exact 64 KiB profile; one selects exact `2/3 + 1/2 + 1/2`. Workspace
query, encoder construction, decoder sizing, and exact decoder admission all
derive from that explicit selector. CLI, benchmark, fuzz, and interoperability
admission remain later boundaries.

That public boundary is now implemented. The 112-byte configuration extent and
all-zero 64 KiB default remain fixed; selected workspace query, construction,
and reciprocal decoder admission share one validated selector. The 1 MiB
public lifecycle emits its exact identity, uses a Match beyond 64 KiB, and
round trips while both reciprocal decoder routes fail before raw publication.
A 1 MiB identity may use a smaller configured search window, whereas the
64 KiB identity cannot admit a larger one. CLI admission is the next boundary.

That next CLI boundary uses the explicit name
`lzss-contextual-adaptive-huffman-1m` for 1 MiB frames, windows, and distance
policy, 13,681 model entries, the fixed Adaptive Huffman payload formula, and
a 128 MiB aggregate limit. The existing name remains exactly 64 KiB. Both
names use the public C lifecycle and reject reciprocal archives before raw
publication. Benchmark, fuzz, and interoperability admission remain later
boundaries.

That CLI boundary is now implemented. The existing and selected names appear
adjacent exactly once, configure their exact public selectors and bounded
policies, emit their expected identities, round trip deterministically, reject
reciprocal archives without output, reject trailing data, and retain case-
sensitive parsing. Benchmark admission is the next boundary.

That next benchmark boundary adds the same explicit `-1m` name and policy,
uses checked `112 + ceil(267N/8) + 80K` complete-stream capacity, obtains both
directional workspace layouts through the public C lifecycle, and requires an
untimed round trip before measurement. Ratio, throughput, and peak workspace
remain descriptive observations. Fuzz and interoperability admission remain
later boundaries.

That benchmark boundary is now implemented. Both exact names query and create
through the common public C lifecycle, and the selected name fixes the 1 MiB
selector, 13,681-entry model ceiling, payload and aggregate bounds. Its strict
smoke verifies the untimed round trip, finite report, full directional
workspace aggregation, adjacent usage, and case-near-miss rejection. Fuzz
admission is the next boundary.

That next sanitizer boundary keeps one Contextual Adaptive Huffman target and
exercises private selected parsing plus both strict public profiles. It grows
only fixed model backing to 9,131 nodes and 4,550 symbols, retains a 1 KiB raw
buffer and 1,024 typed-token entries, and parameterizes deterministic
malformed and reciprocal-admission regressions. The bounded campaign remains
1,000 inputs with no persistent corpus; interoperability admission stays
separate.

That sanitizer boundary is now implemented. Every bounded input reaches the
private complete-frame decoder and both exact public admissions using the
selected maximum model bank without allocating a 1 MiB frame. Eleven focused
regressions pass under both normal compilers, and the fixed 1,000-input Clang
22 sanitizer campaign completed without a crash, hang, or sanitizer finding.
Interoperability inventory admission remains the next independent boundary.

That next interoperability boundary freezes schema 41 and appends
`lzss-contextual-adaptive-huffman-1m` once as schema-42 entry 52. Its generator
must prove a local round trip and inspect exact `2/3 + 1/2 + 1/2` identity.
The verifier enforces exact order, foreign decode equality, and byte-identical
re-encoding; compatibility removes only the new leaf to reconstruct schema 41.
The common fixture remains identity/determinism evidence rather than a distant-
match vector. External exchange follows local dual-compiler admission.

Local schema-42 admission is now complete. Both MSVC and ClangCL generate and
verify all 52 archives, reject a reordered current manifest, remove only the
new final archive to recover schema 41, and complete every downgrade through
schema 1. Four-direction external exchange remains post-push evidence.

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

## Future windows above 1 MiB

Dictionary variant 3 and context variant 2 are permanently bounded to the
documented 1,048,576-byte window. A faster match finder, a larger configured
limit, or a successful private benchmark must not widen either identity.
The next candidate is a private four-MiB match-finder experiment described in
`lzss-hash-tree-match-finder.md`; it has no decoder-visible identity and does
not reserve a public algorithm or variant number.

If that experiment passes its Exact, CPU, compression-opportunity, and memory
gates, a public successor requires a new dictionary variant and a new context
variant. A four-MiB ceiling would require distance classes 0 through 22, a
23-symbol distance alphabet, and 4,566 flattened symbol slots:

```text
3*2 + 17*256 + 3*8 + 8*23 = 4,566
```

These values are design consequences, not current format reservations. Each
entropy backend must receive its own checked descriptor, model, workspace,
malformed-input, fuzz, benchmark, and interoperability admission. Crossed old
and new dictionary/context identities must be rejected. The frame must also
be large enough for a distance above one MiB to occur; widening only the
window while retaining one-MiB frame resets would provide no such benefit.
