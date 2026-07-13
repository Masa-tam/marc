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

Use the complete known-size tANS stream as the streaming encoder oracle. Feed
`ABAAABA` through one-byte input and output buffers with frame size 4 and block
size 2; output must match byte for byte. A flush after `AB` emits only the stream
header and does not shorten the first frame.

For streaming decode, feed that reference stream through one-byte input and
output buffers. Corrupt the second frame's initial tANS state offset and verify
that the first four raw bytes are committed while no byte from the corrupt frame
is published. Exercise short encoded, decoded, and block-view workspaces
independently.
