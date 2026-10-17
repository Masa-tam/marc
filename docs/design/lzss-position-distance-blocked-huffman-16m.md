# Sixteen-MiB position-distance Blocked Huffman

## DD-1618: exact sixteen-MiB specialization

Specialize the first-party family in
`lzss-position-distance-blocked-huffman.md` for window16777216 bytes.
The selector is `lzss-position-distance-blocked-huffman-16m`, identity
dictionary2/12, context1/32, entropy2/7. Frame size is configurable1..16777216
raw bytes, default16777216; original size is known. Reset dictionary and
entropy state each frame. Match lengths are3..258 on wire, with fixed-five
encoder eligibility, longest matching and nearest-distance tie breaking.
Prior selectors, identities and representations remain unchanged.

Use the existing field grammar: preceding token kind, literal context, length
class and distance class. Length residuals are LSB-first uniform bits.
Distance residual bit p uses binary context32+p, low bit first. Contexts0..55
have alphabets `[2]*3 + [256]*17 + [9]*3 + [25]*9 + [2]*24`; pooled field
alphabets are `[2,256,9,25,2]`, extent4658. Groups0..2,3..19,20..22,23..31
and32..55 map to fields0..4. Tables0..4 are pools;5..60 are overrides.
The isolated distance class24 endpoint16777216 cannot occur in a valid reset
frame because a match needs preceding history and at least3 output bytes.

Retain DD-1610's exact complete canonical Huffman model records, ordering,
singleton handling, dense/sparse choice, bit reversal and strict override
savings rule; a cost tie retains the pool. Maximum code length15, five fields,
56 contexts and61 codebooks are bounded format constants. Every declared
field and override must be used at exact completion.

The descriptor prefix is24 little-endian bytes `<IIBBBBHHII`: decision count,
payload size, final valid bits, maximum length15, field count5, field mask,
context count56, reserved0, alphabet extent4658 and flags0. Seven context-mask
bytes follow. All56 mask bits are usable; there is no unused final mask bit.
Field-mask bits5..7 remain zero. Pooled records precede context overrides in
increasing field/context order. Mode0 is a singleton, mode1 packed dense code
lengths and mode2 sorted sparse symbol/length pairs. Code lengths are complete
1..15; reject impossible, noncanonical, truncated, unused or trailing records.
An odd alphabet's final dense high nibble is zero, including alphabet25.
Descriptor extent is31..2575 bytes. Empty entropy is31 zero-model bytes,
no payload and valid bits0. Otherwise final valid bits are1..8 with zero
unused high payload bits. Check all metadata before payload decoding.

The112-byte MARC2.0 stream header has magicMARC, u16@4/6/8/10=2/0/64/1,
u16@12/14/16/18=2/12/2/7, frame size u32@20, u32@28/32/48=16,
known original size u64@40, dictionary u32@64/68/72/76=16777216/3/258/0,
u8@80/81=15/5, context count56 u16@82, extent4658 u32@84 and
context u16@96/98=1/32. All unspecified bytes are zero, including52..63.
Empty input is header-only. Frames exactly partition the known raw extent.

MRF2 frame headers remain64 bytes: magic@0, extent64 u16@4, flags0 u16@6,
sequence u64@8, raw/token/event/decision u32@16/20/24/28 and payload/
descriptor/side/trailer sizes u32@32/36/40/44. Side/trailer and bytes48..63
are zero. Descriptor precedes payload. Sequences start0 and increment once
per committed frame. Parse sizes with checked arithmetic before allocation.
Keep entropy results, token history and reconstructed bytes private until the
entire frame is validated. A failed frame publishes no raw prefix; ordinary
caller outputs retain their defined failure invariants.

For T tokens, R raw bytes and D decisions in a valid reset frame, require
`1 <= T <= R <= frame_size`, `2T <= D <= min(34T,9R)` and
payload size at most `ceil(15D/8)`. The isolated grammar can use34 decisions
per match, but its endpoint class24/length3 case uses28 decisions for3 raw
bytes and is not a valid reset-frame history. Do not incorrectly assert9R
for that isolated case. In a valid frame distance is less than16777216,
so class<=23: length3/4 uses at most27 decisions and length>=5 at most33;
literals use2. Thus9R is a safe valid-frame bound. Full-frame maxima are
150994944 decisions,283115520 payload bytes and283118159 serialized frame
bytes (64+2575+payload). These are wire/admission bounds, not memory defaults
or physical process measurements.

## Independent format checkpoint and remaining work

TVG-1524 freezes267 positive and62 actual negative mathematical vectors,
including all56 upper symbols, the usable final mask bit, isolated distance
classes through16777216, dense descriptor ceiling, odd alphabet25 padding,
complete fifteen-bit codes and exact headers. The generator never calls
production code and rejects overwriting an existing frozen file.

TVG-1525 qualifies the bounded entropy core with267 independent positive
and62 negative wires per normal/direct ASan/UBSan build, independently
decoded production model plans and identical planned bytes across builds.
Ordinary destinations retain their commit-on-success guarantees; private
scratch and maximum-decision admission receive directed finite checks.
This checkpoint does not qualify dictionary history or raw frame publication.

TVG-1526 qualifies single-pass private token decoding and history validation
with76 independent positive and7 negative wires, independent production-wire
decoding, full-frame literal/match spans and exact/one-short storage grants.
Late token or entropy rejection retains ordinary destinations. Token scratch
may contain a private valid prefix and must be discarded after failure.

Frames/owners, public API/CLI, resource
admission/default construction, schema83/93 exchange and final audit remain
open. Further ratio/speed/physical-peak benchmarks and fuzz execution are
omitted by user request through this goal; do not label them passed. Select
future logical defaults from checked requirements and actual construction
correctness evidence, explicitly without new process-peak measurements.
