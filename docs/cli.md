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
| `lzss` | LZSS | None | Variant 1 |
| `lzss-blocked-huffman` | LZSS | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lz78` | LZ78 | None | Variant 1 |
| `lz78-blocked-huffman` | LZ78 | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzw` | LZW | None | Variant 1 |
| `lzw-blocked-huffman` | LZW | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzd` | Lempel-Ziv Double | None | Variant 1 |
| `lzd-blocked-huffman` | Lempel-Ziv Double | Blocked Huffman | Composed dictionary and entropy pipeline |
| `lzmw` | LZMW | None | Variant 1 |

Except for `checksum-raw`, these profiles use the current version 1 stream
representation described in the [format specification](format.md).

The `lz78-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the eight-byte-per-raw-byte LZ78 token bound, at most 128
entropy blocks per frame, and at most 65,536 phrase entries. All actual
workspace extents and alignment still come from the public C ABI requirements
query; the command-line layer does not reproduce the private typed layout.

The `lzw-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the two-byte-per-raw-byte packed LZW bound, at most 32 entropy
blocks per frame, and at most 65,280 additional LZW dictionary entries. Its
actual three workspace extents and alignment likewise come from the public C
ABI requirements query.

The `lzd-blocked-huffman` adapter uses one-MiB raw frames, 65,536-symbol
entropy blocks, the exact four-byte-per-raw-byte worst-case LZD token bound,
at most 64 entropy blocks per frame, and at most 65,536 phrase entries. The
public requirements query supplies all three workspace extents and alignment;
the CLI does not reproduce the private entropy-view, phrase, or expansion-stack
layout. The aggregate internal-buffer limit remains 64 MiB.

## File and error behavior

The destination and its `.tmp` staging path must not already exist. A
successful operation renames the staging file to the destination. A failed
operation removes the staging file, so malformed input does not leave a
partially decoded destination.

The process returns `0` on success, `1` when the requested operation fails, and
`2` for invalid command-line usage or an unknown profile name.
