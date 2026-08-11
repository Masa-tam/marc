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

## DD-253: LZD composition CLI is a fixed public-ABI adapter

- Date: 2026-07-18
- Status: accepted

Add `lzd-blocked-huffman` as an explicit CLI selector while preserving LZ77 as
the default. Fix one-MiB raw frames, 65,536-symbol entropy blocks, the exact
`8*ceil(F/2)` four-MiB token bound, at most 64 entropy blocks, the format-default
65,536 LZD entries, and the existing 64-MiB aggregate workspace policy.

Initialize, query, create, process, and destroy only through the public combined
C ABI. Allocate all three regions from its direction-specific requirements and
honor reported alignment; do not reproduce the decoder's private three-view
layout. Reuse the common atomic file harness for ordinary and empty round trips,
existing-output rejection, malformed and trailing input, and temporary-file
cleanup. This admits CLI support only; benchmark and interoperability remain
separate gates.

## DD-254: LZD composition benchmark preserves the fixed CLI profile

- Date: 2026-07-18
- Status: accepted

Add `lzd-blocked-huffman` to the dependency-free benchmark with the CLI's
one-MiB raw frames, 65,536-symbol entropy blocks, exact four-MiB LZD token
bound, 64 possible entropy blocks, 65,536 entries, and 64-MiB local internal-
buffer policy. Conservatively reserve raw fallback and one 16-byte descriptor
for every possible token block when calculating complete-stream capacity.

Use only the public configuration, requirements, factory, process, and destroy
functions. Verify a full round trip before timing, exclude allocation and
factory lifecycle from timed intervals, and report encode/decode throughput,
complete-stream ratio, all six direction-specific workspace extents, and the
larger three-region sum as codec peak workspace. This closes local profile
admission; interoperability schema publication remains separate.

## DD-255: Interoperability schema 6 appends LZD composition

- Date: 2026-07-18
- Status: accepted

Define schema 6 with codec set `marc-cli-v6`. Preserve the exact sixteen-entry
schema-5 order and append `lzd-blocked-huffman` as the seventeenth archive. The
generator emits schema 6 by default while retaining established artifact names,
the deterministic 8,193-byte fixture, per-file size and SHA-256 fields, complete
source revision, and producer metadata.

The verifier accepts schemas 1 through 6 only through their exact versioned
profile lists and codec-set rules, rejecting missing, extra, duplicate, or
mismatched entries. Generate schema 6 in the local compatibility test, verify
it, then filter successively to frozen schemas 5, 4, 3, 2, and 1 and verify each
generation. Cross-platform execution remains release evidence produced after
push; local admission proves deterministic generation and strict protocol
compatibility without claiming foreign-platform results.

## DD-256: LZMW composition entropizes canonical references

- Date: 2026-07-18
- Status: accepted

Reserve `lzmw-blocked-huffman` for dictionary ID 6 variant 1 plus entropy ID 2
variant 1. Preserve the standalone 16-byte LZMW parameter region, empty entropy
parameters, fixed four-byte reference grammar, adjacent-phrase insertion,
smallest-reference tie rule, dictionary freeze, and ordinary version 1.0 frame
header. Entropy blocks count serialized token bytes and may split a token; both
layers reset at every outer frame.

For raw frame size `F` and entropy block size `E`, bound token staging by
`S = 4F`, block count by `ceil(S/E)`, phrase records by the lesser of
`max(F-1, 0)` and the configured maximum, and the iterative expansion stack by
that admitted count plus one for a nonempty frame. Encoding fixes the entire
LZMW parse before entropy planning. Decoding reconstructs the full token
region, validates its acyclic adjacent-phrase grammar and exact raw extent, and
only then expands transactionally.

Retain the three-region caller-workspace model for future admission. Encoding
needs aligned LZMW phrase-span records; decoding needs Blocked Huffman views,
LZMW phrase records, and explicit expansion-stack references in one checked
opaque layout. This decision specifies bytes, bounds, validation order, a hand
vector, and a reserved name only. Decoder, encoder, streaming, C ABI, CLI,
fuzz, benchmark, completion, and interoperability remain separate steps.

## DD-257: LZMW combined decode validates two bounded grammars

- Date: 2026-07-18
- Status: accepted

Parse and contextually validate the complete generic frame header before
deriving descriptor, payload, token-staging, block-view, and phrase-workspace
extents. Reject truncation, trailing bytes, undersized caller regions, and the
complete validation aggregate before entropy decode. Decode Blocked Huffman
only into caller-owned staging, then run the ordinary LZMW validator across the
entire reconstructed region. A non-multiple-of-four extent, unavailable
reference, adjacent-phrase overflow, premature end, or trailing token is a
dictionary validation failure and publishes no raw bytes.

Derive the iterative stack requirement only from the validated LZMW dictionary
entry count. Before expansion, require raw capacity, stack capacity, and the
complete descriptor, payload, token, block-view, phrase-record, stack, and raw
aggregate. Invoke the existing iterative LZMW decoder only after those checks.
The hand vector and a two-literal adjacent-phrase frame are permanent tests;
all truncations, trailing bytes, layer-specific malformed data, workspace
shortages, unsupported pipelines, and aggregate failures are negative tests.
Encoder and complete-stream behavior remain separate decisions.

## DD-258: LZMW combined planning fixes references before entropy

- Date: 2026-07-18
- Status: accepted

Require one complete deterministic LZMW planning pass before entropy planning.
The caller supplies `min(max(F-1, 0), maximum_entries)` phrase-span records and
up to `4F` token-staging bytes. Validate both extents and their checked aggregate
before serializing the exact four-byte references into staging. Blocked Huffman
then plans only over that immutable logical region, so entropy blocks may split
references without changing the LZMW parse or encoded bytes.

Reject empty frames, input inconsistent with the stream's next frame extent,
invalid parameters, insufficient phrase-span or staging workspace, component
limit failures, and arithmetic overflow before serialized output exists. After
the generic header validates, return the exact header, descriptor/model, and
payload extent. Encoding repeats complete planning, rejects short output
without modifying it, then serializes the header and entropy regions. Tests
require the hand vector byte for byte, repeated deterministic encoding,
reference-boundary splits, raw and canonical-Huffman representations, complete
round trips, workspace and aggregate limits, and frame-size mismatch handling.

## DD-259: LZMW composition exposes one checked opaque typed region

- Date: 2026-07-18
- Status: accepted

Keep the public three-region ownership model: primary frame staging, secondary
canonical-token staging, and one aligned opaque typed region. Encoder sizing
uses the largest actual raw frame `F`, token capacity `4F`, at most
`min(max(F-1, 0), maximum_entries)` phrase spans, raw-fallback entropy extent,
and the complete four-region aggregate. Empty streams require no frame or typed
storage and report alignment one.

Decoder sizing is limits-only. Reserve the configured maximum block views,
token staging, raw staging, and `min(max(T/4-1, 0), maximum_entries)` phrase
records for maximum serialized extent `T`, plus one expansion reference. Do not
reduce phrase capacity from the raw frame limit: a malformed frame may contain
more tokens than its raw declaration, and the grammar validator must reject it
as trailing data rather than fail because the profile under-sized its workspace.
Compute aligned block, phrase, and expansion offsets with checked arithmetic;
partition only when counts, offsets, byte extent, and maximum alignment exactly
match the recomputed layout. Reject short or misaligned storage.

## DD-260: LZMW combined streaming commits complete frames

- Date: 2026-07-18
- Status: accepted

Encode by draining the canonical 80-byte stream prefix, collecting exactly the
next contextual raw frame, invoking the complete-frame planner and encoder in
the profile-provided regions, and draining that immutable frame before reusing
storage. A full frame may be emitted before later `EndInput`; a final short
frame is valid only when it completes the known original size. Nonterminal
`Flush` does not close a partial frame. Reject `ResetBlock`, excess input, and
premature final input with sticky stable errors.

Decode by collecting and validating the prefix, then each 56-byte frame header
before accepting its bounded body. Check the actual encoded frame, token
staging, raw staging, block-view, phrase-record, and expansion-stack aggregate
before body collection. Decode a complete frame only into raw staging and drain
it afterward. Thus malformed frame `N` publishes no byte from `N`, while bytes
from earlier frames remain committed. Require one-byte input/output equivalence
with the complete-frame oracle, empty-stream exactness, truncation and trailing
rejection, workspace failures, sticky later-frame corruption, flush behavior,
repeated terminal status, and aggregate limits.

## DD-261: LZMW composition enters the public C ABI through opaque regions

- Date: 2026-07-18
- Status: accepted

Add one size-tagged `marc_lzmw_blocked_huffman_config` and matching initialize,
workspace-query, and create functions without changing an existing ABI object.
Require known-size encoding and expose frame size, entropy-block size, LZMW
entry limit, and all relevant trusted decoder limits. Preserve the common
primary, secondary, and aligned views ownership contract for both static and
dynamic libraries.

For encode, report raw-frame primary storage, then canonical-reference staging
and serialized-frame storage in secondary, plus opaque LZMW encoder entries.
For decode, report serialized-frame primary storage, reference staging and
transactional raw storage in secondary, plus the checked Blocked Huffman view,
LZMW phrase-record, and expansion-reference layout. Factory construction must
repeat the profile calculation and opaque partition, reject short or misaligned
regions and nonzero reserved fields, and publish no handle on failure. Prove
the lifecycle from a pure C11 translation unit linked to the shared library.

## DD-262: LZMW composition completion is proved through the public C ABI

- Date: 2026-07-18
- Status: accepted

Exercise the combined profile only through its public initialize, requirements,
create, process, and destroy operations. Use 64-byte raw frames, 64-byte
entropy blocks, the exact `4F` canonical-reference maximum, at most `F-1`
generated LZMW entries, and locally bounded aggregate storage. Require empty
input, every one-byte value, the ordered byte alphabet, repeated and periodic
data, deterministic pseudo-random data, and lengths 63, 64, and 65 to round
trip with byte-identical repeated encoding.

For a 193-byte four-frame stream, require one-byte and mixed input/output
chunking to preserve the encoded representation and decoded bytes. Corrupt,
truncate, or append data to the final one-byte frame and require the decoder to
commit exactly the earlier 192 raw bytes, leave the final output byte untouched,
and repeat the same terminal error position without consuming or producing
additional data. This admits completion evidence but does not imply CLI,
benchmark, fuzz, or interoperability admission.

## DD-263: LZMW combined fuzzing fixes token-derived phrase capacity

- Date: 2026-07-18
- Status: accepted

Add a streaming-decoder fuzz target with at most 8 KiB supplied input, 4 KiB
total output, 1 KiB raw frames, 4 KiB canonical-reference and payload extents,
and eight entropy blocks. Derive phrase capacity from malformed-admissible token
storage as `4096/4-1 = 1023`, not from the declared raw frame size, and reserve
1,024 iterative expansion references. Count the fixed encoded frame, reference
staging, raw staging, block views, phrases, and expansion entries in the local
aggregate limit before processing input.

Use byte-derived input/output chunks and a fixed call ceiling; abort only on an
invalid process result, forbidden zero-progress status, impossible exhausted-
input request, or call-ceiling breach. Keep the libFuzzer/ASan/UBSan executable
in the explicit Clang fuzz build and compile its entrypoint warning-clean in
ordinary builds. Permanently test every truncation of a valid one-frame stream,
extreme frame length fields, and a raw entropy block that reconstructs an
unavailable LZMW reference, requiring zero raw publication and sticky failure.

## DD-264: CLI dogfoods LZMW composition only through the public C ABI

- Date: 2026-07-18
- Status: accepted

Add `lzmw-blocked-huffman` as an explicit selector while preserving LZ77 as
the default. Configure one-MiB raw frames, 65,536-byte entropy blocks, the
exact four-byte-per-raw-byte reference maximum, at most 64 entropy blocks,
65,536 generated phrase entries, and the common 64-MiB aggregate policy. Query
all three workspace extents and opaque alignment through
`marc_lzmw_blocked_huffman_workspace_requirements`; create and process the
transform only through the matching public C functions.

Reuse the common bounded 64-KiB I/O loop and transactional `.tmp` output
commit. The integration test must round-trip deterministic and empty files,
refuse overwrite, reject malformed input, reject a valid stream with trailing
data, and leave neither destination nor temporary output on either decode
failure. CLI publication does not imply benchmark or interoperability
admission.

## DD-265: LZMW composition benchmark measures the public profile lifecycle

- Date: 2026-07-18
- Status: accepted

Add `lzmw-blocked-huffman` to the dependency-free benchmark with exactly the
CLI's one-MiB frame, 65,536-byte entropy block, `4F` maximum reference extent,
64-block cap, 65,536-entry dictionary policy, and 64-MiB active aggregate
limit. Construct encoder and decoder only through the public C configuration,
requirements, create, process, and destroy functions. Bound the output buffer
by the 80-byte prefix, four bytes per input byte, 56 bytes per frame, and one
16-byte descriptor per maximum entropy block.

Before timing, encode once, decode once, and require exact input equality.
Measure encode and decode independently, report serialized ratio and each
direction's primary, secondary, and opaque views bytes, and define peak
workspace as the larger sum of those three caller-reserved regions. Keep this
reservation measurement distinct from the limit on simultaneously active
decoder data. Benchmark publication does not imply interoperability admission.

## DD-266: Interoperability schema 7 appends LZMW composition

- Date: 2026-07-18
- Status: accepted

Define schema 7 with codec set `marc-cli-v7`. Preserve the exact seventeen-entry
schema-6 order and append `lzmw-blocked-huffman` as the eighteenth archive. The
generator emits schema 7 by default while retaining the established artifact
names, deterministic 8,193-byte fixture, per-file size and SHA-256 fields,
complete source revision, and producer metadata.

The verifier accepts schemas 1 through 7 only through their exact versioned
profile lists and codec-set rules, rejecting missing, extra, duplicate, or
mismatched entries and out-of-order archives. Generate schema 7 in the local
compatibility test, verify it, reject a reordered copy before decoding, then
filter successively to frozen schemas 6, 5, 4, 3, 2, and 1 and verify each
generation. Cross-platform execution remains release evidence produced after
push; local admission proves deterministic generation and strict protocol
compatibility without claiming foreign-platform results.

## DD-267: Final publication audit derives inventories from public profiles

- Date: 2026-07-18
- Status: accepted

Describe the repository's public surface as eighteen profiles: five standalone
entropy codecs, six standalone dictionary codecs, all six dictionary codecs
composed with Blocked Huffman, and the checksum-raw profile. Prefer this
capability-derived count in the repository README over manually enumerating a
partial subset. State that benchmark and bounded decoder-fuzz adapters cover
the same complete public set, while the CLI reference remains the exact name
registry.

Require one labeled benchmark smoke test for each public profile. The audit
found that the benchmark registry accepted standalone `lz77`, but its default
CLI status had hidden the absence of an explicit benchmark-smoke registration.
Add that eighteenth smoke without changing benchmark measurement behavior.

Treat architecture text as a current contract rather than a chronological
implementation log: published LZ78 and LZD compositions must not be described
in the present tense as reserved, future, or unadmitted. Keep chronological
staging in the implementation records. Retain `actions/checkout@v6`, advance
artifact publication from v4 to the current official
`actions/upload-artifact@v7`, and leave later dependency proposals to
Dependabot. This audit changes no stream byte, C ABI, codec behavior, or
artifact name.

## DD-268: README reports CI and notices carry the dependency license

- Date: 2026-07-18
- Status: accepted

Place one GitHub Actions badge immediately below the README title. Derive it
from `.github/workflows/ci.yml` and constrain it to `main`. Use GitHub's plain
image form so it remains compatible with the repository's strict Markdown link
validator. A badge reports CI state; it does not replace the readiness matrix
or make a compatibility or security guarantee.

Align GoogleTest attribution with the requested mffv1 notice structure: state
that marc itself is MIT licensed, distinguish development/test dependencies
from marc's license, identify the submodule path and upstream repository,
include the exact BSD-3-Clause text from the initialized submodule, point to
the authoritative local license, and state that GoogleTest is not linked into
library artifacts. Do not duplicate the current submodule version or commit in
the notice; the Git link is authoritative and Dependabot may update it without
changing license terms. Add the root notice to mandatory documentation-layout
verification and compare its fenced license text byte-for-byte after newline
normalization with the initialized submodule's `LICENSE`.

## DD-269: Documentation validation recognizes linked images

- Date: 2026-07-18
- Status: accepted

Make the CI badge both status-bearing and navigable by using GitHub's documented
linked-image Markdown form. Before the documentation validator scans ordinary
links, rewrite each linked image into separate image-target and navigation-target
links. This preserves validation of relative image assets and relative badge
destinations without allowing nested brackets to merge unrelated links into one
false broken target. The README badge is the repository-owned regression case.

## DD-270: GoogleTest tracks an explicit stable release branch

- Date: 2026-07-18
- Status: accepted

Pin the GoogleTest submodule to the `v1.17.0` commit and declare `v1.17.x` as
its update branch in `.gitmodules`. The first Dependabot submodule proposal
after publication moved the gitlink from the 2025 `v1.17.0` commit to the 2023
`v1.14.0` commit even though its pull-request title described the change as a
bump and its tests passed. Treat commit ancestry, release tags, and dates as
the dependency-version evidence rather than the proposal label alone.

Allow Dependabot to propose patch-line advances through the explicit branch.
Changing to a later GoogleTest release line remains a deliberate review that
updates `.gitmodules`, the gitlink, and CI evidence together. GoogleTest remains
development/test-only and this policy does not alter marc library artifacts.

## DD-271: Public CI generation evidence is distinct from cross-decoding

- Date: 2026-07-18
- Status: accepted

Record the first successful public pushed-revision workflow by immutable run ID
and full source revision. Require the Windows/MSVC and Ubuntu/Ninja suite jobs,
all four operating-system/linkage installed-package jobs, and both named
interoperability artifacts before closing the CI-generation item in baseline
readiness.

Do not infer foreign cross-decoding, additional-architecture coverage,
representative performance, or long-running sanitizer-fuzz evidence from green
CI or artifact creation. Preserve those as separate release-evidence items so a
status badge cannot broaden the claim.

## DD-272: Cross-platform evidence records environment boundaries

- Date: 2026-07-18
- Status: accepted

Accept an interoperability result only when the producing artifact, local
platform, compiler, exact source revision, and verifier result are reported.
For the first external run, record Ubuntu 26.04 as a WSL2 x86-64 environment
rather than generalizing it to bare-metal Linux or another architecture.

Require both directions for the platform claim: the Ubuntu executable must
verify the Windows and Ubuntu CI bundles, and the Windows executable must verify
the Ubuntu 26.04 bundle. Supplement the verifier results with a direct equality
check over the common input and all eighteen archives. Keep non-x86-64 testing
open even when every x86-64 producer is byte-identical.

## DD-273: The initial project release is source-oriented

- Date: 2026-07-18
- Status: accepted

Prepare `v0.1.0` as a deliberate source release whose project version is
independent of stream-format, C ABI, and interoperability-schema versions.
Publish a changelog and a repository release procedure before creating any tag.
Install the changelog with the existing project documentation.

Do not present CI interoperability bundles as installable binary packages and
do not promise maintainer-built or signed binaries in the initial release.
Require the final tagged commit to match the changelog, CMake version, pushed CI
revision, and reviewed evidence. Preserve outstanding architecture, benchmark,
and fuzz evidence as explicit release decisions rather than allowing a green
status badge to erase them.

## DD-274: The Windows preset opts into MSVC translation-unit parallelism

- Date: 2026-07-19
- Status: accepted

Expose `MARC_MSVC_MULTIPROCESS_COMPILE` as an opt-in CMake option with a default
of OFF. When selected under MSVC, add `/MP` to C and C++ compile steps through
language-scoped generator expressions. Enable the option in the canonical
`windows-msvc` preset so large targets such as the core test executable can
compile independent translation units concurrently.

Do not inject a caller-provided raw flag string, pass `/MP` to non-MSVC tools,
or make it an unavoidable property of installed marc consumers. Permit
memory-constrained Windows builders to disable the option. Treat this solely as
a build-throughput policy with no format, ABI, runtime, or determinism effect.

## DD-275: LZ77 plus Adaptive Huffman is the next specified composition

- Date: 2026-07-19
- Status: accepted

Reserve `lz77-adaptive-huffman` for LZ77 algorithm/variant 1 followed by
Adaptive Huffman algorithm/FGK variant 1 in format version 1.0. Preserve the
canonical 16-byte LZ77 token serialization as the exact entropy-layer symbol
stream. Use the existing 16-byte Adaptive descriptor, one freshly reset FGK
tree per nonempty outer frame, entropy block size zero, block count one, and no
entropy parameter or view-table region.

Cap the profile's raw frame size at 2^20 bytes. The independently specified
LZ77 worst case is sixteen token bytes per raw byte and Adaptive variant 1 caps
its frame at 2^24 symbols, so this profile bound admits every possible frame
without data-dependent configuration failure. Require complete Adaptive decode,
complete LZ77 validation, and private raw reconstruction before current-frame
publication. Treat the 264-bit-per-token-byte bound and all staged aggregate
extents as checked workspace inputs.

Specification reserves the name but does not publish it. Decoder, encoder,
streaming, C ABI, completion, fuzz, CLI, benchmark, and schema evidence remain
separate admission steps.

## DD-276: The first combined Adaptive vector is independently layered

- Date: 2026-07-19
- Status: accepted

Fix raw `A` as the first hand-checkable vector. Derive its 16-byte LZ77 Literal
token from the token grammar, then calculate the FGK payload independently from
the documented tree rules rather than obtaining the expected bytes from marc's
combined implementation. The resulting 31 bits are stored as `00 FF 17 74`
with seven valid final bits and one canonical 16-byte Adaptive descriptor.

Add a permanent boundary test that separately invokes the existing LZ77 and
Adaptive primitives and compares them with the independent token, descriptor,
payload, and 76-byte frame. Do not introduce a combined encoder merely to
generate its own oracle.

## DD-277: LZ77 plus Adaptive validation ends at private token staging

- Date: 2026-07-19
- Status: accepted

Introduce the combined profile's first executable decoder boundary as a
validator that accepts exactly one complete serialized frame and writes only to
caller-supplied private LZ77-token staging. It validates the stream pipeline,
LZ77 parameters, generic frame header, exact serialized extent, 16-byte token
alignment, the 2^24 Adaptive symbol cap, the LZ77 sixteen-byte-per-raw-byte
bound, the Adaptive 33-byte-per-symbol payload bound, and the aggregate active
workspace before entropy decoding.

Parse the fixed Adaptive descriptor, require strict FGK payload exhaustion and
zero padding, decode exactly the declared token bytes, then validate the entire
canonical LZ77 token stream against the declared raw size. Do not reconstruct
or publish raw bytes in this step. A later decoder may publish only after this
validator succeeds and private raw reconstruction also completes.

## DD-278: Combined frame decode publishes only a completed raw staging extent

- Date: 2026-07-19
- Status: accepted

Extend the LZ77 plus Adaptive frame boundary with a decoder that shares the
validator's complete preflight path. Before entropy mutation, require sufficient
dictionary staging, raw staging, and caller output, and include raw staging in
the checked aggregate of descriptor, payload, token bytes, and reconstructed
bytes.

After Adaptive decode and complete LZ77-token validation, reconstruct the frame
into private raw staging. Copy exactly the declared raw extent to caller output
only when LZ77 reconstruction succeeds. Capacity, workspace, header,
descriptor, entropy, token-validation, and reconstruction failures therefore
publish no current-frame raw byte.

## DD-279: Combined frame encoding plans both layers before serialization

- Date: 2026-07-19
- Status: accepted

Plan the complete LZ77 token stream into caller-supplied private staging, then
plan Adaptive FGK over those canonical bytes before writing a frame header,
descriptor, or payload. Check the exact descriptor, payload, token-staging, and
serialized extents with checked arithmetic and the configured aggregate limit.

The emitting operation repeats the deterministic Adaptive plan and requires it
to match the first payload extent before touching serialized output. It then
writes the generic frame header, fixed Adaptive descriptor, and FGK payload.
Insufficient token staging or serialized output and any planning discrepancy
therefore leave the serialized destination unchanged.

## DD-280: The combined reference profile defaults to 64 KiB frames

- Date: 2026-07-19
- Status: accepted

Keep 2^20 raw bytes as the format-level maximum, but default the executable
profile configuration to 65,536 raw bytes. At the maximum LZ77 expansion of
sixteen token bytes per raw byte and the conservative Adaptive bound of 33
payload bytes per token byte, a 1 MiB raw frame would require a 528 MiB payload
reservation and cannot satisfy marc's baseline 64 MiB payload limit.

For every nonempty configuration, calculate the largest raw frame, token
staging, worst-case FGK payload, serialized frame, and active aggregate before
constructing a transform. Decoder workspace depends only on local limits and
the 1 MiB/2^24 profile caps, never on an untrusted stream header. Larger frames
remain selectable when the caller deliberately supplies sufficient limits.

## DD-281: Combined streaming encode owns framing but not caller chunking

- Date: 2026-07-19
- Status: accepted

Emit the canonical stream header and LZ77 parameter region first, then collect
exactly one configured raw frame, plan and encode it through the combined frame
codec, and drain its serialized bytes before reusing any storage. Full frames
may be emitted before `EndInput`; the final short frame is determined solely by
the stream's declared original size.

`Flush` drains already representable prefix or frame bytes but does not close a
partial frame. `ResetBlock` is unsupported because outer frame boundaries are
fixed by the declared raw frame size. Enforce the active raw-input, token, and
serialized-frame aggregate at every prepared frame, and retain sticky terminal
error and ended states. Input/output chunking must not alter serialized bytes.

## DD-282: Streaming decode commits only complete validated frames

- Date: 2026-07-19
- Status: accepted

Collect and validate the fixed stream prefix before interpreting frame extents.
For each frame, parse its generic header into local values, check encoded,
dictionary, decoded, and aggregate capacities, collect exactly the declared
body, then invoke the combined frame decoder's private-staging boundary. Drain
that private raw frame only after every entropy and LZ77 check succeeds.

Expose one shared internal LZ77 reconstruction path for both private-staging and
direct-output complete-frame APIs; only the latter performs the final public
copy. A malformed later frame may leave earlier fully drained frames committed,
but contributes no byte itself. Truncation, trailing data, premature finish,
unsupported reset, and terminal errors are strict and sticky.

## DD-283: LZ77 plus Adaptive Huffman enters the C ABI as a bounded profile

- Date: 2026-07-19
- Status: accepted

Expose `marc_lz77_adaptive_huffman_config` and matching init, workspace-query,
and create functions without exposing either component as a caller-wired
object. Keep known-size input, fixed outer frames, LZ77 variant 1, and Adaptive
Huffman FGK variant 1 as one immutable profile. The configuration carries the
raw frame and LZ77 parameters plus all local decoder limits needed to validate
the composed stream; it has no entropy-block parameter because every outer
frame resets exactly one Adaptive tree.

Retain the common caller-owned workspace convention without an aligned views
region. Encoding uses primary storage for one raw frame and partitions
secondary storage into canonical LZ77-token staging followed by the complete
serialized frame. Decoding uses primary storage for the serialized frame and
partitions secondary storage into token staging followed by private raw-frame
staging. Query requirements again after every configuration change, reject
nonzero reserved fields and undersized regions before construction, and keep
the opaque transform lifecycle and stable status mapping unchanged.

Treat `max_frame_size` in this public profile as the raw outer-frame limit.
When invoking the already specified standalone Adaptive primitive over the
canonical token byte stream, derive a private limits view whose frame/output
extent admits the already validated token size. Keep the caller's compressed,
dictionary, aggregate, and LZ limits unchanged. This prevents the standalone
symbol-count meaning of `max_frame_size` from accidentally rejecting a valid
token stream or weakening the outer frame parser's raw-byte bound.

## DD-284: LZ77 plus Adaptive Huffman completion is audited through the C ABI

- Date: 2026-07-19
- Status: accepted

Use 64-byte raw frames, at most 1,024 canonical LZ77 token bytes per frame, the
33-byte-per-token Adaptive payload bound, and a 65,536-byte active workspace
limit for the public completion matrix. Cover empty input, every one-byte
value, the ordered byte alphabet, repeated bytes and patterns, deterministic
pseudo-random input, long zero runs, and lengths 63, 64, and 65. Require exact
re-encoding and byte-identical streams under one-byte and mixed input/output
chunk schedules.

For a 193-byte four-frame stream, independently corrupt, truncate, and extend
the final frame. Each case must publish exactly the first 192 validated raw
bytes, leave the final output sentinel unchanged, and retain the same stable
error category and position on repetition. This admits local completion
evidence only; fuzzing, CLI, benchmark, interoperability, and cross-architecture
determinism remain separate steps.

## DD-285: LZ77 plus Adaptive Huffman fuzzing covers frame and stream decode

- Date: 2026-07-19
- Status: accepted

Bound fuzz input at 8 KiB, total raw output at 4 KiB, one raw frame at 1 KiB,
canonical token staging at 16 KiB, and compressed payload at 8 KiB. Derive the
maximum serialized-frame and aggregate workspace arithmetically before parsing
and allocate every region at compile time. Do not allocate from serialized
lengths or expose a partially validated frame outside private staging.

Always exercise the incremental stream decoder with input-derived chunks and a
fixed call ceiling. When the first 80 input bytes parse as this exact stream
profile and its LZ77 parameters, also pass the remaining exact extent through
the complete-frame private-staging decoder. Abort on an invalid process result,
zero-progress `Progress`, impossible terminal starvation, or call-ceiling
exhaustion. Retain only the reviewed `MARC\n` seed in the source corpus; keep
generated mutations in ignored build storage.

## DD-286: The LZ77 plus Adaptive Huffman CLI uses the bounded reference profile

- Date: 2026-07-19
- Status: accepted

Add the explicit selector `lz77-adaptive-huffman` through the public C ABI and
the existing transactional temporary-file loop. Use 65,536-byte raw frames,
the 1,048,576-byte canonical LZ77 token bound, the conservative 33-byte-per-
token Adaptive payload bound, and the common 64 MiB aggregate policy. Obtain
the exact direction-specific workspace extents from the public requirements
query; the CLI must not reproduce a private partition.

Require binary and empty round trips, refusal to overwrite an existing output,
malformed-input rejection, strict trailing-data rejection, and removal of both
the requested output and `.tmp` staging path on failure. Keep the selector
explicit; do not change the default `lz77` profile or infer a decoder from the
serialized algorithm IDs.

## DD-287: The LZ77 plus Adaptive Huffman benchmark uses checked profile bounds

- Date: 2026-07-19
- Status: accepted

Add `lz77-adaptive-huffman` to the dependency-free benchmark through only the
public C configuration, workspace-query, create, process, and destroy API. Use
the same 65,536-byte raw frame, 1,048,576-byte canonical LZ77 token bound,
33-byte-per-token Adaptive payload bound, LZ77 window and match limits, and
64 MiB active aggregate policy as the CLI profile.

Compute complete-stream output capacity with 80 prefix bytes, 56 header bytes
and one 16-byte Adaptive descriptor per nonempty 64-KiB frame, plus 528 payload
bytes per raw input byte. Perform checked arithmetic before allocation. Query
encoder and decoder workspaces independently, verify an exact complete round
trip before timing, measure the two directions separately, and report peak
caller-reserved workspace as the larger direction-specific sum. Input and
encoded/decoded buffers remain outside that workspace metric.

## DD-288: Interoperability schema 8 appends the Adaptive composition

- Date: 2026-07-19
- Status: accepted

Define schema 8 with codec set `marc-cli-v8`. Preserve the exact eighteen-entry
schema-7 order and append `lz77-adaptive-huffman` as the nineteenth archive.
The generator emits schema 8 by default; schemas 1 through 7 retain their
frozen profile sets, codec-set rules, and ordering.

Require the verifier to match the exact schema-8 count and order before
decoding, then decode every foreign archive, compare its raw bytes with the
fixture, re-encode locally, and compare the complete canonical archive byte for
byte. The compatibility regression must generate schema 8, reject a reordered
schema-8 manifest, derive each frozen earlier schema by filtering only, and
verify all eight generations.

## DD-289: LZSS plus Adaptive Huffman preserves variable-token framing

- Date: 2026-07-19
- Status: accepted

Reserve `lzss-adaptive-huffman` for LZSS variant 1 followed by Adaptive Huffman
FGK variant 1. Serialize the existing 16-byte LZSS parameter region, no entropy
parameters, and zero entropy block size. Every nonempty outer frame owns exactly
one freshly reset FGK tree; neither LZSS history nor Adaptive state crosses the
frame boundary.

Retain a format-level raw-frame maximum of 1 MiB and use 65,536 raw bytes for
the bounded reference profile. An `F`-byte raw frame produces at most `2F`
canonical LZSS bytes because the all-Literal parse is the exact worst case.
The conservative Adaptive payload bound is therefore `66F` bytes. Require all
token, payload, frame, staging, and aggregate arithmetic to be checked before
allocation or mutation.

Decode one complete Adaptive block into private token staging, require exact
payload-bit exhaustion, validate the complete variable-length LZSS grammar and
derive exactly the declared raw size, reconstruct into separate private raw
staging, and only then publish the frame. Encoding likewise completes the LZSS
parse and Adaptive plan before emitting a frame byte. `Flush` does not shorten
a frame, `ResetBlock` is unsupported at the composition boundary, and empty
input remains the ordinary 80-byte parameterized prefix with no frame.

## DD-290: The first LZSS Adaptive vector is independently hand-checkable

- Date: 2026-07-19
- Status: accepted

Use raw byte `41`, whose canonical LZSS representation is the two-byte Literal
`00 41`. Starting from a fresh FGK NYT root, emit the first unseen symbol `00`
as eight zero literal bits. Emit the second unseen symbol `41` as NYT path `0`
followed by `41` LSB-first. The resulting 17 physical bits are payload
`00 82 00` with one valid bit in the final byte.

Fix the descriptor at symbol count 2, payload size 3, and final-valid-bit count
1. Combine it with a generic header declaring raw size 1, dictionary size 2,
payload size 3, one entropy block, and 16 descriptor bytes for an exact 75-byte
frame. Test the LZSS token and Adaptive payload independently before serializing
the complete frame; do not use a combined-profile encoder as its own oracle.

## DD-291: LZSS Adaptive validation commits only canonical token staging

- Date: 2026-07-19
- Status: accepted

Introduce a decoder-side boundary that accepts exactly one serialized frame,
checks the selected LZSS and Adaptive variants and all generic extents, and
rejects both truncation and trailing bytes. Before entropy decoding, require a
nonzero token extent no greater than `2F`, a payload extent no greater than
33 bytes per token byte, sufficient caller-owned token staging, and the exact
descriptor-plus-payload-plus-token aggregate workspace under the configured
limit. All extent arithmetic is checked.

Decode the single Adaptive block with exact bit exhaustion into caller-owned
private staging, then validate the complete variable-length LZSS token grammar
and require it to derive exactly the declared raw size. This boundary neither
reconstructs nor publishes raw bytes. Short staging and pre-decode limit or
descriptor failures leave staging unchanged; entropy-valid but invalid LZSS
bytes may remain only in the explicitly private token staging. Raw commit,
streaming controllers, and the public factory remain later admission steps.

## DD-292: LZSS Adaptive raw publication uses a second staging region

- Date: 2026-07-19
- Status: accepted

Extend the strict complete-frame validator with a raw reconstruction boundary.
Require caller-owned raw staging large enough for the declared frame before
entropy decoding, and count that complete extent together with descriptor,
payload, and token staging against `max_internal_buffered_bytes`. Decode the
already validated LZSS token sequence into raw staging, retaining overlap-copy
semantics and the standalone decoder's checked limits.

Provide one internal operation that stops after private raw reconstruction and
another that copies exactly the reconstructed frame to caller output only after
all layers succeed. Check output capacity before either staging region is
mutated. Malformed headers, descriptors, Adaptive payloads, or LZSS token
streams publish no raw byte; a failure after entropy decoding may alter only the
explicitly private token staging. Streaming and public adapters remain later
steps.

## DD-293: LZSS Adaptive encoding freezes tokens before entropy planning

- Date: 2026-07-19
- Status: accepted

Plan each nonempty raw frame by first determining the exact variable-length
LZSS token extent, checking the `2F` bound, and serializing that canonical token
sequence once into caller-owned staging. Treat this staging as immutable entropy
input. Plan a fresh Adaptive Huffman tree over the exact bytes, enforce the
33-byte-per-token payload and descriptor-plus-payload-plus-token aggregate
limits, and validate the complete generic header before reporting the serialized
extent.

The frame encoder repeats only the deterministic Adaptive plan needed to recover
its descriptor, verifies the planned payload extent, and checks the complete
serialized destination before writing the header, descriptor, or payload. The
one-byte hand vector must reproduce DD-290 exactly. Short token staging may not
be mutated; a short serialized destination must remain unchanged. Empty input is
owned by the future stream controller and is not a frame-planner input.

## DD-294: LZSS Adaptive streaming decode commits complete frames

- Date: 2026-07-19
- Status: accepted

Decode the known-size stream through an explicit state machine that collects
the fixed 80-byte prefix, then each 56-byte frame header and its exact remaining
body. Validate the LZSS/Adaptive profile and parameters before accepting a
frame. Check encoded-frame storage, token staging, private raw staging, and
their complete aggregate before collecting an input-controlled body. Reject
token extents beyond `2F` and payload extents beyond 33 bytes per token directly
from the frame header before waiting for that body.

Pass each complete frame to the DD-292 private reconstruction boundary and make
its raw staging drainable only after success. Arbitrarily small caller output
may drain a committed frame without retaining caller spans. Latch `EndInput`
while draining; reject premature end, trailing bytes, bad sequence or extent,
unsupported `ResetBlock`, and later-frame corruption without publishing that
frame. Earlier fully decoded frames may already have been returned. Empty input
is the valid 80-byte prefix and repeated calls after completion return
`EndOfStream`.

## DD-295: LZSS Adaptive streaming encode latches finish before draining

- Date: 2026-07-19
- Status: accepted

Encode the known-size stream with a state machine that first drains the fixed
80-byte prefix, collects exactly one raw frame, completes DD-293 planning and
encoding into private frame storage, and only then drains serialized bytes.
Require caller-owned raw input storage for the largest frame, token staging for
the `2F` worst case, exact encoded-frame storage, and the complete
raw-plus-token-plus-serialized aggregate before preparing a frame.

Full frames become drainable as soon as collected; the final short frame is
prepared only when its known remaining extent is complete. `Flush` does not
close a partial frame and `ResetBlock` is unsupported. Validate that `EndInput`
is accompanied by exactly all remaining declared input, then latch it
immediately even if prefix or frame output must drain before any supplied input
can be consumed. Re-presented unconsumed input need not repeat the flag. Return
repeatable `EndOfStream` only after all serialized bytes have been emitted.

## DD-296: LZSS Adaptive profile exposes checked workspace extents

- Date: 2026-07-19
- Status: accepted

Define an internal profile constructor that fixes LZSS variant 1, Adaptive
Huffman FGK variant 1, the canonical 16-byte LZSS parameter extent, zero
entropy parameters, zero entropy block size, and the 65,536-byte reference
frame size. For the largest raw frame `F`, report encoder regions of `F` raw
bytes, `2F` canonical token bytes, and `56 + 16 + 66F` serialized-frame bytes.
Compute every product, sum, conversion, and the complete `F + 2F + serialized`
aggregate with checked arithmetic before returning any nonzero workspace.

For decoding, derive conservative caller-owned regions only from validated
local limits: `56 + max_internal_buffered_bytes` serialized bytes, token bytes
bounded by the minimum of `2 * min(max_frame_size, 1 MiB)`, the dictionary
limit, and Adaptive Huffman's 1-MiB decoded-symbol limit, and one raw region
bounded by `min(max_frame_size, 1 MiB)`. Empty known-size input requires no
per-frame encoder workspace. Reject invalid parameters, format/profile limits,
or unsupported headers with stable core error categories. This boundary fixes
the allocation contract needed by a later C factory without publishing one.

## DD-297: LZSS Adaptive enters the C ABI without allocator policy

- Date: 2026-07-19
- Status: accepted

Expose `marc_lzss_adaptive_huffman_config` and matching initialization,
workspace-query, and creation functions as one immutable LZSS variant 1 plus
Adaptive Huffman FGK variant 1 profile. Retain known-size input and the common
opaque-transform lifecycle. Carry the raw frame size, LZSS parameters, and all
relevant hard limits in a size-tagged configuration; do not expose an entropy
block size because each outer frame owns exactly one reset FGK tree.

Use primary workspace for raw-frame input during encoding and serialized-frame
input during decoding. Internally partition secondary workspace into token
staging followed by serialized-frame staging for encode, or token staging
followed by private raw-frame staging for decode. Require no aligned views
workspace. Obtain every extent from DD-296, reject null or undersized regions
and nonzero reserved fields before construction, and preserve the existing
stable C status mapping. The library allocates only the small opaque transform
handle with nonthrowing construction and never owns caller workspaces.

## DD-298: LZSS Adaptive completion is proven at the public boundary

- Date: 2026-07-19
- Status: accepted

Audit the composed profile only through its public C configuration, workspace
query, factory, process, and destroy functions. Use 64-byte raw frames,
128-byte worst-case LZSS token staging, the 33-byte-per-token Adaptive payload
bound, and a 65,536-byte aggregate limit. Cover empty input, every one-byte
value, the ordered byte alphabet, repeated zeroes, a repeated binary pattern,
deterministic pseudo-random bytes, and lengths 63, 64, and 65. Require repeated
encoding to be byte-identical and successful end state to be repeatable.

For a 193-byte four-frame stream, require identical bytes and round trips under
unchunked, one-byte, and mixed chunk schedules. Independently corrupt the final
frame sequence, truncate its last byte, and append trailing data. Every case
must return a sticky malformed-stream result after publishing exactly the first
192 bytes, leave the final output sentinel unchanged, and retain the same byte
and bit error positions on repetition. This completes the public-ABI evidence
column but does not admit CLI, fuzz, benchmark, or interoperability claims.

## DD-299: LZSS Adaptive fuzzing is fixed-memory and dual-boundary

- Date: 2026-07-19
- Status: accepted

Add one bounded decoder fuzz entry point that truncates every supplied case to
8,192 bytes and exercises both the exact complete-frame private-staging decoder
after a valid 80-byte prefix and the incremental stream decoder for every case.
Fix raw output at 4,096 bytes, one raw frame at 1,024 bytes, canonical LZSS
staging at 2,048 bytes, compressed payload at 8,192 bytes, and all controller
storage in stack-owned arrays. Derive input and output chunks from current
bytes, cap process calls, and abort on an invalid result or impossible stall.

Retain only the reviewed five-byte `MARC\n` truncated-magic seed in source.
Keep generated mutations in ignored build storage. Add permanent regressions
requiring every truncation of a canonical `ABABX` stream, all-ones generic
extent fields, and a nonzero reserved Adaptive descriptor byte to fail without
publishing a raw byte, while preserving sticky error category and position.
Compile the harness under ordinary MSVC and Clang builds; execute it only in a
separate sanitizer-enabled Clang build with explicit run, input, timeout, and
RSS bounds.

## DD-300: LZSS Adaptive CLI preserves file transactions

- Date: 2026-07-19
- Status: accepted

Admit the exact selector `lzss-adaptive-huffman` through only the DD-297 public
C lifecycle. Fix its tool policy at 65,536 raw bytes per frame, at most 131,072
canonical LZSS token bytes, a conservative 4,325,376-byte Adaptive payload,
and the checked raw-plus-token-plus-header-plus-descriptor-plus-payload
aggregate. Treat these as configuration limits only; obtain all actual
workspace extents from the public query and expose no private storage layout.

Retain the common destination transaction: refuse an existing output or
existing `.tmp`, stream into `.tmp`, close successfully, then rename exactly
once. On configuration, allocation, processing, malformed input, trailing
data, close, or rename failure, remove the temporary path and publish no
destination. Require ordinary and empty round trips, second-encode refusal,
malformed-prefix rejection, and valid-stream-plus-trailing-byte rejection in
the same integration script used by established selectors. This admits CLI
behavior but not benchmark or interoperability evidence.

## DD-301: LZSS Adaptive benchmark verifies before timing

- Date: 2026-07-19
- Status: accepted

Add the selector `lzss-adaptive-huffman` to the dependency-free benchmark
through only the DD-297 public C lifecycle and the same 65,536-byte frame,
`2F` token, and 33-byte-per-token payload policy as the CLI. Calculate complete
encoded capacity with checked prefix, per-frame header/descriptor, and payload
terms. Query encoder and decoder workspaces independently; expose no private
partition or inferred typed layout.

Before starting any timer, encode once, decode once, require the decoded extent
to equal the input extent, and compare every byte. Time encode and decode
separately only after that proof. Report codec name, iterations, input and
encoded bytes, complete-stream ratio, direction-specific seconds and MiB/s,
all six public workspace extents, and the larger direction-specific caller
workspace sum. Keep the smoke test free of performance thresholds; observations
depend on build, compiler, CPU, corpus, and system load. This admits benchmark
instrumentation but not interoperability evidence.

## DD-302: Interoperability schema 9 appends LZSS Adaptive Huffman

- Date: 2026-07-19
- Status: accepted

Define schema 9 with codec set `marc-cli-v9`. Preserve the exact nineteen-entry
schema-8 order and append `lzss-adaptive-huffman` as the twentieth archive. The
generator emits schema 9 by default; schemas 1 through 8 retain their immutable
profile sets, identifiers, and order.

Require the verifier to validate the exact schema-9 count and order before
decoding, decode every archive to the common fixture, and reproduce every
complete archive byte for byte through the local CLI. The compatibility
regression must generate schema 9, reject a reordered schema-9 manifest, derive
each frozen predecessor by filtering only, and verify all nine generations.
This admits the local interoperability adapter; pushed cross-platform artifacts
and an external bidirectional report remain separate evidence.

## DD-303: LZ78 Adaptive preserves fixed tokens and typed phrases

- Date: 2026-07-20
- Status: accepted

Reserve `lz78-adaptive-huffman` for LZ78 variant 1 followed by Adaptive Huffman
FGK variant 1. Preserve the 16-byte LZ78 parameter region, empty entropy
parameters, fixed eight-byte token grammar, ordinary version-1.0 frame header,
and zero entropy block size. Reset both the LZ78 dictionary and FGK tree at
every nonempty outer frame; empty input remains the 80-byte prefix.

Fix the format-level raw-frame maximum at 2^20 bytes and the bounded reference
cadence at 65,536 bytes. For raw extent `F`, admit at most `F` tokens, `8F`
canonical token bytes, and `264F` Adaptive payload bytes. Count the aligned
LZ78 phrase table, token staging, raw staging, serialized frame, and complete
aggregate before allocation or mutation.

Decode Adaptive bytes only into private token staging, validate the entire
fixed-width token stream and phrase graph in aligned bounded workspace, derive
the exact declared raw extent, reconstruct privately, and then commit. Encode
by fixing the LZ78 parse and token bytes before Adaptive planning. Freeze the
single-byte `A` vector independently as token `00 41 00 00 00 00 00 00`,
23-bit payload `00 82 7E`, and a 75-byte frame. Specification and vector
admission do not publish implementation, C ABI, CLI, benchmark, fuzz, or
interoperability support.

## DD-304: LZ78 Adaptive validation stops before raw reconstruction

- Date: 2026-07-20
- Status: accepted

Implement the first decoder boundary over one exact serialized frame. Validate
the fixed pipeline and parameters, generic header, complete serialized extent,
`8F` token bound, one Adaptive descriptor, `33D` payload bound for token extent
`D`, token-staging capacity, aligned phrase-entry capacity, and aggregate
workspace before invoking entropy decode.

Strict-decode exactly `D` canonical bytes into private staging, including exact
bit exhaustion and zero padding, then validate the complete fixed-width LZ78
grammar and phrase graph against the declared raw size. Treat the populated
private staging and phrase table as committed only on success; discard both
after any error. Perform no raw reconstruction
and expose no public factory. Require separate regressions for every truncation,
trailing data, descriptor failure, nonzero padding, invalid phrase reference,
impossible token extent, workspace shortage, aggregate limit, sequence, and
pipeline mismatch.

## DD-305: LZ78 Adaptive reconstructs privately before publication

- Date: 2026-07-20
- Status: accepted

Extend the exact-frame decoder only after the DD-304 entropy and phrase-graph
validator succeeds. Require complete raw-staging capacity and, for the
publishing entry point, complete caller-output capacity before entropy decode.
Add raw extent to the aggregate workspace bound. Reuse the iterative LZ78
decoder over validated token staging and aligned phrase records; input-driven
recursion remains forbidden.

Treat token, phrase, and raw staging as private discardable state until exact
reconstruction succeeds. The staging entry point stops with the complete raw
frame in private storage. The publishing entry point then copies exactly that
frame to caller output once. Require a nested `AABABCABC` phrase-chain case,
short raw and output capacities before mutation, aggregate failure including
raw staging, and malformed descriptor and phrase cases that leave caller output
unchanged. Do not add encoding, streaming, or public construction in this step.

## DD-306: LZ78 Adaptive encoding freezes canonical tokens before publication

- Date: 2026-07-20
- Status: accepted

Add an exact-frame planner that validates the fixed composition and raw-frame
extent, admits the complete aligned LZ78 encoder table and token-staging
capacities, plans and emits the deterministic LZ78 token stream into private
staging, and only then plans Adaptive Huffman over those immutable bytes. Count
the encoder table, token staging, Adaptive descriptor, and Adaptive payload in
the aggregate bound with checked arithmetic.

The matching encoder must finish that complete plan and validate serialized
destination capacity before writing any frame byte. It then serializes the
ordinary frame header and descriptor and encodes the exact staged tokens. Any
recomputed extent mismatch or lower-layer failure is an internal error. Require
the frozen single-`A` frame, deterministic nested-phrase round trip, short
encoder, token, and serialized capacities, aggregate shortage, empty and wrong
raw extents, and exact re-encoding of the frozen vector. This step admits no
streaming state machine or public factory.

## DD-307: LZ78 Adaptive streaming encoding preserves exact frame boundaries

- Date: 2026-07-20
- Status: accepted

Build the bounded known-size streaming encoder only over the DD-306 exact-frame
planner and encoder. Serialize the ordinary 80-byte stream prefix once, collect
exactly the configured raw frame extent, freeze and encode that complete frame
privately, and drain its serialized bytes before accepting the next frame.
Retain `EndInput` while prefix or frame bytes are draining and return a stable
terminal result after completion.

Require caller-owned raw-frame, token-staging, serialized-frame, and aligned
LZ78 encoder-entry spans. Validate maximum raw, token, and entry extents at
construction, validate the exact serialized-frame extent after planning, and
for each prepared frame count raw input, exact tokens, exact serialized frame,
and used encoder entries in the aggregate bound. One-byte input and output
must produce the same bytes as independently repeated exact-frame calls. `Flush`
does not shorten a frame, `ResetBlock` is unsupported, premature or excess
known-size input is invalid, and empty input emits only the prefix. This step
does not admit a streaming decoder, public workspace calculator, or C factory.

## DD-308: LZ78 Adaptive streaming decoding commits complete frames only

- Date: 2026-07-20
- Status: accepted

Add the bounded known-size decoder over the DD-305 transactional exact-frame
decoder. Collect and parse the complete 80-byte prefix, then collect each
generic frame header separately. Before accepting its body, reject impossible
token alignment or `8F` extent, non-single Adaptive descriptor layout, `33D`
payload overflow, short encoded, token, raw, or aligned phrase storage, and an
aggregate bound covering the exact frame, tokens, raw frame, and phrase table.

Decode only after the complete serialized frame is privately buffered. Keep
the reconstructed raw frame private and drain it incrementally only after all
entropy and phrase validation succeeds. A malformed later frame may leave
earlier frames committed but must publish none of its own bytes. Require
one-byte encoded input and raw output, output starvation with retained
EndInput, empty input, truncation at every byte, trailing data, later-frame
corruption, all workspace shortages, aggregate rejection, unknown flags,
`ResetBlock`, and stable error and End Of Stream results. This step adds no
public workspace calculator or C factory.

## DD-309: LZ78 Adaptive profile exposes checked typed workspace layouts

- Date: 2026-07-20
- Status: accepted

Define the bounded reference profile with a 65,536-byte raw frame cadence,
LZ78 variant 1, Adaptive Huffman variant 1, and the existing one-MiB format
cap. Encoder sizing uses at most `F` aligned entries, `8F` token bytes, and
`33D` payload bytes for token extent `D`; count raw, token, serialized frame,
and entries before admitting the configuration. Decoder sizing derives raw,
token, complete encoded-frame, and phrase-entry maxima from local limits.

Keep byte spans and typed records separate. Expose one opaque aligned encoder
region containing only `Lz78EncoderEntry` records and one decoder region
containing only `Lz78PhraseEntry` records. Partition helpers must recompute the
entire byte count and alignment, reject altered requirements, shortage, and
misalignment, and return empty views only for the canonical zero-byte layout.
This step admits internal construction and sizing only, not a C factory.

## DD-310: LZ78 Adaptive enters the C ABI with opaque typed views

- Date: 2026-07-20
- Status: accepted

Expose the fixed LZ78 variant 1 plus Adaptive Huffman variant 1 profile through
`marc_lz78_adaptive_huffman_config`, a requirements query, and a direction-
immutable factory. Retain known-size encoding and the common three-workspace
ABI. Primary storage holds raw input while encoding and complete serialized
frames while decoding. Secondary storage is partitioned into canonical LZ78
token staging followed by the complete encoded frame for encode, or token
staging followed by private raw output for decode.

The aligned views region is opaque to C callers. It contains only encoder
entries in the encode direction and only phrase entries in the decode
direction; creation must rederive and partition the exact typed layout instead
of casting caller-reported sizes directly. Require a strict C11 round trip,
default initialization checks, exact small-limit workspace checks, short and
misaligned workspace rejection, reserved-field rejection, and an unchanged
null output handle on failure. No allocator callback or unknown-size input is
introduced.

## DD-311: LZ78 Adaptive completion is audited through the public C ABI

- Date: 2026-07-20
- Status: accepted

Audit only the published C configuration, requirements query, factory,
process, and destroy functions. Use 64-byte raw frames, at most 512 canonical
LZ78 token bytes, the 33-byte-per-token Adaptive payload bound, 64 dictionary
entries, and a 65,536-byte aggregate limit. Allocate the opaque views region
from its queried byte count and alignment in both directions. Cover empty
input, all one-byte values, the ordered byte alphabet, repeated data, binary
patterns, deterministic pseudo-random bytes, and lengths 63, 64, and 65.
Require repeated encoding to be byte-identical and terminal success to be
stable.

For a 193-byte four-frame stream, require exact bytes and round trips under
unchunked, one-byte, and mixed chunk schedules. Corrupt the final frame
sequence, truncate its final byte, and append trailing data independently.
Every failure must be sticky, preserve its error position, publish exactly the
first 192 validated bytes, and leave the final output sentinel unchanged. This
completes public-ABI evidence only; it admits no CLI, benchmark, fuzz, or
interoperability claim.

## DD-312: LZ78 Adaptive fuzzing fixes byte and phrase storage up front

- Date: 2026-07-20
- Status: accepted

Add one bounded decoder fuzz entry point that truncates supplied input to
8,192 bytes and exercises both the exact complete-frame private-staging
decoder after a valid 80-byte prefix and the incremental stream decoder for
every case. Fix total output at 4,096 bytes, one raw frame at 1,024 bytes,
canonical LZ78 token staging at 8,192 bytes, compressed payload at 8,192
bytes, and the phrase table at 1,024 records. Include every byte and record
region in one fixed aggregate limit before processing.

Derive partial input and output chunks from current bytes, cap process calls,
and abort only on an invalid process result or impossible stall. Retain a
repository-authored truncated-magic seed and keep generated mutations in
ignored build storage. Add permanent regressions requiring every truncation of
a canonical `ABABX` stream, all-ones generic extent fields, and a nonzero
reserved Adaptive descriptor byte to fail without publishing raw bytes and to
retain sticky error category and position. Ordinary MSVC and Clang builds only
compile the harness; sanitizer execution remains a separate explicitly
bounded Clang workflow.

## DD-313: LZ78 Adaptive CLI uses the bounded reference profile

- Date: 2026-07-20
- Status: accepted

Publish `lz78-adaptive-huffman` as a command-line selector backed only by the
public C ABI. Use the fixed 65,536-byte raw frame cadence, at most 524,288
canonical LZ78 token bytes, at most 17,301,504 Adaptive payload bytes, and
65,536 dictionary entries. Use a conservative 32-MiB aggregate buffered-byte
limit, but obtain every direction-specific workspace extent and the opaque
typed-view alignment from `marc_lz78_adaptive_huffman_workspace_requirements`.
The CLI must not duplicate the private workspace partition or typed layouts.

Retain the common transactional file contract: write only to the exclusive
`.tmp` staging path, rename it after terminal success, and remove it after any
configuration, allocation, processing, malformed-stream, or commit failure.
Exercise the selector with the common encode/decode round trip and require
strict rejection of appended trailing bytes before claiming CLI publication.

## DD-314: LZ78 Adaptive benchmark measures only a verified public round trip

- Date: 2026-07-20
- Status: accepted

Add `lz78-adaptive-huffman` to the dependency-free benchmark through the same
public C ABI and 65,536-byte, 65,536-entry, 32-MiB policy as the CLI. Reserve
complete-stream encoded capacity with checked arithmetic from the 80-byte
parameterized prefix, one 56-byte frame header and 16-byte Adaptive descriptor
per nonempty frame, and the conservative `264` payload bytes per raw byte.
Do not derive or reproduce either opaque LZ78 record layout in the benchmark;
query both direction-specific workspace sizes and alignments from the ABI.

Before timing, encode once, decode once, and require byte-exact equality with
the source. Time fresh transform instances for each iteration and report
encoded size, ratio, directional elapsed time and throughput, all six queried
workspace extents, and the larger caller-reserved workspace total. Treat only
correctness, bounds, allocation, and API failures as benchmark failures;
throughput and compression ratio are observations, not pass thresholds. Add a
single-iteration smoke test over repository-owned input.

## DD-315: Interoperability schema 10 appends LZ78 Adaptive exactly once

- Date: 2026-07-20
- Status: accepted

Define interoperability schema 10 and codec set `marc-cli-v10` as the exact
twenty-entry schema-9 order followed by `lz78-adaptive-huffman`. Reuse the
unchanged deterministic 8,193-byte binary fixture. Generation must round-trip
all twenty-one profiles before writing the manifest; verification must check
the exact count and order, every declared size and SHA-256 value, foreign
decode equality, and byte-identical local re-encoding.

Keep schemas 1 through 9 frozen. The compatibility test starts from a complete
schema-10 bundle, rejects a reordered schema-10 manifest, removes only the
newest profile while converting to schema 9, and continues the existing
one-generation-at-a-time chain through schema 1. Local schema admission proves
the generator, verifier, and compatibility rules only. Cross-platform evidence
still requires CI artifacts from the same full Git revision and the established
bidirectional external verification procedure.

## DD-316: LZW Adaptive entropizes finalized packed-code bytes

- Date: 2026-07-21
- Status: accepted

Reserve `lzw-adaptive-huffman` for LZW variant 1 followed by Adaptive Huffman
FGK variant 1 under format version 1.0. Preserve the standalone 16-byte LZW
parameters, empty entropy parameters, LSB-first variable-width code schedule,
and final LZW zero padding. Complete the packed-code byte stream before entropy
processing; Adaptive Huffman consumes every resulting byte, including the
final padded byte, without interpreting LZW code or padding boundaries. Reset
both dictionaries at every outer frame.

For raw frame size `F` and maximum code width `W`, use the checked staging bound
`S = ceil(F * W / 8)` and Adaptive payload bound `33S`. Bound generated entries
by `min(F - 1, 2^W - 256, local_limit)` for nonempty frames. The reference
profile uses `F = 65,536` and `W = 16`, so `S = 131,072`, payload is at most
4,325,376 bytes, and generated entries are at most 65,280. Encoding freezes
canonical packed bytes before Adaptive planning. Decoding entropy-decodes into
packed-byte staging, validates width changes, references, `KwKwK`, LZW padding,
and exact raw size, reconstructs privately, and only then publishes.

Freeze the raw-`A` vector independently: LZW bytes `41 00`, Adaptive payload
`41 00 00`, descriptor `(2, 3, 1, 0)`, and the complete 75-byte frame in the
format document. Exercise that vector by composing only the existing standalone
LZW encoder, Adaptive encoder, and generic serializers. This decision specifies
bytes and a reserved name only; it does not publish a combined frame codec,
factory, CLI, benchmark, fuzz, completion, or interoperability claim.

## DD-317: LZW Adaptive validation stops at the packed-byte boundary first

- Date: 2026-07-21
- Status: accepted

Admit the first combined `lzw-adaptive-huffman` implementation as a strict
complete-frame validator only. Validate the stream profile, LZW parameters,
sequence, generic frame header, exact complete-frame extent, packed-code bound,
single 16-byte Adaptive descriptor, payload bound, every caller-owned capacity,
and the aggregate workspace limit before entropy output. Parse the Adaptive
descriptor before mutating packed-byte staging, then decode exactly the declared
packed extent and apply the existing LZW validator to that complete span.

Preserve deterministic error precedence as header and extent errors, workspace
errors, descriptor errors, Adaptive payload errors, then LZW code-stream errors.
The LZW pass owns width growth, references, `KwKwK`, final high-bit padding, and
exact declared raw size. Return the validated code count and diagnostic enums,
but reconstruct and publish no raw bytes at this boundary. Staging and phrase
records remain disposable scratch on every error. Later reconstruction,
encoding, streaming, public API, and completion steps must build on this same
validator rather than weakening or duplicating it.

## DD-318: LZW Adaptive reconstructs only into private raw staging

- Date: 2026-07-21
- Status: accepted

Extend the complete-frame boundary with a decoder that reconstructs the
already validated packed LZW stream into caller-owned private raw staging. Add
the raw extent to both the pre-decode capacity checks and aggregate workspace
accounting, so insufficient staging or policy limits fail before Adaptive
Huffman writes packed bytes. Require input, packed staging, and raw staging not
to overlap.

After the DD-317 validation succeeds, invoke the ordinary bounded LZW decoder
over exactly the validated packed extent, phrase-record prefix, and declared
raw extent. Preserve its stable validation, format, and decode diagnostics and
map an unexpected reconstruction failure to a distinct combined-layer error.
On every error the caller discards all staging. Successful raw bytes remain
private: this step adds no caller-visible output copy, streaming transform,
factory, CLI, benchmark, fuzz, completion, or interoperability claim.

## DD-319: LZW Adaptive publishes only a complete successful frame

- Date: 2026-07-21
- Status: accepted

Add the internal caller-visible complete-frame decoder on top of DD-318. Require
the full destination capacity together with packed staging, phrase records, and
private raw staging before Adaptive decoding begins. Validate both encoded
layers, reconstruct the exact declared raw extent privately, and copy that
complete span to output only after every operation succeeds. Output is not part
of internal workspace accounting because it is caller-visible destination
storage rather than scratch.

Preserve all previously assigned combined error values and append a distinct
output-capacity error. On header, descriptor, entropy, LZW validation,
reconstruction, capacity, or policy failure, publish no destination byte. This
admits an internal transactional frame decoder only; it does not yet add an
encoder, incremental stream transform, public C factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-320: LZW Adaptive planning freezes packed bytes before entropy output

- Date: 2026-07-21
- Status: accepted

Add the exact-frame planner and deterministic encoder for
`lzw-adaptive-huffman`. Require a nonempty raw frame matching the generic frame
contract. Compute and validate the caller-owned LZW encoder-entry prefix, plan
the complete variable-width code stream, check packed staging capacity, and
write those canonical bytes including final zero padding before Adaptive
Huffman planning. Record and cross-check the planned code count.

Count encoder entries, packed staging, the 16-byte Adaptive descriptor, and the
exact payload in the aggregate workspace limit. Validate the synthesized
generic header and complete serialized extent before returning a plan. Encoding
must repeat the deterministic Adaptive plan over the frozen packed span, reject
short output without changing it, then serialize the header, descriptor, and
payload. Append new diagnostics without changing earlier values. This step
adds no incremental transform, public C factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-321: LZW Adaptive streaming encoding buffers one bounded raw frame

- Date: 2026-07-21
- Status: accepted

Add the first bounded streaming encoder for `lzw-adaptive-huffman`. Serialize
the 64-byte stream header and 16-byte LZW parameters into a fixed prefix at
construction. Buffer at most one configured raw frame in caller-owned storage,
then invoke the DD-320 planner and encoder into separate caller-owned packed and
serialized-frame storage. Drain only already completed bytes; never expose a
partially constructed frame.

Derive the packed staging ceiling as checked `ceil(FW/8)` for the largest local
frame and validate raw, packed, LZW-entry, and encoded-frame capacities before
use. At frame preparation, account simultaneously for raw storage, actual
packed extent, exact serialized frame, and the used aligned encoder records.
Preserve `EndInput` while prefix or frame bytes drain, emit full frames as soon
as filled, leave a partial frame open on `Flush`, reject `ResetBlock`, and keep
terminal success and error sticky. Input/output chunking must reproduce the
DD-320 one-shot bytes exactly. This step adds no streaming decoder, public C
factory, CLI, benchmark, fuzz, completion, or interoperability claim.

## DD-322: LZW Adaptive streaming decoding validates before raw draining

- Date: 2026-07-21
- Status: accepted

Add the matching bounded streaming decoder. Collect the exact 80-byte prefix,
parse and validate the LZW/Adaptive profile and parameters, then collect each
56-byte frame header separately. Before body collection, validate sequence and
raw extents, checked `ceil(FW/8)` packed and `33S` payload bounds, descriptor
shape, every caller-owned capacity, complete serialized-frame extent, and the
aggregate bytes for encoded frame, packed staging, private raw staging, and
the used LZW phrase records.

Collect only the admitted body, invoke the DD-318 private-staging decoder on the
complete frame, and drain raw bytes only after that transaction succeeds. A
later malformed frame may not publish any of its bytes; earlier completed
frames remain committed. Preserve `EndInput` during raw draining, reject every
truncation and trailing byte, accept the exact empty prefix, reject
`ResetBlock` and unknown flags, and keep terminal errors and byte positions
sticky. This step adds no public C factory, CLI, benchmark, fuzz, completion,
or interoperability claim.

## DD-323: LZW Adaptive profile exposes checked typed workspace layouts

- Date: 2026-07-21
- Status: accepted

Define the bounded reference profile with a 65,536-byte default raw-frame
cadence, LZW variant 1, Adaptive Huffman variant 1, and the existing one-MiB
profile cap. For largest raw frame `F` and configured maximum code width `W`,
encoder sizing reserves checked `ceil(FW/8)` packed bytes, `33` payload bytes
per packed symbol, at most `min(F-1, code_capacity)` typed encoder entries, and
the complete serialized frame. Admit the configuration only when raw, packed,
serialized, typed, and aggregate extents satisfy all local limits.

Decoder sizing derives its complete-frame, packed, private-raw, and phrase
record ceilings only from validated local limits. Bound the packed region by
both the dictionary-serialization limit and Adaptive Huffman's one-MiB symbol
limit; derive the maximum phrase count from the minimum nine-bit LZW code width
and the greatest code capacity admitted by the local dictionary-entry limit.
Keep encoder and decoder records in separate single-type opaque regions.
Partition helpers must rederive exact byte count and alignment, reject altered
requirements, shortage, and misalignment, and accept only the canonical
zero-byte/one-alignment empty layout. This step adds no public C factory, CLI,
benchmark, fuzz, completion, or interoperability claim.

## DD-324: LZW Adaptive enters the C ABI with opaque typed views

- Date: 2026-07-21
- Status: accepted

Expose the fixed LZW variant 1 plus Adaptive Huffman variant 1 profile through
`marc_lzw_adaptive_huffman_config`, a direction-specific requirements query,
and an immutable-direction factory. Preserve known-size encoding and the common
three-workspace ABI. Primary storage holds raw frame input while encoding and a
complete serialized frame while decoding. Secondary storage contains packed
LZW staging followed by serialized-frame storage for encode, or packed staging
followed by private raw storage for decode.

The aligned views region remains opaque to C. It contains only LZW encoder
entries for encode and only LZW phrase entries for decode. Creation must rerun
the profile calculation and checked typed partition rather than trusting
caller-reported sizes. Require a strict C11 round trip, default initialization
and exact small-limit workspace checks, short and misaligned workspace
rejection, reserved-field rejection, and a null output handle on every factory
failure. This adds no allocator callback, unknown-size input, CLI, benchmark,
fuzz, completion, or interoperability claim.

## DD-325: LZW Adaptive completion is audited through the public C ABI

- Date: 2026-07-21
- Status: accepted

Audit only the published C configuration, requirements query, factory,
process, and destroy functions. Use 64-byte raw frames, the checked 128-byte
maximum packed-code region at width 16, the 33-byte-per-packed-symbol Adaptive
payload bound, 65,536 dictionary entries, and a 65,536-byte aggregate limit.
Allocate both direction-specific opaque views from queried byte counts and
alignment; encoding zero or one raw byte canonically requires no generated LZW
entry and therefore no views bytes. Cover empty input, every one-byte value,
the ordered byte alphabet, repeated data, binary patterns, deterministic
pseudo-random bytes, and lengths 63, 64, and 65. Require repeated encoding to
be byte-identical and terminal success to be sticky.

For a 193-byte four-frame stream, require exact bytes and round trips under
unchunked, one-byte, and mixed chunk schedules. Independently corrupt the final
frame sequence, truncate its final byte, and append trailing data. Every error
must be sticky, preserve byte and bit positions, publish exactly the first 192
validated bytes, and leave the last output sentinel unchanged. This completes
public-ABI evidence only; it adds no fuzz, CLI, benchmark, or interoperability
claim.

## DD-326: LZW Adaptive fuzzing fixes byte and phrase storage up front

- Date: 2026-07-21
- Status: accepted

Add one bounded decoder fuzz entry point that truncates supplied input to
8,192 bytes and exercises both the exact complete-frame private-staging
decoder after a valid 80-byte prefix and the incremental stream decoder for
every case. Fix total raw output at 4,096 bytes, one raw frame at 1,024 bytes,
packed LZW staging at 4,096 bytes, compressed payload at 8,192 bytes, the local
dictionary-entry limit at 4,096, and the phrase table at the 3,639 records
derivable from minimum nine-bit code density. Include every byte and typed
region in one fixed aggregate limit before processing metadata.

Derive partial input and output chunks from current bytes, cap process calls,
and abort only for an invalid process result or impossible stall. Retain a
repository-authored truncated-magic seed; generated mutations remain ignored
build artifacts. Add permanent regressions requiring every truncation of a
canonical `ABABX` stream, all-ones generic extent fields, and a nonzero
reserved Adaptive descriptor byte to fail atomically with sticky category and
position. Ordinary MSVC and Clang builds compile the harness; sanitizer fuzz
execution remains a separate explicitly bounded Clang workflow.

## DD-327: LZW Adaptive CLI uses the bounded reference profile

- Date: 2026-07-21
- Status: accepted

Publish `lzw-adaptive-huffman` as a command-line selector backed only by the
public C ABI. Use 65,536-byte raw frames and maximum LZW code width 16. Bound
the finalized packed-code region by 131,072 bytes, the Adaptive payload by
4,325,376 bytes, generated entries by 65,280, and aggregate internal bytes by
8 MiB. Obtain every direction-specific workspace extent and the opaque record
alignment from `marc_lzw_adaptive_huffman_workspace_requirements`; the CLI
must not reproduce either private record layout or workspace partition.

Retain the common transactional file contract: create only the exclusive
`.tmp` staging path, rename it after terminal success, and remove it after any
configuration, allocation, processing, malformed-stream, or commit failure.
Exercise the selector with the common multi-frame encode/decode round trip and
require strict rejection of appended trailing data before claiming CLI
publication. This step adds no benchmark or interoperability claim.

## DD-328: LZW Adaptive benchmark measures a verified public round trip

- Date: 2026-07-21
- Status: accepted

Add `lzw-adaptive-huffman` to the dependency-free benchmark through the same
public C ABI and 65,536-byte, width-16, 65,280-entry, 8-MiB policy as the CLI.
Reserve complete-stream encoded capacity with checked arithmetic from the
80-byte parameterized prefix, one 56-byte frame header and 16-byte Adaptive
descriptor per nonempty frame, and the conservative 66 payload bytes per raw
byte. Do not derive or reproduce either opaque LZW record layout; query both
direction-specific workspace sizes and alignments from the ABI.

Before timing, encode once, decode once, and require byte-exact equality with
the source. Time fresh transform instances for each iteration and report
encoded size, ratio, directional elapsed time and throughput, all six queried
workspace extents, and the larger caller-reserved workspace total. Treat only
correctness, bounds, allocation, and API failures as benchmark failures;
throughput and compression ratio are observations, not pass thresholds. Add a
single-iteration smoke test over repository-owned input. This step adds no
interoperability claim.

## DD-329: Interoperability schema 11 appends LZW Adaptive exactly once

- Date: 2026-07-21
- Status: accepted

Define interoperability schema 11 and codec set `marc-cli-v11` as the exact
twenty-one-entry schema-10 order followed by `lzw-adaptive-huffman`. Reuse the
unchanged deterministic 8,193-byte binary fixture. Generation must round-trip
all twenty-two profiles before writing the manifest; verification must check
the exact count and order, every declared size and SHA-256 value, foreign
decode equality, and byte-identical local re-encoding.

Keep schemas 1 through 10 frozen. The compatibility test starts from a complete
schema-11 bundle, rejects a reordered schema-11 manifest, removes only the
newest profile while converting to schema 10, and continues the existing
one-generation-at-a-time chain through schema 1. Local schema admission proves
the generator, verifier, compatibility rules, and same-architecture compiler
determinism only. Cross-platform evidence still requires CI artifacts from the
same full Git revision and the established bidirectional external verification
procedure.

## DD-330: LZD Adaptive entropizes finalized reference pairs

- Date: 2026-07-22
- Status: accepted

Reserve `lzd-adaptive-huffman` for LZD variant 1 followed by Adaptive Huffman
FGK variant 1 under format version 1.0. Preserve the standalone 16-byte LZD
parameters, empty entropy parameters, fixed eight-byte little-endian reference
pairs, and terminal absent-right value. Complete the token stream before
entropy processing; Adaptive Huffman consumes every byte without interpreting
token or reference-field boundaries. Reset both dictionaries at every outer
frame.

For raw frame size `F`, use the checked token bound
`S = 8 * ceil(F / 2)` and Adaptive payload bound `33S`. Bound generated phrase
records by `min(floor(F/2), configured_maximum, local_limit)` and the iterative
expansion stack by that count plus one. The reference profile uses
`F = 65,536`, so `S = 262,144`, payload is at most 8,650,752 bytes, generated
phrases are at most 32,768, and expansion references are at most 32,769.
Encoding freezes canonical tokens before Adaptive planning. Decoding
entropy-decodes into token staging, validates the complete backward phrase
graph and terminal rule, derives the exact raw extent, reconstructs privately,
and only then publishes.

Freeze the raw-`A` vector independently: LZD token
`41 00 00 00 FF FF FF FF`, Adaptive payload `41 00 CC 3F 1D`, descriptor
`(8, 5, 5, 0)`, and the complete 77-byte frame in the format document.
Exercise that vector by composing only the existing standalone LZD encoder,
Adaptive encoder, and generic serializers. This decision specifies bytes and
a reserved name only; it does not publish a combined frame codec, factory,
CLI, benchmark, fuzz, completion, or interoperability claim.

## DD-331: LZD Adaptive validation stops at canonical tokens first

- Date: 2026-07-22
- Status: accepted

Admit the first combined `lzd-adaptive-huffman` implementation as a strict
complete-frame validator only. Validate the stream profile, LZD parameters,
sequence, generic frame header, exact complete-frame extent, checked
`8*ceil(F/2)` token bound, multiple-of-eight token shape, single 16-byte
Adaptive descriptor, `33S` payload bound, every caller-owned capacity, and the
aggregate workspace limit before entropy output. Parse the descriptor before
mutating token staging, decode exactly the declared token extent, then apply
the existing LZD validator to that complete span.

Preserve deterministic error precedence as header and extent errors, workspace
errors, descriptor errors, Adaptive payload errors, then LZD grammar errors.
The LZD pass owns backward-reference ordering, checked phrase lengths,
dictionary freeze, the final absent-right rule, and exact declared raw size.
Return token and phrase counts plus layer diagnostics, but reconstruct and
publish no raw bytes. Staging and phrase records remain disposable scratch on
every error. Later reconstruction, encoding, streaming, public API, and
completion steps must consume this validator rather than weaken or duplicate
it.

## DD-332: LZD Adaptive reconstructs only into private raw staging

- Date: 2026-07-22
- Status: accepted

Extend the complete-frame boundary with a decoder that reconstructs the
already validated LZD token stream into caller-owned private raw staging. Add
the raw extent and a conservative expansion workspace of
`phrase_workspace_entries + 1` references to both pre-decode capacity checks
and aggregate workspace accounting, so insufficient staging or policy limits
fail before Adaptive Huffman writes token bytes. Require input, token staging,
and raw staging not to overlap.

After DD-331 validation succeeds, invoke the ordinary bounded iterative LZD
decoder over exactly the validated token extent, phrase-record prefix,
expansion-reference prefix, and declared raw extent. Preserve its stable
validation, format, and decode diagnostics and map an unexpected reconstruction
failure to a distinct combined-layer error. On every error the caller discards
all staging. Successful raw bytes remain private: this step adds no caller-
visible output copy, streaming transform, factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-333: LZD Adaptive publishes only a complete successful frame

- Date: 2026-07-22
- Status: accepted

Add the internal caller-visible complete-frame decoder on top of DD-332.
Require full destination capacity together with token staging, phrase records,
expansion references, and private raw staging before Adaptive decoding begins.
Validate both encoded layers, reconstruct the exact declared raw extent
privately, and copy that complete span to output only after every operation
succeeds. Output is not part of internal workspace accounting because it is
caller-visible destination storage rather than scratch.

Preserve all previously assigned combined error values and append a distinct
output-capacity error. On header, descriptor, entropy, LZD validation,
reconstruction, capacity, or policy failure, publish no destination byte. This
admits an internal transactional frame decoder only; it does not yet add an
encoder, incremental stream transform, public C factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-334: LZD Adaptive encoding freezes canonical tokens first

- Date: 2026-07-22
- Status: accepted

Add the internal exact-frame planner and encoder as the inverse of DD-333.
Run the ordinary deterministic LZD planner, require its complete typed encoder
workspace and canonical token capacity, serialize the entire token sequence
into private staging, and only then let a fresh Adaptive Huffman FGK model plan
those exact bytes. Count typed encoder records, token staging, the fixed
descriptor, and exact entropy payload against the aggregate workspace limit.

Return the complete serialized extent without touching caller-visible output.
The encoder repeats Adaptive planning over the frozen token span, requires an
identical payload extent, and rejects insufficient serialized destination
capacity before writing the generic header, descriptor, or payload. Preserve
all previously assigned combined error values and append encoding-specific
categories. Require byte identity with DD-330's independent raw-`A` vector,
determinism and round trip for phrase references, and sentinel preservation on
capacity failures. This remains an internal complete-frame API; streaming,
public factory, CLI, benchmark, fuzz, completion, and interoperability work
remain separate admissions.

## DD-335: LZD Adaptive streaming encoding buffers one bounded raw frame

- Date: 2026-07-22
- Status: accepted

Add the first incremental encoder for `lzd-adaptive-huffman` as a bounded
adapter over DD-334. Serialize the 64-byte stream header and 16-byte LZD
parameters at construction. Require caller-owned storage for the largest raw
frame, its checked `8*ceil(F/2)` canonical token ceiling, a complete serialized
frame, and the exact typed LZD encoder-entry prefix. Count all four used regions
against the aggregate internal-buffer limit before encoding each frame.

Drain the immutable 80-byte prefix before collecting input, buffer exactly one
outer frame, invoke the exact-frame planner and encoder, and drain that complete
frame before accepting the next one. Preserve a valid `EndInput` observed
during prefix or frame output starvation. `Flush` may expose already prepared
bytes but must not close or alter a partial frame; reject `ResetBlock`, unknown
flags, premature `EndInput`, and input beyond declared original size with
stable terminal errors. Require byte identity with concatenated one-shot frames
under one-byte buffers and sticky `EndOfStream`. This remains internal;
streaming decode, public factory, completion, fuzz, CLI, benchmark, and
interoperability are separate admissions.

## DD-336: LZD Adaptive streaming decode publishes only validated frames

- Date: 2026-07-22
- Status: accepted

Add the matching incremental decoder as a bounded adapter over DD-332. Collect
and validate the exact 80-byte stream prefix before accepting frame headers.
For each header, check the `8*ceil(F/2)` token ceiling, eight-byte token shape,
`33S` payload ceiling, phrase records, phrase-count-plus-one expansion stack,
complete encoded-frame storage, private raw storage, and their aggregate bytes
before collecting the body or starting entropy output.

Buffer one complete encoded frame, invoke the private-staging complete-frame
decoder, and enter raw draining only after Adaptive, LZD grammar, and iterative
reconstruction all succeed. Retain `EndInput` while validated raw bytes drain.
Reject every prefix or frame truncation, data after the declared final frame,
`ResetBlock`, and unknown flags with sticky stable errors. Earlier successful
frames may already be committed, but a failing frame must publish no byte.
Require one-byte input/output equivalence, all truncation positions, empty
stream handling, later-frame atomic corruption, and every caller-workspace and
aggregate limit. This remains internal; public factory, completion, fuzz, CLI,
benchmark, and interoperability are separate admissions.

## DD-337: LZD Adaptive profile exposes bytes, not C++ layouts

- Date: 2026-07-22
- Status: accepted

Add a bounded internal profile for the fixed LZD variant 1 plus Adaptive
Huffman FGK variant 1 composition. For encoding, derive the largest actual raw
frame, checked `8*ceil(F/2)` token ceiling, `33S` payload ceiling, complete
serialized-frame ceiling, and LZD encoder-entry count. Reject any individual or
aggregate extent beyond the configured limits before publishing requirements;
empty input uses zero bytes and neutral alignment.

For decoding, derive conservative encoded-frame, token, private raw, phrase,
and phrase-count-plus-one expansion capacities from local hard limits. Pack
phrase records followed by an explicitly aligned `uint32_t` expansion region
inside one opaque allocation. Partition functions must recompute and compare
every count-derived byte extent, offset, total, and alignment before producing
typed spans; reject altered requirements, short storage, and misaligned bases.
Map profile failures to stable core errors. This step exposes no C ABI or
factory; public admission, completion, fuzz, CLI, benchmark, and
interoperability remain separate.

## DD-338: LZD Adaptive enters the C ABI with coupled opaque views

- Date: 2026-07-22
- Status: accepted

Expose the fixed LZD variant 1 plus Adaptive Huffman variant 1 profile through
`marc_lzd_adaptive_huffman_config`, a direction-specific requirements query,
and an immutable-direction factory. Preserve known-size encoding and the common
three-workspace ABI. Primary storage holds raw frame input while encoding and a
complete serialized frame while decoding. Secondary storage contains canonical
LZD token staging followed by serialized-frame storage for encode, or token
staging followed by private raw storage for decode.

Keep the aligned views region opaque to C. It contains LZD encoder entries for
encode; for decode it contains phrase entries followed by an explicitly aligned
bounded `uint32_t` expansion stack. Creation must rerun the profile calculation
and checked partition rather than trusting caller-reported extents. Require a
strict C11 round trip, exact small-limit workspace checks, short and misaligned
workspace rejection, reserved-field rejection, and a null output handle on
every factory failure. This adds no allocator callback, unknown-size input,
CLI, benchmark, fuzz, completion, or interoperability claim.

## DD-339: LZD Adaptive completion is audited through the public C ABI

- Date: 2026-07-22
- Status: accepted

Audit only the published C configuration, requirements query, factory,
process, and destroy functions. Use 64-byte raw frames, the checked 256-byte
maximum canonical-token region, the 33-byte-per-token-byte Adaptive payload
bound, 32 dictionary entries, and a 65,536-byte aggregate limit. Allocate both
direction-specific opaque views from queried byte counts and alignment;
encoding zero or one raw byte requires no generated LZD phrase entry and
therefore no views bytes. Cover empty input, every one-byte value, the ordered
byte alphabet, repeated data, binary patterns, deterministic pseudo-random
bytes, and lengths 63, 64, and 65. Require repeated encoding to be byte-
identical and terminal success to be sticky.

For a 193-byte four-frame stream, require exact bytes and round trips under
unchunked, one-byte, and mixed chunk schedules. Independently corrupt the final
frame sequence, truncate its final byte, and append trailing data. Every error
must be sticky, preserve byte and bit positions, publish exactly the first 192
validated bytes, and leave the last output sentinel unchanged. This completes
public-ABI evidence only; it adds no fuzz, CLI, benchmark, or interoperability
claim.

## DD-340: LZD Adaptive fuzzing fixes phrase and expansion storage up front

- Date: 2026-07-22
- Status: accepted

Add one bounded decoder fuzz entry point that truncates supplied input to
8,192 bytes and exercises both the exact complete-frame private-staging
decoder after a valid 80-byte prefix and the incremental stream decoder for
every case. Fix total raw output at 4,096 bytes, one raw frame at 1,024 bytes,
canonical LZD token staging at 4,096 bytes, compressed payload at 8,192 bytes,
the phrase table at 512 records, and the iterative expansion stack at 513
references. Include every byte and typed region in one fixed aggregate limit
before processing metadata.

Derive partial input and output chunks from current bytes, cap process calls,
and abort only for an invalid process result or impossible stall. Retain a
repository-authored truncated-magic seed; generated mutations remain ignored
build artifacts. Add permanent regressions requiring every truncation of a
canonical `ABABX` stream, all-ones generic extent fields, and a nonzero
reserved Adaptive descriptor byte to fail atomically with sticky category and
position. Ordinary MSVC and Clang builds compile the harness; sanitizer fuzz
execution remains a separate explicitly bounded Clang workflow.

## DD-341: LZD Adaptive CLI delegates all codec storage to the public ABI

- Date: 2026-07-22
- Status: accepted

Publish `lzd-adaptive-huffman` as a transactional command-line selector over
the DD-338 C requirements query and factory. Fix the CLI profile to 65,536 raw
bytes per frame, 262,144 canonical token bytes, an 8,650,752-byte Adaptive
payload ceiling, 65,536 dictionary entries, and a 16-MiB aggregate internal
limit. Query the exact primary, secondary, and opaque aligned-view extents for
the selected direction; do not reproduce a private C++ record layout in the
tool.

Retain the common temporary-output transaction so malformed input, including
an appended trailing byte, cannot commit a destination file. Require a multi-
frame round trip and strict trailing-data rejection through the public factory.
This admission adds no format variant, benchmark registry entry, or
interoperability schema entry.

## DD-342: LZD Adaptive benchmark measures a verified public round trip

- Date: 2026-07-22
- Status: accepted

Add `lzd-adaptive-huffman` to the dependency-free benchmark through the same
public C ABI and 65,536-byte, 65,536-entry, 16-MiB policy as the CLI. Reserve
complete-stream encoded capacity with checked arithmetic from the 80-byte
parameterized prefix, one 56-byte frame header and 16-byte Adaptive descriptor
per nonempty frame, and `264*ceil(total_raw_bytes/2)` payload bytes so an odd
final frame retains its complete terminal token ceiling. Query
both direction-specific workspace sizes and opaque-view alignments from the ABI;
do not reproduce LZD encoder, phrase, or expansion layouts.

Before timing, encode once, decode once, and require byte-exact equality with
the source. Time fresh transform instances per iteration and report encoded
size, ratio, directional elapsed time and throughput, all six queried workspace
extents, and the larger caller-reserved total. Treat correctness, checked-bound,
allocation, and API failures as benchmark failures, but impose no performance
threshold. Add a one-iteration smoke test over repository-owned input. This step
adds no interoperability claim.

## DD-343: Interoperability schema 12 appends LZD Adaptive exactly once

- Date: 2026-07-22
- Status: accepted

Define interoperability schema 12 and codec set `marc-cli-v12` as the exact
twenty-two-entry schema-11 order followed by `lzd-adaptive-huffman`. Reuse the
unchanged deterministic 8,193-byte binary fixture. Generation must round-trip
all twenty-three profiles before writing the manifest; verification must check
the exact count and order, every declared size and SHA-256 value, foreign decode
equality, and byte-identical local re-encoding.

Keep schemas 1 through 11 frozen. The compatibility test starts from a complete
schema-12 bundle, rejects a reordered schema-12 manifest, removes only the
newest profile while converting to schema 11, and continues the existing
one-generation-at-a-time chain through schema 1. Local schema admission proves
the generator, verifier, compatibility rules, and same-machine CLI determinism
only. Cross-platform evidence still requires CI artifacts from the same full
Git revision and the established four-direction external verification procedure.

## DD-344: LZMW Adaptive entropizes finalized references

- Date: 2026-07-22
- Status: accepted

Reserve `lzmw-adaptive-huffman` for LZMW variant 1 followed by Adaptive
Huffman FGK variant 1 under format version 1.0. Preserve the standalone
16-byte LZMW parameters, empty entropy parameters, and fixed four-byte
little-endian references. Complete the deterministic LZMW parse before
entropy processing; Adaptive Huffman consumes every byte without interpreting
reference boundaries. Reset both dictionaries at every outer frame.

For raw frame size `F`, use checked token bound `S = 4F` and Adaptive payload
bound `33S = 132F`. Bound generated phrase records by
`min(max(F-1, 0), configured_maximum, local_limit)` and the iterative expansion
stack by that count plus one for a nonempty frame. The reference profile uses
`F = 65,536`, so `S = 262,144`, payload is at most 8,650,752 bytes, generated
phrases are at most 65,535, and expansion references are at most 65,536.
Encoding freezes canonical references before Adaptive planning. Decoding
entropy-decodes into token staging, validates the complete adjacent-phrase
graph and exact raw extent, reconstructs privately, and only then publishes.

Freeze the raw-`A` vector independently: LZMW reference
`41 00 00 00`, Adaptive payload `41 00 0C`, descriptor `(4, 3, 4, 0)`, and
the complete 75-byte frame in the format document. Exercise that vector by
composing only the existing standalone LZMW encoder, Adaptive encoder, and
generic serializers. This decision specifies bytes and a reserved name only;
it does not publish a combined frame codec, factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-345: LZMW Adaptive validation stops at canonical references first

- Date: 2026-07-22
- Status: accepted

Admit the first combined `lzmw-adaptive-huffman` implementation as a strict
complete-frame validator only. Validate the stream profile, LZMW parameters,
sequence, generic frame header, exact complete-frame extent, checked `4F`
reference bound, multiple-of-four reference shape, single 16-byte Adaptive
descriptor, `33S` payload bound, every caller-owned capacity, and aggregate
workspace before entropy output. Parse the descriptor before mutating reference
staging, decode exactly the declared reference extent, then apply the existing
LZMW validator to that complete span.

Preserve deterministic error precedence as header and extent errors, workspace
errors, descriptor errors, Adaptive payload errors, then LZMW grammar errors.
The LZMW pass owns literal and generated-reference validity, adjacent-phrase
construction, checked phrase lengths, dictionary freeze, and exact declared raw
size. Return maximum phrase capacity, actual token and generated-phrase counts,
and the resulting expansion-stack ceiling plus layer diagnostics, but
reconstruct and publish no raw bytes. Later reconstruction, encoding, streaming,
public API, and completion steps must consume this validator rather than weaken
or duplicate it.

## DD-346: LZMW Adaptive reconstructs only into private raw staging

- Date: 2026-07-22
- Status: accepted

Extend the complete-frame boundary with a decoder that reconstructs the already
validated LZMW reference stream into caller-owned private raw staging. Before
Adaptive decoding begins, require the complete raw extent and a conservative
expansion workspace derived from the maximum admitted phrase-record count plus
one for a nonempty frame. Count both regions in aggregate workspace and require
input, reference staging, and raw staging not to overlap.

After DD-345 validation succeeds, reduce the reported expansion extent to the
actual generated-phrase count plus one and invoke the ordinary bounded iterative
LZMW decoder over exactly the validated reference extent, phrase-record prefix,
expansion-reference prefix, and declared raw extent. Preserve its stable
validation, format, and decode diagnostics and map an unexpected reconstruction
failure to a distinct combined-layer error. On every error the caller discards
all staging. Successful raw bytes remain private: this step adds no caller-
visible output copy, streaming transform, factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-347: LZMW Adaptive publishes only a complete successful frame

- Date: 2026-07-22
- Status: accepted

Add the internal caller-visible complete-frame decoder on top of DD-346.
Require full destination capacity together with reference staging, phrase
records, expansion references, and private raw staging before Adaptive decoding
begins. Validate both encoded layers, reconstruct the exact declared raw extent
privately, and copy that complete span to output only after every operation
succeeds. Output is not part of internal workspace accounting because it is
caller-visible destination storage rather than scratch.

Preserve all previously assigned combined error values and append a distinct
output-capacity error. On header, descriptor, entropy, LZMW validation,
reconstruction, capacity, or policy failure, publish no destination byte. This
admits an internal transactional frame decoder only; it does not yet add an
encoder, incremental stream transform, public C factory, CLI, benchmark, fuzz,
completion, or interoperability claim.

## DD-348: LZMW Adaptive encoding freezes canonical references first

- Date: 2026-07-22
- Status: accepted

Add the internal exact-frame planner and encoder as the inverse of DD-347. Run
the ordinary deterministic LZMW planner, require its complete typed encoder
workspace and canonical reference capacity, serialize the entire reference
sequence into private staging, and only then let a fresh Adaptive Huffman FGK
model plan those exact bytes. Count typed encoder records, reference staging,
the fixed descriptor, and exact entropy payload against the aggregate workspace
limit.

Return the complete serialized extent without touching caller-visible output.
The encoder repeats Adaptive planning over the frozen reference span, requires
an identical payload extent, and rejects insufficient serialized destination
capacity before writing the generic header, descriptor, or payload. Preserve
all previously assigned combined error values and append encoding-specific
categories. Require byte identity with DD-344's independent raw-`A` vector,
determinism and round trip for generated references, and sentinel preservation
on capacity failures. This remains an internal complete-frame API; streaming,
public factory, CLI, benchmark, fuzz, completion, and interoperability work
remain separate admissions.

## DD-349: LZMW Adaptive streaming encoding buffers one bounded raw frame

- Date: 2026-07-22
- Status: accepted

Add the first incremental encoder for `lzmw-adaptive-huffman` as a bounded
adapter over DD-348. Serialize the 64-byte stream header and 16-byte LZMW
parameters at construction. Require caller-owned storage for the largest raw
frame, its checked `4F` canonical reference ceiling, a complete serialized
frame, and the exact typed LZMW encoder-entry prefix. Count all four used
regions against the aggregate internal-buffer limit before encoding each frame.

Drain the immutable 80-byte prefix before collecting input, buffer exactly one
outer frame, invoke the exact-frame planner and encoder, and drain that complete
frame before accepting the next one. Preserve a valid `EndInput` observed
during prefix or frame output starvation. `Flush` may expose already prepared
bytes but must not close or alter a partial frame; reject `ResetBlock`, unknown
flags, premature `EndInput`, and input beyond declared original size with
stable terminal errors. Require byte identity with concatenated one-shot frames
under one-byte buffers and sticky `EndOfStream`. This remains internal;
streaming decode, public factory, completion, fuzz, CLI, benchmark, and
interoperability are separate admissions.

## DD-350: LZMW Adaptive streaming decode publishes only validated frames

- Date: 2026-07-22
- Status: accepted

Add the matching incremental decoder as a bounded adapter over DD-346. Collect
the 80-byte prefix, then each 56-byte generic frame header before admitting its
complete descriptor and payload extent. At header admission, enforce the `4F`
reference ceiling, four-byte alignment, one Adaptive descriptor, `33S` payload
ceiling, exact caller capacities, and the sum of encoded frame, reference,
private raw, phrase-record, and expansion-stack storage.

Collect one complete serialized frame, invoke the private-staging decoder, and
only then drain the validated raw frame. A malformed later frame may not publish
any byte of that frame or retract earlier completed output. Preserve a valid
`EndInput` while raw output drains; reject every truncation, trailing byte,
unknown flag, and `ResetBlock` with a sticky position-stable terminal error.
Require one-byte input/output round trip and all proper-prefix rejection. This
remains internal; profile, public factory, completion, fuzz, CLI, benchmark,
and interoperability are separate admissions.

## DD-351: LZMW Adaptive profile exposes bytes, not C++ layouts

- Date: 2026-07-22
- Status: accepted

Add the internal bounded profile for the completed streaming pair. For encode,
derive the largest raw frame, `4F` canonical reference staging, the 56-byte
header plus 16-byte descriptor plus conservative `33S` payload, and
`min(max(F-1,0), configured maximum)` LZMW encoder records. For decode, derive
the admitted encoded-frame and private raw byte regions, reference staging,
`min(max(S/4-1,0), decoder maximum)` phrase records, and one additional
nonempty expansion reference.

Expose the typed region requirement only as total bytes and maximum alignment.
Internally rederive every count, aligned offset, and total extent before casting
caller-owned bytes to encoder entries, phrase records, or expansion references.
Reject inconsistent requirements, short storage, misalignment, and arithmetic
overflow before publishing any span. Empty encode views require zero bytes and
alignment one. This admits no C ABI or public codec yet; public requirements,
factory construction, completion, fuzz, CLI, benchmark, and interoperability
remain separate steps.

## DD-352: LZMW Adaptive enters the C ABI with coupled opaque views

- Date: 2026-07-22
- Status: accepted

Expose the fixed LZMW variant 1 plus Adaptive Huffman FGK variant 1 profile
through `marc_lzmw_adaptive_huffman_config`, a direction-specific requirements
query, and an immutable-direction factory. Preserve known-size encoding and the
common three-workspace ABI. Primary storage holds raw frame input while
encoding and a complete serialized frame while decoding. Secondary storage
contains canonical LZMW reference staging followed by serialized-frame storage
for encode, or reference staging followed by private raw storage for decode.

Keep the aligned views region opaque to C. It contains LZMW encoder entries for
encode; for decode it contains phrase entries followed by an explicitly aligned
bounded `uint32_t` expansion stack. Creation must rerun the profile calculation
and checked partition instead of trusting queried extents. Require a strict C11
round trip, exact small-limit workspace checks, short and misaligned workspace
rejection, reserved-field rejection, and a null output handle on every factory
failure. This adds no allocator callback, unknown-size input, completion, fuzz,
CLI, benchmark, or interoperability claim.

## DD-353: LZMW Adaptive completion is audited through the public C ABI

- Date: 2026-07-22
- Status: accepted

Audit only the published C configuration, requirements query, factory,
process, and destroy functions. Use 64-byte raw frames, the checked 256-byte
maximum canonical-reference region, the 33-byte-per-reference-byte Adaptive
payload bound, 63 dictionary entries, and a 65,536-byte aggregate limit.
Allocate both direction-specific opaque views from queried byte counts and
alignment; encoding zero or one raw byte requires no generated LZMW entry and
therefore no views bytes. Cover empty input, every one-byte value, the ordered
byte alphabet, repeated data, binary patterns, deterministic pseudo-random
bytes, and lengths 63, 64, and 65. Require repeated encoding to be byte-
identical and terminal success to be sticky.

For a 193-byte four-frame stream, require exact bytes and round trips under
unchunked, one-byte, and mixed chunk schedules. Independently corrupt the final
frame sequence, truncate its final byte, and append trailing data. Every error
must be sticky, preserve byte and bit positions, publish exactly the first 192
validated bytes, and leave the last output sentinel unchanged. This completes
public-ABI evidence only; it adds no fuzz, CLI, benchmark, or interoperability
claim.

## DD-354: LZMW Adaptive fuzzing fixes reference-derived typed storage

- Date: 2026-07-22
- Status: accepted

Add one bounded decoder fuzz entry point that truncates supplied input to
8,192 bytes and exercises both the exact complete-frame private-staging
decoder after a valid 80-byte prefix and the incremental stream decoder for
every case. Fix total raw output at 4,096 bytes, one raw frame at 1,024 bytes,
canonical LZMW reference staging at 4,096 bytes, compressed payload at 8,192
bytes, the phrase table at 1,023 records, and the iterative expansion stack at
1,024 references. Include every byte and typed region in one fixed aggregate
limit before processing metadata.

Derive partial input and output chunks from current bytes, cap process calls,
and abort only for an invalid process result or impossible stall. Retain a
repository-authored truncated-magic seed; generated mutations remain ignored
build artifacts. Add permanent regressions requiring every truncation of a
canonical `ABABX` stream, all-ones generic extent fields, and a nonzero
reserved Adaptive descriptor byte to fail atomically with sticky category and
position. Ordinary MSVC and Clang builds compile the harness; sanitizer fuzz
execution remains a separate explicitly bounded Clang workflow.

## DD-355: LZMW Adaptive CLI delegates typed storage to the public ABI

- Date: 2026-07-23
- Status: accepted

Publish `lzmw-adaptive-huffman` as a transactional command-line selector over
the DD-352 C requirements query and factory. Fix the CLI profile to 65,536 raw
bytes per frame, 262,144 canonical reference bytes, an 8,650,752-byte Adaptive
payload ceiling, 65,536 dictionary entries, and a 16-MiB aggregate internal
limit. Query the exact primary, secondary, and opaque aligned-view extents for
the selected direction; do not reproduce a private C++ record layout in the
tool.

Retain the common temporary-output transaction so malformed input, including
an appended trailing byte, cannot commit a destination file. Require a multi-
frame round trip and strict trailing-data rejection through the public factory.
This admission adds no format variant, benchmark registry entry, or
interoperability schema entry.

## DD-356: LZMW Adaptive benchmark verifies before measuring

- Date: 2026-07-23
- Status: accepted

Add `lzmw-adaptive-huffman` to the dependency-free benchmark through the same
public C ABI and 65,536-byte, 65,536-entry, 16-MiB policy as the CLI. Reserve
complete-stream encoded capacity with checked arithmetic from the 80-byte
parameterized prefix, one 56-byte frame header and 16-byte Adaptive descriptor
per nonempty frame, and `132*total_raw_bytes` payload bytes. Query both
direction-specific workspace sizes and opaque-view alignments from the ABI; do
not reproduce LZMW encoder, phrase, or expansion layouts.

Before timing, encode once, decode once, and require byte-exact equality with
the source. Time fresh transform instances per iteration and report encoded
size, ratio, directional elapsed time and throughput, all six queried workspace
extents, and the larger caller-reserved total. Treat correctness, checked-bound,
allocation, and API failures as benchmark failures, but impose no performance
threshold. Add a one-iteration smoke test over repository-owned input. This
step adds no interoperability claim.

## DD-357: Interoperability schema 13 appends LZMW Adaptive exactly once

- Date: 2026-07-23
- Status: accepted

Define interoperability schema 13 and codec set `marc-cli-v13` as the exact
twenty-three-entry schema-12 order followed by `lzmw-adaptive-huffman`. Reuse
the unchanged deterministic 8,193-byte binary fixture. Generation must round-
trip all twenty-four profiles before writing the manifest; verification must
check the exact count and order, every declared size and SHA-256 value, foreign
decode equality, and byte-identical local re-encoding.

Keep schemas 1 through 12 frozen. The compatibility test starts from a complete
schema-13 bundle, rejects a reordered schema-13 manifest, removes only the
newest profile while converting to schema 12, and continues the existing one-
generation-at-a-time chain through schema 1. Local schema admission proves the
generator, verifier, compatibility rules, and same-machine CLI determinism
only. Cross-platform evidence still requires CI artifacts from the same full
Git revision and the established four-direction external verification
procedure.

## DD-358: Project version 0.1.1 preserves existing contracts

- Date: 2026-07-23
- Status: accepted

Release the completed Adaptive Huffman composition column as project version
`0.1.1`. Treat project, stream-format, C ABI, and interoperability schema
versions as independent namespaces. This release adds named public profiles,
but retains C ABI version 1 and does not change the representation or
deterministic encoded bytes of any previously published stream variant.

Define the pre-1.0 `0.1.x` project line as compatibility-preserving additions
and fixes. Reserve `0.2.0` for work that may introduce incompatible API or
default changes, or separately identified format variants motivated by speed
or compression-ratio improvements. A later project version must still never
silently reuse an existing algorithm or variant ID for different bytes. Make
the installed CMake package enforce this boundary with `SameMinorVersion`
rather than treating every project version whose major component is zero as
compatible.

## DD-359: LZ77 Dynamic Range entropizes canonical token bytes

- Date: 2026-07-23
- Status: accepted

Reserve `lz77-dynamic-range` for LZ77 variant 1 followed by Dynamic Range Coder
variant 1 under format version 1.0. Preserve the standalone 16-byte LZ77
parameter region, use no entropy parameter bytes, and set stream entropy block
size to zero. Complete the canonical LZ77 token stream before range coding;
the adaptive order-0 model consumes every token byte without interpreting
token fields and resets together with LZ77 history at each outer frame.

For raw frame size `F`, require canonical token extent `S` to be a nonzero
multiple of 16 with `S <= 16F` and `S <= 2^24`. Bound range payload extent by
`P <= 2S + 5`. The format-level raw-frame maximum is therefore 2^20 bytes;
the reference profile uses 65,536-byte frames so token staging, payload
staging, private raw reconstruction, and their checked aggregate remain within
the baseline memory policy.

Store exactly one 16-byte Dynamic Range descriptor and one byte-aligned payload
per nonempty frame. Require descriptor symbol count `S`, payload count `P`, one
entropy block, and the generic header's exact extents. Decode the complete
range payload into bounded private token staging, strictly validate all LZ77
tokens and exact raw extent, reconstruct into separate bounded private raw
staging, and publish only after every stage succeeds. This step reserves the
format and independent vector only; it publishes no factory or CLI selector.

## DD-360: LZ77 Dynamic Range validation stops at private tokens

- Date: 2026-07-23
- Status: accepted

Implement the first executable combined boundary as a bounded complete-frame
validator. Accept only the exact LZ77 variant 1 and Dynamic Range variant 1
pipeline, a raw frame no larger than 2^20 bytes, one 16-byte descriptor, one
entropy block, token extent `S` that is nonzero, divisible by 16 and no larger
than both `16F` and 2^24, and payload extent no larger than `2S + 5`.

Reject truncation, trailing frame bytes, impossible extents, insufficient token
staging, and descriptor-plus-payload-plus-staging aggregate policy failures
before entropy output. Then strictly parse the range descriptor, decode exactly
`S` bytes with canonical initialization and exact payload exhaustion into
private staging, and validate the complete LZ77 token graph and derived raw
extent. Preserve the component error categories in the result. Do not
reconstruct or publish raw bytes in this step.

## DD-361: LZ77 Dynamic Range reconstructs only validated tokens

- Date: 2026-07-23
- Status: accepted

Extend the complete-frame boundary with a private raw-staging decoder. Check
raw capacity and include raw bytes in the descriptor, payload, token-staging,
and raw-staging aggregate before entropy output. Reuse the complete combined
validator so Dynamic Range decoding and the entire LZ77 graph and derived raw
extent succeed before reconstruction begins.

Apply marc's existing iterative LZ77 decoder to the validated private token
region, including bytewise overlapping-copy semantics, and require exactly the
declared raw extent. Preserve nested validation, format, and decode errors in
the result. This function may mutate only its private raw staging on success;
it has no caller-visible output and adds no stream, C ABI, or CLI surface.

## DD-362: LZ77 Dynamic Range publishes only complete frames

- Date: 2026-07-23
- Status: accepted

Add a transactional caller-visible complete-frame boundary above the private
raw decoder. Check caller output capacity before entropy output or mutation of
either private staging region. Decode and validate the complete Dynamic Range
and LZ77 layers, reconstruct exactly the declared raw extent into private raw
staging, and only then copy that extent once to caller output.

Every capacity, descriptor, entropy, token-validation, or reconstruction
failure leaves caller output unchanged. Caller output is destination storage,
not internal buffered workspace, so it is capacity-checked but is not added to
the aggregate internal-buffer limit. This step adds no streaming transform,
public C ABI factory, CLI selector, or encoder.

## DD-363: LZ77 Dynamic Range plans from frozen token bytes

- Date: 2026-07-23
- Status: accepted

Implement exact-frame planning by first planning and encoding the complete
canonical LZ77 token stream into caller-owned private staging. Only after those
bytes are fixed may Dynamic Range variant 1 plan its descriptor and exact
payload extent. Validate the generic frame header and the checked descriptor,
payload, and token-staging aggregate before reporting the serialized extent.

The deterministic frame encoder must complete that plan and reject a short
serialized destination before writing any destination byte. It then replans the
unchanged staged token bytes, requires the same payload extent, and serializes
the generic header, Dynamic Range descriptor, and payload in their specified
order. Preserve component encode errors for diagnosis and treat disagreement
after a successful plan as an internal error. This step adds no streaming or
public C ABI surface.

## DD-364: LZ77 Dynamic Range streaming retains complete encoded frames

- Date: 2026-07-23
- Status: accepted

Add a bounded streaming encoder that owns caller-supplied raw-frame, canonical
token, and serialized-frame regions. Emit the stream header and LZ77 parameter
prefix first. Collect exactly the configured outer-frame extent, invoke the
DD-363 exact planner and encoder, and retain the immutable completed frame until
arbitrary output chunking drains it. Count all three live regions against the
aggregate internal-buffer limit before encoding.

Do not let ordinary input chunking or `Flush` alter frame boundaries or bytes.
Retain `EndInput` once the declared original size has been accepted, but return
`EndOfStream` only after all final serialized bytes are emitted. Reject
premature finish, excess input, unknown flags, and `ResetBlock`; keep terminal
success and failure stable on repeated calls. This step adds no streaming
decoder or public C ABI factory.

## DD-365: LZ77 Dynamic Range streaming publishes validated frames atomically

- Date: 2026-07-23
- Status: accepted

Add a bounded streaming decoder with caller-supplied serialized-frame,
canonical-token, and private-raw regions. Collect and parse the complete stream
prefix first. For every outer frame, validate the generic header and all three
workspace extents before collecting its body, then invoke the DD-361 private
decoder only after the complete declared frame is present. Drain raw bytes only
after entropy decode, token validation, and reconstruction all succeed.

Previously completed frames remain committed, but no byte from a malformed
current frame becomes visible. Retain `EndInput` while a validated final frame
drains; reject truncation once that drain completes, trailing bytes after the
declared original size, impossible workspace, unknown flags, and `ResetBlock`.
Make both terminal success and failure stable. This step adds no typed profile
or public C ABI factory.

## DD-366: LZ77 Dynamic Range profile exposes only bounded byte regions

- Date: 2026-07-23
- Status: accepted

Define a fixed profile with a default 65,536-byte raw frame and the format's
2^20-byte maximum. Encoder requirements use the smaller of known original size
and configured frame size: `F` raw bytes, `16F` canonical-token bytes, and a
serialized-frame capacity consisting of the 56-byte header, 16-byte descriptor,
and conservative `2(16F) + 5` Dynamic Range payload. Reject every component or
aggregate extent that exceeds the caller's local limits.

Decoder requirements must use only trusted local limits and the format cap.
Bound raw bytes by `min(max_frame_size, 2^20)`, token bytes by `16F`, the local
dictionary limit, and the 2^24 Dynamic Range symbol cap, and serialized storage
by the generic header plus the local internal-buffer ceiling. Expose only byte
counts and stable profile errors; do not expose private record layouts or add a
public C ABI factory in this step.

## DD-367: LZ77 Dynamic Range enters ABI v1 through two byte regions

- Date: 2026-07-23
- Status: accepted

Add a named public C config, requirements query, and factory without changing
ABI version 1 or any existing public layout. Mirror the established LZ77
configuration fields and hard limits. Report raw collection as primary and
canonical tokens plus serialized frame as secondary for encode; report
serialized-frame storage as primary and tokens plus private raw staging as
secondary for decode. Report zero views bytes and alignment one.

The factory must repeat configuration and requirements validation, reject every
short or invalid region before construction, partition secondary storage only
at the checked token extent, construct the matching bounded streaming transform
with `nothrow`, and leave the caller's handle null on every failure. This step
adds no CLI, benchmark, fuzz, completion matrix, or interoperability entry.

## DD-368: LZ77 Dynamic Range completion is proven through the C ABI

- Date: 2026-07-23
- Status: accepted

Add a public-ABI completion matrix using a fixed 64-byte raw frame and only the
published config, requirements, factory, process, and destroy lifecycle. Cover
empty input, every one-byte symbol, all byte values, repetitive and patterned
binary input, deterministic generated input, and lengths immediately around
the frame boundary. Encode each required class twice before round-trip decode.

Require identical multi-frame bytes under unchunked, one-byte, and mixed input
and output schedules. Repeated successful terminal calls must remain
`EndOfStream`. Corrupt, truncate, and append data to a fourth frame separately;
each decoder may publish exactly the first three validated frames, must preserve
the final output sentinel, and must repeat the same sticky error category and
position. This step adds no fuzz target, CLI, benchmark, or interoperability
entry.

## DD-369: LZ77 Dynamic Range fuzzing is fixed-memory and dual-boundary

- Date: 2026-07-23
- Status: accepted

Add one decoder fuzz entry point that caps supplied data at 8,192 bytes and
uses only fixed caller-owned arrays. Exercise the private complete-frame
validator when the canonical prefix parses, and independently exercise the
incremental decoder with small byte-derived input and output chunks. Bound the
incremental loop by the maximum input plus maximum output and a fixed margin;
abort on an invalid process result, an impossible zero-progress state, or
exhaustion of that ceiling.

Keep only the repository-authored five-byte truncated-magic seed. Preserve
every proper prefix of a canonical `ABABX` stream, extreme generic frame
extents, and a nonzero reserved Dynamic Range descriptor byte as permanent
regressions. Each must publish no current-frame output, preserve sentinels, and
return the same sticky error on repetition. Any future fuzz finding requires a
new deterministic regression. This step adds no CLI, benchmark, or
interoperability entry and changes no stream representation.

## DD-370: LZ77 Dynamic Range CLI uses the bounded public profile

- Date: 2026-07-23
- Status: accepted

Add the explicit selector `lz77-dynamic-range` through only the public C ABI
and existing transactional temporary-file loop. Use 65,536-byte raw frames,
the `16F = 1,048,576` canonical LZ77 token ceiling, the `2S + 5 = 2,097,157`
Dynamic Range payload ceiling, and the resulting 3,211,341-byte complete-frame
aggregate policy. Obtain both direction-specific workspace extents from the
public requirements query; do not reproduce the private workspace partition.

Require binary and empty round trips, refusal to overwrite an existing output,
malformed-input and strict trailing-data rejection, and removal of both the
requested output and `.tmp` staging path on failure. Keep selection explicit,
retain `lz77` as the default, and require callers to select the same codec for
decode rather than inferring it from the stream. This step changes no stream or
C ABI representation and adds no benchmark or interoperability entry.

## DD-371: LZ77 Dynamic Range benchmark verifies before measuring

- Date: 2026-07-23
- Status: accepted

Add `lz77-dynamic-range` to the dependency-free benchmark through only the
public C requirements, factory, process, and destroy lifecycle. Use the CLI's
65,536-byte frame, `16F` token ceiling, `2S + 5` payload ceiling, 80-byte
parameterized prefix, and checked complete-stream capacity of 32 payload bytes
per raw byte plus a 56-byte header, 16-byte descriptor, and five termination
bytes per nonempty frame.

Query encoder and decoder workspace independently, verify byte-exact round trip
before timing, exclude allocation and transform construction from timed
regions, and report ratio, direction-specific elapsed time and throughput, six
workspace extents, and peak caller-reserved workspace. Performance values are
local observations, not thresholds. This step changes no format or C ABI and
adds no interoperability entry.

## DD-372: Interoperability schema 14 appends LZ77 Dynamic Range once

- Date: 2026-07-23
- Status: accepted

Define interoperability schema 14 and codec set `marc-cli-v14` as the exact
twenty-four-entry schema-13 order followed by `lz77-dynamic-range`. Reuse the
unchanged deterministic 8,193-byte binary fixture. Generation must round-trip
all twenty-five profiles before writing the manifest; verification must check
the exact count and order, every declared size and SHA-256 value, foreign
decode equality, and byte-identical local re-encoding.

Keep schemas 1 through 13 frozen. The compatibility test starts from a complete
schema-14 bundle, rejects a reordered schema-14 manifest, removes only archive
25 while converting to schema 13, and continues the existing one-generation-
at-a-time chain through schema 1. Local admission proves generator, verifier,
compatibility rules, and same-machine CLI determinism only. Cross-platform
evidence still requires CI artifacts from the same full Git revision and the
established four-direction external verification procedure.

## DD-373: LZSS Dynamic Range consumes complete variable tokens

- Date: 2026-07-24
- Status: accepted

Reserve `lzss-dynamic-range` for LZSS variant 1 followed by Dynamic Range Coder
variant 1 under format version 1.0. Preserve the standalone 16-byte LZSS
parameter region, use no entropy parameter bytes, and set stream entropy block
size to zero. Complete the canonical variable-length LZSS token stream before
range coding; the adaptive order-0 model consumes every token byte without
interpreting its role.

For raw frame extent `F`, require nonzero token extent `S <= 2F`, range-symbol
extent `S <= 2^24`, raw extent `F <= 2^23`, and payload extent `P <= 2S + 5`.
Use one descriptor and one fresh model per nonempty frame. On decode, validate
the exact pipeline and all extents, range-decode exactly `S` private bytes with
exact payload exhaustion, parse every complete two- or nine-byte LZSS token
and derive exactly `F` raw bytes, reconstruct privately, and publish only the
whole successful frame.

Fix raw `41` as the independent boundary vector: canonical LZSS bytes `00 41`,
Dynamic Range payload `00 00 41 BE 41 7C 00`, descriptor symbol/payload extents
2 and 7, and a complete 79-byte generic frame. This step reserves format,
bounds, validation order, and vector only. It publishes no combined validator,
transform, C ABI factory, CLI selector, benchmark, fuzz target, or
interoperability entry.

## DD-374: LZSS Dynamic Range validation stops at private tokens

- Date: 2026-07-24
- Status: accepted

Implement the first executable combined boundary as a bounded exact-frame
validator. Accept only LZSS variant 1 plus Dynamic Range variant 1, stream
frames no larger than 2^23 raw bytes, one 16-byte descriptor, nonzero token
extent `S <= min(2F, 2^24)`, and payload extent `P <= 2S + 5`. Reject
truncation, trailing bytes, impossible headers, short staging, and aggregate
descriptor-plus-payload-plus-token storage before entropy output.

Parse and preflight the descriptor, range-decode exactly `S` bytes into
caller-owned private staging with exact payload exhaustion, then run the
existing complete LZSS token validator for exactly `F` raw bytes. Preserve its
format category, token index, and byte offset in the combined result. This
boundary does not reconstruct raw bytes and therefore cannot publish any. It
adds no encoder, streaming transform, public C ABI, CLI, benchmark, fuzz, or
interoperability claim.

## DD-375: LZSS Dynamic Range reconstructs only validated tokens

- Date: 2026-07-24
- Status: accepted

Extend the DD-374 boundary with caller-owned private raw staging. Before range
output, require raw capacity for exactly `F` bytes and count descriptor,
payload, canonical tokens, and raw staging together against the internal
buffer limit. Reuse DD-374 unchanged for pipeline, header, descriptor, entropy,
and complete variable-token validation.

Only after successful validation, invoke the existing bounded LZSS decoder on
the private token and raw spans. Preserve Literal handling and specified
forward overlap-copy semantics; map any impossible post-validation decoder
failure into a distinct combined error with its token and byte position.
Return the private raw extent without copying it to caller-visible output.
This step adds no publication boundary, encoder, streaming transform, C ABI,
CLI, benchmark, fuzz, or interoperability claim.

## DD-376: LZSS Dynamic Range publishes only complete frames

- Date: 2026-07-24
- Status: accepted

Add a caller-visible exact-frame decoder above DD-375. Require output capacity
for exactly `F` bytes before entropy output, in addition to the existing token,
private raw, and aggregate checks. Run the unchanged validator and private
reconstruction path, then copy the complete private raw extent to output only
after success.

Every capacity, header, descriptor, entropy, token, or reconstruction failure
must leave caller output unchanged. Bytes beyond `F` are never touched. This
step completes transactional publication for one exact frame but adds no
encoder, streaming transform, C ABI, CLI, benchmark, fuzz, or interoperability
claim.

## DD-377: LZSS Dynamic Range plans from frozen token bytes

- Date: 2026-07-24
- Status: accepted

Add an exact-frame planner and encoder for the DD-373 representation. Plan and
encode the complete deterministic LZSS token stream into caller-owned private
staging before invoking Dynamic Range planning. Enforce `S <= min(2F, 2^24)`,
`P <= 2S + 5`, descriptor-plus-payload-plus-token aggregate storage, exact
outer-frame extent, and all component limits.

Report the exact `56 + 16 + P` serialized extent. The encoder must reject a
short serialized destination before writing it, replan the unchanged token
bytes, require the same payload extent, and then serialize header, descriptor,
and payload in order. It must reproduce the independent 79-byte frame and be
byte-identical across repeated calls. This step adds no streaming transform,
C ABI, CLI, benchmark, fuzz, or interoperability claim.

## DD-378: LZSS Dynamic Range streaming encoding freezes each frame

- Date: 2026-07-24
- Status: accepted

Add a bounded encoder transform around DD-377. Emit the canonical 64-byte
stream header and 16-byte LZSS parameter region first. Buffer exactly one
declared raw frame, complete and freeze its canonical LZSS token stream, encode
one exact Dynamic Range frame, then drain that immutable serialized extent
before collecting another frame.

Require caller-owned raw storage for `F`, token staging for `2F`, and one
complete serialized-frame region. Count raw, actual tokens, and the exact
serialized frame against the aggregate internal-buffer limit before frame
publication. Input/output chunking and nonterminal `Flush` do not alter frame
boundaries. Accept `EndInput` only with all remaining declared input and retain
it through output starvation; emit prefix only for empty input. Reject
`ResetBlock`, excess or premature input, invalid configuration, and insufficient
workspace with stable transform errors. This step adds no streaming decoder,
C ABI, CLI, benchmark, fuzz, or interoperability claim.

## DD-379: LZSS Dynamic Range streaming decoding admits one frame

- Date: 2026-07-24
- Status: accepted

Add the inverse bounded transform for DD-378. Incrementally collect and parse
the 80-byte stream prefix and one 56-byte frame header. Before collecting its
body, require nonzero `S <= min(2F, 2^24)`, `P <= 2S + 5`, sufficient
serialized-frame, token, and private-raw capacities, and the aggregate
serialized-plus-token-plus-raw limit.

Collect exactly the admitted serialized frame, invoke DD-375 private
reconstruction, and expose raw bytes only after that whole frame succeeds.
Retain a successfully decoded frame across arbitrary output starvation and
retain final `EndInput` until it drains. Earlier complete frames remain
committed if a later frame fails, but the failing frame publishes nothing.
Reject invalid prefixes, parameters, headers, descriptors, payloads, tokens,
premature end, trailing bytes, unknown flags, and `ResetBlock` with stable
transform categories and sticky terminal state. This step adds no workspace
profile, public C ABI, completion matrix, fuzz target, CLI, benchmark, or
interoperability claim.

## DD-380: LZSS Dynamic Range profile exposes byte-only workspaces

- Date: 2026-07-24
- Status: accepted

Add a bounded profile constructor and decoder-workspace query above DD-378 and
DD-379. For largest encoder raw frame `F`, require `F` raw bytes, `2F`
canonical-token bytes, and `56 + 16 + (4F + 5) = 4F + 77` serialized-frame
bytes. Count the simultaneously live regions as `7F + 77`; also enforce the
2^24 token ceiling, compressed-payload limit, dictionary limit, entropy
buffered limit, and aggregate internal-buffer limit. Use the actual largest
frame `min(original_size, configured_frame_size)` and return zero workspaces
for empty input.

Derive decoder raw staging from `min(max_frame_size, 2^23)`, token staging
from the least of `2F`, the dictionary limit, and 2^24, and serialized-frame
storage from the established 56-byte header plus local internal-buffer
ceiling. Validate limits first and check every arithmetic operation and
`size_t` conversion. Publish only byte counts and stable profile-to-core error
mapping; expose no aligned or private typed layout. This step adds no public C
requirements query or factory, completion matrix, fuzz target, CLI, benchmark,
or interoperability claim.

## DD-381: LZSS Dynamic Range enters C ABI version 1

- Date: 2026-07-24
- Status: accepted

Expose `marc_lzss_dynamic_range_config` plus initialization, requirements-query,
and creation functions as one immutable LZSS variant 1 plus Dynamic Range
variant 1 profile. Preserve known-size encoding, the common opaque-transform
lifecycle, stable process statuses, and ABI version 1. Carry frame size, LZSS
parameters, and applicable local limits in fixed-width C fields; require all
reserved fields to remain zero.

Use primary storage for raw frames while encoding or serialized frames while
decoding. Partition secondary storage into canonical tokens followed by the
complete encoded frame for encode, or tokens followed by private raw staging
for decode. Require no views workspace and report alignment one. Revalidate the
same profile during creation, construct the selected completed streaming
transform with `nothrow`, and leave the output handle null on every failure.
This step adds no completion matrix, fuzz target, CLI, benchmark, or
interoperability claim and does not change stream bytes or the ABI version.

## DD-382: LZSS Dynamic Range completion is audited through C

- Date: 2026-07-24
- Status: accepted

Add a public-ABI completion matrix above DD-381 with a fixed 64-byte audit
frame. Through only config initialization, requirements query, factory,
process, and destroy, cover empty input, every one-byte value, all byte values,
long zeros, repeated binary patterns, generated incompressible-looking bytes,
and lengths 63, 64, and 65. Require repeated encoding to be byte-identical.

For a 193-byte four-frame stream, require the same archive under whole-buffer,
1/1, 7/5, and 13/17 input/output schedules and exact decode under each. Repeated
calls after success must remain ended with zero counts. Corrupt the final frame
header, truncate its final byte, and append one trailing byte independently;
each must return a sticky malformed-stream result, commit exactly the first
192 raw bytes, and preserve the final output sentinel. This closes public API
completion evidence but adds no fuzz target, CLI, benchmark, interoperability
entry, format change, or ABI change.

## DD-383: LZSS Dynamic Range fuzzing fixes the complete working set

- Date: 2026-07-24
- Status: accepted

Add an LLVM-compatible decoder harness that reaches both the exact-frame
private-staging boundary and the public incremental transform. Truncate each
supplied case to 8 KiB and fix total output at 4 KiB, one raw frame at 1 KiB,
canonical LZSS token staging at 2 KiB, Dynamic Range payload at 8 KiB, and the
encoded-frame extent at the generic header plus descriptor and payload bound.
Count encoded-frame, token, private-raw, and final-output arrays in one checked
aggregate policy before parsing.

Require an exact LZSS variant 1 plus Dynamic Range variant 1 prefix before the
complete-frame path. Independently feed every case to the incremental decoder
with byte-derived input and output chunks and a fixed call ceiling. Abort the
harness on impossible result counts, `Progress` without progress, or a call-
ceiling violation so a stall becomes reproducible. Retain only a reviewed
five-byte truncated-magic seed.

Add permanent deterministic regressions requiring every proper prefix of a
canonical frame, saturated generic-frame extent fields, and a nonzero reserved
descriptor byte to fail atomically, preserve the entire raw destination, and
retain stable error position. Register ordinary compile-smoke coverage and a
sanitizer-linked libFuzzer executable. Also register the previously omitted
sanitizer executable for the existing LZ77 Dynamic Range harness so documented
and buildable targets agree. This step changes no format or ABI and adds no
CLI, benchmark, or interoperability claim.

## DD-384: LZSS Dynamic Range CLI uses the bounded public profile

- Date: 2026-07-24
- Status: accepted

Add the explicit selector `lzss-dynamic-range` through only the public C ABI
and existing transactional temporary-file loop. Use 65,536-byte raw frames,
the `2F = 131,072` canonical LZSS token ceiling, the
`2S + 5 = 262,149` Dynamic Range payload ceiling, and the resulting
`7F + 77 = 458,829` complete simultaneous-workspace policy. Obtain both
direction-specific workspace extents from the public requirements query; do
not reproduce the private workspace partition.

Require binary and empty round trips, refusal to overwrite an existing output,
malformed-input and strict trailing-data rejection, and removal of both the
requested output and `.tmp` staging path on failure. Keep selection explicit,
retain `lz77` as the default, and require callers to select the same codec for
decode rather than inferring it from the stream. This step changes no stream or
C ABI representation and adds no benchmark or interoperability entry.

## DD-385: LZSS Dynamic Range benchmark verifies before measuring

- Date: 2026-07-24
- Status: accepted

Add `lzss-dynamic-range` to the dependency-free benchmark through only the
public C requirements query, factory, process, and destroy lifecycle. Preserve
the CLI's 65,536-byte frame, 131,072-byte canonical-token,
262,149-byte payload, and 458,829-byte aggregate policy. Query encoder and
decoder workspaces independently and report their exact primary, secondary,
and zero-view extents plus the larger total.

For input extent `N` and nonempty frame count `K`, reserve complete-stream
output using checked arithmetic as `80 + 4N + 77K`: four payload bytes per raw
byte cover the `2F` LZSS and `2S` range bounds, while each frame adds one
56-byte header, one 16-byte descriptor, and five termination bytes. Empty
input reserves only the 80-byte prefix.

Before timing, encode once, decode the exact encoded extent once, and require a
byte-identical complete round trip. Time encode and decode separately and
report ratio, throughput, and workspace without defining pass/fail performance
thresholds. Add a one-iteration README smoke. This changes no format or ABI
and adds no interoperability entry.

## DD-386: Interoperability schema 15 appends LZSS Dynamic Range once

- Date: 2026-07-24
- Status: accepted

Define interoperability schema 15 and codec set `marc-cli-v15` as the exact
twenty-five-entry schema-14 order followed by `lzss-dynamic-range`. Reuse the
deterministic 8,193-byte repository fixture. Generation must round-trip all
twenty-six profiles before writing a manifest; verification must check the
exact count and order, every declared size and SHA-256 value, foreign decode
equality, byte-identical local re-encoding, platform metadata, and a full Git
object ID.

Keep schemas 1 through 14 accepted with their frozen codec sets and orders.
The compatibility test must generate and verify schema 15, reject a reordered
schema-15 manifest, remove only archive 26 to derive schema 14, and continue
the existing one-generation-at-a-time conversion through schema 1. No prior
schema meaning or stream representation changes.

Local generation and verification establish deterministic same-build evidence
only. Cross-platform admission requires the Windows/MSVC and Ubuntu 24.04 CI
artifacts plus an Ubuntu 26.04/Clang bundle to pass the four established
verification directions at the exact pushed revision. Record that evidence
only after it exists.

## DD-387: LZ78 Dynamic Range preserves fixed phrase tokens

- Date: 2026-07-24
- Status: accepted

Reserve `lz78-dynamic-range` for LZ78 variant 1 followed by Dynamic Range Coder
variant 1 under format version 1.0. Preserve the standalone 16-byte LZ78
parameter region, use no entropy parameter bytes, and set stream entropy block
size to zero. Complete the canonical fixed-width LZ78 token stream before
range coding; the adaptive order-0 model consumes every stored token byte
without interpreting its tag, symbol, reserved bytes, or phrase index.

For raw frame extent `F`, require nonzero token extent `S` to be a multiple of
eight with `S <= 8F` and `S <= 2^24`. The format-level raw-frame ceiling is
therefore 2^21 bytes. Require range payload extent `P <= 2S + 5`, one
descriptor, and one fresh LZ78 dictionary and range model per nonempty frame.
Before publication, range-decode exactly `S` private bytes with exact payload
exhaustion, validate every complete LZ78 token and phrase reference in bounded
aligned workspace, derive exactly `F` raw bytes, and reconstruct privately.

Fix raw `41` as the independent boundary vector: canonical LZ78 Pair bytes
`00 41 00 00 00 00 00 00`, Dynamic Range payload
`00 00 41 BE 41 7C 00 00 00 00 00`, descriptor symbol/payload extents 8 and
11, and a complete 83-byte generic frame. This step reserves format, bounds,
validation order, and vector only. It publishes no combined validator,
transform, C ABI factory, CLI selector, benchmark, fuzz target, or
interoperability entry.

## DD-388: LZ78 Dynamic Range validation stops at the phrase graph

- Date: 2026-07-24
- Status: accepted

Implement the first executable combined boundary as a bounded exact-frame
validator. Accept only LZ78 variant 1 plus Dynamic Range variant 1, stream
frames no larger than 2^21 raw bytes, one 16-byte descriptor, nonzero token
extent `S` that is a multiple of eight with `S <= min(8F, 2^24)`, and payload
extent `5 <= P <= 2S + 5`. Reject truncation, trailing bytes, impossible
headers, short token or aligned phrase workspace, and aggregate
descriptor-plus-payload-plus-token-plus-phrase storage before entropy output.

Parse and preflight the descriptor, range-decode exactly `S` bytes into
caller-owned private staging with exact payload exhaustion, then run the
existing complete LZ78 validator for exactly `F` raw bytes in caller-owned
phrase entries. Preserve its format category, token index, and byte offset in
the combined result. This boundary builds and validates the phrase graph but
does not reconstruct or publish any raw byte. It adds no encoder, streaming
transform, public C ABI, CLI, benchmark, fuzz, or interoperability claim.

## DD-389: LZ78 Dynamic Range reconstructs only validated phrases

- Date: 2026-07-25
- Status: accepted

Extend DD-388 with a caller-owned private raw-staging boundary. Before any
entropy output, require raw capacity for the declared frame extent and count
descriptor, payload, token staging, aligned phrase entries, and raw staging in
one checked aggregate. Preserve the existing validator unchanged for callers
that stop at the phrase graph.

Only after strict range decoding and complete phrase-graph validation succeeds,
invoke marc's existing bounded, non-recursive LZ78 decoder over exactly the
validated tokens and phrase entries. Reconstruct exactly the declared raw
extent into private staging, preserve stable dictionary error categories and
positions, and return without copying a byte to caller-visible output. This
step adds no transactional publication, encoder, streaming transform, public C
ABI, CLI, benchmark, fuzz, or interoperability claim.

## DD-390: LZ78 Dynamic Range publishes only complete frames

- Date: 2026-07-25
- Status: accepted

Add a caller-visible exact-frame decoder above DD-389. Require output capacity
for exactly `F` bytes before entropy output, in addition to the existing token,
aligned phrase, private raw, and aggregate checks. Run the unchanged validator
and private reconstruction path, then copy the complete private raw extent to
output only after success.

Every capacity, header, descriptor, entropy, token, phrase, or reconstruction
failure must leave caller output unchanged. Bytes beyond `F` are never touched.
This step completes transactional publication for one exact frame but adds no
encoder, streaming transform, C ABI, CLI, benchmark, fuzz, or interoperability
claim.

## DD-391: LZ78 Dynamic Range planning freezes canonical tokens

- Date: 2026-07-25
- Status: accepted

Add a no-serialized-output exact-frame planner. Require the complete aligned
LZ78 encoder-entry workspace before phrase parsing. Plan the deterministic
parse, require canonical token staging for the exact `S` bytes, emit all
eight-byte tokens there, and only then invoke the unchanged Dynamic Range
planner over that immutable extent.

Count encoder entries, token staging, the 16-byte descriptor, and planned
payload in one checked aggregate. Enforce `S <= 8F`, `S <= 2^24`, `P <= 2S +
5`, the generic header contract, and exact serialized extent `56 + 16 + P`.
Return the plan without writing any serialized frame byte. This step adds no
serialized-frame encoder, streaming transform, C ABI, CLI, benchmark, fuzz, or
interoperability claim.

## DD-392: LZ78 Dynamic Range emits only an exact completed plan

- Date: 2026-07-25
- Status: accepted

Add the deterministic exact-frame encoder above DD-391. Complete the planner
first, then reject a serialized destination shorter than its exact extent
before writing any output byte. Replan Dynamic Range over the unchanged
canonical LZ78 staging and require the same payload extent before serialization.

Serialize the validated generic header, 16-byte descriptor, and exact payload
in order. The raw-`41` input must reproduce the independent 83-byte frame;
repeated encoding of nested `ABAB` must be byte-identical and decode through
the transactional frame boundary. This step adds no streaming transform, C
ABI, CLI, benchmark, fuzz, or interoperability claim.

## DD-393: LZ78 Dynamic Range streams only completed exact frames

- Date: 2026-07-25
- Status: accepted

Add a bounded known-size streaming encoder above DD-392. Serialize the existing
stream header and 16-byte LZ78 parameters as the 80-byte prefix. Collect one
complete configured raw frame, plan and encode it into retained private frame
storage, and drain that immutable frame before collecting another.

The stream bytes must equal concatenated one-shot frame bytes for every
input/output chunking. A nonterminal `Flush` does not close a partial frame.
Accept `EndInput` only when the call supplies the complete remaining declared
input, latch it while prefix and frame bytes drain, and return stable ended or
sticky error states. Reject `ResetBlock` and unknown flags. Count raw, token,
encoder-entry, and serialized-frame storage in the active aggregate. This step
adds no streaming decoder, C ABI, CLI, benchmark, fuzz, or interoperability
claim.

## DD-394: LZ78 Dynamic Range streaming decode admits only complete frames

- Date: 2026-07-25
- Status: accepted

Add the matching bounded streaming decoder above DD-390. Incrementally collect
and validate the fixed 80-byte prefix, then collect one 56-byte frame header.
Before accepting its body, reject impossible `S`, `P`, descriptor, exact
serialized-frame, token staging, private raw, aligned phrase-workspace, and
aggregate buffered-byte extents.

Collect only the admitted descriptor and payload, invoke the existing private
complete-frame decoder, and expose raw bytes only after validation and
reconstruction both succeed. A malformed later frame publishes none of that
frame; already drained frames remain committed. Preserve one-byte input and
output behavior, retained `EndInput`, stable ended and sticky error states,
strict truncation and trailing-data rejection, and rejection of `ResetBlock`
and unknown flags. This step adds no C ABI, CLI, benchmark, fuzz, or
interoperability claim.

## DD-395: LZ78 Dynamic Range profile exposes opaque aligned records

- Date: 2026-07-25
- Status: accepted

Add a bounded profile and direction-specific workspace calculation without
changing the DD-387 representation. For encoding, derive the largest actual
raw frame, its conservative `S = 8F` token staging, `P = 2S + 5` range
payload, complete serialized-frame extent, and
`min(F, maximum_entries)` LZ78 encoder records. Count every active region in
one checked aggregate.

For decoding, derive conservative serialized-frame, token, private-raw, and
phrase-record capacities only from trusted local limits and the 2^21-byte
profile cap. Report typed encoder and phrase records as opaque byte counts plus
alignment. Partition them only after rechecking count, byte extent, alignment,
and caller capacity. Empty encode input requires no active workspace and
retains alignment one. This step adds no C ABI factory, completion matrix, CLI,
benchmark, fuzz, or interoperability claim.

## DD-396: LZ78 Dynamic Range enters C ABI v1 through three workspaces

- Date: 2026-07-25
- Status: accepted

Expose a size-tagged `marc_lz78_dynamic_range_config`, initializer,
direction-specific requirements query, and factory in the existing C ABI
version 1. Retain the known original-size contract and the DD-395 default
65,536-byte raw frame and LZ78 entry limit.

The requirements query reports one primary byte region, one secondary byte
region, and one aligned opaque views region. The factory rechecks every config
tag, reserved field, limit, capacity, and views alignment; recalculates the
profile; partitions typed records privately; and publishes a transform only
after construction succeeds. C callers never name or size a C++ LZ78 record.
This step adds no completion matrix, fuzz target, CLI selector, benchmark, or
interoperability entry.

## DD-397: LZ78 Dynamic Range completion is proven through C ABI v1

- Date: 2026-07-25
- Status: accepted

Add a public-ABI completion matrix above DD-396 with a fixed 64-byte raw frame,
512-byte conservative LZ78 token extent, 1,029-byte Dynamic Range payload
extent, and 65,536-byte active workspace limit. Construct both directions only
through config initialization, requirements query, factory, process, and
destroy.

Cover empty input, every one-byte value, the ordered byte alphabet, long zero
runs, repeated binary patterns, deterministic generated data, and lengths 63,
64, and 65. Require repeated encoding and one-byte or mixed chunking to produce
identical streams, exact round trips, stable repeated success, and no
zero-progress `Progress`.

For 193 raw bytes, corrupt, truncate, and append data to the fourth frame.
Each failure must publish exactly the first three 64-byte frames, preserve the
final output sentinel, and return the same sticky error category and position
on a repeated call. This step adds no fuzz target, CLI selector, benchmark, or
interoperability entry.

## DD-398: LZ78 Dynamic Range fuzzing is fixed-memory and decoder-only

- Date: 2026-07-25
- Status: accepted

Add one LLVM-compatible decoder entry point that bounds accepted input at
8,192 bytes and exercises both the exact-frame private decoder and the outer
streaming decoder. Fix total raw output at 4,096 bytes, each raw frame at 1,024
bytes, token and payload regions at 8,192 bytes each, the phrase table at 1,024
records, and the incremental call budget at input plus output capacity plus 32.

Input bytes may select only bounded chunk sizes. They must not resize an
allocation, change a decoder limit, select recursion, or extend the call
budget. Abort only for a violated internal process invariant or exhausted
finite budget; malformed input is an ordinary decoder result.

Compile this entry point with ordinary warning levels in every test build and
with libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer only when fuzz
targets are explicitly enabled. Preserve canonical truncation, extreme frame
length, and invalid Dynamic Range descriptor failures as normal atomic
regression tests. This step adds no CLI selector, benchmark, or interoperability
entry and does not require an unbounded fuzz campaign.

## DD-399: LZ78 Dynamic Range CLI is a fixed public-ABI adapter

- Date: 2026-07-25
- Status: accepted

Add the explicit `lz78-dynamic-range` selector to the existing transactional
CLI without changing the default codec. Use a 65,536-byte raw frame, the
canonical `S = 8F = 524,288` token ceiling, the
`P = 2S + 5 = 1,048,581` Dynamic Range payload ceiling, the public default
LZ78 entry limit, and a 4-MiB aggregate buffered-byte policy.

The CLI must initialize the public size-tagged config, set only public format
parameters and hard limits, query all three direction-specific workspace
regions and views alignment, and construct the transform through
`marc_lz78_dynamic_range_create()`. It must not name private record types,
recalculate opaque record sizes, or invoke a private C++ frame API.

Retain the existing output refusal and sibling `.tmp` protocol. Encoding or
decoding failure, malformed input, strict trailing data, write failure, close
failure, or rename failure must leave no requested destination or temporary
file. Test binary and empty round trips, overwrite refusal, malformed input,
and a valid stream with trailing data. This step adds no benchmark or
interoperability entry.

## DD-400: LZ78 Dynamic Range benchmark measures the public CLI profile

- Date: 2026-07-25
- Status: accepted

Add `lz78-dynamic-range` to the dependency-free benchmark with the exact DD-399
public profile: 65,536-byte raw frames, 524,288 canonical token bytes,
1,048,581 range-payload bytes, the public default LZ78 entry limit, and a
4-MiB aggregate policy.

For input extent `N` and nonempty frame count `K`, reserve checked complete
stream capacity `80 + 16N + 77K`. The `16N` term covers the conservative
`S <= 8N` and `P <= 2S + 5` payload relation; each frame contributes its
56-byte header, 16-byte descriptor, and five termination bytes. Overflow must
fail before allocating the encoded buffer.

Construct both directions only through the public config initializer,
requirements query, factory, process, and destroy lifecycle. Require one
untimed byte-exact round trip before timing fresh transforms. Report encoded
ratio, encode/decode throughput, all six queried workspace extents, and the
larger three-region sum as descriptive peak workspace. Add a one-iteration
smoke test with no performance threshold. This step adds no interoperability
entry.

## DD-401: Interoperability schema 16 appends LZ78 Dynamic Range once

- Date: 2026-07-25
- Status: accepted

Define interoperability schema 16 and codec set `marc-cli-v16` as the exact
twenty-six-entry schema-15 order followed by `lz78-dynamic-range`. Retain the
deterministic 8,193-byte fixture and all existing manifest fields. The
generator must locally decode every archive before publishing the manifest.
The verifier must require exactly twenty-seven archives in canonical order,
validate all declared extents and SHA-256 values, decode every foreign archive,
and reproduce every archive byte for byte with the local encoder.

The compatibility test must generate and verify schema 16, reject a reordered
schema-16 manifest before archive decoding, remove only archive 27 to derive
schema 15, and continue the complete frozen conversion chain through schema 1.
No previous schema, archive order, codec set, stream representation, fixture,
or manifest field changes. Cross-platform interoperability remains unproven
until one pushed revision passes the established Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang bidirectional artifact procedure.

## DD-402: LZW Dynamic Range entropizes finalized packed-code bytes

- Date: 2026-07-25
- Status: accepted

Reserve `lzw-dynamic-range` for LZW variant 1 followed by Dynamic Range Coder
variant 1 under format version 1.0. Preserve the standalone 16-byte LZW
parameters, empty entropy parameters, LSB-first variable-width code schedule,
and final LZW zero padding. Complete the packed-code byte stream before entropy
processing; Dynamic Range consumes every resulting byte, including the final
padded byte, without interpreting LZW code or padding boundaries. Reset both
the LZW dictionary and adaptive order-0 range model at every outer frame.

For raw frame size `F` and maximum code width `W`, use checked packed staging
bound `S = ceil(F * W / 8)` and Dynamic Range payload bound `P = 2S + 5`.
Bound generated entries for a nonempty frame by
`min(F - 1, 2^W - 256, local_limit)`. Retain the LZW composition format cap
`F <= 2^20`. The reference profile uses `F = 65,536` and `W = 16`, giving
`S = 131,072`, `P = 262,149`, and at most 65,280 generated entries.

Encoding freezes the canonical LZW packed bytes before range planning.
Decoding range-decodes exactly the declared packed-byte count into private
staging, then applies the ordinary LZW width-change, reference, `KwKwK`,
zero-padding, and exact-raw-extent validation before any raw publication.
Error precedence is generic header and extent validation, workspace admission,
range descriptor and payload validation, LZW validation, then private
reconstruction and publication.

Freeze raw `A` independently: LZW code 65 at width nine produces packed bytes
`41 00`; Dynamic Range variant 1 over those two bytes produces payload
`00 40 FF FF BF 00 00` and descriptor `(2, 7, 0)`. Record the complete 79-byte
frame in the format document and prove it by composing only the existing
standalone LZW encoder, Dynamic Range encoder, and generic serializers. This
decision specifies bytes and a reserved name only; it does not publish a
combined validator, decoder, encoder, streaming transform, factory, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-403: LZW Dynamic Range validation stops at the packed-byte boundary

- Date: 2026-07-25
- Status: accepted

Admit the first combined `lzw-dynamic-range` implementation as a strict bounded
complete-frame validator only. Validate the stream profile, LZW parameters,
sequence, generic frame header, exact complete-frame extent, checked
`S = ceil(FW/8)` packed bound, one 16-byte Dynamic Range descriptor,
`P = 2S + 5` payload bound, every caller-owned capacity, aligned phrase bytes,
and the aggregate workspace limit before entropy output.

Parse the descriptor only after that admission succeeds. Range-decode exactly
the declared packed-byte count into private caller-owned staging with exact
payload exhaustion, then invoke the existing LZW validator over the complete
span. Preserve LZW's width schedule, reference and `KwKwK` rules, final
high-bit padding check, exact declared raw extent, code count, format error,
and phrase-table requirements.

Use stable error precedence: unsupported profile, truncated header, generic
header, complete-frame extent, packed and entropy extents, caller workspace,
aggregate workspace, descriptor, range payload, then LZW code stream. On every
error the caller must discard staging and phrase contents; no raw byte is
reconstructed or published. This step adds no private raw decoder, encoder,
streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz target,
completion matrix, or interoperability entry.

## DD-404: LZW Dynamic Range reconstructs only into private raw staging

- Date: 2026-07-25
- Status: accepted

Extend DD-403 with a bounded complete-frame decoder that reconstructs the
already validated packed LZW stream into caller-owned private raw staging.
Require raw capacity for the complete declared frame and add that full extent
to aggregate workspace accounting before parsing the Dynamic Range descriptor
or producing packed bytes.

Reuse the DD-403 validator without weakening or duplicating its header,
`S`/`P`, workspace, descriptor, payload-exhaustion, width-change, reference,
`KwKwK`, padding, and exact-raw-extent checks. Only after all validation
succeeds may the existing iterative LZW decoder expand the staged packed codes
through the validated phrase table into private raw storage. Preserve detailed
LZW validation, format, and decode diagnostics.

On every failure the caller must discard packed staging, phrase records, and
raw staging. No caller-visible raw output is accepted by this API, so no byte
is published at this boundary. This step adds no transactional publication
wrapper, encoder, streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-405: LZW Dynamic Range publishes only a complete successful frame

- Date: 2026-07-26
- Status: accepted

Add an internal caller-visible complete-frame decoder above DD-404. Require
destination capacity for the complete declared raw frame together with packed
staging, aligned phrase records, and private raw staging before parsing the
Dynamic Range descriptor or producing entropy output. Caller-visible output is
not scratch and therefore is not counted in aggregate internal workspace.

Run the unchanged DD-403 validation order and DD-404 private reconstruction.
Only after every generic-frame, range, LZW, capacity, bounds, and exact-extent
check succeeds may the decoder copy the complete private raw span once into
the destination.

Preserve every existing combined error value and append a distinct output-
capacity error. On every failure, publish no destination byte. This step adds
no encoder, streaming transform, profile calculator, C ABI, CLI, benchmark,
fuzz target, completion matrix, or interoperability entry.

## DD-406: LZW Dynamic Range planning freezes packed bytes before range output

- Date: 2026-07-26
- Status: accepted

Add an exact-frame planner for `lzw-dynamic-range`. Require one nonempty raw
frame, validate the selected profile and caller-owned LZW encoder workspace,
and run the deterministic LZW plan. Require packed staging for the exact
planned extent, then encode the complete canonical variable-width code stream,
including final zero padding, before invoking Dynamic Range planning.

Plan Dynamic Range over that immutable packed-byte extent and require the exact
payload to satisfy `P <= 2S + 5`. Count LZW encoder records, packed staging,
the 16-byte descriptor, and exact payload in one checked internal-workspace
sum. Construct and validate the complete generic frame header and report its
exact serialized extent without writing any serialized-frame byte.

Preserve every existing combined error value and append distinct input-size,
encoder-workspace, dictionary-encode, entropy-encode, and internal-consistency
errors. This step adds no serialized encoder, streaming transform, profile
calculator, C ABI, CLI, benchmark, fuzz target, completion matrix, or
interoperability entry.

## DD-407: LZW Dynamic Range encoding is plan-first and deterministic

- Date: 2026-07-26
- Status: accepted

Add the deterministic complete-frame encoder above DD-406. Invoke the exact
planner first so canonical packed LZW bytes, final zero padding, exact range
payload size, generic frame fields, and aggregate workspace are fixed before
serialized output is considered. Require destination capacity for the complete
planned extent before writing any serialized byte.

Repeat Dynamic Range planning over the frozen packed span and require its
payload extent to match DD-406. Serialize the generic frame header and 16-byte
descriptor explicitly, then encode the exact payload into its planned region.
The independent raw-`A` input must reproduce the complete 79-byte vector.

Preserve every existing combined error value and append a distinct serialized-
output-capacity error. Capacity and all planner failures leave serialized
output unchanged. This step adds no streaming transform, profile calculator,
C ABI, CLI, benchmark, fuzz target, completion matrix, or interoperability
entry.

## DD-408: LZW Dynamic Range streaming encode buffers one bounded frame

- Date: 2026-07-26
- Status: accepted

Add a bounded known-size streaming encoder above DD-407. Emit the ordinary
64-byte stream header and 16-byte LZW parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete
serialized representation through the deterministic exact-frame encoder, and
drain that immutable frame under arbitrary output starvation before accepting
bytes for the next frame.

At construction, validate the fixed pipeline, parameters, known original size,
largest raw frame, conservative `ceil(FW/8)` packed staging, and LZW encoder
records. Before encoding each collected frame, count raw collection, exact
packed staging, exact serialized frame, and encoder records in one checked
aggregate and require complete encoded-frame storage.

Input and output chunking alone must not change serialized bytes. `Flush` keeps
the logical stream open and does not shorten a frame. Retain `EndInput` while
the prefix or a frame drains, require its input to complete the declared known
size, reject `ResetBlock` and unknown flags, and make ended and error results
sticky. This step adds no streaming decoder, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-409: LZW Dynamic Range streaming decode validates before draining

- Date: 2026-07-26
- Status: accepted

Add the matching bounded known-size streaming decoder. Incrementally collect
and validate the fixed 80-byte stream prefix, then one 56-byte frame header.
Before admitting the frame body, enforce `S <= ceil(FW/8)`,
`5 <= P <= 2S + 5`, one 16-byte descriptor, exact caller capacities for the
complete encoded frame, packed staging, private raw staging, and aligned LZW
phrase records, plus their checked aggregate workspace.

Collect exactly the admitted descriptor and payload, invoke DD-405's
transactional complete-frame decoder into private raw storage, and only then
drain that immutable raw frame. Do not collect a later frame while validated
raw bytes remain pending. Earlier complete frames may be published, but a
malformed later frame must publish none of its own bytes.

Require exact known-size completion and reject every prefix, header, or body
truncation, trailing byte, wrong pipeline, invalid extent, `ResetBlock`, and
unknown flag. Retain `EndInput` while a validated frame drains and make ended
and error states sticky. This step adds no profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-410: LZW Dynamic Range profiles separate byte and typed storage

- Date: 2026-07-26
- Status: accepted

Add an internal direction-specific profile calculator above DD-408 and DD-409.
For encoding, derive the largest raw frame, conservative
`S = ceil(FW/8)` packed staging, `2S + 5` Dynamic Range payload, complete
serialized-frame storage, and the exact LZW encoder-record count. Count every
region in one checked aggregate before returning any requirement.

For decoding, derive complete encoded-frame collection, bounded packed staging,
private raw staging, and the conservative phrase-record count solely from
validated local limits. Keep byte storage separate from caller-allocated,
properly aligned record storage. Partition helpers must verify the reported
record byte count and alignment before producing typed spans; empty record
storage uses zero bytes and neutral alignment one.

This step adds no C ABI factory, CLI selector, benchmark, fuzz target,
completion matrix, or interoperability entry.

## DD-411: LZW Dynamic Range enters the C ABI with opaque typed views

- Date: 2026-07-26
- Status: accepted

Expose the fixed LZW variant 1 plus Dynamic Range variant 1 profile through a
size-tagged `marc_lzw_dynamic_range_config`, direction-specific requirements
query, and immutable-direction factory without changing C ABI version 1.

Retain the common three caller-owned workspaces. Encoding uses raw primary
storage, packed-plus-serialized secondary storage, and aligned opaque encoder
records. Decoding uses serialized primary storage, packed-plus-private-raw
secondary storage, and aligned opaque phrase records. Creation reruns DD-410
and its checked partition helpers instead of trusting caller-supplied extents.

Require a strict C11 round trip, exact small-limit requirements, one-byte-short
and misaligned workspace rejection, reserved-field rejection, and a null
transform on every factory failure. This step adds no CLI selector, benchmark,
fuzz target, completion matrix, or interoperability entry.

## DD-412: LZW Dynamic Range completion is audited through the public C ABI

- Date: 2026-07-26
- Status: accepted

Audit only `marc_lzw_dynamic_range_config_init`, its requirements query,
factory, the common process function, and transform destruction. Use 64-byte
frames and the checked `S = ceil(FW/8)`, `P = 2S + 5` workspace policy.

Cover empty input, every one-byte value, the full byte alphabet, repetitive
and generated binary inputs, and lengths 63, 64, and 65. Require deterministic
re-encoding and identical streams under `(1,1)`, `(7,5)`, and `(13,17)`
input/output chunk schedules, with repeatable EndOfStream.

For a 193-byte four-frame stream, independently corrupt, truncate, and extend
the fourth frame. Each decoder must commit exactly the first 192 bytes, leave
the failing frame's destination sentinel unchanged, and repeat the same stable
terminal error. Reuse the LZW public-ABI test body across entropy profiles with
only the fixed factory family and payload ceiling parameterized, preventing
evidence drift. This step adds no CLI, benchmark, fuzz, or interoperability
entry.

## DD-413: LZW Dynamic Range fuzzing is fixed-memory and dual-path

- Date: 2026-07-26
- Status: accepted

Add one bounded decoder fuzz entry that exercises both the private complete-
frame decoder and the outer frame-committing streaming decoder. Cap accepted
input at 8,192 bytes, total raw output at 4,096 bytes, one raw frame at 1,024
bytes, packed LZW staging at 4,096 bytes, and dictionary entries at 4,096.
Allocate every byte and phrase region as a fixed local array before inspecting
input.

Derive chunk sizes only within those arrays and stop after
`input_bound + output_bound + 32` calls. Abort on an invalid process result,
zero progress reported as Progress, impossible NeedInput after final input, or
the finite-call ceiling. Reuse the established bounded LZW harness with only
the entropy identity and combined entry points parameterized.

Permanent regressions must reject every proper prefix of a canonical stream,
saturated frame extents, and a nonzero reserved Dynamic Range descriptor byte.
All failures preserve the raw output sentinel and remain sticky. This step adds
no CLI, benchmark, or interoperability entry.

## DD-414: LZW Dynamic Range CLI is a fixed public-ABI adapter

- Date: 2026-07-26
- Status: accepted

Add the explicit `lzw-dynamic-range` selector to the existing transactional
CLI without changing the default codec. Use a 65,536-byte raw frame, the
canonical `S = 2F = 131,072` packed-code ceiling, the
`P = 2S + 5 = 262,149` Dynamic Range payload ceiling, at most 65,280 generated
LZW entries, and an 8-MiB aggregate buffered-byte policy.

The CLI must initialize the public size-tagged config, set only public format
parameters and hard limits, query all three direction-specific workspace
regions and views alignment, and construct the transform through
`marc_lzw_dynamic_range_create()`. It must not name private record types,
recalculate opaque record sizes, or invoke a private C++ frame API.

Retain the existing output refusal and sibling `.tmp` protocol. Encoding or
decoding failure, malformed input, strict trailing data, write failure, close
failure, or rename failure must leave no requested destination or temporary
file. Test binary and empty round trips, overwrite refusal, malformed input,
and a valid stream with trailing data. This step adds no benchmark or
interoperability entry.

## DD-415: LZW Dynamic Range benchmark measures the public CLI profile

- Date: 2026-07-26
- Status: accepted

Add `lzw-dynamic-range` to the dependency-free benchmark with the exact DD-414
public profile: 65,536-byte raw frames, 131,072 packed LZW bytes, 262,149
range-payload bytes, maximum code width 16, 65,280 generated entries, and an
8-MiB aggregate policy.

For input extent `N` and nonempty frame count `K`, reserve checked complete
stream capacity `80 + 4N + 77K`. The `4N` term covers the conservative
`S <= 2N` and `P <= 2S + 5` payload relation; each frame contributes its
56-byte header, 16-byte descriptor, and five termination bytes. Overflow must
fail before allocating the encoded buffer.

Construct both directions only through the public config initializer,
requirements query, factory, process, and destroy lifecycle. Require one
untimed byte-exact round trip before timing fresh transforms. Report encoded
ratio, encode/decode throughput, all six queried workspace extents, and the
larger three-region sum as descriptive peak workspace. Add a one-iteration
smoke test with no performance threshold. This step adds no interoperability
entry.

## DD-416: Interoperability schema 17 appends LZW Dynamic Range once

- Date: 2026-07-26
- Status: accepted

Define interoperability schema 17 and codec set `marc-cli-v17` as the exact
twenty-seven-entry schema-16 order followed by `lzw-dynamic-range`. Retain the
deterministic 8,193-byte fixture and all existing manifest fields. The
generator must locally decode every archive before publishing the manifest.
The verifier must require exactly twenty-eight archives in canonical order,
validate all declared extents and SHA-256 values, decode every foreign archive,
and reproduce every archive byte for byte with the local encoder.

The compatibility test must generate and verify schema 17, reject a reordered
schema-17 manifest before archive decoding, remove only archive 28 to derive
schema 16, and continue the complete frozen conversion chain through schema 1.
No previous schema, archive order, codec set, stream representation, fixture,
or manifest field changes. Cross-platform interoperability remains unproven
until one pushed revision passes the established Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang bidirectional artifact procedure.

## DD-417: LZD Dynamic Range entropizes finalized reference-pair bytes

- Date: 2026-07-26
- Status: accepted

Reserve `lzd-dynamic-range` for LZD variant 1 followed by Dynamic Range Coder
variant 1 under format version 1.0. Preserve the standalone 16-byte LZD
parameters, empty entropy parameters, fixed eight-byte little-endian reference
pairs, and terminal absent-right value. Complete the token byte stream before
entropy processing; Dynamic Range consumes every resulting byte without
interpreting token, reference-field, or terminal-marker boundaries. Reset both
the LZD phrase dictionary and adaptive order-0 range model at every outer
frame.

For raw frame size `F`, use checked token staging bound
`S = 8 * ceil(F / 2)` and Dynamic Range payload bound `P = 2S + 5`. Bound
generated phrases by `min(floor(F / 2), configured_maximum)` and the iterative
expansion stack by that phrase count plus one. Retain the LZD composition
format cap `F <= 2^20`. The reference profile uses `F = 65,536`, giving
`S = 262,144`, `P = 524,293`, at most 32,768 generated phrases, and at most
32,769 expansion references.

Encoding freezes the canonical LZD token bytes before range planning. Decoding
range-decodes exactly the declared token-byte count into private staging, then
applies the ordinary LZD multiple-of-eight, backward-reference, terminal-
absence, phrase-length, and exact-raw-extent validation before iterative
private reconstruction and any raw publication. Error precedence is generic
header and extent validation, workspace admission, range descriptor and
payload validation, LZD validation, then private reconstruction and
publication.

Freeze raw `A` independently: LZD emits terminal token
`41 00 00 00 FF FF FF FF`; Dynamic Range variant 1 over those eight bytes
produces payload `00 40 FF FF C4 DC 92 F3 69 BC 8B 00` and descriptor
`(8, 12, 0)`. Record the complete 84-byte frame in the format document and
prove it by composing only the existing standalone LZD encoder, Dynamic Range
encoder, and generic serializers. This decision specifies bytes and a reserved
name only; it does not publish a combined validator, decoder, encoder,
streaming transform, factory, CLI, benchmark, fuzz target, completion matrix,
or interoperability entry.

## DD-418: LZD Dynamic Range validation stops at the token boundary

- Date: 2026-07-26
- Status: accepted

Admit the first combined `lzd-dynamic-range` implementation as a strict bounded
complete-frame validator only. Validate the stream profile, LZD parameters,
sequence, generic frame header, exact complete-frame extent, checked
`S = 8 * ceil(F/2)` token bound, token-width divisibility, one 16-byte Dynamic
Range descriptor, `P = 2S + 5` payload bound, every caller-owned capacity,
aligned phrase bytes, and the aggregate workspace limit before entropy output.

Parse the descriptor only after that admission succeeds. Range-decode exactly
the declared token-byte count into private caller-owned staging with exact
payload exhaustion, then invoke the existing LZD validator over the complete
span. Preserve LZD's backward-reference, terminal-absence, checked phrase-
length, exact declared raw extent, token count, dictionary-entry count, format
error, and phrase-table requirements.

Use stable error precedence: unsupported profile, truncated header, generic
header, complete-frame extent, token and entropy extents, caller workspace,
aggregate workspace, descriptor, range payload, then LZD token stream. On
every error the caller must discard staging and phrase contents; no raw byte is
reconstructed or published. Report the future iterative expansion requirement
as a diagnostic, but do not include unrequested expansion or raw staging in
this validator's workspace total. This step adds no private raw decoder,
encoder, streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion matrix, or interoperability entry.

## DD-419: LZD Dynamic Range reconstructs only into private raw staging

- Date: 2026-07-26
- Status: accepted

Extend DD-418 with a bounded complete-frame decoder that reconstructs the
already validated LZD token stream into caller-owned private raw staging.
Require raw capacity for the complete declared frame and expansion-stack
capacity for the checked phrase-count-plus-one requirement. Add both full
extents to aggregate workspace accounting before parsing the Dynamic Range
descriptor or producing token bytes.

Reuse the DD-418 validator without weakening or duplicating its header,
`S`/`P`, workspace, descriptor, payload-exhaustion, reference, terminal,
phrase-length, and exact-raw-extent checks. Only after all validation succeeds
may the existing iterative LZD decoder expand the staged token graph through
the validated phrase table and explicit `uint32_t` stack into private raw
storage. Preserve detailed LZD validation, format, and decode diagnostics.

On every failure the caller must discard token staging, phrase records,
expansion stack, and raw staging. No caller-visible raw output is accepted by
this API, so no byte is published at this boundary. This step adds no
transactional publication wrapper, encoder, streaming transform, profile
calculator, C ABI, CLI, benchmark, fuzz target, completion matrix, or
interoperability entry.

## DD-420: LZD Dynamic Range publishes only a complete successful frame

- Date: 2026-07-26
- Status: accepted

Add an internal caller-visible complete-frame decoder above DD-419. Require
destination capacity for the complete declared raw frame together with token
staging, aligned phrase records, explicit expansion-stack storage, and private
raw staging before parsing the Dynamic Range descriptor or producing entropy
output. Caller-visible output is not scratch and therefore is not counted in
aggregate internal workspace.

Run the unchanged DD-418 validation order and DD-419 private reconstruction.
Only after every generic-frame, range, LZD, capacity, bounds, graph, and exact-
extent check succeeds may the decoder copy the complete private raw span once
into the destination.

Preserve every existing combined error value and append a distinct output-
capacity error. On every failure, publish no destination byte. This step adds
no encoder, streaming transform, profile calculator, C ABI, CLI, benchmark,
fuzz target, completion matrix, or interoperability entry.

## DD-421: LZD Dynamic Range planning freezes tokens before range output

- Date: 2026-07-26
- Status: accepted

Add an exact-frame planner for `lzd-dynamic-range`. Require one nonempty raw
frame, validate the selected profile and caller-owned LZD encoder workspace,
and run the deterministic LZD plan. Require token staging for the exact planned
extent, then encode the complete canonical eight-byte reference-pair stream
before invoking Dynamic Range planning.

Plan Dynamic Range over that immutable token-byte extent and require the exact
payload to satisfy `P <= 2S + 5`. Count LZD encoder records, token staging, the
16-byte descriptor, and exact payload in one checked internal-workspace sum.
Construct and validate the complete generic frame header and report its exact
serialized extent without writing any serialized-frame byte.

Preserve every existing combined error value and append distinct input-size,
encoder-workspace, dictionary-encode, entropy-encode, and internal-consistency
errors. This step adds no serialized encoder, streaming transform, profile
calculator, C ABI, CLI, benchmark, fuzz target, completion matrix, or
interoperability entry.

## DD-422: LZD Dynamic Range encoding is plan-first and deterministic

- Date: 2026-07-27
- Status: accepted

Add the deterministic complete-frame encoder above DD-421. Invoke the exact
planner first so canonical LZD token bytes, exact range payload size, generic
frame fields, and aggregate workspace are fixed before serialized output is
considered. Require destination capacity for the complete planned extent before
writing any serialized byte.

Repeat Dynamic Range planning over the frozen token span and require its
payload extent to match DD-421. Serialize the generic frame header and 16-byte
descriptor explicitly, then encode the exact payload into its planned region.
The independent raw-`A` input must reproduce the complete 84-byte vector.

Preserve every existing combined error value and append a distinct serialized-
output-capacity error. Capacity and all planner failures leave serialized
output unchanged. This step adds no streaming transform, profile calculator,
C ABI, CLI, benchmark, fuzz target, completion matrix, or interoperability
entry.

## DD-423: LZD Dynamic Range streaming encode buffers one bounded frame

- Date: 2026-07-27
- Status: accepted

Add a bounded known-size streaming encoder above DD-422. Emit the ordinary
64-byte stream header and 16-byte LZD parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete
serialized representation through the deterministic exact-frame encoder, and
drain that immutable frame under arbitrary output starvation before accepting
bytes for the next frame.

At construction, validate the fixed pipeline, parameters, known original size,
largest raw frame, conservative `8 * ceil(F/2)` token staging, and LZD encoder
records. Before encoding each collected frame, count raw collection, exact
token staging, exact serialized frame, and encoder records in one checked
aggregate and require complete encoded-frame storage.

Input and output chunking alone must not change serialized bytes. `Flush` keeps
the logical stream open and does not shorten a frame. Retain `EndInput` while
the prefix or a frame drains, require its input to complete the declared known
size, reject `ResetBlock` and unknown flags, and make ended and error results
sticky. This step adds no streaming decoder, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-424: LZD Dynamic Range streaming decode validates before draining

- Date: 2026-07-27
- Status: accepted

Add the matching bounded known-size streaming decoder. Incrementally collect
and validate the fixed 80-byte stream prefix, then one 56-byte frame header.
Before admitting the frame body, enforce `S <= 8 * ceil(F/2)`,
`5 <= P <= 2S + 5`, one 16-byte descriptor, exact caller capacities for the
complete encoded frame, token staging, private raw staging, aligned LZD phrase
records, and expansion references, plus their checked aggregate workspace.

Collect exactly the admitted descriptor and payload, invoke DD-420's
transactional complete-frame decoder into private raw storage, and only then
drain that immutable raw frame. Do not collect a later frame while validated
raw bytes remain pending. Earlier complete frames may be published, but a
malformed later frame must publish none of its own bytes.

Require exact known-size completion and reject every prefix, header, or body
truncation, trailing byte, wrong pipeline, invalid extent, `ResetBlock`, and
unknown flag. Retain `EndInput` while a validated frame drains and make ended
and error states sticky. This step adds no profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-425: LZD Dynamic Range profiles separate byte and typed storage

- Date: 2026-07-27
- Status: accepted

Add an internal direction-specific profile calculator above DD-423 and DD-424.
For encoding, derive the largest raw frame, conservative
`S = 8 * ceil(F/2)` token staging, `2S + 5` Dynamic Range payload, complete
serialized-frame storage, and the exact LZD encoder-record count. Count every
region in one checked aggregate before returning any requirement.

For decoding, derive complete encoded-frame collection, bounded token staging,
private raw staging, conservative phrase records, and phrase expansion
references solely from validated local limits. Keep byte storage separate from
caller-allocated, properly aligned typed storage. Partition helpers must verify
the reported record counts, byte extent, expansion offset, and alignment before
producing typed spans; empty encoder-record storage uses zero bytes and neutral
alignment one.

This step adds no C ABI factory, CLI selector, benchmark, fuzz target,
completion matrix, or interoperability entry.

## DD-426: LZD Dynamic Range C ABI owns no caller storage

- Date: 2026-07-27
- Status: accepted

Publish a fixed-width `marc_lzd_dynamic_range_config` and three functions:
configuration initialization, direction-specific workspace requirements, and
transform creation. Retain ABI version 1 because this adds new symbols and a
new independently size-tagged structure without changing any existing layout
or behavior.

Map the C fields to DD-425 and return primary byte storage, combined secondary
byte storage, and one separately aligned opaque views region. On creation,
recompute the requirements, validate all pointers, capacities, reserved fields,
and alignment, partition typed LZD records internally, and construct exactly
the DD-423 encoder or DD-424 decoder. The transform borrows every workspace;
no allocator callback or private record layout crosses the ABI.

Add a pure C11 shared-library test that queries both directions, round-trips
`ABABX`, and rejects each short region, misaligned views, null output handle,
and nonzero reserved field. This step adds no CLI selector, benchmark, fuzz
target, completion matrix, or interoperability entry.

## DD-427: LZD Dynamic Range completion evidence is public-ABI only

- Date: 2026-07-27
- Status: accepted

Reuse the LZD plus Adaptive Huffman public completion schedules through only
the `marc_lzd_dynamic_range_*` configuration, requirements, factory, process,
and destroy lifecycle. Change only the entropy payload ceiling to `2S + 5` and
the public symbol family so evidence for the two LZD compositions cannot drift.

Cover empty input, every one-byte value, all byte values, repetitive and
patterned binary data, deterministic generated data, and lengths immediately
around the 64-byte frame boundary. Require byte-identical repeated encoding,
unchunked, one-byte, and mixed chunk schedules, round trip, and sticky
`EndOfStream`.

For a four-frame stream, independently corrupt the final sequence, truncate
the final payload, and append trailing data. Each failure may publish exactly
the first three validated frames, must preserve the final raw sentinel, and
must repeat its terminal status and error positions. This step adds no CLI
selector, benchmark, fuzz target, or interoperability entry.

## DD-428: LZD Dynamic Range fuzzing is fixed-memory and dual-path

- Date: 2026-07-28
- Status: accepted

Add one bounded decoder fuzz entry that exercises both the private complete-
frame decoder and the outer frame-committing streaming decoder. Cap accepted
input at 8,192 bytes, total raw output at 4,096 bytes, one raw frame at 1,024
bytes, canonical LZD token staging at 4,096 bytes, range payload at 8,192
bytes, phrase records at 512, and iterative expansion references at 513.
Allocate every byte, phrase, and expansion region as a fixed local array before
inspecting input.

Derive chunk sizes only within those arrays and stop after
`input_bound + output_bound + 32` calls. Abort on an invalid process result,
zero progress reported as Progress, impossible NeedInput after final input, or
the finite-call ceiling. Reuse the established bounded LZD harness with only
the entropy identity and combined entry points parameterized.

Permanent regressions must reject every proper prefix of a canonical stream,
saturated frame extents, and a nonzero reserved Dynamic Range descriptor byte.
All failures preserve the raw output sentinel and remain sticky. This step adds
no CLI, benchmark, or interoperability entry.

## DD-429: LZD Dynamic Range CLI is a fixed public-ABI adapter

- Date: 2026-07-28
- Status: accepted

Add the explicit `lzd-dynamic-range` selector to the existing transactional CLI
without changing the default codec. Use a 65,536-byte raw frame, the canonical
`S = 8 * ceil(F/2) = 262,144` LZD token ceiling, the
`P = 2S + 5 = 524,293` Dynamic Range payload ceiling, at most 65,536 dictionary
entries, and a 16-MiB aggregate buffered-byte policy.

The CLI must initialize the public size-tagged config, set only public format
parameters and hard limits, query all three direction-specific workspace
regions and views alignment, and construct the transform through
`marc_lzd_dynamic_range_create()`. It must not name private record types,
recalculate opaque record sizes, or invoke a private C++ frame API.

Retain the existing output refusal and sibling `.tmp` protocol. Encoding or
decoding failure, malformed input, strict trailing data, write failure, close
failure, or rename failure must leave no requested destination or temporary
file. Test binary and empty round trips, overwrite refusal, malformed input,
and a valid stream with trailing data. This step adds no benchmark or
interoperability entry.

## DD-430: LZD Dynamic Range benchmark measures the public CLI profile

- Date: 2026-07-28
- Status: accepted

Add `lzd-dynamic-range` to the dependency-free benchmark with the exact DD-429
public profile: 65,536-byte raw frames, 262,144 canonical LZD token bytes,
524,293 range-payload bytes, 65,536 dictionary entries, and a 16-MiB aggregate
policy.

For input extent `N` and nonempty frame count `K`, reserve checked complete
stream capacity `80 + 16*ceil(N/2) + 77K`. The pair term covers
`S = 8*ceil(N/2)` and `P <= 2S + 5`; each frame contributes its 56-byte header,
16-byte descriptor, and five termination bytes. Overflow must fail before
allocating the encoded buffer.

Construct both directions only through the public config initializer,
requirements query, factory, process, and destroy lifecycle. Require one
untimed byte-exact round trip before timing fresh transforms. Report encoded
ratio, encode/decode throughput, all six queried workspace extents, and the
larger three-region sum as descriptive peak workspace. Add a one-iteration
smoke test with no performance threshold. This step adds no interoperability
entry.

## DD-431: Interoperability schema 18 appends LZD Dynamic Range once

- Date: 2026-07-28
- Status: accepted

Define interoperability schema 18 and codec set `marc-cli-v18` as the exact
twenty-eight-entry schema-17 order followed by `lzd-dynamic-range`. Retain the
deterministic 8,193-byte fixture and all existing manifest fields. The
generator must locally decode every archive before publishing the manifest.

The verifier must require exactly twenty-nine archives in canonical order,
validate all sizes and SHA-256 values, decode each foreign archive, and require
byte-identical local re-encoding. Unknown, duplicate, missing, reordered, or
extra profiles remain errors.

Keep schemas 1 through 17 as explicit frozen codec sets. The compatibility test
must generate schema 18, reject a reordered schema-18 manifest, derive and
verify schema 17, then continue the existing schema-16-through-1 chain. This
step records local admission only; external cross-platform evidence requires
artifacts produced after push.

## DD-432: LZMW Dynamic Range entropizes finalized reference bytes

- Date: 2026-07-28
- Status: accepted

Reserve `lzmw-dynamic-range` for LZMW variant 1 followed by Dynamic Range
Coder variant 1 under format version 1.0. Preserve the standalone 16-byte LZMW
parameters, empty entropy parameters, and fixed four-byte little-endian
references. Complete the reference byte stream before entropy processing;
Dynamic Range consumes every byte without interpreting reference boundaries.
Reset both the LZMW phrase dictionary and adaptive order-0 range model at every
outer frame.

For raw frame size `F`, use checked reference staging bound `S = 4F` and
Dynamic Range payload bound `P = 2S + 5 = 8F + 5`. Bound generated phrases by
the lesser of `max(F - 1, 0)`, the configured LZMW maximum, and the local
decoder limit; bound the iterative expansion stack by that phrase count plus
one for a nonempty frame. Retain the LZMW composition format cap
`F <= 2^20`. The reference profile uses `F = 65,536`, giving `S = 262,144`,
`P = 524,293`, at most 65,535 generated phrases, and at most 65,536 expansion
references.

Encoding freezes canonical LZMW reference bytes before range planning.
Decoding range-decodes exactly the declared reference-byte count into private
staging, then applies ordinary LZMW reference alignment, prior-reference,
adjacent-phrase graph, and exact-raw-extent validation before iterative private
reconstruction and any raw publication. Error precedence is generic header and
extent validation, workspace admission, range descriptor and payload
validation, LZMW validation, then private reconstruction and publication.

For raw `A`, independently freeze LZMW reference `41 00 00 00`; Dynamic Range
variant 1 over those four bytes produces payload
`00 40 FF FF BF 00 00 00` and descriptor `(4, 8, 0)`. Record the complete
80-byte frame in the format document and prove it by composing only the
existing standalone LZMW encoder, Dynamic Range encoder, and generic
serializers. This decision specifies bytes and a reserved name only; it does
not publish a combined implementation or interoperability entry.

## DD-433: LZMW Dynamic Range validation stops at the reference boundary

- Date: 2026-07-28
- Status: accepted

Admit the first combined `lzmw-dynamic-range` implementation as a strict
bounded complete-frame validator only. Validate the exact stream profile,
LZMW parameters, sequence, generic frame header, exact complete-frame extent,
checked `S = 4F` reference bound, four-byte alignment, one 16-byte Dynamic
Range descriptor, `P = 2S + 5` payload bound, every caller-owned capacity,
aligned phrase bytes, and aggregate validation workspace before entropy
output.

Parse the descriptor only after admission succeeds. Range-decode exactly the
declared reference-byte count into private caller-owned staging with exact
payload exhaustion, then invoke the existing LZMW validator over that complete
span. Preserve LZMW's literal/prior-reference validation, bounded adjacent-
phrase construction, checked phrase lengths, exact declared raw extent, token
and dictionary-entry counts, stable format error, and phrase-table
requirements.

On success, reduce the reported expansion-stack ceiling from the conservative
phrase capacity to the actual generated-phrase count plus one for a nonempty
frame. Reconstruct and publish no raw byte. On every failure, the caller must
discard reference and phrase workspace. This decision adds no private raw
decoder, transactional publication, encoder, stream transform, public factory,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-434: LZMW Dynamic Range reconstruction remains private

- Date: 2026-07-28
- Status: accepted

Add a bounded complete-frame decoder that retains DD-433's exact validation
order and reconstructs only into caller-owned private raw staging. Before
entropy output, require the complete raw extent and the conservative expansion
stack derived from phrase capacity and count their bytes with the descriptor,
payload, reference staging, and aligned phrase records against
`max_internal_buffered_bytes`.

After strict range exhaustion and complete LZMW validation succeed, reduce the
active expansion span to the actual generated-phrase count plus one for a
nonempty frame. Invoke the existing iterative LZMW decoder over only that
validated graph. Propagate its stable validation, format, and decode errors;
an unexpected reconstruction failure is a distinct combined-frame error.

No caller-visible output span exists at this boundary. On every failure, the
caller discards reference, phrase, expansion, and raw staging. Prove a literal
frame and a phrase-reference frame, one-entry-short raw and expansion storage,
aggregate workspace one byte short, and unchanged raw guards after descriptor
or reference failure. This decision adds no publication boundary, encoder,
streaming transform, public factory, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-435: LZMW Dynamic Range frame publication is transactional

- Date: 2026-07-28
- Status: accepted

Add an internal complete-frame decoder that accepts a distinct caller-visible
output span. Before descriptor parsing or entropy output, retain all DD-433 and
DD-434 admission checks and additionally require capacity for the complete
declared raw frame. A one-byte-short output must leave reference staging,
phrase records, expansion stack, private raw staging, and caller output
unmodified.

After strict range exhaustion, complete LZMW graph validation, and successful
iterative private reconstruction, copy exactly `raw_size` bytes from private
raw staging to caller output once. Never expose partially reconstructed raw
bytes. Descriptor, payload, reference, phrase, workspace, limit, or
reconstruction failure leaves caller output unchanged.

Prove publication for the single-literal vector and a generated-phrase frame.
Prove preflight atomicity with short output and post-admission atomicity with
descriptor and forward-reference failures. This decision adds no frame
encoder, streaming transform, public factory, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-436: LZMW Dynamic Range planning freezes reference bytes

- Date: 2026-07-28
- Status: accepted

Add an internal exact-frame planner for the inverse of DD-433 through DD-435.
Validate the exact stream profile, LZMW parameters, nonempty input extent, and
frame-local bounds. Determine and require the bounded LZMW encoder-record
capacity before reference staging can change. Plan the deterministic parse,
require the exact checked `S <= 4F` reference extent and staging capacity, then
serialize all canonical four-byte references into caller-owned staging.

Plan Dynamic Range only over that frozen reference span. Require its exact
payload to fit `P <= 2S + 5` and 32-bit frame fields. Count encoder records,
reference staging, the 16-byte descriptor, and exact payload against
`max_internal_buffered_bytes`. Validate the synthesized generic frame header
with sequence and already-committed output context, and report the checked
complete frame extent without writing serialized output.

Prove the exact raw-`A` reference, descriptor, payload and 80-byte extent;
repeat a generated-phrase plan byte-identically; reject encoder records and
reference staging one entry short before staging mutation; and reject aggregate
workspace one byte short, empty input, and a frame-size mismatch. This decision
adds no serialized frame encoder, streaming transform, public factory, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-437: LZMW Dynamic Range encoding is plan-first and deterministic

- Date: 2026-07-28
- Status: accepted

Add the deterministic complete-frame encoder above DD-436. Invoke the exact
planner first so canonical LZMW reference bytes, exact range payload size,
generic frame fields, and aggregate workspace are fixed before serialized
output is considered. Require destination capacity for the complete planned
extent before writing any serialized byte.

Repeat Dynamic Range planning over the frozen reference span and require its
payload extent to match DD-436. Serialize the generic frame header and 16-byte
descriptor explicitly, then encode the exact payload into its planned region.
The independent raw-`A` input must reproduce the complete 80-byte vector.

Preserve every existing combined error value and append a distinct serialized-
output-capacity error. Capacity and all planner failures leave serialized
output unchanged. This step adds no streaming transform, profile calculator,
C ABI, CLI, benchmark, fuzz target, completion matrix, or interoperability
entry.

## DD-438: LZMW Dynamic Range streaming encode buffers one bounded frame

- Date: 2026-07-28
- Status: accepted

Add a bounded known-size streaming encoder above DD-437. Emit the ordinary
64-byte stream header and 16-byte LZMW parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete
serialized representation through the deterministic exact-frame encoder, and
drain that immutable frame under arbitrary output starvation before accepting
bytes for the next frame.

At construction, validate the fixed pipeline, parameters, known original size,
largest raw frame, conservative `4F` reference staging, and LZMW encoder
records. Before encoding each collected frame, count raw collection, exact
reference staging, exact serialized frame, and encoder records in one checked
aggregate and require complete encoded-frame storage.

Input and output chunking alone must not change serialized bytes. `Flush` keeps
the logical stream open and does not shorten a frame. Retain `EndInput` while
the prefix or a frame drains, require its input to complete the declared known
size, reject `ResetBlock` and unknown flags, and make ended and error results
sticky. This step adds no streaming decoder, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-439: LZMW Dynamic Range streaming decode validates before draining

- Date: 2026-07-28
- Status: accepted

Add the matching bounded known-size streaming decoder. Incrementally collect
and validate the fixed 80-byte stream prefix, then one 56-byte frame header.
Before admitting the frame body, enforce `S <= 4F`,
`5 <= P <= 2S + 5`, one 16-byte descriptor, exact caller capacities for the
complete encoded frame, reference staging, private raw staging, aligned LZMW
phrase records, and expansion references, plus their checked aggregate
workspace.

Collect exactly the admitted descriptor and payload, invoke DD-434's private
complete-frame decoder into private raw storage, and only then drain that
immutable raw frame. Do not collect a later frame while validated raw bytes
remain pending. Earlier complete frames may be published, but a malformed
later frame must publish none of its own bytes.

Require exact known-size completion and reject every prefix, header, or body
truncation, trailing byte, wrong pipeline, invalid extent, `ResetBlock`, and
unknown flag. Retain `EndInput` while a validated frame drains and make ended
and error states sticky. This step adds no profile calculator, C ABI, CLI,
benchmark, fuzz target, completion matrix, or interoperability entry.

## DD-440: LZMW Dynamic Range profiles separate byte and typed storage

- Date: 2026-07-28
- Status: accepted

Add an internal direction-specific profile calculator above DD-438 and DD-439.
For encoding, derive the largest raw frame, conservative `S = 4F` reference
staging, `2S + 5` Dynamic Range payload, complete serialized-frame storage,
and the exact LZMW encoder-record count. Count every region in one checked
aggregate before returning any requirement.

For decoding, derive complete encoded-frame collection, bounded reference
staging, private raw staging, conservative phrase records, and phrase expansion
references solely from validated local limits. Keep byte storage separate from
caller-allocated, properly aligned typed storage. Partition helpers must verify
the reported record counts, byte extent, expansion offset, and alignment before
producing typed spans; empty encoder-record storage uses zero bytes and neutral
alignment one.

This step adds no C ABI factory, CLI selector, benchmark, fuzz target,
completion matrix, or interoperability entry.

## DD-441: LZMW Dynamic Range C ABI borrows all workspace

- Date: 2026-07-28
- Status: accepted

Publish `marc_lzmw_dynamic_range_config` with fixed-width fields and matching
configuration initialization, direction-specific workspace requirements, and
opaque-transform factory functions. Retain C ABI version 1 and the existing
status, process, end-state, and destroy contracts.

Map encoding to DD-440's raw-frame primary region, reference-plus-encoded-frame
secondary region, and aligned encoder records. Map decoding to its encoded-
frame primary region, reference-plus-private-raw secondary region, and aligned
phrase-plus-expansion records. Recalculate the complete profile during factory
creation, validate all pointers, capacities, reserved fields, and alignment,
partition typed records privately, and borrow every region until transform
destruction. No allocator callback or C++ record type crosses the ABI.

Prove the lifecycle from pure C11 with raw `ABABX` and two-byte frames. Require
exact queried small-limit regions, a complete round trip, a null output handle
on every failed creation, and rejection of every one-byte-short region,
misaligned views, null transform output, and nonzero reserved state. This
decision adds no completion matrix, fuzz target, CLI selector, benchmark, or
interoperability entry.

## DD-442: LZMW Dynamic Range completion is public-ABI only

- Date: 2026-07-28
- Status: accepted

Audit only the published C configuration, requirements query, factory,
process, and destroy functions. Use 64-byte raw frames, the checked 256-byte
canonical-reference ceiling, the `2S + 5` Dynamic Range payload bound, 63
dictionary entries, and a 65,536-byte aggregate limit. Allocate both
direction-specific opaque views from queried byte counts and alignment;
encoding zero or one raw byte requires no generated LZMW entry and therefore
no views bytes.

Cover empty input, every one-byte value, the ordered byte alphabet, repeated
data, binary patterns, deterministic pseudo-random bytes, and lengths 63, 64,
and 65. Require repeated encoding to be byte-identical and terminal success to
be sticky. For a 193-byte four-frame stream, require exact bytes and round
trips under unchunked, one-byte, and mixed chunk schedules.

Independently corrupt the final frame sequence, truncate its final byte, and
append trailing data. Every error must be sticky, preserve byte and bit
positions, publish exactly the first 192 validated bytes, and leave the final
output sentinel unchanged. This completes public-ABI evidence only; it adds no
fuzz target, CLI selector, benchmark, or interoperability entry.

## DD-443: LZMW Dynamic Range fuzzing fixes every decoder region

- Date: 2026-07-28
- Status: accepted

Add one bounded decoder fuzz entry point that truncates supplied input to
8,192 bytes and exercises both the exact complete-frame private decoder after
a valid 80-byte prefix and the incremental stream decoder for every case.
Fix total raw output at 4,096 bytes, one raw frame at 1,024 bytes, canonical
LZMW reference staging at 4,096 bytes, compressed payload at 8,192 bytes, the
phrase table at 1,023 records, and the iterative expansion stack at 1,024
references. Include every byte and typed region in one fixed aggregate limit
before processing metadata.

Derive partial input and output chunks only from current bytes, cap processing
at `8,192 + 4,096 + 32` calls, and abort only for an invalid process result or
impossible stall. Retain one repository-authored truncated-magic seed and keep
generated mutations outside the source tree.

Add permanent ordinary-test regressions requiring every proper truncation of
the canonical `ABABX` stream, saturated generic frame extents, and a nonzero
final reserved byte in the 16-byte Dynamic Range descriptor to fail
atomically with sticky category and position. This step adds no CLI selector,
benchmark, or interoperability entry.

## DD-444: LZMW Dynamic Range receives a transactional CLI selector

- Date: 2026-07-28
- Status: accepted

Add the explicit `lzmw-dynamic-range` selector to the existing transactional
CLI without changing the default codec. Use a 65,536-byte raw frame, the
canonical `S = 4F = 262,144` LZMW reference ceiling, the
`P = 2S + 5 = 524,293` Dynamic Range payload ceiling, at most 65,536 generated
entries, and a 16-MiB aggregate internal policy.

Initialize configuration, query all direction-specific workspace extents and
alignment, create the transform, process it, and destroy it only through the
public C ABI. Do not reproduce opaque encoder-entry, phrase-record, or
expansion-stack layouts in the tool. Retain the common temporary-file commit,
destination overwrite refusal, strict trailing-data rejection, fixed input
size, and bounded 64-KiB I/O loop.

Prove a multi-frame binary round trip, repeated-output refusal, malformed input
cleanup, appended-data rejection, and empty-stream handling through the generic
CLI regression. Benchmark and interoperability admission remain separate
steps.

## DD-445: LZMW Dynamic Range benchmark measures the public CLI profile

- Date: 2026-07-28
- Status: accepted

Add `lzmw-dynamic-range` to the dependency-free benchmark with the exact DD-444
public profile: 65,536-byte raw frames, 262,144 canonical LZMW reference bytes,
524,293 range-payload bytes, 65,536 generated entries, and a 16-MiB aggregate
policy.

Reserve complete-stream encoded capacity with checked arithmetic as
`80 + 8N + 77K`, where `N` is total raw input and `K` is the nonempty frame
count. This follows from `S <= 4N`, `P <= 2S + 5`, the 16-byte descriptor, and
56-byte generic header. Obtain all encoder and decoder workspace extents and
opaque alignment from the public C query.

Before timing, encode and decode once through fresh public transforms and
require exact byte equality. Each timed sample also creates a fresh transform.
Report ratio, directional throughput, each workspace region, and peak
caller-reserved workspace without imposing a performance floor.
Interoperability admission remains separate.

## DD-446: Interoperability schema 19 appends LZMW Dynamic Range

- Date: 2026-07-28
- Status: accepted

Define interoperability schema 19 and codec set `marc-cli-v19` as the exact
twenty-nine-entry schema-18 order followed by `lzmw-dynamic-range`. Retain the
deterministic 8,193-byte fixture and all existing manifest fields. The
generator must round-trip every archive locally before publishing the
manifest.

The verifier must require exactly thirty archives in canonical order, validate
leaf-only names, complete revision, sizes and SHA-256 values, decode every
foreign archive, and reproduce every archive byte-identically with the local
CLI. Keep schemas 1 through 18 explicit rather than deriving their meaning from
the current profile list.

The compatibility regression must reject a reordered schema-19 manifest before
archive decoding, remove only archive 30 to derive schema 18, then exercise the
existing schema-18-through-schema-1 conversion chain. Cross-platform admission
remains pending until artifacts from the pushed revision are exchanged.

## DD-447: LZ77 rANS entropizes the finalized token byte stream

- Date: 2026-07-28
- Status: accepted

Reserve `lz77-rans` for LZ77 variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZ77 parameter extension,
empty entropy parameters, and canonical 16-byte token serialization. Complete
the token byte stream before entropy processing. rANS treats it as untyped
bytes, so a block may split a token but cannot cross an outer frame. Reset the
LZ77 window and every rANS model and state at each frame.

For raw frame size `F`, use checked token bound `S = 16F`. For nonzero rANS
block size `B`, require `K = ceil(S/B)`, payload bound `P = S + 8K`, and exact
descriptor extent `528K`. Retain the LZ77 composition cap `F <= 2^20`.
Validate generic extents and all rANS descriptors, tables, state paths,
terminal states, and payload exhaustion before reconstructing the exact token
region in private staging. Only then validate 16-byte alignment, LZ77
references, overlap semantics, and exact raw extent before any private raw
reconstruction or publication.

For raw `A`, independently freeze the 16-byte Literal token. Its rANS model is
`00:3840, 41:256`, final-state payload is
`00 A5 22 10 15 00 00 00`, and the complete frame is 592 bytes. Prove this by
composing only the existing standalone LZ77 encoder, rANS encoder, and generic
serializers. This decision specifies bytes and a reserved name only; it does
not publish a combined decoder, encoder, stream transform, C factory, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-448: LZ77 rANS validation stops at the token boundary

- Date: 2026-07-28
- Status: accepted

Admit the first combined `lz77-rans` implementation as a strict bounded
complete-frame validator only. Validate the exact stream profile, LZ77
parameters, sequence, generic frame header, complete frame extent,
`S <= 16F` token bound and alignment, exact
`K = ceil(S/B)` block count, exact `528K` descriptor bytes, bounded
`8K <= P <= S + 8K` payload, caller-owned token and view capacities, and
their aggregate workspace before entropy output.

Parse every descriptor only after admission succeeds. Validate every rANS
block's model, state path, terminal state, and exact payload exhaustion before
decoding any block. Only after that complete validation pass may a second pass
reconstruct exactly `S` token bytes into private caller-owned staging. Invoke
the existing LZ77 validator over the complete span and preserve its stable
token index, format error, reference, overlap, and exact raw-extent checks.

No raw staging or output span exists at this boundary. On every failure the
caller discards token and view workspace. Prove the 592-byte Literal vector,
a token split across four rANS blocks, every truncation, trailing data, short
storage, aggregate admission one byte short, malformed descriptors, a
malformed later block with untouched token staging, invalid reconstructed
LZ77 tokens, impossible entropy extents, and profile rejection. This decision
adds no private raw decoder, transactional publication, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-449: LZ77 rANS reconstruction remains private

- Date: 2026-07-28
- Status: accepted

Extend DD-448 with a bounded complete-frame decoder that reconstructs only
into caller-owned private raw staging. Require capacity for the complete
declared raw frame before descriptor parsing or entropy output, and count
those `F` bytes together with descriptor, payload, token staging, and rANS
views against `max_internal_buffered_bytes`.

Retain DD-448's two-pass entropy rule and complete LZ77 validation. Only after
all rANS blocks and all LZ77 token semantics succeed may the existing
allocation-free decoder reconstruct literals and forward overlapping matches
from immutable token staging into exactly `F` private raw bytes. Preserve its
stable validation, format, and decode error categories; an unexpected failure
after successful validation is a distinct dictionary-decode error.

No caller-visible output span exists. On every failure the caller discards
views, token staging, and raw staging. Prove the Literal frame, a distance-one
overlapping terminal match, raw staging one byte short before token mutation,
aggregate workspace one byte short including raw bytes, and unchanged raw
sentinels after a malformed later rANS block or invalid decoded token. This
decision adds no transactional publication, encoder, streaming transform,
profile calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-450: LZ77 rANS frame publication is transactional

- Date: 2026-07-28
- Status: accepted

Add a caller-visible complete-frame decoder above DD-449. Require capacity for
the entire declared raw frame in a distinct output span before descriptor
parsing, entropy output, token staging mutation, or private raw mutation.
Output remains caller-owned publication storage and is not added to the
internal workspace total already fixed by DD-449.

Retain DD-448's complete entropy and token validation and DD-449's private
reconstruction. Copy exactly `F` bytes from private raw staging to caller
output once, only after the LZ77 decoder succeeds. Preserve all layered error
details and return without publication on every earlier failure.

Prove the Literal frame publishes only its declared byte while preserving
guards, a one-byte-short output fails before private mutation, and malformed
later rANS state or invalid reconstructed token leaves both private raw and
caller output sentinels unchanged. This decision adds no encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-451: LZ77 rANS planning freezes tokens before counting blocks

- Date: 2026-07-28
- Status: accepted

Add an encoder-side exact-frame planner for the inverse of DD-447. Require one
nonempty raw frame and validate the fixed profile and LZ77 parameters. First
run the deterministic LZ77 plan, admit caller-owned staging for the exact
token extent `S`, and encode the complete canonical token sequence once.
Reject short staging before modifying it.

Over the frozen token bytes, plan every consecutive rANS block of at most `B`
bytes without serialized output. Accumulate exact block count `K`, descriptor
extent `528K`, payload extent `P`, and complete serialized extent
`56 + 528K + P` with checked arithmetic. Enforce the block-count ceiling and
count token staging, planned descriptors, and planned payload against the
aggregate internal-buffer limit. Validate the resulting generic frame header.

Prove the one-Literal plan reproduces `S=16`, `K=1`, descriptor size 528,
payload size 8, and complete size 592. Split those token bytes at `B=5` and
require four blocks, reject short staging without mutation, reject empty and
unexpected raw extents, and exercise block-count and aggregate-workspace
ceilings. Callers discard private staging after any later failure. This
decision adds no serialized frame encoder, streaming transform, profile
calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-452: LZ77 rANS frame encoding admits the complete destination

- Date: 2026-07-29
- Status: accepted

Add the deterministic complete-frame encoder above DD-451. Run the exact
planner first and require capacity for the complete serialized extent before
writing any frame byte. Input, canonical token staging, and serialized output
remain mutually non-overlapping caller-owned spans.

After admission, serialize the validated generic frame header. Re-plan each
rANS block over immutable token staging, serialize its 528-byte descriptor in
the contiguous descriptor region, and encode its exact payload in the
contiguous payload region. Require every repeated plan and encoded payload
extent to agree with the first pass, consume exactly `S` token bytes and `P`
payload bytes, and report any disagreement as an internal invariant error.

Prove byte-for-byte equality with the independent 592-byte one-Literal frame.
At `B=5`, require deterministic repeated output, equality with a frame
assembled only from standalone components, four descriptor/payload blocks,
and complete decode back to raw `A`. Submit a 591-byte sentinel destination
for the 592-byte frame and require rejection without modifying any byte. This
decision adds no streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-453: LZ77 rANS streaming encode retains one completed frame

- Date: 2026-07-29
- Status: accepted

Add a bounded known-size streaming encoder above DD-452. Serialize the
ordinary stream header and LZ77 parameter extension into a fixed prefix.
Collect at most one configured outer raw frame in caller-owned storage,
prepare it through the exact planner and deterministic complete-frame encoder,
and retain that immutable serialized frame until arbitrary output starvation
has drained it completely.

At construction, validate the fixed profile, LZ77 parameters, known original
size, largest raw frame, conservative `16F` token staging, and prefix
serialization. Before each frame is encoded, count raw collection, exact token
staging, and exact serialized-frame storage in one checked aggregate. Do not
reuse any of those extents until the pending frame has drained.

Require chunking-independent bytes with one-byte input and output. `Flush`
keeps a partial frame open. Preserve `EndInput` while prefix or frame bytes
remain pending, require it to accompany every remaining declared input byte,
return stable terminal states, reject `ResetBlock` and unknown flags, and emit
only the prefix for empty input. Exercise constructor storage, completed-frame
storage, and aggregate-limit failures. This decision adds no streaming
decoder, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-454: LZ77 rANS streaming decode commits only complete frames

- Date: 2026-07-29
- Status: accepted

Add the bounded streaming decoder matching DD-453. Incrementally collect the
80-byte stream prefix, one generic frame header, and that frame's exact
descriptor/payload body. Before accepting the body, require caller-owned
storage for the complete serialized frame, declared rANS block views,
canonical token extent, and private raw extent. Count all four used extents in
one checked aggregate.

Invoke DD-450's private complete-frame decoder only after the full frame has
been collected. Retain its reconstructed raw bytes unchanged while draining
under arbitrary output starvation. Advance sequence and committed raw extent
only after private decode succeeds. A malformed later frame may follow already
committed earlier frames but must publish no part of itself.

Prove one-byte input and output round-trip of DD-453's multi-frame stream,
stable completion, first-frame-only publication before a corrupt second rANS
descriptor, each caller workspace boundary including views, the aggregate
limit, every final-byte truncation, trailing data, empty input, `Flush`,
premature `EndInput`, and `ResetBlock`. This decision adds no profile
calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-455: LZ77 rANS profile exposes bytes plus a private view count

- Date: 2026-07-29
- Status: accepted

Add an internal direction-specific profile calculator above DD-453 and
DD-454. The reference configuration uses 65,536-byte outer frames and
65,536-byte rANS blocks, retains LZ77 and scalar-rANS variant 1, and accepts
known original size plus ordinary LZ77 parameters.

For largest nonempty raw frame `F = min(original_size, frame_size)`, derive
the conservative token ceiling `S = 16F`, block ceiling `K = ceil(S/B)`,
descriptor bytes `D = 528K`, payload bytes `P = S + 8K`, and serialized-frame
ceiling `E = 56 + D + P`. Report encoder raw, token, and serialized-frame byte
regions only after checking generic limits, 32-bit format fields, the one-MiB
composition cap, and aggregate `F + S + E`. Return zero active frame
workspaces for empty known-size input.

Derive decoder requirements solely from validated local limits: serialized
frame bytes, bounded token bytes, private raw bytes, and an rANS block-view
count. Keep `RansBlockView` private so a later C ABI can expose only checked
opaque bytes and alignment. Use checked arithmetic and checked `size_t`
conversion throughout, reset outputs on failure, and map profile errors to
stable core categories. Prove canonical and short-frame ceilings, empty input,
block/payload/aggregate/frame limits, invalid LZ77 parameters, decoder-limit
and overflow behavior, stable mapping, and construction of both streaming
directions from the reported requirements. This step adds no C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-456: LZ77 rANS enters ABI v1 through three opaque regions

- Date: 2026-07-29
- Status: accepted

Add named public C configuration initialization, direction-specific workspace
requirements, and a transform factory without changing ABI version 1 or any
existing public layout. Mirror the combined LZ77 profile fields, adding the
rANS block size and block-count limit. Keep rANS table and view types absent
from the public header.

For encoding, report raw collection as primary, token staging plus serialized
frame as secondary, and zero views with alignment one. For decoding, report
serialized frame as primary, token plus private raw staging as secondary, and
`block_view_count * sizeof(RansBlockView)` opaque view bytes with the internal
alignment. Use checked additions and multiplication.

The factory must query and repeat profile validation, reject invalid, short,
or misaligned regions before construction, partition secondary only at the
checked token extent, borrow every region for the transform lifetime, use
`nothrow` construction, and leave the output handle null on every failure.
Prove the complete C11 lifecycle with raw `ABABABX`, exact queried region
values, byte-exact round trip, short view rejection, and reserved-field
rejection. This step adds no completion matrix, fuzz target, CLI, benchmark,
completion claim, or interoperability entry.

## DD-457: LZ77 rANS completion is proven through the C ABI

- Date: 2026-07-29
- Status: accepted

Add a public-ABI completion matrix using fixed 64-byte raw frames and 64-byte
rANS blocks. Exercise only the published configuration, requirements query,
factory, process, and destroy lifecycle; allocate and align all three
workspaces from each direction's query.

Cover empty input, every one-byte symbol, all byte values, repetitive and
patterned binary input, deterministic generated input, and lengths immediately
below, equal to, and above the outer-frame boundary. Encode every required
class twice and require identical bytes before round-trip decode.

For a 193-byte stream, require unchunked, one-byte, and mixed schedules to
produce the identical multi-frame representation and decoded bytes. Repeated
successful terminal calls remain `EndOfStream`. Corrupt, truncate, and append
data to the fourth frame independently; each decoder may publish exactly the
first three validated 64-byte frames, must preserve the final raw sentinel,
and must repeat the same sticky error category and position. This step adds no
fuzz target, CLI, benchmark, completion claim, or interoperability entry.

## DD-458: LZ77 rANS fuzzing is fixed-memory and dual-boundary

- Date: 2026-07-29
- Status: accepted

Add one libFuzzer entry point that submits every bounded input to both the
private complete-frame staging decoder and the public-form incremental
streaming decoder. Truncate supplied input to 8,192 bytes. Fix total output at
4,096 bytes, one raw frame at 1,024 bytes, token staging at 4,096 bytes,
payload at 8,192 bytes, and rANS metadata at eight `RansBlockView` records.
Allocate no workspace from serialized metadata.

For the complete-frame path, parse only the exact LZ77/rANS prefix and
parameters before passing the remaining extent to the strict private decoder.
For streaming, derive bounded input and output chunk sizes from bytes and cap
calls at `maximum_input + maximum_output + 32`. Abort on an invalid
`ProcessResult`, progress without progress, input exhaustion reported as
`NeedInput`, or exhaustion of the finite call budget.

Seed the corpus with truncated magic. Add permanent regression tests requiring
atomic output and sticky errors for every truncation of a canonical stream,
saturated frame-length fields, and an invalid rANS frequency table. Build with
libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer and complete a
bounded smoke run. This step adds no CLI, benchmark, completion claim, or
interoperability entry.

## DD-459: LZ77 rANS CLI admission uses only the public profile

- Date: 2026-07-29
- Status: accepted

Add the explicit selector `lz77-rans` to the existing transactional file
adapter. Fix both raw frames and rANS blocks at 65,536 bytes. Derive
`S = 1,048,576` canonical token bytes, `K = 16` blocks, `P = 1,048,704`
payload bytes, `528K = 8,448` descriptor bytes, and the encoder aggregate
`F + S + 56 + 528K + P = 2,171,320` bytes with checked fixed-profile
arithmetic.

Configuration initialization, direction-specific requirements, transform
creation, processing, and destruction must use only `marc_lz77_rans_*` and
the common public C ABI. The CLI must not name private rANS view types or
reproduce workspace partitions. Reuse the shared temporary-file path and
prove nonempty and empty round trips, overwrite refusal, malformed-stream
cleanup, and strict trailing-data rejection. This step adds no benchmark
adapter, readiness claim, interoperability schema, or external evidence.

## DD-460: LZ77 rANS benchmark verifies before measuring

- Date: 2026-07-29
- Status: accepted

Add `lz77-rans` to the dependency-free benchmark through only the public C
requirements, factory, process, and destroy lifecycle. Use DD-459's
65,536-byte raw frame and entropy block, 1,048,576-byte token ceiling,
sixteen-block ceiling, 1,048,704-byte payload ceiling, and 2,171,320-byte
encoder aggregate policy.

Reserve complete-stream output with checked arithmetic as
`80 + 16N + 8632K`, where `N` is total raw input and `K` is the nonempty frame
count. The per-frame term is one 56-byte generic header, sixteen 528-byte
descriptors, and sixteen eight-byte final states. This conservative bound
allows final-short frames to reserve fewer actual blocks without changing
the format.

Query encoder and decoder workspaces independently, verify a byte-exact round
trip before timing, and create each timed transform outside the elapsed
region. Report ratio, direction-specific elapsed time and throughput, all six
workspace extents, and peak caller-reserved workspace. Performance values are
observations rather than thresholds. This step changes no format or C ABI and
adds no interoperability entry or external evidence.

## DD-461: Interoperability schema 20 appends LZ77 rANS

- Date: 2026-07-29
- Status: accepted

Define interoperability schema 20 and codec set `marc-cli-v20` as the exact
thirty-entry schema-19 order followed by `lz77-rans`. Reuse the deterministic
8,193-byte binary fixture and the unchanged public CLI representation. Each
bundle must contain exactly thirty-one archives in canonical order with
recorded size and SHA-256 after local decode equality succeeds.

The verifier must select an explicit expected profile array for every schema
1 through 20, reject unknown or duplicate profiles and any order change,
decode every foreign archive to the fixture, and re-encode every profile
byte-identically. The compatibility regression must reject a reordered
schema-20 manifest, then derive schema 19 by removing only the final
`lz77-rans` archive and changing only version and codec set before exercising
the unchanged schema-19-through-schema-1 chain.

This is local generation and verification evidence. Cross-platform canonical
bytes remain unproven until CI artifacts and an independent platform complete
the four established verification directions.

## DD-462: LZSS rANS entropizes the finalized variable-token stream

- Date: 2026-07-29
- Status: accepted

Reserve `lzss-rans` for LZSS variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZSS parameter extension,
empty entropy parameters, and canonical variable-length token serialization.
Complete the token byte stream before entropy processing. rANS treats it as
untyped bytes, so a block may split a two-byte Literal or nine-byte Match but
cannot cross an outer frame. Reset the LZSS window and every rANS model and
state at each frame.

For raw frame size `F`, use the checked token bound `S <= 2F`. For nonzero
rANS block size `B`, require `K = ceil(S/B)`, payload bounds
`8K <= P <= S + 8K`, and exact descriptor extent `528K`. Retain the
composition cap `F <= 2^20`. Validate generic extents and all rANS descriptors,
models, state paths, terminal states, and payload exhaustion before
reconstructing the exact private token region. Only then parse the complete
LZSS token grammar and validate tags, field truncation, distance, match length,
overlap semantics, and exact raw extent before any raw reconstruction or
publication.

For raw `A`, independently freeze Literal token `00 41`. Its normalized rANS
model is `00:2048, 41:2048`, final-state payload is
`00 10 00 00 02 00 00 00`, and the complete frame is 592 bytes. Prove this by
composing only the existing standalone LZSS encoder, rANS encoder, and generic
serializers. This decision specifies bytes and a reserved name only; it does
not publish a combined decoder, encoder, stream transform, C factory, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-463: LZSS rANS validation stops before raw reconstruction

- Date: 2026-07-30
- Status: accepted

Admit the first combined `lzss-rans` implementation as a strict bounded
complete-frame validator only. Validate the exact stream profile, LZSS
parameters, sequence, generic frame header and frame extent, `0 < S <= 2F`,
exact `K = ceil(S/B)` block count, exact `528K` descriptor bytes,
`8K <= P <= S + 8K`, caller-owned descriptor views and token staging, and
their aggregate workspace before entropy output.

After admission, parse the complete descriptor region and validate every rANS
model, state path, terminal state, and exact payload exhaustion without
producing output. Only when every block succeeds may a second pass reconstruct
exactly `S` private token bytes. Apply the existing LZSS validator to the
complete variable-length region and preserve its stable token index, byte
offset, format error, distance, match-length, overlap, and exact raw-extent
checks.

No raw staging or output span exists at this boundary. Prove the independent
592-byte Literal vector, a block boundary within that Literal, all
truncations, trailing input, short views and staging, aggregate admission one
byte short, malformed descriptor and later-block atomicity, invalid
reconstructed LZSS grammar, impossible dictionary and entropy extents, and
profile rejection. This decision adds no raw decoder, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-464: LZSS rANS reconstruction remains private

- Date: 2026-07-30
- Status: accepted

Extend DD-463 with a bounded complete-frame decoder that reconstructs only
into a distinct caller-owned private raw span. Require capacity for the full
declared `F` bytes before descriptor parsing or entropy output and add those
bytes to the descriptor, payload, token staging, and rANS-view aggregate
checked against `max_internal_buffered_bytes`.

Retain DD-463's all-block entropy preflight and complete LZSS validation. Only
after both succeed may the existing allocation-free LZSS decoder reconstruct
the validated Literal and forward-overlap Match sequence into exactly `F`
private bytes. Preserve stable LZSS format, token index, byte offset, and
decode error details. A decoder failure after successful validation remains a
distinct internal dictionary-decode category.

Prove the independent Literal frame, overlap copying, raw capacity admission
before token mutation, aggregate admission one byte short, and unchanged raw
staging for malformed entropy or dictionary layers. This decision adds no
caller-visible publication, encoder, streaming transform, profile calculator,
C ABI, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-465: LZSS rANS publication is one transactional copy

- Date: 2026-07-30
- Status: accepted

Add a caller-visible complete-frame decoder above DD-464. Require a distinct
output span with capacity for the entire declared raw frame before descriptor
parsing, entropy output, token staging mutation, or private raw mutation.
Caller output is publication storage rather than internal workspace and is
therefore not included in DD-464's aggregate buffered-byte total.

Retain DD-463's complete entropy and token validation and DD-464's private raw
reconstruction. Only after the LZSS decoder succeeds may the wrapper copy
exactly `F` bytes from private staging to output once. No incremental or
partial caller-visible publication is permitted.

Prove single-Literal publication, atomic overlap-Match publication, output
capacity rejection before all private mutation, and unchanged output for
malformed later entropy and invalid reconstructed LZSS grammar. This decision
adds no encoder, streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-466: LZSS rANS planning freezes variable tokens first

- Date: 2026-07-30
- Status: accepted

Add a write-free exact-frame planner as the first combined encoder-side
boundary. Use the standalone LZSS planner to determine exact token extent `S`,
admit caller-owned token staging, and then invoke the standalone LZSS encoder
once to freeze the complete canonical variable-length sequence. Reject empty
or unexpected raw frame extents and enforce `0 < S <= 2F` before rANS work.

Partition only that immutable token span into `K = ceil(S/B)` consecutive
blocks. Plan each scalar rANS block independently with one local descriptor,
sum exact payload sizes with checked arithmetic, enforce block-count and
32-bit serialized-field limits, and count exact descriptor, payload, and
token bytes against `max_internal_buffered_bytes`. Synthesize and validate the
generic frame header before returning exact complete size
`56 + 528K + P`. Accept no serialized output span.

Prove the independent 592-byte Literal extent, a block split inside the
Literal, deterministic planning of an encoder-generated overlap Match,
staging shortage before mutation, empty and unexpected input rejection,
block-count refusal, and aggregate workspace one byte short. This decision
adds no serialized frame encoder, streaming transform, profile calculator,
C ABI, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-467: LZSS rANS frame writing follows the complete plan

- Date: 2026-07-30
- Status: accepted

Add the deterministic complete-frame writer above DD-466. Invoke exact
planning first, including canonical LZSS staging, every rANS block plan,
aggregate workspace, and synthesized-header validation. Require serialized
output capacity for the complete planned extent before writing any byte.

After admission, explicitly serialize the generic header. Walk the immutable
token staging in the same block order, re-plan one descriptor at a time,
require every payload extent to fit and match the frozen aggregate, serialize
the descriptor into its exact 528-byte slot, and encode only that block's
assigned payload subspan. Require final token and payload offsets to equal the
plan. Any post-admission divergence is an internal error, not a new format.

Prove exact reproduction of the independent 592-byte frame, deterministic
round-trip when the Literal is split between two blocks, deterministic
round-trip of an encoder-generated Match split across four blocks, and
complete serialized-output preservation when capacity is one byte short.
This decision adds no streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-468: LZSS rANS streaming encode buffers one bounded frame

- Date: 2026-07-30
- Status: accepted

Add a known-size streaming encoder above DD-467. Emit the ordinary 64-byte
stream header and 16-byte LZSS parameter extension first. Collect at most one
configured raw frame in caller-owned storage, prepare its complete immutable
representation through the deterministic planner and writer, and drain that
frame under arbitrary output starvation before accepting the next frame.

At construction, validate the fixed pipeline, LZSS parameters, known original
size, raw storage for `min(original_size, frame_size)`, worst-case `2F` token
staging, and prefix serialization. At each frame preparation, count raw input,
exact token staging, and complete serialized frame storage together against
`max_internal_buffered_bytes`, and require sufficient encoded-frame storage
before invoking the writer.

`Flush` is non-terminal and does not close a partial frame. `EndInput` is
accepted only with exactly all remaining declared input. Reject `ResetBlock`
and unknown flags, over-input, premature end, and invalid workspace with stable
errors. Ended and error states are sticky and never report false progress.
Prove one-byte input/output equivalence to independently concatenated one-shot
frames, full-buffer and non-terminal-flush behavior, storage and aggregate
failures, empty input, premature end, unsupported reset, and repeated ended
calls. This decision adds no streaming decoder, profile calculator, C ABI,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-469: LZSS rANS streaming decode commits complete frames

- Date: 2026-07-30
- Status: accepted

Add the bounded known-size streaming decoder over DD-465's transactional frame
boundary. Incrementally collect the fixed 80-byte prefix and validate the
exact LZSS/rANS variants and parameter region. For each expected frame,
collect the 56-byte generic header first, validate sequence and committed raw
position, and use its declared extents to admit encoded-frame storage, rANS
views, token staging, raw staging, and their checked aggregate before
collecting the body.

Decode only after one exact frame is present. Invoke the private complete-frame
decoder into caller-owned raw staging, then enter an immutable drain state.
Only fully validated frames may contribute output; a malformed later frame
does not retract earlier committed frames but publishes no byte of its own.
Input and output starvation may occur at every prefix, header, body, and raw
drain byte.

Require exact known-size completion. Reject malformed prefix or parameters,
invalid frame headers or bodies, truncation, trailing bytes, premature end,
unsupported reset and unknown flags, and insufficient or excessive aggregate
workspace with stable errors. Empty streams contain only the prefix. Ended
and error states are sticky. Prove one-byte input/output round-trip, later-
frame corruption after one committed frame, each workspace shortage,
aggregate admission one byte short, strict truncation and trailing rejection,
reset refusal, empty input, non-terminal Flush, and premature end while
draining. This decision adds no profile calculator, C ABI, CLI, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-470: LZSS rANS profile owns all streaming workspace sizes

- Date: 2026-07-30
- Status: accepted

Add an internal immutable profile constructor for the DD-468/DD-469 streaming
pair. Accept known original size, raw frame size, entropy block size, and LZSS
variant-1 parameters; validate them with the caller's complete local limits;
and construct only the exact LZSS/rANS stream identity. Retain the specified
`F <= 2^20` composition cap and 65,536-byte default raw and entropy blocks.

For encoding, size the largest actual frame as
`min(original_size, frame_size)`, reserve conservative canonical LZSS staging
`S = 2F`, compute `K = ceil(S/B)`, `528K` descriptor bytes, `S + 8K`
worst-case payload bytes, and the complete generic-frame storage. Enforce
block-count, 32-bit frame-field, dictionary, payload, entropy-buffer, and
aggregate raw-plus-token-plus-frame limits before publishing requirements.
Empty known-size input publishes an all-zero workspace.

For decoding, derive one complete encoded-frame extent as the generic header
plus `max_internal_buffered_bytes`; derive raw staging from the lesser of the
local frame limit and composition cap; derive token staging from the lesser of
`2F` and the local dictionary limit; and expose rANS view count directly from
the local per-frame block limit. Clear all requirements on failure and map
stable profile errors to core errors.

Prove exact default and short-frame values, empty input, every governing
limit, invalid LZSS parameters, arithmetic overflow, stable error mapping, and
direct construction of a complete streaming round trip using only the
returned extents. This decision adds no C ABI, CLI, benchmark, fuzz target,
completion matrix, or interoperability entry.

## DD-471: LZSS rANS enters ABI v1 through three opaque regions

- Date: 2026-07-30
- Status: accepted

Add named C configuration initialization, direction-specific workspace
requirements, and an immutable transform factory without changing ABI version
1 or any existing public structure. Mirror DD-470's profile fields, including
entropy block size and per-frame block-count limit, while keeping
`RansBlockView` and every C++ type private.

For encoding, report raw-frame collection as primary, token staging plus
serialized-frame storage as secondary, and zero view bytes with alignment one.
For decoding, report encoded-frame storage as primary, token plus private raw
staging as secondary, and checked `block_view_count * sizeof(RansBlockView)`
opaque bytes with the internal alignment.

The factory must invoke the public requirements query, reject null, short, or
misaligned regions before construction, repeat the profile calculation,
partition secondary only at the checked token boundary, borrow all workspaces
for the transform lifetime, use `nothrow` allocation only for the opaque
handle, and leave that handle null on every failure. Prove the complete C11
lifecycle with raw `ABABABX`, exact queried regions, round trip, short views,
and reserved-field rejection. This decision adds no completion matrix, fuzz
target, CLI, benchmark, completion claim, or interoperability entry.

## DD-472: LZSS rANS completion is proven through the C ABI

- Date: 2026-07-30
- Status: accepted

Add a public-ABI completion matrix with fixed 64-byte raw frames and 64-byte
rANS blocks. Exercise only `marc_lzss_rans_config_init`, the requirements
query, factory, generic process function, and destroy lifecycle; allocate and
align all three regions exclusively from each direction's query.

Cover empty input, every one-byte symbol, all byte values, repetitive and
patterned binary input, deterministic generated input, and lengths immediately
below, equal to, and above the outer-frame boundary. Encode each class twice
and require byte identity before decoding.

For a 193-byte stream, require unchunked, one-byte, and mixed schedules to
produce identical multi-frame streams and raw output. Repeated successful
terminal calls remain EndOfStream. Independently corrupt the sequence field,
truncate the final payload, and append one trailing byte to the fourth frame;
each decoder may publish exactly the first three validated 64-byte frames,
must preserve the final sentinel, and must repeat the same sticky error and
position. This decision adds no fuzz target, CLI, benchmark, completion claim,
or interoperability entry.

## DD-473: LZSS rANS fuzzing is fixed-memory and dual-boundary

- Date: 2026-07-31
- Status: accepted

Add one libFuzzer entry point that submits every input, truncated to 8,192
bytes, to both the private complete-frame staging decoder and a streaming
decoder created through the public C ABI. Fix total output at 4,096 bytes, one
raw frame at 1,024 bytes, LZSS token staging at 2,048 bytes, payload at 8,192
bytes, and entropy metadata at eight `RansBlockView` records. No serialized
extent may allocate or resize workspace.

For complete-frame decode, parse only the exact LZSS/rANS prefix and parameters
before passing the remainder to the strict private decoder. For public
streaming, query the fixed configuration but require every reported byte count
and alignment to fit compile-time arrays before factory construction. Derive
bounded input and output chunks from bytes, cap calls at
`maximum_input + maximum_output + 32`, and abort on invalid counts, false
progress, exhausted input reported as NeedInput, or call-budget exhaustion.

Seed the corpus with truncated magic. Add permanent regressions requiring
atomic output and sticky errors for every truncation of a canonical stream,
saturated frame-length fields, and an invalid rANS descriptor. Build with
libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer and complete a
bounded smoke run. This decision adds no CLI, benchmark, completion claim, or
interoperability entry.

## DD-474: LZSS rANS CLI admission uses only the public profile

- Date: 2026-07-31
- Status: accepted

Add the explicit selector `lzss-rans` to the transactional file adapter. Fix
both raw frames and rANS blocks at 65,536 bytes. Derive `S = 131,072`
canonical token bytes, `K = 2` blocks, `P = 131,088` payload bytes, `528K =
1,056` descriptor bytes, and exact encoder aggregate
`F + S + 56 + 528K + P = 328,808` bytes.

Use a conservative 512-KiB `max_internal_buffered_bytes` policy for both
directions. This admits the exact encoder aggregate and the decoder's private
`RansBlockView` extent without naming, sizing, or aligning that C++ type in the
CLI. Configuration initialization, workspace requirements, transform
creation, processing, and destruction must use only `marc_lzss_rans_*` and
the common public C ABI.

Reuse the shared temporary-file transaction. Prove nonempty and empty round
trips, overwrite refusal, malformed-stream cleanup, and strict trailing-data
rejection under both supported Windows compilers. This step adds no benchmark
adapter, readiness claim, interoperability schema, or external evidence.

## DD-475: LZSS rANS benchmark verifies before measuring

- Date: 2026-07-31
- Status: accepted

Add `lzss-rans` to the dependency-free benchmark through only the public C
requirements, factory, process, and destroy lifecycle. Reuse DD-474's
65,536-byte raw frame and entropy block, 131,072-byte token ceiling, two-block
ceiling, 131,088-byte payload ceiling, and conservative 512-KiB aggregate
policy.

Reserve complete-stream output with checked arithmetic as
`80 + 2N + 1128K`, where `N` is total raw input and `K` is the nonempty frame
count. The per-frame term is one 56-byte generic header, two 528-byte
descriptors, and two eight-byte final states. The conservative bound permits a
short final frame to use fewer actual blocks without changing representation.

Query encoder and decoder workspaces independently, verify a byte-exact round
trip before timing, and construct each timed transform outside the elapsed
region. Report ratio, directional elapsed time and throughput, all six
workspace extents, and peak caller-reserved workspace. Performance values are
observations rather than thresholds. This step changes no format or C ABI and
adds no interoperability entry or external evidence.

## DD-476: Interoperability schema 21 appends LZSS rANS

- Date: 2026-07-31
- Status: accepted

Define interoperability schema 21 and codec set `marc-cli-v21` as the exact
thirty-one-entry schema-20 order followed by `lzss-rans`. Reuse the
deterministic 8,193-byte binary fixture and unchanged public CLI
representation. Each bundle must contain exactly thirty-two archives in
canonical order with recorded size and SHA-256 after local decode equality
succeeds.

The verifier must select an explicit expected profile array for every schema
1 through 21, reject unknown or duplicate profiles and every order change,
decode every foreign archive to the fixture, and re-encode every profile
byte-identically. The compatibility regression must reject a reordered
schema-21 manifest, then derive schema 20 by removing only the final
`lzss-rans` archive and changing only version and codec set before exercising
the unchanged schema-20-through-schema-1 chain.

This is local generation and verification evidence. Cross-platform canonical
bytes remain unproven until CI artifacts and an independent platform complete
the four established verification directions.

## DD-477: LZ78 rANS preserves fixed phrase tokens

- Date: 2026-07-31
- Status: accepted

Reserve `lz78-rans` for LZ78 variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZ78 parameter extension,
empty entropy parameters, and canonical fixed eight-byte tokens. Complete the
token stream before entropy processing; an rANS block may split a token but
cannot cross an outer frame. Reset the LZ78 phrase dictionary and every rANS
model and state at each frame.

For raw frame extent `F`, require aligned token extent `0 < S <= 8F`,
`K = ceil(S/B)` for nonzero rANS block size `B`,
`8K <= P <= S + 8K`, and exact descriptor extent `528K`. Bound token count by
`F` and generated phrase records by the Pair count, configured LZ78 entry
limit, and local decoder limit. Preserve the existing one-MiB LZ78 composition
frame cap.

Decoding must validate generic extents and every rANS descriptor, model, state
path, terminal state, and payload exhaustion before reconstructing exactly
`S` private token bytes. Only then validate eight-byte alignment, tags,
reserved fields, backward phrase references, FinalIndex placement, checked
phrase lengths, dictionary growth, and exact raw extent before any raw
reconstruction or publication.

For raw `A`, independently freeze LZ78 Pair token
`00 41 00 00 00 00 00 00`. Its normalized rANS model is
`00:3584, 41:512`, final-state payload is
`00 7C 9D 2F 0A 00 00 00`, and the complete frame is 592 bytes. Prove this by
composing only the existing standalone LZ78 encoder, scalar rANS encoder, and
generic serializers. This decision specifies bytes and a reserved name only;
it does not publish a combined validator, decoder, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-478: LZ78 rANS first validator stops at the phrase graph

- Date: 2026-07-31
- Status: accepted

Implement the first `lz78-rans` combined component as an internal bounded
complete-frame validator. Accept exactly one serialized frame and reject
truncation or trailing bytes. Before touching private token staging, verify
the generic header, `0 < S <= 8F`, eight-byte alignment,
`K = ceil(S/B)`, exact `528K` descriptor bytes,
`8K <= P <= S + 8K`, caller-owned rANS-view, token, and phrase capacities,
and the aggregate descriptor, payload, token, view, and phrase-workspace
limit.

Parse and validate every rANS descriptor, model, state path, terminal state,
and exact payload exhaustion before decoding any block. Only after all blocks
succeed may the validator reconstruct exactly `S` private token bytes and run
the existing bounded LZ78 validator over alignment, fields, backward
references, dictionary growth, phrase lengths, FinalIndex placement, and
exact declared raw extent. A later step must add iterative raw reconstruction
behind a separate private staging boundary. This decision adds no raw decoder,
encoder, streaming transform, C factory, CLI selector, benchmark, fuzz target,
or interoperability entry.

## DD-479: LZ78 rANS reconstructs only into private raw staging

- Date: 2026-07-31
- Status: accepted

Extend DD-478 with a bounded internal decoder that reconstructs one completely
validated frame into caller-owned private raw staging. Require capacity for
the exact declared raw extent and add those bytes to the aggregate descriptor,
payload, rANS-view, token, and phrase workspace total before parsing an rANS
descriptor or mutating token staging.

After every rANS block and the complete LZ78 phrase graph validate, invoke the
existing iterative LZ78 decoder over the exact private token and phrase
regions. Do not add a recursion stack or a second interpretation of phrase
links. Any entropy, token, phrase, capacity, or aggregate-limit failure leaves
raw staging untouched; all other workspaces remain discard-on-error. This
decision adds no caller-visible publication, encoder, streaming transform,
C factory, CLI selector, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-480: LZ78 rANS publication is one post-success copy

- Date: 2026-07-31
- Status: accepted

Extend DD-479 with an internal transactional complete-frame decoder. Require a
caller-visible output span at least as large as the declared raw frame and
check that capacity with all private capacities before parsing an rANS
descriptor or mutating any workspace. The output span is caller storage and
does not count toward the internal-buffer aggregate.

Run the unchanged DD-478 validation and DD-479 private reconstruction. Only
after both succeed, copy exactly the declared raw extent from private staging
to caller output once. Do not touch excess output capacity. Every header,
entropy, token, phrase, capacity, aggregate-limit, or reconstruction failure
must preserve the entire caller output. This decision adds no encoder,
streaming transform, C factory, CLI selector, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-481: LZ78 rANS encoding freezes tokens before block planning

- Date: 2026-07-31
- Status: accepted

Add a bounded exact-frame planner and encoder above DD-480. Complete
deterministic LZ78 parsing first using caller-owned encoder records, then
serialize the entire canonical fixed-width token sequence into separate
staging. Only that immutable byte sequence may be divided into scalar rANS
blocks; block boundaries remain independent of token boundaries.

The planner must determine every rANS payload extent, exact `528K` descriptor
bytes, and complete frame size without writing serialized output. Count LZ78
encoder records, token staging, descriptors, and payloads in one checked
internal-workspace total. Validate the synthesized generic header before
success. The encoder must complete this plan and check complete output capacity
before writing the header, descriptors, or payloads, then repeat the
deterministic block plans and require identical extents. Raw `A` must reproduce
the frozen 592-byte frame exactly. This decision adds no streaming transform,
C factory, CLI selector, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-482: LZ78 rANS streaming encode buffers one exact frame

- Date: 2026-07-31
- Status: accepted

Add a bounded known-size streaming encoder above DD-481. Emit the ordinary
64-byte stream header and 16-byte LZ78 parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete
serialized representation through the exact planner and encoder, and drain
that immutable frame under arbitrary output starvation before accepting bytes
for the next frame.

At construction, validate the fixed pipeline, parameters, known original
size, largest raw frame, conservative `8F` token staging, and LZ78 encoder
records. Before encoding each collected frame, count raw collection, exact
token staging, exact serialized frame, and encoder records in one checked
aggregate. Input and output chunking alone must not change serialized bytes.
`Flush` keeps a partial frame open; `EndInput` is retained while prefix or
frame bytes drain; `ResetBlock`, unknown flags, premature end, and excess input
are rejected. This decision adds no streaming decoder, C factory, CLI selector,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-483: LZ78 rANS streaming decode commits complete frames

- Date: 2026-07-31
- Status: accepted

Add the bounded known-size streaming decoder matching DD-482. Collect and
validate the fixed 80-byte prefix incrementally. For each nonempty frame,
collect the 56-byte generic header first, derive its exact checked serialized
extent, and admit encoded-frame storage, rANS views, canonical token staging,
LZ78 phrase records, private raw staging, and their combined internal byte
total before collecting the body.

Pass only a complete encoded frame to DD-479's private staging decoder. Drain
its raw result under arbitrary output starvation only after all entropy,
token-graph, and reconstruction checks succeed. A malformed later frame may
not publish any byte from that frame or retract earlier committed frames.
Retain `EndInput` while verified raw bytes drain; reject premature end,
trailing bytes, reset, unknown flags, invalid extents, and insufficient
workspace with sticky terminal behavior. Empty known-size input contains only
the prefix. This decision adds no profile calculator, C factory, CLI selector,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-484: LZ78 rANS profile fixes directional workspace layouts

- Date: 2026-07-31
- Status: accepted

Add an internal profile calculator above DD-482 and DD-483. For the actual
largest nonempty raw frame `F`, reserve conservative canonical token extent
`S=8F`, block count `K=ceil(S/B)`, exact `528K` descriptors, maximum `S+8K`
payload, and complete encoded-frame extent `56+528K+S+8K`. Bound LZ78 encoder
records by the lesser of `F` and the configured entry limit. Count raw, token,
complete frame, and encoder records against the aggregate policy.

For decode, derive complete-frame, token, private-raw, maximum rANS-view, and
maximum LZ78-phrase capacities only from local hard limits. Place rANS block
views first in one opaque region, align the phrase offset explicitly, and
place phrase records second. Recompute counts, offsets, total bytes, and
maximum alignment before returning typed spans; reject short or misaligned
storage and altered requirements. Empty encoding has zero byte regions and
alignment one. Prove that calculated regions directly construct the bounded
streaming round trip. This decision adds no C requirements query, public
factory, CLI selector, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-485: LZ78 rANS C factory exposes only opaque workspace bytes

- Date: 2026-07-31
- Status: accepted

Expose size-tagged `marc_lz78_rans_config`, initializer, workspace query, and
factory under ABI version 1. Retain the common three-region lifecycle. Encode
reports raw-frame collection as primary, token staging plus complete frame as
secondary, and aligned LZ78 encoder records as opaque views. Decode reports
encoded-frame collection as primary, token plus private raw staging as
secondary, and the checked rANS-view/padding/LZ78-phrase layout as opaque
views.

The query must map DD-484 errors stably. The factory first invokes that public
query, rejects null, short, or misaligned regions, repeats the profile
calculation, rederives the opaque layout, constructs only with `nothrow`, and
leaves the handle null on every failure. Direction remains immutable. Because
the common rANS validator applies `max_frame_size` to its own decoded byte
block, require local policy to admit the larger of the raw outer-frame extent
and configured entropy-block extent.

Prove the complete pure-C11 lifecycle with two-byte raw frames and five-byte
rANS blocks, queried workspaces, three-frame round trip, every one-byte-short
region, misalignment, null handle output, and reserved-field rejection. This
decision adds no completion matrix, fuzz target, CLI selector, benchmark,
completion claim, or interoperability entry.

## DD-486: LZ78 rANS completion is proved through the public ABI

- Date: 2026-07-31
- Status: accepted

Exercise the published `marc_lz78_rans_*` lifecycle rather than private C++
constructors. Use 64-byte raw frames, 64-byte entropy blocks, the conservative
`S=8F`, `K=ceil(S/B)`, exact `528K` descriptor, and `S+8K` payload bounds,
with all primary, secondary, and aligned opaque-view regions obtained only
from the requirements query.

Require deterministic round trips for empty input, all 256 one-byte values,
`00..FF`, repeated zeros, a four-symbol binary pattern, generated binary data,
and sizes immediately below, equal to, and above the frame boundary. For a
193-byte multi-frame stream, require byte-identical encoding and exact decode
under `(1,1)`, `(7,5)`, and `(13,17)` input/output chunk schedules. Repeated
calls after completion must remain `EndOfStream` with zero progress.

Locate the fourth frame through checked generic-frame extents. Independently
corrupt its sequence, truncate its final byte, and append trailing data. Each
decoder must publish exactly the first three verified frames, preserve the
sentinel for the failing final byte, and repeat the same terminal status and
error position without progress. This decision adds no fuzz target, CLI
selector, benchmark, or interoperability entry.

## DD-487: LZ78 rANS fuzzing fixes both decoder boundaries

- Date: 2026-07-31
- Status: accepted

Submit at most 8,192 arbitrary serialized bytes independently to the private
complete-frame staging decoder after a valid 80-byte profile prefix and to the
published C streaming decoder. Fix total output at 4,096 bytes, one raw frame
at 1,024 bytes, canonical token staging at 8,192 bytes, rANS payload at 16,384
bytes, entropy metadata at eight views, and LZ78 state at 1,024 phrase records.
Count encoded-frame, token, raw, views, and phrase extents into one conservative
local policy before parsing input.

The public path must obtain its exact three-region sizes and alignment from
`marc_lz78_rans_workspace_requirements()`, but may bind them only when they fit
compile-time fixed arrays. Derive partial input and output chunks from bounded
input bytes and impose a fixed call ceiling. Abort on invalid process results,
zero-progress protocol violations, input exhaustion reported as `NeedInput`,
or call-budget exhaustion; ordinary decoder rejection is a successful fuzz
case.

Persist repository-authored regressions for every proper truncation of the
canonical `ABABX` stream, saturated generic-frame extent fields, and a nonzero
rANS descriptor reserved byte. Each must publish no raw byte, preserve the
caller sentinel, and repeat its stable terminal error. This decision changes
no stream representation, public ABI, CLI, benchmark, or interoperability
entry.

## DD-488: LZ78 rANS CLI binds one fixed public profile

- Date: 2026-07-31
- Status: accepted

Add explicit selector `lz78-rans` to the transactional CLI and reach the codec
only through `marc_lz78_rans_config_init()`, its public requirements query,
factory, and generic transform lifecycle. Fix raw frames and entropy blocks at
65,536 bytes. The canonical LZ78 token ceiling is 524,288 bytes, producing at
most eight rANS blocks, 4,224 descriptor bytes, and a 524,352-byte payload.
Permit at most 65,536 generated phrase entries and use a conservative 4-MiB
aggregate internal-buffer policy.

The CLI may state these public format bounds but must not reproduce encoder,
rANS-view, or phrase-record layouts. Allocate primary, secondary, and aligned
opaque views only from the direction-specific public query. Retain known-size
input, immutable direction, overwrite refusal, temporary-output cleanup,
strict malformed and trailing-data rejection, and atomic destination rename.

Prove a multi-frame binary round trip, overwrite refusal, malformed-input
cleanup, later trailing-data cleanup, and empty input through the existing
generic CLI regression. This decision adds no benchmark adapter or
interoperability entry.

## DD-489: LZ78 rANS benchmark verifies before measuring

- Date: 2026-07-31
- Status: accepted

Add `lz78-rans` to the dependency-free benchmark runner using exactly DD-488's
fixed public profile. For raw input extent `N` and nonempty 65,536-byte frame
count `K`, reserve checked complete-stream capacity
`80 + 8N + 4344K`. The per-frame term consists of the 56-byte generic header,
eight 528-byte rANS descriptors, and eight eight-byte final states. Short
frames may use fewer bytes; the bound remains deterministic and conservative.

Initialize encoder and decoder configurations independently, query all three
workspace regions and opaque alignment through the public C ABI, encode once,
decode the exact encoded extent once, and require byte equality before timing.
Each timed sample constructs a fresh transform and invokes only the common
one-shot process boundary while the clock is active. Report complete-stream
ratio, directional throughput, every queried region, and the larger
directional workspace sum. Impose no performance threshold. This decision
adds no interoperability entry.

## DD-490: Interoperability schema 22 appends LZ78 rANS

- Date: 2026-07-31
- Status: accepted

Define interoperability schema 22 and codec set `marc-cli-v22` as the exact
frozen schema-21 profile order followed once by `lz78-rans`. The unchanged
transactional CLI profile becomes archive 33 over the existing deterministic
8,193-byte fixture. Do not add, remove, reorder, or reinterpret any earlier
schema entry.

The generator must locally decode every archive before publishing its size and
SHA-256. The verifier must require exactly thirty-three archives in canonical
order, validate leaf names, extents, and hashes, decode each foreign archive,
and require byte-identical local re-encoding. The compatibility test must
reject a reordered schema-22 manifest, convert schema 22 to the frozen schema
21 set by removing only `lz78-rans`, and continue verifying schemas 21 through
1 unchanged. Cross-platform interoperability remains unproven until external
artifacts produced at one complete revision pass the established four
directions.

## DD-491: LZW rANS preserves finalized packed codes

- Date: 2026-08-01
- Status: accepted

Reserve `lzw-rans` for LZW variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZW parameter extension,
empty entropy parameters, and canonical LSB-first variable-width codes.
Complete the packed byte stream, including final zero padding, before entropy
processing. An rANS block may split a packed code but cannot split a byte or
cross an outer frame. Reset the LZW dictionary and every rANS model and state
at each frame.

For raw frame extent `F` and maximum code width `W`, require packed extent
`0 < S <= ceil(FW/8)`, `K = ceil(S/B)` for nonzero rANS block size `B`,
`8K <= P <= S + 8K`, and exact descriptor extent `528K`. Bound generated
dictionary entries by `F - 1`, `2^W - 256`, the configured entry limit, and
the local decoder limit. Preserve the existing 2^20-byte LZW composition
frame cap.

Decoding must validate generic extents and every rANS descriptor, model, state
path, terminal state, and payload exhaustion before reconstructing exactly
`S` private packed bytes. Only then validate width transitions, the first
literal, ordinary and `KwKwK` references, checked phrase lengths, dictionary
growth, exact packed and raw extents, and zero high padding before any raw
reconstruction or publication.

For raw `A`, independently freeze LZW packed bytes `41 00`. Their normalized
rANS model is `00:2048, 41:2048`, final-state payload is
`00 08 00 00 02 00 00 00`, and the complete frame is 592 bytes. Prove this
by composing only the existing standalone LZW encoder, scalar rANS encoder,
and generic serializers. This decision specifies bytes and a reserved name
only; it does not publish a combined validator, decoder, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-492: LZW rANS validates all entropy before packed codes

- Date: 2026-08-01
- Status: accepted

Implement the first `lzw-rans` combined component as an internal bounded
complete-frame validator. Admit the exact serialized frame, one
`RansBlockView` per declared block, complete packed-byte staging, and the
conservative LZW phrase-record count before parsing any descriptor or
producing entropy output. Count descriptors, payload, packed staging, views,
and phrase records in one checked aggregate workspace bound.

Require the exact DD-491 packed ceiling, block count, descriptor extent, and
payload interval. Parse the full descriptor region, then validate every block
state path and exact payload exhaustion without output. If any block fails,
leave the entire packed staging region unchanged. Only after all blocks
validate may the component reconstruct all packed bytes in order and invoke
the existing LZW validator for width transitions, first literal, ordinary and
`KwKwK` references, dictionary growth, exact raw extent, trailing bits, and
zero high padding.

Report stable frame, block, and LZW error context. Reconstruct and publish no
raw bytes. Prove the independent 592-byte vector, a block boundary inside its
nine-bit code, every truncation, trailing data, short workspaces, aggregate
rejection before mutation, malformed later-block atomicity, invalid LZW
padding after successful entropy decode, impossible extents, and unsupported
pipeline rejection. This step adds no raw decoder, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-493: LZW rANS reconstructs only after complete validation

- Date: 2026-08-01
- Status: accepted

Add a bounded private-raw decoder above DD-492. Require complete raw staging
capacity and count that extent in the aggregate workspace before descriptor
parsing or entropy output. Reuse DD-492 unchanged to validate every rANS block,
reconstruct the complete packed LZW region, and validate the complete code
graph. Only then invoke the existing iterative LZW decoder into the admitted
raw staging span.

Publish no caller-visible bytes. On any error the caller discards all private
workspaces; insufficient raw capacity or aggregate memory must leave packed
and raw staging unchanged. Prove the independent raw-`A` frame, phrase and
`KwKwK` reconstruction across rANS block boundaries, preflight failures, and
invalid-code raw atomicity. This step adds no transactional public output,
encoder, streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-494: LZW rANS publication is one post-success copy

- Date: 2026-08-01
- Status: accepted

Add an internal transactional complete-frame decoder above DD-493. Require a
caller-visible output span at least as large as the declared raw frame and
check it with all private capacities before parsing a descriptor or mutating
any workspace. Caller output is destination storage and does not count toward
the internal-buffer aggregate.

Run the unchanged DD-492 validation and DD-493 private reconstruction. Only
after both succeed, copy exactly the declared raw extent from private staging
to output once, leaving excess output capacity untouched. Every frame,
entropy, LZW, capacity, aggregate, or reconstruction error must preserve all
caller output. This step adds no encoder, streaming transform, profile
calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-495: LZW rANS planning freezes packed codes before entropy

- Date: 2026-08-01
- Status: accepted

Add a bounded write-free exact-frame planner above DD-494. Complete
deterministic LZW parsing using caller-owned encoder records, admit the exact
packed extent, and serialize the full canonical LSB-first code stream including
final zero padding into separate staging. Only those immutable packed bytes may
be divided into scalar rANS blocks; block boundaries remain independent of
code boundaries.

Plan every rANS block without emitting descriptors or payloads, sum exact
payload and `528K` descriptor extents with checked arithmetic, and count
encoder records, packed staging, descriptors, and payload in one aggregate
workspace total. Validate the synthesized generic header and return the exact
complete-frame extent without accepting serialized output. Prove the frozen
592-byte raw-`A` extent, deterministic `ABABABA` codes across three blocks,
capacity atomicity, aggregate rejection, and raw-frame mismatch. This step
adds no frame encoder, streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-496: LZW rANS encoding emits only a completed plan

- Date: 2026-08-01
- Status: accepted

Add the bounded complete-frame encoder above DD-495. Run the exact planner to
completion and admit the full serialized destination before writing any frame
byte. Serialize the generic header explicitly, then repeat each deterministic
rANS block plan over the frozen packed LZW staging and require every payload
extent to match the previously summed plan.

Serialize each fixed descriptor and exact payload into its precomputed region,
then require final packed and payload offsets to match the plan. Raw `A` must
reproduce the independent 592-byte frame exactly. A multi-block `ABABABA`
frame must be byte-identical across runs and decode transactionally to the
source. A one-byte-short output must remain wholly unchanged. This step adds
no streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-497: LZW rANS streaming encode buffers one exact frame

- Date: 2026-08-01
- Status: accepted

Add the first bounded known-size streaming encoder above DD-496. Emit the
canonical 64-byte stream header and 16-byte LZW parameter extension, collect at
most one raw frame, plan and encode that complete frame into private immutable
storage, and drain it fully before accepting input for a later frame.

Construction validates the fixed variant-1 profile, declared original size,
largest raw frame, conservative `ceil(FW/8)` packed capacity, and required LZW
encoder records. Per-frame aggregate accounting includes raw staging, actual
packed staging, the exact serialized frame, and encoder records. Arbitrary
input/output chunking must preserve canonical bytes; `Flush` leaves a partial
frame open; retained `EndInput` drains all pending bytes; and `ResetBlock`,
unknown flags, premature end, or excess input fail stably. This step adds no
streaming decoder, profile calculator, C ABI, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-498: LZW rANS streaming decode publishes complete frames

- Date: 2026-08-01
- Status: accepted

Add the bounded known-size streaming decoder opposite DD-497. Collect and
parse the canonical 80-byte prefix, then collect one 56-byte frame header.
Before accepting its body, derive and validate the packed-code ceiling, rANS
block count and extents, exact serialized-frame size, raw staging, rANS views,
LZW phrase records, and aggregate internal storage.

Decode only after the entire declared frame is present. Reuse the private
complete-frame decoder, drain only the fully validated raw frame, and do not
collect a later header until draining finishes. Retain `EndInput` during the
drain and reject truncation, trailing bytes, `ResetBlock`, and unknown flags.
Corruption in a later frame may leave earlier committed frames visible but
must publish none of the malformed frame. This step adds no profile calculator,
C ABI, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-499: LZW rANS profiles separate byte and typed storage

- Date: 2026-08-01
- Status: accepted

Add an internal direction-specific workspace calculator above DD-497 and
DD-498. Encoding derives largest raw frame `F`, conservative packed staging
`S = ceil(FW/8)`, `K = ceil(S/B)` rANS blocks, the complete
`56 + 528K + S + 8K` frame ceiling, and the exact LZW encoder-record count.
Count raw, packed, complete-frame, and record bytes in one checked aggregate.

Decoding derives serialized-frame, packed, and private-raw byte regions from
validated local limits. Place the maximum rANS view count first in one aligned
opaque region, align upward, then place the conservative LZW phrase count.
Partition helpers must reject altered requirements, insufficient storage, and
misalignment. Empty encoding has zero regions and alignment one. Prove that the
returned requirements directly construct the bounded streaming round trip.
This step adds no C requirements query, public factory, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-500: LZW rANS C ABI retains three opaque workspaces

- Date: 2026-08-01
- Status: accepted

Expose the fixed LZW variant-1 plus scalar-rANS variant-1 profile through a
size-tagged `marc_lzw_rans_config`, a direction-specific workspace query, and
an immutable-direction factory. Include known original size, outer frame size,
entropy block size, maximum LZW width, block-count limit, and the existing hard
limits. Retain ABI version 1 because this is an additive symbol and type set.

Use the common three-workspace ABI. Primary is raw-frame or serialized-frame
storage. Secondary is packed staging followed by encoded-frame or private-raw
storage. Aligned opaque views contain encoder entries or the decoder's rANS
views, padding, and LZW phrases. The query and factory must both use DD-499,
reject wrong metadata, reserved fields, short regions, and misalignment, and
publish a null handle on every construction failure. Prove a pure-C five-byte,
three-frame round trip and all three short-region failures. This step adds no
completion matrix, fuzz target, CLI, benchmark, or interoperability entry.

## DD-501: LZW rANS completion evidence stays on the C ABI

- Date: 2026-08-01
- Status: accepted

Establish the initial public-ABI completion matrix using only DD-500's config,
requirements query, factory, process, and destroy lifecycle. Reuse the common
LZW evidence schedules while supplying the scalar-rANS payload and descriptor
ceiling plus 64-byte entropy-block configuration. This keeps input classes,
chunk schedules, terminal assertions, and malformed-frame publication checks
identical across admitted LZW compositions.

Cover empty input, all 256 one-byte values, all byte values in sequence,
repeated bytes, repeated multi-byte patterns, deterministic pseudo-random data,
and lengths 63, 64, and 65. Require byte-identical multi-frame encoding under
one-byte and mixed input/output chunking, exact decode, sticky ended state, and
sticky malformed state. Corrupt, truncate, and extend the fourth frame of a
193-byte stream; require exactly the first 192 bytes to remain committed and
the final byte untouched. This step adds no fuzz target, CLI, benchmark,
completion claim beyond the public-ABI matrix, or interoperability entry.

## DD-502: LZW rANS fuzzing crosses private and public boundaries

- Date: 2026-08-01
- Status: accepted

Add one bounded decoder fuzz target that presents each input to both the
complete-frame decoder-visible boundary and DD-500's public C streaming
decoder. Fix input at 8 KiB, total raw publication at 4 KiB, frame size at
1 KiB, packed LZW staging at 4 KiB, rANS views at eight, phrase records from
the packed-code ceiling, and all aggregate storage before accepting input.
Never allocate from a fuzz-controlled extent.

Drive the public decoder with deterministic variable chunks and a call budget
bounded by maximum input plus maximum output plus constant protocol overhead.
Abort on invalid process accounting, a zero-progress `Progress`, post-end
input starvation, workspace-calculation disagreement, or call-budget
exhaustion. Ordinary malformed input remains a successful fuzz iteration.
Retain permanent GoogleTest regressions for every truncation of a canonical
stream, saturated generic-frame extents, and nonzero rANS descriptor reserved
metadata; every case must publish zero bytes and retain one stable sticky
error. This step adds no CLI selector, benchmark, completion claim beyond the
fuzz boundary, or interoperability entry.

## DD-503: LZW rANS CLI binds one fixed public profile

- Date: 2026-08-01
- Status: accepted

Add explicit selector `lzw-rans` to the transactional CLI and reach the codec
only through `marc_lzw_rans_config_init()`, its public requirements query,
factory, and generic transform lifecycle. Fix raw frames and entropy blocks at
65,536 bytes and maximum code width at 16. The packed-code ceiling is 131,072
bytes, producing at most two rANS blocks, 1,056 descriptor bytes, and a
131,088-byte payload. Admit at most 65,280 generated dictionary entries and
use a conservative 8-MiB aggregate internal-buffer policy.

The CLI may supply these public bounds but must not reproduce LZW encoder,
phrase, or rANS-view layouts. Allocate primary, secondary, and aligned opaque
views only from the direction-specific public query. Retain known-size input,
immutable direction, overwrite refusal, sibling `.tmp` cleanup, strict
malformed and trailing-data rejection, and atomic destination rename.

Prove a multi-frame binary round trip, overwrite refusal, malformed-input
cleanup, trailing-data cleanup, and empty input through the common CLI
regression under both supported Windows compilers. This step adds no benchmark
adapter or interoperability entry.

## DD-504: LZW rANS benchmark verifies before timing

- Date: 2026-08-01
- Status: accepted

Add `lzw-rans` to the dependency-free benchmark runner using exactly DD-503's
public profile. For input extent `N` and nonempty frame count `K`, allocate
checked encoded capacity `80 + 2N + 1128K`: two packed LZW bytes per raw byte,
one 56-byte generic header, two 528-byte rANS descriptors, and two eight-byte
final states per frame.

Query encoder and decoder primary, secondary, and aligned opaque views
independently through the public C ABI. Encode once, decode the exact encoded
extent once, and require byte equality before timing. Each timed sample creates
a fresh transform, measures only one complete process call, and destroys the
transform after stopping the clock. Report complete-stream ratio, directional
throughput, every queried region, and the larger directional workspace sum.
Impose no performance threshold. This decision adds no interoperability entry.

## DD-505: Interoperability schema 23 appends LZW rANS

- Date: 2026-08-01
- Status: accepted

Define interoperability schema 23 and codec set `marc-cli-v23` as the exact
frozen schema-22 profile order followed once by `lzw-rans`. The unchanged
transactional CLI profile becomes archive 34 over the existing deterministic
8,193-byte fixture. Do not add, remove, reorder, or reinterpret an earlier
schema entry.

The generator must locally decode every archive before publishing its size and
SHA-256. The verifier must require exactly thirty-four archives in canonical
order, validate leaf names, extents, and hashes, decode each foreign archive,
and require byte-identical local re-encoding. The compatibility test must
reject a reordered schema-23 manifest, convert schema 23 to the frozen schema
22 set by removing only `lzw-rans`, and continue verifying schemas 22 through
1 unchanged. Cross-platform interoperability remains unproven until external
artifacts produced at one complete revision pass the established four
directions.

## DD-506: LZD rANS preserves finalized reference pairs

- Date: 2026-08-01
- Status: accepted

Reserve `lzd-rans` for LZD variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZD parameter extension,
empty entropy parameters, and canonical eight-byte little-endian reference
pairs. Complete the token byte stream before entropy processing. An rANS block
may split a four-byte reference or eight-byte token but cannot split a byte or
cross an outer frame. Reset the LZD dictionary and every rANS model and state
at each frame.

For nonempty raw frame extent `F`, require actual token extent
`0 < S <= 8 * ceil(F/2)` with `S mod 8 = 0`, `K = ceil(S/B)` for nonzero rANS
block size `B`, `8K <= P <= S + 8K`, and exact descriptor extent `528K`.
Bound generated phrase records by the lesser of `floor(F/2)` and the configured
entry limit, expansion references by that phrase count plus one, and raw frames
by 2^20 bytes.

Decoding must validate generic extents and every rANS descriptor, model, state
path, terminal state, and payload exhaustion before reconstructing exactly `S`
private token bytes. Only then validate eight-byte alignment, left and right
references, terminal absence, checked phrase lengths, dictionary growth, and
exact raw extent before any raw reconstruction or publication.

For raw `A`, independently freeze LZD token bytes
`41 00 00 00 FF FF FF FF`. Their normalized rANS model is
`00:1536, 41:512, FF:2048`, payload is
`82 27 A1 BD 04 00 00 00 00`, and the complete frame is 593 bytes. Prove this
by composing only the existing standalone LZD encoder, scalar rANS encoder,
and generic serializers. This decision specifies bytes and a reserved name
only; it does not publish a combined validator, decoder, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-507: LZD rANS validates all entropy before the phrase graph

- Date: 2026-08-01
- Status: accepted

Add the first complete-frame validator for DD-506 without reconstructing or
publishing raw bytes. Admit the complete serialized extent, exact rANS view
count, full token staging, bounded LZD phrase records, and their checked
aggregate workspace before parsing descriptors or producing entropy output.
Require DD-506's exact token ceiling and alignment, block count, descriptor
extent, payload interval, pipeline, parameter, sequence, and frame bounds.

Parse the complete descriptor region, then validate every block state path,
terminal state, and exact payload exhaustion without output. If any block
fails, leave the entire token staging region unchanged. Only after all blocks
validate may the implementation decode each block into its predetermined
private token slice. Require the slices to fill exactly the declared token
extent, then invoke the ordinary LZD validator over the complete span.

Report stable outer, controller, entropy, and LZD validation categories plus
the failing block, token, and byte positions where available. Prove the
independent 593-byte vector, blocks that split references and tokens, later-
descriptor rejection before staging mutation, valid entropy carrying an
invalid forward LZD reference, short typed and byte workspaces, truncation,
trailing bytes, and unsupported pipelines. This step adds no raw decoder,
encoder, streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-508: LZD rANS reconstructs raw bytes only in private staging

- Date: 2026-08-01
- Status: accepted

Add private raw reconstruction above DD-507 without a caller-visible output
boundary. Before descriptor parsing or entropy output, require the complete
declared raw staging extent and the ordinary LZD iterative expansion extent of
`phrase_count + 1` references. Count raw bytes and expansion records together
with descriptors, payload, token staging, rANS views, and phrase records in
one checked aggregate workspace bound.

After DD-507 validates every entropy block and the complete LZD phrase graph,
invoke the existing nonrecursive LZD decoder over exactly the validated token
and phrase regions. Reconstruct exactly the declared raw extent into separate
private staging. On any error, callers must discard token, phrase, expansion,
and raw workspaces; this step promises no transaction over an external output
span.

Prove the independent raw-`A` frame and a block-size-five `ABABAB` case whose
four rANS blocks split references while LZD expands generated phrase 256.
Reject raw and expansion workspaces one entry short before token mutation, and
retain private raw bytes on malformed entropy. This step adds no public
transactional decoder, encoder, streaming transform, profile calculator, C
ABI, CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-509: LZD rANS publishes one complete frame transactionally

- Date: 2026-08-01
- Status: accepted

Add a caller-visible complete-frame decoder above DD-508. Before descriptor
parsing, entropy output, token mutation, or private raw mutation, require output
capacity for the complete declared raw extent. Output is not internal workspace
and must not be included in the aggregate-buffer limit.

Retain DD-507's complete entropy and phrase-graph validation and DD-508's
private iterative reconstruction unchanged. Only after both succeed, copy
exactly the declared raw extent once from private staging to caller output.
Leave excess output capacity untouched. Any capacity, header, entropy,
dictionary, workspace, or reconstruction failure must preserve the entire
caller output.

Prove one-copy raw-`A` publication with excess-capacity guards, generated-phrase
`ABABAB` publication, output capacity one byte short before private mutation,
and complete output preservation for malformed later entropy and valid entropy
carrying an invalid LZD forward reference. This step adds no encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-510: LZD rANS planning freezes canonical token bytes

- Date: 2026-08-01
- Status: accepted

Add a bounded write-free exact-frame planner as the inverse of DD-507 through
DD-509. Validate the exact stream profile, LZD parameters, nonempty input, and
frame-local limits. Determine and require the LZD encoder-record count before
token staging can change. Plan the deterministic LZD parse, require its exact
nonzero eight-byte-aligned extent within `S <= 8 * ceil(F/2)`, and serialize
the complete canonical token sequence into caller-owned staging.

Divide only that frozen token span into `K = ceil(S/B)` rANS blocks. Plan each
block independently, accumulate exact payload bytes, require exact descriptor
extent `528K`, and retain `8K <= P <= S + 8K` and all 32-bit frame-field
bounds. Count encoder records, token staging, all descriptors, and exact
payload bytes against `max_internal_buffered_bytes`. Validate the synthesized
generic frame header with sequence and already-committed-output context, then
report the checked complete serialized extent without accepting or mutating a
serialized output span.

Prove the raw-`A` token bytes, one block, 528 descriptor bytes, nine payload
bytes, and 593-byte extent. Repeat a phrase-generating multi-block plan byte
identically. Reject encoder records and token staging one entry short before
token mutation, and reject aggregate workspace one byte short, empty input,
and a frame-size mismatch. This step adds no frame encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-511: LZD rANS encoding is plan-first and deterministic

- Date: 2026-08-01
- Status: accepted

Add the deterministic complete-frame encoder above DD-510. Invoke the exact
planner first so canonical LZD token bytes, exact rANS block count, descriptor
extent, payload extent, generic frame fields, and aggregate workspace are fixed
before serialized output is considered. Require destination capacity for the
complete planned extent before writing any serialized byte.

Serialize the generic frame header explicitly. Replan each rANS block over the
unchanged token staging, require every payload extent and final aggregate
offset to match DD-510, serialize every 528-byte descriptor into the complete
descriptor region, and encode each payload into its exact planned region. The
raw-`A` input must reproduce the independent 593-byte frame exactly.

Preserve every existing combined error value and append a distinct serialized-
output-capacity error. Every planner failure and short destination must leave
the whole serialized destination unchanged. Prove deterministic byte identity
and transactional decode for a phrase-generating frame split across rANS block
boundaries. This step adds no streaming transform, profile calculator, C ABI,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-512: LZD rANS streaming encoding retains complete exact frames

- Date: 2026-08-01
- Status: accepted

Wrap DD-510 and DD-511 in a bounded known-size streaming encoder without
defining another byte representation. Serialize the ordinary 80-byte stream
prefix once. Collect at most one declared raw frame in caller-owned storage,
invoke the exact planner and encoder only when that full or final-short frame
is complete, then retain the complete immutable serialized frame while
draining it to arbitrary caller output chunks. Accept no new raw frame bytes
while a serialized frame is pending.

For the largest possible local frame `L = min(original_size, frame_size)`,
require raw capacity `L`, checked token capacity `8 * ceil(L/2)`, and
`lzd_encoder_workspace_entries(L)` records at construction. At frame
preparation, require complete serialized-frame capacity and count raw input,
exact token staging, exact serialized frame, and active encoder records against
`max_internal_buffered_bytes`. Preserve DD-510's inner aggregate check.

Known-size `EndInput` is valid only when the supplied call completes the exact
declared original extent and remains effective while prefix or frame bytes
drain. A full frame may be prepared before whole-stream `EndInput`; after the
declared extent is committed, await the terminal flag if it has not arrived.
Nonterminal `Flush` does not close a short frame or alter bytes. Reject
`ResetBlock`, unknown flags, premature `EndInput`, and excess input with stable
terminal errors. Repeated calls after success return `EndOfStream`.

Prove equality with concatenated one-shot frames under one-byte input and
output, unchanged bytes under `Flush`, sticky `EndInput` across every drain,
empty known-size input, workspace and aggregate failures, and protocol errors.
This step adds no streaming decoder, profile calculator, C ABI, CLI, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-513: LZD rANS streaming decoding admits complete frames before raw drain

- Date: 2026-08-01
- Status: accepted

Add the matching bounded known-size streaming decoder without changing DD-506
bytes. Collect and parse the fixed 80-byte stream prefix first. For each frame,
collect the 56-byte generic header separately and validate sequence, committed
raw offset, exact LZD token alignment and ceiling, rANS block count,
`528K` descriptor extent, `8K <= P <= S + 8K`, every caller-owned capacity,
the checked complete serialized extent, and simultaneous aggregate workspace
before copying that header into frame storage or accepting body bytes.

After collecting exactly one admitted complete frame, invoke DD-508's private
decoder through all rANS block validation, token reconstruction, LZD graph
validation, and iterative raw reconstruction. Retain the complete raw frame in
private caller-owned storage and drain it to arbitrary output chunks only after
success. Count encoded frame, rANS views, token staging, raw staging, aligned
phrase records, and expansion references together. A malformed later frame may
not publish any byte from that frame; earlier completely drained frames remain
committed.

Preserve `EndInput` while a validated raw frame drains. Reject truncation at
prefix, frame header, or body; trailing bytes after the declared original
extent; malformed descriptors, payload, or LZD graph; `ResetBlock`; and unknown
flags. Nonterminal `Flush` only exposes current starvation. Repeated calls after
success return `EndOfStream`, and errors remain sticky with a stable serialized
byte position.

Prove one-byte input/output, atomic later-frame corruption, every typed and byte
workspace one entry short, aggregate storage one byte short, canonical
truncation and trailing data, empty input, flush starvation, premature end, and
protocol errors. This step adds no profile calculator, C ABI, CLI, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-514: LZD rANS profiles align three decoder record regions

- Date: 2026-08-01
- Status: accepted

Add an internal direction-specific workspace calculator above DD-512 and
DD-513 without changing their byte representation. For encoding, derive the
largest raw frame `F`, conservative token staging `S = 8 * ceil(F/2)`, rANS
block count `K = ceil(S/B)`, complete frame ceiling
`56 + 528K + S + 8K`, and bounded LZD encoder records. Count all four regions
under checked aggregate arithmetic.

For decoding, derive complete encoded, token, and private-raw byte regions from
validated local limits. Place rANS block views first in opaque storage, align
and place LZD phrase records second, then align and place the iterative
`uint32_t` expansion references. Partition helpers must recompute and validate
both offsets, total bytes, alignment, capacity, and base alignment before
publishing typed spans. Empty encoding uses zero regions and alignment one.
Prove that the returned requirements directly construct the existing bounded
streaming round trip. This step adds no public C requirements query, factory,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-515: LZD rANS public C factory preserves opaque record layout

- Date: 2026-08-01
- Status: accepted

Expose DD-514 through a size-tagged fixed-width `marc_lzd_rans_config`, a
direction-specific workspace query, and an immutable-direction transform
factory. The config carries known original size, raw frame size, entropy block
size, LZD entry ceiling, rANS block ceiling, and the common hard limits.
Reserved fields must remain zero.

Keep three caller-owned regions. Encoding maps primary storage to one raw frame,
secondary storage to canonical LZD token staging followed by one complete rANS
frame, and aligned opaque views to LZD encoder entries. Decoding maps primary
storage to one complete encoded frame, secondary storage to token staging
followed by private raw staging, and aligned opaque views to DD-514's rANS
block, LZD phrase, and iterative expansion spans. The requirements query is the
only allocation authority; the factory repeats calculation and checked
partitioning before construction. Prove the pure-C lifecycle, short-region and
misalignment rejection, null-output rejection, reserved-field rejection, and
binary round trip. This publishes no CLI, completion, fuzz, benchmark, or
interoperability claim.

## DD-516: LZD rANS completion reuses one public-ABI schedule

- Date: 2026-08-01
- Status: accepted

Apply the established LZD public-ABI completion matrix to DD-515 with 64-byte
outer frames, 64-byte rANS blocks, at most four entropy blocks per frame, and
maximum token extent `S = 8 * ceil(64/2) = 256`. Bound each rANS payload by
`S + 8K` and the complete frame by `56 + 528K + S + 8K`.

Keep the data classes, deterministic repeated encode, one-byte and mixed chunk
schedules, repeated terminal calls, and malformed fourth-frame schedule
identical to the already reviewed LZD matrix. Add only representation-neutral
test hooks for alternate payload/frame capacity and profile configuration; the
default Adaptive and Dynamic Range instantiations must remain unchanged. The
matrix must use only public C configuration, requirements, factory, process,
and destroy functions. A corrupt, truncated, or extended final frame may
publish the first three complete 64-byte frames but must leave the final raw
byte untouched and return a stable repeated error. This step adds no fuzz,
CLI, benchmark, interoperability entry, or `Ready` claim.

## DD-517: LZD rANS fuzzing fixes both decoder allocation boundaries

- Date: 2026-08-01
- Status: accepted

Add one bounded fuzz entry that presents each input first to the private
complete-frame decoder after strict prefix and parameter admission, then to the
public C streaming decoder created only through DD-515. Cap consumed fuzz bytes
at 8,192, total raw output at 4,096, one raw frame at 1,024, token staging at
4,096, compressed payload at 16,384, dictionary records at 512 phrases plus
513 iterative expansion references, and rANS views at eight blocks. Derive
fixed byte arrays and aligned opaque storage conservatively from those bounds;
abort if the public requirements query exceeds them.

Choose input and output chunks from bounded bytes, but cap total process calls
independently at input cap plus output cap plus 32. Abort on invalid accounting,
zero-progress `Progress`, impossible starvation after all input, or exhaustion
of the finite call budget. Treat an ordinary decoder error, successful end, or
full bounded output as a valid fuzz outcome.

Freeze permanent regressions for every strict prefix of one canonical `ABABX`
stream, saturated generic frame extents, and a nonzero rANS descriptor reserved
byte. Each must publish zero bytes from its only frame, preserve raw sentinels,
and return the same error code and byte position on repetition. This step adds
no CLI, benchmark, interoperability entry, or `Ready` claim.

## DD-518: LZD rANS CLI delegates all storage to the public ABI

- Date: 2026-08-02
- Status: accepted

Publish `lzd-rans` as an explicit selector in the existing transactional CLI.
Use 65,536-byte outer frames and 65,536-byte rANS blocks. The exact LZD bound is
262,144 canonical token bytes, which permits four entropy blocks, 2,112
descriptor bytes, and at most 262,176 payload bytes. Retain the public LZD
maximum-entry default and a conservative 16-MiB aggregate internal-buffer
policy.

Initialize, query, create, process, and destroy the codec only through
`marc_lzd_rans_*` and the common transform lifecycle. Allocate the three
direction-specific workspace regions from the returned requirements, including
the returned opaque alignment; do not reproduce any rANS-view, encoder-entry,
phrase, expansion-stack, or partition layout in the CLI. Reuse the existing
temporary-file transaction so configuration, allocation, codec, truncation,
trailing-data, and destination-collision failures publish no requested output
and leave no sibling temporary file. This step adds no benchmark,
interoperability entry, stream field, format variant, or `Ready` claim.

## DD-519: LZD rANS benchmark retains the half-pair ceiling

- Date: 2026-08-02
- Status: accepted

Add `lzd-rans` to the dependency-free benchmark runner using exactly DD-518's
65,536-byte frame and entropy-block profile and only the published C lifecycle.
Before timing, encode once, decode the exact encoded extent once, and require
byte equality. Then report complete-stream ratio, encode and decode throughput,
all three direction-specific workspace extents, and the larger caller-owned
workspace total.

For input extent `N` and nonempty outer-frame count `K`, reserve checked encoded
capacity `80 + 8 * ceil(N / 2) + 2200K`. The token term deliberately retains
the possible final absent-right reference; do not reduce it to `4N`, which is
one four-byte half-reference too small for odd `N`. The per-frame term reserves
the 56-byte generic header plus four complete 528-byte descriptors and four
eight-byte states. Short final frames may use less. This step adds no stream
field, format variant, interoperability entry, optimization, or representative
performance claim.

## DD-520: Interoperability schema 24 appends LZD rANS once

- Date: 2026-08-02
- Status: accepted

Define interoperability schema 24 as the frozen schema-23 profile order followed
once by `lzd-rans`. Set `schema_version` to 24 and `codec_set` to
`marc-cli-v24`; require exactly 35 archives in manifest order. Keep schemas 1
through 23 explicit and unchanged. No older schema may inherit the new profile.

Generation must round-trip every archive locally before recording its size and
SHA-256. Verification must require leaf-only names, exact profile count and
order, recorded sizes and hashes, foreign decode equality, and byte-identical
local re-encoding. The local compatibility regression must reject a reordered
schema-24 manifest, derive schema 23 by removing only `lzd-rans` and restoring
its version and codec set, then continue through every frozen older schema.
This establishes local format determinism only; external Windows/Linux
cross-checks remain required release evidence and no result is predicted.

## DD-521: LZMW rANS preserves finalized phrase references

- Date: 2026-08-02
- Status: accepted

Reserve `lzmw-rans` for LZMW variant 1 followed by scalar rANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZMW parameter extension,
empty entropy parameters, and canonical four-byte little-endian phrase
references. Complete the reference byte stream before entropy processing. An
rANS block may split a reference but cannot split a byte or cross an outer
frame. Reset the LZMW dictionary and every rANS model and state at each frame.

For nonempty raw frame extent `F`, require actual reference extent
`0 < S <= 4F` with `S mod 4 = 0`, `K = ceil(S/B)` for nonzero rANS block size
`B`, `8K <= P <= S + 8K`, and exact descriptor extent `528K`. Bound generated
phrase records by the lesser of `max(F - 1, 0)` and the configured entry limit,
expansion references by that phrase count plus one, and raw frames by 2^20
bytes.

Decoding must validate generic extents and every rANS descriptor, model, state
path, terminal state, and payload exhaustion before reconstructing exactly `S`
private reference bytes. Only then validate four-byte alignment, literal or
previously generated references, checked adjacent-phrase growth, and exact raw
extent before any raw reconstruction or publication.

For raw `A`, independently freeze LZMW reference bytes `41 00 00 00`. Their
normalized rANS model is `00:3072, 41:1024`, payload is
`00 1C A1 BD 04 00 00 00`, and the complete frame is 592 bytes. Prove this by
composing only the existing standalone LZMW encoder, scalar rANS encoder, and
generic serializers. This decision specifies bytes and a reserved name only;
it does not publish a combined validator, decoder, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-522: LZMW rANS validation stops at the phrase graph

- Date: 2026-08-02
- Status: accepted

Add the first bounded complete-frame validator for DD-521 without reconstructing
or publishing raw bytes. Before descriptor parsing, validate the exact generic
frame extent, `0 < S <= 4F`, four-byte alignment, `K = ceil(S/B)`, exact
`528K` descriptor bytes, `8K <= P <= S + 8K`, caller capacities for rANS block
views, `S` reference staging, and bounded LZMW phrase records, and the aggregate
workspace limit.

Parse every descriptor and validate every rANS payload, including terminal
state and exact byte exhaustion, before writing reference staging. Decode all
blocks only after that pass succeeds, require exactly `S` reconstructed bytes,
then invoke the ordinary LZMW validator for four-byte alignment, literal or
previously generated references, adjacent-phrase growth, configured dictionary
freeze, and exact declared raw extent.

Report stable generic, controller, entropy, and LZMW validation categories plus
the failing block, token, and input-byte positions where available. Prove the
independent 592-byte vector, blocks that split references, later-descriptor
rejection before staging mutation, valid entropy carrying an invalid forward
LZMW reference, short typed and byte workspaces, truncation, trailing bytes,
and unsupported pipelines. This step adds no raw decoder, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-523: LZMW rANS reconstructs raw bytes only in private staging

- Date: 2026-08-02
- Status: accepted

Add private raw reconstruction above DD-522 without a caller-visible output
boundary. Before descriptor parsing or entropy output, require the complete
declared raw staging extent and the conservative ordinary LZMW iterative
expansion extent derived from the maximum admitted phrase-record count plus one
reference. Count raw bytes and expansion records together with descriptors,
payload, reference staging, rANS views, and phrase records in one checked
aggregate workspace bound.

After DD-522 validates every entropy block and the complete LZMW phrase graph,
reduce the active expansion span to the actual generated-entry count plus one
reference and invoke the existing nonrecursive LZMW decoder over exactly the
validated reference and phrase regions. Reconstruct exactly the declared raw
extent into separate private staging. On any error, callers must discard
reference, phrase, expansion, and raw workspaces; this step promises no
transaction over an external output span.

Prove the independent raw-`A` frame and a block-size-five `ABABAB` case whose
four rANS blocks split references while LZMW expands generated phrase 256.
Reject raw and conservative expansion workspaces one entry short before
reference mutation, and retain private raw bytes on malformed entropy. This
step adds no public transactional decoder, encoder, streaming transform,
profile calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-524: LZMW rANS publishes one complete frame transactionally

- Date: 2026-08-02
- Status: accepted

Add a caller-visible complete-frame decoder above DD-523. Before descriptor
parsing, entropy output, reference mutation, or private raw mutation, require
output capacity for the complete declared raw extent. Output is not internal
workspace and must not be included in the aggregate-buffer limit.

Retain DD-522's complete entropy and phrase-graph validation and DD-523's
private iterative reconstruction unchanged. Only after both succeed, copy
exactly the declared raw extent once from private staging to caller output.
Leave excess output capacity untouched. Any capacity, header, entropy,
dictionary, workspace, or reconstruction failure must preserve the entire
caller output.

Prove one-copy raw-`A` publication with excess-capacity guards, generated-phrase
`ABABAB` publication, output capacity one byte short before private mutation,
and complete output preservation for malformed later entropy and valid entropy
carrying an invalid LZMW forward reference. This step adds no encoder,
streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-525: LZMW rANS planning freezes canonical reference bytes

- Date: 2026-08-02
- Status: accepted

Add a bounded write-free exact-frame planner as the inverse of DD-522 through
DD-524. Validate the exact stream profile, LZMW parameters, nonempty input, and
frame-local limits. Determine and require the LZMW encoder-record count before
reference staging can change. Plan the deterministic LZMW parse, require its
exact nonzero four-byte-aligned extent within `S <= 4F`, and serialize the
complete canonical reference sequence into caller-owned staging.

Divide only that frozen reference span into `K = ceil(S/B)` rANS blocks. Plan
each block independently, accumulate exact payload bytes, require exact
descriptor extent `528K`, and retain `8K <= P <= S + 8K` and all 32-bit frame-
field bounds. Count encoder records, reference staging, all descriptors, and
exact payload bytes against `max_internal_buffered_bytes`. Validate the
synthesized generic frame header with sequence and already-committed-output
context, then report the checked complete serialized extent without accepting
or mutating a serialized output span.

Prove the raw-`A` reference bytes, one block, 528 descriptor bytes, eight
payload bytes, and 592-byte extent. Repeat a phrase-generating multi-block plan
byte identically. Reject encoder records and reference staging one unit short
before reference mutation, and reject aggregate workspace one byte short,
empty input, and a frame-size mismatch. This step adds no frame encoder,
streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-526: LZMW rANS encoding is plan-first and deterministic

- Date: 2026-08-02
- Status: accepted

Add the deterministic complete-frame encoder above DD-525. Invoke the exact
planner first so canonical LZMW reference bytes, exact rANS block count,
descriptor extent, payload extent, generic frame fields, and aggregate
workspace are fixed before serialized output is considered. Require destination
capacity for the complete planned extent before writing any serialized byte.

Serialize the generic frame header explicitly. Replan each rANS block over the
unchanged reference staging, require every payload extent and final aggregate
offset to match DD-525, serialize every 528-byte descriptor into the complete
descriptor region, and encode each payload into its exact planned region. The
raw-`A` input must reproduce the independent 592-byte frame exactly.

Preserve every existing combined error value and append a distinct serialized-
output-capacity error. Every planner failure and short destination must leave
the whole serialized destination unchanged. Prove deterministic byte identity
and transactional decode for a phrase-generating frame split across rANS block
boundaries. This step adds no streaming transform, profile calculator, C ABI,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-527: LZMW rANS streaming encoding retains complete exact frames

- Date: 2026-08-02
- Status: accepted

Wrap DD-525 and DD-526 in a bounded known-size streaming encoder without
defining another byte representation. Serialize the ordinary 80-byte stream
prefix once. Collect at most one declared raw frame in caller-owned storage,
invoke the exact planner and encoder only when that full or final-short frame
is complete, then retain the complete immutable serialized frame while
draining it to arbitrary caller output chunks. Accept no new raw frame bytes
while a serialized frame is pending.

For the largest possible local frame `L = min(original_size, frame_size)`,
require raw capacity `L`, checked reference capacity `4L`, and
`lzmw_encoder_workspace_entries(L)` records at construction. At frame
preparation, require complete serialized-frame capacity and count raw input,
exact reference staging, exact serialized frame, and active encoder records
against `max_internal_buffered_bytes`. Preserve DD-525's inner aggregate check.

Known-size `EndInput` is valid only when the supplied call completes the exact
declared original extent and remains effective while prefix or frame bytes
drain. A full frame may be prepared before whole-stream `EndInput`; after the
declared extent is committed, await the terminal flag if it has not arrived.
Nonterminal `Flush` does not close a short frame or alter bytes. Reject
`ResetBlock`, unknown flags, premature `EndInput`, and excess input with stable
terminal errors. Repeated calls after success return `EndOfStream`.

Prove equality with concatenated one-shot frames under one-byte input and
output, unchanged bytes under `Flush`, sticky `EndInput` across every drain,
empty known-size input, workspace and aggregate failures, and protocol errors.
This step adds no streaming decoder, profile calculator, C ABI, CLI, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-528: LZMW rANS streaming decoding admits complete frames before raw drain

- Date: 2026-08-02
- Status: accepted

Add the matching bounded known-size streaming decoder without changing DD-521
bytes. Collect and parse the fixed 80-byte stream prefix first. For each frame,
collect the 56-byte generic header separately and validate sequence, committed
raw offset, exact LZMW four-byte reference alignment and `S <= 4F` ceiling,
rANS block count, `528K` descriptor extent, `8K <= P <= S + 8K`, every caller-
owned capacity, the checked complete serialized extent, and simultaneous
aggregate workspace before copying that header into frame storage or accepting
body bytes.

After collecting exactly one admitted complete frame, invoke DD-523's private
decoder through all rANS block validation, reference reconstruction, LZMW graph
validation, and iterative raw reconstruction. Retain the complete raw frame in
private caller-owned storage and drain it to arbitrary output chunks only after
success. Count encoded frame, rANS views, reference staging, raw staging,
phrase records, and expansion references together. A malformed later frame may
not publish any byte from that frame; earlier completely drained frames remain
committed.

Preserve `EndInput` while a validated raw frame drains. Reject truncation at
prefix, frame header, or body; trailing bytes after the declared original
extent; malformed descriptors, payload, or LZMW graph; `ResetBlock`; and
unknown flags. Nonterminal `Flush` only exposes current starvation. Repeated
calls after success return `EndOfStream`, and errors remain sticky with a stable
serialized byte position.

Prove one-byte input/output, atomic later-frame corruption, every typed and byte
workspace one entry short, aggregate storage one byte short, canonical
truncation and trailing data, empty input, flush starvation, premature end, and
protocol errors. This step adds no profile calculator, C ABI, CLI, benchmark,
fuzz target, completion claim, or interoperability entry.

## DD-529: LZMW rANS profiles align three decoder record regions

- Date: 2026-08-02
- Status: accepted

Add an internal direction-specific workspace calculator above DD-527 and
DD-528 without changing their byte representation. For encoding, derive the
largest raw frame `F`, conservative reference staging `S = 4F`, rANS block
count `K = ceil(S/B)`, complete frame ceiling `56 + 528K + S + 8K`, and bounded
LZMW encoder records. Count all four regions under checked aggregate arithmetic.

For decoding, derive complete encoded, reference, and private-raw byte regions
from validated local limits. Place rANS block views first in opaque storage,
align and place LZMW phrase records second, then align and place the iterative
`uint32_t` expansion references. Partition helpers must recompute and validate
both offsets, total bytes, alignment, capacity, and base alignment before
publishing typed spans. Empty encoding uses zero regions and alignment one.
Prove that the returned requirements directly construct the existing bounded
streaming round trip. This step adds no public C requirements query, factory,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-530: LZMW rANS public C factory preserves opaque record layout

- Date: 2026-08-02
- Status: accepted

Expose DD-529 through a size-tagged fixed-width `marc_lzmw_rans_config`, a
direction-specific workspace query, and an immutable-direction transform
factory. The config carries known original size, raw frame size, entropy block
size, LZMW entry ceiling, rANS block ceiling, and common hard limits. Reserved
fields must remain zero.

Keep three caller-owned regions. Encoding maps primary storage to one raw frame,
secondary storage to canonical LZMW reference staging followed by one complete
rANS frame, and aligned opaque views to LZMW encoder entries. Decoding maps
primary storage to one complete encoded frame, secondary storage to reference
staging followed by private raw staging, and aligned opaque views to DD-529's
rANS block, LZMW phrase, and iterative expansion spans. The requirements query
is the only allocation authority; the factory repeats calculation and checked
partitioning before construction. Prove the pure-C lifecycle, short-region and
misalignment rejection, null-output rejection, reserved-field rejection, and
binary round trip. This publishes no CLI, completion, fuzz, benchmark, or
interoperability claim.

## DD-531: LZMW rANS completion reuses the reviewed public schedule

- Date: 2026-08-02
- Status: accepted

Apply the established public-ABI completion matrix to DD-530 with 64-byte raw
frames, 64-byte rANS blocks, at most four entropy blocks per frame, and the
exact LZMW reference ceiling `S = 4 * 64 = 256`. Bound payload by
`S + 8K = 288` and a complete frame by `56 + 528K + S + 8K = 2,456`.

At this frame size the LZMW ceiling equals the already reviewed LZD completion
ceiling `8 * ceil(64/2)`, so reuse the same data, deterministic repetition,
one-byte and mixed chunk schedules, repeated terminal calls, and malformed
fourth-frame schedule without changing their capacities. Invoke only public C
configuration, requirements, factory, process, and destroy functions. A
corrupt, truncated, or extended final frame may publish the first three
complete 64-byte frames but must leave its final raw byte untouched and return
a stable repeated error. This step adds no fuzz target, CLI, benchmark,
interoperability entry, or `Ready` claim.

## DD-532: LZMW rANS fuzzing fixes all three record regions

- Date: 2026-08-02
- Status: accepted

Add one bounded fuzz entry that presents each input first to the private
complete-frame decoder after strict prefix and parameter admission, then to the
public C streaming decoder created only through DD-530. Cap consumed fuzz bytes
at 8,192, total raw output at 4,096, one raw frame at 1,024, reference staging
at 4,096, compressed payload at 16,384, LZMW phrase records at 1,023,
iterative expansion references at 1,024, and rANS views at eight blocks. Derive
fixed byte arrays and aligned opaque storage conservatively from those bounds;
abort if the public requirements query exceeds them.

Choose input and output chunks from bounded bytes, but cap total process calls
independently at input cap plus output cap plus 32. Abort on invalid accounting,
zero-progress `Progress`, impossible starvation after all input, or exhaustion
of the finite call budget. Treat an ordinary decoder error, successful end, or
full bounded output as a valid fuzz outcome.

Freeze permanent regressions for every strict prefix of one canonical `ABABX`
stream, saturated generic frame extents, and a nonzero rANS descriptor reserved
byte. Each must publish zero bytes from its only frame, preserve raw sentinels,
and return the same error code and byte position on repetition. This step adds
no CLI, benchmark, interoperability entry, or `Ready` claim.

## DD-533: LZMW rANS CLI delegates all storage to the public ABI

- Date: 2026-08-02
- Status: accepted

Publish `lzmw-rans` as an explicit selector in the existing transactional CLI.
Use 65,536-byte outer frames and 65,536-byte rANS blocks. The exact LZMW bound
is 262,144 canonical reference bytes, which permits four entropy blocks, 2,112
descriptor bytes, and at most 262,176 payload bytes. Retain the public LZMW
maximum-entry default and a conservative 16-MiB aggregate internal-buffer
policy.

Initialize, query, create, process, and destroy the codec only through
`marc_lzmw_rans_*` and the common transform lifecycle. Allocate the three
direction-specific workspace regions from the returned requirements, including
the returned opaque alignment; do not reproduce any rANS-view, encoder-entry,
phrase, expansion-stack, or partition layout in the CLI. Reuse the existing
temporary-file transaction so configuration, allocation, codec, truncation,
trailing-data, and destination-collision failures publish no requested output
and leave no sibling temporary file. This step adds no benchmark,
interoperability entry, stream field, format variant, or `Ready` claim.

## DD-534: LZMW rANS benchmark retains the exact four-byte reference ceiling

- Date: 2026-08-02
- Status: accepted

Add `lzmw-rans` to the dependency-free benchmark runner using exactly DD-533's
65,536-byte frame and entropy-block profile and only the published C lifecycle.
Before timing, encode once, decode the exact encoded extent once, and require
byte equality. Then report complete-stream ratio, encode and decode throughput,
all three direction-specific workspace extents, and the larger caller-owned
workspace total.

For input extent `N` and nonempty outer-frame count `K`, reserve checked encoded
capacity `80 + 4N + 2200K`. The reference term follows the exact LZMW ceiling
of one four-byte reference per raw byte. The per-frame term reserves the
56-byte generic header plus four complete 528-byte descriptors and four
eight-byte states. Short final frames may use less. This step adds no stream
field, format variant, interoperability entry, optimization, or representative
performance claim.

## DD-535: Interoperability schema 25 appends LZMW rANS once

- Date: 2026-08-02
- Status: accepted

Define interoperability schema 25 as the frozen schema-24 profile order followed
once by `lzmw-rans`. Set `schema_version` to 25 and `codec_set` to
`marc-cli-v25`; require exactly 36 archives in manifest order. Keep schemas 1
through 24 explicit and unchanged. No older schema may inherit the new profile.

Generation must round-trip every archive locally before recording its size and
SHA-256. Verification must require leaf-only names, exact profile count and
order, recorded sizes and hashes, foreign decode equality, and byte-identical
local re-encoding. The local compatibility regression must reject a reordered
schema-25 manifest, derive schema 24 by removing only `lzmw-rans` and restoring
its version and codec set, then continue through every frozen older schema.
This establishes local format determinism only; external Windows/Linux
cross-checks remain required release evidence and no result is predicted.

## DD-536: Project version 0.1.2 publishes the completed rANS column

- Date: 2026-08-02
- Status: accepted

Release the completed rANS composition column as project version `0.1.2`.
Retain C ABI version 1, stream-format versions 1.0 and 1.1, and every existing
algorithm and variant representation. The six new named dictionary/rANS
profiles and interoperability schema 25 are additions within the established
compatibility-preserving `0.1.x` line; no previously published deterministic
stream byte changes.

Require the complete 2,048-test Release suite under both Windows compilers,
the pushed Windows/MSVC and Ubuntu 24.04 CI, and the recorded four-direction
schema-25 exchange with Ubuntu 26.04 before tagging. Preserve the explicitly
open second-architecture, representative-measurement, and longer sanitizer
fuzz evidence rather than overstating the release claim. Keep `0.2.0` reserved
for potentially incompatible API/default work or separately identified format
variants motivated by performance or compression-ratio changes.

## DD-537: LZ77 tANS entropizes the finalized token byte stream

- Date: 2026-08-02
- Status: accepted

Reserve `lz77-tans` for LZ77 variant 1 followed by tabled tANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZ77 parameter extension,
empty entropy parameters, canonical 16-byte token serialization, tANS table
log 12, and deterministic spread step 2563. Complete the token byte stream
before entropy processing. A tANS block may split a token but cannot cross an
outer frame. Reset the LZ77 window and every tANS model and automaton at each
frame.

For raw frame size `F`, require checked token bound `S <= 16F`. For nonzero
block size `B`, require `K = ceil(S/B)`, exact descriptor extent `528K`, and
per-block payload ceiling `Q(n) = 2 + ceil(12n/8)`. Bound total payload by the
checked sum of `Q` over every full and final-short block. Retain `F <= 2^20`.
Validate generic extents and all tANS descriptors, tables, initial states, bit
paths, terminal states, padding, and payload exhaustion before reconstructing
the exact token region in private staging. Only then validate LZ77 alignment,
references, overlap semantics, and exact raw extent before any raw
reconstruction or publication.

For raw `A`, independently freeze the 16-byte Literal token. Its tANS model is
`00:3840, 41:256`; the documented spread and state recurrence produce payload
`0A 05 03` with five valid transition bits, and the complete frame is 587
bytes. Prove this by composing only the existing standalone LZ77 encoder, tANS
encoder, and explicit generic serializers. This decision specifies bytes and
a reserved name only; it does not publish a combined decoder, encoder, stream
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-538: LZ77 tANS validation stops at the token boundary

- Date: 2026-08-02
- Status: accepted

Admit the first combined `lz77-tans` implementation as a strict bounded
complete-frame validator only. Validate the exact stream profile, LZ77
parameters, sequence, generic frame header, complete frame extent,
`S <= 16F` token bound and alignment, exact `K = ceil(S/B)` block count,
exact `528K` descriptor bytes, bounded payload sum from
`Q(n) = 2 + ceil(12n/8)`, caller-owned token and view capacities, and their
aggregate workspace before entropy output.

Parse every descriptor only after admission succeeds. Validate every tANS
block's model, spread, transition table, initial state, bit path, terminal
state, padding, and exact payload exhaustion before decoding any block. Only
after that complete validation pass may a second pass reconstruct exactly `S`
token bytes into private caller-owned staging. Invoke the existing LZ77
validator over the complete span and preserve its stable token index, format
error, reference, overlap, and exact raw-extent checks.

No raw staging or output span exists at this boundary. On every failure the
caller discards token and view workspace. Prove the 587-byte Literal vector, a
token split across four tANS blocks, every truncation, trailing data, short
storage, aggregate admission one byte short, malformed descriptors, a
malformed later block with untouched token staging, invalid reconstructed
LZ77 tokens, impossible entropy extents, and profile rejection. This decision
adds no private raw decoder, transactional publication, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-539: LZ77 tANS reconstruction remains private

- Date: 2026-08-02
- Status: accepted

Extend DD-538 with a bounded complete-frame decoder that reconstructs only
into caller-owned private raw staging. Require capacity for the complete
declared raw frame before descriptor parsing or entropy output, and count
those `F` bytes together with descriptor, payload, token staging, and tANS
views against `max_internal_buffered_bytes`.

Retain DD-538's all-block validation pass, second token reconstruction pass,
and complete LZ77 validation. Only after every entropy automaton and every
token semantic succeeds may the existing allocation-free LZ77 decoder
reconstruct literals and forward overlapping matches from immutable token
staging into exactly `F` private raw bytes. Preserve stable validation, format,
and decode errors; an unexpected failure after successful validation is a
distinct dictionary-decode error.

No caller-visible output span exists. On every failure the caller discards
views, token staging, and raw staging. Prove the 587-byte Literal frame, a
distance-one overlapping terminal match, raw staging one byte short before
token mutation, aggregate workspace one byte short including raw bytes, and
unchanged raw sentinels after a malformed later tANS block or invalid decoded
token. This decision adds no transactional publication, encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-540: LZ77 tANS frame publication is transactional

- Date: 2026-08-02
- Status: accepted

Add a caller-visible complete-frame decoder above DD-539. Require capacity for
the entire declared raw frame in a distinct output span before descriptor
parsing, entropy output, token staging mutation, or private raw mutation.
Output remains publication storage and is not added to DD-539's internal
workspace total.

Retain DD-538's complete entropy and token validation and DD-539's private
reconstruction. Copy exactly `F` bytes from private raw staging to caller
output once, only after the LZ77 decoder succeeds. Preserve all layered error
details and return without publication on every earlier failure.

Prove the Literal frame publishes only its declared byte while preserving
guards, a one-byte-short output fails before private mutation, and malformed
later tANS state or invalid reconstructed token leaves both private raw and
caller output sentinels unchanged. This decision adds no encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-541: LZ77 tANS planning freezes tokens before counting blocks

- Date: 2026-08-02
- Status: accepted

Add an encoder-side exact-frame planner for the inverse of DD-537. Require one
nonempty raw frame and validate the fixed profile and LZ77 parameters. First
run the deterministic LZ77 plan, admit caller-owned staging for the exact token
extent `S`, and encode the complete canonical token sequence once. Reject short
staging before modifying it.

Over the frozen token bytes, plan every consecutive tANS block of at most `B`
bytes without serialized output. Accumulate exact block count `K`, descriptor
extent `528K`, payload extent `P`, and complete serialized extent
`56 + 528K + P` with checked arithmetic. Enforce the block-count ceiling and
count token staging, planned descriptors, and planned payload against the
aggregate internal-buffer limit. Validate the resulting generic frame header.

Prove the 587-byte Literal extent and exact token bytes, a block size of five
splitting one token into four independently planned blocks, short token staging
without mutation, empty and unexpected raw extents, block-count overflow, and
aggregate workspace one byte short. This decision writes no serialized bytes
and adds no frame writer, streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-542: LZ77 tANS writing follows complete planning

- Date: 2026-08-02
- Status: accepted

Add the complete-frame writer above DD-541. Invoke the exact planner first,
then require capacity for the complete `56 + 528K + P` output before writing
the generic frame header or any entropy bytes. Preserve the frozen canonical
LZ77 token staging produced by planning.

Explicitly serialize the generic header, all `K` fixed tANS descriptors in
block order, and all `K` payloads in the same order. Replan and encode each
block over its unchanged token subspan, require every payload extent to match
the accumulated plan exactly, and reject any internal mismatch rather than
emitting an alternate stream.

Prove byte-for-byte equality with the independent 587-byte Literal frame,
deterministic repeated writing and full decoder round trip when block size five
splits the token, and one-byte-short serialized output without output mutation.
This decision adds no streaming transform, profile calculator, C ABI, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-543: LZ77 tANS streaming encoding buffers one complete frame

- Date: 2026-08-02
- Status: accepted

Add a known-size bounded streaming encoder above DD-542. Require caller-owned
storage for the largest raw frame, its conservative `16F` token staging, and
one complete serialized frame. Count all three active regions against
`max_internal_buffered_bytes` before preparing a frame.

Drain the fixed 80-byte stream prefix first. Collect exactly the next expected
raw frame, plan and write it completely, then drain it before reusing storage.
Allow completed frames to drain before EndInput, retain final EndInput across
output starvation, leave a partial frame open on `Flush`, reject ResetBlock,
and return sticky terminal status or errors without zero-progress `Progress`.

Prove byte identity against independently concatenated exact frames under
one-byte input and output, full-frame preparation plus nonterminal Flush,
constructor and aggregate workspace failures, empty input, premature EndInput,
unsupported reset, and repeated EndOfStream. This decision adds no streaming
decoder, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-544: LZ77 tANS streaming decoding commits complete frames

- Date: 2026-08-03
- Status: accepted

Add the matching known-size bounded streaming decoder. Collect and validate the
80-byte prefix, then each complete 56-byte frame header and declared body in
caller storage. Preflight encoded frame, tANS views, token staging, private raw
staging, and their aggregate before body collection.

Invoke DD-539 private reconstruction only after the full frame is present, and
drain raw bytes only after all tANS and LZ77 validation succeeds. Preserve
previously committed frames when a later frame fails; expose no bytes from the
failing frame. Retain EndInput across draining, reject truncation, trailing
data and ResetBlock, and make ended and error states sticky.

Prove one-byte input/output round trip, later-frame corruption after one commit,
all caller storage and aggregate limits, truncation, trailing data, reset,
empty input, nonterminal Flush, and premature final input. This decision adds
no profile calculator, C ABI, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-545: LZ77 tANS profiles derive the blockwise payload ceiling

- Date: 2026-08-03
- Status: accepted

Add an internal direction-specific profile calculator above DD-543 and DD-544.
For known-size encoding, derive the largest raw frame `F`, conservative `16F`
token staging, `K = ceil(16F/B)` blocks, exact `528K` descriptors, and the sum
of `Q(n) = 2 + ceil(12n/8)` for every full or final-short tANS block. Count raw,
token, and complete encoded-frame storage under the aggregate local limit.

For decoding, derive serialized-frame, token, private-raw, and block-view
requirements solely from validated local limits. Expose a view count rather
than the private `TansBlockView` layout. Use checked arithmetic throughout,
return canonical stream-header fields, and prove that the calculated regions
directly construct the streaming pair. This decision adds no C requirements
query, public factory, CLI selector, benchmark, fuzz target, completion claim,
or interoperability entry.

## DD-546: LZ77 tANS C admission preserves three opaque regions

- Date: 2026-08-03
- Status: accepted

Expose the DD-545 profile through a size-tagged `marc_lz77_tans_config`, a
direction-specific requirements query, and a factory returning the common
opaque transform. Retain the established primary/secondary/views ABI: encode
uses raw primary storage and token-plus-frame secondary storage with no views;
decode uses serialized primary storage, token-plus-private-raw secondary
storage, and aligned opaque tANS views.

Repeat profile admission at construction, validate all three buffer contracts,
and leave the transform pointer null on failure. Expose only view bytes and
alignment, never `TansBlockView` or another C++ layout. Prove a complete C11
round trip, exact queried extents, short-view rejection, and reserved-field
rejection. This step adds no CLI selector, completion matrix, fuzz target,
benchmark, or interoperability entry.

## DD-547: LZ77 tANS completion is proven through the public ABI

- Date: 2026-08-03
- Status: accepted

Exercise the completed `marc_lz77_tans_*` lifecycle without constructing any
private codec object. Cover empty input, every one-byte value, all byte values,
long zero and patterned inputs, deterministic generated bytes, and lengths 63,
64, and 65. Require byte-identical repeated encoding and identical streams
under `(1,1)`, `(7,5)`, and `(13,17)` input/output chunk schedules.

For a 193-byte four-frame stream, corrupt the final frame sequence, truncate
its last byte, and append one trailing byte independently. Each decode must
commit exactly the first 192 bytes, preserve the final sentinel, and return the
same sticky error position on repeated calls. This step adds no fuzz target,
CLI selector, benchmark, interoperability entry, or local `Ready` claim.

## DD-548: LZ77 tANS fuzzing has two bounded decoder boundaries

- Date: 2026-08-03
- Status: accepted

For each input of at most 8,192 bytes, exercise both the complete-frame private
decoder and the incremental stream transform. Bound total raw output to 4,096
bytes, one frame to 1,024 bytes, entropy payload to 8,192 bytes, dictionary
staging to 4,096 bytes, and block views to eight. Use only fixed caller-owned
arrays; derive input and output chunks from the fuzz bytes and abort if a
process result violates the core contract or exceeds the 12,320-call ceiling.

Seed the corpus with a truncated `MARC` prefix. Retain permanent tests requiring
write-free, sticky rejection of every proper prefix of a canonical stream,
saturated frame length fields, and invalid tANS frequency metadata. A Clang
libFuzzer smoke under AddressSanitizer and UndefinedBehaviorSanitizer must
complete 1,000 runs. This step adds no CLI selector, benchmark,
interoperability entry, or local `Ready` claim.

## DD-549: LZ77 tANS CLI admission uses only the public profile

- Date: 2026-08-03
- Status: accepted

Add the explicit selector `lz77-tans` to the transactional file adapter. Fix
raw frames and tANS blocks at 65,536 bytes. Derive `S = 1,048,576` canonical
token bytes, `K = 16` blocks, `528K = 8,448` descriptor bytes, per-block
`Q = 98,306`, aggregate payload `P = 1,572,896`, and encoder workspace
`F + S + 56 + 528K + P = 2,695,512` bytes.

Configuration initialization, directional requirements, transform creation,
processing, and destruction use only `marc_lz77_tans_*` and the common public
C ABI. Do not expose tANS view types or reproduce profile partitions in the
CLI. Reuse temporary-file publication and prove nonempty and empty round trips,
overwrite refusal, malformed cleanup, and strict trailing-data rejection. This
step adds no benchmark, interoperability entry, or local `Ready` claim.

## DD-550: LZ77 tANS benchmark verifies before measuring

- Date: 2026-08-03
- Status: accepted

Add `lz77-tans` to the dependency-free benchmark through only the public C
requirements, factory, process, and destroy lifecycle. Reuse DD-549's 64-KiB
raw frame and entropy block, 1,048,576-byte token ceiling, sixteen-block
ceiling, 1,572,896-byte payload ceiling, and 2,695,512-byte encoder aggregate.

Reserve complete-stream output with checked `80 + 24N + 8536K` arithmetic,
where `K` is the nonempty frame count. Query encoder and decoder workspaces
independently, prove a byte-exact round trip before timing, and create each
timed transform outside the elapsed interval. Report ratio, direction-specific
time and throughput, all six workspace extents, and peak caller workspace.
This changes no format or ABI and adds no interoperability entry or local
`Ready` claim.

## DD-551: Interoperability schema 26 appends LZ77 tANS

- Date: 2026-08-03
- Status: accepted

Freeze the exact thirty-six-entry schema-25 order and append `lz77-tans` once
as entry 37. Name the set `marc-cli-v26`, retain the deterministic 8,193-byte
fixture, and record complete archive sizes and SHA-256 values only after local
decode equality succeeds.

The verifier requires exact schema order, hashes, foreign decode equality, and
byte-identical local re-encoding. The compatibility regression rejects a
reordered schema-26 manifest, derives schema 25 by removing only `lz77-tans`
and restoring its version and codec set, then verifies every frozen schema
through version 1. This is local schema admission; Windows/Linux artifact
exchange remains a separate release gate and the profile remains `In progress`.

## DD-552: Schema 26 external exchange completes LZ77 tANS admission

- Date: 2026-08-03
- Status: accepted

Bind external evidence to exact revision
`5b2aa31ba3333c311ad4086b3438915a6c3ce36d`. Require the Ubuntu 26.04/Clang
executable to verify the Windows/MSVC and Ubuntu 24.04 CI bundles, generate and
self-verify its own schema-26 bundle, and require the Windows/MSVC executable
to verify that Ubuntu-produced bundle.

All four passes must report 37 archives and the exact revision while enforcing
manifest order, size, SHA-256, fixture equality, and byte-identical local
re-encoding. Together with DD-551's local chain, this satisfies every current
admission gate and changes `lz77-tans` from `In progress` to `Ready`. The
evidence remains x86-64 and does not imply testing on another architecture or
a non-WSL Ubuntu 26.04 kernel.

## DD-553: LZSS tANS entropizes finalized token bytes

- Date: 2026-08-03
- Status: accepted

Reserve `lzss-tans` for LZSS variant 1 followed by tabled tANS variant 1.
LZSS must first finalize its complete canonical variable-width token byte
sequence for one outer frame. tANS consumes that sequence as untyped bytes;
therefore an entropy block may split a two-byte Literal or nine-byte Match,
but may not cross the frame boundary where both algorithms reset.

For raw size `F`, token size `S`, nonzero block size `B`, and
`K = ceil(S/B)`, require `0 < S <= 2F`, exact descriptor extent `528K`, and
the checked sum of per-block payload ceilings `Q(n) = 2 + ceil(12n/8)`.
Retain `F <= 2^20`. Decode all tANS blocks into private staging before parsing
the variable-length LZSS grammar or publishing raw bytes.

Independently fix raw `A` as token bytes `00 41`, normalized frequencies
`00:2048` and `41:2048`, tANS payload `06 00 00`, and one complete 587-byte
frame. This decision reserves the representation and vector only; it adds no
combined validator, factory, CLI selector, benchmark, fuzzer, completion
claim, or interoperability entry.

## DD-554: LZSS tANS validation is two-pass and token-atomic

- Date: 2026-08-03
- Status: accepted

Add the first bounded decoder-facing implementation of DD-553. Admit the
complete generic frame extent, exact `K`, exact `528K` descriptor region,
blockwise tANS payload ceiling, caller-owned `K` tANS views, complete `S`
token staging, and their aggregate internal byte count before entropy work.

Parse all descriptors and validate every tANS model, spread, transition table,
initial state, bit path, terminal state, and padding without writing a token
byte. Only when all `K` blocks succeed may a second pass reconstruct exactly
`S` token bytes. Then validate the full variable-length LZSS grammar,
references, output extent, token index, and input offset without reconstructing
raw bytes. A malformed later block must leave all token staging untouched.
This decision adds no raw decoder, transactional publisher, encoder, streaming
transform, profile calculator, C factory, CLI, benchmark, fuzzer, completion
claim, or interoperability entry.

## DD-555: LZSS tANS raw reconstruction remains private

- Date: 2026-08-03
- Status: accepted

Extend DD-554 with a caller-owned raw staging span but no caller-visible output
span. Admit the complete `F` bytes before descriptor parsing or token mutation,
and include them with descriptors, payload, token staging, and tANS views under
`max_internal_buffered_bytes`.

After every tANS block and the complete LZSS token grammar have succeeded,
invoke the existing allocation-free LZSS decoder over the validated token
region and reconstruct exactly `F` bytes. Preserve overlap-copy semantics and
the stable LZSS decode, validation, format, token-index, and byte-offset
diagnostics. Entropy or token failure must leave raw staging untouched. This
decision adds no transactional caller publication, encoder, streaming
transform, profile calculator, C factory, CLI, benchmark, fuzzer, completion
claim, or interoperability entry.

## DD-556: LZSS tANS publication is one transactional copy

- Date: 2026-08-03
- Status: accepted

Wrap DD-555 with a distinct caller output span. Require its complete `F`-byte
capacity before descriptor parsing, token mutation, or raw reconstruction.
Publication storage is not internal workspace and is not counted against
`max_internal_buffered_bytes`.

Preserve the complete validation and private reconstruction sequence, then
copy exactly `F` bytes from raw staging to caller output once. Output-capacity,
entropy, token, or reconstruction failure must leave caller output unchanged.
This decision adds no encoder, streaming transform, profile calculator, C
factory, CLI, benchmark, fuzzer, completion claim, or interoperability entry.

## DD-557: LZSS tANS planning freezes token bytes before sizing

- Date: 2026-08-03
- Status: accepted

Add a write-free exact-frame planner above DD-553's encoder boundary. Plan and
materialize the complete canonical LZSS token sequence once in caller-owned
staging. Enforce `0 < S <= 2F`, then plan each consecutive tANS block over that
frozen sequence and accumulate exact `K`, `528K`, `P`, and
`56 + 528K + P` extents with checked arithmetic.

Reject block-count, tANS-planning, integer, generic-frame, and aggregate
descriptor-plus-payload-plus-token limits without accepting a serialized
output span. Validate the synthesized generic frame header against sequence,
committed raw extent, configured frame size, and local limits. This decision
adds no complete-frame writer, streaming transform, profile calculator, C
factory, CLI, benchmark, fuzzer, completion claim, or interoperability entry.

## DD-558: LZSS tANS frame writing follows one complete plan

- Date: 2026-08-03
- Status: accepted

Add the complete-frame writer above DD-557. Run the exact plan first and admit
the complete serialized output capacity before writing its first byte. Emit the
56-byte generic header, all `K` consecutive 528-byte descriptors, and all `K`
consecutive payloads explicitly.

Replan each block over the unchanged canonical token staging and require its
payload size to equal the accumulated plan. Descriptor serialization or tANS
encoding disagreement is an internal error, not an alternate representation.
One-byte-short output must remain entirely unchanged. This decision adds no
streaming transform, profile calculator, C factory, CLI, benchmark, fuzzer,
completion claim, or interoperability entry.

## DD-559: LZSS tANS streaming encode drains immutable frames

- Date: 2026-08-03
- Status: accepted

Add a bounded known-size streaming encoder above DD-558. Emit the ordinary
64-byte stream header and 16-byte LZSS parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete
serialized representation through the exact writer, and drain that immutable
frame before reusing any workspace.

At construction, validate the fixed pipeline, parameters, known original size,
largest raw frame, conservative `2F` token staging, and prefix serialization.
Before each frame, count raw collection, exact token staging, and exact
serialized frame under one aggregate limit. Input and output capacities may be
one byte. `Flush` does not close a partial frame; `EndInput` remains latched
while prefix or frame bytes drain; full frames may drain before finish.
`ResetBlock`, unknown flags, premature end, excess input, and insufficient
workspace are sticky errors. This decision adds no streaming decoder, profile
calculator, C factory, CLI, benchmark, fuzzer, completion claim, or
interoperability entry.

## DD-560: LZSS tANS streaming decode publishes complete frames

- Date: 2026-08-03
- Status: accepted

Add a bounded known-size streaming decoder above DD-556. Collect and validate
the ordinary 64-byte stream header and 16-byte LZSS parameter region before
accepting frames. For each frame, collect its 56-byte generic header first,
admit the exact declared descriptor-plus-payload body and every caller-owned
workspace, then collect and decode the complete frame into private raw staging.
Drain raw bytes only after the complete transactional decode succeeds.

Count encoded frame storage, tANS block views, canonical token staging, and
private raw staging under one aggregate limit. Input and output capacities may
be one byte. A malformed later frame cannot alter bytes committed from an
earlier frame or expose a prefix of the failing frame. Reject truncated and
trailing data, invalid prefix or frame extents, insufficient workspace,
`ResetBlock`, and unknown flags with sticky errors. `Flush` remains
nonterminal, and `EndInput` remains latched while private raw bytes drain.
This decision adds no profile calculator, C factory, CLI, benchmark, fuzzer,
completion claim, or interoperability entry.

## DD-561: LZSS tANS profiles derive bounded blockwise storage

- Date: 2026-08-03
- Status: accepted

Add an internal direction-specific profile calculator above DD-559 and DD-560.
For known-size encoding, derive the largest raw frame `F`, conservative `2F`
token staging, `K = ceil(2F/B)` blocks, exact `528K` descriptors, and the
sum of `Q(n) = 2 + ceil(12n/8)` for every full or final-short tANS block.
Count raw, token, and complete encoded-frame storage under the aggregate local
limit.

For decoding, derive serialized-frame, token, private-raw, and block-view
requirements solely from validated local limits. Expose a view count rather
than the private `TansBlockView` layout. Use checked arithmetic throughout,
return canonical stream-header fields, map stable core errors, and prove that
the calculated regions directly construct the streaming pair. This decision
adds no C requirements query, public factory, CLI selector, benchmark, fuzzer,
completion claim, or interoperability entry.

## DD-562: LZSS tANS C admission preserves opaque tANS views

- Date: 2026-08-03
- Status: accepted

Expose DD-561 through a size-tagged `marc_lzss_tans_config`, a
direction-specific requirements query, and a factory returning the common
opaque transform. Retain the established primary/secondary/views ABI: encode
uses raw primary storage and token-plus-frame secondary storage with no views;
decode uses serialized primary storage, token-plus-private-raw secondary
storage, and aligned opaque tANS views.

Repeat profile admission at construction, validate all three buffer contracts,
and leave the transform pointer null on failure. Expose only view bytes and
alignment, never `TansBlockView` or another C++ layout. Prove a complete C11
round trip, exact queried extents, short-view rejection, and reserved-field
rejection. This step adds no completion matrix, CLI selector, fuzz target,
benchmark, completion claim, or interoperability entry.

## DD-563: LZSS tANS completion is proven through the public ABI

- Date: 2026-08-03
- Status: accepted

Exercise the completed `marc_lzss_tans_*` lifecycle without constructing any
private codec object. Cover empty input, every one-byte value, all byte values,
repeated bytes, repeated binary patterns, deterministic generated data, and
lengths immediately around frame boundaries. Repeated encoding and varied
input/output chunking must produce identical streams and exact round trips.

For a four-frame stream, corrupt the final frame header, truncate its final
byte, and append trailing data independently. Decoding must commit exactly the
first three frames, leave the final output sentinel unchanged, and return the
same sticky error category and position on repeated calls. This step adds no
fuzz target, CLI selector, benchmark, local `Ready` claim, or interoperability
entry.

## DD-564: LZSS tANS fuzzing is bounded and frame-atomic

- Date: 2026-08-03
- Status: accepted

Exercise both the complete-frame private decoder and incremental public-frame
decoder from one libFuzzer entry point. Cap serialized input and payload at
8 KiB, total output at 4 KiB, one raw frame at 1 KiB, canonical LZSS token
staging at 2 KiB, and tANS metadata at eight caller-owned views. Derive input
and output chunks only from the bounded input and stop at a fixed call ceiling.

Retain ordinary regression tests for every truncation of a canonical frame,
oversized serialized frame lengths, and an invalid tANS descriptor. All such
failures must publish zero bytes from the frame, preserve the caller sentinel,
and remain sticky with the same error category and position. This step adds no
CLI selector, benchmark, local `Ready` claim, or interoperability entry.

## DD-565: LZSS tANS CLI admission uses only the public profile

- Date: 2026-08-03
- Status: accepted

Add the explicit selector `lzss-tans` to the transactional file adapter. Fix
raw frames and tANS blocks at 65,536 bytes. Derive `S = 131,072` canonical
token bytes, `K = 2` blocks, `528K = 1,056` descriptor bytes, `P = 196,612`
payload bytes, and exact encoder aggregate `F + S + 56 + 528K + P = 394,332`
bytes. Use a conservative 512-KiB internal-buffer policy for both directions.

Configuration initialization, directional requirements, transform creation,
processing, and destruction use only `marc_lzss_tans_*` and the common public
C ABI. Opaque tANS views and private workspace partitions remain outside the
CLI. Reuse temporary-file publication and prove nonempty and empty round trips,
overwrite refusal, malformed cleanup, and strict trailing-data rejection under
both supported Windows compilers. This step adds no benchmark,
interoperability entry, or local `Ready` claim.

## DD-566: LZSS tANS benchmark verifies before measuring

- Date: 2026-08-03
- Status: accepted

Add `lzss-tans` to the dependency-free benchmark through only the public C
requirements, factory, process, and destroy lifecycle. Reuse DD-565's 65,536-
byte raw frame and entropy block, 131,072-byte token ceiling, two tANS blocks,
1,056 descriptor bytes, 196,612-byte payload ceiling, and conservative
512-KiB internal policy.

Size the complete encoded destination as the 80-byte prefix plus at most three
transition bytes per raw byte and, for each nonempty frame, one 56-byte header,
two 528-byte descriptors, and two two-byte states. Query encoder and decoder
workspaces independently, prove byte-exact round trip before timing, and create
each transform outside the elapsed interval. Report ratio, direction-specific
time and throughput, all six workspace extents, and peak caller workspace.
This changes no format or ABI and adds no interoperability entry or local
`Ready` claim.

## DD-567: Interoperability schema 27 appends LZSS tANS

- Date: 2026-08-03
- Status: accepted

Freeze the exact thirty-seven-entry schema-26 order and append `lzss-tans`
once as entry 38. Name the new codec set `marc-cli-v27`; retain the existing
deterministic 8,193-byte binary fixture, full Git object ID, file extents, and
SHA-256 records.

Generation must round-trip every archive before recording it. Verification
requires the exact schema order, foreign decode equality, and byte-identical
local re-encoding. The compatibility regression rejects a reordered schema-27
manifest, derives schema 26 by removing only `lzss-tans`, then verifies
schemas 1 through 26 unchanged. This establishes local schema admission but
does not claim cross-platform completion or promote the profile to `Ready`.

## DD-568: LZ78 tANS entropizes finalized fixed-width tokens

- Date: 2026-08-04
- Status: accepted

Reserve `lz78-tans` for LZ78 variant 1 followed by tabled tANS variant 1.
LZ78 must first finalize its complete canonical eight-byte Pair or FinalIndex
sequence for one outer frame. tANS consumes that sequence as untyped bytes;
therefore an entropy block may split a token but may not cross the frame
boundary where both algorithms reset.

For raw size `F`, token size `S`, nonzero block size `B`, and
`K = ceil(S/B)`, require `0 < S <= 8F`, `S mod 8 = 0`, exact descriptor
extent `528K`, and the checked sum of per-block payload ceilings
`Q(n) = 2 + ceil(12n/8)`. Retain `F <= 2^20`. Decode all tANS blocks into
private staging before validating token tags, reserved fields, phrase
references, final-token placement, or exact expansion.

Independently fix raw `A` as token bytes `00 41 00 00 00 00 00 00`, normalized
frequencies `00:3584` and `41:512`, tANS initial-state offset `0x046B`, four
zero transition bits, payload `6B 04 00`, and one complete 587-byte frame.
This decision reserves the representation and vector only; it adds no combined
validator, factory, CLI selector, benchmark, fuzzer, completion claim, or
interoperability entry.

## DD-569: LZ78 tANS validates all entropy before phrase parsing

- Date: 2026-08-04
- Status: accepted

Implement the first complete-frame validator for DD-568 without raw
reconstruction or publication. Before entropy work, require the exact frame
extent, nonzero eight-byte-aligned `S <= 8F`, exact `K` and `528K`, the checked
blockwise payload ceiling, complete caller token staging, `K` tANS views, and
the full bounded LZ78 phrase-record workspace.

Count descriptors, payload, token bytes, tANS views, and phrase records under
`max_internal_buffered_bytes`. Parse every descriptor and validate every tANS
state path before decoding any token byte. Only after all blocks succeed may a
second pass reconstruct exactly `S` bytes and invoke the ordinary LZ78 token
and phrase-graph validator. Preserve block index and LZ78 token/input offsets
where practical. A malformed later block must leave the entire token staging
unchanged. This decision adds no raw decoder, publisher, encoder, streaming
transform, public API, CLI, benchmark, fuzzer, or interoperability entry.

## DD-570: LZ78 tANS reconstructs only into disposable raw staging

- Date: 2026-08-04
- Status: accepted

Extend DD-569 only with private raw reconstruction. Require caller-owned raw
staging of at least the declared `F` bytes before descriptor parsing, token
mutation, or phrase-record mutation, and count those bytes with descriptors,
payload, tokens, tANS views, and phrase records under the aggregate internal-
workspace limit.

Reuse the complete entropy and phrase-graph validation unchanged. Only after
it succeeds may the allocation-free LZ78 decoder expand Pair and FinalIndex
phrases iteratively into exactly `F` raw bytes. Retain LZ78 decode, validation,
format, token-index, and input-offset diagnostics. Expose no caller output span
and require every caller to discard all workspace on failure. This decision
adds no transactional publisher, encoder, streaming transform, public API,
CLI, benchmark, fuzzer, or interoperability entry.

## DD-571: LZ78 tANS publishes only complete validated frames

- Date: 2026-08-04
- Status: accepted

Add one transactional wrapper above DD-570. Require a distinct caller output
span of at least the declared `F` bytes before descriptor parsing or any
private mutation. Do not count publication storage against the internal
workspace limit.

Run the same complete entropy validation, phrase-graph validation, and private
raw reconstruction unchanged. Copy exactly `F` bytes from private raw staging
to caller output once and only after all layers succeed. Output capacity,
entropy, dictionary, or reconstruction failure must preserve every caller
output byte. This decision adds no encoder, streaming transform, public API,
CLI, benchmark, fuzzer, completion claim, or interoperability entry.

## DD-572: LZ78 tANS planning freezes canonical tokens once

- Date: 2026-08-04
- Status: accepted

Add a no-output exact-frame planner for one nonempty raw frame. Preflight the
bounded LZ78 encoder-record count and canonical token capacity, then plan and
materialize the complete eight-byte token sequence exactly once. Reject an
empty or unexpected raw-frame extent.

Plan every consecutive tANS block over the frozen token staging and accumulate
exact block count, `528K` descriptor bytes, payload bytes, and complete frame
extent with checked arithmetic. Count encoder records, tokens, descriptors,
and payload under `max_internal_buffered_bytes`, enforce block limits, and
validate the synthesized generic frame header. Accept no serialized output
span. This decision adds no frame writer, streaming transform, public API,
CLI, benchmark, fuzzer, completion claim, or interoperability entry.

## DD-573: LZ78 tANS writes only after exact complete-frame admission

- Date: 2026-08-04
- Status: accepted

Add the deterministic complete-frame writer above DD-572. Invoke the exact
plan first and require capacity for its entire serialized extent before
writing any output byte. Then explicitly serialize the generic frame header,
all fixed-size descriptors contiguously, and all payloads contiguously.

Replan every block only over the frozen canonical token staging and require
its payload size and the final token and payload offsets to equal the accepted
plan. Treat any discrepancy as an internal error. Short serialized output is
therefore rejected without mutation. This decision adds no streaming
transform, public API, CLI, benchmark, fuzzer, completion claim, or
interoperability entry.

## DD-574: LZ78 tANS streaming encode drains immutable frames

- Date: 2026-08-04
- Status: accepted

Add a bounded known-size streaming encoder above DD-573. Emit the ordinary
64-byte stream header and 16-byte LZ78 parameter region first. Collect at most
one configured raw frame in caller-owned storage, prepare its complete token
and serialized representation through the exact writer, and drain that
immutable frame before reusing any workspace.

At construction, validate the fixed pipeline, parameters, known original size,
largest raw frame, conservative `8F` token staging, encoder records, and prefix
serialization. Before each frame, count raw collection, exact token staging,
encoder records, and exact serialized frame under one aggregate limit. Input
and output capacities may be one byte. `Flush` does not close a partial frame;
`EndInput` remains latched while prefix or frame bytes drain; full frames may
drain before finish. `ResetBlock`, unknown flags, premature end, excess input,
and insufficient workspace are sticky errors. This decision adds no streaming
decoder, profile calculator, C factory, CLI, benchmark, fuzzer, completion
claim, or interoperability entry.

## DD-575: LZ78 tANS streaming decode publishes only complete frames

- Date: 2026-08-04
- Status: accepted

Add the matching bounded known-size streaming decoder above DD-569 through
DD-574. Collect and validate the ordinary 80-byte prefix, then each 56-byte
frame header. Before collecting a frame body, enforce aligned `S <= 8F`, exact
block count and `528K` descriptors, the blockwise 12-bit transition ceiling,
and caller capacity for the complete encoded frame, tANS views, token staging,
LZ78 phrase records, and private raw staging under one aggregate limit.

Decode a frame only after its entire declared body is collected. Invoke the
private transactional reconstruction boundary, then drain raw bytes from
staging before collecting the next frame. One-byte input/output is mandatory;
`Flush` under starvation remains nonterminal; final `EndInput` survives output
drain. Reject truncation, trailing bytes, malformed later frames, reset, and
unknown flags with sticky errors. This decision adds no profile calculator, C
factory, CLI, benchmark, fuzzer, completion claim, or interoperability entry.

## DD-576: LZ78 tANS profile derives every bounded workspace

- Date: 2026-08-04
- Status: accepted

Add an internal direction-specific profile calculator above DD-574 and DD-575.
For the largest known raw frame `F`, calculate encoder token capacity `8F`,
LZ78 encoder-record count, `K = ceil(8F/B)`, `528K` descriptor bytes, the
blockwise tANS payload ceiling, and complete frame capacity with checked
arithmetic. Count raw, token, serialized frame, and encoder records under the
same aggregate policy used by the streaming encoder.

Derive decoder encoded-frame, token, private-raw, block-view, and phrase-entry
capacities only from validated local hard limits. Define one exact aligned
opaque layout for tANS views followed by LZ78 phrase records; reject short,
misaligned, or altered requirements before publishing typed spans. Empty
encoding has zero byte regions and alignment one. Prove the calculated regions
directly construct the streaming round trip. This decision adds no C
requirements query, public factory, CLI, benchmark, fuzzer, completion claim,
or interoperability entry.

## DD-577: LZ78 tANS C factory preserves exact workspace boundaries

- Date: 2026-08-04
- Status: accepted

Expose a size-tagged `marc_lz78_tans_config`, initializer, direction-specific
workspace requirements query, and factory without changing the ABI version or
any existing structure. The config carries known original size, raw frame and
entropy block sizes, LZ78 entry bound, and every local hard limit required by
DD-576.

Map encoder requirements to raw primary storage, token-plus-encoded secondary
storage, and aligned encoder-record views. Map decoder requirements to encoded-
frame primary storage, token-plus-private-raw secondary storage, and one aligned
tANS-view-plus-LZ78-phrase region. Revalidate configuration, exact capacity,
reserved fields, and alignment before typed partition or allocation; publish no
handle on failure. Exercise the lifecycle from a C11 translation unit. This
decision adds no CLI, benchmark, fuzzer, completion claim, or interoperability
entry.

## DD-578: LZ78 tANS CLI admits only the public bounded profile

- Date: 2026-08-04
- Status: accepted

Add the explicit `lz78-tans` selector only after the size-tagged public C
configuration, directional requirements query, and transform factory exist.
Fix raw frames and entropy blocks at 65,536 bytes, canonical LZ78 staging at
`8F = 524,288` bytes, block count at eight, tANS descriptors at 4,224 bytes,
payload at most 786,448 bytes, phrase entries at 65,536, and aggregate
internal storage at 4 MiB.

The adapter must obtain primary, secondary, and aligned opaque-view extents
from the public requirements query and construct the transform only through
the public factory. It must retain existing bounded streaming, destination
non-overwrite, temporary-file transaction, malformed-input cleanup, and strict
trailing-data rejection. Prove the repository-standard multi-frame binary
round trip and negative file behaviors through the generic CLI regression.
This decision adds no benchmark, fuzz target, completion claim, interoperability
archive, or schema revision.

## DD-579: LZ78 tANS benchmark measures the admitted public profile

- Date: 2026-08-04
- Status: accepted

Add `lz78-tans` to the dependency-free benchmark only through DD-578's fixed
public profile and the `marc_lz78_tans_*` lifecycle. Reuse the 65,536-byte raw
frame and entropy block, 524,288-byte token ceiling, eight tANS blocks,
786,448-byte payload ceiling, 65,536 phrase entries, and 4-MiB aggregate
policy. Derive checked complete-stream capacity as `80 + 12N + 4296K` for raw
input extent `N` and nonempty frame count `K`.

Query and report all three direction-specific caller-owned workspace regions,
including aligned opaque views. Before timing, require one byte-exact encode
and decode through independently constructed public transforms. Then report
compression ratio and encode/decode throughput without imposing a performance
floor. Add a one-iteration README smoke under both supported Windows compiler
configurations. This decision adds no optimized format variant, fuzz target,
completion claim, interoperability archive, or schema revision.

## DD-580: LZ78 tANS fuzzing is fixed-memory and dual-boundary

- Date: 2026-08-04
- Status: accepted

Add a bounded decoder fuzz harness only after DD-577's public lifecycle and
DD-569 through DD-575's private and streaming validation paths are stable.
Truncate supplied input to 8 KiB; permit at most 4 KiB total raw output, one
1-KiB raw frame, 8 KiB of canonical LZ78 tokens, 16 KiB of compressed payload,
eight tANS block views, and 1,024 LZ78 phrases. Include encoded-frame, token,
private-raw, mixed aligned views, and output storage in fixed compile-time
ceilings and one aggregate hard limit.

Drive the complete-frame private decoder only after a valid exact profile
prefix, and independently drive the public C streaming decoder with chunk
sizes derived solely within the bounded input. Enforce the process-result
contract and a finite call ceiling; reaching the ceiling is a reproducible
failure. Permanently test every proper prefix of a canonical stream, impossible
frame extents, and an invalid tANS descriptor for zero publication and sticky
error identity. This decision adds no corpus finding, completion claim,
interoperability archive, or schema revision.

## DD-581: LZ78 tANS completion is proved through the public ABI

- Date: 2026-08-04
- Status: accepted

Apply the established public-ABI completion matrix to DD-577 with 64-byte raw
frames, 64-byte entropy blocks, at most 512 canonical token bytes, eight tANS
blocks, 64 phrase entries, and a 65,536-byte aggregate policy. Construct every
encoder and decoder solely through the size-tagged config, directional
requirements query, aligned three-region workspace, transform factory,
process function, and destroy function.

Round-trip empty input, every one-byte value, all byte values in sequence,
long zero runs, repeated binary patterns, deterministic pseudo-random bytes,
and lengths 63, 64, and 65. Require byte-identical repeated encoding and the
same multi-frame bytes under one-byte and mixed input/output chunks. Repeated
calls after success must remain ended with zero progress. For a four-frame
stream, corrupt and truncate the final frame and append trailing data; retain
exactly the first three committed frames, leave the final output sentinel
unchanged, and repeat the same sticky error identity. This decision establishes
local readiness only and adds no interoperability archive or schema revision.

## DD-582: Interoperability schema 28 appends LZ78 tANS

- Date: 2026-08-04
- Status: accepted

Freeze the exact thirty-eight-entry schema-27 order and append `lz78-tans`
once as entry 39. Name the new codec set `marc-cli-v28`; retain the existing
deterministic 8,193-byte binary fixture, full Git object ID, file extents, and
SHA-256 records.

Generation must round-trip every archive before recording it. Verification
requires the exact schema order, foreign decode equality, and byte-identical
local re-encoding. The compatibility regression rejects a reordered schema-28
manifest, derives schema 27 by removing only `lz78-tans`, then verifies
schemas 1 through 27 unchanged. This establishes local schema admission but
does not claim cross-platform completion.

## DD-583: LZW tANS entropizes finalized packed code bytes

- Date: 2026-08-04
- Status: accepted

Reserve `lzw-tans` for LZW variant 1 followed by tabled tANS variant 1. LZW
must first finalize its complete canonical LSB-first packed code region,
including zero high padding through the last byte, for one outer frame. tANS
consumes that region as untyped bytes; therefore an entropy block may split a
variable-width code but may not split a byte or cross the frame boundary where
both algorithms reset.

For raw size `F`, configured maximum code width `W`, packed size `S`, nonzero
block size `B`, and `K = ceil(S/B)`, require
`0 < S <= ceil(FW/8)`, exact descriptor extent `528K`, and the checked sum of
per-block payload ceilings `Q(n) = 2 + ceil(12n/8)`. Retain `F <= 2^20`.
Decode all tANS blocks into private staging before validating LZW code-width
transitions, references, `KwKwK`, dictionary growth, exact raw expansion,
packed exhaustion, or high padding bits.

Independently fix raw `A` as packed bytes `41 00`, normalized frequencies
`00:2048` and `41:2048`, tANS initial-state offset `0x000C`, two zero
transition bits, payload `0C 00 00`, and one complete 587-byte frame. This
decision reserves the representation and vector only; it adds no combined
validator, factory, CLI selector, benchmark, fuzzer, completion claim, or
interoperability entry.

## DD-584: LZW tANS validates all entropy before code parsing

- Date: 2026-08-04
- Status: accepted

Implement the first complete-frame validator for DD-583 without raw
reconstruction or publication. Before entropy work, require the exact frame
extent, nonzero `S <= ceil(FW/8)`, exact `K` and `528K`, the checked blockwise
payload ceiling, complete caller packed staging, `K` tANS views, and the full
bounded LZW phrase-record workspace.

Count descriptors, payload, packed bytes, tANS views, and phrase records under
`max_internal_buffered_bytes`. Parse every descriptor and validate every tANS
state path before decoding any packed byte. Only after all blocks succeed may
a second pass reconstruct exactly `S` bytes and invoke the ordinary LZW code-
stream validator. Preserve block index and LZW code, byte, and bit offsets
where practical. A malformed later block must leave the entire packed staging
unchanged. This decision adds no raw decoder, publisher, encoder, streaming
transform, public API, CLI, benchmark, fuzzer, or interoperability entry.

## DD-585: LZW tANS reconstructs only after complete validation

- Date: 2026-08-04
- Status: accepted

Add a bounded private-raw decoder above DD-584. Require complete raw staging
capacity and count that extent in the aggregate workspace before descriptor
parsing or entropy output. Reuse DD-584 unchanged to validate every tANS block,
reconstruct the complete packed LZW region, and validate the complete code
graph. Only then invoke the existing iterative LZW decoder into the admitted
raw staging span.

Publish no caller-visible bytes. On any error the caller discards all private
workspaces; insufficient raw capacity or aggregate memory must leave packed
and raw staging unchanged. Prove the independent raw-`A` frame, phrase and
`KwKwK` reconstruction across tANS block boundaries, preflight failures, and
invalid-code raw atomicity. This step adds no transactional public output,
encoder, streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-586: LZW tANS publishes one fully validated frame atomically

- Date: 2026-08-04
- Status: accepted

Add a transactional complete-frame wrapper above DD-585. Before descriptor
parsing or any private mutation, require caller output capacity for the entire
declared raw extent in addition to the disposable packed, phrase, view, and raw
staging regions. Caller output is not internal workspace and does not count
against `max_internal_buffered_bytes`.

Run DD-584 validation and DD-585 private reconstruction unchanged. Only after
every tANS block, the complete LZW code graph, and private raw reconstruction
succeed may one final copy publish exactly the declared raw extent. Short
output, malformed entropy, and invalid LZW codes or padding must leave the
complete caller destination unchanged. This step adds no encoder, streaming
transform, profile calculator, C ABI, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-587: LZW tANS planning freezes packed codes before entropy

- Date: 2026-08-04
- Status: accepted

Add a bounded write-free exact-frame planner above DD-586. Complete
deterministic LZW parsing using caller-owned encoder records, admit the exact
packed extent, and serialize the full canonical LSB-first code stream including
final zero padding into separate staging. Only those immutable packed bytes may
be divided into tANS blocks; block boundaries remain independent of code
boundaries.

Plan every tANS block without emitting descriptors or payloads, sum exact
payload and `528K` descriptor extents with checked arithmetic, and count
encoder records, packed staging, descriptors, and payload in one aggregate
workspace total. Validate the synthesized generic header and return the exact
complete-frame extent without accepting serialized output. Prove the frozen
587-byte raw-`A` extent, deterministic `ABABABA` codes across three blocks,
capacity atomicity, block and aggregate rejection, and raw-frame mismatch.
This step adds no frame encoder, streaming transform, profile calculator, C
ABI, CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-588: LZW tANS encoding emits only a completed plan

- Date: 2026-08-04
- Status: accepted

Add the bounded complete-frame encoder above DD-587. Run the exact planner to
completion and admit the full serialized destination before writing any frame
byte. Serialize the generic header explicitly, then repeat each deterministic
tANS block plan over the frozen packed LZW staging and require every payload
extent to match the previously summed plan.

Serialize each fixed descriptor and exact payload into its precomputed region,
then require final packed and payload offsets to match the plan. Raw `A` must
reproduce the independent 587-byte frame exactly. A multi-block `ABABABA`
frame must be byte-identical across runs and decode transactionally to the
source. A one-byte-short output must remain wholly unchanged. This step adds
no streaming transform, profile calculator, C ABI, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-589: LZW tANS streaming encode buffers one exact frame

- Date: 2026-08-05
- Status: accepted

Add the first bounded known-size streaming encoder above DD-588. Emit the
canonical 64-byte stream header and 16-byte LZW parameter extension, collect at
most one raw frame, plan and encode that complete frame into private immutable
storage, and drain it fully before accepting input for a later frame.

Construction validates the fixed variant-1 profile, declared original size,
largest raw frame, conservative `ceil(FW/8)` packed capacity, and required LZW
encoder records. Per-frame aggregate accounting includes raw staging, actual
packed staging, the exact serialized frame, and encoder records. Arbitrary
input/output chunking must preserve canonical bytes; `Flush` leaves a partial
frame open; retained `EndInput` drains all pending bytes; and `ResetBlock`,
unknown flags, premature end, or excess input fail stably. This step adds no
streaming decoder, profile calculator, C ABI, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-590: LZW tANS streaming decode publishes complete frames

- Date: 2026-08-05
- Status: accepted

Add the bounded known-size streaming decoder opposite DD-589. Collect and
parse the canonical 80-byte prefix, then collect one 56-byte frame header.
Before accepting its body, derive and validate the packed-code ceiling, tANS
block count and extents, exact serialized-frame size, raw staging, tANS views,
LZW phrase records, and aggregate internal storage.

Decode only after the entire declared frame is present. Reuse the private
complete-frame decoder, drain only the fully validated raw frame, and do not
collect a later header until draining finishes. Retain `EndInput` during the
drain and reject truncation, trailing bytes, `ResetBlock`, and unknown flags.
Corruption in a later frame may leave earlier committed frames visible but
must publish none of the malformed frame. This step adds no profile calculator,
C ABI, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-591: LZW tANS profiles separate bytes from aligned typed views

- Date: 2026-08-05
- Status: accepted

Add direction-specific bounded workspace calculators above DD-589 and DD-590.
Encoding derives the largest raw frame `F`, conservative packed LZW staging
`S = ceil(FW/8)`, `K = ceil(S/B)` tANS blocks, exact `528K` descriptor bytes,
the blockwise `2 + ceil(12n/8)` payload ceiling, complete-frame storage, and
the exact LZW encoder-record count. Count every simultaneously live encoder
region with checked arithmetic.

Decoding derives encoded-frame, packed, private-raw, tANS-view, and conservative
LZW phrase regions only from validated local limits. Place tANS views first in
one opaque aligned region, align upward for phrases, and publish typed spans
only after checking requirements, capacity, and base alignment. Empty encoding
has zero regions and alignment one. Prove that the returned extents directly
construct the existing bounded streaming round trip. This step adds no C ABI,
public factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-592: LZW tANS C ABI keeps all typed storage opaque

- Date: 2026-08-05
- Status: accepted

Expose the DD-591 profile through a versioned, size-tagged C configuration,
requirements query, and factory. Reuse the established three-region transform
contract: primary holds raw input or encoded-frame collection; secondary holds
packed LZW staging followed by encoded-frame or private-raw storage; aligned
views hold encoder records or tANS views followed by LZW phrases. No private
C++ record type or offset enters the public structure.

The requirements query must be repeated after changing direction, sizes,
maximum code width, block count, or any hard limit. The factory revalidates the
query, capacity, and alignment, publishes no handle on failure, and allocates
only the small transform implementation with non-throwing allocation. Prove a
pure-C11 `ABABX` round trip and reject each one-byte-short region, misalignment,
null handle output, and nonzero reserved metadata. This step adds no CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-593: LZW tANS completion is proven only through the public ABI

- Date: 2026-08-05
- Status: accepted

Audit DD-592 as the sole construction boundary with 64-byte raw frames and
tANS blocks. Cover empty input, every one-byte value, all byte values in
sequence, long zero runs, a repeating binary pattern, deterministic generated
data, and lengths 63, 64, and 65. Encode every case twice and require identical
archives and exact round trips.

For 193 generated bytes, require the unchunked archive to match `(1,1)`,
`(7,5)`, and `(13,17)` encode schedules and decode each schedule exactly.
Corrupt the fourth frame sequence, truncate its final byte, and append trailing
data independently. Each failure may publish the first 192 verified bytes but
must preserve the final sentinel and repeat the same sticky terminal status and
error positions. This step adds no CLI, benchmark, fuzz target, completion of
the whole profile, or interoperability entry.

## DD-594: LZW tANS fuzzing crosses private and public decoders

- Date: 2026-08-05
- Status: accepted

Add one bounded decoder fuzz target that presents every input to both the
complete-frame decoder-visible boundary and DD-592's public C streaming
decoder. Cap input at 8 KiB, raw publication at 4 KiB, a frame at 1 KiB,
packed staging at 4 KiB, tANS views at eight, phrases from the conservative
packed-code count, and every storage array at compile time. No fuzz-controlled
extent may allocate memory.

Drive the public decoder with deterministic variable chunks and at most input
bytes plus output bytes plus 32 calls. Abort only on process-accounting,
progress, workspace, or call-budget invariant violations; ordinary malformed
input is a successful iteration. Retain regressions for every strict prefix of
canonical `ABABX`, saturated generic-frame lengths, and an invalid tANS
frequency descriptor. Each must publish nothing and retain a stable repeated
error. This step adds no CLI, benchmark, whole-profile completion claim, or
interoperability entry.

## DD-595: LZW tANS CLI uses only the public transactional lifecycle

- Date: 2026-08-05
- Status: accepted

Add the explicit `lzw-tans` selector to the transactional CLI. Fix raw frames
and tANS blocks at 65,536 bytes, maximum LZW code width at 16, packed staging
at `S <= 131072`, block count at `K <= 2`, complete entropy payload at
`P <= 196612`, generated dictionary entries at 65,280, and aggregate internal
storage at 8 MiB. These are public policy limits, not new serialized fields.

Initialize, query, construct, process, and destroy the transform only through
DD-592's public C ABI. Do not reproduce the private LZW record, phrase, or tANS
view partition in the command-line layer. Retain the common temporary-output
transaction: existing destinations are rejected, malformed, truncated, and
extended input publishes no destination, and a successful close atomically
renames the temporary file. This step adds no benchmark or interoperability
entry.

## DD-596: LZW tANS benchmark verifies before measuring

- Date: 2026-08-05
- Status: accepted

Add `lzw-tans` to the dependency-free benchmark runner using DD-595's exact
public profile. Construct both directions only through DD-592's C
configuration, requirements query, factory, process, and destroy lifecycle.
Before any timing, require one byte-exact encode/decode round trip.

For input extent `N` and nonempty frame count `K`, reserve the checked complete
stream ceiling `80 + 3N + 1116K`: `3N` bounds tANS coding of at most `2N`
packed LZW bytes, while each frame adds one 56-byte header, two 528-byte
descriptors, and two two-byte initial states. Report all three queried regions
for each direction and their peak sum. Add a one-iteration smoke test over the
repository README, but impose no throughput or compression-ratio threshold.
This step adds no format variant or interoperability entry.

## DD-597: Interoperability schema 29 appends LZW tANS

- Date: 2026-08-05
- Status: accepted

Freeze the exact thirty-nine-entry schema-28 order and append `lzw-tans` once
as entry 40. Name the new codec set `marc-cli-v29`; retain the deterministic
8,193-byte binary fixture, full source revision, platform/compiler metadata,
and SHA-256 for the CLI, input, and every archive.

Generation must decode and compare every archive before recording it. The
verifier requires exact order, count, leaf-only names, sizes, hashes, foreign
decode, and byte-identical local re-encoding. The compatibility regression
rejects a reordered schema-29 manifest, derives schema 28 by removing only
`lzw-tans`, and verifies the unchanged schemas 28 through 1. This admission
changes no codec representation. External cross-platform evidence remains a
post-push release check.

## DD-598: LZD tANS preserves finalized reference pairs

- Date: 2026-08-05
- Status: accepted

Reserve `lzd-tans` for LZD variant 1 followed by tabled tANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZD parameter extension,
empty entropy parameters, and canonical eight-byte little-endian reference
pairs. Complete the token byte stream before entropy processing. A tANS block
may split a four-byte reference or eight-byte token but cannot split a byte or
cross an outer frame. Reset both layers at every frame.

For nonempty raw frame extent `F`, require actual token extent
`0 < S <= 8 * ceil(F/2)` with `S mod 8 = 0`, `K = ceil(S/B)` for nonzero tANS
block size `B`, exact descriptor extent `528K`, and the checked sum of
per-block `2 + ceil(12n/8)` payload ceilings. Bound phrase records by the
lesser of `floor(F/2)` and the configured entry limit, expansion references by
that phrase count plus one, and raw frames by 2^20 bytes.

Decoding must validate every tANS descriptor, table, transition, initial
state, padding, and exact payload exhaustion before reconstructing exactly `S`
private token bytes. Only then validate alignment, references, terminal
absence, checked phrase lengths, dictionary growth, and exact raw extent.

For raw `A`, independently freeze LZD token bytes
`41 00 00 00 FF FF FF FF`. Their normalized tANS model is
`00:1536, 41:512, FF:2048`, payload is `08 03 9B 00` with three final valid
bits, and the complete frame is 588 bytes. Prove this only with the standalone
LZD encoder, tANS encoder, and generic serializers. This decision publishes no
combined validator, streaming transform, C factory, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-599: LZD tANS validation is entropy-first and discard-only

- Date: 2026-08-05
- Status: accepted

Add the first internal `lzd-tans` complete-frame component as a bounded
validator only. Admit the exact generic frame extent, aligned nonempty token
extent, derived block count, exact `528K` descriptor bytes, checked tANS
payload ceiling, caller-owned block views, complete token staging, LZD phrase
records, and their aggregate bytes before entropy output can begin.

Parse all descriptors and strictly validate every tANS table, transition,
initial state, final padding, and payload exhaustion before decoding any block
into token staging. Then reconstruct exactly the declared token extent and
apply the existing LZD validator to the complete private span. Preserve stable
entropy block and LZD token positions. On any error, every workspace is
discard-only and no raw output exists at this boundary.

Prove the independent 588-byte raw-`A` frame, blocks that split both reference
fields and tokens, later-descriptor failure before token mutation, invalid LZD
references after entropy reconstruction, each short workspace, aggregate
workspace one byte short, truncation, trailing bytes, and wrong-pipeline
rejection under MSVC and ClangCL. This decision adds no raw decoder, encoder,
streaming transform, C factory, CLI, benchmark, fuzz target, completion claim,
or interoperability entry.

## DD-600: LZD tANS reconstruction remains private

- Date: 2026-08-05
- Status: accepted

Add a bounded complete-frame decoder that retains DD-599's exact validation
order and reconstructs only into caller-owned private raw staging. Before any
entropy output, require the complete declared raw capacity and the conservative
iterative expansion stack derived from admitted phrase capacity. Count both
regions with descriptors, payload, token staging, tANS views, and phrase
records against `max_internal_buffered_bytes` using checked arithmetic.

After all tANS blocks and the complete LZD graph validate, invoke the existing
allocation-free, non-recursive LZD decoder over only those admitted spans.
Propagate its stable token position, format, validation, and decode errors.
All views, tokens, phrase records, expansion references, and raw bytes are
discard-only after any failure; no caller-visible output span exists.

Prove the independent raw-`A` frame and a phrase-bearing `ABABAB` frame whose
tANS blocks split reference and token boundaries. Reject raw and expansion
storage one entry short before token mutation, count both regions in an
aggregate limit one byte short, and preserve raw sentinels after entropy or LZD
failure. This decision adds no transactional publication, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-601: LZD tANS frame publication is transactional

- Date: 2026-08-06
- Status: accepted

Add an internal complete-frame decoder with a distinct caller-visible output
span. Retain DD-599 and DD-600's admission and validation order and require
capacity for the complete declared raw extent before entropy views, token
staging, phrase records, expansion references, or private raw staging can
change. Output capacity is not internal workspace and does not alter the
aggregate allocation bound.

After strict tANS validation, complete LZD graph validation, and successful
non-recursive reconstruction into private raw staging, copy exactly `raw_size`
bytes to caller output once. Preserve bytes beyond that extent. A short output
or any header, descriptor, payload, reference, phrase, limit, or reconstruction
failure leaves caller output completely unchanged.

Prove transactional publication for the independent raw-`A` vector and the
phrase-bearing `ABABAB` frame, exact preservation beyond a short logical
extent, one-byte-short output before private mutation, and unchanged complete
output after entropy or LZD failure. This decision adds no encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-602: LZD tANS planning freezes reference bytes

- Date: 2026-08-06
- Status: accepted

Add an internal exact-frame planner for the inverse of DD-599 through DD-601.
Validate the exact stream profile, LZD parameters, nonempty input extent, and
frame-local bounds. Determine and require the bounded LZD encoder-record
capacity before token staging can change. Plan the deterministic parse, require
the exact checked aligned token extent, then serialize all canonical eight-byte
reference pairs into caller-owned staging.

Plan tabled tANS only over that frozen token span. Require the derived block
count within limits, exact `528K` descriptors, the exact sum of planned payload
sizes within the blockwise `2 + ceil(12n/8)` ceiling, and 32-bit generic frame
fields. Count encoder records, token staging, descriptors, and exact payload
against `max_internal_buffered_bytes`, validate the synthesized generic header,
and return the checked complete frame extent without a serialized output span.

Prove the exact raw-`A` token, 528-byte descriptor extent, four-byte payload,
and 588-byte frame; repeat a phrase-bearing multi-block plan byte-identically;
reject encoder records and token staging one entry short without staging
mutation; and reject aggregate workspace one byte short, empty input, and a
frame-size mismatch. This decision adds no serialized encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-603: LZD tANS encoding is plan-first and deterministic

- Date: 2026-08-06
- Status: accepted

Add the deterministic complete-frame encoder above DD-602. Invoke the exact
planner first so canonical LZD tokens, every tANS block model and payload
extent, generic frame fields, and aggregate workspace are fixed before a
serialized destination is considered. Require capacity for the complete
planned extent before writing any byte.

Serialize the generic frame header explicitly. For each frozen token subspan,
repeat tANS planning, require the payload extent to remain within the exact
DD-602 sum, serialize the 528-byte descriptor, and encode only its planned
payload region. Require final token and payload offsets to match the plan. Any
unexpected post-admission mismatch is an internal error; ordinary planner and
capacity failures leave the complete destination unchanged.

Prove byte-exact reproduction of the independent 588-byte raw-`A` frame;
encode a phrase-bearing four-block input twice and require byte identity plus
transactional round trip; and preserve every output byte after a one-byte-short
destination or planner failure. This decision adds no streaming transform,
C factory, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-604: LZD tANS streaming encoding drains complete frames

- Date: 2026-08-06
- Status: accepted

Add a bounded immutable-direction streaming encoder above DD-602 and DD-603.
Serialize the ordinary stream header and 16-byte LZD parameters into a fixed
prefix. Keep raw-frame input, canonical token staging, complete encoded-frame
storage, and aligned LZD encoder records caller-owned. Validate their largest
configured extents at construction and count the active frame, exact token
span, exact serialized frame, and encoder records against the aggregate limit
before frame preparation.

Collect exactly one outer raw frame, invoke the exact planner and deterministic
encoder, then drain only the completed immutable frame. Input consumption and
output production remain independent. Retain `EndInput` across prefix and
frame draining without requiring the flag again. A nonterminal `Flush` does not
close a partial frame; reject `ResetBlock`, unknown flags, premature finish,
and excess input with stable sticky errors. Empty known-size input emits only
the prefix and ends after finish.

Prove byte identity with independently concatenated complete frames using
one-byte input and output; retained finish while all regions drain; nonterminal
flush without shortened framing; every caller-owned region short; aggregate
workspace one byte short; empty input; protocol errors; repeated end state; and
sticky failure. This decision adds no streaming decoder, profile calculator,
C factory, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-605: LZD tANS streaming decoding publishes complete frames

- Date: 2026-08-06
- Status: accepted

Add a bounded immutable-direction streaming decoder above DD-600/DD-601. Read
the ordinary 80-byte prefix and each 56-byte frame header incrementally. Admit
the declared complete serialized frame, tANS block views, canonical LZD token
staging, private raw staging, LZD phrase records, and expansion stack before
collecting its body. Count all active extents against the aggregate buffered-
byte limit with checked arithmetic.

After the complete frame body arrives, validate every tANS descriptor and
payload, decode the complete private token region, validate the LZD phrase
graph, and reconstruct the declared raw extent into private staging. Publish
raw bytes only after every layer succeeds. Arbitrary output chunking therefore
changes only delivery. A later malformed frame may not modify output beyond
earlier committed frames.

Retain `EndInput` while validated raw bytes drain. Reject truncated prefix,
header, or frame body; trailing bytes after the declared original size;
`ResetBlock`; and unknown flags with stable sticky errors. Nonterminal `Flush`
does not change parsing. Empty known-size input accepts exactly the prefix and
ends after finish. Prove one-byte input/output, transactional later-frame
corruption, each short workspace, aggregate limit one byte short, truncation,
trailing data, reset, unknown flags, empty input, flush starvation, premature
finish, repeated end, and sticky failure. This decision adds no public profile,
C factory, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-606: LZD tANS profile computes coupled worst-case storage

- Date: 2026-08-06
- Status: accepted

Publish one internal profile calculator for constructing the DD-604/DD-605
streaming transforms without duplicating workspace arithmetic at later public
boundaries. Freeze the largest active raw frame to
`min(original_size, frame_size)`. Bound its canonical LZD token extent by
`ceil(raw_bytes / 2) * 8`, its encoder records by
`min(floor(raw_bytes / 2), maximum_entries)`, and its tANS block count by the
configured byte block size. Bound each tANS payload by
`ceil(12 * block_symbols / 8) + 2` bytes and count every 528-byte descriptor.

The encoder aggregate is the simultaneous raw frame, canonical token staging,
complete encoded frame, and LZD encoder records. The decoder calculator derives
conservative encoded, token, raw, block-view, phrase, and expansion extents
from decoder limits. Pack typed views into one opaque byte allocation with
checked offsets and the maximum required alignment. Partitioners reject altered
requirements, short or misaligned storage, and arithmetic overflow before
forming a typed span.

Empty known-size streams require no frame workspace and use alignment one.
Map stable profile errors to the core error vocabulary and prove that calculated
requirements construct the actual streaming encoder and decoder for a complete
round trip. This decision adds no C ABI, public factory, CLI, benchmark, fuzz
target, completion claim, serialized variant, or interoperability entry.

## DD-607: LZD tANS C ABI preserves the three-region contract

- Date: 2026-08-06
- Status: accepted

Expose LZD plus tANS through a new fixed-size configuration structure and three
new C functions: configuration initialization, direction-specific workspace
query, and transform creation. Add symbols without changing existing structure
layouts or `MARC_ABI_VERSION`.

For encoding, primary storage holds one raw frame; secondary storage is
partitioned into canonical LZD token staging followed by one complete serialized
tANS frame; aligned opaque views hold LZD encoder entries. For decoding,
primary storage holds one serialized frame; secondary storage is partitioned
into private canonical token bytes followed by private raw bytes; aligned opaque
views hold tANS block views, LZD phrase entries, and the bounded expansion stack.

The workspace query delegates all formulas to DD-606. Creation repeats the
query, rejects null, short, or misaligned caller storage, re-partitions opaque
views through the checked local partitioner, constructs the immutable transform
with `std::nothrow`, and leaves the output handle null on every failure. A pure
C11 shared-library test must initialize defaults, query both directions,
round-trip binary input, and reject each short region, misalignment, a null
output handle, and non-zero reserved fields. This decision adds no CLI,
benchmark, fuzz target, completion claim, serialized variant, or
interoperability entry.

## DD-608: LZD tANS completion uses only the public ABI

- Date: 2026-08-06
- Status: accepted

Admit the DD-607 factory through the same public-only LZD completion schedule
used by the earlier entropy compositions. Keep 64-byte outer frames and
64-byte tANS blocks. Bound the largest 256-byte canonical token region by four
528-byte descriptors plus twelve bits per token byte and two final-state bytes
per entropy block.

Through only `marc_lzd_tans_*`, the common process function, and destruction,
round-trip empty input, every one-byte value, all byte values in sequence,
long zero repetition, a repeated binary pattern, deterministic generated data,
and lengths 63, 64, and 65. Re-encode a 193-byte four-frame stream under
one-byte, mixed prime-size, and whole-buffer input/output schedules and require
one exact byte stream. Repeated calls after successful completion remain ended.

Corrupt the final frame header, truncate its final byte, and append one trailing
byte independently. Each decode must publish exactly the first three complete
64-byte frames, leave the final output sentinel unchanged, and return the same
sticky error position on repetition. This decision adds no fuzz target, CLI,
benchmark, interoperability entry, or `Ready` claim.

## DD-609: LZD tANS fuzzing fixes both decoder boundaries

- Date: 2026-08-06
- Status: accepted

Drive the internal complete-frame validator and the DD-607 public incremental
decoder from each input. Cap input at 8,192 bytes, total output at 4,096 bytes,
one raw frame at 1,024 bytes, canonical LZD staging at 4,096 bytes, compressed
payload at 16,384 bytes, dictionary entries at 512, and tANS blocks at eight.
Fix every typed view, phrase, expansion reference, staging region, and output
array before inspecting input. Derive input and output chunks from input bytes,
validate every process-result bound and no-progress state, and impose a finite
call budget.

Persist the canonical `ABABX` stream as three atomic malformed families: every
proper truncation, all generic frame extent fields saturated to `ff`, and a
nonzero tANS descriptor flag. Each must publish no failing-frame byte, preserve
the caller sentinel, and repeat the same terminal error code and position. Seed
sanitizer runs only with repository-reviewed truncated magic. This decision
adds no CLI, benchmark, interoperability entry, format variant, or `Ready`
claim.

## DD-610: LZD tANS CLI uses only the public transactional lifecycle

- Date: 2026-08-06
- Status: accepted

Add `lzd-tans` as an explicit selector in the existing transactional CLI. Fix
outer frames and tANS blocks at 65,536 bytes. Admit the exact LZD ceiling of
262,144 canonical token bytes, at most four entropy blocks, 2,112 descriptor
bytes, and at most 393,224 payload bytes. Retain the public LZD maximum-entry
default and a conservative 16-MiB aggregate internal-buffer policy.

Initialize, query, create, process, and destroy only through the public
`marc_lzd_tans_*` lifecycle. Allocate the exact primary, secondary, and aligned
opaque-view regions returned for each immutable direction; do not reproduce
private tANS views, LZD encoder entries, phrases, expansion references, or
partition offsets. Preserve the common file adapter's overwrite refusal and
publish-on-success behavior. Prove binary and empty round trips plus atomic
rejection of malformed and trailing input without destination or `.tmp`
residue. This decision adds no benchmark, interoperability entry, format
variant, or `Ready` claim.

## DD-611: LZD tANS benchmark verifies before measuring

- Date: 2026-08-06
- Status: accepted

Add `lzd-tans` to the dependency-free benchmark runner using DD-610's exact
public profile. Construct both directions only through DD-607's C
configuration, requirements query, factory, process, and destroy lifecycle.
Before any timing, require one byte-exact encode/decode round trip.

For input extent `N` and nonempty frame count `K`, reserve the checked complete
stream ceiling `80 + 12*ceil(N/2) + 2176K`. Twelve bytes bound tANS coding of
each possible eight-byte LZD reference pair; each frame adds one 56-byte header
plus four 528-byte descriptors and four two-byte initial states. Report ratio,
encode and decode throughput, all three queried workspace regions for each
direction, and their larger sum. Add a one-iteration README smoke without a
performance or compression threshold. This decision adds no interoperability
entry, format variant, or `Ready` claim.

## DD-612: Interoperability schema 30 appends LZD tANS

- Date: 2026-08-06
- Status: accepted

Freeze the exact forty-entry schema-29 order and append `lzd-tans` once as
entry 41. Name the new codec set `marc-cli-v30`; retain the deterministic
8,193-byte binary fixture, full source revision, platform/compiler metadata,
and SHA-256 for the CLI, input, and every archive.

Generation must decode and compare every archive before recording it. The
verifier requires exact order, count, leaf-only names, sizes, hashes, foreign
decode, and byte-identical local re-encoding. The compatibility regression
rejects a reordered schema-30 manifest, derives schema 29 by removing only
`lzd-tans`, and verifies the unchanged schemas 29 through 1. This admission
changes no codec representation. External cross-platform evidence remains a
post-push release check.

That release check completed at revision
`827ddf085efb40c7d8f9bc27628977053179d84c`: the Windows/MSVC and Ubuntu
24.04/Ninja artifacts verified on Ubuntu 26.04/Clang, and the Ubuntu 26.04
bundle verified locally and on Windows/MSVC. Every pass decoded and
byte-identically re-encoded all 41 archives.

## DD-613: LZMW tANS preserves finalized phrase references

- Date: 2026-08-06
- Status: accepted

Reserve `lzmw-tans` for LZMW variant 1 followed by tabled tANS variant 1 under
format version 1.0. Preserve the standalone 16-byte LZMW parameter extension,
empty entropy parameters, and canonical four-byte little-endian phrase
references. Complete the reference byte stream before entropy processing. A
tANS block may split a reference but cannot split a byte or cross an outer
frame. Reset both layers at every frame.

For nonempty raw frame extent `F`, require actual reference extent
`0 < S <= 4F` with `S mod 4 = 0`, `K = ceil(S/B)` for nonzero tANS block size
`B`, exact descriptor extent `528K`, and the checked sum of per-block
`2 + ceil(12n/8)` payload ceilings. Bound generated phrase records by the
lesser of `max(F - 1, 0)` and the configured entry limit, expansion references
by that phrase count plus one, and raw frames by 2^20 bytes.

Decoding must validate generic extents and every tANS descriptor, table,
transition, initial state, final padding, and payload exhaustion before
reconstructing exactly `S` private reference bytes. Only then validate
four-byte alignment, literal or previously generated references, checked
adjacent-phrase growth, dictionary limits, and exact raw extent before any raw
reconstruction or publication.

For raw `A`, independently freeze LZMW reference bytes `41 00 00 00`. Their
normalized tANS model is `00:3072, 41:1024`, payload is `FB 02 07` with three
final valid bits, and the complete frame is 587 bytes. Prove this only with the
standalone LZMW encoder, tANS encoder, and generic serializers. This decision
publishes no combined validator, streaming transform, C factory, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-614: LZMW tANS validation is entropy-first and discard-only

- Date: 2026-08-06
- Status: accepted

Add the first internal `lzmw-tans` complete-frame component as a bounded
validator only. Admit the exact generic frame extent, aligned nonempty
reference extent, derived block count, exact `528K` descriptor bytes, checked
tANS payload ceiling, caller-owned block views, complete reference staging,
LZMW phrase records, and their aggregate bytes before entropy output can begin.

Parse all descriptors and strictly validate every tANS table, transition,
initial state, final padding, and payload exhaustion before decoding any block
into reference staging. Then reconstruct exactly the declared reference extent
and apply the existing LZMW validator to the complete private span. Preserve
stable entropy block and LZMW token positions. On any error, every workspace is
discard-only and no raw output exists at this boundary.

Prove the independent 587-byte raw-`A` frame, blocks that split references,
later-descriptor failure before reference mutation, invalid LZMW references
after entropy reconstruction, each short workspace, aggregate workspace one
byte short, truncation, trailing bytes, and wrong-pipeline rejection under
MSVC and ClangCL. This decision adds no raw decoder, encoder, streaming
transform, C factory, CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-615: LZMW tANS reconstruction and publication are transactional

- Date: 2026-08-06
- Status: accepted

Extend DD-614 with a bounded private raw decoder. Before descriptor parsing or
entropy output, require the complete declared raw staging extent and the
conservative iterative LZMW expansion stack derived from phrase capacity.
Count both with descriptors, payload, reference staging, block views, and
phrase records against `max_internal_buffered_bytes`.

After every tANS block validates, the complete reference region is rebuilt,
and the full LZMW graph validates, reduce the active expansion span to the
actual generated-entry count plus one for a nonempty frame. Invoke only the
existing allocation-free, nonrecursive LZMW decoder into disposable raw
staging. Propagate its stable token, format, validation, and decode details.

The transactional form additionally admits the entire caller destination
before any private mutation and copies exactly the declared raw extent once,
only after reconstruction succeeds. Prove the independent literal vector,
phrase expansion across entropy-block and phrase edges, one-entry-short raw
and expansion storage before entropy mutation, malformed entropy preserving
raw staging, successful one-time publication, and short output preserving all
caller regions. This decision adds no encoder, streaming transform, C factory,
CLI, benchmark, fuzz target, completion claim, or interoperability entry.

## DD-616: LZMW tANS planning freezes the reference stream

- Date: 2026-08-07
- Status: accepted

Add a write-free exact-frame planner as the inverse of DD-614 and DD-615.
Validate the exact stream profile, LZMW parameters, nonempty input extent, and
frame-local limits. Determine and require the bounded LZMW encoder-record
capacity before reference staging can change. Plan the deterministic LZMW
parse, require the exact checked `0 < S <= 4F` aligned reference extent and
staging capacity, then serialize all canonical four-byte references.

Only after the complete reference span is fixed may the planner divide it by
the configured tANS block size. Plan every block independently, sum exact
payload extents with checked arithmetic, require exact `528K` descriptors and
the DD-613 payload ceiling, and count encoder records, reference staging,
descriptors, and exact payload against `max_internal_buffered_bytes`.
Validate the synthesized generic frame header with sequence and committed-
output context and report the exact complete-frame extent without writing it.

Prove the independent raw-`A` reference and exact 587-byte extent, repeated
byte-identical phrase/block planning, encoder records and reference staging one
entry short before staging mutation, aggregate workspace one byte short,
empty input, and frame-size mismatch. This decision adds no serialized frame
encoder, streaming transform, C factory, CLI, benchmark, fuzz target,
completion claim, or interoperability entry.

## DD-617: LZMW tANS frame encoding is plan-first and deterministic

- Date: 2026-08-07
- Status: accepted

Add the deterministic complete-frame encoder above DD-616. Invoke the exact
planner first so canonical LZMW references, every tANS model and payload size,
generic frame fields, aggregate workspace, and complete serialized extent are
fixed before destination capacity is considered. Require capacity for the
complete frame before writing any serialized byte.

Serialize the generic header explicitly, then repeat tANS planning over each
block of the frozen reference span. Require every repeated plan and cumulative
payload extent to match DD-616. Serialize each 528-byte descriptor and encode
its exact payload into the preplanned region. Treat any post-plan divergence as
an internal invariant failure.

Prove byte-for-byte equality with the independent 587-byte raw-`A` frame,
deterministic repeated encoding and round trip for a phrase stream whose tANS
blocks split references, complete output preservation with capacity one byte
short, and complete output preservation after planner rejection. This decision
adds no streaming transform, C factory, CLI, benchmark, fuzz target, completion
claim, or interoperability entry.

## DD-618: LZMW tANS streaming encoding drains immutable frames

- Date: 2026-08-07
- Status: accepted

Add a bounded internal streaming encoder above DD-616 and DD-617. Require
caller-owned storage for the largest outer raw frame, its `4F` canonical LZMW
reference ceiling, one complete encoded frame, and the bounded LZMW encoder
table. Validate and serialize the ordinary stream header plus 16-byte LZMW
parameters during construction without allocation.

Drain the immutable prefix first. Collect exactly one outer frame, invoke the
exact planner and deterministic encoder into private serialized staging, commit
its input extent and sequence only after complete success, then drain that
immutable frame under arbitrary output capacities. Count raw, reference,
serialized, and encoder-table bytes together against
`max_internal_buffered_bytes` before publishing the frame.

Retain `EndInput` while prefix or frame output drains. Nonterminal `Flush` must
not close a partial frame. Reject `ResetBlock`, unknown flags, premature final
input, excess input, short construction storage, and aggregate workspace
overflow with stable terminal errors. Prove exact reference equality with
one-byte input/output, flush invariance, sticky finish, empty streams, and all
workspace and protocol failures. This decision adds no streaming decoder, C
factory, CLI, benchmark, fuzz target, completion claim, or interoperability
entry.

## DD-619: LZMW tANS streaming decoding commits complete frames only

- Date: 2026-08-07
- Status: accepted

Add the bounded internal streaming decoder paired with DD-618. Incrementally
collect and validate the fixed stream prefix, then one generic frame header.
Before accepting its body, derive and admit the complete serialized extent,
tANS block views, canonical reference staging, LZMW phrase records, iterative
expansion stack, private raw staging, and their aggregate live bytes.

Collect exactly one complete frame and invoke DD-615's private transactional
decoder. Enter raw-output draining only after every tANS block, the full LZMW
graph, and iterative reconstruction succeed. Drain the immutable private raw
frame under arbitrary output capacity. Commit sequence and output extent per
successful frame; a later malformed frame may not expose any of its raw bytes
or undo earlier frames.

Retain final-input intent while raw output drains. Reject truncation of prefix,
header, or body, trailing serialized bytes, invalid tables and references,
short caller-owned regions, aggregate overflow, `ResetBlock`, and unknown
flags with a sticky terminal error. Prove one-byte input/output, later-frame
corruption atomicity, every workspace class, aggregate limit, empty stream, and
nonterminal `Flush`. This decision adds no C factory, CLI, benchmark, fuzz
target, completion claim, or interoperability entry.

## DD-620: LZMW tANS profile couples conservative storage

- Date: 2026-08-07
- Status: accepted

Add an internal direction-specific profile calculator above DD-618 and DD-619.
For known-size encoding derive the canonical LZMW/tANS stream header, largest
raw frame `F`, reference ceiling `4F`, `K = ceil(4F/B)` tANS blocks, exact
`528K` descriptor bytes, blockwise `2 + ceil(12n/8)` payload ceilings, at most
`min(F - 1, maximum_entries)` encoder records, and the checked aggregate live
workspace. Empty input requires no active-frame storage.

For decoding derive conservative encoded-frame, reference, private-raw, block-
view, phrase, and expansion capacities only from validated local limits.
Partition the opaque typed region only after recomputing and matching every
offset, total byte count, and alignment. Map profile failures to stable core
errors and prove the returned regions by constructing the existing streaming
pair. This changes no serialized representation and adds no C factory, CLI,
benchmark, fuzz target, completion claim, or interoperability entry.

## DD-621: LZMW tANS C factory keeps typed layouts opaque

- Date: 2026-08-08
- Status: accepted

Expose a size-tagged `marc_lzmw_tans_config` with fixed-width fields matching
DD-620's known-size configuration and local decoder limits. Provide one
requirements query and one immutable-direction factory through the common
opaque transform lifecycle. Encoding maps primary storage to raw collection,
secondary storage to canonical references followed by the complete frame, and
aligned views storage to LZMW encoder entries. Decoding maps primary storage to
encoded-frame collection, secondary storage to canonical references followed
by private raw output, and aligned views storage to tANS block views, LZMW
phrases, and expansion indices.

The query is the sole authority for byte counts and alignment. The factory must
reject wrong structure size or ABI version, nonzero reserved fields, invalid
direction or limits, null-with-size buffers, short or misaligned regions, and a
null result pointer before constructing anything. Prove the declarations from
a pure C11 translation unit with round trip and negative workspace cases. This
changes no format and adds no CLI, benchmark, fuzz target, completion claim, or
interoperability entry.

## DD-622: LZMW tANS public completion reuses equivalent bounds

- Date: 2026-08-08
- Status: accepted

Apply the reviewed public-ABI completion matrix through only the DD-621 symbol
family. Fix raw frames and tANS blocks to 64 bytes. At that frame size LZMW's
`4F` reference ceiling and LZD's `8 * ceil(F/2)` ceiling are both exactly 256
bytes, so the existing tANS capacity, data, chunking, terminal, and malformed
schedules can be reused without weakening or approximating a bound.

Prove empty input, every one-byte value, all byte values, repetitive and
patterned inputs, deterministic generated data, and lengths 63, 64, and 65.
Require repeated encoding and `(1,1)`, `(7,5)`, and `(13,17)` schedules to
produce identical streams and raw output. Corrupt the fourth frame sequence,
truncate its final byte, and append trailing data independently; each failure
must commit exactly the first three frames, preserve the final output sentinel,
and repeat the same sticky status and positions. This adds no format, CLI,
benchmark, fuzz target, `Ready` claim, or interoperability entry.

## DD-623: LZMW tANS fuzzing is bounded before parsing

- Date: 2026-08-08
- Status: accepted

Add a dual-path decoder fuzz entry above DD-615, DD-619, and DD-621. Cap the
supplied input at 8,192 bytes. Exercise the complete-frame private decoder only
after the ordinary prefix and LZMW parameter parsers accept the fixed profile.
Exercise the public incremental decoder with input-derived chunks, at most
4,096 published raw bytes, and a call ceiling of bounded input plus bounded
output plus 32. Allocate all encoded, canonical-reference, raw, tANS-view,
phrase, and expansion regions as fixed arrays before parsing.

Abort the harness only for violated API invariants, impossible queried extents,
construction failure under the fixed valid configuration, progress without
counts, renewed input demand after final input, or exhaustion of the call
ceiling. Treat ordinary malformed-stream status as expected. Add deterministic
regressions for every truncation of a canonical stream, saturated generic frame
lengths, and invalid tANS descriptor metadata; each must publish no raw byte and
remain sticky. This changes no format, API, CLI, benchmark, `Ready` claim, or
interoperability entry.

## DD-624: LZMW tANS CLI is a public-only transaction

- Date: 2026-08-08
- Status: accepted

Add `lzmw-tans` as an explicit selector in the existing transactional CLI.
Fix 65,536-byte raw frames and entropy blocks, `S = 4F = 262,144` canonical
reference bytes, four tANS blocks, `528K = 2,112` descriptor bytes,
`P = 12S/8 + 2K = 393,224` payload bytes, at most 65,536 generated entries,
and a conservative 16-MiB aggregate policy.

Initialize configuration, query all direction-specific byte extents and
alignment, create the transform, process the file, and destroy the transform
only through DD-621's C lifecycle. Do not reproduce private tANS-view,
encoder-entry, phrase, expansion, or partition layouts. Retain destination
overwrite refusal, strict trailing-data rejection, bounded 64-KiB I/O, sibling
`.tmp` staging, deletion on failure, and rename only after complete success.
Prove binary and empty round trips plus atomic malformed and trailing rejection.
This changes no default selector, format, benchmark, `Ready` claim, or
interoperability entry.

## DD-625: LZMW tANS benchmark verifies before measuring

- Date: 2026-08-08
- Status: accepted

Add `lzmw-tans` to the dependency-free benchmark runner using DD-624's exact
65,536-byte frame/block profile and only DD-621's public C lifecycle. Query,
allocate, construct, process, and destroy encode and decode directions
independently. Require one byte-exact untimed round trip before any result is
reported, then measure encode and decode separately and report all three
queried workspace regions, their alignments, and the larger directional sum.

For input extent `N` and nonempty outer-frame count `K`, admit output with the
checked ceiling `80 + 6N + 2176K`. The payload term follows `S <= 4N` and the
tANS ceiling `ceil(12S/8) + 2` per block. The per-frame term is one 56-byte
generic header, four 528-byte descriptors, and four two-byte final states.
This benchmark changes no default selector, stream representation, API,
`Ready` claim, or interoperability schema.

## DD-626: Interoperability schema 31 appends LZMW tANS

- Date: 2026-08-08
- Status: accepted

Freeze the exact forty-one-entry schema-30 order and append `lzmw-tans` once
as entry 42. Name the new codec set `marc-cli-v31`; retain the deterministic
8,193-byte binary fixture, full source revision, platform/compiler metadata,
and SHA-256 for the CLI, input, and every archive.

Generation must decode and compare every archive before recording it. The
verifier requires exact order, count, leaf-only names, sizes, hashes, foreign
decode, and byte-identical local re-encoding. The compatibility regression
rejects a reordered schema-31 manifest, derives schema 30 by removing only
`lzmw-tans`, and verifies the unchanged schemas 30 through 1. This admission
changes no codec representation. External cross-platform evidence remains a
post-push release check.

That release check completed at revision
`903181080556c3bb511ad4a2e5275837ebda48e7`: the Windows/MSVC and Ubuntu
24.04/Ninja artifacts verified on Ubuntu 26.04/Clang, and the Ubuntu 26.04
bundle verified locally and on Windows/MSVC. Every pass decoded and
byte-identically re-encoded all 42 archives.

## DD-627: LZSS is the first typed-token experiment

- Date: 2026-08-08
- Status: accepted

Develop the optional typed-event architecture first with LZSS. Keep all
format-version-1 profiles unchanged and assign dictionary variant 2 to the new
value boundary. Retain LZSS variant 1's greedy longest match, nearest-distance
tie break, Literal and Match semantics, and frame reset, while narrowing the
experimental parameter range to minimum length 5, maximum length 258, and
window 65,536. Typed events are internal values, never native serialization or
public C structs. Define a canonical diagnostic transcript separately from the
actual entropy input.

## DD-628: LzssFieldContext variant 1 is invertible from token history

- Date: 2026-08-08
- Status: accepted

Place an explicit context-model state machine between typed LZSS tokens and
entropy arithmetic. Context selection may depend only on already accepted
token state: previous token kind, the high nibble of the most recent Literal,
and the current Match length class. Split token kind, literal value, length
class, and distance class into fixed context IDs and encode numeric remainders
as LSB-first bypass bits. Reset all context state at the outer frame. Record
token, modeled-event, and entropy-decision counts independently and reject any
mismatch before raw publication.

## DD-629: Entropy backends consume bounded modeled-event frames

- Date: 2026-08-08
- Status: accepted

Make entropy substitution independent of the dictionary and context layers.
The encoder first creates one bounded immutable modeled-event frame, performs a
write-free exact plan, and then encodes that plan atomically. The decoder is
driven by the context model's expected operation kind, context ID, alphabet, or
bypass width; serialized input cannot select allocation or a different model.
The first backend is Dynamic Range variant 2, reusing variant-1 arithmetic with
31 independent adaptive context tables and fixed equiprobable bypass bits.
Every later backend with different bytes receives its own entropy variant.

## DD-630: Typed context streams use isolated format version 2.0

- Date: 2026-08-08
- Status: accepted

Reserve profile `lzss-field-context-dynamic-range` under format 2.0,
dictionary ID/variant 2/2, context ID/variant 1/1, and entropy ID/variant 3/2.
Retain the 64-byte stream-prefix field positions but require feature bit 0,
sixteen-byte dictionary and entropy parameters, and a sixteen-byte context
extension. Use a new 64-byte `MRF2` frame header carrying raw, token, event,
decision, descriptor, and payload extents. Validate and reconstruct the entire
frame in bounded private staging before publication. This reservation adds no
public factory, CLI selector, or interoperability entry.

## DD-631: Format 2 preflight is complete-frame and transactional

- Date: 2026-08-08
- Status: accepted

Keep Format 2 parsing private and isolated from the version-1 `StreamHeader`
types. Parse the 64-byte prefix and all three fixed parameter/extension regions
as one 112-byte transaction. For every nonempty frame, validate `MRF2`, counts,
local limits, the fixed contextual table extent, and the available descriptor
before accepting the complete payload extent. Do not allocate token, event, or
entropy state and do not publish parsed output until the declared frame is
fully present. Return the exact accepted frame extent so following-frame bytes
remain untouched.

## DD-632: Typed LZSS validation precedes reconstruction

- Date: 2026-08-08
- Status: accepted

Represent Format 2 LZSS tokens as private value records, not serialized or ABI
objects. Validate a caller-owned complete token span before reconstruction or
context conversion. Require exact declared token and raw counts, zero unused
fields, known kind, variant-2 parameter bounds, references within the already
validated frame history, checked output growth, aggregate-output policy, and a
bounded local storage extent. Preserve the first failing token index and raw
prefix for diagnostics, but publish no raw bytes from validation.

## DD-633: Typed LZSS reconstruction writes only after complete gating

- Date: 2026-08-08
- Status: accepted

Reconstruct typed LZSS tokens only into private caller-owned raw staging.
Before the first write, require complete token-frame validation, sufficient
output capacity, supported output extent, and disjoint token/output storage.
After those gates, perform Literal stores and Match copies byte by byte so
overlap semantics are exact and no input-dependent failure remains. Write only
the declared raw extent and leave excess staging capacity untouched. Public
downstream publication remains a later complete-decoder transaction.

## DD-634: LZSS field-context inversion validates before materialization

- Date: 2026-08-08
- Status: accepted

Invert a complete modeled-operation frame in two passes. The validation pass
derives operation shape exclusively from prior accepted token state, checks
exact declared event, token, decision, and raw counts, validates reconstructed
typed tokens incrementally, and enforces local and aggregate limits without
writing caller output. Only after sufficient disjoint token staging is proven
does a failure-free materialization pass write the declared token extent.
Preserve stable failing operation/token indices and accepted prefixes for
diagnostics; do not expose this private value boundary through the C ABI.

## DD-635: LZSS field-context forward modeling uses an exact plan

- Date: 2026-08-08
- Status: accepted

Validate a complete typed LZSS frame before calculating its modeled-operation
plan. Count each symbol as one entropy decision and each nonzero-width bypass
operation by its exact bit width; omit zero-width bypass operations. Enforce
the planned operation-storage extent, output capacity, and token/output
non-aliasing before the first write. Materialize the fixed state machine in a
failure-free pass, write only the planned extent, and require exact inversion
to the original typed values in tests. Keep the boundary private and allocation
free.

## DD-636: Contextual range decoding is request driven and sticky

- Date: 2026-08-08
- Status: accepted

Implement Dynamic Range variant 2 as a private stateful backend whose caller
supplies each expected Symbol context/alphabet or nonzero bypass width. Fix all
31 alphabets and 4,518 frequency entries at construction; serialized input
cannot select table shape or allocation. Reuse variant-1 interval,
normalization, rescaling, and five-shift termination arithmetic. Count Symbol
as one decision and bypass bits individually in LSB-first order. Preserve the
caller value on failure, retain the first error, and finish only on exact event,
decision, payload, and model agreement.

## DD-637: Contextual range decoding feeds typed LZSS tokens directly

- Date: 2026-08-08
- Status: accepted

Connect the private contextual Dynamic Range decoder directly to the
`LzssFieldContext` state machine and typed LZSS validator. Derive every symbol
context, alphabet, and bypass width from previously accepted tokens; do not
let serialized values select model shape. Validate the complete frame in a
first pass without writes, then require sufficient disjoint token staging and
repeat the deterministic decode to materialize only the declared token extent.
Keep the modeled-operation transforms as independent specification/test
boundaries, but do not allocate their native representation on this runtime
path. This adds no public API or stream-format change.

## DD-638: Complete Format 2 frame decode gates all workspace first

- Date: 2026-08-08
- Status: accepted

Compose frame preflight, direct contextual range-to-token decoding, and typed
LZSS reconstruction as one private bounded frame operation. After preflight
but before either staging write, require representable declared extents,
sufficient token and raw capacity, and checked pairwise non-overlap of the
serialized frame, exact token region, and exact raw region. Decode tokens only
into private staging, independently revalidate them during reconstruction, and
report serialized consumption only
after exact raw completion. Preserve zero consumption and unchanged raw output
on every failure. Add no public API, stream field, or format variant.

## DD-639: Format 2 streaming decode buffers and commits one frame

- Date: 2026-08-08
- Status: accepted

Implement the first private Format 2 streaming decoder as an immutable decode
state machine with caller-owned serialized-frame, typed-token, and raw-frame
workspaces. Require those construction spans to be pairwise disjoint. After
each frame header, enforce exact workspace capacities and the checked aggregate
of serialized bytes, native token bytes, and raw bytes before collecting the
body. Decode and reconstruct a complete frame privately, then drain it with
arbitrary output capacity before accepting another frame. Preserve prior-frame
publication on later failure, reject output/raw aliasing, truncation, trailing
bytes, reset, and unknown flags, and make end/error terminal behavior sticky.
Preserve `limit_exceeded` from valid stream/frame headers instead of collapsing
local policy rejection into malformed input.
Keep the lifecycle private without changing Format 2 or the C ABI.

## DD-640: Contextual range encoding uses an exact two-pass plan

- Date: 2026-08-08
- Status: accepted

Encode a complete bounded `ModeledOperation` sequence with the same fixed
31-context schema, adaptive update/rescale rules, LSB-first bypass decisions,
and variant-1 arithmetic used by the private decoder. First run the complete
coder without writes to validate every operation and determine exact decision
and payload counts. Enforce local limits, exact capacity, and operation/output
non-aliasing before repeating the deterministic computation into caller-owned
payload storage. Publish the descriptor only after materialization reproduces
the plan exactly, and leave all caller output unchanged on every pre-write
failure. Keep this operation-level boundary private and do not change Format 2
or the C ABI.

## DD-641: Typed LZSS production shares the deterministic match policy

- Date: 2026-08-08
- Status: accepted

Produce variant-2 `Literal` and `Match` values directly from one complete
bounded raw frame while retaining variant 1's greedy longest-match parse,
nearest-distance tie break, overlap comparison, and strict beneficial-match
threshold. Move only match search and cost selection into one shared private
component used by both encoders; do not serialize and reparse bytes to create
typed values. Plan the entire parse without writes, enforce the checked raw and
native-token aggregate, then require sufficient disjoint output before exact
materialization. Keep empty input valid at this component boundary and leave
Format 2 and the C ABI unchanged.

## DD-642: Complete Format 2 frame encoding uses explicit staged composition

- Date: 2026-08-08
- Status: accepted

Compose the private typed LZSS producer, forward `LzssFieldContext`, contextual
Dynamic Range encoder, and explicit Format 2 serializers for one complete
nonempty frame. Permit planning to materialize caller-owned token and operation
staging, but never serialized output. Reject raw/token/operation workspace
overlap before the first staging write and reject serialized-output overlap
before planning. Enforce the checked aggregate of exact raw, native token,
native operation, and serialized extents; require context and entropy decision
counts to agree; validate header and descriptor before materialization; and
write only the exact serialized prefix. Keep all interfaces private and retain
the existing Format 2 bytes and C ABI.

## DD-643: Format 2 streaming encoding prepares one atomic frame at a time

- Date: 2026-08-08
- Status: accepted

Emit the canonical stream header from a validated known-size configuration,
then collect at most one raw frame before invoking the complete private frame
encoder. Keep raw, typed-token, modeled-operation, and serialized-frame
workspaces mutually disjoint and reject caller output that aliases any of
them. Publish no frame byte until its complete representation exists; drain it
fully before consuming the following frame. Treat `Flush` as nonstructural,
reject `ResetBlock`, retain `EndInput` across final draining, and make terminal
states sticky. Keep the lifecycle private and preserve Format 2 and the C ABI.

## DD-644: Format 2 profiles own native typed-view layout

- Date: 2026-08-09
- Status: accepted

Calculate the canonical known-size stream configuration together with a
conservative complete-frame capacity: at most one token, two modeled
operations, six decisions, and twelve payload bytes per raw byte, plus the
five-byte range termination. Enforce all format-field, local-limit, payload,
native-view, and total-workspace bounds before publishing requirements. Return
typed element counts, an aligned operation offset, total view bytes, and base
alignment; require dedicated partition functions to rederive and validate the
layout transactionally. Derive decoder capacities solely from local limits.
Keep the profile private while making it directly reusable by a later C ABI.

## DD-645: Format 2 receives a distinct additive C lifecycle

- Date: 2026-08-09
- Status: accepted

Expose the experimental composition as
`marc_lzss_contextual_dynamic_range_*`, never as a silent change to the
Format 1 `marc_lzss_dynamic_range_*` lifecycle. Use one size-tagged plain-C
configuration and the existing primary, secondary, and aligned opaque views
regions. Map encode to raw/serialized/token-plus-operation storage and decode
to serialized/raw/token storage. Reuse the private profile and partitioners;
validate capacity, alignment, pairwise non-overlap, and reserved fields before
publishing a handle. Keep ABI version 1 because the addition changes no
existing declaration, structure, symbol, or behavior.

## DD-646: Format 2 public completion is frame-atomic

- Date: 2026-08-09
- Status: accepted

Audit the experimental public lifecycle with 64-byte raw frames and only the
three workspaces returned by its C requirements query. Reserve at most
`12F + 5` payload bytes and `12F + 85` complete frame bytes without changing
the encoded representation. Require deterministic output across repeated and
arbitrarily chunked execution, sticky terminal results, and exact recovery of
all required binary data classes. Corruption, truncation, or trailing data at
the fourth frame must leave its one raw byte unpublished after the first three
complete 64-byte frames have been committed. This evidence advances public
completion only; it does not admit the experiment to CLI, fuzz, benchmark, or
interoperability inventories.

## DD-647: Format 2 fuzzing is fixed-memory and dual-path

- Date: 2026-08-09
- Status: accepted

Add one experimental decoder fuzz entry that truncates every supplied case to
8,192 bytes. After a valid Format 2 header, exercise the complete-frame private
decoder with fixed storage. Independently exercise the public C streaming
decoder for every case with input-derived chunks, at most 4,096 published raw
bytes, a 1,024-byte raw frame, a `12F + 5 = 12,293` byte payload ceiling, and
compile-time arrays for serialized frame, typed views, raw staging, and output.
Limit process calls to bounded input plus bounded output plus 32.

Abort only for API-accounting, queried-workspace, construction, progress, final
input, or call-budget invariant violations; malformed streams are successful
iterations. Retain ordinary regressions for every strict prefix of canonical
`ABABX`, saturated Format 2 frame extents, and nonzero range-descriptor
reserved data. Require public frame atomicity and sticky errors; require the
private complete-frame path to preserve raw staging whenever it is applicable.
This adds no format, CLI, benchmark, interoperability entry, or sanitizer
campaign claim.

## DD-648: Format 2 CLI admission is public-only and experimental

- Date: 2026-08-09
- Status: accepted

Add `lzss-contextual-dynamic-range` as an explicit experimental selector in
the transactional CLI without changing the stable 42-profile inventory or
the default `lz77` selector. Fix raw frames and context blocks at 65,536 bytes,
use the public `12F + 5 = 786,437` payload ceiling, and apply an 8-MiB
internal-buffer policy. Obtain primary, secondary, and aligned opaque-view
extents separately for each direction from the public C requirements query.

Construct, process, and destroy the transform only through the public C ABI.
Do not include private Format 2 headers or reproduce token, operation, model,
partition, element-size, or alignment formulas in the command-line layer.
Retain the existing temporary-output transaction and prove nonempty and empty
round trips, overwrite refusal, malformed-input cleanup, and strict trailing
data rejection through the generic CLI regression. This changes no stream
byte, ABI version, stable matrix count, benchmark, or interoperability schema.

## DD-649: Format 2 benchmark retains a separate experimental inventory

- Date: 2026-08-09
- Status: accepted

Add `lzss-contextual-dynamic-range` to the dependency-free benchmark without
changing its stable 42-profile command matrix. Reuse the CLI's public 65,536-
byte frame, `12F + 5` payload, and 8-MiB internal-buffer policy. Bound complete
output for input extent `N` and nonempty frame count `K` by the Format 2 rule
`112 + 12N + 85K`.

Create both directions through the public C configuration, requirements,
factory, process, and destroy lifecycle. Require a byte-exact round trip before
timing, keep file I/O, allocation, construction, destruction, and verification
outside timed intervals, and report ratio, directional throughput, each
queried primary/secondary/views extent, and peak caller-owned workspace. Add a
separately labeled smoke test and record descriptive local results without
claiming stable performance or interoperability admission.

## DD-650: Interoperability schema 32 appends experimental Format 2 once

- Date: 2026-08-09
- Status: accepted

Freeze the exact 42-entry schema-31 order and append one
`lzss-contextual-dynamic-range` archive as entry 43. Set `schema_version` to
32 and `codec_set` to `marc-cli-v32`. Preserve the deterministic 8,193-byte
binary fixture and record the complete Git object ID, producer identity, CLI
SHA-256, and every input/archive size and SHA-256.

Require exact manifest order, one instance of every expected codec, foreign
decode equality, and byte-identical local re-encoding. Reject a reordered
schema-32 manifest. Derive schema 31 by removing only the new final archive
and restoring `marc-cli-v31`, then continue the unchanged compatibility chain
through schema 1. This is local admission evidence; cross-platform schema-32
artifacts remain external evidence and must be recorded only after execution.

That release check completed at revision
`e9cf0c7d649cf32c9bc3a49bf3db9150370db381`: the Windows/MSVC and Ubuntu
24.04/Ninja artifacts verified on Ubuntu 26.04/Clang, and the Ubuntu 26.04
bundle verified locally and on Windows/MSVC. Every pass decoded and
byte-identically re-encoded all 43 archives.

## DD-651: Contextual evolution begins from a paired empirical baseline

- Date: 2026-08-09
- Status: accepted

Before selecting a second context-model or entropy-backend variant, compare
the existing Format 1 `lzss-dynamic-range` and Format 2
`lzss-contextual-dynamic-range` profiles with the same input, revision, build
type, frame policy, and iteration count. Treat encoded extent and queried peak
caller-owned workspace as the reliable outputs of this small smoke; do not
infer throughput from a 4,326-byte input and one rounded timed iteration.

Require both local compilers to agree on each encoded extent. Record the
Format 2 reduction relative to the Format 1 encoded extent, together with its
workspace increase, so later variants have an explicit point of comparison.
This measurement selects no future backend, changes no format or API, and does
not substitute for a representative fixed corpus.

## DD-652: Contextual rANS is the second Format 2 entropy backend

- Date: 2026-08-09
- Status: accepted

Retain typed LZSS variant 2 and `LzssFieldContext` variant 1, and reserve rANS
algorithm ID 4 variant 2 so the next experiment changes only the entropy axis.
Reuse variant 1's scalar 64-bit state, table log 12, total 4,096, lower bound
`2^31`, byte renormalization, deterministic normalization, numeric tie breaks,
and final-state-first payload.

Give each of the 31 Symbol contexts its own frame-static normalized model. Lay
all 4,518 uint16 frequencies out by ascending context and symbol in one fixed
9,052-byte descriptor; unused contexts are zero. Code bypass bits in the same
state with fixed frequencies 2,048/2,048, reversing their per-operation walk
on encode so decode remains LSB first. Charge at most 31 separate 4,096-slot
decode tables before construction, conservatively bound payload by two bytes
per decision plus eight, and preserve frame-atomic publication.

Accept the fixed model overhead as a clear reference-format cost. Any sparse
table representation, state interleaving, or different normalization receives
a later variant. This decision reserves documentation and a one-Literal vector
only; it adds no implementation, public API, CLI, benchmark, or readiness
claim.

## DD-653: Contextual rANS descriptor validation owns no context schema

- Date: 2026-08-09
- Status: accepted

Implement the fixed 9,052-byte contextual rANS descriptor as a private entropy
format boundary before state decoding. Parse into private bounded storage and
publish only after fixed fields, expected decision/payload counts, every
frequency slice, the two-byte-per-decision payload ceiling, the complete
126,976-entry decode-table charge, and descriptor-plus-payload limits pass.
Serialize into a complete temporary byte array after validation and copy only
on success.

Move the 31 alphabets, 32 flattened offsets, and 4,518 frequency-entry count
to an `LzssFieldContext` format header. Update Dynamic Range to consume the
context-owned names directly and remove the backend-named compatibility
aliases rather than preserving ambiguous internal terminology during
pre-release development. This adds no rANS state decoder, table builder,
encoder, frame integration, public API, or format change.

## DD-654: Contextual rANS decode tables are fixed and transactional

- Date: 2026-08-09
- Status: accepted

Materialize every accepted contextual rANS descriptor into a caller-owned
31-by-4,096 `RansDecodeEntry` array. Give context `c` the fixed region starting
at `c * 4096`; fill inactive regions with zero and active regions with the
canonical cumulative start, normalized frequency, and symbol. This favors a
simple checked state-decoder lookup over a compact reference allocation.

Validate the descriptor and complete table charge, check caller capacity, and
copy all 4,518 frequencies before any output write. Publish the complete table
span and active-context flags only after construction, leaving prior storage
and views unchanged on every prewrite failure. This makes descriptor/output
aliasing harmless and adds no state decoder, frame integration, public API, or
format change.

## DD-655: Contextual rANS state decoding is caller-driven and strict

- Date: 2026-08-09
- Status: accepted

Implement a private scalar decoder whose `begin` accepts the fixed descriptor,
exact payload, decoder limits, and caller-owned 126,976-entry table storage.
Validate payload and initial state before building tables. Decode Symbol
requests only from the caller-specified active context and fixed alphabet;
decode bypass requests as 1 through 16 fixed-probability bits LSB first. Both
request forms advance one shared variant-1 rANS state and exact decision
counters without publishing a caller value on failure.

Keep errors sticky. At finish, require exact caller event and decision counts,
the descriptor decision count, use of every nonzero serialized model, terminal
state exactly `L`, and exact payload exhaustion. Treat table storage as
exclusive decoder workspace but validate each selected entry's structural
bounds at use. This adds no typed-token bridge, frame decoder, encoder, public
API, CLI profile, or format change.

## DD-656: Contextual rANS token inversion is a two-pass direct bridge

- Date: 2026-08-09
- Status: accepted

Connect the private scalar decoder directly to `LzssFieldContextState` and
typed LZSS validation. Derive every Symbol context, alphabet, and bypass width
from already accepted token state, reconstruct one complete token locally,
validate it against parameters and raw bounds, and only then advance context.
Do not allocate or expose an intermediate modeled-operation sequence.

Use a write-free validation pass followed by one deterministic token-writing
pass. Rebuild and reuse the same caller-owned 126,976-entry table extent in
both passes. Before either table write reject payload/table overlap; before
token publication also reject payload/token and table/token overlap. Preserve
token output on all prewrite failures. This adds no raw reconstruction, frame
decoder, encoder, public API, CLI profile, or format change.

## DD-657: Contextual rANS frame decoding is one four-region transaction

- Date: 2026-08-09
- Status: accepted

Define dedicated contextual-rANS stream, frame, layout, and error types rather
than reusing the Dynamic Range Format 2 names. Accept only the already reserved
dictionary `2/2`, entropy `4/2`, and context `1/1` identity, fixed 9,052-byte
descriptor, and exact descriptor-plus-payload frame extent. Validate all
headers, counts, limits, and descriptor frequencies before exposing layout.

Require caller-owned decode tables, typed tokens, and raw output. Admit their
exact extents, calculate their byte sizes, and reject all six pairwise overlaps
with serialized input and each other before any table write. Then run the
two-pass direct token inversion and typed LZSS reconstruction in order; report
serialized consumption only after raw reconstruction succeeds. This adds no
encoder, streaming lifecycle, public API, CLI profile, benchmark, archive, or
format change.

## DD-658: Contextual rANS operation encoding reuses numeric normalization

- Date: 2026-08-09
- Status: accepted

Build each used Symbol context's static model from the complete accepted
`ModeledOperation` sequence. Normalize its observed counts independently to
4,096 with the variant-1 integer-error rule: floor the exact scale with a
minimum of one for present symbols, add units to greatest positive error with
lowest-symbol ties, and remove units from least error with highest-symbol ties.
Leave unused context slices zero. Bypass decisions never enter these counts and
always use the fixed 2,048/2,048 model.

Validate and count the entire operation sequence before normalization or state
coding. Encode operations in reverse logical order; within a bypass operation,
encode bits from highest selected index down to zero so forward decoding still
publishes the value LSB first. Use the scalar variant-1 state transition and
backward renormalization-byte fill, plan without output writes, and publish the
descriptor only after exact payload size and limits validate. Reject operation/
payload overlap before encoding. This adds no direct token bridge, frame
encoder, streaming lifecycle, public API, CLI profile, or format change.

## DD-659: Direct token encoding shares a count builder and reverse writer

- Date: 2026-08-09
- Status: accepted

Factor contextual rANS's per-context counting/normalization and scalar reverse
state writer into private entropy primitives used by both the operation-level
reference encoder and a typed-LZSS direct bridge. Keep the established public-
private operation encoder contract and exact bytes unchanged; do not duplicate
state arithmetic or normalization in the context layer.

Validate the complete typed-token frame first, then walk tokens forward to
feed Symbol fields and bypass widths into the shared model builder. For exact
planning and output, walk tokens backward. Derive a token's pre-state from its
immediate predecessor kind and the latest preceding Literal. Maintain the
preceding-Literal position monotonically while moving backward, so context
reconstruction is linear rather than rescanning the prefix per token. Within
each token emit fields in reverse modeled order, leaving bypass bit reversal to
the shared writer. Reject token/payload overlap before output. This adds no raw
dictionary parsing, frame encoder, streaming lifecycle, public API, CLI
profile, or format change.

## DD-660: Contextual rANS frame encoding is a three-region transaction

- Date: 2026-08-09
- Status: accepted

Compose raw-to-typed LZSS parsing, the DD-659 direct contextual rANS bridge,
and the dedicated DD-657 frame serializer without modeled-operation staging.
Planning validates the rANS-specific stream identity and exact raw-frame size,
proves disjoint raw/token regions, materializes typed tokens, obtains exact
event, decision, descriptor, and payload values, validates the complete frame
header, and charges raw plus used token bytes plus exact serialized bytes.

Encoding additionally requires the serialized output to be disjoint from raw
input and the caller-owned token region before planning can write tokens. Admit
the exact output capacity before entropy output, then serialize the already
validated 64-byte header and fixed 9,052-byte descriptor around the payload.
After a successful plan, report the required serialized extent even when the
supplied output is short; only successful encoding commits serialized bytes.
Leave surplus output untouched. This adds no streaming lifecycle, workspace
calculator, public API, CLI profile, benchmark, archive, or format change.

## DD-661: Contextual rANS streaming encode drains immutable frames

- Date: 2026-08-09
- Status: accepted

Add a bounded known-size streaming encoder above DD-660. Serialize the
dedicated 112-byte contextual-rANS stream header during construction, collect
exactly one raw frame in caller-owned storage, prepare its complete canonical
frame in separate caller-owned serialized staging, and drain only immutable
bytes. Commit the raw extent and sequence only after complete preparation, and
do not accept the next frame until the current frame has drained.

Require raw, typed-token, and serialized-frame workspaces to be pairwise
disjoint and reject caller output that aliases any of them. Reuse DD-660's
checked aggregate over the exact raw frame, used token prefix, and complete
serialized frame; no modeled-operation region is introduced. Retain
`EndInput` while the stream header or final frame drains. Nonterminal `Flush`
does not close a partial frame; reject `ResetBlock`, unknown flags, premature
finish, excess input, short storage, and limit failure with sticky terminal
errors. Empty input emits only the stream header, and calls after completion
return `EndOfStream`. This adds no streaming decoder, workspace calculator,
public API, CLI profile, benchmark, fuzz target, archive, or format change.

## DD-662: Contextual rANS streaming decode commits complete raw frames

- Date: 2026-08-09
- Status: accepted

Add the bounded streaming decoder paired with DD-661. Incrementally collect
and validate the dedicated 112-byte stream header and one 64-byte frame header.
Before accepting the frame body, derive its exact serialized extent and admit
caller-owned storage for that extent, the fixed 126,976-entry contextual-rANS
decode tables, the declared typed-token count, and the declared raw extent.
Require all four regions to be pairwise disjoint and charge their checked live
byte total against `max_internal_buffered_bytes`.

Collect the complete descriptor and payload before invoking DD-657. Publish no
raw byte until descriptor/table construction, entropy inversion, typed-token
validation, and LZSS reconstruction all succeed. Then drain only the immutable
raw frame under arbitrary output capacity before accepting another frame.
Retain `EndInput` across raw drain; reject truncated or trailing input,
`ResetBlock`, unknown flags, output/workspace aliasing, insufficient storage,
and configured-limit excess with sticky terminal errors. Empty input accepts
only the stream header and exact finish. This adds no workspace calculator,
public API, CLI profile, benchmark, fuzz target, archive, or format change.

## DD-663: Contextual rANS profile owns conservative typed workspace layout

- Date: 2026-08-09
- Status: accepted

Add private requirements calculators for the DD-661 encoder and DD-662
decoder. For largest raw-frame extent `N`, reserve at most `N` typed tokens,
`6N` decisions, `12N + 8` payload bytes, and therefore
`64 + 9,052 + 12N + 8` complete serialized-frame bytes. The encoder views
region contains only the native token array; raw input and serialized staging
remain separate byte regions. Validate every product, sum, uint32 format bound,
configured payload bound, and exact raw-plus-view-plus-frame aggregate before
publishing requirements.

The decoder calculator derives a conservative raw extent from local frame and
block limits, caps payload by both the configured compressed limit and
`12N + 8`, and reserves one complete serialized frame. Its opaque views region
contains the fixed 126,976 native `RansDecodeEntry` values followed by the
aligned maximum token array. Record the token offset, total bytes, and strongest
alignment explicitly. Partition functions recompute every layout field, reject
forged requirements, insufficient or misaligned storage, and publish views only
after complete validation. This adds no public C API, factory, CLI profile,
benchmark, fuzz target, archive, or format change.

## DD-664: Contextual rANS receives a distinct additive ABI-1 lifecycle

- Date: 2026-08-09
- Status: accepted

Expose the private DD-661 through DD-663 lifecycle as the experimental
`marc_lzss_contextual_rans_*` C family. Use a distinct size-tagged configuration
that carries known size, frame and LZSS parameters, output/frame/block/payload/
buffer/distance/match limits, and entropy-table entries. Do not reuse or alias
the contextual Dynamic Range configuration, names, or factory.

The requirements query maps encoder raw, complete-frame, and token-view
requirements to primary, secondary, and aligned opaque views respectively.
For decode it maps complete serialized frame, atomic raw frame, and fixed-table
plus token views to those same three public regions. The factory repeats the
query, validates exact used-prefix capacities, alignment, pairwise non-overlap,
and recomputed private partitions before allocating the small transform handle.
Failure leaves the output handle null. Prove the header from a C11 translation
unit, requirements-driven multi-frame round trip, Format 2 rANS identity,
direction-specific sizing, and all public validation failures. This changes no
existing function, structure, ABI version, stream byte, CLI selector, benchmark,
fuzz target, archive, or interoperability schema.

## DD-665: Contextual rANS public completion is frame-atomic

- Date: 2026-08-09
- Status: accepted

Audit the experimental contextual-rANS C lifecycle with 64-byte raw frames and
only the three workspaces returned by its requirements query. Admit at most
`6F = 384` modeled decisions, `12F + 8 = 776` payload bytes, and
`64 + 9,052 + 776 = 9,892` complete-frame bytes without changing the format.
Require deterministic output across repeated and arbitrarily chunked calls,
sticky terminal results, and exact recovery of all required binary classes.
Corruption, truncation, or trailing data in the fourth frame must leave its one
raw byte unpublished after three complete 64-byte frames have committed. This
adds completion evidence only, with no CLI, fuzz, benchmark, archive, or
interoperability admission.

## DD-666: Contextual rANS fuzzing fixes the dominant table workspace

- Date: 2026-08-09
- Status: accepted

Add one experimental decoder fuzz entry capped at 32,768 supplied bytes. This
ceiling deliberately exceeds the fixed 9,052-byte descriptor so a complete
Format 2 rANS frame remains reachable. After a valid stream header, exercise
the private complete-frame decoder. Independently exercise the public C
streaming decoder for every case with input-derived chunks, at most 4,096
published raw bytes, a 1,024-byte raw frame, 6,144 modeled decisions, a
`12F + 8 = 12,296` payload ceiling, and a finite input-plus-output-plus-32 call
budget.

Fix serialized, raw, output, typed-token, and 126,976-entry decode-table
storage before accepting input. Keep the large native arrays in thread-local
harness storage so execution neither consumes a multi-megabyte call stack nor
shares mutable state between fuzz threads. Malformed streams end an iteration
normally; abort only for queried-workspace, construction, accounting,
progress, final-input, or call-budget invariant failure. Retain ordinary
atomic regressions for every strict canonical `ABABX` prefix, saturated frame
extents, and nonzero descriptor flags. This changes no format or admission
inventory and does not claim a sanitizer campaign.

## DD-667: Contextual rANS CLI admission is public-only and experimental

- Date: 2026-08-09
- Status: accepted

Add `lzss-contextual-rans` as an explicit experimental selector in the
transactional CLI without changing the stable 42-profile inventory or default
`lz77` selector. Fix raw frames at 65,536 bytes, admit at most `6F = 393,216`
modeled decisions and `12F + 8 = 786,440` payload bytes, and apply an 8-MiB
internal-buffer policy. Obtain primary, secondary, and aligned opaque-view
extents separately for each direction from the public C requirements query.

Construct, process, and destroy the transform only through the public
`marc_lzss_contextual_rans_*` lifecycle. Do not include private Format 2
headers or reproduce token, operation, table, partition, element-size, or
alignment formulas in the command-line layer. Retain temporary-output
transactionality and prove nonempty and empty round trips, overwrite refusal,
malformed-input cleanup, and strict trailing-data rejection through the common
CLI regression. This changes no stream byte, ABI version, stable matrix count,
benchmark inventory, or interoperability schema.

## DD-668: Contextual rANS benchmark remains experimentally inventoried

- Date: 2026-08-09
- Status: accepted

Add `lzss-contextual-rans` to the dependency-free benchmark without changing
the stable 42-profile command matrix. Reuse the CLI's public 65,536-byte frame,
`6F` decision, `12F + 8` payload, and 8-MiB internal-buffer policy. Bound
complete output for input extent `N` and nonempty frame count `K` by the exact
conservative Format 2 rule `112 + 12N + 9,124K`.

Create both directions through the public C configuration, requirements,
factory, process, and destroy lifecycle. Require a byte-exact round trip before
timing; keep file I/O, allocation, construction, destruction, and verification
outside timed intervals; and report ratio, directional throughput, each
queried primary/secondary/views extent, and peak caller-owned workspace. Add a
separately labeled smoke test and record descriptive local results without
claiming stable performance or interoperability admission.

## DD-669: Contextual rANS encoding removes redundant frame planning

- Date: 2026-08-09
- Status: accepted

Treat the first benchmark result as a diagnostic rather than a performance
claim. The streaming encoder previously called the complete-frame planner and
then the complete-frame encoder, although the latter performs the same
transactional plan internally. Within each frame planner it also counted LZSS
tokens in a separate pass before calling the transactional typed-token encoder,
which performs its own preflight. With the reference match finder, these
layers caused six complete match-search passes for every encoded frame.

Pass the full preallocated serialized-frame workspace directly to the
complete-frame encoder and let its single internal plan determine the committed
extent. Within that plan, call the typed-token encoder directly over the
worst-case token workspace; its existing preflight preserves atomic failure.
The resulting two match-search passes per frame retain identical bytes,
limits, overlap rules, and caller-owned workspace while removing four
redundant searches. Prove byte identity against the pre-change README and
format-specification archives, focused streaming behavior, and comparative
Release timing. This optimization does not repair the fixed 9,052-byte model
cost; a compact descriptor requires a distinct entropy variant.

## DD-670: Contextual rANS compact models use canonical hybrid records

- Date: 2026-08-09
- Status: accepted

Reserve entropy algorithm/variant `4/3` for the same contextual-rANS state and
payload rules as variant 2 with a variable descriptor only. Preserve variant
2 decoding under `4/2`; do not silently reinterpret its fixed 9,052 bytes.
Represent active contexts with a 31-bit mask and choose independently between
an inferred-final-frequency dense record and an increasing-symbol sparse
record. Select sparse only when its exact `3K` bytes are strictly fewer than
the dense record's `1 + 2(A-1)` bytes, making ties and every valid model
canonical.

The descriptor is bounded between 23 and 9,025 bytes without allocation or
recursion. The LZSS profile's one-Literal vector uses one three-byte dense
record and one three-byte sparse record, and therefore a 26-byte descriptor.
Retain the existing fixed maximum decode-table workspace initially so this
change addresses wire overhead
without combining it with a table-layout optimization. Keep variant 3 private
until its parser, serializer, malformed suite, state round trip, frame
integration, public lifecycle, benchmark, and cross-platform archive each
complete independently.

## DD-671: Contextual Dynamic Range removes redundant nested planning

- Date: 2026-08-09
- Status: accepted

Apply DD-669's transactional call-graph correction to the first Format 2
backend. The contextual Dynamic Range streaming encoder currently plans a
complete frame before calling the complete-frame encoder, which repeats that
plan. Each frame plan separately counts typed LZSS tokens before the typed
encoder's own preflight and separately counts modeled operations before the
context materializer's own validation. This produces six reference match
searches and four context-plan walks per frame.

Pass the full bounded serialized workspace directly from streaming to the
complete-frame encoder. Within its plan, call the typed-token encoder over the
worst-case token workspace and the context materializer over the worst-case
operation workspace, mapping their existing `output_too_small` failures to the
frame capacity categories. Retain the entropy coder's write-free exact plan,
which is required to determine payload extent before transactional output.
The result performs two match searches and one context validation plus one
materialization per frame without changing bytes, workspace requirements,
limits, or atomic failure semantics.

## DD-672: Compact descriptor parsing reconstructs privately

- Date: 2026-08-09
- Status: accepted

Implement entropy variant 3's descriptor as a distinct internal format module
that reuses the variant-2 in-memory `ContextualRansDescriptor` only after
successful reconstruction. Accept a caller-supplied exact byte span, parse at
most 31 compile-time-bounded context records into a local fixed descriptor,
verify the canonical dense/sparse choice after reconstructing every slice, and
require exact input exhaustion before publication. Allocate nothing and retain
the fixed 126,976-entry decode-table admission rule.

Serialization first validates the complete in-memory descriptor and computes
its exact canonical extent. It writes into a fixed 9,025-byte local array and
copies only after caller capacity is known, leaving output unchanged on every
field, model, limit, arithmetic, or capacity failure. Keep this module
unconnected to variant-2 stream parsing, rANS state decoding, and public APIs.

## DD-673: Compact contextual rANS begins through the existing scalar decoder

- Date: 2026-08-09
- Status: accepted

Add an explicit `begin_compact` entry point to the private contextual-rANS
decoder rather than creating a second table builder or state machine. It
accepts the exact variable descriptor span plus frame-declared decision and
payload sizes and parses into a local fixed descriptor. Extract the existing
already-validated model-to-table and payload-state initialization steps as the
single shared core used by both begin paths. Do not pass compact input through
variant 2's format validator, because its 9,052-byte serialized-descriptor
charge is not variant 3's actual bounded extent. Return the compact format
error beside the ordinary decoder result so malformed representation and
state/payload/workspace failures remain distinguishable.

Reset decoder state before parsing. A compact parse failure becomes sticky
`invalid_descriptor` for subsequent decode calls, leaves caller table storage
unchanged, and preserves the exact compact error. A successful compact begin
must construct byte-for-byte identical 126,976 table entries and follow the
same Symbol, bypass, count, terminal-state, and payload-exhaustion rules as
variant 2. This milestone adds no second decoder arithmetic, frame parser,
typed-token bridge, encoder format selection, public API, or CLI profile.
The shared table materializer defensively revalidates all model fields and
frequency sums before writing, but does not repeat either format's serialized
size charge.

## DD-674: Compact contextual rANS reuses one typed-token state machine

- Date: 2026-08-09
- Status: accepted

Add distinct private validation and decode entries that accept an exact
compact-descriptor span, but retain the existing LZSS field-context token
state machine and two-pass transactional publication. The compact entry calls
the scalar decoder's `begin_compact`; it does not first reconstruct a public
descriptor or route through variant 2's fixed-format validator.

Report the compact representation error beside the existing token and entropy
decode result. Validate parameters, declared token/event/decision/raw bounds,
descriptor/payload/table overlap, token capacity, and all token aliasing with
the same precedence and limits as variant 2. The first pass must publish no
token; the second pass may publish only after the first pass succeeds and
sufficient disjoint token storage is present. Require both passes to agree on
counts, raw extent, payload consumption, and compact error.

Prove the specified one-Literal compact vector yields the same typed token and
accounting as variant 2, and that malformed compact records, short token
storage, and aliased buffers preserve caller token bytes. This milestone adds
no frame parser, stream-header admission, encoder selection, public API, CLI,
benchmark, or interoperability profile.

## DD-675: Compact contextual rANS frames admit exact variable descriptors

- Date: 2026-08-09
- Status: accepted

Add a distinct private compact-frame preflight and complete decoder while
retaining the common 64-byte Format 2 frame-header fields. The compact header
validator accepts descriptor sizes only from 23 through 9,025 bytes, charges
the exact descriptor plus payload extent against the internal-buffer limit,
and passes precisely that descriptor span to the compact parser. It must not
reconstruct or charge the fixed 9,052-byte variant-2 representation.

After complete preflight, require exact caller-owned table, token, and raw
capacities and reject every overlap among the admitted serialized frame and
the three workspaces before any write. Decode typed tokens through the compact
bridge and reconstruct raw bytes only after token success. Any header,
descriptor, truncation, capacity, overlap, entropy, token, or reconstruction
failure reports zero serialized consumption and preserves raw output. This
milestone adds no stream-header parser, encoder selection, streaming lifecycle,
public API, CLI selector, benchmark, or interoperability archive.

## DD-676: Compact contextual rANS has an explicit stream identity

- Date: 2026-08-09
- Status: accepted

Add distinct private compact stream-header parse, serialize, and validation
entries. They share the same 112-byte Format 2 fields and in-memory parameter
structure as contextual-rANS variant 2, but require and emit entropy variant 3.
Variant 2 and variant 3 parsers must reject one another's canonical header as
`unsupported_entropy_variant`; neither entry is a compatibility alias.

Factor only the common field parser and transactional serializer internally.
Keep the public-named variant entries explicit so later streaming factories
cannot silently choose the wrong representation. Parsing publishes neither
the header nor consumed count until all fields and limits pass. Serialization
builds the complete header in a zeroed local array before copying. This
milestone adds no frame-streaming lifecycle, encoder selection, public API,
CLI selector, benchmark, or interoperability archive.

## DD-677: Compact streaming buffers one exact variable frame

- Date: 2026-08-09
- Status: accepted

Add a distinct private streaming decoder for entropy variant 3. It follows the
established Format 2 lifecycle but parses only the compact stream identity,
validates each frame with the compact header rules, and derives the buffered
frame extent from `64 + descriptor_size + payload_size`. It must not reserve or
wait for variant 2's fixed 9,052-byte descriptor.

Buffer at most one admitted serialized frame, decode into private caller-owned
tables/tokens/raw storage, and drain raw bytes before accepting the next frame.
Arbitrary input and output splits, including one byte each, must not alter
results. `EndInput` remains sticky across a zero-capacity drain. A malformed
later frame may not retract earlier output and may not publish its own raw
bytes. Reject trailing data, reset requests, unknown flags, premature end, and
overlapping construction/output storage with a stable terminal error. This
milestone adds no encoder lifecycle, public API, CLI selector, benchmark, fuzz
entry, or interoperability archive.

## DD-678: Compact frame planning uses the exact serialized model

- Date: 2026-08-09
- Status: accepted

Add distinct private compact frame plan and encode entries. Both reuse the
typed-LZSS producer and direct contextual-rANS token encoder, but the plan must
validate the resulting model through the compact descriptor contract and use
its exact serialized size. It must not call the fixed-frame plan or inherit
the 9,052-byte descriptor charge.

The complete serialized extent is checked `64 + compact descriptor + payload`.
Encoding writes the payload into its final admitted position, serializes the
compact descriptor, and writes a compact-validated common frame header whose
descriptor-size field equals the actual model bytes. Header and descriptor
serialization use local transactional buffers; any planning, capacity, limit,
alias, payload, descriptor, or header failure leaves serialized output
unchanged. The one-Literal output must equal the specified 98-byte frame and
decode through the compact complete-frame decoder. This milestone adds no
streaming encoder, public API, CLI selector, benchmark, or interoperability
archive.

## DD-679: Compact streaming encoding shares lifecycle, not identity

- Date: 2026-08-09
- Status: accepted

Add a distinct private compact streaming-encoder type. Reuse the fixed
contextual-rANS encoder's collection, immutable drain, known-size EndInput,
Flush, capacity, alias, and sticky-terminal lifecycle behind an explicit
fixed/compact representation mode. The compact constructor must validate and
serialize only variant 3 stream identity, and frame preparation must call only
the DD-678 compact complete-frame encoder. It must never emit variant 2 or
charge the fixed descriptor extent.

Chunking alone does not alter output. Emit the canonical 112-byte compact
stream header before accepting frame publication, collect exactly one declared
raw frame, construct its complete compact bytes in caller-owned private
workspace, and drain them before collecting another frame. Preserve EndInput
while either header or frame bytes remain pending. Flush may expose already
representable output but does not close a partial raw frame. ResetBlock remains
unsupported. Constructor, workspace, capacity, limit, input-extent, alias,
unknown-flag, and reset failures become stable terminal errors. This milestone
adds no public API, CLI selector, benchmark, new fuzz entry, or interoperability
archive.

## DD-680: Compact profile derives workspace from variant 3 bounds

- Date: 2026-08-09
- Status: accepted

Add explicit private compact profile and decoder-workspace query names. Retain
the existing configuration, requirements, typed-view layout, payload ceiling,
decode-table count, checked arithmetic, and partition functions. Do not turn
the existing variant-2 function names into ambiguous aliases.

The compact encoder validates the synthesized stream through variant 3 and
reserves `64 + 9,025 + (12N + 8)` serialized-frame bytes for largest raw frame
extent `N`; the compact decoder applies the same 9,025-byte descriptor ceiling
to its bounded payload capacity. Continue charging raw, token, table, and
serialized regions against the same aggregate internal-memory limit. Empty
known-size input requires no frame workspace and alignment one. Returned
requirements must construct the distinct compact streaming encoder and decoder
and complete a byte-exact round trip. This milestone adds no public C/C++ API,
CLI selector, benchmark, fuzz entry, or interoperability archive.

## DD-681: Compact contextual rANS receives an explicit additive C lifecycle

- Date: 2026-08-09
- Status: accepted

Add `marc_lzss_contextual_rans_compact_config` and the three ABI-1 symbols
`marc_lzss_contextual_rans_compact_config_init`,
`marc_lzss_contextual_rans_compact_workspace_requirements`, and
`marc_lzss_contextual_rans_compact_create`. The new configuration has the same
field contract and limits as the fixed contextual-rANS configuration but is a
distinct C type; do not accept one type through the other lifecycle or create
compatibility aliases. This additive symbol set does not change
`MARC_ABI_VERSION` or any existing structure, function, or stream.

Delegate workspace arithmetic to DD-680. Map encoder raw, compact complete-frame,
and typed-token requirements to primary, secondary, and aligned views; map
decoder compact serialized-frame, atomic raw-frame, and table-plus-token views
to the same public regions. Creation repeats the query, validates capacity,
alignment, pairwise non-overlap, and private partitions before allocating the
small transform handle. Failure leaves the handle null. Prove the public header
from C11, requirements-driven multi-frame round trip, entropy variant 3,
direction-specific exact sizes, reserved/size/version validation, and every
short, misaligned, overlapping, null, and invalid-direction case. This step adds
no CLI selector, benchmark, new fuzz target, completion claim, or
interoperability archive.

## DD-682: Fixed and compact contextual rANS share one completion matrix

- Date: 2026-08-10
- Status: accepted

Turn the existing public contextual-rANS completion suite into a typed
fixed/compact parameter matrix. Each case must initialize, query, construct,
and process through its own distinct ABI-1 symbol family; the test must not
cast or alias either public configuration type. Use each representation's
exact descriptor ceiling when sizing encoded output.

Require both representations to cover empty input, every one-byte value,
binary patterns, deterministic random bytes, 63/64/65-byte frame boundaries,
repeat determinism, one-byte and mixed input/output schedules, sticky
EndOfStream, and atomic publication across a corrupt, truncated, or trailing
fourth frame. Require fixed streams to identify entropy variant 2 and compact
streams to identify variant 3. This is public completion evidence only; it
does not add a CLI selector, benchmark, compact fuzz target, or
interoperability archive.

## DD-683: Compact contextual rANS has a fixed-memory dual-path fuzz boundary

- Date: 2026-08-10
- Status: accepted

Compile the repository-owned contextual-rANS decoder harness as two distinct
fuzzer executables. The fixed executable selects entropy variant 2; the new
compact executable selects variant 3 at compile time. Both feed the same
bounded input independently to the private complete-frame decoder after
strict stream-header acceptance and to the corresponding public streaming C
decoder. Do not auto-detect or cross-dispatch representations inside either
target.

Retain the 32,768-byte input cap, 4,096-byte aggregate output cap, 1,024-byte
frame cap, exact table/token/raw/serialized workspace arrays, and finite call
budget. Derive the compact serialized extent from its 9,025-byte descriptor
ceiling and require its public workspace query to fit every static region
before construction. Abort only on an internal contract violation; malformed
input is an ordinary terminal outcome. Generalize the permanent truncation,
saturated-length, and nonzero-descriptor-flags regressions over both public and
private representations. This milestone proves target construction and a
bounded sanitizer smoke, not CLI, benchmark, stable-matrix, or
interoperability admission.

## DD-684: Compact contextual rANS receives an explicit CLI selector

- Date: 2026-08-10
- Status: accepted

Add `lzss-contextual-rans-compact` as an independent experimental selector.
Keep `lzss-contextual-rans` bound to fixed-descriptor entropy variant 2 and
bind the new name only to compact variant 3. Do not auto-detect the variant,
change the default codec, or create a shorter ambiguous alias. Encode and
decode require the same explicit selector.

Use the same 65,536-byte frame, 393,216-decision, 786,440-byte payload, and
8 MiB aggregate-buffer policies as the fixed diagnostic profile, but call
only `marc_lzss_contextual_rans_compact_config_init`, its direction-specific
workspace query, compact factory, generic process, and destroy operations.
Obtain all storage extents and view alignment from that query. Reuse the
transactional file adapter so overwrite refusal, malformed input, strict
trailing data, and later failure never publish a destination. This remains an
experimental Format 2 selector outside the stable 42-profile inventory and
adds no benchmark or interoperability archive.

## DD-685: Compact contextual rANS receives a distinct benchmark profile

- Date: 2026-08-10
- Status: accepted

Add `lzss-contextual-rans-compact` to the dependency-free benchmark as an
experimental profile beside, not in place of, fixed
`lzss-contextual-rans`. Use the same 65,536-byte raw frame, `6F` decision,
`12F + 8` payload, and 8 MiB aggregate-buffer policies so descriptor
representation is the controlled difference. Construct each direction only
through the compact public configuration, requirements, and factory calls.

For raw input extent `N` and nonempty frame count `K`, reserve checked complete
stream capacity `112 + 12N + 9,097K`: 112 stream-header bytes, at most 12
payload bytes per raw byte, and per frame 64 common-header bytes, at most 9,025
compact descriptor bytes, and eight final-state bytes. Verify exact round trip
before timing and report encode/decode throughput, complete-stream ratio, peak
caller-owned workspace, and both direction-specific three-region workspaces.
The smoke test is descriptive evidence only; no performance threshold,
stable-matrix entry, or interoperability archive is added.

## DD-686: Schema 33 appends compact contextual rANS once

- Date: 2026-08-10
- Status: accepted

Freeze schema 32's exact 43-entry order and append only
`lzss-contextual-rans-compact` as archive 44. Set `schema_version` to 33 and
`codec_set` to `marc-cli-v33`. Do not add fixed-descriptor
`lzss-contextual-rans`, reorder a stable profile, or alter the shared 8,193-byte
binary fixture. The archive remains experimental and does not enter the stable
42-profile matrix.

The verifier must accept schemas 1 through 33 with their exact historical
codec sets and orders, enforce leaf names, sizes, hashes, unique codecs,
foreign decode equality, and byte-identical local re-encoding, and reject a
reordered schema-33 manifest. Compatibility testing must generate schema 33,
derive schema 32 by deleting only archive 44 and changing only manifest
identity, then traverse the unchanged schema-32-through-1 chain. Local
admission is not external interoperability evidence; the four-direction
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang exchange remains a
post-push requirement.

That release check completed at revision
`2c30be4da1a80d01103dac0ee82fb0c4889f3af4`: the Windows/MSVC and Ubuntu
24.04/Ninja artifacts verified on Ubuntu 26.04/Clang, and the Ubuntu 26.04
bundle verified locally and on Windows/MSVC. Every pass decoded and
byte-identically re-encoded all 44 archives.

## DD-687: Contextual tANS begins with a compact model descriptor

- Date: 2026-08-10
- Status: accepted

Retain typed LZSS variant 2 and `LzssFieldContext` variant 1, and reserve tANS
algorithm ID 5 variant 2 so the next experiment changes only the entropy axis.
Reuse variant 1's table log 12, 4,096-state table, live interval `[L,2L)`,
spread step 2,563, LSB-first additional bits, and terminal state `L`.

Give each used Symbol context its own frame-static normalized tANS table. Code
bypass bits in the same state through one implicit fixed 2,048/2,048 binary
table. Traverse operations and the bits within each bypass field in reverse on
encode so decode remains forward and LSB first. Charge 32 full transition
tables before decoder construction and preserve frame-atomic publication.

Do not repeat contextual rANS variant 2's fixed-descriptor experiment. Use a
24-byte tANS-specific prefix followed by contextual rANS variant 3's exact
canonical dense/sparse model records. This produces a 27 through 9,029-byte
descriptor, retains explicit final-valid-bit metadata, and keeps tANS payload
identity distinct. The decision reserves documentation and a one-Literal
vector only; it adds no implementation, public API, CLI, benchmark, readiness,
or interoperability claim.

## DD-688: Compact context-model records are a shared private primitive

- Date: 2026-08-10
- Status: accepted

Factor only active-mask analysis and canonical dense/sparse record parsing and
serialization out of contextual rANS variant 3. Keep the 20-byte rANS and
24-byte tANS prefixes, payload rules, valid-bit metadata, decoder-table limits,
error mappings, and public format identities in their backend-specific
modules. The shared primitive owns no state, frame, allocation, or backend ID.

Implement the contextual tANS descriptor over fixed local frequency and output
storage. Validate all fields and limits before publishing parsed state; encode
into a zeroed maximum-size local array before copying exact bytes to caller
output. Preserve compact rANS bytes and malformed-input categories through its
existing regression suite. This decision admits only the private descriptor
boundary, not transition tables, state coding, frames, or a public profile.

## DD-689: Contextual tANS decode tables occupy fixed caller-owned regions

- Date: 2026-08-10
- Status: accepted

Lay out 32 decode tables of exactly 4,096 entries: context table `C` begins at
`C * 4096` for `C=0..30`, and the implicit bypass table begins at
`31 * 4096`. Zero every inactive Symbol-context region. Always construct the
bypass region from frequencies 2,048/2,048. Report active Symbol contexts
separately from that mandatory table.

Use the existing tANS variant-1 spreading and transition builder as the only
table-construction authority. Snapshot all 4,518 descriptor frequencies and
preflight every active table plus bypass before changing caller output. Only
then populate the exact 131,072-entry prefix and publish its view; leave
storage and prior view unchanged on descriptor, limit, capacity, or table
failure. This milestone admits decode transitions only, not live-state
decoding, typed reconstruction, framing, or a public profile.

## DD-690: Contextual tANS decoding is operation-driven over one live state

- Date: 2026-08-10
- Status: accepted

Begin only after exact descriptor, payload extent, final-bit padding, initial
state offset, caller table capacity, and decoder limits pass. Decode a Symbol
request through its fixed context region and a bypass field through region 31,
advancing the same state for each decision. Bypass bits are returned least-
significant bit first and count as one event but their declared number of
decisions.

Require every request to match the fixed context alphabet and an active model.
Validate caller-owned transition entries at use time so mutation cannot escape
the live interval or read beyond declared bits. Make errors sticky and assign a
requested value only after its transition succeeds. Finish requires exact
event and decision counts, every active context requested, terminal state
4,096, and exact bit consumption. This remains a private entropy boundary; it
does not reconstruct typed LZSS tokens or admit a frame or encoder.

## DD-691: Contextual tANS reconstructs typed LZSS through a two-pass bridge

- Date: 2026-08-10
- Status: accepted

Drive the contextual tANS decoder directly from `LzssFieldContextState` and
construct each Literal or Match locally; do not materialize an intermediate
modeled-operation array. Validate dictionary parameters, declared token/event/
decision/raw counts, aggregate output, and the complete entropy termination
before publishing tokens. Validate each reconstructed token against current raw
history and configured LZSS limits before advancing context state.

Use the existing validate-then-write two-pass policy. Require exact
131,072-entry table storage, exact declared token storage, and pairwise
disjoint payload/table/token regions before either pass. The write pass must
match the validation pass in token/raw/event/decision/bit counts or report an
internal error. This boundary emits private typed tokens only; raw
reconstruction, frame parsing, streaming, and encoding remain separate.

## DD-692: Contextual tANS complete-frame decoding is one atomic transaction

- Date: 2026-08-10
- Status: accepted

Give entropy identity `5/2` its own Format 2 stream-header parser and serializer
while retaining the common 112-byte layout. Preflight the 64-byte frame header,
27 through 9,029-byte descriptor, payload extent, exact counts, sequence,
stream remainder, decoder limits, and complete serialized extent before
workspace admission. Parse and retain the validated descriptor once.

Require caller storage for 131,072 transitions, the declared token count, and
the declared raw extent. Reject every pairwise overlap among the exact
serialized frame, tables, tokens, and raw output. Decode typed tokens and then
reconstruct raw bytes only after all capacity and overlap checks; report frame
consumption only after both stages succeed. Bytes after the one preflighted
frame remain unconsumed. This admits a private complete-frame decoder, not a
streaming lifecycle or encoder.

## DD-693: Contextual tANS streaming buffers exactly one atomic frame

- Date: 2026-08-10
- Status: accepted

Add a private immutable-direction `core::Transform` for entropy identity
`5/2`. Collect the 112-byte stream header and each 64-byte frame header
incrementally, validate declared extents before workspace admission, then
buffer exactly one descriptor-plus-payload body in caller-owned storage. Run
the complete-frame decoder only after the entire frame is present. Drain its
raw frame through arbitrary output capacities before accepting the next frame.

Require disjoint serialized/table/token/raw workspaces at construction and
disjoint caller output versus every private workspace on every call. Enforce the
aggregate buffered-byte limit before body collection. `ResetBlock`, unknown
flags, premature `EndInput`, trailing bytes, workspace shortage, malformed
frames, and aliasing become sticky errors. `Flush` does not alter framing.
Repeated calls after successful end return `EndOfStream`. This lifecycle adds
no representation, encoder, public API, CLI, benchmark, or interoperability
archive.

## DD-694: Contextual tANS operation encoding owns inverse transition tables

- Date: 2026-08-10
- Status: accepted

Build each used Symbol context's static model from a complete validated
`ModeledOperation` sequence. Normalize observed counts independently to 4,096
with the established integer-error rule and leave unused context slices zero.
Bypass bits use the fixed 2,048/2,048 model and contribute decisions but not
context frequencies.

Construct caller-owned flattened inverse-state tables from marc's canonical
standalone tANS tables. Store 4,096 `uint16` encode states for each of the 31
field contexts plus the bypass context; derive symbol offsets from the
descriptor frequencies. Snapshot and validate the descriptor and every
canonical table before publishing any caller table entry. Starting at terminal
state 4,096, encode operations in
reverse logical order. Within bypass operations encode bit indexes high to low.
Write each transition's additional bits backward so forward decoding remains
LSB-first, then serialize the initial state offset little-endian.

Planning may mutate only the private encode-table workspace. Publish neither
descriptor nor payload until operation validation, normalization, table
construction, exact bit sizing, format validation, limits, capacity, and
operation/table/payload disjointness succeed. This reference boundary adds no
typed-token bridge, frame encoder, streaming encoder, or format change.

## DD-695: Direct contextual tANS token encoding reconstructs contexts backward

- Date: 2026-08-10
- Status: accepted

Validate the complete typed-LZSS frame before model or state construction.
Walk tokens forward to feed token-kind, Literal, match-length class, distance
class, and bypass decisions into the shared contextual-tANS model builder.
Count one event per Symbol or multi-bit bypass request and one decision per
Symbol or bypass bit.

After building caller-owned inverse tables, walk tokens backward without a
materialized operation array. Derive the token and length contexts from the
immediate predecessor kind, and the Literal context from the nearest preceding
Literal maintained monotonically during reverse traversal. Emit each token's
fields in reverse modeled order; the shared writer reverses bits within bypass
requests. Require token, table, and payload regions to be pairwise disjoint
before table or payload writes. Planning may mutate only table scratch;
descriptor and payload publish only after exact counts, state, bit size,
format, limits, capacity, and second-pass agreement. This adds no dictionary
parser, frame encoder, streaming encoder, or format change.

## DD-696: Contextual tANS frame encoding is a four-region transaction

- Date: 2026-08-10
- Status: accepted

Compose one complete nonempty Format 2 frame from raw input, typed-LZSS token
staging, contextual-tANS inverse-table staging, and serialized frame output.
Require the four regions to be pairwise disjoint before token or table writes.
Planning may materialize private tokens and inverse tables, but must not touch
serialized output. Validate the stream identity, exact expected frame input,
token frame, entropy descriptor, frame header, exact descriptor/payload extent,
and aggregate workspace limit before reporting a serialized size.

Encoding repeats only the direct entropy pass required to obtain the final
descriptor and payload, then serializes the canonical variable-size descriptor
and 64-byte frame header into the already admitted destination. Require exact
agreement with all planned token, event, decision, descriptor, payload, and
serialized sizes. The raw, token, inverse-table, and exact serialized-frame
bytes all count toward `max_internal_buffered_bytes`. This adds no streaming
encoder, public API, CLI selector, benchmark, fuzz target, interoperability
archive, or serialized representation.

## DD-697: Contextual tANS streaming encoding drains immutable frame bytes

- Date: 2026-08-10
- Status: accepted

Add a private immutable-direction transform for entropy identity `5/2`. Emit
the canonical 112-byte stream header first, collect exactly one bounded raw
frame, invoke DD-696 once that frame is complete, and then drain the immutable
serialized frame under arbitrary output capacity before collecting more raw
input. Preserve a nonterminal `Flush`; reject `ResetBlock` and unknown flags;
retain `EndInput` across header and frame drain; and return stable
`EndOfStream` after the declared original size is fully emitted.

Keep raw-frame, typed-token, inverse-table, and serialized-frame workspaces
pairwise disjoint at construction. Require caller output to be disjoint from
all four on every call. Empty streams require none of the frame workspaces;
nonempty capacity and aggregate-limit failures become sticky only when the
first affected frame is prepared. Map capacity to `out_of_memory`, configured
limits to `limit_exceeded`, protocol mismatches to `invalid_argument`, and all
unexpected composition failures to `internal_error`. This adds no public API,
profile calculator, CLI selector, benchmark, fuzz target, or interoperability
archive.

## DD-698: Contextual tANS profiles own direction-specific typed layouts

- Date: 2026-08-10
- Status: accepted

Define a private profile over the existing `5/2` stream configuration. Bound
one frame by at most six modeled decisions per raw byte, at most 12 coded bits
per decision, two initial-state bytes, the 9,029-byte maximum canonical
descriptor, and the 64-byte frame header. This is a conservative allocation
ceiling, not a claim that every decision consumes 12 bits.

For encoding, lay out typed tokens followed by 131,072 `uint16_t` inverse-
state entries with checked alignment. For decoding, lay out 131,072
`TansDecodeEntry` transitions followed by typed tokens. Return explicit offsets,
element counts, total byte extents, and maximum alignment; partition only an
exactly self-consistent requirement record and publish no partial views on
failure. Charge raw-frame, exact typed views, and serialized-frame regions
together against the internal-buffer limit. Empty known-size streams retain
zero frame/view requirements and alignment one. Decoder sizing derives only
from local hard limits. This admits private constructors but no public ABI,
CLI, benchmark, fuzz, or interoperability entry.

## DD-699: Contextual tANS public construction keeps typed layouts opaque

- Date: 2026-08-10
- Status: accepted

Add a distinct size-tagged ABI-1 configuration and `config_init`, direction-
specific `workspace_requirements`, and `create` functions for the existing
Format 2 contextual-tANS identity `5/2`. Reuse the common transform process
and destroy functions. This additive family does not change the ABI version
and is not an alias for byte-oriented LZSS plus tANS or either contextual-rANS
variant.

Expose only three caller-owned byte regions and a maximum alignment. Encoding
maps primary to raw input, secondary to the complete frame, and views to tokens
then `uint16_t` inverse tables. Decoding maps primary to serialized input,
secondary to raw output, and views to `TansDecodeEntry` tables then tokens.
Validate configuration tags, limits, capacities, alignment, and all region-
prefix overlaps before publishing a handle; partition typed views only inside
C++. Keep allocator callbacks, CLI selection, completion, fuzzing, benchmarks,
and interoperability outside this milestone.

## DD-700: Contextual tANS completion is proven only through ABI 1

- Date: 2026-08-10
- Status: accepted

Audit the single contextual-tANS representation `5/2` through only its public
configuration, requirements, factory, process, and destroy functions. Cover
empty input, every one-byte value, all byte values, repeated and patterned
binary data, deterministic generated data, 63/64/65-byte boundaries, and
multiple frames. Encode each class twice and require byte equality.

For a four-frame stream, require one-byte and two mixed input/output schedules
to reproduce the one-shot stream and raw output exactly. Corrupt the final
frame sequence, truncate its final byte, and append trailing data independently.
Earlier complete frames may remain committed, but the final raw byte must stay
untouched and each terminal error must repeat with the same category and
position. This completes the public lifecycle audit without adding CLI,
benchmark, fuzz, interoperability, or format changes.

## DD-701: Contextual tANS fuzzing is bounded at two decoder boundaries

- Date: 2026-08-10
- Status: accepted

Retain permanent regressions for every strict prefix of a repository-generated
single-frame `ABABX` stream, all-ones frame extent fields, and a nonzero byte
in the descriptor's reserved table-log padding. Require both the private
complete-frame decoder and public ABI-1 streaming decoder to preserve sentinel
raw output; require the public terminal error and position to remain sticky.

Add one libFuzzer entry point that truncates supplied input to 32 KiB. Fix raw
publication at 4 KiB, a frame at 1 KiB, decisions at 6,144, payload at 9,218
bytes, descriptor at 9,029 bytes, decoder transitions at 131,072 entries, and
typed tokens at 1,024 records. Store large workspaces thread-locally, admit
them under one compile-time aggregate limit, derive chunks only within fixed
bounds, and abort on contract violations or call-budget exhaustion. Compile
the harness warning-clean in ordinary builds; do not claim a sanitizer
campaign merely from compile smoke.

## DD-702: Contextual tANS sanitizer smoke is process-local evidence

- Date: 2026-08-10
- Status: accepted

Execute the contextual-tANS harness for exactly 1,000 inputs with maximum
input 32 KiB, per-input timeout five seconds, and RSS limit 512 MiB. Use the
Clang 22 ASan/UBSan/libFuzzer executable already built from the bounded target,
and prepend that same toolchain's sanitizer runtime directory only to the
campaign process. Supply no persistent corpus; direct any failure artifact to
the ignored build tree.

Record crashes, hangs, sanitizer findings, and peak RSS exactly. A clean run
is evidence only for executed mutations, not proof of memory safety or format
correctness, and does not replace permanent regressions. The campaign changes
no format, API, implementation, limits, or source corpus.

## DD-703: Contextual tANS benchmark admission stays descriptive

- Date: 2026-08-10
- Status: accepted

Add `lzss-contextual-tans` only to the experimental benchmark inventory. Use
the public ABI-1 lifecycle in both directions, a 65,536-byte raw frame, six
modeled decisions per raw byte, the 12-bit transition ceiling, the 9,029-byte
descriptor maximum, two state bytes, and an 8-MiB aggregate policy. Checked
complete-stream capacity is `112 + 9N + 9,095K` for raw extent `N` and
nonempty frame count `K`.

Require an exact round trip before timing and report complete-stream ratio,
both directional workspace regions, peak caller-owned workspace, and
throughput under the existing measurement contract. Compare the same local
input with byte-stream LZSS+tANS, compact contextual rANS, and contextual
Dynamic Range, but treat a single small-input run as descriptive evidence and
derive no default-profile recommendation from it.

## DD-704: Contextual tANS CLI selection is explicit and public-only

- Date: 2026-08-10
- Status: accepted

Add `lzss-contextual-tans` as an experimental CLI selector outside the stable
42-profile inventory. Configure both immutable directions with the public
ABI-1 initializer, 65,536-byte frames, `6F` decisions, `9F + 2` payload bytes,
an 8-MiB internal-buffer limit, and the existing LZSS distance and match
limits. Obtain all three workspace extents and alignment from the public query
and create the transform only through the public factory.

Require the same selector for encode and decode; do not add auto-detection or
private token/table knowledge to the command-line layer. Register exact file
round trip, entropy identity `5/2`, temporary-output commit, and strict
trailing-data rejection. This changes no stream bytes, C ABI, stable profile
count, or default codec.

## DD-705: Interoperability schema 34 appends contextual tANS

- Date: 2026-08-10
- Status: accepted

Freeze schemas 1 through 33 unchanged and append
`lzss-contextual-tans.marc` as archive 45. Set `schema_version` to 34 and
`codec_set` to `marc-cli-v34`. The generator must round-trip the shared 8,193-
byte binary fixture before recording the archive size and SHA-256.

Require the verifier to enforce the exact 45-entry order, leaf names, sizes,
digests, fixture decode, and byte-identical local re-encoding. Reject a
reordered schema-34 manifest. Derive schema 33 by removing only archive 45 and
changing only the schema identity, then validate every earlier schema through
the existing chain. External cross-platform evidence remains separate from
local schema admission.

## DD-706: Schema 34 external admission requires four exact paths

- Date: 2026-08-11
- Status: accepted

Admit external schema-34 evidence only after the pushed Windows/MSVC and
Ubuntu 24.04/Ninja CI artifacts both verify on the independent Ubuntu 26.04
Clang environment, that environment generates and self-verifies its own
bundle, and Windows/MSVC verifies the Ubuntu-generated bundle. Require every
path to report the same full Git revision and all 45 archives.

Each verifier success covers manifest identity and order, file sizes and
SHA-256, exact fixture reconstruction, and byte-identical local re-encoding.
This is evidence for the recorded Windows and WSL2 Linux x86-64 environments;
it does not generalize to untested architectures or native Linux kernels.

## DD-707: Contextual Blocked Huffman begins as a measured descriptor probe

- Date: 2026-08-11
- Status: accepted

Evaluate a future `Contextual Blocked Huffman` backend over the existing LZSS
typed-field context variant 1. Do not call it Adaptive Huffman: every candidate
collects a bounded frame before constructing static canonical tables, whereas
marc's public Adaptive Huffman name denotes the synchronized FGK tree. The
RFC 1951 distinction between literal/length and distance alphabets informs the
field separation only; marc retains its own token-kind, literal, length-class,
distance-class, and raw bypass-bit operations and makes no DEFLATE compatibility
claim.

Before reserving an entropy variant or serialized frame, compare three exact
cost strategies: four pooled field tables; one table per active one of the 31
deterministic contexts; and the same contextual tables with identical
alphabet/code-length vectors stored once and selected by a 31-byte map. Every
strategy charges bypass bits unchanged and byte-aligns the combined payload.
Single-symbol tables store the symbol and consume zero payload bits.

For this probe only, charge an eight-byte descriptor prefix. Each active model
costs a four-byte record prefix. A canonical record then chooses the smaller
of dense four-bit code lengths (`ceil(alphabet/2)` bytes) and sparse
`(symbol,length)` pairs (two bytes per nonzero symbol). The shared strategy
also charges its 31-byte map. These byte counts are a reproducible design
instrument, not a decoder-visible representation, and therefore do not alter
`docs/format.md`.

The initial repository README measurement shows why the format must not yet be
fixed. Four pooled tables cost 2,320 bytes; 31 contextual tables cost 2,692;
and identical-table sharing costs 2,718. Contexts reduce modeled symbol bits
from 14,763 to 13,688, but the provisional descriptor grows from 166 to 673
bytes. The next design must therefore permit model selection or sharing at a
finer granularity rather than requiring all available contexts. This probe
adds no encoder, decoder, format identity, C API, CLI codec, readiness claim,
or interoperability archive.

## DD-708: Selective contextual tables must repay their complete record cost

- Date: 2026-08-11
- Status: accepted

Extend only the non-serializing DD-707 probe with four complete pooled field
models plus optional context overrides. The eight-byte provisional prefix has
room for a 31-bit override mask, so selecting a context adds no separate map
byte. Store selected override records in ascending context-ID order. Keep each
pooled model built from the complete field histogram, including symbols later
coded through overrides; this makes every context decision independent,
deterministic, and bounded rather than a combinatorial subset search.

For an active context, calculate its symbol bits once under the appropriate
pooled table and once under its own table. Select the override only when the
symbol-bit saving is strictly greater than eight times the complete individual
model-record byte count. A tie retains the pooled table. Bypass bits are never
eligible and remain one shared LSB-first payload. The exact final payload is
still byte-aligned only after all modeled and bypass bits are summed.

On the 4,326-byte README no override repays its record, so the selective result
equals the 2,320-byte pooled result. On the 312,817-byte format specification,
nine overrides reduce symbol bits from 235,043 to 231,131 while growing the
descriptor from 166 to 516 bytes. With 299,780 unchanged bypass bits, the
stored estimate falls from 67,019 to 66,880 bytes. Full contextualization costs
67,147 bytes. Selective admission therefore behaves safely on both measured
scales, but this remains evidence for a possible representation rather than a
format reservation, encoder, decoder, public API, CLI codec, or readiness
claim.

## DD-709: Contextual Blocked Huffman reserves Format 2 entropy variant 2

- Date: 2026-08-11
- Status: accepted

Reserve entropy algorithm/variant `2/2` behind typed LZSS `2/2` and
LzssFieldContext `1/1`. Replace the probe's underspecified eight-byte prefix
with the normative 16-byte descriptor prefix required to cross-check decision
count, payload extent, override mask, final valid bits, maximum length, field
mask, and flags. The uniform eight-byte increase changes measured extents but
not any override decision.

Use four inferred pooled alphabets and ascending context overrides. Encode a
one-symbol table as a four-byte zero-bit Single record. Encode every other
complete length-limited table through the canonical sparse/dense record chosen
by `2K < ceil(A/2)`, with equality dense. Keep Symbol codes and LSB-first bypass
bits interleaved in operation order. Permit an empty payload only when all
modeled symbols select Single records and no bypass bit exists.

The descriptor parser and serializer are the first implementation boundary.
They use fixed local staging, validate masks and exact frame-supplied counts,
reject invalid or noncanonical tables and trailing bytes, charge at most 35
bounded decode tables, and publish only after complete success. They do not
decode entropy payload, parse a frame, or admit a public profile.

## DD-710: Contextual Blocked Huffman decoding is request-driven

- Date: 2026-08-11
- Status: accepted

Implement the first payload boundary as an entropy-layer state machine rather
than coupling it directly to LZSS reconstruction. The caller supplies each
expected context/alphabet or bypass width in modeled-operation order. This
keeps context evolution independently testable and gives later typed-LZSS
integration the same shape as contextual range, rANS, and tANS.

Build only non-Single Huffman tables into caller-owned fixed workspace. Retain
the descriptor validator's conservative 35-table limit charge, while allowing
zero table entries for an all-Single frame. Select a serialized override before
its pooled field model and require every override to be used at completion;
do not require pooled models to be used because complete pooled histograms are
retained even when all decisions for a field are overridden.

Use one forward LSB-first cursor for canonical codes and raw bypass bits.
Publish a decoded value only after the entire request succeeds. Completion
requires exact event count, decision count, override use, and valid-bit extent.
This milestone does not reconstruct typed tokens, parse frames, or admit the
public profile.

## DD-711: Contextual Blocked Huffman token inversion is two-pass

- Date: 2026-08-11
- Status: accepted

Connect the request-driven entropy decoder to `LzssFieldContextState` using the
same write-free validation then publication transaction as the other
contextual backends. The first pass reconstructs and validates every literal or
match, advances context only after a valid token, checks entropy completion and
raw size, and writes no caller token. Only then may the second deterministic
pass publish the complete typed-token sequence.

Derive table workspace from active non-Single models rather than requiring the
35-table ceiling. This preserves empty workspace for the documented all-Single
one-Literal vector while retaining the format validator's conservative limit
charge. Reject short workspace and pairwise overlap among payload, used tables,
and token output before building a table. Preserve all token bytes on every
prewrite failure.

This boundary validates typed LZSS tokens and declared frame counts but does
not reconstruct raw bytes, parse a Format 2 frame, or expose the profile.

## DD-712: Contextual Blocked Huffman frames preflight before reconstruction

- Date: 2026-08-11
- Status: accepted

Add a private Format 2 stream/frame format boundary for entropy ID/variant
`2/2`. Reuse the already validated common Format 2 prefix and dictionary/context
layout by translating only the fixed entropy identity and parameter region;
validate maximum length 15, four pooled fields, 31 contexts, model-record
version 1, and zero flags/reserved bytes independently.

Validate the frame's sequence, exact expected raw extent, count inequalities,
24..2,561 descriptor extent, zero-through-ceiling payload extent, zero optional
features, and all decoder limits before parsing the descriptor. The complete
decoder requires pairwise-disjoint serialized input, exact used tables, typed
tokens, and raw output. Publish consumed size only after token inversion and raw
reconstruction both succeed. Leave streaming and profile admission for later.

## DD-713: Contextual Blocked Huffman streaming buffers one bounded frame

- Date: 2026-08-11
- Status: accepted

Build the private streaming decoder from the complete-frame transaction. Parse
the 112-byte stream header and each 64-byte frame header incrementally, collect
only the validated `header || descriptor || payload` extent, decode it into a
private raw-frame buffer, and drain committed raw bytes under the core process
contract. Preserve `EndInput` while a final frame drains and reject truncation,
trailing bytes, `ResetBlock`, unknown flags, and all output/workspace aliases.

Unlike contextual tANS, Contextual Blocked Huffman has a descriptor-dependent
number of decode tables. Do not require the conservative 35-table ceiling at
construction or frame-header time. After the complete descriptor is buffered,
preflight it, derive the exact non-Single table count, and then validate table
capacity and the aggregate serialized/table/token/raw buffered-byte limit before
building any table. This preserves zero table workspace for all-Single frames.

This milestone changes no byte representation and does not admit an encoder,
public profile, C API, CLI selector, benchmark codec, or interoperability
archive.

## DD-714: Contextual Blocked Huffman encoding selects strict-profit overrides

- Date: 2026-08-11
- Status: accepted

Build the first encoder boundary over a caller-supplied modeled-operation span.
Gather the four pooled field histograms and all 31 context histograms in fixed
bounded storage. Construct deterministic length-limited canonical models, then
select a context override only when its symbol-bit saving is strictly greater
than its canonical model-record size in bits. Equality retains the pooled model.

Single records consume zero payload bits. Other symbols emit their canonical
LSB-first code, and bypass fields emit their numeric bits LSB-first into the same
forward cursor. Plan before writing, zero the exact payload extent, preserve the
descriptor on every failure, and reject operation/payload aliasing. This
milestone does not yet connect typed LZSS tokens or emit a frame.

## DD-715: Typed LZSS encoding regenerates contexts without operation storage

- Date: 2026-08-11
- Status: accepted

Split the entropy encoder into a fixed-capacity model builder and a forward
writer. The typed-LZSS adapter validates the complete token frame, walks tokens
once to feed the builder while advancing `LzssFieldContextState`, then walks the
same immutable tokens again to feed the writer under the identical state
transition. It allocates and accepts no modeled-operation workspace.

Require the direct and operation-level boundaries to serialize identical
descriptors and payloads. Count only represented bypass fields as events,
cross-check event and decision totals at writer completion, reject token/payload
overlap before planning, and publish the descriptor only after the second pass
succeeds. Frame serialization remains a later milestone.

## DD-716: Complete Contextual Huffman frames are planned before publication

- Date: 2026-08-11
- Status: accepted

Compose raw LZSS parsing and the direct typed-token entropy encoder behind a
private complete-frame boundary. Require the raw extent to equal the next
frame implied by the validated stream header, keep raw input, used typed-token
storage, and serialized output pairwise disjoint, and admit their aggregate
size under `max_internal_buffered_bytes`.

Planning fixes token, event, decision, descriptor, payload, and complete-frame
sizes and validates the resulting descriptor and frame header before any
serialized output is written. Encoding writes payload first, descriptor
second, and the 64-byte header last. A short destination or any preflight
failure therefore preserves every serialized-output byte. This milestone does
not add a streaming encoder or admit the profile publicly.

## DD-717: Streaming Contextual Huffman encoding retains one complete frame

- Date: 2026-08-11
- Status: accepted

Wrap DD-716 in a private forward-only transform. Serialize and drain the
112-byte stream header first, collect at most one exact raw frame, invoke the
complete-frame encoder, then drain that retained serialized frame before
accepting bytes for its successor. Retain only caller-owned raw, typed-token,
and serialized-frame workspaces; Contextual Blocked Huffman needs no external
encode-table workspace.

`Flush` does not close a partial outer frame. `EndInput` is accepted only when
the current call supplies every remaining byte promised by `original_size`,
remains latched while headers or frames drain, and reaches `EndOfStream` only
after the last serialized byte is emitted. Reject `ResetBlock`, unknown flags,
excess or premature input, construction/output aliasing, and preparation
failures as sticky errors. Empty input emits only the stream header. Public
profile admission remains a later milestone.

## DD-718: The private profile derives conservative bounded workspaces

- Date: 2026-08-11
- Status: accepted

Define one private profile over the fixed Format 2 `2/2` identity. For an
encoder frame of `F` raw bytes, reserve `F` typed tokens, the 2,561-byte maximum
descriptor, and at most `ceil(6F * 15 / 8)` payload bytes. Charge raw input,
typed tokens, and the complete serialized frame together. Empty input requires
no frame workspace.

For decoding, reserve the caller-limit frame extent, 35 worst-case non-Single
Huffman tables, one token per raw byte, and one raw publication frame. Align
typed views explicitly and reject forged, short, or misaligned partitions
transactionally. Keep the existing LZSS `2/2` bounds and map stable profile
errors to the core error categories. This profile remains private and does not
yet add a public C lifecycle or completion claim.

## DD-719: Contextual Blocked Huffman enters ABI 1 through opaque workspaces

- Date: 2026-08-11
- Status: accepted

Expose the private DD-718 profile as a distinct additive C function family.
Use a fixed-width, size-tagged configuration with immutable direction and the
same frame, LZSS, and hard-limit fields as the other contextual profiles.
Require callers to query three direction-specific regions before construction:
encoder raw input, serialized frame, and typed tokens; or decoder serialized
frame, atomic raw output, and aligned tables followed by tokens.

Do not expose C++ view layouts or object placement. The factory validates
configuration tags, reserved fields, capacity, required alignment, and
pairwise used-prefix non-overlap before partitioning or publishing a transform.
Retain the common process/destroy and sticky-error contracts and the unchanged
Format 2 dictionary `2/2`, entropy `2/2` identity. Treat completion, CLI,
benchmark, fuzzing, and interoperability as later, independently audited
milestones.

## DD-720: Contextual Blocked Huffman completion is audited through ABI 1

- Date: 2026-08-11
- Status: accepted

Define public completion solely in terms of the DD-719 C lifecycle. Require
deterministic round trips for empty input, every single-byte value, the complete
byte alphabet, long runs, repeated binary patterns, deterministic pseudo-random
data, and lengths immediately around the 64-byte audit frame boundary. Require
the same serialized stream under whole-buffer and three mixed input/output
chunk schedules, plus stable repeated `EndOfStream`.

Size the audit output by the conservative per-frame ceiling
`64 + 2,561 + ceil(6F * 15 / 8)`. For a malformed fourth frame, permit the
three preceding frames to remain committed but require its final raw byte to
remain untouched. Apply this rule independently to sequence corruption,
truncation, and strict trailing data, and require the first error category and
position to remain stable on repeated calls. This audit changes no stream,
factory, CLI, benchmark, fuzz, or interoperability surface.

## DD-721: Contextual Blocked Huffman fuzzing keeps two bounded decode paths

- Date: 2026-08-11
- Status: accepted

Give permanent malformed-stream regressions and one fuzz entry point the same
two independent targets: the private complete-frame decoder after a validated
112-byte stream header, and the public ABI-1 streaming decoder. Seed permanent
tests from a locally encoded `ABABX` stream and require every strict truncation,
extreme frame-count/extent fields, and nonzero descriptor flags to preserve raw
output atomically in both paths.

Bound the harness before decoding: inspect at most 32 KiB, admit at most a
1,024-byte raw frame, 6,144 decisions, 11,520 payload bytes, 35 Huffman decode
tables, 4 KiB total raw output, fixed caller-owned workspaces, and a finite
call count. Drive public input/output chunks deterministically from the bounded
input. Abort only on violated process/workspace invariants; ordinary malformed
input is an expected terminal result. This milestone adds no corpus, campaign
claim, stream change, CLI, benchmark, or interoperability surface.

## DD-722: The first Contextual Huffman sanitizer run is finite and ephemeral

- Date: 2026-08-11
- Status: accepted

Execute DD-721's already reviewed harness under the repository's established
Windows Clang 22 GNU-driver sanitizer tree. Build only the dedicated target and
select its matching `lib/windows` runtime through a process-local `PATH`.

Run exactly 1,000 in-memory inputs with 32 KiB maximum length, five seconds per
input, and 512 MiB RSS limit. Supply no corpus, create no persistent seed, and
direct failure artifacts only to the ignored build tree. Record the final
coverage/features, peak RSS, and absence or presence of libFuzzer, ASan, or
UBSan findings. This bounded execution is evidence only for exercised inputs
and cannot establish exhaustive safety or change any release surface.

## DD-723: Contextual Blocked Huffman enters the experimental benchmark only

- Date: 2026-08-11
- Status: accepted

Add `lzss-contextual-blocked-huffman` as an explicit experimental benchmark
selector without changing the stable 42-profile inventory, CLI, or
interoperability schema. Use 65,536-byte raw frames, admit at most `6F` typed
decisions and `12F` payload bytes per frame, retain the 2,561-byte descriptor
ceiling, and apply the established 8-MiB aggregate-buffer policy.

For raw input extent `N` and nonempty frame count `K`, reserve complete output
with checked arithmetic using `112 + 12N + 2,625K`; the per-frame term covers
the 64-byte common header and maximum descriptor. Construct each immutable
direction only through the public ABI-1 configuration, requirements query,
and factory. Require an untimed byte-exact round trip before measuring either
direction, then report complete-stream ratio, encode/decode throughput, peak
caller-owned workspace, and every direction-specific workspace region.
Measurements and the registered smoke test are descriptive, not thresholds.

## DD-724: Contextual Blocked Huffman CLI admission stays explicit

- Date: 2026-08-11
- Status: accepted

Add `lzss-contextual-blocked-huffman` as an explicit experimental CLI selector
for both encode and decode. Use the benchmark's 65,536-byte raw frame, `6F`
decision, `12F` payload, 2,561-byte descriptor, and 8-MiB aggregate policies.
Construct the transform only through the public ABI-1 configuration,
direction-specific requirements query, factory, process, and destroy calls;
the CLI must not reproduce typed-token or Huffman-table layouts.

Register nonempty and empty file round trips, require emitted Format 2 entropy
identity `algorithm=2, variant=2`, reject malformed input and strict trailing
data without retaining output, and refuse overwrite as for every other CLI
profile. The selector is not auto-detected, does not enter the stable
42-profile inventory, and does not yet change interoperability schema 34.

## DD-725: Schema 35 appends Contextual Blocked Huffman once

- Date: 2026-08-11
- Status: accepted

Advance the current interoperability manifest to schema 35 and codec set
`marc-cli-v35`. Preserve schema 34's exact 45-archive order and append
`lzss-contextual-blocked-huffman` as archive 46. The generator must round-trip
the common 8,193-byte fixture before recording its size and SHA-256. The
verifier must require the exact 46-entry order, decode equality, and
byte-identical local re-encoding.

Extend compatibility testing by rejecting a reordered schema-35 manifest,
then derive schema 34 by removing only archive 46 and restoring version 34 and
`marc-cli-v34`. Continue deriving and verifying schemas 33 through 1 without
changing any historical profile set. This milestone records local generation
and verification only; cross-platform four-direction evidence requires the
same pushed revision and remains separate.

## DD-726: Standard-library facilities require their defining headers

- Date: 2026-08-11
- Status: accepted

Treat each translation unit as responsible for directly including the standard
header that declares every facility it names. In particular,
`std::in_range` requires `<utility>` even when a Windows standard-library
include graph happens to expose it transitively. Fix the Contextual Blocked
Huffman complete-frame decoder without changing control flow, stream bytes,
limits, or public ABI, and audit every repository `std::in_range` use for the
same omission.

## DD-727: Schema 35 external evidence requires four matching directions

- Date: 2026-08-11
- Status: accepted

Admit schema-35 external evidence only when the pushed Windows/MSVC and Ubuntu
24.04/Ninja CI bundles verify on Ubuntu 26.04/Clang, and a bundle generated by
that Ubuntu 26.04 executable verifies both locally and with Windows/MSVC.
Require every path to report the same full Git revision, producer label, and
all 46 archives. Record exact decode and byte-identical re-encoding evidence
without importing any generated bundle into the repository.

## DD-728: Contextual Adaptive Huffman uses alphabet-bounded FGK trees

- Date: 2026-08-11
- Status: accepted

Reserve `lzss-contextual-adaptive-huffman` as Format 2 dictionary `2/2`,
context model `1/1`, entropy `1/2`. Give each of the 31 fixed Symbol contexts
an independent FGK tree whose capacity and initial numbering derive from that
context's alphabet. Encode a new symbol using the NYT path followed by exactly
`ceil(log2(alphabet))` LSB-first raw bits, rejecting unused numeric values.

Interleave raw bypass bits in the same forward cursor without tree updates and
reset every tree at the outer-frame boundary. Bound raw frames to 2^24 bytes,
so unsigned 32-bit weights need no mid-frame rescaling. Use one fixed 16-byte
descriptor with decision count, payload size, context count, final-valid-bit
count, and zero flags/reserved fields. Reserve the exact one-Literal bytes but
defer parser, tree generalization, coding, public admission, and tools to
separate milestones.

## DD-729: Contextual Adaptive Huffman foundations keep storage caller-owned

- Date: 2026-08-11
- Status: accepted

Implement the entropy `1/2` descriptor as an independent fixed-size parser and
serializer with transactional output. Implement one contextual FGK tree as a
view over caller-supplied `AdaptiveHuffmanNode` and symbol-index spans. Admit
only alphabets 2 through 256 and consume exactly the `2A+1` and `A` prefixes
after validating both capacities.

Preserve Adaptive Huffman variant 1 unchanged. Require the 256-symbol
contextual tree to reproduce its paths and updates, while smaller trees derive
their root order and storage from their fixed alphabet. Detect invalid
initialization, symbols, paths, tree relationships, and weight overflow. Do not
add bit coding or allocate the 31-tree collection in this milestone.

## DD-730: Contextual Adaptive Huffman decoding commits whole operations

- Date: 2026-08-11
- Status: accepted

Partition caller-owned storage into the fixed schema's 9,067 nodes and 4,518
symbol indices and reject any short or overlapping regions before tree reset.
Expose a private operation decoder whose caller supplies the expected context,
alphabet, or bypass width. Keep a request-local bit offset until the complete
path, NYT raw value, and FGK update succeed; publish neither offset nor value
on failure.

Charge the 13,585 model entries to `max_entropy_table_entries` and charge the
payload plus both used model regions to `max_internal_buffered_bytes`. Validate
padding at begin and all trees plus exact counts and valid-bit exhaustion at
finish. Avoid a whole-tree validation in the per-Symbol hot path. Leave LZSS
context inference, frame parsing, streaming, encoding, and public admission to
later milestones.

## DD-731: Contextual Adaptive Huffman token publication uses two passes

- Date: 2026-08-11
- Status: accepted

Place a private LZSS typed-token adapter above the Contextual Adaptive Huffman
operation decoder. Derive every expected context and alphabet solely from the
established `LzssFieldContextState`; reconstruct class and bypass values before
calling the common typed-token validator; and require declared token, event,
decision, and raw extents to agree at completion.

Perform a complete write-free validation pass before decoding again into the
caller-owned token span. Reject short output only after the first pass proves
the stream valid, and reject payload, node, symbol, and token aliases before
decoding. Charge exact token and model storage plus payload to the aggregate
limit. Defer raw-byte reconstruction, frame parsing, encoding, streaming, and
public profile admission.

## DD-732: Contextual Adaptive Huffman preflights complete frame extents

- Date: 2026-08-11
- Status: accepted

Implement the reserved Format 2 stream identity `dictionary=2/2,
entropy=1/2, context=1/1` as a private parser and serializer. Require the
112-byte stream header's entropy region to contain the fixed Symbol-event
ceiling, 31 contexts, eight-bit maximum NYT raw width, zero flags, and zero
reserved bytes. Preserve the 2^24 raw-frame ceiling and exact LZSS parameter
bounds.

Validate the common 64-byte frame header before slicing its fixed 16-byte
descriptor and exact payload. Require nonempty canonical frames, consistent
token/event/decision/raw counts, zero optional side data and checksum trailer,
all decoder limits, exact descriptor/header agreement, checked serialized
extent, and transactional output objects. Keep token and raw reconstruction
outside this format-only milestone.

## DD-733: Contextual Adaptive Huffman frames publish raw bytes last

- Date: 2026-08-11
- Status: accepted

Compose the private frame preflight, Contextual Adaptive Huffman LZSS token
adapter, and typed LZSS reconstructor into one complete-frame decoder. Require
exact caller-owned prefixes of 9,067 FGK nodes, 4,518 symbol indices, the
declared token count, and the declared raw extent. Reject short or pairwise
overlapping serialized, node, symbol, token, and raw regions before invoking
the entropy decoder.

Decode and validate the complete token sequence before reconstructing raw
bytes, and let the reconstructor validate the entire sequence again before its
first write. Set serialized consumption only after raw reconstruction succeeds.
Expose nested preflight, token, and reconstruction diagnostics while mapping
the outer failure to a stable category. Defer streaming lifecycle, encoding,
C ABI, CLI, benchmark, fuzzing, and profile admission.

## DD-734: Contextual Adaptive Huffman streaming buffers one bounded frame

- Date: 2026-08-11
- Status: accepted

Wrap the complete-frame decoder in an immutable-direction `core::Transform`.
Collect the fixed stream header, then one fixed frame header and its declared
body into caller-owned storage. Require exact node and symbol workspace
ceilings at construction and verify all serialized, model, token, and raw
regions are pairwise disjoint. Charge the complete serialized frame, exact
model storage, declared tokens, and raw frame to the aggregate limit before
collecting a body.

Decode a frame only after its complete body arrives, then drain its validated
raw bytes through arbitrary output capacities before collecting the next
header. Latch `EndInput` through draining, reject truncation and strict trailing
data, make terminal errors sticky, reject `ResetBlock`, and return no-progress
statuses exactly as the common process contract requires. Defer encoder and
public profile admission.

## DD-735: Contextual Adaptive Huffman encoding plans before publication

- Date: 2026-08-11
- Status: accepted

Expose private operation-level planning and encoding functions over the common
`ModeledOperation` sequence. Both passes initialize the exact caller-owned
9,067-node and 4,518-symbol model bank and traverse operations forward. A
Symbol writes its current leaf or NYT path, writes the alphabet-width new
value when needed, then updates only that context tree. BypassBits write their
value least-significant bit first and update no tree.

Planning validates the complete operation sequence and computes exact bits,
bytes, decision count, final valid bits, and descriptor without publishing it.
Encoding repeats that deterministic traversal only after output capacity,
limits, and all operation/model/output overlaps are validated. Zero-fill only
the exact payload prefix, preserve trailing capacity, and publish the
descriptor only after the second pass matches the plan. Defer LZSS token
inference, frame construction, streaming, and public admission.

## DD-736: Contextual Adaptive Huffman tokens emit operations directly

- Date: 2026-08-11
- Status: accepted

Add a reusable private forward planner/writer lifecycle to the operation
encoder, then drive it directly from validated LZSS typed tokens and
`LzssFieldContextState`. Do not allocate, serialize, or replay an intermediate
`ModeledOperation` array. Emit token kind, literal, length class and extras,
distance class and extras in canonical forward order, accepting a token into
the context state only after all of its events succeed.

Run a complete planning traversal before validating exact payload capacity and
performing the deterministic writing traversal. Charge typed-token bytes,
exact model storage, and exact payload bytes together; reject every
token/model/output alias before payload publication; and publish the entropy
descriptor only after plan/write counts and bit extents agree. Defer frame
header construction, raw LZSS parsing, streaming, and public admission.

## DD-737: Contextual Adaptive Huffman frame encoding is one planned transaction

- Date: 2026-08-11
- Status: accepted

Add a private complete-frame planner and encoder above the direct typed-token
adapter. First validate the stream and exact non-empty frame input, encode raw
bytes into caller-owned typed-token storage, and plan the contextual payload
with the exact caller-owned 9,067-node and 4,518-symbol model bank. Validate
the fixed 16-byte descriptor and outer frame header before calculating the
complete serialized extent.

Charge raw input, used typed-token storage, exact model storage, and the whole
serialized frame together. Reject every raw, token, node, symbol, and
serialized-region alias before a serialized write. The writing pass repeats
only the deterministic entropy traversal, serializes descriptor and header
after its counts agree with the plan, preserves excess output capacity, and
returns no serialized size on a failed plan. Defer streaming lifecycle and
public profile admission.

## DD-738: Contextual Adaptive Huffman streaming owns one bounded frame

- Date: 2026-08-11
- Status: accepted

Wrap the complete-frame encoder in the immutable-direction transform contract.
Serialize and drain the stream header first, collect at most one raw frame in
caller storage, invoke the complete-frame encoder only when that frame reaches
its exact expected raw size, then drain the retained serialized frame through
arbitrary output capacities. Reset the exact contextual model through each
complete-frame invocation; retain no entropy state between frames.

Latch `EndInput` while a final frame or header is draining, allow a full frame
to be emitted before final input, keep `Flush` non-terminal and leave a partial
raw frame open, reject `ResetBlock`, extra or premature final input, unknown
flags, and every workspace/output alias, and make terminal results sticky.
Constructor validation must bind disjoint raw, token, node, symbol, and
serialized-frame workspaces. Defer public profile admission.

## DD-739: Contextual Adaptive Huffman profiles derive typed model layouts

- Date: 2026-08-11
- Status: accepted

Add private encoder and decoder workspace calculators plus transactional typed
view partitioners. For largest raw frame `F`, reserve at most `F` typed tokens,
the exact 9,067 nodes and 4,518 symbol indices, and a serialized frame ceiling
`64 + 16 + ceil(267F/8)`. The 267-bit raw-byte ceiling is the sum of the
three-bit maximum new token-kind operation and the 264-bit maximum new
256-symbol literal operation; matches consume at least five raw bytes and do
not exceed this per-byte bound.

Lay out encoder views as tokens, aligned nodes, then aligned symbols; lay out
decoder views as nodes, aligned symbols, then aligned tokens. Publish offsets,
byte counts, alignment, stream header, and typed spans only after all checked
arithmetic, format, entropy-entry, payload, and aggregate limits succeed.
Empty encoding requires no frame views. Reject forged requirements, short or
misaligned storage, and map profile errors to stable core categories. Defer C
ABI exposure.

## DD-740: Contextual Adaptive Huffman public construction keeps models opaque

- Date: 2026-08-11
- Status: accepted

Add an ABI-1 C lifecycle dedicated to the already fixed Format 2 Contextual
Adaptive Huffman identity `2/2 + 1/1 + 1/2`. Use a new size-tagged
`marc_lzss_contextual_adaptive_huffman_config`, a direction-specific workspace
query, and a factory accepting primary, secondary, and aligned opaque views.
Do not expose C++ token, FGK-node, or symbol-index types in the public header.

Encoding assigns primary to raw-frame collection, secondary to the retained
serialized frame, and views to tokens, nodes, then symbols. Decoding assigns
primary to serialized-frame collection, secondary to atomic raw output, and
views to nodes, symbols, then tokens. Recalculate requirements whenever any
immutable parameter or hard limit changes; validate sizes, alignment, and all
pairwise used-prefix overlaps before publishing a transform. Keep destroy and
process behavior on the common ABI-1 transform contract. This additive family
does not change the ABI version or serialized bytes and does not yet admit a
CLI selector, benchmark, fuzzer, or interoperability archive.

## DD-741: Contextual Adaptive Huffman completion is proven through ABI 1

- Date: 2026-08-11
- Status: accepted

Audit the single Contextual Adaptive Huffman representation entirely through
its public configuration, requirements, create, process, and destroy calls.
Require empty input, all one-byte values, all byte values in sequence,
repetition, mixed binary patterns, deterministic pseudo-random data, and sizes
immediately around the 64-byte test-frame boundary to round trip with stable
bytes. Require multi-frame bytes and output to be identical under one-byte and
mixed input/output chunk schedules, with sticky EndOfStream.

Corrupt, truncate, and append data to only the final frame of a four-frame
stream. Earlier complete frames may remain committed, but the final frame must
publish no raw byte and the terminal malformed-stream result must be sticky.
Do not use a private parser, encoder, decoder, or workspace type in this audit.
This milestone changes no format or public symbol and does not yet admit
fuzzing, benchmark, CLI, or interoperability support.

## DD-742: Contextual Adaptive Huffman malformed regressions are dual-boundary

- Date: 2026-08-11
- Status: accepted

Create one canonical five-byte public stream, then preserve permanent ordinary
GoogleTests for every strict prefix and independently malformed stream header,
frame length, entropy descriptor, and payload padding classes. Feed inputs
whose stream header parses to both the private complete-frame decoder and the
public ABI-1 streaming decoder; malformed stream headers are still required to
fail through the public boundary.

The private output and public output must retain sentinel bytes on every
single-frame failure. The public result must be malformed stream with zero raw
publication and a sticky byte/bit position and status on the next call. Keep
all workspaces fixed and locally bounded. This is a permanent deterministic
regression milestone, not a fuzz execution, and changes no format, decoder
policy, or public symbol.

## DD-743: Contextual Adaptive Huffman fuzzing keeps two fixed decode paths

- Date: 2026-08-11
- Status: accepted

Add one libFuzzer-compatible entry point that caps supplied input at 64 KiB,
published raw output at 4 KiB, one raw frame at 1 KiB, and payload at the exact
`ceil(267F/8)` profile ceiling. Preallocate the exact 9,067 private FGK nodes,
4,518 private symbol entries, 1,024 private tokens, private raw output, public
primary/secondary/views regions, and final public output in one thread-local
workspace. Charge the public requirements against a compile-time aggregate
bound before construction.

Exercise the private complete-frame decoder only after a valid 112-byte stream
header, and always exercise the public ABI-1 streaming decoder. Derive partial
input/output chunk sizes from input bytes, cap process calls by input plus
output ceilings, and abort on impossible consumption, production, progress, or
input-starvation behavior. Ordinary MSVC and ClangCL builds must compile the
translation unit warning-clean. This milestone does not execute a mutation
campaign, retain a corpus, or claim sanitizer evidence.

## DD-744: Contextual Adaptive Huffman sanitizer admission is finite and local

- Date: 2026-08-11
- Status: accepted

Run the admitted dual-decoder harness under the established Windows Clang 22
GNU-driver libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer build.
The initial smoke processes exactly 1,000 generated inputs, permits input up to
the harness's 64 KiB ceiling, limits each input to five seconds, and limits RSS
to 512 MiB. The matching sanitizer runtime directory is added only to the
fuzzer child process.

Supply no persistent corpus. Keep generated mutations in memory and direct a
failure artifact to the ignored sanitizer build area only if a finding occurs.
Record the exact exit, coverage, features, corpus extent, peak RSS, and finding
status reported by the run. A successful bounded smoke is evidence only for
the exercised inputs and is not an exhaustive safety claim.

## DD-745: Contextual Adaptive Huffman enters the experimental benchmark only

- Date: 2026-08-11
- Status: accepted

Add `lzss-contextual-adaptive-huffman` as an explicit experimental benchmark
selector without changing the stable profile inventory, CLI, or
interoperability schema. Use 65,536-byte raw frames, at most one typed token
per raw byte, the exact 9,067-node plus 4,518-symbol model bank, a
`ceil(267F/8)` payload ceiling, and the established 8-MiB aggregate-buffer
policy.

For raw input extent `N` and nonempty frame count `K`, reserve complete output
with checked arithmetic using `112 + 80K + ceil(267N/8)`; the per-frame term
covers the 64-byte common header and fixed 16-byte descriptor. Construct each
immutable direction only through the public ABI-1 configuration, requirements
query, and factory. Require an untimed byte-exact round trip before measuring
either direction, then report complete-stream ratio, encode/decode throughput,
peak caller-owned workspace, and every direction-specific workspace region.
Measurements and the registered smoke test are descriptive, not thresholds.

## DD-746: Contextual Adaptive Huffman CLI admission stays explicit

- Date: 2026-08-11
- Status: accepted

Add `lzss-contextual-adaptive-huffman` as an explicit experimental CLI
selector for both encode and decode. Use the benchmark's 65,536-byte raw
frame, one-token-per-byte ceiling, exact 13,585-entry model bank,
`ceil(267F/8)` payload limit, and 8-MiB aggregate policy. Construct the
transform only through the public ABI-1 configuration, direction-specific
requirements query, factory, process, and destroy calls; private typed layouts
must not enter the command-line layer.

Register the common file round-trip test with Format 2 entropy identity `1/2`.
Require a deterministic-fixture nonempty round trip and an empty round trip,
refusal to overwrite an existing destination, malformed-stream and
strict-trailing rejection, and no
destination or temporary-file publication on failure. Keep selector choice
explicit in both directions; do not add auto-detection, a stable profile, or
an interoperability archive in this milestone.

## DD-747: Schema 36 appends Contextual Adaptive Huffman once

- Date: 2026-08-11
- Status: accepted

Advance the current interoperability manifest to schema 36 and codec set
`marc-cli-v36`. Preserve schema 35's exact 46-archive order and append
`lzss-contextual-adaptive-huffman` as archive 47. The generator must round-trip
the repository-owned 8,193-byte fixture before recording the archive's leaf
name, size, and SHA-256.

The verifier must preserve schemas 1 through 35 exactly, require the schema-36
identity and complete 47-entry order, validate all manifest hashes and sizes,
decode every archive to the fixture, and reproduce every archive byte for byte
with the local CLI. The compatibility test must reject a reordered schema-36
manifest and derive schema 35 before continuing the existing downgrade chain.
This changes the interoperability inventory only; it does not change the
stable 42-profile inventory or any serialized profile representation.

## DD-748: Schema 36 external evidence requires four matching directions

- Date: 2026-08-11
- Status: accepted

Admit schema-36 external evidence only when the pushed Windows/MSVC and Ubuntu
24.04/Ninja CI bundles verify on Ubuntu 26.04/Clang, and a bundle generated by
that Ubuntu 26.04 executable verifies both locally and with Windows/MSVC.
Require every path to report the same full Git revision, producer label, and
all 47 archives. Record exact decode and byte-identical re-encoding evidence
without importing any generated bundle into the repository.

## DD-749: Contextual rANS variant 3 becomes the sole canonical profile

- Date: 2026-08-11
- Status: accepted

Withdraw fixed-descriptor entropy variant 2 and promote the existing canonical
variable-length descriptor variant 3 under the unqualified Contextual rANS
name. Preserve every variant-3 byte and strict decode rule. Never reassign
variant 2; reject identity `4/2` as unsupported. Remove both the old fixed
surface and every compact-qualified public/selectable name without aliases.

## DD-750: Canonicalization preserves shared rANS and typed-token cores

- Date: 2026-08-11
- Status: accepted

Delete only fixed-descriptor serialization and lifecycle layers. Retain the
typed LZSS context bridge, normalized in-memory model, rANS arithmetic,
decode-table construction, and canonical dense/sparse records used by variant
3 and contextual tANS. Rename the surviving variant-3 frame, profile,
streaming, C ABI, CLI, benchmark, fuzz, and test surfaces after the conflicting
fixed names are removed.

## DD-751: Schema 37 renames archive 44 without changing its bytes

- Date: 2026-08-11
- Status: accepted

Keep schemas 1 through 36 frozen. Schema 37 retains 47 archives and replaces
only archive 44's historical `lzss-contextual-rans-compact` codec and leaf
name with `lzss-contextual-rans`. The verifier privately maps the historical
schema-33-through-36 name to the new CLI selector; the public CLI has no alias.
The compatibility test converts schema 37 to 36 by renaming that one manifest
entry and file before continuing the existing downgrade chain.

## DD-752: Public Contextual rANS names select variant 3 immediately

- Date: 2026-08-12
- Status: accepted

Free the unqualified public meaning by removing fixed dispatch, then bind the
existing size-tagged `marc_lzss_contextual_rans_config` and three-function
lifecycle directly to variant 3. Remove compact-qualified declarations,
selectors, benchmark paths, and duplicate public tests without aliases. Keep
the variant-3 workspace bounds and bytes unchanged while private frame names
are consolidated separately.

## DD-753: Contextual rANS frame routing has one representation

- Date: 2026-08-12
- Status: accepted

Promote the existing variant-3 frame validator, encoder, decoder, streaming
transforms, and workspace profile to the unqualified private names. Remove the
variant selector from both streaming state machines and delete the fixed frame
implementation, compact wrappers, and duplicate tests. The canonical stream
parser accepts only entropy identity `4/3`; it rejects retired identity `4/2`
before publishing parsed state.

## DD-754: Contextual rANS has one fuzz target

- Date: 2026-08-12
- Status: accepted

Use only `marc_fuzz_lzss_contextual_rans_stream` for the canonical public and
private decoder paths. Remove the compile-time representation switch and the
duplicate compact target. Size its fixed thread-local workspace from the
variant-3 maximum descriptor and retain the existing bounded input, output,
frame, payload, table, and aggregate limits.

## DD-755: Contextual rANS has one canonical entropy boundary

- Date: 2026-08-12
- Status: accepted

Delete the fixed 9,052-byte descriptor parser, serializer, decoder entry, and
typed-token bridge. Promote variant 3's 23-through-9,025-byte descriptor to
the unqualified entropy and context names without changing its bytes. Return
the canonical descriptor size in the direct typed-token encode result, admit
decoding only from an exact serialized descriptor span, and retain the shared
generic compact-model records because Contextual tANS also consumes them.
Preserve applicable scalar-state, malformed-input, limit, aliasing, and token
validation tests through the canonical serialized entry rather than deleting
coverage with the retired representation.

## DD-756: Derived contextual headers adapt through identity 4/3

- Date: 2026-08-12
- Status: accepted

Contextual tANS, Blocked Huffman, and Adaptive Huffman reuse the common
Contextual rANS stream-header field parser only through a private adapted
copy. After retiring identity `4/2`, that copy must select the canonical base
identity `4/3`; each derived serializer still overwrites both entropy fields
with its own unchanged identity (`5/2`, `2/2`, or `1/2`). This is a private
validation dependency and must not change any derived profile byte.

## DD-757: Canonical Contextual rANS is admitted only after full regression

- Date: 2026-08-12
- Status: accepted

Merge the canonicalization branch only after both supported Windows compiler
configurations pass every registered test with the 240-second per-test limit,
including schema compatibility; the sole bounded sanitizer target completes a
finite 1,000-input ASan/UBSan/libFuzzer smoke; and the repository README
benchmark reproduces variant 3's previously recorded encoded extent. Treat
compact-qualified names in frozen schema history and explanatory records as
intentional history, but require zero such files or selectable implementation
surfaces. This admission establishes deletion and rename equivalence; it does
not claim a general performance threshold or an external interoperability
result for schema 37.
