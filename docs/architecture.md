# Architecture

## Baseline

marc is a C++20 library of bounded, stateful byte-stream transforms. MSVC is
the reference toolchain, but the implementation avoids compiler extensions and
keeps portable C++ as a design constraint. CMake is the canonical build
description.

On Windows, the canonical preset uses the Visual Studio 2026 generator and
MSBuild. Non-Windows presets use Ninja with the platform's selected compiler.
This avoids depending on localized MSVC `/showIncludes` text for incremental
dependency tracking while retaining Ninja's straightforward portable workflow
on platforms where compiler dependency files are locale-independent.

Canonical commands are:

```text
cmake --preset windows-msvc
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Use the corresponding `ninja-debug` or `ninja-release` presets on non-Windows
hosts. `CMakeUserPresets.json` remains ignored for machine-local overrides.

The same source list builds both a static library and a shared library. The C
ABI is the binary boundary; C++ implementation types are never exported as ABI.

## Layers

Encoding flows from raw input through an optional dictionary transform, an
entropy transform, and the frame controller to a sink. Decoding reverses that
order. Direction is immutable for a transform instance.

Entropy blocks cannot cross frame boundaries. A frame may contain multiple
entropy blocks, and its last entropy block may be shorter than the configured
entropy block size. A frame boundary finalizes and resets every layer whose
state is scoped to that frame.

## Error and ownership policy

Normal control flow does not use exceptions. Public C++ processing functions
are `noexcept`, and no exception may cross the C ABI. Allocation failures and
unexpected internal exceptions at ABI adapters are converted to stable status
codes. Compiler exception support remains enabled so standard-library failures
can be contained safely.

Opaque C handles are created and destroyed by marc. The initial ABI does not
accept allocator callbacks and does not return variable-sized library-owned
buffers. Caller-provided input, output, and diagnostic buffers retain caller
ownership.

## Initial stream-size policy

The baseline framed format requires the original uncompressed size to be known
when encoding begins. Unknown-size streams are a future format capability, not
a baseline requirement. The transform API remains incremental; known size does
not imply that the complete input must be buffered.

## Decoder limits and frame validation

Limits are supplied to the decoder before stream parsing. Their configuration
is validated once, and every decoder-visible size is checked before allocation
or payload decoding. A parsed frame is represented by a `FrameBounds` summary;
validation checks individual dimensions, cumulative output, expansion, and the
sum of simultaneously buffered regions using checked arithmetic.

The frame controller will own the transition from header validation to model
construction and payload decoding. No entropy or dictionary decoder may
allocate from an unvalidated stream length. Validation failure leaves the
controller in its terminal error state and must produce a stable limit error.

### Incremental header collection

The framing parser first collects each fixed-size prefix into a compile-time
bounded accumulator. Collection consumes exactly the bytes still required for
that prefix and leaves following payload bytes untouched. Header bytes are not
exposed to semantic parsing until the prefix is complete. This keeps partial
input, truncation, and header validation separate and makes every split point
testable without allocating from stream-controlled data.

Variable-size header regions will be accepted only after their lengths have
been parsed from a complete fixed prefix and checked against decoder limits.

## Hash taps

`HashTap` observes bytes committed at one explicitly named pipeline boundary.
The caller supplies both the available span and the committed prefix length, so
unused output capacity is never hashed. A tap owns no algorithm object; the
injected `IHashAlgorithm` must outlive it. Neither interface allocates or throws.

Successful finalization is terminal. Algorithm failure and committed-byte count
overflow enter a terminal error state. Caller mistakes such as a committed
length beyond the available span or a wrong digest-buffer size do not mutate the
algorithm and may be retried. Reset explicitly begins a new scope.

Hash target/scope descriptors and concrete algorithms remain pending. They will
be specified before nonzero stream or frame hash regions are accepted.

## Buffered incremental reference encoder

The first `ProcessResult`-based Blocked Huffman encoder is a correctness
reference with caller-owned whole-input and whole-encoded-stream workspaces.
It accepts arbitrarily split input and drains arbitrarily small output spans,
but does not emit bytes before `EndInput`. Non-terminal `Flush` therefore does
not alter or close a frame; `ResetBlock` is rejected as unsupported.

This buffered reference is not the final bounded-frame streaming design. Its
encoded bytes must match the one-shot reference for every chunking pattern. A
later frame-at-a-time implementation will reduce workspace requirements while
retaining that deterministic representation and the same terminal-state rules.

The matching buffered decoder accumulates the encoded stream in caller-owned
storage. At `EndInput` it parses the stream header, checks decoded and view
workspace capacity, and invokes the strict whole-stream decoder. Only a fully
validated stream populates decoded workspace, which is then drained with
arbitrary output capacity. Malformed input and workspace exhaustion are stable
terminal errors.

### Frame-at-a-time encoder

The bounded encoder emits the fixed stream header immediately, collects at most
one configured uncompressed frame, encodes that frame into a caller-owned
serialized-frame workspace, and drains it before reusing both workspaces. It
does not retain the whole input or whole encoded stream. A partial frame remains
open across non-terminal `Flush`; known size determines when the final short
frame is complete.

If output capacity prevents a pending header or frame from draining, input is
left unconsumed and `NeedOutput` is returned. Once a complete frame drains, the
same call may resume input consumption. The emitted representation remains
identical to the buffered and one-shot references.

### Frame-at-a-time decoder

The bounded decoder incrementally collects the fixed stream header and each
fixed frame header, validates declared sizes, then buffers exactly one complete
serialized frame. A frame is decoded atomically into one caller-owned decoded
frame workspace and may be drained before later encoded frames arrive.

Its commit boundary is therefore one validated frame, not the whole stream. If
later input is malformed, previously drained frames remain committed while the
malformed frame contributes no output. Pending decoded output has priority:
`NeedOutput` may leave later encoded input unconsumed, and callers must re-present
that suffix with the applicable flags.

### Profile normalization and workspace queries

Before the C ABI constructs either transform, an internal profile factory
normalizes the public Blocked Huffman settings into the exact version 1 stream
header and reports all required caller-owned workspace capacities. Encoder
requirements use the largest frame that can actually occur for the known-size
input. Decoder requirements are derived only from local hard limits because no
untrusted stream field is available before construction. All capacity
arithmetic is checked before conversion to `size_t`.

Profile failures are collapsed into the stable core categories invalid
argument, unsupported, and limit exceeded. The later C adapter therefore need
not expose internal parser or codec-specific enumerations.

### Adaptive Huffman foundation

Adaptive Huffman variant 1 begins with two allocation-free internal pieces. A
fixed descriptor parser validates frame-controlled symbol count, payload size,
valid-bit count, flags, reserved bytes, format limits, and local limits before
payload traversal. A 513-node FGK tree stores nodes and the 256-entry symbol map
inline, so inserting every possible byte cannot allocate or exceed capacity.

Tree nodes have stable storage indices and explicit FGK order numbers. Swapping
changes parent/child positions and order numbers while symbol lookup continues
to identify the same stable node. A separate invariant validator checks unique
orders and symbols, parent/child reciprocity, internal weight sums, adjacent
siblings, and nondecreasing weight order. It is used at validation and test
boundaries rather than in the per-symbol update path.

The complete Adaptive reference frame composes the generic 56-byte frame
header, exactly one 16-byte algorithm-specific descriptor, and the planned
payload. Generic frame validation recognizes this descriptor-bearing
non-block-buffered profile explicitly; it does not infer Adaptive layout from
the Blocked Huffman structure. Encode capacity is checked for the whole frame
before header mutation, and decoding requires an exact one-frame input span
before invoking the frame-atomic strict entropy decoder.

The known-size Adaptive stream reference emits the fixed stream header and
then independently plans and encodes every original-size-derived frame. Its
strict decoder first scans exact frame extents and calls explicit
validation-only frame decoding, which performs full FGK traversal without
requiring or touching output. Only a successful whole-stream scan is repeated
into caller output. Thus the reference stream has whole-stream atomicity while
retaining independently reset and independently validatable frames.

The frame-at-a-time Adaptive encoder shares the same commit boundary as its
reference format: it collects one original-size-derived raw frame, encodes it
atomically into one caller workspace, and drains it before reuse. The stream
header drains independently. Pending output has priority, non-terminal flush
does not shorten a frame, and explicit reset remains unsupported because the
format already fixes reset at outer frame boundaries.

The matching Adaptive streaming decoder collects only fixed headers and one
declared serialized frame in caller storage. A complete frame is strictly
decoded into one decoded-frame workspace before any of its bytes are exposed.
Decoded output has priority over later encoded input, so `NeedOutput` may leave
an input suffix unconsumed. Earlier validated frames remain committed if a
later frame is malformed; the malformed frame contributes no output.

Adaptive profile normalization constructs only the fixed variant 1 stream
header and reports caller workspace before transform creation. Encoder sizing
uses the largest frame that can occur and a conservative 264 bits per symbol:
at most 256 path bits plus an 8-bit new-symbol literal. Decoder sizing uses
only local limits because no stream header is trusted yet. Empty encoding needs
no frame workspace, and all multiplication, rounding, and `size_t` conversion
are checked.

### C transform ABI

The stateful C ABI exposes Blocked Huffman and Adaptive Huffman variant 1 through
separate versioned, size-tagged configuration, workspace-query, and factory
functions. Both profiles construct the same opaque transform type and share its
process and destroy operations. Encoder workspaces hold one raw and one
serialized frame. Decoder workspaces hold one serialized and one decoded frame;
Blocked Huffman also uses an aligned internal block-view array, while Adaptive
Huffman needs no views workspace. These buffers remain caller-owned and must
outlive the handle.

Only the small opaque handle and its C++ implementation object are allocated by
the library with non-throwing allocation. Processing uses caller input/output
spans and maps stable core status and error categories into fixed C constants.
Every exported function is `noexcept` when compiled as C++.
