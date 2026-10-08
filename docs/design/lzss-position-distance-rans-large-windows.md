# Native position-distance rANS, large windows

## DD-1536: additive family definitions and qualification gates

Extend DD-1532's independently implemented full previous-literal partition,
scalar byte-renormalized rANS4096/table_log12 and known-size framing. Keep
all published variants and the private reduced-literal reference unchanged.
These reservations define representations before implementation; they do
not admit public factories, CLI selectors or exchange entries by themselves.

| Window/frame maximum F (bytes) | Selector suffix | Dictionary | Context | Entropy | Contexts | Frequencies | Mask bytes | Descriptor maximum |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 8,388,608 | 8m | 2/11 | 1/17 | 4/8 | 55 | 4647 | 7 | 9262 |
| 16,777,216 | 16m | 2/12 | 1/18 | 4/9 | 56 | 4658 | 7 | 9283 |
| 33,554,432 | 32m | 2/13 | 1/19 | 4/10 | 57 | 4669 | 8 | 9305 |
| 67,108,864 | 64m | 2/14 | 1/20 | 4/11 | 58 | 4680 | 8 | 9326 |

Dictionary identities reuse the corresponding independently specified
position-distance Dynamic Range dictionary grammar, lengths3..258. The new
context and entropy identities do not reinterpret its model or payload.
The proposed selectors are lzss-position-distance-rans-8m, -16m, -32m and
-64m. Exact tuple and counts are checked before payload interpretation.

Let N=log2(F). Alphabet sequence is [2]*3+[256]*17+[9]*3+[N+1]*9+[2]*N.
Kind contexts0..2, initial literal3, previous-literal high-nibble contexts
4..19, length contexts20..22, distance-class contexts23..31 and distance
bit contexts32..(31+N) are explicit. Matches preserve literal history.
The exact-window distance classN permits only residual zero; it cannot
occur in a nonempty reset frame of at mostF bytes. Directed isolated
grammar tests and reachable F-3/F-258 frame recipes cover different claims.

Descriptor metadata retains decisions u32,payload u32,table_log u8=12,
flags u8=0,contexts u16,frequencies u32. The mask starts at16 and has
ceil(contexts/8) bytes. Unused high bits are zero and strictly rejected.
Records start at16+mask bytes; canonical single/dense/sparse records and
frequency normalization remain DD-1532. The maximum is
16+mask bytes+2*frequencies-contexts. Empty entropy has that metadata/mask
extent and an8-byte state payload.

Stream112 and frame64 layouts remain DD-1532. Replace dictionary variant
at14, entropy variant at18, context variant at98, context count at82,
frequency entries at84, window at64 and configured frame at20 with the
selected definition. All reserved fields remain zero. Known original size,
strict termination/trailing rejection, frame reset and quarantine remain
unchanged. Never publish a failed frame or reinterpret older tuple bytes.

Bound decisions by min((N+10)*T,ceil((N+4)/3)*F), whereT is token count.
A literal costs2 decisions; a length3/4 match costs at mostN+4 decisions,
and longer matches cannot exceed that per-raw-byte ceiling. Thus8MiB uses
min(33T,9F);16/32/64MiB use min(34T,10F),min(35T,10F),min(36T,10F).
Each decision emits at most two renormalization bytes; payload capacity is
18F+8 for8MiB and20F+8 for the other windows. Serialized capacity adds
descriptor maximum and64-byte frame header to that payload bound.
Checked arithmetic precedes every allocation and offset/count conversion.
Fixed working allowance is128KiB, retained separately from capacities.

Qualify8MiB first, then16MiB,32MiB,64MiB. Each requires independent model,
token, frame and split-stream differential tests; malformed/quarantine and
allocation tests; fully instrumented fuzzing; public API/ABI/CLI; full
twelve-member exact roundtrip/compression comparison with its corresponding
contextual rANS; isolated encode/decode samples and process peaks. Capacity
defaults, watchdogs and heavy-test scheduling follow actual measurements,
not extrapolated speed or RSS. Huffman/Range comparisons are not admission
criteria. Preserve all old generated evidence.

Schemas68..71 append one selector each, preserving the first77 archives
and every historical inventory. Final schema71 has81 archives. Run strict
header preflight negatives, complete history and self/cross producer routes.
Keep the latest qualified Windows runtime at its existing fixed output
location, saving the previous runtime before replacement. Local commits
may separate milestones. The maintainer pushes and performs hosted CI and
four external81-archive routes after all four local profiles qualify.
