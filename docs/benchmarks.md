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
`marc_benchmark lzss-contextual-dynamic-range corpus.bin 5`,
`marc_benchmark lzss-contextual-rans corpus.bin 5`, or
`marc_benchmark lzss-contextual-tans corpus.bin 5`,
`marc_benchmark lzss-contextual-blocked-huffman corpus.bin 5`, or
`marc_benchmark lzss-contextual-adaptive-huffman corpus.bin 5`.

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

The experimental `lzss-contextual-rans` benchmark uses the same 65,536-byte
raw frames, admits at most `6F` modeled decisions and `12F + 8` payload bytes,
and applies an 8-MiB internal limit. For input extent `N` and nonempty frame
count `K`, checked output capacity is
`112 + 12N + 9,097K`: each nonempty frame reserves one 64-byte common header,
at most 9,025 descriptor bytes, and eight final-state bytes. Both directions
are constructed only through the public C lifecycle for canonical entropy
variant 3. The report includes complete-stream ratio, both throughputs, peak
caller-owned workspace, and all three directional workspace extents after an
exact pre-timing round trip.

The experimental `lzss-contextual-tans` benchmark uses 65,536-byte raw
frames, admits at most `6F` modeled decisions, reserves `9F + 2` payload
bytes, and applies an 8-MiB internal limit. Checked complete-stream capacity
is `112 + 9N + 9,095K`: each nonempty frame reserves one 64-byte common
header, at most 9,029 descriptor bytes, and two final-state bytes. Both
directions are constructed through the public contextual-tANS C lifecycle;
the report includes all directional workspace regions after an exact
pre-timing round trip.

The experimental `lzss-contextual-blocked-huffman` benchmark uses 65,536-byte
raw frames, admits at most `6F` modeled decisions, reserves `12F` payload
bytes, retains the 2,561-byte descriptor ceiling, and applies an 8-MiB
aggregate limit. Checked complete-stream capacity is
`112 + 12N + 2,625K`, including the Format 2 prefix and each common frame
header plus maximum descriptor. Both directions are constructed only through
the public C lifecycle; an exact round trip precedes timing, and the report
includes ratio, throughput, peak workspace, and all directional regions.

The experimental `lzss-contextual-adaptive-huffman` benchmark uses
65,536-byte raw frames, reserves at most one typed token per raw byte, fixes
the shared model bank at 9,067 nodes plus 4,518 symbol indices, reserves
`ceil(267F/8)` payload bytes, and applies an 8-MiB aggregate limit. Checked
complete-stream capacity is `112 + 80K + ceil(267N/8)`, including the Format 2
prefix and each 64-byte common frame header plus fixed 16-byte descriptor.
Both directions are constructed only through the public C lifecycle; an exact
round trip precedes timing, and the report includes ratio, throughput, peak
workspace, and all directional regions.

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

### BM-0015: Contextual rANS planning audit

One MSVC Release iteration over the 300,194-byte format specification exposed
two independent costs. Before the planning correction, contextual rANS encoded
120,487 bytes at ratio 0.401 in 30.462 seconds, while contextual Dynamic Range
encoded 78,123 bytes at ratio 0.260 in 33.224 seconds. Removing four redundant
LZSS match-search passes leaves the contextual-rANS archive byte-identical and
reduces its measured encode time to 10.053 seconds, approximately 3.03 times
faster in this descriptive run.

The size loss is not in the rANS payload. Across five frames, rANS payload plus
common framing excluding model descriptors occupies 74,795 bytes, but the five
fixed descriptors add 45,260 bytes. The Dynamic Range stream carries no
equivalent static-model cost. A locally evaluated deterministic per-context
dense-or-sparse representation would reduce those descriptors to 7,330 bytes
for this input and project a complete 82,557-byte archive (ratio 0.275). For
the 4,326-byte README it projects 11,081 down to 3,006 bytes (ratio 0.695).
This estimate motivates a distinct compact entropy variant; it is not a result
for the current fixed-descriptor format.

### BM-0016: Contextual Dynamic Range planning audit

The same nested-plan audit applies to contextual Dynamic Range. Before the
correction, one MSVC Release iteration over the then 300,194-byte format
specification encoded 78,123 bytes at ratio 0.260 in 33.224 seconds. After
removing the duplicate outer frame plan, token count, and operation count, the
grown 301,947-byte specification encodes 78,627 bytes at the same displayed
ratio 0.260 in 10.132 seconds. The inputs differ slightly because the compact-
rANS design record was added between runs, so the approximately 3.28-fold time
reduction is descriptive rather than a controlled throughput claim.

Controlled compatibility evidence comes from archives produced over identical
bytes immediately before and after rebuilding the change: README SHA-256
`1F2DB1056161A353B3D5EFBF41E2A3DF09FA1F48693D7B9FBAD676F161AA1B09`
and format-specification SHA-256
`9DE67249AC75D8C8E3BEC1AF130B47799B06FFC813A8C21F5078F1B89E0B9F15`
remain unchanged.

### BM-0017: Compact contextual rANS descriptor result

One MSVC Release iteration over the 4,326-byte `README.md` confirms BM-0015's
descriptor projection exactly. Fixed contextual-rANS variant 2 encodes 11,081
bytes at ratio 2.561, while compact variant 3 encodes 3,006 bytes at ratio
0.695. The compact stream is 8,075 bytes, or approximately 72.9%, smaller than
the fixed stream. Contextual Dynamic Range remains smaller on the same input
at 2,389 bytes and ratio 0.552.

Peak caller-owned workspace changes only by the descriptor-bound difference:
2,409,380 bytes for fixed variant 2 and 2,409,353 bytes for compact variant 3.
The compact encoder reports primary/secondary/views extents of
4,326/61,009/51,912 bytes; its decoder reports
795,529/65,536/1,548,288 bytes. One small-input iteration reports encode
throughput 0.412 MiB/s and decode throughput 10.984 MiB/s, but those timings
are descriptive and are not a performance baseline or pass threshold.

### BM-0018: Contextual tANS benchmark admission

One MSVC Release iteration over the same 4,326-byte `README.md` encodes 3,005
bytes with contextual tANS, ratio 0.695. The byte-stream `lzss-tans` profile
encodes 3,730 bytes at ratio 0.862, compact contextual rANS encodes 3,006
bytes at ratio 0.695, and contextual Dynamic Range encodes 2,389 bytes at
ratio 0.552. Thus the typed contextual boundary improves this sample over
byte-stream tANS, while it does not displace contextual Dynamic Range.

Contextual tANS reports encoder primary/secondary/views extents of
4,326/48,029/314,056 bytes and decoder extents of
598,919/65,536/1,310,720 bytes, for 1,975,175 peak caller-owned bytes. The
single small-input timing reported 0.384 MiB/s encode and 2.456 MiB/s decode;
these values are descriptive only and establish neither a recommendation nor
a pass threshold.

### BM-0019: Contextual Blocked Huffman descriptor probe

The repository-owned estimator parsed the current 4,326-byte `README.md` into
2,390 typed LZSS tokens and 5,494 field operations. Its ordinary canonical
byte serialization would occupy 6,614 bytes. The provisional four-field-table
Huffman representation charges 166 descriptor bytes, 14,763 modeled-symbol
bits, 2,462 bypass bits, and 2,154 payload bytes, for 2,320 bytes total before
any future Format 2 frame overhead.

Using every active fine-grained context reduces modeled-symbol cost to 13,688
bits. Its 19 model records increase the descriptor to 673 bytes, however, so
the total grows to 2,692 bytes. Mapping the 19 active contexts onto 18 distinct
code-length vectors costs a 31-byte map and totals 2,718 bytes. Thus this input
shows 1,075 bits of contextual symbol savings but 507 bytes of additional
model description relative to four pooled tables. The result motivates
selective per-field context admission; it is not an encoded archive, corpus
result, performance measurement, or pass threshold.

### BM-0020: Selective contextual Huffman result

The strict record-repayment rule selects no override for the 4,326-byte
README, preserving BM-0019's 166-byte descriptor, 14,763 symbol bits, 2,462
bypass bits, and 2,320-byte stored estimate. No small-input regression is
introduced by merely making context tables available.

For the repository's 312,817-byte `docs/format.md`, canonical serialized LZSS
would occupy 219,133 bytes. Four pooled Huffman field tables cost 166 descriptor
bytes, 235,043 symbol bits, and 299,780 bypass bits, totaling 67,019 bytes.
Nine profitable overrides grow the descriptor to 516 bytes and reduce symbol
bits to 231,131; the unchanged bypass bits then produce a 66,880-byte estimate,
139 bytes smaller than pooled coding. Full 21-table contextualization totals
67,147 bytes, and identical-table sharing totals 67,173 bytes. This confirms
the selection mechanism on one larger repository-owned input but is neither
a corpus result nor a serialized archive or throughput measurement.

### BM-0021: Normative contextual Huffman prefix adjustment

The decoder-visible descriptor needs 16 prefix bytes rather than the probe's
eight. This uniform correction changes no selected context. The 4,326-byte
README retains zero overrides and now estimates 174 descriptor bytes plus
2,154 payload bytes, or 2,328 bytes. The 312,817-byte format specification
retains nine overrides and now estimates 524 descriptor bytes plus 66,364
payload bytes, or 66,888 bytes. Four pooled tables total 67,027 bytes and full
contextualization totals 67,155 bytes on that input. These remain entropy-body
size observations, not complete framed archives or throughput measurements.

### BM-0022: Contextual Blocked Huffman benchmark admission

One Release iteration over the 4,326-byte `README.md` produces a complete
2,504-byte Contextual Blocked Huffman archive at ratio 0.579 under both MSVC
and ClangCL. BM-0021's 2,328-byte entropy-body estimate plus the 112-byte
Format 2 stream prefix and 64-byte common frame header predicts exactly this
extent, so the public streaming adapter introduces no unaccounted payload or
descriptor bytes.

Both builds report identical workspaces: encoder primary/secondary/views are
4,326/51,293/51,912 bytes and decoder regions are
739,905/65,536/929,652 bytes, for a 1,735,093-byte peak. The single MSVC run
reports 0.461 MiB/s encode and 15.101 MiB/s decode; ClangCL reports 0.434 and
10.495 MiB/s. These small-input timings are descriptive and are neither a
performance baseline nor a pass threshold.

### BM-0023: Contextual Adaptive Huffman benchmark admission

One Release iteration over the 4,326-byte `README.md` produces a complete
2,572-byte Contextual Adaptive Huffman archive at ratio 0.595 under both MSVC
and ClangCL. This is a measured complete stream after the required untimed
round trip; the much larger checked 267-bit-per-byte capacity remains a safety
ceiling rather than a size prediction.

Both builds report identical workspaces: encoder primary/secondary/views are
4,326/144,461/206,020 bytes and decoder regions are
2,187,344/65,536/940,540 bytes, for a 3,193,420-byte peak. The single MSVC run
reports 0.339 MiB/s encode and 1.865 MiB/s decode; ClangCL reports 0.331 and
1.870 MiB/s. These small-input timings are descriptive and are neither a
performance baseline nor a pass threshold.

### BM-0024: LZSS Exact match-finder baseline

The internal match-finder benchmark first verifies that Exhaustive and
HashChain Exact produce the same 2,390 tokens and identical 6,614-byte
canonical serialization for the repository's 4,326-byte `README.md`. Both
finders receive 2,390 queries. Exhaustive inspects 4,435,045 candidates and
compares 4,643,735 byte pairs; HashChain inspects 1,092 candidates and compares
4,988 byte pairs while using 82,840 bytes of caller-owned workspace.

Three ClangCL 22.1.3 Release iterations report 0.956 MiB/s for Exhaustive
planning and 123.644 MiB/s for HashChain planning. The current complete
one-shot encoder, which performs planning and writing as two parses, reports
0.473 and 47.713 MiB/s respectively. MSVC 19.51.36252 reports 0.615 versus
95.796 MiB/s for planning and 0.303 versus 45.705 MiB/s for two-pass encoding.
These sub-millisecond HashChain and small-input timings are descriptive wiring
evidence, not stable speedup claims or pass thresholds. The work counts and
identical output are deterministic; elapsed time is not.

### BM-0025: HashChain typed-token single-pass baseline

The bounded single-pass entry produces the same 2,390 typed tokens as the
precise-capacity two-pass HashChain entry for the 4,326-byte README, while
reserving the conservative 4,326-token output capacity. Ten ClangCL 22.1.3
Release iterations report 70.887 MiB/s for the two-pass typed path and
139.237 MiB/s for the single-pass path. MSVC 19.51.36252 reports 64.503 and
130.888 MiB/s respectively.

This near-twofold result is consistent with removing one complete HashChain
parse, but the timed regions are still sub-millisecond on a small repository-
owned input. It is descriptive evidence for the integration direction, not a
stable speedup claim or threshold. Exact token equality, one finder query per
published token, conservative capacity, and atomic preflight rejection are
the normative observations.

### BM-0026: Contextual Dynamic Range HashChain frame baseline

The first complete-frame HashChain route produces exactly the same 2,277-byte
typed Contextual Dynamic Range frame as the Exhaustive route for the 4,326-byte
README. This is a private frame body rather than the complete public stream;
the measurement includes dictionary parsing, context modeling, entropy coding,
descriptor construction, and frame serialization but excludes the outer stream
prefix and public streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.304 MiB/s for the Exhaustive
frame and 22.025 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.440 and
27.280 MiB/s respectively. The large difference confirms that exhaustive match
search dominates this small input even after contextual entropy work, but the
timings remain descriptive and are neither stable speedup claims nor pass
thresholds. Byte identity, successful decode, bounded workspace, and atomic
failure are the normative evidence.

### BM-0027: Contextual Dynamic Range streaming HashChain promotion

Ten complete public-lifecycle iterations over the 4,326-byte README preserve
the 2,389-byte stream and ratio 0.552 after the Contextual Dynamic Range
streaming encoder moves from Exhaustive to HashChain Exact. MSVC 19.51.36252
reports 22.014 MiB/s encode and 10.761 MiB/s decode; ClangCL 22.1.3 reports
24.364 and 14.953 MiB/s. These small-input timings are descriptive, not stable
thresholds.

Encoder primary and secondary reservations remain 4,326 and 51,997 bytes.
Opaque encoder views increase from 190,344 to 273,184 bytes by adding the
exact 82,840-byte HashChain workspace. Decoder reservations and the
1,638,485-byte direction-maximum peak remain unchanged. Exact stream identity,
bounded workspace partitioning, stable failure mapping, and successful public
round trip are the normative evidence.

### BM-0028: Contextual rANS HashChain frame baseline

The private HashChain route produces exactly the same 2,894-byte Contextual
rANS frame as Exhaustive for the 4,326-byte README. This frame-body measurement
includes typed parsing, contextual event modeling, normalized descriptor
construction, reverse-order rANS payload coding, and frame serialization, but
excludes the outer stream prefix and streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.308 MiB/s for Exhaustive and
7.585 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.436 and 18.154 MiB/s.
The remaining compiler-dependent rANS cost is visible after search removal, so
these small-input values are descriptive and not stable speedup claims or pass
thresholds. Exact descriptor, payload, frame bytes, successful decode, bounded
workspace, and atomic rejection are the normative evidence.

### BM-0029: Contextual rANS streaming HashChain promotion

Ten complete public-lifecycle iterations over the 4,326-byte README preserve
the 3,006-byte canonical stream and ratio 0.695 after the streaming encoder
moves from Exhaustive to HashChain Exact. MSVC 19.51.36252 reports 8.964 MiB/s
encode and 12.358 MiB/s decode; ClangCL 22.1.3 reports 15.939 and 16.232 MiB/s.
These small-input timings are descriptive and are not stable thresholds.

Encoder primary and secondary reservations remain 4,326 and 61,009 bytes.
Opaque encoder views increase by the exact 82,840-byte finder workspace to
134,752 bytes. Decoder reservations and the 2,409,353-byte direction-maximum
peak remain unchanged. Exact stream identity, bounded workspace partitioning,
stable failure mapping, and successful public round trip are the normative
evidence.

### BM-0030: Contextual tANS HashChain frame baseline

The private HashChain route produces exactly the same 2,893-byte Contextual
tANS frame as Exhaustive for the 4,326-byte README. This frame-body measurement
includes typed parsing, contextual event modeling, normalized descriptor and
encode-table construction, tANS state coding, and frame serialization, but
excludes the outer stream prefix and streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.278 MiB/s for Exhaustive and
1.956 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.370 and 1.925 MiB/s.
The remaining table-construction and tANS coding cost dominates after search
removal; these small-input timings are descriptive and not stable speedup
claims or pass thresholds. Exact descriptor, payload, frame bytes, successful
decode, bounded table/finder workspace, and atomic rejection are the normative
evidence.

### BM-0031: Contextual tANS streaming HashChain promotion

Ten complete public-lifecycle iterations over the 4,326-byte README preserve
the 3,005-byte canonical stream and ratio 0.695 after the streaming encoder
moves from Exhaustive to HashChain Exact. MSVC 19.51.36252 reports 1.068 MiB/s
encode and 2.477 MiB/s decode; ClangCL 22.1.3 reports 1.754 and 1.229 MiB/s.
These small-input timings are descriptive and are not stable thresholds.

Encoder primary and secondary reservations remain 4,326 and 48,029 bytes.
Opaque encoder views increase by the exact 82,840-byte finder workspace to
396,896 bytes while retaining fixed encode-table staging. Decoder reservations
and the 1,975,175-byte direction-maximum peak remain unchanged. Exact stream
identity, bounded workspace partitioning, stable failure mapping, and
successful public round trip are the normative evidence.

### BM-0032: Contextual Blocked Huffman HashChain frame baseline

The private HashChain route produces exactly the same 2,392-byte Contextual
Blocked Huffman frame as Exhaustive for the 4,326-byte README. This frame-body
measurement includes typed parsing, contextual event modeling, bounded
canonical-table and descriptor construction, payload coding, and frame
serialization, but excludes the outer stream prefix and streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.303 MiB/s for Exhaustive and
8.613 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.417 and 11.748 MiB/s.
The remaining contextual frequency and Huffman-table cost is visible after
search removal; these small-input timings are descriptive and not stable
speedup claims or pass thresholds. Exact descriptor, payload, and frame bytes,
successful decode, bounded finder workspace, and atomic rejection are the
normative evidence.

### BM-0033: Contextual Blocked Huffman streaming HashChain promotion

Ten complete public-lifecycle iterations over the 4,326-byte README preserve
the 2,504-byte canonical stream and ratio 0.579 after the streaming encoder
moves from Exhaustive to HashChain Exact. MSVC 19.51.36252 reports 9.456 MiB/s
encode and 14.567 MiB/s decode; ClangCL 22.1.3 reports 11.269 and 16.768 MiB/s.
These small-input timings are descriptive and are not stable thresholds.

Encoder primary and secondary reservations remain 4,326 and 51,293 bytes.
Opaque encoder views increase by the exact 82,840-byte finder workspace to
134,752 bytes. Decoder reservations and the 1,735,093-byte direction-maximum
peak remain unchanged. Exact stream identity, bounded workspace partitioning,
stable failure mapping, and successful public round trip are the normative
evidence.

### BM-0034: Contextual Adaptive Huffman HashChain frame baseline

The private HashChain route produces exactly the same 2,460-byte Contextual
Adaptive Huffman frame as Exhaustive for the 4,326-byte README. This frame-body
measurement includes typed parsing, contextual event mapping, bounded FGK
model updates, payload coding, and frame serialization, but excludes the outer
stream prefix and streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.246 MiB/s for Exhaustive and
1.126 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.334 and 1.153 MiB/s.
The remaining adaptive model-update cost is visible after search removal;
these small-input timings are descriptive and not stable speedup claims or
pass thresholds. Exact descriptor, payload, and frame bytes, successful
decode, bounded finder workspace, and atomic rejection are the normative
evidence.

### BM-0035: Contextual Adaptive Huffman streaming HashChain promotion

Ten complete public-lifecycle iterations over the 4,326-byte README preserve
the 2,572-byte canonical stream and ratio 0.595 after the streaming encoder
moves from Exhaustive to HashChain Exact. MSVC 19.51.36252 reports 1.155 MiB/s
encode and 1.870 MiB/s decode; ClangCL 22.1.3 reports 1.227 and 1.899 MiB/s.
These small-input timings are descriptive and are not stable thresholds.

Encoder primary and secondary reservations remain 4,326 and 144,461 bytes.
Opaque encoder views increase from 206,020 to 288,864 bytes by appending the
exact 82,840-byte finder workspace after required alignment. Decoder
reservations and the 3,193,420-byte direction-maximum peak remain unchanged.
Exact stream identity, bounded workspace partitioning, stable failure mapping,
and successful public round trip are the normative evidence.

### BM-0036: Standalone LZSS HashChain frame baseline

The private HashChain route produces exactly the same 6,670-byte entropy-none
LZSS frame as Exhaustive for the 4,326-byte README. This measurement includes
canonical token planning and serialization plus the complete 56-byte frame
header, but excludes the outer stream prefix and streaming lifecycle.

Ten MSVC 19.51.36252 Release iterations report 0.219 MiB/s for Exhaustive and
42.633 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.302 and
42.984 MiB/s. These small-input timings are descriptive and not stable speedup
claims or pass thresholds. Exact header, payload, and frame bytes, successful
decode, bounded finder workspace, complete-frame aggregate accounting, and
atomic rejection are the normative evidence.

### BM-0037: Standalone LZSS public HashChain promotion

The public `lzss` benchmark over the 4,326-byte README retains the exact
6,750-byte stream after selecting HashChain Exact in the streaming and C encode
routes. Encoder primary workspace remains 4,326 bytes; secondary workspace
increases to 91,555 bytes because it now contains the exact aligned finder and
complete worst-case frame. Views remain zero. Decoder reservations and the
3,145,784-byte direction-maximum peak remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 28.401 MiB/s encode and
136.881 MiB/s decode. ClangCL 22.1.3 reports 35.667 and 70.523 MiB/s. These
small-input timings are descriptive, not stable pass thresholds. Exact stream
identity, bounded profile sizing, stable capacity and alias failure, and public
round trip are the normative evidence.

### BM-0038: Byte-oriented LZSS Blocked Huffman HashChain frame baseline

The private HashChain route produces exactly the same 3,403-byte LZSS plus
Blocked Huffman frame as Exhaustive for the 4,326-byte README, including the
generic 56-byte header, Blocked Huffman descriptors, and entropy payload.

Ten MSVC 19.51.36252 Release iterations report 0.212 MiB/s for Exhaustive and
10.974 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.294 and
14.607 MiB/s. These small-input timings are descriptive, not stable speedup
claims or pass thresholds. Exact staged tokens and frame bytes, bounded finder
capacity, complete aggregate accounting, alias rejection, and unchanged decode
are the normative evidence.

### BM-0039: Byte-oriented LZSS Blocked Huffman public HashChain promotion

The public `lzss-blocked-huffman` benchmark over the 4,326-byte README emits a
3,483-byte stream at ratio 0.805 after its streaming and C encode routes select
HashChain Exact. Encoder primary and secondary workspaces are 4,326 and 17,376
bytes, and the direction-dependent opaque views region now reserves 82,840
bytes for the finder. Decoder workspace and the 8,389,872-byte
direction-maximum peak remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 9.904 MiB/s encode and
10.713 MiB/s decode. ClangCL 22.1.3 reports 9.581 and 13.364 MiB/s. These
small-input timings are descriptive, not stable pass thresholds. Exhaustive
stream identity, bounded profile sizing, stable capacity and alias rejection,
and successful public round trip are the normative evidence.

## Reporting results

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results. Smoke measurements establish wiring and correctness only.
