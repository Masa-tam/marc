# Position-distance rANS, 1 MiB

DD-1513 defines the native descriptor boundary before its encoder. The planned
profile is `lzss-position-rans-1m`. This boundary does not yet admit an outer
stream identity, public factory, CLI profile or exchange archive. Existing
contextual rANS and position-distance Dynamic Range representations remain
unchanged. A separate identity must be specified before outer admission.

Use the existing 44-context position-distance 1 MiB grammar, 2,566 frequency
entries, table log 12, total 4,096 and byte-renormalized 64-bit rANS payload
from DD-1512. Length extras are uniform binary decisions; distance extras
use contexts 24 through 43 by bit position. Context selection is derived from
the validated token grammar, never supplied by the payload.

The descriptor is sixteen metadata bytes followed by the six-byte active
mask and canonical compact records of PDRX version 2. Metadata, in order:
decisions u32, payload bytes u32, table log u8=12, flags u8=0, contexts u16=44,
frequency entries u32=2566. All integers are little endian. Records follow
ascending active context IDs. Single-symbol mode 0 contains one u8 symbol
with implicit frequency 4096. Dense mode 1 stores alphabet-minus-one u16
frequencies and infers the final frequency. Sparse mode 2 stores a u16 count,
ascending (u8 symbol, u16 frequency) entries except its final u8 symbol,
whose frequency is inferred. Sparse entries are positive. Every active
context sums to 4096. One symbol selects mode 0; otherwise choose dense when
`1+2*(alphabet-1) <= 1+3*nonzero_count`, sparse otherwise. Reject any other
mode choice, unused high mask bits, impossible sum, duplicate or unordered
symbol, truncation or trailing bytes. Descriptor bounds are 22..5110 bytes.

The native descriptor boundary allows the isolated empty representation:
zero decisions, zero active contexts, payload size eight. A nonempty descriptor
requires at least one active context. Payload bounds are 8..8+2*decisions;
the outer grammar will additionally bound decisions against frame and token
counts. Decoder limits count decisions in `max_block_size`, payload bytes
in `max_compressed_payload_size`, and descriptor staging bytes in
`max_internal_buffered_bytes`. Parsing uses fixed local storage and assigns
the destination only after validation. Serialization stages all bytes before
copying, leaving the destination and byte-count argument unchanged on error,
including overlapping input/output storage. Outer aggregate workspace bounds
remain a separate admission requirement.

Qualification compares compression against contextual rANS. Shared-token
comparisons isolate entropy/model changes; full codec comparisons also report
tokenization policy. Dynamic Range ratio is not an acceptance condition.
Measure encode speed, decode speed and peak memory separately. Huffman,
rANS and tANS may serve different resource tradeoffs; no advantage is assumed.

Native payload core uses state lower bound 2^31 and boundary state below
256*2^31. Encoder processes decisions in reverse. For cumulative start c,
frequency f, renormalize while state >= ((2^31 >> 12) << 8)*f, emitting
the low byte and shifting right eight. Update to
`4096*(state/f) + state%f + c`. Store the final state in eight little-endian
bytes followed by emitted bytes in reverse emission order. Uniform binary
decisions have starts 0/2048 and frequency 2048.

Decoder finds the interval containing `state&4095` in the requested context,
updates to `f*(state>>12) + (state&4095)-c` and consumes bytes while below
2^31. The initial implementation uses bounded cumulative search, retaining
the reference path before adding decode tables. Finish requires exactly the
declared decisions, exact payload exhaustion, terminal state 2^31 and use
of every active context. Begin validates into a candidate object before
assignment. A failed read does not change its symbol destination; the decoder
enters a sticky error state. These are private primitives; caller-visible raw
output still requires successful grammar, frame and integrity validation.

The model builder counts forward decisions in bounded fixed arrays. Normalize
each active context with proportional integer floors retaining each observed
symbol at frequency at least one; adjust toward 4096 by greatest signed
`count*4096-frequency*total` on increment (lower symbol on ties), least on
decrement among frequencies above one (higher symbol on ties). Commit the
model only on success. Reverse writer first measures exact payload extent
without writes, then emits into exact-size private scratch. It may modify
private scratch before detecting an error; frame encoders must never publish
that scratch before every subsequent validation succeeds.

## DD-1514 typed-token boundary

Connect the existing typed dictionary variant 9 (window at most 1 MiB,
minimum length 3, maximum at most 258) without allocating modeled operations
or materializing decisions. A forward field cursor supplies the model;
reverse token traversal supplies the writer. Reverse literal contexts are
found with one monotonically decreasing predecessor cursor, keeping traversal
linear in tokens. Both traversals must reproduce the finite diagnostic bytes.

Planning validates the entire typed frame and counts before assigning its
descriptor. Encoding plans first, checks capacities and disjointness against
every input/configuration/output region, then writes. Configuration and input
must remain immutable during a call. The measured reverse pass proves the
exact payload capacity before the emitting pass; descriptor output is committed
last. All deterministic admission failures leave caller outputs unchanged.

Token decoding derives requested fields from the same cursor, rejects invalid
short-length escapes and impossible distance extras, and validates each token's
history and raw extent before accepting it. A validation pass writes nothing;
transactional decode validates first, then checks output capacity/overlap and
repeats to commit. A separate single-pass private-scratch entry point may retain
a prefix on failure and must never feed reconstruction/publication unless the
entire operation succeeds. Capacity/overlap failures use the transactional
path to retain error precedence. Counts require tokens <= raw <= 1 MiB,
events <= 5*tokens and <= 2*raw, decisions <= 31*tokens and <= 9*raw, and
payload <= 2*decisions+8 and <= 18*raw+8. Empty isolated input has zero counts.

Aggregate admission includes token storage, encoded descriptor/payload bytes
and a conservative 64 KiB fixed working allowance covering nested fixed
model/descriptor/cursor objects. This is a bounded admission allowance, not
a process resident memory measurement; outer frame/finder generations need
additional accounting. Output-already-committed contributes to the total
output ceiling through checked addition.
