# Design decisions

This development record is indexed from [`README.md`](README.md).

## DD-001: Language and build system

- Date: 2026-07-12
- Status: accepted

Use C++20 without compiler-specific language extensions. Use CMake as the
canonical build description. MSVC is the reference implementation environment,
while portable behavior remains mandatory.

## DD-002: C ABI and exceptions

- Date: 2026-07-12
- Status: accepted

Develop the internal C++ core together with a deliberately small C ABI. The ABI
uses opaque handles, fixed-width integer fields, stable numeric error codes, and
caller-owned buffers. Public symbolic values use fixed-width integer typedefs
rather than ABI-dependent C enum storage. Exceptions are not an API mechanism and never cross the
C boundary. ABI adapters translate allocation failure to `out_of_memory` and
other unexpected failures to `internal_error`.

The ABI version is independent of the stream-format version. Extensible public
configuration structures will begin with their structure size and ABI version.

## DD-003: Library forms

- Date: 2026-07-12
- Status: accepted

Build static and shared libraries from the same source list. On Windows, the
shared library is `marc.dll` with its import library, while the static archive
uses the distinct name `marc_static.lib` to prevent artifact collisions.

## DD-004: Frame and entropy-block relationship

- Date: 2026-07-12
- Status: accepted

Entropy blocks never cross frame boundaries. A frame contains zero or more
complete entropy blocks followed by an optional short final block. Closing a
frame closes the current entropy block and resets frame-scoped state.

## DD-005: Initial allocation and size scope

- Date: 2026-07-12
- Status: accepted

Allocator callbacks are outside the initial API. The baseline encoder requires
the original uncompressed size before it emits the stream header. Supporting an
unknown original size requires an explicit later format variant or compatible
versioned extension.

## DD-006: Independent implementation terminology

- Date: 2026-07-12
- Status: accepted

Describe the project as a **specification-driven independent implementation**.
Do not claim a formal clean-room process or a legal guarantee. Algorithm work
is derived from recorded specifications, papers, and independently written
designs rather than implementation source code with incompatible provenance.

## DD-007: Foundational integer and bit primitives

- Date: 2026-07-12
- Status: accepted

Serialization primitives accept bounded spans and explicit offsets. They return
failure without performing an out-of-bounds access. Offset and allocation-size
calculations use checked addition and multiplication helpers.

The bit writer retains at most one pending byte across calls. A write may consume
bits without producing a byte, and a later call may produce that byte without
new logical input. Finishing emits the pending byte with zero high-bit padding.
The bit reader exposes consumed-byte and produced-bit counts independently and
can validate zero padding before byte alignment. These low-level results use a
dedicated completion status rather than the transform-level `Progress` status.

## DD-008: Limits precede frame-format allocation

- Date: 2026-07-12
- Status: accepted

Keep policy limits separate from decoder state and parsed frame bounds. Validate
the policy at decoder creation. After reading only the bounded fixed header,
validate all declared regions and their checked sum before allocating model or
block buffers. Track cumulative output separately so individually valid frames
cannot exceed the whole-stream limit.

Expansion validation uses `compressed_size * ratio + fixed_slack`. The fixed
slack permits valid small and low-payload frames, while the ratio constrains
large malicious expansion. Overflow in this calculation saturates the allowed
value at the unsigned 64-bit maximum; overflow in a declared allocation sum is
an error.

Initial conservative policy defaults are implementation defaults, not encoded
format limits. Applications may lower them. A future format may impose stricter
limits, but no format declaration may raise an application's configured limit.

## DD-009: GoogleTest as test-only infrastructure

- Date: 2026-07-12
- Status: accepted

Use GoogleTest for C++ unit tests and CTest discovery. Keep the pure-C ABI smoke
test as a C translation unit independent of GoogleTest. GoogleTest is a test-only
Git submodule pinned to a reviewed commit; marc library targets never link it.

Tests default to enabled only when marc is the top-level CMake project. A parent
project can build marc without initializing the submodule. When tests are
explicitly enabled and the submodule is absent, configuration fails with an
actionable diagnostic rather than downloading code implicitly.

After publication, Dependabot may propose gitlink updates. Updates are reviewed
and accepted only after CI succeeds; the default branch always records one exact
GoogleTest commit.

## DD-010: Collect fixed framing prefixes before semantic parsing

- Date: 2026-07-12
- Status: accepted

Use an allocation-free, compile-time-sized accumulator for stream and frame
header prefixes. It reports input consumption independently, accepts arbitrary
splits including one byte at a time, and never consumes bytes following the
prefix. Semantic parsers can obtain a read-only span only after collection is
complete.

Reset zeroes storage before reuse so stale header bytes are not exposed through
diagnostics or future mistakes. A zero-sized accumulator is valid and complete
at construction. The accumulator does not define a wire format; it supports a
future versioned parser without prematurely assigning format identifiers.

## DD-011: Version 1.0 has a fixed 64-byte stream prefix

- Date: 2026-07-12
- Status: accepted

Assign the `MARC` magic and stream-format version 1.0 to a 64-byte fixed prefix.
Keep ABI and stream-format versions independent. The prefix identifies the
pipeline, bounded region lengths, frame and entropy-block units, and the known
original size without serializing a native structure.

Reserve explicit algorithm IDs now, while leaving codec-specific parameter and
payload layouts pending until each codec's documentation-first implementation.
Only variant 1 baseline names from `format.md` are recognized. Feature flags,
hash descriptors, and header extensions remain zero until their exact layouts
are defined; this prevents permissive parsing from assigning them accidental
semantics.

## DD-012: Frames are sequenced deterministic decode units

- Date: 2026-07-12
- Status: accepted

Use a fixed 56-byte frame header with its own `MRF1` magic. Store both the
uncompressed output size and dictionary-serialized byte size because entropy
decoding and dictionary decoding have distinct bounded outputs. Store exact
compressed payload and block-descriptor lengths so both can be validated before
buffered entropy decoding.

Apply an independent local limit to dictionary-serialized bytes. This field is
the entropy decoder's output bound even when it can be streamed, so it must not
inherit either the raw frame limit or compressed-payload limit implicitly.

Derive the required uncompressed frame size from the stream's known original
size and configured frame size. Non-final frames cannot be short, frames cannot
continue after declared output completion, and sequence numbers must match the
controller's expected zero-based value. This makes frame boundaries and output
deterministic without a separate end marker.

## DD-013: Use MSBuild for the canonical Windows build

- Date: 2026-07-12
- Status: accepted

Use CMake's Visual Studio 2026 generator for the canonical Windows x64 build,
which delegates dependency tracking and compilation to MSBuild. Use Ninja as
the canonical preset on non-Windows hosts.

This choice follows a reproduced failure where localized MSVC `/showIncludes`
output was recorded with an encoding mismatch and Ninja retained an object built
against an older internal structure layout. A clean build detected no production
defect, but relying on localized diagnostic text is unnecessarily fragile.
Presets contain no installation path; CMake selects the Visual Studio instance,
while machine-specific overrides belong in ignored `CMakeUserPresets.json`.

## DD-014: Hash only explicitly committed byte prefixes

- Date: 2026-07-12
- Status: accepted

Inject hash implementations through a non-owning, no-throw `IHashAlgorithm`
interface. `HashTap::commit` accepts an available span and committed prefix
length, updates exactly that prefix, and tracks a checked 64-bit total. This
matches partial downstream writes without coupling codecs to hash algorithms.

Finalize requires an exactly sized caller-owned digest buffer and is terminal
after success. Hash algorithm failures and byte-count overflow are terminal;
invalid caller arguments leave the running state retryable. Reset is explicit
so frame, block, and whole-stream scopes cannot be conflated implicitly.

## DD-015: Blocked Huffman uses explicit canonical length tables

- Date: 2026-07-12
- Status: accepted

Represent a Huffman model as exactly 256 code-length bytes in symbol order.
Version 1 limits lengths to 15 bits. Construct optimal bounded lengths with
Package-Merge. Order leaves and packages first by total weight, then by their
lowest contained symbol, then with a leaf before a package, and finally by
stable creation order. Assign conventional canonical numeric codes and reverse
each code within its length only when forming the LSB-first encoder table.

Reject oversubscribed and incomplete multi-symbol code spaces. The sole
incomplete exception is a one-symbol model, represented by length 1 and code
zero. The all-zero model is valid only as the internal empty-block model; an
empty serialized entropy block is not permitted inside a nonempty frame.

## DD-016: Raw Blocked Huffman blocks win ties

- Date: 2026-07-12
- Status: accepted

For every nonempty block, select Huffman representation only when its complete
stored body is strictly smaller than raw bytes:

```text
256 + ceil(huffman_payload_bits / 8) < symbol_count
```

Otherwise select raw representation. The fixed per-block descriptor is common
to both alternatives and therefore cancels from the comparison. This makes the
choice deterministic and prevents table overhead from expanding small or
incompressible blocks.

## DD-017: Huffman decoding uses a bounded two-level strategy

- Date: 2026-07-12
- Status: accepted

Build a fixed 8-bit direct lookup table for short physical LSB-first codes and
a bounded binary table with at most 511 nodes for all codes. A lookup consumes
no more than the supplied available-bit count. An incomplete prefix reports
input starvation, while a missing branch reports a malformed code path.

Keep byte acquisition outside this primitive. This permits the same validated
table to serve incremental bit readers without embedding buffering policy or
assuming that 8 bits are always immediately available.

## DD-018: Reference block encoding sizes output before mutation

- Date: 2026-07-12
- Status: accepted

The one-block reference encoder completes frequency collection, length-limited
construction, canonical assignment, exact payload-bit counting, raw selection,
limit checks, and output-capacity checks before writing caller buffers. It
reports required model and payload sizes when capacity is insufficient.

This reference path uses fixed working storage and direct, bounds-proven
LSB-first packing. It is intentionally one bounded block rather than a public
one-shot stream codec; the later streaming controller owns block buffering and
draining.

## DD-019: Reference block decoding validates before output

- Date: 2026-07-12
- Status: accepted

The one-block reference decoder validates descriptor fields, exact model and
payload region sizes, local limits, the complete code-length model, decode-table
bounds, and zero padding before decoding. It then scans the Huffman payload
without output to prove the exact symbol count and exact bit termination.

Only after that scan succeeds does a second bounded scan write caller output.
This intentionally trades reference-decoder throughput for the stronger rule
that malformed input never exposes a partially decoded block. A later streaming
controller may commit smaller validated units, but must document that boundary.

## DD-020: Descriptor regions publish views only after full validation

- Date: 2026-07-12
- Status: accepted

The Blocked Huffman controller scans the complete interleaved descriptor/model
region before publishing block views. It proves the exact block count, normal
and final-short symbol counts, descriptor boundaries, model validity, local
table limits, payload-size sum, and combined buffer limit.

A second scan populates caller-owned views with descriptor values and 32-bit
model and payload offsets. This avoids per-frame allocation and prevents later
malformed blocks from leaving a partially initialized view list visible.

## DD-021: Frame decoding validates every payload before output

- Date: 2026-07-12
- Status: accepted

Given validated block views, frame decoding checks contiguous payload offsets,
model and payload bounds, every block's complete payload semantics, the exact
payload-region end, total dictionary-serialized output, and caller capacity
before writing any byte.

Only then are blocks decoded in order into disjoint output subspans. A reported
block error includes its zero-based block index and the stable block-level error
category. The reference path favors atomic frame output over throughput.

## DD-022: Frame encoding has a separate exact planning pass

- Date: 2026-07-12
- Status: accepted

Expose an internal no-output block planning operation and use it across every
frame block before mutation. The frame plan reports exact block count,
interleaved descriptor/model bytes, and concatenated payload bytes, including
raw/Huffman decisions and the final short block.

Encoding requires both caller-owned regions to satisfy that plan before the
first block is emitted. The reference implementation may recompute a Huffman
model while emitting rather than retain per-block plans; this preserves bounded
memory and deterministic bytes at the cost of extra CPU work.

## DD-023: The first complete frame path is deliberately profile-specific

- Date: 2026-07-12
- Status: accepted

Join the version 1 frame header and body only for the currently implemented
profile: no dictionary transform and Blocked Huffman variant 1 with no parameter
regions. Reject other otherwise-known pipeline IDs rather than interpreting an
incomplete implementation.

The encoder plans the complete serialized size before writing. The strict
decoder requires exactly one frame in its supplied span, rejects truncation and
trailing bytes, validates sequence and original-size-derived boundaries, then
uses the descriptor controller and atomic frame body decoder.

## DD-024: The known-size stream reference path is whole-stream atomic

- Date: 2026-07-12
- Status: accepted

For the implemented profile, serialize the fixed stream header followed by the
original-size-determined sequence of complete frames. Empty input is exactly
the stream header and contains no frame. Planning traverses every input frame
before any output mutation.

Strict decoding parses frame boundaries from validated headers and performs a
complete validation-only frame traversal before a second output traversal.
This reference behavior prevents corruption in a later frame from exposing an
earlier decoded frame. It is intentionally stronger than the future streaming
API, whose commit boundary will be documented explicitly.

## DD-025: Begin incremental work with a buffered encoder oracle

- Date: 2026-07-12
- Status: accepted

The first stateful encoder accumulates the known-size input in caller-owned
storage, invokes the complete reference stream encoder at `EndInput`, and then
drains caller-owned encoded storage. This immediately exercises independent
input consumption, output production, one-byte output, zero-byte final input,
and stable terminal states without changing the format.

It deliberately defers output until finish and treats non-terminal `Flush` as
non-mutating. `ResetBlock` remains unsupported until the frame-at-a-time state
machine can give it exact format semantics.

## DD-026: The buffered decoder preserves whole-stream validation atomicity

- Date: 2026-07-12
- Status: accepted

Accumulate encoded bytes in caller-owned storage until `EndInput`, then parse
the fixed stream header to validate required decoded capacity before invoking
the strict reference decoder. Reuse a caller-owned block-view array for every
frame. Only successful whole-stream validation enters the draining state.

Map malformed format conditions to a stable malformed-stream error and
insufficient encoded, decoded, or view workspace to out-of-memory. Both are
terminal for the instance. Repeated calls after success return end-of-stream.

## DD-027: The bounded encoder commits complete frames

- Date: 2026-07-12
- Status: accepted

Emit the stream header independently, then buffer exactly one uncompressed
frame and one serialized frame. A full normal frame is encoded and may be
drained before `EndInput`; only the final original-size-derived short frame
depends on stream completion.

Pending output has priority and may stop input consumption with `NeedOutput`.
Non-terminal `Flush` cannot shorten a deterministic outer frame and therefore
leaves a partial frame open. This reduces workspace from whole-stream size to
configured frame size without changing a byte of the format.

## DD-028: The bounded decoder commits only validated frames

- Date: 2026-07-12
- Status: accepted

Collect stream and frame prefixes incrementally, but allocate no
stream-controlled storage. After a frame header passes contextual and local
limit validation, collect its exact declared body into caller-owned frame
workspace, validate and decode it atomically, then drain that decoded frame.

Decoded output has priority over consuming the next frame. Consequently a call
may return `NeedOutput` with an encoded-input suffix unconsumed; `EndInput`
continues to apply when that suffix is re-presented. Truncation and trailing
data become terminal only after all earlier committed frame output is drained.

## DD-029: Normalize profiles and size workspaces before ABI construction

- Date: 2026-07-12
- Status: accepted

Keep version-specific header construction and workspace arithmetic behind an
internal factory boundary. For a known-size encoder, calculate from the smaller
of original size and configured frame size; an empty stream needs no frame
workspace. The raw-block upper bound is exact because a Huffman block is chosen
only when its model plus payload is strictly smaller than its raw payload.

A decoder cannot trust or inspect stream configuration before construction, so
its requirement query uses local limits: one frame header plus the maximum
internally buffered body, one maximum decoded frame, and the configured maximum
block-view count. Arithmetic overflow is reported in the stable limit-exceeded
category. This boundary keeps the future C ABI independent of internal enum
layout and avoids hidden allocation-size assumptions.

## DD-030: The initial C ABI uses caller-owned typed workspaces

- Date: 2026-07-12
- Status: accepted

Expose the frame-at-a-time Blocked Huffman variant 1 path through a small opaque
handle. Require callers to initialize a size-tagged, ABI-versioned config, ask
for workspace requirements, and retain three direction-dependent byte buffers
for the transform lifetime. Report the decoder view-buffer alignment explicitly
rather than exposing the internal view type.

The library owns only the fixed-size handle and implementation object, allocated
with non-throwing `new`; no allocator callback is introduced. Creation validates
all pointers, capacities, reserved fields, configuration limits, and alignment
before publishing a handle. Destruction accepts null. The process adapter
preserves independent input consumption and output production and maps internal
errors to the existing stable C status constants.

## DD-031: Install build-tree-equivalent static and shared targets

- Date: 2026-07-12
- Status: accepted

Install public headers, license notices, package version files, and every
enabled library in one relocatable CMake package. Export target names as
`marc::shared` and `marc::static`, matching the build-tree aliases. Do not invent
an ambiguous default target when both linkage forms are present; consumers make
the linkage choice explicitly.

Keep the example as both a top-level build target and a standalone consumer
project using `find_package(marc CONFIG REQUIRED)`. This makes the installed
package, transitive usage requirements, exported DLL import definition, and
public C header independently testable without internal include paths.

## DD-032: CI fixes toolchain families and tests installed consumers

- Date: 2026-07-12
- Status: accepted

Use the explicit GitHub-hosted `windows-2025-vs2026` runner rather than a moving
Windows alias, and build it through the Visual Studio 18 generator and MSBuild.
Use Ubuntu 24.04 with Ninja as the first non-Windows portability check. Both
jobs build shared and static libraries and run the complete test suite.

Add a separate four-entry package matrix for Windows and Ubuntu crossed with
shared-only and static-only builds. It disables repository tests and examples
while producing the package, then builds and runs the standalone pure-C example
from the installed prefix. Checkout the pinned GoogleTest submodule only in
test-suite jobs; package jobs must not depend on it.

Dependabot checks GitHub Actions and git submodules weekly. Updates remain pull
requests that must pass CI rather than automatically changing the pinned
GoogleTest revision or action major without review.

## DD-033: Adaptive Huffman baseline is framed FGK variant 1

- Date: 2026-07-12
- Status: accepted

Use byte-alphabet FGK as Adaptive Huffman variant 1. Begin every outer frame
with one NYT root and reset the complete model at the next frame. A non-empty
frame contains exactly one entropy block; it never crosses a frame boundary.
This makes frames independently decodable and gives model reset, corruption
containment, and bounded counter lifetime one shared boundary.

Number the initial root 512. Splitting NYT number `n` retains `n` for the new
internal node, assigns `n-1` to the new symbol as its right child, and `n-2` to
the new NYT as its left child. Left and right edges emit 0 and 1 respectively.
For an existing symbol, visit its leaf upward: swap with the highest-numbered
node of equal weight that is neither the node, its parent, nor an ancestor or
descendant, increment, then continue at its parent. For a new symbol, create
NYT weight 0, symbol weight 1, and internal weight 1, then continue updates at
the new internal node's former parent.

The format caps this variant's uncompressed frame at 2^24 bytes. Node weights
use 32-bit unsigned storage, so the mandatory frame-boundary full reset occurs
long before overflow. This baseline deliberately uses synchronized reset as its
rescaling policy and has no mid-frame halving or reconstruction rule. A future
continuous or differently rescaled model requires a distinct variant ID.

## DD-034: Adaptive payload final bits use a bounded descriptor

- Date: 2026-07-12
- Status: accepted

Place one fixed 16-byte Adaptive Huffman descriptor between every non-empty
frame header and payload. It repeats the symbol count and payload byte count,
records final valid bits, and reserves all other fields. This lets the decoder
reject contradictory sizes and calculate exact regions before tree traversal.

Do not reinterpret the Blocked Huffman descriptor even though both are 16
bytes. They are algorithm-specific structures selected only after the stream
algorithm and variant have been validated.

## DD-035: Adaptive Huffman reference encoding plans before mutation

- Date: 2026-07-12
- Status: accepted

For one finite nonempty frame, first replay FGK updates into a temporary bounded
tree while summing every path and new-symbol literal bit with checked
arithmetic. Validate the resulting payload size against format and local limits
and publish the planned descriptor only on success.

The encoder repeats the deterministic traversal only after output capacity has
been proven. It zeroes exactly the planned payload span, writes path bits in
root-to-leaf order and new literals numerically LSB-first, then publishes the
descriptor. Capacity failure leaves both output and caller descriptor unchanged.
This two-pass reference favors atomic behavior and testability; a later
streaming encoder must produce identical bytes.

## DD-036: Strict Adaptive decoding validates twice and publishes once

- Date: 2026-07-12
- Status: accepted

Validate descriptor fields, exact payload span, output capacity, combined frame
limits, expansion policy, and zero high padding before tree traversal. Then
decode the declared symbol count into no output, rejecting path/literal
truncation, duplicate NYT literals, invalid tree transitions, and any mismatch
between consumed and declared valid bits.

Only after the complete validation pass succeeds, reset and repeat the same
bounded traversal into caller output. A failure in the validation pass leaves
the entire output span unchanged. This frame-local two-pass policy matches the
reference encoder's clarity and provides a strong oracle for a later streaming
decoder whose commit boundary will remain a complete validated frame.

## DD-037: Adaptive frame composition is exact-span and algorithm-specific

- Date: 2026-07-12
- Status: accepted

Extend generic frame validation with an explicit Adaptive Huffman variant 1
case requiring block count one and descriptor bytes 16. Count the descriptor
and compressed payload together toward buffered-memory limits, while leaving
stream entropy block size zero because the outer frame itself is the model
reset boundary.

Plan and validate the entropy body before constructing the generic header.
Encoding checks total `header + descriptor + payload` capacity first. Strict
decoding accepts exactly one serialized frame span, parses the header and
descriptor in sequence, then delegates payload validation and output atomicity
to the strict reference decoder. Algorithm-specific descriptors remain typed
and cannot be cross-parsed merely because their fixed sizes match.

## DD-038: Adaptive stream reference validates every frame before output

- Date: 2026-07-12
- Status: accepted

Compose the known-size stream as one fixed stream header followed by the exact
original-size-determined frame sequence. Empty input is header-only. Planning
visits every finite frame and proves total capacity before encoding mutates the
stream output.

Provide explicit validation-only entry points at entropy-frame and serialized-
frame levels. The strict stream decoder scans and semantically validates every
frame with no output, rejects truncation or trailing bytes, and only then
repeats decoding into caller storage. Do not treat output-too-small as evidence
of semantic validation. This reference preserves whole-stream atomicity and
will serve as the oracle for a later frame-committing streaming transform.

## DD-039: Adaptive streaming encoding commits complete outer frames

- Date: 2026-07-12
- Status: accepted

Emit the stream header first, then retain at most one configured raw frame and
one serialized frame in caller-owned workspaces. Encode and expose a normal
full frame before end input; the final short frame is complete only when its
known original-size boundary is reached. Drain pending output before consuming
later input, so callers can re-present an unconsumed suffix after `NeedOutput`.

Non-terminal flush leaves a partial frame open and does not change bytes.
Explicit reset is unsupported. Map encoded-workspace exhaustion to out of
memory, format/local planning limits to limit exceeded, boundary misuse to
invalid argument, and impossible post-plan failures to internal error. Every
input/output chunking must match the complete stream reference byte-for-byte.

## DD-040: Adaptive streaming decoding commits validated frames

- Date: 2026-07-12
- Status: accepted

Incrementally collect the fixed stream header, fixed frame header, and exact
declared frame remainder into caller-owned storage. Validate workspace capacity
immediately after the frame header. Strictly decode the complete frame into a
separate decoded workspace, then drain it before accepting later frame bytes.

This makes one validated outer frame the streaming commit boundary. Pending
decoded output leaves later input unconsumed, and callers re-present that suffix
with applicable flags. Truncation, trailing final bytes, malformed descriptors,
and payload errors are terminal, but cannot retract previously drained frames.
Empty streams remain header-only and repeated calls after completion return
end-of-stream.

## DD-041: Adaptive workspace queries guarantee worst-case input

- Date: 2026-07-12
- Status: accepted

Normalize known-size encoder settings into Adaptive Huffman variant 1 before
construction. Size raw input storage from the smaller of original size and
configured frame size. Without inspecting future input, bound every symbol by
the maximum 256-bit tree path plus an 8-bit NYT literal, round the total upward,
and add the fixed descriptor and frame header. Reject a profile whose guaranteed
worst case exceeds compressed-payload or buffered-memory policy.

For decoding, report one frame header plus maximum locally buffered frame body
and one decoded frame capped by both local and variant frame limits. No block
view workspace is needed. Empty streams require no frame storage. Profile
errors map to the same stable invalid-argument, unsupported, and limit-exceeded
categories used by the existing C boundary.

## DD-042: Adaptive Huffman uses a separate configuration in ABI version 1

- Date: 2026-07-12
- Status: accepted

Preserve the existing Blocked Huffman configuration layout and ABI version 1.
Expose Adaptive Huffman through its own size-tagged configuration, initializer,
workspace query, and create function. Both factories return the same opaque
transform and therefore share process and destroy operations without exposing
C++ implementation types.

Adaptive transforms use primary and secondary byte workspaces but no block-view
workspace. Normalize the otherwise irrelevant decoder block-size limit to a
bounded internal value before common limit validation. Verify the shared-library
boundary with a pure-C Adaptive round-trip test, including reserved-field
rejection.

## DD-043: Dynamic Range Coder variant 1 uses delayed byte carry

- Date: 2026-07-12
- Status: accepted

Define variant 1 as a frame-reset adaptive order-0 byte-symbol coder with a
32-bit range, 64-bit low accumulator, base-256 normalization below 2^24, and an
explicit cached-byte carry procedure. Terminate every nonempty frame with five
`shift_low` operations and use the descriptor's symbol count instead of an end
symbol. This makes payload extent, decoder initialization, and trailing-byte
rejection exact.

Initialize all 256 frequencies to one. Increment after each symbol and halve
with upward rounding when total reaches 32768. This keeps every symbol active,
bounds all arithmetic, and gives encoder and decoder one deterministic update
point. Reset the coder and model at every outer frame; a different model,
normalization threshold, carry rule, or reset policy requires another variant.

## DD-044: Dynamic Range frames use one typed descriptor and canonical prefix

- Date: 2026-07-13
- Status: accepted

Represent every nonempty Dynamic Range frame as the generic 56-byte header,
exactly one 16-byte range descriptor, and one byte-aligned payload. Set entropy
block count to one even though stream entropy block size is zero, because the
outer frame is the single model-reset boundary. Generic frame validation checks
the descriptor size and required model-total bound before body traversal.

Require the first of the five decoder-initialization bytes to be zero. A 32-bit
code calculation eventually shifts that byte out, so accepting other values
would permit multiple payload representations for the same interval. Strict
decoding rejects the nonzero prefix to preserve canonical deterministic streams.

## DD-045: Dynamic Range reference streams validate before output

- Date: 2026-07-13
- Status: accepted

Compose known-size streams as one fixed stream header followed by deterministic
outer frames derived from original size and configured frame size. Empty input
is header-only. Plan every frame before encoding mutates output.

Strict reference decoding first scans and semantically validates every frame
without output, rejects truncation and trailing bytes, and only then repeats the
scan into caller storage. This gives the oracle whole-stream atomicity while
proving that each frame resets the coder and order-0 model independently.

## DD-046: Dynamic Range streaming encoding commits complete frames

- Date: 2026-07-13
- Status: accepted

Drain the stream header first, then retain at most one configured raw frame and
one serialized frame in caller-owned workspaces. Encode a frame only when its
known original-size boundary is complete, and drain pending output before
consuming later input. Arbitrary input and output chunking must reproduce the
complete reference stream byte for byte.

Non-terminal flush does not shorten a frame. Explicit reset is unsupported.
Preserve limit-exceeded, invalid-boundary, workspace-exhaustion, and internal
failure as distinct stable core error categories.

## DD-047: Dynamic Range streaming decoding commits validated frames

- Date: 2026-07-13
- Status: accepted

Incrementally collect fixed headers and exactly one declared frame in bounded
caller storage. Strictly decode that frame into a separate decoded workspace,
then drain it before accepting later frame bytes. Pending decoded output leaves
later input unconsumed so the caller can re-present the suffix.

This makes one validated outer frame the streaming commit boundary. Truncation,
noncanonical range state, malformed descriptors, unexpected trailing bytes, and
workspace failures are terminal for the transform but cannot retract earlier
drained frames or expose bytes from the malformed frame.

## DD-048: Dynamic Range workspace uses a two-byte-per-symbol bound

- Date: 2026-07-13
- Status: accepted

Before every interval update, range is at least 2^24 and model total is at most
2^15. Division therefore leaves a unit of at least 2^9. Since every frequency
is nonzero, at most two base-256 normalizations restore range to at least 2^24.
Together with five termination shifts, `2 * frame_symbols + 5` is a conservative
input-independent payload bound for encoder workspace.

Size the raw workspace from the largest frame that can actually occur. Size
decoder workspaces solely from local frame and buffered-byte limits because no
stream field is trusted before construction. Reject a policy whose maximum
range-model total is below the variant-required 32768.

## DD-049: Dynamic Range extends ABI version 1 with a separate config

- Date: 2026-07-13
- Status: accepted

Preserve ABI version 1 and all existing Blocked and Adaptive configuration
layouts. Add a separate size-tagged Dynamic Range configuration, initializer,
workspace query, and factory. Carry maximum range-model total explicitly because
it is a required decoder policy rather than an irrelevant shared-core field.

Return the common opaque transform and reuse the common process and destroy
operations. Dynamic Range uses primary and secondary byte workspaces and no
views workspace. Verify the shared-library boundary with a pure-C round trip,
reserved-field rejection, and insufficient model-policy rejection.

## DD-050: rANS variant 1 is scalar and byte-renormalized

- Date: 2026-07-13
- Status: accepted

Fix variant 1 to one unsigned 64-bit state, `table_log=12`, normalized total
4096, lower bound 2^31, and byte-wise renormalization. Encode symbols in reverse
logical order and serialize the final state little-endian before renormalization
bytes arranged in decoder-consumption order. Require exact terminal state and
payload consumption during strict decoding.

Normalize each finite block with exact integer error correction and explicit
numeric-symbol tie breaks. Serialize all 256 normalized uint16 frequencies in a
fixed 528-byte descriptor. Blocks occur in logical order, reset independently,
never cross outer frames, and use the stream entropy block size in byte-symbol
units. A differing state count, table log, normalization rule, descriptor, or
byte layout requires another variant.

## DD-051: rANS frames validate all blocks before output

- Date: 2026-07-13
- Status: accepted

Serialize one generic frame header, all fixed-size block descriptors in logical
order, then all corresponding payloads in logical order. Plan every block and
the complete frame extent before encoder mutation. Descriptor count is derived
from dictionary byte size and stream entropy block size.

Strict frame decoding first validates the complete descriptor region and every
block payload without output. Only then decode blocks into caller output. This
makes the outer frame the commit boundary even though individual rANS blocks are
independently coded and validated.

## DD-052: rANS reference streams validate before output

- Date: 2026-07-13
- Status: accepted

Compose known-size streams as the fixed stream header followed by deterministic
outer frames. Plan every frame before encoding mutates output; empty input is
header-only. Reuse one caller-owned block-view workspace across frames.

Strict decoding first scans and semantically validates every exact frame extent
without output, then repeats the traversal into caller storage. Reject truncation
and trailing bytes. This reference provides whole-stream atomicity and serves as
the oracle for the later frame-committing streaming transform.

## DD-053: rANS streaming encoding commits complete outer frames

- Date: 2026-07-13
- Status: accepted

Buffer at most one raw outer frame in caller-owned storage, encode it into a
second caller-owned workspace, and drain the complete serialized frame before
accepting later input. Non-terminal flush keeps a partial frame open and
explicit reset is unsupported, so arbitrary chunking reproduces the complete
reference stream byte for byte.

For variant 1, the pre-update state is below `L * 256` and the minimum
renormalization threshold is `L / 16`; one byte emission therefore always
restores the threshold. Each input symbol contributes at most one payload byte,
and each block contributes its eight-byte initial state. Encoder workspace is
bounded by the frame header, `528 * block_count` descriptor bytes,
`frame_symbols + 8 * block_count` payload bytes, and the separate raw frame.

## DD-054: rANS streaming decoding commits validated outer frames

- Date: 2026-07-13
- Status: accepted

Collect one exact serialized frame in bounded caller storage and use a separate
decoded workspace plus caller-owned block views. Parse the generic header,
validate the complete descriptor region, validate every rANS payload, and only
then decode the frame. Drain that decoded frame before accepting bytes belonging
to the next frame.

Malformed later frames cannot retract earlier committed output and cannot expose
any bytes from the failing frame. Decoder workspace sizes are derived only from
local limits because stream fields remain untrusted until parsed.

## DD-055: rANS extends ABI version 1 with a separate config

- Date: 2026-07-13
- Status: accepted

Preserve ABI version 1 and every existing configuration layout. Add a separate
size-tagged rANS configuration with explicit frame size, entropy block size,
and relevant local limits. Reuse the common opaque transform, process result,
and destroy function.

Encoding uses primary raw-frame and secondary serialized-frame byte workspaces.
Decoding uses primary serialized-frame, secondary decoded-frame, and aligned
block-view workspaces. Validate reserved fields, workspace extents, and view
alignment before allocating the C++ implementation object.

## DD-056: tANS variant 1 uses a deterministic 4096-state automaton

- Date: 2026-07-13
- Status: accepted

Fix variant 1 to `table_log=12`, `L=4096`, state interval `[L,2L)`, and the
same exact normalized frequencies as rANS variant 1. Spread normalized symbol
occurrences by walking the table with step 2563 from position zero, processing
symbols in numeric order. The odd step permutes every table position exactly
once and makes table reconstruction independent of host or library behavior.

Serialize the final encoder-state offset as little-endian uint16 followed by
the decoder-consumption-order bit sequence packed LSB-first. Encode source
symbols in reverse and prepend each emitted low-bit chunk logically. Require
exact bit consumption, zero high padding, and terminal state `L`. This is a
repository-defined tANS representation and is not claimed to be FSE-compatible.

## DD-057: tANS frames validate every block before output

- Date: 2026-07-13
- Status: accepted

Use the generic descriptors-first, payloads-second outer-frame composition,
while retaining the distinct tANS descriptor and payload semantics. Plan every
block and the complete frame extent before encoder output mutation.

Strict frame decoding validates the descriptor region and every complete tANS
payload before decoding any block into caller output. One frame is therefore
the atomic commit boundary. With frame size 3 and block size 2, `ABA` is a
canonical 1117-byte frame: 56 header bytes, 1056 descriptor bytes, and five
payload bytes.

## DD-058: tANS reference streams validate before output

- Date: 2026-07-13
- Status: accepted

Compose known-size tANS streams as the fixed stream header followed by
deterministic outer frames. Plan every frame before encoder mutation; empty
input is header-only. Reuse one caller-owned block-view workspace across frames.

Strict reference decoding scans and semantically validates every exact frame
extent without output, then repeats the traversal into caller storage. Reject
truncation and trailing bytes. Two independent `AA` frames each occupy 586
bytes, making the canonical four-byte `AAAA` reset stream 1236 bytes.

## DD-059: tANS streaming encoding commits complete outer frames

- Date: 2026-07-13
- Status: accepted

Buffer one raw outer frame in caller-owned storage, encode it into a second
caller-owned workspace, and drain the complete serialized frame before accepting
later input. Non-terminal flush keeps a partial frame open; explicit reset is
unsupported. Arbitrary chunking therefore reproduces the reference stream.

Each symbol emits at most `table_log=12` bits and every block adds its two-byte
state offset. Calculate encoder workspace per block as
`2 + ceil(12 * block_symbols / 8)`, then add every 528-byte descriptor and the
generic frame header with checked arithmetic. Decoder workspace remains derived
only from local limits and includes caller-owned block views.

## DD-060: tANS streaming decoding commits validated outer frames

- Date: 2026-07-13
- Status: accepted

Collect one exact serialized frame in bounded caller storage and decode it into
a separate workspace using caller-owned block views. Validate the complete
descriptor region and every tANS payload before publishing any byte from that
frame, then drain it before accepting bytes for the next frame.

Malformed later frames cannot retract earlier committed output and cannot expose
bytes from the failing frame. Truncation, trailing bytes, insufficient encoded,
decoded, or view workspace, invalid states, bit extents, and padding are terminal
transform errors with stable categories.

## DD-061: tANS extends ABI version 1 with a separate config

- Date: 2026-07-13
- Status: accepted

Preserve ABI version 1 and all existing configuration layouts. Add a separate
size-tagged tANS configuration with explicit frame size, entropy block size, and
relevant local limits. Reuse the common opaque transform, process result, and
destroy operation.

Encoding uses primary raw-frame and secondary serialized-frame workspaces.
Decoding additionally uses an aligned caller-owned tANS block-view workspace.
Validate tags, reserved fields, extents, and alignment before constructing the
C++ transform. Verify both directions through the shared library from pure C.

## DD-062: LZ77 variant 1 uses fixed canonical copy tokens

- Date: 2026-07-13
- Status: accepted

Use a frame-local sliding window with default size 65,536 bytes, minimum match
length 3, and maximum match length 258. Select the longest match; on equal
length select the nearest distance. Matching may extend through overlap using
the same bytewise semantics as decoding. Dictionary history resets at every
outer frame.

Serialize every token as a fixed 16-byte record with explicit Literal,
MatchThenLiteral, and TerminalMatch tags. The terminal form represents a final
existing match without inventing a following byte. Fixed records prioritize
strict validation and canonical byte-stream integration over compression ratio
in the reference variant. A future compact token representation requires a new
variant ID.

## DD-063: LZ77 streaming decoding retains a caller-owned history ring

- Date: 2026-07-13
- Status: accepted

Accumulate one fixed 16-byte token, validate it against the current frame-local
output position, then drain its match and optional literal directly to caller
output. Retain partial token, match-copy, literal, and terminal state across
calls. Decoded bytes are committed token by token; a malformed later token does
not retract earlier output.

Because caller output buffers may change on every call, retain dictionary
history in caller-owned circular storage sized to
`min(window_size, declared_frame_size)`. This supports overlap copy with one-byte
output capacity without allocating or buffering the decoded frame. `EndInput`
is retained after all supplied input is consumed while output is still pending.
Non-terminal flush is a no-op and explicit reset remains unsupported at this
frame-local transform boundary.

## DD-064: LZ77 streaming encoding buffers one known-size frame

- Date: 2026-07-13
- Status: accepted

Collect exactly the declared raw frame size in caller-owned storage. Once full,
run the deterministic reference planner and encoder into separate caller-owned
serialized storage, then drain those bytes before accepting any later input.
This preserves identical output across input and output chunking while keeping
all memory bounded and allocation-free inside the transform. The exact raw plus
serialized working extents must also fit the aggregate internal-buffer limit.

A full frame may be encoded before `EndInput` because its declared end is known;
the transform then waits for an explicit terminal signal after draining. A
non-terminal flush leaves a partial frame open. Premature `EndInput`, bytes past
the declared frame, insufficient workspace, and unsupported reset are terminal
errors.

## DD-065: The first complete LZ77 frame path uses entropy None

- Date: 2026-07-13
- Status: accepted

Bind LZ77 variant 1 to the generic outer frame with entropy algorithm `None`.
The stream-level dictionary parameter region is exactly 16 bytes. Each frame's
dictionary serialized size and compressed payload size both equal the complete
canonical LZ77 token extent; entropy block count and descriptor size are zero.

Plan the token stream before writing the frame. Strict decode validates the
generic header, exact frame extent, and every token before raw output mutation,
making one complete frame the atomic rejection boundary. Later entropy
combinations reuse the same canonical dictionary bytes without altering this
pipeline's representation.

## DD-066: Known-size LZ77 streams reset at deterministic frames

- Date: 2026-07-13
- Status: accepted

Serialize the 64-byte stream prefix, the single 16-byte LZ77 parameter region,
then zero or more complete LZ77-plus-None frames. Empty input is therefore an
80-byte stream. Partition nonempty input by the declared uncompressed frame
size; reset dictionary history and token parsing at every frame.

Strict reference decoding parses parameters transactionally, scans and
validates all exact frame extents without output, rejects truncation and trailing
bytes, then repeats the traversal into caller storage. A malformed later frame
cannot expose output from any earlier frame in this one-shot API.

## DD-067: LZ77 outer streaming encoding commits complete frames

- Date: 2026-07-13
- Status: accepted

Emit the fixed stream prefix and LZ77 parameter region first. Buffer one raw
outer frame in caller-owned storage, encode it into a separate reusable frame
workspace, then drain the complete generic frame before accepting later raw
input. Pending output has priority and may leave an input suffix unconsumed.

Known original and frame sizes allow every full frame to be committed before
whole-stream termination. Non-terminal flush keeps a partial frame open; final
short-frame completion requires `EndInput`. The output is byte-identical to the
known-size reference stream for every input and output chunking.

## DD-068: LZ77 outer streaming decoding commits validated frames

- Date: 2026-07-13
- Status: accepted

Collect the fixed prefix transactionally, then collect each generic frame header
and its exact declared token payload in bounded caller-owned storage. Validate
and decode the complete frame into separate caller-owned raw storage before
publishing any byte from that frame. Drain committed raw bytes before consuming
the next frame.

Malformed later frames cannot retract earlier output and expose no bytes from
the failing frame. Truncation, trailing bytes, invalid parameters or tokens,
insufficient encoded or decoded workspace, aggregate memory limits, and
unsupported reset become stable terminal transform errors.

## DD-069: LZ77 profiles expose bounded workspace requirements

- Date: 2026-07-13
- Status: accepted

For encoding, derive the largest raw frame from the smaller of original size
and configured frame size. Its conservative serialized frame bound is the
56-byte generic header plus one 16-byte Literal token per raw byte. Require the
raw and serialized extents together to fit the internal-buffer limit.

For decoding, configuration is not trusted or available before construction.
Derive the encoded workspace only from local dictionary, payload, and internal
limits after reserving the generic header and at least one raw byte, and derive
decoded workspace from the local maximum frame size. Actual frame collection
still enforces the combined encoded-plus-decoded extent.

## DD-070: LZ77 extends ABI version 1 with a separate config

- Date: 2026-07-13
- Status: accepted

Preserve ABI version 1 and every existing configuration layout. Add a distinct
size-tagged LZ77 configuration containing frame and match parameters plus the
relevant local hard limits. Reuse the common opaque transform, process result,
workspace descriptor, and destruction operation; LZ77 requires no views
workspace.

The encoder factory normalizes a known-size profile before constructing the
outer streaming encoder. The decoder factory derives workspace only from local
limits and learns stream parameters from the encoded prefix. Validate sizes,
ABI tags, reserved fields, buffers, and limits before object construction.

## DD-071: The first CLI dogfoods the public LZ77 C ABI

- Date: 2026-07-13
- Status: accepted

Build a portable `marc encode|decode <input> <output>` executable from the
public header and link it as an ordinary library consumer. The initial tool
selects the version 1 LZ77-plus-None profile with 1 MiB raw frames and uses
fixed 64 KiB I/O chunks; codec workspaces remain bounded and caller-owned.

Require a known-size regular input and a nonexistent destination. Write to a
sibling `.tmp` path, delete that path on every transform or I/O failure, and
rename it only after successful stream completion and close. This prevents a
malformed stream from exposing partial decoded output and avoids silently
overwriting an existing file. Archive metadata and unknown-size sources remain
outside this first tool's scope.

## DD-072: LZSS variant 1 uses locally costed byte tokens

- Date: 2026-07-13
- Status: accepted

Reuse LZ77's frame-local 65,536-byte default window, overlapping copy semantics,
longest-match parsing, nearest-distance tie break, and maximum match length 258.
Use a default and minimum permitted match length of 5 because of the exact token
cost below. Reset history at every outer frame.

Serialize Literal as a one-byte tag plus its byte, for a total cost of 2 bytes.
Serialize Match as a one-byte tag plus little-endian 32-bit distance and length,
for a total cost of 9 bytes. Match is eligible only when `9 < 2 * length`, so
length 5 is the first strictly beneficial substitution. Independent Literal
tokens keep this comparison local and avoid run-boundary lookahead or dynamic
programming in the reference encoder.

A Match may end the frame, eliminating LZ77's combined following-literal and
terminal forms. Variable-size explicit tokens retain byte-stream composition,
simple bounded parsing, and a 2-to-1 worst-case serialization expansion. A
different packing or literal-run representation requires another variant ID.

## DD-073: LZSS streaming decode accumulates one variable token

- Date: 2026-07-14
- Status: accepted

Read the one-byte tag first, then accumulate exactly 2 bytes for Literal or
9 bytes for Match in a fixed local buffer. Validate the completed token against
the committed frame position before publishing any byte from that token.

Drain a Literal or bytewise overlapping Match directly to caller output while
mirroring each committed byte into a caller-owned circular history region of
`min(window_size, frame_size)` bytes. Preserve partial Match progress and a
consumed `EndInput` request across output starvation. This keeps steady-state
decode allocation-free and permits every input and output boundary.

## DD-074: LZSS streaming encode buffers one known-size frame

- Date: 2026-07-14
- Status: accepted

Collect exactly the declared raw frame in caller-owned storage because greedy
longest-match selection depends on later input and the exact frame end. A
non-terminal Flush does not shorten that frame. Reject premature EndInput and
raw bytes beyond the declared size.

After collection, run the reference planning pass, enforce the combined raw and
serialized workspace limit, encode into a separate caller-owned region, and
drain without consuming later input. Retain a consumed EndInput request while
output is blocked. This makes output byte-identical to the reference encoder
for every chunking and bounds worst-case token storage at twice the raw size.

## DD-075: The first complete LZSS frame path uses entropy None

- Date: 2026-07-14
- Status: accepted

Bind LZSS variant 1 to the generic outer frame with entropy algorithm `None`.
The frame's dictionary serialized size and compressed payload size are the same
exact variable-token extent; entropy block count and descriptor size are zero.

Plan the whole frame before writing its header or body. Strict validation and
reference decoding traverse the variable token sequence to obtain token count;
the count is never inferred by dividing the payload size. Decode validates the
complete token payload and output capacity before publishing frame output.

## DD-076: Known-size LZSS reference streams validate before decode

- Date: 2026-07-14
- Status: accepted

Serialize the stream header and 16-byte LZSS parameter record once, followed by
frame-size-bounded LZSS/None frames with sequence numbers starting at zero. An
empty stream contains only the 80-byte prefix. Each non-empty frame resets its
dictionary history and emits the same canonical bytes independently.

Strict reference decode first scans and validates every frame, including exact
stream extent and cumulative raw size, then performs a second scan that commits
output. A malformed later frame therefore cannot expose an earlier frame's raw
bytes through this one-shot API.

## DD-077: LZSS streaming decode commits complete validated frames

- Date: 2026-07-14
- Status: accepted

Collect one complete serialized LZSS frame in caller-owned storage, decode it
atomically into a separate caller-owned raw-frame workspace, and only then drain
raw bytes. A valid earlier frame may be committed before a later frame arrives;
no byte from a malformed current frame is published.

Retain a consumed EndInput request while draining. Reject ResetBlock because
boundaries are carried by the canonical frame headers. Require the sum of the
encoded and decoded frame workspaces to fit the configured internal-buffer
limit before accepting a frame body.

## DD-078: LZSS streaming encode preserves reference frame boundaries

- Date: 2026-07-14
- Status: accepted

Buffer one complete known-size raw frame in caller-owned storage, plan and
encode it into a separate caller-owned serialized-frame workspace, then drain
the canonical bytes before consuming later frame input. Emit the stream prefix
first and retain a consumed EndInput request until the final frame is drained.

Flush only exposes already representable bytes; it does not shorten a partial
frame. Reject ResetBlock because fixed frame boundaries come from the stream
header. Require raw plus serialized frame storage to fit the configured
internal-buffer limit. Output must match the one-shot reference stream for all
input and output chunking.

## DD-079: LZSS profiles expose bounded workspace requirements

- Date: 2026-07-14
- Status: accepted

Normalize a known-size LZSS/None configuration into the canonical stream header
and report caller-owned workspace before constructing a transform. Size encoder
raw storage from the largest frame that can occur. Since every unmatched byte
is a two-byte Literal and a Match is selected only when strictly cheaper, the
exact input-independent payload upper bound is twice the raw frame size. Add the
generic frame header and enforce dictionary, compressed-payload, and aggregate
buffer limits with checked arithmetic.

Derive decoder encoded workspace solely from local dictionary, payload, and
aggregate limits, reserving one byte for decoded output in the aggregate bound;
derive decoded workspace from the local maximum frame size. Map overflow and
limit failures to the stable core limit-exceeded category.

## DD-080: LZSS uses a separate size-tagged C transform API

- Date: 2026-07-14
- Status: accepted

Add a versioned `marc_lzss_config` with explicit format parameters and local
decoder limits, plus initializer, workspace query, and transform factory.
Retain ABI version 1 because no existing layout or symbol changes. Both
directions use the common opaque transform and process/destroy operations.

Encoding uses primary raw-frame and secondary serialized-frame workspaces.
Decoding uses primary serialized-frame and secondary decoded-frame workspaces.
LZSS needs no views workspace. Reject incorrect size/version tags, nonzero
reserved fields, invalid parameters or limits, and insufficient caller buffers
before construction.

## DD-081: CLI codec selection is explicit and defaults to LZ77

- Date: 2026-07-14
- Status: accepted

Retain `marc encode|decode <input> <output>` as the LZ77-compatible default and
add `--codec lz77|lzss` before the paths. Require the same explicit selection
for LZSS decode because the CLI deliberately uses only public algorithm-specific
C factories and does not parse headers through private C++ APIs.

Derive each codec's workspace limits from its canonical worst-case payload: 16
bytes per raw byte for LZ77 and two for LZSS. Preserve staged output commit,
bounded 64 KiB I/O, malformed-input cleanup, overwrite refusal, and empty-file
round trips for both profiles.

## DD-082: Dictionary benchmarks use the public C transform path

- Date: 2026-07-14
- Status: accepted

Provide an opt-in, dependency-free C++20 benchmark executable for LZ77 and LZSS
that reads a caller-selected corpus and verifies a canonical round trip before
timing. Time only the single complete `marc_transform_process()` call; exclude
file I/O, allocation, workspace query, factory construction/destruction, and
verification. Recreate the terminal transform for every iteration.

Report complete-stream encoded/input ratio, raw-byte encode and decode MiB/s,
direction-specific primary and secondary workspace, and the larger combined
codec workspace. Do not label this last value process peak memory. Require
Release builds and recorded environment/corpus metadata for published results.

## DD-083: LZSS fuzzing covers strict and streaming decode together

- Date: 2026-07-14
- Status: accepted

Feed each arbitrary byte sequence to both the one-shot known-size LZSS decoder
and the outer frame-streaming decoder. Derive streaming input/output chunks from
the bytes, enforce small fixed local limits and workspaces, validate every
ProcessResult, and abort on an exceeded call guard or invalid no-progress state.

Build the full static library and harness with Clang libFuzzer, ASan, and UBSan
only when explicitly requested. Keep normal MSVC builds independent of sanitizer
flags while compiling the harness as a test-build object smoke check. Every fuzz
finding requires a minimized permanent GoogleTest regression and retained corpus
input with provenance.

## DD-084: LZ78 variant 1 uses fixed phrase-index tokens

- Date: 2026-07-14
- Status: accepted

Use frame-local LZ78 phrases numbered consecutively from 1, with index 0 as an
unstored empty root. Serialize every index as a fixed little-endian `uint32` in
an eight-byte Pair or FinalIndex token; index width does not grow. FinalIndex
resolves a frame ending in an already-known phrase without inventing a following
byte. The declared raw frame size remains the primary termination rule.

Bound non-root entries by an explicit stream parameter and local decoder limit.
When that capacity is reached, freeze the dictionary until the next outer frame
rather than adding a clear token or changing the representation. Store prefix,
trailing byte, and checked expanded length, and require non-recursive bounded
expansion. With entropy None, the canonical worst-case payload bound is eight
serialized bytes per raw byte.

## DD-085: LZ78 validation uses a caller-owned prefix table

- Date: 2026-07-14
- Status: accepted

Parse parameters and fixed eight-byte tokens transactionally. Validate a
complete token stream with a caller-owned table containing only prefix index,
trailing byte, and checked expanded length for each retained phrase. Index the
root implicitly, reject forward references before table access, and never use
input-controlled recursion.

Require table capacity for `min(token_count, maximum_entries)` phrases. Once
that many configured entries have been retained, continue validating against
the frozen table without growing it. Report stable token index, byte offset,
committed output length, dictionary-entry count, and format error at the first
failure.

## DD-086: LZ78 reference decoding validates before publication

- Date: 2026-07-14
- Status: accepted

Validate the complete frame token stream and build its caller-owned phrase
table before checking output capacity or publishing any decoded byte. Invalid
input and short output therefore leave the output span untouched; phrase
workspace remains scratch and may be modified by validation.

Expand a phrase iteratively by reserving its already-validated output extent,
following prefix indices toward the implicit root, and writing trailing symbols
backward into that extent. Append the Pair symbol afterward. This produces
forward phrase order without recursion or a phrase-sized temporary buffer and
retains exact behavior after dictionary freeze.

## DD-087: LZ78 reference encoding keeps input-backed phrases

- Date: 2026-07-14
- Status: accepted

Keep the clear reference encoder independent of a trie or hash-chain
optimization. Represent each retained phrase in caller-owned workspace by the
offset and length of its first occurrence in the immutable frame input. Find
the longest phrase by comparing these bounded input spans in ascending phrase
index order; the first equal-length phrase therefore remains selected.

Query worst-case workspace as `min(input_size, maximum_entries)` records and
enforce its byte extent against the local buffered-memory limit before parsing.
Run the same deterministic parse for exact planning and serialization. Complete
planning, policy checks, workspace checks, and output-capacity checks before
writing any token so expected failures leave output untouched.

## DD-088: LZ78 streaming decode retains partial tokens and phrases

- Date: 2026-07-14
- Status: accepted

Collect one fixed eight-byte token across arbitrary input splits, validate its
reference and complete expanded extent, then drain its phrase across arbitrary
output splits. Retain dictionary entries in caller-owned prefix/symbol/length
workspace sized for `min(frame_size, maximum_entries)` phrases. Enforce the
workspace byte extent before accepting input.

Avoid a second phrase-sized staging buffer in the reference decoder. For each
forward output position, iteratively follow the bounded prefix chain until its
stored length identifies that byte. This deliberately simple strategy may be
replaced by a tested optimization later. Preserve terminal input while draining,
accept EndInput with zero final bytes, reject ResetBlock, and return EndOfStream
only after the exact declared frame output has drained and no trailing token
bytes remain.

## DD-089: LZ78 streaming encode buffers one known-size frame

- Date: 2026-07-14
- Status: accepted

Collect exactly the declared raw frame in caller-owned storage, then invoke the
reference LZ78 planner and encoder with a separate caller-owned phrase table.
Drain the resulting canonical token bytes from caller-owned encoded storage.
This deliberately buffered baseline makes encoded bytes independent of input
and output chunking while preserving the exact reference parse.

Require raw storage for the complete frame and dictionary records for
`min(frame_size, maximum_entries)` phrases before accepting input. After exact
planning, require encoded storage for the actual token extent and enforce the
sum of raw, dictionary, and encoded workspace bytes against the local aggregate
buffer limit. Flush does not close a partial frame; ResetBlock remains
unsupported at this layer; terminal input is retained while encoded bytes drain.

## DD-090: The first complete LZ78 frame path uses entropy None

- Date: 2026-07-14
- Status: accepted

Compose LZ78 variant 1 directly with the generic frame header and entropy None.
Set dictionary serialized size and compressed payload size to the exact
eight-byte token extent; entropy block count, descriptor size, and checksum
trailer size remain zero. Retain separate caller-owned encoder and decoder
phrase-table types because their reference representations serve different
bounded operations and are never serialized.

Plan the complete frame before writing its header. During decoding, validate the
generic header and exact payload extent, then let the atomic LZ78 decoder
validate the entire token stream before publishing raw bytes. The canonical
single-byte `A` frame is exactly 64 bytes: a 56-byte frame header followed by
one eight-byte Pair token.

## DD-091: Known-size LZ78 streams validate every frame first

- Date: 2026-07-14
- Status: accepted

Serialize the fixed stream header, the 16-byte LZ78 parameter region, and the
deterministic sequence of complete LZ78/None frames. Reuse caller-owned encoder
or decoder phrase workspace at each frame; frame-local parsing starts entry
numbering from 1 and overwrites prior scratch, thereby enforcing dictionary
reset without serializing an extra reset marker.

Strict reference decode performs a complete validation scan before a second
decode scan. A malformed later frame therefore leaves the entire raw output and
caller-visible parsed stream/parameters untouched. Empty input is exactly the
80-byte header-and-parameter prefix. Two independent `AAA` frames produce equal
16-byte token payloads and a canonical 224-byte reset stream.

## DD-092: Streaming LZ78 decode commits complete validated frames

- Date: 2026-07-14
- Status: accepted

Collect the fixed stream prefix and then one complete LZ78/None frame at a time
in caller-owned storage. Validate and decode the complete token payload into a
caller-owned raw-frame buffer before publishing any byte from that frame. A
malformed later frame therefore preserves bytes from earlier committed frames
while publishing no bytes from the failing frame.

Require a separate caller-owned phrase table sized for the current payload's
bounded token count. Count the used encoded-frame extent, decoded-frame extent,
and required phrase-table bytes together against the aggregate internal buffer
limit before collecting the payload. Retain terminal input while a decoded
frame drains, reject ResetBlock, and require an explicit EndInput observation
before reporting EndOfStream.

## DD-093: Streaming LZ78 encode preserves reference frame bytes

- Date: 2026-07-14
- Status: accepted

Emit the fixed stream prefix, collect one exact known-size raw frame in
caller-owned storage, and invoke the complete reference LZ78/None frame planner
and encoder. Drain that completed frame from separate caller-owned encoded
storage. This keeps the stream byte-for-byte identical to one-shot encoding for
every input and output chunking pattern.

Require caller-owned encoder phrase entries for the largest possible frame at
construction. Before encoding each frame, count its raw bytes, planned complete
frame extent, and required phrase-entry bytes together against the aggregate
internal buffer limit. Flush does not close a partial frame, ResetBlock remains
unsupported at the outer controller, and a received EndInput remains effective
until every final frame byte has drained.

## DD-094: LZ78 profiles expose typed phrase workspace counts

- Date: 2026-07-14
- Status: accepted

Build the canonical LZ78 variant 1 plus entropy None stream header from an
original size, uncompressed-byte frame size, and LZ78 parameters. Encoder
requirements report raw-frame bytes, worst-case complete-frame bytes, and a
count of typed `Lz78EncoderEntry` records; the worst case emits one eight-byte
Pair token per raw byte and freezes the phrase count at `maximum_entries`.

Decoder requirements depend only on trusted local limits and report encoded-
frame bytes, decoded-frame bytes, and typed `Lz78PhraseEntry` records. Find the
largest collectable payload with a monotonic search over the coupled aggregate
bound: header, payload, at least one decoded byte, and the phrase records
implied by complete eight-byte tokens must fit simultaneously. Cap phrase
records by the local dictionary-entry limit and the format's 32-bit entry
space. Stream-supplied parameters never enlarge these local requirements.

## DD-095: The LZ78 C ABI uses the opaque aligned views workspace

- Date: 2026-07-14
- Status: accepted

Expose LZ78 variant 1 through the existing config, workspace-query, create,
process, and destroy lifecycle. Keep encoder and decoder phrase records private
by reporting only their direction-specific byte count and alignment through
`marc_workspace_requirements.views_*`. The create function validates size and
alignment before constructing a transform over caller-owned memory.

The C configuration carries the encoder's `maximum_entries` parameter and the
decoder's trusted `max_dictionary_entries` limit separately. Decoder workspace
calculation ignores the encoder parameter field and remains a function only of
local limits. Adding the new config type and entry points is additive within
ABI version 1 and does not alter existing structures or function signatures.

## DD-096: CLI and benchmarks consume LZ78 only through the C ABI

- Date: 2026-07-14
- Status: accepted

Add `lz78` to the explicit CLI codec selector and benchmark driver without
including private LZ78 headers. Configure a 1 MiB uncompressed frame, an
eight-byte-per-input-byte worst-case payload bound, and a conservative 64 MiB
aggregate local buffer policy. Query all concrete workspace sizes through the
public C ABI.

Allocate opaque views storage with `alignment - 1` spare bytes and derive an
explicitly aligned pointer from the reported requirement; do not rely on the
incidental alignment of a byte array or `vector<uint8_t>`. CLI output retains
the existing temporary-file commit semantics. Benchmarks include views bytes in
peak workspace reporting and verify a complete round trip before timing.

## DD-097: LZ78 fuzzing is bounded and paired with permanent regressions

- Date: 2026-07-14
- Status: accepted

Exercise the strict LZ78 stream decoder and outer frame-streaming decoder in one
libFuzzer entry point. Fix total output at 4 KiB, frame output at 1 KiB,
serialized payload at 4 KiB, and phrase workspace at 512 records. Derive input
and output chunk sizes from the candidate bytes and cap process calls at input
length plus a fixed output margin so a stalled state becomes a reproducible
failure rather than an unbounded run.

Normal MSVC builds compile the harness without invoking a fuzz runtime. Keep
canonical truncation, token-field corruption, extreme frame lengths, and
cross-frame phrase references as ordinary GoogleTest regressions with atomic
one-shot output expectations. Sanitizer fuzz execution remains an explicit,
separate Clang workflow with a bounded maximum input length.

## DD-098: LZW variant 1 uses frame termination and an explicit width schedule

- Date: 2026-07-14
- Status: accepted

Initialize codes `0..255` as the byte alphabet and allocate new strings from
code 256. Do not reserve clear or end codes: every outer frame resets the table,
and its declared raw and dictionary-serialized sizes provide exact termination.
Freeze the table at `2^maximum_code_width`, with a configurable 9..24-bit
maximum and a 16-bit default. Pack codes LSB-first and require zero final
padding.

Remove the conventional early-change/late-change ambiguity by specifying the
two operational views. After insertion, the encoder raises the width when its
incremented next-free code equals the current power-of-two boundary. Before
each code after the first, the decoder raises the width when its one-entry-
behind next-free code equals that boundary minus one. Accept `code ==
next_free` only as the bounded `KwKwK` expansion while insertion remains
possible; reject it after dictionary freeze.

Use caller-owned prefix, trailing-byte, first-byte, and checked-length records
for the reference decoder, without recursive phrase expansion. Treat the
declared raw frame size as the commit bound and reject a phrase crossing it,
premature code bits, trailing bytes, invalid forward codes, and nonzero padding
before publishing output in the strict reference path.

## DD-099: LZW validation scans packed codes into caller-owned phrase metadata

- Date: 2026-07-14
- Status: accepted

Validate the complete packed code region without producing raw bytes. Retain
one caller-owned record per possible non-literal code, bounded conservatively
by `floor(serialized_bytes * 8 / 9) - 1` and the configured code capacity. Each
record stores prefix code, trailing byte, first byte, and checked expanded
length; literals remain implicit.

Report stable code index, failing-code byte and bit offset, dictionary entry
count, and validated output extent. Track loaded bytes separately so partial
reads do not move the reported failure position. Resolve `KwKwK` from the previous
phrase metadata, insert only after the current phrase and output bound validate,
and never follow input-controlled recursion. After exact raw completion, check
the BitReader's buffered high bits for zero before rejecting unread trailing
bytes. Enforce the sum of serialized input and required phrase-record bytes
against the aggregate internal-buffer limit. Parameter parsing publishes only
a fully validated 16-byte value.

## DD-100: LZW reference decoding validates before output publication

- Date: 2026-07-15
- Status: accepted

Run the complete packed-code validator before checking output capacity or
writing a raw byte. On success, repeat the exact width schedule in a second
pass, use the validated caller-owned phrase records as the decode table, and
expand each non-literal phrase backward into its final output range. This needs
no phrase-sized staging allocation and preserves the natural forward byte order.

During the second pass, verify each record expected at the next-free code:
prefix equals the previous code, trailing byte equals the current phrase's first
byte, stored first byte equals the previous phrase's first byte, and stored
length is exactly previous length plus one. Prefix codes must decrease while
walking a phrase, bounding the iterative traversal and excluding cycles. Treat
any post-validation discrepancy as an internal error rather than reclassifying
the already accepted byte stream.

## DD-101: LZW reference encoding uses input-backed phrases and exact planning

- Date: 2026-07-15
- Status: accepted

Represent every non-literal encoder phrase as an offset and length into the
immutable input frame. At each position, begin with its literal byte and scan
the populated records in ascending code order for a strictly longer match.
Before dictionary freeze each phrase value is unique, so no additional tie rule
is observable. Insert the selected phrase plus its following byte when both
exist and capacity remains.

Query the conservative workspace bound as zero for empty input and otherwise
`min(input_size - 1, 2^maximum_code_width - 256)` records. Run the identical
parse once without output to obtain exact code, bit, byte, and entry counts;
enforce all parameters, limits, workspace, and output capacity before a second
pass writes through BitWriter. Raise encoder width only after insertion advances
the next-free code to the power-of-two boundary. Finish once to emit canonical
zero padding.

## DD-102: LZW streaming decode retains partial codes and drains phrases directly

- Date: 2026-07-15
- Status: accepted

Retain BitReader state, a partial numeric code, collected-bit count, current
width, next-free code, and previous phrase metadata across process calls. Raise
width only when beginning a later code at the documented decoder boundary.
After a complete code validates, insert its derived dictionary record before
draining; this makes the `code == next_free` phrase available through the same
prefix representation as every ordinary phrase.

Emit a phrase one forward byte at a time by following decreasing prefix codes
until stored lengths identify the requested position. Require caller workspace
for `min(frame_size - 1, code_capacity)` records, with zero records for an empty
frame. Preserve EndInput while output drains, accept a later zero-byte EndInput,
strictly align zero padding after exact raw completion, reject unread trailing
bytes, and keep Flush non-terminal. ResetBlock remains unsupported at this
single-frame layer.

## DD-103: LZW streaming encode buffers one known-size frame

- Date: 2026-07-15
- Status: accepted

Collect exactly the declared raw frame in caller-owned storage, then invoke the
reference LZW planner and encoder with a separate input-backed phrase table.
Write the exact canonical result into caller-owned encoded storage and drain it
through arbitrary output capacities. This makes streaming bytes identical to
the reference representation for every input and output chunking.

Before accepting input, require complete raw storage and
`min(frame_size - 1, code_capacity)` phrase records, with zero records for an
empty frame. Enforce raw plus phrase bytes against the aggregate buffered limit;
after planning, add the exact encoded extent to the same check before writing.
Flush does not close a partial frame, ResetBlock is unsupported at this layer,
and EndInput remains effective while encoded bytes drain.

## DD-104: LZW plus None frames reuse the generic atomic adapter contract

- Date: 2026-07-15
- Status: accepted

Represent each nonempty LZW variant 1 code stream as the complete body of one
generic frame when entropy is None. Set dictionary serialized size and
compressed payload size to the same exact padded code-byte count, and leave
entropy block and descriptor fields zero. The outer frame size and committed
output position determine the only accepted raw extent.

Provide independent plan, encode, validate, and decode entry points. Planning
uses the reference LZW parser before emitting a header; validation checks the
entire header and payload extent before decoding; reference decode preserves
its atomic publication guarantee. Reject trailing frame bytes, unsupported
pipelines, insufficient workspaces, and malformed packed codes with stable
layered errors. The dictionary is frame-local and is never shared across calls.

## DD-105: LZW one-shot streams validate every frame before publication

- Date: 2026-07-15
- Status: accepted

Serialize one generic stream header and one LZW parameter region, followed by
zero or more LZW plus None frames. Partition nonempty raw input at the declared
frame size, number frames from zero, and reset the implicit alphabet and phrase
dictionary for every frame. Empty input contains only the 80-byte prefix.

Planning requires the raw input size to equal the declared original size and
adds every exact frame extent with checked arithmetic. Decoding parses stream
configuration transactionally, scans and validates every frame and the exact
final stream extent, then performs a second scan to publish raw bytes. Thus a
malformed later frame cannot expose output from an earlier valid frame, and
the caller's stream and parameter outputs remain unchanged on all failures.

## DD-106: LZW outer streaming decode commits complete frames independently

- Date: 2026-07-15
- Status: accepted

Collect the 80-byte stream prefix and each complete serialized LZW plus None
frame across arbitrary input splits. Validate its header before accepting the
body, require caller-owned storage for the exact serialized and raw frame
extents plus the conservative LZW phrase metadata, and enforce their aggregate
bytes against the internal-buffer limit before collecting the body.

Decode one complete frame atomically into raw staging storage, then drain it
through arbitrary output capacities before accepting another frame. A later
malformed frame therefore cannot alter that frame's staging operation, but it
does not retract bytes already committed from earlier frames. Retain EndInput
while staged output drains, accept a later empty EndInput, reject trailing
bytes and ResetBlock, and return EndOfStream only after final output drains.

## DD-107: LZW outer streaming encode preserves one-shot stream bytes

- Date: 2026-07-15
- Status: accepted

Serialize the fixed stream prefix during construction and drain it before raw
frame processing. Collect exactly the next declared raw frame in caller-owned
storage, invoke the reference frame planner and encoder with a separate
input-backed phrase table, then drain the complete serialized frame before
accepting or preparing another frame. This preserves the one-shot stream's
framing, sequence numbers, resets, and exact bytes under arbitrary chunking.

Require raw storage and conservative phrase entries from the largest possible
frame before accepting input. After exact planning, enforce raw, serialized
frame, and phrase bytes together against the internal-buffer limit and require
serialized-frame capacity before encoding. Flush may drain the prefix or a
completed frame but does not shorten a partial frame. Retain EndInput while
output drains, accept a later empty EndInput, and reject premature EndInput,
trailing raw input, and ResetBlock.

## DD-108: LZW profiles derive bounded workspace from format maxima

- Date: 2026-07-15
- Status: accepted

For encoding, derive the largest raw frame from original size and configured
frame size. Reserve at most one code per raw byte, each at the configured
maximum width, so the payload bound is
`ceil(largest_frame * maximum_code_width / 8)`. Reserve phrase records for
`min(largest_frame - 1, 2^maximum_code_width - 256)`, with checked arithmetic
and zero records for empty input. Include the 56-byte frame header in encoded
storage and enforce raw, encoded, and phrase bytes as one aggregate.

For decoding, first select the largest LZW code width whose phrase capacity is
permitted by the local dictionary-entry limit. Derive the maximum possible
phrase count from serialized bytes at the minimum 9-bit code width, cap it by
that permitted capacity, and binary-search the largest payload consistent with
serialized, compressed, and aggregate-buffer limits. Return raw staging for
the local maximum frame size. If even 9-bit LZW is forbidden, report a limit
failure rather than creating a decoder that cannot accept any valid profile.

## DD-109: LZW C ABI exposes opaque aligned phrase workspace

- Date: 2026-07-15
- Status: accepted

Add `marc_lzw_config_init`, `marc_lzw_workspace_requirements`, and
`marc_lzw_create` alongside the existing transform process and destroy calls.
The config contains the known original size, frame size, encoder maximum code
width, and decoder hard limits using fixed-width C types. Reserved fields,
structure size, ABI version, direction, and all buffers are validated before
constructing a C++ object.

Report raw/serialized frame storage through primary and secondary bytes by
direction. Report encoder or decoder phrase storage only as opaque
`views_bytes` plus `views_alignment`; no private C++ type appears in the ABI.
The factory places the selected outer streaming transform behind the existing
opaque handle with `nothrow` allocation. Decoder stream parameters remain
authoritative format input and the config maximum code width is encode-only.

## DD-110: CLI and benchmarks consume LZW only through the C ABI

- Date: 2026-07-15
- Status: accepted

Add `lzw` to the explicit CLI codec selector and benchmark driver without
including private LZW headers. Use a 1 MiB raw frame, maximum code width 16,
two payload bytes per raw byte as the conservative encoder bound, a 65,280
entry local dictionary ceiling, and a 64 MiB aggregate workspace policy. Keep
LZ77 as the backward-compatible CLI default and require explicit matching
codec selection for decode.

Reuse the existing transactional temporary-file workflow and generic process
loop, so partial input/output and failures retain the same behavior. Benchmark
the canonical full stream through the C ABI, verify a round trip before timing,
and report compression ratio, directional throughput, direction-specific
workspace components, and their maximum without calling LZW internals.

## DD-111: LZW fuzzing uses fixed width, memory, output, and call bounds

- Date: 2026-07-15
- Status: accepted

Feed each arbitrary input independently to the strict one-shot LZW stream
decoder and the outer frame-streaming decoder. Fix total output at 4 KiB, raw
frame size at 1 KiB, serialized payload at 4 KiB, and phrase metadata at 768
records. This local dictionary limit admits maximum code widths 9 and 10 only,
covering the first width transition without allowing input-controlled
workspace growth.

Derive streaming input and output chunks from bounded input bytes, validate
every `ProcessResult`, and cap calls by input size plus a fixed output margin.
Compile the harness in ordinary MSVC test builds but execute coverage-guided
fuzzing only in the explicit Clang sanitizer build. Keep canonical truncation,
invalid first-code, padding, extreme header, and cross-frame reset mutations as
permanent GoogleTest regressions with one-shot atomicity assertions.

## DD-112: LZW completion distinguishes local readiness from release evidence

- Date: 2026-07-15
- Status: accepted

Treat LZW variant 1 plus entropy None as locally implementation-complete only
after a single completion matrix covers empty, one-byte, every-byte,
repetitive, patterned, deterministic pseudo-random, and frame-boundary data.
Require byte-identical one-shot encodes and byte-identical outer streaming
encodes with one-byte and unequal input/output chunks across multiple frames.

This local status does not imply release-complete portability evidence. A
release still requires the planned CI to build and test with a non-MSVC
toolchain and to run the bounded sanitizer fuzz target. Cross-toolchain stream
comparisons and any promoted fuzz discoveries become permanent regression
vectors. The current LZW plus None profile stores no hashes, so codec-specific
hash verification is not applicable; the generic HashTap contract remains
independently tested at arbitrary byte-stream boundaries.

## DD-113: LZD variant 1 serializes two dictionary references per phrase

- Date: 2026-07-15
- Status: accepted

Define LZD as Lempel-Ziv Double: select the longest existing byte or phrase at
the current position, then independently select the longest at the following
position, and add their concatenation as one new phrase. Scope the dictionary
to one outer frame and freeze it, without clearing, at the configured nonzero
entry maximum. Phrase references are assigned consecutively from 256 while
`0..255` name literal bytes.

Serialize every token as two little-endian `uint32` references. Reserve
`0xFFFFFFFF` solely for an absent right reference on the final token. This
terminal form replaces the literature's unique sentinel for an arbitrary byte
alphabet and leaves the declared frame raw size as the primary termination
rule. It also keeps all ordinary phrases binary grammar productions and makes
the decoder independent of encoder longest-match validation.

Require references to name only the implicit alphabet or earlier frame-local
phrases. Store phrase lengths with checked arithmetic and expand the acyclic
binary grammar with a bounded explicit stack rather than recursion. Fixed
eight-byte tokens give the checked worst-case bound
`8 * ceil(raw_frame_size / 2)`.

## DD-114: LZD validation builds a bounded acyclic phrase view

- Date: 2026-07-15
- Status: accepted

Parse parameter blocks and individual tokens transactionally: caller-visible
objects change only after complete structural validation. Scan a full token
region without producing raw output. For every right-present token below the
configured freeze threshold, store its two already-valid references and
checked expanded length in caller-owned `LzdPhraseEntry` workspace.

Derive the conservative workspace count from complete eight-byte tokens and
the configured phrase maximum. Count serialized input and phrase records
together against the aggregate internal-buffer limit before scanning. Report
truncation at the first incomplete token boundary and retain token index, byte
offset, committed logical output length, format category, and stable validation
category on failure. Reject forward references before any later decoder can
traverse them, making the stored grammar acyclic by construction.

## DD-115: LZD reference decoding validates before atomic expansion

- Date: 2026-07-15
- Status: accepted

Run the strict validator across the complete token region before writing any
raw byte. Reject insufficient output capacity, phrase workspace, expansion
workspace, or configured memory limits before publication, so these expected
failures leave the caller's output unchanged. Reusing the validator during
reference decoding is preferable to maintaining a second subtly different
parser while the clear implementation remains the priority.

Expand validated references iteratively with a caller-owned `uint32` stack.
Push a phrase's right reference before its left reference so last-in-first-out
processing preserves logical byte order. A grammar containing `N` stored
phrases needs at most `N + 1` stack entries because each expansion replaces
one phrase reference with two strictly earlier references. Include serialized
input, validator phrase records, and this expansion stack in the checked
aggregate internal-buffer limit. Treat a contradiction in already-validated
grammar metadata as an internal error rather than reading or writing outside
the supplied spans.

## DD-116: LZD reference encoding uses input-backed phrase records

- Date: 2026-07-15
- Status: accepted

Represent every generated phrase by the offset and length of its first
occurrence in the immutable raw frame. Search these bounded spans in ascending
reference order after considering the matching literal byte. Select only a
strictly longer candidate; LZD's longest-pair insertion rule makes generated
strings unique, while ascending traversal supplies deterministic behavior if
an internal contradiction were ever introduced.

Compute the exact token extent by running the same clear parse used for
serialization. A right-present token stores its complete input-backed span
when capacity remains; an absent-right terminal token stores nothing. Query at
most `min(floor(raw_size / 2), maximum_entries)` records because every inserted
token consumes at least two raw bytes. Check raw input plus phrase records
against the aggregate internal-buffer limit, then check serialized limits and
output capacity before emitting any token. This quadratic reference search is
the format oracle; later indexed searches must produce identical bytes.

## DD-117: LZD streaming decode commits one validated frame

- Date: 2026-07-15
- Status: accepted

Collect the complete dictionary-token region for one known-size raw frame in
caller-owned encoded storage. `EndInput` fixes that region: invoke the strict
atomic decoder into a separate caller-owned raw frame, then drain validated
bytes across arbitrary output splits. Preserve the draining state without
requiring callers to repeat `EndInput`. A malformed token anywhere in the
frame therefore publishes no raw byte, and the strict decoder's failing token
offset becomes the streaming error byte position.

Derive the conservative encoded extent as `8 * ceil(raw_size / 2)`. From that
extent derive the validator phrase records, an explicit expansion stack, and
the exact decoded extent. Reject unsupported host sizes, arithmetic overflow,
insufficient caller spans, or an aggregate encoded-plus-phrase-plus-stack-plus-
decoded extent beyond the local internal-buffer limit during construction.
Reject input beyond the conservative encoded extent before consuming any of
the offending call. Flush does not close a frame; `ResetBlock` remains
unsupported because the outer frame owns LZD dictionary reset.

## DD-118: LZD streaming encode preserves reference frame bytes

- Date: 2026-07-15
- Status: accepted

Collect exactly the declared raw frame in caller-owned storage, then run the
reference planner and encoder with caller-owned input-backed phrase records.
Drain the resulting canonical token region across arbitrary output splits.
This makes output independent of input and output chunking and permits a full
frame to encode and drain before a later zero-byte `EndInput`. When terminal
input accompanies the final raw bytes, retain it internally until all encoded
bytes drain.

Use a shared format helper for the checked `8 * ceil(raw_size / 2)` maximum
token extent so encoder and decoder workspace calculations cannot diverge.
Derive phrase records as `min(floor(raw_size / 2), maximum_entries)`. Validate
raw storage, maximum encoded storage, phrase records, and their aggregate byte
extent during construction before consuming input. Reject premature EndInput,
bytes beyond the declared frame, and `ResetBlock`; Flush exposes no output for
an incomplete frame and does not change the canonical parse.

## DD-119: The LZD None profile couples all frame workspaces

- Date: 2026-07-15
- Status: accepted

Define the first outer LZD pipeline as dictionary algorithm LZD variant 1 with
entropy None variant 0, a 16-byte dictionary parameter region, no entropy
parameters, and no entropy block size. For a trusted encoder configuration,
derive the largest raw frame, its shared `8 * ceil(raw_size / 2)` token bound,
`min(floor(raw_size / 2), maximum_entries)` input-backed records, and the
56-byte generic frame header. Require raw, complete encoded frame, and phrase
records together to fit the local internal-buffer limit.

Decoder workspace must not depend on untrusted stream parameters. Reserve the
local maximum raw frame and find the largest token payload allowed jointly by
dictionary-serialized, compressed-payload, dictionary-entry, and aggregate
memory limits. Include the complete encoded frame, phrase records, and an
explicit phrase-count-plus-one expansion stack in that aggregate. Use a
monotonic binary search and reject a local configuration when even zero payload
cannot coexist with the frame header, raw frame, and minimum stack entry.

## DD-120: LZD None frames remain atomic across the generic header

- Date: 2026-07-15
- Status: accepted

Represent each nonempty outer frame as the generic 56-byte frame header followed
by the exact canonical LZD token region. With entropy None, dictionary-
serialized size and compressed-payload size are identical. Validate pipeline,
sequence, contextual raw size, lengths, and generic reserved fields before
passing the payload to the strict LZD validator or decoder. Empty streams have
no frame; individual encoded frames are therefore always nonempty.

Plan the complete header-plus-payload extent before encoding and reject short
output without publication. Decode first parses an exact single-frame span,
checks output capacity, validates the full phrase grammar, and only then expands
raw bytes. Enforce raw plus complete frame plus encoder records when encoding;
complete frame plus phrase records when validating; and complete frame plus
raw output, phrase records, and expansion stack when decoding. These checks
keep standalone frame entry points within the same aggregate policy as the
profile and outer streaming path.

## DD-121: LZD one-shot streams validate every frame before publication

- Date: 2026-07-15
- Status: accepted

Serialize one generic stream header and one LZD parameter region, followed by
zero or more LZD plus None frames. Partition nonempty raw input at the declared
frame size, number frames from zero, and reset the byte alphabet and generated
phrase dictionary for every frame. Empty input contains only the 80-byte
prefix.

Planning requires the raw input size to equal the declared original size and
adds each exact planned frame extent with checked arithmetic. Decoding parses
the stream configuration transactionally, scans and validates every exact
frame extent, and only then performs a second scan that expands raw bytes.
Consequently a malformed later frame cannot publish bytes from an earlier
valid frame, and caller-visible stream and parameter objects remain unchanged
on every failure. The same phrase workspace is reused between frames because
frame validation rebuilds it after every dictionary reset; expansion uses its
own caller-supplied bounded stack during the publication scan.
The validation scan also preflights the complete serialized frame, raw frame,
phrase records, and conservative expansion stack against the aggregate internal
buffer limit, so no expected workspace or limit failure remains for the
publication scan.

## DD-122: LZD outer streaming decode commits complete frames independently

- Date: 2026-07-15
- Status: accepted

Collect the fixed 80-byte LZD stream prefix and then each complete serialized
LZD plus None frame across arbitrary input splits. Validate a frame header
before collecting its body and require caller-owned storage for the exact
serialized frame, raw frame, conservative phrase records, and conservative
expansion stack. Check all four regions together against the aggregate internal
buffer limit before accepting body bytes.

Decode one complete frame atomically into raw staging storage and drain that
storage through arbitrary output capacities before collecting the next frame.
A later malformed frame therefore cannot alter its own staging output, although
bytes already committed from earlier frames are not retracted. Retain EndInput
while staged output drains, accept a later empty EndInput, reject trailing bytes
and ResetBlock, and return EndOfStream only after the final frame has drained.

## DD-123: LZD outer streaming encode preserves one-shot stream bytes

- Date: 2026-07-15
- Status: accepted

Serialize the fixed 80-byte stream prefix during construction and drain it
before raw frame processing. Collect exactly the next declared raw frame in
caller-owned storage, invoke the reference LZD frame planner and encoder with a
separate input-backed phrase table, and drain the complete serialized frame
before preparing another one. Sequence numbers, dictionary resets, terminal
absent-right tokens, and every output byte therefore match the one-shot stream
under arbitrary input and output chunking.

Require raw storage and conservative encoder entries for the largest possible
frame before accepting input. After exact planning, enforce raw bytes, the
complete serialized frame, and encoder records together against the aggregate
internal-buffer limit and require serialized-frame capacity before encoding.
Flush may drain the prefix or a completed frame but does not shorten a partial
raw frame. Retain EndInput while output drains, accept a later empty EndInput,
and reject premature EndInput, trailing raw bytes, ResetBlock, and unknown
flags with stable terminal errors.

## DD-124: LZD fuzzing bounds phrase grammar, expansion, output, and calls

- Date: 2026-07-15
- Status: accepted

Feed each arbitrary input independently to the strict one-shot LZD stream
decoder and the outer frame-streaming decoder. Fix total output at 4 KiB, raw
frame size at 1 KiB, serialized payload at 4 KiB, phrase metadata at 512
records, and the explicit expansion stack at 513 entries. Set the aggregate
limit to the exact encoded-frame, raw-frame, phrase-record, and expansion-stack
sum so input fields cannot request unbounded allocation.

Derive streaming input and output chunks from bounded input bytes, validate
every `ProcessResult`, and cap calls by input size plus a fixed output margin.
Compile the harness in ordinary MSVC test builds but execute coverage-guided
fuzzing only in the explicit Clang sanitizer build. Keep every canonical
truncation, absent/forward phrase references, invalid token extent, extreme
frame lengths, and cross-frame reset references as permanent GoogleTest
regressions with one-shot atomicity assertions.

## DD-125: LZD C ABI keeps both decoder tables in one opaque view

- Date: 2026-07-15
- Status: accepted

Expose the known-size LZD plus None profile through the existing C ABI v1
transform lifecycle. Keep `maximum_entries` as the encoder format parameter and
all `max_*` fields as trusted local policy. Report encoder raw-frame,
serialized-frame, and input-backed phrase-table storage through the existing
three workspace fields.

The decoder additionally needs both phrase records and an iterative expansion
stack. Preserve the ABI v1 workspace record by reporting their checked,
alignment-padded sum as one opaque `views_workspace`; partition it internally
after validating the base address against the stricter alignment. No private
C++ type or offset crosses the ABI. Build the public LZD benchmark only through
this C surface, including four payload bytes of per-frame headroom for an odd
final byte.

## DD-126: LZD completion distinguishes local readiness from release evidence

- Date: 2026-07-15
- Status: accepted

Treat LZD variant 1 plus None as locally implementation-ready only after a
single public-ABI completion matrix covers empty input, every one-byte value,
all byte values, repeated bytes and patterns, deterministic high-entropy data,
frame-boundary neighbors, deterministic re-encoding, multi-frame operation,
and one-byte and mixed chunking. Require the C ABI lifecycle and benchmark
smoke in the same regression suite.

This status does not claim release completion. Cross-architecture deterministic
evidence, sanitizer and coverage-guided fuzz runs, representative benchmark
records, and the release similarity review remain explicit release gates.

## DD-127: LZD CLI remains a thin C-ABI streaming client

- Date: 2026-07-15
- Status: accepted

Add `lzd` as an explicit CLI codec without exposing or calling internal C++
types. Configure the same one-MiB known-size LZD plus None profile through the
public C ABI, use the existing 64-MiB aggregate workspace policy for
dictionary-based profiles, and retain the generic bounded I/O loop, temporary-
file commit, overwrite rejection, and malformed-input cleanup behavior.

Use a smaller deterministic repeated-text fixture for the LZD CLI smoke test
because the clear reference LZD encoder intentionally prioritizes correctness
over search performance. Arbitrary chunking and multi-frame behavior remain
covered independently by the completion matrix; reducing this integration
fixture does not change the format or codec acceptance surface.

## DD-128: LZMW uses fixed references and bounded dictionary freeze

- Date: 2026-07-15
- Status: accepted

Define LZMW variant 1 from the formal Miller-Wegman parsing: choose the longest
prefix among the byte alphabet and concatenations of previously adjacent
phrases, then register the just-completed previous-plus-current phrase pair.
Use the smallest numeric reference for equal expanded lengths and reset all
generated state at every outer frame.

Serialize each phrase as one little-endian 32-bit reference, with bytes at
`0..255` and generated entries from 256. Append one generated entry after every
phrase except the first while capacity remains, including a bytewise duplicate,
so decoder numbering never depends on an expensive equality search. Freeze the
dictionary at the configured maximum rather than implementing the original
LRU replacement proposal; this is a deterministic bounded marc variant and is
not claimed to interoperate with another LZMW representation.

Use a 16-byte parameter region containing maximum entries, zero flags, and zero
reserved bytes. Terminate by exact outer-frame size rather than a delimiter or
end token. Validate the complete fixed token grammar and checked phrase lengths
before implementing raw expansion or an encoder.

## DD-129: LZMW reference decode expands only validated acyclic grammar

- Date: 2026-07-15
- Status: accepted

Run the complete LZMW token validator before checking publication capacity or
writing raw output. Generated entry `i` contains only byte references or
generated references below `i`, because both adjacent phrases were available
before the new entry was registered. The grammar is therefore acyclic without
requiring a runtime visited set.

Expand iteratively through a caller-owned reference stack, pushing the right
child before the left child. A conservative `generated entries + 1` stack
bound covers one deferred right child per grammar depth. Check serialized token
bytes, validator phrase records, and the expansion stack together against the
aggregate internal-buffer limit. Any validation, output-capacity, stack-
capacity, host-size, or aggregate-limit failure must leave caller output
unchanged.

## DD-130: LZMW reference encode uses input-backed phrase spans

- Date: 2026-07-15
- Status: accepted

Represent every generated encoder phrase as an offset and length into the
immutable raw frame. Consecutive parsed phrases are adjacent input spans, so
their concatenation is exactly the span from the previous phrase start through
the current phrase end; phrase bytes need not be copied into dictionary
storage.

Search generated entries in ascending reference order and replace the initial
one-byte literal only for a strictly longer match. This directly implements
longest match with the smallest-reference tie break, including duplicate
dictionary strings. Use an exact planning pass to determine token count and
serialized size, enforce input-plus-workspace and serialized limits, and check
all caller capacity before publishing any token byte. The conservative
workspace count is `min(max(input_size - 1, 0), maximum_entries)`.

## DD-131: LZMW streaming decode publishes only complete validated frames

- Date: 2026-07-15
- Status: accepted

Adapt the atomic validator-first LZMW decoder to the transform contract by
collecting one declared frame's reference bytes, decoding into caller-owned raw
staging storage only when `EndInput` is observed, and draining that storage
after complete success. The maximum encoded extent is four bytes per declared
raw byte because every phrase emits one fixed token and every phrase expands to
at least one byte.

Include the encoded extent, phrase records, iterative expansion stack, and raw
staging extent in the constructor's aggregate limit. Reject excess encoded
input before consuming any part of that call. Preserve the draining state after
the final-input call, reject later input as trailing data, and make the ended
and error states stable across repeated calls. `Flush` does not terminate a
partial frame and `ResetBlock` is unsupported at this layer.

## DD-132: LZMW streaming encode preserves exact reference tokens

- Date: 2026-07-15
- Status: accepted

Buffer exactly one known-size raw frame, run the deterministic LZMW planning
and encoding passes once the declared size is collected, and drain the staged
fixed-reference bytes through arbitrary output spans. Allocate caller-owned
storage for the raw frame, the conservative four-byte token per raw-byte
extent, and `min(max(frame_size - 1, 0), maximum_entries)` phrase-span records;
check their complete aggregate before accepting input.

A full frame may encode and drain before `EndInput`, then wait for an empty
terminal call. Remember `EndInput` received while token bytes are still
draining. Reject premature termination and bytes beyond the declared raw size
before publishing more staged token bytes. Keep `Flush` non-terminal, reject
`ResetBlock`, and make ended and error states stable. The resulting token bytes
must equal one-shot reference encoding for every input and chunking.

## DD-133: LZMW plus None profile couples all frame workspace limits

- Date: 2026-07-15
- Status: accepted

Define the baseline outer profile as dictionary LZMW variant 1 followed by
entropy None variant 0, with the 16-byte LZMW parameter region and no entropy
parameter region. Encoder requirements use the largest actual frame,
`min(original_size, frame_size)`, its four-byte-per-raw-byte token bound, and at
most raw-size-minus-one phrase-span records.

Derive decoder requirements only from validated local limits, before any
untrusted stream header is accepted. Search the largest payload satisfying the
serialized, compressed-payload, dictionary-entry, and complete internal-buffer
limits. For `n` complete fixed tokens reserve
`min(max(n - 1, 0), max_dictionary_entries)` phrase records and, because a
nonempty declared frame may begin with a literal, one more expansion-stack
entry. Include the outer frame header and maximum decoded frame in the coupled
aggregate.

## DD-134: LZMW plus None frames reuse the generic atomic envelope

- Date: 2026-07-15
- Status: accepted

Store one independently reset LZMW token stream directly after the 56-byte
generic frame header. Set dictionary serialized size and compressed payload
size to the same fixed-token byte count; keep descriptor, model, hash, and
entropy-block fields zero. Do not repeat the 16-byte LZMW parameter region
inside a frame.

Encoding performs exact body planning and validates the complete contextual
header and raw-plus-frame-plus-encoder-workspace aggregate before publishing.
Decoding parses the complete frame extent, rejects trailing bytes, validates
the token grammar before expansion, and includes serialized frame, raw output,
phrase records, and expansion stack in its aggregate. Short output and all
malformed-body failures leave caller output unchanged.

## DD-135: LZMW one-shot streams preflight every frame before publication

- Date: 2026-07-15
- Status: accepted

Represent a complete known-size LZMW plus None stream as the 64-byte generic
stream header, the 16-byte LZMW parameter region, and zero or more complete
independently reset frames. Partition nonempty input by the declared raw frame
size, number frames from zero, and permit only the final frame to be short.

Encoding plans every frame and the exact total extent before writing the
prefix. Decoding parses header and parameters transactionally, scans and fully
validates every frame and required expansion workspace before publishing any
raw byte, then performs a second decode scan. Reject truncation, bytes after the
declared output completes, sequence or extent errors, and invalid parameters.
Publish parsed stream metadata only after the entire decode succeeds.

## DD-136: LZMW outer streaming decode commits only whole valid frames

- Date: 2026-07-16
- Status: accepted

Collect and validate the complete 80-byte LZMW plus None prefix, then process
each generic frame as header collection, bounded body collection, atomic frame
decode into caller-owned staging, and arbitrary raw-byte draining. Reuse all
frame, phrase, and expansion workspaces after each independently reset frame.

Validate sequence, remaining declared output, payload extent, every typed
workspace, and the full per-frame aggregate before collecting or decoding the
body. A corrupt frame publishes none of its bytes, while fully drained earlier
frames remain committed. Preserve a final `EndInput` while draining, accept a
later empty terminal call, keep `Flush` non-terminal, reject `ResetBlock` and
trailing bytes, and retain stable ended and error states.

## DD-137: LZMW outer streaming encode stages complete canonical frames

- Date: 2026-07-16
- Status: accepted

Serialize and drain the canonical 80-byte LZMW plus None prefix before
collecting raw bytes into one caller-owned frame buffer. Once a declared frame
is complete, plan and encode it atomically with the reference frame codec into
a second caller-owned buffer, then drain that exact representation through
arbitrary output spans. Reuse both buffers and the input-backed phrase-span
workspace for every independently reset frame.

Include the raw frame, full encoded frame, and active phrase-span records in
one checked aggregate. Preserve `EndInput` while pending bytes drain, permit a
full final frame to await a later empty terminal call, and treat `Flush` as
non-terminal so it never closes a partial frame. Reject premature or excess
raw input, `ResetBlock`, unsupported flags, and invalid configuration with
stable terminal errors.

## DD-138: LZMW C ABI publishes only opaque aligned workspace extents

- Date: 2026-07-16
- Status: accepted

Expose the known-size LZMW variant 1 plus entropy None pipeline through a new
size-tagged `marc_lzmw_config`, workspace query, and transform factory. Preserve
all existing ABI constants and declarations; this integration adds symbols and
one independent configuration type without changing an existing layout.

Return the raw and encoded/decoded frame buffers as primary and secondary
extents. Return one aligned opaque views extent containing input-backed phrase
records for encode, or phrase records followed by an aligned iterative
expansion stack for decode. Validate sizes and alignment before constructing a
non-throwing C++ transform. No internal C++ type, count, or offset crosses the
ABI, and all processing continues through the common opaque transform API.

## DD-139: LZMW local completion remains distinct from release evidence

- Date: 2026-07-16
- Status: accepted

Treat LZMW variant 1 plus entropy None as locally implementation-ready only
after a public-C-ABI completion matrix covers empty input, every one-byte value,
all byte values, repeated bytes and patterns, deterministic high-entropy data,
frame-boundary neighbors, deterministic re-encoding, multiple frames, and
one-byte and mixed chunking, and after a bounded decoder fuzz harness has its
compile-smoke and permanent regressions in the suite. Require a C-ABI-only
benchmark smoke that verifies its round trip before reporting ratio,
encode/decode throughput, and workspace. This change establishes the matrix
and benchmark; DD-140 establishes the bounded fuzz compile-smoke and permanent
regression gate.

This status does not claim release completion. Cross-platform deterministic
evidence, sanitizer and coverage-guided fuzz execution, representative Release
benchmark records, and the final similarity review remain explicit release
gates.

## DD-140: LZMW fuzzing bounds fixed references, phrase expansion, and calls

- Date: 2026-07-16
- Status: accepted

Feed each arbitrary input independently to the strict one-shot LZMW stream
decoder and the outer frame-streaming decoder. Fix total output at 4 KiB, raw
frame size at 1 KiB, serialized payload at 4 KiB, phrase metadata at 1024
records, and the explicit expansion stack at 1025 entries. Set the aggregate
limit to the exact encoded-frame, raw-frame, phrase-record, and expansion-stack
sum so input fields cannot request unbounded allocation.

Derive streaming input and output chunks from bounded input bytes, validate
every `ProcessResult`, and cap calls by input size plus a fixed output margin.
Compile the harness in ordinary MSVC test builds, but execute coverage-guided
fuzzing only in the explicit Clang sanitizer build. Keep every canonical
truncation, absent or forward fixed reference, invalid token extent, extreme
frame length, and cross-frame reset reference as permanent GoogleTest
regressions with one-shot output and metadata atomicity assertions.

## DD-141: LZMW CLI remains a transactional public-C-ABI client

- Date: 2026-07-16
- Status: accepted

Add `lzmw` as an explicit command-line codec without exposing or invoking an
internal C++ LZMW type. Configure the one-MiB known-size LZMW plus None profile
through `marc_lzmw_*`, use the common 64-MiB dictionary-profile aggregate
policy, query all direction-specific workspaces, and retain the shared bounded
process loop.

Preserve destination and `.tmp` overwrite rejection, remove staged output on
every failure, and rename only after complete successful close. Use the bounded
320-repeat integration fixture because the reference phrase search prioritizes
clarity. Completion-matrix chunking remains independent of this file-level
smoke and the stream representation is unchanged.

## DD-142: The first combined pipeline is LZ77 plus Blocked Huffman

- Date: 2026-07-16
- Status: accepted

Define the first dictionary-plus-entropy profile as LZ77 variant 1 followed by
Blocked Huffman variant 1. LZ77 produces its unchanged canonical 16-byte token
stream. Blocked Huffman consumes those bytes as symbols in fixed-size blocks;
no entropy block crosses an outer frame, and every frame resets both dictionary
history and Huffman models.

Keep the 16-byte LZ77 parameter region and the empty Blocked Huffman parameter
region in normal stream-prefix order. In each frame, `uncompressed_size` is raw
LZ77 output bytes, `dictionary_serialized_size` is the exact token extent, and
`compressed_payload_size` is the sum of stored entropy payload bytes. Store the
Blocked Huffman descriptor/model region immediately after the generic frame
header and all entropy payloads after that region. Do not store a second copy
of the dictionary token stream.

Use the existing algorithm and variant IDs without changing either standalone
profile. The combined decoder must validate the generic header and complete
entropy layout, decode exactly the declared dictionary byte count into bounded
staging, validate the full LZ77 token stream against the raw frame extent, and
only then publish raw bytes. A failure in either layer publishes no byte from
that frame; previously committed frames remain committed in streaming decode.

## DD-143: Combined-frame validation stops at canonical dictionary bytes

- Date: 2026-07-16
- Status: accepted

Make the first executable LZ77 plus Blocked Huffman component a strict,
complete-frame validator rather than an encoder or raw-output decoder. Parse
the generic header, validate and publish caller-owned entropy block views only
after the complete descriptor/model region is valid, entropy-decode exactly
`dictionary_serialized_size` bytes into caller-owned staging, and validate that
staged extent as a complete canonical LZ77 token stream producing exactly
`uncompressed_size` bytes.

Do not accept a raw-output span in this API. This makes premature publication
structurally impossible while the combined decoder is still being built. Count
the descriptor/model bytes, entropy payload bytes, dictionary staging, and
typed block views together against `max_internal_buffered_bytes`, using checked
arithmetic. Capacity, aggregate-limit, entropy-layout, entropy-payload, and
dictionary-token failures receive distinct stable categories.

## DD-144: Combined raw decode reuses the validated dictionary extent

- Date: 2026-07-16
- Status: accepted

Build the first combined raw decoder directly on DD-143. A frame must complete
generic, entropy-layout, entropy-payload, and canonical LZ77 validation into
dictionary staging before raw-output capacity is considered. Then invoke the
standalone transactional LZ77 decoder over exactly that validated staging and
exactly the declared raw extent.

Keep raw output outside the internal-workspace aggregate because it is the
caller's committed destination, not buffered intermediate state. A short raw
span returns its own stable error after validation and changes no raw byte.
Malformed entropy or dictionary data likewise cannot reach the raw decoder.
Output beyond `uncompressed_size` is never written.

## DD-145: Exact combined planning materializes dictionary bytes

- Date: 2026-07-16
- Status: accepted

Require caller-owned dictionary staging in both the exact frame planner and
encoder. LZ77 token count alone cannot determine Blocked Huffman frequencies,
model selection, descriptor extent, or payload extent. The planner therefore
materializes the canonical token stream, plans every entropy block from those
exact bytes, validates the resulting generic frame header, and reports all
component extents before serialized output is touched.

Treat dictionary staging as scratch that may change on any plan reaching LZ77
encoding. A short staging span changes neither staging nor serialized output;
a short serialized destination may contain the planned staging but leaves the
serialized destination unchanged. Entropy blocks measure dictionary bytes,
may occur multiple times per frame, and retain the existing final-short-block
rule. Serialized output is not counted as intermediate workspace.

## DD-146: Combined complete streams use two-pass atomic decode

- Date: 2026-07-16
- Status: accepted

Serialize a known-size LZ77 plus Blocked Huffman stream as the existing 64-byte
stream header, the existing 16-byte LZ77 parameter region, and zero or more
combined frames. Empty input is exactly the 80-byte prefix. Reuse one
caller-owned dictionary staging span and one caller-owned block-view array for
every frame; their capacities must cover the largest frame, not the sum of all
frames.

Plan every frame before whole-stream encoding so a short serialized destination
is unchanged. Decode in two complete passes: the first parses and validates all
frames through canonical dictionary staging without raw output, and the second
repeats the deterministic traversal and publishes raw frame extents. Publish
the parsed stream header and LZ77 parameters only after both passes succeed.
Consequently malformed later frames leave the entire raw destination and
configuration outputs unchanged. Both LZ77 history and every Blocked Huffman
model reset at each outer-frame boundary.

## DD-147: Combined streaming encode stages three bounded extents

- Date: 2026-07-16
- Status: accepted

Implement known-size combined streaming encode with three disjoint,
caller-owned workspaces: one raw outer frame, its worst-case canonical LZ77
token bytes, and one complete serialized combined frame. Emit the fixed 80-byte
prefix first, collect exactly one raw frame, plan and encode it transactionally,
then drain it before accepting bytes for the next frame. Reuse all three spans
after each drain.

Count the actual raw frame, dictionary staging, and serialized frame together
against `max_internal_buffered_bytes` before committing the frame. Require
dictionary staging for the worst-case 16 bytes per raw input byte at
construction so arbitrary frame contents cannot cause a later capacity
surprise. `Flush` does not close a partial outer frame, `ResetBlock` is
unsupported, and `EndInput` must accompany exactly all remaining known-size
input. Repeated ended/error calls retain stable terminal results.

## DD-148: Combined streaming decode commits one validated frame at a time

- Date: 2026-07-16
- Status: accepted

Use four reusable caller-owned decoder workspaces: one complete serialized
frame, its entropy-decoded dictionary bytes, its raw decoded bytes, and the
Blocked Huffman views for that frame. After collecting the 80-byte prefix,
collect and validate each complete frame, decode it into raw frame staging, and
only then drain that staging through partial output buffers. A malformed frame
publishes none of its raw bytes, while earlier fully drained frames remain
committed.

At frame-header acceptance, check every workspace independently and count the
serialized frame, dictionary staging, raw staging, and typed views together
against `max_internal_buffered_bytes`. Latch `EndInput` whenever its complete
input span has been consumed, including while a non-final decoded frame is
still draining. Every later collection state must observe that latch so output
starvation cannot turn premature termination into an indefinite `NeedInput`.
`Flush` does not change framing and `ResetBlock` remains unsupported.

## DD-149: Combined profiles bound the uncompressed dictionary worst case

- Date: 2026-07-16
- Status: accepted

Define the encoder's worst case independently of input content: every raw byte
becomes one 16-byte LZ77 Literal token and every Blocked Huffman block selects
its mandatory raw representation. For the largest actual raw frame, derive the
dictionary extent, entropy block count, descriptor extent, complete serialized
frame extent, and the streaming encoder's three-workspace aggregate with
checked arithmetic. Reject profiles that cannot encode arbitrary frame content
within local dictionary, payload, block-count, or aggregate limits.

Derive decoder workspace from local policy rather than trusted stream fields:
`56 + max_internal_buffered_bytes` serialized bytes,
`max_dictionary_serialized_size` dictionary bytes, `max_frame_size` raw bytes,
and `max_blocks_per_frame` typed views. Runtime frame validation still applies
the four-way aggregate to actual declared extents. Empty known-size streams
require no frame workspace. Map profile failures to stable core categories and
prove that returned requirements can directly construct both streaming
transforms for a round trip.

## DD-150: The combined C ABI retains three caller-owned regions

- Date: 2026-07-16
- Status: accepted

Expose the LZ77 plus Blocked Huffman profile through its own versioned C
configuration while retaining the common `marc_workspace_requirements` shape.
The primary region has the usual frame role. Partition the secondary byte
region internally: dictionary staging precedes serialized-frame staging for
encode, and dictionary staging precedes raw-frame staging for decode. The
decoder's aligned views region contains the private Blocked Huffman block-view
array; encoding requires no views.

The requirements query performs every partition sum and typed-view byte
calculation with checked arithmetic. Creation repeats the profile calculation,
validates all capacities and view alignment before constructing the transform,
and exposes none of the private C++ record layouts. This keeps the ABI small
without weakening caller ownership, bounded allocation, or strict separation
between dictionary and entropy staging.

## DD-151: CLI composition selection is explicit and keeps LZ77 default

- Date: 2026-07-16
- Status: accepted

Name the composed command-line profile `lz77-blocked-huffman`. Keep unqualified
`marc encode` and `marc decode` mapped to standalone LZ77 variant 1 so adding an
entropy layer does not silently change existing output. Both directions use
the public combined C ABI; the CLI does not reach into C++ codec internals.

For the fixed 1 MiB outer frame and 65,536-symbol entropy block, derive local
workspace policy from the same all-Literal and all-raw bounds as the combined
profile: 16 bytes of dictionary serialization per raw byte, 16 descriptor
bytes per entropy block, and the complete three-way encoder aggregate. The CLI
continues to require known-size regular-file input and atomically renames a
temporary output only after transform completion.

## DD-152: Combined benchmarks use the public ABI and complete-stream bounds

- Date: 2026-07-16
- Status: accepted

Add `lz77-blocked-huffman` to the dependency-free benchmark selector without
introducing an internal C++ shortcut. Configure, query, create, process, and
destroy through the public combined C ABI, and verify a full round trip before
timing under the existing measurement contract.

Size the encoded destination for the 16-byte-per-input dictionary worst case,
the 56-byte header of every outer frame, and 16 descriptor bytes for each of
the maximum 256 entropy blocks per full frame. Use the same three-way aggregate
limit as the CLI and profile. Report the exact queried primary, secondary, and
views workspaces and include them in the existing peak caller-owned workspace
metric; do not count corpus, encoded, or decoded vectors.

## DD-153: Combined fuzzing is bounded before parsing

- Date: 2026-07-16
- Status: accepted

Exercise both the strict one-shot and frame-streaming LZ77 plus Blocked Huffman
decoders from one coverage-guided entry point. Truncate supplied cases to 8 KiB
inside the harness rather than relying only on a runner option. Fix local
policy at 4 KiB total output, 1 KiB per frame, 4 KiB dictionary/payload staging,
and eight entropy block views. Count serialized, dictionary, raw, and typed-view
storage in the aggregate bound.

Derive input and output chunk sizes from bounded input bytes, validate every
`ProcessResult`, and cap calls at bounded input plus bounded output plus a small
state-transition margin. Treat an invalid result, stalled non-starvation state,
or exhausted call guard as a reproducible failure. Normal MSVC test builds only
compile this entry point as an object; instrumented exploration remains an
explicit Clang/libFuzzer workflow with sanitizer coverage.

Treat every file below `fuzz/corpus/` as binary in Git so checkout-time line
ending conversion cannot change a reproducer or seed byte sequence.

## DD-154: Combined local completion remains distinct from release evidence

- Date: 2026-07-16
- Status: accepted

Treat LZ77 variant 1 plus Blocked Huffman variant 1 as locally
implementation-complete only after one public-C-ABI completion matrix covers
empty input, every one-byte value, all byte values in sequence, long zero runs,
repeated binary patterns, deterministic high-entropy data, frame boundaries,
multiple frames, repeat encoding, and mixed input/output chunk sizes.

This local status depends on the existing exact format, validator, one-shot and
streaming codecs, profiles, malformed regressions, bounded fuzz harness, C ABI,
CLI, and benchmark tests. It does not claim release completion. A real
sanitizer-backed fuzz campaign, cross-compiler and cross-architecture byte
identity, package-consumer validation, and a final similarity review remain
release evidence to gather separately.

## DD-155: Windows sanitizer fuzzing uses the matching static CRT

- Date: 2026-07-16
- Status: accepted

When the explicit fuzzer build uses Clang's GNU-style driver on Windows, select
the static multithreaded C runtime before creating any target. The distributed
libFuzzer runtime uses that runtime model, so mixing it with CMake's default
dynamic runtime fails at link time before any test can execute.

Keep the compiler installation path local. Discover the Clang resource directory
through the compiler and add its `lib/windows` child to `PATH` when executing a
sanitizer binary. This is runner setup rather than a stream-format or public-ABI
property and does not affect ordinary MSVC builds where fuzzers are disabled.

## DD-156: C ABI assertions remain active in optimized test builds

- Date: 2026-07-16
- Status: accepted

The pure-C ABI tests use the standard C `assert` facility for both status
checks and compact call-and-check expressions. Include a test-only wrapper that
undefines `NDEBUG` before including the standard header so Release and
RelWithDebInfo builds execute the same API calls and validations as Debug
builds. This policy is test-local and does not alter marc or its consumers.

Give each C ABI test a 30-second CTest timeout. A missing assertion or stalled
transform must become a bounded test failure rather than an indefinitely
running CI job. Cross-compiler verification must include an optimized build so
this test-configuration contract remains exercised.

## DD-157: Compiler independence requires complete archive comparison

- Date: 2026-07-16
- Status: accepted

Use MSVC with MSBuild as the Windows reference and Clang's GNU-style driver with
Ninja as an independent compiler path. Both optimized builds must compile the
shared library, static library, C11 ABI clients, CLI, tests, and benchmarks and
must pass the same test suite.

In addition to in-process determinism tests, encode one common repository-owned
input through every public dictionary CLI profile and the combined LZ77 plus
Blocked Huffman profile. Compare complete output files byte for byte across the
two compilers. This check covers explicit serialization and coder decisions but
does not claim cross-architecture identity; that remains a distinct release
gate.

## DD-158: CI publishes self-describing interoperability bundles

- Date: 2026-07-16
- Status: accepted

After their complete test suites pass, the Windows/MSVC and Ubuntu/Ninja jobs
generate the same bounded binary fixture and encode it with every public
dictionary-oriented CLI selection. Publish the fixture, seven archives, and a
versioned JSON manifest as platform-named workflow artifacts. The manifest
records the source revision, platform metadata, file sizes, and SHA-256 values
without embedding machine-local paths.

Provide one external verifier that treats manifest fields as untrusted, accepts
only the exact codec set and leaf file names, checks sizes and hashes, decodes
every foreign archive, and re-encodes the fixture for exact archive comparison.
Require a new output directory so verification never overwrites caller files.
The bundle proves reproducibility and interoperability only; unsigned hashes do
not authenticate the workflow producer.

## DD-159: CRC-32C begins as a format-neutral hash primitive

- Date: 2026-07-16
- Status: accepted

Reserve hash algorithm ID 1 for CRC-32C using the reflected Castagnoli
parameters documented in `docs/format.md`. Serialize its final 32-bit numeric
value little-endian so digest bytes follow the repository-wide integer rule.

Implement a clear byte-at-a-time, table-free reference algorithm with constant
state and no platform intrinsics. Finalization is a non-mutating snapshot;
`HashTap`, rather than the algorithm object, owns terminal lifecycle policy.
Reject every digest span whose size is not exactly four bytes without changing
it. Do not yet permit hash descriptors or checksum trailers in version 1.0
streams; their target, scope, and inclusion ranges require a separate decision.

## DD-160: SHA-256 preserves its standard digest byte string

- Date: 2026-07-16
- Status: accepted

Reserve hash algorithm ID 2 for SHA-256 exactly as defined by FIPS 180-4. The
32-byte digest is an algorithm-defined byte string, not a repository integer;
retain the standard most-significant-byte-first word concatenation rather than
reversing it under marc's little-endian integer rule.

Use a clear incremental reference implementation with one 64-byte buffer,
eight state words, and checked 64-bit message-bit length. Reject an entire
update before mutation if its length cannot be represented by the FIPS length
field. Finalize through a copied state so repeated snapshots are identical and
further updates remain possible. Require exactly 32 output bytes and leave a
wrong-sized span unchanged. Keep SHA-256 format-neutral until whole-stream hash
descriptor scope and inclusion ranges are separately specified.

## DD-161: Hash descriptors are validated before stream integration

- Date: 2026-07-16
- Status: accepted

Define one fixed 16-byte, little-endian hash descriptor containing algorithm,
target, scope, digest size, zero flags, and zero reserved bytes. Recognize only
the two implemented algorithms and require their exact digest sizes. Keep the
parser allocation-free and transactional: malformed input must not publish a
partially parsed descriptor, and invalid serialization must not alter output.

This record is a bounded format primitive, not an activation of hashing in
version 1.0. Continue rejecting nonzero version 1.0 hash regions. A later
stream version must separately define descriptor ordering, supported
target/scope combinations, exact inclusion ranges, and digest placement. This
prevents a provisional helper from silently changing an existing stream.

## DD-162: Hash descriptor regions have one canonical tuple order

- Date: 2026-07-16
- Status: accepted

Represent a descriptor region as zero or more complete 16-byte records ordered
strictly by `(target, scope, algorithm ID)`. Reject a partial final record,
identical tuple duplicates, and descending tuples. Permit different algorithms
at one target/scope boundary so a checksum and cryptographic hash are not made
artificially exclusive.

Parse in two allocation-free passes: validate all bytes and ordering first,
then publish to a caller-owned descriptor span. Leave both that span and its
published count unchanged on failure. Serialization likewise validates the
complete input and checked required byte count before writing. Region capacity
is supplied by the caller now and will be coupled to explicit decoder limits
when a later stream version activates descriptors.

## DD-163: Version 1.1 prefix parsing is an isolated staged gate

- Date: 2026-07-16
- Status: accepted

Reserve minor version 1 for hash-aware framing while keeping its complete
stream layout disabled. Add separate prefix validation, parsing, and
serialization entry points that require version 1.1, a descriptor byte count
divisible by 16, zero extensions, and a checked combined variable-region size
within the local buffer limit.

Do not broaden the existing version 1.0 entry points. Every current stream
adapter continues to call them and therefore rejects 1.1 before it could treat
descriptor bytes as a frame header. The staged serializer produces only a
prefix primitive for hand vectors and future composition; no public stream
encoder may select it until digest targets, inclusion ranges, and trailers are
fully specified and implemented.

## DD-164: The first hash profile is per-frame CRC-32C over raw bytes

- Date: 2026-07-16
- Status: accepted

Limit the first version 1.1 hash profile to exactly one descriptor: CRC-32C,
UncompressedBytes, PerFrame. Store one four-byte little-endian numeric digest
after every nonempty frame payload and require the frame header's checksum
trailer size to be exactly four.

Hash only the decoded frame's logical uncompressed bytes and reset CRC state at
each frame boundary. Exclude every header, parameter, descriptor, compressed
byte, padding bit, and the digest itself. Implement profile validation,
one-shot trailer generation, and verification as an allocation-free component
before changing generic frame or stream codecs. Checksums detect corruption;
this profile does not provide authenticity.

## DD-165: Version 1.1 frame headers require three-way checksum agreement

- Date: 2026-07-16
- Status: accepted

Add isolated version 1.1 frame-header entry points. They accept the existing
56-byte frame layout only when the stream prefix declares one 16-byte hash
record, the caller supplies exactly the supported per-frame CRC-32C descriptor,
and the frame declares a four-byte checksum trailer. Validate descriptor-region
size, descriptor semantics, and trailer size together before body traversal.

Keep the ordinary frame-header entry points strict to version 1.0, an empty
descriptor view, and a zero trailer. Include the trailer extent in checked
frame-local buffered-byte accounting. Do not yet change any public stream or
codec adapter; staged frame parsing remains unreachable from their version 1.0
paths.

## DD-166: The first complete 1.1 stream is a transactional raw profile

- Date: 2026-07-16
- Status: accepted

Compose the staged prefix, canonical CRC descriptor, staged frame header, raw
payload, and per-frame trailer into an internal None / None version 1.1
reference stream. Require known original size, deterministic frame partitioning,
and the exact 80-byte prefix-plus-descriptor even for empty input.

Plan encoding completely before publication. Decode in two passes: the first
parses every header, proves every extent, and verifies every CRC without writing
raw output; the second copies the previously validated payload spans. Reject
truncation, trailing data, descriptor disagreement, size overflow, and checksum
mismatch transactionally. Keep public selectors and C ABI construction on
version 1.0 until this reference composition has broader streaming and profile
integration.

Compile its decoder fuzz boundary in every test build and provide a dedicated
Clang/libFuzzer target. Cap supplied bytes at 8 KiB and decoded output at 4 KiB;
use only fixed caller-owned storage and conservative local limits. A short
hand-authored `MARC` prefix seed exercises truncation without importing an
external corpus.

## DD-167: Raw checksum streaming commits at verified frame boundaries

- Date: 2026-07-16
- Status: accepted

Add allocation-free incremental transforms for the complete None / None
version 1.1 profile. The encoder stores one raw frame directly in its
caller-owned serialized-frame workspace, then writes the header and CRC around
that payload before draining it. It never needs a second frame-sized copy.

The decoder collects one complete serialized frame in caller-owned storage and
verifies its header, extent, and CRC before making any byte from that frame
available downstream. A later malformed frame may follow already committed
frames, as required by streaming operation, but no prefix of the malformed
frame is published. Prefix, input, and output may all be split one byte at a
time; terminal input remains latched while verified bytes drain through short
output buffers.

Both transforms reject ResetBlock because frame boundaries are determined by
the known original size and configured frame size. Flush does not create a
short frame and only exposes already completed representation. Repeated calls
after completion return EndOfStream, while every terminal error remains sticky.
Keep these transforms internal until a profile workspace query and C ABI
construction contract are separately accepted.

## DD-168: The raw checksum profile owns canonical construction and sizing

- Date: 2026-07-16
- Status: accepted

Centralize version 1.1 raw-checksum construction in one internal profile. Given
known original size and frame size, it produces the None / None stream header,
the sole canonical CRC-32C / UncompressedBytes / PerFrame descriptor, and the
exact largest serialized-frame workspace required by the encoder. Empty input
requires no frame workspace because its complete representation is held in the
transform's fixed prefix storage.

Calculate decoder workspace using only caller policy limits, never untrusted
stream fields. The maximum accepted raw payload is bounded jointly by frame,
compressed-payload, dictionary-serialized, uint32 representation, and aggregate
internal-buffer limits. Report the one serialized-frame span shared by the
incremental decoder's collection and drain phases. Profile failure clears every
output and maps to the existing stable core error categories.

Do not expose a configurable hash choice in this initial profile. A selectable
descriptor set would define a different compatibility and workspace contract.
Keep C ABI publication as the next separate step so its size-tagged structure
can depend only on this tested profile layer.

## DD-169: A dedicated C ABI publishes the fixed checksum profile

- Date: 2026-07-16
- Status: accepted

Expose the complete version 1.1 None / None plus per-frame CRC-32C profile
through `marc_checksum_raw_config`, a workspace query, and a transform factory.
Add these symbols under C ABI version 1 without changing any existing structure
or function. Existing public codec selectors retain their version 1.0 stream
representations.

The size-tagged config contains known original size, frame size, the five local
limits relevant to raw framing, and zero-checked reserved fields. There is no
hash algorithm field: the public name selects exactly the canonical descriptor
defined by the profile. Both directions use only `primary_workspace`; report
zero secondary/views bytes and alignment one.

The C adapter allocates only the small opaque transform and implementation
objects with non-throwing allocation. Frame storage remains caller-owned for
the handle lifetime. Test exact one-shot and one-byte-chunk encoding identity,
round trip, configuration tags, workspace capacity, and corruption in a later
frame. Streaming decode may publish earlier verified frames, but must suppress
the complete corrupted frame.

## DD-170: The CLI dogfoods checksum framing through the C ABI

- Date: 2026-07-16
- Status: accepted

Add the explicit command-line codec name `checksum-raw`. Keep LZ77 as the
default and require callers to use the same explicit name for decode. The CLI
must configure, query, create, process, and destroy this profile only through
`marc_checksum_raw_*`; it must not include or invoke internal C++ frame code.

Use the existing 1 MiB outer-frame policy. Bound raw payload and dictionary
serialized bytes to one frame and aggregate serialized-frame workspace to
`56 + frame_size + 4`. Preserve existing partial I/O, temporary-file commit,
overwrite rejection, and malformed-input cleanup behavior. Add a complete and
empty round-trip CTest plus a multi-frame trailing-data cleanup regression
through the common CLI script. CLI publication does not yet add this profile
to the fixed interoperability artifact manifest; that manifest change requires
a separately versioned codec set.

## DD-171: Raw checksum benchmarking establishes framing baseline cost

- Date: 2026-07-16
- Status: accepted

Add `checksum-raw` to the dependency-free benchmark selector through the public
C ABI only. Measure the same verified encode/decode lifecycle, serialized to
input ratio, MiB/s, and caller-owned workspace fields reported for every other
profile. This mode is a framing and CRC baseline, not a compression-ratio
competitor.

Use payload factor one, an 80-byte stream prefix, and per-frame overhead
`56 + 4` when computing the bounded destination capacity. Use one primary
workspace of `56 + frame_size + 4` in either direction and report zero secondary
and views regions as returned by the ABI. Run a one-iteration optimized smoke
against the repository README. Do not include corpus/result buffers or the two
small allocated transform objects in `codec_peak_workspace_bytes`.

## DD-172: Interoperability schema 2 adds the checksum profile explicitly

- Date: 2026-07-16
- Status: accepted

Publish new interoperability bundles as schema version 2 with codec-set ID
`marc-cli-v2`. Its exact eight-profile set is `checksum-raw` followed by the
seven schema-1 profiles. Generate, locally round-trip, hash, upload, foreign
decode, and exact re-encode the checksum archive under the same bounded fixture
protocol as the existing archives.

Keep the verifier backward compatible with schema 1 and its exact seven-profile
set. A schema-1 manifest must not acquire the new profile implicitly. Schema 2
must declare the exact codec-set ID, archive count, unique allowed codec names,
leaf file names, sizes, and hashes before invoking the CLI. Continue to treat
artifact hashes as transfer integrity rather than producer authentication.

## DD-173: Raw checksum local completion is audited through the public ABI

- Date: 2026-07-16
- Status: accepted

Mark the fixed checksum-raw profile locally implementation-complete only after
one public-C-ABI matrix covers empty input, every one-byte value, the full byte
alphabet, repetitive and deterministic high-entropy inputs, frame-boundary
lengths, deterministic output, and multi-frame input/output chunk schedules.
Repeated calls after successful completion must remain EndOfStream.

Independently corrupt and truncate the last frame and append trailing data.
Require a stable malformed-stream state and frame-atomic publication: the first
three verified 64-byte frames may be returned, while the final one-byte frame
remains uncommitted in all three cases. This local status includes existing
format, component, C, CLI, fuzz, benchmark, and interoperability evidence, but
does not claim release completion without external cross-platform execution.

## DD-174: Adaptive Huffman gets a bounded dual-decoder fuzz boundary

- Date: 2026-07-17
- Status: accepted

Exercise both the strict one-shot Adaptive Huffman stream decoder and the
frame-committing incremental decoder from one libFuzzer entry point. Truncate
each supplied case to 8 KiB, permit at most 4 KiB total output, 1 KiB frames,
4 KiB payloads, and 4 KiB of frame-local buffered descriptor-plus-payload
bytes. Use fixed caller-owned arrays only.

Derive bounded input and output chunk sizes from supplied bytes, validate every
ProcessResult, and abort on an invalid result, an impossible starvation state,
or exhaustion of the checked call ceiling. Compile the harness in every normal
test build and provide an instrumented Clang target plus a repository-authored
truncated-magic seed. A bounded smoke run is execution evidence, not coverage
completion.

## DD-175: Dynamic Range gets a bounded dual-decoder fuzz boundary

- Date: 2026-07-17
- Status: accepted

Exercise both the strict one-shot Dynamic Range stream decoder and its
frame-committing incremental decoder from one libFuzzer entry point. Use the
same 8 KiB input, 4 KiB total-output, 1 KiB frame, 4 KiB payload, and 4 KiB
frame-local buffered-byte limits as the adjacent entropy fuzz boundary, while
fixing the accepted adaptive-model total to variant 1's exact 32,768.

Use only fixed caller-owned arrays, derive bounded 17-byte input and 19-byte
output chunks from supplied bytes, validate every ProcessResult, and abort on
invalid progress, impossible starvation, or checked call-ceiling exhaustion.
Compile the harness in every normal test build and expose an instrumented Clang
target with a repository-authored truncated-prefix seed. Smoke execution is
evidence of the path, not a claim of coverage completion.

## DD-176: rANS fuzzing bounds block metadata and decode tables explicitly

- Date: 2026-07-17
- Status: accepted

Exercise both the strict one-shot rANS stream decoder and frame-committing
incremental decoder from one libFuzzer entry point. Truncate input to 8 KiB,
permit 4 KiB total output, 1 KiB frames, 256-symbol blocks, 4 KiB compressed
payloads, 8 KiB descriptor-plus-payload buffering, at most eight blocks per
frame, and exactly the variant's 4,096 entropy table entries.

Supply fixed aligned arrays for eight `RansBlockView` records to both paths;
never allocate views from serialized block counts. Retain the bounded
byte-derived chunk schedule, ProcessResult validation, starvation checks, and
checked call ceiling. Provide warning-clean normal-build compilation, an
instrumented Clang target, and the repository-authored truncated-prefix seed.
A short sanitizer campaign is execution evidence rather than coverage
completion.

## DD-177: tANS fuzzing fixes views, table size, and transition storage

- Date: 2026-07-17
- Status: accepted

Exercise the strict one-shot and frame-committing tANS stream decoders from one
libFuzzer entry point. Use the rANS boundary's 8 KiB input, 4 KiB output,
1 KiB frame, 256-symbol block, 4 KiB payload, 8 KiB internal, eight-block, and
4,096-table-entry limits. The latter fixes variant 1's table log at 12 while
still sending malformed state and additional-bit transitions to validation.

Give both paths fixed aligned arrays of eight `TansBlockView` records and fixed
byte workspaces. Retain byte-derived 17/19-byte chunks, ProcessResult checks,
starvation assertions, and a checked call ceiling. Compile warning-clean in
normal builds, expose a Clang sanitizer target, and start from the reviewed
truncated-prefix seed. A bounded campaign is execution evidence, not coverage
completion or FSE compatibility evidence.

## DD-178: Standalone Blocked Huffman receives its own fuzz boundary

- Date: 2026-07-17
- Status: accepted

Do not treat the combined LZ77 plus Blocked Huffman target as complete coverage
of the dictionary-none Blocked Huffman stream. Exercise its strict one-shot and
frame-committing decoders from a dedicated libFuzzer entry point with 8 KiB
input, 4 KiB output, 1 KiB frames, 256-symbol blocks, 4 KiB payload, 8 KiB
descriptor-plus-payload buffering, and at most eight fixed block views.

Cap canonical code lengths at 24 and decoder table nodes at 512, covering both
canonical and raw block representations without allocating from serialized
metadata. Retain 17/19-byte chunk caps, ProcessResult and starvation checks, and
the checked call ceiling. Add normal-build compile-smoke, a Clang sanitizer
target, and the reviewed truncated-prefix seed. A short campaign proves only
that the instrumented boundary executes without an observed finding.

## DD-179: Standalone LZ77 receives a dedicated dual-decoder fuzz boundary

- Date: 2026-07-17
- Status: accepted

Do not rely on the LZ77 plus Blocked Huffman composition target to cover the
entropy-None LZ77 stream header, payload, and strict decoder branches. Exercise
both its one-shot decoder and frame-committing outer decoder from one libFuzzer
entry point. Truncate input to 8 KiB and permit 4 KiB total output, 1 KiB
frames, 4 KiB canonical token payloads, and only fixed caller-owned frame
arrays.

Retain the byte-derived 17/19-byte chunk schedule, ProcessResult and starvation
checks, and a checked call ceiling. Compile warning-clean in ordinary builds,
expose a Clang sanitizer target, and begin with the reviewed truncated-prefix
seed. A bounded campaign is execution evidence and does not establish coverage
completion.

## DD-180: CLI exposes standalone Blocked Huffman through the public C ABI

- Date: 2026-07-17
- Status: accepted

Name the explicit command-line profile `blocked-huffman`. Configure one MiB
outer frames and 65,536-symbol entropy blocks, then obtain all workspace sizes
and create both directions exclusively through the public C API. Derive decoder
limits from this fixed local policy rather than serialized fields. Continue to
stage output in a sibling temporary file and publish only after successful
EndOfStream.

Exercise nonempty input across a frame boundary, empty input, overwrite
rejection, malformed input, and trailing bytes through the shared CLI harness.
Do not add this selector to the already versioned interoperability codec set;
that requires a separately identified manifest update. CLI publication is a
step toward benchmark and local-completion evidence, not release completion.

## DD-181: Standalone Blocked Huffman benchmark uses public profile sizing

- Date: 2026-07-17
- Status: accepted

Add `blocked-huffman` to the dependency-free benchmark selector with the same
one MiB frame and 65,536-symbol block policy as the CLI. Configure, query
workspace, and create transforms only through the public C ABI. Bound encoded
storage by the 64-byte stream prefix, one 56-byte header per frame, one 16-byte
descriptor per possible block, and raw fallback for every input byte.

Before measuring, perform an untimed complete encode/decode comparison. Time
only the single full-buffer process call for each new transform and report the
existing ratio, throughput, and caller-owned peak-workspace fields. Add a
Release smoke test, but treat its timing as path validation rather than stable
performance evidence or local-completion evidence by itself.

## DD-182: Standalone Blocked Huffman receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local implementation readiness through the public C transform path with
64-byte frames and 32-symbol entropy blocks. Cover empty input, every one-byte
value, all byte values, repetitive and patterned data, deterministic generated
data, block lengths 31/32/33, and frame lengths 63/64/65. Require byte-identical
re-encoding and exact round trips.

For one four-frame stream, compare one-byte and mixed chunk schedules against
the full-buffer representation. Corrupt the final frame sequence, truncate the
final body, and append trailing data independently; each failure must be sticky
and must publish only the first three validated frames. Successful terminal
calls must remain EndOfStream.

After this matrix passes alongside the existing format, component, profile,
C ABI, CLI, fuzz, benchmark, and documentation evidence, classify standalone
Blocked Huffman as locally implementation-complete. Do not claim release
completion without external cross-platform deterministic execution,
representative benchmark records, and final similarity review.

## DD-183: CLI exposes Adaptive Huffman FGK through the public C ABI

- Date: 2026-07-17
- Status: accepted

Name the explicit command-line profile `adaptive-huffman`. Use one MiB outer
frames and variant 1's conservative worst case of 264 bits, or 33 bytes, per
input symbol. Reserve the fixed 16-byte descriptor separately. Configure,
query workspace, create transforms, and process bytes exclusively through the
public C ABI; serialized input must never control pre-parse allocation.

Run the common file harness across a frame boundary and cover empty input,
overwrite rejection, malformed prefix, trailing bytes, exact round trip, and
temporary-file cleanup. Do not silently change an existing versioned
interoperability codec set. This CLI path is prerequisite evidence for later
benchmarking and local completion, not release completion.

## DD-184: Adaptive Huffman benchmark retains the public FGK profile

- Date: 2026-07-17
- Status: accepted

Add `adaptive-huffman` to the dependency-free benchmark with the CLI's one MiB
frame and FGK variant 1 policy. Use the conservative 33-byte payload bound per
symbol, a 16-byte descriptor per nonempty frame, and the 64-byte stream prefix
when reserving encoded output. Obtain all actual workspace extents and create
both transform directions only through the public C ABI.

Require an untimed complete round trip before measuring. Time only the process
call, retain the existing ratio and throughput definitions, and report peak
caller-owned workspace with a zero views extent. The Release smoke validates
the path and output schema; it is not a stable performance record or sufficient
local-completion evidence alone.

## DD-185: Adaptive Huffman receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local FGK readiness through the public C transform path with 64-byte
frames. Cover empty input, every one-byte symbol, all byte values, repetitive
and patterned data, deterministic generated data, and lengths 63/64/65. Require
identical re-encoding and exact round trips.

For a four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame sequence, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing FGK tree, format, frame, stream,
profile, C ABI, CLI, fuzz, benchmark, and documentation evidence, classify
Adaptive Huffman variant 1 as locally implementation-complete. External
cross-platform deterministic execution, representative benchmark records, and
the final similarity review remain release evidence.

## DD-186: CLI exposes Dynamic Range variant 1 through the public C ABI

- Date: 2026-07-17
- Status: accepted

Name the explicit command-line profile `dynamic-range`. Use one MiB outer
frames, adaptive order-0 model total 32,768, the conservative `2*n+5` payload
bound, and one 16-byte descriptor per nonempty frame. Configure, query
workspace, create transforms, and process data exclusively through the public
C ABI using local limits fixed before serialized input is inspected.

Apply the shared multi-frame file harness for exact nonempty and empty round
trips, overwrite rejection, malformed prefix, trailing bytes, and atomic
temporary-file cleanup. Keep existing versioned interoperability codec sets
unchanged. This CLI surface is prerequisite evidence for benchmarking and the
local completion audit, not release completion.

## DD-187: Dynamic Range benchmark separates symbol and frame overhead

- Date: 2026-07-17
- Status: accepted

Add `dynamic-range` to the dependency-free benchmark with one MiB frames and
model total 32,768. Reserve two payload bytes per input symbol, then add the
canonical five-byte termination, one 16-byte descriptor, and one 56-byte header
for every nonempty frame, plus the 64-byte stream prefix. Use public workspace
queries and transform factories exclusively.

Perform an untimed full round trip before measurement. Time only process calls
and retain the existing ratio, raw-byte throughput, direction-workspace, and
peak-workspace definitions. Views must remain zero. The Release smoke validates
the path and schema, but it is neither a stable performance record nor complete
local-readiness evidence.

## DD-188: Dynamic Range receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local variant 1 readiness through the public C transform path with
64-byte frames and model total 32,768. Cover empty input, every one-byte symbol,
all byte values, repetitive and patterned data, deterministic generated data,
and lengths 63/64/65. Require byte-identical re-encoding and exact round trips.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame sequence, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing model, format, frame, stream,
profile, C ABI, CLI, fuzz, benchmark, and documentation evidence, classify
Dynamic Range variant 1 as locally implementation-complete. External
cross-platform deterministic execution, representative benchmark records, and
the final similarity review remain release evidence.

## DD-189: CLI exposes scalar rANS variant 1 through the public C ABI

- Date: 2026-07-17
- Status: accepted

Name the explicit command-line profile `rans`. Use one MiB outer frames and
65,536-symbol entropy blocks, yielding at most 16 blocks per frame. Reserve one
payload byte per input symbol plus each block's eight-byte final state, and
reserve the fixed 528-byte descriptor separately for every possible block.
Configure, query workspace, create transforms, and process bytes exclusively
through the public C ABI; decoder views must be allocated from the local block
count and reported alignment before serialized input is inspected.

Apply the shared multi-frame file harness for exact nonempty and empty round
trips, overwrite rejection, malformed prefix, trailing bytes, and atomic
temporary-file cleanup. Keep existing versioned interoperability codec sets
unchanged. This CLI path is prerequisite evidence for benchmarking and local
completion, not release completion.

## DD-190: rANS benchmark retains scalar block and view policy

- Date: 2026-07-17
- Status: accepted

Add `rans` to the dependency-free benchmark with the CLI's one MiB frames and
65,536-symbol blocks. Reserve one payload byte per input symbol, eight final
state bytes and one 528-byte descriptor for each of at most 16 blocks per
frame, one 56-byte frame header, and the 64-byte stream prefix. Use public
workspace queries and transform factories exclusively, including the queried
decoder views extent and alignment.

Perform an untimed full round trip before measurement. Time only process calls
and retain the existing ratio, raw-byte throughput, direction-workspace, and
peak-workspace definitions. Report all three workspace regions. The Release
smoke validates the path and schema, but it is neither a stable performance
record nor complete local-readiness evidence.

## DD-191: rANS receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local scalar variant 1 readiness through the public C transform path with
64-byte frames and 32-symbol blocks. Cover empty input, every one-byte value,
all byte values, one-symbol and patterned data, deterministic generated data,
block lengths 31/32/33, and frame lengths 63/64/65. Require byte-identical
re-encoding and exact round trips through aligned queried views.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame sequence, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing normalization, state, format, frame,
stream, profile, C ABI, CLI, fuzz, benchmark, and documentation evidence,
classify scalar rANS variant 1 as locally implementation-complete. External
cross-platform deterministic execution, representative benchmark records, and
the final similarity review remain release evidence.

## DD-192: CLI exposes tabled tANS variant 1 through the public C ABI

- Date: 2026-07-17
- Status: accepted

Name the explicit command-line profile `tans`. Use one MiB outer frames and
65,536-symbol entropy blocks, yielding at most 16 blocks per frame. Reserve the
strict 12-bit transition bound per input symbol plus each block's two-byte
state, and reserve the fixed 528-byte descriptor separately for every possible
block. Configure, query workspace, create transforms, and process bytes
exclusively through the public C ABI; decoder views must be allocated from the
local block count and reported alignment before serialized input is inspected.

Apply the shared multi-frame file harness for exact nonempty and empty round
trips, overwrite rejection, malformed prefix, trailing bytes, and atomic
temporary-file cleanup. Keep existing versioned interoperability codec sets
unchanged. This CLI path is prerequisite evidence for benchmarking and local
completion, not release completion.

## DD-193: tANS benchmark preserves the 12-bit transition bound

- Date: 2026-07-17
- Status: accepted

Add `tans` to the dependency-free benchmark with the CLI's one MiB frames and
65,536-symbol blocks. Reserve `ceil(3*n/2)` payload bytes for the strict 12-bit
transition bound, two state bytes and one 528-byte descriptor for each of at
most 16 blocks per frame, one 56-byte frame header, and the 64-byte stream
prefix. Use public workspace queries and transform factories exclusively,
including the queried decoder views extent and alignment.

Perform an untimed full round trip before measurement. Time only process calls
and retain the existing ratio, raw-byte throughput, direction-workspace, and
peak-workspace definitions. Report all three workspace regions. The Release
smoke validates the path and schema, but it is neither a stable performance
record nor complete local-readiness evidence.

## DD-194: tANS receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local tabled variant 1 readiness through the public C transform path with
64-byte frames and 32-symbol blocks. Cover empty input, every one-byte value,
all byte values, one-symbol and patterned data, deterministic generated data,
block lengths 31/32/33, and frame lengths 63/64/65. Require byte-identical
re-encoding and exact round trips through aligned queried views.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame sequence, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing normalization, spread and transition
tables, format, frame, stream, profile, C ABI, CLI, fuzz, benchmark, and
documentation evidence, classify tabled tANS variant 1 as locally
implementation-complete. External cross-platform deterministic execution,
representative benchmark records, and the final similarity review remain
release evidence.

## DD-195: Standalone LZ77 receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local entropy-None LZ77 variant 1 readiness through the public C
transform path with 64-byte frames. Cover empty input, every one-byte value,
all byte values, repetitive and patterned data, deterministic generated data,
and frame lengths 63, 64, and 65. Require byte-identical re-encoding and exact
round trips.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame header, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing token, format, frame, stream,
profile, C ABI, CLI, fuzz, benchmark, and documentation evidence, classify
standalone LZ77 variant 1 as locally implementation-complete. External
cross-platform deterministic execution, representative benchmark records, and
the final similarity review remain release evidence.

## DD-196: Standalone LZSS receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local entropy-None LZSS variant 1 readiness through the public C
transform path with 64-byte frames. Cover empty input, every one-byte value,
all byte values, repetitive and patterned data, deterministic generated data,
and frame lengths 63, 64, and 65. Require byte-identical re-encoding and exact
round trips through the variable-token profile.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame header, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing literal/match cost rule, format,
frame, stream, profile, C ABI, CLI, fuzz, benchmark, and documentation evidence,
classify standalone LZSS variant 1 as locally implementation-complete. External
cross-platform deterministic execution, representative benchmark records, and
the final similarity review remain release evidence.

## DD-197: Standalone LZ78 receives a public-ABI completion matrix

- Date: 2026-07-17
- Status: accepted

Audit local entropy-None LZ78 variant 1 readiness through the public C
transform path with 64-byte frames and at most 64 phrase entries per frame.
Cover empty input, every one-byte value, all byte values, repetitive and
patterned data, deterministic generated data, and frame lengths 63, 64, and 65.
Require byte-identical re-encoding and exact round trips through queried,
explicitly aligned phrase-table views.
The empty encoder must query zero view bytes; non-empty encoders and decoders
must query nonzero view bytes with a nonzero alignment.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame header, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

After this matrix passes with the existing phrase-index format, validators,
frame and stream paths, profile, C ABI, CLI, fuzz, benchmark, and documentation
evidence, classify standalone LZ78 variant 1 as locally
implementation-complete. External cross-platform deterministic execution,
representative benchmark records, and the final similarity review remain
release evidence.

## DD-198: LZW local completion is re-audited through the public ABI

- Date: 2026-07-17
- Status: accepted

Retain the original internal reference and streaming completion matrix, and add
a supplemental public-C-ABI matrix using 64-byte frames, maximum code width 9,
and a 256-entry local phrase ceiling. Cover empty input, every one-byte value,
all byte values, repetitive and patterned data, deterministic generated data,
and frame lengths 63, 64, and 65. Require byte-identical re-encoding and exact
round trips through queried aligned views. Zero- and one-byte encoders query no
phrase entries; larger encoders and decoders query nonzero views.

For one four-frame stream, compare one-byte and mixed input/output chunking with
the full-buffer representation. Corrupt the final frame header, truncate its
payload, and append trailing data independently. Each error must be sticky and
publish only the first three validated frames; successful terminal calls must
remain EndOfStream.

This matrix strengthens DD-112 without changing the LZW variant or stream
format. External cross-platform deterministic execution, representative
benchmark records, and the final similarity review remain release evidence.

## DD-199: LZD completion adds malformed and terminal public-ABI evidence

- Date: 2026-07-17
- Status: accepted

Retain the existing LZD public-C-ABI data and chunking matrix. Require every
successful transform to return repeatable EndOfStream with zero consumption and
production on later calls.

For the existing 64-byte-frame, 32-entry profile, encode one 193-byte four-frame
stream. Corrupt the final frame header, truncate its reference-pair payload, and
append trailing data independently. Each failure must be sticky, publish only
the first three validated frames, and preserve the final output sentinel.

This strengthens DD-126 without changing Lempel-Ziv Double parsing, format, or
workspace policy. External cross-platform deterministic execution,
representative benchmark records, and the final similarity review remain
release evidence.

## DD-200: LZMW completion adds malformed and terminal public-ABI evidence

- Date: 2026-07-17
- Status: accepted

Retain the existing LZMW public-C-ABI data and chunking matrix. Require every
successful transform to return repeatable EndOfStream with zero consumption and
production on later calls.

For the existing 64-byte-frame, 32-entry profile, encode one 193-byte four-frame
stream. Corrupt the final frame header, truncate its fixed-reference payload,
and append trailing data independently. Each failure must be sticky, publish
only the first three validated frames, and preserve the final output sentinel.

This strengthens DD-139 without changing LZMW parsing, format, or workspace
policy. External cross-platform deterministic execution, representative
benchmark records, and the final similarity review remain release evidence.

## DD-201: Baseline readiness separates local codec completion from release evidence

- Date: 2026-07-17
- Status: accepted

Maintain `docs/baseline-readiness.md` as the cross-profile status index. Mark
all six required dictionary codecs and five required entropy codecs locally
ready only because each now has exact format and validation, bounded one-shot
and streaming paths, public C ABI, CLI, benchmark, decoder fuzz boundary, and a
public-ABI completion matrix. Keep the composed LZ77 plus Blocked Huffman and
checksum-raw profiles in a separate additional-profile table.

Do not infer release completion from this local status. Interoperability schema
2 and codec set `marc-cli-v2` remain frozen at eight archives: six standalone
dictionary profiles, LZ77 plus Blocked Huffman, and checksum-raw. The five
standalone entropy profiles require a new schema and codec-set identifier.
External CI artifacts, additional platform and architecture checks,
representative measurements, longer sanitizer campaigns, and final similarity
review remain release evidence.

Classify unknown-size input, allocator callbacks, authentication, archive
metadata, solid grouping, BWT-family transforms, and additional composed
profiles as future extensions rather than baseline failures.

## DD-202: Interoperability schema 3 appends the entropy profiles

- Date: 2026-07-17
- Status: accepted

Define schema 3 with codec set `marc-cli-v3`. Preserve the exact schema-2 order
as its prefix, then append `blocked-huffman`, `adaptive-huffman`,
`dynamic-range`, `rans`, and `tans`, for thirteen archives total. The generator
emits schema 3 by default.

Keep schema 1 frozen at its seven profiles without a codec-set field. Keep
schema 2 frozen at `marc-cli-v2` and its eight profiles. The verifier must select
one exact list from the manifest version and reject missing, extra, duplicate,
or mismatched-set entries.

Register a PowerShell compatibility test that generates schema 3, verifies it,
derives exact schema-2 and schema-1 bundles, and verifies both legacy forms.
CI artifact names remain unchanged; their manifest self-identifies schema 3.

## DD-203: Separate implementation records from reader-facing documentation

- Date: 2026-07-17
- Status: accepted

Keep public format, API, architecture, validation, and project-operation
documents directly under `docs/`. Place chronological design decisions,
independent-implementation provenance, implementation references, and
test-vector construction records under `docs/implementation/`. Provide an
index at each level and label the record set as development evidence rather
than additional public API or format specification.

Preserve the same hierarchy when installing documentation. Install the project
README beside the package license and install the complete `docs/` tree below
the package documentation directory so relative links remain valid.

## DD-204: Validate documentation topology with CMake

- Date: 2026-07-17
- Status: accepted

Register a script-mode CMake test that requires the reader-facing index and all
implementation-record files, rejects the four former root-level record paths,
and resolves every relative Markdown link and image target. Use only CMake so
the check runs through the same CTest entry on Windows and non-Windows hosts
without introducing another documentation-tool dependency.

Keep asset relocation separate from this validator. The test describes the
selected topology and link integrity but does not prescribe where a valid
linked image must live.

## DD-205: Keep the repository README as a concise entry point

- Date: 2026-07-17
- Status: accepted

Keep one default and one explicitly selected command-line example in the root
README. Move the complete profile matrix, exact CLI behavior, and exit codes to
`docs/cli.md`, linked from both the root README and documentation index. This
keeps the GitHub landing page focused while retaining an installed, versioned
reference for every public CLI profile.

## DD-206: The C ABI enumerates validated profiles, not arbitrary pairings

- Date: 2026-07-17
- Status: accepted

Treat each public C factory as selection of one complete stream profile. A
standalone dictionary factory binds entropy None, and a standalone entropy
factory binds dictionary None. Retain LZ77 plus Blocked Huffman as the sole
baseline dictionary-plus-entropy profile because it is the completed
representative composition, not because the byte-stream architecture prevents
other pairings.

Do not infer a supported cross product from the presence of individual
factories. Every additional pairing needs an additive named profile with exact
format parameters, bounded workspace derivation, transactional validation,
streaming behavior, public ABI coverage, malformed tests, fuzzing, benchmarks,
interoperability policy, and provenance before publication.

## DD-207: Publish one contributor contract before external collaboration

- Date: 2026-07-17
- Status: accepted

Add a root `CONTRIBUTING.md` that routes contributors to the normative format,
architecture, C API, design decisions, references, and provenance record. State
the independent-implementation hygiene, documentation-first algorithm
sequence, build/test entry points, and permanent-regression policy without
claiming legal guarantees.

Include a composed-profile admission checklist so reusable internal parts do
not imply an automatically supported public cross product. Install the
contributor contract beside the project README and include it in portable
documentation link validation. Install `AGENTS.md` beside it so the linked
complete repository contract remains available in packaged documentation.

## DD-208: Distinguish unpublished compositions from incompatibility

- Date: 2026-07-17
- Status: accepted

Publish `docs/composition.md` with the complete baseline dictionary/entropy
matrix. Use named cells only for currently supported C ABI and CLI profiles.
Label all remaining byte-boundary pairings as candidates, explicitly meaning
that their components exist but their combined format and guarantees do not.
Do not present candidate cells as usable streams or release commitments.

Document a staged generator path: introduce a reviewed declarative internal
profile description, reproduce the existing combined profile byte for byte,
add one selected second composition, and generate only repetitive registries,
adapters, tests, benchmarks, interoperability entries, and documentation.
Worst-case formulas, workspace partitions, validation commit points, and
boundary semantics remain reviewed inputs rather than generated assumptions.

## DD-209: The second composition starts with LZSS plus Blocked Huffman

- Date: 2026-07-17
- Status: accepted

Select LZSS variant 1 plus Blocked Huffman variant 1 as the second composed
pipeline. It exercises the same byte-stream seam with two-byte Literal and
nine-byte Match tokens instead of LZ77's fixed 16-byte records, providing an
early check that the composition architecture is generic.

Fix the exact frame body and hand vector before exposing a public profile. The
first executable slice is a strict complete-frame validator: parse the generic
header, control and decode the entropy regions into caller-owned staging, then
validate the entire LZSS token stream and declared raw size. Check descriptors,
payload, staging, and block views under one aggregate workspace limit. Do not
publish raw bytes, a C ABI factory, or a CLI name from this slice.

Continue forward from this validation nucleus with encoder planning, raw
commit, complete-stream and streaming controllers, public adapters, malformed
and fuzz coverage, benchmark registration, and interoperability evidence.

## DD-210: LZSS composition uses token-first exact frame planning

- Date: 2026-07-17
- Status: accepted

Plan the LZSS token stream before any combined frame byte is published because
its two-byte Literal and nine-byte Match forms prevent deriving the dictionary
extent directly from raw size. Encode the planned tokens once into caller-owned
staging, then run the bounded Blocked Huffman planner over that exact extent.

Construct and validate the generic frame header from the resulting dictionary,
descriptor, payload, and block extents. The frame encoder repeats the
deterministic entropy traversal only after verifying complete serialized output
capacity. A short dictionary staging span or serialized destination must leave
that destination unchanged. Retain the frame-only internal status until raw
commit and stream-level behavior are implemented and tested.

## DD-211: LZSS composition publishes raw only after whole-frame validation

- Date: 2026-07-18
- Status: accepted

Implement raw frame decoding as a narrow commit stage over the DD-209 strict
validator. First parse and entropy-decode the entire frame into caller-owned
dictionary staging, and validate the complete variable-length LZSS token stream
against the declared raw extent. Check raw destination capacity only after
those operations succeed.

Pass only the validated staging extent to the standalone transactional LZSS
decoder. A malformed descriptor, entropy payload, token tag, match reference,
derived size, or short raw destination must not publish a raw prefix. Exercise
both raw Blocked Huffman representation and canonical Huffman representation,
including overlapping LZSS match reconstruction.

## DD-212: LZSS composition validates the complete stream before raw commit

- Date: 2026-07-18
- Status: accepted

Compose the version 1.0 stream header, one canonical 16-byte LZSS parameter
region, and consecutive DD-209 frames into a known-size complete stream. Empty
input is the 80-byte prefix only. Plan all frames before writing the prefix so
short serialized output remains atomic and the final size is exact.

Decode in two passes. Parse stream configuration into local objects, validate
every frame and the exact terminal serialized extent without raw output, then
repeat the bounded frame traversal to publish raw bytes. Publish parsed stream
and parameter outputs only after the second pass succeeds. This makes later
frame corruption whole-stream atomic while reusing storage sized for only the
largest frame and its entropy block views.

## DD-213: LZSS combined incremental encoding preserves exact stream bytes

- Date: 2026-07-18
- Status: accepted

Implement the `ProcessResult` encoder with caller-owned raw-frame, LZSS-token,
and serialized-frame workspaces. Size the worst-case token staging as twice the
largest raw frame because every input byte may become a two-byte Literal. Before
preparing a frame, check raw bytes plus actual token bytes plus exact serialized
frame bytes as one aggregate internal-buffer bound.

Drain the canonical 80-byte prefix and each complete frame independently
through partial output buffers. Do not let nonterminal `Flush` close a partial
frame. Latch `EndInput` while draining and report `EndOfStream` only after the
final frame byte is emitted. Require byte identity with DD-212 for all chunking
and retain a stable ended response on repeated calls.

## DD-214: LZSS combined incremental decoding commits complete frames

- Date: 2026-07-18
- Status: accepted

Collect the canonical prefix, one generic frame header, and one complete frame
body using caller-owned serialized-frame storage. Before raw drain, enforce the
aggregate serialized frame, LZSS staging, raw staging, and typed block-view
bound; entropy-decode and validate all tokens; and reconstruct the entire raw
frame into staging.

Commit only that validated frame through partial output buffers. Preserve an
`EndInput` indication while draining. If it arrived after a nonfinal frame,
report truncation after the remaining validated raw bytes drain. This differs
intentionally from the one-shot whole-stream-atomic decoder: earlier complete
frames may be visible, but a malformed frame contributes no raw prefix.

## DD-215: LZSS combined profiles expose trusted workspace bounds

- Date: 2026-07-18
- Status: accepted

Normalize the known-size LZSS variant 1 plus Blocked Huffman variant 1
configuration before constructing a transform. For largest raw-frame extent
`F`, reserve exactly `2F` token bytes because the all-Literal representation is
the LZSS worst case. For entropy block size `E`, reserve
`56 + 16 * ceil(2F/E) + 2F` serialized frame bytes: one generic frame header,
one descriptor for every worst-case token block, and the raw entropy fallback.
The empty stream requires no frame-local workspace.

Check every multiplication, addition, block count, region limit, and the
aggregate `F + 2F + serialized-frame` encoder workspace. Calculate decoder
requirements only from trusted local limits: `56 + max internal buffered` for
serialized collection, the configured dictionary-serialized and frame maxima
for the two byte-staging regions, and the local block-count maximum for aligned
views. Do not inspect an untrusted stream to answer the decoder query. Keep
this contract internal until the separate public-profile admission steps are
complete.

## DD-216: LZSS composition uses a dedicated size-tagged C factory

- Date: 2026-07-18
- Status: accepted

Expose LZSS variant 1 plus Blocked Huffman variant 1 through additive
`marc_lzss_blocked_huffman_*` configuration, workspace-query, and creation
functions. Keep ABI version 1 because existing structures and entry points are
unchanged; the new configuration is independently size-tagged and requires all
reserved fields to remain zero.

Retain the common three-workspace transform lifecycle. Concatenate token and
serialized-frame staging in encoder secondary storage, concatenate token and
raw staging in decoder secondary storage, and use the aligned views region only
for decoder entropy block records. Recalculate and partition every extent from
validated configuration during creation. Reject short, null-inconsistent, or
misaligned caller regions before constructing the opaque transform. Do not add
a CLI selector or claim full profile readiness in this step.

## DD-217: The LZSS composition receives an explicit CLI profile

- Date: 2026-07-18
- Status: accepted

Name the public command-line profile `lzss-blocked-huffman`. Configure it only
through `marc_lzss_blocked_huffman_*`, using one-MiB raw frames, 65,536-symbol
entropy blocks, the default LZSS variant parameters, and checked local limits
derived from the two-byte-per-raw-byte token worst case. Do not let file I/O
construct an internal C++ transform or reinterpret stream configuration.

Use the common atomic-file adapter. Exercise nonempty and empty round trips,
existing-output refusal, malformed-stream rejection, and trailing-data
rejection. A failed operation must remove its temporary destination even though
the incremental decoder may have internally drained earlier validated frames.
CLI availability does not imply benchmark, fuzz, completion-matrix, or
interoperability readiness.

## DD-218: The LZSS composition benchmark uses public profile sizing

- Date: 2026-07-18
- Status: accepted

Add `lzss-blocked-huffman` to the opt-in Release benchmark using only its
public C configuration, workspace query, transform creation, processing, and
destruction functions. Match the CLI's one-MiB frame, 65,536-symbol entropy
block, and LZSS defaults. Bound output by the 80-byte parameterized prefix,
twice the input size, every 56-byte frame header, and 32 worst-case 16-byte
descriptors per full frame.

Verify a complete round trip before timing. Exclude allocation, construction,
destruction, file I/O, and verification from the timed interval. Report the
complete-stream ratio, direction-specific throughput, each queried workspace
region, and the larger direction's primary-plus-secondary-plus-views sum. Keep
measured speed and ratio descriptive; the smoke test requires successful
execution and stable report structure, not a performance threshold.

## DD-219: Combined LZSS fuzzing is fixed-workspace and dual-decoder

- Date: 2026-07-18
- Status: accepted

Add a dedicated libFuzzer boundary for LZSS variant 1 plus Blocked Huffman
variant 1. Feed every case to both the strict whole-stream-atomic decoder and
the incremental frame-committing decoder. Truncate input to 8 KiB and use only
fixed caller-owned arrays for 4 KiB total output, one 1-KiB frame, 4 KiB token
staging, and eight entropy views. Include all four frame-local roles in the
aggregate internal limit.

Derive nonzero input and output chunk sizes from bounded input bytes, but cap
the entire incremental schedule independently at input maximum plus output
maximum plus 32 calls. Abort on an invalid `ProcessResult`, impossible
starvation status, or exhaustion of that ceiling. Retain only a reviewed
five-byte truncated-magic seed in source control. Permanently test all
canonical truncations, extreme frame lengths, and an invalid LZSS tag exposed
only after successful entropy decoding.

## DD-220: Public-profile readiness is an evidence matrix

- Date: 2026-07-18
- Status: accepted

Classify a public profile as locally ready only when its exact format and
validator, bounded streaming implementation, C ABI, CLI, benchmark, bounded
decoder fuzz target, and public-ABI completion test all exist. Record these as
separate columns so an implemented component cannot be mistaken for a
complete callable profile.

The completion test must exercise required binary data classes,
deterministic output, one-byte and mixed input/output chunking, stable repeated
end-of-stream behavior, sticky malformed-stream errors, and frame-atomic
rejection of corrupt, truncated, and trailing final-frame input. Keep external
interoperability as a separate evidence column. A profile may be locally ready
while awaiting a new, immutable interoperability bundle schema.

## DD-221: CI separates implementation evidence from package consumption

- Date: 2026-07-18
- Status: accepted

Enable `MARC_BUILD_BENCHMARKS` explicitly in the clean Windows and Ubuntu
implementation jobs. This makes every public benchmark adapter compile and
runs its non-threshold smoke test alongside the normal suite instead of relying
on a developer build cache.

For the installed-package matrix, explicitly disable tests, examples, tools,
and benchmarks. Build and install exactly one library linkage, configure a
separate pure-C consumer through `find_package(marc CONFIG REQUIRED)`, and run
it against the installed tree. Test shared-only and static-only packages on
both Windows and Ubuntu. Keep external interoperability artifact generation in
the implementation jobs, where the tested CLI and complete codec suite exist.

## DD-222: Similarity review does not inspect external codec source

- Date: 2026-07-18
- Status: accepted

Perform the pre-publication review over tracked first-party source, tests,
headers, build files, public documentation, provenance entries, and license
markers. Treat the pinned GoogleTest submodule as separately licensed test
infrastructure. Check for unexplained third-party notices or distinctive names,
terminology inconsistent with the selected variants, stale future-work claims,
and language that overstates legal, security, compatibility, or release
guarantees.

Do not compare marc source against external codec implementations as a
similarity-search technique. That would conflict with the repository's
independent-implementation boundary. Record only whether first-party expression
is accounted for by marc's specifications, decisions, references, and prior
provenance. State the review's limits and make no non-infringement guarantee.

## DD-223: LZ78 is the first typed-workspace Blocked Huffman composition

- Date: 2026-07-18
- Status: accepted

Reserve `lz78-blocked-huffman` for dictionary ID 3 variant 1 plus entropy ID 2
variant 1. Preserve the standalone 16-byte LZ78 parameter region, empty entropy
parameters, fixed eight-byte token serialization, and ordinary version 1.0
frame header. Entropy blocks count serialized token bytes, never raw bytes or
phrase entries, and both codec states reset at every outer frame.

Bound token staging by eight times the raw frame size and phrase entries by the
lesser of token count and configured maximum. Entropy-decode into staging,
validate the complete LZ78 token stream and phrase graph, and only then decode
to raw output. The encoder likewise fixes its LZ78 parse before planning
Blocked Huffman. This preserves deterministic bytes and frame-atomic failure.

Unlike LZ77 and LZSS, LZ78 needs an aligned private phrase table in both
directions, while combined decoding also needs aligned entropy block views.
Retain the three-region public workspace convention, but require one checked
opaque views partition for both private types. Specification does not publish
the C factory, CLI name, or compatibility promise; those follow only after the
normal implementation and evidence sequence.

## DD-224: Combined LZ78 decoding admits both typed workspaces atomically

- Date: 2026-07-18
- Status: accepted

Make the first implementation step a complete-frame validator and decoder, not
a public factory. Accept separate typed spans for Blocked Huffman block views
and LZ78 phrase entries at the internal boundary. Check both capacities and the
token staging capacity before entropy decoding, and count descriptors,
compressed payload, token staging, block views, and phrase entries in the
aggregate buffered-memory limit.

Decode entropy only after the header, exact serialized extent, and all caller
capacities pass. Validate the resulting fixed-width token stream and build the
phrase graph before checking raw output capacity or invoking LZ78 expansion.
This makes malformed entropy metadata, invalid phrase references, short typed
workspaces, and short raw output frame-atomic. Defer the single opaque C ABI
partition helper until profile sizing and public construction are implemented;
it must reproduce these same two typed extents with checked alignment.

## DD-225: Combined LZ78 encoding freezes tokens before entropy planning

- Date: 2026-07-18
- Status: accepted

Require the frame planner to validate the pipeline and LZ78 parameters, admit
the complete caller-owned `Lz78EncoderEntry` table, and plan the deterministic
LZ78 parse before touching token staging. Count the typed encoder table plus
the exact token staging extent against the aggregate buffered-memory limit.
Encode the canonical token stream once into staging, then plan Blocked Huffman
over precisely those bytes.

The frame encoder repeats that complete plan before checking serialized output
capacity, writes the generic header, and entropy-encodes the already fixed
staging bytes. Expected workspace, limit, frame-extent, and output-capacity
failures therefore publish no serialized byte. Require the specified 80-byte
raw-block vector, multi-block deterministic output, canonical-Huffman
selection, and round-trip decoding before building streaming or public profile
layers.

## DD-226: LZ78 composition profile owns one checked opaque typed layout

- Date: 2026-07-18
- Status: accepted

Retain the common public primary/secondary/views workspace shape. Profile
sizing reports exact frame input, token staging, encoded frame, decoded frame,
typed record counts, opaque byte count, and maximum alignment. The encoder
views region contains only `Lz78EncoderEntry`. The decoder views region contains
the complete `BlockedHuffmanBlockView` array first, checked padding up to
`alignof(Lz78PhraseEntry)`, and then the phrase array.

Do not let a C adapter duplicate this pointer arithmetic. Provide internal
partition helpers that recompute multiplication, padding, total size, and
alignment; reject altered requirements, short storage, and a misaligned base
before returning typed spans. Empty encoding requires zero opaque bytes and
alignment one. The profile remains non-callable until streaming transforms and
public construction use these helpers.

## DD-227: LZ78 composition streams only validated complete frames

- Date: 2026-07-18
- Status: accepted

Use the common composed-profile state machine for the LZ78 plus Blocked
Huffman incremental encoder and decoder. The encoder collects at most one raw
frame, fixes its complete LZ78 token stream in bounded staging, prepares the
complete encoded frame, and then drains it independently of later input. The
decoder collects one complete serialized frame, entropy-decodes into private
staging, validates the complete LZ78 phrase graph and exact raw extent, and
only then makes that frame available for incremental output.

Both directions consume the typed spans returned by the DD-226 partition
helpers. Count only the entries required by the current frame, together with
serialized, staging, and raw storage, against the aggregate buffered-memory
limit. Preserve `EndInput` while a nonfinal decoded frame is draining, reject
truncation or trailing bytes, make terminal errors sticky, and return stable
end-of-stream on repeated calls. `ResetBlock` remains unsupported because
caller-selected dictionary resets require a separately specified frame-control
policy.

## DD-228: LZ78 composition publishes one opaque three-region C factory

- Date: 2026-07-18
- Status: accepted

Add `marc_lz78_blocked_huffman_config`, requirements, and creation functions
without exposing either private LZ78 record type or the entropy block-view
type. Preserve the existing primary/secondary/views ownership model. The
secondary region concatenates dictionary staging and frame storage; the views
region uses the exact size and maximum alignment calculated by the profile.

Creation must repeat profile sizing and invoke the DD-226 partition helpers
over the admitted opaque extent. It must not duplicate typed pointer arithmetic
in the C adapter. Reject null, short, reserved-field, and misaligned inputs
before publishing a transform. Admission at this step covers the public C
factory and exact round trip only; CLI naming, completion matrix, fuzz target,
benchmark, and interoperability publication remain independent gates.

## DD-229: LZ78 composition completion is proved through the public C ABI

- Date: 2026-07-18
- Status: accepted

Drive the composed profile exclusively through its public configuration,
workspace query, creation, process, and destruction functions. Cover empty
input, every one-byte value, all byte values in sequence, long repetition,
multi-byte patterns, deterministic pseudo-random data, dictionary/frame
boundaries, and multiple frames. Repeat encoding and require byte identity.

Require the same encoded stream under one-byte and mixed input/output chunk
schedules. On the fourth frame, independently test header corruption, payload
truncation, and strict trailing data. Only the preceding three complete frames
may be published, the final output byte must remain untouched, and repeated
calls must return the identical positioned terminal error. This admits the C
ABI completion column only; it does not imply CLI, fuzz, benchmark, or
interoperability completion.

## DD-230: LZ78 composition fuzzing fixes both typed workspace bounds

- Date: 2026-07-18
- Status: accepted

Add a dedicated libFuzzer/ASan/UBSan target for the public incremental LZ78
plus Blocked Huffman decoder. Truncate supplied cases to 8 KiB. Permit at most
4 KiB of raw output and token staging, one 1 KiB frame, eight entropy blocks,
and 512 LZ78 phrase records. Count the encoded frame, token staging, decoded
frame, block views, and phrase entries in one fixed aggregate limit.

Derive bounded input and output chunk sizes from the current bytes and stop
after a fixed call ceiling. Abort on an invalid process result, zero-progress
`Progress`, input exhaustion reported as `NeedInput`, or exhaustion of the
call ceiling. Treat output-limit `NeedOutput` as a bounded terminal condition.
Keep a compile-smoke target in ordinary builds and a minimal truncated-magic
seed corpus. CLI, benchmark, and interoperability remain separate gates.

## DD-231: LZ78 composition CLI is a fixed C-ABI adapter

- Date: 2026-07-18
- Status: accepted

Publish `lz78-blocked-huffman` as a command-line selector without adding a
second construction path. Use one-MiB raw frames, 65,536-symbol entropy blocks,
the exact eight-byte-per-raw-byte LZ78 token bound, at most 128 entropy blocks,
and at most 65,536 phrase entries. Fix the local aggregate-buffer policy at
64 MiB, then obtain the actual primary, secondary, and aligned views extents
from `marc_lz78_blocked_huffman_workspace_requirements`.

Require ordinary, empty, malformed, strict-trailing, existing-destination, and
temporary-output cleanup tests through the same CLI round-trip script used by
the other public profiles. The adapter must call only the public C factory;
private typed layout and codec objects remain inaccessible. CLI admission does
not imply benchmark or interoperability completion.

## DD-232: LZ78 composition benchmark uses the public fixed profile

- Date: 2026-07-18
- Status: accepted

Add `lz78-blocked-huffman` to the common benchmark adapter with the identical
one-MiB frame, 65,536-symbol entropy block, eight-byte token bound, 65,536-entry
dictionary limit, 128-block limit, and 64-MiB aggregate-buffer policy used by
the CLI. Obtain primary, secondary, and aligned views requirements solely from
the public C ABI.

Before timing, require an exact full-stream round trip. Report the complete
encoded-to-input ratio, encode and decode throughput over raw input bytes, each
directional workspace region, and the larger combined caller-owned workspace.
Keep allocation, construction, file I/O, and verification outside the timed
region, consistently with the existing benchmark contract. A benchmark smoke
test proves adapter availability but is not a stable performance assertion.
Benchmark admission does not imply interoperability completion.

## DD-233: Interoperability schema 4 appends completed compositions

- Date: 2026-07-18
- Status: accepted

Define schema 4 with codec set `marc-cli-v4`. Preserve the exact thirteen-entry
schema-3 order, then append `lzss-blocked-huffman` and
`lz78-blocked-huffman`, for fifteen archives total. The generator emits schema
4 by default while artifact names remain unchanged and manifests self-identify
their schema and exact codec set.

Keep schemas 1, 2, and 3 frozen at their existing seven, eight, and thirteen
profiles. The verifier must accept each exact historical set and reject
mismatched codec-set identifiers, missing, extra, or duplicate profiles. The
compatibility test generates schema 4, verifies it, derives each earlier bundle
by filtering only its versioned list, and verifies all four forms with the same
public CLI. Local cross-compiler agreement is necessary evidence; external
cross-platform execution remains release evidence after publication.

## DD-234: LZW composition entropizes canonical packed bytes

- Date: 2026-07-18
- Status: accepted

Reserve `lzw-blocked-huffman` for dictionary ID 4 variant 1 plus entropy ID 2
variant 1. Preserve the standalone 16-byte LZW parameter region, empty entropy
parameters, LSB-first variable-width code schedule, and final LZW zero padding.
Blocked Huffman consumes the resulting packed bytes without interpreting code
boundaries; both codec states reset at every outer frame.

For raw frame size `F`, maximum code width `W`, and entropy block size `E`, use
`S = ceil(F*W/8)` as the checked dictionary staging bound and `ceil(S/E)` as
the block-count bound. Encoding stages canonical code bytes
before entropy planning. Decoding stages complete entropy output, then requires
the ordinary LZW validator to accept width transitions, dictionary references,
`KwKwK`, padding, and exact raw size before publication.

Retain the three-region caller-workspace model for future admission. Encoding
needs aligned LZW encoder entries; decoding needs Blocked Huffman block views
plus a separately aligned LZW phrase array. Require checked partition helpers
before constructing either transform. This decision specifies bytes and a
reserved name only; it does not publish a factory, CLI selector, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-235: LZW composition validates entropy before packed codes

- Date: 2026-07-18
- Status: accepted

Implement the decoder-side frame boundary first. Parse and validate the generic
header and complete extent, require caller-owned Blocked Huffman views, packed
LZW staging, and LZW phrase entries, and count descriptors, payload, staging,
views, and phrase records against the aggregate buffered-byte limit before
entropy output.

Decode Blocked Huffman into staging, then invoke the existing LZW validator with
the declared raw frame size. Only after it accepts code-width changes,
references, `KwKwK`, exact completion, trailing data, and zero padding may the
decoder check raw output capacity and invoke transactional LZW reconstruction.
No error may publish a raw byte.

Require the 74-byte hand vector, every strict truncation, trailing data,
independent workspace shortages, aggregate-limit accounting, malformed entropy
metadata before staging writes, nonzero LZW padding after valid entropy decode,
and a 9-to-10-bit width-change vector split across thirty ten-byte entropy
blocks. This admits only the frame validator/decoder boundary; encoder, profile,
streaming, C ABI, and public evidence remain separate steps.

## DD-236: LZW composition plans from finalized packed bytes

- Date: 2026-07-18
- Status: accepted

Implement the frame planner and encoder as a two-stage transaction. First run
the standalone LZW planner with caller-owned aligned encoder entries, encode
the complete variable-width code stream into caller-owned staging, and retain
its final zero-padded byte. Only then plan Blocked Huffman over precisely that
staged span. The generic frame header records the actual packed-byte size, not
the conservative `ceil(F*W/8)` allocation bound.

Count the encoder-entry bytes and actual packed staging bytes together against
the aggregate buffered-byte limit. Reject missing typed workspace or staging
before entropy planning, and complete all planning before writing any byte of
the serialized frame. A short final output therefore leaves the destination
untouched. Require byte identity with the 74-byte `A` vector, deterministic
multi-block encoding and round trip, independent workspace failures, aggregate
limit enforcement, empty-frame rejection, and frame-extent enforcement. This
admits the internal frame encoder only; profile sizing, streaming, C ABI, CLI,
benchmark, fuzzing, and interoperability remain separate steps.

## DD-237: LZW composition sizes and partitions typed workspace

- Date: 2026-07-18
- Status: accepted

Define an internal fixed-profile constructor for LZW variant 1 plus Blocked
Huffman variant 1. For the largest raw frame `F`, maximum LZW width `W`, and
entropy block size `E`, reserve `ceil(F*W/8)` packed staging bytes,
`ceil(staging/E)` descriptors, staging-sized raw entropy payload capacity, and
at most `min(F-1, 2^W-256)` LZW encoder entries. Count frame input, staging,
worst-case serialized frame, and typed entries together against the aggregate
buffer limit before admitting the profile.

Derive decoder storage conservatively from local limits. The opaque typed
region starts with Blocked Huffman block views, aligns the following LZW phrase
table independently, and records the exact phrase offset, total extent, and
maximum alignment. Partition helpers must recompute and compare this layout,
reject short or misaligned storage, and publish no typed span on failure.
Require exact worst-case arithmetic, short-final-frame and empty-stream cases,
block and aggregate limits, both partitions, tampered layout metadata, stable
error mapping, and the minimum 9-bit LZW dictionary capacity. This establishes
internal sizing and layout only; streaming, C ABI, CLI, benchmarks, fuzzing,
completion evidence, and interoperability remain separate admissions.

## DD-238: LZW composition streams only complete validated frames

- Date: 2026-07-18
- Status: accepted

Implement bounded streaming transforms over the internal profile storage. The
encoder emits the stream header and 16-byte LZW parameters first, collects one
raw frame, fixes its packed LZW bytes and complete Blocked Huffman frame in
caller-owned buffers, then drains that immutable frame. Output chunking must
not affect any encoded byte.

The decoder collects and validates the same prefix, then collects each generic
frame header and its exact descriptor-plus-payload extent. Check the required
block views, packed staging, decoded frame, LZW phrases, and their aggregate
bytes before collecting the body. Entropy decode, LZW validation, and raw
reconstruction all finish in private frame storage before any byte of that
frame is published. A malformed later frame may not retract earlier committed
frames and becomes a stable positioned error. Require direct construction from
profile partitions, one-byte input and output, frame-oracle byte identity,
later-frame padding corruption, workspace shortage, truncation, unsupported
reset, empty input, premature finish, and repeated ended/error behavior. This
establishes internal streaming only; C ABI, CLI, benchmark, fuzzing, completion
evidence, and interoperability remain separate admissions.

## DD-239: LZW composition enters the public C ABI through one factory

- Date: 2026-07-18
- Status: accepted

Add `marc_lzw_blocked_huffman_config` with known original size, frame size,
entropy block size, maximum LZW code width, and the complete relevant decoder
limits. Keep the common three-workspace ABI: primary is raw-frame input or
serialized-frame input; secondary is packed LZW staging followed by encoded or
decoded frame storage; aligned views contain encoder entries or the decoder's
block-view/padding/phrase layout.

The requirements function must use the internal profile calculators and expose
only byte counts and alignment. The factory must repeat profile construction,
partition the opaque view region through checked helpers, and instantiate the
existing streaming transforms. Reject wrong struct metadata, reserved fields,
short regions, and misalignment before publishing a handle. Require a pure-C
five-byte, three-frame round trip whose 304-byte output is fixed by the existing
frame oracle. This admits the public factory only; completion matrix, fuzzing,
CLI, benchmark, and interoperability remain separate evidence steps.

## DD-240: LZW composition completion is proved through the public C ABI

- Date: 2026-07-18
- Status: accepted

Drive the composed profile only through its public configuration, requirements,
creation, processing, and destruction functions. Cover empty input, every
one-byte value, all byte values, repetition, binary patterns, deterministic
generated data, frame boundaries, and multiple frames. Require byte-identical
encoding across repeated, one-byte, and mixed-chunk schedules.

Corrupt, truncate, or append data only at the fourth frame and require exactly
the first three frames to remain committed, the final destination byte to stay
untouched, and the positioned terminal error to be sticky. Define an encoder
with zero dictionary entries to require zero view bytes and neutral alignment
one; this makes empty and one-byte construction agree with the checked
partition contract. This admits only completion evidence. Fuzzing, CLI,
benchmark, and interoperability remain independent gates.

## DD-241: LZW composition fuzzing bounds packed codes and phrase state

- Date: 2026-07-18
- Status: accepted

Add a dedicated libFuzzer/ASan/UBSan target around the internal incremental LZW
plus Blocked Huffman decoder. Truncate cases to 8 KiB and permit at most 4 KiB
of raw output and packed-code staging, one 1 KiB frame, eight entropy blocks,
and 4,096 local dictionary entries. The packed-byte bound yields at most 3,639
decoder phrase records and admits serialized LZW widths through 12 bits.

Allocate every byte array, block view, and phrase record before processing.
Count all frame-local storage in one fixed aggregate limit and bound final raw
output separately. Derive bounded input and output chunks from the current
bytes, enforce a fixed call ceiling, and abort on an invalid result,
zero-progress `Progress`, impossible `NeedInput`, or call exhaustion.
Treat output-limit `NeedOutput` as a bounded terminal condition. Retain an
ordinary-build compile smoke and one reviewed truncated-magic seed. CLI,
benchmark, and interoperability remain separate gates.

## DD-242: LZW composition CLI is a fixed public-ABI adapter

- Date: 2026-07-18
- Status: accepted

Add `lzw-blocked-huffman` as an explicit selector while retaining LZ77 as the
default. Fix one-MiB raw frames, 65,536-symbol entropy blocks, the exact
two-byte-per-raw-byte packed-code bound, at most 32 entropy blocks, and 65,280
additional dictionary entries because the first free LZW code is 256. Keep the
existing 64-MiB aggregate internal-buffer policy.

Initialize, query, create, process, and destroy transforms only through the
public combined C ABI. Allocate the primary, secondary, and aligned views
regions from the queried requirements rather than duplicating their private
partition. Reuse the transactional file harness for ordinary, empty,
malformed, trailing-data, overwrite, and temporary-file-cleanup cases.
Benchmark and interoperability admission remain separate evidence steps.

## DD-243: LZW composition benchmark preserves the fixed CLI profile

- Date: 2026-07-18
- Status: accepted

Add `lzw-blocked-huffman` to the dependency-free benchmark with the CLI's one
MiB raw frames, 65,536-symbol entropy blocks, two-byte packed-code bound, 32
block limit, 65,280 additional dictionary entries, and 64-MiB aggregate policy.
Use the public combined C configuration, requirements, factory, process, and
destroy functions exclusively.

Conservatively reserve a descriptor for every possible full-profile entropy
block and permit raw fallback over all packed LZW bytes when sizing the encoded
buffer. Verify a complete round trip before timing, keep allocation and factory
construction outside the timed region, and report direction-specific primary,
secondary, and views extents plus their larger aggregate. This closes local
profile admission; interoperability schema publication remains separate.

## DD-244: Interoperability schema 5 appends LZW composition

- Date: 2026-07-18
- Status: accepted

Define schema 5 with codec set `marc-cli-v5`. Preserve the exact fifteen-entry
schema-4 order and append `lzw-blocked-huffman` as the sixteenth archive. The
generator emits schema 5 by default while retaining the established artifact
names and self-describing manifest fields.

Keep schemas 1 through 4 frozen at seven, eight, thirteen, and fifteen profiles.
Select each exact set from its manifest version and codec-set identifier; reject
missing, extra, duplicate, or mismatched entries. Generate schema 5 in the
compatibility test, derive each earlier bundle mechanically, and verify all
five with the public CLI. Require independent MSVC and ClangCL generation to
produce byte-identical input and sixteen archives before local admission.

## DD-245: LZD composition entropizes canonical reference pairs

- Date: 2026-07-18
- Status: accepted

Reserve `lzd-blocked-huffman` for dictionary ID 5 variant 1 plus entropy ID 2
variant 1. Preserve the standalone 16-byte LZD parameter region, empty entropy
parameters, fixed eight-byte reference-pair grammar, terminal absent-right
form, dictionary freeze, and ordinary version 1.0 frame header. Entropy blocks
count serialized token bytes and may split a token; both layers reset at every
outer frame.

For raw frame size `F` and entropy block size `E`, bound token staging by
`S = 8*ceil(F/2)`, block count by `ceil(S/E)`, phrase records by the lesser of
`floor(F/2)` and the configured maximum, and the iterative expansion stack by
that admitted count plus one. Encoding fixes the entire LZD parse before
entropy planning. Decoding reconstructs the full token region, validates its
acyclic grammar and exact raw extent, and only then expands transactionally.

Retain the three-region caller-workspace model for future admission. Encoding
needs aligned LZD encoder records; decoding needs Blocked Huffman views, LZD
phrase records, and explicit expansion-stack references in one checked opaque
layout. This decision specifies bytes, bounds, validation order, a hand vector,
and a reserved name only. Decoder, encoder, streaming, C ABI, CLI, fuzz,
benchmark, completion, and interoperability remain separate steps.

## DD-246: LZD composition validates before transactional expansion

- Date: 2026-07-18
- Status: accepted

Implement the first executable boundary as a complete-frame validator and
decoder, without admitting an encoder, streaming transform, factory, or public
profile. Parse and validate the generic frame first, validate the Blocked
Huffman controller, reconstruct the complete LZD token region into caller-owned
staging, and then run the ordinary LZD grammar validator. Check final raw-output
and expansion-stack capacities only after the entire serialized dictionary
stream is known to be valid; expansion remains iterative and publishes no bytes
on any earlier failure.

Refine the LZD validation-workspace query with the declared raw frame size. An
eight-byte terminal token for a one-byte frame admits zero stored phrases,
whereas each successful right-present pair consumes at least two raw bytes;
therefore the exact phrase bound is the lesser of the token count,
`floor(F/2)`, and the configured entry maximum. Retain the older conservative
query for callers that do not yet know `F`. Count descriptors, payload,
dictionary staging, block views, phrase records, expansion references, and raw
transactional output in checked aggregate limits appropriate to validation or
decode. Report arithmetic overflow distinctly from an ordinary workspace-limit
failure.

## DD-247: LZD composition fixes tokens before entropy planning

- Date: 2026-07-18
- Status: accepted

Add the matching internal complete-frame planner and encoder without publishing
a stream factory. Query the exact LZD encoder-entry count, complete the
deterministic LZD parse, and serialize the resulting eight-byte reference pairs
into caller-owned staging before asking Blocked Huffman to choose block models
and raw fallbacks. Construct the generic frame header only from those fixed
sizes. A short final serialized destination is rejected after complete planning
and before writing any header, descriptor, or payload byte.

For a one-byte terminal token, require zero encoder entries because the absent
right reference creates no phrase. For a right-present two-byte pair, require
one entry. Count encoder entries and the actual staged token extent together
under checked aggregate limits. Tests that isolate later frame-extent rejection
must first supply the full `8*ceil(F/2)` staging capacity; otherwise the earlier
and more specific staging-capacity error is correct. This preserves validation
order instead of weakening an earlier check to satisfy a later expectation.

## DD-248: LZD composition owns a three-view opaque decoder layout

- Date: 2026-07-18
- Status: accepted

Define internal known-size profile requirements before adding streaming or a C
factory. Bound encoder staging by `8*ceil(F/2)`, encoder records by the lesser
of `floor(F/2)` and the configured maximum, entropy views by the corresponding
block ceiling, and complete serialized capacity by raw Blocked Huffman fallback
for every staged byte. Count raw input, staging, serialized frame, and typed
encoder records under one checked aggregate admission rule.

Place decoder block views first in the opaque typed region, align and append
LZD phrase records, then align and append 32-bit iterative expansion references.
Derive phrase capacity as the lesser of staged whole tokens, `floor(max frame
size/2)`, the local dictionary-entry limit, and the format maximum. Reserve one
additional expansion reference. Record both offsets, total bytes, and maximum
alignment, and require partition helpers to rederive every value before
publishing any span. Zero encoder records use zero bytes and neutral alignment
one. This fixes only internal sizing and layout; streaming and public admission
remain separate steps.

## DD-249: LZD composition streams only complete transactional frames

- Date: 2026-07-18
- Status: accepted

Add bounded incremental transforms over the complete-frame codec and the
DD-248 typed partitions. The encoder buffers one raw frame, fixes and entropy-
codes it privately, then drains the complete serialized frame. The decoder
buffers one serialized frame, reconstructs and validates all LZD state into
private staging, expands into a private raw frame, and only then drains bytes to
the caller. Earlier frames remain committed if a later frame is malformed.

Count actual serialized frame, token staging, raw frame, block views, phrase
records, and expansion references again at the streaming boundary; profile
admission is not the sole safety check. `Flush` preserves a partial frame and
does not create a boundary, while `ResetBlock` remains unsupported. A call that
drains the 80-byte prefix returns `Progress`, not `NeedInput`, because the
process contract reserves starvation statuses for zero-progress calls. Local
dictionary limits must admit the maximum declared in the serialized LZD
parameters even when a particular small frame needs only one record.

## DD-250: LZD composition enters the public C ABI through opaque regions

- Date: 2026-07-18
- Status: accepted

Add `marc_lzd_blocked_huffman_config` with known original size, frame and
entropy-block sizes, maximum LZD entries, and the complete relevant local
limits. Preserve the common three-region ABI: primary holds raw input or
serialized input; secondary holds LZD token staging followed by encoded or
decoded frame storage; aligned views hold encoder entries or the decoder's
block views, phrase entries, and expansion references.

The requirements query delegates all profile arithmetic to the internal
calculators and reveals only byte extents and alignment. The factory repeats
profile construction, revalidates the opaque partition, and creates the
existing bounded transforms without exposing a C++ type. Reject invalid struct
metadata, nonzero reserved fields, short regions, and misalignment before a
handle is published. Fix the public boundary with a pure-C `ABABX` round trip:
three 96-byte frames after the 80-byte prefix, for 368 bytes total. This admits
only the factory; completion, fuzzing, CLI, benchmark, and interoperability are
independent evidence.

## DD-251: LZD composition completion is proved through the public C ABI

- Date: 2026-07-18
- Status: accepted

Exercise the composition only through public configuration, requirements,
creation, processing, and destruction calls. With 64-byte raw frames, bind the
exact LZD worst case to 256 token bytes, 32 phrase entries, four 64-byte entropy
blocks, and four descriptors. Cover empty input, all one-byte values, the full
byte alphabet, repetition, binary patterns, deterministic generated data, and
lengths immediately around the frame boundary. Repeated and differently
chunked encodes must be byte-identical, and repeated ended calls must remain
stable.

For a 193-byte four-frame stream, corrupt the final frame sequence, truncate
its final byte, and append one trailing byte in independent cases. Each failure
must commit exactly the first 192 raw bytes, leave the final destination byte
untouched, and retain the same positioned terminal error on a later call. This
admits completion evidence only; CLI, benchmark, decoder fuzzing, and
interoperability remain separate gates.

## DD-252: LZD composition fuzzing fixes every expansion workspace

- Date: 2026-07-18
- Status: accepted

Add a streaming-decoder libFuzzer boundary with no input-controlled allocation.
Cap supplied input at 8 KiB, total output at 4 KiB, one raw frame at 1 KiB,
compressed payload and reconstructed LZD tokens at 4 KiB each, and entropy
views at eight records. Derive 512 phrase records and 513 iterative expansion
references from the raw-frame limit, and count every frame-local region in one
fixed aggregate limit before constructing the decoder.

Derive partial input and output chunks from current bytes, validate every
reported count and status, reject impossible zero-progress and exhausted-input
states, and enforce a fixed call ceiling. Treat a full bounded output followed
by `NeedOutput` as a normal harness stop. Retain an ordinary MSVC/Clang compile
smoke, a sanitizer-enabled target, and one reviewed five-byte truncated-magic
seed. This admits bounded decoder fuzzing only; CLI, benchmark, and
interoperability remain separate gates.
