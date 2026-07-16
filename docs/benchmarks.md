# Benchmarks

Configure an optimized build with `MARC_BUILD_BENCHMARKS=ON`, then build and run
`marc_benchmark` against a representative input file:

```console
marc_benchmark checksum-raw corpus.bin 5
marc_benchmark blocked-huffman corpus.bin 5
marc_benchmark adaptive-huffman corpus.bin 5
marc_benchmark dynamic-range corpus.bin 5
marc_benchmark lz77 corpus.bin 5
marc_benchmark lz77-blocked-huffman corpus.bin 5
marc_benchmark lzss corpus.bin 5
marc_benchmark lz78 corpus.bin 5
marc_benchmark lzw corpus.bin 5
marc_benchmark lzd corpus.bin 5
marc_benchmark lzmw corpus.bin 5
```

The optional positive iteration count defaults to three. Use the same build,
input, and count when comparing codecs or revisions. Release builds are required
for meaningful throughput results.

The tool verifies a complete round trip before timing. Each timed sample creates
the transform before starting the clock, calls `marc_transform_process()` once
with full input and sufficient output, stops the clock, and then destroys the
transform. Workspace allocation, transform construction/destruction, file I/O,
and verification are outside the timed region.

`encoded_to_input_ratio` includes the complete canonical stream header,
parameters, frame headers, and payload. Empty input reports ratio zero because
division by zero has no useful interpretation. Throughput uses raw input bytes
and binary MiB. `codec_peak_workspace_bytes` is the larger of the encoder and
decoder caller-owned primary-plus-secondary-plus-views workspace requirements;
it excludes the input, encoded, decoded, executable, and operating-system
memory. Direction-specific views-workspace bytes are also reported separately.

`checksum-raw` is the version 1.1 framing and CRC-32C baseline. It intentionally
does not compress payload bytes; its ratio reflects the 80-byte prefix and each
frame's 56-byte header plus four-byte checksum trailer.

`blocked-huffman` uses one MiB outer frames and 65,536-symbol blocks. Its
capacity includes the 64-byte stream header, one 16-byte descriptor per block,
and raw fallback for every input byte. Reported decoder workspace includes the
aligned caller-owned block-view region.

`adaptive-huffman` selects FGK variant 1 with one MiB outer frames. Capacity
planning uses the conservative 264-bit, or 33-byte, payload bound per symbol,
one 16-byte descriptor per nonempty frame, and the 64-byte stream header. Its
workspace report contains no views region because the fixed FGK tree is owned
by the transform rather than sized from serialized input.

`dynamic-range` selects the adaptive order-0 integer range variant with one MiB
frames and model total 32,768. Capacity planning includes two bytes per input
symbol, the five-byte canonical termination sequence, one 16-byte descriptor,
one 56-byte frame header, and the 64-byte stream prefix. The fixed model is
transform-owned, so the views workspace is zero.

`lz77-blocked-huffman` uses the same 1 MiB outer frame and 65,536-symbol
entropy block as the CLI profile. Its capacity calculation includes the
worst-case 16-byte LZ77 token per raw byte, one 16-byte Blocked Huffman
descriptor per entropy block, and raw entropy fallback. Reported workspace
therefore includes dictionary staging and decoder block views in addition to
the ordinary primary and secondary frame regions.

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results.
