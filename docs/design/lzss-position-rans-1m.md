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

## DD-1515 outer representation and complete-frame boundary

Reserve dictionary `2/9`, context `1/10`, entropy `4/4` for this profile.
The existing contextual rANS entropy `4/3` grammar remains unchanged and
rejects this identity. This additive format is initially admitted only by
the new private frame helpers; public/CLI/exchange admission follows later.

The stream header is 112 bytes: common MARC 2.0 prefix of 64 bytes,
16 dictionary parameter bytes, 16 entropy parameter bytes and a 16-byte
context extension. Exact offsets and fields:

| Offset | Type | Value |
|---:|---|---|
| 0 | 4 bytes | ASCII MARC |
| 4, 6 | u16, u16 | major 2, minor 0 |
| 8, 10 | u16, u16 | prefix bytes 64, flags 1 (typed-token pipeline) |
| 12, 14 | u16, u16 | dictionary 2, variant 9 |
| 16, 18 | u16, u16 | entropy 4, variant 4 |
| 20 | u32 | frame size in raw bytes, 1..1048576 |
| 24 | u32 | entropy block size 0 (outer frame controls the block) |
| 28, 32, 36 | u32 each | dictionary bytes 16, entropy bytes 16, hash bytes 0 |
| 40 | u64 | known original raw size, including zero |
| 48 | u32 | context extension bytes 16 |
| 52 | 12 bytes | zero reserved |
| 64, 68, 72, 76 | u32 each | window 1048576, minimum match 3, maximum match 258, flags 0 |
| 80, 81 | u8 each | table log 12, state count 1 |
| 82 | u16 | context count 44 |
| 84 | u32 | frequency entries 2566 |
| 88, 92 | u32 each | entropy flags 0, reserved 0 |
| 96, 98 | u16 each | context algorithm 1, variant 10 |
| 100 | u32 | context flags 0 |
| 104 | 8 bytes | zero reserved |

Dictionary parsing uses the typed variant's validator, not the byte-oriented
minimum-five parameter validator. No unknown-size marker is supported. Empty
stream consists solely of its header. Frame size is immutable and may be
smaller than the fixed dictionary window. Each frame resets history and models.
All integers retain little-endian order; there are no separate packed bits
or padding after the byte-renormalized entropy payload.

Each frame is 64 bytes of MRF2 header, then DD-1513 descriptor, then payload.
Header fields: magic at 0, size u16=64 at 4, flags u16=0 at 6, sequence u64
at 8, raw bytes u32 at 16, token count u32 at 20, event count u32 at 24,
decision count u32 at 28, payload bytes u32 at 32, descriptor bytes u32 at 36,
context side data bytes u32=0 at 40, checksum trailer bytes u32=0 at 44 and
16 zero reserved bytes at 48. No raw fallback is admitted. This profile has
no stored hash descriptors/trailers; composable external HashTap remains usable.

Sequence starts at zero and corresponds exactly to committed_raw/frame_size;
committed raw is aligned to the frame size. Every nonfinal frame has exactly
frame_size raw bytes; final has the exact remaining known raw bytes. Nonempty
frames require the DD-1514 count bounds; descriptor is 22..5110 bytes and
payload is 8..18*raw+8 bytes. Validate the 64-byte header before any variable
buffering, then the complete bounded descriptor before accepting payload.
Preflight success proves sizes and model canonicality, not payload validity.

The exact serialized ceiling is `18*frame_size+5182`. Complete-frame admission
accounts for serialized bytes, `token_count*sizeof(LzssTypedToken)`, raw bytes
and the DD-1514 fixed working allowance, plus all configured expansion/count
ceilings. Output requirements are committed only on successful preflight.
The reference model's 2566 frequency entries count against the configured
entropy table entry ceiling; no 4096-slot-per-context decode tables are built.
Finite frame decode consumes exactly one frame and allows the caller's
subsequent bytes; the streaming layer must reject extra bytes after the stream.
Transactional failure retains token and raw outputs. Scratch decode may retain
private tokens but always retains failed raw output. Reconstruction and frame
publication begin only after token grammar, history, sizes and rANS terminal
conditions succeed. Whole-stream publication is atomic per frame, not per
stream; an earlier successful frame remains committed if a later frame fails.

## DD-1516 borrowed streaming decoder

The borrowed decoder owns no dynamic storage. Supplied serialized, token and
raw workspaces are disjoint and live for its lifetime. Constructor admission
charges all supplied capacities, the decoder object, optional owner overhead
and the 64 KiB bounded call allowance. Reject overlap, invalid limits and
overflow before processing. Calls allocate nothing and never grow workspaces.

Collect the stream header, then each 64-byte frame header. Validate counts and
required workspace capacities before collecting the bounded descriptor;
validate the descriptor before collecting payload. Only successful complete
scratch-frame decoding enters the draining state. No bytes of a failed frame
reach caller output, including a late terminal-state error. Earlier successfully
drained frames remain committed. Error positions are absolute stream offsets.

EndInput is remembered once the supplied final suffix has been consumed.
Callers resubmit any unconsumed suffix with EndInput still set. Finish drains
all pending raw bytes, rejects trailing stream bytes and then returns EndOfStream.
Ended/error states are sticky; repeated ended calls return EndOfStream with
zero counts. Flush preserves framing. ResetBlock and unknown flags are
unsupported. Empty input starvation and zero output capacity are normal;
Progress always has nonzero consumption or production. Input/output must also
be disjoint from each other, the decoder object and retained workspaces.

## DD-1517 owned streaming encoder and decoder

Owned creation first validates configuration and computes all retained buffer
capacities, object bytes and the existing 64 KiB bounded-call allowance with
checked arithmetic. It refuses an insufficient aggregate budget before any
allocation. Allocation failure returns out_of_memory and releases earlier
allocations. Requirements outputs remain unchanged on refusal. No process
call allocates or grows storage.

The encoder retains one raw frame, at most frame_size typed tokens, serialized
capacity `18*frame_size+5182` and the existing exact three/four/five-prefix
finder workspace. Eligibility is fixed at three. Greedy longest matching and
nearest distance on ties preserve variant-9 token semantics. The exhaustive
matcher remains a small-input reference. There are no materialized entropy
operation or decision arrays. Models and dictionary history reset per frame.

The encoder emits the fixed stream header, collects exactly one frame, prepares
private tokens and serialized bytes, then drains that complete successful frame.
Failure cannot publish a partial frame. Known original length is enforced;
premature EndInput, additional raw input, ResetBlock and unknown flags fail.
Flush does not change bytes or frame boundaries. Zero-byte final EndInput is
valid once all declared raw bytes have arrived. Ended and error states are
sticky. The owned decoder retains serialized/raw/token buffers and wraps the
borrowed decoder, charging owner overhead in its admission calculation.
The decoder capacity also caps the admitted stream frame size; a large declared
frame cannot bypass that cap merely because its final raw extent is small.

Workspace capacity is a conservative representation bound, not measured
process peak. Compression ratio, directional throughput and process peak
remain independently qualified before public defaults and CLI admission.

## DD-1518 entropy-independent token eligibility diagnostic

The decoder grammar continues to accept match lengths 3..258. A private
encoder diagnostic may select fixed eligibility three or five, without changing
the dictionary parameters, frame grammar or model layout. Eligibility changes
greedy token selection; it is not a decoder-visible representation change.
Each fixed policy must retain exhaustive nearest-first reference agreement
and identical bytes across chunking. Compare complete native archives and
roundtrips against contextual rANS before selecting the public encoder policy.
Do not infer throughput from runs that combine candidate encoding and decoding.

## DD-1519 fixed-five encoder policy admission

Complete native archives over all twelve corpus members show fixed-three
eligibility reduces aggregate bytes by 1.5740% but grows five members versus
contextual rANS. Fixed-five eligibility reduces aggregate bytes by 2.1506%
and is smaller for every member. Its 207 frame descriptors and payloads equal
the earlier independently qualified shared-token components. Both native
candidates and the contextual control roundtrip every member.

Select fixed-five eligibility as the owned encoder default and the future
public profile policy. Retain fixed-three as a private diagnostic. This leaves
the minimum-three decoder grammar intact and does not use Dynamic Range
compression as an admission criterion. No two-policy trial or adaptive choice
is made in the production inner loop. Directional speed and measured process
peak remain required before CLI resource/default qualification.
## DD-1520 public owning C boundary

`marc_lzss_position_rans_1m_config_init(direction, config)` initializes a
separate additive ABI-1 configuration. The profile fixes dictionary 1MiB/3/258,
entropy 4/4 and encoder eligibility five. Default frame capacity is 1MiB,
decision ceiling 9Mi, payload ceiling 18MiB+8, frequency-entry ceiling 2566,
total raw ceiling 1TiB and aggregate capacity budget 64MiB. Initial input and
output call capacities are 65,536 bytes. Callers may lower limits or provide
additional retained-byte charges. Original size is known on encoding;
decoding takes it from the validated stream header. Capacity-budget admission
does not promise an operating-system RSS ceiling.

`marc_lzss_position_rans_1m_resource_requirements` returns complete raw, token,
serialized, finder, fixed-call allowance, external charge and aggregate bytes
for the selected direction before allocation. Failure leaves the destination
unchanged. Charge declared external bytes and call capacities, the C handle,
boundary object and bounded public helper allowance in addition to all owner
storage. Overflow and too-small aggregate fail before allocation.

`marc_lzss_position_rans_1m_create` publishes a handle only after successful
owned allocation. Config/output-handle overlap is rejected without altering
the aliased destination. Other create failures set the handle to null. Calls
must not overlap the handle or boundary and must fit declared input/output
capacities; misuse becomes a sticky error. Existing Transform process/destroy
functions apply. Direction is immutable. The decoder also caps the stream's
declared frame size to configured capacity. Flush leaves bytes unchanged;
ResetBlock is unsupported; ended calls return EndOfStream with zero counts.

## DD-1525 canonical CLI spelling

The implemented selector is `lzss-position-distance-rans-1m`. The former
spelling in this chronological proposal is not a CLI alias. The
position-distance grammar and rANS representation are unchanged.
