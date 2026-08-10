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
profile schema-7 order. Schema 9 names `marc-cli-v9` and appends LZSS plus
Adaptive Huffman to the frozen nineteen-profile schema-8 order. Schema 10 names
`marc-cli-v10` and appends LZ78 plus Adaptive Huffman to the frozen twenty-
profile schema-9 order. Schema 11 names `marc-cli-v11` and appends LZW plus
Adaptive Huffman to the frozen twenty-one-profile schema-10 order. Schema 12
names `marc-cli-v12` and appends LZD plus Adaptive Huffman to the frozen twenty-
two-profile schema-11 order. Schema 13 names `marc-cli-v13` and appends LZMW
plus Adaptive Huffman to the frozen twenty-three-profile schema-12 order.
Schema 14 names `marc-cli-v14` and appends LZ77 plus Dynamic Range to the frozen
twenty-four-profile schema-13 order. Schema 15 names `marc-cli-v15` and appends
LZSS plus Dynamic Range to the frozen twenty-five-profile schema-14 order.
Schema 16 names `marc-cli-v16` and appends LZ78 plus Dynamic Range to the frozen
twenty-six-profile schema-15 order. Schema 17 names `marc-cli-v17` and appends
LZW plus Dynamic Range to the frozen twenty-seven-profile schema-16 order.
Schema 18 names `marc-cli-v18` and appends LZD plus Dynamic Range to the frozen
twenty-eight-profile schema-17 order. Schema 19 names `marc-cli-v19` and
appends LZMW plus Dynamic Range to the frozen twenty-nine-profile schema-18
order. Schema 20 names `marc-cli-v20` and appends LZ77 plus rANS to the frozen
thirty-profile schema-19 order. Schema 21 names `marc-cli-v21` and appends
LZSS plus rANS to the frozen thirty-one-profile schema-20 order. Schema 22
names `marc-cli-v22` and appends LZ78 plus rANS to the frozen thirty-two-
profile schema-21 order. Schema 23 names `marc-cli-v23` and appends LZW plus
rANS to the frozen thirty-three-profile schema-22 order. Schema 24 names
`marc-cli-v24` and appends LZD plus rANS to the frozen thirty-four-profile
schema-23 order. Schema 25 names `marc-cli-v25` and appends LZMW plus rANS to
the frozen thirty-five-profile schema-24 order. Schema 26 names
`marc-cli-v26` and appends LZ77 plus tANS to the frozen thirty-six-profile
schema-25 order. Schema 27 names `marc-cli-v27` and appends LZSS plus tANS to
the frozen thirty-seven-profile schema-26 order. Schema 28 names
`marc-cli-v28` and appends LZ78 plus tANS to the frozen thirty-eight-profile
schema-27 order. Schemas 1 through 27 retain
their exact versioned profile sets.
Schema 29 names `marc-cli-v29` and appends LZW plus tANS to the frozen
thirty-nine-profile schema-28 order. Schemas 1 through 28 retain their exact
versioned profile sets.

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

## Entropy codec foundations

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

## C transform ABI

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

## Composed profile boundaries

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

### LZ77 plus Blocked Huffman publication evidence

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
all twenty-eight schema-17 archives must compare byte for byte. This establishes
compiler independence on one architecture, while cross-architecture evidence
remains a separate gate.

CI turns this check into an externally consumable protocol. Each reference job
generates the same 8,193-byte binary fixture, validates a local round trip for
all twenty-eight schema-17 profiles, and uploads the fixture, complete archives,
and a JSON manifest containing the source revision. The external verifier first
validates manifest bounds and hashes, then decodes foreign archives and
independently re-encodes the fixture with the local CLI. Artifact hashes detect
transfer mistakes but are not authentication.

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

### LZ77 plus Dynamic Range staged boundary

The Dynamic Range composition preserves the canonical 16-byte LZ77 token
boundary and resets both dictionary history and the adaptive order-0 range
model at every outer frame. Exact encoding owns three caller-supplied byte
regions: raw frame collection, frozen canonical tokens, and the immutable
serialized frame. All three are checked against the aggregate internal-buffer
limit before a completed frame becomes drainable.

The bounded streaming encoder emits the stream prefix, collects exactly one
configured raw frame, plans and encodes through the complete-frame boundary,
and drains the retained result before reusing storage. One-byte I/O and output
starvation therefore cannot alter encoded bytes. `Flush` leaves an incomplete
frame open, while `EndInput` is retained until the last complete frame has been
fully emitted. The matching bounded decoder owns serialized-frame, canonical
token, and private raw regions. It admits a frame to output only after complete
collection, nested validation, and reconstruction, so malformed later frames
are atomic. The later public factory preserves this same commit boundary.

The bounded profile exposes only three byte counts per direction. Encoding
uses raw collection, canonical-token staging, and serialized-frame storage;
decoding uses serialized-frame storage, canonical-token staging, and private
raw storage. Encoder extents use the known original size and configured frame
size, while decoder extents use only trusted local limits and the format cap.
No private C++ object layout or input-controlled allocation crosses this
profile boundary.

The public C requirements query combines each direction's token and trailing
frame/raw extents into one secondary byte region and reports alignment one;
the primary region remains raw collection for encode and serialized-frame
storage for decode. Factory creation repeats the profile calculation, validates
both regions, partitions secondary storage with checked offsets, constructs the
matching streaming transform, and publishes no handle on failure. ABI version
1 gains only named config, query, and factory symbols; no existing layout or
symbol changes.

The public-ABI completion matrix fixes 64-byte frames and exercises required
binary classes through only the C lifecycle. It compares unchunked output with
one-byte and mixed schedules, repeats successful and failing terminal calls,
and proves that a corrupted, truncated, or trailing fourth frame can commit
only the first three complete frames.

The decoder fuzz boundary is intentionally fixed-memory. It caps each supplied
input at 8,192 bytes and exercises both the private complete-frame validator
and the public-style incremental decoder. The streaming path owns fixed arrays
for serialized frames, canonical tokens, reconstructed raw bytes, and output;
derives small input and output chunks only from bounded seed bytes; and aborts
after a fixed call ceiling rather than permitting an input-controlled loop.
Normal regression tests retain every proper prefix of a canonical frame plus
extreme frame extents and a malformed range descriptor. Each case must publish
zero current-frame bytes, preserve the output sentinel, and repeat the same
sticky error.

The command-line adapter selects this composition only through the public C
configuration, requirements, factory, process, and destroy lifecycle. It fixes
65,536-byte raw frames, supplies the documented `16F` token and `2S + 5`
payload limits, and receives both workspace extents from the requirements
query. Output remains hidden in a sibling `.tmp` path until the transform ends,
the file closes successfully, and the final rename commits it.

The dependency-free benchmark uses the same public profile and independently
queries encoder and decoder workspaces. Its checked complete-stream capacity
uses 32 payload bytes per raw byte plus one descriptor, five termination bytes,
and one generic header per frame. It verifies byte-exact decode before timing
either direction and reports the larger caller-reserved workspace total.

### Specified LZ77 plus rANS boundary

The first rANS composition freezes the entire canonical LZ77 token byte stream
before entropy processing. rANS remains a byte transform: its block controller
may divide the stream inside a 16-byte token, while the outer frame controller
prevents a block from crossing the dictionary reset boundary. This keeps
entropy block sizing independent of dictionary record layout.

The decoder-facing boundary is deliberately staged. Generic extents and every
rANS descriptor, normalized model, state transition, terminal state, and
payload byte are admitted before the complete token region is materialized in
private storage. LZ77 alignment, references, overlap semantics, and declared
raw extent are checked only over that complete region. No combined decoder or
public profile is implied until private reconstruction and transactional
publication are implemented and tested.

The first internal validator now realizes this boundary with caller-owned token
and rANS-view spans. It preflights exact descriptor, payload, token, and view
bytes in one aggregate policy. One loop validates every block state path
without output; a second loop fills token staging only after the first loop
succeeds. The existing LZ77 validator then checks the complete reconstructed
token stream. Raw staging and caller-visible output are deliberately absent.

The next internal layer admits a distinct private raw span before any rANS
work and counts it in the same aggregate bound. It reuses the complete
validator, then runs the existing allocation-free LZ77 decoder over immutable
token staging. Literal and overlapping-match reconstruction therefore occurs
only after both entropy and dictionary semantics succeed. Publication remains
a later, separate boundary.

The transactional frame boundary now adds that publication step without
weakening private validation. It preflights the full caller output extent
beside all private capacities, runs the private reconstruction, and performs a
single bounded copy only on success. A failing frame cannot expose a prefix or
modify an existing output sentinel.

The first encoder-side layer mirrors that private boundary without yet
serializing a frame. It freezes the complete canonical LZ77 token sequence in
caller-owned staging, then performs a count-only rANS plan for every byte
block. This establishes the exact descriptor region, payload region, and
complete frame extent before a future encoder receives any serialized
destination. Token staging plus the planned descriptor and payload regions
share one checked aggregate bound.

The complete-frame encoder makes that plan the serialized-output transaction
boundary. It admits the full destination before writing, emits the generic
header, keeps all fixed-size descriptors contiguous ahead of all payloads,
and reproduces each count-only rANS plan during deterministic encoding.
Output-capacity failure therefore publishes no prefix, while any discrepancy
after admission is treated as an internal invariant violation.

The bounded known-size streaming encoder adds an immutable drain state above
that complete-frame transaction. It serializes the fixed stream prefix at
construction, collects no more than one outer raw frame, prepares the entire
combined frame in caller-owned storage, and drains it before that storage may
be reused. `EndInput` is retained across prefix and frame starvation, while
`Flush` leaves partial collection open. The live aggregate consists of raw
collection, canonical token staging, and the completed serialized frame.

The matching streaming decoder first separates prefix admission, generic
frame-header admission, complete body collection, private combined decode,
and raw drain into distinct states. rANS block views are caller-owned and
their exact used bytes join serialized-frame, token, and raw storage in the
aggregate bound. A completed frame becomes visible only through the final raw
drain state, so corruption in a later frame cannot leak that frame's prefix or
roll back already committed earlier frames.

The internal profile calculator now bridges configuration to those streaming
constructors without exposing `RansBlockView`. Encoder requirements are raw
frame collection, conservative `16F` token staging, and the complete
`56 + 528K + S + 8K` frame ceiling. Decoder requirements are serialized-frame,
token, and private-raw byte regions plus a typed view count; a later C ABI may
convert that count to opaque bytes and alignment. All calculations use local
limits and checked conversion to `size_t`.

The public C factory performs that conversion while preserving the existing
three-workspace lifecycle. It revalidates the complete profile, partitions
secondary bytes only at checked token boundaries, rejects short or misaligned
regions before construction, and publishes the opaque transform only after a
`nothrow` streaming allocation succeeds. The C header contains configuration,
byte counts, and alignment only; the rANS descriptor-view type remains an
internal C++ detail.

The public-ABI completion boundary exercises only those C entry points. It
constructs fresh queried workspaces for each direction, compares repeated
encodes byte for byte, varies both input and output chunking across multiple
frames, and confirms that only previously validated frames remain visible
when the final frame is corrupt, truncated, or followed by trailing data.

The fuzz boundary exercises the private complete-frame decoder beside the
incremental decoder with the same fixed limits. Eight rANS views and all byte
regions exist before serialized metadata is inspected; input bytes may vary
only bounded chunk sizes. A finite call ceiling converts any stalled state
machine into a reproducible failure, while permanent regressions preserve
transactional behavior for every canonical truncation and malformed extents.

The command-line adapter selects this contract as `lz77-rans` and remains
strictly above the public C ABI. Its fixed 64-KiB raw frames and entropy blocks
derive a 1,048,576-byte token ceiling, sixteen rANS blocks, a 1,048,704-byte
payload ceiling, and a 2,171,320-byte encoder aggregate bound. Actual byte
regions and decoder-view alignment remain authoritative results of the public
requirements query. The shared temporary-file path preserves overwrite
refusal and whole-operation transactional publication.

The dependency-free benchmark selects the identical public profile and creates
fresh public transforms outside every timed region. It first requires a
byte-exact round trip, then reports compression ratio, directional throughput,
all queried workspace regions, and peak caller-reserved workspace. Its checked
complete-stream capacity is `80 + 16N + 8632K`, where `N` is raw input bytes
and `K` is the number of nonempty 64-KiB outer frames.

Interoperability schema 20 names codec set `marc-cli-v20`, preserves the exact
thirty-entry schema-19 order, and appends this unchanged CLI representation
once. The verifier requires exact order, count, size, SHA-256, foreign decode
equality, and byte-identical local re-encoding while retaining explicit
support for schemas 1 through 19.

The established four-direction exchange verified all thirty-one schema-20
archives across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers at revision `01e87fe19f5c9c90edd87c9caeb8acf36b413aad`.

### Specified LZ77 plus tANS boundary

The first tANS composition freezes the complete canonical LZ77 token byte
stream before entropy processing. tANS remains an untyped byte transform: its
block controller may divide the stream inside a 16-byte token, while the outer
frame controller prevents a block from crossing the LZ77 reset boundary.

The decoder-facing boundary is validation-first. Generic extents, every fixed
tANS descriptor, normalized model, spread and transition table, initial state,
bit extent, terminal state, and padding bit must succeed before the complete
token region is materialized in private storage. LZ77 alignment, references,
overlap semantics, and declared raw extent are checked only over that complete
region. No combined decoder or public profile is implied until bounded private
reconstruction and transactional publication are implemented and tested.

The first internal validator realizes this boundary with caller-owned token
and tANS-view spans. It preflights exact descriptor, payload, token, and view
bytes under one aggregate policy. One loop validates every tANS model, table,
state path, terminal state, and padding bit without output; a second loop fills
token staging only after all blocks succeed. The existing LZ77 validator then
checks the complete reconstructed token stream. Raw staging and caller-visible
output are deliberately absent from that first boundary.

The matching private decoder extends the same preflight with the complete raw
staging extent and counts those `F` bytes in the aggregate workspace before
descriptor parsing or token mutation. Only after every entropy and LZ77 check
succeeds does the allocation-free LZ77 decoder reconstruct literals and
forward overlapping matches into exactly `F` disposable raw bytes. No
caller-visible output span exists at this boundary.

The transactional wrapper admits a distinct caller output span of at least
`F` bytes before descriptor parsing, without counting publication storage as
internal workspace. It preserves the full validation and private reconstruction
sequence, then copies exactly `F` bytes once. Every earlier failure leaves the
caller output unchanged.

The encoder-side planner derives and materializes the complete canonical LZ78
token region once in caller staging. It then plans each tANS block over those
immutable bytes and accumulates exact block, descriptor, payload, and complete-
frame extents. Encoder records, token staging, descriptors, and payload share
the configured aggregate policy. The synthesized generic header is validated,
but no serialized byte is emitted.

The encoder-side planner first derives and materializes the complete canonical
LZ77 token region in caller staging. It then plans each consecutive tANS block
over those immutable bytes and accumulates exact descriptor, payload, block,
and serialized-frame extents with checked arithmetic. It emits no serialized
byte; a later writer must consume the same frozen staging.

The complete-frame writer invokes that planner first and admits the complete
serialized output before writing its first byte. It then emits the explicit
generic frame header, `K` consecutive fixed descriptors, and `K` consecutive
tANS payloads from the same frozen token staging. Replanning each block must
reproduce the exact previously accumulated payload extent.

The known-size streaming encoder drains the 80-byte stream prefix first,
collects exactly one bounded raw frame, prepares it completely through the
writer, and drains the serialized frame before reusing any workspace. Input
and output capacities may be one byte. Full frames may drain before EndInput;
`Flush` does not close a partial frame, and a latched final EndInput survives
output starvation until all bytes are emitted.

The matching streaming decoder collects the 80-byte prefix, each 56-byte frame
header, and exactly its declared body in bounded caller storage. It validates
and reconstructs a complete frame privately before draining raw output. A
malformed later frame therefore cannot alter earlier committed bytes or expose
any prefix from the failing frame.

The internal profile calculator bridges validated configuration to both
streaming constructors. Encoder requirements contain the largest raw frame,
its conservative `16F` token region, and the complete
`56 + 528K + sum(Q(n))` serialized-frame ceiling. Decoder requirements come
only from local hard limits and contain serialized-frame, token, private-raw,
and tANS-view capacities. The calculator exposes only byte counts and a view
count; `TansBlockView` remains private.

The public C adapter preserves that boundary through one size-tagged config,
one directional requirements query, and one factory. Encoding partitions the
secondary byte region after token staging; decoding partitions it before
private raw staging and casts the separately aligned views region only inside
the C++ implementation. Construction revalidates the profile and publishes no
handle on any configuration, capacity, or alignment failure.

The public-ABI completion boundary treats the C lifecycle as the system under
test rather than reconstructing private codec objects. It repeats complete
encoding for byte identity, varies input/output chunk schedules independently,
checks sticky terminal results, and corrupts only the final frame after three
frames have committed. Earlier output remains visible; no byte of the failing
frame may replace its sentinel.

The bounded decoder fuzz boundary exercises both the complete-frame private
entry and the incremental streaming transform for every input. Fixed encoded,
view, token, raw, and caller-output arrays enforce the same local limits as the
profile, while byte-derived chunk sizes vary starvation paths without external
allocation. A finite call ceiling converts any stalled state machine into a
reproducible failure. Canonical truncation, saturated frame lengths, and an
invalid tANS descriptor remain permanent atomic-publication regressions.

The explicit `lz77-tans` CLI selector is a thin public-C adapter over a fixed
64-KiB raw-frame and entropy-block profile. It supplies the checked token,
payload, block-count, and aggregate limits, then allocates only the opaque byte
counts and alignment returned by the direction-specific query. The common
temporary-file transaction publishes no destination for malformed or trailing
input and refuses to overwrite an existing path.

The dependency-free benchmark uses the same fixed public profile. It reserves
complete-stream output with checked `80 + 24N + 8536K` arithmetic, queries
encoder and decoder workspaces independently, and proves a byte-exact round
trip before starting either timer. Transform construction remains outside the
timed interval, and speed and compression ratio are observations rather than
pass thresholds.

Interoperability schema 26 appends the unchanged CLI-selected profile once
after the frozen schema-25 order. Generation verifies all 37 archives before
recording exact sizes and SHA-256 values. Verification requires manifest order,
hashes, foreign decode equality, and byte-identical local re-encoding; the
compatibility regression removes only `lz77-tans` to reconstruct schema 25
before checking every older schema.
The four-direction artifact exchange at revision
`5b2aa31ba3333c311ad4086b3438915a6c3ce36d` verifies all 37 archives from
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers, including
byte-identical re-encoding in both platform directions.

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

### LZSS plus Adaptive Huffman specified boundary

The next Adaptive composition retains LZSS's variable two-byte Literal and
nine-byte Match grammar as the exact entropy input. One frame-local FGK tree
reconstructs the entire token region into private staging before the ordinary
LZSS validator may interpret any tag or reference. Complete validation and raw
reconstruction occur before a current-frame byte becomes drainable.

The exact all-Literal bound is two token bytes per raw byte; the conservative
FGK bound is therefore 66 payload bytes per raw byte. The reference 64-KiB raw
frame keeps token, payload, serialized-frame, and private raw regions within the
common bounded policy. No aligned views region is required by either component.

The first implemented boundary accepts one exact frame, checks the `2F` and
33-byte-per-token extents plus aggregate workspace before entropy decoding,
decodes into caller-owned private token staging, and validates the complete
LZSS grammar against the declared raw size. It intentionally stops before raw
reconstruction.

The following commit boundary requires a second caller-owned staging region,
reconstructs the validated LZSS sequence there, and copies to caller output
only after the full raw frame succeeds. The raw extent participates in the
aggregate workspace check, and capacity failures precede entropy mutation.

The corresponding encoder boundary fixes the variable LZSS extent and writes
the canonical tokens once before planning Adaptive Huffman over that immutable
staging. It checks the `2F`, 33-byte-per-token, aggregate-workspace, header, and
complete serialized-output extents before emitting a frame byte.

The incremental decoder now collects the fixed stream prefix and one exact
frame at a time, invokes private transactional reconstruction, and drains only
committed raw staging. It supports one-byte input and output, latches finish
while draining, and reports later-frame corruption after preserving only output
from earlier complete frames. Profile-specific token and payload bounds are
checked from the header before an input-controlled body is collected. The
incremental encoder likewise completes each exact frame privately before
draining it, keeps `Flush` nonterminal, and latches finish even across prefix or
frame output starvation. Public adapters remain separate steps.

The internal profile constructor now fixes the canonical IDs, parameter
extents, 64-KiB reference cadence, and caller-owned workspace contract. For
largest raw frame `F`, encoder regions are `F` raw, `2F` token, and
`56 + 16 + 66F` serialized bytes, with their complete aggregate checked.
Decoder regions are conservatively capped from validated local limits rather
than input-controlled extents. The public C factory now exposes those exact
requirements through the common caller-owned workspace lifecycle, while full
profile admission remains gated on completion, fuzz, tooling, benchmark, and
interoperability evidence.

The public-ABI completion matrix now exercises the required binary data classes,
frame-boundary lengths, deterministic re-encoding, one-byte and mixed chunking,
repeatable terminal states, and malformed final-frame commit behavior. A
corrupt fourth frame preserves exactly the first three committed frames and no
byte from the fourth. Tooling and broader malformed-input admission remain
separate boundaries.

The bounded fuzz boundary now drives both the exact frame decoder and the
incremental controller using only fixed arrays. It caps input at 8 KiB, output
at 4 KiB, a raw frame at 1 KiB, canonical LZSS staging at 2 KiB, and payload at
8 KiB, with byte-derived chunking and a fixed call ceiling. Permanent tests
retain atomic rejection of every canonical truncation, extreme frame extents,
and a reserved Adaptive descriptor mutation.

The CLI now selects `lzss-adaptive-huffman` solely through its public C
factory and requirements query. Its 64-KiB frame policy configures the `2F`
token and 33-byte-per-token payload bounds without duplicating internal
workspace partitioning. The common temporary-file transaction prevents an
existing destination from being replaced and removes both destination and
staging output after malformed or trailing input.

The dependency-free benchmark selects the identical profile through the public
C lifecycle. It reserves complete-stream capacity from the 64-KiB cadence and
conservative `66F` payload bound, verifies a byte-exact round trip before
timing, then reports encode/decode throughput, ratio, six queried workspace
extents, and peak caller-reserved workspace without pass/fail performance
thresholds.

Interoperability schema 9 preserves the exact nineteen-entry schema-8 order
and appends `lzss-adaptive-huffman`. Bundle verification requires the exact
twenty-entry order, foreign decode equality, and byte-identical local
re-encoding; schemas 1 through 8 retain their frozen meanings.

### LZSS plus Dynamic Range specified boundary

The next Dynamic Range composition preserves LZSS's complete variable-length
token stream as the entropy boundary. The LZSS encoder must finish every
two-byte Literal or nine-byte Match token before one fresh frame-local adaptive
order-0 model consumes those bytes. Entropy decoding must reconstruct the
entire token region into private staging before token tags, reserved fields,
references, or lengths are interpreted.

The exact all-Literal ceiling is two token bytes per raw byte. Combined with
Dynamic Range variant 1's `2S + 5` conservative payload bound and 2^24-symbol
limit, the format raw-frame ceiling is 2^23 bytes; the reference profile
remains 64 KiB. A decoder validates all declared and aggregate extents before
entropy output, parses the complete variable-length token stream, reconstructs
exactly the declared raw extent into separate private storage, and only then
publishes a frame.

The first implemented boundary stops before reconstruction. It validates the
exact frame and one range descriptor, checks `2F`, `2S + 5`, caller staging,
and descriptor-plus-payload-plus-token aggregate bounds before mutation, then
uses the range decoder's no-output preflight before filling private token
staging. The ordinary LZSS validator parses that complete variable-length
region and returns stable token and byte positions. No raw output boundary is
present in that validator.

The private decoder adds a separately bounded raw region to the aggregate
before entropy output. After the complete token stream passes validation, it
invokes the ordinary LZSS reconstruction path, including forward overlap copy,
against only that private raw span. Short raw storage, aggregate-limit failure,
and malformed descriptor or token layers therefore cannot mutate raw staging.
There is still no caller-visible publication boundary in this step.

The transactional complete-frame decoder adds that boundary without changing
the representation. It rejects short caller output before entropy work,
reconstructs only into private raw staging, and performs one final exact-extent
copy after all nested checks succeed. Descriptor, entropy, token, capacity, or
reconstruction failure therefore leaves caller output byte-for-byte unchanged.

Exact encoding uses separate caller-owned raw input, canonical-token staging,
and serialized-frame output. The planner completes LZSS parsing first, plans
the adaptive range payload over those immutable token bytes, validates header
and aggregate bounds, and returns an exact extent. The encoder refuses a short
destination before writing and rechecks that the frozen token bytes yield the
same descriptor and payload extent before serializing.

The first outer streaming encoder owns no allocation. Callers provide one raw
frame region, a `2F` canonical-token region, and one complete serialized-frame
region. The transform drains the 80-byte prefix, collects exactly one format-
declared raw frame, freezes and encodes it through the exact-frame boundary,
then drains that immutable frame before accepting the next. This preserves
byte identity across one-byte input and output, retains finalization across
output starvation, and leaves nonterminal `Flush` unable to alter framing. A
matching streaming decoder incrementally collects the prefix, one fixed frame
header, and only the admitted exact frame body. It validates the Dynamic Range
and LZSS extent ceilings before body collection, reconstructs into caller-owned
private raw staging through the transactional frame decoder, and drains that
frame only after complete success. A malformed later frame therefore leaves
earlier committed output intact and publishes none of the failing frame.

The profile layer now turns these ownership rules into byte-only requirements.
For the encoder it derives raw, token, and complete serialized-frame regions
from the actual largest configured frame, so a short stream and empty input do
not reserve a full default frame. The decoder query depends only on validated
local limits and the 2^23 raw / 2^24 token format ceilings. No aligned or typed
view region is needed; later public construction can partition ordinary byte
storage without exposing a private C++ record layout. The public C boundary
now performs that partition: primary owns raw input or serialized input;
secondary owns token staging followed by serialized-frame or private-raw
staging. The requirements query and factory revalidate the same configuration,
construct the immutable streaming direction with `nothrow`, use no views
workspace, and publish no handle on failure.

The public completion audit exercises this construction boundary rather than
calling internal frame helpers. It spans required binary classes and raw sizes
immediately around the 64-byte audit frame, compares complete multi-frame
archives across arbitrary chunk schedules, and repeats both ended and error
states. A corrupted final one-byte frame demonstrates that C ABI publication
inherits the same complete-frame commit boundary as the internal decoder.

The bounded fuzz boundary drives both the exact-frame private decoder and the
incremental controller with fixed storage. It caps supplied input at 8 KiB,
total output at 4 KiB, a raw frame at 1 KiB, canonical LZSS staging at 2 KiB,
and range payload at 8 KiB. Encoded-frame, token, private-raw, and final-output
arrays are counted in one aggregate policy before parsing. Input-derived
partial-I/O schedules and a fixed call ceiling make a stalled state machine
reproducible. Permanent tests retain frame-atomic rejection of every canonical
truncation, saturated extent fields, and a malformed range descriptor.

The explicit CLI selector is a thin public-C-ABI adapter. It fixes 64-KiB raw
frames, the 128-KiB LZSS token ceiling, the 262,149-byte range-payload ceiling,
and the 458,829-byte aggregate policy, then obtains the actual
direction-specific regions from the public requirements query. Output remains
hidden in a sibling `.tmp` path until the transform ends, the file closes, and
an atomic rename publishes it. Any malformed or trailing input removes the
temporary output, including after earlier frames were decoded internally.

The dependency-free benchmark selects the identical public profile. Its
checked complete-stream destination bound is `80 + 4N + 77K`, where four
payload bytes per raw byte cover the LZSS and range worst cases and each frame
adds a header, descriptor, and termination extent. An untimed byte-exact round
trip gates measurement; encode and decode throughput, ratio, all six queried
workspace extents, and peak caller reservation are descriptive outputs rather
than performance thresholds.

### Specified LZSS plus rANS boundary

The second rANS composition freezes the complete canonical LZSS byte sequence
before entropy processing. Scalar rANS remains unaware of the variable
two-byte Literal and nine-byte Match grammar, so a block may split either
token while the outer frame remains the shared reset boundary.

For raw frame size `F`, token extent `S`, block size `B`, and block count `K`,
the checked bounds are `S <= 2F`, `K = ceil(S/B)`,
`8K <= P <= S + 8K`, and exactly `528K` descriptor bytes. Decoding must admit
and validate the complete entropy representation before reconstructing the
private token region, then validate the whole LZSS grammar and exact raw
extent before any raw publication.

The first internal complete-frame validator now implements that boundary.
It admits descriptor views and token staging plus their aggregate workspace
before entropy work, validates every rANS block without output, reconstructs
the exact private token extent in a second pass, and invokes the existing LZSS
validator. It intentionally stops before raw reconstruction.

The matching private decoder extends admission with the complete raw staging
extent and includes those bytes in the same aggregate workspace calculation.
Only after entropy and LZSS validation succeed does the existing allocation-
free LZSS decoder reconstruct Literal and forward-overlap Match tokens into
that disposable caller-owned span. No caller-visible publication boundary is
present yet.

The transactional decoder adds a distinct caller-visible output span. It
preflights capacity for the complete raw frame before descriptor parsing,
retains the private reconstruction boundary, and copies exactly the validated
`F` bytes once after success. Output capacity is not internal workspace, and
every failure leaves the caller-visible span unchanged.

The encoder-side planner first computes and materializes the complete
canonical LZSS token sequence in caller-owned staging. It then walks immutable
consecutive rANS blocks, retaining only one temporary descriptor at a time,
and sums exact payload and descriptor extents with checked arithmetic. The
planner validates the synthesized generic header and returns the complete
frame size without accepting or modifying serialized output.

The complete-frame writer invokes that planner first and rejects insufficient
serialized output before writing the generic header. It then re-plans each
immutable token block, requires every exact payload extent to match the frozen
aggregate, explicitly serializes its descriptor, and encodes only the assigned
payload subspan. The final token and payload offsets must equal the plan.

The known-size streaming encoder owns no allocation. Caller-owned storage
holds at most one raw frame, its `2F` worst-case LZSS token region, and one
complete encoded frame. The state machine drains the 80-byte stream prefix,
collects exactly the next declared frame extent, prepares one immutable frame
through the deterministic writer, and drains it before accepting the next
frame. `Flush` does not close a partial frame; `EndInput` must coincide with
the declared original size.

The bounded streaming decoder incrementally collects the fixed prefix, one
generic frame header, and then exactly the declared frame body. Header
admission checks encoded storage, rANS views, `2F` token staging, raw staging,
and their aggregate before the body is accepted. A complete frame passes the
private transactional decoder before its raw bytes enter a distinct drain
state. Earlier frames may be committed; a malformed later frame publishes
nothing from that frame and makes the error sticky.

The internal profile is the sole sizing authority for this streaming pair.
For encoding it derives the largest raw frame, conservative `2F` token
staging, exact worst-case descriptor and payload storage, and the complete
encoded-frame extent. For decoding it derives encoded-frame, bounded token,
private raw, and rANS-view capacities only from validated local limits. The
profile retains the composition's specified 1-MiB raw-frame cap; its `2F`
token ceiling therefore remains well within the scalar rANS block bound.

The public C ABI binds that profile without exposing any C++ type. Encoding
uses raw collection as primary and token-plus-frame storage as secondary;
decoding uses encoded-frame storage as primary, token-plus-raw storage as
secondary, and an opaque aligned rANS-view region. The requirements query is
the only public authority for all sizes and view alignment.

The public-ABI completion matrix fixes 64-byte raw frames and entropy blocks.
It proves every required data class, deterministic output across chunking,
sticky terminal calls, and frame-atomic rejection of corruption, truncation,
or trailing data in a final short frame through only the C lifecycle.

The fuzz boundary independently submits bounded input to the private
complete-frame decoder and to a C-ABI-created streaming decoder. All byte
regions and rANS views have compile-time ceilings; the public requirements
query may select only subspans of those arrays. Input-derived chunks are
bounded, and a finite call ceiling turns non-progress into a reproducible
failure.

The command-line adapter selects this contract as `lzss-rans` using only the
public C configuration, workspace query, factory, process, and destroy
functions. It fixes raw and entropy blocks at 65,536 bytes, supplies a
conservative 512-KiB aggregate policy that covers both directions, and leaves
all opaque view sizing and alignment to the public query. File publication
retains the shared temporary-file transaction.

The dependency-free benchmark selects the same public profile and verifies a
complete byte-exact round trip before timing. It creates each public transform
outside the elapsed region and reports ratio, directional throughput, all
queried workspace regions, and peak caller-reserved workspace. Checked
complete-stream capacity is `80 + 2N + 1128K` for raw extent `N` and nonempty
64-KiB frame count `K`.

Interoperability schema 21 names codec set `marc-cli-v21`, preserves the exact
thirty-one-entry schema-20 order, and appends this unchanged CLI
representation once. Local generation and verification require exact order,
count, size, SHA-256, fixture decode equality, and byte-identical local
re-encoding while retaining explicit support for schemas 1 through 20.
The established four-direction exchange subsequently verified all thirty-two
schema-21 archives at revision
`110bf3c9f80f5bc3723232c6f027867e4c2e7a2f` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

### Specified LZSS plus tANS boundary

LZSS completes the canonical two-byte Literal and nine-byte Match sequence for
one frame before tANS sees any symbol. tANS remains an untyped byte transform,
so its block controller may divide either token form internally; the complete
private byte sequence is restored before the LZSS parser decides token
boundaries. The outer controller resets both layers together and forbids an
entropy block from crossing that frame boundary.

The reserved decoder order is generic extent validation, all tANS model and
state validation, complete private token reconstruction, then variable-length
LZSS grammar and semantic validation. Raw reconstruction and publication are
later admission boundaries. Checked storage follows `S <= 2F`, exact `528K`
descriptors, and the blockwise `2 + ceil(12n/8)` payload ceiling. The
independent raw-`A` vector fixes one complete 587-byte frame without creating a
combined codec entry point.

The first internal validator realizes that order with caller-owned token and
tANS-view spans. It admits their complete extents and aggregate workspace up
front, validates every entropy automaton without output, and only then decodes
all blocks into the private token region. The existing LZSS validator runs
after exact token reconstruction and preserves token-index and byte-offset
diagnostics. Raw staging and caller-visible output remain absent.

The private raw decoder adds the exact declared `F`-byte staging extent to the
same up-front capacity and aggregate checks. Only after the two-pass entropy
decode and complete LZSS validation succeed does the allocation-free LZSS
decoder reconstruct literals and overlapping matches. This storage is still
disposable implementation workspace; no caller-visible output is published.

The transactional wrapper admits a distinct caller output extent before any
private mutation, without charging publication storage to the internal
workspace limit. It preserves the same validation and private reconstruction,
then copies exactly the declared raw frame once. Every earlier error leaves
caller output byte-for-byte unchanged.

The encoder-side planner first derives and materializes the complete canonical
LZSS token region in caller staging. It then plans each consecutive tANS block
over those immutable bytes and accumulates exact descriptor, payload, block,
and serialized-frame extents with checked arithmetic. The synthesized generic
header is validated, but no serialized byte is emitted; a later writer must
consume the same frozen token region.

The complete-frame writer invokes that plan before accepting any output
mutation. Once the exact complete capacity is available, it emits the generic
header, consecutive fixed descriptors, and consecutive payloads explicitly.
Every block is replanned over unchanged token bytes and must reproduce the
same payload extent, preserving one deterministic representation.

The known-size streaming encoder drains the 80-byte prefix first, collects at
most one raw frame, prepares the entire immutable frame through the writer,
and drains it before accepting reuse of its storage. One-byte input and output
are valid. Full frames may drain before finish, `Flush` leaves a partial frame
open, and a latched `EndInput` survives output starvation until all bytes are
emitted.

The matching streaming decoder first collects the fixed prefix, then each
generic frame header. Header validation fixes and admits the complete encoded
extent plus tANS views, canonical token staging, and private raw staging before
the body is collected. A complete frame is decoded privately and only then
drained to the caller. Thus an invalid later frame preserves all earlier
commits while publishing no byte from the invalid frame.

The direction-specific profile calculator supplies those caller-owned regions
without allocating them. Encoder sizing uses the actual largest known frame,
the `2F` LZSS ceiling, exact descriptor count, and the blockwise 12-bit tANS
payload ceiling. Decoder sizing depends only on local hard limits and returns a
view count rather than exposing `TansBlockView`. Checked arithmetic and the
same aggregate policy guard every derived extent.

The public C adapter preserves that boundary through one size-tagged config,
one directional requirements query, and one factory. Encoding partitions the
secondary byte region after token staging; decoding partitions it before
private raw staging and casts the separately aligned views region only inside
the C++ implementation. Construction revalidates the profile and publishes no
handle on any configuration, capacity, or alignment failure.

The public-ABI completion boundary treats that C lifecycle as the system under
test. It covers required binary classes, repeated deterministic encoding,
one-byte and mixed chunk schedules, sticky terminal results, and four-frame
decode failures. Corruption, truncation, or trailing data in the final frame
preserves the first three committed frames and exposes no byte from the
failing frame.

The bounded fuzz boundary drives both the complete-frame private decoder and
the incremental decoder from one input. Fixed caller-owned arrays cap input
and payload at 8 KiB, total raw output at 4 KiB, a raw frame at 1 KiB, LZSS
token staging at 2 KiB, and tANS metadata at eight views. Byte-derived chunks
and a finite call ceiling prevent input-controlled allocation or unbounded
state-machine execution. Permanent regressions require every canonical
truncation, impossible frame lengths, and invalid tANS descriptors to reject
without publishing any byte from the failing frame.

The command-line adapter selects this profile as `lzss-tans` through only the
public C lifecycle. It fixes raw frames and tANS blocks at 65,536 bytes, uses
the exact 131,072-byte token, two-block, 1,056-byte descriptor, and
196,612-byte payload ceilings, and applies a conservative 512-KiB aggregate
policy. Directional workspace extents and view alignment still come from the
requirements query. The shared temporary-file transaction prevents malformed
or trailing input from publishing an output file.

The benchmark adapter uses the same fixed profile and public C lifecycle.
Encoded-capacity planning includes the 80-byte prefix, three transition bytes
per raw byte, and a header plus two tANS descriptor/state pairs for every
nonempty frame. An untimed exact round trip gates measurement; transform
construction stays outside timed intervals, while direction-specific
workspace regions and their peak total are reported from the requirements
queries.

Interoperability schema 27 appends the unchanged `lzss-tans` CLI archive once
after the frozen schema-26 order. Generation verifies all 38 archives before
recording their size and SHA-256; verification enforces exact order, foreign
decode equality, and byte-identical re-encoding. The compatibility regression
rejects reordered schema-27 manifests and removes only `lzss-tans` to recover
schema 26 before checking every earlier schema.

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

### Specified LZ78 plus Adaptive Huffman boundary

The next composition preserves LZ78's fixed eight-byte canonical tokens and
aligned phrase table while replacing bounded static entropy blocks with one
fresh FGK tree per outer frame. A raw frame of `F` bytes has at most `8F` token
bytes and the conservative Adaptive payload ceiling is therefore `264F`.
Decoding must entropy-decode into token staging, validate the complete phrase
graph, reconstruct into private raw staging, and only then publish. Encoding
must freeze the LZ78 parse before Adaptive planning. The representation and
independent vector are specified. The first internal validator now accepts one
exact frame, checks all extents and caller capacities, strict-decodes Adaptive
bytes into token staging, and validates the full LZ78 phrase graph in aligned
workspace. It exposes neither raw bytes nor a callable public profile.
The matching private decoder revalidates that token graph, expands each prefix
chain iteratively into raw staging, and commits the exact frame only after
reconstruction. Raw staging is counted in the aggregate bound; output and raw
capacity failures occur before entropy staging is touched.
The matching exact-frame planner sizes and populates the aligned LZ78 encoder
table, freezes canonical tokens in private staging, and plans Adaptive Huffman
over that fixed byte sequence. The encoder validates the complete serialized
destination before writing its header, descriptor, or payload. Encoder-table,
token, descriptor, and payload extents participate in the checked aggregate
workspace bound; streaming and public construction remain separate steps.
The first bounded streaming encoder owns no allocation: callers provide one
raw-frame span, token staging, serialized-frame staging, and an aligned encoder
table. It emits the common 80-byte prefix, collects only complete configured
frames, delegates each frame to the exact planner and encoder, then drains it
before accepting another frame. Output starvation retains all pending state;
`EndInput` remains latched even when prefix drainage consumes no input.
The matching streaming decoder parses the prefix and each frame header in
bounded fixed storage, rejects impossible LZ78 and Adaptive extents before body
collection, and admits the exact frame, token, raw, and phrase-table aggregate
before decoding. A complete frame is reconstructed into private raw storage;
only that validated storage enters the drain state. Consequently a malformed
later frame cannot expose one of its bytes after an earlier frame has drained.
The internal profile fixes the 65,536-byte reference cadence and derives raw,
token, conservative encoded-frame, and typed-record extents with checked
arithmetic. Encoder and decoder opaque regions each contain one record type;
partitioning rederives their exact size and alignment before producing a span,
so public adapters need not reproduce C++ layout arithmetic later.
The public C adapter now binds those pieces without allocation. It exposes the
usual primary, secondary, and opaque aligned views regions, rederives the
direction-specific profile at construction, and delegates typed layout
creation to the profile partition helpers before publishing a transform.
The public completion matrix then treats that factory as the only construction
boundary and verifies required binary data classes, deterministic bytes,
arbitrary chunking, stable terminal states, and transactional rejection of a
malformed final frame.
The bounded dual-decoder fuzz boundary adds no allocation surface: exact-frame
and incremental parsing share fixed local limits, byte arrays, a 1,024-record
phrase table, and a call ceiling. Ordinary builds compile this boundary while
permanent malformed regressions exercise its reviewed failure classes.
The transactional CLI binds this profile only through its public C factory
and requirements query. Its 64-KiB raw cadence configures the `8F` canonical
token and `264F` Adaptive payload ceilings while leaving the opaque phrase
record sizing, alignment, and partitioning inside the checked profile helpers.
The dependency-free benchmark uses the identical public configuration and
queries both directional workspace layouts. It verifies a complete byte-exact
round trip before timing fresh transform instances, then reports ratio,
throughput, all queried extents, and peak caller-reserved workspace without a
performance threshold.
Interoperability schema 10 preserves the exact twenty-entry schema-9 order and
appends `lz78-adaptive-huffman`. Generation round-trips all twenty-one profiles;
verification requires the exact manifest order, foreign decode equality, and
byte-identical local re-encoding while retaining schema 1 through 9 support.
The pushed Windows/MSVC and Ubuntu 24.04 artifacts and an Ubuntu 26.04/Clang
bundle subsequently passed that contract in both operating-system directions
for all twenty-one archives at one full revision.
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

### LZ78 plus Dynamic Range specified boundary

The third Dynamic Range composition preserves LZ78's complete fixed-width
eight-byte token stream as the entropy boundary. The deterministic LZ78 parse
and token serialization finish before one fresh frame-local adaptive order-0
model consumes those bytes. Entropy decoding must reconstruct the entire
private token region before interpreting tags, symbols, reserved bytes, or
phrase indices.

For raw extent `F`, token extent `S` is a nonzero multiple of eight with
`S <= 8F`, and the range payload is bounded by `2S + 5`. The 2^24-symbol range
limit therefore gives this profile a 2^21-byte format frame ceiling; the
reference profile remains 64 KiB. A decoder validates all declared and
aggregate extents before entropy output, builds and checks the complete phrase
graph in bounded aligned workspace, reconstructs exactly the declared raw
extent without recursion, and only then publishes a frame.

The first validator boundary stops at the validated phrase graph. It checks
the exact frame, descriptor, `8F`, `2S + 5`, token staging, aligned phrase
entries, and their aggregate extent before mutation. The strict range decoder
preflights the complete payload before filling private token staging, after
which the ordinary LZ78 validator records the complete bounded phrase graph
and stable token and byte positions.

The next bounded boundary also requires a separate private raw region and
counts its complete extent in the same pre-entropy aggregate check. Only after
the phrase graph is valid does the existing non-recursive LZ78 decoder
iteratively reconstruct exactly the declared raw extent into that region.

The transactional complete-frame decoder adds the caller-visible boundary
without changing the representation. It rejects short caller output before
entropy work, reconstructs only into private raw staging, and performs one
final exact-extent copy after all nested checks succeed. Header, descriptor,
entropy, token, phrase, capacity, or reconstruction failure therefore leaves
caller output byte-for-byte unchanged.

The first encoder boundary is a no-serialized-output planner. It requires the
complete aligned LZ78 encoder-entry region and token staging, performs the
deterministic phrase parse into canonical eight-byte tokens, and only then
plans Dynamic Range payload extent from those immutable bytes. Encoder
entries, tokens, descriptor, and payload participate in one checked aggregate.
The resulting frame extent is exact. The matching encoder rejects short output
after that complete plan, replans the unchanged token staging, requires an
identical payload extent, and serializes header, descriptor, and payload in
order. Repeated calls with identical input and configuration produce identical
frame bytes.

The bounded known-size streaming encoder emits the serialized stream prefix,
collects exactly one configured raw frame, invokes the exact-frame boundary,
and retains that complete serialized frame while draining arbitrary output
chunks. `Flush` does not close a partial frame. `EndInput` is accepted only
with the complete remaining declared input and remains latched until every
prefix and frame byte drains.

The matching bounded streaming decoder incrementally collects the fixed
80-byte prefix and one 56-byte frame header. Before accepting the frame body,
it rejects impossible `S`, `P`, descriptor, caller-workspace, serialized-frame,
and aggregate extents. It then collects exactly the admitted body, validates
and reconstructs the complete frame into private raw storage, and only then
drains raw bytes. A malformed or truncated later frame therefore publishes
none of that frame while leaving earlier completed frames committed.

The bounded profile owns no storage. For encoding it derives raw-frame, token,
serialized-frame, and opaque aligned encoder-entry requirements from the
largest actual known frame. For decoding it derives serialized-frame, token,
private-raw, and opaque aligned phrase-entry requirements only from local
limits and the format cap. Partition helpers revalidate record count, byte
extent, alignment, and capacity before constructing internal typed spans.

The public C factory binds that profile to ABI version 1's size-tagged config,
requirements query, opaque transform handle, and three caller-owned regions.
Factory construction repeats profile admission and typed-view partitioning
before publishing the handle. The C boundary therefore exposes only byte
extents and alignment while retaining the exact streaming encoder and
frame-atomic decoder behavior.

The public-ABI completion matrix constructs both directions only through that
factory. It covers required binary input classes, byte-identical repeated
encoding, one-byte and mixed chunk schedules, stable repeated success, and a
four-frame transaction test. Corruption, truncation, or trailing data in the
fourth frame commits exactly the first three 64-byte frames and no byte from
the fourth.

The bounded decoder fuzz boundary uses the same exact-frame private decoder
and incremental stream decoder without changing either public surface. It
preallocates serialized input, encoded-frame, token, private-raw, final-output,
and aligned phrase-record storage under fixed local limits. Arbitrary bytes
may choose only modulo-bounded input and output chunks. A finite call ceiling
turns a stalled state machine into a reproducible invariant failure rather
than input-controlled work.

The explicit `lz78-dynamic-range` CLI adapter selects a fixed 65,536-byte raw
frame, 524,288-byte canonical token ceiling, 1,048,581-byte range payload
ceiling, and 4-MiB aggregate policy. It obtains all three workspace extents and
opaque alignment through the public requirements query and creates only the
public C transform. The common temporary-file transaction prevents malformed
or trailing input from publishing a destination.

The dependency-free benchmark selects the identical public profile. Its
checked destination formula is `80 + 16N + 77K`; an untimed byte-exact round
trip gates all measurement. Fresh C transforms then report encoded ratio,
directional throughput, all six queried workspace extents, and the larger
three-region sum without a performance threshold.

### Validated LZ78 plus rANS boundary

The third rANS composition freezes the complete canonical LZ78 token sequence
before entropy processing. Scalar rANS remains unaware of the fixed eight-byte
Pair and FinalIndex grammar, so an entropy block may split a token while the
outer frame remains the shared dictionary and model reset boundary.

For raw frame extent `F`, token extent `S`, entropy block size `B`, and block
count `K`, require aligned `0 < S <= 8F`, `K = ceil(S/B)`,
`8K <= P <= S + 8K`, and exactly `528K` descriptor bytes. The decoder must
validate every rANS block into private token staging before checking LZ78
alignment, fields, phrase references, phrase lengths, dictionary growth, and
exact raw extent. Phrase expansion and caller-visible publication remain
outside this first implementation step.

The first internal complete-frame validator implements that boundary. It
checks exact serialized extent, block count, descriptor and payload bounds,
token and phrase workspace capacities, and the aggregate internal-buffer
limit before token mutation. It validates every descriptor and rANS state
path before decoding any block, reconstructs exactly `S` private token bytes,
and runs the bounded LZ78 phrase-graph validator. It deliberately exposes no
caller-visible output, public factory, or streaming transform.

The next internal boundary adds iterative raw reconstruction only after that
complete validation succeeds. Raw staging must hold the exact declared frame
extent and is counted in the aggregate workspace before any entropy output.
The existing non-recursive LZ78 decoder walks the validated parent links into
the separate raw region; malformed entropy or token data therefore cannot
touch raw staging. The result remains private and is not transactionally
published by that boundary.

The transactional complete-frame decoder adds one caller-visible output span.
Its exact raw capacity is checked with the other capacities before any private
mutation, but it is not counted as internal workspace. After complete entropy,
phrase-graph, and raw reconstruction success, one copy publishes exactly the
declared frame extent; failure at any earlier layer leaves the complete output
span unchanged.

Exact encoding first completes deterministic LZ78 parsing into caller-owned
encoder records and freezes the complete canonical token region. Only then
does the planner visit each scalar rANS block in forward serialized order,
accumulate exact descriptor and payload extents, count encoder records,
tokens, descriptors, and payloads in one checked workspace total, and
validate the synthesized generic frame header. The encoder repeats those
deterministic block plans only after serialized output capacity is known,
then writes the header, all descriptors, and all payloads. The independent
raw-`A` input reproduces the frozen 592-byte frame exactly.

The known-size streaming encoder emits the ordinary 80-byte stream prefix,
collects at most one configured raw frame, invokes the exact planner and
encoder into caller-owned immutable frame storage, and drains that frame
before accepting bytes for the next one. Raw collection, encoder records,
canonical tokens, and the complete serialized frame are checked as one
aggregate before encoding. Arbitrary input and output chunking therefore
cannot alter frame bytes. `Flush` keeps a partial frame open, while
`ResetBlock` is unsupported because outer frame boundaries are fixed by the
configured raw frame size.

The matching streaming decoder incrementally admits the fixed 80-byte prefix,
then one 56-byte generic frame header before deriving the exact complete frame
extent. It requires encoded-frame, rANS-view, token, phrase, and private raw
storage and their checked aggregate before collecting the remaining frame.
Only a complete frame is passed to the transactional private decoder; its raw
staging is drained afterward under arbitrary output starvation. Thus a
malformed later frame cannot publish any byte from that frame, while already
drained earlier frames remain committed. Known original size determines the
final frame, and truncation or trailing bytes are rejected.

The internal profile calculator gives those caller-owned regions a stable
typed layout without exposing record definitions at a future ABI boundary.
Encoding uses raw-frame bytes, conservative `8F` token bytes, a complete
`56 + 528K + S + 8K` encoded-frame region, and aligned LZ78 encoder records.
Decoding derives encoded-frame, token, and private-raw byte regions solely
from local limits; one aligned opaque region contains rANS block views first
and LZ78 phrase records at a checked aligned offset. Partitioning rederives
every count, offset, extent, and alignment before returning typed spans.

The public C adapter preserves that ownership model. Its requirements query
reports raw or encoded-frame storage as primary, token-plus-frame/raw storage
as secondary, and only an opaque byte extent plus maximum alignment for typed
records. Factory construction repeats profile calculation and layout
partitioning before publishing an immutable-direction handle. A failed query,
short or misaligned region, or allocation failure leaves the handle null.

The bounded decoder fuzz boundary invokes both the private exact-frame decoder
and that public C streaming lifecycle. It caps supplied input at 8 KiB, total
raw output at 4 KiB, one raw frame at 1 KiB, canonical LZ78 tokens at 8 KiB,
rANS payload at 16 KiB, metadata at eight block views, and phrase state at
1,024 records. Every byte and typed region has a compile-time ceiling before
serialized metadata is parsed. Input-derived chunks remain within a fixed
call budget, so malformed data cannot create allocation or unbounded-progress
behavior.

The command-line adapter selects this contract explicitly as `lz78-rans`.
It fixes 65,536-byte raw frames and entropy blocks, the 524,288-byte token
ceiling, eight rANS blocks, the 524,352-byte payload ceiling, 65,536 phrase
entries, and a 4-MiB aggregate policy. It obtains all direction-specific byte
regions and opaque alignment from the public query, and retains the common
temporary-output transaction so malformed or trailing input cannot leave a
destination file.

The benchmark adapter uses the identical public profile. Its checked encoded
capacity is `80 + 8N + 4344K`, where `N` is raw input and `K` is the nonempty
frame count. It verifies exact decode equality before timing, constructs a
fresh public transform for every sample, and reports the queried primary,
secondary, views, and peak directional workspace without imposing a speed
floor.

Interoperability schema 22 names codec set `marc-cli-v22`, preserves the exact
thirty-two-entry schema-21 order, and appends this unchanged CLI
representation once. Local generation and verification require exact order,
count, size, SHA-256, fixture decode equality, and byte-identical local
re-encoding while retaining explicit support for schemas 1 through 21.
The established four-direction exchange subsequently verified all thirty-three
schema-22 archives at revision
`2aa51ded63bdeacb0e5b2ec28a21075a867bb353` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

The independent raw-`A` vector composes only the existing LZ78 encoder, scalar
rANS encoder, and generic serializers. It freezes the eight-byte Pair token,
the `00:3584` and `41:512` normalized model, the eight-byte final-state
payload, and the complete 592-byte frame.

### Specified LZ78 plus tANS boundary

The third tANS composition freezes the complete canonical LZ78 token byte
stream before entropy processing. Every token is eight bytes, while tANS
remains an untyped byte transform; a block may therefore split a token but may
not cross the outer frame where the phrase dictionary and entropy tables both
reset.

For raw size `F`, token size `S`, entropy block size `B`, and
`K = ceil(S/B)`, require nonzero eight-byte-aligned `S <= 8F`, exact `528K`
descriptor bytes, and the checked blockwise tANS payload ceiling. The decoder
must validate all entropy descriptors and state paths before reconstructing
exactly `S` private bytes, then validate the complete LZ78 phrase graph and
exact `F` expansion without caller-visible publication.

The independent raw-`A` boundary fixes one Pair token, normalized frequencies
`00:3584` and `41:512`, initial-state offset `0x046B`, four zero transition
bits, payload `6B 04 00`, and a complete 587-byte frame. This is a format and
vector reservation.

The first bounded validator realizes the validation-first boundary with
caller-owned tANS views, token staging, and LZ78 phrase records. It admits all
serialized and workspace extents under one aggregate policy, validates every
tANS descriptor and state path without token output, and only then performs a
second entropy pass into private staging. Complete LZ78 token and phrase-graph
validation follows; raw expansion and caller-visible publication remain
absent from this first boundary.

The matching private decoder adds the complete raw extent to preflight and the
aggregate workspace calculation. After the same two-pass entropy validation
and complete phrase-graph validation, the existing allocation-free LZ78
decoder expands phrases iteratively into exactly the declared raw extent.
There is still no caller-visible output span, so every workspace remains
disposable on failure.

The transactional wrapper admits a separate caller output extent before any
private mutation, without counting publication storage as internal workspace.
It preserves the entire validation and reconstruction sequence and copies the
complete raw frame exactly once only after success. Every earlier error leaves
caller output unchanged.

The encoder-side exact-frame planner materializes the complete canonical LZ78
token sequence once, then plans every consecutive tANS block over that frozen
staging. Encoder records, token bytes, descriptors, and payloads are admitted
as one bounded aggregate before the synthesized header and exact serialized
extent are accepted.

The matching complete-frame writer invokes that plan first and admits the
entire serialized destination before writing. It explicitly emits the generic
header, all contiguous descriptors, and all contiguous payloads. Every block
is replanned only over the unchanged token staging and must reproduce its
planned payload size; final token and payload offsets must also match. Planner
and capacity failures therefore publish no serialized byte.

The command-line adapter selects this contract explicitly as `lz78-tans`.
It supplies only the fixed public profile limits, obtains all three directional
workspace extents and opaque alignment from the public C ABI requirements
query, and publishes files through the existing temporary-path transaction.
The dependency-free benchmark selects the same public C profile and requires
an untimed exact round trip before measuring encode/decode throughput,
compression ratio, and caller-owned workspace.
The bounded decoder fuzz boundary drives both complete-frame private
reconstruction and the public C streaming transform. All encoded, token, raw,
tANS-view, phrase, and output regions have compile-time ceilings; byte-derived
chunking has a finite process-call guard.
The public completion matrix drives only the size-tagged C lifecycle. It fixes
required binary data classes, deterministic multi-frame output under arbitrary
chunking, stable repeated end/error results, and final-frame atomicity while
preserving previously committed frames.

Interoperability schema 28 appends the unchanged `lz78-tans` CLI archive once
after the frozen schema-27 order. Generation verifies all 39 archives before
recording their extents and SHA-256 values. Verification enforces exact order,
foreign decode equality, and byte-identical local re-encoding. The compatibility
regression rejects reordered schema-28 manifests and removes only `lz78-tans`
to recover schema 27 before checking every earlier schema.
The four-direction artifact exchange at revision
`3d5001ce7536c425328a597240244551605e8935` verifies all 39 archives from
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers, including
byte-identical re-encoding in both platform directions.

The bounded known-size streaming encoder adds only collection and immutable
drain state above that writer. It emits the ordinary 80-byte stream prefix,
collects one configured raw frame in caller-owned storage, prepares the entire
canonical token and encoded frame, and drains it before any workspace reuse.
Input and output capacities may be one byte. `Flush` leaves a partial frame
open, while a final `EndInput` remains latched across prefix and frame output
starvation. Empty known-size input emits only the prefix; reset, unknown flags,
premature end, excess input, and insufficient storage become sticky errors.

The matching known-size streaming decoder collects the 80-byte prefix and each
56-byte frame header before admitting the exact body extent. At header time it
checks `S <= 8F`, token alignment, `K`, `528K`, the blockwise tANS payload
ceiling, and all encoded, view, token, phrase, and private-raw regions under one
aggregate limit. A complete frame is validated and reconstructed only into
private staging before output drain begins. A malformed later frame therefore
cannot alter earlier committed bytes or publish any raw prefix from the failing
frame.

The internal profile calculator bridges validated configuration to both
streaming constructors. Encoder requirements contain the largest raw frame,
its conservative `8F` token region, LZ78 encoder-record count, and complete
`56 + 528K + sum(Q(n))` serialized-frame ceiling, where
`Q(n) = 2 + ceil(12n/8)`. Decoder requirements come only from local hard limits
and contain encoded-frame, token, private-raw, tANS-view, and LZ78-phrase
capacities. The mixed view region is explicitly aligned and partitioned into
typed spans only after size, offset, and alignment validation.

The public C adapter preserves this boundary through
`marc_lz78_tans_config`, a direction-specific requirements query, and one
factory. Encoding partitions secondary storage after token staging and casts
the aligned encoder-record region only after validation. Decoding partitions
secondary storage before private raw staging and divides the aligned opaque
region into tANS block views and LZ78 phrase records. Construction revalidates
the profile and publishes no handle on any configuration, capacity, reserved-
field, or alignment failure.

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

### Published LZW plus Adaptive Huffman boundary

LZW first completes its variable-width LSB-first code stream and zero-pads the
last partial byte. Adaptive Huffman consumes that finalized byte sequence as
ordinary symbols, so the dictionary padding byte remains visible to the entropy
model while LZW code boundaries remain invisible. Both states reset at every
outer frame.

For raw frame size `F` and maximum code width `W`, packed staging is bounded by
`ceil(F*W/8)` and the conservative Adaptive payload by 33 times that extent.
The reference 65,536-byte, 16-bit profile therefore admits 131,072 packed bytes,
4,325,376 payload bytes, and at most 65,280 generated LZW entries. All raw,
packed, serialized-frame, aligned-record, and aggregate extents must be checked
before mutation.

Decoding must reconstruct the exact packed bytes through a fresh FGK tree,
then apply the ordinary LZW width, reference, `KwKwK`, padding, and raw-extent
validator before private reconstruction and frame publication. The independent
raw-`A` vector fixes packed bytes `41 00`, Adaptive payload `41 00 00`, and the
complete 75-byte frame.

The first combined boundary now implements the validation half of that order.
It admits one exact complete frame, checks all generic and profile extents plus
caller-owned packed and phrase capacities before mutation, entropy-decodes into
private packed staging, and invokes the existing LZW validator before success.
The next boundary adds iterative LZW reconstruction into a separate private raw
span only after that validation succeeds. Raw capacity and aggregate bytes are
checked before entropy output, and malformed input cannot publish raw bytes.
The internal transactional frame decoder also checks complete destination
capacity before entropy output and copies private raw staging only after all
validation and reconstruction succeeds. No failure publishes a destination
byte. The later streaming and public adapters consume this transaction rather
than bypass it.

Encoding now follows the exact inverse ownership order. The LZW planner fixes
the complete code schedule and final padded packed byte in caller-owned staging
before Adaptive planning observes any symbol. The frame planner accounts for
the typed encoder table, packed span, descriptor, and exact entropy payload;
the encoder rejects short serialized output before writing and reproduces the
independent 75-byte frame. The subsequent streaming and public adapters retain
that exact representation.

The bounded streaming encoder now owns four caller-supplied regions: one raw
frame, the conservative packed-code ceiling, one complete encoded frame, and
the LZW encoder table. It emits a prebuilt stream prefix, fills and encodes only
complete outer frames, then drains immutable serialized bytes. This keeps
one-shot framing authoritative while satisfying one-byte I/O, output
starvation, nonterminal `Flush`, retained `EndInput`, and sticky terminal-state
requirements. The matching streaming decoder provides the inverse boundary
described below.

The streaming decoder now provides that inverse boundary. It separates prefix,
frame-header, frame-body, private reconstruction, and raw-drain states; admits
all byte and typed storage before collecting a body; and calls the exact
private-staging decoder only on a complete frame. Consequently a later corrupt
frame cannot leak a prefix of its raw bytes, while previously drained frames
remain committed and terminal error position remains reproducible.

The internal bounded profile now converts a public-style original size,
frame cadence, LZW parameters, and decoder limits into the exact byte regions
needed by those two transforms. Its encoder calculation uses the conservative
packed-code and Adaptive payload ceilings; its decoder calculation derives the
largest packed input and phrase table from local limits. Opaque typed regions
contain only LZW encoder entries or only decoder phrase entries, and checked
partition helpers reject forged sizes, alignments, shortages, and misalignment.
The public C adapter now binds this profile to the common allocation-free
three-region transform ABI. It recalculates the chosen direction at factory
creation and delegates opaque record construction to the checked partition
helpers, so no private C++ entry layout becomes part of the C ABI.
The public completion matrix constructs both directions exclusively through
that ABI and verifies binary data classes, deterministic encoding, arbitrary
chunk schedules, sticky terminal results, and frame-transactional rejection
of corrupted, truncated, or trailing final input.
The decoder fuzz boundary reuses the same exact-frame and incremental paths
with compile-time byte arrays, a phrase-table ceiling derived from nine-bit
code density, byte-derived chunk schedules, and a finite call budget. No input
controls allocation or expands the admitted storage limits.
The transactional CLI binds this profile only through its public C factory
and requirements query. Its 64-KiB raw cadence configures the two-byte-per-raw-
byte packed ceiling, the 33-byte-per-packed-symbol Adaptive payload ceiling,
65,280 generated entries, and an 8-MiB aggregate limit while leaving every
actual workspace extent, typed-record size, and alignment inside the checked
profile helpers.
The dependency-free benchmark uses that identical public configuration and
queries both directional workspace layouts. It verifies a complete byte-exact
round trip before timing fresh transform instances, then reports ratio,
directional throughput, all queried extents, and peak caller-reserved
workspace without imposing a performance threshold.
Interoperability schema 11 preserves the exact twenty-one-entry schema-10
order and appends `lzw-adaptive-huffman`. Generation round-trips all twenty-two
profiles; verification requires exact manifest order, foreign decode equality,
and byte-identical local re-encoding while retaining schema 1 through 10
support. MSVC and ClangCL locally passed that contract in both directions;
the pushed Windows/MSVC and Ubuntu 24.04 artifacts and an independently
generated Ubuntu 26.04/Clang bundle subsequently passed it in both operating-
system directions for all twenty-two archives at one full revision.

### Specified LZW plus Dynamic Range boundary

LZW first completes its variable-width LSB-first code stream and zero-pads the
last partial byte. Dynamic Range consumes that finalized byte sequence as
ordinary symbols, so the complete padding byte remains visible to the entropy
model while LZW code boundaries remain invisible. Both states reset at every
outer frame.

For raw frame size `F` and maximum code width `W`, packed staging is bounded by
`S = ceil(FW/8)` and the conservative range payload by `P = 2S + 5`. The
reference 65,536-byte, 16-bit profile therefore admits 131,072 packed bytes,
262,149 payload bytes, and at most 65,280 generated LZW entries. All raw,
packed, serialized-frame, aligned-record, and aggregate extents must be checked
before mutation.

Decoding must reconstruct the exact packed bytes through a fresh order-0 range
model, then apply the ordinary LZW width-change, reference, `KwKwK`, padding,
and raw-extent validator before private reconstruction and frame publication.
The independent raw-`A` vector fixes packed bytes `41 00`, range payload
`00 40 FF FF BF 00 00`, and the complete 79-byte frame. The first combined
boundary now validates one exact complete frame. It admits the complete header,
packed and entropy extents, caller capacities, and aggregate workspace before
range-decoding into private packed staging, then applies the existing LZW
validator. Its bounded private decoder also admits raw capacity and aggregate
storage before entropy output, then iteratively reconstructs the validated
phrase graph into private raw staging. The internal transactional boundary
also checks destination capacity before entropy output and copies the complete
private raw frame only after success. Public factories retain that transaction.
The exact-frame planner fixes the canonical packed-code extent and
final padding before range planning, checks their combined workspace and
generic header, and reports the complete serialized extent without writing a
frame. The deterministic complete-frame encoder uses that plan to serialize
the header, descriptor, and exact range payload and reproduces the independent
79-byte vector without partial writes on capacity failure. The bounded
streaming encoder collects one raw frame, prepares one complete immutable
encoded frame, and drains it before accepting the next frame. Chunking and
nonterminal `Flush` do not alter canonical bytes. The streaming decoder admits
bounded complete-frame storage from the parsed header, transactionally
validates and reconstructs it, and only then drains immutable raw bytes.
The internal profile calculator derives these direction-specific byte regions
and opaque aligned LZW record extents with checked aggregate bounds. Typed
partitioning rejects inconsistent byte counts, insufficient storage, and
misalignment before exposing encoder entries or decoder phrases.
The bounded C adapter publishes only size-tagged configuration, byte counts,
alignment, opaque buffers, and the ordinary transform handle. It recalculates
and repartitions the profile during creation, so no C++ record layout or
caller-modified requirement becomes trusted ABI state.
The public-ABI completion matrix drives this adapter over required binary
classes, multiple chunk schedules, stable terminal states, and fourth-frame
corruption, truncation, and trailing data. A failing fourth frame never
publishes its final raw byte while the three validated frames remain committed.
The bounded dual-path fuzz boundary fixes all encoded, packed, raw, output, and
phrase storage before accepting input and enforces a finite call budget. Its
permanent regressions preserve frame atomicity across every canonical
truncation, saturated extents, and invalid Dynamic Range descriptor padding.
The explicit `lzw-dynamic-range` CLI adapter selects 65,536-byte raw frames,
a 131,072-byte packed ceiling, a 262,149-byte range-payload ceiling, 65,280
generated dictionary entries, and an 8-MiB aggregate policy. It obtains all
three concrete workspace extents and opaque alignment from the public C
requirements query and uses the common temporary-file transaction, so a
malformed or trailing stream cannot publish a destination.
The dependency-free benchmark selects the identical public profile. Its
checked destination formula is `80 + 4N + 77K`; an untimed byte-exact round
trip gates all measurement. Fresh C transforms then report encoded ratio,
directional throughput, all six queried workspace extents, and the larger
three-region sum without a performance threshold.

### Specified LZW plus rANS boundary

The fourth rANS composition freezes the complete canonical LZW packed-code
byte stream before entropy processing. Scalar rANS remains unaware of the
LSB-first variable-width code grammar and final zero padding, so an entropy
block may split a packed code while the outer frame remains the shared
dictionary and model reset boundary.

For raw frame extent `F`, configured maximum code width `W`, packed extent
`S`, entropy block size `B`, and block count `K`, require
`0 < S <= ceil(FW/8)`, `K = ceil(S/B)`, `8K <= P <= S + 8K`, and exactly
`528K` descriptor bytes. The decoder must validate every rANS block into
private packed staging before checking LZW width transitions, references,
`KwKwK`, final padding, dictionary growth, and exact raw extent.

The first combined validator now admits the complete frame, rANS block views,
packed staging, and LZW phrase records before entropy processing. It validates
every rANS state path without output, reconstructs the packed region only
after all blocks succeed, and then invokes the ordinary LZW validator. No raw
byte is reconstructed or published, and a malformed later block cannot leave
partially reconstructed packed bytes.

The next private boundary admits and aggregate-counts the complete raw staging
region before entropy work. It then reuses the validated packed code graph and
the ordinary iterative LZW decoder to reconstruct exactly one raw frame.
Caller-visible publication remains separate, so malformed input and workspace
failures cannot expose a partial frame.

The caller-visible complete-frame boundary preflights the entire destination
alongside all private regions, performs the unchanged validation and private
reconstruction, and publishes exactly the declared raw extent in one final
copy. Destination bytes are never part of aggregate scratch accounting and
remain entirely unchanged on failure.

The encoding-side planning boundary first fixes the complete canonical packed
LZW region in caller-owned staging, then plans each rANS block over those exact
bytes. It computes and validates the synthesized frame and all aggregate
workspace without accepting serialized output. This prevents descriptor or
payload emission from observing a partial or differently chunked LZW stream.

The complete-frame encoder accepts only a fully successful plan and a complete
destination. It writes the generic header, then reproduces every tANS plan over
the immutable packed staging while placing descriptors and payloads at their
fixed offsets. Extent disagreement is an internal error, and insufficient
destination capacity is detected before serialized output mutation.

The bounded known-size streaming encoder owns caller-supplied storage for one
raw frame, its conservative packed-code ceiling, one exact serialized frame,
and the LZW encoder records. It emits the canonical stream header and LZW
parameters first, collects no more than one raw frame, completes planning and
encoding into the private frame region, and drains that immutable region
before accepting input belonging to a later frame. Consequently arbitrary
input and output chunking cannot change serialized bytes. `Flush` leaves a
partial frame open, while retained `EndInput` finishes and drains the final
short frame before reporting end of stream.

The bounded streaming decoder owns caller-supplied regions for one serialized
frame, its tANS block views, reconstructed packed codes, decoded raw bytes, and
LZW phrase records. It collects the fixed prefix and one frame header before
accepting the declared body extent, so every capacity and aggregate limit is
checked before entropy decoding. A complete frame is validated and expanded
only into private storage, then its raw bytes are drained before the next frame
header is collected. Later corruption therefore cannot retract or partially
publish the current frame, while malformed current-frame data publishes none
of that frame.

The complete-frame encoder accepts only a fully successful plan and a complete
destination. It writes the generic header, then reproduces every rANS plan over
the immutable packed staging while placing descriptors and payloads at their
fixed offsets. Extent disagreement is an internal error, and insufficient
destination capacity is detected before serialized output mutation.

The bounded known-size streaming encoder owns caller-supplied storage for one
raw frame, its conservative packed-code ceiling, one exact serialized frame,
and the LZW encoder records. It emits the canonical stream header and LZW
parameters first, collects no more than one raw frame, completes planning and
encoding into the private frame region, and drains that immutable region
before accepting input belonging to a later frame. Consequently arbitrary
input and output chunking cannot change serialized bytes. `Flush` leaves a
partial frame open, while retained `EndInput` finishes and drains the final
short frame before reporting end of stream.

The bounded streaming decoder owns caller-supplied regions for one serialized
frame, its rANS block views, reconstructed packed codes, decoded raw bytes, and
LZW phrase records. It collects the fixed prefix and one frame header before
accepting the declared body extent, so every capacity and aggregate limit is
checked before entropy decoding. A complete frame is validated and expanded
only into private storage, then its raw bytes are drained before the next frame
header is collected. Later corruption therefore cannot retract or partially
publish the current frame, while malformed current-frame data publishes none
of that frame.

The internal profile calculator bridges validated configuration and limits to
the streaming constructors. Encoding receives raw-frame bytes, conservative
`ceil(FW/8)` packed staging, the complete
`56 + 528K + S + 8K` frame ceiling, and aligned LZW encoder records. Decoding
receives serialized-frame, packed, and private-raw byte regions plus one
aligned opaque layout containing rANS block views followed by LZW phrase
records. Checked partition helpers reject inconsistent counts, offsets,
storage extents, and alignment before exposing typed spans. The returned
requirements directly construct a bounded streaming round trip.

The public C adapter retains the common three-workspace ownership model.
Primary storage is raw-frame input for encoding or complete serialized-frame
input for decoding. Secondary storage is partitioned into packed LZW staging
followed by encoded-frame or private-raw storage. One aligned opaque views
region contains encoder entries, or rANS block views followed by LZW phrase
records. The requirements query and factory both recalculate the internal
profile; the factory validates sizes and alignment, partitions typed views
privately, and publishes a transform handle only after construction succeeds.

Public-ABI completion evidence exercises only that C lifecycle. It spans empty
input, every one-byte value, all byte values, repeated and patterned input,
deterministic pseudo-random input, frame-boundary lengths, and multi-frame
streams under one-byte and mixed chunk schedules. Repeated terminal calls are
stable. Corruption, truncation, or extension of the final frame leaves every
byte of that frame unpublished while earlier drained frames remain committed.

The decoder fuzz boundary fixes serialized input at 8 KiB, raw publication at
4 KiB, frames at 1 KiB, packed-code staging at 4 KiB, rANS views at eight,
LZW phrase records from the packed-code ceiling, and aggregate storage before
accepting arbitrary bytes. One path invokes complete-frame parsing directly;
the other uses only the public C requirements, factory, process, and destroy
lifecycle under variable small chunks. A fixed call ceiling turns failure to
terminate into a reproducible harness failure.

The transactional command-line adapter now selects this public profile as
`lzw-rans`. It fixes raw frames and rANS blocks at 65,536 bytes, supplies only
public format and hard-limit values, obtains all three direction-specific
workspace regions from the public C query, and creates the transform through
the public factory. Existing-output refusal, sibling `.tmp` cleanup, strict
trailing-data rejection, and final atomic rename remain common CLI policy.

The dependency-free benchmark uses that same public CLI profile. It computes
checked complete-stream capacity `80 + 2N + 1128K`, queries and allocates each
direction independently, verifies one byte-exact round trip, and only then
times fresh public transforms. It reports compression ratio, directional
throughput, each primary/secondary/views extent, and the larger directional
workspace sum; performance is descriptive rather than an admission threshold.

Interoperability schema 23 appends this unchanged `lzw-rans` CLI profile once
after the frozen schema-22 order. The generator self-decodes before recording
size and SHA-256. The verifier requires all 34 canonical names in exact order,
decodes every archive, and requires byte-identical local re-encoding. The
compatibility test rejects a reordered schema-23 manifest, removes only
`lzw-rans` to derive schema 22, and verifies the unchanged schemas 22 through
1. Four-direction external validation passed at revision
`5397f261fa04ee49832d9f72b09960a156232aad` across Windows/MSVC, Ubuntu
24.04/Ninja, and Ubuntu 26.04/Clang producers.

The independent raw-`A` vector composes only the existing LZW encoder, scalar
rANS encoder, and generic serializers. It freezes packed bytes `41 00`, the
equal `00:2048` and `41:2048` normalized model, the eight-byte final-state
payload, and the complete 592-byte frame.

### Specified LZW plus tANS boundary

The fourth tANS composition freezes the complete canonical LZW packed byte
stream before entropy processing. The packed region includes zero high padding
through the final byte. tANS remains an untyped byte transform, so a block may
split a variable-width LZW code but may not split a byte or cross the outer
frame where the LZW dictionary and tANS tables both reset.

For raw size `F`, configured maximum code width `W`, packed size `S`, entropy
block size `B`, and `K = ceil(S/B)`, require nonzero
`S <= ceil(FW/8)`, exact `528K` descriptor bytes, and the checked blockwise
tANS payload ceiling. The decoder must validate all entropy descriptors and
state paths before reconstructing exactly `S` private bytes, then validate
LZW width transitions, first-literal and backward-reference rules, `KwKwK`,
dictionary growth, exact `F` expansion, exact packed exhaustion, and zero high
padding without caller-visible publication.

The independent raw-`A` boundary fixes packed bytes `41 00`, normalized
frequencies `00:2048` and `41:2048`, initial-state offset `0x000C`, two zero
transition bits, payload `0C 00 00`, and a complete 587-byte frame. This is a
format and vector reservation.

The first bounded validator realizes the validation-first boundary with
caller-owned tANS views, packed staging, and LZW phrase records. It admits all
serialized and workspace extents under one aggregate policy, validates every
tANS descriptor and state path without packed output, and only then performs a
second entropy pass into private staging. Complete LZW code-width, reference,
dictionary-growth, raw-extent, packed-exhaustion, and padding validation
follows; caller-visible publication remains absent.

The matching private decoder adds the complete raw extent to preflight and the
aggregate workspace calculation. After the same two-pass entropy validation
and complete LZW graph validation, the existing allocation-free LZW decoder
expands phrases iteratively into exactly the declared raw extent. There is
still no caller-visible output span, so every workspace remains disposable on
failure.

The transactional complete-frame wrapper additionally admits the full caller
output extent before descriptor parsing or any private mutation. Caller output
is not internal workspace and is therefore not charged to the aggregate
buffer limit. After entropy validation, packed reconstruction, complete LZW
validation, and private raw reconstruction all succeed, one final copy
publishes exactly the declared raw bytes. Short output and either-layer
malformation leave the complete caller output unchanged.

The encoding-side planning boundary first fixes the complete canonical packed
LZW region in caller-owned staging, then plans each tANS block over those exact
bytes. It computes and validates the synthesized frame and all aggregate
workspace without accepting serialized output. This prevents descriptor or
payload emission from observing a partial or differently chunked LZW stream.

The workspace profile turns those fixed bounds into direction-specific caller
storage. Encoding reports separate raw-frame, packed-code, complete encoded-
frame, and aligned LZW encoder-record regions. Decoding reports encoded-frame,
packed-code, private-raw, and one opaque aligned region partitioned into tANS
block views followed by aligned LZW phrase records. Partitioning is
transactional: invalid requirements, short storage, or misalignment returns no
typed view.

The C ABI binds that profile through `marc_lzw_tans_config`, its requirements
query, and factory. The public structure carries only fixed-width values and
hard limits; the aligned tANS and LZW object layouts remain private. Factory
failure leaves the opaque transform handle null.

The public completion matrix constructs both directions only through that C
factory. It fixes 64-byte frame and block boundaries, proves deterministic
archives across arbitrary chunking, and verifies that a malformed fourth frame
cannot publish its final raw byte or destabilize the repeated terminal error.
The bounded fuzz harness feeds each at-most-8-KiB input to both the private
complete-frame boundary and public C streaming decoder. All raw, encoded,
packed, phrase, and tANS-view storage is fixed before input is examined, and a
strict call budget turns any progress failure or hang into an immediate
invariant violation.

The transactional CLI binds `lzw-tans` only through that public C
configuration, requirements query, factory, process, and destroy lifecycle.
It derives conservative byte ceilings but never names or partitions private
LZW or tANS records. Encoded or decoded output remains temporary until the
whole operation succeeds, so a malformed final frame or strict trailing data
cannot publish a destination file.

The benchmark adapter selects the identical `lzw-tans` public profile. It
queries fresh caller-owned workspaces for each direction, proves one complete
byte-exact round trip before timing, then measures encode and decode
independently. The runner reports every queried region and peak reservation;
it does not inspect the opaque typed partition or enforce a performance floor.

Interoperability schema 29 appends the identical `lzw-tans` CLI archive once
after the frozen schema-28 order. Generation validates all 40 local archives
before writing the manifest; verification requires exact order, hashes,
foreign decode, and byte-identical local re-encoding. Compatibility derives
schema 28 by removing only `lzw-tans`, rejects reordered schema-29 manifests,
and then verifies every unchanged schema through version 1.
External four-direction verification at revision
`2dcc17c09477958c1f8777a266ecfefbb75217d2` confirms all 40 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers, including byte-identical re-encoding in both platform directions.

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

### Published LZD plus Adaptive Huffman boundary

LZD first freezes its complete canonical eight-byte reference-pair stream.
Adaptive Huffman consumes those bytes through one fresh FGK tree per outer
frame, without interpreting reference fields or the terminal absent-right
marker. For raw frame size `F`, token staging is bounded by
`S = 8*ceil(F/2)` and the conservative entropy payload by `33S`.

Decoding must reconstruct exactly the declared token extent, validate its
multiple-of-eight shape, backward phrase graph, checked expansion lengths, and
unique terminal absent-right rule, then reconstruct into private raw staging
before publication. Encoding must fix the deterministic LZD parse before
Adaptive planning. The independent raw-`A` vector fixes terminal token
`41 00 00 00 FF FF FF FF`, Adaptive payload `41 00 CC 3F 1D`, and a complete
77-byte frame.

The first combined boundary implements the validation half of that order. It
checks the complete generic frame, LZD and Adaptive extents, caller-owned token
and phrase capacities, and aggregate workspace before entropy output. It then
reconstructs the exact token region and invokes the ordinary LZD validator.
The next boundary invokes the ordinary iterative LZD decoder only after that
validation succeeds, writing into a distinct private raw span. Raw capacity,
the conservative phrase-count-plus-one expansion stack, and their aggregate
bytes are checked before entropy output. The internal transactional decoder
additionally checks destination capacity before entropy output and copies
private raw staging only after every layer succeeds. No failure publishes a
destination byte.

Encoding applies the inverse ownership order. The exact-frame planner first
fixes the complete deterministic LZD token stream in private staging, then
plans Adaptive Huffman over only those bytes. It accounts for the typed LZD
encoder records, token staging, descriptor, and exact payload before returning
the serialized extent. The encoder rejects insufficient destination capacity
before writing and reproduces the independent 77-byte frame. Streaming and
public adapters must build on these frame transactions rather than bypass them.

The bounded streaming encoder owns four caller-supplied regions: one raw frame,
the `8*ceil(F/2)` token ceiling, one complete encoded frame, and the typed LZD
encoder records. It emits the 80-byte stream prefix, collects exactly one
outer frame, invokes the exact-frame planner and encoder, then drains immutable
serialized bytes before accepting the next frame. Prefix or frame output
starvation retains all offsets and an already observed `EndInput`; `Flush`
does not close a partial frame. The aggregate policy counts raw, token,
serialized, and typed-record bytes before frame construction.

The bounded streaming decoder first collects and parses the 80-byte prefix,
then admits only a complete generic frame header whose token, entropy, phrase,
expansion, serialized, and raw extents fit all caller capacities and the
aggregate policy. It buffers the full encoded frame, invokes private-staging
validation and reconstruction, and only then enters raw draining. Therefore a
later malformed frame may follow already committed earlier output, but no byte
from the malformed frame is published. End-of-input remains retained while a
validated raw frame drains; truncation, trailing input, and repeated terminal
errors are deterministic.

The bounded profile converts a fixed original size, frame size, LZD parameters,
and decoder limits into direction-specific byte requirements. Encoder storage
reports raw-frame, token, complete-frame, and opaque typed-entry extents.
Decoder storage reports complete encoded-frame, token, private raw, phrase,
and expansion extents. The opaque decoder region places phrase records first,
aligns the following `uint32_t` expansion stack explicitly, and revalidates all
counts, offsets, total bytes, and base alignment during partitioning. Public
adapters can therefore reserve byte buffers without exposing C++ record layouts
or performing unchecked casts themselves.

The public C adapter preserves that exact ownership model. Its requirements
query returns direction-specific primary, secondary, and opaque aligned-view
extents; creation recalculates and repartitions those regions before binding
the existing streaming encoder or decoder. C callers never name or size an LZD
entry, phrase record, or expansion-stack element directly.
The completion boundary drives only this public adapter and verifies required
binary classes, byte-identical repeated encoding, arbitrary input/output
chunking, sticky success and failure, and whole-frame publication under final-
frame corruption, truncation, or trailing input.
The decoder fuzz boundary uses the same exact-frame and incremental paths with
compile-time byte arrays, 512 phrase records, 513 expansion references, byte-
derived chunk schedules, and a finite call budget. Serialized metadata cannot
increase an allocation or any admitted workspace ceiling.

The transactional CLI adapter selects this public C factory with 65,536-byte
raw frames, the checked 262,144-byte token ceiling, an 8,650,752-byte Adaptive
payload ceiling, and a 16-MiB aggregate limit. It obtains all direction-specific
workspace extents and opaque-view alignment from the public requirements query.
The command-line layer therefore owns file transaction policy but no private
LZD record layout or alternate stream representation.

The benchmark adapter uses the same public factory and fixed limits. It reserves
complete-stream output with checked arithmetic, performs an untimed byte-exact
round trip, then measures fresh encoder and decoder instances independently.
It reports queried caller-owned workspace rather than estimating private record
layouts, and imposes no throughput or compression-ratio pass threshold.

Interoperability schema 12 preserves the exact twenty-two-entry schema-11 order
and appends `lzd-adaptive-huffman`. Generation round-trips all twenty-three
profiles; verification requires exact manifest order, foreign decode equality,
and byte-identical local re-encoding while retaining schemas 1 through 11.
Local MSVC admission proves the generator, verifier, and compatibility chain;
the pushed Windows/MSVC and Ubuntu 24.04 artifacts and an independently
generated Ubuntu 26.04/Clang bundle subsequently passed it in both operating-
system directions for all twenty-three archives at revision
`7078d0ab20f6e0a1aeaa3c43e480ca866bf8a2fa`.

### Specified LZD plus Dynamic Range boundary

LZD first completes its fixed-width eight-byte little-endian reference-pair
stream. Dynamic Range consumes that finalized byte sequence as ordinary
symbols, so token and reference-field boundaries remain invisible. Both states
reset at every outer frame.

For raw frame size `F`, token staging is bounded by
`S = 8 * ceil(F/2)` and the conservative range payload by `P = 2S + 5`. The
reference 65,536-byte profile therefore admits 262,144 token bytes, 524,293
payload bytes, at most 32,768 generated phrases, and at most 32,769 expansion
references. All raw, token, serialized-frame, aligned-record, expansion-stack,
and aggregate extents must be checked before mutation.

Decoding must reconstruct the exact token bytes through a fresh order-0 range
model, then apply the ordinary LZD token-width, backward-reference, terminal-
absence, phrase-length, and raw-extent validator before iterative private
reconstruction and frame publication. The independent raw-`A` vector fixes
token bytes `41 00 00 00 FF FF FF FF`, range payload
`00 40 FF FF C4 DC 92 F3 69 BC 8B 00`, and the complete 84-byte frame. The
first combined boundary now validates one exact complete frame. It admits the
complete header, token and entropy extents, caller capacities, phrase records,
and aggregate workspace before range-decoding into private token staging, then
applies the existing LZD graph validator. Its bounded private decoder also
admits raw capacity, expansion-stack capacity, and aggregate storage before
entropy output, then iteratively reconstructs the validated graph into private
raw staging. The internal transactional boundary also checks destination
capacity before entropy output and copies the complete private raw frame only
after success. The public factory retains that transaction. The exact-frame
planner fixes the canonical token extent before range planning, checks encoder
records, token bytes, descriptor, payload, and generic header, and reports the
complete serialized extent without writing a frame. The deterministic
complete-frame encoder uses that plan to serialize the header, descriptor, and
exact range payload and reproduces the independent 84-byte vector without
partial writes on capacity failure. The bounded streaming encoder collects one
raw frame, prepares one complete immutable encoded frame, and drains it before
accepting the next frame. Chunking and nonterminal `Flush` do not alter
canonical bytes. The streaming decoder admits bounded complete-frame storage
from the parsed header, transactionally validates and reconstructs it, and only
then drains immutable raw bytes. The internal profile calculator derives these
direction-specific byte regions and opaque aligned LZD record extents with
checked aggregate bounds. Typed partitioning rejects inconsistent byte counts,
offsets, insufficient storage, and misalignment before exposing encoder
entries, decoder phrases, or expansion references. The public C ABI now maps
its fixed-width LZD configuration to this profile, queries all three workspace
regions, partitions opaque typed storage internally, and constructs the
matching immutable-direction streaming transform. The public completion matrix
then exercises this lifecycle exclusively across required data classes,
multiple chunk schedules, repeatable terminal states, and malformed final-frame
publication boundaries. The bounded dual-path fuzz boundary fixes encoded,
token, raw, output, phrase, and expansion storage before accepting input and
enforces a finite call budget. Its permanent regressions preserve frame
atomicity across every canonical truncation, saturated extents, and invalid
Dynamic Range descriptor padding. The explicit `lzd-dynamic-range` CLI adapter
selects 65,536-byte raw frames, a 262,144-byte token ceiling, a 524,293-byte
range-payload ceiling, 65,536 dictionary entries, and a 16-MiB aggregate
policy. It obtains all three concrete workspace extents and opaque alignment
from the public C requirements query and uses the common temporary-file
transaction, so a malformed or trailing stream cannot publish a destination.
The dependency-free benchmark selects the identical public profile. Its
checked destination formula is `80 + 16*ceil(N/2) + 77K`; an untimed byte-exact
round trip gates all measurement. Fresh C transforms then report encoded
ratio, directional throughput, all six queried workspace extents, and the
larger three-region sum without a performance threshold.
Interoperability schema 18 preserves the exact twenty-eight-entry schema-17
order and appends `lzd-dynamic-range`. Generation round-trips all twenty-nine
profiles; verification requires exact manifest order, foreign decode equality,
and byte-identical local re-encoding while retaining schemas 1 through 17.
Local MSVC admission proves the generator, verifier, reordered-manifest
rejection, and the complete compatibility chain. Revision
`fd11d1c7ef833873a02694da91f9f6d8d378948b` additionally has four-direction
external evidence across Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu
26.04/Clang.

### LZD plus rANS boundary

The next rANS composition freezes the complete canonical LZD eight-byte
reference-pair sequence before scalar rANS sees any byte. For raw frame extent
`F`, token bytes are bounded by `S <= 8 * ceil(F/2)` and remain a multiple of
eight. rANS divides the finalized byte region into `K = ceil(S/B)` blocks with
exactly `528K` descriptor bytes and payload interval `8K <= P <= S + 8K`.
A block may split a reference or token but cannot cross an outer frame.

Decoder ordering first validates every entropy block and reconstructs the
complete private token region, then applies LZD alignment, backward-reference,
terminal-absence, phrase-graph, and exact raw-extent validation. The initial
raw-`A` vector independently composes the standalone LZD and rANS encoders with
generic serializers. It fixes token bytes `41 00 00 00 FF FF FF FF`, normalized
frequencies `00:1536`, `41:512`, `FF:2048`, the nine-byte rANS payload, and the
complete 593-byte frame.

The first combined validator admits the complete serialized extent, rANS view
count, token staging, LZD phrase records, and aggregate workspace before
entropy processing. It parses and validates every block before reconstructing
any token byte, then fills the complete private token region and invokes the
ordinary LZD graph validator without expanding raw bytes. Corruption in a
later block therefore cannot leave a partially reconstructed token region, and
valid entropy carrying an invalid LZD graph fails only at the dictionary
boundary.

The private raw decoder extends that same preflight with the complete declared
raw extent and `phrase_count + 1` iterative expansion references, counting both
in the aggregate workspace before descriptor parsing. Only after entropy and
the complete LZD graph validate does it invoke the ordinary nonrecursive LZD
decoder into disposable raw staging. It publishes no caller-visible output;
short raw or expansion regions fail before token staging changes.

The transactional complete-frame boundary additionally admits the entire
caller destination before descriptor parsing, reuses the same validator and
private reconstruction, and copies exactly the declared raw extent once only
after success. Caller output is excluded from internal aggregate accounting;
short capacity, malformed entropy, and invalid LZD graphs preserve every
destination byte.

The encoder-side exact-frame planner first fixes the deterministic LZD parse
and serializes its complete canonical token sequence into caller-owned staging.
Only that immutable byte span is divided into rANS blocks. Encoder records,
token staging, every descriptor, and exact planned payload are checked as one
aggregate workspace before the synthesized frame header and complete extent
are accepted. The planner has no serialized-output span and therefore cannot
publish a partial frame.

The matching complete-frame encoder is plan-first. It admits the entire
serialized destination, writes the generic header explicitly, and regenerates
each descriptor and payload only from the frozen token staging. Repeated block
plans and final offsets must equal the exact plan; planner and capacity failure
therefore occur before any serialized byte is published.

The bounded known-size streaming encoder adds collection and drain state only.
It serializes the fixed stream prefix, collects at most one raw frame, creates
one complete immutable DD-511 frame, and drains it before accepting the next
frame. Raw collection, token staging, complete serialized frame, and aligned
encoder records remain caller-owned and are checked together at preparation;
one-byte I/O and `Flush` cannot change framing or encoded bytes.

The matching streaming decoder separates prefix, frame-header, frame-body, and
raw-drain states. Header admission fixes complete encoded, view, token, phrase,
expansion, and private-raw extents before body collection. Only the existing
private complete-frame decoder may transition an admitted body to raw drain,
so malformed entropy or phrase graphs cannot expose bytes from that frame.

The internal profile turns those constructor contracts into checked allocation
requirements. Encoding derives the raw, maximum token, complete-frame, and LZD
encoder-record regions from trusted known-size configuration. Decoding derives
the complete encoded, token, and private-raw byte regions from local limits and
partitions one opaque aligned region in rANS-view, LZD-phrase, expansion-stack
order. Both partition offsets are recomputed before any typed span is exposed.

The public C adapter retains exactly those three ownership regions. Its
size-tagged fixed-width config carries the known-size encoder parameters or
trusted decoder limits, and the direction-specific requirements query is the
only allocation authority. Factory construction repeats profile calculation,
opaque partition validation, and alignment checks before creating an
immutable-direction transform; no private record type enters the ABI.

The public-ABI completion matrix fixes 64-byte raw frames and rANS blocks and
uses only that allocation and transform lifecycle. It covers the required
binary classes, deterministic repeated encoding, one-byte and mixed chunking,
sticky terminal states, and frame-atomic rejection of a corrupt, truncated, or
extended fourth frame. The shared LZD schedule retains its original defaults
for the Adaptive Huffman and Dynamic Range instantiations.

The bounded decoder fuzz boundary drives both the private complete-frame
decoder and the public C streaming lifecycle. Fixed caller-owned arrays cap
serialized input at 8 KiB, total raw output at 4 KiB, one raw frame at 1 KiB,
rANS payload at 16 KiB, entropy metadata at eight block views, LZD phrase state
at 512 records, and iterative expansion state at 513 records. Input-derived
chunk sizes remain modulo bounded, and a fixed call ceiling turns any stalled
state machine into a reproducible invariant failure. Ordinary builds compile
the harness without executing a sanitizer campaign; canonical strict-prefix,
reserved-field, and saturated-frame-extent cases remain permanent tests.

The explicit `lzd-rans` command-line selector fixes 65,536-byte raw frames and
rANS blocks, supplies the checked 262,144-byte token and 262,176-byte payload
ceilings plus a 16-MiB aggregate policy, and obtains every direction-specific
workspace extent and alignment from the public C requirements query. It reuses
the common temporary-file transaction, so invalid or trailing input, output
collision, allocation failure, or codec failure cannot publish the requested
destination or leave its sibling temporary file.

The dependency-free benchmark selects that same public profile. Its checked
complete-stream capacity retains LZD's absent-right half-reference for odd
input, verifies a byte-exact public-ABI round trip before timing, and then
reports encoded ratio, encode/decode throughput, all queried workspace regions,
and the larger caller-owned total. Smoke measurements establish wiring and
correctness only, not representative performance.

Interoperability schema 24 appends the unchanged CLI profile once after the
frozen schema-23 order. The generator round-trips all thirty-five archives
before recording size and SHA-256; the verifier requires exact order, hashes,
foreign decode equality, and byte-identical local re-encoding. The compatibility
regression rejects reordered schema 24, removes only `lzd-rans` to reconstruct
schema 23, and then verifies every frozen schema through version 1. The
four-direction schema-24 cross-check passed at revision
`dad3638da2acb449afca969176194bf8323309f5` across the recorded Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 environments.

### LZD plus tANS boundary

The reserved `lzd-tans` profile places tabled tANS after the complete canonical
LZD reference-pair byte stream. LZD produces aligned eight-byte token records;
tANS consumes only bytes and may place a block boundary inside either
four-byte reference field or between fields, but never inside a byte or across
an outer frame. Both layers reset with the frame controller.

Decoder construction must admit every descriptor, payload, private token byte,
tANS block view, LZD phrase record, expansion reference, and raw staging region
before the corresponding mutation. All tANS blocks must validate before token
reconstruction, and the complete private token span must pass LZD alignment,
reference, phrase-growth, terminal, and raw-extent checks before any raw byte is
reconstructed or published. The initial reservation fixes representation and
bounds. The first internal combined component now implements that complete-
frame validation boundary with caller-owned views, token staging, and phrase
records. The private complete-frame decoder additionally admits the declared
raw extent and iterative expansion stack before entropy mutation, then invokes
the existing non-recursive LZD decoder only after complete graph validation.
Raw staging remains discard-only. The internal transactional decoder places a
distinct caller-output preflight around that private operation and copies the
complete raw extent once after success. This publication boundary remains
below any streaming or public API.
The inverse planning boundary completes the LZD parse and canonical token
serialization before tANS planning. It keeps raw input, aligned encoder
records, and token staging caller-owned, validates the synthesized frame
header, and reports exact block and frame extents without accepting a final
serialized destination.
The complete-frame encoder invokes that plan before destination admission,
then writes explicit generic-header, descriptor, and payload regions. A short
destination cannot expose a partial frame; repeated encoding over the frozen
tokens must reproduce the same bytes.
The bounded streaming encoder owns no storage. It serializes the fixed prefix,
collects one raw frame in caller memory, invokes the exact planner and encoder,
then drains the immutable result with independent input consumption and output
production. Finish is retained until all pending bytes drain, while flush does
not alter frame boundaries.
The matching bounded streaming decoder incrementally collects the prefix and
one serialized frame, admits all tANS and LZD workspaces from trusted header
bounds, validates and reconstructs privately, then drains only the committed
raw frame. A later malformed frame cannot expose partial output from that
frame; retained finish survives draining of an earlier valid frame.
The profile calculator derives the coupled worst-case raw, token, serialized,
typed-view, phrase, and expansion extents with checked arithmetic. Its opaque
storage partitioners validate exact size metadata and alignment before forming
typed spans, so a later ABI adapter need not duplicate layout arithmetic.
The public C adapter preserves the common three-region ownership model and
delegates all sizing and typed partitioning to that calculator. It adds only a
size-tagged fixed-width configuration and new symbols, leaving the established
ABI version and every existing structure layout unchanged.
The public completion matrix constructs both directions only through that C
factory. It fixes 64-byte frame and block boundaries, proves deterministic
archives across arbitrary chunking, and verifies that a malformed fourth frame
cannot publish its final raw byte or destabilize the repeated terminal error.
The bounded fuzz boundary drives both the internal complete-frame validator and
the public incremental decoder from the same at-most-8,192-byte input. All raw,
token, tANS-view, phrase, expansion, and output storage is fixed before input is
examined; byte-derived chunk sizes and a finite call budget prevent a malformed
stream from creating unbounded storage or execution.
The transactional CLI selects this public profile with 65,536-byte raw frames
and tANS blocks, checked 262,144-byte token and 393,224-byte payload ceilings,
four blocks, the public LZD entry bound, and a 16-MiB aggregate policy. It
allocates only the three queried workspace regions and publishes the output
file only after the complete transform succeeds.
The benchmark adapter reuses that exact public policy independently in each
direction. It admits a checked complete-stream destination, verifies one
byte-exact untimed round trip, then measures encode and decode separately while
reporting every queried region and the larger directional reservation.
Interoperability schema 30 appends the identical `lzd-tans` CLI archive once
after the frozen schema-29 order. Generation validates all 41 local archives
before recording the manifest; verification requires exact order, hashes,
foreign decode, and byte-identical local re-encoding. Compatibility rejects a
reordered schema-30 manifest, derives schema 29 by removing only `lzd-tans`,
and then verifies every unchanged schema through version 1.
External four-direction verification at revision
`827ddf085efb40c7d8f9bc27628977053179d84c` confirms all 41 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers, including byte-identical re-encoding in both platform directions.

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

### Published LZMW plus Adaptive Huffman boundary

LZMW first freezes its complete canonical four-byte reference stream.
Adaptive Huffman consumes those bytes through one fresh FGK tree per outer
frame without interpreting reference boundaries. For raw frame size `F`,
reference staging is bounded by `S = 4F`, the conservative entropy payload by
`33S`, generated phrases by `min(max(F-1,0), configured maximum, local limit)`,
and the nonempty expansion stack by that phrase count plus one.

Decoding must reconstruct exactly the declared reference extent, validate its
multiple-of-four shape, every literal or prior generated reference, the
adjacent-phrase graph, and exact declared raw extent before private iterative
reconstruction and publication. Encoding must fix the deterministic LZMW
parse before Adaptive planning. The independent raw-`A` vector fixes reference
`41 00 00 00`, Adaptive payload `41 00 0C`, and a complete 75-byte frame.

The first combined boundary implements the validation half of that order. It
checks the complete generic frame, reference and Adaptive extents, caller-owned
reference and phrase capacities, and aggregate workspace before entropy output.
It then reconstructs the exact reference region and invokes the ordinary LZMW
validator. The returned actual generated-phrase count determines the later
iterative expansion-stack ceiling. The private reconstruction boundary
conservatively checks raw and maximum expansion capacities plus aggregate bytes
before entropy output, then invokes the ordinary iterative LZMW decoder over
only the validated prefixes. The transactional complete-frame decoder
additionally checks destination capacity before entropy output and copies the
private raw span only after every layer succeeds. No failure publishes a
caller-visible byte.

Encoding applies the inverse ownership order. The exact-frame planner first
fixes the complete deterministic LZMW reference stream in private staging, then
plans Adaptive Huffman over only those bytes. It accounts for typed LZMW encoder
records, reference staging, descriptor, and exact payload before returning the
serialized extent. The encoder rejects insufficient destination capacity before
writing and reproduces the independent 75-byte frame.

The first bounded incremental encoder adds only one outer ownership layer. It
drains the immutable 80-byte prefix, collects at most one raw frame, freezes and
encodes that frame through the exact transaction, and drains it before accepting
the next frame. Raw input, complete reference staging, typed LZMW records, and
the complete serialized frame are caller-owned and checked as one aggregate
working set. Output starvation retains both prepared bytes and a previously
observed valid `EndInput`; nonterminal `Flush` never changes a partial frame.

The matching incremental decoder collects the prefix, admits each frame extent
from its checked generic header, and retains one complete serialized frame. It
invokes private reconstruction into the caller-owned raw staging only after the
body is complete, then drains that validated raw span. Consequently a malformed
later frame cannot publish any part of itself, while raw bytes from already
completed frames remain committed. Truncation and trailing data are terminal.

The internal profile now couples those transforms to caller-owned storage
without making private C++ records part of an ABI. Encode requirements expose
raw, reference, complete-frame, and one opaque typed-region byte extent plus
alignment. Decode requirements expose complete-frame, reference, private-raw,
and one opaque region containing separately aligned phrase and expansion spans.
Partitioning rederives offsets and totals before returning typed internal views.

The public C boundary now binds those requirements and streaming transforms to
the common allocation-free three-region lifecycle. The query exposes only byte
extents and alignment. Factory construction repeats profile calculation and
opaque partitioning before publishing a handle, so encoder entries, phrase
records, and expansion references remain private implementation types.

The public-ABI completion matrix constructs both directions only through that
factory. It covers required binary input classes, exact deterministic encoding,
one-byte and mixed chunk schedules, repeated terminal results, and a four-frame
transaction test. Corruption, truncation, or trailing data at the last frame
commits exactly the first three frames and no byte from the fourth.

The decoder fuzz boundary fixes exact-frame and incremental workspaces before
reading metadata: 1,023 phrase records, 1,024 expansion references, bounded
byte arrays, byte-derived chunk schedules, and a finite call budget. Serialized
input cannot enlarge any allocation or admitted workspace ceiling.

The transactional CLI adapter selects the same public C factory with 65,536-
byte raw frames, a checked 262,144-byte reference ceiling, an 8,650,752-byte
Adaptive payload ceiling, and a 16-MiB aggregate limit. It obtains every
direction-specific workspace extent and opaque-view alignment from the public
requirements query, leaving the command-line layer responsible only for file
transaction policy.

The benchmark adapter uses the same public factory and fixed limits. It
reserves complete-stream output with checked arithmetic, performs an untimed
byte-exact round trip, then measures fresh encoder and decoder instances
independently. It reports queried caller-owned workspace rather than estimating
private record layouts, and imposes no throughput or compression-ratio pass
threshold.

Interoperability schema 13 preserves the exact twenty-three-entry schema-12
order and appends `lzmw-adaptive-huffman`. Generation round-trips all twenty-
four profiles; verification requires exact manifest order, foreign decode
equality, and byte-identical local re-encoding while retaining schemas 1
through 12. Local MSVC admission proves the generator, verifier, and
compatibility chain; external artifacts remain a separate evidence step.

### Published LZMW plus Dynamic Range boundary

LZMW first freezes its complete canonical sequence of four-byte little-endian
references. Dynamic Range consumes those bytes through one freshly reset
adaptive order-0 model per outer frame without interpreting reference
boundaries. For raw frame size `F`, reference staging is bounded by `S = 4F`
and the conservative entropy payload by `P = 2S + 5`.

Decoding must reconstruct exactly the declared reference extent, validate its
multiple-of-four shape, every literal or prior generated reference, the
bounded adjacent-phrase graph, and exact raw extent, then reconstruct into
private raw staging before publication. Encoding must freeze the deterministic
LZMW parse before range planning. The independent raw-`A` vector fixes
reference `41 00 00 00`, range payload `00 40 FF FF BF 00 00 00`, and a
complete 80-byte frame.

The first combined boundary implements the validation half of that order. It
checks the complete generic frame, LZMW and Dynamic Range extents, caller-owned
reference and phrase capacities, and aggregate validation workspace before
entropy output. It then reconstructs the exact reference region, invokes the
ordinary LZMW validator, and reports the actual phrase and expansion ceilings
without reconstructing or publishing raw bytes. The next bounded boundary
preflights private raw staging, the conservative expansion stack, and their
aggregate bytes before entropy output. It then invokes the ordinary iterative
LZMW decoder only over the completely validated graph and writes solely into
discardable private raw staging. The transactional frame boundary also checks
the complete caller destination before entropy output and copies that private
raw extent once only after every operation succeeds. The exact-frame planner
performs the inverse bounded preparation: it freezes the deterministic LZMW
reference stream in caller-owned staging, plans Dynamic Range over those exact
bytes, validates the synthesized generic header and aggregate workspace, and
reports the complete serialized extent without writing serialized output. The
deterministic complete-frame encoder uses that plan to serialize the header,
descriptor, and exact range payload and reproduces the independent 80-byte
vector without partial writes on capacity failure. The bounded streaming
encoder collects one raw frame, prepares one complete immutable encoded frame,
and drains it before accepting the next frame. Chunking and nonterminal
`Flush` do not alter canonical bytes. The streaming decoder admits bounded
complete-frame storage from the parsed header, transactionally validates and
reconstructs it, and only then drains immutable raw bytes.
The internal profile calculator derives these direction-specific byte regions
and opaque aligned LZMW record extents with checked aggregate bounds. Typed
partitioning rejects inconsistent byte counts, offsets, insufficient storage,
and misalignment before exposing encoder entries, decoder phrases, or
expansion references.
The public C boundary expresses the same ownership through one fixed-width
configuration, a direction-specific requirements query, and the common opaque
transform lifecycle. Primary storage holds raw-frame collection while encoding
or encoded-frame collection while decoding. Secondary storage holds canonical
references followed by the encoded frame or private raw frame. The aligned
views region retains all C++ encoder, phrase, and expansion record layouts
behind the ABI.
The completion boundary exercises only that public C lifecycle. It fixes
64-byte raw frames and proves byte-identical output across repeated and
arbitrarily chunked calls, then demonstrates that a malformed fourth frame
cannot publish its final raw byte after three valid frames have committed.

The bounded fuzz boundary exercises both the private complete-frame decoder
and the public incremental stream decoder. Every byte and typed-record region
has a compile-time ceiling, arbitrary input is truncated to 8,192 bytes,
decoded output is capped at 4,096 bytes, and a checked call ceiling prevents
nontermination. Input-derived chunks may vary scheduling but cannot resize any
region.
The transactional `lzmw-tans` CLI selector fixes 64-KiB raw frames and tANS
blocks, the 262,144-byte canonical-reference ceiling, four block views, the
393,224-byte payload ceiling, and a 16-MiB aggregate policy. It obtains every
actual region from the public requirements query and preserves the shared
write-temporary, rename-on-success protocol.
The dependency-free benchmark fixes the same application profile and creates
both directions only through that public lifecycle. For input extent `N` and
nonempty frame count `K`, it admits the checked complete-stream ceiling
`80 + 6N + 2176K`, verifies one untimed byte-exact round trip, then measures
encode and decode separately and reports every queried workspace region plus
the larger directional reservation. Input and result buffers are not included
in the workspace metric.
Interoperability schema 31 appends the unchanged `lzmw-tans` CLI archive once
after schema 30's frozen forty-one-profile order. Generation round-trips all
forty-two archives before recording size and SHA-256. Verification requires
exact order, hashes, foreign decode equality, and byte-identical local
re-encoding; compatibility rejects reordered schema 31, removes only
`lzmw-tans` to recover schema 30, and then checks every frozen schema through
version 1. External four-direction verification at revision
`903181080556c3bb511ad4a2e5275837ebda48e7` confirms all 42 archives across
the recorded Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang
producers, including byte-identical re-encoding in both platform directions.
The bounded fuzz boundary exercises both complete-frame private decoding and
incremental stream decoding with fixed caller-owned arrays, input-derived
finite chunk sizes, and a checked call ceiling. Serialized metadata can never
resize those regions.
The command-line adapter fixes a 65,536-byte raw frame, 262,144-byte reference
ceiling, 524,293-byte range-payload ceiling, 65,536 generated entries, and a
16-MiB aggregate policy. It obtains every direction-specific workspace extent
and alignment from the public C requirements query and creates the transform
only through the public C factory.
The dependency-free benchmark retains that exact profile and public lifecycle.
It reserves complete-stream output with checked `80 + 8N + 77K` arithmetic,
requires an untimed byte-exact round trip, then reports descriptive timing and
all queried workspace extents.
Interoperability schema 19 appends the unchanged CLI representation exactly
once after schema 18's frozen twenty-nine profiles. The manifest verifier keeps
every earlier codec set explicit and requires exact order, count, size,
SHA-256, foreign decode equality, and byte-identical local re-encoding.
The established four-direction exchange verified all thirty archives across
Windows/MSVC, Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang producers at revision
`f8d51680a0ef827fa09f5782ad4ced4c335d346e`.

### LZMW plus rANS boundary

The sixth rANS composition freezes the complete canonical LZMW four-byte
phrase-reference sequence before scalar rANS sees any byte. For raw frame
extent `F`, reference bytes are bounded by `S <= 4F` and remain a multiple of
four. rANS divides the finalized byte region into `K = ceil(S/B)` blocks with
exactly `528K` descriptor bytes and payload interval `8K <= P <= S + 8K`.
A block may split a reference but cannot cross an outer frame.

Decoder ordering first validates every entropy block and reconstructs the
complete private reference region, then applies LZMW alignment, literal-or-
prior-reference, adjacent-phrase-graph, and exact raw-extent validation. The
initial raw-`A` vector independently composes the standalone LZMW and rANS
encoders with generic serializers. It fixes reference bytes `41 00 00 00`,
normalized frequencies `00:3072` and `41:1024`, the eight-byte rANS payload,
and the complete 592-byte frame.

The first combined validator admits the complete serialized extent, rANS view
count, reference staging, LZMW phrase records, and aggregate workspace before
entropy processing. It parses and validates every block before reconstructing
any reference byte, then fills the complete private reference region and
invokes the ordinary LZMW graph validator without expanding raw bytes.
Corruption in a later block therefore cannot leave partially reconstructed
references, and valid entropy carrying an invalid LZMW graph fails only at the
dictionary boundary.

The private raw decoder extends that same preflight with the complete declared
raw extent and conservative iterative expansion references, counting both in
the aggregate workspace before descriptor parsing. After entropy and the
complete LZMW graph validate, it reduces the active stack to
`dictionary_entries + 1` and invokes the ordinary nonrecursive LZMW decoder
into disposable raw staging. It publishes no caller-visible output; short raw
or expansion regions fail before reference staging changes. That private
validation layer itself is not a public entry point.

The transactional complete-frame boundary additionally admits the entire
caller destination before descriptor parsing, reuses the same validator and
private reconstruction, and copies exactly the declared raw extent once only
after success. Caller output is excluded from internal aggregate accounting;
short capacity, malformed entropy, and invalid LZMW graphs preserve every
destination byte.

The encoder-side exact-frame planner first plans and then materializes the
complete canonical LZMW reference region in caller-owned staging. Only that
frozen byte sequence is divided into rANS blocks. It reports exact descriptor,
payload, and serialized frame extents without writing a frame, and admits the
encoder dictionary plus all staged and planned bytes against the aggregate
workspace limit before a later encoder may publish output.

The deterministic complete-frame encoder invokes that plan before admitting
the serialized destination, then explicitly writes the generic header and each
528-byte descriptor and rANS payload into precomputed regions. Every repeated
block extent and final offset must equal the plan. A planner failure or short
destination therefore leaves the complete output unchanged.

The known-size streaming encoder adds no representation. It drains the fixed
80-byte prefix, collects at most one raw frame, prepares one immutable complete
frame, and drains it before accepting the next frame. `EndInput` remains sticky
across prefix and frame starvation; `Flush` does not close a partial frame.
All simultaneously held raw, reference, encoded-frame, and typed-entry storage
is caller-owned and checked as one aggregate.

The matching streaming decoder admits each generic frame header against all
encoded, typed, reference, expansion, and raw capacities before accepting the
body. A complete frame is decoded through rANS validation and LZMW graph
validation into private raw storage, then drained. Later-frame corruption can
therefore leave only earlier, fully validated frames committed.

The direction-specific profile calculator derives every caller-owned byte and
typed-record requirement from the same conservative frame bounds. Decoder
opaque storage is partitioned into aligned rANS views, LZMW phrase records, and
iterative expansion references only after its layout is recomputed and
validated, hiding internal C++ types from the public C ABI.
The public C factory now consumes only the size-tagged fixed-width config and
the three regions returned by the requirements query. It repeats profile and
partition validation before borrowing those regions for an immutable-direction
transform; construction failure publishes no transform.
The public-ABI completion matrix now exercises that boundary alone for all
one-byte values, representative binary and boundary-sized inputs,
byte-identical mixed chunk schedules, repeated terminal calls, and
frame-atomic rejection of a malformed final frame.
A bounded dual-path fuzz harness now drives both the private complete-frame
decoder and that public streaming lifecycle. Its input, output, encoded frame,
reference, rANS-view, phrase, expansion, and call counts are fixed before
untrusted parsing; permanent regressions retain truncation and malformed-field
atomicity.
The explicit CLI selector fixes 65,536-byte raw frames and rANS blocks, a
262,144-byte reference ceiling, four entropy blocks, a 262,176-byte payload
ceiling, and a 16-MiB aggregate policy. It obtains all direction-specific
storage and alignment from the public requirements query and retains the
existing temporary-file publication transaction.
The dependency-free benchmark reuses that public profile without private
layout knowledge. Checked complete-stream capacity is `80 + 4N + 2200K` for
raw extent `N` and nonempty frame count `K`, covering the worst-case reference
bytes, generic frame header, four descriptors, and four final states. An
untimed exact round trip gates ratio, throughput, and queried-workspace output.
Interoperability schema 25 appends the unchanged CLI profile once after the
frozen schema-24 order. Generation round-trips all thirty-six archives before
recording size and SHA-256; verification requires exact order, hashes, foreign
decode equality, and byte-identical local re-encoding. The compatibility
regression rejects reordered schema 25, removes only `lzmw-rans` to reconstruct
schema 24, and then verifies every frozen schema through version 1. The
four-direction schema-25 cross-check passed at revision
`bc4cfa45fc8787d5ec9277894bda0b10df0ef638` across the recorded Windows/MSVC,
Ubuntu 24.04/Ninja, and Ubuntu 26.04/Clang x86-64 environments.

### LZMW plus tANS boundary

The reserved composition first freezes the canonical little-endian four-byte
LZMW reference region, then divides those bytes into independently reset tANS
blocks. A block may split a reference but cannot split a byte or cross the
outer frame. Checked bounds require `0 < S <= 4F`, four-byte alignment,
`K = ceil(S/B)`, exact `528K` descriptors, and the sum of per-block
`2 + ceil(12n/8)` payload ceilings.

Decoder construction must admit every serialized and caller-owned extent
before parsing model data. Every tANS block must validate before private
reference reconstruction, and the complete reference span must pass ordinary
LZMW literal, generated-reference, adjacent-pair growth, dictionary-limit, and
exact-raw-extent validation before raw reconstruction or publication. The
independent raw-`A` vector composes standalone LZMW and tANS
components into a 587-byte frame with payload `FB 02 07`; no combined runtime
path or public profile existed at the specification step. The first internal
combined component now implements this complete-frame validation boundary with
caller-owned views, reference staging, and phrase records. Its private decoder
additionally admits the declared raw extent and iterative expansion stack
before entropy mutation, reconstructs only the validated LZMW graph into
disposable raw staging, and publishes exactly once only after every layer
succeeds. It adds no streaming transform or public surface.

The encoder-side exact-frame planner first plans and materializes the complete
canonical LZMW reference region in caller-owned staging. Only that frozen byte
sequence is divided into tANS blocks. It plans every normalized table and
transition payload, checks their exact aggregate extent and complete generic
header, and reports the exact frame size without writing serialized output.
Short encoder records or reference staging fail before reference mutation.
The deterministic frame encoder invokes that planner first, requires complete
serialized capacity, writes the generic header, then emits every tANS
descriptor and payload over the frozen reference span. It rechecks all planned
block extents and leaves serialized output untouched on any planner or capacity
failure.

The bounded streaming encoder owns caller-supplied storage for one raw outer
frame, its maximum canonical reference region, one complete serialized frame,
and the LZMW encoder table. It first drains the immutable stream prefix, then
collects exactly one outer frame, invokes the exact planner and encoder, and
drains the complete serialized frame without mutation. One-byte input and
output are valid. `EndInput` remains sticky while pending bytes drain; `Flush`
does not close a partial frame, and `ResetBlock` is unsupported.

The matching streaming decoder incrementally collects the fixed prefix and one
complete serialized frame. It validates header-derived tANS views, reference
staging, phrase records, expansion stack, raw staging, and aggregate live bytes
before accepting the frame body. Only the transactional complete-frame decoder
may populate private raw staging; that successful immutable frame then drains
under arbitrary output chunking. A later malformed frame cannot expose any of
its raw bytes or roll back earlier completed frames.

### LZMW plus tANS public profile

#### Profile workspace

The internal LZMW plus tANS profile calculator connects the bounded streaming
pair to caller-owned storage without exposing its layout as an ABI. Encoding
uses separate raw-frame, canonical-reference, complete-frame, and aligned
LZMW-entry regions. Decoding uses complete-frame, canonical-reference,
private-raw, and one aligned region partitioned into tANS block views, LZMW
phrases, and iterative expansion indices. All offsets and aggregate extents
are checked before a typed span is formed.

The public LZMW plus tANS C adapter translates one size-tagged fixed-width
configuration into that profile. Its requirements query returns primary,
secondary, and aligned opaque-view extents; its factory validates those exact
regions and constructs one immutable encode or decode transform. No C++ record
type, constructor, span, exception, or allocation layout crosses the ABI.

The public-ABI completion boundary exercises only that adapter. It fixes
64-byte raw and entropy blocks, proves byte-identical output across repeated
and arbitrarily chunked calls, and verifies that a malformed fourth frame
cannot publish its final raw byte after three valid frames have committed.

## Experimental typed-token context pipeline

The `0.2.x` design adds an orthogonal internal layer without changing the
version-1 byte-stream baseline:

```text
raw bytes
  -> LZSS parser
  -> bounded typed-token frame
  -> context model
  -> bounded modeled-event frame
  -> entropy backend
  -> format-2 frame
```

Decoding reverses those boundaries and publishes a raw frame only after every
layer has validated its complete private result. Direction remains immutable,
frame reset is shared by all layers, and no typed or modeled block crosses an
outer frame.

The first experiment deliberately uses LZSS rather than expanding the existing
six-by-five byte-stream matrix. LZSS exposes distinct Literal, Match, length,
and distance semantics while retaining a small deterministic token vocabulary.
The initial context model separates those fields and conditions them only on
already accepted token state. Context selection is independent of entropy
arithmetic, so later backends can compare speed, ratio, descriptor overhead,
and workspace over the same modeled-event sequence.

The four controlling specifications are:

- [LZSS typed-token protocol](design/lzss-typed-token-protocol.md);
- [context-model contract](design/context-model-contract.md);
- [entropy-backend contract](design/entropy-backend-contract.md);
- [experimental format 2.0](format.md#experimental-typed-token-format-20).

This is a reserved experimental representation, not a public profile. Public C
factories, CLI names, interoperability schemas, and completion claims remain
unchanged until the new decoder validator, vectors, bounded streaming pair,
negative tests, fuzz boundary, and benchmarks satisfy the normal admission
sequence.

### Format 2 header preflight

The first private decoder boundary parses all 112 stream-header bytes before
publishing configuration. It validates the fixed prefix, LZSS parameters,
contextual Dynamic Range parameters, context-model extension, local limits,
and every reserved field as one transaction.

For each nonempty frame, preflight validates the 64-byte `MRF2` header and the
available 16-byte entropy descriptor before accepting the declared payload
extent. Raw, token, modeled-event, entropy-decision, table, payload, and
serialized-frame bounds are checked before entropy decoding or token
allocation. The returned layout becomes visible only when the complete
declared frame is present and every header/descriptor check succeeds. Bytes
following that exact extent remain available for the next frame.

### Typed LZSS frame validation

The private dictionary/context boundary now has a value-only `Literal` and
`Match` representation independent of native layout and canonical transcript
bytes. A complete-frame validator walks caller-owned tokens without allocating
or reconstructing raw output. It validates variant-2 parameters, unused
fields, token kind, match distance against already produced history, match
length, overlap semantics, declared token count, exact raw extent, aggregate
output, and local token-storage limits.

The result retains the first failing token index and the validated raw prefix,
but no raw byte is published. Typed reconstruction may consume a frame only
after this validation boundary succeeds; context-model inversion independently
reconstructs and validates these same typed values from modeled operations.

### Typed LZSS private reconstruction

The typed reconstructor accepts only a complete caller-owned token frame and a
private raw staging span. It completes token-frame validation, required-output
capacity checking, and token/output non-aliasing checks before its first write.
Malformed tokens, local-policy failures, insufficient output, and overlapping
storage therefore leave the entire output span unchanged.

After those gates succeed, Literal publication and Match reconstruction have
no input-dependent failure branch. Matches copy one byte at a time from
`produced - distance`, so newly written bytes are immediately available for
the specified overlapping-reference semantics. Only the declared raw extent
is written; additional staging capacity is untouched. Publication to a public
downstream buffer remains the responsibility of a later complete Format 2
decoder stage.

### LZSS field-context inversion

The private inverse context boundary consumes a complete caller-owned modeled-
operation span. It derives every expected operation kind, context ID, alphabet,
and bypass width from previously accepted typed-token state; input values
cannot select another model. Reconstructed Match fields are passed through the
typed LZSS validator before the next token can affect context state.

Declared event, token, entropy-decision, and raw counts are checked both by
conservative frame bounds and by the exact walk. Operation storage, frame
output, and aggregate output are bounded before materialization. Only after the
full walk succeeds does the inverse write the declared number of typed tokens
to disjoint private staging. A malformed operation, short output span, or
aliasing span therefore leaves every caller-owned token unchanged.

### LZSS field-context forward modeling

The matching private encoder boundary first validates the complete typed-token
frame and then calculates exact modeled-event and entropy-decision counts
without writing output. Literal and Match classes are derived only after token
validity, reference history, raw extent, local storage, and aggregate-output
policy have succeeded.

Materialization requires the exact planned operation capacity and storage
disjoint from the input token span. It then emits the fixed contexts,
alphabets, values, and nonzero-width bypass operations in one failure-free
pass. Empty frames write nothing, excess caller capacity remains untouched,
and applying the inverse boundary reconstructs the original typed tokens.

### Contextual Dynamic Range decoder boundary

The private variant-2 backend decoder owns one fixed table for each of the 31
`LzssFieldContext` symbol contexts. Construction allocates no dynamic memory;
`begin` resets every frequency to one and validates the fixed schema,
descriptor, payload extent, and local table/model limits before reading the
canonical five-byte arithmetic prefix.

Only the context layer can request a symbol context and alphabet or a bypass
width. Each accepted Symbol updates exactly one selected model, while bypass
bits use fixed total two, decode least-significant bit first, and update no
model. Failures are sticky and do not publish the requested value. Finalization
requires exact event and decision counts, complete payload consumption, and
valid frequencies in all models before the private values may advance to
context inversion.

### Direct contextual range-to-LZSS bridge

The private Format 2 decode path connects the contextual Dynamic Range backend
to typed LZSS token staging without first materializing a
`ModeledOperation[]`. The bridge owns the same small `LzssFieldContext` state
machine used by the independently testable operation-level transform. For
each token it derives the required context, alphabet, and bypass width from
previously accepted tokens, requests only that shape from the entropy backend,
reconstructs one local typed value, and validates it against the current raw
history before advancing state.

Decoding is transactional. A first pass checks parameters, declared counts,
local limits, every entropy request, every reconstructed token, exact raw
extent, entropy final state, and payload exhaustion without writing token
storage. After output capacity and payload/output non-aliasing are established,
an identical second pass materializes only the declared token extent. The
operation-level forward and inverse transforms remain as specification and
test boundaries; omitting their native in-memory representation from this
runtime path does not alter the canonical modeled-event sequence or Format 2
bytes.

### Complete private Format 2 frame decode

The first complete private frame boundary composes Format 2 preflight, direct
contextual range-to-token decoding, typed-token validation, and LZSS raw
reconstruction. It reports the exact serialized frame extent only after all
four stages succeed; following bytes remain input for the next frame.

Preflight copies the validated header and descriptor into local values. Before
token decoding can write, the boundary converts declared token and raw extents
to host sizes, requires sufficient caller-owned staging, and proves the exact
serialized-frame, token, and raw regions pairwise disjoint with checked address
arithmetic.
This early alias gate is necessary because a later reconstructor-only check
would occur after token materialization and could already have changed an
overlapping raw region. In-place frame decoding is not part of this private
reference boundary.

After those gates, contextual decoding writes only private typed-token staging.
The independently validating reconstructor then writes only private raw
staging. A preflight, entropy, context, token, size, capacity, limit, or alias
failure returns zero serialized consumption and publishes no raw byte. Public
stream state and downstream publication remain a later transaction boundary.

### Private Format 2 streaming decode lifecycle

The private streaming decoder owns no dynamic allocation. Construction accepts
three caller-owned, pairwise-disjoint workspaces for one serialized frame, its
typed tokens, and its reconstructed raw bytes. It retains fixed inline arrays
for the 112-byte stream header and 64-byte frame header. Invalid limits or
overlapping construction workspaces enter a sticky `invalid_argument` state.

Its immutable decode state advances through:

```text
CollectStreamHeader -> CollectFrameHeader -> CollectFrameBody
                    -> DecodePrivateFrame -> DrainRawFrame
                    -> CollectFrameHeader | AwaitEnd -> Ended
```

The stream and frame headers may arrive one byte at a time. Once a complete
frame header is validated, the decoder computes and checks the exact serialized
frame, native token-storage, and raw-frame extents. Their checked sum must fit
`max_internal_buffered_bytes` before the frame body is accepted. The complete
private frame decoder runs only after all declared bytes have arrived.

No byte from a frame reaches caller output until that entire frame has decoded
and reconstructed successfully. Draining supports one-byte output and does not
accept the next frame until the current raw staging has been drained. Later
corruption therefore preserves already completed frames while publishing none
of the failing frame. Output may not alias raw staging because an overlapping
drain could corrupt bytes that remain pending.

`Flush` does not alter framing. `ResetBlock` and unknown flags are unsupported.
`EndInput` is remembered while draining, rejects every truncated intermediate
state, and reaches `EndOfStream` only after the declared original size is fully
drained with no trailing input. Repeated calls after end return
`EndOfStream`; the first error and byte position remain sticky.
Header syntax violations map to `malformed_stream`, while otherwise valid
stream or frame headers rejected by configured limits retain
`limit_exceeded`.

### Contextual Dynamic Range operation encoder boundary

The private variant-2 reference encoder consumes the complete bounded
`ModeledOperation` sequence produced by the forward `LzssFieldContext`
boundary. It shares the decoder's fixed 31-context schema, starts every model
with frequency one, codes bypass bits least-significant bit first with fixed
probability one-half, and uses the unchanged variant-1 interval and delayed-
carry termination arithmetic.

Encoding is an exact two-pass transaction. The planning pass writes nothing
while validating the entire operation sequence, counting decisions, enforcing
native operation-storage and payload limits, and determining the exact payload
extent. Only after sufficient disjoint output is proven does an identical pass
materialize that extent. The descriptor remains caller-owned and unchanged on
failure. This boundary deliberately stops at modeled operations; typed LZSS
production and complete-frame serialization remain separate composition
steps.

### Typed LZSS producer boundary

The private typed producer parses one complete bounded raw frame directly into
`Literal` and `Match` values. It does not construct or parse the canonical
variant-1 byte transcript. Both byte-token and typed-token encoders call one
shared match finder, so longest-match selection, nearest-distance tie breaking,
overlap comparison, maximum length, and the strict match-cost threshold remain
one policy.

Planning validates the variant-2 parameter subset and raw limits, executes the
complete deterministic parse without writes, computes the exact native token
extent, and enforces the checked raw-plus-token aggregate. Materialization
requires exact capacity and disjoint raw/token regions before repeating the
parse. It writes only the planned token prefix and leaves excess caller storage
untouched. Context modeling and entropy coding remain later private boundaries.

### Complete private Format 2 frame encode

The private complete-frame encoder composes raw-to-typed LZSS parsing,
`LzssFieldContext` forward modeling, contextual Dynamic Range planning, and
explicit little-endian frame serialization. Its planner materializes only the
caller-owned token and modeled-operation staging needed to determine exact
event, decision, payload, and serialized extents; it never writes serialized
output.

Raw, token, operation, and serialized regions are distinct workspaces. The
planner rejects overlapping raw/token/operation capacities before the first
staging write. The encoder additionally rejects the complete serialized-output
capacity when it overlaps any of those regions before invoking the planner.
The checked aggregate includes the exact raw frame, native token prefix,
native operation prefix, and serialized frame. Only an exact plan with a valid
frame header and descriptor may encode the payload and publish their canonical
headers. Excess serialized capacity remains untouched.

### Format 2 streaming encode lifecycle

The private streaming encoder accepts the known-size raw stream and emits the
canonical 112-byte Format 2 header followed by independently prepared frames:

```text
DrainStreamHeader -> CollectRawFrame -> PrepareCompleteFrame
                  -> DrainFrame -> CollectRawFrame | AwaitEnd -> Ended
```

Raw input is committed to one bounded caller-owned frame workspace. A complete
frame is parsed to typed LZSS values, modeled, entropy-coded, and serialized
before any byte from that frame reaches caller output. Token, operation,
serialized-frame, and raw-frame workspaces must be mutually disjoint, and
caller output may not alias any of them. Once prepared, a frame can drain one
byte at a time without accepting the following raw frame, so its sequence and
raw offset remain fixed until all serialized bytes are committed.

`Flush` publishes only bytes already representable and does not close a short
raw frame or alter Format 2 bytes. `ResetBlock` and unknown flags are rejected.
`EndInput` must coincide with the declared original-size boundary and is
remembered while the final header or frame drains. Empty input consists only
of the stream header. Repeated calls after completion return `EndOfStream`,
and construction or processing failures remain sticky.

### Format 2 profile and workspace layout

The private profile calculator converts a known-size LZSS configuration into
the canonical Format 2 stream header plus conservative requirements for the
streaming encoder. For `N` raw bytes in the largest frame it reserves at most
`N` typed tokens, `2N` modeled operations, `6N` arithmetic decisions, and
`12N + 5` payload bytes. The payload ceiling follows from the Format 2 count
bounds and at most two range-normalization bytes per decision, followed by the
five-byte termination. All products, aligned offsets, serialized extents, and
their aggregate are checked before requirements are published.

The decoder calculator derives serialized-frame, raw-frame, and typed-token
capacities only from local hard limits. Separate partition functions verify
that requirement records are internally consistent, storage is large enough,
and its base address satisfies the strongest required alignment before
returning typed spans. On every failure the output views remain empty. This
keeps language-neutral callers on byte storage while preventing them from
reimplementing native layout arithmetic.

### Format 2 C ABI boundary

The experimental public name is `lzss-contextual-dynamic-range`, distinct from
the byte-serialized Format 1 `lzss-dynamic-range` profile. Its additive ABI-1
functions use the `marc_lzss_contextual_dynamic_range_*` prefix and one
size-tagged plain-C configuration. No context, token, or operation structure
crosses the ABI.

Encoding assigns primary storage to one raw frame, secondary storage to one
complete serialized frame, and aligned opaque views storage to token and
operation staging. Decoding assigns primary to the serialized frame,
secondary to atomic raw output, and views to private typed tokens. The query
owns all extents and alignment. The factory validates the exact used prefixes
for capacity, alignment, pairwise non-overlap, and internal layout before
publishing a handle; failure leaves the caller handle null.

### Format 2 public completion boundary

The public completion matrix constructs both directions exclusively through
the C requirements query and factory. Aligned storage is owned by the caller;
tests do not reproduce private token or operation offsets. The matrix fixes
64-byte frames to cover exact frame transitions cheaply, while the profile
calculator remains authoritative for every capacity.

Decode publication remains one complete raw frame at a time. A malformed,
truncated, or extended final frame therefore cannot expose any byte from that
frame, even when earlier frames were already drained. Successful and failed
terminal states are sticky, and chunk schedules cannot alter encoded bytes.

### Format 2 fuzz boundary

The experimental fuzz entry crosses two decoder-visible boundaries: a private
complete frame after successful stream-header validation and the public C
streaming transform for every case. It cannot allocate an input-selected
workspace. Input, encoded frame, typed views, private raw frame, published raw
output, and process-call count all have independent compile-time or constant
ceilings.

Malformed status is normal fuzz completion. The harness aborts only when a
decoder violates consumption/production bounds, progress semantics, queried
workspace guarantees, final-input behavior, or its finite call budget. A
sanitizer campaign remains separate evidence from target construction.

### Format 2 CLI boundary

The transactional CLI admits Format 2 only through the explicit experimental
selector `lzss-contextual-dynamic-range`. It fixes a 65,536-byte frame policy,
then obtains primary, secondary, and aligned opaque-view requirements from the
public C lifecycle separately for encode and decode. The adapter neither
includes private Format 2 headers nor reproduces typed-token, modeled-operation,
or context-table layout. Existing temporary-file publication ensures that a
failed decode never replaces or leaves the requested destination.

### Format 2 benchmark boundary

The dependency-free benchmark reaches the experimental profile through the
same public-only lifecycle as the CLI. It owns caller storage returned by each
direction's requirements query and treats opaque views solely as aligned
bytes. Complete-stream capacity uses serialized Format 2 fields, while native
token and operation layouts remain confined to the library. Verification is
complete before the timed process call begins.

### Format 2 interoperability boundary

Interoperability schema 32 freezes schema 31's 42-entry order and appends the
experimental `lzss-contextual-dynamic-range` CLI archive once. Generation and
verification continue to treat the archive as opaque serialized bytes:
manifest order, size, SHA-256, foreign decoding, and byte-identical local
re-encoding are mandatory. Compatibility derives schema 31 by removing only
that final entry before traversing the existing chain to schema 1.
The recorded four-direction exchange at revision
`e9cf0c7d649cf32c9bc3a49bf3db9150370db381` confirms identical schema-32
bytes and decoding across the three x86-64 producers.

Schema 33 preserves that complete 43-entry order and appends only the compact
contextual-rANS CLI archive. The bundle identity becomes `marc-cli-v33`, while
the archive remains the unchanged Format 2 entropy-variant-3 stream already
selected by the compact public lifecycle. Verification treats all 44 archives
as opaque bytes and derives schema 32 by removing only the final compact entry
before traversing the historical compatibility chain. The fixed-descriptor
variant-2 diagnostic is deliberately absent.
The recorded four-direction exchange at revision
`2c30be4da1a80d01103dac0ee82fb0c4889f3af4` confirms identical schema-33
bytes and decoding across the three x86-64 producers.

### Reserved Format 2 contextual rANS boundary

The next entropy-backend experiment retains typed LZSS variant 2 and
`LzssFieldContext` variant 1 while selecting rANS variant 2. One scalar state
codes the entire bounded modeled frame. Symbol decisions select one of 31
static frame-local models; bypass decisions use a fixed binary model in the
same state. Encoding walks the modeled decisions backward and decoding obtains
each expected context from already accepted token state while walking forward.

The fixed descriptor deliberately serializes all 4,518 normalized frequencies.
This makes the 9,052-byte model cost and the 126,976-entry worst-case decode
workspace explicit before implementation. A later sparse descriptor is a
separate entropy variant rather than an invisible alteration. Complete model,
payload, state, token, and raw validation remains inside one frame-atomic
publication boundary.

The first implementation boundary stops at descriptor serialization and
validation. The fixed context alphabets and flattened offsets live in the
context layer and are referenced directly by both entropy backends. This
prevents Dynamic Range terminology from becoming an accidental owner or
compatibility surface for a context-model property.

The next boundary expands an accepted descriptor into caller-owned reference
decode tables without allocating or decoding state. Every context retains a
stable 4,096-entry address range, so later state decoding can select a table by
checked context ID alone. Validation, capacity admission, and a private
frequency snapshot occur before output is touched; the table span and active
flags become observable only after all 126,976 entries are initialized. This
preserves frame-atomic composition even if descriptor and workspace storage
overlap.

The scalar decoder is the first consumer of this table boundary. It retains
only spans, state, fixed context-use flags, and counters; all large storage
remains caller-owned. Start preflight precedes table construction, Symbol and
bypass requests advance the same state, and sticky error state prevents a
partially decoded value from being mistaken for accepted output. Completion
validates counts, model use, state, and payload extent before the decoder is
marked finished. Typed-token materialization and frame publication remain a
later composition boundary.

The inverse-model bridge is the next composition boundary. It drives the rANS
decoder from the same `LzssFieldContextState` rules as the Dynamic Range path,
but retains rANS's separate caller-owned table region. A write-free first pass
proves the full entropy and token sequence; a deterministic second pass writes
private typed tokens only after all three storage regions are proven disjoint.
This keeps entropy-table construction, token publication, and eventual raw
publication as explicit nested transactions without an intermediate modeled-
operation buffer.

The complete-frame decoder closes that raw-publication boundary. Separate
contextual-rANS stream and frame types prevent Dynamic Range fields or names
from becoming accidental compatibility aliases. Preflight validates the
64-byte frame header, complete 9,052-byte descriptor, exact payload extent,
frame sequence, declared sizes, and all configured limits before any caller
workspace changes. It then requires disjoint serialized, 126,976-entry table,
typed-token, and raw-output regions, performs the two-pass token inversion,
and reconstructs raw bytes only after token validation succeeds. This remains
a private one-frame primitive; streaming, encoding, public C lifecycle, CLI,
benchmark, and interoperability admission are later boundaries.

The first forward rANS boundary consumes an already materialized modeled-
operation sequence. Its write-free plan validates every operation, gathers all
frame-static Symbol statistics, normalizes each context independently, and
runs the reverse scalar state transition to obtain the exact payload extent.
The encode pass rejects operation/payload overlap and must reproduce that plan
before publishing the descriptor. This intentionally keeps context modeling
outside the entropy component; a later direct typed-token bridge may remove
the operation staging from a complete frame without changing these bytes.

The direct forward bridge now performs that composition. It validates typed
tokens and gathers model decisions in a forward pass, then reconstructs the
same pre-token context state while walking tokens backward. A monotonically
retreating previous-Literal cursor makes the reverse traversal linear even
though Literal context depends on earlier values. Both this bridge and the
materialized-operation reference use the same private entropy model builder
and reverse writer, and tests require identical descriptors and payloads. Raw
LZSS parsing and complete frame serialization remain separate outer steps.

The complete-frame encoder supplies those outer steps without reintroducing an
operation region. Planning validates the dedicated stream, exact raw-frame
extent, raw/token separation, typed parsing, direct entropy result, frame
header, descriptor, and aggregate raw-plus-token-plus-serialized workspace.
Encoding additionally excludes serialized output from both caller regions,
admits exact capacity, emits payload, and commits the validated header and
descriptor. The resulting frame is accepted directly by the complete-frame
decoder; at that milestone, streaming and public workspace partitioning
remained later layers.

The private contextual-rANS streaming encoder now owns the next lifecycle
boundary. It drains the dedicated 112-byte stream header, collects one exact
raw frame, prepares the complete header/descriptor/payload representation in
private caller-owned staging, and drains that immutable frame before accepting
more raw input. Raw, typed-token, serialized-frame, and caller-output regions
remain disjoint; the frame encoder's exact three-region aggregate is retained.
`Flush` does not close partial input, while `EndInput` is latched across output
starvation. At that encoder milestone, streaming decoding and public workspace
calculation remained later boundaries.

The paired contextual-rANS streaming decoder incrementally accepts the stream
and frame headers but admits the entire four-region live set before collecting
a frame body: serialized frame, fixed decode tables, typed tokens, and atomic
raw staging. Only a complete descriptor/payload decode and LZSS reconstruction
makes raw bytes drainable. A corrupt later frame therefore cannot retract or
alter an earlier committed frame, and cannot publish a partial current frame.
Streaming output remains disjoint from the raw staging it drains.

The private contextual-rANS profile boundary converts known-size encode
configuration and decoder hard limits into conservative byte and typed-view
requirements. Encoder views contain only typed tokens. Decoder views contain
the fixed rANS tables followed by an explicitly aligned typed-token array; the
partitioner recomputes this offset rather than trusting caller metadata. Both
directions keep raw and serialized frames as separate byte regions and reject
layout arithmetic or aggregate limits before publishing requirements.

The contextual-rANS C boundary is a distinct additive ABI-1 lifecycle. Its
requirements query assigns raw/serialized byte staging and opaque typed views
according to immutable direction, while the factory validates the exact three
used prefixes and recomputes their private partitions. Neither
`LzssTypedToken` nor `RansDecodeEntry` crosses the public header, and no
contextual Dynamic Range name serves as a compatibility alias.

### Contextual rANS public completion boundary

The public completion matrix constructs both contextual-rANS directions only
through the C requirements query and factory. Caller-owned aligned storage is
treated as opaque, and the test never reproduces private token or table
offsets. A 64-byte raw frame bounds modeled decisions at 384, payload at 776
bytes, and the complete serialized frame at 9,892 bytes.

Decode publication remains atomic at one complete raw frame. Corrupting,
truncating, or extending the fourth frame cannot publish its final byte after
the first three frames have drained. Successful and failed terminal results
are sticky, and input/output chunk schedules cannot change encoded bytes.

### Contextual rANS fuzz boundary

The contextual-rANS fuzz entry crosses the same two decoder-visible boundaries
as the first Format 2 profile: the private complete-frame decoder after a
valid 112-byte stream header and the public C streaming transform for every
case. The supplied input is capped above the minimum 9,052-byte descriptor so
complete frames remain reachable. Serialized staging, the fixed 126,976-entry
decode tables, typed tokens, private raw staging, public output, and process
calls all have predetermined ceilings.

The large native rANS views live in one fixed thread-local harness workspace,
not on the per-call stack and never at an input-selected extent. Malformed
status is normal completion; abort is reserved for workspace-query,
construction, accounting, progress, final-input, or call-budget invariant
violations. Compile-smoke and deterministic regressions are distinct from a
sanitizer campaign.

### Contextual rANS CLI boundary

The transactional CLI admits the second Format 2 profile only through the
explicit experimental selector `lzss-contextual-rans`. It fixes a 65,536-byte
frame and conservative decision/payload limits, then obtains primary,
secondary, and aligned opaque-view requirements from the public C lifecycle
separately for encode and decode. The adapter includes no private Format 2
header and reproduces no typed-token, modeled-operation, or rANS-table layout.
Existing temporary-file publication ensures that failed decoding leaves no
requested destination or temporary artifact.

### Contextual rANS benchmark boundary

The dependency-free benchmark reaches `lzss-contextual-rans` through the same
public-only lifecycle as the CLI. It owns caller storage returned by each
direction's requirements query and treats opaque table/token views solely as
aligned bytes. Complete-stream capacity includes the 112-byte prefix and each
frame's 64-byte header, 9,052-byte descriptor, eight-byte state allowance, and
`12N` payload allowance. Verification completes before timing begins.

The encoder path supplies that already bounded serialized-frame workspace
directly to the transactional complete-frame encoder. It does not pre-plan the
same frame in the streaming layer, and the frame planner does not separately
count tokens before invoking the typed-token encoder's own transactional
preflight. This preserves all atomic publication boundaries while reducing the
reference match finder from six full searches to two per frame.

### Compact contextual rANS descriptor boundary

Entropy variant 3 is a wire-level sibling of fixed-descriptor variant 2, not a
compatibility alias. It reconstructs the same fixed in-memory 4,518-frequency
array before table construction, so the model builder, scalar state machine,
typed-token bridge, and initial fixed decode-table layout remain separable from
descriptor parsing. A 31-bit active mask bounds the record loop independently
of input, and every context record is bounded by its compile-time alphabet.

The parser receives the exact descriptor extent from the validated frame
header, rebuilds into a private fixed array, verifies canonical dense/sparse
selection and complete consumption, and publishes only afterward. This keeps
malformed variable-length data outside the entropy state machine and retains
zero recursion and bounded workspace. A later active-table optimization may
change private workspace but must not change variant 3 bytes.

The scalar decoder has a distinct compact begin boundary. It resets before
parsing, reports the compact representation error beside its ordinary state
result, and then enters the same model-to-fixed-table and payload-state core as
variant 2. The shared table materializer revalidates model structure before
writing but applies no serialized-format size. Each outer begin path has
already charged its own exact descriptor extent, so variant 3 does not inherit
variant 2's 9,052-byte internal-buffer requirement.

The next private boundary feeds that compact begin result into the existing
LZSS field-context state machine. Fixed and compact representations therefore
share token-kind, literal, length, distance, bypass, token-validation, and raw-
extent logic. Compact format status remains adjacent to, rather than folded
into, the established token/entropy result. Validation performs a complete
write-free token pass; publication repeats the same bounded pass only after
token capacity and all descriptor/payload/table/token disjointness checks
succeed.

The compact complete-frame boundary retains the common Format 2 frame header
but preflights the header-carried variable descriptor extent rather than the
fixed variant-2 size. The exact compact descriptor span and payload then feed
the compact token bridge; successful typed tokens feed the existing bounded
LZSS reconstructor. Serialized input, decode tables, tokens, and private raw
output must all be pairwise disjoint before table construction. Frame
consumption is committed only after reconstruction succeeds.

The compact stream-header boundary uses the same fixed in-memory configuration
as contextual-rANS variant 2 while keeping wire identity explicit. A shared
private field core accepts an expected entropy variant, but named variant-2 and
variant-3 parse/serialize entries remain distinct. This prevents a later
streaming factory from treating the compact descriptor as an incidental
decoder preference rather than a decoder-visible format choice.

The compact streaming decoder keeps a separate state-machine type while
following the established Format 2 lifecycle. Its frame-body target is derived
from the validated variable descriptor extent, so it neither allocates nor
waits for fixed-descriptor padding. Complete compact frames decode into private
raw staging and are drained before the next header is accepted; previously
drained frames remain committed if a later compact frame fails.

The compact complete-frame encoder plans from the generated contextual model,
not from the fixed-descriptor frame plan. Model normalization and rANS payload
generation remain shared, while compact serialization determines the exact
descriptor extent before output admission. Payload, descriptor, and header are
then written into one already-sized private destination; the header records the
actual compact descriptor bytes.

The matching compact streaming encoder is a distinct transform type over the
same bounded collection and immutable-drain state machine as the fixed
contextual-rANS encoder. Its private representation choice selects the variant
3 stream-header serializer and compact complete-frame encoder; it does not
reinterpret variant 2 bytes. One raw frame, typed-token staging, and one exact
serialized compact frame remain caller-owned and mutually disjoint.

The compact private profile retains the fixed contextual-rANS typed-view
layout and payload ceiling but substitutes the 9,025-byte variant-3 descriptor
maximum in both direction-specific serialized-frame calculations. Explicit
compact query names return requirements suitable for the distinct compact
streaming transform types; the existing variant-2 profile remains unchanged.

The compact C boundary is an additive ABI-1 lifecycle with its own size-tagged
configuration type and explicit function family. It exposes only byte counts,
alignment, opaque transform handles, and process results. Private typed tokens,
rANS tables, representation selection, and descriptor layout remain hidden;
factory validation completes before publishing a handle.

### Contextual Dynamic Range encoder planning boundary

The first Format 2 streaming encoder supplies its already bounded serialized
frame workspace directly to the transactional complete-frame encoder. That
frame's internal plan invokes the typed-token encoder and context materializer
over their conservative caller-owned capacities, relying on each lower layer's
existing preflight rather than first making a duplicate count-only call.

The entropy plan remains distinct because its exact byte count determines the
serialized frame extent before output. Consequently one frame performs two
reference match searches, one context validation, one context materialization,
one entropy size plan, and one entropy write. No output becomes visible until
the same complete-frame transaction succeeds.

### Reserved Format 2 contextual tANS boundary

The next entropy-backend experiment retains typed LZSS variant 2 and
`LzssFieldContext` variant 1 while selecting tANS variant 2. One live state
codes the complete bounded modeled frame. Symbol decisions select independent
frame-static tANS tables for the expected contexts; bypass decisions select
one implicit fixed binary table in the same state. All tables reset at the
outer-frame boundary.

The descriptor adopts the already proven canonical dense/sparse model records
from the beginning and adds tANS final-bit metadata. It does not repeat the
fixed contextual-rANS descriptor as a reference format. Decoder admission
charges all 31 possible context tables plus the fixed bypass table before
construction, and complete descriptor, table, bitstream, state, token, and raw
validation remains inside one frame-atomic publication boundary.

The private contextual tANS format boundary now parses and serializes the
24-byte prefix and exact compact model records without constructing a state or
table. Canonical record analysis, parsing, and serialization are factored into
one private primitive shared with compact contextual rANS; backend-specific
prefix fields, payload bounds, valid-bit rules, and entropy-table limits remain
separate. Both formats publish only after complete validation.

The contextual tANS decode-table boundary now materializes the admitted model
into fixed caller-owned storage. Its 31 Symbol-context regions and one bypass
region reuse the standalone tANS transition authority; inactive regions are
zero. A private model snapshot and complete preflight precede the one output
write phase, so descriptor and workspace admission cannot publish a partial
table set. No live entropy state or typed-token reconstruction is attached yet.

The private contextual tANS decoder now attaches one bounded live state to
those fixed table views. Its caller drives the decoder with validated Symbol or
bypass requests; each request selects a table region while all transitions
share one LSB-first payload cursor. The decoder publishes values only after a
complete transition and accepts finish only after exact counts, context use,
terminal state, and bit extent agree. Typed-token reconstruction remains a
separate later composition boundary.
