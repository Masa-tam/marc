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
