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
