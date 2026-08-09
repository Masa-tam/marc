# Command-line tool

Top-level builds produce a small `marc` executable that exercises the public
[C ABI](c-api.md) with bounded streaming buffers. It is a codec and format
validation tool, not an archive manager: file metadata, directory traversal,
and multi-file containers are outside its scope.

## Usage

LZ77 variant 1 with no entropy layer is the default profile:

```console
marc encode input.bin output.marc
marc decode output.marc restored.bin
```

Select any other profile explicitly, and use the same selection for decoding:

```console
marc encode --codec blocked-huffman input.bin output.marc
marc decode --codec blocked-huffman output.marc restored.bin
```

An explicit `--codec lz77` is equivalent to omitting `--codec`.

## Profiles

### Profile inventory

| CLI name | Dictionary | Entropy | Notes |
|---|---|---|---|
| `checksum-raw` | None | None | Version 1.1 raw framing with mandatory per-frame CRC-32C |
| `blocked-huffman` | None | Blocked Huffman | Independently rebuilt canonical model per block |
| `adaptive-huffman` | None | Adaptive Huffman | FGK variant |
| `dynamic-range` | None | Dynamic Range | Adaptive order-0 byte model |
| `rans` | None | rANS | Scalar, block-based, byte-renormalized variant |
| `tans` | None | tANS | Table-based, block-buffered variant |
| `lz77` | LZ77 | None | Default profile |
| `lz77-blocked-huffman` | LZ77 | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lz77-adaptive-huffman` | LZ77 | Adaptive Huffman | FGK tree reset per outer frame |
| `lz77-dynamic-range` | LZ77 | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lz77-rans` | LZ77 | rANS | Scalar rANS model rebuilt per entropy block |
| `lz77-tans` | LZ77 | tANS | Tabled model rebuilt per entropy block |
| `lzss` | LZSS | None | Variant 1 |
| `lzss-blocked-huffman` | LZSS | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzss-adaptive-huffman` | LZSS | Adaptive Huffman | FGK tree reset per outer frame |
| `lzss-dynamic-range` | LZSS | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lzss-rans` | LZSS | rANS | Scalar rANS model rebuilt per entropy block |
| `lzss-tans` | LZSS | tANS | Tabled model rebuilt per entropy block |
| `lz78` | LZ78 | None | Variant 1 |
| `lz78-blocked-huffman` | LZ78 | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lz78-adaptive-huffman` | LZ78 | Adaptive Huffman | FGK tree reset per outer frame |
| `lz78-dynamic-range` | LZ78 | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lz78-rans` | LZ78 | rANS | Scalar rANS model rebuilt per entropy block |
| `lz78-tans` | LZ78 | tANS | Tabled model rebuilt per entropy block |
| `lzw` | LZW | None | Variant 1 |
| `lzw-blocked-huffman` | LZW | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzw-adaptive-huffman` | LZW | Adaptive Huffman | FGK tree reset per outer frame |
| `lzw-dynamic-range` | LZW | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lzw-rans` | LZW | rANS | Scalar rANS model rebuilt per entropy block |
| `lzw-tans` | LZW | tANS | Tabled model rebuilt per entropy block |
| `lzd` | Lempel-Ziv Double | None | Variant 1 |
| `lzd-blocked-huffman` | Lempel-Ziv Double | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzd-adaptive-huffman` | Lempel-Ziv Double | Adaptive Huffman | FGK tree reset per outer frame |
| `lzd-dynamic-range` | Lempel-Ziv Double | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lzd-rans` | Lempel-Ziv Double | rANS | Scalar rANS model rebuilt per entropy block |
| `lzd-tans` | Lempel-Ziv Double | tANS | Tabled model rebuilt per entropy block |
| `lzmw` | LZMW | None | Variant 1 |
| `lzmw-blocked-huffman` | LZMW | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzmw-adaptive-huffman` | LZMW | Adaptive Huffman | FGK tree reset per outer frame |
| `lzmw-dynamic-range` | LZMW | Dynamic Range | Adaptive order-0 model reset per outer frame |
| `lzmw-rans` | LZMW | rANS | Scalar rANS model rebuilt per entropy block |
| `lzmw-tans` | LZMW | tANS | Tabled model rebuilt per entropy block |

### Experimental profile

`lzss-contextual-dynamic-range` selects the Format 2 LZSS typed-token,
field-context, contextual Dynamic Range pipeline. It is intentionally outside
the stable 42-profile Format 1 inventory above. `lzss-contextual-rans` selects
the corresponding Format 2 typed-token and field-context pipeline with the
fixed-descriptor scalar contextual rANS variant 2. It is a diagnostic, not a
recommended compression profile: every frame carries 9,052 model bytes.
`lzss-contextual-rans-compact` selects the same contexts and scalar coder with
the canonical variable-size descriptor of variant 3. Encode and decode must
use the same explicit selector; neither name auto-detects the other variant.

### Common stream rules

Except for `checksum-raw` and the explicitly experimental Format 2 selectors,
the profiles above use the current version 1 stream representation described
in the [format specification](format.md).

### LZ77 profile parameters

The `lz77-adaptive-huffman` adapter uses 65,536-byte raw frames, at most
1,048,576 canonical LZ77 token bytes, and the conservative 33-byte-per-token
Adaptive payload bound. Its aggregate policy remains below 64 MiB and all
actual workspace extents come from the public C ABI requirements query.

The `lz77-dynamic-range` adapter uses 65,536-byte raw frames, at most 1,048,576
canonical LZ77 token bytes, and the conservative `2S + 5` Dynamic Range payload
bound of 2,097,157 bytes. Its complete-frame aggregate policy is 3,211,341
bytes. Both direction-specific workspace extents come from the public C ABI
requirements query; the CLI does not reproduce the private partition.

The `lz77-rans` adapter uses 65,536-byte raw frames and 65,536-byte entropy
blocks. Its 1,048,576-byte canonical LZ77 token ceiling produces at most 16
rANS blocks, 8,448 descriptor bytes, and a 1,048,704-byte payload. The
complete encoder-side simultaneous-workspace policy is 2,171,320 bytes.
All direction-specific workspace extents and the opaque decoder-view
alignment come from the public C ABI requirements query.

The `lz77-tans` adapter also fixes raw frames and entropy blocks at 65,536
bytes. Its 1,048,576-byte canonical token ceiling produces at most sixteen
tANS blocks, 8,448 descriptor bytes, and a 1,572,896-byte payload. The complete
encoder-side simultaneous-workspace policy is 2,695,512 bytes. All actual
workspace extents and opaque decoder-view alignment come from the public C ABI
requirements query.

### LZSS profile parameters

The `lzss-adaptive-huffman` adapter likewise uses 65,536-byte raw frames. Its
exact LZSS worst case is 131,072 canonical token bytes and its conservative
Adaptive payload limit is 4,325,376 bytes. The CLI supplies these hard limits
but obtains both direction-specific workspace extents from the public C ABI
query; it does not reproduce the private token/frame partition.

The `lzss-dynamic-range` adapter uses the same 65,536-byte raw frames and
131,072-byte canonical LZSS token ceiling. Its conservative `2S + 5` Dynamic
Range payload bound is 262,149 bytes and its complete simultaneous-workspace
policy is 458,829 bytes. Both direction-specific workspace extents come from
the public C ABI requirements query; the CLI does not reproduce the private
partition.

The `lzss-rans` adapter fixes raw frames and entropy blocks at 65,536 bytes.
Its 131,072-byte canonical LZSS token ceiling produces at most two rANS blocks,
1,056 descriptor bytes, and a 131,088-byte payload. The exact encoder
aggregate is 328,808 bytes; the shared configuration uses a conservative
512-KiB internal-buffer policy so the decoder's opaque views also fit without
exposing their C++ layout. Every direction-specific extent and alignment comes
from the public C ABI requirements query.

The `lzss-tans` adapter also fixes raw frames and entropy blocks at 65,536
bytes. Its 131,072-byte canonical LZSS token ceiling produces at most two
tANS blocks, 1,056 descriptor bytes, and a 196,612-byte payload. The exact
encoder aggregate is 394,332 bytes; the shared configuration uses a
conservative 512-KiB internal-buffer policy so decoder views remain opaque.
Every directional extent and alignment comes from the public C ABI
requirements query.

The experimental `lzss-contextual-dynamic-range` adapter fixes raw frames and
context blocks at 65,536 bytes. Its public Format 2 conservative payload bound
is `12F + 5`, or 786,437 bytes, and its CLI internal-buffer policy is 8 MiB.
The command-line layer calls only the public configuration initializer,
direction-specific requirements query, factory, process, and destroy
functions. All three workspace extents and the opaque views alignment come
from the query; no private token, operation, or model layout is reproduced.

The experimental `lzss-contextual-rans` and
`lzss-contextual-rans-compact` adapters also fix raw frames at 65,536 bytes.
Their public Format 2 decision ceiling is `6F = 393,216`, their payload ceiling
is `12F + 8 = 786,440` bytes, and each internal-buffer policy is 8 MiB. The
fixed selector emits entropy variant 2 and the compact selector emits variant
3. Each adapter calls only its distinct public configuration initializer,
direction-specific requirements query, factory, process, and destroy
functions. All byte regions and the opaque fixed-table/token views alignment
come from the query; the command-line layer reproduces no private layout.

### LZ78 profile parameters

The `lz78-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the eight-byte-per-raw-byte LZ78 token bound, at most 128
entropy blocks per frame, and at most 65,536 phrase entries. All actual
workspace extents and alignment still come from the public C ABI requirements
query; the command-line layer does not reproduce the private typed layout.

The `lz78-adaptive-huffman` adapter uses 65,536-byte raw frames, at most
524,288 canonical LZ78 token bytes, and the conservative 17,301,504-byte
Adaptive payload bound. It permits at most 65,536 phrase entries and keeps the
aggregate policy at 32 MiB. Both direction-specific byte extents and the
opaque typed-view alignment come from the public C ABI requirements query.

The `lz78-dynamic-range` adapter uses 65,536-byte raw frames, at most 524,288
canonical LZ78 token bytes, and the conservative `2S + 5` Dynamic Range payload
bound of 1,048,581 bytes. It admits at most 65,536 phrase entries and applies a
4-MiB aggregate policy. All three direction-specific workspace extents and the
opaque typed-view alignment come from the public C ABI requirements query; the
CLI does not name or size private LZ78 records.

The `lz78-rans` adapter fixes raw frames and entropy blocks at 65,536 bytes.
Its 524,288-byte canonical LZ78 token ceiling produces at most eight rANS
blocks, 4,224 descriptor bytes, and a 524,352-byte payload. It permits at most
65,536 phrase entries and uses a conservative 4-MiB aggregate policy. Every
direction-specific workspace extent and opaque alignment comes from the public
C ABI requirements query; the CLI does not reproduce private record layouts.

The `lz78-tans` adapter also fixes raw frames and entropy blocks at 65,536
bytes. Its 524,288-byte canonical LZ78 token ceiling produces at most eight
tANS blocks, 4,224 descriptor bytes, and a 786,448-byte payload. It permits at
most 65,536 phrase entries and uses a conservative 4-MiB aggregate policy.
Every direction-specific workspace extent and opaque alignment comes from the
public C ABI requirements query; the CLI does not reproduce private tANS-view
or LZ78 record layouts.

### LZW profile parameters

The `lzw-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the two-byte-per-raw-byte packed LZW bound, at most 32 entropy
blocks per frame, and at most 65,280 additional LZW dictionary entries. Its
actual three workspace extents and alignment likewise come from the public C
ABI requirements query.

The `lzw-adaptive-huffman` adapter uses 65,536-byte raw frames and maximum
code width 16. Its packed LZW ceiling is 131,072 bytes, its conservative
Adaptive payload ceiling is 4,325,376 bytes, and its aggregate internal limit
is 8 MiB. At most 65,280 generated entries are admitted. The public C ABI
requirements query remains authoritative for all three workspace extents and
the opaque typed-record alignment.

The `lzw-dynamic-range` adapter uses 65,536-byte raw frames and maximum code
width 16. Its packed LZW ceiling is 131,072 bytes, its conservative `2S + 5`
Dynamic Range payload ceiling is 262,149 bytes, and its aggregate internal
limit is 8 MiB. At most 65,280 generated entries are admitted. The public C ABI
requirements query remains authoritative for all three workspace extents and
the opaque typed-record alignment.

The `lzw-rans` adapter uses 65,536-byte raw frames and entropy blocks with
maximum LZW code width 16. Its packed-code ceiling is 131,072 bytes, producing
at most two rANS blocks, 1,056 descriptor bytes, and a 131,088-byte payload.
At most 65,280 generated entries are admitted under a conservative 8-MiB
aggregate policy. Every direction-specific byte extent and opaque alignment
comes from the public C requirements query; the CLI does not reproduce private
LZW entry, phrase, or rANS-view layouts.

The `lzw-tans` adapter also uses 65,536-byte raw frames and entropy blocks with
maximum LZW code width 16. Its packed-code ceiling is 131,072 bytes, producing
at most two tANS blocks, 1,056 descriptor bytes, and a 196,612-byte payload.
At most 65,280 generated entries are admitted under a conservative 8-MiB
aggregate policy. Every direction-specific byte extent and opaque alignment
comes from the public C requirements query; the CLI does not reproduce private
LZW entry, phrase, or tANS-view layouts.

### LZD profile parameters

The `lzd-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the exact four-byte-per-raw-byte worst-case LZD token bound,
at most 64 entropy blocks per frame, and at most 65,536 phrase entries. The
public requirements query supplies all three workspace extents and alignment;
the CLI does not reproduce the private entropy-view, phrase, or expansion-stack
layout. The aggregate internal-buffer limit remains 64 MiB.

The `lzd-adaptive-huffman` adapter uses 65,536-byte raw frames, at most 262,144
canonical LZD token bytes, and the conservative 8,650,752-byte Adaptive payload
ceiling. It admits at most 65,536 dictionary entries and uses a 16-MiB aggregate
internal limit. The public C ABI requirements query supplies the exact primary,
secondary, and opaque aligned-view extents for each direction; the CLI does not
reproduce private encoder-entry, phrase, or expansion-stack layouts.

The `lzd-dynamic-range` adapter uses 65,536-byte raw frames, at most 262,144
canonical LZD token bytes, and the conservative `2S + 5` Dynamic Range payload
ceiling of 524,293 bytes. It admits at most 65,536 dictionary entries and uses
a 16-MiB aggregate internal limit. The public C ABI requirements query supplies
the exact primary, secondary, and opaque aligned-view extents for each
direction; the CLI does not reproduce private encoder-entry, phrase, or
expansion-stack layouts.

The `lzd-rans` adapter uses 65,536-byte raw frames and entropy blocks. Its
262,144-byte canonical LZD token ceiling produces at most four rANS blocks,
2,112 descriptor bytes, and a 262,176-byte payload. It admits at most 65,536
configured dictionary entries under a conservative 16-MiB aggregate policy.
Every direction-specific workspace extent and opaque alignment comes from the
public C requirements query; the CLI does not reproduce private rANS-view,
encoder-entry, phrase, or expansion-stack layouts.

The `lzd-tans` adapter uses the same 65,536-byte raw-frame and entropy-block
policy. Its 262,144-byte canonical LZD token ceiling produces at most four
tANS blocks, 2,112 descriptor bytes, and a 393,224-byte payload. It retains the
public maximum-entry default and a conservative 16-MiB aggregate policy. Every
direction-specific extent and opaque alignment comes from the public
requirements query; the CLI does not reproduce tANS-view, encoder-entry,
phrase, expansion-stack, or partition layouts.

### LZMW profile parameters

The `lzmw-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the exact four-byte-per-raw-byte fixed-reference bound, at most
64 entropy blocks per frame, and at most 65,536 generated phrase entries. The
public requirements query supplies all byte extents and the opaque views
alignment; the CLI does not reproduce entropy-view, phrase-record, or iterative
expansion-stack layouts. The aggregate internal-buffer limit remains 64 MiB.

The `lzmw-adaptive-huffman` adapter uses 65,536-byte raw frames, at most
262,144 canonical LZMW reference bytes, and the conservative 8,650,752-byte
Adaptive payload ceiling. It admits at most 65,536 generated entries and uses
a 16-MiB aggregate internal limit. The public C ABI requirements query supplies
the exact primary, secondary, and opaque aligned-view extents for each
direction; the CLI does not reproduce private encoder-entry, phrase, or
expansion-stack layouts.

The `lzmw-dynamic-range` adapter uses 65,536-byte raw frames, at most 262,144
canonical LZMW reference bytes, and the conservative `2S + 5` Dynamic Range
payload ceiling of 524,293 bytes. It admits at most 65,536 generated entries
and uses a 16-MiB aggregate internal limit. The public C ABI requirements query
supplies the exact primary, secondary, and opaque aligned-view extents for each
direction; the CLI does not reproduce private encoder-entry, phrase, or
expansion-stack layouts.

The `lzmw-rans` adapter uses 65,536-byte raw frames and entropy blocks. Its
262,144-byte canonical LZMW reference ceiling produces at most four rANS
blocks, 2,112 descriptor bytes, and a 262,176-byte payload. It admits at most
65,536 configured dictionary entries under a conservative 16-MiB aggregate
policy. Every direction-specific workspace extent and opaque alignment comes
from the public C requirements query; the CLI does not reproduce private
rANS-view, encoder-entry, phrase, or expansion-stack layouts.

The `lzmw-tans` adapter uses 65,536-byte raw frames and entropy blocks. Its
262,144-byte canonical LZMW reference ceiling produces at most four tANS
blocks, 2,112 descriptor bytes, and a 393,224-byte payload. It admits at most
65,536 configured dictionary entries under a conservative 16-MiB aggregate
policy. Every direction-specific workspace extent and opaque alignment comes
from the public C requirements query; the CLI does not reproduce private
tANS-view, encoder-entry, phrase, or expansion-stack layouts.

## File and error behavior

The destination and its `.tmp` staging path must not already exist. A
successful operation renames the staging file to the destination. A failed
operation removes the staging file, so malformed input does not leave a
partially decoded destination.

The process returns `0` on success, `1` when the requested operation fails, and
`2` for invalid command-line usage or an unknown profile name.
