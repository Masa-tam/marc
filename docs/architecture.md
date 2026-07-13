# Architecture

## Baseline

marc is a C++20 library of bounded, stateful byte-stream transforms. MSVC is
the reference toolchain, but the implementation avoids compiler extensions and
keeps portable C++ as a design constraint. CMake is the canonical build
description.

### LZ77 foundation

LZ77 variant 1 begins with transactional fixed parameter and token parsers.
Contextual validation separates structural token canonicality from frame-local
history, distance, match-length, and output-extent rules. A bounded scanner
validates the complete 16-byte-token region without allocating or producing
output and reports the stable failing token index. The reference decoder first
validates the entire token region and output capacity, then performs the
overlap-safe bytewise copy pass, preserving output on all caller-visible errors.
The reference encoder uses a deliberately clear bounded exhaustive match search,
with the format-defined greedy longest match and nearest-distance tie break. A
planning pass fixes token count and serialized size before output is touched.
The streaming decoder accumulates only one fixed token, then drains its match
and optional literal directly to caller output. Match progress and `EndInput`
are retained across calls, so both encoded input and raw output may be split at
every byte without buffering a decoded frame.
The caller supplies a bounded circular history region of
`min(window_size, frame_size)` bytes so references remain valid when output
buffers change between calls.
The streaming encoder buffers exactly one declared raw frame in caller-owned
storage because the canonical greedy parse depends on later bytes and the exact
frame end. It generates the reference token stream into separate caller-owned
storage, then drains it with arbitrary output capacity.
The first complete LZ77 pipeline binds variant 1 to entropy `None`: the generic
frame header declares the raw and token extents, and the payload is the exact
canonical token stream. Strict decoding validates the whole frame before
committing raw output.
The known-size stream path writes the fixed stream prefix, one canonical LZ77
parameter region, then deterministic frame extents. Strict decoding scans every
frame before a second output pass, so corruption in a later frame leaves the
entire caller output untouched.
The outer streaming encoder emits that 80-byte prefix, then reuses one raw and
one serialized caller-owned frame workspace. Pending prefix or frame output has
priority over accepting later raw input, and arbitrary chunking matches the
known-size reference stream exactly.
The matching outer decoder collects the fixed prefix and one exact serialized
frame into caller-owned storage, atomically decodes it into a second frame
workspace, then drains raw bytes before accepting the next frame. This makes a
validated frame the streaming commit boundary.
LZ77 profile helpers normalize the stream configuration and calculate encoder
workspace from the exact fixed-token worst case. Decoder workspace is derived
only from local limits, before any untrusted stream bytes are inspected.
The C ABI exposes this path through a separate size-tagged LZ77 configuration,
workspace query, and transform factory while retaining ABI version 1. No C++
types, exceptions, or ownership cross the shared-library boundary.

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

### Dynamic Range Coder foundation

Dynamic Range Coder variant 1 begins with a fixed 16-byte descriptor validator
and an allocation-free 256-symbol model. Parsing publishes a descriptor only
after its sizes, flags, reserved bytes, variant frame bound, payload bound,
buffer bound, and required model-total policy all pass.

The reference order-0 model stores 256 nonzero 16-bit frequencies inline and
uses bounded linear cumulative lookup. This deliberately clear baseline has no
dynamic allocation or input-controlled recursion. Its invariant check verifies
the exact total and nonzero frequencies, including immediately after the
specified rescale boundary; a later Fenwick optimization must preserve the same
updates and encoded bytes.

The range reference encoder uses the same run loop first with a counting sink
and then with the caller payload span. Planning validates frame size, model
policy, exact encoded size, payload limits, and the resulting descriptor before
the second pass can mutate output. A short output therefore leaves both payload
and descriptor untouched, while successful encoding is required to reproduce
the planned byte count exactly.

The strict range decoder performs the same bounded decode pass twice. The first
pass has no output span and must consume exactly the declared payload while
producing the declared symbol count with valid interval and model invariants.
Only then may the second pass write caller output. Invalid scaled values,
missing normalization bytes, trailing bytes, limit failures, and model failures
therefore leave the entire frame output untouched.

The complete Dynamic Range frame path composes the generic frame header, one
typed 16-byte range descriptor, and one byte-aligned payload. Generic frame
validation recognizes the required descriptor and model-total bound explicitly.
The reference encoder preflights the whole 79-byte `ABA` vector before mutation;
the decoder requires an exact one-frame span and delegates body atomicity to the
strict range decoder.

The known-size Dynamic Range stream reference emits the fixed stream header and
plans every original-size-derived frame before writing. Strict decoding scans
and semantically validates every exact frame extent without output, then repeats
the traversal into caller storage. This preserves whole-stream atomicity while
each frame independently resets the range state and adaptive model.

The frame-at-a-time Dynamic Range encoder drains the fixed stream header, then
collects at most one raw frame in caller storage. A complete full or final short
frame is encoded atomically into a second caller workspace and drained before
reuse. Pending output has priority; non-terminal flush leaves a partial frame
open, and explicit reset remains unsupported because outer frames define every
coder and model reset.

The matching Dynamic Range streaming decoder incrementally collects the fixed
stream and frame headers, then exactly one declared serialized frame in caller
storage. Strict frame decoding completes into a separate decoded workspace
before any byte of that frame is exposed. Pending decoded output has priority;
previously drained frames remain committed if a later frame is malformed.

Dynamic Range profile normalization fixes algorithm and variant IDs and reports
caller workspace before construction. Since `range >= 2^24` before each symbol
and total is at most 2^15, a minimum-frequency symbol needs at most two byte
normalizations. Encoder payload storage is therefore bounded by
`2 * largest_frame + 5`, plus fixed descriptor and frame headers. Decoder sizing
uses only local limits and requires policy support for the variant's model total.

The Dynamic Range C ABI adds a separate size-tagged configuration and factory
without changing ABI version 1 or existing profile layouts. It carries the
range-model-total policy explicitly, uses only primary and secondary byte
workspaces, and returns the common opaque transform processed and destroyed by
the shared lifecycle functions.

### rANS foundation

rANS variant 1 begins with a fixed 528-byte descriptor validator and an
allocation-free block normalizer. Descriptor parsing publishes only after fixed
fields, reserved bytes, exact normalized sum, caller-expected sizes, table
limits, block limits, payload limits, and combined buffered bytes pass.

The normalizer stores 256 source counts and 256 uint16 frequencies inline. It
uses signed 64-bit normalization errors, bounded scans, and explicit symbol tie
breaks; output is assigned only after the exact sum 4096 is reached. This clear
reference path performs no dynamic allocation or input-controlled recursion.

The rANS reference encoder first normalizes and runs the complete reverse state
machine with a counting sink. After exact capacity and descriptor validation,
the write pass stores renormalization bytes backward from the payload end and
then writes the final state in the first eight bytes. This implements the global
prepend rule without allocation and leaves output untouched on capacity failure.

The strict rANS decoder expands the validated frequency model into a fixed
4096-entry slot table. It then runs the complete block once without output,
requiring valid symbol-boundary states, exact renormalization-byte consumption,
and terminal state `L`. Only a successful validation pass is repeated into
caller output, so malformed blocks remain output-atomic.

The rANS frame controller validates the complete fixed-size descriptor region
twice. Its first pass proves block count, fixed and final-short symbol counts,
every model, checked payload offsets, exact aggregate payload size, and combined
buffer limits without publishing views. The second pass fills caller-provided
bounded block views used by the later frame decoder.

The complete rANS frame path plans every block before writing, then serializes
the generic header, all fixed descriptors, and all payloads in separate regions.
Strict decoding uses controller views to validate every payload before a second
loop writes any block output. Capacity failure and malformed later blocks
therefore leave the whole frame output untouched.

The known-size tANS stream reference plans all deterministic outer frames before
encoding. Strict decoding scans and validates the complete stream without
output, reusing one caller-owned block-view workspace, then repeats the
traversal into caller storage for whole-stream atomicity.

The tANS streaming encoder buffers one raw outer frame and one complete encoded
frame in caller-owned storage, drains pending output before accepting later
input, and keeps partial frames open across flush. Its profile computes the
exact format-independent maximum from 12 bits per symbol plus each block state.

The matching tANS streaming decoder collects one exact frame, validates all
descriptor and payload semantics, decodes into separate caller storage, and only
then drains output. Later malformed frames cannot expose partial frame output or
retract already drained frames.

The known-size rANS stream reference plans all deterministic outer frames before
encoding. Strict decoding scans and semantically validates every complete frame
without output, reusing one caller-owned block-view workspace, then repeats the
scan into caller storage. This preserves whole-stream atomicity while every
frame and block independently rebuilds its model and resets state.

The rANS streaming encoder buffers exactly one raw outer frame, commits its
complete encoded representation to a second caller-owned workspace, and drains
that representation before accepting bytes for the next frame. Its profile
query derives both workspace extents from the largest possible frame and the
configured entropy block size; no steady-state allocation is required.

The matching rANS streaming decoder collects one declared frame, validates all
descriptors and payload state transitions, decodes into a separate bounded
workspace, and only then drains output. Its caller-owned view array is sized
from the local maximum-blocks-per-frame policy.

### tANS foundation

tANS variant 1 begins with a transactional fixed-descriptor validator and a
deterministic table builder. The builder fills all 4096 spread positions,
derives every decode transition, and constructs the exact inverse encode lookup
in temporary bounded storage before publishing either table. No global mutable
table or implementation-defined permutation is used.

The tANS reference encoder normalizes and builds the complete tables, then
performs a count-only reverse traversal before touching caller output. A second
reverse traversal writes each emitted chunk directly into its precomputed final
bit position, avoiding block-size-proportional token or chunk storage while
still producing decoder-consumption order.

The strict tANS decoder builds validated tables and traverses the complete
declared symbol count without output. It requires an in-range initial offset,
available bits for every transition, exact terminal state and bit consumption,
and zero high padding. Only a second identical traversal publishes bytes.

The tANS frame controller validates the exact fixed-descriptor extent, expected
full and final-short block symbol counts, each descriptor model, checked payload
offset sums, and local frame limits in a first scan. A second scan publishes
caller-owned block views only after the whole region is known valid.

The complete tANS frame path plans every block before writing, serializes all
descriptors before all payloads, and validates every block state and bitstream
before a second loop writes output. Capacity failure and malformed later blocks
therefore leave the whole frame output untouched.

### C transform ABI

The stateful C ABI exposes Blocked Huffman, Adaptive Huffman, Dynamic Range,
rANS, and tANS variant 1 through
separate versioned, size-tagged configuration, workspace-query, and factory
functions. All profiles construct the same opaque transform type and share its
process and destroy operations. Encoder workspaces hold one raw and one
serialized frame. Decoder workspaces hold one serialized and one decoded frame;
Blocked Huffman, rANS, and tANS also use aligned internal block-view arrays, while
Adaptive Huffman and Dynamic Range need no views workspace. These buffers remain
caller-owned and must outlive the handle.

Only the small opaque handle and its C++ implementation object are allocated by
the library with non-throwing allocation. Processing uses caller input/output
spans and maps stable core status and error categories into fixed C constants.
Every exported function is `noexcept` when compiled as C++.
