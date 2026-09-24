# LZSS Contextual 64-KiB short-match candidate

Status: decoder-visible reservation with private frame and strict one-shot
whole-stream decoders (2026-09-24). No encoder, public selector, or
interoperability archive admits this identity yet.

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
