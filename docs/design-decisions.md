# Design decisions

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
