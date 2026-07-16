# AGENTS.md

## 1. Purpose

This repository implements independently designed, streaming lossless-compression components.

The required dictionary coders are:

- LZ77
- LZSS
- LZ78
- LZW
- LZD: **Lempel-Ziv Double**
- LZMW

The required entropy coders are:

- Adaptive Huffman (the repository name may retain `DynamicHuffman` for API compatibility, but documentation must call it Adaptive Huffman)
- Blocked Huffman: fixed-size input blocks, with a static canonical Huffman model rebuilt and reset for every block
- Dynamic Range Coder
- rANS
- tANS

Standalone Static Huffman is **not** a required public codec. Static frequency counting, length-limited code construction, canonical-code assignment, table serialization, and decode-table construction remain mandatory internal primitives used by Blocked Huffman. They must operate only on bounded blocks.

The implementation must support round-trip compression and decompression, deterministic stream generation, bounded memory use, malformed-stream detection, and composable hashing at arbitrary byte-stream boundaries.

The initial goal is correctness, clarity, independent implementation, and testability. Optimize only after the reference implementation and format are stable.

### 1.1 Baseline scope exclusions and future extensions

The following are intentionally outside the baseline implementation scope:

- standalone Static Huffman as a public stream codec;
- Burrows-Wheeler Transform (BWT);
- Move-to-Front (MTF);
- BWT-oriented run-length transforms;
- archive/file metadata management;
- solid-compression grouping across multiple files.

These exclusions are primarily to preserve bounded memory, streaming behavior, implementation clarity, and a tractable validation surface. The architecture SHOULD remain extensible so that an outer archive/solid-group controller or a bounded BWT-family block profile can be added later, but baseline tasks, tests, algorithm IDs, and completion criteria MUST NOT require them.

Future extensions must receive their own format variant, limits, documentation, tests, and clean-room provenance. They must not silently alter an existing stream representation.

Normative words `MUST`, `MUST NOT`, `SHOULD`, and `MAY` are used intentionally.

---

## 2. Independent implementation and license hygiene

### 2.1 Clean implementation requirement

Implement algorithms from ideas, mathematics, public specifications, standards, original papers, and independently written design documents.

Do not copy or translate source code, comments, tests, tables, naming schemes, control flow, or distinctive implementation structure from GPL, LGPL, AGPL, or other copyleft implementations.

The presence of the same algorithm does not justify copying its expression. Renaming identifiers is not an independent implementation.

### 2.2 Permitted references

Prefer the following sources, in this order:

1. Published standards and RFCs.
2. Original papers and mathematical descriptions.
3. Patent documents used only as technical specifications, after checking current patent constraints where relevant.
4. Public-domain material.
5. Permissively licensed material when its license is recorded and compatible with this repository.
6. Independently authored pseudocode that is not derived from restricted source code.

Do not use GPL/LGPL source files as implementation references. Do not browse such source code merely to reproduce its behavior or optimization structure.

### 2.3 Provenance records

Maintain these documents:

```text
docs/
    architecture.md
    format.md
    implementation/
        references.md
        design-decisions.md
        clean-room-record.md
        test-vector-generation.md
```

`docs/implementation/clean-room-record.md` must record:

- references used;
- known implementations intentionally not consulted;
- design decisions made independently;
- generated-code prompts or task descriptions when relevant;
- dates and authors/reviewers;
- similarity-review results before release.

### 2.4 No legal guarantee

Do not claim that generated or independently written code is legally guaranteed to be non-infringing. State only the documented implementation process and provenance.

---

## 3. Architectural model

### 3.1 Core transform model

Every codec layer is a stateful byte-stream transform. The core codec API must be neutral rather than tied directly to files, sockets, or a particular reader/writer class.

A public pipeline may expose a reader at the top and a writer/sink at the bottom, but each codec must be implementable and testable as a standalone transform.

Use unambiguous terms:

- **upstream**: supplies input to a transform;
- **downstream**: consumes output from a transform;
- **source**: origin of bytes;
- **sink**: final consumer of bytes.

Do not use `parent` or `child` without defining which direction the term represents.

### 3.2 Direction is immutable

A pipeline is created in exactly one direction:

```text
Encode: raw bytes -> dictionary encoder -> entropy encoder -> frame encoder -> sink
Decode: source -> frame decoder -> entropy decoder -> dictionary decoder -> raw bytes
```

Direction is selected at construction and MUST NOT change while an object is alive.

Prefer explicit factories:

```text
create_encoder(config)
create_decoder(config)
```

A constructor taking `Direction::Encode` or `Direction::Decode` is acceptable if the direction remains immutable.

### 3.3 Core process contract

The API must report input consumption and output production independently.

Language-neutral contract:

```text
ProcessResult process(
    input_buffer,
    output_buffer,
    ProcessFlags flags);
```

Equivalent C++ shape:

```cpp
enum class StreamStatus {
    Progress,
    NeedInput,
    NeedOutput,
    EndOfStream,
    Error,
};

enum class ProcessFlags : std::uint32_t {
    None       = 0,
    Flush      = 1u << 0,
    EndInput   = 1u << 1,
    ResetBlock = 1u << 2,
};

struct ProcessResult {
    std::size_t input_consumed{};
    std::size_t output_produced{};
    StreamStatus status{StreamStatus::Progress};
    StreamError error{};
};
```

Required invariants:

- `input_consumed <= input.size`.
- `output_produced <= output.size`.
- Zero output is not End Of Stream.
- Zero input consumption is not automatically an error.
- `Progress` MUST NOT be returned when both counts are zero.
- `NeedInput` means no progress is possible without more input.
- `NeedOutput` means pending output exists and no progress is possible without output capacity.
- `EndOfStream` is returned only after all final bytes have been emitted.
- `Error` includes a stable error code and does not silently consume unspecified input.
- Repeated calls after the ended state must return `EndOfStream`, or a consistently documented API-misuse error. Choose one policy and test it.

### 3.4 Partial buffers are mandatory

All transforms must work correctly when:

- input arrives one byte at a time;
- output capacity is one byte;
- input and output split points vary arbitrarily;
- an output buffer fills in the middle of a token or bit sequence;
- a call consumes input but produces no output;
- a call produces output without consuming new input.

The encoded byte stream must be identical regardless of input/output chunking, unless the caller explicitly requests a block boundary or flush operation that is defined to alter the stream.

---

## 4. State, flush, finish, and end-of-stream

### 4.1 States

Each transform should behave as a clear state machine:

```text
Running -> Finishing -> Draining -> Ended
                    \-> Error
```

`EndInput` means that no more source input will follow. It does not mean that the transform has finished producing output.

After receiving `EndInput`, the transform must:

1. consume the final input;
2. resolve any pending dictionary phrase or token;
3. close the current entropy block;
4. emit required final coder state;
5. zero-pad the final partial byte where required;
6. finalize configured hashes/checksums;
7. emit all pending headers, payload, and trailers;
8. then return `EndOfStream`.

### 4.2 Flush is not finish

`Flush` requests that currently representable output be made available without ending the logical stream.

A codec that cannot perform a lossless non-terminal flush without closing a block may close the current block, but that behavior must be documented and deterministic.

`ResetBlock` explicitly ends the current independently coded block and resets the state required by that layer. It does not imply whole-stream termination.

Do not conflate:

- temporary input starvation;
- temporary output starvation;
- block completion;
- stream completion;
- error.

---

## 5. Framing and block control

### 5.1 Frame controller

Use a frame/block controller above the codec layers to coordinate boundaries that must affect multiple layers.

A synchronized frame boundary may control:

- dictionary reset;
- entropy-model reset;
- per-frame hash reset/finalization;
- frame header and trailer generation;
- uncompressed and compressed length recording.

Do not allow independent layers to interpret the same numeric block size in different units without an explicit definition.

### 5.2 Units

Every size parameter must state its unit. Supported units may include:

```text
Bytes
Symbols
Tokens
```

For the baseline byte-stream implementation:

- dictionary input and output boundaries are measured in bytes;
- Huffman input symbols are 8-bit byte values unless a typed-token interface is explicitly selected;
- the default outer frame size is configurable and SHOULD be 1 MiB of uncompressed bytes;
- no implementation may allocate an unbounded frame.

### 5.3 Blocked Huffman definition

Blocked Huffman is defined as follows:

- input is divided into fixed-size blocks;
- the size is measured in input symbols to the Blocked Huffman layer;
- the baseline alphabet is `0..255` and therefore symbols equal bytes;
- the default block size is 65,536 symbols;
- each block independently gathers frequencies, constructs a canonical Huffman code, writes its code description and payload, and then resets;
- the final short block is valid;
- empty input is valid;
- a block with one distinct symbol is valid and must have an unambiguous representation;
- blocks may select a raw/uncompressed representation when that is smaller, if this option is specified in `docs/format.md`.

### 5.4 Internal static Huffman primitives

Do not expose standalone Static Huffman as a public stream codec, pipeline algorithm, or top-level stream-format algorithm ID.

Blocked Huffman must use independently testable internal primitives for:

- bounded frequency counting;
- length-limited Huffman code-length construction;
- canonical code assignment;
- LSB-first encoder-code reversal;
- deterministic code-length serialization;
- bounded encoder and decoder table construction;
- table validation.

These primitives operate on one finite Blocked Huffman block at a time. They must not retain or buffer an unbounded logical stream. A future one-shot Static Huffman helper may be added only as a non-baseline API over a caller-supplied finite buffer and must not introduce a new stream format without an explicit format variant.

### 5.5 Buffered algorithms

The following encoders normally require bounded block buffering:

- Blocked Huffman;
- rANS;
- tANS.

The implementation must enforce configured maximum block size and memory limits before allocation.

---

## 6. Byte order, bit order, and serialization

### 6.1 Byte order

All multi-byte integers in the repository-defined stream format are serialized in **little-endian** byte order.

Examples:

```text
uint16 0x1234     -> 34 12
uint32 0x12345678 -> 78 56 34 12
```

This rule is independent of host architecture.

### 6.2 Bit packing

Bit payloads are **LSB-first**.

- The first bit written occupies bit 0 of the first output byte.
- Numeric bit fields are emitted from the numeric value's least-significant bit toward its most-significant bit.
- Bit readers and writers must be exact inverses.

Example: writing the bit sequence `1,0,1,1,0,0,1,0` produces byte `0x4D`.

### 6.3 Huffman representation

Canonical Huffman codes are assigned using the conventional canonical numeric construction.

Before writing a code through the LSB-first BitWriter, reverse the code bits within the code length. Store the reversed value in the encoder lookup table.

The decoder lookup table must be built for the physical LSB-first stream representation.

Do not change canonical code assignment merely to hide bit-order handling.

### 6.4 Alignment and padding

- Stream headers and block headers begin on byte boundaries.
- Entropy payloads may end at a non-byte boundary.
- The next header/trailer begins at the next byte boundary.
- Unused high bits in the final payload byte must be zero.
- Strict decoding must reject non-zero padding bits.

### 6.5 Explicit serialization only

Do not serialize native structs or classes directly.

Forbidden patterns include unchecked casting of byte buffers to integer or structure pointers.

Use explicit helpers such as:

```text
load_le16 / store_le16
load_le32 / store_le32
load_le64 / store_le64
read_bits / write_bits
```

Serialization must not depend on:

- host endianness;
- alignment;
- compiler ABI;
- structure padding;
- enum size;
- signed-integer representation;
- strict-aliasing violations.

Use checked arithmetic for all offsets, lengths, state updates, and allocation sizes.

---

## 7. Stream format

Before implementing an encoder, define the corresponding exact decoder-visible representation in `docs/format.md`.

At minimum, the top-level stream format must identify:

```text
magic
format version
feature flags
dictionary algorithm ID
entropy algorithm ID
algorithm variant IDs
algorithm parameters
frame size or block-size rules
original size, or an explicit unknown-size marker
hash descriptors
```

Each frame/block must provide enough information to validate bounds before decoding its payload. Include, as appropriate:

```text
uncompressed size
compressed payload size
model/table description size
final valid-bit count
coder initial/final state
reset flags
hash/checksum values
```

Unknown algorithm IDs, unsupported versions, impossible sizes, invalid tables, or contradictory parameters must be rejected before unsafe allocation or decoding.

Output must be deterministic for the same input and configuration.

---

## 8. Hashing and integrity checks

### 8.1 Hashing is a composable tap

Do not embed hashing logic separately into every codec.

Implement a `HashTap`, observer, or sink decorator that can be inserted at any byte-stream boundary:

```text
raw source
  -> HashTap(uncompressed)
  -> dictionary codec
  -> HashTap(dictionary-byte-output)
  -> entropy codec
  -> HashTap(compressed-payload)
  -> frame/sink
```

The final sink may also be wrapped. The stream writer is not excluded from integrity checking.

### 8.2 Generic hash interface

Hash algorithms are supplied as objects implementing a stable interface similar to:

```cpp
class IHashAlgorithm {
public:
    virtual ~IHashAlgorithm() = default;
    virtual void reset() = 0;
    virtual bool update(std::span<const std::byte> bytes) = 0;
    virtual bool finalize(std::span<std::byte> digest_out) = 0;
    virtual std::size_t digest_size() const noexcept = 0;
    virtual HashAlgorithmId algorithm_id() const noexcept = 0;
};
```

A hash tap must process each committed byte exactly once.

Do not hash the full capacity of an output buffer. Hash only the `output_produced` bytes that are part of the defined logical boundary.

### 8.3 Hash target and scope

Every stored digest must specify both target and scope.

Suggested targets:

```text
UncompressedBytes
DictionarySerializedBytes
CompressedPayload
FrameCanonicalBytes
```

Suggested scopes:

```text
WholeStream
PerFrame
PerBlock
```

The exact inclusion range must be defined, including whether it covers:

- headers;
- payload;
- zero padding;
- trailers;
- length fields;
- hash descriptor fields.

A hash value must not recursively include itself.

The recommended baseline is:

- optional per-frame checksum over the frame's uncompressed bytes;
- optional whole-stream cryptographic hash over all uncompressed bytes;
- optional compressed-payload checksum over the exact finalized payload bytes, including specified zero padding.

Checksums detect accidental corruption. They do not provide authenticity. Authentication requires a MAC or digital signature and is outside the baseline codec requirement.

---

## 9. Dictionary coder requirements

All dictionary coders must provide encoder and decoder implementations, deterministic output, bounded state, reset support, and round-trip tests.

### 9.1 Common requirements

- Operate on arbitrary binary data, not text.
- Handle empty input.
- Handle one-byte input.
- Handle highly repetitive and incompressible input.
- Define maximum dictionary/window sizes.
- Reject invalid references during decoding.
- Reject output expansion beyond configured limits.
- Define tie-breaking rules so encoding is deterministic.
- Define pending-phrase behavior at `EndInput`.
- Do not read past supplied input or write past supplied output.

### 9.2 LZ77

Define at least:

- window size;
- minimum and maximum match length;
- distance range;
- token representation;
- overlap-copy semantics;
- longest-match selection;
- deterministic tie break, preferably nearest distance on equal length.

### 9.3 LZSS

Use explicit literal/match tokens. Emit a match only when its encoded cost is strictly better than a literal sequence according to the defined token format.

### 9.4 LZ78

Define phrase-index width/growth, reset behavior, end marker or frame-length termination, and handling of a final existing phrase without a following byte.

### 9.5 LZW

Define:

- initial alphabet;
- first free code;
- clear/reset code if present;
- end code if present;
- code-width growth rule;
- dictionary-full behavior;
- the decoder's `KwKwK` case;
- bit packing using the repository LSB-first rule.

### 9.6 LZD

LZD means **Lempel-Ziv Double**.

Document the exact phrase-parsing and phrase-pair dictionary rules before implementation. Do not substitute another algorithm that happens to share the acronym LZD.

### 9.7 LZMW

Document the exact rule for inserting concatenations of consecutive parsed phrases, dictionary reset behavior, and deterministic longest-match lookup.

---

## 10. Entropy coder requirements

### 10.1 Internal canonical Huffman primitives

These are required support components for Blocked Huffman, not a standalone public entropy codec.

Required behavior:

- gather frequencies for one finite, configured-size block;
- construct a valid length-limited canonical Huffman code;
- serialize code lengths deterministically;
- support zero-frequency symbols;
- support a one-symbol alphabet;
- define the empty-block representation;
- validate oversubscribed and otherwise invalid code descriptions;
- define whether incomplete but decodable code spaces are accepted or rejected, and apply that policy consistently;
- provide a bounded fast decode table with a safe bounded fallback path;
- reject invalid code paths and premature payload termination;
- expose no top-level `StaticHuffman` stream algorithm ID in the baseline format.

The exact maximum code length and code-length serialization used by Blocked Huffman must be specified in `docs/format.md`.

### 10.2 Adaptive Huffman

The baseline Adaptive Huffman variant is **FGK** unless `docs/format.md` explicitly defines another variant ID.

Required behavior:

- synchronized encoder/decoder tree updates;
- NYT node handling for previously unseen symbols;
- sibling-property preservation;
- deterministic node numbering and swap rules;
- frequency rescaling before counter overflow;
- identical rescaling behavior in encoder and decoder;
- malformed-stream and impossible-tree detection.

A later Vitter implementation may be added as a distinct stream variant. Do not silently change variants under one algorithm ID.

### 10.3 Blocked Huffman

Use the definition in Section 5.3. Each block resets all Huffman statistics and tables.

### 10.4 Dynamic Range Coder

The baseline implementation is a byte-oriented integer range coder with an adaptive order-0 model over byte symbols.

The exact normalization and carry rules must be documented before coding. Encoder and decoder must use exactly the same integer arithmetic.

Default adaptive model guidance:

- initialize all byte-symbol frequencies to one;
- use a deterministic cumulative-frequency representation;
- cap total frequency at a documented bound;
- rescale deterministically, retaining every active symbol with non-zero frequency;
- support a Fenwick tree or equivalent bounded structure for cumulative lookup;
- check all multiplication, division, and interval invariants.

Do not use floating-point arithmetic in the coding path.

### 10.5 rANS

The baseline rANS implementation must be block based and byte-renormalized.

Required behavior:

- normalized total frequency is a power of two;
- frequencies are normalized deterministically and no present symbol disappears;
- symbols are encoded in reverse logical order;
- decoded symbols appear in forward logical order;
- renormalization bytes and final state have one exact documented layout;
- final state integers use little-endian serialization;
- state and threshold arithmetic are checked;
- single-symbol alphabets are handled explicitly;
- the decoder rejects states outside valid bounds.

Recommended baseline parameters, unless the format document justifies different values:

```text
64-bit internal state
byte-wise renormalization
table_log = 12
total_frequency = 4096
```

Interleaved and SIMD variants are later optimizations and must retain a distinct format variant when their byte stream differs.

### 10.6 tANS

The baseline tANS implementation is table based and block buffered.

Required behavior:

- table size is a power of two;
- frequency normalization is deterministic;
- symbol spreading is deterministic and documented;
- encode and decode tables are validated before use;
- additional bits use LSB-first packing;
- initial/final state representation is documented exactly;
- table log is bounded by decode limits;
- invalid state transitions are rejected.

Recommended default `table_log` is 12, with a conservative documented supported range.

Do not label an implementation FSE-compatible unless it actually matches an explicitly documented FSE format.

---

## 11. Byte-stream and typed-token integration

### 11.1 Required baseline

The required baseline connects all layers as byte streams:

```text
Raw bytes
  -> dictionary codec
  -> deterministic token serialization
  -> byte-oriented entropy codec
  -> framed bitstream
```

Every dictionary token format must have an exact canonical byte serialization.

### 11.2 Optional higher-compression interface

A typed-event interface may be added later to code fields with separate models:

```text
token type
literal
match-length class
match-distance class
extra bits
```

Typed-token support is an extension, not a reason to omit the required byte-stream implementation.

Do not hash an abstract token sequence unless its canonical serialization is fully specified.

---

## 12. Limits, malformed input, and safety

All decoders process untrusted input.

Provide configurable hard limits for at least:

```text
maximum total output size
maximum frame size
maximum block size
maximum compressed payload size
maximum dictionary entries
maximum LZ distance
maximum LZ match length
maximum Huffman code length
maximum entropy table size
maximum range-model total
maximum internal buffered bytes
maximum expansion ratio
maximum recursion depth, preferably zero recursion in parsers
```

Requirements:

- Validate sizes before allocation.
- Reject integer overflow and truncation.
- Reject references before the start of dictionary history.
- Reject invalid code tables before decoding payloads.
- Reject impossible ANS/range states.
- Reject premature End Of Stream.
- Reject trailing data when the selected strict mode forbids it.
- Never loop indefinitely on malformed input.
- Never return `Progress` without progress.
- Avoid recursive tree processing when input-controlled depth could exhaust the stack.
- Do not expose uninitialized memory in encoded output or errors.

A decoder error must be reproducible and report a stable category and byte/bit position when practical.

---

## 13. Memory and performance policy

Correctness comes first.

- Use bounded buffers.
- Reuse allocated working memory across calls where practical.
- Avoid allocation in the steady-state inner loop after initialization, but do not sacrifice clarity in the reference implementation.
- Keep a clear reference implementation before adding hash-chain, trie, SIMD, reciprocal-division, table-fusion, or multi-state optimizations.
- Optimized implementations must produce the same specified output as the reference implementation when using the same format variant.
- Benchmark encode throughput, decode throughput, peak memory, and compression ratio separately.
- Do not use undefined behavior as an optimization.

Instances may be documented as not thread-safe. Separate instances must be safely usable in parallel without global mutable state.

---

## 14. Testing requirements

### 14.1 Round-trip tests

Every algorithm and supported pipeline combination must satisfy:

```text
decode(encode(input)) == input
```

Test at least:

- empty input;
- every one-byte value;
- all byte values in sequence;
- repeated single-byte data;
- repeated multi-byte patterns;
- random data;
- already compressed-looking data;
- long zero runs;
- lengths immediately below, equal to, and above every block/window/table boundary;
- final partial blocks;
- dictionary-full and reset cases;
- one-symbol Huffman/ANS cases.

### 14.2 Chunk-boundary tests

For each test vector, vary:

- every input split for small inputs;
- output capacities from 1 byte upward;
- random input and output chunk sizes;
- `Flush` and `ResetBlock` at permitted locations;
- `EndInput` with zero final bytes and with non-empty final input.

The result must not depend on chunking except where an explicit boundary operation is specified to alter framing.

### 14.3 Determinism tests

The same input and configuration must produce byte-for-byte identical output across runs and supported architectures.

### 14.4 Malformed-stream tests

Create independent negative tests for:

- truncated headers and payloads;
- non-zero padding;
- unknown IDs and versions;
- oversized lengths;
- integer-overflow lengths;
- invalid LZ references;
- invalid Huffman tables and codes;
- invalid Adaptive Huffman transitions where detectable;
- invalid range intervals;
- invalid rANS/tANS states and tables;
- checksum/hash mismatch;
- extra trailing bytes under strict mode.

Use fuzzing for every decoder and bit reader. A fuzzer finding a crash or hang requires a permanent regression test.

### 14.5 Hash tests

Verify that HashTap:

- hashes only committed bytes;
- never hashes bytes twice after partial writes;
- gives the same digest for every chunking pattern;
- resets exactly at configured boundaries;
- detects altered payload, header, and raw output according to its declared target;
- handles empty input correctly.

### 14.6 Interoperability tests

When implementing an existing external standard, use lawful, independently sourced conformance vectors and document their provenance.

Do not import GPL/LGPL test suites merely because they are convenient.

---

## 15. Documentation and implementation sequence

For each algorithm, work in this order:

1. Write or update `docs/implementation/references.md`.
2. Define exact terminology and variant in
   `docs/implementation/design-decisions.md`.
3. Define the complete bitstream representation in `docs/format.md`.
4. Add hand-checkable test vectors.
5. Implement the decoder-side validator and bounded parsing structures.
6. Implement a clear reference encoder and decoder.
7. Add round-trip, split-buffer, malformed-input, and fuzz tests.
8. Add benchmarks.
9. Optimize only with tests proving identical behavior.
10. Update `docs/implementation/clean-room-record.md`.

Recommended implementation order:

```text
Core buffers and status model
-> little-endian serialization
-> LSB-first BitWriter/BitReader
-> framing and limits
-> HashTap
-> canonical Huffman primitives (internal only)
-> Blocked Huffman
-> Adaptive Huffman (FGK)
-> Dynamic Range Coder
-> rANS
-> tANS
-> LZ77
-> LZSS
-> LZ78
-> LZW
-> LZD
-> LZMW
-> pipeline combinations and benchmarks
```

The order may change to fit an existing repository, but foundational stream, bit, framing, validation, and test infrastructure must precede complex codecs.

---

## 16. Completion criteria

A public codec algorithm is not complete until all of the following are true. Internal Huffman primitives must satisfy the applicable documentation, determinism, bounds, validation, and test requirements even though they are not exposed as a standalone codec:

- encoder and decoder exist;
- exact format documentation exists;
- empty and boundary cases pass;
- arbitrary chunking passes;
- deterministic output is proven by tests;
- malformed streams fail safely;
- memory limits are enforced;
- hashes at relevant boundaries remain stable across chunking;
- no native-struct serialization is used;
- little-endian and LSB-first rules are followed;
- clean-room provenance is recorded;
- public APIs and errors are documented;
- benchmarks report compression ratio, speed, and peak memory.

Do not mark work complete solely because a one-shot round-trip test succeeds.
