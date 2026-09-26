# Position-distance LZSS Dynamic Range streaming integration

Status: staged implementation design, 2026-09-26. The codec is still private.
This document follows BM-0131 and the
[compression-ratio study](lzss-contextual-ratio-64k.md).

## Identity and initial scope

Retain exactly format 2.0 dictionary 2/8 + context 1/9 + entropy 3/2 from
[the format specification](../format.md). A stream has a 112-byte header;
each frame resets the dictionary and all forty context models. Known original
size, frames of 1..65,536 raw bytes, a 65,536-byte window, and match lengths
3..258 remain the initial envelope. Only the last frame can be short.

Private incremental processing must produce the same complete stream as the
fixed-policy one-shot writer for the same input and parameters. Start with
eligibility 3 and exact indexed search; retain eligibility 4/5 and exhaustive
search for internal differential tests. Eligibility is an encoder parsing
choice, not a serialized dictionary minimum or a decoder acceptance filter.
The wire minimum remains 3 for every policy.

Existing published codec names, defaults, layouts and bytes remain stable.
Public admission requires a separate implementation step updating the format
status and every relevant public dispatch point together.

## Encoder state and publication

Use a private `core::Transform` with these states:

```text
draining_header -> collecting_raw -> preparing_frame -> draining_frame
                        ^                                  |
                        +----------------------------------+
                  final frame -> awaiting_end -> ended
                  any failure -> error
```

Validate configuration and workspace before exposing the stream header. Copy
consumed raw bytes into one owned frame span. When the required frame is full,
tokenize it once, retain its tokens, and run the prepared frame encoder into
the serialized-frame span. Internal entropy planning/replay still occurs;
this is one dictionary search per frame, not one entropy pass. Drain the
complete validated frame before reusing raw, token, operation or finder storage.
Retain offsets across calls; never retain a borrowed process-input span.

There is no whole-input sizing pass in the incremental encoder. Workspace uses
the conservative frame ceiling. The one-shot writer remains the byte oracle.
Encoder failure can follow an already emitted header or earlier complete
frames; only success through EndOfStream establishes a complete archive.
Do not expose bytes from a failed frame preparation.

## Decoder state and publication

Use a separate private transform with these states:

```text
collecting_stream_header -> collecting_frame_header -> collecting_descriptor
    -> collecting_payload -> validating_frame -> draining_raw_frame
    -> collecting_frame_header (or awaiting_end -> ended)
any failure -> error
```

Check the exact identity and local limits before accepting frames. Check the
fixed frame header, descriptor, sequence, raw extent, counts and checked payload
extent before copying the payload into the bounded serialized-frame workspace.
Reuse complete-frame validation, canonical Range replay and typed reconstruction.
Publish raw bytes only after the entire frame passes, then drain that frame
before accepting the next one. Workspace is allocated from local configuration;
stream fields never increase it.

Publication is atomic per frame. If frame two is corrupt, output from valid
frame one may already have been returned; frame two contributes zero output.
This differs deliberately from the existing strict one-shot decoder, which
validates all input before publishing any whole-stream output. Applications
requiring whole-file atomicity must stage output until successful completion.

After the declared original size, await explicit EndInput to reject trailing
data even when it arrives in a later call. Empty input is a header-only stream.
EndInput before a complete required header, descriptor, payload or raw extent
is malformed input. A full final frame is as valid as a short final frame.

## Process, flags and errors

Use the existing consumed/produced `core::ProcessResult` contract. NeedOutput
requires pending bytes; NeedInput requires additional input; Progress requires
at least one nonzero count. Zero-capacity calls must not lose pending data.
EndOfStream requires all pending output drained and EndInput accepted.
Repeated calls in ended return EndOfStream with zero counts. Errors are sticky,
with zero counts on subsequent calls and stable error category and position.

Flush drains available bytes but does not close a short frame, reset a model or
change stream bytes. ResetBlock and unknown flag bits are unsupported before
consuming input or producing output on that call. EndInput applies to the final
supplied span: if backpressure leaves a suffix unconsumed, the caller resubmits
that suffix with EndInput. Latch completion only when that span is consumed.
Encoder input must match the known original size; early final input or excess
raw bytes is invalid_argument. Decoder truncation/corruption is malformed_stream;
unsupported identity/features are unsupported; resource failures are limit_exceeded.
Pin error offsets to the stream/frame/descriptor or payload location that fails,
independently of how the caller split input. Never claim rollback of bytes
already reported as consumed or produced.

Reject overlapping input/output and overlap with live workspace before changing
state for the call. All workspace spans remain caller-owned and valid for the
transform lifetime. Hash taps observe only committed counts; replay, preparation
and repeated drain calls must not hash bytes twice. No new stored hash feature
is implied by this integration.

## Workspace and resource profile

Let F be the configured maximum frame extent, T at most F, and S the checked
complete serialized-frame ceiling `18*F+85` (payload `18*F+5`). Share one checked
layout implementation between query and construction, including alignment.

| Direction | Primary bytes | Secondary bytes | Aligned views |
| --- | --- | --- | --- |
| Encode | raw frame, F | serialized frame, S | F typed tokens, 5F modeled operations, indexed finder workspace |
| Decode | serialized frame, S | raw frame, F | F typed tokens |

Derive finder size from its existing query. Charge simultaneously live regions,
alignment padding, fixed model arrays, totals, grammar/replay state and stream
state to the aggregate policy without double counting reused regions. Document
fixed transform storage separately from the three caller-provided extents;
neither these extents nor their sum is a peak-RSS measurement. Model size is not
merely 2522 frequency entries. Construction validates alignment, disjointness,
checked extent arithmetic and the same limits used by query, with no steady-state
allocation. Reject one-byte-short required buffers before publishing a handle.

The first new family has a single resource profile. Its initializer must give
a coherent usable configuration; it does not need a ceremonial apply_profile
helper. Follow the existing [profile contract](contextual-profile-application.md)
if additional profiles are later supported. Allow stricter caller hard limits;
reject incoherent settings rather than raising them. Freeze numeric defaults
and actual direction-specific extents with the private workspace implementation
before adding the C ABI. Do not use benchmark scratch totals as the query.

## Planned public names and configuration

Use the additive family `marc_lzss_position_distance_dynamic_range_*` and CLI
codec `lzss-position-distance-dynamic-range`. The name describes the model;
it does not expose the development ordinal context-9. The C family will provide
`config_init`, `workspace_requirements` and `create`, returning the existing
`marc_transform` handle for process/destroy. No compatibility alias is needed.

Use a new size/version-tagged config with direction, original_size, frame_size
and applicable hard limits, following the existing C ABI validation rules.
The initial public encoder fixes eligibility 3 and indexed search. Internal
experimental policies are not advertised as supported public finder strategies.
No change to the existing contextual Dynamic Range config is required. Public
decoding accepts the exact tuple and every legal token, independently of the
encoder policy that produced it. The initial CLI uses the complete initializer;
larger-window suffixes are not part of this family yet.

## Implementation and verification sequence

1. Implement a private incremental decoder and independently split fixed stream
   vectors. Cover complete first-frame publication followed by second-frame
   failure, header/descriptor/payload limits, trailing bytes in a later call,
   all terminal states and one-byte buffers.
2. Add the checked private workspace layout and encoder. Compare complete
   bytes with the one-shot writer and reference search across frame boundaries,
   Flush calls, partial EndInput and randomized chunking. Verify one dictionary
   search per frame and exact workspace boundaries.
3. Add bounded incremental fuzz coverage and hash-tap tests. Measure whole
   incremental encode/decode time and complete Silesia saved sizes, including
   the fixed-policy versus earlier selected-token distinction.
4. Add the C config/query/create path and explicit format/CLI admission with
   ABI, installed-consumer, example and wrong-codec rejection tests. Record
   actual memory requirements and pin compatibility with previous codec bytes.
5. Extend interoperability artifacts, verify Windows/Linux cross-decoding and
   deterministic archives, then review completion and release readiness.

This design completes the next planning step. The immediate implementation is
the private incremental decoder in item 1; public admission remains gated.

## Private decoder implementation

DD-1278 implements item 1 with `LzssPositionDistanceFrameStreamingDecoder`.
Its fixed frame-prefix staging validates the header and descriptor together
before payload buffering; complete-frame decoding still performs canonical
replay. It charges all supplied spans plus retained object/model state and
uses frame-level publication. TVG-1147 defines the decoder's split, boundary,
failure and limit coverage. The next stage is the checked private workspace
layout and encoder in item 2.

## Private workspace implementation

DD-1279 implements the shared layout portion of item 2. The query uses configured
F, including for empty input, and reports raw/serialized/view extents, alignment,
model state, owner-supplied retained stream state and their aggregate separately.
Partition recomputes offsets, charges all supplied capacity and starts typed
object lifetimes only after all checks pass. Returned views are trimmed to the
required extents. The decoder constructor shares the aggregate calculation.
TVG-1148 pins exact limits and short/overlapping storage rejection. The next step
is the private incremental encoder using this layout; its concrete retained
object size and eventual public defaults are not frozen by the test owner size.

## Private encoder implementation

DD-1280 completes the incremental encoder portion of item 2. The concrete owner
supplies its own size to workspace query/partition, shares explicit header
serialization, and calls the raw-frame encoder once per collected frame. Output
draining retains the successful serialized bytes without repeating preparation.
TVG-1149 compares complete bytes against the one-shot oracle under alternate
policies, chunking and Flush, and pins delayed EndInput and frame failure behavior.
The next stage is bounded incremental fuzz and hash-tap coverage in item 3,
followed by whole-stream measurements. No public codec is admitted by this step.

## Incremental hash and fuzz coverage

DD-1281 implements the bounded validation portion of item 3. External SHA-256
taps check consumed and produced prefixes independently; malformed input can be
consumed without its unvalidated frame contributing raw output. The existing
private fuzz target now compares chunk schedules and incremental/reference
encoding, with fixed capacity/call ceilings and boundary cases at initialization.
TVG-1150 records the deterministic schedules and smoke budget. Whole-stream
incremental measurements remain the next step; public admission is still gated.

## Incremental whole-stream measurement

DD-1282 and BM-0132 add a reusable incremental mode to the private stream
benchmark. Mozilla retains exact one-shot archive bytes while preparing each
frame once. Both transform directions are timed with bounded caller workspaces;
oracle generation and whole-file harness storage are reported separately.
This is one corpus member and one parsing policy, not a general performance
claim. Wider corpus measurement and public C ABI/CLI design review remain before
admission; neither public identifiers nor defaults change in this step.

## Remaining corpus measurements

BM-0133 covers the other eleven Silesia members through DD-1283's checkpointed
runner. Every member preserves the one-shot archive and has lower median encode
and decode times for the incremental path. Together with the separate Mozilla
measurement, this closes the planned fixed-policy corpus comparison, not public
admission. Next review the C ABI/CLI integration contract, limits and public
identity handling before implementing public factories or changing defaults.

## Public integration contract (planned, not yet admitted)

BM-0132/BM-0133 complete the fixed-policy measurement gate. This section is the
integration contract. Initializer/query/factory symbols and C consumers are
implemented, including locally verified installed-package examples. Hosted
package CI and public-admission gates remain pending. Explicit CLI selection
and wrong-codec rejection tests are now implemented.

### Additive C family

Add `marc_lzss_position_distance_dynamic_range_config` and the three functions
`marc_lzss_position_distance_dynamic_range_config_init`,
`marc_lzss_position_distance_dynamic_range_workspace_requirements`, and
`marc_lzss_position_distance_dynamic_range_create`. Keep `MARC_ABI_VERSION` 1
and existing generic workspace/result/transform types unchanged. Follow the
existing initializer `(direction, config)` and query/create argument ordering.
No aliases, new public finder values, profile enum or `apply_profile` function
are introduced. Document that initializer defaults suffice for this single
supported configuration range.

The new config contains, in order, `uint32_t struct_size`, `abi_version`,
`marc_direction direction`, `uint32_t reserved`, `uint64_t original_size`,
`uint32_t frame_size`, `reserved2`, then uint64_t limits:
`max_total_output_size`, `max_frame_size`, `max_block_size`,
`max_compressed_payload_size`, `max_internal_buffered_bytes`, `max_lz_distance`,
`max_lz_match_length`, `max_entropy_table_entries`, `max_range_model_total`,
`max_expansion_ratio`, and `expansion_slack`.
Require zero reserved fields, exact struct size, the supported ABI version and
a valid direction. Do not expose window/minimum/maximum match fields whose sole
initial values would be 65536/3/258. Encoder eligibility 3 and indexed search
remain fixed; other private parsing policies are not public options.
Unexposed core limits retain current DecoderLimits defaults; no public field is
silently reused for a different unit or boundary. Validate core relationships
such as max_frame_size <= max_total_output_size and max_block_size <= aggregate.

| Setting | Initializer value | Meaning |
| --- | ---: | --- |
| original_size | 0 | Encode: exact known size; decode: ignored |
| frame_size | 65536 | Encode: 1..65536 raw bytes; decode: ignored |
| max_total_output_size | 1099511627776 | 1 TiB hard output ceiling |
| max_frame_size / max_block_size | 65536 each | Raw-frame limits |
| max_compressed_payload_size | 1179653 | `18*65536+5`, excludes 80-byte frame metadata |
| max_internal_buffered_bytes | 134217728 | 128 MiB aggregate ceiling |
| max_lz_distance / max_lz_match_length | 65536 / 258 | Hard reference limits |
| max_entropy_table_entries | 2522 | Frequency entries across forty contexts |
| max_range_model_total | 32768 | Per-model frequency total ceiling |
| max_expansion_ratio / expansion_slack | 1024 / 1048576 | Existing core ratio semantics; slack in bytes |

Decoder construction does not require the original size or frame size before
reading the header. Query uses `F=min(max_frame_size,65536)` and a synthetic
known-empty header only to calculate capacity; actual received original size is
checked against max_total_output_size. Encode uses frame_size and original_size.
Both directions use the checked private layout and reject incoherent limits,
including block/payload ceilings insufficient for the selected F. Callers may
reduce frame capacity with coherent limits; query never silently raises them.
Larger local ceilings do not extend the fixed wire limits. Unknown-size encoding
remains unsupported. Decode ignores encode-only fields even if they would be
invalid encoding parameters; size/version/reserved validation still applies.

### Workspace and construction

Map primary/secondary to raw/serialized on encode and serialized/raw on decode.
Views holds aligned typed storage. Query reports actual required caller spans,
not aggregate bytes or allocator overhead; derive them from the checked layout.

Charge the opaque `marc_transform` handle as well as the concrete transform,
model/replay state and retained workspace. Subtract the handle's actual sizeof
with checked arithmetic before passing the remaining aggregate budget to both
private query and constructor. BM-0132's private charges of 7,804,677/2,037,541
bytes are observations for one x64 build, not frozen public constants. Query and
create must share accounting, including exact-limit and one-byte-under tests.
Use nothrow allocations and release the implementation if handle publication
fails. No allocator callback or steady-state allocation is introduced.

Accept larger buffers but retain only queried prefixes, as existing factories
do. Validate prefix overlap/alignment and pass only those prefixes to partition
and transform. Unused tails are neither retained nor charged. Validate metadata
overlap before writing config/query/handle outputs; do not corrupt config or
workspace by setting an aliased handle output to null. For a disjoint output
pointer, create publishes null before failure-prone work. Query leaves its
output unchanged on failure. Do not claim to validate arbitrary dangling C
pointers; supplied objects must be valid for the duration of each call.

Invalid size/version/direction/reserved fields and short/misaligned/overlapping
workspace return INVALID_ARGUMENT. Valid configuration exceeding resource
ceilings returns LIMIT_EXCEEDED; allocation failure returns OUT_OF_MEMORY.
Preserve existing process error mapping, sticky states and per-frame publication.
Hashing stays an external tap; applications requiring whole-file atomicity must
stage output until EndOfStream.

### CLI and admission gates

Add only `lzss-position-distance-dynamic-range`, explicitly selected for encode
and decode. Use this initializer and fixed encoder policy. Do not change the
default codec, existing contextual names or their bytes. Header inspection must
not allocate from untrusted fields or upgrade a selected decoder. Reject
unsupported finder/profile options explicitly rather than silently ignoring
them. No larger-window suffixes belong to this family initially.

Admit only format 2.0 dictionary 2/8, context 1/9, entropy 3/2. Old decoders must
continue rejecting the new identity; the new decoder must reject old or crossed
identities. Do not broadly admit other reserved experimental contexts. Update
public format status and relevant explicit dispatch points together, preserving
the strict private parser as oracle.

Stage implementation: C initializer/query and negative tests; factory/process
integration and exact private/public byte comparison; static/shared C consumers
and installed-package example; CLI selection and wrong-codec tests; documented
format admission audit; interoperability artifacts and external cross-platform
verification. Public completion requires the final gate. Initializer/query and
factory/process now have C integration tests. Standalone C11 consumers cover
both enabled library kinds; isolated static-linked allocation-failure injection
checks owner/handle cleanup. The installed examples project now includes a
streaming C consumer per exported library kind, configured in the CI package matrix.
The explicit CLI adapter uses the full initializer and rejects unsupported
options; round trips and reciprocal wrong-codec rejection are covered.
Admission gates remain pending; no DLL allocation interposition is claimed.

## Format admission audit (2026-09-26)

Scope: audit the implemented explicit C/CLI paths, without declaring external
interoperability or adding the identity to the completed profile inventory.
The only candidate is format 2.0, dictionary 2/8, context 1/9, entropy 3/2.

| Boundary | Implementation and decision | Evidence |
|---|---|---|
| C configuration | `prepare_position_distance_config` in `src/marc.cpp` fixes the identity and derives storage from local limits, not stream fields | Config/query, exact aggregate and allocation-failure tests |
| Serialized header | `parse_lzss_position_distance_stream_header` selects the position-distance branch of `parse_stream_impl` in `src/frame/lzss_short_match_preflight.cpp` | Header mutations/truncation tests and C identity-grid rejection |
| Incremental decode | `LzssPositionDistanceFrameStreamingDecoder` calls the strict header parser before frame handling; errors are sticky and frame output is private until validation | Streaming, C factory and HashTap tests |
| Legacy field-context parser | `parse_typed_context_stream_header` is specific to the older field-context family, not a universal dispatcher; keep its accepted set unchanged | Legacy parser rejects the new header without publishing metadata |
| CLI | The explicit selector calls the dedicated C factory; no header-driven selection or limit upgrade | Reciprocal wrong-codec rejection, unsupported options, trailing data and output cleanup tests |
| Reference path | Keep the one-shot decoder and encoder as internal differential oracles | Chunked C output agrees with the independently exercised reference path |

The audit found no missing automatic dispatcher that should be broadened.
Adding dictionary 8/context 9 to the legacy field-context parser would be the
wrong integration: it would widen old codec acceptance and apply incompatible
model assumptions. The dedicated path already performs the required exact
identity check. Contexts 6, 7 and 8 remain separate private experiments; sharing
bounded implementation helpers does not grant them public admission.

The C boundary regression enumerates dictionary variants 0..9 against context
variants 0..10, excluding the one valid pair (109 rejections). It additionally
mutates the other identity words and high bytes (14 rejections), for 123 invalid
headers at both one-byte and complete-header chunk sizes. Require UNSUPPORTED,
zero published bytes, unchanged output sentinels and sticky failure. A valid
empty stream is the positive control. This grid is a regression boundary, not
a claim of exhaustive validation of all 16-bit values or malformed payloads.

The exact candidate is now appended in interoperability schema 58 as archive 68,
with frozen historical-prefix and schema-conversion checks. Next obtain hosted CI and the
external four-way verification, then update completion status. Do not infer
cross-platform success from this local audit. Existing stream bytes and the
general format 1/legacy Format 2 parser contracts remain unchanged.

## External verification and completion review (2026-09-27)

The hosted-CI and external-exchange gates described above now have evidence:
the maintainer reported successful CI and four 68-archive schema-58 verifier
passes at `559c16a8280687f1ad57602ae01e133dbece3611`. Producer/consumer
directions and the evidence scope are recorded in `../interoperability.md`.
This includes the corrected shared-only/static-only installed-package checks.

The remaining step is a consolidated public-completion review, not another
stream-format change: map the C ABI tests to the readiness matrix, add any
missing public-boundary coverage, and only then update the completed inventory
and reserved/staged wording consistently. Preserve the strict legacy parser,
fixed initial profile and all previous archive bytes. Do not generalize this
x64 exchange to other architectures, longer fuzz campaigns or other policies.
