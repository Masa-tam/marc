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
field-context, contextual Dynamic Range pipeline with the frozen 64 KiB
dictionary/context identity `2/2 + 1/1`.
`lzss-contextual-dynamic-range-1m` selects its additive 1 MiB identity
`2/3 + 1/2`. `lzss-contextual-dynamic-range-4m` selects exact identity
`2/4 + 1/3`, and `lzss-contextual-dynamic-range-16m` selects exact identity
`2/5 + 1/4`. `lzss-contextual-dynamic-range-64m` selects exact identity
`2/6 + 1/5`. Decode requires the same selector used for encode; none of these
names auto-detects or admits another profile. All are intentionally outside
the stable 42-profile Format 1 inventory above. `lzss-contextual-rans` selects
the corresponding Format 2 typed-token and field-context pipeline with the
canonical variable-size descriptor of scalar contextual rANS variant 3.
Entropy variant 2 is retired and reserved; no diagnostic selector or alias
remains. `lzss-contextual-rans-1m` selects the additive `2/3 + 1/2` window
profile. `lzss-contextual-rans-4m` selects exact identity
`2/4 + 1/3 + 4/3`, and `lzss-contextual-rans-16m` selects exact identity
`2/5 + 1/4 + 4/3`. `lzss-contextual-rans-64m` selects exact identity
`2/6 + 1/5 + 4/3`. Encode and decode use the same explicit selector; no rANS
name auto-detects or admits another profile.
`lzss-contextual-tans` selects the same typed LZSS contexts with contextual
tANS entropy variant 2 under the frozen 64 KiB identity. The additive
`lzss-contextual-tans-1m` name selects `2/3 + 1/2 + 5/2`, and
`lzss-contextual-tans-4m` selects exact identity `2/4 + 1/3 + 5/2`.
`lzss-contextual-tans-16m` selects exact identity `2/5 + 1/4 + 5/2`, and
`lzss-contextual-tans-64m` selects exact identity `2/6 + 1/5 + 5/2`. Encode
and decode require the same explicit name; no profile auto-detects or admits
another. All five remain outside the stable 42-profile inventory.
`lzss-contextual-blocked-huffman` selects typed LZSS plus the selective
Contextual Blocked Huffman entropy variant 2 under the frozen 64 KiB profile.
`lzss-contextual-blocked-huffman-1m` selects exact
`2/3 + 1/2 + 2/2`, and `lzss-contextual-blocked-huffman-4m` selects exact
`2/4 + 1/3 + 2/2`. `lzss-contextual-blocked-huffman-16m` selects exact
`2/5 + 1/4 + 2/2`. Encode and decode require the same explicit name; no
profile auto-detects or admits another. All four remain experimental.
`lzss-contextual-adaptive-huffman` selects typed LZSS plus Contextual Adaptive
Huffman entropy variant 2 under the frozen 64 KiB profile. The additive
`lzss-contextual-adaptive-huffman-1m` name selects exact
`2/3 + 1/2 + 1/2`, and `lzss-contextual-adaptive-huffman-4m` selects exact
`2/4 + 1/3 + 1/2`. `lzss-contextual-adaptive-huffman-16m` selects exact
`2/5 + 1/4 + 1/2`. Encode and decode require the same explicit name; no
profile auto-detects or admits another. All four remain outside the stable
42-profile inventory.

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

The experimental `lzss-contextual-dynamic-range-1m` adapter uses 1,048,576-
byte raw frames and a 1,048,576-byte LZSS window. Its conservative payload
bound is `12F + 5`, or 12,582,917 bytes, and its aggregate internal-buffer
policy remains the public 128 MiB default. The adapter changes only public C
configuration values before using the same requirements, factory, process,
and destroy lifecycle. Queried workspace extents remain authoritative.

The experimental `lzss-contextual-dynamic-range-4m` adapter uses 4,194,304-
byte raw frames and window, the `14F + 5` payload ceiling of 58,720,261 bytes,
and an explicit 256-MiB aggregate hard limit. This does not change the
library's 128-MiB default. The CLI does not reproduce the native workspace
partition: its public C requirements query supplies all three extents and the
opaque views alignment. A native layout exceeding the application ceiling is
rejected rather than admitted by an implicit limit increase.

The experimental `lzss-contextual-dynamic-range-16m` adapter uses
16,777,216-byte raw frames and window, the `14F + 5` payload ceiling of
234,881,029 bytes, 4,582 model entries, and the public helper's explicit
one-GiB aggregate policy. The CLI selects that helper and then relies on the
direction-specific public workspace query; it does not reproduce private
token, operation, model, or HashChain extents. Encoder workspace scales with
known input, while the decoder conservatively reserves its 452,984,917-byte
full-profile requirement before inspecting an untrusted stream. Decode
requires the same explicit `-16m` name, and profile mismatch remains
transactional.

The experimental `lzss-contextual-dynamic-range-64m` adapter uses the public
profile-value-4 helper: 67,108,864-byte raw frames and window, the `16F + 5`
payload ceiling of 1,073,741,829 bytes, 4,598 model entries, and the explicit
eight-GiB aggregate policy. Directional workspace sizes and alignment come
only from the public query. HashChain encoding requires at most 4,362,600,533
bytes for a full frame, while decoding conservatively reserves
1,946,157,141 bytes before inspecting an untrusted stream. Decode requires the
same explicit `-64m` name. A separate bounded decoder-fuzz harness admits the
same public profile under fixed one-KiB frame storage. Interoperability schema
53 appends this exact name as archive 63 without changing any earlier archive
byte or order.

The experimental `lzss-contextual-rans` adapter also fixes raw frames at
65,536 bytes. Its public Format 2 decision ceiling is `6F = 393,216`, its
payload ceiling is `12F + 8 = 786,440` bytes, and its internal-buffer policy
is 8 MiB. It emits entropy variant 3 and calls only its public initializer,
direction-specific requirements query, factory, process, and destroy
functions. All byte regions and the opaque model-table/token views alignment
come from the query; the command-line layer reproduces no private layout.

The experimental `lzss-contextual-rans-1m` adapter fixes raw frames and the
LZSS window at 1,048,576 bytes. Its public decision ceiling is
`6F = 6,291,456`, its payload ceiling is `12F + 8 = 12,582,920` bytes, and
its internal-buffer policy is 128 MiB. It changes only the public window
profile and bounded configuration values before using the same initializer,
requirements, factory, process, and destroy lifecycle. The queried byte
regions and opaque views alignment remain authoritative.

The experimental `lzss-contextual-rans-4m` adapter fixes raw frames and the
LZSS window at 4,194,304 bytes. Context variant 3 requires the distinct
`7F = 29,360,128` decision/block ceiling and its scalar rANS payload ceiling
is `14F + 8 = 58,720,264` bytes. The application retains the public 128-MiB
aggregate policy: the supported 64-bit encoder and decoder requirements fit
without an implicit increase. It selects public window profile value 2 and
otherwise uses the same initializer, requirements query, factory, process,
and destroy lifecycle. Queried workspace extents and alignment remain
authoritative.

The experimental `lzss-contextual-rans-16m` adapter fixes raw frames and the
LZSS window at 16,777,216 bytes. Its decision/block ceiling is
`7F = 117,440,512`, its scalar rANS payload ceiling is
`14F + 8 = 234,881,032` bytes, and its aggregate policy is 512 MiB. The CLI
selects public profile value 3 and obtains every direction-specific byte
extent and opaque alignment from the public workspace query. It duplicates no
private token, table, HashChain, or frame-layout arithmetic. Decode requires
the same `-16m` name; mismatch, malformed input, and trailing data retain no
destination or temporary output.

The matching dependency-free benchmark selector uses this same public profile
helper and direction-specific workspace query. Its checked output allocation
is `112 + 14N + 9,225K` for input size `N` and nonempty frame count `K`. It
verifies a byte-exact round trip before measuring either direction, reports
all six workspace regions and their directional maximum, and does not include
the external Silesia corpus in the default test suite.

The experimental `lzss-contextual-rans-64m` adapter fixes raw frames and the
LZSS window at 67,108,864 bytes. Its decision/block ceiling is
`8F = 536,870,912`, its scalar rANS payload ceiling is
`16F + 8 = 1,073,741,832` bytes, and its aggregate policy is four GiB. The CLI
selects public profile value 4, obtains every direction-specific extent from
the public workspace query, and duplicates no private descriptor, table,
finder, or frame arithmetic. Decode requires the same `-64m` name; crossed
profiles, malformed input, and trailing data retain no output.

The matching benchmark uses the same public lifecycle. Its checked output
allocation is `112 + 16N + 9,257K` for input size `N` and nonempty frame count
`K`; a byte-exact round trip precedes timing and all workspace reporting.
Interoperability schema 54 appends this exact CLI profile as archive 64 after
the frozen 63-entry schema-53 order without changing earlier bytes or order.

The experimental `lzss-contextual-tans` adapter uses 65,536-byte raw frames,
a `6F = 393,216` decision ceiling, and a `9F + 2 = 589,826` payload ceiling.
Its internal-buffer policy is 8 MiB. Encode and decode call only the public
contextual-tANS configuration initializer, direction-specific requirements
query, factory, process, and destroy functions. The queried opaque views own
all typed tokens and tANS tables; the command-line layer neither names nor
sizes those private layouts.

The experimental `lzss-contextual-tans-1m` adapter uses 1,048,576-byte raw
frames and LZSS window. Its decision ceiling is `6F = 6,291,456`, its payload
ceiling is `9F + 2 = 9,437,186` bytes, and its internal-buffer policy is
128 MiB. It changes only public profile and bounded configuration values
before using the same public lifecycle; queried workspace extents remain
authoritative.

The experimental `lzss-contextual-tans-4m` adapter fixes raw frames and the
LZSS window at 4,194,304 bytes. Context variant 3 requires the distinct
`7F = 29,360,128` decision/block ceiling and the tANS payload ceiling is
`ceil(21F/2) + 2 = 44,040,194` bytes. The application retains the public
128-MiB aggregate policy: supported 64-bit encoder and decoder requirements
fit without an implicit increase. It selects public window profile value 2
and otherwise uses the same initializer, requirements query, factory, process,
and destroy lifecycle. Queried workspace extents and alignment remain
authoritative.

The experimental `lzss-contextual-tans-16m` adapter fixes raw frames, window,
and LZ distance at 16,777,216 bytes. It admits `7F = 117,440,512` decisions,
uses the `ceil(21F/2) + 2 = 176,160,770` payload ceiling, and selects the
512-MiB aggregate policy. Both directions apply public profile value 3 and
obtain every allocation extent and opaque alignment from the public workspace
query. Decode requires the same `-16m` name; crossed-profile, malformed, and
trailing input retain no destination or temporary output.

The experimental `lzss-contextual-tans-64m` adapter fixes raw frames, window,
and maximum distance at 67,108,864 bytes. Public profile value 4 supplies the
`8F = 536,870,912` decision/block ceiling, 805,306,370-byte payload ceiling,
fixed 131,072-entry table bank, and four-GiB aggregate policy. Both directions
obtain every storage extent and opaque alignment from the public workspace
query after applying that profile; the CLI duplicates no private descriptor,
table, finder, frame, or allocation arithmetic. Decode requires the exact
`-64m` name and rejects crossed profiles before retaining output.

The experimental `lzss-contextual-blocked-huffman` adapter uses 65,536-byte
raw frames, a `6F = 393,216` decision ceiling, and a
`12F = 786,432` payload ceiling. Its descriptor is bounded at 2,561 bytes and
its aggregate policy is 8 MiB. Both directions call only the public
configuration initializer, requirements query, factory, process, and destroy
functions. Typed tokens and Huffman tables remain in the queried opaque views;
the CLI neither names nor sizes those private layouts.

The experimental `lzss-contextual-blocked-huffman-1m` adapter uses
1,048,576-byte raw frames and LZSS window, a `6F = 6,291,456` decision
ceiling, and a conservative `12F = 12,582,912` payload ceiling. Its descriptor
is bounded at 2,579 bytes and its aggregate policy is 128 MiB. It changes only
the public exact profile and bounded configuration values before using the
same public lifecycle; queried workspace extents and alignment remain
authoritative.

The experimental `lzss-contextual-blocked-huffman-4m` adapter uses
4,194,304-byte raw frames and LZSS window, a `7F = 29,360,128` decision
ceiling, and the exact conservative `ceil(105F/8) = 55,050,240` payload
ceiling. Its descriptor is bounded at 2,588 bytes and its aggregate policy
remains 128 MiB. It selects public window profile value 2 and otherwise uses
the same public lifecycle; queried workspace extents and alignment remain
authoritative.

The experimental `lzss-contextual-blocked-huffman-16m` adapter uses
16,777,216-byte raw frames and LZSS window, a `7F = 117,440,512` decision
ceiling, and the exact conservative `ceil(105F/8) = 220,200,960` payload
ceiling. Its descriptor is bounded at 2,597 bytes and its aggregate policy is
512 MiB. Both directions apply public profile value 3 and obtain every
allocation extent and opaque alignment from the public workspace query.
Decode requires the same `-16m` name; crossed-profile, malformed, and trailing
input retain no destination or temporary output.

The experimental `lzss-contextual-adaptive-huffman` adapter uses 65,536-byte
raw frames, at most one typed token per raw byte, the exact 9,067-node plus
4,518-symbol model bank, and a `ceil(267F/8) = 2,187,264` payload ceiling. Its
aggregate policy is 8 MiB. Both directions call only the public configuration
initializer, requirements query, factory, process, and destroy functions.
Typed tokens and FGK model storage remain in the queried opaque views; the CLI
neither names nor sizes those private layouts.

The experimental `lzss-contextual-adaptive-huffman-1m` adapter uses
1,048,576-byte raw frames and LZSS window, the exact 9,131-node plus
4,550-symbol model bank, and a `ceil(267F/8) = 34,996,224` payload ceiling.
Its aggregate policy is 128 MiB. It changes only the public exact profile and
bounded configuration values before using the same public lifecycle; queried
workspace extents and alignment remain authoritative.

The experimental `lzss-contextual-adaptive-huffman-4m` adapter applies the
public atomic window-profile helper to select 4,194,304-byte frames and
window, exact 9,163-node plus 4,566-symbol model storage, the
`ceil(267F/8) = 139,984,896` payload ceiling, and a 256-MiB aggregate limit.
The helper is the sole canonical preset; the CLI changes only `original_size`
afterward and obtains every workspace extent and alignment from the public
requirements query.

The experimental `lzss-contextual-adaptive-huffman-16m` adapter applies
public profile value 3 to select 16,777,216-byte frames and window, exact
9,195-node plus 4,582-symbol model storage, the
`ceil(267F/8) = 559,939,584` payload ceiling, and a one-GiB aggregate limit.
It uses only the public helper, requirements query, and lifecycle; it does not
duplicate private layout arithmetic. Decode requires the same exact `-16m`
name. Crossed-profile, malformed, and trailing input retain transactional
destination behavior. Interoperability schema 52 appends this exact name as
archive 62 without changing any earlier archive byte or order.

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

## Contextual rANS canonical selector for 0.2.0

The sole selector is `lzss-contextual-rans`, and it emits and accepts only
entropy variant 3's canonical variable descriptor. The fixed variant-2 path
and the `lzss-contextual-rans-compact` selector are removed without aliases.
Historical schema-33-through-36 manifest names are translated only inside the
interoperability verifier and are not accepted as general CLI input.
