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

## LZ77 variant 1 plus Adaptive Huffman FGK variant 1

The profile name is `lz77-adaptive-huffman`. This composition uses
dictionary algorithm ID 1, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1. It uses format version 1.0. The stream parameter regions are
the 16-byte LZ77 parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

The profile's maximum outer frame size is 2^20 raw bytes. This is a format-level
profile bound, not merely a CLI default: LZ77 can emit one 16-byte Literal token
for each raw byte, while Adaptive Huffman variant 1 accepts at most 2^24 input
symbols per frame. The exact canonical LZ77 token stream must be nonempty, a
multiple of 16 bytes, no larger than 2^24 bytes, and within the decoder's local
dictionary-serialized limit.

The reference profile configuration defaults to 65,536 raw bytes per frame.
This is smaller than the format maximum because the conservative combined
worst case is 16 token bytes per raw byte followed by 33 payload bytes per token
byte. The 65,536-byte default remains within marc's baseline payload and
aggregate-memory limits; a larger configured frame remains valid only when its
complete worst-case workspace also fits the caller's limits.

Every nonempty outer frame is exactly one Adaptive Huffman block. The FGK tree
starts from its single NYT root before the first byte of the frame's LZ77 token
stream and is discarded after that frame. No tree state or LZ77 history crosses
an outer frame boundary. Empty input has no frame and therefore no descriptor
or entropy state.

The generic frame header records raw bytes as `uncompressed size`, canonical
LZ77 token bytes as `dictionary serialized size`, Adaptive payload bytes as
`compressed payload size`, entropy block count 1, descriptor size 16, and
checksum trailer size zero. The body is:

```text
generic frame header
one Adaptive Huffman descriptor
Adaptive Huffman payload over the canonical LZ77 token bytes
```

The Adaptive descriptor's `symbol count` must equal `dictionary serialized
size`; its `payload size` must equal `compressed payload size`. Its flags,
reserved bytes, final-valid-bit rule, exact bit consumption, and zero-padding
requirements are unchanged from standalone Adaptive Huffman variant 1. There
is no separately stored dictionary-token region and no Blocked Huffman model or
view table.

Decoding is transactional at the outer frame boundary. Before publishing any
raw byte, the decoder must:

1. validate the exact stream pipeline, LZ77 parameters, sequence, frame size,
   declared extents, one-block count, and 16-byte descriptor extent;
2. parse and strictly validate the Adaptive descriptor and payload;
3. decode exactly `dictionary serialized size` token bytes into bounded private
   staging and require exact payload-bit exhaustion;
4. validate the complete LZ77 token stream, including reserved bytes,
   references, overlap semantics, terminal-token placement, and derivation of
   exactly `uncompressed size` raw bytes;
5. decode into bounded private raw staging; and only then
6. make that frame's raw bytes available to the caller.

Failure at any stage publishes no byte from the current frame. Earlier complete
frames may remain committed by the incremental stream decoder. Encoder planning
likewise completes the LZ77 token stream and Adaptive payload plan before
writing any byte of the frame. The conservative Adaptive workspace bound is 264
bits per token byte; configuration and decoder limits must reject any resulting
payload, dictionary staging, raw staging, or active aggregate extent before
allocation or mutation.

This section fixes the decoder-visible representation. The bounded C ABI
factory is published as `marc_lz77_adaptive_huffman_*`, and the explicit CLI
selector and dependency-free benchmark use the same name. Interoperability
schema 8 emits and accepts it as the nineteenth archive. Bounded frame/stream decoder
fuzzing and the
public-ABI completion
matrix cover deterministic
round-trips, arbitrary chunking, stable termination, and transactional final
frame rejection.

### Hand-checkable single-Literal frame

For raw input `A`, LZ77 emits one canonical Literal token:

```text
00 00 00 00 00 00 00 00 00 00 00 00 41 00 00 00
```

Starting from a fresh NYT tree, independent FGK traversal emits 31 payload bits.
The payload is `00 FF 17 74`, with seven valid bits in the final byte. The
Adaptive descriptor is:

```text
10 00 00 00 04 00 00 00 07 00 00 00 00 00 00 00
```

The complete 76-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 10 00 00 00  04 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
10 00 00 00 04 00 00 00  07 00 00 00 00 00 00 00
00 FF 17 74
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final four bytes are the FGK payload. This vector
contains no separately stored LZ77 token bytes.

## LZ77 variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lz77-dynamic-range`. This composition uses
dictionary algorithm ID 1, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. The stream parameter regions are
the 16-byte LZ77 parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

For raw frame size `F`, the complete canonical LZ77 token stream has extent
`S`, where `S` is nonzero, is a multiple of 16, and satisfies `S <= 16F`. The
Dynamic Range format accepts at most 2^24 input symbols, so this composition's
format-level raw-frame maximum is 2^20 bytes. The reference profile uses
65,536 raw bytes per frame. Its conservative payload bound is `P <= 2S + 5`;
the token, payload, private raw, and active aggregate extents must all pass
local limits before allocation or mutation.

Each nonempty outer frame is exactly one range-coded entropy block. The LZ77
history, range coder, and adaptive order-0 model all start fresh and are
discarded at the frame boundary. The range model consumes the finalized LZ77
token bytes in stored order without interpreting token fields. Empty input has
no frame, descriptor, payload, or entropy state.

The generic frame header records raw bytes as `uncompressed size`, `S` as
`dictionary serialized size`, `P` as `compressed payload size`, entropy block
count 1, descriptor size 16, and checksum trailer size zero. The body is:

```text
generic frame header
one Dynamic Range descriptor
Dynamic Range payload over the canonical LZ77 token bytes
```

The descriptor's `symbol count` must equal `S` and its `payload size` must
equal `P`. All flags and reserved bytes are zero. The payload is byte aligned,
uses the unchanged variant-1 delayed-carry and five-shift termination rules,
must begin with the canonical zero initialization byte, and must be consumed
exactly while producing `S` bytes. No LZ77 token region is stored separately.

Decoding is transactional at the outer frame boundary. Before publishing any
raw byte, the decoder must:

1. validate the exact pipeline, LZ77 parameters, frame sequence, format-level
   frame ceiling, declared extents, one-block count, and descriptor extent;
2. parse and validate the Dynamic Range descriptor and payload bounds;
3. decode exactly `S` bytes into bounded private token staging and require
   exact payload exhaustion;
4. validate the complete LZ77 token stream, including reserved bytes,
   references, overlap semantics, terminal placement, and derivation of
   exactly `F` raw bytes;
5. reconstruct into separate bounded private raw staging; and only then
6. make the completed frame's raw bytes available to the caller.

Failure at any stage publishes no byte from the current frame. The encoder
finalizes the LZ77 token stream and completes range-payload planning before
writing any frame byte. The internal bounded validator implements steps
1 through 4 and stops at private canonical token staging. The first private
decoder extends that boundary through step 5, checking raw capacity and the
descriptor-plus-payload-plus-token-plus-raw aggregate before entropy output,
then applying the specified overlap-copy semantics only to fully validated
tokens. The transactional complete-frame decoder implements step 6: it checks
caller output capacity before entropy output, reconstructs into private raw
staging, and copies exactly the declared raw extent only after every preceding
stage succeeds. Every failure leaves caller output unchanged. This section
reserves the decoder-visible representation only; no public C ABI factory, CLI
selector, benchmark, fuzz target, or interoperability profile is implied.

The exact-frame planner encodes canonical LZ77 tokens into bounded private
staging, plans Dynamic Range variant 1 over those fixed bytes, validates the
resulting generic frame header and aggregate workspace, and reports the exact
serialized extent. The deterministic encoder repeats that plan, rejects short
serialized output before writing it, then emits the header, descriptor, and
payload in order. Replanning the unchanged private token bytes must reproduce
the exact payload extent and descriptor; disagreement is an internal error.

The bounded streaming encoder writes the stream header and LZ77 parameter
prefix first, collects at most one configured raw frame, invokes the exact
planner and encoder, and drains the resulting immutable serialized frame before
reusing any storage. Input and output chunking do not change the bytes.
`Flush` does not close a partial frame. `EndInput` is retained after all known
input is accepted and becomes `EndOfStream` only after the final frame is fully
drained. `ResetBlock` is unsupported because the outer frame size already owns
the synchronized dictionary and entropy reset boundary.

The bounded streaming decoder collects and validates the complete stream prefix
before accepting frames. For each frame it collects the complete declared
serialized extent into bounded storage, entropy-decodes and validates all LZ77
tokens, reconstructs the declared raw extent into separate private storage,
and only then drains raw bytes. A malformed later frame therefore cannot
publish any raw byte from that frame or retract earlier completed frames.
Truncation under `EndInput`, trailing bytes, unsupported reset, impossible
workspace extents, and nested layer failures enter a sticky error state.

The fixed bounded profile defaults to 65,536 raw bytes per frame. For the
largest possible raw frame `F`, encoder requirements are `F` raw collection
bytes, `16F` canonical-token bytes, and `56 + 16 + (2(16F) + 5)` serialized
frame bytes. The last expression is a conservative capacity; the exact planner
may use less. Decoder requirements derive raw staging from the smaller of the
local frame limit and 2^20, token staging from the smaller of `16F`, the local
dictionary limit, and 2^24, and serialized-frame capacity solely from trusted
local limits. No untrusted frame field controls workspace allocation.

The public C profile maps encoder raw collection to `primary_workspace` and
combines token staging followed by serialized-frame storage in
`secondary_workspace`. Decode maps serialized-frame storage to primary and
combines token staging followed by private raw storage in secondary. Both
regions have byte alignment. The requirements query and factory repeat the same
checked formulas; factory failure publishes no transform handle. These ABI
operations do not alter the stream bytes.

The fixed-memory decoder fuzz boundary changes no representation. It exercises
the same complete-frame and streaming parsers under local limits and retains
canonical truncation and structural-corruption cases as ordinary regressions.
The explicit `lz77-dynamic-range` CLI selector also changes no representation;
it supplies the fixed reference-profile limits to the public C factory. The
matching benchmark uses the same factory and changes no representation.
Interoperability schema 14 emits and accepts this exact profile as archive 25
without changing the version-1.0 stream representation.

### Hand-checkable single-Literal frame

For raw input `A`, LZ77 emits one canonical 16-byte Literal token. Independently
applying Dynamic Range variant 1 to those fixed bytes produces this 16-byte
payload:

```text
00 00 00 00 00 00 00 00 00 06 5C D6 5F 00 00 00
```

The Dynamic Range descriptor is:

```text
10 00 00 00 10 00 00 00 00 00 00 00 00 00 00 00
```

The complete 88-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 10 00 00 00  10 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
10 00 00 00 10 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00  00 06 5C D6 5F 00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the final 16 bytes are the payload. The stream-
level LZ77 parameter region is not repeated in the frame.

## LZSS variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lzss-dynamic-range`. This composition uses
dictionary algorithm ID 2, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. The stream parameter regions are
the 16-byte LZSS parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

For raw frame size `F`, the complete canonical LZSS token stream has extent
`S`, where `S` is nonzero and satisfies `S <= 2F`. Literal tokens are two
bytes and Match tokens are nine bytes, so token boundaries must be recovered
by parsing rather than divisibility. Dynamic Range variant 1 accepts at most
2^24 input symbols; this composition therefore caps a raw frame at 2^23 bytes.
The reference profile uses 65,536 raw bytes per frame. Its conservative range
payload bound is `P <= 2S + 5`. Token, payload, private raw, and active
aggregate extents must all pass trusted local limits before allocation or
mutation.

Each nonempty outer frame is exactly one range-coded entropy block. LZSS
history, the range coder, and its adaptive order-0 model all reset at that
boundary. The range model consumes the finalized LZSS token bytes in stored
order without interpreting tags or fields. Empty input has no frame,
descriptor, payload, or entropy state.

The generic frame header records `F` as `uncompressed size`, `S` as
`dictionary serialized size`, `P` as `compressed payload size`, entropy block
count 1, descriptor size 16, and checksum trailer size zero. The body is:

```text
generic frame header
one Dynamic Range descriptor
Dynamic Range payload over the canonical LZSS token bytes
```

The descriptor's `symbol count` must equal `S` and its `payload size` must
equal `P`. All flags and reserved bytes are zero. The byte-aligned payload
uses unchanged Dynamic Range variant-1 initialization, delayed-carry, and
five-shift termination rules. It must be consumed exactly while producing
exactly `S` token bytes. No separate LZSS token region is stored.

Decoding is transactional at the outer frame boundary. Before publishing a raw
byte, the decoder must:

1. validate the exact variants, LZSS parameters, sequence, format frame cap,
   declared extents, one-block count, descriptor size, and aggregate bounds;
2. parse and validate the Dynamic Range descriptor and payload bounds;
3. range-decode exactly `S` bytes into private token staging and require exact
   payload exhaustion;
4. parse every complete variable-length LZSS token, reject unknown tags,
   truncation, nonzero reserved fields, invalid references or lengths, and
   require the tokens to derive exactly `F` raw bytes;
5. reconstruct into separate bounded private raw staging; and only then
6. publish the complete frame.

Failure at any stage publishes no current-frame byte. Encoding likewise
finalizes the complete LZSS token stream and range-payload plan before writing
the frame. The internal bounded validator implements steps 1 through 4. It
checks descriptor-plus-payload-plus-token aggregate storage before entropy
output, range-decodes only after a successful no-output preflight, and reports
the stable token index and byte offset of LZSS validation failures. It stops at
private canonical token staging. The bounded private decoder extends this
through step 5: it checks raw capacity and the
descriptor-plus-payload-plus-token-plus-raw aggregate before entropy output,
then applies the ordinary LZSS Literal and overlap-Match reconstruction only
to fully validated tokens. Its raw staging remains private and no caller-
visible byte is published. The transactional complete-frame decoder implements
step 6: it checks caller output capacity before entropy output, reconstructs
into private raw staging, and copies exactly `F` bytes only after every nested
stage succeeds. Every failure leaves caller output unchanged.

The exact-frame planner first determines and emits the complete canonical LZSS
token stream into bounded private staging. It plans Dynamic Range variant 1
over those frozen bytes, validates the exact header and aggregate workspace,
and reports the complete serialized extent. The deterministic encoder repeats
the range plan, rejects short serialized output before writing it, then emits
the header, descriptor, and payload in order. Replanning unchanged token bytes
must reproduce the exact payload extent; disagreement is an internal error.

The bounded streaming encoder emits the canonical 80-byte stream prefix, then
collects exactly one declared raw frame in caller-owned storage. It invokes the
exact planner and encoder only when that frame is complete, retains the
resulting immutable serialized frame across arbitrary output starvation, and
does not collect the next frame until the current one has drained. Ordinary
input chunking and `Flush` do not create frame boundaries. `EndInput` is valid
only with all remaining declared raw bytes and remains effective until the
last complete frame has drained. Empty input consists only of the prefix.
`ResetBlock` is unsupported because the format fixes frame resets. No public C
factory, CLI selector, benchmark, fuzz target, or interoperability profile is
implied.

The matching bounded streaming decoder incrementally collects the 80-byte
prefix and each 56-byte frame header. Before accepting a frame body it checks
`S <= 2F`, `S <= 2^24`, `P <= 2S + 5`, all caller capacities, the exact
serialized extent, and aggregate serialized-plus-token-plus-raw workspace.
It then collects exactly that frame, invokes the private complete-frame
decoder, and makes raw bytes drainable only after all entropy, token, and
reconstruction checks succeed. A malformed later frame cannot retract already
committed earlier frames but publishes none of its own bytes. Premature end,
trailing input, and invalid prefix, header, descriptor, payload, or token data
are malformed streams. `EndInput` remains effective while the final verified
frame drains.

The bounded workspace profile derives all regions without inspecting input
bytes. For largest configured raw frame `F`, encoder raw storage is `F`, token
staging is `2F`, the conservative payload is `4F + 5`, and the complete
serialized-frame region is `4F + 77`. Thus the three simultaneously live
encoder byte regions require at most `7F + 77` aggregate bytes. Empty input
requires zero frame workspaces. Decoder requirements are derived independently
from local limits: raw staging is bounded by `min(max_frame_size, 2^23)`,
token staging by the least of `2F`, the dictionary limit, and `2^24`, and
serialized-frame storage by the established frame-header-plus-internal-buffer
ceiling. Every multiplication, addition, and `size_t` conversion is checked.
The public C entry points are `marc_lzss_dynamic_range_config_init()`,
`marc_lzss_dynamic_range_workspace_requirements()`, and
`marc_lzss_dynamic_range_create()`. They bind exactly this representation and
the completed bounded streaming pair. The query exposes two caller-owned byte
workspaces and no views region; the factory publishes no handle on failure.
The public completion audit fixes 64-byte frames and proves byte-identical
archives across one-byte and mixed chunking schedules. It also proves that
header corruption, final-byte truncation, and trailing data in the fourth
frame publish exactly the first three verified frames and no byte of the
failing final frame. These tests add no new format field or variant.

The bounded decoder fuzz boundary applies this unchanged representation to
both exact-frame and incremental parsing. It preallocates encoded-frame,
canonical-token, private-raw, and final-output storage, derives no capacity
from serialized metadata, and uses a finite process-call ceiling. Permanent
regressions preserve transactional rejection for every proper prefix of a
canonical frame, saturated generic-frame extent fields, and an invalid
Dynamic Range descriptor. This evidence adds no format field or variant.
The explicit CLI selector `lzss-dynamic-range` binds this same profile through
the public C ABI and transactional file adapter; it does not infer a codec from
the stream or alter any serialized byte.
The dependency-free benchmark selects that same public profile and adds no
format field or variant. Its conservative output capacity and mandatory
round-trip verification are tooling policy, not part of the stream.
Interoperability schema 15 emits and accepts this exact profile as archive 26
after the frozen twenty-five-entry schema-14 order. This changes no format
version or profile representation.

### Hand-checkable single-Literal frame

For raw input `A`, LZSS emits the canonical two-byte Literal token `00 41`.
Independently applying Dynamic Range variant 1 to those bytes produces:

```text
00 00 41 BE 41 7C 00
```

The Dynamic Range descriptor is:

```text
02 00 00 00 07 00 00 00 00 00 00 00 00 00 00 00
```

The complete 79-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  07 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 07 00 00 00  00 00 00 00 00 00 00 00
00 00 41 BE 41 7C 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the last seven bytes are the payload. The
stream-level LZSS parameter region is not repeated in the frame.

## LZ78 variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lz78-dynamic-range`. This composition uses
dictionary algorithm ID 3, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. The stream parameter regions are
the 16-byte LZ78 parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

For raw frame size `F`, the canonical LZ78 token stream has extent `S`, where
`S` is nonzero, is a multiple of eight, and satisfies `S <= 8F`. Dynamic Range
variant 1 accepts at most 2^24 input symbols, so this composition's format-level
raw-frame ceiling is 2^21 bytes. The reference profile uses 65,536 raw bytes
per frame. Its conservative range payload bound is `P <= 2S + 5`. Token,
payload, aligned phrase-table, private raw, and aggregate extents must all pass
trusted local limits before allocation or mutation.

Each nonempty outer frame is one range-coded entropy block. The LZ78 phrase
dictionary, range coder, and adaptive order-0 model all reset at that boundary.
The range model consumes the finalized LZ78 token bytes in stored order without
interpreting token fields. Empty input has no frame, descriptor, payload, or
entropy state.

The generic frame header records `F` as `uncompressed size`, `S` as
`dictionary serialized size`, `P` as `compressed payload size`, entropy block
count 1, descriptor size 16, and checksum trailer size zero. The body is:

```text
generic frame header
one Dynamic Range descriptor
Dynamic Range payload over the canonical LZ78 token bytes
```

The descriptor's `symbol count` must equal `S` and its `payload size` must
equal `P`. All flags and reserved bytes are zero. The byte-aligned payload uses
the unchanged Dynamic Range variant-1 initialization, delayed-carry, and
five-shift termination rules. It must be consumed exactly while producing
exactly `S` token bytes. No separate LZ78 token region is stored.

Decoding is transactional at the outer frame boundary. Before publishing a raw
byte, the decoder must:

1. validate the exact variants, LZ78 parameters, sequence, format frame cap,
   declared extents, one-block count, descriptor size, and aggregate bounds;
2. parse and validate the Dynamic Range descriptor and payload bounds;
3. range-decode exactly `S` bytes into private token staging and require exact
   payload exhaustion;
4. require a complete sequence of eight-byte tokens, reject unknown tags,
   nonzero reserved or unused fields, forward phrase references, root
   FinalIndex, invalid terminal placement, and checked length overflow, and
   derive exactly `F` raw bytes in bounded aligned phrase workspace;
5. reconstruct into separate bounded private raw staging without recursion;
   and only then
6. publish the complete frame.

Failure at any stage publishes no current-frame byte. Encoding likewise fixes
the deterministic LZ78 parse and complete token stream before range planning
or frame output. The internal bounded validator implements steps 1 through 4.
It checks descriptor, payload, token, and aligned phrase-table aggregate
storage before entropy output, range-decodes only after a successful no-output
preflight, and reports stable LZ78 format, token-index, and byte-offset
failures. It stops after validating the private canonical token staging and
phrase graph.

The internal bounded private decoder extends that boundary through step 5. It
requires the complete raw staging extent and counts descriptor, payload,
tokens, aligned phrase entries, and raw bytes together before entropy output.
After complete phrase-graph validation, it invokes the existing bounded
non-recursive LZ78 decoder to reconstruct exactly `F` private bytes. It does
not publish caller-visible raw bytes. The transactional complete-frame decoder
implements step 6: it checks caller output capacity before entropy output,
reconstructs into private raw staging, and copies exactly `F` bytes only after
every nested stage succeeds. Every failure leaves caller output unchanged. No
combined encoder, streaming transform, C ABI factory, CLI selector, benchmark,
fuzz target, or interoperability profile is implied.

The internal exact-frame planner begins the encode side without changing this
representation. It first requires and counts aligned LZ78 encoder entries,
plans and emits the complete canonical token sequence into private staging,
then plans Dynamic Range payload size from exactly those frozen bytes. Encoder
entries, tokens, the 16-byte descriptor, and payload are checked as one
aggregate. It validates the resulting generic header fields and reports exact
serialized extent `56 + 16 + P`, but writes no serialized frame byte.

The matching internal exact-frame encoder first completes that plan and rejects
a serialized destination shorter than the reported extent before writing. It
replans Dynamic Range over the unchanged canonical token staging, requires the
same `P`, then serializes the generic header, descriptor, and exact payload in
order. Identical input and configuration produce byte-identical frame output.

The bounded known-size streaming encoder changes no representation. It emits
the ordinary 80-byte stream prefix, collects each complete configured raw
frame, invokes the exact-frame encoder, and drains that retained frame before
collecting the next. A nonterminal `Flush` never creates a shorter frame.
`EndInput` must accompany the complete remaining declared input and is retained
while prefix or frame bytes remain pending.

The matching bounded streaming decoder also changes no representation. It
collects the 80-byte prefix, then each 56-byte frame header, and checks the
profile equations, exact serialized extent, caller-owned staging capacities,
and aggregate buffered-byte limit before collecting that frame's descriptor
and payload. Once the complete admitted frame is present, it invokes the
private validator and reconstructor and makes raw bytes drainable only after
the whole frame succeeds. `EndInput` before the declared stream extent,
trailing input after it, and any malformed current frame are sticky errors;
bytes from an earlier completed frame remain committed.

The bounded profile changes no byte representation. Its encoder capacity is
derived from the largest actual raw frame `F`, conservative token extent
`S = 8F`, conservative payload `P = 2S + 5`, complete frame extent
`56 + 16 + P`, and `min(F, maximum entries)` opaque LZ78 encoder records. Its
decoder capacities are derived only from trusted local limits and the
2^21-byte format cap. Typed record layouts remain private C++ details and are
exposed to later ABI layers only as checked byte extents and alignments.

The public representation-neutral C entry points are
`marc_lz78_dynamic_range_config_init()`,
`marc_lz78_dynamic_range_workspace_requirements()`, and
`marc_lz78_dynamic_range_create()`. They require a known original size for
encoding, return direction-specific primary, secondary, and aligned opaque
views requirements, and publish no transform handle when configuration,
capacity, or alignment validation fails.

The completion audit changes no representation. It fixes 64-byte raw frames
only as a test profile and verifies through the public C ABI that input/output
chunking cannot change stream bytes and that a malformed fourth frame cannot
publish any of its raw extent.

The fixed-memory decoder fuzz boundary also changes no representation. It
applies arbitrary bounded bytes to the exact-frame private decoder when a
complete profile prefix is available and always to the incremental stream
decoder. All byte regions, phrase records, decoder limits, chunk ranges, and
the call ceiling are fixed independently of input contents.
The explicit CLI selector `lz78-dynamic-range` binds this unchanged profile
through the public C ABI and transactional file adapter. Its 64-KiB frame and
4-MiB local workspace policy add no serialized field and do not infer the
codec from stream contents.
The dependency-free benchmark selects the same unchanged public profile. Its
checked output capacity, untimed round-trip gate, timing, ratio, and workspace
reporting are tooling policy and add no format field or variant.
Interoperability schema 16 emits and accepts this exact profile as archive 27
after the frozen twenty-six-entry schema-15 order. This changes no format
version or profile representation.

### Hand-checkable single-Pair frame

For raw input `A`, LZ78 emits the canonical eight-byte Pair token:

```text
00 41 00 00 00 00 00 00
```

Independently applying Dynamic Range variant 1 to those bytes produces:

```text
00 00 41 BE 41 7C 00 00 00 00 00
```

The Dynamic Range descriptor is:

```text
08 00 00 00 0B 00 00 00 00 00 00 00 00 00 00 00
```

The complete 83-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  0B 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 0B 00 00 00  00 00 00 00 00 00 00 00
00 00 41 BE 41 7C 00 00  00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the last eleven bytes are the payload. The
stream-level LZ78 parameter region is not repeated in the frame.

## LZSS variant 1 plus Adaptive Huffman FGK variant 1

The reserved profile name is `lzss-adaptive-huffman`. This composition uses
dictionary algorithm ID 2, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1. It uses format version 1.0. The stream parameter regions are
the 16-byte LZSS parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

The format-level maximum outer frame size is 2^20 raw bytes. The bounded
reference profile uses 65,536 raw bytes per frame. For an `F`-byte raw frame,
the canonical LZSS token stream is nonempty, no larger than `2F` bytes, and
within the decoder's local dictionary-serialized limit. This follows from the
exact two-byte Literal cost; every nine-byte Match represents at least five raw
bytes and cannot increase the worst-case token extent.

Every nonempty outer frame is exactly one Adaptive Huffman block. Its FGK tree
starts from the single NYT root before the first LZSS token byte and is
discarded after the frame. The LZSS dictionary likewise starts empty for every
frame. Empty input has no frame, entropy descriptor, or payload.

The generic frame header records raw bytes as `uncompressed size`, canonical
LZSS token bytes as `dictionary serialized size`, Adaptive bytes as `compressed
payload size`, entropy block count one, descriptor size 16, and checksum trailer
size zero. The body is:

```text
generic frame header
one Adaptive Huffman descriptor
Adaptive Huffman payload over the canonical LZSS token bytes
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All descriptor flags,
reserved bytes, final-valid-bit rules, exact bit consumption, zero padding,
FGK numbering, swapping, and rescaling are unchanged from Adaptive Huffman
variant 1. No separately stored LZSS token region exists.

The conservative Adaptive bound is 264 bits, or 33 bytes, per token byte. A raw
frame of `F` bytes therefore reserves at most `66F` compressed payload bytes.
The format maximum remains valid only when the selected local compressed,
dictionary, raw-frame, and aggregate limits admit its complete worst case. The
65,536-byte reference frame bounds token staging at 131,072 bytes and payload
at 4,325,376 bytes.

Decoding is transactional at the outer frame boundary. Before publishing a raw
byte, the decoder must:

1. validate the exact pipeline, LZSS parameters, sequence, generic extents,
   one-block count, and 16-byte descriptor extent;
2. parse the Adaptive descriptor and decode exactly `dictionary serialized
   size` bytes into bounded private token staging with exact bit exhaustion;
3. validate every variable-length LZSS token, reserved and tag rule, distance,
   overlap, length, and terminal extent, deriving exactly `uncompressed size`;
4. reconstruct the validated token stream into separate bounded private raw
   staging; and only then
5. make that frame available to the caller.

Failure at any stage publishes no byte from the current frame. Earlier frames
may already be committed by the incremental decoder. Encoding completes the
deterministic LZSS parse, immutable token staging, Adaptive plan, generic header,
and complete destination-capacity check before writing the frame.

The complete-frame validator implements the first three decoding
checks above through validated canonical token staging. It accepts exactly one
frame and rejects trailing bytes. The internal frame decoder then implements
steps four and five through a distinct private raw-staging extent and an exact
post-success copy to caller output. The public C factory connects these
boundaries through the incremental controllers. The CLI selector
`lzss-adaptive-huffman` selects that same fixed representation through the
public factory.

The internal exact planner implements the encoding order above: determine and
serialize canonical LZSS tokens, plan Adaptive Huffman over those fixed bytes,
then validate all extents and the generic header. The frame encoder requires
complete output capacity before serializing and reproduces the hand-checkable
single-Literal frame exactly.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZSS parameter region and zero or more consecutive frames. Empty
input is exactly this 80-byte prefix. Input/output chunking does not change the
bytes. Nonterminal `Flush` does not close a partial frame, and `ResetBlock` is
unsupported at this cross-layer boundary.

The internal incremental decoder implements this known-size representation by
buffering at most one serialized frame, one canonical token region, and one raw
frame. It validates and reconstructs the entire current frame before exposing
that raw staging for partial draining. A malformed later frame cannot publish a
prefix from that frame.

The internal incremental encoder buffers at most one raw frame, one canonical
token region, and one complete serialized frame. It emits the prefix first,
never closes a partial frame for `Flush`, and retains a valid `EndInput` request
until the final short frame and all pending bytes have drained.

The internal reference-profile constructor reports the exact conservative
encoder regions for the largest known frame `F`: `F` raw bytes, `2F` token
bytes, and `56 + 16 + 66F` complete serialized-frame bytes. The three regions
must also fit as one checked aggregate. Decoder workspace is derived from local
limits and the profile's 1-MiB raw-frame and Adaptive decoded-symbol caps; it
does not infer an allocation from an untrusted frame header. These workspace
rules constrain implementations but add no bytes to the stream representation.
The public C entry points are
`marc_lzss_adaptive_huffman_config_init()`,
`marc_lzss_adaptive_huffman_workspace_requirements()`, and
`marc_lzss_adaptive_huffman_create()`; they select exactly this representation
and introduce no runtime algorithm substitution.

### Hand-checkable single-Literal frame

For raw input `A`, LZSS emits the canonical two-byte Literal token:

```text
00 41
```

Starting from a fresh NYT root, `00` contributes eight zero bits. The unseen
`41` then contributes NYT path `0` followed by its eight literal bits
LSB-first. The complete 17-bit payload is `00 82 00`, with one valid bit in the
final byte. The Adaptive descriptor is:

```text
02 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00
```

The complete 75-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  03 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 03 00 00 00  01 00 00 00 00 00 00 00
00 82 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final three bytes are the FGK payload. No LZSS
token byte is stored separately.

This representation is published through the bounded C factory, CLI, and
benchmark profile. Interoperability schema 9 appends it as the twentieth
archive without changing any earlier schema or stream byte.

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

## LZ78 variant 1 plus Adaptive Huffman FGK variant 1

The reserved profile name is `lz78-adaptive-huffman`. This composition uses
dictionary algorithm ID 3, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZ78 parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

The format-level maximum outer frame size is 2^20 raw bytes, and the bounded
reference profile uses 65,536 raw bytes. For an `F`-byte raw frame, LZ78 emits
at most `F` fixed eight-byte tokens, so dictionary staging is bounded by `8F`.
The token extent must be nonzero, a multiple of eight, no larger than 2^24
bytes, and within the decoder's local dictionary-serialized limit.

Each nonempty outer frame owns exactly one freshly reset FGK tree and one
freshly reset LZ78 phrase dictionary. The generic frame header records raw
bytes as `uncompressed size`, canonical LZ78 token bytes as `dictionary
serialized size`, Adaptive payload bytes as `compressed payload size`, entropy
block count one, descriptor size 16, and checksum trailer size zero. The body
is:

```text
generic frame header
one Adaptive Huffman descriptor
Adaptive Huffman payload over the canonical LZ78 token bytes
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All FGK tree, descriptor,
bit-consumption, final-valid-bit, padding, and reset rules are unchanged from
Adaptive Huffman variant 1. No separate LZ78 token region is stored.

The conservative Adaptive bound is 33 payload bytes per token byte. A raw frame
of `F` bytes therefore reserves at most `264F` compressed payload bytes. The
reference 65,536-byte frame bounds token staging at 524,288 bytes and payload
at 17,301,504 bytes. The selected raw, token, payload, phrase-table, complete
frame, and aggregate-workspace limits must admit every extent before allocation
or mutation. The aligned phrase workspace contains at most the lesser of token
count and the configured LZ78 maximum entries; index zero remains the implicit
root and occupies no phrase entry.

Decoding is transactional at the outer frame boundary. Before publishing raw
bytes, a decoder must validate the exact pipeline, LZ78 parameters, sequence,
generic extents, one-block count, and descriptor extent; decode exactly the
declared token bytes with exact payload-bit exhaustion; validate the complete
fixed-width LZ78 token grammar and phrase graph in bounded aligned workspace;
derive exactly the declared raw size; reconstruct into private raw staging;
and only then expose that frame. A malformed later frame may not publish any of
its bytes, although earlier frames may already be committed.

Encoding first fixes the deterministic LZ78 parse using its bounded aligned
phrase table, serializes the canonical tokens once into immutable staging,
plans Adaptive Huffman over those bytes, and validates the complete header and
destination extent before publishing a frame byte. Encoder-table, token,
descriptor, and payload extents are combined with checked arithmetic and must
fit the configured aggregate workspace bound. The known-size stream is
the ordinary 64-byte version-1.0 header followed by the 16-byte LZ78 parameter
region and zero or more frames. Empty input is exactly this 80-byte prefix.
Nonterminal `Flush` does not shorten a frame, and `ResetBlock` is unsupported
at this composition boundary. A streaming encoder must finish and drain each
exact frame before collecting its successor; input and output chunking alone
must not alter any serialized byte.
The matching streaming decoder buffers one complete serialized frame, validates
and reconstructs it privately, and only then drains raw bytes. No byte from a
malformed frame is observable, although preceding frames may already have been
committed.

The C ABI functions `marc_lz78_adaptive_huffman_config_init()`,
`marc_lz78_adaptive_huffman_workspace_requirements()`, and
`marc_lz78_adaptive_huffman_create()` select exactly this representation. The
aligned views workspace is opaque: it holds encoder records while encoding and
phrase records while decoding. Its required byte count and alignment must be
queried again after changing direction, frame size, entry limit, original
size, or any decoder limit.

### Hand-checkable single-Pair frame

For raw input `A`, LZ78 emits the canonical Pair token:

```text
00 41 00 00 00 00 00 00
```

The first `00` contributes the eight-bit unseen literal `00`. The unseen `41`
then contributes NYT path `0` followed by literal `41` LSB-first. Each of the
remaining six known `00` symbols contributes root-right path `1`. The complete
23-bit payload is `00 82 7E`, with seven valid bits in the final byte. The
Adaptive descriptor is:

```text
08 00 00 00 03 00 00 00 07 00 00 00 00 00 00 00
```

The complete 75-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  03 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 03 00 00 00  07 00 00 00 00 00 00 00
00 82 7E
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final three bytes are the FGK payload. This
representation is published through the bounded C factory, transactional CLI,
and verified benchmark profile. Interoperability schema 10 appends it as the
twenty-first archive without changing any earlier schema or stream byte. The
complete-frame validator implements the header-through-phrase-graph portion of
the required decode order. The frame decoder then expands the validated phrase
graph iteratively into separate private raw staging and copies to caller output
only after exact reconstruction succeeds.

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

The public name for this exact representation is
`lz78-blocked-huffman`. Profile sizing retains the three caller-workspace shape
while treating the aligned views region as opaque storage. Encoding uses one
LZ78 encoder-entry array. Decoding places Blocked Huffman block views first,
aligns the next offset for LZ78 phrase entries, and places the phrase array
there. The checked partition helper must accept this exact derived layout
before exposing either typed span. The public C factory, CLI selector,
benchmark adapter, and interoperability schema-4 tools emit and accept this
representation.

## LZW variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lzw-dynamic-range`. This composition uses
dictionary algorithm ID 4, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZW parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZW dictionary and one freshly
reset adaptive order-0 range model. LZW first produces its complete canonical
LSB-first packed-code byte stream, including the required zero padding in the
high bits of its final byte. Dynamic Range then treats every byte of that
finalized region as one symbol. It does not observe code boundaries and does
not remove or reinterpret the LZW padding.

The generic frame header records raw bytes as `uncompressed size`, packed LZW
code bytes as `dictionary serialized size`, Dynamic Range payload bytes as
`compressed payload size`, entropy block count one, descriptor size 16, and
checksum trailer size zero. The body is:

```text
generic frame header
one Dynamic Range descriptor
Dynamic Range payload over the packed LZW bytes
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All Dynamic Range interval,
normalization, delayed-carry, five-byte termination, exact payload-exhaustion,
model-update, rescaling, descriptor, and reset rules are unchanged from
Dynamic Range variant 1. No separate packed-code region is stored.

For raw frame size `F` and configured maximum LZW code width `W`, the checked
packed-code ceiling is:

```text
S = ceil(F * W / 8)
```

The conservative Dynamic Range payload ceiling is:

```text
P = 2S + 5
```

The generated LZW entry count is zero when `F` is zero; otherwise it is at most
the lesser of `F - 1`, `2^W - 256`, and the local dictionary-entry limit. The
format-level raw-frame cap is 2^20 bytes. The reference profile uses
`F = 65,536` and `W = 16`, giving `S = 131,072`, `P = 262,149`, and at most
65,280 generated entries. Every product, ceiling division, complete-frame
extent, aligned entry-table extent, and aggregate workspace sum must be checked
before allocation or mutation.

Encoding must freeze the deterministic LZW parse, width schedule, packed bytes,
and final zero padding in caller-owned staging before Dynamic Range planning.
The exact-frame planner now validates the complete header, exact descriptor and
payload extents, and aggregate workspace bounds and reports the serialized
extent without writing serialized output. The complete-frame encoder validates
destination extent before publishing any frame byte, repeats the exact range
plan over the frozen packed bytes, and explicitly serializes the header,
descriptor, and payload.

Decoding is transactional at the outer frame boundary. It first validates the
pipeline IDs and variants, LZW parameters, sequence, generic extents, one-block
count, 16-byte descriptor extent, packed-code ceiling, range-payload ceiling,
and caller-owned capacities. Dynamic Range must then reconstruct exactly the
declared packed-byte count with exact payload exhaustion. The ordinary LZW
validator consumes that complete private span, reproduces the specified
width-growth schedule, validates dictionary references and `KwKwK`, requires
zero high padding bits in the final packed byte, and derives exactly the
declared raw extent into bounded phrase records. Before entropy output, the
private decoder requires the complete raw staging extent and counts it with
the descriptor, payload, packed bytes, and aligned phrase records against the
aggregate workspace limit. It then reconstructs only the completely validated
phrase graph into private raw staging. The transactional complete-frame
boundary checks destination capacity before entropy output and copies this
private extent once only after every operation succeeds.
A malformed later frame cannot publish any of that frame's raw bytes, although
earlier frames may already be committed.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZW parameter region and zero or more frames. Empty input is exactly
this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and input/output
chunking alone must not change serialized bytes. The bounded streaming encoder
implements this contract by collecting one raw frame, preparing its complete
serialized representation, and draining that immutable extent before
collecting the next frame. The matching streaming decoder parses and bounds one
complete encoded frame, transactionally reconstructs it into private raw
staging, and drains that immutable raw extent before collecting another frame.

### Hand-checkable single-code frame

For raw input `A`, standalone LZW variant 1 emits code 65 at width nine and
therefore produces the finalized packed bytes:

```text
41 00
```

The high seven bits of the second byte are LZW padding, but the complete byte
is an ordinary zero-valued Dynamic Range symbol. Independently applying
Dynamic Range variant 1 to `41 00` produces:

```text
00 40 FF FF BF 00 00
```

The descriptor is:

```text
02 00 00 00 07 00 00 00 00 00 00 00 00 00 00 00
```

The complete 79-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  07 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 07 00 00 00  00 00 00 00 00 00 00 00
00 40 FF FF BF 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the final seven bytes are its payload. The
stream-level LZW parameter region is not repeated in the frame.

The first combined implementation validates one exact complete frame through
both encoded layers into caller-owned packed-byte staging and phrase records.
It checks generic extents, the packed and payload ceilings, all caller
capacities, and aggregate workspace before parsing the descriptor or decoding
entropy. Dynamic Range must exhaust the payload exactly before the ordinary
LZW validator checks width transitions, references, `KwKwK`, final padding,
and the declared raw extent. This boundary reconstructs and publishes no raw
bytes; later decoding and streaming work must retain the same validation order.

The public C factory and `lzw-dynamic-range` CLI selector use this exact
representation. The CLI selects 65,536-byte raw frames, maximum code width 16,
a 131,072-byte packed ceiling, a 262,149-byte Dynamic Range payload ceiling,
65,280 generated entries, and an 8-MiB aggregate internal limit. It obtains
all concrete workspace extents and opaque alignment from the public
requirements query.
The dependency-free benchmark selects the same unchanged profile. Its checked
capacity, mandatory untimed round trip, timing, ratio, and workspace reporting
are tooling policy and add no format field or variant.
Interoperability schema 17 emits and accepts this exact profile as archive 28
after the frozen twenty-seven-entry schema-16 order. This changes no format
version or profile representation.

## LZW variant 1 plus Adaptive Huffman FGK variant 1

The profile name is `lzw-adaptive-huffman`. This composition uses
dictionary algorithm ID 4, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZW parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZW dictionary and one freshly
reset FGK tree. The LZW encoder first produces its complete canonical LSB-first
packed-code byte stream, including the required zero padding in the high bits
of its final byte. Adaptive Huffman then treats every byte of that finalized
region as an ordinary symbol; it does not see LZW code boundaries and does not
remove or reinterpret LZW padding.

The generic frame header records raw bytes as `uncompressed size`, packed LZW
code bytes as `dictionary serialized size`, Adaptive payload bytes as
`compressed payload size`, entropy block count one, descriptor size 16, and
checksum trailer size zero. The body is:

```text
generic frame header
one Adaptive Huffman descriptor
Adaptive Huffman payload over the packed LZW bytes
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All FGK descriptor,
bit-consumption, final-valid-bit, padding, update, rescaling, and reset rules
are unchanged from Adaptive Huffman variant 1. No separate packed-code region
is stored.

For raw frame size `F` and configured maximum LZW code width `W`, the checked
packed-code ceiling is `S = ceil(F * W / 8)` bytes. The conservative Adaptive
payload ceiling is `33S` bytes. The generated LZW entry count is zero when `F`
is zero; otherwise it is at most the lesser of `F - 1`, `2^W - 256`, and the
local dictionary-entry limit. The format-level raw-frame cap remains 2^20
bytes. The bounded reference profile uses `F = 65,536` and `W = 16`, giving
`S = 131,072` packed bytes, a 4,325,376-byte Adaptive payload ceiling, and at
most 65,280 generated entries.

Every product, ceiling division, complete-frame extent, aligned entry-table
extent, and aggregate sum must be checked before allocation or mutation.
Encoding fixes the deterministic LZW parse and packed bytes in caller-owned
staging before Adaptive planning. It validates the complete header, descriptor,
payload, and destination extent before publishing a frame byte.

Decoding is transactional at the outer frame boundary. It first validates the
pipeline, LZW parameters, sequence, generic extents, one-block count, and
descriptor extent. Adaptive Huffman must then reconstruct exactly the declared
packed-byte count with exact payload-bit exhaustion. The ordinary LZW validator
must consume that complete staged region, reproduce the specified width-growth
schedule, validate dictionary references and `KwKwK`, require zero high padding
bits in the final packed byte, and derive exactly the declared raw extent into
bounded phrase records. Reconstruction occurs in private raw staging, and only
a completely successful frame may be published. A malformed later frame may
not publish any of its bytes, although earlier frames may already be committed.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZW parameter region and zero or more frames. Empty input is exactly
this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and input/output
chunking alone must not change serialized bytes. The bounded streaming encoder
implements this contract by collecting one raw frame, preparing its complete
serialized representation, and draining that immutable extent before
collecting the next frame. The matching streaming decoder parses and bounds one
complete encoded frame, transactionally reconstructs it into private raw
staging, and drains that immutable raw extent before collecting another frame.

The C ABI functions `marc_lzw_dynamic_range_config_init()`,
`marc_lzw_dynamic_range_workspace_requirements()`, and
`marc_lzw_dynamic_range_create()` select exactly this representation. The
aligned views workspace remains opaque and direction-specific; requirements
must be queried again after changing any configuration or local limit.

The C ABI functions `marc_lzw_adaptive_huffman_config_init()`,
`marc_lzw_adaptive_huffman_workspace_requirements()`, and
`marc_lzw_adaptive_huffman_create()` select exactly this representation. The
aligned views workspace is opaque: it contains encoder dictionary entries in
the encode direction and decoder phrase entries in the decode direction.
Requirements must be queried again after changing direction, known original
size, frame size, maximum code width, or any local limit.

### Hand-checkable single-code frame

For raw input `A`, LZW emits code 65 at width nine, producing packed bytes:

```text
41 00
```

The first Adaptive symbol `41` is emitted as its eight literal bits. The unseen
`00` then contributes NYT path `0` followed by eight zero literal bits. The
complete 17-bit payload is `41 00 00`, with one valid bit in the final byte.
The Adaptive descriptor is:

```text
02 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00
```

The complete 75-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  03 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 03 00 00 00  01 00 00 00 00 00 00 00
41 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final three bytes are the FGK payload. The high
seven zero bits in the second packed LZW byte are dictionary-layer padding but
belong to an ordinary zero-valued symbol at the entropy layer. The first
combined implementation validates one exact complete frame through both
encoded layers into caller-owned packed-byte and phrase staging. The next
bounded boundary reconstructs that validated stream into separate caller-owned
private raw staging, counting its complete extent in pre-decode capacity and
aggregate-workspace checks. The transactional complete-frame decoder then
copies that entire private span to caller-visible output only after every
operation succeeds. This is still an internal frame API rather than a public C
decoder. The matching internal planner first freezes the complete canonical LZW
packed region, then plans Adaptive Huffman over exactly those bytes; the
deterministic encoder emits the generic header, descriptor, and payload only
after complete capacity and workspace admission.

The first streaming encoder emits the ordinary 80-byte prefix, buffers no more
than one raw frame, produces that frame solely through the exact planner and
encoder above, and drains the completed bytes under the core partial-buffer
contract. Filling a frame closes it; `Flush` alone does not close a partial
frame, and retained `EndInput` completes only after all prefix and frame bytes
drain. Input and output chunk sizes do not alter the representation.

The matching streaming decoder collects the prefix and one admitted serialized
frame in bounded caller-owned storage. It validates the header and conservative
packed/payload bounds before body collection, invokes the complete private
reconstruction transaction only after the frame is present, and drains only
validated raw bytes. Every truncation and trailing byte is invalid; a malformed
later frame cannot publish any of that frame, while earlier frames remain
committed.

The public C factory and `lzw-adaptive-huffman` CLI selector use this exact
representation. The CLI selects 65,536-byte raw frames, maximum code width 16,
a 131,072-byte packed ceiling, a 4,325,376-byte Adaptive payload ceiling,
65,280 generated entries, and an 8-MiB aggregate internal limit. It obtains
all concrete workspace extents and opaque alignment from the public
requirements query. The public benchmark uses the same configuration and
verifies a byte-exact round trip before timing. Interoperability admission
schema 11 appends this profile as the twenty-second archive without changing
any earlier schema or stream byte. The recorded schema-11 artifacts passed the
complete bidirectional x86-64 verification contract; that evidence does not
change the representation.

## LZW variant 1 plus Blocked Huffman variant 1

This composition uses dictionary algorithm ID 4, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. Its stream parameter regions
are the 16-byte LZW parameters followed by the empty Blocked Huffman parameter
region. `entropy block size` counts bytes in the canonical packed LZW code
stream, including its final zero-padded byte. Blocks reset at and cannot cross
an outer frame.

The generic frame header records raw bytes as `uncompressed size`, packed LZW
code bytes as `dictionary serialized size`, stored entropy bytes as
`compressed payload size`, the exact Blocked Huffman block count, and the
complete descriptor/model region size. The body is:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate packed-code region is stored. Entropy decoding must produce exactly
`dictionary serialized size` bytes. Before any raw-byte publication, the LZW
validator must consume that complete staged region, reproduce the specified
width schedule and dictionary growth, validate the `KwKwK` case and final zero
padding, and derive exactly `uncompressed size` bytes. The entropy boundary has
no relationship to an LZW code boundary; a variable-width code may cross two
entropy input bytes and an entropy block may end at any packed-byte boundary.

For raw frame size `F` and configured maximum code width `W`, a nonempty raw
byte contributes at most one LZW code. The canonical dictionary staging bound
is therefore `S = ceil(F * W / 8)` bytes. The generated-entry bound is zero
when `F` is zero; otherwise it is the lesser of `F - 1`, `2^W - 256`, and the
local dictionary-entry limit. For entropy block size `E`, the block-count bound
is `ceil(S / E)`. Every multiplication, ceiling division,
descriptor extent, aligned typed-workspace extent, and aggregate sum must be
checked before allocation or serialized output.

### Hand-checkable LZW combined raw-block frame

For raw input `A`, default LZW parameters emit one nine-bit code with packed
bytes `41 00`. With entropy block size two, Blocked Huffman selects raw
representation. The complete 74-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 02 00 00 00  02 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
02 00 00 00 02 00 00 00  00 00 01 08 00 00 00 00
41 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final two bytes are the unchanged packed
LZW code stream. The high seven bits of its final byte are LZW padding and are
data bytes from the entropy layer's perspective. The 16-byte LZW parameter
region remains stream-level and is not repeated in this frame.

The reference frame encoder must first plan the complete LZW parse using an
aligned caller-owned encoder table, emit the canonical packed codes once into
staging, and then plan Blocked Huffman over those exact bytes. Only after all
extents and the generic header validate may it publish serialized output.
Repeating the plan during encoding must reproduce the packed-code and entropy
bytes exactly.

The reference frame decoder treats entropy output and LZW phrase records as
uncommitted state. It must validate the complete packed-code stream into an
aligned caller-owned phrase table before checking raw destination capacity and
running transactional expansion. A header, entropy, code-width, code,
dictionary, padding, declared-size, workspace, or raw-capacity failure
publishes no raw byte.

The known-size stream uses the ordinary 64-byte version 1.0 header, followed by
the 16-byte LZW parameter region and zero or more combined frames. Empty input
is exactly this 80-byte prefix. Both the LZW dictionary and every Blocked
Huffman model reset at each frame. Strict decoding requires the frames to
derive exactly `original size` bytes and rejects trailing serialized data.

Incremental encoding and decoding must emit and accept exactly this
representation under arbitrary input and output chunking. A final short frame
is emitted only after the known-size input contract is satisfied. A decoded
frame is not exposed until its entropy payload, packed code stream, phrase
table, padding, and raw extent validate completely; earlier frames may already
be committed when a later frame fails. Nonterminal `Flush` does not shorten a
frame, and `ResetBlock` is unsupported at this profile boundary.

The public name for this exact representation is `lzw-blocked-huffman`. Its
three-region C ABI keeps frame bytes in its primary and secondary regions and
uses aligned opaque views storage for one LZW encoder-entry array, or for
Blocked Huffman block views followed by checked padding and one LZW decoder
phrase array. The checked partition helper rederives and validates the complete
layout before exposing either typed span. The CLI selector uses that public C
factory and does not define another format variant. Interoperability schema 5
emits and accepts this exact profile as its sixteenth archive.

## LZD variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lzd-dynamic-range`. This composition uses
dictionary algorithm ID 5, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZD parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZD phrase dictionary and one
freshly reset adaptive order-0 range model. LZD first produces its complete
canonical sequence of eight-byte little-endian reference pairs. Dynamic Range
then treats every byte in that finalized sequence as one symbol. It does not
observe token, reference-field, or terminal-marker boundaries.

The generic frame header records raw bytes as `uncompressed size`, LZD token
bytes as `dictionary serialized size`, Dynamic Range payload bytes as
`compressed payload size`, entropy block count one, descriptor size 16, and
checksum trailer size zero. The body is:

```text
generic frame header
one Dynamic Range descriptor
Dynamic Range payload over the complete LZD token region
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All Dynamic Range interval,
normalization, delayed-carry, five-byte termination, exact payload-exhaustion,
model-update, rescaling, descriptor, and reset rules are unchanged from
Dynamic Range variant 1. No separate token region is stored.

For raw frame size `F`, the checked token ceiling is:

```text
S = 8 * ceil(F / 2)
```

The conservative Dynamic Range payload ceiling is:

```text
P = 2S + 5
```

At most `floor(F / 2)` right-present tokens can create phrase entries; the
phrase-record count is the lesser of that value and the configured LZD
maximum. Iterative expansion requires at most that phrase count plus one
reference. The format-level raw-frame cap remains 2^20 bytes. The reference
profile uses `F = 65,536`, giving `S = 262,144`, `P = 524,293`, at most 32,768
generated phrases, and at most 32,769 expansion references. Every ceiling
division, product, frame extent, aligned phrase-record extent, expansion-stack
extent, and aggregate workspace sum must be checked before allocation or
mutation.

Encoding must freeze the deterministic LZD parse and complete token bytes in
caller-owned staging before Dynamic Range planning. It must validate the
complete header, descriptor, payload, destination extent, and workspace bounds
before publishing any frame byte. The exact-frame planner now fixes the
canonical token bytes, validates the exact descriptor and payload extents plus
aggregate workspace, validates the synthesized generic header, and reports the
serialized extent without writing serialized output. The complete-frame
encoder validates destination extent before publication, repeats the exact
range plan over the frozen token bytes, and explicitly serializes the header,
descriptor, and payload.

Decoding is transactional at the outer frame boundary. It first validates the
pipeline IDs and variants, LZD parameters, sequence, generic extents, one-block
count, 16-byte descriptor extent, token ceiling, range-payload ceiling, and
caller-owned capacities. Dynamic Range must then reconstruct exactly the
declared token-byte count with exact payload exhaustion. The ordinary LZD
validator consumes that complete private span, requires a multiple of eight
bytes, validates every backward phrase reference and checked phrase length,
permits an absent right reference only on the final token, and derives exactly
the declared raw extent into bounded phrase records. Before entropy output, the
private decoder requires the complete raw and expansion-stack extents and
counts them with the descriptor, payload, token bytes, and aligned phrase
records against the aggregate workspace limit. It then reconstructs only the
completely validated phrase graph iteratively into private raw staging. The
transactional complete-frame boundary checks destination capacity before
entropy output and copies this private extent once only after every operation
succeeds. A malformed later frame cannot publish any of that frame's raw
bytes, although earlier frames may already be committed.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZD parameter region and zero or more frames. Empty input is exactly
this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and input/output
chunking alone must not change serialized bytes. The bounded streaming encoder
collects one raw frame, prepares its complete serialized representation, and
drains that immutable extent before collecting the next frame. The matching
streaming decoder parses and bounds one complete encoded frame,
transactionally reconstructs it into private raw staging, and drains that
immutable raw extent before collecting another frame.

The C ABI functions `marc_lzd_dynamic_range_config_init()`,
`marc_lzd_dynamic_range_workspace_requirements()`, and
`marc_lzd_dynamic_range_create()` select exactly this profile and do not define
another representation. Byte workspace contains the raw/frame and token/raw
regions reported for the immutable direction; aligned opaque workspace holds
private LZD encoder entries or decoder phrase and expansion records.

The `lzd-dynamic-range` CLI selector uses this same factory with 65,536-byte
raw frames, a 262,144-byte token ceiling, a 524,293-byte Dynamic Range payload
ceiling, at most 65,536 dictionary entries, and a 16-MiB aggregate internal
limit. It adds no representation or parameter variant and obtains all concrete
workspace extents and opaque alignment from the public requirements query.
The dependency-free benchmark selects the same unchanged profile. Its checked
capacity, mandatory untimed round trip, timing, ratio, and workspace reporting
are tooling policy and add no format field or variant.
Interoperability schema 18 emits and accepts this exact profile as archive 29
after the frozen twenty-eight-entry schema-17 order. The schema adds no stream
field or codec variant.

### Hand-checkable terminal-token frame

For raw input `A`, standalone LZD variant 1 emits:

```text
41 00 00 00 FF FF FF FF
```

Independently applying Dynamic Range variant 1 to those eight complete bytes
produces:

```text
00 40 FF FF C4 DC 92 F3 69 BC 8B 00
```

The descriptor is:

```text
08 00 00 00 0C 00 00 00 00 00 00 00 00 00 00 00
```

The complete 84-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  0C 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 0C 00 00 00  00 00 00 00 00 00 00 00
00 40 FF FF C4 DC 92 F3  69 BC 8B 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the final twelve bytes are its payload. The
stream-level LZD parameter region is not repeated in the frame.

The first combined implementation validates one exact complete frame through
both encoded layers into caller-owned token staging and phrase records. It
checks generic extents, the token and payload ceilings, all caller capacities,
and aggregate workspace before parsing the descriptor or decoding entropy.
Dynamic Range must exhaust the payload exactly before the ordinary LZD
validator checks the multiple-of-eight token extent, backward references,
terminal absence, phrase lengths, and declared raw extent. This boundary
reconstructs and publishes no raw bytes; later decoding and streaming work
must retain the same validation order.

## LZD variant 1 plus Adaptive Huffman FGK variant 1

The reserved profile name is `lzd-adaptive-huffman`. This composition uses
dictionary algorithm ID 5, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZD parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZD phrase dictionary and one
freshly reset FGK tree. The LZD encoder first completes its canonical sequence
of eight-byte little-endian reference pairs. Adaptive Huffman then treats every
byte in that finalized sequence as an ordinary symbol. It does not see token,
reference-field, or terminal-marker boundaries.

The generic frame header records raw bytes as `uncompressed size`, LZD token
bytes as `dictionary serialized size`, Adaptive payload bytes as `compressed
payload size`, entropy block count one, descriptor size 16, and checksum
trailer size zero. The body is:

```text
generic frame header
one Adaptive Huffman descriptor
Adaptive Huffman payload over the complete LZD token region
```

The descriptor's `symbol count` equals `dictionary serialized size`, and its
`payload size` equals `compressed payload size`. All FGK descriptor,
bit-consumption, final-valid-bit, padding, update, rescaling, and reset rules
are unchanged from Adaptive Huffman variant 1. No separate token region is
stored.

For raw frame size `F`, the checked token ceiling is
`S = 8 * ceil(F / 2)` bytes. At most `floor(F / 2)` right-present tokens can
create phrase entries; the phrase-record count is the lesser of that value and
the configured LZD maximum. Iterative expansion requires at most that phrase
count plus one reference. The conservative Adaptive payload ceiling is `33S`
bytes. The format-level raw-frame cap remains 2^20 bytes. The bounded reference
profile uses `F = 65,536`, giving `S = 262,144` token bytes, an 8,650,752-byte
Adaptive payload ceiling, at most 32,768 generated phrases, and at most 32,769
expansion references.

Every ceiling division, product, frame extent, typed-record extent, and
aggregate sum must be checked before allocation or mutation. Encoding fixes
the deterministic LZD parse and complete token bytes in caller-owned staging
before Adaptive planning. It validates the complete header, descriptor,
payload, and destination extent before publishing a frame byte.

Decoding is transactional at the outer frame boundary. It first validates the
pipeline, LZD parameters, sequence, generic extents, one-block count, and
descriptor extent. Adaptive Huffman must then reconstruct exactly the declared
token-byte count with exact payload-bit exhaustion. The ordinary LZD validator
must consume that complete staged region, require a multiple of eight bytes,
validate every backward phrase reference and checked phrase length, permit an
absent right reference only on the final token, and derive exactly the declared
raw extent into bounded phrase records. Iterative reconstruction occurs in
private raw staging, and only a completely successful frame may be published.
A malformed later frame may not publish any of its bytes, although earlier
frames may already be committed.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZD parameter region and zero or more frames. Empty input is exactly
this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and input/output
chunking alone must not change serialized bytes.

### Hand-checkable terminal-token frame

For raw input `A`, LZD emits the terminal token:

```text
41 00 00 00 FF FF FF FF
```

Feeding those eight bytes to one fresh FGK tree yields an empty path for the
first byte, then paths `0`, `01`, `1`, `00`, `001`, `01`, and `11`. The empty
first path is followed by literal `41`, and the first occurrences of `00` and
`FF` are also followed by their eight literal bits. The complete 37-bit
Adaptive payload is
`41 00 CC 3F 1D`, with five valid bits in the final byte. The descriptor is:

```text
08 00 00 00 05 00 00 00 05 00 00 00 00 00 00 00
```

The complete 77-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  05 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 05 00 00 00  05 00 00 00 00 00 00 00
41 00 CC 3F 1D
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final five bytes are the FGK payload. This vector
is assembled from the standalone LZD and Adaptive Huffman encoders plus generic
serializers. The first combined validator accepts exactly one complete frame,
checks all extents and caller capacities before entropy output, reconstructs
the token region into private staging, and validates the complete LZD phrase
graph and declared raw extent. The next bounded boundary reconstructs that
validated graph iteratively into separate caller-owned private raw staging.
It counts raw capacity and the conservative phrase-count-plus-one expansion
stack in pre-decode capacity and aggregate-workspace checks. It still publishes
no partial output: the transactional complete-frame decoder checks destination
capacity before entropy output, then copies the entire private raw span only
after every operation succeeds. This remains an internal frame API, and no
partial output is exposed on failure. The exact-frame encoder fixes the entire
canonical LZD token stream in private staging before Adaptive planning, counts
typed encoder records, token bytes, descriptor, and exact payload against the
workspace limit, and rejects a short serialized destination before writing.
It reproduces the 77-byte vector above. The internal bounded streaming encoder
emits the ordinary
80-byte stream prefix, buffers at most one configured raw frame, delegates each
complete frame to that exact encoder, and drains the resulting bytes without
changing their representation. `Flush` leaves a partial frame open, while
`EndInput` is retained until all prefix and frame bytes have drained. The
matching internal streaming decoder collects
the complete prefix, header, descriptor, and payload; checks token, phrase,
expansion, raw, and aggregate extents before entropy output; reconstructs into
private raw staging; and only then drains that successful frame. A malformed
frame publishes none of its raw bytes, even when earlier frames were already
committed.

The internal bounded profile does not alter these bytes. It calculates the
largest raw, token, payload, complete-frame, typed encoder, phrase, and
expansion regions from the fixed profile and local hard limits. Phrase records
and the iterative `uint32_t` expansion stack share one opaque caller allocation
with explicit alignment and offset validation; caller-visible output remains
outside scratch-workspace accounting.

The public C factory selects this same fixed profile and adds no format
variant. Encoding remains known-size; decoder workspace sizing uses only local
limits, and stream parameters are validated after collection against them.
The `lzd-adaptive-huffman` CLI selector uses that factory with the 65,536-byte
reference frame and adds no representation or parameter variant.
The benchmark adapter uses the same profile and likewise adds no format variant.
Interoperability schema 12 emits and accepts this exact profile as its twenty-
third archive without changing the version-1.0 stream representation. The
recorded schema-12 bundles passed the complete bidirectional x86-64 verification
contract; that evidence does not change the representation.

## LZD variant 1 plus Blocked Huffman variant 1

This composition uses dictionary algorithm ID 5, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. Its stream parameter regions
are the 16-byte LZD parameters followed by the empty Blocked Huffman parameter
region. The public name is `lzd-blocked-huffman`.

`entropy block size` counts bytes in the canonical fixed-width LZD reference-
pair stream. Blocks reset at and cannot cross an outer frame; a block boundary
need not coincide with an eight-byte token boundary. The generic frame header
records raw bytes as `uncompressed size`, LZD token bytes as `dictionary
serialized size`, stored entropy bytes as `compressed payload size`, the exact
Blocked Huffman block count, and the complete descriptor/model region size.
The body is:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate LZD token region is stored. Entropy decoding must produce exactly
`dictionary serialized size` bytes, and that size must be a multiple of eight.
Before any raw-byte publication, the ordinary LZD validator must consume the
complete staged token region, validate every backward phrase reference and
terminal absent-right form, construct the bounded acyclic phrase grammar, and
derive exactly `uncompressed size` bytes. Expansion remains iterative through
a bounded explicit stack; the entropy layer does not change LZD's frame-local
dictionary or freeze rules.

For a raw frame of `F` bytes, at most `ceil(F/2)` tokens are possible, so the
canonical staging bound is `S = 8*ceil(F/2)` bytes. At most `floor(F/2)`
right-present tokens can create phrase entries. The phrase-record count is the
lesser of that value and the configured LZD maximum, and the expansion stack
requires at most that count plus one reference. For entropy block size `E`,
the block-count bound is `ceil(S/E)`. All ceiling divisions, products,
descriptor extents, typed workspace extents, padding, and aggregate sums must
use checked arithmetic before allocation or output.

### Hand-checkable LZD combined raw-block frame

For raw input `A`, LZD emits the eight-byte terminal token
`41 00 00 00 FF FF FF FF`. With entropy block size eight, Blocked Huffman
selects raw representation. The complete 80-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 08 00 00 00  08 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
08 00 00 00 08 00 00 00  00 00 01 08 00 00 00 00
41 00 00 00 FF FF FF FF
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final eight bytes are the unchanged LZD
token. The 16-byte LZD parameter region is stream-level and is not repeated in
the frame.

The reference encoder must complete the deterministic LZD parse using bounded
caller-owned phrase records, serialize its exact tokens into staging, and only
then plan Blocked Huffman. The reference decoder treats entropy output, phrase
records, and the expansion stack as uncommitted state. It validates the entire
token grammar and exact raw extent before checking raw destination capacity and
expanding. A header, entropy, token, reference, workspace, limit, or capacity
failure publishes no bytes from that frame.

The known-size stream uses the ordinary 64-byte version 1.0 header, followed by
the 16-byte LZD parameter region and zero or more combined frames. Empty input
is exactly this 80-byte prefix. Both the LZD dictionary and every Blocked
Huffman model reset at each outer frame. Strict decoding derives exactly
`original size` bytes and rejects trailing serialized data. Incremental paths
must preserve these exact bytes under arbitrary chunking; a frame is exposed
only after complete validation. Nonterminal `Flush` does not shorten a frame,
and `ResetBlock` is unsupported at this profile boundary.

The three-region C ABI exposes only byte extents and alignment while keeping
encoder records, entropy views, phrase records, and expansion references
private. The CLI and benchmark use that public factory without defining another
format variant. Interoperability schema 6 emits and accepts this exact profile
as its seventeenth archive.

## LZMW variant 1 plus Dynamic Range Coder variant 1

The reserved profile name is `lzmw-dynamic-range`. This composition uses
dictionary algorithm ID 6, dictionary variant 1, entropy algorithm ID 3, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZMW parameters followed by the empty Dynamic Range parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZMW phrase dictionary and one
freshly reset adaptive order-0 range model. The LZMW encoder first completes
its canonical sequence of four-byte little-endian references. Dynamic Range
then treats every byte in that finalized sequence as an ordinary symbol and
does not see reference boundaries.

The generic frame header records raw bytes as `uncompressed size`, LZMW
reference bytes as `dictionary serialized size`, Dynamic Range payload bytes
as `compressed payload size`, entropy block count one, descriptor size 16, and
checksum trailer size zero. The body is one Dynamic Range descriptor followed
by its payload; no separate reference region is stored. The descriptor's
`symbol count` equals `dictionary serialized size`, and its `payload size`
equals `compressed payload size`. Dynamic Range normalization, termination,
exact payload exhaustion, model update, rescaling, descriptor, and reset rules
are unchanged from Dynamic Range variant 1.

For raw frame size `F`, the checked reference ceiling is:

```text
S = 4F
```

The conservative Dynamic Range payload ceiling is:

```text
P = 2S + 5 = 8F + 5
```

At most `max(F - 1, 0)` adjacent phrase pairs can create generated entries.
The phrase-record count is the lesser of that value, the configured LZMW
maximum, and the local decoder limit. A nonempty frame's iterative expansion
stack requires at most that phrase count plus one reference. The format-level
raw-frame cap remains 2^20 bytes. The bounded reference profile uses
`F = 65,536`, giving `S = 262,144`, `P = 524,293`, at most 65,535 generated
phrases, and at most 65,536 expansion references.

All ceiling products, frame extents, aligned typed-record extents, expansion-
stack extents, and aggregate workspace sums must be checked before allocation
or mutation. Encoding freezes the deterministic LZMW parse and complete
reference bytes before range planning. Decoding must validate the exact
pipeline and variants, LZMW parameters, sequence, generic extents, one-block
count, 16-byte descriptor extent, reference and payload ceilings, caller-owned
capacities, and aggregate workspace before entropy output. Dynamic Range must
then reconstruct exactly the declared reference-byte count with exact payload
exhaustion. The ordinary LZMW validator requires a multiple of four bytes,
validates every literal or prior generated reference, builds the bounded
adjacent-phrase graph, and derives exactly the declared raw extent. Iterative
reconstruction occurs in private raw staging; only a completely successful
frame may be published.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZMW parameter region and zero or more frames. Empty input is
exactly this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and ordinary input
or output chunking cannot change serialized bytes. The bounded streaming
encoder implements this contract by collecting one raw frame, preparing its
complete serialized representation, and draining that immutable extent before
collecting the next frame. The matching streaming decoder parses and bounds one
complete encoded frame, transactionally reconstructs it into private raw
staging, and drains that immutable raw extent before collecting another frame.

### Hand-checkable single-reference frame

For raw input `A`, standalone LZMW variant 1 emits:

```text
41 00 00 00
```

Independently applying Dynamic Range variant 1 to those four complete bytes
produces:

```text
00 40 FF FF BF 00 00 00
```

The descriptor is:

```text
04 00 00 00 08 00 00 00 00 00 00 00 00 00 00 00
```

The complete 80-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 04 00 00 00  08 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
04 00 00 00 08 00 00 00  00 00 00 00 00 00 00 00
00 40 FF FF BF 00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Dynamic Range descriptor, and the final eight bytes are its payload. The
stream-level LZMW parameter region is not repeated in the frame.

The first combined implementation validates one exact complete frame through
both encoded layers into caller-owned reference staging and phrase records. It
checks generic extents, the `4F` reference ceiling, four-byte reference
alignment, the single 16-byte descriptor, the `2S + 5` payload ceiling, caller
capacities, and aggregate validation workspace before entropy output. Dynamic
Range must exhaust the payload exactly before the ordinary LZMW validator
checks every literal or prior generated reference, constructs the bounded
adjacent-phrase graph, and derives the declared raw extent. It reports the
actual generated-phrase count and corresponding nonempty expansion-stack
ceiling but reconstructs and publishes no raw byte.

The next bounded boundary requires private raw staging and the conservative
maximum expansion stack before entropy output, counts both regions against the
aggregate workspace limit, then reduces the active expansion span to the
actual generated-phrase count plus one after validation and invokes the
ordinary iterative LZMW decoder. Successful raw bytes remain private, and
descriptor, payload, reference, phrase, capacity, aggregate, or reconstruction
failure requires all staging to be discarded.

The transactional complete-frame decoder additionally validates the complete
caller-visible output capacity before entropy output. It performs the same
private reconstruction and copies the exact raw frame once only after all
layers succeed. A short output, malformed header, descriptor or payload,
invalid LZMW graph, limit failure, or unexpected reconstruction failure leaves
caller-visible output unchanged.

The exact-frame planner performs the inverse preparation without writing
serialized output. It plans the deterministic LZMW parse in bounded caller-
owned encoder records, checks the exact reference extent and staging capacity,
serializes all references into that staging, and plans Dynamic Range over
those frozen bytes. It checks encoder records, reference bytes, descriptor,
exact payload, generic header validity, and aggregate workspace, then reports
the complete `56 + 16 + payload_size` frame extent. Repeated planning of the
same input and configuration must return identical references and extents.
The complete-frame encoder validates destination extent before publication,
repeats the exact range plan over the frozen references, and explicitly
serializes the header, descriptor, and payload. This remains an internal frame
API beneath bounded streaming transforms. The C ABI functions
`marc_lzmw_dynamic_range_config_init()`,
`marc_lzmw_dynamic_range_workspace_requirements()`, and
`marc_lzmw_dynamic_range_create()` select exactly this representation and
borrow all reported workspaces for the transform lifetime. Their publication
does not define another representation. The CLI and dependency-free benchmark
use this same public factory and fixed 65,536-byte profile; their file
transaction, capacity, verification, timing, and reporting policy add no
format field or variant. Interoperability schema 19 emits and accepts this
exact profile as archive 30 after the frozen twenty-nine-entry schema-18 order.
The schema adds no stream field or codec variant.

## LZMW variant 1 plus Adaptive Huffman FGK variant 1

The reserved profile name is `lzmw-adaptive-huffman`. This composition uses
dictionary algorithm ID 6, dictionary variant 1, entropy algorithm ID 1, and
entropy variant 1 under format version 1.0. Its stream parameter regions are
the 16-byte LZMW parameters followed by the empty Adaptive Huffman parameter
region. `entropy block size` is zero.

Each nonempty outer frame owns one freshly reset LZMW phrase dictionary and
one freshly reset FGK tree. The LZMW encoder first completes its canonical
sequence of four-byte little-endian references. Adaptive Huffman then treats
every byte in that finalized sequence as an ordinary symbol and does not see
reference boundaries.

The generic frame header records raw bytes as `uncompressed size`, LZMW
reference bytes as `dictionary serialized size`, Adaptive payload bytes as
`compressed payload size`, entropy block count one, descriptor size 16, and
checksum trailer size zero. The body is one Adaptive Huffman descriptor
followed by its payload; no separate reference region is stored. Descriptor,
bit exhaustion, final-valid-bit, padding, update, rescaling, and reset rules
are exactly Adaptive Huffman variant 1.

For raw frame size `F`, the checked reference ceiling is `S = 4F` bytes.
At most `max(F-1, 0)` adjacent phrase pairs can create generated entries; the
phrase-record count is the lesser of that value, the configured LZMW maximum,
and the local decoder limit. A nonempty frame's iterative expansion stack
requires at most that phrase count plus one reference. The conservative
Adaptive payload ceiling is `33S = 132F` bytes. The format-level raw-frame cap
remains 2^20 bytes. The bounded reference profile uses `F = 65,536`, giving
`S = 262,144`, an 8,650,752-byte Adaptive payload ceiling, at most 65,535
generated phrases, and at most 65,536 expansion references.

All products, frame extents, typed-record extents, and aggregate sums must be
checked before allocation or mutation. Encoding fixes the deterministic LZMW
parse and complete reference bytes in caller-owned staging before Adaptive
planning. Decoding first validates the pipeline, parameters, sequence, generic
extents, one-block count, descriptor extent, and all caller capacities.
Adaptive Huffman must reconstruct exactly the declared reference-byte count
with exact payload-bit exhaustion. The ordinary LZMW validator then requires a
multiple of four bytes, validates every literal or prior generated reference,
builds the bounded adjacent-phrase graph, and derives exactly the declared raw
extent. Reconstruction occurs in private raw staging; only a completely
successful frame may be published.

The known-size stream is the ordinary 64-byte version-1.0 header followed by
the 16-byte LZMW parameter region and zero or more frames. Empty input is
exactly this 80-byte prefix. Nonterminal `Flush` does not shorten a frame,
`ResetBlock` is unsupported at this composition boundary, and ordinary input
or output chunking cannot change serialized bytes.

### Hand-checkable single-reference frame

For raw input `A`, LZMW emits reference `41 00 00 00`. Feeding those four
bytes to one fresh FGK tree yields an empty path for `41`, path `0` plus an
eight-bit literal for the first `00`, then paths `01` and `1` for the two
known zero symbols. The complete 20-bit Adaptive payload is `41 00 0C`, with
four valid bits in the final byte. The descriptor is:

```text
04 00 00 00 03 00 00 00 04 00 00 00 00 00 00 00
```

The complete 75-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 04 00 00 00  03 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
04 00 00 00 03 00 00 00  04 00 00 00 00 00 00 00
41 00 0C
```

The first 56 bytes are the generic frame header, the next 16 bytes are the
Adaptive descriptor, and the final three bytes are the FGK payload. The vector
is assembled from the standalone LZMW and Adaptive Huffman encoders plus
generic serializers.

The first combined validator accepts exactly one complete frame and checks all
generic extents, the `4F` reference ceiling, four-byte reference alignment,
single-descriptor shape, `33S` payload ceiling, caller capacities, and aggregate
workspace before entropy output. It parses the descriptor and reconstructs the
complete reference region in private staging, then invokes the ordinary LZMW
validator to check every literal or prior generated reference, construct the
bounded adjacent-phrase graph, and derive exactly the declared raw size. It
reports the actual generated-phrase count and corresponding nonempty expansion-
stack ceiling but reconstructs and publishes no raw byte. This remains an
internal frame API. The next bounded boundary requires private raw staging and
the conservative maximum expansion stack before entropy output, then reduces
the stack span to the actual generated-phrase count plus one after validation
and invokes the ordinary iterative LZMW decoder. Successful raw bytes remain
private, and every error requires all staging to be discarded. The internal
transactional decoder also checks complete caller-visible destination capacity
before entropy output, reconstructs privately, and copies the raw frame only
after all layers succeed. No failure publishes any destination byte. The exact-
frame encoder performs the inverse transaction: it completes the deterministic
LZMW parse and freezes all canonical references before Adaptive planning,
counts typed encoder records, staging, descriptor, and exact payload, then
checks the complete destination before serializing. It reproduces the 75-byte
vector above. The internal bounded streaming encoder emits the 80-byte prefix,
collects exactly the configured outer frames, and drains each complete exact-
frame encoding before accepting another. One-byte I/O, output starvation, and
a retained valid `EndInput` cannot change bytes; `Flush` does not close a
partial frame and `ResetBlock` is rejected. No public profile is admitted by
these frame-level or streaming steps. The internal streaming decoder collects
one complete serialized frame, validates and reconstructs it privately, and
only then drains raw bytes. Every proper truncation and trailing byte is
rejected, and a malformed later frame publishes none of that frame.

The internal bounded profile derives all byte capacities from these same format
ceilings. Its opaque typed region records only total bytes and maximum
alignment; LZMW encoder entries, phrase records, and expansion references are
reconstructed as internal views after their offsets and extent are revalidated.
This storage description does not change the stream representation or admit a
public ABI.

The public C requirements query and factory select this identical fixed profile
and add no format or parameter variant. Encoding remains known-size; decoder
workspace sizing depends only on caller-supplied local limits, and collected
stream parameters are validated against them before any frame publication.
The public completion matrix exercises this representation without defining a
new stream variant or relaxing strict trailing-data rejection.
The CLI and benchmark adapters select the same fixed representation and add no
format variant.
Interoperability schema 13 emits and accepts this exact profile as its twenty-
fourth archive without changing the version-1.0 stream representation.

## LZMW variant 1 plus Blocked Huffman variant 1

This composition uses dictionary algorithm ID 6, dictionary variant 1,
entropy algorithm ID 2, and entropy variant 1. Its stream parameter regions
are the 16-byte LZMW parameters followed by the empty Blocked Huffman parameter
region. The public profile name is `lzmw-blocked-huffman`.

`entropy block size` counts bytes in the canonical fixed-width LZMW reference
stream. Blocks reset at and cannot cross an outer frame; a block boundary need
not coincide with a four-byte token boundary. The generic frame header records
raw bytes as `uncompressed size`, LZMW token bytes as `dictionary serialized
size`, stored entropy bytes as `compressed payload size`, the exact Blocked
Huffman block count, and the complete descriptor/model region size. The body
is:

```text
generic frame header
Blocked Huffman descriptors and models in block order
Blocked Huffman payloads in the same block order
```

No separate LZMW token region is stored. Entropy decoding must produce exactly
`dictionary serialized size` bytes, and that size must be a multiple of four.
Before any raw-byte publication, the ordinary LZMW validator must consume the
complete staged token region, validate every backward phrase reference and
adjacent-phrase production, construct the bounded acyclic phrase grammar, and
derive exactly `uncompressed size` bytes. Expansion remains iterative through
a bounded explicit stack; the entropy layer does not change LZMW's frame-local
dictionary, duplicate-entry numbering, tie rule, or freeze behavior.

For a raw frame of `F` bytes, at most `F` tokens are possible, so the canonical
staging bound is `S = 4F` bytes. At most `max(F-1, 0)` adjacent phrase pairs can
create entries. The phrase-record count is the lesser of that value and the
configured LZMW maximum, and a nonempty expansion stack requires at most that
count plus one reference. For entropy block size `E`, the block-count bound is
`ceil(S/E)`. All ceiling divisions, products, descriptor extents, typed
workspace extents, padding, and aggregate sums must use checked arithmetic
before allocation or output.

### Hand-checkable LZMW combined raw-block frame

For raw input `A`, LZMW emits the four-byte literal reference
`41 00 00 00`. With entropy block size four, Blocked Huffman selects raw
representation. The complete 76-byte frame is:

```text
4D 52 46 31 38 00 00 00  00 00 00 00 00 00 00 00
01 00 00 00 04 00 00 00  04 00 00 00 01 00 00 00
10 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
04 00 00 00 04 00 00 00  00 00 01 08 00 00 00 00
41 00 00 00
```

The first 56 bytes are the generic frame header, the next 16 bytes are one raw
Blocked Huffman descriptor, and the final four bytes are the unchanged LZMW
token. The 16-byte LZMW parameter region is stream-level and is not repeated
in the frame.

The reference encoder must complete the deterministic LZMW parse using bounded
caller-owned phrase-span records, serialize its exact references into staging,
and only then plan Blocked Huffman. The reference decoder treats entropy
output, phrase records, and the expansion stack as uncommitted state. It
validates the entire token grammar and exact raw extent before checking raw
destination capacity and expanding. A header, entropy, token, reference,
workspace, limit, or capacity failure publishes no bytes from that frame.

The known-size stream uses the ordinary 64-byte version 1.0 header, followed by
the 16-byte LZMW parameter region and zero or more combined frames. Empty input
is exactly this 80-byte prefix. Both the LZMW dictionary and every Blocked
Huffman model reset at each outer frame. Strict decoding derives exactly
`original size` bytes and rejects trailing serialized data. Incremental paths
must preserve these exact bytes under arbitrary chunking; a frame is exposed
only after complete validation. Nonterminal `Flush` does not shorten a frame,
and `ResetBlock` is unsupported at this profile boundary.

The three-region C ABI, CLI selector, and benchmark use this representation
without defining another format variant. Interoperability schema 7 emits and
accepts it as the eighteenth archive.

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

## LZ77 variant 1 plus rANS variant 1

The reserved composition name is `lz77-rans`. Format version 1.0 uses the
ordinary 16-byte LZ77 variant-1 parameter extension, no entropy-parameter
extension, and a nonzero stream entropy block size `B`. Both the LZ77 window
and every rANS model reset at each outer frame.

For a raw frame of `F` bytes, LZ77 first emits its complete canonical 16-byte
token sequence. Let its exact byte size be `S`; require `S` to be a multiple of
16 and enforce the checked bound `S <= 16F`. rANS consumes this sequence as
untyped bytes. It divides the sequence into `K = ceil(S/B)` consecutive
blocks, where the final block may be short. A block boundary may occur inside
an LZ77 token because dictionary parsing occurs only after the complete byte
sequence is reconstructed. No rANS block crosses an outer frame.

The generic frame fields are:

- `uncompressed size = F`;
- `dictionary serialized size = S`;
- `entropy block count = K`;
- `block descriptors size = 528K`;
- `compressed payload size = P`, the checked sum of all descriptor payload
  sizes.

Each block uses the exact rANS descriptor, normalization, payload, and strict
terminal-state rules above. Since variant 1 emits at most one renormalization
byte per input symbol, require `P <= S + 8K`. The format retains the LZ77 raw
frame cap `F <= 2^20`. All products and sums are checked before allocation or
entropy decoding.

Decoding validates the stream profile and LZ77 parameters, generic frame
header and complete extent, exact block count, descriptor region, every rANS
model and state path, and exact aggregate payload exhaustion in that order.
Only then may it reconstruct exactly `S` bytes into private token staging.
It next requires 16-byte alignment and validates the complete LZ77 token
stream, distances, overlap-copy semantics, and exact declared raw extent
before private reconstruction and any caller-visible publication.

The first combined validator implements exactly this decoder-facing boundary.
Before entropy work it additionally admits caller-owned token staging and
`K * sizeof(RansBlockView)` bounded view storage, and counts both with the
descriptor and payload regions against `max_internal_buffered_bytes`. It
validates all `K` rANS payloads before decoding any of them, so a malformed
later block cannot leave an earlier token prefix in staging. After a successful
second pass reconstructs exactly `S` token bytes, LZ77 validation runs without
raw reconstruction. Callers discard token and view storage on every failure.

The bounded private decoder extends that admission with exactly `F` bytes of
separate raw staging and includes those bytes in the same aggregate workspace
check before entropy processing. After all rANS and LZ77 validation succeeds,
it invokes the ordinary LZ77 decoder over the immutable token staging,
including forward overlap-copy semantics, and writes exactly `F` private raw
bytes. No caller-visible output span exists. A malformed entropy block or
token stream leaves raw staging unchanged, and callers discard all private
workspace on any reported failure.

The transactional complete-frame decoder additionally requires a distinct
caller output span of at least `F` bytes during the same preflight. It performs
the unchanged private decode, then copies exactly `F` bytes from raw staging to
output once. Short output, malformed descriptors or rANS state, invalid LZ77
tokens, and reconstruction failure publish no caller-visible byte. Output
capacity is not counted as internal workspace because ownership remains with
the caller and it is the defined publication destination.

The encoder-side exact-frame planner accepts one nonempty raw frame and
caller-owned token staging but no serialized destination. It first computes
the exact LZ77 token extent `S`, admits staging for all `S` bytes, and writes
the complete canonical token sequence once. It then plans each consecutive
full or final-short rANS block over those immutable bytes and accumulates
exact `K`, `528K`, and `P` values with checked arithmetic. Descriptor bytes,
planned payload bytes, and token staging are counted against
`max_internal_buffered_bytes`; caller raw input is not.

The planner validates the resulting generic frame header and returns the exact
serialized extent `56 + 528K + P`. It writes no frame header, descriptor, or
payload byte. Short token staging is rejected before staging mutation. On any
later planning failure the caller discards private staging; no serialized
output exists to expose a partial frame.

The deterministic complete-frame encoder first runs that exact planner and
requires a distinct serialized destination of at least
`56 + 528K + P` bytes. Capacity failure occurs before any serialized byte is
written. After admission it writes the generic frame header, serializes each
block descriptor into the contiguous descriptor region, and encodes each
corresponding rANS payload into the contiguous payload region. Every repeated
block plan and encoded payload extent must equal the values admitted by the
first pass; disagreement is an internal error.

For identical input, parameters, limits, sequence, and committed-output
context, repeated calls emit identical bytes. The one-Literal result is
exactly the independent 592-byte vector below. With `B = 5`, its 16 token
bytes produce four descriptors followed by four payloads, including a final
one-symbol block, and decode back to the same raw byte.

The bounded streaming encoder emits the ordinary 64-byte stream header and
16-byte LZ77 parameter extension, then processes a declared known-size input
as consecutive outer frames. It collects at most `F` raw bytes, freezes at
most `16F` canonical token bytes, invokes the exact complete-frame encoder,
and retains that completed frame unchanged until every byte has drained.
Input/output chunking alone therefore cannot change the stream.

`Flush` does not close a partial raw frame. `EndInput` is valid only when the
same call supplies every remaining declared raw byte, and remains effective
while the prefix or final frame drains. `ResetBlock` and unknown flags are
rejected. Empty known-size input emits only the stream prefix and then ends.
The aggregate steady-state bound counts raw collection, exact token staging,
and exact serialized-frame storage before encoding each frame.

For raw `A`, LZ77 emits:

```text
00 00 00 00 00 00 00 00 00 00 00 00 41 00 00 00
```

With `B = 65,536`, this is one rANS block. Its only nonzero normalized
frequencies are symbol `00:3840` and symbol `41:256`; its payload is:

```text
00 A5 22 10 15 00 00 00
```

The complete frame is 592 bytes. Its generic header is:

```text
4D 52 46 31 38 00 00 00 00 00 00 00 00 00 00 00
01 00 00 00 10 00 00 00 08 00 00 00 01 00 00 00
10 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00
00 00 00 00 00 00 00 00
```

Descriptor bytes 0 through 15 are
`10 00 00 00 08 00 00 00 0C 00 00 00 00 00 00 00`.
The 512-byte frequency region is zero except descriptor offsets 16..17
(`00 0F`) and 146..147 (`00 01`). The eight payload bytes above immediately
follow the descriptor. This sparse notation uniquely defines all 592 bytes
without listing the remaining 254 zero-frequency entries.

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
