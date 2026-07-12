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
