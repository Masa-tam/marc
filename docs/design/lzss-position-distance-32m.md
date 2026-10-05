# Thirty-two-MiB position-distance definition and resource diagnosis

DD-1488 defines the next profile before implementation. This is a reserved
representation and a diagnosed work plan. No new encoder, decoder, factory,
CLI selection or exchange archive is exposed by this stage.

## Representation

Reserve Format 2.0 dictionary 2/13, context 1/14, entropy 3/2. Extend the exact
sixteen-MiB position-distance representation: retain the 112-byte known-size
stream header, 64-byte frame prefix, 16-byte Range descriptor, all existing
field offsets, reserved-zero requirements and five-byte canonical termination.
All integers use explicit little-endian serialization. Numeric extra bits are
coded from least significant to most significant. No native structure is wire
data and no new flags or model table are serialized.

Window and frame maxima are F=33554432 uncompressed bytes. Smaller legal
parameters remain bounded by F and the explicit caller limits. Frames reset
dictionary history and every model. Original size is known, the final short
frame is valid and empty input has only its stream header. Sequence, original
size, frame size and strict trailing checks retain their existing definitions.

Wire match lengths remain 3..258. Lengths 3/4 use class 8 and one equiprobable
extra bit. For L>=5, c=floor(log2(L-4)), L=4+2^c+E, with c equiprobable extra
bits; class 0 has no extra event. Class 7 residual 127 is invalid because it
represents length 259. The encoder retains longest-match/nearest-distance ties
and canonical strict cost eligibility 9<2L, hence a minimum emitted length 5.
The decoder must still accept valid wire matches of lengths 3/4.

Distance D is 1..F with class c=floor(log2(D)), c=0..25 and D=2^c+E. Class 0
has no extra event. For c>0, residual bits p=0..c-1 use adaptive binary context
24+p. Class 25 permits only residual zero. D=F is a grammar bound, unreachable
inside a valid nonempty reset frame because history and the following match
must fit that frame. Reachable maxima are F-3 for wire length 3, F-258 for
length 258 and F-5 for encoder-eligible matches. These use distance class 24.

| Field | Context | Alphabet |
| --- | --- | --- |
| Kind | previous Start=0, Literal=1, Match=2 | 2 |
| Literal before any earlier literal token | 3 | 256 |
| Literal after stored literal B | 4+(B>>5) | 256 |
| Length class | 12+previous kind | 9 |
| Distance class | 15+length class | 26 |
| Distance extra bit p | 24+p, p=0..24 | 2 |

There are 49 contexts and 2621 flattened frequencies. Group offsets are
0, 6, 2310, 2337, 2571, 2621. Matches preserve the stored literal byte.
Frequencies start at one, increment by one per adaptive decision and rescale
with deterministic ceil-half at total 32768. Carry, normalization and finish
rules remain the existing byte-oriented integer Range rules. The descriptor
must declare 49 contexts; reject crossed identities or a 48-context descriptor.

For nonempty raw bytes R<=F, tokens T, field events E, decisions N and payload P:

```text
1 <= T <= R
2T <= E <= min(2R,5T)
E <= N <= min(10R,35T)
5 <= P <= min(2N+5,20R+5)
```

The decision bound is deliberately conservative. A reachable length-three
class-24 match has 28 decisions, exceeding nine per raw byte for that token.
All permitted length/class combinations fit 10 raw-byte decisions and 35
token decisions. A tighter whole-frame 9R bound might be possible from history
constraints; it has not been proved or disproved here. Do not import the old
per-token proof or its 18R payload bound without a separate proof.

Maximum conservative payload/frame sizes are 671088645/671088725 bytes.
These ceilings do not select an allocation policy. Validate descriptor/count
agreement, declared raw/history bounds, caller limits and checked arithmetic
before allocation. Require exact decoded raw count and canonical finish before
publishing. Any failure preserves the previous verified publication, metadata
and caller suffix; private scratch alone may be discarded. Nothing from a
failed frame may drain, even if a valid prefix of its tokens was decoded.

## Resource diagnosis

At a diagnostic 64-MiB payload capacity, retaining the measured 12-byte typed
token representation in two buffers gives:

```text
serialized capacity = 64 MiB + 80
two token capacities = 2 * 12F
two raw capacities   = 2F
five backing bytes   = 26F + 64 MiB + 80
```

| Frame/window | Five typed backing capacities, before controls |
| --- | --- |
| 16 MiB | 503316560 bytes (480 MiB + 80) |
| 32 MiB | 939524176 bytes (896 MiB + 80) |
| 64 MiB, comparison only | 1811939408 bytes (1728 MiB + 80) |

Thus a direct typed-pair extension cannot fit the existing sixteen-MiB CLI's
512-MiB logical budget, even before controls. One typed buffer would still
take 512 MiB plus 80 bytes and controls at F=32 MiB. Omitting a retained buffer
also requires an independent transaction/lifetime design; it is not a free
capacity reduction.

An alternative private compact representation uses two bytes for a literal
and nine for a match. Encoder-eligible matches give the known 2R bound. Decoder
wire matches may be length 3/4, requiring the separate conservative 3R bound.
One literal followed by an overlapping length-three match already takes eleven
record bytes for four raw bytes, exceeding 2R. Two 3F compact buffers, two F
raw buffers and the same serialized capacity total 335544400 bytes
(320 MiB plus 80), before controls. Dense valid length-three overlap records
approach that compact bound. Do not allocate a decoder using the encoder bound.

Separate fresh processes successfully allocate and touch the complete proposed
typed and compact backings. Their observed physical peaks are respectively
943939584 and 339959808 bytes. Existing sixteen-MiB numeric queries check
the same projected typed capacities, exact/one-below refusal and overflow as
an explicitly labeled proxy. That proxy retains sixteen-MiB controls, does not
query a future factory and does not validate a thirty-two-MiB payload.

These are single array experiments. They do not execute either proposed decoder,
measure codec speed/ratio, include future controls, or establish a CLI budget.
Count all future model, cursor, public-boundary, owner, helper, input/output,
caller-retained, temporary and prior-publication capacities before admission.
Actual complete-query and separate codec-process peaks are required before
selecting limits or watchdogs. Global defaults remain unchanged; no automatic
larger-budget retry is permitted.

## Next implementation gate

First implement a bounded model/field reference and independent recipes for
the new identity, including the class-25 grammar rejection in a reset frame.
Then prototype private compact-token decoding, reconstruction and canonical
termination while preserving a clear typed reference. Prove whole-frame equality,
token/metadata invariance on failure, all alias/capacity checks and private
publication before constructing public factories. Existing sixteen-MiB APIs
and their five-workspace contracts must remain intact.

Qualification must include empty/all-byte inputs, arbitrary split buffers,
one-byte output, F-1/F/F+1, contrasting and incompressible retained generations,
F-3/F-258/F-5 references, all valid wire length-three/four cases, allocation
refusals, exact/one-below complete budgets, integer overflow, late corruption,
sticky terminals and strict trailing input. Use explicit measured budgets;
do not remove full-window cases to make admission or timeouts pass.

Only after qualification should public API/CLI policy and append-only exchange
integration be chosen. The current 72 archives, selectors and fixed executable
are unchanged. No external implementation source was consulted.

## Qualified implementation status, 2026-10-06

The design gates above now have separate local evidence for bounded reference
and indexed dictionaries, typed and compact Range paths, finite frame encode/
decode, borrowed and owning streaming, six distinct public C entry points and
an explicit CLI selector. Complete maximum-window boundaries, arbitrary
chunking, valid short wire matches, all allocation refusals, exact/one-below
resource admission and late failed-frame nonpublication are qualified. The
old sixteen-MiB typed workspace ABI and all earlier representations remain
unchanged. Whole-library address/undefined-behavior instrumentation and bounded
fuzz campaigns have separate evidence; no source from external implementations
was consulted.

DD-1498 selects a 536,870,912-byte explicit CLI codec-accounting policy and
67,108,864-byte payload cap after complete owner/backing admission and actual
CLI process measurements; BM-0210 reports time, ratio and process peaks for two
full incompressible frames. Global defaults and the baseline codec matrix stay
unchanged. Schema 63/archive 73 is appended after freezing the old seventy-two
archives. The complete configured suite passes 4,041 tests, three local
production routes agree on all archive bytes and genuine schemas 1..63 pass.
This supersedes the prospective implementation status above. Hosted CI and
revision-specific external exchange remain separate maintainer-owned gates;
local qualification does not establish their success.
