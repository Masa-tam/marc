# LZSS Contextual 64-KiB short-match candidate

Status: decoder-visible reservation with private frame/stream decoders and a
private candidate-selecting frame encoder (2026-09-24). No public encoder,
public selector, or interoperability archive admits this identity yet.

## Purpose and isolation

BM-0099 found 3- and 4-byte equal prefixes at 3,645,272 positions where the
existing 64-KiB greedy parser emits a Literal on Silesia `mozilla`. BM-0100
calibrated its complete Dynamic Range payload against the published profile.
Those counts do not prove that replacing any Literal with a Match saves bytes.
This candidate makes a complete-payload experiment possible without changing
the frozen 0.7.0 identity `dictionary 2/2 + context 1/1 + entropy 3/2`.

The new Format 2.0 identity is exactly:

```text
dictionary algorithm/variant 2/7
context-model algorithm/variant 1/6
entropy algorithm/variant 3/2
```

Dictionary variant 7 and context variant 6 MUST occur together. They are
reserved only with Dynamic Range `3/2`; every crossed dictionary/context
pair and every other entropy backend MUST be rejected before frame or model
allocation. Adding a backend later requires its own exact admission and tests.
No strategy, minimum-benefit policy, or resource profile is inferred from a
stream. Existing variant pairs and archive bytes remain unchanged.

## Dictionary variant 7

The 16-byte LZSS parameter region retains its field offsets, byte order, and
zero-flags rule. For this variant only:

```text
1 <= window_size <= 65,536 bytes
minimum_match_length = 3
3 <= maximum_match_length <= 258
1 <= frame_size <= 65,536 raw bytes
```

The reference resource profile uses 65,536-byte frames and a 65,536-byte
window. History starts empty and resets at every frame; no token or match
crosses a frame. A Match of length `L` and distance `D` requires
`3 <= L <= configured maximum`, `1 <= D <= configured window`, and
`D <= already reconstructed bytes in this frame`. The entire Match must fit
the declared raw frame. Overlap copies are bytewise and permitted. Literal
and Match unused fields remain zero. Token count and reconstructed raw size
must match frame declarations exactly.

This is a typed-token rule, not a relaxation of the canonical byte-serialized
LZSS variant-1 minimum of five. The existing generic parameter validator
currently enforces that older minimum; implementation MUST use variant-aware
validation without weakening any older stream. The encoder's beneficial-match
decision is deliberately not assigned by this decoder reservation: an
encoder-local policy must be specified and measured before any public encoder
is exposed. In particular, `length >= 3` is not by itself a profitability
claim.

## Context variant 6

Reset state, token-kind and Literal contexts, and update order match context
variant 1. The new length value is `V = L - 2`, so `1 <= V <= 256`. Encode

```text
length_class = floor(log2(V))       # 0..8
length_extra = V - 2^length_class
distance_class = floor(log2(D))     # 0..16
distance_extra = D - 2^distance_class
```

as Match kind, length class, optional `length_class` LSB-first bypass bits,
distance class in context `23 + length_class`, and optional
`distance_class` LSB-first bypass bits, in that order. Zero-width bypass
operations are absent. Checked inverse arithmetic reconstructs `L = V + 2`
and `D = 2^distance_class + distance_extra` before validating the token.

| Length | Value | Class | Extra bits/value | Distance context |
| ---: | ---: | ---: | --- | ---: |
| 3 | 1 | 0 | none | 23 |
| 4 | 2 | 1 | 1 / 0 | 24 |
| 5 | 3 | 1 | 1 / 1 | 24 |
| 258 | 256 | 8 | 8 / 0 | 31 |

The selected Symbol model has 32 contexts:

| Context IDs | Count | Alphabet | Meaning |
| ---: | ---: | ---: | --- |
| 0..2 | 3 | 2 | token kind: Start, after Literal, after Match |
| 3..19 | 17 | 256 | Literal value |
| 20..22 | 3 | 9 | length class |
| 23..31 | 9 | 17 | distance class, selected by length class 0..8 |

The flattened frequency bank has exactly
`3*2 + 17*256 + 3*9 + 9*17 = 4,538` entries and 32 totals. The distance
class alphabet retains 16 as its largest value even though distance 65,536
cannot be reached by a Match inside the reference 65,536-byte frame; the
raw-prefix validation still rejects such an impossible reference. Earlier
31-context layouts and their frequencies MUST NOT change.

## Dynamic Range representation and bounds

Entropy variant `3/2` retains its byte-oriented integer arithmetic, model
initialization at frequency one, total limit 32,768, deterministic rescaling,
LSB-first bypass decisions, leading zero, and five-shift termination. The
existing 16-byte descriptor changes no field position: it declares the
frame's exact decision count and payload size, context count **32**, and zero
flags and reserved bytes. Models reset independently for each frame.

For raw frame size `F` and token count `T`, checked preflight requires:

```text
1 <= F <= 65,536
1 <= T <= F
2T <= event_count <= min(2F, 5T)
event_count <= decision_count <= min(9F, 27T)
5 <= payload_size <= min(18F + 5, 2*decision_count + 5)
```

A Literal has two events and two decisions. A Match has at most five events
and `3 + 8 + 16 = 27` decisions while producing at least three raw bytes;
therefore the `9F` decision bound is conservative. The existing Range writer
emits at most two bytes per decision plus five terminal shifts, yielding the
conservative payload ceiling. The complete frame is at most `18F + 85` bytes:
64-byte frame header, 16-byte descriptor, and payload. At `F = 65,536`, these
ceilings are 1,179,653 payload bytes and 1,179,733 complete-frame bytes. All
are limits, not unconditional allocations. Caller-supplied hard limits and
checked aggregate workspace still take precedence.

The stream header is the existing 112-byte Format 2.0 layout: 64-byte prefix,
16-byte dictionary parameters, 16-byte Dynamic Range parameters, and 16-byte
context extension. TypedContext feature bit 0 is set; other feature bits,
flags, hashes, entropy block size, and reserved fields remain zero. Original
size is known. Frames use the unchanged 64-byte `MRF2` header, contiguous
sequence, deterministic raw partition, zero context side data and trailer,
and one 16-byte descriptor followed by exact payload bytes. Little-endian
integer and strict payload/trailing-data rules remain unchanged.

## Hand-checkable decoder vector

For raw bytes `61 61 61 61` (`aaaa`), use stream frame size 4, window 65,536,
minimum length 3, maximum length 258, and original size 4. A legal token
frame is `Literal(0x61), Match(distance=1, length=3)`; it is a decoder vector,
not a commitment that the future encoder must choose this parse. It yields:

```text
Symbol(context 0, alphabet 2, value 0)
Symbol(context 3, alphabet 256, value 97)
Symbol(context 1, alphabet 2, value 1)
Symbol(context 21, alphabet 9, value 0)
Symbol(context 23, alphabet 17, value 0)
```

There are two tokens, five events, five decisions, and no bypass bits. The
Dynamic Range payload is `00 30 BF FF 9E 80 00` (seven bytes), obtained by
the specified integer interval and five-shift termination rules. The frame
header declares raw size 4, token count 2, event count 5, decision count 5,
payload size 7, descriptor size 16, and zero sequence/flags/side data/trailer.
The descriptor declares decisions 5, payload bytes 7, and context count 32.
The complete no-hash stream is 112 + 64 + 16 + 7 = **199 bytes**. Empty input
is the 112-byte stream header alone and has no frame.

## Validation and staged admission

Implement decoder-side variant-aware header and parameter checks first. Reject
unknown or crossed variants, a non-32 descriptor context count, mismatched
alphabets, class 9, missing or excess bypass bits, impossible distances,
lengths outside configured bounds, count overflow, noncanonical Range
termination, incomplete frames, and forbidden trailing bytes before raw-frame
publication. A failure may not publish part of its frame. Preserve all prior
variant vectors and the schema-57 inventory byte-for-byte.

Only after decoder and malformed-stream tests pass should a private encoder
evaluate explicit deterministic selection policies for 3- and 4-byte matches.
It must compare *complete* payload/archive sizes against BM-0100, including
changes to subsequent parse and adaptive model state. Full-corpus ratio,
encode/decode time, workspace, split-buffer identity, sanitizer coverage, and
cross-platform verification gate any public profile or interoperability
archive. No `gzip -9v` comparison is an assertion in codec tests.

## First implementation boundary

The private dictionary token validator now recognizes variant 7 with exactly
minimum match length 3, maximum 3..258, window at most 65,536, zero flags,
caller hard limits, and raw-frame size at most 65,536. It validates distance
against already reconstructed frame bytes and permits bytewise overlap.
Malformed token frames fail before the reconstruction output is written.
The generic serialized-byte LZSS parameter validator still requires minimum
length 5. The shared typed-context stream parser intentionally still rejects
the new `2/7 + 1/6` pair; byte-level parsing and 32-context entropy decoding
must be completed and tested before that gate opens.

## Second implementation boundary

An isolated compile-time layout now supplies 32 alphabets and 33 cumulative
offsets, ending at frequency entry 4,538. The old 31-context arrays and model
storage are not enlarged. A private semantic preflight accepts only the exact
reserved identity and validates a structured stream/frame/descriptor triple:
frame sequence and raw partition, token/event/decision counts, the tighter
`2*decision_count+5` payload bound, descriptor context count 32, configured
hard limits, and checked aggregate space for serialized frame, token array,
raw frame, and fixed Range model. It publishes workspace requirements only
after every check passes. The maximum-frame test verifies the `18F+85`
serialized-frame ceiling at `F = 65,536`.

This helper does **not** parse bytes, inspect magic/reserved fields, decode
Range payloads, or authorize a stream. A subsequent byte-level parser and
32-context decoder must call the semantic preflight before allocation or raw
publication; only then can the reserved stream gate be opened privately.

## Third implementation boundary

An isolated byte-level parser now checks the exact 112-byte stream header,
64-byte frame header, and 16-byte Range descriptor. It checks magic, version,
fixed sizes, algorithm IDs, zero-only reserved fields, and unsupported flags,
then invokes the semantic preflight before exposing parsed fields or workspace
requirements. It requires the declared payload to fit the supplied frame
bytes and reports the serialized frame extent; a caller may retain subsequent
bytes for later frames. It does not inspect or decode that payload, accept the
reserved identity through the published parser, or publish raw output. The
next boundary is the isolated 32-context Range decoder with token validation.

## Fourth implementation boundary

A separate fixed-storage Dynamic Range event decoder now holds exactly 4,538
frequencies and 32 totals. It preserves the published 31-context decoder's
storage and byte behavior. The new decoder checks the 32-context descriptor,
payload extent and leading state byte, enforces each requested context's
alphabet and decision count, resets its models on every `begin`, and verifies
event/decision counts, payload exhaustion, and model totals on `finish`. The
five-event `aaaa` payload and the newly added context 31 have independent
tests. Semantic frame preflight now counts the full decoder object size in
its aggregate workspace requirement. This event decoder cannot yet turn a
payload into typed tokens or reconstruct raw bytes; the reserved stream gate
remains closed.

## Fifth implementation boundary

The private 32-context Range decoder now maps events to typed LZSS tokens.
It uses the variant-6 `V = L - 2` inverse, length classes 0..8 and distance
contexts 23..31, then validates every token with dictionary variant 7 before
accepting it. It checks declared token/event/decision/raw counts and the
payload ceiling. A first pass validates without writing; a second pass writes
only to a nonoverlapping caller-owned token workspace of sufficient size.
The `aaaa` fixture yields `Literal(0x61), Match(1,3)`. Malformed count and
descriptor cases, insufficient output, and payload/output overlap have
separate negative tests. No raw bytes are reconstructed by this function;
frame-level atomic reconstruction and stream admission remain later gates.

## Sixth implementation boundary

A private frame decoder now chains the byte-envelope preflight, two-pass
Range-to-token decoding, and variant-7 typed-token reconstruction. It checks
the serialized frame, token workspace, raw workspace, and all pairwise memory
overlaps before writing either workspace. `serialized_consumed` is published
only after complete reconstruction; truncated or malformed input leaves raw
output untouched. The hand vector reconstructs `aaaa`, including when used as
a subsequent independent frame with sequence 1 and a four-byte committed
prefix. No published whole-stream parser or C API calls this frame decoder;
strict stream termination and cross-frame publication are later work.

## Seventh implementation boundary

A private strict one-shot stream decoder now composes the reserved stream
header parser and frame decoder. Empty input is exactly the 112-byte header
with original size zero and no frames. Nonempty streams require contiguous
frame sequence numbers and the declared raw partition; each frame resets its
dictionary history and Range models. An incomplete subsequent frame and any
bytes after the declared raw size are rejected. Before writing the caller's
whole-stream output, a validation pass decodes every frame into bounded
frame-local scratch. A second pass reconstructs and copies the verified raw
frames. Input and caller workspaces must remain stable throughout the call,
and all regions must be disjoint. Tests cover empty, one- and two-frame
streams, truncation, malformed later frames, trailing data, sequence errors,
insufficient workspaces, and overlap. This does not open the public streaming
decoder, selector, C API, or interoperability inventory.

## Eighth implementation boundary and candidate-selection experiment

A private forward modeler now converts a complete, validated variant-7 typed
frame into variant-6 modeled operations. It emits `L-2` length classes 0..8,
distance contexts 23..31, and only the required LSB-first bypass decisions.
The modeler plans event and decision counts with checked arithmetic before
writing into caller-owned storage, rejects aliases and insufficient storage,
and has hand-vector tests for lengths 3, 4, and 258. It does not yet produce
a Range payload or serialized frame.

For the first complete-payload experiment, compare three deterministic greedy
candidate parsers with minimum eligible match lengths 3, 4, and 5, otherwise
using the same longest-match and nearest-distance tie break, frame size,
window, and maximum length. Each candidate must be encoded with this same
reserved identity and evaluated by **complete serialized frame size**, not
token count or an isolated token-cost estimate. On equal frame size, prefer
the higher minimum eligible length. This is an experimental encoder-selection
policy, not a decoder-visible parameter or a
claim that any short match is profitable. Record full-archive size and
encode/decode time against BM-0100 before considering public admission.

## Ninth implementation boundary

A private Dynamic Range encoder now accepts the 32-context modeled-operation
sequence. It resets the 4,538-frequency bank and 32 totals for each call,
uses the existing Format 2.0 `3/2` integer interval and five-shift finalization
rules, and writes a descriptor with context count 32. A planning pass checks
operation fields, decision count, exact payload size, and caller hard limits
before any payload or descriptor publication. Encoding then rejects aliased or
undersized output. The hand-vector operations reproduce payload
`00 30 BF FF 9E 80 00` byte-for-byte; tests also cover bypass classes and
model-frequency rescaling against the private decoder. This is not yet a
serialized frame encoder or a candidate-size comparison. Published
31-context encoder behavior and public admission remain unchanged.

## Tenth implementation boundary

A private frame encoder now accepts an already selected, complete variant-7
typed-token frame. It validates frame position and raw partition, models the
variant-6 operations, plans the exact Range payload, checks the reserved
frame preflight and aggregate workspace limit, then writes the 64-byte frame
header, 16-byte descriptor, and payload. The fixed header and descriptor are
serialized explicitly in little-endian form and copied only after payload
encoding succeeds. Caller token, operation, and serialized-output regions
must be disjoint. The hand frame is exactly 87 bytes and round-trips through
the private decoder; a later frame with a 258-byte match also round-trips.
This function does not search for matches or choose among candidate parses,
write a stream header, or open public admission. Complete-frame comparison
and real-corpus measurement remain subsequent gates.

## Eleventh implementation boundary

A private reference parser now derives deterministic variant-7 typed tokens
directly from a bounded raw frame. It searches for the longest available
match, prefers the nearest distance on equal length, and emits a match only
when its length meets the candidate threshold 3, 4, or 5. Each candidate
starts from the same raw input and fresh dictionary state. Planning counts
tokens and checks the frame and aggregate workspace limits before writing;
tokenization rejects short or overlapping output storage. The exhaustive
finder intentionally favors a simple, inspectable reference policy over
corpus-scale encode speed.

A private selector evaluates all three candidates through the complete
variant-7 frame planner, including typed-token modeling, Dynamic Range
payload, descriptor, and frame header. It selects the smallest serialized
frame and breaks equal-size ties in favor of the higher threshold. The
chosen candidate is regenerated before the frame is written, so the parser
threshold remains encoder-only and is not stored in the stream. Tests cover
short-match hand cases, nearest-distance tie-breaking, exact frame-size
comparison, private decode of the winner, limits, and buffer overlap. This
does not change any published encoder, admit the reserved format publicly,
or establish a corpus compression-rate or throughput improvement. Bounded
corpus measurement and a faster equivalent search remain later gates.

## Twelfth implementation boundary: bounded corpus pilot

A private benchmark accepts a local input path, a maximum of 1..1,024
frames, and a frame size of 1..65,536 bytes. It measures a complete sample
archive size for the published 64-KiB HashChain/field-context/Dynamic Range
baseline and the reserved short-match selector at the same raw partition.
Every selected private frame is decoded and compared with its source before
the benchmark reports a result. Timing distinguishes baseline *size planning*
from selected-frame *encoding*; those times are not an encode-throughput A/B
comparison. Input samples and generated output remain outside version control.
The 64-KiB-frame `mozilla` pilot is recorded in BM-0101. The exhaustive
candidate parser remains far too slow for corpus-wide use; short-prefix
indexing with an exact-match oracle is the next optimization gate. No public
admission follows from the bounded pilot.

## Thirteenth implementation boundary: exact short-prefix index

A private 3-byte-prefix HashChain uses 65,536 buckets and one link per raw
frame byte (524,288 bytes at a full 65,536-byte frame). Hash collisions are
checked against all three source bytes. Positions are inserted in raw order,
including bytes skipped by a Match, so chain traversal visits the nearest
distance first. A candidate replaces the best Match only on strictly greater
length; a safe best-length byte probe may skip candidates that cannot win.
The index accepts caller-owned aligned storage, validates disjoint regions
and hard limits, and resets independently for each frame and candidate.

The original exhaustive parser and selector remain the oracle. Tests compare
every query on bounded hand/random inputs, candidate tokens for thresholds
3/4/5, and the final serialized frame bytes. The benchmark can run either
search mode; both select identical frame sizes in its tracked smoke fixture.
BM-0102 records the indexed full-Silesia measurement. Despite a small
aggregate gain and practical pilot speed, seven of twelve corpus members
grow, and `mozilla` still misses the stated gzip target. No public encoder,
interoperability archive, or automatic stream-level selection is admitted.

## Fourteenth implementation boundary: matched-token size diagnosis

The private benchmark now reports complete archive sizes for each fixed
eligibility 3/4/5 and the framewise selector, alongside the published
baseline. It also replays the eligibility-5 tokens through the published
31-context Range model and compares those tokens field-by-field with the
production HashChain output. On the external Silesia corpus, all 3,239
frames have identical eligibility-5 and production token sequences, and the
published-model archive sizes therefore coincide. Yet the reserved
32-context representation of those same tokens is larger on every corpus
member, by 551,143 bytes in aggregate. This separates the observed loss
from a match-finder difference under these conditions; it does not prove
which context or symbol mapping accounts for the excess. BM-0103 records
the measurements. Keep the reserved identity private and investigate its
model/representation cost before any stream-level format or API change.

## Fifteenth implementation boundary: operation-level attribution

The private benchmark counts length and distance symbols and their logical
LSB-first bypass bits for the same eligibility-5 tokens in both mappings.
BM-0104 finds equal match-symbol counts and equal distance-bypass totals
across all Silesia members, but 8,376,449 additional length-bypass bits
in the reserved `length - 2` mapping. Range coding and adaptive models make
this a diagnostic count, not an exact byte-cost decomposition.

One next representation hypothesis is to retain the published `length - 4`
classes 0..7 for matches of length at least five and use class 8 plus one
bypass bit for lengths three and four. This would keep long-match length
bypass widths and distance contexts aligned with the published 64-KiB
mapping while isolating short matches. It changes decoder-visible length
semantics, so it requires its own fully specified reserved revision, hand
vectors, decoder-first validation, and complete-frame measurement before
any admission. The current variant-7/variant-6 representation is unchanged.

## Sixteenth implementation boundary: isolated escape-length primitive

Format 2.0 now reserves a *different*, still-private identity
`dictionary 2/8 + context 1/7 + entropy 3/2`. Its complete length field
is specified in `docs/format.md`; all other short-match frame and Range
rules are inherited explicitly from the previous reservation. A bounded,
allocation-free primitive maps every length 3..258 to a class, bypass
width, and extra value, and validates the inverse. Classes 0..7 retain
the published `length - 4` mapping for lengths 5..258; class 8 uses one
bit for lengths 3 and 4. Of the possible class-7 extras, 127 decodes to
259 and is invalid. Tests cover hand boundaries, all 256 permitted lengths,
unique canonical decoding, malformed fields, and rejection of the new
identity by the existing private preflight. There is no Range payload,
frame parser, encoder, public admission, or archive for this identity yet.

## Seventeenth implementation boundary: escape-operation inversion

The private 2/8 + 1/7 identity now has an allocation-free decoder-side
validator for modeled operations and an inversion step to typed LZSS tokens.
It checks the 32-context shape, canonical length class and bypass width,
distance history, frame output, declared operation/decision/token counts,
hard limits, and output-buffer overlap before writing tokens. Inversion
validates the entire operation sequence first, so malformed input leaves
the caller's token buffer untouched. Every length 3..258 is covered by an
operation-level round trip, with separate malformed-operation tests.
The existing public parser, Range payload and frame paths still reject or
do not route to this identity. No encoder or archive has been introduced.

## Eighteenth implementation boundary: private Range payload decoding

The private 2/8 + 1/7 + 3/2 identity now decodes a bounded Range payload
directly into typed tokens. It reuses the existing 32-context arithmetic
model and selects only the independent length interpretation: class 8 reads
one bypass bit for length 3 or 4, while classes 0..7 decode `length - 4`.
The previous 2/7 + 1/6 path retains its original interpretation and tests.
Before token publication the new path validates the descriptor, declared
counts, compressed-size ceilings, history distance, raw output, hard limits,
strict coder termination and payload/output disjointness. Tests encode
first-party modeled operations and decode every length 3..258, as well as
forbidden length 259 and malformed or trailing payloads. The public stream
preflight still rejects this identity; frame parsing, streaming and a
complete encoder remain outside this boundary.

## Nineteenth implementation boundary: private complete-frame decode

The isolated 2/8 + 1/7 + 3/2 identity now has separate private stream-
header and frame preflight entry points, followed by complete-frame token
decode and raw reconstruction. The existing parser and decoder for the
2/7 + 1/6 identity retain their exact selection and semantics. Shared
preflight applies strict format fields, sequence and raw-frame size,
descriptor/count ceilings, aggregate workspace limits, truncation and
reserved-byte rules before exposing a frame layout. The frame decoder
checks token/raw capacities and all workspace overlap before decoding,
and malformed entropy never publishes raw bytes. Hand-built frame tests
cover lengths 3, 4, 5 and 258, wrong IDs, invalid framing, overlap and
limits. The published stream parser still rejects the new identity; this
boundary does not add an encoder, streaming API or public admission.

## Twentieth implementation boundary: private complete-frame encode

The isolated 2/8 + 1/7 + 3/2 identity now maps validated typed tokens to
modeled operations and encodes one complete frame. Lengths 3 and 4 use
class 8 with one bypass bit; lengths 5..258 use classes 0..7 and the
canonical `length - 4` value. The existing 32-context Range writer and
fixed frame layout are reused; only the length interpretation changes.
Planning checks the exact stream identity, frame position, typed tokens,
event and decision counts, payload size, preflight limits and aggregate
workspace before encoding. The encoder checks caller-owned workspace
overlap and output capacity before writing, then commits the fixed header
and descriptor only after the payload matches the plan. Tests exercise all
256 allowed match lengths at the operation boundary, boundary frame
round trips, wrong identity, invalid tokens, insufficient storage and
overlap. The old private identity retains its hand-vector bytes. No
published stream selector, CLI, C API or streaming encoder admits 2/8.

## Twenty-first implementation boundary: private strict stream decode

The isolated 2/8 + 1/7 + 3/2 identity gains a private, one-shot stream
decoder. It checks the exact stream header, walks all complete frames in
sequence, rejects truncation and trailing bytes, and validates every frame
before writing any whole-stream raw output. A second pass reconstructs the
raw bytes using caller-owned bounded token and frame workspaces; all input,
workspace and output regions must be disjoint and stable throughout the
call. An empty stream has only its header. This is not an incremental
streaming API and does not admit the identity through the public parser,
CLI, C API or interoperability inventory.

## Twenty-second implementation boundary: private strict stream encode

The 2/8 + 1/7 + 3/2 identity may be assembled privately from one complete
typed-token span per fixed raw frame. The caller retains ownership of every
token span and one reusable modeled-operation workspace. Planning validates
the exact stream identity, required frame count, each frame's typed tokens,
frame position and all serialized sizes before any stream bytes are written.
Encoding checks that the output is large enough and disjoint from frame
descriptors, tokens and operation storage, emits each frame, and commits the
112-byte stream header only after all frames match the plan. An empty stream
contains only that header. This stage does not tokenize raw input, select a
search policy, offer an incremental writer, or admit the identity publicly.

## Twenty-third implementation boundary: private reference tokenization

The 2/8 + 1/7 + 3/2 identity now has an exhaustive, deterministic raw-byte
to typed-token reference parser for one bounded frame. Match eligibility
3, 4 or 5 is an encoder-local choice, not a stream field. The existing
first-party match finder and nearest-distance tie rule are unchanged;
the new entry points select exact variant-8 parameter validation before
parsing. Planning measures token count and storage without publishing
tokens. Materialization checks capacity and raw/output overlap before
writing. Small hand vectors and a mixed binary vector compare the old and
new token choices, then pass the new tokens through the private frame
encoder and decoder. Indexed search, candidate-size selection, raw-input
stream assembly and all public admission remain separate stages.

## Twenty-fourth implementation boundary: private indexed tokenization

The 2/8 + 1/7 + 3/2 identity may also tokenize one bounded raw frame using
the existing exact three-byte-prefix index. Its caller-owned workspace is
calculated and initialized with explicit variant-8 parameter validation,
while existing variant-7 entry points retain their contract. Planning and
materialization use the same nearest-first chain and 3/4/5 encoder-local
eligibility, so indexed tokens must exactly match the exhaustive reference
for the same frame. Capacity, alignment, workspace limits and all buffer
overlaps are checked before token output is written. This boundary does not
choose between candidates, assemble raw-input streams or admit variant 8
through any public API.

## Twenty-fifth implementation boundary: private candidate-size selection

For one bounded raw frame of the 2/8 + 1/7 + 3/2 identity, plan exact
complete-frame sizes for eligibility 3, 4 and 5. Choose the smallest full
serialized frame; a tie prefers the higher eligibility. The same rule
applies to exhaustive and exact indexed tokenization, while indexed finder
workspace remains charged to the aggregate hard limit. Materialization
recreates only the winning candidate and verifies its planned size before
committing the frame. Eligibility and search method remain encoder-local;
neither changes a format field. This does not add raw-input stream assembly,
an incremental writer or public admission.

## Twenty-sixth implementation boundary: private raw-input stream assembly

The 2/8 + 1/7 + 3/2 identity may now be assembled privately from one
caller-owned raw byte span. Its declared original size must match that span;
fixed-size raw frames are selected independently, with only the last frame
short. Both exhaustive and indexed paths reuse caller-owned token and
modeled-operation storage, and the indexed path reuses one bounded finder
workspace. Planning sums complete selected-frame sizes before output, while
encoding checks all region overlap and capacity and commits the canonical
112-byte stream header only after all frames match the plan. Empty raw input
produces only the header. This remains one-shot and private: there is no
incremental writer, CLI/C API selector, public parser admission or
interoperability inventory entry.
