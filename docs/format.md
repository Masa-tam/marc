# Stream format

The baseline stream prefix is assigned format version 1.0. Frame headers,
algorithm parameter regions, payloads, and trailers remain incomplete. No
encoder implementation may be added until its complete decoder-visible layout
is specified here and accompanied by hand-checkable vectors.

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
| 1 | Adaptive Huffman | FGK |
| 2 | Blocked Huffman | canonical baseline, details pending |
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
