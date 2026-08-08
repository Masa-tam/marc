# Benchmarks

## Running the benchmark

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
marc_benchmark lz77-dynamic-range corpus.bin 5
marc_benchmark lz77-rans corpus.bin 5
marc_benchmark lz77-tans corpus.bin 5
marc_benchmark lzss corpus.bin 5
marc_benchmark lzss-blocked-huffman corpus.bin 5
marc_benchmark lzss-adaptive-huffman corpus.bin 5
marc_benchmark lzss-dynamic-range corpus.bin 5
marc_benchmark lzss-rans corpus.bin 5
marc_benchmark lzss-tans corpus.bin 5
marc_benchmark lz78 corpus.bin 5
marc_benchmark lz78-blocked-huffman corpus.bin 5
marc_benchmark lz78-adaptive-huffman corpus.bin 5
marc_benchmark lz78-dynamic-range corpus.bin 5
marc_benchmark lz78-rans corpus.bin 5
marc_benchmark lz78-tans corpus.bin 5
marc_benchmark lzw corpus.bin 5
marc_benchmark lzw-blocked-huffman corpus.bin 5
marc_benchmark lzw-adaptive-huffman corpus.bin 5
marc_benchmark lzw-dynamic-range corpus.bin 5
marc_benchmark lzw-rans corpus.bin 5
marc_benchmark lzw-tans corpus.bin 5
marc_benchmark lzd corpus.bin 5
marc_benchmark lzd-blocked-huffman corpus.bin 5
marc_benchmark lzd-adaptive-huffman corpus.bin 5
marc_benchmark lzd-dynamic-range corpus.bin 5
marc_benchmark lzd-rans corpus.bin 5
marc_benchmark lzd-tans corpus.bin 5
marc_benchmark lzmw corpus.bin 5
marc_benchmark lzmw-blocked-huffman corpus.bin 5
marc_benchmark lzmw-adaptive-huffman corpus.bin 5
marc_benchmark lzmw-dynamic-range corpus.bin 5
marc_benchmark lzmw-rans corpus.bin 5
marc_benchmark lzmw-tans corpus.bin 5
```

The experimental Format 2 profile is deliberately outside that stable
42-command matrix. Invoke it explicitly as
`marc_benchmark lzss-contextual-dynamic-range corpus.bin 5`.

The optional positive iteration count defaults to three. Use the same build,
input, and count when comparing codecs or revisions. Release builds are required
for meaningful throughput results.

## Measurement contract

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

## Profile configurations

### Framing baseline

`checksum-raw` is the version 1.1 framing and CRC-32C baseline. It intentionally
does not compress payload bytes; its ratio reflects the 80-byte prefix and each
frame's 56-byte header plus four-byte checksum trailer.

### Standalone entropy profiles

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

### LZ77 profiles

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

`lz77-dynamic-range` uses the same 65,536-byte raw frame as its CLI profile,
at most 1,048,576 canonical LZ77 token bytes, and the conservative `2S + 5`
payload ceiling. Checked complete-stream capacity uses a factor of 32 payload
bytes per raw byte plus one 16-byte descriptor, five termination bytes, and
one generic header per frame. The benchmark queries encoder and decoder
workspace independently through the public C ABI and verifies a complete
round trip before timing.

`lz77-rans` uses the CLI's 65,536-byte raw frame and 65,536-byte entropy block.
Capacity planning reserves sixteen canonical LZ77 token bytes per raw byte and
at most sixteen rANS blocks per frame. Thus complete-stream storage is bounded
by the 80-byte parameterized prefix, `16N` token-derived payload bytes, and
8,632 bytes per nonempty frame for the generic header, sixteen 528-byte
descriptors, and sixteen eight-byte states. The benchmark queries both
direction-specific workspaces through the public C ABI and requires a
byte-exact round trip before either direction is timed.

`lz77-tans` uses the same 65,536-byte raw frame and entropy block as its CLI
profile. Its maximum canonical LZ77 region is sixteen bytes per raw byte, and
the tANS 12-bit transition ceiling raises complete-stream payload reservation
to `24N`. Capacity is bounded by `80 + 24N + 8536K`, where each nonempty frame
reserves one 56-byte header, sixteen 528-byte descriptors, and sixteen two-byte
final states. Both direction-specific workspaces come from the public query,
and a byte-exact round trip succeeds before timing.

### LZSS profiles

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

`lzss-dynamic-range` uses the same 65,536-byte raw frame and 131,072-byte
canonical LZSS token ceiling as its CLI profile. Checked complete-stream
capacity is `80 + 4N + 77K` for input extent `N` and nonempty frame count `K`,
covering the `2S + 5` range payload, one 16-byte descriptor, and one 56-byte
generic header per frame. Both direction-specific workspace extents come from
the public C ABI, and a complete byte-exact round trip succeeds before either
direction is timed.

`lzss-rans` uses the CLI's 65,536-byte raw frame and 65,536-byte entropy
block. Capacity planning reserves two canonical LZSS token bytes per raw byte
and at most two rANS blocks per frame. Complete-stream storage is therefore
bounded by `80 + 2N + 1128K`, where `N` is raw input bytes and `K` is the
nonempty frame count; 1,128 bytes covers one generic header, two 528-byte
descriptors, and two eight-byte final states. Both direction-specific
workspaces and the opaque view alignment come from the public C ABI. A
byte-exact round trip succeeds before either direction is timed.

`lzss-tans` uses the CLI's 65,536-byte raw frame and 65,536-byte entropy
block. Capacity planning reserves at most three tANS transition bytes per raw
byte and two tANS blocks per frame. Complete-stream storage is bounded by
`80 + 3N + 1116K`, where `N` is raw input bytes and `K` is the nonempty frame
count; 1,116 bytes covers one generic header, two 528-byte descriptors, and
two two-byte final states. Both directional workspaces and opaque view
alignment come from the public C ABI. A byte-exact round trip succeeds before
either direction is timed; speed and ratio remain descriptive rather than
test thresholds.

The experimental `lzss-contextual-dynamic-range` benchmark uses 65,536-byte
raw frames, the `12F + 5` per-frame payload ceiling, and an 8-MiB internal
limit. For input extent `N` and nonempty frame count `K`, checked output
capacity is `112 + 12N + 85K`, including the Format 2 stream prefix, frame
headers, range descriptors, and termination bytes. Both directions are
constructed only through the public C lifecycle. Their primary, secondary,
and opaque views workspace extents come from separate requirements queries,
and a byte-exact round trip succeeds before timing.

### LZ78 profiles

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

`lz78-dynamic-range` uses the same 65,536-byte raw frame, 524,288-byte
canonical token ceiling, and 4-MiB active aggregate policy as its CLI profile.
Checked complete-stream capacity is `80 + 16N + 77K` for input extent `N` and
nonempty frame count `K`, covering `S <= 8N`, the `P <= 2S + 5` range payload,
one 16-byte descriptor, and one 56-byte header per frame. All three
direction-specific workspace extents and opaque alignment come from the public
C ABI, and an untimed byte-exact round trip succeeds before measurement.

`lz78-rans` uses the CLI's 65,536-byte raw frame and entropy block,
524,288-byte canonical LZ78 token ceiling, eight rANS blocks, and 4-MiB active
aggregate policy. Checked complete-stream capacity is
`80 + 8N + 4344K` for input extent `N` and nonempty frame count `K`; the
per-frame term covers one 56-byte generic header, eight 528-byte descriptors,
and eight eight-byte final states. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI. An untimed
byte-exact round trip succeeds before measurement, and no throughput floor is
applied.

`lz78-tans` uses the CLI's 65,536-byte raw frame and entropy block,
524,288-byte canonical LZ78 token ceiling, eight tANS blocks, and 4-MiB active
aggregate policy. Checked complete-stream capacity is
`80 + 12N + 4296K` for input extent `N` and nonempty frame count `K`; the
per-frame term covers one 56-byte generic header, eight 528-byte descriptors,
and eight two-byte final states. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI. An untimed
byte-exact round trip succeeds before measurement, and no throughput floor is
applied.

### LZW profiles

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

`lzw-dynamic-range` uses the CLI's 65,536-byte raw frame, maximum code width
16, 131,072-byte packed LZW ceiling, 262,149-byte range-payload ceiling, and
8-MiB active aggregate policy. Checked complete-stream capacity is
`80 + 4N + 77K` for input extent `N` and nonempty frame count `K`, covering
`S <= 2N`, `P <= 2S + 5`, one 16-byte descriptor, and one 56-byte header per
frame. Both direction-specific three-region workspaces and opaque alignment
come from the public C ABI, and an untimed byte-exact round trip succeeds
before measurement.

`lzw-rans` uses the CLI's 65,536-byte raw frame and entropy block, maximum
code width 16, 131,072-byte packed-code ceiling, two rANS blocks, and 8-MiB
active aggregate policy. Checked complete-stream capacity is
`80 + 2N + 1128K` for input extent `N` and nonempty frame count `K`; the
per-frame term covers one 56-byte generic header, two 528-byte descriptors,
and two eight-byte final states. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI. An untimed
byte-exact round trip succeeds before measurement and no throughput floor is
applied.

`lzw-tans` uses the CLI's 65,536-byte raw frame and entropy block, maximum
code width 16, 131,072-byte packed-code ceiling, two tANS blocks, and 8-MiB
active aggregate policy. Checked complete-stream capacity is
`80 + 3N + 1116K` for input extent `N` and nonempty frame count `K`; `3N`
bounds the 12-bit tANS transitions for at most `2N` packed LZW bytes, while the
per-frame term covers one 56-byte header, two 528-byte descriptors, and two
two-byte initial states. Both direction-specific three-region workspaces and
opaque alignment come from the public C ABI. An untimed byte-exact round trip
succeeds before measurement and no throughput floor is applied.

### LZD profiles

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

`lzd-dynamic-range` uses the CLI's 65,536-byte raw frame, 262,144-byte
canonical-token ceiling, 524,293-byte range-payload ceiling, 65,536-entry
dictionary policy, and 16-MiB active aggregate limit. Checked complete-stream
capacity is `80 + 16*ceil(N/2) + 77K` for input extent `N` and nonempty frame
count `K`, covering `S = 8*ceil(N/2)`, `P <= 2S + 5`, one 16-byte descriptor,
and one 56-byte header per frame. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI, and an untimed
byte-exact round trip succeeds before measurement.

`lzd-rans` uses the CLI's 65,536-byte raw frame and entropy block,
262,144-byte canonical-token ceiling, four rANS blocks, 2,112 descriptor bytes,
262,176-byte payload ceiling, 65,536-entry dictionary policy, and 16-MiB active
aggregate limit. Encoded capacity is checked as
`80 + 8*ceil(N/2) + 2200K`, retaining the absent-right half-reference for an
odd final input byte. Both direction-specific three-region workspaces and
opaque alignment come from the public C ABI. An untimed byte-exact round trip
succeeds before measurement; the benchmark then reports complete-stream ratio,
both throughputs, every workspace region, and the larger caller-owned total.

`lzd-tans` uses the CLI's 65,536-byte raw frame and entropy block,
262,144-byte canonical-token ceiling, four tANS blocks, 2,112 descriptor bytes,
393,224-byte payload ceiling, public LZD entry policy, and 16-MiB active
aggregate limit. Encoded capacity is checked as
`80 + 12*ceil(N/2) + 2176K` for input extent `N` and nonempty frame count `K`.
The first term is the parameterized stream prefix, each possible LZD reference
pair contributes at most twelve tANS transition bytes, and each frame reserves
one 56-byte header plus four 528-byte descriptors and four two-byte states.
Every run verifies an untimed byte-exact public-ABI round trip before timing,
then reports ratio, directional throughput, every queried workspace, and their
directional peak. No threshold is applied.

### LZMW profiles

`lzmw-blocked-huffman` uses the same one-MiB raw frame, 65,536-symbol entropy
block, four-byte-per-raw-byte reference bound, 64-block cap, 65,536-entry
dictionary policy, and 64-MiB active aggregate limit as the CLI. The benchmark
obtains all three region sizes and alignment from the public C ABI and verifies
a complete round trip before timing. `codec_peak_workspace_bytes` reports the
sum of caller-reserved regions, which can exceed the active aggregate policy
because the conservative maximum serialized-frame reservation coexists with
reference, raw-frame, and typed-view reservations.

`lzmw-adaptive-huffman` uses the CLI's 65,536-byte raw frame, 262,144-byte
canonical-reference ceiling, 8,650,752-byte Adaptive payload ceiling,
65,536-entry dictionary policy, and 16-MiB active aggregate limit. Capacity
planning adds one 16-byte descriptor and 56-byte frame header per nonempty
frame to the 80-byte parameterized prefix and reserves the checked
`132*raw_bytes` payload ceiling. Both direction-specific workspace extents and
opaque-view alignment come from the public C ABI, and a complete byte-exact
round trip succeeds before timing. The reported caller-reserved peak may exceed
16 MiB because conservative encoded-frame, reference, raw, phrase, and
expansion regions coexist even though active codec operations obey the
aggregate limit.

`lzmw-dynamic-range` uses the CLI's 65,536-byte raw frame, 262,144-byte
canonical-reference ceiling, 524,293-byte range-payload ceiling, 65,536-entry
dictionary policy, and 16-MiB active aggregate limit. Checked complete-stream
capacity is `80 + 8N + 77K` for input extent `N` and nonempty frame count `K`,
covering `S <= 4N`, `P <= 2S + 5`, one 16-byte descriptor, and one 56-byte
header per frame. Both direction-specific three-region workspaces and opaque
alignment come from the public C ABI, and an untimed byte-exact round trip
succeeds before measurement.

`lzmw-rans` uses the CLI's 65,536-byte raw frame and entropy block,
262,144-byte canonical-reference ceiling, four rANS blocks, 2,112 descriptor
bytes, 262,176-byte payload ceiling, 65,536-entry dictionary policy, and
16-MiB active aggregate limit. Checked complete-stream capacity is
`80 + 4N + 2200K` for input extent `N` and nonempty frame count `K`; the
per-frame term covers one 56-byte generic header, four 528-byte descriptors,
and four eight-byte final states. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI. An untimed
byte-exact round trip succeeds before measurement.

`lzmw-tans` uses the CLI's 65,536-byte raw frame and entropy block,
262,144-byte canonical-reference ceiling, four tANS blocks, 2,112 descriptor
bytes, 393,224-byte payload ceiling, 65,536-entry dictionary policy, and
16-MiB active aggregate limit. Checked complete-stream capacity is
`80 + 6N + 2176K` for input extent `N` and nonempty frame count `K`; the
per-frame term covers one 56-byte generic header, four 528-byte descriptors,
and four two-byte final states. Both direction-specific three-region
workspaces and opaque alignment come from the public C ABI. An untimed
byte-exact round trip must succeed before encode and decode are timed
separately.

## Recorded smoke measurements

These implementation-time measurements establish wiring and round-trip
correctness only. They are retained in Git introduction order and are not
performance baselines.

### BM-0001: LZSS plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,441-byte README encoded 3,390
bytes, ratio 0.763, and reported 655,493 bytes of peak caller-reserved
workspace; throughput from that small input is descriptive only.

### BM-0002: LZ78 plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,511-byte README encoded 4,630
bytes, ratio 1.026, and reported 5,832,760 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0003: LZW plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,528-byte README encoded 2,948
bytes, ratio 0.651, and reported 9,629,752 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0004: LZD plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,530-byte README encoded 4,021
bytes, ratio 0.888, and reported 17,760,316 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0005: LZMW plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,520-byte README encoded 3,870
bytes, ratio 0.856, and reported 18,415,656 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0006: LZSS plus rANS

A one-iteration MSVC Release smoke over the 4,520-byte README encoded 3,819
bytes, ratio 0.845, and reported 722,008 bytes of peak caller-reserved
workspace; throughput from that small input is descriptive only.

### BM-0007: LZ78 plus rANS

A one-iteration MSVC Release smoke over the 4,522-byte README encoded 4,984
bytes, ratio 1.102, and reported 5,836,984 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0008: LZW plus rANS

A one-iteration MSVC Release smoke over the 4,522-byte README encoded 3,396
bytes, ratio 0.751, and reported 9,630,808 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0009: LZMW plus rANS

A one-iteration MSVC Release smoke over the 4,530-byte README encoded 4,258
bytes, ratio 0.940, and reported 18,417,768 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0010: LZ78 plus tANS

A one-iteration MSVC Release smoke over the 4,581-byte README encoded 5,057
bytes, ratio 1.104, and reported 5,836,984 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0011: LZD plus tANS

A one-iteration MSVC Release smoke over the 4,581-byte README encoded 4,433
bytes, ratio 0.968, and reported 17,762,428 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0012: LZMW plus tANS

A one-iteration MSVC Release smoke over the 4,581-byte README encoded 4,309
bytes, ratio 0.941, and reported 18,417,768 bytes of peak caller reservation;
throughput from this small input is descriptive only.

### BM-0013: Experimental contextual LZSS plus Dynamic Range

A one-iteration MSVC Release smoke over the 4,326-byte README encoded 2,389
bytes, ratio 0.552, and reported 1,638,485 bytes of peak caller-reserved
workspace. The encoder reported primary/secondary/views extents of
4,326/51,997/190,344 bytes; the decoder reported
786,517/65,536/786,432 bytes. Throughput from this small input is descriptive
only, and the result does not join the stable 42-profile comparison matrix.

### BM-0014: Paired byte-stream and contextual LZSS comparison

At revision `6b1fd9b`, the existing MSVC and ClangCL Release benchmark
binaries each ran one iteration of `lzss-dynamic-range` and
`lzss-contextual-dynamic-range` over the same 4,326-byte `README.md`. Both
compilers produced exactly 3,355 bytes at ratio 0.776 for the Format 1
byte-stream profile and 2,389 bytes at ratio 0.552 for the Format 2 contextual
profile. Relative to the encoded Format 1 extent, the contextual result is
966 bytes, or approximately 28.8%, smaller.

The paired run also reports the cost of the experimental staging policy. Peak
caller-owned workspace rises from 655,493 bytes to 1,638,485 bytes. The input
is too small and the single timed iteration too coarse for a throughput claim;
this result establishes only deterministic same-input size and workspace
evidence. It is not a representative corpus result or a stable performance
baseline.

## Reporting results

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results. Smoke measurements establish wiring and correctness only.
