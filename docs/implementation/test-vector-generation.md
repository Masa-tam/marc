# Test-vector generation

This development record is indexed from [`README.md`](README.md).

## Generation policy

Generated vectors must record the generator version, complete configuration,
input bytes, expected output bytes, and whether the vector is normative or only
a regression fixture. Random fixtures must record their deterministic seed and
generator algorithm.

Starting 2026-08-14, local full-suite CTest validation uses a 300-second
per-test timeout. This replaces the former 240-second operational ceiling as
the schema-compatibility chain and registered inventory approach 200 seconds;
it is scheduling margin, not permission to ignore a test-time regression.

The version 1 stream and frame prefixes are assigned, but complete codec
payload vectors are added independently before each encoder is implemented.

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

## Vector development ledger

### TVG-0001

The initial hand-checkable Package-Merge vector uses frequencies
`5, 7, 10, 15, 20, 45` for symbols `0..5`. With maximum length 15, the expected
lengths are `4, 4, 3, 3, 3, 1`; all other symbols have length zero. Three
equal-weight symbols `0, 1, 2` with maximum length 2 produce lengths `2, 2, 1`
under the deterministic package ordering.

### TVG-0002

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

### TVG-0003

The initial complete-stream composition vector contains 604 input bytes:
300 `41` bytes, bytes `01 02 03 04`, then 300 `42` bytes. With frame size 304
and entropy block size 300, it serializes as the 64-byte stream header, a
386-byte first frame, and a 366-byte second frame, for 816 bytes total. This
records region composition and boundaries; individual header and block vectors
remain the source of byte-level field values.

### TVG-0004

The pure-C ABI regression reuses a 200-byte `5A` input with a 64-byte frame and
32-byte entropy block. It first records the representation produced with large
input/output spans, then requires one-byte input and one-byte output calls to
produce byte-for-byte identical encoded data and decoded output. The driver
re-presents unconsumed suffixes, applies `EndInput` to the final suffix until it
is accepted, rejects progress without progress, and verifies repeatable
end-of-stream. Flipping the first magic byte is the initial ABI-level malformed
stream vector and must produce no decoded output.

### TVG-0005

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

### TVG-0006

The initial reset vector is input `AAAA` with frame size 2. Each frame encodes
`AA` independently and therefore has payload `41 01`, descriptor size 16, and
serialized frame size 74. Including the 64-byte stream header, total stream
size is 212 bytes. Corrupting high padding in the second payload must identify
frame index 1 while leaving the whole strict-reference output untouched.

### TVG-0007

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

### TVG-0008

The initial Dynamic Range reset vector is input `AAAA` with frame size 2. Both
frames independently encode `AA` as `00 41 40 BE FF 7E`; each serialized frame
is 78 bytes and the complete stream is 220 bytes including its 64-byte header.
Changing the second frame's initial payload byte to nonzero must report frame
index 1 while leaving whole-stream reference output untouched.

### TVG-0009

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

### TVG-0010

The initial rANS reset stream is `AAAA` with frame size 2 and entropy block size
2. Each frame independently models `AA` as a single symbol and has the identical
eight-byte payload `00 00 00 80 00 00 00 00`. Each frame is 592 bytes and the
complete stream is 1248 bytes including its 64-byte stream header. Corrupting
the second frame's state must report frame index 1 while leaving strict-reference
output untouched.

### TVG-0011

The streaming encoder uses the complete known-size stream as its independent
oracle. Feed the seven-byte `ABAAABA` vector through one-byte input and output
buffers with frame size 4 and block size 2; the resulting bytes must exactly
match the reference stream. A flush after `AB` must emit only the stream header
and must not shorten the first frame.

### TVG-0012

For streaming decode, feed that reference stream through one-byte input and
output buffers. Corrupt the initial state of the second frame and verify that
the first four raw bytes are committed while no byte from the corrupt frame is
published.

### TVG-0013

tANS hand vectors are generated independently from DD-056 by filling the entire
4096-slot spread permutation, scanning numeric state positions to assign each
symbol's reduced states, and then applying the inverse lookup to `A`, `AA`,
`AB`, and `ABA`. Record the final state offset, decoder-order bit chunks,
LSB-first packed bytes, valid-bit count, terminal state, and exact bit
consumption. Single-symbol vectors intentionally produce no bit bytes.

### TVG-0014

The initial tANS reset stream is `AAAA` with frame size 2 and entropy block size
2. Each frame independently models `AA` as one symbol and has the identical
two-byte state-offset payload `00 00`. Each frame is 586 bytes and the complete
stream is 1236 bytes including its 64-byte stream header. Corrupting the second
frame's state offset reports frame index 1 while leaving strict-reference output
untouched.

### TVG-0015

Use the complete known-size tANS stream as the streaming encoder oracle. Feed
`ABAAABA` through one-byte input and output buffers with frame size 4 and block
size 2; output must match byte for byte. A flush after `AB` emits only the stream
header and does not shorten the first frame.

### TVG-0016

For streaming decode, feed that reference stream through one-byte input and
output buffers. Corrupt the second frame's initial tANS state offset and verify
that the first four raw bytes are committed while no byte from the corrupt frame
is published. Exercise short encoded, decoded, and block-view workspaces
independently.

### TVG-0017

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

### TVG-0018

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

### TVG-0019

The initial known-size LZSS reset stream is twelve `A` bytes with frame size 6.
Each frame independently emits one Literal followed by a distance-1, length-5
Match, so each payload is the same 11 bytes. Each frame is 67 bytes and the
complete stream is 214 bytes including its 80-byte header-and-parameter prefix.
Changing the second frame's Match distance to 2 must report frame index 1 while
leaving strict-reference output untouched.

### TVG-0020

For streaming LZSS decode, feed that reference stream through one-byte input and
output buffers. Corrupt the second frame's Match distance and verify that the
first six raw bytes are committed while no byte from the corrupt frame is
published. Exercise short encoded and decoded frame workspaces independently.

### TVG-0021

Use the same complete known-size LZSS stream as the streaming encoder oracle.
Feed the twelve raw bytes through one-byte input and output buffers; the output
must match byte for byte. A Flush after three bytes emits only the 80-byte
prefix and does not shorten the first six-byte frame.

### TVG-0022

The initial LZSS fuzz regressions truncate the complete 214-byte stream at every
byte boundary and require strict decode failure with untouched output. A second
fixture fills the first frame's raw and payload length fields with `FF` bytes and
requires bounded header rejection. New minimized findings must become permanent
regressions and retained corpus entries before the defect is considered fixed.

### TVG-0023

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

### TVG-0024

For streaming LZ78 decode, feed the nested `AABABCABC` vector through one-byte
input and output buffers. Repeat with EndInput first observed while a phrase is
still draining, and with EndInput supplied on a later zero-byte call. Verify
dictionary freeze with maximum entry count one. Truncated final tokens, trailing
bytes, and a forward reference must report stable malformed-stream errors while
preserving only bytes committed by earlier valid tokens.

### TVG-0025

Use the same nested `AABABCABC` vector as the streaming LZ78 encoder oracle.
Feed raw bytes and drain canonical tokens one byte at a time; output must equal
the reference encoder byte for byte. A Flush after four raw bytes must emit
nothing and must not shorten the frame. Exercise terminal input while draining,
dictionary freeze, short raw/token/dictionary workspaces, and the aggregate
workspace limit independently.

### TVG-0026

The canonical LZ78/None frame for raw `A` is 64 bytes: the generic 56-byte frame
header declares raw size 1 and equal dictionary/payload sizes of 8, followed by
Pair `(0, 'A')`. The nested `AABABCABC` frame has four tokens and an 88-byte
total extent. Negative frame vectors alter a later phrase index, truncate or
extend the payload, corrupt the frame sequence, and shorten each typed phrase
workspace independently; malformed decode must leave raw output untouched.

### TVG-0027

The initial known-size LZ78 reset stream is six `A` bytes with frame size 3.
Each frame independently emits Pair `(0, 'A')` followed by Pair `(1, 'A')`, so
both have the same 16-byte payload and 72-byte total extent. Including the
80-byte stream prefix, the complete stream is 224 bytes. Corrupt the second
frame's second phrase index and require frame index 1 while raw output and
caller-visible parsed metadata remain untouched.

### TVG-0028

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

### TVG-0029

Use the same 224-byte two-frame reset stream as the outer streaming encoder
oracle. Supply and drain one byte at a time, then repeat with Flush after two
raw bytes and require the identical stream. Exercise short raw-frame, encoded-
frame, and encoder phrase-table workspaces, plus an aggregate limit one byte
below three raw bytes, a 72-byte complete frame, and three phrase records.
Verify premature EndInput, trailing input, empty input, delayed EndInput, and
unsupported ResetBlock independently.

### TVG-0030

For the canonical LZ78 profile with original size 7 and frame size 4, require
four raw-frame bytes, an 88-byte worst-case complete frame, and four encoder
phrase records. With maximum entries 2, only the phrase-record count shrinks.
For decoder limits of 200 aggregate bytes and otherwise-loose payload and
dictionary bounds, require the largest payload `P` satisfying
`56 + P + 1 + floor(P / 8) * sizeof(Lz78PhraseEntry) <= 200`; the next byte must
fail the same inequality. On the current MSVC x64 ABI a phrase record occupies
16 bytes, so the boundary is 47 payload bytes and five phrase records; the
sixth record introduced at 48 payload bytes exceeds the limit.

### TVG-0031

For the LZ78 C ABI, encode six `A` bytes in two three-byte frames and require
the canonical 224-byte stream, then decode it through a separately initialized
handle and compare all six bytes. The encoder query reports three raw bytes, an
80-byte worst-case complete frame, and a nonempty aligned opaque phrase region.
Reject a deliberately misaligned phrase region, nonzero reserved configuration,
and a zero local dictionary-entry limit.

### TVG-0032

Run the existing CLI overwrite, malformed-input, empty-input, and file
round-trip script with explicit codec `lz78`. The generated stream must decode
back to the exact repeated-text fixture and a rejected malformed stream must
leave neither destination nor temporary file. Run one LZ78 benchmark smoke
iteration over `README.md`; timing begins only around transform processing and
the untimed preflight round trip must succeed first.

### TVG-0033

For permanent LZ78 fuzz regressions, truncate the canonical 224-byte reset
stream at every earlier byte and require one-shot decode to leave all raw output
untouched. Mutate the first token's tag, reserved bytes, and root reference;
replace the first frame's three size fields with all ones; and reference phrase
1 from the first token of the reset second frame. Every case must fail without
publishing caller-visible stream metadata or raw bytes.

### TVG-0034

LZW variant 1 vectors record the longest current string, emitted numeric code,
new prefix-plus-byte entry, next-free code, encoder and decoder width before
each code, and exact LSB-first packed bytes. Begin with `A`, `AA`, `AB`, and
`AAA`, then `ABABABA`; `AAA` is the smallest `KwKwK` case. Add binary zero and
every one-byte input, the transitions immediately before/at/after codes 512 and
1024, the maximum-width boundary, dictionary freeze, final partial bytes, and
outer-frame reset. Independently regenerate packed bytes from the listed
numeric codes rather than using an external LZW encoder.

Negative LZW vectors cover a non-literal first code, a code above next-free,
`code == next_free` after freeze, premature code bits, a phrase crossing the
declared raw size, checked phrase-length overflow, excess payload bytes, and
every nonzero final-padding position. Decoder tests must distinguish the exact
width-boundary schedule from both adjacent off-by-one schedules. Strict
reference failure leaves raw output and caller-visible parsed metadata
untouched.

### TVG-0035

The exact first width-boundary vector is 288 zero bytes followed by
`00 00 08`. It represents 256 literal-zero codes at 9 bits, one literal-zero
code at 10 bits, and `code 512` at 10 bits. The last code is `KwKwK`, so the
declared output is 259 zero bytes. Require 258 codes, 257 new dictionary
entries, 2324 logical code bits, and four zero padding bits. A decoder that
changes width one code late observes the `08` data bit as nonzero padding.

### TVG-0036

For atomic LZW reference decoding, decode the documented `A`, `AA`, `AAA`, and
`ABABABA` payloads, including both `KwKwK` cases, then decode the 291-byte width-
boundary vector into exactly 259 zero bytes. Decode all 256 one-byte values from
their literal nine-bit representation. With maximum width 9, decode 258 literal-
zero codes from 291 zero payload bytes after the 256-entry dictionary freezes.
A forward code, nonzero padding, short workspace, local-limit failure, and short
output must not publish any raw byte.

### TVG-0037

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

### TVG-0038

For LZW streaming decode, feed `ABABABA` through one-byte input and output
buffers, retain EndInput while a phrase drains, and also supply EndInput on a
later zero-byte call. Feed the 2048-byte generated width fixture one encoded
byte at a time with three-byte output capacity, and require the exact raw
fixture. Repeat the complete fixture at maximum width 9 after dictionary
freeze. Truncation, nonzero padding, trailing bytes, and a forward code may
preserve only raw bytes committed by earlier accepted codes. Exercise empty and
ended calls, short dictionary workspace, invalid parameters and limits,
cumulative serialized limits, and unsupported ResetBlock independently.

### TVG-0039

Use the same canonical `ABABABA` bytes as the LZW streaming encoder oracle.
Feed and drain one byte at a time, drain a complete frame before a later empty
EndInput, and retain EndInput while output drains. Flush after three raw bytes
must emit nothing and must not shorten the frame. Require byte identity for the
2048-byte width fixture at both the default maximum and the 9-bit freeze
profile. Exercise premature and trailing input, short raw, encoded, and phrase
workspaces, an aggregate limit one byte below the exact requirement, empty and
ended calls, and unsupported ResetBlock.

### TVG-0040

For the LZW plus entropy None frame adapter, require the documented 58-byte
single-`A` frame byte for byte. Plan, encode, validate, and decode `ABABABA`,
requiring four codes, five payload bytes, and exact raw recovery. Exercise a
short contextual final frame after prior committed output. Verify that short
output, insufficient encoder and decoder phrase workspaces, a non-literal
first code, a truncated frame, trailing bytes, an unexpected sequence, an
empty raw frame, and an unsupported entropy selection fail without publishing
raw output.

### TVG-0041

For the one-shot LZW stream adapter, split six `A` bytes into two three-byte
frames. Require an 80-byte prefix, two identical 59-byte reset frames, a
198-byte total stream, and exact round trip. Empty input must contain only the
prefix. Corrupt the first code of the second frame and require that neither raw
output nor parsed configuration is published. Exercise short encoded and raw
output, truncation, trailing bytes, invalid parameters, insufficient encoder
and decoder phrase workspaces, and declared input-size mismatch.

### TVG-0042

Feed the same 198-byte two-frame LZW stream to the outer streaming decoder one
input byte at a time with one-byte output capacity, and require exact recovery
plus stable ended calls. Corrupt the first code in the second frame and require
the first frame's three bytes to remain committed while no second-frame byte is
published. Exercise short serialized-frame, decoded-frame, and phrase storage,
an aggregate buffered limit one byte below the exact requirement, truncation,
empty streams, trailing bytes, a later empty EndInput, and ResetBlock.

### TVG-0043

Use the same 198-byte two-frame LZW stream as the outer streaming encoder
oracle. Supply raw input and drain encoded output one byte at a time, requiring
exact one-shot byte identity and stable ended calls. Flush after two of three
frame bytes must emit only the 80-byte prefix and leave the partial frame open.
Exercise short raw-frame, serialized-frame, and encoder phrase storage, an
aggregate buffered limit one byte below the exact requirement, premature
EndInput, trailing raw input, empty streams, a later empty EndInput, and
ResetBlock.

### TVG-0044

For LZW profile calculation, require a four-byte default-width frame to reserve
eight payload bytes, a 64-byte serialized frame, and three encoder phrase
records. At maximum width 9, require a 300-byte frame to reserve 338 payload
bytes and freeze at 256 records. Empty input reserves only the frame header.
Exercise invalid widths and an aggregate encoder limit one byte short. For
decoder sizing, verify the largest discrete code width permitted by the local
dictionary-entry limit, then independently enumerate the payload boundary
under a tight aggregate limit and prove that the next byte does not fit.

### TVG-0045

For the LZW C ABI, encode six `A` bytes as two three-byte frames using caller
workspaces and require the canonical 198-byte stream. Query decoder workspace
from restricted local limits, decode through the opaque transform, and require
exact recovery. Verify default maximum width 16, public structure metadata,
nonzero aligned phrase workspace, misalignment rejection, reserved-field and
zero-limit rejection, and an invalid encoder code width.

### TVG-0046

For CLI integration, run the generic nonempty, overwrite-rejection, malformed,
and empty-file round-trip script with explicit codec `lzw`; successful output
must reproduce the source and failed decode must leave no destination or
temporary file. Run one LZW benchmark smoke iteration over README input and
require its internal C-ABI round-trip verification to succeed.

### TVG-0047

For permanent LZW fuzz regressions, truncate the canonical 198-byte reset
stream at every byte and require atomic one-shot rejection. Independently
mutate the first code to 256, set nonzero final padding, saturate frame extent
fields, and use code 256 as the first code after a frame reset. The bounded
fuzz harness applies arbitrary bytes to strict and streaming decode with 4 KiB
total output, 1 KiB frames, 4 KiB payloads, 768 phrase records, input-derived
chunks, and a finite call guard.

### TVG-0048

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

### TVG-0049

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

### TVG-0050

Negative LZD validator vectors cover every incomplete-token remainder, absent
left, a phrase reference one beyond the current dictionary, absent right before
the final token or before exact raw completion, a pair crossing raw extent,
tokens after raw completion, premature token-stream end, short phrase
workspace, serialized/frame/aggregate local limits, and dictionary freeze.
For checked length overflow, begin with `(A,A)`, then emit 63 tokens whose two
components both reference the phrase inserted immediately before. Token 63
attempts to add two lengths of `2^63` and must fail before insertion.

### TVG-0051

LZD decoder tests expand the hand vectors and the published illustrative
factorization above without consulting an encoder. An iterative-depth vector
uses `(A,A)`, `(256,B)`, `(257,C)`, and `(258,258)`, whose four token outputs
concatenate to `AAAABAABCAABC`; it exercises nested left references and the
right-before-left stack rule. A frozen-dictionary vector verifies that later
tokens still reference the last stored phrase. Invalid input, short output,
short phrase workspace, short expansion workspace, and aggregate memory-limit
failures each begin with a nonzero output pattern and verify atomic rejection.

### TVG-0052

LZD encoder tests generate every hand-checkable token vector directly from raw
input and require the published `abbaababaaba` factorization byte for byte.
Encoding `ABABABABAB` with one phrase slot must retain reference 256 after
freeze and emit `(A,B), (256,256), (256,256)`. The deterministic binary vector
contains all byte values twice; a separate 1,025-byte fixed-LCG vector checks
the `8 * ceil(raw_size / 2)` serialized bound. Both are encoded repeatedly and
decoded through the independent strict decoder. Short output, short workspace,
invalid parameters, serialized limits, frame limits, and aggregate raw-plus-
workspace limits remain ordinary atomic negative tests.

### TVG-0053

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

### TVG-0054

LZD streaming encode feeds `abbaababaaba` through one-byte raw and token spans
and compares every byte with the one-shot reference encoder. Separate vectors
drain a full frame before a zero-byte EndInput, retain EndInput while a token
region drains, leave a partial frame open across Flush, and preserve the
one-entry frozen dictionary for `ABABABABAB`. Workspace calculations check
empty, one-byte, frozen, and unsupported extents through the shared format
bound. Premature input end, trailing raw bytes, each of the three short caller
workspaces, aggregate limits, empty input, and unsupported reset remain stable
negative or terminal cases.

### TVG-0055

The LZD None profile uses a ten-byte stream with four-byte frames to verify a
largest-frame encoder requirement of 4 raw bytes, 16 maximum token bytes, a
56-byte frame header, and two phrase records. Empty and final-short-frame cases
exercise zero and odd raw sizes. Decoder workspace tests first allow a 1,024-
byte payload capped at ten phrase records, then enumerate the last payload byte
that fits a 300-byte aggregate limit and compare the binary-search result.
Invalid parameters, token limits, encoder aggregate limits, invalid local
limits, unsupported host extents, and an aggregate too small for even an empty
payload are stable negative cases.

### TVG-0056

LZD frame tests wrap the published `abbaababaaba` factorization in a generic
header and require five tokens. The documented one-byte `A` frame is compared
against all 64 literal bytes from `docs/format.md`. A contextual final frame
uses sequence 1 at committed raw offset 8. Short encoder and decoder output,
forward phrase references, short phrase and expansion workspaces, truncation,
trailing bytes, sequence corruption, unsupported pipelines, and missing
encoder workspace are atomic or stable negative cases. Separate thresholds
prove that encoder planning, validation, and decoding each count the generic
header and their complete caller-owned workspaces in the aggregate limit.

### TVG-0057

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

### TVG-0058

The LZD outer streaming decoder uses the documented 208-byte `ABAB` stream as
its one-byte input/output oracle. It must match the one-shot raw bytes while
retaining EndInput across final-frame draining. Corrupting the second payload
must still publish the complete first frame but none of the corrupt frame.
Independent construction cases cover short encoded-frame storage, raw staging,
phrase records, expansion stack, and their aggregate limit. Truncation, empty
streams, trailing bytes, a later empty EndInput, and ResetBlock are stable state
machine cases. Flush across a partial frame must not close it; invalid
construction and unknown flags enter a reproducible terminal error.

### TVG-0059

The LZD outer streaming encoder feeds raw `ABAB` through one-byte input and
output spans and requires byte-for-byte equality with the documented 208-byte
one-shot stream. Flush after one raw byte may drain only the 80-byte prefix and
must leave the partial two-byte frame open. Separate cases cover raw-frame,
serialized-frame, and encoder-entry storage; their complete aggregate limit;
premature EndInput; trailing raw bytes; empty input; a later empty EndInput;
ResetBlock; unknown flags; invalid construction; and reproducible terminal
errors.

### TVG-0060

The bounded LZD fuzz harness invokes both one-shot and outer streaming decoders
with 4 KiB total output, 1 KiB raw frames, 4 KiB payloads, 512 phrase records,
and 513 expansion entries. Input-derived chunk sizes remain subordinate to a
finite call guard. Permanent GoogleTest cases reject every truncation of the
208-byte canonical stream and mutate absent/forward references, token extents,
extreme frame lengths, and a second-frame phrase reference while proving
one-shot raw/configuration atomicity. The initial repository-owned corpus seed
is the five-byte truncated frame magic `MRF1\n`.

### TVG-0061

For the public LZD integration path, encode and decode `ABAB` through separately
initialized C ABI handles and require the documented 208-byte stream. Query and
honor both direction-specific opaque views regions, reject misalignment and
invalid reserved or entry-limit fields, and run the full completion data-class
matrix through that same public surface. Run the generic CLI overwrite,
malformed cleanup, empty input, and file round-trip script with explicit codec
`lzd`; its repeated-text count is 320 to keep this clear reference encoder
smoke bounded. Run one C-ABI-only LZD benchmark iteration
over `README.md` after its untimed round-trip preflight succeeds.

### TVG-0062

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

### TVG-0063

Reference decode uses the same empty, `A`, `ABAB`, frozen `ABABAB`, and
published `abbaababaaba` token vectors. Add the phrase-reference sequence
`A, A, 256, 257, 258`, whose phrase lengths are `1, 1, 2, 3, 5`, to exercise
iterative expansion of the growing binary grammar. Require the complete raw
concatenation `AAAAAAAAAAAA` (12 `A` bytes) and a five-entry expansion stack
for its four generated entries. Corrupt a later reference, shorten raw output,
shorten the expansion stack, omit phrase records, and lower the full token plus
phrase plus stack aggregate by one byte; every failure leaves patterned output
untouched.

### TVG-0064

The reference encoder must reproduce each preceding LZMW vector byte for byte,
including the eight-reference published factorization and the frozen one-entry
dictionary sequence. Test every possible one-byte input, repeated planning and
encoding, all 256 byte values followed by a second copy, and deterministic
pseudorandom binary input. Decode each nontrivial generated stream through the
independent validator-first decoder. Short output, short phrase-span workspace,
serialized-size rejection, invalid parameters, frame-size rejection, and the
full input-plus-workspace aggregate limit must fail before any patterned output
byte is changed.

### TVG-0065

For streaming decode, generate the fixed reference stream with the independent
reference encoder and feed it through one-byte input and output spans. Exercise
`EndInput` both with the final token bytes and in a later empty call, drain a
nonempty frame without repeating the flag, and accept an empty frame. A
truncated final token must publish no raw byte and retain its stable byte
position on repeated error calls. Also reject a call exceeding the conservative
encoded extent before consuming it, every unsupported flag, each independently
short workspace, and an aggregate limit one byte below the complete encoded,
phrase, stack, and staged-output requirement.

### TVG-0066

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

### TVG-0067

For the LZMW plus None profile, verify a ten-byte stream with four-byte frames,
an empty stream, and a final frame shorter than the configured frame size.
Check exact raw, frame-header-plus-token, phrase-span, phrase-record, and
expansion-stack workspace counts. Reject zero frame size, invalid LZMW
parameters, serialized-token limits, and a complete encoder aggregate one byte
too small. Derive decoder workspace from two independent coupled-limit sets,
including a binary-searched aggregate boundary, and reject an aggregate unable
to hold even the frame header, maximum raw frame, and one expansion reference.

### TVG-0068

For the one-shot LZMW plus None frame codec, encode raw `A` and compare all 60
bytes with the documented frame vector. Encode, validate, and decode the
published `abbaababaaba` factorization, then exercise a contextual final short
frame. Reject short encoded output atomically, short decoded output, an invalid
phrase reference, short phrase and expansion workspaces, truncated and trailing
frame extents, a wrong frame sequence, a non-None pipeline, and short encoder,
validator, and full-decoder aggregates.

### TVG-0069

For the complete LZMW plus None stream, compare raw `ABAB` with frame size 2
against the documented 208-byte, two-reset-frame vector. Verify the exact
80-byte empty stream and a binary final short frame. Corrupt only the later
frame and require that output and parsed metadata remain unchanged; separately
preflight a later frame's expansion workspace and aggregate before publication.
Reject truncated and trailing streams, invalid parameter bytes, short phrase
and expansion workspaces, input/original-size mismatch, short output, and short
encoder workspace.

### TVG-0070

For outer frame-streaming decode, feed the documented two-frame LZMW stream
through one-byte input and output spans. Corrupt the second frame's first
reference and require exactly the first frame's two raw bytes to be committed.
Exercise short encoded-frame, decoded-frame, phrase-record, and expansion
workspaces independently, plus the full per-frame aggregate one byte short.
Verify truncated input, empty stream, trailing data, an empty final `EndInput`,
partial-input `Flush`, unknown flags, `ResetBlock`, invalid limits, and stable
terminal states.

### TVG-0071

For outer frame-streaming encode, use the complete one-shot LZMW stream as the
byte-for-byte oracle. Feed the documented two-frame `ABAB` input through
one-byte input and output spans, keep a partial frame open across `Flush`, and
encode a binary final short frame. Exercise empty input, a full final frame
followed by a later empty `EndInput`, premature and excess input, unsupported
flags, every independently short workspace, and the complete per-frame
aggregate one byte below its required size. Terminal ended and error states
must remain stable.

### TVG-0072

For the public LZMW C integration path, initialize independent encode and
decode configurations and query every workspace extent rather than reproducing
private C++ layout formulas. Encode the documented two-frame `ABAB` stream and
require 208 bytes, then decode it through a separately created transform and
compare all raw bytes. Test a deliberately misaligned views address, nonzero
reserved configuration data, a zero dictionary limit, and zero maximum entries
through a translation unit compiled strictly as C11.

### TVG-0073

The LZMW completion matrix uses frame size 64 and validates empty input, all
256 one-byte inputs, all byte values in sequence, repeated zeroes, a repeated
binary pattern, deterministic pseudorandom bytes, and lengths 63, 64, and 65.
Re-encode every case and require exact bytes. For a 193-byte stream, compare
one-shot-equivalent processing with 1/1, 7/5, and 13/17 input/output chunks and
round-trip each result. Run one public-C-ABI benchmark smoke iteration over the
README input; the benchmark must verify the decoded bytes before timing.

### TVG-0074

The bounded LZMW fuzz harness invokes both one-shot and outer streaming
decoders with 4 KiB total output, 1 KiB raw frames, 4 KiB payloads, 1024 phrase
records, and 1025 expansion entries. Input-derived chunk sizes remain under a
finite call guard. Permanent GoogleTest cases reject every truncation of the
208-byte canonical stream and mutate absent/forward references, token extents,
extreme frame lengths, and a second-frame phrase reference while proving
one-shot raw/configuration atomicity. The repository-owned corpus begins with
the five-byte truncated frame magic `MRF1\n`.

### TVG-0075

For CLI integration, run the generic file-level round-trip script with explicit
codec `lzmw` and a 320-repeat deterministic text fixture. Require encode and
decode success, byte-for-byte restoration, rejection of an existing output,
rejection of malformed input without either destination or `.tmp` residue, and
an empty-file round trip. The executable must reach LZMW only through the public
C ABI configuration, workspace-query, factory, process, and destroy functions.

### TVG-0076

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

### TVG-0077

For combined raw decode, use the same 88-byte `A` frame and require that bytes
beyond its one-byte raw extent remain unchanged. Independently construct a raw
Blocked Huffman frame whose dictionary payload is a Literal `A` followed by a
distance-one, length-four terminal LZ77 match; require output `AAAAA` to exercise
overlap-copy semantics without relying on a combined encoder. A four-byte raw
destination for that five-byte frame and corrupt descriptor/token variants must
leave every raw destination byte unchanged.

### TVG-0078

For combined encode, require plan and encode of raw `A` to reproduce the exact
documented 88-byte frame. Encode the `AAAAA` overlap vector twice and require
identical 104-byte streams plus decoder round-trip. Encode byte values `0..63`
as 1024 canonical Literal-token bytes with one 1024-symbol entropy block and
require the static canonical Huffman representation rather than raw storage.
Repeat with 300-symbol entropy blocks, require four blocks with a final
124-symbol block, and round-trip the complete frame. Short staging and
serialized destinations remain sentinel-filled; empty and contextually wrong
raw frame extents are rejected.

### TVG-0079

For the combined complete stream, encode raw `ABABX` with raw frame size 2 and
entropy block size 16. Require three frames of raw extents 2, 2, and 1 and an
exact total of 408 bytes including the 80-byte prefix. Compare the two `AB`
frame bodies to prove both layers reset, then decode the stream and re-encode it
deterministically. Require empty input to produce only the 80-byte prefix.
Reject every strict prefix and trailing data. Corrupt the second frame's first
staged token and require the full raw output plus parsed stream/parameter
outputs to remain unchanged. Exercise serialized-output, raw-output, block-view,
and dictionary-staging capacity failures independently.

### TVG-0080

For combined streaming encode, use the complete `ABABX` stream as the exact
oracle and feed it through one-byte input and output spans. Keep the first
partial raw frame open across `Flush`, then finish all remaining frames with
`EndInput`. Exercise empty input, stable ended state, premature `EndInput`,
unsupported `ResetBlock`, independently short raw-frame, dictionary-token, and
serialized-frame workspaces, and the actual three-workspace aggregate one byte
below its required 154 bytes.

### TVG-0081

For combined streaming decode, feed the same complete stream through one-byte
input and output spans. Corrupt the second frame's first token and require only
the first frame's two raw bytes to be committed. Exercise short serialized,
dictionary, raw, and view workspaces independently, plus the four-way aggregate
one byte short. Verify truncation, trailing input, empty input, temporary
`Flush` starvation, unsupported `ResetBlock`, stable error/end states, and a
premature `EndInput` latched while a non-final frame still needs output.

### TVG-0082

For the combined profile, check a one-million-byte largest frame as sixteen
million worst-case LZ77 token bytes split into 245 entropy blocks. Check a
17-byte final-only stream, empty input, block-count exhaustion, one-byte-short
aggregate policy, invalid LZ77 parameters, decoder requirements derived only
from local limits, and stable error mapping. Finally allocate every reported
encoder and decoder workspace for the `ABABX` fixture and require the streaming
transforms to produce and consume the exact 408-byte stream.

### TVG-0083

For the combined C ABI, initialize rather than hand-construct both direction
configurations. Encode binary `ABABABX` with a seven-byte frame and 16-symbol
entropy blocks, then decode the resulting stream through independently queried
local limits and require exact byte equality. Check the encoder's hand-derived
worst-case workspace partition, decoder byte/view requirements, a one-byte
short secondary region, and rejection of a nonzero reserved field. Compile the
test as C11 and link it to the shared library so the public declaration and
exported ABI are exercised rather than internal C++ entry points.

### TVG-0084

For CLI composition, run the shared round-trip script with the exact codec name
`lz77-blocked-huffman`. Its repeated binary-safe fixture must encode and decode
through multiple 64 KiB I/O chunks, reject overwrite of the completed archive,
reject a malformed stream without committing output, and compare restored
bytes exactly. Keep the existing unqualified and explicit standalone LZ77 CLI
tests to prove the default mapping did not change.

### TVG-0085

For the combined benchmark smoke test, use README input and one iteration under
the exact selector `lz77-blocked-huffman`. The driver must allocate only queried
C ABI workspaces, verify the encoded stream by decoding it byte-for-byte before
timing, then report complete-stream ratio, both throughputs, each workspace
region, and their direction-wise peak. The smoke result is validation of the
measurement path, not a stable performance threshold.

### TVG-0086

For combined fuzz regression, encode the exact three-frame `ABABX` stream and
require every strict truncation to leave raw output, parsed stream metadata,
and LZ77 parameters unchanged. Replace the first frame's size fields with all
one bits and require the same atomic rejection. Retain the hand-authored
five-byte `MRF1` plus newline truncated-magic seed; it contains no externally
sourced data. Compile the bounded fuzz entry point under every normal build,
but do not treat compilation or a short smoke run as coverage completion.

### TVG-0087

The combined completion matrix uses 64-byte outer frames and 64-symbol entropy
blocks. Require empty input, every individual byte, byte values `0..255`, 257
zeros, a 259-byte `00 FF 55 AA` pattern, deterministic 513-byte high-entropy
data, and deterministic inputs of lengths 63, 64, and 65 to encode identically
twice and round-trip through the public C ABI. For a 193-byte four-frame input,
require encode output to match under input/output chunks `(1,1)`, `(7,5)`, and
`(13,17)`, and decode each schedule byte-for-byte.

### TVG-0088

For the 2026-07-16 bounded sanitizer smoke campaign, use each repository seed
corpus as a read-only input corpus and a separate disposable generated corpus.
Run every decoder target for 10,000 inputs with an 8 KiB maximum input, a
five-second per-input timeout, and a 512 MiB RSS limit. The six campaigns
completed 60,000 total executions without a crash, hang, or sanitizer finding;
generated mutations remain build artifacts unless promoted as a minimized
permanent regression.

### TVG-0089

For the 2026-07-16 compiler-independence check, build RelWithDebInfo with Clang
22.1.3's GNU-style driver and Ninja, and build Release with MSVC and MSBuild.
Run all 863 tests under both optimized builds. Then encode the same checked-out
`README.md` through `lz77`, `lz77-blocked-huffman`, `lzss`, `lz78`, `lzw`,
`lzd`, and `lzmw` using each compiler's CLI and compare every pair byte for
byte. All seven pairs matched. Treat the files as disposable evidence because
the README input changes with documentation; canonical hand vectors remain the
permanent representation tests.

### TVG-0090

The CI interoperability fixture has exactly 8,193 bytes. Bytes `0..255` contain
their own values, bytes `256..1279` are zero, and bytes `1280..3327` repeat
`41 42 41 42 58 00 FF`. For every later zero-based position `i`, generate
`(i*73 + (i>>3)*19 + 41) & 0xFF`. Generate this input independently on every
platform. Schema 2 / codec set `marc-cli-v2` round-trips `checksum-raw` plus the
seven original CLI profiles before publication and records every file's size
and SHA-256 in the bundle manifest. Generated archives are CI evidence rather
than permanent canonical vectors. Legacy schema 1 retains its original exact
seven-profile meaning.

### TVG-0091

For CRC-32C, feed ASCII `123456789` into the reflected Castagnoli recurrence
from initial register `FFFFFFFF`, then XOR the final register with `FFFFFFFF`.
The numeric result is `E3069283`; serialize it through marc's little-endian
digest rule as `83 92 06 E3`. Also require empty input to produce four zero
bytes, every split of the check string to produce the same digest, reset to
restore the empty state, and wrong-sized digest spans to remain unchanged.

### TVG-0092

For SHA-256, use the FIPS standard byte ordering without applying marc integer
endianness to the digest string. Empty input produces
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`;
ASCII `abc` produces
`ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`.
Also use the 56-byte message `abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq`,
whose digest is
`248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1`.
Test every split of the 56-byte vector, repeated final snapshots, reset, exact
digest capacity, and one-byte incremental updates across multiple blocks.

### TVG-0093

For the fixed hash descriptor, serialize CRC-32C target UncompressedBytes with
scope PerFrame as
`01 00 00 00 01 02 04 00 00 00 00 00 00 00 00 00`. Parse SHA-256 target
UncompressedBytes with scope WholeStream from
`02 00 00 00 01 01 20 00 00 00 00 00 00 00 00 00`. Reject unknown algorithm,
target, and scope IDs, an algorithm/digest-size mismatch, flags, and each of the
four reserved bytes independently. Require failed parse and serialization to
leave caller-owned destinations unchanged.

### TVG-0094

For a three-record canonical region, concatenate SHA-256 UncompressedBytes /
WholeStream, CRC-32C UncompressedBytes / PerFrame, and SHA-256
UncompressedBytes / PerFrame in that order. Require exact 48-byte serialization
and parsing. Also accept an empty region, reject a 17-byte region, reject
insufficient descriptor output before publication, corrupt the second record's
reserved field to prove whole-region atomicity, and distinguish an identical
tuple duplicate from a descending tuple. Invalid serialization and a
one-byte-short byte span must remain transactional.

### TVG-0095

For the staged version 1.1 prefix, start with the 64-byte empty framing vector,
set minor version byte 6 to `01`, and set hash-descriptor byte count at offset
36 to little-endian 16. Require the dedicated 1.1 parser and serializer to
round-trip this prefix while the existing 1.0 parser rejects it. Conversely,
the dedicated entry point rejects a 1.0 prefix and unknown minor versions.
Reject descriptor sizes 1 and 17, a nonzero extension, and checked aggregate
dictionary/entropy/descriptor bytes beyond the local internal-buffer limit;
all parse failures leave the destination header unchanged.

### TVG-0096

For the initial version 1.1 checksum profile, accept only the single canonical
CRC-32C / UncompressedBytes / PerFrame descriptor and a declared trailer size
of four. Reject no descriptor, an additional descriptor, every other target or
scope, SHA-256, mismatched digest length, flags, and any trailer size other than
four. Generate `83 92 06 E3` from uncompressed ASCII `123456789` and four zero
bytes from an empty frame byte span. Verify both, reject each one-byte digest
corruption as a mismatch, and require descriptor or output-size failures to
leave caller-owned trailer output unchanged.

### TVG-0097

For the staged version 1.1 frame-header gate, use the raw three-byte frame
vector and set checksum trailer size at offset 36 to little-endian four. Supply
a version 1.1 stream prefix declaring 16 descriptor bytes and the single
CRC-32C / UncompressedBytes / PerFrame descriptor. Require dedicated parse and
serialization to reproduce the 56-byte header. Reject it through the version
1.0 entry point; reject a version 1.0 context through the staged entry point;
and independently reject a missing descriptor, a descriptor-region size
mismatch, an unsupported descriptor, trailer sizes zero and five, and a local
buffer limit one byte below the frame's checked staged extent. All parse and
serialization failures remain transactional.

### TVG-0098

For the complete raw-checksum reference profile, encode `61 62 63` as one
frame and require total size 143: 80-byte prefix/descriptor, 56-byte frame
header, three raw payload bytes, and `B7 3F 4B 36`. Require empty input to
produce exactly 80 bytes and no frame. Use a five-byte input with frame size two
to prove three independent checksum resets and exact round trip. Repeat encoding
for byte identity; reject every strict truncation of the one-frame vector,
trailing input, short output, version 1.0, unsupported algorithms, malformed or
extra descriptors, corrupted frame sizes, altered payload, and every checksum
byte corruption. No decode failure may modify output stream metadata,
descriptor output, or raw output.

The raw-checksum fuzz boundary truncates cases to 8 KiB, supplies a fixed 4 KiB
output, permits at most 1 KiB per frame, and uses no input-sized allocation.
Compile it under normal MSVC and Clang test builds and retain the hand-authored
truncated `MARC` seed as the initial permanent corpus entry.

### TVG-0099

The raw-checksum completion matrix uses 64-byte frames. Require empty input,
every individual byte, byte values `0..255`, 257 zeros, a 259-byte
`00 FF 55 AA` pattern, deterministic 513-byte high-entropy data, and lengths
63, 64, and 65 to encode identically twice and round-trip through the public C
ABI. For a 193-byte four-frame input, require byte-identical encode and decode
under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Corrupt or truncate the last
one-byte frame, then append trailing data independently; each case must report
a stable malformed-stream error, preserve the first 192 verified bytes, and
suppress the final frame. Successful repeated calls remain EndOfStream.

### TVG-0100

For Adaptive Huffman fuzzing, retain the five-byte repository-authored
`MARC`-plus-newline truncated-prefix seed. Bound fuzzer input to 8 KiB, decoded
output to 4 KiB, frames to 1 KiB, compressed payload and frame-local buffering
to 4 KiB, and drive the incremental decoder with byte-derived chunks of at most
17 input and 19 output bytes. The one-shot and streaming paths share the same
limits; neither allocates from input-controlled sizes.

### TVG-0101

For Dynamic Range fuzzing, use the same five-byte repository-authored
truncated-prefix seed and 8 KiB/4 KiB/1 KiB input, output/internal, and frame
bounds. Fix `max_range_model_total` to 32,768 and cap byte-derived incremental
chunks at 17 input and 19 output bytes. Both decoder paths share this policy and
use fixed arrays only.

### TVG-0102

For rANS fuzzing, use the repository-authored five-byte truncated-prefix seed.
Bound input/output/frame/block/payload/internal bytes to
8 KiB/4 KiB/1 KiB/256/4 KiB/8 KiB, allow at most eight block views, and cap the
entropy table at 4,096 entries. Drive the incremental path with at most 17 input
and 19 output bytes per call; both decoder paths use fixed view and byte arrays.

### TVG-0103

For tANS fuzzing, use the same reviewed truncated-prefix seed and
8 KiB/4 KiB/1 KiB/256/4 KiB/8 KiB byte limits as rANS. Supply eight fixed
`TansBlockView` records, cap the state table at 4,096 entries, and drive the
incremental path with at most 17 input and 19 output bytes per call.

### TVG-0104

For standalone Blocked Huffman fuzzing, retain the reviewed five-byte prefix
seed and use 8 KiB input/internal, 4 KiB output/payload, 1 KiB frame,
256-symbol block, and eight-view bounds. Cap code lengths at 24 and decode table
nodes at 512. Feed both decoder paths fixed arrays and drive incremental input
and output with the common 17/19-byte chunk caps.

### TVG-0105

The standalone LZ77 fuzz boundary supplies every bounded case to both strict
whole-stream decode and frame-committing streaming decode. It permits 8 KiB of
serialized input, 4 KiB of total output and dictionary payload, 1 KiB frames,
fixed encoded and decoded frame arrays, 17/19-byte chunk caps, and an
independent checked call ceiling. The initial five-byte `MARC` plus newline
seed is a deliberately truncated repository prefix.

### TVG-0106

For standalone Blocked Huffman CLI integration, run the shared file harness
with explicit codec `blocked-huffman` over enough repeated text to cross the
one MiB frame boundary. Require exact nonempty and empty round trips, existing
destination rejection, malformed-prefix rejection, and trailing-byte rejection
after a valid multi-frame stream. Failed decode must leave neither destination
nor temporary output. Keep this selector outside an existing versioned
interoperability bundle until that manifest receives a new codec-set version.

### TVG-0107

Run the standalone Blocked Huffman benchmark smoke over `README.md` for one
iteration. Its untimed preflight must reproduce the file exactly before any
throughput is reported. Require the public encoder and decoder workspace
queries to supply the measured primary, secondary, and aligned views extents;
capacity planning must include the 64-byte prefix, each possible 16-byte block
descriptor, raw fallback bytes, and each 56-byte outer frame header.

### TVG-0108

The standalone Blocked Huffman completion matrix uses 64-byte frames and
32-symbol entropy blocks. Round-trip empty input, every one-byte value, all
byte values in order, 257 zero bytes, a 259-byte `00 FF 55 AA` pattern, and
513 deterministic generated bytes. Exercise lengths 31, 32, 33, 63, 64, and
65. Encode each case twice and require identical bytes.

For a 193-byte four-frame generated input, require byte-identical streams and
exact decode under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Then corrupt
the final frame sequence, truncate its last byte, and append trailing data in
independent cases. Each error must be sticky, preserve the first 192 committed
bytes, and leave the final output sentinel untouched. Repeated calls after
successful completion must remain EndOfStream.

### TVG-0109

For Adaptive Huffman CLI integration, run the shared file harness with explicit
codec `adaptive-huffman` over enough repeated text to cross the one MiB frame
boundary. Require exact nonempty and empty round trips, existing-destination
rejection, malformed-prefix rejection, and strict trailing-byte rejection.
Failed decode must leave neither destination nor temporary output. Keep the
selector outside existing versioned interoperability bundles until a new codec
set is specified.

### TVG-0110

Run the Adaptive Huffman benchmark smoke over `README.md` for one iteration.
Require its untimed public-C-ABI encode/decode preflight to reproduce the input
before reporting timing. Capacity must include the 64-byte prefix, one 56-byte
header and 16-byte descriptor per frame, and 33 payload bytes per input byte.
Report zero views workspace and the larger direction-specific caller workspace
as the codec peak.

### TVG-0111

The Adaptive Huffman completion matrix uses 64-byte frames. Round-trip empty
input, every one-byte symbol, all byte values in order, 257 zero bytes, a
259-byte `00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise
lengths 63, 64, and 65. Encode every case twice and require byte identity.

For a 193-byte four-frame generated input, require the same stream and decoded
bytes under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame sequence, truncate its payload, and append trailing data. Each
error must remain sticky, commit exactly the first 192 bytes, and preserve the
last output sentinel. Repeated successful calls must remain EndOfStream.

### TVG-0112

For Dynamic Range CLI integration, run the shared file harness with explicit
codec `dynamic-range` over enough repeated text to cross the one MiB frame
boundary. Use model total 32,768, payload bound `2*n+5`, and a fixed 16-byte
descriptor. Require exact nonempty and empty round trips, overwrite rejection,
malformed-prefix rejection, trailing-byte rejection, and cleanup of both final
and temporary outputs after failure. Do not alter a versioned interoperability
codec set without introducing a new set identifier.

### TVG-0113

Run the Dynamic Range benchmark smoke over `README.md` for one iteration.
Require an untimed public-C-ABI round trip before output. Capacity planning
must count two payload bytes per input symbol, then add a five-byte termination,
16-byte descriptor, and 56-byte header for each nonempty frame plus the 64-byte
stream prefix. Report model total 32,768, zero views workspace, both direction
workspaces, and their larger total as the codec peak.

### TVG-0114

The Dynamic Range completion matrix uses 64-byte frames and model total 32,768.
Round-trip empty input, every one-byte symbol, all byte values in order, 257
zero bytes, a 259-byte `00 FF 55 AA` pattern, and 513 deterministic generated
bytes. Exercise lengths 63, 64, and 65. Encode every case twice and require byte
identity.

For a 193-byte four-frame generated input, require the same stream and decoded
bytes under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame sequence, truncate its payload, and append trailing data. Each
error must remain sticky, commit exactly the first 192 bytes, and preserve the
last output sentinel. Repeated successful calls must remain EndOfStream.

### TVG-0115

For rANS CLI integration, run the shared file harness with explicit codec
`rans` over enough repeated text to cross the one MiB frame boundary. Use
65,536-symbol blocks, at most 16 blocks per frame, one payload byte per symbol,
an eight-byte final state and 528-byte descriptor per block. Require exact
nonempty and empty round trips, overwrite rejection, malformed-prefix
rejection, trailing-byte rejection, and cleanup of both final and temporary
outputs after failure. Do not alter a versioned interoperability codec set
without introducing a new set identifier.

### TVG-0116

Run the rANS benchmark smoke over `README.md` for one iteration. Require an
untimed public-C-ABI round trip before output. Capacity planning must count one
payload byte per input symbol, then reserve eight final-state bytes, one
528-byte descriptor for each of at most 16 blocks per frame, one 56-byte header
per frame, and the 64-byte stream prefix. Report all three direction workspaces
and their larger total as the codec peak.

### TVG-0117

The rANS completion matrix uses 64-byte frames and 32-symbol blocks. Round-trip
empty input, every one-byte symbol, all byte values in order, 257 zero bytes, a
259-byte `00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise
block lengths 31, 32, and 33 and frame lengths 63, 64, and 65. Encode every case
twice and require byte identity through queried aligned views.

For a 193-byte four-frame generated input, require the same stream and decoded
bytes under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame sequence, truncate its payload, and append trailing data. Each
error must remain sticky, commit exactly the first 192 bytes, and preserve the
last output sentinel. Repeated successful calls must remain EndOfStream.

### TVG-0118

For tANS CLI integration, run the shared file harness with explicit codec
`tans` over enough repeated text to cross the one MiB frame boundary. Use
65,536-symbol blocks, at most 16 blocks per frame, the 12-bit-per-symbol
transition bound, a two-byte state and 528-byte descriptor per block. Require
exact nonempty and empty round trips, overwrite rejection, malformed-prefix
rejection, trailing-byte rejection, and cleanup of both final and temporary
outputs after failure. Do not alter a versioned interoperability codec set
without introducing a new set identifier.

### TVG-0119

Run the tANS benchmark smoke over `README.md` for one iteration. Require an
untimed public-C-ABI round trip before output. Capacity planning must use
`ceil(3*n/2)` bytes for transitions, then reserve two state bytes and one
528-byte descriptor for each of at most 16 blocks per frame, one 56-byte header
per frame, and the 64-byte stream prefix. Report all three direction workspaces
and their larger total as the codec peak.

### TVG-0120

The tANS completion matrix uses 64-byte frames and 32-symbol blocks. Round-trip
empty input, every one-byte symbol, all byte values in order, 257 zero bytes, a
259-byte `00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise
block lengths 31, 32, and 33 and frame lengths 63, 64, and 65. Encode every case
twice and require byte identity through queried aligned views.

For a 193-byte four-frame generated input, require the same stream and decoded
bytes under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame sequence, truncate its payload, and append trailing data. Each
error must remain sticky, commit exactly the first 192 bytes, and preserve the
last output sentinel. Repeated successful calls must remain EndOfStream.

### TVG-0121

For the standalone LZ77 completion matrix, use 64-byte frames. Round-trip empty
input, every one-byte value, all byte values in order, 257 zero bytes, a
259-byte `00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise
frame lengths 63, 64, and 65, and require byte-identical repeat encoding.

For a 193-byte four-frame generated input, require identical archives and raw
output under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame header, truncate the final payload, and append trailing data.
Each error must remain sticky, commit exactly the first 192 bytes, and preserve
the last output sentinel. Repeated successful calls must remain EndOfStream.

### TVG-0122

For the standalone LZSS completion matrix, use 64-byte frames. Round-trip empty
input, every one-byte value, all byte values in order, 257 zero bytes, a
259-byte `00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise
frame lengths 63, 64, and 65, and require byte-identical repeat encoding.

For a 193-byte four-frame generated input, require identical archives and raw
output under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame header, truncate its final variable-token payload, and append
trailing data. Each error must remain sticky, commit exactly the first 192
bytes, and preserve the last output sentinel. Repeated successful calls must
remain EndOfStream.

### TVG-0123

For the standalone LZ78 completion matrix, use 64-byte frames and at most 64
phrase entries. Round-trip empty input, every one-byte value, all byte values in
order, 257 zero bytes, a 259-byte `00 FF 55 AA` pattern, and 513 deterministic
generated bytes. Exercise frame lengths 63, 64, and 65, require byte-identical
repeat encoding, and satisfy every queried phrase-view alignment.
Require zero view bytes for empty encoding and nonzero aligned views for all
non-empty encoders and decoders.

For a 193-byte four-frame generated input, require identical archives and raw
output under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame header, truncate its phrase-token payload, and append trailing
data. Each error must remain sticky, commit exactly the first 192 bytes, and
preserve the last output sentinel. Repeated successful calls must remain
EndOfStream.

### TVG-0124

For the supplemental LZW public completion matrix, use 64-byte frames, maximum
code width 9, and a 256-entry decoder phrase ceiling. Round-trip empty input,
every one-byte value, all byte values in order, 257 zero bytes, a 259-byte
`00 FF 55 AA` pattern, and 513 deterministic generated bytes. Exercise frame
lengths 63, 64, and 65 and require byte-identical repeat encoding. Require zero
view bytes for zero- and one-byte encoders and nonzero aligned views otherwise.

For a 193-byte four-frame generated input, require identical archives and raw
output under chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Independently corrupt
the final frame header, truncate its packed-code payload, and append trailing
data. Each error must remain sticky, commit exactly the first 192 bytes, and
preserve the last output sentinel. Repeated successful calls must remain
EndOfStream.

### TVG-0125

For the strengthened LZD completion matrix, retain 64-byte frames, 32 phrase
entries, the required binary data classes, 63/64/65 boundaries, and the
193-byte chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Require every successful
terminal call to remain EndOfStream when repeated.

Independently corrupt the final frame header, truncate its reference-pair
payload, and append trailing data. Each error must remain sticky, commit exactly
the first 192 bytes, and preserve the last output sentinel.

### TVG-0126

For the strengthened LZMW completion matrix, retain 64-byte frames, 32 phrase
entries, the required binary data classes, 63/64/65 boundaries, and the
193-byte chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`. Require every successful
terminal call to remain EndOfStream when repeated.

Independently corrupt the final frame header, truncate its fixed-reference
payload, and append trailing data. Each error must remain sticky, commit exactly
the first 192 bytes, and preserve the last output sentinel.

### TVG-0127

Schema 3 / codec set `marc-cli-v3` preserves the eight schema-2 profiles in
their existing order, then appends Blocked Huffman, Adaptive Huffman, Dynamic
Range, rANS, and tANS. Generate and locally round-trip all thirteen archives.
Schema 4 / codec set `marc-cli-v4` preserves that thirteen-profile order, then
appends LZSS plus Blocked Huffman and LZ78 plus Blocked Huffman. Generate and
locally round-trip all fifteen archives. Schema 5 / codec set `marc-cli-v5`
preserves the complete schema-4 order, then appends LZW plus Blocked Huffman.
Generate and locally round-trip all sixteen archives. Schema 6 / codec set
`marc-cli-v6` preserves the complete schema-5 order, then appends LZD plus
Blocked Huffman. Generate and locally round-trip all seventeen archives.
Schema 7 / codec set `marc-cli-v7` preserves the complete schema-6 order, then
appends LZMW plus Blocked Huffman. Generate and locally round-trip all eighteen
archives. Schema 8 / codec set `marc-cli-v8` preserves the complete schema-7
order, then appends LZ77 plus Adaptive Huffman. Generate and locally round-trip
all nineteen archives. Schema 9 / codec set `marc-cli-v9` preserves the
complete schema-8 order, then appends LZSS plus Adaptive Huffman. Generate and
locally round-trip all twenty archives. For compatibility testing, filter that
bundle successively to the exact schema-8 through schema-1 profile lists,
rewrite only the versioned manifest fields, and require the same verifier to
accept all nine generations. Swap the first two schema-9 manifest entries
without changing any archive and require rejection before decoding.

### TVG-0128

For LZSS plus Blocked Huffman, start with raw `A`. LZSS variant 1 emits the
canonical Literal `00 41`. With entropy block size two, the mandatory stored
size comparison selects one raw Blocked Huffman block. Prefix its 16-byte raw
descriptor with the 56-byte generic frame header shown in `format.md` to obtain
the exact 74-byte frame.

Validate the hand frame without exposing raw output. Require two staged bytes,
one block view, dictionary size two, and derived raw size one. Reject every
strict truncation and one trailing byte; insufficient views or staging before
writing; aggregate workspace one byte short; malformed entropy metadata before
staging writes; an unknown staged LZSS tag; a wrong frame sequence; and an
unsupported pipeline variant.

### TVG-0129

Require the exact planner and encoder to reproduce that 74-byte frame from raw
`A`. Encode six `A` bytes twice to prove deterministic composition of a
two-byte Literal and nine-byte Match token, then entropy-decode and compare the
complete staged token extent. Generate 1,024 deterministic xorshift bytes to
exercise canonical Huffman selection when smaller. Use 64 distinct bytes with
a 30-symbol entropy block to produce five blocks and a final short block.
Require one-byte-short dictionary staging and serialized output to leave their
respective destinations unchanged, and reject empty or contextually oversized
raw frames.

### TVG-0130

Decode the 74-byte hand frame into an oversized raw destination and require
only its first byte to change to `A`. Round-trip the six-`A` Literal/overlapping
Match case through the complete frame decoder. Require one-byte-short raw
capacity to preserve every destination byte, and corrupt the entropy descriptor
and staged LZSS tag independently to prove neither layer can publish raw. The
1,024-byte canonical-Huffman selection case must also round-trip to the exact
raw input rather than stopping at token-staging comparison.

### TVG-0131

For the known-size combined stream, split ASCII `ABABX` into uncompressed
two-byte frames. Each `AB` frame produces four LZSS Literal bytes and one raw
Blocked Huffman block, so the first two 76-byte frames have identical bodies;
the final `X` frame is 74 bytes. Prefix them with the 80-byte stream header and
LZSS parameters for an exact 306-byte, three-frame vector. Compare the first
two bodies to prove both resets.

Require deterministic encoding, prefix-only empty input, every strict
truncation, one trailing byte, and independent output/view/staging capacity
failures. Corrupt the first LZSS tag in the second frame and require raw output
and caller-visible stream/parameter objects to remain unchanged.

### TVG-0132

Drive the incremental encoder for `ABABX` with one-byte input and output spans
and require byte identity with the 306-byte complete-stream oracle. Repeat with
a nonterminal `Flush` after the first raw byte and a terminal call containing
the remainder. Verify the 80-byte empty stream, repeated ended calls, premature
`EndInput`, unsupported `ResetBlock`, and independent short raw/token/frame
workspaces. For a two-byte raw frame, set the aggregate limit to 81 and require
failure because raw 2 plus tokens 4 plus serialized frame 76 requires 82 bytes.

### TVG-0133

Decode the 306-byte combined stream with one-byte input and output spans and
require `ABABX`. Corrupt the second frame's first staged token tag and require
only the first validated `AB` frame to be published. Independently shorten the
76-byte serialized-frame storage, four-byte token staging, two-byte raw
staging, and one-view region; then set the aggregate bound one byte below
serialized 76 plus tokens 4 plus raw 2 plus one typed view. Also require final
truncation, trailing data, unsupported `ResetBlock`, empty input, flush
starvation, repeated terminal error, and premature terminal state preserved
across a one-byte raw drain.

### TVG-0134

For the combined LZSS profile query, require a 2,500,000-byte known stream with
one-million-byte frames and 65,536-symbol entropy blocks to reserve 1,000,000
raw bytes, 2,000,000 token bytes, and
`56 + 31 * 16 + 2,000,000` serialized-frame bytes. A 17-byte largest frame with
a 64-symbol entropy block reserves 17, 34, and 106 bytes respectively; empty
input reserves no frame-local bytes. Reject an all-Literal block count above
the local maximum and an aggregate workspace one byte above its limit.

Derive decoder requirements solely from selected local limits and verify that
the returned four regions construct the incremental decoder. Use those
encoder and decoder queries to round-trip the exact 306-byte `ABABX` oracle,
so the arithmetic is exercised by the transforms rather than tested only as
isolated numbers.

### TVG-0135

For the pure-C combined LZSS adapter vector, configure `ABABX` as three raw
frames of at most two bytes with four-symbol entropy blocks. Require the encode
query to return primary 2, secondary 80, and no views; require the public
transform to emit a 306-byte three-frame stream. Under decoder limits of
frame 2, dictionary serialization 4, aggregate body 200, and one block,
require primary 256, secondary 6, and one nonzero aligned views region, then
recover `ABABX`.
Reject secondary storage one byte short, a deliberately misaligned full-sized
views region, and a nonzero reserved configuration field.

### TVG-0136

For `marc --codec lzss-blocked-huffman`, generate the common repeated
`ABRACADABRA-0123456789` binary fixture, encode it through the public C ABI,
decode it through a separately constructed transform, and compare the complete
file. Require a second encode to refuse the existing destination. Also
round-trip an empty file, reject a non-marc input, reject one appended trailing
byte, and require neither failure to retain its destination or `.tmp` path.
Encode the same fixture with the MSVC and ClangCL Release tools and require the
two complete archives to compare byte for byte.

### TVG-0137

For the combined LZSS benchmark smoke, use the repository README for one
iteration. Require public-ABI encode/decode round-trip verification and a
successful report containing codec name, input and encoded sizes, ratio,
directional timings and throughput, all six direction/region workspace values,
and peak caller-owned workspace. Under the fixed one-MiB policy, the decoder
query on the current Windows x64 builds reports 5,243,504 primary bytes,
3,145,728 secondary bytes, and 640 aligned views bytes, for an 8,389,872-byte
peak. Do not assert throughput or compression ratio values.

### TVG-0138

For combined LZSS fuzz regression, generate the exact 306-byte `ABABX` stream
and require every strict truncation to preserve raw output, parsed stream
metadata, and LZSS parameters. Replace the first frame's size fields with all
one bits and require the same atomic rejection. Then replace the first LZSS tag
in the second frame's raw entropy payload with `0x7f`; entropy parsing succeeds
far enough to stage the bytes, but token validation must still reject without
publishing anything.

Compile the dual-decoder harness under both ordinary toolchains. For the
sanitizer smoke, copy the reviewed `MRF1` plus newline seed into a disposable
build corpus and run 10,000 cases with 8 KiB maximum input, five-second timeout,
and 512 MiB RSS limit. Do not copy generated mutations back into the repository
unless one reproduces a reviewed defect and becomes a permanent regression.

### TVG-0139

For the combined LZSS public-ABI completion matrix, use 64-byte raw frames and
64-symbol entropy blocks. Round-trip empty input, all 256 one-byte inputs, all
byte values in sequence, zero runs, repeated binary patterns, deterministic
pseudorandom input, and lengths 63, 64, and 65. Require identical archives for
unchunked, one-byte, and mixed chunk schedules. For a 193-byte four-frame
stream, corrupt the final frame header, truncate its last byte, and append one
trailing byte; in every case require the first 192 bytes to remain committed,
the final byte to remain untouched, and the same positioned error to be
returned on a repeated call.

Apply the same four-frame corruption, truncation, trailing-data, sticky-error,
and repeated-end checks to the existing LZ77 plus Blocked Huffman completion
fixture. This closes the older composed profile against the same matrix rather
than granting it readiness under a weaker historical test definition.

### TVG-0140

For the installed-package audit, configure fresh Visual Studio 2026 x64 trees
with tests, examples, tools, and benchmarks disabled. Build and install once
with only `MARC_BUILD_SHARED` enabled and once with only `MARC_BUILD_STATIC`
enabled. For each install prefix, configure `examples/` as an independent
project using only the installed `marc_DIR`, build its pure-C executable, and
require the public C ABI round trip to succeed. Add the installed shared
library directory to `PATH` only for the shared execution; the static consumer
must run without an installed runtime library search path.

### TVG-0141

For the first LZ78 plus Blocked Huffman vector, encode raw `A` through the
already frozen standalone LZ78 grammar to obtain the eight-byte Pair token
`00 41 00 00 00 00 00 00`. Select entropy block size eight. The mandatory
Blocked Huffman size rule chooses raw storage, producing one 16-byte descriptor
with symbol count and payload size eight, no model, raw flag one, and eight
valid bits. Prepend a generic frame header declaring raw size one, dictionary
and compressed sizes eight, one block, and 16 descriptor bytes. The resulting
frame is exactly 80 bytes. The combined frame planner and encoder reproduce it
byte for byte; the validator and decoder accept it, recover the exact Pair
token, build phrase entry `{root, A, length 1}`, and publish only the single raw
byte. Larger tests additionally require deterministic multi-block output and a
canonical-Huffman block when that representation is smaller than raw storage.

### TVG-0142

For incremental LZ78 plus Blocked Huffman testing, independently assemble the
80-byte stream prefix and each complete frame with the already tested frame
planner/encoder. Feed raw `ABABX` through two-byte frames and compare the
streaming encoder byte for byte against that assembly while limiting both
input and output to one byte per call. Decode the same stream under the same
schedule. Corrupt the first payload byte of the second frame and require only
the first `AB` frame to be published, followed by a sticky malformed-stream
error. Also construct both transforms directly from the profile sizing and
typed partition results so layout metadata is exercised as a construction
contract rather than only as an arithmetic result.

### TVG-0143

For the public-ABI completion matrix, use 64-byte raw frames, 64-byte entropy
blocks, a 64-entry LZ78 dictionary cap, and the exact eight-byte-per-raw-byte
token bound. Exercise empty input, every single byte, `0..255`, 257 zeroes, a
four-byte binary pattern, deterministic generated data, and lengths 63, 64,
and 65. For scheduling at larger scale, encode 193 generated bytes with
unlimited, `1/1`, `7/5`, and `13/17` input/output chunks and require
one identical stream. Locate the fourth frame from its serialized descriptor
and payload sizes; corrupt its sequence field, truncate its last byte, or add
one trailing byte. In each case require exactly 192 decoded bytes, an untouched
sentinel in the final destination byte, and a sticky positioned error.

### TVG-0144

For `marc --codec lz78-blocked-huffman`, generate the common repeated binary
CLI fixture and encode it with one-MiB raw frames, 65,536-symbol entropy
blocks, the eight-times token bound, 128 block views, and 65,536 phrase
entries. Decode and compare the complete file, repeat with empty input, reject
malformed and trailing streams without retaining either the destination or its
temporary file, and reject an attempt to overwrite an existing encoded file.
All transform creation must pass through the public C ABI workspace query.

### TVG-0145

For the specified LZW plus Blocked Huffman vector, encode raw `A` through the
already frozen standalone LZW grammar. Code 65 at initial width nine produces
packed bytes `41 00`, whose final seven high bits are zero LZW padding. Select
entropy block size two. The mandatory Blocked Huffman size rule chooses raw
storage, producing one 16-byte descriptor with symbol count and payload size
two, no model, raw flag one, and eight entropy-valid bits. Prepend a generic
frame header declaring raw size one, dictionary and compressed sizes two, one
block, and 16 descriptor bytes. The specified frame is exactly 74 bytes.

### TVG-0146

The frame planner and encoder reproduce this vector byte for byte, and the
validator recovers the packed bytes before invoking the ordinary LZW validator.
A separate vector crossing the 9-to-10-bit LZW width boundary, with an entropy
block boundary chosen inside the corresponding packed-code byte sequence, so
neither layer can accidentally treat entropy blocks as code boundaries. The
implemented validator uses the documented 291-byte packed vector, splits it
into thirty raw entropy blocks of at most ten bytes, and derives 259 zero bytes.
Corrupt LZW padding only after valid entropy reconstruction and require
transactional rejection before raw publication.

For encoder scheduling, encode raw `AABABCABC` with two-byte entropy blocks.
Plan once, encode twice from the same raw input and caller-owned typed
workspace, and require identical complete frames. Entropy-decode into a
distinct staging region, validate the packed code schedule, and require the
ordinary LZW decoder to reproduce all nine bytes. For raw `AA`, independently
withhold the single required encoder entry, the three-byte staging region, and
one byte of final serialized capacity; each rejection occurs before modifying
the affected destination. Set the aggregate limit to one byte below the
encoder-entry-plus-staging total and require the combined planner to reject it
even though each standalone region fits.

### TVG-0147

For profile sizing with original size 17, ten-byte frames, maximum LZW width
16, and 16-byte entropy blocks, the largest frame reserves 20 packed bytes,
two descriptors, nine encoder entries, and `56 + 32 + 20` serialized bytes.
For decoder layout with 128 packed bytes and the minimum nine-bit code width,
at most 113 whole codes fit and therefore at most 112 phrase entries are
required. Place four Blocked Huffman views first, align the phrase table, and
require a partition round trip to reproduce both exact spans. Increment the
recorded phrase offset or shift the base address by one byte and require
rejection without exposing either typed view.

### TVG-0148

For incremental LZW plus Blocked Huffman testing, independently assemble the
80-byte stream prefix and three complete frames for raw `ABABX` using two-byte
raw frames and four-byte entropy blocks. The first two frames each contain
three packed LZW bytes and serialize to 75 bytes; the final one-byte frame
contains two packed bytes and serializes to 74 bytes. Feed both streaming
transforms one input and output byte at a time and require the encoder to match
this oracle byte for byte and the decoder to recover all five raw bytes.

Change the high padding bits in the second frame's final packed LZW byte only
after its raw Blocked Huffman descriptor. A whole-stream decoder call may
publish the first `AB` frame, but must leave the third output byte untouched,
return malformed-stream at the stable encoded position, and repeat the same
terminal error. Independently withhold the one required LZW phrase entry,
truncate the final encoded byte, request `ResetBlock`, and finish the encoder
before all declared raw input arrives; require bounded, atomic rejection in
each case.

### TVG-0149

For the first pure-C ABI fixture, use the same raw `ABABX`, two-byte frames,
four-byte entropy blocks, and maximum 16-bit LZW width. The requirements query
must report two primary encoder bytes and 80 secondary bytes: four packed-byte
staging bytes plus a worst-case 76-byte frame. Encoding through the public
factory produces the already derived 304-byte stream. Under decoder limits of
four packed bytes, two raw bytes, one entropy block, and 512 aggregate bytes,
the query reports 568 primary bytes and six secondary bytes. Decode and compare
all five bytes, then reject one missing secondary byte, a one-byte-misaligned
views region, and a nonzero reserved field.

### TVG-0150

For public completion, use 64-byte raw frames and entropy blocks, 9-bit LZW,
the 128-byte packed-code bound, two entropy descriptors, and 256 local
dictionary entries. Exercise empty input, every single byte, `0..255`, 257
zeroes, a four-byte binary pattern, deterministic generated data, and lengths
63, 64, and 65. Encode 193 generated bytes with unlimited, `1/1`, `7/5`, and
`13/17` input/output chunks and require one stream. Locate the fourth frame,
then corrupt its sequence, truncate its final byte, or append trailing data.
Require exactly 192 decoded bytes, an untouched final sentinel, and a sticky
positioned error. Separately require empty and one-byte encoder profiles to
publish zero view bytes with alignment one.

### TVG-0151

For CLI composition, run the common binary fixture through the exact selector
`lzw-blocked-huffman` with one-MiB raw frames, 65,536-symbol entropy blocks, a
two-byte-per-raw-byte packed bound, at most 32 blocks, 65,280 additional LZW
entries, and the 64-MiB aggregate policy. Obtain all three workspace regions
from the public C ABI query. Require ordinary and empty round trips, malformed
and trailing-data rejection, overwrite refusal, and removal of the temporary
destination after every failed decode.

### TVG-0152

Run one `lzw-blocked-huffman` benchmark smoke iteration over `README.md` with
the identical CLI profile bounds. Require a successful public-C-ABI round trip
before timing, sufficient complete-stream capacity for raw entropy fallback,
and output of ratio, encode/decode throughput, each direction's three workspace
extents, and the larger aggregate workspace.

### TVG-0153

For the specified LZD plus Blocked Huffman vector, encode raw `A` through the
frozen LZD grammar to terminal token `41 00 00 00 FF FF FF FF`. With entropy
block size eight, independently select the Blocked Huffman raw form: one
16-byte descriptor followed by the unchanged token. Prepend the generic frame
header declaring raw size one, dictionary size eight, compressed payload eight,
one block, and 16 descriptor bytes; require the exact documented 80-byte frame.

For sizing, check raw sizes zero, one, two, and odd/even boundaries. Require
token staging `8*ceil(F/2)`, phrase records `min(floor(F/2), maximum_entries)`,
an expansion stack one larger than the admitted phrase count, and entropy block
count `ceil(staging/E)`. Future decoder tests must reject entropy failure before
staging publication, a non-multiple-of-eight reconstructed token region,
invalid or forward references, a nonterminal absent right, short block views,
phrase or stack workspace, aggregate-limit overflow, and short raw output
without publishing any byte from the frame.

### TVG-0154

The complete-frame validator now consumes that exact 80-byte `A` vector and
recovers the eight token bytes before the ordinary LZD validator confirms the
one-byte raw extent. Require every proper truncation and one appended byte to
fail, and independently withhold a block view, staging byte, or required phrase
record. The one-byte vector fixes the exact zero-phrase workspace rule; a
separate raw `AB` pair fixes the one-record boundary.

Change the reconstructed left reference to 256 to require an LZD grammar error
only after successful entropy decoding. Corrupt an entropy descriptor to
require the controller or entropy error before dictionary validation. Decode
into a sentinel-filled destination and require exact `A` publication only on
success; withhold raw capacity or the expansion-stack entry and require the
sentinel to remain unchanged. Finally set each individual region within its
local cap but the checked validation or decode aggregate one byte too small,
and reject an otherwise valid frame without publication.

### TVG-0155

For encoding, plan raw `A` with zero encoder records and eight staging bytes.
Require one token, zero admitted dictionary entries, one raw entropy block, 16
descriptor bytes, eight payload bytes, and the exact specified 80-byte frame.
Encode twice through a seven-byte entropy block boundary for raw `AABABCABC`;
the boundary must split the fixed-width token representation without changing
either parse, and both frames must be byte-identical and decode to the source.
Encode 1,024 deterministically generated bytes as one entropy block. The input
must leave enough LZD reference pairs for their zero-heavy high bytes to repay
the 272-byte canonical descriptor; require a payload smaller than the staged
token region, canonical flags, and a complete round trip. This separately
exercises the non-raw block path. A long zero run is unsuitable for this test
because LZD itself reduces 1,024 zero bytes to only ten tokens, correctly making
Blocked Huffman's raw fallback smaller than its model overhead.

For raw `AB`, independently withhold its one required encoder record and its
eight staging bytes. In the first case staging remains sentinel-filled. Supply
both regions but one byte less than the planned frame destination and require
the complete destination to remain sentinel-filled. Set the encoder-record plus
staging aggregate one byte below its exact requirement and reject it. Finally,
test empty input and a three-byte input against a two-byte frame declaration;
the latter receives its full 16-byte staging bound so frame extent is the only
remaining failing condition.

### TVG-0156

For profile sizing with original size 17, ten-byte frames, and 16-byte entropy
blocks, reserve 40 staged token bytes, three worst-case raw block descriptors,
five encoder records, and `56 + 48 + 40` serialized bytes. With a seven-byte
final short frame and a two-entry freeze limit, reserve 32 staged bytes and two
records. One-byte input must retain eight staging bytes but use zero opaque
encoder bytes and neutral alignment one; empty input reserves no frame-local
region.

For decoder limits of 128 staged bytes, 64 raw bytes, four blocks, and ten
dictionary entries, require ten phrase records and eleven expansion references.
Place blocks, phrases, and expansion references in that order with independently
checked alignment. Lower the raw frame limit to ten while raising the dictionary
limit and require five phrases plus six expansion references. Increment either
recorded offset, shorten storage by one byte, or shift its base address by one
byte and reject the partition without exposing any typed view.

### TVG-0157

For incremental composition, encode raw `ABABX` as two two-byte frames and one
one-byte final frame, with four-byte entropy blocks. Every eight-byte LZD token
is split across two raw Blocked Huffman blocks, so each frame is 96 bytes and
the complete stream is the 80-byte prefix plus 288 frame bytes. Feed encoder
and decoder one input and output byte at a time and require byte identity with
the complete-frame oracle and exact raw recovery.

Change the first reconstructed reference in the second frame to 256. Require
only the first `AB` frame to be published, leave the third destination byte
untouched, and make the positioned malformed-stream error sticky. Independently
withhold phrase and expansion workspaces, truncate the final byte, request
`ResetBlock`, end the encoder prematurely, and exercise an empty stream. Drain
the prefix, flush after one raw byte, and require the partial frame to remain
open. Set both encoder and decoder aggregate limits one byte below their actual
complete-frame regions and require `limit_exceeded` at the streaming boundary.

### TVG-0158

Expose the same `ABABX` stream through the public C ABI. Configure two-byte
frames, four-byte entropy blocks, 65,536 maximum LZD entries, and two entropy
blocks per frame. Require the requirements query to produce directly usable
three-region storage, encode exactly 368 bytes, decode the five input bytes,
and reject a one-byte-short secondary region, a deliberately misaligned opaque
region, and a nonzero reserved field before publishing a transform handle.

### TVG-0159

For public completion, use 64-byte frames, 64-byte entropy blocks, a 32-entry
LZD dictionary, and the corresponding 256-byte maximum token region. Round-trip
empty input, every one-byte value, all byte values, 257 zero bytes, a 259-byte
binary pattern, 513 deterministic generated bytes, and generated lengths 63,
64, and 65. Encode a 193-byte four-frame fixture repeatedly and with chunk
pairs `(1,1)`, `(7,5)`, and `(13,17)`; every archive must be identical.

Locate the fourth frame from its serialized descriptor and payload extents.
Independently alter its sequence number, remove the stream's final byte, and
append one trailing byte. Require all three decodes to publish exactly the
first 192 bytes, preserve a sentinel in the final destination byte, and return
the same positioned malformed-stream error on a repeated terminal call.

### TVG-0160

For the bounded fuzz boundary, truncate every supplied case to 8,192 bytes and
preallocate 4,280 encoded-frame bytes, 4,096 reconstructed token bytes, 1,024
raw frame bytes, eight entropy views, 512 LZD phrase records, 513 expansion
references, and 4,096 final output bytes. Begin with the five bytes `MARC` plus
newline. Vary input chunks from one through seventeen bytes and output chunks
from one through nineteen bytes using current input bytes, validate every
process result, and abort on an impossible starvation state or call exhaustion.

### TVG-0161

For the CLI adapter, repeat `ABRACADABRA-0123456789` plus newline 320 times,
encode and decode with explicit `--codec lzd-blocked-huffman`, and compare the
restored file byte for byte. Re-run encode against the existing archive and
require overwrite rejection. Decode a hand-authored non-marc input and an
otherwise valid archive with one appended byte; both must fail without leaving
the requested destination or its `.tmp` staging path. Separately round-trip an
empty file.

### TVG-0162

For benchmark smoke, run `marc_benchmark lzd-blocked-huffman README.md 1`.
Require the adapter to encode once, decode and byte-compare before timing, then
complete one encode and one decode measurement. Its output must identify the
selected codec and report input and encoded sizes, ratio, both elapsed times
and throughputs, encoder and decoder primary/secondary/views workspace bytes,
and the larger three-region sum. Do not freeze machine-dependent measurements.

### TVG-0163

For the specified LZMW plus Blocked Huffman vector, encode raw `A` through the
frozen LZMW grammar to literal reference `41 00 00 00`. With entropy block size
four, independently select the Blocked Huffman raw form: one 16-byte descriptor
followed by the unchanged token. Prepend the generic frame header declaring raw
size one, dictionary size four, compressed payload four, one block, and 16
descriptor bytes; require the exact documented 76-byte frame.

For sizing, check raw sizes zero, one, two, and larger boundaries. Require token
staging `4F`, phrase records `min(max(F-1, 0), maximum_entries)`, an expansion
stack one larger than the admitted phrase count for a nonempty frame, and
entropy block count `ceil(staging/E)`. Future decoder tests must reject entropy
failure before staging publication, a non-multiple-of-four reconstructed token
region, forward or unavailable references, phrase-length overflow, short block
views, phrase or stack workspace, aggregate-limit overflow, and short raw
output without publishing any byte from the frame.

### TVG-0164

For the complete-frame decoder, require the documented 76-byte literal frame
to reconstruct reference 65 and raw `A`. Independently construct raw `AB` as
references 65 and 66 in one eight-byte entropy block; require one generated
phrase record `{65, 66, 2}`, two stack entries, and exact raw output. Reject
every strict prefix and one trailing byte. Check short block-view, token,
phrase, stack, and raw regions separately.

Keep an output sentinel unchanged when descriptor control fails, when token
reference 256 appears before any entry exists, when a valid raw entropy block
reconstructs three bytes rather than a multiple of four, and when aggregate
workspace is one byte short. The malformed descriptor must fail before token
staging changes; dictionary failures may change staging but must not publish
raw bytes. Reject a mismatched pipeline before parsing its frame body.

### TVG-0165

For complete-frame encode, plan raw `A` with no phrase spans and four staging
bytes; require one token, zero generated entries, one raw entropy block, and
the exact documented 76-byte frame. Encode a multi-reference input twice with
entropy block size three so boundaries split four-byte references; require
identical frames and a complete decode round trip.

Independently encode 1,024 deterministic pseudo-random bytes in one entropy
block and require a 256-byte canonical model, a payload smaller than the token
region, a Huffman rather than raw descriptor, and exact round trip. Keep token
staging unchanged when phrase-span workspace is short. Keep serialized output
unchanged when one byte short. Check a staging shortage, phrase-span-plus-token
aggregate one byte short, empty frame rejection, and input longer than the
contextual frame extent.

### TVG-0166

For the combined workspace profile, use original size 17, frame size 10, and
entropy block size 16. Require 10 raw bytes, 40 token bytes, three raw-fallback
entropy blocks, nine encoder phrase spans, and the exact encoded-frame and
aggregate extents. Check a configured dictionary freeze, a one-byte frame, an
empty stream, a block-count limit, and an aggregate one byte short.

For decoder layout, independently align maximum Blocked Huffman views, LZMW
phrase records, and 32-bit expansion references in one opaque region. Derive
phrase count from the maximum serialized token count minus one and the
dictionary-entry limit, not from raw frame size. Verify every returned span and
non-overlap, exact offsets and total bytes, short and misaligned storage,
tampered requirements, invalid limits, and stable public error mapping.

### TVG-0167

For combined frame streaming, split raw `ABABX` into contextual frames of two,
two, and one byte. Build the oracle from the stream prefix and complete-frame
encoder. Require a one-byte-input/one-byte-output streaming encoder to match it
exactly and the corresponding decoder to reproduce the raw bytes. Repeated
calls after completion must retain `EndOfStream`.

Corrupt the first reference of the second frame to unavailable reference 256.
Decode in one call and require only the first frame's `AB` to be published; the
second frame contributes no byte and the malformed error remains sticky.
Separately test the exact empty prefix, premature final input, every required
workspace class, a one-byte truncation, unsupported `ResetBlock`, a partial
frame `Flush` that preserves input, and encoder and decoder aggregate limits
one byte below the actual active-frame requirement.

### TVG-0168

For the pure-C combined factory test, encode `ABABX` as frames of two, two, and
one raw byte with four-byte entropy blocks. Require the 80-byte prefix and
three complete frames to total 348 bytes, then decode that exact stream through
a separately constructed transform and compare all five bytes. The encoder
query must report 2 primary bytes and 104 secondary bytes; the bounded decoder
query must report 568 and 10 respectively. Reject a secondary region one byte
short, a deliberately misaligned opaque region when alignment exceeds one, and
a nonzero reserved field.

### TVG-0169

For public completion, generate frames of 64 raw bytes and entropy blocks of
64 canonical-reference bytes. Round-trip empty input, each possible one-byte
input, the ordered byte alphabet, 257 zero bytes, a 259-byte `00 ff 55 aa`
pattern, deterministic 513-byte pseudo-random data seeded with `c001d00d`, and
deterministic lengths 63, 64, and 65. Re-encode every case and require exact
bytes. For a 193-byte input seeded with `6d617263`, compare unchunked processing
with `(1,1)`, `(7,5)`, and `(13,17)` input/output chunks.

Generate a separate 193-byte stream with seed `13579bdf`. Locate its fourth
frame from the serialized descriptor and payload extents. Flip its sequence
field, truncate its last byte, and append one trailing zero as independent
cases. Each decode must report malformed input after publishing exactly the
first 192 bytes; the sentinel final output byte must remain unchanged and the
same error and position must repeat on the next call.

### TVG-0170

For permanent combined-fuzz regressions, encode the single-frame raw input
`ABABX` with a four-byte entropy block and an explicit four-entry LZMW policy.
Require every proper prefix of the resulting canonical stream to fail without
changing a five-byte `a5` output sentinel. Independently overwrite all generic
frame length fields with `ff`, and replace the second raw entropy reference by
little-endian 256 before LZMW validation can admit it. Both mutations must fail
atomically and repeat the same sticky error category.

Seed the coverage-guided target with the hand-authored five bytes `MARC\n`.
Copy that seed into an ignored build corpus before a bounded campaign so new
mutations never modify the reviewed repository corpus.

### TVG-0171

For CLI integration, generate the existing deterministic
`ABRACADABRA-0123456789\n` fixture repeated 320 times. Encode and decode with
explicit `--codec lzmw-blocked-huffman`, compare the restored bytes, and repeat
the same lifecycle for an empty file. Require a second encode to refuse the
existing output. Decode `not-a-marc-stream` and a valid archive with one
trailing `x`; both must fail and leave neither the requested output nor its
`.tmp` staging path.

### TVG-0172

For benchmark smoke, run
`marc_benchmark lzmw-blocked-huffman README.md 1`. Require a verified encode and
decode before measurement and the standard codec name, input and encoded byte
counts, ratio, encode/decode seconds and MiB/s, all six direction-specific
workspace extents, and peak caller-reserved workspace fields. Values are local
measurements rather than frozen performance thresholds.

### TVG-0173

For the first LZ77 plus Adaptive Huffman vector, begin with raw byte `41` and
derive the canonical 16-byte LZ77 Literal token directly from the published
token table. Independently simulate FGK variant 1 from one NYT root: emit path
bits root-to-leaf, literals LSB-first, select the highest permitted equal-weight
order leader, exchange positions and order numbers, and update weights only
after each complete symbol. The sixteen token bytes contain fifteen zeroes and
one `41`; require 31 bits, payload `00 ff 17 74`, and seven final valid bits.

As a separate implementation check, feed raw `41` through marc's existing LZ77
encoder, compare all sixteen token bytes with the hand token, then feed that
fixed token through the existing Adaptive encoder and compare the descriptor
and payload with the independent result. Serialize the generic frame header and
descriptor independently and require the exact documented 76-byte frame. This
test establishes the component boundary without asking the combined-profile
encoder to generate its own oracle.

### TVG-0174

For LZ77 plus Adaptive Huffman public completion, use 64-byte raw frames,
1,024-byte token staging, a 33,792-byte worst-case Adaptive payload, and a
65,536-byte aggregate limit. Round-trip empty input, every one-byte value, the
ordered byte alphabet, 257 zeroes, a 259-byte `00 ff 55 aa` pattern,
deterministic 513-byte pseudo-random data seeded with `c001d00d`, and lengths
63, 64, and 65. Re-encode every case and require exact bytes. For the
193-byte `6d617263` fixture, compare unchunked processing with `(1,1)`, `(7,5)`,
and `(13,17)` input/output chunks.

Generate a separate 193-byte stream with seed `13579bdf`. Locate its fourth
frame from the generic descriptor and payload extents. Flip its sequence field,
truncate its final byte, and append one zero as separate malformed cases. Each
decode must report a sticky malformed-stream result after publishing exactly
the first 192 bytes and must leave the sentinel final byte unchanged.

### TVG-0175

For permanent LZ77 plus Adaptive fuzz regressions, encode the single-frame raw
input `ABABX`. Require every proper prefix to fail without changing the
five-byte `a5` output sentinel. Independently replace all generic frame extent
fields with `ff`, then set the final reserved Adaptive descriptor byte to one;
both complete mutations must fail atomically and retain the same sticky error
category and byte position. Seed the sanitizer target only with the reviewed
five-byte `MARC\n` truncated magic and keep generated mutations in build
storage.

### TVG-0176

For CLI integration, generate the existing deterministic
`ABRACADABRA-0123456789\n` fixture repeated 3,200 times. Encode and decode with
the exact selector `lz77-adaptive-huffman`, compare the restored bytes, and
repeat the lifecycle for empty input. Require a second encode to refuse the
existing output. Decode `not-a-marc-stream` and a valid archive with one
trailing `x`; both must fail and leave neither the requested destination nor
its `.tmp` staging path.

### TVG-0177

For benchmark smoke, run
`marc_benchmark lz77-adaptive-huffman README.md 1`. Require a complete verified
round trip before timing and the standard codec name, byte counts, ratio,
encode/decode seconds and MiB/s, six direction-specific workspace extents, and
peak caller-reserved workspace fields. Treat the values as local observations,
not frozen performance thresholds.

### TVG-0178

For the first LZSS plus Adaptive Huffman vector, begin with raw byte `41` and
require the existing LZSS grammar to produce Literal token `00 41`. Independently
simulate a fresh FGK tree: emit literal `00`, then NYT path `0` and literal `41`,
all LSB-first. Require 17 payload bits, bytes `00 82 00`, and one valid final
bit. Serialize a descriptor for two symbols and three payload bytes, then an
exact 75-byte generic frame. Compare each component result separately before
comparing the full frame; no combined-profile encoder participates.

### TVG-0179

Exercise the first LZSS plus Adaptive Huffman decoder boundary with that exact
75-byte frame. Require every strict prefix and one trailing byte to fail, short
token staging and a one-byte-short aggregate workspace to fail before staging
mutation, and impossible `2F` token or 33-byte-per-token payload declarations
to fail before entropy decoding. Corrupt a reserved descriptor byte separately.
For the cross-layer negative case, independently Adaptive-encode the invalid
LZSS bytes `02 41`; require entropy decoding to succeed but complete LZSS token
validation to reject the unknown tag. Also reject a nonzero entropy block size
and an unexpected frame sequence.

### TVG-0180

For raw reconstruction, first decode the 75-byte hand vector into private raw
staging and prove that caller output remains untouched. Then use the publishing
entry point and require only byte `41` to be copied. Independently serialize a
Literal `A` followed by Match `(distance 1, length 5)`, Adaptive-encode the
eleven token bytes, and require output `AAAAAA`. A short raw staging region,
short output region, and aggregate workspace one byte below
descriptor-plus-payload-plus-token-plus-raw must fail before either staging
region changes. Descriptor corruption and entropy-valid invalid LZSS bytes must
leave both private raw staging and caller output unchanged.

### TVG-0181

For exact frame encoding, plan raw `41` into two token bytes, a 16-byte
descriptor, three payload bytes, and 75 serialized bytes, then require the
encoder to reproduce the independent DD-290 frame byte for byte. Encode six
`A` bytes twice; require the canonical eleven-byte Literal-plus-Match staging,
identical frames, and a complete combined round trip. A one-byte token staging
extent and a 74-byte serialized destination must preserve their sentinels.
Reject empty frame input and an input extent contradicting the stream header,
and reject aggregate workspace one byte below descriptor-plus-payload-plus-token.

### TVG-0182

For incremental decode, build a repository-owned known-size stream with
two-byte raw frames using the exact frame encoder. Feed and drain one byte at a
time and require identical raw output plus repeatable `EndOfStream`. Corrupt the
second frame's Adaptive descriptor and require only the first frame to be
published. Independently make encoded-frame, token, and raw staging one byte
short, then make their aggregate one byte short. Require bounded failures.
Declare token extent above `2F` and payload extent above 33 bytes per token in
otherwise parseable frame headers; require rejection before body collection.
Reject one-byte truncation, one trailing byte, and `ResetBlock`; accept the
empty 80-byte prefix, treat `Flush` during starvation as `NeedInput`, and retain
premature `EndInput` while draining the first frame before reporting truncation.

### TVG-0183

For incremental encode, construct the same five-byte, two-byte-frame stream
independently from explicit prefix and exact frame calls. Feed raw input and
drain output one byte at a time; require byte identity and repeatable
`EndOfStream`. Verify that `Flush` after one raw byte emits only the prefix and
does not close the partial frame. Supply all input with `EndInput` and one byte
of output, then re-present the unconsumed input without the flag and require
finish to remain latched. Make raw-frame storage and `2F` token staging one byte
short at construction, encoded-frame storage one byte short at preparation,
and the aggregate one byte short. Require empty-prefix success and rejection of
premature finish and `ResetBlock`.

### TVG-0184

For the LZSS plus Adaptive profile workspace, use a 65,536-byte largest frame
and require exact encoder extents 65,536 raw, 131,072 token, and 4,325,448
serialized bytes. Repeat with a 17-byte known input and require 17, 34, and
1,194 bytes, then require zero per-frame workspace for empty input. Reject a
1-MiB raw frame whose `2F` token worst case exceeds Adaptive's decoded-symbol
cap, a frame beyond the format cap, and invalid LZSS parameters. With decoder
limits 4,096 raw, 6,000 dictionary, and 8,192 internal bytes, require regions
8,248 encoded, 6,000 token, and 4,096 raw, plus stable core error mappings.

### TVG-0185

Exercise the LZSS plus Adaptive public C lifecycle with seven raw bytes
`41 42 41 42 41 42 58` and a seven-byte frame. Require exact encoder workspace
of 7 primary and 548 secondary bytes with no views region, complete encode, and
then decode with local limits that produce 4,152 primary and 21 secondary
bytes. Compare all seven restored bytes. Require a one-byte-short secondary
decoder workspace and a nonzero reserved field to fail before construction.

### TVG-0186

For LZSS plus Adaptive public completion, use 64-byte raw frames, 128-byte
token staging, a 4,224-byte maximum Adaptive payload, and a 65,536-byte
aggregate limit. Round-trip empty input, all one-byte values, the ordered byte
alphabet, 257 zeroes, a 259-byte `00 ff 55 aa` pattern, deterministic 513-byte
pseudo-random input seeded with `c001d00d`, and lengths 63, 64, and 65. Re-encode
every case exactly. For 193 bytes seeded with `6d617263`, compare unchunked,
`(1,1)`, `(7,5)`, and `(13,17)` input/output schedules.

Generate a separate 193-byte stream seeded with `13579bdf`, locate its fourth
frame from the generic descriptor and payload extents, and separately alter its
sequence, remove the final byte, and append one zero byte. Each decode must
return a sticky malformed-stream result after exactly 192 committed bytes,
preserve the final destination sentinel, and repeat the same byte/bit position.

### TVG-0187

For permanent LZSS plus Adaptive fuzz regressions, encode the five-byte raw
input `ABABX`. Require every proper stream prefix to fail without changing the
five-byte `a5` output sentinel. Independently overwrite all generic frame
extent fields with `ff`, then set the final reserved Adaptive descriptor byte
to one; both complete mutations must fail atomically with sticky error category
and byte position. Seed the sanitizer target only with the reviewed five-byte
`MARC\n` truncated magic and retain generated cases in build storage.

### TVG-0188

For CLI integration, generate the existing deterministic
`ABRACADABRA-0123456789\n` fixture repeated 3,200 times. Encode and decode with
the exact selector `lzss-adaptive-huffman`, compare the restored bytes, and
repeat the lifecycle for empty input. Require a second encode to refuse the
existing output. Decode `not-a-marc-stream` and a valid archive with one
trailing `x`; both must fail and leave neither the destination nor its `.tmp`
staging path.

### TVG-0189

For benchmark smoke, run
`marc_benchmark lzss-adaptive-huffman README.md 1`. Require the standard codec
name, input and encoded byte counts, complete-stream ratio, encode/decode
seconds and MiB/s, all six public direction-specific workspace extents, and
peak caller-reserved workspace. A complete public-ABI round trip must succeed
before measurement. Do not freeze local ratio or throughput as test thresholds.

### TVG-0190

For the first LZ78 plus Adaptive Huffman vector, encode raw `A` independently
through the frozen LZ78 grammar to obtain Pair token
`00 41 00 00 00 00 00 00`. Starting from a fresh FGK root, the first `00` is
an eight-bit unseen literal; unseen `41` contributes NYT path `0` and literal
`41` LSB-first; the remaining six known zero symbols each contribute path `1`.
Require 23 payload bits, bytes `00 82 7E`, descriptor symbol count 8, payload
size 3, final-valid-bit count 7, and the exact 75-byte frame in `docs/format.md`.
Build the test by invoking the standalone LZ78 and Adaptive encoders separately
and explicit generic serializers; do not use a combined-profile encoder as its
own oracle.

### TVG-0191

For the first complete-frame validator, accept the frozen 75-byte single-Pair
frame into eight token-staging bytes and one aligned phrase entry. Reject every
proper prefix and one trailing byte. Before entropy output, reject independently
short token and phrase workspaces and an aggregate limit one byte below the
exact descriptor-plus-payload-plus-token-plus-phrase sum. Reject zero final-bit
metadata, nonzero high padding, a non-multiple-of-eight token extent, wrong
sequence, and wrong pipeline. Independently Adaptive-encode a token with
forward phrase index 1 and require LZ78 phrase validation to reject it after
successful entropy decode. No test may treat failure as raw-output behavior;
this boundary has no raw output.

### TVG-0192

For transactional reconstruction, decode the frozen single-Pair frame into
three-byte sentinel-filled raw and public spans and require only byte zero to
become `A`. Independently assemble an Adaptive frame from standalone LZ78
tokens for `AABABCABC` and require iterative nested-phrase reconstruction to
match every raw byte. Reject empty raw staging and empty public output before
token staging changes. Set the aggregate limit one byte below descriptor,
payload, token, phrase, and raw staging combined. Corrupt the descriptor and
encode a forward phrase reference; both must leave raw and public sentinel
bytes unchanged.

### TVG-0193

For exact-frame encoding, plan raw `A` with one aligned encoder entry and eight
token-staging bytes. Require the planner to reproduce the fixed Pair token and
the descriptor, payload, and complete-frame extents from the independent
vector, then require the encoder to reproduce all 75 bytes exactly. Encode
`AABABCABC` twice and compare every serialized byte before decoding through the
transactional combined decoder. Independently shorten the encoder table, token
staging, and serialized output; the last failure must preserve every output
sentinel. Set the aggregate limit one byte below encoder entries plus token,
descriptor, and payload storage, and reject empty input and a raw extent that
does not match the stream header.

### TVG-0194

For the first streaming encoder, independently concatenate the serialized
80-byte prefix and exact-frame output for each two-byte frame of `ABABX`.
Feed the same raw bytes and drain output one byte at a time; require exact
identity with that oracle and stable repeated End Of Stream. Separately retain
`EndInput` while only one prefix byte can drain, verify that `Flush` after one
raw byte emits no short frame, and cover empty input. Reject short raw, token,
encoded-frame, and aligned-entry workspaces, an aggregate limit one byte below
raw plus tokens plus frame plus entries, premature EndInput, excess input,
unknown flags, and `ResetBlock`.

### TVG-0195

For the first streaming decoder, consume the independently concatenated
`ABABX` stream one encoded byte at a time and expose at most one raw byte per
call; require exact raw identity and stable End Of Stream. Repeat with the
complete encoded input but a one-byte output so EndInput must remain latched
while validated raw frames drain. Require every proper prefix to fail when
finished, append one trailing zero byte, and corrupt the second frame after the
first has committed; the latter may expose exactly the first two raw bytes and
must preserve the next output sentinel. Independently shorten encoded-frame,
token, raw, and phrase workspaces, set the aggregate limit one byte below the
exact frame-plus-token-plus-raw-plus-phrase sum, and reject ResetBlock and an
unknown flag. Empty input must accept exactly the 80-byte prefix.

### TVG-0196

For profile sizing, require the default 65,536-byte frame to report 524,288
token bytes, the `33D` worst payload plus fixed frame metadata, and the exact
bounded entry-table extent. Repeat with a short final frame and empty input.
For decoder sizing, cap raw and token storage by the profile and local limits,
derive phrases from complete eight-byte tokens, and retain the conservative
encoded-frame span. Partition aligned encoder and decoder storage, mutate each
record through the returned view, and reject changed counts, short spans, and
one-byte-misaligned subspans. Verify stable core error mapping.

### TVG-0197

For the first public C ABI round trip, encode `41 42 41 42 58` with two-byte
raw frames and two LZ78 entries, then decode the exact produced stream under
local two-byte raw, sixteen-byte token, and bounded serialized-frame limits.
Require the queried primary, secondary, views byte, and views alignment values
to match the internal profile formulas in both directions. Allocate only those
reported extents, retain every workspace through destruction, and compare all
five decoded bytes. Independently shorten secondary and views storage,
misalign a nonempty views region by one byte, set a reserved field, pass a null
output-handle pointer, and require deterministic invalid-argument results.

### TVG-0198

For public completion, use 64-byte frames and exercise empty input, every
one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte repeating binary
pattern, deterministic 513-byte generated data, and generated lengths 63, 64,
and 65 entirely through the C ABI. Encode each case twice and compare exact
bytes before decoding. For 193 generated bytes, compare unchunked output with
input/output schedules `(1,1)`, `(7,5)`, and `(13,17)`. Locate the fourth frame
by walking three generic frame extents from the 80-byte prefix; mutate its
sequence byte, remove its final byte, and append one zero separately. Each
decoder must publish exactly 192 bytes, preserve the last sentinel, and return
the same terminal status and error positions on repetition.

### TVG-0199

For bounded decoder fuzzing, retain only the five-byte repository-authored
truncated magic `MARC\n` as a seed. Build the canonical regression stream from
raw `ABABX` through the internal bounded encoder with five aligned entries,
40 token bytes, and fixed serialized-frame storage. Feed every proper prefix
to a fresh incremental decoder and require zero raw publication, then replace
the generic frame extent region with `FF` bytes and set the final Adaptive
descriptor reserved byte to one independently. Both malformed cases must
remain atomic and sticky. The fuzz entry point caps input, all byte buffers,
the 1,024-record phrase table, and calls before inspecting input metadata.

### TVG-0200

For the CLI boundary, use the public C factory with the 65,536-byte raw frame,
524,288-byte token ceiling, 17,301,504-byte Adaptive payload ceiling,
65,536-entry phrase limit, and 32-MiB aggregate policy. Run the common file
round trip and append one trailing byte to a valid archive; the latter must
fail without committing destination output.

For the benchmark boundary, reserve the complete encoded vector as the 80-byte
prefix plus `ceil(N / 65,536)` copies of the 56-byte header and 16-byte
descriptor plus `264N` payload bytes, using checked arithmetic. Query both
workspace layouts through the public ABI, require a byte-exact untimed round
trip, then execute one repository-input smoke iteration with no performance
threshold.

For interoperability schema 10, reuse the deterministic 8,193-byte fixture and
the exact twenty-entry schema-9 order, then append `lz78-adaptive-huffman`.
Generate and locally round-trip all twenty-one archives before writing codec
set `marc-cli-v10`. The verifier must enforce exact order, size, SHA-256,
foreign decode equality, and byte-identical local re-encoding. Derive schema 9
by removing only the last profile and continue the frozen conversion chain to
schema 1; swap the first two schema-10 entries and require order rejection.

### TVG-0201

For the specified LZW plus Adaptive Huffman vector, encode raw `A` through the
already frozen standalone LZW grammar. Code 65 at width nine produces packed
bytes `41 00`, whose final seven high bits are zero LZW padding. Feed both bytes
in order to a fresh FGK tree: `41` contributes its eight literal bits, then the
unseen zero byte contributes NYT path `0` and eight zero literal bits. Require
the 17-bit payload `41 00 00`, descriptor `(2, 3, 1, 0)`, and the exact 75-byte
frame recorded in `docs/format.md`. Assemble the test only from standalone LZW
and Adaptive encoders plus generic serializers; do not use a combined encoder.

### TVG-0202

For the first combined validator, reuse that independently assembled 75-byte
frame as the positive anchor. Exercise every proper prefix and one trailing
byte, undersized packed and phrase staging, the exact aggregate-workspace
threshold, a descriptor-count mismatch, nonzero Adaptive padding, a separately
Adaptive-encoded packed stream with nonzero LZW padding, sequence mismatch,
impossible packed extent, and an unsupported entropy variant. Require
pre-decode failures to leave sentinel packed staging unchanged, and require the
valid vector to expose exactly `41 00` and one validated code without producing
raw output.

### TVG-0203

For private reconstruction, decode the same independent single-code frame and
require raw `41` only after packed validation. Independently encode the
repository-owned raw sequence `ABABABA` through standalone LZW and Adaptive
helpers, then require the combined private-staging decoder to reproduce it.
Pass zero raw capacity and an aggregate limit admitting validation bytes but
not the one-byte raw span; both must fail before changing sentinel packed or
raw storage. Corrupt the Adaptive descriptor and require private raw staging to
remain unchanged.

### TVG-0204

For transactional publication, decode the independent raw-`A` frame through
the combined API and require both private staging and output to become `41`
only on success. Supply one output byte for the two-byte raw `AB` frame and
require packed staging, raw staging, and destination sentinel to remain
unchanged. Corrupt the
Adaptive payload padding and require both raw staging and destination to remain
unchanged. Finally publish the independently composed `ABABABA` multi-code
frame and require byte equality across raw staging and caller-visible output.

### TVG-0205

For encoding, plan raw `A` through the combined boundary and require zero LZW
encoder entries, one code, packed extent two, descriptor extent 16, payload
extent three, serialized extent 75, and staged bytes `41 00`. Encode it and
require exact equality with the independently assembled frame. Encode
repository-owned `ABABABA` twice from fresh calls, require byte equality, and
round-trip it through the combined decoder. Independently reject empty input,
an unexpected raw frame extent, an undersized LZW entry table, packed staging,
aggregate workspace limit, and serialized destination; pre-write failures must
leave their sentinel storage unchanged.

### TVG-0206

For streaming encoding, build a reference stream by serializing the ordinary
80-byte prefix and invoking the exact frame encoder over raw `AB`, `AB`, and
final `X` frames. Feed the same `ABABX` input and drain output one byte at a
time, requiring valid process results, exact reference equality, and sticky
`EndOfStream`. Separately require `Flush` after one raw byte to emit only the
prefix, preserve `EndInput` while one prefix byte drains, reject each
undersized raw/packed/encoded/entry region and the aggregate policy threshold,
emit the empty stream prefix, and reject premature finish, excess input,
`ResetBlock`, and an unknown flag.

### TVG-0207

For streaming decoding, generate `ABABX` through the exact frame encoder using
two-byte outer frames. Feed the complete stream with one-byte input and output,
and separately retain `EndInput` while validated raw frames drain; both must
reproduce the source and end stickily. Corrupt the second frame's Adaptive
descriptor and require only the first two raw bytes to publish before a sticky
error at a stable byte position. Reject every proper stream prefix and one
trailing byte, accept the exact empty prefix, reject each undersized encoded,
packed, raw, and phrase region plus the aggregate threshold, and reject
`ResetBlock` and an unknown flag.

### TVG-0208

For the LZW plus Adaptive bounded profile, use original size 17, a ten-byte
frame, and default maximum code width 16. Require a ten-byte raw region,
20 packed bytes, a 732-byte conservative serialized-frame region, and nine
typed encoder entries. Check the shorter seven-byte final frame and the
canonical empty layout. Reject independently short packed, Adaptive payload,
and aggregate limits. For decoder limits of 64 raw bytes, 128 packed bytes,
1,024 internal bytes, and 300 dictionary entries, require 1,080 serialized
bytes, 64 private raw bytes, 128 packed bytes, and 112 phrase entries. Partition
both typed regions, reject shortage, changed requirements, and misalignment,
and verify stable profile-error mapping plus invalid-limit rejection.

### TVG-0209

For the public LZW plus Adaptive C ABI, initialize encode defaults and require
a 65,536-byte frame plus maximum code width 16. Under two-byte frames, width
nine, a 16-byte packed limit, 528 payload bytes, 1,024 aggregate bytes, and 256
dictionary entries, require encode workspace extents of 2 primary and 174
secondary bytes. Encode `41 42 41 42 58`, then query decode requirements of
1,080 primary and 18 secondary bytes and reproduce the input through the C11
transform API. In both directions require a nonempty aligned opaque region.
Reject each one-byte-short workspace, a deliberately misaligned views region,
a null output-handle pointer, and a nonzero reserved field while preserving a
null transform on every factory failure.

### TVG-0210

For public completion, use 64-byte frames and exercise empty input, every
one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte repeating binary
pattern, deterministic 513-byte generated data, and generated lengths 63, 64,
and 65 entirely through the C ABI. Require zero encoder views for raw sizes
zero and one, because neither can generate an LZW dictionary entry. Encode
every case twice and compare exact bytes before decoding. For 193 generated
bytes, compare unchunked output with input/output schedules `(1,1)`, `(7,5)`,
and `(13,17)`. Walk three generic frame extents from the 80-byte prefix to find
the fourth frame; mutate its sequence byte, remove its final byte, and append
one zero separately. Each decoder must publish exactly 192 bytes, preserve the
last sentinel, and return identical terminal status and error positions when
called again.

### TVG-0211

For bounded decoder fuzzing, retain only the five-byte repository-authored
truncated magic `MARC\n` as a seed. Build the canonical regression stream from
raw `ABABX` through the internal bounded encoder with four aligned LZW entries,
ten packed bytes, and fixed serialized-frame storage. Feed every proper prefix
to a fresh incremental decoder and require zero raw publication, then replace
the generic frame extent region with `FF` bytes and set the final Adaptive
descriptor reserved byte to one independently. Both malformed cases must stay
atomic and sticky. The fuzz entry point caps input, all byte buffers, the
3,639-record phrase table, and total calls before inspecting input metadata.

### TVG-0212

For CLI integration, run the common file harness with the exact selector
`lzw-adaptive-huffman`. Encode enough repeated text to cross the 65,536-byte
raw-frame boundary, decode it through a fresh public transform, and compare the
restored file byte for byte. Append one trailing zero byte to the encoded file,
require decode failure, and require that neither the requested destination nor
its temporary staging path remains.

### TVG-0213

For benchmark integration, run
`marc_benchmark lzw-adaptive-huffman README.md 1`. Require a successful
pre-timing byte-exact round trip and a report naming the selected codec,
encoded size, ratio, directional elapsed time and throughput, all six queried
workspace extents, and the greater caller-owned workspace total. Apply no
ratio or throughput pass threshold.

### TVG-0214

For interoperability schema 11, reuse the deterministic 8,193-byte fixture and
the exact twenty-one-entry schema-10 order, then append
`lzw-adaptive-huffman`. Generate and locally round-trip all twenty-two archives
before writing codec set `marc-cli-v11`. The verifier must enforce exact order,
size, SHA-256, foreign decode equality, and byte-identical local re-encoding.
Derive schema 10 by removing only the last profile and continue the frozen
conversion chain to schema 1; swap the first two schema-11 entries and require
order rejection. As a local determinism check, cross-verify an MSVC-generated
bundle with ClangCL and a ClangCL-generated bundle with MSVC.

### TVG-0215

For the specified LZD plus Adaptive Huffman vector, encode raw `A` through the
already frozen standalone LZD grammar. The only token is the little-endian
reference pair `41 00 00 00 FF FF FF FF`, where the absent right reference is
legal because it is terminal and the left literal reaches the declared raw
extent. Feed all eight token bytes in order to a fresh FGK tree. Require the
37-bit payload `41 00 CC 3F 1D`, descriptor `(8, 5, 5, 0)`, and the exact
77-byte frame recorded in `docs/format.md`. Assemble the test only from the
standalone LZD and Adaptive encoders plus generic serializers; do not use a
combined encoder.

### TVG-0216

For the first combined LZD validator, reuse that independently assembled
77-byte frame as the positive anchor. Exercise every proper prefix and one
trailing byte, undersized token and phrase staging, the exact aggregate-
workspace threshold, an invalid Adaptive descriptor, nonzero entropy padding,
sequence mismatch, impossible and non-token-aligned dictionary extents, and an
unsupported entropy variant. Separately Adaptive-encode an absent-right token
that ends before the declared raw extent and a forward phrase reference; both
must reach LZD validation and fail with their stable grammar categories.
Require every pre-decode capacity or descriptor failure to preserve sentinel
token staging. The valid vector must expose exactly the eight canonical token
bytes and one validated token without producing raw output.

### TVG-0217

For private reconstruction, decode the same independent terminal-token frame
and require raw `41` only after token validation. Independently encode the
repository-owned raw sequence `ABABAB` through standalone LZD and Adaptive
helpers, then require the combined private-staging decoder to reproduce it
through two phrases and a three-reference expansion ceiling. Pass zero raw
capacity, zero expansion capacity, and an aggregate limit admitting validation
bytes but neither private extent; all must fail before changing sentinel token
or raw storage. Corrupt the Adaptive descriptor and require private raw staging
to remain unchanged.

### TVG-0218

For transactional publication, decode the independent raw-`A` frame through
the combined API and require both private staging and output to become `41`
only on success. Supply one output byte for the two-byte raw `AB` frame and
require token staging, expansion storage, raw staging, and destination sentinel
to remain unchanged. Corrupt Adaptive payload padding and require both raw
staging and destination to remain unchanged. Finally publish the independently
composed `ABABAB` phrase-reference frame and require byte equality across raw
staging and caller-visible output.

### TVG-0219

For exact-frame encoding, pass raw `A` to the combined planner and require the
frozen eight-byte terminal token, zero generated phrases, one token, the
16-byte descriptor, five-byte payload, and total 77-byte extent fixed by the
independent vector. Require the encoder to reproduce all 77 bytes exactly.
Encode a phrase-reference input twice from the same canonical staging and
require byte identity plus complete-frame round trip. Exercise insufficient
typed encoder workspace, token staging, serialized destination, aggregate
workspace, empty input, and unexpected frame extent; capacity failures must
leave their caller-visible sentinel regions unchanged.

### TVG-0220

For streaming encoding, use the five-byte input `ABABX` with two-byte outer
frames. Build the reference stream from the generic 80-byte prefix followed by
three independently planned and encoded complete frames. Feed and drain one
byte at a time and require byte identity plus sticky `EndOfStream`. Separately
flush after one partial raw byte, pass `EndInput` while only one prefix byte can
drain, and require the same reference stream. Exercise undersized raw, token,
typed-entry, and complete-frame storage plus an aggregate limit one byte below
the exact used total. Cover empty input, premature end, excess input,
`ResetBlock`, and an unknown flag with stable errors.

### TVG-0221

For streaming decoding, consume the same three-frame `ABABX` stream with one
input byte and one output byte per call and require exact raw equality and
sticky `EndOfStream`. Repeatedly pass `EndInput` while permitting only one raw
output byte and require it to remain retained through every validated-frame
drain. Corrupt the second frame's Adaptive descriptor and require only the
first frame's `AB` to be published, with the next output sentinel unchanged and
the same error position returned thereafter. Exercise every proper stream
prefix, one trailing byte, the empty prefix-only stream, undersized encoded,
token, raw, phrase, and expansion regions, an aggregate limit one byte below
the exact frame requirement, `ResetBlock`, and an unknown flag.

### TVG-0222

For the bounded profile, use original size 17 and frame size 10 to require a
10-byte raw frame, forty token bytes, `56+16+40*33` complete-frame bytes, and
five typed encoder entries. Exercise a short final frame, a frozen two-entry
dictionary, one-byte input with neutral entry alignment, and empty input.
Reject token, payload, and aggregate limits one below their required values.
For decoder limits of 64 raw bytes, 128 token bytes, and ten dictionary entries,
require ten phrase records and eleven expansion references. Partition aligned
opaque storage and verify the phrase base, expansion offset, non-overlap,
short-storage rejection, misalignment rejection, and altered-offset rejection.

### TVG-0223

For the public LZD plus Adaptive C ABI, initialize encode defaults and require
a 65,536-byte frame and 65,536 maximum entries. Under two-byte frames, eight
token bytes, a 264-byte payload ceiling, 512 aggregate bytes, and 65,536 entry
limits, require encode workspace extents of 2 primary and 344 secondary bytes.
Encode `41 42 41 42 58`, then query decode requirements of 568 primary and 10
secondary bytes and reproduce the input through the C11 transform API. Require
a nonempty aligned opaque region in both directions. Reject each one-byte-short
workspace, a deliberately misaligned views region, a null output-handle
pointer, and a nonzero reserved field while preserving a null transform on
every factory failure.

### TVG-0224

For public completion, use 64-byte frames and exercise empty input, every
one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte repeating binary
pattern, deterministic 513-byte generated data, and generated lengths 63, 64,
and 65 entirely through the C ABI. Require zero encoder views for raw sizes
zero and one, because neither can generate an LZD phrase entry. Encode every
case twice and compare exact bytes before decoding. For 193 generated bytes,
compare unchunked output with input/output schedules `(1,1)`, `(7,5)`, and
`(13,17)`. Walk three generic frame extents from the 80-byte prefix to find the
fourth frame; mutate its sequence byte, remove its final byte, and append one
zero separately. Each decoder must publish exactly 192 bytes, preserve the
last sentinel, and return identical terminal status and error positions when
called again.

### TVG-0225

For bounded decoder fuzzing, retain only the five-byte repository-authored
truncated magic `MARC\n` as a seed. Build the canonical regression stream from
raw `ABABX` through the internal bounded encoder with two aligned LZD entries,
24 token bytes, and fixed serialized-frame storage. Feed every proper prefix
to a fresh incremental decoder and require zero raw publication, then replace
the generic frame extent region with `FF` bytes and set the final Adaptive
descriptor reserved byte to one independently. Both malformed cases must stay
atomic and sticky. The fuzz entry point caps input, all byte buffers, 512
phrase records, 513 expansion references, and total calls before inspecting
input metadata.

### TVG-0226

For the `lzd-adaptive-huffman` CLI boundary, generate the common deterministic
multi-frame fixture by repeating the repository byte pattern 320 times. Encode
and decode it with explicit `--codec lzd-adaptive-huffman` and compare the
restored bytes exactly. Separately append one zero byte to the valid archive,
require decode failure, and require that neither the destination nor temporary
transaction file remains.

### TVG-0227

For benchmark smoke, run `marc_benchmark lzd-adaptive-huffman README.md 1`.
Require the tool's untimed public-C encode/decode verification to reproduce the
input exactly before it reports any measurement. Treat successful completion
and well-formed metric fields as coverage; record ratio and throughput as
observations rather than fixed expected values.

### TVG-0228

For interoperability schema 12, retain the deterministic 8,193-byte fixture and
the exact schema-11 archive order, then append `lzd-adaptive-huffman` once as
archive 23. Generate and locally decode all archives before writing
`manifest.json`; require codec set `marc-cli-v12`, exact order, sizes, SHA-256,
foreign decode equality, and byte-identical local re-encoding. Reorder two
entries and require rejection, then remove only archive 23 while converting to
schema 11 and verify every frozen schema down through schema 1.

### TVG-0229

For the specified LZMW plus Adaptive Huffman vector, encode raw `A` with the
standalone LZMW encoder and require canonical reference `41 00 00 00`. Feed
that exact four-byte span to a fresh standalone FGK encoder and require 20
payload bits, bytes `41 00 0C`, and descriptor `(4, 3, 4, 0)`. Serialize a
generic frame header for dictionary ID 6 variant 1 and entropy ID 1 variant 1,
append the descriptor and payload, and compare all 75 bytes with the format
document. Do not call a future combined codec while establishing this vector.

### TVG-0230

For the first complete-frame validator, accept that exact 75-byte frame and
require the private reference bytes, one token, zero generated phrases, and one
future expansion reference. Reject every proper truncation and one trailing
byte. Require short reference and phrase workspaces plus one-byte-short aggregate
workspace to fail before entropy output. Corrupt the descriptor and final
padding independently, then entropy-code a forward reference and a raw-size
mismatch to prove deterministic descriptor, entropy, and LZMW error precedence.
For raw `AB`, require the adjacent phrase record `(A, B, 2)`, one generated
entry, and a two-reference future expansion ceiling.

### TVG-0231

For private reconstruction, decode the 75-byte raw-`A` frame into separate
reference, expansion, and raw regions and require only private raw `41`.
Reject a missing raw byte, missing expansion reference, and aggregate workspace
that admits validation but not reconstruction before entropy output; preserve
all guarded regions. Encode raw `ABABAB`, require four references, three
adjacent generated phrases, four expansion references, and exact private raw
equality. Corrupt the descriptor and encode a forward reference independently;
both must leave private raw staging unchanged.

### TVG-0232

For transactional frame publication, decode the independent raw-`A` frame and
require both private staging and caller output to become `41` only on success.
For raw `AB`, provide an output one byte short and require reference staging,
expansion storage, private raw storage, and output sentinels all to remain
unchanged. Corrupt final Adaptive padding and entropy-code a forward LZMW
reference independently; both must preserve caller output. Finally publish the
complete `ABABAB` generated-phrase frame and require private and caller-visible
raw spans to match exactly.

### TVG-0233

For exact-frame encoding, pass raw `A` to the combined planner and require one
four-byte reference, zero generated phrases and typed encoder entries, the
16-byte descriptor, three-byte payload, and total 75-byte extent fixed by the
independent vector. Require the encoder to reproduce all 75 bytes exactly.
Encode raw `ABABAB` twice from the same canonical staging and require byte
identity plus complete-frame round trip. Exercise insufficient typed encoder
workspace, reference staging, serialized destination, aggregate workspace,
empty input, and unexpected frame extent; capacity failures must leave their
caller-visible sentinel regions unchanged.

### TVG-0234

For bounded streaming encoding, serialize the ordinary 80-byte known-size
prefix independently, split raw `ABABX` into configured two-byte outer frames,
and append the result of each exact-frame planner and encoder transaction.
Require the incremental encoder to reproduce that complete reference stream
with one-byte input and output. Separately retain `EndInput` while only one
prefix byte can drain, confirm nonterminal `Flush` leaves a one-byte partial
frame open, and reject short raw, reference, typed-entry, serialized-frame, and
aggregate workspaces plus premature/excess input and unsupported flags.

### TVG-0235

For bounded streaming decoding, feed that same independently assembled stream
with one-byte input and output and require exact raw `ABABX`. Repeat with the
complete input and `EndInput` on every call while allowing only one raw output
byte, proving retained finish across validated-frame drain. Reject every proper
prefix and one appended byte. Corrupt the second frame's Adaptive descriptor,
require only the first two raw bytes to be published, preserve the next output
sentinel, and require the identical sticky byte position on another call.
Separately reject each short encoded-frame, reference, private-raw, phrase, and
expansion region, a one-byte-short aggregate limit, and unsupported flags.

### TVG-0236

For the bounded profile, use original size 17 and frame size 10. Require encode
regions of 10 raw bytes, 40 reference bytes, `56+16+40*33` serialized bytes,
and nine LZMW encoder entries. Freeze the dictionary at two entries for a
seven-byte short frame, then require a one-byte frame and empty stream to expose
zero typed entries. Reject reference, payload, and aggregate ceilings one byte
below their requirements. For decode limits of 64 raw bytes, 128 reference
bytes, ten dictionary entries, and 1,024 aggregate bytes, require ten phrase
records, eleven expansion references, aligned nonoverlapping views, and an
encoded capacity of `56+1024`. Reject altered offsets, short and misaligned
opaque regions, inconsistent empty requirements, and invalid limits.

### TVG-0237

For the public C boundary, configure two-byte frames, eight reference bytes,
264 maximum payload bytes, 512 aggregate bytes, and 65,536 dictionary entries.
Require encode workspace extents of 2 primary and 344 secondary bytes. Encode
raw `41 42 41 42 58`, then require decode extents of 568 primary and 10
secondary bytes and reproduce the input through the C11 transform API. Require
a nonempty aligned opaque region in both directions. Reject each one-byte-short
workspace, a deliberately misaligned views region, a null output-handle pointer,
and a nonzero reserved field while preserving a null transform on every
factory failure.

### TVG-0238

For public completion, use 64-byte frames and exercise empty input, every
one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte repeating binary
pattern, deterministic 513-byte generated data, and generated lengths 63, 64,
and 65 entirely through the C ABI. Require zero encoder views for raw sizes zero
and one. Encode every case twice and compare exact bytes before decoding. For
193 generated bytes, compare unchunked output with input/output schedules
`(1,1)`, `(7,5)`, and `(13,17)`. Walk three generic frame extents from the
80-byte prefix to find the fourth frame; mutate its sequence byte, remove its
final byte, and append one zero separately. Each decoder must publish exactly
192 bytes, preserve the last sentinel, and return identical terminal status and
error positions when called again.

### TVG-0239

For bounded decoder fuzzing, retain only the five-byte repository-authored
truncated magic `MARC\n` as a seed. Build the canonical regression stream from
raw `ABABX` through the internal bounded encoder with four LZMW entry slots,
20 reference bytes, and fixed serialized-frame storage. Feed every proper
prefix to a fresh incremental decoder and require zero raw publication, then
replace the generic frame extent region with `FF` bytes and set the final
Adaptive descriptor reserved byte to one independently. Both malformed cases
must stay atomic and sticky. The fuzz entry point caps input, all byte buffers,
1,023 phrase records, 1,024 expansion references, and total calls before
inspecting input metadata.

### TVG-0240

For the `lzmw-adaptive-huffman` CLI boundary, generate the common deterministic
multi-frame fixture by repeating the repository byte pattern 320 times. Encode
and decode it with explicit `--codec lzmw-adaptive-huffman` and compare the
restored bytes exactly. Separately append one zero byte to the valid archive,
require decode failure, and require that neither the destination nor temporary
transaction file remains.

### TVG-0241

For benchmark smoke, run `marc_benchmark lzmw-adaptive-huffman README.md 1`.
Require the tool's untimed public-C encode/decode verification to reproduce the
input exactly before it reports any measurement. Treat successful completion
and well-formed metric fields as coverage; record ratio and throughput as
observations rather than fixed expected values.

### TVG-0242

For interoperability schema 13, retain the deterministic 8,193-byte fixture
and the exact schema-12 archive order, then append `lzmw-adaptive-huffman` once
as archive 24. Generate and locally decode all archives before writing
`manifest.json`; require codec set `marc-cli-v13`, exact order, sizes, SHA-256,
foreign decode equality, and byte-identical local re-encoding. Reorder two
entries and require rejection, then remove only archive 24 while converting to
schema 12 and verify every frozen schema down through schema 1.

### TVG-0243

For the first LZ77 plus Dynamic Range vector, begin with raw byte `41` and
derive the canonical 16-byte LZ77 Literal token directly from the published
token table. Independently run Dynamic Range variant 1 over those fixed bytes:
start all 256 frequencies at one, update the 32-bit range and 64-bit low in the
documented integer order, normalize below 2^24 with delayed carry, update the
model only after each byte, and perform exactly five termination shifts. The
payload must be `00 00 00 00 00 00 00 00 00 06 5c d6 5f 00 00 00`.

As a separate implementation check, require marc's existing LZ77 encoder to
reproduce the fixed token, then require the standalone Dynamic Range encoder to
reproduce the independently calculated payload and 16-byte descriptor. Serialize
the generic frame header and descriptor independently and require the exact
documented 88-byte frame. Do not use a future combined encoder to construct its
own oracle.

### TVG-0244

For the first LZ77 plus Dynamic Range validator tests, require the 88-byte hand
frame to decode into exactly the fixed 16-byte Literal token staging. Feed every
proper frame prefix and one trailing byte as separate strict failures. Before
allowing entropy output, test a 15-byte staging region, an aggregate limit one
byte below descriptor plus payload plus token staging, a payload extent of 38
bytes against the `2*16 + 5` ceiling, and a nonzero final descriptor reserved
byte; all must leave a `5a` staging sentinel unchanged. Separately replace the
canonical range initialization byte with one, and range-code an invalid LZ77
tag `ff`; require stable entropy and dictionary validation errors respectively.

### TVG-0245

For private LZ77 plus Dynamic Range reconstruction, decode the 88-byte hand
frame into a three-byte `5a` raw sentinel and require only byte zero to become
`41`. Build a separate canonical Literal `A` plus terminal distance-1 length-4
token stream, range-code it through the standalone entropy encoder, and require
five reconstructed `41` bytes. Supply no raw capacity and then an aggregate
limit one byte below descriptor plus payload plus token plus raw staging;
require both failures before token or raw mutation. Range-code tag `ff` and
require dictionary validation failure with the raw sentinel unchanged.

### TVG-0246

For transactional LZ77 plus Dynamic Range publication, decode the same hand
frame into private raw and caller output spans filled with `5a`; require only
the declared first byte to become `41`. Publish all five `41` bytes from the
overlapping-match frame. Supply no caller output capacity and require token and
raw staging to remain unchanged. Corrupt the range descriptor and separately
range-code invalid LZ77 tag `ff`; require both failures to preserve every
caller-output sentinel, and require the invalid-token case to preserve private
raw staging as well.

### TVG-0247

For exact LZ77 plus Dynamic Range encoding, plan raw `41` into a 16-byte
private token region and require raw, token, descriptor, payload, and serialized
extents `1`, `16`, `16`, `16`, and `88`. Encode through the combined boundary
and compare every byte with the independently assembled frame. Encode five
`41` bytes twice, require identical streams, and decode one through the
transactional boundary. Reject 15-byte token staging without changing its
`5a` sentinels, reject 87-byte serialized output without changing its
sentinels, reject empty and unexpected raw frame extents, and reject an
aggregate ceiling one byte below descriptor plus payload plus token staging.

### TVG-0248

For bounded streaming LZ77 plus Dynamic Range encoding, use raw `ABABX` with
two-byte frames. Independently concatenate the stream prefix and three outputs
from the exact-frame encoder, then require the streaming encoder to reproduce
that reference with one-byte input and output. Feed one byte with `Flush` and
require only the prefix, then finish with the remaining input and require the
same complete reference. Reject short raw, token, and serialized workspaces,
and an aggregate ceiling one byte below raw plus canonical tokens plus the
serialized frame. Verify empty input, premature `EndInput`, unsupported
`ResetBlock`, and stable repeated `EndOfStream`.

### TVG-0249

For bounded streaming LZ77 plus Dynamic Range decoding, produce the `ABABX`
three-frame stream through the bounded encoder and recover it with one-byte
input and output. Corrupt the second frame's final descriptor byte and require
only raw `AB` from the first frame to be published, all later output sentinels
to remain `5a`, and the error to stay sticky. Reject each one-byte-short
serialized-frame, token, and raw region, plus an aggregate ceiling one byte
below their sum. Reject final-byte truncation, trailing data, and `ResetBlock`;
accept the empty stream, treat empty `Flush` as input starvation, and prove
that premature `EndInput` fails only after already validated raw bytes drain.

### TVG-0250

For the bounded LZ77 plus Dynamic Range profile, require the default 65,536-byte
raw frame to expose encoder extents 65,536 raw, 1,048,576 token, and 2,097,229
serialized bytes. For original size 17 require 17, 272, and 621 bytes; empty
input requires zero for all three. Lower the compressed-payload limit below the
17-byte worst case, exceed the 2^20 raw-frame cap, and supply invalid LZ77
parameters independently. For decoder limits of 4,096 raw bytes, 6,000 token
bytes, and 8,192 aggregate bytes, require 8,248 serialized, 6,000 token, and
4,096 private-raw bytes. Verify every profile error maps to its stable core
category.

### TVG-0251

For the first public C boundary, initialize the encode config, set original and
frame size to raw `ABABABX`, query two byte regions with no views workspace,
create the transform, and finish the encode in one public process call. Destroy
it, initialize decode with explicit local limits, query again, create, and
recover the exact seven bytes through the same C11 lifecycle. Require decode
secondary storage to equal `16F + F`, reject a one-byte-short secondary region,
and reject a nonzero reserved field while keeping the transform null.

### TVG-0252

For public completion, use 64-byte frames and exercise empty input, every
one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte repeating binary
pattern, deterministic 513-byte generated data, and generated lengths 63, 64,
and 65 entirely through the C ABI. Encode every case twice and compare exact
bytes before decoding. For 193 generated bytes, compare unchunked output with
input/output schedules `(1,1)`, `(7,5)`, and `(13,17)`. Walk three generic
frame extents from the 80-byte prefix to find the fourth frame; mutate its
sequence byte, remove its final byte, and append one zero separately. Each
decoder must publish exactly 192 bytes, preserve the last sentinel, and return
identical terminal status and error positions when called again.

### TVG-0253

For the bounded LZ77 plus Dynamic Range decoder fuzz boundary, seed only the
five-byte truncated magic `4d 41 52 43 0a`. Cap fuzzer input at 8,192 bytes,
use fixed arrays for all parser, dictionary, raw, and output storage, and drive
both the direct complete-frame validator and incremental decoder under a fixed
call ceiling. Independently encode raw `ABABX`, reject every proper prefix,
overwrite all generic frame extent fields with `ff`, and set the final Dynamic
Range descriptor byte to one. Each deterministic failure must publish zero
bytes, leave every `a5` output sentinel unchanged, and repeat the same error
category and byte position.

### TVG-0254

For CLI integration, generate the deterministic
`ABRACADABRA-0123456789\n` fixture repeated 3,200 times. Encode and decode with
the exact selector `lz77-dynamic-range`, compare restored bytes, and repeat the
lifecycle for empty input. Require a second encode to refuse the existing
output. Decode `not-a-marc-stream` and a valid archive with one trailing `x`;
both must fail and leave neither the requested destination nor its `.tmp`
staging path.

### TVG-0255

For benchmark smoke, run
`marc_benchmark lz77-dynamic-range README.md 1`. Require a complete verified
round trip before timing and the standard codec name, byte counts, ratio,
encode/decode seconds and MiB/s, six direction-specific workspace extents, and
peak caller-reserved workspace fields. Treat values as local observations, not
frozen performance thresholds.

### TVG-0256

For interoperability schema 14, retain the deterministic 8,193-byte fixture
and exact schema-13 archive order, then append `lz77-dynamic-range` once as
archive 25. Generate and locally decode all archives before writing
`manifest.json`; require codec set `marc-cli-v14`, exact order, sizes, SHA-256,
foreign decode equality, and byte-identical local re-encoding. Reorder two
entries and require rejection, then remove only archive 25 while converting to
schema 13 and verify every frozen schema down through schema 1.

### TVG-0257

For the second Dynamic Range composition vector, begin with raw byte `41` and
derive the canonical LZSS Literal `00 41` directly from the published token
grammar. Independently apply Dynamic Range variant 1 to those two fixed bytes:
start all 256 frequencies at one, divide the 32-bit range before updating the
64-bit low, normalize below 2^24 with delayed carry, increment only the
just-coded symbol, and perform exactly five termination shifts. The payload
must be `00 00 41 be 41 7c 00`.

As a separate implementation check, require the standalone LZSS encoder to
reproduce `00 41`, then require the standalone Dynamic Range encoder to
reproduce the seven-byte payload and descriptor with symbol count 2 and payload
size 7. Serialize the generic header and descriptor independently and require
the exact documented 79-byte frame. Do not use a future combined encoder to
construct its own oracle.

### TVG-0258

For the first LZSS plus Dynamic Range validator boundary, feed the independent
79-byte single-Literal frame into two bytes of caller-owned private staging and
require exact `00 41`. Reject every proper frame prefix and one appended byte.
Reject one-byte-short token staging, a one-byte-short aggregate workspace,
`S > 2F`, `P > 2S + 5`, a nonzero descriptor reserved byte, a wrong sequence,
an unsupported entropy block size, and a stream frame size above 2^23 before
unsafe mutation.

Build malformed token payloads only with the standalone Dynamic Range encoder.
After one valid Literal, encode an unknown tag and require LZSS token index 1
and byte offset 2. Separately encode a four-byte prefix of a Match token and
require a truncated-token error at token index and byte offset zero. These
checks exercise the variable token grammar without using a future combined
encoder as an oracle.

### TVG-0259

For private LZSS plus Dynamic Range reconstruction, decode the independent
single-Literal frame into a three-byte sentinel-filled raw span and require
only the first byte to become `41`. Construct canonical LZSS bytes for Literal
`A` followed by Match distance 1 and length 5, range-code those bytes with the
standalone entropy encoder, and require six private `A` bytes.

Before entropy output, reject zero raw capacity and preserve token sentinels.
Set the aggregate limit one byte below descriptor plus payload plus tokens plus
raw and preserve both token and raw sentinels. Corrupt the descriptor and,
separately, range-code a valid Literal followed by an unknown tag; neither case
may mutate private raw staging. No caller-visible output participates in these
tests.

### TVG-0260

For transactional LZSS plus Dynamic Range publication, decode the independent
single-Literal frame into private raw and a three-byte sentinel-filled output;
require only output byte zero to become `41`. Publish the distance-one,
length-five overlap vector into a seven-byte output and require six `41` bytes
while preserving the final sentinel.

Reject zero caller output before token or raw staging mutation. Corrupt the
descriptor and, separately, encode a valid Literal followed by an unknown
token tag; both must preserve every output sentinel. These checks distinguish
successful private reconstruction from caller-visible commit and do not add a
streaming contract.

### TVG-0261

For exact LZSS plus Dynamic Range encoding, plan raw `41` into two token bytes,
seven payload bytes, one 16-byte descriptor, and a 79-byte complete frame.
Require the encoder to reproduce the independent frame byte for byte. For six
repeated `A` bytes, encode twice, require identical frames, and decode through
the transactional frame boundary to the original input.

Reject one-byte token staging without changing its sentinel. After a successful
plan, supply a 78-byte serialized destination, require the reported exact
79-byte extent, and preserve every destination sentinel. Reject empty input,
input inconsistent with the outer frame extent, and aggregate storage one byte
below descriptor plus payload plus tokens.

### TVG-0262

For bounded LZSS plus Dynamic Range streaming encoding, independently assemble
the canonical stream prefix and concatenate exact frames produced by the
DD-377 encoder for raw frame extents 2, 2, and 1 over `ABABX`. Feed and drain
the streaming transform one byte at a time and require byte-for-byte equality
with that reference. Separately prove that `Flush` after one raw byte does not
close a partial frame, that a complete final frame retains `EndInput` while
draining, and that repeated calls after completion remain ended.

Reject short raw, token, and serialized-frame regions; set aggregate storage
one byte below raw plus actual tokens plus exact serialized frame and require a
stable limit error. For empty input require only the 80-byte prefix. Reject
premature `EndInput`, excess input, unknown flags, and `ResetBlock` without
silently changing the declared frame sequence.

### TVG-0263

For bounded LZSS plus Dynamic Range streaming decoding, consume the DD-378
multi-frame stream one input byte at a time and drain one raw byte at a time;
require `ABABX`, exact input consumption, and sticky completion. Corrupt the
second frame's Dynamic Range descriptor and require only the first frame's
`AB` to commit while every later output sentinel remains unchanged.

Reject every final-byte truncation, one trailing byte, premature `EndInput`
after the first complete frame, and `ResetBlock`. Exercise short serialized,
token, and raw regions independently, then set the aggregate limit one byte
below the first frame's serialized-plus-token-plus-raw requirement. Finally,
serialize a valid generic header whose token extent is `2F + 1`; require
malformed-stream rejection immediately after that header, before body
collection or output publication. Empty input must accept the prefix alone,
and `Flush` while starved must remain `NeedInput`.

### TVG-0264

For the LZSS plus Dynamic Range workspace profile, use default frame size
65,536 and an original size larger than one frame. Derive `F = 65,536`,
`S = 131,072`, `P = 262,149`, and complete frame storage 262,221 bytes; require
the query to return exactly those raw, token, and serialized extents. For a
17-byte stream require 17, 34, and 145 bytes, and for empty input require all
zeros.

Lower the compressed-payload limit one byte below the short-stream bound,
set the short-stream aggregate limit to 195 bytes rather than the required
196, exceed the 2^23 format frame ceiling, and provide inconsistent LZSS
parameters; require stable failure categories and zeroed output requirements.
For decoder limits with raw frame 4,096, dictionary limit 6,000, and internal
buffered bytes 8,192, require raw 4,096, token 6,000, and serialized 8,248
bytes. Exercise every profile-to-core error mapping explicitly.

### TVG-0265

For the LZSS plus Dynamic Range C ABI smoke, initialize an encoder config,
verify the fixed ABI metadata and 65,536-byte frame/window defaults, then set
the seven-byte `ABABABX` input as one frame. Require workspace query values
primary 7, secondary 119, views 0, alignment 1; construct through the public
factory, finish in one call, and retain the resulting archive.

Initialize decode independently with raw-frame limit 7, token limit 14, and
internal-buffer limit 4,096. Require primary 4,152 and secondary 21, construct
the decoder, and reproduce the seven input bytes exactly. Destroy both handles.
Then shorten secondary by one byte and require creation failure with a null
handle; set a reserved config field and require query rejection. Compile this
test as C11 and link it through the same shared-or-static public target used by
the other C ABI smoke tests.

### TVG-0266

For LZSS plus Dynamic Range public completion, fix the audit frame at 64 raw
bytes and invoke only the public C ABI. Round-trip empty input, every one-byte
value, the ordered 0..255 byte sequence, 257 zeros, a 259-byte
`00 ff 55 aa` repetition, deterministic generated bytes of length 513, and
generated lengths 63, 64, and 65. Encode each case twice and require exact
archive equality.

Generate 193 bytes from the local LCG seed `0x6d617263`. Preserve the
whole-buffer archive as reference, then encode and decode with input/output
chunks 1/1, 7/5, and 13/17 and require identical archive and raw bytes.
Repeat the terminal call and require ended with zero counts.

Generate a separate 193-byte stream from seed `0x13579bdf`, parse the first
three generic frame extents locally, and alter byte 8 of the fourth frame
header. Separately remove the archive's final byte and append one zero byte.
Each public decoder must report sticky malformed stream, produce exactly the
first 192 original bytes, and leave the final `a5` sentinel unchanged.

### TVG-0267

For bounded LZSS plus Dynamic Range fuzzing, clamp arbitrary serialized input
to 8,192 bytes. Fix maximum total raw output at 4,096 bytes, one raw frame at
1,024 bytes, canonical tokens at 2,048 bytes, Dynamic Range payload at 8,192
bytes, and complete encoded-frame storage at `56 + 16 + 8,192` bytes. Count
those arrays and the private raw frame in the local aggregate limit. When the
first 80 bytes parse as the exact profile, pass the remaining exact frame to
private-staging decode. Always run the incremental decoder with input chunks
`1 + byte mod 17`, output chunks `1 + byte mod 19`, and no more than
`8,192 + 4,096 + 32` process calls. Treat impossible counts, progress without
counts, or exhaustion of that ceiling as a harness failure.

Build the permanent canonical regression from raw `ABABX` through the bounded
streaming encoder. For every proper archive prefix, require malformed-stream
failure, zero published bytes, an unchanged `a5` output buffer, and the same
error position on a repeated call. Independently fill generic-frame extent
bytes 16 through 39 with `ff`, then set the last byte of the 16-byte Dynamic
Range descriptor nonzero; both mutations must satisfy the same atomic and
sticky failure checks. Retain only the hand-authored five-byte `MARC\n`
truncation as the initial fuzz corpus.

### TVG-0268

For `lzss-dynamic-range` CLI admission, create the repository-standard binary
fixture by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode
with the explicit selector and compare the restored file byte for byte. Repeat
encode to the same destination and require refusal. Decode `not-a-marc-stream`
and a valid archive with one appended `x`; both must fail and leave neither
the requested destination nor its sibling `.tmp`. Finally round-trip a
zero-byte file. The CLI profile fixes `F = 65,536`, `S = 131,072`,
`P = 262,149`, and aggregate limit 458,829, while all actual workspace extents
must come from the public C requirements query.

### TVG-0269

For the LZSS plus Dynamic Range benchmark smoke, select
`lzss-dynamic-range`, use `README.md`, and run one iteration. Before timing,
encode once into checked capacity `80 + 4N + 77K`, decode the exact encoded
extent once, and require byte equality. Then require one encode and one decode
measurement to reproduce those exact extents while reporting all public
workspace requirements. On the 2026-07-24 MSVC Release build, the 4,441-byte
README produced 3,390 bytes, ratio 0.763, with encoder workspaces
4,441/26,723/0 bytes, decoder workspaces 458,885/196,608/0 bytes, and peak
caller reservation 655,493 bytes. Treat throughput as descriptive only.

### TVG-0270

For interoperability schema 15, retain the deterministic 8,193-byte fixture
and exact schema-14 archive order, then append `lzss-dynamic-range` once as
archive 26. Encode and decode all twenty-six profiles before writing
`manifest.json`; require codec set `marc-cli-v15`, exact order, sizes, SHA-256,
full source revision, platform, compiler, OS, architecture, and CLI hash.
Verification must decode each foreign archive and reproduce every archive byte
for byte locally. Swap the first two entries and require order rejection.
Remove only `lzss-dynamic-range.marc`, change the schema and codec set to 14,
and require the frozen twenty-five-entry bundle to verify before continuing
the established one-generation conversions through schema 1.

### TVG-0271

For the third Dynamic Range composition vector, begin with raw byte `41` and
derive the canonical fixed-width LZ78 Pair
`00 41 00 00 00 00 00 00` directly from the published token grammar.
Independently apply Dynamic Range variant 1 to those eight fixed bytes: start
all 256 frequencies at one, update the 32-bit range and 64-bit low in the
documented integer order, normalize below 2^24 with delayed carry, update the
model after each byte, and perform exactly five termination shifts. The
payload must be `00 00 41 be 41 7c 00 00 00 00 00`.

As a separate implementation check, require the standalone LZ78 encoder to
reproduce the fixed Pair, then require the standalone Dynamic Range encoder to
reproduce the independently calculated payload and descriptor with symbol
count 8 and payload size 11. Serialize the generic header and descriptor
independently and require the exact documented 83-byte frame. Do not use a
future combined encoder to construct its own oracle.

### TVG-0272

For the first LZ78 plus Dynamic Range validator boundary, feed the independent
83-byte single-Pair frame into eight bytes of caller-owned token staging and
one aligned phrase entry. Require exact Pair bytes and phrase
`{prefix 0, symbol 41, length 1}`. Reject every proper frame prefix and one
appended byte. Before entropy output, reject seven-byte token staging, zero
phrase entries, a one-byte-short aggregate workspace, non-multiple-of-eight or
`S > 8F`, `P > 2S + 5`, a nonzero descriptor reserved byte, a wrong sequence,
an unsupported entropy block size, and a stream frame size above 2^21.

Build the malformed phrase-reference payload only with the standalone Dynamic
Range encoder: change the Pair's phrase index from root zero to forward index
one, range-code those eight bytes, and require token index zero, byte offset
zero, and `invalid_phrase_index`. This exercises the fixed token and phrase
graph boundary without using a future combined encoder as its own oracle.

### TVG-0273

For the LZ78 plus Dynamic Range private raw boundary, decode the independent
83-byte single-Pair frame into a three-byte sentinel span and require only its
first byte to become `41`. Separately form canonical tokens for Pair(root,
`A`), Pair(root, `B`), and Pair(index 1, `B`), range-code them with the
standalone Dynamic Range encoder, and require private raw `ABAB` plus the
nested phrase `{prefix 1, symbol 42, length 2}`.

Reject zero raw capacity and a descriptor-plus-payload-plus-token-plus-phrase-
plus-raw aggregate limit one byte short before token staging changes. Corrupt
the descriptor reserved byte and independently range-code a forward phrase
reference; both must leave private raw staging unchanged. No caller-visible
output span participates in this boundary.

### TVG-0274

For transactional LZ78 plus Dynamic Range publication, decode the independent
single-Pair frame into private raw and a three-byte sentinel-filled output;
require only output byte zero to become `41`. Publish the nested `ABAB` phrase
graph into a five-byte output and require its first four bytes while preserving
the final sentinel.

Reject zero caller output before token, phrase, or raw staging mutation.
Corrupt the descriptor and, separately, range-code a forward phrase reference;
both must preserve every output sentinel. These checks distinguish successful
private reconstruction from caller-visible commit and do not add a streaming
contract.

### TVG-0275

For LZ78 plus Dynamic Range frame planning, feed raw `41` through one encoder
entry and eight token bytes. Require canonical Pair
`00 41 00 00 00 00 00 00`, payload extent 11, descriptor extent 16, and exact
serialized extent 83 without supplying serialized output.

Separately plan raw `ABAB` and require three canonical Pair tokens: root plus
`A`, root plus `B`, and phrase index one plus `B`. Reject zero encoder entries
and seven-byte token staging while preserving every token sentinel. Reject
empty input, a raw extent different from the configured frame, and an
encoder-entry-plus-token-plus-descriptor-plus-payload aggregate one byte short.

### TVG-0276

For exact LZ78 plus Dynamic Range emission, encode raw `41` after the complete
plan and require byte equality with the independently assembled 83-byte frame.
Encode raw `ABAB` twice through the same caller-owned entry and token regions,
require byte-identical frames, and decode one through the transactional
complete-frame decoder to the original four bytes.

Provide an 82-byte sentinel-filled destination for raw `41`; require the
reported serialized extent to remain 83, return
`serialized_output_too_small`, and preserve every destination byte. This is
the only capacity failure that reaches the serialized-output boundary.

### TVG-0277

For bounded LZ78 plus Dynamic Range streaming encode, use raw `ABABX` with
two-byte frames. Independently concatenate the serialized stream prefix and
three exact one-shot frames, then require the streaming encoder to reproduce
it with one-byte input and output. Repeat by passing all remaining input with
`EndInput` on the first call while draining one byte at a time.

Issue `Flush` after only the first raw byte and require only the prefix to be
available; complete the remaining input with `EndInput` and require unchanged
reference bytes. Reject short raw, token, encoder-entry, and serialized-frame
storage, and an active raw-plus-token-plus-entry-plus-frame aggregate one byte
short. Also cover empty input, premature end, `ResetBlock`, an unknown flag,
and input beyond the declared size.

### TVG-0278

For bounded LZ78 plus Dynamic Range streaming decode, encode the same raw
`ABABX` as two-byte frames and feed the resulting stream with one-byte input
and output capacities. Require exact raw bytes and a stable ended state.
Corrupt the final frame descriptor and require only the first completed frame
to be published; every destination byte reserved for the failed frame remains
a sentinel.

Exercise serialized-frame, token, private-raw, phrase, and aggregate workspace
shortages independently. End the stream one byte early, append one trailing
byte, issue `ResetBlock`, and issue an unknown flag; require stable errors.
Also cover empty input, nonterminal `Flush`, and `EndInput` after only the first
frame. Finally, rewrite the first frame header with `S > 8F`, serialize its
otherwise valid checksum, and provide no body; require rejection immediately
after the fixed header, proving that impossible extents are not admitted for
body collection.

### TVG-0279

For the LZ78 plus Dynamic Range bounded profile, require a 65,536-byte default
raw frame to produce 524,288 token bytes and a 1,048,653-byte conservative
serialized frame, plus one encoder record per possible raw-byte phrase. For a
17-byte largest frame require 136 token bytes, a 349-byte serialized frame,
and 17 records. Empty input must report zero active regions and alignment one.

Reject the 2^21-byte format cap plus one, invalid LZ78 parameters, a
one-byte-short payload limit, and a one-byte-short active aggregate. Derive a
decoder workspace from a 4,096-byte local raw limit, 6,000 token bytes, and an
8,192-byte buffered limit; require 750 phrase records and a serialized-frame
capacity of `56 + 8192`. Partition aligned encoder and phrase arrays, then
reject changed counts, one-byte-short storage, and deliberately misaligned
storage. Verify every profile error maps to its stable core category.

### TVG-0280

For the first LZ78 plus Dynamic Range C ABI boundary, initialize an encoder,
set raw `ABABX`, two-byte frames, two LZ78 entries, and fixed small local
limits, then require two primary bytes, 125 secondary bytes, and a nonempty
aligned opaque views region. Encode all five bytes with `EndInput`, destroy the
handle, and retain the serialized extent.

Initialize a decoder under the same local frame, token, and entry limits.
Require 1,080 primary bytes, 18 secondary bytes, and a nonempty aligned views
region. Decode the retained stream and require exact `ABABX`. Independently
shorten primary, secondary, and views by one byte, misalign the views pointer,
pass a null publication pointer, and set a reserved config field. Each factory
or query must reject the call and every failed factory must leave the transform
pointer null.

### TVG-0281

For the LZ78 plus Dynamic Range public-ABI completion audit, fix raw frames at
64 bytes, token capacity at 512 bytes, range payload capacity at 1,029 bytes,
and active buffered storage at 65,536 bytes. Construct every transform only
through the public config, requirements, factory, process, and destroy
functions.

Round-trip empty input, all 256 one-byte inputs, the ordered 256-byte alphabet,
257 zeros, a repeated `00 ff 55 aa` pattern, 513 deterministic generated bytes,
and generated lengths 63, 64, and 65. Encode every case twice and require exact
stream identity. For 193 generated bytes, compare unlimited, one-byte, `7/5`,
and `13/17` input/output chunk schedules in both directions and require stable
repeated end.

Locate the fourth frame through the generic little-endian descriptor and
payload length fields. Corrupt its sequence field, remove the final payload
byte, and append one trailing byte in separate cases. Each decode must report a
sticky malformed stream, produce exactly the first 192 raw bytes, and preserve
the final output sentinel.

### TVG-0282

For bounded LZ78 plus Dynamic Range fuzzing, clamp arbitrary serialized input
to 8,192 bytes. Fix maximum total raw output at 4,096 bytes, one raw frame at
1,024 bytes, canonical tokens and Dynamic Range payload at 8,192 bytes each,
the phrase table at 1,024 records, and complete encoded-frame storage at
`56 + 16 + 8,192` bytes. Count every fixed byte array and the phrase records in
the local aggregate limit.

When the first 80 bytes parse as the exact LZ78 plus Dynamic Range profile,
pass the remaining admitted frame to the private exact-frame decoder. Always
run the incremental decoder with input chunks `1 + byte mod 17`, output chunks
`1 + byte mod 19`, and no more than `8,192 + 4,096 + 32` process calls. Treat
impossible counts, progress without counts, or exhaustion of that finite
ceiling as a harness failure; malformed serialized input is an ordinary
decoder result.

Build the permanent canonical regression from raw `ABABX` through the bounded
streaming encoder. For every proper archive prefix, require malformed-stream
failure, zero published bytes, an unchanged `a5` output buffer, and the same
sticky error on a repeated call. Independently fill generic-frame extent bytes
16 through 39 with `ff`, then set the last byte of the 16-byte Dynamic Range
descriptor nonzero; both mutations must satisfy the same atomic failure check.

### TVG-0283

For `lz78-dynamic-range` CLI admission, reuse the repository-standard binary
fixture formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and
decode with the explicit selector and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 524,288`, `P = 1,048,581`, at most
65,536 dictionary entries, and a 4-MiB aggregate policy. Actual primary,
secondary, and aligned opaque-view workspace requirements must come only from
the public C query.

### TVG-0284

For the LZ78 plus Dynamic Range benchmark smoke, select `lz78-dynamic-range`,
use `README.md`, and run one iteration. Before timing, encode once into checked
capacity `80 + 16N + 77K`, decode the exact encoded extent once, and require
byte equality. Then require one encode and one decode measurement to reproduce
those exact extents while reporting all public workspace requirements.

On the 2026-07-25 MSVC Release build, the 4,511-byte README encoded to 4,630
bytes, ratio 1.026, with encoder workspaces 4,511/108,341/72,176 bytes and
decoder workspaces 4,194,360/589,824/1,048,576 bytes. Peak caller reservation
was 5,832,760 bytes. These values document the tested deterministic extents;
the observed throughput is descriptive and not a conformance threshold.

### TVG-0285

For interoperability schema 16, retain the deterministic 8,193-byte fixture
and exact schema-15 archive order, then append `lz78-dynamic-range` once as
archive 27. Generate and locally decode all twenty-seven archives before
writing `manifest.json`; require codec set `marc-cli-v16`, exact order, sizes,
SHA-256 values, and one full Git object ID. Verification must decode every
archive and reproduce it byte for byte.

Copy the generated schema-16 bundle, exchange its first two manifest entries,
and require rejection before archive decoding. Derive schema 15 by removing
only archive 27 and changing only `schema_version` and `codec_set`, verify all
twenty-six frozen archives, then continue the established one-generation
conversion and verification chain through schema 1.

### TVG-0286

For the specified LZW plus Dynamic Range vector, encode raw `A` through the
already frozen standalone LZW grammar. Code 65 at width nine produces packed
bytes `41 00`, whose final seven high bits are zero LZW padding. Independently
apply Dynamic Range variant 1 to those two complete bytes using the documented
integer interval, delayed-carry, model-update, and five-shift termination
rules. Require payload `00 40 FF FF BF 00 00` and descriptor symbol/payload
counts `(2, 7)`.

As a separate implementation check, require the standalone LZW encoder to
reproduce `41 00`, then require the standalone Dynamic Range encoder to
reproduce that payload and descriptor. Serialize the generic frame header and
descriptor independently and compare all 79 bytes with `docs/format.md`. Do
not call a future combined LZW Dynamic Range codec while establishing its own
oracle.

### TVG-0287

For the first combined LZW plus Dynamic Range validator, accept the independent
79-byte raw-`A` frame and require packed staging `41 00`, code count one, and
zero generated phrase entries. Reject every proper frame prefix, one trailing
byte, a dictionary extent above `ceil(FW/8)`, a wrong sequence, and an
unsupported entropy variant.

Prove pre-entropy admission by preserving a sentinel-filled staging buffer when
packed storage is short, the phrase table is short, or aggregate storage is
one byte below the descriptor plus payload plus packed bytes plus aligned
phrase records. Independently corrupt a reserved descriptor byte and the
canonical first range payload byte; require descriptor and entropy errors
before LZW validation. Finally range-encode packed bytes `41 80` and require
the existing LZW validator to report nonzero final padding only after entropy
decoding succeeds.

### TVG-0288

For the LZW plus Dynamic Range private raw decoder, reconstruct the independent
79-byte frame into one private byte and require `A`. Generate a multi-code
frame from raw `ABABABA`, then require exact reconstruction, the expected
nonzero phrase requirement, and the validator's code count.

Fill packed and raw staging with different sentinels. Shorten raw capacity by
one and set the aggregate workspace limit one byte below descriptor, payload,
packed bytes, aligned phrase records, and the complete raw extent; both calls
must fail before entropy output and preserve both sentinels. Corrupt the range
payload and independently range-encode packed bytes with nonzero LZW padding;
both must leave raw staging unchanged. The decoder exposes no caller-visible
output span in this step.

### TVG-0289

For the LZW plus Dynamic Range transactional decoder, initialize output with a
sentinel, decode the independent raw-`A` frame, and require publication of
exactly `A`. Repeat with the multi-code `ABABABA` frame and require one complete
byte-exact publication.

Provide a destination one byte shorter than the declared two-byte raw frame.
Require the dedicated output-capacity error before either packed or private raw
staging changes. Corrupt the raw-`A` range payload independently and require
the complete caller-visible output sentinel to remain unchanged. These tests
establish atomic publication for one complete frame only; they make no
streaming-decoder claim.

### TVG-0290

For the LZW plus Dynamic Range exact-frame planner, pass raw `A` through the
existing LZW plan and encoder into two-byte packed staging. Require bytes
`41 00`, one code, zero encoder entries, a 16-byte descriptor, seven payload
bytes, and the complete 79-byte serialized extent without supplying serialized
output.

Plan raw `ABABABA` twice with separately initialized packed staging and require
identical packed bytes and every reported extent. For raw `AB`, shorten the
encoder workspace and packed staging independently; the former must report its
dedicated capacity error, while the latter must leave its sentinel unchanged.
Set the aggregate workspace limit one byte below the raw-`A` requirement and
require rejection after exact entropy planning. Also reject empty input and a
raw extent inconsistent with the stream frame contract.

### TVG-0291

For the LZW plus Dynamic Range deterministic complete-frame encoder, encode raw
`A` with the exact planner's two-byte packed staging and require byte-for-byte
equality with the independently assembled 79-byte frame. This comparison
covers the generic header, descriptor, and seven-byte range payload.

Encode raw `ABABABA` twice into separately initialized exact-size destinations
and require identical complete frames, then decode one through the
transactional decoder and require the original raw bytes. Shorten the raw-`A`
serialized destination by one byte, fill it with a sentinel, and require the
dedicated capacity error with every destination byte unchanged.

### TVG-0292

For the LZW plus Dynamic Range bounded streaming encoder, independently build
the expected stream prefix and concatenate exact complete-frame encodings for
raw `AB`, `AB`, and final `X`. Feed the streaming encoder one input byte and
one output byte at a time and require exact equality with that reference.

Repeat with a nonterminal `Flush` after one raw byte and require no shortened
frame, then finish with the remaining bytes. Submit all input with `EndInput`
while only one prefix byte can drain and require the retained finish request to
complete later without being repeated. Exercise empty known-size input,
insufficient raw, packed, encoded-frame, and encoder-record storage, aggregate
workspace one byte short, premature and excess input, `ResetBlock`, unknown
flags, sticky error, and repeated post-end calls.

### TVG-0293

For the LZW plus Dynamic Range bounded streaming decoder, feed the streaming-
encoder reference one encoded byte at a time and provide one raw output byte at
a time. Require the original `ABABX`, exact input consumption, and stable
post-end behavior. Separately submit all encoded bytes with `EndInput` while
allowing one raw byte per call and require the finish request to remain active
while validated frames drain.

Corrupt the second frame's Dynamic Range descriptor and require only the first
raw `AB` frame to be published; the failing frame's output sentinel and sticky
error position must remain stable. Reject every proper encoded prefix,
trailing data, short encoded-frame, packed, raw, and phrase workspaces,
aggregate workspace one byte short, `ResetBlock`, and unknown flags. Accept
the exact empty 80-byte stream.

### TVG-0294

For the LZW plus Dynamic Range profile, configure 17 raw bytes with a ten-byte
frame and the default 16-bit LZW ceiling. Require a ten-byte largest frame,
20-byte conservative packed staging, 45-byte range-payload ceiling, 117-byte
complete encoded-frame storage, and nine aligned encoder records. A seven-byte
short frame must instead require 14 packed bytes, 33 payload bytes, and six
records. Empty input must return all-zero regions with alignment one.

Independently lower packed, payload, and aggregate limits by one and require
rejection with cleared requirements. For decoding, use local limits of 64 raw
bytes, 128 packed bytes, 1,024 internally buffered bytes, and 300 dictionary
entries; require 1,080 encoded bytes, 64 private raw bytes, and 112 aligned
phrase records. Partition both record types and reject altered counts, short
storage, and misalignment.

### TVG-0295

For the public LZW plus Dynamic Range C ABI, initialize encode defaults and
require frame size 65,536 and maximum code width 16. With two-byte frames,
width nine, a 16-byte packed limit, 64-byte payload limit, 1,024 aggregate
bytes, and 256 dictionary entries, require two primary bytes and 86 secondary
bytes. Encode `41 42 41 42 58`, then require decode workspaces of 1,080 primary
and 18 secondary bytes and reproduce the original through only the C11
transform API. Both directions require a nonempty aligned opaque region.
Reject every one-byte-short region, deliberate views misalignment, a null
transform output pointer, and a nonzero reserved field.

### TVG-0296

For LZW plus Dynamic Range public completion, reuse the LZW public-ABI data,
chunk, and malformed-frame schedules with the Dynamic Range symbol family and
`2S + 5` payload bound. Exercise empty input, all 256 one-byte values,
`00..FF`, 257 zero bytes, a 259-byte four-symbol pattern, deterministic
513-byte generated input, and generated lengths 63, 64, and 65. Re-encode
every case byte-identically.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the last sentinel, and
repeat the same terminal status and error positions.

### TVG-0297

For bounded LZW plus Dynamic Range decoder fuzzing, run the same fixed-memory
dual-path LZW harness with Dynamic Range profile symbols. Limit input to 8,192
bytes, total output to 4,096, a raw frame to 1,024, packed staging and entries
to 4,096, and the incremental loop to the checked finite call budget. The
complete-frame path parses only a valid 80-byte prefix before invoking private
decode; the streaming path validates every process result and progress state.

Persist three malformed families around the canonical `ABABX` stream: every
proper truncation, saturated generic-frame length fields, and a nonzero final
reserved byte in the 16-byte Dynamic Range descriptor. Each must publish zero
bytes from the failing frame, preserve its sentinel, and return the same error
code and byte position on the next call.

### TVG-0298

For `lzw-dynamic-range` CLI admission, reuse the repository-standard binary
fixture formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and
decode with the explicit selector and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 131,072`, `P = 262,149`, at most
65,280 generated entries, and an 8-MiB aggregate policy. Actual primary,
secondary, and aligned opaque-view workspace requirements must come only from
the public C query.

### TVG-0299

For the LZW plus Dynamic Range benchmark smoke, select `lzw-dynamic-range`,
use `README.md`, and run one iteration. Before timing, encode once into checked
capacity `80 + 4N + 77K`, decode the exact encoded extent once, and require
byte equality. Then require one encode and one decode measurement to reproduce
those exact extents while reporting all public workspace requirements.

On the 2026-07-26 MSVC Release build, the 4,528-byte README encoded to 2,948
bytes, ratio 0.651, with encoder workspaces 4,528/27,245/72,432 bytes and
decoder workspaces 8,388,664/196,608/1,044,480 bytes. Peak caller reservation
was 9,629,752 bytes. These values document the tested deterministic extents;
the observed throughput is descriptive and not a conformance threshold.

### TVG-0300

For interoperability schema 17, retain the deterministic 8,193-byte fixture
and exact schema-16 archive order, then append `lzw-dynamic-range` once as
archive 28. Generate and locally decode all twenty-eight archives before
writing `manifest.json`; require codec set `marc-cli-v17`, exact order, sizes,
SHA-256 values, and one full Git object ID. Verification must decode every
archive and reproduce it byte for byte.

Copy the generated schema-17 bundle, exchange its first two manifest entries,
and require rejection before archive decoding. Derive schema 16 by removing
only archive 28 and changing only `schema_version` and `codec_set`, verify all
twenty-seven frozen archives, then continue the established one-generation
conversion and verification chain through schema 1.

### TVG-0301

For the specified LZD plus Dynamic Range vector, encode raw `A` through the
already frozen standalone LZD grammar. The sole terminal reference pair is
`41 00 00 00 FF FF FF FF`. Independently apply Dynamic Range variant 1 to
those eight complete bytes using the documented integer interval, delayed-
carry, model-update, and five-shift termination rules. Require payload
`00 40 FF FF C4 DC 92 F3 69 BC 8B 00` and descriptor symbol/payload counts
`(8, 12)`.

As a separate implementation check, require the standalone LZD encoder to
reproduce the terminal token, then require the standalone Dynamic Range
encoder to reproduce that payload and descriptor. Serialize the generic frame
header and descriptor independently and compare all 84 bytes with
`docs/format.md`. Do not call a future combined LZD Dynamic Range codec while
establishing its own oracle.

### TVG-0302

For the LZD plus Dynamic Range complete-frame validator, submit the independent
84-byte terminal-token frame and require the exact eight token bytes, one
token, zero generated phrases, and one future expansion-stack entry. Reject
every proper prefix and one trailing byte.

Before entropy output, reject token staging one byte short, phrase storage one
record short for raw `AB`, and aggregate validation storage one byte short.
Corrupt the descriptor and range payload independently and require entropy-
layer errors before LZD validation. Independently range-encode an absent-right
token against a contradictory two-byte raw extent and a forward phrase
reference, then require the existing stable LZD format errors. Also reject
non-multiple-of-eight and over-ceiling token extents, wrong sequence, and an
unsupported entropy variant.

### TVG-0303

For the LZD plus Dynamic Range private raw decoder, reconstruct the independent
84-byte frame into one private byte and require `A`. Generate a phrase-bearing
frame from raw `ABABAB`, then require exact reconstruction, two tokens, two
generated phrases, and three expansion-stack entries.

Fill token, expansion, and raw staging with distinct sentinels. Shorten raw and
expansion capacity independently and set the aggregate workspace limit below
the descriptor, payload, token bytes, aligned phrase records, expansion stack,
and complete raw extent; each must fail before entropy output and preserve the
unreached sentinels. Corrupt the Dynamic Range descriptor and independently
encode a contradictory LZD terminal token; both must leave private raw staging
unchanged. The decoder exposes no caller-visible output span in this step.

### TVG-0304

For the LZD plus Dynamic Range transactional decoder, initialize output with a
sentinel, decode the independent raw-`A` frame, and require publication of
exactly `A`. Repeat with the phrase-bearing `ABABAB` frame and require one
complete byte-exact publication.

Provide a destination one byte shorter than the declared two-byte raw frame.
Require the dedicated output-capacity error before token, expansion, or private
raw staging changes. Corrupt the raw-`A` range payload independently and
require the complete caller-visible output sentinel to remain unchanged. These
tests establish atomic publication for one complete frame only; they make no
streaming-decoder claim.

### TVG-0305

For the LZD plus Dynamic Range exact-frame planner, pass raw `A` through the
existing LZD plan and encoder into eight-byte token staging. Require bytes
`41 00 00 00 FF FF FF FF`, one token, zero encoder entries, a 16-byte
descriptor, twelve payload bytes, and the complete 84-byte serialized extent
without supplying serialized output.

Plan raw `ABABAB` twice with separately initialized token staging and require
identical token bytes and every reported extent. For raw `AB`, shorten the
encoder workspace and token staging independently; the former must report its
dedicated capacity error, while the latter must leave its sentinel unchanged.
Set the aggregate workspace limit one byte below the raw-`A` requirement and
require rejection after exact entropy planning. Also reject empty input and a
raw extent inconsistent with the stream frame contract.

### TVG-0306

For the LZD plus Dynamic Range deterministic complete-frame encoder, encode raw
`A` with the exact planner's eight-byte token staging and require byte-for-byte
equality with the independently assembled 84-byte frame. This comparison
covers the generic header, descriptor, and twelve-byte range payload.

Encode raw `ABABAB` twice into separately initialized exact-size destinations
and require identical complete frames, then decode one through the
transactional decoder and require the original raw bytes. Shorten the raw-`A`
serialized destination by one byte, fill it with a sentinel, and require the
dedicated capacity error with every destination byte unchanged.

### TVG-0307

For the LZD plus Dynamic Range bounded streaming encoder, independently build
the expected stream prefix and concatenate exact complete-frame encodings for
raw `AB`, `AB`, and final `X`. Feed the streaming encoder one input byte and
one output byte at a time and require exact equality with that reference.

Repeat with a nonterminal `Flush` after one raw byte and require no shortened
frame, then finish with the remaining bytes. Submit all input with `EndInput`
while only one prefix byte can drain and require the retained finish request to
complete later without being repeated. Exercise empty known-size input,
insufficient raw, token, encoded-frame, and encoder-record storage, aggregate
workspace one byte short, premature and excess input, `ResetBlock`, unknown
flags, sticky error, and repeated post-end calls.

### TVG-0308

For the LZD plus Dynamic Range bounded streaming decoder, feed the streaming-
encoder reference one encoded byte at a time and provide one raw output byte at
a time. Require the original `ABABX`, exact input consumption, and stable
post-end behavior. Separately submit all encoded bytes with `EndInput` while
allowing one raw byte per call and require the finish request to remain active
while validated frames drain.

Corrupt the second frame's Dynamic Range descriptor and require only the first
raw `AB` frame to be published; the failing frame's output sentinel and sticky
error position must remain stable. Reject every proper encoded prefix,
trailing data, short encoded-frame, token, raw, phrase, and expansion
workspaces, aggregate workspace one byte short, `ResetBlock`, and unknown
flags. Accept the exact empty 80-byte stream.

### TVG-0309

For the LZD plus Dynamic Range profile, configure 17 raw bytes with a ten-byte
frame. Require a ten-byte largest frame, 40-byte conservative token staging,
85-byte range-payload ceiling, 157-byte complete encoded-frame storage, and
five aligned encoder records. A seven-byte short frame with a two-entry
dictionary ceiling must instead require 32 token bytes, 69 payload bytes, and
two records. Empty input must return all-zero regions with alignment one.

Independently lower token, payload, and aggregate limits by one and require
rejection with cleared requirements. For decoding, use local limits of 64 raw
bytes, 128 token bytes, 1,024 internally buffered bytes, and ten dictionary
entries; require 1,080 encoded bytes, 64 private raw bytes, ten aligned phrase
records, and eleven expansion references. Partition both encoder and decoder
typed storage and reject altered offsets, short storage, and misalignment.

### TVG-0310

For the LZD plus Dynamic Range C ABI, initialize encode configuration for raw
`41 42 41 42 58`, two-byte frames, eight token bytes, a 21-byte range-payload
limit, and 512 aggregate bytes. Require workspace extents of 2 primary and 101
secondary bytes plus a nonempty aligned opaque region. Encode, then request
decoder workspaces of 568 primary and 10 secondary bytes and reproduce the
original through only the C11 transform API.

Reject every one-byte-short region, deliberately misaligned views storage, a
null transform output pointer, and a nonzero reserved field. Require factory
failure to leave the transform pointer null.

### TVG-0311

For LZD plus Dynamic Range public completion, reuse the LZD public-ABI data,
chunk, and malformed-frame schedules with the Dynamic Range symbol family and
`2S + 5` payload bound. Exercise empty input, all 256 one-byte values,
`00..FF`, 257 zero bytes, a 259-byte four-symbol pattern, deterministic
513-byte generated input, and generated lengths 63, 64, and 65. Re-encode
every case byte-identically.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the last sentinel, and
repeat the same terminal status and error positions.

### TVG-0312

For bounded LZD plus Dynamic Range decoder fuzzing, run the same fixed-memory
dual-path LZD harness with Dynamic Range profile symbols. Limit input to 8,192
bytes, total output and token staging to 4,096, a raw frame to 1,024, the range
payload to 8,192, phrase records to 512, expansion references to 513, and the
incremental loop to the checked finite call budget. The complete-frame path
parses only a valid 80-byte prefix before invoking private decode; the
streaming path validates every process result and progress state.

Persist three malformed families around the canonical `ABABX` stream: every
proper truncation, saturated generic-frame length fields, and a nonzero final
reserved byte in the 16-byte Dynamic Range descriptor. Each must publish zero
bytes from the failing frame, preserve its sentinel, and return the same error
code and byte position on the next call.

### TVG-0313

For `lzd-dynamic-range` CLI admission, reuse the repository-standard binary
fixture formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and
decode with the explicit selector and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 262,144`, `P = 524,293`, at most
65,536 dictionary entries, and a 16-MiB aggregate policy. Actual primary,
secondary, and aligned opaque-view workspace requirements must come only from
the public C query.

### TVG-0314

For the LZD plus Dynamic Range benchmark smoke, select `lzd-dynamic-range`, use
`README.md`, and run one iteration. Before timing, encode once into checked
capacity `80 + 16*ceil(N/2) + 77K`, decode the exact encoded extent once, and
require byte equality. Then require one encode and one decode measurement to
reproduce those exact extents while reporting all public workspace
requirements.

On the 2026-07-28 MSVC Release build, the 4,530-byte README encoded to 4,021
bytes, ratio 0.888, with encoder workspaces 4,530/54,437/36,240 bytes and
decoder workspaces 16,777,272/327,680/655,364 bytes. Peak caller reservation
was 17,760,316 bytes. These values document the tested deterministic extents;
the observed throughput is descriptive and not a conformance threshold.

### TVG-0315

For interoperability schema 18, generate the same deterministic 8,193-byte
fixture and retain the exact schema-17 archive order. Append exactly one
`lzd-dynamic-range` archive as entry 29, set `schema_version` to 18 and
`codec_set` to `marc-cli-v18`, and record each complete archive's size and
SHA-256 after a local decode equality check.

Verify all twenty-nine archives in manifest order, require foreign decode
equality and byte-identical local re-encoding, and reject a manifest with its
first two entries swapped. Derive schema 17 by removing only the new archive
and restoring `marc-cli-v17`, then verify the unchanged schemas 17 through 1
compatibility chain.

### TVG-0316

For the first LZMW plus Dynamic Range vector, encode raw byte `41` using the
standalone LZMW variant-1 encoder and require the complete four-byte reference
`41 00 00 00`. Pass exactly that frozen byte span to a fresh standalone
Dynamic Range variant-1 encoder and require payload
`00 40 FF FF BF 00 00 00` with descriptor `(symbol_count=4,
payload_size=8, reserved=0)`.

Independently serialize a generic sequence-zero frame header with raw size one,
dictionary-serialized size four, compressed-payload size eight, entropy block
count one, descriptor size sixteen, and no trailer. Concatenate header,
descriptor, and payload and require the exact 80-byte frame recorded in
`docs/format.md`. The vector test must call only the standalone LZMW encoder,
standalone Dynamic Range planner/encoder, and generic serializers; it must not
depend on a future combined implementation.

### TVG-0317

Pass the exact 80-byte frame to the first combined validator with four private
reference bytes and no phrase records. Require complete acceptance, exact
reference bytes, one token, zero generated phrases, and a one-reference future
expansion ceiling. Reject all 80 proper prefixes and one trailing byte.

Generate raw `AB` using only the standalone LZMW and Dynamic Range encoders;
require two references, one phrase record `(A, B, length 2)`, and a two-
reference future expansion ceiling. Before entropy output, reject reference
staging and phrase storage one entry short and reject aggregate validation
workspace one byte short while preserving guarded staging.

Independently corrupt the descriptor and payload, range-code a forward phrase
reference, declare the single literal as two raw bytes, alter reference extent
to both an oversized and an unaligned value, change the expected sequence, and
select an unsupported entropy variant. Require stable descriptor, entropy,
LZMW validation, extent, header, and pipeline error categories in that order;
no case may reconstruct or publish a raw byte.

### TVG-0318

For the LZMW plus Dynamic Range private decoder, pass the exact raw-`A` frame
with four reference bytes, one expansion entry, and one private raw byte.
Require exact private `A` reconstruction and no dictionary decode error.
Independently make raw and expansion storage one entry short and require
rejection before entropy output while all reference, expansion, and raw guards
remain unchanged.

Set the aggregate limit to the validator-only 28 bytes
(`16 descriptor + 8 payload + 4 references`) and require private decoding to
reject the additional four-byte expansion entry and one raw byte. Generate
raw `ABABAB` through standalone LZMW and Dynamic Range encoders, require four
references, three generated phrases, a four-entry active expansion ceiling,
and exact private reconstruction. Corrupt the descriptor and encode a forward
reference independently; both must preserve the private raw guard.

### TVG-0319

For the transactional LZMW plus Dynamic Range frame decoder, decode the exact
raw-`A` frame through distinct reference, expansion, private raw, and caller
output spans. Require private and caller output to become `A` only after
success. Generate raw `ABABAB` through standalone components and require the
complete phrase frame to publish all six bytes once.

For raw `AB`, provide caller output one byte short and seed reference,
expansion, private raw, and output spans with distinct guards. Require
`raw_output_too_small` before entropy output and preserve every guard.
Independently corrupt the descriptor and encode a forward reference with full
capacity; require the corresponding descriptor or LZMW validation error while
preserving both private raw and caller-output guards.

### TVG-0320

For the LZMW plus Dynamic Range exact-frame planner, pass raw `A`, no encoder
records, and four reference-staging bytes. Require one token, zero generated
phrases, frozen reference `41 00 00 00`, descriptor size 16, payload size 8,
and complete frame size 80 without any serialized output span.

For raw `ABABAB`, allocate the exact standalone LZMW encoder-record count and
`4F` reference staging. Plan twice and require identical reference bytes,
token and dictionary-entry counts, payload size, and complete extent. For raw
`AB`, make encoder records one entry short and preserve guarded staging. For
raw `A`, make reference staging one byte short and preserve its guard. Set the
aggregate limit one byte below `4 references + 16 descriptor + 8 payload`;
also pass empty input and two bytes to a one-byte frame independently. Require
stable workspace or input-size errors and no serialized output.

### TVG-0321

For the LZMW plus Dynamic Range deterministic complete-frame encoder, encode
raw `A` with the exact planner's four reference-staging bytes and require byte-
for-byte equality with the independently assembled 80-byte frame. This
comparison covers the generic header, descriptor, and eight-byte range
payload.

Encode raw `ABABAB` twice into separately initialized exact-size destinations
and require identical complete frames, then decode one through the
transactional decoder and require the original raw bytes. Shorten the raw-`A`
serialized destination by one byte, fill it with a sentinel, and require the
dedicated capacity error with every destination byte unchanged.

### TVG-0322

For the LZMW plus Dynamic Range bounded streaming encoder, independently build
the expected stream prefix and concatenate exact complete-frame encodings for
raw `AB`, `AB`, and final `X`. Feed the streaming encoder one input byte and
one output byte at a time and require exact equality with that reference.

Repeat with a nonterminal `Flush` after one raw byte and require no shortened
frame, then finish with the remaining bytes. Submit all input with `EndInput`
while only one prefix byte can drain and require the retained finish request to
complete later without being repeated. Exercise empty known-size input,
insufficient raw, reference, encoded-frame, and encoder-record storage,
aggregate workspace one byte short, premature and excess input, `ResetBlock`,
unknown flags, sticky error, and repeated post-end calls.

### TVG-0323

For the LZMW plus Dynamic Range bounded streaming decoder, feed the streaming-
encoder reference one encoded byte at a time and provide one raw output byte at
a time. Require the original `ABABX`, exact input consumption, and stable post-
end behavior. Separately submit all encoded bytes with `EndInput` while
allowing one raw byte per call and require the finish request to remain active
while validated frames drain.

Corrupt the second frame's Dynamic Range descriptor and require only the first
raw `AB` frame to be published; the failing frame's output sentinel and sticky
error position must remain stable. Reject every proper encoded prefix,
trailing data, short encoded-frame, reference, raw, phrase, and expansion
workspaces, aggregate workspace one byte short, `ResetBlock`, and unknown
flags. Accept the exact empty 80-byte stream.

### TVG-0324

For the LZMW plus Dynamic Range profile, configure 17 raw bytes with a ten-byte
frame. Require a ten-byte largest frame, 40-byte conservative reference
staging, 85-byte range-payload ceiling, 157-byte complete encoded-frame
storage, and nine aligned encoder records. A seven-byte short frame with a two-
entry dictionary ceiling must instead require 28 reference bytes, a 61-byte
payload ceiling, and two records. Empty input must return all-zero regions with
alignment one.

Independently lower reference, payload, and aggregate limits by one and require
rejection with cleared requirements. For decoding, use local limits of 64 raw
bytes, 128 reference bytes, 1,024 internally buffered bytes, and ten dictionary
entries; require 1,080 encoded bytes, 64 private raw bytes, ten aligned phrase
records, and eleven expansion references. Partition both encoder and decoder
typed storage and reject altered offsets, short storage, and misalignment.

### TVG-0325

For the LZMW plus Dynamic Range C ABI, initialize encode configuration for raw
`41 42 41 42 58`, two-byte frames, eight reference bytes, a 21-byte range-
payload limit, and 512 aggregate bytes. Require workspace extents of 2 primary
and 101 secondary bytes plus a nonempty aligned opaque region. Encode, then
request decoder workspaces of 568 primary and 10 secondary bytes and reproduce
the original through only the C11 transform API.

Reject every one-byte-short region, deliberately misaligned views storage, a
null transform output pointer, and a nonzero reserved field. Require factory
failure to leave the transform pointer null.

### TVG-0326

For LZMW plus Dynamic Range public completion, reuse the LZMW public-ABI data,
chunk, and malformed-frame schedules with the Dynamic Range symbol family and
`2S + 5` payload bound. Exercise empty input, all 256 one-byte values,
`00..FF`, 257 zero bytes, a 259-byte four-symbol pattern, deterministic
513-byte generated input, and generated lengths 63, 64, and 65. Re-encode
every case byte-identically.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the final sentinel,
and repeat the same terminal status and error positions.

### TVG-0327

For bounded LZMW plus Dynamic Range decoder fuzzing, run the fixed-memory
dual-path LZMW harness with Dynamic Range profile symbols. Limit input and
range payload to 8,192 bytes, total output and reference staging to 4,096,
one raw frame to 1,024, phrase records to 1,023, expansion references to
1,024, and the incremental loop to the checked finite call budget. The
complete-frame path proceeds only after a valid 80-byte prefix; the streaming
path validates every process result and progress state.

Persist three malformed families around the canonical `ABABX` stream: every
proper truncation, saturated generic-frame length fields, and a nonzero final
reserved byte in the 16-byte Dynamic Range descriptor. Each must publish zero
bytes, preserve its sentinel, and repeat the same error category and byte
position.

### TVG-0328

For `lzmw-dynamic-range` CLI admission, reuse the repository-standard binary
fixture formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and
decode with the explicit selector and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 262,144`, `P = 524,293`, at most
65,536 generated entries, and a 16-MiB aggregate policy. Actual primary,
secondary, and aligned opaque-view workspace requirements must come only from
the public C query.

### TVG-0329

For the LZMW plus Dynamic Range benchmark smoke, select `lzmw-dynamic-range`,
use `README.md`, and run one iteration. Before timing, encode once into checked
capacity `80 + 8N + 77K`, decode the exact encoded extent once, and require
byte equality. Then require one encode and one decode measurement to reproduce
those exact extents while reporting all public workspace requirements.

On the 2026-07-28 MSVC Release build, the 4,520-byte README encoded to 3,870
bytes, ratio 0.856, with encoder workspaces 4,520/54,317/72,304 bytes and
decoder workspaces 16,777,272/327,680/1,310,704 bytes. Peak caller reservation
was 18,415,656 bytes. These values document tested deterministic extents; the
observed throughput is descriptive and not a conformance threshold.

### TVG-0330

For interoperability schema 19, generate the same deterministic 8,193-byte
fixture and retain the exact schema-18 archive order. Append exactly one
`lzmw-dynamic-range` archive as entry 30, set `schema_version` to 19 and
`codec_set` to `marc-cli-v19`, and record each complete archive's size and
SHA-256 after a local decode equality check.

Require the verifier to reject any reordered schema-19 manifest, validate all
thirty archives in exact order, decode every archive to the fixture, and
re-encode every profile byte-identically. Derive schema 18 by removing only
entry 30 and changing its version and codec set, then retain the complete
schema-18-through-schema-1 compatibility chain.

### TVG-0331

For the first LZ77 plus rANS vector, begin with raw byte `41` and independently
require the canonical 16-byte Literal token
`00 00 00 00 00 00 00 00 00 00 00 00 41 00 00 00`. Normalize its byte
counts to rANS total 4096, yielding only `00:3840` and `41:256`. Apply the
documented reverse rANS recurrence from `L = 2^31`; require final little-endian
state payload `00 A5 22 10 15 00 00 00` with no renormalization bytes.

Assemble a generic frame independently with raw size 1, token size 16, payload
size 8, one block, and descriptor size 528. Require descriptor prefix
`10 00 00 00 08 00 00 00 0C 00 00 00 00 00 00 00`, frequency bytes
`00 0F` at offsets 16..17 and `00 01` at offsets 146..147, and zero everywhere
else in the frequency region. Append the payload and compare every one of the
592 bytes against output assembled only from the standalone LZ77 encoder,
rANS encoder, and explicit generic serializers.

### TVG-0332

For the first LZ77 plus rANS validator tests, require the 592-byte hand vector
to reconstruct the exact Literal token in private staging. Re-encode that same
token with rANS block size five and require four blocks, deliberately proving
that entropy boundaries may split a token. Reject every proper frame prefix
and one trailing byte.

Use one-entry-short view storage, one-byte-short token staging, and an
aggregate workspace ceiling one byte below descriptor, payload, token, and
view bytes; each must fail before token mutation. Lower a normalized frequency
to invalidate the descriptor, and replace the second of two eight-symbol
block states with zero; the latter must report block index one while preserving
the entire token sentinel. Finally encode a token with invalid kind `FF` and
require rANS success followed by the stable LZ77 token error, and raise the
declared payload above `S + 8K` for early extent rejection.

### TVG-0333

For private LZ77 plus rANS reconstruction, decode the 592-byte hand frame into
a three-byte guarded raw span and require only its first byte to become `41`.
Independently serialize a Literal `A` followed by terminal match
`(distance=1,length=4)`, rANS-code the complete 32-byte token region, and
require private output `AAAAA`, exercising forward overlap copy.

Submit zero raw capacity for the one-byte frame and require rejection before
token staging changes. Set the aggregate limit one byte below descriptor,
payload, token, view, and raw bytes and require both token and raw sentinels to
remain unchanged. Finally corrupt the second of two rANS block states and
separately rANS-code token kind `FF`; both must leave raw staging untouched,
with entropy and dictionary errors remaining distinct.

### TVG-0334

For transactional LZ77 plus rANS publication, decode the hand frame with
three-byte private and output guards. Require only byte zero of each to become
`41`. Submit an empty output span with otherwise sufficient workspace and
require rejection before token or private raw mutation.

Corrupt the second state of a two-block rANS frame and separately encode token
kind `FF`. Invoke the caller-visible decoder for both and require the stable
entropy or dictionary error while private raw and output sentinels remain
unchanged. These cases distinguish successful private reconstruction from the
single final publication copy.

### TVG-0335

For the first LZ77 plus rANS encoder-side planner, submit raw `41` and require
the existing 16-byte Literal token in private staging, one 528-byte
descriptor, one eight-byte payload, and complete extent 592 without providing
a serialized destination. Repeat with `B=5`; require four rANS blocks even
though the boundaries split that one token.

Submit only 15 staging bytes prefilled with `5A`; require the exact required
size 16 and preserve every sentinel. Reject empty raw input and two raw bytes
against a one-byte frame declaration. Finally set the block-count limit to
three for the four-block case, then restore four blocks and set the aggregate
limit one byte below `4*528 + 4*8 + 16`; require stable entropy-limit and
workspace-limit errors respectively.

### TVG-0336

For deterministic LZ77 plus rANS frame encoding, provide exactly 592 output
bytes for raw `41` and require byte-for-byte equality with the independent
frame already assembled from the generic header, standalone LZ77 token, rANS
descriptor, and rANS payload. Repeat at `B=5`, encode twice, and require both
outputs to equal each other and the separately assembled four-block frame;
then decode it through the combined transactional decoder to raw `41`.

Provide only 591 output bytes prefilled with `5A` for the ordinary one-block
frame. Require the exact planned extent 592, the stable short-output error,
and preservation of every sentinel. This capacity case must fail after token
planning but before generic header, descriptor, or payload publication.

### TVG-0337

For bounded LZ77 plus rANS streaming encode, use raw `ABABX`, outer frame size
two, and rANS block size five. Independently assemble the 80-byte stream
prefix and each complete combined frame through the one-shot APIs. Feed and
drain the streaming encoder one byte at a time and require exact equality with
that reference plus stable repeated `EndOfStream`.

Separately submit one raw byte with `Flush`, require only the prefix and keep
the partial frame open, then finish the remaining input and compare the whole
stream. Reject short raw collection and conservative token staging at
construction, reject completed-frame storage when the first frame becomes
ready, and set the aggregate limit one byte below
`raw + exact tokens + serialized frame`. For empty known-size input require
prefix-only completion; reject premature `EndInput` and `ResetBlock`.

### TVG-0338

For bounded LZ77 plus rANS streaming decode, consume the `ABABX`, `F=2`,
`B=5` stream generated above one input byte at a time and publish one raw byte
at a time; require exact raw bytes and stable repeated `EndOfStream`. Corrupt
descriptor frequency byte 17 of the second frame, submit the whole stream, and
require only first-frame `AB` publication while every remaining output
sentinel stays `5A`.

For the first frame independently shorten serialized-frame storage by one,
provide six rather than seven rANS views, shorten 32-byte token staging by
one, and shorten two-byte private raw staging by one; require `OutOfMemory`.
Then set the aggregate limit one byte below
`serialized + 32 tokens + 2 raw + 7*sizeof(RansBlockView)` with a compatible
block limit and require `LimitExceeded`. Reject the stream missing its final
byte, one appended zero, and `ResetBlock`. Accept prefix-only empty input,
treat `Flush` during starvation as `NeedInput`, and retain premature
`EndInput` through first-frame raw drain before reporting truncation.

### TVG-0339

For the LZ77 plus rANS profile calculator, use a 2,500,000-byte known stream
with default 65,536-byte frame and block sizes. Require `F=65,536`,
`S=1,048,576`, `K=16`, and encoded-frame ceiling 1,057,208 bytes. For a
17-byte stream require 17 raw bytes, 272 token bytes, one descriptor, and an
864-byte encoded-frame ceiling; empty input requires zero active encoder
workspace. Reject independently excessive block count, payload, aggregate,
frame size, and invalid LZ77 parameters.

For decoder sizing, use local limits of 4,096 raw bytes, 6,000 token bytes,
8,192 internal bytes, and seven blocks; require 8,248 serialized bytes,
6,000 token bytes, 4,096 raw bytes, and seven views. Overflow the serialized
extent and require cleared outputs. Finally obtain both direction
requirements for raw `ABABX`, construct the streaming encoder and decoder
only from those capacities, and require byte-exact round trip and stable
profile-to-core error mapping.

### TVG-0340

For the first public LZ77 plus rANS C boundary, initialize encoding for raw
`ABABABX`, set both frame dimensions, and require seven primary bytes, 4,032
secondary bytes, zero views, and alignment one. Create and finish through the
opaque transform lifecycle. Initialize decoding with local 4,096-byte frame
and block limits, 6,000 dictionary bytes, 8,192 internal bytes, and seven
blocks; require 8,248 primary bytes, 10,096 secondary bytes, and nonzero
aligned opaque views. Recover the exact seven raw bytes. Reject one-byte-short
views and a nonzero reserved field while keeping the transform null.

### TVG-0341

For public completion, use 64-byte raw frames and 64-byte rANS blocks. Exercise
empty input, every one-byte value, ordered `00..FF`, 257 zeroes, a 259-byte
repeating binary pattern, deterministic 513-byte data, and generated lengths
63, 64, and 65 entirely through the C ABI. Encode each case twice before
decoding. For 193 generated bytes, compare unchunked output with schedules
`(1,1)`, `(7,5)`, and `(13,17)`.

Walk three generic frame extents from the 80-byte prefix to locate the fourth
frame. Independently alter its sequence field, remove the final encoded byte,
and append one zero byte. Each decode must publish exactly the first 192 raw
bytes, preserve the final `A5` sentinel, and return the identical sticky error
category and position on a subsequent empty call.

### TVG-0342

For the bounded LZ77 plus rANS fuzz target, cap input and payload at 8,192
bytes, total output and token staging at 4,096 bytes, one raw frame at 1,024
bytes, and rANS metadata at eight views. Exercise the strict complete-frame
staging decoder after a valid exact prefix and always exercise the incremental
decoder with byte-derived chunks and at most 12,320 calls.

Retain `MARC\n` as the truncated-magic seed. As permanent regressions, generate
the canonical `ABABX` stream with 16-byte rANS blocks and require every strict
prefix truncation to fail without raw publication. Saturate generic frame
extent fields and alter the first normalized-frequency entry independently;
both must preserve all `A5` output sentinels and repeat the same sticky error.

### TVG-0343

For `lz77-rans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode
with the exact selector and compare the restored file byte for byte. Repeat
encode to the existing destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the destination nor its `.tmp` sibling. Finally round-trip
an empty file.

### TVG-0344

For the LZ77 plus rANS benchmark smoke, select `lz77-rans`, use repository
`README.md` as the ordinary input, and request one iteration. Require the
adapter to complete a public-ABI encode/decode equality check before timing
and to report the selector, input and encoded sizes, ratio, both directional
times and throughputs, all six queried workspace extents, and peak workspace.
No throughput or compression-ratio threshold is a test oracle.

### TVG-0345

For interoperability schema 20, retain the exact schema-19 archive order over
the deterministic 8,193-byte fixture and append exactly one `lz77-rans`
archive as entry 31. Set `schema_version` to 20 and `codec_set` to
`marc-cli-v20`; record every complete archive's size and SHA-256 only after
its local decode matches the fixture.

Require exact thirty-one-entry order, foreign decode equality, byte-identical
local re-encoding, and rejection after swapping the first two manifest
entries. Derive schema 19 by removing only entry 31 and restoring
`schema_version=19` and `codec_set=marc-cli-v19`, then exercise the unchanged
schema-19-through-schema-1 compatibility chain.

### TVG-0346

For the first LZSS plus rANS vector, begin with raw byte `41` and independently
require the standalone LZSS encoder to emit Literal token `00 41`. Submit only
those two frozen bytes to a fresh standalone scalar rANS encoder. Require
normalized frequencies `00:2048` and `41:2048` and payload
`00 10 00 00 02 00 00 00`.

Serialize one generic frame declaring raw size 1, dictionary size 2, payload
size 8, one block, and 528 descriptor bytes. Require descriptor prefix
`02 00 00 00 08 00 00 00 0C 00 00 00 00 00 00 00`, zero frequencies except
`00 08` at offsets 16..17 and 146..147, and the exact payload suffix. Compare
all 592 bytes against a separately assembled sparse expected array.

### TVG-0347

For the first LZSS plus rANS validator suite, generate frames only with the
standalone rANS planner, descriptor serializer, and encoder over already
frozen token bytes. Require the independent 592-byte Literal frame to decode
to private `00 41` staging. Repeat with block size 1 so the block boundary
splits that Literal. Reject every strict truncation, trailing bytes, short
views or staging, an aggregate workspace one byte below the exact requirement,
a malformed descriptor, and a malformed second block before any staging byte
changes. Separately require invalid reconstructed tag `FF` to report the LZSS
token error and reject dictionary or entropy extents beyond DD-462's bounds
before mutation.

### TVG-0348

For private LZSS plus rANS reconstruction, decode the independent raw-`A`
frame into an oversized sentinel-filled raw span and require only its first
byte to become `41`. Build a separate canonical Literal-`A`, Match(distance 1,
length 5) token sequence with the standalone serializer and require six
private `A` bytes, proving forward overlap. A raw span one byte short and an
aggregate limit one byte below descriptor plus payload plus views plus token
plus raw staging must fail before token or raw mutation. Corrupt a later rANS
block and separately encode invalid LZSS grammar; neither may change private
raw staging.

### TVG-0349

For transactional LZSS plus rANS publication, give the independent raw-`A`
frame oversized sentinel-filled private and output spans. Require one private
and one published `41` byte with both tails unchanged. Repeat the canonical
Literal plus overlap-Match case and require exactly six published `A` bytes
with the seventh sentinel intact. A zero-length output for the one-byte frame
must fail before token or raw mutation. A malformed later rANS block and
invalid reconstructed LZSS grammar must leave every output byte unchanged.

### TVG-0350

For exact LZSS plus rANS planning, pass raw `A` through the standalone LZSS
planner and encoder into caller-owned staging, then plan its rANS block without
providing serialized output. Require `S=2`, `K=1`, descriptor size 528,
payload size 8, and complete size 592. Repeat with `B=1` and require two
blocks, 1,056 descriptor bytes, and 16 payload bytes. Plan six repeated `A`
bytes twice with `B=3`; require the same canonical 11-byte Literal-plus-Match
sequence and four blocks. Reject short staging without mutation, empty and
unexpected frame extents, a block limit of one for the split Literal, and
aggregate storage one byte below `528K + P + S`.

### TVG-0351

For deterministic LZSS plus rANS frame writing, require raw `A` to reproduce
all 592 independently assembled bytes. With `B=1`, encode twice and compare
the complete frames with each other and the standalone-component frame, then
transactionally decode to raw `A`. Repeat six `A` bytes with `B=3`; require
the same generated 11-byte Literal-plus-Match staging, byte-identical frames,
and complete round-trip through four rANS blocks. Give the raw-`A` writer a
591-byte sentinel-filled destination and require every byte to remain
unchanged.

### TVG-0352

For bounded LZSS plus rANS streaming encode, use five bytes `ABABX`, raw frame
size 2, and entropy block size 5. Independently concatenate the canonical
80-byte prefix and three one-shot frames with sequence and committed-output
positions 0/0, 1/2, and 2/4. Require the streaming transform to reproduce that
exact sequence with one-byte input and output buffers and to return stable
EndOfStream afterward. Separately prove that `Flush` after one raw byte emits
only available prefix bytes and remains open; reject short raw, `2F` token,
encoded-frame, and aggregate storage; accept canonical empty input; and reject
premature `EndInput` and `ResetBlock`.

### TVG-0353

For bounded LZSS plus rANS streaming decode, generate the same `ABABX`
three-frame stream through the local streaming encoder. Feed encoded and raw
bytes one at a time and require exact output and stable repeated end. Corrupt
the second frame's rANS descriptor and require only the first raw `AB` frame
to be published before a sticky malformed error. Use the first frame's exact
header to reject encoded storage one byte short, zero descriptor views,
three-byte token staging, one-byte raw staging, and aggregate storage one byte
short. Reject every final-byte truncation, one trailing zero byte, and
`ResetBlock`; accept prefix-only empty input and non-terminal Flush; and prove
premature EndInput becomes malformed only after the already validated first
frame finishes draining.

### TVG-0354

For the LZSS plus rANS internal profile, use original size 2,500,000 with
default `F = B = 65,536`. Require 65,536 raw bytes, conservative 131,072-byte
token staging, two rANS blocks, and a 132,200-byte complete encoded-frame
extent. With only 17 raw bytes, require 17 raw, 34 token, and 626 encoded
bytes; empty input requires all-zero encoder extents.

Independently lower block-count, payload, aggregate workspace, and the
composition's 1-MiB raw-frame cap and require rejection with cleared
requirements. For decoding, configure 4,096 raw bytes, 6,000 dictionary
bytes, 8,192 internally buffered bytes, and seven blocks; require 8,248
encoded bytes, 6,000 token bytes, 4,096 raw bytes, and seven views. Finally
allocate exactly the returned encoder and decoder extents and round-trip
`ABABX` with `F=2` and `B=5` through only the two streaming transforms.

### TVG-0355

For the LZSS plus rANS C ABI, initialize encoding for raw `ABABABX`, set
`F=7` and `B=16`, and require seven primary bytes, 620 secondary bytes, zero
view bytes, and alignment one. Encode through only the public C11 transform
lifecycle. Initialize decoding with 4,096-byte raw and block limits, 6,000
token bytes, 8,192 internally buffered bytes, and seven views; require 8,248
primary bytes, 10,096 secondary bytes, nonzero opaque views, and their
reported alignment. Decode byte-exactly, then reject one-byte-short views and
a nonzero reserved field while leaving the transform handle null.

### TVG-0356

For LZSS plus rANS public completion, reuse the public-C data and chunk
schedules with fixed `F=B=64`. Exercise empty input, every one-byte value,
`00..FF`, 257 zero bytes, a 259-byte four-symbol pattern, deterministic
513-byte input, and generated lengths 63, 64, and 65. Encode every case twice
and require byte identity before round-trip decode.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the final sentinel,
and repeat the same terminal status and error positions.

### TVG-0357

For LZSS plus rANS fuzz regression, generate canonical raw `ABABX` with one
five-byte frame and a 16-byte entropy block. Submit every proper prefix to a
fresh streaming decoder with sentinel output and require zero publication,
sticky error category, and stable byte position. Independently saturate frame
fields at offsets 16 through 39 and toggle one normalized frequency byte in
the first rANS descriptor; require the same atomic contract.

Seed the bounded fuzzer only with `MARC\n`. Limit supplied input to 8,192
bytes. The private path parses an exact LZSS/rANS prefix before complete-frame
decode. The public C path fixes 4,096 total raw bytes, 1,024-byte frames and
blocks, 2,048 token bytes, 8,192 payload bytes, and eight views; reject any
requirements exceeding its compile-time arrays. Derive chunks modulo 17 and
19 and cap calls at input plus output plus 32.

### TVG-0358

For `marc --codec lzss-rans`, generate the repository-standard binary fixture
by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and decode with the
explicit selector and require byte equality. Repeat encoding to the occupied
destination and require refusal. Decode `not-a-marc-stream` and a valid archive
with one appended `x`; both must fail and leave neither the requested output
nor its sibling `.tmp`. Finally round-trip an empty file.

Fix `F = B = 65,536`, `S = 131,072`, `K = 2`, `P = 131,088`, descriptor bytes
at 1,056, and the shared aggregate policy at 512 KiB. Obtain primary,
secondary, and opaque-view extents and alignment only from the public C query.

### TVG-0359

For the LZSS plus rANS benchmark smoke, select `lzss-rans`, use `README.md`,
and run one iteration. Reserve checked complete-stream capacity
`80 + 2N + 1128K`. Before timing, encode once, decode the exact encoded extent,
and require byte equality. Then require one encode and one decode measurement
to reproduce those extents while reporting all six public workspace sizes and
the larger three-region sum.

On the 2026-07-31 MSVC Release build, the 4,520-byte README encoded to 3,819
bytes, ratio 0.845, with encoder workspaces 4,520/18,672/0 bytes and decoder
workspaces 524,344/196,608/1,056 bytes. Peak caller reservation was 722,008
bytes. These values document tested deterministic extents; throughput is
descriptive and not a conformance threshold.

### TVG-0360

For interoperability schema 21, retain the exact schema-20 archive order over
the deterministic 8,193-byte fixture and append exactly one `lzss-rans`
archive as entry 32. Set `schema_version` to 21 and `codec_set` to
`marc-cli-v21`; record every complete archive's size and SHA-256 only after
local decode equality succeeds.

Require exact thirty-two-entry order, foreign decode equality, byte-identical
local re-encoding, and rejection of a manifest whose first two entries are
swapped. Remove only `lzss-rans.marc`, change only the schema and codec set to
20, and then exercise the unchanged schema-20-through-schema-1 conversion and
verification chain.

### TVG-0361

For the first LZ78 plus rANS vector, begin with raw byte `41` and independently
derive canonical LZ78 Pair token `00 41 00 00 00 00 00 00`. Feed exactly
those eight bytes to standalone scalar rANS block planning. Require normalized
frequencies `00:3584` and `41:512`, payload
`00 7C 9D 2F 0A 00 00 00`, one 528-byte descriptor, and a complete 592-byte
frame.

Construct the generic header separately with raw extent 1, dictionary extent
8, payload extent 8, one block, and descriptor extent 528. Serialize the rANS
descriptor separately, append the payload, and compare every byte with the
independently recorded sparse frame representation. Do not invoke any future
combined LZ78/rANS implementation in this vector.

### TVG-0362

For the first LZ78 plus rANS complete-frame validator, admit the frozen
592-byte single-Pair frame into one rANS view, eight private token bytes, and
one phrase record. Re-encode the same token with `B = 3` so three entropy
blocks split the token at byte boundaries unrelated to its eight-byte grammar.
Reject every strict prefix and one trailing byte. Exercise one-short view,
token, phrase, and aggregate workspace limits before token mutation. Corrupt
the third block's final state and require all sentinel token bytes to remain
unchanged; separately entropy-code an invalid LZ78 tag and require the nested
token error. Also reject a seven-byte dictionary extent, a payload beyond
`S + 8K`, and a mismatched entropy algorithm.

### TVG-0363

For private LZ78 plus rANS raw reconstruction, decode the frozen single-Pair
frame into a one-byte sentinel region and require `41`. Encode Pair `A`, Pair
`B`, and Pair `(1,B)` as three canonical tokens, split their 24 bytes into
five-byte rANS blocks, and require iterative reconstruction of `41 42 41 42`.
Reject an empty raw region and a one-byte-short aggregate limit before token
or phrase mutation. Corrupt the final rANS block and separately entropy-code
an invalid LZ78 tag; both cases must preserve the raw sentinel.

### TVG-0364

For transactional LZ78 plus rANS publication, decode the frozen single-Pair
frame into private raw and a three-byte sentinel-filled output; require only
output byte zero to become `41`. Publish the nested `ABAB` phrase graph from
five-byte rANS blocks into a five-byte output and preserve its final sentinel.
An empty output must reject before token, phrase, or raw mutation. A corrupted
late rANS block and an entropy-coded invalid LZ78 tag must preserve every
caller-visible output byte.

### TVG-0365

For exact LZ78 plus rANS encoding, plan raw `41` with one LZ78 encoder record
and require canonical token `00 41 00 00 00 00 00 00`, one 528-byte
descriptor, eight payload bytes, and complete extent 592. Encode and compare
every byte with the independent frame. Repeat with `B = 3` and require three
blocks that split the token. Encode raw `41 42 41 42` with `B = 5`, require
the three canonical Pair tokens and five blocks, encode twice byte-identically,
and decode to the original raw bytes. Reject empty or mismatched frame input,
short encoder or token workspace, excessive block count, one-short aggregate
workspace, and one-short serialized output; the last case must preserve every
output sentinel.

### TVG-0366

For LZ78 plus rANS streaming encode, use five raw bytes `41 42 41 42 58`,
two-byte raw frames, and five-byte rANS blocks. Independently concatenate the
80-byte stream prefix and three exact frames produced at sequences 0, 1, and
2, then require one-byte input and output processing to match every byte.
Repeat with a partial-frame `Flush` and with `EndInput` supplied while the
prefix still drains. Exercise short raw, `8F` token, encoder-record, and
serialized-frame storage; set the combined raw, token, frame, and record
aggregate one byte short; and reject premature end, reset, unknown flags, and
excess input. Empty known-size input emits only the prefix and ends.

### TVG-0367

For LZ78 plus rANS streaming decode, feed that exact streaming-encoder output
one encoded byte at a time and accept one raw byte at a time. Require
`41 42 41 42 58`, exact encoded consumption, and stable repeated end. Corrupt
the second frame after a valid first frame and require only raw `41 42` to be
published while the rest of the output sentinel remains unchanged.

Independently reject one-byte-short final input, one trailing zero,
`ResetBlock`, encoded-frame storage one byte short, zero rANS views, token
staging one byte short, raw staging one byte short, zero phrase records, and
the aggregate decoder workspace one byte short. A nonterminal `Flush` with no
input must request input. Submit `EndInput` with only the first complete frame
while allowing one output byte, then require the retained finish request to
report truncation after that verified frame finishes draining. Accept the
exact empty 80-byte stream.

### TVG-0368

For the LZ78 plus rANS profile, configure 2,500,000 raw bytes with default
65,536-byte raw and entropy blocks. Require a 65,536-byte largest frame,
524,288 token bytes, eight rANS blocks, and a 528,632-byte complete encoded
frame ceiling. A 17-byte short stream must instead require 136 token bytes,
one block, and 728 encoded bytes; lowering the LZ78 entry ceiling to two must
reserve only two encoder records. Empty input has zero byte regions and
alignment one.

Independently exceed the block-count, payload, aggregate, and one-MiB profile
limits. For decoding, use local limits of 64 raw bytes, 128 token bytes, 1,024
internally buffered bytes, four rANS blocks, and ten dictionary entries.
Require 1,080 encoded bytes, 64 private raw bytes, four block views, and ten
phrase records. Partition both directional opaque layouts; reject altered
offsets, one-byte-short storage, and misalignment. Finally construct encoder
and decoder solely from calculated regions and round-trip `41 42 41 42 58`.

### TVG-0369

For the LZ78 plus rANS C ABI, initialize encoding for raw
`41 42 41 42 58`, two-byte raw frames, five-byte entropy blocks, two LZ78
entries, and bounded local policy. Require queried primary storage of two
bytes, secondary storage of 2,232 bytes, and a nonempty aligned opaque region.
Encode through only the C11 lifecycle.

For decoding, use an 8,192-byte aggregate policy, seven-block ceiling,
16-byte token ceiling, and a five-byte local frame ceiling so the standalone
rANS layer may admit its configured block. Require 8,248 primary and 21
secondary bytes plus aligned opaque views, then reproduce the original.
Reject each region one byte short, deliberate views misalignment, a null
transform output pointer, and a nonzero reserved field; every creation failure
must leave the transform pointer null.

### TVG-0370

For LZ78 plus rANS public completion, use 64-byte raw frames and entropy
blocks, a 512-byte token ceiling, eight block descriptors, a 576-byte payload
ceiling, and the public requirements query. Exercise empty input, all 256
one-byte values, `00..FF`, 257 zero bytes, a 259-byte four-symbol pattern,
deterministic 513-byte generated input, and generated lengths 63, 64, and 65.
Re-encode every case byte-identically.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the last sentinel,
and repeat the same terminal status and error positions.

### TVG-0371

For bounded LZ78 plus rANS decoder fuzzing, cap supplied input at 8,192 bytes,
total output at 4,096, raw frames at 1,024, canonical tokens at 8,192, rANS
payload at 16,384, block views at eight, and phrase records at 1,024. Exercise
the private complete-frame decoder only after parsing a valid 80-byte prefix,
and always exercise the public C streaming decoder through queried workspaces.
Use byte-derived chunks and a checked finite call budget.

Persist the canonical `ABABX` stream's every proper truncation, all-ones
generic frame extent fields, and a nonzero byte at the first rANS descriptor's
reserved offset 10. Each regression must produce no output from its failing
frame, preserve the output sentinel, and return the same error code and byte
position on the next call.

### TVG-0372

For `lz78-rans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode
with the explicit selector and compare the restored file byte for byte. Repeat
encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 524,288`, `K = 8`, `P = 524,352`,
at most 65,536 generated entries, and a 4-MiB aggregate policy. Actual primary,
secondary, and aligned opaque-view workspace requirements must come only from
the public C query.

### TVG-0373

For the LZ78 plus rANS benchmark smoke, select `lz78-rans`, use `README.md`,
and run one iteration. Before timing, encode once into checked capacity
`80 + 8N + 4344K`, decode the exact encoded extent once, and require byte
equality. Then require one encode and one decode measurement to reproduce those
exact extents while reporting all public workspace requirements.

### TVG-0374

For interoperability schema 22, retain the exact schema-21 archive order over
the deterministic 8,193-byte fixture and append exactly one `lz78-rans`
archive as entry 33. Set `schema_version` to 22 and `codec_set` to
`marc-cli-v22`; record every complete archive's size and SHA-256 only after
local decode equality succeeds.

Require exact thirty-three-entry order, foreign decode equality, byte-identical
local re-encoding, and rejection of a manifest whose first two entries are
swapped. Remove only `lz78-rans.marc`, change only the schema and codec set to
21, and then exercise the unchanged schema-21-through-schema-1 conversion and
verification chain.

### TVG-0375

For the first LZW plus rANS vector, begin with raw byte `41` and independently
derive canonical width-nine packed code bytes `41 00`, including the seven
zero padding bits. Feed exactly those two bytes to standalone scalar rANS
block planning. Require normalized frequencies `00:2048` and `41:2048`,
payload `00 08 00 00 02 00 00 00`, one 528-byte descriptor, and a complete
592-byte frame.

Construct the generic header separately with raw extent 1, dictionary extent
2, payload extent 8, one block, and descriptor extent 528. Serialize the rANS
descriptor separately, append the payload, and compare every byte with the
independently recorded sparse frame representation. Do not invoke any future
combined LZW/rANS implementation in this vector.

### TVG-0376

For the first LZW plus rANS complete-frame validator, admit the frozen
single-code frame into one rANS view, two packed staging bytes, and zero phrase
records. Require exact extents, one validated block and code, and private
`41 00` reconstruction. With `B=1`, require two blocks that split the
nine-bit code while preserving the same packed bytes.

Reject every proper frame prefix and one trailing byte. Before packed mutation,
reject zero views, one packed byte, a missing phrase record for packed `AB`,
and aggregate workspace one byte short. Corrupt only the second block state
and require the entire packed sentinel to remain unchanged. Entropy-code
`41 80` successfully, then require LZW nonzero-padding rejection. Reject a
packed extent above `ceil(FW/8)`, a payload above `S+8K`, and the wrong
pipeline.

### TVG-0377

For private LZW plus rANS reconstruction, decode the frozen raw-`A` frame into
a one-byte sentinel and require `41`. Separately pack codes 65, 66, 256, and
258 as `41 84 00 14 08`, divide those bytes into rANS blocks of at most two
symbols, and require raw `ABABABA`; this crosses both packed-code and phrase
edges and exercises `KwKwK`. Reject missing raw capacity and aggregate storage
one byte short before packed mutation. Entropy-code invalid padded bytes
`41 80` and require the raw sentinel to remain unchanged.

### TVG-0378

For transactional LZW plus rANS publication, decode the frozen raw-`A` frame
into private staging and a two-byte destination sentinel. Require only the
first destination byte to become `41`. Repeat the block-size-two `ABABABA`
`KwKwK` case and require exact private and public output. Supply a destination
one byte short and require packed, phrase, raw, and output storage to remain
unchanged. Corrupt a later rANS state and separately entropy-code `41 80`;
both must preserve the complete destination.

### TVG-0379

For exact LZW plus rANS planning, feed raw `A` to the standalone LZW planner
and encoder, require packed bytes `41 00`, then independently plan its one rANS
block and require descriptor extent 528, payload extent 8, and complete frame
extent 592 without serialized output. For raw `ABABABA`, require codes 65, 66,
256, 258 packed as `41 84 00 14 08`; with `B=2`, require three blocks, 1,584
descriptor bytes, 24 payload bytes, and deterministic repeated plans. Reject a
short encoder workspace and packed span before packed mutation, aggregate
storage one byte short, empty input, and a raw extent inconsistent with the
stream header.

### TVG-0380

For deterministic LZW plus rANS frame encoding, encode raw `A` through the
combined encoder and compare all 592 bytes with the independently assembled
vector. Encode raw `ABABABA` twice with `B=2`, require byte-identical complete
frames, then decode one through the transactional combined decoder and require
the original seven bytes. Give the raw-`A` encoder a 591-byte destination
filled with `A5` and require every byte to remain unchanged.

### TVG-0381

For bounded LZW plus rANS streaming encoding, use raw `ABABX`, outer frames of
two bytes, and rANS blocks of two bytes. Independently serialize the ordinary
80-byte prefix and append each exact frame from the complete-frame planner and
encoder. Require identical output when both input and output capacities are
one byte. Verify that `Flush` leaves a partial frame open, `EndInput` retained
while every region drains produces the same stream, workspace and aggregate
shortages fail stably, empty input emits only the prefix, and premature end,
excess input, `ResetBlock`, and unknown flags are rejected.

### TVG-0382

For bounded LZW plus rANS streaming decoding, feed that canonical multi-frame
stream with one-byte input and output capacities and require raw `ABABX`.
Corrupt the second frame's rANS model and require only first-frame raw `AB` to
be published. Make each serialized-frame, view, packed, raw, and phrase region
one entry short in turn, then set aggregate storage one byte below the exact
sum and require early failure. Reject every final-byte truncation, one trailing
byte, `ResetBlock`, and an unknown flag; accept the empty prefix-only stream;
keep `Flush` non-terminal; and retain premature `EndInput` while draining the
first valid frame before reporting the missing later frame.

### TVG-0383

For the LZW plus rANS profile calculator, freeze a ten-byte largest frame with
maximum width 16 and rANS blocks of four bytes. Require 20 packed bytes, five
blocks, a 2,756-byte complete-frame ceiling, nine encoder entries, and their
exact aligned byte extent. Require canonical zero regions and alignment one for
empty input. Exercise block-count, payload, aggregate, frame, parameter, and
decoder-limit failures. For decoder limits of raw 64, packed 128, four blocks,
and 300 local dictionary entries, require 112 LZW phrase records after an
aligned rANS-view region. Reject altered, short, and misaligned typed storage.
Finally allocate only the reported regions and round-trip `ABABX` through
two-byte frames and two-byte rANS blocks.

### TVG-0384

For the first LZW plus rANS pure-C vector, initialize the public encode config,
then set raw size five, two-byte frames, two-byte entropy blocks, maximum width
16, and bounded local limits. Require primary bytes 2 and secondary bytes
1,136, create the transform from the three queried regions, and encode
`41 42 41 42 58`. Reinitialize for decode, require primary bytes 8,248 and
secondary bytes 20, decode the exact produced stream, and compare all five raw
bytes. Reject primary, secondary, and views regions one byte short, a
misaligned views region, null handle output, and nonzero reserved metadata.

### TVG-0385

For LZW plus rANS public-ABI completion, use 64-byte outer frames, 64-byte rANS
blocks, width 16, and only the public C lifecycle. Round-trip empty input,
every one-byte value, all 256 values, 257 zeroes, a 259-byte four-symbol
pattern, 513 deterministic generated bytes, and generated lengths 63, 64, and
65 twice to prove determinism. Encode 193 generated bytes with unbounded,
`1/1`, `7/5`, and `13/17` input/output chunks and require identical bytes and
exact decode. In turn alter the fourth frame sequence, remove its final byte,
and append one byte; require malformed status, sticky repeated diagnostics,
exact publication of the first three 64-byte frames, and an untouched final
destination byte.

### TVG-0386

For LZW plus rANS fuzz regressions, generate canonical `ABABX` through the
local streaming encoder with one five-byte frame and one 16-byte entropy
block. Present every strict prefix to a fresh decoder and require zero
publication plus a stable repeated error. Separately overwrite generic-frame
extent fields at offsets 16 through 39 with `ff`, and set byte 10 of the first
rANS descriptor to one. Both complete streams must fail atomically. The live
fuzzer accepts at most 8 KiB, uses only fixed caller-owned arrays, and stops
after at most maximum input plus maximum output plus 32 process calls.

### TVG-0387

For `lzw-rans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode
with the explicit selector and compare the restored file byte for byte. Repeat
encoding to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file. Fix `F = B = 65,536`, `S <= 131,072`, `K <= 2`,
`P <= 131,088`, maximum code width 16, at most 65,280 generated entries, and
an 8-MiB aggregate policy. Obtain actual storage only from the public query.

### TVG-0388

For the LZW plus rANS benchmark smoke, select `lzw-rans`, use `README.md`, and
run one iteration. Before timing, encode once into checked capacity
`80 + 2N + 1128K`, decode the exact produced extent, and require byte equality.
Then require one encode and one decode measurement to reproduce those exact
extents while reporting all public workspace requirements. On the 2026-08-01
MSVC Release build, the 4,522-byte README encoded to 3,396 bytes, ratio 0.751,
with 9,630,808 bytes of peak caller reservation. Record throughput only as
descriptive small-input evidence.

### TVG-0389

For interoperability schema 23, retain the exact schema-22 archive order over
the deterministic 8,193-byte fixture and append exactly one `lzw-rans` archive
as entry 34. Set `schema_version` to 23 and `codec_set` to `marc-cli-v23`;
record every complete archive's size and SHA-256 only after local decode
equality succeeds.

Require exact thirty-four-entry order, foreign decode equality, byte-identical
local re-encoding, and rejection of a manifest whose first two entries are
swapped. Remove only `lzw-rans.marc`, change only the schema and codec set to
22, and then exercise the unchanged schema-22-through-schema-1 conversion and
verification chain.

### TVG-0390

For the first LZD plus rANS vector, begin with raw byte `41` and independently
derive canonical terminal-token bytes `41 00 00 00 FF FF FF FF`. Feed exactly
those eight finalized bytes to standalone scalar rANS block planning. Require
normalized frequencies `00:1536`, `41:512`, and `FF:2048`, followed by payload
`82 27 A1 BD 04 00 00 00 00`.

Independently serialize a generic header declaring raw extent 1, dictionary
extent 8, payload extent 9, one entropy block, and 528 descriptor bytes.
Serialize the sparse descriptor and append the payload; compare all 593 bytes
with the separately recorded frame representation. Do not invoke any future
combined LZD/rANS implementation in this vector.

### TVG-0391

For the first LZD plus rANS complete-frame validator, admit the frozen raw-`A`
frame into one rANS view, eight token staging bytes, and zero phrase records.
Require exact extents, one validated block and token, zero generated phrases,
and private `41 00 00 00 FF FF FF FF` reconstruction. With `B=3`, require
three blocks that split both a four-byte reference and the eight-byte token.

With `B=4`, set a reserved byte in the second descriptor and require every
sentinel token-staging byte to remain unchanged. Separately entropy-code a
right reference to unavailable phrase 256 and require entropy reconstruction
to finish before the LZD validator reports its token error. Reject short view,
token, and phrase regions, every one-byte frame truncation represented by the
focused case, one trailing byte, and the wrong entropy pipeline.

### TVG-0392

For private LZD plus rANS reconstruction, decode the frozen raw-`A` frame into
a one-byte sentinel and require `41`. Separately serialize `(A,B), (256,256)`,
divide its sixteen token bytes into rANS blocks of at most five symbols, and
require raw `ABABAB`; this crosses reference, token, entropy-block, and
generated-phrase boundaries while requiring two phrase records and three
expansion references.

Make raw staging one byte short and the expansion stack one entry short before
entropy processing; in both cases require sentinel token staging to remain
unchanged. Corrupt a later rANS descriptor and require the private raw sentinel
to remain unchanged.

### TVG-0393

For transactional LZD plus rANS publication, decode the frozen raw-`A` frame
into private staging and a three-byte destination sentinel. Require only the
first destination byte to become `41`. Repeat the block-size-five `ABABAB`
generated-phrase case and require exact private and public output. Supply a
destination one byte short for raw `AB` and require token staging, private raw,
and output guards all to remain unchanged. Corrupt a later rANS descriptor and
separately entropy-code a forward reference to phrase 256; both must preserve
the complete destination.

### TVG-0394

For LZD plus rANS exact-frame planning, begin with raw `41`, zero encoder
records, and eight token-staging bytes. Require canonical bytes
`41 00 00 00 FF FF FF FF`, one rANS block, 528 descriptor bytes, nine payload
bytes, and complete extent 593 without providing serialized output. For raw
`ABABAB` and block size five, invoke the planner twice with the same bounded
encoder storage and separate staging regions; require sixteen identical token
bytes, four blocks, and identical counts and extents. With raw `AB`, reject an
encoder region one record short and a token region one byte short while the
complete token sentinel remains unchanged. Under block size eight, count
`8 + 528 + 9 = 545` aggregate bytes and reject a 544-byte policy. Also reject
empty input and a declared frame extent different from the supplied input.

### TVG-0395

For deterministic LZD plus rANS frame encoding, pass raw `41` through the
exact planner and encoder with a 593-byte destination, then compare every byte
with the independently assembled frame rather than only decoding it. For raw
`ABABAB` with block size five, plan once, encode twice into differently seeded
destinations, require complete byte identity, and transactionally decode the
result through four rANS views, sixteen token bytes, two phrase records, three
expansion references, and six private raw bytes. Finally provide only 592
serialized bytes for raw `41`; require the distinct capacity error and retain
every destination sentinel byte. Repeat with a full 593-byte sentinel but
empty raw input and require the planner's input-size error to preserve it too.

### TVG-0396

For bounded LZD plus rANS streaming encoding, use raw `ABABX`, raw-frame size
two, and entropy-block size two. Independently serialize the 80-byte stream
prefix and append three one-shot frames for `AB`, `AB`, and `X` with sequences
zero through two and committed raw offsets zero, two, and four. Feed the same
input and collect output one byte at a time; require exact equality with that
reference and sticky `EndOfStream`. Issue nonterminal `Flush` after only `A`
and require only the prefix plus the unchanged later canonical frames. Present
the complete raw input with `EndInput` on the first call while draining one
output byte per call and require the flag to remain effective through all
frames. Separately prove the empty 80-byte stream, raw/token/typed/frame storage
shortages, the simultaneous-workspace sum one byte short, premature end,
excess input, `ResetBlock`, and an unknown flag.

### TVG-0397

For bounded LZD plus rANS streaming decoding, generate the same `ABABX` stream
through the bounded streaming encoder and feed both serialized input and raw
output one byte at a time. Require exact raw equality and sticky
`EndOfStream`. Locate the second frame after the first complete serialized
extent, corrupt one later descriptor frequency byte, and require only the first
raw `AB` frame to be published while every later destination sentinel remains
unchanged. From the first frame header, derive exact encoded-frame, view, token,
raw, phrase, and expansion requirements; reject each one entry short and their
simultaneous byte sum one byte short before body reconstruction. Also reject
the final-byte truncation, one trailing byte, `ResetBlock`, an unknown flag,
and premature `EndInput` after only the first frame; prove the empty 80-byte
stream and nonterminal flush starvation.

### TVG-0398

For the LZD plus rANS profile calculator, freeze a ten-byte largest frame and
four-byte rANS blocks. Require 40 maximum token bytes, ten blocks, a 5,456-byte
complete-frame ceiling, five LZD encoder records, and their exact aligned byte
extent. For decoding under local limits, require four rANS views followed by
ten aligned phrase records and eleven aligned expansion references. Alter an
offset or record count, shorten storage, and require rejection before typed
views are published. Finally allocate every returned byte and record region and
use only those spans to stream `ABABX` through two-byte frames and back.

### TVG-0399

For the first LZD plus rANS pure-C vector, initialize the public encode config,
then set raw size five, two-byte frames, two-byte entropy blocks, and bounded
local limits. Require primary bytes 2 and secondary bytes 2,216, create the
transform from the three queried regions, and encode `41 42 41 42 58`.
Reinitialize for decode, require primary bytes 8,248 and secondary bytes 10,
decode the exact produced extent, and compare all five raw bytes. Shorten each
workspace independently, misalign opaque views, pass a null transform output,
and set a reserved field; every case must fail without publishing a handle.

### TVG-0400

For LZD plus rANS public-ABI completion, use 64-byte outer frames, 64-byte rANS
blocks, at most four blocks, 256 maximum token bytes, 288 maximum payload bytes,
and a 2,456-byte complete-frame ceiling. Through only the public C lifecycle,
round-trip empty input, every one-byte value, all 256 values, 257 zeroes, a
259-byte four-symbol pattern, 513 deterministic generated bytes, and generated
lengths 63, 64, and 65 twice for determinism. Encode 193 generated bytes with
unbounded, one-byte, 7/5-byte, and 13/17-byte input/output schedules and require
identical bytes and raw output. Corrupt, truncate, and extend the fourth frame;
require exactly the first 192 raw bytes, an untouched final sentinel, and a
stable repeated terminal error.

### TVG-0401

For LZD plus rANS fuzz regressions, generate canonical `ABABX` through the local
streaming encoder with one five-byte frame and one 16-byte entropy block.
Present every strict prefix to a fresh decoder and require zero publication
plus a stable repeated error. Separately overwrite generic-frame extent fields
at offsets 16 through 39 with `ff`, and set byte 10 of the first 528-byte rANS
descriptor to one; both complete inputs must fail atomically. The fuzz entry
caps input at 8,192 bytes and drives both private complete-frame and public C
streaming decode with fixed arrays, byte-derived chunks, and an independent
12,320-call ceiling.

### TVG-0402

For `lzd-rans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and decode it
with explicit `--codec lzd-rans` and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `B = 65,536`, `S = 262,144`, `K = 4`,
descriptor extent 2,112, `P = 262,176`, and a 16-MiB aggregate policy. Actual
primary, secondary, and aligned opaque-view workspace requirements must come
only from the public C query.

### TVG-0403

For the LZD plus rANS benchmark smoke, select `lzd-rans`, use `README.md`, and
run one iteration. Before timing, encode into checked capacity
`80 + 8 * ceil(N/2) + 2200K`, decode the exact encoded extent once, and require
byte equality. Require the timed encode and decode to reproduce those extents,
then report complete-stream ratio, both throughputs, every public workspace
region, and the larger direction-specific caller-owned total. Retain the
half-pair rounding explicitly so a one-byte input reserves eight token bytes.

### TVG-0404

For interoperability schema 24, generate the deterministic 8,193-byte fixture
with the unchanged repository recurrence, preserve the exact schema-23 archive
order, and append exactly one `lzd-rans` archive as entry 35. Set
`schema_version` to 24 and `codec_set` to `marc-cli-v24`; record every archive's
size and SHA-256 only after local round-trip success. Verify exact order,
leaf-only names, hashes, foreign decoding, and byte-identical local re-encoding.
Swap the first two manifest entries and require rejection. Derive schema 23 by
removing only `lzd-rans`, restoring `marc-cli-v23`, and then verify the unchanged
schemas 23 through 1.

### TVG-0405

For the first LZMW plus rANS vector, begin with raw byte `41` and independently
derive canonical reference bytes `41 00 00 00`. Feed exactly those four
finalized bytes to standalone scalar rANS block planning. Require normalized
frequencies `00:3072` and `41:1024`, followed by payload
`00 1C A1 BD 04 00 00 00`.

Independently serialize a generic header declaring raw extent 1, dictionary
extent 4, payload extent 8, one entropy block, and 528 descriptor bytes.
Serialize the sparse descriptor and append the payload; compare all 592 bytes
with the separately recorded frame representation. Do not invoke any future
combined LZMW/rANS implementation in this vector.

### TVG-0406

For the first LZMW plus rANS complete-frame validator, admit the frozen raw-`A`
frame into one block view, four reference-staging bytes, and no phrase records.
Require the exact reference bytes and successful one-token graph validation.
Re-encode `A | B` and split its eight reference bytes at three-byte rANS block
boundaries to prove that entropy boundaries do not imply reference boundaries.

With two four-byte blocks, corrupt only the second descriptor and fill staging
with a sentinel; require controller rejection before any sentinel changes.
Separately encode the valid entropy bytes `00 01 00 00` and require LZMW
forward-reference rejection only after all four bytes appear in staging. Test
each caller capacity one unit short, every strict serialized truncation used by
the focused suite, one appended byte, and a tANS pipeline substituted for rANS.

### TVG-0407

For private LZMW plus rANS reconstruction, decode the frozen raw-`A` frame into
a one-byte sentinel and require `41`. Separately serialize references
`A, B, 256, 256`, divide their sixteen bytes into rANS blocks of at most five
symbols, and require raw `ABABAB`; this crosses reference, entropy-block, and
generated-phrase boundaries while requiring three phrase records and four
expansion references.

Make raw staging one byte short and the conservative four-entry expansion stack
one entry short before entropy processing; in both cases require sentinel
reference staging to remain unchanged. Corrupt a later rANS descriptor and
require the private raw sentinel to remain unchanged.

### TVG-0408

For transactional LZMW plus rANS publication, decode the frozen raw-`A` frame
into private staging and a three-byte destination sentinel. Require only the
first destination byte to become `41`. Repeat the block-size-five `ABABAB`
generated-phrase case and require exact private and public output. Supply a
destination one byte short for raw `AB` and require reference staging, private
raw, and output guards all to remain unchanged. Corrupt a later rANS descriptor
and separately entropy-code a forward reference to phrase 256; both must
preserve the complete destination.

### TVG-0409

For LZMW plus rANS exact-frame planning, begin with raw `41`, zero encoder
records, and four reference-staging bytes. Require canonical bytes
`41 00 00 00`, one rANS block, 528 descriptor bytes, eight payload bytes, and
complete extent 592 without providing serialized output. For raw `ABABAB` and
block size five, invoke the planner twice with the same bounded encoder storage
and separate staging regions; require sixteen identical reference bytes, four
blocks, and identical counts and extents. With raw `AB`, reject an encoder
region one record short and a reference region one byte short while the
complete staging sentinel remains unchanged. Under block size eight, count
`4 + 528 + 8 = 540` aggregate bytes and reject a 539-byte policy. Also reject
empty input and a declared frame extent different from the supplied input.

### TVG-0410

For deterministic LZMW plus rANS frame encoding, pass raw `41` through the
exact planner and encoder with a 592-byte destination, then compare every byte
with the independently assembled frame rather than only decoding it. For raw
`ABABAB` with block size five, plan once, encode twice into differently seeded
destinations, require complete byte identity, and transactionally decode the
result through four rANS views, sixteen reference bytes, three phrase records,
four expansion references, and six private raw bytes. Finally provide only 591
serialized bytes for raw `41`; require the distinct capacity error and retain
every destination sentinel byte. Repeat with a full 592-byte sentinel but
empty raw input and require the planner's input-size error to preserve it too.

### TVG-0411

For bounded LZMW plus rANS streaming encoding, independently serialize the
80-byte stream prefix, split raw `ABABX` into configured two-byte outer frames,
and append the result of each exact-frame planner and encoder transaction.
Require the incremental encoder to reproduce that complete reference stream
with one-byte input and output. Separately retain `EndInput` while only one
prefix or frame byte can drain, confirm nonterminal `Flush` leaves a one-byte
partial frame open, and reject short raw, reference, typed-entry, serialized-
frame, and aggregate storage. Cover empty known-size input, premature end,
excess input, explicit reset, unknown flags, and repeated terminal calls.

### TVG-0412

For bounded LZMW plus rANS streaming decoding, generate the canonical
`ABABX` stream with the local streaming encoder and feed it one serialized byte
at a time while allowing one raw output byte. Require exact raw equality and a
stable repeated terminal result. Corrupt a descriptor in the second frame and
require only the first two raw bytes to commit. Derive the first frame's exact
encoded, view, reference, raw, phrase, and expansion extents from its admitted
header and reject each workspace one unit short plus aggregate storage one byte
short. Also reject every final-byte truncation, a trailing byte, premature end
after a complete nonfinal frame drains, explicit reset, and unknown flags;
accept the canonical empty stream and leave nonterminal `Flush` starved.

### TVG-0413

For LZMW plus rANS profile calculation, use original size 17, frame size 10,
and entropy block size four. Require `F=10`, `S=40`, `K=10`, complete encoded
ceiling 5,456 bytes, and nine encoder records. Freeze a seven-byte short frame
at two entries and require 28 reference bytes and 2,228 encoded bytes. Exercise
block, payload, aggregate, and one-MiB frame limits. For decoding, use local
limits that yield four rANS views, ten LZMW phrases, eleven expansion entries,
and verify every aligned offset and partition rejection. Finally construct both
streaming directions solely from the calculated requirements and round-trip
raw `ABABX`.

### TVG-0414

For the LZMW plus rANS C ABI, initialize encode and decode configs through the
public initializer, lower limits to two-byte frames and entropy blocks, query
all three workspaces, allocate only those reported byte counts, and round-trip
binary `ABABX` through `marc_transform_process`. Reject each workspace one byte
short, a deliberately misaligned opaque region when alignment exceeds one, a
null transform output, and a nonzero reserved field.

### TVG-0415

For LZMW plus rANS public-ABI completion, use 64-byte outer frames, 64-byte
rANS blocks, at most four blocks, 256 maximum reference bytes, 288 maximum
payload bytes, and a 2,456-byte complete-frame ceiling. Through only the public
C lifecycle, round-trip empty input, every one-byte value, all 256 values, 257
zeroes, a 259-byte four-symbol pattern, 513 deterministic generated bytes, and
generated lengths 63, 64, and 65 twice for determinism. Encode 193 generated
bytes with unbounded, one-byte, 7/5-byte, and 13/17-byte input/output schedules
and require identical bytes and raw output. Corrupt, truncate, and extend the
fourth frame; require exactly the first 192 raw bytes, an untouched final
sentinel, and a stable repeated terminal error.

### TVG-0416

For LZMW plus rANS fuzz regressions, generate canonical `ABABX` through the
local streaming encoder with one five-byte frame and one 16-byte entropy block.
Present every strict prefix to a fresh decoder and require zero publication
plus a stable repeated error. Separately overwrite generic-frame extent fields
at offsets 16 through 39 with `ff`, and set byte 10 of the first 528-byte rANS
descriptor to one; both complete inputs must fail atomically. The fuzz entry
caps input at 8,192 bytes and drives both private complete-frame and public C
streaming decode with fixed arrays, byte-derived chunks, and an independent
12,320-call ceiling.

### TVG-0417

For `lzmw-rans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and decode it
with explicit `--codec lzmw-rans` and compare the restored file byte for byte.
Repeat encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
decode the valid archive again and require success, proving that failed
transactions do not damage their source. The CLI profile uses 65,536-byte raw
frames and rANS blocks, a 262,144-byte reference ceiling, four blocks, 2,112
descriptor bytes, a 262,176-byte payload ceiling, the public maximum-entry
default, and a 16-MiB aggregate policy. All primary, secondary, and aligned
opaque-view requirements must come only from the public C query.

### TVG-0418

For the LZMW plus rANS benchmark smoke, select `lzmw-rans`, use `README.md`,
and run one iteration. Before timing, encode into checked capacity
`80 + 4N + 2200K`, decode the exact encoded extent once, and require byte
equality. Require the timed encode and decode to reproduce those extents, then
report complete-stream ratio, both throughputs, every public workspace region,
and the larger direction-specific caller-owned total. The four-byte term must
remain the conservative one-reference-per-raw-byte LZMW ceiling.

### TVG-0419

For interoperability schema 25, generate the deterministic 8,193-byte fixture
with the unchanged repository recurrence, preserve the exact schema-24 archive
order, and append exactly one `lzmw-rans` archive as entry 36. Set
`schema_version` to 25 and `codec_set` to `marc-cli-v25`; record every archive's
size and SHA-256 only after local round-trip success. Verify exact order,
leaf-only names, hashes, foreign decoding, and byte-identical local re-encoding.
Swap the first two manifest entries and require rejection. Derive schema 24 by
removing only `lzmw-rans`, restoring `marc-cli-v24`, and then verify the
unchanged schemas 24 through 1.

### TVG-0420

For the first LZ77 plus tANS vector, begin with raw byte `41` and independently
require the canonical 16-byte Literal token
`00 00 00 00 00 00 00 00 00 00 00 00 41 00 00 00`. Normalize its byte
counts to table total 4096, yielding only `00:3840` and `41:256`. Apply the
documented step-2563 spread and inverse encode lookup. Traverse the token bytes
in reverse from live state 4096, prepend each low-bit chunk in decoder order,
and require final live state 5386: offset `0A 05`, five valid transition bits,
and physical bit byte `03`.

Assemble a generic frame independently with raw size 1, token size 16, payload
size 3, one block, and descriptor size 528. Require descriptor prefix
`10 00 00 00 03 00 00 00 0C 05 00 00 00 00 00 00`, frequency bytes
`00 0F` at offsets 16..17 and `00 01` at offsets 146..147, and zero everywhere
else in the frequency region. Append payload `0A 05 03` and compare every one
of the 587 bytes against output assembled only from the standalone LZ77
encoder, tANS encoder, and explicit generic serializers.

### TVG-0421

For the first LZ77 plus tANS validator tests, require the 587-byte hand vector
to reconstruct the exact Literal token in private staging. Re-encode that same
token with tANS block size five and require four blocks, deliberately proving
that entropy boundaries may split a token. Reject every proper frame prefix
and one trailing byte.

Use one-entry-short view storage, one-byte-short token staging, and an
aggregate workspace ceiling one byte below descriptor, payload, token, and
view bytes; each must fail before token mutation. Lower a normalized frequency
to invalidate the descriptor, and replace the second of two eight-symbol
block initial states with `FFFF`; the latter must report block index one while
preserving the entire token sentinel. Finally encode a token with invalid kind
`FF` and require tANS success followed by the stable LZ77 token error, and
raise the declared payload above the checked 12-bit transition ceiling for
early extent rejection.

### TVG-0422

For the first LZ77 plus tANS private raw decoder, reconstruct the same 587-byte
Literal frame into a three-byte sentinel span and require only its declared
first byte to become `41`. Independently serialize a Literal `A` followed by a
distance-one, length-four terminal match, tANS-encode those 32 token bytes, and
require private reconstruction of `AAAAA` to exercise forward overlap copying.

Submit raw staging one byte short and an aggregate limit one byte below
descriptor, payload, token, view, and raw bytes; both must fail before token or
raw mutation. Reuse the invalid second-block initial state and the valid tANS
payload carrying token kind `FF`; each must leave the raw sentinel unchanged.

### TVG-0423

For transactional LZ77 plus tANS publication, decode the 587-byte Literal
frame with three-byte raw and output sentinel spans. Require only the declared
first byte of each to become `41`. Supply a zero-byte output span for the
one-byte frame and require rejection before token or raw mutation. Reuse the
invalid later tANS state and the valid tANS payload carrying token kind `FF`;
both must preserve private raw and caller-output sentinels.

### TVG-0424

For the LZ77 plus tANS exact-frame planner, submit raw `A` and require the
frozen 16-byte Literal token, one descriptor, three payload bytes, and complete
extent 587 without serialized output. Repeat with block size five and require
four blocks that split the token byte region. Reject staging one byte short
without mutation, empty input, and a two-byte input against a one-byte frame.
Set the block-count ceiling to three for the four-block case, then restore four
and set aggregate descriptor-plus-payload-plus-token capacity one byte below
the exact first plan.

### TVG-0425

For the LZ77 plus tANS complete-frame writer, encode raw `A` and compare all
587 bytes against the independently assembled frame. With block size five,
write twice from the same raw input, require byte identity with each other and
with the component-built four-block frame, then decode through the combined
transactional frame decoder and require `A`. Supply a 586-byte sentinel output
for the default frame and require exact size 587 plus no output mutation.

### TVG-0426

For known-size streaming encoding, use raw `ABABX`, frame size two, and tANS
block size five. Build the reference by explicitly serializing the 80-byte
prefix and independently writing frames of two, two, and one raw byte. Require
the streaming transform to match it with one-byte input and output, and again
when `Flush` accompanies the first partial frame. Exercise short raw and
serialized storage, aggregate capacity one byte short, empty input, premature
EndInput, ResetBlock, and repeated ended calls.

### TVG-0427

For streaming decoding, encode the same three-frame `ABABX` stream locally and
decode it with one-byte input and output. Corrupt the second frame frequency
table and require only the first two raw bytes to commit. Exercise each storage
span one entry or byte short, aggregate capacity one byte short, every final
truncation class, one trailing byte, ResetBlock, empty stream, Flush starvation,
and EndInput after only the first frame.

### TVG-0428

For the LZ77 plus tANS profile calculator, use a 2,500,000-byte known stream
with default 65,536-byte frame and block sizes. Require `F=65,536`,
`S=1,048,576`, `K=16`, per-block payload ceiling 98,306, and complete encoded
frame capacity 1,581,400 bytes. For a 17-byte stream require 17 raw bytes, 272
token bytes, one descriptor, a 410-byte payload ceiling, and 994 encoded bytes.
Reject block count, payload, aggregate, and one-MiB profile bounds independently.
Derive decoder regions only from local limits, map stable error categories, and
construct a three-frame streaming round trip directly from calculated storage.

### TVG-0429

For the LZ77 plus tANS C ABI, initialize encoding for seven bytes
`41 42 41 42 41 42 58`, one seven-byte frame, and 16-byte entropy blocks.
Require seven primary bytes, 4,046 secondary bytes, zero view bytes, and
alignment one. Encode through only C11 functions. Reinitialize decoding with
4,096-byte frame/block limits, 6,000 dictionary bytes, 8,192 internal bytes,
and seven blocks; require 8,248 primary and 10,096 secondary bytes plus a
nonempty aligned view region, then reproduce the input. Reject views one byte
short and a nonzero reserved field while leaving the handle null.

### TVG-0430

For LZ77 plus tANS public completion, drive only the C ABI with 64-byte frames
and entropy blocks. Round-trip empty input, every one-byte value, `00..FF`, 257
zero bytes, a 259-byte four-symbol pattern, 513 deterministic generated bytes,
and generated lengths 63, 64, and 65; re-encode each byte-identically.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked generic-frame extents. Mutate its sequence,
remove the stream's final byte, and append one trailing zero independently.
Each case must publish the first 192 raw bytes, preserve the last sentinel, and
repeat the same terminal status and error positions.

### TVG-0431

For LZ77 plus tANS decoder fuzzing, cap each input at 8,192 bytes and exercise
both the complete-frame private decoder and incremental streaming decoder.
Use fixed storage for at most eight tANS views, 4,096 dictionary bytes, a
1,024-byte private frame, 4,096 committed output bytes, and one encoded frame
bounded by its 56-byte header, eight 528-byte descriptors, and 8,192 payload
bytes. Derive chunks from input bytes, enforce the process-result contract and
a 12,320-call ceiling, and seed the corpus with truncated `MARC` bytes.

Generate a canonical five-byte `ABABX` stream locally. Require every proper
prefix to fail without publishing a byte and to repeat the same error. Repeat
after saturating all three frame length fields and after changing one frequency
byte in the first tANS descriptor; both must preserve the caller sentinel and
sticky error position.

### TVG-0432

For `lz77-tans` CLI admission, reuse the repository-standard fixture formed by
repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode with the
exact selector and compare the restored file byte for byte. A second encode to
the existing destination must fail. Decoding `not-a-marc-stream` and a valid
archive with one appended `x` must fail and leave neither the destination nor
its `.tmp` sibling. Finally round-trip an empty file.

### TVG-0433

For the LZ77 plus tANS benchmark smoke, select `lz77-tans`, use repository
`README.md` as input, and request one iteration. Require a public-ABI
encode/decode equality check before timing and output fields for selector,
input and encoded sizes, ratio, both directional times and throughputs, all six
queried workspace extents, and peak workspace. Do not impose a speed or ratio
threshold.

### TVG-0434

For interoperability schema 26, retain the exact schema-25 archive order and
append one `lz77-tans` archive as entry 37. Set `schema_version` to 26 and
`codec_set` to `marc-cli-v26`; record each archive only after local decode
matches the deterministic 8,193-byte fixture. Require exact order, foreign
decode equality, byte-identical local re-encoding, and rejection after swapping
the first two entries. Derive schema 25 by removing only entry 37 and restoring
its version and codec set, then verify schemas 1 through 25 unchanged.

### TVG-0435

For schema-26 external admission, use artifacts and local executables at exact
revision `5b2aa31ba3333c311ad4086b3438915a6c3ce36d`. Require Ubuntu 26.04 to
verify the Windows/MSVC and Ubuntu 24.04 bundles, verify its locally generated
bundle, and require Windows/MSVC to verify that Ubuntu bundle. Each result must
report 37 archives and the exact revision; the verifier itself checks manifest
order, size, SHA-256, decoded fixture equality, and byte-identical re-encoding.

### TVG-0436

For the first LZSS plus tANS vector, begin with raw byte `41` and independently
require canonical Literal bytes `00 41`. Normalize both byte counts to 2048.
Apply the documented spread and reverse recurrence to obtain initial state
offset `06 00`, two zero transition bits, and physical payload `06 00 00`.

Assemble a generic frame independently with raw size 1, token size 2, payload
size 3, one block, and descriptor size 528. Require descriptor prefix
`02 00 00 00 03 00 00 00 0C 02 00 00 00 00 00 00`, frequency bytes
`00 08` at offsets 16..17 and 146..147, and zero everywhere else in the
frequency region. Append the payload and compare all 587 bytes with output
assembled only from the standalone LZSS encoder, tANS encoder, and explicit
generic serializers. This vector reserves no combined implementation.

### TVG-0437

For the first LZSS plus tANS validator, require the independent 587-byte frame
to reconstruct exactly `00 41` in private token staging. Re-encode the same
token with `B = 1` and require two blocks, proving that an entropy boundary may
split a Literal. Reject every proper frame prefix and one trailing byte.

Use one-entry-short view storage, one-byte-short token staging, and an
aggregate workspace ceiling one byte below descriptor, payload, token, and
view bytes; each must fail before token mutation. Corrupt a descriptor and the
second of two block states independently and require full staging preservation.
Entropy-decode `FF 41` successfully but require LZSS token failure at byte
offset zero. Reject `S > 2F`, a payload beyond the 12-bit transition ceiling,
and a non-tANS stream profile before unsafe work.

### TVG-0438

For the first LZSS plus tANS private raw decoder, reconstruct the 587-byte hand
frame into guarded staging and require only its first byte to become `41`.
Build canonical tokens for Literal `A` followed by Match `(distance=1,
length=5)` and require six `A` bytes, proving forward overlap-copy behavior.

Reject one-byte-short raw staging before token mutation. Count the exact raw
extent in aggregate workspace and reject a limit one byte below descriptor,
payload, token, view, and raw storage. Corrupt the second of two tANS blocks
and separately entropy-code an invalid second LZSS token; neither case may
modify any raw-staging sentinel.

### TVG-0439

For transactional LZSS plus tANS publication, decode the 587-byte Literal
frame through separate token, raw, and caller output spans. Require only the
declared output prefix to change. Repeat with Literal `A` plus overlapping
Match `(1,5)` and require six `A` bytes followed by an untouched guard.

Provide output one byte short and require rejection before token or raw
mutation. Independently corrupt the second tANS block and entropy-code an
invalid second LZSS token; both cases must preserve the complete caller output
sentinel.

### TVG-0440

For the LZSS plus tANS exact-frame planner, submit raw `A` and require token
bytes `00 41`, one 528-byte descriptor, three payload bytes, and complete
587-byte extent. Repeat with `B = 1`; require two blocks, 1,056 descriptor
bytes, four payload bytes, and complete 1,116-byte extent, proving that planning
permits a block boundary inside the Literal.

Plan six repeated `A` bytes twice with `B = 3`. Require the same canonical
11-byte Literal-plus-Match region and identical exact extents. Reject token
staging one byte short before mutation, empty and unexpected raw-frame extents,
one-block policy for a two-block plan, and aggregate storage one byte below
descriptor, payload, and token bytes.

### TVG-0441

For the LZSS plus tANS complete-frame writer, encode raw `A` and compare all
587 bytes with the independent frame. With `B = 1`, encode twice and require
byte identity with the separately assembled two-block frame, then decode it
through the transactional wrapper to raw `A`.

Encode six repeated `A` bytes with `B = 3` twice, require byte identity and an
11-byte canonical Literal-plus-Match region, then transactionally decode the
exact raw input. Supply a 586-byte guarded output for raw `A`; require the
exact 587-byte planned size and no output mutation.

### TVG-0442

For the LZSS plus tANS bounded streaming encoder, independently build the
80-byte prefix and concatenate exact complete-frame encodings for raw `AB`,
`AB`, and final `X`. Feed input and output one byte at a time and require exact
byte equality with that reference and stable repeated end status.

Submit one raw byte with nonterminal `Flush`; require only the prefix and keep
the partial frame open. Then submit the remainder with `EndInput` and require
the unchanged reference. Exercise empty known-size input, short raw, token, and
encoded-frame storage, aggregate workspace one byte short, premature end, and
`ResetBlock`. Every error is sticky and must satisfy the no-zero-progress
contract.

### TVG-0443

For the matching bounded streaming decoder, feed the encoder result one byte
at a time and drain raw output one byte at a time; require exact `ABABX` and
stable repeated end status. Corrupt the second frame's tANS descriptor and
require the first raw `AB` only, leaving the rest of a sentinel destination
unchanged and the error sticky.

Withhold one byte independently from encoded-frame, tANS-view, canonical-token,
and private-raw workspace, then set the aggregate limit one byte below their
sum; reject each before any failing-frame output is published. Also reject
every final-byte truncation, one trailing byte, `ResetBlock`, and premature
`EndInput`; accept an empty known-size stream and require nonterminal `Flush`
to report input starvation.

### TVG-0444

For LZSS plus tANS profile sizing with default 65,536-byte frame and entropy
block sizes, require 65,536 raw bytes, 131,072 token bytes, two descriptors,
196,612 payload bytes, and a 197,724-byte complete frame. For a 17-byte stream,
require 17 raw bytes, 34 token bytes, one descriptor, 53 payload bytes, and a
637-byte frame. Empty input reserves no frame-local encoder region.

Independently lower block count, payload, aggregate, and frame limits below
those derived requirements and reject with cleared output requirements. For
decoder limits, require `56 + max_internal_buffered_bytes` encoded bytes,
`min(max_dictionary_serialized_size, 2F)` token bytes, `F` private raw
bytes, and exactly `max_blocks_per_frame` views. Finally allocate every
reported region and require the streaming pair to round-trip `ABABX`.

### TVG-0445

For the LZSS plus tANS pure-C admission test, initialize the size-tagged encode
config, set raw and frame size seven with entropy blocks of sixteen bytes, and
require seven primary bytes, 621 secondary bytes, zero view bytes, and neutral
view alignment. Construct, process `ABABABX` through `EndInput`, and retain
the produced stream only through the common C transform lifecycle.

Initialize decoding with 4,096-byte raw and block limits, 6,000 token bytes,
8,192 internal bytes, and seven block views. Require 8,248 primary bytes,
10,096 secondary bytes, nonzero opaque view bytes, and nonzero alignment.
Round-trip exactly, then reject one-byte-short views with a null handle and
reject a nonzero reserved configuration field during requirements admission.

### TVG-0446

For public-ABI completion, drive only `marc_lzss_tans_*` through the common
process and destroy calls. Round-trip empty input, all 256 one-byte inputs, all
byte values in sequence, 257 zeros, a 259-byte binary pattern, 513 generated
bytes, and generated lengths 63, 64, and 65. Encode each twice and require byte
identity.

Encode 193 generated bytes as four 64-byte frames under one-byte, 7/5-byte,
and 13/17-byte input/output schedules; require the same stream and exact
decoding. Locate the fourth frame only through generic little-endian extents.
Independently alter its declared sequence field, truncate the final stream
byte, and append one trailing byte. Each decode must publish exactly the first
192 bytes, preserve the final sentinel, and repeat the same terminal error
position without consuming or producing another byte.

### TVG-0447

For bounded LZSS plus tANS decoder fuzzing, pass at most 8,192 bytes to both
the complete-frame staging decoder and the incremental decoder. Fix total raw
output at 4,096 bytes, one frame at 1,024 bytes, canonical token staging at
2,048 bytes, encoded payload at 8,192 bytes, and metadata at eight tANS views.
Derive chunks modulo 17 and 19 from input bytes and abort after the fixed
input-plus-output call ceiling. Seed only the reviewed truncated `MARC` magic.

Generate the permanent malformed cases from the local `ABABX` encoder. Submit
every proper prefix, replace generic frame length fields at byte offsets 16
through 39 with `FF`, and toggle the first serialized tANS frequency. Each
case must report error, produce no output, preserve an `A5` sentinel, and
repeat the same sticky error code and byte position.

### TVG-0448

For `marc --codec lzss-tans`, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 3,200 times. Encode and decode
with the exact selector and compare the restored file byte for byte. Reject a
second encode to the existing destination. Decode `not-a-marc-stream` and a
valid archive with one appended `x`; both must fail while leaving neither the
destination nor its `.tmp` sibling. Finally round-trip an empty file.

### TVG-0449

For the LZSS plus tANS benchmark smoke, select `lzss-tans`, use repository
`README.md` as input, and request one iteration. Require an untimed public-C
encode/decode equality check before timing and output fields for selector,
input and encoded sizes, ratio, both directional times and throughputs, all six
queried workspace extents, and peak workspace. Do not impose a speed or
compression-ratio threshold.

### TVG-0450

For interoperability schema 27, retain the exact schema-26 archive order and
append one `lzss-tans` archive as entry 38. Set `schema_version` to 27 and
`codec_set` to `marc-cli-v27`; record an archive only after its local decode
matches the deterministic 8,193-byte fixture. Require exact order, foreign
decode equality, byte-identical local re-encoding, and rejection after swapping
the first two manifest entries. Derive schema 26 by removing only
`lzss-tans.marc` and changing only version and codec set, then verify schemas
1 through 26 unchanged.

### TVG-0451

For the LZ78 plus tANS reservation, encode raw `A` with the existing canonical
LZ78 token rules and independently require exactly
`00 41 00 00 00 00 00 00`. Count seven `00` bytes and one `41`, then apply
the documented deterministic normalization to require frequencies `3584` and
`512`. Build the table using spread step 2563 and apply the reverse-state
recurrence to the fixed token bytes; require initial-state offset `0x046B`,
four zero transition bits, payload `6B 04 00`, and final-valid-bits value 4.

Serialize one frame with raw size 1, dictionary size 8, payload size 3, one
528-byte descriptor, and no hashes. Compare all 587 bytes with the sparse
header, descriptor, frequency, and payload definition in `docs/format.md`.
This test fixes the representation before a combined validator exists.

### TVG-0452

For the first LZ78 plus tANS validator, require the 587-byte hand vector to
reconstruct the exact Pair token in private staging. Re-encode that token with
tANS block size three and require three blocks, proving that entropy boundaries
may split a fixed-width token. Reject every proper frame prefix and one trailing
byte.

Use one-entry-short view storage, one-byte-short token staging, empty phrase
storage, and an aggregate workspace limit one byte below descriptor, payload,
token, view, and phrase bytes; each must fail before token mutation. Corrupt a
frequency descriptor and the initial state of the last block in the
three-block schedule independently; the later-block case must report block
index two while
preserving the complete staging sentinel. Entropy-code an unknown LZ78 tag and
require dictionary-layer rejection with token index and input offset. Reject
misaligned dictionary extent, excessive entropy extent, and another pipeline.

### TVG-0453

For private LZ78 plus tANS reconstruction, decode the independent raw-`A`
frame into a one-byte disposable staging span and require `41`. Entropy-code
the three canonical `ABAB` Pair tokens into five-symbol tANS blocks so that
token boundaries are split, then require iterative phrase expansion to exact
raw bytes `41 42 41 42`.

Use an empty raw span and require capacity rejection before token or phrase
workspace mutation. Set the aggregate ceiling to one byte below descriptors,
payload, tokens, views, phrase records, and raw staging and require all private
bytes to retain their sentinels. Independently corrupt the final tANS block and
entropy-code an unknown LZ78 tag; neither failure may alter raw staging.

### TVG-0454

For transactional LZ78 plus tANS publication, decode the independent raw-`A`
frame with one disposable raw byte and one caller output sentinel; require both
to become `41` only after success. Repeat with an empty output span and require
capacity rejection before token, phrase, or raw mutation.

Independently corrupt the final tANS block of a token-splitting frame and
entropy-code an unknown LZ78 tag. Give each case a full private raw span and
caller output initialized to `A5`; both failures must preserve the caller
output sentinel.

### TVG-0455

For LZ78 plus tANS exact-frame planning, pass raw `A`, one encoder record, and
eight token bytes. Require the canonical Pair token, one block, 528 descriptor
bytes, three payload bytes, and complete 587-byte frame extent without any
serialized output span. Repeat with block size three and require three blocks;
compare the planned payload and complete extent with independently encoded
component blocks.

Use empty encoder-record storage and seven token bytes independently and
require rejection before token mutation. Reject empty input and two raw bytes
under a one-byte frame declaration. With block size one, lower the block limit
from eight to seven, then restore it and lower the combined eight descriptors,
sixteen state bytes, token bytes, and encoder-record workspace by one; require
stable entropy-limit and aggregate-workspace errors respectively.

### TVG-0456

For the LZ78 plus tANS complete-frame writer, encode raw `A` and compare all
587 bytes with the independently reserved frame. Encode raw `ABAB` with a
five-byte entropy block so multiple tANS boundaries split fixed eight-byte
tokens; require two independent writes to be byte-identical and require the
complete decoder to reconstruct the original four bytes. Finally provide only
586 output bytes for the raw-`A` frame, initialize them to `A5`, and require
the exact 587-byte need plus complete sentinel preservation.

### TVG-0457

For the LZ78 plus tANS bounded streaming encoder, independently build the
80-byte prefix and concatenate exact complete-frame encodings for raw `AB`,
`AB`, and final `X`. Feed input and output one byte at a time and require exact
byte equality with that reference and stable repeated end status. Submit all
input with `EndInput` while prefix and frames drain and require the same bytes.

Submit one raw byte with nonterminal `Flush`; require only the prefix and keep
the partial frame open. Then submit the remainder with `EndInput` and require
the unchanged reference. Exercise empty known-size input, short raw, token,
encoder-record, and encoded-frame storage, aggregate workspace one byte short,
premature end, excess input, `ResetBlock`, and an unknown flag. Every error is
sticky and every result must satisfy the no-zero-progress contract.

### TVG-0458

For the matching LZ78 plus tANS bounded streaming decoder, feed the encoder
result one byte at a time and drain raw output one byte at a time; require exact
`ABABX` and stable repeated end status. Corrupt the second frame's tANS
descriptor and require only the first raw `AB`, leaving the rest of a sentinel
destination unchanged and making the malformed error sticky.

Independently shorten encoded-frame, view, token, private-raw, and phrase
storage and lower the combined exact-frame, token, raw, view, and phrase
workspace by one; require stable capacity or aggregate errors at frame-header
admission. Reject the last-byte truncation, one trailing byte, reset, and an
unknown flag. Accept the exact empty stream, keep `Flush` under starvation at
`NeedInput`, and latch a premature `EndInput` while the first decoded frame
drains before reporting truncation.

### TVG-0459

For the LZ78 plus tANS internal profile calculator, require the default
65,536-byte frame to produce 524,288 token bytes, eight tANS blocks, a 790,728-
byte encoded-frame ceiling, and 65,536 encoder records. Freeze a 17-byte final
frame with two maximum entries and require 136 token bytes and a 790-byte frame
ceiling; require empty input to reset every region to zero and alignment one.

Reject one-too-many blocks, a compressed payload limit below the 206-byte
17-frame ceiling, an aggregate encoder workspace one byte short, a frame above
the one-MiB profile bound, and invalid LZ78 parameters. Derive decoder regions
from small explicit hard limits, verify checked overflow rejection, and
partition encoder records plus aligned tANS-view/phrase storage. Alter counts
or offsets, shorten storage, and misalign where possible; require rejection
before typed-span publication. Finally construct both streaming directions
solely from reported requirements and round-trip exact `ABABX`.

### TVG-0460

For the LZ78 plus tANS C ABI, initialize encoding for raw `ABABX`, set raw
frames to two bytes, entropy blocks to five bytes, and small explicit limits.
Require primary size two, secondary size 2,218, nonzero aligned encoder views,
and a complete encode through the opaque transform. Reinitialize decoding
under the same hard limits; require primary size 8,248, secondary size 21, and
aligned mixed views, then reconstruct all five bytes exactly.

Shorten primary, secondary, and views independently and misalign the opaque
region where alignment exceeds one; every factory call must reject without
publishing a handle. Also reject a null handle destination and a nonzero
reserved field through the requirements query. Compile and execute the test as
C11 against the public installed-style header only.

### TVG-0461

For `lz78-tans` CLI admission, reuse the repository-standard deterministic
binary fixture and generic transactional test. Encode and decode enough bytes
to cross the fixed 65,536-byte raw-frame boundary, require exact restoration,
empty-input handling, destination overwrite refusal, malformed-input cleanup,
and strict rejection of a valid stream with one trailing byte. The test must
invoke only the CLI, whose adapter in turn uses only the public C lifecycle.

### TVG-0462

For the LZ78 plus tANS benchmark smoke, select `lz78-tans`, use `README.md`,
and run one iteration. Before timing, require a public-C encode/decode round
trip to reproduce every input byte. Require the report to identify the
selector, encoded size, ratio, directional throughput, each primary,
secondary, and views extent, and peak caller-owned workspace. Treat all speed
values as descriptive and impose no performance threshold.

### TVG-0463

For bounded LZ78 plus tANS decoder fuzzing, cap supplied input at 8,192 bytes,
total raw output at 4,096 bytes, raw frames at 1,024 bytes, token staging at
8,192 bytes, payload at 16,384 bytes, tANS views at eight, and LZ78 phrases at
1,024. Drive both the private complete-frame decoder and public C streaming
decoder with fixed storage, byte-derived chunks, and a finite call ceiling.
As permanent regressions, submit every proper prefix of the canonical `ABABX`
stream, overwrite all frame extent fields with `FF`, and alter one descriptor
frequency byte; require zero caller output, intact sentinels, and stable
repeated errors.

### TVG-0464

For LZ78 plus tANS public completion, use 64-byte raw frames and entropy
blocks, no more than eight tANS blocks, and the public three-workspace C
lifecycle only. Round-trip empty input, all 256 one-byte inputs, `00..FF`, 257
zero bytes, a 259-byte `00 FF 55 AA` pattern, deterministic 513-byte random
data, and lengths 63, 64, and 65. Encode a deterministic 193-byte fixture with
whole, `1/1`, `7/5`, and `13/17` input/output chunks and require identical
bytes. Locate its fourth frame from serialized extents; independently corrupt
its sequence, truncate its last byte, and append one byte. Each decoder must
commit exactly the first 192 raw bytes, preserve the last output sentinel, and
repeat the same terminal error position without progress.

### TVG-0465

For interoperability schema 28, retain the exact schema-27 archive order and
append one `lz78-tans` archive as entry 39. Set `schema_version` to 28 and
`codec_set` to `marc-cli-v28`; record an archive only after its local decode
matches the deterministic 8,193-byte fixture. Require exact order, foreign
decode equality, byte-identical local re-encoding, and rejection after swapping
the first two manifest entries. Derive schema 27 by removing only
`lz78-tans.marc` and changing only version and codec set, then verify schemas
1 through 27 unchanged.

### TVG-0466

For the LZW plus tANS reservation, encode raw `A` with the existing canonical
LZW rules and independently require packed bytes `41 00`: code 65 at width
nine followed by seven zero padding bits. Count one `00` and one `41`, then
apply deterministic normalization to require frequency 2048 for each. Build
the table using spread step 2563 and apply the reverse-state recurrence to the
fixed packed bytes; require initial-state offset `0x000C`, two zero transition
bits, payload `0C 00 00`, and final-valid-bits value 2.

Serialize one frame with raw size 1, dictionary size 2, payload size 3, one
528-byte descriptor, and no hashes. Compare all 587 bytes with the sparse
header, descriptor, frequency, and payload definition in `docs/format.md`.
This test fixes the representation before a combined validator exists.

### TVG-0467

For the first LZW plus tANS validator, require the 587-byte hand vector to
reconstruct exact packed bytes `41 00` in private staging. Re-encode those
bytes with tANS block size one and require two blocks, proving that entropy
boundaries may split one nine-bit code while preserving byte boundaries.
Reject every proper frame prefix and one trailing byte.

Use one-entry-short view storage, one-byte-short packed staging, one-entry-
short phrase storage for packed `AB`, and an aggregate workspace limit one
byte below descriptor, payload, packed, view, and phrase bytes; each must fail
before packed mutation. Corrupt a frequency descriptor and the initial state
of the second one-byte block independently; the later-block case must report
block index one while preserving complete staging. Entropy-code packed
`41 80` and require LZW-layer rejection of nonzero final padding. Reject an
excessive packed extent, an excessive tANS payload extent, and another
pipeline.

### TVG-0468

For private LZW plus tANS reconstruction, decode the frozen raw-`A` frame into
a one-byte sentinel and require `41`. Separately pack codes 65, 66, 256, and
258 as `41 84 00 14 08`, divide those bytes into tANS blocks of at most two
symbols, and require raw `ABABABA`; this crosses both packed-code and phrase
edges and exercises `KwKwK`. Reject missing raw capacity and aggregate storage
one byte short before packed mutation. Entropy-code invalid padded bytes
`41 80` and corrupt a later tANS block independently; each must preserve the
complete raw sentinel.

### TVG-0469

For transactional LZW plus tANS publication, decode the frozen raw-`A` frame
through a one-byte private raw sentinel into a two-byte caller sentinel. Require
only the declared first byte to become `41`. Repeat the block-size-two
`ABABABA` and `KwKwK` case and require equal private and published output.
Provide caller output one byte short and require packed staging, phrase
records, private raw staging, and caller output all to remain unchanged.
Independently corrupt a later tANS block and entropy-code LZW bytes `41 80`
with invalid high padding; neither failure may publish any caller byte.

### TVG-0470

For exact LZW plus tANS planning, feed raw `A` to the standalone LZW planner
and encoder, require packed bytes `41 00`, then independently plan its one tANS
block and require descriptor extent 528, payload extent 3, and complete frame
extent 587 without serialized output. For raw `ABABABA`, require codes 65, 66,
256, 258 packed as `41 84 00 14 08`; with `B=2`, require three blocks, 1,584
descriptor bytes, and deterministic repeated payload and complete-frame
extents matching independent block plans. Reject a short encoder workspace and
packed span before packed mutation, excessive block count, aggregate storage
one byte short, empty input, and a raw extent inconsistent with the stream
header.

### TVG-0471

For deterministic LZW plus tANS frame encoding, encode raw `A` through the
combined encoder and compare all 587 bytes with the independently assembled
vector. Encode raw `ABABABA` twice with `B=2`, require byte-identical complete
frames, then decode one through the transactional combined decoder and require
the original seven bytes. Give the raw-`A` encoder a 586-byte destination
filled with `A5` and require every byte to remain unchanged.

### TVG-0472

For bounded LZW plus tANS streaming encoding, use raw `ABABX`, outer frames of
two bytes, and tANS blocks of two bytes. Independently serialize the ordinary
80-byte prefix and append each exact frame from the complete-frame planner and
encoder. Require identical output when both input and output capacities are
one byte. Verify that `Flush` leaves a partial frame open, `EndInput` retained
while every region drains produces the same stream, workspace and aggregate
shortages fail stably, empty input emits only the prefix, and premature end,
excess input, `ResetBlock`, and unknown flags are rejected.

### TVG-0473

For bounded LZW plus tANS streaming decoding, feed that canonical multi-frame
stream with one-byte input and output capacities and require raw `ABABX`.
Corrupt the second frame's tANS model and require only first-frame raw `AB` to
be published. Make each serialized-frame, view, packed, raw, and phrase region
one entry short in turn, then set aggregate storage one byte below the exact
sum and require early failure. Reject every final-byte truncation, one trailing
byte, `ResetBlock`, and an unknown flag; accept the empty prefix-only stream;
keep `Flush` non-terminal; and retain premature `EndInput` while draining the
first valid frame before reporting the missing later frame.

### TVG-0474

For the LZW plus tANS profile calculator, freeze a ten-byte largest frame with
maximum width 16 and four-byte entropy blocks. Require 20 packed bytes, five
blocks, 2,640 descriptor bytes, 40 payload-ceiling bytes, a 2,736-byte complete
frame region, and nine aligned encoder records. A seven-byte short frame with
eight-byte blocks requires 14 packed bytes, two descriptors, 25 payload bytes,
and 1,137 complete-frame bytes. Require canonical zero regions and alignment
one for empty input. Exercise block, payload, aggregate, frame, parameter, and
decoder-limit failures. For decoder limits of 64 raw bytes, 128 packed bytes,
four blocks, and 300 dictionary entries, require 112 conservative LZW phrase
records after an aligned tANS-view region. Reject altered, short, and
misaligned typed storage. Finally allocate only the reported regions and
round-trip `ABABX` through two-byte frames and two-byte tANS blocks.

### TVG-0475

For the first LZW plus tANS pure-C vector, initialize encode configuration for
raw `41 42 41 42 58`, two-byte frames and entropy blocks, width 16, and bounded
local limits. Require primary bytes 2, secondary bytes 1,126, and nonempty
aligned opaque storage. Encode through only the C11 lifecycle. Reinitialize for
decode, require primary bytes 8,248 and secondary bytes 20, then reproduce all
five bytes. Reject every one-byte-short region, deliberately misaligned views,
a null transform output pointer, and a nonzero reserved field; every failed
factory call must leave the handle null.

### TVG-0476

For LZW plus tANS public-ABI completion, reuse the established LZW data,
chunking, and malformed-final-frame schedules with 64-byte frames and tANS
blocks and `Σ(2 + ceil(12n/8))` payload storage. Round-trip empty input, every
one-byte value, `00..FF`, 257 zero bytes, a 259-byte four-symbol pattern,
deterministic 513-byte generated input, and generated lengths 63, 64, and 65;
re-encode every case byte-identically. For 193 generated bytes, compare the
unchunked stream with `(1,1)`, `(7,5)`, and `(13,17)` schedules. Corrupt the
fourth frame sequence, remove the final byte, and append one zero independently;
each must publish exactly the first 192 bytes, preserve the final sentinel, and
repeat identical terminal status and error positions.

### TVG-0477

For LZW plus tANS fuzz regressions, generate canonical `ABABX` with one
five-byte frame and a 16-byte tANS block. Give every strict stream prefix to a
fresh decoder and require zero publication plus one stable repeated error.
Separately overwrite generic-frame extent fields at offsets 16 through 39 with
`ff`, and toggle the first model frequency at descriptor offset 16; both full
streams must fail atomically. The live dual-decoder harness accepts at most
8 KiB, uses only fixed arrays, and stops after at most maximum input plus
maximum output plus 32 calls.

### TVG-0478

For `lzw-tans` CLI admission, reuse the repository-standard binary fixture and
transaction script with the explicit selector. Require an exact round trip,
existing-destination rejection, malformed-frame rejection without a final or
temporary destination, strict trailing-byte rejection with the same atomicity,
and exact empty-stream round trip. Run the identical script against MSVC and
ClangCL Release executables.

### TVG-0479

For the LZW plus tANS benchmark smoke, select `lzw-tans`, use `README.md`, and
run one iteration. Require successful public-C configuration and workspace
queries in both directions, an untimed byte-exact round trip before timing,
finite encode/decode measurements, and reported primary, secondary, views,
and peak workspace bytes. Do not assert a throughput or compression threshold.

### TVG-0480

For interoperability schema 29, retain the exact schema-28 archive order and
append one `lzw-tans` archive as entry 40. Set `schema_version` to 29 and
`codec_set` to `marc-cli-v29`; record an archive only after its local decode
matches the deterministic 8,193-byte fixture. Verify exact count, order,
leaf-only names, sizes, SHA-256 values, fixture decode, and byte-identical
re-encoding. Reject a manifest with its first two archives swapped. Derive
schema 28 by removing only `lzw-tans.marc` and changing only version and codec
set, then verify schemas 28 through 1 unchanged.

### TVG-0481

For the first LZD plus tANS vector, begin with raw byte `41` and independently
derive canonical terminal-token bytes `41 00 00 00 FF FF FF FF`. Feed exactly
those eight finalized bytes to standalone tANS planning. Require normalized
frequencies `00:1536`, `41:512`, and `FF:2048`, final valid-bit count 3, and
payload `08 03 9B 00`.

Independently serialize a generic header declaring raw extent 1, dictionary
extent 8, payload extent 4, one entropy block, and 528 descriptor bytes.
Serialize the sparse tANS descriptor and append the payload; compare all 588
bytes with the separately recorded frame representation. Do not invoke any
future combined LZD/tANS implementation in this vector.

### TVG-0482

For the first LZD plus tANS complete-frame validator, admit the independently
frozen 588-byte raw-`A` frame and require reconstruction of token bytes
`41 00 00 00 FF FF FF FF` into private staging with no phrase records. Repeat
with three-byte tANS blocks so block boundaries split both references and the
token. Corrupt a later descriptor and require the untouched token sentinel;
encode an invalid forward reference through valid tANS and require the LZD
error only after complete entropy reconstruction. Reject each caller-owned
workspace one entry short, the aggregate allowance one byte short, a truncated
frame, trailing data, and a non-tANS stream declaration.

### TVG-0483

For LZD plus tANS private reconstruction, decode the independent raw-`A` frame
into a one-byte sentinel and require `41`. Independently serialize the valid
reference pairs `(A,B)` and `(256,256)`, tANS-code them in five-byte blocks,
and require non-recursive reconstruction of `ABABAB` across four entropy
blocks. Reject raw staging and the expansion stack one entry short before
token mutation, lower their aggregate allowance by one byte, and preserve the
raw sentinel after a later descriptor failure or entropy-decoded invalid LZD
reference.

### TVG-0484

For LZD plus tANS transactional publication, decode the independent raw-`A`
frame into a three-byte sentinel and require only its first byte to become
`41`. Publish the phrase-bearing `ABABAB` frame and require all six bytes at
once. Supply a one-byte-short output while pre-filling token and raw staging
with distinct sentinels and require every region unchanged. Corrupt a later
tANS descriptor and separately entropy-code an invalid LZD reference; both
must preserve the complete caller-output sentinel.

### TVG-0485

For LZD plus tANS exact-frame planning, submit raw `A` and require canonical
tokens `41 00 00 00 FF FF FF FF`, zero encoder records, one tANS block, 528
descriptor bytes, four payload bytes, and a 588-byte complete extent. Plan raw
`ABABAB` twice with five-byte entropy blocks and require identical sixteen-byte
token staging, four blocks, payload extent, token/phrase counts, and complete
extent. Reject encoder records and token staging one entry short without
changing the token sentinel, then lower aggregate workspace by one byte and
reject empty or mismatched raw extents.

### TVG-0486

For LZD plus tANS deterministic frame encoding, encode raw `A` and require all
588 bytes to equal the independently assembled frame. Encode raw `ABABAB` with
five-byte tANS blocks twice into differently initialized destinations and
require byte identity, then decode transactionally to the original. Provide a
587-byte destination for raw `A` and separately an empty input that fails in
planning; both complete output sentinels must remain unchanged.

### TVG-0487

For LZD plus tANS bounded streaming encoding, independently concatenate the
80-byte prefix and exact complete-frame encodings of raw `AB`, `AB`, and final
`X`. Feed the streaming encoder one input byte and one output byte at a time and
require exact equality. Repeat with all input and `EndInput` on the first call
while one byte drains per call, and require finish to remain latched. Flush
after one raw byte and require canonical frames after the rest arrives. Reject
each short storage region, aggregate workspace one byte short, premature and
excess input, `ResetBlock`, and unknown flags; accept the exact empty prefix.

### TVG-0488

For LZD plus tANS bounded streaming decoding, generate the canonical `ABABX`
stream with two-byte raw frames and two-byte entropy blocks. Decode with
one-byte input and output and require exact raw equality plus stable repeated
end. Corrupt the second frame's first tANS descriptor and require only the
first raw `AB` frame to be published while every later output sentinel remains
unchanged. Reject each encoded, view, token, raw, phrase, and expansion region
one entry or byte short; reject the aggregate bound one byte short, final-byte
truncation, one trailing byte, reset, unknown flags, and premature finish after
one valid frame. Accept the exact empty prefix and treat flush without input as
starvation rather than termination.

### TVG-0489

For the LZD plus tANS profile calculator, freeze a ten-byte active frame from a
17-byte stream. Require 40 worst-case token bytes, five encoder entries, ten
four-byte tANS blocks, and a 5,416-byte complete serialized-frame capacity.
Freeze a seven-byte short frame with two retained entries and require 32 token
bytes and 2,224 serialized bytes; require empty input to report zero storage
and alignment one. Exercise block-count, payload, aggregate, frame, and
arithmetic limits. Recompute the decoder's block/phrase/expansion offsets,
partition aligned opaque allocations, and reject altered, short, or misaligned
layouts. Finally construct both streaming directions exclusively from the
reported requirements and round-trip binary `ABABX`.

### TVG-0490

For the first LZD plus tANS pure-C vector, initialize the public encode config,
set two-byte raw and entropy blocks, query exactly 2 primary and 2,196 secondary
bytes, construct through the shared library, and encode binary `ABABX`. Reuse
the fixed local limits with a decode config, query exactly 8,248 primary and 10
secondary bytes, decode, and compare all five bytes. Reject primary, secondary,
and views storage one byte short, a deliberately misaligned views pointer when
alignment exceeds one, a null transform output, and a non-zero reserved field;
require the transform pointer to remain null after every factory failure.

### TVG-0491

For LZD plus tANS public-ABI completion, use 64-byte outer frames, 64-byte tANS
blocks, at most 256 canonical token bytes, four 528-byte descriptors, and the
documented twelve-bit-plus-state payload ceiling. Through only public C
operations, test empty input, every one-byte value, all byte values, long zeros,
repeated binary patterns, deterministic generated bytes, and sizes 63, 64, and
65. Require identical 193-byte four-frame encoding and successful decoding
under whole-buffer, one-byte, 7/5-byte, and 13/17-byte schedules. Independently
corrupt the fourth sequence number, truncate its final byte, and append one
byte; require exactly 192 committed raw bytes, an unchanged final sentinel,
and an identical repeated terminal error position.

### TVG-0492

For permanent LZD plus tANS fuzz regressions, encode the single-frame raw input
`ABABX`. Submit every proper prefix and require failure without changing the
five-byte `a5` output sentinel. Independently fill the generic frame's six
four-byte extent fields with `ff`, then set the tANS descriptor flags byte to
one; both complete mutations must fail atomically and retain the same sticky
error category and byte position. Seed the bounded sanitizer target only with
the reviewed five-byte `MARC\n` truncated magic.

### TVG-0493

For `lzd-tans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and decode
with the explicit selector and compare the restored file byte for byte. Repeat
encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 262,144`, `K = 4`, `D = 2,112`, and
`P = 393,224` under a 16-MiB aggregate policy. Actual primary, secondary, and
aligned opaque-view workspace requirements must come only from the public C
query.

### TVG-0494

For `lzd-tans` benchmark smoke, run
`marc_benchmark lzd-tans README.md 1`. Require a complete verified round trip
before timing and the standard codec name, input and encoded byte counts,
ratio, encode/decode seconds and MiB/s, all six direction-specific workspace
extents, and peak caller reservation. Bound output with the checked
`80 + 12*ceil(N/2) + 2176K` expression. Treat every measured value as a local
observation rather than a frozen threshold.

### TVG-0495

For interoperability schema 30, retain the exact schema-29 archive order and
append one `lzd-tans` archive as entry 41. Set `schema_version` to 30 and
`codec_set` to `marc-cli-v30`; record an archive only after its local decode
matches the deterministic 8,193-byte fixture. Verify exact count, order,
leaf-only names, sizes, SHA-256 values, fixture decode, and byte-identical
re-encoding. Reject a manifest with its first two archives swapped. Derive
schema 29 by removing only `lzd-tans.marc` and changing only version and codec
set, then verify schemas 29 through 1 unchanged.

### TVG-0496

For the first LZMW plus tANS vector, begin with raw byte `41` and independently
derive canonical reference bytes `41 00 00 00`. Feed exactly those four
finalized bytes to standalone tANS planning. Require normalized frequencies
`00:3072` and `41:1024`, final valid-bit count 3, and payload `FB 02 07`.

Independently serialize a generic header declaring raw extent 1, dictionary
extent 4, payload extent 3, one entropy block, and 528 descriptor bytes.
Serialize the sparse tANS descriptor and append the payload; compare all 587
bytes with the separately recorded frame representation. Do not invoke any
future combined LZMW/tANS implementation in this vector.

### TVG-0497

For the first LZMW plus tANS complete-frame validator, admit the independently
frozen 587-byte raw-`A` frame and require reconstruction of reference bytes
`41 00 00 00` into private staging with no phrase records. Repeat with
three-byte tANS blocks so a block boundary splits the reference. Corrupt a
later descriptor flag and require the untouched reference sentinel; encode an
invalid forward reference through valid tANS and require the LZMW error only
after complete entropy reconstruction. Reject each caller-owned workspace one
entry short, the aggregate allowance one byte short, a truncated frame,
trailing data, and a non-tANS stream declaration.

### TVG-0498

For private LZMW plus tANS reconstruction, decode the independently frozen
raw-`A` frame into guarded raw staging and require `41`. Build references for
raw `ABABAB`, divide them into five-byte tANS blocks so entropy boundaries cut
through references, and require iterative phrase expansion to the exact six
bytes. Reject raw staging and the conservative expansion stack one entry short
before reference staging changes; corrupt a later descriptor and require the
raw sentinel unchanged.

For transactional publication, decode raw `A` into a larger guarded caller
span and require only its declared first byte to change. Repeat a two-literal
frame with caller capacity one byte short and require reference staging,
private raw staging, and caller output all unchanged.

### TVG-0499

For LZMW plus tANS exact-frame planning, begin with raw `41`, zero encoder
records, and four-byte reference staging. Require canonical reference
`41 00 00 00`, one tANS block, 528 descriptor bytes, three payload bytes, and
the exact 587-byte complete-frame extent without serialized output. For raw
`ABABAB`, use five-byte blocks and require two plans to produce identical
reference bytes and exact extents. Reject encoder records and reference staging
one entry short before staging mutation, aggregate workspace one byte short,
empty input, and mismatched declared frame size.

### TVG-0500

For deterministic LZMW plus tANS frame encoding, pass raw `41` through the
exact planner and encoder and compare all 587 output bytes with the independent
composition of standalone LZMW, standalone tANS, and generic serializers. For
raw `ABABAB` with five-byte entropy blocks, encode twice from differently
initialized destinations, require byte identity, and decode through the
combined transactional decoder. Give output capacity 586 for raw `A` and
require every sentinel unchanged; repeat with empty planner input and a full-
size guarded destination.

### TVG-0501

For bounded LZMW plus tANS streaming encoding, use raw `ABABX`, outer frames of
two bytes, and tANS blocks of two reference bytes. Independently concatenate
the stream prefix and each complete-frame encoder result. Feed and drain one
byte at a time and require exact equality. Repeat by giving `EndInput` once
before the prefix drains, and by issuing `Flush` after the first raw byte.
Reject raw, reference, encoded, and encoder-table storage one entry short,
aggregate workspace one byte short, premature final input, excess input,
`ResetBlock`, and an unknown flag. Verify the prefix-only empty stream.

### TVG-0502

For bounded LZMW plus tANS streaming decoding, generate canonical `ABABX`
through the local streaming encoder with two-byte outer frames and two-byte
tANS blocks. Feed encoded input and raw output one byte at a time and require
exact recovery. Corrupt a descriptor in the second frame and require only the
first raw frame to be published. Reject serialized storage, tANS views,
reference staging, private raw staging, and aggregate live bytes one unit
short. Reject truncation, trailing bytes, and `ResetBlock`; accept the empty
prefix-only stream and treat `Flush` during input starvation as `NeedInput`.

### TVG-0503

For the LZMW plus tANS profile calculator, freeze original size 17, frame size
10, and entropy block size 4. The largest frame has a 40-byte canonical LZMW
ceiling, ten tANS blocks, 5,280 descriptor bytes, 80 worst-case payload bytes,
and therefore a 5,416-byte complete-frame region. Require nine encoder entries.
Separately derive an aligned decoder layout from small explicit limits, reject
an altered offset and overflowing encoded capacity, and construct a five-byte
multi-frame streaming round trip solely from returned requirements.

### TVG-0504

For the LZMW plus tANS C boundary, initialize an encode configuration in a
pure C11 translation unit, reduce frame and entropy blocks to two bytes, and
encode bytes `41 42 41 42 58` using only queried workspace. Initialize decode
independently, reproduce the five bytes, then shorten each workspace class by
one, offset the aligned region by one where meaningful, pass a null transform
result, and set `reserved2`. Every negative case must reject before publishing
a transform.

### TVG-0505

For LZMW plus tANS public completion, use 64-byte raw and entropy blocks and
the exact 256-byte maximum canonical reference region. Exercise empty input,
all 256 one-byte values, `00..FF`, 257 zero bytes, a 259-byte four-symbol
pattern, deterministic 513-byte input, and generated lengths 63, 64, and 65.
Encode each case twice and decode it through only the public C lifecycle.

For 193 generated bytes, compare the unchunked stream with `(1,1)`, `(7,5)`,
and `(13,17)` encode/decode schedules. Locate the fourth frame after the
80-byte prefix and three checked frame extents. Mutate its sequence, remove the
stream's final byte, and append one zero independently. Each case must publish
the first 192 raw bytes, preserve the last sentinel, and repeat the same sticky
terminal status and error positions.

### TVG-0506

For LZMW plus tANS fuzz regressions, encode canonical raw bytes
`41 42 41 42 58` with the repository streaming encoder. Present every strict
prefix of that stream independently to a fresh bounded decoder and require
zero output, terminal error, unchanged sentinels, and repeated sticky status
and position. Independently overwrite generic frame bytes 16 through 39 with
`FF`, then alter descriptor byte 10; both complete streams must fail with the
same atomic and sticky guarantees.

Compile the fuzz entry under both strict local toolchains without executing an
open-ended campaign. The entry caps arbitrary input at 8,192 bytes, output at
4,096 bytes, frame size at 1,024 bytes, entropy views at eight blocks, and
process calls at input plus output plus 32.

### TVG-0507

For `lzmw-tans` CLI admission, reuse the repository-standard binary fixture
formed by repeating `ABRACADABRA-0123456789\n` 320 times. Encode and decode
with the explicit selector and compare the restored file byte for byte. Repeat
encode to the same destination and require refusal. Decode
`not-a-marc-stream` and a valid archive with one appended `x`; both must fail
and leave neither the requested destination nor its sibling `.tmp`. Finally
round-trip an empty file.

The CLI profile fixes `F = 65,536`, `S = 262,144`, `K = 4`, descriptor bytes
`528K = 2,112`, payload ceiling `12S/8 + 2K = 393,224`, at most 65,536
generated entries, and a 16-MiB aggregate policy. Actual primary, secondary,
and aligned opaque-view requirements must come only from the public C query.

### TVG-0508

For the `lzmw-tans` benchmark smoke, use the same public configuration and
requirements profile as the CLI. For input extent `N`, derive nonempty outer
frame count `K = ceil(N/65536)` and require checked destination capacity
`80 + 6N + 2176K`. Construct fresh public transforms, encode once, decode the
exact produced extent once, and compare every byte before timing. Thereafter
measure encode and decode separately and require their produced extents to
match the verified values. Report primary, secondary, aligned views, and total
workspace for both directions; do not treat a small-input throughput value as
a stable performance assertion.

### TVG-0509

For interoperability schema 31, freeze the exact schema-30 archive order and
append exactly one `lzmw-tans` archive as entry 42. Set `schema_version` to 31
and `codec_set` to `marc-cli-v31`; record every archive size and SHA-256 only
after local round-trip success. Verify exact order, leaf-only names, hashes,
foreign decoding, and byte-identical local re-encoding. Swap the first two
manifest entries and require rejection. Derive schema 30 by removing only
`lzmw-tans`, restoring `marc-cli-v30`, and then verify the unchanged schemas
30 through 1.

### TVG-0510

For the experimental format-2 one-Literal vector, start a fresh
`LzssFieldContext` frame and map typed `Literal(0x41)` to token-kind context 0,
alphabet 2, value 0, followed by first-Literal context 3, alphabet 256, value
65. Feed those two decisions to an independent transcription of the published
Dynamic Range variant-1 arithmetic with separate fresh models. Before using the
result, require the same transcription to reproduce variant 1's published
single-byte `A` payload `00 40 FF FF BF 00`. The contextual pair must then
produce `00 20 7F FF BF 00`.

Construct the 64-byte `MRF2` header from explicit little-endian stores: raw and
token counts one, event and decision counts two, payload size six, descriptor
size sixteen, and every optional or reserved extent zero. Prefix the payload
with a descriptor declaring two decisions, six payload bytes, 31 contexts, and
zero flags. Independently construct the 112 pre-frame bytes from the 64-byte
version-2 prefix, default bounded LZSS parameters, contextual Dynamic Range
parameters, and context extension. Empty input consists of only those 112
bytes. No native structure serialization or implemented combined encoder is a
vector source.

### TVG-0511

Use TVG-0510's explicit 112-byte stream header and 86-byte one-Literal frame as
the first Format 2 parser vectors. Independently set each nonzero byte at its
documented offset in zero-filled fixed arrays. Require the stream parser to
recover frame size 64, original size one, LZSS `(65536, 5, 258)`, model total
32768, and 31 contexts. Require frame preflight to recover raw/token counts one,
event/decision counts two, payload extent six, and total extent 86.

For every proper prefix, require truncation and unchanged caller output. Mutate
magic, versions, fixed sizes, flags, every algorithm/variant identity, all
three reserved regions, LZSS bounds, context count, sequence, frame counts,
optional extents, descriptor counts/flags/reserved bytes, and representative
local limits. Require stable categorized rejection before layout publication.
Append one byte after the canonical frame and require the reported extent to
remain 86 so the next frame is not consumed.

### TVG-0512

Define typed tokens directly as values rather than deriving them from a byte
serialization. Validate empty input; `Literal('A'), Match(1,5)` as a six-byte
overlap frame; and five Literals followed by `Match(5,10), Literal('X')` as a
sixteen-byte frame. Require exact final token and raw counts.

For negative vectors, alter a later distance beyond produced history, lengths
to 4 and 259, unused fields, the token kind, declared token count, declared raw
extent, variant-2 parameters, local LZ distance, token-storage capacity, and
aggregate output. Require the validator to stop at the first invalid token,
retain the preceding token/raw counts, categorize policy limits separately,
and leave a single-token caller output unchanged on failure.

### TVG-0513

Reconstruct an empty frame without changing a sentinel; one Literal as `A`;
`Literal('A'), Match(1,5)` as `AAAAAA`; and three Literals followed by
`Match(3,6), Literal('X')` as `ABCABCABCX`. Require bytewise overlap behavior,
the exact declared output size, and no write beyond that extent.

For atomic negative vectors, use a later invalid distance, an output one byte
too small, a local LZ-distance policy below the profile requirement, and an
output span deliberately overlapping the token array. Prefill ordinary output
with `CC` and snapshot aliased token values. Require every gate failure before
the first write, preservation of all sentinels/token values, and stable
validation details for malformed input.

### TVG-0514

Map one fresh-frame `Literal('A')` to symbol operations `(context 0, alphabet
2, value 0)` and `(context 3, alphabet 256, value 65)`. For state tracking,
map `Literal('A'), Literal('B'), Match(2,10), Literal('C')`; require the second
and final Literal to use remembered-literal high-nibble context 8, the Match to
use previous-Literal contexts 1 and 21, length class 2 with two bypass bits,
distance context 25, and distance class 1 with one bypass bit. Require eleven
events, twelve decisions, four tokens, and thirteen reconstructed raw bytes.

Independently mutate operation kind, context, alphabet, symbol range, unused
fields, bypass width/value, reconstructed length/distance, truncation, trailing
operations, and every declared count. Exercise operation-storage, frame, and
aggregate-output limits. For short or deliberately aliased output spans,
snapshot all typed-token values and require rejection before the first write;
on success require only the declared token extent to change.

### TVG-0515

Use TVG-0514's four typed tokens as independent forward input and require an
exact plan of eleven operations, twelve decisions, four tokens, and thirteen
raw bytes. Materialize the documented operation sequence field by field, feed
it to the inverse validator, and require exact recovery of every typed-token
field. Separately require `Literal('A'), Match(1,5)` to omit both zero-width
bypass operations, and `Literal('A'), Match(1,258)` to emit length class 7,
seven bypass bits with value 126, distance context 30, and twelve total
decisions.

Validate empty input and propagate malformed typed-token, declared-token,
premature-raw-end, trailing-token, parameter, and limit failures. Prefill short
and excess operation spans with sentinels and construct a deliberately aliased
token/operation span; require every failed gate to preserve all storage and a
successful call to write only the exact planned extent.

### TVG-0516

First decode TVG-0510's published one-Literal payload `00 20 7F FF BF 00`
through requests `(context 0, alphabet 2)` and `(context 3, alphabet 256)`;
require values 0 and 65, two events, two decisions, and exact six-byte payload
consumption.

For a bypass-bearing vector, independently transcribe variant-1 unsigned
interval, delayed-carry, normalization, and five-shift termination arithmetic.
Encode `(cumulative, frequency, total)` decisions `(1,1,2)`, `(2,1,8)`,
`(0,1,2)`, `(1,1,2)`, `(1,1,17)`, `(0,1,2)`, corresponding to values 1, 2,
two LSB-first bypass bits `0,1`, value 1, and bypass bit 0. Require payload
`00 A4 3C 3C 38 00`, five events, and six decisions. The reviewed generator
must truncate the shifted low word to 32 bits exactly as specified; discard an
earlier calculation that retained those discarded high bits.

Mutate descriptor counts, context count, payload extent, canonical leading
zero, expected context/alphabet, bypass widths, decision budget, truncation,
trailing bytes, and local table/model limits. Require unchanged value outputs,
stable first errors, exact accepted counters/offsets, begin-before-use, and one
consistent terminal result.

### TVG-0517

Feed TVG-0516's published one-Literal payload `00 20 7F FF BF 00` through the
direct bridge with one token, two events, two decisions, and one raw byte.
Require a single `Literal('A')`, exact entropy counters and payload
consumption, and no write beyond the declared token extent.

Feed the bypass-bearing payload `00 A4 3C 3C 38 00` through the same state
machine. It reconstructs a first-token Match of length 10 and distance 2;
require typed-token validation to reject that reference before accepting or
publishing a token. Independently mutate the entropy interval, event,
decision, raw, and descriptor counts; exercise invalid parameters, token
storage, aggregate-output, short-output, and payload/token-alias gates. Snapshot
all caller token storage and require every pre-write failure to preserve it.

### TVG-0518

Concatenate the published Format 2 one-Literal frame header, descriptor, and
payload from `docs/format.md` into its exact 86-byte frame. Preflight and decode
it with the specified 64-byte stream frame size and require one typed
`Literal('A')`, one raw byte `A`, exact six-byte entropy payload consumption,
and exactly 86 serialized bytes committed. Append a sentinel next-frame byte
and require it to remain outside the consumed extent.

Truncate the payload and mutate its canonical leading zero; prefill token and
raw staging with sentinels and require preflight or entropy rejection with zero
serialized consumption and no changed workspace. Independently provide short
token staging, short raw staging, and deliberately overlapping token/raw,
serialized/token, and serialized/raw object storage. Require all gates to fail
before the first token write and to report the exact required extents after
successful preflight.

### TVG-0519

Build a two-frame stream from the published 112-byte Format 2 header and two
copies of TVG-0518's one-Literal frame. Set stream frame size to one, original
size to two, and frame sequences to zero and one. Feed every serialized byte
and accept every raw byte individually; require two `A` bytes, valid process
results on every call, exact input consumption, terminal `EndOfStream`, and
the same terminal result on repetition.

Corrupt only the second frame's canonical range prefix and require the first
`A` to be published before a sticky malformed-stream error while the second is
never published. Independently exercise serialized, token, and raw workspace
shortage; a valid-limit configuration whose per-frame serialized/token/raw sum
exceeds the aggregate ceiling; stream-level and frame-level local limit
rejection; final-byte truncation; trailing final
data; reset and unknown flags; flush starvation; premature EndInput after frame
one; EndInput retained across a zero-capacity final drain; overlapping
construction workspaces; and output aliasing raw staging. Include a header-only
empty stream and require immediate clean termination.

### TVG-0520

Feed the two one-Literal modeled operations from TVG-0510 to the private
contextual Dynamic Range encoder and require descriptor `(2, 6, 31)` and exact
payload `00 20 7F FF BF 00`. Feed TVG-0516's five bypass-bearing operations
and require six decisions plus exact payload `00 A4 3C 3C 38 00`. Decode both
encoder outputs through the independently implemented request-driven decoder
and require every original operation value and exact final consumption.

Drive context zero past its total-32,768 rescale boundary with 32,770
alternating binary symbols and require the decoder to remain synchronized
through finalization. Independently mutate operation kind, context, alphabet,
symbol, bypass width, and unused fields; constrain table, model-total,
operation-storage, and payload limits; provide short output and overlapping
operation/output storage. Require stable failing operation indices, unchanged
descriptor and output on every pre-write failure, exact writes on success, and
unchanged excess capacity.

### TVG-0521

Parse empty input and require zero typed tokens. Parse `A` and require exactly
`Literal(0x41)`. Compare `AAAAA` with `AAAAAA`: the former remains five
Literals at the strict cost boundary, while the latter becomes `Literal('A')`
then `Match(distance 1, length 5)` with overlap semantics. Parse
`ABCDE1ABCDE2ABCDE3`, compare every typed value against the independently
parsed canonical byte-token transcript, and require the final Match to use the
nearest equal-length distance six.

Encode all byte values followed by a repeated copy and reconstruct the typed
frame to the exact input. Independently exercise the variant-2 parameter cap,
raw block and aggregate workspace limits, one-token-short output, overlapping
raw/token storage, and excess token capacity. Snapshot all caller storage and
require every pre-write failure to preserve it and success to write only the
planned prefix.

### TVG-0522

Compose the one-byte raw input `A` through typed LZSS production, field-context
modeling, and contextual Dynamic Range encoding. Require one token, two events,
two decisions, six payload bytes, 86 serialized bytes, and exact equality with
the published Format 2 one-Literal frame. Independently serialize its 64-byte
header and 16-byte descriptor, compare both regions to that vector, mutate a
count in each value, and require transactional rejection with unchanged output.

Encode `ABCABCABCX` to exercise a Match and decode the resulting complete frame
through the existing independent private decoder; require exact serialized
consumption and raw reconstruction. Exercise invalid stream parameters,
incorrect initial and final frame extents, short token, operation, and
serialized storage, one-byte-short aggregate workspace, raw/output aliasing,
token/output aliasing, and excess serialized capacity. Snapshot serialized or
aliased caller storage and require every pre-write failure to preserve it.

### TVG-0523

Serialize a known-size two-byte stream as two one-byte frames while supplying
and accepting one byte per call. Require the exact documented 112-byte Format
2 stream header followed by the published 86-byte one-Literal frame twice,
with frame sequences zero and one, and require repeated completion to remain
`EndOfStream`.

Independently prove that a full frame is emitted before whole-stream end,
`Flush` does not close a partial frame, `EndInput` survives zero-capacity final
drain, and empty input emits only the header. Exercise short token, operation,
and serialized workspaces, payload limits, premature declared end, constructor
workspace overlap, output aliasing, unknown flags, and `ResetBlock`; require
stable error categories and sticky failures. Serialize the stream header
against its existing vector and require invalid configuration to leave the
destination unchanged.

### TVG-0524

For the default 65,536-byte largest frame, require the profile to publish the
canonical Format 2 stream configuration, 65,536 token slots, 131,072 operation
slots, and the independently calculated `12N + 85` complete serialized-frame
ceiling. Verify native byte counts from `sizeof`, aligned operation placement,
and strongest base alignment. Repeat with a 17-byte final frame and empty input.

Reject a common-valid but Format-2-unsupported match length, a one-byte-short
payload limit, a one-byte-short block limit, and a one-byte-short aggregate
limit without publishing requirements. Derive decoder capacities from reduced
local limits. Partition encoder and decoder byte storage, reject forged,
short, and misaligned records with empty output views, then construct both
streaming directions solely from returned requirements and require exact
multi-frame round trip.

### TVG-0525

From a C11 translation unit, initialize the experimental contextual LZSS
encoder, reduce the raw frame to two bytes, and query all three workspaces.
Encode binary `41 42 41 42 58`, require a Format 2 `MARC` major-version-2
prefix, then initialize the decoder independently from local limits, query its
direction-specific regions, and require exact five-byte reconstruction.

Require the query to publish the expected two-byte-frame conservative
serialized capacities and nonzero opaque-view alignment. Reject primary,
secondary, and views storage one byte short, a deliberately misaligned views
base, overlap between primary and views, a null result handle, a nonzero
reserved field, null configuration output, and an unknown direction. Every
factory failure must leave the transform pointer null. Require the
documentation-layout check to retain the 42 CLI-backed initializers and admit
exactly one additional initializer with the contextual LZSS public name.

### TVG-0526

Configure the public Format 2 C lifecycle with 64-byte raw frames, a
`12F + 5` payload ceiling, a `12F + 85` complete-frame ceiling, and aligned
opaque storage obtained only from its requirements query. Require two
independent encodes to match exactly and round-trip empty input, every one-byte
value, all byte values in order, long repeated bytes, a repeated four-byte
binary pattern, deterministic generated bytes, and lengths 63, 64, and 65.

For 193 generated bytes, compare the one-shot stream with input/output chunk
schedules `(1,1)`, `(7,5)`, and `(13,17)` in both directions. Repeated calls
after success must remain `EndOfStream`. Locate the fourth frame through its
Format 2 header extents, then independently change its sequence, remove its
last payload byte, and append trailing input. Each decoder must report a sticky
malformed-stream error, publish exactly the first 192 raw bytes, and leave the
final caller byte unchanged.

### TVG-0527

Encode canonical binary `41 42 41 42 58` through the public Format 2 C
lifecycle. For every strict prefix, submit the bytes to the public incremental
decoder and, after a valid 112-byte header, to the private complete-frame
decoder. Require malformed status, zero public raw publication, unchanged
private raw staging whenever invoked, and identical status plus byte/bit
position on a repeated public call.

Independently replace frame-header bytes 16 through 47 with `FF` and set the
last byte of the 16-byte contextual Dynamic Range descriptor to one. Both
mutations must fail atomically through both decoder-visible paths. Compile the
bounded harness as an ordinary warning-clean object under MSVC and ClangCL;
do not record an FZ campaign until a sanitizer-backed run is actually made.

### TVG-0528

Select `lzss-contextual-dynamic-range` explicitly through the common CLI
regression. Encode and decode the deterministic repeated binary-text payload
with 65,536-byte frames, then require exact file equality. Repeat with empty
input. Require a second encode to refuse overwriting the existing archive.
Decode an unrelated malformed byte sequence and a valid archive with one
trailing byte; both must fail without retaining either the requested output or
its temporary file. Exercise only the public C lifecycle reached by the CLI.

### TVG-0529

Run one Release iteration of `lzss-contextual-dynamic-range` over the repository
README through the dependency-free benchmark. Require the benchmark to compute
the checked `112 + 12N + 85K` output capacity, obtain three direction-specific
workspace regions from the public requirements query, encode, decode, and
compare the complete bytes before either timed sample. Require successful
ratio, encode/decode throughput, all six workspace extents, and peak-workspace
reporting. Keep the command outside the stable 42-profile documentation count.

### TVG-0530

For interoperability schema 32, preserve the exact schema-31 archive order and
append exactly one `lzss-contextual-dynamic-range` archive as entry 43. Use the
unchanged deterministic 8,193-byte binary fixture, set `schema_version=32` and
`codec_set=marc-cli-v32`, and record full source revision plus every size and
SHA-256. Require generation-time round trip, exact-order verification, foreign
decode equality, and byte-identical re-encoding. Swap the first two manifest
entries and require rejection. Remove only entry 43 to reconstruct schema 31,
then verify the unchanged schema-31-through-schema-1 compatibility chain.

### TVG-0531

Reserve contextual rANS variant 2 with one raw byte `A`. Require typed token
`Literal(0x41)` and the two Symbol decisions `(context 0, value 0)` and
`(context 3, value 65)`. Normalize each used one-symbol context to frequency
4,096 and leave all other context slices zero. Require flattened frequency
entries 0 and 71 to contain little-endian `00 10`, with every other one of the
4,518 entries zero.

Require decision count two, payload size eight, table log 12, context count 31,
and frequency-entry count 4,518 in the fixed 9,052-byte descriptor. Starting
from `L=2^31`, both one-symbol transitions leave the state unchanged, so the
payload is `00 00 00 80 00 00 00 00`. Require a 64-byte frame header with
descriptor size 9,052 and an exact complete-frame extent of 9,124 bytes. This
vector reserves bytes before implementation and changes no public inventory.

### TVG-0532

Materialize TVG-0531's descriptor through the private contextual rANS
serializer and require its exact 16-byte prefix, frequency bytes at offsets
16/17 and 158/159, and zero remainder. Parse it back and require identical
fields and all 4,518 frequencies.

Independently accept zero unused slices and a two-symbol 2,048/2,048 used
slice. Reject a used slice summing to 4,095 without publishing the parse
destination. Reject zero decisions, payload below eight or above
`2 * decisions + 8`, wrong table log, flags, context count, frequency-entry
count, expected counts, block/payload/table/buffer limits, and invalid
serialization without changing a sentinel-filled output descriptor.

Compile after deleting every Dynamic Range compatibility alias for the shared
context count, alphabets, offsets, and frequency-entry extent. Run the existing
Dynamic Range decoder, encoder, and `LzssFieldContext` suites together with the
new descriptor suite under both supported Windows compilers.

### TVG-0533

Build fixed contextual rANS tables from TVG-0531. Require contexts 0 and 3 to
be active, every one of their 4,096 slots to select symbols 0 and 65
respectively with frequency 4,096, and an inactive context's complete region
to remain zero. Replace context 0 with frequencies 2,731 and 1,365 and require
the symbol transition exactly between slots 2,730 and 2,731, with cumulative
start 2,731 for symbol 1.

Fill caller storage and a preexisting published view with sentinels. Require
an invalid 4,095-total slice, a table-entry limit one below 126,976, and an
output span one entry short to preserve all sentinels and the prior view.
Provide three surplus entries on success and require them to remain unchanged,
proving that only the fixed charged extent is written.

### TVG-0534

Decode TVG-0531 through the scalar lifecycle and require Symbol requests
`(context 0, alphabet 2) -> 0` and `(context 3, alphabet 256) -> 65`, two
events, two decisions, state `L`, and exact eight-byte exhaustion. Independently
use one-symbol context 0 value 1 followed by bypass value 2 of width two. The
reverse transitions produce initial state `0x0000000200001000`, serialized as
`00 10 00 00 02 00 00 00`; require the bypass value to emerge LSB first.

Reject invalid descriptor and payload extent, one-entry-short table storage,
initial state zero, wrong context/alphabet, inactive context, zero bypass
width, decision overrun, a selected table entry changed to zero frequency,
and frequency split 1/4,095 with no required renormalization byte. Preserve
the caller value on every failed request. At finish, independently reject
count mismatch, a nonzero unused model, state `L+1`, and one trailing zero
byte; require errors to remain sticky and a later `begin` to reset lifecycle.

### TVG-0535

Pass TVG-0531 through the direct inverse-model bridge. Require one validated
and then materialized literal token `A`, raw size one, two events, two
decisions, exact eight-byte payload consumption, and an untouched surplus
token. Independently activate one-symbol models for match kind 1, length class
0, and distance class 0; require the resulting history-before-start match to
fail typed validation as `invalid_distance` after three entropy events.

Reject initial state zero as an entropy error, descriptor/context decision
disagreement before entropy, and declared raw size two after a valid one-byte
token. Require short table and token spans to preserve token sentinels. Place
the payload legally inside the object representation of the table or token
storage and require overlap rejection before either region changes. Also
reject invalid LZSS parameters, token-buffer limits, and aggregate output
limits using the same fixed one-Literal vector.

### TVG-0536

Wrap TVG-0531 in its exact 64-byte frame header and require the complete 9,124-
byte frame to preflight, decode one typed literal `A`, reconstruct one raw byte
`A`, consume exactly 9,124 bytes, and leave surplus token and raw output
untouched. Append one unrelated byte and require only the first frame extent
to be consumed.

Reject representative truncations before the header, descriptor, and payload
ends without changing a sentinel layout. Reject Dynamic Range entropy identity,
malformed descriptor frequency, insufficient table/token/raw capacities, and
an all-zero invalid initial state. Place serialized input legally inside table
storage and raw output inside token storage, and independently alias serialized
input with raw output; require rejection before any affected workspace changes.
Run all 14 dedicated format/frame tests under MSVC and ClangCL, then run both
complete Release suites.

### TVG-0537

Encode TVG-0531's two Symbol operations and require decision count two,
payload size eight, normalized frequencies 4,096 at flattened entries 0 and
71 only, and exact payload `00 00 00 80 00 00 00 00`. Decode the result through
the scalar contextual rANS lifecycle and require both original values, exact
counts, terminal state, and payload exhaustion.

Independently encode Symbol `(context 0, value 1)`, Symbol `(context 20,
value 2)`, and bypass value two of width two. Require the decoder to recover
the bypass value LSB first. Exercise a used two-symbol context and require its
normalized sum 4,096 with deterministic numeric tie handling and successful
renormalization round trip. Reject each malformed operation field at its stable
index, empty input, decision/table/payload/buffer limits, short output, and
operation/payload aliasing while preserving descriptor and payload sentinels.

### TVG-0538

Pass one typed Literal `A` through the direct bridge and require the same two
events, two decisions, descriptor frequencies, eight-byte payload, and decoded
token as TVG-0531 and TVG-0537, without supplying operation staging. Compare
the direct result byte-for-byte with the existing operation encoder after the
reference context model materializes the same tokens.

Use a mixed frame containing Literals and overlapping Matches so reverse
context derivation crosses previous-Literal and previous-Match states. Require
direct and operation paths to produce identical descriptors and payloads, then
decode the direct payload back to identical tokens and raw extent. Reject an
invalid typed frame, short payload, token/payload overlap, token-buffer and
entropy limits, and preserve descriptor/output sentinels on every prewrite
failure. Re-run all operation encoder tests to prove shared-core refactoring
does not change their bytes.

### TVG-0539

Encode raw byte `A` as one complete contextual rANS frame and require planning
and exact output size 9,124, one token, two events, two decisions, payload size
eight, the documented 64-byte header, descriptor frequencies at flattened
entries 0 and 71 only, and the lower-bound payload. Decode that frame through
the complete-frame decoder and require raw `A` with exact consumption. Leave
one surplus serialized and token element untouched.

Encode a mixed repetitive raw frame and require planning/encoding agreement,
then complete-frame decode equality. Reject invalid stream identity, wrong raw
frame size, short token and serialized spans, aggregate workspace limit, and
all raw/token/serialized overlaps before affected writes. Preserve serialized
sentinels on every prewrite failure and require repeated encoding with separate
storage to produce byte-identical complete frames.

### TVG-0540

Stream two raw `A` bytes as two one-byte frames through one-byte input and
output spans. Require the exact documented 112-byte contextual-rANS stream
header followed by two 9,124-byte one-Literal frames with sequences zero and
one. Compare the complete output with independently assembled format vectors,
then require repeated calls after final drain to remain `EndOfStream`.

Require a full frame to prepare before `EndInput`, while `Flush` leaves a
partial frame open. Retain `EndInput` across zero-capacity frame drain and emit
only the stream header for empty input. Reject short token or serialized
staging, aggregate limits, premature finish, excess input, construction and
caller-output aliases, `ResetBlock`, and unknown flags. Require every terminal
failure to remain sticky and every reported process result to satisfy the core
consumption/production invariant.

### TVG-0541

Feed the TVG-0540 two-frame stream to the streaming decoder one byte at a time
and drain raw output one byte at a time. Require exact `AA`, complete input
consumption, valid process results on every call, and stable `EndOfStream`.
Corrupt only the second frame descriptor and require the first raw `A` to be
committed while the second frame publishes nothing.

Reject short serialized, fixed-table, token, and raw staging independently;
reject aggregate live-workspace excess after a valid frame header; reject
stream and frame limit excess. Cover truncated header/body, trailing bytes,
empty streams, nonterminal `Flush`, premature final input, construction-region
overlap, caller-output/raw overlap, `ResetBlock`, and unknown flags. Retain
`EndInput` while a decoded final frame waits for output capacity, and require
all terminal errors to remain sticky.

### TVG-0542

For the default 65,536-byte frame, require the encoder calculator to publish
65,536 raw bytes and tokens, payload ceiling 786,440, and complete-frame
ceiling 795,556. For a short 17-byte stream require actual largest-frame
requirements, and for empty input require zero encoder staging with alignment
one. Reject unsupported dictionary parameters, uint32/checked-arithmetic
bounds, configured payload/table/block limits, and aggregate excess without
publishing partial requirements.

For decoder limits, require the serialized ceiling derived from the lesser of
the configured payload bound and `12N + 8`, fixed 126,976 table entries, the
maximum token/raw extent, an aligned token offset after native table bytes, and
the exact total opaque views extent. Partition aligned storage successfully;
reject forged offsets/counts/sizes/alignment, short storage, and a deliberately
misaligned base while leaving all views empty. Use only returned requirements
and views to construct both streaming directions and round-trip a multi-frame
mixed raw sequence.

### TVG-0543

Compile a C11 client against `marc/marc.h`. Initialize encode configuration and
require ABI version 1, a 65,536-byte default frame, the typed-LZSS defaults, and
the default hard limits. For a five-byte `ABABX` stream with two-byte frames,
query all three workspaces, create the encoder exclusively from those extents,
and require the Format 2 prefix with dictionary variant 2, rANS entropy variant
2, and context variant 1. Reinitialize for decode, query its larger fixed-table
views, create the decoder, and require exact round trip.

Reject short primary, secondary, and views prefixes independently; overlapping
regions; misaligned views; null transform output; nonzero reserved fields;
wrong struct size or ABI version; unknown direction; null configuration,
requirements, and initializer output. Require every failed factory call to
leave the handle null. Build and run the same C test against the selected
static/shared library target on both supported Windows compiler configurations.

### TVG-0544

Configure the contextual-rANS public C lifecycle with 64-byte raw frames, a
384-decision block limit, `12F + 8 = 776` payload ceiling, and caller-owned
storage obtained only from its requirements query. Require two independent
encodes to match exactly and round-trip empty input, every one-byte value, all
byte values in order, long repeated bytes, a repeated four-byte binary pattern,
deterministically generated bytes, and lengths 63, 64, and 65.

For 193 generated bytes, compare one-shot output with input/output chunk
schedules `(1,1)`, `(7,5)`, and `(13,17)` in both directions. Repeated calls
after success must remain `EndOfStream`. Locate the fourth frame through its
public Format 2 extents, then independently change its sequence, remove its
last payload byte, and append trailing input. Each decoder must report a sticky
malformed-stream error, publish exactly the first 192 raw bytes, and preserve
the final output sentinel.

### TVG-0545

Encode canonical binary `41 42 41 42 58` through the public contextual-rANS C
lifecycle. For every strict prefix, submit the bytes to the public incremental
decoder and, after a valid 112-byte header, to the private complete-frame
decoder. Require malformed status, zero public raw publication, unchanged
private raw staging whenever invoked, and identical status plus byte/bit
position on a repeated public call.

Independently replace frame-header bytes 16 through 47 with `FF` and set the
contextual-rANS descriptor flags byte at descriptor offset 9 to one. Both
mutations must fail atomically through both decoder-visible paths. Compile the
bounded harness as an ordinary warning-clean object under MSVC and ClangCL;
do not record an FZ campaign until a sanitizer-backed run is separately made.

### TVG-0546

Select `lzss-contextual-rans` explicitly through the common CLI regression.
Encode and decode the deterministic repeated binary-text payload with
65,536-byte frames, then require exact file equality. Repeat with empty input.
Require a second encode to refuse overwriting the existing archive. Decode an
unrelated malformed byte sequence and a valid archive with one trailing byte;
both must fail without retaining either the requested output or its temporary
file. Exercise only the public C lifecycle reached by the CLI.

### TVG-0547

Run one Release iteration of `lzss-contextual-rans` over the repository README
through the dependency-free benchmark. Require the benchmark to compute the
checked `112 + 12N + 9,124K` output capacity, obtain three direction-specific
workspace regions from the public requirements query, encode, decode, and
compare the complete bytes before either timed sample. Require successful
ratio, encode/decode throughput, all six workspace extents, and peak-workspace
reporting. Keep the command outside the stable 42-profile documentation count.

### TVG-0548

Encode the repository README and format specification before and after removing
the contextual-rANS streaming encoder's redundant outer frame plan. Require
the complete archives to remain byte-for-byte identical. Run the focused
streaming-encoder, frame-encoder, public-completion, CLI, and experimental
benchmark cases under MSVC and ClangCL. Compare one Release benchmark iteration
for the format specification and report the measured encode time without
claiming stable throughput; decoding and every serialized byte must remain
unchanged.

### TVG-0549

For one raw byte `A`, reuse variant 2's two decisions, two one-symbol context
models, and eight-byte rANS payload. Set active-context mask bits 0 and 3. Emit
context 0 as dense bytes `00 00 10` because its dense and sparse records tie at
three bytes, and emit context 3 as sparse bytes `01 00 41`, producing the
26-byte compact descriptor:

```text
02 00 00 00 08 00 00 00 0C 00 1F 00 A6 11 00 00
09 00 00 00 00 00 10 01 00 41
```

Require exact parse/serialize round trip and full frequency reconstruction.
Independently truncate every byte, set mask bit 31, clear each required mask
bit, use unknown mode 2, duplicate or reverse sparse symbols, encode zero or
oversummed explicit frequencies, choose the noncanonical dense representation,
and append one trailing byte. Every failure must preserve the destination
descriptor. Add threshold vectors on both sides of `3K < 1 + 2(A-1)` and the
exact 9,025-byte all-dense maximum.

### TVG-0550

Encode the repository README and format specification through
`lzss-contextual-dynamic-range` before and after removing the redundant outer
frame plan, token-count pass, and operation-count pass. Require SHA-256 and
complete byte equality for both archives. Run the focused typed-context frame,
streaming encoder, profile, completion, C API, CLI, and experimental benchmark
tests under MSVC and ClangCL. Compare one descriptive MSVC Release iteration
over the format specification; serialized size, decoding, and workspace
reporting must remain unchanged.

### TVG-0551

Implement TVG-0549 as an executable compact-descriptor suite. In addition to
the exact one-Literal bytes and every strict prefix, create context-20
alphabet-eight threshold models with four and five nonzero symbols to prove
strict sparse selection and dense tie selection. Construct every context with
all symbols present to require the exact 9,025-byte maximum. Require failed
parse and serialize calls to preserve sentinels and enforce decision, payload,
table-entry, exact-buffer, and output-capacity limits independently.

### TVG-0552

Begin the scalar decoder with TVG-0549's exact 26-byte compact descriptor and
variant-2's eight-byte payload, decode contexts 0 and 3 as values 0 and 65,
and require the same terminal state and counts. Build the same descriptor
through the operation encoder, serialize it compactly, and round-trip a Symbol
plus two LSB-first bypass bits. Compare every fixed decode-table entry produced
by ordinary and compact begin paths.

Independently corrupt canonical mode, truncate the descriptor, mismatch the
payload span, invalidate the initial state, and shorten table storage. Require
the compact error only for descriptor parsing, ordinary decoder errors for the
remaining gates, unchanged table sentinels before construction, unchanged
decoded values on sticky failure, and successful reuse after both failure and
completed decoding.

### TVG-0553

Pass TVG-0549's exact 26-byte descriptor and eight-byte payload through the
private LZSS field-context compact decoder with declared counts
`token/event/decision/raw = 1/2/2/1`. Require one literal `A`, zero distance
and length, two entropy events and decisions, complete payload consumption,
and equality with variant 2's typed-token result.

Independently replace the final compact record with its noncanonical dense
form, supply zero token capacity, alias payload and token storage, and shorten
the fixed table workspace. Require compact format errors to remain distinct
from token/state/workspace errors, every prewrite failure to preserve token
sentinels, and validation-only decoding to publish no token.

### TVG-0554

Prefix TVG-0549's exact 26-byte compact descriptor and eight-byte payload with
the common 64-byte Format 2 frame header for sequence zero, raw/token/event/
decision counts `1/1/2/2`, payload size eight, and descriptor size 26. Require
the private compact complete-frame decoder to consume exactly 98 bytes and
publish raw byte `A`, with the same typed literal and accounting as TVG-0553.

Independently truncate the descriptor or payload, replace its final sparse
record with a noncanonical dense record, shorten each caller workspace, and
alias serialized input with writable storage. Require precise preflight or
workspace errors, zero serialized consumption, and unchanged token/raw
sentinels for every failure before publication.

### TVG-0555

Copy the specified 112-byte contextual-rANS variant-2 stream header and change
only entropy variant at offset 18 from little-endian 2 to 3. Require compact
parse and serialize to reproduce those exact bytes and retain frame size 64,
original size one, table log 12, context count 31, and 4,518 frequency entries.

Require the variant-2 parser to reject this header and the compact parser to
reject the variant-2 header. For every strict prefix, invalid parameters, and
insufficient limits, preserve the caller header, consumed count, and serialized
output sentinels.

### TVG-0556

Concatenate TVG-0555's compact stream header with two copies of TVG-0554's
98-byte Literal `A` frame, using sequences zero and one, frame size one, and
original size two. Feed every serialized and raw byte through one-byte input
and output spans and require `AA`, exact input consumption, and stable repeated
EndOfStream.

Independently corrupt the second compact descriptor, truncate the final frame,
append a trailing byte, provide each workspace one element short, exceed the
aggregate-buffer limit, request reset or an unknown flag, alias workspaces or
public output, and end with zero output capacity. Require only the first frame
to publish before later corruption and require every terminal error to remain
sticky.

### TVG-0557

Plan and encode raw byte `A` through typed LZSS and contextual rANS, serialize
the resulting model with variant 3, and require descriptor size 26, payload
size eight, complete frame size 98, and exact equality with TVG-0554. Decode
that frame through the compact complete-frame decoder and require `A`.

Repeat a mixed literal/match frame twice and require identical descriptor,
payload, and complete frame bytes. Independently shorten token staging and
serialized output, constrain aggregate workspace, invalidate stream
parameters, mismatch raw extent, and alias raw/token/output storage. Require
precise stable errors and unchanged serialized sentinels before any write.

### TVG-0558

Encode two one-byte `A` frames through the compact streaming encoder using
one-byte input and output spans. Require exact equality with the canonical
112-byte variant-3 stream header followed by two TVG-0554 98-byte frames whose
sequence fields are zero and one. Decode the result through the compact
streaming decoder and require the original two bytes.

Independently retain EndInput across a zero-capacity final-frame drain, finish
empty input after draining only the compact stream header, Flush a partial raw
frame without closing it, and reject short token/frame workspaces, aggregate
limits, premature or excess input, overlapping construction/output storage,
unknown flags, and ResetBlock. Require stable terminal errors and validate
every ProcessResult count/status combination.

### TVG-0559

Query the compact profile for default 65,536-byte frames and require maximum
serialized-frame storage 795,529 bytes. Query a 17-byte stream and require
9,301 bytes; query empty input and require zero frame/token/view storage with
alignment one. Under decoder limits of 1,024 raw bytes and 2,000 payload bytes,
require 11,089 serialized bytes while retaining the fixed contextual-rANS
decode-table and typed-token view layout.

Partition the returned views, construct the distinct compact streaming encoder
and decoder, and round-trip a multi-frame mixed literal/match input. Require
entropy variant 3 in the encoded stream. Independently reject unsupported
dictionary parameters, payload, block, aggregate, and table limits one unit
past their admitted boundaries without publishing partial requirements.

### TVG-0560

From a pure C11 translation unit, initialize the compact contextual-rANS
configuration in each direction. With five raw bytes, two-byte frames, and the
small public test limits, require encoder workspaces `(2, 9,121, typed views)`
and decoder workspaces `(9,121, 2, table-plus-token views)`. Construct only from
those returned extents, encode through the compact public factory, require
Format 2 entropy algorithm/variant `4/3`, and decode the complete multi-frame
stream back to the original bytes.

Shorten each workspace independently, overlap used prefixes, misalign views,
pass null output/config/requirements pointers, alter reserved fields,
structure size, ABI version, and direction, and call initialization with an
invalid direction or null destination. Require stable public statuses, a null
transform on every creation failure, and no change to the fixed contextual-rANS
C lifecycle or ABI version.

### TVG-0561

Apply one public completion matrix independently to contextual-rANS fixed
variant 2 and compact variant 3. For each lifecycle, encode empty input, all
256 one-byte inputs, all byte values in order, long zero and mixed patterns,
deterministic generated bytes, and lengths 63, 64, and 65 with a 64-byte frame.
Require byte-identical repeated encoding, the selected entropy variant in the
Format 2 header, exact round trip, and identical bytes under `(1,1)`, `(7,5)`,
and `(13,17)` input/output chunk schedules.

Encode four frames, then independently corrupt the fourth frame sequence,
truncate its final byte, and append one strict trailing byte. Require the
decoder to publish exactly the first 192 raw bytes, leave the final output
sentinel unchanged, and repeat the same stable terminal error without further
consumption or production. Repeated calls after successful finish must return
stable EndOfStream.

### TVG-0562

Compile the contextual-rANS fixed-memory decoder harness once for fixed
variant 2 and once for compact variant 3. Bound arbitrary input to 32,768
bytes, raw publication to 4,096 bytes, one decoded frame to 1,024 bytes,
decisions to 6,144, payload to 12,296 bytes, and process calls to the sum of
the bounded input/output extents plus 32. Before construction, require every
public workspace requirement to fit the aligned static arrays.

For both representations, generate one canonical five-byte stream and feed
every strict prefix to the private complete-frame and public streaming paths.
Independently saturate all frame extent fields and set descriptor flags
nonzero. Require no private raw publication, no public output publication, and
a stable public malformed-stream result. Build the compact target with
ASan/UBSan/libFuzzer and run exactly 1,000 bounded cases with maximum input
8,192, five-second per-case timeout, and 512 MiB RSS limit.

### TVG-0563

Run the generic transactional CLI regression with
`--codec lzss-contextual-rans-compact`. Encode a nonempty repeated
mixed-symbol fixture, require entropy algorithm/variant `4/3`, decode it with
the same selector, and compare exact bytes. Repeat for empty input.

Independently attempt to overwrite an existing archive, decode malformed
input, and append one strict trailing byte to an otherwise valid compact
stream. Require nonzero command status and no destination or temporary-file
publication after each failure. Keep the fixed `lzss-contextual-rans`
selector bound to entropy variant 2.

### TVG-0564

Run `marc_benchmark lzss-contextual-rans-compact README.md 1`. Require checked
capacity `112 + 12N + 9,097K`, public compact construction in each direction,
an exact pre-timing round trip, and a successful timed encode and decode.
Require the report to identify `lzss-contextual-rans-compact`, the complete
encoded byte count and ratio, both throughputs, peak caller-owned workspace,
and encoder/decoder primary, secondary, and views extents.

Run the fixed `lzss-contextual-rans` smoke independently and retain its
`112 + 12N + 9,124K` capacity and public symbol family. Treat measured speed
and ratio as descriptive output, never as a pass threshold.

### TVG-0565

Generate interoperability schema 33 from the existing 8,193-byte binary
fixture. Preserve all 43 schema-32 archives byte-for-byte and in exact order,
then append `lzss-contextual-rans-compact.marc` as entry 44. Require manifest
identity `33` / `marc-cli-v33`, exact file sizes and SHA-256 values, successful
local decode equality, and byte-identical local re-encoding for every entry.

Swap the first two schema-33 manifest records without changing files and
require order rejection. Derive schema 32 by removing only archive 44 and
changing the manifest identity to `32` / `marc-cli-v32`; verify it, then derive
and verify each existing schema down through schema 1. Keep fixed
`lzss-contextual-rans` absent from every interoperability inventory.

### TVG-0566

Reserve contextual tANS variant 2 with one raw byte `A`. Require typed token
`Literal(0x41)` and Symbol decisions `(context 0, value 0)` and
`(context 3, value 65)`. Normalize each used one-symbol context to 4,096 and
leave every other context inactive. Require active mask `09 00 00 00`, the
three-byte dense context-0 record `00 00 10`, and the three-byte sparse
context-3 record `01 00 41` after the 24-byte prefix.

Require decision count two, payload size two, table log 12, final valid bits
zero, context count 31, frequency-entry count 4,518, and exact descriptor size
30. Starting from `L=4096`, both one-symbol transitions map `L` to itself and
emit no additional bit, so payload is `00 00`. Require the 64-byte frame header
to declare descriptor size 30 and exact complete-frame extent 96 bytes. The
112-byte stream header selects dictionary/context `2/2` and `1/1` plus entropy
`5/2`, making the complete one-byte stream 208 bytes. This vector reserves
bytes before implementation and changes no public inventory.

### TVG-0567

Require the contextual tANS parser and serializer to reproduce TVG-0566's
exact 30 bytes and reconstruct the identical descriptor. Exercise the strict
sparse-size inequality and dense tie rule, all-dense 9,029-byte maximum, every
strict prefix, trailing data, active-mask bit 31 and empty mask, unknown mode,
noncanonical alternative, duplicate or descending sparse symbols, zero stored
frequency, inferred-frequency exhaustion, and nonzero prefix reserved bytes.

Require field, expected-count, payload-bound, final-valid-bit, local block,
compressed-payload, 131,072-entry entropy-table, aggregate-buffer, and output-
capacity failures to preserve parsed state, serialized output, reported size,
and written extent. Run the existing compact contextual-rANS format suite
beside it and require all exact vectors and malformed categories to remain
unchanged after extracting the shared canonical record primitive.

### TVG-0568

Build contextual tANS decode tables for TVG-0566. Require exactly 131,072
caller-owned entries, active Symbol contexts 0 and 3, zero-filled inactive
regions, and the implicit bypass table at region 31. For each one-symbol table,
require symbol identity, zero additional bits, and a one-to-one next-state
permutation over `[4096,8192)`.

Replace context 0 with normalized frequencies 2,731/1,365 and require its
4,096 entries to equal the standalone tANS builder byte-for-byte. Require the
bypass region to equal a standalone 2,048/2,048 binary table. Exercise an
invalid model, a 131,071-entry output, a 131,071-entry decoder limit, and
oversized caller output; every prewrite failure preserves storage and view,
and success leaves bytes beyond entry 131,071 untouched.

### TVG-0569

Decode TVG-0566 through the contextual state API as Symbol requests `(0,2)`
and `(3,256)`, require values zero and 65, no consumed additional bits, exact
counts two/two, terminal state 4,096, and successful finish. Independently
construct the inverse transitions for Symbol value one followed by a two-bit
bypass value two; require forward LSB-first recovery and counts two/three.

Reject descriptor and payload-size mismatch, invalid initial state, nonzero
padding, short table storage, table-limit excess, invalid context/alphabet,
inactive context, bypass widths zero and over budget, mutated transition
extent, truncated bits, count mismatch, unused active context, nonterminal
state, trailing bits, calls before begin, and calls after finish. Require
sticky errors and preserve requested values when the current event fails.

### TVG-0570

Feed TVG-0566 to the contextual-tANS token bridge with declared counts
`tokens/events/decisions/raw = 1/2/2/1`. Require validation and atomic decode to
produce exactly `Literal(0x41)`, raw extent one, and zero additional bits while
leaving token storage beyond the declared prefix unchanged.

Construct a zero-bit one-Match stream with kind one, length class zero, and
distance class zero; require typed validation to reject distance one at raw
offset zero after three successful entropy decisions. Reject invalid entropy,
decision and raw-count mismatch, invalid LZSS parameters, short table/token
storage, payload/table, payload/token, and table/token overlap, token-buffer
limit, frame/block limit, and aggregate-output excess. Every prewrite failure
must preserve the complete token destination.

### TVG-0571

Serialize and parse the contextual-tANS 112-byte stream header with entropy
identity `5/2`, and reject identity `4/2`. Preflight TVG-0566's exact 96-byte
frame, requiring descriptor size 30, payload size two, parsed frequencies for
contexts 0 and 3, and exact frame-header reserialization.

Decode that frame through caller-owned table, token, and raw regions; require
one consumed frame, `Literal(0x41)`, and raw byte `0x41`, while preserving all
storage beyond declared extents. Append a byte and require it to remain
unconsumed. Reject an active-mask record mismatch as trailing descriptor data,
every strict frame truncation tested, descriptor extent below 27, the
131,072-entry limit gate, short table/token/raw regions, serialized/raw
overlap, and nonterminal entropy payload without publishing raw output.

### TVG-0572

Build a contextual-tANS Format 2 stream from the exact `5/2` 112-byte header
and one or two copies of TVG-0566's 96-byte Literal frame, with consecutive
sequence numbers and one raw byte per frame. Feed every serialized byte and
every raw output byte separately; require deterministic `AA`, valid progress
statuses, exact input consumption, and stable repeated `EndOfStream`.

Corrupt only the second frame and require the first raw byte to commit before a
sticky malformed-stream error. Test empty input, output draining after
`EndInput`, every workspace shortage, aggregate-buffer limit, premature end,
trailing data, `ResetBlock`, unknown flags, caller-output aliasing, and
construction-time workspace aliasing. Every returned `ProcessResult` must
satisfy the core count/status invariants.

### TVG-0573

Encode the two documented contextual-tANS operations `Symbol(0,2,0)` and
`Symbol(3,256,65)`. Require two decisions, normalized frequencies 4,096 at
entries 0 and 71, a two-byte zero payload, and exact decode back to both
symbols and terminal state 4,096.

Encode `Symbol(0,2,1)` followed by two bypass bits with value two. Require the
same descriptor and payload as the independently constructed decoder vector,
then decode the symbol and LSB-first bypass value. Round-trip a 512-decision
alternating context, verify deterministic normalization ties, and reject every
invalid operation field at its stable index. Short table/payload storage,
operation/payload and table/payload overlap, empty operations, local limits,
and all prewrite failures must preserve descriptor and payload. An invalid
descriptor or short inverse-table destination must preserve the entire table
destination.

### TVG-0574

Directly encode one typed `Literal(0x41)` without materializing operations.
Require one token, two events/decisions, descriptor frequencies 4,096 at
entries 0 and 71, and exact payload `00 00`.

For `Literal(A), Literal(B), Match(distance=2,length=6),
Match(distance=1,length=5), Literal(C)`, independently materialize the field
operations and encode them through TVG-0573. Require the direct token plan,
descriptor, payload, event count, and decision count to match exactly, then
decode the direct payload back to all five tokens. Reject invalid token frames,
short encode-table/payload regions, token/table, token/payload, and
table/payload overlap, aggregate limits, and every prewrite failure without
publishing descriptor or payload.

### TVG-0575

Encode raw byte `A` through typed LZSS and contextual tANS into the exact
96-byte frame already specified by TVG-0566. Require one token, two events and
decisions, a 30-byte descriptor, payload `00 00`, untouched destination suffix,
and successful decoding through the existing complete-frame decoder.

Encode a mixed repetitive raw frame twice with separate token and inverse-
table storage; require byte-identical frames and exact raw recovery. Reject an
invalid stream, wrong input extent, short token/table/serialized storage,
raw-token, raw-table, token-table, serialized-raw, serialized-token, and
serialized-table overlap, plus an aggregate four-region workspace excess.
Every failure before admitted encoding must preserve serialized output.

### TVG-0576

Encode two raw `A` bytes as two one-byte contextual-tANS frames while limiting
both input and output to one byte per call. Require the exact `5/2` stream
header followed by TVG-0575's 96-byte frame at sequences zero and one, valid
process counts/statuses, retained final `EndInput`, and stable repeated
`EndOfStream`.

Require a full frame to emit before whole-stream end while `Flush` leaves a
partial raw frame open. Cover empty-stream header drain; short token, inverse-
table, and serialized-frame workspaces; aggregate limits; premature and excess
input; unknown flags; `ResetBlock`; every constructible private-workspace
overlap; caller-output overlap with each private region; and sticky terminal
errors without exposing mutable frame bytes.

### TVG-0577

Build the default private contextual-tANS profile for 2,500,000 raw bytes and
a 65,536-byte frame. Require the canonical `5/2` stream fields, a 65,536-byte
raw region, 65,536 tokens, 131,072 inverse entries, the conservative
`64 + 9,029 + 2 + ceil(65,536 * 6 * 12 / 8)` serialized-frame extent, checked
typed offsets, and the exact four-region aggregate gate. Require zero frame
and view extents for an empty stream.

Derive decoder requirements only from local limits and require the fixed
131,072-entry table, bounded serialized/raw extents, aligned token offset, and
aggregate gate. Partition both directions from aligned byte storage; reject
forged counts, offsets, sizes, alignments, short storage, and misalignment
without publishing views. Construct the private streaming encoder and decoder
solely from returned requirements and round-trip a multi-frame input.

### TVG-0578

Initialize the public contextual-tANS C configuration for both immutable
directions. With raw bytes `41 42 41 42 58`, a two-byte frame, and small hard
limits, query all three workspaces, construct the encoder, require Format 2
identity `dictionary=2/0, entropy=5/2`, then construct the decoder from its own
requirements and recover the exact input.

Reject one-byte-short primary, secondary, or views regions; misaligned views;
overlap between a required prefix of any two regions; null output handles;
nonzero reserved fields; wrong structure size or ABI version; invalid
direction; and null query/init arguments. Every failed creation must leave the
handle null. The C test must link only through the public installed header and
library target.

### TVG-0579

Through only the public contextual-tANS ABI, round-trip empty input, every
one-byte value, bytes `00..ff`, 257 zeros, a 259-byte `00 ff 55 aa` pattern,
513 bytes from the documented LCG seed `c001d00d`, and seeded inputs of lengths
63, 64, and 65. Encode each input twice. Require entropy identity `5/2` and
byte-identical output.

Generate 193 bytes with seed `6d617263`; require one-shot, 1/1, 7/5, and 13/17
input/output chunk schedules to emit identical four-frame bytes and recover
identical raw bytes. Generate a second 193-byte stream with seed `13579bdf`.
Independently flip the final frame sequence, remove the final serialized byte,
and append one trailing zero. Require exactly the first 192 raw bytes to remain
committed, preserve the final output sentinel, and repeat the same terminal
error and position without progress.

### TVG-0580

Generate the canonical single-frame `ABABX` stream through the public
contextual-tANS encoder with a five-byte frame. Submit every strict prefix to
fresh private complete-frame and public streaming decoders and require no raw
publication. Independently overwrite frame-header bytes 16 through 47 with
`ff`, then set byte 9 of the contextual-tANS descriptor to one. Require both
decoder boundaries to preserve `a5` sentinels, and require the public decoder
to repeat the same terminal error category and byte/bit position without
progress.

Compile one bounded fuzz entry that gives each at-most-32-KiB input to the
private complete-frame decoder after exact header admission and always drives
the public decoder with byte-derived chunks. Fix a 4-KiB output ceiling,
1-KiB frame ceiling, 6,144-decision ceiling, 9,218-byte payload ceiling,
9,029-byte descriptor ceiling, 131,072 transition entries, 1,024 tokens, one
aggregate workspace bound, and a finite call budget.

### TVG-0581

Build only `marc_fuzz_lzss_contextual_tans_stream` in the existing Clang 22
GNU-driver sanitizer tree. Query the same compiler's resource directory and
prepend its `lib/windows` directory only for execution. Run with no corpus and
arguments `-runs=1000 -max_len=32768 -timeout=5 -rss_limit_mb=512`; direct
failure artifacts to the ignored contextual-tANS build-artifact directory.

Require normal exit after exactly 1,000 inputs. Record coverage output, peak
RSS, and whether libFuzzer, AddressSanitizer, or UndefinedBehaviorSanitizer
reports a crash, hang, or finding. Do not add generated inputs or an empty
artifact directory to the repository.

### TVG-0582

Register one Release smoke that invokes `marc_benchmark
lzss-contextual-tans README.md 1`. Construct both transforms through the
public contextual-tANS lifecycle, query each workspace independently, reserve
output using `112 + 9N + 9,095K`, and require a byte-exact pre-timing round
trip.

Run the same 4,326-byte repository README once through `lzss-tans`,
`lzss-contextual-dynamic-range`, `lzss-contextual-rans-compact`, and
`lzss-contextual-tans`. Record encoded extent, ratio, directional workspace,
peak workspace, and contextual-tANS timing without turning any measured value
into a test threshold.

### TVG-0583

Invoke the Release CLI with `--codec lzss-contextual-tans` over the existing
repository-owned binary round-trip fixture. Require the encoded stream header
to identify dictionary algorithm/variant `2/0` and entropy algorithm/variant
`5/2`, then decode with the same selector and compare every output byte.

Append one byte to the canonical archive and require strict decode failure.
Verify the destination remains uncommitted through the established temporary-
file workflow. Keep the stable 42-row profile table unchanged while requiring
the experimental selector to appear in CLI documentation and usage text.

### TVG-0584

Generate schema 34 from the established 8,193-byte fixture and existing 44
archives, then append `lzss-contextual-tans.marc` as entry 45. Require manifest
identity `34`/`marc-cli-v34`, exact profile order, leaf-only names, recorded
sizes and SHA-256 values, exact fixture decode, and byte-identical local
re-encoding for every archive.

Swap the first two manifest entries and require order rejection. Copy the
canonical bundle, remove only contextual tANS, change the identity to
`33`/`marc-cli-v33`, and require successful verification before continuing the
unchanged derivation chain through schema 1.

### TVG-0585

At full revision `4929252144e4bfe44fb3ec076f548aa47e4ff111`, verify the
Windows/MSVC and Ubuntu 24.04/Ninja schema-34 CI bundles with the Ubuntu 26.04
Clang 21.1.8 CLI. Generate a schema-34 Ubuntu 26.04 bundle from the same
revision and verify it first with that CLI and then with Windows/MSVC.

Require all four final lines to report 45 archives, the exact producer label,
and the same full revision. Retain the manifest-order, size, SHA-256, fixture-
decode, and byte-identical re-encoding checks performed by the verifier; do
not import the external bundles into the repository.

### TVG-0586

Feed a hand-checkable typed-field sequence containing three literals, one
match, two distinct token symbols, two distinct literal symbols in one
context, and three bypass bits to the provisional contextual-Huffman cost
estimator. Require exact pooled/contextual/shared descriptor sizes of
31/41/68 bytes, symbol-bit counts of 9/4/4, and complete stored extents of
33/42/69 bytes. Require a single-symbol model to consume zero payload bits,
empty input to charge only each strategy's fixed metadata, and malformed
context/alphabet pairs to fail at the exact operation index.

Build the repository-owned estimator and run it over the current 4,326-byte
`README.md`. Require the tool to derive typed LZSS and field operations itself
and print, without threshold assertions, raw size, token and operation counts,
canonical serialized-LZSS extent, and the descriptor/symbol/bypass/payload/
total breakdown for all three candidates. This is descriptive design evidence,
not a compression benchmark or format vector.

### TVG-0587

Construct 200 literal-symbol operations: context 3 emits `A` 100 times, while
context 4 alternates `A` and `B` 100 times. The pooled literal table must cost
16 descriptor bytes plus 200 symbol bits, or 41 stored bytes. Context 3's
single-symbol override saves 100 bits and costs four descriptor bytes, so it
must be selected; context 4's table must remain pooled. Require one selected
context, two active/stored models, 20 descriptor bytes, 100 symbol bits, and
33 stored bytes. Retain the DD-707 small vector as a no-selection case.
Repeat with 32 `A` symbols in context 3 and 32 alternating symbols in context
4; the override's 32-bit saving exactly equals its four-byte record, so the
pooled table and 24-byte total must be retained.

Run the updated estimator over both repository-owned inputs without thresholds.
The 4,326-byte README must report no profitable override. The 312,817-byte
format specification must report nine overrides and print the full pooled,
selective, contextual, and shared-contextual breakdown. These sizes are
descriptive and do not become format vectors or stable pass criteria.

### TVG-0588

Construct the normative `2/2` descriptor for one raw Literal `A`: decision
count two, payload size zero, override mask zero, final-valid-bits zero,
maximum length 15, field mask `0x03`, and zero flags. Append Single records for
token-kind symbol 0 and literal symbol 65. Require exact serialization to the
documented 24 bytes, exact parsing, one untouched output sentinel, and a
complete 88-byte frame/200-byte stream derivation.

Independently require alphabet-2 lengths `(1,1)` to choose a five-byte dense
record and alphabet-256 symbols `A/B` with lengths `(1,1)` to choose an
eight-byte sparse record. Reject the equivalent dense literal record as
noncanonical, an oversubscribed three-symbol length-1 table, truncated extent,
field mask `0x07`, override bit 31, a caller maximum code length below 15, and
short serializer output without publishing descriptor state or byte count.

### TVG-0589

Decode the documented one-Literal descriptor with an empty payload: context 0
must return kind 0, context 3 must return literal 65, neither consumes a bit,
and completion requires two events and two decisions with zero table entries.

Construct a mixed descriptor with pooled kind lengths `(1,1)` and Single
literal `A`, length class 1, and distance class 1. Feed operations kind 0,
literal `A`, kind 1, length 1, one bypass bit 1, distance 1, and one bypass bit
0. The physical bit sequence is `0,1,1,0`, byte `06`, with four final valid
bits; completion requires seven events and seven decisions.

Independently reject insufficient table workspace, nonzero high padding,
truncated canonical input, an invalid context or alphabet, an invalid bypass
width, decision-budget overrun, trailing valid bits, an unused serialized
override, calls before begin, and repeated completion. Preserve every caller
value sentinel on a failed Symbol or bypass request. Also give context 0 a
Single override for kind 0 while its pooled field is Single kind 1, and require
successful decoding of kind 0 plus completion to prove override precedence.

### TVG-0590

Through the typed-LZSS adapter, decode the documented one-Literal `A` vector
with empty payload and empty table workspace. The write-free pass must report
one token, two events, two decisions, zero bits, and raw size one. Publication
must produce exactly Literal `A` while leaving the following sentinel token
unchanged.

Construct Literal `A` followed by Match `(distance=1,length=5)`. Use pooled
kind lengths `(1,1)`, Single literal `A`, Single length class 0, and Single
distance class 0. Payload bits `0,1` form byte `02` with two valid bits. Require
two tokens, five events/decisions, raw size six, exact contexts 0/3 then
1/21/23, and one non-Single table.

Independently reject a Match before history with stable token index and
`invalid_distance`, contradictory decision/raw counts, short table or token
workspace, invalid entropy completion, invalid LZSS parameters, per-frame and
aggregate output limits, and every payload/table/token overlap. Every failure
before publication must preserve all token bytes.

### TVG-0591

Serialize and parse the documented 112-byte stream header for frame size 64,
original size one, LZSS `2/2`, entropy `2/2`, and context model `1/1`. Require
byte-for-byte identity with the format specification. Independently serialize
the one-`A` frame header and append TVG-0588's 24-byte descriptor to form the
documented 88-byte complete frame.

Preflight and decode that frame with zero table entries, one private token, and
one raw byte. Require Literal `A`, raw byte `41`, required sizes `(0,1,1)`, and
consumed extent 88 while ignoring one following sentinel byte. Independently
reject wrong entropy identity/parameters, sequence, expected raw extent,
contradictory counts, nonzero optional sizes/reserved bytes, malformed or
truncated descriptor/payload, short workspaces, and every pairwise overlap.
Preserve raw output and keep consumed extent zero on every failure.

### TVG-0592

Construct a stream with frame size one and original size two by concatenating
the canonical `2/2` stream header with two TVG-0591 one-Literal frames whose
sequences are zero and one. Feed every input and output byte separately and
require exactly `AA`, deterministic progress statuses, latched final
`EndInput`, and stable repeated `EndOfStream`. The all-Single descriptor must
continue to accept empty Huffman table workspace.

Corrupt only the second frame and require the first raw byte to remain the sole
committed output. Independently reject short serialized/token/raw workspace,
aggregate buffered-byte overflow, truncation, trailing input, reset/unknown
flags, wrong entropy identity, construction-time workspace overlap, and output
alias with every workspace. Errors are sticky and must never return Progress
without consuming input or producing output.

### TVG-0593

Plan the documented one-Literal `A` operations and require the exact 24-byte,
all-Single descriptor with no payload. Encode the seven mixed operations from
TVG-0589 and require payload byte `06`, four final valid bits, one non-Single
table, and successful request-by-request decoder inversion.

Create 40 context-0 kind-zero symbols followed by 40 context-1 kind-one
symbols and one Single literal to complete the required active-field mask. The
pooled kind model costs 80 bits; each Single override saves 40 bits and costs
32 bits, so both overrides must be selected and the payload must be empty.
Independently reject malformed or incomplete operations, short payload output,
and operation/output overlap while preserving descriptor and output sentinels.

### TVG-0594

Plan one Literal `A` directly from a typed-token span and require two events,
two decisions, the exact 24-byte all-Single descriptor, and zero payload. Encode
Literal `A` followed by Match `(distance=1,length=5)` and require five events,
five decisions, payload byte `02`, two final valid bits, and successful typed
decoder inversion to six raw bytes.

Independently model the same token pair into its five symbol operations and
require byte-identical descriptor serialization and payload from the operation
and direct boundaries. Reject an initial match, short payload, and token/output
alias while preserving descriptor state.

### TVG-0595

Plan raw byte `A` into one typed Literal and require `(token,event,decision) =
(1,2,2)`, the 24-byte all-Single descriptor, zero payload, and complete size
88. Encode it and require byte identity with TVG-0591's documented frame, then
decode that generated frame through the independent complete-frame decoder.

Encode a mixed literal/match input twice into separate workspaces and require
byte-identical frames plus raw reconstruction. Independently reject an invalid
stream, a raw extent different from the next expected frame, short token and
serialized-output spans, aggregate workspace overflow, and every raw/token/
serialized overlap. All failures detected before publication must preserve the
serialized-output sentinel.

### TVG-0596

For frame size one and raw `AA`, concatenate the documented 112-byte stream
header with two TVG-0595 one-Literal frames whose sequences are zero and one.
Feed and drain one byte at a time and require exact byte identity, valid
progress statuses, latched final `EndInput`, and stable repeated
`EndOfStream`.

Require a full non-final frame to drain before `EndInput`, while `Flush` leaves
a partial frame open. Require empty input to emit only its stream header.
Independently reject short token or serialized-frame workspace, aggregate
buffered-byte overflow, premature and excess input, reset/unknown flags,
construction-time overlap, and output alias with every workspace. Errors are
sticky and must never return Progress without consuming input or producing
output.

### TVG-0597

Calculate the default 65,536-byte encoder frame and require 65,536 raw bytes,
65,536 typed tokens, a 739,905-byte serialized-frame ceiling, and explicitly
aligned token storage. Require a 17-byte input to use a 2,817-byte frame
ceiling, while empty input uses no frame or view storage.

For decoder limits `(frame=4096, block=1024, payload=2000)`, require a
4,625-byte serialized frame, 1,024 raw bytes and tokens, and 35 aligned Huffman
tables. Reject unsupported LZSS parameters, payload/block/table/aggregate
limits, forged layouts, short storage, and misalignment without publishing
views. Finally construct both streaming transforms solely from calculated
requirements and round-trip `ABABX` across three frames.

### TVG-0598

Compile a C11-only client against the public header. Initialize the Contextual
Blocked Huffman encoder configuration and require ABI-1 size tags plus the
documented LZSS defaults. Under a two-byte frame limit, query nonzero bounded
raw, serialized-frame, and aligned opaque-view regions, create the transform,
encode `ABABX`, and require Format 2 dictionary and entropy identities `2/2`.

Independently initialize and query decoding, create it from three fresh caller
regions, and require exact reconstruction across the three encoded frames.
Reject each one-byte-short region, overlapping used prefixes, misaligned views,
a null transform output, nonzero reserved fields, altered structure size or ABI
version, invalid direction, and null configuration/requirements/initializer
arguments. Every failed factory call must leave the handle null.

### TVG-0599

Through ABI 1 only, encode each required binary class twice and require exact
byte identity, dictionary identity `2/2`, entropy identity `2/2`, and exact
decode. Include empty input, all 256 one-byte inputs, ascending byte values,
257 zero bytes, a 259-byte four-value pattern, 513 deterministic generated
bytes, and generated lengths 63, 64, and 65.

For 193 deterministic generated bytes, compare whole-buffer encoding with
input/output chunk pairs `(1,1)`, `(7,5)`, and `(13,17)`, then decode each with
the same schedule. Locate the fourth frame from its checked header extents.
Independently alter its sequence, remove its last byte, and append strict
trailing data. Each decode must retain exactly the first 192 raw bytes, leave
the final output sentinel untouched, and repeat the identical terminal error
category and position without further progress.

### TVG-0600

Encode `ABABX` through ABI 1 under a five-byte frame and bounded `6F` decision
limit. For every strict prefix of the canonical stream, require both the
private complete-frame boundary, whenever its stream header parses, and a fresh
public streaming decoder to reject without changing a raw-output sentinel.
Independently overwrite frame offsets 16 through 47 with `FF`, then set the
descriptor flags byte at `112 + 64 + 15`; require the same dual atomic failure.

For fuzz input, truncate inspection to 32 KiB. Exercise the private frame only
after the exact stream header validates. Exercise the public decoder with
input chunks 1..17 and output chunks 1..19 derived from input bytes, at most
1,024 raw bytes per frame, 6,144 decisions, 11,520 payload bytes, 35 bounded
decode tables, 4 KiB total output, fixed workspaces, and at most
`input + output + 32` calls. Abort on over-reported counts, impossible
zero-progress status, exhausted input requesting more input, or call-bound
exhaustion; accept every documented terminal stream status.

### TVG-0601

Reconfigure the existing Windows Clang 22 GNU-driver sanitizer tree and build
only `marc_fuzz_lzss_contextual_blocked_huffman_stream`. Query that compiler's
resource directory and prepend its `lib/windows` directory only to the child
process `PATH`. Run without a corpus using
`-runs=1000 -max_len=32768 -timeout=5 -rss_limit_mb=512`; direct failure
artifacts to an ignored Contextual Blocked Huffman build directory.

Require normal exit after exactly 1,000 inputs. Record final coverage and
feature counts, peak RSS, and whether libFuzzer, AddressSanitizer, or
UndefinedBehaviorSanitizer reports a crash, hang, or finding. Do not persist
generated inputs or add an empty artifact directory to the repository.

### TVG-0602

Register one experimental benchmark smoke over the repository's `README.md`
with one measured iteration. Select `lzss-contextual-blocked-huffman`, query
encoder and decoder workspace requirements separately through ABI 1, allocate
the three caller-owned regions with the reported opaque-view alignment, and
reserve encoded output through the checked `112 + 12N + 2,625K` ceiling.

Before timing, encode once, decode the exact produced prefix, and require the
decoded extent and every byte to equal the input. During measurement require
every repeated encode and decode to reproduce the verified extents. Report
complete-stream size and ratio, both throughputs, each directional workspace
region, and their peak sum. A smoke pass establishes wiring and round-trip
correctness only; it imposes no ratio, speed, or workspace threshold and does
not add a stable profile.

### TVG-0603

Register `marc_cli_lzss_contextual_blocked_huffman_round_trip` through the
common CLI file script. Build a deterministic repeated binary-text fixture,
encode with the explicit selector, require Format 2 entropy bytes `02/02`,
refuse a second write to the same path, decode with the same selector, and
compare every output byte. Repeat for empty input.

Decode a malformed file and a canonical archive with one appended byte.
Require both operations to fail and leave neither the destination nor its
temporary file. Exercise both directions through the public C lifecycle with
the fixed 65,536-byte frame, 393,216-decision, 786,432-byte payload, and 8-MiB
aggregate policies. This test does not add auto-detection, a stable profile,
or an interoperability archive.

### TVG-0604

Generate a schema-35 bundle from the local CLI and the existing deterministic
8,193-byte fixture. Preserve all 45 schema-34 profiles in order and append
`lzss-contextual-blocked-huffman`. Require generator-side round trip, then
verify `35`/`marc-cli-v35`, exactly 46 archives, leaf-only names, recorded
sizes and SHA-256 values, decoded fixture equality, and byte-identical local
re-encoding for every entry.

Swap the first two schema-35 manifest entries and require exact-order
rejection. Derive schema 34 by retaining its first 45 named profiles and
restoring `34`/`marc-cli-v34`, verify it, then derive and verify every earlier
schema through schema 1. Use temporary directories only and remove them after
success or failure. External producer/consumer evidence is not part of this
local test.

### TVG-0605

Compile `src/frame/lzss_contextual_blocked_huffman_frame_decoder.cpp` alone as
a C++20 translation unit with Ubuntu Clang, repository public/internal include
paths, and `-Wall -Wextra -Wpedantic -fsyntax-only`. Require success without
diagnostics after adding the direct `<utility>` dependency. Audit all source
and header files containing `std::in_range` and require each to include
`<utility>` directly. Then rebuild the affected MSVC and ClangCL library, CLI,
and core-test targets and run the complete registered suite under both local
compilers.

### TVG-0606

At pushed revision `7c276151ab428aa9ba0376f8d9ba9a85a9fbd347`, verify the
Windows/MSVC and Ubuntu 24.04/Ninja schema-35 CI bundles with the Ubuntu 26.04
Clang 21.1.8 CLI. Generate a schema-35 Ubuntu 26.04 bundle from the same
revision, verify it locally, then verify it with the Windows/MSVC CLI.

Require all four final lines to report exactly 46 archives, the expected
producer label, and the identical full revision. Each verifier pass must
therefore cover manifest identity/order, size and SHA-256, fixture decode, and
byte-identical local re-encoding. Record only the results; do not import any
bundle or generated archive into the repository.

### TVG-0607

From reset LZSS context state, map raw byte `A` to the established two Symbol
operations `(context=0, alphabet=2, value=0)` and
`(context=3, alphabet=256, value=65)`. Start an independent NYT-only FGK tree
for each context. Emit one zero raw bit for the first value, followed by the
eight LSB-first bits of `0x41`; pack the nine physical bits as `82 00` with one
valid bit in the final byte.

Serialize decision count 2, payload size 2, context count 31, final-valid-bits
1, and zero flags/reserved fields into the fixed 16-byte descriptor. Combine
it with the specified Format 2 frame and stream headers and require the exact
82-byte frame and 194-byte stream. Documentation checks must also require the
`1/2` entropy identity, 2^24 raw-frame ceiling, 33,554,432 Symbol-event ceiling,
per-alphabet NYT widths, zero padding, and explicit non-implementation status.

### TVG-0608

Serialize the one-Literal descriptor tuple `(decisions=2, payload=2,
contexts=31, valid-bits=1, flags=0)` and require the exact 16 bytes from
TVG-0607. Parse them back, then independently mutate every semantic field,
reserved bytes, expected counts, and payload limits; require a stable error and
unchanged destination object or byte array.

Initialize one caller-owned FGK tree for each alphabet 2, 8, 17, and 256.
Require initial order `2A`, exact bounded-workspace rejection, successful
insertion of every alphabet symbol with validation after each insertion,
out-of-alphabet and duplicate rejection, and reset to NYT. For alphabet 256,
walk `A`, `B`, `A` through both the new tree and Adaptive Huffman variant 1;
require identical paths and valid state after every observation.

### TVG-0609

Allocate exactly 9,067 `AdaptiveHuffmanNode` entries and 4,518 symbol indices,
initialize all 31 contexts, update context 0, and prove context 1 remains reset.
Reject either short region and overlapping node/symbol storage before model
publication.

Decode TVG-0607 payload `82 00` by requesting `(0,2)` then `(3,256)` and
require values 0 and 65, bit offsets 1 and 9, two events, two decisions, and
exact finish. Independently decode payload `5A` with seven valid bits by
requesting context-0 new 0, existing 0, new 1, then three bypass bits; require
values `0,0,1,5`, four events, six decisions, and exact finish.

Require wrong context/alphabet/width and excess decisions to become sticky.
For a one-bit truncated context-3 NYT and the unused 5-bit value 31 in alphabet
17, require unchanged output, zero committed bits, and zero events. Reject
nonzero padding, trailing bits, wrong finish counts, payload/model overlap,
short workspaces, and limits one below the exact entry or aggregate-byte need.

### TVG-0610

Decode the documented `82 00` payload through reset LZSS context state and
require one Literal `0x41`, two events, two decisions, nine consumed bits, and
raw extent one. Decode `82 06 00` with 19 valid bits and require Literal
`0x41` followed by Match distance one and length six, six events, six
decisions, and raw extent seven.

Change the second vector's distance to two at raw position one and require an
invalid-token result with the entire caller token span unchanged. Independently
reject wrong event, decision, and raw declarations, payload truncation, either
short model workspace, short token output, every token alias with payload or
model storage, invalid LZSS parameters, aggregate storage one byte below the
exact requirement, and total-output overflow. Run the vectors under MSVC and
ClangCL before the complete registered suites.

### TVG-0611

Serialize the reserved one-byte stream header and require entropy identity
`1/2`, maximum Symbol events 33,554,432, context count 31, maximum NYT raw
width eight, zero flags, and zero reserved bytes at the documented offsets.
Parse it back without modifying outputs on failure, then reject wrong identity,
parameters, reserved bytes, dictionary bounds, frame ceiling, model-entry
limit, and truncated prefixes.

Combine the documented 64-byte frame header, 16-byte descriptor, and payload
`82 00`; require serialized extent 82 and exact header/descriptor agreement.
Round-trip the header, reject every truncated prefix transactionally, and
independently reject sequence/raw/count/feature/descriptor contradictions,
nonzero reserved bytes, aggregate or payload limits, and extra caller bytes
without consuming them.

### TVG-0612

Decode the documented 82-byte one-Literal frame using exact caller-owned
9,067-node, 4,518-symbol, one-token, and one-byte raw regions. Require one
consumed frame, Literal `0x41`, raw byte `0x41`, exact required capacities, and
unchanged sentinel storage beyond every used prefix. Append one unrelated byte
and require it to remain unconsumed.

For every preflight failure, either short model region, short token or raw
region, and each representative serialized/workspace alias, require zero
serialized consumption and unchanged raw output. Set a high unused payload bit
under a one-valid-bit descriptor and require a nested entropy padding error
before raw publication. Run these tests under MSVC and ClangCL before both
complete registered suites.

### TVG-0613

Construct a stream from the documented 112-byte header and 82-byte one-Literal
frame. Decode it with every input byte supplied separately and one-byte output,
then require raw `0x41`, exact completion, and stable repeated EndOfStream.
Repeat with the whole input while output capacity is zero, latch `EndInput`,
and require `NeedOutput` followed by successful drain without more input.
Encode two independent one-byte frames in one stream and require both literal
bytes after the per-frame model reset.

Exercise empty input, `Flush`, unsupported `ResetBlock`, unknown flags,
truncated stream and frame headers/bodies, strict trailing input, malformed
padding, short serialized/node/symbol/token/raw workspaces, pairwise workspace
overlap, output/workspace overlap, aggregate memory one byte below the exact
need, and sticky terminal errors. Require no `Progress` result with both counts
zero and run under MSVC and ClangCL before both complete registered suites.

### TVG-0614

Plan and encode the documented one-Literal operations `Symbol(0,2,0)` and
`Symbol(3,256,65)` and require payload `82 00`, nine payload bits, two
decisions, and one valid bit in the last byte. Encode repeated symbols and a
three-bit bypass sequence, then decode every operation through the existing
operation decoder and require exact values and synchronized tree completion.

Exercise every invalid operation kind, context, alphabet, symbol, bypass
width, and unused field at a stable index. Require empty input, short node,
symbol, and payload storage, operation/model/output overlap classes, entropy
entry, compressed payload, and aggregate-memory limits to fail before output
or descriptor publication. Verify trailing output capacity is unchanged and
run focused and complete registered suites under MSVC and ClangCL.

### TVG-0615

Plan and encode one literal token `A`; require one token, two events, two
decisions, nine bits, descriptor size two, final-valid-bit count one, and exact
payload `82 00`. Encode `{Literal A, Match(distance=1,length=6)}` directly and
require the established `82 06 00` payload, then decode it through the existing
token adapter and compare both tokens and the seven-byte raw extent.

Exercise invalid parameters and tokens, raw-extent disagreement, short node,
symbol, and payload storage, token/model/output aliases, total-output,
entropy-entry, compressed-payload, and exact aggregate-memory limits. Require
all prewrite failures to preserve payload and descriptor, trailing output
capacity to remain untouched, and fresh workspaces to produce identical bytes.
Run focused and complete registered suites under MSVC and ClangCL.

### TVG-0616

Plan and encode raw byte `41` with the reserved one-byte stream parameters.
Require one typed token, two events, two decisions, the fixed 16-byte
descriptor, two payload bytes, 82 serialized bytes, and exact equality with
the documented complete frame. Decode that output through the existing
complete-frame decoder and require raw `41`. Repeat a mixed literal/match raw
frame with fresh workspaces and require deterministic bytes and round trip.

Exercise short token, node, symbol, and serialized storage; invalid stream and
frame-size inputs; every raw/token/model/output alias class; and aggregate
workspace one byte below the exact requirement. Require all failures before
serialized publication, preserve trailing output capacity and unused token
capacity, and run focused and complete registered suites under MSVC and
ClangCL.

### TVG-0617

Encode two raw `41` bytes with one-byte frames using one-byte input and output
chunks. Require the documented 112-byte stream header followed by two
independent 82-byte frames with sequence numbers zero and one, exact byte
identity, complete input consumption, and sticky EndOfStream. Feed a full
frame before `EndInput`, require immediate frame output, and require `Flush`
on a partial frame to leave that frame open.

Latch final input through zero-capacity frame drain and through empty-stream
header drain. Exercise short token, node, symbol, and serialized-frame
workspaces, aggregate limit, premature and excess input, every constructor and
process-output alias class, unknown flags, and `ResetBlock`; require stable
error mapping and sticky terminal results. Run focused and complete registered
suites under MSVC and ClangCL.

### TVG-0618

Build the default profile for 2,500,000 raw bytes and a 65,536-byte frame.
Require the fixed stream parameters, `F` token entries, 9,067 node entries,
4,518 symbol entries, exact aligned offsets, and encoded-frame ceiling
`64 + 16 + ceil(267F/8)`. Repeat with a 17-byte final frame and with empty
input, whose encoder frame and typed-view requirements are all zero.

Calculate decoder storage from restricted frame, block, payload, entropy-entry,
and aggregate limits. Partition aligned storage in both directions, reject
forged offsets/counts/bytes/alignment, short and misaligned storage without
publishing views, and construct the streaming encoder and decoder solely from
the returned requirements for a multi-frame round trip. Verify stable core
error mapping under MSVC and ClangCL.

### TVG-0619

Initialize the new Contextual Adaptive Huffman C configuration in both
directions and require ABI-1 size tags, canonical LZSS defaults, and the
private profile's exact direction-specific workspace extents. Construct the
encoder solely from the returned three regions, encode `41 42 41 42 58`, and
require the Format 2 identity `2/2 + 1/1 + 1/2`. Reconstruct the input through
a separately queried decoder and the common process/destroy lifecycle.

Reject short primary, secondary, and views regions, misaligned views, every
pairwise used-prefix overlap, a null transform output, nonzero reserved fields,
wrong structure size or ABI version, null arguments, and invalid directions.
Require failed factories to leave the transform pointer null. Run the C11
boundary through the configured production-library target, build both shared
and static libraries, and execute the full registered suites under MSVC and
ClangCL.

### TVG-0620

Through only the public Contextual Adaptive Huffman C lifecycle, round trip
empty input, each one-byte value, the ordered 256-byte alphabet, 257 zeroes, a
259-byte mixed binary pattern, 513 deterministic pseudo-random bytes, and
deterministic inputs of 63, 64, and 65 bytes. Encode each case twice and require
identical Format 2 identity `2/2 + 1/1 + 1/2` and bytes.

Encode and decode 193 deterministic bytes through whole-buffer, one-byte, and
two mixed chunk schedules and require the same stream and raw output. Corrupt
the final frame sequence, truncate its last byte, and append one trailing byte;
require each public decoder to commit exactly the first 192 raw bytes, preserve
the sentinel final output byte, report malformed stream, and repeat the same
terminal error with zero further progress. Run the focused audit and complete
registered suites under MSVC and ClangCL.

### TVG-0621

Generate the canonical public stream for raw `41 42 41 42 58` using one
five-byte frame and locally bounded Contextual Adaptive Huffman limits. Require
every strict prefix to fail atomically through the public decoder and, after a
valid stream header, through the private complete-frame decoder.

Independently mutate fixed stream-header identity and reserved bytes, fill all
frame length/count fields with `FF`, set descriptor flags and reserved bytes,
replace context count and final-valid-bit fields with invalid values, and set
one unused high padding bit in the final payload byte. Require the public
decoder to return sticky malformed stream with zero output and both decoder
boundaries to preserve sentinel raw storage. Run the focused regressions and
complete registered suites under MSVC and ClangCL.

### TVG-0622

Compile the Contextual Adaptive Huffman dual-decoder harness as an ordinary
C++20 object under MSVC and ClangCL with the repository warning set. Require
compile-time bounds for 64 KiB supplied input, 4 KiB output, a 1 KiB raw frame,
`ceil(267 * 1024 / 8)` payload bytes, the exact fixed node/symbol model bank,
1,024 tokens, aligned public views, and a finite input-plus-output call budget.

The harness must parse a stream header before private complete-frame decoding,
always drive the public decoder with byte-derived chunks, reject requirements
outside its fixed storage, and abort on transform-contract violations. Re-run
the five permanent malformed regressions and complete registered suites under
both compilers. Do not execute libFuzzer or write corpus/artifact files in this
milestone.

### TVG-0623

Reconfigure the established Windows Clang 22 GNU-driver sanitizer tree and
build only `marc_fuzz_lzss_contextual_adaptive_huffman_stream`. Query that
compiler's resource directory and prepend its `lib/windows` directory only to
the child process `PATH`. Run without a corpus using
`-runs=1000 -max_len=65536 -timeout=5 -rss_limit_mb=512`; direct failure
artifacts to an ignored Contextual Adaptive Huffman build directory.

Require normal exit after exactly 1,000 inputs. Record final coverage and
feature counts, in-memory corpus extent, peak RSS, and whether libFuzzer,
AddressSanitizer, or UndefinedBehaviorSanitizer reports a crash, hang, or
finding. Do not persist generated inputs or add an empty artifact directory to
the repository.

### TVG-0624

Register one experimental benchmark smoke over the repository's `README.md`
with one measured iteration. Select `lzss-contextual-adaptive-huffman`, query
encoder and decoder workspace requirements separately through ABI 1, allocate
the three caller-owned regions with the reported opaque-view alignment, and
reserve encoded output through the checked
`112 + 80K + ceil(267N/8)` ceiling.

Before timing, encode once, decode the exact produced prefix, and require the
decoded extent and every byte to equal the input. During measurement require
every repeated encode and decode to reproduce the verified extents. Report
complete-stream size and ratio, both throughputs, each directional workspace
region, and their peak sum. A smoke pass establishes wiring and round-trip
correctness only; it imposes no ratio, speed, or workspace threshold and does
not add a stable profile.

### TVG-0625

Register `marc_cli_lzss_contextual_adaptive_huffman_round_trip` through the
common CLI file script. Build a deterministic repeated binary-text fixture,
encode with the explicit selector, require Format 2 entropy bytes `01/02`,
refuse a second write to the same path, decode with the same selector, and
compare every output byte. Repeat for empty input.

Decode a malformed file and a canonical archive with one appended byte.
Require both operations to fail and leave neither the destination nor its
temporary file. Exercise both directions through the public C lifecycle with
the fixed 65,536-byte frame, exact 13,585-entry model bank,
2,187,264-byte payload ceiling, and 8-MiB aggregate policies. This test does
not add auto-detection, a stable profile, or an interoperability archive.

### TVG-0626

Generate a schema-36 bundle from the local CLI and the deterministic
8,193-byte fixture. Preserve all 46 schema-35 profiles in order and append
`lzss-contextual-adaptive-huffman`. Require generator-side round trip, then
verify `36`/`marc-cli-v36`, exactly 47 archives, leaf-only names, recorded
sizes and SHA-256 values, decoded fixture equality, and byte-identical local
re-encoding for every entry.

Swap the first two schema-36 manifest entries and require exact-order
rejection. Convert the canonical schema-36 bundle to schema 35 by removing
only the new final archive, then continue the existing schema-35-through-1
compatibility chain. Leave all generated bundles under the temporary test
root and remove them after either success or failure.

### TVG-0627

At pushed revision `bdcabd439d9cedb9e58f3dd2a3ac4dcb3526e1a2`, verify the
Windows/MSVC and Ubuntu 24.04/Ninja schema-36 CI bundles with the Ubuntu 26.04
Clang 21.1.8 CLI. Generate a schema-36 Ubuntu 26.04 bundle from the same
revision, verify it locally, then verify it with the Windows/MSVC CLI.

Require all four final lines to report exactly 47 archives, the expected
producer label, and the identical full revision. Each verifier pass must
therefore cover manifest identity/order, size and SHA-256, fixture decode, and
byte-identical local re-encoding. Record only the results; do not import any
bundle or generated archive into the repository.

### TVG-0628

Retain the existing variant-3 one-byte hand vector and deterministic fixtures
under the canonical unqualified Contextual rANS names and require every byte
to remain unchanged. Add an entropy-identity `4/2` stream-header negative test.
Require the public C lifecycle, CLI, benchmark, completion matrix, malformed
regressions, split buffers, and bounded fuzz harness to expose no compact
selector or symbol.

Generate schema 37 with 47 archives and canonical
`lzss-contextual-rans` at archive 44. Verify exact order, hashes, fixture
decode, and byte-identical re-encoding. Convert it to schema 36 by renaming
only that entry and leaf file to `lzss-contextual-rans-compact`, use the
verifier's private legacy-name mapping, and continue through schema 1. Run all
registered tests under MSVC and ClangCL, then run bounded sanitizer and
benchmark comparisons before merge.

### TVG-0629

Build the library, CLI, benchmark, core tests, and pure-C Contextual rANS test
after removing the fixed public dispatch and compact-qualified public names.
Require the unqualified CLI round trip to identify entropy `4/3`; require the
variant-3 C11 workspace extents, encode/decode round trip, alignment and
overlap rejection, and configuration validation under the unqualified family.

Collapse the dual-representation public completion and malformed fixtures to
one canonical parameter while retaining every input class, chunk schedule,
terminal check, truncation, extreme-length, descriptor mutation, sentinel, and
sticky-error assertion. Run those six cases plus the C11, CLI, and benchmark
smokes under both MSVC and ClangCL before private frame renaming.

### TVG-0630

After promoting variant 3 to the unqualified frame and profile names, run the
canonical stream-header vector, every truncation, and an explicit entropy
identity `4/2` rejection that preserves caller state. Retain the variant-3
frame encode/decode, capacity, aliasing, limit, one-byte streaming, flush,
sticky-error, completion, malformed, and CLI assertions exactly once. Build
the complete core-test target warning-clean before running this focused set.

### TVG-0631

Compile the sole Contextual rANS fuzz harness without a representation macro
under the warning-clean ClangCL compile-smoke target. Require its private
complete-frame oracle and public streaming lifecycle to select the canonical
variant-3 path, share the same bounded input, output, payload, and workspace
limits, and retain no compact-qualified public symbols or duplicate target.

### TVG-0632

Promote variant 3's exact one-literal descriptor, dense/sparse selection,
maximum-size, strict-prefix, malformed-record, limit, and transactional-output
vectors to the unqualified format suite. Begin the scalar decoder only from
the serialized descriptor and retain state, alphabet, activity, bypass,
decision-budget, mutated-table, truncated-renormalization, terminal-state,
trailing-payload, and lifecycle failures. Exercise the direct typed-token
bridge with invalid matches, contradictory counts, raw-size mismatch, short
and overlapping workspaces, aggregate limits, deterministic direct encoding,
and exact descriptor-size agreement. Build warning-clean and run the focused
canonical Contextual rANS set under ClangCL and MSVC.

### TVG-0633

Generate schema 37 with the canonical Contextual rANS name at archive 44,
verify all 47 archives, and reject a reordered manifest. Convert schema 37 to
36 by renaming only that manifest entry and leaf file, privately map the old
name to the canonical CLI selector, and verify the unchanged bytes before
continuing through every schema to 1. Exercise Contextual tANS, Blocked
Huffman, and Adaptive Huffman stream-header round trips after changing the
common adapted identity from retired `4/2` to canonical `4/3`; require their
serialized entropy identities to remain `5/2`, `2/2`, and `1/2`.

### TVG-0634

Run every registered Release test under MSVC and ClangCL with a 240-second
per-test timeout and require `marc_interoperability_schema_compatibility` to
run rather than be excluded. Rebuild only
`marc_fuzz_lzss_contextual_rans_stream` in the established Windows Clang 22
GNU-driver sanitizer tree, prepend that compiler's runtime directory only to
the child process, and execute without a persistent corpus using
`-runs=1000 -max_len=65536 -timeout=5 -rss_limit_mb=512`.

Run `marc_benchmark lzss-contextual-rans README.md 1` and require the same
4,326-byte input and 3,006-byte encoded extent recorded for variant 3 before
the rename. Treat throughput as descriptive because a one-iteration run is
noisy. Require `git diff --check` to pass and require no compact-qualified
file beneath public, implementation, tool, fuzz, or test roots. Historical
schema mappings and provenance text may retain the old name. Do not claim
schema-37 external interoperability until the four-direction exchange is run
from one pushed revision.

### TVG-0635

At pushed revision `58b829dafa078e7dadd46e5de9ed7b1af45b5cc2`, verify the
Windows/MSVC and Ubuntu 24.04/Ninja schema-37 CI bundles with the Ubuntu 26.04
Clang 21.1.8 CLI. Generate a schema-37 Ubuntu 26.04 bundle from the same
revision, verify it locally, then verify it with the Windows/MSVC CLI.

Require all four final lines to report exactly 47 archives, the expected
producer label, and the identical full revision. Each verifier pass must cover
manifest identity and order, size and SHA-256, fixture decode, and
byte-identical local re-encoding. Record only the results; do not import any
bundle or generated archive into the repository.

### TVG-0636

Freeze the existing Exhaustive LZSS match, typed-token, serialized-token, and
complete-stream outputs before integrating an indexed finder. For HashChain
Exact, compare every selected distance and length against Exhaustive on empty
and one-byte inputs, all byte values, minimum/maximum match neighbors, window
neighbors, distance-one overlap, equal-length nearest-distance ties, deliberate
hash collisions, repeated prefixes, zero and periodic runs, deterministic
pseudorandom bytes, and frame-reset boundaries.

Require byte-identical typed tokens, canonical serialized tokens, and complete
streams for every Exact case under MSVC and ClangCL. Reject short, misaligned,
overlapping, overflowed, or aggregate-limit-exceeding caller workspace before
publication. Add a bounded finite differential fuzz target only after the
deterministic suite passes; Exhaustive is its oracle and every finding becomes
a permanent regression. Measure finder-only candidate counts, parse
throughput, complete compression throughput, peak workspace, and current
two-pass cost before considering BinaryTree, automatic selection, Bounded, or
token reuse.

### TVG-0637

Exercise `LzssExhaustiveMatchFinder` directly at empty and exact-end positions,
distance-one overlap, configured maximum length, equal-length nearest-distance
ties, the exact window boundary, below-minimum candidates, and before/after its
no-op range notification. Require both canonical parsers to compile through
the concept-constrained contract and retain all existing typed/serialized
equivalence tests.

Freeze `ABCDE1ABCDE2ABCDE3` as a complete 34-byte canonical byte-token vector:
six Literals, distance-6 length-5 Match, Literal `2`, the same Match, and
Literal `3`. Require focused Exhaustive, byte-token, and typed-token tests to
pass warning-clean under MSVC and ClangCL before running the complete suites.

### TVG-0638

Calculate zero workspace below the five-byte prefix boundary; for 65,536-byte
input and default window require 65,536 heads, 65,536 links, native alignment,
and exactly `65,536 * (sizeof(size_t) + sizeof(uint32_t))` bytes on supported
targets. For a one-MiB window require one MiB of links but retain the 65,536
bucket cap. Reject invalid limits, invalid LZSS parameters, oversized input,
aggregate-limit excess, short workspace, one-byte misalignment, and actual
input/workspace overlap without publishing a finder.

Compare HashChain Exact with Exhaustive at every raw position for empty and
one-byte inputs, periodic and repeated data, nearest-distance ties, many common
prefixes, all 256 byte values repeated, deterministic pseudorandom input, and
a mixed 1,024-byte corpus across windows 1, 5, 17, 256, and 65,536 and maximum
match lengths 5, 17, and 258. Separately advance over a Match-sized range and
require the next query to remain identical, proving skipped positions enter
the index. Run this deterministic differential layer under MSVC and ClangCL
before production integration or fuzzing.

### TVG-0639

For both canonical one-shot encoders, parse a mixed repeated-prefix and full-
byte-alphabet input with Exhaustive and HashChain Exact. Require identical
plan counts and extents, byte-for-byte serialized tokens, and field-for-field
typed tokens. Require the detailed finder error to remain `none` on success.

Supply a workspace one byte short and require a stable workspace-too-small
detail without changing token output. Alias the supplied output region with
the finder workspace and require overlap rejection before either region is
modified. Set the aggregate internal-buffer limit one byte below raw input
plus exact workspace plus planned token storage and require the established
serialized/token-storage limit error. Run all four focused tests and then the
complete 2,829-test suite under both supported Release compilers, including
documentation layout and schema-37-through-1 compatibility.

### TVG-0640

Instrument `ABCDEABCDE` without changing finder output. Require ten queries
for both finders, 45 Exhaustive candidates, four HashChain candidates under the
specified capped hash table, and fewer HashChain byte comparisons. Leave
statistics optional and caller-owned.

Run the dedicated Release benchmark over the 4,326-byte repository README for
three iterations under MSVC and ClangCL. Before timing, require identical
2,390-token plans and identical 6,614-byte serialized output. Require both
finders to report 2,390 queries, record deterministic candidates and byte
comparisons plus the 82,840-byte workspace, and emit finite planning and two-
pass encoding metric fields. Register a one-iteration experimental smoke, but
assert no timing or ratio. Then run the complete 2,831-test suites, including
schema compatibility, under both compilers.

### TVG-0641

Compare the new single-pass HashChain typed output with the established exact-
capacity two-pass output over repeated prefixes followed by two copies of all
256 byte values. Require identical actual token count, storage extent, and
every token field; require the optional query count to equal the actual token
count and every unused worst-case token slot to retain its sentinel.

Provide one fewer than `input_size` token slots and require the conservative
required count and storage extent with atomic `output_too_small`. Set aggregate
memory one byte below raw plus exact finder workspace plus `input_size` tokens
and require atomic token-storage-limit rejection. Extend the internal benchmark
with verified typed two-pass and single-pass measurements, using ten descriptive
README iterations under MSVC and ClangCL. Run all 2,833 registered tests under
both compilers with schema compatibility included.

### TVG-0642

Encode repeated prefixes followed by two complete byte alphabets through the
existing Exhaustive and new HashChain typed Contextual Dynamic Range frame
routes. Require identical frame plan extents, token and operation counts, one
HashChain query per token, byte-identical serialized frames, and successful
decode back to the exact raw input.

Provide finder workspace one byte short and require the nested stable
workspace-too-small detail while serialized output retains its sentinel. Alias
serialized output with finder workspace and require rejection before that
shared region changes. Set the complete aggregate limit one byte below raw
input plus conservative typed-token reservation plus actual operation storage
plus exact finder workspace plus serialized frame and require workspace-limit
rejection. Extend the internal benchmark with ten verified complete-frame
iterations under MSVC and ClangCL, but assert no timing. Run all 2,835 tests,
including schema compatibility, under both compilers.

### TVG-0643

Build one match-bearing Contextual Dynamic Range stream with the promoted
streaming encoder and compare it byte for byte with the stream header followed
by the Exhaustive reference frame. Require exact input consumption, complete
output publication, and EndOfStream. Supply finder workspace one byte short
and require stable out-of-memory after only the already-valid stream header is
published; alias finder storage with raw staging and require constructor-time
invalid-argument without publication.

Require the encoder profile to calculate the exact largest-frame HashChain
extent, align and partition it after token and operation views, include it in
aggregate limits, and reject forged offsets, short storage, and misalignment
transactionally. Re-run the public C lifecycle, CLI round trip, and benchmark
smoke. Measure ten descriptive README iterations under MSVC and ClangCL while
asserting only the unchanged 2,389-byte stream. Run all 2,836 tests, including
schema compatibility, under both compilers.

### TVG-0644

Encode a match-bearing raw frame through the established Exhaustive and new
HashChain canonical Contextual rANS routes. Require identical plan extents,
descriptor size, token/event/decision counts, payload size, one finder query
per token, byte-identical descriptor and payload, and successful decode to the
exact raw input.

Provide finder workspace one byte short and require the nested stable
workspace-too-small detail while serialized output retains its sentinel. Alias
serialized output with finder workspace and require rejection before the
shared region changes. Set aggregate memory one byte below raw input plus the
conservative typed-token reservation plus exact finder workspace plus complete
frame and require workspace-limit rejection. Extend the internal benchmark
with ten verified Contextual rANS frame iterations under MSVC and ClangCL, but
assert no timing. Run all 2,838 tests, including schema compatibility, under
both compilers.

### TVG-0645

Build one match-bearing canonical Contextual rANS stream with the promoted
streaming encoder and compare it byte for byte with the stream header followed
by the Exhaustive reference frame. Require exact input consumption, complete
output publication, and EndOfStream. Supply finder workspace one byte short
and require stable out-of-memory after only the valid stream header is
published; alias finder storage with raw staging and require constructor-time
invalid-argument without publication.

Require the encoder profile to calculate the exact largest-frame HashChain
extent, align and partition it after typed-token views, include it in aggregate
limits, and carry it through the public C lifecycle. Re-run the public C, CLI,
and benchmark-smoke paths. Measure ten descriptive README iterations under
MSVC and ClangCL while asserting only the unchanged 3,006-byte stream. Run all
2,839 tests, including schema compatibility, under both compilers.

### TVG-0646

Encode a match-bearing raw frame through the established Exhaustive and new
HashChain Contextual tANS routes. Require identical plan extents, descriptor
size, token/event/decision counts, payload size, one finder query per token,
byte-identical descriptor and payload, and successful decode to the exact raw
input.

Provide finder workspace one byte short and require the nested stable
workspace-too-small detail while serialized output retains its sentinel.
Alias finder storage separately with encode tables and serialized output and
require rejection before either shared region changes. Set aggregate memory
one byte below raw input plus conservative token reservation, fixed encode
tables, exact finder workspace, and complete frame and require workspace-limit
rejection. Extend the internal benchmark with ten verified Contextual tANS
frame iterations under MSVC and ClangCL, but assert no timing. Run all 2,841
tests, including schema compatibility, under both compilers.

### TVG-0647

Build one match-bearing canonical Contextual tANS stream with the promoted
streaming encoder and compare it byte for byte with the stream header followed
by the Exhaustive reference frame. Require exact input consumption, complete
output publication, and EndOfStream. Supply finder workspace one byte short
and require stable out-of-memory after only the valid stream header is
published; alias finder storage independently with raw staging, typed tokens,
encode tables, serialized staging, and caller output and require rejection
before frame publication.

Require the encoder profile to calculate the exact largest-frame HashChain
extent, align and partition it after encode tables, reject a forged finder
offset, include it in aggregate limits, and carry it through the public C
lifecycle. Re-run public C, CLI, and benchmark-smoke paths. Measure ten
descriptive README iterations under MSVC and ClangCL while asserting only the
unchanged 3,005-byte stream. Run all 2,841 tests, including schema
compatibility, under both compilers.

### TVG-0648

Encode a match-bearing raw frame through the established Exhaustive and new
HashChain Contextual Blocked Huffman routes. Require identical serialized,
descriptor, token, event, decision, and payload extents, one finder query per
token, byte-identical descriptor and payload, and successful complete-frame
decode to the exact raw input.

Provide finder workspace one byte short and require the nested stable
workspace-too-small detail while serialized output retains its sentinel. Alias
finder workspace independently with raw input, typed-token staging, and
serialized output and require rejection before shared storage changes. Set
aggregate memory one byte below raw input plus conservative token reservation,
exact finder workspace, and complete frame and require workspace-limit
rejection. Extend the internal benchmark with ten verified Contextual Blocked
Huffman frame iterations under MSVC and ClangCL, but assert no timing. Run all
2,842 tests, including schema compatibility, under both compilers.

### TVG-0649

Build one match-bearing canonical Contextual Blocked Huffman stream with the
promoted streaming encoder and compare it byte for byte with the stream header
followed by the Exhaustive reference frame. Require exact input consumption,
complete output publication, and EndOfStream. Supply finder workspace one byte
short and require stable out-of-memory after only the valid stream header is
published; alias caller output with finder workspace and require constructor-
independent process-time rejection.

Require the encoder profile to calculate the exact largest-frame HashChain
extent, align and partition it after typed-token views, reject a forged finder
offset, include it in aggregate limits, and carry it through the public C
lifecycle. Re-run public C, CLI, and benchmark-smoke paths. Measure ten
descriptive README iterations under MSVC and ClangCL while asserting only the
unchanged 2,504-byte stream. Run all 2,843 tests, including schema
compatibility, under both compilers.

### TVG-0650

Encode a match-bearing raw frame through the established Exhaustive and new
HashChain Contextual Adaptive Huffman routes. Require identical serialized,
descriptor, token, event, decision, and payload extents, one finder query per
token, a byte-identical frame, and successful complete-frame decode to the
exact raw input.

Provide finder workspace one byte short and require the nested stable
workspace-too-small detail while serialized output retains its sentinel. Alias
finder workspace independently with raw input, typed-token staging, Adaptive
Huffman nodes, symbol lookup storage, and serialized output and require
rejection before shared storage changes. Set aggregate memory one byte below
raw input plus conservative token reservation, fixed model storage, exact
finder workspace, and complete frame and require workspace-limit rejection.
Extend the internal benchmark with ten verified Contextual Adaptive Huffman
frame iterations under MSVC and ClangCL, but assert no timing. Run all 2,844
tests, including schema compatibility, under both compilers.

### TVG-0651

Build one match-bearing canonical Contextual Adaptive Huffman stream with the
promoted streaming encoder and compare it byte for byte with the stream header
followed by the Exhaustive reference frame. Require exact input consumption,
complete output publication, and EndOfStream. Supply finder workspace one byte
short and require stable out-of-memory after only the valid stream header is
published; alias finder independently with raw, token, node, symbol, frame, and
caller-output regions and require rejection before shared storage is used.

Require the encoder profile to calculate the exact largest-frame HashChain
extent, align and partition it after symbol lookup storage, reject a forged
finder offset, include it in aggregate limits, and carry it through the public
C lifecycle. Re-run public C, CLI, completion, and benchmark-smoke paths.
Measure ten descriptive README iterations under MSVC and ClangCL while
asserting only the unchanged 2,572-byte stream. Run all 2,845 tests, including
schema compatibility, under both compilers.

### TVG-0652

Encode one match-bearing entropy-none LZSS frame through the established
Exhaustive and new HashChain routes. Require identical serialized size, raw
size, token count, frame header, canonical token payload, and complete frame,
then decode the HashChain frame to the exact raw input.

Provide finder workspace one byte short and require the nested stable
match-finder error while the complete serialized output retains its sentinel.
Alias serialized output independently with finder workspace and raw input and
require rejection before header publication. Set aggregate memory one byte
below raw input plus exact finder workspace plus complete frame and require the
stable serialized-limit category. Extend the internal benchmark with ten
verified standalone frame iterations under MSVC and ClangCL, but assert no
timing. Run all 2,846 tests, including schema compatibility, under both
compilers.

### TVG-0653

Build the standalone LZSS streaming encoder with the exact six-byte-frame
HashChain workspace and require complete byte identity with the established
Exhaustive stream across two frames. Repeat with finder capacity one byte short
and require out-of-memory only when the first frame is prepared. Present the
finder as caller output and require invalid-argument before input consumption.

Query and exercise the public C encode lifecycle with its enlarged secondary
reservation, then decode through the unchanged public route and require the
same 214-byte small vector and exact raw output. Run the README benchmark for
ten iterations under MSVC and ClangCL, requiring the unchanged 6,750-byte
stream but asserting no timing. Run all registered tests, including schema
compatibility, under both compilers with the 240-second per-test limit.

### TVG-0654

Encode one match-bearing byte-oriented LZSS plus Blocked Huffman frame through
the established Exhaustive route and the private HashChain route. Require
identical dictionary byte staging, token extent, entropy block count,
descriptor extent, payload extent, serialized extent, complete frame bytes,
and successful strict decode through the unchanged decoder.

Provide finder capacity one byte short and require its stable nested error
without modifying dictionary staging. Alias each raw, dictionary, finder, and
serialized region and require rejection. Set aggregate memory one byte below
raw plus exact dictionary plus finder plus complete frame on a large enough
vector that the lower entropy workspace is admitted. Extend the internal README
benchmark with ten verified iterations under MSVC and ClangCL, but assert no
timing. Run all 2,848 registered tests, including schema compatibility, under
both compilers.

### TVG-0655

Construct the byte-oriented LZSS plus Blocked Huffman streaming encoder with
the exact HashChain workspace and require complete stream identity with the
legacy Exhaustive constructor on a match-bearing input. Repeat with finder
capacity one byte short and require out-of-memory at frame preparation. Alias
finder storage with constructor workspaces and caller output and require
invalid-argument without input consumption.

Query the public C encoder for a match-bearing twelve-byte frame and require a
nonzero aligned opaque finder extent. Reject one-byte-short and misaligned
views, encode through the optimized path, and decode through the unchanged
public path to the exact input. Run the public README benchmark for ten
iterations under MSVC and ClangCL while asserting no timing threshold. Run all
registered tests, including schema compatibility, under both compilers with
the 240-second per-test limit.

Also encode the CLI's 76,800-byte repeated-text frame with a 65,536-symbol
entropy-block limit. Require successful HashChain planning to prove that the
dictionary raw-frame extent is not incorrectly constrained by the downstream
entropy-block extent, then require the public CLI round trip under both
compilers.

### TVG-0656

Encode one match-bearing byte-oriented LZSS plus Adaptive Huffman frame through
the established Exhaustive route and the private HashChain route. Require
identical canonical dictionary staging, dictionary extent, fixed descriptor
extent, payload extent, serialized extent, complete frame bytes, and strict
decode through the unchanged decoder.

Provide finder capacity one byte short and require its stable nested error
without modifying dictionary staging. Alias every pair involving raw input,
dictionary staging, finder workspace, and serialized output and require
rejection before publication. Set aggregate memory one byte below raw plus
exact dictionary plus finder plus complete frame and require workspace-limit.
Extend the internal README benchmark with ten verified iterations under MSVC
and ClangCL while asserting no timing. Run all 2,851 registered tests,
including schema compatibility, under both compilers.

### TVG-0657

Construct the byte-oriented LZSS plus Adaptive Huffman streaming encoder with
the exact aligned HashChain workspace and require complete stream identity
with the retained Exhaustive C++ oracle on a match-bearing frame. Repeat with
finder capacity one byte short and require out-of-memory at frame preparation.
Alias finder storage with constructor workspaces and caller output and require
invalid-argument without input consumption.

Query the public C encoder for a match-bearing twelve-byte frame and require
secondary capacity beyond dictionary and frame staging, encode through the
optimized path, and decode through the unchanged public path to the exact
input. Exercise the public CLI round trip with a bound that includes the
HashChain worst case of one platform-width head and one 32-bit link per raw
position. Run the public README benchmark for ten iterations under MSVC and
ClangCL while asserting no timing threshold. Run all 2,852 registered tests,
including schema compatibility, under both compilers with the 240-second
per-test limit.

### TVG-0658

Encode one match-bearing byte-oriented LZSS plus Dynamic Range frame through
the established Exhaustive route and the private HashChain route. Require
identical canonical dictionary staging, dictionary extent, fixed descriptor
extent, payload extent, serialized extent, complete frame bytes, and strict
decode through the unchanged decoder.

Provide finder capacity one byte short and require its stable nested error
without modifying dictionary staging. Alias every pair involving raw input,
dictionary staging, finder workspace, and serialized output and require
rejection before publication. Set aggregate memory one byte below raw plus
exact dictionary plus finder plus complete frame and require workspace-limit.
Extend the internal README benchmark with ten verified iterations under MSVC
and ClangCL while asserting no timing. Run all 2,853 registered tests,
including schema compatibility, under both compilers.

### TVG-0659

Construct the byte-oriented LZSS plus Dynamic Range streaming encoder with the
exact aligned HashChain workspace and require complete stream identity with the
retained Exhaustive C++ oracle on a match-bearing frame. Repeat with finder
capacity one byte short and require out-of-memory at frame preparation. Alias
finder storage with constructor workspaces and caller output and require
invalid-argument without input consumption.

Query the public C encoder for a match-bearing twelve-byte frame and require
secondary capacity beyond dictionary and frame staging, encode through the
optimized path, and decode through the unchanged public path to the exact
input. Exercise the public CLI with an aggregate bound containing one platform-
width head and one 32-bit link per raw position. Run the public README benchmark
for ten iterations under MSVC and ClangCL while asserting no timing threshold.
Run all 2,854 registered tests, including schema compatibility, under both
compilers with the 240-second per-test limit.

### TVG-0660

Encode one match-bearing byte-oriented LZSS plus rANS frame through the
established Exhaustive route and the private HashChain route. Require identical
canonical dictionary staging, dictionary extent, rANS block count, descriptor
extent, payload extent, serialized extent, complete frame bytes, and strict
decode through the unchanged decoder.

Provide finder capacity one byte short and require its stable nested error
without modifying dictionary staging. Alias every pair involving raw input,
dictionary staging, finder workspace, and serialized output and require
rejection before publication. Set aggregate memory one byte below raw plus
exact dictionary plus finder plus complete frame and require workspace-limit.
Extend the internal README benchmark with ten verified iterations under MSVC
and ClangCL while asserting no timing. Run all 2,855 registered tests,
including schema compatibility, under both compilers.

### TVG-0661

Construct the byte-oriented LZSS plus rANS streaming encoder with the exact
aligned HashChain workspace and require complete stream identity with the
retained Exhaustive C++ oracle on a match-bearing frame. Repeat with finder
capacity one byte short and require out-of-memory at frame preparation. Alias
finder storage with constructor workspaces and caller output and require
invalid-argument without input consumption.

Query the public C encoder for a match-bearing twelve-byte frame and require
secondary capacity beyond dictionary and frame staging; reject a one-byte-
short secondary region, then encode through the optimized path and decode
through the unchanged public path to the exact input. Exercise the public CLI
and benchmark with aggregate limits containing the finder worst case. Run the
public README benchmark for ten iterations under MSVC and ClangCL without a
timing threshold. Run all 2,856 registered tests, including schema
compatibility, under both compilers with the 240-second per-test limit.

### TVG-0662

Encode one match-bearing byte-oriented LZSS plus tANS frame through the
established Exhaustive route and the private HashChain route. Require identical
canonical dictionary staging, dictionary extent, tANS block count, descriptor
extent, payload extent, serialized extent, complete frame bytes, and strict
decode through the unchanged decoder.

Provide finder capacity one byte short and require its stable nested error
without modifying dictionary staging. Alias every pair involving raw input,
dictionary staging, finder workspace, and serialized output and require
rejection before publication. Set aggregate memory one byte below raw plus
exact dictionary plus finder plus complete frame and require workspace-limit.
Extend the internal README benchmark with ten verified iterations under MSVC
and ClangCL while asserting no timing. Run all 2,857 registered tests,
including schema compatibility, under both compilers.

### TVG-0663

Construct the byte-oriented LZSS plus tANS streaming encoder with the exact
aligned HashChain workspace and require complete stream identity with the
retained Exhaustive C++ oracle on a match-bearing frame. Repeat with finder
capacity one byte short and require out-of-memory at frame preparation. Alias
finder storage with constructor workspaces and caller output and require
invalid-argument without input consumption.

Query the public C encoder for a match-bearing twelve-byte frame and require
secondary capacity beyond dictionary and frame staging; reject a one-byte-
short secondary region, then encode through the optimized path and decode
through the unchanged public path to the exact input. Exercise the public CLI
and benchmark with aggregate limits containing the finder worst case. Run the
public README benchmark for ten iterations under MSVC and ClangCL without a
timing threshold. Run all 2,858 registered tests, including schema
compatibility, under both compilers with the 240-second per-test limit.

### TVG-0664

For HashChain phase closure, require the complete branch to pass all 2,858
registered tests under MSVC and ClangCL, including documentation layout and
the schema-37-through-1 compatibility chain. Confirm that match-finder
differential tests cover empty, one-byte, all-byte, repetitive, mixed, and
4,096-byte deterministic pseudorandom input plus window sizes 1, 5, 17, 256,
and 65,536 and maximum match lengths 5, 17, and 258.

Rebuild all eleven public LZSS stream fuzz targets with ClangCL 22
AddressSanitizer, UndefinedBehaviorSanitizer, and libFuzzer. Add the Clang
sanitizer runtime and binary directories to the process-local PATH, then run
each target for 100 bounded iterations with maximum input length 8,192.
Require every target to exit normally without a sanitizer finding, crash, or
hang.

### TVG-0665

For the 1 MiB contextual-window reservation, construct typed-token and modeled-
operation vectors at distances 65,535, 65,536, 65,537, 131,071, 131,072,
1,048,575, and 1,048,576. Require class 16 at distances 65,536 through
131,071, class 17 with zero extra value at 131,072, and class 20 with zero
extra value at 1,048,576. Verify exact LSB-first bypass widths and reject
distance 1,048,577, a reference beyond the accepted frame prefix, every
variant-1 distance above 65,536, classes 17 through 20 under context variant
1, and crossed dictionary/context variants.

Create an input whose only beneficial repeated phrase is more than 65,536
bytes behind the current position. Require Exhaustive and HashChain Exact to
produce the same variant-3 token sequence and require variant 2 to produce no
such Match. Repeat at the 1 MiB boundary, final short frames, one-byte process
buffers, and one-byte-short or aliased workspaces.

For every admitted entropy backend, add a hand-checkable serialized vector,
old-variant byte-identity regressions, exact descriptor and workspace boundary
tests, malformed class/alphabet/bypass/count/terminal/padding/truncation tests,
bounded complete-frame and streaming fuzzing, and same-input ratio, throughput,
and peak-workspace measurements. Do not add a public profile or
interoperability archive until its decoder and encoder meet the full completion
criteria.

### TVG-0666

For shared layout implementation, require the selector to accept exactly
`2/2 + 1/1` and `2/3 + 1/2`, report 4,518/4,550 frequency entries,
17/21 distance alphabets, 16/20 maximum bypass bits, and 26/30 maximum
decisions per token, and reject unknown dictionary IDs, unknown context-model
IDs, unsupported context variants, and both crossed known pairs.

Require the old public constants to equal variant-1 arrays exactly. Check
distance classes at 65,535, 65,536, 65,537, 131,071, 131,072, 1,048,575,
and 1,048,576. Validate variant-3 parameters at the 1 MiB ceiling and reject
the next byte and unknown typed variants. Materialize and invert a variant-2
context sequence whose Match at distance 131,072 produces alphabet 21,
class 17, and 17 zero-valued bypass bits. Require the same configuration to
remain invalid under the frozen variant-1 path.

### TVG-0667

For the first Contextual Dynamic Range decoder slice, derive a variant-3/2
stream header from the frozen one-byte `A` vector by changing only dictionary
variant to 3, frame and window size to 1,048,576, and context variant to 2.
Require exact parse/serialize identity and require both crossed known pairs to
fail atomically. Retain the existing 86-byte literal frame body because none of
its decisions uses an expanded distance context; decode it through both the
complete-frame and one-byte streaming paths and require exactly `A`.

Exercise the selected resource differences independently: require 4,550 table
entries where 4,549 fails, admit 27 decisions for one token only under the new
layout, decode a Symbol decision in distance context 23 with alphabet 21, and
decode 20 bypass bits only under context variant 2. The old variant must keep
rejecting the 20-bit operation and the 1 MiB typed-token parameters. Preserve
all existing old-header serialization vectors byte for byte.

### TVG-0668

For the first Contextual Dynamic Range encoder slice, encode the operation
pair `(Symbol, context 23, alphabet 21, value 20)` and `(Bypass, 20 bits,
0xABCDE)` under context variant 2, then decode both values exactly. Require the
same Symbol operation to fail under the old default layout and require a
4,549-entry table limit to reject the new encoder.

Construct 65,542 raw bytes as five zero bytes, 65,532 nonzero cycling bytes,
and the same five zero bytes. With a 1 MiB window this places the only five-
zero candidate at distance 65,537. Require exact HashChain typed-token and
complete-frame encoding to emit that distance with length five, then decode
the serialized frame to the original bytes. Require the old default typed
variant to reject the 1 MiB parameters. Separately encode raw `A` through the
new complete-frame and one-byte streaming paths: its 86-byte frame remains the
frozen literal frame, while its 112-byte stream header changes only the
reserved dictionary/context identity and 1 MiB frame/window fields.

### TVG-0669

For the internal 1 MiB profile, request original size, frame size, and window
size 1,048,576 with the explicit extended enum. Require stream identity
dictionary variant 3 and context variant 2, frame input 1,048,576 bytes,
1,048,576 token slots, 2,097,152 operation slots, and serialized-frame ceiling
12,582,997 bytes. Independently calculate the exact HashChain workspace and
require the profile's aligned view extent and total aggregate to include it
and remain within the default internal-buffer limit.

Set the entropy-table limit to 4,549 entries. Require the extended decoder
workspace query to fail while the old 4,518-entry query succeeds. Reject an
unknown profile enum without publishing stream or workspace results. Retain
all old default profile values and partition tests unchanged.

### TVG-0670

Initialize the public Contextual Dynamic Range C encode configuration and
require profile value 0. Select value 1 with a 1,048,576-byte window and LZ
distance limit, then require the emitted Format 2 header to contain dictionary
variant 3 and context variant 2. Decode that stream with profile 1 and require
the original bytes exactly; decode the same bytes with profile 0 and require a
stable malformed-stream error before any output.

Require 4,549 entropy-table entries to reject the extended query, then admit
it with the ordinary limit. Reject profile value 2 and a nonzero trailing
reserved word. Preserve the existing profile-0 C lifecycle, header identity,
workspace values, and round trip unchanged.

### TVG-0671

Run the common CLI transaction test once with
`lzss-contextual-dynamic-range-1m`. Require dictionary variant byte 3 and
context variant byte 2 in the encoded header, exact nonempty and empty round
trips, overwrite refusal, malformed and trailing-data rejection, and removal
of temporary output after every error. Decode the same stream through the
64 KiB selector and require rejection without an output file.

Repeat the identity and cross-profile rejection checks in the existing 64 KiB
CLI test: require dictionary variant 2 and context variant 1, then require the
1 MiB selector to reject that stream. Neither selector changes its counterpart
or auto-detects the stream identity.

### TVG-0672

Run one Release iteration of
`marc_benchmark lzss-contextual-dynamic-range-1m README.md 1` under each local
compiler. Require successful public encode and decode, byte-exact untimed
verification, nonzero encoded extent, finite ratio and throughput fields, all
three encoder and decoder workspace extents, and peak caller reservation.

Register the command as an experimental benchmark smoke rather than a stable
matrix member. Retain the 64 KiB smoke unchanged so users can run both commands
over the same larger corpus; do not set size, speed, ratio, or relative-
improvement pass thresholds from the small README input.

### TVG-0673

Generate the five-byte canonical Contextual Dynamic Range stream once for
profile 0 and once for profile 1. For every strict prefix of the extended
stream, require the private frame destination and public output to remain
unchanged. Feed each complete stream to the opposite public profile and
require a stable malformed-stream error with zero output. Set the final
descriptor reserved byte in the extended stream and require the same atomic
failure on both decoder boundaries.

Compile the fixed harness warning-clean under ordinary MSVC and ClangCL. Build
the sanitizer target with Windows Clang 22, then run exactly 1,000 generated
inputs with seed 12345, maximum length 8,192, five-second per-input timeout,
and 512 MiB RSS limit. Do not supply or save a corpus; record any finding as a
new minimized permanent regression before proceeding.

### TVG-0674

Generate schema 38 from the unchanged deterministic 8,193-byte fixture and
the exact schema-37 archive order, then append one
`lzss-contextual-dynamic-range-1m` archive as entry 48. Require an immediate
round trip, `schema_version=38`, `codec_set=marc-cli-v38`, full source
revision, exact manifest order, unique leaf names, size/SHA-256 agreement,
foreign decode equality, and byte-identical local re-encoding.

Swap the first two schema-38 manifest entries and require rejection. Remove
only entry 48 to reconstruct schema 37, then verify the existing schema-37-to-
1 compatibility chain including the canonical-rANS name conversion. Confirm
the archive carries dictionary/context variants `3/2`; do not claim the common
fixture exercises a distance greater than 65,536. Run the complete registered
suite under MSVC and ClangCL before local admission.

### TVG-0675

At pushed revision `363a385168fcfab27adfc8eea3e302129cf01b15`, verify the
Windows/MSVC and Ubuntu 24.04/Ninja schema-38 CI bundles with the Ubuntu 26.04
Clang 21.1.8 CLI. Generate a schema-38 Ubuntu 26.04 bundle from the same
revision, verify it locally, then verify it with the Windows/MSVC CLI.

Require all four final lines to report exactly 48 archives, the expected
producer label, and the identical full revision. Each verifier pass covers
manifest identity and order, size and SHA-256, fixture decoding, and
byte-identical local re-encoding. Record only the results; do not import any
bundle or generated archive into the repository.

### TVG-0676

Run the existing one-Literal and every-dense canonical Contextual rANS
descriptor vectors explicitly with field-context variant 1 and require their
bytes to remain unchanged, including `frequency_entry_count=4,518` and the
9,025-byte all-dense maximum. Set any of the 32 unused backing entries and
require variant-1 validation and serialization to reject without publication.

Construct the corresponding variant-2 one-Literal descriptor with serialized
`frequency_entry_count=4,550`; require parse/serialize identity only when
variant 2 is selected. Fill every selected alphabet densely and require the
exact 9,089-byte maximum, including 21-symbol distance records. Parse each
descriptor under the other variant and require atomic
`invalid_frequency_entry_count`. Exercise one-byte-short output, strict
prefixes, trailing data, unsupported variants, and descriptor/table/buffer
limits. The old default overload, if retained internally during migration,
must select variant 1 only.

### TVG-0677

Run every frozen variant-1 Contextual rANS encoder, decoder, and decode-table
vector through the default internal route and require identical descriptor and
payload bytes. Then construct a variant-2 operation sequence containing a
distance-context symbol with alphabet 21 and value 20 plus a 20-bit bypass
value. Require planning, model normalization, reverse writing, descriptor
serialization, table construction, forward decoding, and terminal checks to
round trip exactly with `frequency_entry_count=4,550`.

Require the same operation sequence to fail under variant 1 before publishing
descriptor or payload. Parse or build the variant-2 model under variant 1 and
the variant-1 model under variant 2 and require atomic crossed-layout
rejection. Exercise unsupported variants, alphabet 17 versus 21, symbol 20,
bypass widths 16, 17, 20, and 21, one-entry-short decode-table output, inactive
contexts, decision budgets, and reuse after failure. Confirm the required
decode-table extent remains exactly 126,976 entries and that output storage,
views, descriptors, payload bytes, and decoded values remain unchanged on
pre-publication failures.

### TVG-0678

Construct a variant-2 typed-token frame with 131,072 Literal `A` tokens
followed by one Match of distance 131,072 and length 5. Use a 1 MiB LZSS
window. Require the selected field model to produce distance class 17 in an
alphabet of 21 plus a 17-bit bypass, then require the direct Contextual rANS
token planner and encoder to match the materialized-operation reference in
event count, decision count, descriptor, payload size, and payload bytes.

Serialize the selected descriptor, validate and decode it back to the exact
typed tokens, and require terminal event, decision, payload, token, and raw-
size counts. Require `frequency_entry_count=4,550` and exact descriptor parse
under variant 2 only. Run the existing frozen literal and mixed-token vectors
through the default route unchanged. Reject the 1 MiB parameter/token frame,
its selected descriptor, unsupported variants, crossed selections, inadequate
table/token/payload capacity, aliasing, invalid declared counts, and malformed
distance reconstruction atomically. Keep all outer stream paths unavailable.

### TVG-0679

Serialize and parse the Contextual rANS stream header with the reserved
`2/3 + 1/2 + 4/3` identity, a 1,048,576-byte window, and matching entropy
entry count 4,550. Require exact round trip, transactional output, and atomic
rejection of crossed known dictionary/context pairs, unsupported identities,
the wrong frequency-entry count, and limits below the selected dictionary or
descriptor/table requirements. Re-run the canonical `2/2 + 1/1 + 4/3`
header and documented one-Literal frame unchanged.

Construct a deterministic raw frame whose exact HashChain parse contains a
Match distance above 65,536 under dictionary variant 3. Plan and encode the
complete frame with the selected 1 MiB identity; require descriptor validation
under context variant 2, complete preflight, decode-table construction, typed-
token reconstruction, and exact raw round trip. Inspect the staged tokens to
prove that the frame exercised the extended distance. Present the same frame
under the 64 KiB identity and require failure before raw output publication.

Exercise both exhaustive and HashChain complete-frame paths at ordinary
sizes, selected maximum decision and descriptor bounds, one-byte-short
descriptor/table/token/raw/output workspaces, aggregate workspace limits,
aliasing, malformed headers and descriptors, crossed layouts, and unsupported
variants. This vector admits only complete-frame internals; streaming,
profiles, public APIs, CLI, benchmarks, fuzzing, and interoperability remain
separate later stages.

### TVG-0680

Query both Contextual rANS profile variants with identical bounded inputs.
Require variant 1 to reproduce its frozen stream identity and every existing
workspace extent. Require variant 2 to emit `2/3 + 1/2 + 4/3`, frequency-entry
count 4,550, a 1,048,576-byte window when configured, and an encoded-frame
ceiling that differs only by the selected descriptor maximum. Decoder queries
must select the same 9,025- or 9,089-byte maximum while retaining exactly
126,976 decode-table entries and checked aligned table/token partitioning.

Drive a variant-2 streaming encoder with a raw frame containing one uniquely
placed repeat at distance above 65,536. Use profile-derived input, token,
HashChain, serialized-frame, decode-table, token, and raw workspaces. Require
bounded multi-call input/output schedules to serialize the selected header,
encode the extended Match, drain deterministically, decode with workspace
sized for variant 2, and reproduce the raw bytes exactly. The private decoder
must select the valid serialized identity independently of which query the
caller used; require crossed dictionary/context header pairs to fail before
raw publication. Public selector policy is deferred.

Reject unsupported profile values, crossed parameters, descriptor and
aggregate limits between the two selected ceilings, one-byte-short and
misaligned workspaces, insufficient frame/token/table/raw capacity, malformed
selected headers, trailing input, and sticky lifecycle misuse. Public C, CLI,
benchmark, fuzzing, and interoperability remain unavailable in this stage.

### TVG-0681

Initialize the public Contextual rANS configuration in both directions and
require `window_profile=0`, the existing structure size, ABI version, limits,
and 64 KiB defaults. Select value 1 with matching 1 MiB frame/window/LZ limits;
require encoder and decoder queries to use the selected 9,089-byte frame
ceiling while retaining the fixed table count and exact aligned views layout.
Reject value 2, nonzero trailing reserved data, incompatible parameters, and
one-below selected limits without publishing non-header workspace extents or
a transform.

Create the value-1 encoder exclusively from queried caller-owned workspaces,
encode a deterministic input, and require stream identities `2/3 + 1/2 +
4/3` with frequency-entry count 4,550. Decode through a value-1 public factory
and require exact bytes. Feed the same stream to a value-0 decoder and require
`MARC_STATUS_MALFORMED_STREAM` before output publication; symmetrically reject
a frozen value-0 stream through value 1.

Repeat short primary, secondary, and views workspaces, misalignment, every
workspace overlap, null output handle, invalid direction, one-byte schedules,
sticky terminal calls, and malformed final-frame atomicity for the selected
profile. Preserve the existing value-0 requirements and encoded bytes. CLI,
benchmark, fuzzing, and interoperability remain separate stages.

### TVG-0682

Encode deterministic binary input through `lzss-contextual-rans-1m` and
require stream identities `2/3 + 1/2 + 4/3`, then decode through the same
name and reproduce the input exactly. Require the unqualified 64 KiB name to
reject that archive without retaining a destination and require the 1 MiB
name to reject an archive produced by the unqualified name in the same way.

Exercise trailing-byte rejection and the existing command-line transactional
cleanup checks for the new selector. Preserve the unqualified selector's
`2/2 + 1/1 + 4/3` identity and bytes. Do not append an interoperability
archive or alter its schema in this CLI-only stage.

### TVG-0683

Run one Release iteration of
`marc_benchmark lzss-contextual-rans-1m README.md 1` under each local
compiler. Require the checked `112 + 12N + 9,161K` output-capacity formula,
public window-profile value 1 in both directions, successful public encode and
decode, byte-exact untimed verification, nonzero encoded extent, finite ratio
and throughput fields, all three encoder and decoder workspace extents, and
peak caller-owned reservation.

Register the command as an experimental benchmark smoke. Run the existing
64 KiB benchmark independently and preserve its name, configuration, and
`112 + 12N + 9,097K` capacity. Treat measured ratio and speed only as
descriptive evidence; do not alter the interoperability schema in this stage.

### TVG-0684

Parameterize the Contextual rANS malformed regression over public window
profiles 0 and 1. For each profile, generate a canonical stream, truncate it
at every byte, corrupt extreme frame lengths and descriptor flags, and require
private/public atomic failure without raw publication. Feed each canonical
stream to the other strict public profile and require malformed-stream failure,
zero output, unchanged sentinel bytes, and sticky category/positions.

Compile the single harness warning-clean under MSVC and ClangCL. Its fixed
thread-local storage must use the 9,089-byte descriptor capacity while keeping
input at 32 KiB, output at 4 KiB, frame/token storage at 1 KiB, 6,144
decisions, 12,296 payload bytes, and 126,976 decode entries. Build the existing
sanitizer target and run a bounded campaign without a persistent corpus using
`-runs=1000 -max_len=32768 -timeout=5 -rss_limit_mb=512`. Do not add a second
target or an interoperability archive.

### TVG-0685

Generate schema 39 from the unchanged deterministic 8,193-byte fixture and
the exact schema-38 archive order, then append
`lzss-contextual-rans-1m.marc` as entry 49. Require generation-time header
bytes 3/2, immediate local round trip, `schema_version=39`, codec set
`marc-cli-v39`, full revision, exact order, unique leaf names/codecs, size and
SHA-256 agreement, fixture decode, and byte-identical local re-encoding.

Swap the first two schema-39 records and require order rejection. Remove only
entry 49 to reconstruct schema 38, verify it, and continue the complete frozen
schema-38-through-1 chain including the historical Contextual rANS rename.
The shared fixture establishes identity rather than an extended-distance
Match; do not import generated bundles into the repository.

After push and successful CI, verify the Windows/MSVC and Ubuntu 24.04/Ninja
schema-39 artifacts with Ubuntu 26.04/Clang. Generate and self-verify an Ubuntu
26.04 schema-39 bundle from the same revision, then verify it with the
Windows/MSVC executable. Require all four final lines to report 49 archives,
the expected producer label, and one identical full revision before recording
external evidence.

### TVG-0686

Preserve the existing one-literal Contextual tANS descriptor byte for byte
under the default 64 KiB layout. Parse, validate, and reserialize it only when
the caller selects field-context variant 1; require serialized frequency-entry
count 4,518, the exact canonical extent, and a zero unused 32-entry tail.

Construct a variant-2 descriptor with frequency-entry count 4,550 and an
active distance context using symbol class 20. Require canonical compact-model
serialization, exact parse/serialize inversion, selected alphabet 21, and a
descriptor extent no greater than 9,093 bytes. Exercise the all-dense maximum
to establish that exact ceiling independently of frame data.

For each layout, replace the entry count with the other layout's value, pass
an unsupported context variant, set one unused variant-1 tail entry, truncate
every descriptor byte, append trailing bytes, corrupt the active mask and
record mode, and provide output one byte short. Require a stable format error,
unchanged destination descriptor or serialized sentinel bytes, checked size
arithmetic, and no table, frame, or raw-output publication. Table construction
and entropy inversion remain later vectors.

### TVG-0687

Run one selected-layout Contextual tANS operation sequence containing a
distance-context Symbol request with alphabet 21 and value 20 followed by a
20-bit bypass value. Require the variant-2 model builder to publish frequency-
entry count 4,550, normalize only the selected 4,550-entry bank, build exactly
131,072 encode and decode transitions, invert the operations through the
selected state coder, consume the exact valid-bit extent, and finish at the
canonical terminal state.

Run the existing one-literal and Symbol-plus-bypass vectors through the default
variant-1 route and require byte-identical descriptors, payloads, tables, and
terminal results. Variant 1 must reject alphabet 21, class 20, and bypass width
20; variant 2 must reject the old alphabet when the selected context requires
21. Both variants reject unknown selections, inactive contexts, crossed
descriptor counts, and nonzero unused fields without publishing model,
transition, payload, decoded value, or terminal state.

Exercise one-entry-short encode/decode table spans, descriptor/table aliasing,
duplicate planning, count overflow, truncated additional bits, nonzero final
padding, trailing bits, unused active contexts, and invalid initial/final
states under the selected route. The transition extent remains fixed because
the wider alphabet changes frequency partitioning inside each context table,
not the number or size of table regions. Typed-token and frame integration are
excluded from this vector.

### TVG-0688

Construct an extended typed-token sequence without allocating one million
Literal tokens: emit one Literal, use bounded distance-1 Matches to grow the
raw prefix deterministically to exactly 1,048,576 bytes, then append a Match
of length 5 at distance 1,048,576. The final Match must produce distance class
20 and a 20-bit LSB-first bypass field. Use 1,048,576-byte LZSS parameters,
typed-token variant 3, and field-context variant 2.

Materialize the selected field-operation sequence independently, then plan and
encode it through the operation-level Contextual tANS API. Plan and encode the
same typed tokens through the direct adapter and require identical event and
decision counts, descriptor frequencies and metadata, fixed 131,072-entry
tables, payload extent, valid-bit count, payload bytes, and terminal state.
Decode through the selected direct adapter and require exact typed-token
identity, reconstructed raw extent 1,048,581, and no publication before the
validate-only pass succeeds.

Run the frozen one-Literal and existing Match vectors through the default
variant-1 adapter and require byte identity. Feed the extended sequence and
its 1 MiB parameters to the default adapter, cross descriptor counts and
alphabets between layouts, and pass an unsupported selection; require stable
parameter, token-validation, or entropy errors with unchanged descriptor,
payload, table sentinel where promised, and token output. Repeat one-entry-
short table, one-token-short output, payload/table/token alias, truncation,
padding, count, and reconstructed-distance failures under variant 2. Complete
frame and streaming admission remain later vectors.

### TVG-0689

Serialize and parse a Contextual tANS stream header with the reserved
`2/3 + 1/2 + 5/2` identity, 1,048,576-byte frame and window limits, and
frequency-entry count 4,550. Require exact round trip and transactional output.
Reject crossed known dictionary/context pairs, unknown variants, the wrong
frequency-entry count, nonzero reserved bytes, and limits below the selected
dictionary, descriptor, payload, or fixed 131,072-entry table requirement.
Re-run the documented `2/2 + 1/1 + 5/2` header and one-Literal frame byte for
byte.

Construct deterministic raw input whose exact HashChain parse contains a
Match distance above 65,536 under dictionary variant 3. Plan and encode the
complete frame with the selected 1 MiB identity. Require its staged tokens to
prove the extended distance, its descriptor to use selected entry count 4,550,
and complete preflight and decoding to select variant 2 before table building,
typed-token reconstruction, and exact raw round trip. The same bytes under the
64 KiB identity, a crossed descriptor count, or a crossed alphabet must fail
without raw publication.

Run Exhaustive and HashChain complete-frame paths at ordinary sizes for both
variants. Exercise selected 26/30 decision bounds, exact 9,029/9,093 descriptor
ceilings, the common payload formula, one-byte-short token/table/raw/output
workspaces, pairwise aliasing, aggregate-memory limits, truncation, padding,
initial/final state, contradictory frame counts, unsupported selections, and
plan/write disagreement. Planning may mutate only its documented token, table,
and HashChain workspaces; serialized output remains untouched until the
complete plan succeeds. Streaming, profiles, public C, CLI, benchmarks,
fuzzing, and interoperability remain later stages.

### TVG-0690

Construct 64 KiB and 1 MiB Contextual tANS profiles from the same bounded
configuration. Require exact stream identities `2/2 + 1/1 + 5/2` and
`2/3 + 1/2 + 5/2`, frequency-entry counts 4,518 and 4,550, fixed 131,072-entry
encode/decode tables, selected descriptor ceilings 9,029 and 9,093, and
serialized-frame ceilings `9F + 9,095` and `9F + 9,159`. Partition every
caller-owned typed view and require exact offsets, alignment, and HashChain
extent. Reject unknown profile values, crossed dictionary parameters,
one-byte-short or misaligned storage, arithmetic overflow, and each aggregate
limit without publishing partial requirements or views.

Use the selected requirements to construct streaming encoder and decoder
instances. Round trip deterministic raw input whose exact HashChain parse
contains a Match distance above 65,536, varying input and output down to one
byte and retaining EndInput across every drain. Require an empty selected
stream and a final short frame to terminate exactly. Run the frozen 64 KiB
oracle unchanged.

Parse both supported stream identities under decoder admissions `any`,
`field_context_64k`, and `field_context_1m`. `any` accepts both; each explicit
admission accepts only its matching pair and rejects the other before frame
collection or raw publication. Exercise crossed identities, truncation,
trailing bytes, unsupported flags, workspace aliases, table/token/raw
shortage, and sticky errors. Public C, CLI, benchmark, fuzz, and
interoperability admission remain later vectors.

### TVG-0691

Compile the public C header as C11 and require
`sizeof(marc_lzss_contextual_tans_config) == 112`. Initialize both directions
and require ABI version 1, the current structure size, 64 KiB frame/window
defaults, `window_profile=0`, and zero reserved fields. Run the established
64 KiB fixture and require byte-identical `2/2 + 1/1 + 5/2` output and exact
workspace counts before and after the tail-field reinterpretation.

Set a 1 MiB window, compatible hard limits, and profile value 1. Query and
partition all three workspaces, create the encoder, and require exact
`2/3 + 1/2 + 5/2` header fields including frequency-entry count 4,550. Query
the matching decoder profile and require exact round trip. Decode that stream
with profile 0 and decode the baseline stream with profile 1; both must report
malformed stream before publishing raw bytes. Unknown profile values, nonzero
reserved fields, wrong structure size or ABI version, invalid direction,
short, aliased, and misaligned workspaces must fail without a transform.

Verify the static and shared libraries expose the same three existing symbol
names and behavior. No new exported function is introduced. CLI, benchmark,
fuzzing, and interoperability admission remain later vectors.

### TVG-0692

Invoke CLI encode and decode with `lzss-contextual-tans` and require exact
dictionary/context/entropy identity `2/2 + 1/1 + 5/2`, unchanged round trip,
and strict trailing-data rejection. Invoke both operations with the new
`lzss-contextual-tans-1m` name and require `2/3 + 1/2 + 5/2`, exact round trip,
and the same strict trailing-data behavior.

Attempt to decode the 64 KiB archive with the 1 MiB name and the 1 MiB archive
with the 64 KiB name. Both commands must fail, consume no trusted raw result,
and leave a pre-existing destination sentinel unchanged. Require the help
inventory to list each name once in order. Unknown near-miss names remain
usage errors. Benchmark, fuzzing, and interoperability publication are later
vectors.

### TVG-0693

Run the experimental benchmark smoke with `lzss-contextual-tans` and
`lzss-contextual-tans-1m` over the same nonempty repository input and one
iteration. Each command must complete its untimed public C encode/decode
verification before reporting a sample. Require the selected name in help and
reject unknown near-miss names through the existing usage path.

For both names require finite encoded ratio and encode/decode throughput,
nonzero direction-specific workspace extents, and a peak equal to the larger
reported primary-plus-secondary-plus-views aggregate. The selected capacity
planner must use 1 MiB frames and `112 + 9N + 9,159K`, while the frozen route
retains `112 + 9N + 9,095K`. Measurements are descriptive and impose no speed
or ratio threshold. Fuzzing and interoperability publication remain later
vectors.

### TVG-0694

Parameterize the Contextual tANS malformed regression over public window
profiles 0 and 1. For each profile, generate a canonical stream, truncate it
at every byte, corrupt extreme frame lengths and descriptor padding, and
require private/public atomic failure without raw publication. Feed each
canonical stream to the other strict public profile and require malformed-
stream failure, zero output, unchanged sentinel bytes, and sticky category
and positions.

Compile the single harness warning-clean under MSVC and ClangCL. Its fixed
thread-local storage must use the 9,093-byte descriptor capacity while keeping
input at 32 KiB, output at 4 KiB, frame/token storage at 1 KiB, 6,144
decisions, 9,218 payload bytes, and 131,072 decode entries. Build the existing
sanitizer target and run a bounded campaign without a persistent corpus using
`-runs=1000 -max_len=32768 -timeout=5 -rss_limit_mb=512`. Do not add a second
target or an interoperability archive.

### TVG-0695

Generate schema 40 from the unchanged deterministic 8,193-byte fixture and
the exact schema-39 archive order, then append
`lzss-contextual-tans-1m.marc` as entry 50. Require generation-time header
identities `2/3 + 1/2 + 5/2`, immediate local round trip,
`schema_version=40`, codec set `marc-cli-v40`, full revision, exact order,
unique leaf names/codecs, size and SHA-256 agreement, fixture decode, and
byte-identical local re-encoding.

Swap the first two schema-40 records and require exact-order rejection. Remove
only entry 50 to reconstruct schema 39, verify it, and continue the complete
frozen schema-39-through-1 chain including the historical Contextual rANS
rename. The shared fixture establishes identity rather than an extended-
distance Match; do not import generated bundles into the repository.

After push and successful CI, verify the Windows/MSVC and Ubuntu 24.04/Ninja
schema-40 artifacts with Ubuntu 26.04/Clang. Generate and self-verify an
Ubuntu 26.04 schema-40 bundle from the same revision, then verify it with the
Windows/MSVC executable. Require all four final lines to report 50 archives,
the expected producer label, and one identical full revision before recording
external evidence.

### TVG-0696

Retain every existing variant-1 descriptor byte and build selected-layout
descriptor vectors without using descriptor length as the selector. For
variant 2, require field alphabets `2, 256, 8, 21`, the 4,550-entry context
layout, minimum size 24, and an independently counted all-dense size of 2,579
bytes. Exercise Single, canonical sparse, and canonical dense distance records
at symbols 0, 16, 17, 20, plus the sparse/dense crossover. Require the unused
high nibble of every dense 21-symbol record to be zero.

Parse and reserialize each accepted descriptor byte-identically under the
explicit selected layout. Reject the same bytes under the crossed layout,
sizes 2,562 through 2,579 under variant 1, an out-of-alphabet class 17 through
20 in variant 1, a class above 20 in variant 2, noncanonical record choice,
invalid Huffman lengths, truncation at every byte, trailing data, and short
output without publishing a descriptor. Prove the 35-table and 17,885-node
ceilings remain unchanged. No typed-token, frame, streaming, public C, CLI,
benchmark, fuzz, or schema vector belongs to this descriptor stage.

### TVG-0697

Retain every existing Contextual Blocked Huffman variant-1 payload and use an
explicit variant-2 operation sequence containing one Single symbol in each of
the four required pooled fields, including distance class 20, followed by the
20-bit LSB-first value `0xabcde`. The Single models contribute no payload bits,
so the exact payload is `de bc 0a`, the decision count is 24, and the final
valid-bit count is 4. Decode the same symbols and bypass value under variant 2
and finish with exact event, decision, and bit counts.

Require variant 1 to reject the 21-symbol request and 20-bit bypass before
publishing a descriptor or mutating output. Reject unknown variants before
output or table construction. Under a started variant-2 decoder, reject the
crossed 17-symbol request and a 21-bit bypass without changing the caller's
value. Retain existing override-profitability, malformed-code, padding,
trailing-bit, capacity, overlap, lifecycle, and hand-vector tests. No typed-
token, frame, streaming, public, benchmark, fuzz, or schema vector belongs to
this coding-core stage.

### TVG-0698

Construct 131,072 literal `A` tokens followed by a length-5 Match at distance
131,072 under the 1 MiB dictionary/context pair. Require the common field-
context modeler to emit a 21-symbol distance request with class 17 and a
17-bit LSB-first bypass. Compare the direct Contextual Blocked Huffman
descriptor and payload byte-for-byte with the selected ModeledOperation path,
then decode through the two-pass direct adapter and compare every typed token.

Under variant 1, reject the same token sequence during typed-token validation
without changing a sentinel descriptor or payload. Reject an unknown variant
and the selected descriptor under the crossed layout without changing caller
tokens or decode tables. Retain all existing 64 KiB direct-adapter vectors,
workspace/alias checks, malformed entropy checks, and raw/count validation.
No frame, streaming, public, benchmark, fuzz, or schema vector belongs to this
typed-token stage.

### TVG-0699

Retain the exact documented variant-1 stream header and one-literal frame.
Serialize and parse a selected stream carrying dictionary variant 3, context
algorithm 1, context variant 2, a 1 MiB window, and the unchanged Contextual
Blocked Huffman entropy identity 2/2. Reject crossed `2/2` and `3/1` pairs,
unknown selectors, a variant-2 descriptor under variant 1, descriptor sizes
2,562..2,579 under variant 1, and counts above the selected 26- or 30-
decisions-per-token ceiling without publishing a header or layout.

For complete-frame proof, place `ABCDE`, 65,536 filler bytes, and `ABCDE` in
one selected frame. Require HashChain Exact to produce at least one Match with
distance above 65,536, encode and decode the frame, and reconstruct the exact
raw bytes. Require Exhaustive and HashChain Exact to remain byte-identical on
the existing small deterministic vector. Decode the extended frame under a
crossed variant-1 stream and require frame rejection without raw mutation.
Retain every capacity, alias, aggregate-workspace, truncation, descriptor,
payload, token, and reconstruction regression. No streaming, profile, public,
benchmark, fuzz, or interoperability vector belongs to this stage.

### TVG-0700

Construct both profile variants from the same bounded configuration. Require
the legacy profile to retain exact `2/1 + 1/1 + 2/2`, existing workspace
extents, and serialized bytes. Require the selected profile to emit exact
`2/3 + 1/2 + 2/2`, accept a 1 MiB window, reserve the 2,579-byte descriptor
maximum, and increase same-limit decoder encoded-frame capacity by exactly 18
bytes while retaining 35 tables and identical token/raw capacities. Reject
unknown profile variants and crossed dictionary parameters without publishing
stream or workspace requirements.

Drive the selected streaming encoder and decoder with one-byte input and
output over the marker-gap-marker frame whose Match exceeds 64 KiB. Require
exact round trip, stable terminal repetition, and no frame publication before
complete validation. An `any` decoder accepts both variants; exact 64 KiB and
1 MiB policies accept only their paired identities and reject the reciprocal
stream before raw output. Reject unknown admission values at construction.
Retain all existing capacity, alias, EndInput, trailing-input, malformed-final-
frame, and workspace partition regressions. No public, CLI, benchmark, fuzz,
or interoperability vector belongs to this stage.

### TVG-0701

Require C configuration initialization to preserve its ABI-1 extent, zero
reserved fields, and 64 KiB selector default. Query and construct both encode
and decode directions for the 1 MiB selector, checking its exact stream
identity, 18-byte descriptor-capacity delta, and a marker-gap-marker Match
beyond 64 KiB. Drive the transform lifecycle with bounded buffers and require
exact round trip plus stable repeated terminal status.

Construct each exact decoder against the reciprocal stream and require
`MARC_STATUS_MALFORMED_STREAM` with zero raw output. Reject an unknown selector,
nonzero trailing reserved word, a 1 MiB window under the legacy selector,
one-byte-short workspace, misalignment, overlap, invalid size/version tags,
and null factory outputs transactionally. Retain all existing public C
binary-class and malformed-final-frame coverage. No CLI, benchmark, fuzz, or
interoperability vector belongs to this stage.

### TVG-0702

Require CLI usage to list `lzss-contextual-blocked-huffman` immediately before
`lzss-contextual-blocked-huffman-1m`, each exactly once, and reject a `-1M`
case near miss. Encode the common CLI fixture through each exact name. Require
the legacy archive to retain `2/2 + 1/1 + 2/2`, the selected archive to emit
`2/3 + 1/2 + 2/2`, byte-identical repeated encoding, exact decoding, and
strict trailing-data rejection.

Decode each archive through the reciprocal CLI name and require command
failure with no accepted output. Retain every existing CLI exit-status,
missing-path, bad-name, and same-path rejection. The small fixture is an
identity/lifecycle vector only; the earlier marker-gap tests remain the proof
of extended-distance use. No benchmark, fuzz, interoperability, or schema
vector belongs to this stage.

### TVG-0703

Run both exact Contextual Blocked Huffman benchmark names over the same bounded
fixture for one Release iteration. Require a successful untimed public C round
trip before timing, finite ratio and throughput fields, positive directional
workspace regions, and a reported peak equal to the greater complete
directional sum. The selected profile must use checked complete-stream
capacity `112 + 12N + 2,643K` and the 1 MiB public configuration throughout.

Require benchmark usage to list the legacy name immediately before
`lzss-contextual-blocked-huffman-1m`, with the selected name exactly once, and
reject a `-1M` case near miss. Retain the legacy benchmark result and policy.
Do not impose speed or compression-ratio thresholds; the small fixture is a
wiring and accounting smoke, while distant-match use remains proven by the
earlier marker-gap vectors. No fuzz, interoperability, schema, CLI, or format
vector belongs to this stage.

### TVG-0704

Parameterize the Contextual Blocked Huffman malformed regression over public
window profiles 0 and 1. For each profile, generate a canonical stream,
truncate it at every byte, corrupt extreme frame lengths and descriptor flags,
and require private/public atomic failure without raw publication. Feed each
canonical stream to the other strict public profile and require malformed-
stream failure, zero output, unchanged sentinels, and sticky error category
and positions.

Compile the single harness warning-clean under MSVC and ClangCL. Its fixed
thread-local storage must use the selected 2,579-byte descriptor capacity
while keeping input at 32 KiB, output at 4 KiB, frame/token storage at 1 KiB,
6,144 decisions, 11,520 payload bytes, and 35 decode tables. Build the existing
sanitizer target and run a bounded campaign without a persistent corpus using
`-runs=1000 -max_len=32768 -timeout=5 -rss_limit_mb=512`. Do not add a second
target or an interoperability archive.

### TVG-0705

Generate schema 41 from the unchanged deterministic 8,193-byte fixture and
the exact schema-40 archive order, then append
`lzss-contextual-blocked-huffman-1m.marc` as entry 51. Require generation-time
header identities `2/3 + 1/2 + 2/2`, immediate local round trip,
`schema_version=41`, codec set `marc-cli-v41`, full revision, exact order,
unique leaf names/codecs, size and SHA-256 agreement, fixture decode, and
byte-identical local re-encoding.

Swap the first two schema-41 records and require exact-order rejection. Remove
only entry 51 to reconstruct schema 40, verify it, and continue the complete
frozen schema-40-through-1 chain including the historical Contextual rANS
rename. The shared fixture establishes identity rather than an extended-
distance Match; do not import generated bundles into the repository.

After push and successful CI, verify the Windows/MSVC and Ubuntu 24.04/Ninja
schema-41 artifacts with Ubuntu 26.04/Clang. Generate and self-verify an
Ubuntu 26.04 schema-41 bundle from the same revision, then verify it with the
Windows/MSVC executable. Require all four final lines to report 51 archives,
the expected producer label, and one identical full revision before recording
external evidence.

### TVG-0706

Initialize the Contextual Adaptive Huffman model bank with each validated
field-context layout. Require variant 1 to retain exactly 4,518 symbol entries,
9,067 node entries, the frozen 31 alphabets and offsets, successful validation,
and identical reset behavior. Require variant 2 to use exactly 4,550 symbols
and 9,131 nodes, with alphabet 21 for contexts 23 through 30 and unchanged
alphabets elsewhere.

For variant 2, independently shorten the node and symbol workspaces by one and
require the corresponding stable capacity error with no initialized bank.
Reject an unsupported or inconsistent layout atomically. Retain the existing
overlapping-workspace rejection and checked byte-extent arithmetic. Exercise
one newly admitted distance-class symbol in each of the eight widened trees,
reset, and prove the initial NYT state and complete validation are restored.
Retain all existing variant-1 model vectors byte-for-byte. No descriptor,
payload, frame, public, fuzz, or interoperability vector belongs to this
stage.

### TVG-0707

Plan and encode the exact variant-2 operations `Symbol(23,21,20)` and
`BypassBits(20,0xabcde)` with selected 9,131-node and 4,550-symbol workspaces.
Require descriptor decision count 21, payload size 4, final-valid-bit count 1,
and exact payload `D4 9B 57 01`. Decode with the same explicit selection and
require values 20 and `0xabcde`, two events, 21 decisions, 25 consumed bits,
and exact completion.

Require independent fresh workspaces to reproduce the same descriptor and
payload. Variant 1 must reject alphabet 21 and 20-bit bypass requests. Reject
an unsupported selection and independently one-short variant-2 node and symbol
regions without changing the caller's descriptor, payload sentinel, or decoded
value. Retain existing variant-1 hand vectors and operation bytes exactly. No
typed-token, frame, streaming, public, benchmark, fuzz, or interoperability
vector belongs to this stage.

### TVG-0708

Construct a variant-2 typed-token sequence from one literal, 4,064 length-258
distance-1 Matches, one length-63 distance-1 Match, and one length-5 Match at
distance 1,048,576. Require typed-token validation to report exact raw size
1,048,581. Independently model the sequence as field-context operations and
require its final two operations to be `Symbol(23..30,21,20)` for the selected
length context followed by `BypassBits(20,0)`.

Plan and encode those operations with the selected Contextual Adaptive
Huffman operation coder, then plan and encode the tokens directly with 9,131
nodes and 4,550 symbols. Require exact descriptor-field and payload identity.
Decode the direct payload with the same variant and require every token,
event, decision, raw-size, and consumed-bit result to match the independent
reference and original sequence.

Retain every variant-1 token payload byte through the source-level default.
Require the 1 MiB parameters and sequence to fail under variant 1, an
unsupported selection to fail, and independently one-short variant-2 node and
symbol workspaces to fail without changing descriptor, payload sentinels, or
token output. A crossed decoder must validate nothing into the caller's token
region. No complete-frame, streaming, public, benchmark, fuzz, or
interoperability vector belongs to this stage.

### TVG-0709

Serialize and parse both exact Contextual Adaptive Huffman stream identities:
variant 1 `2/2 + 1/1 + 1/2` and variant 2 `2/3 + 1/2 + 1/2`. Require offsets
14, 96, and 98 to retain the selected values and all released variant-1 bytes
to remain unchanged. Reject reciprocal dictionary/context pairs, unsupported
selectors, a 1 MiB dictionary under variant 1, and insufficient selected model
limits without publishing a parsed header.

For variant 2, construct `marker + 65,536 filler bytes + marker`, configure a
1 MiB window, and encode through HashChain Exact. Require a staged Match with
distance above 65,536, exact header identities, a 30-decision token ceiling,
9,131 node and 4,550 symbol requirements, deterministic repeated output, and
complete raw round trip. Present that frame under a canonical variant-1 stream
context and require failure with the raw sentinel unchanged.

Retain all variant-1 hand vectors and exhaustive/HashChain identity. Reject
one-short selected node, symbol, token, raw, match-finder, and serialized
regions before destination publication. Exercise contradictory counts above
26 but within 30 decisions per token only under variant 2. No streaming,
public, CLI, benchmark, fuzz, or interoperability vector belongs to this
stage.

### TVG-0710

Build both private Contextual Adaptive Huffman profiles from the same bounded
configuration. Require variant 1 to preserve exact `2/2 + 1/1 + 1/2`, 9,067
nodes, 4,518 symbols, and every existing workspace extent. Require variant 2
to produce exact `2/3 + 1/2 + 1/2`, 9,131 nodes, and 4,550 symbols while
retaining the same frame-encoded ceiling. Its view storage must increase only
by 64 `AdaptiveHuffmanNode` objects and 32 `uint16_t` symbols, with all offsets
recomputed by the common alignment rules.

Construct variant-2 input as marker, 65,536 filler bytes, and repeated marker.
Encode through the selected profile and streaming encoder with one-byte output
chunks. Require a staged HashChain Match beyond 65,536, exact deterministic
stream bytes, and complete decode with one-byte input and output chunks using
both `any` and exact-1m admission. Repeated calls after completion must retain
EndOfStream.

Feed each canonical empty and nonempty stream to the reciprocal exact decoder
and require malformed-stream failure immediately after its 112-byte header,
zero raw publication, and unchanged output sentinels. Reject an unknown
profile or admission value, a 1 MiB dictionary under the 64 KiB profile,
crossed node/symbol requirements, independently one-short selected node,
symbol, token, finder, serialized-frame, raw-frame, and output regions, and
every existing overlap or misalignment. Retain Flush, ResetBlock, premature
EndInput, trailing-input, aggregate-limit, and malformed-final-frame coverage.
No public, CLI, benchmark, fuzz, or interoperability vector belongs to this
stage.

### TVG-0711

Require C configuration initialization to preserve its ABI-1 extent, zero
both reserved words, and select `MARC_LZSS_CONTEXTUAL_WINDOW_64K`. Query and
construct both encode and decode directions for
`MARC_LZSS_CONTEXTUAL_WINDOW_1M`, checking exact stream identity
`2/3 + 1/2 + 1/2`, exact selected model workspace, and a marker-gap-marker
Match beyond 64 KiB. Drive both public transforms with bounded input and
output buffers and require exact deterministic round trip plus stable repeated
terminal status.

Construct each exact public decoder against the reciprocal stream and require
`MARC_STATUS_MALFORMED_STREAM` immediately after the complete stream header
with zero raw output. Reject an unknown selector, nonzero trailing reserved
word, a 1 MiB window under the legacy selector, independently one-byte-short
workspace, view misalignment, every workspace overlap, invalid structure size
or ABI version, and null factory output transactionally. Retain all existing
public C binary-class, chunking, capacity, and malformed-final-frame coverage.
No CLI, benchmark, fuzz, or interoperability vector belongs to this stage.

### TVG-0712

Require CLI usage to list `lzss-contextual-adaptive-huffman` immediately
before `lzss-contextual-adaptive-huffman-1m`, each exactly once, and reject a
`-1M` case near miss. Encode the common CLI fixture through each exact name.
Require the legacy archive to retain `2/2 + 1/1 + 1/2`, the selected archive
to emit `2/3 + 1/2 + 1/2`, byte-identical repeated encoding, exact decoding,
and strict trailing-data rejection.

Decode each archive through the reciprocal CLI name and require command
failure without accepted output. Require the selected route to use the 1 MiB
public selector, frame/window and distance policy, 13,681-entry model ceiling,
`ceil(267F/8)` payload bound, checked complete-stream capacity, and 128 MiB
aggregate limit. Retain all existing CLI exit-status, missing-path, bad-name,
and same-path rejection. No benchmark, fuzz, interoperability, or schema
vector belongs to this stage.

### TVG-0713

Run both exact Contextual Adaptive Huffman benchmark names over the same
bounded fixture for one Release iteration. Require a successful untimed public
C round trip before timing, finite ratio and throughput fields, positive
directional workspace regions, and a reported peak equal to the greater
complete directional sum. The selected profile must use checked complete-
stream capacity `112 + ceil(267N/8) + 80K`, exact 1 MiB public configuration,
and 13,681-entry model ceiling throughout.

Require benchmark usage to list the legacy name immediately before
`lzss-contextual-adaptive-huffman-1m`, with the selected name exactly once,
and reject a `-1M` case near miss. Retain the legacy benchmark result and
policy. Do not impose speed or compression-ratio thresholds; the small fixture
is a wiring and accounting smoke, while distant-match use remains proven by
the earlier marker-gap vectors. No fuzz, interoperability, schema, CLI, or
format vector belongs to this stage.

### TVG-0714

Parameterize the Contextual Adaptive Huffman malformed regression over public
window profiles 0 and 1. For each profile, generate a canonical stream,
truncate it at every byte, mutate stream identity and reserved bytes, corrupt
extreme frame lengths, descriptor fields, and payload padding, and require
private/public atomic failure without raw publication. Feed each canonical
stream to the other strict public profile and require malformed-stream
failure, zero output, unchanged sentinels, and sticky error category and
positions.

Compile the single harness warning-clean under MSVC and ClangCL. Its fixed
thread-local storage must use the selected 9,131-node and 4,550-symbol maxima
while keeping input at 65,536 bytes, output at 4 KiB, raw-frame capacity at
1,024 bytes, typed-token capacity at 1,024 entries, payload at 34,176 bytes,
the descriptor at 16 bytes, and the process-call budget finite. Build the
existing sanitizer target and run a bounded
campaign without a persistent corpus using
`-runs=1000 -max_len=65536 -timeout=5 -rss_limit_mb=512`. Do not add a second
target or an interoperability archive.

### TVG-0715

Generate schema 42 from the unchanged deterministic 8,193-byte fixture and
the exact schema-41 archive order, then append one
`lzss-contextual-adaptive-huffman-1m` archive as entry 52. Require an immediate
round trip, `schema_version=42`, `codec_set=marc-cli-v42`, a full source
revision, exact manifest order, unique leaf-only names, size/SHA-256 agreement,
foreign decode equality, and byte-identical local re-encoding.

Inspect dictionary, context, and entropy identities as exact `2/3 + 1/2 +
1/2`. Swap the first two schema-42 entries and require verifier rejection.
Remove only entry 52 to reconstruct schema 41, then verify its unchanged
compatibility chain through schema 1. Do not claim the common fixture exercises
a distance greater than 65,536. Run the complete registered Release suite,
including documentation layout and schema compatibility, under MSVC and
ClangCL before local admission.

### TVG-0716

Exercise the offline Corpus verifier with temporary small files and a supplied
manifest, never with a downloaded test dependency. Require exact regular
files to succeed in manifest order and return the expected SHA-256. Require a
missing plus unexpected entry to be reported together, wrong size to fail
before content hashing, same-size wrong MD5 to fail, a directory in place of
a member to fail, a missing root to fail, and duplicate manifest names to be
rejected as a programmer error. Invoke the CLI on an invalid directory and
require nonzero return, diagnostic standard error, and empty standard output
so no partial success manifest is published.

Separately run the production manifest against the owner-supplied local
Silesia directory and require all twelve names, published sizes, and MD5
values plus the exact 211,938,580-byte total. This local data validation is
developer evidence, not a persistent CTest fixture or redistributed vector.

### TVG-0717

Retain the existing one-shot README smoke unchanged. Invoke the new frame mode
on that same repository-owned input with 1,024-byte frames, one iteration, and
a 65,536-byte window. Calculate the expected frame count from the current file
size and require exact mode, strategy, input, configuration, frame, and
iteration fields plus positive token, workspace, query, candidate, comparison,
seconds, and throughput fields.

Require omitted optional arguments to select 1/1,048,576/65,536, an unknown
strategy to return usage failure, and zero frame size to fail. Generate an
empty file only in the build tree and require successful zero-byte, zero-frame,
zero-token, and zero-query reporting. Separately process the locally supplied
`dickens` member with 1 MiB frames and both 64 KiB and 1 MiB windows to prove
that a 10,192,446-byte input is traversed as ten bounded frames. Do not add
Corpus data or Corpus-dependent CTest cases.

### TVG-0718

For the hand-checkable `ABCDEABCDE` finder fixture, require four HashChain
candidates to partition into one complete five-byte prefix match and three
prefix mismatches. Require ten queries to occupy logarithmic depth bins as
seven zero-candidate, two one-candidate, and one two-to-three-candidate query,
with maximum depth two and no overflow. Preload a counter and its zero-depth
bin to `uint64` maximum, execute one valid query, and require saturation plus
the explicit overflow flag.

Extend the frame-runner smoke to require positive classification, extension,
maximum-depth, and histogram fields, and require prefix matches plus mismatches
to equal total candidates. Empty input must report zero classifications,
maximum depth zero, and the single zero-valued histogram bin. Separately use
the owner-supplied `dickens` member to distinguish the collision and genuine
equal-prefix populations at 64 KiB and one MiB windows; do not make the Corpus
a CTest dependency.

### TVG-0719

Before implementing BinaryTree Exact, fix the required test families from
DD-845. Use hand-authored insertion sequences for every AVL single and double
rotation; explicit root, leaf, one-child, and two-child retirement; distance
exactly at and one beyond the window; skipped positions after a match; capped
suffix duplicates requiring the newest position; maximum-LCP candidates on
both lexicographic sides; and an all-`0xff` prefix with no finite upper bound.

For small deterministic binary inputs, compare every query position against
Exhaustive and HashChain Exact before comparing typed tokens, serialized
tokens, and complete pipeline bytes. Generate pseudorandom and structured
zero/periodic/equal-prefix cases locally from documented fixed seeds; do not
import external vectors. Validate workspace shortness, alignment, aliasing,
checked size overflow, aggregate limits, index sentinels, parent/child cycles,
height, balance, subtree newest-position metadata, and counter saturation.
Silesia remains external performance evidence and never a correctness gate.

### TVG-0720

Generate five 8,192-byte smoke inputs as two 4,096-byte frames with a
4,096-byte window. Require exact mode, strategy, case, size, frame, window, and
iteration report fields and require candidate classification to reconcile.
Zeros must have prefix matches and no false positives; equal-prefix records
must have more prefix matches than false positives; collision records must
have false positives; the pseudorandom control must have false positives and
no five-byte prefix match. Every case must report a depth histogram. Unknown
case and zero input size must return usage failure.

The equal-prefix record is `ABCDE` plus the low three little-endian bytes of
its record number. The collision record alternates `01 00 00 58 59` and
`00 20 00 58 59`, whose complete 32-bit hash values have the same low 16 bits
(`11039`), and appends the same record number. Pseudorandom bytes use seed
`0x13579bdf`, multiplier `1664525`, and increment `1013904223`; affine
exponentiation positions the sequence at each frame offset without changing
the byte stream. Separately measure one-MiB inputs at 64 KiB, 256 KiB, and one
MiB windows and record results descriptively in BM-0055.

### TVG-0721

For the BinaryTree workspace foundation, derive the six-array layout by hand
for five nodes: offsets 0, 20, 40, 60, 72, and 112, ending at 152 bytes on the
supported 64-bit configurations. Also verify the portable element-size
formula at 65,536 nodes and the 29-MiB result at 1,048,576 nodes.

Initialize a small nonempty frame over caller-owned aligned storage and inspect
every active array element for its documented inactive sentinel. Require an
oversized supplied span to retain its unused tail. Seed a finder successfully,
then present short, misaligned, and overlapping workspaces; require stable
errors and byte-for-byte preservation of both finder observations and supplied
storage. Cover short zero-workspace input, invalid limits and parameters, frame
limits, aggregate workspace limits, and `size_t` addition overflow.

### TVG-0722

Construct three eight-byte records whose first bytes produce descending,
ascending, left-right, and right-left insertion orders. Insert positions 0, 8,
and 16 and require the hand-derived root and children after every single and
double rotation. Repeat with three `ABCDE` prefixes and a five-byte comparison
cap to prove the absolute-position tie-break. Require height two and subtree
maximum position 16 at each final root.

Reject uninitialized, non-indexable, duplicate, full-slot, and collision-slot
insertions without changing the valid tree. Corrupt height, subtree maximum,
child position, parent, index, parent-cycle, and inactive sentinels separately
and require stable validator categories. Finally insert every indexable
position of a 512-byte fixed-seed LCG input into two independent workspaces;
require valid AVL bounds, complete maximum-position metadata, and identical
node snapshots.

### TVG-0723

Build ascending four-record input and remove its left leaf, requiring the
deterministic deletion rotation and subsequent root transitions through a
two-child root, a one-child root, and an empty tree. Separately remove a
three-node root whose successor is its direct right child. For the non-direct
case, build keys `D,B,F,E,G`, remove `D`, and require slot 24 (`E`) itself to
become root while removed slot 0 becomes inactive; no payload may move into
slot 0.

With an eight-byte window, remove position 0 and insert position 8 into the
same modulo slot. Reject absent and non-indexable removals without changing
finder observations or workspace bytes. Finally insert every indexable
position of a 256-byte fixed-seed LCG input into two independent trees, remove
all even positions followed by all odd positions, validate after every
deletion, compare roots throughout and all node snapshots after each parity,
and require both trees to finish empty.

### TVG-0724

With an eight-byte window, advance from zero to eight and require position zero
to remain present for the query at position eight. Then advance one position,
requiring position zero to retire before position eight reuses its modulo slot.
Advance sixteen positions in one call and inspect every skipped position to
prove sequential insertion. Advance a twelve-byte input through its final four
non-indexable positions and require expiry to leave only positions four through
seven active.

On a 128-byte fixed-seed LCG input with a sixteen-byte window, compare a single
whole-range advance against 128 one-byte advances. Require equal next position,
active count, root, and every node snapshot, with both structures valid. Begin
an advance at an unexpected position and require an unchanged workspace,
sticky invalid protocol state, a stable validator category, and rejection of
all later insertion, removal, and advance attempts.

### TVG-0725

Place capped records `ABCDE`, `ABCDG`, and query `ABCDF` at eight-byte
boundaries. Advance to the query and require positions zero and eight to be
its predecessor and successor with LCP four on both sides. Repeat with two
identical `ABCDE` records and an identical query, requiring the virtual
absolute-position tie-break to make the newest equal record the predecessor
with the full capped LCP.

Cover an empty tree, exact input end, and a query with fewer than five bytes
remaining as successful empty results. Reject an unexpected current position,
an out-of-range position, and a sticky-invalid finder without reading the
tree. Finally, for every indexable query in a fixed-seed binary input, compute
the maximum LCP by enumerating the active half-open window and compare it with
the maximum of the BinaryTree predecessor and successor LCPs before advancing
one byte. Validate the tree after every advance.

### TVG-0726

Place `ABCDEA`, `ABCDEL`, `ABCDEN`, and `ABCDEB` records before query
`ABCDEM`. Require the lexicographic predecessor and successor to establish
maximum LCP five while prefix-range aggregation selects the lexicographically
nonadjacent newest record at position 24. Repeat with an all-`0xff` five-byte
prefix, ordering sixth bytes so the newest equal-prefix record is not the
predecessor and no finite prefix upper bound exists.

Require empty and below-minimum LCP results to skip range aggregation and return
no candidate. For every indexable position of a fixed-seed low-alphabet input,
enumerate the active window, determine global maximum LCP and the greatest
position having that length, and compare both fields with the BinaryTree
candidate before advancing. Require at least one qualifying match and validate
the tree after every transition.

### TVG-0727

Convert the nonadjacent newest-prefix fixture from TVG-0726 into the hand-
checkable match `{distance 8, length 5}`. Require empty, one-byte, overlap,
repeated-record, all-byte-value, fixed-seed pseudorandom, and structured mixed
inputs to produce identical `LzssMatch` results from BinaryTree Exact,
HashChain Exact, and Exhaustive at every position through exact input end.

Sweep window sizes 1, 5, 17, 256, and 65,536 and maximum lengths 5, 17, and
258 over the structured mixed input. After each query, advance all stateful
finders over the same one-byte interval and validate BinaryTree structure.
Separately advance across multi-byte parser skips and compare at each landing
position. Do not compare tree shape or diagnostic work counters and do not
connect BinaryTree to production selection.

### TVG-0728

Run the private BinaryTree typed single-pass entry on empty, one-byte,
repetitive, nearest-distance, all-byte-value, fixed-seed pseudorandom, and
structured mixed inputs. Require token count, token storage size, every typed
field, and canonical serialized byte sequence to equal both established Exact
paths. Exercise both 64-KiB and one-MiB typed variants where their parameter
ranges apply.

Pre-fill output with a noncanonical sentinel and reject one-token-short output,
one-byte-short workspace, input/output overlap, input/workspace overlap,
output/workspace overlap, and aggregate-memory shortage without changing any
output token. Require stable generic and BinaryTree-specific errors. Successful
encoding may modify only the reported token prefix; unused output remains the
sentinel. Do not add a production strategy selector or interoperability archive.

### TVG-0729

Run two BinaryTree Exact finders over the same repeated-record fixture with a
sixteen-byte window, one with diagnostics and one without. Compare every match
through exact input end and advance both by one byte. Require the measured
query count to include input end; successful insertion and retirement counts
to match the finite five-byte indexing and window rules; every comparison,
LCP, range, rotation, height, and query-depth category to be exercised; and
the query histogram sum to equal the query count.

Initialize an empty finder with the query counter and zero-depth histogram bin
already at `uint64_t` maximum. One exact-end query must leave both saturated
and set the shared overflow flag. Separately pass diagnostics through the
private typed BinaryTree encoder and require its query count to equal the
produced token count without changing any token or canonical byte comparison.
Do not expose the counters through a public API or production strategy.

### TVG-0730

Run `--frames binary-tree-exact` and `--frames hash-chain-exact` over the same
repository README with 1,024-byte frames and a 65,536-byte window. Require the
BinaryTree report to identify its private strategy; publish every workspace,
query, comparison, LCP, range, rotation, mutation, height, depth, timing, and
throughput field; validate its histogram syntax; and require equal token counts
between the two Exact strategies.

Repeat both strategies on 8,192 generated bytes split into two 4,096-byte
frames for zeros, periodic, equal-prefix, hash-collision, and fixed-seed
pseudorandom inputs. Require positive core BinaryTree work counters and equal
token counts for every class. An empty file must report zero bytes, frames,
tokens, queries, insertions, retirements, and a single zero histogram bin.
Retain the established rejection of unknown strategies and invalid sizes.

### TVG-0731

Parse a hand-written BinaryTree key/value report containing all required
configuration, diagnostic, timing, and histogram fields and require exact
typed values. Reject duplicate keys and an incomplete HashChain report.

Aggregate two artificial BinaryTree member reports at one window. Require
input bytes, iteration-weighted measured bytes, frames, tokens, elapsed time,
and additive counters to sum;
workspace, height, and maximum query depth to take their maximum; histogram
bins to extend and sum; and aggregate throughput to use total bytes divided by
total time. These fixture-only tests must neither access the real Silesia
directory nor start a benchmark process or network operation.

### TVG-0732

Parse and validate a hand-written `synthetic` BinaryTree report with an exact
case identity and complete strategy-specific diagnostics. Reject a different
case identity under otherwise identical configuration.

Accept one complete HashChain/BinaryTree pair only when its token counts are
equal. Reject a mismatch and either incomplete pair. Aggregate an artificial
synthetic report through the shared sum, maximum, histogram, workspace, and
throughput rules, then require the group count to be named `case_count` rather
than `member_count`. These fixture-only tests must not launch the benchmark,
generate a large input, access the Corpus, write result JSON, or use a network.

### TVG-0733

Before implementation, fix HashTree Exact fixtures for the shared five-byte
hash, Chain/PromotedTree state transition, threshold values immediately below,
equal to, and above observed candidate depth, and threshold zero. Require the
non-const triggering query to return the Chain result and publish only a
pending promotion; require the following `advance` to build it before interval
insertion and the next query to use the tree. Make one bucket empty and later
active again without a second promotion.

Construct active predecessor chains containing hash collisions, equal capped
suffixes, window-edge positions, short final suffixes, and skipped parser
intervals. Promote newest-to-oldest without a temporary allocation and require
the promoted position set to equal the active Chain set. At each following
position compare HashTree match, typed token, and canonical serialization with
Exhaustive, HashChain, and global BinaryTree.

For LCP traversal, hand-construct lower-only, upper-only, two-sided, unequal
bracket-LCP, exact-key, and zero-known-prefix paths. Require skipped bytes to
be a proven common prefix and compare every remaining byte needed for order.
Corrupt bucket mode, root, slot generation, links, height, balance, subtree
maximum, and cross-bucket ownership independently and require bounded stable
failure. Counter-present and counter-null runs must have identical promotion
positions and tokens.

### TVG-0734

Fix hand-calculated five-byte prefix-hash vectors for all-zero, a single low
byte followed by zeros, all-`0xff`, and ascending input. Exercise a nonzero
input position, every byte value in every prefix offset, every short remaining
length, and a position beyond the input. Invalid requests must return a zeroed,
invalid result without reading input.

Retain the independently discovered pair `01 00 00 58 59` and
`00 20 00 58 59` as an exact 32-bit collision with hash `0x00102b1f` so the
helper cannot be mistaken for an equality proof. Existing Exhaustive-versus-
HashChain tests remain the token-level differential oracle after extraction.

### TVG-0735

Fix HashTree workspace requirements for input sizes below the five-byte prefix,
exactly five bytes, 64 KiB, and 1 MiB. Check power-of-two bucket sizing, node
capacity, every array offset, element alignment, non-overlap, and exact final
extent. On the current 64-bit supported host, require 35,454,976 bytes for a
one-MiB window and 65,536 buckets.

Reject invalid limits, invalid LZSS parameters, input beyond frame bounds,
workspace beyond the internal-buffer limit, and input-plus-workspace arithmetic
overflow with stable private error categories. The calculator must allocate
nothing and must not receive or modify caller workspace in this stage.

### TVG-0736

Fill an aligned workspace and excess guard region with `0xa5`, initialize a
private HashTree finder, and require only heads, roots, and modes to change to
their canonical Chain control values. Require predecessor links, alignment
padding, all tree-node arrays, and excess caller capacity to retain the exact
byte pattern. Do not inspect a lazy region through its eventual typed view.

Initialize input shorter than five bytes with empty workspace and require a
valid zero-capacity finder. Seed a distinct finder, then independently reject
short workspace, misaligned workspace, and input/workspace overlap. Each
failure must preserve the seeded finder and every caller byte exactly.

### TVG-0737

Run the HashTree Chain-only finder position-by-position over empty, short,
binary, repetitive, collision, incompressible, and window-boundary inputs.
Require every match to equal both Exhaustive and HashChain and require optional
Chain diagnostics to preserve results. Advance across multi-position parser
skips and require skipped positions to become candidates.

Before a position is published, require its lazy predecessor link to be
constructed; unreachable later slots must remain at their byte fixture pattern.
Independently violate query position and advance interval, and corrupt a
reachable head or predecessor distance out of range. Require a stable first
error, sticky-invalid state, empty later matches, unchanged later workspace,
and finite completion. No test may activate a tree mode or inspect an
unconstructed tree-node field through its eventual type.

### TVG-0738

For a fixed completed-query depth, test thresholds immediately below, equal
to, and above it; only the below case may become Pending. At threshold zero,
zero candidates remain Idle and one candidate becomes Pending. Repeat an
identical completed query and require an idempotent Pending state independent
of any diagnostic object.

Require `begin_advance` on Idle to report no work, and on Pending to return the
exact bucket and trigger count while entering Building. Require only a matching
Building bucket to commit. Independently submit an out-of-range bucket, a
different query while Pending, a query while Building, and a wrong commit;
each must preserve the first stable error and terminate without publishing a
bucket mode or touching finder workspace.

### TVG-0739

Build one bucket from hand-checkable newest-to-oldest Chains that require no
rotation, left and right single rotations, and both double rotations. Require
the returned position set to equal the active Chain set, exact parent/child
links, correct height and subtree maximum, and a null parent at the private
root. Include equal capped suffixes so absolute position is the deterministic
final key.

Place one predecessor exactly at the window edge and one immediately beyond;
only the edge node may be constructed. Use a capacity smaller than the
absolute positions to prove ring-slot mapping. Require empty head to produce a
valid empty build without touching tree storage.

Independently reject an invalid bucket count or identity, wrong node-array
extent, query position outside input, future or short-suffix head, excessive
or forward predecessor distance, reachable prefix in another bucket, and a
corrupted constructed invariant. Every failure must return a null root,
terminate within node capacity, and leave caller-owned head, links, roots,
modes, and promotion state unchanged. No test connects the builder to finder
query or advance in this stage.

### TVG-0740

Build promoted bucket trees for ascending, descending, equal-capped,
repetitive, binary, and deliberate hash-collision inputs. At each eligible
query position compare the private tree result byte-for-byte with Exhaustive
and HashChain, including longest length and nearest distance. Cover empty
root, no match, one-sided neighbors, two-sided neighbors, maximum-length
matches, and multiple equal-length candidates.

Independently corrupt root, child index, slot position, bucket ownership,
subtree maximum, and a traversal cycle using already constructed test nodes.
Require a stable private error, empty match, and termination within a bound
derived from node capacity. Query must not modify Chain links, any node field,
root, mode, promotion state, input, or diagnostics. Finder integration and
LCP comparison skipping remain outside this stage.

### TVG-0741

Starting from hand-built promoted buckets, insert keys that require no
rotation, each single rotation, and each double rotation. Remove a leaf, a
one-child node, the root, and a two-child node whose successor is both direct
and deeper. After every operation require the diagnostic active-range
validator to accept and the Exact query to match Exhaustive and HashChain.

Advance a small window position-by-position, deleting the exact distance-W
node only after its query and inserting every skipped position. Include an
empty promoted bucket becoming active again, equal capped keys, hash
collisions, and ring-slot reuse.

Independently attempt duplicate insertion, absent removal, wrong bucket,
short-suffix position, invalid root, corrupt reached child, and a search cycle.
Each preflight failure must return a stable error result, preserve the copied
root and every node byte, and terminate within node capacity. Mutation must
not receive or modify Chain links, bucket roots/modes, or promotion state.

### TVG-0742

Run the connected finder with threshold zero, small finite thresholds, and the
disabled maximum threshold over empty, short, repetitive, periodic, binary,
collision, incompressible, window-boundary, and skipped-interval inputs. At
every query require matches to equal Exhaustive, HashChain, and global
BinaryTree. Counter-present and counter-null runs must produce identical
matches, promotion positions, modes, and roots.

Require the trigger query to leave mode Chain and root null. Require its next
valid advance to publish one validated root and PromotedTree mode before the
following query. Slide beyond the window, exercise ring reuse, make a promoted
bucket empty and active again, and require mode never to return to Chain.

After Pending, corrupt its reachable Chain link and require build failure to
leave root and mode unpublished and make the finder sticky-invalid. Separately
corrupt a published root/node before query and a reached tree path before
mutation; require distinct stable finder errors, empty later matches, no later
workspace mutation, and finite completion. With default options, require the
entire pre-integration Chain-only workspace and diagnostics behavior unchanged.

### TVG-0743

Run an immediately promoting repetitive fixture with and without a statistics
object. At every query and advance require equal matches, roots, modes, and all
workspace bytes. Require the counted run to report two completed Chain queries,
one trigger, one successful promotion, a non-zero promotion-build population,
later Tree queries and visits, post-promotion insertions, and internally
consistent maximum promoted bucket and node populations. Require Chain-query
plus Tree-query counts to equal the generic completed query count.

Run a fixture whose promoted bucket becomes empty and later active again.
Require one monotonic promoted bucket, retirement and reinsertion counts, a
null root while empty without decreasing the recorded maximum, and unchanged
Exact matches. Seed every additive HashTree counter and both reached histogram
bins at `UINT64_MAX`, exercise its update, and require saturation plus the
shared overflow flag rather than wraparound. Default-disabled HashTree must
leave every new strategy-specific counter zero while retaining its established
HashChain diagnostics.

### TVG-0744

Attach and omit the component observer on an identical three-node
double-rotation builder fixture. Require byte-identical trees and results,
positive key comparisons and byte tests, two rotations, and maximum height
two. Require validator comparisons to be included.

On a hand-checkable promoted query, require identical Exact result and
read-only arrays with and without the observer. Require positive finite-key,
LCP, and prefix-range work, and require LCP-skipped bytes to remain zero for
the current reference traversal. On insertion and removal fixtures, require
identical roots and arrays, positive preflight/search comparisons, expected
rotations where forced, and the resulting maximum height.

Seed each currently active component additive counter at `UINT64_MAX`, execute
a path that would increment it, and require saturation plus component
overflow. Keep the reserved LCP-skipped counter at zero until skipping exists.
Through the integrated finder, require each successful component total to
reach its separate HashTree aggregate, maximum height never to decrease,
counter-null and counter-present matches/workspace to remain identical, and
the complete disabled-default diagnostic surface to remain zero.

### TVG-0745

Run frame and all five synthetic benchmark cases through explicit
`hash-tree-exact` with threshold zero and a small non-zero threshold. Require
the report to contain the exact threshold and every HashTree diagnostic field,
finite timing, a valid workspace, internally consistent Chain/Tree histogram
totals, and token counts identical to `hash-chain-exact` on the same input.

Require omitted, non-numeric, overflowing, and disabled-maximum thresholds to
fail before measurement. Require HashChain and BinaryTree to reject an extra
threshold argument so an experiment cannot silently record an unused effort
parameter. Empty frame input must report zero queries, frames, tokens,
promotions, and route histograms without error.

### TVG-0746

Parse a complete hand-checkable HashTree synthetic report and require exact
identity/configuration fields, all component counters and maxima, finite
timing, Chain plus Tree routes equal generic queries, triggers equal
promotions, and each route histogram sum equal its route population. Mutate
each of the latter invariants independently and require rejection.

Use a HashChain token count as the oracle for multiple HashTree thresholds and
require every threshold to agree; one mismatch must reject the entire group.
Aggregate reports with different thresholds and require separate ordered
threshold/window records, retained case counts, and exact counter totals.
Run a small real benchmark sweep through the new runner to prove CLI parsing,
report validation, JSON serialization, and threshold aggregation without
external data. Keep the full default matrix opt-in because performance is not
a CTest pass/fail property.

### TVG-0747

Refactor the complete HashTree report validator behind mode-specific
synthetic and frame wrappers. Require the existing synthetic fixtures to
remain valid. Parse a complete hand-checkable frame report, require its frame
identity and all HashTree invariants, then change its mode to synthetic and
require frame validation to fail.

Freeze the narrowed Silesia default threshold tuple as 16, 64, 256, and 1024.
Aggregate hand-checkable frame reports at two thresholds, require distinct
ordered aggregates, rename the group cardinality to `member_count`, and omit
the synthetic-only `case_count`. Keep the actual twelve-member measurement
outside CTest; the shared Corpus verifier has its own small local fixtures and
the full external data remains opt-in.

### TVG-0748

Run the existing ordered-key mutation and maintenance v2 over identical
hand-checkable builds and leaf, one-child, two-child, and root removals.
Require identical results, roots, and every workspace array. Repeat through
ring-slot reuse and deterministic randomized insert/retire sequences, running
the full active-range validator after every successful mutation.

For v2 insertion, require one ordered-key decision per visited search node and
no child-order comparison from local structural checks. For v2 retirement,
require direct ring-slot selection, reciprocal parent-chain reachability to
the supplied root, and zero ordered-key comparisons through lookup and
successor traversal. Independently corrupt indices, reciprocal links, stored
position, height, balance, and subtree maximum and require stable bounded
failure. Corrupt only key order and require the explicit full validator to
reject it.

Through the integrated finder, retain byte-identical Exact matches and tokens
against HashChain and Exhaustive across boundary, repetitive, collision,
wraparound, and randomized fixtures. Re-run the complete synthetic threshold
matrix only after all functional tests pass. Require Exact agreement as a
hard gate and record maintenance key-byte comparisons descriptively; do not
use timing as a test assertion or proceed to external Silesia evidence without
a material comparison reduction.

### TVG-0749

Change the HashTree workspace-layout oracle so bucket heads use the private
fixed-width stored-position type while node positions and subtree maxima
remain host-width. Check every array offset, size, alignment, non-overlap, the
exact 64-KiB and one-MiB intermediate workspace totals, and unchanged lazy
initialization of non-control arrays.

Accept the largest representable input extent and reject the next extent
before workspace access on hosts where `size_t` is wider than `uint32_t`.
Require stored heads to initialize to their distinct sentinel. Exercise empty,
chain-only, skipped-position, wraparound, promotion, reachable-corruption, and
sticky-error paths and retain Exact equality with HashChain and Exhaustive.
Run all tests, including interoperability schema compatibility and the five
Python tooling tests, under both MSVC and ClangCL.

### TVG-0750

Change the HashTree workspace-layout oracle a second time so the node
absolute-position array uses the fixed-width stored-position type while the
subtree-maximum array remains host-width. Require its alignment and segment
extent to be independent of `size_t`, and fix the exact 64-KiB and one-MiB
intermediate workspace totals.

Make the builder, query, mutation, and integrated-finder fixtures store node
positions in the fixed-width representation. Preserve the host-width result
and subtree-maximum interfaces. Require checked storage, explicit widening at
mixed-width arithmetic, the stored sentinel on retirement, every existing
tree invariant and corruption rejection, Exact match equivalence, and sticky
failure behavior. First observe compilation failures where the old
host-width assumptions cross the new span boundary; then require all four
focused component suites and the complete MSVC and ClangCL suites to pass.

### TVG-0751

Change the last HashTree workspace-layout segment, subtree maximum, to the
fixed-width stored-position representation. Require four-byte workspace
alignment, exact segment adjacency and non-overlap, and exact 64-KiB and one-
MiB final totals of 2,228,224 and 26,804,224 bytes on a 64-bit host.

Change all builder, mutation, and query fixtures to the same stored type.
Require explicit widening when a subtree maximum enters host-size arithmetic,
the representation sentinel after retirement, unchanged atomic corruption
failures, and identical tree arrays between reference and maintenance-v2
routes. Observe the old mixed-width and implicit-narrowing build failures
first. Then require all 51 focused tests and all registered tests, including
the five Python tooling tests and interoperability schema compatibility, under
both compilers. Keep benchmark timing out of functional assertions; rerun the
complete synthetic and external Silesia matrices as a separate evidence step.

### TVG-0752

Build revision `f567415` with MSVC 19.51.36252.0. Verify all twelve local
Silesia members before measurement. Run the unchanged complete synthetic
matrix at thresholds 0, 4, 16, 64, 256, 1024, and 4096 and the unchanged
Silesia matrix at thresholds 16, 64, 256, and 1024 over all three established
windows.

Require all 105 synthetic and 144 Silesia candidates to match their paired
HashChain Exact token counts. Key records by case or member, window, and
threshold and compare every report field with the maintenance-v2 evidence,
excluding only timing and workspace. Require zero mismatches. Report exact
workspace and same-run speed ratios separately. Treat cross-run timing as
descriptive and do not use it to claim a fixed-width speedup.

### TVG-0753

Before adding a public window above one MiB, add a benchmark-only four-MiB
configuration and keep all current profile selectors unchanged. Fix frame and
window to 4,194,304 bytes, maximum match to 258, minimum match to five, and
HashTree promotion threshold initially to 1024. Verify all local Silesia
members before measurement and retain one MiB as the paired control.

For every four-MiB member, run HashChain Exact and HashTree Exact over the same
bytes. Reject a mismatch in token count or a deterministic token fingerprint;
the fingerprint must cover token kind and every literal, length, and distance
field in logical order. Continue to use small Exhaustive fixtures for an
independent third oracle rather than attempting an unbounded Corpus run.

Record per-member and byte-weighted aggregate elapsed time, throughput, token
count, literal count, match count, matched bytes, workspace bytes, route
counts, promotion counts, and the token fingerprint. Report the HashTree to
HashChain same-run speed ratio and the four-MiB to one-MiB parse opportunity
separately. Do not describe fewer tokens or more matched bytes as a final
compressed-size gain. No interoperability vector, stream fixture, ABI test,
or public CLI name is generated at this stage.

### TVG-0754

First require frame, synthetic, and HashTree benchmark smoke tests to reject
reports without literal count, match count, matched bytes, and a 64-digit
lowercase SHA-256 token fingerprint. Require HashTree and HashChain reports
over the same input, frame, and window to have identical token counts and
fingerprints. Empty input must report all counters as zero and the standard
empty SHA-256 value.

Use an independently enumerated hand vector: one eight-byte frame containing
the periodic bytes 0 through 7 produces eight Literals, no Matches, no matched
bytes, and fingerprint
`01bb0535b2b2d15fdd53c366283247566c1bd9411af6b5eddd84f6d838f9aeb9`.
The record sequence consists of the frame marker and size followed by eight
Literal records; do not obtain the expected digest from the benchmark under
test. After implementation, run all three benchmark smoke tests and the full
MSVC and ClangCL suites with the 600-second per-test limit.

### TVG-0755

Add fixture-only tests for the dedicated four-MiB runner before invoking the
external Corpus. Fix its one-MiB and four-MiB frame/window constants and
threshold 1024. Require the common report parser to preserve even an all-
decimal 64-character SHA-256 value as text. Reject uppercase, wrong-length,
or non-hex fingerprints; negative counters; token-kind totals that do not
equal token count; and literal plus matched extents that do not equal input.

Require the four-MiB candidate to match its oracle in token count, literal
count, match count, matched bytes, and fingerprint. Construct consistent
multi-member role fixtures proving byte-summed counters, measured-byte
throughput, maximum workspace, strict CPU and parse-opportunity gates, and the
combined eligibility value. Add a slower candidate with no parse improvement
and require all gates to remain false without raising an error. Register the
test as ordinary tooling independent of Corpus presence. After implementation,
run all six Python tooling tests, then both complete compiler suites including
schema compatibility.

### TVG-0756

Build revision `9de8d29` with MSVC 19.51.36252.0 in Release mode and run the
dedicated four-MiB experiment once over every verified Silesia member. Require
exactly 36 records, twelve unique members, and the fixed one-MiB control plus
four-MiB oracle/candidate roles. Reject publication unless each four-MiB pair
matches in token count, literal count, match count, matched bytes, and token
fingerprint.

Sum input and token counters by role, calculate throughput from summed measured
bytes and seconds, and take the maximum workspace per role. Record the same-run
HashTree/HashChain ratio and four-MiB-minus-one-MiB token and matched-byte
deltas. Preserve negative per-member speed results even when aggregate gates
pass. Keep the ignored local JSON outside version control; record its revision,
configuration, aggregate evidence, and conclusions in maintained documents.

### TVG-0757

Audit the four-MiB encoder bound from checked profile-calculator terms, not
process RSS or average Corpus token count. Use the maximum one-token-per-byte
extent and the actual supported-build `sizeof(LzssTypedToken)`. Add frame input
and the exact fixed-width complete-HashTree workspace before considering any
entropy backend. Require the subtotal to be rejected when it exceeds the
default internal-buffer limit.

For the following sparse-pool calculator, add overflow, alignment, exact-fit,
one-byte-short, zero-pool, full-pool, and backend-specific payload-ceiling
fixtures before mutation code. Prove that pool exhaustion selects a complete
HashChain route for the affected bucket and never exposes a partial tree.
Compare resulting tokens directly with HashChain Exact and retain fingerprint
comparison in later Corpus evidence.

### TVG-0758

For the sparse workspace calculator, derive `N` and `B` exactly as the complete
HashChain and require explicit pool capacity `P <= N`. Verify zero input,
sub-prefix input, zero pool, one node, capacities around four-byte alignment,
exact workspace budget, one-byte-short budget, maximum pool, invalid limits,
invalid parameters, unrepresentable position extent, multiplication/addition
overflow, and aggregate-limit rejection. Assert every offset is aligned,
non-overlapping, monotonic, and reconstructs the reported final size.

For the allocator, require deterministic initialization, unique allocation up
to exhaustion, normal exhaustion without state corruption, LIFO release and
reuse, double-release rejection, foreign/out-of-range node rejection, and
exact free/active accounting. Later promotion fixtures must prove no metadata
change when a bucket does not fit, full rollback on injected build failure,
complete commit on success, and no retry after `PoolRejectedChain`. Mutation
fixtures must force full-pool insertion demotion and compare every resulting
match and token directly with HashChain Exact.

### TVG-0759

Add nine focused tests for the first sparse foundation. Verify empty/sub-
prefix layout, all eleven aligned segments on a small odd capacity, the exact
four-MiB no-pool value 17,629,184, the one-node padded delta 24, the four-node
delta 84, and the full-pool `21P` delta. Reject `P > N`, valid-but-insufficient
aggregate limits, invalid limit configurations, invalid dictionary parameters,
unrepresentable input extent on wide `size_t`, and frame limit excess. Accept
the exact aggregate limit and reject the same bound reduced by one byte.

Initialize capacities zero and three. Require ascending allocation, normal
exhaustion, exact free/active accounting, LIFO reuse, sticky double-release and
out-of-range errors, and rejection of short and misaligned workspaces without
publishing an initialized pool. Run the nine tests under MSVC and ClangCL,
then run every registered test under both compilers with a 600-second per-test
limit and no exclusions, including interoperability schema compatibility.

### TVG-0760

Construct a three-node pool-local AVL tree whose node IDs are deliberately
unrelated to absolute-position modulo values. Use equal capped keys at
positions 0, 5, and 10 with query position 15, and require the Exact nearest
match `{distance = 5, length = 5}`. Reclassifying the same arrays as a complete
ring tree must fail the ring-capacity invariant.

Construct a second pool-local tree whose reached subtree maximum names an
absolute position absent from the tree, then replace it with the stored-position
sentinel. Require bounded `invalid_tree` failure and no match in both cases,
without performing input arithmetic on the sentinel. Run all nine bucket-query
tests under MSVC and ClangCL, then run
every registered test under both compilers with a 600-second per-test limit and
no exclusions, including interoperability schema compatibility.

### TVG-0761

Add eight direct pool-local builder tests. Build a three-position equal-key
chain into exactly three nodes, validate it, and require the pool-local Exact
query to return `{distance = 5, length = 5}`. Reproduce the complete builder's
single-left, single-right, left-right, and right-left rotation shapes and check
the expected root position and height.

With capacity two for a three-node chain, require normal insufficient capacity,
zero allocation, byte-for-byte unchanged node arrays, and no published root or
count. Accept an empty chain with zero capacity. Preallocate and release nodes
to force noncontiguous IDs while retaining one unrelated allocation; require
successful build/query/release without changing it. Reject a malformed chain
before pool mutation. Reject corrupt subtree metadata and require a release
attempt on that tree to leave every pool field unchanged. Require the validated
whole-bucket release to restore exact free/active accounting. Run all
eight tests under MSVC and ClangCL, then run every registered test under both
compilers with a 600-second per-test limit and no exclusions, including
interoperability schema compatibility.

### TVG-0762

Build the three-node pool-local fixture, allocate its fourth node, insert
absolute position 11, and validate the resulting four-position chain and Exact
query. Detach absolute position 5 through the two-child deletion shape, require
the returned node to be in reserved sentinel state, release it through the
pool, and validate the remaining chain and nearest match at position 11.

Reject a non-reserved node and a duplicate position without changing either
the tree or reserved node. Reject ring identity at the pool entry point and
pool identity at the ring entry point. Corrupt a reached child link into a
cycle and require bounded preflight failure with byte-for-byte unchanged pool
arrays. Run these five pool-local fixtures and all fifteen existing ring
mutation fixtures under MSVC and ClangCL, then run every registered test under
both compilers with a 600-second per-test limit and no exclusions, including
interoperability schema compatibility.

### TVG-0763

Drive the sparse bucket state component directly. With a three-position chain
and capacity two, require promotion to publish `PoolRejectedChain` without any
pool mutation. With sufficient capacity, require a complete promoted tree. At
exact pool exhaustion, request a new insertion and require validated release of
all three nodes followed by null root, zero count, and terminal chain mode.
With one spare node, require ordinary pool-local insertion and retained promoted
mode.

Reject re-promotion from `PoolRejectedChain`. Make a malformed chain fail hard
without entering the terminal capacity state. Force a duplicate insertion and
require the original promoted metadata plus exact free/active accounting after
reserved-node rollback. Corrupt a promoted root's subtree maximum at exhaustion
and require pre-release validation failure with no partial node release. Run all
eight focused state tests under MSVC and ClangCL, then run every registered test
under both compilers with a 600-second per-test limit and no exclusions,
including interoperability schema compatibility.

### TVG-0764

Initialize the complete sparse workspace for an eight-byte input and three-node
pool. Require every span length to match the checked layout and every head,
link, root, mode, count, and allocator value to equal its canonical initial
state. Dirty all metadata classes, reserve two nodes, reset the frame, and
require canonical metadata, zero active nodes, the complete free count, and
node zero as the next allocation.

Put the pool into sticky failure, then require frame reset to reject without
changing bucket mode or clearing the allocator error. Reject short and
misaligned storage without publishing an initialized workspace. Accept and
reset the zero-byte layout as a normal initialized workspace. Run these five
owner fixtures together with all nine allocator/layout fixtures under MSVC and
ClangCL, then run every registered test under both compilers with a 600-second
per-test limit and no exclusions, including interoperability schema
compatibility.

### TVG-0765

Promote the three-position sparse fixture, retire its middle absolute position
through the two-child shape, and require a decremented count plus exact pool
free/active accounting. Build a one-node promoted tree, retire its final node,
then insert a new position and require a valid one-node promoted tree. Represent
an empty promoted bucket while an unrelated node exhausts the pool and require
direct terminal-chain fallback without releasing that unrelated node.

Require `Chain` and `PoolRejectedChain` retirement to leave the pool untouched.
Request retirement of a missing position and require unchanged promoted
metadata and accounting. Put the pool into sticky failure and require rejection
before detach. Run these six new fixtures with all eight prior state fixtures
under MSVC and ClangCL, then run the related builder and mutation suites and
every registered test under both compilers with a 600-second per-test limit and
no exclusions, including interoperability schema compatibility.

### TVG-0766

Initialize the real multi-bucket sparse workspace and insert equal-prefix
positions 0, 5, and 10 through the controller. Require deterministic complete-
chain head and distances at the calculated hash bucket. Promote that bucket,
then insert position 20: require position 0 retirement, same-bucket tree
reinsertion, three active nodes, head 20, and ring-slot distance 10. This fixes
the `current + 1` post-retirement active-chain boundary.

Insert into `PoolRejectedChain` and require no promotion retry. Attempt commit
with stale expected metadata and with an internally inconsistent successful
transition; require byte-stable metadata rejection. Supply invalid bucket
metadata and a prefix-less tail position and require no chain write or pool
mutation. Run all seven controller fixtures under MSVC and ClangCL, then run the
related workspace, builder, mutation, and state suites and every registered test
under both compilers with a 600-second per-test limit and no exclusions,
including interoperability schema compatibility.

### TVG-0767

Insert equal-prefix positions into a complete chain and require its Exact query
to choose the nearest of equal-length matches. Set promotion threshold zero,
query the chain, then advance one position: require the pending bucket to become
a promoted pool-local tree before current insertion. Exhaust the pool and
require the same trigger to produce terminal `PoolRejectedChain`; its later
queries must not become pending again.

Corrupt promoted metadata and require query rejection, query a prefix-less tail
and require empty success, and supply a promotion state with mismatched bucket
count and require context rejection. Finally query one chain at a fixed
position, explicitly promote it at that same position, query the resulting tree,
and require identical bucket and `LzssMatch`. Run all fourteen controller
fixtures under MSVC and ClangCL, then run the related workspace, allocator,
builder, mutation, and state suites and every registered test under both
compilers with a 600-second per-test limit and no exclusions, including
interoperability schema compatibility.

### TVG-0768

Drive a mixed literal and repetitive byte sequence at real LZSS token
boundaries. At each boundary compare the private sparse Exact result to the
stateless exhaustive reference, require identical `LzssMatch` and beneficial
decision, and advance every consumed raw position by the selected token width.
Require both token traces and the final cursor to agree at input end.

Start an advance at a position different from its cursor and require a sticky
protocol error without workspace mutation. Advance a prefix-less tail position
and require success without insertion. Build a one-node promoted bucket whose
node expires at a prefix-less tail position and require detach, pool release,
zero node count, and retained empty-tree mode. Run all seventeen controller
fixtures under MSVC and ClangCL, then run the related workspace, allocator,
builder, mutation, state, and controller suites and every registered test under
both compilers with a 600-second per-test limit and no exclusions, including
interoperability schema compatibility.

### TVG-0769

Build a three-position equal-prefix chain with diagnostics enabled, query it at
the fourth position, and require one common query/candidate, five compared
bytes, one HashTree chain query, one candidate, and one promotion trigger.
Advance that position and require one three-node promotion build, one promoted
bucket, four maximum active nodes, and one tree insertion. Query the resulting
tree and require the same match plus non-zero tree-node and key-comparison
counters. Advance across the window boundary and require one retirement, a
second insertion, and unchanged four-node active population.

Initialize the common query counter at `UINT64_MAX`, perform a prefix-less tail
query, and require an unchanged saturated value, `overflowed == true`, and the
same empty match. Run all nineteen controller fixtures under MSVC and ClangCL,
then run the related workspace, allocator, builder, mutation, state, and
controller suites and every registered test under both compilers with a
600-second per-test limit and no exclusions, including interoperability schema
compatibility.

### TVG-0770

Initialize empty and four-byte inputs with zero workspace, query each position,
advance bytewise, and require a valid end cursor. On a mixed repetitive input,
compare sparse and exhaustive matches at every byte while forcing promotion and
retirement. Repeat comparison at actual token boundaries. Limit the pool to one
node on an equal-byte input, force terminal-chain rejection, and require Exact
matches to remain equal with zero active tree nodes.

Reject pool capacity above the window, one-byte-short workspace, misaligned
workspace, and overlapping input/workspace without initializing the destination
finder. Violate the query cursor and require a sticky protocol error with no
later advance. Finally combine all byte values, deterministic pseudorandom
bytes, and copied regions, then require sparse, full HashTree, and exhaustive
matches to agree at every selected token boundary. Run all eight private matcher
fixtures and the related sparse component suites under MSVC and ClangCL, then
run every registered test under both compilers with a 600-second per-test limit
and no exclusions, including interoperability schema compatibility.

### TVG-0771

Invoke the private sparse typed-token entry only with explicit full-pool and
immediate-promotion options. For empty, one-byte, repetitive, copied-pattern,
all-byte-value, deterministic pseudorandom, mixed-pattern, and extended-profile
inputs, compare its token count, token fields, storage size, and canonical byte
serialization with exhaustive, HashChain, and BinaryTree routes. Require one
query per emitted token and no diagnostic overflow.

Supply pool capacity above the input/window bound and a one-byte-short aligned
workspace, require the dedicated sparse error category, and prove the caller's
token storage remains unchanged. Run all fourteen typed-encoder fixtures and
the related sparse matcher/component suites under MSVC and ClangCL, then run
every registered test under both compilers with a 600-second per-test limit and
no exclusions, including all Python tooling tests and
`marc_interoperability_schema_compatibility`.

### TVG-0772

Run `sparse-hash-tree-exact` over a fixture whose final 1,024-byte frame is
shorter than the configured 256-node pool ceiling and require successful
per-frame capacity clamping. Require the explicit strategy, frame/window,
capacity, threshold, workspace, query, chain-candidate, timing, and throughput
fields. Supply capacity 1,025 for a 1,024-byte frame and require argument
rejection.

Run the deterministic equal-prefix synthetic fixture as two 4,096-byte frames
with a 512-node pool and threshold four. Freeze its token fingerprint, require
both chain and tree queries, triggers, successful promotions, and non-zero
promoted population, timing, and throughput. Run both benchmark smoke tests and
then every registered test under MSVC and ClangCL with a 600-second per-test
limit and no exclusions, including all Python tooling tests and
`marc_interoperability_schema_compatibility`.

### TVG-0773

Parse a complete synthetic sparse report with two triggers, one successful
promotion, four-node pool population, consistent route/histogram totals,
matching workspace aliases, and finite timing. Independently corrupt total
queries, promotions, pool population, Chain histogram, timing, and workspace
alias; require stable runner rejection. Require every grid candidate token
count to equal its HashChain baseline and aggregate three distinct
pool/threshold points independently.

Select two names in reverse argument order and require canonical manifest
order. Reject unknown and duplicate names, while a missing selector returns the
complete manifest. Then verify the real twelve-member local Corpus and run only
`dickens` at 64 KiB window, 65,536-node pool, and threshold 64. Require one
valid schema record and aggregate with Exact baseline equality. Run all six
runner unit fixtures and every registered test under MSVC and ClangCL with a
600-second per-test limit and no exclusions, including all seven Python tooling
tests and `marc_interoperability_schema_compatibility`.

### TVG-0774

Write and replace a checkpoint fixture atomically and require parseable UTF-8
JSON with no residual temporary file. Require exact identity acceptance and
reject a changed revision, rebuilt benchmark content, or changed recorded tool-source digest. Build a one-member, one-window, one-pool,
one-threshold fixture whose first run launches one baseline and one sparse
process, then whose second run reconstructs an identical final report while
both launch functions are forbidden. Independently reject duplicate baseline
keys and a sparse token count that differs from its restored baseline.

Run the real local `dickens` smoke once with a fresh checkpoint and again with
benchmark launches absent from progress output. Require identical measurement
records, retained checkpoint, and valid final schemas. Then run every registered
test under MSVC and ClangCL with a 600-second per-test limit and no exclusions,
including all Python tooling tests and
`marc_interoperability_schema_compatibility`.

### TVG-0775

Split the one-member, one-window, one-pool, one-threshold integration fixture
into two batches of one new point. Require the first batch to launch and persist
only its HashChain baseline and the second to reuse that baseline and launch
only its sparse candidate. Then forbid both launch functions and generate two
byte-identical final reports from the completed checkpoint. Retain the direct
complete-resume and corruption cases from TVG-0774.

On the real local Corpus, run the selected `dickens` point with a bounded first
batch, resume the remaining point in a second batch, validate status with a
zero budget, and finally generate an identical canonical report without new
measurement. Run every registered test under MSVC and ClangCL with a 600-second
per-test limit and no exclusions, including all Python tooling tests and
`marc_interoperability_schema_compatibility`.

### TVG-0776

For the shared four-MiB design, derive distance classes independently from
`floor(log2(distance))`. Require exact boundary cases at 1,048,575/1,048,576/
1,048,577, 2,097,151/2,097,152, and 4,194,303/4,194,304, plus rejection of
4,194,305 and every reference beyond current frame history. Check the exact
23-symbol distance alphabet, 4,566-entry flattening, 22-bit bypass ceiling,
32-decision Match ceiling, and `7F` raw-frame decision bound. Require all old
variant vectors and bytes to remain unchanged and every crossed pair to fail
before allocation or publication.

Backend-specific vectors are added only with their vertical implementation.
The first complete-frame vector must force distance 1,048,577; separate
bounded-history vectors must exercise classes 21 and 22 without constructing
millions of Literal tokens. Full admission later requires MSVC and ClangCL
tests with a 600-second per-test limit, sanitizer fuzzing, and
`marc_interoperability_schema_compatibility` without exclusion.

### TVG-0777

Require typed-token variant 4 to accept the documented class-transition and
maximum-distance values through 4,194,304, reject 4,194,305 through its
parameter ceiling, and leave variants 2 and 3 unchanged. Require exact layout
selection for `2/4 + 1/3`, with 23-symbol distance contexts, 4,566 entries,
22 bypass bits, 32 decisions per token, and a seven-decisions-per-raw-byte
preflight multiplier. Reject every crossed pair and the next unknown IDs.

Construct exactly 4,194,304 bytes of history from one Literal, 16,256
distance-one Matches of length 258, and one distance-one Match of length 255.
Append a length-258 Match at distance 4,194,304. Require its final Symbol to be
class 22 in context 30 with alphabet 23, followed by a 22-bit zero bypass, and
require exact typed-token reconstruction. Run every registered test under
MSVC and ClangCL with a 600-second per-test limit and no exclusions, including
`marc_interoperability_schema_compatibility`.

### TVG-0778

For the first four-MiB Dynamic Range vertical stage, encode the final class-22
Symbol in context 30 with alphabet 23 and a 22-bit zero bypass through marc's
operation-level Dynamic Range coder, then require exact decoder recovery.
Construct 1,048,577 bytes of history from one Literal, 4,064 distance-one
Matches of length 258, and one distance-one Match of length 64. Append a
length-258 Match at distance 1,048,577, serialize the existing 64-byte frame
header and 16-byte descriptor, and require complete-frame reconstruction to
all `A` bytes.

Require 4,566 entries at stream preflight, variant-selected `7F` acceptance,
frozen `6F` rejection for the older variant, crossed-pair rejection, and
transactional failure when either token or raw workspace is one element short.
Keep the variant-3 complete-frame encoder closed and prove that its failed
attempt does not alter serialized output. Run all registered tests under MSVC
and ClangCL with the 600-second per-test limit, including
`marc_interoperability_schema_compatibility`.

### TVG-0779

For the four-MiB Dynamic Range encoder lifecycle, require a one-Literal frame
to retain the frozen Dynamic Range frame bytes while its Format 2 stream header
uses exact identities `2/4 + 1/3 + 3/2`. Feed both encoder and decoder one byte
at a time through stable EndInput draining.

For a maximum four-MiB encoder profile, derive `14F + 85` complete-frame bytes,
`F` native typed tokens, `2F` native modeled operations, and the exact
HashChain workspace. Require failure under the 128 MiB default, failure at one
byte below the calculated aggregate, and success at the exact limit. For the
decoder, aggregate its caller-selected payload ceiling plus frame header,
four-MiB raw staging, and `F` native typed tokens; require the same one-short
and exact boundary. Keep public selection and interoperability unchanged.

### TVG-0780

For public C admission, require the new selector value 2 to map the Contextual
Dynamic Range workspace query to exact `2/4 + 1/3 + 3/2`. On supported 64-bit
layouts, require the encoder to fail under the unchanged default and at
264,765,524 bytes, then report 4,194,304 primary, 58,720,341 secondary, and
201,850,880 views bytes at the exact 264,765,525-byte aggregate. Require the
decoder to report 67,108,944 primary, 4,194,304 secondary, and 50,331,648
views bytes after its block limit is explicitly raised.

Require contextual rANS, tANS, Blocked Huffman, and Adaptive Huffman C queries
to reject the known four-MiB selector rather than falling back to another
profile. Keep selector 3 unknown. Preserve both older selectors, all stream
bytes, initializer defaults, CLI names, fuzzing targets, and interoperability
inventory.

### TVG-0781

Run the explicit `lzss-contextual-dynamic-range-4m` CLI name through encode,
decode, deterministic overwrite refusal, malformed input, strict trailing
data, empty input, and transactional profile-mismatch rejection. Require exact
stream identities `2/4 + 1/3 + 3/2`, and require both the 64-KiB and one-MiB
CLI decoders to reject the produced stream without creating or changing an
output file.

Keep the test payload small while retaining the real four-MiB workspace query
and factory lifecycle. Require the 256-MiB application hard limit, the
`14F + 5` payload ceiling, and public-query-owned allocation sizes. Preserve
the older CLI names and stream bytes, and leave benchmark, fuzzing, and the
52-archive interoperability inventory unchanged.

### TVG-0782

Run one Release iteration of
`marc_benchmark lzss-contextual-dynamic-range-4m` over the existing short
synthetic smoke input. Require successful public-C encode/decode round trip,
the exact reported codec name, encoded size and ratio, nonzero throughput
fields, all six direction-specific workspace fields, and peak workspace equal
to the larger aggregate.

Require the checked capacity ceiling to use factor 14 only for the four-MiB
profile and factor 12 for the frozen 64-KiB and one-MiB profiles. Preserve all
older benchmark names and output fields. Run the full MSVC and ClangCL CTest
inventories with Python tooling and interoperability schema compatibility,
while leaving Silesia measurement, fuzzing, and archive inventory for later
explicit stages.

### TVG-0783

Generate the five-byte canonical stream independently through each public
window selector. For selector 2 require exact `2/4 + 1/3 + 3/2`, reject every
proper truncation atomically through both complete-frame and public streaming
decoders, reject a nonzero descriptor-reserved byte, and require all six
ordered cross-profile decoder mismatches to publish no raw byte.

For the live harness retain fixed one-KiB token/raw staging and set its largest
payload bound to `14*1024 + 5`, flattened model limit to 4,566, and distance
limit to 4,194,304. Exercise all three strict public admissions for each input
under the existing finite call bound. Require MSVC and ClangCL warning-clean
compile smoke, then a short ClangCL sanitizer run with bounded input count,
timeout, RSS, and no source-corpus mutation. Preserve the format, CLI,
benchmark, and 52-archive interoperability inventory.

### TVG-0784

Generate schema 43 from the unchanged deterministic 8,193-byte fixture.
Preserve all 52 schema-42 entries and append exactly
`lzss-contextual-dynamic-range-4m` as entry 53. Require dictionary variant
bytes 4/0, context variant bytes 3/0, entropy identity bytes 3/0 + 2/0,
immediate decode equality, full source revision, leaf-only file name, size,
and SHA-256. Require local re-encoding to reproduce every archive byte.

Swap the first two schema-43 entries and require order rejection. Remove only
entry 53, set schema/version names back to 42/`marc-cli-v42`, and verify the
unchanged 52-entry predecessor before continuing compatibility through schema
1. Run complete MSVC and ClangCL inventories including Python tooling. The
later external protocol must report four successful 53-archive directions at
one pushed revision.

### TVG-0785

For the four-MiB Contextual rANS design, derive the 4,566-entry compact model,
9,121-byte maximum descriptor, fixed 126,976-entry decode-table bank, `7F`
decision ceiling, `14F + 8` payload ceiling, and `14F + 9,193` complete-frame
ceiling independently from the selected layout and existing scalar format.

At `F=4,194,304`, require the supported encoder arithmetic to total
130,556,905 bytes from raw, native tokens, exact HashChain workspace, and
encoded frame. Require decoder views of 51,093,504 bytes and aggregate
114,017,257 bytes from six-byte table entries, native tokens, encoded frame,
and raw staging. Record their exact headroom below 128 MiB. No implementation,
selector admission, emitted stream, benchmark, fuzzer, or interoperability
inventory changes in this design-only stage.

### TVG-0786

Serialize a one-Literal descriptor under context variant 3 and require only
its little-endian frequency-entry count to change to 4,566; parse it back and
reject it under variant 2 without publishing a descriptor. Construct every
context as a dense model and require the exact 9,121-byte maximum descriptor
and complete round trip.

Encode one Symbol request in context 23 with alphabet 23 and value 22 followed
by a 22-bit bypass value through canonical contextual rANS, then require the
scalar decoder to recover both values and exact decision counts. Reject bypass
width 23. Run all older descriptor vectors unchanged under MSVC and ClangCL;
outer rANS stream/frame/profile/public selectors remain unchanged.

### TVG-0787

Round-trip a Format 2 rANS stream header with exact identities
`2/4 + 1/3 + 4/3`, four-MiB window, and frequency count 4,566. Reject crossed
known pairs. At raw size five, require decision count 32 to fail the old `6F`
profile and pass variant 3's `7F` plus 32-per-token bounds.

Construct 1,048,577 bytes of history from one Literal, 4,064 distance-one
Matches of length 258, and one distance-one Match of length 64. Append a
length-258 Match at distance 1,048,577, encode only the typed-token rANS
descriptor/payload as a fixture oracle, and require complete-frame decoder
reconstruction to all `A` bytes. Reject token and raw workspaces one entry
short without publication. Prove the complete-frame encoder rejects the same
identity without changing output.

### TVG-0788

Encode one Literal through the four-MiB complete-frame encoder and require the
canonical scalar payload, descriptor frequency count bytes `d6 11`, and exact
complete-decoder recovery. Preserve all old Literal frame bytes.

Create raw input from marker `ABCDE`, 1,048,576 `Z` bytes, and marker `ABCDE`.
Run HashChain Exact with an explicitly raised block limit, require at least one
emitted Match distance above 1,048,576, encode and decode the complete frame,
and compare every raw byte. Decode the same frame under `2/3 + 1/2 + 4/3` and
require preflight rejection with unchanged raw output.

### TVG-0789

For `F=4,194,304`, require profile output identities `2/4 + 1/3 + 4/3`,
4,566 entries, 58,729,449 encoded-frame bytes, 67,633,152 encoder views, and
130,556,905 aggregate bytes on the supported 64-bit layout. Reject aggregate
130,556,904 and accept the exact value under the unchanged 128-MiB default.
Require encoder profile rejection when `max_block_size` is one below the
`7F = 29,360,128` decision ceiling.

Require decoder table offset 761,856, views 51,093,504, and aggregate
114,017,257 bytes; reject one byte below and accept exact. Stream one Literal
through encoder and exact-4m decoder with one-byte buffers, require header
variants 4/3, and require exact-1m admission to reject without output. Preserve
public selector rejection and the 53-archive inventory.

### TVG-0790

Select C ABI window value 2 for a two-byte-frame contextual rANS encoder,
round-trip the five-byte binary fixture, and require stream dictionary/context
bytes 4/3 plus frequency count bytes `d6 11`. Configure a one-MiB exact public
decoder with a permissive four-MiB distance limit and require malformed-stream
profile rejection with zero output, then decode successfully under value 2.

Through the public requirements query, require supported 64-bit encoder regions
4,194,304 / 58,729,449 / 67,633,152 and decoder regions 58,729,449 /
4,194,304 / 51,093,504. Reject aggregate values 130,556,904 and 114,017,256,
then accept exact limits. Configure a four-MiB maximum frame and a 29,360,128-
decision block limit. Keep unknown selector 3 invalid and all older C vectors
byte-identical.

### TVG-0791

Run the common CLI round-trip harness with explicit selector
`lzss-contextual-rans-4m` and a deterministic repeated binary fixture. Require
stream dictionary variant byte 4, context variant byte 3, entropy algorithm
byte 4, and entropy variant byte 3. Decode byte-exactly through the same name.

Require both `lzss-contextual-rans-1m` and `lzss-contextual-rans` to reject the
archive without raw publication, and require strict decode to reject one
trailing byte. Exercise the four-MiB application configuration with a small
fixture so the test verifies the full public query/factory lifecycle without
making runtime proportional to the maximum frame. Run under MSVC and ClangCL;
leave benchmark, fuzzing, and interoperability inventories unchanged.

### TVG-0792

Run `marc_benchmark lzss-contextual-rans-4m README.md 1` under MSVC and
ClangCL Release builds. Require successful pre-timing encode/decode through
the public C lifecycle, exact codec name, finite ratio and throughput fields,
all six positive directional workspace fields, and peak workspace equal to
the larger directional aggregate.

For the 4,326-byte repository README require both builds to produce 3,006
bytes, ratio 0.695, and peak caller-owned workspace 114,017,257 bytes. Record
throughput only as a smoke observation. Register the command as the eighteenth
experimental benchmark smoke; do not change fuzzing or interoperability.

### TVG-0793

Extend the existing contextual rANS malformed regression parameter set with
public window value 2. Generate its canonical five-byte stream, then require
every truncation, extreme frame-length corruption, and nonzero descriptor
flags to fail atomically in both private complete-frame and public streaming
decoders. Require four-MiB versus each older exact decoder to reject in both
directions with stable repeated terminal errors and unchanged raw output.

Compile the fixed-memory harness under MSVC and ClangCL, then build it with
Windows Clang 22 libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.
Run exactly 1,000 inputs with seed 20260822, `-max_len=32768`, `-timeout=5`,
and `-rss_limit_mb=512`. Require zero crash, hang, or sanitizer finding and
no retained artifact. Preserve the 53-archive interoperability inventory.

### TVG-0794

Generate schema 44 from the unchanged deterministic 8,193-byte fixture.
Preserve all 53 schema-43 entries and append exactly
`lzss-contextual-rans-4m` as entry 54. Require dictionary variant bytes 4/0,
context variant bytes 3/0, entropy identity bytes 4/0 + 3/0, immediate decode
equality, full source revision, leaf-only file name, size, and SHA-256. Require
local re-encoding to reproduce every archive byte.

Swap the first two schema-44 entries and require order rejection. Remove only
entry 54, set schema/version names back to 43/`marc-cli-v43`, and verify the
unchanged 53-entry predecessor before continuing compatibility through schema
1. Run complete MSVC and ClangCL inventories including Python tooling.

### TVG-0795

For the four-MiB Contextual tANS design, derive the 4,566-entry compact model,
9,125-byte maximum descriptor, fixed 131,072-entry encode/decode table bank,
`7F` decision ceiling, `ceil(21F/2) + 2` payload ceiling, and
`ceil(21F/2) + 9,191` complete-frame ceiling independently from the selected
layout and existing single-state format.

At `F=4,194,304`, require the supported encoder arithmetic to total
116,138,983 bytes from raw, native tokens, uint16 encode tables, exact
HashChain workspace, and encoded frame. Require decoder views of 50,855,936
bytes and aggregate 99,099,623 bytes from four-byte table entries, native
tokens, encoded frame, and raw staging. Record exact headroom below 128 MiB.
No implementation, selector admission, emitted stream, benchmark, fuzzer, or
interoperability inventory changes in this design-only stage.

### TVG-0796

Construct a Contextual tANS variant-3 descriptor with one sparse distance
model at context 23, symbol 22, frequency 4,096. Require a 27-byte canonical
descriptor, serialized frequency-entry count 4,566 (`d6 11` little-endian),
exact parse equality, and rejection under variant 2. Construct all 31 selected
models densely and require the exact 9,125-byte maximum and parse equality.

Require every strict prefix of the sparse descriptor to fail without changing
a sentinel output object. Reject one trailing byte, an invalid model mode,
undersized output, a variant-3 descriptor under variant 2, and a variant-2
descriptor under variant 3. Retain the existing literal descriptor vector
byte-for-byte. Run the focused descriptor inventory under MSVC and ClangCL;
do not admit an outer four-MiB Contextual tANS frame.

### TVG-0797

Under Contextual tANS context variant 3, encode the operation sequence
`Symbol(23,23,22)` then `BypassBits(22,0x2abcde)`. Require 4,566 descriptor
frequencies, decision count 23, the unchanged 131,072-entry table requirement,
payload `0B 00 B1 FD 07`, six final valid bits, and exact decode equality.
Reject the same operation alphabet under variant 2 without changing a sentinel
descriptor; accept 22 bypass bits and reject 23.

Build a valid typed-LZSS token sequence whose final match has distance
1,048,577 and length 258 after an exact raw prefix. Under dictionary/context
variants 4/3 require materialized and direct operation paths to produce equal
counts, descriptors, and payloads, then recover every token and raw extent.
Require variant 2 to reject before descriptor publication. Run both focused
inventories under MSVC and ClangCL; do not construct an outer frame.

### TVG-0798

Serialize and parse a Contextual tANS stream header carrying exact identity
`2/4 + 1/3 + 5/2`, four-MiB window, and frequency count 4,566. Require the
identity bytes at their fixed offsets and reciprocal rejection of crossed
dictionary/context variants. At raw size five, require decision count 32 to
fail the older `6F` profile and pass variant 3's `7F` and 32-per-token bounds.

Build one canonical complete frame from typed tokens ending in distance
1,048,577 and length 258. Require descriptor preflight, one-short token and
raw failures without raw publication, complete decode, and exact all-`A` raw
output. Encode a literal complete frame and require its descriptor count bytes
to be `d6 11`. Through HashChain Exact, encode a marker repeated beyond one
MiB, require an actual greater-than-one-MiB Match, exact round trip, and atomic
variant-2 rejection. Run all focused frame tests under MSVC and ClangCL;
streaming and public surfaces remain closed.

### TVG-0799

For `F=4,194,304`, require private profile identity `2/4 + 1/3 + 5/2`, 4,566
frequency entries, 44,049,383 encoded-frame bytes, 67,895,296 encoder views,
and 116,138,983 aggregate bytes on the supported 64-bit layout. Under
`max_frame_size=F` and `max_block_size=7F`, require the unchanged 128-MiB
default to pass, reject aggregate 116,138,982, and accept the exact value.

Require decoder table offset 524,288, views 50,855,936, and aggregate
99,099,623 bytes; reject one byte below and accept exact. Stream one Literal
through the encoder and exact-4m decoder with one-byte buffers, require header
variants 4/3, and require exact-1m admission to reject without changing output.
Preserve public selector rejection and the 54-archive inventory.

### TVG-0800

Select C ABI window value 2 for a two-byte-frame Contextual tANS encoder,
round-trip the five-byte binary fixture, and require stream dictionary/context
bytes 4/3 plus frequency count bytes `d6 11`. Configure an exact one-MiB public
decoder with a permissive four-MiB distance limit and require malformed-stream
profile rejection with zero output, then decode successfully under value 2.

Through the public requirements query, require supported 64-bit encoder regions
4,194,304 / 44,049,383 / 67,895,296 and decoder regions 44,049,383 /
4,194,304 / 50,855,936. Under `max_frame_size=F` and `max_block_size=7F`,
reject aggregate values 116,138,982 and 99,099,622, then accept exact limits.
Keep unknown selector 3 invalid and all older C vectors byte-identical.

### TVG-0801

Run the common CLI round-trip harness with explicit selector
`lzss-contextual-tans-4m` and a deterministic repeated binary fixture. Require
stream dictionary variant byte 4, context variant byte 3, entropy algorithm
byte 5, and entropy variant byte 2. Decode byte-exactly through the same name.

Require both `lzss-contextual-tans-1m` and `lzss-contextual-tans` to reject the
archive without raw publication, and require strict decode to reject one
trailing byte. Require CLI usage to list 64-KiB, one-MiB, and four-MiB names
once in exact order. Exercise the full public query/factory lifecycle with a
small fixture; run under MSVC and ClangCL while leaving benchmark, fuzzing,
and interoperability inventories unchanged.

### TVG-0802

Run one dependency-free Release benchmark iteration over the repository README
with `lzss-contextual-tans-4m`. Before timing, require the public C encoder and
decoder to round-trip exactly using the selected profile. Require finite ratio
and throughput fields, all six positive directional workspace regions, and a
reported peak equal to the larger directional sum.

Run the same report harness for the 64-KiB and one-MiB names. Require CLI help
to list all three names once in exact adjacency and reject the case-mismatched
`lzss-contextual-tans-4M` name. For the 4,326-byte README, record 3,005 encoded
bytes, ratio 0.695, encoder regions 4,326/54,614/396,896, decoder regions
44,049,383/4,194,304/50,855,936, and peak 99,099,623 bytes. Exercise both
MSVC and ClangCL while leaving fuzzing and interoperability inventories
unchanged.

### TVG-0803

Extend the Contextual tANS malformed regression parameter set with public
window value 2. Generate its canonical five-byte stream, then require every
truncation, extreme frame-length corruption, and nonzero descriptor reserved
byte to fail atomically in both private complete-frame and public streaming
decoders. Require every four-MiB-versus-older profile direction to reject with
stable repeated terminal errors and unchanged raw output.

Compile the fixed-memory harness under MSVC and ClangCL, then build it with
Windows Clang 22 libFuzzer, AddressSanitizer, and UndefinedBehaviorSanitizer.
Run exactly 1,000 inputs with seed 20260822, `-max_len=32768`, `-timeout=5`,
and `-rss_limit_mb=512`. Require zero crash, hang, or sanitizer finding and no
retained artifact. Preserve the 54-archive interoperability inventory.

### TVG-0804

Generate schema 45 from the unchanged deterministic 8,193-byte binary fixture.
Require the first 54 archives to retain schema-44 names and order, then append
only `lzss-contextual-tans-4m.marc`. Require dictionary variant bytes 4/0,
context variant bytes 3/0, and entropy identity bytes 5/0 + 2/0 before an
immediate byte-exact decode.

Verify all 55 archive sizes and SHA-256 values, decode each archive to the
fixture, and re-encode each byte-identically under both MSVC and ClangCL.
Reject a reordered schema-45 manifest. Remove only entry 55, declare
`marc-cli-v44`, and verify schema 44 before traversing the unchanged legacy
chain through schema 1.

### TVG-0805

Construct context-variant-3 Contextual Blocked Huffman descriptors whose
pooled distance model and all eight distance-context overrides use canonical
23-symbol dense records. Require the exact 2,588-byte maximum and verify that
changing only the selected layout preserves every variant-1 and variant-2
serialized byte.

Exercise distance symbol 22, odd dense-record high-nibble padding, every
descriptor truncation, noncanonical sparse/dense choice, crossed and unknown
layouts, short descriptor output, table-count ceiling 35, and exact/one-short
limits. No entropy payload, typed-token, frame, public, fuzz, or
interoperability boundary selects variant 3 in this first stage.

### TVG-0806

Serialize a four-model descriptor whose distance field is Single symbol 22
under context variant 3, then parse it byte-exactly and require variant 2 to
reject the same model atomically. Serialize four pooled and all 31 override
models densely using variant-3 alphabets and require exactly 2,588 bytes,
distance length 22 present, and high-nibble padding zero in every odd
23-symbol distance record.

Mutate the first pooled distance padding nibble, truncate every byte of the
small hand vector, append one trailing byte, and supply an output one byte
short of the maximum. Require no descriptor, output, or written-count
publication. Retain all variant-1 and variant-2 format tests, parsing their
actual returned serialized extents rather than unused enlarged capacity.

### TVG-0807

Model token kind 1, literal `A`, length class 1, distance class 22 in alphabet
23, and bypass value `0x2ABCDE` with width 22 under context variant 3. Require
26 decisions, three payload bytes `DE BC 2A`, six final valid bits, exact
decode, and atomic rejection under context variant 2. Require a variant-3
model builder to accept bypass width 22 and reject 23.

For direct typed-token composition, emit literal `A`, 16,256 overlapping
distance-one matches of length 258, one distance-one match of length 255, then
a distance-4,194,304 match of length 5. This creates exactly 4,194,304 bytes of
history before the final token using 16,259 tokens total. Require direct
encode/decode equality, distance symbol 22 in the pooled model, exact event
and decision counts, and crossed one-MiB decoding to preserve all tables and
tokens.

### TVG-0808

Serialize and parse stream identity `2/4 + 1/3 + 2/2`; cross dictionary or
context variants independently and require contradiction. For a five-byte
frame, admit 35 decisions only with at least two tokens under variant 3, then
separately admit the 2,588-byte descriptor maximum. Require variant 2 to
reject each widened bound.

Round-trip a canonical literal complete frame under the new identity. Build a
frame whose repeated five-byte marker is separated by more than one MiB,
require HashChain to emit a match beyond one MiB, decode it byte-exactly, and
require crossed one-MiB typed-token validation to preserve raw output. Verify
one-byte streaming with explicit four-MiB admission and reciprocal one-MiB
rejection. Finally require exact 4,194,304-byte profile inputs,
55,052,892-byte encoded frames, 126,880,348/109,722,064 directional aggregates,
one-byte-short failure, and exact-limit success.

### TVG-0809

Set public Contextual Blocked Huffman window profile value 2 with a four-MiB
window and require workspace query, factory creation, exact stream identity
`2/4 + 1/3 + 2/2`, and byte-exact round trip. Decode the same stream under
profile value 1 and require malformed-stream status, zero published bytes, and
unchanged output; then decode it under value 2. Reject unknown value 3.

For a full four-MiB profile, require encoder aggregate 126,880,348 and decoder
aggregate 109,722,064 bytes, one-byte-short failure, exact-limit success, and
exact public workspace extents. List the CLI names in 64K/1M/4M order exactly
once, reject `-4M`, emit dictionary/context bytes 4/3 with entropy 2/2, reject
both older CLI names on decode, round-trip repeated input, and reject trailing
data.

### TVG-0810

Select `lzss-contextual-blocked-huffman-4m` in the dependency-free benchmark,
require the exact reported codec name, mandatory round trip, positive
directional workspaces, peak equal to the larger aggregate, ordered
64K/1M/4M usage inventory, and rejection of `-4M`. Exercise the same smoke
under MSVC and ClangCL.

Parameterize every permanent dual-path malformed regression over all three
window profiles. For each canonical stream, truncate every byte and require
private raw staging and public output to remain unchanged. Corrupt extreme
frame lengths and descriptor flags, then require stable sticky failure.
Decode each profile with both other public selectors and require no raw
publication. Compile the fixed-memory harness warning-clean under both
toolchains, then run 1,000 in-memory sanitizer mutations with no repository
corpus or artifact.

### TVG-0811

Generate schema 46 from the frozen schema-45 profile order plus exactly
`lzss-contextual-blocked-huffman-4m`. Before publishing its archive, require
header bytes 14/15 = 4/0, 16/17 = 2/0, 18/19 = 2/0, and 98/99 = 3/0; then
decode byte-exactly, record size and SHA-256, and retain the exact archive
leaf in the manifest.

Verify all 56 manifest entries in order, decode every foreign archive, and
re-encode each profile byte-identically with the local CLI. Reject a manifest
whose last two entries are exchanged. Remove only archive 56 to reconstruct
schema 45, verify that bundle, and continue through every frozen schema down
to schema 1.

### TVG-0812

Resolve exact field-context variant 3 and require 4,566 symbol entries, 9,163
FGK nodes, and 13,729 combined entropy entries. On the supported 64-bit layout,
derive 139,984,896 payload bytes, a 139,984,976-byte complete frame,
67,788,896 encoder-view bytes, 50,487,388 decoder-view bytes, and directional
aggregates 211,968,176/194,666,668 bytes for a four-MiB frame. The encoder
vector includes four bytes of alignment before the HashChain workspace.
Require each exact limit to succeed and one byte less to fail without
publishing views.

At the typed-token boundary, build enough validated history to encode distance
4,194,304 as class 22 with 22 zero bypass bits and require older layouts to
reject atomically. Separately, encode a complete frame containing a real Match
beyond one MiB, round-trip it with one-byte buffers, and require reciprocal
64-KiB/one-MiB/four-MiB public admissions to reject before raw publication.
Later tool and interoperability vectors remain closed until their preceding
implementation stages pass.

### TVG-0813

Initialize sentinel encode and decode configurations, apply each known window
profile, and require exact backend-specific frame/window, match, frame/block,
payload, aggregate, LZ, and entropy-entry values. Preserve direction, original
size, total-output limit, structure size, ABI version, and reserved zeros.
Repeat an application to prove idempotence.

Pass null, a short structure, wrong ABI version, invalid direction, nonzero
reserved fields, and unknown profile values; require
`MARC_STATUS_INVALID_ARGUMENT` and byte-for-byte unchanged configuration.
After applying four MiB, lower payload and each directional aggregate limit
separately and require the workspace query to return
`MARC_STATUS_LIMIT_EXCEEDED`; restore or raise them and require exact workspace
success. Require the CLI preset and direct helper result to agree before codec
creation.

### TVG-0814

Initialize the Contextual Adaptive Huffman model bank with field-context
variant 3 and exact 9,163-node/4,566-symbol storage. Require all 31 tree
alphabets to match the selected layout, distance contexts to learn symbol 22,
and one-short node or symbol storage to fail atomically.

Build a four-MiB checked profile with the proven payload, entropy-entry, and
256-MiB application limits. Require context variant 3, exact directional
offsets and aggregates, including the encoder's four-byte HashChain alignment,
and one-short aggregate, payload, or entropy limits to fail without publishing
views. Run the complete model/profile suites under both MSVC and ClangCL while
leaving later codec boundaries closed.

### TVG-0815

For field-context variant 3, encode a new distance-class symbol 22 in context
23 with alphabet 23, followed by 22 zero bypass bits. Require 23 decisions,
27 payload bits, four payload bytes `16 00 00 00`, three valid final bits, and
exact reciprocal decoding. Begin the same descriptor under variant 2 and
require alphabet 23 to fail without changing the caller's symbol output.

Construct exactly 4,194,304 bytes of validated history from one literal,
16,256 length-258 matches, and one length-255 match, then append a length-five
match at distance 4,194,304. Require its final operations to be symbol 22 in
alphabet 23 plus 22 zero bypass bits. Compare direct typed-token planning and
encoding against the materialized operation path byte-for-byte, decode every
token exactly, and require variants 1 and 2 to reject without modifying
sentinel token storage.

### TVG-0816

Serialize and parse exact stream identity `2/4 + 1/3 + 1/2`; require header
bytes 14/15 = 4/0, 16/17 = 1/0, 18/19 = 2/0, and 98/99 = 3/0. With `F=5` and
`T=2`, accept decision counts 31 through 35 under variant 3, reject 31 under
variant 2's `6F` bound, reject 36 under variant 3's `7F` bound, and retain the
independent `32T` check. Reject crossed dictionary/context pairs.

Round-trip the canonical literal frame with exact variant-3 model extents.
Then place the same five-byte marker more than one MiB apart inside a complete
HashChain frame, require a real Match whose distance exceeds one MiB, decode
byte-exactly, and reject the crossed one-MiB identity without raw publication.

Drive the selected encoder and decoder to EndOfStream with one-byte input and
output, verify exact header identity, and require the one-MiB admission plus
one-short node and symbol workspaces to fail without changing caller output.
