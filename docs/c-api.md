# C API

The public C ABI is declared by `<marc/marc.h>`. It exposes the same forty-two
validated baseline profiles as the command-line tool: checksum-raw, six
standalone dictionary profiles, five standalone entropy profiles, and the
complete six-dictionary by five-entropy composition matrix. It additionally
exposes five experimental Format 2 LZSS contextual profiles: Dynamic Range,
rANS, tANS, Blocked Huffman, and Adaptive Huffman. Each has an explicit
experimental command-line option; none is part of the baseline 42-profile
matrix.
Encoding uses a known input size, and every transform uses bounded caller-owned
workspace.
All functions are `noexcept` in C++ translation units, and no C++ type appears
in the ABI.

The position-distance family provides the following public C factory integration:
`marc_lzss_position_distance_dynamic_range_config_init()`,
`marc_lzss_position_distance_dynamic_range_workspace_requirements()` and
`marc_lzss_position_distance_dynamic_range_create()`.
Its explicit command-line codec is `lzss-position-distance-dynamic-range`.
Its completion evidence is recorded separately from the baseline matrix in
`baseline-readiness.md`; schema 58 has passed external four-direction verification.
Its initializer needs no profile helper: window 65536 and matches
3..258 are fixed, frame_size defaults to 65536, and hard limits are configurable.
Decode ignores original_size/frame_size and sizes storage from local
max_frame_size capped at 65536. Query returns caller storage requirements,
checks aggregate usage including the handle/owner/model, rejects metadata
overlap and leaves output unchanged on error. Invalid metadata or inconsistent
limits return INVALID_ARGUMENT; insufficient coherent limits return
LIMIT_EXCEEDED. See the [integration contract](design/lzss-position-distance-streaming.md#public-integration-contract-planned-not-yet-admitted)
for the exact fields and defaults. Existing factories and profiles are unchanged.
The new factory retains only queried workspace prefixes, which must be disjoint
and views-aligned. It rejects config/retained-storage overlap and handle-output
overlap with config or supplied storage before mutating those objects. A disjoint
handle output becomes null on ordinary validation/allocation failure. Process
uses the generic contract; Flush preserves frames, ResetBlock is unsupported,
EndInput must be repeated with the final unconsumed suffix, and terminal states
are sticky. Decoding publishes complete validated frames, not an atomic stream.

## Profiles and composition

The C ABI exposes complete, validated stream profiles rather than separate
dictionary and entropy objects that callers combine at runtime. Each standalone
dictionary factory binds entropy `None`, each standalone entropy factory binds
dictionary `None`, and each composed factory fixes one exact dictionary and
entropy pairing. Runtime layer composition is intentionally not part of the
baseline ABI even though all thirty required pairings now have factories.

The [public-profile evidence matrix](baseline-readiness.md#public-profile-evidence-matrix)
records which complete factories have format, streaming, tooling, fuzz, and
completion coverage. It is the readiness record for the factories summarized
here; the declarations in `<marc/marc.h>` remain the normative ABI inventory.

## Lifecycle

### Common lifecycle

1. Call the selected profile's `marc_<profile>_config_init()` function for the
   encode or decode direction. The exact function declarations and associated
   configuration types are listed in `<marc/marc.h>`.
2. Set the desired encoder sizes or decoder hard limits.
3. Call the matching workspace-requirements function.
4. Allocate each reported workspace, respecting `views_alignment`.
5. Call the matching create function and retain every workspace unchanged until
   after `marc_transform_destroy()`.
6. Repeatedly call `marc_transform_process()`, advancing input and output only
   by the reported consumed and produced counts.
7. Destroy the handle. Destroying a null handle is valid.

The library owns the opaque handle. It does not own the three workspaces or any
input/output buffer. No allocator callback is required by these profiles.

An `apply_profile` function, when declared for a configuration, is reserved for
a codec with two or more named resource profiles. A configuration without a
declared profile helper is not incomplete: its `config_init()` result is a
complete usable default, and the caller may apply documented field overrides
before querying workspace requirements. The absence of a profile helper does
not imply reduced codec support.

For either encoder, `primary_bytes` is raw-frame storage and `secondary_bytes`
is serialized-frame storage. For either decoder, `primary_bytes` is serialized-
frame storage and `secondary_bytes` is decoded-frame storage. Blocked Huffman
decoding additionally uses `views_bytes` for a private block table; Adaptive
Huffman requires no views workspace. Adaptive encoder requirements
conservatively allow 264 bits per input symbol before fixed frame overhead.
Dynamic Range also requires no views workspace; its encoder reserves at most
two normalization bytes per input symbol plus five termination bytes.
rANS decoding uses `views_bytes` for its validated block descriptors. Its
encoder reserves at most one renormalization byte per input symbol plus an
eight-byte state and fixed descriptor for every entropy block.
tANS likewise uses aligned decoder views; its encoder workspace uses the strict
12-bit-per-symbol transition bound plus a two-byte state per block.

### LZ77 profiles

LZ77 uses no views workspace. Its encoder buffers one raw frame and the
conservative fixed-token representation; its decoder buffers one encoded frame
and one validated decoded frame.
The LZ77 plus Blocked Huffman profile keeps the common three-workspace ABI.
Its primary region holds raw input while encoding and serialized input while
decoding. Its secondary region is opaque to callers and is internally
partitioned into dictionary staging followed by encoded-frame staging for the
encoder, or dictionary staging followed by raw-frame staging for the decoder.
Only decoding uses the aligned views region, for validated entropy block
descriptors. Query requirements again whenever any size or limit changes.
The LZ77 plus Adaptive Huffman profile needs no views workspace. Its primary
region holds raw-frame input while encoding and serialized-frame input while
decoding. Its secondary region contains canonical LZ77-token staging followed
by serialized-frame staging for encode, or token staging followed by private
raw-frame staging for decode. Every outer frame owns one reset FGK tree, so the
configuration has no entropy-block size. Query requirements again after
changing any frame, LZ77 parameter, original size, or local limit.
The LZ77 plus Dynamic Range profile uses the same two byte workspaces and no
views region. Encoding partitions secondary storage into canonical LZ77 tokens
followed by the complete range-coded frame; decoding partitions it into token
staging followed by private raw staging. Query requirements again after
changing direction, original size, frame size, LZ77 parameters, or any local
limit. Factory failure leaves the transform pointer null.
The LZ77 plus rANS profile retains the common three-workspace ABI. Encoding
uses primary for raw-frame collection, partitions secondary into canonical
LZ77 tokens and the complete rANS frame, and reports no views. Decoding uses
primary for the serialized frame, partitions secondary into token and private
raw staging, and uses aligned opaque views for validated rANS block
descriptors. Query requirements again after changing either frame dimension,
direction, original size, LZ77 parameters, or any local limit. The public
header exposes only byte counts and alignment, never `RansBlockView`.
The LZ77 plus tANS profile has the same three-region contract. Encoding
partitions secondary storage into canonical LZ77 tokens followed by the
complete tANS frame and requires no views. Decoding uses primary for the
serialized frame, partitions secondary into private token and raw staging,
and receives aligned opaque tANS views in the third region. The requirements
query must be repeated after changing direction, either frame dimension,
original size, LZ77 parameters, or any local limit. Factory failure leaves the
transform pointer null, and the header never exposes `TansBlockView`.

### LZSS profiles

LZSS also uses no views workspace. Its encoder's exact worst-case token payload
is two bytes per raw byte. Encoding reserves primary for one raw frame and
partitions secondary into an internally aligned exact HashChain match-finder
region followed by the complete encoded frame. Callers must query requirements
again after changing size, LZSS parameters, or limits; the reported alignment
allowance makes an otherwise unaligned secondary pointer safe. Its decoder uses
the same frame-atomic workspace roles as LZ77.
The LZSS plus Blocked Huffman factory keeps the same three-region convention as
the LZ77 composition. Its secondary region contains token staging followed by
serialized-frame staging while encoding, or token staging followed by raw
staging while decoding. The opaque aligned views region contains the exact
HashChain match-finder workspace while encoding and entropy-block views while
decoding; neither private representation crosses the ABI. Finder selection is
not stream metadata and does not change decoder requirements.
Call `marc_lzss_blocked_huffman_workspace_requirements()` again after changing
any size, LZSS parameter, or local limit.
The LZSS plus Adaptive Huffman factory uses the same primary and secondary
roles without a views region. Encoding partitions secondary storage into
an internally aligned exact HashChain finder, canonical LZSS token staging,
and the complete serialized frame; the workspace query includes the maximum
alignment padding needed for an otherwise unaligned secondary pointer.
Decoding partitions secondary storage into token staging followed by private
raw staging.
Each outer frame resets both LZSS history and its one FGK tree. Call
`marc_lzss_adaptive_huffman_workspace_requirements()` again after changing the
direction, known original size, frame size, LZSS parameters, or any hard limit.
The LZSS plus Dynamic Range factory has the same byte-only ownership and no
views region. Encoding partitions secondary storage into an internally aligned
exact HashChain finder, canonical LZSS token staging, and one complete range-
coded frame; the workspace query includes maximum alignment padding for an
otherwise unaligned secondary pointer. Decoding partitions it into token
staging and private raw staging. The configuration fixes Dynamic Range variant
1 and has no entropy-block parameter. Call
`marc_lzss_dynamic_range_workspace_requirements()` again after changing the
direction, known original size, frame size, LZSS parameters, or any hard limit.
Creation failure leaves the caller's transform pointer null.
The experimental LZSS contextual Dynamic Range factory is a separate Format 2
lifecycle, not an alias for the preceding byte-oriented profile. Call
`marc_lzss_contextual_dynamic_range_workspace_requirements()` for the selected
immutable direction. Encoding uses primary for raw-frame input, secondary for
the complete serialized frame, and aligned opaque views for typed tokens,
modeled operations, and the selected exact match-finder workspace. Decoding
uses primary for serialized input, secondary for
atomic raw-frame output, and views for typed tokens. The factory validates
capacity, alignment, and pairwise non-overlap before publishing a handle.
Encoder sizes and LZSS parameters are read from the size-tagged configuration;
decoder workspace sizing comes only from its hard limits and validates stream
parameters later. `profile` selects one exact dictionary/context pair:
`MARC_LZSS_CONTEXTUAL_PROFILE_64K` selects `2/2 + 1/1` and remains the
initializer default, while `MARC_LZSS_CONTEXTUAL_PROFILE_1M` selects
`2/3 + 1/2`. `MARC_LZSS_CONTEXTUAL_PROFILE_4M` selects `2/4 + 1/3`, and
`MARC_LZSS_CONTEXTUAL_PROFILE_16M` selects `2/5 + 1/4`, only for this Dynamic
Range factory. `MARC_LZSS_CONTEXTUAL_PROFILE_64M` has value 4 and selects
`2/6 + 1/5`, also only for this factory. The selector is not inferred from
`window_size`;
encoding rejects parameters outside the selected profile and decoding rejects
a stream whose
identity does not match it. Re-query all three workspace regions after
changing the selector. Use
`marc_lzss_contextual_dynamic_range_config_apply_profile()` to apply the
selected frame, dictionary, payload, model, and aggregate limits atomically;
it preserves direction, original size, and the caller's total-output limit.
The four-MiB preset raises `max_internal_buffered_bytes` to 256 MiB, which
covers its 264,765,525-byte encoder requirement on the supported 64-bit native
layouts. Its decoder also receives the required `max_block_size` of
4,194,304 bytes. The 16-MiB preset applies a 234,881,029-byte payload ceiling,
4,582 model entries, and a one-GiB aggregate policy. On the supported 64-bit
native layout, the authoritative workspace query returns exactly
1,057,488,981 bytes for encoding and 452,984,917 bytes for decoding; one byte
less fails. The 64-MiB preset applies a 1,073,741,829-byte payload ceiling,
4,598 model entries, and an eight-GiB aggregate policy. Its exact HashChain,
BinaryTree, and decoder workspace requirements are 4,362,600,533,
6,039,797,845, and 1,946,157,141 bytes respectively on supported 64-bit
layouts. A caller may tighten any returned hard limit and must then re-query
before allocation. All five contextual entropy factories admit the 64-MiB
selector through their independently completed lifecycles. The field and its
trailing
32-bit reserved word occupy
the former 64-bit reserved tail, preserving the ABI-1 structure extent and the
all-zero meaning used by earlier callers. Exact CLI and benchmark name
`lzss-contextual-dynamic-range-64m` now selects the same helper. Bounded fuzz
now exercises the same public selector under fixed one-KiB frame storage;
interoperability schema 53 exercises the same profile as archive 63 without
adding an ABI or serialized selector field.
The experimental LZSS contextual rANS factory is a distinct Format 2
lifecycle. Call `marc_lzss_contextual_rans_workspace_requirements()` after
changing direction, known size, frame/LZSS parameters, `profile`, or
hard limits.
Encoding uses primary for raw-frame input, secondary for the complete
serialized frame, and aligned opaque views for typed tokens followed by the
selected exact match-finder workspace. Decoding uses
primary for serialized input, secondary for atomic raw output, and views for
the contextual-rANS tables followed by typed tokens. The factory checks
capacity, alignment, pairwise non-overlap, and the private partition before
publishing a handle. No token or rANS table structure is exposed in the C ABI.
Its public completion audit covers all required binary classes, deterministic
one-byte and mixed chunk schedules, repeated terminal calls, and frame-atomic
malformed final-frame rejection without promoting it into the baseline matrix.
This canonical lifecycle emits only variable-length entropy variant 3.
`MARC_LZSS_CONTEXTUAL_PROFILE_64K` is the initializer default, selects
dictionary/context `2/2 + 1/1`, and uses the 9,025-byte descriptor ceiling.
`MARC_LZSS_CONTEXTUAL_PROFILE_1M` selects `2/3 + 1/2` and uses the 9,089-byte
ceiling. `MARC_LZSS_CONTEXTUAL_PROFILE_4M` selects `2/4 + 1/3`, uses the
9,121-byte ceiling, and retains the 128-MiB aggregate default. On supported
64-bit native layouts its full encoder and decoder requirements are
130,556,905 and 114,017,257 bytes; full-frame callers must raise
`max_frame_size` to 4,194,304 bytes and `max_block_size` to the
29,360,128-decision ceiling. The
`marc_lzss_contextual_rans_config_apply_profile()` helper applies those
values, the payload and table limits, and the 128-MiB aggregate policy as one
atomic preset while preserving caller-specific fields. The selector is not inferred from
`window_size`; encoding validates
parameters against it and public decoding rejects the other profile before
frame allocation. The field and trailing 32-bit reserved word retain the
former 64-bit tail's ABI-1 extent and all-zero meaning. Entropy variant 2 is
retired and reserved; the decoder rejects it.

`MARC_LZSS_CONTEXTUAL_PROFILE_16M` selects `2/5 + 1/4`, uses the 9,153-byte
descriptor ceiling, `7F = 117,440,512`, `14F + 8 = 234,881,032`, and a
512-MiB aggregate policy. On supported 64-bit layouts the authoritative full-
frame query reports encoder regions 16,777,216 / 234,890,249 / 268,959,744
bytes and decoder regions 234,890,249 / 16,777,216 / 202,088,448 bytes. The
explicit `lzss-contextual-rans-16m` CLI name uses only this helper, query, and
factory; the matching dependency-free benchmark uses the same public
lifecycle and reports the query-owned allocations. Neither tool alters ABI 1
nor infers the profile from stream fields. Bounded decoder fuzzing and the
schema-49 archive exercise the same public profile without changing the C ABI.
`MARC_LZSS_CONTEXTUAL_PROFILE_64M` selects `2/6 + 1/5`, uses the 9,185-byte
descriptor ceiling, `8F = 536,870,912`, `16F + 8 = 1,073,741,832`, and a
four-GiB aggregate policy. On supported 64-bit layouts, the authoritative
HashChain, BinaryTree, and decoder aggregate requirements are 2,215,126,057,
3,892,323,369, and 1,946,928,169 bytes. The helper preserves direction,
original size, total-output policy, and the selected Exact finder; the query
returns the selected finder's actual allocation and rejects each aggregate at
one byte short. The initializer remains 64 KiB, unknown profiles leave the
configuration unchanged, and encoded stream fields never enlarge caller-local
limits. Exact CLI and benchmark name `lzss-contextual-rans-64m` selects this
same helper and query without duplicating the numeric policy. The bounded
decoder fuzzer admits the same profile identity while retaining one-KiB local
frame/token/raw storage; it does not invoke a full-profile allocation.
The interoperability schema 54 exercises the same public profile as archive 64
without changing this ABI. Resource-helper names remain unassigned.
The experimental LZSS contextual tANS factory is a third distinct Format 2
lifecycle. Call `marc_lzss_contextual_tans_workspace_requirements()` whenever
the immutable direction, known size, frame/LZSS parameters, `profile`,
or hard limits change. Encoding uses primary for raw-frame input, secondary
for the complete serialized frame, and aligned opaque views for typed tokens
followed by tANS inverse tables. Decoding uses primary for serialized input,
secondary for atomic raw output, and views for fixed tANS decode tables
followed by typed tokens. The factory checks capacity, alignment, pairwise
non-overlap, and the private partition before publishing a handle. It emits
only entropy identity `5/2`; neither typed-token nor table representations form
part of ABI 1.

`MARC_LZSS_CONTEXTUAL_PROFILE_64K` remains the initializer default and selects
dictionary/context identity `2/2 + 1/1`.
`MARC_LZSS_CONTEXTUAL_PROFILE_1M` selects `2/3 + 1/2`, and
`MARC_LZSS_CONTEXTUAL_PROFILE_4M` selects `2/4 + 1/3`.
`MARC_LZSS_CONTEXTUAL_PROFILE_16M` selects `2/5 + 1/4`; this identity uses the
9,157-byte descriptor ceiling, `7F = 117,440,512` as its decision/block limit,
a 176,160,770-byte payload limit, and a 512-MiB aggregate default. The
four-MiB profile retains its 9,125-byte descriptor ceiling and 128-MiB
aggregate default.
Call `marc_lzss_contextual_tans_config_apply_profile()` to apply the selected
frame, decision, payload, fixed-table, LZ, and aggregate limits atomically.
The helper validates before mutation and preserves direction, original size,
and the caller's total-output policy; callers may tighten individual hard
limits before re-querying all workspaces.
On supported 64-bit native layouts its full encoder and decoder requirements
are 116,138,983 and 99,099,623 bytes; full-frame callers set
`max_frame_size` to 4,194,304 and the common `max_block_size` decision limit
to 29,360,128. For the full 16-MiB profile the corresponding encoder and
decoder requirements are 462,169,095 and 394,798,087 bytes; their regions are
16,777,216 / 176,169,991 / 269,221,888 bytes and
176,169,991 / 16,777,216 / 201,850,880 bytes respectively. The selector is not
inferred from `window_size`: encoding validates the selected parameters and
decoding rejects every other known identity before frame collection or raw
publication. The selector and trailing reserved word reuse the former 64-bit
reserved tail, preserving the 112-byte ABI-1 extent and the all-zero default.
`MARC_LZSS_CONTEXTUAL_PROFILE_64M` now admits exact identity
`2/6 + 1/5 + 5/2` through this Contextual tANS factory only. Its helper applies
a 67,108,864-byte frame/window/distance, `8F = 536,870,912` decision limit,
805,306,370-byte payload ceiling, 131,072 table entries, and four-GiB
aggregate policy while preserving direction, original size, total-output
policy, and the selected Exact finder. On supported 64-bit layouts the exact
HashChain, BinaryTree, and decoder aggregate requirements are 1,946,952,743,
3,624,150,055, and 1,678,255,143 bytes; the query reports the selected
allocation and rejects each limit one byte short. Initializers remain 64 KiB,
unknown profiles do not mutate the configuration, callers may tighten limits
after applying the helper, and stream fields never enlarge local policy.
The explicit `lzss-contextual-tans-16m` and `lzss-contextual-tans-64m` CLI and
dependency-free benchmark names use this same helper/query/factory lifecycle
without duplicating private layout arithmetic. The 64-MiB applications select
only public profile value 4 and never infer it from stream fields. The
schema-50 archive exercises the 16-MiB profile without changing the C ABI;
schema 55 exercises the 64-MiB profile as archive 65 without changing the C
ABI.

The completion audit covers all required binary classes, deterministic mixed
and one-byte chunk schedules, stable repeated terminal calls, and frame-atomic
rejection of corrupted, truncated, or trailing final-frame data.
The experimental LZSS Contextual Blocked Huffman factory is a fourth distinct
Format 2 lifecycle. Initialize its size-tagged configuration with
`marc_lzss_contextual_blocked_huffman_config_init()`, repeat
`marc_lzss_contextual_blocked_huffman_workspace_requirements()` whenever the
immutable direction, known size, frame/LZSS parameters, `profile`, or
hard limits change, and give all three returned regions to the factory.
Encoding uses primary for
raw-frame input, secondary for the complete serialized frame, and aligned
opaque views for typed tokens. Decoding uses primary for serialized input,
secondary for atomic raw output, and views for at most 35 bounded Huffman
decode tables followed by typed tokens. Capacity, alignment, and pairwise
prefix non-overlap are checked before a handle is published.
`MARC_LZSS_CONTEXTUAL_PROFILE_64K` remains the initializer default and selects
`2/2 + 1/1 + 2/2`; `MARC_LZSS_CONTEXTUAL_PROFILE_1M` selects
`2/3 + 1/2 + 2/2`; `MARC_LZSS_CONTEXTUAL_PROFILE_4M` selects
`2/4 + 1/3 + 2/2`; and `MARC_LZSS_CONTEXTUAL_PROFILE_16M` selects
`2/5 + 1/4 + 2/2`. The four-MiB profile uses `7F = 29,360,128` as its
decision/block limit and a 55,050,240-byte payload limit. The sixteen-MiB
profile uses `7F = 117,440,512`, a 220,200,960-byte payload limit, and a
512-MiB aggregate policy.
`marc_lzss_contextual_blocked_huffman_config_apply_profile()` applies
the selected frame, decision, payload, 35-table/17,885-node, LZ, and aggregate
limits as one atomic preset. It preserves direction, original size, and the
caller's total-output policy, and callers may tighten hard limits before
re-querying all workspaces. On supported 64-bit layouts its full encoder and
decoder aggregate requirements are 126,880,348
and 109,722,064 bytes, both within the unchanged 128-MiB default. The selector
is exact rather than inferred from `window_size`, and decoding rejects either
other identity before frame or raw publication. It and the trailing 32-bit
reserved word reuse the former 64-bit reserved tail, preserving the 112-byte
ABI-1 extent and all-zero legacy meaning. No C++ token or table layout crosses
the ABI. Benchmark, bounded fuzzing, and schema-46 interoperability admission
are complete. Its public completion
audit covers the required binary classes, deterministic whole and mixed chunk
schedules, stable repeated terminal calls, and frame-atomic rejection of a
corrupted, truncated, or trailing final frame.

For the sixteen-MiB selection, the authoritative supported-layout encoder
query returns primary/secondary/views extents 16,777,216 / 220,203,621 /
268,959,744 bytes, aggregate 505,940,581. The decoder returns
220,203,621 / 16,777,216 / 201,469,812 bytes, aggregate 438,450,649. Equality
succeeds and one byte short fails before requirements or a handle are
published. Applying the helper twice is byte-identical, unknown selectors do
not change the configuration, and a four-MiB decoder rejects the sixteen-MiB
identity before publishing raw bytes. The schema-51 archive exercises this
same public profile without adding a serialized selector or ABI field.
The experimental LZSS Contextual Adaptive Huffman factory is another distinct
Format 2 lifecycle. Initialize its size-tagged configuration with
`marc_lzss_contextual_adaptive_huffman_config_init()`, then call
`marc_lzss_contextual_adaptive_huffman_workspace_requirements()` again after
changing direction, known size, frame/LZSS parameters, or hard limits.
`MARC_LZSS_CONTEXTUAL_PROFILE_64K`, `_1M`, and `_4M` select exact identities
`2/2 + 1/1 + 1/2`, `2/3 + 1/2 + 1/2`, and `2/4 + 1/3 + 1/2` respectively.
After initialization, callers may apply a coherent preset with
`marc_lzss_contextual_adaptive_huffman_config_apply_profile()` and
then override individual values before querying workspaces. The helper
allocates nothing, validates the complete ABI shell before mutation, preserves
direction, original size, total-output limit, metadata, and reserved zeros,
and leaves every byte unchanged on failure. The four-MiB preset uses a
139,984,896-byte payload ceiling, 13,729 entropy entries, and a 256-MiB
aggregate limit.
Encoding uses primary for raw-frame input, secondary for one retained
serialized frame, and aligned opaque views for tokens, FGK nodes, then symbol
indices. Decoding uses primary for serialized input, secondary for atomic raw
output, and views for nodes, symbols, then tokens. Capacity, alignment, and
pairwise used-prefix overlap are validated before handle publication. The
additive ABI-1 family emits only the explicitly selected identity; no typed
C++ layout crosses the ABI.
Its four-MiB identity is covered by the fixed-memory dual-path decoder harness
and schema-47 interoperability bundle without adding an ABI or serialized
selector. `MARC_LZSS_CONTEXTUAL_PROFILE_16M` selects exact identity
`2/5 + 1/4 + 1/2`, 16,777,216-byte frame/window/distance limits, the
559,939,584-byte payload ceiling, 13,777 entropy entries, and a one-GiB
aggregate policy. Its exact full-profile encoder and decoder workspace totals
are 845,832,912 and 778,199,756 bytes on supported 64-bit layouts. The
schema-52 archive exercises this same public profile without adding an ABI or
serialized selector.
`MARC_LZSS_CONTEXTUAL_PROFILE_64M` selects exact identity
`2/6 + 1/5 + 1/2`, 67,108,864-byte frame/window/distance limits, the
2,239,758,336-byte payload ceiling, 13,825 entropy entries, and an eight-GiB
aggregate policy. On supported 64-bit layouts, its HashChain Exact,
BinaryTree Exact, and decoder workspace totals are 3,381,290,224,
5,058,487,536, and 3,112,330,476 bytes. Applying the helper twice is
byte-identical; an unknown selector leaves the configuration unchanged, and
a 16-MiB decoder rejects the 64-MiB identity before publishing raw bytes.
The initializer remains on the 64-KiB profile and stream metadata never raises
local limits. The separately admitted CLI and benchmark use this same public
lifecycle; no additional C API surface is implied.
Its public completion audit covers all required binary classes, deterministic
whole, one-byte, and mixed chunk schedules, stable repeated terminal calls,
and frame-atomic rejection of corrupted, truncated, or trailing final-frame
data.
The LZSS plus rANS factory uses the common three-region convention. Encoding
uses primary for raw-frame collection, partitions secondary into alignment
allowance, exact HashChain finder storage, canonical LZSS tokens, and one
complete rANS frame, and reports zero views. Decoding uses
primary for the serialized frame, partitions secondary into token and private
raw staging, and receives aligned opaque rANS block views. Call
`marc_lzss_rans_workspace_requirements()` again after changing direction,
known original size, either block dimension, LZSS parameters, or any hard
limit. Finder shortage fails before frame publication; its private layout does
not cross the ABI. The public header exposes only byte counts and alignment.
The LZSS plus tANS factory follows the same three-region ownership policy.
Encoding uses primary for raw-frame collection, partitions secondary into
alignment allowance, exact HashChain finder storage, canonical LZSS tokens,
and one complete tANS frame, and reports zero views. Decoding uses primary for
the serialized frame, partitions secondary into token and private raw staging,
and receives aligned opaque tANS block views.
Call `marc_lzss_tans_workspace_requirements()` again after changing direction,
known original size, either block dimension, LZSS parameters, or any hard
limit. Finder shortage fails before frame publication; its private layout does
not cross the ABI. The public header exposes only byte counts and alignment.

### LZ78 profiles

LZ78 uses `views_workspace` as an aligned, opaque phrase table. Its encoder
reserves one eight-byte token and at most one phrase record per raw byte; its
decoder derives the payload and phrase capacities jointly from trusted local
limits. The requirements query supplies direction-specific `views_bytes` and
`views_alignment`; no private C++ record layout appears in the public ABI.
The LZ78 plus Blocked Huffman factory retains that opaque convention while
adding entropy views on decode. Its secondary region contains token staging
followed by serialized-frame staging for encode, or token staging followed by
raw-frame staging for decode. The aligned views region contains encoder phrase
entries in the first direction and a checked block-view/padding/phrase-entry
layout in the second. Only the internal partition helpers know these C++
layouts; callers must allocate exactly from
`marc_lz78_blocked_huffman_workspace_requirements()` and keep the region
unchanged for the transform lifetime.
The LZ78 plus Adaptive Huffman factory uses the same three-region ownership
model without entropy block views. Secondary storage contains canonical LZ78
token staging followed by the complete serialized frame while encoding, or
token staging followed by private raw staging while decoding. The opaque
aligned views region contains only encoder entries in the first direction and
only phrase entries in the second. Call
`marc_lz78_adaptive_huffman_workspace_requirements()` again after changing the
direction, known original size, frame or entry bounds, or any local limit.
The LZ78 plus Dynamic Range factory has the same three-region ownership and
opaque aligned LZ78 record policy. Its secondary encode region contains
canonical token staging followed by the complete range-coded frame; its decode
region contains token staging followed by private raw staging. Query
`marc_lz78_dynamic_range_workspace_requirements()` again after changing
direction, known original size, frame or entry bounds, or any local limit.
The LZ78 plus rANS factory retains this opaque three-region policy while
adding entropy views in the decode direction. Encoding uses primary for raw
frame collection, secondary for canonical LZ78 tokens followed by the complete
rANS frame, and aligned views for encoder records. Decoding uses primary for
the encoded frame, secondary for token staging followed by private raw
staging, and one aligned opaque views region containing rANS block views
followed by LZ78 phrase records. Call
`marc_lz78_rans_workspace_requirements()` again after changing direction,
known original size, either block dimension, maximum entries, or any hard
limit. Because entropy blocks operate on expanded token bytes, a local
`max_frame_size` must also admit the configured entropy block size.
The LZ78 plus tANS factory uses the same three-region contract with tANS block
views in the decoder's aligned opaque region. Encoding uses primary for raw
frame collection, secondary for canonical LZ78 tokens followed by the complete
tANS frame, and aligned views for encoder records. Decoding uses primary for
the encoded frame, secondary for token staging followed by private raw staging,
and aligned views for tANS block views followed by LZ78 phrase records. Call
`marc_lz78_tans_workspace_requirements()` again after changing direction,
known original size, either block dimension, maximum entries, or any hard
limit. The factory rejects short or misaligned regions and publishes no handle
on failure.

### LZW profiles

LZW uses the same opaque aligned-workspace convention. Its encoder requirements
use the configured maximum code width and frame size; decoder requirements use
only trusted local limits and conservatively cover any permitted serialized
LZW parameter width. `maximum_code_width` affects encoding only because decode
parameters are read from the stream and checked against local policy.
The LZW plus Blocked Huffman factory retains the three-region composition
contract. Its secondary region contains packed LZW staging followed by the
serialized frame for encode, or packed staging followed by transactional raw
output for decode. The aligned views region contains encoder dictionary entries
in the first direction and a checked entropy-view/padding/phrase-entry layout
in the second. Query `marc_lzw_blocked_huffman_workspace_requirements()` after
changing any code width, block size, frame size, or hard limit.
The LZW plus Adaptive Huffman factory uses the same packed-byte secondary
layout without entropy block views. The aligned opaque region contains only
encoder dictionary entries while encoding and only phrase entries while
decoding. Query `marc_lzw_adaptive_huffman_workspace_requirements()` again
after changing direction, original size, frame size, maximum code width, or a
local limit; decode sizing is derived solely from trusted local limits and the
stream parameters are validated later.
The LZW plus Dynamic Range factory retains the same opaque three-region
ownership. Encoding uses packed LZW staging followed by one complete
range-coded frame; decoding uses packed staging followed by private raw
staging. The aligned views region contains only encoder entries or only phrase
entries according to the immutable direction. Query
`marc_lzw_dynamic_range_workspace_requirements()` again after changing
direction, original size, frame size, maximum code width, or any local limit.

The LZW plus rANS factory uses the same three-region ownership with an explicit
entropy block size and maximum block count. Encoding uses primary storage for
one raw frame, secondary storage for packed LZW bytes followed by one complete
rANS frame, and aligned opaque views for LZW encoder entries. Decoding uses
primary storage for one encoded frame, secondary storage for packed bytes
followed by private raw staging, and aligned opaque views containing rANS block
views followed by LZW phrase entries. Call
`marc_lzw_rans_workspace_requirements()` again after changing direction,
original size, frame size, entropy block size, maximum code width, or any hard
limit. Private C++ record definitions never enter the C ABI.

The LZW plus tANS factory uses the same three-region ownership and explicit
block controls. Encoding places one raw frame in primary storage, then packed
LZW bytes and one complete tANS frame in secondary storage; aligned views hold
LZW encoder entries. Decoding places one encoded frame in primary storage,
then packed bytes and private raw staging in secondary storage; aligned views
hold tANS block views followed by LZW phrase entries. Call
`marc_lzw_tans_workspace_requirements()` again after changing direction,
sizes, width, block settings, or any hard limit. All typed layouts remain
private to the factory.

### LZD profiles

LZD also uses one opaque aligned views workspace. Encoding uses it for the
input-backed phrase table. Decoding partitions it internally into the phrase
records and bounded iterative expansion stack; the partition and both private
C++ record layouts remain outside the ABI. Encoder requirements use the known
original size and frame size, while decoder requirements derive every region
solely from trusted local payload, frame, entry, and aggregate-buffer limits.
The LZD plus Blocked Huffman factory keeps token staging followed by serialized
frame storage in the secondary encoder region, and token staging followed by
transactional raw output in the secondary decoder region. Its aligned views
region contains encoder entries or a checked block-view/phrase-entry/expansion-
stack layout. Query `marc_lzd_blocked_huffman_workspace_requirements()` after
changing any entry, block, frame, or hard limit; none of those private C++
record layouts is part of the ABI.
The LZD plus Adaptive Huffman factory retains the same primary and secondary
roles without entropy block views. Its aligned opaque region contains encoder
entries while encoding and a checked phrase-entry/expansion-stack layout while
decoding. Call `marc_lzd_adaptive_huffman_workspace_requirements()` again
after changing direction, original size, frame size, maximum entries, or any
hard limit; decode sizing is derived only from trusted local limits.
The LZD plus Dynamic Range factory retains the same three-region ownership.
Encoding uses token staging followed by one complete range-coded frame;
decoding uses token staging followed by private raw staging. Its aligned opaque
region contains encoder entries or a checked phrase-entry/expansion-stack
layout. Call `marc_lzd_dynamic_range_workspace_requirements()` again after
changing direction, original size, frame size, maximum entries, or any hard
limit; no C++ record type crosses the ABI.
The LZD plus rANS factory adds an explicit entropy block size and block-count
limit to the LZD profile. Encoding stores one raw frame in primary storage,
canonical eight-byte LZD tokens followed by one complete rANS frame in
secondary storage, and LZD encoder entries in aligned opaque views. Decoding
stores one encoded frame in primary storage, token staging followed by private
raw staging in secondary storage, and rANS views followed by aligned phrase and
iterative expansion regions in opaque views. Call
`marc_lzd_rans_workspace_requirements()` again after changing direction,
original size, frame size, entropy block size, maximum entries, or any hard
limit. The internal record types and partition offsets do not cross the ABI.
The LZD plus tANS factory preserves the same three-region contract with tANS
block metadata. Encoding stores one raw frame in primary storage, canonical
eight-byte LZD tokens followed by one complete tANS frame in secondary storage,
and LZD encoder entries in aligned opaque views. Decoding stores one serialized
frame in primary storage, private token and raw staging in secondary storage,
and tANS block views followed by aligned LZD phrase and expansion regions in
opaque views. Call `marc_lzd_tans_workspace_requirements()` again after
changing direction, original size, either block dimension, maximum entries, or
any hard limit. The public header exposes only fixed-width configuration,
byte counts, and alignment; no C++ record or partition offset crosses the ABI.

### LZMW profiles

LZMW follows the same opaque aligned-workspace ownership model. Its encoder
stores input-backed phrase spans; its decoder partitions the region into fixed
reference phrase records and an iterative expansion stack. All extents are
queried through `marc_lzmw_workspace_requirements()` before factory creation.
The LZMW plus Blocked Huffman factory adds entropy block views to the decoder's
opaque layout while retaining phrase records and the iterative expansion
stack. Its secondary encoder region contains canonical four-byte reference
staging followed by serialized-frame storage; the decoder region contains
reference staging followed by transactional raw output. Query
`marc_lzmw_blocked_huffman_workspace_requirements()` whenever an entry, frame,
entropy-block, or hard limit changes.
The LZMW plus Adaptive Huffman factory retains the same reference and raw/frame
secondary regions without entropy block views. Its aligned opaque region holds
encoder entries or the decoder's checked phrase-entry/expansion-stack layout.
Query `marc_lzmw_adaptive_huffman_workspace_requirements()` again whenever the
direction, known original size, frame size, maximum entries, or a hard limit
changes.
The LZMW plus Dynamic Range factory uses the same canonical-reference and
raw/frame secondary regions. Its aligned opaque region holds encoder entries
or the decoder's checked phrase-entry/expansion-stack layout. Query
`marc_lzmw_dynamic_range_workspace_requirements()` again whenever the
direction, known original size, frame size, maximum entries, or a hard limit
changes. No C++ record type crosses this ABI.
The LZMW plus rANS factory follows the same three-region ownership model while
adding rANS block views to the decoder's aligned opaque layout. Query
`marc_lzmw_rans_workspace_requirements()` whenever direction, known original
size, frame size, entropy block size, maximum entries, or any hard limit
changes.
The LZMW plus tANS factory has the same ownership contract with tANS block
views in its aligned opaque region. Query
`marc_lzmw_tans_workspace_requirements()` whenever direction, known original
size, frame size, entropy block size, maximum entries, or any hard limit
changes.

## Processing contract

`MARC_STATUS_PROGRESS` always consumes input or produces output.
`MARC_STATUS_NEED_INPUT` requests more input. `MARC_STATUS_NEED_OUTPUT` means
pending output could not fit. In both cases, re-present any unconsumed input
suffix. Zero produced bytes do not imply end-of-stream.

Set `MARC_PROCESS_END_INPUT` only when the supplied span contains the final
remaining input. If output pressure prevents that span from being consumed,
re-present its suffix with `MARC_PROCESS_END_INPUT` still set. Completion occurs
only at `MARC_STATUS_END_OF_STREAM`; later calls return the same status.

Non-terminal `MARC_PROCESS_FLUSH` does not shorten a configured outer frame.
`MARC_PROCESS_RESET_BLOCK` is currently unsupported by these public profiles.

Errors are terminal for a transform and use stable public categories. A decoder
may already have committed earlier validated frames when a later frame is
malformed. The malformed frame itself produces no output.

## Configuration rules

Do not initialize configuration structures manually. The initializer fills
`struct_size`, `abi_version`, defaults, and reserved fields. Changing tags or
reserved fields is invalid. The five Contextual LZSS configurations are the
exception: their former 32-bit field after `direction` is the documented
`match_finder_strategy` selector. Encoder `original_size` is mandatory format
input; unknown-size encoding is outside the baseline profile.

Decoder limits are local policy, not values accepted from the stream. Smaller
limits reduce workspace requirements and the accepted attack surface. The
defaults are conservative but can request substantial workspace, particularly
the 128 MiB maximum buffered frame body.

See [`../examples/c_roundtrip.c`](../examples/c_roundtrip.c) for a complete
single-call round trip. Real streaming callers should also handle partial
consumption and production as described above.

### Installed streaming example

[`position_distance_roundtrip.c`](../examples/position_distance_roundtrip.c)
demonstrates the position-distance family with one-byte buffers, multiple
frames, known-size encoding, local decoder limits and failure-path cleanup.
It needs no profile helper. This example does not imply general format admission.
The installed examples project uses only `find_package(marc CONFIG REQUIRED)`
and public targets; it builds a position-distance consumer for each available
`marc::static` / `marc::shared` target and registers round trips with CTest.
The project enables C and C++; example sources remain C11, while static-library
consumers use the C++ linker for the implementation's runtime dependencies.
Do not hard-code a platform-specific C++ runtime library in C callers.
CI sets `MARC_EXPECTED_LINKAGE=shared` or `static` to reject missing or unexpected
exported targets. Normal users may omit this assertion; `both` is also supported.

For a package installed at `<prefix>` (default `share` data directory):

```sh
cmake -S <prefix>/share/marc/examples -B out/consumer -Dmarc_DIR=<prefix>/lib/cmake/marc
cmake --build out/consumer --config Release
ctest --test-dir out/consumer -C Release --output-on-failure --timeout 600
```

Use the package's actual library/data directories if customized. On Windows,
prepend `<prefix>/bin` to the process PATH for DLL discovery; on Linux use the
installed library directory in `LD_LIBRARY_PATH` if the loader needs it. Keep
these changes local to the test process/environment. CI uses separate shared-only
and static-only packages on Windows and Ubuntu and runs the installed examples.

## Contextual rANS canonical surface for 0.2.0

The final pre-1.0 Contextual rANS C surface uses only the unqualified
`marc_lzss_contextual_rans_*` family and selects entropy variant 3's canonical
variable descriptor. The fixed variant-2 implementation and every
`marc_lzss_contextual_rans_compact_*` declaration are removed together. No
compatibility typedef, wrapper, macro, or exported alias is provided. This API
rename does not renumber `MARC_ABI_VERSION`; callers must compile against the
matching 0.2.0 header and library.

The canonical Contextual rANS encoder's opaque views requirement includes its
caller-owned exact match-finder workspace after typed-token staging. Callers
must use the current
`marc_lzss_contextual_rans_workspace_requirements()` result rather than cache
an earlier extent. The finder layout does not cross the ABI or stream; the
decoder and serialized identity remain unchanged.

The Contextual tANS encoder follows the same opaque-workspace rule. Its current
views requirement contains typed-token staging, fixed encode tables, and an
aligned exact-finder workspace. Callers must obtain the extent from
`marc_lzss_contextual_tans_workspace_requirements()` and must not infer or
cache the private partition. Finder selection changes no ABI revision, stream
variant, or decoder requirement.

The Contextual Blocked Huffman encoder's opaque views requirement likewise
contains its caller-owned exact match-finder workspace after typed-token
staging. Callers must use the current
`marc_lzss_contextual_blocked_huffman_workspace_requirements()` result and must
not infer or cache the private partition. Finder selection changes no ABI
version or stream identity, and decoder table workspace remains unchanged.

The Contextual Adaptive Huffman encoder's opaque views requirement retains its
typed-token, node, and symbol regions and appends an aligned exact-finder
workspace. Callers must use the current
`marc_lzss_contextual_adaptive_huffman_workspace_requirements()` result and
must not infer or cache the private partition. Finder selection changes no ABI
revision, stream variant, or decoder requirement.

## Contextual LZSS match-finder selection

The five Contextual LZSS configurations expose an encoder-local selector in
the ABI-1 slot immediately after `direction`:

```c
config.match_finder_strategy = MARC_LZSS_MATCH_FINDER_HASH_CHAIN_EXACT;
config.match_finder_strategy = MARC_LZSS_MATCH_FINDER_BINARY_TREE_EXACT;
```

Every initializer selects HashChain Exact. `config_apply_profile()` preserves
either known selector, including repeated application, and never raises the
internal-buffer hard limit for BinaryTree. A caller selecting BinaryTree must
set a sufficient `max_internal_buffered_bytes` and then query workspace again.
Unknown selector values are invalid in both directions and never fall back.

The selector is encoder policy only: it is not serialized, and decode accepts
either known value while returning the same requirements and constructing the
same decoder. All five Contextual LZSS encoders execute both exact strategies.
No route silently replaces a requested BinaryTree strategy with HashChain.

Contextual Blocked Huffman now accepts MARC_LZSS_CONTEXTUAL_PROFILE_64M
(selector 4) through its public C lifecycle. apply_profile selects 64-MiB
frame/window, 536,870,912 decisions, 1,006,632,960 payload bytes, 17,885 table
entries, and four-GiB aggregate storage. Direction, original size, total-output
policy, and Exact finder are preserved. Callers may tighten limits and must
query workspace again after changes. The ABI-1 layout and 64K initializer
default are unchanged; the exact identity is 2/6 + 1/5 + 2/2. Tool names,
bounded fuzz profile, and schema 55 are not extended by this admission.

## 1 MiB position-distance integration

The separate `marc_lzss_position_distance_dynamic_range_1m_config` uses
`marc_lzss_position_distance_dynamic_range_1m_config_init()`,
`marc_lzss_position_distance_dynamic_range_1m_workspace_requirements()` and
`marc_lzss_position_distance_dynamic_range_1m_create()`. Its default frame and
fixed window are 1,048,576 bytes, with lengths 3..258. The format identity is
dictionary 2/9, context 1/10, entropy 3/2; contexts number 44 with 2,566 model
entries. It cannot decode the 64 KiB or contextual 1 MiB profiles.

The caller owns queried primary, secondary and aligned views buffers until
handle destruction. Only queried prefixes are retained and charged. Encode
uses raw primary and serialized secondary; decode reverses these roles.
The aggregate check includes the opaque handle, concrete transform and model
state. Decode ignores original_size/frame_size and derives local capacity
from min(max_frame_size, 1,048,576). Limit fields never enlarge wire bounds.
Query errors preserve the requirements; ordinary creation failure sets the
disjoint handle output to null. Metadata aliases are rejected before writes.
Flush preserves frames; ResetBlock is unsupported; final unconsumed input must
be resubmitted with EndInput. End/error states are sticky. No process-time
allocation occurs, and failed frames produce no output from that frame.

This adds a second position-distance C family independently of the baseline
42-profile matrix. Corpus performance and external qualification for the new
family remain pending; the preceding schema-58 evidence covers the older family.

### Five-prefix encoder workspace update

The 1 MiB position-distance encoder now uses the exact five-prefix finder
(DD-1326). For frame size F >= 3 its queried views extent grows by
4 * (65,536 + F) bytes; at the default 1 MiB frame this adds 4,456,448 bytes
(4.25 MiB). Re-query workspace requirements with the current library. Old
smaller storage is rejected and an insufficient aggregate budget returns
LIMIT_EXCEEDED before allocation. Frames shorter than three need no finder
arrays. Decoder workspace, public struct layouts and encoded bytes are unchanged.
The factory still borrows only queried prefixes, performs two allocations and
allocates nothing while processing. Earlier external evidence does not replace
verification of this encoder revision.


## Four-MiB position-distance C family

`marc_lzss_position_distance_dynamic_range_4m_config_init()`,
`marc_lzss_position_distance_dynamic_range_4m_workspace_requirements()` and
`marc_lzss_position_distance_dynamic_range_4m_create()` provide a distinct
fixed-window profile with dictionary/context variants 10/11, 46 contexts and
2588 model entries. Only this family defaults to a four-MiB frame/window,
payload ceiling 75497477 and internal ceiling 512 MiB; generic/one-MiB defaults
and ABI version remain unchanged. Decoder capacity is min(max_frame_size,4194304)
and ignores configured original_size/frame_size. Limits never enlarge wire bounds.

Encode primary is raw and secondary serialized; decode reverses roles. Aligned
views, every supplied workspace capacity and unused tails remain borrowed and
charged until destruction. Aggregate checks include actual concrete codec,
guard and opaque handle. Query errors preserve requirements. Ordinary creation
failure clears a disjoint handle output; metadata aliases are rejected before
writes. Process input/output must be disjoint from all retained capacities and
the handle. No steady-state allocation occurs. Flush preserves frames,
ResetBlock is unsupported, terminal states are sticky, and a failed frame
publishes no raw bytes from that frame. Previously validated frames may remain.

This is the third position-distance C family. CLI selection, new exchange
inventory and revision-specific hosted/external qualification remain separate
integration gates; earlier one-MiB reports do not qualify this family.


## Four-MiB command-line use

The explicit command-line codec `lzss-position-distance-dynamic-range-4m`
uses the public four-MiB initializer, workspace query and factory above.
It inherits their profile-local limits and failed-frame publication contract.
The command-line file transaction commits its destination only after the
entire stream succeeds. Existing C config layouts, exports and generic or
one-MiB defaults are unchanged by this application integration.

## Eight-MiB position-distance prepared owning encoder (encoder only)

Use the separate `marc_lzss_position_distance_dynamic_range_8m_config` with
`marc_lzss_position_distance_dynamic_range_8m_config_init()` and
`marc_lzss_position_distance_dynamic_range_8m_create_encoder()`. The factory
selects immutable encoding direction and accepts no caller workspace or
allocator. The public decoder and command-line profile are not yet provided.

Initialization is a template, not a ready-to-create default configuration.
It selects `MARC_LZSS_POSITION_DISTANCE_8M_PREPARED_OWNING` (1), frame/window/block
8388608, match limit 258 and model total 32768. Set concrete `original_size`,
all remaining limits, declared external retained bytes and full input/output
capacity bounds before querying/creating. Internal budget, total output limit,
payload limit, table limit and expansion ratio start at zero and must be set;
table limit must be at least 2599. Zero budget never means unlimited. There is
no unknown-size sentinel, automatic strategy selection or smaller-window change.

`marc_lzss_position_distance_dynamic_range_8m_resource_requirements()` fills
`marc_lzss_position_distance_dynamic_range_8m_resources`: metadata, external
charge, fixed reservation, initial raw bytes, initial index entries, initial
total and `MARC_LZSS_POSITION_DISTANCE_8M_INITIAL_ONLY` admission scope.
This admits only initial storage. Every later generation is admitted alongside
retained old blocks; an insufficient budget can fail after creation, with no
bytes of the failed frame published. Logical accounting conservatively includes
the complete handle, guard, adapter, controls, helper reserves and complete
declared call capacities, including unused tails. It is not process RSS.

Failed queries preserve the result. Overlap between config and result/handle
output is rejected without modification. Otherwise creation clears the handle
output on failure and publishes only after initial construction succeeds.
Config need not remain alive afterwards. Invalid configuration or process
capacity/overlap maps to INVALID_ARGUMENT; resource overflow/refusal maps to
LIMIT_EXCEEDED; real initial allocation failure maps to OUT_OF_MEMORY.

Use `marc_transform_process()` and `marc_transform_destroy()` as usual.
Input and output must be disjoint from each other, all owned storage and the
entire handle/guard. Only committed bytes and accepted input count. Flush is
neutral; ResetBlock and unknown flags are unsupported. Repeat EndInput on an
unconsumed final suffix. Error/end results are sticky for valid buffers; the
generic dispatcher still rejects null nonempty buffers before dispatch.
Prior valid frames and the header remain committed when a subsequent frame
fails. Exact contracts, qualified scope and pending decoder work are recorded
in docs/design/lzss-position-distance-8m-public-encoder.md.

### Explicit eight-MiB position-distance decoder

The DD-1468 boundary adds `marc_lzss_position_distance_dynamic_range_8m_decoder_config_init()`,
`marc_lzss_position_distance_dynamic_range_8m_decoder_workspace_requirements()`
and `marc_lzss_position_distance_dynamic_range_8m_create_decoder()`.
The distinct decoder config has no original size, encoder strategy or direction
field; those stream parameters are validated from the header. Initialization is
a template requiring explicit remaining limits and memory budget.

Initialize query result `struct_size` and `abi_version` and zero its reserved
field. A rejected query leaves it unchanged. `MARC_LZSS_POSITION_DISTANCE_8M_CAPACITY_ONLY`
reserves capacities; it does not validate a stream or promise payload admission.
Let F=min(max_frame_size,max_block_size,8388608), P=min(max_compressed_payload_size,18F+5).
Recommendations are 80+P serialized bytes, F opaque token elements in each of
two aligned typed workspaces, and F bytes in each of two raw workspaces. The
query reports actual token size through byte capacities and actual alignment;
there is no public token struct. The header retains bounded window 1..8388608
and maximum match 3..258 within caller limits, with 2599 model entries.

The five-buffer descriptor owns no storage. All full capacities remain borrowed
until transform destruction; tokens and token_scratch must be aligned and exact
multiples of the reported token element size. Larger capacities are accepted
only when the full tails also fit the memory budget. Configuration and descriptor
metadata are copied. The factory starts and ends private token object lifetimes;
caller storage must not contain other live objects or be accessed while active.

The full reservation includes all five actual buffer capacities, private owner,
controls/helpers, public guard/handle/controls, declared external retained bytes
and FULL declared input/output capacities. The last two are charged even for
shorter calls. Checked overflow or budget refusal is LIMIT_EXCEEDED; malformed
metadata, alignment, divisibility, capacity or overlap is INVALID_ARGUMENT.
Scalar allocation refusal is OUT_OF_MEMORY. An aliased output slot is unchanged;
after disjoint metadata/output validation, creation failure leaves it null.
Two scalar allocations precede token lifetime construction and readiness.
No workspace growth, retry, fallback or budget transfer occurs.

Serialized, tokens, token_scratch and raw_scratch are discardable working storage
on error. A failed frame leaves the entire validated raw slot unchanged and
contributes no downstream bytes. Previously validated frames may already have
drained during the failing call or before a strict trailing-data error. The
private stream candidate layout is not a public unchanged-on-error guarantee.
Flush is neutral, ResetBlock is unsupported, terminal states are sticky for
valid buffers, and the generic dispatcher's null nonempty-buffer check retains
precedence. Call-capacity and alias errors consume/produce zero bytes and report
accepted encoded position; delegated errors preserve private categories/positions.
This explicit boundary does not yet add command-line selection or a generic reader family.


### Sixteen-MiB position-distance factories

The distinct `marc_lzss_position_distance_dynamic_range_16m_*` API uses the sixteen-MiB stream identity. Encoder configuration selects `MARC_LZSS_POSITION_DISTANCE_16M_COMPACT_OWNING`. Configuration initializers set structural/ABI and profile fields; callers supply the remaining limits, known original size, external retained bytes and full input/output capacities explicitly. Existing APIs and global defaults remain unchanged.

`resource_requirements` admits initial raw/index/control ownership only, indicated by `MARC_LZSS_POSITION_DISTANCE_16M_INITIAL_ONLY`. Every later frame charges its whole candidate and retained previous publication before allocation; successful initial creation cannot guarantee a later frame fits. Oversized calls, aliases, unsupported flags, allocation failures and hard-limit refusals report stable public categories. An error call may contain earlier complete frame bytes but contains no bytes of the failed frame. Configuration is copied; the caller need not retain it. EndInput accompanies each unconsumed final suffix; Flush is neutral, ResetBlock unsupported, and terminal status is sticky.

The separate decoder `decoder_workspace_requirements` returns `MARC_LZSS_POSITION_DISTANCE_16M_CAPACITY_ONLY` capacities for serialized bytes, two opaque aligned token stores and two raw stores. All full capacities remain borrowed until destruction and must be disjoint from each other, controls and calls. Token storage is workspace, never a native wire representation. The serialized request is limited by the smaller of the wire maximum and configured compressed-payload limit, plus the frame prefix. Queries do not validate payloads. Preserve actual token alignment, element multiples and all backing tails when supplying buffers.

Maximum native public experiments with 65,536-byte input/output capacities, a 67,108,864-byte payload limit, explicitly retained controls and an explicit 536,870,912-byte internal policy reconstruct two full pseudo-random frames. For the measured controls, the decoder query reports 503,653,744 bytes, with exact/one-below admission proven. Different caller capacities or external owners change the requirement and may refuse. This measured policy does not replace explicit capacity queries or change global defaults. Late payload corruption publishes no part of that frame; earlier validated output stays committed.


The public entry points are `marc_lzss_position_distance_dynamic_range_16m_config_init()`, `marc_lzss_position_distance_dynamic_range_16m_resource_requirements()` and `marc_lzss_position_distance_dynamic_range_16m_create_encoder()` for encoding; `marc_lzss_position_distance_dynamic_range_16m_decoder_config_init()`, `marc_lzss_position_distance_dynamic_range_16m_decoder_workspace_requirements()` and `marc_lzss_position_distance_dynamic_range_16m_create_decoder()` for decoding. Configured entropy capacity must admit the 2,610 model entries.


### Thirty-two-MiB position-distance factories

The separate `marc_lzss_position_distance_dynamic_range_32m_*` encoder and
decoder configurations select dictionary variant 13 and context variant 14.
The owning encoder's resource query admits initial raw/index storage and all
wrapper, allocator, call-capacity and external-retained charges; every frame
candidate is admitted again before allocation. `INITIAL_ONLY` is not a promise
that later frames will fit. Initializers set profile fields; applications supply
other limits and full retained/input/output capacities explicitly.

The decoder query is `CAPACITY_ONLY`, not payload validation. Its five borrowed
buffers are byte workspaces: serialized frame, compact records, compact record
scratch, raw bytes and raw scratch. For admitted frame capacity R, each record
buffer has at least 3R bytes, `token_alignment` is one and
`record_capacity_bytes` counts bytes. No typed-token construction or element
count is imposed. All full actual buffer capacities are charged at creation;
excess tails and other live owners outside the call views belong in the
external-retained charge. All five buffers must remain disjoint and alive until
transform destruction. Configuration and buffer metadata are copied.

Direction is immutable, Flush is neutral, ResetBlock is rejected, and terminal
results are sticky. A failed frame contributes no output; earlier completed
frames may be returned by the same Error call. Metadata-query failures preserve
the complete output object. Existing sixteen-MiB typed buffer declarations and
factories retain their ABI and behavior. CLI limits and exchange selection are
separate application integration work.


The explicit thirty-two-MiB CLI application sets a 32 MiB frame/window,
64 MiB payload cap, 65,536-byte input/output calls and 512 MiB internal codec
policy. This does not change the public initializer defaults. Capacity queries
include complete wrapper and caller grants; every subsequent encoder candidate
is admitted again while retaining the previous publication. These limits are
codec accounting, not a promise about OS resident memory or all valid wire
streams. The CLI preserves existing targets and commits only successful
whole-file output transactions.

The distinct public entry points are
`marc_lzss_position_distance_dynamic_range_32m_config_init()`,
`marc_lzss_position_distance_dynamic_range_32m_resource_requirements()` and
`marc_lzss_position_distance_dynamic_range_32m_create_encoder()`;
`marc_lzss_position_distance_dynamic_range_32m_decoder_config_init()`,
`marc_lzss_position_distance_dynamic_range_32m_decoder_workspace_requirements()`
and `marc_lzss_position_distance_dynamic_range_32m_create_decoder()`.


## Sixty-four-MiB position-distance public extension

The distinct `marc_lzss_position_distance_dynamic_range_64m_*` lifecycle uses
dictionary variant 14 and context variant 15 with fifty contexts. Encoder and
decoder initializers set profile fields only; callers supply every remaining
limit, external retained byte count and complete input/output call capacity.
The default profile frame/window is 67,108,864 bytes, match lengths 3..258,
and maximum range-model total 32,768. The frequency-bank limit must admit 2,632
entries. Generic initializers and every previous public declaration are unchanged.

`config_init`, `resource_requirements` and `create_encoder` select the compact
owning encoder. Resource admission is INITIAL_ONLY; every candidate frame and
all old/new generations are admitted again before allocation. A successful
query does not guarantee later frames fit. `decoder_config_init`,
`decoder_workspace_requirements` and `create_decoder` select the compact byte
decoder with CAPACITY_ONLY queries. Borrow all five full byte capacities until
transform destruction, even after draining; configuration and buffer metadata
are copied. Token alignment is one and record_capacity_bytes is a byte count,
not a typed element count. Both token buffers require at least three times the
configured maximum frame bytes to admit generic wire lengths three/four.

Failed queries preserve results. Invalid aliased output arguments are refused
without unsafe writes; safe factory failures publish no handle. Encode/decode
direction is immutable. Repeated terminal calls return the same terminal state
with zero counts. Flush is neutral, ResetBlock unsupported, and EndInput is
repeated on unconsumed final suffixes. No failed frame is released; an error call
may include output from an earlier fully validated frame. Actual complete
capacities, aliases, retained owners and controls are checked before publication.
No generic memory default or CLI policy is changed by this API addition.


For the sixty-four-MiB public owning encoder, callers may conservatively admit complete candidates using initial_bytes + 2F + 4Pcap + 240, including the retained old publication and all new generations. This never discounts complete initial controls and does not change INITIAL_ONLY resource query semantics or public initializer defaults. The payload cap remains an explicit caller choice.
## Position-distance rANS one-MiB owning profile

`marc_lzss_position_rans_1m_config_init()`,
`marc_lzss_position_rans_1m_resource_requirements()` and
`marc_lzss_position_rans_1m_create()` select the additive `2/9 + 1/10 + 4/4`
profile. Direction is selected once at initialization. Encoding requires the
known original byte length and uses fixed-five match eligibility; decoding
accepts lengths three through 258 and gets original size from the header.
No caller-supplied workspace is borrowed. Resource queries cover all owned
buffers, controls, the fixed working allowance, declared external retained
bytes and input/output call capacities before allocation. Queried byte budgets
are distinct from process RSS. Query refusal preserves results; safe create
failure sets the handle to null. Config/output aliases are rejected unchanged.

Default frame/window is one MiB, decision ceiling nine Mi symbols, payload
ceiling 18MiB+8, frequency entries 2566 and aggregate capacity budget 64MiB.
Default input/output call capacities are 65,536 bytes. Calls exceeding them
fail without consuming or producing bytes. Callers can lower limits and add
their own retained-byte charge. Transform process/destroy, split-buffer,
unchanged Flush, unsupported ResetBlock, sticky error/completion and failed
frame withholding contracts apply. No failed frame is published, while an
earlier fully validated frame may appear on an error call. Existing profiles
and ABI-1 structure definitions are unchanged. This adds one initializer to
the public inventory without changing the forty-two-profile baseline table.
For this profile `max_block_size` counts entropy decisions, while frame, payload,
aggregate and call capacities count bytes; entropy entries count frequency
entries. Queries include owner controls in the aggregate; the external charge
also includes the C handle, boundary controls and bounded helper allowance.
BM-0216 separately qualifies directional speed, measured process peaks and
the full public/CLI capacity queries. The capacity policy is unchanged by OS
peak observations. The profile is registered as schema-65 archive 75.
The reported minimum admitted aggregate also covers the configured decision
ceiling after external charges are subtracted. It may exceed the retained
layout sum when frame capacity is lowered but the decision ceiling is kept.
Creating with that reported minimum remains valid; lowering the ceiling can
reduce the admitted minimum without changing the frame's wire representation.

## 64KiB position-distance native rANS

`marc_lzss_position_distance_rans_config_init()`,
`marc_lzss_position_distance_rans_resource_requirements()` and
`marc_lzss_position_distance_rans_create()` select `2/8 + 1/9 + 4/5`.
The direction is immutable; encode requires known original size. Default
window/frame is 65,536 bytes, match eligibility five, accepted match lengths
three through 258, model entries 2522, decision limit 589,824 and payload
limit 1,179,656 bytes. The aggregate capacity budget is 4MiB, with 65,536-byte
input/output call capacities. Resource queries include owned capacities and
declared caller buffers; they do not limit OS RSS. With an additional
65,536-byte CLI control allowance the x64 query is 2,827,794 bytes for encode
and 2,303,642 bytes for decode. ABI config/resources sizes are 136/64 bytes.

Insufficient budgets reject before allocation and preserve query destinations.
Safe factory refusal returns a null handle; aliased outputs remain unchanged.
Process does not allocate, accepts partial buffers and retains sticky terminal
errors. Failed frames remain private; earlier validated frames may be returned.
The separate one-MiB public API and every existing configuration remain intact.

For this fixed-frame profile, `Flush` drains representable bytes without
closing a partial frame. `ResetBlock` and unknown process flags return
`MARC_STATUS_UNSUPPORTED`, consuming and producing nothing; the error is
sticky. Explicit arbitrary reset boundaries are not part of this variant.

In this configuration, `max_block_size` counts entropy decisions; frame,
payload, retained-buffer and call-buffer capacities are measured in bytes.
`max_entropy_table_entries` counts normalized-frequency entries.


## Owning four-MiB position-distance rANS

marc_lzss_position_distance_rans_4m_config_init(),
marc_lzss_position_distance_rans_4m_resource_requirements() and
marc_lzss_position_distance_rans_4m_create() expose DD-1532's separate
2/10 + 1/16 + 4/7 tuple. Maximum/default frame and window are4194304 bytes,
fixed-five encoder and decoder grammar3..258. The default capacity budget
is192MiB. Queries include owner, retained capacities,128KiB fixed working,
caller input/output capacities and external controls. The budget floor is
max(owned aggregate,declared maximum block)+external charge, not OS RSS.
No allocation occurs during process; incomplete or failed frames remain
private. Config/resources ABI sizes are136/64 on the supported64-bit ABI.
Factory/config/stream direction and sticky terminal policies match the
existing native rANS profiles. TVG-1397 records public/ABI/CLI qualification;
BM-0220 records directional samples and peaks, and IX-0070 local exchanges.

## Owning eight-MiB position-distance rANS

DD-1538 connects marc_lzss_position_distance_rans_8m_config_init(),
marc_lzss_position_distance_rans_8m_resource_requirements() and
marc_lzss_position_distance_rans_8m_create() to the separate2/11+1/17+4/8
tuple. Default/maximum frame and window are8388608 bytes; fixed-five
encoding and decoder grammar3..258 remain distinct. ABI config/resources
sizes are136/64 on the supported64-bit ABI. Queries charge owner, retained
capacities,128KiB fixed working, declared call buffers and external controls.
DD-1541 retains384MiB checked capacity after BM-0222 public measurements;
it is not OS RSS. TVG-1401 qualifies lifecycle/CLI and FZ-0083 the public
sanitizer campaign. IX-0073 qualifies exchange and fixed-runtime checks.
Existing families and their serialized bytes are unchanged.

## Owning sixteen-MiB position-distance rANS

DD-1544 adds marc_lzss_position_distance_rans_16m_config_init(),
marc_lzss_position_distance_rans_16m_resource_requirements() and
marc_lzss_position_distance_rans_16m_create() for tuple2/12+1/18+4/9.
Maximum frame/window is16777216 bytes; config/resources retain136/64-byte
layouts on the supported64-bit ABI. Direction is immutable and queries
charge the owning buffers, fixed working, declared calls and external
controls before allocation. DD-1546 admits the768MiB capacity grant after BM-0224
complete public directional and resource measurements.
It is not an RSS ceiling. Preserve success-only destination commits,
discard-on-failure private token scratch and failed-frame quarantine.
TVG-1406 qualifies lifecycle/ABI/CLI, FZ-0084 the five sanitizer campaigns
and IX-0074 the schema69 milestone. IX-0075 qualifies committed-source
local exchange and current runtimes; no new hosted CI or external result
is claimed.

## Owning thirty-two-MiB position-distance rANS

DD-1550 adds marc_lzss_position_distance_rans_32m_config_init(),
marc_lzss_position_distance_rans_32m_resource_requirements() and
marc_lzss_position_distance_rans_32m_create() for2/13+1/19+4/10.
Maximum frame/window is33554432 bytes; config/resources retain136/64-byte
layouts on the supported64-bit ABI. Direction remains immutable. Checked
queries include retained owning buffers,128KiB fixed working, declared calls
and external controls before allocation. DD-1551 admits the1536MiB capacity
grant after BM-0227 actual public resource and directional measurements.
It is not an RSS limit. Public
encoding uses fixed-five parsing, while decoding accepts grammar3..258.
Ordinary destinations commit only on success; failed private token scratch
is discarded and failed frames never drain. IX-0077 closes local schema70/80
qualification; hosted CI and maintainer external routes remain separate.

## Sixty-four-MiB native position-distance rANS

The additive marc_lzss_position_distance_rans_64m_config/resources types
are136/64 bytes. marc_lzss_position_distance_rans_64m_config_init(),
marc_lzss_position_distance_rans_64m_resource_requirements() and
marc_lzss_position_distance_rans_64m_create() retain the
owning transform lifecycle and immutable direction. DD-1555 retains3072MiB
after BM-0231's complete public directional and resource qualification.
The checked capacity budget accounts for private owner capacities, public working
allowance, external retained controls and declared input/output capacities.
This is not a process RSS ceiling. Encoding
uses fixed-five matching, while the format accepts lengths3..258. Ordinary
destinations commit on success; failed private token scratch is discarded
and failed frames are never drained. Earlier APIs remain unchanged.

## Sixty-four-KiB owning position-distance Blocked Huffman

DD-1611 adds `marc_lzss_position_distance_blocked_huffman_config_init()`,
`marc_lzss_position_distance_blocked_huffman_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_create()` for dictionary2/8,
context1/28 and entropy2/3 (2/8+1/28+2/3). Use the declared config/resources struct sizes
and ABI version. Window is65536 bytes, frame capacity1..65536, direction
immutable and encode original size known. The encoder uses fixed-five
longest/nearest matching; decoder wire lengths are3..258 with frame resets.

Default8MiB is logical admission, not RSS or physical peak. Default limits
are65536 raw frame bytes,458752 decisions per block,860160 payload bytes and
27083 entropy entries. Code length is fixed at most15. Only max_block_size
counts decisions; other sizes are bytes. Prefix validation charges511 entries
per active codebook before payload decoding. Reduced entry caps may admit
creation and then refuse a frame prefix without publishing that frame.

Resources separate owned buffers, private payload/serialization, tables,
owner, bounded nested work, nonallocated decoder metadata floor and public/
external charges. Minimum aggregate is max(private admission, configured
block ceiling) plus public/external charge. Call capacities default65536 each;
caller-retained storage is additional. Exact query budgets admit creation;
one byte less refuses before allocation. A failed query leaves resources
unchanged. Refused creation nulls the handle except config/handle overlap,
which leaves overlapping storage unchanged. Partial allocations roll back.

Process calls allocate nothing. Capacity violations fail before consumption/
output; Flush is neutral and ResetBlock/unknown flags are unsupported. Errors
and EOS are sticky. Only fully validated frames drain, with prior valid
frames preserved on a later error. These APIs do not imply CLI, fuzz, corpus,
physical peak or exchange qualification; later windows remain separate gates.

## Sixty-four-KiB owning position-distance tANS

DD-1566 adds marc_lzss_position_distance_tans_config_init(),
marc_lzss_position_distance_tans_resource_requirements() and
marc_lzss_position_distance_tans_create() for dictionary2/8, context1/21
and entropy5/3. Config/resources are136/120 bytes on the qualified64-bit
ABI. The physical window is65536 bytes; frame capacity is1..65536 bytes.
Direction is immutable and encoding declares the exact original byte size.
The fixed-five encoder retains longest/nearest parsing; decoding accepts
match lengths3..258 in independently reset frames.

The default8MiB admission includes declared input/output capacities, external
retained bytes and public control/working charges. Limits default to65536
frame bytes,458752 binary decisions per block,688130 payload bytes and200704
table entries. Other size fields are bytes. A resource query separates
private payload, serialized staging, table bank, actual owned buffers,
owner bytes, fixed working allowance and conservative metadata floor.
Minimum aggregate admission is the larger of private admission and the
configured decision-block ceiling, plus external/public charges. The block
ceiling is not an additional allocation. Neither query nor grant is RSS.

With full65536-byte calls and65536 declared external retained bytes, queries
are4202026 bytes encode and2693796 decode. With external retained bytes zero,
they are4136490 and2628260, so4MiB fits; adding the65536-byte retained charge
exceeds4MiB for encode. The8MiB default covers the qualified declared workload
and leaves room for caller controls, but additional storage must still be
declared and admitted. Exact queries admit construction and one-byte-below
queries/create refuse before allocation. Failed resource queries retain their
result. Factory refusals set the handle to null; config/handle overlap refuses
without altering the overlapping storage. All construction failures release
previous allocations. Process calls reuse storage and do not allocate.

Calls exceeding declared capacities fail before consumption/output. Flush is
a no-op; ResetBlock and unknown flags are unsupported. Error/end states are
sticky. Ordinary output commits only from validated frames; a failed private
token/raw prefix is discarded, and a failed frame never drains. Earlier
validated frames may already have published. This C API/static/shared
checkpoint does not qualify CLI, isolated contextual measurements, fuzz,
exchange or the later window profiles; those gates remain required.

### One-MiB position-distance tANS factory

The `marc_lzss_position_distance_tans_1m_config` and `_resources` types have
separate `_config_init`, `_resource_requirements` and `_create` entry points.
The format identity is2/9+1/22+5/4; existing64KiB entry points retain their
behavior. Fixed-five encoding accepts known original size; decoding accepts
the independently specified3..258 match grammar with one-MiB frame resets.

Default frame/window1048576, decisions8388608, payload12582914, table entries
217088 and64MiB aggregate admission apply. Decisions are symbols; other
capacities are bytes. Queries separately report owned capacities, controls,
metadata admission floor and caller/call charges; none is resident memory.
Declare external retained storage and maximum call capacities before creation.
A CLI-shaped query with65536 external bytes and two65536-byte calls reports
65543980 encode and27433809 decode admission bytes. Full encode needs more
than32MiB; the64MiB default passes actual query/create qualification with the declared
CLI-shaped charges. Extra retained storage still requires a successful query.

Failed queries leave output unchanged. Creation sets the handle to NULL on
ordinary refusal; overlapping config/output destinations refuse unchanged.
EndInput is explicit, EOS/error remain sticky and failed raw frames never
publish. Prior successful frames may already be returned before a later
error. Whole-file CLI publication is separately qualified by profile-specific
CLI tests; paired measurements, fuzz and exchange remain required.

### Four-MiB position-distance tANS factory

The `marc_lzss_position_distance_tans_4m_config` and `_resources` types have
separate `_config_init`, `_resource_requirements` and `_create` entry points.
The format identity is `2/10+1/23+5/5`, with a four-MiB dictionary and frame
window. The fixed-five token grammar accepts distances through 4,194,304 and
match lengths 3..258; entropy tables contain 225280 entries in the admitted
configuration.

The default admission uses a 4,194,304-byte frame, a 37,748,736-decision
block ceiling, and a 17,301,506-byte compressed-payload ceiling. The default
aggregate internal limit is 384MiB. These are bounded admission parameters;
the resource query still accounts for caller-declared capacities and external
retained storage, and the measured CLI-shaped query reported 257449900 bytes
for encode and 106109863 bytes for decode. Query results are requirements,
not resident-memory or RSS claims.

Invalid capacities, table counts, frame limits, and overlapping configuration
or output storage fail before consumption and preserve the caller's existing
buffers. Allocation-failure cleanup releases private scratch and leaves the
handle null. EndInput is explicit, EOS and error are sticky, and failed raw
frames never publish or drain; previously validated frames may already have
been returned. The four-MiB internal and public API tests cover these rules,
including one-byte chunking and static/shared C smoke paths. CLI connection,
contextual comparison, benchmark, fuzz, and exchange-bundle qualification
remain separate gates for this profile.

### Eight-MiB position-distance tANS factory

The `marc_lzss_position_distance_tans_8m_config` and `_resources` types have
separate `_config_init`, `_resource_requirements`, and `_create` entry points.
The format identity is `2/11+1/24+5/6`, with an 8,388,608-byte dictionary and
frame window. Fixed-five encoding and decoding use the independently specified
3..258 match grammar; entropy tables contain 229376 entries.

The default admission specifies an 8,388,608-byte frame, a 75,497,472-decision
block ceiling, a 113,246,210-byte compressed-payload ceiling, and a one-GiB
aggregate internal limit. A resource query reports the full-window owned
components and caller-declared capacities before allocation. These are
admission limits, not resident-memory or RSS claims.

Invalid metadata, capacities, table counts, and overlapping configuration or
output storage fail without consumption. EndInput is explicit, EOS and error
remain sticky, and raw bytes drain only after the complete private frame has
validated; a failed frame never drains. The local API tests cover small-frame
round trips and withholding a failed second frame. CLI, contextual comparison,
benchmark, fuzzing, and exchange-bundle qualification remain separate gates.

## Native one-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_1m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_1m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_1m_create()` select the additive
tuple `2/9+1/29+2/4`. The wire grammar permits lengths3..258, and the encoder
uses fixed-five longest/nearest selection. Existing types and variants retain
their layouts and meanings.

Defaults are frame/window1048576, decision ceiling8388608, payload15728640,
codebook entries29127, maximum distance1048576 and match length258. Expansion
ratio is1024 with1048576 slack bytes. Input/output capacities default65536
each and external retention0. The96MiB logical default follows DD-1613's
full-frame query and normal resource precursor. It includes declared calls,
retained caller storage and bounded public controls; it is not a process RSS
limit. Use the resource query for the actual ABI/layout and configured scope.

The query separates owned storage, nested working allowance, metadata floor,
external charges and minimum aggregate admission. Ordinary query output is
unchanged on failure. Create refuses before known-invalid allocation and sets
the destination handle null on refusal, except overlapping config/handle
storage remains untouched. Partial construction releases every allocation.
Reduced active-codebook limits are checked at descriptor admission; constructor
success does not guarantee admission of all possible future descriptors.

Direction is immutable. Process allocates nothing; spans beyond declared
capacities refuse before consumption/publication. Flush is neutral, ResetBlock
is unsupported, errors/EOS remain sticky and failed frames never drain. API
qualification, CLI connection, all-member contextual comparisons/physical
peaks, coverage-guided fuzz and exchange have separate completion records.
The complete one-MiB local qualification is recorded at
[IX-0098](interoperability.md); final family and hosted/external gates remain
separate from that local result.

## Native four-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_4m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_4m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_4m_create()` select the additive
tuple `2/10+1/30+2/5`. Corresponding `_4m_config` and `_4m_resources` types
retain the established native field layouts; prior ABI definitions are unchanged.
Decoder grammar is3..258 and encoding uses fixed-five longest/nearest matches.

Defaults are4194304-byte frame/window,37748736 decisions,70778880 payload
bytes,30149 codebook entries, maximum distance4194304 and match length258.
Expansion ratio is1024 with4194304 slack bytes. Declared input/output capacities
are65536 each and external retention defaults0. DD-1615's320MiB logical grant
follows owner queries and normal resource precursors. It charges actual public
controls, declared I/O and retained caller storage; it is not a process RSS cap.
Use the query for the configured scope and actual ABI/layout.

Queries distinguish owned buffers, owner controls, fixed working allowance,
metadata floor and external charges. `max_block_size` is a decision admission
ceiling and budget floor rather than another allocated buffer. Query refusal
preserves its destination. Create refuses known-invalid configurations before
allocation, clears the handle on failure except config/handle overlap, and
releases all partial allocations. Smaller active-codebook limits can admit
creation and reject a later descriptor according to actual use.

Direction is immutable. Process allocates nothing, enforces declared call
capacities and retains sticky error/EOS. Flush is neutral; ResetBlock is
unsupported. Only completely validated raw frames enter the public draining
state. CLI, complete contextual comparisons/physical peaks, coverage fuzz and
exchange remain separately qualified gates.

## Native eight-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_8m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_8m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_8m_create()` select the additive
tuple `2/11+1/31+2/6`. `_8m_config` and `_8m_resources` retain the established
native field layouts. Decoder grammar is3..258; encoding uses fixed-five
longest/nearest matches. All unspecified or foreign identities are refused.

Defaults are8388608-byte frame/window,75497472 decisions,141557760 payload
bytes,30660 codebook entries, distance8388608 and match length258. Expansion
ratio1024 has8388608 slack bytes. Declared I/O capacities are65536 each;
retained caller storage defaults0. DD-1617's640MiB logical default derives
from owner queries and normal resource precursors. It includes actual public
controls and declared I/O/retention charges; it is not a physical RSS cap.
Use the resource query for the actual ABI/layout and configured caller scope.

Queries expose owned buffers, owner controls, nested working storage, metadata
floor and external charges separately. `max_block_size` is a decision ceiling
and grant floor, not another buffer. Refused queries preserve their destination.
Create checks invalid configurations before allocation, clears failed handles
except config/handle overlap, and rolls back partial allocations. A reduced
model quota may admit construction and refuse a later descriptor by actual use.

Direction is immutable. Process allocates nothing, enforces declared capacity,
keeps sticky error/EOS, accepts neutral Flush and refuses ResetBlock. Only fully
validated frames drain; failed frames never publish a raw prefix. Public C
qualification, CLI, all-member comparisons/ordinary peaks and exchange are
tracked independently. Fuzz is omitted during this goal by user request.

TVG-1519 qualifies the public boundary, including both complete default
constructions. Actual resource queries with65536 caller-retention bytes and
two65536 call buffers report635845671 encode and251157534 decode bytes;
external charges are200816 bytes on the qualified layouts. The unmodified
default retention0 queries are635780135/251091998 bytes. Query each configured
scope rather than treating these values as ABI-independent constants or RSS.
Normal/static/shared tests,499 independent raw fixtures and direct ASan/UBSan
checks retain failure/publication guarantees. Prior declarations and88/91
archive bodies remain unchanged; CLI and later gates remain independently
qualified.

## Native sixteen-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_16m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_16m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_16m_create()` select tuple
`2/12+1/32+2/7`. The additive config/resources retain the established native
field layouts. Wire matches are3..258; the encoder selects fixed-five
longest/nearest matches. Unsupported identities are refused.

Defaults are16777216-byte frame/window,150994944 decisions,283115520 payload
bytes,31171 codebook entries, distance16777216 and match length258. Expansion
ratio1024 has16777216 slack bytes; declared I/O capacities are65536 each and
retained caller storage defaults0. The1280MiB logical grant derives from
checked owning requirements and actual bounded construction, without a new
physical-peak benchmark. It includes public controls and declared I/O and
retention charges. Query the actual ABI/layout and configured caller scope;
this grant is not process RSS.

Queries distinguish owned buffers, controls, working storage, metadata floor
and external charges. The decision ceiling is also a grant floor, not another
buffer. Failed queries preserve output; failed create clears the handle,
except config/handle overlap leaves storage unchanged. Validation precedes
allocation, partial allocations roll back, and process allocates nothing.
Direction is immutable; errors/EOS are sticky, Flush is neutral and ResetBlock
is unsupported. Capacity checks and active model quotas precede unsafe intake.
Only fully validated frames drain; failed frames publish no raw prefix.

Public boundary qualification, CLI and schema83/93 exchange are separate gates.
Further ratio/speed/physical-peak benchmarks and fuzz are omitted by user
request through this goal. Same-window contextual size and performance goals
remain unmeasured and are not reported as met.

TVG-1529 qualifies the public boundary, including both full-default
constructions in static/shared C consumers. Actual queries with65536 retained
bytes and two65536 call buffers return1270239115 encode and501772106 decode
bytes, including200816 external-charge bytes on the qualified layouts.
Query each actual scope and ABI; these are not physical peaks. Nine normal
checks,499 independent raw fixtures and direct ASan/UBSan preserve failure
and publication contracts. Earlier declarations and all starting88/preceding92
archive bodies remain unchanged. CLI and later gates remain separate.
The sixteen-MiB local CLI/exchange gates are closed at [IX-0101](interoperability.md).
Final95-family hosted/external qualification remains separate.

## Native thirty-two-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_32m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_32m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_32m_create()` select tuple
`2/13+1/33+2/8`. The additive config/resources retain the existing native field
layouts. Wire matches are3..258; the encoder uses fixed-five longest/nearest
matches. Unsupported identities are refused.

Defaults are33554432-byte frame/window,335544320 decisions,629145600 payload
bytes,31682 codebook entries, distance33554432 and match length258. Expansion
ratio1024 has33554432 slack bytes. Declared I/O capacities are65536 each and
retained caller storage defaults0. The3072MiB logical grant derives from checked
owning requirements; it includes public controls and declared I/O/retention.
Query the actual ABI/layout and caller scope; these are not process peaks.

Queries distinguish owned buffers, controls, working storage, metadata floor
and external charges. The decision ceiling is a grant floor, not a second
buffer. Query failures preserve output; failed creation clears the handle,
except config/handle overlap preserves storage. Admission precedes allocation;
partial allocations roll back and process allocates nothing. Direction is
immutable, errors/EOS are sticky, Flush is neutral and ResetBlock unsupported.
Only fully validated frames drain; failed frames publish no raw prefix.

Actual default construction and boundary qualification have their own test
checkpoint. CLI and schema84/94 exchange remain separate gates. Fuzz and
further ratio/speed/physical-peak benchmarks are omitted by user request through
this goal; same-window contextual size and performance goals remain unmeasured.

TVG-1537 qualifies the public boundary, including both complete default
constructions in static/shared C consumers. With65536 retained caller bytes
and two65536-byte call buffers, actual queries return2727764634 encoder and
1065910860 decoder admission bytes, including200816 external-charge bytes on
the qualified layouts. Query each actual ABI and scope; these are not physical
peaks. Nine normal checks,499 independent raw fixtures and direct ASan/UBSan
preserve the failure/publication contracts. All earlier declarations and
starting88/preceding93 archive bodies remain unchanged. The thirty-two-MiB
local CLI/exchange gates are closed at [IX-0102](interoperability.md).
Final95-family hosted/external qualification remains separate.

## Native sixty-four-MiB position-distance Blocked Huffman

`marc_lzss_position_distance_blocked_huffman_64m_config_init()`,
`marc_lzss_position_distance_blocked_huffman_64m_resource_requirements()` and
`marc_lzss_position_distance_blocked_huffman_64m_create()` select tuple
`2/14+1/34+2/9`. The additive config/resources retain the existing native field
layouts. Wire matches are3..258; the encoder uses fixed-five longest/nearest
matches. Unsupported identities are refused.

Defaults are67108864-byte frame/window,671088640 decisions,1258291200 payload
bytes,32193 codebook entries, distance67108864 and match length258. Expansion
ratio1024 has67108864 slack bytes. Encoder original_size defaults0; decoding
uses the stored size. Maximum total output defaults to one TiB.
Declared I/O capacities are65536 each and
retained caller storage defaults0. The6144MiB logical grant derives from
checked owning requirements and actual default construction in static/shared
C consumers. It includes public controls and declared I/O/retention. Query the actual ABI,
layout and caller scope; these are not process peaks.

Queries distinguish owned buffers, controls, working storage, metadata floor
and external charges. The decision ceiling is a grant floor, not a second
buffer. Query failures preserve output; failed creation clears the handle,
except config/handle overlap preserves storage. Admission precedes allocation;
partial allocations roll back and process allocates nothing. Direction is
immutable, errors/EOS are sticky, Flush is neutral and ResetBlock unsupported.
Only fully validated frames drain; failed frames publish no raw prefix.

Public boundary qualification, CLI and schema85/95 exchange are separate gates.
Fuzz and further ratio/speed/physical-peak benchmarks are omitted by user request
through this goal; same-window contextual size and performance goals remain
unmeasured and are not reported as met.

TVG-1545 qualifies the public boundary with nine normal checks and499
independent raw fixtures in normal and direct ASan/UBSan builds. With65536
retained bytes and two65536-byte call buffers, actual queries return5454067158
encoder and2131268960 decoder admission bytes, including200816 external-charge
bytes on the qualified layouts. Query each actual ABI and caller scope; these
are not physical peaks. Earlier declarations and all starting88/preceding94
archive bodies remain unchanged. CLI and schema85/95 exchange and the final
family local audit are qualified at [IX-0103](interoperability.md). Hosted CI
and external four-route95 confirmation remain separate.

### Revised64KiB native Huffman output policy

[DD-1624](design/lzss-position-distance-blocked-huffman-64k-pool-refit.md)
explicitly revises the default64KiB encoder reference to refit residual pools.
Public structures, ABI, selector and decoder grammar are unchanged. Existing
streams remain valid; re-encoding under the new policy can change bytes. Query
the current encoder's resource requirements: its additional conservative
private charge is one descriptor plus4096 bytes, propagated through encoder
workspace accounting. Decoder requirements and default grants remain unchanged.
Exact/one-short admission and allocation-failure rollback remain mandatory.
TVG-1553 records static/shared public qualification separately from measurements.

### Larger-window residual-pool investigation

DD-1625 extends qualification to the larger native Huffman encoders. The1MiB
candidate revises encoder bytes under the existing API and selector, with an
extra private descriptor-plus4096 encoder charge propagated by resource queries.
Query the current implementation; do not reuse an older minimum as a fixed
constant. Decoder charges, public structures and default grants are unchanged.
TVG-1558 qualifies static/shared and public failure paths; corpus performance
and old/new corpus compatibility are separate pending gates at that milestone.
