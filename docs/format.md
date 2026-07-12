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
| 1 | LZ77 | baseline, details pending |
| 2 | LZSS | baseline, details pending |
| 3 | LZ78 | baseline, details pending |
| 4 | LZW | baseline, details pending |
| 5 | LZD | Lempel-Ziv Double baseline, details pending |
| 6 | LZMW | baseline, details pending |

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
