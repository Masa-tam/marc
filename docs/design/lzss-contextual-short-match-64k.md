# LZSS Contextual 64-KiB short-match candidate

Status: decoder-visible reservation with private dictionary-token and
structured-header preflight validation (2026-09-24). No byte-stream parser,
entropy decoder, encoder, public selector, or interoperability archive admits
this identity yet.

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
