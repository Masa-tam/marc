# Benchmarks

Configure an optimized build with `MARC_BUILD_BENCHMARKS=ON`, then build and run
`marc_benchmark` against a representative input file:

```console
marc_benchmark checksum-raw corpus.bin 5
marc_benchmark blocked-huffman corpus.bin 5
marc_benchmark adaptive-huffman corpus.bin 5
marc_benchmark dynamic-range corpus.bin 5
marc_benchmark rans corpus.bin 5
marc_benchmark tans corpus.bin 5
marc_benchmark lz77 corpus.bin 5
marc_benchmark lz77-blocked-huffman corpus.bin 5
marc_benchmark lz77-adaptive-huffman corpus.bin 5
marc_benchmark lzss corpus.bin 5
marc_benchmark lzss-blocked-huffman corpus.bin 5
marc_benchmark lzss-adaptive-huffman corpus.bin 5
marc_benchmark lz78 corpus.bin 5
marc_benchmark lz78-blocked-huffman corpus.bin 5
marc_benchmark lz78-adaptive-huffman corpus.bin 5
marc_benchmark lzw corpus.bin 5
marc_benchmark lzw-blocked-huffman corpus.bin 5
marc_benchmark lzw-adaptive-huffman corpus.bin 5
marc_benchmark lzd corpus.bin 5
marc_benchmark lzd-blocked-huffman corpus.bin 5
marc_benchmark lzd-adaptive-huffman corpus.bin 5
marc_benchmark lzmw corpus.bin 5
marc_benchmark lzmw-blocked-huffman corpus.bin 5
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

`rans` selects scalar byte-renormalized variant 1 with one MiB frames and
65,536-symbol blocks. Capacity planning reserves one byte per input symbol,
eight final-state bytes and one 528-byte descriptor for each of at most 16
blocks per frame, each 56-byte frame header, and the 64-byte stream prefix.
Reported decoder workspace includes the aligned caller-owned block-view region.

`tans` selects tabled variant 1 with one MiB frames and 65,536-symbol blocks.
Capacity planning uses `ceil(3*n/2)` bytes for the strict 12-bit transition
bound, plus two state bytes and one 528-byte descriptor for each of at most 16
blocks per frame, each 56-byte frame header, and the 64-byte stream prefix.
Reported decoder workspace includes the aligned caller-owned block-view region.

`lz77-blocked-huffman` uses the same 1 MiB outer frame and 65,536-symbol
entropy block as the CLI profile. Its capacity calculation includes the
worst-case 16-byte LZ77 token per raw byte, one 16-byte Blocked Huffman
descriptor per entropy block, and raw entropy fallback. Reported workspace
therefore includes dictionary staging and decoder block views in addition to
the ordinary primary and secondary frame regions.

`lz77-adaptive-huffman` uses the CLI's 65,536-byte raw frame, at most
1,048,576 canonical LZ77 token bytes, and one independently reset FGK tree per
outer frame. Capacity planning reserves the conservative 33-byte Adaptive
payload bound for each token byte, one 16-byte descriptor per nonempty frame,
each 56-byte frame header, and the 80-byte parameterized stream prefix. The
benchmark obtains both direction-specific workspace extents from the public C
ABI and verifies a complete round trip before timing.

`lzss-blocked-huffman` uses the same frame and entropy-block policy. Capacity
planning substitutes LZSS's two-byte all-Literal token bound, includes one
16-byte descriptor per worst-case token block, and permits raw entropy fallback
for the complete token stream. Reported workspace includes token staging and
decode-side aligned block views.

`lzss-adaptive-huffman` uses the CLI's 65,536-byte raw frame, at most 131,072
canonical LZSS token bytes, and one freshly reset FGK tree per outer frame.
Capacity planning reserves 33 Adaptive payload bytes per token byte, one
16-byte descriptor and 56-byte header per nonempty frame, and the 80-byte
parameterized stream prefix. Both direction-specific workspace extents come
from the public C ABI. A complete byte-exact round trip succeeds before either
direction is timed.

`lz78-blocked-huffman` uses one MiB raw frames, 65,536-symbol entropy blocks,
and at most 65,536 LZ78 phrase entries. Capacity planning uses the exact
eight-byte token bound per raw byte, one 16-byte descriptor per possible token
block, and raw entropy fallback. The benchmark obtains all caller-owned byte
counts and alignment from the public C ABI; the reported views workspace
therefore includes the encoder phrase table or the aligned decoder block views
and phrase table.

`lz78-adaptive-huffman` uses the CLI's 65,536-byte raw frame, at most 524,288
canonical LZ78 token bytes, one freshly reset FGK tree per outer frame, and a
32-MiB aggregate policy. Capacity planning reserves 33 Adaptive payload bytes
per token byte, one 16-byte descriptor and 56-byte header per nonempty frame,
and the 80-byte parameterized stream prefix. Both direction-specific workspace
extents and opaque phrase-table alignment come from the public C ABI. A
complete byte-exact round trip succeeds before either direction is timed. The
reported caller-reserved peak may exceed the 32-MiB active aggregate policy
because the conservative serialized-frame reservation coexists with token,
raw-frame, and typed-view regions.

`lzw-blocked-huffman` uses one MiB raw frames, 65,536-symbol entropy blocks,
the exact two-byte-per-raw-byte packed-code bound, at most 32 entropy blocks,
and at most 65,280 additional LZW entries. Capacity includes one 16-byte
descriptor per possible packed-code block and raw entropy fallback. The public
C ABI query supplies all primary, secondary, and aligned views extents reported
by the benchmark.

`lzw-adaptive-huffman` uses the CLI's 65,536-byte raw frame and maximum code
width 16. Capacity planning reserves at most 131,072 packed LZW bytes,
4,325,376 Adaptive payload bytes, one 16-byte descriptor and 56-byte header per
nonempty frame, and the 80-byte parameterized stream prefix. Its aggregate
active-byte policy is 8 MiB and it admits at most 65,280 generated entries.
Both direction-specific workspace extents and opaque record alignment come
from the public C ABI. A complete byte-exact round trip succeeds before either
direction is timed. The reported caller-reserved peak may exceed 8 MiB because
the conservative complete-frame reservation coexists with packed, raw, and
typed-record workspaces.

`lzd-blocked-huffman` uses the CLI's one-MiB raw frames, 65,536-symbol entropy
blocks, exact four-MiB token bound, at most 64 entropy blocks, and 65,536-entry
LZD dictionary policy. Capacity includes one 16-byte descriptor per possible
token block and raw entropy fallback. The public C ABI query supplies all
reported encoder and decoder workspace bytes, including the decoder's private
entropy-view, phrase, and iterative-expansion storage.

`lzd-adaptive-huffman` uses the CLI's 65,536-byte raw frame, 262,144-byte
canonical-token ceiling, 8,650,752-byte Adaptive payload ceiling, 65,536-entry
dictionary policy, and 16-MiB active aggregate limit. Capacity planning adds
one 16-byte descriptor and 56-byte frame header per nonempty frame to the
80-byte parameterized prefix and reserves Adaptive payload as the checked exact
ceiling `264*ceil(raw_bytes/2)`, including an odd final frame. Both direction-
specific workspace extents and opaque-view alignment come from the public C
ABI, and a complete byte-exact
round trip succeeds before timing. The reported caller-reserved peak may exceed
16 MiB because conservative encoded-frame, token, raw, phrase, and expansion
regions coexist even though active codec operations obey the aggregate limit.

`lzmw-blocked-huffman` uses the same one-MiB raw frame, 65,536-symbol entropy
block, four-byte-per-raw-byte reference bound, 64-block cap, 65,536-entry
dictionary policy, and 64-MiB active aggregate limit as the CLI. The benchmark
obtains all three region sizes and alignment from the public C ABI and verifies
a complete round trip before timing. `codec_peak_workspace_bytes` reports the
sum of caller-reserved regions, which can exceed the active aggregate policy
because the conservative maximum serialized-frame reservation coexists with
reference, raw-frame, and typed-view reservations.

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results.
