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
