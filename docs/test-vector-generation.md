# Test-vector generation

The version 1 stream and frame prefixes are assigned, but complete codec
payload vectors are added independently before each encoder is implemented.

Generated vectors must record the generator version, complete configuration,
input bytes, expected output bytes, and whether the vector is normative or only
a regression fixture. Random fixtures must record their deterministic seed and
generator algorithm.

Blocked Huffman vectors must record the 256 frequencies, maximum code length,
resulting lengths, canonical codes, LSB-first reversed codes, payload bit
count, raw-size comparison, descriptor bytes, model bytes, payload bytes, and
final valid-bit count.

The initial vector set must cover:

- the internal empty model;
- one distinct symbol;
- equal-frequency tie breaking;
- an input requiring length limiting;
- both raw and Huffman representation selection;
- a final partial payload byte; and
- a final short entropy block.

Negative vectors must independently cover an out-of-range length,
oversubscribed and incomplete tables, nonzero final padding, contradictory
sizes, and truncated model and payload regions.

The initial hand-checkable Package-Merge vector uses frequencies
`5, 7, 10, 15, 20, 45` for symbols `0..5`. With maximum length 15, the expected
lengths are `4, 4, 3, 3, 3, 1`; all other symbols have length zero. Three
equal-weight symbols `0, 1, 2` with maximum length 2 produce lengths `2, 2, 1`
under the deterministic package ordering.

Reference block-encoder vectors:

- Four `41` bytes select raw and produce the descriptor and payload already
  shown in `format.md`.
- Three hundred `41` bytes produce a model with length 1 only at symbol `41`,
  a 38-byte all-zero payload, and 4 valid bits in the final byte. The stored
  Huffman body is 294 bytes and therefore beats the 300-byte raw body.
- 512 bytes alternating `41 42` produce length 1 for both symbols and a
  64-byte payload containing only `AA`; the final byte has 8 valid bits.
- 293 alternating two-symbol bytes select raw because the 256-byte model plus
  37-byte Huffman payload ties the raw size; ties are raw.

The initial complete-stream composition vector contains 604 input bytes:
300 `41` bytes, bytes `01 02 03 04`, then 300 `42` bytes. With frame size 304
and entropy block size 300, it serializes as the 64-byte stream header, a
386-byte first frame, and a 366-byte second frame, for 816 bytes total. This
records region composition and boundaries; individual header and block vectors
remain the source of byte-level field values.

The pure-C ABI regression reuses a 200-byte `5A` input with a 64-byte frame and
32-byte entropy block. It first records the representation produced with large
input/output spans, then requires one-byte input and one-byte output calls to
produce byte-for-byte identical encoded data and decoded output. The driver
re-presents unconsumed suffixes, applies `EndInput` to the final suffix until it
is accepted, rejects progress without progress, and verifies repeatable
end-of-stream. Flipping the first magic byte is the initial ABI-level malformed
stream vector and must produce no decoded output.

Adaptive Huffman FGK vectors record, after every symbol, the emitted NYT or
symbol path, literal bits for a new symbol, node numbers, weights, parents,
children, selected equal-weight leader, swaps, and final packed payload. Initial
hand vectors are `A`, `AA`, `AB`, and `ABA` from `format.md`. Negative vectors
independently cover truncated paths, truncated NYT literals, duplicate symbols
after NYT, contradictory descriptor sizes, zero or excessive final-valid-bit
counts, nonzero padding, trailing bits, invalid node relationships, and frames
larger than the variant's 2^24-byte format limit.

Chunk tests reset the model only at outer frame boundaries. The same frame must
produce identical payload bytes for every input and output split. A two-frame
vector repeats the same first symbol in each frame to prove that the second
frame begins with an empty NYT path and an 8-bit literal rather than retaining
the preceding frame's tree.

The initial reset vector is input `AAAA` with frame size 2. Each frame encodes
`AA` independently and therefore has payload `41 01`, descriptor size 16, and
serialized frame size 74. Including the 64-byte stream header, total stream
size is 212 bytes. Corrupting high padding in the second payload must identify
frame index 1 while leaving the whole strict-reference output untouched.

Dynamic Range Coder vectors are generated from the equations in `format.md`,
using arbitrary-precision scratch arithmetic only to audit the declared 32- and
64-bit bounds. Record `low`, `range`, cumulative frequency, symbol frequency,
total, cached byte, pending count, carry, and each emitted byte after every
symbol. The initial hand vectors are `A`, `AA`, `AB`, and `ABA`.

Negative vectors independently cover payloads shorter than five bytes,
nonzero initial bytes, truncated normalization, `scaled >= total`,
contradictory descriptor sizes,
nonzero flags or reserved bytes, trailing payload, and frames beyond 2^24
symbols. Boundary vectors cross total 32768 and record every post-rescale
frequency and the recomputed total. Multi-frame tests repeat the same input to
prove complete model and coder reset at the outer frame boundary.

The initial Dynamic Range reset vector is input `AAAA` with frame size 2. Both
frames independently encode `AA` as `00 41 40 BE FF 7E`; each serialized frame
is 78 bytes and the complete stream is 220 bytes including its 64-byte header.
Changing the second frame's initial payload byte to nonzero must report frame
index 1 while leaving whole-stream reference output untouched.

rANS normalization vectors record source counts, initial clamped frequencies,
signed errors, every selected adjustment symbol, final cumulative frequencies,
and the exact sum 4096. State vectors record reverse symbol order, `x_max`, every
prepended renormalization byte, post-symbol state, serialized final state, and
forward decoder states. Initial hand vectors are `A`, `AA`, `AB`, and `ABA`.

Negative vectors independently cover zero or incorrect frequency sum, frequency
for an impossible empty model, incorrect table log, nonzero flags or reserved
bytes, contradictory sizes, state below `L` or at least `L*256`, unmapped slots,
truncated and trailing renormalization bytes, non-`L` terminal state, descriptor-
size multiplication overflow, and blocks crossing declared frame boundaries.

The initial rANS reset stream is `AAAA` with frame size 2 and entropy block size
2. Each frame independently models `AA` as a single symbol and has the identical
eight-byte payload `00 00 00 80 00 00 00 00`. Each frame is 592 bytes and the
complete stream is 1248 bytes including its 64-byte stream header. Corrupting
the second frame's state must report frame index 1 while leaving strict-reference
output untouched.

The streaming encoder uses the complete known-size stream as its independent
oracle. Feed the seven-byte `ABAAABA` vector through one-byte input and output
buffers with frame size 4 and block size 2; the resulting bytes must exactly
match the reference stream. A flush after `AB` must emit only the stream header
and must not shorten the first frame.

For streaming decode, feed that reference stream through one-byte input and
output buffers. Corrupt the initial state of the second frame and verify that
the first four raw bytes are committed while no byte from the corrupt frame is
published.

tANS hand vectors are generated independently from DD-056 by filling the entire
4096-slot spread permutation, scanning numeric state positions to assign each
symbol's reduced states, and then applying the inverse lookup to `A`, `AA`,
`AB`, and `ABA`. Record the final state offset, decoder-order bit chunks,
LSB-first packed bytes, valid-bit count, terminal state, and exact bit
consumption. Single-symbol vectors intentionally produce no bit bytes.

The initial tANS reset stream is `AAAA` with frame size 2 and entropy block size
2. Each frame independently models `AA` as one symbol and has the identical
two-byte state-offset payload `00 00`. Each frame is 586 bytes and the complete
stream is 1236 bytes including its 64-byte stream header. Corrupting the second
frame's state offset reports frame index 1 while leaving strict-reference output
untouched.

LZ77 vectors record, at every raw position, all candidate distances, bounded
match lengths, the chosen longest length, nearest-distance tie break, selected
token tag, and the exact 16-byte serialization. Include empty input, every
single byte, distance-1 overlap (`AAAA`), terminal match (`ABABA`),
match-then-literal (`ABCABCX`), equal-length distance ties, window boundaries,
maximum-length boundaries, final unmatched bytes, and frame resets.

Negative vectors independently cover unknown tags, nonzero reserved and unused
fields, truncated tokens, zero or excessive distance, reference before history,
length below minimum or above configured/local maximum, output overflow,
misplaced TerminalMatch, trailing tokens, contradictory dictionary serialized
size, and references that would cross an outer frame.

LZSS vectors independently enumerate every candidate distance at each raw
position, including overlapping candidates, then record bounded length,
serialized Match cost, equivalent Literal cost, selected longest length,
nearest-distance tie break, and exact variable-size bytes. Include empty input,
every single byte, the strict cost boundary at lengths 4 and 5, distance-1
overlap (`AAAAAA`), a frame-ending Match (`ABCABCABC`), a Match followed by a
Literal (`ABCABCABCX`), equal-length distance ties, window and maximum-length
boundaries, and frame resets.

Negative LZSS vectors cover unknown tags, truncated Literal and Match fields,
zero or excessive distance, reference before produced history, length below
the configured minimum or above configured/local maximum, checked output
overflow, a token crossing the declared raw frame size, premature serialized
end, trailing tokens after raw completion, contradictory dictionary serialized
size, and references that would cross an outer frame.

The initial known-size LZSS reset stream is twelve `A` bytes with frame size 6.
Each frame independently emits one Literal followed by a distance-1, length-5
Match, so each payload is the same 11 bytes. Each frame is 67 bytes and the
complete stream is 214 bytes including its 80-byte header-and-parameter prefix.
Changing the second frame's Match distance to 2 must report frame index 1 while
leaving strict-reference output untouched.

For streaming LZSS decode, feed that reference stream through one-byte input and
output buffers. Corrupt the second frame's Match distance and verify that the
first six raw bytes are committed while no byte from the corrupt frame is
published. Exercise short encoded and decoded frame workspaces independently.

Use the same complete known-size LZSS stream as the streaming encoder oracle.
Feed the twelve raw bytes through one-byte input and output buffers; the output
must match byte for byte. A Flush after three bytes emits only the 80-byte
prefix and does not shorten the first six-byte frame.

The initial LZSS fuzz regressions truncate the complete 214-byte stream at every
byte boundary and require strict decode failure with untouched output. A second
fixture fills the first frame's raw and payload length fields with `FF` bytes and
requires bounded header rejection. New minimized findings must become permanent
regressions and retained corpus entries before the defect is considered fixed.

LZ78 vectors independently record the longest known phrase at each raw position,
its fixed 32-bit index, the following byte when present, the next consecutive
dictionary index, and the exact eight-byte Pair or FinalIndex serialization.
Include empty input, every one-byte value, final existing phrases (`AA`, `ABA`),
pair-at-frame-end (`ABAB`), binary zero symbols, maximum-entry boundaries,
dictionary freeze, phrase-length boundaries, and outer-frame resets.

Negative LZ78 vectors cover unknown tags, nonzero reserved or unused fields,
truncated tokens, forward phrase references, FinalIndex zero, misplaced
FinalIndex, checked phrase-length overflow, a phrase crossing the declared raw
frame size, premature serialized end, trailing tokens after raw completion,
non-multiple-of-eight dictionary size, excessive entry parameters, and phrase
references crossing an outer frame reset.

For streaming LZ78 decode, feed the nested `AABABCABC` vector through one-byte
input and output buffers. Repeat with EndInput first observed while a phrase is
still draining, and with EndInput supplied on a later zero-byte call. Verify
dictionary freeze with maximum entry count one. Truncated final tokens, trailing
bytes, and a forward reference must report stable malformed-stream errors while
preserving only bytes committed by earlier valid tokens.

Use the same nested `AABABCABC` vector as the streaming LZ78 encoder oracle.
Feed raw bytes and drain canonical tokens one byte at a time; output must equal
the reference encoder byte for byte. A Flush after four raw bytes must emit
nothing and must not shorten the frame. Exercise terminal input while draining,
dictionary freeze, short raw/token/dictionary workspaces, and the aggregate
workspace limit independently.

The canonical LZ78/None frame for raw `A` is 64 bytes: the generic 56-byte frame
header declares raw size 1 and equal dictionary/payload sizes of 8, followed by
Pair `(0, 'A')`. The nested `AABABCABC` frame has four tokens and an 88-byte
total extent. Negative frame vectors alter a later phrase index, truncate or
extend the payload, corrupt the frame sequence, and shorten each typed phrase
workspace independently; malformed decode must leave raw output untouched.

The initial known-size LZ78 reset stream is six `A` bytes with frame size 3.
Each frame independently emits Pair `(0, 'A')` followed by Pair `(1, 'A')`, so
both have the same 16-byte payload and 72-byte total extent. Including the
80-byte stream prefix, the complete stream is 224 bytes. Corrupt the second
frame's second phrase index and require frame index 1 while raw output and
caller-visible parsed metadata remain untouched.

Feed that 224-byte reset stream to the outer streaming decoder with one-byte
input and output buffers. Corrupt the second frame's second phrase reference
and require the first three raw bytes to remain committed while the failing
frame publishes none. Exercise the encoded-frame, decoded-frame, and phrase
table workspaces independently, then set the aggregate workspace limit one byte
below the required 72 encoded bytes, three decoded bytes, and two phrase
records.

Also consume the complete stream without EndInput, drain all six bytes, and
require a later empty EndInput call to make the terminal transition. ResetBlock
is unsupported by this outer decoder and must fail without consuming input.

Use the same 224-byte two-frame reset stream as the outer streaming encoder
oracle. Supply and drain one byte at a time, then repeat with Flush after two
raw bytes and require the identical stream. Exercise short raw-frame, encoded-
frame, and encoder phrase-table workspaces, plus an aggregate limit one byte
below three raw bytes, a 72-byte complete frame, and three phrase records.
Verify premature EndInput, trailing input, empty input, delayed EndInput, and
unsupported ResetBlock independently.

For the canonical LZ78 profile with original size 7 and frame size 4, require
four raw-frame bytes, an 88-byte worst-case complete frame, and four encoder
phrase records. With maximum entries 2, only the phrase-record count shrinks.
For decoder limits of 200 aggregate bytes and otherwise-loose payload and
dictionary bounds, require the largest payload `P` satisfying
`56 + P + 1 + floor(P / 8) * sizeof(Lz78PhraseEntry) <= 200`; the next byte must
fail the same inequality. On the current MSVC x64 ABI a phrase record occupies
16 bytes, so the boundary is 47 payload bytes and five phrase records; the
sixth record introduced at 48 payload bytes exceeds the limit.

For the LZ78 C ABI, encode six `A` bytes in two three-byte frames and require
the canonical 224-byte stream, then decode it through a separately initialized
handle and compare all six bytes. The encoder query reports three raw bytes, an
80-byte worst-case complete frame, and a nonempty aligned opaque phrase region.
Reject a deliberately misaligned phrase region, nonzero reserved configuration,
and a zero local dictionary-entry limit.

Run the existing CLI overwrite, malformed-input, empty-input, and file
round-trip script with explicit codec `lz78`. The generated stream must decode
back to the exact repeated-text fixture and a rejected malformed stream must
leave neither destination nor temporary file. Run one LZ78 benchmark smoke
iteration over `README.md`; timing begins only around transform processing and
the untimed preflight round trip must succeed first.

For permanent LZ78 fuzz regressions, truncate the canonical 224-byte reset
stream at every earlier byte and require one-shot decode to leave all raw output
untouched. Mutate the first token's tag, reserved bytes, and root reference;
replace the first frame's three size fields with all ones; and reference phrase
1 from the first token of the reset second frame. Every case must fail without
publishing caller-visible stream metadata or raw bytes.

LZW variant 1 vectors record the longest current string, emitted numeric code,
new prefix-plus-byte entry, next-free code, encoder and decoder width before
each code, and exact LSB-first packed bytes. Begin with `A`, `AA`, `AB`, and
`AAA`, then `ABABABA`; `AAA` is the smallest `KwKwK` case. Add binary zero and
every one-byte input, the transitions immediately before/at/after codes 512 and
1024, the maximum-width boundary, dictionary freeze, final partial bytes, and
outer-frame reset. Independently regenerate packed bytes from the listed
numeric codes rather than using an external LZW encoder.

The exact first width-boundary vector is 288 zero bytes followed by
`00 00 08`. It represents 256 literal-zero codes at 9 bits, one literal-zero
code at 10 bits, and `code 512` at 10 bits. The last code is `KwKwK`, so the
declared output is 259 zero bytes. Require 258 codes, 257 new dictionary
entries, 2324 logical code bits, and four zero padding bits. A decoder that
changes width one code late observes the `08` data bit as nonzero padding.

For atomic LZW reference decoding, decode the documented `A`, `AA`, `AAA`, and
`ABABABA` payloads, including both `KwKwK` cases, then decode the 291-byte width-
boundary vector into exactly 259 zero bytes. Decode all 256 one-byte values from
their literal nine-bit representation. With maximum width 9, decode 258 literal-
zero codes from 291 zero payload bytes after the 256-entry dictionary freezes.
A forward code, nonzero padding, short workspace, local-limit failure, and short
output must not publish any raw byte.

For deterministic LZW reference encoding, require exact bytes for empty input,
`A`, `AA`, `AAA`, and `ABABABA`, plus the canonical literal representation of
all 256 one-byte values. The 2048-byte width fixture defines byte `i` as
`(i * 37 + floor(i / 7)) mod 256`. With maximum width 16 it produces 969 codes,
9635 bits, 968 entries, and 1205 bytes: 256 codes use 9 bits, 512 use 10 bits,
and 201 use 11 bits. Independent planning and repeated serialization must be
identical and decode to the fixture.

With the same fixture and maximum width 9, require 1255 codes, 11295 bits, 256
entries, and 1412 bytes, followed by an exact round trip after dictionary
freeze. Short output, serialized-size policy, workspace, parameter, input, and
workspace-limit failures must occur before any encoded byte is published.

For LZW streaming decode, feed `ABABABA` through one-byte input and output
buffers, retain EndInput while a phrase drains, and also supply EndInput on a
later zero-byte call. Feed the 2048-byte generated width fixture one encoded
byte at a time with three-byte output capacity, and require the exact raw
fixture. Repeat the complete fixture at maximum width 9 after dictionary
freeze. Truncation, nonzero padding, trailing bytes, and a forward code may
preserve only raw bytes committed by earlier accepted codes. Exercise empty and
ended calls, short dictionary workspace, invalid parameters and limits,
cumulative serialized limits, and unsupported ResetBlock independently.

Use the same canonical `ABABABA` bytes as the LZW streaming encoder oracle.
Feed and drain one byte at a time, drain a complete frame before a later empty
EndInput, and retain EndInput while output drains. Flush after three raw bytes
must emit nothing and must not shorten the frame. Require byte identity for the
2048-byte width fixture at both the default maximum and the 9-bit freeze
profile. Exercise premature and trailing input, short raw, encoded, and phrase
workspaces, an aggregate limit one byte below the exact requirement, empty and
ended calls, and unsupported ResetBlock.

For the LZW plus entropy None frame adapter, require the documented 58-byte
single-`A` frame byte for byte. Plan, encode, validate, and decode `ABABABA`,
requiring four codes, five payload bytes, and exact raw recovery. Exercise a
short contextual final frame after prior committed output. Verify that short
output, insufficient encoder and decoder phrase workspaces, a non-literal
first code, a truncated frame, trailing bytes, an unexpected sequence, an
empty raw frame, and an unsupported entropy selection fail without publishing
raw output.

For the one-shot LZW stream adapter, split six `A` bytes into two three-byte
frames. Require an 80-byte prefix, two identical 59-byte reset frames, a
198-byte total stream, and exact round trip. Empty input must contain only the
prefix. Corrupt the first code of the second frame and require that neither raw
output nor parsed configuration is published. Exercise short encoded and raw
output, truncation, trailing bytes, invalid parameters, insufficient encoder
and decoder phrase workspaces, and declared input-size mismatch.

Feed the same 198-byte two-frame LZW stream to the outer streaming decoder one
input byte at a time with one-byte output capacity, and require exact recovery
plus stable ended calls. Corrupt the first code in the second frame and require
the first frame's three bytes to remain committed while no second-frame byte is
published. Exercise short serialized-frame, decoded-frame, and phrase storage,
an aggregate buffered limit one byte below the exact requirement, truncation,
empty streams, trailing bytes, a later empty EndInput, and ResetBlock.

Use the same 198-byte two-frame LZW stream as the outer streaming encoder
oracle. Supply raw input and drain encoded output one byte at a time, requiring
exact one-shot byte identity and stable ended calls. Flush after two of three
frame bytes must emit only the 80-byte prefix and leave the partial frame open.
Exercise short raw-frame, serialized-frame, and encoder phrase storage, an
aggregate buffered limit one byte below the exact requirement, premature
EndInput, trailing raw input, empty streams, a later empty EndInput, and
ResetBlock.

For LZW profile calculation, require a four-byte default-width frame to reserve
eight payload bytes, a 64-byte serialized frame, and three encoder phrase
records. At maximum width 9, require a 300-byte frame to reserve 338 payload
bytes and freeze at 256 records. Empty input reserves only the frame header.
Exercise invalid widths and an aggregate encoder limit one byte short. For
decoder sizing, verify the largest discrete code width permitted by the local
dictionary-entry limit, then independently enumerate the payload boundary
under a tight aggregate limit and prove that the next byte does not fit.

For the LZW C ABI, encode six `A` bytes as two three-byte frames using caller
workspaces and require the canonical 198-byte stream. Query decoder workspace
from restricted local limits, decode through the opaque transform, and require
exact recovery. Verify default maximum width 16, public structure metadata,
nonzero aligned phrase workspace, misalignment rejection, reserved-field and
zero-limit rejection, and an invalid encoder code width.

For CLI integration, run the generic nonempty, overwrite-rejection, malformed,
and empty-file round-trip script with explicit codec `lzw`; successful output
must reproduce the source and failed decode must leave no destination or
temporary file. Run one LZW benchmark smoke iteration over README input and
require its internal C-ABI round-trip verification to succeed.

For permanent LZW fuzz regressions, truncate the canonical 198-byte reset
stream at every byte and require atomic one-shot rejection. Independently
mutate the first code to 256, set nonzero final padding, saturate frame extent
fields, and use code 256 as the first code after a frame reset. The bounded
fuzz harness applies arbitrary bytes to strict and streaming decode with 4 KiB
total output, 1 KiB frames, 4 KiB payloads, 768 phrase records, input-derived
chunks, and a finite call guard.

The LZW completion matrix uses frame size 64 and checks lengths 63, 64, and 65;
empty input; every one-byte value; all byte values in sequence; 257 zero bytes;
a 259-byte repeating `00 FF 55 AA` pattern; and 513 deterministic pseudo-random
bytes modeling already-compressed data and generated by
`state = state * 1664525 + 1013904223`, taking the high byte. Encode every
vector twice and require identical complete streams before round-trip decode.
For a separate 193-byte four-frame vector seeded with `0x6D617263`, require the
outer streaming encoder to match the one-shot bytes and the streaming decoder
to reproduce the input with chunk-capacity pairs `(1,1)`, `(7,5)`, and
`(13,17)`.

Negative LZW vectors cover a non-literal first code, a code above next-free,
`code == next_free` after freeze, premature code bits, a phrase crossing the
declared raw size, checked phrase-length overflow, excess payload bytes, and
every nonzero final-padding position. Decoder tests must distinguish the exact
width-boundary schedule from both adjacent off-by-one schedules. Strict
reference failure leaves raw output and caller-visible parsed metadata
untouched.

LZD variant 1 vectors are derived by maintaining the implicit literal
references `0..255` and assigning generated phrases from reference 256. At
each position, record the longest current dictionary match, then repeat at the
position immediately after it. Use absent right only when the first match
reaches the exact frame end. This gives `A -> (A,absent)`,
`AB -> (A,B)`, `ABA -> (A,B)(A,absent)`,
`ABAB -> (A,B)(256,absent)`, and
`ABABAB -> (A,B)(256,256)`. Independently expand every pair through only prior
entries and verify the raw extent rather than consulting an LZD encoder.

The published illustrative input without its theoretical sentinel,
`abbaababaaba`, must parse as `ab | ba | abab | aab | a`, represented by
`(a,b), (b,a), (256,256), (a,256), (a,absent)`. The one-byte `A` frame is the
generic 56-byte frame header followed by `41 00 00 00 FF FF FF FF`.

Negative LZD validator vectors cover every incomplete-token remainder, absent
left, a phrase reference one beyond the current dictionary, absent right before
the final token or before exact raw completion, a pair crossing raw extent,
tokens after raw completion, premature token-stream end, short phrase
workspace, serialized/frame/aggregate local limits, and dictionary freeze.
For checked length overflow, begin with `(A,A)`, then emit 63 tokens whose two
components both reference the phrase inserted immediately before. Token 63
attempts to add two lengths of `2^63` and must fail before insertion.

LZD decoder tests expand the hand vectors and the published illustrative
factorization above without consulting an encoder. An iterative-depth vector
uses `(A,A)`, `(256,B)`, `(257,C)`, and `(258,258)`, whose four token outputs
concatenate to `AAAABAABCAABC`; it exercises nested left references and the
right-before-left stack rule. A frozen-dictionary vector verifies that later
tokens still reference the last stored phrase. Invalid input, short output,
short phrase workspace, short expansion workspace, and aggregate memory-limit
failures each begin with a nonzero output pattern and verify atomic rejection.

LZD encoder tests generate every hand-checkable token vector directly from raw
input and require the published `abbaababaaba` factorization byte for byte.
Encoding `ABABABABAB` with one phrase slot must retain reference 256 after
freeze and emit `(A,B), (256,256), (256,256)`. The deterministic binary vector
contains all byte values twice; a separate 1,025-byte fixed-LCG vector checks
the `8 * ceil(raw_size / 2)` serialized bound. Both are encoded repeatedly and
decoded through the independent strict decoder. Short output, short workspace,
invalid parameters, serialized limits, frame limits, and aggregate raw-plus-
workspace limits remain ordinary atomic negative tests.

LZD streaming decode feeds the published-example token stream through one-byte
input and output spans, with `EndInput` attached only to the final encoded byte.
A second vector supplies all encoded bytes with one output byte, then drains
without repeating `EndInput`. Separate cases finish with a zero-byte EndInput,
decode an empty frame, reject a token truncated at byte offset 8 without
changing patterned output, and reject a ninth byte for a one-byte raw frame
before consuming it. Flush while collecting does not close the frame.
Construction tests independently cover each of the four short workspaces,
their aggregate limit, unsupported host extents, and unsupported reset.
Repeated calls after a malformed frame return the same terminal error.

LZD streaming encode feeds `abbaababaaba` through one-byte raw and token spans
and compares every byte with the one-shot reference encoder. Separate vectors
drain a full frame before a zero-byte EndInput, retain EndInput while a token
region drains, leave a partial frame open across Flush, and preserve the
one-entry frozen dictionary for `ABABABABAB`. Workspace calculations check
empty, one-byte, frozen, and unsupported extents through the shared format
bound. Premature input end, trailing raw bytes, each of the three short caller
workspaces, aggregate limits, empty input, and unsupported reset remain stable
negative or terminal cases.

The LZD None profile uses a ten-byte stream with four-byte frames to verify a
largest-frame encoder requirement of 4 raw bytes, 16 maximum token bytes, a
56-byte frame header, and two phrase records. Empty and final-short-frame cases
exercise zero and odd raw sizes. Decoder workspace tests first allow a 1,024-
byte payload capped at ten phrase records, then enumerate the last payload byte
that fits a 300-byte aggregate limit and compare the binary-search result.
Invalid parameters, token limits, encoder aggregate limits, invalid local
limits, unsupported host extents, and an aggregate too small for even an empty
payload are stable negative cases.

LZD frame tests wrap the published `abbaababaaba` factorization in a generic
header and require five tokens. The documented one-byte `A` frame is compared
against all 64 literal bytes from `docs/format.md`. A contextual final frame
uses sequence 1 at committed raw offset 8. Short encoder and decoder output,
forward phrase references, short phrase and expansion workspaces, truncation,
trailing bytes, sequence corruption, unsupported pipelines, and missing
encoder workspace are atomic or stable negative cases. Separate thresholds
prove that encoder planning, validation, and decoding each count the generic
header and their complete caller-owned workspaces in the aggregate limit.

The complete LZD plus None stream vector encodes `ABAB` with raw frame size 2.
It is exactly 208 bytes: an 80-byte prefix followed by two 64-byte frames at
offsets 80 and 144. Each independently reset frame emits the identical `(A,B)`
token. Empty input is the 80-byte prefix alone. Corrupting the second frame is
used to prove that one-shot decode publishes neither the first frame nor parsed
configuration; truncation, trailing bytes, invalid parameters, short output,
and missing phrase or expansion workspaces are stable negative cases. A second
two-frame vector gives an expansion stack that is sufficient for the first
repetitive frame but one entry short for the later incompressible frame; decode
must reject it before publishing any first-frame bytes. The same vector sets an
aggregate limit that admits the smaller first frame but not the larger second
frame and requires the same atomic rejection.

The LZD outer streaming decoder uses the documented 208-byte `ABAB` stream as
its one-byte input/output oracle. It must match the one-shot raw bytes while
retaining EndInput across final-frame draining. Corrupting the second payload
must still publish the complete first frame but none of the corrupt frame.
Independent construction cases cover short encoded-frame storage, raw staging,
phrase records, expansion stack, and their aggregate limit. Truncation, empty
streams, trailing bytes, a later empty EndInput, and ResetBlock are stable state
machine cases. Flush across a partial frame must not close it; invalid
construction and unknown flags enter a reproducible terminal error.

The LZD outer streaming encoder feeds raw `ABAB` through one-byte input and
output spans and requires byte-for-byte equality with the documented 208-byte
one-shot stream. Flush after one raw byte may drain only the 80-byte prefix and
must leave the partial two-byte frame open. Separate cases cover raw-frame,
serialized-frame, and encoder-entry storage; their complete aggregate limit;
premature EndInput; trailing raw bytes; empty input; a later empty EndInput;
ResetBlock; unknown flags; invalid construction; and reproducible terminal
errors.

The bounded LZD fuzz harness invokes both one-shot and outer streaming decoders
with 4 KiB total output, 1 KiB raw frames, 4 KiB payloads, 512 phrase records,
and 513 expansion entries. Input-derived chunk sizes remain subordinate to a
finite call guard. Permanent GoogleTest cases reject every truncation of the
208-byte canonical stream and mutate absent/forward references, token extents,
extreme frame lengths, and a second-frame phrase reference while proving
one-shot raw/configuration atomicity. The initial repository-owned corpus seed
is the five-byte truncated frame magic `MRF1\n`.

For the public LZD integration path, encode and decode `ABAB` through separately
initialized C ABI handles and require the documented 208-byte stream. Query and
honor both direction-specific opaque views regions, reject misalignment and
invalid reserved or entry-limit fields, and run the full completion data-class
matrix through that same public surface. Run the generic CLI overwrite,
malformed cleanup, empty input, and file round-trip script with explicit codec
`lzd`; its repeated-text count is 320 to keep this clear reference encoder
smoke bounded. Run one C-ABI-only LZD benchmark iteration
over `README.md` after its untimed round-trip preflight succeeds.

Use the complete known-size tANS stream as the streaming encoder oracle. Feed
`ABAAABA` through one-byte input and output buffers with frame size 4 and block
size 2; output must match byte for byte. A flush after `AB` emits only the stream
header and does not shorten the first frame.

For streaming decode, feed that reference stream through one-byte input and
output buffers. Corrupt the second frame's initial tANS state offset and verify
that the first four raw bytes are committed while no byte from the corrupt frame
is published. Exercise short encoded, decoded, and block-view workspaces
independently.

LZMW variant 1 vectors start from the formal `abbaababaaba$` factorization but
remove the external delimiter because marc terminates at the declared frame
size. Record `a | b | b | a | ab | ab | aab | a` and the eight references
`97, 98, 98, 97, 256, 256, 259, 97`. Independently derive entry 256 as `ab`,
257 as `bb`, 258 as `ba`, and 259 as `aab` from consecutive emitted phrases.
Also test empty input, `A`, `ABAB`, and maximum-entries-1 `ABABAB` before any
encoder is implemented.

The validator must reject every one-to-three-byte truncated token suffix,
first-token and later forward references, exact-output trailing tokens,
premature token end, phrase-length and output-length overflow, short phrase
workspace, invalid parameters, and each local serialized/frame/aggregate
limit. Check stable token index and byte offset. The workspace requirement is
`min(max(token_count - 1, 0), maximum_entries)` phrase records because the first
token does not create an adjacent-pair entry.

Reference decode uses the same empty, `A`, `ABAB`, frozen `ABABAB`, and
published `abbaababaaba` token vectors. Add the phrase-reference sequence
`A, A, 256, 257, 258`, whose phrase lengths are `1, 1, 2, 3, 5`, to exercise
iterative expansion of the growing binary grammar. Require the complete raw
concatenation `AAAAAAAAAAAA` (12 `A` bytes) and a five-entry expansion stack
for its four generated entries. Corrupt a later reference, shorten raw output,
shorten the expansion stack, omit phrase records, and lower the full token plus
phrase plus stack aggregate by one byte; every failure leaves patterned output
untouched.

The reference encoder must reproduce each preceding LZMW vector byte for byte,
including the eight-reference published factorization and the frozen one-entry
dictionary sequence. Test every possible one-byte input, repeated planning and
encoding, all 256 byte values followed by a second copy, and deterministic
pseudorandom binary input. Decode each nontrivial generated stream through the
independent validator-first decoder. Short output, short phrase-span workspace,
serialized-size rejection, invalid parameters, frame-size rejection, and the
full input-plus-workspace aggregate limit must fail before any patterned output
byte is changed.

For streaming decode, generate the fixed reference stream with the independent
reference encoder and feed it through one-byte input and output spans. Exercise
`EndInput` both with the final token bytes and in a later empty call, drain a
nonempty frame without repeating the flag, and accept an empty frame. A
truncated final token must publish no raw byte and retain its stable byte
position on repeated error calls. Also reject a call exceeding the conservative
encoded extent before consuming it, every unsupported flag, each independently
short workspace, and an aggregate limit one byte below the complete encoded,
phrase, stack, and staged-output requirement.

For streaming encode, use one-shot LZMW encoding as the byte-for-byte oracle.
Feed `abbaababaaba` through one-byte raw and output spans, let a full `ABAB`
frame drain before a later empty `EndInput`, and preserve `EndInput` while a
larger token stream drains. Verify that `Flush` after a partial frame produces
no token and that maximum-entries-1 preserves the frozen dictionary result.
Test empty input, premature and excess raw input, `ResetBlock`, each
independently short raw/token/dictionary workspace, and a construction
aggregate exactly one byte below the complete requirement. After beginning a
one-byte drain, supply a new raw byte and require rejection without consuming
it or publishing another token byte.

For the LZMW plus None profile, verify a ten-byte stream with four-byte frames,
an empty stream, and a final frame shorter than the configured frame size.
Check exact raw, frame-header-plus-token, phrase-span, phrase-record, and
expansion-stack workspace counts. Reject zero frame size, invalid LZMW
parameters, serialized-token limits, and a complete encoder aggregate one byte
too small. Derive decoder workspace from two independent coupled-limit sets,
including a binary-searched aggregate boundary, and reject an aggregate unable
to hold even the frame header, maximum raw frame, and one expansion reference.

For the one-shot LZMW plus None frame codec, encode raw `A` and compare all 60
bytes with the documented frame vector. Encode, validate, and decode the
published `abbaababaaba` factorization, then exercise a contextual final short
frame. Reject short encoded output atomically, short decoded output, an invalid
phrase reference, short phrase and expansion workspaces, truncated and trailing
frame extents, a wrong frame sequence, a non-None pipeline, and short encoder,
validator, and full-decoder aggregates.

For the complete LZMW plus None stream, compare raw `ABAB` with frame size 2
against the documented 208-byte, two-reset-frame vector. Verify the exact
80-byte empty stream and a binary final short frame. Corrupt only the later
frame and require that output and parsed metadata remain unchanged; separately
preflight a later frame's expansion workspace and aggregate before publication.
Reject truncated and trailing streams, invalid parameter bytes, short phrase
and expansion workspaces, input/original-size mismatch, short output, and short
encoder workspace.

For outer frame-streaming decode, feed the documented two-frame LZMW stream
through one-byte input and output spans. Corrupt the second frame's first
reference and require exactly the first frame's two raw bytes to be committed.
Exercise short encoded-frame, decoded-frame, phrase-record, and expansion
workspaces independently, plus the full per-frame aggregate one byte short.
Verify truncated input, empty stream, trailing data, an empty final `EndInput`,
partial-input `Flush`, unknown flags, `ResetBlock`, invalid limits, and stable
terminal states.

For outer frame-streaming encode, use the complete one-shot LZMW stream as the
byte-for-byte oracle. Feed the documented two-frame `ABAB` input through
one-byte input and output spans, keep a partial frame open across `Flush`, and
encode a binary final short frame. Exercise empty input, a full final frame
followed by a later empty `EndInput`, premature and excess input, unsupported
flags, every independently short workspace, and the complete per-frame
aggregate one byte below its required size. Terminal ended and error states
must remain stable.

For the public LZMW C integration path, initialize independent encode and
decode configurations and query every workspace extent rather than reproducing
private C++ layout formulas. Encode the documented two-frame `ABAB` stream and
require 208 bytes, then decode it through a separately created transform and
compare all raw bytes. Test a deliberately misaligned views address, nonzero
reserved configuration data, a zero dictionary limit, and zero maximum entries
through a translation unit compiled strictly as C11.

The LZMW completion matrix uses frame size 64 and validates empty input, all
256 one-byte inputs, all byte values in sequence, repeated zeroes, a repeated
binary pattern, deterministic pseudorandom bytes, and lengths 63, 64, and 65.
Re-encode every case and require exact bytes. For a 193-byte stream, compare
one-shot-equivalent processing with 1/1, 7/5, and 13/17 input/output chunks and
round-trip each result. Run one public-C-ABI benchmark smoke iteration over the
README input; the benchmark must verify the decoded bytes before timing.

The bounded LZMW fuzz harness invokes both one-shot and outer streaming
decoders with 4 KiB total output, 1 KiB raw frames, 4 KiB payloads, 1024 phrase
records, and 1025 expansion entries. Input-derived chunk sizes remain under a
finite call guard. Permanent GoogleTest cases reject every truncation of the
208-byte canonical stream and mutate absent/forward references, token extents,
extreme frame lengths, and a second-frame phrase reference while proving
one-shot raw/configuration atomicity. The repository-owned corpus begins with
the five-byte truncated frame magic `MRF1\n`.

For CLI integration, run the generic file-level round-trip script with explicit
codec `lzmw` and a 320-repeat deterministic text fixture. Require encode and
decode success, byte-for-byte restoration, rejection of an existing output,
rejection of malformed input without either destination or `.tmp` residue, and
an empty-file round trip. The executable must reach LZMW only through the public
C ABI configuration, workspace-query, factory, process, and destroy functions.

For LZ77 plus Blocked Huffman, start with raw `A`. Generate the 16-byte LZ77
Literal token from the independent dictionary vector, then apply the mandatory
Blocked Huffman size rule by hand: a 256-byte model cannot beat 16 raw bytes,
so the single entropy block is raw. Require the exact 88-byte combined frame in
`docs/format.md`. The decoder-side validator test consumes that exact byte
array, checks the staged 16-byte Literal token, rejects every strict prefix and
trailing data, and independently exercises sequence/pipeline mismatch, short
view and staging workspaces, aggregate workspace exhaustion, invalid raw-block
metadata, and an invalid staged LZ77 token. The validator has no raw-output
parameter, so these tests establish non-publication structurally before raw
decode is added.

For combined raw decode, use the same 88-byte `A` frame and require that bytes
beyond its one-byte raw extent remain unchanged. Independently construct a raw
Blocked Huffman frame whose dictionary payload is a Literal `A` followed by a
distance-one, length-four terminal LZ77 match; require output `AAAAA` to exercise
overlap-copy semantics without relying on a combined encoder. A four-byte raw
destination for that five-byte frame and corrupt descriptor/token variants must
leave every raw destination byte unchanged.

For combined encode, require plan and encode of raw `A` to reproduce the exact
documented 88-byte frame. Encode the `AAAAA` overlap vector twice and require
identical 104-byte streams plus decoder round-trip. Encode byte values `0..63`
as 1024 canonical Literal-token bytes with one 1024-symbol entropy block and
require the static canonical Huffman representation rather than raw storage.
Repeat with 300-symbol entropy blocks, require four blocks with a final
124-symbol block, and round-trip the complete frame. Short staging and
serialized destinations remain sentinel-filled; empty and contextually wrong
raw frame extents are rejected.

For the combined complete stream, encode raw `ABABX` with raw frame size 2 and
entropy block size 16. Require three frames of raw extents 2, 2, and 1 and an
exact total of 408 bytes including the 80-byte prefix. Compare the two `AB`
frame bodies to prove both layers reset, then decode the stream and re-encode it
deterministically. Require empty input to produce only the 80-byte prefix.
Reject every strict prefix and trailing data. Corrupt the second frame's first
staged token and require the full raw output plus parsed stream/parameter
outputs to remain unchanged. Exercise serialized-output, raw-output, block-view,
and dictionary-staging capacity failures independently.

For combined streaming encode, use the complete `ABABX` stream as the exact
oracle and feed it through one-byte input and output spans. Keep the first
partial raw frame open across `Flush`, then finish all remaining frames with
`EndInput`. Exercise empty input, stable ended state, premature `EndInput`,
unsupported `ResetBlock`, independently short raw-frame, dictionary-token, and
serialized-frame workspaces, and the actual three-workspace aggregate one byte
below its required 154 bytes.
