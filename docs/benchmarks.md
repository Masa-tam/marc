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
`marc_benchmark lzss-contextual-dynamic-range-1m corpus.bin 5`,
`marc_benchmark lzss-contextual-dynamic-range-4m corpus.bin 5`,
`marc_benchmark lzss-contextual-dynamic-range-16m corpus.bin 5`,
`marc_benchmark lzss-contextual-dynamic-range-64m corpus.bin 5`,
`marc_benchmark lzss-contextual-rans corpus.bin 5`,
`marc_benchmark lzss-contextual-rans-1m corpus.bin 5`,
`marc_benchmark lzss-contextual-rans-4m corpus.bin 5`,
`marc_benchmark lzss-contextual-rans-16m corpus.bin 5`, or
`marc_benchmark lzss-contextual-rans-64m corpus.bin 5`,
`marc_benchmark lzss-contextual-tans corpus.bin 5`,
`marc_benchmark lzss-contextual-tans-1m corpus.bin 5`,
`marc_benchmark lzss-contextual-tans-4m corpus.bin 5`,
`marc_benchmark lzss-contextual-blocked-huffman corpus.bin 5`,
`marc_benchmark lzss-contextual-blocked-huffman-1m corpus.bin 5`,
`marc_benchmark lzss-contextual-blocked-huffman-4m corpus.bin 5`, or
`marc_benchmark lzss-contextual-blocked-huffman-16m corpus.bin 5`,
`marc_benchmark lzss-contextual-blocked-huffman-64m corpus.bin 5`,
`marc_benchmark lzss-contextual-adaptive-huffman corpus.bin 5`, or
`marc_benchmark lzss-contextual-adaptive-huffman-1m corpus.bin 5`, or
`marc_benchmark lzss-contextual-adaptive-huffman-4m corpus.bin 5`, or
`marc_benchmark lzss-contextual-adaptive-huffman-16m corpus.bin 5`.

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

### Large-file LZSS match-finder mode

The internal match-finder benchmark retains its legacy one-shot Exhaustive
equivalence mode for inputs no larger than one MiB. For larger diagnostic
inputs, run:

```console
marc_lzss_match_finder_benchmark --frames hash-chain-exact input.bin 1 1048576 65536
```

Arguments after the input are optional positive iteration count, raw frame
bytes, and window bytes. Their defaults are 1, 1,048,576, and 65,536. The
current frame mode accepts `hash-chain-exact`, `binary-tree-exact`,
`hash-tree-exact`, and `sparse-hash-tree-exact`; the latter two require their
documented promotion arguments. Its default `max_internal_buffered_bytes`
remains 128 MiB.

Large global-tree experiments that cannot fit the default use a distinct,
explicitly bounded route:

```console
marc_lzss_match_finder_benchmark --frames-limited binary-tree-exact corpus.bin 1 16777216 16777216 536870912
```

`--frames-limited` accepts only `hash-chain-exact` and `binary-tree-exact`,
requires every positional argument, and replaces only the local
`max_internal_buffered_bytes`. It reports that limit and the calculator-derived
workspace. It never infers a larger policy from the window and does not change
codec configuration, stream bytes, or the ordinary `--frames` report.

The tool allocates one frame and one maximum-frame HashChain workspace, reads
the file sequentially, and resets the finder at every frame. File opening and
reading are outside the measured intervals. Each interval includes finder
initialization, workspace clearing, and parsing. One untimed pass collects
work counts and the canonical token fingerprint; timed passes disable counters
and hashing and must reproduce its byte, frame, and token totals. Corpus runners
compare the complete untimed summaries and fingerprints between Exact
strategies. This mode reports match-finder behavior only. It neither
emits nor decodes a marc stream, so it reports no compression ratio.

The diagnostic report partitions every visited HashChain candidate into a
five-byte prefix match or prefix mismatch. It separately reports byte
comparisons after the five-byte prefix and the maximum candidates visited by
one query. `hash_chain_query_depth_histogram` is a comma-separated sequence:
index 0 counts zero-candidate queries, index 1 counts one-candidate queries,
and index `n >= 2` counts queries visiting `2^(n-1)` through `2^n - 1`
candidates. Only bins through the observed maximum are printed.

### Synthetic LZSS match-finder mode

Deterministic generated inputs can be measured without storing a fixture:

```console
marc_lzss_match_finder_benchmark --synthetic hash-chain-exact equal-prefix 1048576 1 1048576 1048576
```

The optional arguments are positive input bytes, iterations, frame bytes, and
window bytes. Defaults are one MiB, one, one MiB, and one MiB. Generation is
outside timed intervals. Supported cases are:

- `zeros`: zero bytes;
- `periodic`: the absolute position modulo 251;
- `equal-prefix`: eight-byte records containing `ABCDE` and the low 24 bits of
  the record number;
- `hash-collision`: the alternating five-byte prefixes `01 00 00 58 59` and
  `00 20 00 58 59`, followed by the low 24 record-number bits; and
- `pseudorandom`: the existing fixed-seed 32-bit LCG sequence, with checked
  logarithmic jump-ahead at frame boundaries.

The collision prefixes independently produce the same low 16 hash bits under
marc's documented five-byte HashChain hash. The suffix counter prevents the
fixture from degenerating into only two indefinitely repeated records.

The complete five-case, three-window, two-strategy matrix can be generated as
one versioned local JSON document with:

```console
py -3.14 tools/run_lzss_match_finder_synthetic_matrix.py out/build/windows-clang/marc_lzss_match_finder_benchmark.exe --output out/benchmarks/lzss-match-finder-synthetic-clangcl.json --compiler "ClangCL 22.1.3" --generator Ninja --build-type Release
```

The runner generates no persistent fixture, performs no network or external
data access, validates every strategy-specific report, and rejects unequal
Exact token counts for any case/window pair. It stores the exact commands,
environment, per-run reports, and strategy/window aggregates. The output under
`out/` is an ignored local experiment artifact, not a conformance vector.

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

The experimental `lzss-contextual-dynamic-range-1m` benchmark uses the same
checked `112 + 12N + 85K` complete-stream capacity formula with 1,048,576-byte
frames and window profile 1. Its public 128 MiB aggregate limit admits the
selected exact HashChain, typed-token, modeled-operation, range-frame, and raw
decode workspaces. Both directions are constructed only through the public C
lifecycle, and the report exposes all returned regions and peak caller-owned
reservation after an exact pre-timing round trip. Run the 64 KiB and 1 MiB
commands with the same input/build/count for a meaningful profile comparison.

The experimental `lzss-contextual-dynamic-range-4m` benchmark uses
4,194,304-byte frames/windows, public selector value 2, the `14F + 5` payload
ceiling, and an explicit 256-MiB aggregate hard limit. Its checked complete-
stream capacity is `112 + 14N + 85K`. Direction-specific workspace extents
and alignment come from the public C requirements query; the benchmark does
not reproduce native staging layouts. Availability of this selector does not
by itself select a match finder or promote the profile into interoperability.
The initial one-iteration README smoke produced 2,395 bytes from 4,326 bytes
(ratio 0.554) under both local compilers and reported peak caller-owned
workspace of 113,246,293 bytes. Throughput from this short smoke is descriptive
only; use larger external data for performance conclusions.

The experimental `lzss-contextual-dynamic-range-16m` benchmark selects public
profile value 3 with 16,777,216-byte frames/windows, `14F + 5` payload ceiling,
4,582 model entries, and the helper's one-GiB aggregate policy. Its checked
complete-stream capacity remains `112 + 14N + 85K`. Actual directional
workspace extents and alignment come only from the public query. The encoder
scales with known input; the decoder conservatively reserves the full-profile
452,984,917-byte requirement before parsing an untrusted stream. The initial
one-iteration README smoke produced 2,397 bytes from 4,326 bytes (ratio 0.554)
under both local compilers and reported that decoder requirement as peak
caller-owned workspace. The registered smoke validates the complete report
and round trip; larger external data such as the verified Silesia Corpus
remains an explicit developer measurement, not a default test or repository
fixture.

The experimental `lzss-contextual-dynamic-range-64m` benchmark selects public
profile value 4 through the same configuration helper as the CLI. It uses
67,108,864-byte frames/windows, the `16F + 5` payload ceiling, 4,598 model
entries, and the helper's eight-GiB aggregate policy. The benchmark obtains
directional extents only from the public workspace query; it does not reproduce
the 4,362,600,533-byte full-frame HashChain encoder or 1,946,157,141-byte
decoder layout. The selector is descriptive and does not add a default. A
separate bounded decoder-fuzz harness and schema-53 archive admit the same
public profile without benchmark-sized workspace. The initial
one-iteration README smoke
produced 2,399 bytes from 4,326 bytes (ratio 0.555) under both local compilers
and reported the 1,946,157,141-byte decoder requirement as peak caller-owned
workspace. Throughput from this short smoke is descriptive only.

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

The experimental `lzss-contextual-rans-1m` benchmark fixes raw frames and the
LZSS window at 1,048,576 bytes and selects public window profile 1. It admits
at most `6F` decisions and `12F + 8` payload bytes under the 128 MiB aggregate
policy. Its checked complete-stream capacity for input extent `N` and nonempty
frame count `K` is `112 + 12N + 9,161K`: the per-frame term contains the
9,089-byte selected descriptor ceiling, 64-byte frame header, and 8-byte final
state. Use identical input, build, and iteration count with the unqualified
64 KiB command when comparing ratio, throughput, or workspace. Measurements
are descriptive; the exact pre-timing round trip and bounded public lifecycle
are normative.

The experimental `lzss-contextual-rans-4m` benchmark fixes raw frames and the
LZSS window at 4,194,304 bytes and selects public window profile 2. It admits
at most `7F` decisions and `14F + 8` payload bytes under the unchanged 128-MiB
aggregate policy. Its checked complete-stream capacity for input extent `N`
and nonempty frame count `K` is `112 + 14N + 9,193K`: the per-frame term
contains the 9,121-byte selected descriptor ceiling, 64-byte frame header,
and 8-byte final state. All directional workspace extents and alignment come
from the public requirements query.

The initial one-iteration README smoke produced 3,006 bytes from 4,326 bytes
(ratio 0.695) under both local compilers and reported peak caller-owned
workspace of 114,017,257 bytes. Throughput from this short smoke is descriptive
only; compare all three rANS window profiles on the same larger external input
before drawing performance conclusions.

The experimental `lzss-contextual-rans-16m` benchmark selects public profile
3, uses 16,777,216-byte frames/windows, admits `7F` decisions and `14F + 8`
payload bytes, and applies the 512-MiB aggregate policy. Its checked capacity
is `112 + 14N + 9,225K`. The public helper and direction-specific query remain
the sole resource-policy and allocation authorities.

The experimental `lzss-contextual-rans-64m` benchmark selects public profile
4 through the same helper as the CLI. It uses 67,108,864-byte frames/windows,
admits `8F` decisions, reserves `16F + 8` payload bytes, and applies the
four-GiB aggregate policy. Checked complete-stream capacity is
`112 + 16N + 9,257K`, where `N` is input bytes and `K` is nonempty frames.
The benchmark obtains all six workspace regions from public queries and
performs an untimed byte-exact round trip before measurement.

One README smoke iteration under both local compilers emitted 3,006 bytes from
4,326 bytes at ratio 0.695 and reported the exact decoder aggregate
1,946,928,169 bytes as peak caller-owned workspace. Short-input throughput is
descriptive only and is not a production-performance claim.

The experimental `lzss-contextual-tans` benchmark uses 65,536-byte raw
frames, admits at most `6F` modeled decisions, reserves `9F + 2` payload
bytes, and applies an 8-MiB internal limit. Checked complete-stream capacity
is `112 + 9N + 9,095K`: each nonempty frame reserves one 64-byte common
header, at most 9,029 descriptor bytes, and two final-state bytes. Both
directions are constructed through the public contextual-tANS C lifecycle;
the report includes all directional workspace regions after an exact
pre-timing round trip.

The experimental `lzss-contextual-tans-1m` benchmark uses 1,048,576-byte raw
frames and LZSS window, admits at most `6F` decisions, reserves `9F + 2`
payload bytes, and applies a 128-MiB internal limit. Checked complete-stream
capacity is `112 + 9N + 9,159K`: each nonempty frame reserves one 64-byte
header, at most 9,093 descriptor bytes, and two final-state bytes. Use the
same input, build, and iteration count as the 64 KiB name when comparing ratio,
throughput, or queried workspace; measurements remain descriptive.

The experimental `lzss-contextual-blocked-huffman` benchmark uses 65,536-byte
raw frames, admits at most `6F` modeled decisions, reserves `12F` payload
bytes, retains the 2,561-byte descriptor ceiling, and applies an 8-MiB
aggregate limit. Checked complete-stream capacity is
`112 + 12N + 2,625K`, including the Format 2 prefix and each common frame
header plus maximum descriptor. Both directions are constructed only through
the public C lifecycle; an exact round trip precedes timing, and the report
includes ratio, throughput, peak workspace, and all directional regions.

The experimental `lzss-contextual-blocked-huffman-1m` benchmark uses
1,048,576-byte raw frames and LZSS window, admits at most `6F` modeled
decisions, reserves `12F` payload bytes, retains the selected 2,579-byte
descriptor ceiling, and applies a 128-MiB aggregate limit. Checked complete-
stream capacity is `112 + 12N + 2,643K`. Use identical input, build, and
iteration count with the unqualified 64 KiB command when comparing ratio,
throughput, or queried workspace; measurements remain descriptive.

The experimental `lzss-contextual-adaptive-huffman` benchmark uses
65,536-byte raw frames, reserves at most one typed token per raw byte, fixes
the shared model bank at 9,067 nodes plus 4,518 symbol indices, reserves
`ceil(267F/8)` payload bytes, and applies an 8-MiB aggregate limit. Checked
complete-stream capacity is `112 + 80K + ceil(267N/8)`, including the Format 2
prefix and each 64-byte common frame header plus fixed 16-byte descriptor.
Both directions are constructed only through the public C lifecycle; an exact
round trip precedes timing, and the report includes ratio, throughput, peak
workspace, and all directional regions.

The experimental `lzss-contextual-adaptive-huffman-1m` benchmark uses
1,048,576-byte raw frames and LZSS window, fixes the selected model bank at
9,131 nodes plus 4,550 symbol indices, reserves `ceil(267F/8)` payload bytes,
and applies a 128-MiB aggregate limit. Checked complete-stream capacity is
`112 + 80K + ceil(267N/8)`. Use identical input, build, and iteration count
with the unqualified 64 KiB command when comparing ratio, throughput, or
queried workspace; measurements remain descriptive.

The experimental `lzss-contextual-adaptive-huffman-4m` benchmark selects the
same public atomic preset as the CLI: 4,194,304-byte frames and window,
9,163 nodes plus 4,566 symbol indices, `ceil(267F/8)` payload, and a 256-MiB
aggregate policy. Checked complete-stream capacity remains
`112 + 80K + ceil(267N/8)`. The benchmark adds no private sizing rule and uses
the public requirements query for every caller-owned workspace.

The experimental `lzss-contextual-adaptive-huffman-16m` benchmark selects
the same public profile value 3 as the CLI: 16,777,216-byte frames and window,
9,195 nodes plus 4,582 symbol indices, `ceil(267F/8)` payload, and a one-GiB
aggregate policy. Its checked complete-stream capacity is
`112 + 80K + ceil(267N/8)`. An exact round trip precedes timing, and all
reported workspace regions come from the public requirements query. This
application adapter changes neither the stream representation nor the
encoder-local match-finder strategy.

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

### BM-0040: Byte-oriented LZSS Adaptive Huffman HashChain frame baseline

The private HashChain route produces exactly the same 3,362-byte LZSS plus
Adaptive Huffman frame as Exhaustive for the 4,326-byte README, including the
generic 56-byte header, fixed descriptor, and bounded FGK payload.

Ten MSVC 19.51.36252 Release iterations report 0.084 MiB/s for Exhaustive and
0.136 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.111 and 0.188 MiB/s.
The remaining FGK model-update cost dominates this small input, so these values
are descriptive and not stable speedup claims or pass thresholds. Exact staged
tokens and complete frame bytes, successful strict decode, bounded finder
capacity, aggregate accounting, and atomic alias rejection are the normative
evidence.

### BM-0041: Byte-oriented LZSS Adaptive Huffman public HashChain promotion

The public `lzss-adaptive-huffman` benchmark over the 4,326-byte README emits
the unchanged 3,442-byte stream at ratio 0.796 after its streaming and C encode
routes select HashChain Exact. Encoder primary workspace remains 4,326 bytes;
secondary workspace is 377,087 bytes and now contains alignment allowance,
the exact finder, canonical dictionary staging, and complete worst-case frame.
Views remain zero. Decoder workspace and the 4,718,720-byte direction-maximum
peak remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 0.136 MiB/s encode and
0.340 MiB/s decode. ClangCL 22.1.3 reports 0.150 and 0.368 MiB/s. FGK tree
updates dominate this small input, so these timings are descriptive and not
stable pass thresholds. Exhaustive stream identity, bounded profile sizing,
stable capacity and alias rejection, and successful public round trip are the
normative evidence.

### BM-0042: Byte-oriented LZSS Dynamic Range HashChain frame baseline

The private HashChain route produces exactly the same 3,275-byte LZSS plus
Dynamic Range frame as Exhaustive for the 4,326-byte README, including the
generic 56-byte header, fixed range descriptor, and byte-oriented payload.

Ten MSVC 19.51.36252 Release iterations report 0.205 MiB/s for Exhaustive and
13.717 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.257 and
14.756 MiB/s. These small-input timings are descriptive and not stable speedup
claims or pass thresholds. Exact staged tokens and complete frame bytes,
successful strict decode, bounded finder capacity, aggregate accounting, and
atomic alias rejection are the normative evidence.

### BM-0043: Byte-oriented LZSS Dynamic Range public HashChain promotion

The public `lzss-dynamic-range` benchmark over the 4,326-byte README emits the
unchanged 3,355-byte stream at ratio 0.776 after its streaming and C encode
routes select HashChain Exact. Encoder primary workspace remains 4,326 bytes;
secondary workspace is 108,880 bytes and now contains alignment allowance, the
exact finder, canonical dictionary staging, and complete worst-case frame.
Views remain zero. Decoder workspace and the 655,493-byte direction-maximum
peak remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 7.588 MiB/s encode and
13.548 MiB/s decode. ClangCL 22.1.3 reports 10.051 and 18.444 MiB/s. These
small-input timings are descriptive and not stable pass thresholds. Exhaustive
stream identity, bounded profile sizing, stable capacity and alias rejection,
and successful public round trip are the normative evidence.

### BM-0044: Byte-oriented LZSS rANS HashChain frame baseline

The private HashChain route produces exactly the same 3,654-byte LZSS plus
rANS frame as Exhaustive for the 4,326-byte README, including its unchanged
rANS block partition, descriptors, normalized models, payloads, and generic
header.

Ten MSVC 19.51.36252 Release iterations report 0.218 MiB/s for Exhaustive and
14.430 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.303 and
19.589 MiB/s. These small-input timings are descriptive and not stable speedup
claims or pass thresholds. Exact staged tokens and complete frame bytes,
successful strict decode, bounded finder capacity, aggregate accounting, and
atomic alias rejection are the normative evidence.

### BM-0045: Byte-oriented LZSS rANS public HashChain promotion

The public `lzss-rans` benchmark over the 4,326-byte README emits the unchanged
3,734-byte stream at ratio 0.863 after its streaming and C encode routes select
HashChain Exact. Encoder primary workspace remains 4,326 bytes; secondary
workspace is 100,743 bytes and now contains alignment allowance, the exact
finder, canonical dictionary staging, and a complete worst-case frame. Views
remain zero. Decoder workspace and the 2,294,872-byte direction-maximum peak
remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 11.686 MiB/s encode and
40.755 MiB/s decode. ClangCL 22.1.3 reports 12.173 and 41.501 MiB/s. These
small-input timings are descriptive and not stable pass thresholds. Exhaustive
stream identity, bounded profile sizing, stable capacity and alias rejection,
and successful public round trip are the normative evidence.

### BM-0046: Byte-oriented LZSS tANS HashChain frame baseline

The private HashChain route produces exactly the same 3,650-byte LZSS plus
tANS frame as Exhaustive for the 4,326-byte README, including its unchanged
tANS block partition, descriptors, normalized models, transition tables,
payloads, and generic header.

Ten MSVC 19.51.36252 Release iterations report 0.210 MiB/s for Exhaustive and
6.299 MiB/s for HashChain Exact. ClangCL 22.1.3 reports 0.288 and 9.146 MiB/s.
These small-input timings are descriptive and not stable speedup claims or
pass thresholds. Exact staged tokens and complete frame bytes, successful
strict decode, bounded finder capacity, aggregate accounting, and atomic alias
rejection are the normative evidence.

### BM-0047: Byte-oriented LZSS tANS public HashChain promotion

The public `lzss-tans` benchmark over the 4,326-byte README emits the unchanged
3,730-byte stream at ratio 0.862 after its streaming and C encode routes select
HashChain Exact. Encoder primary workspace remains 4,326 bytes; secondary
workspace is 105,063 bytes and now contains alignment allowance, the exact
finder, canonical dictionary staging, and a complete worst-case frame. Views
remain zero. Decoder workspace and the 2,294,872-byte direction-maximum peak
remain unchanged.

Ten MSVC 19.51.36252 Release iterations report 4.948 MiB/s encode and
22.418 MiB/s decode. ClangCL 22.1.3 reports 6.504 and 20.831 MiB/s. These
small-input timings are descriptive and not stable pass thresholds. Exhaustive
stream identity, bounded profile sizing, stable capacity and alias rejection,
and successful public round trip are the normative evidence.

### BM-0048: 1 MiB Contextual Dynamic Range benchmark admission

One ClangCL 22 Release smoke over the 4,326-byte README emits a 2,393-byte
stream at ratio 0.553 through `lzss-contextual-dynamic-range-1m`. Encoder
primary/secondary/views workspaces are 4,326/51,997/273,184 bytes; decoder
workspaces are 12,582,997/1,048,576/12,582,912 bytes. Peak caller-owned
workspace is 26,214,485 bytes.

The tiny input contains no evidence that a distance beyond 64 KiB improves
ratio, and its single timed iteration is not a throughput claim. This smoke
establishes public-profile wiring, bounded capacity, reported workspace, and
an untimed exact round trip. Same-corpus 64 KiB/1 MiB measurements on inputs
large enough to contain distant repetition remain the useful comparison.

### BM-0049: 1 MiB Contextual rANS benchmark admission

One Release iteration over the 4,326-byte README emits 3,006 bytes at ratio
0.695 through both `lzss-contextual-rans` and
`lzss-contextual-rans-1m`. The selected 1 MiB encoder reports
primary/secondary/views workspaces of 4,326/61,073/134,752 bytes; its decoder
reports 12,592,073/1,048,576/13,344,768 bytes. Peak caller-owned workspace is
26,985,417 bytes, compared with 2,409,353 bytes for the same 64 KiB command.

The 1 MiB smoke reports 8.487/10.630 MiB/s encode/decode under MSVC Release and
13.706/14.824 MiB/s under ClangCL Release. These single-iteration, small-input
timings are descriptive and are not throughput claims. The equal encoded
extent is expected because this input cannot use a distance beyond 64 KiB.
Exact public round trip, selected workspace bounds, checked
`112 + 12N + 9,161K` capacity, and independent smoke success under both local
compilers are the normative evidence.

### BM-0050: 1 MiB Contextual tANS benchmark admission

One Release iteration over the 4,326-byte README emits 3,005 bytes at ratio
0.695 through both `lzss-contextual-tans` and
`lzss-contextual-tans-1m`. The selected 1 MiB encoder reports
primary/secondary/views workspaces of 4,326/48,093/396,896 bytes; its decoder
reports 9,446,343/1,048,576/13,107,200 bytes. Peak caller-owned workspace is
23,602,119 bytes, compared with 1,975,175 bytes for the same 64 KiB command.

The 1 MiB smoke reports 1.800/2.343 MiB/s encode/decode under MSVC Release and
1.872/2.144 MiB/s under ClangCL Release. These single-iteration, small-input
timings are descriptive and are not throughput claims. The equal encoded
extent is expected because this input cannot use a distance beyond 64 KiB.
Exact public round trip, selected workspace bounds, checked
`112 + 9N + 9,159K` capacity, strict name rejection, and independent smoke
success under both local compilers are the normative evidence.

### BM-0051: 1 MiB Contextual Blocked Huffman benchmark admission

One Release iteration over the 4,326-byte README emits 2,504 bytes through
`lzss-contextual-blocked-huffman` and 2,506 bytes through
`lzss-contextual-blocked-huffman-1m`; both round to ratio 0.579. The selected
encoder reports primary/secondary/views workspaces of 4,326/51,311/134,752
bytes. Its decoder reports 11,799,123/1,048,576/12,726,132 bytes, making the
direction-maximum caller-owned workspace 25,573,831 bytes, compared with
1,735,093 bytes for the 64 KiB command.

The selected smoke reports 9.728/14.330 MiB/s encode/decode under MSVC Release
and 10.834/16.410 MiB/s under ClangCL Release. These single-iteration,
small-input timings are descriptive and not throughput claims. The two-byte
stream difference is the selected profile's wider descriptor identity; the
fixture contains no proof of a useful distance beyond 64 KiB. Exact public
round trip, selected workspace bounds, checked `112 + 12N + 2,643K` capacity,
strict name rejection, and independent smoke success under both local
compilers are the normative evidence.

### BM-0052: 1 MiB Contextual Adaptive Huffman benchmark admission

One Release iteration over the 4,326-byte README emits 2,572 bytes at ratio
0.595 through both `lzss-contextual-adaptive-huffman` and
`lzss-contextual-adaptive-huffman-1m`. The selected encoder reports
primary/secondary/views workspaces of 4,326/144,461/289,952 bytes; its decoder
reports 34,996,304/1,048,576/12,738,108 bytes. Peak caller-owned workspace is
48,782,988 bytes, compared with 3,193,420 bytes for the 64 KiB command.

The selected smoke reports 1.136/1.808 MiB/s encode/decode under MSVC Release
and 1.247/1.785 MiB/s under ClangCL Release. These single-iteration,
small-input timings are descriptive and not throughput claims. The equal
encoded extent is expected because this fixture cannot use a distance beyond
64 KiB. Exact public round trip, selected workspace bounds, checked
`112 + ceil(267N/8) + 80K` capacity, strict name rejection, and independent
smoke success under both local compilers are the normative evidence.

### BM-0053: Bounded large-file HashChain frame runner

The strategy-explicit frame mode preserves the legacy one-shot benchmark and
processes the locally supplied 10,192,446-byte `dickens` member as ten
independent one MiB-or-shorter frames. Both MSVC and ClangCL build the modified
benchmark warning-clean and pass the unchanged one-shot smoke plus the new
multi-frame, default, empty-input, and invalid-argument smoke.

One ClangCL 22 Release diagnostic pass with a 65,536-byte window reports
2,175,668 tokens, 21,551,687 candidates, 135,323,122 byte comparisons,
786,432 workspace bytes, and 38.180 MiB/s. Changing only the window to
1,048,576 bytes reports 1,485,210 tokens, 123,501,362 candidates,
775,660,369 byte comparisons, 4,718,592 workspace bytes, and 5.680 MiB/s.
File I/O is excluded; finder initialization and workspace clearing are
included.

These single-file timings are descriptive and are not a performance threshold
or a Corpus-wide result. The current aggregate candidate counter does not
distinguish false hash positives from genuine equal-prefix candidates, so the
observed 1 MiB slowdown demonstrates the need for the next diagnostic stage
but does not establish hash collision as its cause.

### BM-0054: HashChain candidate classification rejects collision hypothesis

The optional statistics path now classifies every HashChain candidate and
records logarithmic per-query depth without changing the counter-free timed
path. The `ABCDEABCDE` hand-checkable fixture visits four candidates: one
matches the complete five-byte prefix and three are bucket false positives.
Its ten queries occupy depth bins as seven at zero, two at one, and one at
two-to-three candidates. Saturating counters expose overflow rather than
wrapping.

One ClangCL 22 Release diagnostic pass over the locally supplied
10,192,446-byte `dickens` member, using one MiB frames and a 65,536-byte
window, visits 21,551,687 candidates. Of these, 19,394,534 (89.99%) match the
five-byte prefix and 2,157,153 (10.01%) are bucket false positives. The maximum
query depth is 786 and measured throughput is 36.043 MiB/s.

Changing only the window to 1,048,576 bytes visits 123,501,362 candidates:
112,912,391 (91.43%) prefix matches and 10,588,971 (8.57%) false positives.
The maximum query depth rises to 10,864, comparisons beyond the prefix rise
from 35,983,231 to 199,553,757, and measured throughput is 5.804 MiB/s.
Therefore the large-window plateau is dominated by genuine equal-prefix chain
growth rather than hash collision. This supports evaluating an exact ordered
tree strategy; the timings remain descriptive rather than normative.

### BM-0055: Synthetic HashChain admission matrix

ClangCL 22 Release measured each deterministic one MiB input as one frame with
64 KiB, 256 KiB, and one MiB windows. Generation is excluded and each result
is one descriptive iteration.

| Case | Window | Candidates | Prefix matches | False positives | Max depth | MiB/s |
|---|---:|---:|---:|---:|---:|---:|
| zeros | 64 KiB | 4,065 | 4,065 | 0 | 1 | 350.988 |
| zeros | 256 KiB | 4,065 | 4,065 | 0 | 1 | 350.079 |
| zeros | 1 MiB | 4,065 | 4,065 | 0 | 1 | 355.859 |
| periodic | 64 KiB | 4,332 | 4,064 | 268 | 2 | 349.736 |
| periodic | 256 KiB | 4,332 | 4,064 | 268 | 2 | 351.741 |
| periodic | 1 MiB | 4,332 | 4,064 | 268 | 2 | 338.021 |
| equal-prefix | 64 KiB | 20,812,519 | 20,643,586 | 168,933 | 8,192 | 11.805 |
| equal-prefix | 256 KiB | 30,008,870 | 29,294,338 | 714,532 | 32,768 | 8.208 |
| equal-prefix | 1 MiB | 34,798,860 | 33,488,643 | 1,310,217 | 65,537 | 6.922 |
| hash-collision | 64 KiB | 24,774,824 | 12,254,418 | 12,520,406 | 8,193 | 10.420 |
| hash-collision | 256 KiB | 42,695,364 | 20,891,842 | 21,803,522 | 32,772 | 6.071 |
| hash-collision | 1 MiB | 55,921,725 | 27,164,333 | 28,757,392 | 65,546 | 4.622 |
| pseudorandom | 64 KiB | 1,014,746 | 0 | 1,014,746 | 9 | 43.905 |
| pseudorandom | 256 KiB | 3,667,908 | 0 | 3,667,908 | 17 | 18.947 |
| pseudorandom | 1 MiB | 8,386,707 | 0 | 8,386,707 | 35 | 7.825 |

Zeros and the 251-byte period quickly produce maximum-length greedy matches,
so token skipping keeps their search depth at one or two. Equal-prefix and
collision records deliberately keep many parse positions while growing the
active candidate population; both reach roughly 65K candidates in one query.
The pseudorandom control instead exposes bucket-cap collision growth with no
five-byte prefix match. BinaryTree therefore has evidence to address the
long-chain cases, but it must also prove that its ordered-key overhead does not
regress short-chain and incompressible inputs before promotion.

### BM-0056: Silesia Exact match-finder matrix

The offline runner verified all twelve locally supplied Silesia members and
measured revision `50160f00d7d343efa51cac38e9367a1682288f8d` with ClangCL
22.1.3, Ninja Release, Python 3.14.5, one iteration, one-MiB frames, and an AMD
Family 25 Model 97 processor on Windows 11. The 211,938,580 input bytes form
207 independent frames. HashChain Exact and BinaryTree Exact produced equal
token counts for every one of the 36 member/window pairs.

| Strategy | Window | Tokens | Measured seconds | MiB/s | Principal search work | Maximum query work | Workspace |
|---|---:|---:|---:|---:|---:|---:|---:|
| HashChain Exact | 64 KiB | 52,377,870 | 11.224 | 18.007 | 1,315,521,317 candidates | 47,251 candidates | 786,432 B |
| HashChain Exact | 256 KiB | 45,236,322 | 28.290 | 7.145 | 3,405,773,748 candidates | 126,159 candidates | 1,572,864 B |
| HashChain Exact | 1 MiB | 42,185,181 | 56.265 | 3.592 | 6,309,333,525 candidates | 296,876 candidates | 4,718,592 B |
| BinaryTree Exact | 64 KiB | 52,377,870 | 101.351 | 1.994 | 4,583,438,677 key comparisons | 54 nodes | 1,900,544 B |
| BinaryTree Exact | 256 KiB | 45,236,322 | 127.455 | 1.586 | 4,964,930,446 key comparisons | 60 nodes | 7,602,176 B |
| BinaryTree Exact | 1 MiB | 42,185,181 | 104.939 | 1.926 | 5,170,659,733 key comparisons | 65 nodes | 30,408,704 B |

The tree bounds query growth: its maximum height rises only from 20 to 25 and
maximum nodes per query from 54 to 65, while HashChain's worst query grows by
more than six times. That asymptotic result does not offset the current tree's
constant work. Across the three windows it performs 40.8, 48.8, and 53.5
billion finite-key byte comparisons plus 251.6, 237.9, and 172.3 million AVL
rotations. Its aggregate throughput is below HashChain at every window.

BinaryTree wins one individual comparison: the `mr` member at a one-MiB
window measures 1.38 MiB/s versus HashChain's 0.80 MiB/s. It loses the other
35 pairs, with ratios as low as approximately 0.03. The private experiment
therefore demonstrates a possible rescue path for a severely degraded long
chain, but the present AVL suffix-key representation is not suitable as the
default or as a public selectable strategy. The ignored full JSON remains the
local audit record; these aggregate values are descriptive, not thresholds.

### BM-0057: Synthetic Exact cost-isolation matrix

Revision `37aadfa3e2ef6acb0fe13f5ca123cd820049e37c` ran the five
deterministic one-MiB cases with ClangCL 22.1.3, Ninja Release, one iteration,
one-MiB frames, and the three standard windows. All fifteen HashChain and
BinaryTree pairs produced equal token counts. The complete 30-run matrix
finished in approximately 40 wall-clock seconds.

| Case | Window | HashChain MiB/s | BinaryTree MiB/s | Tree/chain | Hash candidates | Tree key bytes | Tree rotations |
|---|---:|---:|---:|---:|---:|---:|---:|
| zeros | 64 KiB | 351.61 | 0.53 | 0.001 | 4,065 | 4,611,701,384 | 1,540,000 |
| zeros | 1 MiB | 348.95 | 0.48 | 0.001 | 4,065 | 5,197,676,407 | 1,048,551 |
| periodic | 64 KiB | 349.21 | 0.85 | 0.002 | 4,332 | 2,579,812,438 | 1,529,983 |
| periodic | 1 MiB | 326.12 | 0.70 | 0.002 | 4,332 | 3,286,852,613 | 1,045,302 |
| equal-prefix | 64 KiB | 10.74 | 2.67 | 0.249 | 20,812,519 | 93,397,342 | 1,445,625 |
| equal-prefix | 1 MiB | 7.05 | 3.43 | 0.486 | 34,798,860 | 106,684,679 | 646,626 |
| hash-collision | 64 KiB | 10.15 | 2.64 | 0.260 | 24,774,824 | 90,885,506 | 1,448,963 |
| hash-collision | 1 MiB | 4.64 | 3.42 | 0.735 | 55,921,725 | 102,752,392 | 673,241 |
| pseudorandom | 64 KiB | 43.90 | 1.63 | 0.037 | 1,014,746 | 53,466,367 | 1,091,895 |
| pseudorandom | 1 MiB | 7.77 | 1.59 | 0.204 | 8,386,707 | 69,756,994 | 732,639 |

The omitted 256-KiB rows follow the same ordering. BinaryTree never wins this
matrix, although its relative result improves as deliberate HashChain depth
grows. The strongest case is the one-MiB hash-collision input at 73.5% of
HashChain throughput. This is consistent with the isolated `mr` win in
BM-0056, but it does not supply a safe selection threshold.

Zeros and the 251-byte period expose the decisive weakness. Greedy parsing
produces only 4,066 and 4,315 tokens, but exact future matching still advances
the finder through almost every input position. The global AVL repeatedly
compares long equal capped suffixes and maintains one node per position, while
HashChain performs only about four thousand candidate visits. Equal-prefix
and collision inputs reduce the key-byte cost to roughly 91--107 million and
make the tree more competitive, showing that the data structure's asymptotic
query bound works only after paying its unconditional ordered-maintenance
cost. Pseudorandom input confirms the same fixed-cost regression without long
equal suffixes.

The result rejects micro-tuning or direct admission of the current global AVL.
A successor experiment must first avoid repeated long-key work and unnecessary
global ordering, while preserving every active position and the Exact nearest-
distance tie-break. Timings remain descriptive and the full JSON remains below
ignored `out/` storage.

### First complete synthetic HashTree threshold measurement

The first complete default matrix ran on 2026-08-18 at revision `090a8c6`
with MSVC 19.50 from Visual Studio 18.8.2. Each of the five synthetic cases
used 1 MiB of input, one 1 MiB frame, one timed iteration, all three windows,
and all seven default thresholds. All 105 HashTree records matched their 15
HashChain baselines exactly in token count. No HashTree record exceeded its
corresponding HashChain baseline in measured throughput.

The aggregate best observed HashTree result for each window was:

| Window | HashChain MiB/s | Best threshold | HashTree MiB/s | Ratio |
| ---: | ---: | ---: | ---: | ---: |
| 65,536 | 18.08 | 1,024 | 6.05 | 0.335 |
| 262,144 | 11.34 | 1,024 | 5.36 | 0.472 |
| 1,048,576 | 8.58 | 64 | 6.78 | 0.791 |

Threshold zero promoted 96,848 to 99,679 buckets per window aggregate and
measured only 0.20 to 0.33 MiB/s. Threshold 64 routed approximately 12.4% to
12.6% of queries through trees. Threshold 256 reduced that population to
0.1% to 0.3%, while threshold 1,024 produced only six promotions per window
aggregate. Threshold 4 still caused 67,904 to 99,348 promotions at the two
larger windows. Threshold 4,096 had effectively the same route population as
1,024 and no stable timing advantage in this single-iteration experiment.

These values include finder initialization for every frame and remain
descriptive rather than performance assertions. They reject production
promotion of the current HashTree and narrow the later Silesia experiment to
thresholds 16, 64, 256, and 1,024. That set retains an early, intermediate,
late, and nearly-Chain transition regime without repeating the pathological
zero/four behavior or the redundant 4,096 route. Silesia evidence may still
reject the strategy entirely or motivate reducing its initialization and
unpromoted-route overhead before another production review.

The narrowed external-data experiment is implemented separately as
`tools/run_silesia_hash_tree_threshold_benchmark.py`:

```console
py -3.14 tools/run_silesia_hash_tree_threshold_benchmark.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/hash-tree-threshold-msvc.json --compiler "MSVC 19.50" --generator "Visual Studio 18 2026"
```

The runner verifies the complete local Corpus before launching a benchmark,
performs no network access, and measures thresholds 16, 64, 256, and 1,024 by
default. Its independent `marc-silesia-hash-tree-threshold-v1` JSON stores 36
HashChain baseline records and 144 HashTree records for the default 12-member,
3-window matrix, with separate baseline/window and threshold/window
aggregates. Every candidate must reproduce its paired baseline token count.
The full run is opt-in and its ignored result is not a CTest fixture.

### First complete Silesia HashTree threshold measurement

The first complete Silesia matrix ran on 2026-08-18 at revision `b704ca5`
with MSVC 19.50 from Visual Studio 18.8.2. The strict manifest verified all
twelve members. All 144 HashTree records matched their 36 HashChain baselines
exactly in token count and passed every report invariant.

Threshold 1,024 produced the best aggregate throughput at every window:

| Window | HashChain MiB/s | HashTree MiB/s | Ratio | Promotions | Tree queries |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 65,536 | 12.93 | 2.05 | 0.158 | 781 | 0.28% |
| 262,144 | 5.29 | 1.55 | 0.293 | 2,951 | 0.65% |
| 1,048,576 | 2.78 | 2.07 | 0.744 | 7,758 | 1.00% |

Only seven of 144 candidate records exceeded their paired baseline, all at a
1 MiB window. They belonged to three member/window groups: `mozilla` reached
1.16 times baseline at threshold 1,024, `mr` reached 1.05 times at 1,024, and
`reymont` reached 1.24 times at threshold 256. Threshold 1,024 was the fastest
HashTree setting in 35 of 36 member/window groups; `reymont` at 1 MiB was the
sole threshold-256 exception. The Corpus aggregate therefore rejects a
production threshold while confirming that selective tree search can help a
small subset of large-window inputs.

At threshold 1,024, HashTree reduced Chain candidate visits by 71.9%, 80.5%,
and 85.4% as the window grew. That useful query reduction was overwhelmed by
approximately 101.6, 124.2, and 78.5 billion maintenance key-byte comparisons.
Maximum caller-owned workspace was 3.83, 6.04, and 7.51 times the HashChain
workspace. These measurements include per-frame initialization and show that
the current ordered-key maintenance and full combined workspace, rather than
failure to reduce Chain search, are the dominant blockers.

The current HashTree remains private and must not be selected by a production
encoder. A successor experiment must reduce long-key maintenance work and
workspace before extending LZSS beyond the existing 1 MiB window. It must
retain the same Exact tokens and re-run both synthetic and Silesia evidence;
micro-tuning the promotion threshold alone is not supported by these results.

### First complete sparse HashTree pool/threshold measurement

The complete sparse HashTree matrix ran from 2026-08-21 through 2026-08-22 at
revision `a457ae2b5aaef0f571fe4fc3fea774d62e0a8a06` with MSVC
19.51.36252.0 and Visual Studio 18 2026 x64. The strict manifest verified all
twelve Silesia members. A versioned atomic checkpoint preserved each validated
point across bounded batches, and the completed checkpoint regenerated the
canonical `marc-silesia-sparse-hash-tree-v1` report without relaunching a
measurement.

The report contains 36 HashChain baselines and 432 sparse candidates: three
pool capacities (4,096, 16,384, and 65,536 nodes), four promotion thresholds
(16, 64, 256, and 1,024), and all three established windows. Every sparse
record reproduced its paired HashChain Exact token count. No sparse candidate
was the fastest strategy in any of the 36 member/window groups. Selecting the
best sparse aggregate independently at each window produced:

| Window | HashChain MiB/s | Pool | Threshold | Sparse MiB/s | Ratio | Tree queries | Workspace ratio |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 65,536 | 13.845 | 4,096 | 1,024 | 6.441 | 0.465 | 34,014 (0.065%) | 1.526 |
| 262,144 | 5.453 | 4,096 | 1,024 | 3.428 | 0.629 | 58,504 (0.129%) | 1.263 |
| 1,048,576 | 2.467 | 4,096 | 256 | 2.027 | 0.821 | 98,112 (0.233%) | 1.088 |

Across member/window groups, the best sparse candidate averaged 59.05% of its
paired HashChain throughput. The closest group was `dickens` at 1 MiB, where
sparse reached 89.62%; the widest gap was `xml` at 64 KiB, where it reached
28.12%. Pool 4,096 with threshold 1,024 was the fastest sparse configuration in
22 of 36 groups, while the 1 MiB aggregate preferred threshold 256. These are
descriptive results, not a universal tuning recommendation.

The evidence rejects the current sparse design as a production selector for
windows through 1 MiB. It does not reject sparse promotion as a future
larger-window technique: aggregate relative throughput improves from 46.5% to
82.1% and the workspace premium contracts from 52.6% to 8.8% as the window
grows. Keep the implementation private and default-disabled. A future 4 MiB or
larger-window experiment may reuse it only with an explicit new measurement
profile, the same HashChain/Exact-token oracle, bounded workspace reporting,
and no assumption that the present pool or threshold grid remains optimal.

## External Silesia measurements

### Private HashTree Exact benchmark route

`marc_lzss_match_finder_benchmark` accepts private `hash-tree-exact` in frame
and synthetic modes. Unlike the two established Exact strategies, HashTree
requires every positional setting and a final finite promotion-candidate
threshold. For example:

```console
marc_lzss_match_finder_benchmark --synthetic hash-tree-exact hash-collision 1048576 1 1048576 1048576 32
marc_lzss_match_finder_benchmark --frames hash-tree-exact benchmarks/data/silesia/corpus/dickens 1 1048576 1048576 32
```

Threshold zero promotes after the first non-empty completed Chain query;
larger values retain a bucket as Chain until one completed query visits more
than that many candidates. The report records the exact threshold, separate
Chain/Tree query distributions, promotion and population totals, and
build/query/maintenance comparison work. This is a private experiment only:
it does not select an encoder strategy, change a stream, or participate in the
current `marc-silesia-match-finder-v1` JSON runner. A versioned threshold-sweep
runner is therefore defined independently below; Silesia integration remains
a later schema change.

The independent synthetic threshold sweep is now available as
`tools/run_lzss_hash_tree_threshold_matrix.py`. It measures one HashChain
Exact baseline per case/window and then HashTree Exact at finite thresholds
0, 4, 16, 64, 256, 1024, and 4096 by default:

```console
py -3.14 tools/run_lzss_hash_tree_threshold_matrix.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output out/benchmarks/lzss-hash-tree-threshold-msvc.json --compiler "MSVC 19.50" --generator "Visual Studio 18 2026"
```

Use `python3` in place of `py -3.14` on platforms where appropriate. The
runner performs no network or external-data access. It rejects incomplete or
internally inconsistent HashTree diagnostics and rejects any HashTree token
count that differs from its HashChain baseline. Its versioned
`marc-lzss-hash-tree-threshold-synthetic-v1` output keeps baseline records,
threshold records, and threshold/window aggregates separate. The default
thresholds are descriptive experiment points, not a production default; use
`--thresholds` to supply another unique finite set. Results belong under
ignored `out/` storage.

Silesia Corpus measurements are opt-in development experiments. The Corpus is
not redistributed by marc and is never downloaded by configure, build, CTest,
or benchmark execution. Acquisition and local placement instructions are in
[`benchmarks/data/silesia/README.md`](../benchmarks/data/silesia/README.md).

Before measurement, verify all twelve direct child files by exact name,
uncompressed size, and the MD5 values published by the official Corpus page.
Record locally calculated SHA-256 values with an experiment when practical.
MD5 identifies the published input and is not an authenticity guarantee.

From the repository root, run `py -3 tools/verify_silesia_corpus.py` on
Windows or `python3 tools/verify_silesia_corpus.py` where Python uses the
`python3` command. Pass an alternative Corpus directory as the sole argument.
The verifier performs no network access and emits results only after all
twelve members pass.

Run every member as an independent input. Report per-file results and totals;
do not silently concatenate members or report only an unweighted mean of their
ratios. Corpus absence must never fail an ordinary build or CTest run. The
complete external-data and LZSS diagnostic contract is defined by
[`docs/design/silesia-benchmark-profile.md`](design/silesia-benchmark-profile.md).

These measurements remain descriptive. They may justify work on a new match
finder, but do not by themselves make a throughput number or an adaptive
strategy threshold normative.

After building `marc_lzss_match_finder_benchmark`, the complete offline matrix
can be generated on Windows with:

```console
py -3.14 tools/run_silesia_match_finder_benchmark.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/msvc.json --compiler "MSVC 19.50" --generator "Visual Studio 18 2026"
```

The runner performs the existing exact Corpus verification first, invokes no
network operation, measures both Exact strategies at 64 KiB, 256 KiB, and
1 MiB windows, and rejects any per-member token-count disagreement. Output is
local ignored JSON containing commands, environment metadata, per-member
reports, and byte-weighted aggregate throughput.

### Fixed-width HashTree workspace evidence

On 2026-08-19, revision `f567415` was built with MSVC 19.51.36252.0 and the
complete synthetic and Silesia threshold matrices were rerun after narrowing
all HashTree position arrays to `uint32_t`. The synthetic run contains 15
HashChain baselines, 105 HashTree candidates, and 21 aggregates. The Silesia
run contains 36 baselines, 144 candidates, and 12 aggregates. Every candidate
retains its paired Exact token count.

Compared with the maintenance-v2 evidence at revision `15a6c22`, all 4,455
synthetic and 6,192 Silesia report fields other than time and workspace are
identical. The new maximum HashTree workspaces are 2,228,224, 7,143,424, and
26,804,224 bytes. Against the paired HashChain workspaces these are 2.83,
4.54, and 5.68 times for 64 KiB, 256 KiB, and one MiB, replacing 3.83, 6.04,
and 7.51 times.

At threshold 1024, the same-run Silesia HashTree/HashChain throughput ratios
are 0.36, 0.60, and 1.20. The one-MiB route wins six of twelve individual
members and retains its aggregate CPU win; the two smaller windows remain
slower. The synthetic ratios are 0.36, 0.48, and 0.77. Absolute speed changes
between separate runs disagree between synthetic and Silesia, so no speed
improvement is attributed to narrower storage. This evidence establishes the
memory reduction and logical identity only; HashTree remains private.

### Exact token fingerprints

The match-finder benchmark's untimed verification pass reports literal count,
match count, matched bytes, and `token_fingerprint_sha256`. The digest covers
a nine-byte frame record before every non-empty frame and one nine-byte record
per logical token, using the canonical layout in
[`docs/design/lzss-hash-tree-match-finder.md`](design/lzss-hash-tree-match-finder.md).
It is excluded from timed passes. Empty input reports the SHA-256 digest of an
empty message.

The counters must reconstruct both token count and input extent. Exact
strategy comparisons require both token count and fingerprint equality; the
digest strengthens benchmark evidence but does not replace direct token-array
equality in bounded component tests. It is benchmark metadata only and is not
part of a marc stream or public hash contract.

### Private four-MiB HashTree experiment

Run the fixed offline Silesia experiment with:

```console
py -3.14 tools/run_silesia_hash_tree_4m_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/hash-tree-4m-msvc.json --compiler "MSVC 19.51" --generator "Visual Studio 18 2026"
```

The runner verifies the external local Corpus before launching a benchmark
and performs no network access. It measures 36 independent records: a one-MiB
HashChain control and four-MiB HashChain/HashTree Exact pair for each of twelve
members. The HashTree threshold is fixed at 1,024. Four-MiB token summaries
and fingerprints must match exactly or no result is published.

Aggregate CPU and wider-window parse-opportunity gates are reported rather
than used as process success. A negative gate is useful evidence. A positive
`eligible_for_format_design` permits only the next bounded aggregate-workspace
design; it does not reserve a variant or establish final compressed-size gain.

The 2026-08-19 MSVC 19.51.36252.0 Release measurement at revision `9de8d29`
completed all 36 records over 211,938,580 Corpus bytes. Every four-MiB
HashTree token summary and fingerprint matched its HashChain oracle. Aggregate
throughput was 1.77 MiB/s for HashTree and 0.80 MiB/s for HashChain, a ratio of
2.218. The four-MiB parse used 5,659,280 fewer tokens and covered 5,487,848
more bytes with matches than the one-MiB control. Maximum workspaces were
4,718,592 bytes for the control, 17,301,504 bytes for the oracle, and
105,447,424 bytes for the candidate.

All admission gates are positive, so aggregate-workspace design may begin.
These numbers do not select a production finder, reserve a stream variant, or
prove final compressed-size improvement. Three individual members still ran
slower with HashTree than with the same-size HashChain, and the complete tree
does not yet have a whole-encoder memory proof under the 128-MiB limit.

### Sparse HashTree pool/threshold matrix

Run the independent offline sparse matrix with:

```console
py -3.14 tools/run_silesia_sparse_hash_tree_matrix.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --output benchmarks/data/silesia/results/sparse-hash-tree-msvc.json --compiler "MSVC 19.51" --generator "Visual Studio 18 2026"
```

It verifies the complete local Corpus without network access, measures one
HashChain baseline per member/window, and checks every sparse pool/threshold
point for the same Exact token count. The default capacities are 4,096, 16,384,
and 65,536 nodes; default thresholds are 16, 64, 256, and 1,024. Capacity zero
remains available as an explicit chain-only measurement but is omitted from
the default grid because its result does not depend on the threshold.

Use `--members dickens` (or another explicit set) for a development smoke.
Member selection limits measurement only: the complete twelve-member manifest
is still verified, unknown or repeated names are rejected, and selected records
remain in canonical manifest order. The pool-zero route deliberately measures
the sparse implementation's chain-only behavior and may be slow on collision-
heavy data. Reports remain descriptive, ignored local JSON and do not select a
production strategy.

For a long matrix, pass `--checkpoint` together with `--output`. The checkpoint
is atomically replaced after each completed baseline and sparse point. An
existing checkpoint resumes automatically only when its schema, Git revision,
benchmark path and SHA-256, Corpus path and selected manifest, complete grid,
runner/dependency source SHA-256 values, and the recorded platform/build
environment match. Invalid, duplicate, out-of-grid,
or Exact-token-inconsistent records abort the run. The final report is rebuilt
in canonical grid order, and both checkpoint and final JSON remain ignored
local artifacts.

Use `--max-new-points N` with `--checkpoint` and without `--output` to execute
at most N new baseline/candidate processes. The control is deliberately absent
from checkpoint identity because batch sizes may vary between resumptions. Zero
validates and reports progress without launching a point. Each bounded run exits
successfully only at a saved record boundary; after progress reaches the planned
total, rerun without the limit and with `--output` to materialize the final v1
report from the checkpoint.

### Global BinaryTree 16 MiB comparison

The fixed global AVL comparison is run in bounded batches, for example:

```console
py -3.14 tools/run_silesia_binary_tree_16m_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/binary-tree-16m-msvc.checkpoint.json --max-new-points 2 --compiler "MSVC 19.50" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

The runner has no matrix-size arguments. It always verifies all twelve local
members, then measures a 16,777,216-byte frame with 1/4/16-MiB windows,
HashChain followed by BinaryTree Exact, one iteration, and an explicit 512-MiB
internal-buffer policy: 72 independent processes. It performs no download or
network access.

Each saved record passes the full limited-report validator. BinaryTree is
saved only after all five token-summary fields match its HashChain baseline.
The checkpoint identity includes the full revision, benchmark and dependent
source SHA-256 values, Corpus path and manifest, fixed configuration, and the
recorded build environment. `--max-new-points 0` validates a checkpoint
without launching the benchmark.

After progress reaches `72/72`, omit `--max-new-points` and add an `--output`
path to rebuild the canonical
`marc-silesia-binary-tree-16m-experiment-v1` report. It contains six
strategy/window aggregates and three tree/chain comparisons. Neither ratio
nor parse opportunity is a pass/fail gate or a production-selection rule.

The completed MSVC Release run at revision
`f8e9bc2b163708c0d33288108c1f3dde15f594d1` validated all 72 records and all
36 five-field Exact pairs. Across 211,938,580 bytes and 19 frames, aggregate
BinaryTree-to-HashChain throughput was 0.694925 at 1 MiB, 1.456408 at 4 MiB,
and 3.371567 at 16 MiB. BinaryTree won 1, 5, and 7 of the twelve members at
those windows. Aggregate token counts were 37,561,576, 34,116,898, and
33,137,395, a 9.171% reduction from 1 to 4 MiB and a further 2.871% from 4 to
16 MiB.

The result does not justify selecting by window alone: at 16 MiB, BinaryTree
won on `mr`, `nci`, `mozilla`, `reymont`, `samba`, `webster`, and `dickens`,
while HashChain won on `sao`, `osdb`, `ooffice`, `x-ray`, and `xml`.
Maximum workspaces were 4.5/29 MiB, 16.5/116 MiB, and 64.5/464 MiB for
HashChain/BinaryTree. The 16-MiB BinaryTree aggregate was also 1.28 times its
4-MiB throughput because frame and window extents were equal and the measured
tree retirement count was zero. These are descriptive results for this exact
machine, build, frame policy, and Corpus, not a new default or selector.

### Global BinaryTree 64 MiB preflight

The explicit `--frames-limited` path admits caller-supplied frame and window
sizes through the 32-bit LZ representation range, then subjects them to the
supplied aggregate hard limit and the selected checked finder calculator. The
ordinary `--frames` path and all codec defaults retain their existing 16-MiB
frame/distance and 128-MiB aggregate ceilings.

For the fixed future 64-MiB experiment, calculator-only tests require a
67,108,864-byte frame and window to report HashChain workspace 268,959,744 and
aggregate 336,068,608 bytes, and BinaryTree workspace 1,946,157,056 and
aggregate 2,013,265,920 bytes. Both fit the explicit 2-GiB policy; reducing
either exact aggregate by one byte fails without allocating the workspace.
The dedicated runner and real Corpus matrix remain separate later stages.

### Global BinaryTree 64 MiB comparison runner

Run the fixed comparison in bounded batches, for example:

```console
py -3.14 tools/run_silesia_binary_tree_64m_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/binary-tree-64m-msvc.checkpoint.json --max-new-points 1 --compiler "MSVC 19.51" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

The dedicated runner always verifies the complete local twelve-member Corpus
before launching a benchmark. It fixes the frame at 67,108,864 bytes, measures
16- and 64-MiB windows, launches HashChain before BinaryTree Exact for every
member, uses one timed iteration, and supplies the explicit 2-GiB internal-
buffer policy. The 48 records are separate processes and run sequentially.
The runner performs no download or network access.

Each report must reconstruct its input and token count, contain complete
diagnostics and finite timing, and report the exact planned workspace. At a
16-MiB window this is 67,633,152 bytes for HashChain and 486,539,264 bytes for
BinaryTree; at a 64-MiB window it is 268,959,744 and 1,946,157,056 bytes.
Every BinaryTree record must match its paired HashChain record in token,
literal, match, and matched-byte counts plus the lowercase SHA-256 token
fingerprint.

The checkpoint accepts only a canonical record prefix and binds the revision,
benchmark and tool hashes, Corpus manifest, complete fixed matrix, workspace
expectations, and recorded environment. Repeat bounded runs with the same
checkpoint until progress is `48/48`. Then omit `--max-new-points` and add:

```console
--output benchmarks/data/silesia/results/binary-tree-64m-msvc.json
```

This publishes `marc-silesia-binary-tree-64m-experiment-v1` from the validated
checkpoint without relaunching completed points. A zero-point bounded run
validates identity and progress only. The result is descriptive local evidence
and does not change a codec, format, default strategy, or public profile.

### Recorded global BinaryTree 64 MiB result

The fixed matrix completed on 2026-09-01 at revision
`e0c6dece9ea1395b9640355845fc279c589208af` using MSVC 19.51.36252.0,
Visual Studio 18 2026, x64 Release, Windows 11 build 26200, and an AMD64 Family
25 Model 97 processor. The validated final JSON contains all 48 canonical
records and has SHA-256
`a1d0cc3566ada16da64f6f8f4239e1899db0586eb5ac08bec1cec6c30a7adad9`.

Across 211,938,580 input bytes, the aggregates were:

| Window | Strategy | Time (s) | MiB/s | Tokens | Maximum workspace |
|---:|---|---:|---:|---:|---:|
| 16 MiB | HashChain Exact | 1,243.214 | 0.162579 | 32,084,817 | 67,633,152 |
| 16 MiB | BinaryTree Exact | 327.042 | 0.618026 | 32,084,817 | 486,539,264 |
| 64 MiB | HashChain Exact | 2,178.390 | 0.092784 | 31,670,034 | 268,959,744 |
| 64 MiB | BinaryTree Exact | 270.492 | 0.747233 | 31,670,034 | 1,946,157,056 |

BinaryTree produced the exact paired token summary and fingerprint for every
member and won seven of twelve members at each window. Its aggregate
throughput was 3.801 times HashChain at 16 MiB and 8.053 times HashChain at
64 MiB. Increasing the window reduced total tokens by 414,783, or 1.293%, and
increased matched-byte coverage by 0.166 percentage points. The gain was not
uniform: members smaller than 16 MiB had no new match opportunity, while the
large `mozilla`, `nci`, and `webster` members reduced token count by about
2.70%, 2.85%, and 2.98% respectively.

These one-iteration measurements justify continued investigation of an
explicit high-memory 64-MiB profile and BinaryTree implementation. They do not
justify changing the HashChain default, automatic strategy selection, or
raising limits without caller authorization. BinaryTree's approximately
1.81-GiB finder workspace remains a material cost and must stay visible in
workspace queries and profile policy.

## Reporting results

Measurements are descriptive, not stable tests. Record compiler, build type,
CPU, input provenance, input size, iteration count, and command line when
publishing results. Smoke measurements establish wiring and correctness only.

### Contextual tANS four-MiB profile

The experimental `lzss-contextual-tans-4m` benchmark selects the exact public
four-MiB window profile. It uses 4,194,304-byte raw frames/window and LZ
distance, admits `7F = 29,360,128` decisions, reserves the exact
`ceil(21F/2) + 2 = 44,040,194` payload ceiling, and retains the 128-MiB
aggregate limit. The complete-stream capacity calculation is
`112 + ceil(21N/2) + 9,191K`, where `N` is input bytes and `K` is the number
of nonempty frames. The half-byte term is evaluated with checked integer
arithmetic and no floating point.

One MSVC Release smoke iteration over the 4,326-byte README emitted 3,005
bytes at ratio 0.695. Encoder primary/secondary/views workspaces were
4,326/54,614/396,896 bytes; decoder regions were
44,049,383/4,194,304/50,855,936 bytes. Peak caller-owned workspace was the
decoder aggregate of 99,099,623 bytes. These values establish benchmark
wiring and bounded allocation, not a production-performance claim.

### Contextual Blocked Huffman four-MiB profile

The dependency-free `lzss-contextual-blocked-huffman-4m` benchmark selects
the exact public four-MiB window profile. It uses 4,194,304-byte raw frames,
window, and LZ distance; admits `7F = 29,360,128` decisions; reserves the
exact `ceil(105F/8) = 55,050,240` payload ceiling; and retains the 128-MiB
aggregate limit. Checked complete-stream capacity is
`112 + ceil(105N/8) + 2,652K`, where `N` is total input bytes and `K` is the
number of nonempty frames.

One MSVC Release smoke iteration over the 4,326-byte README emitted 2,507
bytes at ratio 0.580. Encoder primary/secondary/views workspaces were
4,326/59,431/134,752 bytes; decoder regions were
55,052,892/4,194,304/50,474,868 bytes. Peak caller-owned workspace was the
decoder aggregate of 109,722,064 bytes. These values establish public-C
wiring, exact round trip, and bounded allocation, not a stable performance
claim.

### Contextual tANS 16-MiB profile

The experimental `lzss-contextual-tans-16m` benchmark selects exact public
profile `2/5 + 1/4 + 5/2`. It uses a 16,777,216-byte frame/window/distance,
admits `7F = 117,440,512` decisions, reserves
`ceil(21F/2) + 2 = 176,160,770` payload bytes, and applies the 512-MiB
aggregate policy. Checked complete-stream capacity is
`112 + ceil(21N/2) + 9,223K`, where `N` is input bytes and `K` is the number
of nonempty frames. Configuration and all six reported workspace regions come
from the public profile helper and direction-specific query.

One MSVC Release smoke iteration over the 4,326-byte README emitted 3,005
bytes at ratio 0.695. Encoder primary/secondary/views workspaces were
4,326/54,646/396,896 bytes; decoder regions were
176,169,991/16,777,216/201,850,880 bytes. Peak caller-owned workspace was the
decoder aggregate of 394,798,087 bytes. These values establish wiring and
bounded allocation, not a production-performance claim.

### Contextual Blocked Huffman 16-MiB profile

The dependency-free `lzss-contextual-blocked-huffman-16m` benchmark selects
exact public profile `2/5 + 1/4 + 2/2`. It uses a 16,777,216-byte
frame/window/distance, admits `7F = 117,440,512` decisions, reserves
`ceil(105F/8) = 220,200,960` payload bytes, and applies the 512-MiB aggregate
policy. Checked complete-stream capacity is
`112 + ceil(105N/8) + 2,661K`, where `N` is input bytes and `K` is the number
of nonempty frames. Configuration and all six reported workspace regions come
from the public profile helper and direction-specific query.

One MSVC Release smoke iteration over the 4,326-byte README emitted 2,508
bytes at ratio 0.580. Encoder primary/secondary/views workspaces were
4,326/59,440/134,752 bytes; decoder regions were
220,203,621/16,777,216/201,469,812 bytes. Peak caller-owned workspace was the
decoder aggregate of 438,450,649 bytes. These values establish wiring and
bounded allocation, not a production-performance claim.

### BM-0058: 64 MiB Contextual rANS application admission

Revision under test added exact application selector
`lzss-contextual-rans-64m` to the CLI and dependency-free benchmark. One
Release README iteration under both MSVC and ClangCL encoded 4,326 bytes to
3,006 bytes at ratio 0.695. Encoder primary/secondary/views workspaces were
4,326/78,473/134,752 bytes; decoder primary/secondary/views workspaces were
1,073,751,081/67,108,864/806,068,224 bytes. The reported peak caller-owned
workspace was therefore the decoder aggregate of 1,946,928,169 bytes.

The benchmark used the public profile helper, direction-specific workspace
query, and streaming factory, with checked complete-stream capacity
`112 + 16N + 9,257K`. An untimed byte-exact round trip preceded measurement.
The short-input timings are descriptive wiring evidence only and establish no
performance threshold.

### BM-0059: 64 MiB Contextual tANS application admission

The dependency-free `lzss-contextual-tans-64m` benchmark selects exact public
profile `2/6 + 1/5 + 5/2`. It applies 67,108,864-byte frames, window, and
distance, the `8F` decision bound, 805,306,370-byte payload ceiling, fixed
131,072-entry table bank, and four-GiB aggregate policy through the public
profile helper. Direction-specific storage comes exclusively from the public
workspace query and factory. Checked complete-stream capacity is
`112 + ceil(21N/2) + 9,255K`, and an untimed byte-exact round trip precedes
every measurement.

One MSVC Release smoke iteration over the 4,589-byte README emitted 3,142
bytes at ratio 0.685. Encoder primary/secondary/views workspaces were
4,589/64,323/401,108 bytes; decoder regions were
805,315,623/67,108,864/805,830,656 bytes. Peak caller-owned workspace was the
1,678,255,143-byte decoder aggregate. These short-input measurements prove
application wiring and bounded allocation; they are not a performance target.

## 64-MiB Contextual Blocked Huffman application profile

The dependency-free `lzss-contextual-blocked-huffman-64m` benchmark selects
exact `2/6 + 1/5 + 2/2` through the public profile helper, workspace query,
and factory. Checked output capacity is `112 + 15N + 2,670K`. An untimed
byte-exact round trip precedes measurement, and peak caller-owned workspace
is the maximum of encoder and decoder directional totals.

One MSVC Release smoke iteration over the 4,589-byte README emitted 2,657
bytes at ratio 0.579. Encoder primary/secondary/views regions were
4,589/71,505/138,964 bytes; decoder regions were
1,006,635,630/67,108,864/805,449,588 bytes. Peak caller-owned workspace was
1,879,194,082 bytes. These short-input measurements validate application
wiring and accounting, not representative throughput or memory efficiency.
