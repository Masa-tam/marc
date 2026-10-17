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

### Position-distance profile

`lzss-position-distance-dynamic-range` explicitly selects the dedicated C factory
for format 2.0 dictionary 2/8, context 1/9, entropy 3/2 in both directions.
It uses the complete initializer defaults: 65,536-byte frames/window, matches
3..258 and fixed exact indexed encoder search. It has no larger-window suffix,
finder or profile option. Unsupported selections are usage errors.

```console
marc encode --codec lzss-position-distance-dynamic-range input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range output.marc restored.bin
```

This additional Format 2 profile is separate from the baseline inventory below.
Its public completion evidence is recorded in `baseline-readiness.md`. Schema 58
includes it as archive 68 and has passed external four-direction verification. No
header-driven codec selection or automatic limit expansion is performed.
Existing contextual decoders reject this identity, and this decoder rejects
their streams. Failure removes temporary output and preserves existing files.
The default codec and every existing codec's bytes remain unchanged.

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
`2/5 + 1/4 + 2/2`. `lzss-contextual-blocked-huffman-64m` selects exact
`2/6 + 1/5 + 2/2`. Encode and decode require the same explicit name; no
profile auto-detects or admits another. All five remain experimental.
`lzss-contextual-adaptive-huffman` selects typed LZSS plus Contextual Adaptive
Huffman entropy variant 2 under the frozen 64 KiB profile. The additive
`lzss-contextual-adaptive-huffman-1m` name selects exact
`2/3 + 1/2 + 1/2`, and `lzss-contextual-adaptive-huffman-4m` selects exact
`2/4 + 1/3 + 1/2`. `lzss-contextual-adaptive-huffman-16m` selects exact
`2/5 + 1/4 + 1/2`, and `lzss-contextual-adaptive-huffman-64m` selects exact
`2/6 + 1/5 + 1/2`. Encode and decode require the same explicit name; no
profile auto-detects or admits another. All five remain outside the stable
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
Interoperability schema 55 appends this exact CLI profile as archive 65 after
the frozen 64-entry schema-54 order without changing earlier bytes or order.

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

The experimental `lzss-contextual-adaptive-huffman-64m` adapter applies
public profile value 4 to select 67,108,864-byte frames and window, exact
9,227-node plus 4,598-symbol model storage, the
`ceil(267F/8) = 2,239,758,336` payload ceiling, and an eight-GiB aggregate
policy. It uses only the public helper, requirements query, and factory;
decode requires the same exact `-64m` name and a 16-MiB decoder rejects the
identity before output publication. The separately admitted bounded fuzz
profile changes no command behavior; no interoperability archive is added.

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

## 64-MiB Contextual Blocked Huffman application profile

`lzss-contextual-blocked-huffman-64m` explicitly selects `2/6 + 1/5 + 2/2`.
The public profile helper supplies 67,108,864-byte frame/window limits,
536,870,912 decisions, 1,006,632,960 payload bytes, 17,885 table entries,
and a four-GiB aggregate ceiling. The decoder's full-profile caller-owned
workspace is 1,879,194,082 bytes on the validated x64 builds. This allocation
is required even for a short file; use a smaller profile when appropriate.
No input field automatically raises the selected local limits. Existing names
and initializer defaults are unchanged. Encode and decode require the same
explicit codec name; an uppercase `64M` suffix is invalid usage.

Interoperability schema 56 appends the exact
`lzss-contextual-blocked-huffman-64m` CLI profile as archive 66 after the
frozen schema-55 order. The bundle generator checks identity
`2/6 + 1/5 + 2/2` before recording the archive and immediately decodes it back
to the common fixture.

Interoperability schema 57 retains that complete schema-56 prefix and appends
the exact `lzss-contextual-adaptive-huffman-64m` CLI profile as archive 67.
The generator checks identity `2/6 + 1/5 + 1/2` before recording it and
immediately decodes it back to the common fixture.

## 1 MiB position-distance selector

`lzss-position-distance-dynamic-range-1m` selects the separate 1 MiB window
and default frame, dictionary/context identity 2/9 + 1/10, entropy 3/2.
It uses fixed eligibility three and the bounded exact indexed finder.

```sh
marc encode --codec lzss-position-distance-dynamic-range-1m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-1m output.marc restored.bin
```

This explicit selector adds one experimental profile outside the baseline
42-profile matrix. The 64 KiB selector retains its meaning. The two selectors
and contextual 1 MiB reject each other's headers. There is no automatic
selection or `--profile`/`--finder` override. The existing schema-58 bundle
continues to contain its original 68 archives; qualification of the new profile
will use a subsequent interoperability gate.


## 4 MiB position-distance selector

`lzss-position-distance-dynamic-range-4m` selects the separate four-MiB window
and default frame, dictionary/context identity 2/10 + 1/11, entropy 3/2.
It uses the admitted five-prefix scalar encoder with fixed eligibility three
and the retained scalar decoder, through the public four-MiB C factory.

```sh
marc encode --codec lzss-position-distance-dynamic-range-4m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-4m output.marc restored.bin
```

The selected initializer supplies 4194304-byte frame and block ceilings,
75497477-byte compressed-payload ceiling and 536870912-byte aggregate ceiling.
These are profile-local defaults; the CLI has no resource-limit override.
There is no automatic profile selection or `--profile`/`--finder` override.
An uppercase `4M` suffix is invalid usage. The 64-KiB and one-MiB selectors
retain their existing meanings and reject the four-MiB identity, and conversely.

The existing file transaction commits output only after complete stream success;
a failed later frame leaves neither a destination nor its temporary output.
Pre-existing destination or temporary files are refused and preserved.
This adds an explicit experimental selection outside the baseline 42-profile
matrix. Schema 59 and its 69 archives retain their existing inventory; adding
the new exchange archive and revision-specific external verification are later
qualification gates.


## Schema 60 exchange inventory

Schema 60 (`marc-cli-v60`) appends `lzss-position-distance-dynamic-range-4m`
as archive 70 after the unchanged schema-59 prefix of 69 archives. Its exact
identity is 2/10 + 1/11 + 3/2. The generator and verifier validate that identity;
all earlier schema profile lists and the 8193-byte fixture remain unchanged.
This small exchange fixture does not cover the full four-MiB window. Complete
corpus and window-boundary checks are separate evidence. Hosted CI and external
producer/consumer verification must qualify the actual new revision.

## 8 MiB position-distance selector

Use the same explicit selection for both directions:

```console
marc encode --codec lzss-position-distance-dynamic-range-8m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-8m output.marc restored.bin
```

This experimental selection uses window/frame/block 8388608 bytes, matches
3..258 and exact identity 2/11 + 1/12 + 3/2. It uses the prepared owning encoder
and five-buffer decoder under an explicit 1073741824-byte codec logical policy,
150994949-byte payload ceiling and 1099511627776-byte raw output ceiling.
Call buffers are 65536 bytes each; entropy entries/range total are 2599/32768,
expansion ratio/slack are 1024/1048576. Full decoder capacities are retained
even for small files. Allocation or prospective generation refusal fails the
invocation without increasing limits, retrying or switching strategy; initial
encoder query success alone is not complete-stream admission. These bounds
do not assert resident-memory fit or include file-library/allocator overhead.

There is no automatic file-size selection, profile/finder/resource override or
uppercase 8M alias. Existing codec names, defaults and smaller-window meanings
are unchanged. The decoder accepts bounded smaller legal header parameters.
Failed frames are never published. Whole-file output is committed only after
complete success; a failing invocation removes its own temporary output while
preserving pre-existing destination/temporary files. Schema 60 retains its 70
archives; schema 61 with the additional archive and external revision-specific
verification remains a subsequent gate.


## 16 MiB position-distance selector

```console
marc encode --codec lzss-position-distance-dynamic-range-16m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-16m output.marc restored.bin
```

This separate experimental selection uses exact identity 2/12 + 1/13 + 3/2,
window/frame/block 16777216 bytes and matches 3..258. The compact owning encoder
and five-workspace decoder use an explicit 536870912-byte logical internal
policy and 67108864-byte payload ceiling. Call buffers are 65536 bytes each;
model entries/total are 2610/32768, raw ceiling is 1099511627776 bytes and
expansion ratio/slack are 1024/1048576. Complete controls, backing capacities
and retained publications are charged. Initial encoder admission does not
guarantee later frames fit. Refusals do not retry with larger limits. These
logical bounds are not a resident-memory promise and exclude file-library
internals and allocator overhead.

There is no uppercase 16M alias or automatic file-size selection, finder,
profile or memory override. Older selections and global defaults remain
unchanged. The decoder accepts bounded smaller legal header parameters.
Failed frames never publish; the destination commits only after complete
stream success. Pre-existing destinations and temporary files are preserved.
This selection remains outside the unchanged 42-profile baseline table.

## Schema 62 exchange inventory

Schema 62 (`marc-cli-v62`) appends `lzss-position-distance-dynamic-range-16m`
as archive 72 after the frozen 71-entry schema-61 prefix. The 8193-byte fixture
is unchanged. Whole-manifest admission precedes codec execution. Small-bundle
exchange qualification is separate from maximum-window tests and from hosted
CI and external reports for the actual new revision.


## 32 MiB position-distance selector

```console
marc encode --codec lzss-position-distance-dynamic-range-32m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-32m output.marc restored.bin
```

The distinct experimental selection uses 2/13 + 1/14 + 3/2, 33554432-byte
window/frame/block and matches 3..258. It uses the compact owning encoder and
five byte-workspace decoder under an explicit 536870912-byte codec policy,
67108864-byte payload cap and 1099511627776-byte raw output limit. Calls use
65536 bytes each; model entries/total are 2621/32768, expansion ratio/slack
are 1024/1048576. Query all capacities before allocation and charge complete
controls, I/O views and retained generations. Actual per-frame admission is
mandatory after initial creation. Refusal does not increase limits or retry.
The policy counts codec ownership, not file-library internals, allocator
overhead or OS resident memory. The payload cap is deliberately smaller than
the maximum valid wire payload and may reject otherwise valid streams.

Selection is explicit and case-sensitive. There is no automatic file-size
selection or finder/profile/memory override. Older selectors and global
defaults keep their meanings. Legal smaller bounded header parameters remain
accepted. Failed frames never publish; existing targets and temporary files
are preserved. Only whole-stream success commits the output file. This
experimental selection is separate from the unchanged 42-profile baseline
table. Schema-63/archive-73 exchange registration is a subsequent gate.


## Schema 63 exchange inventory

Schema 63 (`marc-cli-v63`) appends `lzss-position-distance-dynamic-range-32m`
as archive 73 after the frozen 72-entry schema-62 prefix. Its exact identity is
2/13 + 1/14 + 3/2. The independent 8193-byte fixture and every older inventory
remain unchanged. Entire-manifest admission precedes codec launch and output
publication, including the final archive's version, dictionary, context and
entropy identity. A rehashed wrong identity remains invalid. Full-window and
failed-frame behavior is established separately by public and actual CLI
qualifications. Hosted CI and four external consumer routes require reports
for the final new revision; local tests do not close those external gates.


## 64 MiB position-distance selector

```console
marc encode --codec lzss-position-distance-dynamic-range-64m input.bin output.marc
marc decode --codec lzss-position-distance-dynamic-range-64m output.marc restored.bin
```

This distinct experimental selector uses dictionary 2/14, context 1/15 and
entropy 3/2, with 67108864-byte window/frame/block and wire matches 3..258.
It connects the compact owning encoder and five byte-workspace decoder under
an explicit 1073741824-byte codec admission policy, 134217728-byte payload cap
and 1099511627776-byte raw output limit. Calls use 65536 bytes each; model
entries/total are 2632/32768, expansion ratio/slack are 1024/1048576. Before
encoder creation, the complete initial public query plus 2F + 4Pcap + 240 must
fit the policy, retaining all old/candidate generations and initial controls.
Query every decoder capacity before allocation. Actual per-frame admission
remains mandatory; refusal never increases limits or retries. The policy counts
codec ownership, not file-library internals, allocator overhead or OS resident
memory. The payload cap is smaller than the maximum valid wire payload and
may reject otherwise valid streams.

Selection is explicit and case-sensitive. There is no automatic file-size
selection or finder/profile/memory override. Generic defaults and every prior
selector retain their meanings. Smaller legal bounded header parameters remain
accepted. Failed frames never publish, and pre-existing targets and temporary
files are preserved. Only whole-stream success commits the output file. This
experimental selection is separate from the unchanged 42-profile baseline
table. Schema-64/archive-74 exchange registration is a subsequent gate.
## Position-distance rANS one-MiB selector

Use explicit, case-sensitive `lzss-position-distance-rans-1m` with encode or decode.
It selects dictionary `2/9`, context `1/10`, entropy `4/4`, a one-MiB frame and
window, and fixed-five encoder eligibility. The decoder grammar includes
lengths three/four. The owning API preflights raw, token, serialized, finder,
working and CLI retained/call capacities against a 64MiB aggregate byte budget.
Input/output chunks are 65,536 bytes; total raw is capped at one TiB, decisions
at nine Mi per frame, payload at 18MiB+8 and frequencies at 2566. This is a
codec capacity policy, not a process RSS promise. There is no automatic
selection or memory-limit retry.

The existing temporary-file contract applies: pre-existing outputs and
temporary files remain intact; only whole-stream success commits a final file.
Truncation, malformed models/states, contradictory parameters and trailing
bytes fail without publishing the failed output. Existing selectors and the
forty-two-profile baseline table retain their meanings. Exchange registration
and measured directional speed/process peak are separate qualification gates.
BM-0216 qualifies the complete public paths against contextual rANS: all
twelve full-corpus archives are smaller, with measured encode improvement,
decode slowdown and increased process peaks. This qualifies the 64MiB capacity
policy without making it an OS RSS limit or a universal speed claim. Schema 65
adds this selector as archive 75 while retaining every old archive byte.

DD-1525 corrects the spelling before publication. The previous
`lzss-position-rans-1m` spelling is not accepted. Schema 65 uses the canonical
name for archive 75 without changing its encoded bytes.

## 64KiB position-distance rANS selector

`lzss-position-distance-rans` selects the native 64KiB profile described in
[its design](design/lzss-position-distance-rans-64k.md). It uses forty contexts
and scalar byte-renormalized rANS; `lzss-position-distance-rans-1m` remains a
separate selector and format. The capacity budget is 4MiB, including the
declared CLI controls and call buffers. A complete file is committed through
the existing temporary-output contract. Malformed streams cannot overwrite
existing output or temporary files.


## Four-MiB native position-distance rANS connection

The explicit case-sensitive selector lzss-position-distance-rans-4m uses
DD-1532's 2/10 + 1/16 + 4/7 format with16 previous-literal buckets. It retains
known-size frames, strict trailing-data rejection, private failed-frame
quarantine and the existing whole-file temporary commit contract.
Default capacity budget192MiB is independent of OS process peaks.
Seven boundary cases pass in both compiler implementations. BM-0220 records
directional samples and separate process peaks; IX-0070 records local
schema67 exchange qualification. Hosted CI/external results are separate.

## Eight-MiB native position-distance rANS

The explicit selector lzss-position-distance-rans-8m uses DD-1536's
2/11+1/17+4/8 known-size format. It retains65536-byte input/output calls,
strict termination and private failed-frame quarantine, and whole-file
temporary commit. DD-1541's384MiB checked capacity grant is admitted after
BM-0222 public resource and peak qualification. TVG-1401 qualifies the seven CLI boundary
cases in two builds. IX-0073 qualifies schema68 exchanges and the fixed runtime; no previous
selector or archive representation changes.

## Sixteen-MiB native position-distance rANS

The explicit selector lzss-position-distance-rans-16m uses DD-1542's
2/12+1/18+4/9 known-size framing with16777216-byte frame/window maximum.
Retain65536-byte calls, fixed-five encoding, strict termination, failed-frame
quarantine and whole-file temporary commit. DD-1546 retains768MiB after
BM-0224 public resource/peak measurements. Boundary qualification, five
sanitizer campaigns and the schema69 exchange milestone pass. IX-0075
qualifies local current-runtime and committed-source exchange closure.
Existing selectors and archive representations remain unchanged; hosted CI
and maintainer external results are separate.

## Thirty-two-MiB native position-distance rANS

The case-sensitive selector lzss-position-distance-rans-32m uses DD-1548's
2/13+1/19+4/10 known-size framing with33554432-byte frame/window maximum.
DD-1550 retains65536-byte calls, fixed-five encoding, strict termination,
failed-frame quarantine and whole-file temporary commit. The1536MiB checked
capacity grant is admitted by DD-1551 after BM-0227 public resource/peak
qualification. IX-0077 closes local schema70/80 source/runtime verification.
Earlier selectors and representations remain unchanged. Maintainer push,
hosted CI and external routes remain separate gates.

## Sixty-four-MiB native position-distance rANS

The case-sensitive selector lzss-position-distance-rans-64m uses DD-1552's
2/14+1/20+4/11 known-size framing with67108864-byte frame/window maximum.
DD-1554 retains65536-byte calls, fixed-five encoding, strict termination,
failed-frame quarantine and whole-file temporary commit. The3072MiB checked
capacity grant is admitted by DD-1555 after BM-0231's complete public
resource and directional qualification. Earlier selectors and representations
remain unchanged. Five instrumented fuzz campaigns and preliminary schema71
qualification also pass. Final fixed-runtime/source81 qualification and
maintainer push, hosted CI and external routes remain pending.

## Sixty-four-KiB position-distance tANS

The case-sensitive selector lzss-position-distance-tans uses dictionary2/8,
context1/21 and entropy5/3, with65536-byte frames/windows and a known original
length. DD-1568 connects the qualified public owning factory with65536-byte
calls and65536 declared external control bytes under its8MiB capacity grant.
This is not an RSS limit. Fixed-five encoding and decoder match lengths3..258
retain their distinct scopes. All stream/frame checks and entropy terminal
validation precede failed-frame publication.

The existing whole-file temporary contract remains: an error, including a
late second-frame error after prior raw output, does not publish the file.
Existing destination/temporary files are refused unchanged. Unsupported IDs,
reserved fields, truncation and trailing data refuse without publication.
Existing selectors and representations are retained. Public API/CLI and
full-corpus byte equivalence are locally qualified; isolated speed/normal
public peaks, instrumented fuzz and additive exchange remain required.
No new hosted CI or external result is claimed.

### One-MiB position-distance tANS

The exact selector `lzss-position-distance-tans-1m` identifies dictionary2/9,
context1/22 and tANS5/4. Known-size streams reset dictionary/models at each
one-MiB frame. Fixed-five encoding preserves longest/nearest matches;
decoding accepts the specified3..258 grammar. The distance-less selector
`lzss-position-tans-1m` is rejected. Existing64KiB and contextual selectors
retain their own identities.

The CLI uses the distinct one-MiB public factory with64MiB aggregate admission,
65536-byte input/output calls and65536 declared retained control bytes. Actual
full query charges are65543980 encode and27433809 decode bytes; these are
capacity accounting rather than process peaks. Failed frames never drain.
Whole-file output is renamed only after complete successful decoding and
strict trailing checks. Failed inputs publish no final destination; existing
output/temporary files remain unchanged. Corpus comparisons, repeated normal
measurements, fuzz and exchange are separate pending qualification gates.

### Four-MiB position-distance tANS

The exact selector `lzss-position-distance-tans-4m` identifies dictionary2/10,
context1/23 and tANS5/5. Known-size streams reset dictionary and models at
each four-MiB frame. Fixed-five encoding preserves longest/nearest matches;
decoding accepts the specified3..258 grammar and distances through4,194,304.
The distance-less name `lzss-position-tans-4m` is rejected.

The CLI uses the four-MiB public factory with a384MiB default aggregate
admission,65536-byte input/output calls and65536 declared retained control
bytes. Resource queries and creation are bounded before allocation. Failed
frames never drain, and whole-file output is renamed only after successful
decoding and strict trailing checks. Boundary, truncation, deterministic
re-encode, wrong-profile and destination-preservation tests are connected;
contextual comparison, paired measurements, fuzz and exchange remain
separate qualification gates.

### Eight-MiB position-distance tANS

The exact selector `lzss-position-distance-tans-8m` identifies dictionary2/11,
context1/24 and tANS5/6. Known-size streams reset dictionary and models at
each eight-MiB frame. Fixed-five encoding preserves longest/nearest matches;
decoding accepts the specified3..258 grammar and distances through8,388,608.
The distance-less name `lzss-position-tans-8m` is rejected.

The CLI uses the eight-MiB public factory with a one-GiB aggregate admission,
65536-byte input/output calls, and65536 declared retained control bytes.
Resource queries and creation complete before input bytes are consumed.
Failed frames never drain, and whole-file output is renamed only after
successful decoding and strict trailing checks. The local selector test covers
deterministic small-frame round trips, header identity, wrong-profile and
truncation rejection, and destination preservation. Full-frame measurements,
coverage-guided fuzzing, contextual comparison, and exchange qualification
remain separate gates.

### Sixty-four-KiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman` identifies native
dictionary2/8, context1/28 and entropy2/3 with65536-byte window/reset frames.
The encoder uses fixed-five longest/nearest matching; the decoder accepts the
specified3..258 wire match lengths. The distance-less name and a64k suffix
are not aliases. Existing byte-stream and contextual Blocked Huffman selectors
retain their representations and are not auto-detected by this decoder.

The CLI uses the native C factory with8MiB logical aggregate admission,
65536-byte input/output calls and65536 declared retained control bytes.
Resource query and construction occur before transform consumption. The grant
is not an RSS or physical peak limit. Failed frames never drain. Whole-file
output is renamed only after successful decoding, exact EOS, sink close and
strict trailing checks. Existing output and temporary files are preserved on
refusal; a failed run removes only its own newly created temporary file.
Deterministic boundary round trips, exact headers, truncation, invalid tables/
metadata, wrong selectors, padding and valid-first/failed-second frame cases
are checked. Corpus comparisons, speed/peak measurements, randomized fuzzing
and exchange qualification remain separate gates.

### One-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-1m` selects native
dictionary2/9, context1/29 and entropy2/4 with1048576-byte window/reset frames.
Fixed-five longest/nearest encoding and3..258 decoder grammar use the public
one-MiB C factory. The distance-less spelling and upper-case1M suffix are not
aliases. The64KiB native selector and all earlier codecs retain their formats.

The logical grant is96MiB with65536-byte input/output arrays and65536 declared
control-retention bytes; it is not an RSS cap. Resource admission precedes
transform input. Raw data is released only after complete private frame
validation, and the ordinary file is renamed only after exact EOS, strict
trailing checks and successful sink close. Existing output and temporary
files remain unchanged on refusal; only a run's own newly created temporary
is removed on failure. Boundary round trips, exact identity, independent
length3/4/258 wires, all short-stream cuts, bad tables/metadata/padding,
foreign profiles and valid-first/failed-second whole-file refusal are checked.
All-member contextual comparisons, speed/physical peaks, coverage-guided fuzz
and schema80 exchange remain separate gates.
Those one-MiB local gates are complete at [IX-0098](interoperability.md),
including all four local exchange paths. Hosted/external qualification of the
final95-archive family remains separate.

### Four-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-4m` selects native
dictionary2/10, context1/30 and entropy2/5 with4194304-byte window/reset frames.
It uses the four-MiB public C factory, fixed-five longest/nearest encoding and
length3..258 decoder grammar. Distance-less spellings, upper-case4M and an
extra64k suffix are not aliases. Earlier profiles retain their identities.

The logical grant is320MiB, with65536-byte input/output arrays and65536
declared control-retention bytes. Admission precedes transform consumption;
the grant is not a process RSS cap. Complete private frame validation precedes
raw draining, and whole-file rename follows exact EOS, strict trailing checks
and successful sink close. Existing output/temporary files stay unchanged on
refusal; only the current run's newly created temporary file is removed.
Shared/static command paths exercise boundaries through two full frames plus
a short tail, exact identity, independent short/max-length references, every
small-stream cut, malformed fields/tables/padding, foreign profiles and a valid
first/failed second frame. Full contextual comparisons/physical peaks, fuzz and
schema81 exchange remain separate qualification gates.

### Eight-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-8m` selects native
dictionary2/11, context1/31 and entropy2/6 with8388608-byte window/reset frames.
It uses the eight-MiB public C factory, fixed-five longest/nearest encoding and
length3..258 decoding. Distance-less spellings, upper-case8M and an extra64k
suffix are not aliases. Previous profiles retain their exact representations.

The logical grant is640MiB with65536-byte input/output arrays and65536
declared control-retention bytes. Actual public admission is635845671 encode
and251157534 decode bytes on the qualified layouts; these are not RSS limits.
Admission precedes transform consumption. Only completely validated private
frames drain; whole-file rename follows exact EOS, strict trailing checks and
successful sink close. Existing output and temporary files stay unchanged on
refusal; only the current run's new temporary file is removed after failure.
Shared/static checks cover seven boundaries through two full frames plus a
tail, exact identity, independent short/max-length references, every small cut,
malformed fields/tables/padding, foreign profiles and a valid first/failed
second frame. Full contextual comparisons/ordinary peaks, schema82 exchange
and final audit remain separate gates. Fuzz is omitted by user request.

### Sixteen-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-16m` selects native
dictionary2/12, context1/32 and entropy2/7 with16777216-byte window/reset frames.
It uses the sixteen-MiB public C factory, fixed-five longest/nearest encoding
and length3..258 decoder grammar. Distance-less spellings, upper-case16M and
an extra64k suffix are not aliases. Earlier identities remain unchanged.

The1280MiB logical grant includes65536-byte input/output arrays and65536
declared control-retention bytes. Actual public admission is1270239115 encode
and501772106 decode bytes on the qualified layouts; these are not process RSS.
Admission precedes transform consumption. Completely validated private frames
drain before whole-file rename, which follows exact EOS, strict trailing checks
and successful sink close. Existing output and temporary files stay unchanged
on refusal; only a newly created temporary file from this run is removed.

Static/shared qualification covers seven boundaries through two full frames
plus a short tail, exact identity, independent short/max-length references,
every small-stream cut, malformed fields/models/padding, foreign profiles and
a valid first/failed second frame. Schema83 exchange and final audit are
separate gates. Ratio/speed/physical-peak benchmarks and fuzz are omitted by
user request through this goal; no unmeasured compression target is claimed.
Those sixteen-MiB local gates are closed at [IX-0101](interoperability.md).
Final95-family hosted/external qualification remains separate.

### Thirty-two-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-32m` selects native
dictionary2/13, context1/33 and entropy2/8 with33554432-byte window/reset frames.
It uses the thirty-two-MiB public C factory, fixed-five longest/nearest encoding
and length3..258 decoder grammar. Distance-less spellings, upper-case32M and
an extra64k suffix are not aliases. Earlier identities remain unchanged.

The3072MiB logical grant includes65536-byte input/output arrays and65536
declared control-retention bytes. Checked public admission is2727764634 encoder
and1065910860 decoder bytes on the qualified layouts; these are not process
peaks. Admission precedes transform consumption. Completely validated private
frames drain before whole-file rename, which follows exact EOS, strict trailing
checks and successful sink close. Existing output and temporary files remain
unchanged on refusal; only a newly created temporary file from this run is
removed after failure. A valid first/failed second frame must never publish a
partial target file. CLI qualification and schema84/94 exchange are separate
gates. Ratio/speed/physical-peak benchmarks and fuzz are omitted by user request;
no unmeasured same-window compression or performance target is claimed.

TVG-1538 qualifies static/shared byte identity at seven boundaries through two
full frames plus a tail, all212 small-stream cuts, independent length3/4/258
references, malformed metadata/models/padding, ten existing foreign profiles
and a valid first/failed second frame. Both paths restore and reproduce all93
preceding archives. Existing target/temporary files and pre-update runtime
artifacts are preserved. Schema84/94 exchange and the final local audit are
qualified at [IX-0102](interoperability.md).
Final95-family hosted/external qualification remains separate.

### Sixty-four-MiB position-distance Blocked Huffman

The exact selector `lzss-position-distance-blocked-huffman-64m` selects native
dictionary2/14, context1/34 and entropy2/9 with67108864-byte window/reset frames.
It uses the sixty-four-MiB public C factory, fixed-five longest/nearest encoding
and length3..258 decoder grammar. Distance-less spellings, upper-case64M and
an extra64k suffix are not aliases. Earlier identities remain unchanged.

The6144MiB logical grant includes65536-byte input/output arrays and65536
declared control-retention bytes. Checked public admission is5454067158 encoder
and2131268960 decoder bytes on the qualified layouts; these are not process
peaks. Admission precedes transform consumption. Completely validated private
frames drain before whole-file rename, which follows exact EOS, strict trailing
checks and successful sink close. Existing output and temporary files remain
unchanged on refusal; only a newly created temporary file from this run is
removed after failure. A valid first/failed second frame must never publish a
partial target file. CLI qualification and schema85/95 exchange are separate
gates. Ratio/speed/physical-peak benchmarks and fuzz are omitted by user request;
no unmeasured same-window compression or performance target is claimed.

TVG-1546 qualifies static/shared byte identity at seven boundaries through two
full frames plus a tail, all212 small-stream cuts, independent length3/4/258
references, malformed metadata/models/padding, eleven existing foreign profiles
and a valid first/failed second frame. Both paths restore and reproduce all94
preceding archives. Existing target/temporary files and pre-update runtime
artifacts are preserved. Schema85/95 exchange and the final family local audit
are qualified at [IX-0103](interoperability.md). Later hosted CI and external
four-route95 confirmation remain separate.
