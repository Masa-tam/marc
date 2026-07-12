# C API

The public C ABI is declared by `<marc/marc.h>`. It exposes Blocked Huffman and
Adaptive Huffman variant 1 with known-size encoding and bounded, caller-owned
workspace. All functions are `noexcept` in C++ translation units, and no C++
type appears in the ABI.

## Lifecycle

1. Call the matching `marc_blocked_huffman_config_init()` or
   `marc_adaptive_huffman_config_init()` for encode or decode direction.
2. Set the desired encoder sizes or decoder hard limits.
3. Call the matching workspace-requirements function.
4. Allocate each reported workspace, respecting `views_alignment`.
5. Call the matching create function and retain every workspace unchanged until
   after `marc_transform_destroy()`.
6. Repeatedly call `marc_transform_process()`, advancing input and output only
   by the reported consumed and produced counts.
7. Destroy the handle. Destroying a null handle is valid.

The library owns the opaque handle. It does not own the three workspaces or any
input/output buffer. No allocator callback is required by this profile.

For either encoder, `primary_bytes` is raw-frame storage and `secondary_bytes`
is serialized-frame storage. For either decoder, `primary_bytes` is serialized-
frame storage and `secondary_bytes` is decoded-frame storage. Blocked Huffman
decoding additionally uses `views_bytes` for a private block table; Adaptive
Huffman requires no views workspace. Adaptive encoder requirements
conservatively allow 264 bits per input symbol before fixed frame overhead.

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
