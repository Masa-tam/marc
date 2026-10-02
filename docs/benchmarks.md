# Benchmarks

## Private context-9 whole-stream measurement

With `MARC_BUILD_BENCHMARKS=ON` and a static library, build
`marc_lzss_position_distance_stream_benchmark`. Example:

```console
marc_lzss_position_distance_stream_benchmark corpus.bin 65536 3 indexed 3 new-stream.marc
```

Arguments are input, frame bytes (1..65536), fixed match eligibility (3..5),
search (`indexed` or `reference`), iterations (1..10), and a new output path.
It reads the entire input, allows at most 64 MiB and 1,024 frames, and caps the
planned archive buffer at 128 MiB. An existing output file is rejected. Every
iteration strictly reconstructs the original bytes and checks archive SHA-256
determinism before the final archive is saved. It uses private context 1/9;
the public CLI does not yet decode this artifact.

`plan_seconds` measures the external sizing pass. Each encode timing includes
the writer's internal full planning pass and frame generation. Each decode
timing includes validation and reconstruction passes. I/O, allocation, hashing
and byte comparisons are excluded. Scratch byte fields report supplied spans;
input/archive/restored buffers are separate. These values exclude allocator
overhead and internal stack state and are not peak RSS. Fixed eligibility is
not the previous per-frame size selector, so compare parsing policies explicitly.

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
`marc_benchmark lzss-contextual-adaptive-huffman-16m corpus.bin 5`, or
`marc_benchmark lzss-contextual-adaptive-huffman-64m corpus.bin 5`.

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
current frame mode accepts `hash-chain-exact`, `binary-tree-exact`, private
experimental `red-black-tree-exact` and `scapegoat-tree-exact`,
`hash-tree-exact`, and `sparse-hash-tree-exact`; the latter two require their
documented promotion arguments. Private tree availability here is diagnostic
only and does not add a public codec selector. Its default
`max_internal_buffered_bytes` remains 128 MiB.

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

The complete five-case, three-window, three-strategy matrix can be generated as
one versioned local JSON document with:

```console
py -3.14 tools/run_lzss_match_finder_synthetic_matrix.py out/build/windows-clang/marc_lzss_match_finder_benchmark.exe --output out/benchmarks/lzss-match-finder-synthetic-clangcl.json --compiler "ClangCL 22.1.3" --generator Ninja --build-type Release
```

The strategies are public `hash-chain-exact`, public AVL
`binary-tree-exact`, and private experimental `red-black-tree-exact`.
Red-Black is accepted only by this synthetic route; it is not a public codec
selector or a Silesia-runner strategy. The runner generates no persistent
fixture, performs no network or external data access, validates every
strategy-specific report, and rejects unequal Exact token counts or lowercase
SHA-256 token fingerprints for any case/window set. It stores the exact
commands, environment, per-run reports, and strategy/window aggregates. The
output under `out/` is an ignored local experiment artifact, not a conformance
vector.

The Red-Black diagnostic pass reports rotations, recolorings, insertion and
removal fix-up work, and `red_black_tree_maximum_final_height`. The latter is
the greatest exact final tree height among frames, measured by an untimed
parent-link traversal after parsing. It is deliberately not named maximum
lifetime height: maintaining that value would add metadata and writes to the
timed candidate, while rescanning after every mutation would make diagnostics
quadratic. Timed passes disable all counters, validation, fingerprinting, and
height traversal.

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

The experimental `lzss-contextual-adaptive-huffman-64m` benchmark selects
the same public profile value 4 as the CLI: 67,108,864-byte frames and window,
9,227 nodes plus 4,598 symbol indices, `ceil(267F/8)` payload, and an
eight-GiB aggregate policy. Checked complete-stream capacity remains
`112 + 80K + ceil(267N/8)`. An exact round trip precedes timing, and all
reported workspace regions come from the public requirements query. The
application adds no private sizing rule or alternate match-finder behavior.

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

For experiments above the default 16-MiB frame/distance and 128-MiB internal
buffer limits, use the separate explicit-policy form:

```console
marc_lzss_match_finder_benchmark --frames-limited sparse-hash-tree-exact INPUT 1 67108864 4194304 4096 64 536870912
```

The final three numeric arguments are pool nodes, promotion-candidate
threshold, and maximum internal buffered bytes. This form does not change the
ordinary `--frames` defaults or infer a memory policy from the input stream.
Invalid capacities and limits are rejected before measurement. The fixed
4/16/64-MiB reevaluation is specified independently in
[`docs/design/lzss-sparse-hash-tree-large-window-experiment.md`](design/lzss-sparse-hash-tree-large-window-experiment.md).

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

### BM-0060: 64 MiB Contextual Adaptive Huffman application admission

The dependency-free `lzss-contextual-adaptive-huffman-64m` benchmark selects
exact public profile `2/6 + 1/5 + 1/2` through the profile helper, workspace
query, and factory. Checked output capacity is
`112 + 80K + ceil(267N/8)`, and an untimed byte-exact round trip precedes
measurement.

One MSVC Release smoke iteration over the 4,624-byte README emitted 2,690
bytes at ratio 0.582. Encoder primary/secondary/views regions were
4,624/154,406/296,352 bytes; decoder regions were
2,239,758,416/67,108,864/805,463,196 bytes. Peak caller-owned workspace was
the exact 3,112,330,476-byte decoder aggregate. These short-input measurements
validate application wiring and accounting; they are not a performance
target.

### BM-0061: Fixed AVL/Red-Black Silesia experiment

The private Red-Black match finder is compared with AVL through a dedicated,
network-free runner:

```console
py -3.14 tools/run_silesia_red_black_tree_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/red-black-tree-msvc.checkpoint.json --max-new-points 2 --compiler "MSVC 19.50" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

The runner fixes one-MiB frames, 64-KiB/256-KiB/one-MiB windows, one measured
iteration, and AVL followed by Red-Black for every one of the twelve verified
Corpus members. Thus the complete matrix contains 72 independent processes.
Repeated invocations resume the identity-bound checkpoint; omit
`--max-new-points` and add `--output` only for a complete uninterrupted run.

Each pair must have identical token-kind counts, matched-byte count, and token
fingerprint. Results aggregate throughput, workspace, tree work, query depth,
and the differently defined AVL lifetime-maximum and Red-Black final-height
fields. The experiment is descriptive and does not make Red-Black a public
strategy or default. The complete frozen contract is in
`docs/design/lzss-red-black-tree-silesia-experiment.md`.

The complete MSVC Release run at revision
`5c0055d02547d295ed49e293e619d6270a085468` processed 211,938,580 bytes per
strategy/window aggregate and passed all 36 AVL/Red-Black Exact comparisons.

| Window | AVL seconds | Red-Black seconds | AVL MiB/s | Red-Black MiB/s | RB/AVL |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 65,536 | 109.495318 | 110.335153 | 1.845927 | 1.831877 | 0.992388 |
| 262,144 | 135.994612 | 140.092370 | 1.486238 | 1.442765 | 0.970750 |
| 1,048,576 | 113.891956 | 120.891013 | 1.774668 | 1.671922 | 0.942104 |

Workspace was equal at 1,900,544, 7,602,176, and 30,408,704 bytes. Red-Black
used 1.113131, 1.146198, and 1.243356 times AVL's key-byte comparisons as the
window grew. Member-level wins demonstrate data dependence, but the aggregate
result does not support public promotion. The checkpoint and complete JSON
remain ignored local measurement artifacts; the fixed conditions and
aggregate evidence are recorded here.

### BM-0062: Private Scapegoat synthetic comparison

Stage 9 adds Scapegoat Exact only to the network-free synthetic runner. Each
case/window/strategy point is a fresh process, and every HashChain, AVL,
Red-Black, and Scapegoat result must have identical token count and canonical
token fingerprint. The matrix now includes a deletion-heavy case that must
produce physical Scapegoat retirement.

A local MSVC Release evidence run used all six cases, 65,536 bytes per case,
32,768-byte frames, one iteration, and 1,024/4,096-byte windows: 48 processes
in total. Every Exact identity comparison passed. The deletion-heavy case
reported 63,488 retirements at 1,024 bytes and 57,344 at 4,096 bytes.

| Window | HashChain MiB/s | AVL MiB/s | Red-Black MiB/s | Scapegoat MiB/s | Scapegoat workspace |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1,024 | 28.937418 | 2.099300 | 1.716766 | 0.742282 | 36,864 |
| 4,096 | 23.250047 | 1.635348 | 1.236656 | 0.536947 | 147,456 |

Across the six cases Scapegoat performed 102,029 and 55,554 subtree rebuilds,
reached final heights 23 and 27, and observed maximum single-update structural
work of 3,259 and 14,989 node visits respectively. These deliberately small
synthetic measurements show substantial rebuild cost and do not support
promotion. They are diagnostic baseline evidence only; the fixed local
Silesia comparison remains required before an admission decision.

### BM-0063: Fixed Scapegoat Silesia runner

The dedicated runner executes the frozen 108-record matrix in canonical
member/window/AVL/Red-Black/Scapegoat order. It verifies the local Corpus,
validates complete reports, requires both candidate strategies to match AVL's
Exact token summary and fingerprint, and saves every accepted process to an
identity-bound atomic checkpoint. It does not download data and does not make
an admission decision.

The initial MSVC Release connection smoke completed the first three records:
`dickens`, 65,536-byte window, and all three strategies. Exact identity passed
and the checkpoint reached `3/108`. This confirms the real executable, Corpus,
validator, identity gates, and resume path; it is not a performance result.

Run bounded batches by retaining `--checkpoint` and choosing a suitable
`--max-new-points`. Once all 108 records exist, rerun without that bound and
add `--output` to write the final aggregate JSON. Checkpoint and result files
belong under the ignored local Corpus results area and must not be committed.

### BM-0064: Fixed Scapegoat Silesia result

The fixed MSVC Release run at commit `29054552` completed twelve verified
Silesia members, three windows, and three process-isolated strategies: 108
records. All Red-Black and Scapegoat candidates matched their immediately
preceding AVL baseline in token counts, matched bytes, and canonical token
fingerprint. Each aggregate processed 211,938,580 bytes in 207 one-MiB frames.

| Window | AVL MiB/s | Red-Black MiB/s | Scapegoat MiB/s | RB/AVL | Scapegoat/AVL | AVL workspace | Scapegoat workspace | Scapegoat max update |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 65,536 | 1.751106 | 1.744737 | 1.062158 | 0.996363 | 0.606564 | 1,900,544 | 2,359,296 | 197,017 |
| 262,144 | 1.423704 | 1.388077 | 0.839236 | 0.974976 | 0.589474 | 7,602,176 | 9,437,184 | 471,595 |
| 1,048,576 | 1.700025 | 1.581188 | 0.950363 | 0.930097 | 0.559029 | 30,408,704 | 37,748,736 | 872,473 |

Scapegoat used 1.241379 times AVL workspace at every size while falling from
60.66% to 55.90% of AVL throughput as the window increased. Its maximum final
height rose from 35 to 41 and maximum single-update structural work rose from
197,017 to 872,473 nodes. The fixed result therefore rejects public promotion
for the tested balance policy. The private implementation and runners remain
available for research; neither correctness nor experiment completion is
treated as evidence of a useful public trade-off. The ignored result JSON has
SHA-256
`754333796855d7a7fa9e01e569fa64e8d1c1cd2b391e3c230aa8ff137af72214`.

### BM-0065: Private WAVL Exact benchmark adapter

The dependency-free match-finder benchmark accepts `wavl-tree-exact` only in
its experimental frame and synthetic routes. WAVL remains absent from the
production match-finder strategy, codec CLI, C ABI, profiles, format, and
interoperability schema.

The adapter performs an untimed diagnostic pass that validates the final WAVL
tree and hashes the canonical typed-token sequence. Only finder initialization,
Exact parsing, and advancement are timed; allocation, input generation or file
I/O, validation, height traversal, diagnostics, and hashing are excluded.
Every comparison fixes the same input, frame bytes, window bytes, maximum match
length, beneficial-match policy, and iteration count. AVL, Red-Black, and WAVL
must agree on token count, literal and match totals, matched bytes, and token
fingerprint before a measurement is usable. Each strategy receives its own
exact checked workspace rather than an artificially equal allocation.

WAVL reports workspace and throughput together with query comparisons, LCP
work, promotions, demotions, insertion/removal rotations, fix-up steps,
bounded removal-preflight visits, final height, and query-depth distribution.
The repository smoke test covers both a file-frame case and the
`deletion-heavy` case with a 4,096-byte frame and 1,024-byte window. It asserts
identity and retirement activity, but intentionally asserts no speed winner.
A fixed process-isolated synthetic measurement is the next admission gate;
ordinary Silesia measurement remains deferred until WAVL passes that gate.

### BM-0066: Fixed WAVL synthetic experiment runner

The dedicated network-free runner freezes the first WAVL admission gate at
six deterministic cases, 65,536 input bytes, 32,768-byte frames, 1,024- and
4,096-byte windows, one iteration, and AVL/Red-Black/WAVL order. The complete
matrix is 36 independent processes. Each candidate must match its preceding
AVL baseline in token, literal, match, and matched-byte counts and canonical
token fingerprint; deletion-heavy records must also prove physical retirement.

Use an ignored checkpoint for bounded batches:

```console
py -3.14 tools/run_lzss_wavl_tree_synthetic_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --checkpoint benchmarks/data/silesia/results/wavl-tree-synthetic-msvc.checkpoint.json --max-new-points 3 --compiler "MSVC 19.50" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

After all points exist, omit `--max-new-points` and add `--output` to emit the
complete aggregate. The checkpoint is content-bound and accepts only a
canonical prefix, so interruption cannot silently mix revisions, binaries,
runner sources, environments, or configurations. This entry records the
measurement contract and runner only; it contains no performance result and
makes no public-admission decision.

The initial MSVC Release connection smoke accepted the canonical first three
records: `zeros`, 1,024-byte window, and AVL/Red-Black/WAVL order. Exact token
identity passed and the checkpoint reached `3/36`. This validates the real
executable, parser, checkpoint, and comparison gate; it is not a performance
result.

### BM-0067: Fixed WAVL synthetic result

The complete MSVC Release run at commit `ac253b51` finished all 36
process-isolated records. Every Red-Black and WAVL candidate matched its AVL
baseline in token, literal, match, and matched-byte counts and canonical token
fingerprint. Each strategy/window aggregate processed 393,216 bytes across
the six fixed cases, and all deletion-heavy reports exercised retirement.

| Window | AVL MiB/s | Red-Black MiB/s | WAVL MiB/s | RB/AVL | WAVL/AVL | WAVL/RB | Workspace |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1,024 | 1.771534 | 1.369793 | 0.926354 | 0.773224 | 0.522911 | 0.676273 | 29,696 |
| 4,096 | 1.361517 | 1.005526 | 0.749954 | 0.738534 | 0.550822 | 0.745832 | 118,784 |

WAVL and AVL workspace are equal and both reach maximum final heights 13 and
15; Red-Black reaches 17 and 21. WAVL removal repair remains bounded at five
or six steps and preflight at 18 or 19 nodes. However, WAVL performs
10,278,461 and 10,152,161 key comparisons, 1.689 and 1.444 times AVL, plus
2,566,585 and 1,992,092 deletion-preflight visits. Its key-byte comparison
ratios against AVL are 1.883 and 1.797. Even the deletion-heavy case reaches
only approximately 68.5% and 74.0% of AVL throughput.

This fixed early gate therefore rejects public promotion and stops before an
ordinary Silesia run. The private component and runner remain as bounded,
reproducible negative-result evidence. The ignored result JSON has SHA-256
`8294204b2cdd3b82c63f36ed132d4f77aa0278dbdd8fb3700cc502dd388ec4ea`.

### BM-0068: Fixed Sparse HashTree large-window runner

The dedicated network-free runner executes the predeclared 360-process
Silesia matrix in canonical member, window, HashChain-first, pool, and
threshold order. It validates the complete five-field Exact token identity,
all fixed workspace values, finite timing aliases, query and histogram mass,
and the invariant that each promotion trigger ends in exactly one promotion
or pool rejection. Every accepted record is saved to an atomic,
content-bound checkpoint before the next process starts.

Run bounded batches with an ignored checkpoint:

```console
py -3.14 tools/run_silesia_sparse_hash_tree_large_window_experiment.py out/build/windows-msvc/Release/marc_lzss_match_finder_benchmark.exe --corpus benchmarks/data/silesia/corpus --checkpoint benchmarks/data/silesia/results/sparse-hash-tree-large-window-msvc.checkpoint.json --max-new-points 3 --compiler "MSVC 19.50" --generator "Visual Studio 18 2026" --architecture x64 --build-label windows-msvc-release
```

Reusing `--max-new-points 0` validates the existing checkpoint without
launching a benchmark. After all 360 records exist, omit the bound and add
`--output` to write the final aggregate JSON. The result reports aggregate,
breadth, workspace-premium, and pool-pressure classifications but does not
promote a public strategy automatically. This entry records the runner and
its validation contract only; no Silesia performance result has yet been
observed.

The initial MSVC Release connection smoke at commit `08e84ec5` accepted the
canonical first three records: `dickens`, the 4-MiB window, HashChain, and the
4,096-node Sparse pool at thresholds 64 and 256. All five Exact identity
fields matched fingerprint
`2ffb93bda7d19e3469a2c2a7878ea20948a6e507bc4cf6f9d60be1023e064e1a`.
HashChain measured 0.962187 MiB/s; the two Sparse points measured 0.887574 and
0.886887 MiB/s. They recorded 605 promotions plus 15,872 pool rejections and
126 promotions plus 3,909 rejections respectively, exactly accounting for
their trigger totals. A zero-work rerun revalidated progress `3/360` without
launching a process. These values prove the executable, strict validator,
pool-pressure diagnostic, atomic checkpoint, and resume identity are connected;
they are not a performance conclusion.

### BM-0069: Fixed Sparse HashTree large-window result

The complete MSVC Release experiment at commit `5c3106e6` finished all 360
process-isolated records: twelve verified Silesia members, three windows, one
HashChain baseline per member/window, and nine Sparse pool/threshold
conditions. Every candidate matched its baseline in token, literal, match,
and matched-byte counts and canonical token fingerprint. Zero-work resume
validation accepted the complete canonical checkpoint without launching a
process.

The best aggregate Sparse condition at every window was pool 4,096 and
threshold 64:

| Window | HashChain MiB/s | Sparse MiB/s | Sparse / HashChain | Sparse wins | Workspace ratio | Pool rejections |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 0.435927 | 0.393705 | 0.903146 | 1 / 12 | 1.023911 | 213,754 |
| 16 MiB | 0.155423 | 0.146837 | 0.944754 | 1 / 12 | 1.006117 | 391,200 |
| 64 MiB | 0.094125 | 0.086622 | 0.920290 | 1 / 12 | 1.001538 | 406,314 |

None of the 27 candidates achieved aggregate or broad gain; 24 met the
predeclared low-workspace-premium threshold, but every candidate observed pool
pressure. The tested policy therefore remains private and is not added to the
selector, ABI, CLI, profile, or format. The ignored canonical result JSON has
SHA-256
`cdd526d40ef81406ec2cd87bb91799e3dc30ab400290a9152f5dfa2869ab8e95`.

### BM-0070: Manifest-driven Sparse reuse-gate runner

The dedicated runner consumes the committed
`silesia-sparse-hash-tree-reuse-gate-v1.json` manifest and executes the fixed
216-record grid from one top-level invocation. It retains one child benchmark
per record, validates all report, workspace, diagnostic, and five-field Exact
contracts before persistence, and atomically replaces its checkpoint after
each accepted point.

The checkpoint identity binds the raw manifest path and SHA-256, its strictly
parsed value, full revision, benchmark and runner-source paths and digests,
verified Corpus manifest, and platform/build environment. Only a canonical
prefix is accepted. Restarting the same command resumes the missing suffix;
a complete checkpoint rewrites the final aggregate without launching a child.
The fake benchmark gate covers all 216 points, a 3-plus-213 resume, complete
zero-work regeneration, manifest type/order/duplicate rejection, corrupt
prefix rejection, child failure atomicity, stale-output refusal, three
baseline aggregates, fifteen reuse aggregates, and twelve gated comparisons.
This entry records infrastructure only; no Silesia performance result or
public-admission decision has been made.

### BM-0071: Fixed Sparse reuse-gate result

The complete MSVC Release experiment at commit `b7d0d547` finished all 216
process-isolated records from one top-level runner invocation: twelve verified
Silesia members, three windows, one HashChain baseline, and Sparse reuse
thresholds 1, 2, 4, 8, and 16 per member/window. Every Sparse record matched
its HashChain baseline in token, literal, match, and matched-byte counts and
canonical token fingerprint. The checkpoint and final result each contain the
complete canonical 216-record sequence.

The best gated threshold at each window was:

| Window | Reuse | Sparse / HashChain | Sparse / reuse 1 | HashChain wins | Reuse-1 wins | Workspace ratio | Pool-rejection delta |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 4 | 0.848201 | 1.000882 | 0 / 12 | 8 / 12 | 1.027699 | -36,260 |
| 16 MiB | 8 | 0.847121 | 1.003037 | 0 / 12 | 9 / 12 | 1.007086 | -98,222 |
| 64 MiB | 16 | 0.872959 | 1.021011 | 0 / 12 | 12 / 12 | 1.001782 | -191,491 |

Repeated-use gating reduces pool rejection and improves the legacy reuse-one
Sparse policy most clearly at 64 MiB. It does not recover the Sparse
promotion, maintenance, and tree-query cost relative to HashChain: no gated
candidate wins any member against HashChain, and every aggregate remains at
most 0.872959 of HashChain throughput. No candidate satisfies the
predeclared aggregate and broad HashChain gain requirements, so the policy
remains private and is not added to the selector, ABI, CLI, profile, or
format. The ignored canonical result JSON has SHA-256
`d42929616c853362ce17411be986c761065472367e9fb1224e8db9cd5dcdc17d`.

### BM-0072: Manifest-driven immutable Sparse snapshot runner

The dedicated network-free runner consumes
`silesia-sparse-hash-tree-immutable-snapshot-v1.json` and owns the fixed
108-record comparison from one top-level invocation. For each Silesia member
and 4/16/64-MiB window it runs HashChain, mutable reuse-sixteen Sparse, and
immutable-snapshot Sparse in canonical order, one child process per record.

Before atomically appending a record it validates the fixed workspace and hard
limit, finite timings, query and trigger accounting, histograms, immutable
lifecycle marker and diagnostics, zero tree insertion/retirement, and all five
Exact identity fields against both prior controls. Checkpoint identity binds
the strict manifest bytes and value, revision, executable, runner dependencies,
verified corpus, and environment. A complete checkpoint regenerates the final
three aggregates and three comparisons without launching a child.

The fake-report gate covers strict manifest rejection, fixed command/grid,
snapshot diagnostic rejection, canonical-prefix and identity rejection,
4-plus-104 resume, complete zero-work rerun, child-failure atomicity, and stale
output refusal. At this infrastructure stage no Silesia result had been
observed and no strategy-admission decision was made.

### BM-0073: Immutable Sparse three-point connection smoke

The MSVC Release runner at commit `9ea8cca0` completed the first canonical
group only: `dickens`, a 4-MiB window, and HashChain, mutable reuse-sixteen
Sparse, then immutable-snapshot Sparse. All three produced 1,081,737 tokens
and fingerprint
`2ffb93bda7d19e3469a2c2a7878ea20948a6e507bc4cf6f9d60be1023e064e1a`.
A zero-new-point rerun revalidated the checkpoint at 3/108 without launching
another child.

| Strategy | Seconds | MiB/s | Relative to HashChain |
| --- | ---: | ---: | ---: |
| HashChain | 9.744228 | 0.997542 | 1.000000 |
| Mutable Sparse reuse 16 | 11.077267 | 0.877498 | 0.879661 |
| Immutable snapshot Sparse | 10.756939 | 0.903628 | 0.905855 |

The immutable candidate was 1.029779 times the mutable control on this one
member/window. It routed 21,252 of 1,081,737 queries through immutable
snapshots, observed 90 promotions and 52 expiration/bulk-release events, and
reported zero tree insertions and retirements. Workspace was 1.027699 times
HashChain. The ignored three-record checkpoint has SHA-256
`f56a7a085f9b9cad60373e9459df1890434c2921122e290bc84b2d23927ebb71`.
This is a connection and restart smoke only, not an aggregate performance or
strategy-admission result; the remaining 105 records were not started.

### BM-0074: Fixed immutable Sparse snapshot result

The complete MSVC Release experiment at commit `e92a4162` finished all 108
process-isolated records: twelve verified Silesia members, 4/16/64-MiB
windows, and HashChain, mutable reuse-sixteen Sparse, and immutable-snapshot
Sparse for every member/window. Every Sparse record matched both applicable
controls in token, literal, match, and matched-byte counts and canonical token
fingerprint. A zero-new-point rerun regenerated the same final result without
launching a benchmark process.

| Window | HashChain MiB/s | Mutable MiB/s | Snapshot MiB/s | Snapshot / HashChain | Snapshot / mutable | Wins vs HashChain | Wins vs mutable | Workspace ratio |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 0.499422 | 0.345457 | 0.362942 | 0.726724 | 1.050614 | 0 / 12 | 12 / 12 | 1.027699 |
| 16 MiB | 0.177926 | 0.128251 | 0.144318 | 0.811116 | 1.125283 | 1 / 12 | 12 / 12 | 1.007086 |
| 64 MiB | 0.097312 | 0.074360 | 0.090966 | 0.934788 | 1.223315 | 3 / 12 | 12 / 12 | 1.001782 |

The immutable lifecycle removes all steady-state tree insertion and retirement
and beats the mutable control for every member/window. Its advantage over the
mutable control grows with the window, but it does not achieve aggregate or
broad gain over HashChain at any tested window. It therefore remains private
and is not added to the selector, ABI, CLI, profile, or format. Snapshot query
counts are 142,510, 255,981, and 362,770; snapshot promotions are 1,800, 471,
and 267. Delta candidates grow to 92,199,351,781 at 64 MiB and remain a likely
cost boundary for any separately designed future hypothesis.

The ignored canonical result JSON has SHA-256
`1fb44c4d9a921302adc9ef851cd8859d3fb18db9d3c8ef5d90ad90351d930be2`.
The completed checkpoint SHA-256 is
`3401aff0f24f0cbb36954b8de9ca44df095505899992452e7c94c0b88636af5f`.

### BM-0075: Snapshot delta-budget synthetic result

The fixed MSVC Release experiment at commit `97937ec0` completed all 126
process-isolated records: six deterministic 64-MiB fixtures, 4/16/64-MiB
windows, HashChain, unbudgeted immutable snapshot, and five budgeted snapshot
candidates. Every candidate matched both controls in all five Exact identity
fields and passed budget, release, immutable-lifecycle, and workspace
accounting. A completed-checkpoint rerun regenerated the final result without
launching another benchmark child.

| Window | Budget | Candidate / unbudgeted | Candidate / HashChain | Workspace / HashChain | Structured breach fixtures | Eligible |
| ---: | ---: | ---: | ---: | ---: | ---: | :---: |
| 4 MiB | 16 | 0.989174 | 0.862395 | 1.027699 | 1 / 5 | no |
| 4 MiB | 64 | 0.989617 | 0.862782 | 1.027699 | 1 / 5 | no |
| 4 MiB | 256 | 0.990254 | 0.863337 | 1.027699 | 1 / 5 | no |
| 4 MiB | 1,024 | 0.993219 | 0.865922 | 1.027699 | 1 / 5 | no |
| 4 MiB | 4,096 | 0.994903 | 0.867391 | 1.027699 | 1 / 5 | no |
| 16 MiB | 16 | 1.038192 | 0.977873 | 1.007086 | 1 / 5 | no |
| 16 MiB | 64 | 1.040940 | 0.980462 | 1.007086 | 1 / 5 | no |
| 16 MiB | 256 | 1.037470 | 0.977193 | 1.007086 | 1 / 5 | no |
| 16 MiB | 1,024 | 0.910025 | 0.857153 | 1.007086 | 1 / 5 | no |
| 16 MiB | 4,096 | 0.769982 | 0.725246 | 1.007086 | 1 / 5 | no |
| 64 MiB | 16 | 0.946023 | 0.972098 | 1.001782 | 1 / 5 | no |
| 64 MiB | 64 | 0.943359 | 0.969361 | 1.001782 | 1 / 5 | no |
| 64 MiB | 256 | 0.932675 | 0.958383 | 1.001782 | 1 / 5 | no |
| 64 MiB | 1,024 | 0.975109 | 1.001986 | 1.001782 | 1 / 5 | no |
| 64 MiB | 4,096 | 0.982317 | 1.009393 | 1.001782 | 1 / 5 | no |

Only `shared-prefix-records` breached among the five structured fixtures for
every candidate. Breaches on `fixed-seed-pseudorandom` are intentionally
excluded by the frozen rule. Thus no candidate reaches the required two
structured fixtures, every per-window shortlist is empty, and no Silesia
follow-up manifest is created. The rule is not relaxed after observing the
timings. The private controller remains test and benchmark evidence only.

The ignored canonical result JSON has SHA-256
`67d3156a6c458e2de5abcf4f39be349c36e937952c26c5c4a84306c4d004fd2d`.
The completed checkpoint SHA-256 is
`37e3b4e8f01f8d554b1c47dee2a52b9af86991a6e60cbaa5ad19add3a6444a3c`.

### BM-0076: HashChain mnemonic-mixer measurement connection

The private `hash-chain-mnemonic-mixer-v1-exact` route is connected only to
the development match-finder benchmark. It shares the legacy HashChain
workspace, parser, statistics validator, token fingerprint, and report schema.
The synthetic benchmark smoke runs both routes on the fixed collision case and
requires equal canonical token identity and workspace plus strictly fewer v1
candidates and prefix mismatches. The complete smoke passes under MSVC and
ClangCL.

A separate 65,536-byte connection check produced 16,398 tokens and fingerprint
`4d20eda6e7c34bbd66e8bfe3c4ec0568c5a62d784bc84d60f127f3a61e9aa160`
for both routes. Legacy visited 1,301,551 candidates with 654,509 prefix
mismatches; v1 visited 652,059 candidates with 5,017 prefix mismatches. This
small check validates wiring and diagnostics only. Its timings are discarded,
it is not one of the frozen 36 records, and it does not authorize Silesia or
parameter tuning.

### BM-0077: Fixed prefix-mixer synthetic experiment infrastructure

The versioned manifest
`benchmarks/experiments/lzss-hash-chain-prefix-mixer-synthetic-v1.json` and
repository runner freeze the pre-Silesia comparison before measurement. The
grid contains 36 process-isolated records: six existing deterministic 64-MiB
fixtures, 4-/16-/64-MiB windows, and legacy then mnemonic-v1 HashChain Exact.
It fixes one iteration, a 64-MiB frame, a 512-MiB aggregate limit, expected
workspace at every window, all five Exact identity fields, and the admission
thresholds from DD-1144.

The runner atomically checkpoints each record and resumes only a validated
canonical prefix bound to revision, executable and tool digests, manifest,
fixtures, environment, and full configuration. Its self-tests simulate a
three-record interruption and complete the remaining 33 without relaunching
the accepted prefix. At this infrastructure gate no record has been measured,
no result artifact exists, and Silesia remains unread. BM-0077 therefore
reports reproducible experiment readiness, not performance.

### BM-0078: Fixed HashChain prefix-mixer synthetic result

The complete MSVC Release experiment at commit `bc14a52e` finished all 36
process-isolated records: six deterministic 64-MiB fixtures, 4-/16-/64-MiB
windows, and legacy plus mnemonic-v1 HashChain Exact for every fixture/window.
Every pair matched in all five Exact identity fields and used the fixed equal
workspace. A zero-new-point rerun validated `progress=36/36` without launching
another benchmark child.

| Window | V1 / legacy throughput | V1 / legacy prefix mismatches | V1 / legacy candidates | V1 fixture wins | Workspace |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 1.002339 | 0.995721 | 0.996040 | 5 / 6 | 17,301,504 |
| 16 MiB | 0.941926 | 0.996272 | 0.996417 | 5 / 6 | 67,633,152 |
| 64 MiB | 1.007384 | 0.993780 | 0.994516 | 5 / 6 | 268,959,744 |

The structured fixtures show local gains, including approximately one-quarter
of legacy mismatch work for both periodic fixtures. The fixed-seed
pseudorandom control remains effectively unchanged and dominates aggregate
candidate work. Consequently, every aggregate mismatch ratio misses the
required `0.5`, and the 16-MiB throughput ratio also misses the `0.98` floor.
The pre-Silesia gate is false despite two faster aggregate windows and five
fixture wins per window. No threshold is relaxed and no Silesia follow-up is
created; v1 remains private and legacy remains production.

The ignored canonical result JSON has SHA-256
`8eb314beb176ed48a0ae72d4eb2294bfd053165a8c0cf8f371e1280faa938f28`.
The completed checkpoint SHA-256 is
`f5fc5973c6de89c535aa1ddce0189a494006aa9d920a9111515353167a56f57a`.

### BM-0079: HashChain bucket-scaling benchmark connection contract

The development match-finder benchmark admits three private strategy names:
`hash-chain-buckets-262144-exact`,
`hash-chain-buckets-1048576-exact`, and
`hash-chain-buckets-4194304-exact`. Each identity is statically bound to the
corresponding private finder and uses its checked workspace calculation. The
legacy and mnemonic-mixer routes retain the production 65,536-cap workspace.

Every HashChain report includes configured bucket cap, actual bucket count,
workspace, the five Exact token identity fields, and the existing candidate,
prefix, extension, maximum-depth, and histogram diagnostics. Synthetic smoke
must cross the legacy cap and compare all identities with legacy before a
long-run manifest may exist. Smoke timings are discarded; this connection is
reproducibility and wiring evidence only and contains no performance result.

### BM-0080: Fixed bucket-scaling synthetic experiment infrastructure

The immutable manifest is
`benchmarks/experiments/lzss-hash-chain-bucket-scaling-synthetic-v1.json` and
the runner is
`tools/run_lzss_hash_chain_bucket_scaling_experiment.py`. The grid contains
72 process-isolated records in fixture/window/strategy order, checkpoints
after every record, and resumes only a validated canonical prefix bound to all
code, data, binary, revision, configuration, and environment identities.

The manifest fixes all six 64-MiB fixture SHA-256 values, three windows, four
strategy identities, configured and actual bucket counts, exact x64
workspaces, the five Exact fields, the private-candidate Pareto rule, and the
pre-Silesia admission thresholds. Runner tests use mocked reports and process
launch only. No long benchmark has run, no result artifact exists, Silesia is
not read, and no performance or admission conclusion is supplied by BM-0080.

### BM-0081: Fixed bucket-scaling synthetic result

The complete MSVC 19.51 Release experiment at commit `f4264d87` finished all
72 process-isolated records: six deterministic 64-MiB fixtures, 4-/16-/64-MiB
windows, legacy HashChain, and the three fixed private bucket caps for every
fixture/window. Every private record matched legacy in token, literal, match,
and matched-byte counts and canonical token fingerprint. A zero-new-point
rerun revalidated `progress=72/72` without launching another child.

| Window | Bucket cap | Candidate / legacy throughput | Candidate / legacy candidates | Pseudorandom mismatches | Legacy pseudorandom mismatches | Workspace bytes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 262,144 | 2.219820 | 0.307451 | 1,040,218,924 | 4,160,758,678 | 18,874,368 |
| 4 MiB | 1,048,576 | 3.247476 | 0.136901 | 260,048,915 | 4,160,758,678 | 25,165,824 |
| 4 MiB | 4,194,304 | 3.754288 | 0.093443 | 65,013,143 | 4,160,758,678 | 50,331,648 |
| 16 MiB | 262,144 | 3.285636 | 0.280740 | 3,757,919,024 | 15,031,625,820 | 69,206,016 |
| 16 MiB | 1,048,576 | 7.553337 | 0.103556 | 939,464,205 | 15,031,625,820 | 75,497,472 |
| 16 MiB | 4,194,304 | 10.970623 | 0.058322 | 234,871,268 | 15,031,625,820 | 100,663,296 |
| 64 MiB | 262,144 | 3.361320 | 0.341874 | 8,588,617,986 | 34,354,578,077 | 270,532,608 |
| 64 MiB | 1,048,576 | 8.309291 | 0.179569 | 2,147,137,702 | 34,354,578,077 | 276,824,064 |
| 64 MiB | 4,194,304 | 13.133280 | 0.137930 | 536,785,402 | 34,354,578,077 | 301,989,888 |

All three private candidates are faster than legacy at all three windows,
retain at least 0.98 of legacy aggregate throughput at every window, and
strictly reduce fixed-seed pseudorandom prefix mismatches at every window.
None dominates another under the frozen throughput/candidate/workspace rule:
each larger table improves speed and search work while consuming more
workspace. Therefore all three candidates pass the predeclared gate for one
separately frozen Silesia follow-up. No candidate is promoted, no public or
production policy changes, and no Silesia record is read at this gate.

The ignored canonical result JSON has SHA-256
`7ee6ae123f7ba52c760db502ca8cfd32db4eb5e9a2448c408c73205c68d315f6`.
The completed checkpoint SHA-256 is
`7f0cc1ac1cdac7c075b35196b7ccbe50f265924200772e171f23a7f8fdd401be`.

### BM-0082: Fixed bucket-scaling Silesia experiment contract

The follow-up must use an inert immutable manifest and a repository runner for
exactly 144 records: twelve verified Silesia members, 4-/16-/64-MiB windows,
and legacy then the three private bucket caps in canonical order. One child
process produces one record, every accepted record is checkpointed
atomically, and a resume accepts only an identity-bound canonical prefix.

The result must report per-window aggregate throughput and candidates,
member wins, worst-member throughput, workspace, eligibility, the fastest
eligible cap, the smallest eligible cap within 0.95 of the fastest, and the
cross-window monotonic-policy classification fixed by DD-1158. Mock-only
runner tests must cover the complete contract and restart behavior. At this
infrastructure gate no real Silesia member is read, no long child is launched,
no performance result is created, and no production or public policy changes.

### BM-0083: Fixed bucket-scaling Silesia result

The complete MSVC 19.51 Release experiment at commit `882ee775` finished all
144 process-isolated records: twelve verified Silesia members, 4-/16-/64-MiB
windows, legacy HashChain, and all three private bucket caps for every
member/window. Every private record matched legacy in token, literal, match,
and matched-byte counts and canonical token fingerprint. A zero-new-point
rerun revalidated `progress=144/144` without launching another child.

| Window | Bucket cap | Throughput / legacy | Candidates / legacy | Member wins | Worst member | Admissible | Selected |
| ---: | ---: | ---: | ---: | ---: | ---: | :---: | :---: |
| 4 MiB | 262,144 | 1.032471 | 0.963770 | 11 / 12 | 0.971684 | yes | yes |
| 4 MiB | 1,048,576 | 1.052754 | 0.955488 | 12 / 12 | 1.005930 | yes | no |
| 4 MiB | 4,194,304 | 1.053588 | 0.952673 | 11 / 12 | 0.954829 | yes | no |
| 16 MiB | 262,144 | 1.117845 | 0.970422 | 12 / 12 | 1.008900 | yes | yes |
| 16 MiB | 1,048,576 | 1.122449 | 0.963446 | 12 / 12 | 1.010392 | yes | no |
| 16 MiB | 4,194,304 | 1.093128 | 0.961048 | 11 / 12 | 0.906096 | yes | no |
| 64 MiB | 262,144 | 1.125943 | 0.971620 | 12 / 12 | 1.010014 | yes | yes |
| 64 MiB | 1,048,576 | 1.154690 | 0.964851 | 11 / 12 | 0.995098 | yes | no |
| 64 MiB | 4,194,304 | 1.173593 | 0.962521 | 11 / 12 | 0.995162 | yes | no |

All nine candidate/window pairs beat legacy aggregate throughput, win at
least half the members, retain at least 0.90 of legacy on the worst member,
and reduce aggregate candidate visits. The fastest cap varies by window, but
262,144 remains within 0.95 of that fastest candidate at every window and is
the smallest admissible cap in that near-fastest set. The selected sequence
is therefore 262,144 at all three windows and satisfies the predeclared
nondecreasing cross-window rule. This is a production-policy proposal, not a
production, public API, ABI, CLI, format, profile, decoder, or default change.

The ignored canonical result JSON has SHA-256
`8c4f0c4cab1edb970250ecc5650e8e345cf9f77d3f845d35330be87c04f1527a`.
The completed checkpoint SHA-256 is
`a37276071d0cc85f4cefdea2a8f8c16ab5c66f66ef01204601f82281d2b4a72d`.

### BM-0084: Post-promotion HashChain performance and memory audit

On 2026-09-21, after the standard HashChain route was rebound to 262,144
buckets, the MSVC Release benchmark built from `fe11a20b` was run at
repository revision `778cbefa` against the locally held Silesia `xml` member
(5,345,280 bytes). The intervening commit changed only a CMake smoke-test
expression and did not rebuild this benchmark. Each route
was measured in a separate, sequential process with `--frames-limited`,
three iterations, a 64-MiB frame, a 512-MiB hard workspace limit, and the
same window and input. These are match-finder throughput measurements, not
whole-codec encode speed or compression-ratio measurements.

| Window | Legacy 65,536 MiB/s | Standard 262,144 MiB/s | Standard / legacy | Legacy workspace | Standard workspace |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB | 5.532556 | 6.227504 | 1.125611 | 17,301,504 B | 18,874,368 B |
| 16 MiB | 5.426152 | 6.141876 | 1.131903 | 67,633,152 B | 69,206,016 B |
| 64 MiB | 5.432875 | 6.158357 | 1.133536 | 268,959,744 B | 270,532,608 B |

At each window the two routes emitted identical token, literal, match, and
matched-byte counts and the same SHA-256 token fingerprint. At 4 MiB the
standard route also matched the explicit 262,144 specialization in all
reported identity and search-work fields. The exact workspace increase is
1,572,864 bytes at each listed window. In separate single-process 64-MiB
window runs, polling the process working set at 10-ms intervals observed
sampled peaks of 341,446,656 bytes (legacy) and 343,019,520 bytes
(standard), also a 1,572,864-byte difference. Sampled working set is an
observation, not a guaranteed process peak or a substitute for workspace
limits.

The earlier all-12-member, 144-record fixed Silesia result in BM-0083 remains
the selection evidence; its ignored result file was rechecked against the
recorded SHA-256. That v1 manifest fixes the old name `hash-chain-exact` to
65,536 and must not be rerun unchanged against the promoted binary. The
current `xml` spot measurement confirms production wiring but is too small
and timing-sensitive to replace the full-Corpus result or establish a new
cross-platform speed guarantee. A three-iteration `mozilla` spot run was
stopped when the existing result showed its single iteration would take
roughly eight minutes; no partial timing from that run is used.

### BM-0085: End-to-end HashChain promotion A/B spot check

On 2026-09-21, the immediate pre-promotion commit `64f79321` and promoted
commit `fe11a20b` were built as separate MSVC 19.51 x64 Release programs.
Both used `/O2 /Ob2 /DNDEBUG`, the same compiler, and the same `marc_benchmark`
source, public `lzss-contextual-rans-4m` codec, input, and iteration count.
The old build's initial failed configuration had left Release optimization
flags empty; an unoptimized exploratory run was discarded, the flags were
matched explicitly, and the old library and benchmark were fully rebuilt
before any value below was accepted. The benchmark verifies a round trip
before timing encode and decode independently.

| Silesia member | Input | Iterations per process | Old encode seconds (two runs) | New encode seconds (two runs) | Old/new median encode-time ratio | Encoded bytes, both |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `xml` | 5,345,280 B | 3 | 2.636, 2.536 | 2.412, 2.481 | 1.057 | 549,164 B |
| `x-ray` | 8,474,240 B | 2 | 4.564, 4.585 | 2.435, 2.419 | 1.885 | 5,450,402 B |

Separate old/new CLI encode runs produced byte-identical complete archives:
the `xml` SHA-256 was
`8a0d129c2cedf105ab9207e533182a6dcd1f9975cd607c74954f189e2e61760`,
and `x-ray` was
`0adb83d0b113e1daa122ea9f06fecc2bd30eddce92266927b822b7de953ab08a`.
The C API benchmark reported the same decoder workspace and, for both
inputs, a codec peak queried workspace of 130,556,905 bytes before and
132,129,769 bytes after promotion: exactly 1,572,864 bytes more. Separate
`xml` process runs sampled working set every 10 ms and observed peaks of
335,638,528 and 337,211,392 bytes, respectively. Those sampled values are
not guaranteed OS peak-memory measurements.

This spot check shows that the selected HashChain change reaches a complete
codec pipeline without changing archive bytes on these inputs. Decode-time
samples varied and are not used to attribute a decoder improvement. The two
members, few process runs, and local machine cannot establish a universal
speedup or replace BM-0083's all-member selection experiment.

### BM-0086: Resumable end-to-end A/B dry run

On 2026-09-21 the fixed v1 runner compared the clean, isolated
`64f79321ec20169f2cc55787b0f637dc7075ea57` and
`fe11a20b0c5d3e79101f7567c97b70cf97ea28da` source trees. Both used
MSVC 19.51 x64 Release with matching C/C++ defaults,
`/O2 /Ob2 /DNDEBUG`, and nonincremental linker flags. An earlier baseline
build from a partially initialized cache was rejected and completely
rebuilt; no value from it appears below. Each public
`lzss-contextual-rans-4m` benchmark used one measured iteration and verified
its own round trip before timing. Each side also independently produced a
complete CLI archive whose byte count matched the benchmark report.

| Member | Baseline encode | Candidate encode | Baseline decode | Candidate decode | Archive bytes, both | Queried workspace, baseline / candidate |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `xml` | 0.852 s | 0.808 s | 0.041 s | 0.041 s | 549,164 | 130,556,905 / 132,129,769 B |
| `x-ray` | 2.286 s | 1.205 s | 0.389 s | 0.373 s | 5,450,402 | 130,556,905 / 132,129,769 B |

The A/B complete-archive SHA-256 values agreed per member:
`xml` = `8a0d129c2cedf105ab9207e533182a6dcd1f9975cd607c74954f189e2e61760`;
`x-ray` = `0adb83d0b113e1daa122ea9f06fecc2bd30eddce92266927b822b7de953ab08a`.
The exact queried-workspace increase was 1,572,864 bytes in both cases.
The ignored dry-run result SHA-256 is
`882d6466b86be6e72c5840783c4a210dff3919fb817bde9d2a9d3a2c51455df4`;
its completed checkpoint SHA-256 is
`745016ce89a90d09d943e02d6ecf8318cd9516fcb987e6c13adf74bfd5e1b4a7`.

The first invocation stopped at 2/4 records, the second resumed only the
remaining pair, and a third completed without another measurement. These are
dry-run observations on two selected inputs, not the all-twelve-member result,
not a speed guarantee, and not a decoder-performance conclusion. The full
campaign will use a separate checkpoint and exclude these timings.

### BM-0087: Full-Corpus end-to-end HashChain A/B experiment

On 2026-09-22, the fixed v1 runner completed 24 process-isolated benchmark
records: one measured encode/decode iteration for each side of all twelve
locally supplied Silesia members, with per-member CLI archive checks. The
source revisions, MSVC 19.51 x64 Release build conditions, codec, and
preflight are those fixed in the experiment design and BM-0086. The 2-member
dry-run records were excluded. A completed-checkpoint replay launched no new
measurement.

| Member | Baseline encode | Candidate encode | Archive bytes, both |
| --- | ---: | ---: | ---: |
| `dickens` | 6.312 s | 5.855 s | 3,265,887 |
| `mozilla` | 75.655 s | 74.849 s | 18,954,972 |
| `mr` | 62.645 s | 63.912 s | 3,386,820 |
| `nci` | 39.363 s | 39.280 s | 2,465,068 |
| `ooffice` | 1.959 s | 1.403 s | 3,028,821 |
| `osdb` | 2.361 s | 1.635 s | 3,286,689 |
| `reymont` | 8.170 s | 8.074 s | 1,591,187 |
| `samba` | 19.984 s | 19.478 s | 4,851,746 |
| `sao` | 3.197 s | 1.831 s | 5,270,047 |
| `webster` | 31.533 s | 30.407 s | 10,378,367 |
| `xml` | 0.835 s | 0.792 s | 549,164 |
| `x-ray` | 2.276 s | 1.184 s | 5,450,402 |

Both sides processed 211,938,580 input bytes and produced 62,479,170
archive bytes (aggregate encoded/input ratio 0.2947984742). Every member's
complete-archive byte count and SHA-256 matched between sides. Summed encode
times were 254.290 s baseline and 248.700 s candidate; byte-weighted
throughputs were 0.794842 and 0.812708 MiB/s, respectively, a candidate to
baseline ratio of 1.022477. The median per-member baseline/candidate
encode-time ratio was 1.045662; the lowest was 0.980176 (`mr`). Queried peak
codec workspace was 130,556,905 versus 132,129,769 bytes on every member,
an exact increase of 1,572,864 bytes. Summed decode times were 4.315 versus
4.318 s and do not establish a decoder difference.

The ignored canonical full result SHA-256 is
`cb2a84023e8d7c1bf7814db2f41421c40b97cb410fe304b37de88171717560d0`;
the completed checkpoint SHA-256 is
`e97469de30eaa17abbfa2321cb2919f4a138593dd35e8276394bc58aca7b1708`.
The result retains each raw benchmark report, complete-archive digest,
verified Corpus identity, executable hash, source revision, and build flags.
The single iteration and local process scheduling make the modest aggregate
speed difference descriptive, not a universal guarantee or CI threshold.

### BM-0088: Selected contextual rANS encode-phase pilot

On 2026-09-22, the fixed `xml`/`x-ray`/`mr` pilot completed three independent
one-iteration processes per member at clean revision
`f4987173ec9e6812718dd6818be020cba8b83967`. It used MSVC
19.51.36252.0, x64 Release, `/O2 /Ob2 /DNDEBUG`, and the private
`lzss-contextual-rans-4m` diagnostic executable SHA-256
`525b6f8bd04a482761e69412bda9ec23513ccc2ecae429443c6b441dfcd6eac8`.
The generated target project SHA-256 was
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`.
The full local Silesia Corpus was verified before measurement; the ignored
result records each selected member's published MD5 and local SHA-256.

Each process performed an untimed public encode/decode round trip and
checked both untimed and timed private complete archives against the public
archive before reporting phase times. All nine attempts passed. Each
member's archive byte count and SHA-256 remained identical across attempts;
the queried encoder workspace was 132,129,769 bytes in each case.

| Member | Raw total times, seconds | Median total | Tokenize | First plan | Second plan | Reverse write | Other phases |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `xml` | 0.799, 0.807, 0.792 | 0.799 s | 93.79% | 2.25% | 2.26% | 1.66% | 0.04% |
| `x-ray` | 1.239, 1.209, 1.201 | 1.209 s | 51.22% | 17.83% | 17.91% | 12.97% | 0.06% |
| `mr` | 67.392, 66.356, 66.426 | 66.426 s | 99.54% | 0.17% | 0.17% | 0.12% | 0.00% |

The percentages come from the single attempt with each member's median
total, so they describe one valid partition; the ignored result also retains
all nine raw stage counters and separate per-stage medians. `Other phases`
combines the small `frame_finish` and `other` counters, with displayed
percentages rounded. The result SHA-256 is
`f83cc3657efa48617f6e508baf18c1c2193a34f1a68fe75a014a6a3ad7e716dc`.

On these selected inputs, tokenization (including match search) dominates
`xml` and `mr`; the two existing contextual plans together account for about
36% of `x-ray`. Instrumentation overhead and local scheduling are included.
This is a diagnostic pilot, not a full-Corpus estimate, codec speed guarantee,
or justification to remove the second plan or its validation. A separately
frozen all-member measurement is required before selecting an optimization.

### BM-0089: Full-Corpus contextual rANS encode-phase campaign

On 2026-09-22, the separately frozen v1 campaign completed all 36 records:
three independent one-iteration processes for each of the twelve verified
Silesia members, in manifest order. It used clean source revision
`574cac57f62bb1675260d72ad45f458ac7116c67`, MSVC 19.51.36252.0,
x64 Release, `/O2 /Ob2 /DNDEBUG`, and the private
`lzss-contextual-rans-4m` diagnostic executable SHA-256
`525b6f8bd04a482761e69412bda9ec23513ccc2ecae429443c6b441dfcd6eac8`.
The generated target project SHA-256 was
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`;
the fixed manifest SHA-256 was
`109dec1508ea1efb85edc1b2e5cff1ff03cb780304fb33d4cf3287566e4892e7`.
Pilot records were not reused. Each process passed the untimed public round
trip, timed and untimed private complete-archive identity checks, workspace
accounting, and same-invocation phase partition. Archive size and SHA-256
were stable across the three processes for every member. The queried encoder
workspace was 132,129,769 bytes throughout.

| Member | Median total | Tokenize | Two plans | Reverse write |
| --- | ---: | ---: | ---: | ---: |
| `dickens` | 6.571 s | 96.07% | 2.88% | 1.04% |
| `mozilla` | 73.155 s | 96.73% | 2.39% | 0.88% |
| `mr` | 67.565 s | 99.54% | 0.34% | 0.12% |
| `nci` | 41.252 s | 99.53% | 0.34% | 0.12% |
| `ooffice` | 1.441 s | 71.73% | 19.84% | 8.40% |
| `osdb` | 1.662 s | 77.06% | 16.81% | 6.09% |
| `reymont` | 7.991 s | 97.94% | 1.55% | 0.50% |
| `samba` | 19.743 s | 97.31% | 1.96% | 0.72% |
| `sao` | 1.844 s | 58.61% | 29.88% | 11.48% |
| `webster` | 32.310 s | 97.42% | 1.88% | 0.69% |
| `xml` | 0.805 s | 93.88% | 4.52% | 1.57% |
| `x-ray` | 1.209 s | 51.16% | 35.86% | 12.93% |

Each row selects the invocation with that member's median total. Its phase
shares therefore describe one actual, internally consistent partition;
separate per-phase medians in the ignored result must not be summed as if
they were one invocation. `Two plans` combines first and second plan only.
The omitted frame-finish plus residual share is at most 0.05% after rounding
in these rows. The ignored full result retains all 36 raw reports, member
archive digests, verified Corpus identities, and execution metadata. Its
SHA-256 is
`b9b6a666b73c88cf1e15aa7a4a4c0adf366ab036d2207c0e446856f69208c748`;
the completed checkpoint SHA-256 is
`4b50e0d071f8604bae49f4f982b865c8a8f2d1e5e1f9d24e1ad88ab515f58ee9`.
A completed replay validated all 36 records and left the result unchanged.

Token production, which includes match search, exceeds 90% of median-total
time on eight members. It does not isolate match-search time from other token
work. The two existing contextual plans remain material on `x-ray` (35.86%),
`sao` (29.88%), `ooffice` (19.84%), and `osdb` (16.81%). The pilot's selected
`xml`, `x-ray`, and `mr` pattern is consistent with the full campaign, but
their attempts are independent and not pooled. Instrumentation overhead and
local scheduling are included. This is descriptive diagnostic evidence, not
a cross-platform speed claim, a CI performance gate, or authority to remove
either plan or its validation.

### BM-0090: Selected contextual rANS token-production pilot

On 2026-09-22, the fixed `mr`/`sao`/`x-ray` pilot completed three
independent one-iteration processes per member at clean source revision
`7d4731a21f75136b7e821793ae998d71e4abaceb`. It used the 4 MiB
contextual rANS public profile, production HashChain exact matching,
Visual Studio 18 2026 x64 Release, MSVC 19.51.36252.0, CMake 4.3.4,
and `/O2 /Ob2 /DNDEBUG`. The static-only diagnostic executable SHA-256 was
`1f60268e6d99a71798b329871e85b364974eccc676cbe784b63015910e7b6961`;
its generated project SHA-256 was
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`.
All twelve local Silesia files matched the published sizes and MD5 values
before measurement. The selected inputs and complete archive identities
were stable across all three attempts per member:

| Member | Input bytes | Input SHA-256 | Archive bytes | Archive SHA-256 | Tokens |
| --- | ---: | --- | ---: | --- | ---: |
| `mr` | 9,970,564 | `68637ed52e3e4860174ed2dc0840ac77d5f1a60abbcb13770d5754e3774d53e6` | 3,386,820 | `6e8525bdbdf6694563451b70c3e6de477a5a168969d5166cdfe2b8a7c35290f3` | 1,608,818 |
| `sao` | 7,251,944 | `c2d0ea2cc59d4c21b7fe43a71499342a00cbe530a1d5548770e91ecd6214adcc` | 5,270,047 | `a4ff2578dfc960df69874a8078f55e04c992303716ca07b639a5d474ca127087` | 4,270,470 |
| `x-ray` | 8,474,240 | `7de9fce1405dc44ae5e6813ed21cd5751e761bd4265655a005d39b9685d1c9ad` | 5,450,402 | `0adb83d0b113e1daa122ea9f06fecc2bd30eddce92266927b822b7de953ab08a` | 3,372,785 |

Each process performed an untimed public encode/decode round trip, then
verified untimed and timed private complete-archive byte count and SHA-256
against the public output. The queried encoder workspace was 132,129,769
bytes for each selected input. The old whole-encode phase partition and
the new inner `tokenize` partition passed for every process; query and
advance calls equaled token count, and advanced bytes equaled input size.
Raw measured values below are nanoseconds from each actual process, not
per-phase medians:

| Member | Attempt | Total | Tokenize | Initialize | Query | Advance | Token-other |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| `mr` | 1 | 72,066,697,300 | 71,722,308,100 | 736,900 | 71,513,864,400 | 144,551,500 | 63,155,300 |
| `mr` | 2 | 71,286,624,200 | 70,956,053,300 | 742,500 | 70,754,862,200 | 137,180,900 | 63,267,700 |
| `mr` | 3 | 71,482,710,700 | 71,140,598,000 | 725,400 | 70,946,539,800 | 130,116,200 | 63,216,600 |
| `sao` | 1 | 2,955,580,700 | 2,051,859,600 | 500,100 | 1,765,123,400 | 123,857,400 | 162,378,700 |
| `sao` | 2 | 2,638,041,600 | 1,728,690,300 | 522,000 | 1,444,633,700 | 120,542,000 | 162,992,600 |
| `sao` | 3 | 2,443,288,000 | 1,531,429,900 | 567,800 | 1,248,428,300 | 120,306,000 | 162,127,800 |
| `x-ray` | 1 | 1,713,586,400 | 1,042,070,100 | 586,900 | 796,791,600 | 116,447,000 | 128,244,600 |
| `x-ray` | 2 | 1,679,724,500 | 1,015,507,200 | 616,100 | 757,335,800 | 129,524,400 | 128,030,900 |
| `x-ray` | 3 | 1,898,820,700 | 1,110,864,100 | 632,200 | 855,437,500 | 124,191,000 | 130,603,400 |

Selecting the actual invocation with each member's median total gives
`mr` attempt 3, `sao` attempt 2, and `x-ray` attempt 1. In those same
invocations, tokenization represented respectively 99.52%, 65.53%, and
60.81% of measured total time. `find_match` represented respectively
99.73%, 83.57%, and 76.46% of that invocation's tokenization time;
`advance` represented 0.18%, 6.97%, and 11.17%; token-other represented
0.09%, 9.43%, and 12.31%. Finder initialization was below 0.06% of
tokenization in all three selected invocations. These are nested shares
of one invocation, not additive shares of whole-encode time.

Per-token clock calls and accumulation perturb the timed path, especially
for short inputs. The residual also includes clock and accumulator overhead
and is not an isolated token-write cost. This selected pilot supports
investigating HashChain query cost next; it does not establish an
uninstrumented speedup, justify changing match-finder policy, remove any
contextual plan or validation, or set a CI timing threshold. Its source and
instrumentation differ from BM-0089, so absolute times must not be treated
as a before/after comparison. A separately frozen all-member campaign
would be required for a Corpus-wide conclusion.

### BM-0091: Full-Corpus contextual rANS token-production breakdown

On 2026-09-22, the separately frozen token-production campaign completed
all 36 records: three independent one-iteration processes for each of the
twelve verified Silesia members. The clean source revision was
`2151d215597252fe8e08ed029b9181024314cac8`, with Visual Studio 18
2026 x64 Release, MSVC 19.51.36252.0, `/O2 /Ob2 /DNDEBUG`, the private
diagnostic executable SHA-256
`1f60268e6d99a71798b329871e85b364974eccc676cbe784b63015910e7b6961`,
and generated target project SHA-256
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`.
The fixed manifest SHA-256 was
`5c55ac850e1b9d19fbc0369ad7771648e27304669f39b8cd92164e8cc4288105`.
No BM-0089 or BM-0090 process was reused. Each process passed the public
round trip, untimed and timed private complete-archive identity checks,
whole-encode and nested tokenization partitions, and structural count
checks. Archive size and SHA-256 were stable across all three processes
for every member. The queried encoder workspace was 132,129,769 bytes.

Each row uses the actual invocation with that member's median total time.
`Tokenize` is a share of that invocation's whole-encode total; the inner
columns are shares of its own `tokenize` interval, not additional shares
of the total. Initialization, omitted from the table, was below 0.1% of
tokenization for every selected invocation.

| Member | Median total | Tokenize/total | Query/tokenize | Advance/tokenize | Token-other/tokenize |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dickens` | 6.367 s | 95.92% | 97.53% | 1.67% | 0.79% |
| `mozilla` | 81.483 s | 96.68% | 98.51% | 0.85% | 0.63% |
| `mr` | 73.230 s | 99.55% | 99.72% | 0.20% | 0.09% |
| `nci` | 42.442 s | 99.53% | 99.28% | 0.63% | 0.09% |
| `ooffice` | 1.691 s | 73.84% | 87.36% | 6.25% | 6.35% |
| `osdb` | 1.874 s | 77.64% | 86.79% | 7.20% | 5.96% |
| `reymont` | 8.148 s | 98.43% | 98.83% | 0.88% | 0.28% |
| `samba` | 21.655 s | 97.15% | 98.43% | 1.04% | 0.53% |
| `sao` | 2.372 s | 59.60% | 79.87% | 8.68% | 11.41% |
| `webster` | 34.036 s | 97.33% | 98.02% | 1.48% | 0.48% |
| `xml` | 0.856 s | 93.80% | 93.81% | 4.90% | 1.25% |
| `x-ray` | 1.546 s | 56.63% | 71.75% | 13.48% | 14.70% |

`find_match` exceeds 90% of tokenization in eight members and 80% in ten.
The remaining `sao` and `x-ray` members still spend 79.87% and 71.75%
respectively in query, while their contextual plans remain important in
the outer partition. The complete local result retains all 36 raw reports
and identities under the ignored Silesia results directory. Its SHA-256 is
`bee7edb0a4137116c1d1acc19dbe460bd7faa6abe01334579d8f868580813944`;
the completed checkpoint SHA-256 is
`c00dd604865c6f3cfd16525949d0ff6d15f8eeca7aecf99ea7eb0119608bc9aa`.
A completed replay validated all records and left the result unchanged.

Per-token clock calls and accumulation perturb this diagnostic path, and
`token-other` includes clock/accumulator overhead. These data support
investigating HashChain query work, especially on the eight query-heavy
members; they do not establish uninstrumented throughput gain, justify a
particular alternative match finder, remove contextual plans or validation,
or set a CI timing gate. BM-0089 and BM-0090 used different source revisions
or attempts, so their absolute times are not before/after comparisons.

### BM-0092: Selected 4 MiB match-finder query spot check

After BM-0091, a local exploratory spot check used the existing private
`marc_lzss_match_finder_benchmark` on `mr`, `sao`, and `x-ray`. The clean
repository revision was `b122913bba8da30d795f384c938fc2c46d309e2c`,
the MSVC x64 Release executable SHA-256 was
`d67421f6c5ccaaa8db4eaa725630a0bf8e11c8f97c86f446fefd56aa8cba1b48`,
and its generated project SHA-256 was
`7a9b4b23a9966549958f8f8a580cf7d112c9c854ba624080c12c055f38e1640e`.
For each member and strategy, the invocation was
`--frames-limited <strategy> <member> 1 4194304 4194304 536870912`:
one iteration, 4 MiB frame and window, and a 512 MiB hard internal-buffer
limit. The benchmark first collects and validates structural statistics,
then measures a separate statistics-disabled parse. These are isolated
match-finder timings, not complete contextual rANS encode timings.

| Member | HashChain time | Binary Tree time | Binary Tree / HashChain time | HashChain candidates/query | HashChain maximum candidates/query |
| --- | ---: | ---: | ---: | ---: | ---: |
| `mr` | 73.335263 s | 10.708430 s | 0.146 | 3,207.51 | 1,177,133 |
| `sao` | 1.087293 s | 7.632505 s | 7.020 | 13.54 | 5,167 |
| `x-ray` | 0.617753 s | 6.857630 s | 11.101 | 8.78 | 316 |

On `mr`, HashChain visited 5,160,303,018 candidates over 1,608,818
queries; 5,152,880,935 candidates (99.86%) matched its five-byte
prefix. It performed 102,426,638,746 byte comparisons, including
76,654,663,995 comparisons beyond that prefix. By contrast, `sao`
visited 57,817,594 candidates over 4,270,470 queries and `x-ray`
visited 29,610,716 over 3,372,785. The `mr` observation therefore points
to many genuinely repeated prefixes, not merely distinct prefixes that
collide in the hash bucket. The two strategies produced identical token
counts, literal/match counts, matched-byte counts, and exact token SHA-256
fingerprints for each member:

| Member | Shared token fingerprint SHA-256 |
| --- | --- |
| `mr` | `2490709a4533ba44772937870104bf77c4cdc510c17362f1c676bc1392d428ce` |
| `sao` | `de12729cd821085f8f029568706e1f7495bee4cb8a34af65725598ba4bd53c31` |
| `x-ray` | `efbc28dd3424e2471170b401fe301704b345a9140d54dc30d19466dc55733b98` |

HashChain required 18,874,368 bytes of finder workspace at this frame
size; Binary Tree required 121,634,816 bytes (6.44 times as much). This is
one process and one timed iteration per member/strategy, without a fixed
all-member campaign or interleaved repetitions. The large `mr` improvement
is a useful hypothesis, but the opposite `sao`/`x-ray` behavior and memory
cost prohibit a blanket promotion. No whole-codec speedup, compression-ratio
change, default-policy change, or CI timing threshold is claimed.

### BM-0093: Selected 4 MiB HashChain best-length-probe pilot

The fixed `silesia-hash-chain-best-length-probe-v1.json` pilot completed
all 18 records: `mr`, `sao`, and `x-ray`, each with three independent
baseline/probe process pairs. The source revision was
`bd9c066499265d927c0990b268198763ef7e826b`; MSVC 19.51.36252.0 x64 Release
used `/O2 /Ob2 /DNDEBUG`. Executable SHA-256 was
`2b7c6328e276c6bbcf474305a80581ac3ee64375c4d366645148451cf36d47ca`,
generated project SHA-256 was
`7a9b4b23a9966549958f8f8a580cf7d112c9c854ba624080c12c055f38e1640e`,
and manifest SHA-256 was
`fd1fc6bc4e087982ef9760638415e296b3a2bda9b825cd1a359a245768b9e5d4`.

Each invocation used one timed iteration, 4 MiB frames and windows,
5..258-byte matches, a 128 MiB hard internal-buffer limit, and 262,144
hash buckets. Both strategies required 18,874,368 bytes of finder workspace.
Diagnostics and the probe's baseline identity check ran before the separate
statistics-disabled timed pass. These are isolated matcher/parse times,
not complete codec encode times or process wall-clock durations.

| Member | Baseline median | Probe median | Baseline / probe speed | Probe / baseline byte comparisons | Candidates pruned |
| --- | ---: | ---: | ---: | ---: | ---: |
| `mr` | 70.687884 s | 20.647723 s | 3.424x | 0.078185 | 97.975% |
| `sao` | 1.123977 s | 1.031232 s | 1.090x | 0.332405 | 50.789% |
| `x-ray` | 0.626893 s | 0.609399 s | 1.029x | 0.539610 | 42.921% |

Every record retained identical token fingerprints, literal/match counts,
matched bytes, candidate visitation counts, and query-depth distributions
across strategies. Per-strategy diagnostic counters were stable across
attempts. On `mr`, 5,055,791,397 candidates were pruned; total byte
comparisons, including the extra probe reads, fell from 102,426,638,746 to
8,008,243,802. Token fingerprints match those recorded in BM-0092, but
BM-0092's absolute timings are not used as this pilot's baseline.

The local complete result SHA-256 is
`fd2894ec6cd658c4ae3b316647886a4d258698d89bf5fab6cb32154c97d6e526`;
the checkpoint SHA-256 is
`cedd7c6d924c7e5567fc9fb1531e4b20b47b7e89b66e5d6dfa3b9e62ba4b626f`.
Both remain in the ignored Silesia results directory. Completed replay
validated all records without launching measurements or rewriting results.

The large `mr` gain supports a separately frozen all-member experiment.
The small `x-ray` difference may include timing noise; three selected
members do not establish general non-regression. Whole-codec archive
identity, encode/decode throughput, compression ratio, and memory still
need their own audit before production adoption. The default is unchanged.

### BM-0094: All-member 4 MiB HashChain best-length-probe experiment

The frozen `silesia-hash-chain-best-length-probe-full-v1.json` campaign
completed all 72 records at clean revision
`f8580cc2c490ae419eb07fce799b083f68074d3a`. All twelve locally verified
Silesia members received three independent baseline/probe pairs, in the
predeclared member/attempt/strategy order. No child failed or timed out.
The build used MSVC 19.51.36252.0 x64 Release with `/O2 /Ob2 /DNDEBUG`.
Executable SHA-256 was
`2b7c6328e276c6bbcf474305a80581ac3ee64375c4d366645148451cf36d47ca`,
project SHA-256 was
`7a9b4b23a9966549958f8f8a580cf7d112c9c854ba624080c12c055f38e1640e`,
and full-manifest SHA-256 was
`6e04e109ecf6356e4f7f614accb0b0b9d39279f5c875f2e4382ff1da9e8e27f7`.

Conditions remain 4 MiB frame/window, 5..258-byte matches, 262,144 hash
buckets, one timed iteration per child, and a 128 MiB hard internal-buffer
limit. Both finders use 18,874,368 bytes of workspace. Diagnostic collection
and the probe's baseline identity check precede the statistics-disabled
timing pass. These timings cover the isolated matcher/parse, not complete
codec processing or the verification-inclusive child wall time.

| Member | Baseline median | Probe median | Baseline / probe speed | Probe / baseline byte comparisons |
| --- | ---: | ---: | ---: | ---: |
| `dickens` | 5.549236 s | 4.989700 s | 1.112x | 0.204497 |
| `mozilla` | 74.730577 s | 31.505351 s | 2.372x | 0.120905 |
| `mr` | 69.777218 s | 20.916999 s | 3.336x | 0.078185 |
| `nci` | 40.668699 s | 18.573331 s | 2.190x | 0.153898 |
| `ooffice` | 1.074852 s | 0.831079 s | 1.293x | 0.177277 |
| `osdb` | 1.277413 s | 0.984592 s | 1.297x | 0.134975 |
| `reymont` | 7.913199 s | 6.669169 s | 1.187x | 0.172137 |
| `samba` | 20.263488 s | 5.629278 s | 3.600x | 0.052513 |
| `sao` | 1.104467 s | 1.025720 s | 1.077x | 0.332405 |
| `webster` | 29.870848 s | 23.716806 s | 1.259x | 0.130943 |
| `xml` | 0.761991 s | 0.574397 s | 1.327x | 0.123986 |
| `x-ray` | 0.618464 s | 0.594634 s | 1.040x | 0.539610 |

Summed per-member median time is 253.610452 seconds for baseline and
116.011056 seconds for probe: a 2.186x aggregate speedup, or a 54.256%
time reduction. This is a ratio of summed times, not a mean of the member
ratios. Every member's median improved; the smallest observed gain is
1.040x on `x-ray`. Small differences remain sensitive to measurement noise,
and this fixed-order, single-machine result does not establish a universal
non-regression guarantee.

All records passed exact token fingerprint, literal/match count,
matched-byte count, candidate count, query-depth distribution, and workspace
checks. Diagnostic counters were identical across attempts for each
member/strategy. Byte-comparison ratios include probe reads. Raw records,
all Corpus identities, and per-member medians remain in the ignored result
file with SHA-256
`a352184b9fbfbbd25972d02c1cd1be195b39ff2018dc560597cdd99713654eab`.
The complete checkpoint SHA-256 is
`4898ba791ec8eab768e5470cf2d84ad63a3215cb8236ea20a0a91738d84774ef`.
Completed replay revalidated all 72 records without new measurements or
result rewriting.

The full-Corpus result supports the next whole-codec audit: exact archive
bytes, encode/decode throughput, compression ratio, and peak/bounded working
memory. It does not itself establish those properties or change the public
strategy/default. Keep the probe private until that evaluation is complete.

### BM-0095: Selected whole-codec contextual rANS best-length-probe pilot

The frozen `silesia-contextual-rans-best-length-probe-v1.json` campaign
completed all eighteen records at clean revision
`97b777f0b82126da5beacbcf2ef50be2942cf233`. Each of `mr`, `sao`, and `x-ray`
received three independent baseline/probe pairs in the declared order.
No child failed or timed out. The build was MSVC 19.51.36252.0 x64 Release,
`/O2 /Ob2 /DNDEBUG`, with executable SHA-256
`95b462d4b269071442017bcad09625ab4a57627c7d83d1bdbaa0b2aafb612418`,
project SHA-256
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`,
and manifest SHA-256
`0492ed54dfefbdb77a9e657a7db2a4f4b2419be3218bb6dd8c18eb243c54ac8f`.

Both sides used contextual rANS with 4 MiB frame/window and 5..258-byte
matches. One uninstrumented encode and ordinary decode process call per
child was timed; allocation, construction, file I/O, oracle generation,
hashing and post-run comparison were outside those intervals. Validation,
tokenization, model construction and framing inside `process` remain
included. These are whole-codec process times, not child wall times or
isolated matcher times. Source bytes were recovered on every trial.

| Member | Baseline encode median | Probe encode median | Encode speedup | Baseline decode median | Probe decode median |
| --- | ---: | ---: | ---: | ---: | ---: |
| `mr` | 60.089460 s | 20.833385 s | 2.884x | 0.246595 s | 0.243066 s |
| `sao` | 1.834228 s | 1.772736 s | 1.035x | 0.328597 s | 0.324320 s |
| `x-ray` | 1.222464 s | 1.209357 s | 1.011x | 0.380113 s | 0.403836 s |

Encode throughput calculated from those medians was respectively
0.158242/0.456415 MiB/s (`mr`), 3.770520/3.901310 MiB/s (`sao`), and
6.610965/6.682615 MiB/s (`x-ray`), baseline/probe. Decode throughput was
38.559931/39.119724, 21.047042/21.324576, and 21.261223/20.012260 MiB/s.
The decoder and its input archive are unchanged. In particular, the slower
observed `x-ray` decode median is retained, not attributed to an algorithm
change or removed from the result. Small encode gains and decode differences
remain sensitive to timing noise and the fixed-order, single-machine setup.

Every private archive was byte-for-byte identical to the public encoder's
oracle. Hashes, lengths, ratio and queried workspace were stable across all
attempts and both strategies:

| Member | Archive bytes | Encoded/input ratio | Archive SHA-256 |
| --- | ---: | ---: | --- |
| `mr` | 3,386,820 | 0.339681888 | `6e8525bdbdf6694563451b70c3e6de477a5a168969d5166cdfe2b8a7c35290f3` |
| `sao` | 5,270,047 | 0.726708176 | `a4ff2578dfc960df69874a8078f55e04c992303716ca07b639a5d474ca127087` |
| `x-ray` | 5,450,402 | 0.643172957 | `0adb83d0b113e1daa122ea9f06fecc2bd30eddce92266927b822b7de953ab08a` |

For all three inputs, encoder workspace was 132,129,769 bytes and decoder
workspace 114,017,257 bytes on both sides. Sequential peak codec workspace
was therefore 132,129,769 bytes, within the 128 MiB internal-buffer limit.
This is queried codec workspace, not process RSS: benchmark input, oracle,
output, alignment slack, objects and allocator overhead are not included.

The ignored complete result has SHA-256
`3c893ccfa20ead693a6d497ec630aa7c9aa235fad0b17d75df9bfefa0f09d4f4`;
the checkpoint has SHA-256
`da29215efa9fa9a080ce6f9b2eab8fbbaf50e23806c580f2cca5648e041416ea`.
Completed replay validated all eighteen points without launching new
measurements, and both file hashes remained unchanged.

The large `mr` improvement survives complete entropy coding and framing,
without a ratio or workspace cost on these inputs. This supports a separately
frozen all-member whole-codec campaign. It does not establish a universal
non-regression guarantee or authorize production promotion. Keep the public
default unchanged while the full-Corpus audit remains pending.

### BM-0096: All-member whole-codec contextual rANS best-length-probe results

On 2026-09-23, the frozen
`silesia-contextual-rans-best-length-probe-full-v1.json` campaign completed
all 72 records at clean revision
`764aa44a9d26403f361fa77f93676a5d28145867`: twelve members, three independent
baseline/probe pairs each, in declared order. No child failed or timed out.
The build was MSVC 19.51.36252.0 x64 Release, `/O2 /Ob2 /DNDEBUG`, with
executable SHA-256
`95b462d4b269071442017bcad09625ab4a57627c7d83d1bdbaa0b2aafb612418`,
project SHA-256
`68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`,
and manifest SHA-256
`53891532fec9e3c46804df942e398b281d054bbd48f4fa33001a1b9e0e8a1cd5`.

Both strategies used the public contextual rANS 4 MiB frame/window profile,
5..258-byte matches, and unchanged HashChain buckets. Timing boundaries
match BM-0095: uninstrumented encoder and ordinary decoder `process` calls,
including validation, tokenization, model construction and framing inside
those calls, but excluding allocation, construction/destruction, file I/O,
oracle generation, digests and post-run comparisons. These are whole-codec
process times, not isolated matcher times or child wall times.

| Member | Baseline encode median | Probe encode median | Encode speedup | Baseline decode median | Probe decode median |
| --- | ---: | ---: | ---: | ---: | ---: |
| `dickens` | 5.891207 s | 5.460138 s | 1.079x | 0.249587 s | 0.254036 s |
| `mozilla` | 70.065247 s | 35.007636 s | 2.001x | 1.334681 s | 1.327592 s |
| `mr` | 59.520798 s | 21.084188 s | 2.823x | 0.242267 s | 0.243436 s |
| `nci` | 35.976682 s | 18.854191 s | 1.908x | 0.188666 s | 0.184818 s |
| `ooffice` | 1.413045 s | 1.189423 s | 1.188x | 0.211187 s | 0.211385 s |
| `osdb` | 1.628375 s | 1.335878 s | 1.219x | 0.223298 s | 0.223713 s |
| `reymont` | 7.847577 s | 6.690958 s | 1.173x | 0.111534 s | 0.112891 s |
| `samba` | 17.337052 s | 6.345758 s | 2.732x | 0.327718 s | 0.330774 s |
| `sao` | 1.829933 s | 1.769141 s | 1.034x | 0.323511 s | 0.323752 s |
| `webster` | 38.318193 s | 24.317771 s | 1.576x | 0.755611 s | 0.759779 s |
| `xml` | 0.787055 s | 0.616491 s | 1.277x | 0.040834 s | 0.041015 s |
| `x-ray` | 1.196079 s | 1.175698 s | 1.017x | 0.370937 s | 0.368497 s |

Summed per-member encode medians are 241.8112430 seconds for baseline and
123.8472713 seconds for probe: a 1.952496x aggregate speedup. This divides
summed times, rather than averaging speedup ratios. All twelve encode
medians improved, with a smallest observed speedup of 1.017335x on `x-ray`.
The corresponding decode sums are 4.3798314 and 4.3816869 seconds, a
0.999577x ratio. Decode medians were slower on `dickens`, `mr`, `ooffice`,
`osdb`, `reymont`, `samba`, `sao`, `webster`, and `xml`; the worst ratio was
0.982488x on `dickens`. These observations are retained even though the
decoder and its input bytes are unchanged. Small differences remain
sensitive to noise; fixed-order measurements on one machine do not prove
a universal non-regression guarantee.

Every private archive matched the public baseline oracle byte for byte,
and every ordinary decode recovered the original input. Archive hashes,
sizes, ratios and workspace were stable across attempts and strategies.

| Member | Archive bytes | Encoded/input ratio |
| --- | ---: | ---: |
| `dickens` | 3,265,887 | 0.320422301 |
| `mozilla` | 18,954,972 | 0.370066270 |
| `mr` | 3,386,820 | 0.339681888 |
| `nci` | 2,465,068 | 0.073466912 |
| `ooffice` | 3,028,821 | 0.492315747 |
| `osdb` | 3,286,689 | 0.325876658 |
| `reymont` | 1,591,187 | 0.240099366 |
| `samba` | 4,851,746 | 0.224551337 |
| `sao` | 5,270,047 | 0.726708176 |
| `webster` | 10,378,367 | 0.250330238 |
| `xml` | 549,164 | 0.102738117 |
| `x-ray` | 5,450,402 | 0.643172957 |

Every member required 132,129,769 bytes of encoder workspace and
114,017,257 bytes of decoder workspace on both strategies. Sequential
peak codec workspace was 132,129,769 bytes, within the 128 MiB internal
limit. This is not RSS: input, oracle, output, alignment slack, objects and
allocator overhead are excluded. The optimization adds no queried workspace.

The ignored complete result
`silesia-contextual-rans-best-length-probe-full-v1.json` retains all raw
records, input/archive hashes and throughput fields; its SHA-256 is
`d745954d937f90f084365f30f150bd2f67dd8d8daa31b542c09bdf91c8e8761b`.
The complete checkpoint SHA-256 is
`7483cea0c6315fe6c7b8c4621ebaddd5b28217a6f03dbb8ec54edf273eca6df4`.
Completed replay revalidated all 72 records without launching measurements
or changing either file hash.

The full-Corpus audit supports considering production adoption: the encode
gain survives complete entropy coding and framing without changing bytes,
ratio or workspace. Adoption still requires a separate scope and regression
review; this result does not change the public strategy, API or format.

### BM-0097: Post-switch full contextual rANS probe comparison

On 2026-09-24, the frozen BM-0096 manifest and runner completed a new 72-record
campaign at clean revision `ef879a3b7625fc4ac6f6e98c58e55b508f712b21`,
after the production HashChain route enabled best-length probing. This is twelve
Silesia members, three independent processes per member and route, with a
4 MiB frame/window and 5..258-byte matches. The explicit `hash-chain-exact`
route remains the independent no-probe control; the explicit probe route and
public encoder were checked against the same archive bytes on every invocation.
The measurement uses the same whole-codec process boundaries as BM-0096.

Build identity: MSVC 19.51.36252.0 x64 Release, `/O2 /Ob2 /DNDEBUG`, executable
SHA-256 `2b53c060e41c94632ef5809c6ed728ceba980878ea211be3a5ab9757d12b2786`,
project SHA-256 `68ef20fe8a17922c2cd24405cfcdef78b3e59bc28b55b48a2aa761c7e68bff4d`,
manifest SHA-256 `53891532fec9e3c46804df942e398b281d054bbd48f4fa33001a1b9e0e8a1cd5`.
The result and checkpoint use separate ignored `probe-post-switch-ef879a3b`
paths; neither changes the historical BM-0096 result.

| Member | No-probe encode median | Probe encode median | Speedup |
| --- | ---: | ---: | ---: |
| `dickens` | 5.802 s | 5.246 s | 1.106x |
| `mozilla` | 82.510 s | 34.228 s | 2.411x |
| `mr` | 73.862 s | 20.924 s | 3.530x |
| `nci` | 43.566 s | 18.984 s | 2.295x |
| `ooffice` | 1.486 s | 1.216 s | 1.222x |
| `osdb` | 1.707 s | 1.371 s | 1.245x |
| `reymont` | 8.130 s | 6.783 s | 1.199x |
| `samba` | 22.089 s | 6.171 s | 3.580x |
| `sao` | 1.870 s | 1.810 s | 1.033x |
| `webster` | 34.276 s | 25.719 s | 1.333x |
| `xml` | 0.857 s | 0.666 s | 1.288x |
| `x-ray` | 1.335 s | 1.344 s | 0.993x |

The sums of per-member encode medians are 277.4899617 seconds for no-probe
and 124.4603120 seconds for probe, a 2.229546x ratio. Eleven members improved;
`x-ray` slowed by about 0.66%. Decode sums are 4.5553300 and 4.5047356
seconds, a 1.011231x ratio. Six decode medians were slower with the probe
archive (`mr`, `nci`, `osdb`, `sao`, `webster`, `xml`), worst 0.919335x on
`osdb`. The decoder and archive bytes are identical, so decode timing
differences are measurement variation, not a claimed decoder optimization.

All 72 records passed the runner's archive identity and round-trip checks.
All twelve archive hashes and sizes also match the earlier BM-0096 campaign.
Ratios and queried workspace are unchanged. Encoder workspace is 132,129,769
bytes and decoder workspace 114,017,257 bytes for every member and route;
the sequential peak is 132,129,769 bytes, excluding RSS and other allocations
as described in BM-0096. Completed replay validated the saved records without
running measurements again. The ignored result SHA-256 is
`3132e87ab0e794762f0c640020a45b53d07aa01a3e98f481764235217ea48131`;
the checkpoint SHA-256 is
`375478088ea1498f7a75542c441f7676528b0354def34523cb87b2ec5ae3bbe2`.
The source/build difference means BM-0096 timings are context rather than a
controlled cross-revision speed comparison. External CI and cross-platform
archive verification remain the final adoption gates.

### BM-0098: Contextual Dynamic Range 64-KiB `mozilla` ratio baseline

On 2026-09-24, with the working tree at revision
`be380bb8edfea819bb61286964429a508dd202a3`, the existing MSVC Release CLI
reproduced the maintainer's `mozilla` output size:

```text
marc encode --codec lzss-contextual-dynamic-range mozilla output.marc
```

The external Silesia input has 51,220,480 bytes and SHA-256
`657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b`.
The 20,085,366-byte output has SHA-256
`95eec4f4450a991c75af5dc805c3bde02cafb20d4f21197a55145f2338cd8317`.
An ordinary CLI decode recovered the exact input SHA-256. The CLI executable
SHA-256 was
`f1720f1f9f2c582f84e863b7272761ac3b2bd257c1267477d75c2a72d3629a26`.
The executable predates the release-publication documentation commit, with no
intervening executable source change. Generated input/output files remain in
ignored local directories.

The maintainer reported gzip 18,994,139, bzip2 17,914,392 and lzma
13,365,111 bytes on the named member, each with `-9v`. Exact external command
lines, tool versions and container details are still to be frozen. Against
that provisional gzip result, the 64-KiB marc deficit is 1,091,227 bytes.
The first candidate is a bounded
diagnostic of short-match opportunities and actual coded size, as specified
in [the 64-KiB ratio study](design/lzss-contextual-ratio-64k.md). This is
one-member motivation, not whole-corpus or new-variant performance evidence.

### BM-0099: 64-KiB `mozilla` short-match opportunity diagnostic

On 2026-09-24, the private MSVC Release diagnostic was run against the same
51,220,480-byte external `mozilla` input and independent 65,536-byte frames:

```text
marc_lzss_short_match_diagnostic mozilla
```

It uses an exact fixed-capacity 3/4-byte prefix index and the production
exact HashChain typed-token parser at minimum length 5. The index
matched an exhaustive small-input oracle; the README smoke test checked
reported counter relationships. No corpus file or generated report is tracked.

| Observation | Count |
| --- | ---: |
| Frames | 782 |
| Baseline literals | 14,711,301 |
| Baseline matches | 3,065,042 |
| Baseline matched bytes | 36,509,179 |
| All-position equal 3-byte prefixes | 37,298,509 |
| All-position equal 4-byte prefixes | 31,922,000 |
| Parser-visited equal 3-byte prefixes | 6,710,314 |
| Parser-visited equal 4-byte prefixes | 4,024,782 |
| Literal positions with equal 4-byte prefix | 959,740 |
| Literal positions with equal 3-byte but no 4-byte prefix | 2,685,532 |

The existing match-length histogram is: 5: 450,636; 6..7: 1,056,253;
8..15: 1,127,439; 16..31: 313,475; 32..63: 86,724; 64..127: 17,103;
128..258: 13,412. The nearest-distance buckets for literal 3-only
opportunities (1..16, 17..256, 257..4096, 4097..65535) are 143,638,
766,726, 953,607, 821,561. For literal 4-byte opportunities they are
18,000, 211,328, 336,523, 393,889. These are counts under the existing
parser, not independent hypothetical replacements. Accepted short matches
would skip later positions and alter range-model history. The diagnostic
therefore establishes available prefixes but neither a coded-bit saving nor
gzip parity. BM-0100 measures the baseline modeled events and complete
payload size before judging a new format variant.

### BM-0100: 64-KiB Contextual Dynamic Range modeled-event baseline

On 2026-09-24, the private diagnostic was extended to obtain the exact
production HashChain typed tokens for each 65,536-byte frame, feed them to
marc's existing field-context mapper, and plan each complete Dynamic Range
payload. On the external Silesia `mozilla` input used in BM-0098 and BM-0099:

| Baseline measure | Count |
| --- | ---: |
| Token-kind symbols | 17,776,343 |
| Literal symbols | 14,711,301 |
| Length-class symbols | 3,065,042 |
| Distance-class symbols | 3,065,042 |
| Length bypass operations / bits | 2,614,406 / 5,467,985 |
| Distance bypass operations / bits | 3,052,013 / 25,892,152 |
| Total modeled operations | 44,284,147 |
| Total arithmetic decisions | 69,977,865 |
| Sum of complete Range frame payloads | 20,022,694 bytes |
| Stream and frame overhead | 62,672 bytes |
| Predicted archive | 20,085,366 bytes |

The predicted archive exactly equals the independently measured CLI archive
from BM-0098. Tracked README and generated two-frame tests separately compare
the diagnostic prediction against CLI output, so the result is not supported
only by the external corpus. The 112-byte stream header plus 782 pairs of
64-byte frame header and 16-byte Range descriptor account for overhead.
Symbol and bypass counts describe the current representation, not additive
compressed-bit costs or a forecast for a shorter-match format. No new
encoder policy, archive variant, or compression-ratio improvement is claimed.

### BM-0101: Bounded 64-KiB short-match candidate pilot

On 2026-09-24, the private MSVC Release benchmark measured a prefix of the
external Silesia `mozilla` input. No corpus bytes or generated report are
tracked. The command was:

```text
marc_lzss_short_match_candidate_benchmark mozilla 16 65536
```

It compares the published minimum-5 HashChain typed-token path against the
reserved minimum-3 variant's exhaustive candidate selector (eligibility 3,
4, or 5) on the same frame partition. Both reported sizes include a 112-byte
stream header and complete 64-byte frame header, 16-byte Range descriptor,
and planned payload for each frame. The private side actually encodes every
selected frame and decodes it byte-for-byte against the sampled source.

| Measure | Published baseline | Reserved candidate |
| --- | ---: | ---: |
| Sample | 1,048,576 bytes / 16 frames | same |
| Complete sample archive | 670,917 bytes | 670,231 bytes |
| Size difference | reference | -686 bytes (-0.102%) |
| Selected thresholds 3 / 4 / 5 | not applicable | 2 / 13 / 1 frames |
| Size-planning time | 0.067 s | not measured separately |
| Selection plus frame-encode time | not measured | 211.285 s |
| Private frame-decode time | not measured | 0.113 s |

The single first frame measured 16,163 versus 15,815 bytes; the smaller
16-frame gain shows why that isolated observation must not be extrapolated.
The timing columns are deliberately **not** an encode-speed comparison:
baseline timing only plans sizes, whereas candidate timing performs repeated
exhaustive search, three complete size plans, and a final encode. The
candidate's observed latency alone rules out the current reference parser
for public use. This is a bounded prefix pilot, not a whole-`mozilla` or
whole-Silesia result, and does not establish `gzip -9v` parity. A validated
short-prefix match index and corpus-wide measurement are still required.

### BM-0102: Indexed short-match candidate across all Silesia members

On 2026-09-24, the private MSVC Release benchmark used each external
Silesia member in full, with 65,536-byte frames and a 65,536-byte window:

```text
marc_lzss_short_match_candidate_benchmark <member> 1024 65536 indexed
```

Every selected reserved frame was encoded and privately decoded against its
source. The complete-size column adds the fixed 112-byte stream header to
the actual selected frame lengths, but no public archive was produced. The
published baseline column uses the production HashChain token parser and
complete Range payload planner; BM-0100 and tracked CLI-identity tests
validate that baseline size method. No corpus bytes or generated reports are
tracked.

| Member | Published baseline | Reserved indexed | Difference |
| --- | ---: | ---: | ---: |
| dickens | 4,097,287 | 4,127,385 | +30,098 |
| mozilla | 20,085,366 | 19,824,809 | -260,557 |
| mr | 3,596,195 | 3,621,840 | +25,645 |
| nci | 3,584,048 | 3,602,900 | +18,852 |
| ooffice | 3,233,855 | 3,193,254 | -40,601 |
| osdb | 4,112,363 | 4,133,678 | +21,315 |
| reymont | 2,028,288 | 2,027,716 | -572 |
| samba | 5,756,275 | 5,757,403 | +1,128 |
| sao | 5,616,349 | 5,430,503 | -185,846 |
| webster | 12,974,519 | 13,047,813 | +73,294 |
| x-ray | 6,000,150 | 5,840,373 | -159,777 |
| xml | 765,900 | 768,851 | +2,951 |
| **Total** | **71,850,595** | **71,376,525** | **-474,070 (-0.660%)** |

Five members improve and seven regress. The `mozilla` result remains
830,670 bytes above the user's provisional `gzip -9v` size of 18,994,139.
For the first sixteen `mozilla` frames, the indexed selector produced the
same 670,231-byte size and 2/13/1 threshold counts as the exhaustive
selector, while its selection-plus-encode time fell from 211.285 s to
0.303 s. The full `mozilla` indexed run selected thresholds 3/4/5 on
477/276/29 frames and took 17.957 s to select and encode, plus 2.445 s
to decode. Across all twelve members, the indexed selection-plus-encode
times summed to 55.432 s; baseline size-planning times summed to 8.445 s.
Those two timing quantities perform different work and are **not** an
encode-throughput ratio. This candidate remains private. The mixed member
result and unmet gzip target rule out a claim that short-match eligibility
alone solves the 64-KiB compression-ratio deficit.

### BM-0103: Separate short-match eligibility from model cost

On 2026-09-24, the private MSVC Release benchmark repeated BM-0102's full
external Silesia run with the same 65,536-byte frame/window partition and
`indexed` search mode. It additionally measured every fixed eligibility
3/4/5 candidate and recoded eligibility-5 tokens through the published
31-context model. The benchmark compares every eligibility-5 token field
(`kind`, `literal`, `distance`, `length`) with the production HashChain
tokenizer's result. No corpus bytes or generated reports are tracked.

| Member | Frames with equal tokens / all | Published size | Reserved eligibility-5 size | Eligibility-5 excess | Selected size |
| --- | ---: | ---: | ---: | ---: | ---: |
| dickens | 156/156 | 4,097,287 | 4,152,284 | +54,997 | 4,127,385 |
| mozilla | 782/782 | 20,085,366 | 20,199,183 | +113,817 | 19,824,809 |
| mr | 153/153 | 3,596,195 | 3,651,423 | +55,228 | 3,621,840 |
| nci | 512/512 | 3,584,048 | 3,602,900 | +18,852 | 3,602,900 |
| ooffice | 94/94 | 3,233,855 | 3,258,508 | +24,653 | 3,193,254 |
| osdb | 154/154 | 4,112,363 | 4,133,678 | +21,315 | 4,133,678 |
| reymont | 102/102 | 2,028,288 | 2,048,580 | +20,292 | 2,027,716 |
| samba | 330/330 | 5,756,275 | 5,795,066 | +38,791 | 5,757,403 |
| sao | 111/111 | 5,616,349 | 5,640,677 | +24,328 | 5,430,503 |
| webster | 633/633 | 12,974,519 | 13,096,607 | +122,088 | 13,047,813 |
| x-ray | 130/130 | 6,000,150 | 6,051,020 | +50,870 | 5,840,373 |
| xml | 82/82 | 765,900 | 771,812 | +5,912 | 768,851 |
| **Total** | **3,239/3,239** | **71,850,595** | **72,401,738** | **+551,143** | **71,376,525** |

Recoding the exact eligibility-5 tokens through the published model gave
the published size on every member. Thus the observed eligibility-5 excess
comes from the reserved token mapping/context representation, not different
matches, for this corpus and configuration. This does not identify the
specific expensive contexts. The selected reserved frames beat the
published baseline in 1,298 frames, tied in 4, and lost in 1,937; their
673,183 saved bytes minus 199,113 extra bytes yield the 474,070-byte net
gain already reported in BM-0102. Fixed eligibility-3/4 sizes and those
framewise counters are emitted by the benchmark for further diagnosis.
The stream header fixes the variant, so the baseline cannot simply be
selected for individual frames within the current reserved stream. No
public format or encoder-policy change follows from this diagnostic.

### BM-0104: Locate the short-match representation's extra bypass work

On 2026-09-24, the private MSVC Release benchmark repeated BM-0103's
full-Silesia, 65,536-byte-frame run. It counted the actual modeled
operations for the same eligibility-5 tokens under the published 31-context
and reserved 32-context mappings. A bypass operation is attributed to the
preceding length or distance symbol. The counts below are *logical bypass
bits*, not independently byte-aligned payload sizes or exact contributions
to the Range-coded output. No corpus bytes are tracked.

| Member | Matches in both mappings | Extra reserved length-bypass bits | Extra reserved distance-bypass bits |
| --- | ---: | ---: | ---: |
| dickens | 1,183,792 | 909,642 | 0 |
| mozilla | 3,065,042 | 1,840,893 | 0 |
| mr | 820,053 | 691,845 | 0 |
| nci | 970,567 | 299,846 | 0 |
| ooffice | 414,595 | 318,956 | 0 |
| osdb | 505,105 | 302,232 | 0 |
| reymont | 648,312 | 410,358 | 0 |
| samba | 1,160,944 | 648,143 | 0 |
| sao | 410,075 | 349,940 | 0 |
| webster | 3,266,954 | 2,103,213 | 0 |
| x-ray | 409,092 | 408,618 | 0 |
| xml | 199,076 | 92,763 | 0 |
| **Total** | **13,053,607** | **8,376,449** | **0** |

Both mappings also emitted the same number of length and distance symbols
for each member. The published mapping classifies `length - 4`; the
reserved mapping classifies `length - 2`, shifting many existing long
matches into wider length classes. The extra 8,376,449 logical bypass bits
would be about 1,047,056 bytes if packed independently, but the observed
eligibility-5 archive excess is only 551,143 bytes (BM-0103). Different
adaptive symbol probabilities and Range-coder state account for the fact
that these figures are not additive. The extra bypass work identifies a
promising representation hypothesis, not a measured byte-level attribution
or permission to change the reserved decoder-visible mapping silently.

### BM-0105: Measure the private short-length escape over complete frames

On 2026-09-25, the private MSVC Release benchmark measured all twelve
locally supplied Silesia members with 65,536-byte raw frames, a 65,536-byte
window, indexed search, and at most 1,024 frames per member. Every member
fits that bound. All columns include the 112-byte stream header and complete
frame headers, Range descriptors, and payloads. The published column uses
the production HashChain token parser and complete payload planner; the two
reserved columns encode each selected complete frame and verify its decoded
bytes. Eligibility 3/4/5 is chosen independently per frame by minimum
serialized size, with higher eligibility on ties. Corpus bytes and generated
archives are not tracked.

| Member | Published baseline | Reserved 2/7 + 1/6 | Escape 2/8 + 1/7 | Escape vs. baseline |
| --- | ---: | ---: | ---: | ---: |
| dickens | 4,097,287 | 4,127,385 | 4,097,626 | +339 |
| mozilla | 20,085,366 | 19,824,809 | 19,824,381 | -260,985 |
| mr | 3,596,195 | 3,621,840 | 3,588,975 | -7,220 |
| nci | 3,584,048 | 3,602,900 | 3,584,936 | +888 |
| ooffice | 3,233,855 | 3,193,254 | 3,192,736 | -41,119 |
| osdb | 4,112,363 | 4,133,678 | 4,112,656 | +293 |
| reymont | 2,028,288 | 2,027,716 | 2,022,925 | -5,363 |
| samba | 5,756,275 | 5,757,403 | 5,752,366 | -3,909 |
| sao | 5,616,349 | 5,430,503 | 5,454,241 | -162,108 |
| webster | 12,974,519 | 13,047,813 | 12,975,833 | +1,314 |
| x-ray | 6,000,150 | 5,840,373 | 5,988,302 | -11,848 |
| xml | 765,900 | 768,851 | 765,818 | -82 |
| **Total** | **71,850,595** | **71,376,525** | **71,360,795** | **-489,800 (-0.682%)** |

The escape identity improves eight members and regresses four against the
published baseline. Its total is 15,730 bytes below the earlier reserved
mapping, but that is not uniform: `sao` and `x-ray` lose 23,738 and 147,929
bytes relative to 2/7 + 1/6. On `mozilla`, escape eligibility-5 alone is
20,086,821 bytes, only 1,455 above the published baseline, compared with
20,199,183 for the earlier mapping. The per-frame escape selector chooses
eligibility 3/4/5 on 515/186/81 `mozilla` frames and reaches 19,824,381
bytes. This remains 830,242 bytes above the user's provisional `gzip -9v`
size of 18,994,139. The 51,220,480-byte `mozilla` run took 17.875 s for
escape candidate selection plus encoding and 2.533 s for its private frame
decoding, versus 3.163 s for *baseline size planning*; the latter is a
different workload, not an encode-throughput comparison. The representation
penalty is substantially reduced, but short-match eligibility and this
mapping alone do not meet the stated compression target. Public admission
and an interoperability archive remain gated on broader ratio, speed,
workspace, malformed-input, and cross-platform evidence.

### BM-0106: Bound framewise profile-switching headroom

On 2026-09-25, the private MSVC Release benchmark repeated BM-0105's full
local Silesia run. The two-way diagnostic sums, for each complete raw frame,
the smaller of the published-baseline and 2/8 + 1/7 escape frame sizes. The
three-way diagnostic also permits the earlier private 2/7 + 1/6 candidate.
Each total includes one 112-byte stream header. Neither is a decodable
archive: current identities are fixed for a whole stream, and any future
per-frame identity signal would add bytes. Every private candidate selected
for measurement is decoded and compared with its raw frame; corpus bytes
and generated archives remain untracked.

| Member | Escape stream | Baseline/escape lower bound | Three-way lower bound |
| --- | ---: | ---: | ---: |
| dickens | 4,097,626 | 4,097,287 | 4,097,287 |
| mozilla | 19,824,381 | 19,824,219 | 19,803,849 |
| mr | 3,588,975 | 3,588,782 | 3,588,782 |
| nci | 3,584,936 | 3,584,048 | 3,584,048 |
| ooffice | 3,192,736 | 3,192,725 | 3,189,770 |
| osdb | 4,112,656 | 4,112,363 | 4,112,363 |
| reymont | 2,022,925 | 2,022,898 | 2,022,898 |
| samba | 5,752,366 | 5,751,918 | 5,743,367 |
| sao | 5,454,241 | 5,454,241 | 5,430,503 |
| webster | 12,975,833 | 12,974,519 | 12,974,349 |
| x-ray | 5,988,302 | 5,988,230 | 5,840,373 |
| xml | 765,818 | 765,716 | 765,440 |
| **Total** | **71,360,795** | **71,356,946** | **71,153,029** |

Across 3,239 frames, escape beats the published baseline in 1,235, ties in
10, and loses in 1,994. On `mozilla` specifically the counts are 693/6/83;
the 261,147 saved bytes and 162 excess bytes yield the measured 260,985-byte
net gain. Choosing the published frame only on those 83 losing frames saves
merely 162 more bytes than the escape stream. Even the impossible, free
three-way switch saves only 20,532 bytes beyond escape and remains 809,710
bytes above the user's provisional `gzip -9v` size of 18,994,139. Thus
per-frame choice among these existing identities cannot explain or close
the remaining `mozilla` gap. Its small theoretical headroom does not justify
a mixed-identity stream extension at this stage; parsing or entropy modeling
must be investigated separately before a new format is proposed.

### BM-0107: Attribute mozilla's modeled field costs

On 2026-09-25, the private MSVC Release benchmark repeated the full
51,220,480-byte `mozilla` measurement with 782 independent 65,536-byte
frames and indexed search. It replayed the published and decoded winning
escape tokens through their respective frequency-one models. Symbol scores
sum log2(total/frequency) before each update, including specified rescaling;
bypass costs are their logical bit counts. The table divides these scores
by eight for readability. They are byte-equivalent information quantities,
not separately serialized fields. Every selected private frame round-tripped.

| Field | Published byte-equivalent | Escape byte-equivalent | Change |
| --- | ---: | ---: | ---: |
| Token kind | 1,239,466.379 | 1,281,414.164 | +41,947.786 |
| Literal symbol | 12,822,774.873 | 8,998,384.169 | -3,824,390.705 |
| Length class | 788,825.575 | 1,221,384.801 | +432,559.226 |
| Distance class | 1,247,884.594 | 2,085,417.420 | +837,532.825 |
| Length bypass | 683,498.125 | 921,344.625 | +237,846.500 |
| Distance bypass | 3,236,519.000 | 5,250,102.875 | +2,013,583.875 |
| **Total** | **20,018,968.546** | **19,758,048.053** | **-260,920.493** |

Literal tokens fall from 14,711,301 to 9,444,672 while Match tokens rise
from 3,065,042 to 4,905,913. The modeled literal saving of 3,824,390.705
byte-equivalents is offset by 3,563,470.212 in the remaining fields. This
locates a large cost in match metadata, especially distance bypass, but
does not prove that any individual Match loses: token choices change later
parsing and model state as well. A distance-sensitive short-match admission
experiment is consequently a stronger next candidate than profile switching.

Both streams have 62,672 actual framing bytes (112 + 782 * 80). Subtracting
those bytes and modeled information from actual archive size leaves
3,725.454 bytes for the published path and 3,660.947 for escape, consistent
with small integer-coder/termination costs rather than the approximately
830,000-byte target gap. No coder-overhead optimization is justified by this
aggregate measurement alone.

The escape Literal group's empirical within-frame/context histogram score
is 67,376,237.378 bits, against 71,987,073.348 adaptive bits, a difference
of 576,354.496 byte-equivalents. This suggests a separate model-initialization
or context-sharing experiment may be useful. The histogram uses future
counts and charges no model storage; it is neither a feasible encoded size
nor a universal lower bound for nonstationary adaptive data. It cannot be
added to a predicted parser gain without remeasuring the combined system.
All floating-point work is benchmark-only; actual codec arithmetic and
stream bytes are unchanged.

### BM-0108: Distance-capped short matches on mozilla

On 2026-09-25, the private MSVC Release benchmark processed all 51,220,480
bytes of `mozilla` with indexed search and 782 independent 65,536-byte frames.
The optional `distance-policies` experiment reparses each frame for seven
policies. Caps are inclusive; zero disables that match length. Lengths five
and above retain existing eligibility. Rejected matches emit one literal
and resume search at the next byte, rather than literalizing the entire match.
Every policy frame was actually encoded and decoded with private variant 8.

| Policy | Length-three distance cap | Length-four distance cap | Archive bytes |
| --- | ---: | ---: | ---: |
| 0 (minimum-five control) | 0 | 0 | 20,086,821 |
| 1 (minimum-four control) | 0 | 65,536 | 19,977,575 |
| 2 (minimum-three control) | 65,536 | 65,536 | 19,844,180 |
| 3 | 256 | 4,096 | 19,671,252 |
| 4 | 1,024 | 16,384 | 19,642,913 |
| 5 | 4,096 | 65,536 | 19,685,258 |
| 6 | 16,384 | 65,536 | 19,761,230 |
| Per-frame minimum | varies | varies | **19,633,027** |

Totals charge one 112-byte stream header and actual serialized frame sizes.
All policies share one private representation, so selection requires no
decoder-visible policy switch. The benchmark sums sizes; it does not publish
a new public stream API. The three controls reproduce existing minimum-length
totals. The selected total improves BM-0107's 19,824,381 bytes by 191,354,
and the published baseline by 452,339, but remains 638,888 bytes above the
user-reported `gzip -9v` result. Per-frame selection gains only 9,886 bytes
beyond the best fixed policy in this sample.

This is a single-member pilot, not evidence of a generally optimal distance
cap. Full-corpus ratio, encode/decode time and workspace measurements remain
admission gates. Existing benchmark timers do not include this optional
policy sweep and must not be interpreted as its throughput. Public defaults,
APIs and stream identities are unchanged.

### BM-0109: Full-Silesia distance-policy screening

On 2026-09-25, MSVC Release processed all twelve verified Silesia files
(211,938,580 bytes, 3,239 frames) sequentially with the BM-0108 grid. Invoke
`marc_lzss_short_match_candidate_benchmark <file> 1024 65536 indexed distance-policies`
for each member. Every policy frame round-tripped. Local per-member JSON
checkpoints retained executable/input SHA-256 and arguments; a second batch
invocation reused all twelve completed reports without launching benchmarks.
Corpus verification confirmed all twelve expected files. Neither corpus nor
local checkpoint files are included in the repository.

| Member | Published bytes | Prior escape bytes | Fixed policy 4 bytes | Selected bytes |
| --- | ---: | ---: | ---: | ---: |
| dickens | 4,097,287 | 4,097,626 | 4,133,853 | 4,097,624 |
| mozilla | 20,085,366 | 19,824,381 | 19,642,913 | 19,633,027 |
| mr | 3,596,195 | 3,588,975 | 3,701,052 | 3,588,937 |
| nci | 3,584,048 | 3,584,936 | 3,640,360 | 3,584,906 |
| ooffice | 3,233,855 | 3,192,736 | 3,158,984 | 3,155,517 |
| osdb | 4,112,363 | 4,112,656 | 4,202,605 | 4,112,656 |
| reymont | 2,028,288 | 2,022,925 | 2,008,887 | 1,999,706 |
| samba | 5,756,275 | 5,752,366 | 5,739,373 | 5,718,282 |
| sao | 5,616,349 | 5,454,241 | 5,403,526 | 5,403,457 |
| webster | 12,974,519 | 12,975,833 | 13,033,832 | 12,941,027 |
| x-ray | 6,000,150 | 5,988,302 | 6,041,700 | 5,970,116 |
| xml | 765,900 | 765,818 | 772,096 | 763,858 |
| **Total** | **71,850,595** | **71,360,795** | **71,479,181** | **70,969,113** |

Selection saves 391,682 bytes versus prior escape and 881,482 versus published.
It retains small published-baseline regressions on dickens (+337), nci (+858)
and osdb (+293). Policy 4, best fixed policy for mozilla, loses 118,386 bytes
against prior escape over the full corpus. A single-member winner is therefore
not suitable as an unconditional default.

| Policy | Total bytes | Parse + encode seconds | Decode seconds |
| --- | ---: | ---: | ---: |
| 0 | 71,856,743 | 11.984297 | 10.233165 |
| 1 | 72,054,749 | 11.013533 | 9.317262 |
| 2 | 72,247,609 | 10.729982 | 8.286260 |
| 3 | 71,193,193 | 11.423323 | 9.384815 |
| 4 | 71,479,181 | 11.071976 | 8.966292 |
| 5 | 71,842,542 | 10.816582 | 8.588972 |
| 6 | 72,021,258 | 10.727351 | 8.354795 |

These are single sequential observations, not statistically stable rankings.
Each policy timer includes parsing and one actual complete-frame encoding;
decode excludes output comparison. The prior multi-candidate escape selector
measured 57.962188 seconds to encode and 9.387658 to decode. Its work differs
from a fixed single-pass policy, so this is not a like-for-like codec speedup.
The diagnostic sweep repeats all seven policies as well as the prior selector;
its time is not the cost of a production selector. Policy evaluation reuses
existing bounded token, operation, finder and frame buffers, rather than
retaining seven copies. Process peak memory was not measured in this run.

Policy 3 is the smallest fixed policy in aggregate; selection gains another
224,080 bytes. The next experiment should measure smaller candidate sets and
their selection cost, retaining per-member reporting and round trips. Neither
the seven-pass sweep nor a fixed cap is admitted to the public codec yet.

### BM-0110: Reduced short-distance candidate sets

On 2026-09-25, repeat BM-0109's complete twelve-file, 3,239-frame MSVC
Release measurement using the six sets fixed in DD-1210. All seven policy
totals reproduced the previous per-member results exactly; every policy
frame round-tripped. Local hash-bound checkpoints again resumed all twelve
completed files without rerunning them. Sets use only their named members;
the prior selector is not implicitly included.

| Policies | Total bytes | Excess over seven policies | Member encode seconds sum |
| --- | ---: | ---: | ---: |
| 0, 3 | 71,044,174 | 75,061 | 22.690927 |
| 0, 4 | 71,072,529 | 103,416 | 22.344193 |
| 3, 4 | 71,129,372 | 160,259 | 21.768512 |
| **0, 3, 4** | **70,980,791** | **11,678** | **33.401815** |
| 0, 2, 3, 4 | 70,978,651 | 9,538 | 43.814921 |
| 0 through 6 | 70,969,113 | 0 | 75.383769 |

| Member | Policies 0,3,4 bytes | Seven-policy bytes | Excess |
| --- | ---: | ---: | ---: |
| dickens | 4,097,624 | 4,097,624 | 0 |
| mozilla | 19,636,009 | 19,633,027 | 2,982 |
| mr | 3,596,220 | 3,588,937 | 7,283 |
| nci | 3,584,906 | 3,584,906 | 0 |
| ooffice | 3,155,610 | 3,155,517 | 93 |
| osdb | 4,112,656 | 4,112,656 | 0 |
| reymont | 1,999,706 | 1,999,706 | 0 |
| samba | 5,718,530 | 5,718,282 | 248 |
| sao | 5,403,519 | 5,403,457 | 62 |
| webster | 12,941,027 | 12,941,027 | 0 |
| x-ray | 5,971,124 | 5,970,116 | 1,008 |
| xml | 763,860 | 763,858 | 2 |

The three-policy set saves 380,004 bytes against the prior escape selector,
retaining 97.02% of the seven-policy improvement. Its measured member-work
sum is 55.69% lower than the seven-policy sum in this run. This is screening
of component work, not a measured end-to-end speedup: the benchmark still
executes all policies, and no reduced selector retaining a winning frame is
implemented here. Timing remains a single-run observation. A fourth candidate
saves only another 2,140 bytes in aggregate, so {0,3,4} is the next bounded
selector implementation candidate, not yet a public default.

Most lost compression is on mr and mozilla. The reduced set makes mr 25
bytes larger than the published baseline and keeps the small dickens/nci/osdb
regressions; it is not a no-regression guarantee. Mozilla remains 641,870
bytes above the user's gzip result. Profile/model improvement remains needed
to meet that target even after reducing selection cost.

### BM-0111: Dedicated three-policy selector with retained output

On 2026-09-25, MSVC Release repeated the full 211,938,580-byte Silesia
measurement with the private DD-1211 selector. All 3,239 selected frames
round-tripped; each candidate size matched its independently measured policy
and each member total reproduced BM-0110's {0,3,4} column exactly. The total
remains **70,980,791 bytes**. Completed local checkpoints were reused without
rerunning benchmarks on a second invocation.

| Member | Prior selector encode seconds | Dedicated selector encode seconds | Dedicated decode seconds |
| --- | ---: | ---: | ---: |
| dickens | 3.776210 | 2.034262 | 0.514261 |
| mozilla | 17.892000 | 10.297775 | 2.604569 |
| mr | 3.702220 | 1.992032 | 0.428877 |
| nci | 4.957480 | 2.271944 | 0.373656 |
| ooffice | 1.923670 | 1.360654 | 0.444732 |
| osdb | 1.861410 | 1.416925 | 0.539631 |
| reymont | 2.841600 | 1.325626 | 0.192271 |
| samba | 4.046960 | 2.422792 | 0.721752 |
| sao | 2.168010 | 1.793694 | 0.773925 |
| webster | 10.678900 | 6.093757 | 1.542555 |
| x-ray | 2.215490 | 1.953037 | 0.842924 |
| xml | 0.660484 | 0.358358 | 0.086433 |
| **Total** | **56.724434** | **33.320856** | **9.065586** |

Prior selector decode took 9.093179 seconds. Dedicated encode time includes
three parses/encodes, buffer checks and provisional-winner copies, but not
I/O, diagnostic profiling or verification decoding. Unlike BM-0110's work
sum, this is the actual selector call. The same run's component sum was
33.359674 seconds; the small difference is measurement/cache variation, not
evidence that retention is free. This single sequential run suggests lower
selection cost, not a stable production throughput guarantee. The prior
selector also uses different parsing policies; this is not an isolated
measurement of copying versus reparsing.

Maximum supplied capacity was 8,978,602 bytes (raw input, token/operation
scratch, indexed finder, candidate frame and winner frame). Including the
existing frame contract's 9,272-byte fixed model charge gives 8,987,874 bytes
against the aggregate limit. This is not process RSS, allocator overhead or
all scalar stack state. Buffers are reused across candidates without heap
allocation inside the selector. Public codecs and defaults remain unchanged.

### BM-0112: Retained-selector repeatability

On 2026-09-25, two additional full-corpus runs used exactly the BM-0111
executable, arguments and inputs. The first additional run reversed member
order; the second restored it. Executable/input SHA-256, prior and retained
archive sizes and both memory counts matched for every member before a
checkpoint was accepted. Both completed runs resumed without relaunching
benchmarks. Each run covers 211,938,580 bytes and 3,239 frames, with all
candidate-size checks and retained-winner round trips enabled.

| Run | Prior encode seconds | Retained encode seconds | Prior decode seconds | Retained decode seconds |
| --- | ---: | ---: | ---: | ---: |
| BM-0111, original order | 56.724434 | 33.320856 | 9.093179 | 9.065586 |
| Repeat 1, reversed order | 57.083673 | 33.717818 | 9.237193 | 9.238103 |
| Repeat 2, original order | 57.113057 | 33.729774 | 9.271127 | 9.258336 |
| **Median** | **57.083673** | **33.717818** | **9.237193** | **9.238103** |

Encode ranges are 56.724434--57.113057 seconds for the prior selector and
33.320856--33.729774 for the retained selector. Every member in every run
encoded faster with the retained selector. Decode times remain close; no
decode speedup is claimed. All three runs produced **70,980,791 bytes**
for the retained selector, and maximum supplied/required memory counts
remained 8,978,602 / 8,987,874 bytes. These observations support repeatable
lower encode cost on this host and harness, not a cross-machine guarantee.

Within each frame the prior selector still runs before the retained selector,
with diagnostics and other policy trials between them. Corpus-order reversal
does not remove that cache/order bias. The selectors also use different
policies, so the result is not an isolated retention optimization comparison.
No runtime source changed for these repeats. The next compression-ratio
investigation can use the retained three-policy selector as a private
reference point; the remaining mozilla gap is not solved by this speed result.

### BM-0113: Attribute retained-winner field costs on mozilla

On 2026-09-25, the MSVC Release benchmark decoded and profiled the retained
winner for all 782 mozilla frames (51,220,480 bytes). Archive size remained
19,636,009 bytes, versus 19,824,381 for prior escape. Each winner round-tripped
before its decoded tokens were modeled; the last trial's scratch tokens are
not used. Diagnostic work is outside encode/decode timers and coding decisions.

| Field | Prior escape byte-equivalent | Retained byte-equivalent | Change |
| --- | ---: | ---: | ---: |
| Token kind | 1,281,414.164 | 1,339,295.924 | +57,881.759 |
| Literal symbol | 8,998,384.169 | 9,930,840.507 | +932,456.339 |
| Length class | 1,221,384.801 | 1,196,161.521 | -25,223.280 |
| Distance class | 2,085,417.420 | 1,824,879.672 | -260,537.748 |
| Length bypass | 921,344.625 | 863,472.500 | -57,872.125 |
| Distance bypass | 5,250,102.875 | 4,415,033.000 | -835,069.875 |
| **Total** | **19,758,048.053** | **19,569,683.124** | **-188,364.930** |

Match count falls from 4,905,913 to 4,456,695, while Literal count rises
from 9,444,672 to 10,620,073. Length/distance metadata saves 1,178,703.028
byte-equivalents, offset by 990,338.098 in literal/kind costs. This supports
the distance-cap mechanism, rather than suggesting a lower per-symbol coder
overhead. Actual savings are 188,372 bytes. With the unchanged 62,672 framing
bytes removed, retained modeled information leaves 3,653.876 bytes of actual
integer-coder/termination overhead, still far below the gzip target gap.

Retained Literal adaptive information is 79,446,724.060 bits; the empirical
within-frame/context score is 74,479,752.821 bits, a 620,871.405-byte-equivalent
difference. It uses future counts without paying model storage and is neither
an achievable size prediction nor a universal lower bound. In particular it
must not be subtracted from the remaining 641,870-byte gzip gap as a promised
gain. It motivates a bounded comparison of literal-model initialization and
update rules on fixed retained tokens before considering another format.
Any actual model change requires its own decoder-visible specification;
this diagnostic changes no existing representation or public default.

### BM-0114: Literal increment screening on fixed mozilla tokens

On 2026-09-25, MSVC Release replayed the same retained winners for all
782 mozilla frames, resetting each diagnostic context per frame. The actual
archive remained 19,636,009 bytes and all selected frames round-tripped.
Only literal update increments changed in the diagnostic; initialization,
tokens, context assignment and nonliteral model rules stayed fixed.

| Literal increment | Adaptive literal bits | Change from one, byte-equivalent |
| --- | ---: | ---: |
| 1 (control) | 79,446,724.059640 | 0 |
| 2 | 79,485,725.810702 | +4,875.219 |
| 4 | 79,999,768.104840 | +69,130.506 |
| 8 | 80,945,313.902510 | +187,323.730 |

Increment one exactly reproduces BM-0113. None of the larger steps improves
this aggregate score, so a uniformly increased literal increment is not the
next implementation candidate. This single-input negative result does not
establish that other update schedules or other inputs cannot benefit.

The result also cautions against treating the empirical/adaptive gap as pure
initialization loss. Increasing the step weakens the initial frequency-one
prior relative to observations but also increases rescaling frequency and
shortens memory. Separating those effects would require a different experiment.
No actual alternate-model range encoding was performed, so the table reports
information quantities, not compressed sizes. A next diagnostic should examine
literal-context partitioning and sharing on the same fixed token sequence
rather than changing the public model in response to the histogram gap alone.

### BM-0115: Literal partition screening on fixed mozilla tokens

On 2026-09-25, MSVC Release processed all 51,220,480 mozilla bytes in 782
64-KiB frames. All retained winners round-tripped before the same operations
were scored with increment one under six partitions. Matches do not update
the previous-literal-token history. Initial-context separation is retained
except for shared. Each model resets per frame; diagnostics are outside timers.

| Partition | Contexts | Adaptive literal bits | Empirical literal bits | Adaptive change, byte-equivalent |
| --- | ---: | ---: | ---: | ---: |
| shared | 1 | 79,613,629.162798 | 78,986,838.754268 | +20,863.138 |
| high0 | 2 | 79,614,600.712523 | 78,981,578.782986 | +20,984.582 |
| high1 | 3 | 79,324,758.608026 | 78,253,175.397112 | -15,245.681 |
| high2 | 5 | 79,096,165.151220 | 77,285,332.142252 | -43,819.864 |
| high3 | 9 | 79,099,652.914591 | 76,068,682.572416 | -43,383.893 |
| high4 (control) | 17 | 79,446,724.059640 | 74,479,752.821076 | 0 |

The control exactly reproduces BM-0113/0114 for 10,620,073 literals. Actual
archive size remains 19,636,009 bytes: no alternate-model encoding occurred.
High2 is best on this input, only 435.970 byte-equivalents ahead of high3.
Full sharing is worse. Coarser partitions reduce this adaptive score despite
worse empirical scores, illustrating that finer historical histograms alone
do not predict online performance with limited per-frame observations.

The best reduction is small relative to the 641,870-byte gap to the reported
gzip result. It is not an actual archive saving and does not establish a
corpus-wide winner. Next compare the same six fixed-token controls across
the complete corpus before selecting a model for actual range coding. This
screen leaves public models, APIs and serialized representations unchanged.

### BM-0116: Complete-corpus literal partition screening

On 2026-09-25 the unchanged BM-0115 MSVC Release executable processed all
twelve Silesia members: 211,938,580 bytes and 3,239 64-KiB frames. Executable
SHA-256 was `CE6DEC24461EF2DE4C137249101BC8533F66531CCDBB5508569434F353DCF693`.
Arguments were `1024 65536 indexed distance-policies` for every member.
Local ignored reports in `out/literal-partition-corpus` checkpoint each member
with executable/input hashes and arguments. All selected frames round-tripped;
original high4 scores matched retained diagnostics and all actual archive sizes
matched prior member records. Aggregate actual size remains 70,980,791 bytes.

The following applies one fixed partition across the complete corpus, not
per-frame or per-file winner selection. Changes are adaptive bits divided by
eight relative to high4; negative values are better, but are not actual bytes.

| Partition | Adaptive literal bits | Empirical literal bits | Change, byte-equivalent | Improved members |
| --- | ---: | ---: | ---: | ---: |
| shared | 256,863,265.382691 | 253,477,534.967287 | +286,806.907 | 7/12 |
| high0 | 256,870,144.615972 | 253,458,669.979914 | +287,666.811 | 7/12 |
| high1 | 254,852,967.283443 | 250,505,716.606365 | +35,519.645 | 8/12 |
| high2 | 253,133,333.947555 | 246,026,299.083264 | -179,434.522 | 9/12 |
| high3 | 252,584,021.327774 | 241,652,961.873226 | -248,098.600 | 11/12 |
| high4 | 254,568,810.125770 | 237,541,977.922866 | 0 | control |

| Member | high2 change, byte-equivalent | high3 change, byte-equivalent |
| --- | ---: | ---: |
| dickens | -16,779.304 | -18,232.808 |
| mozilla | -43,819.864 | -43,383.893 |
| mr | -24,479.270 | -24,292.173 |
| nci | -28,213.568 | -26,155.266 |
| ooffice | +13,807.215 | +4,227.566 |
| osdb | +26,371.922 | -8,339.468 |
| reymont | -8,479.552 | -6,187.049 |
| samba | -16,375.957 | -27,887.257 |
| sao | -8,612.554 | -6,690.971 |
| webster | -92,934.485 | -71,913.628 |
| x-ray | +25,609.249 | -14,097.351 |
| xml | -5,528.353 | -5,146.302 |

High3 (nine contexts including the initial context) is the next actual-coder
experiment candidate: its aggregate is best and it improves eleven members.
The ooffice regression prevents a claim of universal improvement. High2 wins
on mozilla alone but regresses on three members and loses in aggregate.
Full sharing improves several inputs yet loses overall, notably on x-ray.

No alternate-model archive was produced. Before adoption, specify a private
decoder-visible representation, encode/decode it and compare actual sizes,
throughput and bounded memory. Retain the existing public model unchanged.
This evidence supports a modest model experiment, not a claim that the gzip
gap has been closed. Empirical histograms still use future counts and omit
model transmission; their scores are not achievable savings promises.

### BM-0117: Actual reduced-literal coding on fixed selected tokens

On 2026-09-25, MSVC Release at `db6f7fed` processed all twelve Silesia members
with `1024 65536 indexed distance-policies`: 211,938,580 input bytes and 3,239
frames. Executable SHA-256 was
`D9E58E869231B6E462C6993500E80F287CBC20CC913BE7593F98AE5671F7A48B`.
Ignored local reports in `out/reduced-literal-corpus` retain executable/input
hashes, arguments and per-member results. The old 0/3/4 context-7 selector was
unchanged; its selected tokens were encoded with context 8 without dictionary
search or new-model reselection. Every new frame strictly decoded to the same
token fields and raw bytes. Old sizes reproduced BM-0116 for every member.

These are actual encoded frame totals plus 112 stream-header bytes per member,
not logarithmic cost estimates. No public stream was emitted or admitted.

| Member | Context 7 bytes | Context 8 bytes | Change |
| --- | ---: | ---: | ---: |
| dickens | 4,097,624 | 4,079,396 | -18,228 |
| mozilla | 19,636,009 | 19,592,635 | -43,374 |
| mr | 3,596,220 | 3,571,931 | -24,289 |
| nci | 3,584,906 | 3,558,750 | -26,156 |
| ooffice | 3,155,610 | 3,159,844 | +4,234 |
| osdb | 4,112,656 | 4,104,316 | -8,340 |
| reymont | 1,999,706 | 1,993,514 | -6,192 |
| samba | 5,718,530 | 5,690,657 | -27,873 |
| sao | 5,403,519 | 5,396,832 | -6,687 |
| webster | 12,941,027 | 12,869,094 | -71,933 |
| x-ray | 5,971,124 | 5,957,025 | -14,099 |
| xml | 763,860 | 758,720 | -5,140 |
| **Total** | **70,980,791** | **70,732,714** | **-248,077** |

The 0.3495% reduction improves eleven members but is not universal. The actual
saving differs from BM-0116's 248,098.600 byte-equivalent prediction by only
21.600 bytes across the corpus. Mozilla still exceeds the maintainer's reported
gzip -9v size of 18,994,139 bytes by 598,496 bytes; this experiment does not
close that gap or rerun gzip under controlled conditions.

The same bounded benchmark buffers were reused; no new token/payload buffer
was allocated for this comparison. Encoder and strict decoder timing exclude
dictionary selection and diagnostic scoring. Single-run totals are recorded
as 5.573251 s encode and 10.358276 s decode. They are not an end-to-end speedup
claim, and peak process memory was not measured. Existing per-frame hard-limit
enforcement remains active.

Next evaluate policy reselection under context 8 separately from this fixed-token
control. Retain the private status and unchanged public model until broader
validation and an explicit adoption decision.

### BM-0118: Context-8 policy reselection across the complete corpus

On 2026-09-25, the MSVC Release executable at `e91c423c` processed all twelve
Silesia members with `1024 65536 indexed distance-policies`: 211,938,580 bytes
and 3,239 frames. Executable SHA-256 was
`748732B0447161B34ABFC86314513C30C6DC5C6F014FB3AB90F1E6C28283EBEA`.
Local ignored checkpoints in `out/reduced-reselected-corpus` retain input and
executable hashes and arguments. All context-7 totals and fixed-token context-8
totals reproduced their previous per-member values. Every evaluated candidate
strictly recovered identical token fields and raw bytes. All twelve completed
checkpoints were revalidated without relaunching the benchmark.

Only policies 0,3,4 were eligible, exactly as in the fixed-token control. Each
frame selects the smallest actual context-8 size; ties prefer the first policy
in that order. Totals include the common 112-byte stream-header charge per file.

| Member | Fixed-token bytes | Reselected bytes | Additional bytes saved | Changed-policy frames |
| --- | ---: | ---: | ---: | ---: |
| dickens | 4,079,396 | 4,079,396 | 0 | 0 |
| mozilla | 19,592,635 | 19,592,366 | 269 | 25 |
| mr | 3,571,931 | 3,571,772 | 159 | 9 |
| nci | 3,558,750 | 3,558,718 | 32 | 4 |
| ooffice | 3,159,844 | 3,158,736 | 1,108 | 38 |
| osdb | 4,104,316 | 4,104,316 | 0 | 0 |
| reymont | 1,993,514 | 1,993,514 | 0 | 0 |
| samba | 5,690,657 | 5,690,638 | 19 | 5 |
| sao | 5,396,832 | 5,396,799 | 33 | 1 |
| webster | 12,869,094 | 12,869,080 | 14 | 7 |
| x-ray | 5,957,025 | 5,957,007 | 18 | 1 |
| xml | 758,720 | 758,717 | 3 | 3 |
| **Total** | **70,732,714** | **70,731,059** | **1,655** | **93** |

Selected policy counts were 1,134 / 1,368 / 737 for policies 0 / 3 / 4.
Changed-policy counts include tie changes, not only strict improvements.
The additional reduction is about 0.00234%; most of the earlier gain came from
the literal partition itself, not policy reselection. Ooffice recovers 1,108
bytes but remains 3,126 bytes larger than its context-7 result. Mozilla remains
598,227 bytes above the maintainer's reported gzip -9v size.

This is a size-only experiment, not a retained-output selector implementation
or speed/RSS comparison. It does not justify expanding the policy search solely
for these small gains. If context 8 is adopted, its own candidate sizes can
replace context-7 scoring; a second old-model selection stage is not required.
Keep public behavior unchanged and return to the remaining model/parser cost
breakdown before adding further search combinations.

### BM-0119: Remaining fixed-token context-8 field costs

On 2026-09-25, aggregate the checked BM-0118 reports without rerunning the
encoder. Use `literal_partition_high3_adaptive_bits / 8` for literals and
`retained_cost_*_adaptive_bits / 8` for unchanged kind/length/distance classes;
divide each retained bypass-bit count by eight. These diagnostics describe
BM-0117's **fixed context-7-selected tokens**, not BM-0118's reselected tokens.
They share the recorded input identities and reproduce the fixed byte totals.

| Component | Mozilla byte-equivalent | Complete corpus byte-equivalent |
| --- | ---: | ---: |
| Kind symbols | 1,339,295.924 | 4,844,836.680 |
| Literal symbols (high3) | 9,887,456.614 | 31,573,002.666 |
| Length classes | 1,196,161.521 | 4,411,366.581 |
| Distance classes | 1,824,879.672 | 6,624,433.508 |
| Length bypass | 863,472.500 | 3,165,227.250 |
| Distance bypass | 4,415,033.000 | 19,838,224.875 |
| **Modeled total** | **19,526,299.231** | **70,457,091.560** |
| Explicit framing bytes | 62,672 | 260,464 |
| Actual size minus modeled total and framing | 3,663.769 | 15,158.440 |
| **Actual fixed-token bytes** | **19,592,635** | **70,732,714** |

Framing is 112 bytes per member plus 80 bytes per frame. The residual includes
integer interval rounding and coder termination; it is not an independently
measured field. It is far smaller than the 598,496-byte mozilla gap to the
reported gzip result, so arithmetic termination is not the next size priority.

The high3 literal adaptive-minus-empirical gap is 378,871.293 byte-equivalents
on mozilla and 1,366,382.432 across the corpus. These future-histogram scores
exclude model transmission and do not promise recoverable bytes. Literal
modeling remains important; the prior increment and partition experiments do
not exhaust its design space.

Distance bypass is the second-largest component here, but its magnitude says
nothing about its bias. Its empirical information was not measured in these
reports. Next screen causal binary models on fixed tokens before specifying
any new format. This is a testable hypothesis, not a claim that bypass bits
are compressible or that the gzip gap can be closed.

### BM-0120: Mozilla fixed-token distance extra-bit screening

On 2026-09-25, executable `30798f7a` processed mozilla with
`1024 65536 indexed distance-policies`: 51,220,480 bytes in 782 frames.
Executable SHA-256:
`965523E7E10218388C52D5ACF0754508E5A57F42E859CEEC9EDC0027EA0B13F5`.
Input SHA-256:
`657FC3764B0C75AC9DE9623125705831EBBFBE08FED248DF73BC2DC66E2A963B`.
The ignored checkpoint `out/distance-bit-corpus/mozilla.json` preserves hashes,
arguments and all report fields; a second invocation revalidated and reused it
without rerunning measurement. Wall time was 126.949476 seconds for the whole
multi-experiment benchmark, not a codec throughput measurement.

All 782 frames passed the diagnostic count checks. Distance extras contained
35,320,264 bits: 23,606,409 zeros and 11,713,855 ones. Their count exactly
reproduced the retained distance-bypass control. Old selected size 19,636,009,
fixed-token context-8 size 19,592,635 and reselected size 19,592,366 bytes all
reproduced BM-0118. Diagnostics use the fixed context-7-selected tokens only.

| Distance-bit model | Causal adaptive byte-equivalent | Uniform minus adaptive | Empirical byte-equivalent |
| --- | ---: | ---: | ---: |
| Uniform control | 4,415,033.000 | 0.000 | Not applicable |
| Bit position (16 binary models) | 3,365,044.940 | 1,049,988.060 | 3,357,120.885 |
| Class and bit position (272 binary models) | 3,337,036.578 | 1,077,996.422 | 3,287,786.691 |

Each frame resets frequencies to one; scores predict before updating and use
the specified ceiling-half rescaling. Empirical scores use future histograms
and exclude their transmission cost. Class/position improves causal information
by only 28,008.362 byte-equivalents over position alone while using seventeen
times as many binary models. Keep both candidates for corpus screening rather
than choosing the larger bank from this single member.

Both causal estimates exceed the previous 598,496-byte fixed-token gap to the
reported gzip result, but they are **not actual archive savings**. Integer
range coding, termination, representation and runtime costs are unmeasured;
the existing archives still have the unchanged sizes above. Next screen all
twelve corpus members, including regressions, before designing a new private
representation. No public API, format, default or performance claim changes.

### BM-0121: Complete-corpus distance extra-bit screening

On 2026-09-25, extend BM-0120 to all twelve Silesia members using the same
executable SHA-256 and `1024 65536 indexed distance-policies` arguments.
Reuse mozilla's checked result; measure the other eleven members. Local ignored
`out/distance-bit-corpus` checkpoints preserve each input hash, executable hash,
arguments and report fields. A second invocation revalidated all twelve without
relaunching the benchmark. All old selected, fixed context-8 and reselected
byte totals reproduced the previous controls.

The 211,938,580 input bytes span 3,239 verified frames. Uniform distance extras
total 158,705,799 bits (89,239,378 zeros and 69,466,421 ones), exactly agreeing
with retained bypass counts. Scores describe fixed context-7-selected tokens,
not context-8 reselection. Positive savings below mean fewer estimated bits;
negative savings mean a regression. Values are byte-equivalents, not archives.

| Member | Uniform distance bytes | Position adaptive savings | Class/position adaptive savings |
| --- | ---: | ---: | ---: |
| dickens | 1,737,204.125 | 5,665.908 | -401.442 |
| mozilla | 4,415,033.000 | 1,049,988.060 | 1,077,996.422 |
| mr | 1,202,043.000 | 104,729.647 | 100,656.118 |
| nci | 1,336,278.000 | 18,789.502 | 85,441.890 |
| ooffice | 689,960.750 | 16,897.829 | 17,313.745 |
| osdb | 752,808.375 | 15,367.471 | 11,118.225 |
| reymont | 961,052.750 | 3,247.549 | 795.337 |
| samba | 1,617,384.625 | 35,280.476 | 62,555.828 |
| sao | 986,497.750 | 203,301.845 | 231,958.125 |
| webster | 5,029,660.875 | 11,393.569 | -11,480.821 |
| x-ray | 826,857.000 | 100,633.792 | 113,129.330 |
| xml | 283,444.625 | 957.987 | 1,237.569 |
| **Total** | **19,838,224.875** | **1,566,253.635** | **1,690,320.326** |

Position-only causal information totals 18,271,971.240 byte-equivalents versus
18,147,904.549 for class/position. Empirical future-histogram scores total
18,239,208.897 and 17,956,100.202 respectively; these exclude transmission
costs and are not causal coding results. The larger bank gains another
124,066.691 byte-equivalents overall but loses to position-only on five members
and exceeds uniform coding on two. Position-only beats uniform on all twelve
members at file-total granularity; this does not establish a per-frame guarantee.

Use the 16-model position-only candidate as the first private representation
design target. It has broader observed behavior and one seventeenth the binary
model count. Retain class/position as a measured alternative, not a default or
an input-name-dependent selector. Next specify exact binary range operations,
reset/update rules and decoder bounds before implementing actual coding. Keep
tokens and non-distance fields fixed for the first comparison. Measure actual
bytes, encode/decode cost and workspace before considering public admission;
the current results do not establish actual savings or a gzip win.

### BM-0122: Mozilla actual position-adaptive distance coding

On 2026-09-25, benchmark revision `0f3dd715` processed all 51,220,480
mozilla bytes in 782 frames with `1024 65536 indexed distance-policies`.
Executable SHA-256:
`54520B039E5A6235C5101DCE93E6E26E57F59754628C9CC51D7C0024BCCD008D`.
Input SHA-256:
`657FC3764B0C75AC9DE9623125705831EBBFBE08FED248DF73BC2DC66E2A963B`.
The ignored `out/position-distance-corpus/mozilla.json` records all report
fields, hashes and arguments. This is one completed measurement, not a
multi-run timing study or a resumable per-frame run.

| Fixed-token model | Accounted archive bytes | Encode seconds | Decode seconds |
| --- | ---: | ---: | ---: |
| Context 8, uniform distance extras | 19,592,635 | 1.556813 | 2.965092 |
| Context 9, position-adaptive distance extras | 18,542,748 | 2.554061 | 4.427039 |

Both models use the identical context-7 selector's retained tokens; no context-9
reselection or dictionary reparse occurs. All 782 frames reproduced their raw
input and token sequence. Baseline 20,085,366, old selected 19,636,009 and
context-8 reselected 19,592,366 bytes reproduce the prior controls.

The net context-8 reduction is 1,049,887 bytes (5.359%): improving frames save
1,049,977 bytes while regressing frames add 90. Thus improvement is not universal
per frame. The causal information estimate predicted 1,049,988.060 bytes,
only 101.060 bytes above the actual reduction; integer coding and termination
account for the residual collectively, without a separately measured split.

Against the maintainer-reported gzip -9v result of 18,994,139 bytes, the private
accounted size is 451,391 bytes smaller (2.376%). This is a single-member size
comparison, not a reproduced gzip run, CLI release result or general superiority
claim. The total includes the common 112-byte stream-header allowance and
80 bytes per frame, but no public context-9 stream writer exists yet.

Fixed-token encoding took 1.641 times and decoding 1.493 times the context-8
time in this run. These phase timings exclude dictionary search and policy
selection and include the validating frame paths; they are not end-to-end CLI
throughput. Peak resident memory was not measured. Keep the private candidate,
next measure all twelve corpus members on the same frozen-token controls,
and examine speed and memory before public admission.

### BM-0123: Complete-corpus actual position-distance coding

On 2026-09-25, extend BM-0122 to all twelve Silesia members with the same
executable hash and `1024 65536 indexed distance-policies` arguments. Reuse
mozilla after checking its input/executable hashes and arguments; measure the
other eleven sequentially. Ignored `out/position-distance-corpus` records
preserve each input hash and all report fields. A second local runner invocation
validated and reused all twelve without launching the benchmark. Checkpoints
are per member, not per frame; an interrupted member must restart.

All 211,938,580 bytes in 3,239 frames passed raw and typed-token round trips.
Prior baseline, selected, context-8 and reselected size controls all reproduced.
Tokens remain fixed to the context-7 selector; no context-9 reselection occurs.

| Member | Context 8 bytes | Context 9 bytes | Net saved bytes |
| --- | ---: | ---: | ---: |
| dickens | 4,079,396 | 4,073,776 | 5,620 |
| mozilla | 19,592,635 | 18,542,748 | 1,049,887 |
| mr | 3,571,931 | 3,467,223 | 104,708 |
| nci | 3,558,750 | 3,539,963 | 18,787 |
| ooffice | 3,159,844 | 3,142,951 | 16,893 |
| osdb | 4,104,316 | 4,088,956 | 15,360 |
| reymont | 1,993,514 | 1,990,298 | 3,216 |
| samba | 5,690,657 | 5,655,392 | 35,265 |
| sao | 5,396,832 | 5,193,548 | 203,284 |
| webster | 12,869,094 | 12,857,802 | 11,292 |
| x-ray | 5,957,025 | 5,856,405 | 100,620 |
| xml | 758,720 | 757,766 | 954 |
| **Total** | **70,732,714** | **69,166,828** | **1,565,886** |

Net reduction is 2.214% and all member totals improve. Some individual frames
regress: extra bytes total 118 (mozilla 90, samba 11, xml 17). Gross savings
are 1,566,004 bytes. BM-0121's causal information estimate exceeds actual net
savings by about 367.635 bytes; it predicted the direction accurately but was
not itself an encoded-size measurement.

Summed fixed-token encode phase time increases from 5.809752 to 10.439411
seconds (1.797 times), and decode from 10.912139 to 17.418667 seconds
(1.596 times). These are one sample per member, including reused mozilla,
not repeated trials or end-to-end throughput. They exclude dictionary search
and policy selection. No peak resident-memory measurement was made. Header
accounting and private/non-published stream caveats remain as in BM-0122.

The size result justifies retaining context 9, but does not justify changing
a public default. Next investigate the extra cost of grammar/model processing
and repeated validation in the fixed-token path, preserving exact bytes and
bounds; separate unavoidable adaptive-coding work from removable overhead.
Do not introduce file-name-dependent policies or infer general superiority
from this corpus alone.

### BM-0124: Repeated mozilla measurement after binary specialization

On 2026-09-25, run revision `f35bfa40` three times sequentially with the
BM-0122 arguments and input hash. Executable SHA-256:
`E5531D85D926308CAB5AD654A0CC93268227894AB05D8C4354722D6FCC9A2CCF`.
Ignored `out/position-distance-specialized/mozilla-{1,2,3}.json` records
retain all fields and hashes. Reinvocation validated and reused all three
checkpoints. Each run verified all 782 frames and matched every non-time
report field against BM-0122, including 18,542,748 context-9 bytes. Aggregate
size equality is not a per-frame payload hash comparison; independent fixed
vectors and differential tests supply separate byte/decoder evidence.

| Trial | Context-8 encode s | Context-9 encode s | Context-8 decode s | Context-9 decode s | Decode ratio 9/8 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 1.639979 | 2.708053 | 3.083887 | 4.317964 | 1.400 |
| 2 | 1.628794 | 2.674184 | 3.097136 | 4.347153 | 1.404 |
| 3 | 1.647543 | 2.709036 | 3.109211 | 4.376196 | 1.407 |

Context-9 decode median is 4.347153 seconds, with range 4.317964..4.376196.
BM-0122's old generic-path single sample was 4.427039 seconds: the new median
is about 1.8% lower. However, the unchanged context-8 control rose from
2.965092 to a median 3.097136 seconds. The within-run relative penalty fell
from 1.493 to about 1.404, but normalization is not proof of a causal speedup.
Runs were sequential in fixed order, the older executable was not rerun,
and thermal/scheduling/build effects were not controlled.

The specialization preserves measured sizes and has a favorable indication,
not a conclusive speed claim. It does not eliminate the context-9 overhead.
Next compare generic and specialized distance decoding in the same executable
on identical prebuilt payloads, with alternating order and repeated samples.
Keep encoding, I/O and token selection outside that timing region and keep
correctness checks outside it where possible. Report workload/phase boundaries
explicitly; do not silently compare payload-only times with frame timings.

## BM-0125: Same-binary mozilla distance-decoder comparison

Measured on 2026-09-25 at revision `e80d8f42`, using the MSVC Release
candidate benchmark with arguments `1024 65536 indexed distance-policies 5`.
The full mozilla input contains 51,220,480 bytes in 782 frames.

- Executable SHA-256: `1E4CBB2084B64528CD2A85BC7F9080F50B552BFD09C9A3DC1D89E1B0AC35E6D8`.
- Input SHA-256: `657FC3764B0C75AC9DE9623125705831EBBFBE08FED248DF73BC2DC66E2A963B`.
- Local, ignored checkpoint: `out/position-distance-decode-ab/mozilla.json`.

Each pair decodes identical prebuilt context-9 payloads with the retained generic
and specialized paths in one executable. Ordering alternates by frame and pair;
each pair has 391 generic-first and 391 specialized-first frames. Both paths
passed modeled-operation equality checks for every frame.

| Pair | Generic seconds | Specialized seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 2.171811 | 1.904824 | 12.29% |
| 2 | 2.173442 | 1.903509 | 12.42% |
| 3 | 2.173072 | 1.904032 | 12.38% |
| 4 | 2.162785 | 1.899995 | 12.15% |
| 5 | 2.170440 | 1.896983 | 12.60% |

The generic median is 2.171811 seconds and the specialized median is 1.903509
seconds; the median of the five paired reductions is 12.38%. These are five
pairs within one process, not five independent process trials. The timings
cover single-pass operation decoding, including initialization and finish,
but exclude search, encoding, file I/O and comparison with expected operations.
Warmup runs generic then specialized; cache, scheduling and warmup-order effects
are not eliminated. Do not compare these values directly with two-pass frame
decode timings in BM-0122/BM-0124 or claim equivalent CLI throughput gains.

All prior non-time report controls match BM-0122. Context-9 accounted archive
size remains 18,542,748 bytes against context-8's 19,592,635 bytes, with 782
verified frames. These are private benchmark representations, not a public
context-9 CLI format. The 5,242,880-byte operation output buffer is benchmark
storage, not a codec workspace change or a peak-RSS measurement.

The paired evidence supports the specialization on mozilla, without resolving
encoder overhead or establishing corpus-wide speed gains. Next extend this
same-binary comparison to all twelve corpus members before generalizing.

## BM-0126: Corpus-wide paired distance-decoder comparison

On 2026-09-25, extend BM-0125 to all twelve local Silesia members using the
same executable SHA-256 and arguments. Reuse the verified mozilla checkpoint
from BM-0125; measure the other eleven members sequentially. The executable
was built at `e80d8f42`; measurement/documentation HEAD was `b4aeb98e`.
The full corpus contains 211,938,580 bytes and 3,239 frames.

The ignored runner `out/position-distance-decode-ab/run.ps1` checkpoints each
member atomically after validation, preserving input/executable SHA-256,
arguments and raw fields in per-member JSON. A second invocation validated
and reused all twelve checkpoints without launching another benchmark.
All non-time controls match BM-0123, including accounted archive totals:
70,732,714 bytes for context 8 and 69,166,828 bytes for context 9.

| Member | Frames | Generic median seconds | Specialized median seconds | Median paired time reduction |
| --- | ---: | ---: | ---: | ---: |
| dickens | 156 | 0.468943 | 0.379490 | 19.08% |
| mozilla | 782 | 2.171811 | 1.903509 | 12.38% |
| mr | 153 | 0.371750 | 0.316178 | 15.11% |
| nci | 512 | 0.338610 | 0.281206 | 16.94% |
| ooffice | 94 | 0.334458 | 0.297897 | 10.83% |
| osdb | 154 | 0.402935 | 0.365673 | 9.22% |
| reymont | 102 | 0.209101 | 0.157662 | 24.52% |
| samba | 330 | 0.584307 | 0.505187 | 13.58% |
| sao | 111 | 0.541794 | 0.501613 | 7.72% |
| webster | 633 | 1.373565 | 1.111533 | 19.10% |
| x-ray | 130 | 0.581733 | 0.546373 | 6.59% |
| xml | 82 | 0.074685 | 0.060864 | 17.99% |

For each pair index, summing member times gives:

| Pair | Generic seconds | Specialized seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 7.443404 | 6.421844 | 13.72% |
| 2 | 7.459639 | 6.435358 | 13.73% |
| 3 | 7.461524 | 6.424731 | 13.90% |
| 4 | 7.448054 | 6.420605 | 13.79% |
| 5 | 7.459155 | 6.429827 | 13.80% |

All sixty member/pair comparisons favor specialization, and every pair verifies
all modeled operations against the original sequence. Generic-first counts are
ceil(frame_count/2) for even pair indices and floor(frame_count/2) for odd ones.
These sums combine sequential member runs (including prior mozilla), not five
independent corpus-wide executions. Median paired reductions are calculated
from paired ratios, not ratios of independently selected medians.

BM-0125's timing boundary and caveats still apply: warm, single-pass operation
decode; initialization and finish included; search, encoding, I/O and correctness
comparison excluded. Fixed warmup order and scheduling effects remain.
The extra 5,242,880-byte benchmark buffer is not peak RSS or a codec workspace
change. This supports the private decoder specialization across this corpus,
not a whole-CLI speed claim or a public-format promotion. Next audit the private
binary distance encoder's repeated model/interval work while retaining exact
bytes, safety checks and a generic differential reference.

## BM-0127: Paired mozilla binary distance encoding

Measured on 2026-09-26 at revision `c97f9e19` with the MSVC Release candidate
benchmark and arguments `1024 65536 indexed distance-encode-ab 5`.
The complete mozilla input is 51,220,480 bytes, split into 782 frames.

- Executable SHA-256: `2E208BBBD792DA97D16E1ADA42EB302B5C478D0063C43084A9DDD9E13C821912`.
- Input SHA-256: `657FC3764B0C75AC9DE9623125705831EBBFBE08FED248DF73BC2DC66E2A963B`.
- Ignored local checkpoint: `out/position-distance-encode-ab/mozilla.json`.

| Pair | Generic seconds | Specialized seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 1.638619 | 1.557007 | 4.98% |
| 2 | 1.632186 | 1.555625 | 4.69% |
| 3 | 1.637613 | 1.561231 | 4.66% |
| 4 | 1.638246 | 1.563654 | 4.55% |
| 5 | 1.635125 | 1.551252 | 5.13% |

All pairs verify all 782 frames, with 391 generic-first and 391 specialized-first
frames. Every output byte, descriptor field and operation/decision count agrees
with the already round-trip-verified frame payload. All previous non-time
controls match BM-0122; context-9 accounted archive size remains 18,542,748 bytes
against context-8's 19,592,635. Executable/input hashes were rechecked afterwards.

Generic and specialized time medians are 1.637613 and 1.557007 seconds.
The median paired reduction is 4.69%, not the ratio of those separate medians.
The reusable output buffer is 1,179,733 bytes, allocated outside timing; this is
not peak RSS or an added codec workspace requirement.

The timing boundary is the checked operation encoder's count-only plan plus
payload writing. It excludes dictionary search, context modeling, frame
preflight/serialization and output comparisons. Do not equate this two-pass
result with full three-pass frame timing or whole-CLI speed. Five pairs run in
one process, with generic-then-specialized warmup; scheduling/cache effects
remain. The improvement supports the local specialization on mozilla but does
not establish corpus-wide benefit. Next repeat this pairing across all twelve
members; repeated frame planning remains a separate optimization.

## BM-0128: Corpus-wide paired binary distance encoding

On 2026-09-26, extend BM-0127 to all twelve full Silesia members (211,938,580
bytes, 3,239 frames), using its executable SHA-256 and arguments.
The binary was built at `c97f9e19`; measurement HEAD is `7db0a558`.
Reuse the validated mozilla result and measure the other eleven sequentially.
The ignored `out/position-distance-encode-ab/run.ps1` records per-member
input/executable hashes, arguments and raw fields in JSON after validation.
A second invocation verified reuse of all twelve checkpoints.

| Member | Generic median seconds | Specialized median seconds | Median paired time reduction |
| --- | ---: | ---: | ---: |
| dickens | 0.424156 | 0.400613 | 5.40% |
| mozilla | 1.637613 | 1.557007 | 4.69% |
| mr | 0.330472 | 0.316096 | 4.35% |
| nci | 0.335414 | 0.309613 | 8.07% |
| ooffice | 0.281222 | 0.268052 | 4.19% |
| osdb | 0.344605 | 0.333172 | 3.31% |
| reymont | 0.204587 | 0.193665 | 5.34% |
| samba | 0.518595 | 0.493182 | 4.78% |
| sao | 0.423752 | 0.412760 | 2.44% |
| webster | 1.292560 | 1.220352 | 6.05% |
| x-ray | 0.480494 | 0.463267 | 3.47% |
| xml | 0.072992 | 0.067135 | 8.48% |

Summing member times by pair index gives:

| Pair | Generic seconds | Specialized seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 6.360812 | 6.033947 | 5.14% |
| 2 | 6.333148 | 6.037197 | 4.67% |
| 3 | 6.353352 | 6.030684 | 5.08% |
| 4 | 6.353936 | 6.029029 | 5.11% |
| 5 | 6.346631 | 6.033507 | 4.93% |

All sixty member/pair comparisons favor specialization. Every pair verifies all
3,239 frames, complete payload bytes and metadata. All prior non-time controls
match BM-0123, including 70,732,714 context-8 and 69,166,828 context-9 accounted
archive bytes. Generic-first counts match alternating frame/pair parity,
including odd-frame members. Input/executable hashes and checkpoint reuse pass.

These are warm plan-plus-write operation timings, not full frame or CLI times.
The five aggregate rows combine sequential member runs, including prior mozilla,
not independent corpus-wide trials. Median paired reductions use paired ratios,
not separate time medians. Fixed warmup order, scheduling and cache caveats from
BM-0127 remain. The 1,179,733-byte reusable output buffer is benchmark storage,
not peak RSS. This corpus evidence supports retaining binary specialization;
it does not remove context-9's overall cost or justify a public-format change.
Next design safe reuse of frame-level planning, separately from this optimization.

## BM-0129: Paired mozilla frame encoding with plan reuse

Measured on 2026-09-26 at `572d347d`, MSVC Release, with
`1024 65536 indexed distance-frame-ab 5`. Full input: 51,220,480 bytes,
782 frames of at most 65,536 bytes.

- Executable SHA-256: `68561D666D3555263F7AD7FAF43D6A8E958415F7B8DB09CDC6D0553D1250F22B`.
- Input SHA-256: `657FC3764B0C75AC9DE9623125705831EBBFBE08FED248DF73BC2DC66E2A963B`.
- Ignored local checkpoint: `out/position-distance-frame-ab/mozilla.json`.

| Pair | Three-run seconds | Two-run seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 2.640353 | 1.868338 | 29.24% |
| 2 | 2.655326 | 1.863139 | 29.83% |
| 3 | 2.642049 | 1.852428 | 29.89% |
| 4 | 2.634086 | 1.851088 | 29.73% |
| 5 | 2.630404 | 1.861584 | 29.23% |

Each pair verifies all 782 complete frames and metadata, with 391 three-run-first
frames. Prior non-time controls match BM-0122; context-9 accounted archive size
is still 18,542,748 bytes against context-8's 19,592,635. Input and executable
hashes were rechecked after execution.

Separate time medians are 2.640353 and 1.861584 seconds. The median paired
reduction is 29.73%; it is not computed as the ratio of those separate medians.
Both paths use specialized binary encoding, so this comparison isolates plan
reuse rather than also changing the model-update implementation.

Timing includes context modeling, validation, entropy coding and frame
serialization from retained tokens; it excludes dictionary search, allocation,
I/O, surrounding candidate selection and output comparison. Five pairs are
within one process, with three-run-then-two-run warmup. Cache/scheduling effects
remain. The 1,179,733-byte output buffer is benchmark storage, not peak RSS.
Do not claim the same reduction for whole CLI compression, add it to prior
encoder percentages, or generalize beyond mozilla without corpus measurements.

The result supports keeping the prepared frame path. Next repeat the paired
comparison across all twelve corpus members with identical conditions.

## BM-0130: Corpus-wide paired frame plan reuse

On 2026-09-26, extend BM-0129 to twelve complete Silesia members: 211,938,580
bytes and 3,239 frames. Use its binary SHA-256 and
`1024 65536 indexed distance-frame-ab 5`; binary revision is `572d347d`,
measurement HEAD is `2922303b`. Reuse validated mozilla and measure the other
eleven sequentially. Ignored `out/position-distance-frame-ab/run.ps1` stores
per-member identity hashes, arguments and raw fields after validation.
A second invocation validated all twelve checkpoints without rerunning timing.

| Member | Three-run median seconds | Two-run median seconds | Median paired time reduction |
| --- | ---: | ---: | ---: |
| dickens | 0.689097 | 0.481599 | 30.05% |
| mozilla | 2.640353 | 1.861584 | 29.73% |
| mr | 0.530329 | 0.369861 | 30.30% |
| nci | 0.513142 | 0.362272 | 29.62% |
| ooffice | 0.449668 | 0.319204 | 29.71% |
| osdb | 0.548440 | 0.381570 | 30.38% |
| reymont | 0.315455 | 0.220344 | 30.49% |
| samba | 0.798395 | 0.561483 | 29.61% |
| sao | 0.667377 | 0.466924 | 29.83% |
| webster | 2.074991 | 1.449124 | 30.25% |
| x-ray | 0.807593 | 0.565036 | 29.84% |
| xml | 0.110806 | 0.077964 | 29.35% |

Summing member times by pair index:

| Pair | Three-run seconds | Two-run seconds | Time reduction |
| --- | ---: | ---: | ---: |
| 1 | 10.136386 | 7.111297 | 29.84% |
| 2 | 10.137324 | 7.118461 | 29.78% |
| 3 | 10.153728 | 7.111225 | 29.96% |
| 4 | 10.144984 | 7.105246 | 29.96% |
| 5 | 10.144013 | 7.128577 | 29.73% |

All sixty member/pair comparisons favor two-run encoding, with full-byte and
metadata agreement for every frame in each pair. Prior non-time controls match
BM-0123, including total accounted sizes of 70,732,714 bytes for context 8 and
69,166,828 for context 9. Hashes and parity-correct first-path counts validate.

This isolates plan reuse with binary specialization held constant. Each aggregate
row combines sequential member runs (including earlier mozilla), not an independent
whole-corpus trial. Median paired reductions differ from ratios of separate
medians. BM-0129's warmup/cache/scheduling caveats and timing boundary apply:
complete frame encoding from retained tokens, excluding dictionary search,
allocation, I/O and output comparison. The 1,179,733-byte reusable output buffer
is not peak RSS. This does not establish whole-CLI throughput or remove the
need for malformed-input and write-failure validation before public admission.

Retain the prepared path. Close the remaining write-time failure/publication
test gap from TVG-1130 before expanding private context-9 integration.

## BM-0131: Private context-9 emitted mozilla streams

On 2026-09-26, measure actual saved streams from the private whole-stream
benchmark at revision `cf54bf3eb68f0fea32541d96e0411b5f2fe9f520`. The
Release executable SHA-256 is
`2a5ca95fba0ad550b44139ca6b8b177bc7f825469c011ace401eb3496a2af16d`.
Input is Silesia `mozilla`, 51,220,480 bytes, SHA-256
`657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b`.
Use 65,536-byte raw frames, indexed matching, fixed eligibility 3/4/5 and
three iterations per policy. Each stream has 782 frames. Strict whole-stream
reconstruction and repeated archive digest checks passed. Saved file sizes and
SHA-256 values were independently checked on disk.

| Eligibility | Saved bytes | Plan seconds | Encode seconds (three runs; median) | Decode seconds (three runs; median) |
| --- | ---: | ---: | --- | --- |
| 3 | 18,655,833 | 4.545883400 | 9.851782800, 9.810357000, 9.820307400; 9.820307400 | 8.005607500, 8.016552200, 8.008731200; 8.008731200 |
| 4 | 19,048,383 | 4.459929100 | 9.691433600, 9.694140000, 9.763465200; 9.694140000 | 8.343723500, 8.354892500, 8.498961900; 8.354892500 |
| 5 | 19,351,929 | 4.955417500 | 10.654928000, 10.635883700, 10.608695400; 10.635883700 | 8.787551200, 8.791648600, 8.827818100; 8.791648600 |

Archive SHA-256 values for eligibility 3, 4 and 5 respectively are
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`,
`6b57b146d3e4aa8d09fd1a7228e93bfc780e358528389b53d3a6afefb8939307`
and `4ec3dae0cba9777fd38c254febce3c1db571b3da2905b748d914a843770f46a9`.
The first saved header was also parsed independently: `MARC`, dictionary 2/8,
entropy 3/2, context 1/9, frame size 65,536 and original size 51,220,480.
These are private format identifiers, not public codec admission.

The maintainer previously reported an Ubuntu `gzip -9v` archive of 18,994,139
bytes for this input. Eligibility 3 is 338,306 bytes (1.78%) smaller; eligibility
4 is 54,244 bytes larger and eligibility 5 is 357,790 bytes larger. That gzip
archive was not independently reproduced in this Windows measurement. The
earlier 18,542,748-byte context-9 accounted estimate retained selected tokens;
it is not the same fixed parsing policy or an emitted whole-stream result.
Eligibility 3's saved stream is 113,085 bytes larger than that estimate.

Planning is timed separately. Encode timing includes the writer's full preflight
and repeated raw tokenization; decode timing includes its two passes. File I/O,
allocation, digest calculation and byte comparison are outside these timings.
Caller-supplied encode and decode scratch spans are 6,553,600 and 851,968 bytes;
input and restored buffers are each 51,220,480 bytes, plus the archive buffer.
These are supplied capacities, not measured peak RSS. Local saved archives under
ignored `out/position-distance-stream/` are measurement artifacts, not corpus
or repository contents. This result supports further private integration review,
not a public API or throughput claim for the CLI.

## BM-0132: Private context-9 incremental Mozilla streams

On 2026-09-26, extend BM-0131's benchmark at parent revision `d41baf3c` with
the DD-1282 incremental driver. Final Release executable SHA-256 is
`2b4623c1e26c6957885712233e400f33ee1d7351d8fdf9d61cbeb5d9d099fd99`.
Use the same 51,220,480-byte Mozilla input and input hash as BM-0131, 65,536-byte
frames, indexed search, eligibility 3 and three iterations. Incremental input
and output chunks are both 65,536 bytes. Run incremental first, then the final
one-shot baseline, without concurrent tests or benchmarks.

| Mode | Encode seconds (three runs; median) | Decode seconds (three runs; median) |
| --- | --- | --- |
| One-shot | 10.241325600, 10.204168300, 10.334294200; 10.241325600 | 8.536131900, 8.427436000, 8.425614600; 8.427436000 |
| Incremental | 5.600656700, 5.603228900, 5.551535100; 5.600656700 | 4.314007300, 4.301369100, 4.266093700; 4.301369100 |

Median elapsed reductions are about 45.3% for encoding and 49.0% for decoding.
These sequential samples are not randomized paired trials and do not establish
general corpus performance. A preliminary one-shot run before a reporting-only
rebuild gave encode median 10.376325400 and decode median 8.584161000 seconds;
the table uses the final common executable only.

Both saved archives are exactly 18,655,833 bytes in 782 frames, SHA-256
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`.
Every incremental iteration compares all wire bytes with the untimed one-shot
oracle, reconstructs the raw input, checks repeated digests and requires exactly
782 frame preparations. File hashes were checked independently after writing.
Incremental operation preserves compression ratio while removing repeated
whole-stream work; it does not introduce a different parsing policy or format.

Plan setup takes 4.625622400 seconds for incremental and 4.585728000 for one-shot.
Incremental timing includes transform construction and process calls; its
one-shot oracle/setup is excluded. One-shot encoding includes its internal
preflight and decoding includes whole-stream two-pass validation. The incremental
decoder validates before publishing each frame, not atomically for the whole
stream. File I/O, allocation, digests and comparisons are excluded from both.

Incremental aggregate policy charges are 7,804,677 bytes for encode and 2,037,541
for decode, including 5,304 bytes of model/replay state each and owner sizes
504/536 bytes respectively. The harness additionally retains raw/restored
buffers of 51,220,480 bytes each, archive/oracle buffers of 18,655,833 each and
one-shot scratch. Reported oracle encode/decode scratch spans (6,553,600/851,968)
share token storage; do not sum them as disjoint allocations. Aggregate charges
and supplied capacities are not measured peak RSS or allocator overhead.

Reproduce with the private benchmark's existing arguments and append
`incremental 65536 65536`; omit those arguments for the one-shot baseline. Use
new output filenames. Archives remain in ignored `out/position-distance-stream/`.
Public API, CLI admission and defaults remain unchanged. Broader Silesia coverage
and public integration review remain separate follow-up work.

## BM-0133: Remaining Silesia members with incremental context-9

On 2026-09-26, run the eleven non-Mozilla members at parent revision `05d5166b`
with `tools/run_silesia_position_distance_streams.py` and
`benchmarks/experiments/silesia-position-distance-streams-v1.json`. Use BM-0132's
same Release executable (SHA-256
`2b4623c1e26c6957885712233e400f33ee1d7351d8fdf9d61cbeb5d9d099fd99`), indexed
eligibility 3, 65536-byte frames and incremental input/output chunks, three
iterations per child and no concurrent benchmark/test execution. Verify the
entire local corpus manifest before launch. Alternate first mode per member,
starting with one-shot for dickens. The 22 child records and a no-relaunch resume
verification all succeeded.

Times below are medians in seconds; archive sizes apply to both modes.

| Member | Archive bytes | One-shot encode | Incremental encode | One-shot decode | Incremental decode |
| --- | ---: | ---: | ---: | ---: | ---: |
| dickens | 4,155,283 | 2.1857424 | 1.2040415 | 1.6476796 | 0.8126692 |
| mr | 3,545,896 | 1.9074683 | 1.0452458 | 1.4919187 | 0.7716052 |
| nci | 3,687,394 | 3.2686458 | 1.7282724 | 1.3671348 | 0.6905292 |
| ooffice | 3,170,470 | 1.1057639 | 0.6324001 | 1.3493603 | 0.6624796 |
| osdb | 4,204,247 | 1.0256639 | 0.6107630 | 1.7436375 | 0.8503613 |
| reymont | 2,015,408 | 1.6372990 | 0.9091498 | 0.7755745 | 0.4291165 |
| samba | 5,756,911 | 2.4890827 | 1.3669854 | 2.3780715 | 1.1766209 |
| sao | 5,219,872 | 1.3075775 | 0.7780659 | 2.3631438 | 1.1890161 |
| webster | 13,190,513 | 6.4413052 | 3.5373056 | 5.0176731 | 2.4925460 |
| xml | 776,876 | 0.3869566 | 0.2095833 | 0.2865777 | 0.1409979 |
| x-ray | 5,747,816 | 1.3447236 | 0.8168626 | 2.4796074 | 1.2397255 |

All eleven improve in both directions: encode elapsed reductions range from
39.3% to 47.1%, decode from 44.7% to 51.2%. Summed member medians fall from
23.1002289 to 12.8386754 seconds for encode (44.4%) and from 20.9003789 to
10.4556674 seconds for decode (50.0%). These sums are not separately measured
corpus trials. The archive total is 51,470,686 bytes; adding the separately
measured Mozilla archive gives 70,126,519 bytes across all twelve members.

Each child verifies all iterations and raw reconstruction; incremental children
compare exact one-shot bytes and one preparation per frame. Both modes' saved
archive sizes/SHA-256 agree for every member. Resume rehashes every saved archive
and checks executable, tool and input identities before accepting the records.
The complete raw reports remain in ignored
`out/position-distance-stream/silesia-incremental-v1/checkpoint.json`, SHA-256
`148b61dfa8bfdcf918ffc52c0c83e6e24a0e4574bebc14d24eb569c0fc376e15`.

Reproduce from the repository root (substitute the local Python/executable path):

```text
python tools/run_silesia_position_distance_streams.py --benchmark out/build/windows-msvc/Release/marc_lzss_position_distance_stream_benchmark.exe --corpus benchmarks/data/silesia/corpus --manifest benchmarks/experiments/silesia-position-distance-streams-v1.json --output out/position-distance-stream/silesia-incremental-v1
```

Use a new output directory for independent timing samples; the same directory
resumes validated records. An abrupt termination may leave `running.lock`; inspect
running processes before removing that lock. Uncheckpointed archives are retained,
not accepted or overwritten. No network download is performed.

BM-0132's timing boundaries, whole-stream versus per-frame publication distinction
and workspace-versus-RSS caveats still apply. Alternating order limits a simple
order bias but is not a randomized repeated trial. This establishes no measured
regression for this fixed corpus/configuration and supports proceeding to public
integration design; it does not admit a public format or change defaults.

## BM-0134: Public CLI Mozilla confirmation after position-distance admission

On 2026-09-27 measure the Windows/MSVC x64 Release CLI at revision
`0e79e6156789efdb88cc755a03eb67b6239380b9`. Official CMake 4.3.4 rebuilt
the CLI target (the sandbox FileTracker access failure was resolved by running
the identical build outside the sandbox). The measurement ran inside the
sandbox with no concurrent repository test or benchmark. Executable SHA-256:
`3033d205fb3680ffdbef35cfd88c8aebb08544e77e002e9a7f6b1a184f54a758`;
shared library SHA-256:
`2f397672a8228bef7ac405748b2833166bb13a96f440089490394ff721eb95cd`.

Input is the unchanged Silesia Mozilla file, 51,220,480 bytes, SHA-256
`657fc3764b0c75ac9de9623125705831ebbfbe08fed248df73bc2dc66e2a963b`.
Use the default settings of `lzss-contextual-dynamic-range` (old) and
`lzss-position-distance-dynamic-range` (new), each with a 64-KiB window.
Perform three encode/decode pairs per codec, alternating first codec by iteration
(old/new, new/old, old/new), with new output filenames and a 600-second child
limit. No separate warmup or cold-cache control was used.

| CLI profile | Archive bytes | Encode seconds, median | Decode seconds, median | Observed encode peak working set, MiB | Observed decode peak working set, MiB |
|---|---:|---:|---:|---:|---:|
| `lzss-contextual-dynamic-range` | 20,085,366 | 4.211524 | 3.524065 | 8.168 | 5.406 |
| `lzss-position-distance-dynamic-range` | 18,655,833 | 5.467712 | 4.714370 | 10.871 | 5.414 |

Old encode samples: 4.2390230, 4.2115239, 4.2065390 seconds; decode:
3.4016148, 3.7768085, 3.5240649. New encode samples: 5.5039435, 5.4677116,
5.4651399; decode: 4.7494976, 4.5373855, 4.7143698.
The table's memory values are the maximum observed per-process
`GetProcessMemoryInfo.PeakWorkingSetSize` across the three samples, queried
approximately every 10 ms. They include executable/DLL pages, caller buffers,
codec storage and I/O state; they are not workspace-query charges, portable RSS,
peak private allocations or an allocator trace. Very late unobserved growth
cannot be excluded. Exact observed maxima in bytes are old encode 8,564,736,
old decode 5,668,864, new encode 11,399,168 and new decode 5,677,056.

Wall time includes process creation, DLL loading, allocation, file I/O, codec
processing and exit observation (including polling delay), unlike BM-0132's
internal timings. Harness hashing/verification occurs outside each measurement.
Do not directly interpret differences from BM-0132 as a codec slowdown.
Within this common CLI experiment, new encode/decode medians are 29.83%/33.78%
longer than the old profile, in exchange for 1,429,533 fewer bytes (7.12%).

All six pairs reconstruct the input digest. Each codec's archive digest is
identical across its three runs. The old archive SHA-256 is
`95eec4f4450a991c75af5dc805c3bde02cafb20d4f21197a55145f2338cd8317`;
the new archive SHA-256 is
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`,
identical to BM-0131/BM-0132's actual emitted streams.

The maintainer's earlier Ubuntu `gzip -9v` result is 18,994,139 bytes. The public
new CLI output is 338,306 bytes (1.78%) smaller, satisfying the Mozilla size goal
relative to that reported file. gzip was not rerun here; gzip timing, memory,
version, header options and cross-corpus superiority are not established.
The marc window is 64 KiB, not gzip's 32 KiB. This is not an equal-window result.

The local JSON manifest, measurement script, six archives/restored files and
atomic pair-level checkpoint are retained under ignored
`out/public-cli-mozilla-20260927/`. Resume validates source, executable, DLL,
runner, revision and saved outputs; a second invocation verified all six records
without launching codecs. Partial uncheckpointed outputs require inspection,
not silent overwrite. Checkpoint SHA-256:
`b33de9c81c3cd65d3e747654a64f65f5de8af0f0337152ce55a4534e989452af`.
The corpus and generated binaries are not committed.

To reproduce archive sizes, run the following for each selector with fresh paths:

```text
marc encode --codec <selector> benchmarks/data/silesia/corpus/mozilla <archive>
marc decode --codec <selector> <archive> <restored>
```

Verify restored SHA-256 and repeat three times. For timing comparisons use the
same process/I/O boundaries and report the OS/memory measurement method.
The next useful optimization investigation is a bounded profile of the new
encode/decode hot paths, especially model updates and symbol lookup; this result
does not itself identify the bottleneck. Preserve the new archive bytes and
measure before changing search, models, memory limits or default selection.

## BM-0135: Position-distance Mozilla stage isolation

On 2026-09-27, at revision `2d27905d` use a local, ignored C++20 Release
diagnostic linked against the existing MSVC static library. No production
instrumentation, source change or new public API is introduced. Reuse BM-0134's
exact Mozilla input and its first position-distance CLI archive. Three sequential
iterations each traverse all 782 frames. Every generated frame must equal the
corresponding CLI frame bytes, every reconstruction must equal the original raw
frame, and the final archive offset must equal the complete archive size.
All checks passed in all three iterations.

| Stage, summed over 782 frames | Run 1 seconds | Run 2 seconds | Run 3 seconds | Median seconds |
|---|---:|---:|---:|---:|
| Indexed LZSS tokenization | 3.5922574 | 3.7136397 | 3.6048818 | 3.6048818 |
| Frame encoding from retained tokens | 1.9102961 | 1.9719084 | 1.9201463 | 1.9201463 |
| Decoder frame-byte preflight | 0.0002466 | 0.0002584 | 0.0002475 | 0.0002475 |
| Range decode plus token grammar/validation | 4.0492605 | 4.1590874 | 4.0638221 | 4.0638221 |
| Typed-token reconstruction | 0.1443027 | 0.1481362 | 0.1447112 | 0.1447112 |

The encoder calls `tokenize_lzss_short_length_escape_candidate_indexed` with
eligibility 3 and then `encode_lzss_position_distance_frame`, retaining tokens
and charging finder storage as the real raw-frame wrapper does. The latter
measurement includes field mapping, planning, model reset, prepared entropy
encoding and frame serialization; it is not pure arithmetic-coder time.
Decoder stages use `preflight_lzss_position_distance_frame_bytes`,
`decode_lzss_position_distance_range_tokens` and `reconstruct_lzss_typed_frame`.
The real incremental decoder uses the same complete-frame path. Token decoding
includes context selection, model reset/update/rescaling, symbol lookup, range
arithmetic, canonical replay and grammar/reference validation. Reconstruction
also includes its own token validation, not only copying.

These are warm, in-memory stage measurements, not a profiler attribution of
the CLI wall time. File I/O, process startup, allocation, archive/input loading,
outer incremental buffering and byte comparisons are outside the stage timers.
The diagnostic interleaves encoding and decoding per frame, changing cache
conditions relative to BM-0134. A small amount of intervening error checking is
included in adjacent timers. Do not subtract these times from CLI times to
estimate unmeasured overhead, or treat the new-only stage split as proof of
which stage caused the old/new difference.

Within this diagnostic, tokenization is approximately 65% of measured encoder
time and frame encoding 35%. Range/token decoding is approximately 96.6% of
measured decoder time; reconstruction is about 3.4%, and preflight is negligible.
Prioritize deeper decoder profiling before copy optimization. Encoding search
remains a separate substantial target; the retained-token path already performs
one search per frame, so reintroducing a duplicate-search explanation would be
incorrect.

Code inspection finds linear cumulative summation in range encoding and linear
symbol lookup in generic range decoding, and the generic decoder recomputes the
interval unit in `decode_interval` after calculating it for symbol selection.
These are candidates, not proven costs: compiler optimization may eliminate
redundant arithmetic. The distance-bit path already has a binary specialization.
Next measure per-context decision/lookup work or obtain symbol-level samples,
then evaluate a bounded lookup change and/or explicit unit reuse against the
unchanged reference bytes. Do not remove validation or change probability updates
merely to improve the benchmark. Any candidate must retain malformed-input,
memory-limit and deterministic-byte behavior and be compared under one harness.

Local diagnostic files are retained under ignored
`out/position-distance-stages-20260927/` (CMake project, probe, executable and
`results.txt`). No corpus or generated archive enters the repository.
Probe source SHA-256:
`9bebd966342a1829a6288cac716843a6fb04024652eea746fbdb7f532cb9c779`;
executable SHA-256:
`c3b75587ab4ca79c4b1ba9e372a8397e8fe87396659dab39557770bf3db7475b`;
results SHA-256:
`b62c8a702019667c4e4a99c83c91bf5c11de6b13545d9c0adb60b2addbf04f97`.
Run the probe with the Mozilla input path and the BM-0134 saved CLI archive path;
it refuses missing/empty/oversized inputs and reports failure on any disagreement.
This single-file diagnostic does not establish corpus-wide performance.

## BM-0136: Position-distance event work and repeated token decoding

On 2026-09-27 extend BM-0135's ignored diagnostic at revision `b9efc799`.
After each ordinary frame round trip, independently run the production range
decoder's begin/decode_next/finish sequence once into bounded operation storage.
Outside its timer, compare every operation field with the encoder's retained
operations and classify fields using the independent cursor. Count only the
first iteration; use three iterations for timings. All 782 frames passed byte
equality, raw reconstruction, operation equality and final coder validation on
every iteration. No codec source was changed.

| Scope | Run 1 seconds | Run 2 seconds | Run 3 seconds |
|---|---:|---:|---:|
| Existing range-to-token decoder | 3.9553164 | 3.9520367 | 3.9605720 |
| Single direct event replay | 1.7792370 | 1.7813834 | 1.7816050 |

Direct replay includes coder initialization, field grammar, model updates,
canonical checks and operation writes, but not the outer token assembler or
history/output-limit validation. Comparisons and counters are outside its timer.
It runs after ordinary decoding, so cache/order bias exists. The difference is
not a measured speedup for a safe replacement decoder.

| Generic modeled alphabet | Calls per logical pass | Inferred linear lookup visits |
|---|---:|---:|
| 2 (token kind) | 14,011,113 | 19,118,793 |
| 9 (length class) | 5,107,680 | 28,894,085 |
| 17 (distance class) | 5,107,680 | 48,944,506 |
| 256 (literal contexts) | 8,903,433 | 1,035,825,785 |

The inferred visits are decoded symbol value plus one, matching the current
generic linear lookup loop on valid input; these are algorithmic visits, not
hardware load counts or instruction samples. Literal contexts account for about
91.4% of the 1,132,783,169 generic visits. Additionally there are 4,743,553
uniform-length-extra events (7,569,337 bits) and 5,090,600 adaptive-distance-extra
events (43,836,826 binary decisions). The distance path already uses its binary
specialization, so those decisions are not generic linear scans. No attribution
of elapsed time to division versus lookup follows from these counts alone.

Source inspection establishes a larger structural cost: the position-distance
entry point in `src/context/lzss_short_match_range_tokens.cpp` calls
`decode_tokens<LzssPositionDistanceRangeDecoder>`. That wrapper first calls
`run_pass` with no output to validate, then calls it again with token storage.
Both passes initialize and run the same entropy decoder. The successful current
path therefore repeats the logical events and lookup work above. This is not
duplicate LZSS search; it is a token-output transaction guarantee.

The next candidate should be a separately contracted one-pass decode into
discardable private token scratch inside the already frame-atomic stream path.
Preserve the existing transactional helper and its unchanged-on-error contract
as a reference. Check capacities/overlap before writes, perform all counts,
reference bounds and canonical termination checks, reconstruct only after
success, and publish no raw bytes from a bad frame. Preserve prior-frame output,
sticky errors and exact error categories/positions as required by the public
contract. Do not substitute the direct replay diagnostic for those checks.
Only after differential malformed-input and boundary tests should timing decide
adoption. Literal-model lookup remains a subsequent, separate optimization.

The updated local probe source/executable SHA-256 values are
`4470347da742d429cc5113421604807f30bf5700a27745bb437542ae1b11182d`
and `0a7218f0888fe71696274284fd0577b29bcf69d26c55c5973278e929de8d4973`.
`out/position-distance-stages-20260927/event-results.txt` SHA-256 is
`934909a3e6b835a0eb5c44543b55583af47b657206273e6ba3efc1826ae280fb`.
BM-0135's earlier results file is retained; its probe version is identified by
the earlier hash. The approval-review transport failure interrupted the new
build before execution; after the maintainer switched approval mode, build and
measurement succeeded. These observations do not change public defaults,
workspace charges, stream bytes or the reported external interoperability scope.

## BM-0137: One-pass position-distance token scratch comparison

On 2026-09-27, after the complete 3,942-test suite and FZ-0045 passed, compare
the retained transactional complete-frame decoder with the new scratch adapter
in the same binary. Both paths include frame preflight, full token/entropy
validation, canonical finish and raw reconstruction. Before timing, compare
every successful token/result and raw byte across all 782 Mozilla frames.
Use four timed traversals per path, alternating which path runs first. Archive
loading and the initial equality checks are outside the timers. This measures
frame decoding, not just entropy replay and not public CLI wall time.

| Path | Run 1 seconds | Run 2 seconds | Run 3 seconds | Run 4 seconds | Median seconds | MiB/s at median |
|---|---:|---:|---:|---:|---:|---:|
| Transactional reference | 4.2960176 | 4.1740248 | 4.3255428 | 4.1931568 | 4.2445872 | 11.508 |
| Private token scratch | 2.2269422 | 2.1592472 | 2.1766852 | 2.1738275 | 2.1752564 | 22.456 |

Median frame decoding time decreases by 48.75% in this paired experiment.
Alternating order reduces but does not eliminate cache or scheduling effects;
these four traversals are not a general throughput guarantee. Unlike BM-0136's
direct event replay, the scratch path retains the complete token validation.

Separately run three public CLI encode/decode pairs. Every encoded archive
equals the prior oracle byte-for-byte and every restored file equals the input.
The 51,220,480-byte input still produces 18,655,833 archive bytes, or 36.4226%
of the raw size. Archive SHA-256 remains
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`.

| Public CLI operation | Median wall seconds | Raw MiB/s at median | Median observed process peak bytes | Observed peak range bytes |
|---|---:|---:|---:|---:|
| Encode | 5.4671266 | 8.935 | 11,313,152 | 11,313,152..11,317,248 |
| Decode | 2.4193192 | 20.191 | 5,582,848 | 5,550,080..5,591,040 |

CLI timings include process startup and file I/O. Observed process high-water
samples are not codec allocation bounds; existing workspace-charge and allocation
tests pass unchanged. Do not derive a paired CLI speedup from BM-0134's earlier
run. Detailed experiment identity, diagnostic source and raw results are retained
in local artifacts; no earlier measurements were overwritten.

Retain the new adapter only in frame-atomic position-distance streaming.
Transactional token/complete-frame helpers remain the reference, and complete
stream helpers and other variants keep their existing paths. Public ABI, format,
encoded bytes, allocations and workspace charges remain unchanged. Error-result
equivalence, failed-raw immutability, previous-frame publication and sticky errors
are covered by TVG-1163 and the full suite. New external interoperability testing
is separate from the previously reported schema-58 exchange.

## BM-0138: Stateless grouped literal search comparison

After all 3,944 tests and FZ-0046 passed on 2026-09-27, compare linear and
eight-frequency grouped literal lookup in one binary. Both paths use the same
specialized distance decoder, initialization, operation output, model updates
and canonical finish. Alternate first path over four complete traversals.
Before timing, compare all operations for all 782 Mozilla frames and reconstruct
every frame through the scratch decoder against the original raw input.

Timers cover entropy operation decoding only. Header/preflight work and raw
reconstruction are outside them. These times are not directly comparable to
BM-0137's complete-frame decoder times, nor a measurement of lookup alone.

| Literal search | Run 1 seconds | Run 2 seconds | Run 3 seconds | Run 4 seconds | Median seconds | Raw-equivalent MiB/s |
|---|---:|---:|---:|---:|---:|---:|
| Linear | 1.6723763 | 1.7417103 | 1.7049795 | 1.6741487 | 1.6895641 | 28.911 |
| Groups of eight | 1.3952754 | 1.3843056 | 1.3936621 | 1.4684767 | 1.3944688 | 35.030 |

The median entropy decoding time decreases by approximately 17.5% in this
experiment. This is a finite single-input measurement, not a general speedup
guarantee; alternating order does not eliminate scheduling/cache effects.

Three separate public CLI encode/decode pairs all reproduce the existing
18,655,833-byte oracle archive from the 51,220,480-byte input (36.4226%) and
restore identical raw bytes. Archive SHA-256 remains
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`.

| Public CLI operation | Median wall seconds | Raw MiB/s at median | Median observed process peak bytes | Observed peak range bytes |
|---|---:|---:|---:|---:|
| Encode | 5.4088766 | 9.031 | 11,321,344 | 11,317,248..11,321,344 |
| Decode | 2.1181471 | 23.062 | 5,554,176 | 5,545,984..5,570,560 |

CLI wall times include startup and file I/O; they are new standalone observations,
not a paired comparison with earlier CLI runs. Observed process peaks are not
codec allocation bounds. Decoder state size, tables, allocations and workspace
charges remain unchanged and their existing limit tests pass. Detailed identity,
source and raw measurements are retained in separate local artifacts.

Retain the grouped search for the position-distance decoder. The private linear
reference remains available, including a comparator with identical distance
decoding. TVG-1164 and FZ-0046 cover interval boundaries, model rescaling,
operation/error equivalence and canonical state; previous frame-publication
tests remain in the complete suite. No format, ABI, encoder or other codec
change is made, and no new external-platform interoperability result is claimed.

## BM-0139: Fixed-profile Silesia validation of grouped literal lookup

On 2026-09-27 extend BM-0138 to all twelve provisioned Silesia inputs at
`33029427`. Reuse the same verified diagnostic binary and public CLI, with
fixed eligibility 3 and 65,536-byte frames. No codec source changes are made.
For each input, run three CLI encode/decode pairs and four alternating-order
linear/grouped entropy comparisons. Both lookup paths use identical specialized
distance decoding. Verify every operation and raw frame before timing, repeated
archive hashes/sizes, exact restored input hashes and complete frame extents.

All 3,239 frames across 211,938,580 input bytes passed; all 36 CLI pairs
round-tripped and repeated archives were identical within each input. The sum
of the twelve separate archive sizes is 70,126,519 bytes (33.0881% of raw size).
This is not a solid archive or a concatenated-input experiment.

| Input | Archive bytes | Linear entropy median s | Grouped entropy median s | Reduction | CLI encode median s | CLI decode median s |
|---|---:|---:|---:|---:|---:|---:|
| dickens | 4,155,283 | 0.316797 | 0.307297 | 3.00% | 1.197318 | 0.487980 |
| mozilla | 18,655,833 | 1.678509 | 1.380622 | 17.75% | 5.387600 | 2.113436 |
| mr | 3,545,896 | 0.295354 | 0.275083 | 6.86% | 1.042861 | 0.432442 |
| nci | 3,687,394 | 0.266028 | 0.257826 | 3.08% | 1.746402 | 0.527054 |
| ooffice | 3,170,470 | 0.263667 | 0.225690 | 14.40% | 0.642556 | 0.360382 |
| osdb | 4,204,247 | 0.345015 | 0.292438 | 15.24% | 0.619436 | 0.467193 |
| reymont | 2,015,408 | 0.150226 | 0.144541 | 3.78% | 0.887244 | 0.240153 |
| samba | 5,756,911 | 0.468377 | 0.402280 | 14.11% | 1.353946 | 0.651790 |
| sao | 5,219,872 | 0.486874 | 0.369384 | 24.13% | 0.755138 | 0.539485 |
| webster | 13,190,513 | 1.003098 | 0.969557 | 3.34% | 3.511196 | 1.464460 |
| x-ray | 5,747,816 | 0.504855 | 0.444001 | 12.05% | 0.845039 | 0.630278 |
| xml | 776,876 | 0.057104 | 0.053924 | 5.57% | 0.239135 | 0.124834 |

The sum of per-input entropy medians decreases from 5.83590485 to 5.12264200
seconds, or 12.22%. All twelve medians and all 48 observed traversal pairs
favor grouped lookup. Small differences remain sensitive to scheduling/cache
effects; this finite campaign is not a general no-regression guarantee.
Entropy timers exclude preflight, token assembly and raw reconstruction.

Summed per-input CLI medians are 18.2278711 seconds encode and 8.0394859 seconds
decode, equivalent to 11.089 and 25.141 raw MiB/s respectively. These sums
include individual process startup and I/O, not one whole-corpus timed process.
Observed process-peak samples span 11,202,560..11,325,440 bytes for encode and
5,492,736..5,619,712 for decode; they are not codec allocation bounds.

Historical corpus records identify the same input hashes but reuse selected
token parses. They are not size or byte oracles for the public fixed-eligibility
profile, as also distinguished in BM-0131. A preliminary size assertion stopped
after the first dickens pair (historical selected-token accounting 4,073,776
bytes versus fixed-profile 4,155,283). The restored input matched. Inspection
confirmed the different parsing conditions; the corrected campaign used a new
result set and retained the preliminary artifacts. Its timings are excluded
from the table. Current deterministic bytes are established by the new repeated
archives, not inferred from historical size equality.

This evidence supports retaining grouped lookup beyond Mozilla. No format,
ABI, allocation, workspace or codec implementation changes are introduced by
this validation step. Earlier full-suite/fuzz evidence remains evidence for
the unchanged implementation; it was not rerun as part of this measurement-only
step. External-platform validation remains separate.

## BM-0140: Remove the indexed encoder's internal counting traversal

The 2026-09-27 source audit corrects BM-0135's single-search interpretation.
One outer raw-frame preparation retains tokens through entropy encoding, but
the indexed tokenizer itself counted tokens and then reset/repeated the parse
to write them. An initial Mozilla diagnostic verified 782 frames and 14,011,113
tokens against the retained archive. Four alternating runs gave median planning
time 1.7147106 seconds and full transactional tokenization 3.43977335 seconds.
Planning includes one finder initialization/parse, not merely arithmetic sizing.
This diagnostic alone is not a speedup measurement for a safe replacement.

DD-1297 adds a separate bounded scratch entry. After TVG-1167, FZ-0048 and the
complete 3,947-test suite passed, compare it with the unchanged transactional
entry in one diagnostic binary. Use fixed eligibility 3, 65,536-byte frames,
identical finder storage and four alternating traversals per input. Before
timing, compare every token field/count, encode each frame against a retained
fixed-profile archive, and reconstruct the exact raw input. Timers include each
tokenizer call and its validation/index initialization; they exclude entropy
encoding, raw reconstruction, file I/O, allocation and comparison work.

| Input | Transactional median s | Scratch median s | Reduction |
|---|---:|---:|---:|
| dickens | 0.70958140 | 0.35885595 | 49.43% |
| mozilla | 3.45988915 | 1.73860420 | 49.75% |
| mr | 0.61062615 | 0.30722080 | 49.69% |
| nci | 1.37583050 | 0.67863885 | 50.67% |
| ooffice | 0.29653425 | 0.14599770 | 50.77% |
| osdb | 0.18488765 | 0.09415490 | 49.07% |
| reymont | 0.63873105 | 0.32060150 | 49.81% |
| samba | 0.77538895 | 0.38981110 | 49.73% |
| sao | 0.26314870 | 0.13215080 | 49.78% |
| webster | 2.03123830 | 1.02086330 | 49.74% |
| x-ray | 0.19944260 | 0.10264445 | 48.53% |
| xml | 0.13376960 | 0.06581535 | 50.80% |

All 3,239 frames and 45,136,568 tokens passed. All 48 observed pairs favored
scratch. The sum of per-input medians is 10.6790683 versus 5.3553589 seconds,
a 49.85% reduction; this is not one timed whole-corpus process or a general
no-regression guarantee. Capacity- or memory-constrained fallback performance
is not represented by these full-capacity measurements.

Public CLI checks additionally passed three Mozilla encode/decode pairs and
one pair for each other input. Every archive hash matches its retained
fixed-profile oracle, and every restored hash matches raw input. The separate
archive-size sum remains 70,126,519 bytes for 211,938,580 raw bytes (33.0881%).
Mozilla remains 18,655,833 bytes with SHA-256
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`.
Its CLI medians are 3.6487658 seconds encode and 2.0734543 seconds decode,
including process startup and I/O. Earlier CLI timings are not paired controls
for an end-to-end improvement percentage.

No new peak-memory measurement is claimed. Finder/token storage, allocations
and workspace charges are unchanged and existing allocation/limit tests pass.
Retain the scratch path in the position-distance adapter while preserving the
transactional entry and its exact diagnostic fallback. Local raw data and
execution identities are retained separately. Prior external exchange predates
this encoder change; new hosted/external validation remains a separate gate.

## BM-0141: Encoder stage refresh after single-pass tokenization

At `eb3e1213b1286a84211690a491133db7beb6fd0a`, run a first-party diagnostic on
all twelve provisioned Silesia inputs with fixed eligibility 3 and 65,536-byte
frames. Before timing, compare scratch/transactional token fields and counts,
re-encode retained frame bytes and reconstruct raw input. Each of three timed
traversals measures scratch tokenization and complete retained-token frame
encoding separately. It additionally replays prepared entropy planning and
writing over the emitted operations, requiring the same complete payload bytes.
All 3,239 frames and 45,136,568 tokens passed; every timed traversal retained
exact frame/payload equality. No production implementation changed.

| Measured scope | Mozilla median seconds | Sum of twelve input medians, seconds |
|---|---:|---:|
| Scratch tokenization | 1.7535913 | 5.3929078 |
| Complete retained-token frame encoding | 1.9045272 | 7.3468571 |
| Additional prepared entropy planning replay | 0.8170884 | 3.2207503 |
| Additional prepared entropy writing replay | 0.8140441 | 3.2137421 |

Frame encoding includes token validation/mapping, entropy planning/writing,
size/limit checks and serialization. The entropy rows are separate diagnostic
replays of work already included in that frame call; do not add them to frame
time or subtract them to attribute the remainder. Replay follows frame encoding,
so order/cache effects apply. Comparisons, allocation, file I/O, outer streaming
and process startup are outside the timers. These are warm in-memory medians,
not a paired comparison with earlier measurements or CLI profiler samples.
The sums are per-input medians, not one whole-corpus timed process.

Within the two measured main stages, frame encoding represents 57.67% of the
summed medians. Source inspection confirms that prepared planning runs the
coder without output to establish exact size/limits, then writing resets and
runs it with output. Planning and writing have comparable diagnostic cost.
DD-1298 therefore prioritizes investigating bounded private payload scratch
before further dictionary tuning. Removing planning is not yet validated:
capacity, memory, diagnostics and failure-publication checks must be preserved.

No new peak-memory or compression-ratio result is claimed; retained archives
remain byte-identical. Detailed input/binary identities and measurements are
kept separately. This diagnostic adds no new full-suite, fuzz or external
interoperability evidence and does not establish a one-pass entropy speedup.

## BM-0142: Single-pass private entropy payload scratch comparison

After TVG-1168 differential tests and FZ-0049 bounded fuzz, compare DD-1299's
private frame scratch writer with the retained prepared two-run frame encoder
in the same binary. Use all twelve Silesia inputs, 64 KiB frames and fixed
eligibility 3. Verify tokenization, reconstruction and saved frame bytes before
timing. In three timed traversals, alternate execution order by frame and
repetition; compare both complete outputs with the retained archive after each
pair. All 3,239 frames and 45,136,568 tokens pass, with identical frame bytes in
every traversal. Every input has a lower scratch median.

| Complete retained-token frame encoding | Prepared reference, seconds | Private scratch, seconds |
|---|---:|---:|
| Mozilla median | 1.8816176 | 1.1117105 |
| Sum of twelve input medians | 7.2447237 | 4.1802971 |

The summed medians fall 42.30%. Timers include token validation/mapping, entropy
coding, frame preflight and serialization, but exclude dictionary search,
comparisons, allocation, I/O and process startup. These are warm in-memory
measurements; alternating order reduces but does not eliminate cache effects.
The aggregate is a sum of per-input medians, not one timed corpus invocation.
The conservative shape scan remains; one pass refers to probability-model
and range coding, not to every traversal of operation/token storage.

Three separate Mozilla CLI round trips produce the same 18,655,833-byte archive
and restored input hashes. Median elapsed encode/decode times are 2.8983150 /
2.1023530 seconds, including process startup and file I/O. These are standalone
current measurements, not a paired CLI comparison with earlier reports.
Retained archive sizes and compression ratio are unchanged; no new peak-memory
measurement is claimed. Working-state charges and allocations are unchanged.
Full tests and bounded fuzz support retaining the private streaming path;
external verification of the new implementation remains separate.

One additional CLI round trip for each of the other eleven inputs also matches
retained archive and restored-input hashes (fourteen CLI round trips total).

## BM-0143: Post-entropy-scratch stages and short-prefix rejection counts

At the externally verified DD-1299 implementation, repeat three warm traversals
of all twelve Silesia inputs with 64 KiB frames and fixed eligibility 3. Check
reference/scratch tokens, raw reconstruction and retained archive frame bytes
before timing. Each timed frame measures production scratch tokenization and
private scratch frame encoding, then separately replays entropy scratch over
the emitted operations. Require exact frame and replayed payload equality.
All 3,239 frames and 45,136,568 tokens pass, including every timed comparison.

| Scope | Mozilla median seconds | Sum of twelve input medians, seconds |
|---|---:|---:|
| Indexed scratch tokenization | 1.7838410 | 5.4476582 |
| Complete retained-token scratch frame encoding | 1.1300110 | 4.3155456 |
| Additional entropy scratch replay | 0.8582195 | 3.3945074 |

Tokenization includes finder initialization, search, insertion and token writes.
It accounts for 55.80% of the two main-stage sums, not 55.80% of whole-CLI time.
Entropy replay repeats work included in frame encoding; do not add or subtract
it to attribute other costs. Timers exclude comparisons, allocation, I/O and
process startup. Order/cache effects apply; sums are per-input medians, not a
single timed corpus invocation or a paired comparison with earlier reports.

Before timers, an isolated instrumented replay follows the current short-prefix
finder and compares every selected token with production. Aggregate counts:

| Candidate outcome/work | Count |
|---|---:|
| Candidate visits | 1,975,496,378 |
| Prefix rejection | 19,870,801 |
| Best-length probe rejection after prefix success | 1,826,557,215 |
| Candidates entering length extension | 129,068,362 |
| Extension byte comparisons | 726,096,307 |
| Equal extension bytes | 597,066,577 |
| Strict best-length updates | 33,921,948 |
| Maximum-length exits | 38,632 |

Prefix rejection, best-length rejection and extension partition all visits.
Best-length rejection accounts
for 92.46% of aggregate visits and 74.91% to 95.26% per input. This supports
DD-1300's investigation of earlier best-length rejection, while requiring the
same exact prefix checks for survivors. These are logical operation counts,
not hardware load counts, CPU attribution or a predicted speedup. Replay
instrumentation is outside production code and all timing scopes.

No codec, format, allocation or workspace change, new peak-memory measurement,
full-suite run, fuzz campaign or external exchange is claimed here. Retained
archive sizes remain identical. Detailed identities and raw measurements are
kept separately; this step is diagnosis and design only.

## BM-0144: Short-prefix probe-first comparison-order trial

After TVG-1169, full-suite checks and FZ-0050, compare both orders in the same
binary using one bounded greedy parser, 64 KiB frames and fixed eligibility 3.
Each timed path includes finder initialization, queries, insertion and token
writes; outer tokenizer admission checks are excluded. Alternate execution
order by frame and repetition across three traversals. Compare every token
with production scratch tokenization and every complete frame with retained
archives. All 3,239 frames and 45,136,568 tokens match, including each timed
comparison; raw reconstruction is checked before timing.

| Input | Prefix-first median seconds | Probe-first median seconds |
|---|---:|---:|
| mozilla | 1.7729517 | 1.6091326 |
| dickens | 0.3307663 | 0.3219036 |
| mr | 0.2958121 | 0.2818128 |
| nci | 0.6713012 | 0.6170770 |
| ooffice | 0.1396999 | 0.1332606 |
| osdb | 0.0887977 | 0.0866403 |
| reymont | 0.3045441 | 0.2960589 |
| samba | 0.3727713 | 0.3552304 |
| sao | 0.1218394 | 0.1202904 |
| webster | 0.9602183 | 0.9331892 |
| x-ray | 0.0975958 | 0.0949104 |
| xml | 0.0625965 | 0.0606662 |
| Sum of input medians | 5.2188943 | 4.9101724 |

The summed medians fall 5.92%, with per-input reductions from 1.27% to 9.24%.
These are warm in-memory results with remaining order/cache and measurement
noise, not confidence intervals or a predicted gain on other environments.
Comparisons, allocation, frame/entropy encoding, I/O and startup are outside
the timers. The sum is not one timed corpus invocation or a full-tokenizer/
whole-CLI speedup. BM-0143's 92.46% logical rejection count is not a timing gain.

Fourteen separate CLI round trips (Mozilla three times, every other input once)
preserve retained archive and restored-input hashes. Mozilla's 18,655,833-byte
archive is unchanged; median elapsed encode/decode times are 2.7455343 /
2.0779457 seconds, including startup and I/O. These standalone measurements
are not a paired CLI comparison with earlier runs. No new peak-memory result
is claimed; object/workspace layout and allocations are unchanged. DD-1301
retains the candidate based on the bounded change, differential coverage and
this paired parser comparison; new-revision external verification is separate.

## BM-0145: Post-probe-order chain diagnostic

At base revision `b4dc77d0bd95f14bb5a73c1fbf05b6ba612368ba`, replay the
unchanged production finder over all twelve Silesia inputs, fixed eligibility
3 and 64 KiB frames. All 3,239 frames and 45,136,568 tokens match production;
raw reconstruction and complete frames match retained inputs and archives.
The production library matches the previously validated build. Input,
archive, probe source, binary and library hashes remain stable during the run.

Instrumentation runs outside timing. It inspects prefix membership separately
and classifies the logical probe-first outcome, not machine instruction order.
Candidate visits partition as follows:

| Classification | Visits |
|---|---:|
| Best-length probe rejects | 1,835,376,124 |
| Subsequent prefix rejects | 11,051,892 |
| Extension attempted | 129,068,362 |
| Total | 1,975,496,378 |

At entry to each visit, current best lengths are zero for 32,631,233 visits,
three for 147,113,778, four through seven for 718,784,330, eight through fifteen
for 441,739,513 and at least sixteen for 635,227,524. There are 1,858 token
queries with fewer than four remaining input bytes.

Among all visits, 811,356,638 (41.07%) have a best length of at least three,
more than three available match bytes, an exact three-byte prefix and a
different fourth byte. Per-input shares range from 10.37% to 79.12%; Mozilla
has 307,273,263 such visits (32.95%). These are candidates that cannot improve
the established match, not visits proven removable by a particular new hash
table. A new index introduces collisions, construction and insertion costs.
The diagnostic does not predict a speedup.

Three warm production traversals give summed per-input median seconds of
4.9806436 for scratch tokenization, 4.1959657 for complete retained-token frame
encoding, and 3.2779245 for an additional entropy-only replay. The entropy
replay repeats work already included in frame encoding; do not add these
three values or subtract them for causal attribution. Counters and byte
comparisons are outside timers; each timed output is checked. These are
standalone stage observations, not a paired comparison against BM-0144 or
whole-CLI measurements. No new peak-memory measurement is claimed. DD-1302
specifies the bounded next experiment and memory-admission constraints.

## BM-0146: Isolated dual-prefix versus production probe-first parsing

At base revision `adb61787918a99e09b6cb4c2ef05890fb1ab8968`, compare the
DD-1303 isolated finder with the unchanged production probe-first finder.
Use three warm paired traversals, alternating order by frame and repetition.
Include finder initialization, insertion, search and token writes; exclude
allocation, comparison, frame encoding, I/O and startup. The prototype clears
heads only and has less admission/misuse checking than production. This is a
whole prototype parsing comparison, not attribution solely to the extra index.

| Input | Production median seconds | Prototype median seconds |
|---|---:|---:|
| mozilla | 1.6159514 | 1.2683943 |
| dickens | 0.3230624 | 0.1860815 |
| mr | 0.2840965 | 0.2697332 |
| nci | 0.6048465 | 0.4124284 |
| ooffice | 0.1331876 | 0.0844808 |
| osdb | 0.0850452 | 0.0886862 |
| reymont | 0.2904137 | 0.1850650 |
| samba | 0.3535628 | 0.3019683 |
| sao | 0.1186500 | 0.1038020 |
| webster | 0.9294650 | 0.6257704 |
| x-ray | 0.0941003 | 0.0757260 |
| xml | 0.0609876 | 0.0526207 |
| Sum of input medians | 4.8933690 | 3.6547568 |

The sum falls 25.31%; eleven input medians improve and osdb regresses 4.28%.
Three additional osdb invocations, each with three paired traversals, all
show the prototype slower, confirming the observed direction within this run.
This is not a confidence interval, whole-CLI speedup, or universal adoption
result. Remaining cache/order effects and timing noise still apply.

All 3,239 frames and 45,136,568 tokens match production and retained archives
in each timed comparison; independent reconstruction also passes. Corpus,
archive, source, executable and production-library hashes remain unchanged
during measurement. The experiment preallocates 1 MiB of index elements;
the existing full-frame finder uses 512 KiB. These are element-storage sizes,
not measured peak process memory, and both coexist in this comparison driver.
No production memory admission or default changes. DD-1303 holds integration
pending regression investigation and optional scratch design.

## BM-0147: Compact dual-prefix trial and osdb index costs

Base revision `a1b04ce02c2466c92db4d9284c8ab62b9282f357`. An isolated osdb
experiment times initialization and bulk insertion separately, rotating three
implementations over nine traversals. Per-frame insertion covers all positions
before the final four bytes; a subsequent query verifies equivalent state.
Median summed seconds are:

| Implementation | Initialization | Bulk insertion |
|---|---:|---:|
| Production | 0.0007286 | 0.0151596 |
| Wide dual-prefix | 0.0007282 | 0.0321980 |
| Compact dual-prefix | 0.0003557 | 0.0316173 |

The extra index approximately doubles insertion cost in this isolated setup;
initialization is much smaller. These measurements do not share the greedy
parser's interleaved cache state and must not be subtracted from parser times
to claim exact search costs or a complete causal explanation of the regression.

Then run three warm corpus traversals, rotating production/wide/compact order
by frame and repetition. Times include initialization, queries, insertion and
token writes; allocation, comparisons, frame encoding, I/O and startup are
outside timers. Both prototypes retain DD-1303's reduced admission checks and
head-only initialization; production integration must be remeasured.

| Input | Production seconds | Wide seconds | Compact seconds |
|---|---:|---:|---:|
| mozilla | 1.6189166 | 1.2835374 | 1.3315608 |
| dickens | 0.3247511 | 0.1894133 | 0.1847193 |
| mr | 0.2911037 | 0.2756193 | 0.2874543 |
| nci | 0.6213777 | 0.4239551 | 0.4062838 |
| ooffice | 0.1343860 | 0.0869259 | 0.0811898 |
| osdb | 0.0860131 | 0.0921232 | 0.0832526 |
| reymont | 0.2938678 | 0.1885091 | 0.1782426 |
| samba | 0.3578583 | 0.3195399 | 0.2993929 |
| sao | 0.1197187 | 0.1094952 | 0.1002056 |
| webster | 0.9621039 | 0.6780750 | 0.6294034 |
| x-ray | 0.0945599 | 0.0803575 | 0.0709979 |
| xml | 0.0605627 | 0.0526801 | 0.0500795 |
| Sum of input medians | 4.9652195 | 3.7802310 | 3.7027825 |

Compact reduces the sum 25.43% versus production, with all twelve medians
lower. osdb improves 3.21%; nine additional paired traversals all put compact
below production and wide above production. Compact is slower than wide on
Mozilla and mr. These are measured prototype results with timing/cache noise,
not confidence intervals, whole-CLI gains or a universal speed claim.
All 3,239 frames and 45,136,568 tokens agree; candidate frame bytes match the
retained archives. Measurement inputs, sources, executable and library hashes
remain stable. Compact preallocates 512 KiB of index elements versus wide's
1 MiB; all implementations coexist in this driver. No peak process-memory
measurement is claimed. DD-1304 proposes equal-budget production integration.

## BM-0148: Integrated compact-prefix search with complete initialization

Compare the DD-1305 final source worktree based on
`ab9efd097f40a3c4b4068f009037f5dd18717396` with the prior production finder,
retained in a separate namespace in the same measurement executable. Preserve
the old initialization, 32-bit arrays, insertion and probe-first query. The new
path includes production admission checks and construction of all compact
heads and links. Rotate old/new order by frame and repetition over three warm
traversals. Allocation, comparisons, frame encoding, startup and I/O are outside
timers; initialization, queries, insertion and token writes are included.

| Input | Prior production seconds | Integrated compact seconds |
|---|---:|---:|
| mozilla | 1.6048262 | 1.2933002 |
| dickens | 0.3301732 | 0.1857844 |
| mr | 0.2824958 | 0.2763132 |
| nci | 0.6168382 | 0.3774108 |
| ooffice | 0.1332996 | 0.0812904 |
| osdb | 0.0848569 | 0.0821795 |
| reymont | 0.2926439 | 0.1727845 |
| samba | 0.3548189 | 0.2862211 |
| sao | 0.1182870 | 0.1000297 |
| webster | 0.9625485 | 0.6202319 |
| x-ray | 0.0926548 | 0.0706842 |
| xml | 0.0596121 | 0.0464924 |
| Sum of input medians | 4.9330551 | 3.5927223 |

The sum falls 27.17%, with lower medians on all twelve inputs. osdb falls
3.16%, with nine additional paired traversals all below the prior production
path. All 3,239 frames and 45,136,568 tokens agree, and each timed candidate
sequence produces retained frame bytes. Source/executable/library/corpus hashes
remain stable during measurement. The initial integration had an osdb regression;
the final guarded probe, nearest-prefix chain entry and shared-key insertion
are measured together here, without claiming isolated attribution to each.

These are warm initialized-parser results, not confidence intervals, a
whole-CLI speedup or a prediction for every input. Required index bytes and
alignment remain unchanged; no new peak process-memory measurement is claimed.

Fourteen final-source CLI round trips (Mozilla three times, other inputs once)
preserve retained archive and restored-input hashes. Mozilla's unchanged
18,655,833-byte archive has median elapsed encode/decode times of 2.4555232 /
2.0277341 seconds including startup and I/O. These standalone CLI observations
are not paired against the prior production CLI and do not establish its
percentage speedup.

## BM-0149: Post-compact encoder stage and cumulative-work refresh

At `8d5ee5bf67924ee5da36bbd0130dc5c4cb15dc59`, use the unchanged validated
production library and fixed eligibility 3 with 64 KiB frames. All twelve
inputs, 3,239 frames and 45,136,568 tokens agree with transactional/scratch
tokenization, raw reconstruction and retained archives. Source, probe binary,
library and input/archive hashes remain stable during measurement.

Three warm traversals produce these per-input median seconds:

| Input | Scratch tokenization | Retained-token frame encode | Additional entropy replay |
|---|---:|---:|---:|
| mozilla | 1.2933683 | 1.1355649 | 0.8540877 |
| dickens | 0.1847175 | 0.2725678 | 0.2233172 |
| mr | 0.2746883 | 0.2329222 | 0.1884856 |
| nci | 0.3766689 | 0.2174856 | 0.1694426 |
| ooffice | 0.0827459 | 0.1968241 | 0.1473196 |
| osdb | 0.0824368 | 0.2331873 | 0.1831691 |
| reymont | 0.1772507 | 0.1265847 | 0.1037558 |
| samba | 0.2850052 | 0.3312905 | 0.2585433 |
| sao | 0.1010583 | 0.2834007 | 0.2180216 |
| webster | 0.6112755 | 0.8477184 | 0.6795752 |
| x-ray | 0.0704258 | 0.3566837 | 0.2846408 |
| xml | 0.0471759 | 0.0467369 | 0.0370437 |
| Sum of input medians | 3.5868171 | 4.2809668 | 3.3474022 |

Frame encoding represents 54.41% of the first two summed stages. The entropy
replay repeats work already included in frame encoding; do not sum all three
columns or subtract replay time for exact causal attribution. Allocation,
comparisons and diagnostics are outside timers. Each timed output is checked.
These are stage observations, not a paired comparison with earlier records,
whole-CLI throughput or peak-memory measurements.

An untimed grammar-validated operation scan counts 112,003,867 ordinary symbol
operations, including 23,405,837 literals, and 41,203,691 bypass operations
containing 221,235,238 adaptive distance bits and 31,190,340 uniform length
bits. The existing ordinary-symbol prefix sums read 2,898,522,345 frequency
entries logically; always selecting the shorter prefix/suffix would read
1,699,423,852 (41.37% fewer). Literal-only frequency reads would fall from
2,551,334,098 to 1,533,506,470 (39.89% fewer). Changing only literal queries
would reduce total ordinary-symbol frequency reads by 35.12%.

These counts omit runtime branch/subtraction cost and compiler vectorization;
they are not machine-load counts, profiler attribution or predicted speedups.
For example, dickens literal reads only fall from 31,255,682 to 31,255,612,
while Mozilla falls from 1,026,922,352 to 549,633,908. Range decisions and
model updates remain necessary. DD-1306 selects a bounded, no-extra-state
encoder query experiment; no production implementation changes in this record.

## BM-0150: Complementary literal-cumulative entropy trial

At base revision `62ef4a7d303cd85bf566820d6dfefc6fb67ba91b`, compare the
isolated DD-1307 encoder with current production. Both scratch paths include
the bound scan, initialization, grammar validation, range decisions and model
updates. Alternate execution order by frame and repetition over three warm
traversals. Preparation, allocation and payload/descriptor comparisons are
outside these entropy timers. Every candidate output matches production and
the retained frame payload; all 3,239 frames and 45,136,568 tokens agree.

| Input | Production median seconds | Trial median seconds |
|---|---:|---:|
| mozilla | 0.8035125 | 0.7946559 |
| dickens | 0.2148448 | 0.2186874 |
| mr | 0.1800652 | 0.1821676 |
| nci | 0.1646575 | 0.1724796 |
| ooffice | 0.1420923 | 0.1420233 |
| osdb | 0.1766766 | 0.1766110 |
| reymont | 0.0986665 | 0.1012487 |
| samba | 0.2496585 | 0.2501882 |
| sao | 0.2096122 | 0.2024841 |
| webster | 0.6552398 | 0.6673272 |
| x-ray | 0.2751020 | 0.2725606 |
| xml | 0.0357003 | 0.0366507 |
| Sum of input medians | 3.2058282 | 3.2170843 |

The trial sum is 0.35% higher. Five input medians fall and seven rise, from
a 3.40% reduction on sao to a 4.75% increase on nci. Three additional paired
traversals each for Mozilla, dickens and sao preserve the observed direction
in every sample: Mozilla/sao lower, dickens higher. Small differences remain
subject to timing/cache/code-layout effects; these are not confidence intervals.

Source, executable, library and corpus/archive hashes remain stable during the
main run. The candidate changes neither model state nor allocation charges;
no new peak-memory or whole-CLI measurement is claimed. BM-0149's 35.12%
logical-read reduction does not translate into a corpus-wide speedup here.
DD-1307 therefore retains this as a negative isolated result, with production
unchanged. The separately repeated production frame stage is not a candidate
frame benchmark and is not added to or subtracted from these entropy timings.

## BM-0151: Standalone grammar, model-recording and range-replay diagnostic

At base revision `a793532b05ed13b86b640a5158814a8808addc43`, time four
standalone workloads over three traversals: production scratch entropy encoding,
grammar acceptance, grammar plus cumulative queries/model updates/decision
recording, and range-writer replay of the recorded tuples. Rotate order by
frame and repetition. Preparation, allocation, reference recording and exact
comparisons are outside timers. Replay includes range checks and finalization;
recording includes grammar, model initialization, updates and rescaling, but
does not perform range arithmetic. Grammar returns a checked field count.

| Input | Production seconds | Grammar seconds | Recording seconds | Replay seconds |
|---|---:|---:|---:|---:|
| mozilla | 0.8235094 | 0.1961537 | 0.6392433 | 0.3395604 |
| dickens | 0.2163324 | 0.0400484 | 0.1681693 | 0.0988816 |
| mr | 0.1802593 | 0.0366434 | 0.1409198 | 0.0837827 |
| nci | 0.1651676 | 0.0336194 | 0.1210651 | 0.0872353 |
| ooffice | 0.1436867 | 0.0337209 | 0.1112422 | 0.0605878 |
| osdb | 0.1782492 | 0.0385696 | 0.1405865 | 0.0718867 |
| reymont | 0.1000225 | 0.0187719 | 0.0777530 | 0.0466099 |
| samba | 0.2592509 | 0.0545674 | 0.2022284 | 0.1079626 |
| sao | 0.2131072 | 0.0490544 | 0.1694171 | 0.0769239 |
| webster | 0.6641332 | 0.1269698 | 0.5136337 | 0.3050218 |
| x-ray | 0.2760813 | 0.0584589 | 0.2174625 | 0.1227204 |
| xml | 0.0367726 | 0.0071739 | 0.0276746 | 0.0176233 |
| Sum of input medians | 3.2565723 | 0.6937517 | 2.5293955 | 1.4187964 |

All 3,239 frames and 45,136,568 tokens agree with retained archives; replayed
payloads, production descriptors/counts and repeated decision tuples agree.
All source, executable, library and input hashes checked by the runner remain
stable. An initial diagnostic build lacked optimization after configuration
failure; its interrupted measurements are retained but excluded. The table
uses a fresh build with verified Release optimization settings.

Do not add these columns or subtract grammar from recording to estimate the
production model cost. Materialized decisions introduce memory traffic and
different code/cache behavior. Model updates are not isolated from queries
and recording here. The diagnostic is not an optimization, throughput gain,
peak-memory measurement or whole-CLI result. DD-1308 selects a bounded
validation-consolidation trial based on source invariants, with integrated
timing and equivalence still required before any adoption.

## BM-0152: Redundant-storage-validation entropy trial

At base revision `125fc5082e25e963c7da2dce3b91d8df5364e142`, compare the
isolated DD-1309 scratch encoder with production over three traversals. Both
timers include the bound scan, grammar, models, cumulative queries, range
coding and finalization. Alternate order by frame and repetition; allocations,
preparation and exact comparisons are outside timers. Verify Release
optimization settings and stable source/executable/library/input hashes.

| Input | Production median seconds | Trial median seconds |
|---|---:|---:|
| mozilla | 0.8140378 | 0.7876925 |
| dickens | 0.2161809 | 0.2057817 |
| mr | 0.1815097 | 0.1736499 |
| nci | 0.1637901 | 0.1637406 |
| ooffice | 0.1437347 | 0.1373025 |
| osdb | 0.1777892 | 0.1727945 |
| reymont | 0.1002721 | 0.0960502 |
| samba | 0.2515570 | 0.2447804 |
| sao | 0.2118563 | 0.2060279 |
| webster | 0.6565000 | 0.6300950 |
| x-ray | 0.2764422 | 0.2652478 |
| xml | 0.0356570 | 0.0352445 |
| Sum of input medians | 3.2293270 | 3.1184075 |

The sum falls by 3.43%. All twelve initial medians fall, but nci's 0.03%
difference is too small to treat as an established gain. Three additional
paired traversals each for Mozilla, dickens and nci give respective production/
trial medians of 0.8114353/0.7868339, 0.2152339/0.2069694 and
0.1635066/0.1635484 seconds. Every Mozilla/dickens sample improves; nci has
two slightly lower trial samples and one higher, with a 0.026% higher median.
These are observed samples, not confidence intervals or universal speedups.

All payloads, descriptors and result fields agree across 3,239 frames and
45,136,568 tokens. Charged encoder state and allocation policy are unchanged;
no new peak-memory measurement is claimed. The driver's separate production
tokenization/frame timings do not measure a candidate pipeline and are not
added to or subtracted from these values. Whole-CLI improvement and production
integration are not established by this isolated result.

## BM-0153: Integrated validated-field storage checks

Measure the actual DD-1310 production library against a private copy of the
pre-integration optimized encoder from `5302a9542bae35c1256632a831bea5daddb30f7e`.
Both include their scratch bound scan, grammar, model initialization/updates,
range arithmetic and finalization. Alternate order by frame/repetition over
three traversals; preparation, allocation and exact comparisons are outside
timers. All builds/tests/fuzz finish before these standalone measurements.

| Input | Prior encoder median seconds | Integrated encoder median seconds |
|---|---:|---:|
| mozilla | 0.8064505 | 0.7957243 |
| dickens | 0.2114816 | 0.2061576 |
| mr | 0.1777305 | 0.1739619 |
| nci | 0.1648303 | 0.1594990 |
| ooffice | 0.1414284 | 0.1388340 |
| osdb | 0.1789091 | 0.1772463 |
| reymont | 0.0978451 | 0.0958571 |
| samba | 0.2469863 | 0.2439660 |
| sao | 0.2073867 | 0.2056034 |
| webster | 0.6485722 | 0.6350051 |
| x-ray | 0.2712585 | 0.2673601 |
| xml | 0.0361522 | 0.0348517 |
| Sum of input medians | 3.1890314 | 3.1340665 |

The summed median falls by 1.72%, with lower medians on all twelve measured
inputs. This differs from the isolated trial's 3.43%; use these production
measurements rather than transferring the trial percentage. These finite
samples are not confidence intervals or a universal performance guarantee.
All payloads, descriptors and result fields agree across 3,239 frames and
45,136,568 tokens, with retained archives and raw reconstruction also exact.

Three standalone Mozilla CLI runs have median encode/decode times of
2.4261440/2.0813656 seconds. The archive remains 18,655,833 bytes with SHA-256
`244b2fbd55fb394c92501e6d50e26c59823ae1251a5a430834cee69ca59ddaf2`.
The other eleven inputs each pass an additional CLI encode/decode check, for
fourteen complete CLI round trips. These are current absolute CLI timings,
not a paired whole-CLI improvement claim. Separate retained-token frame timers
are not added to entropy timings. Source, binaries and inputs remain stable;
allocation charges are unchanged and no new peak-memory measurement is claimed.

## BM-0154: Public position-distance 1 MiB corpus comparison

The executable `marc_lzss_position_distance_public_benchmark` accepts a profile
(`position-64k`, `position-1m` or `contextual-1m`) and an input-file argument.
It reports three verified repetitions, archive hash/size and budget fields.

DD-1318 compares all twelve verified Silesia members through the public C API.
Each profile uses its default frame/window, fixed 65,536-byte input/output
chunks, one warmup and three measured repetitions. Runs are serial, with
profile order rotated by member. Every repetition reconstructs the input and
matches the warmup archive byte-for-byte. These are profile comparisons:
window, framing, model and match policy differ; this does not isolate window size.

Times include transform creation/process/destruction and harness output copying;
input file I/O and workspace allocation are outside timing. Throughput is total
raw bytes divided by the sum of per-member median times. Results describe this
measurement snapshot, not a portable speed guarantee.

| Profile | Total archive bytes | Encode MiB/s | Decode MiB/s | Encoder budget bytes | Decoder budget bytes |
|---|---:|---:|---:|---:|---:|
| position-64k | 70,126,519 | 26.11 | 29.58 | 7,804,685 | 2,037,549 |
| position-1m | 63,558,293 | 4.34 | 37.78 | 74,979,189 | 32,511,893 |
| contextual-1m | 64,358,213 | 5.07 | 27.29 | 66,060,373 | 26,214,485 |

Total raw input is 211,938,580 bytes. Relative to position-64k, position-1m
reduces archive bytes by 9.37% but takes 6.01 times
the encode time; decode time changes by -21.69%.
Relative to contextual-1m, archive bytes change by -1.24%,
encode time by +16.62% and decode time by -27.77%.

Budget columns are the minimum accepted aggregate limits for each API, found
by querying fixed configurations; they are policy charges, not allocator/RSS
measurements. The harness retains both directional workspaces, raw input,
archives and restored output. Its observed resident-memory peaks (OS peak
counters sampled every 10 ms) are listed separately and may miss the final
unsampled interval; they must not be called per-direction codec memory.

| Profile | Maximum observed whole-harness resident bytes |
|---|---:|
| position-64k | 154,664,960 |
| position-1m | 251,502,592 |
| contextual-1m | 237,957,120 |

| Member | 1 MiB archive bytes | Bytes vs position-64k | Bytes vs contextual-1m |
|---|---:|---:|---:|
| dickens | 3,472,780 | -16.42% | +0.79% |
| mozilla | 18,241,669 | -2.22% | -4.34% |
| mr | 3,403,468 | -4.02% | -0.81% |
| nci | 2,825,695 | -23.37% | +2.51% |
| ooffice | 3,037,421 | -4.20% | -0.29% |
| osdb | 3,544,894 | -15.68% | +4.42% |
| reymont | 1,691,028 | -16.10% | -0.17% |
| samba | 5,049,664 | -12.29% | +0.16% |
| sao | 5,146,691 | -1.40% | -3.65% |
| webster | 11,109,357 | -15.78% | +1.09% |
| xml | 585,045 | -24.69% | +1.67% |
| x-ray | 5,450,581 | -5.17% | -2.21% |

This establishes a tradeoff rather than a universal replacement. Keep the
explicit selectors and existing defaults. A further encoder optimization
should start with a measured time breakdown; the comparison alone does not
identify the responsible stage. Schema-59 external qualification remains a
separate gate. No corpus input or local environment details are redistributed.

## BM-0155: 1 MiB encoder phase diagnostic

`marc_lzss_position_distance_1m_phase_benchmark <input-file>` implements
DD-1319 for nonempty inputs up to 64 MiB. Twelve manifest-verified Silesia
members were measured serially using the production indexed parser, fixed
eligibility 3 and 1 MiB frames. For each frame, an unsplit raw-frame oracle
is reconstructed exactly; one split-path warmup precedes three repetitions.
Every split frame and replayed entropy payload must equal its oracle.

The following times sum per-member medians of the three iteration totals.
Allocation, file I/O, oracle generation, reconstruction and equality checks
are excluded. The tokenizer/frame clocks cover existing function calls;
nested replay clocks separately cover token plan/model and Range prepare/write.
Replay changes cache history, so these are diagnostic timings rather than a
replacement for the public end-to-end benchmark or a measured speedup.

| Phase | Seconds | Share of tokenizer plus frame |
|---|---:|---:|
| Tokenizer, including finder initialization and updates | 37.2813 | 79.39% |
| Complete frame encoding from selected tokens | 9.6806 | 20.61% |

The independent nested replay totals are 3.9770 seconds for token planning
and modeling, 2.8291 seconds for Range preparation, and 2.8227 seconds for
Range writing. They overlap the complete frame phase and are not additive
to the table. Differences between replay sums and frame time cannot reliably
be attributed to header handling or validation overhead.

| Member | Tokenizer seconds | Frame seconds |
|---|---:|---:|
| dickens | 3.2906 | 0.5373 |
| mozilla | 8.3198 | 2.8133 |
| mr | 3.3619 | 0.5575 |
| nci | 3.1043 | 0.3986 |
| ooffice | 0.5478 | 0.4629 |
| osdb | 0.7369 | 0.5188 |
| reymont | 3.5474 | 0.2539 |
| samba | 2.2589 | 0.7377 |
| sao | 1.0115 | 0.7571 |
| webster | 10.3977 | 1.6676 |
| xml | 0.3115 | 0.0860 |
| x-ray | 0.3931 | 0.8899 |

All oracle reconstruction and repeated frame/payload comparisons passed.
Serialized frame totals plus the stream header also match the earlier public
archive sizes for every member; this size check alone is not an archive hash
comparison. Source and executable identities accompany the retained results.
No new codec memory or compression-ratio improvement is claimed.

Prioritize the tokenizer for further diagnosis, especially candidate traversal,
match comparison, index initialization and advance. This result does not
separate their costs or justify shortening search, changing nearest-match
rules, or modifying failure contracts. Existing formats and defaults remain.

## BM-0156: 1 MiB finder timing and work counts

`marc_lzss_position_distance_1m_finder_benchmark <input-file>` implements
DD-1320 for nonempty inputs up to 64 MiB. All twelve manifest-verified Silesia
members use 1 MiB frames and eligibility 3. Production candidate tokens supply
the exact replay schedule. A warmup without per-call clocks precedes three
clocked passes through the production finder; a separate untimed counter
implementation must agree at every token. Runs are serial; builds and tests
are excluded from the corpus measurement interval.

| Measured production call | Sum of member median seconds |
|---|---:|
| Finder initialization | 0.065829 |
| Candidate search (`find_match`) | 37.591772 |
| Index advance | 1.307687 |

The clocked runs' wall-time median sums are 40.424305 seconds; the unclocked-call
warmup wall times sum to 37.745229 seconds. These executions include match
checks and schedule traversal. Per-call clocks perturb cache/scheduling and
short calls, so neither the difference nor the call sums establish an exact
timer cost or public encoder throughput. Do not combine these timings with
BM-0155 as though they were one experiment.

| Untimed work count across twelve members | Count |
|---|---:|
| Tokens | 32,791,633 |
| Short-chain candidate visits | 122,657,700 |
| Long-chain candidate visits | 10,099,400,746 |
| Long candidates rejected by improvement-byte probe | 9,596,590,435 |
| Remaining candidates rejected by four-byte prefix check | 2,896,281 |
| Match-extension byte comparisons | 3,517,728,772 |
| Equal extension bytes | 3,017,864,447 |
| Short-prefix insertions | 211,938,166 |
| Long-prefix insertions | 211,937,959 |

About 95.02% of long-chain visits fail the improvement-byte probe. That is a
work-count fraction, not a CPU-time fraction. The probe precedes prefix
validation, so rejected candidates are not classified as hash collisions or
exact-prefix entries. Extension comparisons exclude prefix/probe comparisons
and count both equality and the terminating mismatch when present.

| Member | Search seconds | Advance seconds | Long-chain visits |
|---|---:|---:|---:|
| dickens | 3.2581 | 0.0592 | 336,426,180 |
| mozilla | 8.6459 | 0.3917 | 4,752,931,258 |
| mr | 3.3768 | 0.0623 | 1,656,437,442 |
| nci | 3.1113 | 0.1034 | 985,856,850 |
| ooffice | 0.5513 | 0.0500 | 134,613,688 |
| osdb | 0.7336 | 0.0710 | 55,209,239 |
| reymont | 3.5397 | 0.0340 | 424,033,978 |
| samba | 2.2881 | 0.1218 | 537,643,706 |
| sao | 1.0640 | 0.1009 | 66,087,032 |
| webster | 10.3135 | 0.2121 | 1,086,773,904 |
| xml | 0.2986 | 0.0183 | 42,538,234 |
| x-ray | 0.4108 | 0.0829 | 20,849,235 |

Prioritize reducing long-chain candidate traversal while retaining exact
longest-match and nearest-distance rules. Initialization and index maintenance
are secondary targets in this measurement. Before selecting an index/filter
change, distinguish collisions from exact-prefix chains and evaluate its memory
cost. Any prototype still requires token and archive differential checks; this
diagnostic itself changes no production behavior or compression ratio.

## BM-0157: Experimental five-byte prefix chain

DD-1321's benchmark-only finder adds a five-byte prefix index while retaining
nearest exact three/four-byte fallbacks and exhaustive longest/nearest choice.
`marc_lzss_position_distance_1m_five_prefix_benchmark <input-file>` accepts
nonempty inputs up to 64 MiB. It compares complete reset/token replay passes
with the production finder using 1 MiB frames and eligibility 3. One warmup
precedes three repetitions per frame; order alternates by frame and iteration.
Allocation, equality checks, counter classification and frame encoding are
outside timing. Both timed paths write tokens; neither timing is public C API
throughput or the complete candidate-validator/stream-encoder cost.

All twelve manifest-verified Silesia members were run serially. Summing member
median times gives 37.121968 seconds for production finder replay and
22.771125 seconds for the prototype, a 38.66% time reduction. This is a single
measurement snapshot with repeated runs, not a portable performance guarantee.
Every repeated token vector and each final serialized frame match production.

| Member | Baseline seconds | Prototype seconds | Time change |
|---|---:|---:|---:|
| dickens | 3.2840 | 1.3586 | -58.63% |
| mozilla | 8.2540 | 5.5800 | -32.40% |
| mr | 3.2931 | 2.0761 | -36.95% |
| nci | 3.0665 | 2.6596 | -13.27% |
| ooffice | 0.5527 | 0.3926 | -28.96% |
| osdb | 0.7295 | 0.5092 | -30.20% |
| reymont | 3.5369 | 1.6860 | -52.33% |
| samba | 2.2627 | 1.6014 | -29.23% |
| sao | 1.0240 | 0.5895 | -42.43% |
| webster | 10.4185 | 5.6167 | -46.09% |
| xml | 0.3072 | 0.2642 | -13.99% |
| x-ray | 0.3930 | 0.4373 | +11.28% |

Separate classification of all 10,099,400,746 old long-chain visits finds
9,968,915,549 exact four-byte-prefix entries and 130,485,197 collisions:
98.71% exact versus 1.29% colliding. The classification includes entries that
fail the improvement-byte probe. This supports investigating finer exact
prefix partitioning rather than assuming that bucket expansion alone removes
the dominant candidate population; it does not measure collision CPU cost.

At full frame size, the additional index array payload is 4,456,448 bytes
(4.25 MiB), raising finder array payload from 8,912,896 to 13,369,344 bytes.
This excludes vector metadata and allocator overhead and is not an observed
process peak or an admitted public memory budget. The prototype's reset cost
is timed; its allocations occur before timing.

Keep this as an experimental candidate. Eleven members improve, but x-ray
regresses; investigate index-update overhead before production integration.
Any admission must update bounded workspace accounting and validate allocation,
limits, failure atomicity, full archive identity and end-to-end performance.
Production code, format, public defaults and failure contracts are unchanged.


## BM-0158: Shared-key five-prefix advance

DD-1322 retains the original five-prefix advance as a compile-time reference
and changes only key assembly in the new experiment. The executable
`marc_lzss_position_distance_1m_shared_advance_benchmark <input-file>` compares
production, original five-prefix and shared-key five-prefix reset/token replay.
It uses 1 MiB frames, eligibility 3, one warmup and three measured passes,
rotating order by frame and iteration. Allocation, equality checks and frame
encoding are excluded. All twelve verified Silesia members run serially.
Both experimental instances retain allocated arrays during this comparison.
These finder replay times are not public encoder throughput, and comparisons
are within this experiment rather than ratios of separate historical runs.

| Path | Sum of member median seconds |
|---|---:|
| Production finder | 37.600693 |
| Original five-prefix | 23.401251 |
| Shared-key five-prefix | 22.111953 |

The shared-key path changes time by -41.19% relative to
production and -5.51% relative to the original prototype.
Token vectors match after every pass. A fresh shared replay also produces
identical complete serialized frames through the existing frame encoder.

| Member | Production seconds | Original seconds | Shared seconds | Shared vs production |
|---|---:|---:|---:|---:|
| x-ray | 0.3957 | 0.4700 | 0.3883 | -1.87% |
| dickens | 3.2766 | 1.3739 | 1.3508 | -58.77% |
| mozilla | 8.4949 | 5.9616 | 5.4653 | -35.66% |
| mr | 3.3869 | 2.1502 | 2.1110 | -37.67% |
| nci | 3.1838 | 2.7917 | 2.6545 | -16.62% |
| ooffice | 0.5532 | 0.3905 | 0.3615 | -34.66% |
| osdb | 0.7307 | 0.5128 | 0.4538 | -37.89% |
| reymont | 3.5507 | 1.7006 | 1.6705 | -52.95% |
| samba | 2.2822 | 1.6212 | 1.5199 | -33.40% |
| sao | 1.0149 | 0.5840 | 0.5446 | -46.34% |
| webster | 10.4170 | 5.5779 | 5.3463 | -48.68% |
| xml | 0.3142 | 0.2668 | 0.2454 | -21.88% |

Because x-ray is close to production, five additional serial process runs
repeat its three-way comparison. The per-run median time differences for
shared versus production range from -1.99% to +0.30%.
These repeated observations describe this measurement environment; they do
not establish a portable non-regression guarantee or isolate instruction costs.
All repeated token and frame comparisons pass.

Array payload is unchanged from DD-1321: the five-prefix experiment retains
4,456,448 additional bytes (4.25 MiB) per full frame versus production.
No additional arrays or public memory charges are introduced by key sharing.
Production source, defaults, format and failure contracts remain unchanged.
Before admitting this candidate, implement bounded workspace accounting and
verify allocation/limit failures, frame publication rules, archive identity
and end-to-end public performance. This benchmark does not qualify those gates.


## BM-0159: Integrated five-prefix streaming lifecycle comparison

DD-1325 measures the actual private owned encoder with indexed and
indexed_five_prefix selectors, eligibility three, 1 MiB frames and 64 KiB
input/output chunks. Timing includes owner creation, process calls and
destruction. File I/O, sink collection, byte comparison and decoding are
excluded. One warmup pair precedes three measured pairs with alternating
first-path order. Per-member medians are summed; this is internal codec
lifecycle timing rather than CLI wall time or isolated finder replay.

All twelve manifest-verified Silesia members pass complete-stream identity and
exact reconstruction, including every warmup and timed output comparison.
Input totals 211,938,580 bytes and archives total 63,558,293 bytes on both paths.
The unchanged encoded bytes preserve compression ratio and decoder input.
Baseline median sum is 46.833756 seconds versus 31.711242 seconds for five-prefix:
32.29% less encode time, or 1.477 times throughput for the same bytes.

| Member | Indexed seconds | Five-prefix seconds | Time change |
|---|---:|---:|---:|
| x-ray | 1.3058 | 1.2836 | -1.70% |
| dickens | 3.8208 | 1.8395 | -51.85% |
| mozilla | 11.0402 | 8.2439 | -25.33% |
| mr | 3.8767 | 2.6115 | -32.64% |
| nci | 3.4517 | 3.0135 | -12.69% |
| ooffice | 1.0274 | 0.8299 | -19.22% |
| osdb | 1.2603 | 0.9890 | -21.52% |
| reymont | 3.7961 | 1.9327 | -49.09% |
| samba | 3.0046 | 2.2696 | -24.46% |
| sao | 1.7664 | 1.2954 | -26.66% |
| webster | 12.0825 | 7.0626 | -41.55% |
| xml | 0.4015 | 0.3399 | -15.33% |

The internal owned-encoder policy charge is 74,979,253 bytes for indexed and
79,435,701 bytes for five-prefix in this build: an additional 4,456,448 bytes
(4.25 MiB, 5.94%). This is the queried bounded-memory charge, not measured peak
process RSS, and does not substitute for a public C factory query. No decoder
algorithm or stream representation changes. The one reconstruction timing per
member is diagnostic and does not establish a decode-speed improvement.

Five additional serial x-ray runs each repeat the three-pair comparison.
Their per-run median changes range from -2.11% to -0.06%, with all byte and
reconstruction checks passing. Treat x-ray as near parity; these observations
are not a portable non-regression guarantee.

The aggregate improvement supports proceeding to public admission with explicit
memory-query/factory alignment and allocation/failure regression checks. This
benchmark does not switch the public default. Existing external verification
of the old public path does not certify a later factory change. The previous
internal integration passed 4,033 tests per compiler; this benchmark-only
addition runs its new smoke on both compilers and under ASan/UBSan rather than
claiming a new complete regression or hosted CI run.


## BM-0160: Remaining encoder cost after five-prefix admission

DD-1327 selects the admitted five-prefix tokenizer in the first-party phase
diagnostic. The retained indexed raw-frame encoder supplies the byte oracle.
All twelve manifest-verified Silesia members pass complete frame identity and
reconstruction; serialized-frame sizes plus stream headers agree with the
earlier public archives. One warmup precedes three measured repetitions per
frame. The following values sum per-member medians.

| Sequential phase | Seconds | Share of phase total |
|---|---:|---:|
| Five-prefix tokenization | 21.949739 | 69.21% |
| Complete frame encoding | 9.765394 | 30.79% |
| Total | 31.715133 | 100.00% |

Separate replay measurements give 3.997246 seconds for model planning and
materialization, 2.881262 seconds for range preparation, and 2.912589 seconds
for range writing. These overlap the complete-frame work and must not be added
to the sequential total. Independently timed replays have different timing/cache
conditions and are not an exact partition of complete-frame time.

| Member | Tokenization seconds | Complete frame seconds |
|---|---:|---:|
| dickens | 1.2988 | 0.5306 |
| mozilla | 5.4558 | 2.8319 |
| mr | 2.0807 | 0.5462 |
| nci | 2.6116 | 0.4022 |
| ooffice | 0.3611 | 0.4645 |
| osdb | 0.4566 | 0.5265 |
| reymont | 1.6489 | 0.2523 |
| samba | 1.5023 | 0.7425 |
| sao | 0.5324 | 0.7681 |
| webster | 5.3631 | 1.7096 |
| xml | 0.2510 | 0.0865 |
| x-ray | 0.3874 | 0.9043 |

Tokenization remains the largest measured phase. The next diagnostic should
separate search, index advance and initialization for the admitted bounded
five-prefix finder, and count its candidate/probe work before selecting an
optimization. This result alone does not say which of those operations dominates;
earlier dual-prefix counters do not establish the new path's distribution.
Model planning/materialization is a secondary candidate, not evidence that
validation can be removed.

Both compiler configurations pass indexed and five-prefix smoke tests. Public
codec source, defaults, memory requirements and formats are unchanged by this
diagnostic. These timings are neither a new public lifecycle speedup nor a peak
memory result, and no new full regression or external verification is claimed.


## BM-0161: Admitted five-prefix finder timing and work counts

DD-1328 times the actual bounded five-prefix finder while retaining indexed
tokens as the oracle. A separate untimed first-party replay counts logical work.
All twelve manifest-verified Silesia members pass every token decision comparison
in the warmup, three measured replays and counter replay. Per-member timing
medians are summed below.

| Timed operation | Seconds |
|---|---:|
| Initialization | 0.099512 |
| Find | 21.312083 |
| Advance | 1.540283 |
| Enclosing measured replay | 24.402366 |

The warmup replay without per-call clocks totals 21.704168 seconds.
Measured wall time includes timer/validation overhead and does not equal the
sum of measured operation durations. Per-call timing perturbs the loop; these
are diagnostic measurements, not public encode throughput or exact CPU shares.

| Logical operation | Count |
|---|---:|
| Three-prefix visits | 122,657,700 |
| Four-prefix visits | 47,159,827 |
| Five-prefix visits | 6,443,217,975 |
| Exact five-byte prefixes among those visits | 6,332,136,383 |
| Colliding five-byte prefixes among those visits | 111,081,592 |
| Improvement-probe rejections | 6,121,941,203 |
| Prefix rejections after passing the probe | 2,700,230 |
| Extension comparisons | 3,017,814,759 |
| Equal extension comparisons | 2,699,287,905 |
| Improved matches | 20,451,630 |
| Maximum-length exits | 49,688 |

Exact prefixes constitute 98.28% of five-prefix
visits, and probe rejections constitute 95.01%.
These classifications overlap; neither percentage describes CPU time.
Three/four/five-prefix insertion totals are 211,938,166,
211,937,959 and 211,937,752, matching independent frame-length
formulas. The replay covers 32,791,633 tokens.

Search remains the next optimization target. Most long-chain visits involve
exactly matching five-byte prefixes, so bucket collision reduction alone does
not address most visits. Investigate reducing repeated candidates within those
prefix groups, such as a longer-prefix index with nearest shorter-match fallbacks.
Any such prototype must preserve longest/nearest selection and encoded bytes,
charge added arrays, and demonstrate a measured benefit; this diagnosis does
not establish a specific longer prefix or speedup.

Both compiler smoke tests pass. ASan/UBSan passes the smoke plus three short
fallback fixtures and a deterministic 1,048,581-byte binary boundary fixture.
All corpus insertion/classification invariants pass. Production code and public
defaults are unchanged; no new full regression, hosted CI, external verification
or decoder-fuzz result is claimed.


## BM-0162: Six-prefix candidate-index experiment

DD-1329 compares the admitted bounded five-prefix finder, a vector-backed
shared-advance five-prefix control, and the six-prefix prototype. One warmup
precedes three timed token replays per frame, rotating the three-path order.
Timing includes finder reset/initialization and token replay after allocating
benchmark buffers, excluding byte comparisons, frame coding and decoding.

All twelve manifest-verified Silesia members pass token identity against the
retained indexed oracle, complete serialized-frame identity and reconstruction.
Summed per-member medians are 21.959297 seconds for admitted
five-prefix, 21.474676 seconds for the vector five-prefix control,
and 17.764926 seconds for six-prefix. The prototype reduces replay
time by 19.10% versus the admitted finder and
17.28% versus the storage-structure control.
This is not an end-to-end public encode-speed result.

| Member | Admitted five seconds | Vector five seconds | Six seconds | Six vs admitted |
|---|---:|---:|---:|---:|
| x-ray | 0.3831 | 0.3991 | 0.4149 | +8.31% |
| dickens | 1.3146 | 1.3057 | 0.7271 | -44.69% |
| mozilla | 5.5223 | 5.2440 | 4.4499 | -19.42% |
| mr | 2.0513 | 2.0146 | 2.0187 | -1.59% |
| nci | 2.6189 | 2.5193 | 2.4407 | -6.80% |
| ooffice | 0.3568 | 0.3563 | 0.3386 | -5.11% |
| osdb | 0.4486 | 0.4469 | 0.4257 | -5.09% |
| reymont | 1.6518 | 1.6521 | 0.9369 | -43.28% |
| samba | 1.5077 | 1.4858 | 1.2669 | -15.97% |
| sao | 0.5402 | 0.5377 | 0.5141 | -4.85% |
| webster | 5.3187 | 5.2728 | 3.9978 | -24.84% |
| xml | 0.2454 | 0.2402 | 0.2336 | -4.79% |

The additional head/link pair requires 4,456,448 bytes (4.25 MiB) for a full
frame, reaching 17,825,792 array bytes. These are array payload sizes, not
measured process peak RSS or admitted public workspace requirements.

Five additional serial x-ray runs reproduce a +8.55% to +9.05%
replay-time increase versus the admitted finder; all comparisons and
reconstructions still pass. The aggregate improvement therefore does not
qualify an unconditional public switch. Retain the prototype and investigate
the regression before bounded/public admission, separating extra-index
initialization, advance and search costs. The current evidence does not
establish which operation causes the regression.

Both compiler smoke runs and explicitly instrumented ASan/UBSan checks pass,
including exhaustive small-position, wide-distance, nearest-tie and short-tail
fixtures. Public implementation, memory queries and defaults are unchanged.
No new full regression, decoder-fuzz, hosted CI or external verification result
is claimed.

## BM-0163: Six-prefix initialization, search and advance diagnosis

This focused follow-up to BM-0162 compares the same three finders on verified
x-ray input in five independent processes, plus one dickens control. Each
process uses warmup and three measured frame replays with rotating path order.
The table reports the median of the five process medians, in seconds. All
token comparisons pass. Coarse mode omits per-call clocks; detailed mode adds
them. The advance-only batch excludes reset and the final token and does not
perform searches inside timing.

| Measurement on x-ray | Bounded five | Vector five | Six |
| --- | ---: | ---: | ---: |
| Coarse initialization | 0.004050 | 0.003663 | 0.004710 |
| Coarse complete replay | 0.387467 | 0.402293 | 0.456365 |
| Separate advance-only batch | 0.038342 | 0.036996 | 0.049254 |
| Detailed find | 0.396291 | 0.411429 | 0.458015 |
| Detailed advance | 0.094569 | 0.093698 | 0.105137 |
| Detailed complete replay | 0.627241 | 0.649236 | 0.705317 |

Six's separate advance batch takes 25.84% to 33.97% longer than vector five
across all five processes. The extra initialization is small in comparison.
The dickens control has coarse replay 1.300210/1.283007/0.721651 seconds and
advance-only 0.045820/0.039416/0.053548 seconds: reduced search work can still
outweigh additional update work on that input.

However, x-ray full replay is variable: the six-versus-bounded delta ranges
from -20.55% to +38.13%. Three independent runs of the original uninstrumented
benchmark also vary (bounded/vector-five/six, seconds):

| Control run | Bounded five | Vector five | Six |
| --- | ---: | ---: | ---: |
| 1 | 0.385682 | 0.423717 | 0.499841 |
| 2 | 0.490847 | 0.508832 | 0.458899 |
| 3 | 0.377868 | 0.383407 | 0.435282 |

All original-control frame identity and reconstruction checks pass. No outlier
is discarded. These observations do not replace BM-0162's earlier repeated
regression range with a new reliable effect size. They also prevent assigning
a precise fraction of that regression to search or updates. Detailed clocks
perturb execution, and the isolated batch has different cache conditions;
none of these timings establishes particular hardware-level causes.

Both compiler smokes and explicit ASan/UBSan smoke pass. Preserve the six-prefix
prototype without public admission. Next test a benchmark-only reduction of
index-update work and require stable uninstrumented per-member measurements
before deciding on integration. No production change, full-suite/fuzz run,
new external verification or public end-to-end speedup is claimed.

## BM-0164: Omitting the five-byte index from six-prefix search

DD-1331's benchmark-only compact specialization maintains three-, four- and
six-byte indexes. It removes one head/link pair and its per-position stores,
but must scan the four-byte chain for the nearest five-byte fallback. Array
payload falls from 17,825,792 to 13,369,344 bytes per full frame: a 4,456,448-byte
saving, matching the admitted five-prefix array payload. These are structural
array sizes, not measured peak RSS or a new public memory budget.

The comparison uses the admitted bounded five-prefix finder, retained original
six-prefix specialization and compact six. Each frame has warmup and three
measured replays, rotating path order; reported seconds are per-member medians.
Initialization and token replay are timed after buffer allocation, with token,
frame and reconstruction checks outside timing. This is not public end-to-end
compression timing. All twelve manifest-verified members pass those checks.

| Member | Bounded five | Original six | Compact six | Compact vs bounded |
| --- | ---: | ---: | ---: | ---: |
| x-ray | 0.388097 | 0.423986 | 0.553576 | +42.64% |
| dickens | 1.286815 | 0.853324 | 0.822819 | -36.06% |
| mozilla | 5.486645 | 4.723391 | 5.006621 | -8.75% |
| mr | 2.070362 | 2.347958 | 2.138740 | +3.30% |
| nci | 2.559491 | 2.864767 | 2.221261 | -13.21% |
| ooffice | 0.368856 | 0.383622 | 0.427538 | +15.91% |
| osdb | 0.460236 | 0.448422 | 0.542688 | +17.92% |
| reymont | 1.633308 | 1.001499 | 0.936479 | -42.66% |
| samba | 1.514043 | 1.285114 | 1.341309 | -11.41% |
| sao | 0.543947 | 0.613471 | 0.686692 | +26.24% |
| webster | 5.349191 | 5.890153 | 4.126502 | -22.86% |
| xml | 0.247003 | 0.218756 | 0.213771 | -13.45% |

Sums of member medians are 21.9079953, 21.0544639 and 19.0179954 seconds,
respectively. These aggregates do not qualify the experiment: the initial
x-ray result is 42.64% slower than bounded five, and several other members
also regress. Five further independent x-ray processes retain exact tokens,
frames and reconstruction but vary from -19.70% to +50.06% versus bounded five
and -20.62% to +31.36% versus original six. Keep all results; do not claim a
stable regression magnitude, causal attribution or reliable aggregate speedup.

Both compiler smoke sets and explicit ASan/UBSan checks pass, including added
five-byte fallback fixtures. The compact approach is not admitted: the intended
x-ray regression resolution is unproven, despite lower structural update work
and array storage. Preserve it as a diagnostic comparison. Before another
performance decision, investigate run-to-run measurement variability; subsequent
update trials should retain the five-byte index rather than broadening fallback
search. Public codec, memory queries and failure contracts remain unchanged.
No new full-suite/fuzz, hosted CI or external verification is claimed.

## BM-0165: Default versus fixed-processor x-ray replay

Follow BM-0164 with its unchanged executable and verified x-ray input. Run
five independent process pairs, alternating which scheduling condition runs
first: default eligibility or affinity to one permitted logical processor.
Verify child affinity, preserve the embedded warmup and three rotating-order
iterations per frame, and perform no concurrent repository build/test work.
All ten processes pass token/frame identity and reconstruction checks.

The table gives medians of the five process medians, with the full observed
process-median range in parentheses, in seconds. These are initialized finder
replays, not public end-to-end compression measurements.

| Condition | Bounded five | Original six | Compact six |
| --- | ---: | ---: | ---: |
| Default | 0.383683 (0.377920–0.384581) | 0.414570 (0.409493–0.416923) | 0.465977 (0.464284–0.474424) |
| Fixed processor | 0.398614 (0.391014–0.399904) | 0.430745 (0.426765–0.434293) | 0.485768 (0.482265–0.490303) |

Within each process, original six takes 6.96%–8.43% longer than bounded five
in the default condition and 7.06%–9.25% longer in the fixed condition. Compact
six takes 20.90%–23.39% and 21.59%–23.34% longer, respectively. All ten runs
therefore support retaining bounded five for x-ray; omitting the five-byte
index does not resolve the regression. No result is discarded.

The earlier large variation is not reproduced under either condition. This
experiment consequently does not establish that migration caused it, nor that
fixed affinity is faster or necessary for stable results. Fixed affinity does
not isolate a core from other activity. Ancillary whole-process CPU/wall times
include untimed oracle and validation work and do not identify a finder-phase
cause. Preserve BM-0163/BM-0164's variable results rather than replacing them
with a universal effect size.

Keep both six-prefix prototypes unadmitted. The next update-work experiment
should retain the five-byte fallback index and compare against these controls
with repeated, paired measurements. This stage changes documentation only;
there is no new full-corpus, compiler/sanitizer, full-suite/fuzz, hosted CI or
external verification claim, and no production or format change.

## BM-0166: Rolling four-byte key during six-prefix updates

DD-1333 retains all four indexes and the existing search, but carries a
four-byte key between positions within each advance call. Compare bounded
five, original six and rolling six within one executable, with warmup, three
measured iterations per frame and rotating path order. Timings include
initialization and token replay after allocation; oracle, frame comparison
and reconstruction execute outside timing. No per-call clocks are added.

The focused screen verifies the corpus manifest and uses x-ray plus dickens,
not a new full-corpus measurement. Both pass exact token/frame identity and
reconstruction. Initial per-member median seconds are:

| Member | Bounded five | Original six | Rolling six |
| --- | ---: | ---: | ---: |
| x-ray | 0.383403 | 0.416055 | 0.409496 |
| dickens | 1.290411 | 0.724821 | 0.721964 |

Five further independent x-ray runs also pass all identity checks:

| Run | Bounded five | Original six | Rolling six |
| --- | ---: | ---: | ---: |
| 1 | 0.387369 | 0.416704 | 0.415705 |
| 2 | 0.383547 | 0.415047 | 0.415277 |
| 3 | 0.393989 | 0.420056 | 0.417815 |
| 4 | 0.392351 | 0.430208 | 0.423915 |
| 5 | 0.383427 | 0.424402 | 0.419554 |

Rolling six differs from original six by -1.46% to +0.06% and remains
6.05% to 9.42% slower than bounded five. Preserve every run; the small difference
is not a demonstrated universal improvement, and no isolated update-phase or
hardware-cause claim follows from full replay timing. The intended x-ray
regression resolution is not achieved, so do not broaden this experiment into
public admission. Array storage is unchanged from original six.

Both compiler sets pass four affected smokes and explicit ASan/UBSan passes
the rolling specialization. Retain the prototype for comparison, with bounded
five still public. Before further update micro-optimizations, compare untimed
search-work counts for five and six, including short-prefix fallbacks and
long-chain candidates, to identify a better supported optimization target.
No public end-to-end speedup, full-corpus/full-suite/fuzz, hosted CI or new
external verification is claimed.

## BM-0167: Five- versus six-prefix search-work counts

Use an untimed diagnostic on manifest-verified x-ray and dickens. Each token
position must agree among the admitted bounded five finder, original six finder,
both counter replays and retained indexed tokens. All comparisons pass. The
five-counter reports exactly reproduce BM-0161; the new counters do not alter
existing finder code. No throughput or hardware-cause measurement is made.

| Search-work count | x-ray | dickens |
| --- | ---: | ---: |
| Three-byte visits, either finder | 9,006,675 | 1,480,578 |
| Four-byte visits, either finder | 5,053,305 | 1,835,259 |
| Five finder: long-chain visits | 12,150,401 | 123,676,417 |
| Six finder: nearest-five fallback visits | 10,022,640 | 2,128,041 |
| Six finder: long-chain visits | 2,673,292 | 58,509,964 |
| Six fallback plus long visits | 12,695,932 | 60,638,005 |
| Five finder: extension comparisons | 498,440 | 16,164,838 |
| Six finder: extension comparisons | 143,580 | 9,189,067 |

After the shared four-byte stage, six has 4.49% more candidate visits on x-ray
but 50.97% fewer on dickens. These sums count visits at different stages, with
different per-visit work; they are not a CPU-cost accounting identity. The
additional six-byte index receives 8,474,195 and 10,192,396 inserts respectively.

| Six nearest-five fallback classification | x-ray | dickens |
| --- | ---: | ---: |
| Fifth byte differs | 9,632,108 | 993,015 |
| Fifth equal, another prefix byte differs | 40,677 | 43,011 |
| Full five-byte match | 349,855 | 1,092,015 |

Thus 96.10% of x-ray fallback visits, versus 46.66% on dickens, could be
rejected by checking the fifth byte first. Current six checks the prefix in
forward order. Classification alone does not prove that changing the order
is faster: existing checks may already reject early, and added branches or
loads have their own costs. The next supported trial is fifth-byte-first
fallback filtering while retaining all indexes and exact nearest-match rules.

Final token counts are 2,753,162 and 1,295,242. Their literal/length-three/
length-four/length-five/length-six-or-longer bins are respectively
828,715/543,950/1,030,642/222,137/127,718 and
41,260/62,288/99,679/140,143/951,872. This further distinguishes the two inputs
without assigning time to those token categories.

Both compiler smokes, manually checked counts, explicit ASan/UBSan and report
consistency checks pass. Preserve existing admission decisions: bounded five
remains public, six prototypes remain experimental. No new frame reconstruction,
full-corpus/full-suite/fuzz, hosted CI or external verification is claimed.

## BM-0168: Fifth-byte-first nearest-five fallback

DD-1335 changes only the equality order in the nearest-five fallback of the
four-index six-prefix experiment. Compare bounded five, original six and
fifth-first six in one executable, with warmup, rotating finder order and
three measured iterations per frame. Time initialization and replay after
allocation; perform exact token/frame/reconstruction checks outside timing.

Initial focused-screen median seconds are:

| Member | Bounded five | Original six | Fifth-first six |
| --- | ---: | ---: | ---: |
| x-ray | 0.395366 | 0.422360 | 0.429788 |
| dickens | 1.296931 | 0.730689 | 0.735608 |

Five independent x-ray repetitions give:

| Run | Bounded five | Original six | Fifth-first six |
| --- | ---: | ---: | ---: |
| 1 | 0.389599 | 0.417092 | 0.416743 |
| 2 | 0.378513 | 0.414084 | 0.412839 |
| 3 | 0.387748 | 0.417975 | 0.415951 |
| 4 | 0.385544 | 0.412153 | 0.413163 |
| 5 | 0.391528 | 0.417340 | 0.424374 |

Fifth-first differs from original six by -0.48% to +1.69% and remains
6.97% to 9.07% slower than bounded five. All screened and repeated runs pass
exact identity and reconstruction checks, but the comparison-order change
does not establish an improvement or resolve the x-ray regression. The
96.10% logical filterability in BM-0167 therefore does not justify adopting
this predicate order. No instruction-level or hardware explanation is inferred.

Both compiler smoke sets pass six affected tests each, and explicit ASan/UBSan
passes the trial. Preserve it as a benchmark-only comparison; do not expand
it into public admission or claim full-corpus/end-to-end improvement. Array
storage and production behavior remain unchanged.

The compact-index, rolling-key and comparison-order screens have not resolved
the original six-prefix tradeoff. Stop this sequence of local update/comparison
tweaks. Before another implementation trial, assess a workload-dependent choice
of exact five/six search, including selection cost, bounded state and evidence
needed for admission; no such selection policy is implemented or qualified
here. Bounded five remains public. No new full-suite/fuzz, hosted CI or external
verification is claimed.

## BM-0169: Cost model for optional sixth-index activation

This is a design assessment using BM-0162 through BM-0168, not a new timing
result. File-level medians and total visit counts do not identify the beneficial
transition point within a frame. Do not infer an adaptive threshold from the
x-ray/dickens distinction or use corpus names as selection inputs.

DD-1336 proposes beginning with exact five-prefix search and constructing the
sixth index at most once, at a token boundary. Three common index pairs remain
live. At position p in a complete N-byte frame, catch-up inserts
min(p, max(N - 5, 0)) historical positions in ascending order, including match
interiors. The proposed first prototype clears the reserved sixth head/link
regions on activation, adding O(65536+N) initialization and O(p) catch-up work.
These costs must be measured, not treated as free index reuse.

| Design cost | Required accounting |
| --- | --- |
| A frame never activating six | Five search/advance plus any monitoring; extra capacity is still reserved |
| A frame activating at p | Five prefix, monitoring, sixth initialization, catch-up, then six suffix |
| Full-frame array capacity | 17,825,792 bytes versus 13,369,344 for five-only |
| Additional pair capacity | 4,456,448 bytes; not a claim about RSS or total codec memory |
| Remaining uncertainty | Monitoring cost, transition profitability, changing input distribution and selector generalization |

The next benchmark should first force transitions at recorded token boundaries,
with never/zero controls and original five/six comparators. Report initialization,
catch-up and complete replay as well as tokens, bytes, transitions and memory;
keep phase measurements distinct from uninstrumented comparisons. Do not use
a hindsight minimum of the two static times as measured adaptive throughput.

Before proposing a policy, obtain frame-level evidence and include control
overhead even on no-transition inputs. Validate thresholds on separate inputs
and mixed/reversed distributions; preserve per-member results and all adverse
runs. No numeric threshold, break-even point or speedup is established here.
An abstract 9,963-case insertion-order model supports the reconstruction
argument only. Public bounded five and all admission decisions remain unchanged.

## BM-0170: Forced lazy-six transitions and catch-up cost

DD-1337 implements the structural experiment without a content classifier.
Per-frame checkpoints are precomputed from retained token boundaries outside
timing: never, zero, first eligible token at/after one-quarter or one-half of
raw bytes, and the last eligible token. At least six remaining bytes are
required; otherwise that path stays Five. These are forced experimental
controls, not adaptive thresholds or predictions.

Three independent processes for each of x-ray/dickens use warmup, three timed
iterations per frame and rotating seven-path order. The table gives medians
of process medians in seconds. Complete replay includes reset, checkpoint
comparison and any sixth-index initialization/catch-up, after allocation.
Oracle, token/frame comparison and reconstruction are outside timing.

| Path | x-ray | dickens |
| --- | ---: | ---: |
| Bounded five | 0.408378 | 1.319800 |
| Original six | 0.445615 | 0.752613 |
| Lazy never | 0.406254 | 1.323176 |
| Lazy zero | 0.431079 | 0.745077 |
| Lazy quarter | 0.428615 | 0.757543 |
| Lazy half | 0.428550 | 0.870340 |
| Lazy late | 0.422450 | 1.342524 |

All x-ray forced-switch paths are slower than lazy never in each of the three
processes. On dickens, zero/quarter/half are faster than lazy never while late
is slower in each process. This supports the need to account for remaining
work and transition cost; it does not supply a general rule for choosing p.
Never versus bounded five also includes implementation differences and cannot
be interpreted as the isolated cost of one conditional branch. These are
finder replay times, not public encoder lifecycle or adaptive throughput.

The separate phase diagnostic prepares prior history without searches, then
measures activation initialization and catch-up. Below are medians of process
medians in seconds, summed over the input's frames; history counts are the
number of inserted prior byte positions. There are nine eligible x-ray frames
and ten dickens frames, with one transition per frame in these four modes.

| Input and checkpoint | Sixth initialization | Catch-up | Historical positions |
| --- | ---: | ---: | ---: |
| x-ray, zero | 0.0012020 | 0.0000003 | 0 |
| x-ray, quarter | 0.0012103 | 0.0054564 | 2,118,570 |
| x-ray, half | 0.0012271 | 0.0111193 | 4,237,134 |
| x-ray, late | 0.0011527 | 0.0216559 | 8,474,171 |
| dickens, zero | 0.0013856 | 0.0000005 | 0 |
| dickens, quarter | 0.0013601 | 0.0064556 | 2,548,145 |
| dickens, half | 0.0013341 | 0.0135201 | 5,096,272 |
| dickens, late | 0.0014149 | 0.0260135 | 10,192,332 |

Phase cache conditions differ from interleaved replay. Do not subtract these
numbers from complete time or infer an instruction-level cause. Zero has no
historical inserts but still initializes the sixth arrays. Late activation
constructs almost the entire history with little subsequent search left.
Actual per-frame checkpoint positions and raw iterations are retained.

All twelve corpus members pass verification-only token/frame identity and
reconstruction for every lazy path; this is full-corpus correctness validation,
not full-corpus timing. Both compiler smokes, explicit ASan/UBSan, exhaustive
small switch boundaries, wide distances, prior-match-interior references and
a seeded one-MiB-plus-five-byte cross-compiler/sanitizer fixture pass.

Reported full-capacity array payload is 17,825,792 bytes, 4,456,448 above bounded
five, including on the lazy-never path. This excludes object/allocator overhead
and other benchmark buffers and is not measured peak RSS or an admitted public
workspace. Activation makes no allocation calls; no allocation-count result
for a public factory is claimed.

The structural trial succeeds, but automatic selection and its monitoring
cost remain unimplemented. Next collect bounded per-frame observations and
measure their cost on no-transition paths before proposing or calibrating a
rule, followed by separate-input and mixed-distribution validation. Do not
choose a threshold from these two named inputs or use hindsight timing to
claim an adaptive speedup. Public bounded five and existing memory/failure
contracts remain unchanged. No full-suite/fuzz, hosted CI or external
verification result is claimed.

## BM-0171: Lazy-five observation cost and per-frame evidence

Date: 2026-09-29. DD-1338 adds four counters without additional byte comparisons.
Compare bounded five with identical-layout unobserved/observed lazy-never paths.
All arrays are reserved before measurement; reset and complete token replay are
timed. After warmup, rotate three paths across three iterations per frame, sum
per-path iteration times across frames, then take process medians. Run three
independent processes each for x-ray and dickens with alternating member order.
The table gives medians of those process medians in seconds; the final column
is the range of paired process percentage differences.

| Input | Bounded five | Unobserved | Observed | Observed vs unobserved |
|---|---:|---:|---:|---:|
| x-ray | 0.3914644 | 0.3961574 | 0.3924812 | -1.19% to +0.39% |
| dickens | 1.3044857 | 1.2956998 | 1.2825646 | -1.01% to +0.19% |

No large observation penalty appears in these focused runs. Differences include
compiler code-generation and run variation; a slightly faster instrumented
path does not mean counting has negative cost. These are finder replay results,
not end-to-end or full-corpus timing, and omit snapshot and decision overhead.

All twelve corpus members pass verification-only token identity, observed
frame identity and reconstruction, with counters matching the earlier untimed
diagnostic. Both compiler observation/lazy-six smokes and explicit observation
ASan/UBSan pass. The seeded final-short-frame fixture produces identical reports
across both compilers and the sanitizer build (TVG-1204).

Untimed cumulative snapshots are taken at the first token end reaching each
raw-byte quarter. The following ranges use differences between snapshots,
not cumulative averages, and describe five-chain visits per query. Short final
frames are included; these are observations, not thresholds or predictions.

| Input | First-quarter range | Last-quarter range |
|---|---:|---:|
| x-ray | 0.08 to 1.02 | 0.54 to 10.85 |
| dickens | 20.09 to 28.29 | 128.32 to 201.20 |
| mozilla | 0.07 to 523.44 | 0.17 to 5491.98 |

Growing history and changing content can alter search work within a frame.
In particular, the broad mozilla range warns against selecting one aggregate
cutoff from two named files. Preserve all twelve members' actual positions and
four raw counters for later analysis. Probe and prefix passes count only
surviving candidates; they cannot classify every rejected candidate or directly
predict six-chain visits and fallback cost.

Counter payload is 32 bytes on the measured builds. Full-capacity index payload
remains 17,825,792 bytes including the reserved sixth pair, even without a
transition. Neither number is peak memory for the benchmark or a public query.

Next specify a deterministic observation/checkpoint and activation-cost model,
then test it on separate inputs and mixed/reversed distributions before
calibrating or admitting a policy. Do not infer adaptive speedup from these
never-activation runs. Public bounded five, stream format, memory preflight,
failure atomicity and failed-frame non-publication remain unchanged. No new
full-suite/fuzz, hosted CI or external qualification is claimed.

## BM-0172: Frozen activation-model sensitivity on separate and mixed inputs

Date: 2026-09-29. DD-1339 freezes nine hypothetical saving/cost assumptions and
quarter/half decisions before measurement. Use seven one-MiB inputs: first full
frames from ooffice, osdb and sao, seeded structured (high), random (low), and
both orders of their first half-frame pieces (TVG-1205). The corpus inputs had
previously been observed; these are separate timing cases, not blind holdouts.

Run three independent forced-replay processes per case, reversing case order
in the middle process. Each warms up and rotates seven modes across three
iterations. The following seconds are medians of process medians. Mode counts
show the predictions from all nine models; never results are retained rather
than dropping models that miss savings. No evaluation timing changes a model.

| Case | Never | Quarter | Half | Models: quarter / half / never |
|---|---:|---:|---:|---:|
| ooffice-first-frame | 0.0674925 | 0.0613086 | 0.0634849 | 0 / 1 / 8 |
| osdb-first-frame | 0.0478583 | 0.0464144 | 0.0475151 | 0 / 0 / 9 |
| sao-first-frame | 0.0758870 | 0.0732522 | 0.0741663 | 0 / 0 / 9 |
| high | 0.0918287 | 0.0591228 | 0.0723012 | 2 / 0 / 7 |
| low | 0.0822371 | 0.0869540 | 0.0870852 | 0 / 0 / 9 |
| high_low | 0.0987779 | 0.0892445 | 0.1022269 | 2 / 0 / 7 |
| low_high | 0.0914683 | 0.0574286 | 0.0577078 | 0 / 0 / 9 |

Only cost scale 1 with saving fractions 1/2 or 3/4 selects quarter for high and
high_low. Only scale 1 with fraction 3/4 selects half for ooffice. All models
stay Five for low, low_high, osdb and sao. These outcomes are diagnostic and do
not select a preferred coefficient set.

Paired process ranges relative to never are:

| Case and forced choice | Difference |
|---|---:|
| high, quarter | -35.66% to -34.25% |
| high_low, quarter | -11.70% to -8.41% |
| high_low, half | +0.97% to +3.99% |
| low, half | +3.31% to +6.46% |
| low_high, half | -37.30% to -36.55% |

Low and low_high have exactly the same observed evidence at both available
decision boundaries, including token-end positions and all four counters. Half
activation hurts low in all three runs but helps low_high in all three. No
threshold change using only those same observations can distinguish the two
cases at that boundary. The late-heavy case exposes the deadline at half and
the extrapolation assumption; it does not prove all online selection is futile.
High_low additionally shows that a later activation can lose after an earlier
one would have won. Coefficients fitted to aggregate work cannot resolve an
unobserved distribution change.

All seven observation cases retain exact tokens, frames and reconstruction;
reports agree across both compilers and explicit ASan/UBSan. All 21 forced
processes retain token/frame identity for every lazy mode. Model arithmetic,
strict ties, prefix causality and report validation tests pass (TVG-1205).

Forced timings include reset, checkpoint checks and activation/catch-up but
exclude online counters and decision evaluation. They are counterfactual checks,
not adaptive throughput or end-to-end speedups. Isolated activation-phase times
remain separate. Existing fourth-index reservation remains 17,825,792 array
payload bytes; no memory reduction or peak-RSS measurement is claimed.

Do not admit or tune this grid. Next trial observation opportunities after half
with an explicit remaining-work/catch-up guard, retaining these identical-prefix
and reversed-distribution cases and measuring the entire online decision path.
Later evidence may help but cannot guarantee arbitrary future savings. Public
bounded five and its format, preflight, failure atomicity and failed-frame
non-publication contracts remain unchanged. No new full-suite/fuzz, hosted CI,
external qualification or full-corpus timing is claimed.

## BM-0173: Complete online monitoring, late decision and activation costs

Date: 2026-09-29. DD-1340 implements later observations while retaining the nine
unfitted hypothetical models. Reuse the seven TVG-1205 inputs unchanged. Run three
independent processes per input (reverse order in the middle process), with
warmup and three rotated iterations of twelve paths. Time reset, token replay,
counters, schedule tests, integer decisions, bounded trace writes and any full
sixth-index construction/catch-up. Reporting and equality/frame checks are outside
timing. These are complete finder replays, not end-to-end encoder measurements.

The table shows milliseconds as medians of process medians. s2k1 and s3k1 mean
saving fractions 2/4 and 3/4 with work scale one. They are the only models that
activate on these seven inputs, not newly selected production policies. The
last column retains the range of paired changes versus unobserved across every
process and all seven other models, all of which stay Five on these inputs.

| Case | Unobserved | Monitor only | s2k1 | s3k1 | Other seven vs unobserved |
|---|---:|---:|---:|---:|---:|
| ooffice-first-frame | 69.157 | 69.587 | 67.600 | 65.901 | -3.87% to +3.00% |
| osdb-first-frame | 50.560 | 48.963 | 48.537 | 48.327 | -5.61% to +0.11% |
| sao-first-frame | 77.098 | 77.768 | 77.120 | 76.820 | -1.60% to +3.43% |
| high | 93.467 | 93.055 | 62.662 | 62.568 | -1.49% to +20.26% |
| low | 83.901 | 86.306 | 87.008 | 85.878 | -1.25% to +6.32% |
| high_low | 107.341 | 104.976 | 96.001 | 93.573 | -6.95% to +4.08% |
| low_high | 92.652 | 93.820 | 96.011 | 70.373 | -5.06% to +12.81% |

The actual activation position is 262,149 for s2k1/s3k1 on high and high_low,
327,684 for s3k1 on ooffice, and 655,360 for s3k1 on low_high. All other cases
and models remain Five. Decisions repeat across iterations and compiler builds.

Paired process changes for the activating s3k1 path versus unobserved are:

| Case | Change |
|---|---:|
| ooffice-first-frame | -8.27% to -2.71% |
| high | -33.78% to -32.07% |
| high_low | -12.83% to -7.94% |
| low_high | -27.94% to -21.60% |
| low | -0.19% to +6.62% |

The late-heavy mixed case now responds after half and improves in all three
runs with counters and catch-up included. It is evidence that exact 1MiB search
still has input-dependent headroom, not a general adaptive-speed guarantee.
On low, no model activates; monitor-only costs 1.90% to 4.65% versus unobserved.
Across the other inputs monitor differences vary in sign, and high includes a
16.53% slow run. Preserve those results; do not infer a constant monitoring cost,
a causal compiler explanation or a precise small gain from three processes.

All twelve corpus members pass separate verification-only token/frame checks
and rational trace validation. Eight cross-build fixtures, including actual
activation followed by a five-byte final frame, agree between both compilers
and explicit ASan/UBSan. Remaining-work guard boundary smokes pass (TVG-1206).
Full-corpus correctness does not substitute for full-corpus timing or public
failure/chunking qualification. The fixed nine-event trace allocates no replay
storage dynamically. Full-capacity index array payload remains 17,825,792 bytes,
4,456,448 above bounded five; no peak RSS or public workspace claim is made.

Do not admit a model or tune its coefficients from these results. Late observation
addresses the previous half-frame deadline, but monitoring regression and the
hypothetical savings estimate remain unresolved. Preserve this online prototype
as a comparison route. Before adding more selector complexity, a separate
benchmark-only experiment may reduce collisions in the existing five-prefix
index by changing its long-prefix bucket count. The admitted specialized finder
still uses 65,536 buckets; DD-1160's 262,144 cap applies to the different standard
HashChain route. Such a trial must keep exact tokens and independently measure
initialization, memory and complete replay; it is not an approved public memory
change or a claim of benefit on repeated exact prefixes.

Public bounded five, format, preflight, failure atomicity and failed-frame
non-publication remain unchanged. No new full-suite/fuzz, hosted CI or external
qualification is claimed.

## BM-0174: Five-prefix bucket width, collisions and initialization tradeoffs

Date: 2026-09-29. DD-1341 fixes five-byte bucket widths 16/18/20 while retaining
16-bit three/four-byte indexes. Compare bounded production, original shared-key
five and a guarded 16-bit control with both wider variants. All variants retain
exact tokens. Timed replay includes reset and all index updates/searches; counted
classifications, IO, reporting, equality and frame checks are outside timing.

The following is one process per complete corpus member, each with warmup and
three rotated measured iterations. Entries are median seconds. Treat this as
screening, not replicated whole-corpus evidence or end-to-end throughput.

| Input | Bounded | Original | 16 bits | 18 bits | 20 bits |
|---|---:|---:|---:|---:|---:|
| dickens | 1.760367 | 1.280800 | 1.288446 | 1.783788 | 1.609105 |
| mozilla | 5.565820 | 5.542885 | 5.558057 | 5.335096 | 5.404386 |
| mr | 2.066333 | 2.084594 | 2.051940 | 2.062713 | 2.055249 |
| nci | 2.639501 | 2.557447 | 2.545426 | 2.943380 | 3.281345 |
| ooffice | 0.416606 | 0.413584 | 0.410764 | 0.403415 | 0.344473 |
| osdb | 0.490237 | 0.464512 | 0.466230 | 0.460174 | 0.485409 |
| reymont | 1.651863 | 1.669499 | 1.638251 | 1.917180 | 1.857247 |
| samba | 1.545251 | 1.524939 | 1.498213 | 1.471803 | 1.488807 |
| sao | 0.666847 | 0.551965 | 0.584519 | 0.549717 | 0.598368 |
| webster | 5.427272 | 7.513576 | 5.352788 | 5.209194 | 5.224205 |
| xml | 0.250498 | 0.253440 | 0.244531 | 0.241631 | 0.241364 |
| x-ray | 0.381712 | 0.400764 | 0.399684 | 0.289885 | 0.326619 |
| Sum | 22.862307 | 24.258005 | 22.038849 | 22.667974 | 22.916577 |

Two additional independent processes each for x-ray/dickens give these paired
ranges against the guarded 16-bit control. Keeping this control avoids assigning
all production-versus-benchmark implementation differences to bucket width.

| Input | 18 vs 16 bits | 20 vs 16 bits |
|---|---:|---:|
| x-ray | -28.02% to -14.87% | -33.86% to -18.28% |
| dickens | -4.80% to +38.44% | -3.91% to +24.89% |

Untimed classification confirms identical true-prefix visits and decreasing
collisions for every frame. Selected totals explain why collision removal has
different potential across inputs, without predicting CPU time:

| Input | True-prefix visits, all widths | Collisions, 16 | Collisions, 18 | Collisions, 20 |
|---|---:|---:|---:|---:|
| x-ray | 628,776 | 11,521,625 | 2,879,470 | 714,455 |
| dickens | 114,905,033 | 8,771,384 | 2,368,087 | 605,946 |
| mozilla | 2,810,179,612 | 28,742,158 | 6,709,609 | 1,766,467 |
| nci | 828,954,423 | 3,549,443 | 915,793 | 623,121 |

Full-capacity index payload is 13,369,344 / 14,155,776 / 17,301,504 bytes. Wider
heads add 768 KiB / 3.75 MiB; the number of position links is unchanged. Payload
excludes object, allocator, token/frame buffers and process overhead; no peak
RSS or admitted public workspace measurement is claimed.

Separate warmed-array reset diagnostics sum to 0.090510 / 0.093130 / 0.120719 seconds
across the twelve members. Cache conditions differ from interleaved replay;
do not subtract these times to claim a causal search/initialization breakdown.
Fewer logical visits need not imply proportionally shorter elapsed time, and
these measurements do not establish an instruction- or cache-level cause.

All corpus token/frame/restoration checks, counter identities, compiler/sanitizer
smokes and eight cross-build fixtures pass (TVG-1207). Preserve both improvements
and regressions. Do not promote a wider table solely because it removes more
collisions or wins on one member. Public bounded five, format, workspace limits,
failure atomicity and failed-frame non-publication remain unchanged. No new
full-suite/fuzz, hosted CI or external qualification is claimed.

The single-pass corpus sum is +2.85% for 18 bits and +3.98%
for 20 bits versus guarded 16. On x-ray both wider variants improve in all three
processes. On dickens the first process is slower, while both subsequent
processes are faster with identical visit counts. Retain the first result rather
than discarding it as an outlier. The mechanism of this process-level variation
is unverified. The bounded/original controls also differ for some inputs, so a
production-versus-vector comparison alone cannot identify bucket-width effects.

Keep both wider variants unadmitted. The experiment establishes exact collision
refinement and input-dependent potential, not a robust global policy. Next
isolate the reproducibility of complete replay timing with unchanged binaries
before selecting a width or implementing a bounded production variant. Preserve
the old online experiment as a separate comparison; do not combine unqualified
changes or silently increase public memory requirements.

## BM-0175: Unchanged width benchmark scheduling controls

Date: 2026-09-29. DD-1342 retains the BM-0174 executable and source identities.
Measure five independent default/single-logical-processor pairs per input for
dickens and x-ray, alternating condition and input order across pairs. Child
affinity is inherited at creation and checked. All twenty scheduled processes
are retained. There are no rebuilds or concurrent build/test runs.

Each process still measures reset plus token replay, with warmup and three
rotated iterations, for bounded five, original shared-key five and guarded
16/18/20-bit paths. Values below use each process's median of those three totals.
The ratios compare a wider path with guarded 16 in the same process. Negative
means faster; ranges include every run and are not confidence intervals.

| Input | Eligibility | 18 vs 16 range | Median ratio change | 20 vs 16 range | Median ratio change |
|---|---|---:|---:|---:|---:|
| dickens | default | -59.61% to +9.15% | -8.62% | -58.09% to -3.45% | -6.00% |
| dickens | one logical processor | -9.03% to -4.22% | -4.88% | -7.36% to -0.92% | -4.36% |
| x-ray | default | -58.85% to -2.61% | -43.03% | -55.26% to -28.36% | -40.94% |
| x-ray | one logical processor | -42.12% to -8.51% | -29.99% | -35.90% to -2.83% | -33.08% |

Large apparent improvements can reflect a slow comparison denominator. Absolute
process-median ranges in seconds are therefore necessary context:

| Input / eligibility | Bounded five | Original five | Guarded 16 | Guarded 18 | Guarded 20 |
|---|---:|---:|---:|---:|---:|
| dickens / default | 1.296–1.579 | 1.355–3.238 | 1.290–3.144 | 1.237–1.459 | 1.230–1.330 |
| dickens / fixed | 1.330–2.792 | 1.333–1.856 | 1.317–1.415 | 1.242–1.287 | 1.255–1.320 |
| x-ray / default | 0.383–0.429 | 0.477–0.668 | 0.477–0.702 | 0.289–0.478 | 0.298–0.352 |
| x-ray / fixed | 0.402–0.708 | 0.419–0.693 | 0.448–0.555 | 0.317–0.409 | 0.315–0.455 |

Some large slowdowns persist across all three internal iterations. For example,
one default dickens process records guarded 16 totals of 2.759/2.794/2.794 seconds;
a fixed process records bounded five at 2.815/2.773/2.792; another default process
records original five at 3.224/3.282/3.238. These are not single slow iterations
discarded by the median. Different paths exhibit the slowdown across processes.
Preserve these runs and the earlier adverse BM-0174 results.

Both wider paths improve on x-ray in all ten new processes. Fixed dickens also
improves in all five, but default dickens retains an 18-bit regression. This is
evidence of input-dependent potential, not a reliable global gain. Eligibility
restriction alone does not stabilize every control, and the data do not identify
CPU migration, frequency, cache behavior, address placement or compiler effects
as the cause. No hardware-counter or per-frame timing evidence was collected.

All non-timing fields match BM-0174 in all twenty processes (TVG-1208), including
exact/collision counts and token/frame totals. Executable/source identities are
unchanged. Separate warmed-array reset values are preserved but not subtracted
to infer a search cost. This is neither replicated whole-corpus nor end-to-end
compression evidence, and no peak-memory measurement is added.

Keep both wider variants unadmitted. Next isolate same-code controls: multiple
16-bit instances with separate working storage, controlled allocation/execution
order and retained per-frame timing. Treat storage/order sensitivity as a testable
hypothesis, not an established explanation. Public format, workspace limits,
failure atomicity and failed-frame non-publication remain unchanged. No new
full-suite/fuzz, hosted CI or external qualification is claimed.

## BM-0176: Same-code instance and order controls

Date: 2026-09-29. DD-1343 adds a separate diagnostic executable without changing
the guarded 16-bit finder or the earlier width executable. Three independently
allocated instances of the same type enter one non-inlined function measuring
reset plus token replay. They share input and output buffers. Allocation labels
are constructed forward/reverse; execution direction is independently selectable.
For each frame, one warmup cycle and three rotated measured cycles make every
instance occupy every execution rank once. Token comparison, frame checks and
reporting occur outside the measured function.

The protocol fixes three processes per allocation/execution combination for
each of dickens and x-ray: 24 total. Configuration order rotates/reverses across
repetitions, input order alternates, processor eligibility remains at its default,
and no build/test work runs concurrently. All scheduled runs are retained.

For each process, take each instance's median of its three whole-input iteration
totals. Define spread as `100 * (largest instance median / smallest - 1)`.
This is diagnostic variation among identical implementations, not speedup.

| Input | Processes | Minimum spread | Median spread | Maximum spread | All instance medians, seconds |
|---|---:|---:|---:|---:|---:|
| dickens | 12 | 0.063% | 0.304% | 0.859% | 1.270562–1.295803 |
| x-ray | 12 | 0.037% | 0.568% | 1.328% | 0.381962–0.389157 |

Retain the configuration breakdown rather than selecting a favorable order.
Each cell below contains spreads for all three independent repetitions:

| Allocation / execution | dickens spread (%) | x-ray spread (%) |
|---|---|---|
| forward / forward | 0.063, 0.244, 0.348 | 0.788, 0.554, 0.582 |
| forward / reverse | 0.762, 0.302, 0.306 | 1.145, 1.197, 1.328 |
| reverse / forward | 0.282, 0.369, 0.150 | 0.037, 0.414, 0.349 |
| reverse / reverse | 0.859, 0.501, 0.231 | 0.737, 0.396, 0.374 |

Per-frame medians preserve shorter-scale differences that totals can conceal.
Across 120 dickens frame observations, maximum inter-instance spread is 4.42%
(0.067997/0.068007/0.071000 seconds). Across 108 x-ray frame observations,
maximum is 16.83% on the short final frame
(0.0012092/0.0010453/0.0010350 seconds). Thus small total-time spreads do not
mean every individual frame is equally stable. Raw per-frame times and execution
ranks are retained, and their sums agree with reported iteration totals.

All 24 processes pass indexed-token, representative serialized-frame and restored
byte comparisons, with sizes and token/frame-byte totals matching BM-0174.
TVG-1209 also covers both compiler/sanitizer smokes and 48 cross-build fixture
executions. Measured source/executable hashes and the older width executable's
identity remain unchanged. Three finder array payloads are 13,369,344 bytes each,
40,108,032 bytes combined; this excludes other diagnostic buffers and is not a
peak-memory or proposed public workspace measurement.

The large BM-0175 variation is not reproduced under this harness's tested
allocation/execution permutations. This is a useful same-code control, not proof
that allocation cannot matter or that inlining caused the earlier behavior.
The new function boundary, code layout, number/types of live instances and memory
layout differ from the old harness. Physical page placement, alignment effects,
cache behavior and hardware counters were not measured. Do not compare absolute
times between the two harnesses as an optimization gain.

Next reintroduce width comparisons through an explicit common timing boundary,
retaining duplicate same-code controls and balanced execution order. Both wider
variants remain unadmitted; retain all earlier improvements and regressions.
Public codec/format, memory limits, failure atomicity and failed-frame publication
are unchanged. No new full-suite/fuzz, hosted CI or external qualification claim.

## BM-0177: Width comparison with duplicate 16-bit controls

Date: 2026-09-29. DD-1344 compares four live instances: two guarded 16-bit
controls, one 18-bit and one 20-bit finder. Reset plus token replay uses one
non-inlined function template, with dispatch outside timing. Both controls share
a specialization; different widths have different generated specializations.
No finder or production implementation changes. Keep the earlier diagnostic
executables and observations intact.

Each frame has one warmup cycle and four measured rotations, so every slot
occupies each rank once. Allocation and execution directions independently
reverse. Three processes for each of four combinations on dickens and x-ray
give 24 processes, all retained. Configuration/input order is predetermined;
default processor eligibility is unchanged and build/test work is excluded.
All values below use the median of four whole-input iteration totals per slot.
Negative changes mean faster; ranges are observed ranges, not confidence intervals.

| Input | Width | Change vs control 0 | Change vs control 1 | Faster than both controls |
|---|---:|---:|---:|---:|
| dickens | 18 | -25.68% to +41.33% | -54.16% to +41.38% | 7/12 |
| dickens | 20 | -25.62% to +32.35% | -54.23% to +32.82% | 9/12 |
| x-ray | 18 | -50.84% to -9.22% | -38.04% to +14.65% | 11/12 |
| x-ray | 20 | -57.79% to +8.18% | -46.31% to -2.21% | 11/12 |

The duplicate control spread is `100 * (larger control median / smaller - 1)`.
It must be reported alongside width ratios; choosing only a slow denominator
can exaggerate a gain.

| Input | Control spread min / median / max | Control 0 seconds | Control 1 seconds | 18-bit seconds | 20-bit seconds |
|---|---:|---:|---:|---:|---:|
| dickens | 0.04% / 1.27% / 106.48% | 1.290967–1.646816 | 1.295571–2.726024 | 1.223848–1.894648 | 1.224984–1.747411 |
| x-ray | 0.20% / 15.41% / 68.64% | 0.383154–0.708476 | 0.392670–0.507422 | 0.289447–0.455536 | 0.272441–0.424164 |

Both improvements and adverse runs remain evidence. In the first forward-allocation,
reverse-execution dickens process, controls are 1.336075/1.344011 seconds
(0.59% spread), while 18/20 bits take 1.627518/1.438746 seconds. Width regressions
therefore cannot all be attributed to a slow control. Conversely, the first
reverse/reverse x-ray process has controls 0.392096/0.433743 and 20-bit 0.424164
seconds: it beats only one control. Neither example is removed from the table.

Per-frame timing and execution ranks are retained and independently sum-checked.
All content checks pass, together with both compiler/sanitizer smokes, 48 fixture
executions and twelve corpus verification runs (TVG-1210). The corpus pass is
verification-only; it is not a whole-corpus performance measurement.

Full-capacity array payload is 13,369,344 bytes per 16-bit instance, 14,155,776
for 18 and 17,301,504 for 20: 58,195,968 combined. This excludes other diagnostic
buffers and is not peak process memory or a proposed public workspace size.

Adding widths to the common-boundary harness does not preserve BM-0176's small
duplicate-control spread. Live allocations and generated code differ, so this
does not identify cache behavior, page placement, instruction layout or frequency
as a cause. A common source-level timing boundary alone is insufficient to make
all comparisons stable. Keep wider variants unadmitted and do not turn a
focused-input win into a global width policy. Public format, memory limits,
failure atomicity and failed-frame non-publication remain unchanged. No new
full-suite/fuzz, hosted CI or external qualification is claimed.

## BM-0178: Runtime widths through one code path and storage instance

Date: 2026-09-29. DD-1345 replaces separate diagnostic instances by one finder,
one set of maximum-capacity arrays and one non-inlined runtime reset/replay
function. Logical slots request 16/16/18/20 bits. The duplicate 16-bit slots
repeat the same configuration on the same storage. Reset validates width/length
before mutation, clears active heads and input-sized links, then starts replay.
Backing long-head capacity stays at 20 bits even during narrower modes.

After a warmup cycle, four measured rotations per frame balance execution rank.
Three processes per direction on dickens and x-ray produce twelve predetermined
processes, all retained. Direction/input order alternates across repetitions.
Default processor eligibility is unchanged and no builds/tests run concurrently.
Each slot value is the median of four whole-input reset/replay iteration totals.
Ratios compare each wider slot with both repeated 16-bit slots in its process;
negative means faster. These are observed ranges, not confidence intervals.

| Input | Width | Change vs 16-bit slot 0 | Change vs 16-bit slot 1 | Faster than both |
|---|---:|---:|---:|---:|
| dickens | 18 | -5.61% to -4.40% | -6.03% to -4.68% | 6/6 |
| dickens | 20 | -6.63% to -5.45% | -6.36% to -5.66% | 6/6 |
| x-ray | 18 | -26.57% to -25.49% | -25.95% to -25.36% | 6/6 |
| x-ray | 20 | -33.31% to -32.39% | -33.09% to -32.02% | 6/6 |

Control spread is `100 * (larger 16-bit median / smaller - 1)`:

| Input | Control spread min / median / max | Slot 0 seconds | Slot 1 seconds | 18-bit seconds | 20-bit seconds |
|---|---:|---:|---:|---:|---:|
| dickens | 0.20% / 0.47% / 0.76% | 1.293921–1.320632 | 1.298829–1.329905 | 1.235187–1.249694 | 1.221952–1.245335 |
| x-ray | 0.15% / 0.25% / 1.35% | 0.391803–0.401926 | 0.390779–0.401026 | 0.291398–0.299325 | 0.262402–0.270056 |

Per-frame times and ranks are retained; finite-duration, rank and timing-sum
checks pass. All token/frame/restoration checks, both compiler/sanitizer smokes,
48 cross-build fixture executions and twelve corpus verification runs pass
(TVG-1211). The corpus runs are verification-only, not full-corpus timing.

At a full frame the single finder array payload is 17,301,504 bytes for every
mode, excluding object and other diagnostic buffers. Only active heads are reset.
This is neither measured peak process memory nor a public workspace proposal.
The narrower modes retain excess backing capacity and use variable shifts;
their timings cannot be substituted for production specialized-finder timings.

Shared storage removes separate-instance placement from the within-process
comparison, and the runtime function removes distinct width specializations.
It does not fix cache residency, frequency or all preceding-execution effects,
nor identify the cause of the earlier mixed-instance variation. Preserve those
earlier results; do not compare times across harnesses as an optimization gain.
The focused measurements alone do not justify a global width policy. Public
format, memory limits, failure atomicity and failed-frame non-publication remain
unchanged; no full-suite/fuzz, hosted CI or external qualification claim.

Both wider modes improve against both repeated controls in all six processes per
input. Maximum whole-input control spread is 0.76% on dickens and 1.35% on x-ray;
per-frame control spread is larger, reaching 4.39% across 60 dickens observations
and 5.78% across 54 x-ray observations. Shared runtime replay produces consistent
focused-input direction in this run, not a universal performance guarantee.
Next replicate timing across all twelve corpus members using this preserved
binary and retain memory/cross-harness limitations before proposing any bounded
production integration. Wider production variants remain unadmitted.

## BM-0179: Three complete-corpus passes with runtime widths

Date: 2026-09-29. DD-1346 keeps the BM-0178 runtime binary and source unchanged.
Three passes over twelve manifest-verified members give 36 independent processes.
Member order rotates by four positions per pass, with the middle pass reversed;
each member's execution direction alternates. Each member appears twice in one
direction and once in the other, balanced globally. All planned runs are retained.
No rebuild or concurrent build/test work occurs during measurement.

One finder and maximum-capacity storage serve logical slots 16/16/18/20. Each
frame has warmup and four rank-balanced reset/replay iterations. Slot values are
medians of the four whole-input iteration totals. The following sums add those
member medians within each pass; they are not end-to-end compression time,
concatenated-input measurements or a newly selected fastest path. Negative
relative changes mean faster; each range includes comparison with both controls.

| Pass | 16-bit slot 0 sum (s) | 16-bit slot 1 sum (s) | 18-bit sum (s) | 20-bit sum (s) | 18-bit change | 20-bit change |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 22.137724 | 22.138611 | 21.382209 | 21.277236 | -3.42% to -3.41% | -3.89% to -3.89% |
| 2 | 22.602104 | 22.591725 | 21.694747 | 21.556629 | -4.01% to -3.97% | -4.63% to -4.58% |
| 3 | 23.902235 | 23.989946 | 23.014844 | 23.012741 | -4.06% to -3.71% | -4.07% to -3.72% |

Per-member observed ranges include all three processes and both 16-bit controls.
The win count requires being faster than both controls in that process. Maximum
control spread is `100 * (larger control median / smaller - 1)`. Ranges are not
confidence intervals; small differences remain sensitive to measurement variation.

| Input | 18-bit change range | Wins vs both | 20-bit change range | Wins vs both | Maximum control spread |
|---|---:|---:|---:|---:|---:|
| dickens | -5.02% to -3.73% | 3/3 | -6.20% to -4.81% | 3/3 | 1.36% |
| mozilla | -5.22% to -3.15% | 3/3 | -6.36% to -2.51% | 3/3 | 0.50% |
| mr | -2.77% to -2.10% | 3/3 | -3.15% to -2.37% | 3/3 | 0.65% |
| nci | -1.08% to -0.19% | 3/3 | -0.30% to +1.19% | 2/3 | 0.90% |
| ooffice | -12.11% to -8.28% | 3/3 | -15.77% to -11.80% | 3/3 | 1.22% |
| osdb | -9.34% to -4.67% | 3/3 | -14.06% to -5.16% | 3/3 | 0.96% |
| reymont | -2.06% to -0.88% | 3/3 | -1.93% to -1.16% | 3/3 | 0.61% |
| samba | -4.10% to -3.18% | 3/3 | -3.92% to +0.65% | 2/3 | 0.96% |
| sao | -8.17% to -7.19% | 3/3 | -9.00% to -5.95% | 3/3 | 0.58% |
| webster | -3.45% to -3.00% | 3/3 | -3.86% to -3.29% | 3/3 | 0.28% |
| xml | -3.81% to -2.39% | 3/3 | -5.32% to -3.79% | 3/3 | 1.19% |
| x-ray | -34.19% to -26.07% | 3/3 | -44.89% to -32.46% | 3/3 | 0.57% |

All 36 content checks pass, including earlier adverse cases nci/reymont. Tokens,
frame bytes and restoration agree with previous reports, and independent rank/
timing-sum checks pass (TVG-1212). Complete per-frame timing/rank reports and
old/new source/executable hashes are preserved. Compiler/sanitizer tests were
not rerun; the unchanged executable retains BM-0178's validation history.

Every mode retains the same 17,301,504-byte full-frame array backing. Narrower
active heads are cleared only up to their selected width. A specialized 18-bit
array design would add 786,432 bytes over 16 bits; 20 bits would add 3,932,160.
These are array-payload arithmetic, not observed peak memory or an admitted
workspace change. Extra diagnostic backing, runtime shifts and shared storage
mean the measurements do not establish production throughput or memory use.

Preserve the earlier specialized/mixed-instance adverse results. This experiment
does not identify why their variation occurred. Neither overall sums nor a single
member's gains alone justify a global/adaptive policy. Public format, memory
limits, failure atomicity and failed-frame non-publication remain unchanged.
No new full-suite/fuzz, hosted CI or external qualification is claimed.

Across all 36 processes, 18 bits is faster than both repeated controls. The
pass-sum reduction spans 3.41%–4.06%. This is a consistent observed direction,
not proof that nci's small 0.19%–1.08% differences exceed measurement noise.
20 bits beats both controls in 34/36 processes: the third-pass nci result is
slower than both and the third-pass samba result lies between them. Its pass-sum
reduction spans 3.72%–4.63%. Duplicate-control spread never exceeds 1.36% in
whole-input medians, while absolute process times vary between passes.

Relative to 18-bit sums, 20 bits reduces time by about 0.49%, 0.64% and 0.01%
across the three passes. Its specialized array payload would require another
3 MiB beyond 18 bits. Prefer 18 bits as the first candidate for a bounded private
workspace prototype, retaining a reference 16-bit path and explicit size/preflight
checks. This is a next experiment, not public admission: the current runtime
harness cannot establish the speed or memory behavior of that implementation.
Validate exact tokens/frames and failure preservation, then measure that bounded
implementation before changing a public default or workspace contract.

## BM-0180: Bounded private 18-bit candidate screening

Date: 2026-09-29. DD-1347 implements separate private 18-bit finder/tokenizer
sources, compiled only into dedicated tests and diagnostics. Production library
source lists, 16-bit finder/tokenizer, factory and default memory limits remain
unchanged. Only the long-prefix head count/projection and explicit array layout
differ from the retained bounded reference.

For N >= 3 the candidate borrows 4 * (393216 + 3*N) bytes aligned for uint32_t;
shorter inputs borrow no arrays. The query charges input plus active arrays plus
finder state against the internal-buffer limit. Full-frame arrays occupy
14,155,776 bytes, 786,432 bytes above 16-bit arrays. This is explicit array
arithmetic, not a peak-process or admitted public-workspace measurement.

A separate diagnostic shares one maximum candidate workspace across slots
16/16/18, each initializing only its queried prefix. The two 16-bit controls
share the same specialization; the candidate has a different specialization.
The measured scope includes checked initialization and token replay. Warmup and
three rotated iterations per frame balance execution ranks. Three processes
per direction on dickens and x-ray give twelve predetermined processes. No
build/test work overlaps timing; old and new source/executable identities are
checked, and every run is retained.

Use the median of each slot's three whole-input iteration totals and compare
18 bits against both controls. Negative means faster. The ranges below include
all six processes and are observations, not confidence intervals.

| Input | Change vs control 0 | Change vs control 1 | Faster than both | Control spread min / median / max |
|---|---:|---:|---:|---:|
| dickens | -6.36% to -4.66% | -5.14% to -3.89% | 6/6 | 0.04% / 0.61% / 1.31% |
| x-ray | -26.71% to -25.77% | -27.00% to -25.81% | 6/6 | 0.03% / 0.35% / 0.99% |

| Input | Control 0 seconds | Control 1 seconds | 18-bit seconds |
|---|---:|---:|---:|
| dickens | 1.291650–1.350810 | 1.292181–1.338578 | 1.228789–1.286559 |
| x-ray | 0.382242–0.386587 | 0.380301–0.386773 | 0.280915–0.286950 |

All twelve timed processes match indexed tokens and representative serialized
frames and restore the input. Both compilers and ASan/UBSan pass detailed tests,
both allocation guards pass, 48 cross-build fixture reports agree, and all twelve
corpus members pass verification-only differential checks (TVG-1213). The corpus
pass is not a whole-corpus performance result. Preserve per-frame ranks/times,
all regressions and duplicate-control differences rather than selecting a
favorable denominator.

The diagnostic keeps one maximum workspace live and uses direct finder replay;
it is not the public encoder or end-to-end compression. Earlier runtime-width
gains cannot be substituted for this candidate's measurements. No public
workspace size or algorithm default changes, and no new full-suite/fuzz, hosted
CI or external qualification is claimed. Keep the candidate unadmitted pending
its own bounded-path evidence; all original failure/publication contracts remain.

The candidate beats both bounded 16-bit controls in all six processes per input:
dickens improves 3.89%–6.36%, x-ray 25.77%–27.00%. Maximum whole-input control
spread is 1.31% and 0.99%, respectively. This supports further bounded-path
measurement, not a public default change. Next replicate this preserved bounded
binary across all twelve corpus members, including nci/reymont and all earlier
adverse cases, before any factory/workspace integration decision.


## BM-0181: Replicated complete-corpus bounded 18-bit measurements

Date: 2026-09-29. DD-1348 repeats the preserved DD-1347 diagnostic over all
twelve manifest-verified members in three passes. Member order rotates by four
per pass and reverses on the middle pass; each member alternates forward/reverse
execution. All 36 processes are retained. The source and executable identities,
including earlier diagnostic artifacts, match before and after measurement.

Each frame has one warmup and three rank-balanced measured iterations across
slots 16/16/18 using one shared maximum-sized workspace. Take each slot's median
of whole-input iteration totals, then sum those medians across each pass. These
are checked initialization and finder replay times, not public pipeline times.
Negative percentage means less elapsed time. No builds or tests overlap timing.

| Pass | Control 0 seconds | Control 1 seconds | 18-bit seconds | Change vs control 0 | Change vs control 1 |
|---|---:|---:|---:|---:|---:|
| 1 | 22.060775 | 22.061967 | 21.359796 | -3.18% | -3.18% |
| 2 | 21.909480 | 21.926326 | 21.210562 | -3.19% | -3.26% |
| 3 | 22.046465 | 22.040964 | 21.315568 | -3.32% | -3.29% |

The following ranges include all three processes and both controls for each
member. They are observed ranges, not confidence intervals.

| Member | 18-bit change range | Faster than both controls | Slower than both | Maximum control spread |
|---|---:|---:|---:|---:|
| dickens | -4.92% to -4.09% | 3/3 | 0/3 | 0.60% |
| mozilla | -3.07% to -1.98% | 3/3 | 0/3 | 0.41% |
| mr | -2.97% to -1.35% | 3/3 | 0/3 | 0.90% |
| nci | -1.36% to +0.09% | 1/3 | 0/3 | 1.47% |
| ooffice | -11.04% to -9.23% | 3/3 | 0/3 | 0.71% |
| osdb | -5.94% to -4.46% | 3/3 | 0/3 | 0.57% |
| reymont | -1.59% to -0.70% | 3/3 | 0/3 | 0.79% |
| samba | -4.18% to -3.36% | 3/3 | 0/3 | 0.52% |
| sao | -6.74% to -5.47% | 3/3 | 0/3 | 0.41% |
| webster | -3.63% to -2.68% | 3/3 | 0/3 | 0.40% |
| xml | -3.61% to -1.68% | 3/3 | 0/3 | 1.05% |
| x-ray | -27.46% to -25.27% | 3/3 | 0/3 | 1.44% |

Across passes, sums improve 3.18% to 3.32% against
the two controls. The candidate is faster than both in 34/36 processes,
slower than both in 0/36; maximum whole-input control spread is 1.47%.
Retain near-ties and adverse observations. Small member differences within
control variation do not establish a statistically significant improvement.

Every measured replay matches indexed-reference tokens. A representative replay
per frame serializes identically and restores the input. Frame sizes, token and
serialized-byte totals agree with retained verification-only reports; execution
ranks, finite timing values and frame-to-input time sums pass validation.

The candidate's full-frame arrays remain 14,155,776 bytes, 786,432 bytes above
16 bits. This is array arithmetic, not peak process memory. Shared scratch,
different measured specializations and direct replay remain diagnostic limits.
No public factory, workspace contract, codec format or failure/publication
behavior changes. No compiler/sanitizer/full-suite/fuzz, hosted CI or external
qualification is newly claimed in this measurement-only stage.

Next evaluate a private complete encoder path with explicit workspace accounting,
exact output and limit/failure tests before public admission. The bounded replay
results justify that evaluation, not an assumed end-to-end speedup or automatic
default change. Keep all earlier negative observations and the retained binary.

In particular, nci lies between controls in two passes and is faster than both
in one; its full observed range is -1.36% to +0.09% with control spread up to
1.47%. Treat nci as near-flat, not a demonstrated gain. Reymont is faster than
both in all three passes, by 0.70% to 1.59% across both controls; this remains a
small observed margin rather than a statistical significance claim.


## BM-0182: Private bounded 18-bit complete encoder screening

Date: 2026-09-29. DD-1349 adds separate private workspace, raw-frame, streaming
and owning adapters to dedicated test/benchmark targets only. The ordinary
production libraries and public factory still select the retained 16-bit path.
Within the named private adapters indexed_five_prefix denotes 18-bit search;
other policies remain references. No stream representation or public enum changes.

Measure owner creation, process calls and destruction, including tokenization,
framing and entropy encoding. Exclude file I/O, sink-buffer copies, comparisons
and decoding. Input/output chunks are 65536 bytes, frames 1048576 bytes. Each
process captures a retained-16-bit stream and decodes it, then performs one
warmup and three rank-balanced measured iterations across slots 16/16/18.
Both controls use the same retained owner implementation; all owners allocate
their own queried storage and are destroyed before the next slot.

Run one process per manifest member, alternating direction by member index.
Retain all twelve processes; no tuning, fastest-run selection or concurrent
builds/tests. Hashes identify the validated binary and all retained earlier
artifacts. Take each slot's median of its three full-input iteration totals.
This is one screening pass, not replicated end-to-end admission evidence.

| Member | Control 0 seconds | Control 1 seconds | 18-bit seconds | Change vs control 0 | Change vs control 1 | Control spread |
|---|---:|---:|---:|---:|---:|---:|
| dickens | 1.833545 | 1.840269 | 1.765567 | -3.71% | -4.06% | 0.37% |
| mozilla | 8.319153 | 8.244925 | 8.215675 | -1.24% | -0.35% | 0.90% |
| mr | 2.609580 | 2.602553 | 2.593967 | -0.60% | -0.33% | 0.27% |
| nci | 2.994090 | 3.000178 | 3.026336 | +1.08% | +0.87% | 0.20% |
| ooffice | 0.826972 | 0.835055 | 0.787612 | -4.76% | -5.68% | 0.98% |
| osdb | 0.979814 | 0.979866 | 0.969302 | -1.07% | -1.08% | 0.01% |
| reymont | 1.899995 | 1.899810 | 1.874659 | -1.33% | -1.32% | 0.01% |
| samba | 2.240079 | 2.234573 | 2.202919 | -1.66% | -1.42% | 0.25% |
| sao | 1.311505 | 1.321272 | 1.274966 | -2.79% | -3.50% | 0.74% |
| webster | 6.982911 | 7.006503 | 6.809205 | -2.49% | -2.82% | 0.34% |
| xml | 0.337365 | 0.338329 | 0.335352 | -0.60% | -0.88% | 0.29% |
| x-ray | 1.290159 | 1.290284 | 1.188792 | -7.86% | -7.87% | 0.01% |

Sum of member medians: controls 31.625167 and 31.593616 seconds, candidate
31.044353 seconds; changes -1.84% and -1.74%. Negative means faster.
The candidate is faster than both controls in 11/12 processes and
slower than both in 1/12. Maximum control spread is
0.98%. Small differences within control variation are
not evidence of statistically significant improvement. Keep all input-dependent
regressions and near-ties when deciding whether a memory increase is worthwhile.

Full-frame policy charges are 79,435,701 bytes for
the retained owner and 80,222,133 bytes for the private
owner, including arrays and charged state. Finder arrays increase by 786,432
bytes. These are checked policy charges, not measured peak process memory.

All complete output streams match the retained 16-bit encoding byte-for-byte;
restoration succeeds. TVG-1215 covers compiler, sanitizer, allocation, budgets,
chunk boundaries and failed-frame publication. The independent-owner smoke
rejects accidental sharing of the candidate raw-frame symbol with the baseline.

Public selection remains unchanged. Repeat the preserved complete-encoder
diagnostic across the corpus before any admission decision, including small
margins and adverse cases. Retain existing memory limits and failure contracts;
no new hosted CI, external qualification or public speed guarantee is claimed.


## BM-0183: Replicated private 18-bit complete encoder evaluation

Date: 2026-09-29. DD-1350 repeats the unchanged DD-1349 complete-encoder
diagnostic over all twelve manifest members in three passes, retaining all
36 processes. Rotate member order by four per pass, reverse the middle pass
and alternate each member's execution direction. No builds, tests or tuning
overlap timing. Sources, executable and earlier artifact hashes are preserved.

Measure owner creation, process and destruction with independently queried
storage; exclude file I/O, sink copies, byte comparisons and decoding. Every
process compares one warmup and three balanced measured iterations of slots
16/16/18 against a retained-16-bit stream and verifies restoration. Use medians
of each slot's three whole-input totals and sum member medians per pass.
Negative percentage means less elapsed time.

| Pass | Control 0 seconds | Control 1 seconds | 18-bit seconds | Change vs control 0 | Change vs control 1 |
|---|---:|---:|---:|---:|---:|
| 1 | 31.846195 | 31.783876 | 31.406790 | -1.38% | -1.19% |
| 2 | 31.959678 | 31.968088 | 31.560994 | -1.25% | -1.27% |
| 3 | 31.337245 | 31.367873 | 30.812983 | -1.67% | -1.77% |

Per-member ranges include all three processes against both controls. They
are observations, not confidence intervals or guarantees.

| Member | 18-bit change range | Faster than both | Slower than both | Maximum control spread |
|---|---:|---:|---:|---:|
| dickens | -3.76% to -2.67% | 3/3 | 0/3 | 0.61% |
| mozilla | -0.85% to +0.23% | 2/3 | 0/3 | 0.30% |
| mr | -1.86% to -0.39% | 3/3 | 0/3 | 1.49% |
| nci | +0.01% to +2.56% | 0/3 | 3/3 | 1.16% |
| ooffice | -5.41% to +10.04% | 2/3 | 1/3 | 1.05% |
| osdb | -3.14% to -1.11% | 3/3 | 0/3 | 1.05% |
| reymont | -1.55% to -0.76% | 3/3 | 0/3 | 0.37% |
| samba | -2.66% to -1.24% | 3/3 | 0/3 | 1.47% |
| sao | -3.12% to -1.13% | 3/3 | 0/3 | 1.45% |
| webster | -2.85% to -1.21% | 3/3 | 0/3 | 0.91% |
| xml | -5.48% to -1.22% | 3/3 | 0/3 | 1.91% |
| x-ray | -7.85% to -7.02% | 3/3 | 0/3 | 0.49% |

Pass aggregate changes span -1.77% to -1.19% across
both controls. The candidate is faster than both in 31/36 processes, slower
than both in 4/36 and between controls in 1/36. Maximum
whole-input duplicate-control spread is 1.91%. Do not select a favorable
denominator or ignore input-specific regressions.

The first ooffice process reverses the earlier screening result: its candidate
iterations are 0.9959043, 0.9208670 and 0.8100319 seconds. The median is slower
than both control medians, although the fastest candidate iteration is not.
Control 0 also has an individual 1.0304212-second iteration. Retain these
observations without attributing them to allocator, scheduling or hardware
behavior that was not established by this experiment.

Full-frame owner policy charges remain 79,435,701 and 80,222,133 bytes; arrays
increase by 786,432 bytes. This is checked budget arithmetic, not measured peak
process memory. Each complete stream remains identical to the retained encoder
and restores the input. TVG-1216 checks metadata, rank coverage and identities.

Keep the public 16-bit default. The aggregate benefit does not remove the
input-specific regressions or variable small margins, so this result does not
justify unconditional admission of the larger workspace. Preserve the private
18-bit candidate and evidence for an explicit future speed/memory tradeoff.
Further work should answer a specific unresolved question (such as ooffice
variability), rather than simply repeating measurements until a favorable run.
No format, public memory limit, failure contract or failed-frame publication
behavior changes. No new compiler/sanitizer/full-suite/fuzz, hosted CI or
external qualification is claimed in this documentation-only measurement stage.


## BM-0184: Reusing validated 1 MiB operation plans

Date: 2026-09-30. After DD-1350 retains the 16-bit dictionary default, DD-1351
evaluates the repeated planning in frame-side operation generation. The existing
frame path calls plan, then model, which internally calls plan again. The private
prepared bridge calls the retained planner once and maps once with the same
field cursor. It does not skip token validation or change the operation grammar.

Use unchanged admitted five-prefix tokens. Exclude tokenization, reference-frame
encoding, validation comparisons and entropy writing from timed mapping calls.
Compare two identical non-inlined plan-plus-model controls and one prepared
bridge, with one warmup and three rank-balanced iterations per frame. Screen
all twelve manifest members once, alternating execution direction. Keep all
processes and iterations; no builds/tests overlap measurement.

Take each slot's median whole-input iteration total. Negative means faster.

| Member | Control 0 seconds | Control 1 seconds | Prepared seconds | Change vs control 0 | Change vs control 1 |
|---|---:|---:|---:|---:|---:|
| dickens | 0.192296 | 0.191410 | 0.126947 | -33.98% | -33.68% |
| mozilla | 1.248498 | 1.252236 | 0.825462 | -33.88% | -34.08% |
| mr | 0.209334 | 0.213105 | 0.137939 | -34.11% | -35.27% |
| nci | 0.144618 | 0.145045 | 0.095540 | -33.94% | -34.13% |
| ooffice | 0.191030 | 0.192275 | 0.125545 | -34.28% | -34.71% |
| osdb | 0.205640 | 0.206562 | 0.135405 | -34.15% | -34.45% |
| reymont | 0.091250 | 0.092103 | 0.061103 | -33.04% | -33.66% |
| samba | 0.312065 | 0.312901 | 0.208868 | -33.07% | -33.25% |
| sao | 0.336929 | 0.338763 | 0.222475 | -33.97% | -34.33% |
| webster | 0.608125 | 0.609939 | 0.405982 | -33.24% | -33.44% |
| xml | 0.033108 | 0.032733 | 0.021605 | -34.74% | -34.00% |
| x-ray | 0.353642 | 0.352951 | 0.232856 | -34.15% | -34.03% |

Sums of member medians are 3.926537, 3.940021 and 2.599727 seconds; prepared
mapping changes -33.79% and -34.02% against the two controls. It is faster
than both controls for 12/12 members. Maximum whole-input control
spread is 1.80%. These are mapping-only screening observations,
not complete-encoder gains or confidence intervals.

The prepared owner occupies 104 bytes in the measured build; the
implementation charges sizeof(owner), not a hard-coded architecture size.
It needs no extra token/operation arrays and performs no dynamic allocation.
The retained planner's token, active-operation and cursor charge is increased
explicitly for the owner. Public workspace requirements are unchanged.

Every mapped operation matches the reference. A representative entropy payload
also matches and the reference frame restores the input. Input/token/operation/
frame counts match retained BM-0160 records. All ranks, timing sums and source/
binary identities pass. TVG-1217 covers compiler/sanitizer and failure contracts.

Keep this bridge private. Next connect it to a separate frame-encoder trial and
prove exact frame bytes, preflight ordering, serialized-output preservation and
failed-frame non-publication before measuring complete-encoder impact. Account
for its transient owner state alongside existing model-state charges; do not
assume it is free or translate this mapping speedup directly to whole-pipeline
throughput. No public/default change or new external qualification is claimed.


## BM-0185: Prepared mapping in a private complete encoder

DD-1352 integrates the prepared operation bridge into distinct diagnostic-only
frame, raw-frame, streaming and owning adapters. Keep the admitted 16-bit
five-prefix dictionary and unchanged entropy coder, workspace policy and decoder.

Screen all twelve verified members once, alternating execution direction. Each
process creates two identical retained owners and one trial owner independently,
with one warmup and three rank-balanced measured complete-input iterations.
Measure owner creation, process calls and destruction; exclude file I/O, sink
storage/comparisons and decoding. Use 1 MiB frames and 64 KiB input/output chunks.
Keep every result. Builds and validation do not overlap performance measurement.

Each cell is the median of three complete-input times; negative is faster.

| Member | Control 0 seconds | Control 1 seconds | Prepared seconds | Change vs control 0 | Change vs control 1 |
|---|---:|---:|---:|---:|---:|
| dickens | 1.862400 | 1.862966 | 1.776609 | -4.61% | -4.64% |
| mozilla | 8.443692 | 8.330253 | 7.948727 | -5.86% | -4.58% |
| mr | 2.694945 | 2.683419 | 2.670832 | -0.89% | -0.47% |
| nci | 3.088469 | 3.083361 | 3.063911 | -0.80% | -0.63% |
| ooffice | 0.840122 | 0.849641 | 0.778594 | -7.32% | -8.36% |
| osdb | 1.034895 | 1.031494 | 0.968281 | -6.44% | -6.13% |
| reymont | 1.951350 | 1.957476 | 1.914592 | -1.88% | -2.19% |
| samba | 2.294418 | 2.294052 | 2.162063 | -5.77% | -5.75% |
| sao | 1.329051 | 1.308574 | 1.200520 | -9.67% | -8.26% |
| webster | 7.055553 | 7.075547 | 6.841415 | -3.04% | -3.31% |
| xml | 0.341558 | 0.347863 | 0.329352 | -3.57% | -5.32% |
| x-ray | 1.318859 | 1.295051 | 1.195584 | -9.35% | -7.68% |

Sums of member medians are 32.255312,
32.119698 and 30.850480 seconds.
Changes against the two controls are -4.36% and
-3.95%; the trial is faster than both for
12/12 members and slower than both for 0/12.
Maximum duplicate-control spread is 1.85%.
These are complete-encoder screening observations, not confidence intervals,
replicated admission evidence or a prediction from mapping-only BM-0184.

Both owners require 79435701 policy bytes for the full-frame configuration.
No extra arrays are introduced. The prepared owner and field cursor fit within
the existing logical model-state charge and die before range preparation; this
is bounded workspace accounting, not a measured peak resident-memory claim.

Every measured and warmup stream matches the retained output byte-for-byte;
the unchanged decoder restores it. Input/archive sizes and chunk/workspace
metadata match retained complete-stream reports. Corpus identity, balanced ranks,
finite times and inherited source/executable hashes pass. TVG-1218 records
compiler, sanitizer, error parity and publication validation.

Keep the candidate private. Replicate this complete-encoder comparison across
all twelve members before deciding admission. Public defaults, format IDs,
workspace requirements and the previous external qualification remain unchanged.


## BM-0186: Replicated prepared-mapping complete-encoder measurements

DD-1353 repeats the unchanged BM-0185 binary over all twelve verified members
in three passes. Rotate member order by four per pass, reverse the middle pass,
and alternate per-member execution direction with pass parity. Eighteen of the
36 processes run in each direction. Keep all observations, without selective
reruns, parameter tuning or concurrent builds/tests.

Each process has two identical retained owners and one private prepared-mapping
owner. One warmup precedes three rank-balanced complete-input iterations; each
slot's median is its reported time. Measure owner creation, processing and
destruction, excluding file I/O, sink storage/comparisons and decoding. Frames
are 1 MiB; input/output chunks are 64 KiB. Negative changes mean shorter time.

| Pass | Control 0 sum seconds | Control 1 sum seconds | Prepared sum seconds | Change vs control 0 | Change vs control 1 |
|---|---:|---:|---:|---:|---:|
| 1 | 31.844590 | 31.841138 | 30.658356 | -3.73% | -3.71% |
| 2 | 31.617721 | 31.615135 | 30.304904 | -4.15% | -4.14% |
| 3 | 32.020010 | 33.159835 | 30.278489 | -5.44% | -8.69% |

These sums combine per-member medians, not a concatenated-input throughput test.

| Member | Change range across three passes and both controls | Faster than both | Maximum control spread |
|---|---:|---:|---:|
| dickens | -4.40% to -2.87% | 3/3 | 0.53% |
| mozilla | -5.37% to -3.10% | 3/3 | 0.17% |
| mr | -2.99% to -2.24% | 3/3 | 0.59% |
| nci | -1.76% to -1.49% | 3/3 | 0.17% |
| ooffice | -7.98% to -7.59% | 3/3 | 0.39% |
| osdb | -8.05% to -6.59% | 3/3 | 1.24% |
| reymont | -1.95% to -0.86% | 3/3 | 0.26% |
| samba | -5.18% to -4.17% | 3/3 | 0.75% |
| sao | -9.61% to -8.79% | 3/3 | 0.87% |
| webster | -20.07% to -2.90% | 3/3 | 15.16% |
| xml | -4.51% to -1.06% | 3/3 | 0.50% |
| x-ray | -9.67% to -8.96% | 3/3 | 0.58% |

The trial is faster than both controls for all 36 process medians. Passes one
and two show aggregate changes from -3.71% to -4.15%. Pass three is affected by
substantial within-process variation on webster: its control medians are
7.396356 and 8.517888 seconds, a 15.16% spread, versus trial median 6.808134.
Individual control iterations reach 11.791724 and 11.983207 seconds, and a trial
iteration reaches 10.750528 seconds. No cause is established. Preserve this
process and its rank/order data; neither its -20.07% comparison nor the third
pass's aggregate range establishes a stable improvement of that magnitude.
Do not discard or selectively rerun it, or interpret these samples as confidence
intervals. The first two passes, earlier BM-0185 screen and all-member direction
of change support progressing the integration, with a modest full-encoder gain.

All 36 streams match byte-for-byte and restore through the unchanged decoder.
Archive/input/chunk metadata agree with BM-0185. Both owners require 79,435,701
policy bytes, with zero additional arrays; this is unchanged logical workspace
accounting, not a new physical peak-memory measurement. Source/executable hashes,
manifest, ranks and positive finite times are checked. TVG-1219 records scope.

Proceed to a separate public-path integration and regression-validation step,
retaining the old frame oracle and tight-budget fallback. No public factory,
format, default, workspace contract or external qualification changes here.


## BM-0187: Public-path integration of the measured prepared mapper

DD-1354 selects the prepared streaming encoder in the public 1 MiB factory after
BM-0185/BM-0186 screening and replication. TVG-1220 validates that integration
with both compiler suites, sanitizer/public failure tests and exact twelve-member
CLI comparisons against the retained encoder.

No new throughput measurement is performed in this integration stage. The prior
replication's first two aggregate comparisons show reductions of 3.71% to 4.15%;
retain the documented third-pass variation and do not reinterpret its larger
observed reduction as stable. Public factory/CLI overhead is not separately
benchmarked here. The admitted implementation retains the same bounded workspace
requirements and introduces no additional token/operation arrays. Object size
and alignment are checked against the reference; no new physical peak-memory
measurement is claimed.

The old frame/raw-frame/streaming/owning path remains an independent oracle and
supplies the tight-budget fallback. The admitted 16-bit dictionary, wire format,
C ABI and failed-frame publication contract are unchanged. Hosted CI and external
platform qualification must refer to the eventual integration revision separately.


## BM-0188: Phase diagnosis of the admitted prepared-mapping encoder

DD-1355 preserves the old phase diagnostic and adds a separate target using the
admitted five-prefix finder and prepared frame/model functions. Screen all twelve
verified corpus members in manifest order, one process each, with one warmup and
three repetitions per frame. Each value is the median of whole-input iteration
phase totals. No builds or tests overlap timing and no samples are discarded.

| Member | Tokenize seconds | Complete frame seconds | Replay model seconds | Replay range preparation seconds | Replay range write seconds |
|---|---:|---:|---:|---:|---:|
| dickens | 1.284175 | 0.451744 | 0.127919 | 0.160840 | 0.161268 |
| mozilla | 5.427684 | 2.323050 | 0.807331 | 0.752911 | 0.755365 |
| mr | 2.053260 | 0.457616 | 0.136924 | 0.159700 | 0.161506 |
| nci | 2.619004 | 0.342831 | 0.095687 | 0.122075 | 0.122909 |
| ooffice | 0.356969 | 0.392054 | 0.124930 | 0.131984 | 0.131820 |
| osdb | 0.446498 | 0.435694 | 0.134940 | 0.149705 | 0.150548 |
| reymont | 1.641229 | 0.214219 | 0.060919 | 0.076846 | 0.076405 |
| samba | 1.488785 | 0.623468 | 0.201940 | 0.207941 | 0.207474 |
| sao | 0.530399 | 0.623871 | 0.218591 | 0.201107 | 0.201943 |
| webster | 5.276421 | 1.427562 | 0.401859 | 0.506179 | 0.506078 |
| xml | 0.241898 | 0.074546 | 0.021466 | 0.026010 | 0.025668 |
| x-ray | 0.380725 | 0.756857 | 0.230626 | 0.259419 | 0.259175 |

Summed tokenize/frame medians are 21.747048/8.123511 seconds,
representing 72.80%/27.20% of their split-path sum.
This excludes whole-owner construction/destruction and stream I/O; it is not a
whole-CLI throughput profile. Separate replay sums are 2.563133
seconds for mapping, 2.754718 for range preparation and
2.760157 for range writing. Replays overlap complete-frame work and
must not be added to it or treated as exact additive subdivisions. Do not divide
these measurements by earlier separately built timings to claim a new speedup.

Every encoded frame equals the retained raw-frame reference and restores exactly.
Replayed Range payloads match. Input/token/operation/frame counts agree with the
retained phase reports; source/executable identities and finite positive timing
samples pass. Both compiler smoke runs cover a multi-frame input and empty-input
rejection. No new public implementation, memory policy or format change occurs.

Dictionary search remains the largest aggregate cost, but the two Range passes
provide a distinct next opportunity after prior dictionary and cumulative-query
trials. DD-1355 selects a bounded scratch-lifetime investigation before another
production change. The measured times do not establish its eligibility or gain.


## BM-0189: Untimed eligibility of finder-scratch Range encoding

DD-1356 adds private frame/raw-frame prototypes that reuse expired finder byte
storage for Range payloads. This diagnostic records actual successful scratch
path selection, reference frame identity and unchanged-decoder restoration.
It does not measure throughput or imply a speedup.

| Member | Frames | Scratch frames | Maximum checked payload bound, bytes |
|---|---:|---:|---:|
| dickens | 10 | 10 | 5068351 |
| mozilla | 49 | 49 | 5640041 |
| mr | 10 | 10 | 5226379 |
| nci | 32 | 32 | 1400251 |
| ooffice | 6 | 6 | 6492475 |
| osdb | 10 | 10 | 3954227 |
| reymont | 7 | 7 | 3687447 |
| samba | 21 | 21 | 3985157 |
| sao | 7 | 7 | 6507815 |
| webster | 40 | 40 | 4241253 |
| xml | 6 | 6 | 2192753 |
| x-ray | 9 | 9 | 9318125 |

All 207 frames across twelve verified members use the scratch path;
all 211938580 input bytes are covered. The full-frame finder capacity
is 13,369,344 bytes and the largest checked payload bound is
9,318,125 bytes. The eligibility check also enforces model,
compressed-payload and simultaneous workspace limits and disjointness. These
results describe this corpus/configuration, not every possible frame or limit.

Every frame matches the admitted prepared raw-frame reference and restores
exactly. Aggregate input/token/operation/frame counts match the retained phase
records. The final conservative capacity-accounting version was checked over
all twelve members; initial implementation evidence is retained separately.

The prototype allocates no additional arrays and performs no dynamic allocation
in repeated raw-frame processing. It borrows already charged finder storage;
this is not a physical peak-memory measurement. TVG-1223 covers exact/short
budgets, fallbacks, malformed input, aliases, failed output and reinitialization.
Next integrate the prototype into a separate private stream/owner diagnostic
and measure complete-encoder cost against two identical admitted prepared owners.
No public factory, format or workspace requirement changes here.


## BM-0190: Finder-scratch complete-encoder screening

DD-1357 compares two identical admitted prepared owners with a separately named
private finder-scratch owner. All use the five-prefix search, 1 MiB frames and
64 KiB input/output chunks. Each member has one warmup and three rank-balanced
measured iterations; member directions alternate. Values below are medians in
seconds. Time includes owner creation, process and destruction, excluding file
I/O, sink storage, byte comparisons and decoding. No builds/tests run alongside
timing. This is one twelve-member screen, not an independent repeated campaign.

| Member | Prepared 0 | Prepared 1 | Finder scratch | Change vs 0 | Change vs 1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| dickens | 1.756596 | 1.749567 | 1.592542 | -9.34% | -8.98% |
| mozilla | 7.757927 | 7.758323 | 7.018227 | -9.53% | -9.54% |
| mr | 2.521835 | 2.513461 | 2.353029 | -6.69% | -6.38% |
| nci | 2.950349 | 2.947383 | 2.823239 | -4.31% | -4.21% |
| ooffice | 0.760634 | 0.756128 | 0.629814 | -17.20% | -16.71% |
| osdb | 0.934429 | 0.905187 | 0.758507 | -18.83% | -16.20% |
| reymont | 1.865899 | 1.864370 | 1.788477 | -4.15% | -4.07% |
| samba | 2.104646 | 2.103714 | 1.904456 | -9.51% | -9.47% |
| sao | 1.173324 | 1.167769 | 0.971606 | -17.19% | -16.80% |
| webster | 6.683715 | 6.741744 | 6.191768 | -7.36% | -8.16% |
| xml | 0.326367 | 0.325197 | 0.302208 | -7.40% | -7.07% |
| x-ray | 1.153023 | 1.157239 | 0.900949 | -21.86% | -22.15% |
| Sum of member medians | 29.988745 | 29.990082 | 27.234821 | -9.18% | -9.19% |

The trial beats both controls on 12/12 members and loses to both on 0/12.
Maximum duplicate-control median spread is 3.23%. Retain every
member and iteration; no selective rerun or absolute comparison to older builds.

Every warmup and timed stream equals the captured prepared baseline, which the
unchanged decoder restores exactly. Input and archive sizes agree with retained
whole-stream reports. Both policy requirements are 79,435,701 bytes, with zero
additional array bytes. This is workspace accounting, not measured physical peak
memory. No format, compression-ratio or decoder change is introduced.

TVG-1224 validates streaming failure privacy and allocation behavior. The result
supports a separately ordered replicated complete-encoder comparison before
public admission; it does not extend hosted CI or external qualification to this
private trial. Preserve old executables and all timing/identity records.


## BM-0191: Replicated finder-scratch complete-encoder results

Repeat BM-0190 using the unchanged executable, admitted prepared controls and
private finder-scratch owner. Three complete twelve-member passes rotate order
by four, reverse the middle pass and alternate member execution direction.
All 36 processes are retained: eighteen forward and eighteen reverse. Each uses
one warmup and three rank-balanced measured iterations. Creation, processing and
destruction are timed; file I/O, sink storage, comparison and decode are excluded.
No builds/tests run alongside timing and no observations are selectively rerun.

| Pass | Prepared 0 sum (s) | Prepared 1 sum (s) | Trial sum (s) | Change vs 0 | Change vs 1 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 30.025688 | 29.927163 | 27.248906 | -9.25% | -8.95% |
| 2 | 29.923587 | 29.888612 | 27.229601 | -9.00% | -8.90% |
| 3 | 29.897798 | 29.944164 | 27.250673 | -8.85% | -9.00% |

Each sum adds per-member medians, not pooled iterations.

| Member | Change range across three passes and both controls | Wins against both | Maximum control spread |
| --- | ---: | ---: | ---: |
| dickens | -9.47% to -8.87% | 3/3 | 0.67% |
| mozilla | -9.59% to -8.85% | 3/3 | 0.63% |
| mr | -7.11% to -6.05% | 3/3 | 0.99% |
| nci | -4.35% to -3.90% | 3/3 | 0.28% |
| ooffice | -17.09% to -16.06% | 3/3 | 0.51% |
| osdb | -17.35% to -15.58% | 3/3 | 0.95% |
| reymont | -4.19% to -3.01% | 3/3 | 0.12% |
| samba | -9.50% to -9.00% | 3/3 | 0.10% |
| sao | -17.14% to -16.42% | 3/3 | 0.37% |
| webster | -8.85% to -7.40% | 3/3 | 1.04% |
| xml | -9.17% to -6.87% | 3/3 | 1.51% |
| x-ray | -22.61% to -21.63% | 3/3 | 0.25% |

The trial wins against both controls in 36/36 processes, loses against
both in 0/36 and lies between them in 0/36. Maximum duplicate-control
median spread is 1.51%. No isolated timing cause is inferred.

All 36 reports pass byte identity and restoration. Every warmup and measured
output matches the captured admitted prepared archive; the unchanged decoder
restores it. Input/archive sizes and all stable report metadata match BM-0190.
Requirements remain 79,435,701 bytes on both paths with zero additional arrays;
this is policy accounting, not a physical peak-memory measurement. All current
and inherited source/executable identity checks pass.

These are repetitions with one unchanged build and benchmark environment, not
cross-platform timing qualification. Earlier compiler, sanitizer, allocation
and failure-publication tests remain TVG-1224 evidence, not newly repeated tests.
The public factory and format remain unchanged in this measurement-only step.


The three aggregate reductions range from 8.85% to 9.25%, consistent with the
initial screen. Every member wins against both controls on all three passes.
This supports DD-1358's next public-integration validation step, while retaining
the prepared reference/fallback and requiring public-boundary validation before
admission. No claim of identical speedup on other platforms is made.


## BM-0192: Public integration of the replicated finder-scratch path

DD-1359 selects the finder-scratch stream for the public 1 MiB position-distance
encoder. The measured codec bodies remain those of BM-0190/BM-0191; only public
factory selection, library integration and explanatory comments change.

BM-0191 supplies the performance evidence: three complete twelve-member passes
reduce summed member medians by 8.85% to 9.25%, with all 36 trials beating both
admitted prepared controls. This integration step does not introduce a new public
CLI timing claim or physical peak-memory measurement. Concrete stream size and
alignment remain equal; workspace formulas and the five-prefix policy are
unchanged, and prepared encoding remains the fallback/reference.

TVG-1226 passes both full 3,957-test suites, 52 sanitizer cases and two selected
optional diagnostic smokes. All twelve old prepared/new public CLI archive
comparisons and both new decoder restorations pass. Existing format, output
bytes, workspace contracts and failed-frame publication rules are preserved.
The original private owning adapter remains diagnostic-only.

This records local integration readiness. Hosted CI and four-route external
bundle verification must be recorded separately for the resulting revision.

## BM-0193: Initial four-MiB complete-profile reference screen

DD-1367 measures twelve manifest-verified Silesia members containing
211,938,580 raw bytes. Compare the private four-MiB reference owner, qualified
one-MiB public position-distance profile and existing public contextual four-MiB
profile. Complete archives include stream headers and every frame prefix.

All profiles use known sizes and 65,536-byte input/output chunks. Capture and
restore complete archives with both compiler targets before measurement, then
freeze their bytes and executable identities. TVG-1234 checks 72 complete streams,
cross-compiler identity, retained reference frames and qualified one-MiB archives.
The targeted smoke also passes on both compiler targets and with ASan/UBSan.

Use one unchanged diagnostic executable for timing. Each member, profile
and direction runs once in a fresh process, with profile order rotated/reversed
across members. Time allocation, creation, process calls and destruction. Exclude
file I/O, sink comparison and formatting. The private owner and public C interface
adapters differ; creation includes their respective query/allocation paths.
All 72 measured processes verify every produced byte. No build or unrelated test
runs alongside timing. This is an initial screen, not replicated speed admission.

| Profile | Complete archive bytes | Archive/raw | Encode sum (s) | Encode MiB/s | Decode sum (s) | Decode MiB/s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 4 MiB position-distance reference | 61,643,620 | 29.086% | 534.153934 | 0.378 | 6.954367 | 29.064 |
| 1 MiB position-distance public | 63,558,293 | 29.989% | 28.878322 | 6.999 | 5.293731 | 38.181 |
| 4 MiB contextual public | 61,992,826 | 29.250% | 150.028456 | 1.347 | 6.628007 | 30.495 |

Throughput divides total raw MiB by the sum of the twelve observed codec times;
these sums are not medians or averages of per-member percentage changes.

Against 1 MiB position-distance public, the reference archive total changes by -3.012%;
it is smaller on 11/12 members, larger on 1/12 and equal on 0/12.
Against 4 MiB contextual public, the reference archive total changes by -0.563%;
it is smaller on 5/12 members, larger on 7/12 and equal on 0/12.

| Member | 4 MiB reference bytes | 1 MiB public bytes | 4 MiB contextual bytes |
| --- | ---: | ---: | ---: |
| dickens | 3,258,714 | 3,472,780 | 3,243,966 |
| mozilla | 18,192,069 | 18,241,669 | 18,792,234 |
| mr | 3,318,766 | 3,403,468 | 3,348,820 |
| nci | 2,513,746 | 2,825,695 | 2,461,724 |
| ooffice | 3,010,968 | 3,037,421 | 3,004,480 |
| osdb | 3,452,162 | 3,544,894 | 3,262,734 |
| reymont | 1,572,002 | 1,691,028 | 1,575,971 |
| samba | 4,826,491 | 5,049,664 | 4,799,306 |
| sao | 5,185,835 | 5,146,691 | 5,251,920 |
| webster | 10,395,877 | 11,109,357 | 10,334,799 |
| xml | 553,321 | 585,045 | 545,175 |
| x-ray | 5,363,669 | 5,450,581 | 5,371,697 |

| Member | Reference encode/decode (s) | 1 MiB encode/decode (s) | Contextual encode/decode (s) |
| --- | ---: | ---: | ---: |
| dickens | 83.797058 / 0.388985 | 1.632569 / 0.280628 | 7.919067 / 0.252153 |
| mozilla | 81.142716 / 2.008224 | 7.697472 / 1.555552 | 35.488432 / 2.288852 |
| mr | 40.173847 / 0.392401 | 2.464047 / 0.291414 | 21.734409 / 0.297207 |
| nci | 38.978078 / 0.270327 | 2.857202 / 0.226914 | 20.005869 / 0.199439 |
| ooffice | 8.486152 / 0.337141 | 0.745958 / 0.253249 | 1.396775 / 0.373223 |
| osdb | 7.317705 / 0.382697 | 0.784358 / 0.286656 | 1.643555 / 0.365787 |
| reymont | 62.666415 / 0.178018 | 1.835842 / 0.136576 | 11.367713 / 0.122515 |
| samba | 22.398501 / 0.522935 | 2.189133 / 0.410380 | 6.697001 / 0.522866 |
| sao | 18.323696 / 0.600531 | 1.004286 / 0.427068 | 2.300492 / 0.766484 |
| webster | 163.112003 / 1.142022 | 6.250369 / 0.884084 | 39.797786 / 0.823122 |
| xml | 1.699412 / 0.068316 | 0.314314 / 0.049454 | 0.620143 / 0.049778 |
| x-ray | 6.058352 / 0.662771 | 1.102774 / 0.491755 | 1.057214 / 0.566581 |

Query storage and measured process peaks are separate observations. The minimum
query budget is the smallest configured internal-buffer budget the workspace
query accepts; it is a policy threshold, not allocated bytes or codec RSS.
Each profile retains its own query accounting rules. Queried buffer storage
adds primary, secondary and views query sizes. Peak working set
and peak commit are whole-process lifetime counters after codec destruction;
input, frozen expected archive and runtime overhead contribute. Peak commit is
the process pagefile-usage counter, not measured disk traffic. Report maxima
across members, without subtracting unlike baselines or inferring codec-only RSS.

| Profile / direction | Minimum query budget (bytes) | Queried buffers (bytes) | Maximum process working set (MiB) | Maximum process commit (MiB) |
| --- | ---: | ---: | ---: | ---: |
| 4 MiB position-distance reference / encode | 281,286,669 | 281,280,597 | 270.844 | 337.977 |
| 4 MiB position-distance reference / decode | 130,029,573 | 130,023,509 | 126.609 | 193.473 |
| 1 MiB position-distance public / encode | 79,435,637 | 79,429,717 | 130.109 | 145.141 |
| 1 MiB position-distance public / decode | 32,511,893 | 32,505,941 | 85.340 | 100.293 |
| 4 MiB contextual public / encode | 266,338,389 | 266,338,389 | 232.133 | 324.266 |
| 4 MiB contextual public / decode | 113,246,293 | 113,246,293 | 115.359 | 178.008 |

The reference is intentionally unoptimized: nearest-first single-prefix matching
and scalar Range coding retain clear differential oracles. The public one-MiB
profile already uses replicated finder/model/scratch optimizations. Contextual
four-MiB uses a different grammar/model and minimum match length. Frame, window,
finder, model and adapter differences prevent attributing these totals to window
size alone. No individual phase bottleneck is inferred from complete timings.

Preserve this baseline. Next, measure reference selection versus frame coding
before transferring the retained exact finder optimizations, with selected-token
and complete-byte differentials. Any speed admission needs repeated trials;
public selection, full-suite/fuzz validation and external exchange qualification
remain later stages. No existing public codec or format changes in this screen.

## BM-0194: Four-MiB reference selection and frame-coding phases

DD-1368 measures the unchanged reference candidate tokenizer and selected-token
frame encoder in disjoint intervals. Use the twelve maintained Silesia members,
211,938,580 raw bytes and 57 frames. Frozen DD-1367 complete archives
occupy 61,643,620 bytes, including twelve 112-byte stream headers.
Both compiler builds first verify the split path against those archives and
restore every frame; TVG-1235 records targeted smoke and sanitizer validation.

Measure one fresh process per member with one observation per frame, after
untimed verification. Query and partition buffers once, reusing them across
frames. Allocation/initialization, file I/O, stream-header validation, byte
comparison, scratch decoding/reconstruction and report formatting are excluded
from both timers. Each measured frame must match the frozen bytes and restore
the source before success. No build or unrelated test runs alongside timing.

Selection includes candidate validation, finder reset/search and token
materialization. Frame coding includes token planning/modeling, scalar Range
planning/encoding and prefix construction. The phases do not overlap and may be
added; these are not replay timings nested inside another measured interval.
They are not a complete streaming/owning-transform timing or pure search counter.

| Phase | Sum (s) | Share of measured phase sum |
| --- | ---: | ---: |
| Reference token selection | 319.183101 | 96.317% |
| Selected-token frame coding | 12.205703 | 3.683% |
| Additive phase sum | 331.388804 | 100% |

The reports contain 28,533,369 selected tokens and 110,846,666
modeled operations. Aggregate shares divide summed phase times, rather than
averaging per-member percentages. All twelve measured processes / 57 frames
pass complete-byte identity and raw restoration. No observations are discarded
or selectively rerun. Per-member results follow.

| Member | Frames | Tokens | Operations | Selection (s) | Coding (s) | Selection share |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| dickens | 3 | 1,115,635 | 5,447,454 | 29.887721 | 0.658089 | 97.846% |
| mozilla | 13 | 9,916,815 | 35,286,329 | 70.431741 | 3.607787 | 95.127% |
| mr | 3 | 1,285,352 | 5,972,613 | 33.681323 | 0.687610 | 97.999% |
| nci | 8 | 786,563 | 3,825,152 | 30.520195 | 0.481817 | 98.446% |
| ooffice | 2 | 1,390,454 | 5,508,315 | 6.314471 | 0.612938 | 91.152% |
| osdb | 3 | 1,562,268 | 5,665,836 | 4.380913 | 0.669952 | 86.736% |
| reymont | 2 | 542,149 | 2,547,375 | 21.907217 | 0.308287 | 98.612% |
| samba | 6 | 2,521,184 | 8,743,452 | 18.505286 | 0.902244 | 95.351% |
| sao | 2 | 3,363,927 | 9,655,100 | 7.383891 | 0.972198 | 88.365% |
| webster | 10 | 3,515,622 | 17,026,350 | 93.255400 | 2.041806 | 97.857% |
| xml | 2 | 213,518 | 955,318 | 1.291469 | 0.108743 | 92.234% |
| x-ray | 3 | 2,319,882 | 10,213,372 | 1.623475 | 1.154233 | 58.447% |

The retained encode query, with zero streaming-object charge for this split
diagnostic, admits 281,286,093 bytes and includes 17,039,360
finder bytes. This is workspace policy accounting, not measured physical memory.
Input and frozen-archive storage are outside that codec workspace charge.
No new peak-memory claim is made; BM-0193 reports complete-profile memory counters.

This initial phase screen identifies token selection as the larger measured
reference phase. It does not distinguish search from reset/materialization or
qualify an optimization speedup. Timing boundaries and execution order differ
from BM-0193; subtracting those totals would not isolate stream overhead.
The cause of the absolute timing difference between those screens has not been
isolated. Future reference/trial comparisons must use the same measurement path.

Prioritize exact finder transfer next, retaining the clear single-prefix and
exhaustive oracles. A five-prefix trial must preserve nearest ties, indexing
inside matches, selected tokens, complete frame bytes, checked budgets/aliases
and failure-publication contracts. Qualify any speed gain through exclusive
repeated trials after correctness. Public four-MiB admission and external
qualification remain separate stages.

## BM-0195: Repeated four-MiB exact five-prefix phase comparison

DD-1370 uses identical candidate-then-scalar-frame paths for two duplicate
single-prefix references and one exact five-prefix trial, all with eligibility
three. Three complete twelve-input passes rotate process order by member and
pass. Each of 108 fresh processes runs exclusively, with one observation per
frame and no warmup or discarded observations. Timing excludes allocation,
I/O, oracle decoding, token/frame comparisons and reporting. Candidate time
includes validation/reset/search/materialization; additive phase totals are
neither pure search nor whole-stream owner throughput.

Both compiler verification runs and all 513 measured frames preserve selected
tokens, frozen archive bytes and restoration. Per pass and finder the corpus is
211,938,580 raw bytes, 61,643,620 archive bytes, 57 frames, 28,533,369 tokens and
110,846,666 modeled operations. Compression ratio remains 29.085606% of raw size.
Measurements use one frozen timing build; the second compiler verifies bytes.

| Pass | Reference 0 selection/coding/total (s) | Reference 1 selection/coding/total (s) | Trial selection/coding/total (s) | Total reduction vs control mean | Total control spread |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 318.218049/12.288496/330.506545 | 319.303258/12.249420/331.552678 | 74.080295/12.221833/86.302128 | 73.929% | 0.317% |
| 2 | 318.593215/12.246264/330.839479 | 318.127592/12.205778/330.333370 | 74.032062/12.200520/86.232582 | 73.915% | 0.153% |
| 3 | 318.590523/12.180406/330.770929 | 320.277720/12.245892/332.523612 | 74.776582/12.233235/87.009817 | 73.764% | 0.530% |

Per-input totals below are the arithmetic means of all three passes, retaining
every observation. Reduction compares the trial with the mean of both reference
controls; negative values are regressions. Per-pass comparisons remain recorded.

| Member | Reference 0 total (s) | Reference 1 total (s) | Trial total (s) | Total reduction |
| --- | ---: | ---: | ---: | ---: |
| dickens | 30.536785 | 30.590718 | 5.082022 | 83.372% |
| mozilla | 74.197213 | 74.664294 | 20.987701 | 71.802% |
| mr | 34.371664 | 34.335052 | 8.121976 | 76.358% |
| nci | 30.297750 | 30.442361 | 9.755735 | 67.877% |
| ooffice | 6.857983 | 6.858134 | 1.589578 | 76.822% |
| osdb | 5.008854 | 4.996854 | 2.001710 | 59.989% |
| reymont | 22.051568 | 22.069457 | 5.846245 | 73.499% |
| samba | 19.449576 | 19.385142 | 4.769447 | 75.437% |
| sao | 8.375852 | 8.379162 | 2.734286 | 67.362% |
| webster | 95.370367 | 95.561770 | 22.697534 | 76.225% |
| xml | 1.397720 | 1.403477 | 0.590697 | 57.825% |
| x-ray | 2.790321 | 2.783466 | 2.337910 | 16.111% |

Across 36 member/pass comparisons, the trial is faster than both controls in 36, slower than both in 0, and between them in 0. The maximum per-member/pass total control spread is 1.364%.

Full-frame finder storage rises from 17,039,360 to 51,118,080 bytes; zero-owner
codec policy accounting rises from 281,286,093 to 315,364,813 bytes. The additional
34,078,720 bytes are a deterministic bound, not measured peak RSS. Both modes
also allocate 50,331,648 oracle token bytes plus input/archive buffers outside
that codec charge. No complete-owner or new process-memory gain is claimed.

These same-path observations support assessing the private selection trial.
Do not compare totals with BM-0193/0194 to infer another speedup or stream
overhead; their absolute timing disparity remains unisolated. Private owner
integration requires bounded-storage/failure-publication tests and subsequent
complete-stream timing. Public profile admission and external qualification
remain separate.

All 36 input/pass comparisons beat both controls. Additive corpus phase time
falls by 73.764% to 73.929% against the control mean, while
maximum input/pass control spread is 1.364%.
This repeated phase evidence supports private owner integration next, retaining
the original reference and bounded failure/publication tests. Complete-owner
throughput and physical-memory effects must be measured after integration;
this phase result does not establish those gains or public admission.

## BM-0196: Repeated native four-MiB five-prefix owner comparison

DD-1371 uses one common native Transform diagnostic for two duplicate reference
owners and one private five-prefix owner. All use eligibility three, four-MiB
frames, known original size and 65,536-byte input/output chunks. Three complete
twelve-input passes rotate encode order by input and pass. After each encode
triple, one fresh process measures the unchanged shared decoder. All 144 exclusive
processes are retained: 108 encode and 36 decode, no warmup or discarded reruns.

Encode/decode times include the owner creation query, allocation/initialization,
all process calls and destruction. Format configuration, diagnostic query/budget
search, I/O, sink comparison/capture and report formatting are outside clocks.
Every encoded byte matches the frozen complete archive, and every decoded byte
matches the original input. Each corpus observation covers raw 211,938,580 bytes,
archive 61,643,620 bytes and 57 frames; ratio remains 29.085606%. Both compiler
verification paths agree; one frozen build supplies the measurements.

| Pass | Reference 0 encode (s) | Reference 1 encode (s) | Trial encode (s) | Reduction vs control mean | Control spread | Shared decode (s) |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 502.899270 | 508.404386 | 108.331560 | 78.576% | 1.095% | 6.954504 |
| 2 | 457.950879 | 469.415062 | 111.729770 | 75.904% | 2.503% | 6.932932 |
| 3 | 397.976922 | 390.663994 | 87.850007 | 77.721% | 1.872% | 6.931407 |

| Member | Reference 0 mean encode (s) | Reference 1 mean encode (s) | Trial mean encode (s) | Reduction vs control mean |
| --- | ---: | ---: | ---: | ---: |
| dickens | 46.601838 | 44.919997 | 6.843602 | 85.045% |
| mozilla | 89.691873 | 88.826559 | 23.896950 | 73.227% |
| mr | 49.654960 | 48.464771 | 8.931919 | 81.794% |
| nci | 37.459594 | 38.542895 | 11.254593 | 70.384% |
| ooffice | 8.311668 | 7.720829 | 1.880784 | 76.538% |
| osdb | 6.664630 | 5.989862 | 2.661616 | 57.934% |
| reymont | 32.733579 | 27.895894 | 7.121313 | 76.509% |
| samba | 23.610648 | 24.242344 | 5.203121 | 78.254% |
| sao | 9.166008 | 9.760934 | 3.574151 | 62.232% |
| webster | 144.168200 | 154.797125 | 27.779486 | 81.416% |
| xml | 1.487569 | 1.491405 | 0.685093 | 54.005% |
| x-ray | 3.391790 | 3.508531 | 2.804484 | 18.714% |

Across 36 input/pass comparisons, trial beats both controls in 36, loses to both in 0, and lies between them in 0. Maximum per-input/pass encode control spread is 24.850%. All observations are included; no unexplained variability is removed.

Full-frame encode policy budget is 281,286,669 reference versus 315,365,389 trial:
34,078,720 additional bytes. Raw/serialized storage remains 4,194,304/75,497,557;
view storage is 201,588,736 reference versus 235,667,456 trial. Shared decode budget
is 130,029,573 with 4,194,304 raw, 75,497,557 serialized and 50,331,648 token bytes.
These accepted query thresholds are policy accounting, not physical memory.

Process-memory observations below are maximum lifetime working-set and commit
peaks across every member/pass, measured in fresh processes. They include input,
archive and runtime state, and are neither isolated codec RSS nor disk traffic.
Both file buffers are loaded before codec clocks; counters also retain earlier
process peaks. Do not subtract separate-process maxima to infer exact allocations.

| Path | Maximum pre-codec working set (bytes) | Maximum process peak working set (bytes) | Maximum process peak commit (bytes) |
| --- | ---: | ---: | ---: |
| encode-reference-0 | 74,694,656 | 283,963,392 | 354,398,208 |
| encode-reference-1 | 74,694,656 | 283,971,584 | 354,406,400 |
| encode-five-prefix | 74,686,464 | 318,058,496 | 388,517,888 |
| decode-five-prefix | 74,678,272 | 132,734,976 | 202,850,304 |

Only this common-path owner comparison establishes these observed differences.
Do not compare BM-0193 or phase BM-0194/0195 totals as another speedup or subtract
them to infer ownership overhead. Their different paths, storage and timing
boundaries do not isolate those causes; the historical absolute-time disparity
remains unverified. Shared decoder times are absolute observations, not a new
decoder optimization. Public admission and revision-specific external
qualification remain separate from this private integration.

All 36 comparisons beat both controls, but time varies across passes and some
duplicate controls differ substantially. Their cause has not been isolated.
Next split native owner creation, process/frame preparation and destruction
through a separate diagnostic before attributing the variability or transferring
prepared-model/scratch optimizations. Keep the reference and all observations;
the repeated relative gains are evidence for this private owner, not a precise
hardware-independent performance guarantee or public admission.


## BM-0197: Native four-MiB owner call phases

DD-1372 adds a separate optional diagnostic, preserving the previous source,
build and results. Use the same native reference and five-prefix owners, bounded
65,536-byte input/output chunks, frozen complete archives and shared decoder.
Report one disjoint clock interval for creation, each process call and destruction.
Creation includes the factory's query, allocation and initialization; diagnostic
budget searches, configuration construction, I/O, sink copies/comparisons and
classification are outside clocks. Preparation calls consume through a known raw
frame boundary; they include final input copying, frame validation/search/coding
and any same-call drain. Other consuming calls are collection plus any drain;
zero-consumption calls are drain/other. These labels do not isolate search, coding,
pure copying or allocator internals. Decoder process calls remain unsplit.

Three rotated passes retain all 108 encode and 36 shared-decoder observations,
without warmup or discarded reruns. Every complete encoded archive and restored
input matches. The phase sums equal owner totals and preparation counts equal
the expected 57 frames per corpus pass/profile. No build or test overlaps timing.

Corpus phase seconds, summed over twelve members:

| Pass | Encode path | Create | Preparation calls | Collection calls | Drain/other calls | Destroy | Total |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | reference-0 | 0.272603 | 330.813435 | 0.013405 | 0.001496 | 0.060546 | 331.161485 |
| 1 | reference-1 | 0.273855 | 330.586276 | 0.013495 | 0.001512 | 0.062194 | 330.937332 |
| 1 | five-prefix | 0.272485 | 85.753101 | 0.014177 | 0.001466 | 0.074377 | 86.115607 |
| 2 | reference-0 | 0.273120 | 330.630606 | 0.013701 | 0.001456 | 0.061293 | 330.980177 |
| 2 | reference-1 | 0.273207 | 330.834191 | 0.013618 | 0.001569 | 0.062583 | 331.185167 |
| 2 | five-prefix | 0.272480 | 85.645862 | 0.013941 | 0.001436 | 0.073492 | 86.007211 |
| 3 | reference-0 | 0.274237 | 330.616430 | 0.013807 | 0.001477 | 0.063049 | 330.969000 |
| 3 | reference-1 | 0.272588 | 330.883921 | 0.013950 | 0.001554 | 0.061642 | 331.233655 |
| 3 | five-prefix | 0.272607 | 85.940522 | 0.014248 | 0.001514 | 0.074009 | 86.302900 |

Preparation calls account for 95.189% to 99.969% of each encode
owner observation. Maximum individual creation and destruction times across all
observations are 0.024362 and 0.007169 seconds.

For each member/profile, pair the lowest and highest complete-owner times across
the three passes, then subtract their corresponding phases. Keep signed phase
differences; other phases may decrease while the owner total increases. The five
largest observed total differences are:

| Member | Path | Total difference (s) | Preparation difference (s) | Other phases combined difference (s) |
| --- | --- | ---: | ---: | ---: |
| mozilla | reference-0 | 0.352504 | 0.352502 | 0.000003 |
| mr | reference-0 | 0.272096 | 0.272531 | -0.000435 |
| webster | five-prefix | 0.264633 | 0.264807 | -0.000174 |
| samba | reference-0 | 0.202960 | 0.203069 | -0.000109 |
| webster | reference-1 | 0.171539 | 0.170769 | 0.000770 |

This pairing locates elapsed variation among native calls. It does not identify
an operating-system, frequency, cache or algorithm-internal cause, and it does
not explain historical differences by subtracting totals from other diagnostics.
The existing policy budgets remain 281,286,669 reference encode, 315,365,389
five-prefix encode and 130,029,573 shared decode bytes. Whole-process memory
counters include input/archive/runtime and remain separate from codec policy.
Raw/archive corpus sizes remain 211,938,580/61,643,620 bytes. No codec body,
wire representation, public profile, interface or external inventory changes.

Shared decoder owner totals are 6.890119, 6.929269, 6.892012 seconds. These are unchanged-decoder observations.


Whole-process lifetime memory maxima, including input/archive/runtime:

| Path | Peak working set (bytes) | Peak commit (bytes) |
| --- | ---: | ---: |
| encode-reference-0 | 284,004,352 | 354,394,112 |
| encode-reference-1 | 284,004,352 | 354,418,688 |
| encode-five-prefix | 318,058,496 | 388,550,656 |
| decode-five-prefix | 132,739,072 | 202,842,112 |

These counters are not isolated codec resident memory. Independent maxima must not be subtracted to infer allocations.

The five-prefix owner beats both controls in 36/36 comparisons; maximum duplicate-control spread is 1.464%. All observations remain available.


The earlier BM-0196 maximum duplicate-control spread was 24.850%. The present
observations do not retrospectively attribute that earlier variation to a phase
or explain the historical absolute-time differences. Their cause remains
unverified even when this diagnostic's repetitions are closer together.


## BM-0198: Repeated native four-MiB prepared owner comparison

DD-1374 compares the scalar five-prefix owner with the prepared mapping owner
through the same optional native driver. Range coding, finder, decoder, input/
output chunks (65,536 bytes), format, frozen archives and workspace policy are
shared. Preparation classifies a process call that consumes through a raw frame
boundary and includes final input copying/frame work/same-call draining. Use
one disjoint interval per create/process/destroy; factory creation includes
query/allocation/init. Diagnostic query searches, I/O, sink comparisons and
classification are untimed. Decoder process is unsplit and unchanged. Keep the
earlier diagnostic sources/builds/results; do not subtract their totals as
ownership overhead or use historical fluctuations to explain this run.

Reverify the corpus and archives first. Three rotated complete passes retain
108 encode and 36 unchanged-decoder observations in fresh exclusive processes.
No warmup, discarded observation or rerun; no build/test overlaps measurement.
Every complete stream byte and restored input matches, as do frame counts,
per-frame/phase sums and minimum query budgets.

| Pass | Scalar control 0 (s) | Scalar control 1 (s) | Prepared (s) | Prepared reduction vs control mean | Shared decode (s) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 87.182864 | 87.385784 | 85.965115 | 1.511% | 6.869607 |
| 2 | 87.452341 | 87.189698 | 85.811400 | 1.729% | 6.900155 |
| 3 | 87.338138 | 87.410001 | 86.809383 | 0.646% | 6.883055 |

Prepared beats both controls in 32/36 comparisons, loses to both in 2 and lies between them in 2. Maximum member/pass duplicate-control spread is 4.125%.

The trial misses the required improvement across every member/pass comparison.
Retain the scalar five-prefix owner as the reference; the prepared owner remains
a separate private candidate. No existing factory is changed.

Pass three loses on webster (-1.117%) and xml (-1.944%); pass one lies between
controls on dickens and nci. These observations are retained without reruns.


| Member | Mean reduction vs control mean | Minimum / maximum reduction | Wins / losses / between |
| --- | ---: | ---: | ---: |
| dickens | 0.721% | 0.036% / 1.447% | 2 / 0 / 1 |
| mozilla | 1.835% | 1.000% / 2.275% | 3 / 0 / 0 |
| mr | 0.726% | 0.165% / 1.064% | 3 / 0 / 0 |
| nci | 0.657% | 0.396% / 0.826% | 2 / 0 / 1 |
| ooffice | 4.777% | 4.384% / 5.131% | 3 / 0 / 0 |
| osdb | 4.203% | 3.139% / 5.223% | 3 / 0 / 0 |
| reymont | 0.517% | 0.427% / 0.579% | 3 / 0 / 0 |
| samba | 2.096% | 1.758% / 2.482% | 3 / 0 / 0 |
| sao | 3.825% | 2.785% / 4.578% | 3 / 0 / 0 |
| webster | 0.326% | -1.117% / 1.112% | 2 / 1 / 0 |
| xml | 0.989% | -1.944% / 2.914% | 2 / 1 / 0 |
| x-ray | 4.599% | 4.060% / 4.989% | 3 / 0 / 0 |


| Pass | Path | Create (s) | Preparation calls (s) | Collection calls (s) | Drain/other calls (s) | Destroy (s) |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| 1 | reference-0 | 0.273265 | 86.820775 | 0.014043 | 0.001489 | 0.073292 |
| 1 | reference-1 | 0.271847 | 87.024809 | 0.014033 | 0.001494 | 0.073602 |
| 1 | prepared-model | 0.272630 | 85.605071 | 0.013939 | 0.001470 | 0.072005 |
| 2 | reference-0 | 0.272839 | 87.090568 | 0.013862 | 0.001484 | 0.073588 |
| 2 | reference-1 | 0.273242 | 86.826726 | 0.014053 | 0.001473 | 0.074205 |
| 2 | prepared-model | 0.271894 | 85.450153 | 0.013641 | 0.001505 | 0.074207 |
| 3 | reference-0 | 0.271851 | 86.978157 | 0.014059 | 0.001505 | 0.072566 |
| 3 | reference-1 | 0.271234 | 87.051016 | 0.013761 | 0.001378 | 0.072613 |
| 3 | prepared-model | 0.271476 | 86.448509 | 0.013923 | 0.001492 | 0.073983 |


Both encoder policy budgets are 315,365,389 bytes; finder storage is 51,118,080
bytes and the unchanged decoder budget is 130,029,573 bytes. Query equivalence
was checked, not assumed. Corpus raw/archive bytes remain 211,938,580/61,643,620.
Physical counters below are whole-process lifetime peaks including input/archive/
runtime, not isolated codec resident memory. Do not subtract independent maxima
to infer allocations or interpret them as disk traffic.

| Path | Peak working set (bytes) | Peak commit (bytes) |
| --- | ---: | ---: |
| encode-reference-0 | 318,087,168 | 388,571,136 |
| encode-reference-1 | 318,054,400 | 388,554,752 |
| encode-prepared-model | 318,066,688 | 388,538,368 |
| decode-prepared-model | 132,714,496 | 202,858,496 |


These results qualify only this private mapping-owner comparison. Range-preparation
transfer, finder-scratch reuse, full-suite/fuzz, public admission and revision-specific
external verification remain separate. Deeper/historical timing causes remain
unverified; no hardware-independent performance guarantee is inferred.


## BM-0199: Private range-prepared four-MiB common owner comparison

Two scalar five-prefix controls versus the independent range-prepared owner,
with identical scalar token mapping, fixed eligibility three and unchanged
decoder. Three complete passes rotate labels per member/pass, using 144 fresh
exclusive processes, no warmup/discarded reruns. Creation/process/destruction
clocks are disjoint; boundary calls classify complete preparation including
same-call copies/drain. I/O, byte comparison, restoration, queries and capture
remain outside clocks. Every observed stream matches frozen bytes and restores
raw data. These are native observations, not an algorithm-only window comparison.

| Pass | Scalar 0 seconds | Scalar 1 seconds | Prepared range seconds | Reduction versus control mean |
| --- | ---: | ---: | ---: | ---: |
| 1 | 87.7324060 | 87.8649898 | 85.0355265 | 3.1472% |
| 2 | 89.6270032 | 89.3665347 | 86.4431883 | 3.4119% |
| 3 | 90.2621031 | 90.1527095 | 87.0666929 | 3.4817% |

Raw/archive bytes remain 211,938,580/61,643,620 (29.0856% archived size).
Wins against both controls: 33/36; losses: 3; between:
0. Maximum within-member/pass control spread:
8.0990%. The trial does not satisfy the requirement to beat both controls in all 36 comparisons; retain the scalar owner as reference and the trial as a separate private candidate.
Exceptions retained: pass 2 nci (-2.9382%, loss); pass 3 dickens (-1.9980%, loss); pass 3 reymont (-7.9766%, loss)

| Member | Mean reduction | Minimum | Maximum | Wins/losses/between |
| --- | ---: | ---: | ---: | ---: |
| dickens | 1.9190% | -1.9980% | 3.9373% | 2/1/0 |
| mozilla | 5.0648% | 4.2922% | 5.7644% | 3/0/0 |
| mr | 3.6859% | 2.4735% | 5.0148% | 3/0/0 |
| nci | 0.9090% | -2.9382% | 4.7900% | 2/1/0 |
| ooffice | 8.3003% | 6.6555% | 10.2879% | 3/0/0 |
| osdb | 6.4059% | 4.5207% | 9.9111% | 3/0/0 |
| reymont | -1.1219% | -7.9766% | 3.4237% | 2/1/0 |
| samba | 4.2163% | 3.0592% | 5.7564% | 3/0/0 |
| sao | 5.2256% | 3.3809% | 6.6350% | 3/0/0 |
| webster | 2.3100% | 1.7106% | 2.8954% | 3/0/0 |
| xml | 4.5363% | 3.6214% | 5.5644% | 3/0/0 |
| x-ray | 10.8419% | 7.4483% | 13.0541% | 3/0/0 |

| Pass | Control mean preparation seconds | Trial preparation seconds | Control mean other seconds | Trial other seconds |
| --- | ---: | ---: | ---: | ---: |
| 1 | 87.4378498 | 84.6750497 | 0.3608481 | 0.3604768 |
| 2 | 89.1353455 | 86.0788785 | 0.3614235 | 0.3643098 |
| 3 | 89.8451374 | 86.7025239 | 0.3622690 | 0.3641690 |

Other seconds sum creation, collection/drain and destruction. Preparation is
the complete boundary call, including search, scalar mapping, range coding,
prefix/copies and any same-call drain; no inner-loop-only speed is inferred.
Workspace policy budgets are 315365461 bytes for the range-prepared encoder,
315365389 for scalar controls and 130029573 for the unchanged decoder. All
query thresholds, frame/preparation counts, phase sums and per-frame sums pass.
Decode corpus seconds per pass: 6.9623667, 6.9674336, 6.9595621.
These are unchanged-decoder observations, not a decode optimization claim.

| Mode/profile | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| encode/reference-0 | 318111744 | 388558848 |
| encode/reference-1 | 318160896 | 388546560 |
| encode/prepared-range | 318156800 | 388550656 |
| decode/prepared-range | 132784128 | 202833920 |

Memory counters are whole-process working-set/commit peaks, including input,
archive and runtime; they are separate from fixed workspace policy and are not
codec-only RSS. Do not subtract independently observed maxima. No operating
system, cache or processor cause or architecture-independent gain is inferred.
Prior mapping-only candidate and all diagnostics remain preserved. Public
admission, finder scratch reuse, full-suite/fuzz and external qualification are
separate stages.


## BM-0200: Private finder-scratch four-MiB common owner comparison

Two scalar five-prefix controls versus the independent finder-scratch owner,
with identical scalar token mapping, fixed eligibility three and unchanged
decoder. Three complete passes rotate labels per member/pass, using 144 fresh
exclusive processes, no warmup/discarded reruns. Creation/process/destruction
clocks are disjoint; boundary calls classify complete preparation including
same-call copies/drain. I/O, byte comparison, restoration, queries and capture
remain outside clocks. Every observed stream matches frozen bytes and restores
raw data. These are native observations, not an algorithm-only window comparison.

| Pass | Scalar 0 seconds | Scalar 1 seconds | Finder scratch seconds | Reduction versus control mean |
| --- | ---: | ---: | ---: | ---: |
| 1 | 87.2604425 | 87.7423665 | 81.7369438 | 6.5878% |
| 2 | 89.6540955 | 90.4755273 | 83.1257129 | 7.7046% |
| 3 | 90.6316554 | 91.7119135 | 85.5698217 | 6.1444% |

Raw/archive bytes remain 211,938,580/61,643,620 (29.0856% archived size).
Wins against both controls: 35/36; losses: 0; between:
1. Maximum within-member/pass control spread:
6.3513%. The trial does not satisfy the requirement to beat both controls in all 36 comparisons; retain the scalar owner as reference and the trial as a separate private candidate.
Exceptions retained: pass 2 nci (2.9891%, between)

For that observation, scalar controls took 10.0928995 and 10.7208093 seconds;
the trial took 10.0957861 seconds. It was 0.0028866 seconds slower than the
faster control. The origin of this control spread and small ordering difference
is unverified; this observation is retained without a selected rerun.

| Member | Mean reduction | Minimum | Maximum | Wins/losses/between |
| --- | ---: | ---: | ---: | ---: |
| dickens | 6.3817% | 6.2343% | 6.5347% | 3/0/0 |
| mozilla | 7.7725% | 6.3789% | 9.2281% | 3/0/0 |
| mr | 4.1543% | 3.9287% | 4.4279% | 3/0/0 |
| nci | 2.8100% | 2.3903% | 3.0507% | 2/0/1 |
| ooffice | 14.9204% | 12.1545% | 19.0986% | 3/0/0 |
| osdb | 15.6911% | 12.4943% | 18.0782% | 3/0/0 |
| reymont | 2.6853% | 2.1719% | 3.1326% | 3/0/0 |
| samba | 8.4104% | 6.8777% | 9.7632% | 3/0/0 |
| sao | 15.8595% | 15.5542% | 16.1747% | 3/0/0 |
| webster | 5.3445% | 3.0306% | 9.1440% | 3/0/0 |
| xml | 8.3449% | 8.0841% | 8.8626% | 3/0/0 |
| x-ray | 21.6391% | 16.6161% | 24.4739% | 3/0/0 |

| Pass | Control mean preparation seconds | Trial preparation seconds | Control mean other seconds | Trial other seconds |
| --- | ---: | ---: | ---: | ---: |
| 1 | 87.1420122 | 81.3741771 | 0.3593923 | 0.3627667 |
| 2 | 89.7022772 | 82.7619796 | 0.3625342 | 0.3637333 |
| 3 | 90.8102023 | 85.2098301 | 0.3615821 | 0.3599916 |

Other seconds sum creation, collection/drain and destruction. Preparation is
the complete boundary call, including search, scalar mapping, range coding,
prefix/copies and any same-call drain; no inner-loop-only speed is inferred.
Workspace policy budgets are 315365389 bytes for the finder-scratch encoder,
315365389 for scalar controls and 130029573 for the unchanged decoder. All
query thresholds, frame/preparation counts, phase sums and per-frame sums pass.
Decode corpus seconds per pass: 6.9300845, 6.9493474, 6.9280025.
These are unchanged-decoder observations, not a decode optimization claim.

| Mode/profile | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| encode/reference-0 | 318148608 | 388538368 |
| encode/reference-1 | 318164992 | 388554752 |
| encode/finder-scratch | 318160896 | 388542464 |
| decode/finder-scratch | 132808704 | 202866688 |

Memory counters are whole-process working-set/commit peaks, including input,
archive and runtime; they are separate from fixed workspace policy and are not
codec-only RSS. Do not subtract independently observed maxima. No operating
system, cache or processor cause or architecture-independent gain is inferred.
Prior mapping-only candidate and all diagnostics remain preserved. Public
admission, full-suite/fuzz and external qualification are
separate stages.


## BM-0201: Predeclared complete finder-scratch owner repetition

Use the same DD-1379/BM-0200 native executable and validated sources without
rebuilding or modifying the codec. Reverse all twelve corpus members and use
the complementary three permutations of two scalar five-prefix controls and
the finder-scratch candidate. Three complete passes run 144 fresh exclusive
processes, including 36 unchanged-decoder observations. No concurrent builds or
tests, warmup, discarded observations or selected favourable reruns. Every stream
matches frozen bytes and restores raw data; I/O, capture, queries and validation
remain outside disjoint create/process/destroy clocks. Preparation classifies
whole boundary calls including search, mapping, range coding and same-call drain.

| Pass | Scalar 0 seconds | Scalar 1 seconds | Finder scratch seconds | Reduction versus control mean |
| --- | ---: | ---: | ---: | ---: |
| 1 | 88.8211713 | 89.9196927 | 83.7445270 | 6.2950% |
| 2 | 92.0281719 | 92.2275925 | 85.5511094 | 7.1387% |
| 3 | 90.2317418 | 92.2267965 | 83.5905327 | 8.3731% |

Raw/archive bytes per pass remain 211,938,580/61,643,620 (29.0856% archived size).
New evidence: 36 wins against both controls, 0 losses against both and 0 intermediate observations. Maximum member/pass control spread:
17.2098%. The independent repetition improves against both controls in all 36 comparisons.
Combined BM-0200/BM-0201 evidence: 71/72 wins, 0 losses and 1 intermediate observations; the historical nci intermediate
observation is preserved. Public admission and the original strict comparison
condition are not silently changed by this repetition.

New exceptions retained: None.

| Member | Mean reduction | Minimum | Maximum | Wins/losses/intermediate |
| --- | ---: | ---: | ---: | ---: |
| x-ray | 22.2410% | 17.9640% | 25.4307% | 3/0/0 |
| xml | 7.7153% | 6.4495% | 8.3604% | 3/0/0 |
| webster | 3.6631% | 2.9175% | 4.1947% | 3/0/0 |
| sao | 15.1759% | 12.2532% | 17.8401% | 3/0/0 |
| samba | 9.7641% | 8.4908% | 10.6251% | 3/0/0 |
| reymont | 5.1510% | 2.2189% | 9.6558% | 3/0/0 |
| osdb | 15.7702% | 9.8479% | 20.3740% | 3/0/0 |
| ooffice | 13.6317% | 10.9367% | 15.9649% | 3/0/0 |
| nci | 3.7732% | 2.0559% | 6.9537% | 3/0/0 |
| mr | 4.4361% | 2.6467% | 7.1363% | 3/0/0 |
| mozilla | 9.4325% | 6.9857% | 12.9730% | 3/0/0 |
| dickens | 8.8993% | 6.2608% | 13.9337% | 3/0/0 |

| Pass | Member | Scalar 0 seconds | Scalar 1 seconds | Trial seconds | Control spread |
| --- | --- | ---: | ---: | ---: | ---: |
| 1 | x-ray | 2.3581920 | 2.4685671 | 1.7996396 | 4.6805% |
| 1 | xml | 0.6217800 | 0.6297875 | 0.5736185 | 1.2878% |
| 1 | webster | 22.7078633 | 23.6317152 | 22.4938159 | 4.0684% |
| 1 | sao | 2.7705936 | 2.7771144 | 2.4339669 | 0.2354% |
| 1 | samba | 4.7857985 | 4.8018280 | 4.3867790 | 0.3349% |
| 1 | reymont | 5.8790748 | 5.9887808 | 5.7216021 | 1.8660% |
| 1 | osdb | 2.0961328 | 2.0397916 | 1.7145751 | 2.7621% |
| 1 | ooffice | 1.6045124 | 1.5945343 | 1.3441611 | 0.6258% |
| 1 | nci | 9.9759704 | 10.0066392 | 9.7858971 | 0.3074% |
| 1 | mr | 8.2149387 | 8.3153760 | 7.9737944 | 1.2226% |
| 1 | mozilla | 22.6992871 | 22.5753179 | 20.7496226 | 0.5491% |
| 1 | dickens | 5.1070277 | 5.0902407 | 4.7670547 | 0.3298% |
| 2 | x-ray | 2.3492432 | 2.3552708 | 1.8035159 | 0.2566% |
| 2 | xml | 0.6204472 | 0.6164035 | 0.5785398 | 0.6560% |
| 2 | webster | 24.8949890 | 24.8072269 | 23.8086760 | 0.3538% |
| 2 | sao | 2.9123328 | 2.7843815 | 2.3402082 | 4.5953% |
| 2 | samba | 5.0302299 | 4.8421163 | 4.4338500 | 3.8849% |
| 2 | reymont | 5.8596563 | 5.8682607 | 5.7338434 | 0.1468% |
| 2 | osdb | 2.0791665 | 2.3174805 | 1.7504366 | 11.4620% |
| 2 | ooffice | 1.6123427 | 1.6214681 | 1.4400696 | 0.5660% |
| 2 | nci | 10.4870203 | 10.4403771 | 10.2219982 | 0.4468% |
| 2 | mr | 8.6890315 | 8.3092397 | 7.8926085 | 4.5707% |
| 2 | mozilla | 22.3969793 | 22.2914983 | 20.7833318 | 0.4732% |
| 2 | dickens | 5.0967332 | 5.9738691 | 4.7640314 | 17.2098% |
| 3 | x-ray | 2.3845775 | 2.5403092 | 2.0200898 | 6.5308% |
| 3 | xml | 0.6252193 | 0.6252789 | 0.5729757 | 0.0095% |
| 3 | webster | 24.0628130 | 23.3572670 | 22.7907711 | 3.0207% |
| 3 | sao | 2.8621554 | 2.8376644 | 2.4100445 | 0.8631% |
| 3 | samba | 4.8997513 | 4.9409980 | 4.3975783 | 0.8418% |
| 3 | reymont | 6.0108761 | 6.7602808 | 5.7689980 | 12.4675% |
| 3 | osdb | 2.0563102 | 2.1066912 | 1.8765170 | 2.4501% |
| 3 | ooffice | 1.6499055 | 1.6661180 | 1.4259967 | 0.9826% |
| 3 | nci | 11.2727494 | 10.0082913 | 9.9006106 | 12.6341% |
| 3 | mr | 8.2001027 | 8.1953626 | 7.9807620 | 0.0578% |
| 3 | mozilla | 21.1034090 | 24.1159334 | 19.6765163 | 14.2751% |
| 3 | dickens | 5.1038724 | 5.0726017 | 4.7696727 | 0.6165% |

| Pass | Control mean preparation seconds | Trial preparation seconds | Control mean other seconds | Trial other seconds |
| --- | ---: | ---: | ---: | ---: |
| 1 | 89.0062940 | 83.3799447 | 0.3641379 | 0.3645823 |
| 2 | 91.7652513 | 85.1859781 | 0.3626309 | 0.3651313 |
| 3 | 90.8671412 | 83.2298143 | 0.3621280 | 0.3607184 |

Other seconds sum creation, collection/drain and destruction. These are complete
owner observations; no isolated inner-loop or window-size-only speed is inferred.
Fixed workspace policy remains 315365389 bytes for both encoders and 130029573
for the unchanged decoder. Its corpus seconds per pass are
6.8764914, 6.8846134, 6.9496224;
no decoder optimization is claimed.

| Mode/profile | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| encode/reference-0 | 318164992 | 388546560 |
| encode/reference-1 | 318169088 | 388562944 |
| encode/finder-scratch | 318107648 | 388567040 |
| decode/finder-scratch | 132804608 | 202858496 |

Memory counters are whole-process peaks including input, archive and runtime,
separate from fixed workspace policy. They are not codec-only RSS; independently
observed maxima are not subtracted. Source/executable/frozen archive/prior artifact
identities and all rotation, budget, frame and phase checks pass. Variation causes
remain unverified. No implementation change, public/default switch, new full-suite,
fuzz or external qualification occurs. All earlier evidence remains intact.


### Four-MiB finder operation diagnostics (DD-1390)

This untimed private counter traversal is separate from BM-0197 through BM-0201.
No benchmark observation or speed conclusion is added. Both compilers agree
on all twelve members, 57 frames, complete frozen stream bytes and restored
211938580 raw bytes. The split harness charges 315365109 codec bytes, including
the 296-byte diagnostic finder instance; input/archive/oracle/report buffers
are bounded harness overhead. This is not the public owner workspace policy,
codec RSS or a measured process peak.

| Operation | Complete twelve-member count |
| --- | ---: |
| Initialized head/link words | 647022396 |
| Find calls / generated tokens | 28533369 |
| Three-byte chain visits | 311699647 |
| Four-byte chain visits | 113690608 |
| Five-byte chain visits / candidate-filter comparisons | 21577377133 |
| Five-byte prefix comparisons | 5080082336 |
| Extension comparisons | 9645789313 |
| Equal extension bytes | 8631859089 |
| Three-/four-/five-byte insertions | 211938466 / 211938409 / 211938352 |
| Frame-coded modeled operations | 110846666 |

Five-byte visits are 98.0666511 percent of all chain visits. Visits and extension
comparisons are different operations and cannot be combined into a time share.
Five-byte visit counts rank mozilla (9149839318), mr (5171579616), and nci
(3384461066) highest; this differs from the earlier complete-owner elapsed
ranking and does not replace it. Next investigate candidate rejection and
extension work privately before a new throughput proposal. Instrumentation
overhead is not speed evidence, and previous candidate exceptions remain.


### Four-MiB five-byte candidate classification (DD-1391)

Extend DD-1390 without clocks. Both compilers agree on all twelve members and
57 frames; every prior per-frame operation count, frozen token/frame byte and
restored byte remains identical. The larger 408-byte diagnostic finder raises
the split harness codec charge to 315365221 bytes, by 112 bytes. This is not
the public owner policy or a measured process peak.

| Five-byte candidate category | Count | Share of filtered candidates |
| --- | ---: | ---: |
| Best-length-byte rejection | 20553549388 | 95.2550871% |
| Five-byte prefix rejection | 9841253 | 0.0456091% |
| Extension attempt | 1013986492 | 4.6993037% |

The fixed-window corpus has no five-byte out-of-window visits; a bounded fixture
separately tests that category. Extension attempts partition into 22135643
current-best improvements, zero equal results and 991850849 shorter results
(97.8169687 percent of attempts). An improvement may be superseded later.
Mismatch stops total 1013930224; maximum-length stops/updates total 56268.

| Extension comparisons | Count | Share of extension comparisons |
| --- | ---: | ---: |
| Improving candidates | 226656894 | 2.3498014% |
| Non-improving candidates | 9419132419 | 97.6501986% |

Equal extension bytes split into 204577519 improving and 8427281570
non-improving. Non-improving comparison counts rank nci (3392994178), mozilla
(2839860818) and mr (2339117637) highest. These operation categories do not
measure elapsed costs or predict a speed gain. A private additional necessary
byte check may reject some shorter candidates before extension, but its cost
and selectivity require differential qualification and a separate declared
performance trial. Earlier scratch/prepared exceptions and non-admission remain.


### Four-MiB private prefix-end probe selectivity (DD-1392)

This separate counter trial takes no clocks and preserves the DD-1391 baseline.
All twelve members, 57 frames and complete token/frame/raw identity pass in both
compilers. The additional probe checks best-length minus one after the original
best-length byte passes, only when the best exceeds five. It performs 1006653744
comparisons and rejects 559581648 candidates, 55.5882945 percent of its probes.

| Operation | DD-1391 baseline | Private probe trial |
| --- | ---: | ---: |
| Extension attempts | 1013986492 | 459754194 |
| Five-byte prefix rejections | 9841253 | 4491903 |
| Five-byte prefix comparisons | 5080082336 | 2303373506 |
| Improving extension comparisons | 226656894 | 226656894 |
| Non-improving extension comparisons | 9419132419 | 5041820158 |
| Total extension comparisons | 9645789313 | 5268477052 |
| Five-byte chain visits | 21577377133 | 21577377133 |
| Current-best improvements | 22135643 | 22135643 |

Extension comparisons decrease by 4377312261, or 45.3805502 percent, with
1006653744 added probe comparisons. Non-improving extension work decreases
46.4725631 percent. Distinct operations are not converted into elapsed costs
or a speed forecast. Unchanged chain visits remain substantial, and the extra
comparison has overhead. The 424-byte counter finder raises this split-harness
codec charge by sixteen bytes to 315365237; public owner policy is unchanged.

Proceed to a separately qualified counter-free private complete-owner trial
with a predeclared exclusive protocol before deciding adoption. No new timing,
BM record, measured process peak, throughput benefit or public admission is
claimed here. Previous scratch/prepared exceptions remain unchanged.


## BM-0202: Counter-free private prefix-end complete owner (DD-1393)

Qualify TVG-1260 before timing, then declare three passes over all twelve
members in manifest order. For each member/pass rotate control-0, end-probe
and control-1 by the sum of their zero-based indices modulo three, followed
by the unchanged decoder. Each observation uses a fresh process; retain all
108 encodes and 36 decodes without warmup, discard or rerun until success.
Both controls use the admitted five-prefix scalar complete owner, not the
earlier three-prefix reference or unadmitted scratch/prepared candidates.
The trial has no diagnostic counters and retains the same ownership policy.

Window/frame are 4194304 bytes; input/output chunks are 65536 bytes. Both
encoders charge 315365389 bytes at the measured query policy; unchanged
decoder charge is 130029573. Add creation, input collection, complete frame
preparation, draining and destruction intervals to obtain encode time.
Preparation includes input copying and any drain performed in that call;
it does not isolate search or range coding. Decode includes creation, process
and destruction. File I/O, queries, sink comparison and memory sampling are
untimed. Every observation checks full frozen archive or restored raw bytes.
These private owner measurements do not measure the public C or CLI wrapper.

The declared numerical screen requires all 36 trials strictly faster than
both controls and maximum duplicate-control spread at most two percent.
Initial process enumeration reported an error but was incorrectly accepted
as an empty process list. An attempted interruption did not stop the
controller; the original campaign continued. A later successful audit saw
only the controller and its current owner among selected codec/build process
names. The initial interval remains unverified. This makes the campaign
exploratory evidence, regardless of its numerical screen. All individual
observations and the audit incident are retained, with no replacement run.

| Pass | Profile/mode | Total seconds | MiB/s |
| ---: | --- | ---: | ---: |
| 1 | control-0-encode | 87.4856509 | 2.3103 |
| 1 | control-1-encode | 87.5531064 | 2.3085 |
| 1 | end-probe-encode | 85.7390379 | 2.3574 |
| 1 | end-probe-decode | 7.0212447 | 28.7870 |
| 2 | control-0-encode | 87.6331908 | 2.3064 |
| 2 | control-1-encode | 88.3782396 | 2.2870 |
| 2 | end-probe-encode | 85.6803938 | 2.3590 |
| 2 | end-probe-decode | 6.9882547 | 28.9229 |
| 3 | control-0-encode | 87.4321339 | 2.3117 |
| 3 | control-1-encode | 87.7049007 | 2.3046 |
| 3 | end-probe-encode | 86.0260172 | 2.3495 |
| 3 | end-probe-decode | 6.9901511 | 28.9150 |

| Pass | Profile/mode | Create | Prepare | Collect | Drain | Decode process | Destroy (seconds) |
| ---: | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | control-0-encode | 0.2726834 | 87.1211724 | 0.0143001 | 0.0015738 | 0.0000000 | 0.0759212 |
| 1 | control-1-encode | 0.2760599 | 87.1863225 | 0.0143162 | 0.0015868 | 0.0000000 | 0.0748210 |
| 1 | end-probe-encode | 0.2737026 | 85.3737863 | 0.0142863 | 0.0015266 | 0.0000000 | 0.0757361 |
| 1 | end-probe-decode | 0.0747290 | 0.0000000 | 0.0000000 | 0.0000000 | 6.9289118 | 0.0176039 |
| 2 | control-0-encode | 0.2745329 | 87.2697103 | 0.0142926 | 0.0015794 | 0.0000000 | 0.0730756 |
| 2 | control-1-encode | 0.2738624 | 88.0156465 | 0.0144228 | 0.0015374 | 0.0000000 | 0.0727705 |
| 2 | end-probe-encode | 0.2717059 | 85.3190000 | 0.0142027 | 0.0015018 | 0.0000000 | 0.0739834 |
| 2 | end-probe-decode | 0.0738078 | 0.0000000 | 0.0000000 | 0.0000000 | 6.9007357 | 0.0137112 |
| 3 | control-0-encode | 0.2737656 | 87.0611528 | 0.0144676 | 0.0016130 | 0.0000000 | 0.0811349 |
| 3 | control-1-encode | 0.2724899 | 87.3435135 | 0.0142798 | 0.0016628 | 0.0000000 | 0.0729547 |
| 3 | end-probe-encode | 0.2732390 | 85.6620383 | 0.0140898 | 0.0015579 | 0.0000000 | 0.0750922 |
| 3 | end-probe-decode | 0.0751070 | 0.0000000 | 0.0000000 | 0.0000000 | 6.9007448 | 0.0142993 |

| Member | Trials faster than both controls | Gain versus faster control, range across three passes |
| --- | ---: | ---: |
| dickens | 0/3 | -0.7913% .. -0.1633% |
| mozilla | 3/3 | 1.8687% .. 5.3007% |
| mr | 3/3 | 1.0519% .. 1.7797% |
| nci | 3/3 | 4.1904% .. 5.8617% |
| ooffice | 2/3 | -0.1298% .. 1.0150% |
| osdb | 2/3 | -1.4338% .. 2.5591% |
| reymont | 1/3 | -0.3275% .. 0.3110% |
| samba | 3/3 | 1.4520% .. 3.0970% |
| sao | 1/3 | -0.9789% .. 1.1555% |
| webster | 2/3 | -0.2993% .. 0.9807% |
| xml | 2/3 | -0.0561% .. 0.5148% |
| x-ray | 1/3 | -0.9611% .. 0.4900% |

There are 23 wins, 5 trials between controls and 8 losses.
Maximum duplicate-control spread is 3.607395 percent
(ooffice, pass 3: controls 1.5891026 and
1.6464278 seconds; trial 1.5729737). The numerical screen
does not pass. The audit limitation independently prevents admission. No cause
for timing variation is established, and prior candidate exceptions remain.

| Profile/mode | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| control-0-encode | 318144512 | 388534272 |
| control-1-encode | 318136320 | 388567040 |
| end-probe-encode | 318144512 | 388546560 |
| end-probe-decode | 132780032 | 202825728 |

Process peaks include executable, allocator, input/archive/comparison buffers
and codec allocations. They are separate observed maxima, not codec-only RSS,
query charges or a subtraction-derived incremental peak. Every corpus pass
represents 211938580 raw and 61643620 archive bytes in 57 frames, archive/raw
ratio 0.290856058; neither representation nor compression ratio changes.
Unchanged decoding is recorded as a control, with no decoder speedup claim.

Keep the trial private. The earlier 45.3805502 percent reduction in extension
comparisons is not an elapsed gain. A future measurement protocol must require
explicit successful process audits before timing and retain the present
exceptions; this campaign does not authorize public adoption or a repeat until
success. No public format, default, ABI, selection or inventory changes occur.


### Four-MiB retained prefix-end evidence and audit repair (DD-1394)

Reanalyze the complete preserved BM-0202 dataset without new measurements.
All 144 observations, rotated ordering, frozen-byte checks and 36 classifications
reconcile: 23 wins, eight losses, five intermediate results. The original
maximum control spread and process-audit limitation remain. The table joins
DD-1392 operation categories to each member's existing faster-control timing
range. These separate campaigns do not establish time per comparison.

| Member | Wins/losses/between | Probe rejection % | Extension comparisons removed % | Removed extension comparisons / added probe | Existing gain range % |
| --- | --- | ---: | ---: | ---: | ---: |
| dickens | 0/2/1 | 82.1890 | 62.8547 | 1.2983 | -0.7913 .. -0.1633 |
| mozilla | 3/0/0 | 60.8464 | 58.9954 | 3.3614 | 1.8687 .. 5.3007 |
| mr | 3/0/0 | 8.0985 | 17.0740 | 4.0466 | 1.0519 .. 1.7797 |
| nci | 3/0/0 | 50.4214 | 44.7762 | 5.7156 | 4.1904 .. 5.8617 |
| ooffice | 2/0/1 | 71.2898 | 55.1125 | 2.2045 | -0.1298 .. 1.0150 |
| osdb | 2/1/0 | 63.3866 | 34.0187 | 3.8560 | -1.4338 .. 2.5591 |
| reymont | 1/1/1 | 70.7579 | 62.7522 | 2.0047 | -0.3275 .. 0.3110 |
| samba | 3/0/0 | 83.7428 | 81.4235 | 13.6204 | 1.4520 .. 3.0970 |
| sao | 1/1/1 | 74.1083 | 39.6775 | 1.0251 | -0.9789 .. 1.1555 |
| webster | 2/1/0 | 85.5232 | 77.0654 | 4.6554 | -0.2993 .. 0.9807 |
| xml | 2/0/1 | 69.5479 | 43.7725 | 6.0652 | -0.0561 .. 0.5148 |
| x-ray | 1/2/0 | 74.4168 | 0.0155 | 0.0030 | -0.9611 .. 0.4900 |

High rejection or removed-comparison percentage alone does not demonstrate
speed benefit: dickens removes 62.8547 percent of extension comparisons yet
is slower than the faster duplicate control in all three passes. Mr removes
17.0740 percent and wins three times. Mozilla, mr, nci and samba have three
wins each; the other members have mixed classifications. This does not prove
a branch/cache/noise cause or justify choosing a corpus-specific path.

`tools/benchmark_process_audit.py` provides an explicit receipt and idle launch
gate for future protocols. For a standalone snapshot run:

```console
python -B tools/benchmark_process_audit.py
```

Exit zero requires a successfully enumerated empty snapshot; exit two reports
an audit error and exit three reports active selected codec/build processes.
Keep detailed receipts and command lines as local evidence. For orchestration,
`run_after_idle(operation)` validates the snapshot before invoking the operation.
An empty stdout, bare empty list or zero exit with stderr is an error. An
operation failure is propagated without retry. The helper is read-only and
does not terminate processes or change persistent execution policy.

This is a selected-name snapshot, not a system-wide exclusivity lock. Freeze
its scope and require it immediately before each fresh observation; retain
audit errors and completed observations and stop on failure. Thirteen unit
cases plus actual idle/denied/busy checks validate the gate. The busy helper
finishes naturally; the following idle callback is an untimed sentinel.
Repairing the audit does not retroactively qualify BM-0202 or admit its trial.

Before a new predicate or campaign, obtain bounded per-best-length probe and
rejected-extension categories. Current member totals cannot establish an
application threshold. No new BM timing record, codec run, speedup claim or
public adoption follows from this retained-evidence stage.


### Four-MiB current-best-length necessary-byte distribution (DD-1395)

This new diagnostic takes no clocks. It executes all original DD-1391 candidate
work, observing which candidates the DD-1392 additional byte could reject, and
attributes their actual work to the preceding best length. All original counts
and frozen tokens/frame/raw bytes remain exact in both compilers. Counts sum
to 1006653744 added probes, 559581648 would-rejections, 2776708830 removed prefix
comparisons and 4377312261 removed extension comparisons; the removed extension
equal-byte count is 3823079963. These match the actual DD-1392 differences.

| Preceding best length | Added probes | Would-rejections | Removed prefix comparisons | Removed extension comparisons | Removed extension comparisons / probe |
| --- | ---: | ---: | ---: | ---: | ---: |
| 6..8 | 176959984 | 166725212 | 824437672 | 267825820 | 1.513482 |
| 9..16 | 254339539 | 129008642 | 637171857 | 458936242 | 1.804424 |
| 17..32 | 235984252 | 102612519 | 510768509 | 936976874 | 3.970506 |
| 33..64 | 173479746 | 86766602 | 432311730 | 1363080381 | 7.857288 |
| 65..128 | 98593065 | 40168488 | 200565307 | 595440687 | 6.039377 |
| 129..257 | 67297158 | 34300185 | 171453755 | 755052257 | 11.219675 |

Bins zero through five and 258 are zero. Would-rejected prefix failures have
zero extension work; they are retained in the rejection/prefix distribution.
No histogram count is converted into time or a CPU cost. The enlarged private
finder state is 14928 bytes, split codec charge 315379741 and unchanged full-frame
array charge 51118080; these are not public policy or process peaks.

As a single future counter-free hypothesis, require preceding best at least
seventeen for the additional probe. This descriptive cutoff retains
575354221 probes (57.155127 percent) and
3650550199 removable extension comparisons
(83.397071 percent) of the full-probe distribution.
The following table describes work retained by that condition, not a timing
forecast or an admitted policy.

| Member | Retained probes | Retained probe % | Retained removable extension comparisons | Retained removable extension % |
| --- | ---: | ---: | ---: | ---: |
| dickens | 1064948 | 6.2349 | 1689016 | 7.6165 |
| mozilla | 170619323 | 33.7748 | 1104376950 | 65.0373 |
| mr | 86505765 | 87.3680 | 400184149 | 99.8789 |
| nci | 261306544 | 96.7384 | 1526544800 | 98.8781 |
| ooffice | 575352 | 17.2709 | 1893482 | 25.7829 |
| osdb | 857902 | 47.0841 | 5685464 | 80.9212 |
| reymont | 6628622 | 33.9377 | 21581192 | 55.1174 |
| samba | 16230856 | 60.5990 | 343052708 | 94.0360 |
| sao | 0 | 0.0000 | 0 | 0.0000 |
| webster | 30515974 | 50.0850 | 237540078 | 83.7459 |
| xml | 1048917 | 69.2048 | 8002360 | 87.0501 |
| x-ray | 18 | 0.0525 | 0 | 0.0000 |

The longer-best hypothesis retains most removed extension work with fewer
probes across the complete corpus, while substantially reducing probe counts
on several earlier mixed/regressing members. Its extra condition still has
overhead and chain traversal remains unchanged. This does not prove elapsed
benefit, an optimal cutoff, cache/branch behavior or a cure for BM-0202's
exceptions. Preserve the previous trial's non-admission and audit limitation.

Before timing, qualify a distinct counter-free candidate on complete frozen
bytes, exact budgets, aliases, failure invariance and failed-frame publication.
Any future measurement must be separately declared, retain duplicate admitted
controls and every observation, and require explicit successful process audits
before each observation. No new BM timing record or public adoption occurs here.


### Four-MiB longer-best probe owner qualification (DD-1396)

Qualify a separate counter-free owner with the necessary-byte probe enabled
only when the preceding best length is at least seventeen. The first-party
DD-1395 distribution motivated this single hypothesis; retained comparison
counts are not elapsed-time forecasts. Full-probe and diagnostic candidates
remain separate and unadmitted.

Both compilers verify all twelve full frozen streams and raw restorations for
the admitted five-prefix control and trial: 48 encodes and 48 restorations,
with exact complete-owner encode charge 315365389 bytes. The unchanged decoder
charge remains 130029573; finder/streaming/owner layout, arrays and ownership
policy remain unchanged. Standalone guards and the new cutoff-boundary oracle
test pass, including failed-frame non-publication and failure invariance.

Only the harness verification mode is run; codec elapsed clocks are disabled.
No new BM timing record, performance result, peak-memory campaign or admission
is claimed. Any next campaign must freeze duplicate admitted controls, all
observations and the strict per-observation process audit gate. Qualification
does not erase BM-0202's numerical failure or its initial audit limitation.


## BM-0203: Four-MiB longer-best complete owner with strict per-launch audits

Measure the DD-1396 counter-free owner, whose extra necessary-byte check starts
at preceding best length seventeen, against duplicate admitted five-prefix
owners. MSVC C++20 release flags are `/O2 /MD`; both owners share the qualified
buffer/state policy and executable. Three canonical complete corpus passes
rotate control-0/trial/control-1 for every member, with unchanged decoder last
and a fresh process per observation. No warmup, discarded result, replacement
or rerun until success. All 144 launches have persisted explicit successful
zero-process audit receipts immediately beforehand. This selected-name snapshot
is not an OS lock or guarantee of future quiescence.

The complete-owner clock includes creation, collecting raw input, complete
frame preparation, draining and destruction, with 65536-byte input/output
chunks. It excludes file I/O, sink comparisons, query and memory sampling.
Archive/raw checks pass every observation. Encode charge is 315365389 bytes;
unchanged decode charge is 130029573. Complete corpus totals are 211938580 raw
bytes, 61643620 archive bytes and 57 frames. DD-1396 qualification remains the
two-compiler correctness/sanitizer evidence; these clocks use one compiler.

All encode observations follow; seconds include complete owner phases.
Gain uses the faster of the two controls; spread is `(slower/faster-1)*100`.
A loss means the trial is no faster than either control; between means it is
strictly faster than only the slower control. Every pass is retained.

| Member | Pass | Control 0 s | Trial s | Control 1 s | Gain % | Control spread % | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| dickens | 1 | 5.2056266 | 5.1808865 | 5.1994806 | 0.357615 | 0.118204 | win |
| mozilla | 1 | 21.6831109 | 20.9266831 | 21.6502783 | 3.342198 | 0.151650 | win |
| mr | 1 | 8.5663197 | 8.2408183 | 8.5441212 | 3.549843 | 0.259810 | win |
| nci | 1 | 10.4994457 | 9.4342734 | 10.1629454 | 7.169890 | 3.311051 | win |
| ooffice | 1 | 1.8697856 | 1.8178077 | 1.9205832 | 2.779886 | 2.716761 | win |
| osdb | 1 | 2.5098447 | 2.1692270 | 2.2900538 | 5.276156 | 9.597630 | win |
| reymont | 1 | 6.6392707 | 7.1063535 | 7.1046519 | -7.035152 | 7.009523 | loss |
| samba | 1 | 4.9572076 | 4.9020521 | 5.0578624 | 1.112632 | 2.030474 | win |
| sao | 1 | 2.9361624 | 3.1744668 | 2.9625746 | -8.116186 | 0.899548 | loss |
| webster | 1 | 25.5112103 | 23.5872475 | 23.5291864 | -0.246762 | 8.423682 | between |
| xml | 1 | 0.6326595 | 0.6218073 | 0.6312086 | 1.489413 | 0.229861 | win |
| x-ray | 1 | 2.5290299 | 2.3981386 | 2.4288305 | 1.263649 | 4.125418 | win |
| dickens | 2 | 5.1722370 | 5.2034270 | 5.1591194 | -0.858821 | 0.254260 | loss |
| mozilla | 2 | 22.3010567 | 22.1594400 | 22.7342766 | 0.635022 | 1.942598 | win |
| mr | 2 | 8.5285640 | 8.1161849 | 8.5285511 | 4.835126 | 0.000151 | win |
| nci | 2 | 10.4933106 | 11.1120134 | 11.1186899 | -5.896164 | 5.959790 | between |
| ooffice | 2 | 1.6716538 | 1.7453700 | 1.8150538 | -4.409777 | 8.578331 | between |
| osdb | 2 | 2.2304732 | 2.0934610 | 2.1876612 | 4.305978 | 1.956976 | win |
| reymont | 2 | 6.6694267 | 5.9438838 | 5.9220361 | -0.368922 | 12.620501 | between |
| samba | 2 | 5.0291030 | 4.9459876 | 4.9077129 | -0.779889 | 2.473456 | between |
| sao | 2 | 3.1822924 | 2.8964517 | 2.9906070 | 3.148368 | 6.409582 | win |
| webster | 2 | 24.7620417 | 24.2868316 | 24.6389237 | 1.429008 | 0.499689 | win |
| xml | 2 | 0.6259572 | 0.6298605 | 0.6330616 | -0.623573 | 1.134966 | between |
| x-ray | 2 | 2.8071326 | 2.7514757 | 2.7061888 | -1.673457 | 3.730109 | between |
| dickens | 3 | 5.7160812 | 5.8047040 | 5.8067803 | -1.550412 | 1.586736 | between |
| mozilla | 3 | 24.9521085 | 24.8918206 | 25.3517436 | 0.241614 | 1.601609 | win |
| mr | 3 | 9.1462956 | 8.5565416 | 8.9413225 | 4.303400 | 2.292425 | win |
| nci | 3 | 11.5893722 | 10.2992247 | 14.3687128 | 11.132160 | 23.981805 | win |
| ooffice | 3 | 1.9462802 | 1.9015853 | 1.9012589 | -0.017168 | 2.367973 | between |
| osdb | 3 | 2.8726100 | 2.2511184 | 2.6421818 | 14.800776 | 8.721133 | win |
| reymont | 3 | 8.0701611 | 8.5321997 | 6.3188857 | -35.026967 | 27.714940 | loss |
| samba | 3 | 5.3903277 | 5.3310635 | 5.4823943 | 1.099454 | 1.707996 | win |
| sao | 3 | 3.6338668 | 3.2888182 | 3.5509001 | 7.380717 | 2.336498 | win |
| webster | 3 | 27.6288813 | 24.8009672 | 28.7919263 | 10.235355 | 4.209526 | win |
| xml | 3 | 0.6780062 | 0.6423354 | 0.6329811 | -1.477817 | 7.113182 | between |
| x-ray | 3 | 2.7420343 | 2.8063918 | 2.7801113 | -2.347071 | 1.388641 | loss |

| Pass | Control 0 corpus s | Trial corpus s | Control 1 corpus s | Per-member faster controls s | Gain vs faster controls % | Unchanged decode corpus s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 93.5396736 | 89.5597618 | 91.4817769 | 90.8385311 | 1.407739 | 6.9938850 |
| 2 | 93.4732489 | 91.8843872 | 93.3418821 | 92.1327785 | 0.269601 | 6.9546719 |
| 3 | 104.3660251 | 99.1067704 | 106.5691987 | 102.0063353 | 2.842534 | 6.9798033 |

| Member | Unchanged decode pass 1 s | Pass 2 s | Pass 3 s |
| --- | ---: | ---: | ---: |
| dickens | 0.3744120 | 0.3761659 | 0.3775204 |
| mozilla | 2.0221882 | 2.0065452 | 2.0248878 |
| mr | 0.3962356 | 0.3953717 | 0.3963966 |
| nci | 0.2769092 | 0.2776426 | 0.2749769 |
| ooffice | 0.3420788 | 0.3415777 | 0.3452145 |
| osdb | 0.3857159 | 0.3768980 | 0.3857696 |
| reymont | 0.1855772 | 0.1810138 | 0.1811711 |
| samba | 0.5244924 | 0.5285517 | 0.5229668 |
| sao | 0.6008890 | 0.5962264 | 0.5965968 |
| webster | 1.1498571 | 1.1463293 | 1.1474704 |
| xml | 0.0669499 | 0.0670607 | 0.0667426 |
| x-ray | 0.6685797 | 0.6612889 | 0.6600898 |

The predeclared screen fails: 21 wins, 5 losses and 10 between-controls cases. Maximum
duplicate-control spread is 27.714940 percent;
all 36 strict wins and spread at most two percent are required. Individual
gain against the faster control ranges from
-35.026967 to
14.800776 percent. Aggregate gains do
not prove a universal speedup, an optimal cutoff or a cause of variation.

| Profile/direction | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| control-0-encode | 318144512 | 388558848 |
| control-1-encode | 318132224 | 388571136 |
| probe17-encode | 318140416 | 388567040 |
| probe17-decode | 132792320 | 202854400 |

These are whole-process peaks, including input/archive and runtime storage,
not codec-only resident memory or a replacement for exact allocation charges.
The candidate remains private; no automatic public admission, format/ABI/
default change, fuzz or external gate is claimed. Preserve BM-0202's failed
screen and initial audit limitation without retroactively qualifying them.
All prior artifacts and fixed verification binaries remain intact.


### Retained owner-phase and corresponding-frame review (DD-1398)

Review existing BM-0203 values without new clocks. Preparation has the largest
absolute phase delta in every current duplicate-control pair. The table retains
all 36 pairs: delta means control-1 minus control-0, and the preparation share
uses the sum of absolute phase deltas as its denominator. It is not an elapsed
work share, statistical variance or evidence of a cause.

| Member | Pass | Total delta s | Preparation delta s | Preparation share of absolute phase deltas % |
| --- | ---: | ---: | ---: | ---: |
| dickens | 1 | -0.0061460 | -0.0062171 | 89.894448 |
| mozilla | 1 | -0.0328326 | -0.0325840 | 99.049750 |
| mr | 1 | -0.0221985 | -0.0220722 | 98.872509 |
| nci | 1 | -0.3365003 | -0.3366663 | 99.915181 |
| ooffice | 1 | 0.0507976 | 0.0511755 | 98.017828 |
| osdb | 1 | -0.2197909 | -0.2198264 | 99.926587 |
| reymont | 1 | 0.4653812 | 0.4645163 | 99.808405 |
| samba | 1 | 0.1006548 | 0.1001940 | 99.542198 |
| sao | 1 | 0.0264122 | 0.0264319 | 99.111696 |
| webster | 1 | -1.9820239 | -1.9824193 | 99.980059 |
| xml | 1 | -0.0014509 | -0.0014670 | 95.204101 |
| x-ray | 1 | -0.1001994 | -0.1008685 | 99.283929 |
| dickens | 2 | -0.0131176 | -0.0138899 | 94.732714 |
| mozilla | 2 | 0.4332199 | 0.4335961 | 99.913312 |
| mr | 2 | -0.0000129 | -0.0011559 | 49.336293 |
| nci | 2 | 0.6253793 | 0.6257218 | 99.928949 |
| ooffice | 2 | 0.1434000 | 0.1418582 | 98.780444 |
| osdb | 2 | -0.0428120 | -0.0423495 | 98.375107 |
| reymont | 2 | -0.7473906 | -0.7478224 | 99.941998 |
| samba | 2 | -0.1213901 | -0.1222599 | 99.293590 |
| sao | 2 | -0.1916854 | -0.1921442 | 99.761790 |
| webster | 2 | -0.1231180 | -0.1237707 | 99.256841 |
| xml | 2 | 0.0071044 | 0.0067645 | 92.001469 |
| x-ray | 2 | -0.1009438 | -0.1002502 | 99.285542 |
| dickens | 3 | 0.0906991 | 0.0901712 | 99.330134 |
| mozilla | 3 | 0.3996351 | 0.4006138 | 99.756295 |
| mr | 3 | -0.2049731 | -0.2044544 | 99.720574 |
| nci | 3 | 2.7793406 | 2.7792096 | 99.971669 |
| ooffice | 3 | -0.0450213 | -0.0454810 | 98.459707 |
| osdb | 3 | -0.2304282 | -0.2305086 | 99.944328 |
| reymont | 3 | -1.7512754 | -1.7502208 | 99.935752 |
| samba | 3 | 0.0920666 | 0.0915153 | 99.302180 |
| sao | 3 | -0.0829667 | -0.0821898 | 98.963643 |
| webster | 3 | 1.1630450 | 1.1620133 | 99.911293 |
| xml | 3 | -0.0450251 | -0.0453803 | 98.003658 |
| x-ray | 3 | 0.0380770 | 0.0384299 | 99.043581 |

Across those pairs, preparation contributes
99.816934 percent of all absolute
phase deltas. All 36 same-profile encode three-pass range comparisons also
have preparation as their largest phase delta. Same-profile pass ranges are
descriptive retained minima/maxima, not replacement observations or paired
trial effects. The maximum trial range is reymont at 43.545870 percent; the
unchanged decoder's maximum same-member range is 2.521023 percent, which does
not prove stability of the different encode workload.

| Pass | Profile | Total corpus s | Preparation s | Creation s | Collection s | Drain s | Destruction s |
| --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | control-0 | 93.5396736 | 93.1732299 | 0.2772193 | 0.0139830 | 0.0017303 | 0.0735111 |
| 1 | control-1 | 91.4817769 | 91.1134268 | 0.2769487 | 0.0141879 | 0.0015976 | 0.0756159 |
| 1 | probe17 | 89.5597618 | 89.1935118 | 0.2769644 | 0.0143173 | 0.0016134 | 0.0733549 |
| 2 | control-0 | 93.4732489 | 93.1094686 | 0.2757190 | 0.0142260 | 0.0016350 | 0.0722003 |
| 2 | control-1 | 93.3418821 | 92.9737665 | 0.2789371 | 0.0139936 | 0.0015115 | 0.0736734 |
| 2 | probe17 | 91.8843872 | 91.5184403 | 0.2757169 | 0.0140224 | 0.0015929 | 0.0746147 |
| 3 | control-0 | 104.3660251 | 103.9970770 | 0.2771669 | 0.0143721 | 0.0017361 | 0.0756730 |
| 3 | control-1 | 106.5691987 | 106.2007952 | 0.2776090 | 0.0141935 | 0.0015827 | 0.0750183 |
| 3 | probe17 | 99.1067704 | 98.7407813 | 0.2755979 | 0.0139495 | 0.0016084 | 0.0748333 |

Preparation accounts for 99.591055 through 99.654306 percent of these elapsed
encode totals. Those elapsed shares are separate from the delta-magnitude
shares above. The largest duplicate-control spread, third-pass reymont, has
total delta -1.7512754 seconds and preparation delta -1.7502208 seconds. Its
corresponding frame differences are retained below, with no comparison between
different frame contents or lengths.

| Frame index | Control 0 s | Control 1 s | Delta s |
| ---: | ---: | ---: | ---: |
| 0 | 6.3351667 | 4.7850681 | -1.5500986 |
| 1 | 1.7044188 | 1.5042966 | -0.2001222 |

Historical BM-0202 is reviewed separately, preserving its initial audit
limitation: preparation leads 34 control pairs and destruction leads two
small-delta pairs (mr pass one and x-ray pass three). Its preparation share
of all absolute phase deltas is 98.503152
percent. Different campaigns are not paired evidence of a cutoff effect.

The preparation call includes input copy, dictionary tokenization, complete
frame coding/validation and any drain in that call. It cannot isolate finder
or Range costs. CPU frequency, scheduling, unselected workloads, cache and
branch state are unobserved; selected-process audits do not identify them.
Preserve both failed numerical screens and all observations. No new BM record,
elapsed campaign, public admission or causal performance explanation is added.
The next proposed diagnostic separates tokenization from frame coding/validation
and must qualify bytes, budgets and failure/report-publication contracts before
any separately declared instrumented timing.


### Preparation-split diagnostic qualification without timing (DD-1399)

Qualify a separate admitted-baseline private diagnostic before taking new
clocks. Optional inner intervals cover the complete dictionary tokenizer call
and complete frame coding/validation call. The latter includes Range coding
and serialization; input copying, raw validation and other preparation/drain
work remain outside the two intervals. These scopes cannot directly attribute
unobserved CPU, scheduling, cache or branch causes.

The bounded report retains up to sixteen successful frame samples and exports
only after the stream successfully ends. Report state adds 656 bytes, staged
sample/clock state 48 bytes: complete owner query 315366093, 704 above admitted
315365389. Arrays/buffers and unchanged decode charge 130029573 are preserved.
Caller/harness report copies are separate from the codec-owned allocation
charge; no new process-peak measurement or codec-only resident memory is claimed.

Both compilers verify all twelve complete frozen streams and raw restorations
for admitted control and diagnostic: 48 encodes and 48 restorations. Report
metadata matches all 57 corresponding frame headers per compiler. Additional
fifteen/sixteen-frame zero-vector boundaries pass eight encodes/restorations;
the private seventeen-frame bound and alias/failure/report guards are tested.
Only disabled-clock verification runs, with all split and outer elapsed fields
zero. No new BM record, enabled-hook performance result or public admission.

A future instrumented campaign must first exercise enabled hooks and reconcile
split intervals with their enclosing preparation calls, preserving exact bytes
and report publication guards. Freeze a fresh protocol, qualified hashes and
strict successful receipts before every launch; retain every observation and
stop on audit/operation failure. Instrumented-path timing is diagnostic evidence,
not an automatic speed claim for the unchanged counter-free owner. Preserve
BM-0202/BM-0203 failures and all earlier artifacts without replacement.


## BM-0204: Enabled private tokenization/frame preparation split

Measure the DD-1399 qualified diagnostic with enabled optional inner clocks,
without rebuilding. Three predeclared rotated passes over all twelve complete
members retain duplicate admitted controls, diagnostic encode and unchanged
decode: 144 fresh processes, 108 encodes and 36 restorations. No warmup,
discard, replacement or retries until success. Every launch has a persisted
strict successful selected-process audit; this is a snapshot, not an OS lock
or an observation of CPU/scheduling/cache/branch causes.

All full streams/restorations match their frozen or raw oracle. Validate the
first and every enabled sample before interpreting ratios: all 171 frame
metadata records match frozen headers; all inner intervals are finite and
nonnegative, and inner sums fit enclosing preparation intervals. Tolerance is
max(1e-8 seconds, enclosing seconds times 1e-9). Signed residuals are retained
without clipping; all are positive. Outer phase and preparation sums conserve.

The tokenization interval is the complete admitted FivePrefix tokenizer call, including initialization and insertion; it is not pure dictionary search. The frame interval is the complete encoder call, including operation materialization, validation, Range coding and serialization; it is not pure Range coding. Raw prevalidation, input copying, other preparation and drains remain outside both inner intervals.

The following are corpus sums in each diagnostic pass. Shares divide inner
times by enclosing preparation time; the residual is that preparation time
minus both inner intervals. These are instrumented elapsed scopes, not pure
finder/Range costs, statistical variance attribution or causal speed effects.

| Pass | Preparation seconds | Tokenization seconds | Frame coding/validation seconds | Signed residual seconds | Tokenization share | Coding share |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 91.341152 | 79.173923 | 12.166662 | 0.000567 | 86.679356% | 13.320023% |
| 2 | 88.780770 | 76.739503 | 12.040728 | 0.000539 | 86.437078% | 13.562315% |
| 3 | 88.776028 | 76.739099 | 12.036390 | 0.000539 | 86.441240% | 13.558153% |


Across individual member/pass aggregates, tokenization share spans
50.949809% to
95.498271%. Corpus aggregates
must not be interpreted as the same distribution for every input.

12 strict wins, 12 losses and 12 between-control observations; maximum duplicate-control spread 8.506055%. The declared 36-win/2% numerical screen fails. These owner-time comparisons do not establish an improvement for
the uninstrumented owner. No automatic public adoption is allowed, regardless
of the numerical screen. Earlier BM-0202/BM-0203 failures remain unchanged;
historical audit limitations are not retroactively qualified, and separate
campaigns are not paired causal experiments.

Diagnostic codec-owned charge is 315366093 bytes, control 315365389 and decode
130029573: 704 diagnostic bytes are explicitly charged. Report retrieval is
inside outer destruction; caller report storage is separate. Maximum whole
process working-set/pagefile samples for diagnostic encode are
318156800 /
388562944 bytes.
These are whole-process peaks, not codec-only resident memory or allocation
charges. No public source, format, defaults, inventory or fixed binary changes.


### Tokenizer-internal scope qualification without timing (DD-1401)

Qualify an additional finite-buffer private diagnostic after BM-0204. Its
optional intervals cover complete admitted finder initialization, find_match
and advance calls. The finder itself is unchanged. Advance includes hashing,
validation, insertion and position updates; tokenizer decisions/stores, counters,
loop work, preflight and clock overhead remain outside those inner intervals.
This is not pure insertion time or an unexplored search-branch attribution.

An 80-byte staged aggregate and one live 8-byte clock point add 88 transient
bytes, explicitly charged before mutation. No per-token records or tokenization
allocation. The bounded verifier workspace charge 315365037 is separate from
admitted owned-factory minimum 315365389 and from process-peak measurements;
caller reports and file/oracle buffers remain bounded harness overhead.

Six targeted tests, both release compilers, partial sanitizers and fresh targeted
integration qualify clocks disabled. All 24 complete corpus re-encodes and 24
restorations match their frozen/token/raw oracles; 114 frame reports have exact
metadata/counts and all elapsed fields zero. Twenty negative executable cases
withhold all reports, including failure after an earlier successful frame;
two empty cases pass. Token and report failure invariance and exact added-state
limits pass. Initial build/fixture failure artifacts remain retained.

No new BM result: enabled hooks have not been run. Clock-enabled find and
advance use four clock reads per token in total, plus initialization reads,
which may materially perturb the hot loop. Any next enabled campaign must
freeze hashes/order/counts/tolerance, validate the first and every enabled
sample and conserve inner sums against the enclosing tokenizer call, persist
strict successful selected-process receipts before every launch, and retain
all evidence without warmup/discard/replacement/retries until success.
Do not subtract an assumed timer cost, infer uninstrumented proportions or
causal environmental effects, or reinterpret BM-0204 and failed probe screens
as paired experiments. Public selection, format, defaults, inventory and fixed
binaries remain unchanged; no speed/admission/fuzz/external-gate result.


## BM-0205: Fine-clock path against duplicate outer-only tokenizer controls

The DD-1402 private harness adds outer-only clocks around the same finite
tokenizer call. Duplicate controls keep inner clocks disabled; the diagnostic
enables four inner clock reads per token plus two per initialization and its
accumulations. Executable, admitted finder, counters, report layout and buffer
capacities are identical across modes. File I/O, allocations, frozen decoding,
frame encoding/comparison, destruction and report printing are outside the
measured interval. This is not complete owned-codec/preparation timing.

After two-compiler clock-disabled complete-corpus/guard qualification, freeze
three rotated passes over all twelve members: 108 retained fresh processes,
without warmup, discard, replacement or retries until success. Four additional
declared later-frame failure runs cover both measured modes and withhold all
reports. Each of 112 launches has a strict successful selected-process receipt
persisted before it. This snapshot is not an OS lock, future quiescence or a
measurement of CPU/scheduling/cache/branch causes.

All complete token/frame/raw comparisons match their oracles. All 513 measured
frame records conserve counts and metadata; all 171 inner-enabled records have
finite nonnegative intervals and positive outer time. Inner sums fit per-frame
and aggregate outer times within max(1e-8 seconds, outer seconds times 1e-9).
Signed residuals remain unmodified. Controls have zero inner intervals.

The following corpus sums compare observed whole instrumented paths. The final
column divides the instrumented corpus total by the sum of faster/slower
duplicate controls chosen separately for each member, then subtracts one.
Positive values mean the enabled path took longer. These differences include
all mode interactions and are not isolated timer costs to subtract.

| Pass | Outer control 0 seconds | Inner-enabled outer seconds | Outer control 1 seconds | Observed difference versus faster/slower member controls |
| --- | ---: | ---: | ---: | ---: |
| 1 | 75.621511 | 77.693037 | 75.305436 | 3.454006% / 2.459781% |
| 2 | 75.977270 | 79.032006 | 75.740927 | 4.516573% / 3.850819% |
| 3 | 76.544196 | 79.954989 | 76.698231 | 4.831485% / 3.874864% |


The next table shows enabled intervals and their fractions of the enabled
outer tokenizer interval. Initialize is the complete initialization call;
find is the complete admitted search call; advance includes validation,
hash/bucket work, insertion and position updates. The residual includes token
choice/store, loop/counter work, preflight, reporting and clock overhead outside
the inner intervals. These fractions do not reconstruct uninstrumented costs.

| Pass | Initialize seconds | Find seconds | Advance seconds | Signed residual seconds | Instrumented outer shares: initialize / find / advance / residual |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.094601 | 74.763431 | 1.456043 | 1.378962 | 0.121763% / 96.229255% / 1.874097% / 1.774885% |
| 2 | 0.094059 | 76.052102 | 1.502070 | 1.383775 | 0.119013% / 96.229497% / 1.900584% / 1.750905% |
| 3 | 0.094310 | 76.999660 | 1.470028 | 1.390992 | 0.117954% / 96.303758% / 1.838569% / 1.739719% |


27 of 36 duplicate-control pairs meet the predeclared 2% spread review threshold; maximum spread is 12.825013%. Retain every pair, including those exceeding the threshold; do not treat noisy individual contrasts as resolved instrumentation effects.

No optimization admission screen is being applied to this diagnostic mode
contrast, and no public speed result is claimed. Earlier BM-0202/BM-0203/BM-0204
failures remain unchanged; this campaign is not a paired causal comparison with
those campaigns. Added diagnostic state is still 88 transient bytes; the
finite harness workspace bound remains 315365037, distinct from owned-factory
minimum and whole-process peak. No new process-peak campaign or memory-saving
claim. Public sources, format, defaults, inventory and fixed binaries remain.


## DD-1403: Private bucket18 qualification before owner timing

Date: 2026-10-02. The private 4MiB trial refines only the five-prefix head
partition to 18 bits. The specialized 1MiB width/noise evidence in BM-0174 through
BM-0176 did not admit wider buckets and does not establish this trial's speed.
BM-0205 fine-clock shares remain instrumented observations, without assumed
timer-cost subtraction or transfer into uninstrumented performance claims.

Two-compiler full token/frame and complete-owner qualification passes under
TVG-1270. Every compiler/owner produces 61643620 archive bytes for 211938580 raw
bytes across 57 frames; complete bytes/restorations match the frozen reference.
No inner counters/clocks are added. Complete-owner qualification disables outer
phase clocks, so no elapsed observation or BM-0206 is recorded here.

The enlarged heads add 786432 checked buffer bytes. Finder arrays at a full
frame are 51904512 bytes; owner minimum is 316151821 versus admitted 315365389,
with unchanged state size and decoder minimum 130029573. These are workspace
charges, not a new peak-memory campaign. Larger initialization/reset work may
offset shorter collision chains; no benefit is inferred. A later full-owner
campaign must retain exact-output qualification, duplicate admitted controls,
strict process receipts, complete phase costs, all observations and the retained
admission criteria. Public selection and earlier failed screens remain unchanged.


## BM-0206: Four-MiB bucket18 complete-owner screen

Date: 2026-10-02. Measure DD-1403's qualified counter-free owner with 18-bit
five-prefix heads and retained 16-bit three/four-prefix heads. Compare duplicate
admitted FivePrefix owners within the same qualified executable. MSVC C++20
release flags are `/O2 /MD`; no rebuild or codec/harness change. Three canonical
complete corpus passes rotate control-0/trial/control-1 for each member, followed
by the unchanged decoder. Each observation has a fresh process and a persisted
strict successful zero-selected-process audit receipt before invocation. All
144 observations are retained; no warmup, discard, replacement or retry until
success. Selected-name snapshots are not an OS lock or future quiescence proof.

The complete-owner clocks include creation/allocation, raw collection, frame
preparation/reset, drain and destruction with 65536-byte input/output chunks.
File I/O, sink comparisons, query and memory sampling are excluded. No inner
counters/report hooks/clocks are added. Both head initialization and the additional
786432 bytes are included: admitted encode minimum 315365389, trial 316151821,
unchanged decoder 130029573. State and all other buffers are unchanged. Complete
archive/raw checks pass all observations. Each complete traversal has 211938580
raw bytes and 61643620 archive bytes; each encode prepares 57 frames. DD-1403 is
the two-compiler correctness evidence; this timing campaign uses one compiler.

All encode observations follow. Gain uses the faster of the two controls;
control spread is `(slower/faster-1)*100`. A win is strictly faster than both,
a loss is no faster than either, and between is faster only than the slower
control. No member or pass is removed.

| Member | Pass | Control 0 s | Trial s | Control 1 s | Gain % | Control spread % | Result |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- |
| dickens | 1 | 5.1303923 | 4.8889875 | 5.1531711 | 4.705387 | 0.443997 | win |
| mozilla | 1 | 21.2053392 | 20.2606282 | 21.1375384 | 4.148592 | 0.320760 | win |
| mr | 1 | 8.2514876 | 8.3282661 | 8.2486184 | -0.965588 | 0.034784 | loss |
| nci | 1 | 10.0016284 | 10.1308177 | 10.0378348 | -1.291683 | 0.362005 | loss |
| ooffice | 1 | 1.5995436 | 1.4411997 | 1.5752857 | 8.511853 | 1.539905 | win |
| osdb | 1 | 1.9862603 | 1.8510626 | 2.0007581 | 6.806646 | 0.729904 | win |
| reymont | 1 | 5.9089592 | 5.8392108 | 5.9342593 | 1.180384 | 0.428165 | win |
| samba | 1 | 4.8077611 | 4.5971448 | 4.8158239 | 4.380756 | 0.167704 | win |
| sao | 1 | 2.7173517 | 2.5460128 | 2.7057249 | 5.902747 | 0.429711 | win |
| webster | 1 | 22.9122556 | 22.2131270 | 22.8866108 | 2.942698 | 0.112052 | win |
| xml | 1 | 0.6217622 | 0.6077029 | 0.6254463 | 2.261202 | 0.592526 | win |
| x-ray | 1 | 2.3130294 | 1.8094436 | 2.3255429 | 21.771699 | 0.541000 | win |
| dickens | 2 | 5.0950604 | 4.8873878 | 5.1543133 | 4.075960 | 1.162948 | win |
| mozilla | 2 | 21.3093493 | 20.1828570 | 21.2334959 | 4.948026 | 0.357235 | win |
| mr | 2 | 8.2218600 | 8.2879861 | 8.2875966 | -0.804272 | 0.799534 | loss |
| nci | 2 | 10.0326946 | 10.2456185 | 10.0363257 | -2.122300 | 0.036193 | loss |
| ooffice | 2 | 1.5803699 | 1.4338573 | 1.6085660 | 9.270779 | 1.784146 | win |
| osdb | 2 | 2.0008560 | 1.8648524 | 2.0401051 | 6.797271 | 1.961615 | win |
| reymont | 2 | 5.9299219 | 5.8568188 | 5.8991089 | 0.716890 | 0.522333 | win |
| samba | 2 | 4.8121667 | 4.6071343 | 4.8063995 | 4.145831 | 0.119990 | win |
| sao | 2 | 2.7045733 | 2.5540504 | 2.7205578 | 5.565495 | 0.591017 | win |
| webster | 2 | 22.9186602 | 22.1500545 | 22.8831831 | 3.203788 | 0.155036 | win |
| xml | 2 | 0.6319581 | 0.6079503 | 0.6249696 | 2.723220 | 1.118214 | win |
| x-ray | 2 | 2.4624456 | 1.7924930 | 2.3910673 | 25.033770 | 2.985207 | win |
| dickens | 3 | 5.2578910 | 4.9054399 | 5.2473048 | 6.515057 | 0.201745 | win |
| mozilla | 3 | 21.9702700 | 20.4703016 | 21.6565868 | 5.477711 | 1.448442 | win |
| mr | 3 | 8.2542930 | 8.3259977 | 8.2837495 | -0.868696 | 0.356863 | loss |
| nci | 3 | 10.0663209 | 10.1645050 | 10.0547430 | -1.091644 | 0.115149 | loss |
| ooffice | 3 | 1.6376117 | 1.4872130 | 1.6222536 | 8.324260 | 0.946714 | win |
| osdb | 3 | 2.0625797 | 1.9559545 | 2.0546212 | 4.802184 | 0.387346 | win |
| reymont | 3 | 5.9163453 | 5.8365392 | 5.9315032 | 1.348909 | 0.256204 | win |
| samba | 3 | 4.8589759 | 4.6394438 | 4.8920759 | 4.518073 | 0.681214 | win |
| sao | 3 | 2.7373200 | 2.6209532 | 2.8187985 | 4.251122 | 2.976579 | win |
| webster | 3 | 22.9825396 | 22.3885436 | 24.2180885 | 2.584553 | 5.376033 | win |
| xml | 3 | 0.6175851 | 0.6201734 | 0.6282796 | -0.419100 | 1.731664 | between |
| x-ray | 3 | 2.4003726 | 1.8683316 | 2.4430239 | 22.164934 | 1.776862 | win |

| Pass | Control 0 corpus s | Trial corpus s | Control 1 corpus s | Per-member faster controls s | Gain % | Unchanged decode corpus s |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 87.4557706 | 84.5136037 | 87.4466146 | 87.3235711 | 3.217880 | 6.9350082 |
| 2 | 87.6999160 | 84.4710604 | 87.6856888 | 87.4736385 | 3.432552 | 6.9402054 |
| 3 | 88.7621048 | 85.2833965 | 89.8510285 | 88.4029409 | 3.528779 | 6.9411028 |

| Member | Unchanged decode pass 1 s | Pass 2 s | Pass 3 s |
| --- | ---: | ---: | ---: |
| dickens | 0.3735030 | 0.3793417 | 0.3792580 |
| mozilla | 2.0105528 | 2.0081235 | 2.0123054 |
| mr | 0.3894345 | 0.3891109 | 0.3930139 |
| nci | 0.2736014 | 0.2724321 | 0.2770696 |
| ooffice | 0.3421012 | 0.3379241 | 0.3398934 |
| osdb | 0.3824197 | 0.3818272 | 0.3813005 |
| reymont | 0.1792267 | 0.1820321 | 0.1793871 |
| samba | 0.5278051 | 0.5266076 | 0.5202754 |
| sao | 0.5928226 | 0.5917413 | 0.5896688 |
| webster | 1.1381827 | 1.1363977 | 1.1446719 |
| xml | 0.0678573 | 0.0673457 | 0.0659376 |
| x-ray | 0.6575012 | 0.6673215 | 0.6583212 |

The predeclared screen fails: 29 wins, 6 losses and 1 between-controls cases; maximum control
spread 5.376033%. All 36 strict wins and every
control spread at most 2% are required. Gain versus the faster control ranges
from -2.122300% to
25.033770%. Aggregate gain does not prove
universal improvement, an optimal width or a cause of timing variation.

| Profile/direction | Maximum process peak working set bytes | Maximum process peak commit bytes |
| --- | ---: | ---: |
| control-0-encode | 318144512 | 388579328 |
| control-1-encode | 318144512 | 388567040 |
| bucket18-encode | 318930944 | 389341184 |
| bucket18-decode | 132780032 | 202854400 |

Whole-process peaks include input/archive/runtime storage. They are not codec-only
resident memory or substitutes for exact workspace charges. Keep the candidate
private; no automatic admission, public format/ABI/default/inventory/fixed-binary
change, new fuzz or external gate. Preserve BM-0202/BM-0203/BM-0204 failed screens
and their historical audit limitations. Specialized 1MiB width/noise records and
BM-0205 instrumented tokenizer shares remain distinct; do not infer a collision
forecast, subtract assumed timer overhead or claim an environmental cause.
All prior artifacts and fixed verification binaries remain intact.
