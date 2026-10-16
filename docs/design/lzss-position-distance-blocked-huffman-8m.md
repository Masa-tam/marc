# Eight-MiB position-distance Blocked Huffman

## DD-1616: exact eight-MiB specialization

This format specializes the family definition in
`lzss-position-distance-blocked-huffman.md` for window and maximum frame
8,388,608 bytes. Its selector is
`lzss-position-distance-blocked-huffman-8m`. The identity is dictionary2/11,
context1/31 and entropy2/6. Match lengths are3..258; known original size,
fixed-five eligibility, longest matching and nearest-distance tie breaking are
required. Earlier selectors and generated bodies are unchanged.

Each nonempty frame is an independently reset typed-event Huffman block.
Frame size is configurable1..8,388,608 raw bytes. The event grammar is the
same field grammar as the preceding family variants: preceding token kind,
literal value, length class and distance class are modeled; length and distance
residuals are LSB-first uniform bits. Contexts are 0..54 with alphabets
`[2]*3 + [256]*17 + [9]*3 + [24]*9 + [2]*23`; pooled fields are
`[2,256,9,24,2]`. The alphabet extent is4647. Distance class23 is the
isolated grammar endpoint and cannot be emitted by a nonempty reset frame whose
raw extent is at most the window.

The descriptor uses the 24-byte little-endian prefix
`<IIBBBBHHII`: decision count, payload size, final valid bits, maximum code
length15, five fields, field mask, context count55, reserved0, alphabet extent
4647 and flags0. Seven override-mask bytes follow; the upper bit of the final
byte and field-mask bits5..7 are zero. Pooled records precede context override
records in field/context order. Mode0 is a singleton, mode1 is packed dense
lengths and mode2 is sorted sparse symbol/length pairs. Singleton and dense vs
sparse selection is canonical, code lengths are complete1..15, and canonical
codes are reversed only at the LSB-first physical writer. Descriptor extent is
31..2563 bytes; all bounds and model usage are checked before payload decode.

The 112-byte MARC2.0 header declares the identity above, frame/window
8,388,608, dictionary minimum3/maximum258, maximum code length15, five fields,
55 contexts and extent4647. All unspecified fields, flags, side data and
trailers are zero. Each 64-byte MRF2 frame header carries raw/token/event/
decision counts and descriptor/payload sizes. Frames exactly partition the
known original size; empty input is header-only. Failed descriptors, tokens,
history and raw reconstruction remain private until the complete frame is
validated, and a failed frame publishes no raw prefix.

For T tokens, R raw bytes and D decisions,
`1 <= T <= R <= frame_size`, `2T <= D <= min(33T,9R)` and payload is at most
`ceil(15D/8)`. The full-frame checked bounds are D<=75,497,472,
payload<=141,557,760 and serialized frame<=141,560,387 bytes. These are wire
validation bounds, not memory defaults or physical peak claims. Resource
defaults require fresh normal measurements.

## Qualification checkpoint

The independent first-party oracle and frozen vectors generate196 positive and
44 negative cases, including every distance class through8,388,608, all55
contexts, full dense descriptor ceiling, complete fifteen-bit codes, exact
header identity and malformed metadata/truncation. Production implementation,
API/CLI, measurements, contextual comparison, fuzz, exchange and final local
audit remain unstarted for this window. No completion or hosted result is
claimed.

## Independent format qualification checkpoint

TVG-1512 freezes196 positive and44 negative mathematical cases. The oracle
checks the55-context partition,4647 extent,2563-byte descriptor ceiling,
distance classes through8,388,608, the33T/9R decision bounds and exact header
identity. No production source, API, CLI, measurement, exchange or fuzz work
is claimed by this checkpoint.
