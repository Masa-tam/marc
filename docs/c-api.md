# C API

The public C ABI is declared by `<marc/marc.h>`. It exposes Blocked Huffman,
Adaptive Huffman, Dynamic Range, rANS, tANS, LZ77 variant 1, the LZ77 plus
Blocked Huffman profile, LZSS variant 1, the LZSS plus Blocked Huffman profile,
LZ78 variant 1, the LZ78 plus Blocked Huffman profile, LZW variant 1, the LZW
plus Blocked Huffman profile, LZD variant 1, the LZD plus Blocked Huffman
profile, and LZMW variant 1 and the LZMW plus Blocked Huffman profile with
known-size encoding and bounded caller-owned workspace. All functions are
`noexcept` in C++ translation units,
and no C++ type appears in the ABI.

## Profiles and composition

The C ABI exposes complete, validated stream profiles rather than separate
dictionary and entropy objects that callers combine at runtime. Each standalone
dictionary factory binds entropy `None`, and each standalone entropy factory
binds dictionary `None`. `marc_lz77_blocked_huffman_*`,
`marc_lzss_blocked_huffman_*`, `marc_lz78_blocked_huffman_*`,
`marc_lzw_blocked_huffman_*`, `marc_lzd_blocked_huffman_*`, and
`marc_lzmw_blocked_huffman_*` are the currently public
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
   `marc_lz77_blocked_huffman_config_init()`, or
   `marc_lzss_config_init()`, `marc_lzss_blocked_huffman_config_init()`,
   `marc_lz78_config_init()`, `marc_lz78_blocked_huffman_config_init()`, or
   `marc_lzw_config_init()`, `marc_lzw_blocked_huffman_config_init()`,
   `marc_lzd_config_init()`, `marc_lzd_blocked_huffman_config_init()`, or
   `marc_lzmw_config_init()`, `marc_lzmw_blocked_huffman_config_init()` for
   encode or decode
   direction.
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
LZSS also uses no views workspace. Its encoder's exact worst-case token payload
is two bytes per raw byte; its decoder uses the same frame-atomic workspace
roles as LZ77.
The LZSS plus Blocked Huffman factory keeps the same three-region convention as
the LZ77 composition. Its secondary region contains token staging followed by
serialized-frame staging while encoding, or token staging followed by raw
staging while decoding. Only decode requires aligned entropy-block views.
Call `marc_lzss_blocked_huffman_workspace_requirements()` again after changing
any size, LZSS parameter, or local limit.
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
