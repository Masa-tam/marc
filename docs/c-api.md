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
before allocation. Contextual rANS also admits the 64-MiB selector through its
own completed lifecycle. Contextual tANS, Contextual Blocked Huffman, and
Contextual Adaptive Huffman admit common selectors only through 16 MiB; their
helpers reject the 64-MiB selector. The field and its trailing
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
64-MiB interoperability remains closed.

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
