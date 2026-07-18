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
The standalone LZ77 fuzz boundary drives both the strict complete-stream
decoder and the frame-committing outer decoder. It fixes all byte extents in
caller-owned arrays, derives chunk sizes from bounded input, and enforces an
independent call ceiling so malformed token streams cannot allocate or stall
without becoming a reproducible finding.
The standalone public-ABI completion matrix consolidates local LZ77 readiness
above these component paths. It covers required binary data classes, frame
boundary neighbors, deterministic re-encoding, one-byte and mixed chunking,
repeatable EndOfStream, and frame-atomic rejection of final-frame corruption,
truncation, and trailing bytes. This establishes local implementation evidence,
not external cross-platform release completion.

### LZSS foundation

LZSS variant 1 uses transactional variable-size token parsing and a strict
frame scanner before its atomic reference decoder. The deterministic reference
encoder shares one exhaustive nearest-first greedy parse between planning and
writing, and applies the exact two-byte Literal versus nine-byte Match cost.
The streaming decoder accumulates at most one nine-byte token, validates it
against committed frame history, and drains its Literal or Match through a
caller-owned circular history region. Token collection, overlap-copy progress,
and `EndInput` survive arbitrary input and output splits without allocation.
The streaming encoder buffers one complete known-size raw frame and its
canonical token stream in separate caller-owned regions, then drains bytes
without accepting later input. Its output is identical to the reference encoder
for every input and output chunking.
The first complete LZSS pipeline binds these canonical bytes directly to the
generic frame through entropy `None`. Frame planning fixes both raw and
variable-token extents, while strict decode validates the entire payload before
committing any raw byte.
The known-size LZSS stream repeats this frame-local reset profile until the
declared original size is reached. Reference decode validates every frame before
publishing any output, so corruption in a later frame leaves the caller's whole
output buffer unchanged.
The streaming decoder instead commits one fully validated frame at a time. It
buffers encoded and decoded forms in separate caller-owned workspaces, allowing
arbitrary input/output chunking without exposing bytes from a malformed frame.
The streaming encoder similarly buffers one raw frame and its complete encoded
form in separate caller-owned workspaces. Completed frames drain immediately;
partial-frame Flush does not create a format boundary.
The LZSS profile builder normalizes this pipeline and reports encoder workspace
from the exact two-byte-per-input worst case. Decoder workspace depends only on
local frame, dictionary-payload, compressed-payload, and aggregate limits.
The C ABI exposes the same path through an independent size-tagged LZSS config,
workspace query, and encoder/decoder factory without changing ABI version 1 or
passing C++ ownership across the boundary.
An opt-in benchmark executable drives every public C transform over
caller-selected files. It reports full-stream ratio, timed transform
throughput, and profile-derived codec workspace under one documented method.
The first dictionary fuzz harness presents the same bounded arbitrary input to
the strict and streaming LZSS decoders. Local limits, fixed caller workspaces,
chunk-derived scheduling, and a call guard keep malformed exploration bounded.
The standalone public-ABI completion matrix consolidates local LZSS readiness
above these component paths. It covers required binary data classes, frame
boundary neighbors, deterministic re-encoding, one-byte and mixed chunking,
repeatable EndOfStream, and frame-atomic rejection of final-frame corruption,
truncation, and trailing bytes. This is local implementation evidence rather
than external cross-platform release completion.

### LZ78 foundation

LZ78 variant 1 serializes each phrase as one fixed eight-byte index-plus-byte
token and resets its phrase table at every outer frame. Strict validation uses
caller-owned phrase records, rejects forward or out-of-range references, and
checks the declared raw extent before atomic decode. The deterministic encoder
selects the longest existing phrase with stable index-order tie breaking and
handles a final existing phrase through the specified terminal token form.

The one-shot and outer streaming paths prepend the common 80-byte parameterized
prefix and wrap each canonical token region in the generic frame header.
Profiles derive encoded, raw, and phrase-table workspace from checked local
limits. The public C ABI exposes only byte extents and alignment; CLI,
benchmark, and fuzz paths use that same bounded transform surface.

The public-ABI completion matrix uses queried, explicitly aligned phrase-table
views in both directions. It covers required binary data classes, frame
boundary neighbors, deterministic re-encoding, one-byte and mixed chunking,
repeatable EndOfStream, and frame-atomic rejection of final-frame corruption,
truncation, and trailing bytes. An empty encoder queries zero phrase-view bytes
because it can emit no phrase; non-empty encoders and decoders query nonzero
aligned view storage. These are local implementation checks, not external
release evidence.

### LZW foundation

LZW variant 1 begins with a transactional 16-byte parameter codec and a strict
packed-code validator. The initial 256 literal strings are implicit; every
non-literal phrase occupies one caller-owned prefix, trailing-byte, first-byte,
and checked-length record. The validator reads repository LSB-first fields,
applies the separately specified encoder/decoder width boundary, resolves the
`KwKwK` case without recursion, and validates exact output extent and zero
padding without publishing raw bytes. The atomic reference decoder then repeats
the packed-code traversal over that validated metadata, verifies each expected
insertion record, and writes every phrase backward through bounded prefix links
into its final caller-owned output range. The reference encoder stores each
non-literal phrase as a bounded span into the immutable input frame, finds the
longest phrase by ascending code, and runs the same parse for exact planning and
LSB-first serialization. The streaming decoder retains partial numeric fields
inside BitReader plus an explicit partial-code accumulator, inserts phrase
metadata before draining the accepted phrase, and resolves each requested
forward byte through bounded prefix links without phrase-sized staging. The
streaming encoder buffers one declared raw frame, invokes the exact reference
planner and encoder into separate caller-owned storage, then drains those fixed
bytes without accepting later input. The LZW plus entropy None frame adapter
wraps one nonempty code stream in the generic 56-byte frame header, keeps the
dictionary and compressed extents identical, and exposes separate plan,
encode, validate, and atomic decode operations. It accepts only the exact
declared frame extent and resets the LZW dictionary at that boundary. The
one-shot stream adapter prepends the generic stream header and one 16-byte LZW
parameter region, partitions known-size raw input by the configured frame
extent, and validates every frame before publishing any decoded stream bytes.
The outer streaming decoder accumulates the fixed prefix and one bounded
serialized frame in caller storage, atomically decodes that frame into a
caller-owned raw staging buffer, and drains it before accepting the next frame.
Consequently an accepted frame is committed independently while later frame
corruption remains locally detectable.
The outer streaming encoder emits the same 80-byte prefix, buffers one raw
frame, invokes the exact LZW frame planner and encoder into caller-owned
storage, and drains that complete serialized frame before reusing the buffers.
Its output is byte-identical to the one-shot stream for every chunking pattern.
The LZW profile builder converts a high-level known-size configuration into the
canonical LZW plus None stream header and conservative encoder workspace. Its
decoder workspace calculator couples serialized-frame, raw-frame, phrase-table,
and aggregate local limits without trusting an unparsed stream parameter.
The C ABI exposes those calculations through an initialized plain-C config,
direction-specific workspace query, and opaque transform factory. Only byte
counts and alignment cross the ABI; private LZW phrase-record layouts do not.
The CLI and benchmark consume only that public ABI. Their LZW profile uses a
1 MiB frame, width 16, the matching 65,280-entry local decoder ceiling, and
bounded aligned workspace supplied by the application.
The LZW fuzz harness presents bounded arbitrary bytes to both the strict and
outer streaming decoders with width capped at 10, fixed caller workspaces, and
a call-count guard. Ordinary builds compile this harness without executing it;
permanent malformed cases remain normal deterministic tests.
The supplemental public-ABI completion matrix closes the gap left by the
original internal-API matrix. It uses a bounded 9-bit profile, queried aligned
phrase views, required binary data classes, deterministic re-encoding,
one-byte and mixed chunking, repeatable EndOfStream, and frame-atomic rejection
of final-frame corruption, truncation, and trailing bytes. Encoders for zero or
one raw byte query no phrase entries; larger encoders and all decoders use
nonzero aligned view storage.

### LZD foundation

LZD variant 1 begins with a transactional 16-byte parameter codec and fixed
eight-byte reference-pair codec. The strict validator accepts no output buffer:
it scans one complete token region, resolves only literals or earlier
frame-local phrases, records each inserted binary production and checked
expanded length in caller-owned workspace, and reports the stable failing token
and byte offset. Dictionary freeze preserves the existing reference namespace
without allocating further records. An absent right reference is accepted only
on the last token when its left expansion reaches the declared raw extent.
The atomic reference decoder completes validation and all capacity checks
before publishing output, then expands the acyclic grammar without recursion
through a caller-owned reference stack. Pushing the right reference before the
left preserves byte order; at most the stored phrase count plus one stack
entries are required. Serialized input, phrase records, and expansion stack
are checked together against the aggregate internal-buffer limit. The atomic
reference encoder retains each generated phrase as an offset and length into
the immutable raw frame, performs two deterministic longest-match searches
per token, and runs the same parse for exact planning and serialization. Raw
input plus phrase records are bounded before parsing, and output capacity is
checked before publication. The streaming decoder buffers one known-size frame
in caller-owned encoded storage, invokes the atomic reference decoder into a
separate caller-owned raw frame, and drains only after complete validation.
Its workspace query derives conservative encoded, phrase, expansion-stack,
and decoded extents from the declared raw size; construction enforces their
aggregate limit. The streaming encoder similarly collects one exact raw frame,
uses the reference planner and encoder, and drains canonical tokens from a
caller-owned maximum token extent. Encoder and decoder workspace queries share
one checked format-level `8 * ceil(raw_size / 2)` helper. The outer profile
now fixes LZD variant 1 plus entropy None and derives trusted encoder workspace
from the configured largest frame. Decoder workspace depends only on coupled
local limits and includes the frame header, token payload, raw frame, phrase
records, and expansion stack. The one-shot frame codec now plans and emits the
generic 56-byte header plus canonical LZD tokens, validates the entire header
and grammar before decode, and enforces header-inclusive aggregate limits.
The complete one-shot stream codec writes the 80-byte prefix, partitions raw
input at declared frame boundaries, and validates every frame before publishing
any raw output or parsed configuration. The outer streaming decoder collects
one complete frame into bounded caller storage, decodes it atomically into raw
staging, and then drains arbitrary output spans. The outer streaming encoder
drains the canonical prefix, collects one bounded raw frame, emits it through
the reference frame codec, and drains arbitrary output spans with bytes equal
to one-shot encoding. A bounded decoder fuzz harness now covers the one-shot
and outer streaming paths with compile-smoke and permanent malformed-stream
regressions. The C ABI, benchmark, completion matrix, and CLI now use only the
public bounded transform surface. Cross-platform determinism, sanitizer fuzz
execution, representative measurements, and release similarity review remain
release evidence rather than locally completed implementation work.

The strengthened public-ABI completion matrix adds repeatable terminal-state
checks and frame-atomic rejection of final-frame corruption, truncation, and
trailing bytes to the existing deterministic data and chunking matrix. This
closes current local LZD implementation evidence without treating external
release gates as locally satisfied.

### Published LZD plus Blocked Huffman boundary

LZD composition remains byte-oriented. The dictionary layer finishes its
canonical eight-byte reference-pair stream in bounded staging, and Blocked
Huffman divides those bytes without interpreting token boundaries. Decoding
reconstructs the exact staged byte region before the ordinary LZD validator
builds its acyclic phrase records and checks the terminal absent-right form.
Only a completely validated frame may be expanded to raw output.

For raw frame size `F`, staging is bounded by `8*ceil(F/2)`, generated phrase
records by `floor(F/2)` and the configured maximum, and the iterative expansion
stack by the admitted phrase count plus one. The checked opaque workspace
partition accommodates encoder records, or decoder Blocked Huffman views,
phrase records, and expansion-stack references without exposing their C++
layouts. The public factory, CLI, benchmark, fuzz target, completion matrix,
and schema-6 interoperability entry all retain this validation order.

### LZMW foundation

LZMW variant 1 begins with a transactional 16-byte parameter codec, fixed
four-byte reference tokens, and a strict decoder-side grammar validator. Bytes
`0..255` are implicit phrases. After every phrase except the first, the
validator records the previous-plus-current binary production and its checked
expanded length in caller-owned workspace until the configured dictionary
freezes. Each token may reference only the byte alphabet or an entry that
already existed before that token.

The validator enforces exact frame output length, fixed token alignment,
stable token index and byte offset, dictionary and serialized limits, and the
complete token-plus-phrase-workspace aggregate before recording entries. It
detects overflow even for adversarial phrase sequences whose lengths grow like
Fibonacci numbers. The atomic reference decoder runs that validator first and
then expands only its acyclic phrase records through a caller-owned iterative
stack, pushing right before left to preserve byte order. Output capacity and
the complete token, phrase-record, and stack aggregate are checked before the
first raw byte is published. The deterministic reference encoder represents
every generated phrase as a caller-owned `{input offset, length}` record over
the immutable raw frame. It performs an exact planning pass before publication,
searches available entries in reference order, and replaces a literal only on
a strictly longer match. Thus it emits the smallest numeric reference on equal
lengths without copying phrase bytes. Capacity and aggregate-limit failures
occur before output is modified.

The streaming reference decoder is a bounded frame adapter around the atomic
decoder. It collects no more than the declared frame's worst-case fixed-token
extent, validates and expands the complete frame into caller-owned staging
storage at `EndInput`, and only then drains raw bytes through arbitrary output
spans. Encoded tokens, phrase records, expansion stack, and staged raw bytes
are included in one checked aggregate before construction succeeds.

The matching streaming encoder collects exactly one declared raw frame,
invokes the deterministic reference encoder once, and drains its staged token
bytes without changing their representation. Raw storage, the four-bytes-per-
raw-byte worst-case token extent, and input-backed phrase-span records are all
caller-owned and covered by one checked construction aggregate. A full frame
may drain before the caller later confirms `EndInput`; a partial-frame `Flush`
does not create a format boundary.

The LZMW plus None profile builder derives encoder storage from the largest
actual frame and decoder storage solely from coupled local limits. Encoder
phrase-span capacity is at most raw bytes minus one. Decoder phrase capacity is
at most fixed tokens minus one, while a potentially nonempty frame always
reserves one additional iterative expansion-stack entry. Frame header, staged
payload/raw bytes, and typed records must fit the configured aggregate.

The outer frame-streaming decoder consumes the complete stream prefix and then
reuses one encoded-frame buffer, decoded-frame staging buffer, phrase-record
workspace, and expansion stack. Each frame header is validated contextually
before its bounded payload is collected. A frame is decoded atomically into
staging and only then drained, so a later corrupt frame cannot expose any of
its raw bytes or retract already committed earlier frames.

The matching outer frame-streaming encoder drains the canonical 80-byte prefix,
collects at most one raw frame, encodes that frame atomically into reusable
caller-owned storage, and drains it before proceeding. Full frames are emitted
without inventing an end boundary, a final short frame is committed only after
the declared final input arrives, and non-terminal `Flush` keeps a partial
frame open. Its bytes are identical to the one-shot complete-stream encoding.

The public-ABI completion matrix exercises required binary data classes,
deterministic re-encoding, frame-boundary neighbors, multiple frames, and
one-byte and mixed input/output chunking. The benchmark uses the same public
configuration, workspace query, factory, process, and destroy surface, verifies
a round trip before timing, and reports full-stream ratio, throughput, and
caller-owned workspace. These are local readiness checks, not release evidence.
The strengthened matrix also requires repeatable terminal states and
frame-atomic rejection of final-frame corruption, truncation, and trailing
bytes, with only earlier validated frames remaining committed.
A bounded LZMW decoder fuzz harness now covers both one-shot and outer
streaming decode with compile-smoke and permanent malformed regressions.
Coverage-guided sanitizer execution remains release evidence rather than a
claim made by the normal MSBuild suite.

The command-line tool selects LZMW explicitly through the public C ABI and
shares the generic bounded streaming loop and transactional output-file policy.
It never names an internal LZMW C++ type. The integration smoke verifies file
and empty round trips, overwrite rejection, and malformed-input cleanup.

### Published LZMW plus Blocked Huffman boundary

LZMW composition keeps the canonical four-byte reference stream as the exact
byte boundary between layers. Blocked Huffman may divide that region without
regard to reference alignment. Decode reconstructs the complete reference
region before the existing LZMW validator checks fixed-token alignment,
backward-only phrase references, adjacent-phrase productions, dictionary
freeze, and exact raw extent. Expansion and publication occur only after the
whole frame passes both entropy and dictionary validation.

For raw frame size `F`, reference staging is bounded by `4F`, generated phrase
records by the lesser of `max(F-1, 0)` and the configured maximum, and the
iterative expansion stack by the admitted phrase count plus one for a nonempty
frame. The checked opaque workspace profile now partitions encoder phrase
spans, or decoder Blocked Huffman views, LZMW phrase records, and expansion-
stack references without exposing their C++ layouts. Decoder phrase capacity
is derived from the maximum admitted serialized-token extent rather than only
the raw frame bound, so token-heavy malformed frames can reach the validator
and be rejected without an allocation or premature workspace failure. The
complete-frame validator and decoder now implement the decode half of this
boundary: header and descriptor extents are fixed first, entropy output is
staged, the full LZMW grammar is validated, aggregate workspace and raw
capacity are checked, and only then does iterative expansion publish bytes.
The matching planner first fixes the complete deterministic LZMW parse in
caller-owned phrase spans, serializes the exact four-byte references into
staging, and plans Blocked Huffman only over those bytes. The encoder publishes
the generic header, descriptors/models, and payload only after exact output
capacity is known. Complete-frame encode and decode are now implemented; outer
streaming adapters now reuse one frame input/output region, canonical-reference
staging, and the profile's typed views. The encoder drains the canonical
80-byte prefix, collects exactly one contextual raw frame, and drains its
complete encoded form. The decoder collects and validates one complete encoded
frame, decodes into raw staging, and only then drains it. One-byte boundaries,
nonterminal flush, exact finish, sticky malformed errors, and preservation of
already committed earlier frames are tested.

The public C factory now binds that profile to the common transform lifecycle.
Its size-tagged configuration fixes the known original size, raw-frame size,
entropy-block size, LZMW entry limit, and every decoder hard limit. The query
reports raw or serialized primary storage, a secondary region internally split
between canonical references and serialized or raw frame storage, and one
aligned opaque views region. Construction repeats profile validation and the
complete checked typed partition before publishing a handle; no entropy view,
phrase record, or expansion-stack representation crosses the ABI.
The public completion matrix now exercises binary data classes, deterministic
encoding, dictionary and frame-boundary neighbors, one-byte and mixed
chunking, repeated terminal calls, and frame-atomic final corruption,
truncation, and trailing-data rejection exclusively through that C factory.
A dedicated decoder fuzz boundary fixes every frame, reference, entropy-view,
phrase-record, expansion-stack, and total-output region before inspecting
serialized bytes. Byte-derived partial I/O and a fixed call ceiling exercise
the incremental state machine; permanent regressions retain complete canonical
truncation, extreme frame lengths, and an unavailable reconstructed reference.
The explicit `lzmw-blocked-huffman` CLI selector reaches the profile only
through its public C ABI. It shares the common temporary-file transaction,
bounded streaming loop, output-overwrite refusal, and cleanup on malformed or
trailing input. Its fixed one-MiB raw frame, 64-KiB entropy block, four-MiB
reference cap, 64-block limit, and 64-MiB aggregate policy are local admission
choices rather than new serialized parameters.
The dependency-free benchmark selects the same profile through the public C
ABI, verifies a full round trip before measurement, and reports full-stream
ratio, encode/decode throughput, each caller-owned workspace region, and their
directional peak. Reserved workspace totals are reported separately from the
decoder's active aggregate limit; they are intentionally not presented as the
same memory quantity.
Interoperability schema 7 appends this CLI representation to the frozen
schema-6 profile set as its eighteenth archive.

### Combined dictionary and entropy pipelines

The first combined profile is LZ77 variant 1 followed by Blocked Huffman
variant 1. It preserves the existing canonical LZ77 byte serialization and
feeds those bytes directly into frame-local fixed-size Huffman blocks. The
generic frame already separates raw, dictionary, descriptor/model, and payload
extents, so this profile requires no new algorithm ID or envelope revision.

The choice is a representative baseline composition, not a special coupling
between those algorithms. Other dictionary serializations can likewise feed a
byte-oriented entropy layer, but the public C ABI enumerates only profiles with
their complete format, bounds, validation, streaming, and test contracts.
Supporting standalone components does not automatically publish their complete
cross product.

Combined decode is frame-transactional: entropy output is staged and checked
against the declared dictionary extent, then the complete LZ77 token region is
validated before raw output is published. Streaming may retain already drained
earlier frames, but a failing current frame contributes no raw bytes.

On Windows, the canonical preset uses the Visual Studio 2026 generator and
MSBuild. Non-Windows presets use Ninja with the platform's selected compiler.
This avoids depending on localized MSVC `/showIncludes` text for incremental
dependency tracking while retaining Ninja's straightforward portable workflow
on platforms where compiler dependency files are locale-independent.

The Windows preset also enables `MARC_MSVC_MULTIPROCESS_COMPILE`. The option
adds `/MP` only to MSVC C and C++ compile steps, allowing independent
translation units inside a large target to compile concurrently. It is OFF for
non-preset and non-MSVC configurations unless selected explicitly, and may be
disabled on memory-constrained builders. This complements build-tool target
parallelism; it does not alter source, ABI, or generated stream bytes.

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

The cross-profile implementation and release-evidence status is maintained in
`docs/baseline-readiness.md`. Local completion requires the same bounded public
transform evidence for every required codec; external interoperability,
representative measurements, longer sanitizer campaigns, and final similarity
review remain separate release gates.

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

CRC-32C is the first concrete `IHashAlgorithm`. Its clear byte-at-a-time
reference update has bounded constant state, no allocation, and no platform
intrinsics; `HashTap` supplies lifecycle enforcement and committed-byte
accounting. The version 1.1 `checksum-raw` profile is the sole current stream
integration and fixes one per-frame CRC-32C descriptor over uncompressed bytes.
Other target/scope combinations remain reserved and cannot activate a nonzero
hash region.

SHA-256 is the corresponding format-neutral cryptographic hash primitive. It
buffers at most one 64-byte message block, checks the FIPS 64-bit bit-length
bound before accepting a complete update span, and finalizes through a private
state snapshot so observation does not mutate the incremental state. The clear
reference transform uses only fixed arrays and portable 32-bit integer
operations; hardware acceleration is a later interchangeable optimization.

The frame layer also owns a fixed-size hash-descriptor parser and serializer.
It recognizes implemented algorithm IDs, target and scope vocabularies, exact
digest sizes, flags, and reserved bytes without allocation. Parsing publishes
only a fully validated value. This is deliberately separate from stream
activation: version 1.0 rejects hash regions, while version 1.1 currently
defines inclusion ranges and digest placement only for `checksum-raw`.

Descriptor-region handling adds no container allocation. A first pass validates
each fixed record and its strict `(target, scope, algorithm ID)` ordering; only
then does a second pass populate the caller's bounded descriptor span. The same
canonical-order validator runs before serialization, so malformed metadata
cannot partially publish parsed objects or bytes.

Hash-aware prefix work is isolated behind version-specific 1.1 helpers. The
ordinary header parser remains a strict 1.0 gate used by every version 1.0
stream adapter, so descriptor metadata cannot shift their expected frame
offset. The 1.1 helper validates only the fixed prefix and declared bounded
region extent; the `checksum-raw` profile supplies the separate complete-stream
policy and CRC connection.

The initial hash profile component narrows the broad descriptor vocabulary to
one per-frame CRC-32C over logical uncompressed bytes. It validates the exact
descriptor set and declared trailer extent, then generates or verifies the
four-byte trailer without allocation. Keeping this boundary independent lets
`checksum-raw` connect its already validated raw staging span without
duplicating CRC lifecycle or inclusion-range decisions, and leaves the same
component available to a separately specified future profile.

The version 1.1 frame-header gate couples three declarations:
stream descriptor-region size, parsed descriptor objects, and frame trailer
extent. Its version-specific entry point validates all three and includes the
trailer in bounded frame accounting. Version 1.0 codec frames remain attached
to the strict 1.0 gate, while `checksum-raw` uses this version 1.1 gate. The 1.0
gate rejects descriptor objects as well as a nonzero trailer.

The raw-checksum reference stream is the first end-to-end consumer of the
version 1.1 components. It owns no dynamic storage: encoding plans exact
extents before writing, while decoding scans the caller's serialized span
twice. The first scan validates all frame headers and CRC trailers; the second
copies raw payload
spans to caller output. This establishes complete-stream atomicity independently
of dictionary and entropy implementations.

Its fuzz boundary invokes the same strict two-pass decoder with one fixed output
array and local limits; arbitrary bytes cannot request workspace or alter the
harness call count. Normal builds compile the boundary without a fuzz runtime,
while the separate Clang configuration links libFuzzer and sanitizers.

The incremental raw-checksum path uses one caller-owned serialized-frame
workspace in each direction. Encoding collects raw bytes at the payload offset
and later fills the surrounding header and CRC trailer. Decoding buffers
header, payload, and trailer together, verifies the complete frame, and only
then drains the raw payload. This is transactional per frame rather than across
the whole stream: earlier verified frames may already be visible when a later
frame fails.

The raw-checksum profile is the construction boundary above these transforms.
It emits the canonical 1.1 header and descriptor and computes one serialized
frame workspace. Encoder sizing uses the actual largest frame; decoder sizing
uses only local limits and therefore occurs before parsing untrusted bytes.

The public C adapter exposes this fixed profile through a distinct size-tagged
configuration. It has one caller-owned primary workspace and no secondary or
views region. Encoding queries the exact profile size; decoding queries the
limits-only conservative size. The adapter offers no hash selector, so every
successful encoder construction emits the same canonical CRC descriptor.

The command-line adapter names the profile `checksum-raw` and reaches it only
through that public C ABI. Its fixed 1 MiB frame policy maps to one-frame raw,
dictionary, and aggregate workspace limits before querying storage. Existing
temporary-file publication means a malformed checksum stream leaves no partial
destination even though the streaming decoder may have produced verified bytes
internally before a later-frame error.

The benchmark adapter selects the same `checksum-raw` public ABI and reports it
as a framing-plus-CRC baseline. Its destination bound uses one raw byte per
input byte plus the 80-byte prefix and 60 bytes per frame. Peak codec workspace
therefore reflects the single caller-owned serialized-frame span rather than
the benchmark's corpus or result vectors.

Interoperability schema 4 names codec set `marc-cli-v4`, preserves the exact
thirteen-profile schema-3 order, and appends the LZSS and LZ78 Blocked Huffman
profiles. Schema 5 names `marc-cli-v5` and appends LZW plus Blocked Huffman to
that frozen fifteen-profile order. The external verifier dispatches each
manifest's exact versioned set through the public CLI. Schema 6 names
`marc-cli-v6` and appends LZD plus Blocked Huffman to the frozen sixteen-profile
schema-5 order. Schema 7 names `marc-cli-v7` and appends LZMW plus Blocked
Huffman to the frozen seventeen-profile schema-6 order. Schema 8 names
`marc-cli-v8` and appends LZ77 plus Adaptive Huffman to the frozen eighteen-
profile schema-7 order. Schemas 1 through 7 remain frozen at seven, eight,
thirteen, fifteen, sixteen, seventeen, and eighteen profiles respectively.

The checksum profile's public-ABI completion matrix is the consolidated local
audit above the component, streaming, C, CLI, fuzz, benchmark, and
interoperability tests. It proves required binary data classes, deterministic
encoding, representative short-buffer schedules, stable terminal states, and
frame-atomic suppression for checksum failure, truncation, and trailing data.
This is local implementation evidence rather than cross-platform release
completion.

## Buffered incremental reference encoder

The first `ProcessResult`-based Blocked Huffman encoder is a correctness
reference with caller-owned whole-input and whole-encoded-stream workspaces.
It accepts arbitrarily split input and drains arbitrarily small output spans,
but does not emit bytes before `EndInput`. Non-terminal `Flush` therefore does
not alter or close a frame; `ResetBlock` is rejected as unsupported.

This buffered path is retained as a whole-stream correctness reference. Its
encoded bytes must match the one-shot reference for every chunking pattern. The
frame-at-a-time implementation below reduces workspace requirements while
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
argument, unsupported, and limit exceeded. The public C adapter therefore does
not expose internal parser or codec-specific enumerations.

The standalone Blocked Huffman fuzz boundary covers dictionary-none stream
validation that the combined profile cannot reach. Strict and frame-committing
decoders share eight fixed views, bounded byte arrays, a 24-bit code-length cap,
and a 512-node decode-table cap; byte-derived chunks and a checked call ceiling
turn invalid progress or stalls into reproducible findings.

The command-line adapter names this profile `blocked-huffman` and reaches it
only through the public C workspace query, transform factory, and process
contract. Its one MiB frame and 65,536-symbol block policy is translated into
checked local decoder bounds before any archive bytes are parsed. File output
retains the common temporary-file commit boundary. This selector does not
silently change the existing versioned interoperability bundle codec set.

The benchmark adapter uses the same public profile name and fixed frame/block
policy. It calculates a raw-fallback capacity from the 64-byte stream prefix,
one frame header, one descriptor per possible block, and one payload byte per
input byte. An untimed full round trip precedes timing; reported peak workspace
is the larger public encoder or decoder requirement, including aligned block
views.

The public-ABI completion matrix consolidates local readiness evidence above
the component tests. It covers every one-byte value, representative binary and
generated data, both sides of frame and entropy-block boundaries, deterministic
re-encoding, one-byte and mixed chunk schedules, repeatable EndOfStream, and
frame-atomic rejection of a malformed final frame, truncation, and trailing
data. This closes local implementation evidence without claiming external
cross-platform release completion.

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

The bounded Adaptive fuzz boundary sends every case through both the strict
whole-stream decoder and the frame-committing decoder. Fixed encoded-frame,
decoded-frame, and aggregate-output arrays enforce the same local policy in
both paths; byte-derived chunk schedules and a checked call ceiling turn
partial-I/O invariant failures or stalls into reproducible findings.

The command-line adapter selects FGK variant 1 as `adaptive-huffman` and uses
only the public C configuration, workspace query, factory, and process calls.
Its one MiB frame policy reserves the profile's conservative 33 payload bytes
per symbol plus the fixed descriptor. Decoder allocation is derived from those
local limits before input parsing, and the common temporary-file commit policy
prevents failed streams from publishing partial files.

The benchmark adapter uses that same FGK profile and public C lifecycle. Its
capacity bound includes the 64-byte stream prefix, one 56-byte frame header and
16-byte descriptor per frame, and 33 payload bytes per input symbol. It runs an
untimed full round trip before measuring process calls and reports the larger
encoder/decoder caller-owned workspace; the views extent remains zero because
the fixed FGK tree is transform-owned.

The public-ABI completion matrix consolidates Adaptive local-readiness evidence
above the format and tree tests. It covers every one-byte symbol, binary and
generated data, frame boundaries, deterministic re-encoding, one-byte and
mixed chunk schedules, repeatable EndOfStream, and frame-atomic rejection of a
malformed final frame, truncation, and trailing bytes. This closes the local
implementation loop without claiming external release evidence.

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

The bounded Dynamic Range fuzz boundary drives both the strict whole-stream and
frame-committing decoders under identical fixed storage limits. It additionally
pins the accepted model total to 32,768, while byte-derived chunk schedules and
a checked call ceiling expose invalid partial-I/O progress or stalls as
reproducible findings.

The command-line adapter names the adaptive order-0 profile `dynamic-range`
and reaches it only through the public C lifecycle. Its one MiB frame policy
uses the exact model-total limit 32,768, a conservative `2*n+5` payload bound,
and one fixed descriptor per frame. Decoder workspaces come from these local
limits before parsing, while the shared temporary-file boundary prevents any
failed stream from publishing a destination.

The benchmark adapter uses the same adaptive order-0 profile and public C
lifecycle. Its output bound separates two bytes per input symbol from the
per-frame five-byte termination, 16-byte descriptor, and 56-byte header, with
one 64-byte stream prefix. An untimed full round trip gates measurement, and
the reported peak is the larger public direction workspace; views remain zero
because the fixed model is transform-owned.

The public-ABI completion matrix consolidates Dynamic Range local-readiness
evidence above the model and interval tests. It covers every one-byte symbol,
binary and generated data, frame boundaries, deterministic re-encoding,
one-byte and mixed chunk schedules, repeatable EndOfStream, and frame-atomic
rejection of a malformed final frame, truncation, and trailing bytes. This
closes the local implementation loop without claiming external release
evidence.

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

The bounded tANS fuzz boundary supplies the strict and frame-committing
decoders with eight aligned views, fixed byte arrays, and a 4,096-entry table
cap. It exercises malformed state and additional-bit transitions under
byte-derived chunk schedules, while a checked call ceiling turns invalid
progress or stalls into reproducible findings.

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

The bounded rANS fuzz boundary applies the same fixed policy to the strict and
frame-committing decoders. Eight aligned block views, a 4,096-entry table cap,
and separate descriptor-plus-payload, frame, and output arrays prevent
serialized metadata from controlling allocation; byte-derived chunk schedules
and a checked call ceiling expose invalid progress or stalls.

The command-line adapter names scalar variant 1 `rans` and uses only the public
C configuration, workspace query, factory, and process lifecycle. Its one MiB
frame and 65,536-symbol block policy permits at most 16 blocks. Capacity is
bounded by one payload byte per symbol, one eight-byte final state and one
528-byte descriptor per block; decoder views are allocated from the queried
count and alignment before parsing input. The shared temporary-file boundary
prevents failed streams from publishing a destination.

The benchmark adapter uses the same scalar profile and public C lifecycle. Its
capacity bound includes the 64-byte stream prefix, one 56-byte header per
frame, one payload byte per input symbol, and an eight-byte state plus 528-byte
descriptor for each of at most 16 blocks per frame. An untimed complete round
trip gates measurement; direction and peak workspace totals include the
queried aligned decoder views region.

The public-ABI completion matrix consolidates scalar rANS local-readiness
evidence above the normalization and state tests. It covers every one-byte
symbol, one-symbol and generated data, block and frame boundaries,
deterministic re-encoding, one-byte and mixed chunk schedules, repeatable
EndOfStream, and frame-atomic rejection of a malformed final frame, truncation,
and trailing bytes. Queried aligned views are used throughout. This closes the
local implementation loop without claiming external release evidence.

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

The command-line adapter names tabled variant 1 `tans` and uses only the public
C configuration, workspace query, factory, and process lifecycle. Its one MiB
frame and 65,536-symbol block policy permits at most 16 blocks. Capacity is
bounded by 12 transition bits per input symbol, one two-byte state and one
528-byte descriptor per block; decoder views are allocated from the queried
count and alignment before parsing input. The shared temporary-file boundary
prevents failed streams from publishing a destination.

The benchmark adapter uses the same tabled profile and public C lifecycle. Its
capacity bound includes the 64-byte stream prefix, one 56-byte header per
frame, `ceil(3*n/2)` transition bytes, and a two-byte state plus 528-byte
descriptor for each of at most 16 blocks per frame. An untimed complete round
trip gates measurement; direction and peak workspace totals include the
queried aligned decoder views region.

The public-ABI completion matrix consolidates tabled tANS local-readiness
evidence above normalization, spread, and transition-table tests. It covers
every one-byte symbol, one-symbol and generated data, block and frame
boundaries, deterministic re-encoding, one-byte and mixed chunk schedules,
repeatable EndOfStream, and frame-atomic rejection of a malformed final frame,
truncation, and trailing bytes. Queried aligned views are used throughout. This
closes the local implementation loop without claiming external release
evidence.

### C transform ABI

The stateful C ABI exposes the fixed version 1.1 raw-checksum profile plus
Blocked Huffman, Adaptive Huffman, Dynamic Range, rANS, tANS, LZ77, LZSS, LZ78,
LZW, LZD, and LZMW variant 1 through
separate versioned, size-tagged configuration, workspace-query, and factory
functions. All profiles construct the same opaque transform type and share its
process and destroy operations. The raw-checksum profile uses one serialized
frame workspace in either direction. Other encoder workspaces hold one raw and
one serialized frame, while decoder workspaces hold one serialized and one
decoded frame. Blocked Huffman, rANS, and tANS use aligned internal block-view
arrays. LZ78 and LZW use opaque aligned phrase-table workspaces. LZD and LZMW
each use one opaque aligned region for input-backed phrase records when encoding
and partition that region into phrase records plus an iterative expansion stack
when decoding. Adaptive Huffman, Dynamic Range, LZ77, and LZSS need no views
workspace. These buffers remain caller-owned and must outlive the handle.

Only the small opaque handle and its C++ implementation object are allocated by
the library with non-throwing allocation. Processing uses caller input/output
spans and maps stable core status and error categories into fixed C constants.
Every exported function is `noexcept` when compiled as C++.

### LZ77 plus Blocked Huffman validation boundary

The first combined-pipeline component accepts exactly one serialized frame and
has no raw-output parameter. It reuses the generic frame parser, transactional
Blocked Huffman controller and decoder, and canonical LZ77 token validator in
that order. Entropy output is written only to caller-owned dictionary staging;
raw-byte reconstruction is deliberately deferred to the later decoder step.

The caller supplies both the block-view array and dictionary staging. Their
required extents are derived from the validated frame header. Descriptor/model
bytes, entropy payload bytes, dictionary staging, and the typed view array form
one checked aggregate workspace bound. This preserves bounded memory while
keeping all allocation policy outside the validator.

The complete-frame raw decoder is a thin commit stage over this boundary. It
first runs the same validator, checks raw destination capacity, and then passes
only the validated dictionary staging to the standalone transactional LZ77
decoder. Raw output is therefore unreachable from malformed generic headers,
entropy metadata, entropy payloads, or token streams. The standalone decoder's
prevalidation also protects the raw destination if its API is used separately.

The matching frame encoder exposes an exact planner because the generic header
must precede entropy descriptors and payloads. Planning first emits canonical
LZ77 tokens into caller-owned staging, then computes the Blocked Huffman model
choice and exact extents for every dictionary-byte block. Encoding repeats only
the deterministic entropy traversal into already-sized descriptor and payload
regions. No raw-frame-sized hidden allocation or duplicate token copy is used.

At complete-stream scope, the combined controller places the fixed LZ77
parameter region immediately after the stream header and reuses frame-local
staging and views. Encoding plans all frames before emitting the prefix.
Decoding first validates the full serialized stream without raw publication,
then decodes it in a second pass. This gives whole-stream atomicity for the
one-shot API while keeping memory bounded by the largest frame and its entropy
block count rather than total stream size.

The combined streaming encoder is the incremental counterpart of the one-shot
planner. It owns no variable-size storage: callers provide raw-frame,
dictionary-token, and serialized-frame spans. The transform drains the prefix
and each completed frame through partial output buffers, while frame collection
continues across non-terminal `Flush`. Dictionary and entropy state are rebuilt
only when a complete outer frame is prepared.

The combined streaming decoder mirrors this with serialized-frame,
dictionary-byte, raw-frame, and block-view workspaces. It never drains directly
from dictionary decode: a complete frame reaches raw staging only after both
entropy and LZ77 validation succeed. Its source-ended latch is independent of
output draining, so a terminal input indication survives any number of
`NeedOutput` calls.

The combined profile layer centralizes workspace arithmetic for callers and the
public C ABI. Encoder requirements are exact worst-case bounds for the selected
known-size stream and frame/block configuration. Decoder requirements are
conservative bounds derived solely from local limits; untrusted serialized
headers never influence allocation requests before parsing.

### LZ77 plus Adaptive Huffman validation boundary

The Adaptive composition preserves the same canonical 16-byte LZ77 token
boundary but resets one FGK tree for every nonempty outer frame. Complete
entropy decode and LZ77 token validation occur in caller-owned token staging;
raw reconstruction then completes in a separate private frame region before
the incremental decoder may drain any current-frame byte.

The public C factory exposes no entropy-block parameter or aligned views
workspace. Encoding partitions its secondary byte region into token staging
and serialized-frame storage; decoding partitions it into token staging and
private raw storage. The requirements query derives both partitions from the
known-size encoder profile or trusted local decoder limits before construction.

Outer `max_frame_size` remains a raw-byte limit. The Adaptive primitive receives
a private limits view sized to the already validated canonical token extent,
because its standalone symbol unit is a byte at the entropy boundary. This
unit translation neither enlarges the untrusted outer frame allowance nor
changes compressed-payload, dictionary, aggregate, or LZ limits.

The public completion matrix fixes 64-byte raw frames and audits all required
binary data classes through the C ABI. It compares unchunked output with
one-byte and mixed chunk schedules, repeats successful terminal calls, and
proves that corruption, truncation, or trailing data in a fourth frame can
publish only the first three complete frames.

The bounded fuzz boundary fixes serialized input, token staging, raw staging,
and final output before parsing. A valid profile prefix admits the remaining
extent to complete-frame private-staging decode, while every input also reaches
the incremental decoder under byte-derived chunking and a fixed call ceiling.

The transactional CLI selector uses the 64-KiB reference frame through the
public C ABI. It obtains both workspace extents from the requirements query and
commits the temporary output path only after complete stream termination.

The benchmark selects that identical fixed profile through the same public C
lifecycle. Its checked capacity calculation uses the 64-KiB frame cadence and
the conservative 528-byte Adaptive payload bound for every raw byte. A complete
round trip precedes separate encode/decode timing; the reported peak workspace
is the larger queried two-region sum and excludes corpus and result buffers.

Interoperability schema 8 preserves the frozen schema-7 order and appends this
CLI representation as its nineteenth archive. Generation verifies a local
round trip; foreign verification checks manifest order and hashes, decodes to
the common fixture, and requires byte-identical local re-encoding.

### LZSS plus Blocked Huffman validation boundary

The second selected composition begins with the same deliberately narrow
decoder-side boundary. It accepts one exact frame, entropy-decodes into
caller-owned dictionary staging, and validates the complete variable-length
LZSS token stream against the frame's declared raw size. It exposes no raw
output and performs no input-controlled allocation.

This boundary demonstrates that composition is not coupled to LZ77's fixed
16-byte tokens. Descriptor/model bytes, payload bytes, staged LZSS bytes, and
the aligned block-view array are checked as one aggregate workspace.

The matching exact planner first determines the variable LZSS token extent,
emits those tokens once into caller-owned staging, and then plans Blocked
Huffman over the actual bytes. Only after the generic header and both entropy
regions have exact extents does the frame encoder check serialized capacity and
publish output.

The raw frame decoder is a commit stage over the strict validator. It checks
raw destination capacity only after the complete entropy output has passed the
LZSS token validator, then gives that validated staging to the standalone
transactional LZSS decoder. Neither malformed outer layers nor a short raw
destination can publish a raw prefix. Stream controllers and public adapters
continue from these frame boundaries without weakening their commit order.

The known-size complete-stream controller writes the normal 64-byte stream
header followed by the 16-byte LZSS parameter region and then consecutive
combined frames. Encoding plans every frame before publishing the prefix.
Decoding parses configuration into local objects and validates every frame in a
first pass with no raw output; only a successful complete scan permits the
second commit pass. A malformed later frame therefore cannot expose an earlier
raw frame or partially replace caller-visible configuration.

The incremental encoder owns no variable-size allocation. Caller-provided raw
frame, LZSS-token, and serialized-frame spans bound its state. It drains the
canonical prefix and each complete encoded frame through arbitrary output
capacity, including one byte. Nonterminal `Flush` does not close a partial
frame, and an `EndInput` indication remains latched until the final serialized
frame has completely drained. Its output is byte-identical to the known-size
encoder for every input/output chunk schedule.

The incremental decoder collects the fixed prefix, one frame header, and one
complete frame body into caller-owned storage. It entropy-decodes and validates
the complete LZSS token stream, reconstructs raw into frame staging, and only
then drains that frame through partial output capacity. This intentionally
commits validated earlier frames even if a later frame is malformed, while no
byte from the malformed frame is exposed. A terminal indication remains
latched across `NeedOutput` and becomes truncation after a nonfinal frame has
finished draining without more serialized input.

The internal profile factory fixes the version 1.0 algorithm and variant IDs,
serializes the canonical 16-byte LZSS parameter record, and calculates exact
known-size encoder workspace. If `F` is the largest raw frame, the worst-case
LZSS staging extent is `2F`. For entropy block size `E`, the maximum number of
blocks is `ceil(2F/E)`, and serialized frame staging is exactly the 56-byte
generic header, one 16-byte descriptor per block, and the `2F` raw-fallback
payload. The factory rejects every arithmetic, per-region, block-count, and
aggregate-workspace limit before returning a configuration.

Decoder workspace calculation deliberately has no serialized configuration
argument. It derives the serialized-frame, token-staging, raw-frame, and typed
block-view capacities only from trusted local limits. This makes the query safe
before an untrusted stream header is parsed and gives the C adapter an
opaque allocation contract without changing the transform's four distinct
internal spans.

The dedicated C adapter publishes this configuration through the ordinary
opaque transform lifecycle. It preserves the three-workspace ABI: encoder
secondary storage is partitioned into LZSS token staging and serialized-frame
staging; decoder secondary storage is partitioned into token and raw staging;
only decode uses the aligned views region. The adapter re-runs the checked
profile calculation at creation and never exposes private C++ view layout.

The command-line adapter names this profile `lzss-blocked-huffman` and reaches
it only through the public C ABI. It fixes one-MiB raw frames, 64-KiB entropy
blocks, and profile-specific limits derived from the two-byte-per-raw-byte LZSS
worst case. File I/O therefore cannot bypass profile validation or introduce a
second allocation policy. Failed decode removes the temporary destination even
when earlier validated frames were already drained internally.

A bounded fuzz boundary, completion matrix, and interoperability entry remain
separate admission steps and must not be inferred from CLI availability.

The benchmark adapter uses the identical public profile and fixed policy. Its
encoded destination bound counts the 80-byte prefix, two token bytes per raw
byte, each generic frame header, and every worst-case entropy descriptor. It
times only `marc_transform_process`; allocation, transform lifecycle, file I/O,
and mandatory round-trip verification remain outside the timed interval. Peak
workspace is the larger queried sum of primary, secondary, and views regions.

The bounded fuzz adapter invokes both the strict two-pass stream decoder and
the incremental frame-committing decoder. It truncates each supplied case to
8 KiB, uses fixed stack storage for at most 4 KiB of raw and token data, one
1-KiB frame, and eight entropy views, and derives partial-I/O chunk sizes from
the bytes. An independent call ceiling converts any stalled state machine into
a reproducible failure rather than an unbounded run.

### Published LZ78 plus Blocked Huffman frame boundary

The composition now has matching frame planner/encoder and validator/decoder
boundaries. Encoding fixes the LZ78 parse in token staging before Blocked
Huffman planning; decoding entropy-decodes a complete frame into staging, then
validates phrase references and the exact derived raw extent before
publication. Unlike the first two compositions, both directions require an
aligned LZ78 phrase table; decoding additionally requires aligned Blocked
Huffman block views.

This makes typed-workspace composition an explicit admission boundary rather
than an implementation detail. The public adapter retains the common
primary/secondary/views C ABI shape, and its opaque views region is partitioned
with checked alignment and size arithmetic for both private record types. The
internal frame API accepts separate typed spans so capacity and aggregate-
memory failures occur before entropy output or serialized output.
Profile sizing fixes the three-region ABI: frame bytes occupy the
primary and secondary regions, while the aligned opaque views region contains
an encoder phrase table or a decoder block-view array followed by checked
padding and the decoder phrase table. Partition helpers rederive and validate
the complete layout before exposing typed spans. The incremental encoder and
decoder now consume those spans directly. They preserve the common 80-byte
prefix and frame state machine under one-byte input and output, and the decoder
publishes raw bytes only after a whole frame has passed entropy and phrase-graph
validation. The public C factory exposes only byte counts and alignment, then
delegates the opaque-region partition back to the checked internal helpers.
The `lz78-blocked-huffman` profile is therefore callable through the C ABI;
its completion matrix proves required binary classes, chunk independence,
determinism, stable terminal behavior, and transactional malformed-final-frame
rejection through that ABI. Its bounded decoder fuzz target fixes every byte,
typed-workspace, and call-count limit before processing arbitrary input. The
CLI and benchmark reach the profile only through the C ABI and obtain all
three workspace extents from its requirements query. The benchmark verifies a
round trip before timing and reports complete-stream ratio, directional
throughput, and the larger caller-owned workspace total. Interoperability
schema 4 covers the same public profile through deterministic foreign decode
and local re-encode checks.

### Published LZW plus Blocked Huffman boundary

LZW's canonical dictionary output is a packed variable-width bitstream rather
than a fixed-width token array. Composition nevertheless remains byte-oriented:
the LZW encoder finishes its frame-local code stream, including zero padding to
the next byte, before Blocked Huffman divides those exact bytes into entropy
blocks. Entropy block boundaries therefore never split a byte but need not
coincide with LZW code boundaries.

Decoding reverses this transactionally. Blocked Huffman reconstructs the exact
packed byte region into staging; the ordinary LZW validator then checks the
width schedule, dictionary references, `KwKwK`, final padding, and exact raw
extent before publication. This preserves both layers' existing validators
instead of teaching either layer the other's token grammar.

The frame boundary now implements this ordering in both directions. Encoding
first completes the standalone LZW plan and writes the exact packed bytes into
caller-owned staging; Blocked Huffman planning and generic-header construction
then consume that immutable span. The frame records the actual packed extent,
while the conservative format bound remains an allocation admission rule.

Decoding uses separate caller-owned Blocked Huffman views, packed-byte staging,
and LZW phrase entries. It checks all three capacities and their aggregate
bytes before entropy output, then validates LZW completely before checking raw
output capacity. The
9-to-10-bit width-transition test crosses thirty independent entropy blocks,
demonstrating that block boundaries do not become code boundaries.

The internal profile now resolves the typed-workspace boundary. Its encoder
requirements expose an aligned LZW encoder-entry region. Decoder requirements
combine Blocked Huffman views and a separately aligned LZW phrase table in one
opaque region, recording the phrase offset, total bytes, and maximum alignment.
Partition helpers recompute that layout before exposing either span. The format,
complete frame boundary, sizing, and safe partition feed bounded streaming
transforms. The encoder buffers one raw frame and its finalized representation;
the decoder buffers one serialized frame and reconstructs it privately before
draining raw output. Consequently a malformed later frame cannot publish a
partial raw frame or alter an earlier committed one. The small C ABI now admits
that exact implementation through a direction-specific requirements query and
factory; no second codec construction path or private C++ record layout crosses
the ABI. Its public completion matrix now covers required binary classes,
deterministic and chunk-independent streams, stable terminal behavior, and
transactional malformed-final-frame rejection. Empty and one-byte inputs also
fix the zero-entry encoder view contract at zero bytes with neutral alignment
one. A dedicated decoder fuzz target fixes serialized input, raw output,
frame, packed-code staging, entropy views, LZW phrases, aggregate memory, and
process-call limits before accepting arbitrary bytes.
The CLI reaches this composed profile only through the public C ABI. It fixes
one-MiB raw frames and 65,536-symbol entropy blocks, then obtains all three
workspace extents and alignment from the requirements query. The benchmark
uses that same public profile and reports the queried direction-specific
regions after verifying a complete round trip. Schema 5 appends the resulting
CLI representation to the frozen schema-4 profile set.

### Published composed-profile evidence

The published LZ77 plus Blocked Huffman public-ABI completion matrix closes the
local implementation loop by driving required binary data classes through
queried workspaces and both stream directions. It repeats encoding for byte
identity and compares
multi-frame output across one-byte and mixed chunk schedules. This is a local
readiness assertion, not a substitute for sanitizer campaigns or portability
evidence on independent toolchains and architectures.

The first independent-toolchain check builds the complete project with Clang's
GNU-style driver and Ninja on Windows, then runs the same optimized suite used
by the MSVC build. As a separate representation check, the MSVC and Clang
command-line tools encode one common input through every public CLI profile;
all nineteen schema-8 archives must compare byte for byte. This establishes
compiler independence on one architecture, while cross-architecture evidence
remains a separate gate.

CI turns this check into an externally consumable protocol. Each reference job
generates the same 8,193-byte binary fixture, validates a local round trip for
all nineteen schema-8 profiles, and uploads the fixture, complete archives, and
a JSON manifest containing the source revision. The external verifier first
validates manifest bounds and hashes, then decodes foreign archives and
independently re-encodes the fixture with the local CLI. Artifact hashes detect
transfer mistakes but are not authentication.

### Published LZD plus Blocked Huffman implementation evidence

The LZD composition has a complete-frame validator and
transactional decoder. Blocked Huffman first reconstructs the entire canonical
eight-byte LZD token region into bounded caller-owned staging. The ordinary LZD
validator then checks token extent, reference ordering, terminal form, phrase
limits, and exact declared raw size before the decoder checks destination and
iterative expansion-stack capacities or publishes raw bytes.

Phrase workspace is derived from both serialized tokens and the declared raw
frame size. A terminal one-byte frame stores no phrase record, while a
right-present pair necessarily accounts for at least two raw bytes. Validation
and decoding count their distinct caller-owned regions under checked aggregate
limits. The public decoder retains this transactional boundary before exposing
raw bytes through the streaming C ABI.

The matching internal planner and encoder now complete the LZD parse and write
the exact canonical token region before entropy planning. Blocked Huffman sees
only that immutable byte span, so its blocks may split a token without changing
dictionary parsing. The planner derives the generic header and final serialized
extent from the chosen block representations; the encoder refuses a short final
destination before publishing any frame byte. The streaming transform and
public factory reuse this exact frame representation.

The internal profile now gives the caller-owned third region a stable typed
shape. Encoding exposes aligned LZD encoder records. Decoding exposes Blocked
Huffman views followed by separately aligned LZD phrase records and iterative
expansion references; both offsets and the complete extent are rederived before
any span is returned. Primary raw/frame buffers and secondary token staging
remain byte regions. The streaming adapter and C ABI construction path use
these requirements without exposing the private layouts.

The bounded incremental transforms now consume those exact profile regions.
The encoder collects one raw frame and drains only its completed serialized
representation. The decoder collects and validates one complete serialized
frame, expands into private raw storage, and then drains it. Consequently a
malformed later frame cannot partially publish that frame or retract earlier
output. Chunking down to one byte does not alter the stream, nonterminal flush
does not shorten a frame, and reset remains an unsupported cross-layer request.
The public C factory now admits this profile through the common transform
handle and three caller-owned regions. The requirements query exposes only byte
extents and maximum alignment. Factory construction repeats profile admission
and checked opaque partitioning before publishing a handle, so entropy views,
LZD phrase records, and expansion references never become ABI types. CLI,
benchmark, decoder fuzzing, completion, and interoperability were admitted
independently against this same factory.

The public-ABI completion matrix now fixes required binary data classes,
determinism across one-byte and mixed chunking, stable repeated termination,
and transactional final-frame rejection. Its 64-byte frame profile derives a
256-byte maximum LZD token region and 32 phrase entries from the fixed pair
grammar rather than borrowing another dictionary codec's bounds. The bounded
decoder fuzz target, CLI, benchmark, and schema-6 entry cover the same profile.

The bounded decoder fuzz boundary preallocates the complete combined working
set: serialized frame, token staging, raw staging, entropy views, LZD phrase
records, expansion references, and final output. Serialized input cannot alter
those capacities. Byte-derived chunk schedules exercise partial I/O, while a
fixed call ceiling converts any stalled state machine into a reproducible
failure. This admits fuzzing without changing the public format or ABI.

The CLI selector `lzd-blocked-huffman` is a fixed public-ABI adapter. It uses
one-MiB raw frames, 64-KiB entropy blocks, the exact four-MiB LZD token bound,
64 entropy blocks, 65,536 phrase entries, and the common 64-MiB aggregate
policy. Workspace bytes and alignment come only from the public requirements
query. The existing temporary-file protocol keeps malformed or trailing input
from publishing a partial destination.

The benchmark selects the identical fixed profile through the same public C
ABI. It verifies a complete round trip before timing, measures encoding and
decoding separately, and reports complete-stream ratio plus direction-specific
primary, secondary, and aligned views extents. Peak workspace is the larger
queried three-region sum; benchmark inputs and output buffers remain outside
that metric.
