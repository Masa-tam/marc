# Eight-MiB prepared owning encoder C boundary

DD-1466 / IR-1225 / TVG-1333 / CR-1637. This implements the encoder portion
of DD-1465. The public decoder, generic parser admission, CLI selector and
exchange inventory remain separate gates; the public codec is not complete.

## API and configuration

`marc_lzss_position_distance_dynamic_range_8m_config_init` initializes a
distinct encoder-only template. Strategy is explicitly
`MARC_LZSS_POSITION_DISTANCE_8M_PREPARED_OWNING` (1); all other values are
invalid. The initializer sets frame/window/block to 8388608, match limit to
258 and range total to 32768. Original size, external storage, input/output
capacities and all other exposed limits start at zero. Set the remaining limits
before querying or creating; a zero budget is invalid, never unlimited.

`marc_lzss_position_distance_dynamic_range_8m_resource_requirements` reports
external charge, fixed reservation, initial raw bytes, initial index entries and
initial total. Its result structure is
`marc_lzss_position_distance_dynamic_range_8m_resources`. Success sets scope
`MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY` (1). Failure preserves the entire
result, including metadata-overlap failure. Creation accepts no caller workspace
or allocator. It sets a valid disjoint handle-output pointer to null on failure;
overlap with config is rejected without modifying the aliased objects.

The configuration is copied during creation and need not remain alive. Original
size is concrete; no unknown-size sentinel is interpreted. Frame size must be
positive and at most 8388608. The unchanged profile uses window 8388608,
matches 3..258, dictionary variant 11, context algorithm 1/variant 12,
47 contexts, entropy 3/variant 2 and range total 32768. The semantic validator
requires table limit at least 2599 and the corresponding window, match and range
limits. A smaller frame leaves the wire identity unchanged. This adds no generic
profile alias and changes no smaller-window API, strategy or default.

All exposed limits pass the core validator. The non-exposed dictionary/Huffman
and block-count fields retain bounded core defaults, with no new tuning API.
Semantic or arithmetic resource refusal maps to LIMIT_EXCEEDED; invalid config
maps to INVALID_ARGUMENT. A real initial allocation failure maps to
OUT_OF_MEMORY. The zero-input, zero-output readiness probe publishes no bytes;
the handle is exposed only after successful initial construction and allocation.

## Logical resources and guards

The unchanged owning query calculates X=A+H+E+I+O and
R=W+C+K+X+I+O. E now includes declared external storage, the entire C handle,
entire public guard and public factory/query/process controls. The guard embeds
the adapter, so its complete size conservatively duplicates adapter storage;
no subtraction or overlapping-grant discount is performed.

Initial storage is R plus r raw bytes and, for nonempty input, four times
(65536+r) index bytes, where r=min(frame_size,original_size). Empty input needs
neither block. Full declared input/output capacities, including unused tails,
remain charged twice according to the qualified private contract. All arithmetic
is checked before size conversion or allocation. This accounting is a logical
contract; it is not RSS, allocator overhead or instrumentation overhead.

On the three qualified routes, concrete adapter size is 1360, its control
reserve 856, coordinator working charge 12740, allocator controls plus working
charge 912, public guard 1440, handle 8 and public controls 1256 bytes.
For external storage zero and call capacities 160/4096, public E=2704,
X=9176, R=27084. A 64-byte initial frame uses 65600 index entries and
initial total 289548. These are source-bound target results, not ABI constants
or universal architecture assumptions. Adding 13 input and 17 output capacity
bytes increases X by 30 and R by 60; adding 19 external bytes increases R by 19.

Creation success admits only initial storage. Every candidate generation needs
independent prospective admission alongside all retained previous blocks.
Blocks remain charged until real deletion. No fallback, retry, budget increase,
pool, or automatic operation strategy is added. The operation encoder remains
the measured speed baseline; this work contains no new speed measurement.

The public guard rejects input/output overlap with each other, the whole handle
or whole guard. The unchanged adapter/coordinator additionally checks declared
call capacities and all retained private regions. Rejection has zero counts and
becomes sticky. Terminal results take precedence for subsequently valid buffers
and preserve error category and positions. The generic C dispatcher still
rejects null nonempty buffers before dispatch, as for existing APIs.

Flush is neutral; ResetBlock and unknown flags are unsupported. Repeat EndInput
for an unconsumed final suffix. Only accepted input and committed output count.
The stream header and prior valid frames can remain published when a later
frame fails. No fragment of the failed frame is published; the unchanged owner
keeps the prior generation's contents, capacities and publication state and
really releases the candidate. Previously qualified private guarantees are
retained by delegation, not replaced with a new implementation.

## Qualification and its limits

Three routes (two optimized, one address/undefined-behavior instrumented) each
pass 41 ordinary and 84 isolated grouped C++ checks. All records agree across
routes: 375 grouped rows in six final C++ launches. Six additional launches pass
real C consumers against static and shared libraries. Each C++ launch checks
all 256 one-byte values and empty input, grouped in one row, with fresh private
decoders. A 160-byte, 64/64/32 frame fixture has seven literals and one match per
frame and matches the independently retained operation encoder's complete wire
under whole-call and one-byte input/output paths. Configuration can be destroyed
after creation, as exercised by the C consumers.

Checks cover invalid metadata, alias preservation, overflow and semantic limits,
full capacity/guard/handle overlaps, unsupported flags, sticky errors and end,
early end, extra input, initial exact/one-below budgets, initial-only deferred
refusal and block-limit refusal. The isolated suite discovers 17 typed delegated
allocations and independently refuses each; two scalar factory allocations are
also refused. Real receipt deletion reaches zero after destruction. All six
synthetic late postcondition modes run at fresh/replacement/tail frames with
mode-zero controls. They follow real helper success and do not establish every
original helper-error path. No public allocator seam exists in the library.

For the complete retained diagnostic fixture, fixed reservation is 394896,
actual block peak is 263252 and exact complete logical peak is 658148 bytes.
The exact budget succeeds; one below refuses the replacement before publication
and retains the earlier prefix. This is recipe-specific evidence, not a proposed
public default or maximum-frame worst-case estimate.

The first shared build exposed a missing internal range-decoder dependency,
which is now registered for token validation; it adds no public decoder. The
instrumented shared link required explicit compiler-driver runtime linkage;
unchanged bound objects were retained and linked to separate artifacts. A first
isolated test incorrectly expected LIMIT_EXCEEDED from a delegated null return;
the unchanged core correctly returned OUT_OF_MEMORY. Only that test expectation
was corrected before a separately bound final campaign. Earlier failed artifacts
and results remain retained and are not pooled into the final counts.

Production/static/shared source builds and exports are checked. The new ordinary
test and static/shared C consumers are registered in CMake. After removing
redundant source-list entries, the effective 421-source set is unchanged; a
fresh build from the final CMake passes all three newly registered CTests.
The first CTest filter also selected 26 unbuilt private targets; that incomplete
selection is preserved separately from the final exact three-target selection.
The standalone matrix and registered tests are separate evidence; this is not
a complete repository CTest run.
Maximum-frame public-boundary execution, exhaustive splits, decoder fuzzing,
new timings, CI and external exchange are not claimed. Prior maximum-frame
private qualification remains intact. Next DD-1467 defines the public decoder
workspace and admission contract before implementing it.
