# Stream format

The baseline stream prefix is assigned format version 1.0. Representations not
defined below remain incomplete. No encoder implementation may be added until
its complete decoder-visible layout is specified here and accompanied by
hand-checkable vectors.

Accepted baseline constraints:

- multi-byte integers are little-endian;
- bit payloads are LSB-first;
- the original uncompressed size is present and known;
- entropy blocks do not cross frame boundaries;
- one frame may contain multiple entropy blocks;
- the last entropy block in a frame may be short;
- no public standalone Static Huffman algorithm ID will be assigned.

ABI version 1 does not imply stream-format version 1. These namespaces evolve
independently.

## Version 1.0 stream header prefix

Every stream begins with this fixed 64-byte prefix. All integers are unsigned
little-endian values. The prefix is collected completely before semantic
validation; no variable region is allocated before its declared length passes
local decoder limits.

| Offset | Size | Field | Version 1.0 rule |
|---:|---:|---|---|
| 0 | 4 | magic | ASCII `MARC`, bytes `4D 41 52 43` |
| 4 | 2 | major version | `1` |
| 6 | 2 | minor version | `0` |
| 8 | 2 | fixed prefix size | `64` |
| 10 | 2 | feature flags | `0`; unknown bits are rejected |
| 12 | 2 | dictionary algorithm ID | table below |
| 14 | 2 | dictionary variant ID | `0` for None, otherwise `1` |
| 16 | 2 | entropy algorithm ID | table below |
| 18 | 2 | entropy variant ID | `0` for None, otherwise `1` |
| 20 | 4 | frame size | uncompressed bytes, nonzero |
| 24 | 4 | entropy block size | entropy input symbols; see rule below |
| 28 | 4 | dictionary parameter bytes | follows the fixed prefix |
| 32 | 4 | entropy parameter bytes | follows dictionary parameters |
| 36 | 4 | hash descriptor bytes | must be zero until descriptors are defined |
| 40 | 8 | original size | required uncompressed byte count |
| 48 | 4 | header extension bytes | must be zero in version 1.0 |
| 52 | 12 | reserved | all zero; nonzero is malformed |

After the prefix, regions occur in this order: dictionary parameters, entropy
parameters, hash descriptors, header extensions. Version 1.0 currently accepts
only the first two; their combined size must fit the configured internal-buffer
limit. A None algorithm must have variant zero and a zero-sized parameter region.

Entropy block size is nonzero only for Blocked Huffman, rANS, and tANS. It is
zero for None, Adaptive Huffman, and Dynamic Range Coder. The declared frame,
block, and original sizes must not exceed local decoder limits.

### Algorithm IDs

| Dictionary ID | Algorithm | Variant 1 |
|---:|---|---|
| 0 | None | variant 0 only |
| 1 | LZ77 | fixed canonical copy-token variant defined below |
| 2 | LZSS | explicit literal/match byte-token variant defined below |
| 3 | LZ78 | fixed phrase-index byte-token variant defined below |
| 4 | LZW | variable-width frame-local variant defined below |
| 5 | LZD | fixed phrase-pair Lempel-Ziv Double variant defined below |
| 6 | LZMW | fixed phrase-reference Miller-Wegman variant defined below |

| Entropy ID | Algorithm | Variant 1 |
|---:|---|---|
| 0 | None | variant 0 only |
| 1 | Adaptive Huffman | framed FGK baseline defined below |
| 2 | Blocked Huffman | canonical baseline defined below |
| 3 | Dynamic Range Coder | byte-oriented adaptive order-0 baseline |
| 4 | rANS | scalar 64-bit byte-renormalized baseline |
| 5 | tANS | table-based baseline |

Static Huffman has no public algorithm ID. IDs outside these tables and variant
IDs other than those listed are rejected.

### Hash algorithm IDs and digest bytes

The hash interface reserves algorithm ID `1` for CRC-32C (Castagnoli). This ID
does not enable a version 1.0 stream field: stream hash descriptor size and
frame checksum trailer size must remain zero until their complete layouts are
defined.

CRC-32C uses width 32, polynomial `0x1EDC6F41`, reflected input and output,
initial register `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`. The reflected update
uses polynomial `0x82F63B78`. Its numeric check value for ASCII `123456789` is
`0xE3069283`. A four-byte marc digest stores that numeric value little-endian,
so the check vector is `83 92 06 E3`.

Hash algorithm ID `2` is SHA-256 as defined by FIPS 180-4. Its digest is the
standard ordered 32-byte string produced by concatenating `H(0)` through
`H(7)`, each word most-significant byte first. Digest bytes are not reinterpreted
as a repository integer and therefore are not reversed. For ASCII `abc`, the
digest begins `BA 78 16 BF` and ends `F2 00 15 AD`.

### Hash descriptor record reserved for a later stream version

A hash descriptor has the following canonical 16-byte representation. This
record definition permits allocation-free parsing and validation before stream
integration. It does **not** permit a nonzero hash-descriptor region in version
1.0; version 1.0 decoders continue to reject one as an unsupported feature.

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | hash algorithm ID | `1` CRC-32C or `2` SHA-256 |
| 4 | 1 | target | table below |
| 5 | 1 | scope | table below |
| 6 | 2 | digest size | exactly 4 for ID 1; exactly 32 for ID 2 |
| 8 | 4 | flags | zero |
| 12 | 4 | reserved | zero |

Target IDs are `1` UncompressedBytes, `2` DictionarySerializedBytes, `3`
CompressedPayload, and `4` FrameCanonicalBytes. Scope IDs are `1` WholeStream,
`2` PerFrame, and `3` PerBlock. Unknown IDs, a digest-size mismatch, nonzero
flags, or nonzero reserved bytes are malformed.

The descriptor alone does not define where a digest is stored or its precise
inclusion range. A later stream-format version must define those properties,
supported target/scope combinations, and the trailer layout before enabling
this record in a stream.

A structurally valid descriptor region is empty or has a byte length that is
an exact multiple of 16. Records are strictly ordered by the unsigned tuple
`(target, scope, hash algorithm ID)`. Two records may use different algorithms
for the same target and scope, but an identical tuple is a forbidden duplicate.
An out-of-order region is noncanonical. Region parsing must validate every
record and the complete ordering before publishing any caller-owned descriptor
output. These region rules remain inactive in version 1.0 streams.

### Version 1.1 hash-prefix gate

Version 1.1 retains the 64-byte prefix layout and all version 1.0 field rules,
except that minor version is `1` and the hash-descriptor byte count may be
nonzero. That count must be an exact multiple of 16. Dictionary parameters,
entropy parameters, and hash descriptors together must fit the decoder's local
internal-buffer limit. Header extensions remain zero.

This helper is a prefix-level gate, not a general configurable version 1.1
stream format. It must not activate arbitrary target/scope combinations. The
complete `checksum-raw` profile below is its only public stream use and fixes
the descriptor, inclusion range, digest placement, and frame trailer. Existing
version 1.0 header entry points reject a 1.1 prefix so a descriptor region
cannot be mistaken for frame bytes. The separate 1.1 entry points reject 1.0
and every other version.

A hand-checkable empty-transform prefix declaring one 16-byte descriptor region is:

```text
4D 41 52 43 01 00 01 00 40 00 00 00 00 00 00 00
00 00 00 00 00 00 10 00 00 00 00 00 00 00 00 00
00 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

Hand-checkable records are:

```text
CRC-32C, UncompressedBytes, PerFrame:
01 00 00 00 01 02 04 00 00 00 00 00 00 00 00 00

SHA-256, UncompressedBytes, WholeStream:
02 00 00 00 01 01 20 00 00 00 00 00 00 00 00 00
```

### Version 1.1 per-frame checksum component

The descriptor set used by the current version 1.1 stream composition contains
exactly one record: CRC-32C, target UncompressedBytes, scope PerFrame,
digest size 4, and flags zero. No other target, scope, algorithm, or additional
descriptor is accepted by this profile.

Each nonempty frame declares a checksum trailer size of exactly 4. The trailer
follows the block descriptors and compressed payload and contains the CRC-32C
numeric result in marc's little-endian digest representation. The hash input is
exactly the frame's `uncompressed_size` logical output bytes in their decoded
order. It excludes the stream header, descriptor region, frame header,
dictionary serialization, block descriptors, compressed payload, padding, and
the checksum trailer itself. CRC state resets at every frame boundary. An empty
stream has no frames and therefore no per-frame trailer.

For a frame whose uncompressed bytes are ASCII `123456789`, the trailer is:

```text
83 92 06 E3
```

This component enables standalone validation, generation, and verification of
its trailer. The complete `checksum-raw` profile below wires it to the version
1.1 prefix and frame-header gates. All other currently public codec profiles
remain on version 1.0.

### Version 1.1 frame-header gate

The version 1.1 frame header retains the version 1.0 56-byte layout and
`MRF1` magic. Under the checksum component, `checksum trailer bytes` at
offset 36 is exactly little-endian 4 rather than zero. The stream prefix must
declare exactly 16 hash-descriptor bytes, the parsed region must contain the
single supported CRC-32C descriptor, and the frame header must declare the
matching four-byte trailer. Any disagreement is malformed before frame-body
processing.

The ordinary version 1.0 frame-header entry points continue to require a
version 1.0 stream context, no descriptor objects, and a zero checksum trailer.
The dedicated version 1.1 entry points require a 1.1 stream context and the
initial profile. Neither entry point accepts the other's version.

For a raw three-byte version 1.1 frame whose uncompressed bytes are `61 62 63`,
the frame header is the version 1.0 raw vector below except bytes 36 through 39
are `04 00 00 00`. Its body is payload `61 62 63`, followed by CRC-32C trailer
`B7 3F 4B 36`.

### Complete version 1.1 raw-checksum reference profile

The first complete version 1.1 stream profile selects dictionary None and
entropy None, has no algorithm parameter regions, and contains exactly the
single initial CRC-32C descriptor. Its byte order is:

```text
64-byte version 1.1 stream prefix
16-byte CRC-32C / UncompressedBytes / PerFrame descriptor
zero or more frames, each:
    56-byte version 1.1 frame header
    uncompressed bytes (also the compressed payload under None / None)
    4-byte CRC-32C trailer
```

The stream prefix's original size is known. Frames use the deterministic fixed
size/final remainder rule. Empty input is represented by only the 80-byte
prefix and descriptor and contains no frame or checksum. Strict decoding
rejects truncation at every byte, extra trailing bytes, any prefix/descriptor/
frame disagreement, and any checksum mismatch.

The reference decoder validates the entire stream and every checksum before
publishing any uncompressed output. Its second pass copies only already
validated raw payload spans. Thus corruption in a later frame cannot expose an
accepted prefix of output. This profile is initially an internal reference
composition. The dedicated `marc_checksum_raw_*` C ABI selects this version 1.1
profile; all previously published codec selectors continue to emit version 1.0.

Incremental encoding produces exactly the same bytes as one-shot encoding.
`Flush` does not end a partial raw frame; the fixed partition remains solely a
function of `original_size` and `frame_size`. Incremental decoding verifies the
four-byte trailer before releasing that frame's payload. Consequently checksum
failure suppresses the complete affected frame even when earlier frames have
already been released.

For raw input `61 62 63` in one frame, serialized size is 143 bytes: the
80-byte prefix and descriptor, the 56-byte checksum frame header, three payload
bytes, and trailer `B7 3F 4B 36`.

### Empty framing-only header vector

This vector selects no dictionary or entropy transform, a 1 MiB frame size,
and an original size of zero. Spaces separate bytes; line breaks have no format
meaning.

```text
4D 41 52 43 01 00 00 00 40 00 00 00 00 00 00 00
00 00 00 00 00 00 10 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
```

## Version 1.0 frame header

After the stream-level parameter regions, the stream contains frames until the
sum of frame uncompressed sizes equals the stream header's original size. An
original size of zero has no frames. Every frame begins with this fixed 56-byte
header.

| Offset | Size | Field | Version 1.0 rule |
|---:|---:|---|---|
| 0 | 4 | frame magic | ASCII `MRF1`, bytes `4D 52 46 31` |
| 4 | 2 | fixed frame-header size | `56` |
| 6 | 2 | frame flags | `0`; unknown bits are rejected |
| 8 | 8 | frame sequence | zero-based, exactly one greater per frame |
| 16 | 4 | uncompressed size | expected fixed size or final remainder |
| 20 | 4 | dictionary serialized size | entropy-decoder output bytes |
| 24 | 4 | compressed payload size | exact payload bytes in this frame |
| 28 | 4 | entropy block count | algorithm-specific bounded block count |
| 32 | 4 | block descriptor bytes | precedes compressed payload |
| 36 | 4 | checksum trailer bytes | must be zero until checksums are defined |
| 40 | 16 | reserved | all zero; nonzero is malformed |

The frame body order is block descriptors, compressed payload, then checksum
trailer. Version 1.0 currently requires a zero-sized checksum trailer.

Frame boundaries are deterministic. If `remaining = original_size - committed`
then the next frame's uncompressed size must equal
`min(stream_frame_size, remaining)`. Thus only the final frame may be short.
The decoder rejects a frame after the declared original size is reached.

With no dictionary transform, dictionary serialized size equals uncompressed
size. With no entropy transform, compressed payload size also equals that size,
and block count and descriptor size are zero. Blocked Huffman, rANS, and tANS
require nonzero block count and descriptor size. Adaptive Huffman and Dynamic
Range Coder variant 1 use exactly one descriptor and one entropy block per
nonempty frame.

### Raw three-byte frame vector

For a stream selecting no transforms, original size 3, frame size 1 MiB, and raw
bytes `61 62 63`, the frame header and body are:

```text
4D 52 46 31 38 00 00 00 00 00 00 00 00 00 00 00
03 00 00 00 03 00 00 00 03 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
61 62 63
```

The first 56 bytes are the frame header; the final three bytes are its raw
compressed payload.

## LZ77 variant 1

LZ77 variant 1 is a frame-local byte dictionary transform. Its stream dictionary
parameter region is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | window size | bytes; default 65,536; nonzero |
| 4 | 4 | minimum match length | default 3; at least 3 |
| 8 | 4 | maximum match length | default 258; at least minimum |
| 12 | 4 | flags/reserved | zero |

All integers are little-endian. Window size must not exceed the local maximum
LZ distance. Maximum match length must not exceed the local maximum LZ match
length. Dictionary history starts empty and resets at every outer frame; no
reference crosses a frame boundary.

The encoder parses raw frame bytes from left to right. At each position, search
distances `1..min(window_size, bytes_already_parsed)`. A candidate compares
bytewise and may overlap: candidate byte `i` is the raw input byte at
`position - distance + i`, including bytes within the same match. Choose the
longest match up to the configured maximum and remaining frame input; on equal
length choose the smaller distance. Matches shorter than the configured minimum
are not selected.

If no match is selected, emit Literal and advance by one raw byte. If the chosen
match reaches the frame end, emit TerminalMatch and advance by its length.
Otherwise emit MatchThenLiteral using the raw byte immediately after the match
and advance by `length + 1`. This greedy rule is deterministic.

Every dictionary token is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 1 | tag | 0 Literal, 1 MatchThenLiteral, 2 TerminalMatch |
| 1 | 3 | reserved | zero |
| 4 | 4 | distance | rules below |
| 8 | 4 | match length | rules below |
| 12 | 1 | literal | rules below |
| 13 | 3 | reserved | zero |

For tag 0, distance and length are zero and literal is the one raw byte. For tag
1, distance is `1..window_size`, length is within the configured match range,
and literal follows the copied bytes. For tag 2, distance and length follow the
same rules, literal is zero, the match must end exactly at the raw frame size,
and this must be the final token. Tag 1 must leave room for its following
literal. Any unused field or reserved byte must be zero.

The decoder copies match bytes one at a time from `output_position-distance`,
so overlap has defined repeating-copy semantics. Before every token it validates
distance against both configured window size and produced history, length
against configured and local limits, checked output extent, and the declared
raw frame size. Unknown tags, impossible references, premature token end,
output beyond the declared size, bytes after completion, and a TerminalMatch
that does not end the frame are malformed.

The dictionary serialized size in the generic frame header is exactly
`16 * token_count`. It is the byte input to the selected entropy layer. When the
entropy algorithm is None, compressed payload size equals dictionary serialized
size and the frame body contains these tokens directly. The worst-case reference
expansion is 16 serialized bytes per raw byte and must fit local buffered and
payload limits before allocation.

### Hand-checkable LZ77 token vectors

With default parameters, spaces divide fields only for readability:

```text
Input `A`:
00 00 00 00  00 00 00 00  00 00 00 00  41 00 00 00

Input `AAAA`:
00 00 00 00  00 00 00 00  00 00 00 00  41 00 00 00
02 00 00 00  01 00 00 00  03 00 00 00  00 00 00 00

Input `ABABA`:
00 00 00 00  00 00 00 00  00 00 00 00  41 00 00 00
00 00 00 00  00 00 00 00  00 00 00 00  42 00 00 00
02 00 00 00  02 00 00 00  03 00 00 00  00 00 00 00

Input `ABCABCX`:
00 00 00 00  00 00 00 00  00 00 00 00  41 00 00 00
00 00 00 00  00 00 00 00  00 00 00 00  42 00 00 00
00 00 00 00  00 00 00 00  00 00 00 00  43 00 00 00
01 00 00 00  03 00 00 00  03 00 00 00  58 00 00 00
```

`AAAA` explicitly exercises overlapping distance-1 copying. `ABCABCX`
exercises MatchThenLiteral rather than TerminalMatch.

### Hand-checkable LZ77 plus None frame vector

For a stream selecting LZ77 variant 1 and entropy None, with original size and
frame size both permitting the one-byte raw input `A`, the complete frame is 72
bytes. Its header declares one raw byte and one 16-byte dictionary/payload
token; entropy block and descriptor fields are zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 10 00 00 00  10 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 41 00 00 00
```

The 16-byte LZ77 parameter region belongs after the stream prefix and before
the first frame; it is not repeated inside this frame.

## LZSS variant 1

LZSS variant 1 is a frame-local byte dictionary transform with explicit
Literal and Match tokens. Its stream dictionary parameter region is exactly
16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | window size | bytes; default 65,536; nonzero |
| 4 | 4 | minimum match length | default 5; at least 5 |
| 8 | 4 | maximum match length | default 258; at least minimum |
| 12 | 4 | flags/reserved | zero |

All integers are little-endian. Window size must not exceed the local maximum
LZ distance. Maximum match length must not exceed the local maximum LZ match
length. Dictionary history starts empty and resets at every outer frame; no
reference crosses a frame boundary.

The encoder parses raw frame bytes from left to right. At each position, search
distances `1..min(window_size, bytes_already_parsed)`. A candidate compares
bytewise and may overlap: candidate byte `i` is the raw input byte at
`position - distance + i`, including bytes within the same match. Consider only
matches within the configured length range. Choose the longest match; on equal
length choose the smaller distance.

A Literal token costs exactly 2 serialized bytes and represents one raw byte.
A Match token costs exactly 9 serialized bytes and represents `length` raw
bytes. Emit the selected Match only when its serialized cost is strictly less
than the corresponding Literal sequence:

```text
9 < 2 * length
```

The minimum permitted match length of 5 makes this true for every encodable
Match. If no eligible and beneficial match exists, emit one Literal. Advance by
one byte after a Literal or by the complete match length after a Match. This
greedy rule and nearest-distance tie break make encoder output deterministic.

Tokens are concatenated without padding:

| Tag | Serialized size | Fields |
|---:|---:|---|
| 0 | 2 | tag, then one literal byte |
| 1 | 9 | tag, `uint32` distance, then `uint32` match length |

Distance and match length in tag 1 are little-endian. There are no implicit
native fields and no terminal-only token: a Match may end exactly at the frame
boundary. Tags other than 0 and 1 are malformed.

The decoder copies Match bytes one at a time from
`output_position - distance`, so overlap has defined repeating-copy semantics.
Before every token it validates distance against both configured window size
and produced history, length against configured and local limits, checked
output extent, and the declared raw frame size. It rejects truncated fields,
impossible references, output beyond the declared frame, premature token end,
bytes after the declared output size, and any token crossing an outer frame.

The dictionary serialized size in the generic frame header is the exact sum of
2 bytes per Literal and 9 bytes per Match. It is the byte input to the selected
entropy layer. With entropy None, compressed payload size equals dictionary
serialized size and the frame body contains these tokens directly. The
worst-case reference expansion is 2 serialized bytes per raw byte and must fit
local buffered and payload limits before allocation.

### Hand-checkable LZSS token vectors

With default parameters:

```text
Input `A`:
00 41

Input `AAAAAA`:
00 41
01 01 00 00 00 05 00 00 00

Input `ABCABCABC`:
00 41 00 42 00 43
01 03 00 00 00 06 00 00 00

Input `ABCABCABCX`:
00 41 00 42 00 43
01 03 00 00 00 06 00 00 00
00 58
```

`AAAAAA` exercises distance-1 overlap. `ABCABCABC` shows that Match naturally
ends a frame without a separate terminal form. `ABCABCABCX` shows that a Match
does not absorb the following Literal.

### Hand-checkable LZSS plus None frame vector

For a stream selecting LZSS variant 1 and entropy None, with one raw byte `A`,
the complete frame is 58 bytes. Its header declares one raw byte and one 2-byte
dictionary/payload token; entropy block and descriptor fields are zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  02 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 41
```

The 16-byte LZSS parameter region belongs after the stream prefix and before
the first frame; it is not repeated inside this frame.

## LZ78 variant 1

LZ78 variant 1 is a frame-local phrase dictionary transform. Its stream
dictionary parameter region is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | maximum phrase entries | default 65,536; nonzero |
| 4 | 4 | flags/reserved | zero |
| 8 | 8 | reserved | zero |

All integers are little-endian. The configured maximum counts non-root phrase
entries, must not exceed `UINT32_MAX`, and must not exceed the local maximum
dictionary-entry limit. Dictionary index 0 denotes the empty root phrase and
does not count toward this maximum. Non-root phrases receive consecutive
indices beginning at 1. Every serialized phrase index is a fixed little-endian
`uint32`; its width never grows within a frame.

The phrase dictionary starts with only the root and resets at every outer frame.
The encoder parses raw frame bytes from left to right. At each position it finds
the longest dictionary phrase `P` matching the remaining input. If at least one
input byte `C` follows `P`, it emits Pair `(index(P), C)`, inserts `P || C` at
the next consecutive index when capacity remains, and advances by
`length(P) + 1`. If the matched phrase consumes all remaining input, it emits
FinalIndex `(index(P))`, inserts nothing, and ends the frame. FinalIndex is
therefore never needed for the root. The dictionary freezes once it reaches the
configured maximum; subsequent Pair tokens remain valid but do not add entries.
No in-band clear token exists.

This parse is deterministic. Before the dictionary freezes, every phrase added
by the canonical encoder is new, so the longest match has one index. A frozen
dictionary is likewise searched by phrase value. Decoder acceptance does not
depend on rechecking encoder optimality.

Every dictionary token is exactly 8 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 1 | tag | 0 Pair, 1 FinalIndex |
| 1 | 1 | symbol | Pair byte; zero for FinalIndex |
| 2 | 2 | reserved | zero |
| 4 | 4 | phrase index | little-endian; rules below |

For Pair, phrase index is zero or names an existing non-root entry. The decoder
outputs that phrase followed by symbol, then inserts the same concatenation at
the next index if capacity remains. For FinalIndex, phrase index must name an
existing non-root entry, symbol must be zero, the expanded phrase must end
exactly at the declared raw frame size, and the token must be last. A Pair may
also end the frame and then must be last. Frame size, not an end token, is the
primary termination rule; empty frames contain no tokens.

The decoder stores each phrase as a bounded prefix index, trailing byte, and
checked expanded length. It validates an index before following it, validates
the expanded length against the remaining declared frame size and local output
limits, and expands without input-controlled recursion. Unknown tags, nonzero
reserved or unused fields, forward references, a FinalIndex for root, checked
length overflow, premature serialized end, output beyond the declared frame,
or bytes after raw completion are malformed.

The dictionary serialized size in the generic frame header is exactly
`8 * token_count`. It is the byte input to the selected entropy layer. With
entropy None, compressed payload size equals dictionary serialized size and the
frame body contains these tokens directly. A nonempty token expands to at least
one raw byte, so the input-independent serialized upper bound is 8 bytes per raw
frame byte. Token count, dictionary growth, phrase lengths, and this bound must
be checked before allocation.

### Hand-checkable LZ78 token vectors

With default parameters:

```text
Input `A`:
00 41 00 00 00 00 00 00

Input `AA`:
00 41 00 00 00 00 00 00
01 00 00 00 01 00 00 00

Input `ABA`:
00 41 00 00 00 00 00 00
00 42 00 00 00 00 00 00
01 00 00 00 01 00 00 00

Input `ABAB`:
00 41 00 00 00 00 00 00
00 42 00 00 00 00 00 00
00 42 00 00 01 00 00 00
```

`AA` and `ABA` exercise the otherwise ambiguous final existing phrase.
`ABAB` inserts phrase `AB` from Pair `(1, 'B')` and needs no FinalIndex.

### Hand-checkable LZ78 plus None frame vector

For a stream selecting LZ78 variant 1 and entropy None, with one raw byte `A`,
the complete frame is 64 bytes. Its header declares one raw byte and one 8-byte
dictionary/payload token; entropy block and descriptor fields are zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
00 41 00 00 00 00 00 00
```

The 16-byte LZ78 parameter region belongs after the stream prefix and before
the first frame; it is not repeated inside this frame.

## LZW variant 1

LZW variant 1 is a frame-local byte-string dictionary transform. Its stream
dictionary parameter region is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | maximum code width | default 16; range 9..24 bits |
| 4 | 4 | flags/reserved | zero |
| 8 | 8 | reserved | zero |

All integers are little-endian. The initial dictionary contains every
one-byte string: codes `0..255` name the byte of the same value. The first free
code is 256. There is no clear code and no end code. The dictionary resets to
the initial alphabet at every outer frame and freezes when the next free code
would equal `2^maximum_code_width`. A decoder also rejects a parameter whose
`2^maximum_code_width - 256` possible non-literal entries exceed its local
dictionary-entry limit.

The encoder parses a nonempty raw frame from left to right. It retains the
longest dictionary string `W` matching the current input. If a following byte
`K` makes `W || K` an existing dictionary string, that longer string becomes
`W`. Otherwise the encoder emits `code(W)`, inserts `W || K` at the next free
code when capacity remains, and continues with the one-byte string `K`. At the
end of the frame it emits the remaining `code(W)`. Empty input emits no codes.
The dictionary maps each byte string to exactly one code, so this parse is
deterministic.

Codes are packed as unsigned numeric fields through the repository LSB-first
BitWriter. Code width begins at 9. The encoder writes a code at the current
width, performs the insertion caused by the following failed extension, and
increments the width for future codes when the incremented next-free code is
exactly `2^current_width`. It never increments beyond the configured maximum.
This is the only width-change schedule for variant 1.

The decoder reads the first code at width 9; it must be a literal code below
256. Before reading each later code, it increments the width when the next-free
code is exactly `2^current_width - 1` and the current width is below the
configured maximum. This one-entry-earlier decoder test compensates for the
encoder having performed the pending insertion before emitting its next code.
After resolving the new code, the decoder inserts
`previous_string || first_byte(current_string)` when capacity remains.

A later code below the next-free code names its existing dictionary string. A
code equal to the next-free code is the `KwKwK` case and expands to
`previous_string || first_byte(previous_string)`; it is valid only while a new
entry can still be inserted. A code greater than the next-free code, or equal
to it after the dictionary has frozen, is malformed. Phrase expansion and
first-byte discovery use bounded prefix links and no input-controlled
recursion.

The declared raw frame size is the primary termination rule. Every decoded
string must fit completely in the remaining raw extent. Immediately after the
code that completes that extent, the dictionary byte region may contain only
zero padding to the next byte boundary. There are therefore between zero and
seven padding bits, all high bits of the final byte. Premature bits, an extra
complete or partial byte, nonzero padding, a first non-literal code, an invalid
forward code, checked phrase-length overflow, or output beyond the declared
frame size is malformed. Decoder acceptance does not depend on rechecking the
encoder's longest-match choice.

The generic frame header's dictionary serialized size is the exact number of
packed code bytes, including final zero padding. With entropy None, compressed
payload size equals dictionary serialized size and the frame body contains
those bytes directly. A nonempty raw byte contributes at most one code, so the
input-independent bound is
`ceil(raw_frame_size * maximum_code_width / 8)` bytes, with checked arithmetic.

### Hand-checkable LZW code vectors

With default parameters, every code in these short vectors is nine bits:

| Input | Decimal codes | Packed bytes |
|---|---|---|
| `A` | `65` | `41 00` |
| `AA` | `65, 65` | `41 82 00` |
| `AAA` | `65, 256` | `41 00 02` |
| `AB` | `65, 66` | `41 84 00` |
| `ABABABA` | `65, 66, 256, 258` | `41 84 00 14 08` |

`AAA` makes the second code equal to the decoder's next-free code and is the
smallest `KwKwK` vector. `ABABABA` exercises the same rule after an ordinary
dictionary reference. The high seven bits of the final `A` byte, high six bits
of the final `AA`, `AAA`, and `AB` bytes, and high four bits of the final
`ABABABA` byte are zero padding.

The 9-to-10-bit boundary validator vector contains 256 literal-zero codes at
9 bits, one literal-zero code at 10 bits, then `code 512` at 10 bits. The final
code is a valid `KwKwK` expansion, producing two bytes, so the decoded result is
259 zero bytes. Its 291-byte representation is 288 zero bytes followed by
`00 00 08`; bits 4..7 of the last byte are padding. This vector rejects a
decoder that changes width one code late because the `08` data bit then appears
in padding.

### Hand-checkable LZW plus None frame vector

For a stream selecting LZW variant 1 and entropy None, with one raw byte `A`,
the complete frame is 58 bytes. Its header declares one raw byte and two
dictionary/payload bytes; entropy block and descriptor fields are zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  02 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
41 00
```

The 16-byte LZW parameter region belongs after the stream prefix and before the
first frame; it is not repeated inside this frame.

## LZD variant 1

LZD means Lempel-Ziv Double. Variant 1 is a frame-local phrase grammar in which
each ordinary phrase is the concatenation of two longest dictionary matches.
Its stream dictionary parameter region is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | maximum phrase entries | default 65,536; range 1..`0xFFFFFEFF` |
| 4 | 4 | flags/reserved | zero |
| 8 | 8 | reserved | zero |

All integers are little-endian. The configured maximum counts generated phrase
entries, not the implicit byte alphabet, and must not exceed the local maximum
dictionary-entry limit. Phrase codes begin at 256. At most `0xFFFFFEFF`
phrases can be represented because `0xFFFFFFFF` is reserved as described
below. There is no in-band clear token. The dictionary resets at every outer
frame and freezes when it reaches the configured maximum.

The dictionary initially contains the 256 one-byte strings at reference values
`0..255`. Generated phrases receive consecutive reference values beginning at
256. At each raw position, the encoder chooses the longest string in the
current byte-or-phrase dictionary that matches the remaining input; this is
the left component. If input remains, it independently chooses the longest
dictionary string at the new position as the right component, emits both
references, and inserts their concatenation at the next phrase reference when
the dictionary is not frozen. It advances by the sum of both component
lengths. Existing dictionary strings are unique, so no equal-length tie exists.

If the left component consumes the final input suffix, the encoder emits it
with an absent right component, inserts nothing, and ends the frame. This is
marc's binary-input replacement for the theoretical unique end symbol: no byte
value is reserved and no sentinel becomes part of the decoded data. Empty
frames contain no tokens. A right-present token may also end the frame and is
inserted normally when capacity remains. Dictionary freeze changes only future
insertion; both longest searches continue over the fixed dictionary.

Every dictionary token is exactly 8 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | left reference | little-endian byte or prior-phrase reference |
| 4 | 4 | right reference | same, or `0xFFFFFFFF` only for terminal absence |

Reference values `0..255` denote the corresponding literal byte.
`256..0xFFFFFFFE` denote phrase number `reference - 256` and must name an
entry inserted by an earlier token in the same frame. `0xFFFFFFFF` is invalid
as a left reference. It is valid as a right reference only on the final token,
when expanding the left reference alone ends exactly at the declared raw frame
size. A right-present token expands left followed by right. Its combined
checked length must fit the remaining raw extent; if it reaches that extent,
the token must be last.

The decoder records each inserted phrase as two backward references plus a
checked expanded length. References are validated before insertion, so the
phrase graph is acyclic. Expansion uses a bounded explicit work stack, never
input-controlled recursion. The decoder rejects a non-multiple-of-eight token
region, an unknown or forward phrase reference, absent left, nonterminal absent
right, checked length or reference overflow, output beyond the declared frame,
premature serialized end, bytes after raw completion, or workspace and local
limit violations. Decoder acceptance does not depend on reproducing the
encoder's longest-match decisions.

The generic frame header's dictionary serialized size is exactly
`8 * token_count`. It is the byte input to the selected entropy layer. With
entropy None, compressed payload size equals dictionary serialized size and the
frame body contains these tokens directly. Every right-present token consumes
at least two raw bytes and an optional final absent-right token consumes at
least one, so the input-independent serialized bound is
`8 * ceil(raw_frame_size / 2)` bytes, with checked arithmetic.

### Hand-checkable LZD token vectors

With default parameters:

```text
Input `A`:
41 00 00 00 FF FF FF FF

Input `AB`:
41 00 00 00 42 00 00 00

Input `ABA`:
41 00 00 00 42 00 00 00
41 00 00 00 FF FF FF FF

Input `ABAB`:
41 00 00 00 42 00 00 00
00 01 00 00 FF FF FF FF

Input `ABABAB`:
41 00 00 00 42 00 00 00
00 01 00 00 00 01 00 00
```

The first token of `AB` inserts phrase 0, string `AB`, at reference 256.
`ABAB` then uses that phrase as its terminal left component. `ABABAB` uses the
same existing phrase for both components and inserts `ABAB` at reference 257.

For comparison with the published factorization example but without its
theoretical sentinel, `abbaababaaba` parses as `ab | ba | abab | aab | a` and
serializes as `(a,b), (b,a), (256,256), (a,256), (a,absent)`.

### Hand-checkable LZD plus None frame vector

For a stream selecting LZD variant 1 and entropy None, with one raw byte `A`,
the complete frame is 64 bytes. Its header declares one raw byte and one 8-byte
dictionary/payload token; entropy block and descriptor fields are zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
41 00 00 00 FF FF FF FF
```

The 16-byte LZD parameter region belongs after the stream prefix and before the
first frame; it is not repeated inside this frame.

### Hand-checkable LZD plus None stream vector

The complete known-size stream consists of the 64-byte stream prefix, one
16-byte LZD parameter region, and then zero or more complete frames. Empty input
has exactly the 80-byte prefix and no frame. Nonempty input is partitioned at
the declared raw frame size; sequence numbers begin at zero, and the LZD phrase
dictionary resets for every frame.

For raw input `ABAB`, frame size 2, default LZD parameters, and entropy None,
the complete stream is 208 bytes. Offsets 80 and 144 begin its two 64-byte
frames. Both payloads are the independently reset token `(A,B)`:

```text
4D 41 52 43 01 00 00 00  40 00 00 00 05 00 01 00
00 00 00 00 02 00 00 00  00 00 00 00 10 00 00 00
00 00 00 00 00 00 00 00  04 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 01 00 00 00 00 00  00 00 00 00 00 00 00 00
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
02 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  41 00 00 00 42 00 00 00
4D 52 46 31 38 00 00 00  01 00 00 00 00 00 00 00
02 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  41 00 00 00 42 00 00 00
```

## Foundational hand-checkable vectors

These vectors define primitives used by every later format variant.

| Operation | Logical value or bits | Serialized bytes |
|---|---:|---:|
| store little-endian 16 | `0x1234` | `34 12` |
| store little-endian 32 | `0x12345678` | `78 56 34 12` |
| store little-endian 64 | `0x0123456789ABCDEF` | `EF CD AB 89 67 45 23 01` |
| write bits | `1,0,1,1,0,0,1,0` | `4D` |
| write 3 bits then finish | `1,0,1` | `05` |

For the final vector, bits 3 through 7 are padding and must be zero. Strict
alignment rejects, for example, byte `FD` after consuming its low three bits.

## Limits versus format fields

Decoder limits are local policy and are not serialized. Stream and future frame
headers declare the sizes required to validate one frame. Header
parsing must not allocate based on those declarations until all applicable
policy checks succeed.

The baseline implementation defaults are:

| Limit | Default |
|---|---:|
| total decoded output | 1 TiB |
| one uncompressed frame | 16 MiB |
| one entropy block | 1 MiB |
| one compressed frame payload | 64 MiB |
| dictionary serialized bytes per frame | 64 MiB |
| dictionary entries | 16,777,216 |
| LZ distance | 16 MiB |
| LZ match length | 1 MiB |
| Huffman code length | 24 bits |
| entropy table entries | 1,048,576 |
| range-model total | 16,777,216 |
| simultaneously buffered bytes | 128 MiB |
| entropy blocks per frame | 65,536 |
| expansion ratio | 1024:1 plus 1 MiB slack |

These values bound what the implementation accepts; they do not select codec
parameters. For example, the later Blocked Huffman format may specify a maximum
code length lower than the policy ceiling.

## LZMW variant 1

LZMW variant 1 is a frame-local byte dictionary transform. Its stream
dictionary parameter region is exactly 16 bytes:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | maximum generated entries | default 65,536; nonzero |
| 4 | 4 | flags | zero |
| 8 | 8 | reserved | zero |

References `0..255` denote the corresponding one-byte alphabet symbol.
Generated references begin at 256 and are assigned consecutively. Values that
do not denote a byte or an already generated entry are malformed. The maximum
entry count must not exhaust the 32-bit reference namespace or exceed the local
dictionary-entry limit.

At the start of every outer frame the generated dictionary and previous phrase
are empty. Parse raw bytes greedily from left to right. The first phrase is
necessarily one byte. Each later phrase is the longest remaining-input prefix
equal to either one byte or a generated entry that existed before that phrase
was selected. On equal lengths, select the smaller numeric reference. Emit its
reference and advance by the phrase length.

After emitting a phrase other than the first, if the generated dictionary is
not full, append exactly one entry equal to the concatenation of the previous
phrase and current phrase. The entry receives reference
`256 + generated_entry_index`. Each adjacent pair consumes one entry even if
its expanded bytes equal an earlier entry; the smaller-reference tie rule makes
such duplicates harmless and keeps encoder and decoder numbering independent
of string-equality searches. Once `maximum generated entries` is reached, the
dictionary freezes for the rest of the frame. It is not cleared, replaced, or
updated, and parsing continues against the frozen entries. This bounded freeze
rule is marc-specific and does not claim byte compatibility with the original
LRU-replacement proposal.

Every token is exactly four bytes: one unsigned little-endian reference. There
is no end code. The generic frame's declared uncompressed size terminates
decoding, and the token region must end at exactly the same point. Empty input
has no tokens. A nonempty frame must contain at least one token. Premature end,
tokens after exact output completion, forward or unavailable references,
checked length overflow, or expansion beyond the declared frame size are
malformed.

The decoder reconstructs each generated entry as two already valid references
plus their checked combined length. Later raw expansion must be iterative and
bounded; input-controlled recursion is forbidden. The dictionary serialized
size is `4 * token_count`, at most four bytes per raw input byte. With entropy
None, compressed payload size equals dictionary serialized size and the frame
body contains these token references directly.

### Hand-checkable LZMW token vectors

The empty input produces no token bytes. `A` emits reference 65:

```text
41 00 00 00
```

`ABAB` parses as `A | B | AB`. After `B`, entry 256 is `AB`, so the token bytes
are:

```text
41 00 00 00  42 00 00 00  00 01 00 00
```

For `abbaababaaba`, the formal LZMW parsing without an external delimiter is
`a | b | b | a | ab | ab | aab | a`. Generated entry 256 is `ab`, and entry
259 is `aab`. The 32 token bytes are:

```text
61 00 00 00  62 00 00 00  62 00 00 00  61 00 00 00
00 01 00 00  00 01 00 00  03 01 00 00  61 00 00 00
```

With maximum entries 1, `ABABAB` parses as `A | B | AB | AB`. Entry 256 is
created after the second phrase and the dictionary then freezes:

```text
41 00 00 00  42 00 00 00  00 01 00 00  00 01 00 00
```

### Hand-checkable LZMW plus None frame vector

For a stream selecting LZMW variant 1 and entropy None, with one raw byte `A`,
the complete frame is 60 bytes. Its header declares one raw byte and one
four-byte dictionary/payload reference; entropy block and descriptor fields are
zero:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 04 00 00 00  04 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
41 00 00 00
```

The 16-byte LZMW parameter region belongs after the stream prefix and before
the first frame; it is not repeated inside this frame. Every frame resets the
LZMW phrase dictionary.

### Hand-checkable LZMW plus None stream vector

The complete known-size stream consists of the 64-byte stream prefix, one
16-byte LZMW parameter region, and zero or more complete frames. Empty input is
exactly the 80-byte prefix. For raw `ABAB`, frame size 2, default parameters,
and entropy None, the stream is 208 bytes. Offsets 80 and 144 begin independent
64-byte frames, and both reset to the literal references `A, B`:

```text
4D 41 52 43 01 00 00 00  40 00 00 00 06 00 01 00
00 00 00 00 02 00 00 00  00 00 00 00 10 00 00 00
00 00 00 00 00 00 00 00  04 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 01 00 00 00 00 00  00 00 00 00 00 00 00 00
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
02 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  41 00 00 00 42 00 00 00
4D 52 46 31 38 00 00 00  01 00 00 00 00 00 00 00
02 00 00 00 08 00 00 00  08 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  41 00 00 00 42 00 00 00
```

## Blocked Huffman variant 1

Blocked Huffman consumes the dictionary-serialized byte stream in consecutive
blocks of the stream header's entropy block size. No block crosses a frame.
The block count is exactly
`ceil(dictionary_serialized_size / entropy_block_size)`. Every block except the
last has the configured size; the last contains the remaining bytes. A
nonempty frame cannot contain an empty entropy block.

The entropy-parameter region is empty for variant 1. Each block contributes one
16-byte descriptor to the frame descriptor region. Any Huffman model follows
its descriptor immediately. Descriptors and models occur in block order; the
payload region then contains each corresponding payload in the same order.

### Block descriptor

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | symbol count | input bytes in this block |
| 4 | 4 | payload size | stored payload bytes |
| 8 | 2 | model size | raw: 0; Huffman: 256 |
| 10 | 1 | flags | bit 0 raw; all other bits zero |
| 11 | 1 | final valid bits | 1 through 8; raw requires 8 |
| 12 | 4 | reserved | zero |

A Huffman model is 256 bytes in symbol order `0..255`; each byte is that
symbol's code length. Length zero means absent and lengths 1 through 15 are
valid. Multi-symbol models must describe a complete, non-oversubscribed prefix
code. A one-symbol model must use length 1. Canonical codes are assigned by
increasing length and then increasing symbol value. Encoder codes reverse those
bits within their lengths for physical LSB-first output.

The decoder derives the exact payload bit count from decoded symbols and must
consume precisely the declared payload. For a final partial byte, unused high
bits are zero. `final valid bits` is 8 when the final byte is completely used.
Payload size is nonzero for every nonempty block.

Raw representation stores the input bytes unchanged and has no model. Huffman
representation is selected only when
`256 + ceil(payload_bits / 8) < symbol_count`; ties select raw. This choice is
mandatory, not an encoder heuristic.

### Hand-checkable raw block

Four bytes `41 41 41 41` select raw representation because the Huffman model
overhead exceeds the input size. The descriptor and payload are:

```text
04 00 00 00 04 00 00 00 00 00 01 08 00 00 00 00
41 41 41 41
```

The corresponding internal one-symbol Huffman model has length 1 for symbol
`41`, zero for every other symbol, canonical and reversed code zero, payload
byte `00`, and four valid bits. It remains a primitive test vector even though
the mandatory stored-size rule selects raw for this block.

## LZ77 variant 1 plus Blocked Huffman variant 1

This combined profile uses dictionary algorithm ID 1, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. The stream parameter regions are
the 16-byte LZ77 parameters followed by the empty Blocked Huffman parameter
region. `entropy block size` counts bytes in the canonical LZ77 token stream;
the default is 65,536. Blocks reset at and cannot cross an outer frame.

For every frame, the LZ77 encoder first determines the complete canonical token
stream. The generic frame header records raw bytes as `uncompressed size`, token
bytes as `dictionary serialized size`, stored entropy bytes as `compressed
payload size`, the exact Blocked Huffman block count, and the complete
descriptor/model region size. The body is:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate dictionary-token region is stored. The entropy decoder must produce
exactly `dictionary serialized size` bytes. The LZ77 validator then consumes
that complete staged region and must derive exactly `uncompressed size` raw
bytes before raw publication begins.

### Hand-checkable combined raw-block frame

For raw input `A`, LZ77 emits the documented 16-byte Literal token. With an
entropy block size at least 16, Blocked Huffman selects raw representation. The
complete 88-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 10 00 00 00  10 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
10 00 00 00 10 00 00 00  00 00 01 08 00 00 00 00
00 00 00 00 00 00 00 00  00 00 00 00 41 00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final 16 bytes are the unchanged LZ77 token.
The 16-byte LZ77 parameter region remains stream-level and is not repeated in
this frame.

## LZSS variant 1 plus Blocked Huffman variant 1

This composition uses dictionary algorithm ID 2, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. Its stream parameter regions
are the 16-byte LZSS parameters followed by the empty Blocked Huffman parameter
region. `entropy block size` counts bytes in the canonical variable-length
LZSS token stream. Blocks reset at and cannot cross an outer frame.

The generic frame header records raw bytes as `uncompressed size`, LZSS token
bytes as `dictionary serialized size`, stored entropy bytes as `compressed
payload size`, the exact Blocked Huffman block count, and the complete
descriptor/model region size. The body uses the same ordering as the first
composition:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate LZSS token region is stored. Entropy decoding must produce exactly
`dictionary serialized size` bytes. Before any raw-byte publication, the LZSS
validator must consume the complete staged token region and derive exactly
`uncompressed size` bytes. This rule is significant because LZSS Literal
tokens occupy two bytes while Match tokens occupy nine bytes.

### Hand-checkable LZSS combined raw-block frame

For raw input `A`, LZSS emits the two-byte Literal token `00 41`. With entropy
block size two, Blocked Huffman selects raw representation. The complete
74-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  02 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 02 00 00 00  00 00 01 08 00 00 00 00
00 41
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final two bytes are the unchanged LZSS
Literal token. The 16-byte LZSS parameter region is stream-level and is not
repeated in this frame.

The reference frame encoder plans the complete LZSS token stream into
caller-owned staging before writing the frame. It then derives the exact
Blocked Huffman descriptor and payload extents from those bytes, validates the
generic header, and checks the complete serialized destination before emitting
any header or entropy byte. Repeating the plan during encoding must reproduce
the same token and entropy representation byte for byte.

The reference frame decoder treats entropy output as uncommitted staging. It
validates the complete LZSS token region and its exact derived raw extent before
checking raw destination capacity and beginning LZSS reconstruction. A failure
in the generic header, Blocked Huffman metadata or payload, LZSS token grammar,
declared size, or raw capacity must publish no raw byte.

The known-size complete stream uses the ordinary 64-byte version 1.0 stream
header, followed by the 16-byte LZSS parameter region and zero or more combined
frames in sequence. Empty input is exactly this 80-byte prefix. Nonempty input
is split by the declared uncompressed frame size; both LZSS dictionary state
and every Blocked Huffman model reset at each frame. Strict decoding requires
the frames to derive exactly `original size` bytes and rejects any remaining
serialized byte.

Incremental encoding does not define another representation. Input and output
chunking, temporary starvation, and nonterminal `Flush` leave these exact bytes
unchanged. A full uncompressed frame may be emitted before whole-stream
`EndInput`; a final short frame is emitted only after the known-size input
contract is satisfied. `ResetBlock` is unsupported at this profile boundary.

Incremental decoding likewise does not alter the representation. A frame is
not exposed until its complete serialized body, entropy metadata and payload,
LZSS token stream, and declared raw extent validate. Earlier validated frames
may already have been committed when a later frame fails. `EndInput` received
while raw staging is draining remains effective after the drain and makes a
missing subsequent frame a truncation error.

The internal profile factory only normalizes this already specified stream
configuration and calculates caller workspace. It introduces no additional
field, algorithm ID, variant, padding rule, or alternative byte
representation.

The dedicated C ABI factory constructs this same representation. Its
configuration and workspace structures are process-local ABI data and are not
serialized into the stream.

The CLI name `lzss-blocked-huffman` selects this exact representation with
one-MiB raw frames and 65,536-symbol entropy blocks. The name and fixed local
policy do not add format fields.

## LZ78 variant 1 plus Blocked Huffman variant 1

This composition uses dictionary algorithm ID 3, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. Its stream parameter regions
are the 16-byte LZ78 parameters followed by the empty Blocked Huffman parameter
region. `entropy block size` counts bytes in the canonical fixed-width LZ78
token stream. Blocks reset at and cannot cross an outer frame.

The generic frame header records raw bytes as `uncompressed size`, LZ78 token
bytes as `dictionary serialized size`, stored entropy bytes as `compressed
payload size`, the exact Blocked Huffman block count, and the complete
descriptor/model region size. The body is:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate LZ78 token region is stored. Entropy decoding must produce exactly
`dictionary serialized size` bytes, and that size must be a multiple of eight.
Before any raw-byte publication, the LZ78 validator must consume the complete
staged token region, validate all phrase references and dictionary growth, and
derive exactly `uncompressed size` bytes. Phrase expansion remains iterative;
the entropy layer does not change LZ78's frame-local dictionary rules.

For a raw frame of `F` bytes, the token count is at most `F`, so the canonical
dictionary staging bound is `8F` bytes. The maximum phrase-entry count for
encoding or validation is the lesser of the token count and the configured
LZ78 maximum. For entropy block size `E`, the block-count bound is
`ceil(8F/E)`. Every multiplication, ceiling division, descriptor extent,
aligned phrase-table extent, and aggregate workspace sum must be checked before
allocation or output.

### Hand-checkable LZ78 combined raw-block frame

For raw input `A`, LZ78 emits the eight-byte Pair token
`00 41 00 00 00 00 00 00`. With entropy block size eight, Blocked Huffman
selects raw representation. The complete 80-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  08 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 08 00 00 00  00 00 01 08 00 00 00 00
00 41 00 00 00 00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final eight bytes are the unchanged LZ78
Pair token. The 16-byte LZ78 parameter region remains stream-level and is not
repeated in this frame.

The reference frame encoder must first plan the complete LZ78 parse using an
aligned caller-owned phrase table, emit the canonical tokens once into staging,
and then plan Blocked Huffman over those exact bytes. Only after all extents and
the generic header validate may it publish serialized output. Repeating the
plan during encoding must reproduce both token and entropy bytes exactly.

The reference frame decoder treats entropy output and phrase records as
uncommitted state. It must validate the complete token stream into an aligned
caller-owned phrase table before checking raw destination capacity and running
the transactional LZ78 decoder. A header, entropy, token, phrase-reference,
declared-size, workspace, or raw-capacity failure publishes no raw byte.

The known-size stream uses the ordinary 64-byte version 1.0 header, followed by
the 16-byte LZ78 parameter region and zero or more combined frames. Empty input
is exactly this 80-byte prefix. Both the LZ78 phrase dictionary and every
Blocked Huffman model reset at each frame. Strict decoding requires the frames
to derive exactly `original size` bytes and rejects trailing serialized data.

Incremental encoding and decoding must emit and accept exactly this
representation under arbitrary input and output chunking. A final short frame
is emitted only after the known-size input contract is satisfied. A decoded
frame is not exposed until its entropy payload, token stream, phrase table, and
raw extent validate completely; earlier frames may already be committed when a
later frame fails. Nonterminal `Flush` does not shorten a frame, and
`ResetBlock` is unsupported at this profile boundary.

The reserved public name for this exact representation is
`lz78-blocked-huffman`. Profile sizing retains the three caller-workspace shape
while treating the aligned views region as opaque storage. Encoding uses one
LZ78 encoder-entry array. Decoding places Blocked Huffman block views first,
aligns the next offset for LZ78 phrase entries, and places the phrase array
there. The checked partition helper must accept this exact derived layout
before exposing either typed span. Reserving the name and format here does not
publish a factory, CLI selector, or compatibility promise until the normal
completion criteria pass.

## Adaptive Huffman FGK variant 1

Adaptive Huffman variant 1 accepts byte symbols `0..255`, has no entropy
parameter region, and requires stream entropy block size zero. Every nonempty
frame is one independently coded entropy block and starts from the initial NYT
tree. The variant's format-level maximum uncompressed frame size is 2^24 bytes,
even if a decoder's local frame limit is larger.

The initial tree is one NYT leaf of weight 0 and number 512. An internal node's
left edge is bit 0 and right edge is bit 1. Path bits are emitted root to leaf.
Because physical packing is LSB-first, the first path bit occupies the next
available low-order bit; the path is not numerically reversed as a unit.

To encode a symbol already present, emit its current root-to-leaf path. To
encode a new symbol, emit the current NYT path followed by the symbol's numeric
8-bit value least-significant bit first. The decoder rejects a literal following
NYT if that symbol is already present.

### Tree insertion and update

Splitting NYT number `n` replaces it with an internal node retaining number
`n`. Its left child is the new NYT with number `n-2` and weight 0. Its right
child is the new symbol with number `n-1` and weight 1. The internal node starts
at weight 1. At most 513 nodes exist.

For an existing symbol, the update cursor begins at its leaf. For a new symbol,
it begins at the former parent of the replaced NYT internal node; if the split
node was the root, updating is complete. At each cursor node:

1. Find the highest-numbered equal-weight node that is not the cursor, its
   parent, an ancestor, or a descendant. If one exists, exchange the two nodes'
   parent/child positions and node numbers. Roots are never exchanged.
2. Increment the cursor weight by one.
3. Continue with the cursor's parent after any exchange.

Ties therefore have one exact outcome. Encoder and decoder perform insertion
and update only after the complete symbol has been emitted or decoded.

Every frame boundary resets all nodes, weights, and symbol mappings to the
single NYT root. Weights are unsigned 32-bit integers. Since at most 2^24
symbols occur before the mandatory reset, overflow is impossible. This full
frame reset is variant 1's frequency-rescaling rule; there is no mid-frame
halving. A different reset or rescale policy requires another variant ID.

### Adaptive descriptor

Exactly one 16-byte descriptor precedes each frame payload:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | symbol count | equals dictionary serialized size |
| 4 | 4 | payload size | equals frame compressed payload size |
| 8 | 1 | final valid bits | 1 through 8 |
| 9 | 1 | flags | zero |
| 10 | 6 | reserved | zero |

Both sizes and the payload are nonzero. After exactly `symbol count` symbols,
the decoder must have consumed precisely the declared valid payload bits.
Unused high bits of the final byte are zero. Truncation, a duplicate NYT
literal, an invalid tree relationship, excess valid bits, trailing payload
bytes, or nonzero padding is malformed.

### Hand-checkable payload vectors

These vectors begin from a fresh frame. ASCII `A` is `0x41`, and `B` is
`0x42`.

| Input | Logical emission | Payload | Final valid bits |
|---|---|---|---:|
| `A` | empty NYT path, literal `41` | `41` | 8 |
| `AA` | literal `41`, existing path `1` | `41 01` | 1 |
| `AB` | literal `41`, NYT path `0`, literal `42` | `41 84 00` | 1 |
| `ABA` | preceding `AB`, existing `A` path `1` | `41 84 02` | 2 |

After `A`, node 512 is the weight-1 root with left NYT 510 weight 0 and
right `A` 511 weight 1. After `AB`, root 512 has weight 2, left internal 510
weight 1, right `A` 511 weight 1; node 510 has left NYT 508 weight 0 and right
`B` 509 weight 1. Updating the final `A` selects no other leader, leaving the
same shape with weights root 3 and `A` 2.

The descriptor for the one-symbol `A` vector is:

```text
01 00 00 00 01 00 00 00 08 00 00 00 00 00 00 00
```

The descriptor for `ABA` declares symbol count 3, payload size 3, and two
valid bits:

```text
03 00 00 00 03 00 00 00 02 00 00 00 00 00 00 00
```

For a one-frame stream whose frame size and original size are both 3, the
complete encoded `ABA` frame is 75 bytes:

```text
4D 52 46 31 38 00 00 00 00 00 00 00 00 00 00 00
03 00 00 00 03 00 00 00 03 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
03 00 00 00 03 00 00 00 02 00 00 00 00 00 00 00
41 84 02
```

The first 56 bytes are the frame header, the next 16 are the Adaptive
descriptor, and the final 3 are the payload. Sequence is zero, entropy block
count is one, and descriptor byte count is 16.

## Dynamic Range Coder variant 1

Dynamic Range Coder variant 1 is a byte-oriented integer range coder with an
adaptive order-0 model over symbols `0..255`. It has no entropy parameter region
and requires stream entropy block size zero. Each nonempty outer frame is one
independent entropy block and resets the complete coder and model. The format-
level maximum uncompressed frame size is 2^24 bytes.

The model starts with frequency 1 for every symbol and total 256. Cumulative
frequency is the sum for all numerically smaller symbols. After coding a symbol,
increment its frequency and the total. When the total becomes 32768, replace
every frequency `f` with `(f + 1) / 2`, using integer division, then recompute
the total. Thus every symbol remains active. Encoder and decoder update only
after completing the same symbol.

### Interval update and byte normalization

Coder state is unsigned `low` (64 bits) and `range` (32 bits), initialized to 0
and `0xFFFFFFFF`. For cumulative frequency `c`, symbol frequency `f`, and model
total `t`, encode one symbol in this exact order:

```text
range = range / t
low   = low + c * range
range = range * f
while range < 0x01000000:
    range = range << 8
    shift_low()
```

The pre-division range is always at least `0x01000000`, and `t <= 32768`, so
the unit range is nonzero. Products fit their declared types. `shift_low()`
performs delayed carry in base 256. State includes an 8-bit `cache`, initialized
to 0, and a positive `pending` count, initialized to 1:

```text
lo32  = low & 0xFFFFFFFF
carry = low >> 32
if lo32 < 0xFF000000 or carry != 0:
    emit (cache + carry) & 0xFF
    emit pending - 1 bytes of (0xFF + carry) & 0xFF
    cache = lo32 >> 24
    pending = 0
pending = pending + 1
low = uint32(lo32 << 8), then widen that value to 64 bits
```

The stated invariants constrain `carry` to 0 or 1; emitted additions are reduced
to their low 8 bits as shown. After all symbols, call `shift_low()` exactly five
times. There is no end symbol. The payload is byte aligned and contains the
normalization bytes plus the five-byte termination sequence.

Decoding initializes `range` to `0xFFFFFFFF` and reads exactly five payload
bytes into a 32-bit `code`, in stored order, using
`code = (code << 8) | byte`. The first of these five bytes must be zero; this
rejects otherwise equivalent noncanonical payloads. For each declared symbol:

```text
unit   = range / total
scaled = code / unit
find the unique symbol with cumulative <= scaled
    and scaled < cumulative + frequency
code   = code - cumulative * unit
range  = unit * frequency
while range < 0x01000000:
    range = range << 8
    code = (code << 8) | next_payload_byte
```

Reject `scaled >= total`, a missing normalization byte, arithmetic invariant
failure, or any payload bytes left after the declared symbol count. Model update
then follows the encoder rule. All shifts and wraparound of `code` are unsigned
32-bit operations.

### Range descriptor and vectors

Exactly one 16-byte descriptor precedes each frame payload:

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | symbol count | equals dictionary serialized size |
| 4 | 4 | payload size | equals frame compressed payload size; at least 5 |
| 8 | 1 | flags | zero |
| 9 | 7 | reserved | zero |

Fresh-frame payload vectors are:

| Input | Payload |
|---|---|
| `A` | `00 40 FF FF BF 00` |
| `AA` | `00 41 40 BE FF 7E` |
| `AB` | `00 41 42 BD 01 7A 00` |
| `ABA` | `00 41 42 FD 40 3C F0` |

For `A`, the descriptor is:

```text
01 00 00 00 06 00 00 00 00 00 00 00 00 00 00 00
```

For a one-frame stream whose frame size and original size are both 3, the
complete encoded `ABA` frame is 79 bytes:

```text
4D 52 46 31 38 00 00 00 00 00 00 00 00 00 00 00
03 00 00 00 03 00 00 00 07 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
03 00 00 00 07 00 00 00 00 00 00 00 00 00 00 00
00 41 42 FD 40 3C F0
```

The first 56 bytes are the generic frame header, the next 16 are the range
descriptor, and the final 7 are the payload.

## rANS variant 1

rANS variant 1 is scalar, block buffered, and byte renormalized. The alphabet is
`0..255`, `table_log` is exactly 12, normalized total `M` is 4096, internal state
is unsigned 64-bit, and lower normalization bound `L` is 2^31. Stream entropy
block size is nonzero and defaults to 65,536 input symbols. Every block rebuilds
its static model independently; the final short block is valid.

### Deterministic frequency normalization

Count one finite block. Absent symbols receive normalized frequency zero. For a
nonempty block of size `N`, initialize each present symbol `s` to
`max(1, floor(count[s] * M / N))`. Let its signed normalization error be:

```text
error[s] = count[s] * M - normalized[s] * N
```

While the normalized sum is below `M`, increment the present symbol with largest
current error, breaking ties toward the lower numeric symbol. Recompute that
symbol's error after every increment. While the sum is above `M`, decrement a
symbol whose normalized frequency exceeds one and whose current error is
smallest, breaking ties toward the higher numeric symbol. Recompute after every
decrement. The final frequencies are positive exactly for present symbols and
sum to 4096. A one-symbol block therefore assigns that symbol 4096.

Cumulative frequency is the sum of normalized frequencies for numerically
smaller symbols. All normalization arithmetic uses exact integers.

### State update and byte layout

Initialize encoder state `x = L` and process block symbols in reverse logical
order. For a symbol with frequency `f` and cumulative `c`:

```text
x_max = ((L >> table_log) << 8) * f
while x >= x_max:
    prepend byte(x & 0xFF) to the renormalization region
    x = x >> 8
x = floor(x / f) * M + (x mod f) + c
```

The prepend rule applies to the whole region, including multiple bytes from one
symbol. The block payload is final `x` as an explicit little-endian uint64,
followed by that completed renormalization region. Payload size is at least 8.
The final state must satisfy `L <= x < L * 256`.

Decoding reads the final state, then produces symbols in forward order:

```text
slot = x & (M - 1)
find the unique symbol with c <= slot < c + f
x = f * (x >> table_log) + slot - c
while x < L:
    x = (x << 8) | next_renormalization_byte
```

Reject a state outside `[L, L*256)` at initialization or a symbol boundary, an
unmapped slot, arithmetic overflow, missing or trailing renormalization bytes,
or a final state other than exactly `L`. A state below `L` immediately after the
inverse update is valid only while the specified renormalization loop restores
it before the next boundary. Exact terminal state and byte consumption make the
payload canonical. Symbols appear in forward order even though encoding
traverses them in reverse.

### rANS descriptor and frame layout

Each block has one 528-byte descriptor. All frame descriptors occur first in
logical block order, followed by all block payloads in the same order.

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | symbol count | configured block size or final remainder |
| 4 | 4 | payload size | this block's exact payload bytes; at least 8 |
| 8 | 1 | table log | exactly 12 |
| 9 | 1 | flags | zero |
| 10 | 6 | reserved | zero |
| 16 | 512 | normalized frequencies | 256 little-endian uint16 values |

Descriptor frequencies must sum to 4096 and at least one must be nonzero; exact
source counts and therefore the number of source-present symbols are not
serialized. A one-symbol model has one frequency 4096. The decoder builds and
validates a bounded 4096-entry slot table before payload traversal.

Frame entropy block count is exactly
`ceil(dictionary_serialized_size / stream_entropy_block_size)`. Descriptor size
is exactly block count times 528. Frame compressed payload size is the checked
sum of descriptor-declared payload sizes. No rANS block crosses an outer frame.

### Hand-checkable rANS vectors

Fresh-block normalized models and payloads are:

| Input | Nonzero normalized frequencies | Payload |
|---|---|---|
| `A` | `A:4096` | `00 00 00 80 00 00 00 00` |
| `AA` | `A:4096` | `00 00 00 80 00 00 00 00` |
| `AB` | `A:2048, B:2048` | `00 10 00 00 02 00 00 00` |
| `ABA` | `A:2731, B:1365` | `80 10 00 60 03 00 00 00` |

For `A`, descriptor bytes 0 through 15 are:

```text
01 00 00 00 08 00 00 00 0C 00 00 00 00 00 00 00
```

All frequency entries are zero except symbol `41` at descriptor offset 146,
whose little-endian uint16 value is `00 10`.

With frame size 3 and entropy block size 2, input `ABA` produces two descriptors
and two 8-byte payloads. The serialized frame size is
`56 + 2*528 + 16 = 1128` bytes. Its first payload is the `AB` vector and its
second payload is the one-symbol `A` vector above.

## tANS variant 1

tANS variant 1 is block buffered and table based. The alphabet is `0..255`,
`table_log` is exactly 12, table size `L` is 4096, and live states occupy
`[L,2L)`. Stream entropy block size is nonzero and defaults to 65,536 byte
symbols. Every block rebuilds and validates its model and tables independently.

Normalize frequencies exactly as specified for rANS variant 1. Construct a
4096-entry spread table as follows:

```text
position = 0
step = 2563
for symbol = 0..255:
    repeat normalized_frequency[symbol] times:
        spread[position] = symbol
        position = (position + step) & 4095
```

After filling, `position` must return to zero and every slot must have been
written exactly once. Scan spread positions `j=0..4095` in numeric order. For
the symbol `s=spread[j]`, assign the next consecutive reduced state
`q` from `[frequency[s], 2*frequency[s])`. The decode entry for live state
`L+j` is:

```text
symbol = s
bit_count = table_log - floor(log2(q))
state_base = q << bit_count
```

`state_base` and `state_base + (1<<bit_count) - 1` must both lie in `[L,2L)`.
The inverse encode lookup maps each pair `(s,q)` back to `L+j`.

Initialize encoder state `x=L` and traverse source symbols in reverse. For the
next symbol `s`, find the unique `k` for which `q=x>>k` is in
`[frequency[s],2*frequency[s])` and that q's decode `bit_count` equals `k`.
Logically prepend the `k` low bits of `x`, least-significant bit first, then set
`x=encode_lookup[s,q]`. The completed block bit sequence is therefore already
in decoder consumption order even though source traversal was reversed.

The payload begins with little-endian uint16 `x-L`, followed by that bit
sequence packed by the repository LSB-first rule. Decoding starts at
`x=L+state_offset`; for every declared symbol it selects the decode entry,
reads `bit_count` bits as an LSB-first numeric value, and sets
`x=state_base+value`. Reject an offset at least L, an invalid table entry,
missing bits, extra declared valid bits, nonzero high padding, or a terminal
state other than exactly L.

### tANS descriptor and frame layout

Each block has one 528-byte descriptor. All descriptors precede all payloads in
logical block order, matching the rANS frame-region convention.

| Offset | Size | Field | Rule |
|---:|---:|---|---|
| 0 | 4 | symbol count | configured block size or final remainder |
| 4 | 4 | payload size | exact bytes; at least 2 |
| 8 | 1 | table log | exactly 12 |
| 9 | 1 | final valid bits | 0 iff no bit bytes; otherwise 1..8 |
| 10 | 1 | flags | zero |
| 11 | 5 | reserved | zero |
| 16 | 512 | normalized frequencies | 256 little-endian uint16 values |

Payload size is exactly `2 + ceil(encoded_bit_count/8)`. When payload size is
two, final valid bits is zero. Otherwise it identifies the valid low bits of the
last byte; unused high bits are zero. Frequencies sum to 4096 and are nonzero
for at least one symbol. Frame block count, descriptor extent, payload sum, and
outer-frame boundary rules are identical to rANS.

### Hand-checkable tANS vectors

Using the deterministic spread above:

| Input | Nonzero normalized frequencies | Payload | Final valid bits |
|---|---|---|---:|
| `A` | `A:4096` | `00 00` | 0 |
| `AA` | `A:4096` | `00 00` | 0 |
| `AB` | `A:2048, B:2048` | `06 00 00` | 2 |
| `ABA` | `A:2731, B:1365` | `0C 0B 00` | 2 |

For `A`, descriptor bytes 0 through 15 are:

```text
01 00 00 00 02 00 00 00 0C 00 00 00 00 00 00 00
```

All frequency entries are zero except symbol `41` at descriptor offset 146,
whose little-endian uint16 value is `00 10`.
