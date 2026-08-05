# C API

The public C ABI is declared by `<marc/marc.h>`. It exposes Blocked Huffman,
Adaptive Huffman, Dynamic Range, rANS, tANS, LZ77 variant 1, the LZ77 plus
Blocked Huffman, LZ77 plus Adaptive Huffman, and LZ77 plus Dynamic Range
profiles, the LZ77 plus rANS and LZ77 plus tANS profiles, LZSS variant 1, the LZSS plus Blocked
Huffman and LZSS plus Adaptive Huffman profiles, and the LZSS plus Dynamic
Range, LZSS plus rANS, and LZSS plus tANS profiles,
LZ78 variant 1, the LZ78 plus Blocked Huffman, LZ78 plus Adaptive Huffman, and
LZ78 plus Dynamic Range, LZ78 plus rANS, and LZ78 plus tANS profiles, LZW
variant 1, the LZW
plus Blocked Huffman,
LZW plus Adaptive Huffman, and LZW plus Dynamic Range profiles, LZD variant 1,
the LZD plus Blocked Huffman, LZD plus Adaptive Huffman, LZD plus Dynamic
Range, LZD plus rANS, and LZD plus tANS profiles, and LZMW variant 1 and the
LZMW plus Blocked Huffman, LZMW plus Adaptive Huffman, and LZMW plus Dynamic
Range profiles with known-size encoding and bounded caller-owned workspace.
All functions are `noexcept` in C++ translation units, and no C++ type appears
in the ABI.

## Profiles and composition

The C ABI exposes complete, validated stream profiles rather than separate
dictionary and entropy objects that callers combine at runtime. Each standalone
dictionary factory binds entropy `None`, and each standalone entropy factory
binds dictionary `None`. `marc_lz77_blocked_huffman_*`,
`marc_lz77_adaptive_huffman_*`,
`marc_lz77_dynamic_range_*`,
`marc_lz77_rans_*`,
`marc_lz77_tans_*`,
`marc_lzss_blocked_huffman_*`, `marc_lzss_adaptive_huffman_*`,
`marc_lzss_dynamic_range_*`, `marc_lzss_rans_*`,
`marc_lzss_tans_*`,
`marc_lz78_blocked_huffman_*`, `marc_lz78_adaptive_huffman_*`,
`marc_lz78_dynamic_range_*`,
`marc_lz78_rans_*`,
`marc_lz78_tans_*`,
`marc_lzw_blocked_huffman_*`, `marc_lzw_adaptive_huffman_*`,
`marc_lzw_dynamic_range_*`, `marc_lzw_rans_*`,
`marc_lzd_blocked_huffman_*`, `marc_lzd_adaptive_huffman_*`,
`marc_lzd_dynamic_range_*`, `marc_lzd_rans_*`, `marc_lzd_tans_*`, and
`marc_lzmw_blocked_huffman_*`, `marc_lzmw_adaptive_huffman_*`, and
`marc_lzmw_dynamic_range_*` are the currently public
dictionary-plus-entropy factories.

This is a scope and validation decision, not an incompatibility unique to the
other algorithms. The byte-stream architecture can feed any canonical
dictionary serialization into a byte-oriented entropy layer. Publishing an
additional pairing still requires its exact format parameters, worst-case
workspace calculation, transactional decoder validation, streaming behavior,
C ABI configuration, and complete test surface to be fixed together. A
standalone factory therefore does not imply that every cross-product pairing is
already a supported public profile.

The [public-profile evidence matrix](baseline-readiness.md#public-profile-evidence-matrix)
records which complete factories have format, streaming, tooling, fuzz, and
completion coverage. It deliberately does not represent unpublished
cross-product pairings as callable C ABI features.

## Lifecycle

1. Call the matching `marc_blocked_huffman_config_init()` or
   `marc_adaptive_huffman_config_init()` or
   `marc_dynamic_range_config_init()`, `marc_rans_config_init()`, or
   `marc_tans_config_init()`, `marc_lz77_config_init()`,
   `marc_lz77_blocked_huffman_config_init()`,
   `marc_lz77_adaptive_huffman_config_init()`,
   `marc_lz77_dynamic_range_config_init()`,
   `marc_lz77_rans_config_init()`,
   `marc_lz77_tans_config_init()`,
   `marc_lzss_config_init()`, `marc_lzss_blocked_huffman_config_init()`,
   `marc_lzss_adaptive_huffman_config_init()`,
   `marc_lzss_dynamic_range_config_init()`,
   `marc_lzss_rans_config_init()`,
   `marc_lzss_tans_config_init()`,
   `marc_lz78_config_init()`, `marc_lz78_blocked_huffman_config_init()`,
   `marc_lz78_adaptive_huffman_config_init()`,
   `marc_lz78_dynamic_range_config_init()`,
   `marc_lz78_rans_config_init()`, or
   `marc_lzw_config_init()`, `marc_lzw_blocked_huffman_config_init()`,
   `marc_lzw_adaptive_huffman_config_init()`,
   `marc_lzw_dynamic_range_config_init()`,
   `marc_lzw_rans_config_init()`,
   `marc_lzd_config_init()`, `marc_lzd_blocked_huffman_config_init()`,
   `marc_lzd_adaptive_huffman_config_init()`,
   `marc_lzd_dynamic_range_config_init()`,
   `marc_lzd_rans_config_init()`, `marc_lzd_tans_config_init()`, or
   `marc_lzmw_config_init()`, `marc_lzmw_blocked_huffman_config_init()`,
   `marc_lzmw_adaptive_huffman_config_init()`, or
   `marc_lzmw_dynamic_range_config_init()` for encode or decode direction.
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
Its public completion matrix fixes 64-byte frames and blocks and verifies
empty input, every one-byte value, representative binary and generated data,
determinism, one-byte and mixed chunking, repeated terminal calls, and atomic
rejection of a malformed final frame entirely through these C functions.
The public completion matrix fixes 64-byte frames and verifies every one-byte
value, representative binary and generated data, exact determinism, one-byte
and mixed chunking, repeated terminal calls, and atomic rejection of a
malformed fourth frame entirely through these C functions.
The LZ77 plus tANS profile has the same three-region contract. Encoding
partitions secondary storage into canonical LZ77 tokens followed by the
complete tANS frame and requires no views. Decoding uses primary for the
serialized frame, partitions secondary into private token and raw staging,
and receives aligned opaque tANS views in the third region. The requirements
query must be repeated after changing direction, either frame dimension,
original size, LZ77 parameters, or any local limit. Factory failure leaves the
transform pointer null, and the header never exposes `TansBlockView`. Its
profile-specific completion matrix uses 64-byte frames and blocks and covers
empty input, all one-byte values, representative binary and generated data,
deterministic re-encoding, one-byte and mixed chunking, repeated terminal
calls, and atomic malformed-final-frame rejection through these C functions.
The `lz77-tans` CLI selector uses only this lifecycle, allocating every region
from the direction-specific query without reproducing a private partition or
view layout.
The dependency-free `lz77-tans` benchmark also uses only this lifecycle and
reports all six queried workspace extents after its pre-timing round trip.
Interoperability schema 26 serializes the unchanged CLI-selected C profile and
introduces no ABI or stream-format variant.
LZSS also uses no views workspace. Its encoder's exact worst-case token payload
is two bytes per raw byte; its decoder uses the same frame-atomic workspace
roles as LZ77.
The LZSS plus Blocked Huffman factory keeps the same three-region convention as
the LZ77 composition. Its secondary region contains token staging followed by
serialized-frame staging while encoding, or token staging followed by raw
staging while decoding. Only decode requires aligned entropy-block views.
Call `marc_lzss_blocked_huffman_workspace_requirements()` again after changing
any size, LZSS parameter, or local limit.
The LZSS plus Adaptive Huffman factory uses the same primary and secondary
roles without a views region. Encoding partitions secondary storage into
canonical LZSS token staging followed by the complete serialized frame;
decoding partitions it into token staging followed by private raw staging.
Each outer frame resets both LZSS history and its one FGK tree. Call
`marc_lzss_adaptive_huffman_workspace_requirements()` again after changing the
direction, known original size, frame size, LZSS parameters, or any hard limit.
The LZSS plus Dynamic Range factory has the same byte-only ownership and no
views region. Encoding partitions secondary storage into canonical LZSS tokens
and one complete range-coded frame; decoding partitions it into token staging
and private raw staging. The configuration fixes Dynamic Range variant 1 and
has no entropy-block parameter. Call
`marc_lzss_dynamic_range_workspace_requirements()` again after changing the
direction, known original size, frame size, LZSS parameters, or any hard limit.
Creation failure leaves the caller's transform pointer null.
The LZSS plus Dynamic Range public completion matrix uses only its public
functions with 64-byte frames. It covers required binary classes,
deterministic multi-frame output, arbitrary chunking, and sticky terminal
behavior.
The LZSS plus rANS factory uses the common three-region convention. Encoding
uses primary for raw-frame collection, partitions secondary into canonical
LZSS tokens and one complete rANS frame, and reports zero views. Decoding uses
primary for the serialized frame, partitions secondary into token and private
raw staging, and receives aligned opaque rANS block views. Call
`marc_lzss_rans_workspace_requirements()` again after changing direction,
known original size, either block dimension, LZSS parameters, or any hard
limit. The public header exposes only byte counts and alignment.
The LZSS plus rANS public completion matrix uses only these functions with
64-byte raw and entropy blocks. It covers empty input, all 256 one-byte values,
all byte values in sequence, repetition, generated binary data, frame-boundary
lengths, deterministic multi-frame output under four chunk schedules, sticky
completion, and sticky failure. Corruption, truncation, or one trailing byte
in the fourth frame commits the first 192 raw bytes and leaves the final
output sentinel unchanged.
The LZSS plus tANS factory follows the same three-region ownership policy.
Encoding uses primary storage for raw-frame collection and partitions
secondary storage into canonical LZSS tokens and one complete tANS frame;
decoding uses primary for the serialized frame, partitions secondary into
token and private raw staging, and receives aligned opaque tANS block views.
Call `marc_lzss_tans_workspace_requirements()` again after changing direction,
known original size, either block dimension, LZSS parameters, or any hard
limit. The `lzss-tans` CLI selector allocates only the reported extents and
uses the common public transform lifecycle without naming a private view type.
The dependency-free `lzss-tans` benchmark uses the same lifecycle, performs
an untimed byte-exact round trip before measurement, and reports each queried
workspace extent without reproducing private partitions.
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
The public completion matrix uses only this C lifecycle with 64-byte frames.
It covers every one-byte value, representative binary and generated input,
frame-boundary lengths, deterministic one-byte and mixed chunking, repeated
terminal calls, and atomic rejection of a corrupted, truncated, or extended
fourth frame.
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
The public completion matrix uses only this lifecycle with 64-byte frames and
covers required binary classes, deterministic one-byte and mixed chunking,
repeatable terminal states, and frame-atomic rejection of corrupted,
truncated, and extended fourth frames.

The LZW plus rANS factory uses the same three-region ownership with an explicit
entropy block size and maximum block count. Encoding uses primary storage for
one raw frame, secondary storage for packed LZW bytes followed by one complete
rANS frame, and aligned opaque views for LZW encoder entries. Decoding uses
primary storage for one encoded frame, secondary storage for packed bytes
followed by private raw staging, and aligned opaque views containing rANS block
views followed by LZW phrase entries. Call
`marc_lzw_rans_workspace_requirements()` again after changing direction,
original size, frame size, entropy block size, maximum code width, or any hard
limit. Private C++ record definitions never enter the C ABI. The public
completion matrix uses only this lifecycle with 64-byte frames and blocks and
covers required binary classes, deterministic one-byte and mixed chunking,
repeatable terminal states, and frame-atomic rejection of corrupted,
truncated, and extended fourth frames.

The LZW plus tANS factory uses the same three-region ownership and explicit
block controls. Encoding places one raw frame in primary storage, then packed
LZW bytes and one complete tANS frame in secondary storage; aligned views hold
LZW encoder entries. Decoding places one encoded frame in primary storage,
then packed bytes and private raw staging in secondary storage; aligned views
hold tANS block views followed by LZW phrase entries. Call
`marc_lzw_tans_workspace_requirements()` again after changing direction,
sizes, width, block settings, or any hard limit. All typed layouts remain
private to the factory. The public completion matrix uses only this lifecycle
with 64-byte frames and blocks and covers required binary classes, repeat
determinism, arbitrary chunking, stable terminal calls, and frame-atomic
rejection of corrupted, truncated, and extended fourth frames.
The bounded fuzz harness also drives this public decoder with deterministic
chunks under fixed 8-KiB input, 4-KiB output, storage, and call ceilings while
independently exercising the private complete-frame boundary.
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
The public completion matrix exercises this factory exclusively, including
zero encoder views for empty and one-byte input, byte-identical repeated and
arbitrarily chunked encoding, and atomic rejection of a malformed final frame.
The LZD plus Dynamic Range factory retains the same three-region ownership.
Encoding uses token staging followed by one complete range-coded frame;
decoding uses token staging followed by private raw staging. Its aligned opaque
region contains encoder entries or a checked phrase-entry/expansion-stack
layout. Call `marc_lzd_dynamic_range_workspace_requirements()` again after
changing direction, original size, frame size, maximum entries, or any hard
limit; no C++ record type crosses the ABI. The public completion matrix uses
only this lifecycle and covers zero-view edge cases, byte-identical repeated
and arbitrarily chunked encoding, sticky terminal results, and frame-atomic
rejection of malformed final frames.
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
The public completion matrix uses only this lifecycle with 64-byte frames and
blocks and covers required binary inputs, deterministic repeated and arbitrarily
chunked encoding, sticky terminal results, and frame-atomic rejection of a
corrupt, truncated, or extended final frame. The bounded decoder fuzz boundary
exercises this same public lifecycle alongside the private complete-frame
decoder while retaining fixed caller-owned storage and call ceilings. The
`lzd-rans` CLI selector likewise uses only this lifecycle and allocates all
three regions from the returned sizes and alignment. The benchmark adapter
uses the same lifecycle independently in each direction and reports those
queried regions after an untimed verified round trip. Interoperability schema
24 serializes the unchanged CLI-selected C profile and introduces no ABI or
stream-format change.
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
The public completion matrix uses only this lifecycle with 64-byte frames and
blocks and covers required binary classes, repeat determinism, arbitrary
chunking, stable terminal calls, and frame-atomic rejection of corrupted,
truncated, and extended fourth frames.
The `lzd-tans` CLI selector uses this lifecycle exclusively, allocating the
three returned regions with the reported alignment and retaining the common
transactional file-publication boundary.
The benchmark adapter constructs fresh transforms through the same public
lifecycle, verifies one complete round trip before timing, and reports all
three requirements for each direction.
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
The public completion matrix exercises this factory exclusively, including zero
encoder views for empty and one-byte input, byte-identical repeated and
arbitrarily chunked encoding, sticky success and error results, and atomic
rejection of a malformed final frame.
The LZMW plus Dynamic Range factory uses the same canonical-reference and
raw/frame secondary regions. Its aligned opaque region holds encoder entries
or the decoder's checked phrase-entry/expansion-stack layout. Query
`marc_lzmw_dynamic_range_workspace_requirements()` again whenever the
direction, known original size, frame size, maximum entries, or a hard limit
changes. No C++ record type crosses this ABI.
The LZMW plus Dynamic Range completion matrix uses only this lifecycle,
including zero encoder views for empty and one-byte input, byte-identical
repeated and arbitrarily chunked encoding, sticky success and error results,
and atomic rejection of a malformed final frame.
The LZMW plus rANS factory follows the same three-region ownership model while
adding rANS block views to the decoder's aligned opaque layout. Query
`marc_lzmw_rans_workspace_requirements()` whenever direction, known original
size, frame size, entropy block size, maximum entries, or any hard limit
changes. Its completion matrix and bounded dual-path fuzz target use only this
public lifecycle. The `lzmw-rans` CLI selector likewise allocates all regions
and their alignment from that query and adds no private layout dependency. The
benchmark adapter independently queries the same three regions in each
direction and verifies an exact round trip before timing.

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
`MARC_PROCESS_RESET_BLOCK` is currently unsupported for this profile.

Errors are terminal for a transform and use stable public categories. A decoder
may already have committed earlier validated frames when a later frame is
malformed. The malformed frame itself produces no output.

## Configuration rules

Do not initialize configuration structures manually. The initializer fills
`struct_size`, `abi_version`, defaults, and reserved fields. Changing tags or
reserved fields is invalid. Encoder `original_size` is mandatory format input;
unknown-size encoding is outside the baseline profile.

Decoder limits are local policy, not values accepted from the stream. Smaller
limits reduce workspace requirements and the accepted attack surface. The
defaults are conservative but can request substantial workspace, particularly
the 128 MiB maximum buffered frame body.

See [`../examples/c_roundtrip.c`](../examples/c_roundtrip.c) for a complete
single-call round trip. Real streaming callers should also handle partial
consumption and production as described above.
